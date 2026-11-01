// skin_demo.cpp -- "skin-demo": a 13 s utility clip for rig::loadSkin. A body baked onto the
// humanoid rig by tools/skin_bake.py walks, runs, waves and sits on a block, playing the same
// clips the primitive characters use; mid-walk it turns translucent and its joints show.
// Silent; the clip labels are the script.
//
//   python tools/skin_bake.py body.glb out/body.glb
//   raymotion render skin-demo --props '{"glb": "out/body.glb"}'
//
// See docs/skin-demo-brief.md and docs/skin-demo-qc.md.
#include "rmtn/raymotion.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace rmtn {
namespace {

constexpr float W = 1920.f, H = 1080.f;
constexpr int FPS = 30, TOTAL = 13 * FPS;

const Color PAPER = mkColor(0xf3efe6), INK = mkColor(0x1f1d2b), DIM = mkColor(0x8a8597),
            TEAL = mkColor(0x16a39a), FLOOR = mkColor(0xe9e2d4), LINE = mkColor(0xd8cfbd),
            BLOCK = mkColor(0xb9a98c), BODY = mkColor(0xc9c4bc);

// ------------------------------------------------------------------ timing (brief §3), seconds
constexpr double RUN_AT = 3.8, SLOW_AT = 5.9, STOP_AT = 6.6, TURN_AT = 6.7, TURN_END = 7.3;
constexpr double WAVE_AT = 7.2, WAVE_END = 9.2, SIT_AT = 9.6, SIT_FADE = 0.8, END_AT = 11.3;
constexpr double XRAY_FROM = 1.6, XRAY_TO = 3.4;
constexpr float SEAT_BACK = 0.36f;      // the block's centre behind the standing feet

float smooth(double u) { u = std::clamp(u, 0.0, 1.0); return (float)(u * u * (3 - 2 * u)); }
// 0 -> 1 over 8 frames from `from`, back to 0 over 8 frames ending at `to`.
float window(double s, double from, double to) {
    const double f = 8.0 / FPS;
    return std::min(smooth((s - from) / f), smooth((to - s) / f));
}
void text(const Ctx& cx, const std::string& s, Vector2 at, float size, Color col, float ax = 0, float ay = 0.5f,
          bool bold = false, float spacing = 0) {
    dr::text(fonts::ui(bold), cx, s.c_str(), at, size, spacing, col, ax, ay);
}
Ctx faded(const Ctx& cx, float a) { Ctx m = cx; m.a *= std::clamp(a, 0.f, 1.f); return m; }

// Distance from speeds at knots (seconds, m/s), linear between them: the exact integral.
struct Track {
    std::vector<std::pair<double, double>> v;
    double at(double s) const {
        double x = 0;
        if (s <= v[0].first) return 0;
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

struct World {
    rig::Skin skin;
    rig::CharacterPtr ch;
    Track track;
    float x0 = -6, stopX = 0;
    float xAt(double s) const { return x0 + (float)track.at(s); }
    float yawAt(double s) const { return 90 * (1 - smooth((s - TURN_AT) / (TURN_END - TURN_AT))); }
    Matrix node(double s) const {
        return MatrixMultiply(MatrixRotateY(yawAt(s) * DEG2RAD), MatrixTranslate(xAt(s), 0, 0));
    }
    float xray(double s) const { return window(s, XRAY_FROM, XRAY_TO); }
};

std::shared_ptr<World> makeWorld(const std::string& glb) {
    auto w = std::make_shared<World>();
    w->skin = rig::loadSkin(glb);
    const rig::Build b = w->skin.build;
    rig::Look look = rig::Look::toon();
    look.build = b;
    w->ch = std::make_shared<rig::Character>(look);
    // The seat: its top where the shins just reach the floor, so the seated feet plant.
    const float seat = b.shin + b.ankle - b.legRadius * 0.9f - 0.01f;
    const rig::Clip sit = rig::clips::seated(b, seat);
    // The root stays where the body stood; the seated pose is moved back onto the block.
    const rig::Clip sitBack = [sit](double s) { rig::Pose p = sit(s); p.offset.z -= SEAT_BACK; return p; };
    // A hello wave: clips::wave with the elbow held bent, the forearm up and the hand waving
    // (the clip's own elbow straightens on each beat).
    const rig::Clip wave0 = rig::clips::wave();
    const rig::Clip hello = [wave0](double s) {
        rig::Pose p = wave0(s);
        const float k = smooth(s / 0.35);
        p[rig::Joint::ElbowR] = { -10 * k, 0, k * (85 + 16 * (float)std::sin(TAU * 1.8 * s)) };
        return p;
    };
    w->ch->motion.play(rig::clips::walk(1), 0)
        .play(rig::clips::run(1.4f), RUN_AT, 0.4)
        .play(rig::clips::walk(1), SLOW_AT, 0.4)
        .play(rig::clips::idle(0.6f), STOP_AT, 0.4)
        .play(hello, WAVE_AT, 0.3)
        .play(rig::clips::idle(0.6f), WAVE_END, 0.3)
        .play(sitBack, SIT_AT, SIT_FADE);
    const double vw = rig::walkSpeed(b, 1), vr = rig::runSpeed(b, 1.4f);
    w->track.v = { { 0, vw }, { RUN_AT, vw }, { RUN_AT + 0.4, vr }, { SLOW_AT, vr }, { SLOW_AT + 0.4, vw },
                   { STOP_AT, vw }, { STOP_AT + 0.4, 0 } };
    w->stopX = w->xAt(100);
    return w;
}

// The camera: a 3/4 follow ahead of the body, settling frontal for the wave and the sit.
struct Cam { Vector3 pos, tgt; float fov; };
Cam camAt(const World& w, double s) {
    const float x = w.xAt(s), k = smooth((s - 6.0) / 1.8), push = smooth((s - END_AT + 0.6) / 1.6);
    const Vector3 follow{ x + 2.3f, 1.45f, 4.4f }, followT{ x + 0.5f, 0.95f, 0 };
    const Vector3 front{ w.stopX + 0.9f, 1.35f, 4.3f }, frontT{ w.stopX + 0.15f, 0.82f, -0.1f };
    Cam c{ Vector3Lerp(follow, front, k), Vector3Lerp(followT, frontT, k), 34 };
    c.pos.z -= 0.5f * push;
    c.tgt.y += 0.12f * push;
    return c;
}
Vector2 screenOf(const World& w, double s, Vector3 p) {
    const Cam c = camAt(w, s);
    g3d::Camera cam;
    cam.position = c.pos;
    cam.target = c.tgt;
    cam.fov = c.fov;
    Vector2 out{ -1000, -1000 };
    g3d::project(cam, p, { 0, 0, W, H }, out);
    return out;
}

std::shared_ptr<g3d::Scene> makeScene(const std::shared_ptr<World>& w) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->skyColor = { 250, 246, 238, 255 };
    scene->groundColor = { 196, 186, 168, 255 };
    scene->environmentGenerator = g3d::studioEnvironment({ 252, 249, 242, 255 }, { 226, 219, 204, 255 }, { 150, 140, 124, 255 }, 3);
    scene->environmentIntensity = 0.45f;
    scene->ambient = 0.45f;
    scene->fogColor = PAPER;
    scene->fogNear = 14;
    scene->fogFar = 40;
    g3d::Light key = g3d::Light::directional({ -2.5f, 5, 4 }, { 255, 247, 234, 255 }, 1.2f, true);
    key.up = [w](int t, g3d::Light& l) {
        const float x = w->xAt(t / (double)FPS);
        l.target = { x, 0, 0 };
        l.position = Vector3Add(l.target, { -2.5f, 5, 4 });
        l.shadowExtent = 4;
    };
    scene->lights.push_back(key);
    scene->lights.push_back(g3d::Light::directional({ 3, 2.5f, -2 }, { 200, 214, 255, 255 }, 0.5f));
    scene->camera.up = [w](int t, g3d::Camera& cam) {
        const Cam c = camAt(*w, t / (double)FPS);
        cam.position = c.pos;
        cam.target = c.tgt;
        cam.fov = c.fov;
        cam.farPlane = 200;
    };
    // The floor, with a line every metre so a sliding foot would show.
    g3d::Material floor;
    floor.color = FLOOR;
    floor.roughness = 0.95f;
    floor.castShadow = false;
    scene->add(g3d::Geometry::plane(60, 14), floor)->position = { 0, 0, 0 };
    g3d::Material line = floor;
    line.color = LINE;
    for (int i = -20; i <= 25; i++) scene->add(g3d::Geometry::plane(0.02f, 14), line)->position = { (float)i, 0.001f, 0 };
    // The block it sits on.
    const rig::Build& b = w->skin.build;
    const float seat = b.shin + b.ankle - b.legRadius * 0.9f - 0.01f;
    g3d::Material wood;
    wood.color = BLOCK;
    wood.roughness = 0.8f;
    scene->add(g3d::Geometry::box(0.5f, seat, 0.42f), wood)->position = { w->stopX, seat / 2, -SEAT_BACK };
    // The body.
    g3d::Object3D* root = rig::add(*scene, w->ch, w->skin, FPS);
    g3d::Object3D* body = root->children.front().get();
    body->modelMaterials = false;
    body->material.color = BODY;
    body->material.roughness = 0.7f;
    body->material.doubleSided = true;      // the bake splits the legs at the crotch: show the inside of the cut, not the floor
    root->up = [w](int t, g3d::Object3D& o) {
        const double s = t / (double)FPS;
        o.position = { w->xAt(s), 0, 0 };
        o.rotation = { 0, w->yawAt(s), 0 };
    };
    const auto pose = body->up;
    body->up = [w, pose](int t, g3d::Object3D& o) {
        pose(t, o);
        const float k = w->xray(t / (double)FPS);
        o.material.opacity = k > 0.001f ? 1 - 0.65f * k : 1.f;
    };
    return scene;
}

// The proof: the rig's joints, drawn through the translucent body.
void joints(const Ctx& cx, const World& w, double s) {
    const float k = w.xray(s);
    if (k <= 0.001f) return;
    const rig::Frames f = rig::solve(w.skin.build, w.ch->poseAt(s));
    const Matrix m = w.node(s);
    auto at = [&](rig::Joint j) { return screenOf(w, s, Vector3Transform(rig::position(f, j), m)); };
    const Ctx c = faded(cx, k);
    for (int i = (int)rig::Joint::Pelvis; i <= (int)rig::Joint::AnkleR; i++) {
        const rig::Joint j = (rig::Joint)i, p = rig::parent(j);
        if (p != rig::Joint::Root) dr::strokeSeg(c, at(p), at(j), 3, withAlpha(TEAL, 170));
    }
    for (int i = (int)rig::Joint::Pelvis; i <= (int)rig::Joint::AnkleR; i++) {
        const Vector2 q = at((rig::Joint)i);
        dr::circle(c, q, 8, PAPER);
        dr::circle(c, q, 6, TEAL);
    }
}

// One clip label at a time (brief reveal map): the clip's name, the call under it.
struct Label { double from, to; const char* name; const char* call; };
const Label LABELS[] = {
    { 0.3, 3.6, "walk", "rig::clips::walk()" },
    { 3.9, 6.4, "run", "rig::clips::run()" },
    { 7.3, 9.3, "wave", "rig::clips::wave(), elbow held" },
    { 9.9, 11.2, "sit", "rig::clips::seated(build, seat)" },
};

void overlays(const Ctx& cx, const World& w, int t) {
    const double s = t / (double)FPS;
    joints(cx, w, s);
    const float kk = window(s, 0.3, END_AT - 0.1);
    text(faded(cx, kk), "SKINNED BODY", { 96, 92 }, 26, TEAL, 0, 0.5f, true, 6);
    text(faded(cx, kk), "one baked mesh, the rig's own clips", { 96, 128 }, 26, DIM, 0, 0.5f);
    for (const Label& l : LABELS) {
        const float a = window(s, l.from, l.to);
        if (a <= 0.001f) continue;
        Ctx m = faded(cx, a);
        m.o.y += (1 - a) * 14 * m.s;
        text(m, l.name, { 96, 880 }, 96, INK, 0, 0.5f, true);
        dr::text(code::font(false), m, l.call, { 100, 960 }, 30, 0, TEAL, 0, 0.5f);
    }
    const float kx = window(s, XRAY_FROM + 0.1, XRAY_TO);
    if (kx > 0.001f) {
        const Vector2 p = screenOf(w, s, Vector3Transform({ 0, w.skin.build.thigh + w.skin.build.shin + 0.9f, 0 }, w.node(s)));
        text(faded(cx, kx), "the rig's joints, inside the mesh", { p.x + 110, p.y - 20 }, 30, TEAL, 0, 0.5f, true);
    }
}

void endCard(const Ctx& cx, int t) {
    const float kt = smooth(t / 14.0), ka = smooth((t - 24) / 14.0);
    Ctx tm = faded(cx, kt);
    tm.o.y += (1 - kt) * 20 * tm.s;
    const Color clear = withAlpha(PAPER, 0);
    dr::rectGrad4(faded(cx, kt), 0, 760, W, 100, clear, clear, PAPER, PAPER);
    dr::rect(faded(cx, kt), 0, 860, W, 220, PAPER);
    text(tm, "ONE MESH, ANY CLIP.", { W / 2, 900 }, 72, INK, 0.5f, 0.5f, true, 2);
    dr::text(code::font(false), faded(cx, ka), "tools/skin_bake.py  \xC2\xB7  rig::loadSkin  \xC2\xB7  rig::add(scene, ch, skin)",
             { W / 2, 980 }, 30, 0, TEAL, 0.5f, 0.5f);
}

// No mesh given: say how to make one.
void missing(Comp& c, const std::string& why) {
    addRoot(c.roots, 0, c.duration)->fn = [why](const Ctx& cx, int) {
        dr::rect(cx, 0, 0, W, H, PAPER);
        text(cx, "SKINNED BODY", { 160, 380 }, 30, TEAL, 0, 0.5f, true, 6);
        text(cx, "Give this composition a baked mesh.", { 160, 460 }, 64, INK, 0, 0.5f, true);
        dr::text(code::font(false), cx, "python tools/skin_bake.py body.glb out/body.glb", { 160, 570 }, 32, 0, INK, 0, 0.5f);
        dr::text(code::font(false), cx, "raymotion render skin-demo --props '{\"glb\": \"out/body.glb\"}'", { 160, 620 }, 32, 0, INK, 0, 0.5f);
        if (!why.empty()) text(cx, why, { 160, 720 }, 26, DIM, 0, 0.5f);
    };
}

void build(Comp& c, const Json& props) {
    c.roots.clear();
    const std::string glb = props.str("glb", "");
    std::shared_ptr<World> w;
    try {
        if (!glb.empty()) w = makeWorld(glb);
    } catch (const std::exception& e) {
        missing(c, e.what());
        return;
    }
    if (!w) { missing(c, ""); return; }
    const auto scene = makeScene(w);
    addRoot(c.roots, 0, TOTAL)->fn = [scene, w](const Ctx& cx, int t) {
        dr::rect(cx, 0, 0, W, H, PAPER);
        g3d::render(*scene, t, cx);
        overlays(cx, *w, t);
    };
    const int e = (int)(END_AT * FPS);
    addRoot(c.roots, e, TOTAL - e)->fn = [](const Ctx& cx, int t) { endCard(cx, t); };
}

} // namespace

void registerSkinDemo(Registry& out) {
    Comp c;
    c.name = "skin-demo";
    c.w = (int)W; c.h = (int)H; c.fps = FPS;
    c.duration = TOTAL;
    c.background = PAPER;
    c.defaultProps = Json::parse(R"({"glb": ""})");
    c.build = [](Comp& c, const Json& p) { build(c, p); };
    out.push_back(std::move(c));
}

} // namespace rmtn
