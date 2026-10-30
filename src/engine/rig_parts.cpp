// rig:: parts -- the built-in part catalog and the registry.
#include "rig_internal.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <stdexcept>

namespace rmtn {
namespace rig {

namespace {
// ---- shape helpers
Prim super(const char* name, Joint j, Vector3 at, Vector3 size, float sqV, float sqH, Paint paint) {
    Prim p;
    p.name = name; p.joint = j; p.at = at; p.size = size; p.squareV = sqV; p.squareH = sqH; p.paint = paint;
    return p;
}
Prim dome(const char* name, Joint j, Vector3 at, Vector3 size, float from, Paint paint, float sqV = 1, float sqH = 1) {
    Prim p = super(name, j, at, size, sqV, sqH, paint);
    p.from = from;
    return p;
}
Prim outfit(Prim p, Region r, float v0, float v1) {
    p.paint = Paint::Outfit; p.region = r; p.v0 = v0; p.v1 = v1;
    return p;
}
Prim fixed(Prim p, Color c) { p.paint = Paint::Fixed; p.color = c; return p; }
Prim rotated(Prim p, Vector3 r) { p.rot = r; return p; }

// The joint on `side` for a left-side joint.
Joint J(Side side, Joint left) {
    if (side == Side::Left) return left;
    switch (left) {
    case Joint::ShoulderL: return Joint::ShoulderR;
    case Joint::ElbowL: return Joint::ElbowR;
    case Joint::WristL: return Joint::WristR;
    case Joint::HipL: return Joint::HipR;
    case Joint::KneeL: return Joint::KneeR;
    case Joint::AnkleL: return Joint::AnkleR;
    default: return left;
    }
}
float sgn(Side side) { return side == Side::Left ? 1.f : -1.f; }
Region armRegion(Side s) { return s == Side::Left ? Region::ArmL : Region::ArmR; }
Region legRegion(Side s) { return s == Side::Left ? Region::LegL : Region::LegR; }

using Make = std::function<std::vector<Prim>(const Build&, Side, float)>;

std::map<std::string, PartDef>& registry() {
    static std::map<std::string, PartDef> r;
    return r;
}
std::mutex& registryMutex() {
    static std::mutex m;
    return m;
}

void def(const char* name, Slot slot, Make make, std::vector<std::pair<Joint, Vector3>> locks = {}) {
    registry()[name] = PartDef{ name, slot, std::move(make), std::move(locks) };
}

// ---- heads (the head joint sits at the top of the neck; the face is on +z)
void heads() {
    def("head.round", Slot::Head, [](const Build& b, Side, float) {
        const float h = b.head;
        return std::vector<Prim>{
            super("skull", Joint::Head, { 0, h * 0.5f, 0 }, { h * 0.92f, h, h * 0.9f }, 1, 1, Paint::Face),
            super("ear-L", Joint::Head, { h * 0.45f, h * 0.48f, -h * 0.02f }, { h * 0.1f, h * 0.2f, h * 0.12f }, 1, 1, Paint::Skin),
            super("ear-R", Joint::Head, { -h * 0.45f, h * 0.48f, -h * 0.02f }, { h * 0.1f, h * 0.2f, h * 0.12f }, 1, 1, Paint::Skin) };
    });
    def("head.block", Slot::Head, [](const Build& b, Side, float) {
        const float h = b.head;
        return std::vector<Prim>{
            super("skull", Joint::Head, { 0, h * 0.44f, 0 }, { h * 0.86f, h * 0.8f, h * 0.86f }, 0.18f, 1, Paint::Face),
            [&] { Prim stud = super("stud", Joint::Head, { 0, h * 0.88f, 0 }, { h * 0.5f, h * 0.14f, h * 0.5f }, 0.12f, 1, Paint::Skin);
                  stud.depth2d = -h; return stud; }() };   // under any hair in 2D
    });
    def("head.box", Slot::Head, [](const Build& b, Side, float) {
        const float h = b.head;
        return std::vector<Prim>{ super("skull", Joint::Head, { 0, h * 0.5f, 0 }, { h * 0.95f, h * 0.92f, h * 0.92f }, 0.22f, 0.22f, Paint::Face) };
    });
    def("head.egg", Slot::Head, [](const Build& b, Side, float) {
        const float h = b.head;
        Prim p = super("skull", Joint::Head, { 0, h * 0.5f, 0 }, { h * 0.86f, h * 1.02f, h * 0.88f }, 1, 1, Paint::Face);
        p.taper = 1.12f;
        return std::vector<Prim>{ p };
    });
}

// ---- hair and hats
// In 2D, hair that hangs at the back sorts behind the head, and a cap of hair just over the
// skull, with its front edge raised to the brow as it faces the viewer (in 3D the skull's
// front pokes through the cap there); hats sort over both.
Prim behind(Prim p) { p.depth2d = -0.2f; return p; }
Prim capOf(Prim p, float brow) { p.depth2d = 0.05f; p.from2d = brow; return p; }
Prim inFront(Prim p) { p.depth2d = 0.2f; return p; }
void hair() {
    def("hair.short", Slot::Hair, [](const Build& b, Side, float) {
        const float h = b.head;
        return std::vector<Prim>{
            capOf(dome("cap", Joint::Head, { 0, h * 0.56f, -h * 0.05f }, { h * 1.0f, h * 0.92f, h * 0.98f }, -0.05f, Paint::Hair), 0.42f),
            rotated(super("fringe", Joint::Head, { -h * 0.1f, h * 0.9f, h * 0.3f }, { h * 0.55f, h * 0.16f, h * 0.3f }, 1, 1, Paint::Hair), { 18, 0, -10 }) };
    });
    def("hair.bob", Slot::Hair, [](const Build& b, Side, float) {
        const float h = b.head;
        return std::vector<Prim>{
            capOf(dome("cap", Joint::Head, { 0, h * 0.56f, -h * 0.04f }, { h * 1.04f, h * 0.94f, h * 1.0f }, -0.05f, Paint::Hair), 0.42f),
            behind(super("back", Joint::Head, { 0, h * 0.4f, -h * 0.2f }, { h * 1.02f, h * 0.85f, h * 0.62f }, 0.8f, 1, Paint::Hair)),
            super("lock-L", Joint::Head, { h * 0.42f, h * 0.36f, 0 }, { h * 0.2f, h * 0.62f, h * 0.5f }, 0.8f, 1, Paint::Hair),
            super("lock-R", Joint::Head, { -h * 0.42f, h * 0.36f, 0 }, { h * 0.2f, h * 0.62f, h * 0.5f }, 0.8f, 1, Paint::Hair),
            super("fringe", Joint::Head, { 0, h * 0.86f, h * 0.32f }, { h * 0.8f, h * 0.18f, h * 0.3f }, 0.7f, 1, Paint::Hair) };
    });
    def("hair.bun", Slot::Hair, [](const Build& b, Side s, float k) {
        auto v = parts::get("hair.short").make(b, s, k);
        v.push_back(super("bun", Joint::Head, { 0, b.head * 1.02f, -b.head * 0.2f }, Vector3Scale({ 1, 1, 1 }, b.head * 0.38f), 1, 1, Paint::Hair));
        return v;
    });
    def("hair.ponytail", Slot::Hair, [](const Build& b, Side s, float k) {
        auto v = parts::get("hair.short").make(b, s, k);
        const float h = b.head;
        v.push_back(super("tie", Joint::Head, { 0, h * 0.7f, -h * 0.47f }, { h * 0.14f, h * 0.14f, h * 0.14f }, 1, 1, Paint::Accent));
        v.push_back(rotated(super("tail", Joint::Head, { 0, h * 0.42f, -h * 0.6f }, { h * 0.22f, h * 0.62f, h * 0.22f }, 1, 1, Paint::Hair), { -22, 0, 0 }));
        return v;
    });
    def("hair.spiky", Slot::Hair, [](const Build& b, Side, float) {
        const float h = b.head;
        std::vector<Prim> v{ capOf(dome("cap", Joint::Head, { 0, h * 0.56f, -h * 0.05f }, { h * 1.0f, h * 0.92f, h * 0.98f }, -0.05f, Paint::Hair), 0.42f) };
        const float spikes[][3] = { { 0, 1.0f, 0.1f }, { -0.25f, 0.95f, 0.05f }, { 0.25f, 0.95f, 0.05f },
                                    { -0.12f, 0.9f, -0.25f }, { 0.12f, 0.9f, -0.25f }, { 0, 0.85f, 0.3f } };
        for (int i = 0; i < 6; i++) {
            Prim c = super("spike", Joint::Head, { spikes[i][0] * h, spikes[i][1] * h, spikes[i][2] * h }, { h * 0.26f, h * 0.36f, h * 0.26f }, 1, 1, Paint::Hair);
            c.kind = Prim::Kind::Cone;
            c.rot = { spikes[i][2] * 90, 0, -spikes[i][0] * 110 };
            v.push_back(c);
        }
        return v;
    });
    def("hair.mohawk", Slot::Hair, [](const Build& b, Side, float) {
        const float h = b.head;
        return std::vector<Prim>{ super("crest", Joint::Head, { 0, h * 0.92f, -h * 0.04f }, { h * 0.16f, h * 0.42f, h * 0.92f }, 1, 0.9f, Paint::Hair) };
    });
    def("hair.long", Slot::Hair, [](const Build& b, Side s, float k) {
        auto v = parts::get("hair.short").make(b, s, k);
        const float h = b.head;
        v.push_back(behind(super("back", Joint::Head, { 0, h * 0.2f, -h * 0.3f }, { h * 0.96f, h * 1.15f, h * 0.36f }, 0.9f, 1, Paint::Hair)));
        return v;
    });
    def("hair.block", Slot::Hair, [](const Build& b, Side, float) {
        const float h = b.head;
        return std::vector<Prim>{
            capOf(dome("helmet", Joint::Head, { 0, h * 0.6f, -h * 0.05f }, { h * 1.02f, h * 0.76f, h * 1.0f }, -0.25f, Paint::Hair, 0.55f, 1), 0.25f),
            behind(super("back", Joint::Head, { 0, h * 0.36f, -h * 0.26f }, { h * 0.98f, h * 0.6f, h * 0.48f }, 0.4f, 0.9f, Paint::Hair)),
            rotated(super("fringe", Joint::Head, { h * 0.08f, h * 0.84f, h * 0.36f }, { h * 0.86f, h * 0.2f, h * 0.24f }, 0.5f, 0.8f, Paint::Hair), { 14, 0, 6 }) };
    });
    def("hair.cap", Slot::Hair, [](const Build& b, Side, float) {
        const float h = b.head;
        return std::vector<Prim>{ capOf(dome("cap", Joint::Head, { 0, h * 0.68f, -h * 0.04f }, { h * 1.02f, h * 0.62f, h * 0.98f }, -0.1f, Paint::Hair, 0.3f, 0.25f), 0.3f) };
    });

    def("hat.cap", Slot::Hat, [](const Build& b, Side, float) {
        const float h = b.head;
        return std::vector<Prim>{
            inFront(dome("crown", Joint::Head, { 0, h * 0.7f, -h * 0.02f }, { h * 1.06f, h * 0.76f, h * 1.04f }, 0, Paint::Accent)),
            super("brim", Joint::Head, { 0, h * 0.72f, h * 0.44f }, { h * 0.66f, h * 0.05f, h * 0.48f }, 0.5f, 1, Paint::Accent),
            super("button", Joint::Head, { 0, h * 1.08f, -h * 0.02f }, { h * 0.1f, h * 0.06f, h * 0.1f }, 1, 1, Paint::Dark) };
    });
    def("hat.beanie", Slot::Hat, [](const Build& b, Side, float) {
        const float h = b.head;
        return std::vector<Prim>{
            inFront(dome("knit", Joint::Head, { 0, h * 0.64f, -h * 0.03f }, { h * 1.08f, h * 0.92f, h * 1.06f }, -0.12f, Paint::Accent)),
            super("pompom", Joint::Head, { 0, h * 1.12f, -h * 0.03f }, { h * 0.24f, h * 0.24f, h * 0.24f }, 1, 1, Paint::Accent) };
    });
    def("hat.hard", Slot::Hat, [](const Build& b, Side, float) {
        const float h = b.head;
        return std::vector<Prim>{
            fixed(inFront(dome("shell", Joint::Head, { 0, h * 0.72f, 0 }, { h * 1.1f, h * 0.78f, h * 1.12f }, 0, Paint::Fixed)), { 236, 176, 40, 255 }),
            fixed(super("brim", Joint::Head, { 0, h * 0.73f, h * 0.04f }, { h * 1.28f, h * 0.05f, h * 1.3f }, 0.2f, 1, Paint::Fixed), { 236, 176, 40, 255 }) };
    });
    def("hat.top", Slot::Hat, [](const Build& b, Side, float) {
        const float h = b.head;
        return std::vector<Prim>{
            super("crown", Joint::Head, { 0, h * 1.12f, 0 }, { h * 0.72f, h * 0.62f, h * 0.72f }, 0.1f, 1, Paint::Dark),
            super("band", Joint::Head, { 0, h * 0.9f, 0 }, { h * 0.74f, h * 0.1f, h * 0.74f }, 0.1f, 1, Paint::Accent),
            super("brim", Joint::Head, { 0, h * 0.83f, 0 }, { h * 1.15f, h * 0.04f, h * 1.15f }, 0.2f, 1, Paint::Dark) };
    });
    def("hat.antenna", Slot::Hat, [](const Build& b, Side, float) {
        const float h = b.head;
        return std::vector<Prim>{
            super("mast", Joint::Head, { 0, h * 1.08f, 0 }, { h * 0.05f, h * 0.36f, h * 0.05f }, 0.2f, 1, Paint::Metal),
            super("bulb", Joint::Head, { 0, h * 1.28f, 0 }, { h * 0.14f, h * 0.14f, h * 0.14f }, 1, 1, Paint::Accent) };
    });
    def("hat.police", Slot::Hat, [](const Build& b, Side, float) {
        const float h = b.head;
        const Color black{ 26, 26, 32, 255 };
        return std::vector<Prim>{
            inFront(super("crown", Joint::Head, { 0, h * 0.9f, -h * 0.03f }, { h * 1.08f, h * 0.3f, h * 1.08f }, 0.45f, 1, Paint::Dark)),
            inFront(fixed(super("band", Joint::Head, { 0, h * 0.77f, -h * 0.02f }, { h * 0.98f, h * 0.12f, h * 0.98f }, 0.2f, 1, Paint::Fixed), black)),
            fixed(rotated(super("visor", Joint::Head, { 0, h * 0.74f, h * 0.44f }, { h * 0.66f, h * 0.04f, h * 0.32f }, 0.5f, 1, Paint::Fixed), { 14, 0, 0 }), black),
            fixed(super("badge", Joint::Head, { 0, h * 0.9f, h * 0.53f }, { h * 0.13f, h * 0.15f, h * 0.04f }, 0.6f, 0.6f, Paint::Fixed), { 232, 192, 72, 255 }) };
    });
    def("hat.riot", Slot::Hat, [](const Build& b, Side, float) {
        const float h = b.head;
        Prim visor = fixed(super("visor", Joint::Head, { 0, h * 0.5f, h * 0.5f }, { h * 1.0f, h * 0.56f, h * 0.14f }, 0.7f, 0.7f, Paint::Fixed), { 170, 205, 235, 80 });
        visor.depth2d = 0.25f;
        return std::vector<Prim>{
            inFront(dome("helmet", Joint::Head, { 0, h * 0.6f, -h * 0.03f }, { h * 1.14f, h * 0.96f, h * 1.12f }, 0.12f, Paint::Dark)),
            visor };
    });
}

// ---- torsos and hips (the chest joint is the waist; the pelvis joint the hip line)
Prim neck(const Build& b, float r = 0.3f) {
    return super("neck", Joint::Chest, { 0, b.torso.y + b.neck * 0.5f, 0 }, { b.torso.x * r, b.neck + b.head * 0.18f, b.torso.x * r }, 0.4f, 1, Paint::Skin);
}
void torsos() {
    def("torso.round", Slot::Torso, [](const Build& b, Side, float) {
        Prim t = outfit(super("chest", Joint::Chest, { 0, b.torso.y * 0.5f, 0 }, b.torso, 0.5f, 0.75f, Paint::Outfit), Region::Torso, 0, 1);
        t.taper = 0.94f;
        return std::vector<Prim>{ t, neck(b) };
    });
    def("torso.block", Slot::Torso, [](const Build& b, Side, float) {
        Prim t = outfit(super("chest", Joint::Chest, { 0, b.torso.y * 0.5f, 0 }, b.torso, 0.08f, 0.12f, Paint::Outfit), Region::Torso, 0, 1);
        t.taper = 0.7f;
        return std::vector<Prim>{ t, neck(b, 0.26f) };
    });
    def("torso.box", Slot::Torso, [](const Build& b, Side, float) {
        return std::vector<Prim>{ outfit(super("chest", Joint::Chest, { 0, b.torso.y * 0.5f, 0 }, b.torso, 0.2f, 0.2f, Paint::Outfit), Region::Torso, 0, 1), neck(b, 0.28f) };
    });
    def("torso.pear", Slot::Torso, [](const Build& b, Side, float) {
        Prim t = outfit(super("chest", Joint::Chest, { 0, b.torso.y * 0.5f, 0 }, { b.torso.x * 1.1f, b.torso.y, b.torso.z * 1.25f }, 0.7f, 0.85f, Paint::Outfit), Region::Torso, 0, 1);
        t.taper = 0.72f;
        return std::vector<Prim>{ t, neck(b) };
    });

    def("hips.round", Slot::Hips, [](const Build& b, Side, float) {
        return std::vector<Prim>{ outfit(super("pelvis", Joint::Pelvis, { 0, 0, 0 }, { b.torso.x * 0.9f, b.hips * 1.35f, b.torso.z * 0.95f }, 0.6f, 0.8f, Paint::Outfit), Region::Hips, 0, 1) };
    });
    def("hips.block", Slot::Hips, [](const Build& b, Side, float) {
        return std::vector<Prim>{ outfit(super("pelvis", Joint::Pelvis, { 0, 0, 0 }, { b.torso.x, b.hips, b.torso.z }, 0.1f, 0.12f, Paint::Outfit), Region::Hips, 0, 1) };
    });
    def("hips.box", Slot::Hips, [](const Build& b, Side, float) {
        return std::vector<Prim>{ outfit(super("pelvis", Joint::Pelvis, { 0, 0, 0 }, { b.torso.x * 0.95f, b.hips * 1.2f, b.torso.z * 0.95f }, 0.2f, 0.2f, Paint::Outfit), Region::Hips, 0, 1) };
    });
}

// ---- arms (from the shoulder; they hang along -y)
void arms() {
    def("arm.soft", Slot::Arm, [](const Build& b, Side s, float soft) {
        Prim t;
        t.kind = Prim::Kind::Tube; t.name = "arm";
        t.a = J(s, Joint::ShoulderL); t.b = J(s, Joint::ElbowL); t.c = J(s, Joint::WristL); t.joint = t.a;
        t.r0 = b.armRadius * 1.08f; t.r1 = b.armRadius * 0.86f; t.softness = soft;
        return std::vector<Prim>{ outfit(t, armRegion(s), 0, 1) };
    });
    def("arm.hose", Slot::Arm, [](const Build& b, Side s, float) {
        Prim t;
        t.kind = Prim::Kind::Tube; t.name = "arm";
        t.a = J(s, Joint::ShoulderL); t.b = J(s, Joint::ElbowL); t.c = J(s, Joint::WristL); t.joint = t.a;
        t.r0 = t.r1 = b.armRadius * 0.6f; t.softness = 1;
        return std::vector<Prim>{ outfit(t, armRegion(s), 0, 1) };
    });
    auto rigid = [](float sq, bool ball) {
        return [sq, ball](const Build& b, Side s, float) {
            const float r = b.armRadius, L1 = b.upperArm, L2 = b.forearm, vs = L1 / (L1 + L2);
            std::vector<Prim> v{
                outfit(super("upper-arm", J(s, Joint::ShoulderL), { 0, -L1 * 0.45f, 0 }, { r * 2.1f, L1 + r * 1.1f, r * 2.1f }, sq, sq < 0.5f ? sq : 1.f, Paint::Outfit), armRegion(s), 0, vs),
                outfit(super("forearm", J(s, Joint::ElbowL), { 0, -L2 * 0.5f, 0 }, { r * 1.85f, L2 + r * 0.7f, r * 1.85f }, sq, sq < 0.5f ? sq : 1.f, Paint::Outfit), armRegion(s), vs, 1) };
            if (ball) v.push_back(outfit(super("elbow", J(s, Joint::ElbowL), { 0, 0, 0 }, Vector3Scale({ 1, 1, 1 }, r * 2), 1, 1, Paint::Outfit), armRegion(s), vs, vs));
            return v;
        };
    };
    def("arm.rigid", Slot::Arm, rigid(0.6f, true));
    def("arm.box", Slot::Arm, rigid(0.2f, false));
    // A brick figure's arm: one moulded piece, bent at a fixed elbow.
    def("arm.block", Slot::Arm, [](const Build& b, Side s, float) {
        const float r = b.armRadius, L1 = b.upperArm, L2 = b.forearm, vs = L1 / (L1 + L2);
        const float bend = 35 * DEG2RAD;
        const Vector3 fc{ 0, -L1 - L2 * 0.42f * std::cos(bend), L2 * 0.42f * std::sin(bend) };
        return std::vector<Prim>{
            outfit(super("upper-arm", J(s, Joint::ShoulderL), { 0, -L1 * 0.45f, 0 }, { r * 1.9f, L1 * 1.1f, r * 2.0f }, 0.45f, 0.85f, Paint::Outfit), armRegion(s), 0, vs),
            outfit(rotated(super("forearm", J(s, Joint::ShoulderL), fc, { r * 1.7f, L2 * 0.95f, r * 1.8f }, 0.4f, 0.85f, Paint::Outfit), { -35, 0, 0 }), armRegion(s), vs, 1) };
    }, { { Joint::ElbowL, { -35, 0, 0 } } });
    def("arm.robot", Slot::Arm, [](const Build& b, Side s, float) {
        const float r = b.armRadius, L1 = b.upperArm, L2 = b.forearm;
        return std::vector<Prim>{
            super("shoulder", J(s, Joint::ShoulderL), { 0, 0, 0 }, Vector3Scale({ 1, 1, 1 }, r * 2.4f), 1, 1, Paint::Dark),
            super("upper-arm", J(s, Joint::ShoulderL), { 0, -L1 * 0.5f, 0 }, { r * 1.3f, L1, r * 1.3f }, 0.12f, 1, Paint::Metal),
            super("elbow", J(s, Joint::ElbowL), { 0, 0, 0 }, Vector3Scale({ 1, 1, 1 }, r * 1.9f), 1, 1, Paint::Dark),
            super("forearm", J(s, Joint::ElbowL), { 0, -L2 * 0.5f, 0 }, { r * 1.5f, L2 * 0.9f, r * 1.5f }, 0.3f, 0.3f, Paint::Metal) };
    });
}

// ---- hands (from the wrist; palm inwards, thumb forwards)
void hands() {
    def("hand.mitten", Slot::Hand, [](const Build& b, Side s, float) {
        const float h = b.hand;
        return std::vector<Prim>{
            super("palm", J(s, Joint::WristL), { 0, -h * 0.5f, 0 }, { h * 0.62f, h * 1.05f, h * 0.9f }, 0.8f, 1, Paint::Skin),
            rotated(super("thumb", J(s, Joint::WristL), { 0, -h * 0.32f, h * 0.38f }, { h * 0.3f, h * 0.5f, h * 0.3f }, 1, 1, Paint::Skin), { -40, 0, 0 }) };
    });
    def("hand.glove", Slot::Hand, [](const Build& b, Side s, float) {
        const float h = b.hand;
        const Color white{ 246, 246, 242, 255 };
        return std::vector<Prim>{
            fixed(super("cuff", J(s, Joint::WristL), { 0, -h * 0.05f, 0 }, { h * 0.8f, h * 0.3f, h * 0.8f }, 0.5f, 1, Paint::Fixed), white),
            fixed(super("palm", J(s, Joint::WristL), { 0, -h * 0.58f, 0 }, { h * 0.75f, h * 1.1f, h * 1.0f }, 1, 1, Paint::Fixed), white),
            fixed(rotated(super("thumb", J(s, Joint::WristL), { 0, -h * 0.38f, h * 0.45f }, { h * 0.34f, h * 0.55f, h * 0.34f }, 1, 1, Paint::Fixed), { -40, 0, 0 }), white) };
    });
    def("hand.fingers", Slot::Hand, [](const Build& b, Side s, float) {
        const float h = b.hand;
        std::vector<Prim> v{ super("palm", J(s, Joint::WristL), { 0, -h * 0.32f, 0 }, { h * 0.42f, h * 0.62f, h * 0.85f }, 0.7f, 0.9f, Paint::Skin) };
        for (int i = 0; i < 4; i++) {
            const float z = (-0.3f + 0.2f * i) * h;
            v.push_back(rotated(super("finger", J(s, Joint::WristL), { 0, -h * (0.78f - 0.04f * std::fabs(i - 1.5f)), z },
                                      { h * 0.17f, h * (0.5f - 0.05f * std::fabs(i - 1.8f)), h * 0.17f }, 1, 1, Paint::Skin), { -8.f * i + 10, 0, 0 }));
        }
        v.push_back(rotated(super("thumb", J(s, Joint::WristL), { -sgn(s) * h * 0.05f, -h * 0.3f, h * 0.42f }, { h * 0.18f, h * 0.42f, h * 0.18f }, 1, 1, Paint::Skin), { -55, 0, 0 }));
        return v;
    });
    def("hand.clip", Slot::Hand, [](const Build& b, Side s, float) {
        const float h = b.hand;
        return std::vector<Prim>{
            super("wrist", J(s, Joint::WristL), { 0, -h * 0.05f, 0 }, { h * 0.42f, h * 0.3f, h * 0.42f }, 0.2f, 1, Paint::Skin),
            rotated(super("clip", J(s, Joint::WristL), { 0, -h * 0.5f, h * 0.05f }, { h * 0.75f, h * 0.55f, h * 0.9f }, 0.35f, 1, Paint::Skin), { 90, 0, 0 }),
            rotated(super("slot", J(s, Joint::WristL), { 0, -h * 0.68f, h * 0.05f }, { h * 0.3f, h * 0.6f, h * 0.5f }, 0.3f, 1, Paint::Dark), { 90, 0, 0 }) };
    });
    def("hand.box", Slot::Hand, [](const Build& b, Side s, float) {
        const float h = b.hand;
        return std::vector<Prim>{ super("palm", J(s, Joint::WristL), { 0, -h * 0.45f, 0 }, { h * 0.9f, h * 0.9f, h * 0.9f }, 0.25f, 0.25f, Paint::Skin) };
    });
    def("hand.claw", Slot::Hand, [](const Build& b, Side s, float) {
        const float h = b.hand;
        return std::vector<Prim>{
            super("palm", J(s, Joint::WristL), { 0, -h * 0.2f, 0 }, Vector3Scale({ 1, 1, 1 }, h * 0.6f), 1, 1, Paint::Dark),
            rotated(super("finger", J(s, Joint::WristL), { 0, -h * 0.65f, h * 0.18f }, { h * 0.22f, h * 0.75f, h * 0.24f }, 0.3f, 0.5f, Paint::Metal), { -18, 0, 0 }),
            rotated(super("finger", J(s, Joint::WristL), { 0, -h * 0.65f, -h * 0.18f }, { h * 0.22f, h * 0.75f, h * 0.24f }, 0.3f, 0.5f, Paint::Metal), { 18, 0, 0 }) };
    });
    def("hand.ball", Slot::Hand, [](const Build& b, Side s, float) {
        return std::vector<Prim>{ super("palm", J(s, Joint::WristL), { 0, -b.hand * 0.42f, 0 }, Vector3Scale({ 1, 1, 1 }, b.hand * 0.85f), 1, 1, Paint::Skin) };
    });
}

// ---- legs (from the hip) and feet (from the ankle; toes towards +z)
void legs() {
    def("leg.soft", Slot::Leg, [](const Build& b, Side s, float soft) {
        Prim t;
        t.kind = Prim::Kind::Tube; t.name = "leg";
        t.a = J(s, Joint::HipL); t.b = J(s, Joint::KneeL); t.c = J(s, Joint::AnkleL); t.joint = t.a;
        t.r0 = b.legRadius * 1.12f; t.r1 = b.legRadius * 0.84f; t.softness = soft;
        return std::vector<Prim>{ outfit(t, legRegion(s), 0, 1) };
    });
    auto rigid = [](float sq, bool ball) {
        return [sq, ball](const Build& b, Side s, float) {
            const float r = b.legRadius, L1 = b.thigh, L2 = b.shin, vs = L1 / (L1 + L2);
            std::vector<Prim> v{
                outfit(super("thigh", J(s, Joint::HipL), { 0, -L1 * 0.46f, 0 }, { r * 2.15f, L1 + r * 0.9f, r * 2.15f }, sq, sq < 0.5f ? sq : 1.f, Paint::Outfit), legRegion(s), 0, vs),
                outfit(super("shin", J(s, Joint::KneeL), { 0, -L2 * 0.5f, 0 }, { r * 1.85f, L2 + r * 0.6f, r * 1.85f }, sq, sq < 0.5f ? sq : 1.f, Paint::Outfit), legRegion(s), vs, 1) };
            if (ball) v.push_back(outfit(super("knee", J(s, Joint::KneeL), { 0, 0, 0 }, Vector3Scale({ 1, 1, 1 }, r * 2), 1, 1, Paint::Outfit), legRegion(s), vs, vs));
            return v;
        };
    };
    def("leg.rigid", Slot::Leg, rigid(0.6f, true));
    def("leg.box", Slot::Leg, rigid(0.2f, false));
    // A brick figure's leg: one block from hip to ground, foot moulded on, knee straight.
    def("leg.block", Slot::Leg, [](const Build& b, Side s, float) {
        const float r = b.legRadius, L = b.thigh + b.shin, total = L + b.ankle;
        return std::vector<Prim>{
            outfit(super("leg", J(s, Joint::HipL), { 0, -L * 0.5f + r * 0.2f, 0 }, { r * 1.9f, L + r * 0.4f, r * 1.8f }, 0.08f, 0.12f, Paint::Outfit), legRegion(s), 0, 0.82f),
            outfit(super("foot", J(s, Joint::HipL), { 0, -total + b.ankle * 0.9f, b.foot * 0.18f }, { r * 1.9f, b.ankle * 1.8f, b.foot }, 0.1f, 0.15f, Paint::Outfit), legRegion(s), 0.82f, 1) };
    }, { { Joint::KneeL, { 0, 0, 0 } }, { Joint::AnkleL, { 0, 0, 0 } } });
    def("leg.robot", Slot::Leg, [](const Build& b, Side s, float) {
        const float r = b.legRadius, L1 = b.thigh, L2 = b.shin;
        return std::vector<Prim>{
            super("hip", J(s, Joint::HipL), { 0, 0, 0 }, Vector3Scale({ 1, 1, 1 }, r * 2.2f), 1, 1, Paint::Dark),
            super("thigh", J(s, Joint::HipL), { 0, -L1 * 0.5f, 0 }, { r * 1.4f, L1, r * 1.4f }, 0.12f, 1, Paint::Metal),
            super("knee", J(s, Joint::KneeL), { 0, 0, 0 }, Vector3Scale({ 1, 1, 1 }, r * 1.9f), 1, 1, Paint::Dark),
            super("shin", J(s, Joint::KneeL), { 0, -L2 * 0.5f, 0 }, { r * 1.7f, L2 * 0.92f, r * 1.7f }, 0.3f, 0.3f, Paint::Metal) };
    });
    def("leg.peg", Slot::Leg, [](const Build& b, Side s, float) {
        const float r = b.legRadius, L1 = b.thigh, L2 = b.shin + b.ankle;
        Prim peg = fixed(super("peg", J(s, Joint::KneeL), { 0, -L2 * 0.5f, 0 }, { r * 0.9f, L2, r * 0.9f }, 0.15f, 1, Paint::Fixed), { 140, 96, 56, 255 });
        peg.taper = 1.6f;
        return std::vector<Prim>{
            outfit(super("thigh", J(s, Joint::HipL), { 0, -L1 * 0.46f, 0 }, { r * 2.15f, L1 + r * 0.9f, r * 2.15f }, 0.6f, 1, Paint::Outfit), legRegion(s), 0, 0.5f),
            peg };
    });

    def("foot.shoe", Slot::Foot, [](const Build& b, Side s, float) {
        const float r = b.legRadius, f = b.foot, a = b.ankle;
        return std::vector<Prim>{
            super("shoe", J(s, Joint::AnkleL), { 0, -a * 0.4f, f * 0.26f }, { r * 2.1f, a * 1.5f, f }, 0.5f, 0.8f, Paint::Shoes),
            super("sole", J(s, Joint::AnkleL), { 0, -a * 0.92f, f * 0.26f }, { r * 2.2f, a * 0.32f, f * 1.03f }, 0.25f, 0.8f, Paint::Dark) };
    });
    def("foot.boot", Slot::Foot, [](const Build& b, Side s, float) {
        const float r = b.legRadius, f = b.foot, a = b.ankle;
        return std::vector<Prim>{
            super("shaft", J(s, Joint::AnkleL), { 0, a * 0.8f, 0 }, { r * 2.25f, a * 2.6f, r * 2.25f }, 0.3f, 1, Paint::Shoes),
            super("boot", J(s, Joint::AnkleL), { 0, -a * 0.35f, f * 0.22f }, { r * 2.3f, a * 1.5f, f * 1.05f }, 0.35f, 0.7f, Paint::Shoes),
            super("sole", J(s, Joint::AnkleL), { 0, -a * 0.92f, f * 0.22f }, { r * 2.4f, a * 0.3f, f * 1.08f }, 0.2f, 0.7f, Paint::Dark) };
    });
    def("foot.bare", Slot::Foot, [](const Build& b, Side s, float) {
        const float r = b.legRadius, f = b.foot, a = b.ankle;
        std::vector<Prim> v{ super("sole", J(s, Joint::AnkleL), { 0, -a * 0.45f, f * 0.2f }, { r * 1.9f, a * 1.15f, f * 0.92f }, 0.8f, 0.9f, Paint::Skin) };
        // Five toes, the big one on the inside (towards the body: -x for the left foot).
        const float inner = -sgn(s);
        for (int i = 0; i < 5; i++) {
            const float k = i == 0 ? 1.0f : 0.72f - 0.06f * i;
            const float x = inner * (0.62f - 0.31f * i) * r;
            v.push_back(super("toe", J(s, Joint::AnkleL), { x, -a * 0.7f, f * (0.66f - 0.035f * i) }, Vector3Scale({ 1, 0.8f, 1.1f }, r * 0.42f * k), 1, 1, Paint::Skin));
        }
        return v;
    });
    def("foot.block", Slot::Foot, [](const Build& b, Side s, float) {
        const float r = b.legRadius, f = b.foot, a = b.ankle;
        return std::vector<Prim>{ super("shoe", J(s, Joint::AnkleL), { 0, -a * 0.35f, f * 0.2f }, { r * 2.0f, a * 1.5f, f * 0.95f }, 0.2f, 0.2f, Paint::Shoes) };
    });
    def("foot.flipper", Slot::Foot, [](const Build& b, Side s, float) {
        const float r = b.legRadius, f = b.foot, a = b.ankle;
        Prim fin = fixed(super("fin", J(s, Joint::AnkleL), { 0, -a * 0.8f, f * 0.55f }, { r * 3.2f, a * 0.35f, f * 1.7f }, 0.4f, 0.9f, Paint::Fixed), { 60, 170, 120, 255 });
        fin.taper = 1.0f;
        return std::vector<Prim>{ fixed(super("heel", J(s, Joint::AnkleL), { 0, -a * 0.35f, f * 0.05f }, { r * 2.1f, a * 1.3f, f * 0.6f }, 0.6f, 0.9f, Paint::Fixed), { 60, 170, 120, 255 }), fin };
    });

    def("extra.backpack", Slot::Extra, [](const Build& b, Side, float) {
        return std::vector<Prim>{
            super("pack", Joint::Chest, { 0, b.torso.y * 0.5f, -b.torso.z * 0.5f - b.torso.z * 0.32f }, { b.torso.x * 0.72f, b.torso.y * 0.78f, b.torso.z * 0.62f }, 0.35f, 0.4f, Paint::Accent),
            super("flap", Joint::Chest, { 0, b.torso.y * 0.72f, -b.torso.z * 0.5f - b.torso.z * 0.62f }, { b.torso.x * 0.6f, b.torso.y * 0.3f, b.torso.z * 0.12f }, 0.3f, 0.4f, Paint::Dark) };
    });
}

// ---- held props (in the wrist's frame: the palm's centre at (0, -hand/2, 0), the palm
// facing -x on the left hand and +x on the right, the thumb towards +z) and rides (on the
// Floor joint). Sizes follow the body (detail::propScale), so every build can use them.
Prim cylinderAlong(Prim p, Vector3 rot) { p.squareV = 0.12f; p.squareH = 1; p.rot = rot; return p; }
float palmSide(Side s) { return s == Side::Left ? -1.f : 1.f; }
void props() {
    using namespace detail;
    def("prop.phone", Slot::Hold, [](const Build& b, Side s, float) {
        const float h = b.hand, k = propScale(b), t = 0.009f * k, pn = palmSide(s);
        return std::vector<Prim>{
            super("phone", J(s, Joint::WristL), { pn * (h * 0.31f + t / 2), -h * 0.55f, 0 }, { t, 0.15f * k, 0.072f * k }, 0.15f, 0.2f, Paint::Dark),
            fixed(super("screen", J(s, Joint::WristL), { pn * (h * 0.31f + t + 0.001f * k), -h * 0.55f, 0 }, { 0.002f * k, 0.13f * k, 0.062f * k }, 0.1f, 0.2f, Paint::Fixed), { 96, 196, 236, 255 }) };
    });
    def("prop.tablet", Slot::Hold, [](const Build& b, Side s, float) {
        const float h = b.hand, k = propScale(b), t = 0.011f * k, pn = palmSide(s);
        const float y = -h * 0.5f - 0.06f * k;
        return std::vector<Prim>{
            super("tablet", J(s, Joint::WristL), { pn * (h * 0.31f + t / 2), y, 0 }, { t, 0.25f * k, 0.18f * k }, 0.12f, 0.12f, Paint::Dark),
            fixed(super("screen", J(s, Joint::WristL), { pn * (h * 0.31f + t + 0.001f * k), y, 0 }, { 0.002f * k, 0.225f * k, 0.155f * k }, 0.1f, 0.1f, Paint::Fixed), { 120, 205, 225, 255 }) };
    });
    def("prop.sign", Slot::Hold, [](const Build& b, Side s, float) {
        // A pole through the fist (the thumb runs along it) and a card at its top, facing the
        // way the fingers point.
        const SignGeom g = signGeom(b);
        const float h = b.hand, k = propScale(b), x = palmSide(s) * h * 0.3f, t = 0.014f * k;
        Prim card = super("card", J(s, Joint::WristL), { x, -h * 0.5f - (g.radius + t / 2 + 0.003f * k), g.pole - g.grip - g.height / 2 + 0.02f * k },
                          { g.width, g.height, t }, 0.1f, 0.1f, Paint::Label);
        card.color = { 247, 243, 232, 255 };
        card.rot = { 90, 0, 0 };
        card.depth2d = 0.03f;                           // in 2D, over its own pole
        return std::vector<Prim>{
            fixed(cylinderAlong(super("pole", J(s, Joint::WristL), { x, -h * 0.5f, g.pole / 2 - g.grip }, { g.radius * 2, g.pole, g.radius * 2 }, 1, 1, Paint::Fixed), { 90, 0, 0 }), { 176, 132, 84, 255 }),
            card };
    });
    def("prop.shield", Slot::Hold, [](const Build& b, Side s, float) {
        const ShieldGeom g = shieldGeom(b);
        const float h = b.hand, k = propScale(b), x = palmSide(s) * h * 0.3f, t = 0.016f * k;
        Prim shield = super("shield", J(s, Joint::WristL), { x - sgn(s) * 0.05f * k, -h * 0.5f - (0.04f * k + t / 2), g.height / 2 - g.below },
                            { g.width, g.height, t }, 0.08f, 0.08f, Paint::Label);
        shield.color = { 120, 140, 160, 110 };          // smoked polycarbonate
        shield.rot = { 90, 0, 0 };
        shield.depth2d = 0.15f;                         // in 2D, in front of the arm that holds it
        return std::vector<Prim>{
            super("handle", J(s, Joint::WristL), { x, -h * 0.5f - 0.02f * k, 0 }, { 0.025f * k, 0.03f * k, 0.15f * k }, 0.3f, 0.3f, Paint::Dark),
            shield };
    });
    def("prop.baton", Slot::Hold, [](const Build& b, Side s, float) {
        const float h = b.hand, k = propScale(b), len = 0.62f * k;
        return std::vector<Prim>{
            cylinderAlong(super("baton", J(s, Joint::WristL), { palmSide(s) * h * 0.3f, -h * 0.5f, len / 2 - 0.1f * k }, { 0.034f * k, len, 0.034f * k }, 1, 1, Paint::Dark), { 90, 0, 0 }) };
    });

    def("ride.skateboard", Slot::Ride, [](const Build& b, Side, float) {
        const BoardGeom g = boardGeom(b);
        const float k = propScale(b), L = g.length, W = g.width;
        std::vector<Prim> v{
            super("deck", Joint::Floor, { 0, g.top - 0.008f * k, 0 }, { L, 0.016f * k, W }, 0.3f, 0.35f, Paint::Accent),
            super("grip-tape", Joint::Floor, { 0, g.top + 0.0005f * k, 0 }, { L * 0.92f, 0.003f * k, W * 0.88f }, 0.2f, 0.35f, Paint::Dark) };
        for (float sx : { -1.f, 1.f }) {
            v.push_back(super("truck", Joint::Floor, { sx * 0.3f * L, (g.top - 0.016f * k + g.wheel * 1.6f) / 2, 0 }, { 0.05f * k, 0.035f * k, W * 0.8f }, 0.4f, 0.4f, Paint::Metal));
            for (float sz : { -1.f, 1.f })
                v.push_back(fixed(cylinderAlong(super("wheel", Joint::Floor, { sx * 0.3f * L, g.wheel, sz * (W * 0.5f - 0.012f * k) }, { g.wheel * 2, 0.022f * k, g.wheel * 2 }, 1, 1, Paint::Fixed), { 90, 0, 0 }), { 240, 232, 210, 255 }));
        }
        return v;
    });
    def("ride.scooter", Slot::Ride, [](const Build& b, Side, float) {
        const ScooterGeom g = scooterGeom(b);
        const float k = propScale(b), w = g.wheel;
        const Color tyre{ 52, 52, 58, 255 };
        std::vector<Prim> v{
            super("deck", Joint::Floor, { 0, g.top - 0.015f * k, -0.02f * k }, { g.width, 0.03f * k, g.length }, 0.3f, 0.3f, Paint::Dark),
            fixed(cylinderAlong(super("wheel", Joint::Floor, { 0, w, -g.length / 2 - w * 0.3f }, { w * 2, 0.03f * k, w * 2 }, 1, 1, Paint::Fixed), { 0, 0, 90 }), tyre),
            fixed(cylinderAlong(super("wheel", Joint::Floor, { 0, w, g.barZ + 0.01f * k }, { w * 2, 0.03f * k, w * 2 }, 1, 1, Paint::Fixed), { 0, 0, 90 }), tyre),
            super("neck", Joint::Floor, { 0, g.top, g.length / 2 }, { g.width * 0.8f, 0.04f * k, 0.09f * k }, 0.3f, 0.3f, Paint::Accent),
            cylinderAlong(super("stem", Joint::Floor, { 0, (w + g.barY) / 2, g.barZ }, { 0.034f * k, g.barY - w, 0.034f * k }, 1, 1, Paint::Metal), { 0, 0, 0 }),
            cylinderAlong(super("bar", Joint::Floor, { 0, g.barY, g.barZ }, { 0.028f * k, g.barW, 0.028f * k }, 1, 1, Paint::Metal), { 0, 0, 90 }) };
        for (float sx : { -1.f, 1.f })
            v.push_back(cylinderAlong(super("grip", Joint::Floor, { sx * (g.barW / 2 - 0.05f * k), g.barY, g.barZ }, { 0.036f * k, 0.1f * k, 0.036f * k }, 1, 1, Paint::Dark), { 0, 0, 90 }));
        return v;
    });
}

void ensureBuiltins() {
    static bool done = false;
    if (done) return;
    done = true;
    heads(); hair(); torsos(); arms(); hands(); legs(); props();
}
} // namespace

namespace parts {
void add(PartDef d) {
    std::lock_guard<std::mutex> lock(registryMutex());
    ensureBuiltins();
    registry()[d.name] = std::move(d);
}
const PartDef& get(const std::string& n) {
    std::lock_guard<std::mutex> lock(registryMutex());
    ensureBuiltins();
    auto it = registry().find(n);
    if (it == registry().end()) throw std::runtime_error("rig: unknown part \"" + n + "\"");
    return it->second;
}
bool has(const std::string& n) {
    std::lock_guard<std::mutex> lock(registryMutex());
    ensureBuiltins();
    return registry().count(n) > 0;
}
std::vector<std::string> names(Slot slot) {
    std::lock_guard<std::mutex> lock(registryMutex());
    ensureBuiltins();
    std::vector<std::string> out;
    for (const auto& [n, d] : registry()) if (d.slot == slot) out.push_back(n);
    return out;
}
PartDef model(const std::string& name, Slot slot, const std::string& file, Joint joint, Vector3 at, Vector3 rot,
              float scale, const std::string& svg, Vector2 size) {
    PartDef d;
    d.name = name;
    d.slot = slot;
    d.make = [=](const Build&, Side side, float) {
        Prim p;
        p.kind = Prim::Kind::Model; p.name = name;
        p.joint = J(side, joint);
        p.at = side == Side::Right ? Vector3{ -at.x, at.y, at.z } : at;
        p.rot = side == Side::Right ? Vector3{ rot.x, -rot.y, -rot.z } : rot;
        p.size = { scale, scale, scale };
        p.model = file; p.svg = svg; p.flat = size;
        return std::vector<Prim>{ p };
    };
    return d;
}
} // namespace parts

std::vector<Prim> prims(const Look& l, const std::string& slot) {
    for (const auto& [key, part] : slots(l)) {
        if (key != slot) continue;
        if (part.empty()) return {};
        const Side side = key.back() == 'R' ? Side::Right : Side::Left;
        auto v = parts::get(part).make(l.build, side, std::clamp(l.softness, 0.f, 1.f));
        for (Prim& p : v)
            if (p.paint == Paint::Label && p.label.empty()) p.label = l.sign;
        return v;
    }
    return {};
}

std::vector<Prim> prims(const Look& l) {
    std::vector<Prim> out;
    for (const auto& [key, part] : slots(l)) {
        auto v = prims(l, key);
        out.insert(out.end(), v.begin(), v.end());
    }
    return out;
}

Pose locked(const Look& l, Pose p) {
    for (const auto& [key, part] : slots(l)) {
        if (part.empty()) continue;
        const Side side = key.back() == 'R' ? Side::Right : Side::Left;
        for (const auto& [j, r] : parts::get(part).locks) p[J(side, j)] = r;
    }
    return p;
}

} // namespace rig
} // namespace rmtn
