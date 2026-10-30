// rig_tests.cpp -- configurable characters: skeleton and poses, clips and motion, part
// locks and the catalog, limb curves, face timing and mouth shapes, outfits, swaps and the
// prefab (no window); then 3D and 2D renders, determinism and preview sheets (GL).
#include "rmtn/raymotion.hpp"
#include "../src/engine/rig_internal.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

template <class F> static bool throws(F f) {
    try { f(); } catch (const std::runtime_error&) { return true; }
    return false;
}
static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }
static bool near3(Vector3 a, Vector3 b, float eps = 1e-4f) { return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps); }
static bool same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

static void skeleton() {
    const rig::Build b = rig::Build::toon();
    const auto f = rig::solve(b, rig::Pose{});
    const Vector3 sh = rig::position(f, rig::Joint::ShoulderL), wr = rig::position(f, rig::Joint::WristL);
    CHECK(sh.x > 0 && rig::position(f, rig::Joint::ShoulderR).x < 0);          // left is +x
    CHECK(near3(wr, { sh.x, sh.y - b.upperArm - b.forearm, sh.z }));
    CHECK(near(rig::position(f, rig::Joint::AnkleL).y, b.ankle));
    CHECK(near(rig::position(f, rig::Joint::Head).y + b.head, b.height()));
    CHECK(rig::parent(rig::Joint::WristR) == rig::Joint::ElbowR && rig::parent(rig::Joint::Root) == rig::Joint::Root);
    CHECK(std::string(rig::name(rig::Joint::KneeL)) == "knee-L");
    for (const rig::Build& x : { rig::Build::adult(), rig::Build::toon(), rig::Build::block(), rig::Build::chibi() })
        CHECK(x.height() > 1.0f && x.height() < 2.0f);
    CHECK(near(rig::Build::adult().scaled(0.5f).height(), rig::Build::adult().height() * 0.5f));

    // One set of numbers poses either side: raising both arms sideways lifts both wrists outwards.
    rig::Pose p;
    p[rig::Joint::ShoulderL].z = p[rig::Joint::ShoulderR].z = 90;
    const auto g = rig::solve(b, p);
    const Vector3 wl = rig::position(g, rig::Joint::WristL), wrr = rig::position(g, rig::Joint::WristR);
    CHECK(wl.x > sh.x + 0.3f && wrr.x < -sh.x - 0.3f);
    CHECK(near(wl.y, wrr.y) && near(wl.x, -wrr.x));
    // x < 0 swings an arm forward.
    rig::Pose q;
    q[rig::Joint::ShoulderL].x = -90;
    CHECK(rig::position(rig::solve(b, q), rig::Joint::WristL).z > 0.3f);
    // A mirrored pose swaps the sides.
    const auto m = rig::solve(b, p.mirrored());
    CHECK(near3(rig::position(m, rig::Joint::WristL), wl));
    // Pure: the same input gives the same frames.
    const auto g2 = rig::solve(b, p);
    CHECK(std::memcmp(&g, &g2, sizeof(g)) == 0);
}

static void motion() {
    const rig::Build b = rig::Build::toon();
    const rig::Clip walk = rig::clips::walk(1);
    const rig::Pose a = walk(0.25), c = walk(0.75);
    CHECK(a[rig::Joint::HipL].x < -10 && a[rig::Joint::HipR].x > 10);          // legs opposite
    CHECK(c[rig::Joint::HipL].x > 10);
    CHECK(near(walk(0.1)[rig::Joint::HipL].x, walk(1.1)[rig::Joint::HipL].x, 1e-3f));   // cyclic
    CHECK(rig::walkSpeed(b) > 0.3f && rig::runSpeed(b) > rig::walkSpeed(b));
    // Waving raises the hand above the head.
    const auto f = rig::solve(b, rig::clips::wave(rig::Side::Right)(1.0));
    CHECK(rig::position(f, rig::Joint::WristR).y > rig::position(f, rig::Joint::Head).y);
    CHECK(rig::position(f, rig::Joint::WristL).y < rig::position(f, rig::Joint::Pelvis).y);
    for (const char* n : { "rest", "idle", "walk", "run", "wave", "wave-left", "point", "cheer", "jump", "sit", "shrug" })
        CHECK(!throws([n] { rig::clips::named(n)(0.5); }));
    CHECK(throws([] { rig::clips::named("moonwalk"); }));

    rig::Pose p0, p1;
    p1[rig::Joint::Chest].x = 20;
    rig::Motion mo;
    mo.play(rig::clips::hold(p0), 0, 0).play(rig::clips::hold(p1), 2, 1.0);
    CHECK(near(mo.at(1.9)[rig::Joint::Chest].x, 0));
    CHECK(near(mo.at(2.5)[rig::Joint::Chest].x, 10, 1e-3f));                   // halfway through the fade
    CHECK(near(mo.at(3.5)[rig::Joint::Chest].x, 20));
    rig::Pose up;
    up[rig::Joint::ShoulderL].z = 80;
    up[rig::Joint::HipL].x = -30;
    mo.layer(rig::clips::hold(up), 4, 6, rig::Mask::ArmL, 0.5);
    CHECK(near(mo.at(5)[rig::Joint::ShoulderL].z, 80) && near(mo.at(5)[rig::Joint::HipL].x, 0));
    CHECK(near(mo.at(7)[rig::Joint::ShoulderL].z, 0));
    const rig::Clip k = rig::clips::keys({ { 0, p0 }, { 1, p1 } }, true);
    CHECK(near(k(1.5)[rig::Joint::Chest].x, 10, 1e-3f));
}

static void partsAndLocks() {
    for (rig::Slot s : { rig::Slot::Head, rig::Slot::Hair, rig::Slot::Hat, rig::Slot::Torso, rig::Slot::Hips,
                         rig::Slot::Arm, rig::Slot::Hand, rig::Slot::Leg, rig::Slot::Foot })
        CHECK(rig::parts::names(s).size() >= 3);
    CHECK(rig::parts::has("arm.soft") && rig::parts::has("foot.bare") && !rig::parts::has("arm.tentacle"));
    CHECK(throws([] { rig::parts::get("arm.tentacle"); }));
    // Every part builds sensible shapes for every build and side.
    for (rig::Slot s : { rig::Slot::Head, rig::Slot::Hair, rig::Slot::Hat, rig::Slot::Torso, rig::Slot::Hips,
                         rig::Slot::Arm, rig::Slot::Hand, rig::Slot::Leg, rig::Slot::Foot, rig::Slot::Extra })
        for (const std::string& n : rig::parts::names(s))
            for (const rig::Build& b : { rig::Build::toon(), rig::Build::block(), rig::Build::adult() })
                for (rig::Side side : { rig::Side::Left, rig::Side::Right }) {
                    const auto v = rig::parts::get(n).make(b, side, 0.5f);
                    CHECK(!v.empty());
                    for (const rig::Prim& p : v) {
                        CHECK(p.size.x > 0 && p.size.y > 0 && p.size.z > 0);
                        CHECK(std::fabs(p.at.x) < 1 && std::fabs(p.at.y) < 1 && std::fabs(p.at.z) < 1);
                    }
                }
    // The left and right versions of a limb mirror each other.
    const auto l = rig::parts::get("arm.rigid").make(rig::Build::toon(), rig::Side::Left, 0);
    const auto r = rig::parts::get("arm.rigid").make(rig::Build::toon(), rig::Side::Right, 0);
    CHECK(l.size() == r.size() && l[0].joint == rig::Joint::ShoulderL && r[0].joint == rig::Joint::ShoulderR);
    CHECK(l[0].region == rig::Region::ArmL && r[0].region == rig::Region::ArmR);

    // A brick figure keeps its elbows bent and knees straight whatever the clip says.
    auto brick = std::make_shared<rig::Character>(rig::Look::block());
    brick->motion.play(rig::clips::run(), 0);
    for (double s : { 0.1, 0.4, 0.7 }) {
        const rig::Pose p = brick->poseAt(s);
        CHECK(near3(p[rig::Joint::ElbowL], { -35, 0, 0 }) && near3(p[rig::Joint::ElbowR], { -35, 0, 0 }));
        CHECK(near3(p[rig::Joint::KneeL], { 0, 0, 0 }) && near3(p[rig::Joint::KneeR], { 0, 0, 0 }));
    }
    CHECK(brick->poseAt(0.4)[rig::Joint::HipL].x != 0);                        // hips still swing
    rig::Look mixed = rig::Look::toon();
    mixed.armR = "arm.block";
    const rig::Pose lp = rig::locked(mixed, rig::clips::run()(0.3));
    CHECK(near3(lp[rig::Joint::ElbowR], { -35, 0, 0 }) && !near3(lp[rig::Joint::ElbowL], { -35, 0, 0 }));
    CHECK(rig::prims(mixed, "armR").size() == 2 && rig::prims(mixed, "armL").size() == 1);
    CHECK(rig::prims(mixed, "hat").empty());
    // Unknown parts throw when a character puts them on.
    rig::Look bad;
    bad.hat = "hat.wizard";
    CHECK(throws([&] { rig::Character c(bad); }));
}

static void limbs() {
    const rig::Build b = rig::Build::toon();
    rig::Pose p;
    p[rig::Joint::ElbowL].x = -90;
    const auto f = rig::solve(b, p);
    rig::Prim t = rig::parts::get("arm.soft").make(b, rig::Side::Left, 1)[0];
    for (float soft : { 0.f, 0.5f, 1.f }) {
        t.softness = soft;
        const auto c = rig::detail::tubeCurve(f, t, 24);
        CHECK(c.size() == 25);
        CHECK(near3(c.front(), rig::position(f, rig::Joint::ShoulderL)) && near3(c.back(), rig::position(f, rig::Joint::WristL)));
        float closest = 1e9f, len = 0;
        for (size_t i = 0; i < c.size(); i++) {
            closest = std::min(closest, Vector3Distance(c[i], rig::position(f, rig::Joint::ElbowL)));
            if (i) len += Vector3Distance(c[i - 1], c[i]);
        }
        CHECK(closest < 0.03f);                                                 // passes (near) the elbow
        CHECK(len > (b.upperArm + b.forearm) * 0.8f && len <= (b.upperArm + b.forearm) * 1.25f);
    }
    // Boxy shapes sample their faces evenly: the front quarter of longitudes lands on the front face.
    rig::Prim box;
    box.size = { 0.4f, 0.4f, 0.2f };
    box.squareV = box.squareH = 0.1f;
    const Vector3 frontPt = rig::detail::superPoint(box, 0, PI / 2 + 0.3f);
    CHECK(near(frontPt.z, 0.1f, 0.01f) && frontPt.x < -0.03f);
}

static void faces() {
    const auto cues = rig::visemes("mama, oh", 1, 2);
    CHECK(cues.front().first == 1 && cues.front().second == 'A');
    CHECK(cues.back().first == 2 && cues.back().second == 'X');
    CHECK(std::is_sorted(cues.begin(), cues.end(), [](auto& a, auto& b) { return a.first < b.first; }));
    bool sawD = false, sawE = false;
    for (auto& [t, m] : cues) { sawD |= m == 'D'; sawE |= m == 'E'; }
    CHECK(sawD && sawE);

    rig::FaceTrack ft;
    ft.expression(1, "happy", 0.5).look(2, { 1, 0 }, 0).say(3, 4, "hello");
    CHECK(ft.at(0.5).expression == "neutral");
    const rig::FaceState mid = ft.at(1.25);
    CHECK(mid.expression == "happy" && mid.previous == "neutral" && mid.mix > 0.2f && mid.mix < 0.8f);
    CHECK(near(mid.weight("happy") + mid.weight("neutral"), 1));
    CHECK(near(ft.at(2.5).look.x, 1));
    CHECK(ft.at(3.3).mouth != 'X' && ft.at(4.5).mouth == 'X');
    // Blinks come by themselves, the same every time.
    int shut = 0;
    for (int i = 0; i < 300; i++) if (ft.at(i / 30.0).blink > 0.9f) shut++;
    CHECK(shut >= 1);
    CHECK(near(ft.at(1.234).blink, ft.at(1.234).blink));
    rig::FaceTrack still;
    still.autoBlink = false;
    still.blink(1);
    CHECK(still.at(0.5).blink == 0 && still.at(1.07).blink > 0.9f);

    const char* svg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
        <ellipse id="eye-L" cx="65" cy="40" rx="8" ry="10" fill="#222"/>
        <ellipse id="eye-R" cx="35" cy="40" rx="8" ry="10" fill="#222"/>
        <path id="mouth" d="M35 70 Q50 78 65 70" stroke="#222" stroke-width="3" fill="none"/>
        <path id="mouth-D" d="M38 66 Q50 90 62 66 Z" fill="#622"/>
        <circle id="blush-happy" cx="25" cy="60" r="6" fill="#f99"/></svg>)";
    CHECK(!throws([&] { rig::svgFaceFromText(svg); }));
    CHECK(throws([] { rig::svgFace("no/such/face.svg"); }));
}

static void outfits() {
    const Color top{ 200, 30, 30, 255 }, bottom{ 20, 30, 120, 255 }, skin{ 230, 180, 140, 255 };
    const rig::Outfit o = rig::Outfit::basic(top, bottom, skin, 0.5f, 0.8f);
    CHECK(same(o.sample(rig::Region::ArmL, 0.5f, 0.2f), top) && same(o.sample(rig::Region::ArmR, 0.5f, 0.9f), skin));
    CHECK(same(o.sample(rig::Region::LegL, 0.5f, 0.5f), bottom) && same(o.sample(rig::Region::LegR, 0.5f, 0.95f), skin));
    CHECK(same(o.sample(rig::Region::Torso, 0.1f, 0.1f), top) && same(o.sample(rig::Region::Hips, 0.5f, 0.5f), bottom));
    // The strips tile the texture without overlapping.
    float area = 0;
    for (int i = 0; i < (int)rig::Region::Count; i++) {
        const Rectangle r = rig::region((rig::Region)i);
        area += r.width * r.height;
        CHECK(r.x >= 0 && r.y >= 0 && r.x + r.width <= 1.0001f && r.y + r.height <= 1.0001f);
    }
    CHECK(near(area, 1, 1e-4f));
    CHECK(near(rig::front(rig::Region::Torso).width, 0.25f));
    rig::Outfit printed = o;
    printed.over([](const Ctx&, int, float) {});
    CHECK(printed.id != o.id && o.id != 0);
}

static void swaps() {
    rig::Look a = rig::Look::toon(), b = a;
    b.hair = "hair.bun";
    b.armR = "arm.robot";
    rig::Character ch(a);
    ch.wear(2, b, 0.5);
    CHECK(ch.lookIndex("hair", 1.9) == 0 && ch.lookIndex("hair", 2.1) == 1);
    CHECK(ch.lookIndex("armL", 3) == 0 && ch.lookIndex("armR", 3) == 1);       // unchanged parts stay put
    CHECK(ch.popAt("hair", 2.0) <= 0.001f && ch.popAt("hair", 2.4) > 1.0f && near(ch.popAt("hair", 3), 1));
    CHECK(near(ch.popAt("armL", 2.2), 1));
    CHECK(ch.lookAt(2.5).hair == "hair.bun");
    // A new outfit swaps every part that shows it, and only those.
    rig::Look c = b;
    c.outfit = rig::Outfit::basic(RED, GREEN);
    ch.wear(4, c, 0.3);
    CHECK(ch.lookIndex("torso", 4.5) == 2 && ch.lookIndex("hips", 4.5) == 2 && ch.lookIndex("hair", 4.5) == 1);
    CHECK(ch.lookIndex("armR", 4.5) == 1);                                      // the robot arm wears no fabric
}

static void prefabs() {
    rig::registerPrefabs();
    const auto names = prefab::names();
    CHECK(std::find(names.begin(), names.end(), "character") != names.end());
    Comp c;
    c.w = 640; c.h = 360;
    CHECK(!throws([&] { prefab::use(c.roots, 0, 30, "character", Json::parse(R"({"preset": "block", "arms": "arm.soft"})")); }));
    CHECK(throws([&] { prefab::use(c.roots, 0, 30, "character", Json::parse(R"({"preset": "pirate"})")); }));
    CHECK(throws([&] { prefab::use(c.roots, 0, 30, "character", Json::parse(R"({"hat": "hat.wizard"})")); }));
    CHECK(throws([&] { prefab::use(c.roots, 0, 30, "character", Json::parse(R"({"clip": "moonwalk"})")); }));
}

// ------------------------------------------------------------------ GL
static Image frame(Comp& c, int t) {
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, t, canvas);
    Image image = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&image);
    canvas.unload();
    return image;
}

static int differing(const Image& img, Color bg) {
    const Color* px = (const Color*)img.data;
    int n = 0;
    for (int i = 0; i < img.width * img.height; i++)
        if (std::abs(px[i].r - bg.r) + std::abs(px[i].g - bg.g) + std::abs(px[i].b - bg.b) > 30) n++;
    return n;
}

static std::shared_ptr<g3d::Scene> studio(float h) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->camera.position = { 0, h * 0.62f, h * 3.2f };
    scene->camera.target = { 0, h * 0.5f, 0 };
    scene->camera.fov = 30;
    scene->ambient = 0.55f;
    scene->environmentGenerator = g3d::studioEnvironment({ 236, 240, 246, 255 }, { 170, 182, 196, 255 }, { 60, 62, 70, 255 }, 3);
    scene->environmentIntensity = 0.7f;
    scene->lights.push_back(g3d::Light::directional({ -2, 5, 4 }, { 255, 246, 232, 255 }, 1.6f, true));
    scene->lights.push_back(g3d::Light::directional({ 3, 3, -3 }, { 170, 200, 255, 255 }, 0.6f));
    g3d::Material floor;
    floor.color = { 210, 214, 220, 255 };
    floor.roughness = 0.9f;
    scene->add(g3d::Geometry::plane(8, 8), floor);
    return scene;
}

static rig::CharacterPtr cast(const rig::Look& look, const char* clip, double t0 = 0) {
    auto ch = std::make_shared<rig::Character>(look);
    ch->motion.play(rig::clips::named(clip), t0);
    ch->face.expression(0.2, "happy", 0.2).say(0.5, 1.4, "hello there");
    return ch;
}

static bool render(const std::filesystem::path& dir, bool preview) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(640, 360, "rig tests");
    if (!IsWindowReady()) return false;
    const Color bg{ 32, 36, 44, 255 };
    if (preview) std::filesystem::create_directories(dir);

    // 3D: each preset, posed mid-wave, mid-word.
    const std::pair<const char*, rig::Look> casting[] = {
        { "toon", rig::Look::toon() }, { "block", rig::Look::block() }, { "avatar", rig::Look::avatar() }, { "robot", rig::Look::robot() } };
    for (const auto& [name, look] : casting) {
        Comp c;
        c.w = 480; c.h = 540; c.fps = 30; c.duration = 60; c.background = bg;
        auto scene = studio(look.build.height());
        rig::add(*scene, cast(look, "wave"))->rotation.y = 20;
        addRoot(c.roots, 0, 60)->fn = g3d::draw(scene);
        Image a = frame(c, 30), b = frame(c, 30);
        CHECK(differing(a, bg) > 480 * 540 / 12);
        CHECK(std::memcmp(a.data, b.data, (size_t)a.width * a.height * 4) == 0);   // same frame, same pixels
        if (preview) ExportImage(a, (dir / (std::string("3d-") + name + ".png")).string().c_str());
        UnloadImage(a);
        UnloadImage(b);
    }

    // Soft versus rigid: the same bent arm.
    {
        Comp c;
        c.w = 960; c.h = 540; c.fps = 30; c.duration = 30; c.background = bg;
        auto scene = studio(1.6f);
        scene->camera.position = { 0, 1.0f, 5.2f };
        scene->camera.target = { 0, 0.75f, 0 };
        const char* arms[] = { "arm.rigid", "arm.soft", "arm.hose" };
        const float softness[] = { 0, 0.6f, 1 };
        for (int i = 0; i < 3; i++) {
            rig::Look l = rig::Look::toon();
            l.arms(arms[i]).legs(i == 0 ? "leg.rigid" : "leg.soft");
            l.softness = softness[i];
            l.hands(i == 2 ? "hand.glove" : "hand.mitten");
            rig::Pose p;
            p[rig::Joint::ShoulderL] = { -30, 0, 50 };
            p[rig::Joint::ElbowL] = { -100, 0, 0 };
            p[rig::Joint::ShoulderR] = { -70, 0, 15 };
            p[rig::Joint::ElbowR] = { -60, 0, 0 };
            p[rig::Joint::HipL].x = -40;
            p[rig::Joint::KneeL].x = 70;
            auto ch = std::make_shared<rig::Character>(l);
            ch->motion.play(rig::clips::hold(p), 0);
            g3d::Object3D* root = rig::add(*scene, ch);
            root->position.x = (i - 1) * 1.35f;
            root->rotation.y = 35;
        }
        addRoot(c.roots, 0, 30)->fn = g3d::draw(scene);
        Image a = frame(c, 0);
        CHECK(differing(a, bg) > 960 * 540 / 15);
        if (preview) ExportImage(a, (dir / "3d-soft-vs-rigid.png").string().c_str());
        UnloadImage(a);
    }

    // Parts sheet: hands and feet close up.
    {
        Comp c;
        c.w = 960; c.h = 540; c.fps = 30; c.duration = 30; c.background = bg;
        auto scene = studio(1.6f);
        const char* hands[] = { "hand.mitten", "hand.fingers", "hand.clip", "hand.claw", "hand.glove" };
        const char* feet[] = { "foot.shoe", "foot.bare", "foot.boot", "foot.block", "foot.flipper" };
        scene->camera.position = { 0, 0.7f, 3.4f };
        scene->camera.target = { 0, 0.42f, 0 };
        for (int i = 0; i < 5; i++) {
            rig::Look l = rig::Look::toon();
            l.hands(hands[i]).feet(feet[i]);
            rig::Pose p;
            p[rig::Joint::ShoulderL] = { -35, 0, 12 };
            p[rig::Joint::ShoulderR] = { -35, 0, 12 };
            p[rig::Joint::ElbowL] = p[rig::Joint::ElbowR] = { -50, 0, 0 };
            auto ch = std::make_shared<rig::Character>(l);
            ch->motion.play(rig::clips::hold(p), 0);
            g3d::Object3D* root = rig::add(*scene, ch);
            root->position.x = (i - 2) * 0.72f;
            root->rotation.y = 30;
        }
        addRoot(c.roots, 0, 30)->fn = g3d::draw(scene);
        Image a = frame(c, 0);
        if (preview) ExportImage(a, (dir / "3d-hands-feet.png").string().c_str());
        UnloadImage(a);
    }

    // Swaps: a part pops in, the others stay.
    {
        rig::Look a = rig::Look::toon(), b = a;
        b.hair = "hair.spiky"; b.hat = "hat.cap";
        auto ch = std::make_shared<rig::Character>(a);
        ch->wear(1.0, b, 0.4);
        ch->motion.play(rig::clips::idle(), 0);
        Comp c;
        c.w = 480; c.h = 540; c.fps = 30; c.duration = 60; c.background = bg;
        auto scene = studio(a.build.height());
        rig::add(*scene, ch);
        addRoot(c.roots, 0, 60)->fn = g3d::draw(scene);
        Image before = frame(c, 20), after = frame(c, 50);
        CHECK(std::memcmp(before.data, after.data, (size_t)before.width * before.height * 4) != 0);
        if (preview) ExportImage(after, (dir / "3d-swapped.png").string().c_str());
        UnloadImage(before);
        UnloadImage(after);
    }

    // An SVG outfit and an SVG face on the 3D body.
    {
        rig::Look l = rig::Look::toon();
        l.outfit = rig::Outfit::svg("characters/outfit-sailor.svg");
        l.face = rig::svgFace("characters/face-pip.svg");
        l.hair = "hair.ponytail";
        auto ch = cast(l, "idle");
        Comp c;
        c.w = 480; c.h = 540; c.fps = 30; c.duration = 60; c.background = bg;
        auto scene = studio(l.build.height());
        rig::add(*scene, ch)->rotation.y = 25;
        addRoot(c.roots, 0, 60)->fn = g3d::draw(scene);
        Image a = frame(c, 20);
        CHECK(differing(a, bg) > 480 * 540 / 12);
        if (preview) ExportImage(a, (dir / "3d-svg-outfit-face.png").string().c_str());
        UnloadImage(a);
    }

    // 2D: a line-up of puppets at several yaws.
    {
        Comp c;
        c.w = 1280; c.h = 540; c.fps = 30; c.duration = 60; c.background = { 238, 234, 224, 255 };
        const std::pair<rig::Look, float> line[] = { { rig::Look::toon(), 0 }, { rig::Look::block(), 25 },
                                                     { rig::Look::avatar(), 60 }, { rig::Look::robot(), -30 }, { rig::Look::toon(), 90 } };
        float x = 140;
        for (const auto& [look, yaw] : line) {
            auto ch = cast(look, yaw == 90 ? "walk" : "wave");
            rig::View2D v;
            v.yaw = yaw; v.scale = 280; v.origin = { x, 500 };
            addRoot(c.roots, 0, 60)->fn = [ch, v](const Ctx& cx, int t) { rig::draw(cx, *ch, t / 30.0, v); };
            x += 250;
        }
        Image a = frame(c, 30), b = frame(c, 30);
        CHECK(differing(a, c.background) > 1280 * 540 / 14);
        CHECK(std::memcmp(a.data, b.data, (size_t)a.width * a.height * 4) == 0);
        if (preview) ExportImage(a, (dir / "2d-lineup.png").string().c_str());
        UnloadImage(a);
        UnloadImage(b);
    }

    // Faces: every expression and mouth shape, flat.
    {
        Comp c;
        c.w = 1280; c.h = 600; c.fps = 30; c.duration = 1; c.background = { 222, 170, 130, 255 };
        addRoot(c.roots, 0, 1)->fn = [](const Ctx& cx, int) {
            const rig::FacePtr cartoon = rig::simpleFace(), dots = rig::simpleFace([] { rig::FaceStyle s; s.eyes = rig::FaceStyle::Eyes::Dots; return s; }());
            const char* ex[] = { "neutral", "happy", "sad", "angry", "surprised", "wink", "sleepy", "smug" };
            for (int i = 0; i < 8; i++) {
                rig::FaceState s;
                s.expression = s.previous = ex[i];
                cartoon->draw(cx, { 10.f + i * 158, 10, 150, 180 }, s);
            }
            const char mouths[] = "ABCDEFGHX";
            for (int i = 0; i < 9; i++) {
                rig::FaceState s;
                s.mouth = s.previousMouth = mouths[i];
                dots->draw(cx, { 10.f + i * 140, 210, 130, 170 }, s);
            }
            // The SVG face: expressions, a half blink, a glance, a mouth shape mid-morph.
            const rig::FacePtr pip = rig::svgFace("characters/face-pip.svg");
            for (int i = 0; i < 8; i++) {
                rig::FaceState s;
                s.expression = s.previous = ex[i];
                if (i == 0) s.blink = 0.5f;
                if (i == 3) s.look = { 1, 0 };
                if (i == 7) { s.previousMouth = 'A'; s.mouth = 'D'; s.mouthMix = 0.5f; }
                pip->draw(cx, { 10.f + i * 158, 410, 150, 120 }, s);
            }
        };
        Image a = frame(c, 0);
        CHECK(differing(a, c.background) > 1280 * 600 / 40);
        if (preview) ExportImage(a, (dir / "faces.png").string().c_str());
        UnloadImage(a);
    }

    // Canvas textures: drawn once per frame, upright, straight alpha.
    {
        Comp c;
        c.w = 320; c.h = 320; c.fps = 30; c.duration = 1; c.background = BLACK;
        auto scene = std::make_shared<g3d::Scene>();
        scene->camera.orthographic = true;
        scene->camera.orthoHeight = 2;
        scene->camera.position = { 0, 0, 5 };
        auto tex = std::make_shared<g3d::DynamicTexture>();
        tex->width = tex->height = 64;
        tex->draw = [](const Ctx& cx, int) {
            vg::fill(cx, vg::Path::rect({ 0, 0, 64, 32 }), RED);      // top half red
            vg::fill(cx, vg::Path::rect({ 0, 32, 64, 32 }), BLUE);    // bottom half blue
        };
        g3d::Material m;
        m.unlit = true;
        m.dynamicMap = tex;
        auto* quad = scene->add(g3d::Geometry::plane(2, 2), m);
        quad->rotation.x = 90;                                         // faces +z, texture v down = -y
        addRoot(c.roots, 0, 1)->fn = g3d::draw(scene);
        Image a = frame(c, 0);
        const Color top = GetImageColor(a, 160, 60), bottom = GetImageColor(a, 160, 260);
        CHECK(top.r > 200 && top.b < 60);
        CHECK(bottom.b > 200 && bottom.r < 60);
        UnloadImage(a);
    }
    shutdownGraphics();
    CloseWindow();
    return true;
}

int main(int argc, char** argv) try {
    skeleton();
    motion();
    partsAndLocks();
    limbs();
    faces();
    outfits();
    swaps();
    prefabs();
    const bool preview = argc == 3 && std::string(argv[1]) == "--preview";
    if (!(argc == 2 && std::string(argv[1]) == "--no-gl")) CHECK(render(preview ? argv[2] : "", preview));
    if (failures) std::fprintf(stderr, "%d failure(s)\n", failures);
    else std::printf("rig tests passed\n");
    return failures ? 1 : 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "uncaught: %s\n", e.what());
    return 2;
}
