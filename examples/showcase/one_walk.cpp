// one_walk.cpp -- "one-walk-any-body": a 44 s promo for configurable characters (rig::).
// One walk cycle runs unbroken while every part of the body is swapped out from under it;
// then the same elbow in three materials, a face that is an SVG of named shapes, and one
// character drawn flat and in depth. Text-only: the captions are the script.
//
//   raymotion render one-walk-any-body
//   raymotion still  one-walk-any-body --at 300 --png out/one-walk-300.png
//
// See docs/one-walk-any-body-brief.md and docs/one-walk-any-body-qc.md.
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <raymath.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace rmtn {
namespace {

constexpr float W = 1920.f, H = 1080.f;
constexpr int FPS = 30, XF = 10, SCENES = 6;
constexpr int DUR[SCENES] = { 135, 300, 255, 285, 255, 135 };

constexpr int total() {
    int t = 0;
    for (int i = 0; i < SCENES; i++) t += DUR[i] - (i < SCENES - 1 ? XF : 0);
    return t;
}
constexpr int TOTAL = total();

std::array<int, SCENES> sceneStarts() {
    std::array<int, SCENES> s{};
    int end = 0;
    for (int i = 0; i < SCENES; i++) { s[(size_t)i] = end; end += DUR[i] - (i < SCENES - 1 ? XF : 0); }
    return s;
}

const std::string ASSETS = "out/one-walk-assets";
const std::string SRT = "out/one-walk-any-body.srt";
const std::string FACE_SVG = "characters/face-pip.svg";
const std::string OUTFIT_SVG = "characters/outfit-sailor.svg";

// Palette (brief §6): teal = motion only, coral = the part that just changed, amber = material.
const Color PAPER = mkColor(0xf3efe6), INK = mkColor(0x1f1d2b), DIM = mkColor(0x8a8597),
            TEAL = mkColor(0x139e98), CORAL = mkColor(0xe8553f), AMBER = mkColor(0xf0a830),
            CARD = mkColor(0xfbf9f4), RULE = mkColor(0xd9d3c6),
            FLOOR = mkColor(0xd4d0c8), FLOOR_LINE = mkColor(0xbfb5a2);

const char* const KICKERS[SCENES] = { "ONE WALK", "ANY BODY", "RIGID OR SOFT", "A FACE FROM VECTORS", "FLAT OR DEEP", "" };

// The script (brief §4): scene, window in the scene's local seconds, text. Each line is
// gone (with its 8-frame fade) before its scene's crossfade starts.
struct Caption { int scene; double from, to; const char* text; };
const Caption CAPTIONS[] = {
    { 0, 0.27, 3.85, "This walk has no body yet. Just joints and timing." },
    { 1, 0.27, 3.4, "Put parts on it. The walk never changes." },
    { 1, 3.6, 9.35, "Swap any part, mid-stride." },
    { 2, 0.27, 7.85, "Same elbow, three materials." },
    { 3, 0.27, 3.0, "The face is an SVG of named shapes." },
    { 3, 3.2, 8.85, "Hi! I'm Pip." },
    { 4, 0.27, 7.85, "One character, drawn flat or in depth." },
};

// Chapter 2's swaps, in its local seconds (brief §3b / §9): the tray card that flips.
struct Swap { double at; const char* slot; };
const Swap SWAPS[] = { { 0.6, "BODY" }, { 1.5, "ARMS" }, { 2.4, "HEAD" }, { 3.6, "HEAD" },
                       { 4.8, "ARMS" }, { 6.0, "LEGS" }, { 7.2, "HAT" }, { 8.4, "FEET" } };
constexpr int SWAP_COUNT = (int)(sizeof(SWAPS) / sizeof(SWAPS[0]));
constexpr float CADENCE = 1;                    // strides per second per foot (clips::walk)

// Chapter 3: softness 0 -> 1 between these local seconds.
float softnessAt(double s) {
    return (float)interpolate(s, { 3.6, 7.0 }, { 0.0, 1.0 }, clamped(ease::inOut(ease::cubic)));
}
// Chapter 4: speech and expressions, local seconds.
constexpr double SAY_FROM = 3.2, SAY_TO = 5.1;
const std::pair<double, const char*> FACE_KEYS[] = { { 5.6, "happy" }, { 6.8, "surprised" }, { 8.0, "wink" } };
// Chapter 5: one yaw for both renderings, and the outfit change.
float yawAt(double s) {
    return (float)interpolate(s, { 2.3, 5.0 }, { 0.0, 62.0 }, clamped(ease::inOut(ease::cubic)));
}
constexpr double OUTFIT_AT = 5.8;

// ------------------------------------------------------------------ small helpers
float easeIn(double t, double at, double dur = 12) {
    return (float)interpolate(t - at, { 0.0, dur }, { 0.0, 1.0 }, clamped(ease::out(ease::cubic)));
}
float spr(double t, double at) {
    SpringOptions o;
    o.fps = FPS;
    o.delay = (int)at;
    o.config.damping = 12;
    return (float)spring(t, o);
}
void text(const Ctx& cx, const std::string& s, Vector2 at, float size, Color col, float ax = 0, float ay = 0.5f,
          bool bold = false, float spacing = 0) {
    dr::text(fonts::ui(bold), cx, s.c_str(), at, size, spacing, col, ax, ay);
}
Ctx faded(const Ctx& cx, float a) { Ctx m = cx; m.a *= std::clamp(a, 0.f, 1.f); return m; }

// A ring with an arc from 12 o'clock and a label: the film's one dial.
void dial(const Ctx& cx, Vector2 at, float r, float u, Color col, const std::string& value, const std::string& name) {
    dr::ring(cx, at, r, 10, RULE);
    dr::arc(cx, at, r, 10, -90, -90 + 360 * std::clamp(u, 0.f, 1.f), col);
    const float a = (-90 + 360 * u) * DEG2RAD;
    dr::strokeSeg(cx, at, { at.x + std::cos(a) * (r - 18), at.y + std::sin(a) * (r - 18) }, 5, INK);
    dr::circle(cx, at, 8, INK);
    text(cx, value, { at.x, at.y + r + 34 }, 28, INK, 0.5f, 0.5f, true);
    text(cx, name, { at.x, at.y + r + 66 }, 22, DIM, 0.5f, 0.5f);
}

std::shared_ptr<g3d::Scene> stage(Color floor, float shadowExtent) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->ambient = 0.6f;
    scene->skyColor = { 250, 246, 238, 255 };
    scene->groundColor = { 196, 186, 168, 255 };
    scene->environmentGenerator = g3d::studioEnvironment({ 252, 249, 242, 255 }, { 226, 219, 204, 255 }, { 150, 140, 124, 255 }, 3);
    scene->environmentIntensity = 0.45f;
    scene->ambient = 0.45f;
    // The floor melts into the paper at the horizon instead of ending on a hard line.
    scene->fogColor = PAPER;
    scene->fogNear = 8;
    scene->fogFar = 17;
    g3d::Light key = g3d::Light::directional({ -2.5f, 5, 4 }, { 255, 247, 234, 255 }, 1.2f, true);
    key.shadowExtent = shadowExtent;
    scene->lights.push_back(key);
    scene->lights.push_back(g3d::Light::directional({ 3, 2.5f, -2 }, { 200, 214, 255, 255 }, 0.5f));
    if (floor.a > 0) {
        g3d::Material m;
        m.color = floor;
        m.roughness = 0.95f;
        m.castShadow = false;
        scene->add(g3d::Geometry::plane(30, 30), m)->name = "floor";
    }
    return scene;
}

// Pip's brows sit high in its SVG; a lower box keeps them clear of the hair's fringe.
constexpr Rectangle PIP_BOX{ 0.18f, 0.34f, 0.64f, 0.52f };

// Falls back to the procedural face and plain colours when the committed SVGs are absent.
rig::FacePtr pipFace() {
    if (!std::filesystem::exists(media::resolve(FACE_SVG))) return rig::simpleFace();
    return rig::svgFace(FACE_SVG);
}
rig::Outfit sailor(const rig::Look& l) {
    if (!std::filesystem::exists(media::resolve(OUTFIT_SVG)))
        return rig::Outfit::basic({ 244, 241, 232, 255 }, { 60, 93, 143, 255 }, l.skin, 0.4f, 0.85f);
    return rig::Outfit::svg(OUTFIT_SVG);
}

// ------------------------------------------------------------------ chapters 1-2: the walker
struct Walker {
    rig::CharacterPtr ch;
    std::shared_ptr<g3d::Scene> scene;
    double swapStart = 0;                       // chapter 2's start, seconds
};

rig::Look bare() {
    rig::Look l = rig::Look::toon();
    l.head = l.hair = l.hat = l.torso = l.hips = l.extra = "";
    l.arms("").hands("").legs("").feet("");
    return l;
}

Walker makeWalker(int chapter2Start) {
    Walker w;
    w.swapStart = chapter2Start / (double)FPS;
    rig::Look l = bare();
    l.outfit = rig::Outfit::basic({ 232, 120, 70, 255 }, { 52, 66, 104, 255 }, l.skin, 0.45f, 1.0f);
    w.ch = std::make_shared<rig::Character>(l);
    std::vector<rig::Look> steps;
    l.torso = "torso.round"; l.hips = "hips.round"; l.legs("leg.soft").feet("foot.shoe");  steps.push_back(l);
    l.arms("arm.soft").hands("hand.mitten");                                             steps.push_back(l);
    l.head = "head.round"; l.hair = "hair.short";                                         steps.push_back(l);
    l.head = "head.block"; l.hair = "hair.block";                                         steps.push_back(l);
    l.arms("arm.block").hands("hand.clip");                                               steps.push_back(l);
    l.legs("leg.robot");                                                                  steps.push_back(l);
    l.hat = "hat.hard";                                                                   steps.push_back(l);
    l.feet("foot.flipper");                                                               steps.push_back(l);
    for (int i = 0; i < SWAP_COUNT; i++) w.ch->wear(w.swapStart + SWAPS[i].at, steps[(size_t)i], 0.35);
    w.ch->motion.play(rig::clips::walk(CADENCE), 0);
    w.ch->face.expression(0, "happy", 0);

    w.scene = stage(FLOOR, 2.5f);
    // A floor of planks sliding backwards at walking speed: the walk goes somewhere.
    const float speed = rig::walkSpeed(l.build, CADENCE);
    g3d::Material plank;
    plank.roughness = 0.9f;
    plank.castShadow = false;
    plank.color = FLOOR_LINE;
    for (int i = 0; i < 24; i++) {
        g3d::Object3D* p = w.scene->add(g3d::Geometry::box(30, 0.004f, 0.05f), plank);
        p->up = [i, speed](int t, g3d::Object3D& o) {
            const float off = std::fmod(speed * (t / (float)FPS), 0.6f);
            o.position = { 0, 0.001f, 6.0f - i * 0.6f - off };
        };
    }
    rig::add(*w.scene, w.ch, FPS);
    w.scene->camera.position = { 2.9f, 1.45f, 3.9f };
    w.scene->camera.target = { 0.62f, 0.74f, 0 };
    w.scene->camera.fov = 34;
    return w;
}

const std::pair<rig::Joint, rig::Joint> BONES[] = {
    { rig::Joint::Pelvis, rig::Joint::Chest }, { rig::Joint::Chest, rig::Joint::Neck }, { rig::Joint::Neck, rig::Joint::Head },
    { rig::Joint::Neck, rig::Joint::ShoulderL }, { rig::Joint::ShoulderL, rig::Joint::ElbowL }, { rig::Joint::ElbowL, rig::Joint::WristL },
    { rig::Joint::Neck, rig::Joint::ShoulderR }, { rig::Joint::ShoulderR, rig::Joint::ElbowR }, { rig::Joint::ElbowR, rig::Joint::WristR },
    { rig::Joint::Pelvis, rig::Joint::HipL }, { rig::Joint::HipL, rig::Joint::KneeL }, { rig::Joint::KneeL, rig::Joint::AnkleL },
    { rig::Joint::Pelvis, rig::Joint::HipR }, { rig::Joint::HipR, rig::Joint::KneeR }, { rig::Joint::KneeR, rig::Joint::AnkleR },
};

// The x-ray: every joint projected from the pose, with the bones between them.
void skeleton(const Ctx& cx, const Walker& w, int t, float alpha) {
    if (alpha <= 0.01f) return;
    const double s = t / (double)FPS;
    const rig::Build& b = w.ch->lookAt(s).build;
    const auto f = rig::solve(b, w.ch->poseAt(s));
    const g3d::Camera cam = g3d::cameraAt(w.scene->camera, t);
    const Rectangle vp{ 0, 0, W, H };
    std::array<Vector2, rig::kJoints> p{};
    for (int i = 0; i < rig::kJoints; i++) g3d::project(cam, rig::position(f, (rig::Joint)i), vp, p[(size_t)i]);
    const Ctx m = faded(cx, alpha);
    for (const auto& [a, c] : BONES) dr::strokeSeg(m, p[(size_t)a], p[(size_t)c], 6, TEAL);
    Vector2 top;
    g3d::project(cam, Vector3Transform({ 0, b.head * 0.5f, 0 }, f[(size_t)rig::Joint::Head]), vp, top);
    dr::ring(m, top, 34, 6, TEAL);
    for (int i = 1; i < rig::kJoints; i++) {
        dr::circle(m, p[(size_t)i], 9, TEAL);
        dr::circle(m, p[(size_t)i], 4.5f, CARD);
    }
}

// One turn per stride; a dot at each footfall.
void phaseRing(const Ctx& cx, Vector2 at, double s, float alpha) {
    if (alpha <= 0.01f) return;
    const Ctx m = faded(cx, alpha);
    const float u = (float)std::fmod(s * CADENCE, 1.0);
    dr::ring(m, at, 58, 8, RULE);
    dr::arc(m, at, 58, 8, -90, -90 + 360 * u, TEAL);
    for (int k = 0; k < 2; k++) {
        const float a = (-90 + 90 + 180 * k) * DEG2RAD;
        dr::circle(m, { at.x + std::cos(a) * 58, at.y + std::sin(a) * 58 }, 9, INK);
    }
    text(m, "1 stride", { at.x, at.y }, 22, INK, 0.5f, 0.5f, true);
    text(m, "walk cycle", { at.x, at.y + 96 }, 22, DIM, 0.5f, 0.5f);
}

// The parts tray: one card per slot, flipping to its new part.
void tray(const Ctx& cx, const Walker& w, double s, float alpha) {
    if (alpha <= 0.01f) return;
    const Ctx m = faded(cx, alpha);
    struct Row { const char* label; const char* slot; };
    const Row rows[] = { { "HEAD", "head" }, { "HAIR", "hair" }, { "HAT", "hat" }, { "BODY", "torso" },
                         { "ARMS", "armL" }, { "HANDS", "handL" }, { "LEGS", "legL" }, { "FEET", "footL" } };
    const float x = 1400, y0 = 250, rowH = 74, wCard = 420;
    text(m, "PARTS", { x, y0 - 50 }, 22, DIM, 0, 0.5f, true, 4);
    for (int i = 0; i < 8; i++) {
        const float y = y0 + i * rowH;
        const rig::Look& look = w.ch->looks()[(size_t)w.ch->lookIndex(rows[i].slot, s)];
        std::string part;
        for (const auto& [key, name] : rig::slots(look)) if (key == rows[i].slot) part = name;
        const float pop = w.ch->popAt(rows[i].slot, s);
        const bool fresh = w.ch->lookIndex(rows[i].slot, s) > 0 && s - w.ch->changeTime(w.ch->lookIndex(rows[i].slot, s)) < 0.9;
        dr::roundRect(m, x, y, wCard, rowH - 12, 10, part.empty() ? withAlpha(CARD, 0.6f) : CARD);
        dr::roundRect(m, x, y, 8, rowH - 12, 4, fresh ? CORAL : (part.empty() ? RULE : INK));
        text(m, rows[i].label, { x + 28, y + (rowH - 12) / 2 }, 22, DIM, 0, 0.5f, true, 2);
        Ctx pm = m;
        pm.a *= std::min(1.f, pop);
        text(pm, part.empty() ? "-" : part, { x + 150, y + (rowH - 12) / 2 }, 28, fresh ? CORAL : INK, 0, 0.5f, false);
    }
}

// ------------------------------------------------------------------ chapter 3: materials
struct Materials {
    std::shared_ptr<g3d::Scene> scene;
    std::array<g3d::Object3D*, 3> roots{};
};
constexpr double ENTER[3] = { 0.3, 1.2, 2.1 };
const char* const MATERIAL[3] = { "RIGID", "SOFT", "RUBBER HOSE" };

Materials makeMaterials() {
    Materials m;
    m.scene = stage(FLOOR, 3.0f);
    const char* arms[3] = { "arm.rigid", "arm.soft", "arm.hose" };
    const char* legs[3] = { "leg.rigid", "leg.soft", "leg.soft" };
    const char* hands[3] = { "hand.mitten", "hand.mitten", "hand.glove" };
    const Color tops[3] = { { 70, 120, 190, 255 }, { 232, 120, 70, 255 }, { 120, 160, 90, 255 } };
    for (int i = 0; i < 3; i++) {
        rig::Look l = rig::Look::toon();
        l.arms(arms[i]).legs(legs[i]).hands(hands[i]);
        l.hair = i == 1 ? "hair.bun" : (i == 2 ? "hair.spiky" : "hair.short");
        l.outfit = rig::Outfit::basic(tops[i], { 52, 66, 104, 255 }, l.skin, 0.45f, 1.0f);
        l.softness = i == 2 ? 1.f : 0.f;
        auto ch = std::make_shared<rig::Character>(l);
        ch->motion.play(rig::clips::idle(), 0).play(rig::clips::wave(rig::Side::Left), 2.8, 0.4);
        ch->face.expression(2.8, "happy", 0.3);
        if (i == 1) ch->softness = [](double s) { return softnessAt(s); };
        g3d::Object3D* root = rig::add(*m.scene, ch, FPS);
        const float x = (i - 1) * 1.45f;
        const double enter = ENTER[i];
        root->up = [x, enter](int t, g3d::Object3D& o) {
            const float k = spr(t, enter * FPS);
            o.position = { x, 0, 0 };
            o.rotation = { 0, 18, 0 };
            o.scale = { k, k, k };
            o.visible = k > 0.001f;
        };
        m.roots[(size_t)i] = root;
    }
    m.scene->camera.position = { 0, 1.15f, 5.6f };
    m.scene->camera.target = { 0, 0.72f, 0 };
    m.scene->camera.fov = 34;
    return m;
}

void materialsOverlay(const Ctx& cx, const Materials& m, int t) {
    const double s = t / (double)FPS;
    const g3d::Camera cam = g3d::cameraAt(m.scene->camera, t);
    for (int i = 0; i < 3; i++) {
        const float k = easeIn(t, ENTER[i] * FPS + 4);
        Vector2 at;
        g3d::project(cam, { (i - 1) * 1.45f, 0, 0.4f }, { 0, 0, W, H }, at);
        text(faded(cx, k), MATERIAL[i], { at.x, at.y + 54 }, 28, i == 1 ? AMBER : INK, 0.5f, 0.5f, true, 2);
    }
    const float k = easeIn(t, 3.0 * FPS);
    if (k > 0.01f) {
        char v[32];
        std::snprintf(v, sizeof v, "softness %.2f", softnessAt(s));
        dial(faded(cx, k), { 1690, 230 }, 62, softnessAt(s), AMBER, v, "same elbow, same wave");
    }
}

// ------------------------------------------------------------------ chapter 4: the face
struct FaceShot {
    rig::CharacterPtr ch;
    std::shared_ptr<g3d::Scene> scene;
    std::shared_ptr<const vg::Drawing> svg;     // for the mouth-shape strip (null: no SVG)
};

FaceShot makeFace() {
    FaceShot f;
    rig::Look l = rig::Look::toon();
    l.face = pipFace();
    l.faceBox = PIP_BOX;
    l.hair = "hair.ponytail";
    l.hairColor = { 120, 64, 38, 255 };
    l.outfit = sailor(l);
    f.ch = std::make_shared<rig::Character>(l);
    f.ch->motion.play(rig::clips::idle(0.7f), 0);
    f.ch->face.say(SAY_FROM, SAY_TO, "Hi! I'm Pip.");
    for (const auto& [at, name] : FACE_KEYS) f.ch->face.expression(at, name, 0.18);
    f.ch->face.look(0.0, { 0.35f, 0 }, 0).look(2.6, { 0, 0 }, 0.4).look(6.8, { -0.4f, -0.2f }, 0.3).look(7.6, { 0, 0 }, 0.3);
    if (std::filesystem::exists(media::resolve(FACE_SVG)))
        f.svg = std::make_shared<vg::Drawing>(vg::Drawing::load(FACE_SVG));
    f.scene = stage({ 0, 0, 0, 0 }, 1.5f);
    g3d::Object3D* root = rig::add(*f.scene, f.ch, FPS);
    root->rotation.y = -12;
    const float head = l.build.height() - l.build.head * 0.5f;
    f.scene->camera.fov = 30;
    f.scene->camera.up = [head](int t, g3d::Camera& c) {
        const float k = (float)interpolate(t, { 0.0, (double)DUR[3] }, { 0.0, 1.0 }, clamped(ease::inOut(ease::quad)));
        c.position = { 0.05f, head + 0.05f, 1.75f - 0.3f * k };
        c.target = { -0.42f, head - 0.08f, 0 };
    };
    return f;
}

void faceOverlay(const Ctx& cx, const FaceShot& f, int t) {
    const double s = t / (double)FPS;
    const rig::FaceState st = f.ch->face.at(s);
    const float k = easeIn(t, 0.9 * FPS);
    if (k <= 0.01f) return;
    const Ctx m = faded(cx, k);
    const char shapes[] = "ABCDEFGH";
    const float x0 = 120, y0 = 360, cw = 150, chh = 130, gap = 18;
    text(m, "MOUTH SHAPES", { x0, y0 - 46 }, 22, DIM, 0, 0.5f, true, 4);
    for (int i = 0; i < 8; i++) {
        const float x = x0 + (i % 4) * (cw + gap), y = y0 + (i / 4) * (chh + gap + 30);
        const float lit = (st.mouth == shapes[i] ? st.mouthMix : 0.f) + (st.previousMouth == shapes[i] ? 1 - st.mouthMix : 0.f);
        dr::roundRect(m, x, y, cw, chh, 12, CARD);
        if (lit > 0.01f) dr::roundRect(faded(m, lit), x - 3, y - 3, cw + 6, chh + 6, 14, CORAL);
        dr::roundRect(m, x + 4, y + 4, cw - 8, chh - 8, 10, CARD);
        if (f.svg) {
            if (const vg::Shape* sh = f.svg->find(std::string("mouth-") + shapes[i])) {
                Ctx sc = m;
                const Rectangle b = sh->path.bounds();
                const float scale = std::min((cw - 40) / std::max(1.f, b.width), (chh - 50) / std::max(1.f, b.height));
                sc.s *= scale;
                sc.o = mapP(m, { x + cw / 2 - (b.x + b.width / 2) * scale, y + chh / 2 - 8 - (b.y + b.height / 2) * scale });
                vg::draw(sc, sh->path, sh->style);
            }
        }
        text(m, std::string(1, shapes[i]), { x + cw / 2, y + chh - 16 }, 20, lit > 0.5f ? CORAL : DIM, 0.5f, 0.5f, true);
    }
    text(m, "letters to shapes (approx.)", { x0, y0 + 2 * (chh + gap + 30) - 8 }, 22, DIM, 0, 0.5f);
    // The expression in play.
    const float ke = easeIn(t, FACE_KEYS[0].first * FPS - 8);
    if (ke > 0.01f) {
        const Ctx e = faded(m, ke);
        text(e, "EXPRESSION", { x0, 870 }, 22, DIM, 0, 0.5f, true, 4);
        text(e, st.expression, { x0, 915 }, 40, CORAL, 0, 0.5f, true);
    }
}

// ------------------------------------------------------------------ chapter 5: flat or deep
struct Twin {
    rig::CharacterPtr ch;
    std::shared_ptr<g3d::Scene> scene;
    g3d::Object3D* root = nullptr;
};

Twin makeTwin() {
    Twin w;
    rig::Look l = rig::Look::toon();
    l.hair = "hair.bun";
    l.hairColor = { 40, 34, 40, 255 };
    l.outfit = rig::Outfit::basic({ 232, 120, 70, 255 }, { 52, 66, 104, 255 }, l.skin, 0.45f, 1.0f);
    l.face = pipFace();
    l.faceBox = PIP_BOX;
    w.ch = std::make_shared<rig::Character>(l);
    rig::Look striped = l;
    striped.outfit = sailor(l);
    w.ch->wear(OUTFIT_AT, striped, 0.35);
    w.ch->motion.play(rig::clips::idle(), 0).play(rig::clips::walk(CADENCE), 1.4, 0.4).play(rig::clips::cheer(), 6.4, 0.4);
    w.ch->face.expression(0, "happy", 0).expression(OUTFIT_AT, "surprised", 0.15).expression(OUTFIT_AT + 0.9, "happy", 0.3);
    w.scene = stage({ 0, 0, 0, 0 }, 2.5f);      // no floor: both halves stand on the paper
    w.root = rig::add(*w.scene, w.ch, FPS);
    w.root->up = [](int t, g3d::Object3D& o) { o.rotation = { 0, yawAt(t / (double)FPS), 0 }; };
    w.scene->camera.position = { 0, 1.0f, 4.2f };
    w.scene->camera.target = { 0, 0.68f, 0 };
    w.scene->camera.fov = 32;
    return w;
}

void twinScene(const Ctx& cx, const Twin& w, int t) {
    const double s = t / (double)FPS;
    // Left: the 2D cut-out.
    rig::View2D v;
    v.yaw = yawAt(s);
    v.scale = 360;
    v.origin = { 480, 880 };
    v.ink = INK;
    v.shadow = { 60, 50, 40, 40 };
    rig::draw(faded(cx, easeIn(t, 0)), *w.ch, s, v);
    // Right: the 3D meshes of the same character.
    const float k3 = easeIn(t, 0.9 * FPS);
    if (k3 > 0.01f) g3d::render(*w.scene, t, faded(cx, k3), { 960, 0, 960, H });
    dr::rect(cx, W / 2 - 1, 140, 2, 780, RULE);
    text(faded(cx, easeIn(t, 4)), "2D cut-out", { 480, 950 }, 28, INK, 0.5f, 0.5f, true);
    text(faded(cx, k3), "3D meshes", { 1440, 950 }, 28, INK, 0.5f, 0.5f, true);
    const float kd = easeIn(t, 2.0 * FPS);
    if (kd > 0.01f) {
        char val[32];
        std::snprintf(val, sizeof val, "yaw %.0f deg", yawAt(s));
        dial(faded(cx, kd), { W / 2, 250 }, 56, yawAt(s) / 360.f, TEAL, val, "one number, both");
    }
}

// ------------------------------------------------------------------ chapter 6: the line-up
std::shared_ptr<g3d::Scene> makeLineup() {
    auto scene = stage(FLOOR, 4.0f);
    const rig::Look looks[4] = { rig::Look::toon(), rig::Look::block(), rig::Look::avatar(), rig::Look::robot() };
    for (int i = 0; i < 4; i++) {
        auto ch = std::make_shared<rig::Character>(looks[i]);
        ch->motion.play(rig::clips::idle(), 0).play(rig::clips::cheer(), 0.5 + 0.12 * i, 0.3);
        ch->face.expression(0.5, "happy", 0.2);
        g3d::Object3D* r = rig::add(*scene, ch, FPS);
        r->position = { (i - 1.5f) * 1.25f, 0, 0 };
        r->rotation.y = (i - 1.5f) * -8;
    }
    scene->camera.position = { 0, 1.15f, 5.2f };
    scene->camera.target = { 0, 0.55f, 0 };
    scene->camera.fov = 36;
    return scene;
}

// ------------------------------------------------------------------ chrome, captions, sound
void chrome(const Ctx& cx, int scene, int local) {
    if (scene >= SCENES - 1) return;
    const Ctx m = faded(cx, easeIn(local, 0) * (1 - easeIn(local, DUR[scene] - 12)));
    text(m, "CHARACTERS", { 96, 72 }, 22, TEAL, 0, 0.5f, true, 4);
    char n[16];
    std::snprintf(n, sizeof n, "%02d / %02d", scene + 1, SCENES);
    text(m, n, { W - 96, 72 }, 22, DIM, 1, 0.5f, false, 2);
    dr::rect(m, 96, 100, W - 192, 2, RULE);
    dr::rect(m, 96, 100, (W - 192) * std::min(1.f, (local + 0.5f) / DUR[scene]), 2, TEAL);
    const float kh = easeIn(local, 8);
    Ctx hm = faded(m, kh);
    hm.o.x += (1 - kh) * -24 * hm.s;
    text(hm, KICKERS[scene], { 96, 160 }, 64, INK, 0, 0.5f, true);
}

void captions(const Ctx& cx, int t, const std::array<int, SCENES>& st) {
    for (const Caption& c : CAPTIONS) {
        const double from = st[(size_t)c.scene] + c.from * FPS, to = st[(size_t)c.scene] + c.to * FPS;
        if (t < from - 1 || t > to + 8) continue;
        const float a = easeIn(t, from, 8) * (1 - easeIn(t, to, 8));
        text(faded(cx, a), c.text, { W / 2, 1010 }, 36, INK, 0.5f, 0.5f);
    }
}

void genScore(const std::string& path, const std::array<int, SCENES>& st, const FaceShot& face) {
    procgen::Score s(TOTAL / (double)FPS + 1);
    auto at = [&](int scene, double sec) { return st[(size_t)scene] / (double)FPS + sec; };
    // A light pad, C G Am F, through the film; it thins under the face chapter's start.
    const int chords[4][3] = { { 60, 64, 67 }, { 55, 62, 67 }, { 57, 60, 64 }, { 53, 57, 60 } };
    for (int k = 0; k * 4.4 < TOTAL / (double)FPS - 4; k++) {
        const double t0 = k * 4.4;
        const bool hush = t0 >= at(3, 0) - 1 && t0 < at(3, 3.0);
        for (int n : chords[k % 4]) s.pad(t0, 4.6, n, hush ? 0.035f : 0.075f, 900, 0.3f + 0.2f * (n % 3));
        s.bass(t0, 4.2, chords[k % 4][0] - 24, 0.12f);
    }
    // Footfalls: two per stride for as long as the walker walks.
    for (double t = 0.25; t < at(2, 0.3); t += 0.5 / CADENCE) s.tick(t, 0.05f);
    // A pluck per part, rising.
    const int notes[SWAP_COUNT] = { 72, 74, 76, 79, 81, 84, 86, 88 };
    for (int i = 0; i < SWAP_COUNT; i++) s.pluck(at(1, SWAPS[i].at), notes[i], 0.08f, 0.35f + 0.04f * i, 5);
    // The softness dial: a riser under the sweep, a bell where it lands.
    s.riser(at(2, 3.6), at(2, 7.0), 0.16f);
    s.bell(at(2, 7.0), 84, 0.07f, 0.5f);
    // Mouth blips while Pip speaks, then a bell per expression.
    char last = 'X';
    for (int f = (int)(SAY_FROM * FPS); f <= (int)(SAY_TO * FPS) + 2; f++) {
        const char mth = face.ch->face.at(f / (double)FPS).mouth;
        if (mth != last && mth != 'X') s.tick(at(3, f / (double)FPS), 0.035f);
        last = mth;
    }
    for (const auto& [t, name] : FACE_KEYS) s.bell(at(3, t), 79, 0.05f, 0.6f);
    // The turn and the outfit change.
    s.riser(at(4, 2.3), at(4, 5.0), 0.1f);
    s.pluck(at(4, OUTFIT_AT), 79, 0.08f, 0.5f, 5);
    // End card.
    s.impact(at(5, 0.7), 0.5f);
    for (int j = 0; j < 4; j++) s.bell(at(5, 0.75 + 0.12 * j), 72 + j * 4, 0.06f, 0.3f + 0.12f * j);
    for (int n : { 48, 55, 60, 64 }) s.pad(at(5, 0.7), 3.6, n, 0.07f, 1300, 0.5f);
    std::vector<float> mix = s.mix();
    for (float& x : mix) x = std::tanh(x * 2.0f);
    float peak = 0;
    for (float x : mix) peak = std::max(peak, std::fabs(x));
    const float g = peak > 0 ? 0.70f / peak : 1.f;
    for (float& x : mix) x *= g;
    media::writeWav(path, mix, procgen::Score::RATE);
}

void writeSrt(const std::array<int, SCENES>& st) {
    sub::Track tr;
    for (const Caption& c : CAPTIONS)
        tr.cues.push_back({ (st[(size_t)c.scene] / (double)FPS) + c.from, (st[(size_t)c.scene] / (double)FPS) + c.to, c.text, false });
    tr.cues.push_back({ st[5] / (double)FPS + 0.7, TOTAL / (double)FPS, "One rig. Any body.", false });
    sub::writeSrt(SRT, tr);
}

bool assetsReady = false;
void ensureAssets(const std::array<int, SCENES>& st) {
    std::filesystem::create_directories(ASSETS);
    const std::string score = ASSETS + "/score.wav";
    if (!std::filesystem::exists(score)) genScore(score, st, makeFace());
    writeSrt(st);
    assetsReady = true;
}

// ------------------------------------------------------------------ film
void buildFilm(Comp& c, const std::array<int, SCENES>& st) {
    addRoot(c.roots, 0, TOTAL)->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, W, H, PAPER); };

    // Chapters 1-2 share one walker on its own root, so the walk is never cut.
    const auto walker = std::make_shared<Walker>(makeWalker(st[1]));
    const int walkEnd = st[2] + XF;
    TNode* wn = addRoot(c.roots, 0, walkEnd);
    wn->fn = g3d::draw(walker->scene);
    wn->up = [walkEnd](int t, NodeProps& p) { p.opacity = 1 - easeIn(t, walkEnd - XF, XF); };
    TNode* xr = addRoot(c.roots, 0, walkEnd);
    xr->fn = [walker, st](const Ctx& cx, int t) {
        const double s = t / (double)FPS;
        // Full strength while there is no body; an x-ray once parts arrive.
        const float bodyIn = easeIn(t, st[1] + SWAPS[0].at * FPS, 10);
        skeleton(cx, *walker, t, 1 - 0.5f * bodyIn);
        phaseRing(cx, { 1230, 820 }, s, easeIn(t, 1.73 * FPS));
    };
    xr->up = [walkEnd](int t, NodeProps& p) { p.opacity = 1 - easeIn(t, walkEnd - XF, XF); };

    TransitionSeries ts(c.roots, W, H);
    auto add = [&](int i, std::function<void(const Ctx&, int)> fn) {
        if (i) ts.transition(Transition::fade(XF));
        TNode* n = ts.add(DUR[i]);
        n->fn = std::move(fn);
        if (n->start != st[(size_t)i]) std::fprintf(stderr, "one-walk scene %d start %d expected %d\n", i, n->start, st[(size_t)i]);
    };
    add(0, [](const Ctx&, int) {});
    add(1, [walker, st](const Ctx& cx, int t) { tray(cx, *walker, (st[1] + t) / (double)FPS, easeIn(t, 0, 9)); });
    const auto materials = std::make_shared<Materials>(makeMaterials());
    add(2, [materials](const Ctx& cx, int t) {
        g3d::render(*materials->scene, t, cx);
        materialsOverlay(cx, *materials, t);
    });
    const auto face = std::make_shared<FaceShot>(makeFace());
    add(3, [face](const Ctx& cx, int t) {
        g3d::render(*face->scene, t, faded(cx, easeIn(t, 0, 9)));
        faceOverlay(cx, *face, t);
    });
    const auto twin = std::make_shared<Twin>(makeTwin());
    add(4, [twin](const Ctx& cx, int t) { twinScene(cx, *twin, t); });
    const auto lineup = makeLineup();
    add(5, [lineup](const Ctx& cx, int t) {
        g3d::render(*lineup, t, faded(cx, easeIn(t, 0, 9)));
        const float kt = easeIn(t, 0.7 * FPS);
        Ctx tm = faded(cx, kt);
        tm.o.y += (1 - kt) * 20 * tm.s;
        text(tm, "ONE RIG. ANY BODY.", { W / 2, 860 }, 76, INK, 0.5f, 0.5f, true, 2);
        const float ka = easeIn(t, 1.6 * FPS);
        dr::text(code::font(false), faded(cx, ka), "rig::Character  \xC2\xB7  rig::Look  \xC2\xB7  rig::parts", { W / 2, 940 }, 30, 0, TEAL, 0.5f, 0.5f);
    });

    addRoot(c.roots, 0, TOTAL)->fn = [st](const Ctx& cx, int t) {
        int i = 0;
        while (i + 1 < SCENES && t >= st[(size_t)i + 1]) i++;
        chrome(cx, i, t - st[(size_t)i]);
        captions(cx, t, st);
    };
    TNode* logo = prefab::use(c.roots, st[5] + 48, DUR[5] - 48, "raymotion-logo",
                              Json::parse(R"({"size": 64, "layout": "horizontal", "tagline": "video as code", "delay": 2, "ink": "#1f1d2b", "accent": "#139e98"})"));
    logo->props.pos = { W / 2 - 150, 1020 };
    // The call to action: where the code lives.
    addRoot(c.roots, st[5] + 60, DUR[5] - 60)->fn = [](const Ctx& cx, int t) {
        dr::text(code::font(false), faded(cx, easeIn(t, 0)), "github.com/pebaryan/raymotion", { W / 2 - 30, 1020 }, 26, 0, DIM, 0, 0.5f);
    };
}

void buildThumb(Comp& c) {
    const auto lineup = makeLineup();
    // The same line-up, framed off-centre so the title has the left third.
    lineup->camera.position = { -1.15f, 1.2f, 7.4f };
    lineup->camera.target = { -1.15f, 0.62f, 0 };
    addRoot(c.roots, 0, c.duration)->fn = [lineup](const Ctx& cx, int) {
        dr::rect(cx, 0, 0, W, H, PAPER);
        g3d::render(*lineup, 45, cx);
        text(cx, "CHARACTERS", { 96, 300 }, 30, TEAL, 0, 0.5f, true, 6);
        text(cx, "ONE RIG.", { 96, 400 }, 110, INK, 0, 0.5f, true);
        text(cx, "ANY BODY.", { 96, 520 }, 110, CORAL, 0, 0.5f, true);
        text(cx, "rigid or soft  /  SVG faces  /  2D or 3D", { 96, 640 }, 34, DIM, 0, 0.5f);
    };
}

} // namespace

void registerOneWalk(Registry& out) {
    const std::array<int, SCENES> st = sceneStarts();
    Comp c;
    c.name = "one-walk-any-body";
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
    th.name = "one-walk-any-body-thumb";
    th.w = (int)W; th.h = (int)H; th.fps = FPS; th.duration = 1;
    th.background = PAPER;
    th.build = [](Comp& c, const Json&) { c.roots.clear(); buildThumb(c); };
    out.push_back(std::move(th));
}

} // namespace rmtn
