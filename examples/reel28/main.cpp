// reel28/main.cpp -- "reel28oktober": the 28 October / Sumpah Pemuda reel.
//
// Nine shots on the audio cut grid (0/2/5/8/11/15/19/23/26/30 s at 30 fps), built from the
// 14 verified PBR prefabs in D:/code/reel-28oktober/assets and the three audio stems.
//
//   raymotion_reel28 list
//   raymotion_reel28 still reel28oktober --at 45 --png probe_45.png
//   raymotion_reel28 render reel28oktober
#include "rmtn/raymotion.hpp"
#include "rmtn/icons.hpp"
#include <map>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

namespace rmtn {
namespace {

constexpr int W = 1080, H = 1920, FPS = 30, TOTAL = 1080;

const Color BG = mkColor(0x0a0c14), INK = mkColor(0xf3f6ff), DIM = mkColor(0x9aa3bd),
            AQUA = mkColor(0x47dad8), GOLDC = mkColor(0xffc857), WARM = mkColor(0xffcf9a);

const std::string ASSETS = "D:/code/reel-28oktober/assets/";
const std::string AUDIO = "D:/code/reel-28oktober/audio/";

// ------------------------------------------------------------------ helpers

g3d::ModelPtr model(const char* id) {
    static std::map<std::string, g3d::ModelPtr> cache;
    auto it = cache.find(id);
    if (it != cache.end()) return it->second;
    g3d::ModelPtr m = g3d::loadModel(ASSETS + id + ".glb");
    cache[id] = m;
    return m;
}

unsigned hash(unsigned x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x;
}
float rnd(unsigned a, unsigned b = 0) { return (hash(a * 9781u + b * 6271u + 17u) & 0xffffff) / 16777216.f; }

float smooth01(float x) {
    x = std::clamp(x, 0.f, 1.f);
    return x * x * (3 - 2 * x);
}

// A floor + studio environment + one shadow-casting key light, in the reel's palette.
g3d::ScenePtr stage(Color bg, Color fog, float fogNear, float fogFar, Vector3 keyFrom, Color keyCol,
                    float keyI, Color floorCol) {
    auto s = std::make_shared<g3d::Scene>();
    s->environmentGenerator = g3d::studioEnvironment(mkColor(0x39486a), mkColor(0x1a2133), mkColor(0x08090f), 3);
    s->environmentIntensity = 0.85f;
    s->background = bg;
    s->fogColor = fog;
    s->fogNear = fogNear;
    s->fogFar = fogFar;
    s->ambient = 0.32f;
    g3d::Material m;
    m.color = floorCol;
    m.roughness = 0.85f;
    g3d::Object3D* f = s->add(g3d::Geometry::plane(60, 60), m);
    f->position = { 0, 0, 0 };
    s->lights.push_back(g3d::Light::directional(keyFrom, keyCol, keyI, true));
    s->lights.back().shadowExtent = 7;
    return s;
}

g3d::Object3D* put(g3d::ScenePtr& s, const char* id, Vector3 at, float yawDeg = 0, float scale = 1) {
    g3d::Object3D* o = s->add(model(id));
    if (const char* st = getenv("REEL_STYLE"); st && *st) {
        if (!strcmp(st, "cartoon")) {                     // flat light bands + rim, per npr.cpp ToonInk
            o->material.shading = g3d::Shading::Toon;
            o->material.toon.bands = 2;
            o->material.toon.rim = Color{ 255, 255, 255, 110 };
        } else if (!strcmp(st, "gooch")) {                // cool shadow / warm light
            o->material.shading = g3d::Shading::Gooch;
        }
    }
    o->position = at;
    o->rotation = { 0, yawDeg, 0 };
    o->scale = { scale, scale, scale };
    return o;
}

// ------------------------------------------------------------------ the nine shots

// A 9:16 frame is only 0.5625 m wide per metre of visible height, so every distance below is
// derived from the subject's real size in manifest.json rather than guessed.
float distFor(float visibleHeight, float fovDeg) {
    return visibleHeight / (2.f * std::tan(fovDeg * PI / 360.f));
}

// Walk/ride motion: a figure that arrives, with a small vertical bob and a lean into the move.
struct Move {
    Vector3 from, to;
    float yawFrom, yawTo;
    int t0, t1;          // frames the move runs over
    float bob = 0.02f;   // metres of vertical bob while travelling
    float lean = 0.f;    // degrees of tilt about z while travelling
};

void walk(g3d::Object3D* o, Move m) {
    o->up = [m](int t, g3d::Object3D& self) {
        const float k = smooth01((t - m.t0) / float(m.t1 - m.t0));
        const float moving = (t > m.t0 && t < m.t1) ? std::sin(k * PI) : 0.f;
        self.position.x = m.from.x + (m.to.x - m.from.x) * k;
        self.position.z = m.from.z + (m.to.z - m.from.z) * k;
        self.position.y = m.from.y + (m.to.y - m.from.y) * k +
                          m.bob * moving * std::sin(t * 0.55f);
        self.rotation.y = m.yawFrom + (m.yawTo - m.yawFrom) * k;
        self.rotation.z = m.lean * moving;
    };
}

g3d::Object3D* plinth(g3d::ScenePtr& s, float w, float h, Color col) {
    g3d::Material m; m.color = col; m.roughness = 0.9f;
    g3d::Object3D* o = s->add(g3d::Geometry::box(w, h, w), m);
    o->position = { 0, h * 0.5f, 0 };
    return o;
}

// 1. 1928: the 0.90 m torch standing on a stone plinth, sepia, slow push.
g3d::ScenePtr shotTorch() {
    auto s = stage(mkColor(0x0b0806), mkColor(0x2a1d12), 2.5f, 12.f, { -2.2f, 3.4f, -2.0f }, WARM, 1.3f,
                   mkColor(0x2a2118));
    s->ambient = 0.24f;
    plinth(s, 0.62f, 0.52f, mkColor(0x3b3229));
    put(s, "torch_1928", { 0, 0.52f, 0 }, 0, 1.25f);            // 1.13 m of torch above the stone
    s->lights.push_back(g3d::Light::point({ 0, 1.85f, 0 }, mkColor(0xffb45e), 3.0f, 2.8f));
    s->lights.push_back(g3d::Light::point({ 0.9f, 1.2f, 1.4f }, mkColor(0xffd9a8), 0.9f, 4.0f));
    s->lights[1].up = [](int t, g3d::Light& l) {          // flame flicker: three sines, as the campfire
        l.intensity = 2.2f + 0.55f * std::sin(t * 0.9f) + 0.35f * std::sin(t * 2.3f + 1.f) +
                      0.25f * std::sin(t * 5.1f);
    };
    fx::Emitter3D flame = fx::fire3d(0.095f);             // the burning body of the flame
    flame.seed = 290;
    s->add(flame)->position = { 0, 1.52f, 0 };
    fx::Emitter3D embers = fx::embers3d(0.13f);
    embers.rate = 1.2f; embers.seed = 281;
    s->add(embers)->position = { 0, 1.66f, 0 };
    fx::Emitter3D smoke = fx::smoke3d(0.07f);
    smoke.rate = 0.45f; smoke.seed = 282;
    s->add(smoke)->position = { 0, 1.86f, 0 };
    if (const char* st = getenv("REEL_STYLE"); st && *st) {
        if (!strcmp(st, "watercolor")) {                  // boiling ink contours for the wash
            s->lines.enabled = true;
            s->lines.color = mkColor(0x1c1a26);
            s->lines.width = 2.5f;
            s->lines.wobble = 1.6f;
            s->lines.boilEvery = 3;
        } else if (!strcmp(st, "cartoon")) {              // cel shading + boiling outlines
            s->lines.enabled = true;
            s->lines.color = mkColor(0x1c1a26);
            s->lines.width = 3.5f;
            s->lines.wobble = 1.6f;
            s->lines.boilEvery = 3;
        } else if (!strcmp(st, "gooch")) {                // technical-illustration line weight
            s->lines.enabled = true;
            s->lines.color = mkColor(0x1d2440);
            s->lines.width = 2.f;
        }
    }
    const float fov = 34.f;
    s->camera.fov = fov;
    const float d0 = distFor(2.45f, fov);
    s->camera.up = [d0](int t, g3d::Camera& cam) {
        const float k = smooth01(t / 60.f);
        cam.position = { 0.09f * std::sin(t * 0.02f), 1.10f, d0 - 0.40f * k };
        cam.target = { 0, 0.98f, 0 };
    };
    return s;
}

// 2. The skate beat: skater (1.75 m) in front of the quarterpipe.
g3d::ScenePtr shotSkate() {
    auto s = stage(mkColor(0x0e1726), mkColor(0x16233a), 5.5f, 24.f, { -4, 6, 4 }, mkColor(0xdfe9ff), 1.05f,
                   mkColor(0x1d2637));
    s->ambient = 0.45f;
    put(s, "skatepark_quarterpipe", { -0.30f, 0, -1.55f }, 8);
    g3d::Object3D* sk = put(s, "skater", { 0.42f, 0, 0.55f }, -20);
    g3d::Object3D* bd = put(s, "skateboard", { -0.05f, 0.10f, 0.62f }, -6);
    // Ride in, crouch, then one clean parabolic ollie: the board stays under the feet (no spin),
    // only pitching nose-up off the tail and levelling for the landing.
    auto arc = [](int t0, int t1, int t) -> float {                       // 0..1..0 parabola
        if (t <= t0 || t >= t1) return 0.f;
        const float u = (t - t0) / float(t1 - t0);
        return 4.f * u * (1.f - u);
    };
    sk->up = [arc](int t, g3d::Object3D& self) {
        const float k = smooth01(t / 44.f);
        const float crouch = (float)interpolate(t, { 44, 54, 62 }, { 0, -0.07, 0 }, clamped());
        const float air = arc(58, 88, t);
        const float ride = (t > 0 && t < 44) ? std::sin(k * PI) : 0.f;
        const float gx = 1.75f + (0.55f - 1.75f) * k;                     // ground path
        const float u2 = t < 58 ? 0.f : (t > 88 ? 1.f : (t - 58) / 30.f);
        const float ax = 0.55f + (0.42f - 0.55f) * u2;
        self.position = { t < 58 ? gx : ax, air * 0.55f + crouch, 0.30f + 0.22f * (t < 58 ? k : 1.f) };
        self.rotation = { -9.f * (float)interpolate(t, { 54, 62, 88 }, { 0, 1, 0.4f }, clamped()),
                          -28.f + 22.f * k, -5.f * ride };
        self.scale = { 1, 1, 1 };
    };
    bd->up = [arc](int t, g3d::Object3D& self) {
        const float k = smooth01(t / 44.f);
        const float air = arc(58, 88, t);
        const float gx = 1.90f + (0.55f - 1.90f) * k;
        const float u2 = t < 58 ? 0.f : (t > 88 ? 1.f : (t - 58) / 30.f);
        const float ax = 0.55f + (0.42f - 0.55f) * u2;
        self.position = { t < 58 ? gx : ax, 0.02f + air * 0.50f, 0.30f + 0.26f * (t < 58 ? k : 1.f) };
        self.rotation = { (float)interpolate(t, { 58, 66, 88 }, { -14.f, -4.f, 0.f }, clamped()), -8.f, 0.f };
    };
    s->lights.push_back(g3d::Light::point({ 2.4f, 2.4f, 2.0f }, mkColor(0xbfd6ff), 1.1f, 8.0f));
    // Parented to the board, firing on the landing frame: the board is at his final spot by then,
    // so the puff appears under the wheels instead of where he started.
    // On the ground, not on the board: a child emitter would lift with the ollie, so these stay in
    // world space at the board's final footprint (x 0.42, z 0.52).
    fx::Emitter3D dust = fx::debris3d(90);
    dust.burstAt = 88; dust.spread = 11; dust.seed = 284;
    dust.colors = { mkColor(0xc6ccd6), mkColor(0x98a2b0), mkColor(0xe2e6ee) };
    dust.size = { 0.06f, 0.16f };
    dust.speed = { 0.012f, 0.05f };
    dust.life = { 34, 60 };
    s->add(dust)->position = { 0.42f, 0.04f, 0.50f };
    fx::Emitter3D push = fx::debris3d(30);        // and a smaller scuff at take-off
    push.burstAt = 40; push.spread = 7; push.seed = 294;
    push.colors = { mkColor(0xa8b0bc), mkColor(0xd0d5dd) };
    push.size = { 0.03f, 0.09f };
    push.life = { 20, 36 };
    s->add(push)->position = { 0.60f, 0.04f, 0.46f };
    const float fov = 40.f;
    s->camera.fov = fov;
    const float d0 = distFor(3.55f, fov);          // covers 1.75 m skater + 0.55 m ollie
    s->camera.up = [d0](int t, g3d::Camera& cam) {
        const float k = smooth01(t / 90.f);
        const float a = 0.20f + 0.30f * k;
        const float d = d0 - 0.30f * k;
        cam.position = { d * std::sin(a), 1.62f, d * std::cos(a) };
        cam.target = { 0.0f, 1.20f, -0.55f };
    };
    return s;
}

// 3a. The split, left: the torch, closer. Panels are 540x1920, so the fov has to open up.
g3d::ScenePtr shotTorchStatic() {
    auto s = stage(mkColor(0x0b0806), mkColor(0x2a1d12), 2.5f, 12.f, { -2.2f, 3.4f, -2.0f }, WARM, 1.25f,
                   mkColor(0x2a2118));
    s->ambient = 0.26f;
    plinth(s, 0.48f, 0.42f, mkColor(0x3b3229));
    put(s, "torch_1928", { 0, 0.42f, 0 }, 0, 1.15f);
    s->lights.push_back(g3d::Light::point({ 0, 1.60f, 0 }, mkColor(0xffb45e), 2.8f, 2.6f));
    s->lights[1].up = [](int t, g3d::Light& l) {
        l.intensity = 2.2f + 0.5f * std::sin(t * 0.85f) + 0.3f * std::sin(t * 2.1f);
    };
    fx::Emitter3D flame = fx::fire3d(0.072f);
    flame.seed = 291;
    s->add(flame)->position = { 0, 1.34f, 0 };
    fx::Emitter3D embers = fx::embers3d(0.10f);
    embers.rate = 0.9f; embers.seed = 283;
    s->add(embers)->position = { 0, 1.44f, 0 };
    const float fov = 58.f;
    s->camera.fov = fov;
    s->camera.position = { 0.06f, 1.30f, distFor(1.92f, fov) };
    s->camera.target = { 0, 1.00f, 0 };
    return s;
}

// 3b. The split, right: the desk with the phone, close.
g3d::ScenePtr shotDesk() {
    auto s = stage(mkColor(0x0e1524), mkColor(0x18223a), 4.5f, 18.f, { -2.5f, 5, 3.5f }, mkColor(0xcfe0ff), 1.1f,
                   mkColor(0x1b2436));
    s->ambient = 0.5f;
    put(s, "student_desk", { 0, 0, 0 });
    put(s, "smartphone", { 0.02f, 0.78f, 0.02f }, 6);
    s->lights.push_back(g3d::Light::point({ 0.6f, 1.5f, 1.2f }, mkColor(0xbfd6ff), 1.2f, 5.0f));
    const float fov = 58.f;
    s->camera.fov = fov;
    s->camera.position = { 0.55f, 1.02f, distFor(1.15f, fov) };
    s->camera.target = { 0.0f, 0.72f, 0 };
    return s;
}

// 4. Two people, one frame: 0.72 m wide each at +-0.45 m, so the frame needs 3.0 m of height.
g3d::ScenePtr shotMeet() {
    auto s = stage(mkColor(0x121a28), mkColor(0x1a2436), 6.f, 26.f, { 4, 6, 5 }, mkColor(0xffe6cf), 1.1f,
                   mkColor(0x222a38));
    s->ambient = 0.42f;
    g3d::Object3D* a = put(s, "skater", { -0.45f, 0, 0.1f }, 90);
    g3d::Object3D* b = put(s, "barista", { 0.45f, 0, 0.1f }, -90);
    walk(a, Move{ { -1.85f, 0, 0.35f }, { -0.32f, 0, 0.10f }, 74.f, 68.f, 0, 58, 0.028f, 3.f });
    walk(b, Move{ {  1.85f, 0, 0.35f }, {  0.32f, 0, 0.10f }, -74.f, -68.f, 6, 64, 0.028f, -3.f });
    s->lights.push_back(g3d::Light::point({ 0, 2.4f, -1.6f }, mkColor(0x9fd8ff), 1.8f, 6.0f));
    s->lights.push_back(g3d::Light::point({ 0, 1.9f, 2.4f }, mkColor(0xffd8b8), 1.9f, 6.0f));
    const float fov = 38.f;
    s->camera.fov = fov;
    const float d0 = distFor(3.75f, fov);          // two 1.75 m figures plus headroom for the walk-in
    s->camera.up = [d0](int t, g3d::Camera& cam) {
        const float k = smooth01(t / 90.f);
        cam.position = { 0.05f, 1.50f, d0 - 0.40f * k };
        cam.target = { 0, 1.10f, 0 };
    };
    return s;
}

// 5. Warung kopi: barista at the counter, stall (2.5 x 2.2 m) behind, warm bulb.
g3d::ScenePtr shotWarung() {
    auto s = stage(mkColor(0x141009), mkColor(0x2a2016), 4.5f, 18.f, { -3.5f, 5.5f, 3.5f }, mkColor(0xffd9a8), 1.2f,
                   mkColor(0x2c2418));
    s->ambient = 0.40f;
    put(s, "warung_kopi_stall", { 0, 0, -1.15f });
    g3d::Object3D* ba = put(s, "barista", { -0.62f, 0, 0.26f }, 26);   // aside: never in front of the pour
    g3d::Object3D* kettle = put(s, "coffee_kettle", { 0.06f, 1.05f, 0.16f }, 15);
    put(s, "coffee_cup", { 0.42f, 1.05f, 0.20f }, 0);
    ba->up = [](int t, g3d::Object3D& self) {          // settles, then leans over the pour
        const float lean = (float)interpolate(t, { 34, 52, 92, 108 }, { 0, -8, -8, 0 }, clamped());
        self.rotation = { lean, 26, 0 };            // keeps facing the vessels while leaning
        self.position.y = 0.012f * std::sin(t * 0.09f);
    };
    kettle->up = [](int t, g3d::Object3D& self) {      // tips towards the cup and back
        const float tip = (float)interpolate(t, { 30, 48, 96, 112 }, { 0, -34, -34, 0 }, clamped());
        self.rotation = { 0, 15, tip };
    };
    s->lights.push_back(g3d::Light::point({ 0, 2.15f, 0.10f }, mkColor(0xffc27a), 2.2f, 4.2f));
    s->lights.push_back(g3d::Light::point({ 0, 1.55f, 1.05f }, mkColor(0xffd8a8), 2.0f, 4.5f));  // fill the stall front
    s->lights.push_back(g3d::Light::point({ 0, 1.15f, -1.05f }, mkColor(0xffb070), 2.2f, 3.0f));  // inside
    fx::Emitter3D steam = fx::smoke3d(0.018f);
    steam.rate = 0.90f; steam.seed = 285;
    steam.size = { 0.008f, 0.022f };
    steam.life = { 24, 44 };
    s->add(steam)->position = { 0.06f, 1.22f, 0.16f };           // kettle spout
    fx::Emitter3D cupSteam = fx::smoke3d(0.014f);
    cupSteam.rate = 0.60f; cupSteam.seed = 286;
    cupSteam.size = { 0.006f, 0.016f };
    cupSteam.life = { 20, 36 };
    s->add(cupSteam)->position = { 0.42f, 1.10f, 0.20f };        // the cup
    const float fov = 40.f;
    s->camera.fov = fov;
    const float d0 = distFor(2.95f, fov);
    s->camera.up = [d0](int t, g3d::Camera& cam) {
        const float k = smooth01(t / 120.f);
        const float a = 0.10f - 0.14f * k;
        const float d = d0 + 0.45f - 0.30f * k;
        cam.position = { d * std::sin(a), 1.80f, d * std::cos(a) };
        cam.target = { -0.12f, 1.14f, -0.55f };
    };
    return s;
}

// 6. Campus: the desk and the student, three-quarter.
g3d::ScenePtr shotCampus() {
    auto s = stage(mkColor(0x0f1626), mkColor(0x1a2438), 6.0f, 24.f, { -4, 6.5f, 4.5f }, mkColor(0xd8e6ff), 1.05f,
                   mkColor(0x1e2839));
    s->ambient = 0.48f;
    put(s, "student_desk", { 0.10f, 0, 0 });
    put(s, "smartphone", { -0.12f, 0.78f, 0.05f }, -18);
    g3d::Object3D* st = put(s, "barista", { -0.70f, 0, -0.30f }, 12);
    walk(st, Move{ { -2.15f, 0, -0.95f }, { -0.70f, 0, -0.30f }, -58.f, 12.f, 0, 52, 0.03f, 4.f });
    st->up = [](int t, g3d::Object3D& self) {
        const float k = smooth01((t - 0) / 52.f);
        const float bend = (float)interpolate(t, { 56, 74 }, { 0, -9 }, clamped());
        const float moving = (t > 0 && t < 52) ? std::sin(k * PI) : 0.f;
        self.position = { -2.15f + 1.45f * k, 0.03f * moving * std::sin(t * 0.55f), -0.95f + 0.65f * k };
        self.rotation = { bend, -58.f + 70.f * k, 4.f * moving };
    };
    s->lights.push_back(g3d::Light::point({ 1.4f, 1.9f, 1.6f }, mkColor(0xbfd6ff), 1.3f, 6.0f));
    const float fov = 40.f;
    s->camera.fov = fov;
    const float d0 = distFor(3.05f, fov);
    s->camera.up = [d0](int t, g3d::Camera& cam) {
        const float k = smooth01(t / 120.f);
        const float a = 0.30f - 0.24f * k;
        const float d = d0 - 0.30f * k;
        cam.position = { d * std::sin(a), 1.45f, d * std::cos(a) };
        cam.target = { -0.35f, 1.05f, -0.35f };
    };
    return s;
}

// 7. Esports: the desk from a low angle, lit.
g3d::ScenePtr shotEsports() {
    auto s = stage(mkColor(0x0b0d18), mkColor(0x171a2c), 5.0f, 20.f, { -3, 4, 4 }, mkColor(0xc9d8ff), 0.9f,
                   mkColor(0x181d2e));
    s->ambient = 0.70f;
    put(s, "esports_desk", { 0, 0, 0 });
    g3d::Object3D* phone = put(s, "smartphone", { 0.10f, 0.76f, 0.02f }, 8);
    phone->up = [](int t, g3d::Object3D& self) {
        self.rotation = { -18.f + 6.f * std::sin(t * 0.11f), 8.f + 14.f * std::sin(t * 0.05f), 0 };
        self.position.y = 0.76f + 0.004f * std::sin(t * 0.2f);
    };
    s->lights.push_back(g3d::Light::point({ 0.8f, 0.55f, -1.1f }, mkColor(0xff5fa8), 3.0f, 4.5f));
    s->lights.push_back(g3d::Light::point({ 0.0f, 2.1f, 1.6f }, mkColor(0xcfe0ff), 3.0f, 7.0f));
    s->lights.push_back(g3d::Light::point({ 0.0f, 0.95f, 1.5f }, mkColor(0xdfe9ff), 2.4f, 5.0f));
    const float fov = 42.f;
    s->camera.fov = fov;
    const float d0 = distFor(1.75f, fov);
    s->camera.up = [d0](int t, g3d::Camera& cam) {
        const float k = smooth01(t / 120.f);
        const float a = -0.24f + 0.44f * k;
        const float d = d0 - 0.25f * k;
        cam.position = { d * std::sin(a), 0.80f + 0.20f * k, d * std::cos(a) };
        cam.target = { 0.05f, 0.80f, -0.05f };
    };
    return s;
}

// 8. The street: the scooter (1.86 m long) three-quarter, helmet on the seat.
g3d::ScenePtr shotScooter() {
    auto s = stage(mkColor(0x0a1018), mkColor(0x152030), 7.0f, 26.f, { 5, 5, -3 }, mkColor(0xffd9b0), 1.05f,
                   mkColor(0x1a2130));
    s->ambient = getenv("REEL_STYLE") ? 0.58f : 0.42f;
    // Street and city, baked from the CityJSON kit to plain GLBs (tools/city2glb.py) so they go
    // through the same model path as every other prefab.
    put(s, "city/city_road", { 0, 0.015f, 0 });
    put(s, "city/city_shelter", { 1.7f, 0, -3.6f });
    put(s, "city/city_streetlight", { -0.9f, 0, -3.1f });
    put(s, "city/city_streetlight", { 3.6f, 0, -3.3f });
    put(s, "city/city_tree", { -3.2f, 0, -3.8f });
    s->lights.push_back(g3d::Light::point({ 1.7f, 2.6f, -3.4f }, mkColor(0xffd9a0), 1.8f, 6.0f));  // shelter lamp
    s->lights.push_back(g3d::Light::point({ -0.9f, 3.4f, -2.9f }, mkColor(0xffcf90), 2.2f, 7.0f)); // street light
    // The beat is a scooter PASSING ("from city to city"), not a parked prop: it travels through the
    // frame, so the helmet rides on the seat and the exhaust trails as children of the scooter.
    g3d::Object3D* scoot = put(s, "ojek_scooter", { 0, 0, 0 }, 0);   // length is on X: yaw 0 = profile
    scoot->add(model("helmet"))->position = { -0.30f, 0.56f, 0.04f };   // measured seat top in frame

    fx::Emitter3D exhaust = fx::smoke3d(0.05f);
    exhaust.rate = 1.1f; exhaust.seed = 287;
    exhaust.size = { 0.008f, 0.026f };
    exhaust.life = { 18, 38 };
    exhaust.colors = { mkColor(0x9aa6b8), mkColor(0x6f7b8c) };
    scoot->add(exhaust)->position = { -0.88f, 0.40f, -0.02f };   // behind the tail (local -X)

    scoot->up = [](int t, g3d::Object3D& o) {
        const float u = smooth01(t / 90.f);
        const float moving = (t > 2 && t < 88) ? std::sin(u * PI) : 0.f;
        o.position = { -2.30f + 4.30f * u, 0.012f * moving * std::sin(t * 0.7f), 0 };
        o.rotation = { 0, 0.f, 3.5f * moving };                   // rides along +X, leaning forward
    };

    s->lights.push_back(g3d::Light::point({ 1.6f, 1.4f, 1.8f }, mkColor(0xffd9b0), 2.2f, 7.0f));
    s->lights.push_back(g3d::Light::point({ -0.8f, 1.0f, 1.4f }, mkColor(0x9fc8ff), 1.6f, 5.0f));

    const float fov = 42.f;
    s->camera.fov = fov;
    const float d0 = distFor(3.40f, fov);
    s->camera.up = [d0](int t, g3d::Camera& cam) {
        const float u = smooth01(t / 90.f);
        const float sx = -2.30f + 4.30f * u;
        cam.position = { sx * 0.62f, 1.05f, d0 };                // mostly fixed: the scooter crosses
        cam.target = { sx * 0.70f, 0.78f, 0 };
    };
    return s;
}

// 9. The closer: torch between two people; impact at 29 s.
// ---- closing card: night field with drifting embers behind the wordmark text node
g3d::ScenePtr shotCredits() {
    auto s = stage(mkColor(0x05060c), mkColor(0x120a1c), 3.0f, 14.f, { -1.4f, 3.0f, -1.6f }, WARM, 1.1f,
                   mkColor(0x16121f));
    s->ambient = 0.22f;
    s->lights.push_back(g3d::Light::point({ 0, 2.2f, 1.2f }, mkColor(0xffc857), 0.8f, 5.0f));
    for (int i = 0; i < 3; i++) {                       // ember columns at three depths
        fx::Emitter3D e = fx::embers3d(0.09f + i * 0.02f);
        e.rate = 0.55f + i * 0.2f; e.seed = 300 + i;
        s->add(e)->position = { -0.8f + i * 0.8f, 0.4f + i * 0.3f, -0.6f + i * 0.5f };
    }
    s->camera.fov = 34;
    s->camera.up = [](int t, g3d::Camera& cam) {
        const float u = (float)interpolate(t, { 0, 180 }, { 0, 1 }, clamped(ease::inOut(ease::sine)));
        cam.position = { 0.15f * (1 - u) - 0.15f * u, 1.15f + 0.25f * u, 4.3f - 0.35f * u };
        cam.target = { 0, 1.35f + 0.1f * u, 0 };
    };
    return s;
}

g3d::ScenePtr shotCloser() {
    auto s = stage(mkColor(0x0c0a10), mkColor(0x241d26), 6.0f, 22.f, { 0, 6.5f, 5 }, mkColor(0xfff0dc), 1.2f,
                   mkColor(0x24202c));
    s->ambient = 0.44f;
    put(s, "torch_1928", { 0, 0, -0.30f }, 0, 1.30f);           // 1.17 m of torch, at head height
    g3d::Object3D* pa = put(s, "skater", { -0.40f, 0, 0.55f }, 8);
    g3d::Object3D* pb = put(s, "barista", { 0.40f, 0, 0.55f }, -8);
    walk(pa, Move{ { -1.55f, 0, 0.95f }, { -0.40f, 0, 0.55f }, 14.f, 2.f, 0, 46, 0.03f, 3.f });
    walk(pb, Move{ {  1.55f, 0, 0.95f }, {  0.40f, 0, 0.55f }, -14.f, -2.f, 5, 51, 0.03f, -3.f });
    s->lights.push_back(g3d::Light::point({ 0, 1.55f, -0.30f }, mkColor(0xffb45e), 2.6f, 3.4f));
    s->lights.back().up = [](int t, g3d::Light& l) {
        l.intensity = 2.1f + 0.5f * std::sin(t * 0.9f) + 0.3f * std::sin(t * 2.4f + 0.7f);
    };
    g3d::Light flash = g3d::Light::point({ 0, 1.8f, 1.6f }, mkColor(0xfff4e0), 0.0f, 10.0f);
    flash.up = [](int t, g3d::Light& l) {
        const float k = (float)interpolate(t, { 88, 92, 116 }, { 0, 2.6, 0 }, clamped());
        l.intensity = 2.2f + k;
    };
    s->lights.push_back(flash);
    fx::Emitter3D flame = fx::fire3d(0.10f);
    flame.seed = 292;
    s->add(flame)->position = { 0, 1.05f, -0.30f };
    fx::Emitter3D embers = fx::embers3d(0.12f);
    embers.rate = 1.2f; embers.seed = 288;
    s->add(embers)->position = { 0, 1.16f, -0.30f };
    fx::Emitter3D hit = fx::burst3d(150, { mkColor(0xffd08a), mkColor(0xff8a3c), mkColor(0xfff0c8) });
    hit.burstAt = 90; hit.spread = 5; hit.seed = 289;
    hit.speed = { 0.02f, 0.075f };
    hit.size = { 0.015f, 0.045f };
    hit.gravity = { 0, -0.004f, 0 };
    s->add(hit)->position = { 0, 1.05f, -0.30f };
    const float fov = 46.f;
    s->camera.fov = fov;
    const float d0 = distFor(3.95f, fov);
    s->camera.up = [d0](int t, g3d::Camera& cam) {
        const float k = smooth01(t / 120.f);
        const float punch = (float)interpolate(t, { 88, 92, 112 }, { 0, 0.40, 0 }, clamped());
        cam.position = { 0, 1.50f + 0.04f * punch, d0 - 0.55f * k - 0.30f * punch };
        cam.target = { 0, 1.08f, 0 };
        cam.dof = { d0 - 0.9f, 0.5f, 9 };
    };
    return s;
}

// ------------------------------------------------------------------ 2D overlays

struct Caption {
    int from, to;
    const char* id;
    const char* en;
};

const Caption CAPTIONS[] = {
    {   0,  60, "Sumpah Pemuda 1928",          "The 1928 Youth Pledge" },
    {  60, 150, "Dari 1928 ke hari ini",       "From 1928 to today" },
    { 150, 240, "Api yang sama",               "The same fire" },
    { 240, 330, "Satu nusa, satu bangsa",      "One homeland, one nation" },
    { 330, 450, "Ngopi, ngobrol, nyambung",    "Coffee, talk, connection" },
    { 450, 570, "Belajar bareng",              "Learning together" },
    { 570, 690, "Main, menang, bareng",        "Play, win, together" },
    { 690, 780, "Dari kota ke kota",           "From city to city" },
    { 780, 900, "Satu Nusa, Satu Bangsa, Satu Bahasa", "One homeland, one nation, one language" },
};

void captionLayer(const Ctx& cx, int t) {
    for (const Caption& c : CAPTIONS) {
        if (t < c.from || t >= c.to) continue;
        const float in = smooth01((t - c.from) / 10.f);
        const float out = 1.f - smooth01((t - (c.to - 8)) / 8.f);
        const float a = std::min(in, out);
        if (a <= 0.001f) continue;

        // bottom scrim: a stack of thin bands, so the copy always sits on its own darkness
        for (int i = 0; i < 26; i++) {
            const float y = H - 360.f + i * 14.f;
            dr::rect(cx, 0, y, (float)W, 15.f, withAlpha(BLACK, 0.42f * a * (i / 25.f)));
        }
        const float lift = 26.f * (1.f - in);
        dr::text(fonts::ui(true), cx, c.id, { 72, H - 176.f + lift }, 62, 0, withAlpha(INK, a), 0, 0.5f);
        dr::rect(cx, 72, H - 132.f + lift, 96, 5, withAlpha(GOLDC, a));
        dr::text(fonts::ui(false), cx, c.en, { 72, H - 106.f + lift }, 30, 0, withAlpha(DIM, a), 0, 0.5f);
        break;
    }
}

// Archival grain + vignette, only over the frames it belongs to.
void grainLayer(const Ctx& cx, int t) {
    if (t >= 240) return;                       // shots 1-3 only
    for (unsigned i = 0; i < 900; i++) {
        const float x = rnd(i, 11) * W, y = rnd(i, 12) * H;
        const float a = 0.03f + 0.07f * rnd(i, (unsigned)(t / 2) + 13);
        dr::rect(cx, x, y, 1.6f, 1.6f, withAlpha(WHITE, a));
    }
    for (int i = 0; i < 40; i++) {               // a soft top/bottom vignette
        const float k = i / 39.f;
        dr::rect(cx, 0, 0, (float)W, 8.f, withAlpha(BLACK, 0.055f * (1 - k)));
        dr::rect(cx, 0, H - 8.f - i * 8.f, (float)W, 8.f, withAlpha(BLACK, 0.055f * (1 - k)));
    }
}

} // namespace

void registerReel28(Registry& out) {
    Comp comp;
    comp.name = "reel28oktober";
    comp.w = W;
    comp.h = H;
    comp.fps = FPS;
    comp.duration = TOTAL;
    comp.background = BG;

    const Rectangle full{ 0, 0, (float)W, (float)H };
    const Rectangle left{ 0, 0, W / 2.f, (float)H };
    const Rectangle right{ W / 2.f, 0, W / 2.f, (float)H };

    struct Shot {
        int at, len;
        g3d::ScenePtr scene;
    };
    const Shot shots[] = {
        {   0,  60, shotTorch()   },
        {  60,  90, shotSkate()   },
        { 150,  90, nullptr       },   // the split, two scenes in two viewports
        { 240,  90, shotMeet()    },
        { 330, 120, shotWarung()  },
        { 450, 120, shotCampus()  },
        { 570, 120, shotEsports() },
        { 690,  90, shotScooter() },
        { 780, 120, shotCloser()  },
        { 900, 180, shotCredits() },
    };

    for (const Shot& s : shots) {
        TNode* n = addRoot(comp.roots, s.at, s.len);
        if (s.scene) n->fn = g3d::draw(s.scene, full);
    }

    // the split: warm torch on the left, cool desk on the right, plus the hairline between
    {
        g3d::ScenePtr a = shotTorchStatic(), b = shotDesk();
        TNode* n = addRoot(comp.roots, 150, 90);
        n->add(0, 90)->fn = g3d::draw(a, left);
        n->add(0, 90)->fn = g3d::draw(b, right);
        n->add(0, 90)->fn = [](const Ctx& cx, int) {
            dr::rect(cx, W / 2.f - 1.5f, 0, 3, (float)H, withAlpha(GOLDC, 0.55f));
        };
    }

    TNode* overlay = addRoot(comp.roots, 0, TOTAL);
    overlay->add(0, TOTAL)->fn = [](const Ctx& cx, int t) { grainLayer(cx, t); };
    overlay->add(0, TOTAL)->fn = [](const Ctx& cx, int t) { captionLayer(cx, t); };

    // ---- closing wordmark: "MADE BY / RAYMOTION / animated prefab", staggered in, fade to black
    TNode* cr = addRoot(comp.roots, 930, 150);
    cr->fn = [](const Ctx& cx, int t) {
        const float in = (float)interpolate(t, { 0, 16 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        const float dark = (float)interpolate(t, { 96, 120 }, { 0, 1 }, clamped(ease::in(ease::cubic)));
        const Font fb = fonts::ui(true), fr = fonts::ui(false);
        const float cxm = W / 2.f;
        auto fade = [](Color base, int t0, int t1, int tt) {
            return withAlpha(base, (float)interpolate(tt, { (double)t0, (double)t1 }, { 0, 1 },
                                                      clamped(ease::out(ease::cubic))));
        };
        dr::text(fb, cx, "MADE BY", { cxm, 806 }, 36, 3, fade(mkColor(0x9aa3bd), 2, 14, t), 0.5f, 0.5f);
        dr::text(fb, cx, "RAYMOTION", { cxm, 890 }, 116, 2, fade(INK, 6, 22, t), 0.5f, 0.5f);
        dr::rect(cx, cxm - 150 * in, 966, 300 * in, 4, fade(mkColor(0xffc857), 14, 26, t));
        dr::text(fr, cx, "animated prefab", { cxm, 1016 }, 46, 1, fade(mkColor(0xcfe0ff), 18, 32, t), 0.5f, 0.5f);
        // the AGBPMN creative team: agent role and the model that ran it
        struct Team { const char* role; const char* model; };
        const Team team[7] = {
            { "Creative Director", "Qwen3.5 9B" },
            { "Writer", "Qwen3.5 9B" },
            { "Art Production", "Codex · ChatGPT 6 Sol" },
            { "Animation & Edit", "Hermes · GLM" },
            { "Score", "YuE2" },
            { "Editor", "Qwen3.5 9B" },
            { "Social", "Qwen3.5 9B" },
        };
        for (int i = 0; i < 7; i++) {
            const float a = (float)interpolate(t, { 22.0 + i * 2, 34.0 + i * 2 }, { 0, 1 },
                                               clamped(ease::out(ease::cubic)));
            const float y = 1130.f + i * 46.f;
            dr::text(fb, cx, team[i].role, { cxm - 40, y }, 30, 0,
                     fade(mkColor(0xdfe6ff), 22 + i * 2, 34 + i * 2, t), 1.0f, 0.5f);
            dr::text(fr, cx, team[i].model, { cxm + 40, y }, 30, 0,
                     fade(mkColor(0x9aa3bd), 22 + i * 2, 34 + i * 2, t), 0.0f, 0.5f);
        }
        // social badges grow in and their marks write themselves (icons prefab API)
        icons::Look xm{ icons::Style::Mono };
        xm.color = mkColor(0xe8edff);
        const float by = 1698.f;
        icons::draw(cx, "x", { cxm - 206, by }, 42, xm,
                    (float)interpolate(t, { 34.0, 46.0 }, { 0, 1 }, clamped(ease::out(ease::cubic))));
        dr::text(fr, cx, "@pebaryan", { cxm - 177, by }, 32, 0,
                 fade(mkColor(0xbfc9e2), 38, 48, t), 0.f, 0.5f);
        icons::draw(cx, "youtube", { cxm + 45, by }, 42, {},
                    (float)interpolate(t, { 38.0, 50.0 }, { 0, 1 }, clamped(ease::out(ease::cubic))));
        dr::text(fr, cx, "@pebaryan", { cxm + 74, by }, 32, 0,
                 fade(mkColor(0xbfc9e2), 42, 52, t), 0.f, 0.5f);
        dr::rect(cx, 0, 0, (float)W, (float)H, withAlpha(BLACK, dark));
        if (t < 18) dr::rect(cx, 0, 0, (float)W, (float)H, withAlpha(BLACK, 1 - in));
    };

    // ------------------------------------------------------------ sound
    TNode* score = addRoot(comp.roots, 0, TOTAL);
    media::AudioOptions so; so.volume = 0.62f;
    media::audio(score, AUDIO + "score.wav", so);

    TNode* sfx = addRoot(comp.roots, 0, TOTAL);
    media::AudioOptions xo; xo.volume = 0.95f;
    media::audio(sfx, AUDIO + "sfx.wav", xo);

    TNode* vo = addRoot(comp.roots, 786, 108);     // 26.2 s: three exclamations landing on the 29 s hit
    media::AudioOptions vo_o; vo_o.volume = 1.0f;
    media::audio(vo, AUDIO + "vo.wav", vo_o);

    const char* style = getenv("REEL_STYLE");
    if (style && *style) {
        std::vector<Filter> filters;
        if (!strcmp(style, "poster"))
            filters = { Filter::posterize(5), Filter::inkEdges(3.f, mkColor(0x1c1a26), 0.18f) };
        else if (!strcmp(style, "watercolor"))
            filters = { Filter::watercolor(9, 0.8f, 0.6f), Filter::paperGrain(0.5f) };
        int styled = 0;
        for (auto& r : comp.roots)
            if (r.get() != overlay && r.get() != cr) { r->isolate().filters = filters; ++styled; }   // captions layer stays clean
        fprintf(stderr, "[style] %s on %d roots (captions excluded)\n", style, styled);
    }
    out.push_back(std::move(comp));
}

} // namespace rmtn


int main(int argc, const char** argv) { return rmtn::runCli(argc, argv, rmtn::registerReel28); }
