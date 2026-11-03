// bundaran_hi_banner.cpp -- "bundaran-hi-banner": a 24 s illustrated film at the Selamat Datang
// Monument (Bundaran HI, Jakarta). Students in yellow and blue jackets face a police shield line
// across a strip of road; two of them climb the monument carrying a rolled banner and let it
// unroll down the plinth. The banner is physics::Cloth: its rows are held on a spiral roll and
// released one by one, the roll sinking as it pays out.
//
//   raymotion render bundaran-hi-banner
//
// Illustration, not documentary: see docs/bundaran-hi-banner-brief.md (sources and what is
// invented) and docs/bundaran-hi-banner-qc.md.
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <cstring>
#include <string>
#include <tuple>
#include <vector>

namespace rmtn {
namespace {

constexpr float W = 1920.f, H = 1080.f;
constexpr int FPS = 30, TOTAL = 720;                 // 24 s
const std::string ASSETS = "out/bundaran-hi-banner-assets";

const Color INK = mkColor(0x14202b), BONE = mkColor(0xf1ede2), TEAL = mkColor(0x2fc4b2), AMBER = mkColor(0xf0a43c),
            DIM = mkColor(0xb4bcc4), SKY = mkColor(0xe9b98a);

// ------------------------------------------------------------------ timing (brief sections 3 and 9), seconds
constexpr double STANDOFF_AT = 4.5, CLIMB_AT = 10.0, UNROLL_AT = 16.5, CARD_AT = 21.5;
constexpr double CLIMB_FROM = 6.0, CLIMB_TO = 14.0;        // up the ropes on the side walls; it starts while the crowd faces the police
constexpr double WALK0 = 14.6;                              // over the lip, then along the cap to the front
constexpr double CARRY_OUT = 15.0, CARRY_DOWN = 15.2, LOWERED = 16.4;   // at the top: they stand, then the ties come off
constexpr double ROW_DT = 1.0 / 16;                         // the banner pays out 16 rows (4 m) a second
constexpr double CHEER_AT = 18.8;                           // the crowd, as the banner passes mid-height
constexpr double HAUL0 = 14.15, HAUL1 = 15.2;               // the climbing ropes hauled up behind them

// ------------------------------------------------------------------ the junction (metres; x east, z south)
// From OpenStreetMap (assets/data/bundaran-hi): the pool's edge is 33-46 m from the middle, the
// roundabout's centre line 52-63 m; it is drawn here as circles. Buildings and roads are the OSM data.
constexpr float POOL = 40.0f, PLANTER_OUT = 49.5f, RING_IN = 50.0f, RING_OUT = 65.5f;
const char* const CITY = "data/bundaran-hi/bundaran-hi.city.json";   // under the assets root, so it resolves from any directory

// ------------------------------------------------------------------ the monument and the banner
// About 30 m in all: the figures 5 m (7 m to the raised hand) on a tall pedestal that is a hollow
// frame, open front and back, between two side walls under a cap.
constexpr float BASE_TOP = 1.2f, SILL_TOP = 2.6f, SHAFT_TOP = 21.4f, CAP_TOP = 23.0f;
constexpr float SHAFT_HALF = 2.3f, SHAFT_HALF_Z = 1.6f, WALL = 0.9f, CAP_HALF = 2.5f, CAP_HALF_Z = 1.8f;
constexpr int COLS = 11, ROWS = 77;                         // the banner: 2.5 m wide, 19 m long
constexpr float CELL = 0.25f, ATTACH_Y = CAP_TOP - 0.6f, ATTACH_Z = CAP_HALF_Z + 0.04f;
constexpr float ROLL_R0 = 0.06f + 0.0028f * (ROWS - 1), ROLL_PITCH = 0.0028f;
constexpr float HOIST_Z = SHAFT_HALF_Z + 0.65f;             // the roll rides up just in front of the frame
constexpr float ROPE_X = CAP_HALF + 0.12f;                  // the climbing ropes hang just outside the cap
constexpr float STREET_Z = 70.0f, CROWD_X = 6.0f;           // the barricade across the mouth of Jl. Sudirman

float smooth(double u) { u = std::clamp(u, 0.0, 1.0); return (float)(u * u * (3 - 2 * u)); }
float window(double s, double from, double to) {
    const double f = 8.0 / FPS;
    return std::min(smooth((s - from) / f), smooth((to - s) / f));
}
Color ca(Color c, int a) { c.a = (unsigned char)std::clamp(a, 0, 255); return c; }   // an alpha of 0..255
void text(const Ctx& cx, const std::string& s, Vector2 at, float size, Color col, float ax = 0, float ay = 0.5f,
          bool bold = false, float spacing = 0) {
    dr::text(fonts::ui(bold), cx, s.c_str(), at, size, spacing, col, ax, ay);
}
Ctx faded(const Ctx& cx, float a) { Ctx m = cx; m.a *= std::clamp(a, 0.f, 1.f); return m; }

// ------------------------------------------------------------------ people
rig::Look student(int i) {
    rig::Look l = rig::Look::toon();
    l.build = l.build.scaled(1.7f / l.build.height());
    const bool yellow = i % 2 == 0;
    const Color skins[] = { { 232, 190, 150, 255 }, { 205, 150, 110, 255 }, { 170, 118, 84, 255 }, { 140, 92, 62, 255 } };
    const Color hairs[] = { { 25, 20, 20, 255 }, { 45, 32, 26, 255 }, { 20, 18, 24, 255 } };
    l.skin = skins[(i / 2) % 4];
    l.hairColor = hairs[(i / 3) % 3];
    l.outfit = rig::Outfit::basic(yellow ? mkColor(0xf2c230) : mkColor(0x2b4f9e), mkColor(0x2a2e3a), l.skin, 0.55f, 1.0f);
    return l;
}
rig::Look officer(int i) {
    rig::Look l = rig::Look::toon();
    l.build = l.build.scaled((1.74f + 0.03f * (float)(i % 3)) / l.build.height());
    const Color skins[] = { { 205, 150, 110, 255 }, { 170, 118, 84, 255 } };
    l.skin = skins[i % 2];
    l.hairColor = { 20, 18, 22, 255 };
    l.outfit = rig::Outfit::basic(mkColor(0x2d3748), mkColor(0x222833), l.skin, 0.55f, 1.0f);
    l.holdR = "prop.baton";
    return l;
}
rig::Look bronze() {
    rig::Look l = rig::Look::toon();
    l.build = l.build.scaled(5.0f / l.build.height());        // illustrative scale, brief section 5
    const Color metal = mkColor(0x8a6a3c);
    l.skin = metal;
    l.hairColor = mkColor(0x5e4528);
    l.outfit = rig::Outfit::basic(metal, mkColor(0x7a5c34), metal, 0.6f, 1.0f);
    l.finish = rig::Finish::Matte;
    return l;
}

// The statue holds a waving pose, still: the outer arm raised, the inner arm bent up with the bouquet.
rig::Pose statuePose(int k) {
    const rig::Side up = k == 0 ? rig::Side::Right : rig::Side::Left;
    rig::Pose p = rig::clips::wave(up)(1.0);        // the wave clip once it has settled...
    const bool l = up == rig::Side::Left;
    p[l ? rig::Joint::ElbowL : rig::Joint::ElbowR] = { -10, 0, 40 };    // ...with the swing taken out of the elbow
    const rig::Side other = l ? rig::Side::Right : rig::Side::Left;
    p[other == rig::Side::Left ? rig::Joint::ShoulderL : rig::Joint::ShoulderR] = { -45, 0, 0 };
    p[other == rig::Side::Left ? rig::Joint::ElbowL : rig::Joint::ElbowR] = { -105, 0, 0 };
    return p;
}

// The climb: arms and legs alternate, one reach for each 0.55 m of wall, a pure function of the height.
rig::Pose climbPose(float phase) {
    rig::Pose a, b;
    a[rig::Joint::ShoulderR] = { -165, 0, 0 };
    a[rig::Joint::ElbowR] = { -20, 0, 0 };
    a[rig::Joint::ShoulderL] = { -95, 0, 0 };
    a[rig::Joint::ElbowL] = { -60, 0, 0 };
    a[rig::Joint::HipL] = { -75, 0, 0 };
    a[rig::Joint::KneeL] = { 85, 0, 0 };
    a[rig::Joint::HipR] = { -10, 0, 0 };
    b = a.mirrored();
    const float u = phase - std::floor(phase);
    return rig::Pose::mix(a, b, 0.5f - 0.5f * std::cos(2 * PI * u));
}
rig::Pose carryPose() {
    rig::Pose p;
    for (rig::Joint j : { rig::Joint::ShoulderL, rig::Joint::ShoulderR }) p[j] = { -70, 0, 0 };
    for (rig::Joint j : { rig::Joint::ElbowL, rig::Joint::ElbowR }) p[j] = { -85, 0, 0 };
    return p;
}

// ------------------------------------------------------------------ what is built, once
struct Bodies {
    std::vector<crowd::Baked> idle, cheer, police;
};
const Bodies& bodies() {
    static const Bodies b = [] {
        Bodies out;
        constexpr float CADENCE = 1;
        for (int v = 0; v < 6; v++) {
            const rig::Look look = student(v);
            const float stride = rig::walkSpeed(look.build, CADENCE);
            out.idle.push_back(crowd::bakeWalker(look, rig::clips::walk(CADENCE), 1.0 / CADENCE, rig::clips::idle(0.8f), 2, stride));
            out.cheer.push_back(crowd::bakeWalker(look, rig::clips::walk(CADENCE), 1.0 / CADENCE, rig::clips::cheer(), 1, stride));
        }
        for (int v = 0; v < 4; v++) {
            const rig::Look look = officer(v);
            out.police.push_back(crowd::bakeWalker(look, rig::clips::walk(CADENCE), 1.0 / CADENCE, rig::clips::shieldLine(look.build), 2,
                                                   rig::walkSpeed(look.build, CADENCE)));
        }
        return out;
    }();
    return b;
}

struct World {
    crowd::CrowdPtr students, police;
};
std::shared_ptr<World> world() {
    static std::shared_ptr<World> w = [] {
        auto out = std::make_shared<World>();
        crowd::World sw;
        crowd::Group g;
        g.name = "students";
        g.count = 230;
        g.spawn = { CROWD_X, 0, STREET_Z + 21 };
        g.spawnSize = { 40, 0, 22 };
        g.heading0 = { 0, 0, -1 };
        g.headingSpread = 14;
        g.cruise = 0;
        g.maxSpeed = 0.05f;
        g.steer.wander = 0;
        g.steer.goal = 0;
        g.steer.cohesion = 0;
        g.steer.alignment = 0;
        g.steer.separation = 2;
        g.steer.personal = 0.7f;
        sw.groups.push_back(g);
        sw.boundsMin = { -200, 0, -200 };
        sw.boundsMax = { 200, 20, 200 };
        sw.from = 0;
        sw.to = 24;
        sw.seed = 11;
        out->students = crowd::bake(sw);

        crowd::World pw = sw;
        pw.groups.clear();
        g.name = "police";
        g.count = 64;
        g.spawn = { CROWD_X, 0, STREET_Z - 1.3f };
        g.spawnSize = { 62, 0, 0.5f };
        g.heading0 = { 0, 0, 1 };
        g.headingSpread = 4;
        g.steer.personal = 0.55f;
        g.steer.separation = 1.2f;
        pw.groups.push_back(g);
        pw.seed = 21;
        out->police = crowd::bake(pw);
        return out;
    }();
    return w;
}

// ------------------------------------------------------------------ the camera (brief section 6)
float climbY(double s, double lag);
g3d::Camera camera(double s) {
    g3d::Camera cam;
    cam.nearPlane = 1;                              // nothing comes nearer; keeps depth precise for roads and walls 300 m out
    cam.farPlane = 1600;
    auto lerp3 = [](Vector3 a, Vector3 b, float u) { return Vector3Lerp(a, b, u); };
    // The crowd stands on Jl. Sudirman, south of the roundabout (z 80-104); the long shots look up the
    // road from behind it with a long lens, so the monument, the police line and the crowd stack up.
    const Vector3 low{ 21, 6, 132 }, banner{ 20, 3.6f, 126 };
    if (s < STANDOFF_AT + 0.6) {                    // high over Sudirman, coming down to the crowd
        const float u = smooth(s / (STANDOFF_AT + 0.6));
        cam.position = lerp3({ 150, 85, 300 }, { 44, 18, 178 }, u);
        cam.target = lerp3({ 0, 10, 0 }, { 4, 9, 50 }, u);
        cam.fov = Lerp(36, 27, u);
    } else if (s < CLIMB_AT) {                      // low behind the students; the climbers are already on the ropes
        const float u = smooth((s - STANDOFF_AT - 0.6) / (CLIMB_AT - STANDOFF_AT - 0.6));
        cam.position = lerp3({ 44, 18, 178 }, low, u);
        cam.target = lerp3({ 4, 9, 50 }, { 1, 11, 0 }, u);
        cam.fov = Lerp(27, 21, u);
    } else if (s < UNROLL_AT) {                     // in over the crowd to the monument and up with the climbers, then back out
        const float p = smooth((climbY(s, 0.25) - BASE_TOP) / (CAP_TOP - BASE_TOP));
        const float near = smooth((s - CLIMB_AT) / 2.8) * (1 - smooth((s - WALK0) / (UNROLL_AT - WALK0)));
        const float u = smooth((s - CLIMB_AT) / (UNROLL_AT - CLIMB_AT));
        const Vector3 far = lerp3(low, banner, u);
        // in polar form round the monument, so the move stays clear of it
        const float rf = std::hypot(far.x, far.z), af = std::atan2(far.x, far.z);
        const float rad = Lerp(rf, 34.f, near), az = Lerp(af, 16.f * DEG2RAD, near);
        const float eye = Lerp(far.y, Lerp(5.f, CAP_TOP - 1.5f, p), near);
        cam.position = { rad * std::sin(az), eye, rad * std::cos(az) };
        const Vector3 wide{ 1, 11, 0 }, climber{ 0, Lerp(4.f, CAP_TOP - 0.5f, p), 1.2f }, hung{ 0, 13.5f, ATTACH_Z };
        cam.target = lerp3(lerp3(wide, hung, u), climber, near);
        cam.fov = Lerp(Lerp(21, 22, u), 38, near);
    } else if (s < CARD_AT) {                       // the banner from the crowd, a slow push
        const float u = smooth((s - UNROLL_AT) / (CARD_AT - UNROLL_AT));
        cam.position = lerp3(banner, { 16, 3.2f, 116 }, u);
        cam.target = { 0, 13.5f, ATTACH_Z };
        cam.fov = Lerp(22, 21, u);
    } else {
        const float u = smooth((s - CARD_AT) / (24 - CARD_AT));
        cam.position = lerp3({ 16, 3.2f, 116 }, { 26, 11, 150 }, u);
        cam.target = { 0, 13.5f, ATTACH_Z };
        cam.fov = Lerp(21, 24, u);
    }
    return cam;
}

// ------------------------------------------------------------------ the climbers
// Height of a climber at time s (0 before the climb, standing on the cap after it).
float climbY(double s, double lag) {
    const float u = smooth((s - CLIMB_FROM - lag) / (CLIMB_TO - CLIMB_FROM));
    return BASE_TOP + (CAP_TOP - BASE_TOP) * u;
}
float walked(double s) { return smooth((s - WALK0) / (CARRY_OUT - WALK0)); }
float ropeZ(int) { return 1.0f; }
float side(int who) { return who == 0 ? -1.f : 1.f; }       // 0 climbs the west face, 1 the east face
Vector3 climberAt(double s, int who) {
    const double lag = who == 0 ? 0 : 0.2;
    const float y = climbY(s, lag);
    const float over = smooth((y - (SHAFT_TOP - 0.2f)) / 0.9f);          // from the wall, over the lip, onto the cap
    const Vector3 arrive{ side(who) * Lerp(SHAFT_HALF + 0.4f, 1.6f, over), y, ropeZ(who) };
    const Vector3 end{ side(who) * 1.25f, CAP_TOP, 1.15f };
    return Vector3Lerp(arrive, end, walked(s));
}
float climberYaw(double s, int who) { return -side(who) * 90.f * (1 - walked(s)); }   // facing the wall, then out over the crowd

// ------------------------------------------------------------------ the banner
// Where a row sits while it is still on the roll: a spiral round the roll's axis (along x),
// outermost row on top. The roll hangs just below the rows already paid out and sinks with them.
float unrolled(double s) { return (float)std::clamp((s - UNROLL_AT) / ROW_DT, 0.0, (double)(ROWS - 1)); }

Vector3 rollCentre(double s, float x) {
    if (s >= UNROLL_AT) return { x, ATTACH_Y - CELL - ROLL_R0 - CELL * unrolled(s), ATTACH_Z };
    // Hoisted up the front between the two climbers, tied to their lines; once they are at the top
    // the ties come off and it is lowered over the lip into place.
    const Vector3 hung{ x, ATTACH_Y - CELL - ROLL_R0, ATTACH_Z };
    const float lifted = std::min(climbY(s, 0), climbY(s, 0.2)) + 0.9f;
    const Vector3 hoisted{ x, lifted, HOIST_Z };
    if (s < CARRY_DOWN) return hoisted;
    return Vector3Lerp(hoisted, hung, smooth((s - CARRY_DOWN) / (LOWERED - CARRY_DOWN)));
}

Vector3 rolledRow(int row, int col, double s) {
    const float x = -0.5f * CELL * (COLS - 1) + CELL * (float)col;
    const float n = std::max(0.0f, (float)row - unrolled(s) - 1);
    const float r = std::max(0.05f, ROLL_R0 - ROLL_PITCH * n);
    const float turn = -(CELL / ROLL_PITCH) * std::log(r / ROLL_R0);
    const Vector3 c = rollCentre(s, x);
    return { x, c.y + r * std::cos(turn), c.z + r * std::sin(turn) };
}

// The pedestal as boxes for the cloth and ropes: the two side walls, the sill, the cap and the base.
std::vector<physics::Collider> monumentColliders() {
    const float wallX = SHAFT_HALF - 0.5f * WALL, midY = 0.5f * (BASE_TOP + SHAFT_TOP), halfY = 0.5f * (SHAFT_TOP - BASE_TOP);
    return { physics::Collider::box({ -wallX, midY, 0 }, { 0.5f * WALL, halfY, SHAFT_HALF_Z }),
             physics::Collider::box({ wallX, midY, 0 }, { 0.5f * WALL, halfY, SHAFT_HALF_Z }),
             physics::Collider::box({ 0, 0.5f * (BASE_TOP + SILL_TOP), 0 }, { SHAFT_HALF, 0.5f * (SILL_TOP - BASE_TOP), SHAFT_HALF_Z }),
             physics::Collider::box({ 0, 0.5f * (SHAFT_TOP + CAP_TOP), 0 }, { CAP_HALF, 0.5f * (CAP_TOP - SHAFT_TOP), CAP_HALF_Z }),
             physics::Collider::box({ 0, 0.5f * BASE_TOP, 0 }, { 4.5f, 0.5f * BASE_TOP, 3.5f }) };
}

physics::Cloth makeBanner() {
    physics::Params p;
    p.fps = FPS;
    p.substeps = 6;
    p.iterations = 12;
    p.damping = 0.02f;
    const float x0 = -0.5f * CELL * (COLS - 1);
    physics::Cloth banner(COLS, ROWS, CELL, { x0, ATTACH_Y, ATTACH_Z }, { 1, 0, 0 }, { 0, -1, 0 }, p);
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            const int index = row * COLS + col;
            if (row == 0) {
                const float x = x0 + CELL * (float)col;
                banner.pin(index, [col, x](double s) {              // carried with the roll until it is hung on the cap
                    if (s >= LOWERED) return Vector3{ x, ATTACH_Y, ATTACH_Z };
                    return Vector3Add(rolledRow(0, col, s), { 0, CELL, 0 });
                });
            } else {
                banner.pin(index, [row, col](double s) { return rolledRow(row, col, s); }, UNROLL_AT + ROW_DT * row);
            }
        }
    }
    for (const physics::Collider& c : monumentColliders()) banner.collide(c);
    banner.wind({ { 0.25f, 0, 1 }, 3.0f, 0.6f, 0.45f, 7, 0.9f, 4.5f });   // billows it out of the open frame, rippling down its length
    return banner;
}

g3d::DynamicTexturePtr bannerTexture() {
    static constexpr float TW = 288, TH = 288 * 7;          // 11 x 77 cells: 1 to 7
    auto tex = std::make_shared<g3d::DynamicTexture>();
    tex->width = (int)TW;
    tex->height = (int)TH;
    tex->supersample = 2;
    tex->draw = [](const Ctx& c, int) {
        dr::rect(c, 0, 0, TW, TH, mkColor(0x101a26));          // dark cloth: it reads against light stone and busy windows
        dr::rect(c, 0, 0, TW, 70, mkColor(0xc4262e));
        dr::rect(c, 0, TH - 70, TW, 70, mkColor(0xc4262e));
        dr::rect(c, 14, 90, 6, TH - 180, mkColor(0xf6f1e4));    // a white rule down each edge
        dr::rect(c, TW - 20, 90, 6, TH - 180, mkColor(0xf6f1e4));
        // One line down the cloth, the letters turned a quarter clockwise.
        Ctx m = c;
        m.rot = c.rot + 90;
        m.o = mapP(c, { TW / 2, TH / 2 });
        dr::text(fonts::ui(true), m, "#INDONESIASADAR", { 0, 0 }, 196, 0, mkColor(0xf6f1e4), 0.5f, 0.5f);
    };
    return tex;
}

// ------------------------------------------------------------------ the scene
Mesh annulus(float r0, float r1, int segments) {
    Mesh m{};
    m.vertexCount = (segments + 1) * 2;
    m.triangleCount = segments * 2;
    m.vertices = (float*)std::calloc((size_t)m.vertexCount * 3, sizeof(float));
    m.normals = (float*)std::calloc((size_t)m.vertexCount * 3, sizeof(float));
    m.texcoords = (float*)std::calloc((size_t)m.vertexCount * 2, sizeof(float));
    m.indices = (unsigned short*)std::calloc((size_t)m.triangleCount * 3, sizeof(unsigned short));
    for (int i = 0; i <= segments; i++) {
        const float a = 2 * PI * (float)i / (float)segments;
        for (int k = 0; k < 2; k++) {
            const float r = k == 0 ? r0 : r1;
            const int v = i * 2 + k;
            m.vertices[v * 3] = r * std::cos(a);
            m.vertices[v * 3 + 1] = 0;
            m.vertices[v * 3 + 2] = r * std::sin(a);
            m.normals[v * 3 + 1] = 1;
            m.texcoords[v * 2] = m.vertices[v * 3] * 0.05f;
            m.texcoords[v * 2 + 1] = m.vertices[v * 3 + 2] * 0.05f;
        }
    }
    for (int i = 0; i < segments; i++) {
        const unsigned short a = (unsigned short)(i * 2), b = (unsigned short)(i * 2 + 1), c = (unsigned short)(i * 2 + 2), d = (unsigned short)(i * 2 + 3);
        const int t = i * 6;
        m.indices[t] = a; m.indices[t + 1] = c; m.indices[t + 2] = b;
        m.indices[t + 3] = b; m.indices[t + 4] = c; m.indices[t + 5] = d;
    }
    UploadMesh(&m, false);
    return m;
}

g3d::Object3D* box(g3d::Scene& scene, Vector3 size, Vector3 at, Color col, float rough = 0.9f) {
    g3d::Material m;
    m.color = col;
    m.roughness = rough;
    g3d::Object3D* o = scene.add(g3d::Geometry::box(size.x, size.y, size.z), m);
    o->position = at;
    return o;
}

// A facade tile: `bays` x `floors` of windows, drawn once and repeated over the OSM buildings' walls
// (city::Look::textureSize). The material's vertex colour (a tint per building) colours the wall.
//   style 0: low shophouses and offices, small punched windows
//   style 1: mid-rise offices, ribbon windows
//   style 2: towers, curtain wall glass with mullions
constexpr float FLOOR_H = 3.8f, BAY_W = 3.4f;
g3d::DynamicTexturePtr facade(int style, int floors, int bays, unsigned seed) {
    auto tex = std::make_shared<g3d::DynamicTexture>();
    tex->width = 64 * bays;
    tex->repeat = true;
    tex->height = 64 * floors;
    tex->supersample = 1;
    tex->draw = [style, floors, bays, seed](const Ctx& c, int) {
        const float cw = 64, ch = 64, w = cw * bays, h = ch * floors;
        dr::rect(c, 0, 0, w, h, style == 2 ? mkColor(0xb9c6cf) : mkColor(0xe2ddd2));
        for (int f = 0; f < floors; f++) {
            const float y = ch * (float)f;
            for (int b = 0; b < bays; b++) {
                const unsigned long long id = (unsigned long long)seed * 100000 + (unsigned long long)(f * 64 + b);
                const float x = cw * (float)b;
                const double u = rnd01(21, id);
                const unsigned char d = (unsigned char)(rnd01(22, id) * 22);
                Color glass = u < 0.14 ? mkColor(0xf2c978) : Color{ (unsigned char)(42 + d), (unsigned char)(66 + d), (unsigned char)(82 + d), 255 };
                if (style == 0) {
                    dr::rect(c, x + 16, y + 16, cw - 32, ch - 30, glass);
                    dr::rect(c, x + 12, y + ch - 14, cw - 24, 4, mkColor(0xb4afa4));
                } else if (style == 1) {
                    dr::rect(c, x + 4, y + 14, cw - 8, ch - 30, glass);
                    dr::rect(c, x + 4, y + 14, cw - 8, 4, ca(WHITE, 60));
                } else {
                    if (u < 0.14) glass = mkColor(0xd9c08a);
                    dr::rect(c, x + 2, y + 6, cw - 4, ch - 10, glass);
                    dr::rect(c, x + 2, y + 6, cw - 4, 6, ca(WHITE, 50));
                }
            }
            dr::rect(c, 0, y + ch - 3, w, 3, style == 2 ? mkColor(0x7f8c96) : mkColor(0xa9a498));   // slab line
        }
        // keep the top-left texel plain wall: roofs sample it (city::Look::flatUV)
        dr::rect(c, 0, 0, 2, 2, style == 2 ? mkColor(0xb9c6cf) : mkColor(0xe2ddd2));
    };
    return tex;
}

// Triangles with flat normals and vertex colours, gathered into one mesh: the trees, poles and lamp
// heads along the OSM roads, and the lane markings, draw as a handful of meshes, not thousands.
struct Batch {
    std::vector<float> v, n;
    std::vector<unsigned char> c;
    void tri(Vector3 a, Vector3 b, Vector3 d, Color col) {
        const Vector3 nn = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(d, a)));
        for (Vector3 p : { a, b, d }) {
            v.insert(v.end(), { p.x, p.y, p.z });
            n.insert(n.end(), { nn.x, nn.y, nn.z });
            c.insert(c.end(), { col.r, col.g, col.b, col.a });
        }
    }
    void quad(Vector3 a, Vector3 b, Vector3 d, Vector3 e, Color col) { tri(a, b, d, col); tri(a, d, e, col); }
    // A box turned `yaw` radians about y.
    void box(Vector3 at, Vector3 half, float yaw, Color col) {
        const float cs = std::cos(yaw), sn = std::sin(yaw);
        auto P = [&](float x, float y, float z) { return Vector3{ at.x + x * cs + z * sn, at.y + y, at.z - x * sn + z * cs }; };
        const float X = half.x, Y = half.y, Z = half.z;
        quad(P(-X, Y, -Z), P(-X, Y, Z), P(X, Y, Z), P(X, Y, -Z), col);          // top
        quad(P(-X, -Y, -Z), P(X, -Y, -Z), P(X, -Y, Z), P(-X, -Y, Z), col);      // bottom
        quad(P(-X, -Y, Z), P(X, -Y, Z), P(X, Y, Z), P(-X, Y, Z), col);          // +z
        quad(P(X, -Y, -Z), P(-X, -Y, -Z), P(-X, Y, -Z), P(X, Y, -Z), col);      // -z
        quad(P(X, -Y, Z), P(X, -Y, -Z), P(X, Y, -Z), P(X, Y, Z), col);          // +x
        quad(P(-X, -Y, -Z), P(-X, -Y, Z), P(-X, Y, Z), P(-X, Y, -Z), col);      // -x
    }
    // A low-poly ball (an octahedron split once), slightly flattened: a tree's crown.
    void ball(Vector3 at, Vector3 radius, Color col) {
        const Vector3 o[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
        const int f[8][3] = { { 0, 2, 4 }, { 4, 2, 1 }, { 1, 2, 5 }, { 5, 2, 0 }, { 4, 3, 0 }, { 1, 3, 4 }, { 5, 3, 1 }, { 0, 3, 5 } };
        auto at_ = [&](Vector3 d) { d = Vector3Normalize(d); return Vector3{ at.x + d.x * radius.x, at.y + d.y * radius.y, at.z + d.z * radius.z }; };
        for (const auto& t : f) {
            const Vector3 a = o[t[0]], b = o[t[1]], d = o[t[2]];
            const Vector3 ab = Vector3Add(a, b), bd = Vector3Add(b, d), da = Vector3Add(d, a);
            tri(at_(a), at_(ab), at_(da), col);
            tri(at_(ab), at_(b), at_(bd), col);
            tri(at_(da), at_(bd), at_(d), col);
            tri(at_(ab), at_(bd), at_(da), col);
        }
    }
    g3d::GeometryPtr geometry() const {
        auto self = std::make_shared<Batch>(*this);
        return std::make_shared<g3d::Geometry>([self] {
            Mesh m{};
            m.vertexCount = std::max(3, (int)(self->v.size() / 3));
            m.triangleCount = m.vertexCount / 3;
            m.vertices = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
            m.normals = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
            m.colors = (unsigned char*)MemAlloc((unsigned)(m.vertexCount * 4));
            std::copy(self->v.begin(), self->v.end(), m.vertices);
            std::copy(self->n.begin(), self->n.end(), m.normals);
            std::copy(self->c.begin(), self->c.end(), m.colors);
            UploadMesh(&m, false);
            return m;
        });
    }
};

// The top of the k-th flag's pole: held by a student in the crowd at hand height, lifted and pumped
// when the crowd cheers. A pure function of time, so the flag's cloth can follow it.
constexpr int FLAG_COUNT = 7;
Vector3 flagTop(const World& w, int k, double s) {
    const int agent = w.students->first("students") + 9 + k * 19;
    const Vector3 at = w.students->at(agent, s).pos;
    const float cheer = smooth((s - CHEER_AT) / 0.6);
    const float pump = cheer * 0.22f * std::sin(2 * PI * 1.1f * (float)(s - CHEER_AT) + (float)k);
    return { at.x + 0.28f, 1.05f + 2.55f + 0.5f * cheer + pump, at.z + 0.1f };
}

std::shared_ptr<g3d::Scene> makeScene(const std::shared_ptr<World>& w) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->sky = g3d::Sky::at(16.6f);
    scene->sky.sunLight = 0;
    scene->sky.clouds = 0.5f;
    scene->sky.cloudScale = 2.2f;
    scene->sky.resolution = 2048;
    scene->sky.seed = 7;
    scene->fogMode = g3d::FogMode::Exponential;
    scene->fogDensity = 0.0019f;
    scene->fogFromSky = true;
    scene->sky.sunGlow = 0.35f;
    scene->ambient = 0.6f;
    scene->shadowMapSize = 4096;
    g3d::Light key = g3d::Light::directional({ -34, 17, 26 }, { 255, 214, 160, 255 }, 2.5f, true);
    key.shadowExtent = 60;
    // The shadow map covers the part of the junction the shot is looking at (the crowd, or the monument).
    key.up = [](int t, g3d::Light& l) {
        const g3d::Camera cam = camera(t / (double)FPS);
        const float reach = std::clamp(Vector3Distance(cam.position, cam.target), 30.f, 160.f);
        const Vector3 mid = Vector3Lerp(cam.position, cam.target, 0.55f);
        l.target = { mid.x, 0, mid.z };
        l.position = Vector3Add(l.target, Vector3Scale(Vector3Normalize({ -34, 17, 26 }), 200));
        l.shadowExtent = 0.55f * reach + 20;
    };
    scene->lights.push_back(key);
    scene->camera.up = [](int t, g3d::Camera& c) { const auto keep = c.up; c = camera(t / (double)FPS); c.up = keep; };   // assigning a whole Camera would drop `up`

    scene->water = g3d::Water::calm();
    scene->water.level = -0.45f;
    scene->water.fps = FPS;
    scene->water.openDepth = 0;
    scene->water.caustics = 1;

    // ---- the roundabout, as circles at the OSM radii: pool, planted ring, ring road, pavement
    g3d::Material road;
    road.color = mkColor(0x4c4f56);
    road.roughness = 0.95f;
    road.doubleSided = true;
    scene->add(std::make_shared<g3d::Geometry>([] { return annulus(RING_IN, RING_OUT, 240); }), road)->position = { 0, 0.02f, 0 };
    g3d::Material pave;
    pave.color = mkColor(0x9a968c);
    pave.roughness = 0.95f;
    pave.doubleSided = true;
    scene->add(std::make_shared<g3d::Geometry>([] { return annulus(RING_OUT, 1500, 240); }), pave);
    scene->add(std::make_shared<g3d::Geometry>([] { return annulus(PLANTER_OUT, RING_IN, 240); }), pave)->position = { 0, 0.2f, 0 };
    g3d::Material green;
    green.color = mkColor(0x56763e);
    green.roughness = 0.95f;
    green.doubleSided = true;
    scene->add(std::make_shared<g3d::Geometry>([] { return annulus(POOL + 0.5f, PLANTER_OUT, 240); }), green)->position = { 0, 0.25f, 0 };
    g3d::Material floorM;
    floorM.color = mkColor(0x7d8e96);
    floorM.roughness = 0.9f;
    scene->add(g3d::Geometry::cylinder(POOL + 0.4f, 0.1f, 128), floorM)->position = { 0, -1.3f, 0 };
    g3d::Material stone;
    stone.color = mkColor(0xb9b2a4);
    stone.roughness = 0.85f;

    Batch marks, poles, heads, trees;                       // merged meshes, filled below
    const Color white = mkColor(0xe9e5d8);
    // the ring's lane lines: three dashed, an edge line each side
    for (int k = 0; k <= 4; k++) {
        const float rr = RING_IN + 0.4f + (RING_OUT - RING_IN - 0.8f) * (float)k / 4;
        const bool edge = k == 0 || k == 4;
        const int n = (int)(2 * PI * rr / (edge ? 1.5f : 9.0f));
        for (int i = 0; i < n; i++) {
            const float a0 = 2 * PI * (float)i / n, a1 = a0 + 2 * PI / n * (edge ? 1.0f : 0.34f);
            const float in = rr - 0.08f, out = rr + 0.08f;
            marks.quad({ in * std::cos(a0), 0.04f, in * std::sin(a0) }, { in * std::cos(a1), 0.04f, in * std::sin(a1) },
                       { out * std::cos(a1), 0.04f, out * std::sin(a1) }, { out * std::cos(a0), 0.04f, out * std::sin(a0) }, white);
        }
    }
    // a low stone coping round the pool and a kerb round the planted ring
    for (const auto [rr, hgt, wid, n] : { std::tuple{ POOL + 0.25f, 0.6f, 0.8f, 240 }, std::tuple{ PLANTER_OUT, 0.3f, 0.35f, 300 } }) {
        for (int i = 0; i < n; i++) {
            const float a = 2 * PI * (float)i / (float)n, len = 2 * PI * rr / (float)n * 1.06f;
            marks.box({ rr * std::cos(a), 0.5f * hgt, rr * std::sin(a) }, { 0.5f * wid, 0.5f * hgt, 0.5f * len }, -a, mkColor(0xbdb6a8));
        }
    }
    // hedges and clipped shrubs in the planted ring
    for (int i = 0; i < 160; i++) {
        const unsigned long long id = (unsigned long long)i;
        const float a = 2 * PI * ((float)i + (float)rnd01(41, id)) / 160, rr = Lerp(POOL + 1.6f, PLANTER_OUT - 1.2f, (float)rnd01(42, id));
        const float size = 0.7f + 0.6f * (float)rnd01(43, id);
        trees.ball({ rr * std::cos(a), 0.25f + 0.5f * size, rr * std::sin(a) }, { size, 0.7f * size, size },
                   rnd01(44, id) < 0.5 ? mkColor(0x4f7a3a) : mkColor(0x3f6a33));
    }

    // ---- the city: OSM buildings and roads, as CityJSON (tools/osm_bundaran_hi.py)
    const city::Model town = city::load(CITY);
    const Vector3 shift = Vector3Negate(city::toScene(town.local({ 0, 0, 0 })));   // the file's origin is the middle of the pool
    struct Road { std::vector<Vector2> line; float width; int lanes; bool oneway; std::string kind; };
    std::vector<Road> roads;
    std::vector<BoundingBox> footprints;
    for (const city::Object& o : town.objects) {
        if (o.type == "Road") {
            Road rd;
            for (const Json& q : o.attributes["line"].asArray()) rd.line.push_back({ (float)q[0].asNumber(), -(float)q[1].asNumber() });
            rd.width = (float)o.attributes.num("width", 7);
            rd.lanes = o.attributes.integer("lanes", 1);
            rd.oneway = o.attributes.flag("oneway", false);
            rd.kind = o.attributes.str("class", "");
            roads.push_back(std::move(rd));
        } else if (o.type == "Building") {
            BoundingBox b = city::toScene(o.bounds());
            b.min = Vector3Add(b.min, shift);
            b.max = Vector3Add(b.max, shift);
            footprints.push_back(b);
        }
    }
    {
        city::Look look;
        look.road = mkColor(0x50535a);
        look.color = [](const city::Object& o, const city::Face&) {
            return o.attributes.str("class", "") == "busway" ? mkColor(0x8b8780) : BLANK;   // the TransJakarta lane: concrete
        };
        g3d::Material m;
        m.roughness = 0.95f;
        m.castShadow = false;
        const city::Model streets = town.filter([](const city::Object& o) { return o.type == "Road"; });
        g3d::Object3D* o = scene->add(city::mesh(streets, look), m);
        o->position = shift;
        o->scale = { 1, 2, 1 };                             // the ribbons' 3-5 cm lifts become 6-10 cm: clear of the pavement and of each other
    }
    // Buildings in three facade styles by height; each its own tint, roofs plain.
    constexpr int TILE_FLOORS[3] = { 3, 4, 6 }, TILE_BAYS[3] = { 4, 4, 4 };
    for (int style = 0; style < 3; style++) {
        const city::Model part = town.filter([style](const city::Object& o) {
            if (o.type != "Building") return false;
            const float h = (float)o.attributes.num("height", 10);
            return style == (h < 20 ? 0 : h < 70 ? 1 : 2);
        });
        city::Look look;
        look.textureSize = { BAY_W * TILE_BAYS[style], FLOOR_H * TILE_FLOORS[style] };
        look.flatUV = { 0.004f, 0.004f };
        look.color = [style](const city::Object& o, const city::Face& f) -> Color {
            if (f.kind == "RoofSurface") return mkColor(0x8e8c86);
            const unsigned long long id = std::hash<std::string>{}(o.id);
            const Color warm[] = { mkColor(0xf1ebe0), mkColor(0xe6dccb), mkColor(0xd9d2c6), mkColor(0xeee6d6), mkColor(0xd6cbb8) };
            const Color cool[] = { mkColor(0xd7e1ea), mkColor(0xc6d3dd), mkColor(0xe1e6ea), mkColor(0xbfcbd2) };
            return style == 2 ? cool[id % 4] : warm[id % 5];
        };
        g3d::Material m;
        m.roughness = style == 2 ? 0.35f : 0.75f;
        m.metalness = style == 2 ? 0.25f : 0.05f;
        m.dynamicMap = facade(style, TILE_FLOORS[style], TILE_BAYS[style], (unsigned)(style + 3));
        m.castShadow = false;                               // tower shadows would swamp the plaza at this hour
        scene->add(city::mesh(part, look), m)->position = shift;
    }

    // ---- along the OSM roads: lane lines, trees and street lamps
    auto clearOf = [&](Vector2 q, float margin) {
        if (std::hypot(q.x, q.y) < RING_OUT + 3) return false;
        for (const BoundingBox& b : footprints)
            if (q.x > b.min.x - margin && q.x < b.max.x + margin && q.y > b.min.z - margin && q.y < b.max.z + margin) return false;
        for (const Road& rd : roads)
            for (size_t i = 0; i + 1 < rd.line.size(); i++) {
                const Vector2 a = rd.line[i], b = rd.line[i + 1], ab = Vector2Subtract(b, a);
                const float t = std::clamp(Vector2DotProduct(Vector2Subtract(q, a), ab) / std::max(1e-4f, Vector2DotProduct(ab, ab)), 0.f, 1.f);
                if (Vector2Distance(q, Vector2Add(a, Vector2Scale(ab, t))) < 0.5f * rd.width + 0.6f) return false;
            }
        // keep the camera's way in over the crowd clear
        if (q.y > 100 && q.y < 200 && q.x > -10 && q.x < 70) return false;
        return true;
    };
    const Color post = mkColor(0x2f3a38);
    int treeCount = 0;
    for (const Road& rd : roads) {
        const bool main = rd.kind == "primary" || rd.kind == "secondary" || rd.kind == "trunk";
        const bool lined = main || rd.kind == "tertiary";
        float along = 0;
        for (size_t i = 0; i + 1 < rd.line.size(); i++) {
            const Vector2 a = rd.line[i], b = rd.line[i + 1];
            const float len = Vector2Distance(a, b);
            if (len < 0.05f) continue;
            const Vector2 dir = Vector2Scale(Vector2Subtract(b, a), 1 / len), left{ dir.y, -dir.x };
            for (float d = std::fmod(-along, 9.f) + (along > 0 ? 9.f : 0.f); d < len; d += 9.f) {   // dashed lane lines, 3 m on, 6 m off
                if (rd.lanes < 2 || rd.kind == "busway") break;
                const Vector2 p0 = Vector2Add(a, Vector2Scale(dir, d)), p1 = Vector2Add(a, Vector2Scale(dir, std::min(len, d + 3)));
                for (int k = 1; k < rd.lanes; k++) {
                    const float off = -0.5f * rd.width + rd.width * (float)k / (float)rd.lanes;
                    const Vector2 q0 = Vector2Add(p0, Vector2Scale(left, off)), q1 = Vector2Add(p1, Vector2Scale(left, off));
                    const Vector2 w = Vector2Scale(left, 0.07f);
                    marks.quad({ q0.x - w.x, 0.09f, q0.y - w.y }, { q1.x - w.x, 0.09f, q1.y - w.y }, { q1.x + w.x, 0.09f, q1.y + w.y },
                               { q0.x + w.x, 0.09f, q0.y + w.y }, white);
                }
            }
            if (lined) {
                for (float d = std::fmod(-along, 16.f) + (along > 0 ? 16.f : 0.f); d < len; d += 16.f) {
                    for (int sideK : { -1, 1 }) {
                        const Vector2 at = Vector2Add(Vector2Add(a, Vector2Scale(dir, d)), Vector2Scale(left, (float)sideK * (0.5f * rd.width + 2.4f)));
                        if (!clearOf(at, 1.0f)) continue;
                        const unsigned long long id = (unsigned long long)(treeCount++);
                        const bool lamp = main && (id % 2 == 0);
                        if (lamp) {                               // a street lamp leaning over the road
                            const Vector3 base{ at.x, 0, at.y };
                            const float yaw = std::atan2(-left.x * sideK, -left.y * sideK);
                            poles.box({ base.x, 4.5f, base.z }, { 0.09f, 4.5f, 0.09f }, 0, post);
                            const Vector3 reach{ -left.x * sideK * 1.4f, 0, -left.y * sideK * 1.4f };
                            poles.box({ base.x + reach.x, 8.95f, base.z + reach.z }, { 0.06f, 0.06f, 1.4f }, yaw, post);
                            heads.box({ base.x + 2 * reach.x, 8.85f, base.z + 2 * reach.z }, { 0.28f, 0.08f, 0.16f }, yaw, mkColor(0xf3e6c2));
                        } else {                                  // a rain tree: trunk and a broad, flattened crown
                            const float h = 4.5f + 2.5f * (float)rnd01(51, id), crown = 2.6f + 1.6f * (float)rnd01(52, id);
                            poles.box({ at.x, 0.5f * h, at.y }, { 0.22f, 0.5f * h, 0.22f }, (float)rnd01(53, id), mkColor(0x5a4630));
                            const Color leaf = rnd01(54, id) < 0.5 ? mkColor(0x3f6b37) : mkColor(0x4c7a3f);
                            trees.ball({ at.x, h + 0.6f * crown, at.y }, { crown, 0.6f * crown, crown }, leaf);
                            trees.ball({ at.x + 0.6f * crown, h + 0.9f * crown, at.y - 0.3f * crown }, { 0.7f * crown, 0.45f * crown, 0.7f * crown }, leaf);
                        }
                    }
                }
            }
            along += len;
        }
    }
    {
        g3d::Material m;
        m.roughness = 0.9f;
        m.castShadow = false;
        scene->add(marks.geometry(), m);
        g3d::Material dark;
        dark.roughness = 0.7f;
        scene->add(poles.geometry(), dark);
        g3d::Material leaves;
        leaves.roughness = 0.95f;
        scene->add(trees.geometry(), leaves);
        g3d::Material glow;
        glow.emissive = { 255, 226, 160, 255 };
        glow.castShadow = false;
        scene->add(heads.geometry(), glow);
    }
    // globe lamps on the pool's coping
    const Color lampGlow = { 255, 226, 160, 255 };
    auto part = [&](std::shared_ptr<g3d::Geometry> geom, Color col, Vector3 at, Color glow = BLACK, bool shadow = true) {
        g3d::Material m;
        m.color = col;
        m.roughness = 0.7f;
        m.emissive = glow;
        m.castShadow = shadow;
        g3d::Object3D* o = scene->add(std::move(geom), m);
        o->position = at;
        return o;
    };
    for (int i = 0; i < 24; i++) {
        const float a = 2 * PI * ((float)i + 0.5f) / 24, rr = POOL + 0.25f, x = rr * std::cos(a), z = rr * std::sin(a);
        part(g3d::Geometry::cylinder(0.08f, 3.0f, 10), post, { x, 1.5f, z });
        part(g3d::Geometry::sphere(0.3f, 10, 14), mkColor(0xf2eddc), { x, 3.15f, z }, lampGlow, false);
    }

    // ---- the monument: a stone island and base, the hollow pedestal frame, the cap, the figures
    g3d::Object3D* island = scene->add(g3d::Geometry::cylinder(8.0f, 1.5f, 64), stone);
    island->position = { 0, -0.7f, 0 };
    const Color grey = mkColor(0xd3cdc0), shade = mkColor(0xbdb6a8);
    box(*scene, { 9, BASE_TOP, 7 }, { 0, BASE_TOP * 0.5f, 0 }, grey);
    box(*scene, { 2 * SHAFT_HALF, SILL_TOP - BASE_TOP, 2 * SHAFT_HALF_Z }, { 0, 0.5f * (BASE_TOP + SILL_TOP), 0 }, grey);
    for (int k : { -1, 1 })
        box(*scene, { WALL, SHAFT_TOP - BASE_TOP, 2 * SHAFT_HALF_Z }, { (float)k * (SHAFT_HALF - 0.5f * WALL), 0.5f * (BASE_TOP + SHAFT_TOP), 0 }, grey);
    box(*scene, { 2 * CAP_HALF, CAP_TOP - SHAFT_TOP, 2 * CAP_HALF_Z }, { 0, 0.5f * (SHAFT_TOP + CAP_TOP), 0 }, shade);
    for (int k = 0; k < 2; k++) {
        auto ch = std::make_shared<rig::Character>(bronze());
        ch->motion.play([k](double) { return statuePose(k); }, 0, 0);
        g3d::Object3D* figure = rig::add(*scene, ch, FPS);
        figure->position = { k == 0 ? -0.65f : 0.65f, CAP_TOP, 0.2f };
        figure->rotation = { 0, 0, 0 };
    }

    // The barricade: concrete blocks across the mouth of Jl. Sudirman, police just behind.
    for (int i = -16; i <= 16; i++) {
        box(*scene, { 1.9f, 0.9f, 0.55f }, { CROWD_X + 1.95f * (float)i, 0.45f, STREET_Z }, i % 2 ? mkColor(0xc9c9c6) : mkColor(0xb7b7b4));
        box(*scene, { 1.9f, 0.14f, 0.57f }, { CROWD_X + 1.95f * (float)i, 0.72f, STREET_Z }, i % 2 ? mkColor(0xc4262e) : mkColor(0xeeeeee));
    }

    // The people. Idle students first, the cheering set from CHEER_AT; both play the same crowd, so
    // everyone is where they were.
    g3d::Material cloth;
    cloth.roughness = 0.9f;
    g3d::Object3D* calm = crowd::addInstanced(*scene, w->students, "students", bodies().idle, cloth, FPS);
    g3d::Object3D* glad = crowd::addInstanced(*scene, w->students, "students", bodies().cheer, cloth, FPS);
    const auto calmUp = calm->up, gladUp = glad->up;
    calm->up = [calmUp](int t, g3d::Object3D& o) { if (calmUp) calmUp(t, o); o.visible = t / (double)FPS < CHEER_AT; };
    glad->up = [gladUp](int t, g3d::Object3D& o) { if (gladUp) gladUp(t, o); o.visible = t / (double)FPS >= CHEER_AT; };
    crowd::addInstanced(*scene, w->police, "police", bodies().police, cloth, FPS);
    // Shields: clear panels braced in front of each officer (the rig's own shield bakes too faint to read).
    g3d::Material clear;
    clear.color = { 150, 190, 215, 255 };
    clear.opacity = 0.42f;
    clear.roughness = 0.15f;
    clear.castShadow = false;
    const int first = w->police->first("police");
    for (int i = 0; i < w->police->size(); i++) {
        const crowd::State st = w->police->at(first + i, 0);
        const float yaw = st.yaw * DEG2RAD;
        const Vector3 ahead{ std::sin(yaw), 0, std::cos(yaw) }, side{ std::cos(yaw), 0, -std::sin(yaw) };
        g3d::Object3D* panel = scene->add(g3d::Geometry::box(0.62f, 0.95f, 0.035f), clear);
        panel->position = { st.pos.x + ahead.x * 0.42f + side.x * 0.12f, 0.98f, st.pos.z + ahead.z * 0.42f + side.z * 0.12f };
        panel->rotation = { 0, st.yaw, 0 };
    }

    // Red-and-white flags on poles held up in the crowd: small cloths pinned down one edge to a pole, flown
    // by a gusting wind; the poles go up and pump when the crowd cheers.
    {
        auto red = std::make_shared<g3d::DynamicTexture>();
        red->width = 96;
        red->height = 64;
        red->supersample = 1;
        red->draw = [](const Ctx& c, int) {
            dr::rect(c, 0, 0, 96, 32, mkColor(0xd0202c));
            dr::rect(c, 0, 32, 96, 32, mkColor(0xf4f1ea));
        };
        g3d::Material cloth;
        cloth.dynamicMap = red;
        cloth.doubleSided = true;
        cloth.roughness = 0.85f;
        cloth.emissive = { 24, 22, 22, 255 };
        g3d::Material wood;
        wood.color = mkColor(0x6b5a44);
        wood.roughness = 0.8f;
        for (int k = 0; k < FLAG_COUNT; k++) {
            const auto top = [w, k](double s) { return flagTop(*w, k, s); };
            physics::Params fp;
            fp.fps = FPS;
            fp.substeps = 4;
            fp.iterations = 10;
            fp.damping = 0.03f;
            constexpr int FC = 16, FR = 11;
            constexpr float FS = 0.1f;
            physics::Cloth flag(FC, FR, FS, top(0), { 1, 0, 0 }, { 0, -1, 0 }, fp);
            for (int row = 0; row < FR; row++)
                flag.pin(row * FC, [top, drop = FS * (float)row](double s) { return Vector3Subtract(top(s), { 0, drop, 0 }); });
            flag.wind({ { 1, 0, 0.35f }, 7.5f, 0.45f, 0.9f, (unsigned)(31 + k), 1.8f, 2.5f });   // aerodynamic, gusts rolling down the flag
            scene->add(physics::clothObject(flag, cloth));
            g3d::Object3D* pole = scene->add(g3d::Geometry::cylinder(0.03f, 2.6f, 8), wood);
            pole->up = [top](int t, g3d::Object3D& o) { o.position = Vector3Subtract(top(t / (double)FPS), { 0, 1.25f, 0 }); };
        }
    }

    // The climbing ropes: one over the cap's lip for each climber, hanging down the side of the shaft.
    g3d::Material hemp;
    hemp.color = mkColor(0xb89c6a);
    hemp.roughness = 0.95f;
    g3d::Material iron;
    iron.color = mkColor(0x30343a);
    iron.roughness = 0.6f;
    for (int who = 0; who < 2; who++) {
        const float z = ropeZ(who);
        physics::Params rp;
        rp.fps = FPS;
        rp.substeps = 6;
        rp.iterations = 14;
        const float rx = side(who) * ROPE_X;
        physics::Rope rope({ rx, CAP_TOP - 0.2f, z }, { rx, BASE_TOP + 0.15f, z }, 64, rp);
        rope.pin(0, [rx, z](double) { return Vector3{ rx, CAP_TOP - 0.2f, z }; });
        rope.pinEnd([who, rx, z](double s) {                // hauled up after the climb, so nobody can follow
            const Vector3 rest{ rx, BASE_TOP + 0.15f, z };
            const float u = smooth((s - HAUL0) / (HAUL1 - HAUL0));
            if (u <= 0) return rest;
            const Vector3 lip{ rx * 1.05f, CAP_TOP + 0.9f, z }, hand = Vector3Add(climberAt(s, who), { -side(who) * 0.2f, 1.1f, 0.25f });
            if (u < 0.75f) return Vector3Lerp(rest, lip, u / 0.75f);                  // straight up the outside of the shaft
            return Vector3Lerp(lip, hand, (u - 0.75f) / 0.25f);                    // and over the lip into the hand
        });
        for (const physics::Collider& c : monumentColliders()) rope.collide(c);
        g3d::Object3D* line = scene->add(physics::ropeObject(rope, 0.03f, hemp));
        const auto inner = line->up;
        line->up = [inner](int t, g3d::Object3D& o) {
            if (inner) inner(t, o);
            o.visible = t / (double)FPS >= STANDOFF_AT;
        };
        // the hoisting line: from the climber's hands to the end of the roll
        const float end = side(who) * (0.5f * CELL * (COLS - 1) + 0.05f);
        const auto hand = [who](double s) { return Vector3Add(climberAt(s, who), { side(who) * -0.1f, 1.25f, 0 }); };
        const auto tie = [end](double s) {
            return Vector3{ end, std::min(climbY(s, 0), climbY(s, 0.2)) + 0.9f + ROLL_R0, HOIST_Z };
        };
        physics::Rope towLine(hand(CLIMB_AT), tie(CLIMB_AT), 14, rp);
        towLine.pin(0, hand);
        towLine.pin(14, tie, CARRY_DOWN);                    // untied at the top: the line falls and swings from the hand
        g3d::Object3D* tow = scene->add(physics::ropeObject(towLine, 0.02f, hemp));
        const auto towInner = tow->up;
        tow->up = [towInner](int t, g3d::Object3D& o) {
            if (towInner) towInner(t, o);
            const double s = t / (double)FPS;
            o.visible = s >= STANDOFF_AT;
        };
        // an iron cleat on the lip that the rope is made fast to
        g3d::Object3D* cleat = scene->add(g3d::Geometry::box(0.22f, 0.12f, 0.3f), iron);
        cleat->position = { side(who) * (CAP_HALF - 0.05f), CAP_TOP + 0.06f, z };
    }

    // The climbers: full characters, up the wall and onto the cap, then cheering.
    for (int who = 0; who < 2; who++) {
        auto ch = std::make_shared<rig::Character>(student(who));
        ch->motion.play([who](double s) -> rig::Pose {
            const Vector3 p = climberAt(s, who);
            if (s < WALK0) return climbPose((p.y - BASE_TOP) / 1.1f);
            if (s < UNROLL_AT) return carryPose();
            return rig::clips::cheer()(s);
        }, 0, 0);
        g3d::Object3D* group = scene->add();
        group->name = who == 0 ? "climber-a" : "climber-b";
        group->up = [who](int t, g3d::Object3D& o) {
            const double s = t / (double)FPS;
            o.position = climberAt(s, who);
            o.rotation = { 0, climberYaw(s, who), 0 };
            o.visible = s >= STANDOFF_AT;
        };
        rig::add(*group, ch, FPS);
    }

    // The banner.
    g3d::Material flag;
    flag.roughness = 0.9f;
    flag.emissive = { 34, 34, 40, 255 };
    flag.dynamicMap = bannerTexture();
    flag.doubleSided = true;
    g3d::Object3D* banner = scene->add(physics::clothObject(makeBanner(), flag));
    const auto inner = banner->up;
    banner->up = [inner](int t, g3d::Object3D& o) {
        if (inner) inner(t, o);
        o.visible = t / (double)FPS >= STANDOFF_AT;
    };
    return scene;
}

// ------------------------------------------------------------------ overlays and card
// A label tied to a point in the scene: a dot on the thing, a leader line, the API name and one line
// of what it is doing. Fades in and out over [from, to].
void callout(const Ctx& cx, double s, Vector3 at, const char* api, const char* note, double from, double to, Vector2 offset) {
    const float k = window(s, from, to);
    Vector2 p;
    if (k < 0.001f || !g3d::project(camera(s), at, { 0, 0, W, H }, p)) return;
    const Ctx m = faded(cx, k);
    const Vector2 q{ p.x + offset.x, p.y + offset.y };
    dr::strokeSeg(m, p, q, 2, ca(BONE, 220));
    dr::circle(m, p, 6, BONE);
    dr::circle(m, p, 3, TEAL);
    const float ax = offset.x < 0 ? 1.f : 0.f;
    const float tx = q.x + (offset.x < 0 ? -10.f : 10.f);
    // a dark backing so the label reads over the busy facades
    const float bw = std::max(17.f * (float)std::strlen(api), 11.f * (float)std::strlen(note)) + 30;
    dr::roundRect(m, offset.x < 0 ? tx - bw + 14 : tx - 14, q.y - 40, bw, 80, 10, ca(INK, 175));
    dr::text(code::font(true), m, api, { tx, q.y - 14 }, 28, 0, TEAL, ax, 0.5f);
    dr::text(fonts::ui(false), m, note, { tx, q.y + 18 }, 22, 0, BONE, ax, 0.5f);
}

void overlays(const Ctx& cx, int t) {
    const double s = t / (double)FPS;
    const float kt = window(s, 0.4, CARD_AT + 1.5);
    text(faded(cx, kt), "ILUSTRASI", { W - 96, 84 }, 22, BONE, 1, 0.5f, true, 6);
    if (const float k = window(s, 0.9, 4.3); k > 0.001f) {
        const Ctx m = faded(cx, k);
        // a scrim that is dark under the type (the plaza behind is mid-grey and busy), fading out above and to the right
        //            corners: top-left, top-right, bottom-right, bottom-left
        dr::rectGrad4(m, 0, H - 330, 700, 140, ca(INK, 0), ca(INK, 0), ca(INK, 190), ca(INK, 205));
        dr::rectGrad4(m, 0, H - 190, 700, 190, ca(INK, 205), ca(INK, 190), ca(INK, 210), ca(INK, 225));
        dr::rectGrad4(m, 700, H - 330, 600, 140, ca(INK, 0), ca(INK, 0), ca(INK, 0), ca(INK, 190));
        dr::rectGrad4(m, 700, H - 190, 600, 190, ca(INK, 190), ca(INK, 0), ca(INK, 0), ca(INK, 210));
        text(m, "BUNDARAN HI, JAKARTA", { 96, H - 116 }, 30, BONE, 0, 0.5f, true, 5);
        text(m, "Monumen Selamat Datang, 2026", { 96, H - 76 }, 24, ca(BONE, 225), 0, 0.5f);
        text(m, "Bangunan dan jalan: \xC2\xA9 kontributor OpenStreetMap", { 96, H - 42 }, 18, ca(BONE, 175), 0, 0.5f);
    }
    const World& w = *world();
    int shown = 0;                                   // the flag nearest the middle of the frame
    float best = 1e9f;
    for (int k = 0; k < FLAG_COUNT; k++) {
        Vector2 q;
        if (!g3d::project(camera(s), flagTop(w, k, s), { 0, 0, W, H }, q)) continue;
        const float d = std::fabs(q.x - W * 0.5f) + std::fabs(q.y - H * 0.55f);
        if (d < best) { best = d; shown = k; }
    }
    callout(cx, s, Vector3Add(flagTop(w, shown, s), { 0.7f, -0.3f, 0 }), "physics::Cloth", "pinned to a pole, flown by gusting wind", 5.4, 8.4, { 150, -150 });
    // on the east rope just below its climber, so the label rides up with the shot
    callout(cx, s, { ROPE_X, std::max(BASE_TOP + 1.5f, climbY(s, 0.2) - 2.5f), ropeZ(1) }, "physics::Rope", "64 segments, hung from the lip", 11.0, 13.6, { 230, 40 });
    callout(cx, s, { -ROPE_X * 1.05f, CAP_TOP + 0.3f, ropeZ(0) }, "physics::Rope", "hauled up: the free end follows a hand", 14.3, 16.2, { -230, -60 });
    callout(cx, s, { 0, 12.0f, ATTACH_Z + 0.05f }, "physics::Cloth", "11 \xC3\x97 77, the rows let go one by one", 17.4, 20.6, { 260, -40 });
}

void endCard(const Ctx& cx, int t) {
    const float kt = smooth(t / 14.0), ka = smooth((t - 18) / 14.0);
    Ctx tm = faded(cx, kt);
    tm.o.y += (1 - kt) * 20 * tm.s;
    dr::rectGrad4(faded(cx, kt), 0, 700, W, 140, ca(INK, 0), ca(INK, 0), INK, INK);
    dr::rect(faded(cx, kt), 0, 840, W, 240, INK);
    text(tm, "#INDONESIASADAR", { W / 2, 878 }, 84, BONE, 0.5f, 0.5f, true, 3);
    dr::text(code::font(false), faded(cx, ka), "physics::Cloth  \xC2\xB7  physics::Rope  \xC2\xB7  city::load", { W / 2, 954 }, 28, 0, TEAL, 0.5f, 0.5f);
    dr::text(fonts::ui(false), faded(cx, ka), "Map data \xC2\xA9 OpenStreetMap contributors (ODbL)", { W - 40, H - 28 }, 16, 0, ca(DIM, 200), 1, 0.5f);
}

// ------------------------------------------------------------------ sound (brief section 7)
using procgen::Score;
constexpr int RATE = Score::RATE;

void genScore(const std::string& path) {
    Score s(TOTAL / (double)FPS + 1);
    for (int n : { 45, 52, 57, 60 }) s.pad(0, CLIMB_AT + 0.4, n, 0.05f, 700, 0.5f);                  // the standoff: a held low chord
    s.bass(0, CLIMB_AT, 28, 0.1f);
    for (double t = CLIMB_AT; t < UNROLL_AT - 0.3; t += 0.5 - 0.18 * (t - CLIMB_AT) / (UNROLL_AT - CLIMB_AT))
        s.pluck(t, 50 + (int)std::lround(8 * (t - CLIMB_AT) / (UNROLL_AT - CLIMB_AT)), 0.05f, 0.25f, 4);   // the climb's pulse, quickening
    s.riser(UNROLL_AT - 1.2, UNROLL_AT, 0.1f);
    s.impact(UNROLL_AT, 0.18f);
    for (int n : { 52, 59, 64, 71 }) s.pad(UNROLL_AT, CARD_AT - UNROLL_AT + 0.4, n, 0.06f, 1300, 0.4f);
    s.bass(UNROLL_AT, CARD_AT - UNROLL_AT, 33, 0.1f);
    s.impact(CARD_AT + 0.1, 0.24f);
    for (int j = 0; j < 4; j++) s.bell(CARD_AT + 0.15 + 0.12 * j, 71 + j * 3, 0.06f, 0.3f + 0.12f * j);
    for (int n : { 52, 59, 64, 71 }) s.pad(CARD_AT, TOTAL / (double)FPS - CARD_AT, n, 0.07f, 1300, 0.5f);

    // Wind and a crowd's murmur: filtered noise, the wind swelling with the unroll and gone before the card.
    auto bed = [&](double a, double b, float amp, float lo, float hi, float pan) {
        const int i0 = (int)(a * RATE), len = (int)((b - a) * RATE);
        procgen::Svf f;
        for (int k = 0; k < len; k++) {
            const double u = (double)k / len;
            const float env = (float)std::sin(PI * u);
            s.put(i0 + k, f.bp(s.noise(), lo + (hi - lo) * (float)u, 0.6f, RATE) * env * amp, pan, 0.3f, 0);
        }
    };
    bed(1.0, UNROLL_AT, 0.05f, 300, 700, 0.5f);                  // the crowd, murmuring
    bed(UNROLL_AT - 0.6, CARD_AT, 0.07f, 900, 2600, 0.5f);       // the wind
    bed(CHEER_AT - 0.2, CARD_AT + 0.4, 0.07f, 500, 1500, 0.5f);  // the cheer

    std::vector<float> mix = s.mix();
    for (float& x : mix) x = std::tanh(x * 3.0f);
    float peak = 0;
    for (float x : mix) peak = std::max(peak, std::fabs(x));
    const float g = peak > 0 ? 0.43f / peak : 1.f;
    for (float& x : mix) x *= g;
    media::writeWav(path, mix, RATE);
}

void ensureAssets() {
    std::filesystem::create_directories(ASSETS);
    const std::string score = ASSETS + "/score.wav";
    if (!std::filesystem::exists(score)) genScore(score);
}

void build(Comp& c, const Json&) {
    c.roots.clear();
    const auto w = world();
    const auto scene = makeScene(w);
    addRoot(c.roots, 0, TOTAL)->fn = [scene](const Ctx& cx, int t) {
        dr::rect(cx, 0, 0, W, H, SKY);
        g3d::render(*scene, t, cx);
        overlays(cx, t);
    };
    const int e = (int)(CARD_AT * FPS);
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

void registerBundaranHiBanner(Registry& out) {
    Comp c;
    c.name = "bundaran-hi-banner";
    c.w = (int)W; c.h = (int)H; c.fps = FPS;
    c.duration = TOTAL;
    c.background = SKY;
    c.assets = [](const Comp&, const Json&) { ensureAssets(); };
    c.build = [](Comp& c, const Json& p) { build(c, p); };
    out.push_back(std::move(c));
}

} // namespace rmtn
