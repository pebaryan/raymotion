// weather_tests.cpp -- g3d::Weather (W1, one weather state): one wind for every system, off means
// off, mix is continuous, overcast dims the scene, and a weather animated by Scene::up is pure.
// Small frames; every check measures something that is drawn (or the CPU function the draw calls),
// and each has a negative control that must fail when the system ignores the weather.
#include "rmtn/raymotion.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
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

int main() try {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 180, "raymotion weather");
    if (!IsWindowReady()) return 77;
    cpuRules();
    mixContinuous();
    oneWind();
    offMeansOff();
    mixRenders();
    overcastDims();
    purity();
    waterAliasing();
    shutdownGraphics();
    CloseWindow();
    std::printf(failures ? "[weather] %d failures\n" : "[weather] all passed\n", failures);
    return failures ? 1 : 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "weather tests threw: %s\n", e.what());
    return 1;
}
