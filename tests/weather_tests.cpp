// weather_tests.cpp -- g3d::Weather (W1, one weather state): one wind for every system, off means
// off, mix is continuous, overcast dims the scene, and a weather animated by Scene::up is pure.
// Small frames; every check measures something that is drawn (or the CPU function the draw calls),
// and each has a negative control that must fail when the system ignores the weather.
#include "rmtn/raymotion.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <complex>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <vector>

using namespace rmtn;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

// ---------------------------------------------------------------- helpers
static Image render(Comp& c, int frame) {
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, frame, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}
static Comp comp(g3d::ScenePtr scene, int w, int h) {
    Comp c;
    c.w = w; c.h = h; c.duration = 600; c.background = BLACK;
    addRoot(c.roots, 0, 600)->fn = g3d::draw(scene, {});
    return c;
}
static Image look(g3d::ScenePtr scene, int w, int h, int frame) {
    Comp c = comp(std::move(scene), w, h);
    return render(c, frame);
}
static float luma(Color c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }
static std::vector<float> lumaOf(const Image& img) {
    std::vector<float> v((size_t)img.width * img.height);
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++) v[(size_t)y * img.width + x] = luma(GetImageColor(img, x, y));
    return v;
}
static long long diffImages(const Image& a, const Image& b) {
    if (a.width != b.width || a.height != b.height) return -1;
    long long d = 0;
    for (int y = 0; y < a.height; y++)
        for (int x = 0; x < a.width; x++) {
            const Color p = GetImageColor(a, x, y), q = GetImageColor(b, x, y);
            d += std::abs(p.r - q.r) + std::abs(p.g - q.g) + std::abs(p.b - q.b) + std::abs(p.a - q.a);
        }
    return d;
}
static void dump(const char* name, const Image& img) {
    if (const char* dir = std::getenv("RMTN_WEATHER_DUMP")) {
        fs::create_directories(dir);
        ExportImage(img, (fs::path(dir) / name).string().c_str());
    }
}
// Degrees between two headings, 0..180.
static float apart(float a, float b) {
    float d = std::fmod(std::fabs(a - b), 360.f);
    return d > 180 ? 360 - d : d;
}
static float headingOf(float x, float z) { return std::atan2(z, x) * RAD2DEG; }
static Vector2 project(const g3d::Camera& cam, Vector3 p, int w, int h) {
    Camera3D rc{ cam.position, cam.target, cam.upVector, cam.fov, CAMERA_PERSPECTIVE };
    return GetWorldToScreenEx(p, rc, w, h);
}
// A screen displacement (pixels) as a world heading (degrees about +y) on the plane through `at`.
static float screenHeading(const g3d::Camera& cam, Vector3 at, Vector2 d, int w, int h) {
    const Vector2 o = project(cam, at, w, h), ex = Vector2Subtract(project(cam, { at.x + 1, at.y, at.z }, w, h), o),
                  ez = Vector2Subtract(project(cam, { at.x, at.y, at.z + 1 }, w, h), o);
    const float det = ex.x * ez.y - ex.y * ez.x;
    const float wx = (d.x * ez.y - d.y * ez.x) / det, wz = (ex.x * d.y - ex.y * d.x) / det;
    return headingOf(wx, wz);
}

static g3d::Weather windy(float direction, float speed, float gust = 0) {
    g3d::Weather w = g3d::Weather::clear();
    w.wind.direction = direction;
    w.wind.speed = speed;
    w.wind.gust = gust;
    return w;
}

// A screen displacement (pixels, y down) of a point on the ground, as a heading about +y, for a camera
// without perspective: the screen's right and up axes are the camera's, whatever the scale.
static float orthoHeading(const g3d::Camera& cam, Vector2 d) {
    const Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, cam.upVector)), up = Vector3CrossProduct(right, fwd);
    const float det = right.x * up.z - right.z * up.x;
    const float u = d.x, v = -d.y;
    return headingOf((u * up.z - right.z * v) / det, (right.x * v - up.x * u) / det);
}

// ---------------------------------------------------------------- the five systems, one wind
// Grass: the centroid of the bright blade tips, in this wind against the opposite one.
static Vector2 g_ownDirection{ 1, 0.3f };
static g3d::ScenePtr grassScene(const g3d::Weather& w, bool ownWind) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 9, 0.3f };
    s->camera.upVector = { 0, 0, -1 };
    s->camera.target = { 0, 0, 0 };
    s->camera.orthographic = true;
    s->camera.orthoHeight = 2.6f;
    s->ambient = 0.9f;
    g3d::Material ground;
    ground.color = BLACK;
    ground.roughness = 1;
    ground.unlit = true;
    s->add(g3d::Geometry::plane(40, 40), ground);
    auto f = std::make_shared<g3d::Field>();
    f->min = { -1, -1 }; f->max = { 1, 1 };
    f->mask = [](float x, float z) { return x * x + z * z < 0.36f ? 1.f : 0.f; };
    f->density = 800; f->height = 1; f->heightVariation = 0.1f; f->width = 0.04f; f->lean = 0; f->tileSize = 2;
    f->colorVariation = 0; f->dryness = 0; f->castShadows = false;
    f->root = { 0, 0, 0, 255 }; f->tip = { 255, 255, 255, 255 };
    f->windDirection = g_ownDirection;   // its own wind, used when the weather is off or ownWind is set
    f->sway = 0.4f; f->gust = 0;
    f->ownWind = ownWind;
    g3d::Material blade;
    blade.unlit = true;
    s->add(f, blade);
    s->weather = w;
    return s;
}
static Vector2 tipCentroid(const Image& img) {
    double sx = 0, sy = 0, n = 0;
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++) {
            const float l = luma(GetImageColor(img, x, y)) - 100;
            if (l > 0) { sx += l * x; sy += l * y; n += l; }
        }
    return n > 0 ? Vector2{ (float)(sx / n), (float)(sy / n) } : Vector2{ -1, -1 };
}
// Where the bright tips are, on average over 24 moments (the blades flutter).
static Vector2 meanTips(const g3d::Weather& w, bool ownWind, const char* dumpAs) {
    constexpr int GW = 320, GH = 180;
    Comp c = comp(grassScene(w, ownWind), GW, GH);
    Vector2 b{ 0, 0 };
    for (int k = 0; k < 24; k++) {
        Image bent = render(c, 3 + 7 * k);
        b = Vector2Add(b, Vector2Scale(tipCentroid(bent), 1.f / 24));
        if (k == 0 && dumpAs) dump(dumpAs, bent);
        UnloadImage(bent);
    }
    return b;
}
// The lean of the grass in this weather, in degrees about +y (and its size in px through `px`): half
// the way between the tips in this wind and in the opposite one, so what the two share (a slightly
// oblique view, the way thin blades are drawn) cancels.
static float grassHeading(const g3d::Weather& w, bool ownWind, const char* tag, float* px = nullptr) {
    const Vector2 own = g_ownDirection;
    g3d::Weather back = w;
    back.wind.direction += 180;
    const Vector2 a = meanTips(w, ownWind, (std::string("grass-") + tag + ".png").c_str());
    g_ownDirection = { -own.x, -own.y };
    const Vector2 b = meanTips(back, ownWind, nullptr);
    g_ownDirection = own;
    const Vector2 d = Vector2Scale(Vector2Subtract(a, b), 0.5f);
    if (px) *px = Vector2Length(d);
    return orthoHeading(grassScene(w, ownWind)->camera, d);
}

// Trees: where the top of a growing tree leans (its tilt node, as the draw poses it), and the
// instances of a forest (pitch and bank as the instanced draw uses them).
static flora::TreePtr cheapTree() {
    flora::TreeBuild b;
    b.style = flora::TreeStyle::LowPoly;
    b.frames = 1; b.views = 8; b.viewSize = 40; b.elevations = 1;
    return flora::makeTree(flora::tree("oak"), b);
}
static g3d::ScenePtr treeStage(const g3d::Weather& w) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 6, 22 };
    s->camera.target = { 0, 5, 0 };
    g3d::Material g;
    g.color = { 90, 120, 70, 255 };
    s->add(g3d::Geometry::plane(80, 80), g);
    s->lights.push_back(g3d::Light::directional({ 20, 30, 14 }, WHITE, 2));
    s->weather = w;
    return s;
}
static float growingTreeHeading(const g3d::Weather& w, flora::TreePtr tree, float yaw, bool own, float* lean = nullptr) {
    auto s = treeStage(w);
    flora::ForestSpec spec;
    spec.ownWind = own;
    spec.windDirection = { 1, 0 };           // its own: towards +x
    spec.wind = 1.2f; spec.gust = 0;
    g3d::Object3D* group = flora::addGrowingTree(*s, tree, { 0, 0, 0 }, yaw, 1, [](int) { return 1.f; }, spec);
    Image img = look(s, 160, 90, 3);
    UnloadImage(img);
    g3d::Object3D* holder = group->children[0].get();
    g3d::Object3D* tilt = holder->children[0].get();
    // The top of the tree is where its up axis goes: the tilt node, then the holder's yaw.
    const Matrix m = MatrixMultiply(MatrixRotateXYZ(Vector3Scale(tilt->rotation, DEG2RAD)),
                                    MatrixRotateXYZ(Vector3Scale(holder->rotation, DEG2RAD)));
    const Vector3 top = Vector3Transform({ 0, 1, 0 }, m);
    if (lean) *lean = std::hypot(top.x, top.z);
    return headingOf(top.x, top.z);
}
static float forestHeading(const g3d::Weather& w, flora::TreePtr tree, float* lean = nullptr) {
    auto s = treeStage(w);
    flora::ForestSpec spec;
    spec.windDirection = { 1, 0 };
    spec.wind = 1.2f; spec.gust = 0;
    std::vector<flora::Placement> at;
    for (int i = 0; i < 4; i++) at.push_back({ { -3.f + 2 * i, 0, 0 }, 40.f * i, 1, 0, (uint32_t)i });
    g3d::Object3D* group = flora::addForest(*s, at, { tree }, spec);
    Image img = look(s, 160, 90, 3);
    UnloadImage(img);
    double sx = 0, sz = 0;
    int n = 0;
    for (auto& child : group->children) {
        if (!child->instances) continue;
        for (const g3d::Instance& i : child->instances->list) {
            const Matrix tilt = MatrixRotateXYZ(Vector3Scale({ i.pitch, 0, i.bank }, DEG2RAD));
            const Matrix turn = MatrixRotateXYZ(Vector3Scale({ 0, i.yaw, 0 }, DEG2RAD));
            const Vector3 top = Vector3Transform({ 0, 1, 0 }, MatrixMultiply(tilt, turn));
            sx += top.x; sz += top.z; n++;
        }
    }
    if (lean) *lean = n ? (float)std::hypot(sx, sz) / n : 0;
    return n ? headingOf((float)sx, (float)sz) : 1000.f;
}

// Water: waves spread about one heading; the axis of the picture's gradients is the heading.
static g3d::Water testWater() {
    g3d::Water w;
    w.enabled = true;
    w.waves = { { 0, 5, 0.09f, 1, 1 }, { 22, 3, 0.05f, 1, 1 }, { -22, 3, 0.05f, 1, 1 }, { 11, 2, 0.025f, 1, 1 }, { -11, 2, 0.025f, 1, 1 } };
    w.foam = 0; w.sunGlint = 0.3f; w.openDepth = 30; w.fetch = 400;
    return w;
}
static g3d::ScenePtr waterScene(const g3d::Weather& w, bool ownWind) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 26, 0.5f };
    s->camera.target = { 0, 0, 0 };
    s->camera.fov = 25;
    s->sky = g3d::Sky::day();
    s->sky.clouds = 0;
    s->water = testWater();
    s->water.ownWind = ownWind;
    s->weather = w;
    return s;
}
static float waterAxis(const g3d::Weather& w, bool ownWind, const char* tag) {
    constexpr int WW = 320, WH = 180;
    auto s = waterScene(w, ownWind);
    Image img = look(s, WW, WH, 4);
    dump((std::string("water-") + tag + ".png").c_str(), img);
    const std::vector<float> l = lumaOf(img);
    double jxx = 0, jyy = 0, jxy = 0;
    for (int y = 20; y < WH - 20; y++)
        for (int x = 20; x < WW - 20; x++) {
            const float gx = l[(size_t)y * WW + x + 1] - l[(size_t)y * WW + x - 1], gy = l[(size_t)(y + 1) * WW + x] - l[(size_t)(y - 1) * WW + x];
            jxx += gx * gx; jyy += gy * gy; jxy += gx * gy;
        }
    const float theta = 0.5f * std::atan2((float)(2 * jxy), (float)(jxx - jyy));      // screen axis, y down
    UnloadImage(img);
    return screenHeading(s->camera, { 0, 0, 0 }, { std::cos(theta), std::sin(theta) }, WW, WH);
}
// Which way the crests travel (from the CPU surface the shader mirrors): the sign of the axis.
static float waterTravel(const g3d::Water& water, float axis) {
    // h(x, t) = f(k . x - w t): d h / d t = -w f', grad h . axis = (k . axis) f'; so the travel is the
    // axis when the sum of (d h / d t) (grad h . axis) is negative.
    const float ax = std::cos(axis * DEG2RAD), az = std::sin(axis * DEG2RAD);
    double sum = 0;
    for (int j = -20; j <= 20; j++)
        for (int i = -20; i <= 20; i++) {
            const float x = i * 0.7f, z = j * 0.7f;
            const float h0 = water.height(x, z, 1.0), h1 = water.height(x, z, 1.05);
            const Vector2 g = water.slope(x, z, 1.0);
            sum += (h1 - h0) / 0.05 * (g.x * ax + g.y * az);
        }
    return sum < 0 ? axis : axis + 180;
}

// Clouds: the sky shifts between two frames; find the shift (pixels) that best maps one onto the other.
static g3d::ScenePtr skyScene(const g3d::Weather& w, bool ownClouds) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 0, 0 };
    s->camera.target = { 0, 1, 0 };
    s->camera.upVector = { 0, 0, 1 };
    s->camera.fov = 70;
    s->sky = g3d::Sky::day();
    s->sky.clouds = 0.6f;
    s->sky.cloudScale = 3;
    s->sky.resolution = 2048;
    s->sky.sunGlow = 0;
    s->sky.cloudDrift = { 0.0003f, 0 };         // its own drift: clouds towards -x
    s->sky.ownClouds = ownClouds;
    s->weather = w;
    return s;
}
static Vector2 bestShift(const std::vector<float>& a, const std::vector<float>& b, int w, int h, int range) {
    // The shift s that makes b(x) = a(x - s): minimise the mean squared difference over the middle.
    auto cost = [&](int dx, int dy) {
        double c = 0;
        int n = 0;
        for (int y = range; y < h - range; y += 2)
            for (int x = range; x < w - range; x += 2) {
                const float d = b[(size_t)y * w + x] - a[(size_t)(y - dy) * w + (x - dx)];
                c += d * d; n++;
            }
        return c / n;
    };
    double best = 1e30;
    int bx = 0, by = 0;
    for (int dy = -range; dy <= range; dy++)
        for (int dx = -range; dx <= range; dx++) {
            const double c = cost(dx, dy);
            if (c < best) { best = c; bx = dx; by = dy; }
        }
    // Sub-pixel by a parabola through the neighbours along each axis.
    auto refine = [&](double cm, double c0, double cp) { const double den = cm - 2 * c0 + cp; return den > 1e-9 ? 0.5 * (cm - cp) / den : 0.0; };
    const double fx = refine(cost(bx - 1, by), best, cost(bx + 1, by)), fy = refine(cost(bx, by - 1), best, cost(bx, by + 1));
    return { (float)(bx + fx), (float)(by + fy) };
}
static float cloudHeading(const g3d::Weather& w, bool ownClouds, const char* tag, float* px = nullptr) {
    constexpr int SW = 320, SH = 180;
    g3d::Weather cloudy = w;
    cloudy.clouds.cover = 0.6f;
    auto s = skyScene(cloudy, ownClouds);
    Image a = look(s, SW, SH, 0), b = look(skyScene(cloudy, ownClouds), SW, SH, 18);
    dump((std::string("sky-") + tag + ".png").c_str(), b);
    const Vector2 shift = bestShift(lumaOf(a), lumaOf(b), SW, SH, 40);
    UnloadImage(a); UnloadImage(b);
    if (px) *px = Vector2Length(shift);
    // The camera looks up: a world displacement along (x, z) on the plane above it.
    return screenHeading(s->camera, { 0, 1, 0 }, shift, SW, SH);
}

// Rain: the mean sideways velocity of the particles the draw would make.
static float rainHeading(const g3d::Weather& w, bool own, float* slant = nullptr) {
    fx::Emitter3D rain = fx::rain3d(10, 10, 5);
    rain.ownWind = own;
    const fx::Emitter3D e = g3d::inWeather(rain, w, MatrixIdentity());
    double sx = 0, sz = 0, sy = 0;
    int n = 0;
    fx::each(e, 40.0, [&](const fx::Particle3D& p) { sx += p.vel.x; sz += p.vel.z; sy += p.vel.y; n++; });
    if (slant) *slant = n ? (float)(std::atan2(std::hypot(sx, sz), -sy) * RAD2DEG) : 0.f;
    return n ? headingOf((float)sx, (float)sz) : 1000.f;
}

static void oneWind() {
    const auto tree = cheapTree();
    for (float dir : { 30.f, 200.f }) {
        const g3d::Weather w = windy(dir, 30);
        float gpx = 0, cpx = 0, lean = 0, forestLean = 0, slant = 0;
        const float grass = grassHeading(w, false, dir < 100 ? "30" : "200", &gpx);
        const float growing = growingTreeHeading(w, tree, 70, false, &lean);
        const float forest = forestHeading(w, tree, &forestLean);
        const float wave = waterAxis(w, false, dir < 100 ? "30" : "200");
        const float cloud = cloudHeading(w, false, dir < 100 ? "30" : "200", &cpx);
        const float rain = rainHeading(w, false, &slant);
        g3d::Water drawn = g3d::inWeather(testWater(), w);
        const float waveHeading = waterTravel(drawn, wave);
        // Waves are known by axis from the picture (mod 180); the CPU surface says which way they travel.
        const float waveErr = std::min(apart(wave, dir), apart(wave + 180, dir));
        std::printf("[weather] wind towards %3.0f: grass %6.1f (%.1f px), growing tree %6.1f (lean %.2f), forest %6.1f (lean %.2f), "
                    "wave axis %6.1f travel %6.1f, cloud %6.1f (%.1f px), rain %6.1f (slant %.1f deg)\n",
                    dir, grass, gpx, growing, lean, forest, forestLean, wave, waveHeading, cloud, cpx, rain, slant);
        CHECK(gpx > 6);
        CHECK(apart(grass, dir) < 5);
        CHECK(lean > 0.02f && apart(growing, dir) < 5);
        CHECK(forestLean > 0.02f && apart(forest, dir) < 5);
        CHECK(waveErr < 5 && apart(waveHeading, dir) < 5);
        CHECK(cpx > 8 && apart(cloud, dir) < 5);
        CHECK(slant > 10 && apart(rain, dir) < 5);
    }
    // ownWind (ownClouds) keeps each system's own direction, whatever the weather says.
    const g3d::Weather w = windy(200, 14);
    float px = 0;
    const float own = grassHeading(w, true, "own");
    const float ownWave = waterAxis(w, true, "own");
    const float ownCloud = cloudHeading(w, true, "own", &px);
    const float ownRain = rainHeading(w, true);
    const float ownTree = growingTreeHeading(w, tree, 70, true);
    std::printf("[weather] own wind with the weather towards 200: grass %.1f (own 16.7), waves axis %.1f (own 0), clouds %.1f (own 180, drift 0.0003), "
                "rain %.1f (own 0), growing tree %.1f (own 0 + yaw 70)\n", own, ownWave, ownCloud, ownRain, ownTree);
    CHECK(apart(own, 16.7f) < 5);
    CHECK(std::min(apart(ownWave, 0), apart(ownWave, 180)) < 5);
    CHECK(apart(ownCloud, 180) < 8 && px > 8);          // the clouds move against the sign of the drift
    CHECK(ownRain > 900 || apart(ownRain, 0) < 8);      // no drift of its own: rain3d's direction slants it towards +x
    CHECK(apart(ownTree, 0) < 5);
}

// ---------------------------------------------------------------- off means off
static g3d::ScenePtr world(const g3d::Weather* w, bool setButOff, bool allOwn, bool busy = true) {
    static const flora::TreePtr tree = cheapTree();
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 2.4f, 9 };
    s->camera.target = { 0, 1.2f, 0 };
    s->camera.fov = 50;
    s->ambient = 0.35f;
    s->sky = g3d::Sky::day();
    s->sky.clouds = 0.35f;
    s->sky.sunLight = 0;
    s->sky.ownClouds = allOwn;
    s->sky.ownLight = allOwn;
    s->ownFog = allOwn;
    s->fogMode = g3d::FogMode::Exponential;      // the scene's own fog
    s->fogDensity = 0.01f;
    s->fogFromSky = true;
    g3d::Material isle;
    isle.color = { 70, 110, 50, 255 };
    isle.roughness = 1;
    g3d::Object3D* ground = s->add(g3d::Geometry::plane(14, 8), isle);
    ground->position = { 0, 0.02f, 0 };
    s->lights.push_back(g3d::Light::directional({ 3, 5, 4 }, { 255, 240, 220, 255 }, 2.2f, true));
    s->lights[0].shadowExtent = 12;
    if (busy) s->water = g3d::Water::choppy();
    s->water.ownWind = allOwn;
    auto f = std::make_shared<g3d::Field>();
    f->min = { -6, -3 }; f->max = { 6, 3 };
    f->density = 200; f->height = 0.5f; f->tileSize = 2;
    f->ownWind = allOwn;
    if (busy) s->add(f, {});
    flora::ForestSpec spec;
    spec.ownWind = allOwn;
    flora::addGrowingTree(*s, tree, { -3, 0, 0 }, 20, 1, [](int) { return 1.f; }, spec);
    flora::addForest(*s, { { { 3, 0, 0 }, 0, 1, 0, 1 }, { { 4.5f, 0, 1 }, 90, 1, 0, 2 } }, { tree }, spec);
    fx::Emitter3D rain = fx::rain3d(10, 8, 5);
    rain.ownWind = allOwn;
    if (busy) s->add(rain)->position = { 0, 4, 0 };
    if (w) {
        s->weather = *w;
        s->weather.enabled = !setButOff;
        s->ground.ownWeather = allOwn;
    }
    return s;
}
static void offMeansOff() {
    constexpr int OW = 320, OH = 180;
    const g3d::Weather storm = g3d::Weather::storm();
    Image never = look(world(nullptr, false, false), OW, OH, 9);
    Image off = look(world(&storm, true, false), OW, OH, 9);          // set to a storm, but not enabled
    Image own = look(world(&storm, false, true), OW, OH, 9);          // enabled, with every opt-out set
    Image on = look(world(&storm, false, false), OW, OH, 9);          // enabled, nothing opted out
    const long long dOff = diffImages(never, off), dOwn = diffImages(never, own), dOn = diffImages(never, on);
    std::printf("[weather] off means off: summed difference from the weather-never-set frame: disabled %lld, enabled with every opt-out %lld; "
                "(enabled, no opt-out %lld, for scale)\n", dOff, dOwn, dOn);
    CHECK(dOff == 0);
    CHECK(dOwn == 0);
    CHECK(dOn > 20000);
    dump("world-never.png", never);
    dump("world-storm.png", on);
    UnloadImage(never); UnloadImage(off); UnloadImage(own); UnloadImage(on);
}

// ---------------------------------------------------------------- mix is continuous
static std::vector<float> fieldsOf(const g3d::Weather& w) {
    return { w.wind.speed, w.wind.gust, w.wind.gustSpeed, w.wind.gustScale, w.clouds.cover, w.precipitation.intensity, w.wetness,
             w.snowCover, w.haze, w.lightningRate, w.fps };
}
static void mixContinuous() {
    const g3d::Weather a = g3d::Weather::clear(), b = g3d::Weather::storm();
    const std::vector<float> fa = fieldsOf(a), fb = fieldsOf(b);
    const char* names[] = { "speed", "gust", "gustSpeed", "gustScale", "cover", "intensity", "wetness", "snowCover", "haze", "lightningRate", "fps" };
    g3d::Weather prev = g3d::Weather::mix(a, b, 0);
    std::vector<float> worst(fa.size(), 0);
    float worstTurn = 0;
    for (int i = 1; i <= 200; i++) {
        const g3d::Weather w = g3d::Weather::mix(a, b, i / 200.f);
        const std::vector<float> p = fieldsOf(prev), q = fieldsOf(w);
        for (size_t k = 0; k < q.size(); k++) worst[k] = std::max(worst[k], std::fabs(q[k] - p[k]));
        worstTurn = std::max(worstTurn, apart(w.wind.direction, prev.wind.direction));
        prev = w;
    }
    for (size_t k = 0; k < fa.size(); k++) {
        const float range = std::fabs(fb[k] - fa[k]), bound = range / 200 * 1.05f + 1e-6f;
        std::printf("[weather] mix step %-14s largest %.5f, range / 200 = %.5f\n", names[k], worst[k], range / 200);
        CHECK(worst[k] <= bound);
    }
    // The wind direction goes the short way round: 350 to 10 through 0, not through 180.
    g3d::Weather x = g3d::Weather::clear(), y = g3d::Weather::clear();
    x.wind.direction = 350; y.wind.direction = 10;
    float farthest = 0;
    worstTurn = 0;
    float before = 350;
    for (int i = 0; i <= 200; i++) {
        const float d = g3d::Weather::mix(x, y, i / 200.f).wind.direction;
        farthest = std::max(farthest, apart(d, 0));
        worstTurn = std::max(worstTurn, apart(d, before));
        before = d;
    }
    const float mid = g3d::Weather::mix(x, y, 0.5f).wind.direction;
    std::printf("[weather] mix 350 to 10 degrees: at 0.5 -> %.2f, never more than %.2f from 0, largest turn in a step %.3f (a swing through 180 would be 1.7)\n",
                mid, farthest, worstTurn);
    CHECK(apart(mid, 0) < 0.01f && farthest <= 10.01f);
    // 30 to 200 turns by 170 (the short way), not 190.
    g3d::Weather p = g3d::Weather::clear(), q = g3d::Weather::clear();
    p.wind.direction = 30; q.wind.direction = 200;
    CHECK(apart(g3d::Weather::mix(p, q, 0.5f).wind.direction, 115) < 0.01f);
    // A wide turn goes at an even rate (a mix of the components would crawl at the ends and race through
    // the middle): 0 to 170 degrees, a step of 0.85 each, never more than 5% off.
    {
        g3d::Weather r = g3d::Weather::clear(), u = g3d::Weather::clear();
        r.wind.direction = 0; u.wind.direction = 170;
        float widest = 0, before = 0;
        for (int i = 1; i <= 200; i++) {
            const float d = g3d::Weather::mix(r, u, i / 200.f).wind.direction;
            widest = std::max(widest, std::fabs(d - before));
            before = d;
        }
        const float tenth = g3d::Weather::mix(r, u, 0.1f).wind.direction;
        std::printf("[weather] mix 0 to 170 degrees: at 0.1 -> %.2f (17 for an even turn), widest step %.3f (0.85)\n", tenth, widest);
        CHECK(std::fabs(tenth - 17) < 0.01f && widest <= 0.85f * 1.05f);
    }
    CHECK(worstTurn <= 20.f / 200 * 1.05f + 0.001f);
    // The kinds switch at 0.5 and nowhere else.
    CHECK(g3d::Weather::mix(a, b, 0.49f).clouds.type == g3d::CloudType::Cumulus && g3d::Weather::mix(a, b, 0.5f).clouds.type == g3d::CloudType::Cumulonimbus);
    CHECK(g3d::Weather::mix(a, b, 0.49f).precipitation.kind == g3d::Precipitation::None && g3d::Weather::mix(a, b, 0.5f).precipitation.kind == g3d::Precipitation::Rain);
    CHECK(!g3d::Weather::mix(g3d::Weather{}, g3d::Weather{}, 0.5f).enabled && g3d::Weather::mix(g3d::Weather{}, b, 0.5f).enabled);
}
// A scene whose weather is a function of t (clear into storm over 100 frames), so frames render alone.
static g3d::ScenePtr mixingWorld(bool breakIt = false, bool busy = true) {
    auto s = world(nullptr, false, false, busy);
    s->weather = g3d::Weather::clear();
    s->ground.ownWeather = true;                     // the ground would jump where the kind of rain switches: this test is about the sky
    s->up = [breakIt](int t, g3d::Scene& self) {
        const float u = t / 100.f;
        self.weather = g3d::Weather::mix(g3d::Weather::clear(), g3d::Weather::storm(), u);
        if (breakIt) self.weather.clouds.cover = u < 0.5f ? 0.15f : 1.f;       // a pop at 0.5, to prove the check sees one
    };
    return s;
}
struct Step { float mean, worstCell; };
static std::vector<float> brightOf(const Image& img) {      // the mean of the channels: a sky greying at the same luma still moves it
    std::vector<float> v((size_t)img.width * img.height);
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++) { const Color c = GetImageColor(img, x, y); v[(size_t)y * img.width + x] = (c.r + c.g + c.b) / 3.f; }
    return v;
}
static Step stepBetween(const Image& a, const Image& b) {
    const std::vector<float> p = brightOf(a), q = brightOf(b);
    double ma = 0, mb = 0;
    for (size_t i = 0; i < p.size(); i++) { ma += p[i]; mb += q[i]; }
    Step s{ (float)std::fabs(ma - mb) / (float)p.size(), 0 };
    const int cell = 8;
    for (int cy = 0; cy + cell <= a.height; cy += cell)
        for (int cx = 0; cx + cell <= a.width; cx += cell) {
            double ca = 0, cb = 0;
            for (int y = 0; y < cell; y++)
                for (int x = 0; x < cell; x++) { ca += p[(size_t)(cy + y) * a.width + cx + x]; cb += q[(size_t)(cy + y) * a.width + cx + x]; }
            s.worstCell = std::max(s.worstCell, (float)std::fabs(ca - cb) / (cell * cell));
        }
    return s;
}
static Step stepAt(int centre, bool breakIt) {
    constexpr int MW = 320, MH = 180;
    Comp c = comp(mixingWorld(breakIt, false), MW, MH);          // no rain, grass or water: their own motion would drown a pop
    Image a = render(c, centre - 2), b = render(c, centre), d = render(c, centre + 2);
    const Step s1 = stepBetween(a, b), s2 = stepBetween(b, d);
    UnloadImage(a); UnloadImage(b); UnloadImage(d);
    return { std::max(s1.mean, s2.mean), std::max(s1.worstCell, s2.worstCell) };
}
static void mixRenders() {
    // The reference is the step just before the switch (u = 0.44..0.48): the wind there is nearly as
    // fast as at 0.5, so the clouds drift as far per frame, and only a pop can make 0.5 differ.
    const Step at30 = stepAt(30, false), at46 = stepAt(46, false), at50 = stepAt(50, false);
    const Step broken = stepAt(50, true);
    std::printf("[weather] mix pixels: step of 2 frames at u = 0.3: mean %.3f, worst 8x8 cell %.2f; at u = 0.46: mean %.3f, worst cell %.2f; "
                "across u = 0.5: mean %.3f, worst cell %.2f; with a deliberate pop at 0.5: mean %.3f, worst cell %.2f\n",
                at30.mean, at30.worstCell, at46.mean, at46.worstCell, at50.mean, at50.worstCell, broken.mean, broken.worstCell);
    CHECK(at50.mean <= at46.mean * 1.25f + 0.1f);
    CHECK(at50.worstCell <= at46.worstCell * 1.25f + 1.f);
    CHECK(broken.mean > at46.mean * 1.25f + 0.1f && broken.mean > 10 * at50.mean);   // the mean check is able to see a pop
    CHECK(broken.worstCell > at46.worstCell * 1.25f + 1.f);                          // and so is the cell check
}

// ---------------------------------------------------------------- overcast dims
static g3d::ScenePtr lightScene(const g3d::Weather& w, bool looksUp, float skyLighting = 1, bool ownClouds = false, bool ownLight = false) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 2, 6 };
    s->camera.target = looksUp ? Vector3{ 0, 8, 0 } : Vector3{ 0, 0, 0 };
    s->camera.fov = 40;
    s->ambient = 0.35f;
    s->sky = g3d::Sky::day();
    s->sky.sunLight = 0;
    s->sky.clouds = 0;
    s->sky.lighting = skyLighting;
    s->sky.ownClouds = ownClouds;
    s->sky.ownLight = ownLight;
    g3d::Material ground;
    ground.color = { 150, 150, 150, 255 };
    ground.roughness = 1;
    s->add(g3d::Geometry::plane(60, 60), ground);
    s->lights.push_back(g3d::Light::directional({ 3, 8, 4 }, { 255, 244, 225, 255 }, 2.2f));
    s->weather = w;
    return s;
}
static float saturation(Color c) {
    const float mx = (float)std::max({ c.r, c.g, c.b }), mn = (float)std::min({ c.r, c.g, c.b });
    return mx > 0 ? (mx - mn) / mx : 0;
}
static void overcastDims() {
    g3d::Weather overcast = g3d::Weather::overcast();
    overcast.haze = 0;                      // no fog in this comparison
    g3d::Weather clear = g3d::Weather::clear();
    clear.haze = 0;
    auto groundMean = [&](const g3d::Weather& w, const char* tag, float lighting = 1, bool ownClouds = false, bool ownLight = false) {
        Image img = look(lightScene(w, false, lighting, ownClouds, ownLight), 160, 90, 0);
        double sum = 0;
        int n = 0;
        for (int y = 55; y < 85; y++)
            for (int x = 40; x < 120; x++) { sum += luma(GetImageColor(img, x, y)); n++; }
        dump(tag, img);
        UnloadImage(img);
        return (float)(sum / n);
    };
    auto skyTop = [&](const g3d::Weather& w, const char* tag) {
        Image img = look(lightScene(w, true), 160, 90, 0);
        long long r = 0, g = 0, b = 0;
        int n = 0;
        for (int y = 5; y < 25; y++)
            for (int x = 60; x < 100; x++) { const Color c = GetImageColor(img, x, y); r += c.r; g += c.g; b += c.b; n++; }
        dump(tag, img);
        UnloadImage(img);
        return Color{ (unsigned char)(r / n), (unsigned char)(g / n), (unsigned char)(b / n), 255 };
    };
    const float lc = groundMean(clear, "light-clear.png"), lo = groundMean(overcast, "light-overcast.png");
    const Color sc = skyTop(clear, "sky-clear.png"), so = skyTop(overcast, "sky-overcast.png");
    std::printf("[weather] overcast: sunlit ground luma %.1f clear -> %.1f overcast (%.0f%%); sky top colour (%d,%d,%d) sat %.2f -> (%d,%d,%d) sat %.2f\n",
                lc, lo, 100 * lo / lc, sc.r, sc.g, sc.b, saturation(sc), so.r, so.g, so.b, saturation(so));
    CHECK(lo < lc * 0.9f);
    CHECK(saturation(so) < saturation(sc) * 0.6f);
    // Where the dimming comes from: the sun light alone (the sky left as it is), and the sky alone.
    {
        const float sunOnly = groundMean(overcast, "light-sunonly.png", 1, true, false), skyOnly = groundMean(overcast, "light-skyonly.png", 1, false, true);
        const float noWeather = groundMean(g3d::Weather{}, "light-noweather.png");
        const float clearSun = groundMean(clear, "light-clear-sunonly.png", 1, true, false);
        std::printf("[weather] ground luma, sky lighting 1: no weather %.1f, clear %.1f, overcast %.1f; overcast sun dimming only %.1f (clear, same %.1f); overcast sky greying only %.1f\n",
                    noWeather, lc, lo, sunOnly, clearSun, skyOnly);
        CHECK(sunOnly < clearSun * 0.9f);
    }
    // Haze is the fog: the weather's fog is the explicit exponential fog of density 0.065 * haze^2, pixel for pixel.
    auto fogFrame = [&](bool viaWeather, float haze) {
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 1.5f, 0 };
        s->camera.target = { 0, 1.2f, -20 };
        s->camera.fov = 50;
        s->fogColor = { 230, 230, 235, 255 };
        g3d::Material ground;
        ground.color = { 40, 60, 30, 255 };
        ground.roughness = 1;
        s->add(g3d::Geometry::plane(400, 400), ground);
        s->lights.push_back(g3d::Light::directional({ 3, 8, 4 }, WHITE, 1.5f));
        g3d::Weather w = g3d::Weather::clear();
        w.haze = haze;
        if (viaWeather) s->weather = w;
        else {
            s->fogMode = g3d::FogMode::Exponential;
            s->fogDensity = w.fogDensity();
            s->fogFromSky = true;
        }
        return look(s, 160, 90, 0);
    };
    const float hz = 0.4f;
    Image viaWeather = fogFrame(true, hz), explicitFog = fogFrame(false, hz), thin = fogFrame(true, 0.1f);
    const long long dd = diffImages(viaWeather, explicitFog), dThin = diffImages(viaWeather, thin);
    const float density = g3d::Weather::mix(g3d::Weather::clear(), g3d::Weather::clear(), 0).fogDensity();
    g3d::Weather hw = g3d::Weather::clear();
    hw.haze = hz;
    std::printf("[weather] haze %.1f: fog density %.5f (0.065 * haze^2 = %.5f); frame against the same fog set by hand differs by %lld; against haze 0.1 by %lld; clear's own density %.6f\n",
                hz, hw.fogDensity(), 0.065 * hz * hz, dd, dThin, density);
    CHECK(std::fabs(hw.fogDensity() - 0.065f * hz * hz) < 1e-6f);
    CHECK(dd == 0 && dThin > 100000);
    UnloadImage(viaWeather); UnloadImage(explicitFog); UnloadImage(thin);
}

// ---------------------------------------------------------------- purity
static void purity() {
    constexpr int PW = 320, PH = 180;
    Image alone = look(mixingWorld(), PW, PH, 61);
    Comp c = comp(mixingWorld(), PW, PH);
    Image x = render(c, 12), y = render(c, 97), z = render(c, 61), again = render(c, 61);
    const long long d1 = diffImages(alone, z), d2 = diffImages(z, again);
    std::printf("[weather] purity: frame 61 alone against the same frame after frames 12 and 97: summed difference %lld (and %lld after itself)\n", d1, d2);
    CHECK(d1 == 0 && d2 == 0);
    CHECK(diffImages(x, y) > 20000);            // the weather did change between those frames
    // The scene's weather struct is as the scene's `up` left it: nothing was written back.
    auto s = world(nullptr, false, false);
    s->weather = g3d::Weather::storm();
    s->weather.wind.speed = 11.5f;
    s->sky.clouds = 0.35f;
    const float ambient = s->ambient, sun = s->lights[0].intensity, fog = s->fogDensity, lighting = s->sky.lighting;
    Image k = look(s, PW, PH, 4);
    UnloadImage(k);
    CHECK(s->ambient == ambient && s->lights[0].intensity == sun && s->fogDensity == fog && s->sky.clouds == 0.35f && s->weather.wind.speed == 11.5f);
    CHECK(s->sky.lighting == lighting && s->sky.cloudShift.x == 0 && s->sky.cloudDrift.x == g3d::Sky{}.cloudDrift.x);
    CHECK(s->fogMode == g3d::FogMode::Exponential && s->fogFromSky);
    UnloadImage(alone); UnloadImage(x); UnloadImage(y); UnloadImage(z); UnloadImage(again);
}

static void cpuRules() {
    // The mappings, as tabulated in weather.hpp.
    {   // the layer follows the type: a cumulonimbus reaches the tropopause, a cumulus keeps the author's layer
        g3d::Weather cw = windy(40, 8);
        cw.clouds.type = g3d::CloudType::Cumulonimbus;
        CHECK(g3d::inWeather(g3d::Clouds{}, cw).top >= 12000.f);
        cw.clouds.type = g3d::CloudType::Cumulus;
        CHECK(g3d::inWeather(g3d::Clouds{}, cw).top == g3d::Clouds{}.top);
    }
    const g3d::Weather w = windy(40, 8);
    const g3d::Field f = g3d::inWeather(g3d::Field{}, w);
    CHECK(std::fabs(f.sway - 0.12f) < 1e-5f && apart(headingOf(f.windDirection.x, f.windDirection.y), 40) < 0.01f);
    CHECK(f.gustSpeed == w.wind.gustSpeed && f.gustScale == w.wind.gustScale);
    const flora::ForestSpec fs = flora::inWeather(flora::ForestSpec{}, w);
    CHECK(std::fabs(fs.wind - 1.f) < 1e-5f && apart(headingOf(fs.windDirection.x, fs.windDirection.y), 40) < 0.01f);
    // Waves: every direction turns by one angle; height and length are what the wind raises over the fetch.
    g3d::Water calm = g3d::Water::calm();
    const g3d::Water dw = g3d::inWeather(calm, w);
    float sx = 0, sy = 0, wsx = 0, wsy = 0;
    for (size_t i = 0; i < calm.waves.size(); i++) {
        const float a = calm.waves[i].direction * DEG2RAD, m = calm.waves[i].amplitude;
        sx += m * std::cos(a); sy += m * std::sin(a);
        const float b = dw.waves[i].direction * DEG2RAD, n = dw.waves[i].amplitude;
        wsx += n * std::cos(b); wsy += n * std::sin(b);
    }
    CHECK(apart(headingOf(wsx, wsy), 40) < 0.01f);
    CHECK(apart(dw.waves[1].direction - dw.waves[0].direction, calm.waves[1].direction - calm.waves[0].direction) < 1e-3f);
    // Fetch-limited sea: significant height 4 sqrt(sum a^2 / 2) against JONSWAP, capped at the fully developed sea.
    {
        auto hsOf = [](const g3d::Water& x) { float s = 0; for (const auto& v : x.waves) s += v.amplitude * v.amplitude / 2; return 4 * std::sqrt(s); };
        auto steepest = [](const g3d::Water& x) { float m = 0; for (const auto& v : x.waves) m = std::max(m, 2 * std::fabs(v.amplitude) / v.wavelength); return m; };
        auto longest = [](const g3d::Water& x) { float m = 0; for (const auto& v : x.waves) m = std::max(m, v.wavelength); return m; };
        const float g = 9.81f;
        g3d::Weather gale = g3d::Weather::storm();           // 20 m/s
        float worst = 0;
        struct Case { const char* name; float fetch; } cases[] = { { "pond 20 m", 20 }, { "lake 1 km", 1000 }, { "bay 10 km", 10000 }, { "open sea 200 km", 200000 }, { "ocean 2000 km (fully developed cap)", 2000000 } };
        for (const Case& cs : cases) {
            g3d::Water sea = g3d::Water::storm();
            sea.fetch = cs.fetch;
            const g3d::Water got = g3d::inWeather(sea, gale);
            const float U = gale.wind.speed;
            const float want = std::min(0.0016f * U * std::sqrt(cs.fetch / g), 0.21f * U * U / g);
            const float tp = std::min(1.f / (3.5f * (g / U) * std::pow(g * cs.fetch / (U * U), -0.33f)), 7.16f * U / g);
            const float wantL = g * tp * tp / (2 * PI);
            std::printf("[weather] %s, 20 m/s: Hs %.4f m (formula %.4f), longest wavelength %.2f m (formula %.2f), steepest %.3f\n",
                        cs.name, hsOf(got), want, longest(got), wantL, steepest(got));
            CHECK(std::fabs(longest(got) - wantL) < 0.01f * wantL + 1e-4f);
            CHECK(steepest(got) <= g3d::Weather::kMaxSteepness + 1e-4f);
            if (cs.fetch <= 1000 || cs.fetch >= 100000) CHECK(std::fabs(hsOf(got) - want) < 0.1f * want);
            worst = std::max(worst, steepest(got));
        }
        g3d::Water pond = g3d::Water::storm();
        pond.fetch = 20;
        CHECK(hsOf(g3d::inWeather(pond, gale)) < 0.1f);
        g3d::Water sea = g3d::Water::storm();
        sea.fetch = 200000;
        CHECK(hsOf(g3d::inWeather(sea, gale)) > 20 * hsOf(g3d::inWeather(pond, gale)));
        // A wind that cannot reach a steep sea: a steep authored wave is clamped, not folded.
        g3d::Water steep = g3d::Water::storm();
        steep.waves[0].sharpness = 1; steep.fetch = 200000;
        gale.wind.speed = 9;
        CHECK(steepest(g3d::inWeather(steep, gale)) <= g3d::Weather::kMaxSteepness + 1e-4f);
        std::printf("[weather] steepest wave over all cases %.3f (clamp %.2f)\n", worst, g3d::Weather::kMaxSteepness);
    }
    // The sky greys under cover and the sun's disc and the stars go; it drifts downwind (against the offset's sign).
    {
        g3d::Sky day = g3d::Sky::day();
        day.stars = 0.5f;
        g3d::Weather over = g3d::Weather::overcast();
        over.wind.direction = 90;
        const g3d::Sky grey = g3d::inWeather(day, over, 60);
        auto sat = [](Color c) { const float mx = (float)std::max({ c.r, c.g, c.b }), mn = (float)std::min({ c.r, c.g, c.b }); return mx > 0 ? (mx - mn) / mx : 0.f; };
        std::printf("[weather] sky under overcast (cover %.2f): zenith saturation %.2f -> %.2f, sun size %.2f -> %.2f, stars %.2f -> %.2f, cloud shift (%.5f, %.5f) after 2 s of %.0f m/s\n",
                    over.clouds.cover, sat(day.zenith), sat(grey.zenith), day.sunSize, grey.sunSize, day.stars, grey.stars, grey.cloudShift.x, grey.cloudShift.y, over.wind.speed);
        CHECK(sat(grey.zenith) < sat(day.zenith) * 0.5f && grey.sunSize < day.sunSize * 0.1f + 0.01f && grey.stars < day.stars * 0.2f);
        CHECK(grey.clouds == over.clouds.cover && grey.cloudDrift.x == 0);
        CHECK(std::fabs(grey.cloudShift.x) < 1e-6f && grey.cloudShift.y < -0.001f);       // towards +z, so the offset is -z
        CHECK(std::fabs(grey.cloudShift.y + over.wind.speed * 2 * g3d::Weather::kCloudPerMetre) < 1e-5f);
    }
    // A disabled weather changes nothing, in every system.
    g3d::Weather off = w;
    off.enabled = false;
    CHECK(g3d::inWeather(calm, off).waves[0].direction == calm.waves[0].direction);
    CHECK(g3d::inWeather(g3d::Field{}, off).sway == g3d::Field{}.sway);
    CHECK(flora::inWeather(flora::ForestSpec{}, off).wind == flora::ForestSpec{}.wind);
    CHECK(g3d::inWeather(fx::rain3d(), off, MatrixIdentity()).drift.x == 0);
}

// ---- water shading: waves too short for the pixel must not alias into a lattice
static g3d::ScenePtr grazingWater(bool filter, int h, float lengthScale) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 1.2f, 0 };
    s->camera.target = { 0, 0.85f, 10 };
    s->camera.fov = 50;
    s->sky = g3d::Sky::day();
    s->sky.clouds = 0.3f;
    g3d::Water w;
    w.enabled = true;
    w.foam = 0; w.openDepth = 20;
    // Short waves, spread about +z, lengths not simple fractions of each other.
    const float len[] = { 1, 0.73f, 0.58f, 0.41f, 0.33f, 0.27f, 0.21f }, dir[] = { 90, 122, 63, 50, 104, 130, 80 };
    for (int i = 0; i < 7; i++) w.waves.push_back({ dir[i], 0.5f * len[i] * lengthScale, 0.0085f * len[i] * lengthScale, 1, 1 });
    w.filter = filter;
    s->water = w;
    s->lights.clear();
    s->lights.push_back(g3d::Light::directional({ 10, 12, -8 }, WHITE, 1.5f));
    (void)h;
    return s;
}
// Normalised autocorrelation of a 64x64 crop (mean removed): the strongest value at a lag of 4 pixels or more
// against the median of all lags.
static float periodicity(const std::vector<float>& l, int W, int x0, int y0) {
    const int N = 64;
    std::vector<float> c((size_t)N * N);
    double m = 0;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) { c[(size_t)y * N + x] = l[(size_t)(y0 + y) * W + x0 + x]; m += c[(size_t)y * N + x]; }
    m /= N * N;
    double var = 0;
    for (float& v : c) { v -= (float)m; var += (double)v * v; }
    if (var < 1e-6) return 0;
    std::vector<float> peaks;
    float best = 0;
    for (int dy = 0; dy <= 24; dy++)
        for (int dx = -24; dx <= 24; dx++) {
            if (dy == 0 && dx <= 0) continue;
            double s = 0; int n = 0;
            for (int y = 0; y + dy < N; y++) for (int x = std::max(0, -dx); x < std::min(N, N - dx); x++) { s += (double)c[(size_t)y * N + x] * c[(size_t)(y + dy) * N + x + dx]; n++; }
            const float r = (float)(s / n / (var / (N * N)));
            if (dx * dx + dy * dy >= 16) { best = std::max(best, r); peaks.push_back(std::fabs(r)); }
        }
    std::sort(peaks.begin(), peaks.end());
    return best / std::max(peaks[peaks.size() / 2], 0.02f);
}
static Image shrink(const Image& big, int k) {
    Image out = GenImageColor(big.width / k, big.height / k, BLACK);
    for (int y = 0; y < out.height; y++)
        for (int x = 0; x < out.width; x++) {
            int r = 0, g = 0, b = 0;
            for (int j = 0; j < k; j++) for (int i = 0; i < k; i++) { const Color c = GetImageColor(big, x * k + i, y * k + j); r += c.r; g += c.g; b += c.b; }
            ImageDrawPixel(&out, x, y, { (unsigned char)(r / (k * k)), (unsigned char)(g / (k * k)), (unsigned char)(b / (k * k)), 255 });
        }
    return out;
}
static void waterAliasing() {
    constexpr int W = 320, H = 180;
    float with = 0, without = 0;
    for (int pass = 0; pass < 2; pass++) {
        Image img = look(grazingWater(pass == 0, H, 1), W, H, 4);
        dump(pass == 0 ? "water-grazing-filter.png" : "water-grazing-nofilter.png", img);
        const std::vector<float> l = lumaOf(img);
        float worst = 0;
        for (int x0 : { 40, 120, 200 }) worst = std::max(worst, periodicity(l, W, x0, 100));        // the near-middle rows
        (pass == 0 ? with : without) = worst;
        UnloadImage(img);
    }
    std::printf("[weather] grazing short waves, autocorrelation peak / median over 3 crops: filter on %.1f, filter off %.1f\n", with, without);
    CHECK(with < 4.0f && without > 1.5f * with);
    // The average must survive the filter: a distant patch at 4x supersampling (the truth), filtered and not.
    double mean[2] = { 0, 0 };
    for (int pass = 0; pass < 2; pass++) {
        Image big = look(grazingWater(pass == 0, H, 1), W * 4, H * 4, 4);
        Image small = shrink(big, 4);
        const std::vector<float> l = brightOf(small);
        double s = 0; int n = 0;
        for (int y = 92; y < 120; y++) for (int x = 100; x < 220; x++) { s += l[(size_t)y * W + x]; n++; }
        mean[pass] = s / n;
        UnloadImage(big); UnloadImage(small);
    }
    // And the 1x render with the filter on against the 4x truth without it.
    Image one = look(grazingWater(true, H, 1), W, H, 4);
    const std::vector<float> o = brightOf(one);
    double s1 = 0; int n1 = 0;
    for (int y = 92; y < 120; y++) for (int x = 100; x < 220; x++) { s1 += o[(size_t)y * W + x]; n1++; }
    UnloadImage(one);
    std::printf("[weather] distant patch mean (4x supersampled): filter on %.2f, off %.2f; 1x with filter %.2f\n", mean[0], mean[1], s1 / n1);
    CHECK(std::fabs(mean[0] - mean[1]) < 4.0 && std::fabs(s1 / n1 - mean[1]) < 4.0);
}

// ---------------------------------------------------------------- volumetric clouds (W2)
// A sky, a sun (light 0, along the sky's), flat ground at y = 0 and a cloud layer. Large far plane: the
// clouds are kilometres away.
static g3d::ScenePtr cloudScene(Vector3 pos, Vector3 target, float cover, g3d::CloudType type = g3d::CloudType::Cumulus,
                                float hour = 12.5f, bool ground = true, float base = 1500, float top = 3500) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = pos;
    s->camera.target = target;
    s->camera.fov = 60;
    s->camera.nearPlane = 1;
    s->camera.farPlane = 300000;
    s->sky = g3d::Sky::at(hour);
    s->sky.sunLight = 0;
    s->sky.resolution = 2048;
    const Vector3 sun = Vector3Normalize(s->sky.sunDirection);
    s->lights.push_back(g3d::Light::directional(Vector3Scale(sun, 100), { 255, 244, 225, 255 }, 2.2f));
    s->ambient = 0.35f;
    if (ground) {
        g3d::Material m;
        m.color = { 90, 120, 70, 255 };
        m.roughness = 1;
        s->add(g3d::Geometry::plane(600000, 600000), m);
    }
    s->clouds.enabled = true;
    s->clouds.base = base;
    s->clouds.top = top;
    s->clouds.cover = cover;
    s->clouds.type = type;
    s->clouds.quality = g3d::Clouds::Quality::Film;
    s->clouds.shadows = false;
    return s;
}

// Small test frames: Preview quality at the full (small) frame, which is what a quick check can afford.
static g3d::ScenePtr testClouds(Vector3 pos, Vector3 target, float cover, g3d::CloudType type, bool on, float base = 1500,
                                float top = 3500, float fov = 60) {
    auto s = cloudScene(pos, target, cover, type, 12.5f, true, base, top);
    s->camera.fov = fov;
    s->clouds.enabled = on;
    s->clouds.quality = g3d::Clouds::Quality::Preview;
    s->clouds.resolution = 1;
    return s;
}
static int pixelDelta(Color a, Color b) { return std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b); }

// (a) Looking straight up, the share of the sky under cloud is `cover`. One window sees only a few cloud
// cells, so the share is averaged over a grid of 16 places 7 km apart; a pixel is cloud when it differs from
// the same pixel of the clear sky.
static double skyFraction(float cover, g3d::CloudType type) {
    constexpr int W = 320, H = 180;
    long long cloud = 0, total = 0;
    for (int gz = 0; gz < 4; gz++)
        for (int gx = 0; gx < 4; gx++) {
            const Vector3 at{ gx * 7000.f, 2, gz * 7000.f };
            Image on = look(testClouds(at, { at.x, 100, at.z - 1 }, cover, type, true, 1500, 3500, 120), W, H, 0);
            Image off = look(testClouds(at, { at.x, 100, at.z - 1 }, cover, type, false, 1500, 3500, 120), W, H, 0);
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) { total++; if (pixelDelta(GetImageColor(on, x, y), GetImageColor(off, x, y)) >= 45) cloud++; }
            UnloadImage(on); UnloadImage(off);
        }
    return (double)cloud / (double)total;
}
static void coverFromBelow() {
    if (std::getenv("RMTN_CLOUD_CAL"))
        for (g3d::CloudType ty : { g3d::CloudType::Cumulus, g3d::CloudType::Deck, g3d::CloudType::Stratus })
            for (float c : { 0.1f, 0.3f, 0.5f, 0.7f, 0.9f }) std::printf("[weather] cal %d %.1f -> %.3f\n", (int)ty, c, skyFraction(c, ty));
    for (g3d::CloudType ty : { g3d::CloudType::Cumulus, g3d::CloudType::Deck })
        for (float c : { 0.3f, 0.7f }) {
            const double f = skyFraction(c, ty);
            std::printf("[weather] clouds from below: %s cover %.1f -> %.3f of the sky\n", ty == g3d::CloudType::Deck ? "deck" : "cumulus", c, f);
            CHECK(std::fabs(f - c) < 0.08);
        }
}

// (b) From 3 000 m above a layer that ends at 2 200 m, looking straight down: the ground shows through the gaps
// of a 0.6 cumulus field, and not through a full deck. A pixel is ground when it is green.
static double groundFraction(float cover, g3d::CloudType type) {
    constexpr int W = 320, H = 180;
    long long ground = 0, total = 0;
    for (int gz = 0; gz < 3; gz++)
        for (int gx = 0; gx < 3; gx++) {
            const Vector3 at{ gx * 9000.f, 3000, gz * 9000.f };
            auto s = testClouds(at, { at.x, 0, at.z - 0.01f }, cover, type, true, 500, 2200, 90);
            s->camera.upVector = { 0, 0, -1 };
            Image img = look(s, W, H, 0);
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    const Color c = GetImageColor(img, x, y);
                    total++;
                    if (c.g > c.r + 12 && c.g > c.b + 25) ground++;
                }
            UnloadImage(img);
        }
    return (double)ground / (double)total;
}
static void groundThroughGaps() {
    const double part = groundFraction(0.6f, g3d::CloudType::Cumulus), full = groundFraction(1.0f, g3d::CloudType::Deck);
    std::printf("[weather] clouds from above at 3000 m: ground shows through %.3f of a 0.6 cumulus field, %.3f of a full deck\n", part, full);
    CHECK(part > 0.08 && part < 0.6);
    CHECK(full < 0.03);
}

// (c) Climbing through the layer: N frames from 300 m under the base to 300 m over the top (N = 40, then 160).
// The worst step of the mean brightness and of the worst 8x8 cell between frames is taken over the whole climb.
// Brightness changes fast where the sky opens (the optical depth overhead falls from many to none within a few
// tens of metres) but it changes continuously, so four times the frames cut the worst step by half or more; a switch that starts
// or stops the march at an edge makes a jump that keeps its size. The steps between frames well inside the layer
// bound the steps at the base (each is also printed against the others).
struct Climb { float mean, cell, baseMean, baseCell, innerMean, innerCell; };
enum class Cut { None, Base, Top };
static Climb climbThrough(Cut cut, int N) {
    constexpr int W = 160, H = 90;
    const float base = 1500, top = 1800;
    auto yAt = [=](int t) { return base - 300 + (top - base + 600) * (float)t / (N - 1); };
    auto s = testClouds({ 0, base - 300, 0 }, { 0, base - 300, -100 }, 1.0f, g3d::CloudType::Deck, true, base, top, 70);
    s->clouds.density = 0.5f;
    s->up = [=](int t, g3d::Scene& sc) {
        const float y = yAt(t);
        sc.camera.position = { 0, y, 0 };
        sc.camera.target = { 0, y + 47, -100 };         // 25 degrees up
        if (cut == Cut::Base) sc.clouds.enabled = y >= base;          // the march starts only once the camera is in the layer
        if (cut == Cut::Top) sc.clouds.enabled = y < top;
    };
    Comp c = comp(s, W, H);
    Climb r{ 0, 0, 0, 0, 0, 0 };
    Image prev = render(c, 0);
    for (int t = 1; t < N; t++) {
        Image cur = render(c, t);
        const Step st = stepBetween(prev, cur);
        if (cut == Cut::None && N == 40 && t % 3 == 0) { char nm[64]; std::snprintf(nm, sizeof nm, "climb-%02d.png", t); dump(nm, cur); }
        const float y = yAt(t);
        r.mean = std::max(r.mean, st.mean); r.cell = std::max(r.cell, st.worstCell);
        if (y > base - 80 && y < base + 40) { r.baseMean = std::max(r.baseMean, st.mean); r.baseCell = std::max(r.baseCell, st.worstCell); }
        if (y > base + 40 && y < top - 120) { r.innerMean = std::max(r.innerMean, st.mean); r.innerCell = std::max(r.innerCell, st.worstCell); }
        UnloadImage(prev);
        prev = cur;
    }
    UnloadImage(prev);
    return r;
}
static void noPopThroughBase() {
    const Climb a = climbThrough(Cut::None, 40), a2 = climbThrough(Cut::None, 160);
    const Climb b = climbThrough(Cut::Base, 40), b2 = climbThrough(Cut::Base, 160);
    const Climb t = climbThrough(Cut::Top, 40), t2 = climbThrough(Cut::Top, 160);
    auto ratio = [](float x, float y) { return x / std::max(y, 0.01f); };
    std::printf("[weather] climb, worst step (mean / worst 8x8 cell) at 40 and 160 frames, ratio:\n"
                "[weather]   clouds as they are      %6.2f / %6.2f   %6.2f / %6.2f   ratio %.2f / %.2f   (steps at the base %.2f / %.2f, well inside %.2f / %.2f)\n"
                "[weather]   hard switch at the base %6.2f / %6.2f   %6.2f / %6.2f   ratio %.2f / %.2f\n"
                "[weather]   hard switch at the top  %6.2f / %6.2f   %6.2f / %6.2f   ratio %.2f / %.2f\n",
                a.mean, a.cell, a2.mean, a2.cell, ratio(a.mean, a2.mean), ratio(a.cell, a2.cell), a.baseMean, a.baseCell, a.innerMean, a.innerCell,
                b.mean, b.cell, b2.mean, b2.cell, ratio(b.mean, b2.mean), ratio(b.cell, b2.cell),
                t.mean, t.cell, t2.mean, t2.cell, ratio(t.mean, t2.mean), ratio(t.cell, t2.cell));
    CHECK(ratio(a.mean, a2.mean) > 1.5f && ratio(a.cell, a2.cell) > 1.5f);       // a jump in the mean is hidden by the real change, so the cell decides below
    CHECK(ratio(b.cell, b2.cell) < 1.3f);
    CHECK(ratio(t.cell, t2.cell) < 1.3f);
}

// (d) The shadows of the clouds on flat ground move with the wind. Camera 1 500 m over the ground looking
// straight down, under a cumulus layer at 2 000 m; the same crop 90 frames apart is cross-correlated.
static std::vector<float> groundLuma(int frame, int W, int H, float evolution = 0, float windDir = 40, bool shadowsOn = true) {
    auto s = testClouds({ 0, 1500, 0 }, { 0, 0, -0.01f }, 0.5f, g3d::CloudType::Cumulus, true, 2000, 3200, 70);
    s->camera.upVector = { 0, 0, -1 };
    s->clouds.shadows = shadowsOn;
    s->clouds.shadowExtent = 7000;
    s->clouds.evolution = evolution;
    s->clouds.quality = g3d::Clouds::Quality::Film;         // the shadow map's resolution
    s->weather = g3d::Weather::clear();
    s->weather.haze = 0;
    s->weather.wind.direction = windDir;
    s->weather.wind.speed = 20;
    s->weather.clouds.cover = 0.5f;
    Image img = look(s, W, H, frame);
    if (frame == 0) dump(shadowsOn ? "cloud-shadow-0.png" : "cloud-shadow-off.png", img);
    if (frame == 90) dump("cloud-shadow-90.png", img);
    std::vector<float> l = lumaOf(img);
    UnloadImage(img);
    return l;
}
// Shift (dx, dy) in pixels that maps crop A onto crop B (B(x, y) = A(x - dx, y - dy)), by normalised
// cross-correlation over +-R, refined to a fraction of a pixel by a parabola through the peak.
static Vector2 shiftBetween(const std::vector<float>& A, const std::vector<float>& B, int W, int H, int R, float* peak = nullptr) {
    const int x0 = R + 4, x1 = W - R - 4, y0 = R + 4, y1 = H - R - 4;
    auto score = [&](int dx, int dy) {
        double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0; int n = 0;
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++) {
                const double a = A[(size_t)(y - dy) * W + (x - dx)], b = B[(size_t)y * W + x];
                sa += a; sb += b; saa += a * a; sbb += b * b; sab += a * b; n++;
            }
        const double va = saa - sa * sa / n, vb = sbb - sb * sb / n;
        return va > 1e-9 && vb > 1e-9 ? (sab - sa * sb / n) / std::sqrt(va * vb) : 0.0;
    };
    std::vector<double> sc((size_t)(2 * R + 1) * (2 * R + 1));
    int bx = 0, by = 0; double best = -2;
    for (int dy = -R; dy <= R; dy++)
        for (int dx = -R; dx <= R; dx++) {
            const double v = score(dx, dy);
            sc[(size_t)(dy + R) * (2 * R + 1) + dx + R] = v;
            if (v > best) { best = v; bx = dx; by = dy; }
        }
    auto at = [&](int dx, int dy) { return sc[(size_t)(dy + R) * (2 * R + 1) + dx + R]; };
    auto vertex = [](double l, double c, double r) { const double d = l - 2 * c + r; return std::fabs(d) < 1e-12 ? 0.0 : 0.5 * (l - r) / d; };
    float fx = (float)bx, fy = (float)by;
    if (bx > -R && bx < R) fx += (float)vertex(at(bx - 1, by), best, at(bx + 1, by));
    if (by > -R && by < R) fy += (float)vertex(at(bx, by - 1), best, at(bx, by + 1));
    if (peak) *peak = (float)best;
    return { fx, fy };
}
static void cloudShadowsDrift() {
    constexpr int W = 480, H = 270, R = 40;
    const std::vector<float> a = groundLuma(0, W, H), b = groundLuma(90, W, H);
    float peak = 0;
    const Vector2 d = shiftBetween(a, b, W, H, R, &peak);
    const float mPerPx = 2 * 1500 * std::tan(35 * DEG2RAD) / H;        // straight down: the same size everywhere on flat ground
    const float heading = headingOf(d.x, d.y), metres = std::sqrt(d.x * d.x + d.y * d.y) * mPerPx, expect = 20.f * 3.f;
    std::printf("[weather] cloud shadow on the ground, frames 0 and 90: shift (%.2f, %.2f) px = %.1f m at %.1f deg (wind 40 deg, 20 m/s x 3 s = %.0f m); correlation %.3f\n",
                d.x, d.y, metres, heading, expect, peak);
    CHECK(peak > 0.5f);
    CHECK(apart(heading, 40) < 5.f);
    CHECK(std::fabs(metres - expect) < 0.1f * expect);
    // The shadow is what moves: with the shadow map off the ground is the same at both frames.
    const std::vector<float> n0 = groundLuma(0, W, H, 0, 40, false), n1 = groundLuma(90, W, H, 0, 40, false);
    double flat = 0; for (size_t i = 0; i < n0.size(); i++) flat += std::fabs(n0[i] - n1[i]);
    double moved = 0; for (size_t i = 0; i < a.size(); i++) moved += std::fabs(a[i] - b[i]);
    std::printf("[weather] mean luma change between frames 0 and 90: shadows off %.3f, on %.3f\n", flat / n0.size(), moved / a.size());
    CHECK(flat / n0.size() < 0.05 && moved / a.size() > 1.0);
}

// (e) Depth. A tall opaque box in front of the cloud hides it: the box's pixels are those of the same frame
// without clouds. A hill that rises into a low layer is fogged only where it is inside.
static Image occlusionShot(bool on, bool boxFirst) {
    constexpr int W = 320, H = 180;
    auto s = testClouds({ 0, 2, 0 }, { 0, 2 + 47, -100 }, 0.9f, g3d::CloudType::Cumulus, on);
    g3d::Material m;
    m.unlit = true;
    m.color = { 255, 0, 255, 255 };
    // the box stands between the camera and the layer, or (the control) beyond it
    g3d::Object3D* box = s->add(g3d::Geometry::box(30, 900, 6), m);
    box->position = boxFirst ? Vector3{ 4, 400, -25 } : Vector3{ 4, 21000, -45000 };
    box->scale = boxFirst ? Vector3{ 1, 1, 1 } : Vector3{ 1500, 40, 1500 };
    return look(s, W, H, 0);
}
static void occlusion() {
    for (bool boxFirst : { true, false }) {
        Image on = occlusionShot(true, boxFirst), off = occlusionShot(false, boxFirst);
        const int W = on.width, H = on.height;
        std::vector<char> box((size_t)W * H);
        long long boxPx = 0, cloudPx = 0;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                const Color c = GetImageColor(off, x, y);
                box[(size_t)y * W + x] = c.r > 240 && c.b > 240 && c.g < 20;
                if (box[(size_t)y * W + x]) boxPx++;
                else if (pixelDelta(GetImageColor(on, x, y), c) >= 45) cloudPx++;
            }
        long long inside = 0, changed = 0;
        for (int y = 3; y < H - 3; y++)
            for (int x = 3; x < W - 3; x++) {
                bool all = true;
                for (int j = -3; j <= 3 && all; j++) for (int i = -3; i <= 3; i++) if (!box[(size_t)(y + j) * W + x + i]) { all = false; break; }
                if (!all) continue;
                inside++;
                if (pixelDelta(GetImageColor(on, x, y), GetImageColor(off, x, y)) > 0) changed++;
            }
        std::printf("[weather] box %s the layer: %lld box px (%lld away from its edge), %lld of them changed by the clouds; %lld cloud px elsewhere\n",
                    boxFirst ? "in front of" : "behind", boxPx, inside, changed, cloudPx);
        CHECK(inside > 200 && cloudPx > 3000);
        if (boxFirst) CHECK(changed == 0);
        else CHECK(changed > inside / 3);                // control: a box behind the cloud is covered by it
        UnloadImage(on); UnloadImage(off);
    }
    // A hill in a thin layer from 250 m to 1 400 m (a deck: its lower half, up to 820 m), seen side-on from 9 km:
    // the slope under the base is as it was, the slope inside the layer is fogged, and more so higher up.
    constexpr int W = 320, H = 180;
    Image img[2];
    for (int on = 0; on < 2; on++) {
        auto s = testClouds({ 0, 100, 9000 }, { 0, 330, 0 }, 1.0f, g3d::CloudType::Deck, on == 1, 250, 1400, 40);
        s->clouds.density = 0.012f;
        g3d::Material m;
        m.color = { 150, 130, 100, 255 };
        m.roughness = 1;
        g3d::Object3D* hill = s->add(g3d::Geometry::cone(3500, 800, 64), m);
        hill->position = { 0, 400, 0 };
        img[on] = look(s, W, H, 0);
        dump(on ? "cloud-hill-on.png" : "cloud-hill-off.png", img[on]);
    }
    // The slope in the picture: rows whose pixels are hill in the clear frame (brownish). Bands by row from the
    // bottom of the hill (under the base) to the peak.
    int top = H, bottom = -1;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const Color c = GetImageColor(img[0], x, y);
            if (c.r > c.b + 30 && c.r > c.g + 8) { top = std::min(top, y); bottom = std::max(bottom, y); }
        }
    double lowD = 0, midD = 0, highD = 0; int lowN = 0, midN = 0, highN = 0;
    for (int y = top; y <= bottom; y++) {
        const float u = (float)(bottom - y) / std::max(1, bottom - top);        // 0 at the foot of the hill, 1 at the peak
        for (int x = 0; x < W; x++) {
            const Color c = GetImageColor(img[0], x, y);
            if (!(c.r > c.b + 30 && c.r > c.g + 8)) continue;
            const double d = pixelDelta(c, GetImageColor(img[1], x, y));
            if (u < 0.25f) { lowD += d; lowN++; } else if (u < 0.6f) { midD += d; midN++; } else if (u > 0.8f) { highD += d; highN++; }
        }
    }
    lowD /= std::max(1, lowN); midD /= std::max(1, midN); highD /= std::max(1, highN);
    std::printf("[weather] hill into a low layer (rows %d..%d): mean change by the clouds at the foot %.1f, mid slope %.1f, peak %.1f\n", top, bottom, lowD, midD, highD);
    CHECK(lowN > 100 && highN > 20);
    CHECK(lowD < 8 && highD > 40 && highD > midD && midD > lowD);
    UnloadImage(img[0]); UnloadImage(img[1]);
}

// (g) Banding: a cloud edge at Film quality, 64x64 crop, has no periodic ripple (the autocorrelation peak at a lag
// of 4 px or more over the median lag) from the march's step spacing. The crop is the 64x64 window with the most
// contrast (a cloud edge) of the frame with the jitter on; the same window with the jitter off must score higher.
static float bandScore(bool jitter, int* cx, int* cy, bool pick) {
    constexpr int W = 640, H = 360;
    auto s = testClouds({ 0, 2, 0 }, { 0, 2 + 35, -100 }, 0.45f, g3d::CloudType::Cumulus, true, 1500, 3500, 60);
    s->clouds.quality = g3d::Clouds::Quality::Film;
    s->clouds.resolution = 0;
    s->clouds.jitter = jitter;
    s->weather = g3d::Weather{};
    Image img = look(s, W, H, 0);
    dump(jitter ? "cloud-band-jitter.png" : "cloud-band-nojitter.png", img);
    const std::vector<float> l = lumaOf(img);
    if (pick) {
        float best = -1;
        for (int y = 0; y + 64 <= H; y += 16)
            for (int x = 0; x + 64 <= W; x += 16) {
                double m = 0, v = 0;
                for (int j = 0; j < 64; j += 2) for (int i = 0; i < 64; i += 2) m += l[(size_t)(y + j) * W + x + i];
                m /= 1024;
                for (int j = 0; j < 64; j += 2) for (int i = 0; i < 64; i += 2) { const double d = l[(size_t)(y + j) * W + x + i] - m; v += d * d; }
                if (v > best) { best = (float)v; *cx = x; *cy = y; }
            }
    }
    const float sc = periodicity(l, W, *cx, *cy);
    if (!pick && jitter) { Image crop = ImageFromImage(img, { (float)*cx, (float)*cy, 64, 64 }); ImageResizeNN(&crop, 192, 192); dump("cloud-band-crop.png", crop); UnloadImage(crop); }
    if (pick) {
        Image crop = ImageFromImage(img, { (float)*cx, (float)*cy, 64, 64 });
        ImageResizeNN(&crop, 192, 192);                             // the 3x enlargement
        dump("cloud-band-crop.png", crop);
        UnloadImage(crop);
    }
    UnloadImage(img);
    return sc;
}
// The worst autocorrelation peak / median over the 64 x 64 crops of the frame that have contrast (a cloud edge or body).
static float bandWorst(bool jitter, int* crops) {
    constexpr int W = 640, H = 360;
    auto s = testClouds({ 0, 2, 0 }, { 0, 2 + 35, -100 }, 0.45f, g3d::CloudType::Cumulus, true, 1500, 3500, 60);
    s->clouds.quality = g3d::Clouds::Quality::Film;
    s->clouds.resolution = 0;
    s->clouds.jitter = jitter;
    s->weather = g3d::Weather{};
    Image img = look(s, W, H, 0);
    dump(jitter ? "cloud-band-jitter.png" : "cloud-band-nojitter.png", img);
    const std::vector<float> l = lumaOf(img);
    float worst = 0; int n = 0;
    for (int y = 0; y + 64 <= H * 2 / 3; y += 32)
        for (int x = 0; x + 64 <= W; x += 32) {
            double m = 0, v = 0;
            for (int j = 0; j < 64; j += 2) for (int i = 0; i < 64; i += 2) m += l[(size_t)(y + j) * W + x + i];
            m /= 1024;
            for (int j = 0; j < 64; j += 2) for (int i = 0; i < 64; i += 2) { const double d = l[(size_t)(y + j) * W + x + i] - m; v += d * d; }
            if (v / 1024 < 60) continue;                                   // sky only
            n++;
            worst = std::max(worst, periodicity(l, W, x, y));
        }
    if (crops) *crops = n;
    UnloadImage(img);
    return worst;
}
// Error against a reference: the Film frame (96 steps) minus the High frame (160 steps, jittered), both box-blurred 7 x 7
// so the march's own noise averages out, as the RMS over the cloud pixels (luma levels). Step-lattice bands and
// rings are a systematic error, which the blur does not remove.
static float stepError(bool jitter, bool nearest, float* meanAbs = nullptr) {
    constexpr int W = 640, H = 360;
    auto mk = [&](g3d::Clouds::Quality q, bool j, bool nr) {
        auto s = testClouds({ 0, 2, 0 }, { 0, 2 + 35, -100 }, 0.45f, g3d::CloudType::Cumulus, true, 1500, 3500, 60);
        s->clouds.quality = q;
        s->clouds.resolution = 0;
        s->clouds.jitter = j;
        s->clouds.nearestSlice = nr;
        s->weather = g3d::Weather{};
        return look(s, W, H, 0);
    };
    Image a = mk(g3d::Clouds::Quality::Film, jitter, nearest), r = mk(g3d::Clouds::Quality::High, true, false);
    const std::vector<float> la = lumaOf(a), lr = lumaOf(r);
    auto blur = [&](const std::vector<float>& v) {
        std::vector<float> o(v.size(), 0.f);
        for (int y = 3; y < H - 3; y++)
            for (int x = 3; x < W - 3; x++) {
                float s = 0; for (int j = -3; j <= 3; j++) for (int i = -3; i <= 3; i++) s += v[(size_t)(y + j) * W + x + i];
                o[(size_t)y * W + x] = s / 49.f;
            }
        return o;
    };
    const std::vector<float> ba = blur(la), br = blur(lr);
    double s2 = 0; long long n = 0;
    for (int y = 3; y < H * 2 / 3; y++)
        for (int x = 3; x < W - 3; x++) {
            const float d = ba[(size_t)y * W + x] - br[(size_t)y * W + x];
            s2 += (double)d * d; n++;
        }
    UnloadImage(a); UnloadImage(r);
    (void)meanAbs;
    return n ? (float)std::sqrt(s2 / n) : 0.f;
}
static void banding() {
    int n = 0, n2 = 0;
    const float with = bandWorst(true, &n), without = bandWorst(false, &n2);   // reported only: the worst-crop ratio mostly measures how smooth a cloud is
    const float eFilm = stepError(true, false), eOff = stepError(false, false), eNear = stepError(true, true);
    std::printf("[weather] step error vs the High frame (blurred RMS, luma levels): Film %.4f, jitter off %.3f, nearest slice %.3f\n", eFilm, eOff, eNear);
    CHECK(eFilm < 1.f);                       // the shipped march has no systematic step error ...
    CHECK(eOff > 3.f);                        // ... and the lattice without the jitter has a lot: the detector separates them by orders of magnitude
    CHECK(eNear > eFilm + 0.1f);              // the nearest slice of the noise atlas shows too
    std::printf("[weather] cloud edges at Film quality, worst of %d crops: autocorrelation peak / median %.2f with the jitter, %.2f without (%d crops)\n", n, with, without, n2);
    CHECK(n >= 3);
}

// (f) Frames are pure.
static void cloudPurity() {
    constexpr int W = 240, H = 135;
    auto s = testClouds({ 0, 2, 0 }, { 0, 2 + 60, -100 }, 0.5f, g3d::CloudType::Cumulus, true, 1500, 3500, 70);
    s->weather = g3d::Weather::clear();
    s->weather.haze = 0;
    s->clouds.shadows = true;
    s->clouds.quality = g3d::Clouds::Quality::Film;
    Comp c = comp(s, W, H);
    Image a = render(c, 61);
    Image o1 = render(c, 5), o2 = render(c, 140);
    Image b = render(c, 61), c2 = render(c, 61);
    const long long d1 = diffImages(a, b), d2 = diffImages(b, c2), dd = diffImages(a, o1);
    std::printf("[weather] clouds purity: frame 61 alone vs after frames 5 and 140: diff %lld; twice: diff %lld; (frame 5 differs from 61 by %lld)\n", d1, d2, dd);
    CHECK(d1 == 0 && d2 == 0 && dd > 0 && diffImages(a, o2) > 0);
    UnloadImage(a); UnloadImage(b); UnloadImage(c2); UnloadImage(o1); UnloadImage(o2);
}

// (h) Off means off: a scene that never touches `clouds`, and one with every cloud setting changed but
// `enabled` false, draw the same pixels (sky, lights, water and a ground of the lit shader included).
static g3d::ScenePtr offScene(bool touch) {
    auto s = cloudScene({ 0, 3, 0 }, { 0, 20, -100 }, 0.5f, g3d::CloudType::Cumulus, 12.5f, true);
    s->clouds = g3d::Clouds{};
    s->water = g3d::Water::calm();
    s->water.level = -2;
    g3d::Material m;
    m.color = { 200, 120, 80, 255 };
    s->add(g3d::Geometry::sphere(3), m)->position = { 6, 3, -20 };
    if (touch) {
        s->clouds.cover = 0.9f; s->clouds.type = g3d::CloudType::Cumulonimbus; s->clouds.base = 400; s->clouds.top = 900;
        s->clouds.shadows = true; s->clouds.density = 3; s->clouds.quality = g3d::Clouds::Quality::High; s->clouds.flash.intensity = 5;
    }
    return s;
}
static void cloudsOffMeansOff() {
    Image a = look(offScene(false), 320, 180, 3), b = look(offScene(true), 320, 180, 3);
    const long long d = diffImages(a, b);
    std::printf("[weather] clouds off: untouched vs every setting changed but disabled: diff %lld\n", d);
    CHECK(d == 0);
    auto on = offScene(true);
    on->clouds.enabled = true;                                      // control: switched on, the same scene is not the same picture
    Image c = look(on, 320, 180, 3);
    std::printf("[weather] clouds on, same scene: diff %lld\n", diffImages(a, c));
    CHECK(diffImages(a, c) > 0);
    UnloadImage(a); UnloadImage(b); UnloadImage(c);
}

// (i) Curvature: from 10 km up, looking level, the far edge of a full layer lies below the flat-world horizon
// (the middle row). On a flat plane the layer's far edge would be at 8 000 m / 250 km = 1.8 degrees down at
// the march's limit (0 degrees without a limit); on the planet it is 3 degrees down.
static float farEdgeDepression(bool print = true) {
    constexpr int W = 640, H = 360;
    Image on = look(testClouds({ 0, 10000, 0 }, { 0, 10000, -100 }, 1.0f, g3d::CloudType::Deck, true, 1000, 3000, 60), W, H, 0);
    auto sOff = testClouds({ 0, 10000, 0 }, { 0, 10000, -100 }, 1.0f, g3d::CloudType::Deck, false, 1000, 3000, 60);
    Image off = look(sOff, W, H, 0);
    int topRow = -1;
    for (int y = 0; y < H && topRow < 0; y++) {
        int n = 0;
        for (int x = 0; x < W; x++) if (pixelDelta(GetImageColor(on, x, y), GetImageColor(off, x, y)) >= 45) n++;
        if (n > W / 4) topRow = y;
    }
    UnloadImage(on); UnloadImage(off);
    const float tanY = std::tan(30 * DEG2RAD);
    const float down = std::atan(((topRow + 0.5f) / (H / 2.f) - 1.f) * tanY) * RAD2DEG;
    if (print) std::printf("[weather] curvature: from 10 km, the layer's far edge is at row %d of %d = %.2f degrees below level (a flat layer: 1.8 at the 250 km limit, 0 beyond)\n", topRow, H, down);
    return topRow < 0 ? -1.f : down;
}
static void curvature() {
    const float d = farEdgeDepression();
    CHECK(d > 2.5f);
}

// Timing (RMTN_CLOUD_TIMING): ms a frame at 1920x1080 for each quality, with the clouds off and on over the
// same scene, so the readback that makes the timing honest is subtracted. Printed, not checked.
static void cloudTiming() {
    constexpr int W = 1920, H = 1080;
    for (int view = 0; view < 2; view++) {
        for (int q = 0; q < 3; q++) {
            double ms[2] = { 1e9, 0 }, mean[2] = { 0, 0 };
            for (int on = 0; on < 2; on++) {
                auto s = view == 0 ? cloudScene({ 0, 2, 0 }, { 0, 2 + 58, -100 }, 0.5f) : cloudScene({ 0, 3000, 0 }, { 0, 2950, -100 }, 0.6f, g3d::CloudType::Deck, 12.5f, true, 600, 2000);
                s->clouds.enabled = on == 1;
                s->clouds.shadows = true;
                s->clouds.quality = (g3d::Clouds::Quality)q;
                Comp c = comp(s, W, H);
                Canvas canvas;
                canvas.load(W, H, 1);
                const double tw = GetTime();
                presentFrame(c, 0, canvas);
                Image warm = LoadImageFromTexture(canvas.output().texture);
                UnloadImage(warm);
                if (on && view == 0) std::printf("[weather] first frame with clouds (%s), including shader compile and noise bake once per process: %.1f ms\n", q == 0 ? "Preview" : q == 1 ? "Film" : "High", (GetTime() - tw) * 1000);
                for (int i = 0; i < 10; i++) {
                    const double t0 = GetTime();
                    presentFrame(c, i, canvas);
                    Image img = LoadImageFromTexture(canvas.output().texture);
                    const double dt = (GetTime() - t0) * 1000;
                    UnloadImage(img);
                    ms[on] = on ? std::min(ms[on], dt) : std::min(ms[on], dt);
                    mean[on] += dt / 10;
                    if (on && i == 0) ms[1] = dt;
                    if (on && i > 0) ms[1] = std::min(ms[1], dt);
                }
                canvas.unload();
            }
            std::printf("[weather] timing %s view, %s: best %.1f ms, mean %.1f ms for the clouds (frame %.1f / %.1f ms with and %.1f / %.1f ms without them: best / mean)\n",
                        view == 0 ? "under cumulus" : "over a deck", q == 0 ? "Preview" : q == 1 ? "Film" : "High", ms[1] - ms[0], mean[1] - mean[0],
                        ms[1], mean[1], ms[0], mean[0]);
        }
    }
}

// ---------------------------------------------------------------- the look of the clouds (W2 review)
// Film quality, march at the full frame: what a render uses.
static g3d::ScenePtr filmClouds(Vector3 pos, Vector3 target, float cover, g3d::CloudType type, bool on, float base = 1500,
                                float top = 3500, float fov = 60, bool ground = true) {
    auto s = cloudScene(pos, target, cover, type, 12.5f, ground, base, top);
    s->camera.fov = fov;
    s->clouds.enabled = on;
    s->clouds.quality = g3d::Clouds::Quality::Film;
    s->clouds.resolution = 0;
    s->clouds.shadows = false;
    return s;
}
struct Blob { int x0, y0, x1, y1, area; std::vector<int> px; bool border; };
static std::vector<Blob> blobsOf(const std::vector<char>& mask, int W, int H) {
    std::vector<Blob> out;
    std::vector<char> seen(mask.size(), 0);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const int i = y * W + x;
            if (!mask[(size_t)i] || seen[(size_t)i]) continue;
            Blob b{ x, y, x, y, 0, {}, false };
            std::vector<int> stack{ i };
            seen[(size_t)i] = 1;
            while (!stack.empty()) {
                const int j = stack.back(); stack.pop_back();
                const int jx = j % W, jy = j / W;
                b.px.push_back(j); b.area++;
                b.x0 = std::min(b.x0, jx); b.x1 = std::max(b.x1, jx); b.y0 = std::min(b.y0, jy); b.y1 = std::max(b.y1, jy);
                if (jx == 0 || jy == 0 || jx == W - 1 || jy == H - 1) b.border = true;
                const int nx[4] = { jx - 1, jx + 1, jx, jx }, ny[4] = { jy, jy, jy - 1, jy + 1 };
                for (int k = 0; k < 4; k++)
                    if (nx[k] >= 0 && nx[k] < W && ny[k] >= 0 && ny[k] < H) {
                        const int q = ny[k] * W + nx[k];
                        if (mask[(size_t)q] && !seen[(size_t)q]) { seen[(size_t)q] = 1; stack.push_back(q); }
                    }
            }
            out.push_back(std::move(b));
        }
    return out;
}
// Where the clouds are: the pixels that change when they are switched off.
static std::vector<char> cloudMask(const Image& on, const Image& off, int thresh = 45) {
    std::vector<char> m((size_t)on.width * on.height);
    for (int y = 0; y < on.height; y++)
        for (int x = 0; x < on.width; x++) m[(size_t)y * on.width + x] = pixelDelta(GetImageColor(on, x, y), GetImageColor(off, x, y)) >= thresh;
    return m;
}
// Width of a silhouette (the extent of the blob along the row) at `frac` of its height up from the bottom.
static int widthAt(const Blob& b, int W, float frac) {
    const int h = b.y1 - b.y0 + 1;
    const int row = b.y1 - (int)std::lround(frac * (h - 1));
    int lo = W, hi = -1;
    for (int j : b.px) if (j / W == row) { lo = std::min(lo, j % W); hi = std::max(hi, j % W); }
    return hi < lo ? 0 : hi - lo + 1;
}

// A cumulus seen side-on is a heap, not a column: the silhouette is narrower at 80% of its height than at 30%. The
// median over the isolated clouds in the frame (cover 0.3, from below the base).
static float cumulusTaper(bool extruded, int* count = nullptr) {
    constexpr int W = 640, H = 360;
    std::vector<float> r;
    for (int cam = 0; cam < 6; cam++) {                                       // six looks over the layer: enough isolated clouds
        const float x = (cam - 2.5f) * 2500.f;
        auto mk = [&](bool on) {
            auto s = filmClouds({ x, 1000, 0 }, { x, 1000 + 130, -400 }, 0.3f, g3d::CloudType::Cumulus, on, 1500, 3500, 45, false);
            s->clouds.extruded = extruded;
            return look(s, W, H, 0);
        };
        Image on = mk(true), off = mk(false);
        if (cam == 2) dump(extruded ? "cloud-cumulus-extruded.png" : "cloud-cumulus-side.png", on);
        const std::vector<Blob> bl = blobsOf(cloudMask(on, off), W, H);
        for (const Blob& b : bl)
            if (!b.border && b.y1 - b.y0 >= 24 && b.area > 600) {
                const int w30 = widthAt(b, W, 0.3f), w80 = widthAt(b, W, 0.8f);
                if (w30 > 0) r.push_back((float)w80 / (float)w30);
            }
        UnloadImage(on); UnloadImage(off);
    }
    if (count) *count = (int)r.size();
    if (r.empty()) return 99.f;
    std::sort(r.begin(), r.end());
    return r[r.size() / 2];
}
// A cumulonimbus is wider at the top than half way up (the anvil): the width at 90% of a tower's height over the
// width at 50%, for the tallest ISOLATED tower of a view (a blob of the cloud mask 40 px or more tall with no other cloud
// within 6 px of it), the median over views from 20 km at eight places and three seeds. The haze is off: it washes the
// stems out of the mask and is not the shape.
static float cbAnvil(bool extruded, int* heightPx = nullptr, int* views = nullptr) {
    constexpr int W = 640, H = 360;
    std::vector<float> q; int hMax = 0;
    for (unsigned seed = 1; seed <= 3; seed++)
        for (int cam = 0; cam < 8; cam++) {
            const float x = (cam - 3.5f) * 3000.f;
            auto mk = [&](bool on) {
                const float cx = x - 12000.f;                                  // the cell (explicit, one a view) stands left of the centre so its anvil is in the frame
                auto s = filmClouds({ cx, 2, 38000 }, { cx, 5600, 0 }, 0.f, g3d::CloudType::Cumulonimbus, on, 1200, 11000, 75, false);
                s->clouds.storms = { g3d::Clouds::Storm{ x + 977.f * (float)seed, 0.f, 5500.f, 10500.f } };
                s->clouds.extruded = extruded;
                s->clouds.haze = 0;
                s->clouds.seed = seed;
                s->sky.enabled = false;                                       // clouds on black: the mask is the cloud itself, not a difference from a blue sky and its painted clouds
                s->skyColor = BLACK;
                {                                                             // a black wall 32 km out hides the belt of far towers that merges into one blob along the horizon
                    g3d::Material wm;
                    wm.unlit = true;
                    wm.color = BLACK;
                    g3d::Object3D* wall = s->add(g3d::Geometry::box(300000, 90000, 10), wm);
                    wall->position = { cx, 30000, -12000 };
                }
                return look(s, W, H, 0);
            };
            Image on = mk(true), off = mk(false);
            if (cam == 3 && seed == 1) dump(extruded ? "cloud-cb-extruded.png" : "cloud-cb.png", on);
            const std::vector<char> mask = cloudMask(on, off, 8);
            const std::vector<Blob> bl = blobsOf(mask, W, H);
            const Blob* tall = nullptr;
            for (const Blob& b : bl) {
                if (b.border || b.y1 - b.y0 < 40 || b.area <= 1000 || (b.x1 - b.x0) > 4.f * (b.y1 - b.y0)) continue;       // a tower with its anvil, not a belt of merged ones
                if (!tall || b.y1 - b.y0 > tall->y1 - tall->y0) tall = &b;
            }
            if (tall) {
                const int w50 = widthAt(*tall, W, 0.5f), w90 = std::max({ widthAt(*tall, W, 0.7f), widthAt(*tall, W, 0.76f), widthAt(*tall, W, 0.82f), widthAt(*tall, W, 0.88f) });   // the anvil spreads over the top third
                if (w50 > 0) { q.push_back((float)w90 / (float)w50); hMax = std::max(hMax, tall->y1 - tall->y0 + 1); }
            }
            UnloadImage(on); UnloadImage(off);
        }
    if (heightPx) *heightPx = hMax;
    if (views) *views = (int)q.size();
    if (q.empty()) return 0.f;
    std::sort(q.begin(), q.end());
    return q[q.size() / 2];
}
static void notExtruded() {
    int n = 0, h = 0, nv = 0;
    const float taper = cumulusTaper(false, &n), taperOld = cumulusTaper(true);
    const float anvil = cbAnvil(false, &h, &nv), anvilOld = cbAnvil(true);
    std::printf("[weather] shape: cumulus width at 80%% of the height over 30%%: %.2f (median of %d isolated clouds; the extruded old shape %.2f); cumulonimbus widest of 70-88%% of the height over 50%%: %.2f (one storm cell a view) (median of %d isolated towers, tallest %d px; extruded %.2f)\n",
                taper, n, taperOld, anvil, nv, h, anvilOld);
    CHECK(n >= 3 && taper < 0.8f);
    CHECK(taperOld >= 0.8f);
    CHECK(nv >= 6 && anvil > 1.2f);
    CHECK(anvilOld < 1.1f);                                           // control: the extruded old shape has no anvil
}

// Soft edges: the alpha ramp (10% to 90%) across cumulus edges at about 2 km, at 1080p. The alpha is the share of the
// way from the sky to the cloud: the change when the clouds are switched on, over its plateau along the scanline.
static float edgeRamp(bool extruded, int* edges, float* softFrac = nullptr, bool binary = false) {
    constexpr int W = 1920, H = 1080;
    auto mk = [&](bool on) {
        auto s = filmClouds({ 0, 1300, 0 }, { 0, 1300 + 500, -500 }, 0.4f, g3d::CloudType::Cumulus, on, 1500, 3500, 60, false);
        s->clouds.extruded = extruded;
        return look(s, W, H, 0);
    };
    Image on = mk(true), off = mk(false);
    std::vector<float> d((size_t)W * H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) d[(size_t)y * W + x] = (float)pixelDelta(GetImageColor(on, x, y), GetImageColor(off, x, y));
    UnloadImage(on); UnloadImage(off);
    if (binary)                                                  // control: the same picture with the alpha cut to 0 or 1
        for (float& v : d) v = v >= 60.f ? 300.f : 0.f;
    std::vector<float> widths;
    for (int y = 40; y < H - 40 && widths.size() < 40; y += 13) {
        for (int x = 70; x < W - 70 && widths.size() < 40; x++) {
            // a monotone rise or fall within +-30 px from < 8 to > 120 (of 765) on the other side
            for (int dir = -1; dir <= 1; dir += 2) {
                const float lo = d[(size_t)y * W + x - 30 * dir], hi = d[(size_t)y * W + x + 30 * dir];
                if (lo > 8 || hi < 120) continue;
                bool mono = true;
                for (int k = -30; k < 30 && mono; k++)
                    if ((d[(size_t)y * W + x + (k + 1) * dir] - d[(size_t)y * W + x + k * dir]) < -6) mono = false;
                if (!mono) continue;
                int a = -1, b = -1;
                for (int k = -30; k <= 30; k++) {
                    const float v = (d[(size_t)y * W + x + k * dir] - lo) / (hi - lo);
                    if (a < 0 && v >= 0.1f) a = k;
                    if (b < 0 && v >= 0.9f) b = k;
                }
                if (a >= 0 && b >= a) { widths.push_back((float)(b - a + 1)); x += 80; break; }
            }
        }
    }
    if (edges) *edges = (int)widths.size();
    if (softFrac) { int soft = 0; for (float w : widths) soft += w >= 3.f; *softFrac = widths.empty() ? 0.f : (float)soft / (float)widths.size(); }
    if (widths.empty()) return 0.f;
    std::sort(widths.begin(), widths.end());
    return widths[widths.size() / 2];
}
static void softEdges() {
    int n = 0, nOld = 0, nBin = 0;
    float soft = 0, softBin = 0, softOld = 0;
    const float w = edgeRamp(false, &n, &soft), wOld = edgeRamp(true, &nOld, &softOld), wBin = edgeRamp(false, &nBin, &softBin, true);
    std::printf("[weather] edge ramp (10%% to 90%%) at 1080p, median of %d cumulus edges: %.1f px, %.0f%% of the crossings span 3 px or more (the extruded hard-surface shape: %.1f px, %.0f%%; the same frame with the alpha cut to 0/1: %.1f px, %.0f%%)\n",
                n, w, 100 * soft, wOld, 100 * softOld, wBin, 100 * softBin);
    CHECK(n >= 10 && w >= 6.f);
    CHECK(soft >= 0.8f);
    CHECK(wOld < w * 0.7f);
    CHECK(softBin <= 0.1f);
}

// Whiskers: horizontal combing along the lower edges of far clouds. It came from a coarse step lattice shared by every
// pixel of a row (the jitter spanned 70 m of a 1.5 km step), so the cloud edge was cut at the same depth along the
// whole row. The index is, over the lower band of the noon frame, the mask changes between rows per mask change along a
// row: combed edges raise it.
static constexpr float WHISKER_BOUND = 2.8f;   // 2.55 clean; 3.12 with the jitter spanning 70 m of a 1.5 km step
static float whiskerIndex() {
    constexpr int W = 1920, H = 1080;
    const float c30 = std::tan(30 * DEG2RAD);
    auto mk = [&](bool on) {
        auto s = cloudScene({ 0, 2, 0 }, { 0, 2 + c30 * 100, -100 }, 0.45f);
        s->clouds.enabled = on;
        s->clouds.shadows = false;
        return look(s, W, H, 0);
    };
    Image on = mk(true), off = mk(false);
    const std::vector<char> m = cloudMask(on, off, 40);
    UnloadImage(on); UnloadImage(off);
    const int y0 = 880;
    long long horiz = 0, vert = 0;                                      // changes of the mask along a row, and between rows
    for (int y = y0; y < H - 1; y++)
        for (int x = 0; x < W - 1; x++) {
            horiz += m[(size_t)y * W + x] != m[(size_t)y * W + x + 1];
            vert += m[(size_t)y * W + x] != m[(size_t)(y + 1) * W + x];
        }
    return horiz > 0 ? (float)vert / (float)horiz : 0.f;
}
// Stipple: the Laplacian variance of the luma over the cloud pixels (a 5-point Laplacian over the pixels of the mask
// and the ring of 3 round it): how much high-frequency energy the march's own noise leaves.
static float stippleEnergy(bool denoise) {
    constexpr int W = 960, H = 540;
    auto mk = [&](bool on) {
        auto s = filmClouds({ 0, 2, 0 }, { 0, 2 + 40, -100 }, 0.45f, g3d::CloudType::Cumulus, on, 1500, 3500, 60, false);
        s->clouds.denoise = denoise;
        return look(s, W, H, 0);
    };
    Image on = mk(true), off = mk(false);
    const std::vector<char> m = cloudMask(on, off, 30);
    std::vector<float> l = lumaOf(on);
    double s1 = 0, s2 = 0; long long n = 0;
    for (int y = 4; y < H - 4; y++)
        for (int x = 4; x < W - 4; x++) {
            bool near = false;
            for (int j = -3; j <= 3 && !near; j++) for (int i = -3; i <= 3; i++) if (m[(size_t)(y + j) * W + x + i]) { near = true; break; }
            if (!near) continue;
            const double lap = 4.0 * l[(size_t)y * W + x] - l[(size_t)y * W + x - 1] - l[(size_t)y * W + x + 1] - l[(size_t)(y - 1) * W + x] - l[(size_t)(y + 1) * W + x];
            s1 += lap; s2 += lap * lap; n++;
        }
    UnloadImage(on); UnloadImage(off);
    return n ? (float)(s2 / n - (s1 / n) * (s1 / n)) : 0.f;
}
static void stipple() {
    const float with = stippleEnergy(true), without = stippleEnergy(false);
    std::printf("[weather] stipple: Laplacian variance over the cloud pixels %.1f with the blue noise and the filter, %.1f with the white noise and no filter\n", with, without);
    CHECK(with < 46.f);                       // 1.3 x the clean render (35.2); 203.6 with the filter weighting alpha as it did (2.5), 98 before the jitter was fixed
    CHECK(without > 2.f * with);
    const float wh = whiskerIndex();
    std::printf("[weather] whiskers: %.2f mask changes between rows per change along a row, noon frame lower band (combed edges raise it)\n", wh);
    CHECK(wh < WHISKER_BOUND);
}


// No seam. A faint vertical line at the middle column of a frame made cloud-gap edges end at a common column. A true
// cloud edge moves across the screen when the camera yaws; a screen-fixed seam does not. So three frames are looked
// at, yawed by -2, 0 and +2 degrees: the excess of each column's mean difference to its neighbour (over the median
// of the columns round it) is taken in each, and the smallest of the three is the joint excess. Only a column that
// stands out in every yaw, a line fixed to the screen, scores high; the same check must see a seam added to the three.
static std::vector<float> seamExcess(const Image& img) {
    const std::vector<float> l = lumaOf(img);
    const int W = img.width, H = img.height;
    std::vector<float> diff((size_t)W - 1, 0.f), ex((size_t)W, 0.f);
    for (int x = 0; x + 1 < W; x++) {
        double s = 0;
        for (int y = H / 2; y < H; y++) s += std::fabs(l[(size_t)y * W + x] - l[(size_t)y * W + x + 1]);
        diff[(size_t)x] = (float)(s / (H - H / 2));
    }
    for (int x = 4; x + 5 < W; x++) {
        std::vector<float> nb;
        for (int k = -4; k <= 4; k++) if (k != 0) nb.push_back(diff[(size_t)(x + k)]);
        std::sort(nb.begin(), nb.end());
        ex[(size_t)x] = diff[(size_t)x] / std::max(nb[nb.size() / 2], 0.5f);
    }
    return ex;
}
static float jointSeam(const std::vector<std::vector<float>>& ex, int* at) {
    float worst = 0;
    for (size_t x = 0; x < ex[0].size(); x++) {
        float m = 1e9f;
        for (const auto& e : ex) m = std::min(m, e[x]);
        if (m > worst) { worst = m; if (at) *at = (int)x; }
    }
    return worst;
}
// The cause of the seam (a faint vertical line near the middle column of the 12 km frame, where cloud gaps ended) was
// the coverage map's interpolation. Easing the bilinear fraction (smoothstep) removes the kinks of the hardware blend
// but flattens the field on every texel centre line; those are world planes (hundreds of metres apart at the coarse
// levels far clouds read), and from a height they all pile onto the frame's middle columns. The line is fixed in the
// world, not the screen (it moves with the camera's yaw, which is why the yawed frames below cannot see it alone).
// Looking straight down, the cloud edges of an isotropic field point every way alike; along lattice lines they
// cluster on the two axes. The measure is the weighted mean of cos(4 x edge direction) over the edge pixels of the
// cloud mask (|gradient|^4 weights): 0 for edges in all directions, 1 for edges only along the axes. Averaged over 4
// seeds (which place the lattice differently) at three heights (which change the level read).
static float edgeAxisAlignment(int seeds, float height) {
    constexpr int S = 640;
    double num = 0, den = 0;
    for (int seed = 1; seed <= seeds; seed++) {
        auto mk = [&](bool on) {
            auto s = filmClouds({ 0, height, 0 }, { 0, 0, -1500 }, 0.5f, g3d::CloudType::Cumulus, on, 1500, 3500, 50, true);
            s->clouds.seed = (unsigned)seed;
            s->clouds.resolution = 0.5f;
            return look(s, S, S, 0);
        };
        Image on = mk(true), off = mk(false);
        std::vector<float> m((size_t)S * S);
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) m[(size_t)y * S + x] = pixelDelta(GetImageColor(on, x, y), GetImageColor(off, x, y)) >= 45 ? 1.f : 0.f;
        UnloadImage(on); UnloadImage(off);
        std::vector<float> b(m.size(), 0.f);                                   // a 5 x 5 box blur: edges a few pixels wide
        for (int y = 2; y < S - 2; y++)
            for (int x = 2; x < S - 2; x++) {
                float a = 0; for (int j = -2; j <= 2; j++) for (int i = -2; i <= 2; i++) a += m[(size_t)(y + j) * S + x + i];
                b[(size_t)y * S + x] = a / 25.f;
            }
        for (int y = 4; y < S - 4; y++)
            for (int x = 4; x < S - 4; x++) {
                const float gx = b[(size_t)y * S + x + 1] - b[(size_t)y * S + x - 1], gy = b[(size_t)(y + 1) * S + x] - b[(size_t)(y - 1) * S + x];
                const double g2 = (double)gx * gx + (double)gy * gy;
                num += (double)gx * gx * gx * gx - 6.0 * (double)gx * gx * gy * gy + (double)gy * gy * gy * gy;
                den += g2 * g2;
            }
    }
    return den > 0 ? (float)(num / den) : 0.f;
}
static void noSeam() {
    {
        float a = -1, heights[3] = { 12000.f, 15000.f, 20000.f }, each[3];
        for (int i = 0; i < 3; i++) { each[i] = edgeAxisAlignment(4, heights[i]); a = std::max(a, each[i]); }
        std::printf("[weather] coverage map isotropy: cloud edges seen from above at 12, 15 and 20 km cluster on the axes by %.3f, %.3f, %.3f (0 = every direction alike, 1 = axes only)\n", each[0], each[1], each[2]);
        CHECK(a < 0.05f);
    }
    constexpr int W = 1920, H = 1080;
    std::vector<Image> imgs;
    for (float yaw : { -2.f, 0.f, 2.f }) {
        const float a = yaw * DEG2RAD;
        const float ox = 100 * std::sin(a), oz = -100 * std::cos(a);          // the view direction, yawed
        auto s = filmClouds({ 0, 12000, 0 }, { ox, 11990, oz }, 0.5f, g3d::CloudType::Cumulus, true, 1500, 3500, 60);
        imgs.push_back(look(s, W, H, 0));
    }
    dump("cloud-12km.png", imgs[1]);
    std::vector<std::vector<float>> ex, exFake;
    for (const Image& img : imgs) {
        ex.push_back(seamExcess(img));
        Image fake = ImageCopy(img);
        for (int y = 0; y < fake.height; y++)
            for (int x = fake.width / 2; x < fake.width; x++) {
                Color c = GetImageColor(fake, x, y);
                c.r = (unsigned char)std::min(255, c.r + 10); c.g = (unsigned char)std::min(255, c.g + 10); c.b = (unsigned char)std::min(255, c.b + 10);
                ImageDrawPixel(&fake, x, y, c);
            }
        exFake.push_back(seamExcess(fake));
        UnloadImage(fake);
    }
    int at = 0, atFake = 0;
    const float real = jointSeam(ex, &at), fakeSeam = jointSeam(exFake, &atFake);
    float single = 0; for (float v : ex[1]) single = std::max(single, v);
    std::printf("[weather] seam: joint column excess over yaws -2, 0, +2 degrees %.2f (column %d; the worst of the one frame alone %.2f); with a seam of 10 levels added at column %d: %.2f (column %d)\n",
                real, at, single, W / 2, fakeSeam, atFake);
    CHECK(real < 2.f);
    CHECK(fakeSeam > 3.f);
    for (Image& i : imgs) UnloadImage(i);
}

// The sun stays crisp: where no cloud is near, the picture with clouds is the picture without, pixel for pixel; and the
// disc is a clean disc (10% to 90% in at most 13 px with its glow). A sky baked at 256 px makes the blocky sun of the review.
static float sunEdge(const Image& img, Vector2 c) {
    std::vector<float> sum(60, 0.f); std::vector<int> cnt(60, 0);
    for (int y = (int)c.y - 59; y <= (int)c.y + 59; y++)
        for (int x = (int)c.x - 59; x <= (int)c.x + 59; x++) {
            if (x < 0 || y < 0 || x >= img.width || y >= img.height) continue;
            const float r = std::sqrt((x + 0.5f - c.x) * (x + 0.5f - c.x) + (y + 0.5f - c.y) * (y + 0.5f - c.y));
            if (r < 60) { sum[(size_t)r] += luma(GetImageColor(img, x, y)); cnt[(size_t)r]++; }
        }
    std::vector<float> p(60);
    for (int i = 0; i < 60; i++) p[(size_t)i] = cnt[(size_t)i] ? sum[(size_t)i] / cnt[(size_t)i] : 0.f;
    const float in = p[0], out = p[40];
    float a = -1, b = -1;
    for (int i = 0; i < 60; i++) {
        const float v = (in - p[(size_t)i]) / std::max(in - out, 1.f);
        if (a < 0 && v >= 0.1f) a = (float)i;
        if (b < 0 && v >= 0.9f) b = (float)i;
    }
    return a < 0 || b < 0 ? 99.f : b - a + 1.f;
}
static void crispSun() {
    constexpr int W = 1280, H = 720;
    int done = 0;
    for (unsigned seed = 1; seed <= 12 && !done; seed++) {
        auto mk = [&](bool on, int skyRes) {
            auto s = filmClouds({ 0, 2, 0 }, { 0, 2, -100 }, 0.15f, g3d::CloudType::Cumulus, on, 1500, 3500, 60, false);
            const Vector3 sun = Vector3Normalize(s->sky.sunDirection);
            s->camera.target = { sun.x * 100, 2 + sun.y * 100, sun.z * 100 };
            s->camera.fov = 40;
            s->clouds.seed = seed;
            s->sky.resolution = skyRes;
            return s;
        };
        Image on = look(mk(true, 2048), W, H, 0), off = look(mk(false, 2048), W, H, 0);
        Image blocky = look(mk(false, 256), W, H, 0);
        const std::vector<char> m = cloudMask(on, off, 10);
        const Vector2 c{ W / 2.f, H / 2.f };                                   // the sun is at the middle of the frame
        int nearCloud = 0;
        for (int y = (int)c.y - 90; y < (int)c.y + 90; y++) for (int x = (int)c.x - 90; x < (int)c.x + 90; x++) nearCloud += m[(size_t)y * W + x];
        if (nearCloud == 0) {
            long long diff = 0;
            for (int y = (int)c.y - 90; y < (int)c.y + 90; y++) for (int x = (int)c.x - 90; x < (int)c.x + 90; x++) diff += pixelDelta(GetImageColor(on, x, y), GetImageColor(off, x, y));
            const float e = sunEdge(on, c), eBlocky = sunEdge(blocky, c);
            std::printf("[weather] sun (seed %u, no cloud within 90 px): %lld summed difference to the cloud-free frame in the 180 px square; disc edge %.1f px (a sky baked at 256: %.1f px)\n", seed, diff, e, eBlocky);
            CHECK(diff == 0);
            CHECK(e <= 13.f);
            CHECK(eBlocky > 1.5f * e);
            done = 1;
        }
        UnloadImage(on); UnloadImage(off); UnloadImage(blocky);
    }
    CHECK(done == 1);
}

// Haze: clouds within 5 degrees of the horizon seen from 12 km differ less from the sky than clouds 20 degrees below it.
static float bandContrast(const Image& on, const Image& off, float degFrom, float degTo, float fovY) {
    const int H = on.height, W = on.width;
    const float tanY = std::tan(fovY * 0.5f * DEG2RAD);
    double s = 0; long long n = 0;
    for (int y = 0; y < H; y++) {
        const float dep = std::atan(((y + 0.5f) / (H / 2.f) - 1.f) * tanY) * RAD2DEG;       // degrees below level
        if (dep < degFrom || dep > degTo) continue;
        for (int x = 0; x < W; x++) {
            const float d = (float)pixelDelta(GetImageColor(on, x, y), GetImageColor(off, x, y));
            if (d >= 15) { s += d; n++; }
        }
    }
    return n > 200 ? (float)(s / n) : -1.f;
}
static void hazeAtHorizon() {
    constexpr int W = 640, H = 480;
    float nearH[2], farH[2];
    for (int hz = 0; hz < 2; hz++) {
        auto mk = [&](bool on) {
            auto s = filmClouds({ 0, 12000, 0 }, { 0, 12000, -100 }, 0.6f, g3d::CloudType::Cumulus, on, 1500, 3500, 80, false);
            s->clouds.haze = hz == 0 ? 1.f : 0.f;
            return look(s, W, H, 0);
        };
        Image on = mk(true), off = mk(false);
        dump(hz == 0 ? "cloud-haze-on.png" : "cloud-haze-off.png", on);
        nearH[hz] = bandContrast(on, off, 3.3f, 5.f, 80);
        farH[hz] = bandContrast(on, off, 18.f, 22.f, 80);
        UnloadImage(on); UnloadImage(off);
    }
    std::printf("[weather] haze, mean difference to the sky: 3.3 to 5 degrees below the horizon %.1f, 18 to 22 degrees %.1f (haze on); %.1f and %.1f with the haze off\n", nearH[0], farH[0], nearH[1], farH[1]);
    CHECK(nearH[0] > 0 && farH[0] > 0 && nearH[0] < 0.8f * farH[0]);
    CHECK(nearH[0] / farH[0] < 0.8f * nearH[1] / farH[1]);     // control: the haze, not the view, makes the horizon clouds weaker
}

// The nearest clouds carry the most detail: in the noon frame, the Laplacian variance inside the cloud overhead
// (1.5 km up) over the squared mean luma of the same crop is at least half that of a cloud at about 5 km, and
// switching the detail off lowers it (the control).
static float crispness(const Image& on, const Image& off, int cx, int cy, int cw, int ch, float* covered, float distance) {
    const std::vector<float> l = lumaOf(on);
    const int W = on.width;
    // Band-pass energy over the same sizes on the cloud, not the same pixels: a 3 m to 35 m scale at the cloud's
    // distance (a pixel is 0.0016 rad of the 60 degree view), a box mean of radius rs minus one of radius rl. A
    // single-pixel Laplacian only sees the march's noise, and a cloud 2 km away has none of that at its scale.
    const float mPerPx = distance * 0.0016f;
    const int rs = std::max(1, (int)std::lround(3.f / mPerPx)), rl = std::max(rs + 2, (int)std::lround(35.f / mPerPx));
    auto box = [&](int x, int y, int r) {
        double s = 0; for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++) s += l[(size_t)(y + j) * W + x + i];
        return s / ((2 * r + 1) * (2 * r + 1));
    };
    int inside = 0; double m = 0, s1 = 0, s2 = 0; long long n = 0;
    for (int y = cy; y < cy + ch; y++)
        for (int x = cx; x < cx + cw; x++) {
            inside += pixelDelta(GetImageColor(on, x, y), GetImageColor(off, x, y)) >= 60;
            m += l[(size_t)y * W + x];
            const double bp = box(x, y, rs) - box(x, y, rl);
            s1 += bp; s2 += bp * bp; n++;
        }
    m /= (double)n;
    if (covered) *covered = (float)inside / (float)n;
    return (float)((s2 / n - (s1 / n) * (s1 / n)) / (m * m) * 1000.0);
}
static void nearDetail() {
    constexpr int W = 1280, H = 720;
    const float c30 = std::tan(30 * DEG2RAD);
    auto mk = [&](bool on, float detail) {
        auto s = cloudScene({ 0, 2, 0 }, { 0, 2 + c30 * 100, -100 }, 0.45f);
        s->clouds.enabled = on;
        s->clouds.detail = detail;
        s->clouds.shadows = false;
        return look(s, W, H, 0);
    };
    Image on = mk(true, 0.5f), off = mk(false, 0.5f), flat = mk(true, 0.f);
    float cn = 0, cf = 0;
    const float eNear = crispness(on, off, 40, 40, 150, 130, &cn, 2100.f), eFar = crispness(on, off, 800, 470, 150, 50, &cf, 5000.f);
    const float eNearFlat = crispness(flat, off, 40, 40, 150, 130, nullptr, 2100.f);
    std::printf("[weather] detail: band-pass (3 to 35 m on the cloud) variance / luma^2 (x1000) in the nearest cloud %.2f (%.0f%% cloud), in a cloud at about 5 km %.2f (%.0f%% cloud); the nearest cloud with the detail off %.2f\n",
                eNear, 100 * cn, eFar, 100 * cf, eNearFlat);
    dump("cloud-noon-1280.png", on);
    CHECK(cn > 0.8f && cf > 0.8f);
    CHECK(eNear >= 0.7f * eFar);                  // the nearest cloud is not the blurriest (it was an airbrushed blob at 0.05 of the far one)
    CHECK(eNearFlat < 0.5f * eNear);              // and the detail switch is what gives it its structure
    UnloadImage(on); UnloadImage(off); UnloadImage(flat);
}

// Bands in height: the row-wise high-pass energy of a flat cloud interior over the column-wise one. Bands in height
// (a step lattice, or noise sampled at the nearest slice) make the rows far stronger than the columns.
static float bandAniso(bool jitter, bool nearest) {
    constexpr int W = 960, H = 540;
    const float c30 = std::tan(30 * DEG2RAD);
    auto s = cloudScene({ 0, 2, 0 }, { 0, 2 + c30 * 100, -100 }, 0.45f);
    s->clouds.jitter = jitter;
    s->clouds.nearestSlice = nearest;
    s->clouds.shadows = false;
    Image img = look(s, W, H, 0);
    const std::vector<float> l = lumaOf(img);
    const int x0 = 20, y0 = 40, w = 160, h = 110;
    auto rms = [&](bool rows) {
        const int n = rows ? h : w, m = rows ? w : h;
        std::vector<double> p((size_t)n, 0.0);
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < m; j++) p[(size_t)i] += rows ? l[(size_t)(y0 + i) * W + x0 + j] : l[(size_t)(y0 + j) * W + x0 + i];
            p[(size_t)i] /= m;
        }
        double s2 = 0; int c = 0;
        for (int i = 4; i < n - 4; i++) {
            double avg = 0; for (int k = -4; k <= 4; k++) avg += p[(size_t)(i + k)];
            avg /= 9; s2 += (p[(size_t)i] - avg) * (p[(size_t)i] - avg); c++;
        }
        return std::sqrt(s2 / c);
    };
    const double r = rms(true), c = rms(false);
    UnloadImage(img);
    return (float)(r / std::max(c, 0.05));
}
static void noBandsInHeight() {
    const float good = bandAniso(true, false), lattice = bandAniso(false, false), nearest = bandAniso(true, true);
    std::printf("[weather] bands in height: row / column high-pass energy of a cloud interior %.2f (jittered steps, interpolated slices); %.2f with the jitter off; %.2f with the nearest slice\n", good, lattice, nearest);
    CHECK(good < 3.f);
}

// ---------------------------------------------------------------- water that reflects the scene (W6)
// (e) Off means off. A busy water scene (a sun with shadows, fog, a choppy sea, a red pole, a plate and a box
// under the surface) drawn by the engine before W6 existed: the mean colour of a 6 x 4 grid of cells, recorded
// then. With reflections and refraction off the pictures must be the same, to the level or two a driver
// differs by.
static g3d::ScenePtr w6Golden() {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 2.2f, 9 };
    s->camera.target = { 0, 0.4f, 0 };
    s->camera.fov = 50;
    s->sky = g3d::Sky::day();
    s->sky.clouds = 0.35f;
    s->sky.sunLight = 0;
    s->lights.push_back(g3d::Light::directional(Vector3Scale(Vector3Normalize(s->sky.sunDirection), 60), WHITE, 2.0f));
    s->lights[0].castShadow = true;
    s->lights[0].shadowExtent = 14;
    s->shadowMapSize = 1024;
    s->ambient = 0.4f;
    s->fogMode = g3d::FogMode::Exponential;
    s->fogDensity = 0.004f;
    s->fogFromSky = true;
    s->water = g3d::Water::choppy();
    s->water.waves = { { 15, 14.0f, 0.16f, 1.6f, 1 }, { -30, 7.0f, 0.1f, 1.8f, 1 }, { 60, 3.3f, 0.06f, 1.5f, 1 }, { -75, 1.8f, 0.035f, 1, 1 }, { 5, 0.9f, 0.02f, 1, 1 }, { 120, 0.5f, 0.01f, 1, 1 } };    // the choppy preset as it was when the golden was recorded
    s->water.openDepth = 0;
    g3d::Material red; red.color = { 220, 30, 30, 255 };
    s->add(g3d::Geometry::cylinder(0.15f, 2.0f), red)->position = { -1.5f, 1.0f, 0 };
    g3d::Material white; white.color = { 240, 240, 240, 255 };
    s->add(g3d::Geometry::box(3, 0.05f, 3), white)->position = { 2.2f, -1.0f, 0 };
    g3d::Material green; green.color = { 30, 190, 60, 255 };
    s->add(g3d::Geometry::box(0.8f, 1.4f, 0.3f), green)->position = { 0.2f, -0.3f, -1.5f };
    g3d::Material rock; rock.color = { 120, 110, 100, 255 };
    s->add(g3d::Geometry::sphere(2.5f), rock)->position = { 3.5f, -0.5f, -9 };
    return s;
}
static std::vector<float> gridMeans(const Image& img, int gx, int gy) {
    std::vector<float> out;
    for (int j = 0; j < gy; j++)
        for (int i = 0; i < gx; i++) {
            double r = 0, g = 0, b = 0; int n = 0;
            for (int y = j * img.height / gy; y < (j + 1) * img.height / gy; y++)
                for (int x = i * img.width / gx; x < (i + 1) * img.width / gx; x++) {
                    const Color c = GetImageColor(img, x, y); r += c.r; g += c.g; b += c.b; n++;
                }
            out.push_back((float)(r / n)); out.push_back((float)(g / n)); out.push_back((float)(b / n));
        }
    return out;
}

// Recorded from the engine before W6 (RMTN_W6_GOLDEN=1 prints it), RGB per cell, 6 x 4 cells of 320 x 180, frame 7.
static const float W6_GOLD[72] = {
    159.40f, 178.88f, 216.09f, 156.94f, 175.46f, 213.63f, 158.38f, 176.13f, 213.77f, 154.51f, 174.41f, 213.79f, 153.03f, 175.50f, 215.60f, 159.50f, 182.37f, 219.94f,
    147.28f, 162.28f, 189.27f, 142.22f, 155.28f, 181.14f, 140.33f, 146.52f, 168.90f, 115.26f, 130.38f, 149.97f, 135.93f, 152.87f, 179.86f, 142.01f, 161.02f, 190.52f,
    93.12f, 97.95f, 108.29f, 93.59f, 98.21f, 108.72f, 92.70f, 96.46f, 104.20f, 107.16f, 145.37f, 154.49f, 112.18f, 135.10f, 147.28f, 108.21f, 115.39f, 128.14f,
    81.66f, 83.03f, 86.00f, 79.21f, 79.70f, 81.09f, 79.43f, 79.27f, 79.40f, 79.51f, 79.54f, 79.88f, 79.55f, 79.75f, 80.39f, 113.56f, 112.74f, 111.14f };
static void offMeansOffWater() {
    auto s = w6Golden();
    s->water.reflections = g3d::Water::Reflections::Off;     // explicit: the defaults are these
    s->water.refraction = false;
    s->water.reflectionDistort = 3;
    Image img = look(s, 320, 180, 7);
    dump("w6-off.png", img);
    const std::vector<float> m = gridMeans(img, 6, 4);
    float worst = 0;
    for (size_t i = 0; i < m.size(); i++) worst = std::max(worst, std::fabs(m[i] - W6_GOLD[i]));
    std::printf("[weather] W6 off means off: the busy water scene against the pre-W6 recording, worst cell mean off by %.3f levels\n", worst);
    CHECK(worst < 1.5f);
    // Control: reflections on is a different picture.
    auto on = w6Golden();
    on->water.reflections = g3d::Water::Reflections::Scene;
    Image img2 = look(on, 320, 180, 7);
    const std::vector<float> m2 = gridMeans(img2, 6, 4);
    float worstOn = 0;
    for (size_t i = 0; i < m2.size(); i++) worstOn = std::max(worstOn, std::fabs(m2[i] - W6_GOLD[i]));
    std::printf("[weather] W6 off means off, control: with reflections on the worst cell moves by %.1f levels\n", worstOn);
    CHECK(worstOn > 0.8f);
    // And refraction alone moves it too, and both settings back to off restore it exactly (same process, same pixels).
    auto both = w6Golden();
    both->water.reflections = g3d::Water::Reflections::Scene;
    both->water.refraction = true;
    Image img3 = look(both, 320, 180, 7);
    Image img4 = look(s, 320, 180, 7);
    CHECK(diffImages(img, img4) == 0);
    CHECK(diffImages(img, img3) > 20000);
    UnloadImage(img); UnloadImage(img2); UnloadImage(img3); UnloadImage(img4);
}

// A lake at y = 0: flat when `chop` is 0, else four short crossing waves. openDepth 30 makes it opaque unless asked.
static g3d::Water w6Water(float chop, float open = 30) {
    g3d::Water w;
    w.enabled = true;
    w.foam = 0; w.sunGlint = 0; w.caustics = 0; w.openDepth = open;
    w.deep = { 20, 70, 90, 255 };
    if (chop > 0) w.waves = { { 10, 3.0f, 0.06f * chop, 1, 1 }, { -25, 1.7f, 0.035f * chop, 1, 1 }, { 40, 1.1f, 0.02f * chop, 1, 1 }, { 70, 0.7f, 0.012f * chop, 1, 1 } };
    w.reflections = g3d::Water::Reflections::Scene;
    return w;
}
static g3d::ScenePtr w6Scene(const g3d::Water& water, Vector3 pos, Vector3 target, float fov = 40) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = pos;
    s->camera.target = target;
    s->camera.fov = fov;
    s->camera.farPlane = 2000;
    s->sky = g3d::Sky::day();
    s->sky.clouds = 0;
    s->lights.push_back(g3d::Light::directional({ 10, 14, 6 }, WHITE, 1.6f));
    s->ambient = 0.5f;
    s->water = water;
    return s;
}
static g3d::Material flat(Color c) { g3d::Material m; m.color = c; m.unlit = true; return m; }
static float redness(Color c) { return (float)c.r - (float)std::max(c.g, c.b); }
static float greenness(Color c) { return (float)c.g - (float)std::max(c.r, c.b); }
static float rowAvg(const Image& img, int x, int y, int hw, float (*f)(Color)) {
    float s = 0;
    for (int i = -hw; i <= hw; i++) s += f(GetImageColor(img, x + i, y));
    return s / (2 * hw + 1);
}
// What the reflection adds: the picture with reflections on minus the same one with them off, per channel,
// averaged across the column x +- hw at row y (the waves, the sky and the pole are in both).
static Vector3 addedAt(const Image& on, const Image& off, int x, int y, int hw) {
    Vector3 s{ 0, 0, 0 };
    for (int i = -hw; i <= hw; i++) {
        const Color a = GetImageColor(on, x + i, y), b = GetImageColor(off, x + i, y);
        s.x += a.r - b.r; s.y += a.g - b.g; s.z += a.b - b.b;
    }
    return Vector3Scale(s, 1.f / (2 * hw + 1));
}

// A red pole 1.2 m tall stands in a lake, seen from 1.5 m up, 9 m away, level.
static g3d::ScenePtr poleLake(float chop, float distort, bool reflections) {
    auto s = w6Scene(w6Water(chop), { 0, 1.5f, 9 }, { 0, 1.5f, 0 });
    s->water.reflectionDistort = distort;
    if (!reflections) s->water.reflections = g3d::Water::Reflections::Off;
    s->add(g3d::Geometry::cylinder(0.12f, 1.2f), flat({ 215, 25, 25, 255 }))->position = { 0, 0.6f, 0 };
    return s;
}

// (a) A red pole in calm water: upside down below its base, as long as it is, starting at its base.
struct PoleLook { int top, base, end, gap; float lengthRatio; };
static PoleLook poleLook(const char* tag, float chop = 0, std::function<void(g3d::Scene&)> tweak = nullptr) {
    constexpr int W = 480, H = 270;
    auto s = poleLake(chop, 1, true);
    if (tweak) tweak(*s);
    auto off = poleLake(chop, 1, false);
    Image img = look(s, W, H, 3), ref = look(off, W, H, 3);
    dump((std::string("w6-pole-") + tag + ".png").c_str(), img);
    const int x = (int)std::lround(project(s->camera, { 0, 0, 0 }, W, H).x);
    const int baseRow = (int)std::lround(project(s->camera, { 0, 0, 0 }, W, H).y);
    PoleLook r{};
    int y = baseRow - 3;
    while (y > 0 && rowAvg(img, x, y, 2, redness) > 90) y--;
    r.top = y + 1;
    y = baseRow - 3;
    while (y < H - 1 && rowAvg(img, x, y, 2, redness) > 90) y++;
    r.base = y;                                       // the pole's last row
    auto added = [&](int yy) { const Vector3 a = addedAt(img, ref, x, yy, 2); return std::fabs(a.x) + std::fabs(a.y) + std::fabs(a.z); };
    int end = r.base, gap = 0;
    for (int yy = r.base + 1; yy < H - 1; yy++) {     // the reflection: rows it changes, a gap or two allowed
        if (added(yy) > 30) { end = yy; gap = 0; } else if (++gap > 3) break;
    }
    r.end = end;
    int first = r.base + 1;
    while (first < H - 1 && added(first) <= 30) first++;
    r.gap = first - r.base;
    r.lengthRatio = (float)(r.end - r.base) / std::max(1, r.base - r.top);
    UnloadImage(img); UnloadImage(ref);
    return r;
}
static void poleUpsideDown() {
    const PoleLook p = poleLook("calm");
    std::printf("[weather] W6 pole: top row %d, base row %d, reflection ends at row %d (starts %d rows below the base), length ratio %.2f\n", p.top, p.base, p.end, p.gap, p.lengthRatio);
    CHECK(p.end > p.base + 10);                       // the reflection's far end is lower on screen than its base
    CHECK(p.gap <= 3);                                // it starts at the base
    CHECK(p.lengthRatio > 0.85f && p.lengthRatio < 1.15f);
}

// (b) Waves break the reflection up and keep its colour. The reflection's profile down the pole's column is what
// it adds to the picture (red channel). Its total variation over its range: 1 for a smooth fade, more for a broken one.
struct Breakup { float variation; Vector3 mean, picture; };
static Breakup choppyPole(float chop, float distort, const char* tag) {
    constexpr int W = 480, H = 270;
    auto s = poleLake(chop, distort, true);
    auto off = poleLake(chop, distort, false);
    Image img = look(s, W, H, 5), ref = look(off, W, H, 5);
    dump((std::string("w6-choppy-") + tag + ".png").c_str(), img);
    const int x = (int)std::lround(project(s->camera, { 0, 0, 0 }, W, H).x);
    const int y0 = (int)std::lround(project(s->camera, { 0, 0, 0 }, W, H).y) + 4, y1 = (int)std::lround(project(s->camera, { 0, -1.2f, 0 }, W, H).y);
    std::vector<float> p;
    for (int y = y0; y <= y1; y++) p.push_back(addedAt(img, ref, x, y, 3).x);
    if (std::getenv("RMTN_W6_PROFILE")) { std::printf("%s:", tag); for (float v : p) std::printf(" %.0f", v); std::printf("\n"); }
    double sum = 0, sq = 0;
    for (float v : p) { sum += v; sq += (double)v * v; }
    const double mean = sum / p.size(), sd = std::sqrt(std::max(sq / p.size() - mean * mean, 0.0));
    Breakup b{};
    b.variation = (float)(sd / std::max(mean, 5.0));
    Vector3 m{ 0, 0, 0 };
    int n = 0;
    for (int y = y0; y < H; y++) { m = Vector3Add(m, addedAt(img, ref, x, y, 60)); n++; }
    b.mean = Vector3Scale(m, 1.f / n);
    Vector3 pm{ 0, 0, 0 };
    for (int y = y0; y < H; y++)
        for (int xx = x - 60; xx <= x + 60; xx++) { const Color c = GetImageColor(img, xx, y); pm = Vector3Add(pm, { (float)c.r, (float)c.g, (float)c.b }); }
    b.picture = Vector3Scale(pm, 1.f / (n * 121.f));
    UnloadImage(img); UnloadImage(ref);
    return b;
}
static void wavesBreakIt() {
    const Breakup calm = choppyPole(0, 1, "calm"), rough = choppyPole(0.5f, 1, "rough"), bendOff = choppyPole(0.5f, 0, "rough-nobend");
    std::printf("[weather] W6 waves: spread of the reflection down its column (std / mean of the red it adds), calm %.2f, choppy %.2f (choppy, bending off %.2f); mean added colour over the patch calm (%.1f %.1f %.1f), choppy (%.1f %.1f %.1f)\n",
                calm.variation, rough.variation, bendOff.variation, calm.mean.x, calm.mean.y, calm.mean.z, rough.mean.x, rough.mean.y, rough.mean.z);
    std::printf("[weather] W6 waves: whole picture over the patch, mean colour calm (%.1f %.1f %.1f), choppy (%.1f %.1f %.1f), choppy with the bending off (%.1f %.1f %.1f)\n",
                calm.picture.x, calm.picture.y, calm.picture.z, rough.picture.x, rough.picture.y, rough.picture.z, bendOff.picture.x, bendOff.picture.y, bendOff.picture.z);
    CHECK(calm.variation < 0.12f);
    CHECK(rough.variation > 0.5f);
    CHECK(bendOff.variation < 0.3f * rough.variation);      // the bending, not something else, breaks it
    // Breaking the reflection up must not change the patch's colour: the same waves with the bending off, and the calm lake (which has no
    // wave shading of its own), within a few levels.
    CHECK(std::fabs(rough.picture.x - bendOff.picture.x) < 4 && std::fabs(rough.picture.y - bendOff.picture.y) < 4 && std::fabs(rough.picture.z - bendOff.picture.z) < 4);
    CHECK(std::fabs(rough.picture.x - calm.picture.x) < 9 && std::fabs(rough.picture.y - calm.picture.y) < 9 && std::fabs(rough.picture.z - calm.picture.z) < 9);
}

// (c) No leak: a green box standing in the lake from 1.3 m under the surface to 0.3 m above it. The water is opaque,
// so under the line only its reflection shows green: the 0.3 m that stands out, not the 1.6 m of the whole box.
static int greenBelow(bool clip, int* aboveRows) {
    constexpr int W = 480, H = 270;
    auto s = w6Scene(w6Water(0), { 0, 1.0f, 7 }, { 0, 1.0f, 0 });
    s->add(g3d::Geometry::box(1.0f, 1.6f, 0.3f), flat({ 30, 200, 60, 255 }))->position = { 0, -0.5f, 0 };
    (void)clip;
    Image img = look(s, W, H, 3);
    dump(clip ? "w6-sunk-box.png" : "w6-sunk-box-noclip.png", img);
    const int x = (int)std::lround(project(s->camera, { 0, 0, 0 }, W, H).x);
    const int line = (int)std::lround(project(s->camera, { 0, 0, 0.15f }, W, H).y);     // where the front face meets the water
    int up = 0;
    for (int y = line - 1; y > 0 && rowAvg(img, x, y, 2, greenness) > 40; y--) up++;
    int down = 0, gap = 0;
    for (int y = line + 1; y < H; y++) {
        if (rowAvg(img, x, y, 2, greenness) > 8) { down = y - line; gap = 0; } else if (++gap > 3) break;
    }
    if (aboveRows) *aboveRows = up;
    UnloadImage(img);
    return down;
}
static void noLeak() {
    int up = 0;
    const int down = greenBelow(true, &up);
    std::printf("[weather] W6 no leak: the box stands %d rows above the line and its reflection runs %d rows below it (the whole box would be %d)\n", up, down, (int)std::lround(up * 1.6 / 0.3));
    CHECK(up > 8);
    CHECK(down > 0.5f * up && down < 1.6f * up);
}

// (d) Refraction: a white plate under clear water.
static g3d::ScenePtr plateScene(float depth, bool refract) {
    auto s = w6Scene(w6Water(0, 0), { 0, 2.5f, 6 }, { 0, -0.4f, 1 }, 40);
    s->water.reflections = g3d::Water::Reflections::Off;
    s->water.refraction = refract;
    s->water.absorb = { 0.34f, 0.07f, 0.04f };
    s->add(g3d::Geometry::box(6, 0.04f, 6), flat({ 255, 255, 255, 255 }))->position = { 0, -depth, 0 };
    return s;
}
// The row of the plate's far edge (z = -3) down the middle: the strongest rise in brightness going down.
static int plateEdgeRow(const Image& img) {
    int best = 0; float bestStep = 0;
    for (int y = 4; y < img.height - 4; y++) {
        float a = 0, b = 0;
        for (int i = -3; i <= 3; i++) { a += luma(GetImageColor(img, img.width / 2 + i, y - 2)); b += luma(GetImageColor(img, img.width / 2 + i, y + 2)); }
        if (b - a > bestStep) { bestStep = b - a; best = y; }
    }
    return best;
}
static void refraction() {
    constexpr int W = 480, H = 270;
    const float depth = 1.0f, camH = 2.5f, camZ = 6, edgeZ = -3;
    auto s = plateScene(depth, true);
    // Where the surface point must be so that the refracted ray (Snell, 1.333) reaches the edge at depth 1.
    float lo = edgeZ, hi = camZ - 0.01f;
    for (int i = 0; i < 60; i++) {
        const float zs = 0.5f * (lo + hi), dc = camZ - zs, ti = std::atan2(dc, camH), tt = std::asin(std::sin(ti) / 1.333f);
        if (zs - depth * std::tan(tt) > edgeZ) hi = zs; else lo = zs;
    }
    const float zs = 0.5f * (lo + hi);
    const float refrRow = project(s->camera, { 0, 0, zs }, W, H).y, straightRow = project(s->camera, { 0, -depth, edgeZ }, W, H).y;
    Image on = look(s, W, H, 3), off = look(plateScene(depth, false), W, H, 3);
    dump("w6-plate-refract.png", on); dump("w6-plate-straight.png", off);
    const int rowOn = plateEdgeRow(on), rowOff = plateEdgeRow(off);
    std::printf("[weather] W6 refraction: the plate's far edge is at row %d with refraction (predicted %.1f; straight view predicts %.1f, drawn without refraction at %d)\n", rowOn, refrRow, straightRow, rowOff);
    CHECK(std::fabs(rowOn - refrRow) < 3.5f);
    CHECK(std::fabs(rowOn - straightRow) > 4.f);
    CHECK(std::fabs(rowOff - straightRow) < 3.5f);
    // Deeper is dimmer and bluer.
    auto patch = [&](float d) {
        Image img = look(plateScene(d, true), W, H, 3);
        dump(d < 0.5f ? "w6-plate-shallow.png" : "w6-plate-deep.png", img);
        double r = 0, g = 0, b = 0; int n = 0;
        for (int y = 110; y < 170; y++) for (int x = 200; x < 280; x++) { const Color c = GetImageColor(img, x, y); r += c.r; g += c.g; b += c.b; n++; }
        UnloadImage(img);
        return Vector3{ (float)(r / n), (float)(g / n), (float)(b / n) };
    };
    const Vector3 shallow = patch(0.2f), deep = patch(1.0f);
    const float lumShallow = 0.2126f * shallow.x + 0.7152f * shallow.y + 0.0722f * shallow.z, lumDeep = 0.2126f * deep.x + 0.7152f * deep.y + 0.0722f * deep.z;
    std::printf("[weather] W6 refraction: plate at 0.2 m (%.0f %.0f %.0f), at 1 m (%.0f %.0f %.0f)\n", shallow.x, shallow.y, shallow.z, deep.x, deep.y, deep.z);
    CHECK(lumDeep < 0.985f * lumShallow);
    CHECK(deep.z / std::max(deep.x, 1.f) > 1.1f * shallow.z / std::max(shallow.x, 1.f));
    UnloadImage(on); UnloadImage(off);
}

// (f) Clouds and sky are in the reflection: the sky above the horizon, flipped, is the water below it (compared
// without each row's mean, so the gradient does not count), and not flipped sideways.
static g3d::ScenePtr cloudLake(bool clouds) {
    auto s = w6Scene(w6Water(0), { 0, 1.5f, 0 }, { 0, 1.5f, -100 }, 60);
    s->sky = g3d::Sky::at(12.5f);
    s->sky.sunLight = 0;
    s->lights[0] = g3d::Light::directional(Vector3Scale(Vector3Normalize(s->sky.sunDirection), 100), { 255, 244, 225, 255 }, 2.2f);
    s->clouds.enabled = clouds;
    s->clouds.base = 700; s->clouds.top = 2400; s->clouds.cover = 0.55f;
    s->clouds.quality = g3d::Clouds::Quality::Film;
    s->clouds.shadows = false;
    s->camera.nearPlane = 0.5f;
    s->camera.farPlane = 300000;
    return s;
}
static float rowDetrended(const std::vector<float>& a, const std::vector<float>& b, int w, int h) {
    std::vector<float> A(a), B(b);
    for (int y = 0; y < h; y++) {
        double ma = 0, mb = 0;
        for (int x = 0; x < w; x++) { ma += A[(size_t)y * w + x]; mb += B[(size_t)y * w + x]; }
        ma /= w; mb /= w;
        for (int x = 0; x < w; x++) { A[(size_t)y * w + x] -= (float)ma; B[(size_t)y * w + x] -= (float)mb; }
    }
    double sab = 0, saa = 0, sbb = 0;
    for (size_t i = 0; i < A.size(); i++) { sab += A[i] * B[i]; saa += A[i] * A[i]; sbb += B[i] * B[i]; }
    return (float)(sab / std::sqrt(std::max(saa * sbb, 1e-9)));
}
struct SkyWater { float same, sideways, shifted; };
static SkyWater cloudsInWater(bool clouds) {
    constexpr int W = 480, H = 270, N = 90, SHIFT = 25;
    Image img = look(cloudLake(clouds), W, H, 0);
    dump(clouds ? "w6-cloud-lake.png" : "w6-cloud-lake-off.png", img);
    const std::vector<float> l = lumaOf(img);
    const int mid = H / 2;
    std::vector<float> sky((size_t)W * N), water((size_t)W * N), sideways((size_t)W * N), shifted((size_t)W * N);
    for (int j = 0; j < N; j++)
        for (int x = 0; x < W; x++) {
            sky[(size_t)j * W + x] = l[(size_t)(mid - 8 - j) * W + x];                // j rows above the horizon
            water[(size_t)j * W + x] = l[(size_t)(mid + 8 + j) * W + x];              // j rows below it
            sideways[(size_t)j * W + x] = l[(size_t)(mid + 8 + j) * W + (W - 1 - x)];
            shifted[(size_t)j * W + x] = l[(size_t)(mid + 8 + j + SHIFT) * W + x];
        }
    UnloadImage(img);
    return { rowDetrended(sky, water, W, N), rowDetrended(sky, sideways, W, N), rowDetrended(sky, shifted, W, N) };
}
static void cloudsReflect() {
    const SkyWater on = cloudsInWater(true);
    std::printf("[weather] W6 clouds in the water: sky/water correlation (row means removed) %.2f; flipped sideways %.2f; the water %d rows out of place %.2f\n", on.same, on.sideways, 25, on.shifted);
    CHECK(on.same > 0.75f);
    CHECK(on.same > on.sideways + 0.3f);
    CHECK(on.same > on.shifted + 0.3f);
}


// (i) A calm lake is a mirror. The weather's wind is 1 m/s over 900 m of open water (the calm preset's waves,
// scaled to Hs 1.5 cm), the camera is 2 m up looking along the water at cones standing on a far shore with
// Film clouds above. The reflection of the centre cone must be as tall as the cone (>= 0.85 of its screen
// height), must correlate with the cone flipped upside down (>= 0.8), and the big cloud must show in the water
// (>= 0.6). The same scene with reflections off is the control, and a crop shifted off the cone the second one.
static g3d::ScenePtr calmLake(g3d::Water::Reflections r, float wind, bool clouds) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 2, 0 };
    s->camera.target = { 0, 2, -100 };
    s->camera.fov = 46;
    s->camera.nearPlane = 0.5f;
    s->camera.farPlane = 300000;
    s->sky = g3d::Sky::at(12.5f);
    s->sky.sunLight = 0;
    s->lights.push_back(g3d::Light::directional(Vector3Scale(Vector3Normalize(s->sky.sunDirection), 100), { 255, 244, 225, 255 }, 2.2f));
    s->ambient = 0.35f;
    s->weather = g3d::Weather::clear();
    s->weather.enabled = true;
    s->weather.wind.speed = wind;
    s->weather.clouds.cover = 0.45f;
    s->clouds.enabled = clouds;
    s->clouds.base = 900; s->clouds.top = 1900; s->clouds.cover = 0.45f;
    s->clouds.quality = g3d::Clouds::Quality::Film;
    s->clouds.shadows = false;
    s->water = g3d::Water::calm();
    s->water.fetch = 900;
    s->water.openDepth = 6;
    s->water.reflections = r;
    for (int i = 0; i < 5; i++) {
        g3d::Material m = flat({ 25, 55, 40, 255 });
        s->add(g3d::Geometry::cone(2.2f, 9.f, 16), m)->position = { -40.f + 20.f * i, 4.5f, -100 };
    }
    return s;
}
struct CalmMirror { float span, flip, flipShifted, cloud, cloudSideways; };
static CalmMirror calmMirror(float wind, bool dumpIt) {
    constexpr int W = 960, H = 540;
    Image onC = look(calmLake(g3d::Water::Reflections::Scene, wind, true), W, H, 0);
    Image on = look(calmLake(g3d::Water::Reflections::Scene, wind, false), W, H, 0);
    Image off = look(calmLake(g3d::Water::Reflections::Off, wind, false), W, H, 0);
    if (dumpIt) { dump("w6-calm-lake.png", onC); dump("w6-calm-lake-nocloud.png", on); dump("w6-calm-lake-off.png", off); }
    const float tanHalf = std::tan(23.f * DEG2RAD), pxm = (H * 0.5f) / (100.f * tanHalf);
    const int cx = W / 2, base = (int)std::lround(H * 0.5f + 2.f * pxm), top = (int)std::lround(H * 0.5f - 7.f * pxm), ht = base - top;
    // How far down the reflection reaches: rows below the waterline where it changes the picture, contiguous from the line.
    int last = base;
    for (int y = base + 1; y < base + 2 * ht && y < H; y++) {
        float d = 0;
        for (int x = cx - 1; x <= cx + 1; x++) d += std::fabs(luma(GetImageColor(on, x, y)) - luma(GetImageColor(off, x, y)));
        if (d / 3 < 18) break;
        last = y;
    }
    const std::vector<float> lo = lumaOf(on), lc = lumaOf(onC);
    auto pearson = [&](int shiftX, int shiftY) {
        const int hw = 22;
        std::vector<float> a, b;
        for (int j = 3; j < ht - 3; j++)
            for (int x = cx - hw; x <= cx + hw; x++) {
                a.push_back(lo[(size_t)(base - 1 - j) * W + x]);
                b.push_back(lo[(size_t)(base + 1 + j + shiftY) * W + x + shiftX]);
            }
        double ma = 0, mb = 0;
        for (size_t i = 0; i < a.size(); i++) { ma += a[i]; mb += b[i]; }
        ma /= a.size(); mb /= b.size();
        double sab = 0, saa = 0, sbb = 0;
        for (size_t i = 0; i < a.size(); i++) { sab += (a[i] - ma) * (b[i] - mb); saa += (a[i] - ma) * (a[i] - ma); sbb += (b[i] - mb) * (b[i] - mb); }
        return (float)(sab / std::sqrt(std::max(saa * sbb, 1e-9)));
    };
    CalmMirror m{};
    m.span = (float)(last - base) / ht;
    m.flip = pearson(0, 0);
    m.flipShifted = pearson(0, ht / 2);
    // The cloud: the sky above the horizon against the water below it, row means removed, over the right part of the frame.
    constexpr int N = 120, X0 = 540, X1 = 940, GAP = 6, mid = H / 2;
    std::vector<float> sky, water, side;
    for (int j = 0; j < N; j++)
        for (int x = X0; x < X1; x++) {
            sky.push_back(lc[(size_t)(mid - GAP - j) * W + x]);
            water.push_back(lc[(size_t)(mid + GAP + j) * W + x]);
            side.push_back(lc[(size_t)(mid + GAP + j) * W + (W - 1 - (x - X0) - 20)]);
        }
    m.cloud = rowDetrended(sky, water, X1 - X0, N);
    m.cloudSideways = rowDetrended(sky, side, X1 - X0, N);
    UnloadImage(on); UnloadImage(off); UnloadImage(onC);
    return m;
}
static void calmIsAMirror() {
    const CalmMirror m = calmMirror(1.f, true);
    std::printf("[weather] W6 calm lake (1 m/s): the far cone's reflection spans %.2f of its height; correlation with the cone flipped %.2f (shifted off it %.2f); the cloud in the water %.2f (mirrored sideways %.2f)\n",
                m.span, m.flip, m.flipShifted, m.cloud, m.cloudSideways);
    CHECK(m.span >= 0.85f);
    CHECK(m.flip >= 0.8f);
    CHECK(m.flip > m.flipShifted + 0.3f);
    CHECK(m.cloud >= 0.6f);
    CHECK(m.cloud > m.cloudSideways + 0.3f);
    const CalmMirror rough = calmMirror(8.f, false);
    std::printf("[weather] W6 calm lake, the same at 8 m/s: span %.2f, flip %.2f, cloud %.2f (it must break up)\n", rough.span, rough.flip, rough.cloud);
    CHECK(rough.flip < m.flip - 0.3f);
}

// (j) Refraction has no edge at the bottom of the picture. The camera is 1.6 m over calm clear water looking
// down at 40 degrees on a bright sandy floor 1.5 m deep. Rays under the bottom of the frame leave the picture
// before they meet the floor; they must go on with the colour at the edge, not turn to deep water. Measured on
// row means: no row of the bottom 30% differs from the one above by more than 1.5 levels, and the bottom rows
// stay within 6 levels of the rows at 60% height (Fresnel alone gives +6 without refraction) (the same floor, the same water).
static g3d::ScenePtr sandFloor(bool refract) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 1.6f, 0 };
    s->camera.target = { 0, -0.4f, -2.4f };
    s->camera.fov = 50;
    s->camera.farPlane = 1000;
    s->sky = g3d::Sky::day();
    s->sky.clouds = 0;
    s->lights.push_back(g3d::Light::directional({ 10, 20, 6 }, WHITE, 1.8f));
    s->ambient = 0.6f;
    s->water = g3d::Water::calm();
    s->water.openDepth = 0;
    s->water.caustics = 0;
    s->water.reflections = g3d::Water::Reflections::Scene;
    s->water.refraction = refract;
    g3d::Material sand = flat({ 232, 216, 168, 255 });
    sand.unlit = false;
    s->add(g3d::Geometry::box(200, 0.2f, 200), sand)->position = { 0, -1.6f, -50 };
    return s;
}
static void refractionHasNoEdge() {
    float refMid = 0;
    constexpr int W = 480, H = 270;
    Image img = look(sandFloor(true), W, H, 0);
    dump("w6-sand-floor.png", img);
    auto rowMean = [&](int y) { double s = 0; for (int x = 0; x < W; x++) s += luma(GetImageColor(img, x, y)); return (float)(s / W); };
    float worstStep = 0;
    for (int y = (int)(H * 0.7f); y < H; y++) worstStep = std::max(worstStep, std::fabs(rowMean(y) - rowMean(y - 1)));
    float mid = 0, bottom = 0;
    for (int y = (int)(H * 0.58f); y < (int)(H * 0.62f); y++) mid += rowMean(y) / (int)(H * 0.62f - (int)(H * 0.58f));
    for (int y = H - 12; y < H; y++) bottom += rowMean(y) / 12;
    std::printf("[weather] W6 refraction edge: worst row-to-row step of the row mean over the bottom 30%% %.2f levels; bottom rows %.1f, rows at 60%% height %.1f\n", worstStep, bottom, mid);
{
        Image ref = look(sandFloor(false), W, H, 0);
        dump("w6-sand-floor-straight.png", ref);
        double m = 0, b = 0;
        for (int y = (int)(H * 0.58f); y < (int)(H * 0.62f); y++) for (int x = 0; x < W; x++) m += luma(GetImageColor(ref, x, y)) / (W * ((int)(H * 0.62f) - (int)(H * 0.58f)));
        for (int y = H - 12; y < H; y++) for (int x = 0; x < W; x++) b += luma(GetImageColor(ref, x, y)) / (W * 12.0);
        std::printf("[weather] W6 refraction edge, reference without refraction: bottom rows %.1f, rows at 60%% %.1f\n", b, m);
        refMid = (float)m;
        UnloadImage(ref);
    }
    // Absorbed once: the floor under the refracting surface is as bright as the unrefracted view's (which tints it by the straight
    // path), not darker by a second pass of the same absorption (207 against 223 when it was applied twice).
    CHECK(std::fabs(mid - refMid) < 5.f);
    CHECK(worstStep < 1.5f);
    CHECK(std::fabs(bottom - mid) < 6.f);
    UnloadImage(img);
}

// (k) No lattice. The presets' waves used to sit 210 degrees apart (calm) at lengths in near-simple ratios, which a
// camera 2 m up reads as a grid of dots in the near water (out/w6/w6-calm-near-water-old.png against ...-near-water.png).
// An image statistic did not separate the two (the autocorrelation peak over the median lag was 4.8 for flat water,
// 5.5 for the old preset and 6.5 for the new streaks, a floor set by the sky's gradient), so this checks what the fix
// is: every preset's waves lie within 45 degrees of its amplitude-weighted heading (the old ones: 78 to 140), and the Hs and the longest
// length are the old preset's (the W1 fetch tests rely on them).
struct WaveSpec { float hs, longest, spread, nearestSimple; };
static WaveSpec waveSpec(const g3d::Water& w) {
    double sx = 0, sy = 0, sum = 0;
    float longest = 0;
    for (const g3d::Water::Wave& v : w.waves) {
        sx += std::fabs(v.amplitude) * std::cos(v.direction * DEG2RAD);
        sy += std::fabs(v.amplitude) * std::sin(v.direction * DEG2RAD);
        sum += v.amplitude * v.amplitude / 2;
        longest = std::max(longest, v.wavelength);
    }
    const double heading = std::atan2(sy, sx) * RAD2DEG;
    float spread = 0, nearest = 9;
    for (const g3d::Water::Wave& v : w.waves) {
        float d = std::fabs((float)(v.direction - heading));
        spread = std::max(spread, std::min(d, 360.f - d));
    }
    for (size_t i = 0; i < w.waves.size(); i++)
        for (size_t j = i + 1; j < w.waves.size(); j++) {
            const float r = w.waves[i].wavelength / w.waves[j].wavelength;
            for (int p = 1; p <= 4; p++) for (int q = 1; q <= 4; q++) nearest = std::min(nearest, std::fabs(r / ((float)p / q) - 1));
        }
    return { (float)(4 * std::sqrt(sum)), longest, spread, nearest };
}
static void noLattice() {
    g3d::Water oldCalmW = g3d::Water::calm(), oldChoppy = g3d::Water::choppy(), oldStorm = g3d::Water::storm();
    oldCalmW.waves = { { 20, 7.0f, 0.045f, 1, 1 }, { -35, 3.1f, 0.03f, 1, 1 }, { 75, 1.7f, 0.022f, 1, 1 }, { -58, 1.05f, 0.014f, 1, 1 },
                       { 31, 0.72f, 0.009f, 1, 1 }, { 112, 0.52f, 0.0065f, 1, 1 }, { -17, 0.4f, 0.005f, 1, 1 }, { 152, 0.29f, 0.0035f, 1, 1 } };
    oldChoppy.waves = { { 15, 14.0f, 0.16f, 1.6f, 1 }, { -30, 7.0f, 0.1f, 1.8f, 1 }, { 60, 3.3f, 0.06f, 1.5f, 1 }, { -75, 1.8f, 0.035f, 1, 1 }, { 5, 0.9f, 0.02f, 1, 1 }, { 120, 0.5f, 0.01f, 1, 1 } };
    oldStorm.waves = { { 10, 34.0f, 0.7f, 2.4f, 1 }, { -25, 17.0f, 0.42f, 2.6f, 1 }, { 40, 9.0f, 0.24f, 2.2f, 1 }, { -60, 4.5f, 0.12f, 1.8f, 1 }, { 80, 2.3f, 0.06f, 1.4f, 1 }, { -5, 1.2f, 0.03f, 1, 1 } };
    const struct { const char* name; g3d::Water now, old; } sets[] = { { "calm", g3d::Water::calm(), oldCalmW }, { "choppy", g3d::Water::choppy(), oldChoppy }, { "storm", g3d::Water::storm(), oldStorm } };
    for (const auto& s : sets) {
        const WaveSpec n = waveSpec(s.now), o = waveSpec(s.old);
        std::printf("[weather] W6 %s preset: Hs %.4f (was %.4f), longest %.2f m (was %.2f), directions within %.0f degrees of the heading (was %.0f), nearest length ratio to a simple one %.0f%% (was %.0f%%)\n",
                    s.name, n.hs, o.hs, n.longest, o.longest, n.spread, o.spread);
        CHECK(n.spread <= 45.f && o.spread > 45.f);
        CHECK(std::fabs(n.hs - o.hs) < 1e-5f && std::fabs(n.longest - o.longest) < 1e-5f);
    }
    // Pictures of both, for the eye: the near water of a 1 m/s lake.
    for (int i = 0; i < 2; i++) {
        auto sc = calmLake(g3d::Water::Reflections::Off, 1.f, false);
        sc->water = i == 0 ? sets[0].now : sets[0].old;
        sc->water.fetch = 900; sc->water.openDepth = 6;
        Image img = look(sc, 960, 540, 0);
        dump(i == 0 ? "w6-calm-near-water.png" : "w6-calm-near-water-old.png", img);
        UnloadImage(img);
    }
}

// (l) No rings on a rough sea. A choppy sea at 6 m/s under Film clouds from 1.2 m up: with reflections on, the
// reflected cloud outlines used to be thrown about by the facets into thin closed contours (out/w6/open-sea-grazing.png
// of the previous round). The water's thin high-contrast detail (the share of water pixels more than 14 levels off their
// 9 x 9 mean) with reflections on must stay within 1.3 times what the same sea has with them off, plus 0.3 points (7.6% against 6.4% now; 9.8% without the blur that follows the bend).
static g3d::ScenePtr roughSea(g3d::Water::Reflections r) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 1.2f, 0 };
    s->camera.target = { 0, 2.2f, -100 };
    s->camera.fov = 50;
    s->camera.nearPlane = 0.5f;
    s->camera.farPlane = 300000;
    s->sky = g3d::Sky::at(20.f);
    s->sky.sunLight = 0;
    s->lights.push_back(g3d::Light::directional({ 50, 30, -100 }, WHITE, 2.2f));
    s->ambient = 0.3f;
    s->weather = g3d::Weather::clear();
    s->weather.enabled = true;
    s->weather.wind.speed = 6;
    s->weather.wind.direction = 90;
    s->weather.clouds.cover = 0.5f;
    s->clouds.enabled = true;
    s->clouds.base = 900; s->clouds.top = 2200;
    s->clouds.quality = g3d::Clouds::Quality::Film;
    s->clouds.shadows = false;
    s->water = g3d::Water::choppy();
    s->water.fetch = 20000;
    s->water.reflections = r;
    return s;
}
static float thinDetail(const Image& img, int y0) {
    const int W = img.width, H = img.height, R = 4;
    const std::vector<float> l = lumaOf(img);
    long n = 0, hit = 0;
    for (int y = y0; y < H - R; y++)
        for (int x = R; x < W - R; x++) {
            double m = 0;
            for (int j = -R; j <= R; j++) for (int i = -R; i <= R; i++) m += l[(size_t)(y + j) * W + x + i];
            m /= (2 * R + 1) * (2 * R + 1);
            hit += std::fabs(l[(size_t)y * W + x] - m) > 14;
            n++;
        }
    return 100.f * (float)hit / n;
}
static void noRings() {
    constexpr int W = 960, H = 540;
    Image on = look(roughSea(g3d::Water::Reflections::Scene), W, H, 0), off = look(roughSea(g3d::Water::Reflections::Off), W, H, 0);
    dump("w6-rough-sea-on.png", on); dump("w6-rough-sea-off.png", off);
    const float a = thinDetail(on, H / 2 + 20), b = thinDetail(off, H / 2 + 20);
    std::printf("[weather] W6 rough sea: share of water pixels with thin detail, reflections on %.2f%%, off %.2f%%\n", a, b);
    CHECK(a < 1.3f * b + 0.3f);
    UnloadImage(on); UnloadImage(off);
}
// (g) Purity: a frame alone is the frame after others, with the reflection pass on, clouds, waves and a moving pole.
static g3d::ScenePtr movingLake() {
    auto s = cloudLake(true);
    s->water = w6Water(1);
    s->water.refraction = true;
    s->water.openDepth = 0;
    g3d::Object3D* pole = s->add(g3d::Geometry::box(1.2f, 1.4f, 0.3f), flat({ 215, 25, 25, 255 }));
    pole->position = { 0, 0.7f, -6 };
    pole->up = [](int t, g3d::Object3D& o) { o.position.x = -2.f + 0.12f * t; };
    return s;
}
static void waterPurity() {
    constexpr int W = 320, H = 180;
    Image alone = look(movingLake(), W, H, 10);
    Comp c = comp(movingLake(), W, H);
    Image x = render(c, 3), y = render(c, 20), z = render(c, 10), again = render(c, 10);
    const long long d1 = diffImages(alone, z), d2 = diffImages(z, again);
    std::printf("[weather] W6 purity: frame 10 alone against after frames 3 and 20: difference %lld (%lld after itself); frames 3 and 20 differ by %lld\n", d1, d2, diffImages(x, y));
    CHECK(d1 == 0 && d2 == 0);
    CHECK(diffImages(x, y) > 20000);
    UnloadImage(alone); UnloadImage(x); UnloadImage(y); UnloadImage(z); UnloadImage(again);
}

// (h) No shimmer far away: a bright far shore reflected in short waves, at a grazing view. One frame later the
// water near the camera has changed, and the far water, whose waves are shorter than a pixel, must not have
// changed more (its normal is the band-limited one).
static void noShimmer() {
    constexpr int W = 480, H = 270;
    auto make = [&](int) {
        g3d::Water w = w6Water(0);
        w.waves.clear();
        const float len[] = { 1.6f, 1.2f, 0.9f, 0.7f, 0.5f, 0.4f, 0.3f }, dir[] = { 80, 110, 65, 95, 120, 70, 100 };
        for (int i = 0; i < 7; i++) w.waves.push_back({ dir[i], len[i], 0.02f * len[i], 1, 1 });
        auto s = w6Scene(w, { 0, 1.3f, 0 }, { 0, 1.1f, -40 }, 50);
        s->add(g3d::Geometry::box(400, 8, 2), flat({ 245, 245, 235, 255 }))->position = { 0, 4, -160 };
        s->add(g3d::Geometry::box(400, 6, 2), flat({ 30, 60, 30, 255 }))->position = { 0, 3, -140 };
        for (int i = -3; i <= 3; i++) s->add(g3d::Geometry::box(0.4f, 1.6f, 0.4f), flat({ 230, 230, 230, 255 }))->position = { i * 1.3f, 0.8f, -5.5f - i * 0.4f };
        return s;
    };
    g3d::ScenePtr s = make(0);
    Comp c = comp(s, W, H);
    Image a = render(c, 10), b = render(c, 11);
    dump("w6-shimmer-a.png", a);
    const int horizon = (int)std::lround(project(s->camera, { 0, 0, -1e5f }, W, H).y);
    auto change = [&](int y0, int y1) {
        double sum = 0; int cells = 0;
        for (int cy = y0; cy + 12 <= y1; cy += 12)
            for (int cx = 0; cx + 12 <= W; cx += 12) {
                double d = 0;
                for (int y = cy; y < cy + 12; y++) for (int x = cx; x < cx + 12; x++) d += std::fabs(luma(GetImageColor(a, x, y)) - luma(GetImageColor(b, x, y)));
                sum += d / 144; cells++;
            }
        return (float)(sum / std::max(cells, 1));
    };
    const float far = change(horizon + 3, horizon + 27), nearC = change(H - 60, H);
    std::printf("[weather] W6 shimmer: mean per-cell change between frames 10 and 11, far water %.3f, near water %.3f (horizon row %d)\n", far, nearC, horizon);
    CHECK(far <= nearC);
    UnloadImage(a); UnloadImage(b);
}

// Cost at 1080p (RMTN_W6_TIMING=1): a lake with 600 trees (1 200 draws) and a jetty, with and without Film clouds, for each setting.
static g3d::ScenePtr timingLake(g3d::Water::Reflections r, bool refract, bool clouds, int trees) {
    auto s = cloudLake(clouds);
    s->water = w6Water(1, 0);
    s->water.reflections = r;
    s->water.refraction = refract;
    s->camera.position = { 0, 2.0f, 30 };
    s->camera.target = { 0, 3, -100 };
    g3d::Material bark = flat({ 90, 60, 40, 255 }), leaf = flat({ 40, 110, 50, 255 });
    bark.unlit = leaf.unlit = false;
    for (int i = 0; i < trees; i++) {
        const float a = rnd01(i, 1) * 6.2831f, d = 60 + rnd01(i, 2) * 60, h = 5 + rnd01(i, 3) * 6;
        const float x = std::cos(a) * d * 1.8f, z = -std::fabs(std::sin(a)) * d - 30;
        s->add(g3d::Geometry::cylinder(0.3f, h * 0.4f, 8), bark)->position = { x, h * 0.2f, z };
        s->add(g3d::Geometry::cone(2.0f, h, 12), leaf)->position = { x, h * 0.4f + h * 0.5f, z };
    }
    return s;
}
static void waterTiming() {
    constexpr int W = 1920, H = 1080;
    struct Row { const char* name; g3d::Water::Reflections r; bool refract; };
    const Row rows[] = { { "off", g3d::Water::Reflections::Off, false }, { "quarter", g3d::Water::Reflections::Quarter, false },
                         { "half", g3d::Water::Reflections::Half, false }, { "full", g3d::Water::Reflections::Scene, false },
                         { "refraction only", g3d::Water::Reflections::Off, true }, { "full + refraction", g3d::Water::Reflections::Scene, true } };
    for (int clouds = 0; clouds < 3; clouds++)
        for (const Row& row : rows) {
            const int trees = clouds == 2 ? 20 : 600;
            Comp c = comp(timingLake(row.r, row.refract, clouds == 1, trees), W, H);
            Canvas canvas;
            canvas.load(W, H, 1);
            presentFrame(c, 0, canvas);
            Image warm = LoadImageFromTexture(canvas.output().texture);
            UnloadImage(warm);
            double best = 1e9, mean = 0;
            for (int i = 0; i < 10; i++) {
                const double t0 = GetTime();
                presentFrame(c, i, canvas);
                Image img = LoadImageFromTexture(canvas.output().texture);
                const double dt = (GetTime() - t0) * 1000;
                UnloadImage(img);
                best = std::min(best, dt);
                mean += dt / 10;
            }
            canvas.unload();
            std::printf("[weather] W6 timing 1080p, %s, %s: best %.1f ms, mean %.1f ms\n", clouds == 1 ? "600 trees and Film clouds" : clouds == 2 ? "40 draws, no clouds" : "600 trees (1200 draws), no clouds", row.name, best, mean);
        }
}

static void waterReflections() {
    offMeansOffWater();
    poleUpsideDown();
    wavesBreakIt();
    noLeak();
    refraction();
    cloudsReflect();
    calmIsAMirror();
    refractionHasNoEdge();
    noLattice();
    noRings();
    waterPurity();
    noShimmer();
}

// ---------------------------------------------------------------- W7: lakes, rivers and pools (g3d::WaterBody)
// A top-down orthographic camera over (cx, cz): `height` metres of the world from the top of the picture to the
// bottom, +x to the right, -z up.
static g3d::Camera topDown(float cx, float cz, float height) {
    g3d::Camera c;
    c.position = { cx, 30, cz };
    c.target = { cx, 0, cz };
    c.upVector = { 0, 0, -1 };
    c.orthographic = true;
    c.orthoHeight = height;
    return c;
}
static Vector2 topPx(const g3d::Camera& c, float x, float z, int w, int h) {
    const float perM = (float)h / c.orthoHeight;
    return { w * 0.5f + (x - c.target.x) * perM, h * 0.5f + (z - c.target.z) * perM };
}
static g3d::ScenePtr bodyWorld(const g3d::Camera& cam) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera = cam;
    s->skyColor = { 150, 190, 235, 255 };
    s->groundColor = { 90, 100, 90, 255 };
    s->ambient = 0.6f;
    s->lights.push_back(g3d::Light::directional({ 10, 14, 6 }, WHITE, 1.4f));
    return s;
}
static bool inPoly(const std::vector<Vector2>& p, float x, float y) {
    bool in = false;
    for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++)
        if ((p[i].y > y) != (p[j].y > y) && x < (p[j].x - p[i].x) * (y - p[i].y) / (p[j].y - p[i].y) + p[i].x) in = !in;
    return in;
}
static float distToPoly(const std::vector<Vector2>& p, float x, float y) {
    float best = 1e30f;
    for (size_t i = 0; i < p.size(); i++) {
        const Vector2 a = p[i], b = p[(i + 1) % p.size()], ab = Vector2Subtract(b, a), ap = Vector2Subtract({ x, y }, a);
        const float t = std::clamp(Vector2DotProduct(ap, ab) / std::max(Vector2DotProduct(ab, ab), 1e-9f), 0.f, 1.f);
        best = std::min(best, Vector2Distance({ x, y }, Vector2Add(a, Vector2Scale(ab, t))));
    }
    return best;
}
static int pixelDiff(Color a, Color b) { return std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b); }

// (a) A pool in a box: an L-shaped pool over a sand floor. Water inside the outline, none outside: the picture with
// the pool minus the picture without it changes inside (3 px in from the edge) and nowhere outside (3 px out).
// A pool drawn as its bounding box would also change the notch.
static std::vector<Vector2> lPool() { return { { -4, -2.5f }, { 4, -2.5f }, { 4, 0 }, { 0, 0 }, { 0, 2.5f }, { -4, 2.5f } }; }
static g3d::ScenePtr poolScene(bool water) {
    auto s = bodyWorld(topDown(0, 0, 9));
    s->add(g3d::Geometry::plane(40, 40), flat({ 170, 150, 110, 255 }))->position = { 0, -1, 0 };
    if (water) {
        g3d::WaterBody b = g3d::WaterBody::lake(lPool(), 0);
        b.look.reflections = g3d::Water::Reflections::Off;
        b.shoreFoam = 0;
        s->waterBodies.push_back(b);
    }
    return s;
}
static void poolInBox() {
    constexpr int W = 320, H = 180;
    auto on = poolScene(true), off = poolScene(false);
    Image a = look(on, W, H, 3), b = look(off, W, H, 3);
    dump("w7-pool.png", a);
    std::vector<Vector2> px;
    for (Vector2 p : lPool()) px.push_back(topPx(on->camera, p.x, p.y, W, H));
    int inN = 0, inHit = 0, outN = 0, outHit = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const float d = distToPoly(px, x + 0.5f, y + 0.5f);
            if (d <= 3) continue;
            const int diff = pixelDiff(GetImageColor(a, x, y), GetImageColor(b, x, y));
            if (inPoly(px, x + 0.5f, y + 0.5f)) { inN++; inHit += diff > 12; }
            else { outN++; outHit += diff > 0; }
        }
    std::printf("[weather] W7 pool in a box: %d of %d pixels inside change, %d of %d outside change\n", inHit, inN, outHit, outN);
    CHECK(inN > 5000 && outN > 5000);
    CHECK(inHit >= inN * 95 / 100);
    CHECK(outHit <= 3);
    UnloadImage(a); UnloadImage(b);
}

// A bed that rises at 1:4 towards +x, through the level at x = 2, under a lake whose polygon runs far past the shore.
// `wind` < 0: the body's own authored waves, rough (energy 1). Otherwise calm-lake waves under that weather wind (m/s) over
// a 2 km fetch, so the foam follows the wind.
static g3d::ScenePtr shoreScene(const g3d::Camera& cam, float foam, float soft, float foamWidth, float wind = -1) {
    auto s = bodyWorld(cam);
    g3d::Object3D* bed = s->add(g3d::Geometry::plane(40, 20), flat({ 80, 70, 55, 255 }));
    bed->position = { 2, 0, 0 };
    bed->rotation = { 0, 0, std::atan(0.25f) * RAD2DEG };
    g3d::WaterBody b = g3d::WaterBody::lake({ { -9, -6 }, { 7, -6 }, { 7, 6 }, { -9, 6 } }, 0);
    if (wind == -2) b.look = g3d::Water{};
    else if (wind < 0) b.look = w6Water(2);
    else {
        b.look = g3d::Water::calm();
        b.fetch = 2000;
        s->weather = g3d::Weather::clear();
        s->weather.enabled = true;
        s->weather.wind.speed = wind;
    }
    b.look.foam = 0; b.look.caustics = 0; b.look.sunGlint = 0;
    b.look.reflections = g3d::Water::Reflections::Off;
    b.shoreFoam = foam; b.edgeSoft = soft; b.foamWidth = foamWidth;
    s->waterBodies.push_back(b);
    return s;
}
// How much white a shoreline picture has over the same one without shore foam: per pixel the share of the way from
// the foam-off colour to white (0 none, 1 solid). Top-down, so columns map to the bed's known depth.
static std::vector<float> foamAlpha(const Image& on, const Image& off) {
    std::vector<float> a((size_t)on.width * on.height);
    for (int y = 0; y < on.height; y++)
        for (int x = 0; x < on.width; x++) {
            const float lo = luma(GetImageColor(off, x, y)), ln = luma(GetImageColor(on, x, y));
            a[(size_t)y * on.width + x] = std::clamp((ln - lo) / std::max(245.f - lo, 1.f), 0.f, 1.f);
        }
    return a;
}
// (b) Shore foam only in the shallows. Seen from straight above, so the depth is the vertical one; the bed is known
// (depth = (2 - x) / 4 metres). None where the depth passes 1.3 foam widths; some in the first 0.8; and a calm shore's
// foam stays translucent (alpha under 0.6) while it is white, not tan.
static void foamInTheShallows() {
    constexpr int W = 320, H = 180;
    const float fw = 0.5f;
    auto s = shoreScene(topDown(-1.5f, 0, 8), 1, 0.3f, fw);
    auto bare = shoreScene(topDown(-1.5f, 0, 8), 0, 0.3f, fw);
    Image img = look(s, W, H, 6), none = look(bare, W, H, 6);
    dump("w7-shore-foam.png", img);
    const std::vector<float> al = foamAlpha(img, none);
    auto region = [&](float x0, float x1, float* mx) {
        double sum = 0; int n = 0; *mx = 0;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                const float wx = s->camera.target.x + (x + 0.5f - W * 0.5f) * s->camera.orthoHeight / H;
                if (wx < x0 || wx > x1) continue;
                n++; sum += al[(size_t)y * W + x]; *mx = std::max(*mx, al[(size_t)y * W + x]);
            }
        return n ? (float)(sum / n) : 0.f;
    };
    float mxDeep, mxShallow;
    const float deep = region(-8, 2 - 1.3f * fw * 4, &mxDeep), shallow = region(2 - 0.8f * fw * 4, 1.95f, &mxShallow);
    std::printf("[weather] W7 shore foam: mean foam alpha %.4f where the depth is over 1.3 foam widths (max %.3f), %.3f in the first 0.8 (max %.2f)\n", deep, mxDeep, shallow, mxShallow);
    CHECK(mxDeep < 0.01f);
    CHECK(shallow > 0.04f);
    UnloadImage(img); UnloadImage(none);
}

// Foam follows the wave energy: the same calm lake under a 1 m/s and an 8 m/s wind. The calm one has under a quarter
// of the foam (summed alpha), and none of its foam is more than 60 % opaque.
static void foamFollowsTheWind() {
    constexpr int W = 320, H = 180;
    float mass[2] = { 0, 0 }, peak[2] = { 0, 0 };
    for (int k = 0; k < 2; k++) {
        const float wind = k ? 8.f : 1.f;
        auto s = shoreScene(topDown(-1.5f, 0, 8), 1, 0.3f, 0.5f, wind), bare = shoreScene(topDown(-1.5f, 0, 8), 0, 0.3f, 0.5f, wind);
        Image on = look(s, W, H, 6), off = look(bare, W, H, 6);
        if (k == 0) dump("w7-shore-calm.png", on); else dump("w7-shore-wind.png", on);
        const std::vector<float> al = foamAlpha(on, off);
        for (float a : al) { mass[k] += a; peak[k] = std::max(peak[k], a); }
        UnloadImage(on); UnloadImage(off);
    }
    std::printf("[weather] W7 foam and wind: summed foam alpha %.1f at 1 m/s (peak %.2f), %.1f at 8 m/s (peak %.2f)\n", mass[0], peak[0], mass[1], peak[1]);
    CHECK(mass[1] > 50);
    CHECK(mass[0] < 0.25f * mass[1]);
    CHECK(peak[0] <= 0.6f);
}

// (c) The edge is soft. From a low angle the surface mirrors the bright sky, so a hard waterline would step from
// bed to mirror; down a column across the shoreline no pixel steps from the next by more than a bound.
static float shoreStep() {
    constexpr int W = 320, H = 180;
    g3d::Camera cam;
    cam.position = { -8, 1.0f, 0 };
    cam.target = { 2, 0, 0 };
    cam.fov = 40;
    auto s = shoreScene(cam, 0, 0.3f, 0.4f, -2);
    Image img = look(s, W, H, 6);
    dump("w7-shore-soft.png", img);
    const int r0 = (int)std::lround(project(cam, { 2, 0, 0 }, W, H).y);
    int worst = 0;
    for (int y = std::max(r0 - 30, 1); y < std::min(r0 + 25, H); y++)
        worst = std::max(worst, pixelDiff(GetImageColor(img, W / 2, y), GetImageColor(img, W / 2, y - 1)));
    std::printf("[weather] W7 soft edge: largest step between pixels near the shoreline (row %d): %d\n", r0, worst);
    UnloadImage(img);
    return (float)worst;
}
static void softEdge() { CHECK(shoreStep() < 40); }

// A straight river along +x seen from above, its ripples drawn as the surface's slope in colour (Ground::debug 2).
static g3d::ScenePtr riverWorld(float speed) {
    auto s = bodyWorld(topDown(0, 0, 4.5f));
    s->add(g3d::Geometry::plane(60, 30), flat({ 90, 80, 60, 255 }))->position = { 0, -2, 0 };
    g3d::WaterBody r = g3d::WaterBody::river({ { -14, 0 }, { -5, 0 }, { 5, 0 }, { 14, 0 } }, 6, 0, speed);
    r.look.reflections = g3d::Water::Reflections::Off;
    r.shoreFoam = 0;
    s->waterBodies.push_back(r);
    s->ground.debug = 2;
    return s;
}
// Red channel of a crop, minus its mean.
static std::vector<float> cropOf(const Image& img, int x0, int x1, int y0, int y1) {
    std::vector<float> v;
    double m = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) { v.push_back((float)GetImageColor(img, x, y).r); m += v.back(); }
    m /= std::max<size_t>(v.size(), 1);
    for (float& f : v) f -= (float)m;
    return v;
}
// How many pixels the pattern of `b` lies along +x of the pattern of `a`: the peak of their correlation over
// shifts, refined by a parabola through the peak and its neighbours.
static float patternShift(const Image& a, const Image& b, int maxShift, float* peak = nullptr) {
    const int y0 = a.height / 2 - 40, y1 = a.height / 2 + 40, xa = 50, xb = a.width - 50;
    const std::vector<float> A = cropOf(a, xa, xb, y0, y1);
    std::vector<double> corr;
    for (int s = -maxShift; s <= maxShift; s++) {
        const std::vector<float> B = cropOf(b, xa + s, xb + s, y0, y1);
        double ab = 0, aa = 0, bb = 0;
        for (size_t i = 0; i < A.size(); i++) { ab += (double)A[i] * B[i]; aa += (double)A[i] * A[i]; bb += (double)B[i] * B[i]; }
        corr.push_back(ab / std::sqrt(std::max(aa * bb, 1e-9)));
    }
    size_t k = (size_t)(std::max_element(corr.begin(), corr.end()) - corr.begin());
    if (peak) *peak = (float)corr[k];
    float sub = 0;
    if (k > 0 && k + 1 < corr.size()) {
        const double l = corr[k - 1], c = corr[k], r = corr[k + 1], den = l - 2 * c + r;
        if (std::fabs(den) > 1e-12) sub = (float)(0.5 * (l - r) / den);
    }
    return (float)k - maxShift + sub;
}
// (d) The ripples move downstream at the flow speed: between two frames 8/30 s apart, the pattern has moved
// speed * dt metres along +x, within 15 %.
static void riverFlows() {
    constexpr int W = 320, H = 180;
    const float speed = 1.2f, dt = 8.f / 30.f, perM = H / 4.5f;
    auto s = riverWorld(speed);
    Image a = look(s, W, H, 6), b = look(s, W, H, 14);
    dump("w7-river-debug.png", a);
    float peak = 0;
    const float shift = patternShift(a, b, 30, &peak);
    const float measured = shift / perM / dt;
    std::printf("[weather] W7 river flow: pattern moved %.2f px = %.3f m/s against %.2f m/s (correlation %.2f)\n", shift, measured, speed, peak);
    CHECK(peak > 0.5f);
    CHECK(measured > speed * 0.85f && measured < speed * 1.15f);
    UnloadImage(a); UnloadImage(b);
}
// (e) No reset seam: the change between consecutive frames over 10 s. Pairs that straddle the flow map's phase
// switches (every half cycle: a layer's weight is zero there) against pairs in the middle of the cycles: the
// largest change over the median change stays under a bound.
static float seamRatio() {
    constexpr int W = 160, H = 90;
    auto s = riverWorld(1.2f);
    auto change = [&](int f) {
        Image a = look(s, W, H, f), b = look(s, W, H, f + 1);
        double d = 0; int n = 0;
        for (int y = H / 2 - 20; y < H / 2 + 20; y++)
            for (int x = 20; x < W - 20; x++) { d += std::abs(GetImageColor(a, x, y).r - GetImageColor(b, x, y).r); n++; }
        UnloadImage(a); UnloadImage(b);
        return (float)(d / n);
    };
    std::vector<float> all;
    float boundary = 0;
    for (int j = 1; j <= 19; j++) {   // every half cycle: both layers' reset points
        const float c = change(15 * j - 1);
        boundary = std::max(boundary, c);
        all.push_back(c);
        all.push_back(change(15 * j + 6));
    }
    std::sort(all.begin(), all.end());
    const float median = all[all.size() / 2], ratio = boundary / std::max(median, 1e-3f);
    std::printf("[weather] W7 flow-map seam: change per frame, median %.3f, largest at a phase switch %.3f, ratio %.2f\n", median, boundary, ratio);
    return ratio;
}
static void noResetSeam() { CHECK(seamRatio() < 1.5f); }

// (f) A city water polygon sits at its level. A CityJSON lake at height 1.5 over a bed that rises at 1:4 through zero
// at x = 2 meets the bed at x = 8; the last column where the picture differs from the one without the lake is there.
static const char* cityLakeJson() {
    return R"({"type":"CityJSON","version":"2.0","transform":{"scale":[0.5,0.5,0.5],"translate":[0,0,0]},
      "CityObjects":{"lake":{"type":"WaterBody","geometry":[{"type":"MultiSurface","lod":"1","boundaries":[[[0,1,2,3]]],
      "semantics":{"surfaces":[{"type":"WaterSurface"}],"values":[0]}}]},
      "ground":{"type":"TINRelief","geometry":[{"type":"MultiSurface","lod":"1","boundaries":[[[4,5,6]]]}]}},
      "vertices":[[-80,-80,3],[80,-80,3],[80,80,3],[-80,80,3],[-80,-80,0],[-70,-80,0],[-70,-70,0]]})";
}
static g3d::ScenePtr cityLake(bool realWater, bool lake) {
    auto s = bodyWorld(topDown(3, 0, 8));
    g3d::Object3D* bed = s->add(g3d::Geometry::plane(40, 20), flat({ 80, 70, 55, 255 }));
    bed->position = { 2, 0, 0 };
    bed->rotation = { 0, 0, std::atan(0.25f) * RAD2DEG };
    if (lake) {
        city::Model m = city::parse(cityLakeJson());
        city::Look lk;
        lk.realWater = realWater;
        city::add(*s, m, lk);
        for (g3d::WaterBody& b : s->waterBodies) {
            b.look.reflections = g3d::Water::Reflections::Off;
            b.look.absorb = { 3, 3, 3 };
            b.edgeSoft = 0.02f; b.shoreFoam = 0;
        }
    }
    return s;
}
static float lastWaterX(Image& with, Image& without, const g3d::Camera& cam) {
    const int W = with.width, H = with.height;
    float edge = -1e9f;
    for (int x = 0; x < W; x++) {
        double d = 0;
        for (int y = H / 2 - 30; y < H / 2 + 30; y++) d += pixelDiff(GetImageColor(with, x, y), GetImageColor(without, x, y));
        if (d / 60 > 8) edge = cam.target.x + (x + 0.5f - W * 0.5f) * cam.orthoHeight / H;
    }
    return edge;
}
static void cityWaterLevel() {
    constexpr int W = 320, H = 180;
    auto on = cityLake(true, true), base = cityLake(true, false), off = cityLake(false, true);
    CHECK(on->waterBodies.size() == 1);
    CHECK(off->waterBodies.empty());
    Image a = look(on, W, H, 3), b = look(base, W, H, 3), c = look(off, W, H, 3);
    dump("w7-city-lake.png", a);
    const float edge = lastWaterX(a, b, on->camera);
    std::printf("[weather] W7 city water: level %.2f, shoreline at x %.2f (expected 8.00)\n", on->waterBodies.empty() ? -1.f : on->waterBodies[0].level, edge);
    CHECK(!on->waterBodies.empty() && std::fabs(on->waterBodies[0].level - 1.5f) < 1e-4f);
    CHECK(std::fabs(edge - 8.f) < 0.4f);
    // Off: the water stays a face of the mesh, as before (the same picture as the model added through city::mesh).
    auto manual = bodyWorld(topDown(3, 0, 8));
    g3d::Object3D* bed = manual->add(g3d::Geometry::plane(40, 20), flat({ 80, 70, 55, 255 }));
    bed->position = { 2, 0, 0 };
    bed->rotation = { 0, 0, std::atan(0.25f) * RAD2DEG };
    city::Model m = city::parse(cityLakeJson());
    g3d::Material mat;
    mat.doubleSided = true;
    manual->add(city::mesh(m, city::Look{}), mat);
    Image d = look(manual, W, H, 3);
    const long long same = diffImages(c, d), differs = diffImages(a, c);
    std::printf("[weather] W7 city water off: %lld against the plain mesh; on against off: %lld\n", same, differs);
    CHECK(same == 0);
    CHECK(differs > 20000);
    UnloadImage(a); UnloadImage(b); UnloadImage(c); UnloadImage(d);
}

// (g) Scenes without bodies are unchanged: bodies that cannot be seen (behind the camera, or with no shape) leave
// the sea's picture as it was, pixel for pixel.
static void noBodiesNoChange() {
    constexpr int W = 320, H = 180;
    auto plain = poleLake(1, 1, true);
    auto with = poleLake(1, 1, true);
    g3d::WaterBody behind = g3d::WaterBody::lake({ { -5, 30 }, { 5, 30 }, { 5, 40 }, { -5, 40 } }, 0);
    with->waterBodies.push_back(behind);
    with->waterBodies.push_back(g3d::WaterBody{});
    with->waterBodies.push_back(g3d::WaterBody::river({ { 0, 20 } }, 3, 0, 1));
    Image a = look(plain, W, H, 4), b = look(with, W, H, 4);
    const long long d = diffImages(a, b);
    std::printf("[weather] W7 no visible bodies: difference %lld\n", d);
    CHECK(d == 0);
    UnloadImage(a); UnloadImage(b);
}

// (h) Purity: a frame on its own equals the frame after others.
static void bodiesPure() {
    constexpr int W = 240, H = 135;
    auto s = riverWorld(1.2f);
    s->ground.debug = 0;
    s->waterBodies.push_back(g3d::WaterBody::lake({ { -3, -3 }, { -1, -3 }, { -1, -1 }, { -3, -1 } }, 0));
    Comp c = comp(s, W, H);
    Image alone = render(c, 21);
    for (int f : { 3, 40, 12 }) UnloadImage(render(c, f));
    Image after = render(c, 21);
    Comp fresh = comp(s, W, H);
    Image first = render(fresh, 21);
    const long long d1 = diffImages(alone, after), d2 = diffImages(alone, first);
    std::printf("[weather] W7 purity: %lld after other frames, %lld on a fresh composition\n", d1, d2);
    CHECK(d1 == 0 && d2 == 0);
    UnloadImage(alone); UnloadImage(after); UnloadImage(first);
}

// (i) A lake reflects the scene: a red pole at its shore shows red below its base in the water, which is the
// planar reflection pass shared by a body (one pass, at the level of the first reflecting body); with reflections
// off, it does not.
static g3d::ScenePtr poleBody(bool reflect) {
    auto s = w6Scene(g3d::Water{}, { 0, 1.5f, 9 }, { 0, 1.5f, 0 });
    g3d::WaterBody b = g3d::WaterBody::lake({ { -40, -40 }, { 40, -40 }, { 40, 20 }, { -40, 20 } }, 0);
    b.look = w6Water(0);
    b.look.refraction = false;
    b.look.reflections = reflect ? g3d::Water::Reflections::Scene : g3d::Water::Reflections::Off;
    s->waterBodies.push_back(b);
    s->add(g3d::Geometry::cylinder(0.12f, 1.2f), flat({ 215, 25, 25, 255 }))->position = { 0, 0.6f, 0 };
    return s;
}
static void lakeReflects() {
    constexpr int W = 480, H = 270;
    auto on = poleBody(true), off = poleBody(false);
    Image a = look(on, W, H, 3), b = look(off, W, H, 3);
    dump("w7-lake-pole.png", a);
    const int x = (int)std::lround(project(on->camera, { 0, 0, 0 }, W, H).x), baseRow = (int)std::lround(project(on->camera, { 0, 0, 0 }, W, H).y);
    float red = 0;
    for (int y = baseRow + 4; y < baseRow + 24; y++) red += addedAt(a, b, x, y, 2).x / 20;
    std::printf("[weather] W7 lake reflection: red added under the pole %.1f levels\n", red);
    CHECK(red > 40);
    UnloadImage(a); UnloadImage(b);
}

// (j) A lake mirrors the hill behind it. Striped hill strips rise from the far shore; the picture of the lake must
// carry the stripes, upside down below the shoreline: the column profile of the lake picture correlates with the
// profile of a picture of the mirrored hill (no water), over the rows the mirrored hill covers.
static g3d::ScenePtr hillScene(bool real, bool lake) {
    auto s = w6Scene(g3d::Water{}, { 0, 1.2f, 9 }, { 0, 1.0f, -5 }, 50);
    s->sky.clouds = 0;
    const float k = 0.5f, th = std::atan(k), cs = std::cos(th), sn = std::sin(th);
    for (int i = 0; i < 8; i++) {
        const float tc = 1.f + 2.f * i;
        g3d::Object3D* o = s->add(g3d::Geometry::plane(60, 2), flat(i % 2 ? Color{ 40, 120, 40, 255 } : Color{ 190, 220, 120, 255 }));
        o->material.doubleSided = !real;
        o->position = { 0, (real ? 1.f : -1.f) * tc * sn, -5 - tc * cs };
        o->rotation = { (real ? 1.f : -1.f) * th * RAD2DEG, 0, 0 };
    }
    if (lake) {
        g3d::WaterBody b = g3d::WaterBody::lake({ { -30, -5 }, { 30, -5 }, { 30, 40 }, { -30, 40 } }, 0);
        b.look = w6Water(0);
        b.look.refraction = false;
        b.shoreFoam = 0;
        s->waterBodies.push_back(b);
    }
    return s;
}
static float hillReflection() {
    constexpr int W = 480, H = 270;
    auto on = hillScene(true, true), truth = hillScene(false, false);
    Image a = look(on, W, H, 3), b = look(truth, W, H, 3);
    dump("w7-hill-lake.png", a);
    dump("w7-hill-truth.png", b);
    const float th = std::atan(0.5f);
    const int r0 = (int)std::lround(project(on->camera, { 0, 0, -5 }, W, H).y) + 4;
    const int r1 = (int)std::lround(project(on->camera, { 0, -16 * std::sin(th), -5 - 16 * std::cos(th) }, W, H).y) - 4;
    std::vector<float> pa, pb;
    for (int y = r0; y < std::min(r1, H); y++) {
        float sa = 0, sb = 0;
        for (int x = W / 3; x < 2 * W / 3; x++) { sa += luma(GetImageColor(a, x, y)); sb += luma(GetImageColor(b, x, y)); }
        pa.push_back(sa); pb.push_back(sb);
    }
    for (size_t i = 0; i + 1 < pa.size(); i++) { pa[i] = pa[i + 1] - pa[i]; pb[i] = pb[i + 1] - pb[i]; }      // the stripes, not the slow gradient under them
    if (!pa.empty()) { pa.pop_back(); pb.pop_back(); }
    auto mean = [](const std::vector<float>& v) { double m = 0; for (float f : v) m += f; return m / std::max<size_t>(v.size(), 1); };
    const double ma = mean(pa), mb = mean(pb);
    double ab = 0, aa = 0, bb = 0;
    for (size_t i = 0; i < pa.size(); i++) { ab += (pa[i] - ma) * (pb[i] - mb); aa += (pa[i] - ma) * (pa[i] - ma); bb += (pb[i] - mb) * (pb[i] - mb); }
    const float corr = (float)(ab / std::sqrt(std::max(aa * bb, 1e-9)));
    std::printf("[weather] W7 hill in the lake: rows %d..%d, correlation with the mirrored hill %.2f\n", r0, r1, corr);
    UnloadImage(a); UnloadImage(b);
    return pa.size() > 20 ? corr : 0.f;
}
static void hillInTheLake() { CHECK(hillReflection() >= 0.9f); }

// (k) No stripes on a lake: the calm preset under a light wind over a lake's own small fetch, seen near (from above) and
// far (grazing), at 960 x 540. In windows 240 wide the luma is averaged along x, detrended, and its strongest spectral
// line over periods of 3 to 16 rows is compared with the median of the band: one dominant wavelength stripes the
// water and shows as a spike (the look before the waves were broken up measured 22 and 26; now 13 and 8).
static g3d::ScenePtr lakeLook(bool near) {
    g3d::Camera cam;
    cam.position = near ? Vector3{ -8, 5.5f, 26.5f } : Vector3{ 0, 1.6f, 28 };
    cam.target = near ? Vector3{ -5, 1.25f, -14 } : Vector3{ 0, 0.5f, -10 };
    cam.fov = 50;
    cam.farPlane = 400;
    auto s = bodyWorld(cam);
    s->sky = g3d::Sky::day();
    s->sky.clouds = 0;
    s->weather = g3d::Weather::clear();
    s->weather.enabled = true;
    s->weather.wind.speed = 4.5f;
    s->weather.wind.direction = 70;
    g3d::WaterBody b = g3d::WaterBody::lake({ { -30, -40 }, { 30, -40 }, { 30, 40 }, { -30, 40 } }, 0);
    b.look = g3d::Water::calm();
    b.look.reflections = g3d::Water::Reflections::Scene;
    b.look.refraction = true;
    b.shoreFoam = 0;
    s->waterBodies.push_back(b);
    s->add(g3d::Geometry::plane(300, 300), flat({ 60, 90, 50, 255 }))->position = { 0, -1.5f, 0 };
    return s;
}
// Strongest spectral line over the median of the band, for the rows of a window: the luma is averaged along x (stripes
// of a regular wave run across), the slow trend taken out, and the power of periods of 3 to 16 rows compared.
static float stripePeak(const std::vector<float>& l, int W, int x0, int x1, int y0, int rows) {
    std::vector<double> p((size_t)rows);
    for (int y = 0; y < rows; y++) { double s = 0; for (int x = x0; x < x1; x++) s += l[(size_t)(y0 + y) * W + x]; p[(size_t)y] = s / (x1 - x0); }
    std::vector<double> d((size_t)rows);
    for (int y = 0; y < rows; y++) { double s = 0; int n = 0; for (int k = -8; k <= 8; k++) if (y + k >= 0 && y + k < rows) { s += p[(size_t)(y + k)]; n++; } d[(size_t)y] = p[(size_t)y] - s / n; }
    std::vector<double> pw;
    for (double per = 3; per <= 16; per += 0.25) {
        double re = 0, im = 0;
        for (int y = 0; y < rows; y++) { const double w = 0.5 - 0.5 * std::cos(2 * PI * (y + 0.5) / rows), a = 2 * PI * y / per; re += w * d[(size_t)y] * std::cos(a); im += w * d[(size_t)y] * std::sin(a); }
        pw.push_back(re * re + im * im);
    }
    std::vector<double> s = pw;
    std::sort(s.begin(), s.end());
    return (float)(*std::max_element(pw.begin(), pw.end()) / std::max(s[s.size() / 2], 1e-9));
}
static float lakePeriodicity(bool near) {
    constexpr int W = 960, H = 540;
    Image img = look(lakeLook(near), W, H, 5);
    dump(near ? "w7-lake-near.png" : "w7-lake-far.png", img);
    const std::vector<float> l = lumaOf(img);
    float worst = 0;
    for (int y0 : near ? std::vector<int>{ 280, 400 } : std::vector<int>{ 280, 370 })
        for (int x0 : { 60, 360, 660 }) {
            const float p = stripePeak(l, W, x0, x0 + 240, y0, 128);
            std::printf("  window %d,%d: %.1f\n", x0, y0, p);
            worst = std::max(worst, p);
        }
    UnloadImage(img);
    return worst;
}
static void lakeNoLattice() {
    const float n = lakePeriodicity(true), f = lakePeriodicity(false);
    std::printf("[weather] W7 lake stripes: strongest spectral line over the band median, near %.1f, far %.1f\n", n, f);
    CHECK(n < 16.0f && f < 16.0f);
}

// (l) A sea at level 2 is the sea at level 0 lifted by two metres: the shader decides above or below the water from
// the surface height, which used to count the level twice (so a camera a metre above a level-2 sea was taken to be
// under it). The picture of a pole standing in a level-2 sea, seen from a metre above it, equals the level-0 one
// with everything lowered by 2, and its waterline sits at the level.
static g3d::ScenePtr liftedSea(float level) {
    g3d::Water w = w6Water(1);
    w.level = level;
    auto s = w6Scene(w, { 0, level + 1.0f, 9 }, { 0, level + 0.4f, 0 }, 40);
    s->add(g3d::Geometry::cylinder(0.12f, 1.2f), flat({ 215, 25, 25, 255 }))->position = { 0, level + 0.6f, 0 };
    return s;
}
static void seaAtLevelTwo() {
    constexpr int W = 320, H = 180;
    auto hi = liftedSea(2), lo = liftedSea(0);
    Image a = look(hi, W, H, 3), b = look(lo, W, H, 3);
    dump("w7-sea-level2.png", a);
    const long long d = diffImages(a, b);
    const int row = (int)std::lround(project(hi->camera, { 0, 2, 0 }, W, H).y), x = (int)std::lround(project(hi->camera, { 0, 2, 0 }, W, H).x);
    float redAbove = 0, redBelow = 0;
    for (int y = row - 12; y < row - 2; y++) redAbove += redness(GetImageColor(a, x, y)) / 10;
    for (int y = row + 2; y < row + 12; y++) redBelow += redness(GetImageColor(a, x, y)) / 10;
    std::printf("[weather] W7 sea at level 2: %lld differing against level 0 lowered; pole redness %.0f above the waterline row %d, %.0f below\n", d, redAbove, row, redBelow);
    CHECK(d < 2000);
    CHECK(redAbove > 100 && redBelow < redAbove * 0.5f);
    UnloadImage(a); UnloadImage(b);
}

// (m) A pool reads as clear water over a light floor: a striped floor 1.2 m under the surface, seen at 45 degrees,
// shows through the water. The luma of the water pixels correlates with the render of the floor alone (>= 0.6), and
// what the water adds (the residual of a linear fit of one on the other) has no step between neighbouring rows or
// columns of more than a bound: no saturated wedge, no hard-edged patch. A negative control, the same pool with an
// opaque absorbing water, must not show the floor (correlation under 0.3): the measure sees a "pale flat sheet".
static g3d::ScenePtr clearPool(bool water, float absorb) {
    g3d::Camera cam;
    cam.position = { 0, 6.5f, 6.5f };
    cam.target = { 0, 0, 0 };
    cam.fov = 40;
    auto s = bodyWorld(cam);
    // The floor seen through water appears about 0.45 m nearer the camera than where the straight ray would meet it
    // (1.2 m deep, 45 degrees): the picture without water gets its stripes moved to match.
    for (int i = 0; i < 80; i++) {       // strips 0.1 m wide whose brightness is a sine of period 1.5 m: a smooth pattern
        const float z = -4.0f + 0.1f * i + 0.05f, v = 0.5f + 0.5f * std::sin(2 * PI * z / 1.5f);
        g3d::Object3D* o = s->add(g3d::Geometry::plane(8, 0.1f), flat({ (unsigned char)(110 + 120 * v), (unsigned char)(140 + 95 * v), (unsigned char)(150 + 85 * v), 255 }));
        o->position = { 0, -1.2f, z - (water ? 0.f : 0.45f) };
    }
    if (water) {
        g3d::WaterBody b = g3d::WaterBody::lake({ { -3, -3 }, { 3, -3 }, { 3, 3 }, { -3, 3 } }, 0);
        b.look = g3d::Water::calm();
        b.look.deep = { 40, 140, 160, 255 };
        b.look.absorb = { absorb, absorb * 0.2f, absorb * 0.12f };
        b.look.reflections = g3d::Water::Reflections::Off;
        b.look.refraction = true;
        b.shoreFoam = 0;
        s->waterBodies.push_back(b);
    }
    return s;
}
static void poolFloorShows() {
    constexpr int W = 480, H = 270;
    auto truthS = clearPool(false, 0);
    Image truth = look(truthS, W, H, 3);
    float corr[2] = { 0, 0 }, stepRow = 0, stepCol = 0;
    for (int k = 0; k < 2; k++) {
        auto s = clearPool(true, k == 0 ? 0.45f : 40.f);
        Image a = look(s, W, H, 3);
        if (k == 0) dump("w7-clear-pool.png", a);
        std::vector<float> la, lt, rr;
        std::vector<int> px, py;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                // inside the pool, a margin in from its edge: the projected corners of the 6 m square at the level
                const Vector2 p0 = project(s->camera, { -3, 0, -3 }, W, H), p1 = project(s->camera, { 3, 0, 3 }, W, H);
                const Vector2 q0 = project(s->camera, { 3, 0, -3 }, W, H), q1 = project(s->camera, { -3, 0, 3 }, W, H);
                (void)q0; (void)q1;
                const float fy = (y - p0.y) / (p1.y - p0.y);
                if (fy < 0.15f || fy > 0.85f) continue;
                const float wx = (x - W * 0.5f);
                const float half = (1 - fy) * std::fabs(q0.x - W * 0.5f) + fy * std::fabs(q1.x - W * 0.5f);
                if (std::fabs(wx) > half * 0.8f) continue;
                la.push_back(luma(GetImageColor(a, x, y))); lt.push_back(luma(GetImageColor(truth, x, y))); px.push_back(x); py.push_back(y);
            }
        double ma = 0, mt = 0;
        for (size_t i = 0; i < la.size(); i++) { ma += la[i]; mt += lt[i]; }
        ma /= std::max<size_t>(la.size(), 1); mt /= std::max<size_t>(la.size(), 1);
        double aa = 0, tt = 0, at = 0;
        for (size_t i = 0; i < la.size(); i++) { aa += (la[i] - ma) * (la[i] - ma); tt += (lt[i] - mt) * (lt[i] - mt); at += (la[i] - ma) * (lt[i] - mt); }
        corr[k] = (float)(at / std::sqrt(std::max(aa * tt, 1e-9)));
        {
            const double slope = at / std::max(tt, 1e-9);
            std::map<int, std::pair<double, int>> rows, cols;
            for (size_t i = 0; i < la.size(); i++) {
                const double r = la[i] - ma - slope * (lt[i] - mt);
                rows[py[i]].first += r; rows[py[i]].second++; cols[px[i]].first += r; cols[px[i]].second++;
            }
            auto worst = [](std::map<int, std::pair<double, int>>& m) {
                float w = 0; double prev = 0; int pk = -100;
                for (auto& kv : m) { const double v = kv.second.first / kv.second.second; if (kv.first == pk + 1) w = std::max(w, (float)std::fabs(v - prev)); prev = v; pk = kv.first; }
                return w;
            };
            stepRow = worst(rows); stepCol = worst(cols);
        }
        UnloadImage(a);
    }
    UnloadImage(truth);
    std::printf("[weather] W7 clear pool: floor correlation %.2f (opaque control %.2f), residual step per row %.1f, per column %.1f\n", corr[0], corr[1], stepRow, stepCol);
    CHECK(corr[0] >= 0.6f);
    CHECK(corr[1] < 0.3f);
    CHECK(stepRow < 20.f && stepCol < 20.f);
}

static void waterBodies() {
    poolInBox();
    foamInTheShallows();
    foamFollowsTheWind();
    softEdge();
    riverFlows();
    noResetSeam();
    cityWaterLevel();
    noBodiesNoChange();
    bodiesPure();
    lakeReflects();
    hillInTheLake();
    lakeNoLattice();
    seaAtLevelTwo();
    poolFloorShows();
}

// ---------------------------------------------------------------- W8: wakes, ripples and floating objects
// A boat on a straight line along +x at speed U (deep water, no waves of its own to hide the V).
static g3d::Wake straightBoat(float U, float life = 40) {
    g3d::Wake w;
    w.path = { { -400, 0 }, { 400, 0 } };
    w.speed = U; w.start = 0; w.life = life;
    return w;
}
// The half angle of a wake's V, in degrees, `behind` metres behind the boat (a CPU field): the outer edge of the
// envelope of the slope, where it has fallen to `edge` of its peak, found along the lateral line.
static float vEdge(const g3d::WakeField& f, float boatX, float behind, float edge, float* peakAt = nullptr, float* peakSlope = nullptr) {
    std::vector<float> env;
    const float dz = 0.1f, zMax = 0.9f * behind;
    for (float z = 0; z < zMax; z += dz) {
        float m = 0;
        for (float x = boatX - behind - 3; x <= boatX - behind + 3; x += 0.25f) {
            const Vector2 g = f.slope(x, z);
            m = std::max(m, std::hypot(g.x, g.y));
        }
        env.push_back(m);
    }
    size_t pk = 0;
    for (size_t i = 0; i < env.size(); i++) if (env[i] > env[pk]) pk = i;
    // the cusp is the outermost strong maximum of the envelope across the wake (the transverse crests are nearer the track)
    const float top = env[pk];
    for (size_t i = 3; i + 3 < env.size(); i++)
        if (env[i] > 0.3f * top && env[i] >= env[i - 3] && env[i] >= env[i + 3] && i > pk) pk = i;
    size_t o = pk;
    while (o + 1 < env.size() && env[o] > edge * env[pk]) o++;
    if (peakAt) *peakAt = std::atan2((float)pk * dz, behind) * RAD2DEG;
    if (peakSlope) *peakSlope = env[pk];
    return std::atan2((float)o * dz, behind) * RAD2DEG;
}
// A big flat pool seen from straight above (orthographic, +x right, +z down), drawing the surface's slope as colour
// (ground.debug 2: pixel = 0.5 + 2 slope on red and green) when `slopeView`, or the lit water otherwise.
static g3d::ScenePtr wakeWorld(const g3d::Camera& cam, bool slopeView) {
    auto s = bodyWorld(cam);
    g3d::WaterBody b = g3d::WaterBody::lake({ { -500, -500 }, { 500, -500 }, { 500, 500 }, { -500, 500 } }, 0);
    b.look.waves.clear();
    b.look.reflections = g3d::Water::Reflections::Off;
    b.look.foam = 0;
    b.shoreFoam = 0;
    s->waterBodies.push_back(b);
    if (slopeView) s->ground.debug = 2;
    return s;
}
static Vector2 slopeOfPixel(const Image& img, int x, int y) {
    const Color c = GetImageColor(img, x, y);
    return { (c.r / 255.f - 0.5f) / 2.f, (c.g / 255.f - 0.5f) / 2.f };
}
// A boat on the x axis that is at x = `at` at second 0.
static g3d::Wake boatAt(float U, float at, float life = 40) {
    g3d::Wake w = straightBoat(U, life);
    w.start = -(at + 400.f) / U;
    return w;
}

// A lit oblique view for counting feathers: orthographic, 45 degrees down, looking towards -z, the sun a little off the
// mirror direction so the crests' faces glint. World (x, z) to pixel for it: x = W/2 + (x - cx) perM, y = H/2 + (z - cz) sin(el) perM.
struct FeatherView { g3d::ScenePtr scene; float cx, cz, perM, sinEl; int w, h; };
static FeatherView featherView(const g3d::Wake* wake, float cx, float cz, float height, int w, int h) {
    const float el = 20 * DEG2RAD;
    g3d::Camera c;
    c.target = { cx, 0, cz };
    c.position = { cx, 60 * std::sin(el), cz + 60 * std::cos(el) };
    c.orthographic = true; c.orthoHeight = height; c.nearPlane = 1; c.farPlane = 300;
    auto s = wakeWorld(c, false);
    s->sky.enabled = true; s->sky.show = true; s->sky.sunDirection = { 0.5f, 0.35f, -0.7f };
    if (wake) s->wakes.push_back(*wake);
    return { s, cx, cz, h / height, std::sin(el), w, h };
}
// Crests crossed along the line from the bow back along the V's arm, `inside` metres towards the track: the line
// runs 3 boat lengths (21 m), its luma minus a 3 m running mean crossing up through +thr after dipping under -thr.
static int armCrests(const FeatherView& v, const Image& img, float bowX, float side, float length, float inside, float thr, float* spread = nullptr) {
    const float ang = 19.5f * DEG2RAD, step = 0.1f;
    std::vector<float> L;
    for (float s = 2; s < 2 + 3 * length; s += step) {
        const float x = bowX - s * std::cos(ang) + 0, z = side * (s * std::sin(ang) - inside);
        const int px = (int)(v.w * 0.5f + (x - v.cx) * v.perM), py = (int)(v.h * 0.5f + (z - v.cz) * v.sinEl * v.perM);
        if (px < 1 || py < 1 || px >= v.w - 1 || py >= v.h - 1) { L.push_back(NAN); continue; }
        L.push_back(luma(GetImageColor(img, px, py)));
    }
    const int half = (int)(1.5f / step);
    int crests = 0; bool low = false;
    float lo = 1e9f, hi = -1e9f;
    for (size_t i = 0; i < L.size(); i++) {
        if (std::isnan(L[i])) continue;
        float m = 0; int n = 0;
        for (int j = (int)i - half; j <= (int)i + half; j++) if (j >= 0 && j < (int)L.size() && !std::isnan(L[j])) { m += L[j]; n++; }
        const float d = L[i] - m / n;
        lo = std::min(lo, d); hi = std::max(hi, d);
        if (d < -thr) low = true;
        else if (d > thr && low) { crests++; low = false; }
    }
    if (spread) *spread = hi - lo;
    return crests;
}
static void w8Images() {
    constexpr int W = 960, H = 540;
    for (float U : { 3.f, 4.f, 6.5f }) {                           // crests in the CPU field along the arm
        g3d::Wake w = boatAt(U, 20); w.length = 7; w.beam = 2.2f;
        const g3d::WakeField f(w, 0);
        for (float in : { 1.5f, 3.f, 4.f, 5.f }) {
            std::vector<float> h;
            for (float s = 2; s < 23; s += 0.1f) h.push_back(f.height(23.5f - s * std::cos(19.5f * DEG2RAD), (s * std::sin(19.5f * DEG2RAD) - in)));
            int c = 0; float mx = 0;
            for (size_t i = 1; i + 1 < h.size(); i++) { if (h[i] > h[i - 1] && h[i] >= h[i + 1] && h[i] > 0.01f) c++; mx = std::max(mx, std::fabs(h[i])); }
            std::printf("[probe] CPU height crests U %.1f inside %.1f: %d maxima over 0.01 m, peak %.3f\n", U, in, c, mx);
        }
    }
    for (float U : { 2.f, 2.5f, 3.f }) {                           // feathers along the arms (lit oblique, ortho)
        g3d::Wake w = boatAt(U, 20); w.length = 7; w.beam = 2.2f; w.foam = 0;
        FeatherView fv = featherView(&w, 8, 0, 18, 960, 540);
        Image im = look(fv.scene, 960, 540, 0);
        char name[64]; std::snprintf(name, sizeof name, "w8-feathers-%.1f.png", U);
        dump(name, im);
        for (float in : { 1.5f, 3.f, 4.f, 5.f }) {
            float sp; const int n1 = armCrests(fv, im, 20 + 3.5f, 1, 7, in, 3.f, &sp), n2 = armCrests(fv, im, 20 + 3.5f, -1, 7, in, 3.f);
            std::printf("[probe] feathers U %.1f inside %.1f m: %d and %d crests over 21 m (luma spread %.1f)\n", U, in, n1, n2, sp);
        }
        UnloadImage(im);
    }
    {                                                             // the example's wake, near the jetty (frame 300)
    {                                                             // lit top-down: sky reflection, sun to the side, with foam
        auto sc = wakeWorld(topDown(-30, 0, 60), false);
        sc->sky.enabled = true; sc->sky.show = true; sc->sky.sunDirection = { 0.5f, 0.35f, -0.7f };
        g3d::Wake w = boatAt(6.5f, 20); w.length = 7; w.beam = 2.2f;
        sc->wakes.push_back(w);
        Image im = look(sc, W, H, 0);
        dump("w8-lit-topdown.png", im);
        UnloadImage(im);
    }
    {                                                             // lit look: sky reflection, sun off to one side, no foam
        for (int on = 0; on < 2; on++) {
            g3d::Camera oc; oc.position = { -8, 7, 26 }; oc.target = { -8, 0, -2 }; oc.fov = 45;
            auto sc = wakeWorld(oc, false);
            sc->sky.enabled = true; sc->sky.show = true; sc->sky.sunDirection = { 0.5f, 0.35f, -0.7f }; sc->sky.sunLight = -1;
            if (on) { g3d::Wake w = boatAt(6.5f, 20); w.length = 7; w.beam = 2.2f; w.foam = 0; sc->wakes.push_back(w); }
            Image im = look(sc, W, H, 0);
            dump(on ? "w8-lit-on.png" : "w8-lit-off.png", im);
            UnloadImage(im);
        }
    }
        g3d::Wake w; w.path = { { 46, -1.5f }, { 26, -5 }, { 4, -9 }, { -16, -10.5f }, { -30, -7.5f }, { -46, -3 } };
        w.speed = 6.5f; w.start = 0.4f; w.length = 7; w.beam = 2.2f; w.strength = 1; w.life = 34; w.trail = 10;
        for (int v = 0; v < 2; v++) {
            auto sc = wakeWorld(topDown(-4, -11, 14), v == 1);
            sc->wakes.push_back(w);
            Image im = look(sc, W, H, 300);
            dump(v ? "w8-ex-slope.png" : "w8-ex-lit.png", im);
            UnloadImage(im);
        }
        const g3d::WakeField f(w, 10.0);
        float mx = 0, mh = 0;
        for (float x = -30; x < 10; x += 0.2f) for (float z = -25; z < 5; z += 0.2f) { const Vector2 g = f.slope(x, z); mx = std::max(mx, std::hypot(g.x, g.y)); mh = std::max(mh, std::fabs(f.height(x, z))); }
        std::printf("[probe] example wake at 10 s, strength 1: peak slope %.3f, peak height %.3f m, amplitude %.3f\n", mx, mh, w.amplitude());
    }
    const float U = 4;
    auto cam = topDown(-40, 0, 80);
    auto s = wakeWorld(cam, true);
    s->wakes.push_back(boatAt(U, 20));
    Image img = look(s, W, H, 0);
    dump("w8-slope.png", img);
    const float perM = H / cam.orthoHeight, foot = 1.f / perM;
    const g3d::WakeField f(s->wakes[0], 0);
    double sum = 0, ref = 0;
    int n = 0;
    for (int y = 0; y < H; y += 3)
        for (int x = 0; x < W; x += 3) {
            const float wx = cam.target.x + (x + 0.5f - W * 0.5f) / perM, wz = cam.target.z + (y + 0.5f - H * 0.5f) / perM;
            const Vector2 g = f.slope(wx, wz, foot), p = slopeOfPixel(img, x, y);
            sum += std::fabs(g.x - p.x) + std::fabs(g.y - p.y);
            ref += std::fabs(g.x) + std::fabs(g.y);
            n++;
        }
    std::printf("[probe] GPU vs CPU slope: mean abs diff %.5f against mean abs slope %.5f\n", sum / n, ref / n);
    UnloadImage(img);
    // The beauty view.
    auto lit = wakeWorld(cam, false);
    lit->wakes.push_back(boatAt(U, 20));
    Image b = look(lit, W, H, 0);
    dump("w8-wake-top.png", b);
    UnloadImage(b);
    for (int zoomed = 0; zoomed < 2; zoomed++) {                 // the CPU field as a grey height map, for a look at the waves
        const float cx = zoomed ? 6.f : -20.f, hgt = zoomed ? 18.f : 50.f;
        const g3d::Wake w = boatAt(U, 20);
        const g3d::WakeField fc(w, 0);
        Image h = GenImageColor(W, H, BLACK);
        const float pm = H / hgt;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                const float v = fc.height(cx + (x + 0.5f - W * 0.5f) / pm, (y + 0.5f - H * 0.5f) / pm, 1.f / pm);
                const unsigned char g = (unsigned char)std::clamp(128.f + v * 500.f, 0.f, 255.f);
                ImageDrawPixel(&h, x, y, Color{ g, g, g, 255 });
            }
        dump(zoomed ? "w8-cpu-height-close.png" : "w8-cpu-height.png", h);
        UnloadImage(h);
    }
    {                                                             // an oblique view
        g3d::Camera oc;
        oc.position = { 8, 2.4f, 14 }; oc.target = { -6, 0.2f, 0 }; oc.fov = 50;
        auto os = wakeWorld(oc, false);
        os->wakes.push_back(boatAt(U, 6));
        Image oi = look(os, W, H, 0);
        dump("w8-wake-oblique.png", oi);
        UnloadImage(oi);
    }
    for (int v = 0; v < 2; v++) {
        auto c2 = wakeWorld(topDown(6, 0, 18), v == 1);
        c2->wakes.push_back(boatAt(U, 20));
        Image cl = look(c2, W, H, 0);
        dump(v ? "w8-wake-close-slope.png" : "w8-wake-close.png", cl);
        UnloadImage(cl);
    }
    // Ripple radius against time (CPU).
    g3d::Ripple r;
    for (float T : { 1.f, 2.f, 3.f, 5.f, 8.f }) {
        float best = 0, bestR = 0;
        for (float rad = 0.02f; rad < 3; rad += 0.005f) {
            const float v = std::fabs(r.slope(rad, T));
            if (v > best) { best = v; bestR = rad; }
        }
        std::printf("[probe] ripple T %.0f: peak slope %.4f at r %.3f (%.3f m/s), group speed of the peak wave %.3f\n", T, best, bestR, bestR / T, g3d::Ripple::groupSpeed(r.peakWave()));
    }
    // Euler read-back.
    const Vector3 want = Vector3Normalize({ 1, 0.3f, 0.2f });
    const Vector3 rot = g3d::lookRotation(want, { 0, 1, 0 });
    const Vector3 got = Vector3Transform({ 0, 0, 1 }, MatrixRotateXYZ(Vector3Scale(rot, DEG2RAD)));
    std::printf("[probe] lookRotation: want %.3f %.3f %.3f, MatrixRotateXYZ gives %.3f %.3f %.3f\n", want.x, want.y, want.z, got.x, got.y, got.z);
}
// The cusp's half angle (degrees) of a wake at `U` m/s from the CPU twin: the slope of the V's outer edge (the outer
// 66% of the envelope's peak: the Airy edge) against the distance behind the boat, between 30 and 60 m, which drops the
// apex's offset.
static float cuspAngle(float U, float* near30 = nullptr, float* far60 = nullptr) {
    const g3d::Wake w = boatAt(U, 20, 70);
    const g3d::WakeField f(w, 0);
    const float a30 = vEdge(f, 20, 40, 0.66f), a60 = vEdge(f, 20, 80, 0.66f);
    if (near30) *near30 = a30;
    if (far60) *far60 = a60;
    const float z30 = 40 * std::tan(a30 * DEG2RAD), z60 = 80 * std::tan(a60 * DEG2RAD);
    (void)z30; (void)z60;
    return a60;                                   // the edge line's slope is too noisy at one decimetre of resolution; far enough behind, the apex offset is a degree or less
}
// The radius (m) of the strongest ring along each of `n` directions in a slope-view image of a ripple at the centre of
// an orthographic top view `height` metres high, searched in [lo, hi] m.
static std::vector<float> ringRadii(const Image& img, float height, int n, float lo, float hi) {
    const float perM = img.height / height;
    std::vector<float> out;
    for (int d = 0; d < n; d++) {
        const float a = 2 * PI * (d + 0.5f) / n;
        float best = -1, at = 0;
        for (float r = lo; r < hi; r += 0.5f / perM) {
            const Vector2 g = slopeOfPixel(img, (int)(img.width * 0.5f + std::cos(a) * r * perM), (int)(img.height * 0.5f + std::sin(a) * r * perM));
            const float m = std::hypot(g.x, g.y);
            if (m > best) { best = m; at = r; }
        }
        out.push_back(at);
    }
    return out;
}
static void waterWakes() {
    // (a) and (b): the V is 19.5 degrees (Kelvin) at any speed.
    for (float U : { 4.f, 7.f }) {
        float n30, n60;
        const float ang = cuspAngle(U, &n30, &n60);
        std::printf("[weather] Kelvin wake at %.0f m/s: outer edge %.1f deg at 40 m and %.1f deg at 80 m behind, V half angle %.2f deg at 80 m (want 19.5 +- 2)\n", U, n30, n60, ang);
        CHECK(std::fabs(ang - 19.5f) < 2.0f);
    }
    {
        const float a4 = cuspAngle(4), a7 = cuspAngle(7);
        std::printf("[weather] the V at 4 and 7 m/s differs by %.2f deg (speed independent: under 1.5)\n", std::fabs(a4 - a7));
        CHECK(std::fabs(a4 - a7) < 1.5f);
    }
    // The GPU draws what the CPU twin says (slope view, top down), and nothing lies ahead of the bow.
    {
        constexpr int W = 960, H = 540;
        auto cam = topDown(-40, 0, 80);
        auto s = wakeWorld(cam, true);
        s->wakes.push_back(boatAt(4, 20));
        Image img = look(s, W, H, 0);
        const float perM = H / cam.orthoHeight, foot = 1.f / perM;
        const g3d::WakeField f(s->wakes[0], 0);
        double sum = 0, ref = 0, ahead = 0;
        const Vector2 bias = slopeOfPixel(img, W - 1, 0);          // the 8-bit zero: 128 is not quite one half
        int n = 0;
        for (int y = 0; y < H; y += 3)
            for (int x = 0; x < W; x += 3) {
                const float wx = cam.target.x + (x + 0.5f - W * 0.5f) / perM, wz = cam.target.z + (y + 0.5f - H * 0.5f) / perM;
                const Vector2 g = f.slope(wx, wz, foot), p = slopeOfPixel(img, x, y);
                sum += std::fabs(g.x - p.x) + std::fabs(g.y - p.y);
                ref += std::fabs(g.x) + std::fabs(g.y);
                if (wx > 24.f) ahead += std::fabs(p.x - bias.x) + std::fabs(p.y - bias.y);
                n++;
            }
        UnloadImage(img);
        std::printf("[weather] wake on the GPU vs the CPU twin: mean abs slope difference %.5f against mean abs slope %.5f; ahead of the bow %.6f\n", sum / n, ref / n, ahead);
        CHECK(sum < 0.3 * ref);
        CHECK(ahead < 0.01 * ref * n);
    }
    // The propwash is a trail about the beam wide with translucent, streaky foam that fades (GPU, top down, foam on against off).
    {
        constexpr int W = 960, H = 540;
        const float U = 6.5f, beam = 2.2f;
        auto cam = topDown(-10, 0, 60);
        const float perM = H / cam.orthoHeight;
        Image img[2];
        for (int on = 0; on < 2; on++) {
            auto s = wakeWorld(cam, false);
            g3d::Wake w = boatAt(U, 20); w.length = 7; w.beam = beam; w.life = 34; w.trail = 10; w.foam = (float)on;
            s->wakes.push_back(w);
            img[on] = look(s, W, H, 0);
        }
        float top = 0;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) top = std::max(top, luma(GetImageColor(img[1], x, y)));
        auto alphaAt = [&](float wx, float wz) {
            const int px = (int)(W * 0.5f + (wx - cam.target.x) * perM), py = (int)(H * 0.5f + wz * perM);
            const float lo = luma(GetImageColor(img[0], px, py)), hi = luma(GetImageColor(img[1], px, py));
            return std::clamp((hi - lo) / std::max(top - lo, 1.f), 0.f, 1.f);
        };
        float a30 = 0;                                     // strongest foam 28..32 m behind the bow (x = -8..-12), across the track
        for (float x = -12; x <= -8; x += 0.25f) for (float z = -6; z <= 6; z += 0.1f) a30 = std::max(a30, alphaAt(x, z));
        float a3 = 0;                                       // 3 m behind the stern, for scale
        for (float x = 7; x <= 12; x += 0.25f) for (float z = -1; z <= 1; z += 0.1f) a3 = std::max(a3, alphaAt(x, z));
        float width = 0;                                    // widest extent over 0.1 alpha, 9..11 m behind the bow
        for (float x = 9; x <= 11; x += 0.5f) {
            float lo = 1e9f, hi = -1e9f;
            for (float z = -12; z <= 12; z += 0.05f) if (alphaAt(x, z) > 0.1f) { lo = std::min(lo, z); hi = std::max(hi, z); }
            if (hi > lo) width = std::max(width, hi - lo);
        }
        dump("w8-trail.png", img[1]);
        std::printf("[weather] propwash: foam alpha %.2f near the stern, %.2f at 30 m behind (want under 0.25); width over 0.1 alpha at 10 m behind %.2f m = %.1f beams (want under 2.5)\n", a3, a30, width, width / beam);
        CHECK(a3 > 0.2f);
        CHECK(a30 < 0.25f);
        CHECK(width < 2.5f * beam);
        UnloadImage(img[0]); UnloadImage(img[1]);
    }
    // The V shows in a lit view: sky reflection, the sun off to the side, no foam. The change the wake makes along columns across the arms.
    {
        constexpr int W = 960, H = 540;
        auto lit = [&](float strength) {
            g3d::Camera oc; oc.position = { -8, 7, 26 }; oc.target = { -8, 0, -2 }; oc.fov = 45;
            auto sc = wakeWorld(oc, false);
            sc->sky.enabled = true; sc->sky.show = true; sc->sky.sunDirection = { 0.5f, 0.35f, -0.7f };
            if (strength > 0) { g3d::Wake w = boatAt(6.5f, 20); w.length = 7; w.beam = 2.2f; w.foam = 0; w.strength = strength; sc->wakes.push_back(w); }
            return look(sc, W, H, 0);
        };
        Image off = lit(0), on = lit(1), weak = lit(0.05f), none = lit(0);
        auto contrast = [&](const Image& a) {                 // mean over columns of the biggest change from the plain water
            double sum = 0; int n = 0;
            for (int x = 100; x < 700; x += 4) {
                float m = 0;
                for (int y = 160; y < 480; y++) m = std::max(m, std::fabs(luma(GetImageColor(a, x, y)) - luma(GetImageColor(off, x, y))));
                sum += m; n++;
            }
            return (float)(sum / n);
        };
        const float cOn = contrast(on), cWeak = contrast(weak), cNone = contrast(none);
        std::printf("[weather] lit V: biggest luma change from the plain water along columns, mean %.2f levels with the wake, %.2f with a wake a twentieth as strong, %.2f without (want over 8)\n", cOn, cWeak, cNone);
        CHECK(cOn > 8.f);
        CHECK(cNone == 0.f);
        UnloadImage(off); UnloadImage(on); UnloadImage(weak); UnloadImage(none);
    }
    // Feathers: several short crests stacked along each arm of the V of a boat at displacement speed (2 m/s, 7 m long), counted
    // in the luma of a lit oblique view along a line 2 m inside the arm, over 3 boat lengths, on both sides.
    {
        g3d::Wake w = boatAt(2.f, 20); w.length = 7; w.beam = 2.2f; w.foam = 0;
        FeatherView fv = featherView(&w, 8, 0, 18, 960, 540);
        Image im = look(fv.scene, 960, 540, 0);
        float spread = 0;
        const int left = armCrests(fv, im, 23.5f, 1, 7, 2.f, 3.f, &spread), right = armCrests(fv, im, 23.5f, -1, 7, 2.f, 3.f);
        dump("w8-feathers-test.png", im);
        UnloadImage(im);
        std::printf("[weather] feathers: %d and %d crests along the two arms over 21 m, 2 m inside the arm (want at least 5 each); luma swing %.1f levels\n", left, right, spread);
        CHECK(left >= 5 && right >= 5);
    }
    // (c) ripple radius grows at the group speed and the rings are round (GPU, slope view, top down).
    {
        constexpr int W = 960, H = 540;
        const float hgt = 4;
        float radius[2] = { 0, 0 };
        const int frames[2] = { 60, 135 };            // 2.0 s and 4.5 s
        for (int i = 0; i < 2; i++) {
            auto s = wakeWorld(topDown(0, 0, hgt), true);
            g3d::Ripple r; r.x = 0; r.z = 0; r.time = 0; r.strength = 0.4f;
            s->ripples.push_back(r);
            Image img = look(s, W, H, frames[i]);
            const float T = frames[i] / 30.f, c = g3d::Ripple::groupSpeed(r.peakWave());
            const auto rad = ringRadii(img, hgt, 16, 0.5f * c * T, 1.3f * c * T);
            dump(i == 0 ? "w8-rings-2s.png" : "w8-rings-4s.png", img);
            UnloadImage(img);
            float mean = 0, lo = 1e9f, hi = 0;
            for (float v : rad) { mean += v / rad.size(); lo = std::min(lo, v); hi = std::max(hi, v); }
            radius[i] = mean;
            const float spread = (hi - lo) / mean;
            std::printf("[weather] ripple at %.1f s: ring radius %.3f m (%.3f m/s; group speed of the peak wave %.3f m/s), spread over 16 directions %.1f%%\n", T, mean, mean / T, c, 100 * spread);
            CHECK(mean / T > 0.8f * c && mean / T < 1.2f * c);
            CHECK(spread < 0.12f);
        }
        std::printf("[weather] ripple radius grew %.2fx from 2.0 s to 4.5 s (time ratio 2.25: a constant radius gives 1)\n", radius[1] / radius[0]);
        CHECK(radius[1] / radius[0] > 1.7f && radius[1] / radius[0] < 2.8f);
    }
    // (d) a floating box on a rough sea: it follows the surface, its pitch follows the slope, the footprint calms it.
    {
        auto s = std::make_shared<g3d::Scene>();
        s->water = g3d::Water::choppy();
        g3d::Object3D box;
        box.position = { 5, 0, 3 };
        g3d::Ride how; how.length = 6; how.beam = 2.4f; how.heading = 0; how.along = 7; how.across = 5;
        g3d::Ride tiny = how; tiny.length = 0.05f; tiny.beam = 0.05f;
        std::vector<double> pitchBig, pitchTiny;
        double errMean = 0, signOk = 0, signN = 0, corrNum = 0, corrA = 0, corrB = 0;
        const int N = 240;
        for (int i = 0; i < N; i++) {
            const double t = i / 30.0;
            g3d::ride(box, *s, t, how);
            double mean = 0;
            for (int a = 0; a < 7; a++)
                for (int c = 0; c < 5; c++) mean += g3d::surfaceHeight(*s, 5 + ((a + 0.5f) / 7 - 0.5f) * 6, 3 + ((c + 0.5f) / 5 - 0.5f) * 2.4f, t) / 35.0;
            errMean = std::max(errMean, std::fabs(box.position.y - mean));
            const Vector3 bow = Vector3Transform({ 0, 0, 1 }, MatrixRotateXYZ(Vector3Scale(box.rotation, DEG2RAD)));
            pitchBig.push_back(bow.y);
            double slope = 0;
            for (int a = 0; a < 7; a++) slope += g3d::surfaceSlope(*s, 5 + ((a + 0.5f) / 7 - 0.5f) * 6, 3, t).x / 7.0;
            if (std::fabs(slope) > 0.01) { signN++; if ((slope > 0) == (bow.y > 0)) signOk++; }
            corrNum += slope * bow.y; corrA += slope * slope; corrB += bow.y * bow.y;
            g3d::Object3D t2; t2.position = { 5, 0, 3 };
            g3d::ride(t2, *s, t, tiny);
            pitchTiny.push_back(Vector3Transform({ 0, 0, 1 }, MatrixRotateXYZ(Vector3Scale(t2.rotation, DEG2RAD))).y);
        }
        auto jitter = [](const std::vector<double>& v) {
            double s2 = 0; int n = 0;
            for (size_t i = 1; i + 1 < v.size(); i++) { const double d = v[i + 1] - 2 * v[i] + v[i - 1]; s2 += d * d; n++; }
            return std::sqrt(s2 / n);
        };
        const double jb = jitter(pitchBig), jt = jitter(pitchTiny);
        const double corr = corrNum / std::sqrt(corrA * corrB + 1e-12);
        std::printf("[weather] floating box: height off the footprint mean at most %.4f m; pitch has the slope's sign in %.0f%% of %.0f frames, correlation %.2f; pitch jitter %.5f with a 6 m footprint against %.5f centre only (%.1fx)\n",
                    errMean, 100 * signOk / std::max(signN, 1.0), signN, corr, jb, jt, jt / std::max(jb, 1e-9));
        CHECK(errMean < 0.01);
        CHECK(signN > 20 && signOk / signN > 0.85);
        CHECK(corr > 0.7);
        CHECK(jb < 0.6 * jt);
        // its draft sinks it, and on a lake body a wake lifts it the same way
        g3d::Object3D dr; dr.position = { 5, 0, 3 };
        g3d::Ride deep = how; deep.draft = 0.3f;
        g3d::ride(dr, *s, 1.0, deep);
        g3d::ride(box, *s, 1.0, how);
        std::printf("[weather] draft 0.3 m: %.3f m lower\n", box.position.y - dr.position.y);
        CHECK(std::fabs(box.position.y - dr.position.y - 0.3f) < 1e-4f);
        auto lake = std::make_shared<g3d::Scene>();
        lake->waterBodies.push_back(g3d::WaterBody::lake({ { -300, -300 }, { 300, -300 }, { 300, 300 }, { -300, 300 } }, 0.5f));
        lake->waterBodies.back().look.waves.clear();
        g3d::Wake w = boatAt(5, 0, 30); w.length = 8; w.beam = 2.5f; w.strength = 2;
        lake->wakes.push_back(w);
        float lo = 1e9f, hi = -1e9f;
        g3d::Object3D lb; lb.position = { -12, 0, 6 };
        g3d::Ride lr; lr.length = 3; lr.beam = 1.5f; lr.heading = 0;
        g3d::ride(lb, *lake, -10.0, lr);
        const float still = lb.position.y;
        for (double t = 0; t < 40; t += 0.1) { g3d::ride(lb, *lake, t, lr); lo = std::min(lo, lb.position.y); hi = std::max(hi, lb.position.y); }
        std::printf("[weather] a box on a lake at 0.5 m before the boat: %.4f m; when the wake passes it ranges %.4f..%.4f m\n", still, lo, hi);
        CHECK(std::fabs(still - 0.5f) < 1e-4f);
        CHECK(hi - lo > 0.02f);
    }
    // (e) a dinghy beside the course starts to move no earlier than the waves can reach it (group speed U/2 at most).
    {
        auto s = std::make_shared<g3d::Scene>();
        s->waterBodies.push_back(g3d::WaterBody::lake({ { -500, -500 }, { 500, -500 }, { 500, 500 }, { -500, 500 } }, 0));
        s->waterBodies.back().look.waves.clear();
        g3d::Wake w; w.path = { { -100, 0 }, { 100, 0 } }; w.speed = 5; w.start = 0; w.life = 40; w.length = 6; w.beam = 2;
        s->wakes.push_back(w);
        const Vector2 dinghy{ 0, 10 };
        float bound = 1e9f;
        for (float tau = 0; tau < 40; tau += 0.02f) {
            Vector2 p, h; w.at(tau, &p, &h);
            bound = std::min(bound, tau + std::hypot(p.x - dinghy.x, p.y - dinghy.y) / (0.5f * w.speed));
        }
        g3d::Object3D d; d.position = { dinghy.x, 0, dinghy.y };
        g3d::Ride r; r.length = 3.2f; r.beam = 1.5f; r.heading = -90;
        float onset = -1, biggest = 0;
        for (float t = 0; t < 60; t += 0.05f) {
            g3d::ride(d, *s, t, r);
            const float m = std::fabs(d.position.y);
            biggest = std::max(biggest, m);
            if (onset < 0 && m > 0.002f) onset = t;
        }
        std::printf("[weather] dinghy 10 m off the course: first motion (over 2 mm) at %.2f s, the earliest a wave can arrive is %.2f s; it rises %.3f m at most\n", onset, bound, biggest);
        CHECK(onset > bound - 1.5f);
        CHECK(biggest > 0.02f);
    }
    // (f) pure functions of the time, on the CPU and on the GPU.
    {
        const g3d::Wake w = boatAt(4, 20);
        const float h1 = w.height(10, 3, 5), h2 = w.height(10, 3, 5);
        const float other = w.height(-30, -8, 11);
        const float h3 = w.height(10, 3, 5);
        std::printf("[weather] wake height at one point: %.6f %.6f, and %.6f after another time and place (%.6f)\n", h1, h2, h3, other);
        CHECK(h1 == h2 && h1 == h3);
        auto make = [] {
            auto s = wakeWorld(topDown(0, 0, 8), false);
            s->wakes.push_back(boatAt(4, 0));
            g3d::Ripple r; r.x = 2; r.time = 0.5f; r.strength = 0.3f;
            s->ripples.push_back(r);
            return s;
        };
        Image a = look(make(), 320, 180, 40);
        Image warm = look(make(), 320, 180, 10);
        Image b = look(make(), 320, 180, 40);
        const long long d = diffImages(a, b);
        std::printf("[weather] frame 40 alone against frame 40 after another frame: diff %lld\n", d);
        CHECK(d == 0);
        UnloadImage(a); UnloadImage(warm); UnloadImage(b);
    }
    // (g) off means off: a boat that has not started and a stone that has not fallen change nothing.
    {
        auto base = [] { return wakeWorld(topDown(0, 0, 8), false); };
        auto early = base();
        g3d::Wake w = boatAt(4, 0); w.start = 1000;
        early->wakes.push_back(w);
        g3d::Ripple r; r.time = 900;
        early->ripples.push_back(r);
        Image a = look(base(), 320, 180, 40), b = look(early, 320, 180, 40);
        const long long d = diffImages(a, b);
        auto live = base();
        live->wakes.push_back(boatAt(4, 0));
        Image c = look(live, 320, 180, 40);
        const long long on = diffImages(a, c);
        std::printf("[weather] a boat and a stone that come later: diff %lld against none; the boat under way: diff %lld\n", d, on);
        CHECK(d == 0);
        CHECK(on > 0);
        UnloadImage(a); UnloadImage(b); UnloadImage(c);
    }
}
static void cloudImages(const char* dir) {
    fs::create_directories(dir);
    constexpr int W = 1920, H = 1080;
    auto shot = [&](const char* name, g3d::ScenePtr sc, int frame = 0) {
        Image img = look(std::move(sc), W, H, frame);
        ExportImage(img, (fs::path(dir) / name).string().c_str());
        UnloadImage(img);
    };
    const float c30 = std::tan(30 * DEG2RAD);
    shot("under-cumulus-noon.png", cloudScene({ 0, 2, 0 }, { 0, 2 + c30 * 100, -100 }, 0.45f));
    {                                                         // the noon clouds' left edge at 3x (nearest neighbour), from the same frame
        Image full = look(cloudScene({ 0, 2, 0 }, { 0, 2 + c30 * 100, -100 }, 0.45f), W, H, 0);
        Image crop = ImageFromImage(full, { 0, 680, 400, 220 });
        ImageResizeNN(&crop, 1200, 660);
        ExportImage(crop, (fs::path(dir) / "noon-edge-3x.png").string().c_str());
        UnloadImage(crop); UnloadImage(full);
    }
    // Dusk: the camera faces the low sun, so the clouds are lit from behind (silver lining, orange tops).
    {
        Vector3 sun = g3d::Sky::at(17.6f).sunDirection;
        sun.y = 0;
        sun = Vector3Normalize(sun);
        shot("under-cumulus-dusk.png", cloudScene({ 0, 2, 0 }, { sun.x * 100, 2 + 14, sun.z * 100 }, 0.5f, g3d::CloudType::Cumulus, 17.6f));
    }
    shot("inside-cloud.png", cloudScene({ 0, 2200, 0 }, { 0, 2200, -100 }, 0.8f));
    shot("above-sea.png", cloudScene({ 0, 3000, 0 }, { 0, 2800, -100 }, 0.6f, g3d::CloudType::Deck, 12.5f, true, 600, 2000));
    shot("from-12km.png", cloudScene({ 0, 12000, 0 }, { 0, 11990, -100 }, 0.5f));
    // A storm tower from the ground, 20 km away, looking slightly up; and its top at 3x.
    {
        auto s = cloudScene({ -1500, 2, 20000 }, { -1500, 5600, 0 }, 0.05f, g3d::CloudType::Cumulonimbus, 15.f, true, 1200, 11000);
        s->camera.fov = 75;
        Image full = look(s, W, H, 0);
        ExportImage(full, (fs::path(dir) / "cumulonimbus-20km.png").string().c_str());
        Image crop = ImageFromImage(full, { 700, 100, 400, 220 });
        ImageResizeNN(&crop, 1200, 660);
        ExportImage(crop, (fs::path(dir) / "cumulonimbus-top-3x.png").string().c_str());
        UnloadImage(crop); UnloadImage(full);
    }
    // The ground with the clouds' shadows, two seconds apart (frames 0 and 60): an oblique view over the plain.
    for (int frame : { 0, 60 }) {
        auto s = cloudScene({ 0, 700, 1500 }, { 0, 0, -1500 }, 0.5f, g3d::CloudType::Cumulus, 14.f, true, 2200, 3600);
        s->clouds.shadows = true;
        s->clouds.shadowExtent = 9000;
        s->weather = g3d::Weather::clear();
        s->weather.haze = 0.02f;
        s->weather.wind.direction = 90;
        s->weather.wind.speed = 40;                       // 80 m in two seconds
        s->weather.clouds.cover = 0.5f;
        s->camera.fov = 70;
        shot(frame == 0 ? "cloud-shadows-t0.png" : "cloud-shadows-t2s.png", s, frame);
    }
    // A cloud edge at Film quality, 3x enlarged (nearest neighbour) from a 640 x 360 render: the banding check's crop.
    {
        auto s = cloudScene({ 0, 2, 0 }, { 0, 2 + 35, -100 }, 0.45f);
        s->camera.fov = 60;
        s->clouds.quality = g3d::Clouds::Quality::Film;
        Image full = look(s, 640, 360, 0);
        Image crop = ImageFromImage(full, { 300, 110, 320, 180 });
        ImageResizeNN(&crop, 320 * 3, 180 * 3);
        ExportImage(crop, (fs::path(dir) / "cloud-edge-film-3x.png").string().c_str());
        ExportImage(full, (fs::path(dir) / "cloud-edge-film-full.png").string().c_str());
        UnloadImage(crop); UnloadImage(full);
    }
}

// ---------------------------------------------------------------- W3: precipitation (rain and snow at scale)
// A side-on orthographic view (no perspective, so a drop's pixel is its world place) of rain on a black background,
// 16.2 pixels a metre: 0.37 m is 6.0 px. Drops are drawn fat (6-9 cm) so a pixel is a measurement, and thin so they
// rarely overlap. Every check has a break that must fail it (see the comments).
static constexpr int PW = 320, PH = 180;
static constexpr float P_HEIGHT = 11.1f, P_PPM = PH / P_HEIGHT;

static g3d::Downfall testRain(float density = 0.3f) {
    g3d::Downfall d;
    d.enabled = true;
    d.kind = g3d::Precipitation::Rain;
    d.intensity = 1;
    d.rainDensity = density;
    d.snowDensity = density;
    d.rainSize = { 0.06f, 0.09f };
    d.opacity = 2;
    d.snowSize = { 0.07f, 0.10f };
    d.farLayer = false;
    d.splashes = false;
    return d;
}
static g3d::ScenePtr dropScene(const g3d::Downfall& d, Vector3 cam = { 0, 1.5f, 8 }) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = cam;
    s->camera.target = { cam.x, cam.y, cam.z - 8 };
    s->camera.orthographic = true;
    s->camera.orthoHeight = P_HEIGHT;
    s->ambient = 1;
    s->skyColor = WHITE;
    s->groundColor = WHITE;
    s->precipitation = d;
    return s;
}
static g3d::ScenePtr tunnelScene(const g3d::Downfall& d) {          // a roof 3 m wide and 120 m long over the camera and the view
    auto s = dropScene(d, { 0, 1.5f, 30 });
    g3d::Material m;
    m.color = { 40, 40, 44, 255 };
    s->add(g3d::Geometry::box(3, 0.1f, 120), m)->position = { 0, 3.0f, 0 };
    return s;
}
// Pixels in the rectangle that `img` lights more than `base` does.
static int litIn(const std::vector<float>& img, const std::vector<float>& base, int x0, int y0, int x1, int y1, float thr = 25) {
    int n = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            if (img[(size_t)y * PW + x] - base[(size_t)y * PW + x] > thr) n++;
    return n;
}
static std::vector<float> dropLuma(g3d::ScenePtr s, int frame = 5) {
    Image img = look(std::move(s), PW, PH, frame);
    std::vector<float> v = lumaOf(img);
    UnloadImage(img);
    return v;
}
static std::vector<float> noRain(bool tunnel) {
    g3d::Downfall off = testRain();
    off.enabled = false;
    return dropLuma(tunnel ? tunnelScene(off) : dropScene(off));
}
static int totalLit(const std::vector<float>& a, const std::vector<float>& b) { return litIn(a, b, 0, 0, PW, PH); }

// (a) Under a roof there is no rain, beside it there is. Break: occlusion off.
static void roofKeepsRainOff() {
    const std::vector<float> base = noRain(true);
    const int ux0 = PW / 2 - (int)(1.2f * P_PPM), ux1 = PW / 2 + (int)(1.2f * P_PPM), bx0 = PW / 2 + (int)(3.0f * P_PPM);
    const int y0 = PH / 2 - (int)((2.8f - 1.5f) * P_PPM), y1 = PH / 2 + (int)(1.5f * P_PPM), wdt = ux1 - ux0;
    g3d::Downfall d = testRain();
    const std::vector<float> on = dropLuma(tunnelScene(d));
    d.occlusion = false;
    const std::vector<float> off = dropLuma(tunnelScene(d));
    const int under = litIn(on, base, ux0, y0, ux1, y1), beside = litIn(on, base, bx0, y0, bx0 + wdt, y1);
    const int underBroken = litIn(off, base, ux0, y0, ux1, y1);
    std::printf("[weather] roof: lit pixels under %d, beside %d (same size, %dx%d); occlusion off: under %d\n", under, beside, wdt, y1 - y0, underBroken);
    CHECK(under <= 2);
    CHECK(beside > 60);
    CHECK(underBroken > 60);
}

// (b) Drop coverage grows in proportion to the intensity.
static void intensityScales() {
    const std::vector<float> base = noRain(false);
    int c[3];
    const float levels[3] = { 0.25f, 0.5f, 1.f };
    for (int i = 0; i < 3; i++) {
        g3d::Downfall d = testRain();
        d.intensity = levels[i];
        c[i] = totalLit(dropLuma(dropScene(d)), base);
    }
    const float r25 = (float)c[0] / (0.25f * c[2]), r50 = (float)c[1] / (0.5f * c[2]);
    std::printf("[weather] intensity: lit pixels %d, %d, %d at 0.25, 0.5, 1 (coverage / (intensity x full): %.2f, %.2f)\n", c[0], c[1], c[2], r25, r50);
    CHECK(c[2] > 2000);
    CHECK(r25 > 0.75f && r25 < 1.25f);
    CHECK(r50 > 0.75f && r50 < 1.25f);
}

// The separate streaks of a picture: 4-connected blobs of lit pixels (luma over 25) with their extent and second moments.
struct StreakBlob { int area = 0, minY = 0, maxY = 0; bool border = false; double mx = 0, my = 0, mxx = 0, myy = 0, mxy = 0; };
static std::vector<StreakBlob> findStreakBlobs(const std::vector<float>& v) {
    std::vector<unsigned char> on((size_t)PW * PH), seen((size_t)PW * PH, 0);
    for (size_t i = 0; i < on.size(); i++) on[i] = v[i] > 25 ? 1 : 0;
    std::vector<StreakBlob> out;
    for (int y0 = 0; y0 < PH; y0++)
        for (int x0 = 0; x0 < PW; x0++) {
            if (!on[(size_t)y0 * PW + x0] || seen[(size_t)y0 * PW + x0]) continue;
            std::vector<int> stack{ y0 * PW + x0 };
            seen[(size_t)y0 * PW + x0] = 1;
            StreakBlob b;
            b.minY = b.maxY = y0;
            while (!stack.empty()) {
                const int p = stack.back(); stack.pop_back();
                const int x = p % PW, y = p / PW;
                b.area++; b.minY = std::min(b.minY, y); b.maxY = std::max(b.maxY, y);
                b.mx += x; b.my += y; b.mxx += (double)x * x; b.myy += (double)y * y; b.mxy += (double)x * y;
                if (x == 0 || y == 0 || x == PW - 1 || y == PH - 1) b.border = true;
                const int nb[4][2] = { { x + 1, y }, { x - 1, y }, { x, y + 1 }, { x, y - 1 } };
                for (const auto& q : nb)
                    if (q[0] >= 0 && q[1] >= 0 && q[0] < PW && q[1] < PH && on[(size_t)q[1] * PW + q[0]] && !seen[(size_t)q[1] * PW + q[0]]) {
                        seen[(size_t)q[1] * PW + q[0]] = 1;
                        stack.push_back(q[1] * PW + q[0]);
                    }
            }
            out.push_back(b);
        }
    return out;
}

// The median tilt from vertical, in degrees (positive: the streaks lean to the right going down), of the long streaks' principal axes.
static float streakTilt(const std::vector<float>& v) {
    std::vector<float> tilts;
    for (const StreakBlob& b : findStreakBlobs(v)) {
        if (b.border || b.area < 15) continue;
        const double n = b.area, mx = b.mx / n, my = b.my / n;
        const double cxx = b.mxx / n - mx * mx, cyy = b.myy / n - my * my, cxy = b.mxy / n - mx * my;
        tilts.push_back((float)(0.5 * std::atan2(2 * cxy, cyy - cxx) * RAD2DEG));
    }
    if (tilts.size() < 10) return 999.f;
    std::sort(tilts.begin(), tilts.end());
    return tilts[tilts.size() / 2];
}

// (c) The streaks lean as atan(follow x wind / the mean fall speed). Break: the wind not applied.
static void windSlants() {
    float err[2];
    const float winds[2] = { 4.f, 10.f };
    for (int i = 0; i < 2; i++) {
        g3d::Downfall d = testRain(0.1f);
        d.exposure = 0.15f;
        d.windDirection = 0;                                       // towards +x: to the right of this view
        d.windSpeed = winds[i];
        const float want = std::atan(g3d::Weather::kRainFollow * winds[i] / 7.5f) * RAD2DEG;
        const float got = streakTilt(dropLuma(dropScene(d)));
        g3d::Downfall still = d;
        still.windSpeed = 0;
        const float broken = streakTilt(dropLuma(dropScene(still)));
        err[i] = std::fabs(got - want);
        std::printf("[weather] slant at %.0f m/s: measured %.1f deg, expected %.1f; with the wind ignored %.1f\n", winds[i], got, want, broken);
        CHECK(err[i] < 5);
        CHECK(std::fabs(broken - want) > 8);
    }
}

// Pearson correlation of two luma images shifted by `dx` pixels (a[x] against b[x - dx]), over the overlap less a margin.
static float shiftedCorrelation(const std::vector<float>& a, const std::vector<float>& b, int dx) {
    double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0;
    long long n = 0;
    for (int y = 12; y < PH - 12; y++)
        for (int x = 20; x < PW - 20; x++) {
            const double p = a[(size_t)y * PW + x], q = b[(size_t)y * PW + x - dx];
            sa += p; sb += q; saa += p * p; sbb += q * q; sab += p * q; n++;
        }
    const double cov = sab / n - sa / n * sb / n, va = saa / n - sa / n * sa / n, vb = sbb / n - sb / n * sb / n;
    return (float)(cov / std::sqrt(std::max(va * vb, 1e-9)));
}

// (d) Moving the camera 0.37 m sideways (not a tile multiple) moves the picture by exactly that, nothing else. Break: positions made
// relative to the camera (worldFixed off).
static void worldFixed() {
    float good[3], broken[3];
    const int shifts[3] = { 6, 3, 9 };
    for (int pass = 0; pass < 2; pass++) {
        g3d::Downfall d = testRain(0.15f);
        d.exposure = 0.05f;
        d.worldFixed = pass == 0;
        const std::vector<float> a = dropLuma(dropScene(d, { 0, 1.5f, 8 })), b = dropLuma(dropScene(d, { 0.37f, 1.5f, 8 }));
        for (int i = 0; i < 3; i++) (pass == 0 ? good : broken)[i] = shiftedCorrelation(a, b, shifts[i]);
    }
    std::printf("[weather] world-fixed: correlation at the 6 px parallax %.2f (3 px %.2f, 9 px %.2f); drops carried with the camera: %.2f at 6 px\n", good[0], good[1], good[2], broken[0]);
    CHECK(good[0] > 0.9f);
    CHECK(good[1] < 0.5f && good[2] < 0.5f);
    CHECK(broken[0] < 0.5f);
}

// The mean height in pixels of the separate streaks (blobs that do not touch the border).
static float meanStreakHeight(const std::vector<float>& v, int* count = nullptr) {
    double sum = 0;
    int n = 0;
    for (const StreakBlob& b : findStreakBlobs(v))
        if (!b.border && b.area >= 4) { sum += b.maxY - b.minY + 1; n++; }
    if (count) *count = n;
    return n ? (float)(sum / n) : 0.f;
}

// (e) A streak is as long as speed x shutter: double the exposure, double the streak. Break: a fixed length.
static void streakLength() {
    float m[2];
    int n[2];
    const float exposures[2] = { 0.1f, 0.2f };
    for (int i = 0; i < 2; i++) {
        g3d::Downfall d = testRain(0.04f);
        d.exposure = exposures[i];
        m[i] = meanStreakHeight(dropLuma(dropScene(d)), &n[i]);
    }
    std::printf("[weather] streak length: mean %.1f px over %d streaks at 0.1 s, %.1f px over %d at 0.2 s (ratio %.2f; expected 2, 7.5 m/s x 0.1 s = %.1f px)\n",
                m[0], n[0], m[1], n[1], m[1] / std::max(m[0], 1e-3f), 7.5f * 0.1f * P_PPM);
    CHECK(n[0] > 15 && n[1] > 15);
    CHECK(m[1] / m[0] > 1.7f && m[1] / m[0] < 2.3f);
    CHECK(m[0] > 0.7f * 7.5f * 0.1f * P_PPM && m[0] < 1.3f * 7.5f * 0.1f * P_PPM);
}

// ---- the shader against its CPU twin, projected into the picture
// How many of the CPU mirror's drops sit on a lit pixel (3x3 around where the mirror says they are), the picture
// shifted `shift` px sideways (0: the real thing; a shift is the control that must find fewer).
static float mirrorHits(const g3d::Downfall& d, int frame, int shift, int* tested = nullptr) {
    Image shot = look(dropScene(d), PW, PH, frame);
    const std::vector<float> v = lumaOf(shot);
    const double secs = frame / 30.0;
    int hit = 0, total = 0;
    for (int iy = -1; iy <= 0; iy++)
        for (int iz = -1; iz <= 1; iz++)
            for (int ix = -2; ix <= 1; ix++)
                for (uint32_t k = 0; k < d.dropsPerTile(); k++) {
                    const g3d::Downfall::Drop p = d.drop(ix, iy, iz, k, secs);
                    const float dist = Vector3Distance(p.position, { 0, 1.5f, 8 });
                    if (p.rank > 0.85f || dist > 9.5f || dist < 2.5f || p.position.z > 5.5f) continue;     // in front of the camera: an orthographic one clips what is behind it
                    const int px = (int)std::floor(PW * 0.5f + p.position.x * P_PPM) + shift, py = (int)std::floor(PH * 0.5f - (p.position.y - 1.5f) * P_PPM);
                    if (px < 12 || py < 12 || px >= PW - 12 || py >= PH - 12) continue;
                    total++;
                    float best = 0;
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dx = -1; dx <= 1; dx++) best = std::max(best, v[(size_t)(py + dy) * PW + px + dx]);
                    if (best > 20) hit++;
                }
    UnloadImage(shot);
    if (tested) *tested = total;
    return total ? (float)hit / total : 0.f;
}

// (h) Snow falls slowly and sways: its drops over one second move sideways by a bounded, nonzero amount and fall 0.5-1.5 m.
static void snowSways() {
    for (int wind = 0; wind < 2; wind++) {
        g3d::Downfall d = testRain();
        d.kind = g3d::Precipitation::Snow;
        d.windDirection = 40;
        d.windSpeed = wind ? 3.f : 0.f;
        double hMin = 1e9, hMax = 0, fMin = 1e9, fMax = 0, hSum = 0;
        int n = 0;
        for (uint32_t k = 0; k < d.dropsPerTile(); k++) {
            const g3d::Downfall::Drop a = d.drop(0, 0, 0, k, 3.0), b = d.drop(0, 0, 0, k, 4.0);
            if (b.phase < a.phase) continue;                                   // wrapped to the top of its tile this second
            const double h = std::hypot(b.position.x - a.position.x, b.position.z - a.position.z), f = a.position.y - b.position.y;
            hMin = std::min(hMin, h); hMax = std::max(hMax, h); fMin = std::min(fMin, f); fMax = std::max(fMax, f); hSum += h; n++;
        }
        std::printf("[weather] snow (wind %d m/s): %d flakes, sideways %.3f..%.3f m in 1 s (mean %.2f), fall %.2f..%.2f m/s\n", wind * 3, n, hMin, hMax, hSum / std::max(n, 1), fMin, fMax);
        CHECK(n > 50);
        CHECK(hMin > 0.002 && hMax < (wind ? 3.0 : 0.8));
        CHECK(fMin > 0.49 && fMax < 1.51);
    }
    // The shader draws the same flakes as the mirror says: where it says, not where it does not.
    g3d::Downfall d = testRain(0.2f);
    d.kind = g3d::Precipitation::Snow;
    d.windSpeed = 2;
    d.windDirection = 0;
    int n = 0;
    const float real = mirrorHits(d, 40, 0, &n), control = mirrorHits(d, 40, 9);
    std::printf("[weather] snow flakes on the mirror's places: %.2f of %d (shifted 9 px: %.2f)\n", real, n, control);
    CHECK(n > 40);
    CHECK(real > 0.9f && control < 0.5f);
}

// Rain, too, is where the mirror puts it.
static void mirrorsRain() {
    g3d::Downfall d = testRain(0.2f);
    d.windSpeed = 6;
    d.windDirection = 0;
    d.exposure = 0.05f;
    int n = 0;
    const float real = mirrorHits(d, 17, 0, &n), control = mirrorHits(d, 17, 9);
    std::printf("[weather] rain drops on the mirror's places: %.2f of %d (shifted 9 px: %.2f)\n", real, n, control);
    CHECK(n > 40);
    CHECK(real > 0.9f && control < 0.5f);
}

// (f) A frame is the same alone as after others; a different frame is different.
static void precipitationPurity() {
    auto make = [] {
        g3d::Downfall d = testRain(0.3f);
        d.splashes = true;
        d.farLayer = true;
        d.splashRadius = 40;
        d.windSpeed = 6;
        d.windDirection = 20;
        auto s = tunnelScene(d);
        s->camera.orthographic = false;
        s->camera.position = { 0, 2.2f, 30 };
        s->camera.target = { 0, 1.0f, 22 };
        s->camera.fov = 60;
        g3d::Material m;
        m.color = { 30, 40, 30, 255 };
        s->add(g3d::Geometry::plane(200, 200), m);
        return s;
    };
    Image alone = look(make(), PW, PH, 7);
    Comp c = comp(make(), PW, PH);
    Image x1 = render(c, 2), x2 = render(c, 20), after = render(c, 7), other = render(c, 8);
    const long long same = diffImages(alone, after), differs = diffImages(after, other);
    std::printf("[weather] precipitation purity: frame 7 alone vs after frames 2, 20: diff %lld; frame 8 vs 7: diff %lld\n", same, differs);
    CHECK(same == 0);
    CHECK(differs > 5000);
    UnloadImage(alone); UnloadImage(x1); UnloadImage(x2); UnloadImage(after); UnloadImage(other);
}

// (g) Disabled, the precipitation changes no pixel and submits no drop, whatever else it holds.
static void precipitationOff() {
    auto make = [](bool set, bool enabled) {
        g3d::Downfall d = testRain(0.3f);
        d.kind = g3d::Precipitation::Snow;
        d.intensity = 1;
        d.splashes = true;
        d.farLayer = true;
        d.enabled = enabled;
        auto s = tunnelScene(g3d::Downfall{});
        if (set) s->precipitation = d;
        return s;
    };
    Image never = look(make(false, false), PW, PH, 9), off = look(make(true, false), PW, PH, 9);
    const long long dOff = diffImages(never, off);
    const long long dropsOff = g3d::precipitationDropsSubmitted();
    Image on = look(make(true, true), PW, PH, 9);
    const long long dOn = diffImages(never, on);
    std::printf("[weather] precipitation off: diff %lld, drops submitted %lld; on: diff %lld, drops %lld, splashes %lld\n", dOff, dropsOff, dOn, g3d::precipitationDropsSubmitted(), g3d::precipitationSplashesSubmitted());
    CHECK(dOff == 0 && dropsOff == 0);
    CHECK(dOn > 20000);
    UnloadImage(never); UnloadImage(off); UnloadImage(on);
}

// (i) Splashes sit on the surface they land on: on the ground and on a table top, not above them. A 3/4 view from above; the
// CPU mirror says where every splash should be (from the hash alone), and the picture is searched there and, as the control, 0.6 m higher.
static g3d::ScenePtr splashScene(const g3d::Downfall& d, Vector3* right, Vector3* up, Vector3* target) {
    auto s = std::make_shared<g3d::Scene>();
    const Vector3 tgt{ 0, 0.5f, 0 };
    const Vector3 back = Vector3Scale(Vector3Normalize({ 0, std::sin(35 * DEG2RAD), std::cos(35 * DEG2RAD) }), 14.f);
    s->camera.position = Vector3Add(tgt, back);
    s->camera.target = tgt;
    s->camera.orthographic = true;
    s->camera.orthoHeight = P_HEIGHT;
    s->ambient = 1;
    s->skyColor = WHITE;
    s->groundColor = WHITE;
    g3d::Material dark;
    dark.color = { 6, 6, 6, 255 };
    dark.unlit = true;
    s->add(g3d::Geometry::plane(300, 300), dark);
    s->add(g3d::Geometry::box(10, 0.2f, 10), dark)->position = { 0, 0.9f, 0 };     // the top is at y = 1
    s->precipitation = d;
    const Vector3 fwd = Vector3Normalize(Vector3Subtract(tgt, s->camera.position));
    *right = Vector3Normalize(Vector3CrossProduct(fwd, { 0, 1, 0 }));
    *up = Vector3CrossProduct(*right, fwd);
    *target = tgt;
    return s;
}
static void splashesLand() {
    g3d::Downfall d = testRain(0.15f);
    d.splashes = true;
    d.splashRadius = 40;
    d.exposure = 0.001f;                          // the drops themselves nearly vanish: what is left is the splashes
    d.rainSize = { 0.008f, 0.012f };              // so a splash is about 0.2 m across: a few pixels
    const int frame = 21;
    const double secs = frame / 30.0;
    Vector3 right, up, tgt;
    const std::vector<float> with = dropLuma(splashScene(d, &right, &up, &tgt), frame);
    g3d::Downfall none = d;
    none.splashes = false;
    const std::vector<float> without = dropLuma(splashScene(none, &right, &up, &tgt), frame);
    int expected = 0, onSurface = 0, floating = 0, onTable = 0, onGround = 0;
    for (int iy = -1; iy <= 0; iy++)
        for (int iz = -2; iz <= 1; iz++)
            for (int ix = -2; ix <= 1; ix++)
                for (uint32_t k = 0; k < d.dropsPerTile(); k++) {
                    const g3d::Downfall::Drop p = d.drop(ix, iy, iz, k, secs);
                    if (p.snow || p.rank > 0.9f) continue;
                    // The surface this drop's line of fall meets (no wind: it falls straight): the table if over it, else the ground.
                    const bool overTable = std::fabs(p.position.x) < 5 && std::fabs(p.position.z) < 5;
                    const float ys = overTable ? 1.f : 0.f, yBase = iy * g3d::Downfall::kCell, hL = ys - yBase;
                    if (hL <= 0 || hL > g3d::Downfall::kCell) continue;      // the drop belongs to another tile's cell
                    const float fl = 1 - hL / g3d::Downfall::kCell, v = -p.velocity.y;
                    if (p.phase < fl || (p.phase - fl) * g3d::Downfall::kCell / v > 0.2f) continue;      // not splashing now (or just born)
                    const Vector3 at{ p.position.x, ys + 0.004f, p.position.z };
                    auto lit = [&](Vector3 w) {
                        const Vector3 r = Vector3Subtract(w, tgt);
                        const int px = (int)std::floor(PW * 0.5f + Vector3DotProduct(r, right) * P_PPM), py = (int)std::floor(PH * 0.5f - Vector3DotProduct(r, up) * P_PPM);
                        if (px < 10 || py < 10 || px >= PW - 10 || py >= PH - 10) return -1;
                        return litIn(with, without, px - 3, py - 3, px + 4, py + 4, 2) > 0 ? 1 : 0;
                    };
                    const int a = lit(at), b = lit({ at.x, at.y + 1.0f, at.z });
                    if (a < 0 || b < 0) continue;
                    expected++; onSurface += a; floating += b;
                    (overTable ? onTable : onGround)++;
                }
    const int pixels = litIn(with, without, 0, 0, PW, PH, 2);
    std::printf("[weather] splashes: %d expected by the hash (%d on the table, %d on the ground), %d found at the surface, %d found 1 m above it; %d splash pixels in all\n",
                expected, onTable, onGround, onSurface, floating, pixels);
    CHECK(expected > 40 && onTable > 8 && onGround > 10);
    CHECK(onSurface > 0.8f * expected);
    CHECK(floating < 0.6f * onSurface);
    CHECK(pixels > 120);                       // the splashes are sparse and faint now (a spray, not blobs); see splashesAreSparks
}

// What the rain costs at 1080p: a porch-sized scene with a few objects, drops at intensity 1, with and without splashes and the far layer.
static g3d::ScenePtr timingRain(g3d::Precipitation kind, bool on, bool splashes, bool farLayer, bool occlusion) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { -1.2f, 1.55f, 9 };
    s->camera.target = { 0.4f, 1.45f, 0 };
    s->camera.fov = 48;
    s->ambient = 0.3f;
    s->skyColor = { 111, 127, 163, 255 };
    s->groundColor = { 42, 46, 36, 255 };
    s->background = { 90, 100, 120, 255 };
    s->fogMode = g3d::FogMode::Exponential;
    s->fogDensity = 0.004f;
    s->fogColor = { 120, 128, 144, 255 };
    s->lights.push_back(g3d::Light::directional({ -10, 1.8f, -14 }, { 255, 185, 138, 255 }, 2.4f));
    s->lights.push_back(g3d::Light::point({ 3.2f, 2.6f, 3.2f }, { 255, 183, 102, 255 }, 3.2f, 11));
    g3d::Material ground, wall, roof;
    ground.color = { 43, 50, 37, 255 };
    wall.color = { 148, 134, 114, 255 };
    roof.color = { 74, 59, 51, 255 };
    s->add(g3d::Geometry::plane(300, 300), ground);
    s->add(g3d::Geometry::box(12, 3.5f, 5), wall)->position = { 0, 1.75f, -4.5f };
    s->add(g3d::Geometry::box(5.6f, 0.16f, 3.8f), roof)->position = { 0, 2.78f, -0.2f };
    g3d::Downfall& d = s->precipitation;
    d.enabled = on;
    d.kind = kind;
    d.intensity = 1;
    d.splashes = splashes;
    d.farLayer = farLayer;
    d.occlusion = occlusion;
    return s;
}
// How many near-layer drops of the timing scene are in the view and survive the shader's thinning (by the CPU mirror: every tile
// in range, every drop whose rank is under intensity x keep(distance), seen inside the frustum of the 1920 x 1080 view).
static long long visibleNearDrops(const g3d::Scene& s) {
    const g3d::Downfall& d = s.precipitation;
    const g3d::Camera& cam = s.camera;
    const Vector3 f = Vector3Normalize(Vector3Subtract(cam.target, cam.position)), r = Vector3Normalize(Vector3CrossProduct(f, cam.upVector)), u = Vector3CrossProduct(r, f);
    const float ty = std::tan(cam.fov * 0.5f * DEG2RAD), tx = ty * 1920.f / 1080.f;
    const float R = d.radius, S = g3d::Downfall::kTile, Hc = g3d::Downfall::kCell;
    long long n = 0;
    for (int iy = (int)std::floor((cam.position.y - R) / Hc); iy <= (int)std::floor((cam.position.y + R) / Hc); iy++)
        for (int iz = (int)std::floor((cam.position.z - R) / S) - 2; iz <= (int)std::floor((cam.position.z + R) / S) + 2; iz++)
            for (int ix = (int)std::floor((cam.position.x - R) / S) - 2; ix <= (int)std::floor((cam.position.x + R) / S) + 2; ix++) {
                const Vector3 centre{ (ix + 0.5f) * S, (iy + 0.5f) * Hc, (iz + 0.5f) * S };
                const Vector3 cv = Vector3Subtract(centre, cam.position);
                const float cz = Vector3DotProduct(cv, f);
                if (Vector3Length(cv) > R + 9 || cz < -9 || std::fabs(Vector3DotProduct(cv, r)) > cz * tx + 12 || std::fabs(Vector3DotProduct(cv, u)) > cz * ty + 12) continue;
                for (uint32_t k = 0; k < d.dropsPerTile(); k++) {
                    const g3d::Downfall::Drop p = d.drop(ix, iy, iz, k, 0.4);
                    const Vector3 v = Vector3Subtract(p.position, cam.position);
                    const float z = Vector3DotProduct(v, f), dist = Vector3Length(v);
                    if (z < 0.4f || std::fabs(Vector3DotProduct(v, r)) > z * tx || std::fabs(Vector3DotProduct(v, u)) > z * ty) continue;
                    const float t1 = std::clamp((dist - 0.75f * R) / (0.25f * R), 0.f, 1.f), t2 = std::clamp((dist - 0.4f) / 1.6f, 0.f, 1.f);
                    const float keep = std::min(1.f, std::pow(0.35f * R / dist, 1.5f)) * (1 - t1 * t1 * (3 - 2 * t1)) * (t2 * t2 * (3 - 2 * t2));
                    if (p.rank < d.intensity * keep) n++;
                }
            }
    return n;
}

static void precipitationTiming() {
    constexpr int W = 1920, H = 1080;
    struct Row { const char* name; g3d::Precipitation kind; bool on, splashes, farLayer, occlusion; };
    const Row rows[] = { { "no rain", g3d::Precipitation::Rain, false, false, false, false },
                         { "rain: near drops only", g3d::Precipitation::Rain, true, false, false, false },
                         { "rain: + occlusion map", g3d::Precipitation::Rain, true, false, false, true },
                         { "rain: + far curtains", g3d::Precipitation::Rain, true, false, true, true },
                         { "rain: + splashes (everything)", g3d::Precipitation::Rain, true, true, true, true },
                         { "snow: everything", g3d::Precipitation::Snow, true, true, true, true } };
    for (const Row& row : rows) {
        g3d::ScenePtr scene = timingRain(row.kind, row.on, row.splashes, row.farLayer, row.occlusion);
        if (row.on && row.splashes && row.kind == g3d::Precipitation::Rain)
            std::printf("[weather] W3 near drops in view after thinning (rain, intensity 1, 1080p): %lld\n", visibleNearDrops(*scene));
        Comp c = comp(scene, W, H);
        Canvas canvas;
        canvas.load(W, H, 1);
        presentFrame(c, 0, canvas);
        Image warm = LoadImageFromTexture(canvas.output().texture);
        UnloadImage(warm);
        double best = 1e9, mean = 0;
        for (int i = 0; i < 12; i++) {
            const double t0 = GetTime();
            presentFrame(c, i, canvas);
            Image img = LoadImageFromTexture(canvas.output().texture);
            const double dt = (GetTime() - t0) * 1000;
            UnloadImage(img);
            best = std::min(best, dt);
            mean += dt / 12;
        }
        canvas.unload();
        std::printf("[weather] W3 timing 1080p, %s: best %.1f ms, mean %.1f ms; drops submitted %lld, splashes %lld\n", row.name, best, mean,
                    g3d::precipitationDropsSubmitted(), g3d::precipitationSplashesSubmitted());
    }
}

// (j) A lamp in rain: streaks near a point light are brighter than streaks far from it, falling off with distance. The sun is out of
// the scene. Break: the lamp's light term removed (here: the lamp switched off, which must bring the ratio to about 1).
static float lampRatio(float lampIntensity, float* nearMean = nullptr, float* farMean = nullptr, int* nNear = nullptr, int* nFar = nullptr) {
    constexpr int W = 640, H = 360;
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 1.6f, 10 };
    s->camera.target = { 0, 1.6f, 0 };
    s->camera.fov = 70;
    s->ambient = 0.02f;
    s->skyColor = WHITE; s->groundColor = WHITE;
    s->background = BLACK;
    if (lampIntensity > 0) s->lights.push_back(g3d::Light::point({ 0, 1.6f, 0 }, WHITE, lampIntensity, 40));
    g3d::Downfall d;
    d.enabled = true; d.kind = g3d::Precipitation::Rain; d.intensity = 1;
    d.rainDensity = 120; d.rainSize = { 0.02f, 0.03f };
    d.farLayer = false; d.splashes = false; d.radius = 40; d.opacity = 1;
    s->precipitation = d;
    Image img = look(s, W, H, 5);
    const std::vector<float> v = lumaOf(img);
    if (std::getenv("RMTN_W3_DEBUG") && lampIntensity > 0) ExportImage(img, "out/w3/lamp_test.png");
    UnloadImage(img);
    const g3d::Camera& cam = s->camera;
    const Vector3 f = Vector3Normalize(Vector3Subtract(cam.target, cam.position)), r = Vector3Normalize(Vector3CrossProduct(f, cam.upVector)), u = Vector3CrossProduct(r, f);
    const float ty = std::tan(cam.fov * 0.5f * DEG2RAD), tx = ty * (float)W / H, S = g3d::Downfall::kTile, Hc = g3d::Downfall::kCell;
    double sum[2] = {}; int n[2] = {};
    for (int iy = -1; iy <= 0; iy++)
        for (int iz = -6; iz <= 2; iz++)
            for (int ix = -6; ix <= 5; ix++)
                for (uint32_t k = 0; k < d.dropsPerTile(); k++) {
                    const g3d::Downfall::Drop p = d.drop(ix, iy, iz, k, 5 / 30.0);
                    if (p.rank > 0.5f) continue;
                    const Vector3 rel = Vector3Subtract(p.position, cam.position);
                    const float z = Vector3DotProduct(rel, f);
                    if (z < 2) continue;
                    const float sx = Vector3DotProduct(rel, r) / (z * tx), sy = Vector3DotProduct(rel, u) / (z * ty);
                    const int px = (int)((sx * 0.5f + 0.5f) * W), py = (int)((0.5f - sy * 0.5f) * H);
                    if (px < 4 || py < 4 || px >= W - 4 || py >= H - 4) continue;
                    const float dl = Vector3Distance(p.position, { 0, 1.6f, 0 });
                    const float dc = Vector3Length(rel);                  // both groups lie 8-12 m from the camera, so that distance does not decide the brightness
                    if (dc < 8.f || dc > 12.f) continue;
                    const int bin = dl < 2 ? 0 : (dl > 8 ? 1 : -1);
                    if (bin < 0) continue;
                    float best = 0;
                    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) best = std::max(best, v[(size_t)(py + dy) * W + px + dx]);
                    if (std::getenv("RMTN_W3_DEBUG") && lampIntensity > 0 && bin == 0) { static double hs[4]; static int hn[4]; const int hb = std::min(3, (int)(dl * 2)); hs[hb] += best; hn[hb]++; if (hn[0] + hn[1] + hn[2] + hn[3] == 99999) std::printf("lamp bins: %.0f(%d) %.0f(%d) %.0f(%d) %.0f(%d)\n", hs[0] / std::max(hn[0], 1), hn[0], hs[1] / std::max(hn[1], 1), hn[1], hs[2] / std::max(hn[2], 1), hn[2], hs[3] / std::max(hn[3], 1), hn[3]); }
                    sum[bin] += best; n[bin]++;
                }
    const float a = n[0] ? (float)(sum[0] / n[0]) : 0.f, b = n[1] ? (float)(sum[1] / n[1]) : 1.f;
    if (nearMean) *nearMean = a;
    if (farMean) *farMean = b;
    if (nNear) *nNear = n[0];
    if (nFar) *nFar = n[1];
    return a / std::max(b, 1.f);
}
static void lampGlints() {
    float a, b, ao, bo; int na, nb, nao, nbo;
    const float ratio = lampRatio(3.2f, &a, &b, &na, &nb), off = lampRatio(0, &ao, &bo, &nao, &nbo);
    std::printf("[weather] lamp in rain: mean streak brightness %.1f within 2 m of the lamp (%d streaks), %.1f beyond 8 m (%d): ratio %.2f; lamp off: %.1f vs %.1f, ratio %.2f\n",
                a, na, b, nb, ratio, ao, bo, off);
    CHECK(na > 12 && nb > 40);
    CHECK(ratio >= 2.5f);
    CHECK(off < 1.6f);
}

// (k) A flake is lit, not glowing: a flake's colour on a grey sky is the lit-white expectation from the scene's own light (albedo 0.9,
// the sky's ambient and a white light straight behind the camera), within 15% and not above it; the mean chroma under white light is low.
// Break: an additive glow on the flake (it lands above the expectation) or a warm tint (chroma).
static void snowIsLit() {
    const float I = 0.3f, amb = 0.25f;
    g3d::Downfall d = testRain(0.3f);
    d.kind = g3d::Precipitation::Snow;
    d.opacity = 1;
    d.flutter = 0;
    auto s = dropScene(d);
    s->ambient = amb;
    s->skyColor = { 160, 160, 160, 255 }; s->groundColor = { 160, 160, 160, 255 };
    s->background = { 150, 150, 150, 255 };
    g3d::Light l = g3d::Light::directional({ 0, 1.5f, 8 }, WHITE, I);
    l.target = { 0, 1.5f, 0 };
    s->lights.push_back(l);
    Image img = look(s, PW, PH, 5);
    const float skyLin = std::pow(160 / 255.f, 2.2f) * amb;
    const float expectLin = 0.9f * (skyLin + I);
    const float expectDisp = std::pow(expectLin, 1 / 2.2f) * 255;
    std::vector<float> ys; double chroma = 0, lum = 0; int n = 0;
    for (int iy = -1; iy <= 0; iy++)
        for (int iz = -1; iz <= 1; iz++)
            for (int ix = -2; ix <= 1; ix++)
                for (uint32_t k = 0; k < d.dropsPerTile(); k++) {
                    const g3d::Downfall::Drop p = d.drop(ix, iy, iz, k, 5 / 30.0);
                    const float dist = Vector3Distance(p.position, { 0, 1.5f, 8 });
                    if (p.rank > 0.5f || dist < 3.f || p.position.z > 5.5f) continue;
                    const int px = (int)std::floor(PW * 0.5f + p.position.x * P_PPM), py = (int)std::floor(PH * 0.5f - (p.position.y - 1.5f) * P_PPM);
                    if (px < 12 || py < 12 || px >= PW - 12 || py >= PH - 12) continue;
                    const Color c = GetImageColor(img, px, py);
                    // undo the blend over the sky: pixel = 0.9 flake + 0.1 sky
                    float ch[3] = { (c.r - 15.f) / 0.9f, (c.g - 15.f) / 0.9f, (c.b - 15.f) / 0.9f };
                    ys.push_back(0.2126f * ch[0] + 0.7152f * ch[1] + 0.0722f * ch[2]);
                    chroma += std::max({ ch[0], ch[1], ch[2] }) - std::min({ ch[0], ch[1], ch[2] });
                    lum += ys.back(); n++;
                }
    UnloadImage(img);
    std::sort(ys.begin(), ys.end());
    const float med = ys.empty() ? 0.f : ys[ys.size() / 2];
    const float meanChroma = n ? (float)(chroma / std::max(lum, 1.0)) : 1.f;
    std::printf("[weather] snow lit: median flake colour %.1f of 255 over %d flakes, expected %.1f (albedo 0.9 x (sky + light)); chroma %.3f of the mean\n", med, n, expectDisp, meanChroma);
    CHECK(n > 25);
    CHECK(med > 0.85f * expectDisp && med < 1.15f * expectDisp);
    CHECK(meanChroma < 0.06f);
}

// (l) Splashes are sparks, not blobs: seen from a porch-like camera at 1080p each splash is a few bright pixels (a sparse crown and a thin
// ring), so the mean area of a connected splash component is at most 6 px and no splash pixel is more than 60% opaque. The alpha comes
// from rendering over a white and a black ground (unlit): pixel = c a + ground (1 - a), so a = 1 - (white - black) / 255. Break: the old blobs (min 5 px, alpha 0.8).
static void splashesAreSparks() {
    constexpr int W = 1920, H = 1080;
    int frame = 11;
    auto render = [&](bool splashes, Color bg, std::vector<float>& out) {
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 1.55f, 10 };
        s->camera.target = { 0, -3.3f, 0 };
        s->camera.fov = 48;
        s->ambient = 0.6f;
        s->skyColor = WHITE; s->groundColor = WHITE;
        s->background = bg;
        g3d::Material ground;
        ground.color = bg; ground.unlit = true;
        s->add(g3d::Geometry::plane(300, 300), ground);
        g3d::Downfall d;
        d.enabled = true; d.kind = g3d::Precipitation::Rain; d.intensity = 1;
        d.splashes = splashes; d.farLayer = false; d.rainDensity = 0.4f;     // a few splashes a square metre, so they seldom touch
        s->precipitation = d;
        Image img = look(s, W, H, frame);
        out.resize((size_t)W * H);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) out[(size_t)y * W + x] = (float)GetImageColor(img, x, y).g;
        UnloadImage(img);
    };
    int comps = 0; long long area = 0, litPx = 0; float maxA = 0; std::vector<float> alphas;
    for (frame = 11; frame < 11 + 14 * 5; frame += 5) {                // fourteen frames, so there are enough splashes to measure
    std::vector<float> onW, onB, offW, offB;
    render(true, WHITE, onW); render(true, BLACK, onB); render(false, WHITE, offW); render(false, BLACK, offB);
    std::vector<float> a((size_t)W * H);

    for (size_t i = 0; i < a.size(); i++) {
        const float on = 1 - (onW[i] - onB[i]) / 255.f, off = 1 - (offW[i] - offB[i]) / 255.f;
        a[i] = on - off;
        maxA = std::max(maxA, a[i]);
        if (a[i] > 0.03f) alphas.push_back(a[i]);
    }
    std::vector<char> seen(a.size(), 0);
    if (std::getenv("RMTN_W3_DEBUG")) { Image dbg = GenImageColor(W, H, BLACK); for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { const int v = (int)std::clamp(a[(size_t)y * W + x] * 1000.f, 0.f, 255.f); ImageDrawPixel(&dbg, x, y, { (unsigned char)v, (unsigned char)v, (unsigned char)v, 255 }); } ExportImage(dbg, "out/w3/spark_alpha.png"); UnloadImage(dbg); }
    std::vector<int> stack;
    for (int y = H / 10; y < H * 3 / 5; y++)           // the ground 3-13 m away, where a porch camera sees the path
        for (int x = 0; x < W; x++) {
            const size_t i = (size_t)y * W + x;
            if (a[i] <= 0.03f) continue;
            litPx++;
            if (seen[i]) continue;
            comps++;
            stack.assign(1, (int)i); seen[i] = 1;
            while (!stack.empty()) {
                const int j = stack.back(); stack.pop_back();
                area++;
                const int jx = j % W, jy = j / W;
                for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                    const int nx = jx + dx, ny = jy + dy;
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    const size_t k = (size_t)ny * W + nx;
                    if (!seen[k] && a[k] > 0.03f) { seen[k] = 1; stack.push_back((int)k); }
                }
            }
        }
    }
    const float mean = comps ? (float)area / comps : 0.f;
    std::printf("[weather] splash sparks at 1080p: %d components, mean area %.1f px, %lld splash pixels, strongest splash alpha %.2f\n", comps, mean, litPx, maxA);
    CHECK(comps > 25);
    CHECK(mean <= 6.f);
    std::sort(alphas.begin(), alphas.end());
    const float p999 = alphas.empty() ? 0.f : alphas[(size_t)((alphas.size() - 1) * 0.999)];
    std::printf("[weather] splash alpha: 99.9th percentile %.2f, maximum %.2f (the maximum is where a splash's ring and droplets, or two splashes, overlap)\n", p999, maxA);
    CHECK(p999 <= 0.6f);
    CHECK(maxA <= 0.8f);
}

// (m) Far snow is a veil, not speckle: with the whole snowfall on against a black sky, the per-pixel standard deviation that snow adds,
// on a 4 x 4 downsample, stays under a bound. Break: sub-pixel flakes clamped to a 2 px dot (the old rule), which is all speckle.
static constexpr double SNOW_VEIL_SD = 4.5;     // fine snow: 2.4 as built, 7.3 with every sub-pixel flake drawn as an opaque 2 px dot
static void snowIsAVeil() {
    constexpr int W = 960, H = 540;
    auto render = [&](bool on) {
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 1.6f, 0 };
        s->camera.target = { 0, 1.6f, -10 };
        s->camera.fov = 50;
        s->ambient = 0.25f;
        s->skyColor = { 160, 160, 160, 255 }; s->groundColor = { 160, 160, 160, 255 };
        s->background = BLACK;
        g3d::Downfall d;
        d.enabled = on; d.kind = g3d::Precipitation::Snow; d.intensity = 1;
        d.splashes = false;
        d.snowSize = { 0.002f, 0.005f };               // fine snow: almost every flake is under a pixel
        s->precipitation = d;
        Image img = look(s, W, H, 9);
        std::vector<float> v = lumaOf(img);
        UnloadImage(img);
        return v;
    };
    const std::vector<float> on = render(true), off = render(false);
    const int w4 = W / 4, h4 = H / 4;
    std::vector<double> d4((size_t)w4 * h4, 0.0);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) d4[(size_t)(y / 4) * w4 + x / 4] += (on[(size_t)y * W + x] - off[(size_t)y * W + x]) / 16.0;
    double mean = 0, var = 0;
    for (double v : d4) mean += v;
    mean /= d4.size();
    for (double v : d4) var += (v - mean) * (v - mean);
    const double sd = std::sqrt(var / d4.size());
    std::printf("[weather] snow veil: mean added %.1f, per-pixel stddev on the 4x downsample %.1f (of 255)\n", mean, sd);
    CHECK(sd < SNOW_VEIL_SD);
}

static void precipitation() {
    roofKeepsRainOff();
    intensityScales();
    windSlants();
    worldFixed();
    streakLength();
    mirrorsRain();
    snowSways();
    precipitationPurity();
    precipitationOff();
    splashesLand();
    lampGlints();
    snowIsLit();
    splashesAreSparks();
    snowIsAVeil();
}


// ================================================================ W4: wet ground and settling snow
// A uniform bright environment (sky = ground colour), so what a surface reflects does not depend on where it faces.
static constexpr int WW = 320, WH = 180;
static g3d::ScenePtr w4Base(float ambient = 1.f) {
    auto s = std::make_shared<g3d::Scene>();
    s->skyColor = { 215, 218, 225, 255 };
    s->groundColor = s->skyColor;
    s->ambient = ambient;
    return s;
}
static float roiLuma(const Image& img, int x0, int y0, int x1, int y1) {
    double sum = 0;
    int n = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) { sum += luma(GetImageColor(img, x, y)); n++; }
    return n ? (float)(sum / n) : 0.f;
}
static float peakDiff(const Image& a, const Image& b, int x0, int y0, int x1, int y1) {
    float m = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            const Color p = GetImageColor(a, x, y), q = GetImageColor(b, x, y);
            m = std::max(m, (float)std::max({ std::abs(p.r - q.r), std::abs(p.g - q.g), std::abs(p.b - q.b) }));
        }
    return m;
}

// (a) A puddle where the ground is flat, none on a 60 degree slope: the same plane, once flat and once tipped, seen
// from the same angle against its surface (the camera turns with it) in a uniform environment.
static float tiltedGain(float degrees, float puddles) {
    auto make = [&](float wet) {
        auto s = w4Base();
        s->ground.wetness = wet;
        s->ground.puddles = puddles;
        s->ground.puddleSize = 2;
        g3d::Material m;
        m.color = { 70, 70, 74, 255 };
        m.roughness = 0.8f;
        g3d::Object3D* p = s->add(g3d::Geometry::plane(60, 60), m);
        p->rotation = { degrees, 0, 0 };
        const Matrix r = MatrixRotateX(degrees * DEG2RAD);
        s->camera.position = Vector3Transform({ 0, 1.2f, 3.0f }, r);
        s->camera.target = Vector3Transform({ 0, 0, -6 }, r);
        s->camera.upVector = Vector3Transform({ 0, 1, 0 }, r);
        s->camera.fov = 50;
        return s;
    };
    Image dry = look(make(0), WW, WH, 3), wet = look(make(1), WW, WH, 3);
    const float g = roiLuma(wet, 60, 100, 260, 170) - roiLuma(dry, 60, 100, 260, 170);
    dump(degrees < 1 ? (puddles > 0 ? "w4-puddle-flat.png" : "w4-puddle-off.png") : "w4-puddle-slope.png", wet);
    UnloadImage(dry); UnloadImage(wet);
    return g;
}
static void puddleOnFlatNotSlope() {
    const float flat = tiltedGain(0, 2), slope = tiltedGain(60, 2), noPuddles = tiltedGain(0, 0);
    std::printf("[weather] wet gain in mean luma: flat plane %.1f, 60 degree slope %.1f, flat with puddles off %.1f\n", flat, slope, noPuddles);
    CHECK(flat > 25.f);                         // the film of standing water reflects the sky
    CHECK(slope < 0.35f * flat);                // the slope is damp (darker, a faint film) but holds no puddle
    CHECK(noPuddles < 0.5f * flat);
}

// (b) Rain rings. A top-down view of a puddle that fills the plane, drawing the surface normal as colour (Ground::debug).
// The CPU twin says where the rings are.
static g3d::ScenePtr ringScene(float rate, float cx, float cz) {
    auto s = std::make_shared<g3d::Scene>();
    s->ground.wetness = 1; s->ground.puddles = 2; s->ground.debug = 1; s->ground.rippleRate = rate; s->ground.seed = 1;
    s->precipitation.enabled = true;
    s->precipitation.kind = g3d::Precipitation::Rain;
    s->precipitation.intensity = 1;
    s->precipitation.splashes = false;
    s->precipitation.occlusion = false;
    s->precipitation.farLayer = false;
    s->precipitation.radius = 2;
    s->precipitation.rainDensity = 0;
    s->precipitation.seed = 11;
    s->camera.position = { cx, 5, cz }; s->camera.target = { cx, 0, cz }; s->camera.upVector = { 0, 0, -1 };
    s->camera.orthographic = true; s->camera.orthoHeight = 1.2f;
    g3d::Material m;
    m.color = { 90, 90, 90, 255 };
    s->add(g3d::Geometry::plane(60, 60), m);
    return s;
}
// The tilt of the debug colour (0.5 + n.x, 0.5 + n.z): the normal's x and z at a pixel.
static Vector2 tiltAt(const Image& img, int x, int y) {
    const Color c = GetImageColor(img, x, y);
    return { c.r / 255.f - 0.5f, c.g / 255.f - 0.5f };
}
// Every ring that can reach the view round (cx, cz): the twin's rings from nine points, without repeats.
static std::vector<g3d::Ground::Ring> ringsNear(const g3d::Ground& g, float cx, float cz, double t, float rate) {
    std::vector<g3d::Ground::Ring> all;
    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++)
            for (const auto& r : g.rings(cx + 0.7f * i, cz + 0.7f * j, t, rate, 11)) {
                bool dup = false;
                for (const auto& a : all) dup = dup || (std::fabs(a.centre.x - r.centre.x) < 1e-5f && std::fabs(a.centre.y - r.centre.y) < 1e-5f);
                if (!dup) all.push_back(r);
            }
    return all;
}
static float s_ringStretch = 1;       // 1: round; another value makes the roundness check measure an ellipse (to show it fails)
static void ringsRoundAndSpaced() {
    // CPU: alive rings a square metre are rate x the ring's life, counted over 400 cells and 8 moments.
    for (float rate : { 1.f, 3.f, 6.f }) {
        g3d::Ground g;
        g.seed = 5;
        long long n = 0;
        int samples = 0;
        for (int k = 0; k < 8; k++)
            for (int i = 0; i < 20; i++)
                for (int j = 0; j < 20; j++) {
                    const float x = (i + 0.5f) * g3d::Ground::kCell, z = (j + 0.5f) * g3d::Ground::kCell;
                    for (const auto& r : g.rings(x, z, 3.1 + 2.3 * k, rate))
                        if (std::floor(r.centre.x / g3d::Ground::kCell) == i && std::floor(r.centre.y / g3d::Ground::kCell) == j) n++;
                    samples++;
                }
        const float density = (float)n / (samples * g3d::Ground::kCell * g3d::Ground::kCell), want = rate * g3d::Ground::kPeriod;
        std::printf("[weather] rings alive per m2 at rate %.0f: %.2f (rate x life %.2f)\n", rate, density, want);
        CHECK(std::fabs(density / want - 1.f) < 0.25f);
    }
    // The picture: one ring alone in the view, found by the CPU twin, is round and as wide as the speed says.
    const float cx = 0.37f, cz = 0.41f;
    g3d::Ground g;
    g.seed = 1;
    int found = -1;
    float age = 0;
    Vector2 centre{ 0, 0 };
    for (int f = 0; f < 9000 && found < 0; f++) {
        const auto rs = ringsNear(g, cx, cz, f / 30.0, 0.4f);
        int inside = 0;
        bool good = false;
        Vector2 gc{ 0, 0 };
        float ga = 0;
        for (const auto& r : rs) {
            const float d = Vector2Distance(r.centre, { cx, cz });
            if (d < 1.1f) inside++;
            if (d < 0.3f && r.age > 0.35f && r.age < 0.75f) { good = true; gc = r.centre; ga = r.age; }
        }
        if (good && inside == 1) { found = f; age = ga; centre = gc; }
    }
    CHECK(found >= 0);
    if (found < 0) return;
    if (std::getenv("RMTN_RING_DEBUG"))
        for (const auto& r : ringsNear(g, cx, cz, found / 30.0, 0.4f))
            std::printf("ring centre px (%.0f, %.0f) age %.2f\n", (r.centre.x - cx) / (1.2f / (WH * 2)) + WW, (r.centre.y - cz) / (1.2f / (WH * 2)) + WH, r.age);
    Image img = look(ringScene(0.4f, cx, cz), WW * 2, WH * 2, found);
    dump("w4-ring.png", img);
    const float m = 1.2f / (WH * 2), r0 = g3d::Ground::kSpeed * age;
    const float px = (centre.x - cx) / m + WW, py = (centre.y - cz) / m + WH;
    std::vector<float> radii;
    for (int a = 0; a < 48; a++) {
        const float th = a * 2 * PI / 48, ca = std::cos(th), sa = std::sin(th) * s_ringStretch;
        float best = 0, at = 0;
        for (float r = 0.4f * r0; r < 1.5f * r0; r += m * 0.5f) {
            const int x = (int)std::lround(px + ca * r / m), y = (int)std::lround(py + sa * r / m);
            if (x < 0 || y < 0 || x >= img.width || y >= img.height) { at = -1; break; }
            const Vector2 t = tiltAt(img, x, y);
            const float v = std::fabs(t.x * std::cos(th) + t.y * std::sin(th));
            if (v > best) { best = v; at = r; }
        }
        if (at > 0 && best > 0.03f) radii.push_back(at);
    }
    double mean = 0, var = 0;
    for (float r : radii) mean += r;
    mean /= std::max<size_t>(radii.size(), 1);
    for (float r : radii) var += (r - mean) * (r - mean);
    const double spread = radii.empty() ? 1.0 : std::sqrt(var / radii.size()) / mean;
    std::printf("[weather] one ring at age %.2f s: radius %.3f m expected %.3f, over %zu of 48 angles, spread %.3f of the radius\n", age, mean, r0, radii.size(), spread);
    CHECK(radii.size() >= 40);
    CHECK(std::fabs(mean / r0 - 1.f) < 0.06f);
    CHECK(spread < 0.045);
    UnloadImage(img);
    // The rate shows in the picture: three times the rate, about three times the tilt (averaged over twelve moments).
    auto energy = [&](float rate) {
        double e = 0;
        for (int k = 0; k < 12; k++) {
            Image im = look(ringScene(rate, cx, cz), WW, WH, 40 + 37 * k);
            for (int y = 0; y < im.height; y++)
                for (int x = 0; x < im.width; x++) { const Vector2 t = tiltAt(im, x, y); e += std::fabs(t.x) + std::fabs(t.y); }
            UnloadImage(im);
        }
        return e / 12;
    };
    const double e1 = energy(1.f), e3 = energy(3.f), e0 = energy(0.f);
    std::printf("[weather] ring tilt energy: none %.0f, rate 1 %.0f, rate 3 %.0f (ratio %.2f)\n", e0, e1, e3, (e3 - e0) / std::max(e1 - e0, 1.0));
    CHECK(e1 > e0 + 250);
    CHECK((e3 - e0) / (e1 - e0) > 2.2 && (e3 - e0) / (e1 - e0) < 3.9);
}

// (c) Snow is zero at the start and never falls; the top of a cube whitens, its sides do not.
static void snowSettlesOnTops() {
    g3d::Weather w = g3d::Weather::snow();
    w.precipitation.intensity = 0.6f;
    w.accumulationScale = 500;
    double prev = -1, end = 0;
    const double d0 = w.snowDepthAt(0);
    bool mono = true;
    for (int i = 0; i <= 80; i++) {
        const double d = w.snowDepthAt(i * 0.25);
        mono = mono && d >= prev;
        prev = d;
        end = d;
    }
    std::printf("[weather] snow depth: t = 0: %.4f m, 10 s: %.3f m, 20 s: %.3f m, nondecreasing %d\n", d0, w.snowDepthAt(10), end, mono ? 1 : 0);
    CHECK(d0 == 0 && mono && end > 0.04 && end < 0.12 && w.snowDepthAt(10) < end);
    // The drawing: a cube seen from above and the side, bare and then under 8 cm of snow.
    auto make = [](float cover) {
        auto s = w4Base(0.6f);
        s->ground.snowCover = cover;
        s->lights.push_back(g3d::Light::directional({ -3, 6, 4 }, WHITE, 1.2f));
        g3d::Material m;
        m.color = { 150, 80, 60, 255 };
        s->add(g3d::Geometry::box(1.5f, 1.5f, 1.5f), m)->position = { 0, 0.75f, 0 };
        s->camera.position = { 2.6f, 3.2f, 3.6f }; s->camera.target = { 0, 0.7f, 0 }; s->camera.fov = 45;
        return s;
    };
    Image bare = look(make(0), WW, WH, 5);
    float topPrev = -1;
    bool rises = true;
    for (float cover : { 0.1f, 0.3f, 0.6f, 0.9f }) {
        Image im = look(make(cover), WW, WH, 5);
        const float top = roiLuma(im, 130, 52, 200, 70);
        rises = rises && top >= topPrev - 1;
        topPrev = top;
        UnloadImage(im);
    }
    Image snowy = look(make(0.9f), WW, WH, 5);
    dump("w4-cube-bare.png", bare); dump("w4-cube-snow.png", snowy);
    const float topBare = roiLuma(bare, 130, 52, 200, 70), topSnow = roiLuma(snowy, 130, 52, 200, 70);
    const float sideBare = roiLuma(bare, 100, 110, 140, 135), sideSnow = roiLuma(snowy, 100, 110, 140, 135);
    const float side2Bare = roiLuma(bare, 175, 110, 215, 135), side2Snow = roiLuma(snowy, 175, 110, 215, 135);
    std::printf("[weather] cube top %.0f -> %.0f; sides %.0f -> %.0f and %.0f -> %.0f; top rises with cover %d\n", topBare, topSnow, sideBare, sideSnow, side2Bare, side2Snow, rises ? 1 : 0);
    CHECK(topSnow > topBare + 50);
    CHECK(std::fabs(sideSnow - sideBare) < 4 && std::fabs(side2Snow - side2Bare) < 4);
    CHECK(rises);
    UnloadImage(bare); UnloadImage(snowy);
}

// (d) Wetness soaks in while it rains and dries when it stops: it never rises after the rain, and is under 0.1 by dryingTime().
static void wetnessDries() {
    g3d::Weather w = g3d::Weather::rain();
    w.accumulationScale = 20;
    w.history = { { 0, g3d::Precipitation::Rain, 1 }, { 60, g3d::Precipitation::None, 0 } };
    w.precipitation.kind = g3d::Precipitation::None;
    w.precipitation.intensity = 0;
    const float dryFilm = w.dryingTime() / w.accumulationScale;                   // film seconds
    float prev = -1;
    const float wetAtStop = w.wetnessAt(60);
    bool risesWhileRaining = true, neverRises = true;
    for (int i = 0; i <= 60; i++) { const float v = w.wetnessAt(i); risesWhileRaining = risesWhileRaining && v >= prev; prev = v; }
    for (int i = 0; i <= 240; i++) { const float v = w.wetnessAt(60 + i * dryFilm / 100.f); neverRises = neverRises && v <= prev + 1e-6f; prev = v; }
    const float atDry = w.wetnessAt(60 + dryFilm), half = w.wetnessAt(60 + 0.4f * dryFilm);
    std::printf("[weather] wetness: %.2f when the rain stops (60 s), %.2f at 0.4 x the drying time, %.3f at the drying time (%.0f weather s = %.1f film s); never rises %d\n",
                wetAtStop, half, atDry, w.dryingTime(), dryFilm, neverRises ? 1 : 0);
    CHECK(risesWhileRaining && neverRises);
    CHECK(wetAtStop > 0.9f);
    CHECK(atDry < 0.1f && half > 0.1f);
    // Wind dries it faster; a pin holds.
    g3d::Weather windy = w, calm = w;
    windy.wind.speed = 20; calm.wind.speed = 0;
    CHECK(windy.dryingTime() < calm.dryingTime());
    g3d::Weather pinned = w;
    pinned.wetness = 0.4f;
    CHECK(pinned.wetnessAt(1) == 0.4f && pinned.wetnessAt(500) == 0.4f);
    // Snow melts above freezing.
    g3d::Weather sn = g3d::Weather::snow();
    sn.accumulationScale = 500;
    sn.history = { { 0, g3d::Precipitation::Snow, 1 }, { 10, g3d::Precipitation::None, 0 } };
    sn.temperature = -2;
    const float cold = sn.snowDepthAt(30);
    sn.temperature = 6;
    const float warm = sn.snowDepthAt(30);
    std::printf("[weather] snow depth after 30 s: %.3f m at -2 C, %.3f m at +6 C\n", cold, warm);
    CHECK(cold > 0.03f && warm < 0.5f * cold);
}

// A street: a wet plane, two boxes, rain with a history, under a grey sky; the camera low.
static g3d::ScenePtr streetScene(bool reflections, bool dryGround = false, float puddles = 1) {
    auto s = w4Base(0.8f);
    s->skyColor = { 150, 160, 180, 255 };
    s->groundColor = { 90, 90, 95, 255 };
    s->weather = g3d::Weather::rain();
    s->weather.accumulationScale = 400;
    s->weather.history = { { 0, g3d::Precipitation::Rain, 0.8f } };
    s->ground.reflections = reflections;
    s->ground.puddleSize = 2.5f;
    s->ground.puddles = puddles;
    if (dryGround) s->ground.wetness = 0;
    s->precipitation.enabled = true;
    s->precipitation.splashes = false;
    s->precipitation.farLayer = false;
    s->precipitation.opacity = 0;
    s->lights.push_back(g3d::Light::directional({ -4, 6, 3 }, { 255, 235, 210, 255 }, 1.0f));
    g3d::Material m;
    m.color = { 120, 118, 116, 255 };
    m.roughness = 0.9f;
    s->add(g3d::Geometry::plane(80, 80), m);
    g3d::Material red = m, blue = m;
    red.color = { 230, 30, 20, 255 };
    blue.color = { 30, 60, 220, 255 };
    s->add(g3d::Geometry::box(0.8f, 1.6f, 0.8f), red)->position = { -0.8f, 0.8f, -4 };
    s->add(g3d::Geometry::box(0.8f, 1.2f, 0.8f), blue)->position = { 1.0f, 0.6f, -5.5f };
    s->camera.position = { 0, 0.55f, 2.5f }; s->camera.target = { 0, 0.3f, -4 }; s->camera.fov = 50;
    return s;
}

// (e) Purity: a frame alone equals the same frame after others, with the accumulators, rings and the reflection all on.
static g3d::ScenePtr visibleRain(g3d::ScenePtr s) { s->precipitation.opacity = 1; return s; }
static void groundPurity() {
    Image alone = look(visibleRain(streetScene(true)), WW, WH, 200);
    Comp c = comp(visibleRain(streetScene(true)), WW, WH);
    Image a = render(c, 20), b = render(c, 400), after = render(c, 200), other = render(c, 260);
    const long long same = diffImages(alone, after), differs = diffImages(after, other);
    std::printf("[weather] ground purity: frame 200 alone vs after frames 20, 400: diff %lld; frame 260 vs 200: diff %lld\n", same, differs);
    CHECK(same == 0 && differs > 2000);
    g3d::Weather w = streetScene(true)->weather;
    const float w1 = w.wetnessAt(7.3);
    (void)w.wetnessAt(1.0);
    CHECK(w.wetnessAt(7.3) == w1);
    UnloadImage(alone); UnloadImage(a); UnloadImage(b); UnloadImage(after); UnloadImage(other);
}

// (f) Scenes with no ground state are untouched: configuring the ground changes nothing while it holds no wetness or snow.
static void groundOffMeansOff() {
    auto base = [](bool weather, bool configure, bool pinZero) {
        auto s = w4Base(0.7f);
        if (weather) s->weather = g3d::Weather::clear();
        if (configure) { s->ground.puddles = 2; s->ground.rippleRate = 9; s->ground.seed = 77; s->ground.reflections = true; s->ground.puddleSize = 1; }
        if (pinZero) { s->ground.wetness = 0; s->ground.snowCover = 0; }
        s->lights.push_back(g3d::Light::directional({ -3, 5, 2 }, WHITE, 1.f));
        g3d::Material m;
        m.color = { 120, 140, 90, 255 };
        s->add(g3d::Geometry::plane(30, 30), m);
        s->add(g3d::Geometry::box(1, 1, 1), m)->position = { 0, 0.5f, -3 };
        s->camera.position = { 0, 1.6f, 4 }; s->camera.target = { 0, 0.5f, -3 };
        return s;
    };
    Image none = look(base(false, false, false), WW, WH, 4), set = look(base(false, true, false), WW, WH, 4);
    Image clear = look(base(true, false, false), WW, WH, 4), clearPinned = look(base(true, true, true), WW, WH, 4);
    const long long d1 = diffImages(none, set), d2 = diffImages(clear, clearPinned);
    // A weather that never rains or snows: wetness 0, depth 0.
    g3d::Weather w = g3d::Weather::overcast();
    const bool zero = w.wetnessAt(300) == 0 && w.snowDepthAt(300) == 0;
    std::printf("[weather] ground off: no weather vs ground configured: diff %lld; clear weather vs configured and pinned at 0: diff %lld; overcast accumulates nothing %d\n", d1, d2, zero ? 1 : 0);
    CHECK(d1 == 0 && d2 == 0 && zero);
    UnloadImage(none); UnloadImage(set); UnloadImage(clear); UnloadImage(clearPinned);
}

// (g) A material with weathering off stays dry and bare under the same weather; the default one changes.
static void weatheringOptOut() {
    auto make = [](bool ground) {
        auto s = w4Base(0.7f);
        if (ground) { s->ground.wetness = 1; s->ground.snowCover = 0.8f; s->ground.puddles = 0; }
        s->lights.push_back(g3d::Light::directional({ -3, 6, 3 }, WHITE, 1.f));
        g3d::Material m;
        m.color = { 110, 120, 140, 255 };
        s->add(g3d::Geometry::plane(30, 30), m);
        g3d::Material a = m, b = m;
        b.weathering = false;
        s->add(g3d::Geometry::box(1, 1, 1), a)->position = { -1.0f, 0.5f, 0 };
        s->add(g3d::Geometry::box(1, 1, 1), b)->position = { 1.0f, 0.5f, 0 };
        s->camera.position = { 0, 3.0f, 5 }; s->camera.target = { 0, 0.5f, 0 }; s->camera.fov = 40;
        return s;
    };
    Image dry = look(make(false), WW, WH, 2), wet = look(make(true), WW, WH, 2);
    dump("w4-optout.png", wet);
    const float kept = peakDiff(dry, wet, 190, 85, 225, 110), changed = peakDiff(dry, wet, 95, 85, 130, 110);
    std::printf("[weather] weathering off: opted-out box changes by at most %.0f levels, the default box by %.0f\n", kept, changed);
    CHECK(kept <= 1 && changed > 20);
    UnloadImage(dry); UnloadImage(wet);
}

// (h) A porous surface at full wetness has about 0.5-0.7 of the dry albedo under the same diffuse light, and a stronger
// sky reflection at a grazing view.
static void wetDarkens() {
    auto make = [](float wet, float elevation, Color albedo) {
        auto s = w4Base(1);
        s->ground.wetness = wet; s->ground.puddles = 0;
        s->skyColor = { 235, 240, 255, 255 };
        s->groundColor = s->skyColor;
        s->lights.push_back(g3d::Light::directional({ 3, 4, 0 }, WHITE, 1.f));    // off the view's axis: no highlight
        g3d::Material m;
        m.color = albedo;
        m.roughness = 0.9f;
        s->add(g3d::Geometry::plane(60, 60), m);
        const float e = elevation * DEG2RAD;
        s->camera.position = { 0, 4.f * std::sin(e), 4.f * std::cos(e) }; s->camera.target = { 0, 0, 0 }; s->camera.fov = 30;
        return s;
    };
    Image dryTop = look(make(0, 88, { 190, 180, 165, 255 }), WW, WH, 1), wetTop = look(make(1, 88, { 190, 180, 165, 255 }), WW, WH, 1);
    const float rTop = std::pow(roiLuma(wetTop, 120, 70, 200, 110) / roiLuma(dryTop, 120, 70, 200, 110), 2.2f);
    UnloadImage(dryTop); UnloadImage(wetTop);
    Image dryLow = look(make(0, 6, { 60, 55, 50, 255 }), WW, WH, 1), wetLow = look(make(1, 6, { 60, 55, 50, 255 }), WW, WH, 1);
    const float low0 = roiLuma(dryLow, 120, 100, 200, 140), low1 = roiLuma(wetLow, 120, 100, 200, 140);
    dump("w4-grazing-wet.png", wetLow);
    UnloadImage(dryLow); UnloadImage(wetLow);
    std::printf("[weather] wet porous ground: linear brightness x%.2f of dry looking down; grazing %.0f -> %.0f levels\n", rTop, low0, low1);
    CHECK(rTop > 0.5f && rTop < 0.7f);
    CHECK(low1 > low0 + 8);
}

// (i) The scene is reflected in wet ground: a red box stands on a wet street seen at a low angle; the glow below its
// base is reddish only when the reflection march is on (the sky reflection alone is not red).
static void wetReflectsScene() {
    Image on = look(streetScene(true, false, 2), WW, WH, 200), off = look(streetScene(false, false, 2), WW, WH, 200), dry = look(streetScene(true, true, 2), WW, WH, 200);
    dump("w4-street-ssr.png", on); dump("w4-street-nossr.png", off); dump("w4-street-dry.png", dry);
    auto redness = [](const Image& img, int x0, int x1, int y0, int y1) {
        double r = 0;
        int n = 0;
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++) { const Color c = GetImageColor(img, x, y); r += std::max(0, c.r - (c.g + c.b) / 2 - 12); n++; }
        return (float)(r / std::max(n, 1));
    };
    auto isRed = [](Color c) { return c.r > 150 && c.g < 90 && c.b < 90; };
    int base = -1;
    for (int y = WH - 1; y > 0 && base < 0; y--) {
        int hits = 0;
        for (int x = 0; x < WW; x++) if (isRed(GetImageColor(dry, x, y))) hits++;
        if (hits > 6) base = y;
    }
    CHECK(base > 0);
    if (base < 0) { UnloadImage(on); UnloadImage(off); UnloadImage(dry); return; }
    int xa = WW, xb = 0;
    for (int x = 0; x < WW; x++) if (isRed(GetImageColor(dry, x, base - 4))) { xa = std::min(xa, x); xb = std::max(xb, x); }
    const int y0 = base + 3, y1 = std::min(WH, base + 3 + (base - 40) / 2);
    const float redOn = redness(on, xa, xb, y0, y1), redOff = redness(off, xa, xb, y0, y1), redDry = redness(dry, xa, xb, y0, y1);
    std::printf("[weather] reddish glow below the red box (x %d..%d, rows %d..%d): reflection on %.1f, off %.1f, dry ground %.1f\n", xa, xb, y0, y1, redOn, redOff, redDry);
    CHECK(xb > xa);
    CHECK(redOn > redOff + 2.f && redOn > redDry + 2.f);
    UnloadImage(on); UnloadImage(off); UnloadImage(dry);
}

// (j) Under a roof the rain does not reach: no wetness there, while the open ground beside it changes.
static void shelterStaysDry() {
    auto make = [](bool ground, bool roof) {
        auto s = w4Base(0.8f);
        if (ground) { s->ground.wetness = 1; s->ground.snowCover = 0; s->ground.puddles = 0; }
        s->precipitation.enabled = true;
        s->precipitation.kind = g3d::Precipitation::Rain;
        s->precipitation.intensity = 0.3f;
        s->precipitation.opacity = 0;
        s->precipitation.splashes = false;
        s->precipitation.farLayer = false;
        s->precipitation.occlusion = true;
        g3d::Material m;
        m.color = { 170, 150, 130, 255 };
        s->add(g3d::Geometry::plane(40, 40), m);
        if (roof) s->add(g3d::Geometry::box(5, 0.1f, 20), m)->position = { -2.5f, 3.0f, 0 };
        s->camera.position = { 8, 1.2f, 0 }; s->camera.target = { -3, 0.3f, 0 }; s->camera.fov = 40;
        return s;
    };
    Image d = look(make(false, true), WW, WH, 3), w = look(make(true, true), WW, WH, 3), open = look(make(true, false), WW, WH, 3);
    dump("w4-shelter.png", w); dump("w4-shelter-open.png", open);


    // Seen along the street from its open side at 1.2 m: the ground under the roof (x -5..0, 8 to 13 m away) lies at rows 93 to 107.
    const float under = peakDiff(d, w, 90, 95, 230, 105), exposed = peakDiff(d, open, 90, 95, 230, 105);
    std::printf("[weather] shelter: the ground under a roof changes by %.0f levels when the ground gets wet, the same ground with no roof by %.0f\n", under, exposed);
    CHECK(exposed > 12);
    CHECK(under < 0.15f * exposed);
    UnloadImage(d); UnloadImage(w); UnloadImage(open);
}

// W4 cost at 1080p: the street scene with the ground at wetness 0 (the wet path off), wet with puddles and rings, and wet with the SSR pass.
static void groundTiming() {
    constexpr int W = 1920, H = 1080;
    struct Row { const char* name; bool reflections, dry; };
    const Row rows[] = { { "wetness 0 (dry, path off)", false, true }, { "wet, puddles and rings, no SSR", false, false }, { "wet + SSR", true, false } };
    for (const Row& row : rows) {
        Comp c = comp(streetScene(row.reflections, row.dry, 1), W, H);
        Canvas canvas;
        canvas.load(W, H, 1);
        presentFrame(c, 200, canvas);
        Image warm = LoadImageFromTexture(canvas.output().texture);
        UnloadImage(warm);
        double best = 1e9, mean = 0;
        for (int i = 0; i < 12; i++) {
            const double t0 = GetTime();
            presentFrame(c, 200 + i, canvas);
            Image img = LoadImageFromTexture(canvas.output().texture);
            const double dt = (GetTime() - t0) * 1000;
            UnloadImage(img);
            best = std::min(best, dt);
            mean += dt / 12;
        }
        canvas.unload();
        std::printf("[weather] W4 timing 1080p, %s: best %.1f ms, mean %.1f ms\n", row.name, best, mean);
    }
}

// A thin rail (6 cm deep, 8 cm tall) at 1 m, in snowfall with the rain's depth map on: its own top is the nearest thing in the
// map, so it must not shelter itself. The top whitens, the face towards the camera stays bare.
static g3d::ScenePtr railScene(float cover, bool shelter = true) {
    auto s = w4Base(0.2f);
    s->ground.snowCover = cover;
    s->precipitation.enabled = true;
    s->precipitation.kind = g3d::Precipitation::Snow;
    s->precipitation.intensity = 0.3f;
    s->precipitation.opacity = 0;
    s->precipitation.splashes = false;
    s->precipitation.farLayer = false;
    s->precipitation.occlusion = shelter;
    s->lights.push_back(g3d::Light::directional({ -3, 6, 4 }, WHITE, 0.45f));
    g3d::Material m;
    m.color = { 110, 70, 40, 255 };
    m.roughness = 0.9f;
    auto* rail = s->add(g3d::Geometry::box(4.f, 0.08f, 0.06f), m);
    rail->position = { 0, 1.0f, 0 };
    rail->rotation = { 25, 0, 0 };                            // rolled 25 degrees (a rail that has sagged on its post): the depth map then slopes over a texel
    s->camera.position = { 0, 1.9f, 1.7f }; s->camera.target = { 0, 1.0f, 0 }; s->camera.fov = 30;
    return s;
}
static void railTop() {
    Image bare = look(railScene(0), WW * 2, WH * 2, 5), snowy = look(railScene(0.8f), WW * 2, WH * 2, 5);
    dump("w4-rail-bare.png", bare); dump("w4-rail-snow.png", snowy);
    // The rail runs across the middle of the picture: find its top (the rows that brighten) and its front face (the rows that do not).
    const int x0 = 200, x1 = 440;
    int topRows = 0, faceRows = 0;
    float topLuma = 0;
    for (int y = 0; y < bare.height; y++) {
        const float a = roiLuma(bare, x0, y, x1, y + 1), b = roiLuma(snowy, x0, y, x1, y + 1);
        if (a < 12) continue;                                   // not the rail
        if (b > a + 50) { topRows++; topLuma = std::max(topLuma, b); }
        else if (std::fabs(b - a) < 6) faceRows++;
    }
    std::printf("[weather] rail under snow: %d pixel rows of its top whiten (by 50+ levels, brightest %.0f), %d rows of its front face keep their colour\n", topRows, topLuma, faceRows);
    CHECK(topRows >= 3);
    CHECK(faceRows >= 3);
    // Self-shelter: the same rail with the depth map off is the answer the map must agree with.
    Image open = look(railScene(0.8f, false), WW * 2, WH * 2, 5);
    float topOpen = 0;
    for (int y = 0; y < open.height; y++) topOpen = std::max(topOpen, roiLuma(open, 200, y, 440, y + 1));
    float topMap = 0;
    for (int y = 0; y < snowy.height; y++) topMap = std::max(topMap, roiLuma(snowy, 200, y, 440, y + 1));
    std::printf("[weather] rail top, brightest row: depth map on %.0f, depth map off %.0f\n", topMap, topOpen);
    CHECK(topMap > 0.97f * topOpen);
    UnloadImage(open);
    UnloadImage(bare); UnloadImage(snowy);
}

// Far ground at a grazing view: the puddle mask, the rings and the snow edge are noise a pixel there is far larger than, and
// must fade to their mean instead of aliasing. High-frequency energy = mean |pixel - 5x5 blur| over a band of rows.
static double hfEnergy(const Image& img, int y0, int y1) {
    const auto L = lumaOf(img);
    const int w = img.width, h = img.height;
    double e = 0;
    long n = 0;
    for (int y = std::max(y0, 2); y < std::min(y1, h - 2); y++)
        for (int x = 2; x < w - 2; x++) {
            double s = 0;
            for (int j = -2; j <= 2; j++)
                for (int i = -2; i <= 2; i++) s += L[(size_t)(y + j) * w + x + i];
            e += std::fabs(L[(size_t)y * w + x] - s / 25);
            n++;
        }
    return n ? e / n : 0;
}
static g3d::ScenePtr grazingGround(bool snow) {
    auto s = w4Base(0.6f);
    s->precipitation.enabled = true;
    s->precipitation.kind = snow ? g3d::Precipitation::Snow : g3d::Precipitation::Rain;
    s->precipitation.intensity = 1.f;
    s->precipitation.opacity = 0;
    s->precipitation.splashes = false;
    s->precipitation.farLayer = false;
    if (snow) s->ground.snowCover = 0.35f;
    else { s->ground.wetness = 1; s->ground.puddles = 1.4f; s->ground.puddleSize = 1.7f; s->ground.rippleRate = 3; }
    s->lights.push_back(g3d::Light::directional({ -3, 6, 4 }, WHITE, 1.2f));
    g3d::Material m;
    m.color = { 120, 105, 85, 255 };
    m.roughness = 0.9f;
    s->add(g3d::Geometry::plane(600, 600), m);
    s->camera.position = { 0, 1.6f, 0 }; s->camera.target = { 0, 0.9f, -20 }; s->camera.fov = 40;
    return s;
}
static void farGroundBandLimited() {
    for (int snow = 0; snow < 2; snow++) {
        Image img = look(grazingGround(snow != 0), WW * 2, WH * 2, 250);
        dump(snow ? "w4-far-snow.png" : "w4-far-wet.png", img);
        const int h0 = 2 * 81;                                   // the horizon is a little above the middle of the picture
        const int third = (img.height - h0) / 3;
        const double far = hfEnergy(img, h0 + 6, h0 + third), mid = hfEnergy(img, h0 + third, h0 + 2 * third), near = hfEnergy(img, h0 + 2 * third, img.height);
        std::printf("[weather] far %s ground, high-frequency energy: far third %.2f, middle %.2f, near %.2f levels\n", snow ? "snow" : "wet", far, mid, near);
        CHECK(far < 1.5);                                        // with the band limit off: 9 (wet) and 17 (snow)
        UnloadImage(img);
    }
}

// The edge of a shelter is a ramp: snow on the ground goes from the dusting under a slab to the full cover in the open over
// at least 0.3 m, with no row of the picture stepping by more than a fifth of the whole change.
static g3d::ScenePtr rampScene() {
    auto s = w4Base(0.25f);
    s->ground.snowCover = 0.9f;
    s->precipitation.enabled = true;
    s->precipitation.kind = g3d::Precipitation::Snow;
    s->precipitation.intensity = 0.3f;
    s->precipitation.opacity = 0;
    s->precipitation.splashes = false;
    s->precipitation.farLayer = false;
    s->precipitation.occlusion = true;
    s->lights.push_back(g3d::Light::directional({ -3, 6, 4 }, WHITE, 0.5f));
    g3d::Material m;
    m.color = { 90, 80, 60, 255 };
    m.roughness = 0.9f;
    s->add(g3d::Geometry::plane(60, 60), m);
    g3d::Material roof;
    roof.color = { 60, 60, 60, 255 };
    s->add(g3d::Geometry::box(30, 0.1f, 15), roof)->position = { 0, 3.0f, -7.5f };       // covers z -15..0
    s->camera.position = { 0, 7, 7 }; s->camera.target = { 0, 0, 0 }; s->camera.fov = 30;
    return s;
}
static void shelterRamp() {
    constexpr int PW = WW * 2, PH = WH * 2;
    Image img = look(rampScene(), PW, PH, 5);
    dump("w4-ramp.png", img);
    // Ground z for a row of the centre column: the ray of a pinhole camera at (0,7,7) looking at the origin.
    const float fy = 1.f / std::tan(15.f * DEG2RAD), pitch = std::atan2(7.f, 7.f);
    auto groundZ = [&](int row) {
        const float v = (PH * 0.5f - (row + 0.5f)) / (PH * 0.5f) / fy;          // tan of the angle above the axis
        const float ang = pitch - std::atan(v);                                   // angle below the horizon
        return ang > 0.01f ? 7.f - 7.f / std::tan(ang) : -1000.f;
    };
    std::vector<float> prof;
    std::vector<float> zs;
    for (int row = 0; row < PH; row++) {
        const float z = groundZ(row);
        if (z < -4.f || z > 4.f) continue;
        prof.push_back(roiLuma(img, 200, row, 440, row + 1));
        zs.push_back(z);
    }
    // The bare level under the slab (z -6..-2) and the full cover in the open (z 2..6).
    double lo = 0, hi = 0;
    int nlo = 0, nhi = 0;
    for (size_t i = 0; i < prof.size(); i++) {
        if (zs[i] < -2.f) { lo += prof[i]; nlo++; }
        if (zs[i] > 2.f) { hi += prof[i]; nhi++; }
    }
    lo /= std::max(nlo, 1); hi /= std::max(nhi, 1);
    float z10 = 1e9f, z90 = -1e9f, step = 0;
    for (size_t i = 0; i < prof.size(); i++) {
        const float s = (float)((prof[i] - lo) / std::max(hi - lo, 1.0));
        if (s > 0.1f) z10 = std::min(z10, zs[i]);
        if (s > 0.9f) z90 = std::max(z90, zs[i]);
        if (i > 0) step = std::max(step, std::fabs(s - (float)((prof[i - 1] - lo) / std::max(hi - lo, 1.0))));
    }
    // z10 is the nearest row above 10%, z90 the first row at 90%: take the first crossing of each going out from the slab.
    float first10 = 1e9f, first90 = 1e9f;
    for (size_t i = 0; i < prof.size(); i++) {
        const float s = (float)((prof[i] - lo) / std::max(hi - lo, 1.0));
        if (s > 0.1f) first10 = std::min(first10, zs[i]);
        if (s > 0.9f) first90 = std::min(first90, zs[i]);
    }
    if (std::getenv("RMTN_RAMP_DEBUG"))
        for (size_t i = 0; i < prof.size(); i++) std::printf("z %.2f luma %.0f\n", zs[i], prof[i]);
    std::printf("[weather] shelter edge: bare level %.0f, open level %.0f; 10%% at z %.2f, 90%% at z %.2f (width %.2f m); largest row step %.2f of the change\n", lo, hi, first10, first90, first90 - first10, step);
    CHECK(hi > lo + 40);
    CHECK(first90 - first10 >= 0.3f);
    CHECK(step < 0.25f);
    UnloadImage(img);
}

static void wetAndSnow() {
    puddleOnFlatNotSlope();
    ringsRoundAndSpaced();
    snowSettlesOnTops();
    wetnessDries();
    groundPurity();
    groundOffMeansOff();
    weatheringOptOut();
    wetDarkens();
    wetReflectsScene();
    shelterStaysDry();
    railTop();
    shelterRamp();
    farGroundBandLimited();
}

// ---------------------------------------------------------------- the spectrum sea (W9)
// A sea of the spectrum under a plain sky: wind towards +x at U m/s over `fetch` metres, no lights, no sky image.
static g3d::ScenePtr oceanScene(float U, float fetch, Vector3 pos, Vector3 target, float fov, int seed = 3) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = pos; s->camera.target = target; s->camera.fov = fov;
    s->lights.clear();
    g3d::Water w;
    w.enabled = true; w.waves.clear(); w.fetch = fetch; w.openDepth = 40; w.foam = 1; w.sunGlint = 0;
    w.spectrum.enabled = true; w.spectrum.windSpeed = U; w.spectrum.windDirection = 0; w.spectrum.seed = (unsigned)seed;
    s->water = w;
    return s;
}
// A top-down orthographic view of `span` metres (up is -z), the screen being a debug read-out of the water shader.
static g3d::ScenePtr oceanFromAbove(float U, float fetch, float span, int debug, Vector2 at = { 0, 0 }, float height = 3000) {
    auto s = oceanScene(U, fetch, { at.x, height, at.y }, { at.x, 0, at.y }, 30);
    s->camera.orthographic = true; s->camera.orthoHeight = span; s->camera.upVector = { 0, 0, -1 };
    s->camera.nearPlane = 1; s->camera.farPlane = height * 2;
    s->ground.debug = debug;
    return s;
}
static float linearOf(unsigned char v) { return std::pow(v / 255.f, 2.2f); }

// ---- a 2D FFT of a square power-of-two image (radix 2), for the tiling check
static void fft1d(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2 * 3.14159265358979323846 / (double)len;
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (size_t j = 0; j < len / 2; j++, w *= wl) {
                const std::complex<double> u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v;
            }
        }
    }
}
// Power of the picture's channel (a Hann window, the mean removed), summed over the other axis: P[b] for b = 0 .. N/2 - 1
// is the power at b cycles across the picture along x (alongX) or z, both signs.
static std::vector<double> axisPower(const Image& img, int channel, bool alongX) {
    const int N = img.width;
    std::vector<std::vector<std::complex<double>>> m((size_t)N, std::vector<std::complex<double>>((size_t)N));
    double mean = 0;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        const Color c = GetImageColor(img, x, y);
        mean += channel == 0 ? c.r : c.g;
    }
    mean /= (double)N * N;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        const Color c = GetImageColor(img, x, y);
        const double hann = 0.25 * (1 - std::cos(2 * 3.14159265358979323846 * (x + 0.5) / N)) * (1 - std::cos(2 * 3.14159265358979323846 * (y + 0.5) / N));
        m[(size_t)y][(size_t)x] = ((channel == 0 ? c.r : c.g) - mean) * hann;
    }
    for (int y = 0; y < N; y++) fft1d(m[(size_t)y]);
    std::vector<std::complex<double>> col((size_t)N);
    for (int x = 0; x < N; x++) {
        for (int y = 0; y < N; y++) col[(size_t)y] = m[(size_t)y][(size_t)x];
        fft1d(col);
        for (int y = 0; y < N; y++) m[(size_t)y][(size_t)x] = col[(size_t)y];
    }
    std::vector<double> P((size_t)N / 2, 0.0);
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        const int b = alongX ? (x <= N / 2 ? x : N - x) : (y <= N / 2 ? y : N - y);
        if (b < N / 2) P[(size_t)b] += std::norm(m[(size_t)y][(size_t)x]);
    }
    return P;
}
// How much the power at the repeat frequencies of a patch of `L` metres (f_n = n span / L, in cycles across the picture)
// stands above the power half a spacing either side of it (the smooth level there): the mean of log10 over the harmonics
// n = 1 .. `upTo` whose neighbours are inside the spectrum (the largest ratio is reported too).
struct Comb { double meanLog = 0, worst = 0; int n = 0; };
static Comb combOf(const std::vector<double>& P, float span, float L, int upTo) {
    Comb c;
    const double s = span / L;
    auto at = [&](double b) { const int i = std::clamp((int)std::lround(b), 1, (int)P.size() - 1); return std::max(P[(size_t)i], 1e-30); };
    auto peak = [&](double b) { return std::max({ at(b - 1), at(b), at(b + 1) }); };
    for (int n = 1; n <= upTo; n++) {
        const double f = n * s;
        if (f + s / 2 >= (double)P.size() - 2 || f - s / 2 < 2) break;
        const double floorLevel = std::sqrt(at(f - s / 2) * at(f + s / 2)), r = peak(f) / floorLevel;
        c.meanLog += std::log10(r); c.worst = std::max(c.worst, r); c.n++;
    }
    if (c.n) c.meanLog /= c.n;
    return c;
}

// (a) no tiling: the slope picture of 2 km and of 400 m of sea has no comb at the patches' repeat frequencies.
static double tilingScore(bool decorrelate, int cascades, const char* tag, double* worstOut = nullptr) {
    double score = -1e9, worst = 0;
    struct View { float span; int cascade; int harmonics; };
    for (const View& v : { View{ 2000, 0, 12 }, View{ 400, 1, 12 } }) {
        auto s = oceanFromAbove(10, 50000, v.span, 2);
        s->water.spectrum.decorrelate = decorrelate;
        s->water.spectrum.cascades = cascades;
        const std::vector<float> patch = s->water.patchSizes();
        if ((int)patch.size() <= v.cascade) continue;
        Image img = look(s, 512, 512, 0);
        dump((std::string("w9-slope-") + tag + "-" + std::to_string((int)v.span) + ".png").c_str(), img);
        const Comb cx = combOf(axisPower(img, 0, true), v.span, patch[(size_t)v.cascade], v.harmonics),
                   cz = combOf(axisPower(img, 1, false), v.span, patch[(size_t)v.cascade], v.harmonics);
        std::printf("[weather]   %s, %.0f m view, patch %.1f m: comb along x mean log10 %.2f (worst %.1f), along z %.2f (worst %.1f), %d harmonics\n",
                    tag, v.span, patch[(size_t)v.cascade], cx.meanLog, cx.worst, cz.meanLog, cz.worst, cx.n);
        score = std::max(score, std::max(cx.meanLog, cz.meanLog));
        worst = std::max(worst, std::max(cx.worst, cz.worst));
        UnloadImage(img);
    }
    if (worstOut) *worstOut = worst;
    return score;
}
static void oceanNoTiling() {
    double worst = 0;
    const double good = tilingScore(true, 3, "spectrum", &worst);
    const double noWarp = tilingScore(false, 3, "no-decorrelation");
    const double one = tilingScore(false, 1, "one-cascade-no-decorrelation");
    std::printf("[weather] ocean tiling: comb of the slope picture's spectrum at the patch repeat frequencies, mean log10 of peak over smooth level: spectrum %.2f (worst ratio %.1f), lookups not bent %.2f, one cascade not bent %.2f\n",
                good, worst, noWarp, one);
    CHECK(good < 0.6 && worst < 12.0);
    CHECK(noWarp > good + 1.5);
    CHECK(one > good + 1.5);
}

// (b) the significant wave height follows the wind as the spectrum says: 4 sigma of the height over one big patch, at two times.
static float measuredHs(const g3d::Water& w) {
    const std::vector<float> patch = w.patchSizes();
    const int N = 100;
    double sum = 0, sum2 = 0, n = 0;
    for (double t : { 0.0, 4.3 })
        for (int j = 0; j < N; j++)
            for (int i = 0; i < N; i++) {
                const double h = w.height((i + 0.37f) * patch[0] / N, (j + 0.61f) * patch[0] / N, t) - w.level;
                sum += h; sum2 += h * h; n++;
            }
    const double mean = sum / n, var = sum2 / n - mean * mean;
    return (float)(4 * std::sqrt(std::max(var, 0.0)));
}
static void oceanHs() {
    float hs[2][3];
    const float winds[3] = { 5, 10, 15 };
    for (int developed = 0; developed < 2; developed++)
        for (int k = 0; k < 3; k++) {
            auto s = oceanScene(winds[k], developed ? 1e7f : 50000.f, { 0, 5, 0 }, { 0, 0, -10 }, 40);
            s->water.spectrum.curvature = false;
            g3d::Weather wx; wx.wind.speed = winds[k];
            const float expect = wx.waveHeight(s->water.fetch), got = measuredHs(s->water);
            hs[developed][k] = got;
            std::printf("[weather] ocean Hs, U %.0f m/s, %s: measured %.2f m, W1 formula %.2f m (%+.0f%%), spectrumHs() %.2f\n", winds[k],
                        developed ? "fully developed" : "50 km fetch", got, expect, 100 * (got / expect - 1), s->water.spectrumHs());
            CHECK(std::fabs(got / expect - 1) < 0.20f);
        }
    const float ratio = hs[1][2] / hs[1][0];
    std::printf("[weather] ocean Hs, fully developed, 15 m/s over 5 m/s: %.1fx (U^2 gives 9)\n", ratio);
    CHECK(ratio > 7.2f && ratio < 10.8f);
}

// (c) purity: a frame alone is the frame after others, and a rebuilt model gives the same heights; two runs agree.
static void oceanPure() {
    auto make = [] { auto s = oceanScene(10, 50000, { 0, 3, 0 }, { 0, 1.5f, -30 }, 50); s->lights.push_back(g3d::Light::directional({ 10, 9, -12 }, WHITE, 2)); s->water.sunGlint = 1; return s; };
    auto s = make();
    Comp c = comp(s, 160, 90);
    Image a = render(c, 33);
    for (int f : { 5, 90, 17, 61 }) { Image o = render(c, f); UnloadImage(o); }
    Image b = render(c, 33);
    Comp c2 = comp(make(), 160, 90);
    Image d = render(c2, 33);
    const long long ab = diffImages(a, b), ad = diffImages(a, d);
    // The CPU surface: flush the frame cache by asking for many other times, then ask again.
    const g3d::Water& w = s->water;
    const float h0 = w.height(12.5f, -3.25f, 7.0), s0 = w.slope(12.5f, -3.25f, 7.0).x;
    for (int i = 0; i < 10; i++) w.height(1, 1, 100.0 + i);
    const float h1 = w.height(12.5f, -3.25f, 7.0), s1 = w.slope(12.5f, -3.25f, 7.0).x;
    std::printf("[weather] ocean purity: frame alone vs after others %lld, two runs %lld, height after a flushed cache %g -> %g\n", ab, ad, h0, h1);
    CHECK(ab == 0 && ad == 0 && h0 == h1 && s0 == s1);
    // A frame at another time differs (the sea moves).
    Image e = render(c, 34);
    CHECK(diffImages(a, e) > 2000);
    UnloadImage(a); UnloadImage(b); UnloadImage(d); UnloadImage(e);
}

// (d) off means off: with the spectrum disabled, nothing it holds changes a pixel; enabling it does.
static void oceanOffMeansOff() {
    auto make = [](bool perturb, bool on) {
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 3, 8 }; s->camera.target = { 0, 0, -20 }; s->camera.fov = 50;
        s->sky = g3d::Sky::day();
        s->water = g3d::Water::choppy();
        s->water.openDepth = 30;
        s->lights.clear();
        s->lights.push_back(g3d::Light::directional({ 10, 12, -8 }, WHITE, 1.5f));
        if (perturb) { auto& sp = s->water.spectrum; sp.windSpeed = 22; sp.cascades = 4; sp.choppiness = 3; sp.curvature = false; sp.seed = 99; sp.visibility = 100; sp.spray = 0; sp.swell = 1; }
        s->water.spectrum.enabled = on;
        return s;
    };
    Image a = look(make(false, false), 320, 180, 12), b = look(make(true, false), 320, 180, 12), c = look(make(true, true), 320, 180, 12);
    const long long off = diffImages(a, b), on = diffImages(a, c);
    std::printf("[weather] ocean off means off: perturbed but disabled differs by %lld (must be 0); enabled differs by %lld\n", off, on);
    CHECK(off == 0 && on > 20000);
    UnloadImage(a); UnloadImage(b); UnloadImage(c);
}

// (e) the horizon sits at the dip below horizontal: sea against a white background, a column of rows.
static float horizonRows(float h, float fov, bool curvature, int H = 720) {
    auto s = oceanScene(8, 50000, { 0, h, 0 }, { 0, h, -100 }, fov);
    s->background = { 0, 255, 0, 255 };
    s->skyColor = { 40, 40, 40, 255 }; s->groundColor = { 40, 40, 40, 255 };
    s->water.spectrum.curvature = curvature; s->water.spectrum.visibility = 1e9f;   // no haze: the edge is then the geometry, not the fade
    Image img = look(s, 96, H, 0);
    float at = -1;
    std::vector<float> avg((size_t)H);
    for (int y = 0; y < H; y++) { double l = 0; for (int x = 20; x < 76; x++) { const Color c = GetImageColor(img, x, y); l += 255 - std::max(c.r, c.b); } avg[(size_t)y] = (float)(l / 56); }
    for (int y = 1; y < H; y++)
        if (avg[(size_t)y] < 250 && avg[(size_t)y - 1] >= 250) { at = (y - 1) + (avg[(size_t)y - 1] - 250) / (avg[(size_t)y - 1] - avg[(size_t)y]) + 0.5f; break; }
    UnloadImage(img);
    return at;
}
static void oceanHorizon() {
    struct Case { float h, fov; };
    for (const Case& k : { Case{ 10, 4 }, Case{ 10000, 12 } }) {
        const int H = 720;
        const float expect = (H / 2.f) * std::tan(g3d::Water::horizonDip(k.h)) / std::tan(k.fov * 0.5f * DEG2RAD);
        const float row = horizonRows(k.h, k.fov, true) - H / 2.f, flat = horizonRows(k.h, k.fov, false) - H / 2.f;
        std::printf("[weather] ocean horizon from %.0f m: %.1f px below the centre, expected %.1f px (dip %.3f deg); without curvature %.1f px\n", k.h, row, expect,
                    g3d::Water::horizonDip(k.h) * RAD2DEG, flat);
        CHECK(row > 0 && std::fabs(row - expect) < 1.5f);
        if (k.h > 1000) CHECK(flat < 0 || std::fabs(flat - expect) > 30.f);
    }
}

// (f) whitecaps follow the wind: the mean foam of a 60 m top-down view (the foam as the shader draws it), over three times.
static float foamAt(float U, float fetch, double* spread = nullptr) {
    double mean = 0, m2 = 0;
    const int frames[6] = { 0, 40, 80, 20, 60, 100 };
    const Vector2 at[6] = { { 0, 0 }, { 530, 190 }, { -410, 770 }, { 880, -640 }, { -950, -300 }, { 260, 1210 } };      // the shader's slow mask thins foam by place: sample six
    for (int fi = 0; fi < 6; fi++) {
        const int f = frames[fi];
        Image img = look(oceanFromAbove(U, fetch, 60, 3, at[fi], 300), 160, 160, f);
        double s = 0;
        for (int y = 0; y < img.height; y++) for (int x = 0; x < img.width; x++) s += linearOf(GetImageColor(img, x, y).r);
        s /= (double)img.width * img.height;
        mean += s / 6; m2 += s * s / 6;
        UnloadImage(img);
    }
    if (spread) *spread = std::sqrt(std::max(m2 - mean * mean, 0.0));
    return (float)mean;
}
static void oceanWhitecaps() {
    float cover[4];
    const float winds[4] = { 3, 8, 12, 18 };
    for (int i = 0; i < 4; i++) {
        double sd;
        cover[i] = foamAt(winds[i], 200000, &sd);
        std::printf("[weather] whitecaps at %.0f m/s (200 km fetch): mean foam %.4f (spread over 6 places %.4f), area law %.4f\n", winds[i], cover[i],
                    sd, oceanScene(winds[i], 200000, { 0, 5, 0 }, { 0, 0, 1 }, 40)->water.whitecapCover());
    }
    CHECK(cover[0] < 0.006f);
    CHECK(cover[1] > cover[0] * 3 && cover[2] > cover[1] * 1.4f && cover[3] > cover[2] * 1.4f);
    CHECK(cover[3] > 0.04f);
    // A short fetch holds the sea down: 18 m/s over 300 m of water breaks far less than over 200 km.
    const float shortFetch = foamAt(18, 300);
    std::printf("[weather] whitecaps at 18 m/s over 300 m of water: %.4f\n", shortFetch);
    CHECK(shortFetch < 0.7f * cover[3]);          // (the foam map is a quantile of the tile, so a short fetch is held down less than the law says)
}

// (g) no far shimmer: the far third of a grazing view changes little from one frame to the next as the camera creeps.
static float farChange(bool filter, const char* tag) {
    auto make = [&](float x) {
        auto s = oceanScene(10, 50000, { x, 1.5f, 0 }, { x, 1.2f, -10 }, 50);
        s->lights.push_back(g3d::Light::directional({ 10, 9, -12 }, WHITE, 2));
        s->water.sunGlint = 1; s->water.filter = filter;
        s->sky = g3d::Sky::day(); s->sky.clouds = 0.3f;
        return s;
    };
    Image a = look(make(0), 320, 180, 4), b = look(make(0.04f), 320, 180, 5);
    dump((std::string("ocean-grazing-") + tag + ".png").c_str(), a);
    double d = 0; int n = 0;
    for (int y = 88; y < 118; y++) for (int x = 20; x < 300; x++) { d += std::fabs(luma(GetImageColor(a, x, y)) - luma(GetImageColor(b, x, y))); n++; }
    UnloadImage(a); UnloadImage(b);
    return (float)(d / n);
}
static void oceanNoShimmer() {
    const float on = farChange(true, "filter"), off = farChange(false, "nofilter");
    std::printf("[weather] ocean far third, mean luma change between frames with the camera creeping 4 cm: band limit on %.2f levels, off %.2f\n", on, off);
    CHECK(on < 2.0f && off > 2.0f * on);
}

// (h) the CPU surface is the drawn one: a top-down read-out of the mesh's height against Water::height; and a box rides it.
static void oceanCpuTwin() {
    auto s = oceanFromAbove(10, 50000, 12, 4, { 0, 0 }, 50);
    s->camera.farPlane = 200;
    const double t = frameTime(0) / s->water.fps;
    Image img = look(s, 180, 180, 0);
    const g3d::Water w = g3d::waterOf(*s);
    double worst = 0, lo = 1e9, hi = -1e9;
    const int px[5][2] = { { 90, 90 }, { 60, 100 }, { 110, 70 }, { 80, 120 }, { 125, 105 } };
    for (const auto& p : px) {
        const Color c = GetImageColor(img, p[0], p[1]);
        const float a = linearOf(c.r), g = linearOf(c.g);
        const float coarse = a * 16.f - 8.f, fine = g;            // the metre's fraction: pick the whole metre that agrees with the coarse byte
        const float y = std::round(coarse + 8.f - fine) + fine - 8.f;
        const float x = (p[0] + 0.5f - 90) * (12.f / 180), z = (p[1] + 0.5f - 90) * (12.f / 180);
        const float h = w.height(x, z, t);
        worst = std::max(worst, (double)std::fabs(y - h));
        lo = std::min(lo, (double)h); hi = std::max(hi, (double)h);
        std::printf("[weather]   probe (%.2f, %.2f) m: drawn %.4f m, CPU %.4f m\n", x, z, y, h);
    }
    std::printf("[weather] ocean CPU twin: worst difference over 5 probes %.1f mm (heights span %.2f m)\n", worst * 1000, hi - lo);
    CHECK(worst < 0.02 && hi - lo > 0.15);
    UnloadImage(img);
    // ride(): a small hull follows the surface and its slope.
    auto sea = oceanScene(10, 50000, { 0, 5, 0 }, { 0, 0, -10 }, 40);
    g3d::Object3D box;
    box.position = { 40, 0, 25 };
    g3d::Ride how; how.length = 1; how.beam = 1; how.heading = 0;
    double err = 0, lo2 = 1e9, hi2 = -1e9;
    for (int i = 0; i < 90; i++) {
        const double ts = i / 15.0;
        g3d::ride(box, *sea, ts, how);
        err = std::max(err, (double)std::fabs(box.position.y - g3d::surfaceHeight(*sea, 40, 25, ts)));
        lo2 = std::min(lo2, (double)box.position.y); hi2 = std::max(hi2, (double)box.position.y);
    }
    std::printf("[weather] ocean ride(): a 1 m hull stays within %.1f cm of the surface over 6 s, rising and falling over %.2f m\n", err * 100, hi2 - lo2);
    CHECK(err < 0.06 && hi2 - lo2 > 0.2);
}

// (i) the shading's slope variance is the sea's: the grids' mean square slope plus the unresolved tail, near Cox and Munk's 0.003 + 0.00512 U.
static void oceanSlopeVariance() {
    for (float U : { 5.f, 10.f, 18.f }) {
        auto s = oceanScene(U, 200000, { 0, 5, 0 }, { 0, 0, -10 }, 40);
        float tail = 0;
        const float total = s->water.shadingSlopeVariance(&tail), cm = 0.003f + 0.00512f * U;
        std::printf("[weather] ocean slope variance at %.0f m/s: grids %.4f + unresolved tail %.4f = %.4f, Cox-Munk %.4f (%+.0f%%); grids alone %+.0f%%\n", U, total - tail, tail, total, cm,
                    100 * (total / cm - 1), 100 * ((total - tail) / cm - 1));
        CHECK(std::fabs(total / cm - 1) < 0.30f);
        CHECK(std::fabs((total - tail) / cm - 1) > 0.30f || U < 7);     // the shading-only fine band alone would fail at the strong winds
    }
}

// (j) whitecaps lie along the crest: connected foam patches of a top-down view are elongated, across the wind.
static void oceanWhitecapShape() {
    std::vector<double> aspect, off;
    int total = 0;
    for (int fi = 0; fi < 4; fi++) {
        const int f = fi * 30;
        const Vector2 where[4] = { { 0, 0 }, { 530, 190 }, { -410, 770 }, { 880, -640 } };
        const int N = 400;
        Image img = look(oceanFromAbove(12, 200000, 160, 3, where[fi], 300), N, N, f);
        std::vector<unsigned char> on((size_t)N * N);
        for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) on[(size_t)y * N + x] = linearOf(GetImageColor(img, x, y).r) > 0.5f;
        UnloadImage(img);
        std::vector<int> seen((size_t)N * N, 0), stack;
        for (int y0 = 0; y0 < N; y0++)
            for (int x0 = 0; x0 < N; x0++) {
                if (!on[(size_t)y0 * N + x0] || seen[(size_t)y0 * N + x0]) continue;
                double n = 0, sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
                bool edge = false;
                stack.assign(1, y0 * N + x0); seen[(size_t)y0 * N + x0] = 1;
                while (!stack.empty()) {
                    const int p = stack.back(); stack.pop_back();
                    const int x = p % N, y = p / N;
                    n++; sx += x; sy += y; sxx += (double)x * x; syy += (double)y * y; sxy += (double)x * y;
                    if (x == 0 || y == 0 || x == N - 1 || y == N - 1) edge = true;
                    const int nb[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
                    for (const auto& d : nb) {
                        const int xx = x + d[0], yy = y + d[1];
                        if (xx < 0 || yy < 0 || xx >= N || yy >= N || !on[(size_t)yy * N + xx] || seen[(size_t)yy * N + xx]) continue;
                        seen[(size_t)yy * N + xx] = 1; stack.push_back(yy * N + xx);
                    }
                }
                if (n < 30 || edge) continue;
                const double mx = sx / n, my = sy / n, cxx = sxx / n - mx * mx, cyy = syy / n - my * my, cxy = sxy / n - mx * my;
                const double tr = cxx + cyy, det = cxx * cyy - cxy * cxy, disc = std::sqrt(std::max(tr * tr / 4 - det, 0.0));
                const double l1 = tr / 2 + disc, l2 = std::max(tr / 2 - disc, 1e-6);
                const double ang = 0.5 * std::atan2(2 * cxy, cxx - cyy) * RAD2DEG;       // major axis from +x (the wind), image y is z
                aspect.push_back(std::sqrt(l1 / l2));
                off.push_back(std::fabs(std::fabs(ang) - 90.0));                        // degrees from perpendicular to the wind
                total++;
            }
    }
    std::sort(aspect.begin(), aspect.end()); std::sort(off.begin(), off.end());
    const double medA = aspect.empty() ? 0 : aspect[aspect.size() / 2], medO = off.empty() ? 90 : off[off.size() / 2];
    std::printf("[weather] whitecap shape at 12 m/s: %d patches over 4 times, median aspect ratio %.2f, median angle off perpendicular to the wind %.1f deg\n", total, medA, medO);
    CHECK(total >= 8 && medA >= 2.0 && medO <= 30.0);
}

// (k) artefacts of a storm seen low: the sea's reflection of the sky shows no contour rings, and foam edges are not hard stair steps.
// Rings: in dark water, the share of pixels that start a flat terrace (6+ equal levels) ending in a 1-3 level step.
// Foam: of the horizontal neighbours on the foam-only view that differ at all, the share that jump by more than half the range.
static void oceanArtefacts() {
    auto storm = [](int dbg) {
        auto s = oceanScene(18, 1000000, { 0, 3.f, 0 }, { 0, 2.4f, -20 }, 60);
        s->water.openDepth = 40; s->water.spectrum.choppiness = 1.8f; s->water.spectrum.windDirection = 90;
        s->sky = g3d::Sky::day(); s->sky.clouds = 0.3f;
        s->lights.push_back(g3d::Light::directional({ 3, 3.6, -12 }, { 255, 240, 216, 255 }, 2.6f));
        s->water.sunGlint = 1; s->water.reflections = g3d::Water::Reflections::Quarter;
        s->ground.debug = dbg;
        return s;
    };
    Image a = look(storm(0), 1920, 1080, 20);
    const std::vector<float> L = lumaOf(a);
    // Rings are thin contour lines in a smooth gradient: the mean absolute Laplacian of the dark water's luma (levels).
    double lap = 0; long n = 0;
    for (int y = 600; y < 1060; y++)
        for (int x = 10; x < 1900; x++) {
            const float v = L[(size_t)y * 1920 + x];
            if (v > 150) continue;
            lap += std::fabs(L[(size_t)y * 1920 + x - 1] + L[(size_t)y * 1920 + x + 1] + L[(size_t)(y - 1) * 1920 + x] + L[(size_t)(y + 1) * 1920 + x] - 4 * v);
            n++;
        }
    UnloadImage(a);
    const double ring = lap / std::max(n, 1L);
    Image f = look(storm(3), 960, 540, 20);
    long any = 0, hard = 0;
    for (int y = 100; y < 540; y++) {
        int run = 0; float prev = linearOf(GetImageColor(f, 0, y).r);
        for (int x = 1; x < 960; x++) {
            const float v = linearOf(GetImageColor(f, x, y).r);
            if (v > 0.1f && v < 0.9f) run++;
            if ((prev - 0.5f) * (v - 0.5f) < 0) { any++; hard += (run <= 1); run = 0; }
            prev = v;
        }
    }
    UnloadImage(f);
    const double stair = (double)hard / std::max(any, 1L);
    std::printf("[weather] ocean artefacts, storm seen from 3 m: dark water mean Laplacian %.3f levels; foam edges crossed with at most one in-between pixel %.1f%%\n", ring, 100 * stair);
    CHECK(ring < 4.0);
    CHECK(stair < 0.015);
}

static void ocean() {
    oceanNoTiling();
    oceanSlopeVariance();
    oceanHs();
    oceanPure();
    oceanOffMeansOff();
    oceanHorizon();
    oceanWhitecaps();
    oceanWhitecapShape();
    oceanArtefacts();
    oceanNoShimmer();
    oceanCpuTwin();
}

static void w9Images() {
    auto lit = [](float U, Vector3 pos, Vector3 target, float fov, float cloud) {
        auto s = oceanScene(U, 200000, pos, target, fov);
        s->sky = g3d::Sky::day(); s->sky.clouds = cloud;
        s->lights.push_back(g3d::Light::directional({ 3, 3.6, -12 }, { 255, 240, 216, 255 }, 2.6f));
        s->water.openDepth = 40; s->water.sunGlint = 1; s->water.reflections = g3d::Water::Reflections::Quarter;
        return s;
    };
    const char* dir = std::getenv("RMTN_WEATHER_DUMP");
    if (!dir) return;
    fs::create_directories(dir);
    auto save = [&](const char* name, const Image& img) { ExportImage(img, (fs::path(dir) / name).string().c_str()); };
    Image a = look(lit(10, { 0, 5.f, 0 }, { 0, 3.6f, -10 }, 60, 0.3f), 1920, 1080, 20);
    save("w9_grazing_10ms.png", a);
    Image crop = ImageFromImage(a, { 560, 640, 640, 360 });
    ImageResizeNN(&crop, 1920, 1080);
    save("w9_grazing_10ms_crop3x.png", crop);
    UnloadImage(a); UnloadImage(crop);
    { auto gf = lit(10, { 0, 5.f, 0 }, { 0, 3.6f, -10 }, 60, 0.3f); gf->ground.debug = 3; Image e = look(gf, 960, 540, 20); save("w9_grazing_10ms_foamonly.png", e); UnloadImage(e); }
    Image b = look(lit(10, { 0, 10000, 0 }, { 0, 10000 - 80, -400 }, 62, 0.1f), 1920, 1080, 20);
    save("w9_from_10km.png", b); UnloadImage(b);
    auto top = oceanFromAbove(10, 200000, 2000, 0, { 0, 0 }, 3000);
    top->sky = g3d::Sky::day();
    top->lights.push_back(g3d::Light::directional({ 3, 3.6, -12 }, { 255, 240, 216, 255 }, 2.6f));
    top->water.sunGlint = 1;
    Image c = look(top, 1080, 1080, 20);
    save("w9_topdown_2km.png", c); UnloadImage(c);
    auto storm = [&](bool foamOnly) {
        auto s = lit(18, { 0, 3.f, 0 }, { 0, 2.4f, -20 }, 60, 1.0f);
        { g3d::Water st = g3d::Water::storm(); st.spectrum = s->water.spectrum; st.waves.clear(); st.fetch = s->water.fetch; st.openDepth = 40; st.sunGlint = 1; st.reflections = s->water.reflections; st.foam = 1; s->water = st; }
        s->weather = g3d::Weather::storm(); s->weather.wind.speed = 18; s->weather.wind.direction = 90; s->weather.wind.gust = 0.3f;
        s->water.fetch = 1000000; s->water.spectrum.choppiness = 1.8f; s->water.spectrum.windDirection = 90;
        if (foamOnly) s->ground.debug = 3;
        if (std::getenv("RMTN_W9_NOREFL")) s->water.reflections = g3d::Water::Reflections::Off;
        if (const char* dbg = std::getenv("RMTN_W9_DEBUG")) s->ground.debug = std::atoi(dbg);
        return s;
    };
    { auto ss = storm(false); const g3d::Water ww = g3d::waterOf(*ss); std::printf("[probe] storm: spectrumHs %.2f m, measured Hs %.2f m, wind %.1f m/s, cover %.3f, slope variance %.3f\n", ww.spectrumHs(), measuredHs(ww), ww.spectrum.windSpeed, ww.whitecapCover(), ww.shadingSlopeVariance()); }
    { auto ss = storm(false); const g3d::Water ww = g3d::waterOf(*ss); const double t0 = frameTime(20) / ww.fps; float hi = -1e9f, lo = 1e9f, hi60 = -1e9f; for (int ix = -300; ix <= 300; ix += 3) for (int iz = 5; iz <= 400; iz += 3) { const float h = ww.height((float)ix, -(float)iz, t0); hi = std::max(hi, h); lo = std::min(lo, h); if (iz < 60 && std::abs(ix) < iz) hi60 = std::max(hi60, h); } std::printf("[probe] storm crest heights in view (camera 3 m above the mean level): max crest %.2f m, deepest trough %.2f m, highest crest within 60 m ahead %.2f m\n", hi, lo, hi60); }
    Image d = look(storm(false), 1920, 1080, 20);
    save("w9_storm_18ms.png", d);
    Image dc = ImageFromImage(d, { 560, 640, 640, 360 });
    ImageResizeNN(&dc, 1920, 1080);
    save("w9_storm_18ms_crop3x.png", dc);
    UnloadImage(d); UnloadImage(dc);
    { Image e = look(storm(true), 960, 540, 20); save("w9_storm_18ms_foamonly.png", e); UnloadImage(e); }
}

static void w9Timing() {
    constexpr int W = 1920, H = 1080;
    for (int on = 0; on < 2; on++)
        for (float h : { 2.4f, 10000.f }) {
            auto s = oceanScene(9, 60000, { 0, h, 0 }, { 0, h - (h > 100 ? 80.f : 0.3f), -400 }, 62);
            s->sky = g3d::Sky::day(); s->sky.clouds = 0.3f;
            s->lights.push_back(g3d::Light::directional({ 3, 3.6, -12 }, WHITE, 2.6f));
            s->water.sunGlint = 1; s->water.reflections = g3d::Water::Reflections::Quarter;
            if (!on) { s->water.spectrum.enabled = false; s->water.waves = g3d::Water::choppy().waves; }
            Comp c = comp(s, W, H);
            Canvas canvas;
            canvas.load(W, H, 1);
            presentFrame(c, 0, canvas);
            double best = 1e9, mean = 0;
            for (int i = 0; i < 12; i++) {
                const double t0 = GetTime();
                presentFrame(c, i + 1, canvas);
                Image img = LoadImageFromTexture(canvas.output().texture);
                const double dt = (GetTime() - t0) * 1000;
                UnloadImage(img);
                best = std::min(best, dt); mean += dt / 12;
            }
            canvas.unload();
            std::printf("[weather] W9 timing 1080p, spectrum %s, camera %.1f m: best %.1f ms, mean %.1f ms\n", on ? "on" : "off", h, best, mean);
        }
    // The CPU cost of one frame of the spectrum (3 cascades): the FFTs, uncached.
    auto s = oceanScene(9, 60000, { 0, 5, 0 }, { 0, 0, -10 }, 40);
    double t0 = GetTime();
    for (int i = 0; i < 20; i++) s->water.height(1, 1, 1000.0 + i);
    std::printf("[weather] W9 CPU frame build (3 cascades, FFTs + foam): %.1f ms per new time\n", (GetTime() - t0) * 1000 / 20);
}

// ---------------------------------------------------------------- W5: lightning, storm light, rainbow
// A dark ground under a camera, a strike from a cloud base 1.5 km up to the ground 900 m away at second 1 (frame 30).
static g3d::ScenePtr boltScene(uint32_t seed, bool enabled = true, float minVisible = 0.07f) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 6, 0 };
    s->camera.target = { 0, 150, -1000 };
    s->camera.fov = 60;
    s->camera.nearPlane = 1;
    s->camera.farPlane = 100000;
    s->ambient = 0.2f;
    g3d::Material m;
    m.color = { 120, 120, 120, 255 };
    m.roughness = 1;
    s->add(g3d::Geometry::plane(20000, 20000), m);
    s->lightning.enabled = enabled;
    s->lightning.minVisible = minVisible;
    g3d::Lightning::Strike st;
    st.time = 1.f;
    st.start = { 0, 1500, -900 };
    st.end = { 15, 0, -905 };
    st.seed = seed;
    s->lightning.strikes.push_back(st);
    return s;
}
static float meanLuma(const Image& img) {
    double sum = 0;
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++) sum += luma(GetImageColor(img, x, y));
    return (float)(sum / ((double)img.width * img.height));
}

// (a) the same seed makes the same bolt and another seed another: the geometry, and the pixels.
static void boltSeeds() {
    const bolt::Bolt a = bolt::make({ 0, 1500, -900 }, { 15, 0, -905 }, 5), b = bolt::make({ 0, 1500, -900 }, { 15, 0, -905 }, 5),
                     c = bolt::make({ 0, 1500, -900 }, { 15, 0, -905 }, 6);
    auto same = [](const bolt::Bolt& p, const bolt::Bolt& q) {
        if (p.segments.size() != q.segments.size()) return false;
        for (size_t i = 0; i < p.segments.size(); i++)
            if (Vector3Distance(p.segments[i].a, q.segments[i].a) > 1e-4f || Vector3Distance(p.segments[i].b, q.segments[i].b) > 1e-4f) return false;
        return true;
    };
    CHECK(same(a, b));
    CHECK(!same(a, c));
    CHECK(a.segments.size() > 100);
    int branches = 0;
    for (const bolt::Segment& sg : a.segments) branches += sg.level > 0 ? 1 : 0;
    CHECK(branches > 4);
    constexpr int W = 320, H = 180, F = 32;                                 // 67 ms into the strike: the return stroke
    Image i5 = look(boltScene(5), W, H, F), i5b = look(boltScene(5), W, H, F), i6 = look(boltScene(6), W, H, F), base = look(boltScene(5, false), W, H, F);
    const long long dSame = diffImages(i5, i5b), dOther = diffImages(i5, i6), dBolt = diffImages(i5, base);
    std::printf("[weather] W5 seeds: geometry %zu segments (%d on branches), same seed pixel diff %lld, another seed %lld, bolt vs none %lld\n",
                a.segments.size(), branches, dSame, dOther, dBolt);
    dump("w5-bolt-seed5.png", i5);
    dump("w5-bolt-seed6.png", i6);
    CHECK(dSame == 0);
    CHECK(dOther > 2000);
    CHECK(dBolt > 20000);
    UnloadImage(i5); UnloadImage(i5b); UnloadImage(i6); UnloadImage(base);
}

// Frames of a strike at second 1 (frame 30): how many differ from the frame without lightning, and in which way.
struct Window { int bright = 0, brightRun = 0, outsideDiff = 0, lower = 0; float peak = 0; };
static Window strikeWindow(uint32_t seed, bool enabled, float minVisible, bool dumpOne) {
    constexpr int W = 320, H = 180;
    Window w;
    Image none = look(boltScene(seed, false), W, H, 0);
    const float base = meanLuma(none);
    auto scene = boltScene(seed, enabled, minVisible);
    Comp c = comp(scene, W, H);
    const float dur = bolt::duration(seed, scene->lightning.leaderTime, minVisible);
    int run = 0;
    for (int f = 24; f <= 54; f++) {
        Image img = render(c, f);
        const float tau = f / 30.f - 1.f, m = meanLuma(img);
        const bool inside = tau >= 0 && tau < dur;
        if (!inside) { if (diffImages(img, none) != 0) w.outsideDiff++; }
        else {
            if (m > base + 0.05f) { w.bright++; run++; w.brightRun = std::max(w.brightRun, run); } else run = 0;
            if (m < base - 0.01f) w.lower++;
            w.peak = std::max(w.peak, m - base);
        }
        if (dumpOne && f == 32) dump("w5-bolt-frame32.png", img);
        UnloadImage(img);
    }
    UnloadImage(none);
    return w;
}
// (b) the picture brightens during the strike and only then: no change before the leader or after the last restrike, and the
// bolt is on at least two frames of a 30 fps film whatever its seed.
// Frames (at 30 fps, any phase) on which the first return stroke is at full brightness: at least two with the minimum
// visible time, whatever the seed.
static int strokeFrames(uint32_t seed, float phase, float minVisible) {
    int run = 0;
    for (int k = 0; k < 40; k++) {
        const bolt::Profile p = bolt::profile(seed, phase + k / 30.f, 0.035f, minVisible);
        if (p.flash > 0.9f) run++;
        else if (run > 0) break;
    }
    return run;
}
static void boltWindow() {
    int minBright = 99, minRun = 99, outside = 0, lower = 0;
    float peak = 0;
    for (uint32_t seed = 1; seed <= 2; seed++) {
        const Window w = strikeWindow(seed, true, 0.07f, seed == 1);
        minBright = std::min(minBright, w.bright); minRun = std::min(minRun, w.brightRun);
        outside += w.outsideDiff; lower += w.lower; peak = std::max(peak, w.peak);
    }
    const Window off = strikeWindow(1, false, 0.07f, false);               // control: lightning off
    int fewest = 99, fewestBroken = 99;
    for (uint32_t seed = 1; seed <= 300; seed++)
        for (int ph = 0; ph < 4; ph++) {
            fewest = std::min(fewest, strokeFrames(seed, ph / 4.f / 30.f, 0.07f));
            fewestBroken = std::min(fewestBroken, strokeFrames(seed, ph / 4.f / 30.f, 0.004f));      // control: no minimum visible time
        }
    std::printf("[weather] W5 window: frames brighter than the dark scene, per strike over 2 seeds at least %d, longest run at least %d; outside the strike %d changed frames, %d darker, peak +%.1f luma; lightning off %d; return stroke frames over 300 seeds x 4 phases at least %d (with no minimum visible time %d)\n",
                minBright, minRun, outside, lower, peak, off.bright, fewest, fewestBroken);
    CHECK(minBright >= 2 && minRun >= 2);
    CHECK(outside == 0);
    CHECK(lower == 0);
    CHECK(peak > 1.0f);
    CHECK(off.bright == 0);
    CHECK(fewest >= 2);
    CHECK(fewestBroken < 2);
}
// (f) strikes from a rate: deterministic, about `rate` a minute over a long window, another seed other times.
static void strikeRate() {
    auto s = std::make_shared<g3d::Scene>();
    s->lightning.enabled = true;
    s->lightning.rate = 12;
    s->lightning.area = { 200, -300 };
    s->lightning.radius = 2500;
    const auto a = g3d::lightningStrikes(*s, 0, 600), b = g3d::lightningStrikes(*s, 0, 600);
    s->lightning.seed = 9;
    const auto c = g3d::lightningStrikes(*s, 0, 600);
    bool same = a.size() == b.size();
    for (size_t i = 0; same && i < a.size(); i++) same = a[i].time == b[i].time && a[i].seed == b[i].seed && a[i].start.x == b[i].start.x && a[i].end.z == b[i].end.z;
    bool inArea = true, ground = false, cloud = false, sorted = true;
    for (size_t i = 0; i < a.size(); i++) {
        inArea = inArea && std::hypot(a[i].start.x - 200, a[i].start.z + 300) <= 2500.f * 1.01f;
        (a[i].end.y <= 50 ? ground : cloud) = true;
        if (i && a[i].time < a[i - 1].time) sorted = false;
    }
    bool differs = a.size() != c.size();
    for (size_t i = 0; !differs && i < a.size(); i++) differs = a[i].time != c[i].time;
    const double expect = 12.0 * 10.0;
    std::printf("[weather] W5 rate: 12 a minute over 600 s: %zu strikes (expected %.0f, within 20%% is %.0f..%.0f), identical on a second call %d, another seed differs %d, all in the area %d, ground and in-cloud both %d\n",
                a.size(), expect, expect * 0.8, expect * 1.2, same ? 1 : 0, differs ? 1 : 0, inArea ? 1 : 0, ground && cloud ? 1 : 0);
    CHECK(same);
    CHECK(differs);
    CHECK(inArea && sorted);
    CHECK(ground && cloud);
    CHECK(a.size() >= (size_t)(expect * 0.8) && a.size() <= (size_t)(expect * 1.2));
    s->lightning.rate = 0;
    CHECK(g3d::lightningStrikes(*s, 0, 600).empty());                // control: a rate of nothing
    // thunder: the delay is distance / 343, and the cue list agrees
    CHECK(std::fabs(bolt::thunderDelay(3430.f) - 10.f) < 1e-4f);
    auto t = boltScene(1);
    const std::vector<g3d::Thunder> cues = g3d::thunderCues(*t, 300, 30);
    CHECK(cues.size() == 1);
    if (!cues.empty()) {
        const double want = 1.0 + cues[0].distance / 343.0;
        std::printf("[weather] W5 thunder: strike at %.2f s, %.0f m away, thunder at %.2f s (expected %.2f)\n", cues[0].strikeTime, cues[0].distance, cues[0].arrives, want);
        CHECK(std::fabs(cues[0].arrives - want) < 1e-3);
        CHECK(cues[0].distance > 800 && cues[0].distance < 1000);
    }
}

// (e) a strike inside a cloud lights the cloud round it: the rise in the pixels near the channel against the pixels far from it.
static g3d::ScenePtr cloudBoltScene(bool withBolt, float cloudLight = 1) {
    auto s = testClouds({ 0, 500, 0 }, { 0, 2400, -1500 }, 0.9f, g3d::CloudType::Cumulus, true, 1500, 3500, 70);
    s->clouds.shadows = false;
    s->lights.clear();
    s->ambient = 0.3f;
    s->lightning.enabled = withBolt;
    s->lightning.cloudLight = cloudLight;
    s->lightning.light = 0;                                           // the cloud's own glow, not the point light on the ground
    g3d::Lightning::Strike st;
    st.time = 1.f;
    st.start = { 0, 2900, -1700 };
    st.end = { 120, 2300, -1650 };
    st.seed = 3;
    s->lightning.strikes.push_back(st);
    return s;
}
static void cloudGlow() {
    constexpr int W = 320, H = 180, F = 32;
    auto probe = cloudBoltScene(true);
    const Vector2 p = project(probe->camera, { 60, 2600, -1675 }, W, H);
    Image on = look(cloudBoltScene(true), W, H, F), off = look(cloudBoltScene(false), W, H, F), dark = look(cloudBoltScene(true), W, H, 0), off0 = look(cloudBoltScene(false), W, H, 0),
          dim = look(cloudBoltScene(true, 0), W, H, F);
    double near = 0, far = 0, nearDim = 0;
    int nn = 0, nf = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const float d = std::hypot((float)x - p.x, (float)y - p.y);
            const float gain = luma(GetImageColor(on, x, y)) - luma(GetImageColor(off, x, y)), gainDim = luma(GetImageColor(dim, x, y)) - luma(GetImageColor(off, x, y));
            if (d < 40) { near += gain; nearDim += gainDim; nn++; }
            else if (d > 110) { far += gain; nf++; }
        }
    near /= std::max(nn, 1); far /= std::max(nf, 1); nearDim /= std::max(nn, 1);
    const long long before = diffImages(dark, off0);
    std::printf("[weather] W5 cloud glow: mean luma rise within 40 px of the channel %.2f (%d px), beyond 110 px %.2f (%d px); cloudLight 0: %.2f; before the strike the picture differs by %lld\n",
                near, nn, far, nf, nearDim, before);
    dump("w5-cloud-flash.png", on);
    CHECK(nn > 100 && nf > 100);
    CHECK(near > 6.0 && near > 3.0 * std::max(far, 0.5));
    CHECK(nearDim < 0.5 * near);                                     // control: the glow switched off takes the rise away
    CHECK(before == 0);
    UnloadImage(on); UnloadImage(off); UnloadImage(dark); UnloadImage(dim); UnloadImage(off0);
}

// The rainbow: a rain-wet sky with the sun 30 degrees round and `elev` high; the camera looks at the point opposite it
// (or, with `towardsSun`, at the sun itself), with a wide field of view.
static g3d::ScenePtr bowScene(float elev, bool towardsSun, g3d::Weather::Rainbow::Mode mode, float rain = 0.8f, Vector3* antiOut = nullptr) {
    auto s = std::make_shared<g3d::Scene>();
    const float el = elev * DEG2RAD, az = 30 * DEG2RAD;
    const Vector3 sun{ std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az) };
    s->sky = g3d::Sky::day();
    s->sky.sunLight = -1;
    s->sky.sunDirection = sun;
    s->sky.clouds = 0;
    s->sky.sunGlow = 0;
    s->sky.resolution = 1024;
    s->camera.position = { 0, 2, 0 };
    const Vector3 dir = towardsSun ? sun : Vector3Negate(sun);
    s->camera.target = Vector3Add(s->camera.position, Vector3Scale(dir, 100));
    s->camera.fov = 112;
    s->camera.nearPlane = 1;
    s->camera.farPlane = 100000;
    s->weather = g3d::Weather::rain();
    s->weather.enabled = true;
    s->weather.clouds.cover = 0.1f;
    s->weather.precipitation.intensity = rain;
    s->weather.rainbow.mode = mode;
    if (antiOut) *antiOut = Vector3Negate(sun);
    return s;
}
// The picture's pixels as directions: the angle from `centre`, the azimuth round it, and the change the bow made.
struct Ring { std::vector<float> th, phi, dl, dr, db; };
static Ring ringOf(const Image& on, const Image& off, const g3d::Camera& cam, Vector3 centre, int W, int H) {
    Ring r;
    const Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position)), right = Vector3Normalize(Vector3CrossProduct(fwd, { 0, 1, 0 })), up = Vector3CrossProduct(right, fwd);
    const float ty = std::tan(cam.fov * 0.5f * DEG2RAD), tx = ty * (float)W / H;
    const Vector3 e1 = Vector3Normalize(Vector3CrossProduct({ 0, 1, 0 }, centre)), e2 = Vector3CrossProduct(centre, e1);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const float nx = ((x + 0.5f) / W) * 2 - 1, ny = 1 - ((y + 0.5f) / H) * 2;
            const Vector3 d = Vector3Normalize(Vector3Add(fwd, Vector3Add(Vector3Scale(right, nx * tx), Vector3Scale(up, ny * ty))));
            const float th = std::acos(std::clamp(Vector3DotProduct(d, centre), -1.f, 1.f)) * RAD2DEG;
            const Color a = GetImageColor(on, x, y), b = GetImageColor(off, x, y);
            r.th.push_back(th);
            r.phi.push_back(std::atan2(Vector3DotProduct(d, e2), Vector3DotProduct(d, e1)));
            r.dl.push_back(luma(a) - luma(b));
            r.dr.push_back((float)(a.r - b.r));
            r.db.push_back((float)(a.b - b.b));
        }
    return r;
}
// Kasa's circle fit to points (x, y): centre and radius.
static void fitCircle(const std::vector<Vector2>& p, Vector2& c, float& radius) {
    double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0, sz = 0, sxz = 0, syz = 0;
    for (const Vector2& q : p) {
        const double x = q.x, y = q.y, z = x * x + y * y;
        sx += x; sy += y; sxx += x * x; syy += y * y; sxy += x * y; sz += z; sxz += x * z; syz += y * z;
    }
    const double n = (double)p.size();
    // [sxx sxy sx; sxy syy sy; sx sy n] * [A B C] = [sxz syz sz], the circle x2 + y2 = A x + B y + C
    double a[3][4] = { { sxx, sxy, sx, sxz }, { sxy, syy, sy, syz }, { sx, sy, n, sz } };
    for (int i = 0; i < 3; i++) {
        int piv = i;
        for (int j = i + 1; j < 3; j++) if (std::fabs(a[j][i]) > std::fabs(a[piv][i])) piv = j;
        for (int k = 0; k < 4; k++) std::swap(a[i][k], a[piv][k]);
        for (int j = 0; j < 3; j++)
            if (j != i) { const double f = a[j][i] / a[i][i]; for (int k = 0; k < 4; k++) a[j][k] -= f * a[i][k]; }
    }
    const double A = a[0][3] / a[0][0], B = a[1][3] / a[1][1], C = a[2][3] / a[2][2];
    c = { (float)(A / 2), (float)(B / 2) };
    radius = (float)std::sqrt(std::max(C + (A * A + B * B) / 4, 0.0));
}
static float binPeak(const Ring& r, float lo, float hi, float step = 0.25f) {
    const int n = (int)std::ceil((hi - lo) / step);
    std::vector<double> sum((size_t)n, 0.0);
    for (size_t i = 0; i < r.th.size(); i++) {
        const int b = (int)((r.th[i] - lo) / step);
        if (b >= 0 && b < n && r.dl[i] > 0) sum[(size_t)b] += r.dl[i];
    }
    int best = 0;
    for (int i = 1; i < n; i++) if (sum[(size_t)i] > sum[(size_t)best]) best = i;
    return lo + (best + 0.5f) * step;
}
static float meanRB(const Ring& r, float lo, float hi) {
    double s = 0; int n = 0;
    for (size_t i = 0; i < r.th.size(); i++)
        if (r.th[i] >= lo && r.th[i] < hi) { s += r.dr[i] - r.db[i]; n++; }
    return n ? (float)(s / n) : 0.f;
}
// (c) the rainbow's geometry: a ring centred on the point opposite the sun, the primary at 42 degrees (red outside), the
// secondary at 51 (red inside).
static void rainbowGeometry() {
    constexpr int W = 480, H = 270;
    Vector3 anti;
    auto on = bowScene(25, false, g3d::Weather::Rainbow::Mode::Auto, 0.8f, &anti);
    auto off = bowScene(25, false, g3d::Weather::Rainbow::Mode::Off);
    Image a = look(on, W, H, 0), b = look(off, W, H, 0);
    const Ring r = ringOf(a, b, on->camera, anti, W, H);
    const float p1 = binPeak(r, 36, 46), p2 = binPeak(r, 47.5f, 56);
    std::vector<Vector2> pts;
    for (size_t i = 0; i < r.th.size(); i++)
        if (std::fabs(r.th[i] - p1) < 0.6f && r.dl[i] > 1.f) pts.push_back({ r.th[i] * std::cos(r.phi[i]), r.th[i] * std::sin(r.phi[i]) });
    Vector2 c{ 0, 0 };
    float radius = 0;
    if (pts.size() > 20) fitCircle(pts, c, radius);
    const float prOut = meanRB(r, p1 + 0.4f, p1 + 1.2f), prIn = meanRB(r, p1 - 1.2f, p1 - 0.4f), seIn = meanRB(r, p2 - 1.2f, p2 - 0.3f), seOut = meanRB(r, p2 + 0.3f, p2 + 1.5f);
    std::printf("[weather] W5 rainbow: primary peak %.2f deg (circle fit of %zu px: centre (%.2f, %.2f) deg from the anti-solar point, radius %.2f deg), secondary peak %.2f deg; red minus blue: primary outside %.1f inside %.1f, secondary inside %.1f outside %.1f\n",
                p1, pts.size(), c.x, c.y, radius, p2, prOut, prIn, seIn, seOut);
    dump("w5-rainbow-wide.png", a);
    CHECK(pts.size() > 200);
    CHECK(std::fabs(radius - 42.f) <= 1.f);
    CHECK(std::hypot(c.x, c.y) < 0.5f);
    CHECK(std::fabs(p2 - 51.f) <= 1.5f);
    CHECK(prOut > 3.f && prOut > prIn + 8.f);                      // red outside; inside the colours pile up towards white
    CHECK(seIn > 0.5f && seOut < -0.5f);
    UnloadImage(a); UnloadImage(b);
}
// (d) no rainbow where there should be none: the sun in front of the camera, no rain, the sun too high - the picture equals
// the one with the bow Off; and the controls (behind, rain, 25 degrees; forced On; distant rain) differ.
static void rainbowAbsent() {
    constexpr int W = 240, H = 135;
    using M = g3d::Weather::Rainbow::Mode;
    auto diff = [&](g3d::ScenePtr s) {
        Image a = look(s, W, H, 0);
        s->weather.rainbow.mode = M::Off;
        Image b = look(s, W, H, 0);
        const long long d = diffImages(a, b);
        UnloadImage(a); UnloadImage(b);
        return d;
    };
    const long long front = diff(bowScene(25, true, M::Auto)), dry = diff(bowScene(25, false, M::Auto, 0.f)), high = diff(bowScene(60, false, M::Auto)),
                    ctl = diff(bowScene(25, false, M::Auto)), forced = diff(bowScene(60, false, M::On));
    auto d = bowScene(25, false, M::Auto, 0.f);
    d->weather.rainbow.distantRain = 0.8f;
    const long long distant = diff(d);
    std::printf("[weather] W5 rainbow absent: sun in front diff %lld, no rain %lld, sun at 60 deg %lld; control (behind, rain, 25 deg) %lld; forced On at 60 deg %lld; distant rain only %lld\n", front, dry, high, ctl, forced, distant);
    CHECK(front == 0 && dry == 0 && high == 0);
    CHECK(ctl > 30000);
    CHECK(forced > 30000);
    CHECK(distant > 30000);
}

// ---- the cumulonimbus: a storm cell with an anvil growing out of its top, downwind
struct Tower { bool valid = false; float ratio = 0, shift = 0, top = 0, aspect = 0, cont = 0; std::vector<char> shape; };
// The silhouette measures of one cell (a blob in a view with the wind to the right). ratio: width at 90% of the height over
// width at 50%. shift: the top centroid minus the base centroid, in heights. aspect: the width of the core (at 30% of the
// height) over the height. cont: the share of the rows from 8% to 90% of the height that are cloud in the column of the
// base centroid (+-4% of the height): 1 is a tower with no gap up to its anvil.
static Tower statsOf(const Blob& tall, int W, int H) {
    Tower t;
    const int h = tall.y1 - tall.y0 + 1, w50 = widthAt(tall, W, 0.5f), w90 = widthAt(tall, W, 0.9f), w30 = widthAt(tall, W, 0.3f);
    if (w50 <= 0) return t;
    t.valid = true;
    t.ratio = (float)w90 / (float)w50;
    t.top = (float)w90 / (float)h;
    t.aspect = (float)w30 / (float)h;
    double top = 0, base = 0; int nt = 0, nb = 0;
    std::vector<char> occ((size_t)W * H, 0);
    for (int j : tall.px) {
        occ[(size_t)j] = 1;
        const int rowFromBottom = tall.y1 - j / W;
        if (rowFromBottom >= 0.86f * h) { top += j % W; nt++; }
        if (rowFromBottom <= 0.25f * h) { base += j % W; nb++; }
    }
    t.shift = nt && nb ? (float)((top / nt - base / nb) / h) : 0.f;
    if (nb) {
        const int cx = (int)std::lround(base / nb), half = std::max(1, (int)std::lround(0.04f * h));
        int rows = 0, full = 0;
        for (int r = (int)std::ceil(0.08f * h); r <= (int)(0.90f * h); r++) {
            const int y = tall.y1 - r;
            bool any = false;
            for (int x = cx - half; x <= cx + half && !any; x++) any = x >= 0 && x < W && occ[(size_t)y * W + x];
            rows++; full += any;
        }
        t.cont = rows ? (float)full / (float)rows : 0.f;
    }
    t.shape.assign((size_t)64 * 48, 0);
    const float cxm = 0.5f * (tall.x0 + tall.x1);
    for (int j : tall.px) {
        const int gx = (int)std::lround(32 + ((j % W) - cxm) / (h * 0.8f) * 32), gy = (int)((float)(tall.y1 - j / W) / h * 47);
        if (gx >= 0 && gx < 64 && gy >= 0 && gy < 48) t.shape[(size_t)gy * 64 + gx] = 1;
    }
    return t;
}
constexpr float kAnvilViewX = 12000.f;          // the cell sits this far to the upwind side of the view centre, so its anvil is in the frame
static g3d::ScenePtr anvilView(float x, float driftX, bool on) {
    const float cx = x - (driftX >= 0 ? 1.f : -1.f) * kAnvilViewX;
    auto s = filmClouds({ cx, 2, 38000 }, { cx, 5600, 0 }, 0.f, g3d::CloudType::Cumulonimbus, on, 1200, 11000, 75, false);
    s->sky.enabled = false;
    s->skyColor = BLACK;
    g3d::Material wm;
    wm.unlit = true;
    wm.color = BLACK;
    g3d::Object3D* wall = s->add(g3d::Geometry::box(300000, 90000, 10), wm);
    wall->position = { cx, 30000, -12000 };
    return s;
}
static std::vector<Tower> towers(float anvil, float variety, float driftX, const std::vector<Image>& offs, unsigned seed = 1) {
    constexpr int W = 480, H = 270;
    std::vector<Tower> out;
    for (int cam = 0; cam < (int)offs.size(); cam++) {
        const float x = (cam - 2.5f) * 3500.f + 4321.f * (float)(seed - 1);
        auto s = anvilView(x, driftX, true);
        s->clouds.haze = 0;
        s->clouds.seed = seed;
        s->clouds.anvil = anvil;
        s->clouds.towerVariety = variety;
        s->clouds.drift = { driftX, 0 };
        s->clouds.storms = { g3d::Clouds::Storm{ x, 0.f, 5500.f, 10500.f } };
        Image on = look(s, W, H, 0);
        const std::vector<Blob> bl = blobsOf(cloudMask(on, offs[(size_t)cam], 8), W, H);
        UnloadImage(on);
        const Blob* tall = nullptr;
        for (const Blob& b : bl) {
            if (b.border || b.y1 - b.y0 < 40 || b.area <= 1000) continue;
            if (!tall || b.area > tall->area) tall = &b;
        }
        out.push_back(tall ? statsOf(*tall, W, H) : Tower{});
    }
    return out;
}
// The median over the views where both have a tower of the share of the pixels they have in common, in a silhouette
// normalised by the tower height (so a bigger tower is not a different one).
static float iouBetween(const std::vector<Tower>& a, const std::vector<Tower>& b, int* pairs = nullptr) {
    std::vector<float> v;
    for (size_t i = 0; i < a.size() && i < b.size(); i++) {
        if (!a[i].valid || !b[i].valid) continue;
        int in = 0, un = 0;
        for (size_t k = 0; k < a[i].shape.size(); k++) { in += a[i].shape[k] && b[i].shape[k]; un += a[i].shape[k] || b[i].shape[k]; }
        if (un) v.push_back((float)in / un);
    }
    if (pairs) *pairs = (int)v.size();
    if (v.empty()) return 1.f;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}
static float medianOf(std::vector<float> v) {
    if (v.empty()) return 0.f;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}
// A synthetic silhouette in the manner of the removed layout: a pillar a quarter as wide as it is tall, and a flat plate
// floating over it with a gap between. The measures above must flag it (it is the control of the continuity and aspect checks).
static Tower pillarAndPlate() {
    constexpr int W = 480, H = 270, h = 200, y1 = 240;
    Blob b{ W, y1 - (h - 1), 0, y1, 0, {}, false };
    auto add = [&](int x0, int x1, int r0, int r1) {
        for (int r = r0; r <= r1; r++)
            for (int x = x0; x <= x1; x++) { b.px.push_back((y1 - r) * W + x); b.x0 = std::min(b.x0, x); b.x1 = std::max(b.x1, x); b.area++; }
    };
    add(200, 250, 0, (int)(0.62f * h));                 // the pillar: 50 px wide in 200 tall
    add(120, 380, (int)(0.80f * h), h - 1);             // the plate, with a gap under it
    return statsOf(b, W, H);
}
// A synthetic plain column, as wide at the top as in the middle (the control of the anvil width ratio).
static Tower columnOnly() {
    constexpr int W = 480, H = 270, h = 200, y1 = 240;
    Blob b{ 150, y1 - (h - 1), 330, y1, 0, {}, false };
    for (int r = 0; r < h; r++)
        for (int x = 150; x <= 330; x++) { b.px.push_back((y1 - r) * W + x); b.area++; }
    return statsOf(b, W, H);
}
static void anvilShape() {
    constexpr int W = 480, H = 270, CAMS = 6;
    auto offsFor = [&](unsigned seed, float driftX) {
        std::vector<Image> offs;
        for (int cam = 0; cam < CAMS; cam++) offs.push_back(look(anvilView((cam - 2.5f) * 3500.f + 4321.f * (float)(seed - 1), driftX, false), W, H, 0));
        return offs;
    };
    std::vector<Image> offs = offsFor(1, 10), offsUp = offsFor(1, -10), offs2 = offsFor(2, 10);
    const std::vector<Tower> on = towers(1, 1, 10, offs), upwind = towers(1, 1, -10, offsUp), clones = towers(1, 0, 10, offs),
                             other = towers(1, 1, 10, offs2, 2), again = towers(1, 1, 10, offs);
    auto med = [](const std::vector<Tower>& t, float Tower::* m, int* n = nullptr) {
        std::vector<float> v;
        for (const Tower& x : t) if (x.valid) v.push_back(x.*m);
        if (n) *n = (int)v.size();
        return medianOf(v);
    };
    int nOn = 0, pSeed = 0, pVar = 0;
    const float rOn = med(on, &Tower::ratio, &nOn), colRatio = columnOnly().ratio, sOn = med(on, &Tower::shift), sUp = med(upwind, &Tower::shift);
    const float aspOn = med(on, &Tower::aspect), contOn = med(on, &Tower::cont);
    const float iouSeed = iouBetween(on, other, &pSeed), iouSame = iouBetween(on, again), iouVar = iouBetween(on, clones, &pVar);
    const Tower pp = pillarAndPlate();
    // The count of cells in 60 x 60 km at the example settings, over seeds and places; and what the removed layout (one
    // possible centre in each 9 km square, present with probability min(1, 3.5 cover)) would put there on average.
    g3d::Clouds cl;
    cl.cover = 0.16f;
    int maxCells = 0, boxes = 0;
    double sumCells = 0;
    for (unsigned sd = 1; sd <= 20; sd++)
        for (int k = 0; k < 4; k++) {
            cl.seed = sd;
            const float ox = (float)(k % 2) * 60000.f - 80000.f, oz = (float)(k / 2) * 60000.f - 70000.f;
            const int n = (int)g3d::stormCells(cl, ox, oz, ox + 60000.f, oz + 60000.f).size();
            maxCells = std::max(maxCells, n); sumCells += n; boxes++;
        }
    const double oldCells = (60.0 / 9.0) * (60.0 / 9.0) * std::min(1.0, 3.5 * 0.16);
    std::printf("[weather] W5 anvil (%d cells): width at 90%% over 50%% %.2f (a plain column %.2f); top centroid minus base centroid, in heights: downwind %.2f, wind reversed %.2f; core width over height %.2f, column cloud up to the anvil %.2f; silhouette IoU for seed 1 and seed 2 %.2f (median of %d), the same again %.2f, with towerVariety 0 %.2f (%d)\n",
                nOn, rOn, colRatio, sOn, sUp, aspOn, contOn, iouSeed, pSeed, iouSame, iouVar, pVar);
    std::printf("[weather] W5 storm cells: in 60 x 60 km at cover 0.16 over %d boxes: at most %d, mean %.2f; the removed dense layout would put %.1f; synthetic pillar and plate: core width over height %.2f, column cloud %.2f\n",
                boxes, maxCells, sumCells / boxes, oldCells, pp.aspect, pp.cont);
    CHECK(nOn >= 4);
    CHECK(rOn > 1.2f);
    CHECK(colRatio < 1.2f);                                          // control: a plain column (synthetic) has no wider top
    CHECK(sOn > 0.05f);
    CHECK(sUp < sOn - 0.1f);                                         // control: the other way when the wind is
    CHECK(aspOn >= 0.5f);                                            // a storm cell is as wide as it is tall, not a pillar
    CHECK(pp.valid && pp.aspect < 0.5f);                             // control: the pillar-and-plate silhouette fails it
    CHECK(contOn >= 0.95f);                                          // no gap between the tower and the anvil
    CHECK(pp.valid && pp.cont < 0.9f);                               // control: a floating plate fails it
    CHECK(maxCells <= 4 && sumCells / boxes > 0.3);                  // few cells (and not none)
    CHECK(oldCells > 4);                                             // control: the dense layout had far more
    CHECK(pSeed >= 3 && iouSeed < 0.85f);
    CHECK(iouSame > 0.999f);                                         // control: the same seed is the same tower
    CHECK(pVar >= 3 && iouVar < 0.9f);                               // towerVariety changes the towers
    for (Image& i : offs) UnloadImage(i);
    for (Image& i : offsUp) UnloadImage(i);
    for (Image& i : offs2) UnloadImage(i);
}

// (h) pure and off means off.
static void w5Purity() {
    constexpr int W = 320, H = 180;
    // a frame alone equals the frame after others
    Image alone = look(boltScene(5), W, H, 33);
    Comp c = comp(boltScene(5), W, H);
    for (int f : { 60, 31, 41, 32 }) { Image x = render(c, f); UnloadImage(x); }
    Image later = render(c, 33);
    // lightning set but disabled, or far in the future: the plain picture
    Image p = look(boltScene(5, false), W, H, 33), q = look(boltScene(5, false), W, H, 33);
    auto future = boltScene(5);
    future->lightning.strikes[0].time = 100;
    Image f0 = look(future, W, H, 33), nofl = look(boltScene(5, false), W, H, 33);
    // a sky of cumulus: anvil on or off is the same picture (no towers)
    auto sky = [&](float anvil) {
        auto s = testClouds({ 0, 500, 0 }, { 0, 1500, -2000 }, 0.5f, g3d::CloudType::Cumulus, true, 1500, 3500, 60);
        s->clouds.anvil = anvil;
        return s;
    };
    Image s1 = look(sky(1), W, H, 0), s0 = look(sky(0), W, H, 0);
    const long long dAlone = diffImages(alone, later), dOff = diffImages(p, q), dFuture = diffImages(f0, nofl), dSky = diffImages(s1, s0);
    std::printf("[weather] W5 purity: frame alone vs after others %lld; disabled twice %lld; strike at 100 s vs none %lld; cumulus sky anvil 1 vs 0 %lld\n", dAlone, dOff, dFuture, dSky);
    CHECK(dAlone == 0);
    CHECK(dOff == 0);
    CHECK(dFuture == 0);
    CHECK(dSky == 0);
    UnloadImage(alone); UnloadImage(later); UnloadImage(p); UnloadImage(q); UnloadImage(f0); UnloadImage(nofl); UnloadImage(s1); UnloadImage(s0);
}

static void lightningStorm() {
    boltSeeds();
    boltWindow();
    strikeRate();
    cloudGlow();
    rainbowGeometry();
    rainbowAbsent();
    anvilShape();
    w5Purity();
}

// ---------------------------------------------------------------- W10: light in the air (volumetric shafts, aerial perspective)
static g3d::Material airBlack() {
    g3d::Material m;
    m.color = BLACK;
    m.roughness = 1;                   // lit, so that it casts a shadow (an unlit surface casts none); black, so that it adds nothing but its depth
    return m;
}
static float meanLuma(const Image& img, int cx, int cy, int r) {
    double s = 0;
    int n = 0;
    for (int y = std::max(cy - r, 0); y <= std::min(cy + r, img.height - 1); y++)
        for (int x = std::max(cx - r, 0); x <= std::min(cx + r, img.width - 1); x++) { s += luma(GetImageColor(img, x, y)); n++; }
    return n ? (float)(s / n) : 0.f;
}
// The mean light in a patch, in linear units times 1000 (levels are gamma-encoded: a ratio of light needs the linear number).
static float meanLin(const Image& img, int cx, int cy, int r) {
    double s = 0;
    int n = 0;
    for (int y = std::max(cy - r, 0); y <= std::min(cy + r, img.height - 1); y++)
        for (int x = std::max(cx - r, 0); x <= std::min(cx + r, img.width - 1); x++) { s += 1000.0 * std::pow(luma(GetImageColor(img, x, y)) / 255.0, 2.2); n++; }
    return n ? (float)(s / n) : 0.f;
}
static Vector3 meanRGB(const Image& img, int cx, int cy, int r) {
    double s[3] = { 0, 0, 0 };
    int n = 0;
    for (int y = cy - r; y <= cy + r; y++)
        for (int x = cx - r; x <= cx + r; x++) {
            if (x < 0 || y < 0 || x >= img.width || y >= img.height) continue;
            const Color c = GetImageColor(img, x, y);
            s[0] += c.r; s[1] += c.g; s[2] += c.b; n++;
        }
    return n ? Vector3{ (float)(s[0] / n), (float)(s[1] / n), (float)(s[2] / n) } : Vector3{ 0, 0, 0 };
}
static const char* qualityName(g3d::Volumetric::Quality q) {
    return q == g3d::Volumetric::Quality::Off ? "off" : q == g3d::Volumetric::Quality::Low ? "low" : q == g3d::Volumetric::Quality::Medium ? "medium" : "high";
}

// A roof with a long slot in it (3 m wide in x, 12 m long in z), the sun above and to +x: a sheet of light falls
// through the slot into fog. Everything is unlit black but the fog, so the picture is the fog's light alone.
constexpr float SLIT_SX = 0.53f, SLIT_SY = 0.85f;      // the way to the sun
static g3d::ScenePtr slitScene(g3d::Volumetric::Quality q, float aniso = 0.7f, bool shadow = true, float ambient = 0.05f, bool roof = true, bool wall = true) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { -2.5f, 2, 30 };
    s->camera.target = { -2.5f, 4, 0 };
    s->camera.fov = 38;
    s->camera.farPlane = 400;
    s->ambient = 0;
    s->fogMode = g3d::FogMode::Exponential;
    s->fogDensity = 0.03f;
    s->fogColor = { 120, 120, 120, 255 };
    s->shadowMapSize = 2048;
    const g3d::Material black = airBlack();
    if (roof) {
        s->add(g3d::Geometry::box(98.5f, 0.4f, 120), black)->position = { -50.75f, 8, 0 };
        s->add(g3d::Geometry::box(98.5f, 0.4f, 120), black)->position = { 50.75f, 8, 0 };
        s->add(g3d::Geometry::box(3.0f, 0.4f, 54), black)->position = { 0, 8, 33 };
        s->add(g3d::Geometry::box(3.0f, 0.4f, 54), black)->position = { 0, 8, -33 };
        if (wall) s->add(g3d::Geometry::box(400, 120, 1), black)->position = { 0, 20, -70 };
    }
    g3d::Light sun = g3d::Light::directional({ SLIT_SX * 60, SLIT_SY * 60, 0 }, WHITE, 2, shadow);
    sun.shadowExtent = 40;
    s->lights.push_back(sun);
    s->volumetric.quality = q;
    s->volumetric.anisotropy = aniso;
    s->volumetric.ambient = ambient;
    return s;
}
// The image position of the shaft's centre line at height y (it crosses the roof at x = 0 and leans towards -x).
static Vector2 slitPoint(const g3d::Camera& cam, float y, float dx, int w, int h) {
    return project(cam, { (y - 8) * SLIT_SX / SLIT_SY + dx, y, 0 }, w, h);
}
static float slitFactor(const Image& img, const g3d::Camera& cam, int w, int h, float* onOut = nullptr, float* offOut = nullptr) {
    double on = 0, off = 0;
    int n = 0;
    for (float y : { 2.5f, 3.5f, 4.5f, 5.5f, 6.5f }) {
        const Vector2 a = slitPoint(cam, y, 0, w, h), b = slitPoint(cam, y, -6, w, h), c = slitPoint(cam, y, 6, w, h);
        on += meanLin(img, (int)a.x, (int)a.y, 2);
        off += 0.5 * (meanLin(img, (int)b.x, (int)b.y, 2) + meanLin(img, (int)c.x, (int)c.y, 2));
        n++;
    }
    if (onOut) *onOut = (float)(on / n);
    if (offOut) *offOut = (float)(off / n);
    return (float)((on / n) / std::max(off / n, 1.0));
}

// (a) The shaft is where the sun gets through the slot, found by the shadow map: on its line the fog is much brighter than beside it.
static void slitShaft() {
    constexpr int W = 480, H = 270;
    for (auto q : { g3d::Volumetric::Quality::Low, g3d::Volumetric::Quality::Medium, g3d::Volumetric::Quality::High }) {
        auto sc = slitScene(q);
        Image img = look(sc, W, H, 3);
        float on = 0, off = 0;
        const float f = slitFactor(img, sc->camera, W, H, &on, &off);
        std::printf("[weather] W10 slit shaft (%s): on the line %.1f, beside it %.1f (linear light x 1000), factor %.2f (want over 2)\n", qualityName(q), on, off, f);
        CHECK(f > 2.f);
        if (q == g3d::Volumetric::Quality::Medium) dump("w10-slit.png", img);
        UnloadImage(img);
    }
    // No shadow on the sun (it lights all the fog alike, the negative control): no shaft.
    auto sc = slitScene(g3d::Volumetric::Quality::Medium, 0.7f, false);
    Image flat = look(sc, W, H, 3);
    float on = 0, off = 0;
    const float f = slitFactor(flat, sc->camera, W, H, &on, &off);
    std::printf("[weather] W10 slit, no shadow map on the sun (uniform light, the negative control): factor %.2f (want under 1.15)\n", f);
    CHECK(f < 1.15f);
    UnloadImage(flat);
}

// (b) Off means off: the fields set but quality Off, aerial perspective with enabled false: the same pixels as a scene that never set them.
static g3d::ScenePtr airStreet(bool setFields) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 3, 0 };
    s->camera.target = { 0, 2, -30 };
    s->camera.fov = 50;
    s->ambient = 0.5f;
    s->fogMode = g3d::FogMode::Exponential;
    s->fogDensity = 0.03f;
    s->fogColor = { 184, 200, 216, 255 };
    s->shadowMapSize = 1024;
    g3d::Material ground;
    ground.color = { 80, 120, 60, 255 };
    ground.roughness = 1;
    s->add(g3d::Geometry::plane(400, 400), ground);
    const Color cols[3] = { { 200, 60, 50, 255 }, { 60, 90, 210, 255 }, { 220, 200, 70, 255 } };
    const float zs[3] = { -8, -20, -45 };
    for (int i = 0; i < 3; i++) {
        g3d::Material m;
        m.color = cols[i];
        m.roughness = 0.8f;
        s->add(g3d::Geometry::box(4, 6, 3), m)->position = { -6.f + 6.f * i, 3, zs[i] };
    }
    s->lights.push_back(g3d::Light::directional({ 20, 30, 14 }, WHITE, 2));
    if (setFields) {
        s->volumetric.quality = g3d::Volumetric::Quality::Off;
        s->volumetric.density = 0.2f;
        s->volumetric.heightFalloff = 0.3f;
        s->volumetric.anisotropy = 0.9f;
        s->volumetric.scatterLights = true;
        s->aerial.enabled = false;
        s->aerial.strength = 9;
        s->aerial.scaleHeight = 100;
    }
    return s;
}
static void airOffMeansOff() {
    Image a = look(airStreet(false), 320, 180, 4), b = look(airStreet(true), 320, 180, 4);
    const long long d = diffImages(a, b);
    std::printf("[weather] W10 off means off: volumetric Off and aerial disabled, with every other field set, differ from a scene that never set them by %lld (want 0)\n", d);
    CHECK(d == 0);
    UnloadImage(a); UnloadImage(b);
}

// (c) Looking toward the sun the fog is brighter than looking away (forward scattering).
static g3d::ScenePtr phaseScene(bool toward, float aniso) {
    auto s = std::make_shared<g3d::Scene>();
    const Vector3 sun = Vector3Normalize({ 0.94f, 0.34f, 0 });
    s->camera.position = { 0, 0, 0 };
    s->camera.target = { toward ? 10.f : -10.f, 0, 0 };
    s->camera.fov = 30;
    s->camera.farPlane = 300;
    s->ambient = 0;
    s->fogMode = g3d::FogMode::Exponential;
    s->fogDensity = 0.02f;
    s->fogColor = { 90, 90, 90, 255 };
    g3d::Material black = airBlack();
    black.unlit = true;                // nothing here needs to cast a shadow
    s->add(g3d::Geometry::box(1, 200, 300), black)->position = { 60, 0, 0 };
    s->add(g3d::Geometry::box(1, 200, 300), black)->position = { -60, 0, 0 };
    s->lights.push_back(g3d::Light::directional(Vector3Scale(sun, 50), WHITE, 2));
    s->volumetric.quality = g3d::Volumetric::Quality::Medium;
    s->volumetric.anisotropy = aniso;
    s->volumetric.ambient = 0.1f;
    return s;
}
static void phaseForward() {
    Image to = look(phaseScene(true, 0.7f), 160, 90, 2), away = look(phaseScene(false, 0.7f), 160, 90, 2);
    Image to0 = look(phaseScene(true, 0), 160, 90, 2), away0 = look(phaseScene(false, 0), 160, 90, 2);
    const float t = meanLin(to, 80, 45, 20), a = meanLin(away, 80, 45, 20), t0 = meanLin(to0, 80, 45, 20), a0 = meanLin(away0, 80, 45, 20);
    std::printf("[weather] W10 phase: toward the sun %.1f, away %.1f (linear light x 1000; ratio %.2f, want over 2); with anisotropy 0 toward %.1f, away %.1f (ratio %.2f, want 1.0 +- 0.05)\n",
                t, a, t / std::max(a, 0.5f), t0, a0, t0 / std::max(a0, 0.5f));
    CHECK(t > 2.f * a);
    CHECK(std::fabs(t0 / std::max(a0, 0.5f) - 1.f) < 0.05f);
    UnloadImage(to); UnloadImage(away); UnloadImage(to0); UnloadImage(away0);
}

// (d) No double fog: with nothing to shade the march, and no phase, the volumetric fog is the analytic one.
static void noDoubleFog() {
    constexpr int W = 320, H = 180;
    auto off = airStreet(false);
    auto on = airStreet(false);
    on->volumetric.quality = g3d::Volumetric::Quality::Medium;
    on->volumetric.anisotropy = 0;
    Image a = look(off, W, H, 4), b = look(on, W, H, 4);
    std::vector<float> d;
    double sum = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const Color p = GetImageColor(a, x, y), q = GetImageColor(b, x, y);
            const float v = (std::abs(p.r - q.r) + std::abs(p.g - q.g) + std::abs(p.b - q.b)) / 3.f;
            d.push_back(v);
            sum += v;
        }
    std::sort(d.begin(), d.end());
    const float mean = (float)(sum / d.size()), p99 = d[(size_t)(d.size() * 0.99)];
    // The same with the other kind of fog: one that thins with height.
    auto offH = airStreet(false), onH = airStreet(false);
    for (auto* sc : { offH.get(), onH.get() }) { sc->fogHeight = 0; sc->fogFalloff = 0.08f; sc->fogDensity = 0.05f; }
    onH->volumetric.quality = g3d::Volumetric::Quality::Medium;
    onH->volumetric.anisotropy = 0;
    Image c = look(offH, W, H, 4), e = look(onH, W, H, 4);
    double sumH = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const Color p = GetImageColor(c, x, y), q = GetImageColor(e, x, y);
            sumH += (std::abs(p.r - q.r) + std::abs(p.g - q.g) + std::abs(p.b - q.b)) / 3.0;
        }
    const float meanH = (float)(sumH / (W * H));
    std::printf("[weather] W10 no double fog: exponential fog analytic vs marched: mean %.2f levels, 99th percentile %.1f (want mean under 1.5, p99 under 6); height fog mean %.2f (want under 1.5)\n", mean, p99, meanH);
    CHECK(mean < 1.5f);
    CHECK(p99 < 6.f);
    CHECK(meanH < 1.5f);
    dump("w10-fog-analytic.png", a); dump("w10-fog-marched.png", b);
    UnloadImage(a); UnloadImage(b); UnloadImage(c); UnloadImage(e);
}

// (e) Aerial perspective: identical grey boxes at 1, 5, 15 and 40 km (each subtends the same angle, just above the horizon).
static g3d::ScenePtr ridgeBoxes(bool boxes, bool aerial, float scaleHeight = 2500) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 1.7f, 0 };
    s->camera.target = { 0, 1.7f, -20000.f };
    s->camera.fov = 30;
    s->camera.nearPlane = 50;
    s->camera.farPlane = 120000;
    s->sky = g3d::Sky::day();
    s->sky.hazeAmount = 0;
    s->sky.clouds = 0;
    s->sky.horizon = { 150, 185, 235, 255 };
    s->aerial.enabled = aerial;
    s->aerial.scaleHeight = scaleHeight;
    if (boxes) {
        g3d::Material m;
        m.color = { 40, 40, 40, 255 };
        m.unlit = true;
        const float ds[4] = { 1000, 5000, 15000, 40000 }, az[4] = { -9, -3, 3, 9 };
        for (int i = 0; i < 4; i++) {
            const float sz = ds[i] * 0.04f;
            s->add(g3d::Geometry::box(sz, sz, sz), m)->position = { ds[i] * std::tan(az[i] * DEG2RAD), ds[i] * 0.045f, -ds[i] };
        }
    }
    return s;
}
static void aerialDistance() {
    constexpr int W = 640, H = 360;
    auto on = ridgeBoxes(true, true);
    Image img = look(on, W, H, 2), plain = look(ridgeBoxes(true, false), W, H, 2), sky = look(ridgeBoxes(false, true), W, H, 2);
    const float ds[4] = { 1000, 5000, 15000, 40000 }, az[4] = { -9, -3, 3, 9 };
    float dist[4], br[4], brOff[4];
    for (int i = 0; i < 4; i++) {
        const Vector2 p = project(on->camera, { ds[i] * std::tan(az[i] * DEG2RAD), ds[i] * 0.045f, -ds[i] }, W, H);
        const Vector3 c = meanRGB(img, (int)p.x, (int)p.y, 3), s = meanRGB(sky, (int)p.x, (int)p.y, 3), o = meanRGB(plain, (int)p.x, (int)p.y, 3);
        dist[i] = (std::fabs(c.x - s.x) + std::fabs(c.y - s.y) + std::fabs(c.z - s.z)) / 3.f;
        br[i] = c.z / std::max(c.x, 1.f);
        brOff[i] = o.z / std::max(o.x, 1.f);
        std::printf("[weather] W10 aerial %5.0f km: box (%.0f, %.0f, %.0f), the sky there (%.0f, %.0f, %.0f), distance to it %.1f, blue/red %.3f (off: %.3f)\n",
                    ds[i] / 1000, c.x, c.y, c.z, s.x, s.y, s.z, dist[i], br[i], brOff[i]);
    }
    CHECK(dist[0] > dist[1] && dist[1] > dist[2] && dist[2] > dist[3]);
    CHECK(dist[3] < 0.35f * dist[0]);
    CHECK(br[0] < br[1] && br[1] < br[2] && br[2] < br[3]);
    CHECK(brOff[3] < 1.05f);                                 // without it the grey box stays grey
    dump("w10-aerial.png", img);
    UnloadImage(img); UnloadImage(plain); UnloadImage(sky);

    // From 3 km up the same box 15 km off is hazier less: the haze thins with altitude.
    auto fade = [&](float eye, float scaleHeight) {
        auto sc = ridgeBoxes(true, true, scaleHeight);
        sc->camera.position.y = eye;
        sc->camera.target.y = eye > 100 ? eye - 3000.f : eye;
        Image a = look(sc, W, H, 2);
        auto pl = ridgeBoxes(true, false, scaleHeight);
        pl->camera = sc->camera;
        Image c = look(pl, W, H, 2);
        const Vector2 p = project(sc->camera, { 15000.f * std::tan(3 * DEG2RAD), 15000.f * 0.045f, -15000.f }, W, H);
        const Vector3 col = meanRGB(a, (int)p.x, (int)p.y, 3), plainCol = meanRGB(c, (int)p.x, (int)p.y, 3);
        UnloadImage(a); UnloadImage(c);
        // The share of the way from the box's grey to the horizon colour (red 150) the red channel has gone.
        return (col.x - plainCol.x) / (150.f - plainCol.x);
    };
    const float low = fade(1.7f, 2500), high = fade(3000, 2500), highFlat = fade(3000, 1e7f), lowFlat = fade(1.7f, 1e7f);
    std::printf("[weather] W10 aerial altitude: red haze share of a box 15 km off: %.3f from the ground, %.3f from 3 km up (want under 0.8x); with no altitude dependence (scaleHeight 1e7) %.3f and %.3f (want equal)\n",
                low, high, lowFlat, highFlat);
    CHECK(low > 0.05f);
    CHECK(high < 0.8f * low);
    CHECK(std::fabs(highFlat - lowFlat) < 0.05f * std::max(lowFlat, 0.01f));
}

// (f) Purity and stability: a frame alone is the frame after others; a static scene does not change from frame to frame;
// and the march's jitter is gone in a flat part of the shaft.
static void airPurity() {
    constexpr int W = 480, H = 270;
    Image alone = look(slitScene(g3d::Volumetric::Quality::Medium), W, H, 5);
    Comp c = comp(slitScene(g3d::Volumetric::Quality::Medium), W, H);
    Image x1 = render(c, 11), x2 = render(c, 40), later = render(c, 5), twice = render(c, 5), next = render(c, 6);
    const long long dAlone = diffImages(alone, later), dTwice = diffImages(later, twice), dNext = diffImages(later, next);
    std::printf("[weather] W10 purity: frame alone vs after others %lld; two renders of one frame %lld; frame 5 vs 6 of a static scene %lld (all want 0)\n", dAlone, dTwice, dNext);
    CHECK(dAlone == 0);
    CHECK(dTwice == 0);
    CHECK(dNext == 0);
    // Noise: a 9 x 9 patch on the line, minus its own 5 x 5 mean (the shaft is smooth), at each quality.
    for (auto q : { g3d::Volumetric::Quality::Low, g3d::Volumetric::Quality::Medium, g3d::Volumetric::Quality::High }) {
        auto sc = slitScene(q);
        Image img = look(sc, W, H, 5);
        const Vector2 p = slitPoint(sc->camera, 4.5f, 0, W, H);
        double sum = 0;
        int n = 0;
        for (int dy = -4; dy <= 4; dy++)
            for (int dx = -4; dx <= 4; dx++) {
                const int px = (int)p.x + dx, py = (int)p.y + dy;
                sum += std::pow(luma(GetImageColor(img, px, py)) - meanLuma(img, px, py, 2), 2);
                n++;
            }
        const float rms = (float)std::sqrt(sum / n);
        std::printf("[weather] W10 shaft noise (%s): rms %.2f levels about the local mean, shaft %.1f levels (want rms under 1.0)\n", qualityName(q), rms, meanLuma(img, (int)p.x, (int)p.y, 4));
        CHECK(rms < 1.0f);
        UnloadImage(img);
    }
    UnloadImage(alone); UnloadImage(x1); UnloadImage(x2); UnloadImage(later); UnloadImage(twice); UnloadImage(next);
}

// (h) A spot light's cone shows in the fog when scatterLights is on, and does not when it is off.
static g3d::ScenePtr coneScene(bool scatter) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 2, 12 };
    s->camera.target = { 0, 3, 0 };
    s->camera.fov = 40;
    s->ambient = 0;
    s->fogMode = g3d::FogMode::Exponential;
    s->fogDensity = 0.04f;
    s->fogColor = { 20, 20, 20, 255 };
    g3d::Material wall = airBlack();
    wall.unlit = true;
    s->add(g3d::Geometry::box(200, 100, 1), wall)->position = { 0, 20, -30 };
    g3d::Light spot = g3d::Light::spot({ 0, 8, 0 }, { 0, 0, 0 }, { 255, 240, 220, 255 }, 6, 25);
    spot.range = 20;
    spot.innerAngle = 10;
    s->lights.push_back(spot);
    s->volumetric.quality = g3d::Volumetric::Quality::Medium;
    s->volumetric.scatterLights = scatter;
    return s;
}
static void spotCone() {
    constexpr int W = 320, H = 180;
    float ratio[2];
    for (int k = 0; k < 2; k++) {
        auto sc = coneScene(k == 0);
        Image img = look(sc, W, H, 2);
        const Vector2 a = project(sc->camera, { 0, 4, 0 }, W, H), b = project(sc->camera, { -5, 4, 0 }, W, H), c = project(sc->camera, { 5, 4, 0 }, W, H);
        const float on = meanLin(img, (int)a.x, (int)a.y, 2), off = 0.5f * (meanLin(img, (int)b.x, (int)b.y, 2) + meanLin(img, (int)c.x, (int)c.y, 2));
        ratio[k] = on / std::max(off, 1.f);
        std::printf("[weather] W10 spot cone, scatterLights %s: on the axis %.1f, outside the cone %.1f (linear x 1000), ratio %.2f\n", k == 0 ? "on" : "off", on, off, ratio[k]);
        if (k == 0) dump("w10-cone.png", img);
        UnloadImage(img);
    }
    CHECK(ratio[0] > 3.f);
    CHECK(ratio[1] < 1.15f);
}

// (i) Foliage silhouettes: a vertical card of 0.3 m clear and 0.3 m solid stripes (8 px each) over the left half of the shaft, the clear
// stripes seen against the lit fog. The depth-aware filter drops the taps across every edge, and where few were left the
// march's jitter showed as a dot screen. The fog seen through the stripes must be as clean as the same shaft seen free.
static g3d::ScenePtr canopyScene(bool casts, bool wall);
static float stripeNoise(g3d::Volumetric::Quality q, float* nearOut, float* farOut, Image* keep = nullptr) {
    constexpr int W = 480, H = 270;
    static std::string tex;
    if (tex.empty()) {
        Image img = GenImageColor(256, 4, WHITE);
        for (int y = 0; y < 4; y++) for (int x = 0; x < 256; x++) if ((x / 2) % 2 == 0) ImageDrawPixel(&img, x, y, Color{ 255, 255, 255, 0 });
        fs::create_directories("out/tmp-tests");
        tex = (fs::path("out/tmp-tests") / "w10-stripes.png").string();
        ExportImage(img, tex.c_str());
        UnloadImage(img);
    }
    auto sc = slitScene(q);
    g3d::Material leaf = airBlack();
    leaf.map = tex;
    leaf.alphaCutoff = 0.5f;
    leaf.doubleSided = true;
    auto* card = sc->add(g3d::Geometry::plane(38, 14), leaf);
    card->position = { -21.5f, 3, 15 };
    card->rotation = { 90, 0, 0 };
    Image img = look(sc, W, H, 3);
    const Vector2 lo = slitPoint(sc->camera, 6.5f, 0, W, H), hi = slitPoint(sc->camera, 2.5f, 0, W, H);
    const Vector2 edge = project(sc->camera, { -2.5f, 4, 15 }, W, H);            // the card's right edge: the left of it is striped
    auto hf = [&](int x, int y) {                                                   // the pixel against its 3 x 3 mean
        double m = 0;
        for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) m += luma(GetImageColor(img, x + i, y + j));
        return luma(GetImageColor(img, x, y)) - m / 9.0;
    };
    double sn = 0, sf = 0;
    int nn = 0, nf = 0;
    // Striped side: the pixels in the clear stripes (the bright ones: a solid stripe is black) at least 2 px from a stripe's edge.
    for (int y = (int)lo.y; y <= (int)hi.y; y++) {
        float mn = 1e9f, mx = -1e9f;                                                // a clear stripe sees the fog to the far wall: brighter than a solid one
        for (int x = (int)edge.x - 26; x < (int)edge.x - 2; x++) { const float l = luma(GetImageColor(img, x, y)); mn = std::min(mn, l); mx = std::max(mx, l); }
        const float mid = 0.5f * (mn + mx);
        for (int x = (int)edge.x - 22; x < (int)edge.x - 2; x++) {
            bool clear = mx - mn > 3.f;
            for (int i = -2; i <= 2; i++) if (luma(GetImageColor(img, x + i, y)) < mid) clear = false;
            if (!clear) continue;
            const double e = hf(x, y);
            sn += e * e; nn++;
        }
    }
    // Free side: the same number of columns of the shaft, to the right of the card.
    for (int y = (int)lo.y; y <= (int)hi.y; y++)
        for (int x = (int)edge.x + 3; x < (int)edge.x + 23; x++) { const double e = hf(x, y); sf += e * e; nf++; }
    *nearOut = nn ? (float)std::sqrt(sn / nn) : -1.f;
    *farOut = (float)std::sqrt(sf / std::max(nf, 1));
    if (keep) *keep = img; else UnloadImage(img);
    return *nearOut / std::max(*farOut, 0.4f);
}
static void edgeNoise() {
    for (auto q : { g3d::Volumetric::Quality::Low, g3d::Volumetric::Quality::Medium, g3d::Volumetric::Quality::High }) {
        float n = 0, f = 0;
        Image img{};
        const float ratio = stripeNoise(q, &n, &f, &img);
        std::printf("[weather] W10 silhouette noise (%s): high-frequency rms in the fog between foliage edges %.2f levels, in the same shaft with no foliage %.2f (ratio %.2f, want at most 1.5)\n", qualityName(q), n, f, ratio);
        CHECK(n >= 0 && ratio <= 1.5f);
        if (q == g3d::Volumetric::Quality::Medium) dump("w10-stripes.png", img);
        UnloadImage(img);
    }
}

// (j) The shadow of cut-out foliage: a canopy of one alpha-tested card with a 3 m clear strip makes a shaft, like the slot of
// the roof does. With the card casting no shadow there is none.
static g3d::ScenePtr canopyScene(bool casts, bool wall) {
    static std::string tex;
    if (tex.empty()) {
        Image img = GenImageColor(256, 4, WHITE);
        for (int y = 0; y < 4; y++) for (int x = 124; x < 132; x++) ImageDrawPixel(&img, x, y, Color{ 255, 255, 255, 0 });
        fs::create_directories("out/tmp-tests");
        tex = (fs::path("out/tmp-tests") / "w10-canopy.png").string();
        ExportImage(img, tex.c_str());
        UnloadImage(img);
    }
    auto s = slitScene(g3d::Volumetric::Quality::Medium, 0.7f, true, 0.05f, false);
    s->camera.position.y = 2; s->camera.target.y = 4;

    g3d::Material leaf = airBlack();
    leaf.map = tex;
    leaf.alphaCutoff = 0.5f;
    leaf.doubleSided = true;
    leaf.castShadow = casts;
    s->add(g3d::Geometry::plane(100, 120), leaf)->position = { 0, 8, 0 };
    if (wall) s->add(g3d::Geometry::box(400, 120, 1), airBlack())->position = { 0, 20, -70 };
    return s;
}
static void canopyShaft() {
    constexpr int W = 480, H = 270;
    float f[2];
    for (int k = 0; k < 2; k++) {
        auto sc = canopyScene(k == 0, true);
        Image img = look(sc, W, H, 3);
        float on = 0, off = 0;
        f[k] = slitFactor(img, sc->camera, W, H, &on, &off);
        std::printf("[weather] W10 canopy of cut-out cards, cards %s: on the gap's line %.1f, beside it %.1f (linear x 1000), contrast %.2f (want over 2 casting, under 1.15 not)\n", k == 0 ? "cast" : "do not cast", on, off, f[k]);
        if (k == 0) dump("w10-canopy.png", img);
        UnloadImage(img);
    }
    CHECK(f[0] > 2.f);
    CHECK(f[1] < 1.15f);
}

// (k) The sea is not in the depth buffer, so land under the water must not be hazed through it: over a submerged shelf the sea pixels
// are the same with the aerial perspective on and off (it belongs to what is in front of the sea; W9's haze is the sea's own).
static void aerialOverSea() {
    constexpr int W = 320, H = 180;
    Image img[2];
    for (int k = 0; k < 2; k++) {
        auto s = ridgeBoxes(false, true);
        s->camera.position = { 0, 6, 0 };
        s->camera.target = { 0, 4, -20000.f };
        s->water = g3d::Water::calm();
        s->water.level = 0;
        s->aerial.strength = 4;
        s->aerial.enabled = k == 0;
        {
            g3d::Material m;
            m.color = { 40, 60, 30, 255 };
            m.unlit = true;
            s->add(g3d::Geometry::box(40000, 20, 20000), m)->position = { 0, -30, -16000 };
        }
        img[k] = look(s, W, H, 3);
    }
    float worst = 0;
    for (int y = H / 2 + 6; y < H / 2 + 40; y += 6) {
        const Vector3 a = meanRGB(img[0], W / 2, y, 6), b = meanRGB(img[1], W / 2, y, 6);
        worst = std::max(worst, std::max({ std::fabs(a.x - b.x), std::fabs(a.y - b.y), std::fabs(a.z - b.z) }));
    }
    std::printf("[weather] W10 aerial over the sea: sea pixels over a submerged shelf differ with aerial perspective on and off by %.1f levels at worst (want under 3)\n", worst);
    CHECK(worst < 3.f);
    dump("w10-sea-shelf.png", img[0]);
    UnloadImage(img[0]); UnloadImage(img[1]);
}

static void lightInAir() {
    slitShaft();
    spotCone();
    airOffMeansOff();
    phaseForward();
    noDoubleFog();
    aerialDistance();
    airPurity();
    edgeNoise();
    canopyShaft();
    aerialOverSea();
}

// (g) The cost at 1080p: the fog with shafts off, and each quality, over a street of 400 boxes (the march does not depend on the geometry).
static void airTiming() {
    constexpr int W = 1920, H = 1080;
    for (int row = 0; row < 5; row++) {
        auto s = slitScene(g3d::Volumetric::Quality::Off);
        for (int i = 0; i < 400; i++) {
            g3d::Material m;
            m.color = { 120, 110, 100, 255 };
            s->add(g3d::Geometry::box(1, 2, 1), m)->position = { -30.f + 3.f * (float)(i % 20), 1, -60.f + 6.f * (float)(i / 20) };
        }
        s->volumetric.quality = (g3d::Volumetric::Quality)(row == 4 ? 3 : row);
        if (row == 4) s->aerial.enabled = true;
        Comp c = comp(s, W, H);
        Canvas canvas;
        canvas.load(W, H, 1);
        presentFrame(c, 5, canvas);
        UnloadImage(LoadImageFromTexture(canvas.output().texture));
        double best = 1e9, mean = 0;
        for (int i = 0; i < 12; i++) {
            const double t0 = GetTime();
            presentFrame(c, 5 + i, canvas);
            Image img = LoadImageFromTexture(canvas.output().texture);
            const double dt = (GetTime() - t0) * 1000;
            UnloadImage(img);
            best = std::min(best, dt);
            mean += dt / 12;
        }
        canvas.unload();
        std::printf("[weather] W10 timing 1080p, %s: best %.1f ms, mean %.1f ms\n",
                    row == 0 ? "analytic fog (off)" : row == 1 ? "low" : row == 2 ? "medium" : row == 3 ? "high" : "high + aerial perspective", best, mean);
    }
}

// ================================================================ W11: camera weather (rmtn/lens.hpp)
// Small 2D compositions with fx::lens on a layer, read back from the canvas. The CPU twin (fx::lensDrops) says where each
// drop is; the pixels say where it was drawn; the checks tie the two together.
static void lensRamps(const Ctx&, int) {            // R rises to the right, G down the frame: a flipped drop shows both reversed
    DrawRectangleGradientEx({ 0, 0, 640, 360 }, BLACK, GREEN, YELLOW, RED);
}
static void lensChecker(const Ctx&, int) {          // mid grey with 4 px checks: scatter and blur change it everywhere
    DrawRectangle(0, 0, 640, 360, { 128, 128, 128, 255 });
    for (int y = 0; y < 360; y += 4)
        for (int x = (y / 4) % 2 * 4; x < 640; x += 8) DrawRectangle(x, y, 4, 4, { 92, 92, 100, 255 });
}
static void lensDark(const Ctx&, int) { DrawRectangle(0, 0, 640, 360, { 20, 22, 30, 255 }); }
static Comp lensComp(const fx::Lens* l, int w, int h, std::function<void(const Ctx&, int)> fn) {
    Comp c;
    c.w = w; c.h = h; c.fps = 30; c.duration = 4000; c.background = BLACK;
    TNode* n = addRoot(c.roots, 0, 4000);
    n->fn = std::move(fn);
    Layer& layer = n->isolate();
    if (l) layer.filters.push_back(fx::lens(*l));
    return c;
}
static Image lensImg(const fx::Lens* l, int frame, std::function<void(const Ctx&, int)> fn = lensRamps, int w = 640, int h = 360) {
    Comp c = lensComp(l, w, h, std::move(fn));
    return render(c, frame);
}
static int frameOf(double sec) { return (int)std::lround(sec * 30.0); }
static bool differs(Color a, Color b, int tol = 0) {
    return std::abs(a.r - b.r) > tol || std::abs(a.g - b.g) > tol || std::abs(a.b - b.b) > tol;
}

// (c) Everything at zero is the image as it was, to the last level.
static void lensOffMeansOff() {
    fx::Lens zero;
    zero.frost.startFrame = 10; zero.frost.endFrame = 50;      // a creep to 0 is 0
    zero.rain.blurBehind = 12; zero.rain.size = 1.4f;          // settings without a rate do nothing
    for (int f : { 0, 77, 301 }) {
        Image a = lensImg(nullptr, f), b = lensImg(&zero, f);
        const long long d = diffImages(a, b);
        std::printf("[weather] W11 off: diff at frame %d = %lld\n", f, d);
        CHECK(d == 0);
        UnloadImage(a); UnloadImage(b);
    }
    // and a wiper with no blade and no rain changes nothing either
    fx::Lens wip; wip.wiper.period = 2; wip.wiper.drawBlade = false;
    Image a = lensImg(nullptr, 90), b = lensImg(&wip, 90);
    CHECK(diffImages(a, b) == 0);
    UnloadImage(a); UnloadImage(b);
}

// (a) Drops stay put and slide at the documented speed, in the twin and in the pixels.
static void lensDropsStayAndSlide() {
    fx::Lens still;
    still.rain.rate = 1; still.rain.slide = 0; still.rain.seed = 4;
    int shared = 0, moved = 0;
    {
        const auto a = fx::lensDrops(still, 12.0, 640, 360), b = fx::lensDrops(still, 12.5, 640, 360);
        for (const auto& p : a)
            for (const auto& q : b)
                if (p.id == q.id) { shared++; if (std::hypot(p.pos.x - q.pos.x, p.pos.y - q.pos.y) > 1e-3f) moved++; }
        std::printf("[weather] W11 static: %zu drops, %d kept over 0.5 s, %d moved\n", a.size(), shared, moved);
        CHECK(a.size() > 80 && shared > 60 && moved == 0);
    }
    // a sliding drop, from the twin; then the same drop in the pixels at two frames
    fx::Lens run;
    run.rain.rate = 0.12f; run.rain.size = 1.5f; run.rain.seed = 4;
    const float dt = 0.3f;
    bool found = false;
    fx::LensDrop d1, d2;
    int f1 = 0, f2 = 0;
    for (double s = 3; s < 40 && !found; s += 0.25) {
        const int fa = frameOf(s), fb = fa + frameOf(dt);
        const auto A = fx::lensDrops(run, fa / 30.0, 640, 360), B = fx::lensDrops(run, fb / 30.0, 640, 360);
        for (const auto& p : A) {
            if (p.kind != fx::LensDrop::Kind::Rain || !p.sliding || p.radius < 13 || p.pos.x < 60 || p.pos.x > 580 || p.pos.y < 60 || p.pos.y > 240) continue;
            for (const auto& q : B) {
                if (q.id != p.id || !q.sliding) continue;
                bool alone = true;
                for (const auto* L : { &A, &B })
                    for (const auto& o : *L)
                        if (o.id != p.id && o.kind != fx::LensDrop::Kind::Trail && std::hypot(o.pos.x - p.pos.x, o.pos.y - p.pos.y) < 2.6f * p.radius + o.radius + 6) alone = false;
                if (alone) { found = true; d1 = p; d2 = q; f1 = fa; f2 = fb; }
            }
            if (found) break;
        }
    }
    CHECK(found);
    if (!found) return;
    const double span = (f2 - f1) / 30.0;
    const float twinSpeed = (d2.pos.y - d1.pos.y) / (float)span;
    Image a = lensImg(&run, f1), b = lensImg(&run, f2), src = lensImg(nullptr, 0);
    auto centroid = [&](const Image& img, fx::LensDrop d, float& cx) {
        double sy = 0, sx = 0, n = 0;
        const int x0 = (int)(d.pos.x - 1.5f * d.radius), x1 = (int)(d.pos.x + 1.5f * d.radius);
        const int y0 = (int)(d.pos.y - 1.5f * d.radius), y1 = (int)(d.pos.y + 1.5f * d.radius);
        for (int y = std::max(y0, 0); y < std::min(y1, 360); y++)
            for (int x = std::max(x0, 0); x < std::min(x1, 640); x++)
                if (std::hypot(x - d.pos.x, y - d.pos.y) < 1.3f * d.radius && differs(GetImageColor(img, x, y), GetImageColor(src, x, y), 3)) { sy += y; sx += x; n += 1; }
        cx = n > 0 ? (float)(sx / n) : -1;
        return n > 0 ? (float)(sy / n) : -1.f;
    };
    float cx1, cx2;
    const float y1m = centroid(a, d1, cx1), y2m = centroid(b, d2, cx2);
    std::printf("[weather] W11 slide: radius %.1f px, documented speed %.1f px/s, twin %.1f px/s, drawn %.1f px/s (centroid y %.1f -> %.1f, twin %.1f -> %.1f)\n",
                d1.radius, d1.speed, twinSpeed, (y2m - y1m) / (float)span, y1m, y2m, d1.pos.y, d2.pos.y);
    CHECK(std::fabs(twinSpeed / d1.speed - 1) < 0.05f);
    CHECK(y1m > 0 && y2m > 0);
    CHECK(std::fabs(y1m - d1.pos.y) < 2.5f && std::fabs(y2m - d2.pos.y) < 2.5f);              // drawn where the twin says
    CHECK(std::fabs((y2m - y1m) / (float)span / d1.speed - 1) < 0.05f);                         // and moving at the documented speed
    CHECK(std::fabs(cx1 - d1.pos.x) < 2.5f);
    // twice the slide: twice the speed
    fx::Lens fast = run; fast.rain.slide = 2;
    const auto F = fx::lensDrops(fast, f2 / 30.0, 640, 360);
    for (const auto& q : F)
        if (q.id == d1.id && q.sliding) {
            std::printf("[weather] W11 slide x2: speed %.1f (was %.1f)\n", q.speed, d1.speed);
            CHECK(std::fabs(q.speed / d1.speed - 2) < 0.01f);
        }
    UnloadImage(a); UnloadImage(b); UnloadImage(src);

    // every drop the twin lists is drawn, and pixels away from all drops are the source's (so nothing is drawn that the twin lacks)
    {
        const int f = frameOf(14.0);
        fx::Lens dense = run; dense.rain.rate = 1;
        Image img = lensImg(&dense, f), base = lensImg(nullptr, f);
        const auto L = fx::lensDrops(dense, f / 30.0, 640, 360);
        int drawn = 0, big = 0, strays = 0;
        for (const auto& d : L)
            if (d.radius >= 6 && d.pos.x > 12 && d.pos.x < 628 && d.pos.y > 12 && d.pos.y < 348) {
                big++;
                int changed = 0;
                for (int y = (int)d.pos.y - 3; y <= (int)d.pos.y + 3; y++)
                    for (int x = (int)d.pos.x - 3; x <= (int)d.pos.x + 3; x++)
                        if (differs(GetImageColor(img, x, y), GetImageColor(base, x, y), 2)) changed++;
                if (changed > 6) drawn++;
            }
        for (int y = 0; y < 360; y += 2)
            for (int x = 0; x < 640; x += 2) {
                bool near = false;
                for (const auto& d : L) if (std::hypot(x - d.pos.x, y - d.pos.y) < d.radius * 1.15f + 3.f) { near = true; break; }
                if (!near && differs(GetImageColor(img, x, y), GetImageColor(base, x, y), 0)) strays++;
            }
        std::printf("[weather] W11 twin vs pixels: %d of %d drops (r >= 6) drawn at their centre, %d stray pixels away from every drop\n", drawn, big, strays);
        CHECK(big > 30 && drawn >= big - 1);
        CHECK(strays == 0);
        UnloadImage(img); UnloadImage(base);
    }
}

// (a) A camera move does not move the drops: a 3D scene under the filter, panned.
static g3d::ScenePtr lensStreet(float yaw) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 1.5f, 6 };
    s->camera.target = { 6.f * std::sin(yaw), 1.5f, 6 - 6.f * std::cos(yaw) };
    s->ambient = 0.9f;
    g3d::Material ground;
    ground.color = { 90, 96, 110, 255 };
    s->add(g3d::Geometry::plane(80, 80), ground);
    for (int i = 0; i < 9; i++) {
        g3d::Material m;
        m.color = { (unsigned char)(80 + 20 * i), (unsigned char)(200 - 15 * i), 120, 255 };
        m.unlit = true;
        s->add(g3d::Geometry::box(1, 2 + (float)(i % 3), 1), m)->position = { -8.f + 2.f * (float)i, 1.5f, -3.f - (float)(i % 4) };
    }
    return s;
}
static void lensCameraMove() {
    fx::Lens l;
    l.rain.rate = 1; l.rain.slide = 0; l.rain.seed = 7;
    std::vector<std::vector<char>> masks, valid;            // valid: the picture is bright enough there for a drop to show
    for (float yaw : { 0.f, 0.5f }) {
        Comp c;
        c.w = 480; c.h = 270; c.fps = 30; c.duration = 600; c.background = BLACK;
        TNode* n = addRoot(c.roots, 0, 600);
        n->fn = g3d::draw(lensStreet(yaw), {});
        n->isolate().filters.push_back(fx::lens(l));
        Comp plain;
        plain.w = 480; plain.h = 270; plain.fps = 30; plain.duration = 600; plain.background = BLACK;
        TNode* m = addRoot(plain.roots, 0, 600);
        m->fn = g3d::draw(lensStreet(yaw), {});
        m->isolate();
        Image a = render(c, 200), b = render(plain, 200);
        std::vector<char> mask((size_t)480 * 270, 0), ok((size_t)480 * 270, 0);
        int n1 = 0;
        for (int y = 0; y < 270; y++)
            for (int x = 0; x < 480; x++) {
                const Color q = GetImageColor(b, x, y);
                ok[(size_t)y * 480 + x] = q.r + q.g + q.b > 240;
                if (differs(GetImageColor(a, x, y), q, 2)) { mask[(size_t)y * 480 + x] = 1; n1++; }
            }
        masks.push_back(mask);
        valid.push_back(ok);
        UnloadImage(a); UnloadImage(b);
        CHECK(n1 > 1500);
    }
    // the drops are clear now, so a drop over smooth picture changes nothing: check against the twin instead of the other camera.
    // No changed pixel lies away from every drop the twin lists, and the drops that have bright picture behind them show up in both.
    const auto L = fx::lensDrops(l, 200 / 30.0, 480, 270);
    long long stray[2] = { 0, 0 }, total[2] = { 0, 0 };
    int tested = 0, seen[2] = { 0, 0 };
    for (int cam = 0; cam < 2; cam++)
        for (int y = 0; y < 270; y++)
            for (int x = 0; x < 480; x++) {
                if (!masks[cam][(size_t)y * 480 + x]) continue;
                total[cam]++;
                bool near = false;
                for (const auto& d : L) if (std::hypot(x - d.pos.x, y - d.pos.y) < d.radius + 2.5f) { near = true; break; }
                if (!near) stray[cam]++;
            }
    for (const auto& d : L) {
        if (d.radius < 6.f) continue;
        int okpx = 0; bool hit[2] = { false, false };
        for (int y = std::max(0, (int)(d.pos.y - d.radius - 2)); y < std::min(270, (int)(d.pos.y + d.radius + 3)); y++)
            for (int x = std::max(0, (int)(d.pos.x - d.radius - 2)); x < std::min(480, (int)(d.pos.x + d.radius + 3)); x++)
                if (std::hypot(x - d.pos.x, y - d.pos.y) < d.radius + 2.5f) {
                    okpx += valid[0][(size_t)y * 480 + x] && valid[1][(size_t)y * 480 + x];
                    for (int cam = 0; cam < 2; cam++) hit[cam] = hit[cam] || masks[cam][(size_t)y * 480 + x];
                }
        if (okpx < 40) continue;
        tested++; seen[0] += hit[0]; seen[1] += hit[1];
    }
    std::printf("[weather] W11 camera pan: %lld / %lld changed pixels away from every drop (two cameras); of %d drops over bright picture, %d and %d show in the two cameras\n",
                stray[0], total[0], tested, seen[0], seen[1]);
    CHECK(stray[0] == 0 && stray[1] == 0);
    CHECK(tested >= 5 && seen[0] >= 0.85 * tested && seen[1] >= 0.85 * tested);
}

// (b) The wiper clears what it sweeps, then the glass refills.
static void lensWiper() {
    fx::Lens l;
    l.rain.rate = 1; l.rain.slide = 0; l.rain.seed = 9;
    l.wiper.period = 6; l.wiper.pivot = { 0.5f, 1.2f }; l.wiper.length = 1.4f; l.wiper.from = -60; l.wiper.to = 60; l.wiper.drawBlade = false;
    fx::Lens bare = l; bare.wiper.period = 0;
    const float W = 640, H = 360, px = 0.5f * W, py = 1.2f * H, L = 1.4f * H;
    const double m = 2;                                    // the third sweep, so every drop has had time to build up before it
    auto inSector = [&](const fx::LensDrop& d, float margin) {
        const float dx = d.pos.x - px, dy = d.pos.y - py, rho = std::hypot(dx, dy), phi = std::atan2(dx, -dy) * RAD2DEG;
        return rho + d.radius < L - margin && phi - d.radius / rho * RAD2DEG > -60 + margin && phi + d.radius / rho * RAD2DEG < 60 - margin;
    };
    auto count = [&](const std::vector<fx::LensDrop>& v) { int n = 0; for (const auto& d : v) n += inSector(d, 0) ? 1 : 0; return n; };
    const double tAfter = m * 6 + 3.6 + 0.04;               // just after the blade is back at its rest
    const auto wiped = fx::lensDrops(l, tAfter, W, H), unwiped = fx::lensDrops(bare, tAfter, W, H);
    const int nBefore = count(unwiped), nAfter = count(wiped);
    int young = 0, outsideOld = 0, outsideKept = 0;
    auto touches = [&](const fx::LensDrop& d) {            // any part of the drop within the blade's reach
        const float rr = std::max(d.radius, 14.f), dx = d.pos.x - px, dy = d.pos.y - py, rho = std::hypot(dx, dy), phi = std::atan2(dx, -dy) * RAD2DEG, dl = std::asin(std::min(rr / rho, 1.f)) * RAD2DEG;
        return rho - rr <= L && phi + dl >= -60 && phi - dl <= 60;
    };
    for (const auto& d : wiped) if (inSector(d, 0)) young += d.born >= m * 6 - 1e-3 ? 1 : 0;
    for (const auto& d : unwiped) if (!touches(d)) { outsideOld++; for (const auto& e : wiped) if (e.id == d.id) { outsideKept++; break; } }
    std::printf("[weather] W11 wiper: sector holds %d drops without the wiper, %d just after the sweep (%d of them born since it began); outside it %d of %d drops remain\n",
                nBefore, nAfter, young, outsideKept, outsideOld);
    CHECK(nBefore > 40);
    CHECK(nAfter <= nBefore * 0.3f);
    CHECK(young == nAfter);                                // every survivor is younger than the sweep
    CHECK(outsideOld > 10 && outsideKept == outsideOld);   // drops the blade never touches keep their place
    // the pixels: inside the swept sector, away from the survivors, the image is the drop-free image to the last level
    Image img = lensImg(&l, frameOf(tAfter)), clean = lensImg(nullptr, 0), withAll = lensImg(&bare, frameOf(tAfter));
    int checked = 0, wrong = 0, wouldBeWrong = 0;
    for (int y = 0; y < 360; y++)
        for (int x = 0; x < 640; x++) {
            fx::LensDrop probe; probe.pos = { (float)x, (float)y };
            if (!inSector(probe, 6)) continue;
            bool near = false;
            for (const auto& d : wiped) if (std::hypot(x - d.pos.x, y - d.pos.y) < d.radius * 1.2f + 3) { near = true; break; }
            if (near) continue;
            checked++;
            if (differs(GetImageColor(img, x, y), GetImageColor(clean, x, y), 0)) wrong++;
            if (differs(GetImageColor(withAll, x, y), GetImageColor(clean, x, y), 0)) wouldBeWrong++;
        }
    std::printf("[weather] W11 wiper pixels: %d sector pixels checked, %d differ from the drop-free image (%d would without the wiper)\n", checked, wrong, wouldBeWrong);
    CHECK(checked > 30000 && wrong == 0 && wouldBeWrong > 2000);
    UnloadImage(img); UnloadImage(clean); UnloadImage(withAll);
    // refill: two seconds later the sector holds more drops, at about the spawn rate of the glass outside it
    const int n2 = count(fx::lensDrops(l, tAfter + 2.0, W, H));
    std::printf("[weather] W11 wiper refill: %d drops in the sector at the sweep's end, %d two seconds later (%d with no wiper)\n", nAfter, n2, count(fx::lensDrops(bare, tAfter + 2.0, W, H)));
    CHECK(n2 >= nAfter + 8);
    // the blade passes: it is drawn at the angle the twin gives
    fx::Lens drawn = l; drawn.wiper.drawBlade = true; drawn.rain.rate = 0;
    const double tMid = m * 6 + 1.8;                        // the top of the sweep
    const float ang = fx::wiperAngle(drawn.wiper, tMid);
    Image bl = lensImg(&drawn, frameOf(tMid), lensChecker);
    const float bx = px + std::sin(ang * DEG2RAD) * 0.6f * L, by = py - std::cos(ang * DEG2RAD) * 0.6f * L;
    std::printf("[weather] W11 blade: angle %.1f deg at its top; pixel at 60%% of the arm: %d (glass 92..128)\n", ang, GetImageColor(bl, (int)bx, (int)by).r);
    CHECK(std::fabs(ang - 60) < 1.0f);
    CHECK(GetImageColor(bl, (int)bx, (int)by).r < 40 && GetImageColor(bl, (int)bx - 30, (int)by).r >= 85);
    UnloadImage(bl);
}

// (d) A drop refracts: the picture inside it is flipped and minified.
static void lensRefraction() {
    fx::Lens l;
    l.rain.rate = 1; l.rain.size = 1.5f; l.rain.slide = 0; l.rain.seed = 3;
    fx::LensDrop best;
    int f = 0;
    for (double s = 3; s < 30 && best.radius < 17; s += 0.5) {
        for (const auto& d : fx::lensDrops(l, frameOf(s) / 30.0, 640, 360))
            if (d.kind == fx::LensDrop::Kind::Rain && d.radius > best.radius && d.pos.x > 80 && d.pos.x < 560 && d.pos.y > 80 && d.pos.y < 280) { best = d; f = frameOf(s); }
    }
    CHECK(best.radius >= 14);
    const auto all = fx::lensDrops(l, f / 30.0, 640, 360);
    const Vector2 bc = best.pos;      // a pattern centred on the drop: R = 128 + 1.5 dx, G = 128 + 1.5 dy, B = 0
    Image img = lensImg(&l, f, [bc](const Ctx&, int) {
        DrawRectangle(0, 0, 640, 360, BLACK);
        DrawRectangleGradientEx({ bc.x - 60, bc.y - 60, 120, 120 }, { 38, 38, 0, 255 }, { 38, 218, 0, 255 }, { 218, 218, 0, 255 }, { 218, 38, 0, 255 });
    });
    { Image crop = ImageFromImage(img, { bc.x - 30, bc.y - 30, 60, 60 }); ImageResizeNN(&crop, 360, 360); dump("w11-refraction-crop.png", crop); UnloadImage(crop); }
    // fit the colour inside the drop against the offset from its centre
    std::vector<double> rr, gg, dxs, dys;
    for (int y = (int)(best.pos.y - best.radius); y <= (int)(best.pos.y + best.radius); y++)
        for (int x = (int)(best.pos.x - best.radius); x <= (int)(best.pos.x + best.radius); x++) {
            const float dx = x - best.pos.x, dy = y - best.pos.y;
            if (std::hypot(dx, dy) > 0.5f * best.radius) continue;
            bool clear = true;
            for (const auto& o : all) if (o.id != best.id && std::hypot(x - o.pos.x, y - o.pos.y) < o.radius + 2) clear = false;
            if (!clear) continue;
            const Color c = GetImageColor(img, x, y);
            rr.push_back((double)c.r - c.b); gg.push_back((double)c.g - c.b); dxs.push_back(dx); dys.push_back(dy);
        }
    auto corr = [](const std::vector<double>& a, const std::vector<double>& b) {
        double ma = 0, mb = 0; for (size_t i = 0; i < a.size(); i++) { ma += a[i]; mb += b[i]; } ma /= a.size(); mb /= a.size();
        double sab = 0, saa = 0, sbb = 0;
        for (size_t i = 0; i < a.size(); i++) { sab += (a[i] - ma) * (b[i] - mb); saa += (a[i] - ma) * (a[i] - ma); sbb += (b[i] - mb) * (b[i] - mb); }
        return sab / std::sqrt(std::max(saa * sbb, 1e-9));
    };
    auto slope = [](const std::vector<double>& a, const std::vector<double>& b) {      // d a / d b
        double ma = 0, mb = 0; for (size_t i = 0; i < a.size(); i++) { ma += a[i]; mb += b[i]; } ma /= a.size(); mb /= a.size();
        double sab = 0, sbb = 0; for (size_t i = 0; i < a.size(); i++) { sab += (a[i] - ma) * (b[i] - mb); sbb += (b[i] - mb) * (b[i] - mb); }
        return sab / std::max(sbb, 1e-9);
    };
    const double cx = rr.empty() ? 0 : corr(rr, dxs), cy = gg.empty() ? 0 : corr(gg, dys);
    // the source is a ramp of 1.5 levels per px: a minified image has a slope of 1.5 * k per px with k > 1 for the
    // neighbourhood being squeezed into the drop, and the sign of a flip is negative
    const double kx = rr.empty() ? 0 : -slope(rr, dxs) / 1.5, ky = gg.empty() ? 0 : -slope(gg, dys) / 1.5;
    std::printf("[weather] W11 refraction: drop r=%.1f px, %zu px inside; corr(R, dx) = %.3f, corr(G, dy) = %.3f; minification %.2f (x), %.2f (y)\n",
                best.radius, rr.size(), cx, cy, kx, ky);
    CHECK(rr.size() > 80);
    CHECK(cx < -0.9 && cy < -0.9);
    CHECK(kx > 2.5 && kx < 7 && ky > 2.5 && ky < 7);
    UnloadImage(img);
}

// (e) Frost creeps in from the edges, and grows with the amount and with time.
static void lensFrostEdges() {
    auto meanDiff = [](const Image& a, const Image& b, int x0, int y0, int x1, int y1) {
        double s = 0; int n = 0;
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++) {
                const Color p = GetImageColor(a, x, y), q = GetImageColor(b, x, y);
                s += std::abs(p.r - q.r) + std::abs(p.g - q.g) + std::abs(p.b - q.b); n++;
            }
        return s / (3.0 * std::max(n, 1));
    };
    Image base = lensImg(nullptr, 0, lensChecker);
    double prevAll = -1;
    for (float amount : { 0.2f, 0.4f, 0.6f, 0.9f }) {
        fx::Lens l;
        l.frost.amount = amount; l.frost.seed = 2;
        Image img = lensImg(&l, 0, lensChecker);
        const double band = (meanDiff(img, base, 0, 0, 640, 40) + meanDiff(img, base, 0, 320, 640, 360) + meanDiff(img, base, 0, 40, 40, 320) + meanDiff(img, base, 600, 40, 640, 320)) / 4;
        const double centre = meanDiff(img, base, 220, 110, 420, 250), all = meanDiff(img, base, 0, 0, 640, 360);
        std::printf("[weather] W11 frost amount %.1f: mean change %.1f in the edge band, %.1f in the centre, %.1f overall\n", amount, band, centre, all);
        if (amount <= 0.4f) CHECK(band > 2.0 * centre + 3);
        if (amount <= 0.6f) CHECK(band > centre);
        CHECK(all > prevAll * 1.02 + 0.2);
        prevAll = all;
        UnloadImage(img);
    }
    // in time: the creep between two frames
    fx::Lens t;
    t.frost.amount = 0.9f; t.frost.startFrame = 0; t.frost.endFrame = 120; t.frost.seed = 2;
    double prev = -1;
    for (int f : { 10, 40, 70, 100, 130 }) {
        Image img = lensImg(&t, f, lensChecker);
        const double all = meanDiff(img, base, 0, 0, 640, 360);
        std::printf("[weather] W11 frost creep, frame %d: mean change %.1f\n", f, all);
        if (f < 120) CHECK(all > prev + 0.2); else CHECK(all >= prev - 0.5);
        prev = all;
        UnloadImage(img);
    }
    // a cleared patch stays clear
    fx::Lens c;
    c.frost.amount = 1; c.frost.seed = 2; c.frost.clears = { { 320, 180, 70 } };
    Image img = lensImg(&c, 0, lensChecker);
    long long d = 0, cnt = 0;
    for (int y = 130; y < 230; y++)
        for (int x = 270; x < 370; x++)
            if (std::hypot(x - 320.f, y - 180.f) < 40) { d += differs(GetImageColor(img, x, y), GetImageColor(base, x, y), 0); cnt++; }
    std::printf("[weather] W11 frost clear patch: %lld of %lld pixels inside its core differ from the glass-free image\n", d, cnt);
    CHECK(d == 0);
    UnloadImage(img); UnloadImage(base);
}

// Snowflakes: one lands, sits as a bright blob, melts into a drop.
static void lensSnowflakes() {
    fx::Lens l;
    l.snow.rate = 1; l.snow.seed = 5;
    fx::LensFlake flake;
    double tL = -1;
    for (double s = 3; s < 40 && tL < 0; s += 0.2) {
        for (const auto& f : fx::lensFlakes(l, s, 640, 360))
            if (f.alpha > 0.99f && f.melt == 0 && f.pos.x > 80 && f.pos.x < 560 && f.pos.y > 60 && f.pos.y < 300) { flake = f; tL = s; break; }
    }
    CHECK(tL > 0);
    if (tL < 0) return;
    Image sitting = lensImg(&l, frameOf(tL), lensDark), base = lensImg(nullptr, 0, lensDark);
    const Color at = GetImageColor(sitting, (int)flake.pos.x, (int)flake.pos.y);
    std::printf("[weather] W11 snow: flake r=%.1f px sits at (%.0f, %.0f), centre pixel %d (glass %d)\n", flake.radius, flake.pos.x, flake.pos.y, at.r, GetImageColor(base, 5, 5).r);
    CHECK(at.r > 90);
    // later it is a drop: the twin lists a Melt drop at the same place and the blob is gone
    double tM = tL;
    bool melted = false;
    for (double s = tL + 0.1; s < tL + 12 && !melted; s += 0.1)
        for (const auto& d : fx::lensDrops(l, s, 640, 360))
            if (d.kind == fx::LensDrop::Kind::Melt && d.radius > 1.5f && std::hypot(d.pos.x - flake.pos.x, d.pos.y - flake.pos.y) < 1.f) { melted = true; tM = s; }
    Image later = lensImg(&l, frameOf(tM), lensDark);
    std::printf("[weather] W11 snow: after melting %s; centre pixel %d\n", melted ? "a drop is listed" : "no drop", GetImageColor(later, (int)flake.pos.x, (int)flake.pos.y).r);
    CHECK(melted);
    CHECK(GetImageColor(later, (int)flake.pos.x, (int)flake.pos.y).r < at.r - 35);
    UnloadImage(sitting); UnloadImage(base); UnloadImage(later);
}

// (f) Purity: a frame alone equals the frame after others.
static void lensPurity() {
    fx::Lens l;
    l.rain.rate = 0.8f; l.rain.blurBehind = 6; l.snow.rate = 0.6f; l.frost.amount = 0.5f; l.frost.startFrame = 0; l.frost.endFrame = 200;
    l.wiper.period = 3; l.wiper.drawBlade = true;
    Image a = lensImg(&l, 150, lensChecker);
    for (int f : { 20, 400, 99, 150, 7 }) UnloadImage(lensImg(&l, f, lensChecker));
    Image b = lensImg(&l, 150, lensChecker);
    const long long d = diffImages(a, b);
    // the same inside one composition, frames out of order
    Comp c = lensComp(&l, 640, 360, lensChecker);
    Image x = render(c, 150);
    for (int f : { 60, 300, 12 }) UnloadImage(render(c, f));
    Image y = render(c, 150);
    const long long d2 = diffImages(x, y);
    const auto d1 = fx::lensDrops(l, 5.0, 640, 360), d3 = fx::lensDrops(l, 5.0, 640, 360);
    bool same = d1.size() == d3.size();
    for (size_t i = 0; same && i < d1.size(); i++) same = d1[i].id == d3[i].id && d1[i].pos.x == d3[i].pos.x && d1[i].pos.y == d3[i].pos.y;
    std::printf("[weather] W11 purity: diff %lld (new canvas), %lld (same comp, other frames between); twin repeatable: %s\n", d, d2, same ? "yes" : "no");
    CHECK(d == 0 && d2 == 0 && same);
    // and a different seed is a different glass
    fx::Lens other = l; other.rain.seed = 2;
    Image z = lensImg(&other, 150, lensChecker);
    CHECK(diffImages(a, z) > 20000);
    UnloadImage(a); UnloadImage(b); UnloadImage(x); UnloadImage(y); UnloadImage(z);
}

// Rain that follows the weather: the drop count follows the intensity of the rain over time.
static void lensFollowsWeather() {
    g3d::Weather w = g3d::Weather::rain();
    w.history = { { 0, g3d::Precipitation::Rain, 0.1f }, { 20, g3d::Precipitation::Rain, 1.0f } };
    fx::Lens l;
    l.rain.rateAt = fx::rainOf(w);
    l.rain.slide = 0;
    size_t prev = 0;
    for (double s : { 14.0, 24.0, 34.0 }) {
        const size_t n = fx::lensDrops(l, s, 640, 360).size();
        std::printf("[weather] W11 weather rain: %zu drops at %.0f s (rain %.2f at spawn time)\n", n, s, w.rainAt(s - 3));
        CHECK(n > prev);
        prev = n;
    }
}

// The cost at 1080p: each filter on a flat layer, against the same layer with no filter.
static void lensTiming() {
    constexpr int W = 1920, H = 1080;
    struct Row { const char* name; fx::Lens l; };
    std::vector<Row> rows;
    rows.push_back({ "no filter", {} });
    { fx::Lens l; rows.push_back({ "isolated layer, lens at zero", l }); }
    { fx::Lens l; l.rain.rate = 0.8f; rows.push_back({ "rain, rate 0.8", l }); }
    { fx::Lens l; l.rain.rate = 0.8f; l.rain.blurBehind = 12; rows.push_back({ "rain + blurBehind 12", l }); }
    { fx::Lens l; l.frost.amount = 0.8f; rows.push_back({ "frost 0.8", l }); }
    { fx::Lens l; l.frost.amount = 0.8f; l.frost.kind = fx::LensFrost::Kind::Condensation; l.rain.rate = 0.5f; rows.push_back({ "condensation + rain", l }); }
    { fx::Lens l; l.snow.rate = 1; rows.push_back({ "snow, rate 1", l }); }
    { fx::Lens l; l.wiper.period = 3; rows.push_back({ "wiper blade", l }); }
    { fx::Lens l; l.rain.rate = 0.8f; l.rain.blurBehind = 12; l.snow.rate = 0.5f; l.frost.amount = 0.4f; l.wiper.period = 3; rows.push_back({ "all four", l }); }
    for (size_t r = 0; r < rows.size(); r++) {
        Comp c = lensComp(r == 0 ? nullptr : &rows[r].l, W, H, [](const Ctx&, int) {
            DrawRectangleGradientEx({ 0, 0, 1920, 1080 }, BLACK, GREEN, YELLOW, RED);
        });
        Canvas canvas;
        canvas.load(W, H, 1);
        presentFrame(c, 150, canvas);
        UnloadImage(LoadImageFromTexture(canvas.output().texture));
        double best = 1e9, mean = 0;
        for (int i = 0; i < 12; i++) {
            const double t0 = GetTime();
            presentFrame(c, 150 + 7 * i, canvas);
            Image img = LoadImageFromTexture(canvas.output().texture);
            const double dt = (GetTime() - t0) * 1000;
            UnloadImage(img);
            best = std::min(best, dt);
            mean += dt / 12;
        }
        canvas.unload();
        std::printf("[weather] W11 timing 1080p, %s: best %.1f ms, mean %.1f ms\n", rows[r].name, best, mean);
    }
}

constexpr double FROST_STEP_BOUND = 1.0, FROST_ORIENT_BOUND = 0.3;
// A drop is almost clear and irregular: over a flat grey its inside is the grey (the sharp picture refracted), only a thin rim
// is darker, and its outline is not a circle.
static void lensFlat(const Ctx&, int) { DrawRectangle(0, 0, 640, 360, { 128, 128, 128, 255 }); }
static void lensDropLook() {
    fx::Lens l;
    l.rain.rate = 1; l.rain.size = 1.5f; l.rain.slide = 1; l.rain.seed = 3;
    int n = 0;
    double sumCirc = 0, worstLum = 0, sumLum = 0, minCirc = 9;
    for (double s = 6; s < 40; s += 2.5) {
        const int f = frameOf(s);
        const auto L = fx::lensDrops(l, f / 30.0, 640, 360);
        Image img = lensImg(&l, f, lensFlat);
        for (const auto& d : L) {
            if (d.kind != fx::LensDrop::Kind::Rain || d.radius < 10 || d.pos.x < 40 || d.pos.x > 600 || d.pos.y < 40 || d.pos.y > 320) continue;
            bool alone = true;
            for (const auto& o : L)
                if (o.id != d.id && std::hypot(o.pos.x - d.pos.x, o.pos.y - d.pos.y) < 1.5f * d.radius + o.radius + 5) alone = false;
            if (!alone) continue;
            // the inside: mean luminance within 0.4 r of the centre
            double sum = 0; int cnt = 0;
            for (int y = (int)(d.pos.y - d.radius); y <= (int)(d.pos.y + d.radius); y++)
                for (int x = (int)(d.pos.x - d.radius); x <= (int)(d.pos.x + d.radius); x++)
                    if (std::hypot(x - d.pos.x, y - d.pos.y) < 0.4f * d.radius) { sum += luma(GetImageColor(img, x, y)); cnt++; }
            const double lum = sum / std::max(cnt, 1), rel = std::fabs(lum / 128.0 - 1);
            // the outline: the outermost changed pixel along 16 rays, as a polygon
            std::vector<Vector2> pts;
            for (int k = 0; k < 16; k++) {
                const float a = 6.2831853f * (float)k / 16.f;
                float rr = 0;
                for (float t = 1.5f * d.radius + 4; t > 0; t -= 0.25f) {
                    const int x = (int)std::lround(d.pos.x + std::cos(a) * t), y = (int)std::lround(d.pos.y + std::sin(a) * t);
                    if (differs(GetImageColor(img, x, y), { 128, 128, 128, 255 }, 2)) { rr = t; break; }
                }
                pts.push_back({ std::cos(a) * rr, std::sin(a) * rr });
            }
            double P = 0, A = 0;
            for (size_t k = 0; k < pts.size(); k++) {
                const Vector2 p = pts[k], q = pts[(k + 1) % pts.size()];
                P += std::hypot(q.x - p.x, q.y - p.y);
                A += 0.5 * (p.x * q.y - q.x * p.y);
            }
            const double circ = P * P / (4 * 3.14159265358979 * std::fabs(A));
            n++; sumCirc += circ; minCirc = std::min(minCirc, circ); worstLum = std::max(worstLum, rel); sumLum += lum;
        }
        UnloadImage(img);
    }
    std::printf("[weather] W11 drop look over flat grey 128: %d drops of r >= 10; inside mean luminance %.1f (worst %.1f%% off); circularity P^2/4piA mean %.3f (min %.3f)\n",
                n, n ? sumLum / n : 0.0, worstLum * 100, n ? sumCirc / n : 0.0, minCirc);
    CHECK(n >= 8);
    CHECK(worstLum <= 0.10);
    CHECK(n && sumCirc / n > 1.08);
}

// Frost: no hard steps (anti-aliased), and its fronds run inward from the edges.
static void lensFrostLook() {
    fx::Lens l;
    l.frost.amount = 0.9f; l.frost.seed = 2;
    Image img = lensImg(&l, 0, lensFlat);
    std::vector<float> Y((size_t)640 * 360);
    for (int y = 0; y < 360; y++) for (int x = 0; x < 640; x++) Y[(size_t)y * 640 + x] = luma(GetImageColor(img, x, y));
    long long steps = 0, pairs = 0, soft = 0;
    for (int y = 1; y < 359; y++)
        for (int x = 1; x < 639; x++) {
            const float c = Y[(size_t)y * 640 + x];
            const float dx = std::fabs(Y[(size_t)y * 640 + x + 1] - c), dy = std::fabs(Y[(size_t)(y + 1) * 640 + x] - c);
            pairs += 2; steps += (dx >= 60) + (dy >= 60); soft += (dx >= 20) + (dy >= 20);
        }
    const double stepFrac = (double)steps / (double)pairs;
    std::printf("[weather] W11 frost steps: %.4f%% of 4-neighbour pairs jump >= 60 levels (%.3f%% jump >= 20)\n", stepFrac * 100, 100.0 * (double)soft / (double)pairs);
    CHECK(stepFrac < FROST_STEP_BOUND);
    CHECK((double)soft / (double)pairs < 0.18);            // a hard-edged crystal pattern jumps >= 20 levels on 26% of pairs, the anti-aliased one on about 12%
    // orientation: the structure tensor of the luminance in each edge band; fronds are lines, so their gradients are across them
    struct Band { const char* name; int x0, y0, x1, y1; int inward; };    // inward: 0 down, 1 up, 2 right, 3 left
    const Band bands[] = { { "top", 60, 8, 580, 64, 0 }, { "bottom", 60, 296, 580, 352, 1 }, { "left", 8, 60, 64, 300, 2 }, { "right", 576, 60, 632, 300, 3 } };
    double sum = 0;
    for (const Band& b : bands) {
        // the dominant orientation of the stems: the gradient tensor of the picture blurred over 5 px (the finest barbs wash out, the stems
        // and long branches stay). m > 0 where the structure runs vertically; towards > 0 where it runs along the inward direction.
        double jxx = 0, jyy = 0;
        for (int y = b.y0; y < b.y1; y++)
            for (int x = b.x0; x < b.x1; x++) {
                auto box = [&](int cx, int cy) { double s = 0; for (int j = -2; j <= 2; j++) for (int i = -2; i <= 2; i++) s += Y[(size_t)(cy + j) * 640 + cx + i]; return s / 25.0; };
                const double gx = box(x + 2, y) - box(x - 2, y), gy = box(x, y + 2) - box(x, y - 2);
                jxx += gx * gx; jyy += gy * gy;
            }
        const double m = (jxx - jyy) / std::max(jxx + jyy, 1e-9);
        const double towards = (b.inward < 2) ? m : -m;
        std::printf("[weather] W11 frost orientation, %s band: line direction vs inward %.3f\n", b.name, towards);
        sum += towards;
        CHECK(towards > FROST_ORIENT_BOUND);
    }
    std::printf("[weather] W11 frost orientation: mean %.3f\n", sum / 4);
    // corners: no seam along the diagonal. The mean |difference| between a pixel and its mirror across the diagonal (1-3 px each
    // side) must be no larger than across a line parallel to it, 8 px away.
    {
        double dDiag = 0, dOff = 0, lDiag = 0, lOff = 0; long long cnt = 0;
        const int cxs[4] = { 0, 639, 0, 639 }, cys[4] = { 0, 0, 359, 359 };
        for (int c = 0; c < 4; c++) {
            const int sx = cxs[c] == 0 ? 1 : -1, sy = cys[c] == 0 ? 1 : -1;
            for (int t = 30; t <= 130; t++)
                for (int k = 1; k <= 3; k++)
                    for (int pass = 0; pass < 2; pass++) {
                        const int bx = cxs[c] + sx * t + (pass ? 8 * sx : 0), by = cys[c] + sy * t;
                        const float a = Y[(size_t)(by - sy * k) * 640 + bx + sx * k], b = Y[(size_t)(by + sy * k) * 640 + bx - sx * k];
                        (pass ? dOff : dDiag) += std::fabs(a - b);
                        (pass ? lOff : lDiag) += 0.5 * (a + b);
                    }
            cnt += 101 * 3;
        }
        std::printf("[weather] W11 frost corners: mean |difference| across the diagonal %.2f, across a parallel line 8 px away %.2f\n", dDiag / cnt, dOff / cnt);
        std::printf("[weather] W11 frost corners: mean luminance in the diagonal band %.1f, in the parallel band %.1f\n", lDiag / cnt, lOff / cnt);
        CHECK(dDiag <= dOff * 1.0);
        CHECK(dDiag >= 0.88 * dOff);                          // and the structure does not die out along it
        CHECK(lDiag >= 0.97 * lOff);                          // crystals do not stop along the diagonal
    }
    UnloadImage(img);
}

// Tiny drops are a faint lens, not a dark mark: over flat grey nothing inside a drop under 5 px falls below 0.8 of the grey.
static void lensTinyDrops() {
    fx::Lens l;
    l.rain.rate = 1; l.rain.size = 0.5f; l.rain.slide = 0; l.rain.seed = 7;
    int n = 0; double worst = 1e9;
    for (double s = 6; s < 30; s += 2) {
        const int f = frameOf(s);
        const auto L = fx::lensDrops(l, f / 30.0, 640, 360);
        Image img = lensImg(&l, f, lensFlat);
        for (const auto& d : L) {
            if (d.radius < 2.f || d.radius >= 5.f || d.pos.x < 20 || d.pos.x > 620 || d.pos.y < 20 || d.pos.y > 340) continue;
            bool alone = true;
            for (const auto& o : L) if (o.id != d.id && std::hypot(o.pos.x - d.pos.x, o.pos.y - d.pos.y) < d.radius + o.radius + 4) alone = false;
            if (!alone) continue;
            double mn = 1e9;
            for (int y = (int)std::floor(d.pos.y - d.radius); y <= (int)std::ceil(d.pos.y + d.radius); y++)
                for (int x = (int)std::floor(d.pos.x - d.radius); x <= (int)std::ceil(d.pos.x + d.radius); x++)
                    if (std::hypot(x - d.pos.x, y - d.pos.y) <= d.radius) mn = std::min<double>(mn, luma(GetImageColor(img, x, y)));
            n++; worst = std::min(worst, mn);
        }
        UnloadImage(img);
    }
    std::printf("[weather] W11 tiny drops: %d drops of 2-5 px over grey 128; darkest pixel inside any of them %.1f (%.0f%% of the grey)\n", n, worst, 100 * worst / 128.0);
    CHECK(n >= 8);
    CHECK(worst >= 0.8 * 128.0);
}

// Snowflakes on a lens are big, out of focus and nearly featureless: at 0.6 r the six-fold harmonic is under 15% of the disc's brightness.
static void lensFlakeLook() {
    fx::Lens l;
    l.snow.rate = 1; l.snow.seed = 9;
    int n = 0; double worst = 0, sumR = 0, minR = 1e9, maxR = 0;
    for (double s = 3; s < 60; s += 0.7) {
        const int f = frameOf(s);
        const auto L = fx::lensFlakes(l, f / 30.0, 640, 360);
        Image img = lensImg(&l, f, lensFlat);
        for (const auto& fl : L) {
            if (fl.alpha < 0.99f || fl.melt > 0 || fl.pos.x < 70 || fl.pos.x > 570 || fl.pos.y < 70 || fl.pos.y > 290) continue;
            bool alone = true;
            for (const auto& o : L) if (&o != &fl && std::hypot(o.pos.x - fl.pos.x, o.pos.y - fl.pos.y) < 2.5f * (fl.radius + o.radius)) alone = false;
            if (!alone) continue;
            double re = 0, im = 0, mean = 0; const int N = 72;
            for (int k = 0; k < N; k++) {
                const double a = 6.2831853 * k / N;
                const double v = luma(GetImageColor(img, (int)std::lround(fl.pos.x + 0.6 * fl.radius * std::cos(a)), (int)std::lround(fl.pos.y + 0.6 * fl.radius * std::sin(a))));
                re += (v - 128) * std::cos(6 * a); im += (v - 128) * std::sin(6 * a); mean += v - 128;
            }
            const double amp = 2 * std::hypot(re, im) / N, disc = mean / N;       // the harmonic against the disc's lift over the glass
            if (disc < 10) continue;
            n++; worst = std::max(worst, amp / disc); sumR += fl.radius; minR = std::min<double>(minR, fl.radius); maxR = std::max<double>(maxR, fl.radius);
        }
        UnloadImage(img);
    }
    std::printf("[weather] W11 flakes: %d sitting flakes of radius %.0f-%.0f px (mean %.0f) on a 360 px layer; six-fold contrast at 0.6 r at most %.3f of the disc\n", n, minR, maxR, n ? sumR / n : 0.0, worst);
    CHECK(n >= 4);
    CHECK(worst <= 0.10);
}

static void cameraWeather() {
    lensOffMeansOff();
    lensDropsStayAndSlide();
    lensCameraMove();
    lensWiper();
    lensRefraction();
    lensDropLook();
    lensTinyDrops();
    lensFlakeLook();
    lensFrostEdges();
    lensFrostLook();
    lensSnowflakes();
    lensPurity();
    lensFollowsWeather();
    lensTiming();
}

// ---------------------------------------------------------------- the base of a cumulus (W2, base look)
// Real cumulus bases are level but ragged (small lumps, pendant bulges) and unevenly lit (dark under the thick middle, light at the thin
// edge). A flat cut with one grey shade failed both. (a) From the side at the height of the base, the lowest cloud pixel of each column is
// a line only if the base is a plane: the rms of its residual from a straight line, over the middle 70% of a cloud's width. (b) From below,
// the luminance over the base, after a planar trend is removed, varies (stddev/mean), and the centre of a thick cloud is darker than its rim.
static void fitLine(const std::vector<float>& ys, const std::vector<int>& xs, float* a, float* b) {
    double sx = 0, sy = 0, sxx = 0, sxy = 0; const double n = (double)xs.size();
    for (size_t i = 0; i < xs.size(); i++) { sx += xs[i]; sy += ys[i]; sxx += (double)xs[i] * xs[i]; sxy += (double)xs[i] * ys[i]; }
    const double den = n * sxx - sx * sx;
    *a = den != 0 ? (float)((n * sxy - sx * sy) / den) : 0.f;
    *b = (float)((sy - *a * sx) / n);
}
static float baseRagged(const char* tag, int* clouds = nullptr) {
    constexpr int W = 960, H = 540;
    std::vector<float> r;
    for (int cam = 0; cam < 8; cam++) {
        const float x = (cam - 3.5f) * 2500.f;
        auto mk = [&](bool on) {
            auto s = filmClouds({ x, 1500, 0 }, { x, 1500, -400 }, 0.3f, g3d::CloudType::Cumulus, on, 1500, 3500, 40, false);
            return look(s, W, H, 0);
        };
        Image on = mk(true), off = mk(false);
        if (cam == 3) dump((std::string("cloud-base-side-") + tag + ".png").c_str(), on);
        const std::vector<Blob> bl = blobsOf(cloudMask(on, off), W, H);
        for (const Blob& b : bl) {
            if (b.x1 - b.x0 < 150 || b.area < 4000) continue;
            std::vector<int> low((size_t)W, -1);
            for (int j : b.px) low[(size_t)(j % W)] = std::max(low[(size_t)(j % W)], j / W);
            const int w = b.x1 - b.x0 + 1, x0 = b.x0 + (int)(0.15f * w), x1 = b.x1 - (int)(0.15f * w);
            std::vector<int> xs; std::vector<float> ys;
            for (int c = x0; c <= x1; c++) if (low[(size_t)c] >= 0) { xs.push_back(c); ys.push_back((float)low[(size_t)c]); }
            if (xs.size() < 100) continue;
            float a, bb; fitLine(ys, xs, &a, &bb);
            double ss = 0;
            for (size_t i = 0; i < xs.size(); i++) { const double e = ys[i] - (a * xs[i] + bb); ss += e * e; }
            r.push_back((float)std::sqrt(ss / (double)xs.size()));
        }
        UnloadImage(on); UnloadImage(off);
    }
    if (clouds) *clouds = (int)r.size();
    if (r.empty()) return 0.f;
    std::sort(r.begin(), r.end());
    return r[r.size() / 2];
}
struct BaseShade { float cv = 0, mottle = 0, centreOverRim = 99; int clouds = 0; };
static BaseShade baseShade(const char* tag) {
    constexpr int W = 640, H = 360;
    std::vector<float> cvs, ratios, mott;
    for (int cam = 0; cam < 10; cam++) {
        const float x = (cam - 4.5f) * 2200.f;
        auto mk = [&](bool on) { return look(filmClouds({ x, 2, 0 }, { x, 2000, -100 }, 0.55f, g3d::CloudType::Cumulus, on, 1500, 3500, 100, false), W, H, 0); };
        Image on = mk(true), off = mk(false);
        if (cam == 4) dump((std::string("cloud-base-below-") + tag + ".png").c_str(), on);
        const std::vector<char> mask = cloudMask(on, off, 60);
        const std::vector<float> L = lumaOf(on);
        for (const Blob& b : blobsOf(mask, W, H)) {
            if (b.area < 6000) continue;
            // distance to the rim by repeated erosion
            std::vector<int> dist((size_t)W * H, 0);
            std::vector<char> cur = mask;
            for (int it = 1; it <= 60; it++) {
                std::vector<char> nxt = cur; bool any = false;
                for (int j : b.px) {
                    if (!cur[(size_t)j]) continue;
                    const int jx = j % W, jy = j / W;
                    const bool edge = (jx > 0 && !cur[(size_t)j - 1]) || (jx < W - 1 && !cur[(size_t)j + 1]) || (jy > 0 && !cur[(size_t)j - W]) || (jy < H - 1 && !cur[(size_t)j + W]);   // the frame's edge is not a rim
                    if (edge) nxt[(size_t)j] = 0; else { dist[(size_t)j] = it; any = true; }
                }
                cur = nxt;
                if (!any) break;
            }
            int dmax = 0;
            for (int j : b.px) dmax = std::max(dmax, dist[(size_t)j]);
            if (dmax < 16) continue;
            // planar trend by least squares on (1, x, y)
            double A[3][3] = {}, rhs[3] = {};
            for (int j : b.px) { const double v[3] = { 1.0, (double)(j % W) / W, (double)(j / W) / H }; for (int p = 0; p < 3; p++) { rhs[p] += v[p] * L[(size_t)j]; for (int q = 0; q < 3; q++) A[p][q] += v[p] * v[q]; } }
            for (int p = 0; p < 3; p++) for (int q = p + 1; q < 3; q++) { const double f = A[q][p] / A[p][p]; for (int k = p; k < 3; k++) A[q][k] -= f * A[p][k]; rhs[q] -= f * rhs[p]; }
            double co[3];
            for (int p = 2; p >= 0; p--) { double s = rhs[p]; for (int k = p + 1; k < 3; k++) s -= A[p][k] * co[k]; co[p] = s / A[p][p]; }
            double mean = 0, ss = 0;
            for (int j : b.px) mean += L[(size_t)j];
            mean /= b.area;
            for (int j : b.px) { const double e = L[(size_t)j] - (co[0] + co[1] * (j % W) / W + co[2] * (j / W) / H); ss += e * e; }
            cvs.push_back((float)(std::sqrt(ss / b.area) / std::max(mean, 1.0)));
            double ci = 0, ri = 0; int nc = 0, nr = 0;
            for (int j : b.px) {
                if (dist[(size_t)j] >= (int)(0.6f * dmax)) { ci += L[(size_t)j]; nc++; }
                else if (dist[(size_t)j] >= 2 && dist[(size_t)j] <= 8) { ri += L[(size_t)j]; nr++; }
            }
            if (nc > 50 && nr > 50) ratios.push_back((float)((ci / nc) / (ri / nr)));
            // mottling: what is left of the luminance once a 19 px box average is taken out, inside the rim (the rim's own ramp is not texture)
            double ms = 0, mm = 0; int nm = 0;
            for (int j : b.px) {
                if (dist[(size_t)j] < 10) continue;
                const int jx = j % W, jy = j / W;
                double s = 0; int n = 0;
                for (int dy = -9; dy <= 9; dy++) for (int dx = -9; dx <= 9; dx++) {
                    const int qx = jx + dx, qy = jy + dy;
                    if (qx < 0 || qy < 0 || qx >= W || qy >= H || !mask[(size_t)qy * W + qx]) continue;
                    s += L[(size_t)qy * W + qx]; n++;
                }
                const double e = L[(size_t)j] - s / n;
                ms += e * e; mm += L[(size_t)j]; nm++;
            }
            if (nm > 200) mott.push_back((float)(std::sqrt(ms / nm) / std::max(mm / nm, 1.0)));
        }
        UnloadImage(on); UnloadImage(off);
    }
    BaseShade out;
    out.clouds = (int)cvs.size();
    std::sort(cvs.begin(), cvs.end()); std::sort(ratios.begin(), ratios.end()); std::sort(mott.begin(), mott.end());
    if (!mott.empty()) out.mottle = mott[mott.size() / 2];
    if (!cvs.empty()) out.cv = cvs[cvs.size() / 2];
    if (!ratios.empty()) out.centreOverRim = ratios[ratios.size() / 2];
    return out;
}
// The underside as the camera on the ground sees it (about 10 degrees up, clouds a few km away): the lowest third of each cloud's
// silhouette. stddev/mean of its luminance after a planar trend, and the same after a 9 px box average (the fine mottle).
struct BandShade { float cv = 0, mottle = 0; int clouds = 0; };
static BandShade bandShade(const char* tag) {
    constexpr int W = 960, H = 540;
    std::vector<float> cvs, mott;
    for (int cam = 0; cam < 8; cam++) {
        const float x = (cam - 3.5f) * 2500.f;
        auto mk = [&](bool on) { return look(filmClouds({ x, 2, 0 }, { x, 2 + 450, -2500 }, 0.4f, g3d::CloudType::Cumulus, on, 1500, 3500, 60, false), W, H, 0); };
        Image on = mk(true), off = mk(false);
        if (cam == 3) dump((std::string("cloud-base-band-") + tag + ".png").c_str(), on);
        const std::vector<char> mask = cloudMask(on, off, 60);
        const std::vector<float> L = lumaOf(on);
        for (const Blob& b : blobsOf(mask, W, H)) {
            if (b.area < 8000 || b.y1 - b.y0 < 60) continue;
            const int yCut = b.y1 - (int)(0.35f * (b.y1 - b.y0));
            std::vector<int> px;
            for (int j : b.px) if (j / W >= yCut && j / W < H - 1) px.push_back(j);
            if (px.size() < 3000) continue;
            double A[3][3] = {}, rhs[3] = {}, mean = 0;
            for (int j : px) { const double v[3] = { 1.0, (double)(j % W) / W, (double)(j / W) / H }; mean += L[(size_t)j]; for (int p = 0; p < 3; p++) { rhs[p] += v[p] * L[(size_t)j]; for (int q = 0; q < 3; q++) A[p][q] += v[p] * v[q]; } }
            mean /= (double)px.size();
            for (int p = 0; p < 3; p++) for (int q = p + 1; q < 3; q++) { const double f = A[q][p] / A[p][p]; for (int k = p; k < 3; k++) A[q][k] -= f * A[p][k]; rhs[q] -= f * rhs[p]; }
            double co[3];
            for (int p = 2; p >= 0; p--) { double s = rhs[p]; for (int k = p + 1; k < 3; k++) s -= A[p][k] * co[k]; co[p] = s / A[p][p]; }
            double ss = 0, ms = 0;
            for (int j : px) {
                const int jx = j % W, jy = j / W;
                const double e = L[(size_t)j] - (co[0] + co[1] * jx / W + co[2] * jy / H);
                ss += e * e;
                double s = 0; int n = 0;
                for (int dy = -4; dy <= 4; dy++) for (int dx = -4; dx <= 4; dx++) {
                    const int qx = jx + dx, qy = jy + dy;
                    if (qx < 0 || qy < 0 || qx >= W || qy >= H || !mask[(size_t)qy * W + qx]) continue;
                    s += L[(size_t)qy * W + qx]; n++;
                }
                const double m = L[(size_t)j] - s / n;
                ms += m * m;
            }
            cvs.push_back((float)(std::sqrt(ss / (double)px.size()) / std::max(mean, 1.0)));
            mott.push_back((float)(std::sqrt(ms / (double)px.size()) / std::max(mean, 1.0)));
        }
        UnloadImage(on); UnloadImage(off);
    }
    BandShade out;
    out.clouds = (int)cvs.size();
    std::sort(cvs.begin(), cvs.end()); std::sort(mott.begin(), mott.end());
    if (!cvs.empty()) { out.cv = cvs[cvs.size() / 2]; out.mottle = mott[mott.size() / 2]; }
    return out;
}
// The lowest cloud pixel of each column over the middle 70% of every wide blob, and the rms of its residual from a straight line (px), the
// median over the blobs. `mk(view, on)` renders one view; `clouds` gets the number of blobs measured.
static float lowerLineRms(const std::function<Image(int, bool)>& mk, int views, int W, int H, int thresh, int minWidth, const char* dumpAs, int* clouds, int mode = 0) {
    std::vector<float> r;
    for (int v = 0; v < views; v++) {
        Image on = mk(v, true), off = mk(v, false);
        if (v == 0 && dumpAs) dump(dumpAs, on);
        for (const Blob& b : blobsOf(cloudMask(on, off, thresh), W, H)) {
            if (b.x1 - b.x0 < minWidth || b.area < 3000) continue;
            std::vector<int> low((size_t)W, -1);
            for (int j : b.px) low[(size_t)(j % W)] = std::max(low[(size_t)(j % W)], j / W);
            const int w = b.x1 - b.x0 + 1, x0 = b.x0 + (int)(0.15f * w), x1 = b.x1 - (int)(0.15f * w);
            std::vector<int> xs; std::vector<float> ys;
            for (int c = x0; c <= x1; c++) if (low[(size_t)c] >= 0) { xs.push_back(c); ys.push_back((float)low[(size_t)c]); }
            if (xs.size() < 80) continue;
            double ss = 0; size_t cnt = 0;
            if (mode == 2) {                                               // the columns at the level of the base only (a tower is much taller than its base, an anvil is higher): their spread
                int mx = 0;
                for (size_t i = 0; i < ys.size(); i++) mx = std::max(mx, (int)ys[i]);
                int flat = 0;                                              // the share of base columns NOT on the plateau (within 2 px of the lowest row): a plate has none
                for (size_t i = 0; i < ys.size(); i++) if (ys[i] >= mx - 30) { cnt++; if (ys[i] >= mx - 2) flat++; }
                if (cnt >= 100) { ss = (double)(cnt - (size_t)flat) / (double)cnt; ss = ss * ss * (double)cnt; } else cnt = 0;      // so that the sqrt of ss / cnt is that share
            } else if (mode == 1) {                                        // what is left once a 31 px running mean is taken out: the small lumps
                for (size_t i = 15; i + 15 < xs.size(); i++) {
                    double m = 0;
                    for (size_t k = i - 15; k <= i + 15; k++) m += ys[k];
                    const double e = ys[i] - m / 31.0; ss += e * e; cnt++;
                }
            } else {
                float a, bb; fitLine(ys, xs, &a, &bb);
                for (size_t i = 0; i < xs.size(); i++) { const double e = ys[i] - (a * xs[i] + bb); ss += e * e; cnt++; }
            }
            if (cnt) r.push_back((float)std::sqrt(ss / (double)cnt));
        }
        UnloadImage(on); UnloadImage(off);
    }
    if (clouds) *clouds = (int)r.size();
    if (r.empty()) return 0.f;
    std::sort(r.begin(), r.end());
    return r[r.size() / 2];
}
// Cumulus at range: from the ground, a few km away, the lumps are under a pixel; the base must still be a wavy, uneven line, not a ruler.
static float midRangeRms(int* n) {
    return lowerLineRms([](int v, bool on) {
        const float x = (v - 3.5f) * 2500.f;
        return look(filmClouds({ x, 2, 0 }, { x, 2 + 450, -2500 }, 0.4f, g3d::CloudType::Cumulus, on, 1500, 3500, 60, false), 960, 540, 0);
    }, 8, 960, 540, 60, 100, "cloud-base-mid.png", n, 1);
}
// A storm cell seen from the side at the height of its base, 38 km away: its lower silhouette (shreds included) is not a plate.
static g3d::ScenePtr cbScene(float cx, float x, unsigned seed, bool on, float camY, float camZ, Vector3 target, float fov, float shafts) {
    auto s = filmClouds({ cx, camY, camZ }, target, 0.f, g3d::CloudType::Cumulonimbus, on, 1200, 11000, fov, false);
    s->clouds.storms = { g3d::Clouds::Storm{ x + 977.f * (float)seed, 0.f, 5500.f, 10500.f } };
    s->clouds.haze = 0;
    s->clouds.seed = seed;
    s->clouds.shafts = shafts;
    return s;
}
static float cbBaseRms(int* n) {
    return lowerLineRms([](int v, bool on) {
        const unsigned seed = (unsigned)(v % 3) + 1;
        const float x = (float)(v / 3 - 1) * 3000.f, cx = x + 977.f * (float)seed + 4000.f;       // the cell stands left of centre; its base is the lowest, widest part
        auto s = cbScene(cx, x, seed, on, 1200, 30000, { cx, 1200, 0 }, 40, 0);
        s->sky.enabled = false; s->skyColor = BLACK;                       // clouds on black
        g3d::Material wm; wm.unlit = true; wm.color = BLACK;
        s->add(g3d::Geometry::box(300000, 90000, 10), wm)->position = { cx, 30000, -12000 };
        return look(s, 960, 540, 0);
    }, 9, 960, 540, 40, 150, "cloud-cb-base-side.png", n, 2);
}
// The underside of a storm cell seen from the ground, 9 km away: the lower half of the cloud's silhouette is a flat grey plate or it is lumpy
// and mottled. The luminance left over once a 15 px box average is taken out, over its mean.
static float cbUndersideMottle(int* n) {
    constexpr int W = 960, H = 540;
    std::vector<float> r;
    for (int v = 0; v < 6; v++) {
        const unsigned seed = (unsigned)(v % 3) + 1;
        const float x = (float)(v / 3) * 3000.f, cx = x + 977.f * (float)seed;
        auto mk = [&](bool on) {
            auto s = cbScene(cx, x, seed, on, 2, 9000, { cx, 1500, 0 }, 70, 0);
            s->sky.enabled = false; s->skyColor = BLACK;
            return look(s, W, H, 0);
        };
        Image on = mk(true), off = mk(false);
        if (v == 0) dump("cloud-cb-underside.png", on);
        const std::vector<char> mask = cloudMask(on, off, 8);
        const std::vector<float> L = lumaOf(on);
        for (const Blob& b : blobsOf(mask, W, H)) {
            if (b.area < 20000) continue;
            const int yMid = (b.y0 + b.y1) / 2;
            double ms = 0, mm = 0; int nm = 0;
            for (int j : b.px) {
                const int jx = j % W, jy = j / W;
                if (jy < yMid || jx < 8 || jy < 8 || jx >= W - 8 || jy >= H - 8) continue;
                double s = 0; int k = 0;
                for (int dy = -7; dy <= 7; dy += 2) for (int dx = -7; dx <= 7; dx += 2) { const int q = (jy + dy) * W + jx + dx; if (mask[(size_t)q]) { s += L[(size_t)q]; k++; } }
                if (k < 40) continue;                                       // not near the rim
                const double e = L[(size_t)j] - s / k;
                ms += e * e; mm += L[(size_t)j]; nm++;
            }
            if (nm > 2000) r.push_back((float)(std::sqrt(ms / nm) / std::max(mm / nm, 1.0)));
        }
        UnloadImage(on); UnloadImage(off);
    }
    if (n) *n = (int)r.size();
    if (r.empty()) return 0.f;
    std::sort(r.begin(), r.end());
    return r[r.size() / 2];
}
// The ratio of vertical-edge energy (horizontal gradient) to horizontal-edge energy (vertical gradient) over the lower third of a storm cell's
// silhouette seen from the ground (the base band): torn puffs are about 1; a texture stretched down in columns (flames, drips) is well above.
static float cbBaseAnisotropy(int* n) {
    constexpr int W = 960, H = 540;
    std::vector<float> r;
    for (int v = 0; v < 3; v++) {
        const unsigned seed = (unsigned)(v % 3) + 1;
        const float x = 0, cx = 0;
        auto mk = [&](bool on) {
            auto s = cbScene(0, 0, seed, on, 9, 0, { 400, 2300, -9000 }, 54, 0);                    // the film's view: 11 km from the cell, base 6 degrees up
            s->clouds.storms = { g3d::Clouds::Storm{ -1000.f, -11000.f, 6500.f, 10500.f } };
            s->clouds.cover = 0.16f; s->sky.enabled = false; s->skyColor = BLACK;
            return look(s, W, H, 0);
        };
        Image on = mk(true), off = mk(false);
        const std::vector<char> mask = cloudMask(on, off, 8);
        std::vector<float> L = lumaOf(on);
        for (const Blob& b : blobsOf(mask, W, H)) {
            if (b.area < 20000) continue;
            const int yCut = b.y1 - (int)(0.33f * (b.y1 - b.y0)) ;
            double gx = 0, gy = 0; int cnt = 0;
            for (int j : b.px) {
                const int jx = j % W, jy = j / W;
                if (jy < yCut || jx < 6 || jy < 6 || jx >= W - 6 || jy >= H - 6) continue;
                bool inner = true;
                for (int dy = -5; dy <= 5 && inner; dy += 5) for (int dx = -5; dx <= 5; dx += 5) if (!mask[(size_t)(jy + dy) * W + jx + dx]) { inner = false; break; }
                if (!inner) continue;                                       // the silhouette's own edge is not texture
                const double ex = L[(size_t)j + 2] - L[(size_t)j - 2], ey = L[(size_t)j + 2 * W] - L[(size_t)j - 2 * W];
                gx += ex * ex; gy += ey * ey; cnt++;
            }
            if (cnt > 1500 && gy > 0) r.push_back((float)(gx / gy));
        }
        if (v == 0) dump("cloud-cb-base-band.png", on);
        UnloadImage(on); UnloadImage(off);
    }
    if (n) *n = (int)r.size();
    if (r.empty()) return 0.f;
    std::sort(r.begin(), r.end());
    return r[r.size() / 2];
}
// A squall's stratus deck seen from the ground at 20-40 degrees: stddev/mean of the luminance after the planar trend is taken out. A flat
// sheet is near 0; soft cells, rolls and pannus show.
static float deckMottle() {
    constexpr int W = 640, H = 360;
    std::vector<float> r;
    for (int v = 0; v < 4; v++) {
        const float yaw = (float)v * 1.3f;
        auto s = filmClouds({ 0, 2, 0 }, { std::cos(yaw) * 1000.f, 2 + 577.f, std::sin(yaw) * 1000.f }, 0.98f, g3d::CloudType::Stratus, true, 450, 1800, 30, false);
        s->clouds.density = 1.6f;
        Image on = look(s, W, H, 0);
        if (v == 0) dump("cloud-deck-below.png", on);
        std::vector<float> L = lumaOf(on);
        double A[3][3] = {}, rhs[3] = {}, mean = 0;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { const double p[3] = { 1.0, (double)x / W, (double)y / H }; const double l = L[(size_t)y * W + x]; mean += l; for (int a = 0; a < 3; a++) { rhs[a] += p[a] * l; for (int b = 0; b < 3; b++) A[a][b] += p[a] * p[b]; } }
        mean /= (double)(W * H);
        for (int a = 0; a < 3; a++) for (int b = a + 1; b < 3; b++) { const double f = A[b][a] / A[a][a]; for (int k = a; k < 3; k++) A[b][k] -= f * A[a][k]; rhs[b] -= f * rhs[a]; }
        double co[3];
        for (int a = 2; a >= 0; a--) { double q = rhs[a]; for (int k = a + 1; k < 3; k++) q -= A[a][k] * co[k]; co[a] = q / A[a][a]; }
        double ss = 0;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { const double e = L[(size_t)y * W + x] - (co[0] + co[1] * x / W + co[2] * y / H); ss += e * e; }
        r.push_back((float)(std::sqrt(ss / (double)(W * H)) / std::max(mean, 1.0)));
        UnloadImage(on);
    }
    std::sort(r.begin(), r.end());
    return r[r.size() / 2];
}
// Rain shafts: with the rain on, a column under the cell's base is darker and greyer than the same pixels without it; rain off is the plain picture.
static void rainShafts() {
    constexpr int W = 960, H = 540;
    auto mk = [&](float shafts) {
        auto s = cbScene(0, 0, 1, true, 2, 20000, { 0, 700, 0 }, 60, shafts);
        return look(s, W, H, 0);
    };
    Image off = mk(0), neg = mk(-1), on = mk(1);
    dump("cloud-shafts-off.png", off); dump("cloud-shafts-on.png", on);
    CHECK(diffImages(off, neg) == 0);                                         // no rain: the plain picture
    double lOn = 0, lOff = 0, sOn = 0, sOff = 0; int n = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const Color a = GetImageColor(on, x, y), b = GetImageColor(off, x, y);
            if (pixelDelta(a, b) < 30) continue;
            lOn += luma(a); lOff += luma(b); sOn += saturation(a); sOff += saturation(b); n++;
        }
    std::printf("[weather] rain shafts: %d px differ; luminance there %.1f with, %.1f without; saturation %.2f with, %.2f without\n", n, n ? lOn / n : 0, n ? lOff / n : 0, n ? sOn / n : 0, n ? sOff / n : 0);
    CHECK(n >= 1500);
    CHECK(n && lOn < 0.88 * lOff);
    UnloadImage(off); UnloadImage(neg); UnloadImage(on);
}
static void cumulusBases() {
    int n = 0;
    const float rms = baseRagged("now", &n);
    const BandShade band = bandShade("now");
    std::printf("[weather] cumulus underside seen from the ground (lowest third of %d clouds): luminance cv %.3f, 9 px mottle %.3f\n", band.clouds, band.cv, band.mottle);
    CHECK(band.clouds >= 3);
    const BaseShade sh = baseShade("now");
    std::printf("[weather] cumulus base: lower silhouette rms from a line %.2f px (%d clouds); from below luminance cv %.3f (planar trend out), mottle %.3f (19 px average out), centre/rim %.2f (%d clouds)\n", rms, n, sh.cv, sh.mottle, sh.centreOverRim, sh.clouds);
    CHECK(n >= 3 && sh.clouds >= 3);
    CHECK(rms >= 3.0f);                                             // the ruler-flat base measures 1.28
    CHECK(sh.cv >= 0.10f);
    CHECK(sh.mottle >= 0.060f);                                    // the flat slab measures 0.041
    CHECK(sh.centreOverRim <= 0.85f);
    int nm = 0, nc = 0;
    int ncm = 0;
    const float mid = midRangeRms(&nm), cb = cbBaseRms(&nc), cbm = cbUndersideMottle(&ncm);
    std::printf("[weather] base line: cumulus at range rough %.2f px (%d clouds); storm cell side-on non-plateau share %.2f (%d cells); storm underside mottle %.3f (%d cells)\n", mid, nm, cb, nc, cbm, ncm);
    CHECK(ncm >= 3);
    CHECK(cbm >= 0.15f);                                           // the flat plate measures 0.087
    int nan = 0;
    const float aniso = cbBaseAnisotropy(&nan), deck = deckMottle();
    std::printf("[weather] storm base band: vertical/horizontal edge energy %.2f (%d cells); stratus deck from below: luminance cv %.3f (planar trend out)\n", aniso, nan, deck);
    CHECK(nan >= 3);
    CHECK(aniso <= 1.3f);
    CHECK(deck >= 0.08f);
    rainShafts();
}

// Cloud cost of two presets at 1080p (RMTN_BASE_TIMING): frame time with the clouds minus without, best of 12.
static void presetCloudTiming() {
    constexpr int W = 1920, H = 1080;
    for (const char* name : { "cumulus day", "thunderstorm" }) {
        double best[2] = { 1e9, 1e9 };
        for (int on = 0; on < 2; on++) {
            auto s = cloudScene({ 0, 2, 0 }, { 0, 2 + 300, -2000 }, 0.5f);
            wx::applyPreset(*s, name);
            s->clouds.enabled = on == 1;
            s->clouds.shadows = true;
            s->clouds.quality = g3d::Clouds::Quality::Film;
            Comp c = comp(s, W, H);
            Canvas canvas;
            canvas.load(W, H, 1);
            presentFrame(c, 0, canvas);
            for (int i = 0; i < 12; i++) {
                const double t0 = GetTime();
                presentFrame(c, i, canvas);
                Image img = LoadImageFromTexture(canvas.output().texture);
                best[on] = std::min(best[on], (GetTime() - t0) * 1000);
                UnloadImage(img);
            }
            canvas.unload();
        }
        std::printf("[weather] preset %s at 1080p: clouds cost %.1f ms (frame %.1f with, %.1f without)\n", name, best[1] - best[0], best[1], best[0]);
    }
}

int main() try {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 180, "raymotion weather");
    if (!IsWindowReady()) return 77;
    if (std::getenv("RMTN_BASE_TIMING")) { presetCloudTiming(); shutdownGraphics(); CloseWindow(); return 0; }
    if (std::getenv("RMTN_BASE_ONLY")) {
        cumulusBases();
        shutdownGraphics(); CloseWindow();
        std::printf(failures ? "[weather] %d failures\n" : "[weather] all passed\n", failures);
        return failures ? 1 : 0;
    }
    if (const char* dir = std::getenv("RMTN_CLOUD_IMAGES")) { cloudImages(dir); shutdownGraphics(); CloseWindow(); return 0; }
    if (std::getenv("RMTN_CLOUD_TIMING")) { cloudTiming(); shutdownGraphics(); CloseWindow(); return 0; }
    if (std::getenv("RMTN_W6_GOLDEN")) {
        Image g = look(w6Golden(), 320, 180, 7);
        dump("w6-golden.png", g);
        for (float v : gridMeans(g, 6, 4)) std::printf("%.2ff, ", v);
        std::printf("\n");
        shutdownGraphics(); CloseWindow(); return 0;
    }
    if (std::getenv("RMTN_W9_IMAGES")) { w9Images(); shutdownGraphics(); CloseWindow(); return 0; }
    if (std::getenv("RMTN_W10_TIMING")) { airTiming(); shutdownGraphics(); CloseWindow(); return 0; }
    if (std::getenv("RMTN_W11_ONLY")) {
        cameraWeather();
        shutdownGraphics(); CloseWindow();
        std::printf(failures ? "[weather] %d failures\n" : "[weather] all passed\n", failures);
        return failures ? 1 : 0;
    }
    if (std::getenv("RMTN_W10_ONLY")) {
        lightInAir();
        shutdownGraphics(); CloseWindow();
        std::printf(failures ? "[weather] %d failures\n" : "[weather] all passed\n", failures);
        return failures ? 1 : 0;
    }
    if (std::getenv("RMTN_W9_TIMING")) { w9Timing(); shutdownGraphics(); CloseWindow(); return 0; }
    if (std::getenv("RMTN_W5_ONLY")) {
        lightningStorm();
        shutdownGraphics(); CloseWindow();
        std::printf(failures ? "[weather] %d failures\n" : "[weather] all passed\n", failures);
        return failures ? 1 : 0;
    }
    if (std::getenv("RMTN_W9_ONLY")) {
        ocean();
        shutdownGraphics(); CloseWindow();
        std::printf(failures ? "[weather] %d failures\n" : "[weather] all passed\n", failures);
        return failures ? 1 : 0;
    }
    if (std::getenv("RMTN_W8_IMAGES")) { w8Images(); shutdownGraphics(); CloseWindow(); return 0; }
    if (std::getenv("RMTN_W6_TIMING")) { waterTiming(); shutdownGraphics(); CloseWindow(); return 0; }
    if (std::getenv("RMTN_W4_TIMING")) { groundTiming(); shutdownGraphics(); CloseWindow(); return 0; }
    if (std::getenv("RMTN_W3_TIMING")) { precipitationTiming(); shutdownGraphics(); CloseWindow(); return 0; }
    if (std::getenv("RMTN_W3_ONLY")) {
        precipitation();
        shutdownGraphics(); CloseWindow();
        std::printf(failures ? "[weather] %d failures\n" : "[weather] all passed\n", failures);
        return failures ? 1 : 0;
    }
    if (std::getenv("RMTN_W4_ONLY")) {
        wetAndSnow();
        shutdownGraphics(); CloseWindow();
        std::printf(failures ? "[weather] %d failures\n" : "[weather] all passed\n", failures);
        return failures ? 1 : 0;
    }
    if (std::getenv("RMTN_W8_ONLY")) {
        waterWakes();
        shutdownGraphics(); CloseWindow();
        std::printf(failures ? "[weather] %d failures\n" : "[weather] all passed\n", failures);
        return failures ? 1 : 0;
    }
    if (std::getenv("RMTN_W7_ONLY")) {
        waterBodies();
        shutdownGraphics(); CloseWindow();
        std::printf(failures ? "[weather] %d failures\n" : "[weather] all passed\n", failures);
        return failures ? 1 : 0;
    }
    if (std::getenv("RMTN_W6_ONLY")) {
        waterReflections();
        shutdownGraphics(); CloseWindow();
        std::printf(failures ? "[weather] %d failures\n" : "[weather] all passed\n", failures);
        return failures ? 1 : 0;
    }
    cpuRules();
    mixContinuous();
    oneWind();
    offMeansOff();
    mixRenders();
    overcastDims();
    purity();
    waterAliasing();
    coverFromBelow();
    groundThroughGaps();
    noPopThroughBase();
    cloudShadowsDrift();
    occlusion();
    cloudPurity();
    banding();
    cloudsOffMeansOff();
    curvature();
    notExtruded();
    cumulusBases();
    softEdges();
    stipple();
    noSeam();
    crispSun();
    hazeAtHorizon();
    nearDetail();
    noBandsInHeight();
    waterReflections();
    precipitation();
    wetAndSnow();
    waterBodies();
    waterWakes();
    ocean();
    lightningStorm();
    lightInAir();
    cameraWeather();
    shutdownGraphics();
    CloseWindow();
    std::printf(failures ? "[weather] %d failures\n" : "[weather] all passed\n", failures);
    return failures ? 1 : 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "weather tests threw: %s\n", e.what());
    return 1;
}
