// far_and_near.cpp -- "far-and-near": the engine demo for raymotion 0.13 (not a film). One forest,
// seen from every distance, from a globe down to a blade of grass. At each distance the same
// frame is wiped between the old way and the new, with the engine's own counts under each side:
//   1 globe     geo3d::forest, ForestCover::impostorBelow / mergeBelow (0 = every tree a mesh)
//   2 valley    TreeBuild::elevations, ForestSpec::impostorShadows, Scene::environmentFaces
//   3 low       ForestSpec::canopyBelow (trees only against the far canopy)
//   4 ground    g3d::Field::shadowDensity (tufts in a gust: a third of the blades in the shadow map)
//   5 2D        flora2d::meadow(path) with motion blur on the tips
// Every number on screen is read from the scene (bodies, triangles, shadow blades, placed trees);
// where the trees stand is illustrative. Scenes are built at the first frame that draws them, so
// `raymotion list` pays nothing. The score is generated (procgen::Score) into out/far-and-near-assets.
// Set RMTN_FAN_CLEARANCE=<file> to log the camera's height over the ground at every frame it is
// asked for (the QC check that no camera is under the terrain or inside the trees).
//   raymotion render far-and-near
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

constexpr int FPS = 30, XF = 20;
constexpr int LEN[6] = { 390, 330, 330, 240, 120, 150 };           // scene lengths
constexpr int START[6] = { 0, 370, 680, 990, 1210, 1310 };         // scene starts: LEN summed, less XF each
constexpr int TOTAL = 1460;
static_assert(START[1] == LEN[0] - XF && START[2] == START[1] + LEN[1] - XF && START[3] == START[2] + LEN[2] - XF &&
              START[4] == START[3] + LEN[3] - XF && START[5] == START[4] + LEN[4] - XF && TOTAL == START[5] + LEN[5],
              "the scene starts are the lengths less the overlaps");

const Color INK = mkColor(0xf2f4ff), SUB = mkColor(0xb4bfcc), AMBER = mkColor(0xffd27a), TEAL = mkColor(0x3ad6c5);
const Color SCRIM = Color{ 12, 16, 22, 255 };

Font bold() { return fonts::ui(true); }
Font regular() { return fonts::ui(false); }

float sm(double t, double a, double b) {                              // smoothstep of t between a and b
    const double u = std::clamp((t - a) / (b - a), 0.0, 1.0);
    return (float)(u * u * (3 - 2 * u));
}
float ramp(int t, double a, double b) { return (float)interpolate(t, { a, b }, { 0, 1 }, clamped(ease::out(ease::cubic))); }
Ctx faded(const Ctx& cx, float a) { Ctx c = cx; c.a *= std::clamp(a, 0.f, 1.f); return c; }

std::string thousands(long long v) {
    std::string d = std::to_string(v), out;
    for (size_t i = 0; i < d.size(); i++) {
        if (i && (d.size() - i) % 3 == 0) out += ',';
        out += d[i];
    }
    return out;
}
std::string triangles(long long v) {
    char s[32];
    if (v >= 1000000) std::snprintf(s, sizeof s, "%.1f million", v / 1e6);
    else return thousands(v);
    return s;
}

// ---------------------------------------------------------------- the wipes

// A wipe on the same frame: the "before" picture comes in from the left edge to the middle, rests
// there, and goes back. `in` and `out` are scene-local frames (the one table the picture, the labels
// and the score all read).
struct Wipe {
    int in, out;
    static constexpr int RAMP = 24;
    int rest() const { return in + RAMP; }
    int gone() const { return out + RAMP; }
    float x(int t) const { return 960.f * (sm(t, in, in + RAMP) - sm(t, out, out + RAMP)); }   // the divider, px
};
constexpr Wipe WIPE[4] = { { 126, 252 }, { 120, 240 }, { 102, 236 }, { 78, 180 } };

// A scene of the film: built at the first draw, drawn with the engine's counters read straight after.
struct Shot {
    std::function<g3d::ScenePtr(Shot&)> make;
    g3d::ScenePtr scene;
    long long bodies = 0, triangles = 0, shadowBlades = 0, trees = 0;
    g3d::Scene& get() {
        if (!scene) scene = make(*this);
        return *scene;
    }
};
using ShotP = std::shared_ptr<Shot>;

void drawShot(const ShotP& s, const Ctx& cx, int t) {
    g3d::render(s->get(), t, cx);
    s->bodies = g3d::instanceBodiesSubmitted();                      // assigned each frame, read by the labels of the same frame
    s->triangles = g3d::instanceTrianglesSubmitted();
    s->shadowBlades = g3d::fieldShadowBladesSubmitted();
}

struct Side { std::string head, sub; };
enum class Stat { Bodies, ShadowBlades, None };

void statLine(const Ctx& cx, float x, float y, const std::string& number, const char* unit, bool right) {
    const float nw = MeasureTextEx(bold(), number.c_str(), 44, 0).x, uw = MeasureTextEx(regular(), unit, 24, 0).x;
    const float x0 = right ? x - nw - 12 - uw : x;
    dr::text(bold(), cx, number.c_str(), { x0, y }, 44, 0, TEAL, 0, 0);
    dr::text(regular(), cx, unit, { x0 + nw + 12, y + 16 }, 24, 0, SUB, 0, 0);
}

// The two side labels, the divider and a scrim behind them, over a wipe.
void wipeLabels(const Ctx& cx, int t, const Wipe& w, const Side& before, const Side& after, const Shot& b, const Shot& a, Stat stat) {
    const float k = ramp(t, w.rest() - 8, w.rest() + 10) * (1 - ramp(t, w.out - 6, w.out + 8));
    const float x = w.x(t);
    if (x > 2) {
        dr::rect(cx, x - 2, 0, 4, 1080, AMBER);
        const Vector2 tri1[3] = { { x - 8, 540 - 12 }, { x - 8, 540 + 12 }, { x - 20, 540 } }, tri2[3] = { { x + 8, 540 - 12 }, { x + 8, 540 + 12 }, { x + 20, 540 } };
        dr::circle(cx, { x, 540 }, 26, AMBER);
        dr::fillPoly(cx, tri1, 3, SCRIM);
        dr::fillPoly(cx, tri2, 3, SCRIM);
    }
    if (k <= 0) return;
    const Ctx c = faded(cx, k);
    dr::rectGrad4(c, 0, 0, 1920, 300, withAlpha(SCRIM, 0.62f), withAlpha(SCRIM, 0.62f), withAlpha(SCRIM, 0), withAlpha(SCRIM, 0));
    auto side = [&](const Side& s, const Shot& shot, float edge, bool right, Color head) {
        const float hw = MeasureTextEx(bold(), s.head.c_str(), 40, 0).x, sw = MeasureTextEx(regular(), s.sub.c_str(), 24, 0).x;
        dr::text(bold(), c, s.head.c_str(), { right ? edge - hw : edge, 56 }, 40, 0, head, 0, 0);
        dr::text(regular(), c, s.sub.c_str(), { right ? edge - sw : edge, 106 }, 24, 0, SUB, 0, 0);
        if (stat == Stat::Bodies) {
            statLine(c, edge, 150, thousands(shot.bodies), "draws", right);
            statLine(c, edge, 204, triangles(shot.triangles), "triangles", right);
        } else if (stat == Stat::ShadowBlades) {
            statLine(c, edge, 150, thousands(shot.shadowBlades), "blades in the shadow pass", right);
        }
    };
    side(before, b, 960 - 40, true, SUB);
    side(after, a, 960 + 40, false, INK);
}

// The headline pill of a wipe: "lead BIG tail", and an optional second line "lead2 BIG2 tail2".
void pill(const Ctx& cx, float k, const char* lead, const char* big, const char* tail, const char* lead2, const char* big2, const char* tail2) {
    const Ctx c = faded(cx, k);
    const float wa = MeasureTextEx(regular(), lead, 40, 0).x, wb = MeasureTextEx(bold(), big, 64, 0).x, wd = MeasureTextEx(regular(), tail, 40, 0).x;
    float w2 = 0, a2 = 0, b2 = 0;
    if (lead2) {
        a2 = MeasureTextEx(regular(), lead2, 40, 0).x;
        b2 = MeasureTextEx(bold(), big2, 64, 0).x;
        w2 = a2 + 16 + b2 + 8 + MeasureTextEx(regular(), tail2, 40, 0).x;
    }
    const float w1 = wa + 16 + wb + 8 + wd, total = std::max(w1, w2) + 56, x0 = 960 - total / 2, h = lead2 ? 168.f : 100.f;
    dr::roundRect(c, x0, 280, total, h, 20, withAlpha(SCRIM, 0.84f));
    const float xs = 960 - w1 / 2;
    dr::text(regular(), c, lead, { xs, 308 }, 40, 0, INK, 0, 0);
    dr::text(bold(), c, big, { xs + wa + 16, 292 }, 64, 0, TEAL, 0, 0);
    dr::text(regular(), c, tail, { xs + wa + 16 + wb + 8, 308 }, 40, 0, INK, 0, 0);
    if (lead2) {
        const float x2 = 960 - w2 / 2;
        dr::text(regular(), c, lead2, { x2, 380 }, 40, 0, INK, 0, 0);
        dr::text(bold(), c, big2, { x2 + a2 + 16, 364 }, 64, 0, AMBER, 0, 0);
        dr::text(regular(), c, tail2, { x2 + a2 + 16 + b2 + 8, 380 }, 40, 0, INK, 0, 0);
    }
}

// Caption card (as flora.cpp): what the chapter shows, and the call that makes it.
struct Chapter { std::string title, note, code; };
void caption(TNode* n, int len, const Chapter& ch, int at) {
    TNode* card = n->add(at, len - at);
    card->up = [len, at](int t, NodeProps& p) {
        const float in = (float)interpolate(t, { 0, 18 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        const float out = (float)interpolate(t + at, { len - 40.0, len - 22.0 }, { 1, 0 }, clamped());
        p.opacity = in * out;
        p.pos = { 0, 24 * (1 - in) };
    };
    card->fn = [ch](const Ctx& cx, int) {
        const float w = std::max({ MeasureTextEx(regular(), ch.code.c_str(), 26, 0).x, MeasureTextEx(bold(), ch.title.c_str(), 60, 0).x,
                                   MeasureTextEx(regular(), ch.note.c_str(), 28, 0).x }) + 80;
        const float y = 1080 - 64 - 186;
        dr::roundRect(cx, 64, y, w, 186, 18, withAlpha(SCRIM, 0.72f));
        dr::text(bold(), cx, ch.title.c_str(), { 104, y + 30 }, 60, 0, INK, 0, 0);
        dr::text(regular(), cx, ch.note.c_str(), { 104, y + 100 }, 28, 0, SUB, 0, 0);
        dr::text(regular(), cx, ch.code.c_str(), { 104, y + 142 }, 26, 0, AMBER, 0, 0);
    };
}

// A counter, top left: `number unit` pairs on one line, in and out by scene-local frame.
struct Part { std::string number, unit; };
void counter(const Ctx& cx, int t, double in, double out, const std::vector<Part>& parts, float y0 = 56) {
    const float k = ramp(t, in, in + 15) * (1 - ramp(t, out, out + 15));
    if (k <= 0) return;
    const Ctx c = faded(cx, k);
    float w = 0;
    for (const Part& p : parts) w += MeasureTextEx(bold(), p.number.c_str(), 56, 0).x + 14 + MeasureTextEx(regular(), p.unit.c_str(), 28, 0).x + 34;
    dr::roundRect(c, 64, y0 - 14 * (1 - k), w + 24, 92, 16, withAlpha(SCRIM, 0.7f));
    float x = 82;
    for (const Part& p : parts) {
        dr::text(bold(), c, p.number.c_str(), { x, y0 + 10 - 14 * (1 - k) }, 56, 0, TEAL, 0, 0);
        x += MeasureTextEx(bold(), p.number.c_str(), 56, 0).x + 14;
        dr::text(regular(), c, p.unit.c_str(), { x, y0 + 34 - 14 * (1 - k) }, 28, 0, INK, 0, 0);
        x += MeasureTextEx(regular(), p.unit.c_str(), 28, 0).x + 34;
    }
}

// The wipe as nodes: the "after" scene under everything, the "before" scene on top clipped to the
// left of the divider (only while the divider is out). Returns nothing; labels are drawn by the caller.
void wipeNodes(TNode* n, int len, const Wipe& w, const ShotP& after, const ShotP& before) {
    n->add(0, len)->fn = [after](const Ctx& cx, int t) { drawShot(after, cx, t); };
    const int from = w.in - 1, dur = w.gone() + 2 - from;
    TNode* b = n->add(from, dur);
    b->isolate().mask = [w, from](const Ctx& cx, int t) { dr::rect(cx, 0, 0, w.x(t + from), 1080, mkColor(0xffffff)); };
    b->fn = [before, from](const Ctx& cx, int t) { drawShot(before, cx, t + from); };
}

// ---------------------------------------------------------------- the camera log (QC)

void logClearance(int chapter, int t, float clearance) {
    static const char* path = std::getenv("RMTN_FAN_CLEARANCE");
    if (!path) return;
    if (FILE* f = std::fopen(path, "a")) {
        std::fprintf(f, "%d %d %.3f\n", chapter, t, clearance);
        std::fclose(f);
    }
}

struct Pose { Vector3 pos, target; float clearance; };

// ---------------------------------------------------------------- the sky and the sun

const char* FACES[6] = { "assets/textures/skybox-example/px.png", "assets/textures/skybox-example/nx.png",
                         "assets/textures/skybox-example/py.png", "assets/textures/skybox-example/ny.png",
                         "assets/textures/skybox-example/pz.png", "assets/textures/skybox-example/nz.png" };
// The sun painted in the faces (found by looking: see the brief), about 7 degrees up, ahead and right.
const Vector3 SUN = Vector3Normalize({ 0.78f, 0.28f, -0.56f });
const Color SUNLIGHT = mkColor(0xffc890);

void skyFaces(g3d::Scene& s) {
    for (const char* f : FACES) s.environmentFaces.push_back(f);
    s.showEnvironment = true;
    s.environmentIntensity = 1.6f;
    s.fogFromSky = true;
}

// ---------------------------------------------------------------- 1: the globe

const char* REGIONS[] = { "RUS", "FIN", "SWE", "NOR", "EST", "LVA", "LTU", "BLR" };
constexpr double GLOBE_LON = 40, GLOBE_LAT = 60;

std::vector<flora::TreePtr> lowPolyTrees() {                         // what geo3d::forest builds when given none
    flora::TreeBuild b;
    b.style = flora::TreeStyle::LowPoly;
    b.frames = 1;
    b.views = 8;
    b.viewSize = 40;
    b.elevations = 4;
    std::vector<flora::TreePtr> trees;
    for (const char* n : { "oak", "birch", "pine" }) trees.push_back(flora::makeTree(flora::tree(n), b));
    return trees;
}

struct GlobeShared {
    std::shared_ptr<geo3d::Surface> surface = std::make_shared<geo3d::Surface>(geo::Projection::equalEarth(), geo::LonLat{ GLOBE_LON, GLOBE_LAT }, 1.f);
    std::vector<flora::TreePtr> trees;
};

Pose globePose(const geo3d::Surface& s, int t) {
    // From 8 globe radii in to 6.8, where a tree is about 7 px tall and the wipe is held, then a dive to 0.8.
    const double ld = std::log(8.0) + (std::log(6.8) - std::log(8.0)) * sm(t, 0, 150) + (std::log(0.8) - std::log(6.8)) * sm(t, 258, 390);
    const float d = (float)std::exp(ld), el = (90.f - 10.f * sm(t, 0, 250) - 25.f * sm(t, 258, 390)) * DEG2RAD;
    const geo::LonLat at{ GLOBE_LON, GLOBE_LAT };
    const Vector3 focus = s.point(at, 0), up = s.up(at), north = s.north(at);
    const Vector3 dir = Vector3Subtract(Vector3Scale(up, std::sin(el)), Vector3Scale(north, std::cos(el)));
    return { Vector3Add(focus, Vector3Scale(dir, d)), focus, d * std::sin(el) };       // the clearance: height over the surface
}

ShotP globeShot(const std::shared_ptr<GlobeShared>& g, bool after) {
    auto shot = std::make_shared<Shot>();
    shot->make = [g, after](Shot& self) {
        if (g->trees.empty()) g->trees = lowPolyTrees();
        auto s = std::make_shared<g3d::Scene>();
        s->ambient = 0.42f;
        s->lights.push_back(g3d::Light::directional({ 3, 2.5f, 6 }, mkColor(0xfff2e0), 1.35f));
        s->camera.fov = 30;
        s->camera.nearPlane = 0.02f;
        s->camera.farPlane = 40;
        g3d::Object3D* world = s->add();
        g3d::Material land;
        {
            globe::Map map(2048, mkColor(0x0d1a33));
            map.fill(geo::land().merged(), mkColor(0x39483a)).stroke(geo::countries().merged(), mkColor(0x5a6d58), 1.2f);
            land.dynamicMap = map.texture();
        }
        land.roughness = 0.95f;
        geo3d::base(*world, g->surface, land, 192);
        std::vector<geo::Geometry> regions;
        for (const char* iso : REGIONS) regions.push_back(geo::countries().get(iso).geometry);
        geo3d::ForestCover cover;
        cover.trees = g->trees;
        cover.spacing = 0.1f;
        cover.limit = 100000;
        if (!after) { cover.impostorBelow = 0; cover.mergeBelow = 0; }
        g3d::Object3D* forest = geo3d::forest(*world, g->surface, regions, cover);
        // How many trees were placed: the instances of the full trees, before any level is chosen (outside a draw
        // every tree is a mesh). Read, never typed.
        forest->up(0, *forest);
        long long n = 0;
        for (const auto& tr : g->trees) {                // each tree has leaves, or wood alone: count that body
            const bool leaves = tr->hero.leaves && tr->hero.leaves->frames > 0;
            for (const auto& ch : forest->children)
                if (ch->instances && ch->instances->mesh == (leaves ? tr->hero.leaves : tr->hero.wood)) n += (long long)ch->instances->list.size();
        }
        self.trees = n;
        const auto surface = g->surface;
        s->camera.up = [surface](int t, g3d::Camera& cam) {
            const Pose p = globePose(*surface, t);
            cam.position = p.pos;
            cam.target = p.target;
            cam.upVector = { 0, 1, 0 };
        };
        return s;
    };
    return shot;
}

void globeScene(TNode* n) {
    auto g = std::make_shared<GlobeShared>();
    ShotP after = globeShot(g, true), before = globeShot(g, false);
    const Wipe& w = WIPE[0];
    wipeNodes(n, LEN[0], w, after, before);
    n->add(0, LEN[0])->fn = [after, before, w](const Ctx& cx, int t) {
        // kicker: the conceit, first
        const float k = ramp(t, 6, 18) * (1 - ramp(t, 100, 114));
        if (k > 0) {
            const Ctx c = faded(cx, k);
            dr::roundRect(c, 64, 56, 1030, 138, 18, withAlpha(SCRIM, 0.62f));
            dr::text(bold(), c, "ONE FOREST. EVERY DISTANCE.", { 96, 74 + 12 * (1 - k) }, 64, 0, INK, 0, 0);
            dr::text(regular(), c, "raymotion 0.13: far trees, far canopy, grass shadows, a cube skybox", { 98, 150 }, 26, 0, SUB, 0, 0);
        }
        const std::string trees = thousands(after->trees), drawn = thousands(after->bodies);
        counter(cx, t, 60, w.in - 6, { { trees, "trees" }, { drawn, "draws" } }, 214);
        counter(cx, t, w.gone() + 18, 1e9, { { trees, "trees" }, { drawn, "draws" } }, 56);
        wipeLabels(cx, t, w, { "BEFORE", "every tree a mesh: impostorBelow 0, mergeBelow 0" }, { "AFTER", "the 0.13 defaults: pictures, far trees merged" },
                   *before, *after, Stat::Bodies);
    };
    caption(n, 292, { "A globe of trees", "trees under impostorBelow px become pictures; the farthest are merged",
                         "ForestCover { impostorBelow, mergeBelow, pictureSpeckle }" }, w.rest() + 14);
}

// ---------------------------------------------------------------- 2 and 3: the valley forest

constexpr float RX = 1500, Z0 = -3600;                                 // the forest: x in [-RX, RX], z in [Z0, 0]

float groundAt(float x, float z) { return 0.00002f * x * x + 16 * flora::noise(x, z, 700, 3) + 6 * flora::noise(x, z, 240, 5); }

struct Valley {
    flora::Area land = flora::rect({ -RX - 300, Z0 - 300 }, { RX + 300, 300 });
    std::vector<flora::Placement> at;
    std::vector<flora::TreePtr> trees3, trees1;                        // elevations 3 (0.13) and 1 (0.12)
    g3d::GeometryPtr terrain;
    Valley() {
        land.ground = groundAt;
        flora::ScatterSpec sc;
        sc.spacing = 7.5f;
        sc.clumping = 0.4f;
        sc.clumpScale = 90;
        sc.variants = 4;
        flora::Area wood = flora::rect({ -RX, Z0 }, { RX, 0 });
        wood.ground = groundAt;
        at = flora::scatter(wood, sc, 11);
        for (const char* name : { "oak", "birch", "pine", "spruce" }) {
            flora::TreeBuild b3, b1;
            b1.elevations = 1;
            trees3.push_back(flora::makeTree(flora::tree(name), b3));
            trees1.push_back(flora::makeTree(flora::tree(name), b1));
        }
        terrain = flora::terrain(land, 254, 254);
    }
};

// One path for chapters 2 and 3 (T is frames from chapter 2's start; chapter 3 starts at T = 310): a flight
// 40 m over the trees looking 52 degrees down, then a crane up and back to a long view over the forest.
Pose valleyPose(int T) {
    const float fwd = 100 * sm(T, 0, 260), back = 260 * sm(T, 190, 380);
    const float x = 25 * std::sin(T * 0.004f), z = -260 - fwd + back;
    const float H = 55 + 240 * sm(T, 190, 360) - 30 * sm(T, 400, 640);
    const float pitch = (52.f - 43.f * sm(T, 190, 360)) * DEG2RAD;
    float floorY = groundAt(x, z);
    for (float ahead : { 20.f, 40.f }) floorY = std::max(floorY, groundAt(x, z - ahead));
    const Vector3 pos{ x, floorY + H, z };
    const Vector3 dir{ 0.06f, -std::sin(pitch), -std::cos(pitch) };
    return { pos, Vector3Add(pos, Vector3Scale(dir, 100)), pos.y - groundAt(x, z) };
}

struct ValleyLook { int chapter, offset; float fov; float impostorFrom; bool shadowsOn; };

ShotP valleyShot(const std::shared_ptr<std::shared_ptr<Valley>>& lazy, ValleyLook look, bool oldTrees, float canopyBelow) {
    auto shot = std::make_shared<Shot>();
    shot->make = [lazy, look, oldTrees, canopyBelow](Shot& self) {
        if (!*lazy) *lazy = std::make_shared<Valley>();
        const Valley& v = **lazy;
        self.trees = (long long)v.at.size();
        auto s = std::make_shared<g3d::Scene>();
        skyFaces(*s);
        s->fogNear = 500;
        s->fogFar = 3500;
        s->shadowMapSize = 4096;
        s->camera.fov = look.fov;
        s->camera.nearPlane = 0.5f;
        s->camera.farPlane = 6000;
        g3d::Light sun = g3d::Light::directional(Vector3Scale(SUN, 400), SUNLIGHT, 3.1f, true);
        sun.shadowExtent = look.chapter == 2 ? 150.f : 320.f;
        const int offset = look.offset, chapter = look.chapter;
        s->lights.push_back(sun);
        s->lights[0].up = [offset](int t, g3d::Light& l) {
            const Pose p = valleyPose(t + offset);
            Vector3 focus = p.pos;
            focus.y = groundAt(focus.x, focus.z);
            focus.z -= 100;                                              // the shadow map lies mostly ahead
            l.target = focus;
            l.position = Vector3Add(focus, Vector3Scale(SUN, 400));
        };
        g3d::Material ground;
        ground.color = mkColor(0x4c6b34);
        ground.roughness = 1;
        s->add(v.terrain, ground);
        flora::ForestSpec spec;
        spec.fps = FPS;
        spec.wind = 0.8f;
        spec.impostorFrom = look.impostorFrom;
        spec.impostorShadows = look.shadowsOn;
        spec.canopyBelow = canopyBelow;
        flora::addForest(*s, v.at, oldTrees ? v.trees1 : v.trees3, spec);
        s->camera.up = [offset, chapter](int t, g3d::Camera& cam) {
            const Pose p = valleyPose(t + offset);
            logClearance(chapter, t, p.clearance);
            cam.position = p.pos;
            cam.target = p.target;
        };
        return s;
    };
    return shot;
}

// A ring on the ground where a tree is `d` metres from the eye (3D distance), over the terrain.
void groundRing(const Ctx& cx, const g3d::Camera& cam, Vector3 eye, float d, Color col) {
    std::vector<Vector2> pts;
    std::vector<bool> ok;
    for (int k = 0; k <= 120; k++) {
        const float a = (float)k / 120 * 2 * PI;
        float r = d;
        for (int it = 0; it < 4; it++) {
            const float gy = groundAt(eye.x + r * std::cos(a), eye.z + r * std::sin(a));
            const float dy = eye.y - gy;
            r = d > std::fabs(dy) ? std::sqrt(d * d - dy * dy) : 0.f;
        }
        const Vector3 w{ eye.x + r * std::cos(a), groundAt(eye.x + r * std::cos(a), eye.z + r * std::sin(a)) + 0.5f, eye.z + r * std::sin(a) };
        Vector2 p;
        const bool vis = r > 1 && g3d::project(cam, w, { 0, 0, 1920, 1080 }, p) && p.x > -300 && p.x < 2220 && p.y > -300 && p.y < 1380;
        pts.push_back({ p.x, p.y });
        ok.push_back(vis);
    }
    Ctx raw = cx;
    raw.o = { 0, 0 };
    raw.s = 1;
    raw.rot = 0;
    for (size_t k = 0; k + 1 < pts.size(); k++)
        if (ok[k] && ok[k + 1]) dr::strokeSeg(raw, pts[k], pts[k + 1], 4, withAlpha(col, cx.a));
}

// Aerial perspective for the skybox's hills: the cube faces carry a dark hill band that fog cannot touch (fog
// only reaches geometry), so a haze toward the sky's horizon colours is laid over it, rising from the horizon.
void haze(const Ctx& cx, int offset, float fov, int t, float k) {
    g3d::Camera cam;
    const Pose p = valleyPose(t + offset);
    cam.position = p.pos;
    cam.target = p.target;
    cam.fov = fov;
    Vector2 h;
    if (!g3d::project(cam, { p.pos.x, p.pos.y, p.pos.z - 30000 }, { 0, 0, 1920, 1080 }, h)) return;
    const float y1 = std::clamp(h.y + 130, 0.f, 1080.f), y0 = std::clamp(h.y - 240, 0.f, 1080.f);
    if (y1 <= y0 + 1) return;
    const Color left = mkColor(0xd6b4ac), right = mkColor(0xf2d4a8);
    const float a = 0.9f * k;
    const float mid = std::clamp(h.y, y0, y1);
    dr::rectGrad4(cx, 0, y0, 1920, mid - y0, withAlpha(left, 0), withAlpha(right, 0), withAlpha(left, a), withAlpha(right, a));
    dr::rectGrad4(cx, 0, mid, 1920, y1 - mid, withAlpha(left, a), withAlpha(right, a), withAlpha(left, 0), withAlpha(right, 0));
}

void valleyScene(TNode* n, const std::shared_ptr<std::shared_ptr<Valley>>& lazy) {
    const float impostorFrom = 70;
    const Wipe& w = WIPE[1];
    ShotP after = valleyShot(lazy, { 2, 0, 60, impostorFrom, true }, false, 0);
    ShotP before = valleyShot(lazy, { 2, 0, 60, impostorFrom, false }, true, 0);
    wipeNodes(n, LEN[1], w, after, before);
    n->add(0, LEN[1])->fn = [after, before, w, impostorFrom](const Ctx& cx, int t) {
        haze(cx, 0, 60, t, 1);
        // the distance band: a ring where trees become pictures
        const float k = ramp(t, 18, 32) * (1 - ramp(t, 100, 114));
        if (k > 0) {
            g3d::Camera cam;
            const Pose p = valleyPose(t);
            cam.position = p.pos;
            cam.target = p.target;
            cam.fov = 60;
            groundRing(faded(cx, k), cam, p.pos, impostorFrom, AMBER);
            Vector2 at;
            const float r = std::sqrt(std::max(impostorFrom * impostorFrom - p.clearance * p.clearance, 1.f));
            if (g3d::project(cam, { p.pos.x, groundAt(p.pos.x, p.pos.z - r) + 0.5f, p.pos.z - r }, { 0, 0, 1920, 1080 }, at)) {
                const Ctx c = faded(cx, k);
                const std::string l1 = "trees beyond this line are pictures", l2 = "ForestSpec::impostorFrom = " + thousands((long long)impostorFrom) + " m";
                const float wd = std::max(MeasureTextEx(bold(), l1.c_str(), 30, 0).x, MeasureTextEx(regular(), l2.c_str(), 24, 0).x) + 40;
                dr::roundRect(c, at.x - wd / 2, at.y - 110, wd, 82, 14, withAlpha(SCRIM, 0.78f));
                dr::text(bold(), c, l1.c_str(), { at.x - wd / 2 + 20, at.y - 100 }, 30, 0, INK, 0, 0);
                dr::text(regular(), c, l2.c_str(), { at.x - wd / 2 + 20, at.y - 62 }, 24, 0, AMBER, 0, 0);
            }
        }
        const float tn = ramp(t, 30, 42) * (1 - ramp(t, 100, 114));
        if (tn > 0) {
            const Ctx c = faded(cx, tn);
            const std::string l = "meshes and reduced trees inside, pictures beyond";
            dr::roundRect(c, 64, 56, MeasureTextEx(regular(), l.c_str(), 28, 0).x + 48, 64, 14, withAlpha(SCRIM, 0.7f));
            dr::text(regular(), c, l.c_str(), { 88, 72 }, 28, 0, INK, 0, 0);
        }
        wipeLabels(cx, t, w, { "BEFORE: the 0.12 settings", "elevations 1, impostorShadows off" }, { "AFTER: the 0.13 defaults", "elevations 3, impostorShadows on" },
                   *before, *after, Stat::None);
    };
    caption(n, LEN[1], { "A valley from above", "crowns seen from above, and far trees that cast their shadows",
                         "TreeBuild::elevations   ForestSpec::impostorShadows   Scene::environmentFaces" }, w.rest() + 14);
}

void lowScene(TNode* n, const std::shared_ptr<std::shared_ptr<Valley>>& lazy) {
    const Wipe& w = WIPE[2];
    ShotP before, after;
    auto low = [&](float canopyBelow) {
        ValleyLook look{ 3, START[2] - START[1], 75, 80, false };
        return valleyShot(lazy, look, false, canopyBelow);
    };
    before = low(0);
    after = low(7);
    wipeNodes(n, LEN[2], w, after, before);
    n->add(0, LEN[2])->fn = [after, before, w](const Ctx& cx, int t) {
        haze(cx, START[2] - START[1], 75, t, 1);
        counter(cx, t, 12, w.in - 6, { { thousands(after->trees), "trees placed" } });
        wipeLabels(cx, t, w, { "BEFORE: trees only", "canopyBelow 0" }, { "AFTER: far canopy", "canopyBelow 7" }, *before, *after, Stat::Bodies);
        const float k = ramp(t, 162 - 8, 162 + 10) * (1 - ramp(t, w.out - 6, w.out + 8));
        if (k > 0 && before->bodies > 0) {
            char pc[16], tr[48];
            std::snprintf(pc, sizeof pc, "%.0f%%", 100.0 * (1.0 - (double)after->bodies / (double)before->bodies));
            // the other side of the trade, read from the same counters, in the headline: a patch is a dense height field
            std::snprintf(tr, sizeof tr, "%.1fx", before->triangles > 0 ? (double)after->triangles / (double)before->triangles : 0.0);
            pill(cx, k, "same picture,", pc, " fewer draws", "but", tr, " the triangles");
        }
    };
    caption(n, LEN[2], { "Low over the forest", "where trees are a few pixels tall, one height-field patch a cell",
                         "ForestSpec { canopyBelow = 7 }   (low cameras: canopyMaxElevation 20)" }, 30);
}

// ---------------------------------------------------------------- 4: the ground

constexpr float GROUND_X = 45, GROUND_Z = 50;
float meadowGround(float x, float z) { return 0.5f * flora::noise(x, z, 14, 9) + 0.2f * flora::noise(x, z, 5, 10); }

Pose groundPose(int t) {
    const float z = 18 - 11 * sm(t, 0, 240), x = 0.5f * std::sin(t * 0.012f);
    const float h = 0.85f - 0.3f * sm(t, 60, 240);
    const Vector3 pos{ x, meadowGround(x, z) + h, z };
    return { pos, Vector3Add(pos, { 1.5f, -0.5f, -10 }), h };
}

struct Tuft { float x, z; };
std::vector<Tuft> tufts() {                                              // beside the path, never on it
    std::vector<Tuft> out;
    for (int i = 0; i < 18; i++) {
        const float side = i % 2 ? 1.f : -1.f;
        out.push_back({ side * (1.5f + 2.4f * (float)rnd01(5, (unsigned long long)i)), 14 - 1.5f * i + 0.8f * (float)rnd01(6, (unsigned long long)i) });
    }
    return out;
}

ShotP groundShot(float density) {
    auto shot = std::make_shared<Shot>();
    shot->make = [density](Shot&) {
        auto s = std::make_shared<g3d::Scene>();
        skyFaces(*s);
        s->fogNear = 60;
        s->fogFar = 400;
        s->shadowMapSize = 4096;
        s->camera.fov = 58;
        s->camera.nearPlane = 0.05f;
        s->camera.farPlane = 800;
        g3d::Light sun = g3d::Light::directional(Vector3Scale(SUN, 100), SUNLIGHT, 2.7f, true);
        sun.shadowExtent = 15;
        s->lights.push_back(sun);
        s->lights[0].up = [](int t, g3d::Light& l) {
            const Pose p = groundPose(t);
            Vector3 focus = p.pos;
            focus.x += 1.5f;
            focus.y = meadowGround(focus.x, focus.z);
            focus.z -= 6;
            l.target = focus;
            l.position = Vector3Add(focus, Vector3Scale(SUN, 100));
        };
        flora::Area land = flora::rect({ -GROUND_X, -GROUND_Z }, { GROUND_X, GROUND_Z });
        land.ground = meadowGround;
        g3d::Material soil;
        soil.color = mkColor(0x3e4a2a);
        soil.roughness = 1;
        s->add(flora::terrain(land, 254, 254), soil);
        g3d::Field lawn = flora::meadow();
        lawn.height = 0.4f;
        lawn.density = 450;
        lawn.sway = 0.1f;
        lawn.gust = 0.8f;
        lawn.gustSpeed = 5;
        lawn.gustScale = 14;
        lawn.fps = FPS;
        lawn.shadowDensity = density;
        g3d::Object3D* meadow = flora::addGrass(*s, land, lawn, 4);
        (void)meadow;
        const g3d::Material blade = flora::grassMaterial();
        for (const Tuft& tf : tufts()) {                                  // tall tufts: dense discs of 1.4 m blades
            auto f = std::make_shared<g3d::Field>(flora::tallGrass());
            f->min = { tf.x - 0.5f, tf.z - 0.5f };
            f->max = { tf.x + 0.5f, tf.z + 0.5f };
            f->mask = [tf](float x, float z) { return (x - tf.x) * (x - tf.x) + (z - tf.z) * (z - tf.z) < 0.2f ? 1.f : 0.f; };
            f->ground = meadowGround;
            f->density = 2600;
            f->height = 1.4f;
            f->heightVariation = 0.2f;
            f->width = 0.02f;
            f->tileSize = 1;
            f->mapResolution = 64;
            f->sway = 0.1f;
            f->gust = 0.9f;
            f->gustSpeed = 5;
            f->gustScale = 14;
            f->windDirection = { 1, 0.3f };
            f->fps = FPS;
            f->shadowDensity = density;
            f->shadowDistance = 30;
            s->add(f, blade);
        }
        s->camera.up = [](int t, g3d::Camera& cam) {
            const Pose p = groundPose(t);
            // nothing stands within 1.2 m of the path: the camera is never inside a tuft
            float nearest = 1e9f;
            for (const Tuft& tf : tufts()) nearest = std::min(nearest, std::hypot(tf.x - p.pos.x, tf.z - p.pos.z));
            logClearance(4, t, std::min(p.clearance, nearest));
            cam.position = p.pos;
            cam.target = p.target;
        };
        return s;
    };
    return shot;
}

void groundScene(TNode* n) {
    const Wipe& w = WIPE[3];
    ShotP after = groundShot(0.35f), before = groundShot(1.f);
    wipeNodes(n, LEN[3], w, after, before);
    n->add(0, LEN[3])->fn = [after, before, w](const Ctx& cx, int t) {
        wipeLabels(cx, t, w, { "BEFORE: shadowDensity 1", "every blade is in the shadow map" }, { "AFTER: shadowDensity 0.35", "a random third of the blades, drawn wider" },
                   *before, *after, Stat::ShadowBlades);
        const float k = ramp(t, w.rest() + 6, w.rest() + 24) * (1 - ramp(t, w.out - 6, w.out + 8));
        if (k > 0 && before->shadowBlades > 0) {
            char pc[16];
            std::snprintf(pc, sizeof pc, "%.0f%%", 100.0 * (1.0 - (double)after->shadowBlades / (double)before->shadowBlades));
            pill(cx, k, "about the same shadows,", pc, " fewer blades in the shadow map", nullptr, nullptr, nullptr);
        }
    };
    caption(n, LEN[3], { "On the ground", "tall tufts in gusts under a low sun: their shadows sway with the blades", "g3d::Field { shadowDensity }" }, 12);
}

// ---------------------------------------------------------------- 5: the 2D coda

void flatScene(TNode* n) {
    TNode* hill = n->add(0, LEN[4]);
    hill->isolate().motionBlur = { 8, 0.8f };
    hill->fn = [](const Ctx& cx, int t) {
        const std::string pond = "M 570 840 C 570 765 840 765 840 840 C 840 915 570 915 570 840 Z";
        const std::string shape = "M 90 960 C 390 450 960 390 1350 630 C 1560 750 1770 810 1860 960 Z " + pond;
        dr::rectGrad4(cx, 0, 0, 1920, 1080, mkColor(0xe9eff2), mkColor(0xe9eff2), mkColor(0xcbd9de), mkColor(0xcbd9de));
        dr::rect(cx, 0, 960, 1920, 120, mkColor(0x4a6a34));
        const vg::Path path = vg::Path::parse(shape);
        vg::fill(cx, path, mkColor(0x5d7a3c), vg::FillRule::EvenOdd);
        vg::fill(cx, vg::Path::parse(pond), mkColor(0x6f9fbf));
        flora2d::Wind wind;
        wind.strength = 1.3f;
        wind.gust = 1;
        wind.gustSpeed = 420;
        wind.gustWidth = 300;
        wind.sway = 0.3f;
        flora2d::Grass grass = flora2d::tallGrass();
        grass.height = 96;
        grass.density = 0.3f;
        flora2d::meadow(cx, path, grass, t, ramp(t, 0, 75), 3, wind, 0.5f, {}, vg::FillRule::EvenOdd);
        // the path itself, drawn once: the shape the meadow was given
        const float k = ramp(t, 4, 18) * (1 - ramp(t, 60, 80));
        if (k > 0) vg::stroke(cx, path, vg::Stroke(withAlpha(mkColor(0x23301f), k), 4));
    };
    TNode* text = n->add(0, LEN[4]);
    text->fn = [](const Ctx& cx, int t) {
        const float k = ramp(t, 6, 20) * (1 - ramp(t, 100, 118));
        const Ctx c = faded(cx, k);
        dr::text(bold(), c, "The same meadow, in 2D", { 96, 70 + 12 * (1 - k) }, 64, 0, mkColor(0x23301f), 0, 0);
        dr::text(regular(), c, "a curved hill with a pond cut out; gusts, with motion blur on the tips", { 98, 150 }, 28, 0, mkColor(0x3a4a34), 0, 0);
        dr::text(regular(), c, "flora2d::meadow(c, path, grass, t, u, seed, wind, perspective, {}, vg::FillRule::EvenOdd)", { 98, 196 }, 26, 0, mkColor(0x7a4a00), 0, 0);
    };
}

// ---------------------------------------------------------------- 6: the end card

struct Api { const char* call; const char* what; };
const Api API[] = {
    { "TreeBuild::elevations", "impostors seen from above" },
    { "ForestSpec::impostorShadows", "far trees cast their shadows" },
    { "ForestSpec::canopyBelow", "a far canopy for low cameras" },
    { "ForestCover::impostorBelow", "map forests by size on screen" },
    { "Field::shadowDensity", "fewer blades in the shadow map" },
    { "Scene::environmentFaces", "a skybox from six faces" },
    { "flora2d::meadow(path)", "2D meadows on a curve" },
};

void endScene(TNode* n) {
    n->add(0, LEN[5])->fn = [](const Ctx& cx, int t) {
        dr::rect(cx, 0, 0, 1920, 1080, mkColor(0x101418));
        const float k = ramp(t, 6, 24);
        const Ctx c = faded(cx, k);
        const float w = MeasureTextEx(bold(), "ONE FOREST. EVERY DISTANCE.", 96, 0).x;
        dr::text(bold(), c, "ONE FOREST. EVERY DISTANCE.", { 960 - w / 2, 150 + 14 * (1 - k) }, 96, 0, INK, 0, 0);
        for (int i = 0; i < 7; i++) {
            const float a = ramp(t, 36 + i * 6, 52 + i * 6);
            if (a <= 0) continue;
            const Ctx l = faded(cx, a);
            const float y = 340 + i * 74 + 10 * (1 - a);
            dr::text(code::font(false), l, API[i].call, { 330, y }, 36, 0, AMBER, 0, 0);
            dr::text(regular(), l, API[i].what, { 1010, y + 4 }, 32, 0, SUB, 0, 0);
        }
    };
    TNode* logo = prefab::use(n, 90, LEN[5] - 90, "raymotion-logo",
                              Json::parse(R"({"size": 56, "layout": "horizontal", "tagline": "video as code", "delay": 2, "ink": "#f1ede2", "accent": "#2fc4b2"})"));
    logo->props.pos = { 960 - 150, 930 };
    n->add(98, LEN[5] - 98)->fn = [](const Ctx& cx, int t) {
        dr::text(code::font(false), faded(cx, ramp(t, 0, 12)), "github.com/pebaryan/raymotion", { 960 - 30, 930 }, 26, 0, SUB, 0, 0.5f);
    };
}

// ---------------------------------------------------------------- the score

const std::string SCORE = "out/far-and-near-assets/score.wav";

// 72 bpm, G major, a bar per chord (G, Em, C, D). The pad thins as the camera comes down; wind rises
// into the ground chapter; a soft pulse where each wipe lands, a chime on the percentage, light
// plucks in the 2D coda, bells on the title. Scene starts and wipe frames are the picture's own tables.
void genScore(const std::string& path) {
    using procgen::Score;
    const double end = TOTAL / (double)FPS, bar = 4 * 60.0 / 72, stop = end - 2.6;
    auto at = [](int scene, int local) { return (START[scene] + local) / (double)FPS; };
    Score s(end + 0.5);
    struct Chord { int root; int pad[3]; };
    const Chord prog[4] = { { 43, { 62, 67, 71 } }, { 40, { 64, 67, 71 } }, { 36, { 60, 64, 67 } }, { 38, { 62, 66, 69 } } };
    int i = 0;
    for (double t = 0; t < stop; t += bar, i++) {
        const Chord& c = prog[i % 4];
        const double thin = t < at(1, 0) ? 1.0 : (t < at(3, 0) ? 0.7 : 0.4);        // the pad thins on the way down
        for (int q : c.pad) s.pad(t, bar * 0.98, q, (float)(0.06 * thin), t < at(1, 0) ? 1100.f : 800.f, 0.5f);
        s.pad(t, bar * 0.98, c.pad[0] - 12, (float)(0.04 * thin), 500, 0.5f);
        s.bass(t, bar * 0.9, c.root, (float)(0.11 * thin));
        if (t >= at(4, 0) && t < at(5, 0))                                           // the 2D hill: light plucks
            for (int e = 0; e < 4; e++)
                if (rnd01(7, (unsigned long long)(i * 4 + e)) < 0.7) s.pluck(t + e * bar / 4, c.pad[e % 3] + 12, 0.03f, e % 2 ? 0.3f : 0.7f, 6);
    }
    {   // wind: band-passed noise that swells with gusts, strongest on the ground
        procgen::Svf f;
        const int n = (int)(end * Score::RATE);
        for (int k = 0; k < n; k++) {
            const double t = (double)k / Score::RATE;
            const float open = t < at(1, 0) ? 0.35f : (t < at(3, 0) ? 0.6f : (t < at(4, 0) ? 1.f : 0.4f));
            const float gust = 0.55f + 0.45f * (float)std::sin(t * 1.1 + 0.8 * std::sin(t * 0.37));
            const float fade = (float)std::min({ 1.0, t / 1.5, (end - t) / 2.0 });
            const float x = f.bp(s.noise(), 380 + 520 * gust, 0.6f, Score::RATE);
            s.put(k, x * 0.07f * gust * open * std::max(0.f, fade), 0.5f + 0.2f * (float)std::sin(t * 0.5), 0.3f, 0);
        }
    }
    s.riser(at(0, 0) + 0.1, at(0, 0) + 3.0, 0.03f);                                   // the descent begins
    for (int k = 0; k < 4; k++) {                                                     // a soft pulse where each wipe lands
        const double t = at(k, WIPE[k].rest());
        s.pluck(t, 74, 0.05f, 0.5f, 4);
        s.bass(t, 0.9, 43, 0.09f);
    }
    s.bell(at(2, 162), 91, 0.035f, 0.4f);                                             // the percentage
    s.bell(at(2, 162) + 0.1, 98, 0.022f, 0.6f);
    s.riser(at(3, 0) - 1.0, at(3, 0) + 0.6, 0.03f);                                   // down to the grass
    for (int e = 0; e < 4; e++) s.pluck(at(5, 6) + e * 0.22, prog[0].pad[e % 3] + 12, 0.03f, 0.5f, 5);
    s.bell(at(5, 6), 86, 0.05f, 0.5f);
    s.bell(at(5, 6) + 0.18, 91, 0.04f, 0.6f);
    for (int q : prog[0].pad) s.pad(stop, 2.6, q, 0.08f, 900, 0.5f);
    s.bass(stop, 2.6, prog[0].root - 12, 0.13f);
    std::vector<float> mixed = s.mix();
    for (float& v : mixed) v *= 0.8f;
    media::writeWav(path, mixed, Score::RATE);
}

}  // namespace

void registerFarAndNear(Registry& out) {
    Comp c;
    c.name = "far-and-near";
    c.w = 1920; c.h = 1080; c.fps = FPS;
    c.background = mkColor(0x070a10);
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    auto valley = std::make_shared<std::shared_ptr<Valley>>();      // one forest for chapters 2 and 3, made at the first draw

    TNode* globe = ts.add(LEN[0]);
    globeScene(globe);
    ts.transition(Transition::fade(XF));
    TNode* v = ts.add(LEN[1]);
    valleyScene(v, valley);
    ts.transition(Transition::fade(XF));
    TNode* low = ts.add(LEN[2]);
    lowScene(low, valley);
    ts.transition(Transition::fade(XF));
    TNode* ground = ts.add(LEN[3]);
    groundScene(ground);
    ts.transition(Transition::fade(XF));
    TNode* flat = ts.add(LEN[4]);
    flatScene(flat);
    ts.transition(Transition::fade(XF));
    TNode* card = ts.add(LEN[5]);
    endScene(card);

    c.duration = ts.end();
    if (c.duration != TOTAL || globe->start != START[0] || v->start != START[1] || low->start != START[2] || ground->start != START[3] ||
        flat->start != START[4] || card->start != START[5])
        throw std::logic_error("far-and-near: the scene table and the series disagree");
    c.assets = [](const Comp&, const Json&) {
        if (std::filesystem::exists(SCORE)) return;
        std::filesystem::create_directories(std::filesystem::path(SCORE).parent_path());
        genScore(SCORE);
    };
    media::audio(addRoot(c.roots, 0, c.duration), SCORE);
    out.push_back(std::move(c));
}

}  // namespace rmtn
