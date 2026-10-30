// rig:: animals -- the four-legged body plan, its species (dog, cat, horse, cow, giraffe),
// their parts, and the clips that make them stand and walk, trot, canter or gallop with
// their paws planted. Every pose is computed from the time alone.
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

// ---- the skeleton
// Joint order: the body first (root, pelvis, spine, chest), then neck, head and tail, then
// the legs, left before right, then the floor.
struct LegNames { const char *top, *mid, *low, *paw; };
const LegNames FRONT{ "shoulder", "elbow", "wrist", "paw-front" };
const LegNames HIND{ "hip", "knee", "hock", "paw-hind" };

const SkeletonPtr& planOf(const Build& b, const char* what) {
    if (!b.plan || b.plan->kind != "quadruped")
        throw std::runtime_error(std::string("rig: ") + what + " needs a quadruped build (rig::quadruped)");
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
Prim cone(const char* name, Joint j, Vector3 at, Vector3 size, float rz, Paint paint) {
    Prim p = super(name, j, at, size, 1, 1, paint);
    p.kind = Prim::Kind::Cone; p.rot = { 0, 0, rz };
    return p;
}
Prim fixed(Prim p, Color c) { p.paint = Paint::Fixed; p.color = c; return p; }
Prim turned(Prim p, Vector3 r) { p.rot = r; return p; }
float sgn(Side s) { return s == Side::Left ? 1.f : -1.f; }
const char* LR(Side s) { return s == Side::Left ? "-L" : "-R"; }
Joint J(const Skeleton& k, const char* base, Side s) { return k[std::string(base) + LR(s)]; }

const Color WHITE_EYE{ 250, 248, 240, 255 };
const Color IVORY{ 236, 226, 200, 255 };

// An eye: a white ball with a dark pupil pushed out along `look` (unit, in the head frame).
void eye(std::vector<Prim>& v, Joint head, Vector3 at, float d, Vector3 look) {
    v.push_back(fixed(ball("eye", head, at, d, Paint::Fixed), WHITE_EYE));
    v.push_back(ball("pupil", head, Vector3Add(at, Vector3Scale(look, d * 0.3f)), d * 0.55f, Paint::Dark));
}

// A neck segment's direction and its top side, in the segment's own joint frame at rest.
Vector3 neckDir(float pitch) { return { 0, sinDeg(pitch), cosDeg(pitch) }; }
Vector3 neckTop(float pitch) { return { 0, cosDeg(pitch), -sinDeg(pitch) }; }

// ---- parts
std::vector<Prim> bodyPart(const Build& b) {
    const Skeleton& k = *planOf(b, "body.quad");
    const float r = k.dim("bodyRadius"), rump = r * k.trait("rump", 1), chest = r * k.trait("chest", 1);
    const float nr = k.dim("neckRadius");
    if (k.variant == "turtle") {
        // A domed shell over a flat plastron; the neck comes out from under its front rim.
        const float L = k.dim("bodyLength");
        Prim dome = super("shell", k["spine"], { 0, -r * 0.2f, 0 }, { r * 2.5f, r * 2.1f, L * 1.75f }, 0.85f, 1, Paint::Skin);
        dome.from = 0;
        Prim rim = super("rim", k["spine"], { 0, -r * 0.22f, 0 }, { r * 2.6f, r * 0.22f, L * 1.82f }, 1, 1, Paint::Skin);
        return { dome, rim, super("plastron", k["spine"], { 0, -r * 0.4f, 0 }, { r * 2.2f, r * 0.3f, L * 1.55f }, 1, 1, Paint::Accent),
                 tube("neck", k["neck-1"], k["neck-2"], k["head"], nr * 1.2f, nr, 1, Paint::Hair),
                 ball("neck-base", k["neck-1"], { 0, 0, 0 }, nr * 2.4f, Paint::Hair) };
    }
    std::vector<Prim> v{
        tube("barrel", k["pelvis"], k["spine"], k["chest"], rump, chest, 1, Paint::Skin),
        super("rump", k["pelvis"], { 0, r * 0.06f, -r * 0.22f }, { rump * 1.96f, rump * 1.9f, rump * 1.8f }, 1, 1, Paint::Skin),
        super("breast", k["chest"], { 0, -r * 0.05f, r * 0.2f }, { chest * 1.9f, chest * 1.95f, chest * 1.8f }, 1, 1, Paint::Skin),
        tube("neck", k["neck-1"], k["neck-2"], k["head"], nr * 1.25f, nr * 0.85f, 1, Paint::Skin),
        ball("neck-base", k["neck-1"], { 0, 0, 0 }, nr * 2.6f, Paint::Skin) };
    return v;
}

std::vector<Prim> legPart(const Build& b, Side s) {
    const Skeleton& k = *planOf(b, "leg.quad");
    const float lr = k.dim("legRadius"), paw = k.dim("paw");
    const bool hooves = k.trait("hooves") > 0.5f, turtle = k.variant == "turtle";
    const Paint skin = turtle ? Paint::Hair : Paint::Skin;   // a turtle's shell is its Skin colour
    std::vector<Prim> v;
    for (const LegNames* L : { &FRONT, &HIND }) {
        const bool front = L == &FRONT;
        const Joint top = J(k, L->top, s), mid = J(k, L->mid, s), low = J(k, L->low, s), pw = J(k, L->paw, s);
        const float upper = Vector3Length(restOffset(b, mid)), cannon = Vector3Length(restOffset(b, pw));
        // The haunch or shoulder blade blends the leg into the body.
        const size_t first = v.size();
        if (!turtle) {
            if (front) v.push_back(super("shoulder-blade", top, { 0, -upper * 0.1f, 0 }, { lr * 3.0f, upper * 1.0f, lr * 3.4f }, 1, 1, Paint::Skin));
            else v.push_back(super("haunch", top, { 0, -upper * 0.22f, -lr * 0.3f }, { lr * 3.5f, upper * 1.2f, lr * 3.9f }, 1, 1, Paint::Skin));
        }
        v.push_back(tube(front ? "foreleg" : "hindleg", top, mid, low, lr * (front ? 1.5f : 1.7f), lr * 0.72f, 0.5f, skin));
        v.push_back(super("cannon", low, { 0, -cannon * 0.5f, 0 }, { lr * 1.3f, cannon * 1.1f, lr * 1.3f }, 0.4f, 1, skin));
        // In 2D the upper leg sorts behind the barrel, so legs come out from under the body.
        for (size_t i = first; i < v.size(); i++) v[i].depth2d = -k.dim("bodyRadius") * 1.2f;
        if (hooves) {
            Prim h = super("hoof", pw, { 0, -paw * 0.45f, lr * 0.12f }, { lr * 1.65f, paw * 1.1f, lr * 1.9f }, 0.25f, 1, Paint::Shoes);
            h.taper = 0.8f;
            v.push_back(h);
        } else {
            v.push_back(super("paw", pw, { 0, -paw * 0.35f, lr * 0.45f }, { lr * 1.75f, paw * 1.5f, lr * 2.4f }, 0.7f, 1, skin));
        }
    }
    return v;
}

// Heads: the skull at the head joint (the top of the neck), the muzzle along +z.
std::vector<Prim> headPart(const Build& b, const std::string& species) {
    const Skeleton& k = *planOf(b, "head");
    const Joint h = k["head"];
    const float L = k.dim("headLength"), r = k.dim("headRadius");
    std::vector<Prim> v;
    auto skull = [&](float w, float ht, float len, float z) {
        v.push_back(super("skull", h, { 0, 0, z }, { r * 2 * w, r * 2 * ht, len }, 1, 1, Paint::Skin));
    };
    auto muzzle = [&](float w, float len, float y, float z, float taper) {
        Prim m = super("muzzle", h, { 0, y, z }, { r * 2 * w, r * 2 * w * 0.9f, len }, 1, 1, Paint::Accent);
        m.taper = taper;
        v.push_back(m);
    };
    auto nose = [&](Vector3 at, float d) { v.push_back(super("nose", h, at, { d * 1.3f, d * 0.8f, d * 0.7f }, 1, 1, Paint::Dark)); };
    auto sideEyes = [&](float y, float z, float d) {
        for (Side s : { Side::Left, Side::Right })
            eye(v, h, { sgn(s) * r * 0.86f, y, z }, d, Vector3Normalize({ sgn(s) * 1.f, 0, 0.35f }));
    };
    auto frontEyes = [&](float x, float y, float z, float d) {
        for (Side s : { Side::Left, Side::Right })
            eye(v, h, { sgn(s) * x, y, z }, d, Vector3Normalize({ sgn(s) * 0.25f, 0, 1 }));
    };
    if (species == "horse") {
        skull(0.9f, 1.0f, L * 0.55f, L * 0.1f);
        muzzle(0.72f, L * 0.62f, -r * 0.18f, L * 0.55f, 0.9f);
        for (Side s : { Side::Left, Side::Right }) {
            nose({ sgn(s) * r * 0.32f, -r * 0.05f, L * 0.84f }, r * 0.18f);
            v.push_back(cone("ear", h, { sgn(s) * r * 0.45f, r * 0.85f, -L * 0.06f }, { r * 0.42f, r * 0.85f, r * 0.3f }, -sgn(s) * 10, Paint::Skin));
        }
        sideEyes(r * 0.38f, L * 0.1f, r * 0.42f);
    } else if (species == "cow") {
        skull(1.0f, 1.0f, L * 0.55f, L * 0.08f);
        muzzle(0.9f, L * 0.5f, -r * 0.2f, L * 0.55f, 1.0f);
        for (Side s : { Side::Left, Side::Right }) {
            nose({ sgn(s) * r * 0.38f, -r * 0.08f, L * 0.8f }, r * 0.2f);
            v.push_back(turned(super("ear", h, { sgn(s) * r * 1.15f, r * 0.42f, -L * 0.04f }, { r * 0.85f, r * 0.24f, r * 0.48f }, 1, 1, Paint::Skin), { 0, 0, sgn(s) * -12 }));
            v.push_back(fixed(cone("horn", h, { sgn(s) * r * 0.62f, r * 0.92f, -L * 0.06f }, { r * 0.26f, r * 0.7f, r * 0.26f }, -sgn(s) * 55, Paint::Fixed), IVORY));
        }
        sideEyes(r * 0.4f, L * 0.12f, r * 0.42f);
    } else if (species == "giraffe") {
        skull(0.88f, 0.95f, L * 0.5f, L * 0.08f);
        muzzle(0.66f, L * 0.62f, -r * 0.2f, L * 0.52f, 0.85f);
        for (Side s : { Side::Left, Side::Right }) {
            nose({ sgn(s) * r * 0.28f, -r * 0.05f, L * 0.82f }, r * 0.16f);
            v.push_back(turned(super("ear", h, { sgn(s) * r * 1.0f, r * 0.5f, -L * 0.08f }, { r * 0.75f, r * 0.22f, r * 0.36f }, 1, 1, Paint::Skin), { 0, 0, sgn(s) * -18 }));
            v.push_back(turned(super("ossicone", h, { sgn(s) * r * 0.36f, r * 1.15f, -L * 0.06f }, { r * 0.2f, r * 0.75f, r * 0.2f }, 0.15f, 1, Paint::Skin), { 0, 0, -sgn(s) * 10 }));
            v.push_back(ball("ossicone-tip", h, { sgn(s) * r * 0.43f, r * 1.52f, -L * 0.06f }, r * 0.26f, Paint::Hair));
        }
        sideEyes(r * 0.42f, L * 0.1f, r * 0.4f);
    } else if (species == "turtle") {
        v.push_back(super("skull", h, { 0, 0, L * 0.3f }, { r * 2, r * 1.8f, L * 1.1f }, 1, 1, Paint::Hair));
        Prim beak = super("beak", h, { 0, -r * 0.2f, L * 0.78f }, { r * 1.2f, r * 0.9f, L * 0.4f }, 1, 1, Paint::Hair);
        beak.taper = 0.6f;
        v.push_back(beak);
        for (Side s : { Side::Left, Side::Right })
            eye(v, h, { sgn(s) * r * 0.78f, r * 0.3f, L * 0.5f }, r * 0.42f, Vector3Normalize({ sgn(s), 0, 0.5f }));
    } else if (species == "cat") {
        skull(1.0f, 0.92f, r * 1.9f, 0);
        muzzle(0.5f, r * 0.7f, -r * 0.35f, r * 0.72f, 1.0f);
        nose({ 0, -r * 0.12f, r * 1.05f }, r * 0.2f);
        for (Side s : { Side::Left, Side::Right })
            v.push_back(cone("ear", h, { sgn(s) * r * 0.55f, r * 0.7f, -r * 0.1f }, { r * 0.7f, r * 0.8f, r * 0.32f }, -sgn(s) * 14, Paint::Skin));
        frontEyes(r * 0.42f, r * 0.15f, r * 0.75f, r * 0.42f);
    } else {   // dog
        skull(0.95f, 0.95f, L * 0.6f, 0);
        muzzle(0.6f, L * 0.62f, -r * 0.28f, L * 0.55f, 0.95f);
        nose({ 0, -r * 0.05f, L * 0.86f }, r * 0.32f);
        for (Side s : { Side::Left, Side::Right })
            v.push_back(turned(super("ear", h, { sgn(s) * r * 0.9f, r * 0.12f, -L * 0.05f }, { r * 0.28f, r * 1.1f, r * 0.62f }, 1, 1, Paint::Hair), { 0, 0, sgn(s) * 14 }));
        frontEyes(r * 0.42f, r * 0.32f, L * 0.22f, r * 0.36f);
    }
    return v;
}

std::vector<Prim> tailPrims(const Build& b, const std::string& species);
std::vector<Prim> tailPart(const Build& b, const std::string& species) {
    std::vector<Prim> v = tailPrims(b, species);
    for (Prim& p : v) p.depth2d = -0.5f * b.plan->dim("bodyRadius");
    return v;
}
std::vector<Prim> tailPrims(const Build& b, const std::string& species) {
    const Skeleton& k = *planOf(b, "tail");
    const float tr = k.dim("tailRadius");
    const Joint t1 = k["tail-1"], t2 = k["tail-2"], t3 = k["tail-3"];
    if (species == "horse")
        return { tube("tail", t1, t2, t3, tr, tr * 2.2f, 1, Paint::Hair), ball("tail-end", t3, { 0, 0, 0 }, tr * 4.2f, Paint::Hair) };
    if (species == "cow" || species == "giraffe")
        return { tube("tail", t1, t2, t3, tr, tr * 0.8f, 1, Paint::Skin),
                 super("tuft", t3, { 0, -tr * 2.5f, 0 }, { tr * 4.5f, tr * 8, tr * 4.5f }, 1, 1, Paint::Hair) };
    if (species == "cat") return { tube("tail", t1, t2, t3, tr, tr * 0.9f, 1, Paint::Skin) };
    if (species == "turtle") return { tube("tail", t1, t2, t3, tr, tr * 0.2f, 1, Paint::Hair) };
    return { tube("tail", t1, t2, t3, tr * 1.4f, tr * 0.55f, 1, Paint::Skin) };   // dog
}

// Hair along the top of the neck, attached to its two segments so it bends with them.
std::vector<Prim> manePart(const Build& b, float height, float thickness) {
    const Skeleton& k = *planOf(b, "mane");
    const float p = k.trait("neckPitch"), nr = k.dim("neckRadius"), seg = k.dim("neckLength") / 2;
    std::vector<Prim> v;
    for (const char* j : { "neck-1", "neck-2" })
        for (float t : { 0.12f, 0.38f, 0.64f, 0.9f }) {
            const Vector3 at = Vector3Add(Vector3Scale(neckDir(p), seg * t), Vector3Scale(neckTop(p), nr * 0.8f));
            v.push_back(turned(super("mane", k[j], at, { nr * thickness, nr * height, seg * 0.34f }, 1, 1, Paint::Hair), { -p, 0, 0 }));
        }
    v.push_back(super("forelock", k["head"], { 0, k.dim("headRadius") * 0.95f, 0 }, { nr * thickness * 1.4f, nr * height * 0.7f, nr * 0.7f }, 1, 1, Paint::Hair));
    return v;
}

// The Euler XYZ angles (degrees) that turn a prim's axes to x, y, z (MatrixRotateXYZ's
// inverse, as in rig_act).
Vector3 eulerOfAxes(Vector3 x, Vector3 y, Vector3 z) {
    const float sy = std::clamp(z.x, -1.f, 1.f);
    Vector3 r{ 0, std::asin(sy), 0 };
    if (std::fabs(sy) < 0.9999f) {
        r.x = -std::atan2(z.y, z.z);
        r.z = -std::atan2(y.x, x.x);
    } else r.x = std::atan2(x.z, y.y);
    return Vector3Scale(r, RAD2DEG);
}
// A flat patch lying on a surface: `normal` out of it, `along` the body's axis, turned
// `spin` degrees about the normal; w across, l along, sunk so only its face shows.
Prim patch(Joint j, Vector3 at, Vector3 normal, Vector3 along, float w, float l, float spin, float squareness) {
    Vector3 y = Vector3Normalize(normal), z = Vector3Normalize(Vector3Subtract(along, Vector3Scale(y, Vector3DotProduct(along, y))));
    Vector3 x = Vector3CrossProduct(y, z);
    const float c = cosDeg(spin), sn = sinDeg(spin);
    const Vector3 x2 = Vector3Add(Vector3Scale(x, c), Vector3Scale(z, sn)), z2 = Vector3Subtract(Vector3Scale(z, c), Vector3Scale(x, sn));
    Prim p = super("patch", j, at, { w, std::min(w, l) * 0.16f, l }, 1, squareness, Paint::Dark);
    p.rot = eulerOfAxes(x2, y, z2);
    return p;
}
// Patches on the barrel (in the spine's frame it runs from the hips, z = -L/2, to the
// withers, z = L/2) and on the neck's two segments.
struct Spot { float z, angle, w, l, spin; };   // z along (-1..1); angle round (0 = top, 90 = left); size times the radius
std::vector<Prim> spots(const Build& b, const std::vector<Spot>& body, const std::vector<Spot>& neck, float squareness) {
    const Skeleton& k = *planOf(b, "coat");
    const float L = k.dim("bodyLength"), r = k.dim("bodyRadius"), rump = k.trait("rump", 1), chest = k.trait("chest", 1);
    const Vector3 rise = restOffset(b, k["spine"]);   // the spine's offset from the hips
    const Vector3 axisDir = Vector3Normalize({ 0, rise.y, rise.z });
    std::vector<Prim> v;
    for (const Spot& s : body) {
        const float rr = r * (rump + (chest - rump) * (s.z + 1) / 2);
        const Vector3 centre{ 0, rise.y * s.z, L / 2 * s.z };
        const Vector3 out{ sinDeg(s.angle), cosDeg(s.angle), 0 };
        v.push_back(patch(k["spine"], Vector3Add(centre, Vector3Scale(out, rr * 0.99f)), out, axisDir, rr * s.w, rr * s.l, s.spin, squareness));
    }
    const float p = k.trait("neckPitch"), nr = k.dim("neckRadius"), seg = k.dim("neckLength") / 2;
    for (const Spot& s : neck) {
        const char* j = s.z < 0 ? "neck-1" : "neck-2";
        const float t = s.z < 0 ? s.z + 1 : s.z;      // along that segment, 0..1
        const float rr = nr * (1.25f - 0.4f * (s.z + 1) / 2);
        const Vector3 out = Vector3Add(Vector3Scale({ 1, 0, 0 }, sinDeg(s.angle)), Vector3Scale(neckTop(p), cosDeg(s.angle)));
        const Vector3 at = Vector3Add(Vector3Scale(neckDir(p), seg * t), Vector3Scale(out, rr * 0.99f));
        v.push_back(patch(k[j], at, out, neckDir(p), rr * s.w, rr * s.l, s.spin, squareness));
    }
    return v;
}
// A deterministic wobble for patch layouts.
float jitter(int i, float amp) { return amp * std::sin(i * 12.9898f + 78.233f) * 0.5f; }

std::vector<Prim> cowCoat(const Build& b) {
    const Skeleton& k = *planOf(b, "coat.cow");
    std::vector<Spot> body;
    // Big irregular patches, each two overlapping ovals.
    const Spot big[] = { { -0.7f, 70, 0.9f, 1.1f, 20 }, { 0.15f, 85, 1.1f, 1.3f, -15 }, { 0.75f, 60, 0.7f, 0.8f, 30 },
                         { -0.35f, -70, 1.0f, 1.2f, -25 }, { 0.45f, -80, 0.9f, 1.0f, 10 }, { -0.05f, 10, 0.6f, 0.8f, 40 } };
    for (const Spot& s : big) {
        body.push_back(s);
        body.push_back({ s.z + 0.12f, s.angle + 18, s.w * 0.7f, s.l * 0.6f, s.spin + 50 });
    }
    std::vector<Prim> v = spots(b, body, {}, 1);
    const float r = k.dim("bodyRadius");
    v.push_back(super("udder", k["pelvis"], { 0, -r * 0.82f, r * 0.45f }, { r * 0.62f, r * 0.42f, r * 0.62f }, 1, 1, Paint::Accent));
    return v;
}
// A turtle's scutes: a row of plates down the top of the shell and two rows either side.
std::vector<Prim> turtleShell(const Build& b) {
    const Skeleton& k = *planOf(b, "shell.turtle");
    const float r = k.dim("bodyRadius"), L = k.dim("bodyLength");
    const float a = r * 1.25f, c = r * 1.05f, d = L * 0.875f;   // the dome's semi-axes
    std::vector<Prim> v;
    auto plate = [&](float u, float ang, float w) {             // u along (-1..1), ang round from the top
        const float z = u * d * 0.8f, ring = std::sqrt(std::max(0.f, 1 - (z / d) * (z / d)));
        const Vector3 at{ a * ring * sinDeg(ang), -r * 0.2f + c * ring * cosDeg(ang), z };
        const Vector3 n = Vector3Normalize({ at.x / (a * a), (at.y + r * 0.2f) / (c * c), z / (d * d) });
        Prim p = patch(k["spine"], Vector3Add(at, Vector3Scale(n, r * 0.01f)), n, { 0, 0, 1 }, r * w, r * w * 1.05f, 0, 0.45f);
        p.size.y = r * 0.12f;
        v.push_back(p);
    };
    for (float u : { -0.62f, -0.2f, 0.2f, 0.62f }) plate(u, 0, 0.62f);
    for (float side : { -1.f, 1.f })
        for (float u : { -0.55f, 0.f, 0.55f }) plate(u, side * 55, 0.6f);
    return v;
}
std::vector<Prim> giraffeCoat(const Build& b) {
    std::vector<Spot> body, neck;
    int i = 0;
    for (int row = 0; row < 7; row++)
        for (int col = -5; col <= 5; col++, i++) {
            const float z = -0.9f + row * 0.3f + (col % 2 ? 0.15f : 0.f) + jitter(i, 0.08f);
            body.push_back({ std::min(z, 0.95f), col * 25.f + jitter(i + 7, 8), 0.36f, 0.4f, jitter(i + 3, 40), });
        }
    for (int row = 0; row < 14; row++)
        for (int col = -3; col <= 3; col++, i++)
            neck.push_back({ -0.95f + row * 0.14f + (col % 2 ? 0.07f : 0.f), col * 42.f + jitter(i, 10), 0.62f, 0.85f, jitter(i + 5, 30) });
    return spots(b, body, neck, 0.55f);
}
} // namespace

namespace detail {
void creatureParts(std::map<std::string, PartDef>& reg) {
    // On a body without the quadruped plan these parts add nothing.
    auto def = [&](const std::string& name, Slot slot, std::function<std::vector<Prim>(const Build&, Side, float)> make) {
        reg[name] = PartDef{ name, slot, [make](const Build& b, Side s, float soft) {
            return b.plan && b.plan->kind == "quadruped" ? make(b, s, soft) : std::vector<Prim>{};
        }, {} };
    };
    def("body.quad", Slot::Torso, [](const Build& b, Side, float) { return bodyPart(b); });
    def("leg.quad", Slot::Leg, [](const Build& b, Side s, float) { return legPart(b, s); });
    for (const char* sp : { "dog", "cat", "horse", "cow", "giraffe", "turtle" }) {
        const std::string species = sp;
        def("head." + species, Slot::Head, [species](const Build& b, Side, float) { return headPart(b, species); });
        def("tail." + species, Slot::Hips, [species](const Build& b, Side, float) { return tailPart(b, species); });
    }
    def("mane.horse", Slot::Hair, [](const Build& b, Side, float) { return manePart(b, 1.3f, 0.45f); });
    def("mane.giraffe", Slot::Hair, [](const Build& b, Side, float) { return manePart(b, 0.75f, 0.3f); });
    def("coat.cow", Slot::Extra, [](const Build& b, Side, float) { return cowCoat(b); });
    def("coat.giraffe", Slot::Extra, [](const Build& b, Side, float) { return giraffeCoat(b); });
    def("shell.turtle", Slot::Extra, [](const Build& b, Side, float) { return turtleShell(b); });
}
} // namespace detail

// ------------------------------------------------------------------ species
QuadSpec QuadSpec::dog() { return QuadSpec{}; }
QuadSpec QuadSpec::cat() {
    QuadSpec s;
    s.species = "cat";
    s.bodyLength = 0.3f; s.bodyRadius = 0.075f;
    s.frontUpper = 0.1f; s.frontLower = 0.12f; s.frontCannon = 0.06f;
    s.hindUpper = 0.12f; s.hindLower = 0.11f; s.hindCannon = 0.05f;
    s.paw = 0.02f; s.legRadius = 0.022f; s.stance = 0.045f;
    s.neckLength = 0.07f; s.neckRadius = 0.045f; s.neckPitch = 45;
    s.headLength = 0.1f; s.headRadius = 0.06f; s.headPitch = 0;
    s.tailLength = 0.3f; s.tailRadius = 0.016f; s.tailPitch = 60;
    return s;
}
QuadSpec QuadSpec::horse() {
    QuadSpec s;
    s.species = "horse";
    s.bodyLength = 0.95f; s.bodyRadius = 0.33f; s.rump = 1.05f; s.chest = 1.05f;
    s.frontUpper = 0.4f; s.frontLower = 0.42f; s.frontCannon = 0.3f;
    s.hindUpper = 0.45f; s.hindLower = 0.42f; s.hindCannon = 0.25f;
    s.paw = 0.08f; s.legRadius = 0.07f; s.stance = 0.17f;
    s.neckLength = 0.8f; s.neckRadius = 0.13f; s.neckPitch = 52;
    s.headLength = 0.58f; s.headRadius = 0.12f; s.headPitch = 55;
    s.tailLength = 0.75f; s.tailRadius = 0.05f; s.tailPitch = -60;
    s.hooves = true;
    return s;
}
QuadSpec QuadSpec::cow() {
    QuadSpec s;
    s.species = "cow";
    s.bodyLength = 1.15f; s.bodyRadius = 0.4f; s.chest = 1.05f;
    s.frontUpper = 0.28f; s.frontLower = 0.33f; s.frontCannon = 0.17f;
    s.hindUpper = 0.3f; s.hindLower = 0.3f; s.hindCannon = 0.18f;
    s.paw = 0.06f; s.legRadius = 0.075f; s.stance = 0.2f;
    s.neckLength = 0.5f; s.neckRadius = 0.18f; s.neckPitch = 20;
    s.headLength = 0.45f; s.headRadius = 0.14f; s.headPitch = 35;
    s.tailLength = 0.8f; s.tailRadius = 0.022f; s.tailPitch = -80;
    s.hooves = true;
    return s;
}
QuadSpec QuadSpec::giraffe() {
    QuadSpec s;
    s.species = "giraffe";
    s.bodyLength = 1.0f; s.bodyRadius = 0.36f; s.rump = 0.9f; s.chest = 1.1f;
    s.frontUpper = 0.62f; s.frontLower = 0.7f; s.frontCannon = 0.55f;
    s.hindUpper = 0.55f; s.hindLower = 0.6f; s.hindCannon = 0.42f;
    s.paw = 0.1f; s.legRadius = 0.065f; s.stance = 0.17f;
    s.neckLength = 1.8f; s.neckRadius = 0.13f; s.neckPitch = 62;
    s.headLength = 0.5f; s.headRadius = 0.11f; s.headPitch = 45;
    s.tailLength = 0.85f; s.tailRadius = 0.025f; s.tailPitch = -75;
    s.hooves = true;
    return s;
}

QuadSpec QuadSpec::turtle() {
    QuadSpec s;
    s.species = "turtle";
    s.bodyLength = 0.2f; s.bodyRadius = 0.11f;
    s.frontUpper = 0.05f; s.frontLower = 0.05f; s.frontCannon = 0.025f;
    s.hindUpper = 0.05f; s.hindLower = 0.05f; s.hindCannon = 0.025f;
    s.paw = 0.02f; s.legRadius = 0.03f; s.stance = 0.1f;
    s.neckLength = 0.1f; s.neckRadius = 0.028f; s.neckPitch = 20;
    s.headLength = 0.065f; s.headRadius = 0.032f; s.headPitch = -10;
    s.tailLength = 0.05f; s.tailRadius = 0.018f; s.tailPitch = -25;
    return s;
}

Build quadruped(const QuadSpec& q) {
    auto k = std::make_shared<Skeleton>();
    k->kind = "quadruped";
    k->variant = q.species;
    const float front = q.frontUpper + q.frontLower + q.frontCannon + q.paw;
    const float hind = q.hindUpper + q.hindLower + q.hindCannon + q.paw;
    auto add = [&](const std::string& name, const std::string& parent, Vector3 rest, bool right = false) {
        JointDef d;
        d.name = name; d.rest = rest; d.right = right;
        d.parent = parent.empty() ? 0 : k->find(parent);
        k->joints.push_back(d);
    };
    k->joints.push_back(JointDef{ "root", 0, { 0, 0, 0 } });
    const Vector3 rise{ 0, (front - hind) / 2, q.bodyLength / 2 };
    add("pelvis", "root", { 0, hind, -q.bodyLength / 2 });
    add("spine", "pelvis", rise);
    add("chest", "spine", rise);
    const float r = q.bodyRadius;
    add("neck-1", "chest", { 0, r * 0.45f, r * q.chest * 0.8f });
    add("neck-2", "neck-1", Vector3Scale(neckDir(q.neckPitch), q.neckLength / 2));
    add("head", "neck-2", Vector3Scale(neckDir(q.neckPitch), q.neckLength / 2));
    const Vector3 tailStep{ 0, sinDeg(q.tailPitch) * q.tailLength / 2, -cosDeg(q.tailPitch) * q.tailLength / 2 };
    add("tail-1", "pelvis", { 0, r * 0.5f, -r * q.rump * 1.1f });
    add("tail-2", "tail-1", tailStep);
    add("tail-3", "tail-2", tailStep);
    for (Side s : { Side::Left, Side::Right }) {
        const bool right = s == Side::Right;
        const std::string e = LR(s);
        const float x = sgn(s) * q.stance;
        add(std::string(FRONT.top) + e, "chest", { x, 0, r * 0.1f }, right);
        add(std::string(FRONT.mid) + e, std::string(FRONT.top) + e, { 0, -q.frontUpper, 0 }, right);
        add(std::string(FRONT.low) + e, std::string(FRONT.mid) + e, { 0, -q.frontLower, 0 }, right);
        add(std::string(FRONT.paw) + e, std::string(FRONT.low) + e, { 0, -q.frontCannon, 0 }, right);
        add(std::string(HIND.top) + e, "pelvis", { x, 0, 0 }, right);
        add(std::string(HIND.mid) + e, std::string(HIND.top) + e, { 0, -q.hindUpper, 0 }, right);
        add(std::string(HIND.low) + e, std::string(HIND.mid) + e, { 0, -q.hindLower, 0 }, right);
        add(std::string(HIND.paw) + e, std::string(HIND.low) + e, { 0, -q.hindCannon, 0 }, right);
    }
    add("floor", "root", { 0, 0, 0 });
    k->joints.back().grounded = true;
    k->height = front + r * 0.35f + sinDeg(q.neckPitch) * q.neckLength + q.headRadius * 1.6f;
    k->dims = { { "bodyLength", q.bodyLength }, { "bodyRadius", q.bodyRadius }, { "legRadius", q.legRadius },
                { "paw", q.paw }, { "stance", q.stance }, { "neckLength", q.neckLength }, { "neckRadius", q.neckRadius },
                { "headLength", q.headLength }, { "headRadius", q.headRadius }, { "tailLength", q.tailLength },
                { "tailRadius", q.tailRadius }, { "frontLength", front }, { "hindLength", hind } };
    k->traits = { { "rump", q.rump }, { "chest", q.chest }, { "neckPitch", q.neckPitch }, { "headPitch", q.headPitch },
                  { "tailPitch", q.tailPitch }, { "hooves", q.hooves ? 1.f : 0.f } };
    Build b;
    b.plan = k;
    return b;
}

std::vector<std::string> animals() { return { "dog", "cat", "horse", "cow", "giraffe", "turtle", "chicken", "duck", "trex", "snake", "fish", "chimp", "gorilla", "orangutan" }; }

Look animal(const std::string& species) {
    Look ape;
    if (detail::apeLook(species, ape) || detail::birdLook(species, ape) || detail::chainLook(species, ape)) return ape;
    QuadSpec q;
    if (species == "dog") q = QuadSpec::dog();
    else if (species == "cat") q = QuadSpec::cat();
    else if (species == "horse") q = QuadSpec::horse();
    else if (species == "cow") q = QuadSpec::cow();
    else if (species == "giraffe") q = QuadSpec::giraffe();
    else if (species == "turtle") q = QuadSpec::turtle();
    else throw std::runtime_error("rig: unknown animal \"" + species + "\"");
    Look l;
    l.build = quadruped(q);
    l.head = "head." + species;
    l.hair = species == "horse" ? "mane.horse" : species == "giraffe" ? "mane.giraffe" : "";
    l.hat = "";
    l.torso = "body.quad";
    l.hips = "tail." + species;
    l.extra = species == "cow" ? "coat.cow" : species == "giraffe" ? "coat.giraffe" : species == "turtle" ? "shell.turtle" : "";
    l.arms("").hands("").legs("leg.quad").feet("");
    l.face = nullptr;
    l.finish = Finish::Matte;
    l.outfit = Outfit{};
    l.outfit.fallback = l.skin;
    if (species == "dog") {
        l.skin = { 206, 152, 84, 255 }; l.hairColor = { 168, 112, 58, 255 }; l.accent = { 222, 180, 120, 255 };
        l.dark = { 34, 28, 28, 255 };
    } else if (species == "cat") {
        l.skin = { 228, 140, 62, 255 }; l.hairColor = { 196, 108, 44, 255 }; l.accent = { 246, 228, 204, 255 };
        l.dark = { 40, 32, 30, 255 };
    } else if (species == "horse") {
        l.skin = { 138, 82, 46, 255 }; l.hairColor = { 34, 26, 24, 255 }; l.accent = { 96, 64, 46, 255 };
        l.shoes = { 58, 52, 48, 255 }; l.dark = { 24, 20, 20, 255 };
    } else if (species == "turtle") {
        l.skin = { 116, 98, 52, 255 }; l.hairColor = { 112, 132, 78, 255 }; l.accent = { 214, 190, 120, 255 };
        l.dark = { 70, 58, 30, 255 };
    } else if (species == "cow") {
        l.skin = { 246, 243, 236, 255 }; l.hairColor = { 32, 30, 32, 255 }; l.accent = { 236, 166, 164, 255 };
        l.shoes = { 62, 56, 52, 255 }; l.dark = { 34, 32, 34, 255 };
    } else {
        l.skin = { 236, 202, 134, 255 }; l.hairColor = { 112, 62, 32, 255 }; l.accent = { 222, 188, 124, 255 };
        l.shoes = { 60, 50, 42, 255 }; l.dark = { 150, 86, 42, 255 };
    }
    return l;
}

// ------------------------------------------------------------------ clips
namespace {
struct Leg { Joint top, mid, low, paw; bool front; };
// Left hind, left front, right hind, right front.
std::array<Leg, 4> legsOf(const Skeleton& k) {
    auto leg = [&](const LegNames& n, Side s, bool front) {
        return Leg{ J(k, n.top, s), J(k, n.mid, s), J(k, n.low, s), J(k, n.paw, s), front };
    };
    return { leg(HIND, Side::Left, false), leg(FRONT, Side::Left, true), leg(HIND, Side::Right, false), leg(FRONT, Side::Right, true) };
}
float reachOf(const Build& b, const Leg& l) { return Vector3Length(restOffset(b, l.mid)) + Vector3Length(restOffset(b, l.low)); }

// Each paw on its target, the cannon (wrist or hock to paw) tipped back by `tuck` degrees.
Pose legsTo(const Build& b, Pose p, const std::array<Leg, 4>& legs, const std::array<Vector3, 4>& at, const std::array<float, 4>& tuck) {
    for (size_t i = 0; i < 4; i++) {
        const Leg& l = legs[i];
        const float cannon = Vector3Length(restOffset(b, l.paw)), a = tuck[i];
        const Vector3 up{ 0, cosDeg(a), sinDeg(a) };
        const Vector3 wrist = Vector3Add(at[i], Vector3Scale(up, cannon));
        p = limb(b, p, l.top, l.mid, l.low, wrist, l.front ? Vector3{ 0, 0, -1 } : Vector3{ 0, 0, 1 }, l.front ? -1.f : 1.f);
        p = orient(b, p, l.low, up, { 0, -sinDeg(a), cosDeg(a) });
    }
    return p;
}
// Where each paw stands at rest (character space).
std::array<Vector3, 4> restPaws(const Build& b, const std::array<Leg, 4>& legs) {
    const Frames f = solve(b, Pose{});
    const float paw = b.plan->dim("paw");
    std::array<Vector3, 4> r;
    for (size_t i = 0; i < 4; i++) {
        const Vector3 q = position(f, legs[i].paw);
        r[i] = { q.x, paw, q.z };
    }
    return r;
}
// How far the body sinks so every leg reaches paws swept `half` ahead and behind, the
// knees still a little bent.
float sinkFor(const Build& b, const std::array<Leg, 4>& legs, float half, float bend) {
    float sink = 0;
    for (const Leg& l : legs) {
        const float R = reachOf(b, l) * (1 - bend);
        sink = std::max(sink, reachOf(b, l) - std::sqrt(std::max(0.f, R * R - half * half)));
    }
    return sink;
}
float legLength(const Skeleton& k) { return (k.dim("frontLength") + k.dim("hindLength")) / 2; }

struct GaitDef {
    std::array<float, 4> phase;              // when each paw lands: LH, LF, RH, RF (cycles)
    float duty;                              // share of the cycle each paw is down
    float stride;                            // ground per cycle, times the leg length
    float pace;                              // natural cadence: pace * sqrt(g / leg length)
    float lift;                              // swing height, times the leg length
    float tuckFront, tuckHind;               // degrees the cannons fold back mid-swing
    float bob;                               // body bounce, times the leg length
    float nod; int nods;                     // head nod (degrees) and nods per cycle
    float flex;                              // spine flex (degrees)
};
GaitDef gaitDef(Gait g) {
    switch (g) {
    case Gait::Walk: return { { 0, 0.25f, 0.5f, 0.75f }, 0.65f, 0.75f, 0.3f, 0.1f, 60, 25, 0.012f, 5, 2, 0 };
    case Gait::Trot: return { { 0.5f, 0, 0, 0.5f }, 0.45f, 1.25f, 0.42f, 0.14f, 75, 35, 0.02f, 2, 2, 0 };
    case Gait::Canter: return { { 0, 0.33f, 0.33f, 0.6f }, 0.4f, 1.5f, 0.45f, 0.16f, 85, 40, 0.03f, 9, 1, 5 };
    case Gait::Gallop: default: return { { 0, 0.45f, 0.1f, 0.55f }, 0.3f, 1.9f, 0.5f, 0.18f, 95, 45, 0.035f, 10, 1, 9 };
    }
}
void tailSway(const Skeleton& k, Pose& p, double s, float rate, float amp) {
    const char* names[3] = { "tail-1", "tail-2", "tail-3" };
    for (int i = 0; i < 3; i++) p[k[names[i]]].y = amp * (0.7f + 0.3f * i) * std::sin(TAU * rate * (float)s - 0.9f * i);
}
} // namespace

float gaitCadence(const Build& b, Gait g) {
    if (!b.plan && g == Gait::Walk) return detail::knuckleCadence(b);
    if (detail::isBiped(b)) return detail::bipedCadence(b, g);
    if (detail::isChain(b)) return detail::chainCadence(b, g);
    const Skeleton& k = *planOf(b, "gaitCadence");
    return gaitDef(g).pace * std::sqrt(9.81f / std::max(0.05f, legLength(k)));
}
float gaitSpeed(const Build& b, Gait g, float cadence) {
    if (!b.plan && g == Gait::Walk) return detail::knuckleSpeed(b, cadence);
    if (detail::isBiped(b)) return detail::bipedSpeed(b, g, cadence);
    if (detail::isChain(b)) return detail::chainSpeed(b, g, cadence);
    const Skeleton& k = *planOf(b, "gaitSpeed");
    const float c = cadence > 0 ? cadence : gaitCadence(b, g);
    return gaitDef(g).stride * legLength(k) * c;
}

namespace clips {
Clip stand(const Build& b) {
    if (!b.plan) return idle();
    if (detail::isBiped(b)) return detail::bipedStand(b);
    if (detail::isChain(b)) return detail::chainStand(b);
    const SkeletonPtr k = planOf(b, "clips::stand");
    const auto legs = legsOf(*k);
    const auto paws = restPaws(b, legs);
    const float sink = sinkFor(b, legs, 0, 0.03f), L = legLength(*k), headPitch = k->trait("headPitch");
    return [b, k, legs, paws, sink, L, headPitch](double s) {
        Pose p;
        const float t = (float)s;
        p.offset.y = -sink + 0.004f * L * std::sin(TAU * t / 3.4f);
        p[(*k)["spine"]].x = 0.8f * std::sin(TAU * t / 3.4f);
        // A slow look round, the head leading the neck.
        const float look = std::sin(TAU * t / 9.0f) * smooth(0.5f + 0.5f * std::sin(TAU * t / 4.5f));
        p[(*k)["neck-1"]] = { -2 + 3 * std::sin(TAU * t / 5.3f), 10 * look, 0 };
        p[(*k)["neck-2"]].y = 8 * look;
        p[(*k)["head"]] = { headPitch, 10 * look, 0 };
        tailSway(*k, p, s, 0.45f, 10);
        return legsTo(b, p, legs, paws, { 0, 0, 0, 0 });
    };
}

Clip gait(const Build& b, Gait g, float cadence) {
    if (!b.plan && g == Gait::Walk) return knuckleWalk(b, cadence);
    if (detail::isBiped(b)) return detail::bipedGait(b, g, cadence);
    if (detail::isChain(b)) return detail::chainGaitClip(b, g, cadence);
    const SkeletonPtr k = planOf(b, "clips::gait");
    const GaitDef d = gaitDef(g);
    const float c = cadence > 0 ? cadence : gaitCadence(b, g);
    const float L = legLength(*k), sweep = d.stride * L * d.duty, lift = d.lift * L;
    const auto legs = legsOf(*k);
    const auto paws = restPaws(b, legs);
    const float sink = sinkFor(b, legs, sweep / 2, 0.03f), headPitch = k->trait("headPitch");
    const float paw = k->dim("paw");
    return [b, k, d, c, L, sweep, lift, legs, paws, sink, headPitch, paw](double s) {
        const float u = frac(s * c);
        Pose p;
        const bool bound = d.flex > 0;           // canter and gallop: one bounce, the spine flexing
        p.offset.y = -sink + d.bob * L * (bound ? std::cos(TAU * (u - 0.2f)) : std::cos(2 * TAU * u));
        if (bound) {
            p[(*k)["pelvis"]].x = d.flex * std::sin(TAU * u);
            p[(*k)["chest"]].x = -1.2f * d.flex * std::sin(TAU * u);
        } else {
            p[(*k)["pelvis"]].z = 2 * std::sin(TAU * u);
            p[(*k)["pelvis"]].y = 2 * std::sin(TAU * u + 0.5f);
        }
        const float nod = d.nod * std::sin(TAU * d.nods * u);
        p[(*k)["neck-1"]].x = nod;
        p[(*k)["head"]].x = headPitch - 0.5f * nod;
        tailSway(*k, p, s, c, bound ? 4 : 9);
        if (bound) p[(*k)["tail-1"]].x = -12;
        std::array<Vector3, 4> at;
        std::array<float, 4> tuck{};
        for (size_t i = 0; i < 4; i++) {
            const float v = frac(u - d.phase[i]);
            Vector3 q = paws[i];
            if (v < d.duty) {
                q.z += sweep / 2 - sweep * (v / d.duty);
            } else {
                const float w = (v - d.duty) / (1 - d.duty);
                q.z += -sweep / 2 + sweep * smooth(w);
                q.y = paw + lift * std::sin(PI * w);
                tuck[i] = (legs[i].front ? d.tuckFront : d.tuckHind) * std::sin(PI * std::min(1.f, w * 1.15f));
            }
            at[i] = q;
        }
        return legsTo(b, p, legs, at, tuck);
    };
}

Clip paddle(const Build& b, float rate) {
    const SkeletonPtr k = planOf(b, "clips::paddle");
    const float r = rate > 0 ? rate : 0.5f * std::sqrt(9.81f / std::max(0.05f, legLength(*k)));
    const bool turtle = k->variant == "turtle";
    const auto legs = legsOf(*k);
    return [k, r, turtle, legs](double s) {
        const float ph = TAU * frac(s * r);
        Pose p;
        p.offset.y = 0.01f * legLength(*k) * std::sin(2 * ph);
        // Head up and out; the legs stroke in diagonal pairs, reaching forward high and
        // pushing back low (a turtle's front flippers sweep out wide, like wings).
        p[(*k)["neck-1"]].x = -15;
        p[(*k)["head"]].x = turtle ? 0 : -20;
        tailSway(*k, p, s, r, 6);
        for (size_t i = 0; i < 4; i++) {
            const Leg& l = legs[i];
            const float lag = (i == 0 || i == 3) ? 0 : PI;            // LH with RF, LF with RH
            const float w = std::sin(ph + lag), lift = std::cos(ph + lag);
            const float out = turtle ? (l.front ? 55.f : 30.f) : 8.f;
            if (l.front) {
                p[l.top] = { -40 - 40 * w, 0, out + (turtle ? 20 * lift : 0) };
                p[l.mid] = { 20 + 30 * std::max(0.f, lift), 0, 0 };
            } else {
                p[l.top] = { -10 - 35 * w, 0, out };
                p[l.mid] = { 25 + 25 * std::max(0.f, lift), 0, 0 };
            }
            p[l.low].x = l.front ? -20 : 30;
        }
        return p;
    };
}
} // namespace clips

} // namespace rig
} // namespace rmtn
