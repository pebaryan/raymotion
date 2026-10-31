// rig:: 3D -- characters as g3d subtrees: superellipsoid meshes on a joint hierarchy,
// soft limbs re-swept every frame, outfit and face textures drawn on the GPU.
#include "rig_internal.hpp"
#include <cstring>
#include <map>
#include <memory>

namespace rmtn {
namespace rig {

namespace {
using namespace detail;

constexpr int kLat = 24, kLon = 40;              // Super grid
constexpr int kTubeBody = 22, kTubeCap = 4, kTubeSides = 20;

struct MeshData {
    std::vector<float> pos, nrm, uv;
    std::vector<unsigned short> idx;
};

// How finely the shapes are cut: the full grid for a character, a coarse one for the baked
// bodies of a crowd, which are drawn hundreds at a time.
struct Grid { int lat = kLat, lon = kLon, tubeBody = kTubeBody, tubeCap = kTubeCap, tubeSides = kTubeSides; };

Mesh toMesh(const MeshData& d, const std::vector<unsigned char>* colours = nullptr) {
    Mesh m{};
    m.vertexCount = (int)d.pos.size() / 3;
    m.triangleCount = (int)d.idx.size() / 3;
    m.vertices = (float*)MemAlloc((unsigned)(d.pos.size() * sizeof(float)));
    m.normals = (float*)MemAlloc((unsigned)(d.nrm.size() * sizeof(float)));
    m.texcoords = (float*)MemAlloc((unsigned)(d.uv.size() * sizeof(float)));
    m.indices = (unsigned short*)MemAlloc((unsigned)(d.idx.size() * sizeof(unsigned short)));
    std::memcpy(m.vertices, d.pos.data(), d.pos.size() * sizeof(float));
    std::memcpy(m.normals, d.nrm.data(), d.nrm.size() * sizeof(float));
    std::memcpy(m.texcoords, d.uv.data(), d.uv.size() * sizeof(float));
    std::memcpy(m.indices, d.idx.data(), d.idx.size() * sizeof(unsigned short));
    if (colours && colours->size() == (size_t)m.vertexCount * 4) {
        m.colors = (unsigned char*)MemAlloc((unsigned)colours->size());
        std::memcpy(m.colors, colours->data(), colours->size());
    }
    UploadMesh(&m, true);
    return m;
}

Vector2 outfitUV(const Prim& p, float u, float v) {
    const Rectangle r = region(p.region);
    const float vv = std::clamp(p.v0 + v * (p.v1 - p.v0), 0.f, 1.f);
    // Keep half a texel inside the strip so neighbouring strips do not bleed in.
    return { r.x + std::clamp(u, 0.002f, 0.998f) * r.width, r.y + std::clamp(vv, 0.002f, 0.998f) * r.height };
}

MeshData superMesh(const Prim& p, const Grid& g = Grid{}) {
    const int kLat = g.lat, kLon = g.lon;
    MeshData d;
    const float l0 = latFrom(p), l1 = latTo(p), B = p.size.y / 2, C = p.size.z / 2;
    const float eps = 1e-3f;
    for (int r = 0; r <= kLat; r++) {
        const float lat = l0 + (l1 - l0) * r / kLat;
        for (int c = 0; c <= kLon; c++) {
            const float u = (float)c / kLon, lon = 2 * PI * u - PI / 2;
            const Vector3 q = superPoint(p, lat, lon);
            const Vector3 dLat = Vector3Subtract(superPoint(p, std::min(lat + eps, PI / 2), lon), superPoint(p, std::max(lat - eps, -PI / 2), lon));
            const Vector3 dLon = Vector3Subtract(superPoint(p, lat, lon + eps), superPoint(p, lat, lon - eps));
            Vector3 n = Vector3CrossProduct(dLat, dLon);
            if (Vector3Length(n) < 1e-9f) n = { 0, lat > 0 ? 1.f : -1.f, 0 };
            n = Vector3Normalize(n);
            if (Vector3DotProduct(n, q) < 0) n = Vector3Negate(n);
            d.pos.insert(d.pos.end(), { q.x, q.y, q.z });
            d.nrm.insert(d.nrm.end(), { n.x, n.y, n.z });
            Vector2 t;
            if (p.paint == Paint::Outfit) t = outfitUV(p, u, 1 - (q.y / std::max(1e-6f, B) + 1) / 2);
            else if (p.paint == Paint::Label) {
                // Flat across the front. An opaque card is mirrored on its back so that side reads
                // too; a see-through one is not, or its back would show through reversed.
                const bool mirror = q.z < 0 && p.color.a == 255;
                t = { 0.5f + (mirror ? -q.x : q.x) / p.size.x, 0.5f - q.y / p.size.y };
            }
            else if (p.paint == Paint::Face) {
                t = { 0.5f + q.x / p.size.x, 0.5f - q.y / p.size.y };
                if (q.z < -0.05f * C) t = { 0.004f, 0.004f };   // the back of the head: plain skin
            } else t = { u, (float)r / kLat };
            d.uv.insert(d.uv.end(), { t.x, t.y });
        }
    }
    for (int r = 0; r < kLat; r++)
        for (int c = 0; c < kLon; c++) {
            const auto a00 = (unsigned short)(r * (kLon + 1) + c), a01 = (unsigned short)(a00 + 1);
            const auto a10 = (unsigned short)(a00 + kLon + 1), a11 = (unsigned short)(a10 + 1);
            d.idx.insert(d.idx.end(), { a00, a10, a11, a00, a11, a01 });
        }
    return d;
}

// A limb tube for the given joint frames: positions and normals (and, once, uvs/indices).
// Rings run from joint a to c with hemispherical caps; round each ring the front (the
// first joint's +z) is at u = 0.5.
void tubeMesh(const Frames& f, const Prim& p, float pop, MeshData& d, bool topology, const Grid& g = Grid{}) {
    const int kTubeCap = g.tubeCap, kTubeSides = g.tubeSides;
    const std::vector<Vector3> pts = tubeCurve(f, p, g.tubeBody);
    const int n = (int)pts.size();
    std::vector<Vector3> T(n), N(n), Bn(n);
    for (int i = 0; i < n; i++) {
        const Vector3 a = pts[(size_t)std::max(0, i - 1)], b = pts[(size_t)std::min(n - 1, i + 1)];
        Vector3 t = Vector3Subtract(b, a);
        T[(size_t)i] = Vector3Length(t) > 1e-7f ? Vector3Normalize(t) : Vector3{ 0, -1, 0 };
    }
    const Matrix& fa = f[(size_t)p.a];
    Vector3 fwd = Vector3Subtract(Vector3Transform({ 0, 0, 1 }, fa), Vector3Transform({ 0, 0, 0 }, fa));
    for (int i = 0; i < n; i++) {
        const Vector3 t = T[(size_t)i];
        Vector3 nn = Vector3Subtract(fwd, Vector3Scale(t, Vector3DotProduct(fwd, t)));
        if (Vector3Length(nn) < 1e-5f) nn = Vector3Perpendicular(t);
        nn = Vector3Normalize(nn);
        N[(size_t)i] = nn;
        Bn[(size_t)i] = Vector3CrossProduct(t, nn);
        fwd = nn;                                          // parallel transport
    }
    struct Ring { Vector3 c; float r; int at; float along, v; };
    std::vector<Ring> rings;
    for (int k = kTubeCap; k >= 1; k--) {
        const float th = PI / 2 * k / kTubeCap;
        rings.push_back({ Vector3Subtract(pts[0], Vector3Scale(T[0], p.r0 * pop * std::sin(th))), p.r0 * pop * std::cos(th), 0, -std::sin(th), 0 });
    }
    for (int i = 0; i < n; i++) {
        const float t = (float)i / (n - 1);
        rings.push_back({ pts[(size_t)i], (p.r0 + (p.r1 - p.r0) * t) * pop, i, 0, t });
    }
    for (int k = 1; k <= kTubeCap; k++) {
        const float th = PI / 2 * k / kTubeCap;
        rings.push_back({ Vector3Add(pts[(size_t)n - 1], Vector3Scale(T[(size_t)n - 1], p.r1 * pop * std::sin(th))), p.r1 * pop * std::cos(th), n - 1, std::sin(th), 1 });
    }
    d.pos.clear(); d.nrm.clear();
    if (topology) { d.uv.clear(); d.idx.clear(); }
    for (const Ring& g : rings) {
        const Vector3 t = T[(size_t)g.at], nn = N[(size_t)g.at], bb = Bn[(size_t)g.at];
        const float c = std::sqrt(std::max(0.f, 1 - g.along * g.along));
        for (int s = 0; s <= kTubeSides; s++) {
            const float u = (float)s / kTubeSides, phi = (u - 0.5f) * 2 * PI;
            const Vector3 radial = Vector3Add(Vector3Scale(nn, std::cos(phi)), Vector3Scale(bb, std::sin(phi)));
            const Vector3 q = Vector3Add(g.c, Vector3Scale(radial, g.r));
            const Vector3 nrm = Vector3Normalize(Vector3Add(Vector3Scale(radial, c), Vector3Scale(t, g.along)));
            d.pos.insert(d.pos.end(), { q.x, q.y, q.z });
            d.nrm.insert(d.nrm.end(), { nrm.x, nrm.y, nrm.z });
            if (topology) {
                const Vector2 uv = p.paint == Paint::Outfit ? outfitUV(p, u, g.v) : Vector2{ u, g.v };
                d.uv.insert(d.uv.end(), { uv.x, uv.y });
            }
        }
    }
    if (topology) {
        const int R = (int)rings.size();
        for (int r = 0; r + 1 < R; r++)
            for (int s = 0; s < kTubeSides; s++) {
                const auto a00 = (unsigned short)(r * (kTubeSides + 1) + s), a01 = (unsigned short)(a00 + 1);
                const auto a10 = (unsigned short)(a00 + kTubeSides + 1), a11 = (unsigned short)(a10 + 1);
                d.idx.insert(d.idx.end(), { a00, a01, a11, a00, a11, a10 });
            }
    }
}

g3d::Material materialFor(const Look& l, const Prim& p) {
    g3d::Material m;
    m.color = paintColor(l, p);
    switch (l.finish) {
    case Finish::Plastic: m.roughness = 0.32f; break;
    case Finish::Matte: m.roughness = 0.8f; break;
    case Finish::Toon:
        m.shading = g3d::Shading::Toon;
        m.roughness = 0.6f;
        m.toon.rim = { 255, 255, 255, 70 };
        break;
    }
    if (p.paint == Paint::Metal) { m.metalness = 0.75f; m.roughness = 0.3f; }
    if (p.paint == Paint::Dark) m.roughness = std::min(m.roughness, 0.55f);
    if (p.paint == Paint::Hair && l.finish == Finish::Matte) m.roughness = 0.62f;
    if (p.roughness >= 0) m.roughness = p.roughness;
    if (p.kind == Prim::Kind::Super && (p.from > -0.99f || p.to < 0.99f)) m.doubleSided = true;
    if ((p.paint == Paint::Fixed || p.paint == Paint::Label) && p.color.a < 255) {   // visors, shields
        m.opacity = std::max(0.05f, p.color.a / 255.f);
        m.roughness = 0.12f;
        m.doubleSided = true;
    }
    return m;
}

struct Built {
    struct Part { g3d::Object3D* obj; int look; std::string slot; Vector3 base; };
    struct Tube { g3d::Object3D* obj; g3d::GeometryPtr geo; Prim prim; int look; std::string slot; };
    std::array<g3d::Object3D*, kMaxJoints> joints{};
    std::vector<Part> parts;
    std::vector<Tube> tubes;
};
} // namespace

g3d::Object3D* add(g3d::Object3D& parentNode, CharacterPtr ch, int fps) {
    g3d::Object3D* root = parentNode.add();
    root->name = "character";
    g3d::Object3D* inner = root->add();
    inner->name = "rig";
    auto built = std::make_shared<Built>();
    const Build& build0 = ch->looks()[0].build;
    for (int i = 0; i < jointCount(build0); i++) {
        const Joint j = (Joint)i;
        // Grounded joints (Floor) hang from the character's origin like Root, so they do not
        // take the pose's offset.
        g3d::Object3D* node = isGrounded(build0, j) ? inner->add() : built->joints[(size_t)parent(build0, j)]->add();
        node->name = std::string("rig:") + name(build0, j);
        node->position = restOffset(build0, j);
        built->joints[(size_t)i] = node;
    }

    std::map<unsigned, g3d::DynamicTexturePtr> outfits;
    const auto& looks = ch->looks();
    for (int k = 0; k < (int)looks.size(); k++) {
        const Look& look = looks[(size_t)k];
        g3d::DynamicTexturePtr outfitTex;
        if (look.outfit.painted()) {
            auto& tex = outfits[look.outfit.id];
            if (!tex) {
                tex = std::make_shared<g3d::DynamicTexture>();
                tex->width = tex->height = std::max(16, look.outfit.size);
                tex->supersample = 1;
                const auto paint = look.outfit.paint;
                const float size = (float)tex->width;
                tex->draw = [paint, size](const Ctx& c, int t) { paint(c, t, size); };
            }
            outfitTex = tex;
        }
        g3d::DynamicTexturePtr faceTex;
        if (look.face) {
            faceTex = std::make_shared<g3d::DynamicTexture>();
            faceTex->width = faceTex->height = 512;
            const FacePtr face = look.face;
            const Color skin = look.skin;
            const Rectangle fb = look.faceBox;
            std::weak_ptr<Character> weak = ch;
            faceTex->draw = [face, skin, fb, weak, fps](const Ctx& c, int t) {
                vg::fill(c, vg::Path::rect({ 0, 0, 512, 512 }), skin);
                FaceState st;
                if (auto chr = weak.lock()) st = chr->face.at((double)t / fps);
                st.skin = skin;
                face->draw(c, { fb.x * 512, fb.y * 512, fb.width * 512, fb.height * 512 }, st);
            };
        }
        for (const std::string& slot : slotKeys()) {
            for (const Prim& p : prims(look, slot)) {
                g3d::Material m = materialFor(look, p);
                if (p.paint == Paint::Outfit && outfitTex) { m.dynamicMap = outfitTex; m.color = WHITE; }
                if (p.paint == Paint::Face && faceTex) { m.dynamicMap = faceTex; m.color = WHITE; }
                if (p.paint == Paint::Label) {
                    auto tex = std::make_shared<g3d::DynamicTexture>();
                    tex->width = 512;
                    tex->height = std::max(16, (int)std::lround(512 * p.size.y / std::max(1e-4f, p.size.x)));
                    tex->every = 1 << 30;                          // drawn once: the text never changes
                    const std::string text = p.label;
                    const Color bg = p.color;
                    const float w = (float)tex->width, h = (float)tex->height;
                    tex->draw = [text, bg, w, h](const Ctx& c, int) { detail::drawLabel(c, { 0, 0, w, h }, text, bg); };
                    m.dynamicMap = tex;
                    m.color = WHITE;
                    if (bg.a < 255) m.opacity = 0.99f;            // the texture's alpha carries it
                }
                const std::string nm = "look" + std::to_string(k) + ":" + slot + ":" + p.name;
                if (p.kind == Prim::Kind::Tube) {
                    const auto frames = solve(look.build, clips::rest()(0));
                    auto geo = std::make_shared<g3d::Geometry>([frames, p] {
                        MeshData d;
                        tubeMesh(frames, p, 1, d, true);
                        return toMesh(d);
                    });
                    g3d::Object3D* o = inner->add(geo, m);
                    o->name = nm;
                    built->tubes.push_back({ o, geo, p, k, slot });
                    continue;
                }
                g3d::Object3D* o = nullptr;
                g3d::Object3D* at = built->joints[(size_t)p.joint];
                if (p.kind == Prim::Kind::Model) {
                    o = at->add(g3d::loadModel(p.model));
                    o->scale = p.size;
                } else if (p.kind == Prim::Kind::Cone) {
                    o = at->add(g3d::Geometry::cone(p.size.x / 2, p.size.y, 24), m);
                } else {
                    const Prim shape = p;
                    o = at->add(std::make_shared<g3d::Geometry>([shape] { return toMesh(superMesh(shape)); }), m);
                }
                o->name = nm;
                o->position = p.at;
                o->rotation = p.rot;
                built->parts.push_back({ o, k, slot, o->scale });
            }
        }
    }

    inner->up = [ch, built, fps](int t, g3d::Object3D&) {
        const double s = (double)t / fps;
        const Pose pose = ch->poseAt(s);
        const Build& build = ch->lookAt(s).build;
        for (int i = 0; i < jointCount(build); i++) {
            if (!built->joints[(size_t)i]) continue;              // a look with a larger plan than the first
            const Joint j = (Joint)i;
            Vector3 off = restOffset(build, j);
            if (j == Joint::Root) off = Vector3Add(off, pose.offset);
            built->joints[(size_t)i]->position = off;
            built->joints[(size_t)i]->rotation = jointRotation(build, pose, j);
        }
        std::map<std::string, std::pair<int, float>> active;    // slot -> look index, pop
        auto state = [&](const std::string& slot) {
            auto it = active.find(slot);
            if (it == active.end()) it = active.emplace(slot, std::make_pair(ch->lookIndex(slot, s), ch->popAt(slot, s))).first;
            return it->second;
        };
        for (auto& part : built->parts) {
            const auto [k, pop] = state(part.slot);
            part.obj->visible = part.look == k && pop > 0.001f;
            part.obj->scale = Vector3Scale(part.base, pop);
        }
        bool solved = false;
        Frames frames{};
        for (auto& tube : built->tubes) {
            const auto [k, pop] = state(tube.slot);
            tube.obj->visible = tube.look == k && pop > 0.001f;
            if (!tube.obj->visible) continue;
            if (!solved) { frames = solve(build, pose); solved = true; }
            auto data = std::make_shared<MeshData>();
            Prim prim = tube.prim;
            if (ch->softness) prim.softness = std::clamp(ch->softness(s), 0.f, 1.f);
            tubeMesh(frames, prim, pop, *data, false);
            tube.geo->update([data](Mesh& m) {
                const size_t n = std::min(data->pos.size(), (size_t)m.vertexCount * 3);
                std::memcpy(m.vertices, data->pos.data(), n * sizeof(float));
                std::memcpy(m.normals, data->nrm.data(), n * sizeof(float));
            });
        }
    };
    return root;
}

namespace {
// A cone, apex +y and centred, as Geometry::cone makes it.
MeshData coneData(float radius, float height, int slices) {
    MeshData d;
    const float h2 = height / 2, slope = radius / std::max(1e-6f, height);
    for (int i = 0; i <= slices; i++) {
        const float a = 2 * PI * i / slices, c = std::cos(a), s = std::sin(a);
        const Vector3 n = Vector3Normalize({ c, slope, s });
        d.pos.insert(d.pos.end(), { radius * c, -h2, radius * s });
        d.nrm.insert(d.nrm.end(), { n.x, n.y, n.z });
        d.uv.insert(d.uv.end(), { (float)i / slices, 1.f });
        d.pos.insert(d.pos.end(), { 0, h2, 0 });
        d.nrm.insert(d.nrm.end(), { n.x, n.y, n.z });
        d.uv.insert(d.uv.end(), { (float)i / slices, 0.f });
    }
    for (int i = 0; i < slices; i++) {
        const auto a = (unsigned short)(i * 2), b = (unsigned short)(i * 2 + 1), c = (unsigned short)(i * 2 + 2);
        d.idx.insert(d.idx.end(), { a, c, b });
    }
    const auto centre = (unsigned short)(d.pos.size() / 3);
    d.pos.insert(d.pos.end(), { 0, -h2, 0 });
    d.nrm.insert(d.nrm.end(), { 0, -1, 0 });
    d.uv.insert(d.uv.end(), { 0.5f, 0.5f });
    for (int i = 0; i <= slices; i++) {
        const float a = 2 * PI * i / slices;
        d.pos.insert(d.pos.end(), { radius * std::cos(a), -h2, radius * std::sin(a) });
        d.nrm.insert(d.nrm.end(), { 0, -1, 0 });
        d.uv.insert(d.uv.end(), { 0.5f, 0.5f });
    }
    for (int i = 0; i < slices; i++)
        d.idx.insert(d.idx.end(), { centre, (unsigned short)(centre + 1 + i), (unsigned short)(centre + 2 + i) });
    return d;
}
} // namespace

std::shared_ptr<g3d::AnimatedMesh> bakeAnimated(const Look& look, const std::vector<BakeClip>& clips,
                                                std::vector<BakedClip>* where, bool coarse) {
    if (clips.empty()) throw std::runtime_error("rig::bakeAnimated: no clips");
    const Grid grid = coarse ? Grid{ 8, 12, 6, 2, 8 } : Grid{};
    // What to bake: opaque shapes (a visor, a model file, a see-through label are left out).
    std::vector<Prim> shapes;
    for (const Prim& p : prims(look))
        if (p.kind != Prim::Kind::Model && !((p.paint == Paint::Fixed || p.paint == Paint::Label) && p.color.a < 255))
            shapes.push_back(p);
    std::vector<MeshData> local(shapes.size());           // rigid shapes in their own space
    for (size_t i = 0; i < shapes.size(); i++) {
        const Prim& p = shapes[i];
        if (p.kind == Prim::Kind::Cone) local[i] = coneData(p.size.x / 2, p.size.y, 10);
        else if (p.kind != Prim::Kind::Tube) local[i] = superMesh(p, grid);
    }

    auto am = std::make_shared<g3d::AnimatedMesh>();
    auto topo = std::make_shared<MeshData>();
    auto colours = std::make_shared<std::vector<unsigned char>>();
    Vector3 lo{ 1e9f, 1e9f, 1e9f }, hi{ -1e9f, -1e9f, -1e9f };
    int frame = 0;
    for (const BakeClip& bc : clips) {
        if (bc.frames <= 0) throw std::runtime_error("rig::bakeAnimated: a clip needs frames");
        if (where) where->push_back({ frame, bc.frames });
        for (int k = 0; k < bc.frames; k++, frame++) {
            const Frames frames = solve(look.build, locked(look, bc.clip(bc.seconds * k / bc.frames)));
            const bool first = frame == 0;
            int vertices = 0;
            for (size_t i = 0; i < shapes.size(); i++) {
                const Prim& p = shapes[i];
                MeshData tube;
                const MeshData* src = &local[i];
                Matrix m = MatrixIdentity();
                if (p.kind == Prim::Kind::Tube) {
                    tubeMesh(frames, p, 1, tube, first, grid);
                    src = &tube;
                } else {
                    m = MatrixMultiply(MatrixMultiply(MatrixRotateXYZ(Vector3Scale(p.rot, DEG2RAD)), MatrixTranslate(p.at.x, p.at.y, p.at.z)),
                                       frames[(size_t)p.joint]);
                }
                const int n = (int)src->pos.size() / 3;
                for (int v = 0; v < n; v++) {
                    Vector3 q{ src->pos[(size_t)v * 3], src->pos[(size_t)v * 3 + 1], src->pos[(size_t)v * 3 + 2] };
                    Vector3 nv{ src->nrm[(size_t)v * 3], src->nrm[(size_t)v * 3 + 1], src->nrm[(size_t)v * 3 + 2] };
                    if (p.kind != Prim::Kind::Tube) {
                        q = Vector3Transform(q, m);
                        nv = Vector3Normalize(Vector3Subtract(Vector3Transform(nv, m), Vector3Transform({ 0, 0, 0 }, m)));
                    }
                    am->positions.insert(am->positions.end(), { q.x, q.y, q.z });
                    am->normals.insert(am->normals.end(), { nv.x, nv.y, nv.z });
                    lo = Vector3Min(lo, q);
                    hi = Vector3Max(hi, q);
                }
                if (first) {
                    const auto base = (unsigned short)vertices;
                    if (vertices + n > 65535) throw std::runtime_error("rig::bakeAnimated: too many vertices for one mesh");
                    for (const unsigned short ix : src->idx) topo->idx.push_back((unsigned short)(base + ix));
                    topo->uv.insert(topo->uv.end(), src->uv.begin(), src->uv.end());
                    const Color c = paintColor(look, p);
                    for (int v = 0; v < n; v++) colours->insert(colours->end(), { c.r, c.g, c.b, c.a });
                }
                vertices += n;
            }
            if (first) am->vertices = vertices;
            else if (vertices != am->vertices) throw std::runtime_error("rig::bakeAnimated: the mesh changed topology between frames");
        }
    }
    am->frames = frame;
    am->centre = Vector3Scale(Vector3Add(lo, hi), 0.5f);
    float r = 0;
    for (size_t i = 0; i + 2 < am->positions.size(); i += 3)
        r = std::max(r, Vector3Distance(am->centre, { am->positions[i], am->positions[i + 1], am->positions[i + 2] }));
    am->radius = r + 0.05f;
    // The rest mesh is frame 0: it carries the indices, uvs and colours the shader keeps.
    const size_t n3 = (size_t)am->vertices * 3;
    topo->pos.assign(am->positions.begin(), am->positions.begin() + (std::ptrdiff_t)n3);
    topo->nrm.assign(am->normals.begin(), am->normals.begin() + (std::ptrdiff_t)n3);
    am->rest = std::make_shared<g3d::Geometry>([topo, colours] {
        return toMesh(*topo, colours.get());
    });
    return am;
}

g3d::Object3D* add(g3d::Scene& scene, CharacterPtr ch, int fps) {
    g3d::Object3D* holder = scene.add();
    holder->name = "character-holder";
    g3d::Object3D* root = add(*holder, ch, fps);
    // Hand the character's root to the scene directly so callers move what they get back.
    std::unique_ptr<g3d::Object3D> mine = std::move(holder->children.front());
    holder->children.clear();
    scene.objects.back() = std::move(mine);
    return root;
}

} // namespace rig
} // namespace rmtn
