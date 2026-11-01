// rig:: legless animals -- the chain body plan (a head and a run of body joints to a tail
// tip), its species (snake, fish), their parts, and the clips that make them slither along
// a path or swim with a wave down the body. Every pose is computed from the time alone.
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
float sgn(Side s) { return s == Side::Left ? 1.f : -1.f; }

const SkeletonPtr& planOf(const Build& b, const char* what) {
    if (!b.plan || b.plan->kind != "chain")
        throw std::runtime_error(std::string("rig: ") + what + " needs a chain build (rig::chain)");
    return b.plan;
}
int segmentsOf(const Skeleton& k) { return (int)k.trait("segments"); }
Joint bodyJoint(const Skeleton& k, int i) { return k["body-" + std::to_string(i)]; }
// The joints down the body, head end first, ending at the tail tip.
std::vector<Joint> spineOf(const Skeleton& k) {
    std::vector<Joint> v;
    for (int i = 0; i < segmentsOf(k); i++) v.push_back(bodyJoint(k, i));
    v.push_back(k["tip"]);
    return v;
}

// ---- shapes
Prim super(const char* name, Joint j, Vector3 at, Vector3 size, float sqV, float sqH, Paint paint) {
    Prim p;
    p.name = name; p.joint = j; p.at = at; p.size = size; p.squareV = sqV; p.squareH = sqH; p.paint = paint;
    return p;
}
Prim ball(const char* name, Joint j, Vector3 at, float d, Paint paint) { return super(name, j, at, { d, d, d }, 1, 1, paint); }
Prim tube(const char* name, Joint a, Joint b, Joint c, float r0, float r1, Paint paint) {
    Prim t;
    t.kind = Prim::Kind::Tube; t.name = name; t.a = a; t.b = b; t.c = c; t.joint = a;
    t.r0 = r0; t.r1 = r1; t.softness = 1; t.paint = paint;
    return t;
}
Prim fixed(Prim p, Color c) { p.paint = Paint::Fixed; p.color = c; return p; }
Prim turned(Prim p, Vector3 r) { p.rot = r; return p; }
const Color WHITE_EYE{ 250, 248, 240, 255 };

// The body's half-width at u (0 at the head, 1 at the tail tip), times the radius.
float profile(const std::string& species, float u) {
    if (species == "fish") {
        if (u < 0.35f) return 0.75f + 0.25f * u / 0.35f;
        return 1 - 0.8f * std::pow((u - 0.35f) / 0.65f, 1.2f);
    }
    if (u < 0.15f) return 0.75f + 0.25f * u / 0.15f;             // snake: a neck, a long body,
    if (u < 0.6f) return 1;                                      //   a tapering tail
    return 1 - 0.88f * (u - 0.6f) / 0.4f;
}

std::vector<Prim> bodyPart(const Build& b) {
    const Skeleton& k = *planOf(b, "body.chain");
    const std::string& sp = k.variant;
    const auto spine = spineOf(k);
    const int n = (int)spine.size();
    const float R = k.dim("radius"), seg = k.dim("segment"), depth = k.trait("depth", 1);
    auto u = [&](int i) { return (float)i / (n - 1); };
    std::vector<Prim> v;
    if (sp == "fish") {
        // A fish's front is stiff and its rear bends: one long egg for the front, a smaller
        // one behind it, and the tail stalk a tube that tapers to the fin.
        const float L = k.dim("length");
        const int m = n - 1, third = m / 3, half = m / 2;   // segments; spine[m] is the tip
        v.push_back(super("body", spine[1], { 0, 0, -seg * 0.3f }, { R * 1.95f, R * 2 * depth, L * 0.56f }, 1, 1, Paint::Skin));
        v.push_back(super("rear", spine[third], { 0, 0, -seg * 1.9f }, { R * 1.5f, R * 1.35f * depth, L * 0.5f }, 1, 1, Paint::Skin));
        v.push_back(tube("stalk", spine[half], spine[(half + m) / 2], spine[m], R * 1.05f, R * 0.4f, Paint::Skin));
        return v;
    }
    // A snake: smooth tubes through the joints in pairs, a pattern of diamonds along the back.
    for (int i = 0; i + 2 < n; i += 2)
        v.push_back(tube("body", spine[i], spine[i + 1], spine[i + 2], R * profile(sp, u(i)), R * profile(sp, u(i + 2)), Paint::Skin));
    for (int i = 0; i < n - 1; i++) {
        const float r = R * profile(sp, u(i) + 0.5f / (n - 1));
        v.push_back(turned(super("diamond", spine[i], { 0, r * 0.82f, -seg * 0.5f }, { r * 1.0f, r * 0.3f, r * 1.0f }, 1, 0.3f, Paint::Dark), { 0, 45, 0 }));
        v.push_back(super("belly", spine[i], { 0, -r * 0.45f, -seg * 0.5f }, { r * 1.6f, r * 0.9f, seg * 1.05f }, 1, 1, Paint::Accent));
    }
    return v;
}

std::vector<Prim> headPart(const Build& b) {
    const Skeleton& k = *planOf(b, "head.chain");
    const Joint h = k["head"], jaw = k["jaw"];
    const float L = k.dim("headLength"), r = k.dim("headRadius"), depth = k.trait("depth", 1);
    std::vector<Prim> v;
    if (k.variant == "fish") {
        v.push_back(super("head", h, { 0, 0, L * 0.15f }, { r * 2, r * 2 * depth, L * 1.6f }, 1, 1, Paint::Skin));
        v.push_back(super("lip", jaw, { 0, r * 0.05f, L * 0.72f }, { r * 0.7f, r * 0.3f, r * 0.4f }, 1, 1, Paint::Accent));
        for (Side s : { Side::Left, Side::Right }) {
            const Vector3 at{ sgn(s) * r * 0.72f, r * 0.25f * depth, L * 0.45f };
            v.push_back(fixed(ball("eye", h, at, r * 0.55f, Paint::Fixed), WHITE_EYE));
            v.push_back(ball("pupil", h, Vector3Add(at, { sgn(s) * r * 0.16f, 0, r * 0.05f }), r * 0.34f, Paint::Dark));
            v.push_back(turned(super("gill", h, { sgn(s) * r * 0.85f, 0, -L * 0.15f }, { r * 0.12f, r * 1.4f * depth, r * 0.25f }, 1, 1, Paint::Accent), { 0, 0, 0 }));
        }
        return v;
    }
    v.push_back(super("head", h, { 0, 0, L * 0.5f }, { r * 2, r * 1.25f, L * 1.15f }, 0.8f, 1, Paint::Skin));
    v.push_back(super("jaw", jaw, { 0, 0, L * 0.45f }, { r * 1.7f, r * 0.45f, L * 0.95f }, 0.8f, 1, Paint::Accent));
    for (Side s : { Side::Left, Side::Right }) {
        const Vector3 at{ sgn(s) * r * 0.72f, r * 0.32f, L * 0.62f };
        v.push_back(fixed(ball("eye", h, at, r * 0.5f, Paint::Fixed), { 232, 196, 70, 255 }));
        v.push_back(super("pupil", h, Vector3Add(at, { sgn(s) * r * 0.16f, 0, 0 }), { r * 0.14f, r * 0.42f, r * 0.14f }, 1, 1, Paint::Dark));
        v.push_back(ball("nostril", h, { sgn(s) * r * 0.3f, r * 0.3f, L * 1.02f }, r * 0.12f, Paint::Dark));
    }
    // The tongue, from the lip: the "tongue" joint turned 180 degrees in y stows it inside
    // the head; at 0 it flicks out.
    const Color red{ 196, 40, 60, 255 };
    v.push_back(fixed(super("tongue", k["tongue"], { 0, 0, L * 0.3f }, { r * 0.22f, r * 0.08f, L * 0.6f }, 1, 1, Paint::Fixed), red));
    for (float x : { -1.f, 1.f })
        v.push_back(fixed(turned(super("fork", k["tongue"], { x * r * 0.08f, 0, L * 0.64f }, { r * 0.1f, r * 0.06f, L * 0.2f }, 1, 1, Paint::Fixed), { 0, x * 25, 0 }), red));
    return v;
}

// A fish's fins: the tail on the tip, a dorsal and an anal fin, and pectorals that flutter
// (on their own joints).
std::vector<Prim> finsPart(const Build& b) {
    const Skeleton& k = *planOf(b, "fins.fish");
    const float R = k.dim("radius"), depth = k.trait("depth", 1), seg = k.dim("segment"), fin = k.dim("fin");
    const int n = segmentsOf(k);
    std::vector<Prim> v;
    for (float sy : { -1.f, 1.f })
        v.push_back(turned(super("tail-fin", k["tip"], { 0, sy * fin * 0.3f, -fin * 0.38f }, { R * 0.12f, fin * 0.45f, fin * 0.85f }, 1, 0.5f, Paint::Accent), { sy * 38, 0, 0 }));
    Prim dorsal = turned(super("dorsal", bodyJoint(k, n / 3), { 0, R * depth * 0.85f, -seg * 0.6f }, { R * 0.1f, R * depth * 0.75f, seg * 3.4f }, 1, 0.5f, Paint::Accent), { -12, 0, 0 });
    dorsal.taper = 0.35f;
    v.push_back(dorsal);
    v.push_back(turned(super("anal", bodyJoint(k, n * 2 / 3), { 0, -R * depth * 0.6f, -seg * 0.3f }, { R * 0.1f, R * depth * 0.5f, seg * 1.3f }, 1, 0.5f, Paint::Accent), { 20, 0, 0 }));
    for (Side s : { Side::Left, Side::Right })
        v.push_back(super("pectoral", k[std::string("fin") + (s == Side::Left ? "-L" : "-R")], { sgn(s) * fin * 0.22f, 0, -fin * 0.12f }, { fin * 0.42f, R * 0.08f, fin * 0.3f }, 1, 0.6f, Paint::Accent));
    return v;
}
} // namespace

namespace detail {
void chainParts(std::map<std::string, PartDef>& reg) {
    auto def = [&](const std::string& name, Slot slot, std::function<std::vector<Prim>(const Build&)> make) {
        reg[name] = PartDef{ name, slot, [make](const Build& b, Side, float) {
            return b.plan && b.plan->kind == "chain" ? make(b) : std::vector<Prim>{};
        }, {} };
    };
    def("body.chain", Slot::Torso, bodyPart);
    def("head.chain", Slot::Head, headPart);
    def("fins.fish", Slot::Extra, [](const Build& b) { return b.plan->variant == "fish" ? finsPart(b) : std::vector<Prim>{}; });
}
} // namespace detail

// ------------------------------------------------------------------ species
ChainSpec ChainSpec::snake() { return ChainSpec{}; }
ChainSpec ChainSpec::fish() {
    ChainSpec s;
    s.species = "fish";
    s.length = 0.36f; s.radius = 0.035f; s.depth = 2.2f; s.segments = 8;
    s.headLength = 0.08f; s.headRadius = 0.03f; s.fin = 0.1f;
    return s;
}

Build chain(const ChainSpec& q) {
    if (q.segments < 3 || q.segments > kMaxJoints - 10) throw std::runtime_error("rig::chain: 3 to 38 segments");
    auto k = std::make_shared<Skeleton>();
    k->kind = "chain";
    k->variant = q.species;
    const int n = q.segments;
    const float seg = (q.length - q.headLength) / n, lift = q.radius * q.depth;
    auto add = [&](const std::string& name, const std::string& parent, Vector3 rest, bool right = false) {
        JointDef d;
        d.name = name; d.rest = rest; d.right = right;
        d.parent = parent.empty() ? 0 : k->find(parent);
        k->joints.push_back(d);
    };
    k->joints.push_back(JointDef{ "root", 0, { 0, 0, 0 } });
    // Head end first: body-0 behind the head, each joint one segment further back.
    add("body-0", "root", { 0, lift, q.length / 2 - q.headLength });
    for (int i = 1; i < n; i++) add("body-" + std::to_string(i), "body-" + std::to_string(i - 1), { 0, 0, -seg });
    add("tip", "body-" + std::to_string(n - 1), { 0, 0, -seg });
    add("head", "body-0", { 0, 0, 0 });
    add("jaw", "head", { 0, -q.headRadius * 0.3f, q.headRadius * 0.1f });
    add("tongue", "head", { 0, -q.headRadius * 0.15f, q.headLength * 0.98f });
    add("fin-L", "body-0", { q.radius * 0.85f, -lift * 0.3f, -seg * 0.3f });
    add("fin-R", "body-0", { -q.radius * 0.85f, -lift * 0.3f, -seg * 0.3f }, true);
    add("floor", "root", { 0, 0, 0 });
    k->joints.back().grounded = true;
    k->height = 2 * lift;
    k->dims = { { "length", q.length }, { "radius", q.radius }, { "segment", seg }, { "headLength", q.headLength },
                { "headRadius", q.headRadius }, { "fin", q.fin } };
    k->traits = { { "depth", q.depth }, { "segments", (float)n } };
    Build b;
    b.plan = k;
    return b;
}

namespace detail {
bool chainLook(const std::string& species, Look& l) {
    ChainSpec q;
    if (species == "snake") q = ChainSpec::snake();
    else if (species == "fish") q = ChainSpec::fish();
    else return false;
    l = Look{};
    l.build = chain(q);
    l.head = "head.chain";
    l.hair = ""; l.hat = ""; l.hips = "";
    l.torso = "body.chain";
    l.extra = species == "fish" ? "fins.fish" : "";
    l.arms("").hands("").legs("").feet("");
    l.face = nullptr;
    l.outfit = Outfit{};
    if (species == "snake") {
        l.skin = { 104, 128, 60, 255 }; l.accent = { 214, 196, 120, 255 }; l.dark = { 44, 58, 30, 255 };
    } else {
        l.skin = { 240, 130, 40, 255 }; l.accent = { 250, 190, 110, 255 }; l.dark = { 18, 18, 22, 255 };
        l.finish = Finish::Plastic;
    }
    l.outfit.fallback = l.skin;
    return true;
}

// ------------------------------------------------------------------ clips
namespace {
// Lay the body along a sideways wave h(s) (s: distance back from body-0): each joint turned
// so the next one lands on the wave. The root shifts so body-0 is on it too.
template <class H> Pose alongWave(const Skeleton& k, Pose p, H h) {
    const auto spine = spineOf(k);
    const float seg = k.dim("segment");
    float heading = 0;
    p.offset.x = h(0.f);
    for (size_t i = 0; i + 1 < spine.size(); i++) {
        const float s = seg * i;
        const float a = std::asin(std::clamp((h(s) - h(s + seg)) / seg, -0.95f, 0.95f)) * RAD2DEG;
        p[spine[i]].y = a - heading;
        heading = a;
    }
    return p;
}
// The tongue stowed, but flicked out for a moment every `every` seconds.
Pose tongueFlick(const Skeleton& k, Pose p, double s, double every) {
    const float w = frac(s / every) * (float)every;   // seconds into this period
    p[k["tongue"]] = w < 0.3f ? Vector3{ 10 * std::sin(TAU * w / 0.1f), 0, 0 } : Vector3{ 0, 180, 0 };
    return p;
}
struct ChainGait { float speed, wave, amp; };   // body lengths per second, wavelength and amplitude (times the length)
ChainGait chainGait(const Skeleton& k, Gait g) {
    const bool fast = g != Gait::Walk;
    if (k.variant == "fish") return { fast ? 1.6f : 0.7f, 0.9f, fast ? 0.1f : 0.07f };
    return { fast ? 0.5f : 0.25f, 0.45f, 0.07f };
}
} // namespace

bool isChain(const Build& b) { return b.plan && b.plan->kind == "chain"; }
float chainSpeed(const Build& b, Gait g, float cadence) {
    const Skeleton& k = *planOf(b, "gaitSpeed");
    const ChainGait d = chainGait(k, g);
    const float L = k.dim("length");
    return cadence > 0 ? cadence * d.wave * L * (k.variant == "fish" ? 0.6f : 1.f) : d.speed * L;
}
float chainCadence(const Build& b, Gait g) {
    const Skeleton& k = *planOf(b, "gaitCadence");
    const ChainGait d = chainGait(k, g);
    // A snake's wave stands still on the ground (one wavelength per cycle); a fish's runs
    // back faster than it swims.
    return d.speed / (d.wave * (k.variant == "fish" ? 0.6f : 1.f));
}

Clip chainGaitClip(const Build& b, Gait g, float cadence) {
    const SkeletonPtr k = planOf(b, "clips::gait");
    const ChainGait d = chainGait(*k, g);
    const float c = cadence > 0 ? cadence : chainCadence(b, g);
    const float L = k->dim("length"), lambda = d.wave * L, A = d.amp * L;
    const bool fish = k->variant == "fish";
    return [k, c, L, lambda, A, fish](double s) {
        const float phase = TAU * frac(s * c);
        Pose p;
        if (fish) {
            // The wave grows towards the tail; the head barely sways.
            p = alongWave(*k, p, [&](float d) {
                const float u = std::clamp(d / L, 0.f, 1.f);
                return A * (0.15f + 0.85f * u * u) * std::sin(phase - TAU * d / lambda);
            });
            const float pect = 25 * std::sin(phase * 2);
            p[(*k)["fin-L"]] = { 0, 15, -pect };
            p[(*k)["fin-R"]] = { 0, 15, -pect };
            p.offset.y = 0.01f * L * std::sin(phase);
            p[(*k)["jaw"]].x = 6 + 6 * std::sin(phase * 0.5f);
        } else {
            // The path stands still on the ground while the snake slides along it (in place:
            // the path moves back under it).
            p = alongWave(*k, p, [&](float d) { return A * std::sin(TAU * d / lambda - phase); });
            p[(*k)["head"]] = { -10, -0.8f * p[(*k)["body-0"]].y, 0 };
            p = tongueFlick(*k, p, s, 1.7);
        }
        return p;
    };
}

Clip chainStand(const Build& b) {
    const SkeletonPtr k = planOf(b, "clips::stand");
    const float L = k->dim("length");
    const bool fish = k->variant == "fish";
    return [k, L, fish](double s) {
        const float t = (float)s;
        Pose p;
        if (fish) {
            // Hovering: a slow sway, the pectorals sculling.
            p = alongWave(*k, p, [&](float d) { return 0.025f * L * (d / L) * (d / L) * std::sin(TAU * t * 0.8f - 4 * d / L); });
            const float pect = 30 * std::sin(TAU * t * 2.2f);
            p[(*k)["fin-L"]] = { 0, 20, -pect };
            p[(*k)["fin-R"]] = { 0, 20, pect };
            p.offset.y = 0.015f * L * std::sin(TAU * t * 0.4f);
            p[(*k)["jaw"]].x = 8 + 8 * std::sin(TAU * t * 1.1f);
        } else {
            // Resting in a loose S, head up and tasting the air.
            p = alongWave(*k, p, [&](float d) { return 0.08f * L * std::sin(TAU * d / (0.5f * L) + 0.3f * std::sin(TAU * t * 0.1f)); });
            p[(*k)["head"]] = { -18 + 3 * std::sin(TAU * t * 0.3f), -0.8f * p[(*k)["body-0"]].y + 15 * std::sin(TAU * t * 0.12f), 0 };
        }
        if (!fish) p = tongueFlick(*k, p, s, 2.3);
        return p;
    };
}
} // namespace detail

} // namespace rig
} // namespace rmtn
