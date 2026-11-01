// crowd_demo.cpp -- "crowd-demo": a 23 s utility clip for crowd::. One world, baked once:
// two streams of people cross a plaza round a fountain, a herd flees a dog, a flock of
// ducks banks through a turn overhead, and a school of fish opens round a pike. One agent
// per beat shows its rules as teal rings; a threat's reach is amber. The labels are the
// script; the score and its effects are generated from the same bake.
//
//   raymotion render crowd-demo
//
// See docs/crowd-demo-brief.md and docs/crowd-demo-qc.md.
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
constexpr int FPS = 30, TOTAL = 696;           // 23.2 s
const std::string ASSETS = "out/crowd-demo-assets";

const Color PAPER = mkColor(0xf3efe6), INK = mkColor(0x1f1d2b), DIM = mkColor(0x8a8597),
            TEAL = mkColor(0x16a39a), AMBER = mkColor(0xd98b2b), PLAZA = mkColor(0xe6dece),
            FIELD = mkColor(0xe4e2c8), LINE = mkColor(0xd3c9b6), STONE = mkColor(0xb9a98c),
            WATER = mkColor(0x9cc9c4), BED = mkColor(0x6f8f93), PIKE = mkColor(0x56623a);

// ------------------------------------------------------------------ timing (brief §3), seconds
constexpr double HERD_AT = 5.6, FLOCK_AT = 10.6, SCHOOL_AT = 14.8, END_AT = 19.8, MOVE = 1.2, DIVE = 1.4;
constexpr double DOG_GO = 5.6, DOG_TURN = 8.6, HERD_MOVES = 9.0;
constexpr double PIKE_GO = 16.5, PIKE_HIT = 17.45;  // the pike sets off; it reaches the school

// ------------------------------------------------------------------ the world (metres)
const Vector3 FOUNTAIN{ 0, 0, 0 };
constexpr float FOUNTAIN_R = 1.6f;
const Vector3 F{ 34, 0, 0 };                     // the field's centre
constexpr float PANIC = 9;                       // how near the dog the horses bolt
const Vector3 SKY{ 34, 7, 0 };                   // the centre of the ducks' circle
constexpr float CIRCLE_R = 6, DUCK_SPEED = 5;
const Vector3 POND{ 20, 0, -10 };                // beside the flock's camera, clear of the dog
constexpr float POND_R = 4, SCHOOL_R = 2.4f, STRIKE_R = 3;   // the school circles; the pike strikes to
constexpr float FISH_SPEED = 1.1f, FISH_PANIC = 1.6f, FISH_Y = 0.2f, SURFACE_Y = 0.34f;   // under the rim's top

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

// `aim`: the angle round the pond where the pike strikes; without one it only waits.
crowd::World makeWorld(const float* aim) {
    crowd::World w;
    w.from = -2.5;                               // a pre-roll: everyone is under way at frame 0
    w.to = TOTAL / (double)FPS + 0.2;
    w.boundsMin = { -40, 0, -40 };
    w.boundsMax = { 80, 30, 40 };
    w.seed = 11;
    w.obstacles.push_back({ FOUNTAIN, FOUNTAIN_R });

    // Two streams of people, one each way, through the fountain.
    for (int dir : { 1, -1 }) {
        crowd::Group g;
        g.name = dir > 0 ? "north" : "south";
        g.count = 20;
        g.spawn = { 0, 0, -8.f * dir };
        g.spawnSize = { 10, 0, 4 };
        g.heading0 = { 0, 0, (float)dir };
        g.headingSpread = 20;
        g.goal = [dir](double, int i) { return Vector3{ (float)(i % 5 - 2) * 2.2f, 0, 18.f * dir }; };
        w.groups.push_back(g);
    }
    // A herd at pasture, and the dog that sends it to the far side.
    crowd::Group herd;
    herd.name = "herd";
    herd.count = 12;
    herd.spawn = F;
    herd.spawnSize = { 9, 0, 7 };
    herd.heading0 = { -1, 0, 0 };
    herd.headingSpread = 120;
    herd.cruise = 1.1f;
    herd.maxSpeed = 7;
    herd.maxAccel = 6;
    herd.maxTurn = 220;
    herd.steer.personal = 1.7f;
    herd.steer.neighbour = 8;
    herd.steer.panic = PANIC;
    herd.flees = { "dog" };
    herd.goal = [](double s, int i) {
        const Vector3 at = s < HERD_MOVES ? F : Vector3{ F.x + 10, 0, F.z + 7 };
        return Vector3{ at.x + (float)(i % 4 - 1.5f) * 2.6f, 0, at.z + (float)(i / 4 - 1) * 2.4f };
    };
    w.groups.push_back(herd);
    crowd::Group dog;
    dog.name = "dog";
    dog.count = 1;
    dog.spawn = { F.x - 23, 0, F.z - 3 };
    dog.spawnSize = { 0, 0, 0 };
    dog.heading0 = { 1, 0, 0 };
    dog.headingSpread = 0;
    dog.cruise = 5;
    dog.maxSpeed = 6;
    dog.maxAccel = 8;
    dog.maxTurn = 240;
    dog.speedJitter = 0;
    dog.sizeJitter = 0;
    dog.steer.personal = 0.8f;
    dog.goal = [](double s, int) {
        if (s < DOG_GO) return Vector3{ F.x - 15, 0, F.z - 3 };  // trots up and waits, facing the herd
        if (s < DOG_TURN) return F;                              // straight at the herd
        return Vector3{ F.x + 2, 0, F.z - 16 };                  // then off, its job done
    };
    w.groups.push_back(dog);
    // Ducks circling the field: the goal runs ahead of them round the circle.
    crowd::Group ducks;
    ducks.name = "ducks";
    ducks.medium = crowd::Medium::Air;
    ducks.count = 20;
    ducks.spawn = { SKY.x + CIRCLE_R, SKY.y, SKY.z };
    ducks.spawnSize = { 4, 2, 6 };
    ducks.heading0 = { 0, 0, 1 };
    ducks.cruise = DUCK_SPEED;
    ducks.maxSpeed = 7;
    ducks.maxAccel = 5;
    ducks.maxTurn = 90;
    ducks.height0 = SKY.y - 1.5f;
    ducks.height1 = SKY.y + 1.5f;
    ducks.steer.personal = 1;
    ducks.steer.neighbour = 8;
    ducks.goal = [](double s, int) {
        const double a = s * DUCK_SPEED / CIRCLE_R + 0.7;          // ~40 degrees ahead on the circle
        return Vector3{ SKY.x + CIRCLE_R * (float)std::cos(a), SKY.y, SKY.z + CIRCLE_R * (float)std::sin(a) };
    };
    w.groups.push_back(ducks);
    // A school circling the pond, its goal running ahead of it, round the pike that waits in
    // the middle and then strikes out straight through it.
    crowd::Group fish;
    fish.name = "fish";
    fish.medium = crowd::Medium::Water;
    fish.count = 30;
    fish.spawn = { POND.x + SCHOOL_R, FISH_Y, POND.z };
    fish.spawnSize = { 1.4f, 0.2f, 2.2f };
    fish.heading0 = { 0, 0, 1 };
    fish.headingSpread = 20;
    fish.cruise = FISH_SPEED;
    fish.maxSpeed = 3.4f;
    fish.maxAccel = 7;
    fish.maxTurn = 300;
    fish.height0 = FISH_Y - 0.08f;
    fish.height1 = FISH_Y + 0.08f;
    fish.steer.personal = 0.32f;
    fish.steer.neighbour = 1.4f;
    fish.steer.panic = FISH_PANIC;
    fish.steer.flee = 6;
    fish.flees = { "pike" };
    fish.penAt = POND;
    fish.penRadius = POND_R - 0.65f;             // a body length inside the rim
    fish.goal = [](double s, int) {
        const double a = s * FISH_SPEED / SCHOOL_R + 0.6;
        return Vector3{ POND.x + SCHOOL_R * (float)std::cos(a), FISH_Y, POND.z + SCHOOL_R * (float)std::sin(a) };
    };
    w.groups.push_back(fish);
    crowd::Group pike;
    pike.name = "pike";
    pike.medium = crowd::Medium::Water;
    pike.count = 1;
    const float b = aim ? *aim : 0.f;
    const Vector3 u{ std::cos(b), 0, std::sin(b) };
    pike.spawn = { POND.x, FISH_Y, POND.z };
    pike.spawnSize = { 0, 0, 0 };
    pike.heading0 = { 1, 0, 0 };
    pike.headingSpread = 0;
    pike.cruise = 2.8f;
    pike.maxSpeed = 3.6f;
    pike.maxAccel = 9;
    pike.maxTurn = 360;
    pike.speedJitter = 0;
    pike.sizeJitter = 0;
    pike.height0 = FISH_Y - 0.03f;
    pike.height1 = FISH_Y + 0.03f;
    pike.steer.personal = 0.2f;
    pike.penAt = POND;
    pike.penRadius = POND_R - 0.9f;
    pike.steer.goal = 4;                         // a pike strikes: full acceleration at once
    pike.goal = [u, struck = aim != nullptr](double s, int) {
        // Waits in the middle; then out at full speed (a far goal: arrival would brake it), and
        // pulls up short of the rim.
        const float r = !struck || s < PIKE_GO ? 0 : s < PIKE_GO + 1.0 ? 12 : STRIKE_R;
        return Vector3{ POND.x + u.x * r, FISH_Y, POND.z + u.z * r };
    };
    w.groups.push_back(pike);
    return w;
}

struct World {
    crowd::CrowdPtr crowd;
    int walker = 0, dog = 0, duck = 0;           // the agents the overlays follow
    Vector3 skyCam{};                            // where the camera stands to look up at the flock
    int pike = 0;
    double bolt = 0;                             // when the herd breaks into flight (its first horse)
    Vector3 centre(const std::string& group, double s) const {
        const int a0 = crowd->first(group), n = crowd->groupOf(a0).count;
        Vector3 m{};
        for (int k = 0; k < n; k++) m = Vector3Add(m, crowd->at(a0 + k, s).pos);
        return Vector3Scale(m, 1.f / n);
    }
    Vector3 flock(double s) const { return centre("ducks", s); }
};

std::shared_ptr<World> bakeWorld() {
    auto w = std::make_shared<World>();
    // The pike strikes where the school will be. A first bake, with the pike only waiting,
    // finds that place; the school's path up to the strike is the same in both bakes, since
    // the pike waits in the same place in both.
    w->crowd = crowd::bake(makeWorld(nullptr));
    const Vector3 at = w->centre("fish", PIKE_HIT);
    const float aim = std::atan2(at.z - POND.z, at.x - POND.x);
    w->crowd = crowd::bake(makeWorld(&aim));
    const crowd::Crowd& c = *w->crowd;
    // The walker: the northbound person who starts most squarely behind the fountain.
    const int n0 = c.first("north");
    float best = 1e9f;
    for (int k = 0; k < c.groupOf(n0).count; k++) {
        const float x = std::fabs(c.at(n0 + k, 0).pos.x);
        if (x < best) { best = x; w->walker = n0 + k; }
    }
    w->dog = c.first("dog");
    w->duck = c.first("ducks");
    w->pike = c.first("pike");
    const int h0 = c.first("herd");
    for (double s = HERD_AT; s < FLOCK_AT && w->bolt == 0; s += 1.0 / 120)   // the first horse to break
        for (int k = 0; k < c.groupOf(h0).count; k++)
            if (c.at(h0 + k, s).speed > 2.5f) w->bolt = s;
    // The flock beat's camera: on the ground outside the circle, where the flock passes
    // closest in the middle of the beat.
    const Vector3 mid = w->flock((FLOCK_AT + END_AT) / 2);
    const Vector3 out = Vector3Normalize({ mid.x - SKY.x, 0, mid.z - SKY.z });
    w->skyCam = { mid.x + out.x * 5, 1.6f, mid.z + out.z * 5 };
    return w;
}

// ------------------------------------------------------------------ camera
struct Cam { Vector3 pos, tgt; float fov; };
Cam camAt(const World& w, double s) {
    const Cam plaza{ { -9 + 0.25f * (float)s, 11, -16 }, { 0.5f, 0, 0.5f }, 36 };
    const Cam field{ { F.x - 8, 8.5f, -18 }, { F.x + 2.5f, 0, 2 }, 40 };
    const Cam sky{ w.skyCam, w.flock(s), 54 };    // looking up, following the flock
    const Cam pond{ { POND.x + 0.6f, 9.6f, POND.z - 4.6f }, { POND.x, 0, POND.z - 0.5f }, 42 };   // steep: the far rim's inner face hides
    auto mix = [](const Cam& a, const Cam& b, float k) {
        return Cam{ Vector3Lerp(a.pos, b.pos, k), Vector3Lerp(a.tgt, b.tgt, k), a.fov + (b.fov - a.fov) * k };
    };
    if (s < HERD_AT + MOVE) return mix(plaza, field, smooth((s - HERD_AT) / MOVE));
    if (s < FLOCK_AT + MOVE) return mix(field, sky, smooth((s - FLOCK_AT) / MOVE));
    // Down to the pond: the aim leads the move, so the pond is in view before the camera arrives.
    const float u = (float)((s - SCHOOL_AT) / DIVE);
    Cam c = mix(sky, pond, smooth(u));
    c.tgt = Vector3Lerp(sky.tgt, pond.tgt, smooth(u * 1.6f));
    return c;
}
g3d::Camera camera(const World& w, double s) {
    const Cam c = camAt(w, s);
    g3d::Camera cam;
    cam.position = c.pos;
    cam.target = c.tgt;
    cam.fov = c.fov;
    cam.farPlane = 300;
    return cam;
}
bool screenOf(const World& w, double s, Vector3 p, Vector2& out) { return g3d::project(camera(w, s), p, { 0, 0, W, H }, out); }

std::shared_ptr<g3d::Scene> makeScene(const std::shared_ptr<World>& w) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->skyColor = PAPER;
    scene->groundColor = { 196, 186, 168, 255 };
    scene->environmentGenerator = g3d::studioEnvironment({ 252, 249, 242, 255 }, { 226, 219, 204, 255 }, { 150, 140, 124, 255 }, 3);
    scene->environmentIntensity = 0.45f;
    scene->ambient = 0.5f;
    scene->fogColor = PAPER;
    scene->fogNear = 30;
    scene->fogFar = 80;
    g3d::Light key = g3d::Light::directional({ -3, 6, 4 }, { 255, 247, 234, 255 }, 1.15f, true);
    key.up = [w](int t, g3d::Light& l) {
        const Cam c = camAt(*w, t / (double)FPS);
        l.target = { c.tgt.x, 0, c.tgt.z };
        l.position = Vector3Add(l.target, { -3, 6, 4 });
        l.shadowExtent = t / (double)FPS > SCHOOL_AT + DIVE / 2 ? 7 : 18;   // sharp fish shadows at the pond
    };
    scene->lights.push_back(key);
    scene->lights.push_back(g3d::Light::directional({ 3, 2.5f, -2 }, { 200, 214, 255, 255 }, 0.5f));
    scene->camera.up = [w](int t, g3d::Camera& cam) {
        const Cam c = camAt(*w, t / (double)FPS);   // fields only: assigning a Camera would drop this hook
        cam.position = c.pos;
        cam.target = c.tgt;
        cam.fov = c.fov;
        cam.farPlane = 300;
    };

    // Ground: paper, the plaza a shade darker with a line every 2 m (a sliding foot shows),
    // the field greener.
    g3d::Material ground;
    ground.color = PAPER;
    ground.roughness = 0.95f;
    ground.castShadow = false;
    scene->add(g3d::Geometry::plane(400, 400), ground);
    g3d::Material plaza = ground;
    plaza.color = PLAZA;
    scene->add(g3d::Geometry::plane(30, 40), plaza)->position = { 0, 0.002f, 0 };
    g3d::Material line = ground;
    line.color = LINE;
    for (int i = -7; i <= 7; i++) scene->add(g3d::Geometry::plane(0.03f, 40), line)->position = { 2.f * i, 0.004f, 0 };
    for (int i = -10; i <= 10; i++) scene->add(g3d::Geometry::plane(30, 0.03f), line)->position = { 0, 0.004f, 2.f * i };
    g3d::Material field = ground;
    field.color = FIELD;
    scene->add(g3d::Geometry::plane(150, 200), field)->position = { 15 + 75, 0.002f, 0 };   // from the plaza's edge on
    // The fountain: a stone basin, water, a column.
    g3d::Material stone;
    stone.color = STONE;
    stone.roughness = 0.8f;
    scene->add(g3d::Geometry::cylinder(FOUNTAIN_R, 0.5f), stone)->position = { FOUNTAIN.x, 0.25f, FOUNTAIN.z };
    g3d::Material water;
    water.color = WATER;
    water.roughness = 0.2f;
    scene->add(g3d::Geometry::cylinder(FOUNTAIN_R - 0.15f, 0.02f), water)->position = { FOUNTAIN.x, 0.51f, FOUNTAIN.z };
    scene->add(g3d::Geometry::cylinder(0.22f, 1.4f), stone)->position = { FOUNTAIN.x, 0.7f, FOUNTAIN.z };
    // The pond: a blue-grey bed the fish shadow, a stone rim, a faint surface above them.
    g3d::Material bed = ground;
    bed.color = BED;
    scene->add(g3d::Geometry::cylinder(POND_R, 0.02f), bed)->position = { POND.x, 0.01f, POND.z };
    g3d::Object3D* rim = scene->add(g3d::Geometry::torus(POND_R + 0.12f, 0.2f, 96, 16), stone);
    rim->position = { POND.x, 0.2f, POND.z };
    rim->rotation = { 90, 0, 0 };
    g3d::Material surface = water;
    surface.opacity = 0.2f;
    surface.castShadow = false;
    scene->add(g3d::Geometry::cylinder(POND_R, 0.005f), surface)->position = { POND.x, SURFACE_Y, POND.z };

    // The cast. Faces on the nearer few only: each is a texture drawn every frame.
    const crowd::CrowdPtr c = w->crowd;
    const auto north = crowd::cast(c, "north", [](int i) { return crowd::person(i); }, 8);
    const auto south = crowd::cast(c, "south", [](int i) { return crowd::person(i + 20); }, 8);
    const auto horses = crowd::cast(c, "herd", [](int i) { return crowd::herd("horse", i); });
    const auto dog = crowd::cast(c, "dog", [](int i) { return crowd::herd("dog", i); });
    const auto ducks = crowd::cast(c, "ducks", [](int i) { return crowd::herd("duck", i); });
    const auto fish = crowd::cast(c, "fish", [](int i) {
        rig::Look l = crowd::herd("fish", i);
        l.build = l.build.scaled(1.4f);                  // koi rather than minnows: they read from 10 m
        return l;
    });
    const auto pike = crowd::cast(c, "pike", [](int i) {
        rig::Look l = crowd::herd("fish", i);
        l.build = l.build.scaled(3.4f);
        l.skin = PIKE;
        l.accent = { 196, 188, 118, 255 };
        l.outfit.fallback = l.skin;
        return l;
    });
    crowd::addAll(*scene, c, "north", north, FPS);
    crowd::addAll(*scene, c, "south", south, FPS);
    crowd::addAll(*scene, c, "herd", horses, FPS);
    crowd::addAll(*scene, c, "dog", dog, FPS);
    crowd::addAll(*scene, c, "ducks", ducks, FPS);
    crowd::addAll(*scene, c, "fish", fish, FPS);
    crowd::addAll(*scene, c, "pike", pike, FPS);
    return scene;
}

// ------------------------------------------------------------------ the proof overlays
// A ring of radius r round p, level, drawn through the camera; dashed draws every other arc.
void ringAt(const World& w, const Ctx& cx, double s, Vector3 p, float r, float width, Color col, bool dashed) {
    constexpr int N = 72;
    Vector2 prev{};
    bool had = false;
    for (int i = 0; i <= N; i++) {
        const float a = (float)i / N * 2 * PI;
        Vector2 q;
        const bool ok = screenOf(w, s, { p.x + r * std::cos(a), p.y, p.z + r * std::sin(a) }, q);
        if (ok && had && (!dashed || (i / 2) % 2 == 0)) dr::strokeSeg(cx, prev, q, width, col);
        prev = q;
        had = ok;
    }
}
void arrowAt(const World& w, const Ctx& cx, double s, Vector3 from, Vector3 to, Color col) {
    Vector2 a, b;
    if (!screenOf(w, s, from, a) || !screenOf(w, s, to, b)) return;
    dr::strokeSeg(cx, a, b, 5, col);
    const Vector2 d = Vector2Normalize(Vector2Subtract(b, a)), n{ -d.y, d.x };
    const Vector2 back = Vector2Subtract(b, Vector2Scale(d, 22));
    const Vector2 tri[3] = { b, Vector2Add(back, Vector2Scale(n, 11)), Vector2Subtract(back, Vector2Scale(n, 11)) };
    dr::fillPoly(cx, tri, 3, col);
}

void proofs(const Ctx& cx, const World& w, double s) {
    const crowd::Crowd& c = *w.crowd;
    // 1: the walker's personal space, its sight and where it will be in a second.
    if (const float k = window(s, 1.2, HERD_AT - 0.2); k > 0.001f) {
        const crowd::State st = c.at(w.walker, s);
        const crowd::Steer& S = c.groupOf(w.walker).steer;
        const Ctx m = faded(cx, k);
        const Vector3 p{ st.pos.x, 0.03f, st.pos.z };
        ringAt(w, m, s, p, S.neighbour, 3, withAlpha(TEAL, 150), true);
        ringAt(w, m, s, p, S.personal, 5, TEAL, false);
        arrowAt(w, m, s, p, Vector3Add(p, { st.vel.x, 0, st.vel.z }), TEAL);
    }
    // 2: the dog's reach: inside it, the herd bolts.
    if (const float k = window(s, 6.6, 9.8); k > 0.001f) {
        const crowd::State st = c.at(w.dog, s);
        const Ctx m = faded(cx, k);
        ringAt(w, m, s, { st.pos.x, 0.03f, st.pos.z }, PANIC, 5, AMBER, false);
    }
    // 3: one duck's bank: a level line (dashed) and its wings' line, rolled into the turn. From
    // 13 s, when the flock flies away from the camera: from below, a roll can't be seen.
    if (const float k = window(s, 13.0, SCHOOL_AT - 0.2); k > 0.001f) {
        const crowd::State st = c.at(w.duck, s);
        const Ctx m = faded(cx, k);
        // The node's own axes (crowd::add: yaw on the holder, pitch and bank under it).
        const Matrix turn = MatrixRotateXYZ(Vector3Scale({ 0, st.yaw, 0 }, DEG2RAD));
        const Matrix tilt = MatrixMultiply(MatrixRotateXYZ(Vector3Scale({ st.pitch, 0, st.bank }, DEG2RAD)), turn);
        const Vector3 level = Vector3Transform({ 1, 0, 0 }, turn), wings = Vector3Transform({ 1, 0, 0 }, tilt);
        constexpr float span = 1.3f;
        Vector2 a, b;
        if (screenOf(w, s, Vector3Subtract(st.pos, Vector3Scale(level, span)), a) &&
            screenOf(w, s, Vector3Add(st.pos, Vector3Scale(level, span)), b))
            for (int i = 0; i < 9; i += 2)                     // dashed
                dr::strokeSeg(m, Vector2Lerp(a, b, i / 9.f), Vector2Lerp(a, b, (i + 1) / 9.f), 3, withAlpha(TEAL, 170));
        if (screenOf(w, s, Vector3Subtract(st.pos, Vector3Scale(wings, span)), a) &&
            screenOf(w, s, Vector3Add(st.pos, Vector3Scale(wings, span)), b)) {
            dr::strokeSeg(m, a, b, 5, TEAL);
            const Vector2 tag = a.x > b.x ? a : b;
            text(m, TextFormat("bank %.0f\xC2\xB0",std::fabs(st.bank)), { tag.x + 18, tag.y }, 30, TEAL, 0, 0.5f, true);
        }
        arrowAt(w, m, s, st.pos, Vector3Add(st.pos, Vector3Scale(st.vel, 0.5f)), TEAL);
    }
    // 4: the pike's reach, at the fishes' depth: the school opens where it passes.
    if (const float k = window(s, 16.4, 18.8); k > 0.001f) {
        const crowd::State st = c.at(w.pike, s);
        ringAt(w, faded(cx, k), s, { st.pos.x, FISH_Y, st.pos.z }, FISH_PANIC, 5, AMBER, false);
    }
}

// One beat label at a time (brief reveal map): the beat, the rules it shows.
struct Label { double from, to; const char* name; const char* rules; };
const Label LABELS[] = {
    { 0.6, 5.4, "crowd", "separation \xC2\xB7 passing \xC2\xB7 goals" },
    { 6.4, 10.4, "herd", "cohesion \xC2\xB7 flee" },
    { 11.2, 14.6, "flock", "alignment \xC2\xB7 banking into turns" },
    { 15.8, 19.4, "school", "3D steering \xC2\xB7 split \xC2\xB7 rejoin" },
};

void overlays(const Ctx& cx, const World& w, int t) {
    const double s = t / (double)FPS;
    proofs(cx, w, s);
    const float kk = window(s, 0.3, END_AT - 0.2);
    text(faded(cx, kk), "CROWDS", { 96, 92 }, 26, TEAL, 0, 0.5f, true, 6);
    text(faded(cx, kk), "boids, baked once, sampled per frame", { 96, 128 }, 26, DIM, 0, 0.5f);
    for (const Label& l : LABELS) {
        const float a = window(s, l.from, l.to);
        if (a <= 0.001f) continue;
        Ctx m = faded(cx, a);
        m.o.y += (1 - a) * 14 * m.s;
        text(m, l.name, { 96, 880 }, 96, INK, 0, 0.5f, true);
        dr::text(code::font(false), m, l.rules, { 100, 960 }, 30, 0, TEAL, 0, 0.5f);
    }
}

void endCard(const Ctx& cx, int t) {
    const float kt = smooth(t / 14.0), ka = smooth((t - 18) / 14.0);
    Ctx tm = faded(cx, kt);
    tm.o.y += (1 - kt) * 20 * tm.s;
    const Color clear = withAlpha(PAPER, 0);
    dr::rectGrad4(faded(cx, kt), 0, 720, W, 120, clear, clear, PAPER, PAPER);
    dr::rect(faded(cx, kt), 0, 840, W, 240, PAPER);
    text(tm, "ONE BAKE, ANY FRAME.", { W / 2, 860 }, 72, INK, 0.5f, 0.5f, true, 2);
    dr::text(code::font(false), faded(cx, ka), "crowd::bake(world)  \xC2\xB7  crowd::cast  \xC2\xB7  crowd::addAll",
             { W / 2, 940 }, 30, 0, TEAL, 0.5f, 0.5f);
}

// ------------------------------------------------------------------ sound (brief section 7)
// Every effect is placed from the bake: the ground an agent covers sets its steps, and where
// the camera sees it sets how loud and on which side.
using procgen::Score;
constexpr int RATE = Score::RATE;

struct Heard { float gain, pan; };
Heard heard(const World& w, double s, Vector3 p, float near) {
    const float d = Vector3Distance(camAt(w, s).pos, p);
    Vector2 q;
    const bool seen = screenOf(w, s, p, q) && q.x > -100 && q.x < W + 100 && q.y > -100 && q.y < H + 100;
    return { std::min(1.f, near / std::max(d, 0.1f)) * (seen ? 1.f : 0.35f),
             seen ? std::clamp(0.5f + 0.7f * (q.x / W - 0.5f), 0.15f, 0.85f) : 0.5f };
}
// hit(s) each time agent a has covered another `step` metres between from and to.
template <class F> void strides(const crowd::Crowd& c, int a, double from, double to, double step, F hit) {
    const double phase = rnd01(23, (unsigned long long)a) * step;
    double next = std::floor((c.travelled(a, from) - phase) / step + 1) * step + phase;
    for (double s = from; s < to; s += 1.0 / 240)
        if (c.travelled(a, s) >= next) { hit(s); next += step; }
}

// A footfall on stone: a short band of noise.
void scuff(Score& sc, double t0, float amp, float pan, float hz, float decay) {
    const int a = (int)(t0 * RATE), len = (int)(0.09 * RATE);
    procgen::Svf f;
    for (int k = 0; k < len; k++) {
        const double t = (double)k / RATE;
        const float env = (float)(std::min(1.0, t / 0.002) * std::exp(-t * decay));
        sc.put(a + k, f.bp(sc.noise(), hz, 0.6f, RATE) * env * amp, pan, 0.06f, 0);
    }
}
// A hoof on turf: a low thump with a little grit.
void hoof(Score& sc, double t0, float amp, float pan) {
    const int a = (int)(t0 * RATE), len = (int)(0.16 * RATE);
    procgen::Svf f;
    double ph = 0;
    for (int k = 0; k < len; k++) {
        const double t = (double)k / RATE;
        ph += 2 * PI * (70 + 90 * std::exp(-t * 40)) / RATE;
        const float x = (float)(std::sin(ph) * std::exp(-t * 28)) + 0.4f * f.bp(sc.noise(), 520, 0.7f, RATE) * (float)std::exp(-t * 60);
        sc.put(a + k, x * amp, pan, 0.05f, 0);
    }
}
// A wingbeat: a breath of noise that swells and falls.
void wing(Score& sc, double t0, float amp, float pan) {
    const int a = (int)(t0 * RATE), len = (int)(0.12 * RATE);
    procgen::Svf f;
    for (int k = 0; k < len; k++) {
        const float u = (float)k / len;
        sc.put(a + k, f.bp(sc.noise(), 900 + 900 * u, 0.5f, RATE) * std::sin(PI * u) * amp, pan, 0.1f, 0);
    }
}
// Filtered noise from t0 to t1, its band sliding hz0 -> hz1 and its pan pan0 -> pan1. `swell`:
// rises and falls (a rush past); otherwise a steady bed that breathes.
void wash(Score& sc, double t0, double t1, float amp, float hz0, float hz1, float pan0, float pan1, bool swell) {
    const int a = (int)(t0 * RATE), len = (int)((t1 - t0) * RATE);
    procgen::Svf f;
    for (int k = 0; k < len; k++) {
        const float u = (float)k / len, t = (float)k / RATE;
        const float env = swell ? std::sin(PI * u) * std::sin(PI * u)
                                : std::min({ 1.f, u * 6, (1 - u) * 6 }) * (0.7f + 0.3f * std::sin(t * 2.1f));
        sc.put(a + k, f.bp(sc.noise(), hz0 + (hz1 - hz0) * u, 0.5f, RATE) * env * amp, pan0 + (pan1 - pan0) * u, 0.3f, 0);
    }
}

void genScore(const std::string& path, const World& w) {
    const crowd::Crowd& c = *w.crowd;
    Score s(TOTAL / (double)FPS + 1);
    // A pad per beat: D in the plaza, B minor as the dog waits, G under the flock, E minor at
    // the pond; and a pluck on each beat's name, climbing.
    struct Chord { double at, to; int notes[3]; int bass; };
    const Chord chords[] = { { 0, HERD_AT, { 62, 66, 69 }, 38 }, { HERD_AT, FLOCK_AT, { 59, 62, 66 }, 35 },
                             { FLOCK_AT, SCHOOL_AT, { 55, 59, 62 }, 43 }, { SCHOOL_AT, END_AT, { 52, 55, 59 }, 40 } };
    for (const Chord& ch : chords) {
        for (int n : ch.notes) s.pad(ch.at, ch.to - ch.at, n, 0.07f, 850, 0.3f + 0.2f * (n % 3));
        s.bass(ch.at, ch.to - ch.at - 0.1, ch.bass, 0.1f);
    }
    const int names[] = { 74, 78, 79, 83 };
    for (int k = 0; k < 4; k++) s.pluck(LABELS[k].from, names[k], 0.07f, 0.4f + 0.06f * k, 5);

    // The plaza: a step each 0.75 m anyone walks.
    for (const char* g : { "north", "south" }) {
        const int a0 = c.first(g);
        for (int k = 0; k < c.groupOf(a0).count; k++)
            strides(c, a0 + k, 0, FLOCK_AT, 0.75, [&](double t) {
                const crowd::State st = c.at(a0 + k, t);
                if (st.speed < 0.2f) return;
                const Heard h = heard(w, t, st.pos, 9);
                scuff(s, t, 0.05f * h.gain, h.pan, 1100, 70);
            });
    }
    // The field: the dog's patter, a hoof each 1.6 m a horse covers (standing horses are
    // silent; the gallop is a rumble), a riser into the bolt and a hit on it.
    strides(c, w.dog, 0, FLOCK_AT + MOVE, 0.5, [&](double t) {
        const Heard h = heard(w, t, c.at(w.dog, t).pos, 10);
        scuff(s, t, 0.06f * h.gain, h.pan, 1800, 110);
    });
    const int h0 = c.first("herd");
    for (int k = 0; k < c.groupOf(h0).count; k++)
        strides(c, h0 + k, 0, SCHOOL_AT, 1.6, [&](double t) {
            const crowd::State st = c.at(h0 + k, t);
            const Heard h = heard(w, t, st.pos, 12);
            hoof(s, t, 0.12f * h.gain * std::clamp(st.speed / 5, 0.25f, 1.f), h.pan);
        });
    if (w.bolt > 0) {
        s.riser(w.bolt - 1.4, w.bolt, 0.1f);
        s.impact(w.bolt, 0.16f);
    }
    // The flock: a wingbeat per flap at each duck's rate, quiet while it glides down.
    const float flap = rig::flapRate(rig::animal("duck").build);
    const int d0 = c.first("ducks");
    for (int k = 0; k < c.groupOf(d0).count; k++) {
        const double rate = flap * (0.9 + 0.2 * rnd01(31, (unsigned long long)k));
        for (double t = HERD_AT + rnd01(29, (unsigned long long)k) / rate; t < SCHOOL_AT + DIVE; t += 1 / rate) {
            const crowd::State st = c.at(d0 + k, t);
            if (st.vel.y < -0.25f) continue;
            const Heard h = heard(w, t, st.pos, 6);
            wing(s, t, 0.05f * h.gain, h.pan);
        }
    }
    // The pond: a water bed; the pike's rush across it; a high note as each fish scatters.
    wash(s, SCHOOL_AT - 0.4, END_AT + 1.2, 0.05f, 420, 520, 0.5f, 0.5f, false);
    const Heard p0 = heard(w, PIKE_GO, c.at(w.pike, PIKE_GO).pos, 10);
    const Heard p1 = heard(w, PIKE_GO + 1.8, c.at(w.pike, PIKE_GO + 1.8).pos, 10);
    wash(s, PIKE_GO - 0.1, PIKE_GO + 1.8, 0.16f, 300, 1400, p0.pan, p1.pan, true);
    const int f0 = c.first("fish");
    const int scale[] = { 76, 79, 81, 83, 86, 88 };
    for (int k = 0; k < c.groupOf(f0).count; k++)
        for (double t = PIKE_GO; t < END_AT; t += 1.0 / 60)
            if (Vector3Distance(c.at(f0 + k, t).pos, c.at(w.pike, t).pos) < FISH_PANIC) {
                const Heard h = heard(w, t, c.at(f0 + k, t).pos, 10);
                s.pluck(t, scale[k % 6], 0.035f, h.pan, 9);
                break;
            }
    // The card: a hit, four bells, the plaza's D again.
    s.impact(END_AT + 0.1, 0.3f);
    for (int j = 0; j < 4; j++) s.bell(END_AT + 0.15 + 0.12 * j, 74 + j * 3, 0.06f, 0.3f + 0.12f * j);
    for (int n : { 50, 57, 62, 66 }) s.pad(END_AT, TOTAL / (double)FPS - END_AT, n, 0.07f, 1300, 0.5f);

    std::vector<float> mix = s.mix();
    for (float& x : mix) x = std::tanh(x * 3.0f);
    float peak = 0;
    for (float x : mix) peak = std::max(peak, std::fabs(x));
    const float g = peak > 0 ? 0.43f / peak : 1.f;              // about -16 LUFS integrated
    for (float& x : mix) x *= g;
    media::writeWav(path, mix, RATE);
}

void ensureAssets() {
    std::filesystem::create_directories(ASSETS);
    const std::string score = ASSETS + "/score.wav";
    if (!std::filesystem::exists(score)) genScore(score, *bakeWorld());
}

void build(Comp& c, const Json&) {
    c.roots.clear();
    const auto w = bakeWorld();
    const auto scene = makeScene(w);
    addRoot(c.roots, 0, TOTAL)->fn = [scene, w](const Ctx& cx, int t) {
        dr::rect(cx, 0, 0, W, H, PAPER);
        g3d::render(*scene, t, cx);
        overlays(cx, *w, t);
    };
    const int e = (int)(END_AT * FPS);
    addRoot(c.roots, e, TOTAL - e)->fn = [](const Ctx& cx, int t) { endCard(cx, t); };
    TNode* logo = prefab::use(c.roots, e + 28, TOTAL - e - 28, "raymotion-logo",
                              Json::parse(R"({"size": 56, "layout": "horizontal", "tagline": "video as code", "delay": 2, "ink": "#1f1d2b", "accent": "#139e98"})"));
    logo->props.pos = { W / 2 - 150, 1020 };
    addRoot(c.roots, e + 36, TOTAL - e - 36)->fn = [](const Ctx& cx, int t) {
        dr::text(code::font(false), faded(cx, smooth(t / 12.0)), "github.com/pebaryan/raymotion", { W / 2 - 30, 1020 }, 26, 0, DIM, 0, 0.5f);
    };
    media::audio(addRoot(c.roots, 0, TOTAL), ASSETS + "/score.wav");
}

} // namespace

void registerCrowdDemo(Registry& out) {
    Comp c;
    c.name = "crowd-demo";
    c.w = (int)W; c.h = (int)H; c.fps = FPS;
    c.duration = TOTAL;
    c.background = PAPER;
    c.assets = [](const Comp&, const Json&) { ensureAssets(); };
    c.build = [](Comp& c, const Json& p) { build(c, p); };
    out.push_back(std::move(c));
}

} // namespace rmtn
