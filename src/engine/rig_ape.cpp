// rig:: apes -- chimpanzee, gorilla and orangutan on the humanoid skeleton (long arms,
// short legs, fur outfits, ape heads), and three humanoid clips that suit them and work on
// anyone: knuckle-walking, hanging from a bar, and swinging hand over hand along bars.
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
Prim super(const char* name, Joint j, Vector3 at, Vector3 size, Paint paint) {
    Prim p;
    p.name = name; p.joint = j; p.at = at; p.size = size; p.paint = paint;
    return p;
}

// An ape's head: a furred skull, a bare face with the features on it, a jutting muzzle and
// a brow ridge. The head joint sits at the top of the neck; the face is on +z.
std::vector<Prim> apeHead(const Build& b, const std::string& kind) {
    const float h = b.head;
    std::vector<Prim> v{
        super("skull", Joint::Head, { 0, h * 0.52f, -h * 0.04f }, { h * 0.92f, h * 0.95f, h * 0.88f }, Paint::Hair),
        super("face", Joint::Head, { 0, h * 0.46f, h * 0.2f }, { h * 0.74f, h * 0.72f, h * 0.5f }, Paint::Face),
        super("muzzle", Joint::Head, { 0, h * 0.24f, h * 0.36f }, { h * 0.56f, h * 0.36f, h * 0.36f }, Paint::Skin),
        super("brow", Joint::Head, { 0, h * 0.66f, h * 0.36f }, { h * 0.7f, h * 0.13f, h * 0.2f }, Paint::Skin) };
    const float earSize = kind == "chimp" ? 0.26f : 0.14f;
    for (float sx : { -1.f, 1.f })
        v.push_back(super("ear", Joint::Head, { sx * h * 0.47f, h * 0.5f, 0 }, { h * 0.08f, h * earSize, h * earSize * 0.8f }, Paint::Skin));
    if (kind == "gorilla") v.push_back(super("crest", Joint::Head, { 0, h * 0.9f, -h * 0.12f }, { h * 0.4f, h * 0.38f, h * 0.6f }, Paint::Hair));
    if (kind == "orangutan")
        for (float sx : { -1.f, 1.f }) {
            Prim pad = super("cheek-pad", Joint::Head, { sx * h * 0.45f, h * 0.42f, h * 0.18f }, { h * 0.34f, h * 0.62f, h * 0.12f }, Paint::Skin);
            pad.rot = { 0, sx * 30, 0 };
            v.push_back(pad);
        }
    return v;
}
} // namespace

namespace detail {
void apeParts(std::map<std::string, PartDef>& reg) {
    for (const char* k : { "chimp", "gorilla", "orangutan" }) {
        const std::string kind = k;
        reg["head.ape." + kind] = PartDef{ "head.ape." + kind, Slot::Head,
                                           [kind](const Build& b, Side, float) { return apeHead(b, kind); }, {} };
    }
}

bool apeLook(const std::string& species, Look& l) {
    if (species != "chimp" && species != "gorilla" && species != "orangutan") return false;
    Build b;
    b.head = 0.26f; b.neck = 0.03f; b.torso = { 0.34f, 0.42f, 0.24f }; b.hips = 0.14f; b.hipWidth = 0.17f;
    b.upperArm = 0.33f; b.forearm = 0.31f; b.armRadius = 0.06f; b.hand = 0.12f;
    b.thigh = 0.22f; b.shin = 0.2f; b.legRadius = 0.07f; b.foot = 0.2f; b.ankle = 0.05f;
    Color fur{ 40, 34, 32, 255 }, skin{ 196, 160, 132, 255 };
    if (species == "gorilla") {
        b.head = 0.3f; b.torso = { 0.56f, 0.55f, 0.36f }; b.hips = 0.18f; b.hipWidth = 0.26f;
        b.upperArm = 0.42f; b.forearm = 0.4f; b.armRadius = 0.1f; b.hand = 0.15f;
        b.thigh = 0.3f; b.shin = 0.26f; b.legRadius = 0.1f; b.foot = 0.26f; b.ankle = 0.06f;
        fur = { 34, 32, 34, 255 }; skin = { 52, 46, 46, 255 };
    } else if (species == "orangutan") {
        b.head = 0.28f; b.torso = { 0.4f, 0.42f, 0.28f }; b.hips = 0.15f;
        b.upperArm = 0.42f; b.forearm = 0.4f; b.armRadius = 0.065f; b.hand = 0.14f;
        b.thigh = 0.2f; b.shin = 0.18f;
        fur = { 176, 82, 34, 255 }; skin = { 104, 76, 62, 255 };
    }
    l = Look{};
    l.build = b;
    l.head = "head.ape." + species;
    l.hair = ""; l.hat = "";
    l.torso = "torso.round"; l.hips = "hips.round";
    l.arms("arm.soft").hands("hand.mitten").legs("leg.soft").feet("foot.bare");
    l.skin = skin;
    l.hairColor = fur;
    l.outfit = Outfit::basic(fur, fur, skin, 1.0f, 1.0f);
    l.softness = 0.9f;
    FaceStyle fs;
    fs.eyes = FaceStyle::Eyes::Dots;
    fs.ink = { 20, 16, 16, 255 };
    fs.browWeight = 0;
    fs.brows = false;
    fs.eyeSpacing = 0.8f;
    l.face = simpleFace(fs);
    l.faceBox = { 0.18f, 0.22f, 0.64f, 0.5f };
    return true;
}
} // namespace detail

// ------------------------------------------------------------------ clips
namespace {
float sgnOf(Side s) { return s == Side::Left ? 1.f : -1.f; }
Joint hipOf(Side s) { return s == Side::Left ? Joint::HipL : Joint::HipR; }
Joint kneeOf(Side s) { return s == Side::Left ? Joint::KneeL : Joint::KneeR; }
Joint shoulderOf(Side s) { return s == Side::Left ? Joint::ShoulderL : Joint::ShoulderR; }
Joint elbowOf(Side s) { return s == Side::Left ? Joint::ElbowL : Joint::ElbowR; }
Joint ankleOf(Side s) { return s == Side::Left ? Joint::AnkleL : Joint::AnkleR; }
// A paw-like cycle: down (sweeping back) for `duty` of the cycle, then a lifted swing forward.
Vector3 stepAt(Vector3 home, float v, float duty, float sweep, float lift) {
    if (v < duty) return { home.x, home.y, home.z + sweep / 2 - sweep * (v / duty) };
    const float w = (v - duty) / (1 - duty);
    return { home.x, home.y + lift * std::sin(PI * w), home.z - sweep / 2 + sweep * smooth(w) };
}
constexpr float kKnuckleDuty = 0.65f;
// Knuckle-walking's shape for a build: how far the body leans forward from the hips (until
// the knuckles reach the floor with the arms nearly straight), how far the hips sink, where
// the shoulders end up and how long a step is.
struct KnuckleGeom { float lean, drop, reachZ, sweep, lift; };
KnuckleGeom knuckleGeom(const Build& b) {
    const float arm = (b.upperArm + b.forearm) * 0.95f + b.hand * 0.5f, legs = b.thigh + b.shin;
    const float drop = legs * 0.12f, hip = detail::hipHeight(b) - drop;
    const float up = b.hips / 2 + b.torso.y - b.armRadius * 1.2f;
    float lean = 20;
    while (lean < 80 && hip + up * std::cos(lean * DEG2RAD) > arm) lean += 1;
    return { lean, drop, up * std::sin(lean * DEG2RAD) * 0.9f, std::min(legs, arm) * 0.55f, legs * 0.14f };
}
} // namespace

namespace detail {
float knuckleCadence(const Build& b) { return 0.35f * std::sqrt(9.81f / std::max(0.1f, hipHeight(b))); }
float knuckleSpeed(const Build& b, float cadence) {
    return knuckleGeom(b).sweep * (cadence > 0 ? cadence : knuckleCadence(b)) / kKnuckleDuty;
}
} // namespace detail

namespace clips {
Clip knuckleWalk(const Build& b, float cadence) {
    const KnuckleGeom g = knuckleGeom(b);
    const float c = cadence > 0 ? cadence : detail::knuckleCadence(b);
    return [b, g, c](double s) {
        const float u = frac(s * c);
        Pose p;
        p.offset.y = -g.drop + 0.012f * std::cos(2 * TAU * u);
        p[Joint::Pelvis] = { g.lean, 3 * std::sin(TAU * u), 2 * std::sin(TAU * u) };
        p[Joint::Chest].x = 4;
        p[Joint::Neck].x = -g.lean * 0.45f;
        p[Joint::Head].x = -g.lean * 0.45f;
        // A lateral-sequence walk: left foot, left hand, right foot, right hand.
        for (Side sd : { Side::Left, Side::Right }) {
            const float sx = sgnOf(sd), ph = sd == Side::Left ? 0.f : 0.5f;
            const Vector3 foot = stepAt({ sx * b.hipWidth / 2, b.ankle, 0 }, frac(u - ph), kKnuckleDuty, g.sweep, g.lift);
            p = plant(b, p, sd, foot, { 0, 0, 1 });
            p = orient(b, p, ankleOf(sd), { 0, 1, 0 }, { 0, 0, 1 });
        }
        for (Side sd : { Side::Left, Side::Right }) {
            const float sx = sgnOf(sd), ph = sd == Side::Left ? 0.25f : 0.75f;
            const Vector3 knuckle = stepAt({ sx * (b.torso.x / 2 + b.armRadius), b.hand * 0.5f, g.reachZ }, frac(u - ph),
                                           kKnuckleDuty, g.sweep, g.lift);
            // Knuckles down: the fingers to the floor, the palm facing back.
            p = hand(b, p, sd, knuckle, { 0, -1, 0.15f }, { 0, 0, -1 }, { 0.6f, 0, -1 });
        }
        return p;
    };
}

Clip hang(const Build& b, float bar, float swing) {
    return [b, bar, swing](double s) {
        const float w = std::sin(TAU * 0.45f * (float)s);
        Pose p;
        p[Joint::Root].x = swing * w;
        for (Side sd : { Side::Left, Side::Right }) {
            p[shoulderOf(sd)] = { -172, 0, 9 };
            p[elbowOf(sd)] = { -6, 0, 0 };
            p[hipOf(sd)] = { -18 - 10 * w, 0, 6 };
            p[kneeOf(sd)] = { 28 + 8 * w, 0, 0 };
        }
        p[Joint::Head].x = -10;
        // Hands on the bar: shift the whole body so the palms' midpoint is on it.
        const Vector3 mid = Vector3Scale(Vector3Add(palmAt(b, p, Side::Left), palmAt(b, p, Side::Right)), 0.5f);
        p.offset = Vector3Subtract({ 0, bar, 0 }, mid);
        return p;
    };
}

Clip brachiate(const Build& b, float bar, float spacing, float rate) {
    // In place, like a gait: the bars slide back under the hands. The shoulders swing on a
    // circle round the holding hand while the free hand reaches over for the next bar; at the
    // end of each swing the hands trade (consecutive swings meet in the same pose).
    const Frames rest = solve(b, Pose{});
    const float arm = (b.upperArm + b.forearm + b.hand * 0.5f) * 0.97f;
    const float x = position(rest, Joint::ShoulderL).x;
    const float r = rate > 0 ? rate : brachiateRate(b);
    return [b, bar, spacing, r, arm, x](double s) {
        const double halves = s * r * 2;
        const int n = (int)std::floor(halves);
        const float u = frac(halves);
        const Side held = n % 2 == 0 ? Side::Left : Side::Right, free = held == Side::Left ? Side::Right : Side::Left;
        const float gz = spacing / 2 - spacing * u;               // the held bar, sliding back
        const float drop = std::sqrt(std::max(0.f, arm * arm - gz * gz));
        // The legs lead through the bottom of the swing; at either end, both hands up on
        // bars, the body tips towards them (the same at the end of one swing and the start
        // of the next).
        const float kick = std::sin(PI * u);
        Pose p;
        p[Joint::Root].x = 12 * std::cos(TAU * u);
        p[Joint::Head].x = -15;
        for (Side sd : { Side::Left, Side::Right }) {
            p[hipOf(sd)] = { -20 - 35 * kick, 0, 5 };
            p[kneeOf(sd)] = { 35 + 25 * kick, 0, 0 };
        }
        // The shoulders ride the circle round the held bar: one arm's length from it, under
        // the middle of the path.
        const Frames f0 = solve(b, p);
        const Vector3 sh = position(f0, Joint::ShoulderL);
        p.offset = { 0, bar - drop - sh.y, -sh.z };
        const Vector3 grip{ sgnOf(held) * x, bar, gz };
        const Vector3 next{ sgnOf(free) * x, bar + 0.06f * kick, -spacing / 2 + spacing * smooth(u) };
        const Frames f = solve(b, p);
        for (Side sd : { held, free }) {
            const Vector3 at = sd == held ? grip : next;
            const Vector3 fingers = Vector3Normalize(Vector3Subtract(at, position(f, shoulderOf(sd))));
            p = hand(b, p, sd, at, fingers, { 0, 0, 1 }, { 1, 0, -0.4f });
        }
        return p;
    };
}
} // namespace clips

// A pendulum's pace for the arm the body swings from.
float brachiateRate(const Build& b) { return 0.35f * std::sqrt(9.81f / std::max(0.2f, b.upperArm + b.forearm + b.hand)); }

} // namespace rig
} // namespace rmtn
