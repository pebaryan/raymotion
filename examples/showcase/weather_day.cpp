// weather_day.cpp -- "weather-day": the engine demo for raymotion 0.14 (not a film). One lake seen
// from one jetty through one day: dawn mist, cumulus building, a squall with lightning, a rainbow, a
// calm evening. The camera stays where it is; one g3d::Weather (through wx::apply and a few keys)
// moves the sky, the clouds, the wind, the lake, the rain, the ground and the light. Every number on
// screen is read from the scene's own state at draw time (docs/one-weather-day-brief.md).
// Scenes are built at the first frame that draws them, so `raymotion list` pays nothing.
// Set RMTN_WD_CLEARANCE=<file> to log the camera's height over the ground at every frame, and
// RMTN_WD_THUNDER=<file> writes the engine's thunderCues (distance, arrival) for every strike, to check the readout table.
//   raymotion render weather-day
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace rmtn {
namespace {

constexpr int FPS = 30, TOTAL = 1620;
constexpr double END_S = TOTAL / 30.0;

const Color INK = mkColor(0xf2f4ff), SUB = mkColor(0xb4bfcc), AMBER = mkColor(0xffd27a), TEAL = mkColor(0x3ad6c5);
const Color SCRIM = Color{ 12, 16, 22, 255 };
Font bold() { return fonts::ui(true); }
Font regular() { return fonts::ui(false); }

float sm(double t, double a, double b) {
    const double u = std::clamp((t - a) / (b - a), 0.0, 1.0);
    return (float)(u * u * (3 - 2 * u));
}
float rampS(double s, double a, double b) { return (float)std::clamp((s - a) / (b - a), 0.0, 1.0); }
Ctx faded(const Ctx& cx, float a) { Ctx c = cx; c.a *= std::clamp(a, 0.f, 1.f); return c; }

// A piecewise-linear curve of film seconds.
struct Key { double s; float v; };
float curve(double s, const std::vector<Key>& k) {
    if (s <= k.front().s) return k.front().v;
    for (size_t i = 1; i < k.size(); i++)
        if (s <= k[i].s) return k[i - 1].v + (k[i].v - k[i - 1].v) * (float)((s - k[i - 1].s) / (k[i].s - k[i - 1].s));
    return k.back().v;
}

// ---------------------------------------------------------------- the day: the film's keys (film seconds)
// Chapter times; the picture, the lightning, the sound and the labels all read these.
constexpr double CH[6] = { 0, 8.5, 17.5, 33.0, 40.5, 48.0 };
constexpr double STRIKE_T[1] = { 26.9 };
constexpr double LENS_T0 = 27.8, LENS_T1 = 33.8;
constexpr double SWITCH_S = 26.0;                    // the cloud layer changes from cumulus to storm on the first flash

const std::vector<Key> HOUR = { { 0, 6.0f }, { 8.5, 7.6f }, { 17.5, 12.0f }, { 25, 14.2f }, { 33, 15.4f }, { 40.5, 17.0f }, { 48, 18.1f }, { 54, 18.3f } };
const std::vector<Key> WIND = { { 0, 0.6f }, { 8.5, 1.2f }, { 12, 5 }, { 17.5, 8 }, { 22, 11 }, { 26, 14 }, { 31, 13 }, { 34, 7 }, { 38, 3 }, { 41, 1.8f }, { 48, 1.2f }, { 54, 1 } };
const std::vector<Key> COVER = { { 0, 0.12f }, { 8, 0.25f }, { 12, 0.45f }, { 17, 0.38f }, { 21, 0.3f }, { 26, 0.34f }, { 32, 0.5f }, { 36, 0.4f }, { 40, 0.2f }, { 54, 0.2f } };
const std::vector<Key> HAZE = { { 0, 0.2f }, { 7, 0.17f }, { 12, 0.06f }, { 26, 0.14f }, { 31, 0.18f }, { 36, 0.08f }, { 42, 0.1f }, { 54, 0.1f } };
const std::vector<Key> DENSITY = { { 0, 0.7f }, { 17, 1.1f }, { 26, 1.8f }, { 36, 1.0f }, { 41, 0.45f }, { 54, 0.4f } };
const std::vector<Key> GLOOM = { { 0, 0 }, { 20, 0.1f }, { 26, 0.55f }, { 32, 0.5f }, { 37, 0.1f }, { 54, 0 } };
const std::vector<Key> BOW = { { 0, 0 }, { 33, 0 }, { 34.5, 0.5f }, { 37, 0.6f }, { 39.5f, 0.2f }, { 40.5f, 0 } };
const std::vector<Key> CLOUDV = { { 0, 60 }, { 12, 100 }, { 17, 250 }, { 24, 900 }, { 30, 700 }, { 34, 300 }, { 54, 100 } };   // time-lapse m/s of the cloud run

constexpr float WIND_DIR = 100;                       // degrees: towards +z and a little back to the camera
constexpr float CAM_X = 6, CAM_Y = 2.82f;             // the jetty
constexpr float DECK_Y = 1.12f;

float cloudRun(double s) {                            // metres the clouds have run: the integral of CLOUDV
    float run = 0;
    for (double u = 0.025; u < s; u += 0.05) run += curve(u, CLOUDV) * 0.05f;
    return run;
}
Vector2 windVec() { return { std::cos(WIND_DIR * DEG2RAD), std::sin(WIND_DIR * DEG2RAD) }; }

// ---------------------------------------------------------------- the land
constexpr float LCX = 200, LCZ = 0, LAX = 185, LAZ = 125;
float lakeRho(float x, float z) {
    const float dx = (x - LCX) / LAX, dz = (z - LCZ) / LAZ;
    const float wob = 1 + 0.2f * (flora::noise(x, z, 150, 5) - 0.5f);
    return std::sqrt(dx * dx + dz * dz) / wob;
}
struct Ridge { float x, h, w; };
const Ridge RIDGE[5] = { { 2600, 190, 1100 }, { 6200, 520, 2000 }, { 11500, 1050, 3200 }, { 17500, 1750, 4500 }, { 25000, 2700, 6000 } };
float crest(int i, float z) {
    return 0.62f + 0.38f * std::sin(z * 0.00061f + i * 2.1f) * std::sin(z * 0.00153f + i * 0.7f + 1) + 0.18f * std::sin(z * 0.0043f + i * 1.3f);
}
float groundAt(float x, float z) {
    const float near = sm(std::hypot(x - CAM_X, z), 10, 90);
    float land = 1.0f + near * (2.2f * flora::noise(x, z, 70, 3) + 1.5f * flora::noise(x, z, 25, 9));
    const float hills = sm(x, 330, 1500);
    land += hills * (30 * flora::noise(x, z, 600, 7) + 18 * flora::noise(x, z, 200, 11)) + 0.000003f * z * z;
    for (int i = 0; i < 5; i++) {
        const float d = (x - RIDGE[i].x) / RIDGE[i].w;
        land += RIDGE[i].h * crest(i, z) * std::exp(-d * d);
    }
    const float b = sm(lakeRho(x, z), 0.93f, 1.06f);        // the bank slopes down into the lake
    return -2.2f + (land + 2.2f) * b;
}
std::vector<Vector2> lakeOutline(float rho, int n = 64) {
    std::vector<Vector2> ring;
    for (int i = 0; i < n; i++) {
        const float a = 2 * PI * i / n, c = std::cos(a), s = std::sin(a);
        float lo = 0.05f, hi = 2.2f;
        for (int k = 0; k < 30; k++) {
            const float m = 0.5f * (lo + hi);
            (lakeRho(LCX + c * LAX * m, LCZ + s * LAZ * m) < rho ? lo : hi) = m;
        }
        ring.push_back({ LCX + c * LAX * lo, LCZ + s * LAZ * lo });
    }
    return ring;
}

// ---------------------------------------------------------------- the camera
float headingDeg(double s) { return -8.f + 16.f * sm(s, 0, 48); }
void poseCamera(g3d::Camera& cam, int t) {
    const double s = t / 30.0;
    const float h = headingDeg(s) * DEG2RAD, pitch = (3.f + 2.f * sm(s, 8, 20)) * DEG2RAD;
    cam.position = { CAM_X, CAM_Y, 0 };
    cam.target = { CAM_X + 100 * std::cos(h), CAM_Y + 100 * std::tan(pitch), 100 * std::sin(h) };
    cam.upVector = { 0, 1, 0 };
}

// ---------------------------------------------------------------- strikes
struct StrikeAt { float time; Vector3 foot, top; uint32_t seed; bool tagged; };
constexpr int NSTRIKES = 7;
// Seven strikes across the squall, near and far, left and right of the lens axis; the three tagged ones carry the
// distance / delay readout (the far one, the middle one and the close one over the lake).
const StrikeAt STRIKES[NSTRIKES] = {
    { 24.6f, { 1300, 0, -520 }, { 1260, 1000, -480 }, 5, false },
    { 25.6f, { 3000, 0, 500 }, { 2950, 1300, 460 }, 41, false },
    { (float)STRIKE_T[0], { 2300, 0, 300 }, { 2250, 1000, 260 }, 7, true },
    { 27.8f, { 700, 0, 220 }, { 660, 900, 180 }, 13, false },
    { 29.0f, { 1000, 0, -350 }, { 940, 950, -320 }, 19, true },
    { 31.0f, { 520, 0, 150 }, { 470, 900, 120 }, 31, true },
    { 32.4f, { 1500, 0, 650 }, { 1450, 1000, 610 }, 23, false } };
float strikeDistance(const StrikeAt& k) {                 // as g3d::thunderCues: the nearest of the ends and the middle
    const Vector3 cam{ CAM_X, CAM_Y, 0 }, mid = Vector3Scale(Vector3Add(k.foot, k.top), 0.5f);
    return std::min({ Vector3Distance(cam, k.foot), Vector3Distance(cam, k.top), Vector3Distance(cam, mid) });
}

// ---------------------------------------------------------------- the shot: one scene, built lazily
struct Day {
    g3d::ScenePtr scene;
    float hour = 6.2f;
    flora::Area wood;
    std::vector<flora::Placement> at;
    std::vector<flora::TreePtr> trees;
    std::vector<g3d::Clouds::Storm> cells;           // the storm cells at full size, in the pattern frame
};
using DayP = std::shared_ptr<Day>;

g3d::Weather::Key hk(float s, g3d::Precipitation k, float v) { g3d::Weather::Key h; h.seconds = s; h.kind = k; h.intensity = v; return h; }
std::vector<g3d::Weather::Key> rainHistory() {
    using P = g3d::Precipitation;
    return { hk(0, P::None, 0), hk(23, P::Rain, 0), hk(26.5f, P::Rain, 0.9f), hk(31, P::Rain, 0.8f), hk(34.5f, P::Rain, 0), hk(34.6f, P::None, 0) };
}

void logClearance(int t, float h) {
    static const char* path = std::getenv("RMTN_WD_CLEARANCE");
    if (!path) return;
    if (FILE* f = std::fopen(path, "a")) { std::fprintf(f, "%d %.3f\n", t, h); std::fclose(f); }
}

// The weather at film second s, onto the scene (pure: everything from s).
void setDay(Day& d, g3d::Scene& sc, int t) {
    const double s = t / 30.0;
    wx::Look A = wx::look("fog dawn"), B = wx::look("cumulus day"), C = wx::look("thunderstorm"), D = wx::look("golden evening");
    wx::Look k;
    if (s < 14) k = wx::mix(A, B, sm(s, 2, 12));
    else if (s < 30) k = wx::mix(B, C, sm(s, 16, 25));
    else k = wx::mix(C, D, sm(s, 31, 39));
    k.hour = curve(s, HOUR);
    k.weather.wind.speed = curve(s, WIND);
    k.weather.wind.direction = WIND_DIR;
    k.weather.clouds.cover = curve(s, COVER);
    const bool tower = true; (void)SWITCH_S;                  // cumulus until the first flash, then the storm layer (a cut on the flash)
    k.weather.clouds.type = tower ? g3d::CloudType::Cumulonimbus : g3d::CloudType::Cumulus;
    k.weather.haze = curve(s, HAZE);
    k.weather.lightningRate = 0;
    k.weather.rainbow.distantRain = curve(s, BOW);
    k.weather.rainbow.strength = sm(s, 33.0, 34.6);      // the bow belongs to chapter 4: Auto would draw it in the squall (rain falling with the sun out)
    if (std::getenv("RMTN_WD_NOBOW")) k.weather.rainbow.mode = g3d::Weather::Rainbow::Mode::Off;
    k.cloudBase = 1300.f - 600.f * sm(s, 16, 26);      // the base lowers and the top climbs before the type changes, so the switch is small
    k.cloudTop = 3000.f + 8000.f * sm(s, 16, 26);
    k.cloudDensity = curve(s, DENSITY);
    k.gloom = curve(s, GLOOM);
    k.shafts = g3d::Volumetric::Quality::Medium;
    k.aerial = 1.2f;
    if (std::getenv("RMTN_WD_NOVOL")) { k.shafts = g3d::Volumetric::Quality::Off; k.aerial = 0; }
    wx::apply(sc, k, FPS);
    if (std::getenv("RMTN_WD_NOCLOUD")) sc.clouds.enabled = false;
    d.hour = k.hour;
    g3d::Weather& w = sc.weather;
    w.history = rainHistory();
    w.accumulationScale = 60;
    w.wetness = -1;
    w.windRun = cloudRun(s);
    sc.clouds.shafts = std::clamp(0.9f * sm(s, 12, 22) * (1.f - sm(s, 33, 38)), 0.f, 1.f);   // the cells' rain veils, before the rain reaches the jetty
    {   // the cells leave: after the squall they drift off to the far side (a move, not a shrink)
        const float away = sm(s, 33, 44) * 45000.f, awayX = sm(s, 33, 44) * 70000.f;
        sc.clouds.storms = d.cells;
        const float gr = 0.35f + 0.65f * sm(s, 6, 26);        // the tower is a small cell on the far ridge at first and grows
        for (auto& c : sc.clouds.storms) { c.radius *= gr; c.top = 1800 + (c.top - 1800) * gr; }
        for (auto& c : sc.clouds.storms) { c.z += away; c.x += awayX; }
        const float come = 15000.f * (1.f - sm(s, 10, 30)) + 60000.f * (1.f - sm(s, 6, 16));   // the cells arrive from beyond the far ridges (hidden behind them for the dawn)
        for (auto& c : sc.clouds.storms) c.x += come;
    }
    w.precipitation.kind = g3d::Precipitation::Rain;
    w.precipitation.intensity = w.rainAt(s);
    sc.precipitation.enabled = w.precipitation.intensity > 0.001f;
    sc.precipitation.radius = 45;
    sc.precipitation.farRadius = 500;
    logClearance(t, CAM_Y - groundAt(CAM_X, 0));
}

DayP makeDay() {
    auto d = std::make_shared<Day>();
    auto s = std::make_shared<g3d::Scene>();
    d->scene = s;
    g3d::Scene& sc = *s;
    sc.ambient = 0.45f;
    sc.shadowMapSize = 8192;
    sc.camera.fov = 48;
    sc.camera.nearPlane = 0.3f;
    sc.camera.farPlane = 60000;
    sc.camera.up = [](int t, g3d::Camera& cam) { poseCamera(cam, t); };
    g3d::Light sun = g3d::Light::directional({ 90, 20, 0 }, { 255, 244, 224, 255 }, 2.4f, true);
    sun.target = { 90, 0, 0 };
    sun.shadowExtent = 90;
    sc.lights.push_back(sun);
    sc.sky.enabled = true;
    sc.sky.resolution = 512;
    sc.fogFalloff = 0.012f;

    // the land: the near mesh over the lake and its banks, a coarse one out to the ridges
    flora::Area near = flora::rect({ -80, -520 }, { 430, 520 });
    near.ground = groundAt;
    flora::Area far = flora::rect({ 380, -20000 }, { 32000, 20000 });
    far.ground = groundAt;
    g3d::Material ground;
    ground.color = mkColor(0x4a6334);
    ground.roughness = 1;
    sc.add(flora::terrain(near, 254, 254), ground);
    g3d::Material farGround = ground;
    farGround.color = mkColor(0x2f4a2c);
    sc.add(flora::terrain(far, 254, 254), farGround)->position = { 0, -0.4f, 0 };

    // the lake
    wx::WaterSpec lake;
    lake.shore = lakeOutline(1.0f);
    lake.look = wx::Surface::Breezy;
    g3d::WaterBody body = wx::waterBody(lake);
    body.look.fps = FPS;
    sc.waterBodies.push_back(body);

    // grass on the near bank, and the trees round the lake
    const std::vector<Vector2> keepOut = lakeOutline(1.07f);
    flora::Area bank = flora::rect({ -40, -60 }, { 60, 60 });
    bank.ground = groundAt;
    bank.exclude.push_back(keepOut);
    bank.edge = 6;
    g3d::Field grass = flora::grass("lawn");
    grass.fps = FPS;
    grass.fadeEnd = 60;
    grass.density = 260;
    flora::addGrass(sc, bank, grass, 5);
    d->wood = flora::rect({ -60, -330 }, { 430, 330 });
    d->wood.ground = groundAt;
    d->wood.exclude.push_back(keepOut);
    d->wood.clearings.push_back({ { CAM_X, 0 }, 9 });
    flora::ScatterSpec ss;
    ss.spacing = 9;
    ss.clumping = 0.55f;
    ss.clumpScale = 60;
    ss.variants = 4;
    d->at = flora::scatter(d->wood, ss, 21);
    for (const char* n : { "oak", "birch", "pine", "spruce" }) d->trees.push_back(flora::makeTree(flora::tree(n)));
    flora::ForestSpec fs;
    fs.fps = FPS;
    fs.impostorFrom = 90;
    flora::addForest(sc, d->at, d->trees, fs);

    // the jetty: a deck, plank lines, piles, posts and a rail
    g3d::Material wood;
    wood.color = mkColor(0x6d5a44);
    wood.roughness = 0.9f;
    g3d::Material dark = wood;
    dark.color = mkColor(0x3d3226);
    sc.add(g3d::Geometry::box(24, 0.12f, 2.2f), wood)->position = { 16, DECK_Y, 0 };
    for (int i = 0; i < 26; i++) sc.add(g3d::Geometry::box(0.03f, 0.02f, 2.2f), dark)->position = { 4.6f + i * 0.92f, DECK_Y + 0.07f, 0 };
    for (int i = 0; i < 7; i++)
        for (int side : { -1, 1 }) {
            const float x = 4.5f + i * 4;
            sc.add(g3d::Geometry::cylinder(0.12f, 3.2f, 12), dark)->position = { x, DECK_Y - 1.6f, side * 1.0f };
            sc.add(g3d::Geometry::cylinder(0.09f, 1.0f, 12), dark)->position = { x, DECK_Y + 0.5f, side * 1.05f };
        }
    for (int side : { -1, 1 }) sc.add(g3d::Geometry::box(24, 0.07f, 0.07f), wood)->position = { 16, DECK_Y + 0.95f, side * 1.05f };

    // the boat: it rides the lake and its own wake is left out of its height
    wx::Surface unused = wx::Surface::Calm; (void)unused;
    g3d::Wake wake;
    wake.path = { { 34, -40 }, { 34, 60 } };            // across the lake in front of the jetty end: the boat shows its side
    wake.speed = 5;
    wake.start = 37;
    wake.length = 7.3f;
    wake.beam = 2.7f;
    sc.wakes.push_back(wake);
    g3d::Object3D* boat = sc.add();
    g3d::Material hull, trim;
    hull.color = mkColor(0x2f5f7a); hull.roughness = 0.5f;
    trim.color = mkColor(0xe8e2d4); trim.roughness = 0.6f;
    boat->add(g3d::Geometry::box(1.7f, 0.55f, 3.2f), hull)->position = { 0, 0.12f, -0.4f };
    g3d::Object3D* bow = boat->add();
    bow->position = { 0, 0.12f, 1.2f };
    bow->scale = { 1, 1, 1.5f };
    bow->add(g3d::Geometry::box(1.2f, 0.55f, 1.2f), hull)->rotation = { 0, 45, 0 };
    boat->add(g3d::Geometry::box(1.76f, 0.08f, 3.3f), trim)->position = { 0, 0.42f, -0.4f };
    {   // a small cabin cruiser: cabin, windows, bow rail
        g3d::Material glass, stripe;
        glass.color = mkColor(0x1b2630); glass.roughness = 0.2f;
        stripe.color = mkColor(0xc2452d); stripe.roughness = 0.5f;
        boat->add(g3d::Geometry::box(1.2f, 0.7f, 1.3f), trim)->position = { 0, 0.82f, -0.75f };
        boat->add(g3d::Geometry::box(1.24f, 0.26f, 0.9f), glass)->position = { 0, 0.92f, -0.72f };
        boat->add(g3d::Geometry::box(1.3f, 0.06f, 1.4f), stripe)->position = { 0, 1.2f, -0.75f };
        boat->add(g3d::Geometry::box(1.74f, 0.07f, 3.3f), stripe)->position = { 0, 0.34f, -0.4f };
    }
    boat->scale = { 1.6f, 1.6f, 1.6f };
    g3d::Scene* scp = &sc;
    boat->up = [wake, scp](int t, g3d::Object3D& self) {
        const double s = t / 30.0;
        Vector2 p, h;
        self.visible = wake.at(s, &p, &h);
        if (!self.visible) return;
        self.position = { p.x, 0, p.y };
        self.rotation = { 0, std::atan2(h.x, h.y) * RAD2DEG, 0 };
        g3d::Ride how;
        how.length = 7.3f; how.beam = 2.7f; how.draft = 0.05f; how.heading = std::atan2(h.y, h.x) * RAD2DEG; how.skipWake = 0;
        g3d::ride(self, *scp, s, how);
    };

    // clouds, strikes
    sc.clouds.seed = 4;
    sc.clouds.quality = g3d::Clouds::Quality::Film;
    sc.clouds.anvil = 1;
    {   // two cells whose centres lie where we want them at film second 28, in the pattern's frame
        const Vector2 dv = windVec();
        const float run = cloudRun(28);
        auto cell = [&](float wx_, float wz_, float r, float top) {
            g3d::Clouds::Storm c;
            c.x = wx_ - run * dv.x; c.z = wz_ - run * dv.y; c.radius = r; c.top = top;
            return c;
        };
        d->cells = { cell(2500, -300, 3500, 9500), cell(7000, -6000, 3000, 8500) };
    }
    sc.lightning.enabled = true;
    sc.lightning.rate = 0;
    sc.lightning.fps = FPS;
    sc.lightning.width = 4.f;                          // a bolt a kilometre away still reads as a line
    sc.lightning.groundY = 0;
    {
        const Vector2 dv = windVec();
        for (const StrikeAt& k : STRIKES) {
            const float run = cloudRun(k.time);
            g3d::Lightning::Strike st;
            st.time = k.time;
            st.start = { k.top.x - run * dv.x, k.top.y, k.top.z - run * dv.y };
            st.end = { k.foot.x - run * dv.x, k.foot.y, k.foot.z - run * dv.y };
            st.seed = k.seed;
            st.intensity = 1.6f;
            sc.lightning.strikes.push_back(st);
        }
    }
    sc.ground.seed = 3;
    sc.precipitation.seed = 3;
    wx::apply(sc, wx::look("fog dawn"), FPS);
    Day* dp = d.get();
    sc.up = [dp](int t, g3d::Scene& scene) { setDay(*dp, scene, t); };
    if (const char* path = std::getenv("RMTN_WD_THUNDER")) {   // the engine's own reading of every strike, to compare with the readout table
        const auto cues = g3d::thunderCues(sc, TOTAL, (float)FPS);
        if (FILE* lf = std::fopen(path, "w")) {
            for (const g3d::Thunder& c : cues) std::fprintf(lf, "strike %.2f s  distance %.1f m  arrives %.2f s  toGround %d\n", c.strikeTime, c.distance, c.arrives, c.toGround ? 1 : 0);
            std::fclose(lf);
        }
    }
    return d;
}

// ---------------------------------------------------------------- readings, from the scene at draw time
struct Reads {
    float wind = 0, hsCm = 0, visKm = 0, sunEl = 0, rainMmH = 0, wet = 0, bow = 0, bowSun = 0;
};
float fetchOf(const g3d::Scene& sc) {
    const Vector4 e = sc.waterBodies[0].extent();
    return std::max(e.z - e.x, e.w - e.y);
}
float rainRate(const g3d::Downfall& d) {                  // mm/h from the engine's own drops in one tile
    if (!d.enabled || d.intensity <= 0.001f) return 0;
    static float cache[101];
    static bool have[101] = {};
    const int q = (int)std::lround(std::clamp(d.intensity, 0.f, 1.f) * 100);
    if (have[q]) return cache[q];
    double flux = 0;
    const uint32_t n = d.dropsPerTile();
    for (uint32_t k = 0; k < n; k++) {
        const g3d::Downfall::Drop dr = d.drop(0, 0, 0, k, 0.0);
        if (dr.rank >= q / 100.f) continue;
        flux += (PI / 6.0) * std::pow((double)dr.diameter, 3) * std::fabs(dr.velocity.y);
    }
    const double vol = (double)g3d::Downfall::kTile * g3d::Downfall::kTile * g3d::Downfall::kCell;
    have[q] = true;
    return cache[q] = (float)(flux / vol * 1000.0 * 3600.0);
}
Reads readDay(const g3d::Scene& sc, double s) {
    Reads r;
    r.wind = sc.weather.wind.speed;
    r.hsCm = 100 * sc.weather.waveHeight(fetchOf(sc));
    r.visKm = 3.912f / std::max(sc.weather.fogDensity(), 1e-6f) / 1000.f;
    r.sunEl = std::asin(Vector3Normalize(sc.sky.sunDirection).y) * RAD2DEG;
    r.rainMmH = rainRate(g3d::inWeather(sc.precipitation, sc.weather));
    r.wet = g3d::groundOf(sc, s).wetness;
    r.bow = g3d::rainbowAngle(550, 1);
    r.bowSun = g3d::rainbowOf(sc, s).elevation;
    return r;
}
// What is left of a distant thing's own light: the aerial formula (docs/weather.md), green channel, along the ray.
float lightLeft(const g3d::AerialPerspective& a, float dist, float top) {
    double tau = 0;
    const int n = 48;
    for (int i = 0; i < n; i++) {
        const double u = (i + 0.5) / n, alt = CAM_Y + (top - CAM_Y) * u;
        tau += a.strength * (dist / n) / 30000.0 * std::exp(-alt / a.scaleHeight);
    }
    return (float)std::exp(-tau);
}

// ---------------------------------------------------------------- labels
std::string fixed(float v, int dp) { char b[32]; std::snprintf(b, sizeof b, "%.*f", dp, v); return b; }

struct Chapter { std::string title, note, code; };
void captionCard(const Ctx& cx, const Chapter& ch, float k) {
    if (k <= 0) return;
    const Ctx c = faded(cx, k);
    const float w = std::max({ MeasureTextEx(regular(), ch.code.c_str(), 26, 0).x, MeasureTextEx(bold(), ch.title.c_str(), 60, 0).x,
                               MeasureTextEx(regular(), ch.note.c_str(), 28, 0).x }) + 80;
    const float y = 1080 - 64 - 186 + 24 * (1 - k);
    dr::roundRect(c, 64, y, w, 186, 18, withAlpha(SCRIM, 0.72f));
    dr::text(bold(), c, ch.title.c_str(), { 104, y + 30 }, 60, 0, INK, 0, 0);
    dr::text(regular(), c, ch.note.c_str(), { 104, y + 100 }, 28, 0, SUB, 0, 0);
    dr::text(regular(), c, ch.code.c_str(), { 104, y + 142 }, 26, 0, AMBER, 0, 0);
}

// A gauge row at the top right: label, number, unit, and a bar filled to `fill` (0..1).
void gauge(const Ctx& cx, float k, int row, const char* label, const std::string& number, const char* unit, float fill) {
    if (k <= 0) return;
    const Ctx c = faded(cx, k);
    const float w = 380, x = 1920 - 64 - w, y = 64 + row * 112 - 12 * (1 - k);
    dr::roundRect(c, x, y, w, 100, 16, withAlpha(SCRIM, 0.72f));
    dr::text(regular(), c, label, { x + 20, y + 10 }, 24, 0, SUB, 0, 0);
    dr::text(bold(), c, number.c_str(), { x + 20, y + 34 }, 52, 0, TEAL, 0, 0);
    const float nw = MeasureTextEx(bold(), number.c_str(), 52, 0).x;
    dr::text(regular(), c, unit, { x + 30 + nw, y + 56 }, 26, 0, INK, 0, 0);
    dr::rect(c, x + 20, y + 88, w - 40, 4, withAlpha(INK, 0.2f));
    dr::rect(c, x + 20, y + 88, (w - 40) * std::clamp(fill, 0.f, 1.f), 4, TEAL);
}

void tag(const Ctx& cx, float k, Vector2 at, const std::string& l1, const std::string& l2, bool left = false, float lift = 0) {
    if (k <= 0) return;
    const Ctx c = faded(cx, k);
    const float w = std::max(MeasureTextEx(bold(), l1.c_str(), 34, 0).x, MeasureTextEx(regular(), l2.c_str(), 26, 0).x) + 36;
    const float x = std::clamp(left ? at.x - w - 20 : at.x + 20, 64.f, 1856.f - w), y = std::clamp(at.y - 40 - lift, 20.f, 960.f);
    dr::strokeSeg(c, at, { left ? x + w : x, y + 36 }, 2, AMBER);
    dr::circle(c, at, 6, AMBER);
    dr::roundRect(c, x, y, w, 76, 12, withAlpha(SCRIM, 0.8f));
    dr::text(bold(), c, l1.c_str(), { x + 18, y + 6 }, 34, 0, TEAL, 0, 0);
    dr::text(regular(), c, l2.c_str(), { x + 18, y + 42 }, 26, 0, INK, 0, 0);
}

bool proj(const g3d::Scene& sc, int t, Vector3 p, Vector2& out) {
    return g3d::project(g3d::cameraAt(sc.camera, t), p, { 0, 0, 1920, 1080 }, out);
}

// ---------------------------------------------------------------- the overlay: everything read at draw time
void overlay(const Ctx& cx, int t, const DayP& d) {
    const g3d::Scene& sc = *d->scene;
    const double s = t / 30.0;
    const Reads r = readDay(sc, s);
    // the clock: the hour that sets the sky
    {
        const int h = (int)d->hour, m = (int)((d->hour - h) * 60);
        char b[16];
        std::snprintf(b, sizeof b, "%02d:%02d", h, m);
        const float k = rampS(s, 0.6, 1.2) * (1 - rampS(s, 47.7, 48.3));
        const Ctx c = faded(cx, k);
        dr::roundRect(c, 64, 56, 250, 76, 14, withAlpha(SCRIM, 0.6f));
        dr::text(bold(), c, b, { 84, 62 }, 52, 0, INK, 0, 0);
        dr::text(regular(), c, "time-lapse", { 84 + 130, 82 }, 24, 0, SUB, 0, 0);
    }
    auto win = [&](double in, double out) { return rampS(s, in, in + 0.4) * (1 - rampS(s, out, out + 0.5)); };
    // chapter cards
    const Chapter ch[5] = {
        { "Dawn mist", "the sun low behind the trees: light in the haze, and ridges that pale with distance", "Scene::volumetric  ·  Scene::aerial" },
        { "Cumulus building", "one wind raises the lake, and the mirror breaks up", "Weather::wind  ·  Water::reflections" },
        { "A squall line", "rain, wet ground, a strike and its thunder delay", "Scene::precipitation  ·  Scene::ground  ·  Scene::lightning" },
        { "Clearing", "the sun behind you and rain in front of you", "Weather::rainbow" },
        { "Calm evening", "the same lake, a mirror again, and a boat's wake", "Scene::wakes  ·  Water::reflections" } };
    const double cardIn[5] = { 1.2, 8.9, 17.8, 33.2, 40.7 }, cardOut[5] = { 7.6, 16.7, 32.4, 39.8, 47.4 };
    for (int i = 0; i < 5; i++) captionCard(cx, ch[i], win(cardIn[i], cardOut[i]));
    // chapter 1
    gauge(cx, win(2.8, 7.6), 0, "visibility", fixed(r.visKm, 1), "km", r.visKm / 8);
    gauge(cx, win(2.8, 7.6), 1, "sun", fixed(r.sunEl, 0), "deg up", r.sunEl / 60);
    for (int i : { 0, 4 }) {
        const double in = i == 0 ? 4.4 : 5.6;
        const float k = win(in, 7.6);
        if (k <= 0) continue;
        const float zc = i == 0 ? -30.f : 0.f, x = RIDGE[i].x, y = groundAt(x, zc) + 4;
        const float dist = x - CAM_X, left = lightLeft(sc.aerial, dist, y);
        Vector2 p;
        if (proj(sc, t, { x, y, zc }, p))
            tag(cx, k, p, "ridge " + fixed(dist / 1000, 1) + " km", fixed(left * 100, 0) + "% of its light left", i == 4, i == 4 ? 70.f : 0.f);   // the far ridge's tag sits to the left and above, clear of the near one
    }
    // chapter 2
    gauge(cx, win(10.6, 16.7), 0, "wind", fixed(r.wind, 1), "m/s", r.wind / 16);
    gauge(cx, win(12.2, 16.7), 1, "waves, Hs", fixed(r.hsCm, 0), "cm", r.hsCm / 20);
    // chapter 3
    gauge(cx, win(22.4, 32.4), 0, "rain", fixed(r.rainMmH, 0), "mm/h", r.rainMmH / 120);
    gauge(cx, win(25.0, 32.4), 1, "ground wet", fixed(r.wet * 100, 0), "%", r.wet);
    int tagNo = 0;
    for (int i = 0; i < NSTRIKES; i++) {
        const StrikeAt& k = STRIKES[i];
        if (!k.tagged) continue;
        const int ti = tagNo++;
        const float dist = strikeDistance(k), delay = bolt::thunderDelay(dist), arrives = k.time + delay;
        const double in = k.time + 0.3, out = arrives + 0.9;
        const float a = rampS(s, in, in + 0.3) * (1 - rampS(s, out, out + 0.4));
        if (a <= 0) continue;
        Vector2 p;
        if (!proj(sc, t, k.foot, p)) continue;
        if (ti == 0) p.y -= 120;                         // the far strike keeps its tag clear of the later ones
        const float left = (float)(arrives - s);
        tag(cx, a, p, "strike " + (dist >= 1000 ? fixed(dist / 1000, 1) + " km" : fixed(dist, 0) + " m"),
            left > 0 ? "thunder in " + fixed(left, 1) + " s" : std::string("thunder now"), p.x > 1100);
    }
    // chapter 4: the bow's ring, from the same anti-solar point the engine uses
    {
        const float k = win(35.4, 39.8);
        const g3d::RainbowFrame rf = g3d::rainbowOf(sc, s);
        if (k > 0 && rf.strength > 0.01f) {
            const Vector3 a = Vector3Normalize(rf.antiSolar);
            Vector3 u = Vector3Normalize(Vector3CrossProduct(a, { 0, 1, 0 })), v = Vector3CrossProduct(a, u);
            Vector2 prev{ 0, 0 }, top{ 0, 1e9f };
            bool havePrev = false;
            const float ang = (r.bow + 3.f) * DEG2RAD;
            for (int i = 0; i <= 160; i++) {
                const float f = 2 * PI * i / 160;
                const Vector3 dir = Vector3Add(Vector3Scale(a, std::cos(ang)), Vector3Scale(Vector3Add(Vector3Scale(u, std::cos(f)), Vector3Scale(v, std::sin(f))), std::sin(ang)));
                Vector2 p;
                const bool ok = proj(sc, t, Vector3Add({ CAM_X, CAM_Y, 0 }, Vector3Scale(dir, 1000)), p);
                if (ok && p.y < top.y && p.x > 100 && p.x < 1500) top = p;
                (void)prev;
                prev = p;
                havePrev = ok;
            }
            if (top.y < 1e8f) tag(cx, k, top, "bow " + fixed(r.bow, 1) + " deg", "from the point opposite the sun, " + fixed(r.bowSun, 0) + " deg up");
        }
    }
    // chapter 5
    gauge(cx, win(42.4, 47.4), 0, "waves, Hs", fixed(r.hsCm, 1), "cm", r.hsCm / 20);
    {
        const float k = win(44.2, 47.4);
        Vector2 p, h;
        if (k > 0 && !sc.wakes.empty() && sc.wakes[0].at(s, &p, &h)) {
            const g3d::Wake& w = sc.wakes[0];
            const float fr = w.speed / std::sqrt(9.81f * w.length), half = std::asin(1.f / 3);
            Vector2 sp;
            if (proj(sc, t, { p.x, 0.1f, p.y }, sp)) {
                for (int side : { -1, 1 }) {
                    const float ca = std::cos(side * half), sa = std::sin(side * half);
                    const Vector2 back{ -h.x, -h.y }, dir{ back.x * ca - back.y * sa, back.x * sa + back.y * ca };
                    Vector2 q;
                    if (proj(sc, t, { p.x + dir.x * 25, 0.1f, p.y + dir.y * 25 }, q)) dr::strokeSeg(faded(cx, k * 0.8f), sp, q, 2, AMBER);
                }
                tag(cx, k, sp, "wake " + fixed(half * RAD2DEG, 1) + " deg", "boat " + fixed(w.speed, 1) + " m/s, Froude " + fixed(fr, 2), true, 150);
            }
        }
    }
}

// ---------------------------------------------------------------- the end card
struct Api { const char* call; const char* what; };
const Api API[] = {
    { "wx::applyBlend / Weather::mix", "one state for sky, wind, rain" },
    { "Scene::clouds", "volumetric clouds, anvils, shadows" },
    { "Scene::volumetric / aerial", "light in the haze, blue distance" },
    { "Scene::precipitation / ground", "rain, wet ground, puddle rings" },
    { "Scene::lightning, thunderCues", "strikes and the thunder delay" },
    { "Water::reflections, WaterBody", "a lake that mirrors the scene" },
    { "Scene::wakes, fx::lens", "a Kelvin wake; rain on the lens" },
};
void endCard(const Ctx& cx, int t) {            // t from the card's start (film second 47.7)
    const float scrim = sm(t, 0, 18);
    dr::rect(cx, 0, 0, 1920, 1080, withAlpha(mkColor(0x0c1016), 0.86f * scrim));
    const float k = rampS(t / 30.0, 0.6, 1.0);
    const Ctx c = faded(cx, k);
    const float w = MeasureTextEx(bold(), "ONE PLACE. ONE WEATHER.", 96, 0).x;
    dr::text(bold(), c, "ONE PLACE. ONE WEATHER.", { 960 - w / 2, 150 + 14 * (1 - k) }, 96, 0, INK, 0, 0);
    for (int i = 0; i < 7; i++) {
        const float a = rampS(t / 30.0, 1.5 + i * 0.2, 1.9 + i * 0.2);
        if (a <= 0) continue;
        const Ctx l = faded(cx, a);
        const float y = 340 + i * 74 + 10 * (1 - a);
        dr::text(code::font(false), l, API[i].call, { 330, y }, 36, 0, AMBER, 0, 0);
        dr::text(regular(), l, API[i].what, { 1010, y + 4 }, 32, 0, SUB, 0, 0);
    }
}

// ---------------------------------------------------------------- the sound
const std::string SOUND = "out/weather-day-assets/day.wav";
void genSound(const std::string& path) {
    using procgen::Score;
    Score s(END_S + 0.5);
    const int n = (int)(END_S * Score::RATE);
    procgen::Svf wind, rain, lowp;
    const std::vector<g3d::Weather::Key> hist = rainHistory();
    g3d::Weather rw;
    rw.history = hist;
    rw.precipitation.kind = g3d::Precipitation::Rain;
    for (int k = 0; k < n; k++) {
        const double t = (double)k / Score::RATE;
        const float fade = (float)std::min({ 1.0, t / 1.0, (END_S - t) / 2.0 });
        const float u = curve(t, WIND) / 14.f;
        const float gust = 0.7f + 0.3f * (float)std::sin(t * 0.9 + 1.7 * std::sin(t * 0.31));
        const float wv = wind.bp(s.noise(), 260 + 700 * u * gust, 0.6f, Score::RATE) * (0.13f + 0.03f * std::pow(u, 0.7f)) * gust;
        const float rv = rain.hp(s.noise(), 3000, 0.8f, Score::RATE) * 0.035f * rw.rainAt(t);
        s.put(k, (wv + rv) * std::max(0.f, fade), 0.5f, 0.2f, 0);
    }
    for (const StrikeAt& k : STRIKES) {                 // thunder at strike + distance / 343, quieter with distance
        const float dist = strikeDistance(k);
        const double at = k.time + bolt::thunderDelay(dist);
        const float amp = 1.15f / (1.f + dist / 1800.f);
        const float far = std::clamp(dist / 3000.f, 0.f, 1.f);       // far: a longer, lower rumble and no crack
        const float crack = 1.f - std::clamp((dist - 500.f) / 1500.f, 0.f, 1.f);   // near: a sharp crack
        const int a = (int)(at * Score::RATE), len = (int)(8.0 * Score::RATE);
        procgen::Svf snap;
        for (int i = 0; i < len; i++) {
            const double t = (double)i / Score::RATE;
            const double dec = 0.9 - 0.4 * far;
            const float env = (float)(std::min(1.0, t / (0.04 + 0.1 * far)) * (std::exp(-t * dec) * (0.6 + 0.4 * std::sin(t * 7 + k.seed)) + (t < 0.25 ? std::exp(-t * 14) : 0.0)));
            const float x = lowp.lp(s.noise(), 90 + 400 * (float)std::exp(-t * 1.5), 0.7f, Score::RATE);
            const float c = crack > 0 ? snap.bp(s.noise(), 1400, 0.7f, Score::RATE) * (float)std::exp(-t * 22) * crack * 0.9f : 0.f;
            s.put(a + i, (x * env * 1.3f + c) * amp, 0.5f, 0.35f, 0);
        }
    }
    for (int q : { 55, 62, 67 }) s.pad(CH[4], 7.0, q, 0.03f, 700, 0.5f);       // the evening
    for (int q : { 55, 62, 67 }) s.pad(CH[5] + 0.5, 6.0, q, 0.04f, 800, 0.5f);
    std::vector<float> mixed = s.mix();
    for (float& v : mixed) v = 0.7f * std::tanh(v * 1.3f / 0.7f);   // a soft limiter: the mix is normalised to 0.9; this holds it under -3 dBFS (true-peak room for the AAC encode) and lifts the quiet bed 2 dB
    media::writeWav(path, mixed, Score::RATE);
}

}  // namespace

void registerWeatherDay(Registry& out) {
    Comp c;
    c.name = "weather-day";
    c.w = 1920; c.h = 1080; c.fps = FPS;
    c.background = mkColor(0x070a10);
    c.duration = TOTAL;
    auto day = std::make_shared<DayP>();
    auto get = [day]() -> DayP& { if (!*day) *day = makeDay(); return *day; };
    TNode* root = addRoot(c.roots, 0, TOTAL);
    auto sceneNode = [&](int from, int to, bool lens) {
        TNode* n = root->add(from, to - from);
        n->fn = [get, from](const Ctx& cx, int t) { g3d::render(*const_cast<DayP&>(get()).get()->scene, t + from, cx); };
        if (lens) {
            fx::Lens l;
            l.rain.rateAt = [](double sec) { return 0.9f * curve(sec + LENS_T0, { { 0, 0 }, { 0.2, 0 }, { 1.6, 1 }, { 4.2, 1 }, { 6.0, 0.4f } }) * (float)rampS(sec + LENS_T0, 27.8, 29.0); };
            l.rain.blurBehind = 0;
            l.wiper.period = 0;
            l.wiper.from = -60; l.wiper.to = 60;
            n->isolate().filters.push_back(fx::lens(l));
        }
    };
    const int l0 = (int)std::lround(LENS_T0 * FPS), l1 = (int)std::lround(LENS_T1 * FPS);
    sceneNode(0, l0, false);
    sceneNode(l0, l1, true);
    sceneNode(l1, TOTAL, false);
    root->add(0, TOTAL)->fn = [get](const Ctx& cx, int t) { overlay(cx, t, get()); };
    const int e0 = (int)std::lround(47.7 * FPS);
    root->add(e0, TOTAL - e0)->fn = [](const Ctx& cx, int t) { endCard(cx, t); };
    TNode* logo = prefab::use(root, 1560, TOTAL - 1560, "raymotion-logo",
                              Json::parse(R"({"size": 56, "layout": "horizontal", "tagline": "video as code", "delay": 2, "ink": "#f1ede2", "accent": "#2fc4b2"})"));
    logo->props.pos = { 960 - 150, 930 };
    root->add(1560, TOTAL - 1560)->fn = [](const Ctx& cx, int t) {
        dr::text(code::font(false), faded(cx, rampS(t / 30.0, 0.2, 0.6)), "github.com/pebaryan/raymotion", { 960 - 30, 930 }, 26, 0, SUB, 0, 0.5f);
    };
    c.assets = [](const Comp&, const Json&) {
        if (std::filesystem::exists(SOUND)) return;
        std::filesystem::create_directories(std::filesystem::path(SOUND).parent_path());
        genSound(SOUND);
    };
    media::audio(addRoot(c.roots, 0, c.duration), SOUND);
    out.push_back(std::move(c));
}

}  // namespace rmtn
