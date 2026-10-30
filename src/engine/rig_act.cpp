// rig:: reaching and acting -- two-bone IK, hands placed in space, and the clips that use
// them with furniture and props: sitting, typing, phones and tablets, placards, riot
// shields, skateboards and scooters. Every pose is computed from the time alone.
#include "rig_internal.hpp"
#include "rmtn/timeline.hpp"
#include <cmath>

namespace rmtn {
namespace rig {

namespace {
using namespace detail;
constexpr float TAU = 6.28318530718f;

float smooth(float u) {
    u = std::clamp(u, 0.f, 1.f);
    return u * u * (3 - 2 * u);
}
float frac(double x) { return (float)(x - std::floor(x)); }

// A rotation's axes: column i is where local axis i points (raymath's m0 m1 m2 / m4 m5 m6
// / m8 m9 m10).
Vector3 axis(const Matrix& m, int i) {
    if (i == 0) return { m.m0, m.m1, m.m2 };
    if (i == 1) return { m.m4, m.m5, m.m6 };
    return { m.m8, m.m9, m.m10 };
}
Matrix basis(Vector3 x, Vector3 y, Vector3 z) {
    Matrix m = MatrixIdentity();
    m.m0 = x.x; m.m1 = x.y; m.m2 = x.z;
    m.m4 = y.x; m.m5 = y.y; m.m6 = y.z;
    m.m8 = z.x; m.m9 = z.y; m.m10 = z.z;
    return m;
}
// The Euler XYZ angles (degrees) that MatrixRotateXYZ turns into `m`.
Vector3 eulerOf(const Matrix& m) {
    const float sy = std::clamp(m.m8, -1.f, 1.f);
    Vector3 r{ 0, std::asin(sy), 0 };
    if (std::fabs(sy) < 0.9999f) {
        r.x = -std::atan2(m.m9, m.m10);
        r.z = -std::atan2(m.m4, m.m0);
    } else r.x = std::atan2(m.m6, m.m5);                  // gimbal lock: all the turn in x
    return Vector3Scale(r, RAD2DEG);
}
Vector3 perp(Vector3 v, Vector3 n) {                      // v without its component along unit n
    return Vector3Subtract(v, Vector3Scale(n, Vector3DotProduct(v, n)));
}
Vector3 unitPerp(Vector3 v, Vector3 n) {
    Vector3 r = perp(v, n);
    if (Vector3Length(r) < 1e-5f) r = Vector3Perpendicular(n);
    return Vector3Normalize(r);
}

// Joint j's local rotation set so that, in character space, its axes are x, y, z.
void setWorld(const Build& b, Pose& p, Joint j, Vector3 x, Vector3 y, Vector3 z) {
    Vector3 px{ 1, 0, 0 }, py{ 0, 1, 0 }, pz{ 0, 0, 1 };
    if (!isGrounded(b, j)) {
        const auto f = solve(b, p);
        const Matrix& P = f[(size_t)parent(b, j)];
        px = axis(P, 0); py = axis(P, 1); pz = axis(P, 2);
    }
    auto local = [&](Vector3 d) { return Vector3{ Vector3DotProduct(px, d), Vector3DotProduct(py, d), Vector3DotProduct(pz, d) }; };
    Vector3 e = eulerOf(basis(local(x), local(y), local(z)));
    if (isRight(b, j)) { e.y = -e.y; e.z = -e.z; }
    p[j] = e;
}

// Upper joint and middle joint turned so the end of a two-bone chain (lengths a, c) lands
// on `target`, the middle joint towards `pole`. `flex` is the sign of the middle joint's
// x bend (-1 an elbow, +1 a knee).
Pose twoBone(const Build& b, Pose p, Joint upper, Joint mid, float a, float c, Vector3 target, Vector3 pole, float flex) {
    const Vector3 S = position(solve(b, p), upper);
    const Vector3 v = Vector3Subtract(target, S);
    const float len = Vector3Length(v);
    const Vector3 f1 = len > 1e-6f ? Vector3Scale(v, 1 / len) : Vector3{ 0, -1, 0 };
    const float d = std::clamp(len, std::fabs(a - c) + 1e-4f, a + c - 1e-4f);
    const float bend = PI - std::acos(std::clamp((a * a + c * c - d * d) / (2 * a * c), -1.f, 1.f));
    p[mid] = { flex * bend * RAD2DEG, 0, 0 };
    // The chain in the upper joint's own frame (no turn yet): the end, and the side the
    // middle joint juts out to.
    const Vector3 E0{ 0, -a, 0 };
    const Vector3 W0 = Vector3Add(E0, Vector3Transform({ 0, -c, 0 }, MatrixRotateXYZ({ flex * bend, 0, 0 })));
    const Vector3 e1 = Vector3Normalize(W0), e2 = unitPerp({ 0, 0, flex }, e1), e3 = Vector3CrossProduct(e1, e2);
    const Vector3 f2 = unitPerp(pole, f1), f3 = Vector3CrossProduct(f1, f2);
    // The turn taking e1 e2 e3 to f1 f2 f3; its columns are where the joint's axes point.
    auto image = [&](float k1, float k2, float k3) {
        return Vector3Add(Vector3Add(Vector3Scale(f1, k1), Vector3Scale(f2, k2)), Vector3Scale(f3, k3));
    };
    setWorld(b, p, upper, image(e1.x, e2.x, e3.x), image(e1.y, e2.y, e3.y), image(e1.z, e2.z, e3.z));
    return p;
}

Joint wristOf(Side s) { return s == Side::Left ? Joint::WristL : Joint::WristR; }
Joint ankleOf(Side s) { return s == Side::Left ? Joint::AnkleL : Joint::AnkleR; }
float sideSign(Side s) { return s == Side::Left ? 1.f : -1.f; }
Vector3 mirrorX(Vector3 v, Side s) { return { v.x * sideSign(s), v.y, v.z }; }
Vector3 add(Vector3 a, Vector3 b) { return Vector3Add(a, b); }
Vector3 scale(Vector3 a, float k) { return Vector3Scale(a, k); }

// Heights on the standing body (character space).
float chestY(const Build& b) { return hipHeight(b) + b.hips / 2 + b.torso.y * 0.6f; }
float shoulderY(const Build& b) { return hipHeight(b) + b.hips / 2 + b.torso.y - b.armRadius * 1.2f; }
Vector3 eyes(const Build& b, float drop = 0) {
    return { 0, hipHeight(b) + b.hips / 2 + b.torso.y + b.neck + b.head * 0.45f - drop, b.head * 0.35f };
}

Pose base(double s, float energy) { return clips::idle(energy)(s); }
// Both feet flat on the floor, toes forward.
Pose flatFeet(const Build& b, Pose p) {
    p = orient(b, p, Joint::AnkleL, { 0, 1, 0 }, { 0, 0, 1 });
    return orient(b, p, Joint::AnkleR, { 0, 1, 0 }, { 0, 0, 1 });
}
// The pose's hips lowered, if need be, until `side`'s ankle can reach `at` with the knee
// still a little bent (a skater's standing knee giving as the other foot pushes).
void sinkToReach(const Build& b, Pose& p, Side side, Vector3 at) {
    const Vector3 hip = position(solve(b, p), side == Side::Left ? Joint::HipL : Joint::HipR);
    const float reachMax = (b.thigh + b.shin) * 0.97f;
    const float dx = hip.x - at.x, dz = hip.z - at.z, h2 = dx * dx + dz * dz;
    if (h2 >= reachMax * reachMax) return;
    const float y = at.y + std::sqrt(reachMax * reachMax - h2);
    if (hip.y > y) p.offset.y -= hip.y - y;
}
// A leg's ankle on `at` with the foot flat.
Pose footOn(const Build& b, Pose p, Side s, Vector3 at, Vector3 pole = { 0, 0, 1 }) {
    p = plant(b, p, s, at, pole);
    return orient(b, p, ankleOf(s), { 0, 1, 0 }, { 0, 0, 1 });
}
} // namespace

// ------------------------------------------------------------------ IK
Pose reach(const Build& b, Pose p, Side side, Vector3 target, Vector3 pole) {
    const bool l = side == Side::Left;
    return twoBone(b, p, l ? Joint::ShoulderL : Joint::ShoulderR, l ? Joint::ElbowL : Joint::ElbowR, b.upperArm, b.forearm,
                   target, mirrorX(pole, side), -1);
}

Pose plant(const Build& b, Pose p, Side side, Vector3 target, Vector3 pole) {
    const bool l = side == Side::Left;
    return twoBone(b, p, l ? Joint::HipL : Joint::HipR, l ? Joint::KneeL : Joint::KneeR, b.thigh, b.shin, target,
                   mirrorX(pole, side), 1);
}

Pose limb(const Build& b, Pose p, Joint upper, Joint mid, Joint end, Vector3 target, Vector3 pole, float flex) {
    return twoBone(b, p, upper, mid, Vector3Length(restOffset(b, mid)), Vector3Length(restOffset(b, end)), target, pole, flex);
}

Pose orient(const Build& b, Pose p, Joint j, Vector3 up, Vector3 forward) {
    const Vector3 y = Vector3Normalize(up), z = unitPerp(forward, y);
    setWorld(b, p, j, Vector3CrossProduct(y, z), y, z);
    return p;
}

Pose hand(const Build& b, Pose p, Side side, Vector3 at, Vector3 fingers, Vector3 palm, Vector3 pole) {
    const Vector3 fd = Vector3Normalize(fingers), pn = unitPerp(palm, fd);
    p = reach(b, p, side, Vector3Subtract(at, Vector3Scale(fd, b.hand * 0.5f)), pole);
    // The wrist's frame: -y along the fingers, the palm on -x (left hand) or +x (right).
    const Vector3 y = Vector3Negate(fd), x = side == Side::Left ? Vector3Negate(pn) : pn;
    setWorld(b, p, wristOf(side), x, y, Vector3CrossProduct(x, y));
    return p;
}

Vector3 palmAt(const Build& b, const Pose& p, Side side) {
    return Vector3Transform({ 0, -b.hand * 0.5f, 0 }, solve(b, p)[(size_t)wristOf(side)]);
}

// ------------------------------------------------------------------ mounts
Matrix frameAt(const Character& ch, double s, Joint j) {
    return solve(ch.lookAt(s).build, ch.poseAt(s))[(size_t)j];
}

namespace detail {
Pose mounted(const Character& ch, double s, Pose p) {
    const Pose own = p;
    const Build& b = ch.lookAt(s).build;
    for (const Mount& m : ch.mounts()) {
        const auto carrier = m.carrier.lock();
        if (!carrier || s < m.from - m.blend || s > m.until + m.blend) continue;
        float w = 1;
        if (m.blend > 0) w = smooth((float)((s - (m.from - m.blend)) / m.blend)) * smooth((float)((m.until + m.blend - s) / m.blend));
        if (w <= 0) continue;
        const Build& cb = carrier->lookAt(s).build;
        const Frames cf = solve(cb, carrier->poseAt(s));
        // The rider's root: its own root motion, in the mount's frame, in the carrier joint's.
        const Matrix rootOwn = MatrixMultiply(MatrixRotateXYZ(Vector3Scale(own[Joint::Root], DEG2RAD)), MatrixTranslate(own.offset.x, own.offset.y, own.offset.z));
        const Matrix attach = MatrixMultiply(MatrixRotateXYZ(Vector3Scale(m.rot, DEG2RAD)), MatrixTranslate(m.at.x, m.at.y, m.at.z));
        const Matrix M = MatrixMultiply(MatrixMultiply(rootOwn, attach), cf[(size_t)m.joint]);
        Pose q = own;
        q[Joint::Root] = eulerOf(M);
        q.offset = { M.m12, M.m13, M.m14 };
        if (!b.plan)
            for (const Grip& g : m.grips) {
                const Vector3 at = Vector3Transform(g.at, cf[(size_t)g.joint]);
                switch (g.limb) {
                case Limb::HandL: q = reach(b, q, Side::Left, at, { 0.5f, -0.6f, -1 }); break;
                case Limb::HandR: q = reach(b, q, Side::Right, at, { 0.5f, -0.6f, -1 }); break;
                case Limb::FootL: q = plant(b, q, Side::Left, at, { 0.2f, 0, 1 }); break;
                case Limb::FootR: q = plant(b, q, Side::Right, at, { 0.2f, 0, 1 }); break;
                }
            }
        p = w >= 1 ? q : Pose::mix(p, q, w);
    }
    return p;
}
} // namespace detail

Mount saddle(std::shared_ptr<const Character> horse, double from, double until) {
    const Build& hb = horse->looks()[0].build;
    if (!hb.plan || hb.plan->kind != "quadruped") throw std::runtime_error("rig::saddle needs a four-legged animal");
    const Skeleton& k = *hb.plan;
    const float r = k.dim("bodyRadius"), L = k.dim("bodyLength");
    Mount m;
    m.carrier = horse;
    m.joint = k["spine"];
    m.at = { 0, r * 1.02f, L * 0.06f };
    m.from = from; m.until = until;
    for (Side s : { Side::Left, Side::Right }) {
        const float x = s == Side::Left ? 1.f : -1.f;
        m.grips.push_back({ s == Side::Left ? Limb::FootL : Limb::FootR, k["spine"], { x * r * 1.12f, -r * 0.3f, L * 0.12f } });
        m.grips.push_back({ s == Side::Left ? Limb::HandL : Limb::HandR, k["chest"], { x * r * 0.22f, r * 1.25f, r * 0.15f } });
    }
    return m;
}

// ------------------------------------------------------------------ acting
namespace clips {

Clip ride(const Build& b) {
    return [b](double s) {
        const float t = (float)s;
        Pose p;
        p.offset = { 0, -hipHeight(b), 0 };          // the hips at the origin: on the seat
        p[Joint::Pelvis].x = 4;
        p[Joint::Chest].x = 6 + 2 * std::sin(TAU * t * 0.5f);
        p[Joint::Head].x = -6;
        for (Joint h : { Joint::HipL, Joint::HipR }) p[h] = { -55, 0, 28 };
        for (Joint k : { Joint::KneeL, Joint::KneeR }) p[k] = { 70, 0, 0 };
        for (Joint sh : { Joint::ShoulderL, Joint::ShoulderR }) p[sh] = { -30, 0, 8 };
        for (Joint e : { Joint::ElbowL, Joint::ElbowR }) p[e] = { -50, 0, 0 };
        return p;
    };
}

Clip seated(const Build& b, float seat) {
    return [b, seat](double s) {
        Pose p;
        const float hipY = seat + b.legRadius * 0.9f;
        p.offset = { 0, hipY - hipHeight(b), -b.thigh * 0.4f };
        p.offset.y += 0.004f * std::sin((float)s * TAU * 0.25f);          // breathing
        p[Joint::HipL] = { -90, 0, 4 };
        p[Joint::HipR] = { -90, 0, 4 };
        p[Joint::Chest].x = 4 + 1.5f * std::sin((float)s * TAU * 0.25f);
        const float kneeZ = p.offset.z + b.thigh;
        const float hang = hipY - b.shin - b.ankle;                         // foot's height if the shin hangs
        for (Side sd : { Side::Left, Side::Right }) {
            const Joint knee = sd == Side::Left ? Joint::KneeL : Joint::KneeR;
            if (hang < 0.01f) {
                // The shins reach the floor: feet planted a little ahead of the knees.
                p = footOn(b, p, sd, { sideSign(sd) * b.hipWidth * 0.55f, b.ankle, kneeZ + b.foot * 0.25f });
            } else {
                // Dangling: the shins hang and swing lazily, out of step.
                const float ph = (float)s * TAU * 0.45f + (sd == Side::Left ? 0 : 2.2f);
                p[knee] = { 82 + 9 * std::sin(ph), 0, 0 };
                p[ankleOf(sd)] = { -10, 0, 0 };
            }
        }
        // Hands resting on the thighs, palms down.
        for (Side sd : { Side::Left, Side::Right })
            p = hand(b, p, sd, { sideSign(sd) * (b.hipWidth * 0.5f + b.legRadius * 0.3f), hipY + b.legRadius + b.hand * 0.2f,
                                 p.offset.z + b.thigh * 0.6f },
                     { -sideSign(sd) * 0.2f, -0.2f, 1 }, { 0, -1, 0 }, { 0.6f, -0.2f, -1 });
        return p;
    };
}

Clip typing(const Build& b, float seat, Vector3 keys, float rate) {
    const Clip sit = seated(b, seat);
    return [b, keys, rate, sit](double s) {
        Pose p = sit(s);
        p[Joint::Chest].x = 9;
        p[Joint::Neck].x = 6;
        p[Joint::Head] = { 8, 4 * std::sin((float)s * 0.7f), 0 };
        const float spread = b.torso.x * 0.3f, k = propScale(b);
        for (Side sd : { Side::Left, Side::Right }) {
            // Keystrokes: a short lift and drop in irregular slots, the hands out of step.
            const double slot = s * rate * 7 + (sd == Side::Left ? 0 : 0.5);
            const auto n = (unsigned long long)std::floor(slot);
            const float on = rnd01(sd == Side::Left ? 71 : 73, n) > 0.3 ? 1.f : 0.f;
            const float tap = 0.014f * k * on * std::sin(PI * frac(slot));
            const float drift = 0.03f * k * ((float)rnd01(sd == Side::Left ? 79 : 83, n / 6) - 0.5f);
            const Vector3 at{ keys.x + sideSign(sd) * spread + drift, keys.y + b.hand * 0.3f + tap, keys.z - b.hand * 0.2f };
            p = hand(b, p, sd, at, { -sideSign(sd) * 0.2f, -0.45f, 1 }, { 0, -1, 0.15f }, { 0.7f, -1, -0.3f });
        }
        return p;
    };
}

Clip texting(const Build& b, Side side) {
    return [b, side](double s) {
        Pose p = base(s, 0.4f);
        p[Joint::Neck].x = 14;
        p[Joint::Head].x = 12;
        const float k = propScale(b);
        const float thumb = 0.005f * k * std::sin((float)s * TAU * 2.6f) * (rnd01(91, (unsigned long long)(s * 1.5)) > 0.25 ? 1.f : 0.f);
        const Vector3 at{ sideSign(side) * b.torso.x * 0.12f, chestY(b) + 0.03f * k + thumb, b.torso.z * 0.5f + 0.2f * k };
        const Vector3 toEyes = Vector3Normalize(Vector3Subtract(eyes(b, b.head * 0.15f), at));
        p = hand(b, p, side, at, { 0, 1, 0.35f }, toEyes, { 0.3f, -1, -0.4f });
        return p;
    };
}

Clip calling(const Build& b, Side side) {
    return [b, side](double s) {
        Pose p = base(s, 0.5f);
        p[Joint::Head].z = -sideSign(side) * 6;
        const Vector3 ear{ sideSign(side) * (b.head * 0.46f + b.hand * 0.32f), eyes(b).y - b.head * 0.08f, b.head * 0.05f };
        p = hand(b, p, side, ear, { 0, 1, 0.25f }, { -sideSign(side), 0, 0 }, { 0.6f, -1, 0.1f });
        return p;
    };
}

Clip tablet(const Build& b) {
    return [b](double s) {
        Pose p = base(s, 0.35f);
        p[Joint::Neck].x = 12;
        p[Joint::Head].x = 10;
        const float k = propScale(b);
        // The left palm cradles the tablet, its screen turned up to the eyes.
        const Vector3 at{ b.torso.x * 0.22f, chestY(b) - b.torso.y * 0.02f, b.torso.z * 0.5f + 0.24f * k };
        // Tipped up towards the eyes, but held at a reading slant rather than flat.
        const Vector3 toEyes = Vector3Normalize(Vector3Add(Vector3Normalize(Vector3Subtract(eyes(b), at)), { 0, 0, -0.9f }));
        const Vector3 across = Vector3Normalize(perp({ -1, 0, 0 }, toEyes));
        p = hand(b, p, Side::Left, at, across, toEyes, { 0.6f, -1, -0.2f });
        // The right hand taps points on the screen.
        const Vector3 screen = add(add(palmAt(b, p, Side::Left), scale(toEyes, b.hand * 0.31f + 0.02f * k)),
                                   scale(across, 0.08f * k));
        const auto n = (unsigned long long)std::floor(s * 1.4);
        const Vector3 spot = add(screen, add(scale(across, 0.05f * k * ((float)rnd01(97, n) - 0.5f)),
                                             scale(Vector3CrossProduct(across, toEyes), 0.06f * k * ((float)rnd01(101, n) - 0.5f))));
        const float lift = (0.015f + 0.03f * (0.5f + 0.5f * std::cos(frac(s * 1.4) * TAU))) * k;
        const Vector3 point = Vector3Negate(toEyes);
        p = hand(b, p, Side::Right, add(spot, scale(toEyes, lift + b.hand * 0.5f)), point, { 0, -1, 0 }, { 0.4f, -1, -0.6f });
        return p;
    };
}

Clip protest(const Build& b, float beat) {
    return [b, beat](double s) {
        Pose p = base(s, 0.6f);
        const SignGeom g = signGeom(b);
        const float k = propScale(b);
        const float pump = 0.07f * k * std::pow(std::max(0.f, std::sin((float)s * TAU * beat)), 2.f);
        p[Joint::Chest].x = -4;                                 // set before the hands: they hang from it
        p[Joint::Head].x = -6 - 4 * std::max(0.f, std::sin((float)s * TAU * beat));
        // Fists round the pole, which stands off to the bearer's right so the face shows:
        // fingers forward, palms in towards it, thumbs up.
        const float z = b.torso.z * 0.5f + 0.13f * k, pole = -(b.head * 0.36f + 0.03f * k), x = b.hand * 0.3f;
        p = hand(b, p, Side::Right, { pole - x, g.hand + pump, z }, { 0, 0, 1 }, { 1, 0, 0 }, { 0.8f, -1, -0.4f });
        p = hand(b, p, Side::Left, { pole + x, g.hand + pump - (g.grip - 0.04f * k), z }, { 0, 0, 1 }, { -1, 0, 0 }, { 0.8f, -1, -0.4f });
        return p;
    };
}

Clip shieldLine(const Build& b, bool advance) {
    return [b, advance](double s) {
        Pose p;
        const float k = propScale(b);
        p.offset.y = -0.07f * k + 0.004f * std::sin((float)s * TAU * 0.3f);
        p[Joint::Chest].x = 8;
        p[Joint::Neck].x = -4;
        for (Side sd : { Side::Left, Side::Right }) {
            // Left foot ahead, right behind; stepping in place when advancing.
            float lift = 0;
            if (advance) lift = 0.05f * k * std::max(0.f, std::sin((float)s * TAU * 0.9f + (sd == Side::Left ? 0 : PI)));
            const float zf = sd == Side::Left ? 0.16f * k : -0.12f * k;
            p = footOn(b, p, sd, { sideSign(sd) * (b.hipWidth * 0.5f + 0.04f * k), b.ankle + lift, zf });
        }
        // The shield's handle in the left fist, square to the front.
        p = hand(b, p, Side::Left, { b.torso.x * 0.1f, chestY(b) - 0.1f * k, b.torso.z * 0.5f + 0.2f * k }, { 0, 0, 1 },
                 { -1, 0, 0 }, { 1, -0.6f, -0.6f });
        // The baton raised over the right shoulder, tipped back.
        p = hand(b, p, Side::Right, { -b.torso.x * 0.6f, shoulderY(b) + 0.06f * k, 0.02f * k }, { 0, 0.35f, 1 },
                 { 1, 0, 0 }, { 0.6f, -1, -0.3f });
        return p;
    };
}

Clip skate(const Build& b, float period) {
    return [b, period](double s) {
        Pose p;
        const BoardGeom g = boardGeom(b);
        const float k = propScale(b);
        const float foot = g.top + b.ankle;
        // How far through a push the right foot is: 0 riding, else off the board.
        const float u = period > 0 ? frac(s / period) : 1.f;
        const float push = u < 0.55f ? smooth(u / 0.12f) * (1 - smooth((u - 0.42f) / 0.13f)) : 0.f;
        p.offset.y = g.top - 0.04f * k - 0.06f * k * push + 0.004f * std::sin((float)s * TAU * 0.5f);
        p[Joint::Chest] = { 6 + 6 * push, 0, 0 };
        p[Joint::Head].x = -4;
        Vector3 rf{ -g.length * 0.27f, foot, 0 };
        if (push > 0) {
            // Down beside the board's front, a sweep back along the ground, up again.
            const float side = g.width * 0.5f + b.foot * 0.35f;
            const float sweep = smooth((u - 0.12f) / 0.30f);
            const Vector3 ground{ g.length * (0.12f - 0.7f * sweep), b.ankle, side };
            const float arc = 0.06f * k * std::sin(PI * std::clamp(u < 0.3f ? u / 0.12f : (u - 0.42f) / 0.13f, 0.f, 1.f));
            const bool onGround = u > 0.12f && u < 0.42f;
            rf = Vector3Lerp(rf, ground, push);
            if (!onGround) rf.y += arc;
        }
        sinkToReach(b, p, Side::Right, rf);
        p = footOn(b, p, Side::Left, { g.length * 0.27f, foot, 0 }, { 0.3f, 0, 1 });
        p = footOn(b, p, Side::Right, rf, { -0.2f, 0, 1 });
        // Arms out a little for balance, swinging with the push.
        p[Joint::ShoulderL] = { -10 + 20 * push, 0, 28 };
        p[Joint::ShoulderR] = { -10 - 25 * push, 0, 24 };
        p[Joint::ElbowL] = p[Joint::ElbowR] = { -25, 0, 0 };
        return p;
    };
}

Clip scooter(const Build& b, float period) {
    return [b, period](double s) {
        Pose p;
        const ScooterGeom g = scooterGeom(b);
        const float k = propScale(b);
        const float foot = g.top + b.ankle;
        const float u = period > 0 ? frac(s / period) : 1.f;
        const float kick = u < 0.7f ? smooth(u / 0.15f) * (1 - smooth((u - 0.5f) / 0.2f)) : 0.f;
        p.offset.y = g.top - 0.025f * k - 0.05f * k * kick;
        p[Joint::Chest].x = 10;
        p[Joint::Neck].x = -6;
        Vector3 rf{ -0.03f * k, foot, -0.17f * k };
        if (kick > 0) {
            const float sweep = smooth((u - 0.15f) / 0.35f);
            const Vector3 ground{ -(g.width * 0.5f + 0.08f * k), b.ankle, 0.1f * k - 0.45f * k * sweep };
            rf = Vector3Lerp(rf, ground, kick);
            if (u < 0.15f || u > 0.5f) rf.y += 0.04f * k * kick;
        }
        sinkToReach(b, p, Side::Right, rf);
        p = footOn(b, p, Side::Left, { 0.03f * k, foot, 0.02f * k }, { 0, 0, 1 });
        p = footOn(b, p, Side::Right, rf, { 0, 0, 1 });
        // Overhand grips on the bar ends.
        for (Side sd : { Side::Left, Side::Right }) {
            const Vector3 bar{ sideSign(sd) * (g.barW * 0.5f - b.hand * 0.55f), g.barY, g.barZ };
            p = hand(b, p, sd, add(bar, { 0, b.hand * 0.3f, 0 }), { 0, -0.2f, 1 }, { 0, -1, 0 }, { 0.8f, -0.4f, -1 });
        }
        return p;
    };
}

} // namespace clips
} // namespace rig
} // namespace rmtn
