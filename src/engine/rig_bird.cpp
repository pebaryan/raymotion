// rig:: two-legged animals -- the biped body plan (birds and dinosaurs), its species
// (chicken, duck, T. rex), their parts, wings that fold and spread, and the clips that make
// them stand, walk and run with their feet planted. Every pose is computed from the time.
#include "rig_internal.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rmtn {
namespace rig {

namespace {
constexpr float TAU = 6.28318530718f;

float frac(double x) { return (float)(x - std::floor(x)); }
float smooth(float u) {
    u = std::clamp(u, 0.f, 1.f);
    return u * u * (3 - 2 * u);
}
float sinDeg(float d) { return std::sin(d * DEG2RAD); }
float cosDeg(float d) { return std::cos(d * DEG2RAD); }
float sgn(Side s) { return s == Side::Left ? 1.f : -1.f; }
const char* LR(Side s) { return s == Side::Left ? "-L" : "-R"; }
Joint J(const Skeleton& k, const char* base, Side s) { return k[std::string(base) + LR(s)]; }
// A deterministic value in 0..1 for an integer.
float hash01(int i) {
    const float x = std::sin(i * 12.9898f + 78.233f) * 43758.5453f;
    return x - std::floor(x);
}

const SkeletonPtr& planOf(const Build& b, const char* what) {
    if (!b.plan || b.plan->kind != "biped")
        throw std::runtime_error(std::string("rig: ") + what + " needs a biped build (rig::biped)");
    return b.plan;
}

// ---- shapes
Prim super(const char* name, Joint j, Vector3 at, Vector3 size, float sqV, float sqH, Paint paint) {
    Prim p;
    p.name = name; p.joint = j; p.at = at; p.size = size; p.squareV = sqV; p.squareH = sqH; p.paint = paint;
    return p;
}
Prim ball(const char* name, Joint j, Vector3 at, float d, Paint paint) { return super(name, j, at, { d, d, d }, 1, 1, paint); }
Prim tube(const char* name, Joint a, Joint b, Joint c, float r0, float r1, float softness, Paint paint) {
    Prim t;
    t.kind = Prim::Kind::Tube; t.name = name; t.a = a; t.b = b; t.c = c; t.joint = a;
    t.r0 = r0; t.r1 = r1; t.softness = softness; t.paint = paint;
    return t;
}
// A cone with its apex along `rot` from +y (apex up at rot 0).
Prim cone(const char* name, Joint j, Vector3 at, Vector3 size, Vector3 rot, Paint paint) {
    Prim p = super(name, j, at, size, 1, 1, paint);
    p.kind = Prim::Kind::Cone; p.rot = rot;
    return p;
}
Prim fixed(Prim p, Color c) { p.paint = Paint::Fixed; p.color = c; return p; }
Prim turned(Prim p, Vector3 r) { p.rot = r; return p; }
Prim behind(Prim p, float d) { p.depth2d = d; return p; }

const Color WHITE_EYE{ 250, 248, 240, 255 };
const Color IVORY{ 236, 226, 200, 255 };

void eye(std::vector<Prim>& v, Joint head, Vector3 at, float d, Vector3 look) {
    v.push_back(fixed(ball("eye", head, at, d, Paint::Fixed), WHITE_EYE));
    v.push_back(ball("pupil", head, Vector3Add(at, Vector3Scale(look, d * 0.3f)), d * 0.6f, Paint::Dark));
}
// Directions in the vertical plane: `pitch` degrees above level, forwards or backwards.
Vector3 ahead(float pitch) { return { 0, sinDeg(pitch), cosDeg(pitch) }; }
Vector3 astern(float pitch) { return { 0, sinDeg(pitch), -cosDeg(pitch) }; }
// The Euler XYZ angles (degrees) that turn a prim's +y along `dir` (in the y-z plane).
Vector3 alongYZ(Vector3 dir) { return { std::atan2(dir.z, dir.y) * RAD2DEG, 0, 0 }; }

// ---- parts
std::vector<Prim> bodyPart(const Build& b) {
    const Skeleton& k = *planOf(b, "body.biped");
    const float L = k.dim("bodyLength"), r = k.dim("bodyRadius"), bp = k.trait("bodyPitch");
    const float nr = k.dim("neckRadius");
    const std::string& sp = k.variant;
    std::vector<Prim> v;
    // The body: an egg along the body's pitch, fuller at the breast.
    v.push_back(turned(super("body", k["pelvis"], Vector3Scale(ahead(bp), L * 0.1f), { r * 2, r * 1.9f, L * 1.2f }, 1, 1, Paint::Skin), { -bp, 0, 0 }));
    Prim breast = ball("breast", k["chest"], { 0, -r * 0.15f, 0 }, r * 1.6f, Paint::Skin);
    if (sp == "duck") breast = fixed(breast, { 120, 64, 46, 255 });
    v.push_back(breast);
    if (sp == "trex")
        v.push_back(turned(super("belly", k["spine"], { 0, -r * 0.55f, 0 }, { r * 1.5f, r * 0.9f, L * 0.9f }, 1, 1, Paint::Accent), { -bp, 0, 0 }));
    v.push_back(tube("neck", k["neck-1"], k["neck-2"], k["head"], nr * 1.3f, nr * 0.95f, 1, Paint::Skin));
    v.push_back(ball("neck-base", k["neck-1"], { 0, 0, 0 }, nr * 2.7f, Paint::Skin));
    if (sp == "duck") {
        // The drake's white collar, round the top of the neck.
        const float np = k.trait("neckPitch"), seg = k.dim("neckLength") / 2;
        v.push_back(fixed(turned(super("collar", k["neck-2"], Vector3Scale(ahead(np), seg * 0.55f), { nr * 2.3f, nr * 0.4f, nr * 2.3f }, 1, 1, Paint::Fixed), { 90 - np, 0, 0 }),
                          { 246, 246, 240, 255 }));
    }
    return v;
}

std::vector<Prim> headPart(const Build& b) {
    const Skeleton& k = *planOf(b, "head.biped");
    const Joint h = k["head"], jaw = k["jaw"];
    const float L = k.dim("headLength"), r = k.dim("headRadius");
    const std::string& sp = k.variant;
    std::vector<Prim> v;
    if (sp == "trex") {
        Prim skull = super("skull", h, { 0, r * 0.15f, L * 0.4f }, { r * 1.7f, r * 1.6f, L * 0.95f }, 0.55f, 0.8f, Paint::Skin);
        skull.taper = 0.85f;
        v.push_back(skull);
        v.push_back(super("snout", h, { 0, 0, L * 0.78f }, { r * 1.15f, r * 1.05f, L * 0.42f }, 0.6f, 0.8f, Paint::Skin));
        v.push_back(super("jaw", jaw, { 0, -r * 0.12f, L * 0.48f }, { r * 1.35f, r * 0.55f, L * 0.9f }, 0.6f, 0.8f, Paint::Skin));
        v.push_back(super("throat", jaw, { 0, -r * 0.2f, L * 0.12f }, { r * 1.3f, r * 0.75f, L * 0.35f }, 1, 1, Paint::Accent));
        for (Side s : { Side::Left, Side::Right }) {
            const float x = sgn(s);
            v.push_back(super("brow", h, { x * r * 0.62f, r * 0.82f, L * 0.22f }, { r * 0.4f, r * 0.22f, r * 0.6f }, 1, 1, Paint::Skin));
            eye(v, h, { x * r * 0.78f, r * 0.55f, L * 0.24f }, r * 0.26f, Vector3Normalize({ x, 0.1f, 0.4f }));
            v.push_back(ball("nostril", h, { x * r * 0.3f, r * 0.4f, L * 0.98f }, r * 0.14f, Paint::Dark));
            for (int i = 0; i < 6; i++) {
                const float z = L * (0.42f + 0.1f * i);
                v.push_back(fixed(cone("tooth", h, { x * r * 0.5f, -r * 0.5f, z }, { r * 0.12f, r * 0.3f, r * 0.12f }, { 180, 0, 0 }, Paint::Fixed), IVORY));
                if (i < 5)
                    v.push_back(fixed(cone("tooth", jaw, { x * r * 0.46f, r * 0.12f, z + L * 0.03f }, { r * 0.1f, r * 0.24f, r * 0.1f }, { 0, 0, 0 }, Paint::Fixed), IVORY));
            }
        }
    } else if (sp == "duck") {
        v.push_back(super("head", h, { 0, r * 0.1f, r * 0.15f }, { r * 2.0f, r * 1.9f, r * 2.3f }, 1, 1, Paint::Accent));
        const Color bill{ 226, 196, 70, 255 };
        v.push_back(fixed(super("bill", h, { 0, -r * 0.18f, r * 1.0f + L * 0.4f }, { r * 0.95f, r * 0.3f, L * 1.05f }, 0.5f, 1, Paint::Fixed), bill));
        v.push_back(fixed(super("bill-low", jaw, { 0, r * 0.08f, r * 0.8f + L * 0.45f }, { r * 0.85f, r * 0.16f, L * 0.9f }, 0.5f, 1, Paint::Fixed), bill));
        v.push_back(ball("nail", h, { 0, -r * 0.15f, r * 1.0f + L * 0.9f }, r * 0.25f, Paint::Dark));
        for (Side s : { Side::Left, Side::Right })
            eye(v, h, { sgn(s) * r * 0.82f, r * 0.35f, r * 0.45f }, r * 0.32f, Vector3Normalize({ sgn(s), 0, 0.4f }));
    } else {   // chicken
        v.push_back(ball("head", h, { 0, r * 0.1f, r * 0.25f }, r * 2, Paint::Skin));
        v.push_back(cone("beak", h, { 0, -r * 0.02f, r * 1.05f + L * 0.25f }, { r * 0.62f, L * 0.6f, r * 0.5f }, { 90, 0, 0 }, Paint::Shoes));
        v.push_back(cone("beak-low", jaw, { 0, r * 0.12f, r * 0.85f + L * 0.2f }, { r * 0.45f, L * 0.42f, r * 0.3f }, { 90, 0, 0 }, Paint::Shoes));
        for (int i = 0; i < 4; i++)
            v.push_back(ball("comb", h, { 0, r * (1.0f + 0.1f * std::sin(i * 1.3f)), r * (0.75f - 0.38f * i) }, r * (0.58f - 0.06f * std::abs(i - 1.5f)), Paint::Accent));
        for (Side s : { Side::Left, Side::Right }) {
            v.push_back(super("wattle", jaw, { sgn(s) * r * 0.16f, -r * 0.35f, r * 0.75f }, { r * 0.3f, r * 0.62f, r * 0.42f }, 1, 1, Paint::Accent));
            eye(v, h, { sgn(s) * r * 0.78f, r * 0.32f, r * 0.45f }, r * 0.36f, Vector3Normalize({ sgn(s), 0, 0.35f }));
        }
    }
    return v;
}

std::vector<Prim> tailPart(const Build& b) {
    const Skeleton& k = *planOf(b, "tail.biped");
    const float tl = k.dim("tailLength"), tr = k.dim("tailRadius"), tp = k.trait("tailPitch"), r = k.dim("bodyRadius");
    const Joint t1 = k["tail-1"], t2 = k["tail-2"], t3 = k["tail-3"];
    const std::string& sp = k.variant;
    std::vector<Prim> v;
    if (sp == "trex") {
        v.push_back(tube("tail", t1, t2, t3, tr, tr * 0.32f, 1, Paint::Skin));
        v.push_back(turned(super("tail-tip", t3, Vector3Scale(astern(tp), tl * 0.1f), { tr * 0.55f, tl * 0.22f, tr * 0.5f }, 1, 1, Paint::Skin), alongYZ(astern(tp))));
        v.push_back(turned(super("tail-base", t1, Vector3Scale(astern(tp), -tl * 0.05f), { r * 1.5f, r * 1.5f, tl * 0.3f }, 1, 1, Paint::Skin), { -tp, 0, 0 }));
    } else if (sp == "duck") {
        v.push_back(turned(super("tail", t1, Vector3Scale(astern(tp), tl * 0.35f), { r * 1.0f, r * 0.4f, tl }, 1, 1, Paint::Skin), { -tp, 0, 0 }));
        v.push_back(ball("curl", t2, { 0, tr * 1.5f, 0 }, tr * 1.6f, Paint::Accent));
    } else {   // chicken: an upright fan of feathers
        for (int i = 0; i < 5; i++) {
            const float a = tp - 30 + 15 * i, x = (i - 2) * tr * 0.5f;
            const Vector3 d = astern(a);
            v.push_back(turned(super("feather", t2, Vector3Add({ x, 0, 0 }, Vector3Scale(d, tl * 0.4f)), { tr * 0.45f, tl * 0.95f, tr * 2.2f }, 1, 1, Paint::Hair), alongYZ(d)));
        }
        v.push_back(ball("tail-root", t1, { 0, 0, 0 }, r * 0.9f, Paint::Skin));
    }
    for (Prim& p : v) p.depth2d = -0.4f * r;
    return v;
}

std::vector<Prim> legPart(const Build& b, Side s) {
    const Skeleton& k = *planOf(b, "leg.biped");
    const float lr = k.dim("legRadius"), toe = k.dim("toe"), foot = k.dim("foot"), tarsus = k.dim("tarsus"), r = k.dim("bodyRadius");
    const Joint hip = J(k, "hip", s), knee = J(k, "knee", s), ankle = J(k, "ankle", s), ft = J(k, "foot", s);
    const std::string& sp = k.variant;
    const bool rex = sp == "trex";
    std::vector<Prim> v;
    v.push_back(behind(tube("drumstick", hip, knee, ankle, lr * (rex ? 2.2f : 2.8f), lr * 0.75f, 0.6f, Paint::Skin), -r * 0.7f));
    if (rex) v.push_back(behind(super("haunch", hip, { 0, -k.dim("thigh") * 0.25f, 0 }, { lr * 3.2f, k.dim("thigh") * 1.0f, lr * 3.6f }, 1, 1, Paint::Skin), -r * 0.7f));
    const Paint scale = rex ? Paint::Skin : Paint::Shoes;
    v.push_back(super("tarsus", ankle, { 0, -tarsus * 0.5f, 0 }, { lr * 0.8f, tarsus * 1.05f, lr * 0.8f }, 0.4f, 1, scale));
    v.push_back(ball("heel", ankle, { 0, 0, 0 }, lr * 1.05f, scale));
    const float spread = sp == "duck" ? 32.f : 28.f;
    for (float yaw : { -spread, 0.f, spread }) {
        const Vector3 d{ sinDeg(yaw), 0, cosDeg(yaw) };
        const float len = toe * (yaw == 0 ? 1.f : 0.8f);
        v.push_back(turned(super("toe", ft, Vector3Add({ 0, -foot * 0.45f, 0 }, Vector3Scale(d, len * 0.5f)), { lr * 0.55f, lr * 0.5f, len }, 1, 1, scale), { 0, yaw, 0 }));
        if (rex)
            v.push_back(fixed(cone("claw", ft, Vector3Add({ 0, -foot * 0.5f, 0 }, Vector3Scale(d, len * 1.02f)), { lr * 0.35f, lr * 0.6f, lr * 0.3f }, { 105, yaw, 0 }, Paint::Fixed), IVORY));
    }
    if (sp == "duck")
        v.push_back(super("web", ft, { 0, -foot * 0.55f, toe * 0.45f }, { toe * 0.9f, foot * 0.25f, toe * 0.8f }, 1, 0.6f, Paint::Shoes));
    else if (!rex)
        v.push_back(turned(super("toe-back", ft, { 0, -foot * 0.45f, -toe * 0.22f }, { lr * 0.5f, lr * 0.45f, toe * 0.45f }, 1, 1, Paint::Shoes), { 0, 0, 0 }));
    return v;
}

// Wings: feather panels on the three bones, the chord trailing behind (-z) and the bones
// along x at rest (the right wing along -x).
std::vector<Prim> wingPart(const Build& b, Side s) {
    const Skeleton& k = *planOf(b, "wing.bird");
    const float x = sgn(s), chord = k.dim("chord"), ar = k.dim("armRadius");
    const Joint sh = J(k, "shoulder", s), el = J(k, "elbow", s), wr = J(k, "wrist", s), tip = J(k, "tip", s);
    const float a = k.dim("upperArm"), f = k.dim("forearm"), h = k.dim("hand");
    std::vector<Prim> v;
    v.push_back(tube("leading-edge", sh, el, wr, ar * 1.3f, ar * 0.8f, 0.6f, Paint::Hair));
    v.push_back(super("coverts", sh, { x * a * 0.5f, 0, -chord * 0.38f }, { a * 1.25f, chord * 0.14f, chord * 0.9f }, 1, 0.8f, Paint::Hair));
    v.push_back(super("secondaries", el, { x * f * 0.5f, 0, -chord * 0.45f }, { f * 1.2f, chord * 0.1f, chord * 1.0f }, 1, 0.7f, Paint::Hair));
    Prim prim = super("primaries", wr, { x * h * 0.55f, 0, -chord * 0.32f }, { h * 1.35f, chord * 0.08f, chord * 0.85f }, 1, 0.6f, Paint::Hair);
    v.push_back(prim);
    return v;
}

// A T. rex's arms: short, hanging, two claws.
std::vector<Prim> armPart(const Build& b, Side s) {
    const Skeleton& k = *planOf(b, "arm.trex");
    const float ar = k.dim("armRadius");
    const Joint sh = J(k, "shoulder", s), el = J(k, "elbow", s), wr = J(k, "wrist", s), tip = J(k, "tip", s);
    std::vector<Prim> v{ tube("arm", sh, el, wr, ar * 1.3f, ar * 0.8f, 0.6f, Paint::Skin),
                         super("hand", wr, { 0, -k.dim("hand") * 0.4f, 0 }, { ar * 1.2f, k.dim("hand") * 0.8f, ar * 1.1f }, 1, 1, Paint::Skin) };
    for (float dx : { -0.4f, 0.4f })
        v.push_back(fixed(cone("claw", tip, { dx * ar, 0, ar * 0.3f }, { ar * 0.45f, ar * 1.3f, ar * 0.4f }, { 160, 0, 0 }, Paint::Fixed), IVORY));
    return v;
}
} // namespace

namespace detail {
void birdParts(std::map<std::string, PartDef>& reg) {
    // On a body without the biped plan these parts add nothing.
    auto def = [&](const std::string& name, Slot slot, std::function<std::vector<Prim>(const Build&, Side)> make) {
        reg[name] = PartDef{ name, slot, [make](const Build& b, Side s, float) {
            return b.plan && b.plan->kind == "biped" ? make(b, s) : std::vector<Prim>{};
        }, {} };
    };
    def("body.biped", Slot::Torso, [](const Build& b, Side) { return bodyPart(b); });
    def("head.biped", Slot::Head, [](const Build& b, Side) { return headPart(b); });
    def("tail.biped", Slot::Hips, [](const Build& b, Side) { return tailPart(b); });
    def("leg.biped", Slot::Leg, [](const Build& b, Side s) { return legPart(b, s); });
    def("wing.bird", Slot::Arm, [](const Build& b, Side s) { return wingPart(b, s); });
    def("arm.trex", Slot::Arm, [](const Build& b, Side s) { return armPart(b, s); });
}
} // namespace detail

// ------------------------------------------------------------------ species
BipedSpec BipedSpec::chicken() { return BipedSpec{}; }
BipedSpec BipedSpec::duck() {
    BipedSpec s;
    s.species = "duck";
    s.bodyLength = 0.3f; s.bodyRadius = 0.1f; s.bodyPitch = 8;
    s.thigh = 0.06f; s.shank = 0.07f; s.tarsus = 0.045f; s.foot = 0.012f; s.toe = 0.06f;
    s.legRadius = 0.018f; s.stance = 0.055f; s.rake = 10;
    s.neckLength = 0.13f; s.neckRadius = 0.032f; s.neckPitch = 78;
    s.headLength = 0.055f; s.headRadius = 0.042f; s.headPitch = 0;
    s.tailLength = 0.07f; s.tailRadius = 0.012f; s.tailPitch = 25;
    s.upperArm = 0.08f; s.forearm = 0.1f; s.hand = 0.13f; s.armRadius = 0.016f; s.chord = 0.11f;
    return s;
}
BipedSpec BipedSpec::trex() {
    BipedSpec s;
    s.species = "trex";
    s.bodyLength = 3.0f; s.bodyRadius = 0.85f; s.bodyPitch = -4;
    s.thigh = 1.3f; s.shank = 1.25f; s.tarsus = 0.7f; s.foot = 0.18f; s.toe = 0.55f;
    s.legRadius = 0.26f; s.stance = 0.5f; s.rake = 22;
    s.neckLength = 0.9f; s.neckRadius = 0.42f; s.neckPitch = 25;
    s.headLength = 1.4f; s.headRadius = 0.42f; s.headPitch = 30;
    s.tailLength = 5.2f; s.tailRadius = 0.6f; s.tailPitch = 2;
    s.upperArm = 0.4f; s.forearm = 0.32f; s.hand = 0.18f; s.armRadius = 0.09f; s.chord = 0;
    s.wings = false;
    return s;
}

Build biped(const BipedSpec& q) {
    auto k = std::make_shared<Skeleton>();
    k->kind = "biped";
    k->variant = q.species;
    const float legs = q.thigh + q.shank + q.tarsus, hipY = legs + q.foot, r = q.bodyRadius, L = q.bodyLength;
    auto add = [&](const std::string& name, const std::string& parent, Vector3 rest, bool right = false) {
        JointDef d;
        d.name = name; d.rest = rest; d.right = right;
        d.parent = parent.empty() ? 0 : k->find(parent);
        k->joints.push_back(d);
    };
    k->joints.push_back(JointDef{ "root", 0, { 0, 0, 0 } });
    // The hips at the middle of the body (the weight over the feet), the chest ahead of them.
    add("pelvis", "root", { 0, hipY, 0 });
    add("spine", "pelvis", Vector3Scale(ahead(q.bodyPitch), L * 0.2f));
    add("chest", "spine", Vector3Scale(ahead(q.bodyPitch), L * 0.25f));
    add("neck-1", "chest", Vector3Add(Vector3Scale(ahead(q.bodyPitch), L * 0.05f), { 0, r * 0.4f, 0 }));
    add("neck-2", "neck-1", Vector3Scale(ahead(q.neckPitch), q.neckLength / 2));
    add("head", "neck-2", Vector3Scale(ahead(q.neckPitch), q.neckLength / 2));
    add("jaw", "head", { 0, -q.headRadius * 0.3f, q.headRadius * 0.1f });
    add("tail-1", "pelvis", Vector3Add(Vector3Scale(ahead(q.bodyPitch), -L * 0.45f), { 0, r * 0.25f, 0 }));
    add("tail-2", "tail-1", Vector3Scale(astern(q.tailPitch), q.tailLength / 2));
    add("tail-3", "tail-2", Vector3Scale(astern(q.tailPitch), q.tailLength / 2));
    for (Side s : { Side::Left, Side::Right }) {
        const bool right = s == Side::Right;
        const std::string e = LR(s);
        const float x = sgn(s);
        add("hip" + e, "pelvis", { x * q.stance, 0, 0 }, right);
        add("knee" + e, "hip" + e, { 0, -q.thigh, 0 }, right);
        add("ankle" + e, "knee" + e, { 0, -q.shank, 0 }, right);
        add("foot" + e, "ankle" + e, { 0, -q.tarsus, 0 }, right);
        if (q.wings) {
            add("shoulder" + e, "chest", { x * r * 0.7f, r * 0.3f, -r * 0.15f }, right);
            add("elbow" + e, "shoulder" + e, { x * q.upperArm, 0, 0 }, right);
            add("wrist" + e, "elbow" + e, { x * q.forearm, 0, 0 }, right);
            add("tip" + e, "wrist" + e, { x * q.hand, 0, 0 }, right);
        } else {
            add("shoulder" + e, "chest", { x * r * 0.6f, -r * 0.35f, r * 0.3f }, right);
            add("elbow" + e, "shoulder" + e, { 0, -q.upperArm, 0 }, right);
            add("wrist" + e, "elbow" + e, { 0, -q.forearm, 0 }, right);
            add("tip" + e, "wrist" + e, { 0, -q.hand, 0 }, right);
        }
    }
    add("floor", "root", { 0, 0, 0 });
    k->joints.back().grounded = true;
    k->height = hipY + L * 0.45f * std::max(0.f, sinDeg(q.bodyPitch)) + r * 0.4f + sinDeg(q.neckPitch) * q.neckLength + q.headRadius * 1.5f;
    k->dims = { { "bodyLength", L }, { "bodyRadius", r }, { "thigh", q.thigh }, { "shank", q.shank }, { "tarsus", q.tarsus },
                { "foot", q.foot }, { "toe", q.toe }, { "legRadius", q.legRadius }, { "stance", q.stance },
                { "neckLength", q.neckLength }, { "neckRadius", q.neckRadius }, { "headLength", q.headLength },
                { "headRadius", q.headRadius }, { "tailLength", q.tailLength }, { "tailRadius", q.tailRadius },
                { "upperArm", q.upperArm }, { "forearm", q.forearm }, { "hand", q.hand }, { "armRadius", q.armRadius },
                { "chord", q.chord }, { "legLength", legs } };
    k->traits = { { "bodyPitch", q.bodyPitch }, { "neckPitch", q.neckPitch }, { "headPitch", q.headPitch },
                  { "tailPitch", q.tailPitch }, { "rake", q.rake }, { "wings", q.wings ? 1.f : 0.f } };
    Build b;
    b.plan = k;
    return b;
}

namespace detail {
bool birdLook(const std::string& species, Look& l) {
    BipedSpec q;
    if (species == "chicken") q = BipedSpec::chicken();
    else if (species == "duck") q = BipedSpec::duck();
    else if (species == "trex") q = BipedSpec::trex();
    else return false;
    l = Look{};
    l.build = biped(q);
    l.head = "head.biped";
    l.hair = ""; l.hat = "";
    l.torso = "body.biped";
    l.hips = "tail.biped";
    l.arms(q.wings ? "wing.bird" : "arm.trex").hands("").legs("leg.biped").feet("");
    l.face = nullptr;
    l.outfit = Outfit{};
    if (species == "chicken") {
        l.skin = { 246, 242, 232, 255 }; l.hairColor = { 236, 230, 214, 255 }; l.accent = { 212, 42, 40, 255 };
        l.shoes = { 236, 178, 62, 255 }; l.dark = { 24, 20, 20, 255 };
    } else if (species == "duck") {
        l.skin = { 196, 192, 184, 255 }; l.hairColor = { 128, 110, 92, 255 }; l.accent = { 36, 96, 62, 255 };
        l.shoes = { 238, 140, 48, 255 }; l.dark = { 20, 18, 18, 255 };
    } else {
        l.skin = { 104, 110, 74, 255 }; l.hairColor = { 80, 84, 58, 255 }; l.accent = { 184, 170, 128, 255 };
        l.shoes = { 70, 64, 52, 255 }; l.dark = { 20, 16, 14, 255 };
    }
    l.outfit.fallback = l.skin;
    return true;
}
} // namespace detail

// ------------------------------------------------------------------ wings
Pose wings(const Build& b, Pose p, float spread, float flap, float sweep) {
    const Skeleton& k = *planOf(b, "rig::wings");
    if (k.trait("wings") < 0.5f) return p;
    const float s = smooth(spread);
    // Directions in the chest's frame for the left wing: each bone, and its chord (the way
    // the feathers trail). Folded, the wing lies along the flank in a Z, feathers down;
    // spread, the bones run out sideways, raised `flap` degrees (the tip lagging behind).
    struct Seg { const char* joint; Vector3 foldBone, foldChord; float lag, back; };
    const Seg segs[3] = { { "shoulder", { 0.15f, -0.2f, -1 }, { 0.1f, -1, 0 }, 1.0f, 0.1f },
                          { "elbow", { 0.08f, -0.05f, 1 }, { 0.1f, -1, 0 }, 1.25f, 0.0f },
                          { "wrist", { 0.05f, -0.1f, -1 }, { 0.1f, -1, 0 }, 1.45f, 0.2f } };
    for (Side side : { Side::Left, Side::Right }) {
        const float x = sgn(side);
        for (const Seg& g : segs) {
            const float a = flap * g.lag, back = g.back + sweep;
            const Vector3 openBone = Vector3Normalize({ cosDeg(a), sinDeg(a), -back });
            const Vector3 openChord{ 0, -0.05f, -1 };
            Vector3 bone = Vector3Normalize(Vector3Lerp(g.foldBone, openBone, s));
            Vector3 chord = Vector3Normalize(Vector3Lerp(g.foldChord, openChord, s));
            bone.x *= x; chord.x *= x;               // mirrored for the right wing
            // The joint's frame: x along the bone, outwards (the right wing's bones run along
            // its -x), z against the chord.
            const Vector3 xa = Vector3Scale(bone, x);
            Vector3 za = Vector3Negate(chord);
            za = Vector3Normalize(Vector3Subtract(za, Vector3Scale(xa, Vector3DotProduct(za, xa))));
            const Vector3 ya = Vector3CrossProduct(za, xa);
            const Matrix C = solve(b, p)[(size_t)k["chest"]];
            auto toChar = [&](Vector3 v) { return Vector3Subtract(Vector3Transform(v, C), Vector3Transform({ 0, 0, 0 }, C)); };
            p = orient(b, p, J(k, g.joint, side), toChar(ya), toChar(za));
        }
    }
    return p;
}

// ------------------------------------------------------------------ clips
namespace {
struct BirdLeg { Joint hip, knee, ankle, foot; };
std::array<BirdLeg, 2> legsOf(const Skeleton& k) {
    auto leg = [&](Side s) { return BirdLeg{ J(k, "hip", s), J(k, "knee", s), J(k, "ankle", s), J(k, "foot", s) }; };
    return { leg(Side::Left), leg(Side::Right) };
}
// The foot's joint on `at`, the tarsus raked back `rake` degrees from upright, the toes
// curled down `curl` degrees.
Pose footTo(const Build& b, Pose p, const BirdLeg& l, Vector3 at, float rake, float curl) {
    const Skeleton& k = *b.plan;
    const Vector3 up{ 0, cosDeg(rake), -sinDeg(rake) };
    const Vector3 ankle = Vector3Add(at, Vector3Scale(up, k.dim("tarsus")));
    p = limb(b, p, l.hip, l.knee, l.ankle, ankle, { 0, 0, 1 }, 1);
    p = orient(b, p, l.ankle, up, { 0, sinDeg(rake), cosDeg(rake) });
    return orient(b, p, l.foot, { 0, cosDeg(curl), sinDeg(curl) }, { 0, -sinDeg(curl), cosDeg(curl) });
}
// How far the body sinks so both legs reach feet swept `half` ahead and behind, the
// tarsus raked, the knees a little bent.
float sinkFor(const Skeleton& k, float half, float rake, float bend) {
    const float R = (k.dim("thigh") + k.dim("shank")) * (1 - bend), t = k.dim("tarsus");
    const float dz = half + t * sinDeg(rake);
    const float hip = k.dim("foot") + t * cosDeg(rake) + std::sqrt(std::max(0.f, R * R - dz * dz));
    return k.dim("foot") + k.dim("legLength") - hip;
}
std::array<Vector3, 2> restFeet(const Build& b, const std::array<BirdLeg, 2>& legs) {
    const Frames rest = solve(b, Pose{});
    std::array<Vector3, 2> r;
    for (size_t i = 0; i < 2; i++) {
        const Vector3 q = position(rest, legs[i].foot);
        r[i] = { q.x, b.plan->dim("foot"), q.z };
    }
    return r;
}
struct BipedGait { float duty, stride, pace, lift, tuck, bob; };
BipedGait gaitOf(Gait g) {
    if (g == Gait::Walk) return { 0.62f, 0.8f, 0.32f, 0.1f, 35, 0.012f };
    return { 0.38f, 1.7f, 0.48f, 0.16f, 70, 0.03f };   // a run, whatever the four-legged name
}
// A darting look round: the head holds still, then snaps to a new angle (-1..1).
float darting(double s, float every, int seed) {
    const double k = s / every;
    const int n = (int)std::floor(k);
    const float u = smooth((float)(k - n) / 0.18f);
    auto at = [&](int i) { return hash01(i * 7 + seed) * 2 - 1; };
    return at(n) + (at(n + 1) - at(n)) * u;
}
Joint tailJoint(const Skeleton& k, int i) { return k[std::string("tail-") + char('1' + i)]; }
} // namespace

namespace detail {
bool isBiped(const Build& b) { return b.plan && b.plan->kind == "biped"; }
float bipedCadence(const Build& b, Gait g) {
    const Skeleton& k = *planOf(b, "gaitCadence");
    return gaitOf(g).pace * std::sqrt(9.81f / std::max(0.03f, k.dim("legLength")));
}
float bipedSpeed(const Build& b, Gait g, float cadence) {
    const Skeleton& k = *planOf(b, "gaitSpeed");
    return gaitOf(g).stride * k.dim("legLength") * (cadence > 0 ? cadence : bipedCadence(b, g));
}

Clip bipedStand(const Build& b) {
    const SkeletonPtr k = planOf(b, "clips::stand");
    const auto legs = legsOf(*k);
    const float rake = k->trait("rake"), sink = sinkFor(*k, 0, rake, 0.04f), L = k->dim("legLength");
    const float headPitch = k->trait("headPitch");
    const bool rex = k->variant == "trex";
    const auto feet = restFeet(b, legs);
    return [b, k, legs, rake, sink, L, headPitch, rex, feet](double s) {
        const float t = (float)s;
        Pose p;
        const float breath = std::sin(TAU * t / (rex ? 4.5f : 2.2f));
        p.offset.y = -sink + 0.006f * L * breath;
        p[(*k)["spine"]].x = 1.0f * breath;
        if (rex) {
            // Slow menace: the head sweeping low, the tail swaying against it, now and then a roar.
            const float sway = std::sin(TAU * t / 6.0f);
            p[(*k)["neck-1"]] = { 4 + 3 * breath, 12 * sway, 0 };
            const float roar = smooth(1 - std::fabs(frac(t / 7.0) - 0.5f) / 0.12f);
            p[(*k)["head"]] = { headPitch - 6 - 14 * roar, 8 * sway, 0 };
            p[(*k)["jaw"]].x = 4 + 32 * roar;
            for (int i = 0; i < 3; i++) p[tailJoint(*k, i)].y = -5 * (1 + 0.4f * i) * std::sin(TAU * t / 6.0f - 0.7f * i);
            for (Side sd : { Side::Left, Side::Right }) {
                p[J(*k, "shoulder", sd)] = { -35, 0, 10 };
                p[J(*k, "elbow", sd)] = { -70 + 6 * breath, 0, 0 };
            }
        } else {
            // A bird: the head darts round and holds; the tail flicks.
            const float look = darting(s, 0.9f, k->variant == "duck" ? 3 : 1);
            p[(*k)["neck-1"]] = { 3 * breath, 0, 0 };
            p[(*k)["neck-2"]].y = 20 * look;
            p[(*k)["head"]] = { headPitch + 10 * darting(s + 0.4, 1.3f, 5), 25 * look, 0 };
            p[(*k)["tail-1"]].y = 8 * std::sin(TAU * t * 0.7f) * smooth(std::sin(TAU * t * 0.23f));
            p = wings(b, p, 0, 0, 0);
        }
        for (size_t i = 0; i < 2; i++) p = footTo(b, p, legs[i], feet[i], rake, 0);
        return p;
    };
}

Clip bipedGait(const Build& b, Gait g, float cadence) {
    const SkeletonPtr k = planOf(b, "clips::gait");
    const BipedGait d = gaitOf(g);
    const float c = cadence > 0 ? cadence : bipedCadence(b, g);
    const float L = k->dim("legLength"), sweep = d.stride * L * d.duty, lift = d.lift * L, rake = k->trait("rake");
    const float sink = sinkFor(*k, sweep / 2, rake, 0.04f), speed = d.stride * L * c;
    const auto legs = legsOf(*k);
    const auto home = restFeet(b, legs);
    const bool walk = g == Gait::Walk;
    return [b, k, d, c, L, sweep, lift, rake, sink, speed, legs, home, walk](double s) {
        const float u = frac(s * c);
        const std::string& sp = k->variant;
        Pose p;
        p.offset.y = -sink + d.bob * L * std::cos(2 * TAU * u);
        float headPitch = k->trait("headPitch");
        if (sp == "trex") {
            p[(*k)["pelvis"]] = { 2 * std::sin(2 * TAU * u), 4 * std::sin(TAU * u), 3 * std::sin(TAU * u) };
            for (int i = 0; i < 3; i++) p[tailJoint(*k, i)].y = -6 * (1 + 0.3f * i) * std::sin(TAU * u - 0.6f * i);
            p[(*k)["neck-1"]].y = -3 * std::sin(TAU * u);
            p[(*k)["head"]].x = headPitch - 3 * std::cos(2 * TAU * u);
            for (Side sd : { Side::Left, Side::Right }) {
                p[J(*k, "shoulder", sd)] = { -35, 0, 10 };
                p[J(*k, "elbow", sd)] = { -70, 0, 0 };
            }
        } else {
            // Ducks waddle (the body rolls over the foot that is down); chickens bob the head:
            // it holds still in the world while the body walks under it, then thrusts on.
            const float waddle = sp == "duck" ? 11.f : 4.f;
            p[(*k)["pelvis"]].z = -waddle * std::sin(TAU * u);
            p[(*k)["pelvis"]].y = 0.5f * waddle * std::sin(TAU * u);
            if (sp == "chicken" && walk) {
                const float step = speed / (2 * c), h = frac(2 * u);
                const float dz = h < 0.75f ? step / 2 - step * h / 0.75f : -step / 2 + step * smooth((h - 0.75f) / 0.25f);
                const float a = std::asin(std::clamp(dz / k->dim("neckLength"), -0.9f, 0.9f)) * RAD2DEG;
                p[(*k)["neck-1"]].x = a;
                headPitch -= a;
            }
            p[(*k)["head"]].x = headPitch;
            p[(*k)["tail-1"]].y = 0.6f * waddle * std::sin(TAU * u - 0.8f);
            p = wings(b, p, walk ? 0 : 0.12f, walk ? 0 : 10 * std::sin(2 * TAU * u), 0);
        }
        for (size_t i = 0; i < 2; i++) {
            const float v = frac(u - 0.5f * i);
            Vector3 q = home[i];
            float r = rake, curl = 0;
            if (v < d.duty) {
                q.z += sweep / 2 - sweep * (v / d.duty);
            } else {
                const float w = (v - d.duty) / (1 - d.duty);
                q.z += -sweep / 2 + sweep * smooth(w);
                q.y += lift * std::sin(PI * w);
                r += d.tuck * std::sin(PI * w);
                curl = 40 * std::sin(PI * std::min(1.f, w * 1.4f));
            }
            p = footTo(b, p, legs[i], q, r, curl);
        }
        return p;
    };
}
} // namespace detail

// ------------------------------------------------------------------ flight
namespace {
// In the air: the body levelled, the neck stretched forward, the legs folded up under the tail.
Pose airborne(const Skeleton& k, Pose p, float stretch) {
    p[k["pelvis"]].x = k.trait("bodyPitch");
    p[k["neck-1"]].x = stretch;
    p[k["neck-2"]].x = stretch * 0.4f;
    p[k["head"]].x = k.trait("headPitch") - stretch * 1.3f - k.trait("bodyPitch");
    for (Side s : { Side::Left, Side::Right }) {
        p[J(k, "hip", s)] = { 75, 0, 4 };
        p[J(k, "knee", s)] = { -110, 0, 0 };
        p[J(k, "ankle", s)] = { 110, 0, 0 };
        p[J(k, "foot", s)] = { 70, 0, 0 };
    }
    return p;
}
const Skeleton& wingedPlan(const Build& b, const char* what) {
    const Skeleton& k = *planOf(b, what);
    if (k.trait("wings") < 0.5f) throw std::runtime_error(std::string("rig: ") + what + " needs a bird with wings");
    return k;
}
} // namespace

float flapRate(const Build& b) {
    const Skeleton& k = wingedPlan(b, "flapRate");
    const float span = k.dim("upperArm") + k.dim("forearm") + k.dim("hand");
    return 0.8f * std::sqrt(9.81f / std::max(0.05f, span));
}

namespace clips {
Clip fly(const Build& b, float rate) {
    const Skeleton& k = wingedPlan(b, "clips::fly");
    const float r = rate > 0 ? rate : flapRate(b), L = k.dim("bodyLength");
    const SkeletonPtr plan = b.plan;
    const float stretch = plan->variant == "duck" ? 55.f : 30.f;
    return [b, plan, r, L, stretch](double s) {
        const float ph = TAU * frac(s * r);
        Pose p = airborne(*plan, Pose{}, stretch);
        // The downstroke lifts the body; the wings sweep back a little on the way up.
        p.offset.y = 0.06f * L * std::cos(ph);
        p[(*plan)["pelvis"]].x += 3 * std::sin(ph);
        p[(*plan)["tail-1"]].x = -6 * std::sin(ph);
        return wings(b, p, 1, 48 * std::sin(ph) + 8, 0.12f + 0.12f * std::max(0.f, std::cos(ph)));
    };
}

Clip glide(const Build& b) {
    const Skeleton& k = wingedPlan(b, "clips::glide");
    const SkeletonPtr plan = b.plan;
    const float L = k.dim("bodyLength");
    const float stretch = plan->variant == "duck" ? 55.f : 30.f;
    return [b, plan, L, stretch](double s) {
        const float t = (float)s;
        Pose p = airborne(*plan, Pose{}, stretch);
        p.offset.y = 0.03f * L * std::sin(TAU * t * 0.4f);
        p[(*plan)["pelvis"]].z = 6 * std::sin(TAU * t * 0.25f);          // banking
        return wings(b, p, 1, 6 + 3 * std::sin(TAU * t * 0.7f), 0.08f);
    };
}
} // namespace clips

} // namespace rig
} // namespace rmtn
