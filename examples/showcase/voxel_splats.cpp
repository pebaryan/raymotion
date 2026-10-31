// voxel_splats.cpp -- "voxel-splats": a 20 s utility clip for voxel:: and splat::. One island is
// built twice: as a voxel model, greedy-meshed into a few hundred quads and lit, shadowed and
// standing in the new water; and as a cloud of Gaussian splats made from it. A front sweeps
// across and hands the island from one form to the other, with the counts of each read from the
// build. The score is generated from the same build.
//
//   raymotion render voxel-splats
//
// See docs/voxel-splats-brief.md and docs/voxel-splats-qc.md.
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace rmtn {
namespace {

constexpr float W = 1920.f, H = 1080.f;
constexpr int FPS = 30, TOTAL = 600;           // 20 s
const std::string ASSETS = "out/voxel-splats-assets";

const Color INK = mkColor(0x14202b), BONE = mkColor(0xf1ede2), TEAL = mkColor(0x2fc4b2), AMBER = mkColor(0xf0a43c),
            DIM = mkColor(0xa9b4bd), SKY = mkColor(0x8fb8d6);

// ------------------------------------------------------------------ timing (brief section 3), seconds
constexpr double BUILD_FROM = 0.2, BUILD_TO = 1.9;       // the island rises layer by layer
constexpr double OUTLINE_FROM = 3.8, OUTLINE_TO = 5.0;   // the quads draw in
constexpr double SWEEP1_FROM = 5.4, SWEEP1_TO = 6.6;     // voxels -> splats
constexpr double SWEEP2_FROM = 12.5, SWEEP2_TO = 15.0;   // splats -> voxels, as far as the middle
constexpr double END_AT = 17.0;

// ------------------------------------------------------------------ the island (metres)
constexpr int N = 64, NY = 28;
constexpr float CELL = 0.25f, SEABED = -1.6f;
const Vector3 ORIGIN{ 0, SEABED, 0 };                    // the grid is centred on x and z by MeshOptions

Color ca(Color c, int a) { c.a = (unsigned char)std::clamp(a, 0, 255); return c; }   // an alpha of 0..255
float smooth(double u) { u = std::clamp(u, 0.0, 1.0); return (float)(u * u * (3 - 2 * u)); }
float window(double s, double from, double to) {
    const double f = 8.0 / FPS;
    return std::min(smooth((s - from) / f), smooth((to - s) / f));
}
void text(const Ctx& cx, const std::string& s, Vector2 at, float size, Color col, float ax = 0, float ay = 0.5f,
          bool bold = false, float spacing = 0) {
    dr::text(fonts::ui(bold), cx, s.c_str(), at, size, spacing, col, ax, ay);
}
Ctx faded(const Ctx& cx, float a) { Ctx m = cx; m.a *= std::clamp(a, 0.f, 1.f); return m; }
std::string grouped(long long n) {
    std::string s = std::to_string(n), out;
    for (size_t i = 0; i < s.size(); i++) {
        if (i && (s.size() - i) % 3 == 0) out += ',';
        out += s[i];
    }
    return out;
}

float vnoise(float x, float z, unsigned seed) {
    const int xi = (int)std::floor(x), zi = (int)std::floor(z);
    const float fx = x - xi, fz = z - zi;
    auto h = [&](int a, int b) { return (float)rnd01(seed, (unsigned long long)((a + 4096) * 8192 + (b + 4096))); };
    const float sx = fx * fx * (3 - 2 * fx), sz = fz * fz * (3 - 2 * fz);
    return Lerp(Lerp(h(xi, zi), h(xi + 1, zi), sx), Lerp(h(xi, zi + 1), h(xi + 1, zi + 1), sx), sz);
}

voxel::Grid makeIsland() {
    voxel::Grid g(N, NY, N);
    const Color pal[] = { { 0, 0, 0, 0 }, mkColor(0xd6c496), mkColor(0x60a448), mkColor(0x42803c), mkColor(0x807e86), mkColor(0x785234),
                          mkColor(0x368c4c), mkColor(0xbe463c), mkColor(0xe2d6ba), mkColor(0xaa966e), mkColor(0xf0d24a) };
    for (size_t i = 1; i < sizeof(pal) / sizeof(pal[0]); i++) g.palette[i] = pal[i];
    std::vector<int> height((size_t)N * N, 0);
    auto hAt = [&](int x, int z) -> int& { return height[(size_t)z * N + (size_t)x]; };
    for (int z = 0; z < N; z++)
        for (int x = 0; x < N; x++) {
            const float rc = std::hypot(x - 31.5f, z - 31.5f) / 28.f;
            const float dome = 15.f * std::pow(std::max(0.f, 1 - rc * rc), 1.15f);
            const float n = (vnoise(x * 0.11f, z * 0.11f, 5) - 0.5f) * 5.5f + (vnoise(x * 0.3f, z * 0.3f, 6) - 0.5f) * 1.6f;
            hAt(x, z) = dome > 0.4f ? std::clamp((int)std::lround(dome + n * std::min(1.f, dome * 0.4f)), 1, 20) : 0;
        }
    // A flat place for the hut, then the columns.
    const int hx = 38, hz = 27, hw = 7, hd = 6;
    int flat = 0;
    for (int z = hz; z < hz + hd; z++) for (int x = hx; x < hx + hw; x++) flat += hAt(x, z);
    flat = std::max(flat / (hw * hd), 9);
    for (int z = hz - 1; z <= hz + hd; z++) for (int x = hx - 1; x <= hx + hw; x++) hAt(x, z) = flat;
    for (int z = 0; z < N; z++)
        for (int x = 0; x < N; x++) {
            const int h = hAt(x, z);
            for (int y = 0; y < h; y++) {
                uint8_t c = y >= 8 ? 4 : 9;                                        // rock inland, damp sand low down
                if (y == h - 1) c = h <= 8 ? 1 : h <= 11 ? (rnd01(7, (unsigned long long)(x * 64 + z)) < 0.2 ? 3 : 2) : h <= 13 ? 3 : 4;
                else if (y >= h - 3 && h > 8 && h <= 13) c = 9;
                g.set(x, y, z, c);
            }
        }
    // Trees on the slopes.
    int placed = 0;
    for (int k = 0; k < 400 && placed < 7; k++) {
        const int x = 8 + (int)(rnd01(9, (unsigned long long)k * 2) * (N - 16)), z = 8 + (int)(rnd01(9, (unsigned long long)k * 2 + 1) * (N - 16));
        const int h = hAt(x, z);
        if (h < 9 || h > 13 || (x >= hx - 3 && x < hx + hw + 3 && z >= hz - 3 && z < hz + hd + 3)) continue;
        g.box(x, h, z, x, h + 4, z, 5);
        g.sphere(x + 0.5f, h + 6.2f, z + 0.5f, 2.7f, 6);
        placed++;
    }
    // The hut: walls, a door, a pyramid roof.
    g.box(hx, flat, hz, hx + hw - 1, flat + 3, hz + hd - 1, 8);
    g.box(hx + 1, flat, hz + 1, hx + hw - 2, flat + 3, hz + hd - 2, 0);
    g.box(hx + 3, flat, hz, hx + 3, flat + 2, hz, 0);
    for (int l = 0; l < 4; l++) g.box(hx - 1 + l, flat + 4 + l, hz - 1 + l, hx + hw - l, flat + 4 + l, hz + hd - l, 7);
    return g;
}

// ------------------------------------------------------------------ what is built, once
struct World {
    voxel::Grid grid;
    voxel::MeshStats stats;
    voxel::MeshOptions opts;
    std::vector<voxel::Quad> quads;
    std::vector<float> quadX, splatX;                       // sorted world x, for the counts
    std::vector<int> layerVoxels;                           // voxels below each cell layer
    std::shared_ptr<const splat::Cloud> cloud;
    size_t ringed = 0;                                      // the splat the enlarged one is
    Vector3 cellToWorld(Vector3 c) const { return { -N * CELL * 0.5f + c.x * CELL, SEABED + c.y * CELL, -N * CELL * 0.5f + c.z * CELL }; }
};

std::shared_ptr<World> bake() {
    auto w = std::make_shared<World>();
    w->grid = makeIsland();
    w->opts.size = CELL;
    w->opts.origin = ORIGIN;
    w->opts.centre = true;
    voxel::mesh(w->grid, w->opts, &w->stats);
    w->quads = voxel::quads(w->grid, true);
    for (const voxel::Quad& q : w->quads) {
        float x = 0;
        for (const Vector3& c : q.corner) x += w->cellToWorld(c).x;
        w->quadX.push_back(x / 4);
    }
    std::sort(w->quadX.begin(), w->quadX.end());
    w->layerVoxels.assign((size_t)NY + 1, 0);
    for (int y = 0; y < NY; y++) {
        int n = 0;
        for (int z = 0; z < N; z++) for (int x = 0; x < N; x++) n += w->grid.get(x, y, z) != 0;
        w->layerVoxels[(size_t)y + 1] = w->layerVoxels[(size_t)y] + n;
    }
    // The splats: flat discs on the exposed faces, lit by hand (they take no light of their own),
    // with a view-dependent tint (degree-1 spherical harmonics) so the colour follows the camera.
    splat::Cloud c = splat::fromVoxels(w->grid, w->opts, 9, 0.6f, 12);
    const Vector3 sun = Vector3Normalize({ 7, 11, 5 });
    c.sh.assign(c.size() * 9, 0.f);
    float best = -1e9f;
    for (size_t i = 0; i < c.size(); i++) {
        const float qw = c.rotation[i * 4], qx = c.rotation[i * 4 + 1], qy = c.rotation[i * 4 + 2], qz = c.rotation[i * 4 + 3];
        const Vector3 n{ 2 * (qx * qz + qw * qy), 2 * (qy * qz - qw * qx), 1 - 2 * (qx * qx + qy * qy) };
        const float lit = 0.62f + 0.5f * std::max(0.f, Vector3DotProduct(n, sun));
        const float jitter = 0.94f + 0.12f * (float)rnd01(13, (unsigned long long)i);
        const float y = c.position[i * 3 + 1];
        const float depth = std::clamp(-y * 0.4f, 0.f, 0.55f);                  // under the water: bluer, darker
        const float k = lit * jitter * (1 - 0.3f * depth);
        c.colour[i * 3] = std::min(1.f, Lerp(c.colour[i * 3] * k, 0.12f, depth));
        c.colour[i * 3 + 1] = std::min(1.f, Lerp(c.colour[i * 3 + 1] * k, 0.45f, depth));
        c.colour[i * 3 + 2] = std::min(1.f, Lerp(c.colour[i * 3 + 2] * k, 0.55f, depth));
        const float j = (float)rnd01(14, (unsigned long long)i) - 0.5f;
        c.sh[i * 9 + 2] = 0.18f + 0.06f * j;                                     // red follows x
        c.sh[i * 9 + 8] = -0.18f - 0.06f * j;                                    // blue against it
        if (std::fabs(c.position[i * 3]) < 1.2f && std::fabs(c.position[i * 3 + 2]) < 1.2f && n.y > 0.9f && y > best) { best = y; w->ringed = i; }
        w->splatX.push_back(c.position[i * 3]);
    }
    std::sort(w->splatX.begin(), w->splatX.end());
    c.bound();
    w->cloud = std::make_shared<const splat::Cloud>(std::move(c));
    return w;
}
std::shared_ptr<World> world() { static std::shared_ptr<World> w = bake(); return w; }

// ------------------------------------------------------------------ the shots
struct Cam { Vector3 pos, tgt; float fov; };
Cam camAt(double s) {
    // An orbit that pushes in for the splats and opens out for the dissolve.
    const double angle = 0.55 + 0.17 * s + 0.1 * std::max(0.0, s - 11.0);
    double radius = 15, height = 6.2;
    if (s > SWEEP1_FROM) {
        const float k = smooth((s - SWEEP1_FROM) / 3.6);
        radius = Lerp(15.f, 10.6f, k);
        height = Lerp(6.2f, 4.6f, k);
    }
    if (s > 11.5) {
        const float k = smooth((s - 11.5) / 3.5);
        radius = Lerp(10.6f, 14.2f, k);
        height = Lerp(4.6f, 5.8f, k);
    }
    if (s > END_AT - 1.0) {
        const float k = smooth((s - (END_AT - 1.0)) / 2.0);
        radius = Lerp(14.2f, 15.5f, k);
        height = Lerp(5.8f, 6.8f, k);
    }
    const double a = s > END_AT - 1.0 ? Lerp((float)angle, (float)(0.55 + 0.17 * (END_AT - 1.0) + 0.1 * (END_AT - 1.0 - 11.0)), smooth((s - (END_AT - 1.0)) / 2.0)) : angle;
    Cam c;
    c.pos = { (float)(radius * std::cos(a)), (float)height, (float)(radius * std::sin(a)) };
    c.tgt = { 0, 1.9f, 0 };
    c.fov = 42;
    return c;
}
g3d::Camera camera(double s) {
    const Cam c = camAt(s);
    g3d::Camera cam;
    cam.position = c.pos;
    cam.target = c.tgt;
    cam.fov = c.fov;
    cam.farPlane = 500;
    return cam;
}
bool screenOf(double s, Vector3 p, Vector2& out) { return g3d::project(camera(s), p, { 0, 0, W, H }, out); }

// The front, in world x: -100 = the sweep has not begun, +100 = it is over.
float front1(double s) { return s <= SWEEP1_FROM ? -100.f : s >= SWEEP1_TO ? 100.f : Lerp(-9.5f, 9.5f, smooth((s - SWEEP1_FROM) / (SWEEP1_TO - SWEEP1_FROM))); }
float front2(double s) { return s <= SWEEP2_FROM ? -100.f : Lerp(-9.5f, 0.f, smooth((s - SWEEP2_FROM) / (SWEEP2_TO - SWEEP2_FROM))); }
float buildY(double s) { return Lerp(SEABED - 0.1f, SEABED + NY * CELL + 0.2f, smooth((s - BUILD_FROM) / (BUILD_TO - BUILD_FROM))); }

std::shared_ptr<g3d::Scene> makeScene(const std::shared_ptr<World>& w) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->sky = g3d::Sky::at(15.0f);
    scene->sky.sunLight = 0;
    scene->sky.clouds = 0.f;
    scene->sky.sunGlow = 0.15f;
    scene->ambient = 0.55f;
    scene->shadowMapSize = 4096;
    g3d::Light key = g3d::Light::directional({ 7, 11, 5 }, { 255, 240, 214, 255 }, 2.3f, true);
    key.shadowExtent = 11;
    scene->lights.push_back(key);
    scene->camera.up = [](int t, g3d::Camera& c) { c = camera(t / (double)FPS); };

    scene->water = g3d::Water::calm();
    scene->water.level = 0;
    scene->water.fps = FPS;
    scene->water.openDepth = 0;
    scene->water.caustics = 1;

    g3d::Material sand;
    sand.color = mkColor(0xc8b68a);
    sand.roughness = 0.95f;
    scene->add(g3d::Geometry::plane(400, 400), sand)->position = { 0, SEABED, 0 };

    g3d::Material rough;
    rough.roughness = 0.85f;
    g3d::Object3D* vox = scene->add(voxel::mesh(w->grid, w->opts), rough);
    vox->name = "voxels";
    vox->up = [](int t, g3d::Object3D& o) {
        const double s = t / (double)FPS;
        g3d::Material::Clip& c = o.material.clip;
        c = {};
        o.visible = true;
        if (s < BUILD_TO) {                                       // rising out of the sea, a glowing edge at the top
            c.normal = { 0, 1, 0 };
            c.offset = buildY(s);
            c.edgeWidth = 0.35f;
            c.edgeColor = TEAL;
        } else if (s < SWEEP1_FROM) {
        } else if (s < SWEEP1_TO) {                               // voxels stay ahead of the front
            c.normal = { -1, 0, 0 };
            c.offset = -front1(s);
            c.edgeWidth = 0.5f;
            c.edgeColor = AMBER;
        } else if (s < SWEEP2_FROM) {
            o.visible = false;
        } else {                                                  // voxels gather behind the second front
            c.normal = { 1, 0, 0 };
            c.offset = front2(s);
            c.edgeWidth = 0.5f;
            c.edgeColor = AMBER;
        }
    };

    auto splats = std::make_shared<g3d::Splats>();
    splats->cloud = w->cloud;
    g3d::Object3D* spl = scene->add(splats);
    spl->name = "splats";
    spl->up = [splats](int t, g3d::Object3D&) {
        const double s = t / (double)FPS;
        g3d::Splats::Look& l = splats->look;
        l.clipBand = 0.9f;
        l.scatter = 1.1f;
        if (s < SWEEP1_FROM) { l.clipNormal = { 1, 0, 0 }; l.clipOffset = -100; }
        else if (s < SWEEP1_TO) { l.clipNormal = { 1, 0, 0 }; l.clipOffset = front1(s); }
        else if (s < SWEEP2_FROM) { l.clipOffset = 1e9f; }
        else { l.clipNormal = { -1, 0, 0 }; l.clipOffset = -front2(s); }
    };
    return scene;
}

// ------------------------------------------------------------------ the proofs
// What the front has passed, by the numbers of the build.
long long quadsRight(const World& w, float x) { return w.quadX.end() - std::upper_bound(w.quadX.begin(), w.quadX.end(), x); }
long long quadsLeft(const World& w, float x) { return std::upper_bound(w.quadX.begin(), w.quadX.end(), x) - w.quadX.begin(); }
long long splatsLeft(const World& w, float x) { return std::upper_bound(w.splatX.begin(), w.splatX.end(), x) - w.splatX.begin(); }
long long splatsRight(const World& w, float x) { return w.splatX.end() - std::upper_bound(w.splatX.begin(), w.splatX.end(), x); }

void counter(const Ctx& cx, const std::string& big, const std::string& unit, float y, Color col = BONE) {
    text(cx, big, { W - 96, y }, 74, col, 1, 0.5f, true);
    text(cx, unit, { W - 96, y + 52 }, 28, DIM, 1, 0.5f);
}

// The merged top faces drawn in over the island: each outline is one quad.
void outlines(const Ctx& cx, const World& w, double s, float k, float reveal) {
    const g3d::Camera cam = camera(s);
    const float x0 = -N * CELL * 0.5f, span = N * CELL;
    for (const voxel::Quad& q : w.quads) {
        if (q.axis != 1 || !q.positive) continue;
        Vector3 p[4];
        float cxm = 0;
        for (int i = 0; i < 4; i++) { p[i] = w.cellToWorld(q.corner[(size_t)i]); cxm += p[i].x; }
        cxm /= 4;
        if (p[0].y < -0.2f || (cxm - x0) / span > reveal) continue;
        Vector2 a[4];
        bool ok = true;
        for (int i = 0; i < 4; i++) ok = ok && g3d::project(cam, p[i], { 0, 0, W, H }, a[i]);
        if (!ok) continue;
        for (int i = 0; i < 4; i++) dr::strokeSeg(faded(cx, k), a[i], a[(i + 1) % 4], 2.2f, ca(TEAL, 235));
    }
}

// One splat, enlarged: the Gaussian it is, with its 1, 2 and 3 sigma rings, joined to where it sits.
void enlarged(const Ctx& cx, const World& w, double s, float k) {
    const Ctx m = faded(cx, k);
    const size_t i = w.ringed;
    const Vector3 at{ w.cloud->position[i * 3], w.cloud->position[i * 3 + 1], w.cloud->position[i * 3 + 2] };
    Vector2 q;
    const Vector2 c{ W - 330, 520 };
    const float R = 168;
    dr::rect(m, c.x - R - 44, c.y - R - 60, 2 * R + 88, 2 * R + 190, ca(INK, 205));
    const Color col{ (unsigned char)(w.cloud->colour[i * 3] * 255), (unsigned char)(w.cloud->colour[i * 3 + 1] * 255),
                     (unsigned char)(w.cloud->colour[i * 3 + 2] * 255), 255 };
    // exp(-d^2 / 2) as stacked discs: each layer's alpha is what lifts the cover over its ring to the Gaussian.
    float outer = 0;
    for (int j = 28; j >= 1; j--) {
        const float d = 3.f * (j - 0.5f) / 28.f, want = 0.92f * std::exp(-0.5f * d * d);
        const float a = std::clamp(1 - (1 - want) / (1 - outer), 0.f, 1.f);
        dr::circle(m, c, R * j / 28.f, ca(col, (int)std::lround(255 * a)));
        outer = want;
    }
    for (int sg = 1; sg <= 3; sg++) {
        dr::ring(m, c, R * sg / 3.f, 2.5f, ca(AMBER, 230));
        text(m, std::to_string(sg) + " sigma", { c.x + R * sg / 3.f * 0.7071f + 6, c.y - R * sg / 3.f * 0.7071f - 8 }, 22, AMBER, 0, 0.5f, true);
    }
    text(m, "one splat", { c.x, c.y + R + 40 }, 34, BONE, 0.5f, 0.5f, true);
    dr::text(code::font(false), m, "position · scale · rotation · opacity · colour", { c.x, c.y + R + 84 }, 15, 0, DIM, 0.5f, 0.5f);
    if (screenOf(s, at, q)) {
        dr::ring(m, q, 30, 4, AMBER);
        dr::strokeSeg(m, { q.x + 26, q.y - 8 }, { c.x - R - 44, c.y - 40 }, 3, ca(AMBER, 200));
    }
}

struct Label { double from, to; const char* name; const char* rules; };
const Label LABELS[] = {
    { 0.5, 5.2, "voxels", "voxel::Grid  \xC2\xB7  voxel::mesh" },
    { 6.8, 12.2, "splats", "splat::Cloud  \xC2\xB7  splat::fromVoxels" },
    { 12.4, 16.8, "same shape, two ways", "Material::clip  \xC2\xB7  Splats::look" },
};

void overlays(const Ctx& cx, const World& w, int t) {
    const double s = t / (double)FPS;
    if (const float k = window(s, OUTLINE_FROM, OUTLINE_TO + 0.5); k > 0.001f) outlines(cx, w, s, k, smooth((s - OUTLINE_FROM) / 1.0));
    if (const float k = window(s, 9.6, 12.2); k > 0.001f) enlarged(cx, w, s, k);

    const float kw = window(s, 0.0, END_AT - 0.2);
    const Color dark = ca(INK, (int)(215 * kw)), clear = ca(INK, 0);
    dr::rectGrad4(cx, 0, 0, 900, 340, dark, clear, clear, dark);
    dr::rectGrad4(cx, W - 640, 0, 640, 360, clear, dark, dark, clear);
    const float kk = window(s, 0.3, END_AT - 0.2);
    text(faded(cx, kk), "VOXELS & SPLATS", { 96, 92 }, 26, TEAL, 0, 0.5f, true, 6);
    text(faded(cx, kk), "one island, two ways", { 96, 128 }, 26, BONE, 0, 0.5f);
    for (const Label& l : LABELS) {
        const float a = window(s, l.from, l.to);
        if (a <= 0.001f) continue;
        Ctx m = faded(cx, a);
        m.o.y += (1 - a) * 14 * m.s;
        text(m, l.name, { 96, 190 }, 84, BONE, 0, 0.5f, true);
        dr::text(code::font(false), m, l.rules, { 100, 262 }, 30, 0, TEAL, 0, 0.5f);
    }
    // The counters: read from the grid, the mesh and the cloud.
    if (const float k = window(s, 1.7, 3.7); k > 0.001f) {
        const int layer = std::clamp((int)std::floor((buildY(s) - SEABED) / CELL), 0, NY);
        counter(faded(cx, k), grouped(w.layerVoxels[(size_t)layer]), "voxels", 100);
    }
    if (const float k = window(s, 3.8, 5.3); k > 0.001f) {
        counter(faded(cx, k), grouped(w.stats.quads), "quads", 100, TEAL);
        text(faded(cx, k), "from " + grouped(w.stats.faces) + " faces", { W - 96, 196 }, 26, DIM, 1, 0.5f);
    }
    if (const float k = window(s, 7.6, 9.4); k > 0.001f) counter(faded(cx, k), grouped((long long)w.cloud->size()), "splats", 100);
    if (const float k = window(s, 13.2, END_AT - 0.2); k > 0.001f) {
        const float f = s < SWEEP2_FROM ? -100.f : front2(s);
        counter(faded(cx, k), grouped(quadsLeft(w, f)), "voxel quads", 96, TEAL);
        counter(faded(cx, k), grouped(splatsRight(w, f)), "splats", 210, AMBER);
    }
}

void endCard(const Ctx& cx, int t) {
    const float kt = smooth(t / 14.0), ka = smooth((t - 18) / 14.0);
    Ctx tm = faded(cx, kt);
    tm.o.y += (1 - kt) * 20 * tm.s;
    const Color clear = ca(INK, 0);
    dr::rectGrad4(faded(cx, kt), 0, 700, W, 140, clear, clear, INK, INK);
    dr::rect(faded(cx, kt), 0, 840, W, 240, INK);
    text(tm, "ONE ISLAND. TWO WAYS.", { W / 2, 868 }, 72, BONE, 0.5f, 0.5f, true, 2);
    dr::text(code::font(false), faded(cx, ka), "voxel::mesh  \xC2\xB7  splat::fromVoxels", { W / 2, 946 }, 30, 0, TEAL, 0.5f, 0.5f);
}

// ------------------------------------------------------------------ sound (brief section 7)
using procgen::Score;
constexpr int RATE = Score::RATE;

void genScore(const std::string& path, const World& w) {
    (void)w;
    Score s(TOTAL / (double)FPS + 1);
    // A bright bed on the island, an airy one for the splats, a lift into the dissolve, a resolve on the card.
    for (int n : { 55, 59, 62, 66 }) s.pad(0, SWEEP1_FROM + 0.4, n, 0.05f, 900, 0.4f);
    s.bass(0, SWEEP1_FROM, 31, 0.1f);
    for (int n : { 62, 66, 69, 74 }) s.pad(SWEEP1_FROM, SWEEP2_FROM - 1.0 - SWEEP1_FROM, n, 0.045f, 1400, 0.5f);
    s.riser(SWEEP2_FROM - 1.0, SWEEP2_FROM, 0.1f);
    s.impact(SWEEP2_FROM, 0.16f);
    for (int n : { 59, 62, 66, 71 }) s.pad(SWEEP2_FROM, END_AT - SWEEP2_FROM, n, 0.055f, 1200, 0.35f);
    s.bass(SWEEP2_FROM, END_AT - SWEEP2_FROM - 0.2, 31, 0.1f);
    s.impact(END_AT + 0.1, 0.24f);
    for (int j = 0; j < 4; j++) s.bell(END_AT + 0.15 + 0.12 * j, 74 + j * 3, 0.06f, 0.3f + 0.12f * j);
    for (int n : { 55, 62, 67, 71 }) s.pad(END_AT, TOTAL / (double)FPS - END_AT, n, 0.07f, 1300, 0.5f);

    // A tick for every layer the island rises through, quickening with the rise.
    int last = -1;
    for (double t = BUILD_FROM; t < BUILD_TO; t += 1.0 / 240) {
        const int layer = (int)std::floor((buildY(t) - SEABED) / CELL);
        if (layer != last && layer >= 0 && layer < NY) { s.pluck(t, 79 + (layer % 5) * 2, 0.04f, 0.12f, 4); last = layer; }
    }
    s.pluck(OUTLINE_FROM, 86, 0.05f, 0.5f, 4);
    // A shimmer across the front of each sweep, and a chime when the splat is shown.
    auto shimmer = [&](double a, double b, float amp) {
        const int i0 = (int)(a * RATE), len = (int)((b - a) * RATE);
        procgen::Svf f, g;
        for (int k = 0; k < len; k++) {
            const double u = (double)k / len;
            const float env = (float)(std::sin(PI * u));
            const float n = s.noise();
            s.put(i0 + k, (f.bp(n, 2400 + 3200 * (float)u, 0.5f, RATE) + 0.5f * g.bp(n, 7200, 0.6f, RATE)) * env * amp, 0.15f + 0.7f * (float)u, 0.3f, 0);
        }
    };
    shimmer(SWEEP1_FROM, SWEEP1_TO, 0.06f);
    shimmer(SWEEP2_FROM, SWEEP2_TO, 0.07f);
    s.bell(9.7, 86, 0.06f, 0.6f);

    std::vector<float> mix = s.mix();
    for (float& x : mix) x = std::tanh(x * 3.0f);
    float peak = 0;
    for (float x : mix) peak = std::max(peak, std::fabs(x));
    const float g = peak > 0 ? 0.43f / peak : 1.f;               // about -16 LUFS integrated
    for (float& x : mix) x *= g;
    media::writeWav(path, mix, RATE);
}

void ensureAssets() {
    std::filesystem::create_directories(ASSETS);
    const std::string score = ASSETS + "/score.wav";
    if (!std::filesystem::exists(score)) genScore(score, *world());
    // The island as a MagicaVoxel file, loaded back: the loader's proof, kept in the assets.
    const std::string vox = ASSETS + "/island.vox";
    voxel::save(vox, world()->grid);
}

void build(Comp& c, const Json&) {
    c.roots.clear();
    const auto w = world();
    const auto scene = makeScene(w);
    addRoot(c.roots, 0, TOTAL)->fn = [scene, w](const Ctx& cx, int t) {
        dr::rect(cx, 0, 0, W, H, SKY);
        g3d::render(*scene, t, cx);
        overlays(cx, *w, t);
    };
    const int e = (int)(END_AT * FPS);
    addRoot(c.roots, e, TOTAL - e)->fn = [](const Ctx& cx, int t) { endCard(cx, t); };
    TNode* logo = prefab::use(c.roots, e + 28, TOTAL - e - 28, "raymotion-logo",
                              Json::parse(R"({"size": 56, "layout": "horizontal", "tagline": "video as code", "delay": 2, "ink": "#f1ede2", "accent": "#2fc4b2"})"));
    logo->props.pos = { W / 2 - 150, 1020 };
    addRoot(c.roots, e + 36, TOTAL - e - 36)->fn = [](const Ctx& cx, int t) {
        dr::text(code::font(false), faded(cx, smooth(t / 12.0)), "github.com/pebaryan/raymotion", { W / 2 - 30, 1020 }, 26, 0, DIM, 0, 0.5f);
    };
    media::audio(addRoot(c.roots, 0, TOTAL), ASSETS + "/score.wav");
}

} // namespace

void registerVoxelSplats(Registry& out) {
    Comp c;
    c.name = "voxel-splats";
    c.w = (int)W; c.h = (int)H; c.fps = FPS;
    c.duration = TOTAL;
    c.background = SKY;
    c.assets = [](const Comp&, const Json&) { ensureAssets(); };
    c.build = [](Comp& c, const Json& p) { build(c, p); };
    out.push_back(std::move(c));
}

} // namespace rmtn
