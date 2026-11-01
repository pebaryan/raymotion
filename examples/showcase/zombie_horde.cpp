// zombie_horde.cpp -- "zombie-horde": a 20 s utility clip for instanced crowds. A street at dusk:
// eight zombies shamble at the camera (full rig characters), and behind them the street fills
// with 500 more, drawn as four baked bodies through g3d::Instances; then 1000 bats pour out
// of a belfry and wheel over them, one baked wingbeat each. A counter reads how many bodies
// are in frame; a strip of baked poses with one pointer per ringed body shows how a mass of
// bodies is a few poses at their own moments. The score is generated from the same bakes.
//
//   raymotion render zombie-horde
//
// See docs/zombie-horde-brief.md and docs/zombie-horde-qc.md.
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
const std::string ASSETS = "out/zombie-horde-assets";

const Color SKY_TOP = mkColor(0x2b2640), SKY_LOW = mkColor(0x5b3d5a), GROUND = mkColor(0x34303a), BONE = mkColor(0xe9e4d4),
            SICK = mkColor(0x8fa77a), AMBER = mkColor(0xe6a23c), BAT = mkColor(0x17141f), DIM = mkColor(0x9a93a8);

// ------------------------------------------------------------------ timing (brief section 3), seconds
constexpr double SWARM_AT = 11.0, END_AT = 16.0, CRANE_FROM = 2.6, CRANE_TO = 5.0, BAT_GO = 12.2;
constexpr double ZOMBIE_RING_FROM = 7.2, STRIP_FROM = 6.1, BAT_STRIP_FROM = 12.6;

// ------------------------------------------------------------------ the world (metres)
constexpr int HEROES = 8, HORDE = 500, BATS = 1000, VARIANTS = 4;
constexpr float STREET_HALF = 11, BELFRY_X = -24, BELFRY_Z = 36, BELFRY_DOOR_Y = 17;
const Vector3 BELFRY_DOOR{ BELFRY_X + 2.05f, BELFRY_DOOR_Y, BELFRY_Z };
constexpr float BAT_FLAPS = 5.5f;            // wingbeats a second
constexpr float BAT_SIZE = 2.4f;
const std::vector<Vector3> BARRICADE = { { -8.0f, 0, 34 }, { 0.0f, 0, 35 }, { 8.0f, 0, 34 } };
const Vector3 LAMP{ 6.5f, 0, 12 };

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

crowd::World makeZombies() {
    crowd::World w;
    w.from = -12;                                // a pre-roll: the horde is under way at frame 0
    w.to = TOTAL / (double)FPS + 0.2;
    w.boundsMin = { -40, 0, -60 };
    w.boundsMax = { 40, 5, 200 };
    w.seed = 21;
    for (const Vector3& p : BARRICADE) w.obstacles.push_back({ p, 1.45f });
    w.obstacles.push_back({ LAMP, 0.4f });

    // Eight in front: the ones the first beat is about.
    crowd::Group hero;
    hero.name = "hero";
    hero.count = HEROES;
    hero.spawn = { 0, 0, 19 };
    hero.spawnSize = { 7, 0, 5 };
    hero.heading0 = { 0, 0, -1 };
    hero.headingSpread = 12;
    hero.cruise = 0.95f;
    hero.maxSpeed = 1.3f;
    hero.maxAccel = 1.2f;
    hero.maxTurn = 90;
    hero.steer.personal = 1.0f;
    hero.steer.wander = 0.12f;
    hero.goal = [](double, int i) { return Vector3{ (float)(i % 4 - 1.5f) * 2.0f, 0, -40 }; };
    w.groups.push_back(hero);

    // The horde behind them, through the gaps of the barricade.
    crowd::Group horde;
    horde.name = "horde";
    horde.count = HORDE;
    horde.spawn = { 0, 0, 64 };
    horde.spawnSize = { 20, 0, 40 };
    horde.heading0 = { 0, 0, -1 };
    horde.headingSpread = 15;
    horde.cruise = 0.8f;
    horde.speedJitter = 0.25f;
    horde.maxSpeed = 1.5f;
    horde.maxAccel = 1.0f;
    horde.maxTurn = 80;
    horde.steer.personal = 0.5f;
    horde.steer.neighbour = 2.5f;
    horde.steer.wander = 0.2f;
    horde.rest = 0.12f;
    horde.goal = [](double, int i) { return Vector3{ (float)(i % 9 - 4) * 2.3f, 0, -40 }; };
    w.groups.push_back(horde);
    return w;
}

// Bats are baked apart, from the moment they leave: before it they sit, hidden, in the belfry.
crowd::World makeBats() {
    crowd::World w;
    w.from = BAT_GO - 0.4;
    w.to = TOTAL / (double)FPS + 0.2;
    w.boundsMin = { -60, 2, -40 };
    w.boundsMax = { 60, 40, 120 };
    w.seed = 33;
    crowd::Group bats;
    bats.name = "bats";
    bats.medium = crowd::Medium::Air;
    bats.count = BATS;
    bats.spawn = { BELFRY_X + 0.6f, BELFRY_DOOR_Y, BELFRY_Z };
    bats.spawnSize = { 1.4f, 1.0f, 1.4f };
    bats.heading0 = { 1, 0, 0 };
    bats.headingSpread = 40;
    bats.cruise = 9;
    bats.maxSpeed = 13;
    bats.maxAccel = 10;
    bats.maxTurn = 140;
    bats.height0 = 6;
    bats.height1 = 28;
    bats.steer.personal = 1.7f;
    bats.steer.neighbour = 7;
    bats.steer.cohesion = 0.6f;
    bats.steer.alignment = 1.3f;
    // The goal runs ahead of the swarm round a circle that takes in the belfry and the street.
    bats.goal = [](double s, int) {
        const double a = 3.14159 + (s - BAT_GO) * 0.42 + 0.9;
        return Vector3{ 0 + 19.f * (float)std::cos(a), 19.f + 2.5f * (float)std::sin(a * 1.7), 38 + 19.f * (float)std::sin(a) };
    };
    w.groups.push_back(bats);
    return w;
}

struct Bodies {
    std::vector<crowd::Baked> zombies;
    crowd::Baked bat;
};
// Four zombies, one each of the looks `zombie(0..3)`, baked with the same shamble the heroes
// play. Built once; nothing here touches GL.
const Bodies& bodies() {
    static const Bodies b = [] {
        Bodies out;
        constexpr float CADENCE = 1;
        for (int v = 0; v < VARIANTS; v++) {
            const rig::Look look = crowd::zombie(v);
            const float stride = rig::walkSpeed(look.build, CADENCE);
            rig::Clip idle = [](double s) {
                rig::Pose p = rig::clips::shamble(1)(0);
                for (rig::Joint j : { rig::Joint::HipL, rig::Joint::HipR, rig::Joint::KneeL, rig::Joint::KneeR, rig::Joint::AnkleL, rig::Joint::AnkleR })
                    p[j] = { 0, 0, 0 };
                p.offset = { 0, -0.02f, 0 };
                const float ph = 2 * PI * (float)s / 2;
                p[rig::Joint::Head].z += 5 * std::sin(ph);
                p[rig::Joint::ShoulderL].x += 5 * std::sin(ph + 0.8f);
                p[rig::Joint::ShoulderR].x += 5 * std::sin(ph + 2.0f);
                return p;
            };
            out.zombies.push_back(crowd::bakeWalker(look, rig::clips::shamble(CADENCE), 1.0 / CADENCE, idle, 2, stride));
        }
        out.bat = crowd::bakeBat(12);
        return out;
    }();
    return b;
}

struct World {
    crowd::CrowdPtr zombies, bats;
    int ringed = 0;                              // the horde zombie the strip follows (index in the horde)
    int ringedBat = 0;
};

std::shared_ptr<World> bakeWorld() {
    auto w = std::make_shared<World>();
    w->zombies = crowd::bake(makeZombies());
    w->bats = crowd::bake(makeBats());
    // The ringed zombie: walking, near the middle of the street at 7.5 s, closest to the lens.
    float best = 1e9f;
    for (int i = 0; i < HORDE; i++) {
        const crowd::State st = w->zombies->at(w->zombies->first("horde") + i, 7.5);
        const float score = std::fabs(st.pos.x) * 2 + std::fabs(st.pos.z - 22);
        if (st.speed > 0.45f && score < best) { best = score; w->ringed = i; }
    }
    // The ringed bat: the one nearest the middle of the swarm at 13.5 s.
    Vector3 mid{};
    for (int i = 0; i < BATS; i++) mid = Vector3Add(mid, w->bats->at(i, 13.5).pos);
    mid = Vector3Scale(mid, 1.f / BATS);
    best = 1e9f;
    for (int i = 0; i < BATS; i++) {
        const float d = Vector3Distance(w->bats->at(i, 13.5).pos, mid);
        if (d < best) { best = d; w->ringedBat = i; }
    }
    return w;
}

// ------------------------------------------------------------------ camera
struct Cam { Vector3 pos, tgt; float fov, far; };
Cam camAt(double s) {
    const Cam low{ { 0, 1.25f, -3 }, { 0, 1.45f, 14 }, 38, 31 };
    const Cam high{ { 2, 9, -8 }, { 0, 1, 32 }, 44, 301 };
    const Cam high2{ { -2, 10, -6 }, { 0, 1, 30 }, 44, 301 };
    const Cam belfry{ { 7, 3.5f, -2 }, { -16, 13, 36 }, 58, 300 };
    const Cam belfry2{ { 5, 5, 0 }, { -10, 15, 34 }, 58, 300 };
    const Cam end{ { 0, 8, -18 }, { 0, 1, 28 }, 40, 300 };
    auto mix = [](const Cam& a, const Cam& b, float k) {
        return Cam{ Vector3Lerp(a.pos, b.pos, k), Vector3Lerp(a.tgt, b.tgt, k), a.fov + (b.fov - a.fov) * k, a.far + (b.far - a.far) * k };
    };
    if (s < CRANE_FROM) return low;
    if (s < CRANE_TO) {
        // Up and back; the far plane opens after the camera has risen (the fog hides the edge).
        Cam c = mix(low, high, smooth((s - CRANE_FROM) / (CRANE_TO - CRANE_FROM)));
        // The far plane opens slowly, so the counter climbs as the mass comes in range; the fog
        // ends just short of it, so the edge never shows.
        c.far = low.far + 70 * smooth((s - CRANE_FROM) / 2.4);
        return c;
    }
    if (s < SWARM_AT - 0.8) {
        Cam c = mix(high, high2, smooth((s - CRANE_TO) / (SWARM_AT - 0.8 - CRANE_TO)));
        c.far = 101 + 200 * smooth((s - CRANE_TO) / 1.2);
        return c;
    }
    if (s < SWARM_AT + 0.8) return mix(high2, belfry, smooth((s - (SWARM_AT - 0.8)) / 1.6));
    if (s < END_AT - 0.8) return mix(belfry, belfry2, smooth((s - (SWARM_AT + 0.8)) / (END_AT - 1.6 - SWARM_AT)));
    if (s < END_AT + 0.8) return mix(belfry2, end, smooth((s - (END_AT - 0.8)) / 1.6));
    return end;
}
g3d::Camera camera(double s) {
    const Cam c = camAt(s);
    g3d::Camera cam;
    cam.position = c.pos;
    cam.target = c.tgt;
    cam.fov = c.fov;
    cam.farPlane = c.far;
    return cam;
}
bool screenOf(double s, Vector3 p, Vector2& out) { return g3d::project(camera(s), p, { 0, 0, W, H }, out); }

// ------------------------------------------------------------------ the street
// Windows are instances too: one baked quad, 1500 of them, lit.
std::shared_ptr<g3d::AnimatedMesh> windowMesh() {
    auto am = std::make_shared<g3d::AnimatedMesh>();
    constexpr float hw = 0.4f, hh = 0.6f;
    am->frames = 1;
    am->vertices = 4;
    am->positions = { -hw, 0, -hh, hw, 0, -hh, hw, 0, hh, -hw, 0, hh };
    am->normals = { 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0 };
    am->radius = 1.0f;
    am->rest = std::make_shared<g3d::Geometry>([p = am->positions, n = am->normals] {
        Mesh m{};
        m.vertexCount = 4;
        m.triangleCount = 2;
        m.vertices = (float*)MemAlloc(sizeof(float) * 12);
        m.normals = (float*)MemAlloc(sizeof(float) * 12);
        m.texcoords = (float*)MemAlloc(sizeof(float) * 8);
        m.indices = (unsigned short*)MemAlloc(sizeof(unsigned short) * 6);
        std::copy(p.begin(), p.end(), m.vertices);
        std::copy(n.begin(), n.end(), m.normals);
        const float uv[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
        std::copy(uv, uv + 8, m.texcoords);
        const unsigned short ix[6] = { 0, 2, 1, 0, 3, 2 };
        std::copy(ix, ix + 6, m.indices);
        UploadMesh(&m, true);
        return m;
    });
    return am;
}

std::shared_ptr<g3d::Scene> makeScene(const std::shared_ptr<World>& w) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->sky.enabled = true;
    scene->sky.zenith = SKY_TOP;
    scene->sky.horizon = SKY_LOW;
    scene->sky.ground = SKY_LOW;
    scene->sky.sunSize = 0;
    scene->sky.sunGlow = 0;
    scene->sky.haze = mkColor(0x8a5a62);
    scene->sky.hazeAmount = 0.4f;
    scene->sky.stars = 0.5f;
    scene->sky.lighting = 0.5f;
    scene->skyColor = SKY_LOW;
    scene->groundColor = { 60, 52, 66, 255 };
    scene->ambient = 0.8f;
    scene->fogColor = SKY_LOW;
    scene->fogNear = 10;
    scene->fogFar = 26;
    // The fog opens with the crane: close at the low camera (it hides the horde's far end and
    // the clipped edge), far from above.
    scene->up = [](int t, g3d::Scene& sc) {
        const double s = (double)t / FPS;
        const float k = smooth((s - CRANE_FROM) / (CRANE_TO - CRANE_FROM));
        sc.fogFar = std::min(camAt(s).far - 4, 26 + 150 * k);
        sc.fogNear = std::min(10 + 50 * k, sc.fogFar * 0.5f);
    };
    scene->shadowMapSize = 4096;
    scene->cullFloor = 0;
    // A low red sun from behind the buildings on the left, a cold sky fill from the right.
    g3d::Light key = g3d::Light::directional({ -22, 16, 14 }, { 255, 176, 130, 255 }, 2.1f, true);
    key.up = [](int t, g3d::Light& l) {
        const Cam c = camAt(t / (double)FPS);
        l.target = { c.tgt.x * 0.5f, 0, c.tgt.z * 0.6f + 8 };
        l.position = Vector3Add(l.target, { -22, 16, 14 });
        l.shadowExtent = t / (double)FPS < CRANE_FROM ? 22 : 44;
    };
    scene->lights.push_back(key);
    scene->lights.push_back(g3d::Light::directional({ 8, 10, -6 }, { 120, 140, 210, 255 }, 0.5f));
    scene->lights.push_back(g3d::Light::directional({ 0, 5, -12 }, { 255, 214, 190, 255 }, 0.85f));   // from the camera's side: faces
    scene->lights.push_back(g3d::Light::point({ LAMP.x, 5.1f, LAMP.z }, { 255, 190, 110, 255 }, 1.6f, 16));
    scene->camera.up = [](int t, g3d::Camera& cam) {
        const Cam c = camAt(t / (double)FPS);
        cam.position = c.pos;
        cam.target = c.tgt;
        cam.fov = c.fov;
        cam.farPlane = c.far;
    };

    g3d::Material ground;
    ground.color = mkColor(0x403b4a);
    ground.roughness = 0.95f;
    ground.castShadow = false;
    scene->add(g3d::Geometry::plane(500, 500), ground);
    g3d::Material walk = ground;
    walk.color = mkColor(0x5a5266);
    for (int side : { -1, 1 }) {
        scene->add(g3d::Geometry::plane(16, 400), walk)->position = { (STREET_HALF + 8) * side, 0.01f, 90 };
    }
    g3d::Material lane = ground;
    lane.color = mkColor(0x8d8576);
    for (int i = 0; i < 24; i++) scene->add(g3d::Geometry::plane(0.25f, 3), lane)->position = { 0, 0.012f, -6.f + 6.f * i };

    // Buildings down both sides, and their lit windows.
    g3d::Material wall;
    wall.roughness = 0.9f;
    auto windows = windowMesh();
    auto left = std::make_shared<g3d::Instances>(), right = std::make_shared<g3d::Instances>();
    left->mesh = right->mesh = windows;
    for (int side : { -1, 1 }) {
        float z = -14;
        for (int k = 0; z < 130; k++) {
            const unsigned long long id = (unsigned long long)(k * 2 + (side > 0));
            const float depth = 9 + 6 * (float)rnd01(41, id), height = 9 + 13 * (float)rnd01(42, id);
            const float width = 12 + 12 * (float)rnd01(43, id);
            if (!(side < 0 && z + width > 28 && z < 46)) {          // the belfry stands where the left side has a gap
                const unsigned char shade = (unsigned char)(52 + 26 * rnd01(44, id));
                wall.color = { shade, (unsigned char)(shade - 4), (unsigned char)(shade + 8), 255 };
                const float x = (STREET_HALF + 2 + depth / 2) * (float)side;
                scene->add(g3d::Geometry::box(depth, height, width), wall)->position = { x, height / 2, z + width / 2 };
                // Windows on the face towards the street.
                for (int row = 0; row < (int)(height / 3.2f); row++)
                    for (int col = 0; col < (int)(width / 3.0f); col++) {
                        if (rnd01(45, id * 977 + (unsigned long long)(row * 31 + col)) < 0.62) continue;
                        g3d::Instance in;
                        in.position = { (STREET_HALF + 2 - 0.05f) * (float)side, 2.6f + 3.2f * row, z + 1.8f + 3.0f * col };
                        in.bank = side < 0 ? -90.f : 90.f;
                        in.shade = 0.7f + 0.6f * (float)rnd01(46, id * 1013 + (unsigned long long)(row * 17 + col));
                        (side < 0 ? left : right)->list.push_back(in);
                    }
            }
            z += width + 0.6f;
        }
    }
    g3d::Material glow;
    glow.color = AMBER;
    glow.unlit = true;
    glow.castShadow = false;
    scene->add(left, glow);
    scene->add(right, glow);

    // The belfry: a stone tower with a dark door that glows, and a roof.
    g3d::Material stone;
    stone.color = mkColor(0x3a3442);
    stone.roughness = 0.9f;
    scene->add(g3d::Geometry::box(4, 20, 4), stone)->position = { BELFRY_X, 10, BELFRY_Z };
    scene->add(g3d::Geometry::cone(3.4f, 5.5f, 4), stone)->position = { BELFRY_X, 22.7f, BELFRY_Z };
    g3d::Material hole = glow;
    hole.color = mkColor(0x2a1a14);
    g3d::Object3D* door = scene->add(g3d::Geometry::box(0.2f, 2.8f, 1.8f), hole);
    door->position = { BELFRY_X + 2.0f, BELFRY_DOOR_Y, BELFRY_Z };
    g3d::Material light = glow;
    scene->add(g3d::Geometry::box(0.1f, 2.4f, 1.4f), light)->up = [](int t, g3d::Object3D& o) {
        o.position = { BELFRY_X + 2.06f, BELFRY_DOOR_Y, BELFRY_Z };
        o.visible = t / (double)FPS < BAT_GO - 0.3;     // the glow goes out as the bats leave
    };

    // A street lamp and the barricade: cars nose to tail across the street.
    g3d::Material iron;
    iron.color = mkColor(0x1f1d26);
    iron.roughness = 0.5f;
    iron.metalness = 0.5f;
    scene->add(g3d::Geometry::cylinder(0.12f, 5.2f), iron)->position = { LAMP.x, 2.6f, LAMP.z };
    scene->add(g3d::Geometry::sphere(0.35f, 12, 16), glow)->position = { LAMP.x, 5.3f, LAMP.z };
    for (size_t i = 0; i < BARRICADE.size(); i++) {
        g3d::Material car;
        const unsigned char c = (unsigned char)(50 + 30 * rnd01(47, i));
        car.color = { c, (unsigned char)(c - 10), (unsigned char)(c + 6), 255 };
        car.roughness = 0.5f;
        g3d::Object3D* body = scene->add(g3d::Geometry::box(2.1f, 1.3f, 4.4f), car);
        body->position = { BARRICADE[i].x, 0.75f, BARRICADE[i].z };
        body->rotation = { 0, 70.f + 40.f * (float)rnd01(48, i), 0 };
        g3d::Object3D* cab = scene->add(g3d::Geometry::box(1.8f, 0.7f, 2.2f), car);
        cab->position = { BARRICADE[i].x, 1.75f, BARRICADE[i].z };
        cab->rotation = body->rotation;
    }

    // The cast. The heroes are full characters with faces, walking the shamble the baked
    // bodies were made from; the rest are instances; the bats one more set.
    const crowd::CrowdPtr c = w->zombies;
    const int h0 = c->first("hero");
    for (int i = 0; i < HEROES; i++) {
        rig::Look look = crowd::zombie(i);
        look.build = look.build.scaled(c->sizeFactor(h0 + i));
        auto ch = std::make_shared<rig::Character>(look);
        const float stride = rig::walkSpeed(look.build, 1);
        const rig::Clip shamble = rig::clips::shamble(1);
        ch->motion.play([c, h0, i, stride, shamble](double s) { return shamble(c->travelled(h0 + i, s) / stride); }, 0, 0);
        crowd::add(*scene, c, h0 + i, ch, FPS);
    }
    g3d::Material skin;
    skin.roughness = 0.9f;
    crowd::addInstanced(*scene, c, "horde", bodies().zombies, skin, FPS);
    g3d::Material fur;
    fur.roughness = 0.7f;
    g3d::Object3D* swarm = crowd::addFlock(*scene, w->bats, "bats", bodies().bat, BAT_FLAPS, fur, FPS);
    // Before they leave, the bats are in the belfry; they are not drawn until the glow goes out.
    const auto inner = swarm->up;
    swarm->up = [inner](int t, g3d::Object3D& o) {
        inner(t, o);
        for (g3d::Instance& in : o.instances->list) in.scale *= BAT_SIZE;   // a bat has to read from 30 m
        o.visible = t / (double)FPS >= BAT_GO - 0.3;
    };
    return scene;
}

// ------------------------------------------------------------------ the proof overlays
int bodiesInFrame(const World& w, double s) {
    const g3d::Camera cam = camera(s);
    int n = 0;
    auto seen = [&](Vector3 p, float r) { return g3d::visibleSphere(cam, W / H, p, r); };
    const int h0 = w.zombies->first("hero"), z0 = w.zombies->first("horde");
    for (int i = 0; i < HEROES; i++) n += seen(Vector3Add(w.zombies->at(h0 + i, s).pos, { 0, 0.95f, 0 }), 1.1f);
    for (int i = 0; i < HORDE; i++) n += seen(Vector3Add(w.zombies->at(z0 + i, s).pos, { 0, 0.95f, 0 }), 1.1f);
    if (s >= BAT_GO - 0.3)
        for (int i = 0; i < BATS; i++) n += seen(w.bats->at(i, s).pos, 0.4f);
    return n;
}

void ringAt(const Ctx& cx, double s, Vector3 p, float r, float width, Color col) {
    constexpr int N = 60;
    Vector2 prev{};
    bool had = false;
    for (int i = 0; i <= N; i++) {
        const float a = (float)i / N * 2 * PI;
        Vector2 q;
        const bool ok = screenOf(s, { p.x + r * std::cos(a), p.y, p.z + r * std::sin(a) }, q);
        if (ok && had) dr::strokeSeg(cx, prev, q, width, col);
        prev = q;
        had = ok;
    }
}

// A band across the bottom with `n` poses in a row and a pointer at `phase` (0..1 along it).
void stripBand(const Ctx& cx, float k, float phase, int n, float spacing, const std::function<void(const Ctx&, int, Vector2)>& pose) {
    const float x0 = W / 2 - spacing * n / 2, y0 = 770, h = 270;
    dr::rect(faded(cx, k * 0.78f), 0, y0, W, h, withAlpha(SKY_TOP, 255));
    const Ctx m = faded(cx, k);
    for (int i = 0; i < n; i++) pose(m, i, { x0 + spacing * (i + 0.5f), y0 + h - 34 });
    // The pointer: a bar along the band's foot and a marker at the pose's own moment.
    const float px = x0 + spacing * n * phase;
    dr::rect(m, x0, y0 + h - 14, spacing * n, 3, withAlpha(BONE, 90));
    const Vector2 tri[3] = { { px, y0 + h - 16 }, { px - 12, y0 + h - 2 }, { px + 12, y0 + h - 2 } };
    dr::fillPoly(m, tri, 3, SICK);
}

void proofs(const Ctx& cx, const World& w, double s) {
    // Beat 2: one zombie ringed, its pointer riding the strip of baked poses.
    if (const float k = window(s, STRIP_FROM, SWARM_AT - 0.4); k > 0.001f) {
        const crowd::Baked& body = bodies().zombies[0];
        const crowd::Cycle cy = crowd::walkCycle(w.zombies, "horde", w.ringed, s, body);
        static std::vector<rig::CharacterPtr> chars;
        if (chars.empty()) {
            for (int i = 0; i < 8; i++) {
                auto ch = std::make_shared<rig::Character>(crowd::zombie(0));
                ch->motion.play(rig::clips::hold(rig::clips::shamble(1)(i / 8.0)), 0, 0);
                chars.push_back(ch);
            }
        }
        stripBand(cx, k, cy.still ? 0.f : cy.phase, 8, 150, [&](const Ctx& m, int i, Vector2 at) {
            rig::View2D v;
            v.yaw = 90;
            v.scale = 118;
            v.origin = at;
            v.ink = BONE;
            v.outline = 2.5f;
            v.shadow = BLANK;
            rig::draw(m, *chars[(size_t)i], 0, v);
        });
        if (const float kr = window(s, ZOMBIE_RING_FROM, SWARM_AT - 0.4); kr > 0.001f) {
            const crowd::State st = w.zombies->at(w.zombies->first("horde") + w.ringed, s);
            ringAt(faded(cx, kr), s, { st.pos.x, 0.04f, st.pos.z }, 1.5f, 5, SICK);
        }
    }
    // Beat 3: one bat ringed (a ring of light round it), its pointer on a strip of wingbeats.
    if (const float k = window(s, BAT_STRIP_FROM, END_AT - 0.4); k > 0.001f) {
        const float phase = crowd::flapCycle(w.bats, "bats", w.ringedBat, s, BAT_FLAPS);
        stripBand(cx, k, phase, 6, 190, [&](const Ctx& m, int i, Vector2 at) {
            const float ph = (float)i / 6, lift = 52 * DEG2RAD * std::sin(2 * PI * ph), lag = 52 * DEG2RAD * std::sin(2 * PI * ph - 0.9f) - 0.18f;
            const float u = 330;                                         // px per metre
            const Vector2 c{ at.x, at.y - 120 };
            for (int side = -1; side <= 1; side += 2) {
                const float sx = (float)side;
                const Vector2 sh{ c.x + sx * 0.03f * u, c.y }, wr{ sh.x + sx * 0.12f * u * std::cos(lift), sh.y - 0.12f * u * std::sin(lift) };
                const Vector2 t1{ wr.x + sx * 0.19f * u * std::cos(lag), wr.y - 0.19f * u * std::sin(lag) };
                const Vector2 t3{ wr.x + sx * 0.12f * u * std::cos(lag) * 0.4f, wr.y - 0.12f * u * std::sin(lag) + 0.1f * u };
                const Vector2 hip{ c.x + sx * 0.028f * u, c.y + 0.06f * u };
                const Vector2 poly[4] = { sh, wr, t1, t3 };
                dr::fillPoly(m, poly, 4, withAlpha(BONE, 70));
                dr::strokeSeg(m, sh, wr, 3, BONE);
                dr::strokeSeg(m, wr, t1, 3, BONE);
                dr::strokeSeg(m, t3, hip, 2, withAlpha(BONE, 160));
            }
            dr::circle(m, c, 0.034f * u, BONE);
        });
        if (const float kr = window(s, BAT_STRIP_FROM + 0.6, END_AT - 0.4); kr > 0.001f) {
            const crowd::State st = w.bats->at(w.ringedBat, s);
            Vector2 q;
            if (screenOf(s, st.pos, q)) dr::ring(faded(cx, kr), q, 34, 4, SICK);
        }
    }
}

struct Label { double from, to; const char* name; const char* rules; };
const Label LABELS[] = {
    { 0.6, 4.8, "horde", "8 bodies" },
    { 5.2, 10.8, "horde", "500 \xC2\xB7 4 bodies \xC2\xB7 1 strip" },
    { 11.4, 15.8, "swarm", "1000 bats" },
};

void overlays(const Ctx& cx, const World& w, int t) {
    const double s = t / (double)FPS;
    proofs(cx, w, s);
    // A dark wash behind the text corners, so a window or a lamp never sits under a word.
    const float kw = window(s, 0.0, END_AT - 0.2);
    const Color dark = withAlpha(SKY_TOP, (unsigned char)(150 * kw)), clear = withAlpha(SKY_TOP, 0);
    dr::rectGrad4(cx, 0, 0, 760, 330, dark, clear, clear, dark);
    dr::rectGrad4(cx, W - 520, 0, 520, 220, clear, dark, dark, clear);
    const float kk = window(s, 0.3, END_AT - 0.2);
    text(faded(cx, kk), "HORDE", { 96, 92 }, 26, SICK, 0, 0.5f, true, 6);
    text(faded(cx, kk), "baked once, sampled per frame", { 96, 128 }, 26, DIM, 0, 0.5f);
    for (const Label& l : LABELS) {
        const float a = window(s, l.from, l.to);
        if (a <= 0.001f) continue;
        Ctx m = faded(cx, a);
        m.o.y += (1 - a) * 14 * m.s;
        text(m, l.name, { 96, 190 }, 84, BONE, 0, 0.5f, true);
        dr::text(code::font(false), m, l.rules, { 100, 262 }, 30, 0, SICK, 0, 0.5f);
    }
    // The counter: every body the camera can see, by the renderer's own sphere test.
    const float kc = window(s, 0.6, END_AT - 0.2);
    if (kc > 0.001f) {
        const Ctx m = faded(cx, kc);
        text(m, std::to_string(bodiesInFrame(w, s)), { W - 96, 100 }, 64, BONE, 1, 0.5f, true);
        text(m, "bodies in frame", { W - 96, 150 }, 26, DIM, 1, 0.5f);
    }
}

void endCard(const Ctx& cx, int t) {
    const float kt = smooth(t / 14.0), ka = smooth((t - 18) / 14.0);
    Ctx tm = faded(cx, kt);
    tm.o.y += (1 - kt) * 20 * tm.s;
    const Color clear = withAlpha(SKY_TOP, 0);
    dr::rectGrad4(faded(cx, kt), 0, 700, W, 140, clear, clear, SKY_TOP, SKY_TOP);
    dr::rect(faded(cx, kt), 0, 840, W, 240, SKY_TOP);
    text(tm, "500 + 1000. ONE STRIP EACH.", { W / 2, 868 }, 72, BONE, 0.5f, 0.5f, true, 2);
    dr::text(code::font(false), faded(cx, ka), "crowd::bake  \xC2\xB7  crowd::addInstanced", { W / 2, 946 }, 30, 0, SICK, 0.5f, 0.5f);
}

// ------------------------------------------------------------------ sound (brief section 7)
using procgen::Score;
constexpr int RATE = Score::RATE;

struct Heard { float gain, pan; };
Heard heard(double s, Vector3 p, float near) {
    const Cam c = camAt(s);
    const float d = Vector3Distance(c.pos, p);
    Vector2 q;
    const bool seen = screenOf(s, p, q) && q.x > -100 && q.x < W + 100 && q.y > -100 && q.y < H + 100;
    return { std::min(1.f, near / std::max(d, 0.1f)) * (seen ? 1.f : 0.3f),
             seen ? std::clamp(0.5f + 0.7f * (q.x / W - 0.5f), 0.15f, 0.85f) : 0.5f };
}
template <class F> void strides(const crowd::Crowd& c, int a, double from, double to, double step, F hit) {
    const double phase = rnd01(23, (unsigned long long)a) * step;
    double next = std::floor((c.travelled(a, from) - phase) / step + 1) * step + phase;
    for (double s = from; s < to; s += 1.0 / 120)
        if (c.travelled(a, s) >= next) { hit(s); next += step; }
}
// A dragging footfall: a low scuff with a little wet grit.
void shuffle(Score& sc, double t0, float amp, float pan) {
    const int a = (int)(t0 * RATE), len = (int)(0.14 * RATE);
    procgen::Svf f, g;
    for (int k = 0; k < len; k++) {
        const double t = (double)k / RATE;
        const float env = (float)(std::min(1.0, t / 0.004) * std::exp(-t * 34));
        const float n = sc.noise();
        sc.put(a + k, (f.bp(n, 380, 0.7f, RATE) + 0.5f * g.bp(n, 1500, 0.5f, RATE)) * env * amp, pan, 0.05f, 0);
    }
}
// A groan: a buzzy source through two formants that glide, swelling and sinking in pitch.
void groan(Score& sc, double t0, double dur, float hz, float amp, float pan) {
    const int a = (int)(t0 * RATE), len = (int)(dur * RATE);
    procgen::Svf f1, f2;
    double ph = 0;
    for (int k = 0; k < len; k++) {
        const float u = (float)k / len;
        const float env = std::sin(PI * std::pow(u, 0.7f)) * std::min(1.f, u * 8);
        ph += 2 * PI * hz * (1.15f - 0.35f * u) * (1 + 0.02f * std::sin(k * 0.0009f)) / RATE;
        const float src = (float)(std::fmod(ph / (2 * PI), 1.0) * 2 - 1) * 0.6f + 0.3f * sc.noise();
        const float v = f1.bp(src, 520 - 140 * u, 0.9f, RATE) + 0.6f * f2.bp(src, 980 + 220 * u, 0.9f, RATE);
        sc.put(a + k, v * env * amp, pan, 0.2f, 0);
    }
}
void thump(Score& sc, double t0, float amp) {
    const int a = (int)(t0 * RATE), len = (int)(0.3 * RATE);
    double ph = 0;
    for (int k = 0; k < len; k++) {
        const double t = (double)k / RATE;
        ph += 2 * PI * (48 + 30 * std::exp(-t * 30)) / RATE;
        sc.put(a + k, (float)(std::sin(ph) * std::exp(-t * 14)) * amp, 0.5f, 0.02f, 0);
    }
}

void genScore(const std::string& path, const World& w) {
    const crowd::Crowd& c = *w.zombies;
    Score s(TOTAL / (double)FPS + 1);
    // The bed: D minor and a low fifth for the street; it drops out half a second before the
    // bats leave, so the stinger lands; the choir pad of the swarm; D major on the card.
    for (int n : { 50, 57, 62 }) s.pad(0, SWARM_AT - 0.2, n, 0.06f, 700, 0.4f);
    s.bass(0, SWARM_AT - 0.4, 26, 0.12f);
    for (int n : { 53, 57, 62 }) s.pad(SWARM_AT, BAT_GO - 0.7 - SWARM_AT, n, 0.05f, 600, 0.5f);
    s.riser(BAT_GO - 0.7, BAT_GO, 0.12f);
    s.impact(BAT_GO, 0.2f);
    for (int n : { 57, 60, 65, 69 }) s.pad(BAT_GO, END_AT - BAT_GO, n, 0.06f, 1100, 0.3f + 0.15f * (n % 3));
    s.bass(BAT_GO, END_AT - BAT_GO - 0.2, 29, 0.12f);
    s.impact(END_AT + 0.1, 0.26f);
    for (int j = 0; j < 4; j++) s.bell(END_AT + 0.15 + 0.12 * j, 74 + j * 3, 0.06f, 0.3f + 0.12f * j);
    for (int n : { 50, 57, 62, 66 }) s.pad(END_AT, TOTAL / (double)FPS - END_AT, n, 0.07f, 1300, 0.5f);
    s.pluck(0.6, 62, 0.05f, 0.4f, 4);
    s.pluck(STRIP_FROM, 69, 0.05f, 0.6f, 4);

    // A heartbeat that quickens as the horde closes (lub-dub), until the bats take over.
    for (double t = 0.4, bpm = 52; t < BAT_GO - 0.8; t += 60 / bpm) {
        bpm = 52 + 30 * smooth(t / 10);
        thump(s, t, 0.17f);
        thump(s, t + 0.2, 0.11f);
    }
    // Steps: the heroes and every horde zombie the lens can hear.
    auto steps = [&](const char* group, int count, float near, float amp) {
        const int a0 = c.first(group);
        for (int k = 0; k < count; k++)
            strides(c, a0 + k, 0, END_AT + 1, 0.8, [&](double t) {
                const crowd::State st = c.at(a0 + k, t);
                if (st.speed < 0.2f) return;
                const Heard h = heard(t, st.pos, near);
                if (h.gain < 0.03f) return;
                shuffle(s, t, amp * h.gain, h.pan);
            });
    };
    steps("hero", HEROES, 10, 0.1f);
    steps("horde", HORDE, 14, 0.06f);
    // Groans from the heroes, and from the ringed zombie as the ring lands.
    const int h0 = c.first("hero");
    for (int k = 0; k < HEROES; k++)
        for (double t = 0.8 + 0.9 * rnd01(51, (unsigned long long)k); t < SWARM_AT - 0.5; t += 3.1 + 2.5 * rnd01(52, (unsigned long long)(k * 7 + (int)t))) {
            const Heard h = heard(t, c.at(h0 + k, t).pos, 8);
            if (h.gain > 0.05f) groan(s, t, 1.1, 82 + 18 * (float)rnd01(53, (unsigned long long)k), 0.1f * h.gain, h.pan);
        }
    const Heard hr = heard(ZOMBIE_RING_FROM + 0.3, c.at(c.first("horde") + w.ringed, ZOMBIE_RING_FROM + 0.3).pos, 12);
    groan(s, ZOMBIE_RING_FROM + 0.3, 1.2, 95, 0.12f * std::max(0.4f, hr.gain), hr.pan);

    // The swarm: a flutter bed that follows how many bats are in the air and in view.
    {
        const int a = (int)(BAT_GO * RATE), len = (int)((TOTAL / (double)FPS - BAT_GO) * RATE);
        std::vector<float> level;
        for (double t = BAT_GO; t < TOTAL / (double)FPS; t += 0.1) level.push_back(std::sqrt(bodiesInFrame(w, t) / 1500.f));
        procgen::Svf f, g;
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            const size_t i = std::min(level.size() - 1, (size_t)(t / 0.1));
            const float lv = level[i] * std::min(1.f, (float)t * 3) * (END_AT - BAT_GO > t ? 1.f : 0.5f);
            const float flutter = 0.55f + 0.45f * std::sin(2 * PI * BAT_FLAPS * (float)t);
            const float n = s.noise();
            s.put(a + k, (f.bp(n, 3200 + 600 * std::sin((float)t * 1.3f), 0.5f, RATE) + 0.5f * g.bp(n, 6800, 0.6f, RATE)) * lv * flutter * 0.05f,
                  0.5f + 0.3f * std::sin((float)t * 0.7f), 0.3f, 0);
        }
    }

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
        dr::rect(cx, 0, 0, W, H, SKY_LOW);
        g3d::render(*scene, t, cx);
        overlays(cx, *w, t);
    };
    const int e = (int)(END_AT * FPS);
    addRoot(c.roots, e, TOTAL - e)->fn = [](const Ctx& cx, int t) { endCard(cx, t); };
    TNode* logo = prefab::use(c.roots, e + 28, TOTAL - e - 28, "raymotion-logo",
                              Json::parse(R"({"size": 56, "layout": "horizontal", "tagline": "video as code", "delay": 2, "ink": "#e9e4d4", "accent": "#8fa77a"})"));
    logo->props.pos = { W / 2 - 150, 1020 };
    addRoot(c.roots, e + 36, TOTAL - e - 36)->fn = [](const Ctx& cx, int t) {
        dr::text(code::font(false), faded(cx, smooth(t / 12.0)), "github.com/pebaryan/raymotion", { W / 2 - 30, 1020 }, 26, 0, DIM, 0, 0.5f);
    };
    media::audio(addRoot(c.roots, 0, TOTAL), ASSETS + "/score.wav");
}

} // namespace

void registerZombieHorde(Registry& out) {
    Comp c;
    c.name = "zombie-horde";
    c.w = (int)W; c.h = (int)H; c.fps = FPS;
    c.duration = TOTAL;
    c.background = SKY_LOW;
    c.assets = [](const Comp&, const Json&) { ensureAssets(); };
    c.build = [](Comp& c, const Json& p) { build(c, p); };
    out.push_back(std::move(c));
}

} // namespace rmtn
