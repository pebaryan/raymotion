// atmosphere.cpp -- "atmosphere": g3d's sky and fog. A valley from before dawn to after
// dusk under g3d::Sky::at(hour): stars fade, the sun comes up warm through ground fog that
// pools in the valley and lifts by noon, clouds drift, the lake mirrors the sky, and the
// stars come back. The sun light follows the sky's sun; fog takes the sky's colours.
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <vector>

namespace rmtn {
namespace {

const Color INK = mkColor(0xeef0ff), DIM = mkColor(0xb8bfdc), AQUA = mkColor(0x47dad8), BG = mkColor(0x05070f);

void caption(TNode* scene, const char* kicker, const char* title) {
    scene->add(0, scene->dur)->fn = [kicker, title](const Ctx& cx, int t) {
        const float k = (float)interpolate(t, { 0, 14 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        Ctx c = cx;
        c.a *= k;
        dr::rectGrad4(c, 0, 0, 1300, 380, withAlpha(BG, 0.55f), withAlpha(BG, 0), withAlpha(BG, 0), withAlpha(BG, 0));
        dr::text(fonts::ui(false), c, kicker, { 120, 96 + 12 * (1 - k) }, 28, 4, AQUA);
        dr::text(fonts::ui(true), c, title, { 120, 136 + 12 * (1 - k) }, 64, 0, INK);
    };
}

float smooth01(float x) {
    x = std::clamp(x, 0.f, 1.f);
    return x * x * (3 - 2 * x);
}

unsigned hash(unsigned x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x;
}
float rnd(unsigned a, unsigned b = 0) { return (hash(a * 9781u + b * 6271u + 17u) & 0xffffff) / 16777216.f; }

float vnoise(float x, float y, unsigned seed) {
    const int ix = (int)std::floor(x), iy = (int)std::floor(y);
    const float fx = x - ix, fy = y - iy, u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    auto h = [seed](int a, int b) { return rnd((unsigned)a * 73856093u ^ (unsigned)b * 19349663u, seed); };
    return (h(ix, iy) * (1 - u) + h(ix + 1, iy) * u) * (1 - v) + (h(ix, iy + 1) * (1 - u) + h(ix + 1, iy + 1) * u) * v;
}
float fbm(float x, float y, unsigned seed) {
    float v = 0, a = 0.5f;
    for (int i = 0; i < 5; i++) { v += a * vnoise(x, y, seed + i); x = x * 2.03f + 7.1f; y = y * 2.03f + 3.3f; a *= 0.5f; }
    return v;
}

// ---------------------------------------------------------------- the valley

constexpr float SIZE = 150, WATER = 0.35f;

// Hills all round, a valley winding through the middle and a lake in its bend.
float height(float x, float z) {
    const float river = z - 6 * std::sin(x * 0.09f) - 2;
    const float valley = 1 - std::exp(-river * river / 90);                 // 0 along the valley floor
    const float hills = 2 + 10 * fbm(x * 0.035f + 3, z * 0.035f + 1, 11) + 3 * fbm(x * 0.12f, z * 0.12f, 21);
    const float rim = smooth01((std::hypot(x, z) - 48) / 20) * 9;          // mountains towards the edges
    const float lake = 3.2f * std::exp(-((x + 6) * (x + 6) + (z - 3) * (z - 3)) / 60);
    return -0.6f + valley * (hills + rim) * 0.85f + 0.8f * fbm(x * 0.3f, z * 0.3f, 5) - lake;
}

Vector3 normalAt(float x, float z) {
    const float e = 0.2f;
    return Vector3Normalize({ height(x - e, z) - height(x + e, z), 2 * e, height(x, z - e) - height(x, z + e) });
}

Color groundColor(float x, float z) {
    const float h = height(x, z), slope = 1 - normalAt(x, z).y, n = fbm(x * 0.25f, z * 0.25f, 7);
    Color c = mixC(mkColor(0x6f9a46), mkColor(0x3f6b35), smooth01((h - 1.5f) / 3 + (n - 0.5f)));   // meadow to forest
    c = mixC(mkColor(0xc9b98c), c, smooth01((h - WATER - 0.1f) / 0.5f));                           // a shore
    c = mixC(c, mkColor(0x7d7468), smooth01((slope - 0.18f) / 0.15f));                              // rock on steep ground
    c = mixC(c, mkColor(0xf2f4f6), smooth01((h - 10.5f + 2 * n) / 1.2f));                          // snow on the tops
    return c;
}

// The terrain as one mesh: a grid with vertex colours (the lit shader multiplies them in).
g3d::GeometryPtr terrain(int n) {
    return std::make_shared<g3d::Geometry>([n] {
        Mesh m{};
        const int verts = (n + 1) * (n + 1);
        m.vertexCount = verts;
        m.triangleCount = n * n * 2;
        m.vertices = (float*)MemAlloc(verts * 3 * sizeof(float));
        m.normals = (float*)MemAlloc(verts * 3 * sizeof(float));
        m.texcoords = (float*)MemAlloc(verts * 2 * sizeof(float));
        m.colors = (unsigned char*)MemAlloc(verts * 4);
        m.indices = (unsigned short*)MemAlloc(m.triangleCount * 3 * sizeof(unsigned short));
        for (int j = 0; j <= n; j++)
            for (int i = 0; i <= n; i++) {
                const int k = j * (n + 1) + i;
                const float x = (i / (float)n - 0.5f) * SIZE, z = (j / (float)n - 0.5f) * SIZE;
                const Vector3 nn = normalAt(x, z);
                const Color c = groundColor(x, z);
                m.vertices[k * 3] = x; m.vertices[k * 3 + 1] = height(x, z); m.vertices[k * 3 + 2] = z;
                m.normals[k * 3] = nn.x; m.normals[k * 3 + 1] = nn.y; m.normals[k * 3 + 2] = nn.z;
                m.texcoords[k * 2] = i / (float)n; m.texcoords[k * 2 + 1] = j / (float)n;
                m.colors[k * 4] = c.r; m.colors[k * 4 + 1] = c.g; m.colors[k * 4 + 2] = c.b; m.colors[k * 4 + 3] = 255;
            }
        int t = 0;
        for (int j = 0; j < n; j++)
            for (int i = 0; i < n; i++) {
                const unsigned short a = (unsigned short)(j * (n + 1) + i), b = (unsigned short)(a + 1),
                                     c = (unsigned short)(a + n + 1), d = (unsigned short)(c + 1);
                m.indices[t++] = a; m.indices[t++] = c; m.indices[t++] = b;     // counter-clockwise from above
                m.indices[t++] = b; m.indices[t++] = c; m.indices[t++] = d;
            }
        UploadMesh(&m, false);
        return m;
    });
}

// ---------------------------------------------------------------- the day

constexpr int DAY = 600;
constexpr float FROM_HOUR = 4.6f, TO_HOUR = 21.4f;

float hourAt(int t) {
    const float k = std::clamp(t / (float)DAY, 0.f, 1.f);
    return FROM_HOUR + (TO_HOUR - FROM_HOUR) * (0.5f * k + 0.5f * smooth01(k));   // a touch slower at the ends
}

// Ground fog: thick before sunrise, burnt off by late morning, a little back at dusk.
float fogDensityAt(float hour) {
    const float morning = 1 - smooth01((hour - 6.5f) / 3.5f), evening = smooth01((hour - 17.5f) / 3);
    return 0.005f + 0.5f * morning + 0.05f * evening;
}

// ---------------------------------------------------------------- the score

const std::string SCORE = "out/atmosphere-assets-v1/score.wav";

// 66 bpm in D major: a pad that brightens with the light (filter opens towards noon), a
// slow bass, a few birds (high bells) after dawn, crickets (soft ticks) at night.
void genScore(const std::string& path, int frames) {
    using procgen::Score;
    const double end = frames / 30.0;
    Score s(end + 0.5);
    struct Chord { int root; int pad[3]; };
    const Chord prog[4] = { { 38, { 62, 66, 69 } }, { 43, { 62, 67, 71 } }, { 45, { 61, 64, 69 } }, { 42, { 61, 66, 69 } } };
    const double bar = 4 * 60.0 / 66, stop = end - 2.4;
    int i = 0;
    for (double t = 0; t < stop; t += bar, i++) {
        const Chord& ch = prog[i % 4];
        const float hour = hourAt((int)(t * 30)), light = smooth01((hour - 5) / 4) * (1 - smooth01((hour - 17) / 3));
        for (int p : ch.pad) s.pad(t, bar * 0.98, p, 0.07f, 500 + 1500 * light, 0.5f);
        s.pad(t, bar * 0.98, ch.pad[0] - 12, 0.045f, 450, 0.5f);
        s.bass(t, bar * 0.9, ch.root, 0.13f);
        if (light > 0.3f)
            for (int e = 0; e < 4; e++)
                if (rnd((unsigned)i, (unsigned)e) < 0.6f) s.pluck(t + e * bar / 4, ch.pad[e % 3] + 12, 0.018f * light, e % 2 ? 0.3f : 0.7f, 5);
    }
    for (int k = 0; k < 40; k++) {                          // birds, mostly in the morning
        const double t = end * (0.18 + 0.4 * rnd((unsigned)k, 3));
        const int note = 86 + (int)(rnd((unsigned)k, 4) * 7);
        s.bell(t, note, 0.022f, rnd((unsigned)k, 5));
        s.bell(t + 0.09, note + 2, 0.016f, rnd((unsigned)k, 5));
    }
    for (double t = 0; t < end; t += 0.11) {                // crickets, in the dark at each end
        const float hour = hourAt((int)(t * 30)), dark = 1 - smooth01((hour - 5.2f) / 0.8f) + smooth01((hour - 19.5f) / 1);
        if (dark > 0.05f && rnd((unsigned)(t * 100), 9) < 0.7f) s.hat(t, 0.01f * std::min(1.f, dark), rnd((unsigned)(t * 100), 10));
    }
    s.riser(end * 0.24, end * 0.32, 0.02f);                  // sunrise
    for (int p : prog[0].pad) s.pad(stop, 2.4, p, 0.09f, 900, 0.5f);
    s.bass(stop, 2.4, prog[0].root - 12, 0.16f);
    media::writeWav(path, s.mix(), Score::RATE);
}

Vector3 hamlet{ -6, 1, 3 };   // the middle of the thickest cluster of houses, set by valley()

// The valley, its lights, sky and fog, at hour(t).
std::shared_ptr<g3d::Scene> valley(std::function<float(int)> hour) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->shadowMapSize = 2048;
    scene->camera.fov = 38;
    scene->camera.farPlane = 400;
    scene->fogMode = g3d::FogMode::Exponential;
    scene->fogHeight = 0;
    scene->fogFalloff = 0.9f;
    scene->fogFromSky = true;
    scene->up = [hour](int t, g3d::Scene& self) {
        const float h = hour(t);
        g3d::Sky sky = g3d::Sky::at(h);
        sky.sunLight = 0;
        sky.resolution = 2048;
        sky.clouds = std::max(sky.clouds, 0.32f);
        sky.cloudDrift = { 0.0005f, 0.00015f };
        self.sky = sky;
        self.fogDensity = fogDensityAt(h);
    };
    // The sun (from the sky's own sun, warm and dim near the horizon) and a cool moon.
    g3d::Light sun = g3d::Light::directional({ 0, 50, 0 }, WHITE, 1.4f, true);
    sun.shadowExtent = 45;
    sun.shadowBias = 0.08f;
    sun.up = [hour](int t, g3d::Light& self) {
        const g3d::Sky sky = g3d::Sky::at(hour(t));
        const Vector3 d = Vector3Normalize(sky.sunDirection);
        self.position = Vector3Scale(d, 80);
        self.target = { 0, 0, 0 };
        const float up = smooth01(d.y * 4);
        self.intensity = 1.5f * up;
        self.color = mixC(mkColor(0xffa060), mkColor(0xfff4e4), smooth01(d.y * 2.5f));
    };
    scene->lights.push_back(sun);
    g3d::Light moon = g3d::Light::directional({ -30, 40, 30 }, mkColor(0x8ea8ff), 0.3f);
    moon.up = [hour](int t, g3d::Light& self) {
        const float h = hour(t);
        self.intensity = 0.3f * (1 - smooth01((h - 5) / 1.5f) + smooth01((h - 19.5f) / 1.2f));
    };
    scene->lights.push_back(moon);

    g3d::Material ground;
    ground.roughness = 0.95f;
    scene->add(terrain(240), ground)->name = "valley";
    g3d::Material water;
    water.color = mkColor(0x24506e);
    water.roughness = 0.06f;
    water.castShadow = false;
    scene->add(g3d::Geometry::plane(SIZE, SIZE), water)->position = { 0, WATER, 0 };

    // Pines on the lower slopes, a hamlet by the lake.
    auto pine = g3d::Geometry::cone(0.55f, 2.2f, 7);
    g3d::Material needles;
    needles.color = mkColor(0x2e5233);
    needles.roughness = 0.9f;
    for (unsigned i = 0, placed = 0; i < 6000 && placed < 420; i++) {
        const float x = (rnd(i, 1) - 0.5f) * 90, z = (rnd(i, 2) - 0.5f) * 90, h = height(x, z);
        if (h < WATER + 0.5f || h > 8 || normalAt(x, z).y < 0.82f || fbm(x * 0.08f, z * 0.08f, 41) < 0.47f) continue;
        g3d::Object3D* p = scene->add(pine, needles);
        const float s = 0.7f + 0.6f * rnd(i, 3);
        p->position = { x, h + 1.1f * s - 0.1f, z };
        p->scale = { s, s, s };
        placed++;
    }
    auto box = g3d::Geometry::box(1, 1, 1);
    auto roof = g3d::Geometry::cone(0.8f, 0.7f, 4);
    g3d::Material wall, tiles, window;
    wall.color = mkColor(0xe8dcc6);
    wall.roughness = 0.9f;
    tiles.color = mkColor(0x9a4a3a);
    tiles.roughness = 0.8f;
    window.color = BLACK;
    window.emissive = mkColor(0xffc77a);
    std::vector<Vector3> homes;
    for (unsigned i = 0, placed = 0; i < 400 && placed < 11; i++) {
        const float a = rnd(i, 7) * 2 * PI, r = 7 + 3 * rnd(i, 8);
        const float x = -6 + r * std::cos(a), z = 3 + r * std::sin(a), h = height(x, z);
        if (h < WATER + 0.3f || h > 2.5f || normalAt(x, z).y < 0.9f) continue;
        g3d::Object3D* house = scene->add();
        house->position = { x, h, z };
        house->rotation = { 0, rnd(i, 9) * 90, 0 };
        house->add(box, wall)->scale = { 1.1f, 0.9f, 0.9f };
        house->children.back()->position = { 0, 0.45f, 0 };
        g3d::Object3D* top = house->add(roof, tiles);
        top->position = { 0, 1.25f, 0 };
        top->rotation = { 0, 45, 0 };
        top->scale = { 1, 1, 0.85f };
        // A lit window that comes on at dusk.
        g3d::Object3D* w = house->add(box, window);
        w->position = { 0, 0.45f, 0.46f };
        w->scale = { 0.25f, 0.22f, 0.02f };
        w->up = [hour](int t, g3d::Object3D& self) {
            const float h = hour(t);
            self.visible = h < 6.2f || h > 18.6f;
        };
        placed++;
        homes.push_back(house->position);
    }
    size_t most = 0;
    for (const Vector3& a : homes) {
        Vector3 sum{ 0, 0, 0 };
        size_t n = 0;
        for (const Vector3& b : homes)
            if (Vector3Distance(a, b) < 5) { sum = Vector3Add(sum, b); n++; }
        if (n > most) { most = n; hamlet = Vector3Scale(sum, 1.f / n); }
    }
    return scene;
}

} // namespace

void registerAtmosphere(Registry& out) {
    Comp comp;
    comp.name = "atmosphere";
    comp.w = 1920; comp.h = 1080; comp.fps = 30;
    comp.background = BG;
    comp.duration = DAY;

    auto scene = valley(hourAt);
    scene->camera.up = [](int t, g3d::Camera& cam) {
        const float a = (200 + 40 * t / (float)DAY) * DEG2RAD;
        const float x = 30 * std::cos(a), z = 30 * std::sin(a);
        cam.position = { x, std::max(7.5f, height(x, z) + 3.5f), z };
        cam.target = { -4, cam.position.y - 2.6f, 2 };
    };

    TNode* root = addRoot(comp.roots, 0, DAY);
    root->add(0, DAY)->fn = g3d::draw(scene);
    caption(root, "G3D::SKY, FOGMODE, FOGFROMSKY", "a day in the valley");
    // The time of day.
    root->add(0, DAY)->fn = [](const Ctx& cx, int t) {
        Ctx c = cx;
        c.a *= (float)interpolate(t, { 10, 24 }, { 0, 1 }, clamped());
        const float h = hourAt(t);
        char clock[16];
        std::snprintf(clock, sizeof clock, "%02d:%02d", (int)h, (int)((h - std::floor(h)) * 60));
        dr::rectGrad4(c, 1300, 760, 620, 320, withAlpha(BG, 0), withAlpha(BG, 0), withAlpha(BG, 0.55f), withAlpha(BG, 0));
        dr::text(fonts::ui(true), c, clock, { 1800, 900 }, 96, 0, INK, 1, 0.5);
        char line[48];
        std::snprintf(line, sizeof line, "Sky::at(%.1f)", h);
        dr::text(code::font(false), c, line, { 1800, 972 }, 24, 0, DIM, 1, 0.5);
    };

    comp.assets = [](const Comp&, const Json&) {
        if (std::filesystem::exists(SCORE)) return;
        std::filesystem::create_directories(std::filesystem::path(SCORE).parent_path());
        genScore(SCORE, DAY);
    };
    media::audio(addRoot(comp.roots, 0, DAY), SCORE);

    // ---- the miniature: tilt-shift, first through the lens, then as a layer filter
    static constexpr int MINI = 300, HALF = 150;
    Comp mc;
    mc.name = "atmosphere-miniature";
    mc.w = 1920; mc.h = 1080; mc.fps = 30;
    mc.background = BG;
    mc.duration = MINI;
    auto town = valley([](int t) { return 10.4f + 0.6f * t / (float)MINI; });
    town->camera.fov = 30;
    town->camera.farPlane = 400;
    town->camera.up = [](int t, g3d::Camera& cam) {
        // From high on the sunny side, looking down on the hamlet, drifting round it.
        const float a = (-50 + 24 * t / (float)MINI) * DEG2RAD;
        cam.target = hamlet;
        cam.position = { hamlet.x + 30 * std::cos(a), hamlet.y + 26, hamlet.z + 30 * std::sin(a) };
        const float d = Vector3Distance(cam.position, cam.target);
        // Through the lens for the first half: a thin plane of focus leaning towards the camera.
        cam.dof = t < HALF ? g3d::Camera::DepthOfField{ d, 2.5f, 16, -38 } : g3d::Camera::DepthOfField{};
    };
    TNode* mroot = addRoot(mc.roots, 0, MINI);
    mroot->add(0, HALF)->fn = g3d::draw(town);
    TNode* filtered = mroot->add(HALF, MINI - HALF);
    filtered->isolate().filters = { Filter::tiltShift(0.55f, 0.07f, 14, 0, 0.3f) };
    filtered->fn = g3d::draw(town);
    caption(mroot->add(0, HALF), "G3D CAMERA DOF.TILT", "a miniature, through the lens");
    caption(mroot->add(HALF, MINI - HALF), "FILTER::TILTSHIFT", "a miniature, as a layer filter");
    mc.assets = comp.assets;
    media::audio(addRoot(mc.roots, 0, MINI), SCORE);
    out.push_back(std::move(comp));
    out.push_back(std::move(mc));
}

} // namespace rmtn
