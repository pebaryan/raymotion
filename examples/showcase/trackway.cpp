// trackway.cpp -- "the-trackway": a 70 s promo for the rig's animals and mounts.
// Every animal crosses one strip of sand and leaves its own trail: each print is pressed
// where a foot of the solved pose touched down, and never moves. A footfall chart and the
// footfall sounds come from the same contacts. Trails end where a body leaves the ground --
// into the air, the water, onto the bars, or onto another body's back. Text-only: the
// captions are the script.
//
//   raymotion render the-trackway
//   raymotion still  the-trackway --at 470 --png out/trackway-470.png
//
// See docs/the-trackway-brief.md and docs/the-trackway-qc.md.
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include <filesystem>
#include <string>
#include <vector>

namespace rmtn {
namespace {

constexpr float W = 1920.f, H = 1080.f;
constexpr int FPS = 30, SCENES = 8;
// One continuous world: the chapters follow each other with no crossfade; the camera
// trucks along the strip from one to the next (brief §3).
constexpr int DUR[SCENES] = { 180, 360, 270, 180, 210, 360, 360, 180 };
constexpr int total() {
    int t = 0;
    for (int d : DUR) t += d;
    return t;
}
constexpr int TOTAL = total();
std::array<int, SCENES> sceneStarts() {
    std::array<int, SCENES> s{};
    int end = 0;
    for (int i = 0; i < SCENES; i++) { s[(size_t)i] = end; end += DUR[i]; }
    return s;
}
double chapterAt(int i) {                      // a chapter's start, film seconds
    int t = 0;
    for (int k = 0; k < i; k++) t += DUR[k];
    return t / (double)FPS;
}

const std::string ASSETS = "out/the-trackway-assets";
const std::string SRT = "out/the-trackway.srt";

// Palette (brief §6): sand and its prints; teal = the trace of motion; coral = where a trail ends.
const Color PAPER = mkColor(0xf3efe6), INK = mkColor(0x1f1d2b), DIM = mkColor(0x8a8597),
            TEAL = mkColor(0x139e98), CORAL = mkColor(0xe8553f), CARD = mkColor(0xfbf9f4),
            RULE = mkColor(0xd9d3c6), SAND = mkColor(0xe6dcc6), PRESSED = mkColor(0xb59d72),
            RIM = mkColor(0xf4eee0), BANK = mkColor(0xd3c4a6), WATER = mkColor(0x6fa9b4),
            WOOD = mkColor(0x8a6a4a);

const char* const KICKERS[SCENES] = { "THE GROUND REMEMBERS", "FOUR FEET", "TWO FEET", "ANY SIZE", "NO FEET",
                                      "WHERE TRAILS END", "ONE TRAIL", "" };

// The script (brief §4): chapter, window in its local seconds, text.
struct Caption { int scene; double from, to; const char* text; };
const Caption CAPTIONS[] = {
    { 0, 0.27, 5.6, "The ground remembers every step." },
    { 1, 0.27, 7.6, "Walk, trot, gallop: the same four feet, new orders." },
    { 1, 7.9, 11.6, "At the gallop, for a moment, nothing touches." },
    { 2, 0.27, 4.5, "A hen holds her head still, then catches up." },
    { 2, 4.7, 8.6, "A duck doesn't bob. It waddles." },
    { 3, 0.27, 5.6, "The same three toes, nine times the length." },
    { 4, 0.27, 6.6, "A snake leaves one groove. Every part of it follows." },
    { 5, 0.27, 11.6, "Some trails stop: in the air, in the water, on the bars." },
    { 6, 0.27, 4.8, "Two bodies. One trail." },
    { 6, 5.0, 11.6, "A rider, a passenger, a pilot." },
};

// ------------------------------------------------------------------ timing tables (brief §7b)
// Film seconds. Chapter 2's gait changes, blended over GAIT_FADE.
const double C1 = chapterAt(1), C2 = chapterAt(2), C3 = chapterAt(3), C4 = chapterAt(4), C5 = chapterAt(5),
             C6 = chapterAt(6), C7 = chapterAt(7);
const double TROT_AT = C1 + 2.9, GALLOP_AT = C1 + 5.5;
constexpr double GAIT_FADE = 0.4;
constexpr double REVEAL_FROM = 2.7, REVEAL_TO = 3.4;     // chapter 1: the horse appears over its prints
// Chapter 6's endings.
const double TAKEOFF_AT = C5 + 1.2, SHORE_AT = C5 + 3.8, BARS_AT = C5 + 7.0;
// Chapter 7's mounts.
const double SADDLE_AT = C6 + 2.0, BACK_AT = C6 + 5.8, PALM_AT = C6 + 8.6;

// ------------------------------------------------------------------ the strip
constexpr float STRIP_W = 3.2f;                 // z from -1.6 to 1.6
constexpr float TILE = 4.f;                     // metres of sand per texture
constexpr int PPM = 256;                        // texture pixels per metre
constexpr float POND_LEN = TILE;              // the pond replaces one tile of sand
constexpr float STRIP_X0 = -10;
constexpr float K = 2.6f;                       // the titan: a robot this many times life size

// ------------------------------------------------------------------ small helpers
float easeIn(double t, double at, double dur = 12) {
    return (float)interpolate(t - at, { 0.0, dur }, { 0.0, 1.0 }, clamped(ease::out(ease::cubic)));
}
float smooth(double u) { u = std::clamp(u, 0.0, 1.0); return (float)(u * u * (3 - 2 * u)); }
void text(const Ctx& cx, const std::string& s, Vector2 at, float size, Color col, float ax = 0, float ay = 0.5f,
          bool bold = false, float spacing = 0) {
    dr::text(fonts::ui(bold), cx, s.c_str(), at, size, spacing, col, ax, ay);
}
Ctx faded(const Ctx& cx, float a) { Ctx m = cx; m.a *= std::clamp(a, 0.f, 1.f); return m; }
Color mixc(Color a, Color b, float u) {
    u = std::clamp(u, 0.f, 1.f);
    return { (unsigned char)(a.r + (b.r - a.r) * u), (unsigned char)(a.g + (b.g - a.g) * u),
             (unsigned char)(a.b + (b.b - a.b) * u), (unsigned char)(a.a + (b.a - a.a) * u) };
}

// Distance along a straight line from speeds given at knots (film seconds, m/s), linear
// between knots and constant outside: the integral, exactly.
struct Track {
    float x0 = 0;
    std::vector<std::pair<double, double>> v;
    double at(double s) const {
        if (v.empty()) return x0;
        double x = x0;
        if (s <= v[0].first) return x0 + v[0].second * (s - v[0].first);
        for (size_t i = 0; i + 1 < v.size(); i++) {
            const auto [a, va] = v[i];
            const auto [b, vb] = v[i + 1];
            if (s <= b) {
                const double u = s - a, k = b > a ? (vb - va) / (b - a) : 0;
                return x + va * u + 0.5 * k * u * u;
            }
            x += 0.5 * (va + vb) * (b - a);
        }
        return x + v.back().second * (s - v.back().first);
    }
};

// ------------------------------------------------------------------ actors and their marks
enum class Mark { Hoof, Paw, Hen, Duck, Rex, Shoe, ApeFoot, Knuckle, Block };
struct Foot { rig::Joint j; Mark mark; float size; float fwd; };   // fwd: the print's centre ahead of the joint
struct Contact {
    double t = 0, until = 0;                    // touchdown and lift-off, film seconds
    Vector2 at{};                               // world x, z
    float heading = 0;                          // radians in the x-z plane
    Mark mark = Mark::Hoof;
    float size = 0.1f;
    int actor = 0, foot = 0;
    bool last = false;                          // the last print of a trail that ends
    bool ghost = false;                         // an outline laid in for comparison, not a footfall
};

struct Actor {
    std::string name;
    rig::CharacterPtr ch;
    std::function<Vector3(double)> pos;         // the node's position, film seconds
    float yaw = 90;                             // 90: facing +x, along the strip
    double show0 = 0, show1 = 0;                // drawn in this window
    double ground0 = -1e9, ground1 = 1e9;       // leaves prints in this window
    std::vector<Foot> feet;
    double endsAt = -1;                         // the trail ends here (its last prints turn coral)
    float eps = 0.012f, vmax = 0.25f;           // contact: within eps of the lowest, slower than vmax
};

Matrix nodeMatrix(const Actor& a, double s) {
    const Vector3 p = a.pos(s);
    return MatrixMultiply(MatrixRotateY(a.yaw * DEG2RAD), MatrixTranslate(p.x, p.y, p.z));
}
Vector3 worldJoint(const Actor& a, double s, rig::Joint j) {
    const rig::Build& b = a.ch->lookAt(s).build;
    return Vector3Transform(rig::position(rig::solve(b, a.ch->poseAt(s)), j), nodeMatrix(a, s));
}

// The footfalls of one actor: a foot is down while it is within eps of the lowest it gets
// and nearly still; each touchdown leaves one print.
void detect(const Actor& a, int index, std::vector<Contact>& out) {
    const double dt = 1.0 / 60, s0 = std::max(a.show0, a.ground0), s1 = std::min(a.show1, a.ground1);
    if (s1 <= s0) return;
    const float hx = std::sin(a.yaw * DEG2RAD), hz = std::cos(a.yaw * DEG2RAD);
    for (size_t fi = 0; fi < a.feet.size(); fi++) {
        const Foot& f = a.feet[fi];
        std::vector<Vector3> w;
        std::vector<double> ts;
        for (double s = s0; s <= s1; s += dt) { ts.push_back(s); w.push_back(worldJoint(a, s, f.j)); }
        // The planted height: a low percentile of the foot's height, not its minimum (blends
        // between gaits dip lower for a moment).
        std::vector<float> ys;
        for (const Vector3& p : w) ys.push_back(p.y);
        std::nth_element(ys.begin(), ys.begin() + (long)(ys.size() * 0.15), ys.end());
        const float ymin = ys[(size_t)(ys.size() * 0.15)];
        const size_t first = out.size();
        size_t start = 0;
        bool in = false;
        for (size_t i = 0; i <= w.size(); i++) {
            const bool down = i < w.size() && w[i].y < ymin + a.eps &&
                              (i == 0 || std::hypot(w[i].x - w[i - 1].x, w[i].z - w[i - 1].z) / dt < a.vmax);
            if (down && !in) { start = i; in = true; }
            if (!down && in) {
                in = false;
                if (i - start < 3) continue;
                const Vector3 p = w[std::min(start + 3, i - 1)];
                Contact c;
                c.t = ts[start];
                c.until = ts[i - 1];
                c.at = { p.x + hx * f.fwd, p.z + hz * f.fwd };
                c.heading = std::atan2(hz, hx);
                c.mark = f.mark;
                c.size = f.size;
                c.actor = index;
                c.foot = (int)fi;
                out.push_back(c);
            }
        }
        if (a.endsAt > 0 && out.size() > first) out.back().last = true;
    }
}

// ------------------------------------------------------------------ the world
struct Bar { float x; };
struct Groove { std::vector<std::pair<double, Vector2>> pts; float width = 0.05f; double highlight = 1e9; };
struct Trace { std::vector<std::pair<double, float>> pts; };   // head x over time
struct Cam { Vector3 pos, tgt; float fov = 34; float shadow = 4; };
struct Shot { double at, blend; std::function<Cam(double)> cam; };

struct World {
    std::vector<Actor> actors;
    std::vector<Contact> prints;               // sorted by touchdown
    Groove groove;
    Trace henHead, duckHead;
    double henHead0 = 0, duckHead0 = 0;
    std::vector<Shot> shots;
    float x0 = -8, x1 = 0;                      // the strip's extent
    float pondX = 0;                            // the pond's near edge
    std::vector<Bar> bars;
    float barHeight = 2.4f;
    double gap0 = 0, gap1 = 0;                  // the gallop's first moment with no hoof down
    Contact rexPrint, henGhost;                 // chapter 4's focus print and the hen's inside it
    std::vector<int> footfallActor;             // index of the chart's horse
    int horse = 0, hen = 0, duck = 0, rex = 0, snake = 0, hen2 = 0, turtle = 0, chimp = 0;
    int horse2 = 0, rider = 0, walker = 0, chimp2 = 0, titan = 0, pilot = 0;
    Cam camAt(double s) const;
};

Cam lerpCam(const Cam& a, const Cam& b, float u) {
    return { Vector3Lerp(a.pos, b.pos, u), Vector3Lerp(a.tgt, b.tgt, u), a.fov + (b.fov - a.fov) * u,
             a.shadow + (b.shadow - a.shadow) * u };
}
Cam World::camAt(double s) const {
    size_t i = 0;
    while (i + 1 < shots.size() && shots[i + 1].at <= s) i++;
    Cam c = shots[i].cam(s);
    if (i > 0 && s - shots[i].at < shots[i].blend)
        c = lerpCam(shots[i - 1].cam(s), c, (float)ease::inOut(ease::cubic)(std::clamp((s - shots[i].at) / shots[i].blend, 0.0, 1.0)));
    return c;
}

// A camera locked to a moving point: target = point + dt, position = target + dp.
std::function<Cam(double)> follow(std::function<Vector3(double)> at, Vector3 dt, Vector3 dp, float fov, float shadow) {
    return [at, dt, dp, fov, shadow](double s) {
        const Vector3 t = Vector3Add(at(s), dt);
        return Cam{ Vector3Add(t, dp), t, fov, shadow };
    };
}

std::vector<Foot> quadFeet(const rig::Build& b, Mark m, float size) {
    std::vector<Foot> f;
    for (const char* n : { "paw-hind-L", "paw-front-L", "paw-front-R", "paw-hind-R" }) f.push_back({ rig::joint(b, n), m, size, 0 });
    return f;
}
std::vector<Foot> birdFeet(const rig::Build& b, Mark m, float toe) {
    return { { rig::joint(b, "foot-L"), m, toe, 0 }, { rig::joint(b, "foot-R"), m, toe, 0 } };
}
std::vector<Foot> humanFeet(const rig::Build& b, Mark m = Mark::Shoe) {
    return { { rig::Joint::AnkleL, m, b.foot, b.foot * 0.28f }, { rig::Joint::AnkleR, m, b.foot, b.foot * 0.28f } };
}

// An in-place clip moved within its node: offset by `from` + forward (z) at `speed` for `dur`.
rig::Clip carried(rig::Clip c, Vector3 from, float speed, double dur) {
    return [c, from, speed, dur](double s) {
        rig::Pose p = c(s);
        p.offset = Vector3Add(p.offset, { from.x, from.y, from.z + speed * (float)std::min(s, dur) });
        return p;
    };
}

// Bending down to the ground by `k` (0..1): hips down and back, the body leaning in, the
// feet kept where they stood.
rig::Pose squat(const rig::Build& b, rig::Pose p, float k) {
    const rig::Frames f = rig::solve(b, p);
    const Vector3 l = rig::position(f, rig::Joint::AnkleL), r = rig::position(f, rig::Joint::AnkleR);
    p.offset.y -= k * 0.46f * (b.thigh + b.shin);
    p.offset.z -= k * 0.3f * b.thigh;
    p[rig::Joint::Pelvis].x += k * 40;
    p[rig::Joint::Chest].x += k * 45;
    p[rig::Joint::Head].x -= k * 50;
    p = rig::plant(b, p, rig::Side::Left, l);
    return rig::plant(b, p, rig::Side::Right, r);
}

rig::Look person(Color top, Color bottom, const char* hair) {
    rig::Look l = rig::Look::toon();
    l.build = rig::Build::adult();
    l.build.head = 0.34f;
    l.hair = hair;
    l.outfit = rig::Outfit::basic(top, bottom, l.skin, 0.45f, 1);
    l.feet("foot.boot");
    return l;
}

std::shared_ptr<World> makeWorld() {
    auto w = std::make_shared<World>();
    auto add = [&](Actor a) { w->actors.push_back(std::move(a)); return (int)w->actors.size() - 1; };

    // ---- chapters 1-2: the horse, walk / trot / gallop.
    {
        Actor a;
        a.name = "horse";
        a.ch = std::make_shared<rig::Character>(rig::animal("horse"));
        const rig::Build b = a.ch->looks()[0].build;
        a.ch->motion.play(rig::clips::gait(b, rig::Gait::Walk), 0)
            .play(rig::clips::gait(b, rig::Gait::Trot), TROT_AT, GAIT_FADE)
            .play(rig::clips::gait(b, rig::Gait::Gallop), GALLOP_AT, GAIT_FADE);
        const double vw = rig::gaitSpeed(b, rig::Gait::Walk), vt = rig::gaitSpeed(b, rig::Gait::Trot),
                     vg = rig::gaitSpeed(b, rig::Gait::Gallop);
        Track tr;
        tr.x0 = 0;
        tr.v = { { 0, vw }, { TROT_AT, vw }, { TROT_AT + GAIT_FADE, vt }, { GALLOP_AT, vt }, { GALLOP_AT + GAIT_FADE, vg } };
        a.pos = [tr](double s) { return Vector3{ (float)tr.at(s), 0, -0.1f }; };
        a.show0 = 0;
        a.show1 = C2 + 0.5;
        a.feet = quadFeet(b, Mark::Hoof, 0.13f);
        a.eps = 0.015f;
        a.vmax = 0.4f;
        w->horse = add(a);
    }
    const Actor& horse = w->actors[(size_t)w->horse];
    float cursor = horse.pos(C2).x + 16;

    // ---- chapter 3: a hen, then a duck on the far lane.
    {
        Actor a;
        a.name = "hen";
        a.ch = std::make_shared<rig::Character>(rig::animal("chicken"));
        const rig::Build b = a.ch->looks()[0].build;
        a.ch->motion.play(rig::clips::gait(b, rig::Gait::Walk), C2 - 0.6);
        const double v = rig::gaitSpeed(b, rig::Gait::Walk);
        Track tr;
        tr.x0 = cursor;
        tr.v = { { C2, v } };
        a.pos = [tr](double s) { return Vector3{ (float)tr.at(s), 0, 0.25f }; };
        a.show0 = C2 - 0.6;
        a.show1 = C3 + 1.0;
        a.feet = birdFeet(b, Mark::Hen, b.plan->dim("toe"));
        a.eps = 0.004f;
        a.vmax = 0.08f;
        w->hen = add(a);
    }
    {
        const Actor& hen = w->actors[(size_t)w->hen];
        Actor a;
        a.name = "duck";
        a.ch = std::make_shared<rig::Character>(rig::animal("duck"));
        const rig::Build b = a.ch->looks()[0].build;
        const double enter = C2 + 4.4;
        a.ch->motion.play(rig::clips::gait(b, rig::Gait::Walk), enter - 0.6);
        const double v = rig::gaitSpeed(b, rig::Gait::Walk);
        Track tr;
        tr.x0 = hen.pos(C2 + 5.6).x - 0.5f - (float)(v * (C2 + 5.6 - enter));   // level with the hen by 5.6 s
        tr.v = { { enter, v } };
        a.pos = [tr](double s) { return Vector3{ (float)tr.at(s), 0, -0.3f }; };
        a.show0 = enter - 0.6;
        a.show1 = C3 + 1.0;
        a.feet = birdFeet(b, Mark::Duck, b.plan->dim("toe"));
        a.eps = 0.004f;
        a.vmax = 0.08f;
        w->duck = add(a);
    }
    cursor = w->actors[(size_t)w->hen].pos(C3).x + 9;

    // ---- chapter 4: a T. rex takes two steps and roars.
    {
        Actor a;
        a.name = "trex";
        a.ch = std::make_shared<rig::Character>(rig::animal("trex"));
        const rig::Build b = a.ch->looks()[0].build;
        const double stop = C3 + 3.0;
        a.ch->motion.play(rig::clips::gait(b, rig::Gait::Walk), C3 - 1.5).play(rig::clips::stand(b), stop, 0.6);
        const double v = rig::gaitSpeed(b, rig::Gait::Walk);
        Track tr;
        tr.x0 = cursor;
        tr.v = { { C3, v }, { stop, v }, { stop + 0.6, 0 } };
        a.pos = [tr](double s) { return Vector3{ (float)tr.at(s), 0, 0 }; };
        a.show0 = C3 - 1.5;
        a.show1 = C4 + 1.0;
        a.feet = birdFeet(b, Mark::Rex, b.plan->dim("toe"));
        a.eps = 0.03f;
        a.vmax = 0.4f;
        w->rex = add(a);
    }
    cursor = w->actors[(size_t)w->rex].pos(C4).x + 10;

    // ---- chapter 5: a snake.
    {
        Actor a;
        a.name = "snake";
        a.ch = std::make_shared<rig::Character>(rig::animal("snake"));
        const rig::Build b = a.ch->looks()[0].build;
        a.ch->motion.play(rig::clips::gait(b, rig::Gait::Walk), C4 - 2.0);
        const double v = rig::gaitSpeed(b, rig::Gait::Walk);
        Track tr;
        tr.x0 = cursor;
        tr.v = { { C4, v } };
        a.pos = [tr](double s) { return Vector3{ (float)tr.at(s), 0, 0 }; };
        a.show0 = C4 - 2.0;
        a.show1 = C5 + 1.0;
        w->snake = add(a);
    }
    cursor = w->actors[(size_t)w->snake].pos(C5).x + 8;

    // ---- chapter 6: three endings -- a hen takes off, a turtle swims away, a chimp swings off.
    {
        Actor a;
        a.name = "hen2";
        a.ch = std::make_shared<rig::Character>(rig::animal("chicken"));
        const rig::Build b = a.ch->looks()[0].build;
        a.ch->motion.play(rig::clips::gait(b, rig::Gait::Trot), C5 - 1.0).play(rig::clips::fly(b), TAKEOFF_AT, 0.2);
        const double v = rig::gaitSpeed(b, rig::Gait::Trot);
        Track tr;
        tr.x0 = cursor;
        tr.v = { { C5, v }, { TAKEOFF_AT, v }, { TAKEOFF_AT + 1.2, 2.2 } };
        const double up = TAKEOFF_AT;
        a.pos = [tr, up](double s) {
            const double u = std::max(0.0, s - up);
            return Vector3{ (float)tr.at(s), (float)(0.9 * u * u + 0.25 * u), 0.2f };
        };
        a.show0 = C5 - 1.0;
        a.show1 = C5 + 3.6;
        a.ground1 = TAKEOFF_AT + 0.1;
        a.endsAt = TAKEOFF_AT;
        a.feet = birdFeet(b, Mark::Hen, b.plan->dim("toe"));
        a.eps = 0.004f;
        a.vmax = 0.1f;
        w->hen2 = add(a);
    }
    // The pond starts on a tile boundary, a little past where the hen took off.
    w->pondX = STRIP_X0 + TILE * std::ceil((w->actors[(size_t)w->hen2].pos(SHORE_AT).x + 2.0f - STRIP_X0) / TILE);
    {
        Actor a;
        a.name = "turtle";
        a.ch = std::make_shared<rig::Character>(rig::animal("turtle"));
        const rig::Build b = a.ch->looks()[0].build;
        const double v = rig::gaitSpeed(b, rig::Gait::Walk);
        const double swim = SHORE_AT + 0.4;
        a.ch->motion.play(rig::clips::gait(b, rig::Gait::Walk), C5).play(rig::clips::paddle(b), swim, 0.5);
        Track tr;
        tr.x0 = w->pondX - 0.05f - (float)(v * (SHORE_AT - C5));
        tr.v = { { C5, v }, { swim, v }, { swim + 0.6, 0.35 } };
        a.pos = [tr, swim](double s) { return Vector3{ (float)tr.at(s), -0.11f * smooth((s - swim + 0.2) / 0.7), 0.1f }; };
        a.show0 = C5 + 1.4;
        a.show1 = C5 + 8.0;
        a.ground1 = SHORE_AT;
        a.endsAt = SHORE_AT;
        a.feet = quadFeet(b, Mark::Paw, 0.05f);
        a.eps = 0.006f;
        a.vmax = 0.08f;
        w->turtle = add(a);
    }
    {
        Actor a;
        a.name = "fish";
        a.ch = std::make_shared<rig::Character>(rig::animal("fish"));
        const rig::Build b = a.ch->looks()[0].build;
        a.ch->motion.play(rig::clips::gait(b, rig::Gait::Walk), C5);
        const double v = rig::gaitSpeed(b, rig::Gait::Walk);
        const float px = w->pondX;
        a.pos = [px, v](double s) { return Vector3{ px + 0.6f + (float)(v * (s - C5 - 2.0)), -0.28f, -0.6f }; };
        a.show0 = C5 + 1.4;
        a.show1 = C5 + 8.0;
        add(a);
    }
    {
        Actor a;
        a.name = "chimp";
        const rig::Look l = rig::animal("chimp");
        a.ch = std::make_shared<rig::Character>(l);
        const rig::Build b = l.build;
        const float spacing = 0.8f, rate = rig::brachiateRate(b);
        w->barHeight = 2.4f;
        a.ch->motion.play(rig::clips::knuckleWalk(b), C5 + 2.0).play(rig::clips::brachiate(b, w->barHeight, spacing), BARS_AT, 0.45);
        const double vk = rig::gaitSpeed(b, rig::Gait::Walk), vb = 2 * spacing * rate;
        Track tr;
        const float barsX = w->pondX + POND_LEN + 4.5f;
        tr.x0 = barsX - (float)(vk * (BARS_AT - (C5 + 2.0)));
        tr.v = { { C5 + 2.0, vk }, { BARS_AT, vk }, { BARS_AT + 0.45, vb } };
        a.pos = [tr](double s) { return Vector3{ (float)tr.at(s), 0, 0 }; };
        // The bars sit where the hands grip: spacing apart from half a spacing past the
        // node's position where brachiation starts (see clips::brachiate). The clip slides
        // its bars back at the swinging speed from its first frame, but the node only reaches
        // that speed after the ramp: the hands land short of a moving-from-the-start node by
        // the ground lost in the ramp.
        const double settled = BARS_AT + 1.0;
        const float lag = (float)(tr.at(BARS_AT) + vb * (settled - BARS_AT) - tr.at(settled));
        const float start = (float)tr.at(BARS_AT) - lag;
        for (int m = 0; m < 12; m++) w->bars.push_back({ start + spacing * 0.5f + m * spacing });
        a.show0 = C5 + 2.0;
        a.show1 = C6 + 0.5;
        a.ground1 = BARS_AT + 0.1;
        a.endsAt = BARS_AT;
        a.feet = humanFeet(b, Mark::ApeFoot);
        a.feet.push_back({ rig::Joint::WristL, Mark::Knuckle, 0.07f, 0.02f });
        a.feet.push_back({ rig::Joint::WristR, Mark::Knuckle, 0.07f, 0.02f });
        a.eps = 0.015f;
        a.vmax = 0.12f;
        w->chimp = add(a);
    }
    cursor = w->bars.back().x + 6;

    // ---- chapter 7: two bodies, one trail.
    // A rider walks up to a standing horse and mounts; the horse walks on.
    {
        Actor h;
        h.name = "horse2";
        h.ch = std::make_shared<rig::Character>(rig::animal("horse"));
        const rig::Build hb = h.ch->looks()[0].build;
        const double go = SADDLE_AT + 1.0;
        h.ch->motion.play(rig::clips::stand(hb), C6 - 1.0).play(rig::clips::gait(hb, rig::Gait::Walk), go, 0.5);
        const double vw = rig::gaitSpeed(hb, rig::Gait::Walk);
        Track tr;
        tr.x0 = cursor + 3.5f;
        tr.v = { { go, 0 }, { go + 0.5, vw } };
        h.pos = [tr](double s) { return Vector3{ (float)tr.at(s), 0, -0.1f }; };
        h.show0 = C6 - 1.0;
        h.show1 = C6 + 6.5;
        h.feet = quadFeet(hb, Mark::Hoof, 0.13f);
        h.eps = 0.015f;
        h.vmax = 0.4f;
        w->horse2 = add(h);

        Actor r;
        r.name = "rider";
        const rig::Look rl = person({ 180, 40, 50, 255 }, { 236, 230, 214, 255 }, "hair.short");
        r.ch = std::make_shared<rig::Character>(rl);
        const float cad = 0.9f, vr = rig::walkSpeed(rl.build, cad);
        const Vector3 from{ -0.8f, 0, -3.2f };
        const double walkFor = 3.0 / vr;
        const double arrive = SADDLE_AT - 0.5;
        const double start = arrive - walkFor;
        r.ch->motion.play(carried(rig::clips::walk(cad), from, vr, walkFor), start)
            .play(carried(rig::clips::idle(), { from.x, 0, from.z + vr * (float)walkFor }, 0, 0), arrive, 0.3)
            .play(rig::clips::ride(rl.build), SADDLE_AT, 0.4);
        // A mount eases in over the `blend` before its `from`: the lift starts at SADDLE_AT.
        rig::Mount m = rig::saddle(h.ch, SADDLE_AT + 0.6);
        m.blend = 0.6;
        r.ch->mount(m);
        r.pos = h.pos;
        r.yaw = h.yaw;
        r.show0 = std::min(start, C6 - 1.0);
        r.show1 = h.show1;
        r.ground1 = SADDLE_AT;
        r.endsAt = SADDLE_AT;
        r.feet = humanFeet(rl.build);
        r.eps = 0.02f;
        r.vmax = 0.35f;
        w->rider = add(r);
    }
    // A chimp catches up with a walking person and climbs on their back.
    {
        const float base = w->actors[(size_t)w->horse2].pos(C6 + 5.0).x + 9;
        Actor p;
        p.name = "walker";
        const rig::Look pl = person({ 70, 120, 190, 255 }, { 52, 66, 104, 255 }, "hair.bun");
        p.ch = std::make_shared<rig::Character>(pl);
        const rig::Look cl = rig::animal("chimp");
        const double vk = rig::gaitSpeed(cl.build, rig::Gait::Walk);
        const float cad = std::min(0.9f, (float)(0.55 * vk / rig::walkSpeed(pl.build, 1)));
        const float vp = rig::walkSpeed(pl.build, cad);
        p.ch->motion.play(rig::clips::walk(cad), C6 + 1.0);
        Track tr;
        tr.x0 = base;
        tr.v = { { C6 + 5.0, vp } };
        p.pos = [tr](double s) { return Vector3{ (float)tr.at(s), 0, 0.05f }; };
        p.show0 = C6 + 1.0;
        p.show1 = C6 + 9.6;
        p.feet = humanFeet(pl.build);
        p.eps = 0.02f;
        p.vmax = 0.35f;
        w->walker = add(p);

        Actor c;
        c.name = "chimp2";
        c.ch = std::make_shared<rig::Character>(cl);
        const rig::Build& h = pl.build;
        const float behind = -h.torso.z * 0.5f - cl.build.torso.z * 0.55f - 0.25f;
        const double catchFor = 3.4;
        const float rel = (float)(vk - vp);
        const Vector3 from{ 0, 0, behind - rel * (float)catchFor };
        const double start = BACK_AT - catchFor;
        c.ch->motion.play(carried(rig::clips::knuckleWalk(cl.build), from, rel, catchFor), start)
            .play(rig::clips::ride(cl.build), BACK_AT, 0.3);
        rig::Mount back;
        back.carrier = p.ch;
        back.joint = rig::Joint::Chest;
        back.at = { 0, -h.torso.y * 0.35f, -h.torso.z * 0.5f - cl.build.torso.z * 0.55f };
        back.from = BACK_AT + 0.5;
        back.blend = 0.5;
        back.grips = { { rig::Limb::HandL, rig::Joint::ShoulderL, { 0, 0.04f, -0.02f } },
                       { rig::Limb::HandR, rig::Joint::ShoulderR, { 0, 0.04f, -0.02f } },
                       { rig::Limb::FootL, rig::Joint::Pelvis, { h.hipWidth * 0.75f, -0.02f, h.torso.z * 0.25f } },
                       { rig::Limb::FootR, rig::Joint::Pelvis, { -h.hipWidth * 0.75f, -0.02f, h.torso.z * 0.25f } } };
        c.ch->mount(back);
        c.pos = p.pos;
        c.yaw = p.yaw;
        c.show0 = start;
        c.show1 = p.show1;
        c.ground0 = start;
        c.ground1 = BACK_AT;
        c.endsAt = BACK_AT;
        c.feet = humanFeet(cl.build, Mark::ApeFoot);
        c.feet.push_back({ rig::Joint::WristL, Mark::Knuckle, 0.07f, 0.02f });
        c.feet.push_back({ rig::Joint::WristR, Mark::Knuckle, 0.07f, 0.02f });
        c.eps = 0.015f;
        c.vmax = 0.12f;
        w->chimp2 = add(c);
    }
    // A pilot walks to a titan's foot; the titan lifts it on its palm and walks on.
    {
        const float base = w->actors[(size_t)w->walker].pos(C6 + 8.0).x + 9;

        Actor t;
        t.name = "titan";
        rig::Look tl = rig::Look::robot();
        tl.build = rig::Build::adult().scaled(K);
        t.ch = std::make_shared<rig::Character>(tl);
        const rig::Build tb = tl.build;
        // It bends down and lays its palm on the sand in front of the pilot, the pilot steps
        // on (PALM_AT .. +0.45), and it stands up, lifting the palm to its chest.
        const double lower0 = PALM_AT - 1.4, raise0 = PALM_AT + 0.6, raise1 = PALM_AT + 2.0, go = PALM_AT + 2.1;
        const float cad = 0.6f, stopZ = 0.32f * K + 0.05f;
        const Vector3 low{ -0.3f * K, 0.3f, stopZ + 0.3f }, carry{ -0.34f * K, 1.12f * K, 0.42f * K };
        auto palmTarget = [raise0, raise1, low, carry](double s) {
            return Vector3Lerp(low, carry, smooth((s - raise0) / (raise1 - raise0)));
        };
        auto bend = [lower0, raise0, raise1](double s) {
            return smooth((s - lower0) / 1.2) * (1 - smooth((s - raise0) / (raise1 - raise0)));
        };
        t.ch->motion.play([tb, palmTarget, bend, lower0](double s) {
            const double at = lower0 + s;
            const rig::Pose p = squat(tb, rig::clips::idle(0.5f)(s), bend(at));
            const rig::Pose h = rig::hand(tb, p, rig::Side::Right, palmTarget(at), { -0.3f, 0, 1 }, { 0, 1, 0 });
            return rig::Pose::mix(p, h, smooth(s / 1.2));
        }, lower0).play([tb, palmTarget, go](double s) {
            rig::Pose p = rig::clips::walk(0.6f)(s);
            return rig::hand(tb, p, rig::Side::Right, palmTarget(go + s), { -0.3f, 0, 1 }, { 0, 1, 0 });
        }, go, 0.5);
        const float vt = rig::walkSpeed(tb, cad);
        Track tr;
        tr.x0 = base + 4;
        tr.v = { { go, 0 }, { go + 0.5, vt } };
        t.pos = [tr](double s) { return Vector3{ (float)tr.at(s), 0, -0.3f }; };
        t.show0 = C6 + 6.0;
        t.show1 = chapterAt(8);
        t.feet = humanFeet(tb, Mark::Block);
        t.eps = 0.05f;
        t.vmax = 0.8f;
        w->titan = add(t);

        Actor p;
        p.name = "pilot";
        rig::Look pl = rig::Look::toon();
        pl.hair = "hair.ponytail";
        pl.hairColor = { 120, 64, 38, 255 };
        pl.outfit = rig::Outfit::basic({ 240, 168, 48, 255 }, { 60, 70, 90, 255 }, pl.skin, 0.45f, 1);
        p.ch = std::make_shared<rig::Character>(pl);
        const float pc = 1.0f, vp = rig::walkSpeed(pl.build, pc);
        const Vector3 from{ -0.3f * K, 0, -3.4f };
        const double walkFor = (stopZ - from.z) / vp;
        const double arrive = PALM_AT - 0.9, start = arrive - walkFor;
        p.ch->motion.play(carried(rig::clips::walk(pc), from, vp, walkFor), start)
            .play(carried(rig::clips::idle(), { from.x, 0, stopZ }, 0, 0), arrive, 0.3)
            .play(rig::clips::idle(0.6f), PALM_AT, 0.3);
        rig::Mount palm;
        palm.carrier = t.ch;
        palm.joint = rig::Joint::WristR;
        palm.at = { 0.04f * K, -tb.hand * 0.55f, 0 };
        palm.upright = true;
        palm.from = PALM_AT + 0.45;
        palm.blend = 0.45;
        p.ch->mount(palm);
        p.pos = t.pos;
        p.yaw = t.yaw;
        p.show0 = std::min(start, C6 + 6.0);
        p.show1 = t.show1;
        p.ground1 = PALM_AT;
        p.endsAt = PALM_AT;
        p.feet = humanFeet(pl.build);
        p.eps = 0.02f;
        p.vmax = 0.35f;
        w->pilot = add(p);
    }

    // ---- footfalls, the groove, the head traces.
    for (size_t i = 0; i < w->actors.size(); i++) detect(w->actors[i], (int)i, w->prints);
    {
        const Actor& s = w->actors[(size_t)w->snake];
        const rig::Build& b = s.ch->looks()[0].build;
        const rig::Joint head = rig::joint(b, "head");
        for (double t = s.show0; t <= s.show1; t += 1.0 / 60) {
            const Vector3 p = worldJoint(s, t, head);
            w->groove.pts.push_back({ t, { p.x, p.z } });
        }
        w->groove.width = b.plan->dim("radius") * 2.2f;
        w->groove.highlight = C4 + 2.0;
    }
    auto trace = [&](int who, double from, Trace& out) {
        const Actor& a = w->actors[(size_t)who];
        const rig::Joint head = rig::joint(a.ch->looks()[0].build, "head");
        for (double t = from; t <= from + 4.2; t += 1.0 / 60) out.pts.push_back({ t, worldJoint(a, t, head).x });
    };
    w->henHead0 = C2 + 0.9;
    w->duckHead0 = C2 + 5.2;
    trace(w->hen, w->henHead0, w->henHead);
    trace(w->duck, w->duckHead0, w->duckHead);

    // The gallop's first full moment with no hoof down (a tenth of a second or more: the
    // blend into the gallop leaves slivers before the steady stride).
    {
        std::vector<std::pair<double, double>> down;
        for (const Contact& c : w->prints) if (c.actor == w->horse) down.push_back({ c.t, c.until });
        double from = -1;
        for (double t = GALLOP_AT + 1.0; t < C2; t += 1.0 / 120) {
            bool any = false;
            for (const auto& [a, b] : down) any = any || (t >= a && t <= b);
            if (!any && from < 0) from = t;
            if (any && from >= 0) {
                if (t - from >= 0.1) { w->gap0 = from; w->gap1 = t - 1.0 / 120; break; }
                from = -1;
            }
        }
    }
    // Chapter 4's focus: the T. rex's first print after the chapter starts, and a hen's
    // print laid inside its middle toe for scale.
    for (const Contact& c : w->prints)
        if (c.actor == w->rex && c.until >= C3 + 0.2 && c.until <= C3 + 1.6) { w->rexPrint = c; break; }
    w->henGhost = w->rexPrint;
    w->henGhost.mark = Mark::Hen;
    w->henGhost.size = w->actors[(size_t)w->hen].ch->looks()[0].build.plan->dim("toe");
    w->henGhost.at = { w->rexPrint.at.x + std::cos(w->rexPrint.heading) * w->rexPrint.size * 0.45f,
                       w->rexPrint.at.y + std::sin(w->rexPrint.heading) * w->rexPrint.size * 0.45f };
    w->henGhost.t = std::max(C3 + 1.55, w->rexPrint.until + 0.1);      // 0.3 s before "nine times"
    w->henGhost.ghost = true;
    w->prints.push_back(w->henGhost);
    std::sort(w->prints.begin(), w->prints.end(), [](const Contact& a, const Contact& b) { return a.t < b.t; });

    w->x0 = STRIP_X0;
    w->x1 = w->actors[(size_t)w->titan].pos(chapterAt(8) + 6).x + 14;

    // ---- the camera (brief §6: one truck along the strip, a crane in chapters 4 and 8).
    auto posOf = [w](int i) { return w->actors[(size_t)i].pos; };
    auto mid = [w](int a, int b) {
        auto pa = w->actors[(size_t)a].pos, pb = w->actors[(size_t)b].pos;
        return std::function<Vector3(double)>([pa, pb](double s) { return Vector3Scale(Vector3Add(pa(s), pb(s)), 0.5f); });
    };
    auto fixed = [](Vector3 at) { return std::function<Vector3(double)>([at](double) { return at; }); };
    auto& S = w->shots;
    // 1: low over the sand where the prints press in.
    S.push_back({ 0, 0, follow(posOf(w->horse), { 0.5f, 0.4f, 0 }, { -1.2f, 2.1f, 5.6f }, 34, 3) });
    // 2: wider, the horse in the upper half above the chart.
    S.push_back({ C1 - 0.4, 1.4, follow(posOf(w->horse), { 0.3f, 0.35f, 0 }, { -0.6f, 1.9f, 8.4f }, 34, 4) });
    // 3: the hen, then both birds.
    S.push_back({ C2, 1.0, follow(posOf(w->hen), { 0.3f, 0.2f, 0 }, { -0.25f, 0.5f, 2.3f }, 34, 1.5f) });
    S.push_back({ C2 + 4.4, 1.2, follow(mid(w->hen, w->duck), { 0.45f, 0.2f, 0 }, { -0.25f, 0.55f, 2.6f }, 34, 1.5f) });
    // 4: low on the T. rex's foot as it lands, then a crane up to the head.
    const Vector3 rexAt{ w->rexPrint.at.x, 0, w->rexPrint.at.y };
    S.push_back({ C3, 1.0, follow(fixed(rexAt), { 0.1f, 0.6f, 0 }, { -0.8f, 1.4f, 5.4f }, 34, 5) });
    const Vector3 henAt{ w->henGhost.at.x, 0, w->henGhost.at.y };
    S.push_back({ w->henGhost.t - 0.5, 1.0, follow(fixed(henAt), { 0, 0.02f, 0 }, { -0.25f, 0.8f, 1.6f }, 34, 2) });
    S.push_back({ C3 + 3.4, 2.2, follow(posOf(w->rex), { 0.6f, 2.8f, 0 }, { -3.0f, 0.6f, 14.0f }, 36, 10) });
    // 5: over the snake, looking down into its groove.
    S.push_back({ C4, 1.0, follow(posOf(w->snake), { 0.2f, 0, 0 }, { -0.6f, 1.6f, 2.0f }, 36, 2) });
    // 6: the hen taking off, the turtle at the water, the chimp at the bars.
    const auto hen2 = posOf(w->hen2);
    const double lift = TAKEOFF_AT + 0.3;      // the camera stops there: the hen flies out of frame over its last prints
    const auto hen2Ground = std::function<Vector3(double)>([hen2, lift](double s) { Vector3 p = hen2(std::min(s, lift)); p.y = 0; return p; });
    S.push_back({ C5, 1.0, follow(hen2Ground, { 0.4f, 0.35f, 0 }, { -0.4f, 0.8f, 3.2f }, 34, 2) });
    S.push_back({ C5 + 2.6, 1.0, follow(fixed({ w->pondX + 0.3f, 0, 0 }), { 0, 0.1f, 0 }, { -0.5f, 1.3f, 3.4f }, 34, 2.5f) });
    S.push_back({ C5 + 6.2, 1.0, follow(posOf(w->chimp), { 0.6f, 1.0f, 0 }, { -0.8f, 0.9f, 6.0f }, 34, 4) });
    // 7: the rider, the passenger, the pilot.
    S.push_back({ C6, 1.2, follow(posOf(w->horse2), { -0.6f, 0.8f, 0 }, { -0.8f, 1.2f, 7.2f }, 34, 4) });
    S.push_back({ C6 + 4.6, 1.0, follow(posOf(w->walker), { -0.5f, 0.7f, 0 }, { -0.6f, 0.9f, 5.6f }, 34, 4) });
    S.push_back({ C6 + 7.6, 1.2, follow(posOf(w->titan), { 0.2f, 2.2f, 0 }, { 4.0f, 0.6f, 9.6f }, 36, 8) });
    // 8: crane up until the trackway reads as a map.
    const auto titanPos = posOf(w->titan);
    S.push_back({ C7, 3.4, [titanPos](double s) {
                     const Vector3 t = titanPos(s);
                     return Cam{ { t.x - 3, 19, 15 }, { t.x - 10, 0, 0 }, 40, 18 };
                 } });
    return w;
}

// ------------------------------------------------------------------ the sand
// A print's shape in its own metres (u forward, v to the left), drawn through `to` into
// texture pixels.
void drawMark(const Ctx& c, const Contact& m, const std::function<Vector2(Vector2)>& to, Color col, bool rim) {
    const float s = m.size;
    auto path = [&](const vg::Path& p) { return p.transformed(to); };
    std::vector<vg::Path> fills;
    std::vector<std::pair<vg::Path, float>> strokes;    // centre line, width (metres)
    switch (m.mark) {
    case Mark::Hoof:
        fills.push_back(vg::Path::ellipse({ 0, 0 }, s * 0.52f, s * 0.46f));
        break;
    case Mark::Paw:
        fills.push_back(vg::Path::ellipse({ 0, 0 }, s * 0.5f, s * 0.36f));
        for (int k = -1; k <= 1; k++) fills.push_back(vg::Path::circle({ s * 0.62f, k * s * 0.24f }, s * 0.1f));
        break;
    case Mark::Hen:
    case Mark::Rex:
    case Mark::Duck: {
        const float spread = m.mark == Mark::Duck ? 38.f : 28.f, w = m.mark == Mark::Rex ? 0.2f : 0.16f;
        std::vector<Vector2> tips;
        for (int k = -1; k <= 1; k++) {
            const float a = k * spread * DEG2RAD, L = s * (k == 0 ? 1.f : 0.82f);
            tips.push_back({ std::cos(a) * L, std::sin(a) * L });
            strokes.push_back({ vg::Path::line({ 0, 0 }, tips.back()), s * w });
        }
        if (m.mark == Mark::Hen) strokes.push_back({ vg::Path::line({ 0, 0 }, { -s * 0.32f, s * 0.05f }), s * w * 0.8f });
        if (m.mark == Mark::Duck) {
            vg::Path web;
            web.moveTo({ 0, 0 }).lineTo(tips[0]).quadTo({ s * 0.55f, -s * 0.1f }, tips[1]).quadTo({ s * 0.55f, s * 0.1f }, tips[2]).close();
            fills.push_back(web);
        }
        fills.push_back(vg::Path::circle({ 0, 0 }, s * w * (m.mark == Mark::Rex ? 0.9f : 1.1f)));
        break;
    }
    case Mark::Shoe:
    case Mark::ApeFoot: {
        const bool ape = m.mark == Mark::ApeFoot;
        fills.push_back(vg::Path::ellipse({ -s * 0.28f, 0 }, s * 0.17f, s * (ape ? 0.15f : 0.13f)));
        fills.push_back(vg::Path::ellipse({ s * 0.12f, 0 }, s * 0.34f, s * (ape ? 0.2f : 0.17f)));
        if (ape) fills.push_back(vg::Path::ellipse({ s * 0.12f, s * 0.24f }, s * 0.14f, s * 0.07f));
        break;
    }
    case Mark::Block:
        fills.push_back(vg::Path::rect({ -s * 0.4f, -s * 0.24f, s * 0.86f, s * 0.48f }, s * 0.12f));
        break;
    case Mark::Knuckle:
        for (int k = -1; k <= 1; k++) fills.push_back(vg::Path::ellipse({ -std::abs((float)k) * s * 0.12f, k * s * 0.32f }, s * 0.16f, s * 0.13f));
        break;
    }
    const float ppm = Vector2Distance(to({ 0, 0 }), to({ 1, 0 }));
    auto paint = [&](Vector2 off, Color k) {
        Ctx o = c;
        o.o = Vector2Add(o.o, off);
        for (const vg::Path& p : fills) vg::fill(o, path(p), k);
        for (const auto& [p, wd] : strokes) vg::stroke(o, path(p), vg::Stroke(k, wd * ppm));
    };
    if (m.ghost) {
        for (const vg::Path& p : fills) vg::stroke(c, path(p), vg::Stroke(col, 5));
        for (const auto& [p, wd] : strokes) vg::stroke(c, path(p), vg::Stroke(col, std::max(5.f, wd * ppm)));
        return;
    }
    if (rim) paint({ -2.5f, -2.5f }, RIM);
    paint({ 0, 0 }, col);
}

// One tile of sand: its prints, the groove, the ripples. World x from x0, z across.
void drawTile(const Ctx& c, const World& w, float x0, int t) {
    const double s = t / (double)FPS;
    const float hpx = STRIP_W * PPM;
    dr::rect(c, 0, 0, TILE * PPM, hpx, SAND);
    auto toTex = [x0](Vector2 world) { return Vector2{ (world.x - x0) * PPM, (world.y + STRIP_W / 2) * PPM }; };
    // The groove: the path of the snake's head, pressed as it passes; teal down its middle
    // once every part of the body is shown lying in it.
    {
        std::vector<Vector2> pts;
        for (const auto& [ts, p] : w.groove.pts) {
            if (ts > s) break;
            if (p.x < x0 - 0.5f || p.x > x0 + TILE + 0.5f) continue;
            pts.push_back(toTex(p));
        }
        if (pts.size() > 1) {
            const vg::Path g = vg::Path::polyline(pts);
            Ctx o = c;
            o.o = Vector2Add(o.o, { -2.5f, -2.5f });
            vg::stroke(o, g, vg::Stroke(RIM, w.groove.width * PPM));
            vg::stroke(c, g, vg::Stroke(PRESSED, w.groove.width * PPM));
            const float k = easeIn(t, w.groove.highlight * FPS, 14);
            if (k > 0) vg::stroke(c, g, vg::Stroke(withAlpha(TEAL, 0.85f * k), 5));
        }
    }
    for (const Contact& m : w.prints) {
        if (m.t > s) break;
        if (m.at.x < x0 - 1.2f || m.at.x > x0 + TILE + 1.2f) continue;
        const float press = std::clamp((float)((s - m.t) * FPS / 4), 0.f, 1.f);
        Color col = mixc(SAND, PRESSED, press);
        if (m.last) col = mixc(col, CORAL, easeIn(t, (w.actors[(size_t)m.actor].endsAt + 0.05) * FPS, 10));
        if (m.ghost) col = withAlpha(INK, easeIn(t, m.t * FPS, 10));
        const float ch = std::cos(m.heading), sh = std::sin(m.heading);
        const Vector2 at = m.at;
        drawMark(c, m, [&](Vector2 q) {
            // u forward along the heading; v to the walker's left (-z when walking +x).
            return toTex({ at.x + ch * q.x + sh * q.y, at.y + sh * q.x - ch * q.y });
        }, col, press > 0.5f);
    }
}

// ------------------------------------------------------------------ the scene
std::shared_ptr<g3d::Scene> makeScene(const std::shared_ptr<World>& w, bool ghost) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->skyColor = { 250, 246, 238, 255 };
    scene->groundColor = { 196, 186, 168, 255 };
    scene->environmentGenerator = g3d::studioEnvironment({ 252, 249, 242, 255 }, { 226, 219, 204, 255 }, { 150, 140, 124, 255 }, 3);
    scene->environmentIntensity = 0.45f;
    scene->ambient = 0.45f;
    scene->fogColor = PAPER;
    scene->fogNear = 40;
    scene->fogFar = 90;
    g3d::Light key = g3d::Light::directional({ -2.5f, 5, 4 }, { 255, 247, 234, 255 }, 1.2f, true);
    key.up = [w](int t, g3d::Light& l) {
        const Cam c = w->camAt(t / (double)FPS);
        l.target = { c.tgt.x, 0, c.tgt.z };
        l.position = Vector3Add(l.target, Vector3Scale({ -2.5f, 5, 4 }, std::max(1.f, c.shadow)));
        l.shadowExtent = c.shadow;
    };
    scene->lights.push_back(key);
    scene->lights.push_back(g3d::Light::directional({ 3, 2.5f, -2 }, { 200, 214, 255, 255 }, 0.5f));
    scene->camera.up = [w](int t, g3d::Camera& cam) {
        const Cam c = w->camAt(t / (double)FPS);
        cam.position = c.pos;
        cam.target = c.tgt;
        cam.fov = c.fov;
        cam.farPlane = 400;
    };
    // Shown when the camera's view reaches it.
    auto near = [w](float xa, float xb) {
        return [w, xa, xb](int t, g3d::Object3D& o) {
            const Cam c = w->camAt(t / (double)FPS);
            const float r = Vector3Distance(c.pos, c.tgt) * 1.25f + 2;
            const float lo = std::min(c.pos.x, c.tgt.x) - r, hi = std::max(c.pos.x, c.tgt.x) + r;
            o.visible = xb > lo && xa < hi;
        };
    };
    if (!ghost) {
        // The sand: tiles with their prints drawn each frame; a body under them; the pond.
        const float pond0 = w->pondX, pond1 = w->pondX + POND_LEN;
        for (float x = w->x0; x < w->x1; x += TILE) {
            if (std::fabs(x - pond0) < 0.01f) continue;     // the pond's tile
            auto tex = std::make_shared<g3d::DynamicTexture>();
            tex->width = (int)(TILE * PPM);
            tex->height = (int)(STRIP_W * PPM);
            tex->draw = [w, x](const Ctx& c, int t) { drawTile(c, *w, x, t); };
            g3d::Material m;
            m.color = { 214, 208, 198, 255 };
            m.roughness = 0.95f;
            m.castShadow = false;
            m.dynamicMap = tex;
            g3d::Object3D* o = scene->add(g3d::Geometry::plane(TILE, STRIP_W), m);
            o->position = { x + TILE / 2, 0, 0 };
            o->up = near(x, x + TILE);
        }
        g3d::Material bank;
        bank.color = BANK;
        bank.roughness = 1;
        bank.castShadow = false;
        for (auto [a, b] : { std::pair<float, float>{ w->x0, pond0 }, std::pair<float, float>{ pond1, w->x1 } }) {
            g3d::Object3D* o = scene->add(g3d::Geometry::box(b - a, 0.4f, STRIP_W), bank);
            o->position = { (a + b) / 2, -0.202f, 0 };
        }
        g3d::Material bed = bank;
        bed.color = mixc(BANK, INK, 0.25f);
        scene->add(g3d::Geometry::box(POND_LEN, 0.05f, STRIP_W), bed)->position = { (pond0 + pond1) / 2, -0.42f, 0 };
        g3d::Material water;
        water.color = WATER;
        water.opacity = 0.55f;
        water.roughness = 0.15f;
        water.castShadow = false;
        scene->add(g3d::Geometry::box(POND_LEN, 0.36f, STRIP_W), water)->position = { (pond0 + pond1) / 2, -0.22f, 0 };
        // The bars: rungs across the strip on two rails and four posts.
        g3d::Material wood;
        wood.color = WOOD;
        wood.roughness = 0.8f;
        const float bh = w->barHeight, b0 = w->bars.front().x - 0.4f, b1 = w->bars.back().x + 0.4f;
        for (const Bar& b : w->bars) {
            g3d::Object3D* r = scene->add(g3d::Geometry::cylinder(0.025f, 1.5f, 16), wood);
            r->position = { b.x, bh, 0 };
            r->rotation = { 90, 0, 0 };
        }
        for (float z : { -0.75f, 0.75f }) {
            g3d::Object3D* rail = scene->add(g3d::Geometry::box(b1 - b0, 0.06f, 0.06f), wood);
            rail->position = { (b0 + b1) / 2, bh + 0.02f, z };
            for (float x : { b0, b1 }) scene->add(g3d::Geometry::box(0.08f, bh + 0.05f, 0.08f), wood)->position = { x, (bh + 0.05f) / 2, z };
        }
    }
    for (size_t i = 0; i < w->actors.size(); i++) {
        const Actor& a = w->actors[i];
        if (ghost && (int)i != w->horse) continue;
        g3d::Object3D* root = rig::add(*scene, a.ch, FPS);
        auto pos = a.pos;
        const float yaw = a.yaw;
        const double s0 = a.show0, s1 = a.show1;
        const bool isHorse = (int)i == w->horse;
        root->up = [pos, yaw, s0, s1, ghost, isHorse](int t, g3d::Object3D& o) {
            const double s = t / (double)FPS;
            o.position = pos(s);
            o.rotation = { 0, yaw, 0 };
            o.visible = s >= s0 && s <= s1;
            // Chapter 1: the horse is drawn faint in a second pass until it has appeared.
            if (isHorse && !ghost && s < REVEAL_TO) o.visible = false;
            if (ghost) o.visible = s < REVEAL_TO + 0.05;
        };
    }
    return scene;
}

// ------------------------------------------------------------------ overlays
Vector2 screenOf(const World& w, double s, Vector3 p) {
    const Cam c = w.camAt(s);
    g3d::Camera cam;
    cam.position = c.pos;
    cam.target = c.tgt;
    cam.fov = c.fov;
    Vector2 out{ -1000, -1000 };
    g3d::project(cam, p, { 0, 0, W, H }, out);
    return out;
}

// Chapter 2: four lanes, a bar while each hoof is down, time running right.
void footfallChart(const Ctx& cx, const World& w, int t, float alpha) {
    if (alpha <= 0.01f) return;
    const double s = t / (double)FPS;
    const Ctx m = faded(cx, alpha);
    const float x0 = 300, x1 = 1620, y0 = 760, lane = 30, pxs = 300;     // 4.4 s across
    const char* names[4] = { "LH", "LF", "RF", "RH" };
    dr::roundRect(m, x0 - 120, y0 - 44, x1 - x0 + 160, lane * 4 + 72, 12, withAlpha(CARD, 0.92f));
    text(m, "HOOVES DOWN", { x0 - 96, y0 - 22 }, 20, DIM, 0, 0.5f, true, 3);
    for (int k = 0; k < 4; k++) {
        const float y = y0 + k * lane;
        text(m, names[k], { x0 - 96, y + lane / 2 }, 20, DIM, 0, 0.5f, true);
        dr::rect(m, x0, y + lane / 2 - 1, x1 - x0, 2, RULE);
    }
    auto xOf = [&](double ts) { return (float)(x1 - (s - ts) * pxs); };
    for (const Contact& c : w.prints) {
        if (c.actor != w.horse || c.t > s) continue;
        const float a = std::max(x0, xOf(c.t)), b = std::min(x1, xOf(std::min(c.until, s)));
        if (b <= a) continue;
        dr::roundRect(m, a, y0 + c.foot * lane + 5, b - a, lane - 10, 5, TEAL);
    }
    // The gait names where each starts.
    for (auto [at, name] : { std::pair<double, const char*>{ C1 + 0.1, "WALK" }, { TROT_AT, "TROT" }, { GALLOP_AT, "GALLOP" } }) {
        const float x = xOf(at);
        if (x < x0 - 40 || x > x1 || at > s) continue;
        dr::rect(m, std::max(x0, x), y0 - 6, 2, lane * 4 + 6, INK);
        text(m, name, { std::max(x0, x) + 8, y0 + lane * 4 + 16 }, 18, INK, 0, 0.5f, true, 2);
    }
    // The gap, once the horse has passed it.
    const double shown = std::max(w.gap1 + 0.1, C1 + 7.6);      // 0.3 s before its caption
    if (s > shown) {
        const float k = easeIn(t, shown * FPS, 10);
        const float a = xOf(w.gap0), b = xOf(w.gap1);
        if (b > x0 && a < x1) {
            const Ctx g = faded(m, k);
            const float pad = 6;
            const Vector2 r[4] = { { a - pad, y0 - 8 }, { b + pad, y0 - 8 }, { b + pad, y0 + lane * 4 + 8 }, { a - pad, y0 + lane * 4 + 8 } };
            dr::polyStroke(g, r, 4, true, 4, CORAL);
            text(g, "nothing touches", { (a + b) / 2, y0 - 26 }, 22, CORAL, 0.5f, 0.5f, true);
        }
    }
}

// Chapter 2: the 1878 card, pinned once the gap has been shown.
void muybridgeCard(const Ctx& cx, float alpha) {
    if (alpha <= 0.01f) return;
    const Ctx m = faded(cx, alpha);
    const float x = 1380, y = 210, wd = 440, ht = 110;
    dr::roundRect(m, x, y, wd, ht, 12, CARD);
    dr::rect(m, x, y + 14, 6, ht - 28, CORAL);
    text(m, "MUYBRIDGE, 1878", { x + 28, y + 34 }, 24, INK, 0, 0.5f, true, 2);
    text(m, "photographed a galloping horse", { x + 28, y + 66 }, 21, DIM, 0, 0.5f);
    text(m, "with all four hooves off the ground", { x + 28, y + 92 }, 21, DIM, 0, 0.5f);
}

// Chapter 3: the head's forward position over time, hen then duck, on one scale.
void headChart(const Ctx& cx, const World& w, int t) {
    const double s = t / (double)FPS;
    const float k = easeIn(t, (w.henHead0 - 0.2) * FPS, 10);
    if (k <= 0.01f) return;
    const Ctx m = faded(cx, k);
    const float x0 = 1260, x1 = 1800, y0 = 230, y1 = 470;
    dr::roundRect(m, x0 - 30, y0 - 56, x1 - x0 + 60, y1 - y0 + 110, 12, withAlpha(CARD, 0.92f));
    text(m, "HEAD, DISTANCE FORWARD", { x0, y0 - 26 }, 20, DIM, 0, 0.5f, true, 3);
    dr::rect(m, x0, y1, x1 - x0, 2, RULE);
    dr::rect(m, x0, y0, 2, y1 - y0, RULE);
    text(m, "time", { x1, y1 + 22 }, 18, DIM, 1, 0.5f);
    float span = 0.01f;
    for (const Trace* tr : { &w.henHead, &w.duckHead })
        if (!tr->pts.empty()) span = std::max(span, tr->pts.back().second - tr->pts.front().second);
    auto line = [&](const Trace& tr, Color col, const char* label, float yLabel) {
        if (tr.pts.empty() || s < tr.pts.front().first) return;
        std::vector<Vector2> pts;
        const double t0 = tr.pts.front().first, dur = tr.pts.back().first - t0;
        for (const auto& [ts, x] : tr.pts) {
            if (ts > s) break;
            pts.push_back({ x0 + (float)((ts - t0) / dur) * (x1 - x0), y1 - (x - tr.pts.front().second) / span * (y1 - y0) });
        }
        if (pts.size() > 1) vg::stroke(m, vg::Path::polyline(pts), vg::Stroke(col, 4));
        text(m, label, { x0 + 14, yLabel }, 20, col, 0, 0.5f, true);
    };
    line(w.henHead, TEAL, "hen: hold, thrust, hold", y0 + 18);
    line(w.duckHead, INK, "duck: smooth", y0 + 46);
}

void chrome(const Ctx& cx, int scene, int local) {
    if (scene >= SCENES - 1) return;
    const Ctx m = faded(cx, easeIn(local, 0) * (1 - easeIn(local, DUR[scene] - 12)));
    text(m, "THE TRACKWAY", { 96, 72 }, 22, TEAL, 0, 0.5f, true, 4);
    char n[16];
    std::snprintf(n, sizeof n, "%02d / %02d", scene + 1, SCENES);
    text(m, n, { W - 96, 72 }, 22, DIM, 1, 0.5f, false, 2);
    dr::rect(m, 96, 100, W - 192, 2, RULE);
    dr::rect(m, 96, 100, (W - 192) * std::min(1.f, (local + 0.5f) / DUR[scene]), 2, TEAL);
    const float kh = easeIn(local, scene == 0 ? 8 : 26);
    Ctx hm = faded(m, kh);
    hm.o.x += (1 - kh) * -24 * hm.s;
    text(hm, KICKERS[scene], { 96, 160 }, 64, INK, 0, 0.5f, true);
}

void captions(const Ctx& cx, int t, const std::array<int, SCENES>& st) {
    for (const Caption& c : CAPTIONS) {
        const double from = st[(size_t)c.scene] + c.from * FPS, to = st[(size_t)c.scene] + c.to * FPS;
        if (t < from - 1 || t > to + 8) continue;
        const float a = easeIn(t, from, 8) * (1 - easeIn(t, to, 8));
        // A soft band of paper under the line keeps it legible over the sand.
        const Ctx m = faded(cx, a);
        const float wd = MeasureTextEx(fonts::ui(false), c.text, 36, 0).x;
        dr::roundRect(faded(m, 0.82f), W / 2 - wd / 2 - 28, 1010 - 30, wd + 56, 60, 30, PAPER);
        text(m, c.text, { W / 2, 1010 }, 36, INK, 0.5f, 0.5f);
    }
}

void label(const Ctx& cx, Vector2 at, const char* s, Color col, float alpha, float dy = -40) {
    if (alpha <= 0.01f) return;
    const Ctx m = faded(cx, alpha);
    dr::strokeSeg(m, at, { at.x, at.y + dy + (dy > 0 ? -18 : 14) }, 2, col);
    text(m, s, { at.x, at.y + dy }, 24, col, 0.5f, 0.5f, true, 1);
}

void overlays(const Ctx& cx, const World& w, int t, const std::array<int, SCENES>& st) {
    const double s = t / (double)FPS;
    int i = 0;
    while (i + 1 < SCENES && t >= st[(size_t)i + 1]) i++;
    const int local = t - st[(size_t)i];
    auto ground = [&](Vector2 xz) { return screenOf(w, s, { xz.x, 0, xz.y }); };
    if (i == 1) {
        footfallChart(cx, w, t, easeIn(local, 0, 10) * (1 - easeIn(local, DUR[1] - 12, 10)));
        muybridgeCard(cx, easeIn(t, (std::max(w.gap1 + 0.1, C1 + 7.6) + 1.0) * FPS, 12) * (1 - easeIn(local, DUR[1] - 12, 10)));
    }
    if (i == 2) {
        Ctx m = faded(cx, 1 - easeIn(local, DUR[2] - 12, 10));
        headChart(m, w, t);
    }
    if (i == 3) {
        const float fade = 1 - easeIn(local, DUR[3] - 12, 10);
        const float kr = easeIn(t, (w.rexPrint.t + 0.4) * FPS) * (1 - easeIn(local, 3.4 * FPS, 10)) * fade;
        const Vector2 rp = ground(w.rexPrint.at);
        label(cx, rp, "T. rex", INK, kr, 90);
        const float kh = easeIn(t, (w.henGhost.t + 0.3) * FPS) * (1 - easeIn(local, 3.4 * FPS, 10)) * fade;
        const Vector2 hp = ground(w.henGhost.at);
        label(cx, hp, "hen, same scale", INK, kh, -80);
        text(faded(cx, kh), "sizes illustrative", { hp.x, hp.y - 112 }, 20, DIM, 0.5f, 0.5f);
    }
    if (i == 4) {
        const float k = easeIn(t, w.groove.highlight * FPS) * (1 - easeIn(local, DUR[4] - 12, 10));
        // Label the groove a little behind the head.
        const Actor& sn = w.actors[(size_t)w.snake];
        const double back = s - 3.4;
        for (auto it = w.groove.pts.rbegin(); it != w.groove.pts.rend(); ++it)
            if (it->first <= back) { label(cx, ground(it->second), "one groove", TEAL, k, 80); break; }
        (void)sn;
    }
    if (i == 5) {
        const float fade = 1 - easeIn(local, DUR[5] - 12, 10);
        struct End { int actor; const char* name; };
        for (const End& e : { End{ w.hen2, "air" }, End{ w.turtle, "water" }, End{ w.chimp, "bars" } }) {
            const Actor& a = w.actors[(size_t)e.actor];
            Vector2 at{ 0, 0 };
            int n = 0;
            for (const Contact& c : w.prints) if (c.actor == e.actor && c.last) { at = Vector2Add(at, c.at); n++; }
            if (!n) continue;
            at = Vector2Scale(at, 1.f / n);
            const float k = easeIn(t, (a.endsAt + 0.35) * FPS) * fade;
            label(cx, ground(at), (std::string("trail ends: ") + e.name).c_str(), CORAL, k, 60);
        }
    }
    if (i == 6) {
        // The counter: two bodies, one trail, for each pairing in turn.
        struct Pair { double at, until; const char* name; };
        const Pair pairs[3] = { { SADDLE_AT + 0.3, C6 + 4.6, "RIDER + HORSE" }, { BACK_AT + 0.3, C6 + 7.6, "CHIMP + PERSON" },
                                { PALM_AT + 0.4, C7, "PILOT + TITAN" } };
        for (const Pair& p : pairs) {
            const float k = easeIn(t, p.at * FPS, 10) * (1 - easeIn(t, (p.until - 0.3) * FPS, 8));
            if (k <= 0.01f) continue;
            const Ctx m = faded(cx, k);
            const float x = W - 96, y = 170;
            text(m, p.name, { x, y }, 24, DIM, 1, 0.5f, true, 3);
            text(m, "BODIES 2  \xC2\xB7  TRAILS 1", { x, y + 44 }, 40, CORAL, 1, 0.5f, true, 1);
        }
    }
}

// ------------------------------------------------------------------ sound
// A dry hiss for the snake, filtered noise under its slide.
void hiss(procgen::Score& sc, double t0, double t1, float amp) {
    const int a = (int)(t0 * procgen::Score::RATE), len = (int)((t1 - t0) * procgen::Score::RATE);
    procgen::Svf f;
    for (int k = 0; k < len; k++) {
        const double u = (double)k / len;
        const float env = (float)std::min({ 1.0, u * 8, (1 - u) * 8 }) * (0.75f + 0.25f * (float)std::sin(u * 40));
        sc.put(a + k, f.bp(sc.noise(), 4200, 0.4f, procgen::Score::RATE) * env * amp, 0.5f, 0.15f, 0);
    }
}

void genScore(const std::string& path, const World& w) {
    procgen::Score s(TOTAL / (double)FPS + 1);
    // A sparse pad under the open sand: D A Bm G, thinning in chapter 4's impact.
    const int chords[4][3] = { { 62, 66, 69 }, { 57, 64, 69 }, { 59, 62, 66 }, { 55, 59, 62 } };
    for (int k = 0; k * 4.8 < TOTAL / (double)FPS - 5; k++) {
        const double t0 = k * 4.8;
        for (int n : chords[k % 4]) s.pad(t0, 5.0, n, 0.075f, 800, 0.3f + 0.2f * (n % 3));
        s.bass(t0, 4.6, chords[k % 4][0] - 24, 0.11f);
    }
    // Footfalls, from the contacts that press the prints.
    for (const Contact& c : w.prints) {
        if (c.ghost) continue;
        const std::string& who = w.actors[(size_t)c.actor].name;
        if (who == "horse" || who == "horse2") s.kick(c.t, 0.16f);
        else if (who == "trex") { s.kick(c.t, 0.6f); s.impact(c.t, 0.22f); }
        else if (who == "titan") { s.kick(c.t, 0.5f); s.impact(c.t, 0.12f); }
        else if (who == "hen" || who == "hen2" || who == "duck") s.tick(c.t, who == "duck" ? 0.02f : 0.03f);
        else if (c.mark == Mark::Knuckle) s.tick(c.t, 0.02f);
        else s.kick(c.t, 0.07f);
    }
    // The snake's hiss; wingbeats; the splash; a creak per bar.
    const Actor& sn = w.actors[(size_t)w.snake];
    hiss(s, std::max(sn.show0, C4 - 0.3), sn.show1 - 0.6, 0.05f);
    const float flap = rig::flapRate(w.actors[(size_t)w.hen2].ch->looks()[0].build);
    for (double t = TAKEOFF_AT; t < TAKEOFF_AT + 2.2; t += 1.0 / flap) s.hat(t, 0.05f, 0.4f);
    s.impact(SHORE_AT + 0.45, 0.08f);
    for (int k = 0; k < 4; k++) s.hat(SHORE_AT + 0.45 + 0.07 * k, 0.04f, 0.6f);
    const float rate = rig::brachiateRate(w.actors[(size_t)w.chimp].ch->looks()[0].build);
    for (double t = BARS_AT + 0.4; t < C6; t += 0.5 / rate) s.pluck(t, 45, 0.06f, 0.55f, 9);
    // A rising pluck per mount; the T. rex's roar is a riser.
    s.riser(C3 + 2.4, C3 + 4.2, 0.12f);
    const double mounts[3] = { SADDLE_AT, BACK_AT, PALM_AT };
    for (int k = 0; k < 3; k++) s.pluck(mounts[k] + 0.3, 72 + 4 * k, 0.08f, 0.45f, 5);
    // End card.
    s.impact(C7 + 1.8, 0.4f);
    for (int j = 0; j < 4; j++) s.bell(C7 + 1.85 + 0.12 * j, 74 + j * 3, 0.06f, 0.3f + 0.12f * j);
    for (int n : { 50, 57, 62, 66 }) s.pad(C7 + 1.8, 4.0, n, 0.07f, 1300, 0.5f);
    std::vector<float> mix = s.mix();
    for (float& x : mix) x = std::tanh(x * 3.0f);
    float peak = 0;
    for (float x : mix) peak = std::max(peak, std::fabs(x));
    const float g = peak > 0 ? 0.72f / peak : 1.f;
    for (float& x : mix) x *= g;
    media::writeWav(path, mix, procgen::Score::RATE);
}

void writeSrt(const std::array<int, SCENES>& st) {
    sub::Track tr;
    for (const Caption& c : CAPTIONS)
        tr.cues.push_back({ (st[(size_t)c.scene] / (double)FPS) + c.from, (st[(size_t)c.scene] / (double)FPS) + c.to, c.text, false });
    tr.cues.push_back({ C7 + 1.8, TOTAL / (double)FPS, "Every body signs the ground.", false });
    sub::writeSrt(SRT, tr);
}

void ensureAssets(const std::array<int, SCENES>& st) {
    std::filesystem::create_directories(ASSETS);
    const std::string score = ASSETS + "/score.wav";
    if (!std::filesystem::exists(score)) genScore(score, *makeWorld());
    writeSrt(st);
}

// ------------------------------------------------------------------ film
void buildFilm(Comp& c, const std::array<int, SCENES>& st) {
    const auto w = makeWorld();
    const auto scene = makeScene(w, false), ghost = makeScene(w, true);
    addRoot(c.roots, 0, TOTAL)->fn = [scene, ghost](const Ctx& cx, int t) {
        dr::rect(cx, 0, 0, W, H, PAPER);
        g3d::render(*scene, t, cx);
        const double s = t / (double)FPS;
        if (s < REVEAL_TO + 0.05) {
            const float k = smooth((s - REVEAL_FROM) / (REVEAL_TO - REVEAL_FROM));
            if (k > 0.001f) g3d::render(*ghost, t, faded(cx, k));
        }
    };
    addRoot(c.roots, 0, TOTAL)->fn = [w, st](const Ctx& cx, int t) {
        int i = 0;
        while (i + 1 < SCENES && t >= st[(size_t)i + 1]) i++;
        overlays(cx, *w, t, st);
        chrome(cx, i, t - st[(size_t)i]);
        captions(cx, t, st);
    };
    // End card.
    const int e = st[7];
    addRoot(c.roots, e, DUR[7])->fn = [](const Ctx& cx, int t) {
        const float kt = easeIn(t, 1.8 * FPS);
        Ctx tm = faded(cx, kt);
        tm.o.y += (1 - kt) * 20 * tm.s;
        const Color clear = withAlpha(PAPER, 0);
        dr::rectGrad4(faded(cx, kt), 0, 720, W, 120, clear, clear, PAPER, PAPER);
        dr::rect(faded(cx, kt), 0, 840, W, 240, PAPER);
        text(tm, "EVERY BODY SIGNS THE GROUND.", { W / 2, 860 }, 72, INK, 0.5f, 0.5f, true, 2);
        const float ka = easeIn(t, 3.0 * FPS);
        dr::text(code::font(false), faded(cx, ka), "rig::animal  \xC2\xB7  clips::gait  \xC2\xB7  Character::mount", { W / 2, 940 }, 30, 0, TEAL, 0.5f, 0.5f);
    };
    TNode* logo = prefab::use(c.roots, e + 100, DUR[7] - 100, "raymotion-logo",
                              Json::parse(R"({"size": 56, "layout": "horizontal", "tagline": "video as code", "delay": 2, "ink": "#1f1d2b", "accent": "#139e98"})"));
    logo->props.pos = { W / 2 - 150, 1020 };
    addRoot(c.roots, e + 108, DUR[7] - 108)->fn = [](const Ctx& cx, int t) {
        dr::text(code::font(false), faded(cx, easeIn(t, 0)), "github.com/pebaryan/raymotion", { W / 2 - 30, 1020 }, 26, 0, DIM, 0, 0.5f);
    };
}

void buildThumb(Comp& c) {
    const auto w = makeWorld();
    const auto scene = makeScene(w, false);
    // The film's proof in one picture: chapter 4's push-in, a hen's print inside a T. rex's.
    const int at = (int)((w->henGhost.t + 1.0) * FPS);
    const Vector2 rex = screenOf(*w, at / (double)FPS, { w->rexPrint.at.x, 0, w->rexPrint.at.y });
    const Vector2 hen = screenOf(*w, at / (double)FPS, { w->henGhost.at.x, 0, w->henGhost.at.y });
    addRoot(c.roots, 0, c.duration)->fn = [scene, at, rex, hen](const Ctx& cx, int) {
        dr::rect(cx, 0, 0, W, H, PAPER);
        g3d::render(*scene, at, cx);
        text(cx, "THE TRACKWAY", { 96, 90 }, 30, TEAL, 0, 0.5f, true, 6);
        text(cx, "EVERY BODY SIGNS", { 96, 185 }, 100, INK, 0, 0.5f, true);
        text(cx, "THE GROUND.", { 96, 295 }, 100, CORAL, 0, 0.5f, true);
        label(cx, rex, "T. rex", INK, 1, 110);
        label(cx, hen, "hen", INK, 1, -90);
        text(cx, "hooves / claws / grooves / riders", { 96, 1000 }, 31, INK, 0, 0.5f);
    };
}

} // namespace

void registerTrackway(Registry& out) {
    const std::array<int, SCENES> st = sceneStarts();
    Comp c;
    c.name = "the-trackway";
    c.w = (int)W; c.h = (int)H; c.fps = FPS;
    c.duration = TOTAL;
    c.background = PAPER;
    c.assets = [st](const Comp&, const Json&) { ensureAssets(st); };
    c.build = [st](Comp& c, const Json&) {
        c.roots.clear();
        media::audio(addRoot(c.roots, 0, TOTAL), ASSETS + "/score.wav");
        buildFilm(c, st);
    };
    out.push_back(std::move(c));

    Comp th;
    th.name = "the-trackway-thumb";
    th.w = (int)W; th.h = (int)H; th.fps = FPS; th.duration = 1;
    th.background = PAPER;
    th.build = [](Comp& c, const Json&) { c.roots.clear(); buildThumb(c); };
    out.push_back(std::move(th));
}

} // namespace rmtn
