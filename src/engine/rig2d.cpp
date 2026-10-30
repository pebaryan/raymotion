// rig:: 2D -- the character as a flat cut-out puppet: its shapes projected, outlined and
// drawn back to front; and the "character" prefab.
#include "rig_internal.hpp"
#include "rmtn/prefab.hpp"
#include "rmtn/render.hpp"
#include <rlgl.h>
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>

namespace rmtn {
namespace rig {

namespace {
using namespace detail;

Color darker(Color c, float k) {
    return { (unsigned char)(c.r * (1 - k)), (unsigned char)(c.g * (1 - k)), (unsigned char)(c.b * (1 - k)), c.a };
}
float cross(Vector2 o, Vector2 a, Vector2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }

std::vector<Vector2> hull(std::vector<Vector2> p) {
    std::sort(p.begin(), p.end(), [](Vector2 a, Vector2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
    if (p.size() < 3) return p;
    std::vector<Vector2> h(p.size() * 2);
    size_t k = 0;
    for (size_t i = 0; i < p.size(); i++) {
        while (k >= 2 && cross(h[k - 2], h[k - 1], p[i]) <= 0) k--;
        h[k++] = p[i];
    }
    for (size_t i = p.size() - 1, t = k + 1; i-- > 0;) {
        while (k >= t && cross(h[k - 2], h[k - 1], p[i]) <= 0) k--;
        h[k++] = p[i];
    }
    h.resize(k - 1);
    return h;
}

// Character space -> view space (x right, y up, z towards the viewer).
Vector3 viewSpace(const View2D& v, Vector3 p) {
    const float y = -v.yaw * DEG2RAD, pt = v.pitch * DEG2RAD;
    const Vector3 a{ p.x * std::cos(y) + p.z * std::sin(y), p.y, -p.x * std::sin(y) + p.z * std::cos(y) };
    return { a.x, a.y * std::cos(pt) - a.z * std::sin(pt), a.y * std::sin(pt) + a.z * std::cos(pt) };
}

// SVG stand-ins for model parts, parsed once.
std::shared_ptr<const vg::Drawing> flatDrawing(const std::string& file) {
    static std::mutex mu;
    static std::map<std::string, std::shared_ptr<const vg::Drawing>> cache;
    std::lock_guard<std::mutex> lock(mu);
    auto& d = cache[file];
    if (!d) d = std::make_shared<vg::Drawing>(vg::Drawing::load(file));
    return d;
}

// The part of a convex polygon where f(p) = dot(p - o, n) - k >= 0.
std::vector<Vector2> clipHalf(const std::vector<Vector2>& poly, Vector2 o, Vector2 n, float k) {
    std::vector<Vector2> out;
    auto f = [&](Vector2 p) { return (p.x - o.x) * n.x + (p.y - o.y) * n.y - k; };
    for (size_t i = 0; i < poly.size(); i++) {
        const Vector2 a = poly[i], b = poly[(i + 1) % poly.size()];
        const float fa = f(a), fb = f(b);
        if (fa >= 0) out.push_back(a);
        if ((fa >= 0) != (fb >= 0)) out.push_back(Vector2Lerp(a, b, fa / (fa - fb)));
    }
    return out;
}

// A cap of hair's outline: its convex hull with the stretch of hairline that runs inside
// the hull (over the brow, where the hairline arches up) put back in. `rim` is the hairline
// as a closed loop, `front` its point over the brow; `top` picks the hull's arc over the crown.
std::vector<Vector2> withHairline(const std::vector<Vector2>& outline, const std::vector<Vector2>& rim, size_t front,
                                  Vector2 top) {
    const size_t n = outline.size(), Q = rim.size();
    if (n < 3 || Q < 3) return outline;
    // The hull edge nearest a point, and how far it is.
    auto nearest = [&](Vector2 p, float* dist) {
        size_t best = 0;
        float bd = 1e30f;
        for (size_t i = 0; i < n; i++) {
            const Vector2 a = outline[i], b = outline[(i + 1) % n], ab = Vector2Subtract(b, a);
            const float t = std::clamp(Vector2DotProduct(Vector2Subtract(p, a), ab) / std::max(1e-9f, Vector2DotProduct(ab, ab)), 0.f, 1.f);
            const float d = Vector2Distance(p, Vector2Add(a, Vector2Scale(ab, t)));
            if (d < bd) { bd = d; best = i; }
        }
        if (dist) *dist = bd;
        return best;
    };
    auto onHull = [&](size_t i) { float d; nearest(rim[i % Q], &d); return d < 0.75f; };
    if (onHull(front)) return outline;
    size_t a = front, b = front, steps = 0;
    while (!onHull(a + Q - 1) && ++steps < Q) a = (a + Q - 1) % Q;
    while (!onHull(b + 1) && ++steps < Q) b = (b + 1) % Q;
    if (steps >= Q) return outline;
    a = (a + Q - 1) % Q;
    b = (b + 1) % Q;
    const size_t ea = nearest(rim[a], nullptr), eb = nearest(rim[b], nullptr);
    size_t t = 0;
    for (size_t i = 1; i < n; i++)
        if (Vector2Distance(outline[i], top) < Vector2Distance(outline[t], top)) t = i;
    // From b back round to a, the way that passes over the crown.
    std::vector<Vector2> forward, backward;
    bool crownForward = false;
    for (size_t k = 0, cnt = ea == eb ? n : (ea + n - eb) % n; k < cnt; k++) {
        const size_t i = (eb + 1 + k) % n;
        forward.push_back(outline[i]);
        crownForward |= i == t;
    }
    for (size_t k = 0, cnt = (eb + n - ea) % n; k < cnt; k++) backward.push_back(outline[(eb + n - k) % n]);
    std::vector<Vector2> out;
    for (size_t i = a;; i = (i + 1) % Q) {
        out.push_back(rim[i]);
        if (i == b) break;
    }
    const auto& arc = crownForward ? forward : backward;
    out.insert(out.end(), arc.begin(), arc.end());
    return out;
}

// A face turned away from the viewer narrows but keeps its height. Faces fit their box
// with uniform scale, so the face is drawn at its unturned size into this canvas, then
// placed as a quad squashed across. Drawn and placed one face at a time (drawToTexture
// flushes the previous quad before overwriting the canvas).
constexpr int kFaceCanvas = 1024;
const RenderTexture& faceCanvas() {
    static RenderTexture rt{};
    if (!rt.id) {
        rlDrawRenderBatchActive();      // creating a render texture rebinds the framebuffer
        rt = LoadRenderTexture(kFaceCanvas, kFaceCanvas);
        onGraphicsShutdown([] { UnloadRenderTexture(rt); rt = RenderTexture{}; });
    }
    return rt;
}

// Something painted into a w x h box at full size, then placed centred on `at`, turned by
// `angle` and narrowed across by `squash` (faces, the text on placards).
void drawSquashed(const Ctx& c, const std::function<void(Rectangle)>& paint, Vector2 at, float angle, float w, float h,
                  float squash) {
    // The canvas box keeps the box's aspect, so the drawing fits it exactly as it would on screen.
    const float k = kFaceCanvas / std::max(w, h);
    const Rectangle box{ 0, 0, w * k, h * k };
    drawToTexture(faceCanvas(), 1, [&] { paint(box); });
    const Vector2 p = mapP(c, at);
    const float dw = w * squash * c.s, dh = h * c.s;
    const unsigned char a = (unsigned char)std::lround(255 * std::clamp(c.a, 0.f, 1.f));
    DrawTexturePro(faceCanvas().texture, box, { p.x, p.y, dw, dh }, { dw / 2, dh / 2 }, c.rot + angle,
                   Color{ 255, 255, 255, a });
}
void drawTurnedFace(const Ctx& c, const FacePtr& f, const FaceState& st, Vector2 at, float angle, float w, float h,
                    float squash) {
    drawSquashed(c, [&](Rectangle box) { f->draw(Ctx{}, box, st); }, at, angle, w, h, squash);
}

struct Item {
    float depth;
    std::function<void()> draw;
};
} // namespace

Vector2 project(const View2D& v, Vector3 p) {
    const Vector3 q = viewSpace(v, p);
    return { v.origin.x + q.x * v.scale, v.origin.y - q.y * v.scale };
}

void draw(const Ctx& c, const Character& ch, double s, const View2D& view) {
    const Look& now = ch.lookAt(s);
    const Pose pose = ch.poseAt(s);
    const auto frames = solve(now.build, pose);
    const float chestDepth = viewSpace(view, position(frames, Joint::Chest)).z;
    const float lw = view.outline;
    vg::Stroke ink(view.ink, lw);
    std::vector<Item> items;

    for (const std::string& slot : slotKeys()) {
        const int k = ch.lookIndex(slot, s);
        const float pop = ch.popAt(slot, s);
        if (pop <= 0.001f) continue;
        const Look& look = ch.looks()[(size_t)k];
        for (const Prim& p : prims(look, slot)) {
            if (p.kind == Prim::Kind::Tube) {
                Prim soft = p;
                if (ch.softness) soft.softness = std::clamp(ch.softness(s), 0.f, 1.f);
                const auto curve = tubeCurve(frames, soft, 24);
                std::vector<Vector2> pts;
                float depth = 0;
                for (const Vector3& q : curve) {
                    pts.push_back(project(view, q));
                    depth += viewSpace(view, q).z;
                }
                depth /= (float)curve.size();
                const float width = (p.r0 + p.r1) * pop * view.scale;
                const bool far = view.shade && depth < chestDepth - now.build.torso.z * 0.3f;
                // Colour bands along the limb (sleeves, cuffs) from the outfit.
                std::vector<std::pair<float, Color>> bands;
                const int K = 16;
                for (int i = 0; i < K; i++) {
                    const float v = (i + 0.5f) / K;
                    Color col = p.paint == Paint::Outfit && look.outfit.sample
                                    ? look.outfit.sample(p.region, 0.5f, p.v0 + v * (p.v1 - p.v0)) : paintColor(look, p);
                    if (far) col = darker(col, 0.14f);
                    if (bands.empty() || std::memcmp(&bands.back().second, &col, sizeof(Color)) != 0) bands.push_back({ (float)i / K, col });
                }
                items.push_back({ depth, [pts, width, bands, ink, lw, c]() {
                    const vg::Path path = vg::Path::polyline(pts);
                    if (lw > 0) vg::stroke(c, path, vg::Stroke(ink.color, width + 2 * lw));
                    for (size_t i = 0; i < bands.size(); i++) {
                        const float a = bands[i].first, b = i + 1 < bands.size() ? bands[i + 1].first : 1.f;
                        vg::Stroke st(bands[i].second, width);
                        st.cap = vg::Cap::Butt;
                        vg::stroke(c, path.trim(a, b), st);
                    }
                    vg::fill(c, vg::Path::circle(pts.front(), width / 2), bands.front().second);
                    vg::fill(c, vg::Path::circle(pts.back(), width / 2), bands.back().second);
                } });
                continue;
            }
            const Matrix M = MatrixMultiply(primMatrix(p, pop), frames[(size_t)p.joint]);
            const Vector3 centre = Vector3Transform({ 0, 0, 0 }, M);
            const float depth = viewSpace(view, centre).z + p.depth2d;
            const bool far = view.shade && depth < chestDepth - now.build.torso.z * 0.3f;
            if (p.kind == Prim::Kind::Model) {
                if (p.svg.empty()) continue;
                const Vector2 at = project(view, centre);
                const Vector2 sz{ p.flat.x * view.scale * pop, p.flat.y * view.scale * pop };
                auto drawing = flatDrawing(p.svg);
                items.push_back({ depth, [drawing, at, sz, c]() {
                    vg::draw(c, *drawing, { at.x - sz.x / 2, at.y - sz.y / 2, sz.x, sz.y });
                } });
                continue;
            }
            std::vector<Vector2> pts, rim;      // rim: a cap of hair's hairline, one point per longitude
            if (p.kind == Prim::Kind::Cone) {
                pts.push_back(project(view, Vector3Transform({ 0, p.size.y / 2, 0 }, M)));
                for (int i = 0; i < 16; i++) {
                    const float a = 2 * PI * i / 16;
                    pts.push_back(project(view, Vector3Transform({ std::cos(a) * p.size.x / 2, -p.size.y / 2, std::sin(a) * p.size.z / 2 }, M)));
                }
            } else {
                const float l1 = latTo(p);
                // A cap of hair: as it faces the viewer its hairline rises towards `from2d` over
                // the brow and falls back to `from` at the temples, so it arches.
                float facing = 0;
                if (p.from2d >= -1) {
                    const Vector3 front = Vector3Subtract(viewSpace(view, Vector3Transform({ 0, 0, 1 }, M)), viewSpace(view, centre));
                    facing = std::max(0.f, front.z / std::max(1e-6f, Vector3Length(front)));
                }
                const int Q = facing > 0 ? 48 : 24;
                for (int q = 0; q < Q; q++) {
                    const float lon = 2 * PI * q / Q;
                    Prim raised = p;
                    raised.from = p.from + (p.from2d - p.from) * std::sqrt(facing) * std::pow(std::max(0.f, std::sin(lon)), 2.5f);
                    const float l0 = latFrom(raised);
                    for (int r = 0; r <= 10; r++) {
                        pts.push_back(project(view, Vector3Transform(superPoint(p, l0 + (l1 - l0) * r / 10, lon), M)));
                        if (r == 0 && facing > 0) rim.push_back(pts.back());
                    }
                }
            }
            Color col = paintColor(look, p);
            if (far) col = darker(col, 0.14f);
            auto outline = hull(pts);
            if (!rim.empty())
                outline = withHairline(outline, rim, rim.size() / 4,
                                       project(view, Vector3Transform({ 0, p.size.y / 2, 0 }, M)));
            // Outfit parts: the texture's rows as bands across the part (stripes stay stripes).
            std::vector<std::pair<std::vector<Vector2>, Color>> bands;
            if (p.paint == Paint::Outfit && look.outfit.sample && p.kind == Prim::Kind::Super && outline.size() >= 3) {
                const int N = 32;
                std::vector<Color> rows = outfitRows(look.outfit, p.region, p.v0, p.v1, N);
                const Vector2 top = project(view, Vector3Transform({ 0, p.size.y / 2, 0 }, M));
                const Vector2 bottom = project(view, Vector3Transform({ 0, -p.size.y / 2, 0 }, M));
                const float len = Vector2Distance(top, bottom);
                if (len > 1e-3f) {
                    const Vector2 d = Vector2Scale(Vector2Subtract(bottom, top), 1 / len);
                    for (int k0 = 0; k0 < N;) {
                        int k1 = k0 + 1;
                        while (k1 < N && std::memcmp(&rows[(size_t)k1], &rows[(size_t)k0], sizeof(Color)) == 0) k1++;
                        std::vector<Vector2> band = outline;
                        if (k0 > 0) band = clipHalf(band, top, d, len * k0 / N);
                        if (k1 < N) band = clipHalf(band, top, Vector2Negate(d), -len * k1 / N);
                        if (band.size() >= 3) bands.push_back({ band, far ? darker(rows[(size_t)k0], 0.14f) : rows[(size_t)k0] });
                        k0 = k1;
                    }
                }
            }
            std::function<void()> face;
            if (p.paint == Paint::Face && look.face) {
                const Vector3 front = Vector3Subtract(viewSpace(view, Vector3Transform({ 0, 0, 1 }, M)), viewSpace(view, centre));
                const float facing = front.z / std::max(1e-6f, Vector3Length(front));
                if (facing > 0.12f) {
                    const Rectangle fb = look.faceBox;
                    const Vector3 local{ (fb.x + fb.width / 2 - 0.5f) * p.size.x, (0.5f - (fb.y + fb.height / 2)) * p.size.y, p.size.z * 0.46f };
                    const Vector2 at = project(view, Vector3Transform(local, M));
                    const Vector2 up = Vector2Subtract(project(view, Vector3Transform({ local.x, local.y + 0.1f, local.z }, M)), at);
                    const float angle = std::atan2(up.x, -up.y) * RAD2DEG;
                    // Full size across and down; turning only foreshortens it across.
                    const float w = fb.width * p.size.x * view.scale * pop;
                    const float h = fb.height * p.size.y * view.scale * pop;
                    const float squash = std::clamp(facing, 0.f, 1.f);
                    FaceState st = ch.face.at(s);
                    st.skin = look.skin;
                    const FacePtr f = look.face;
                    face = [f, st, at, angle, w, h, squash, c]() {
                        if (squash > 0.995f) {          // facing the viewer: draw the vectors directly
                            Ctx fc;
                            fc.o = mapP(c, at);
                            fc.s = c.s;
                            fc.rot = c.rot + angle;
                            fc.a = c.a;
                            f->draw(fc, { -w / 2, -h / 2, w, h }, st);
                        } else drawTurnedFace(c, f, st, at, angle, w, h, squash);
                    };
                }
            }
            // A label's text across whichever face of the card is towards the viewer.
            std::function<void()> label;
            if (p.paint == Paint::Label && !p.label.empty() && p.kind == Prim::Kind::Super) {
                const Vector3 nz = Vector3Subtract(viewSpace(view, Vector3Transform({ 0, 0, 1 }, M)), viewSpace(view, centre));
                const float facing = nz.z / std::max(1e-6f, Vector3Length(nz));
                if (std::fabs(facing) > 0.15f) {
                    const float zf = (facing > 0 ? 0.5f : -0.5f) * p.size.z;
                    const Vector2 at = project(view, Vector3Transform({ 0, 0, zf }, M));
                    const Vector2 ex = Vector2Subtract(project(view, Vector3Transform({ p.size.x / 2, 0, zf }, M)), at);
                    const Vector2 ey = Vector2Subtract(project(view, Vector3Transform({ 0, p.size.y / 2, zf }, M)), at);
                    const float angle = std::atan2(ey.x, -ey.y) * RAD2DEG;
                    const float w = 2 * Vector2Length(ex), h = 2 * Vector2Length(ey);
                    const std::string text = p.label;
                    const Color bg = p.color;
                    if (w > 2 && h > 2)
                        label = [text, bg, at, angle, w, h, c]() {
                            drawSquashed(c, [&](Rectangle box) { drawLabel(Ctx{}, box, text, bg, false); }, at, angle, w, h, 1);
                        };
                }
            }
            items.push_back({ depth, [outline, col, bands, ink, lw, label, c]() {
                const vg::Path path = vg::Path::polyline(outline, true);
                if (bands.empty()) {
                    vg::Style st = vg::Style::filled(col);
                    if (lw > 0) st.stroke = ink;
                    vg::draw(c, path, st);
                } else {
                    for (const auto& [band, bc] : bands) vg::fill(c, vg::Path::polyline(band, true), bc);
                    if (lw > 0) vg::stroke(c, path, ink);
                }
                if (label) label();
            } });
            // The face is its own item, just over a cap of hair: in 3D the brow pokes through
            // the cap's front, which one flat outline of the cap cannot show. Fringes and hats
            // still sort over it.
            if (face) items.push_back({ depth + 0.06f, face });
        }
    }

    if (view.shadow.a > 0) {
        const Vector2 at = project(view, { pose.offset.x, 0, pose.offset.z });
        const float rx = now.build.torso.x * 0.85f * view.scale;
        const float lift = std::clamp(1 - pose.offset.y * 2, 0.4f, 1.f);
        vg::fill(c, vg::Path::ellipse(at, rx * lift, rx * (0.16f + 0.5f * std::sin(std::fabs(view.pitch) * DEG2RAD)) * lift), view.shadow);
    }
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.depth < b.depth; });
    for (const Item& it : items) it.draw();
}

// ------------------------------------------------------------------ prefab
namespace {
Look preset(const std::string& name) {
    if (name == "toon") return Look::toon();
    if (name == "block") return Look::block();
    if (name == "avatar") return Look::avatar();
    if (name == "robot") return Look::robot();
    throw std::runtime_error("character: unknown preset \"" + name + "\" (toon, block, avatar, robot)");
}
} // namespace

void registerPrefabs() {
    prefab::add({ "character", Json::parse(R"({"mode": "2d", "preset": "toon", "clip": "idle", "yaw": 0,
                                                "scale": 300, "x": 0, "y": 0, "fps": 30})"),
                  [](TNode& n, const Json& p) {
        Look look = preset(p.str("preset", "toon"));
        auto part = [&](const char* key, std::string& field) { if (p.has(key)) field = p.str(key, field); };
        part("head", look.head); part("hair", look.hair); part("hat", look.hat); part("torso", look.torso);
        part("hips", look.hips); part("extra", look.extra);
        part("holdL", look.holdL); part("holdR", look.holdR); part("ride", look.ride); part("sign", look.sign);
        part("armL", look.armL); part("armR", look.armR); part("handL", look.handL); part("handR", look.handR);
        part("legL", look.legL); part("legR", look.legR); part("footL", look.footL); part("footR", look.footR);
        if (p.has("arms")) look.arms(p.str("arms", ""));
        if (p.has("hands")) look.hands(p.str("hands", ""));
        if (p.has("legs")) look.legs(p.str("legs", ""));
        if (p.has("feet")) look.feet(p.str("feet", ""));
        for (const auto& [key, name] : slots(look))
            if (!name.empty() && !parts::has(name)) throw std::runtime_error("character: unknown part \"" + name + "\" for " + key);
        look.skin = p.color("skin", look.skin);
        look.hairColor = p.color("hair-color", look.hairColor);
        look.shoes = p.color("shoes", look.shoes);
        look.accent = p.color("accent", look.accent);
        look.softness = (float)p.num("softness", look.softness);
        if (p.has("top") || p.has("bottom"))
            look.outfit = Outfit::basic(p.color("top", { 70, 130, 200, 255 }), p.color("bottom", { 50, 60, 90, 255 }), look.skin,
                                        (float)p.num("sleeve", 0.45), (float)p.num("trouser", 1.0));
        auto ch = std::make_shared<Character>(look);
        ch->motion.play(clips::named(p.str("clip", "idle")), 0);
        if (p.has("expression")) ch->face.expression(0, p.str("expression", "neutral"), 0);
        if (p.has("say")) {
            const std::string text = p.str("say", "");
            const double from = p.num("sayFrom", 0.5);
            ch->face.say(from, p.num("sayTo", from + 0.075 * (double)text.size()), text);
        }
        const std::string mode = p.str("mode", "2d");
        if (mode == "2d") {
            View2D v;
            v.yaw = (float)p.num("yaw", 0);
            v.scale = (float)p.num("scale", 300);
            v.origin = { (float)p.num("x", 0), (float)p.num("y", 0) };
            n.fn = [ch, v](const Ctx& c, int t) { draw(c, *ch, (double)t / frameRate(), v); };
        } else if (mode == "3d") {
            auto scene = std::make_shared<g3d::Scene>();
            const float h = look.build.height();
            scene->camera.position = { 0, h * 0.62f, h * 3.0f };
            scene->camera.target = { 0, h * 0.5f, 0 };
            scene->camera.fov = 30;
            scene->ambient = 0.55f;
            scene->environmentGenerator = g3d::studioEnvironment({ 236, 240, 246, 255 }, { 170, 182, 196, 255 }, { 60, 62, 70, 255 }, 3);
            scene->environmentIntensity = 0.7f;
            scene->lights.push_back(g3d::Light::directional({ -2, 5, 4 }, { 255, 246, 232, 255 }, 1.6f, true));
            scene->lights.push_back(g3d::Light::directional({ 3, 3, -3 }, { 170, 200, 255, 255 }, 0.6f));
            g3d::Object3D* root = add(*scene, ch, p.integer("fps", 30));
            root->rotation.y = (float)p.num("yaw", 0);
            n.fn = g3d::draw(scene);
        } else throw std::runtime_error("character: mode must be \"2d\" or \"3d\"");
    } });
}

} // namespace rig
} // namespace rmtn
