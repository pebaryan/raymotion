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
                    auto v = rig::parts::get(n).make(b, side, 0.5f);
                    // Animal parts draw nothing on a humanoid; on an animal's build they do.
                    if (v.empty()) v = rig::parts::get(n).make(rig::quadruped(rig::QuadSpec::dog()), side, 0.5f);
                    if (v.empty()) v = rig::parts::get(n).make(rig::biped(rig::BipedSpec::chicken()), side, 0.5f);
                    if (v.empty()) v = rig::parts::get(n).make(rig::chain(rig::ChainSpec::fish()), side, 0.5f);
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
// Reaching and acting: IK lands hands and feet where asked, on either side; the clips
// put hands on keyboards, bars and poles, and feet on the floor, decks and boards.
static void acting() {
    using rig::Joint;
    using rig::Side;
    for (const rig::Build& b : { rig::Build::adult(), rig::Build::toon(), rig::Build::block() }) {
        const float arm = b.upperArm + b.forearm;
        for (Side sd : { Side::Left, Side::Right }) {
            const Joint wrist = sd == Side::Left ? Joint::WristL : Joint::WristR;
            const Joint elbow = sd == Side::Left ? Joint::ElbowL : Joint::ElbowR;
            const Joint shoulder = sd == Side::Left ? Joint::ShoulderL : Joint::ShoulderR;
            const float sx = sd == Side::Left ? 1.f : -1.f;
            for (int i = 0; i < 24; i++) {
                const Vector3 S = rig::position(rig::solve(b, rig::Pose{}), shoulder);
                // Targets all round the shoulder, within reach.
                const Vector3 dir = Vector3Normalize({ (float)rrange(3, i, -1, 1), (float)rrange(5, i, -1, 1), (float)rrange(7, i, -0.3, 1) });
                const Vector3 target = Vector3Add(S, Vector3Scale(dir, arm * (float)rrange(11, i, 0.3, 0.95)));
                const rig::Pose p = rig::reach(b, rig::Pose{}, sd, target);
                const auto f = rig::solve(b, p);
                CHECK(Vector3Distance(rig::position(f, wrist), target) < 2e-3f);
                // The elbow bends forward-only (x < 0) and juts out towards the pole's side.
                CHECK(p[elbow].x <= 0);
                const Vector3 mid = Vector3Lerp(S, target, 0.5f), e = rig::position(f, elbow);
                CHECK(Vector3DotProduct(Vector3Subtract(e, mid), { sx * 0.5f, -0.4f, -1 }) > -1e-3f);
            }
            // Out of reach: the arm points straight at the target.
            const Vector3 S = rig::position(rig::solve(b, rig::Pose{}), shoulder);
            const Vector3 far = Vector3Add(S, { sx * 3, 1, 2 });
            const auto f = rig::solve(b, rig::reach(b, rig::Pose{}, sd, far));
            const Vector3 w = rig::position(f, wrist);
            CHECK(near(Vector3Distance(S, w), arm, 2e-3f));
            CHECK(Vector3DotProduct(Vector3Normalize(Vector3Subtract(w, S)), Vector3Normalize(Vector3Subtract(far, S))) > 0.999f);
            // Legs: the ankle lands, the knee bends back-only (x > 0).
            const Vector3 foot{ sx * 0.15f, b.ankle + 0.1f, 0.2f };
            const rig::Pose q = rig::plant(b, rig::Pose{}, sd, foot);
            CHECK(Vector3Distance(rig::position(rig::solve(b, q), sd == Side::Left ? Joint::AnkleL : Joint::AnkleR), foot) < 2e-3f);
            CHECK(q[sd == Side::Left ? Joint::KneeL : Joint::KneeR].x >= 0);
            // A hand: palm centre on the spot, fingers and palm facing as asked.
            const Vector3 at{ sx * 0.2f, b.height() * 0.55f, 0.3f };
            const rig::Pose h = rig::hand(b, rig::Pose{}, sd, at, { 0, 0.3f, 1 }, { -sx, 0, 0 });
            const auto fh = rig::solve(b, h);
            CHECK(Vector3Distance(rig::palmAt(b, h, sd), at) < 2e-3f);
            const Matrix& W = fh[(size_t)wrist];
            const Vector3 o = Vector3Transform({ 0, 0, 0 }, W);
            const Vector3 fingers = Vector3Subtract(Vector3Transform({ 0, -1, 0 }, W), o);
            const Vector3 palm = Vector3Subtract(Vector3Transform({ -sx, 0, 0 }, W), o);
            CHECK(Vector3DotProduct(fingers, Vector3Normalize({ 0, 0.3f, 1 })) > 0.999f);
            CHECK(Vector3DotProduct(palm, { -sx, 0, 0 }) > 0.95f);
        }
        // orient: a joint's axes where asked, whatever its parents do.
        rig::Pose bent;
        bent[Joint::Chest] = { 20, 30, -10 };
        bent[Joint::Neck] = { -15, 10, 5 };
        const rig::Pose o = rig::orient(b, bent, Joint::Head, { 0, 0, 1 }, { 0, -1, 0 });
        const Matrix& H = rig::solve(b, o)[(size_t)Joint::Head];
        const Vector3 hc = Vector3Transform({ 0, 0, 0 }, H);
        CHECK(near3(Vector3Subtract(Vector3Transform({ 0, 1, 0 }, H), hc), { 0, 0, 1 }, 1e-3f));
        CHECK(near3(Vector3Subtract(Vector3Transform({ 0, 0, 1 }, H), hc), { 0, -1, 0 }, 1e-3f));

        // Floor stays on the ground when the body bobs.
        rig::Pose bob;
        bob.offset = { 0, -0.2f, 0.1f };
        CHECK(near3(rig::position(rig::solve(b, bob), Joint::Floor), { 0, 0, 0 }));

        // Seated: the hips just over the seat; feet planted when the shins reach the floor.
        const float lowSeat = b.shin + b.ankle - b.legRadius - 0.05f, highSeat = 0.62f;
        for (float seat : { lowSeat, highSeat }) {
            const auto f = rig::solve(b, rig::clips::seated(b, seat)(1.0));
            CHECK(near(rig::position(f, Joint::HipL).y, seat + b.legRadius * 0.9f, 0.01f));
            if (seat == lowSeat) CHECK(near(rig::position(f, Joint::AnkleL).y, b.ankle, 2e-3f));
        }
        // Typing: palms over the keyboard.
        const Vector3 keys{ 0, highSeat + 0.22f, 0.32f };
        for (double t : { 0.0, 0.7, 1.9 }) {
            const rig::Pose p = rig::clips::typing(b, highSeat, keys)(t);
            for (Side sd : { Side::Left, Side::Right })
                CHECK(Vector3Distance(rig::palmAt(b, p, sd), keys) < b.torso.x * 0.3f + b.hand + 0.06f);
        }
        // Scooter: the left foot on the deck, hands on the bar, at every phase of a kick.
        const auto sg = rig::detail::scooterGeom(b);
        for (double t : { 0.0, 0.3, 0.6, 1.2 }) {
            const rig::Pose p = rig::clips::scooter(b)(t);
            const auto f = rig::solve(b, p);
            CHECK(near(rig::position(f, Joint::AnkleL).y, sg.top + b.ankle, 3e-3f));
            for (Side sd : { Side::Left, Side::Right })
                CHECK(std::fabs(rig::palmAt(b, p, sd).y - (sg.barY + b.hand * 0.3f)) < 0.02f);
        }
        // Skateboard: the front foot stays on the board through a push; the back foot touches
        // the ground mid-push and is back on the board after it.
        const auto bg = rig::detail::boardGeom(b);
        const rig::Clip sk = rig::clips::skate(b, 2.4f);
        CHECK(near(rig::position(rig::solve(b, sk(0.6)), Joint::AnkleL).y, bg.top + b.ankle, 3e-3f));
        CHECK(near(rig::position(rig::solve(b, sk(0.6)), Joint::AnkleR).y, b.ankle, 3e-3f));
        CHECK(near(rig::position(rig::solve(b, sk(2.0)), Joint::AnkleR).y, bg.top + b.ankle, 3e-3f));
        // Placard: the card clears the head.
        const auto g = rig::detail::signGeom(b);
        CHECK(g.hand + g.pole - g.grip - g.height > b.height());
    }
    // Props and rides are parts like any other; labels take the look's sign.
    for (const char* name : { "prop.phone", "prop.tablet", "prop.sign", "prop.shield", "prop.baton" })
        CHECK(rig::parts::has(name) && rig::parts::get(name).slot == rig::Slot::Hold);
    for (const char* name : { "ride.skateboard", "ride.scooter" })
        CHECK(rig::parts::has(name) && rig::parts::get(name).slot == rig::Slot::Ride);
    CHECK(rig::parts::has("hat.police") && rig::parts::has("hat.riot"));
    rig::Look l;
    l.holdR = "prop.sign";
    l.sign = "NO TO\nCARS";
    bool labelled = false;
    for (const rig::Prim& p : rig::prims(l, "holdR"))
        if (p.paint == rig::Paint::Label) labelled = p.label == "NO TO\nCARS";
    CHECK(labelled);
    l.ride = "ride.scooter";
    for (const rig::Prim& p : rig::prims(l, "ride")) CHECK(p.joint == Joint::Floor);
    // A sign change is a swap (the label is part of what the slot shows).
    auto ch = std::make_shared<rig::Character>(l);
    rig::Look l2 = l;
    l2.sign = "YES";
    ch->wear(1.0, l2);
    CHECK(ch->lookIndex("holdR", 1.5) == 1);
}

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

    // Acting with furniture and props: an office (typing at a desk, a phone, a tablet) and a
    // street (a placard, a riot line, a skateboard, a scooter), in 3D and as 2D cut-outs.
    {
        Comp c;
        c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90; c.background = bg;
        auto scene = studio(1.8f);
        scene->camera.position = { 1.6f, 2.0f, 4.6f };
        scene->camera.target = { 0.2f, 0.85f, 0 };
        const rig::Build adult = rig::Build::adult();
        // The desk, a workstation on it, a chair pulled up to it.
        interior3d::add(*scene, interior3d::Kind::OfficeDesk)->position = { -0.6f, 0, -0.35f };
        interior3d::add(*scene, interior3d::Kind::Workstation)->position = { -0.6f, 0.7825f, -0.42f };
        // The chair turned to face the desk (its front is +z).
        g3d::Object3D* chair = interior3d::add(*scene, interior3d::Kind::ErgonomicChair);
        chair->position = { -0.46f, 0, 0.3f };
        chair->rotation.y = 180;
        rig::Look worker = rig::Look::toon();
        worker.build = adult;
        worker.outfit = rig::Outfit::basic({ 230, 120, 70, 255 }, { 50, 60, 100, 255 }, worker.skin, 0.9f, 1);
        auto typist = std::make_shared<rig::Character>(worker);
        // The seat top is 0.61 m up. The keyboard (workstation-local 0.14, 0.035, 0.32) in the
        // typist's space: it sits where the chair is, turned to face -z.
        const Vector3 kw{ -0.6f + 0.14f, 0.7825f + 0.035f, -0.42f + 0.32f };
        const Vector3 seat{ -0.46f, 0, 0.3f };
        typist->motion.play(rig::clips::typing(adult, 0.61f, { -(kw.x - seat.x), kw.y, -(kw.z - seat.z) }), 0);
        g3d::Object3D* t = rig::add(*scene, typist);
        t->position = seat;
        t->rotation.y = 180;
        rig::Look caller = worker;
        caller.outfit = rig::Outfit::basic({ 80, 150, 120, 255 }, { 40, 40, 50, 255 }, caller.skin, 0.4f, 1);
        caller.hair = "hair.bob";
        caller.holdR = "prop.phone";
        auto phone = std::make_shared<rig::Character>(caller);
        phone->motion.play(rig::clips::texting(adult), 0);
        g3d::Object3D* ph = rig::add(*scene, phone);
        ph->position = { 0.75f, 0, 0.2f };
        ph->rotation.y = -20;
        rig::Look reader = rig::Look::toon();
        reader.holdL = "prop.tablet";
        auto tab = std::make_shared<rig::Character>(reader);
        tab->motion.play(rig::clips::tablet(reader.build), 0);
        g3d::Object3D* tb = rig::add(*scene, tab);
        tb->position = { 1.55f, 0, -0.2f };
        tb->rotation.y = -35;
        addRoot(c.roots, 0, 90)->fn = g3d::draw(scene);
        Image a = frame(c, 45), b2 = frame(c, 45);
        CHECK(std::memcmp(a.data, b2.data, (size_t)a.width * a.height * 4) == 0);
        if (preview) ExportImage(a, (dir / "3d-office.png").string().c_str());
        UnloadImage(a);
        UnloadImage(b2);
    }
    {
        Comp c;
        c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90; c.background = bg;
        auto scene = studio(1.8f);
        scene->camera.position = { 0.4f, 1.9f, 6.4f };
        scene->camera.target = { 0.4f, 0.95f, 0 };
        const rig::Build adult = rig::Build::adult();
        rig::Look marcher = rig::Look::toon();
        marcher.build = adult;
        marcher.holdR = "prop.sign";
        marcher.sign = "MORE\nBIKE LANES";
        marcher.outfit = rig::Outfit::basic({ 240, 196, 60, 255 }, { 60, 70, 90, 255 }, marcher.skin, 0.45f, 1);
        auto m = std::make_shared<rig::Character>(marcher);
        m->motion.play(rig::clips::protest(adult), 0);
        rig::add(*scene, m)->position = { -1.7f, 0, 0 };
        rig::Look cop = rig::Look::toon();
        cop.build = adult;
        cop.hat = "hat.riot";
        cop.hair = "";
        cop.holdL = "prop.shield";
        cop.holdR = "prop.baton";
        cop.sign = "POLICE";
        cop.dark = { 30, 36, 52, 255 };
        cop.outfit = rig::Outfit::basic({ 36, 44, 64, 255 }, { 30, 34, 46, 255 }, cop.skin, 1, 1);
        auto po = std::make_shared<rig::Character>(cop);
        po->motion.play(rig::clips::shieldLine(adult), 0);
        rig::add(*scene, po)->position = { -0.4f, 0, 0 };
        rig::Look skater = rig::Look::toon();
        skater.ride = "ride.skateboard";
        skater.hat = "hat.beanie";
        auto sk = std::make_shared<rig::Character>(skater);
        sk->motion.play(rig::clips::skate(skater.build), 0);
        rig::add(*scene, sk)->position = { 1.0f, 0, 0.2f };
        rig::Look rider = rig::Look::toon();
        rider.ride = "ride.scooter";
        rider.hair = "hair.ponytail";
        auto sc = std::make_shared<rig::Character>(rider);
        sc->motion.play(rig::clips::scooter(rider.build), 0);
        g3d::Object3D* s3 = rig::add(*scene, sc);
        s3->position = { 2.3f, 0, 0 };
        s3->rotation.y = -60;
        addRoot(c.roots, 0, 90)->fn = g3d::draw(scene);
        Image a = frame(c, 20);
        if (preview) ExportImage(a, (dir / "3d-street.png").string().c_str());
        UnloadImage(a);
    }
    {
        Comp c;
        c.w = 1600; c.h = 600; c.fps = 30; c.duration = 90; c.background = { 238, 234, 224, 255 };
        struct Act { rig::Look look; rig::Clip clip; float yaw; };
        std::vector<Act> acts;
        rig::Look a1 = rig::Look::toon(); a1.holdR = "prop.phone";
        acts.push_back({ a1, rig::clips::texting(a1.build), 20 });
        rig::Look a2 = rig::Look::toon(); a2.holdL = "prop.tablet"; a2.hair = "hair.bob";
        acts.push_back({ a2, rig::clips::tablet(a2.build), -20 });
        rig::Look a3 = rig::Look::toon(); a3.holdR = "prop.sign"; a3.sign = "CLIMATE\nNOW";
        acts.push_back({ a3, rig::clips::protest(a3.build), 10 });
        rig::Look a4 = rig::Look::toon(); a4.hat = "hat.riot"; a4.hair = ""; a4.holdL = "prop.shield"; a4.holdR = "prop.baton";
        a4.sign = "POLICE"; a4.outfit = rig::Outfit::basic({ 36, 44, 64, 255 }, { 30, 34, 46, 255 }, a4.skin, 1, 1);
        acts.push_back({ a4, rig::clips::shieldLine(a4.build), 25 });
        rig::Look a5 = rig::Look::toon(); a5.ride = "ride.skateboard"; a5.hat = "hat.beanie";
        acts.push_back({ a5, rig::clips::skate(a5.build), 0 });
        rig::Look a6 = rig::Look::toon(); a6.ride = "ride.scooter";
        acts.push_back({ a6, rig::clips::scooter(a6.build), 70 });
        float x = 130;
        for (const Act& act : acts) {
            auto ch = std::make_shared<rig::Character>(act.look);
            ch->motion.play(act.clip, 0);
            rig::View2D v;
            v.yaw = act.yaw; v.scale = 250; v.origin = { x, 540 };
            addRoot(c.roots, 0, 90)->fn = [ch, v](const Ctx& cx, int t) { rig::draw(cx, *ch, t / 30.0, v); };
            x += 268;
        }
        Image a = frame(c, 20);
        CHECK(differing(a, c.background) > 1600 * 600 / 16);
        if (preview) ExportImage(a, (dir / "2d-acting.png").string().c_str());
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
    // Animals: walking in 3D, side on, and the same line-up as 2D puppets.
    {
        Comp c;
        c.w = 1600; c.h = 720; c.fps = 30; c.duration = 90; c.background = bg;
        auto scene = studio(2.4f);
        scene->camera.position = { 0.2f, 2.0f, 11.0f };
        scene->camera.target = { 0.2f, 1.55f, 0 };
        const std::pair<const char*, rig::Gait> cast[] = { { "cat", rig::Gait::Walk }, { "dog", rig::Gait::Trot }, { "horse", rig::Gait::Canter },
                                                            { "cow", rig::Gait::Walk }, { "giraffe", rig::Gait::Walk } };
        const float xs[] = { -3.6f, -2.9f, -1.5f, 0.75f, 3.0f };
        for (int i = 0; i < 5; i++) {
            auto a = std::make_shared<rig::Character>(rig::animal(cast[i].first));
            a->motion.play(rig::clips::gait(a->looks()[0].build, cast[i].second), 0);
            g3d::Object3D* o = rig::add(*scene, a);
            o->position = { xs[i], 0, 0 };
            o->rotation.y = 90;
        }
        addRoot(c.roots, 0, 90)->fn = g3d::draw(scene);
        Image a = frame(c, 20), b2 = frame(c, 20);
        CHECK(std::memcmp(a.data, b2.data, (size_t)a.width * a.height * 4) == 0);
        if (preview) ExportImage(a, (dir / "3d-animals.png").string().c_str());
        UnloadImage(a);
        UnloadImage(b2);
    }
    {
        Comp c;
        c.w = 1600; c.h = 600; c.fps = 30; c.duration = 90; c.background = { 238, 234, 224, 255 };
        const char* names[] = { "cat", "dog", "horse", "cow", "giraffe" };
        const float xs[] = { 110, 300, 600, 980, 1360 }, yaws[] = { 90, 70, 90, 110, 90 };
        for (int i = 0; i < 5; i++) {
            auto a = std::make_shared<rig::Character>(rig::animal(names[i]));
            a->motion.play(i == 2 ? rig::clips::gait(a->looks()[0].build, rig::Gait::Gallop) : rig::clips::stand(a->looks()[0].build), 0);
            rig::View2D v;
            v.yaw = yaws[i]; v.scale = 160; v.origin = { xs[i], 540 };
            addRoot(c.roots, 0, 90)->fn = [a, v](const Ctx& cx, int t) { rig::draw(cx, *a, t / 30.0, v); };
        }
        Image a = frame(c, 20);
        CHECK(differing(a, c.background) > 1600 * 600 / 20);
        if (preview) ExportImage(a, (dir / "2d-animals.png").string().c_str());
        UnloadImage(a);
    }
    // The horse's four gaits, five moments through one cycle each (rows: walk, trot, canter, gallop).
    {
        Comp c;
        c.w = 1600; c.h = 1200; c.fps = 30; c.duration = 2; c.background = { 238, 234, 224, 255 };
        const rig::Look horse = rig::animal("horse");
        const rig::Gait gaits[] = { rig::Gait::Walk, rig::Gait::Trot, rig::Gait::Canter, rig::Gait::Gallop };
        for (int row = 0; row < 4; row++)
            for (int col = 0; col < 5; col++) {
                auto a = std::make_shared<rig::Character>(horse);
                a->motion.play(rig::clips::gait(horse.build, gaits[row]), 0);
                const double at = col / 5.0 / rig::gaitCadence(horse.build, gaits[row]);
                rig::View2D v;
                v.yaw = 90; v.scale = 105; v.origin = { 160.f + col * 320, 280.f + row * 290 };
                addRoot(c.roots, 0, 2)->fn = [a, v, at](const Ctx& cx, int) { rig::draw(cx, *a, at, v); };
            }
        Image a = frame(c, 0);
        if (preview) ExportImage(a, (dir / "2d-gaits.png").string().c_str());
        UnloadImage(a);
    }
    // Birds and a dinosaur in 3D: a hen walking, a duck waddling, a hen with its wings
    // spread, and a T. rex striding (its own camera: it is 12 m long).
    {
        Comp c;
        c.w = 1600; c.h = 720; c.fps = 30; c.duration = 90; c.background = bg;
        auto scene = studio(0.5f);
        scene->camera.position = { 0, 0.42f, 2.3f };
        scene->camera.target = { 0, 0.2f, 0 };
        const rig::Look hen = rig::animal("chicken"), duck = rig::animal("duck");
        auto put = [&](const rig::Look& l, rig::Clip clip, Vector3 pos, float yaw) {
            auto a = std::make_shared<rig::Character>(l);
            a->motion.play(clip, 0);
            g3d::Object3D* o = rig::add(*scene, a);
            o->position = pos;
            o->rotation.y = yaw;
        };
        put(hen, rig::clips::gait(hen.build, rig::Gait::Walk), { -0.75f, 0, 0 }, 90);
        put(duck, rig::clips::gait(duck.build, rig::Gait::Walk), { -0.05f, 0, 0 }, 60);
        const rig::Build hb = hen.build, db = duck.build;
        put(hen, [hb](double s) { return rig::wings(hb, rig::clips::stand(hb)(s), 1, 25); }, { 0.7f, 0, 0 }, 20);
        put(duck, rig::clips::fly(db), { 0.3f, 0.3f, -0.7f }, 110);
        put(hen, rig::clips::fly(hb), { -0.45f, 0.3f, -0.7f }, 70);
        addRoot(c.roots, 0, 90)->fn = g3d::draw(scene);
        Image a = frame(c, 20);
        CHECK(differing(a, bg) > 1600 * 720 / 30);
        if (preview) ExportImage(a, (dir / "3d-birds.png").string().c_str());
        UnloadImage(a);
    }
    {
        Comp c;
        c.w = 1600; c.h = 720; c.fps = 30; c.duration = 90; c.background = bg;
        auto scene = studio(5.f);
        scene->camera.position = { 0, 3.5f, 22 };
        scene->camera.target = { 0, 2.6f, 0 };
        auto a = std::make_shared<rig::Character>(rig::animal("trex"));
        const rig::Build rb = a->looks()[0].build;
        a->motion.play(rig::clips::gait(rb, rig::Gait::Walk), 0);
        g3d::Object3D* o = rig::add(*scene, a);
        o->position = { -1.5f, 0, 0 };
        o->rotation.y = 75;
        addRoot(c.roots, 0, 90)->fn = g3d::draw(scene);
        Image img = frame(c, 20);
        if (preview) ExportImage(img, (dir / "3d-trex.png").string().c_str());
        UnloadImage(img);
    }
    // Birds in 2D: a hen's walk through one cycle (the head holding still, then thrusting),
    // a duck waddling, a T. rex striding and roaring.
    {
        Comp c;
        c.w = 1600; c.h = 900; c.fps = 30; c.duration = 2; c.background = { 238, 234, 224, 255 };
        const rig::Look hen = rig::animal("chicken"), duck = rig::animal("duck"), rex = rig::animal("trex");
        const float cad = rig::gaitCadence(hen.build, rig::Gait::Walk);
        auto cell = [&](const rig::Look& l, rig::Clip clip, double at, Vector2 origin, float scale, float yaw) {
            auto a = std::make_shared<rig::Character>(l);
            a->motion.play(clip, 0);
            rig::View2D v;
            v.yaw = yaw; v.scale = scale; v.origin = origin;
            addRoot(c.roots, 0, 2)->fn = [a, v, at](const Ctx& cx, int) { rig::draw(cx, *a, at, v); };
        };
        for (int col = 0; col < 5; col++)
            cell(hen, rig::clips::gait(hen.build, rig::Gait::Walk), col / 5.0 / cad, { 160.f + col * 320, 380 }, 520, 90);
        cell(duck, rig::clips::gait(duck.build, rig::Gait::Walk), 0.2, { 130, 820 }, 560, 90);
        cell(duck, rig::clips::gait(duck.build, rig::Gait::Walk), 0.35, { 360, 820 }, 560, 20);
        cell(hen, rig::clips::gait(hen.build, rig::Gait::Gallop), 0.1, { 590, 820 }, 520, 90);
        cell(rex, rig::clips::gait(rex.build, rig::Gait::Walk), 0.3, { 930, 860 }, 30, 90);
        cell(rex, rig::clips::stand(rex.build), 3.5, { 1340, 860 }, 30, 60);
        Image a = frame(c, 0);
        if (preview) ExportImage(a, (dir / "2d-birds.png").string().c_str());
        UnloadImage(a);
    }
    // Snakes and fish in 3D, from above and the side.
    {
        Comp c;
        c.w = 1600; c.h = 720; c.fps = 30; c.duration = 90; c.background = bg;
        auto scene = studio(0.6f);
        scene->camera.position = { 0, 1.5f, 1.9f };
        scene->camera.target = { 0, 0.05f, 0 };
        auto put = [&](const char* sp, rig::Gait g, bool walk, Vector3 pos, float yaw) {
            auto a = std::make_shared<rig::Character>(rig::animal(sp));
            const rig::Build cb = a->looks()[0].build;
            a->motion.play(walk ? rig::clips::gait(cb, g) : rig::clips::stand(cb), 0);
            g3d::Object3D* o = rig::add(*scene, a);
            o->position = pos;
            o->rotation.y = yaw;
        };
        put("snake", rig::Gait::Walk, true, { -0.55f, 0, 0.1f }, 90);
        put("snake", rig::Gait::Walk, false, { -0.3f, 0, -0.35f }, -30);
        put("fish", rig::Gait::Walk, true, { 0.55f, 0.12f, 0.15f }, 90);
        put("fish", rig::Gait::Gallop, true, { 0.6f, 0.3f, -0.3f }, 60);
        put("turtle", rig::Gait::Walk, true, { 0.05f, 0, 0.3f }, 70);
        auto sw = std::make_shared<rig::Character>(rig::animal("turtle"));
        sw->motion.play(rig::clips::paddle(sw->looks()[0].build), 0);
        g3d::Object3D* so = rig::add(*scene, sw);
        so->position = { 0.05f, 0.3f, -0.45f };
        so->rotation.y = 120;
        addRoot(c.roots, 0, 90)->fn = g3d::draw(scene);
        Image a = frame(c, 25);
        CHECK(differing(a, bg) > 1600 * 720 / 40);
        if (preview) ExportImage(a, (dir / "3d-chains.png").string().c_str());
        UnloadImage(a);
    }
    {
        Comp c;
        c.w = 1600; c.h = 600; c.fps = 30; c.duration = 2; c.background = { 238, 234, 224, 255 };
        const rig::Look snake = rig::animal("snake"), fish = rig::animal("fish");
        for (int col = 0; col < 4; col++) {
            auto a = std::make_shared<rig::Character>(col < 2 ? snake : fish);
            const rig::Build cb = a->looks()[0].build;
            a->motion.play(col == 1 ? rig::clips::stand(cb) : rig::clips::gait(cb, rig::Gait::Walk), 0);
            rig::View2D v;
            v.yaw = 90; v.pitch = col < 2 ? 70 : 0; v.scale = col < 2 ? 380 : 700;
            v.origin = { 220.f + col * 390, col < 2 ? 330.f : 400.f };
            const double at = 0.4 + col * 0.3;
            addRoot(c.roots, 0, 2)->fn = [a, v, at](const Ctx& cx, int) { rig::draw(cx, *a, at, v); };
        }
        Image a = frame(c, 0);
        if (preview) ExportImage(a, (dir / "2d-chains.png").string().c_str());
        UnloadImage(a);
    }
    // Riding: a rider on a cantering horse, in 3D and as a 2D puppet.
    {
        Comp c;
        c.w = 1600; c.h = 720; c.fps = 30; c.duration = 90; c.background = bg;
        auto scene = studio(2.4f);
        scene->camera.position = { 0, 1.7f, 7.5f };
        scene->camera.target = { 0, 1.3f, 0 };
        auto horse = std::make_shared<rig::Character>(rig::animal("horse"));
        const rig::Build hb = horse->looks()[0].build;
        horse->motion.play(rig::clips::gait(hb, rig::Gait::Canter), 0);
        rig::Look rl = rig::Look::toon();
        rl.build = rig::Build::adult();
        rl.outfit = rig::Outfit::basic({ 180, 40, 50, 255 }, { 236, 230, 214, 255 }, rl.skin, 1, 1);
        rl.feet("foot.boot");
        auto rider = std::make_shared<rig::Character>(rl);
        rider->motion.play(rig::clips::ride(rl.build), 0);
        rider->mount(rig::saddle(horse));
        for (auto ch : { horse, rider }) {
            g3d::Object3D* o = rig::add(*scene, ch);
            o->position = { -1.4f, 0, 0 };
            o->rotation.y = 70;
        }
        addRoot(c.roots, 0, 90)->fn = g3d::draw(scene);
        for (auto ch : { horse, rider }) {
            rig::View2D v;
            v.yaw = 90; v.scale = 150; v.origin = { 1250, 640 };
            addRoot(c.roots, 0, 90)->fn = [ch, v](const Ctx& cx, int t) { rig::draw(cx, *ch, t / 30.0, v); };
        }
        Image a = frame(c, 20);
        if (preview) ExportImage(a, (dir / "riding.png").string().c_str());
        UnloadImage(a);
    }
    // Carried: a chimp piggyback on a walking person (hands on the shoulders, feet hooked at
    // the hips), and a pilot standing on a titan's raised palm.
    {
        Comp c;
        c.w = 1600; c.h = 720; c.fps = 30; c.duration = 90; c.background = bg;
        auto scene = studio(3.f);
        scene->camera.position = { 0, 3.6f, 16 };
        scene->camera.target = { 0, 2.9f, 0 };
        rig::Look hl = rig::Look::toon();
        hl.build = rig::Build::adult();
        auto human = std::make_shared<rig::Character>(hl);
        human->motion.play(rig::clips::walk(0.9f), 0);
        const rig::Look cl = rig::animal("chimp");
        auto chimp = std::make_shared<rig::Character>(cl);
        chimp->motion.play(rig::clips::ride(cl.build), 0);
        const rig::Build& h = hl.build;
        rig::Mount back;
        back.carrier = human;
        back.joint = rig::Joint::Chest;
        back.at = { 0, -h.torso.y * 0.35f, -h.torso.z * 0.5f - cl.build.torso.z * 0.55f };
        back.grips = { { rig::Limb::HandL, rig::Joint::ShoulderL, { 0, 0.04f, -0.02f } }, { rig::Limb::HandR, rig::Joint::ShoulderR, { 0, 0.04f, -0.02f } },
                       { rig::Limb::FootL, rig::Joint::Pelvis, { h.hipWidth * 0.75f, -0.02f, h.torso.z * 0.25f } },
                       { rig::Limb::FootR, rig::Joint::Pelvis, { -h.hipWidth * 0.75f, -0.02f, h.torso.z * 0.25f } } };
        chimp->mount(back);
        for (auto ch : { human, chimp }) {
            g3d::Object3D* o = rig::add(*scene, ch);
            o->position = { -2.2f, 0, 0 };
            o->rotation.y = 150;
        }
        // The titan: a robot three and a half times life size, its right palm held up
        // before its chest; the pilot rides the palm.
        rig::Look tl = rig::Look::robot();
        tl.build = rig::Build::adult().scaled(3.5f);
        auto titan = std::make_shared<rig::Character>(tl);
        const rig::Build tb = tl.build;
        titan->motion.play([tb](double s) {
            rig::Pose p = rig::clips::idle(0.5f)(s);
            return rig::hand(tb, p, rig::Side::Right, { -0.35f * 3.5f, 1.15f * 3.5f, 0.45f * 3.5f }, { -0.3f, 0, 1 }, { 0, 1, 0 });
        }, 0);
        auto pilot = std::make_shared<rig::Character>(rig::Look::toon());
        pilot->motion.play(rig::clips::idle(), 0);
        rig::Mount palm;
        palm.carrier = titan;
        palm.joint = rig::Joint::WristR;
        palm.at = { 0.04f * 3.5f, -tb.hand * 0.55f, 0 };
        palm.rot = { 0, 0, -90 };                 // stand on the palm: the pilot's up along its normal
        pilot->mount(palm);
        for (auto ch : { titan, pilot }) {
            g3d::Object3D* o = rig::add(*scene, ch);
            o->position = { 2.2f, 0, -1.5f };
            o->rotation.y = -25;
        }
        addRoot(c.roots, 0, 90)->fn = g3d::draw(scene);
        Image a = frame(c, 20);
        CHECK(differing(a, bg) > 1600 * 720 / 20);
        if (preview) ExportImage(a, (dir / "carried.png").string().c_str());
        UnloadImage(a);
    }
    // Apes in 3D: knuckle-walking, hanging from a bar, swinging along bars.
    {
        Comp c;
        c.w = 1600; c.h = 720; c.fps = 30; c.duration = 90; c.background = bg;
        auto scene = studio(2.4f);
        scene->camera.position = { 0, 1.8f, 9.5f };
        scene->camera.target = { 0, 1.25f, 0 };
        g3d::Material steel;
        steel.color = { 120, 124, 132, 255 };
        steel.metalness = 0.8f;
        steel.roughness = 0.35f;
        const double at = 20 / 30.0;
        // A bar across the character's path: along its x, at height y and its z (character space).
        auto bar = [&](Vector3 pos, float yaw, float y, float z) {
            g3d::Object3D* o = scene->add(g3d::Geometry::box(0.9f, 0.045f, 0.045f), steel);
            o->position = { pos.x + std::sin(yaw * DEG2RAD) * z, y, pos.z + std::cos(yaw * DEG2RAD) * z };
            o->rotation.y = yaw;
        };
        const rig::Look chimp = rig::animal("chimp"), gorilla = rig::animal("gorilla"), orang = rig::animal("orangutan");
        auto put = [&](const rig::Look& l, rig::Clip clip, Vector3 pos, float yaw) {
            auto a = std::make_shared<rig::Character>(l);
            a->motion.play(clip, 0);
            g3d::Object3D* o = rig::add(*scene, a);
            o->position = pos;
            o->rotation.y = yaw;
        };
        put(chimp, rig::clips::knuckleWalk(chimp.build), { -3.2f, 0, 0 }, 90);
        put(gorilla, rig::clips::knuckleWalk(gorilla.build), { -1.3f, 0, 0 }, 70);
        put(orang, rig::clips::hang(orang.build, 2.3f), { 1.0f, 0, 0 }, 35);
        bar({ 1.0f, 0, 0 }, 35, 2.3f, 0);
        const float d = 0.8f, u = (float)std::fmod(at * 2 * rig::brachiateRate(chimp.build), 1.0);
        put(chimp, rig::clips::brachiate(chimp.build, 2.4f, d), { 3.1f, 0, 0 }, 70);
        for (int k = -1; k <= 2; k++) bar({ 3.1f, 0, 0 }, 70, 2.4f, d / 2 - d * u + k * d);
        addRoot(c.roots, 0, 90)->fn = g3d::draw(scene);
        Image a = frame(c, 20);
        CHECK(differing(a, bg) > 1600 * 720 / 20);
        if (preview) ExportImage(a, (dir / "3d-apes.png").string().c_str());
        UnloadImage(a);
    }
    // Apes in 2D: a chimp's knuckle-walk through one cycle, and an orangutan swinging bar to bar.
    {
        Comp c;
        c.w = 1600; c.h = 760; c.fps = 30; c.duration = 2; c.background = { 238, 234, 224, 255 };
        const rig::Look chimp = rig::animal("chimp"), orang = rig::animal("orangutan");
        const float cad = rig::gaitCadence(chimp.build, rig::Gait::Walk);
        for (int col = 0; col < 5; col++) {
            auto a = std::make_shared<rig::Character>(chimp);
            a->motion.play(rig::clips::knuckleWalk(chimp.build), 0);
            const double at = col / 5.0 / cad;
            rig::View2D v;
            v.yaw = 90; v.scale = 230; v.origin = { 160.f + col * 320, 330 };
            addRoot(c.roots, 0, 2)->fn = [a, v, at](const Ctx& cx, int) { rig::draw(cx, *a, at, v); };
        }
        const float d = 0.8f, bar = 2.3f, rate = rig::brachiateRate(orang.build);
        for (int col = 0; col < 5; col++) {
            auto a = std::make_shared<rig::Character>(orang);
            a->motion.play(rig::clips::brachiate(orang.build, bar, d), 0);
            const double at = col / 4.0 / (2 * rate);   // one swing, start to end
            const float u = (float)std::fmod(at * 2 * rate, 1.0) + (col == 4 ? 1.f : 0.f);
            rig::View2D v;
            v.yaw = 90; v.scale = 110; v.origin = { 160.f + col * 320, 730 };
            addRoot(c.roots, 0, 2)->fn = [a, v, at, u, d, bar](const Ctx& cx, int) {
                for (int k = -1; k <= 2; k++) {
                    const float z = d / 2 - d * u + k * d;
                    DrawCircleV({ v.origin.x - z * v.scale, v.origin.y - bar * v.scale }, 5, { 90, 94, 104, 255 });
                }
                rig::draw(cx, *a, at, v);
            };
        }
        Image a = frame(c, 0);
        if (preview) ExportImage(a, (dir / "2d-apes.png").string().c_str());
        UnloadImage(a);
    }
    CloseWindow();
    return true;
}


// Four-legged animals: a plan of their own, paws planted while down, every species built.
static void animals() {
    const rig::Build dog = rig::quadruped(rig::QuadSpec::dog());
    CHECK(rig::jointCount(dog) == 27);
    CHECK((int)rig::joint(dog, "paw-hind-R") == dog.plan->find("paw-hind-R"));
    for (int i = 1; i < rig::jointCount(dog); i++) CHECK((int)rig::parent(dog, (rig::Joint)i) < i);
    CHECK(rig::isRight(dog, rig::joint(dog, "knee-R")) && !rig::isRight(dog, rig::joint(dog, "knee-L")));
    CHECK(rig::isGrounded(dog, rig::joint(dog, "floor")));
    // At rest the legs are straight and the paws stand at paw height.
    const auto rest = rig::solve(dog, rig::Pose{});
    for (const char* n : { "paw-front-L", "paw-hind-R" })
        CHECK(near(rig::position(rest, rig::joint(dog, n)).y, dog.plan->dim("paw"), 1e-4f));
    // Scaling scales lengths, not angles.
    const rig::Build big = dog.scaled(2);
    CHECK(near(big.plan->dim("bodyLength"), 2 * dog.plan->dim("bodyLength")));
    CHECK(near(big.plan->trait("neckPitch"), dog.plan->trait("neckPitch")));
    CHECK(near(big.height(), 2 * dog.height(), 1e-4f));
    // Humanoid joints are unchanged by the bigger Pose.
    CHECK(rig::jointCount(rig::Build::toon()) == rig::kJoints);

    // Gaits: a paw that is down stays at paw height and moves back at the gait's speed (so
    // moving the animal by gaitSpeed leaves it still on the ground).
    for (const std::string& sp : rig::animals()) {
        const rig::Look look = rig::animal(sp);
        const rig::Build& b = look.build;
        CHECK(!rig::prims(look).empty());
        if (!b.plan || b.plan->kind != "quadruped") continue;   // apes() and birds() below
        const rig::Joint lh = rig::joint(b, "paw-hind-L");
        for (rig::Gait g : { rig::Gait::Walk, rig::Gait::Trot, rig::Gait::Canter, rig::Gait::Gallop }) {
            const rig::Clip clip = rig::clips::gait(b, g);
            const float c = rig::gaitCadence(b, g), v = rig::gaitSpeed(b, g);
            // Check the left hind paw early in its stance: it lands at the start of the cycle,
            // or halfway through in a trot.
            const double land = g == rig::Gait::Trot ? 0.5 : 0, t0 = (land + 0.04) / c, t1 = (land + 0.2) / c;
            const Vector3 a = rig::position(rig::solve(b, clip(t0)), lh), z = rig::position(rig::solve(b, clip(t1)), lh);
            const float paw = b.plan->dim("paw"), L = b.plan->dim("hindLength");
            CHECK(near(a.y, paw, 0.01f * L) && near(z.y, paw, 0.01f * L));
            CHECK(near((a.z - z.z) / (float)(t1 - t0), v, 0.04f * v));
        }
        const rig::Pose st = rig::clips::stand(b)(1.3);
        CHECK(near(rig::position(rig::solve(b, st), rig::joint(b, "paw-front-R")).y, b.plan->dim("paw"), 0.01f * b.plan->dim("frontLength")));
    }
    bool threw = false;
    try { rig::clips::gait(rig::Build::toon(), rig::Gait::Trot); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
}

// Birds and dinosaurs: two legs, feet planted while down; wings fold and spread.
static void birds() {
    const rig::Build hen = rig::biped(rig::BipedSpec::chicken());
    for (int i = 1; i < rig::jointCount(hen); i++) CHECK((int)rig::parent(hen, (rig::Joint)i) < i);
    CHECK(rig::isRight(hen, rig::joint(hen, "ankle-R")) && !rig::isRight(hen, rig::joint(hen, "tip-L")));
    const auto rest = rig::solve(hen, rig::Pose{});
    CHECK(near(rig::position(rest, rig::joint(hen, "foot-L")).y, hen.plan->dim("foot"), 1e-4f));
    for (const char* sp : { "chicken", "duck", "trex" }) {
        const rig::Look look = rig::animal(sp);
        const rig::Build& b = look.build;
        CHECK(b.plan && b.plan->kind == "biped" && !rig::prims(look).empty());
        const float foot = b.plan->dim("foot"), L = b.plan->dim("legLength");
        const rig::Joint lf = rig::joint(b, "foot-L");
        for (rig::Gait g : { rig::Gait::Walk, rig::Gait::Gallop }) {
            const rig::Clip clip = rig::clips::gait(b, g);
            const float c = rig::gaitCadence(b, g), v = rig::gaitSpeed(b, g);
            const double t0 = 0.03 / c, t1 = 0.25 / c;
            const Vector3 a = rig::position(rig::solve(b, clip(t0)), lf), z = rig::position(rig::solve(b, clip(t1)), lf);
            CHECK(near(a.y, foot, 0.01f * L) && near(z.y, foot, 0.01f * L));
            CHECK(near((a.z - z.z) / (float)(t1 - t0), v, 0.04f * v));
        }
        const rig::Pose st = rig::clips::stand(b)(2.1);
        CHECK(near(rig::position(rig::solve(b, st), rig::joint(b, "foot-R")).y, foot, 0.01f * L));
    }
    // Folded, a wing tip stays by the body; spread, it is a wingspan out, level and mirrored.
    const rig::Joint tipL = rig::joint(hen, "tip-L"), tipR = rig::joint(hen, "tip-R"), sh = rig::joint(hen, "shoulder-L");
    const float span = hen.plan->dim("upperArm") + hen.plan->dim("forearm") + hen.plan->dim("hand");
    const auto folded = rig::solve(hen, rig::wings(hen, rig::Pose{}, 0));
    CHECK(std::fabs(rig::position(folded, tipL).x) < hen.plan->dim("bodyRadius") * 1.3f);
    const auto open = rig::solve(hen, rig::wings(hen, rig::Pose{}, 1));
    const Vector3 l = rig::position(open, tipL), r = rig::position(open, tipR), s0 = rig::position(open, sh);
    CHECK(l.x - s0.x > span * 0.9f && near(l.y, s0.y, span * 0.15f));
    CHECK(near3(l, { -r.x, r.y, r.z }, 1e-3f));
    const auto up = rig::solve(hen, rig::wings(hen, rig::Pose{}, 1, 40));
    CHECK(rig::position(up, tipL).y > l.y + span * 0.4f);
    const rig::Build rex = rig::biped(rig::BipedSpec::trex());
    CHECK(near3(rig::wings(rex, rig::Pose{}, 1, 30)[rig::joint(rex, "shoulder-L")], { 0, 0, 0 }));
    // Flying: the wing tips beat up and down once per wingbeat, symmetric; the feet are off
    // the ground (folded up under the body); a T. rex cannot.
    const float rate = rig::flapRate(hen);
    const rig::Clip fl = rig::clips::fly(hen);
    float hi = -1e9f, lo = 1e9f;
    for (int i = 0; i < 12; i++) {
        const auto f = rig::solve(hen, fl(i / (12 * rate)));
        const Vector3 a = rig::position(f, tipL), b2 = rig::position(f, tipR);
        CHECK(near3(a, { -b2.x, b2.y, b2.z }, 1e-3f));
        hi = std::max(hi, a.y);
        lo = std::min(lo, a.y);
        CHECK(rig::position(f, rig::joint(hen, "foot-L")).y > hen.plan->dim("foot") * 3);
    }
    CHECK(hi - lo > span);
    CHECK(near3(rig::position(rig::solve(hen, fl(0.1)), tipL), rig::position(rig::solve(hen, fl(0.1 + 1 / rate)), tipL), 1e-3f));
    CHECK(throws([&] { rig::clips::fly(rex); }));
    CHECK(!rig::clips::glide(rig::animal("duck").build)(1.0).rot.empty());
}

// Two characters: one carried by another. A rider on a cantering horse keeps its hips on
// the saddle and its hands and feet on the reins and stirrups; a hand-off moves a pilot
// from one carrier joint to another without a jump.
static void mounts() {
    auto horse = std::make_shared<rig::Character>(rig::animal("horse"));
    const rig::Build hb = horse->looks()[0].build;
    horse->motion.play(rig::clips::gait(hb, rig::Gait::Canter), 0);
    rig::Look riderLook = rig::Look::toon();
    riderLook.build = rig::Build::adult();
    auto rider = std::make_shared<rig::Character>(riderLook);
    rider->motion.play(rig::clips::ride(riderLook.build), 0);
    const rig::Mount m = rig::saddle(horse);
    rider->mount(m);
    for (double t : { 0.0, 0.37, 1.21 }) {
        const auto rf = rig::solve(riderLook.build, rider->poseAt(t));
        const Vector3 seat = Vector3Transform(m.at, rig::frameAt(*horse, t, m.joint));
        CHECK(near3(rig::position(rf, rig::Joint::Pelvis), seat, 0.02f));
        for (const rig::Grip& g : m.grips) {
            const Vector3 target = Vector3Transform(g.at, rig::frameAt(*horse, t, g.joint));
            const rig::Joint end = g.limb == rig::Limb::HandL ? rig::Joint::WristL : g.limb == rig::Limb::HandR ? rig::Joint::WristR
                                 : g.limb == rig::Limb::FootL ? rig::Joint::AnkleL : rig::Joint::AnkleR;
            CHECK(Vector3Distance(rig::position(rf, end), target) < 0.06f);
        }
    }
    // A hand-off between two mounts on the same carrier blends smoothly.
    auto pilot = std::make_shared<rig::Character>(rig::Look::toon());
    rig::Mount a, b;
    a.carrier = horse; a.joint = rig::joint(hb, "neck-2"); a.until = 2;
    b.carrier = horse; b.joint = rig::joint(hb, "pelvis"); b.from = 1.8;
    pilot->mount(a).mount(b);
    Vector3 last{};
    float jump = 0;
    for (int i = 0; i <= 60; i++) {
        const double t = 1.4 + i * 0.02;
        const Vector3 at = rig::position(rig::solve(pilot->looks()[0].build, pilot->poseAt(t)), rig::Joint::Root);
        if (i) jump = std::max(jump, Vector3Distance(at, last));
        last = at;
    }
    CHECK(jump < 0.2f);                          // 1.2 m over the 0.3 s blend, never in one step
    CHECK(throws([&] { rig::saddle(rider); }));
}

// Turtles: a quadruped species with a shell, that walks and paddles.
static void turtles() {
    const rig::Look t = rig::animal("turtle");
    CHECK(t.build.plan && t.build.plan->variant == "turtle" && t.extra == "shell.turtle");
    CHECK(!rig::parts::get("shell.turtle").make(t.build, rig::Side::Left, 0).empty());
    const rig::Clip pd = rig::clips::paddle(t.build);
    const rig::Joint sh = rig::joint(t.build, "shoulder-L");
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < 16; i++) {
        const float x = pd(i * 0.1)[sh].x;
        lo = std::min(lo, x);
        hi = std::max(hi, x);
    }
    CHECK(hi - lo > 60);                         // the flipper strokes
    CHECK(rig::clips::paddle(rig::animal("dog").build)(0.3)[rig::joint(rig::animal("dog").build, "hip-L")].x != 0);
    CHECK(throws([] { rig::clips::paddle(rig::Build::toon()); }));
}

// Snakes and fish: a chain of joints laid along a wave.
static void chains() {
    const rig::Build snake = rig::chain(rig::ChainSpec::snake());
    const int n = (int)snake.plan->trait("segments");
    CHECK(rig::jointCount(snake) == n + 8);
    for (int i = 1; i < rig::jointCount(snake); i++) CHECK((int)rig::parent(snake, (rig::Joint)i) < i);
    CHECK(throws([] { rig::chain(rig::ChainSpec{ "snake", 1, 0.03f, 1, 60 }); }));
    // At rest the body lies straight along z, nose to tail one length.
    const auto rest = rig::solve(snake, rig::Pose{});
    const Vector3 nose = rig::position(rest, rig::joint(snake, "body-0")), tip = rig::position(rest, rig::joint(snake, "tip"));
    CHECK(near(nose.z - tip.z + snake.plan->dim("headLength"), 1.6f, 1e-3f) && near(nose.y, snake.plan->dim("radius"), 1e-4f));
    // Slithering: the body follows a path that stands still on the ground -- a point of the
    // body at a given ground position moves sideways only as the snake slides past it, so a
    // joint's sideways offset recurs one wavelength later at the next joint along.
    const rig::Clip sl = rig::clips::gait(snake, rig::Gait::Walk);
    const float v = rig::gaitSpeed(snake, rig::Gait::Walk), seg = snake.plan->dim("segment");
    const double dt = seg / v;
    for (double t : { 0.3, 1.1 }) {
        const Vector3 a = rig::position(rig::solve(snake, sl(t)), rig::joint(snake, "body-3"));
        const Vector3 b = rig::position(rig::solve(snake, sl(t + dt)), rig::joint(snake, "body-4"));
        CHECK(near(a.x, b.x, 0.01f));                       // the same ground point, a segment later
        // Every segment keeps its length.
        const auto f = rig::solve(snake, sl(t));
        CHECK(near(Vector3Distance(rig::position(f, rig::joint(snake, "body-5")), rig::position(f, rig::joint(snake, "body-6"))), seg, 1e-4f));
    }
    // A fish swims with a wave that grows to the tail: the tail swings far more than the head.
    const rig::Build fish = rig::animal("fish").build;
    const rig::Clip sw = rig::clips::gait(fish, rig::Gait::Walk);
    float head = 0, tail = 0;
    for (int i = 0; i < 20; i++) {
        const auto f = rig::solve(fish, sw(i * 0.05));
        head = std::max(head, std::fabs(rig::position(f, rig::joint(fish, "head")).x));
        tail = std::max(tail, std::fabs(rig::position(f, rig::joint(fish, "tip")).x));
    }
    CHECK(tail > 3 * head && tail > 0.01f);
    CHECK(rig::gaitSpeed(fish, rig::Gait::Gallop) > rig::gaitSpeed(fish, rig::Gait::Walk));
    for (const char* sp : { "snake", "fish" }) {
        const rig::Look l = rig::animal(sp);
        CHECK(!rig::prims(l).empty());
        CHECK(rig::clips::stand(l.build)(1.0)[rig::joint(l.build, "body-2")].y != 0);
    }
}

// Apes: humanoid builds that knuckle-walk, hang and swing along bars.
static void apes() {
    for (const char* sp : { "chimp", "gorilla", "orangutan" }) {
        const rig::Look look = rig::animal(sp);
        const rig::Build& b = look.build;
        CHECK(!b.plan && rig::jointCount(b) == rig::kJoints);
        CHECK(b.upperArm + b.forearm > b.thigh + b.shin);
        CHECK(!rig::prims(look).empty());
        // Knuckle-walking: a foot and a hand down early in their stance stay put on the floor
        // while the character moves at gaitSpeed.
        const rig::Clip kw = rig::clips::gait(b, rig::Gait::Walk);
        const float c = rig::gaitCadence(b, rig::Gait::Walk), v = rig::gaitSpeed(b, rig::Gait::Walk);
        const double f0 = 0.05 / c, f1 = 0.3 / c, h0 = 0.3 / c, h1 = 0.55 / c;
        const Vector3 fa = rig::position(rig::solve(b, kw(f0)), rig::Joint::AnkleL), fz = rig::position(rig::solve(b, kw(f1)), rig::Joint::AnkleL);
        CHECK(near(fa.y, b.ankle, 0.01f) && near(fz.y, b.ankle, 0.01f));
        CHECK(near((fa.z - fz.z) / (float)(f1 - f0), v, 0.05f * v));
        const Vector3 ha = rig::palmAt(b, kw(h0), rig::Side::Left), hz = rig::palmAt(b, kw(h1), rig::Side::Left);
        CHECK(near(ha.y, b.hand * 0.5f, 0.02f) && near(hz.y, b.hand * 0.5f, 0.02f));
        CHECK(near((ha.z - hz.z) / (float)(h1 - h0), v, 0.08f * v));
        // Hanging: both palms on the bar, whatever the sway.
        for (double t : { 0.0, 0.7, 1.9 }) {
            const rig::Pose p = rig::clips::hang(b, 2.2f)(t);
            const Vector3 l = rig::palmAt(b, p, rig::Side::Left), r = rig::palmAt(b, p, rig::Side::Right);
            CHECK(near((l.y + r.y) / 2, 2.2f, 0.01f) && near(l.z + r.z, 0, 0.02f));
            CHECK(near(l.y, r.y, 0.02f));
        }
        // Brachiating: the holding palm is on its bar, which slides back at the travel speed;
        // the pose is continuous where the hands trade.
        const float d = 0.9f, rate = rig::brachiateRate(b);
        const rig::Clip br = rig::clips::brachiate(b, 2.6f, d);
        for (double u : { 0.1, 0.5, 0.9 }) {
            const double t = u / (2 * rate);
            const Vector3 g = rig::palmAt(b, br(t), rig::Side::Left);
            CHECK(near(g.y, 2.6f, 0.03f) && near(g.z, d / 2 - d * (float)u, 0.03f));
        }
        const double swap = 1 / (2 * rate);
        const rig::Pose before = br(swap - 1e-4), after = br(swap + 1e-4);
        CHECK(near3(before.offset, after.offset, 0.01f));
        for (rig::Side sd : { rig::Side::Left, rig::Side::Right })
            CHECK(near3(rig::palmAt(b, before, sd), rig::palmAt(b, after, sd), 0.03f));
    }
    // Apes stand like people.
    CHECK(near3(rig::clips::stand(rig::animal("chimp").build)(0.5)[rig::Joint::Chest], rig::clips::idle()(0.5)[rig::Joint::Chest]));
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
    acting();
    animals();
    apes();
    birds();
    chains();
    turtles();
    mounts();
    const bool preview = argc == 3 && std::string(argv[1]) == "--preview";
    if (!(argc == 2 && std::string(argv[1]) == "--no-gl")) CHECK(render(preview ? argv[2] : "", preview));
    if (failures) std::fprintf(stderr, "%d failure(s)\n", failures);
    else std::printf("rig tests passed\n");
    return failures ? 1 : 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "uncaught: %s\n", e.what());
    return 2;
}
