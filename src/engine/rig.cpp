// rig:: core -- skeleton, poses, clips, motion, face timing, outfits, looks, characters.
#include "rmtn/rig.hpp"
#include "rmtn/media.hpp"
#include "rig_internal.hpp"
#include <raymath.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <tuple>
#include <sstream>
#include <stdexcept>

namespace rmtn {
namespace rig {

namespace {
constexpr float TAU = 6.28318530718f;
float smooth01(double u) {
    const float x = (float)std::clamp(u, 0.0, 1.0);
    return x * x * (3 - 2 * x);
}
float lerpf(float a, float b, float u) { return a + (b - a) * u; }
Vector3 lerp3(Vector3 a, Vector3 b, float u) { return { lerpf(a.x, b.x, u), lerpf(a.y, b.y, u), lerpf(a.z, b.z, u) }; }
Color lerpc(Color a, Color b, float u) {
    return { (unsigned char)std::lround(lerpf(a.r, b.r, u)), (unsigned char)std::lround(lerpf(a.g, b.g, u)),
             (unsigned char)std::lround(lerpf(a.b, b.b, u)), (unsigned char)std::lround(lerpf(a.a, b.a, u)) };
}
unsigned nextOutfitId() {
    static unsigned id = 0;
    return ++id;
}
} // namespace

// ------------------------------------------------------------------ skeleton
const char* name(Joint j) {
    static const char* names[kJoints] = {
        "root", "pelvis", "chest", "neck", "head",
        "shoulder-L", "elbow-L", "wrist-L", "shoulder-R", "elbow-R", "wrist-R",
        "hip-L", "knee-L", "ankle-L", "hip-R", "knee-R", "ankle-R" };
    const int i = (int)j;
    return i >= 0 && i < kJoints ? names[i] : "?";
}

Joint parent(Joint j) {
    switch (j) {
    case Joint::Root: case Joint::Pelvis: return Joint::Root;
    case Joint::Chest: case Joint::HipL: case Joint::HipR: return Joint::Pelvis;
    case Joint::Neck: case Joint::ShoulderL: case Joint::ShoulderR: return Joint::Chest;
    case Joint::Head: return Joint::Neck;
    case Joint::ElbowL: return Joint::ShoulderL;
    case Joint::WristL: return Joint::ElbowL;
    case Joint::ElbowR: return Joint::ShoulderR;
    case Joint::WristR: return Joint::ElbowR;
    case Joint::KneeL: return Joint::HipL;
    case Joint::AnkleL: return Joint::KneeL;
    case Joint::KneeR: return Joint::HipR;
    case Joint::AnkleR: return Joint::KneeR;
    default: return Joint::Root;
    }
}

bool isRight(Joint j) {
    return j == Joint::ShoulderR || j == Joint::ElbowR || j == Joint::WristR ||
           j == Joint::HipR || j == Joint::KneeR || j == Joint::AnkleR;
}

Build Build::adult() {
    Build b;
    b.head = 0.24f; b.neck = 0.08f; b.torso = { 0.38f, 0.48f, 0.22f }; b.hips = 0.16f; b.hipWidth = 0.19f;
    b.upperArm = 0.30f; b.forearm = 0.27f; b.armRadius = 0.05f; b.hand = 0.09f;
    b.thigh = 0.44f; b.shin = 0.42f; b.legRadius = 0.07f; b.foot = 0.25f; b.ankle = 0.08f;
    return b;
}
Build Build::toon() {
    Build b;
    b.head = 0.42f; b.neck = 0.04f; b.torso = { 0.34f, 0.36f, 0.22f }; b.hips = 0.14f; b.hipWidth = 0.16f;
    b.upperArm = 0.22f; b.forearm = 0.20f; b.armRadius = 0.055f; b.hand = 0.10f;
    b.thigh = 0.24f; b.shin = 0.22f; b.legRadius = 0.07f; b.foot = 0.19f; b.ankle = 0.06f;
    return b;
}
Build Build::block() {
    Build b;
    b.head = 0.34f; b.neck = 0.05f; b.torso = { 0.46f, 0.44f, 0.22f }; b.hips = 0.10f; b.hipWidth = 0.23f;
    b.upperArm = 0.22f; b.forearm = 0.20f; b.armRadius = 0.07f; b.hand = 0.09f;
    b.thigh = 0.20f; b.shin = 0.20f; b.legRadius = 0.11f; b.foot = 0.22f; b.ankle = 0.05f;
    return b;
}
Build Build::chibi() {
    Build b;
    b.head = 0.55f; b.neck = 0.03f; b.torso = { 0.30f, 0.26f, 0.20f }; b.hips = 0.12f; b.hipWidth = 0.14f;
    b.upperArm = 0.15f; b.forearm = 0.14f; b.armRadius = 0.05f; b.hand = 0.09f;
    b.thigh = 0.15f; b.shin = 0.14f; b.legRadius = 0.065f; b.foot = 0.16f; b.ankle = 0.05f;
    return b;
}
float Build::height() const { return ankle + shin + thigh + hips / 2 + torso.y + neck + head; }
Build Build::scaled(float k) const {
    Build b = *this;
    b.head *= k; b.neck *= k; b.torso = Vector3Scale(torso, k); b.hips *= k; b.hipWidth *= k;
    b.upperArm *= k; b.forearm *= k; b.armRadius *= k; b.hand *= k;
    b.thigh *= k; b.shin *= k; b.legRadius *= k; b.foot *= k; b.ankle *= k;
    return b;
}
Build Build::mix(const Build& a, const Build& b, float u) {
    Build r;
    r.head = lerpf(a.head, b.head, u); r.neck = lerpf(a.neck, b.neck, u); r.torso = lerp3(a.torso, b.torso, u);
    r.hips = lerpf(a.hips, b.hips, u); r.hipWidth = lerpf(a.hipWidth, b.hipWidth, u);
    r.upperArm = lerpf(a.upperArm, b.upperArm, u); r.forearm = lerpf(a.forearm, b.forearm, u);
    r.armRadius = lerpf(a.armRadius, b.armRadius, u); r.hand = lerpf(a.hand, b.hand, u);
    r.thigh = lerpf(a.thigh, b.thigh, u); r.shin = lerpf(a.shin, b.shin, u);
    r.legRadius = lerpf(a.legRadius, b.legRadius, u); r.foot = lerpf(a.foot, b.foot, u);
    r.ankle = lerpf(a.ankle, b.ankle, u);
    return r;
}

Vector3 restOffset(const Build& b, Joint j) {
    const float sx = b.torso.x / 2 + b.armRadius * 0.6f, sy = b.torso.y - b.armRadius * 1.2f;
    switch (j) {
    case Joint::Root: return { 0, 0, 0 };
    case Joint::Pelvis: return { 0, b.ankle + b.shin + b.thigh, 0 };
    case Joint::Chest: return { 0, b.hips / 2, 0 };
    case Joint::Neck: return { 0, b.torso.y, 0 };
    case Joint::Head: return { 0, b.neck, 0 };
    case Joint::ShoulderL: return { sx, sy, 0 };
    case Joint::ShoulderR: return { -sx, sy, 0 };
    case Joint::ElbowL: case Joint::ElbowR: return { 0, -b.upperArm, 0 };
    case Joint::WristL: case Joint::WristR: return { 0, -b.forearm, 0 };
    case Joint::HipL: return { b.hipWidth / 2, 0, 0 };
    case Joint::HipR: return { -b.hipWidth / 2, 0, 0 };
    case Joint::KneeL: case Joint::KneeR: return { 0, -b.thigh, 0 };
    case Joint::AnkleL: case Joint::AnkleR: return { 0, -b.shin, 0 };
    default: return { 0, 0, 0 };
    }
}

// ------------------------------------------------------------------ poses
Pose Pose::mix(const Pose& a, const Pose& b, float u) {
    Pose r;
    for (int i = 0; i < kJoints; i++) r.rot[(size_t)i] = lerp3(a.rot[(size_t)i], b.rot[(size_t)i], u);
    r.offset = lerp3(a.offset, b.offset, u);
    return r;
}
Pose Pose::operator+(const Pose& o) const {
    Pose r;
    for (int i = 0; i < kJoints; i++) r.rot[(size_t)i] = Vector3Add(rot[(size_t)i], o.rot[(size_t)i]);
    r.offset = Vector3Add(offset, o.offset);
    return r;
}
Pose Pose::operator*(float k) const {
    Pose r;
    for (int i = 0; i < kJoints; i++) r.rot[(size_t)i] = Vector3Scale(rot[(size_t)i], k);
    r.offset = Vector3Scale(offset, k);
    return r;
}
Pose Pose::mirrored() const {
    Pose r = *this;
    auto swap = [&](Joint a, Joint b) { std::swap(r[a], r[b]); };
    swap(Joint::ShoulderL, Joint::ShoulderR); swap(Joint::ElbowL, Joint::ElbowR); swap(Joint::WristL, Joint::WristR);
    swap(Joint::HipL, Joint::HipR); swap(Joint::KneeL, Joint::KneeR); swap(Joint::AnkleL, Joint::AnkleR);
    for (Joint j : { Joint::Root, Joint::Pelvis, Joint::Chest, Joint::Neck, Joint::Head }) {
        r[j].y = -r[j].y;
        r[j].z = -r[j].z;
    }
    r.offset.x = -r.offset.x;
    return r;
}

Vector3 jointRotation(const Pose& p, Joint j) {
    Vector3 r = p[j];
    if (isRight(j)) { r.y = -r.y; r.z = -r.z; }
    return r;
}

std::array<Matrix, kJoints> solve(const Build& b, const Pose& p) {
    std::array<Matrix, kJoints> w{};
    for (int i = 0; i < kJoints; i++) {
        const Joint j = (Joint)i;
        Vector3 off = restOffset(b, j);
        if (j == Joint::Root) off = Vector3Add(off, p.offset);
        const Matrix local = MatrixMultiply(MatrixRotateXYZ(Vector3Scale(jointRotation(p, j), DEG2RAD)),
                                            MatrixTranslate(off.x, off.y, off.z));
        w[(size_t)i] = j == Joint::Root ? local : MatrixMultiply(local, w[(size_t)parent(j)]);
    }
    return w;
}

Vector3 position(const std::array<Matrix, kJoints>& f, Joint j) {
    return Vector3Transform({ 0, 0, 0 }, f[(size_t)j]);
}

// ------------------------------------------------------------------ clips
namespace clips {
namespace {
Pose restPose() {
    Pose p;
    p[Joint::ShoulderL].z = p[Joint::ShoulderR].z = 5;
    p[Joint::ElbowL].x = p[Joint::ElbowR].x = -6;
    return p;
}
// The swing of one walking/running leg and its opposite arm at phase `ph` (radians).
void gait(Pose& p, float ph, float hipAmp, float kneeAmp, float armAmp, float elbow) {
    for (int side = 0; side < 2; side++) {
        const float s = std::sin(ph + side * PI);              // > 0: this leg swings forward
        const Joint hip = side ? Joint::HipR : Joint::HipL, knee = side ? Joint::KneeR : Joint::KneeL;
        const Joint ankle = side ? Joint::AnkleR : Joint::AnkleL;
        const Joint shoulder = side ? Joint::ShoulderL : Joint::ShoulderR;   // the opposite arm
        const Joint elb = side ? Joint::ElbowL : Joint::ElbowR;
        p[hip].x = -hipAmp * s;
        p[knee].x = 6 + kneeAmp * std::max(0.f, std::sin(ph + side * PI + 0.9f));
        p[ankle].x = -0.45f * (p[hip].x + p[knee].x);
        p[shoulder].x = -armAmp * s;
        p[shoulder].z = 6;
        p[elb].x = -elbow - 0.4f * elbow * std::max(0.f, s);
    }
}
} // namespace

Clip rest() { return [](double) { return restPose(); }; }
Clip hold(Pose p) { return [p](double) { return p; }; }

Clip idle(float e) {
    return [e](double s) {
        Pose p = restPose();
        const float br = std::sin(TAU * (float)s / 3.4f), shift = std::sin(TAU * (float)s / 6.1f);
        p[Joint::Chest].x = 1.5f * br * e;
        p[Joint::Head].x = -1.0f * br * e;
        p[Joint::Head].y = 5 * std::sin(TAU * (float)s / 7.3f) * e;
        p[Joint::Neck].z = 2 * shift * e;
        p[Joint::ShoulderL].z = p[Joint::ShoulderR].z = 5 + 1.5f * br * e;
        p[Joint::ElbowL].x = -8 - 2 * br * e;
        p[Joint::ElbowR].x = -8 + 2 * br * e;
        p[Joint::Pelvis].z = 2.5f * shift * e;
        p[Joint::Chest].z = -2.5f * shift * e;
        p[Joint::HipL].z = -2.5f * shift * e;
        p[Joint::HipR].z = 2.5f * shift * e;
        p.offset = { 0.012f * shift * e, 0.004f * br * e, 0 };
        return p;
    };
}

Clip walk(float cadence) {
    return [cadence](double s) {
        Pose p = restPose();
        const float ph = TAU * cadence * (float)s;
        gait(p, ph, 24, 38, 22, 14);
        p[Joint::Chest].x = 4;
        p[Joint::Chest].y = 7 * std::sin(ph);
        p[Joint::Pelvis].y = -6 * std::sin(ph);
        p[Joint::Head].y = -4 * std::sin(ph);
        p.offset.y = 0.018f * std::fabs(std::cos(ph)) - 0.012f;
        return p;
    };
}

Clip run(float cadence) {
    return [cadence](double s) {
        Pose p = restPose();
        const float ph = TAU * cadence * (float)s;
        gait(p, ph, 40, 85, 38, 75);
        p[Joint::Chest].x = 12;
        p[Joint::Head].x = -8;
        p[Joint::Chest].y = 10 * std::sin(ph);
        p[Joint::Pelvis].y = -8 * std::sin(ph);
        p.offset.y = 0.05f * std::fabs(std::cos(ph)) - 0.03f;
        return p;
    };
}

Clip wave(Side side) {
    return [side](double s) {
        Pose r = restPose(), p = r;
        const Joint sh = side == Side::Left ? Joint::ShoulderL : Joint::ShoulderR;
        const Joint el = side == Side::Left ? Joint::ElbowL : Joint::ElbowR;
        p[sh] = { -12, 0, 115 };
        p[el] = { -10, 0, 40 + 28 * std::sin(TAU * 2.1f * (float)s) };
        p[Joint::Head].z = side == Side::Left ? 4.f : -4.f;
        p[Joint::Chest].z = side == Side::Left ? -2.f : 2.f;
        return Pose::mix(r, p, smooth01(s / 0.35));
    };
}

Clip point(Side side) {
    return [side](double s) {
        Pose r = restPose(), p = r;
        const Joint sh = side == Side::Left ? Joint::ShoulderL : Joint::ShoulderR;
        const Joint el = side == Side::Left ? Joint::ElbowL : Joint::ElbowR;
        p[sh] = { -84, 0, 10 };
        p[el] = { -4, 0, 0 };
        p[Joint::Chest].y = side == Side::Left ? 10.f : -10.f;
        p[Joint::Head].y = side == Side::Left ? 8.f : -8.f;
        return Pose::mix(r, p, smooth01(s / 0.4));
    };
}

Clip cheer() {
    return [](double s) {
        Pose r = restPose(), p = r;
        const float b = std::fabs(std::sin(TAU * 0.8f * (float)s));
        p[Joint::ShoulderL] = p[Joint::ShoulderR] = { -10, 0, 155 + 8 * b };
        p[Joint::ElbowL] = p[Joint::ElbowR] = { -8, 0, 15 };
        p[Joint::HipL].x = p[Joint::HipR].x = -12 * (1 - b);
        p[Joint::KneeL].x = p[Joint::KneeR].x = 24 * (1 - b);
        p[Joint::AnkleL].x = p[Joint::AnkleR].x = -12 * (1 - b);
        p[Joint::Head].x = -10;
        p.offset.y = 0.06f * b - 0.02f;
        return Pose::mix(r, p, smooth01(s / 0.3));
    };
}

Clip jump(float period) {
    Pose stand = restPose(), crouch = stand, air = stand, land = stand;
    crouch[Joint::HipL].x = crouch[Joint::HipR].x = -45;
    crouch[Joint::KneeL].x = crouch[Joint::KneeR].x = 80;
    crouch[Joint::AnkleL].x = crouch[Joint::AnkleR].x = -35;
    crouch[Joint::Chest].x = 18;
    crouch[Joint::ShoulderL].x = crouch[Joint::ShoulderR].x = 35;
    crouch.offset.y = -0.12f;
    air[Joint::ShoulderL] = air[Joint::ShoulderR] = { -20, 0, 150 };
    air[Joint::HipL].x = air[Joint::HipR].x = -10;
    air[Joint::KneeL].x = air[Joint::KneeR].x = 20;
    air.offset.y = 0.45f;
    land = crouch;
    land.offset.y = -0.08f;
    return keys({ { 0, stand }, { 0.28 * period, crouch }, { 0.48 * period, air }, { 0.68 * period, land },
                  { period, stand } }, true);
}

Clip sit(float drop) {
    return [drop](double s) {
        Pose r = restPose(), p = r;
        p[Joint::HipL].x = p[Joint::HipR].x = -90;
        p[Joint::KneeL].x = p[Joint::KneeR].x = 90;
        p[Joint::ShoulderL].x = p[Joint::ShoulderR].x = -30;
        p[Joint::ElbowL].x = p[Joint::ElbowR].x = -40;
        p[Joint::Chest].x = 4;
        p.offset.y = -drop;
        return Pose::mix(r, p, smooth01(s / 0.5));
    };
}

Clip shrug() {
    return [](double s) {
        Pose r = restPose(), p = r;
        const float k = smooth01(s / 0.3) * (1 - smooth01((s - 0.9) / 0.4));
        p[Joint::ShoulderL] = p[Joint::ShoulderR] = { -10, 0, 28 };
        p[Joint::ElbowL] = p[Joint::ElbowR] = { -75, 0, 25 };
        p[Joint::Head].z = 10;
        p[Joint::Neck].x = -6;
        p.offset.y = 0.02f;
        return Pose::mix(r, p, k);
    };
}

Clip keys(std::vector<std::pair<double, Pose>> ks, bool loop) {
    std::sort(ks.begin(), ks.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    return [ks, loop](double s) {
        if (ks.empty()) return restPose();
        if (loop && ks.back().first > 0) s = std::fmod(std::fmod(s, ks.back().first) + ks.back().first, ks.back().first);
        if (s <= ks.front().first) return ks.front().second;
        for (size_t i = 1; i < ks.size(); i++)
            if (s <= ks[i].first) {
                const double span = ks[i].first - ks[i - 1].first;
                return Pose::mix(ks[i - 1].second, ks[i].second, span > 0 ? smooth01((s - ks[i - 1].first) / span) : 1.f);
            }
        return ks.back().second;
    };
}

Clip named(const std::string& n) {
    if (n == "rest") return rest();
    if (n == "idle") return idle();
    if (n == "walk") return walk();
    if (n == "run") return run();
    if (n == "wave" || n == "wave-right") return wave(Side::Right);
    if (n == "wave-left") return wave(Side::Left);
    if (n == "point" || n == "point-right") return point(Side::Right);
    if (n == "point-left") return point(Side::Left);
    if (n == "cheer") return cheer();
    if (n == "jump") return jump();
    if (n == "sit") return sit(0.3f);
    if (n == "shrug") return shrug();
    throw std::runtime_error("rig: unknown clip \"" + n + "\"");
}
} // namespace clips

float walkSpeed(const Build& b, float cadence) {
    const float leg = b.thigh + b.shin + b.ankle;
    return 2 * (2 * leg * std::sin(24 * DEG2RAD)) * cadence;
}
float runSpeed(const Build& b, float cadence) {
    const float leg = b.thigh + b.shin + b.ankle;
    return 2 * (2 * leg * std::sin(40 * DEG2RAD)) * 1.25f * cadence;
}

bool masks(Mask m, Joint j) {
    const bool armL = j == Joint::ShoulderL || j == Joint::ElbowL || j == Joint::WristL;
    const bool armR = j == Joint::ShoulderR || j == Joint::ElbowR || j == Joint::WristR;
    const bool head = j == Joint::Neck || j == Joint::Head;
    switch (m) {
    case Mask::All: return true;
    case Mask::Upper: return armL || armR || head || j == Joint::Chest;
    case Mask::Lower: return j == Joint::Root || j == Joint::Pelvis || j == Joint::HipL || j == Joint::KneeL ||
                             j == Joint::AnkleL || j == Joint::HipR || j == Joint::KneeR || j == Joint::AnkleR;
    case Mask::ArmL: return armL;
    case Mask::ArmR: return armR;
    case Mask::Arms: return armL || armR;
    case Mask::Head: return head;
    }
    return false;
}

// ------------------------------------------------------------------ motion
Motion& Motion::play(Clip clip, double from, double fade) {
    Entry e{ std::move(clip), from, 1e30, std::max(0.0, fade), Mask::All };
    auto at = std::upper_bound(base_.begin(), base_.end(), from, [](double f, const Entry& x) { return f < x.from; });
    base_.insert(at, std::move(e));
    return *this;
}

Motion& Motion::layer(Clip clip, double from, double to, Mask mask, double fade) {
    layers_.push_back({ std::move(clip), from, to, std::max(0.0, fade), mask });
    return *this;
}

Pose Motion::at(double s) const {
    Pose p;
    if (base_.empty()) p = clips::rest()(s);
    else {
        size_t i = 0;
        while (i + 1 < base_.size() && base_[i + 1].from <= s) i++;
        const Entry& e = base_[i];
        p = e.clip(std::max(0.0, s - e.from));
        if (i > 0 && e.fade > 0 && s - e.from < e.fade) {
            const Entry& prev = base_[i - 1];
            p = Pose::mix(prev.clip(std::max(0.0, s - prev.from)), p, smooth01((s - e.from) / e.fade));
        }
    }
    for (const Entry& l : layers_) {
        if (s < l.from || s > l.to + l.fade) continue;
        const float w = smooth01(l.fade > 0 ? (s - l.from) / l.fade : 1.0) *
                        smooth01(l.fade > 0 ? (l.to + l.fade - s) / l.fade : (s <= l.to ? 1.0 : 0.0));
        if (w <= 0) continue;
        const Pose q = l.clip(s - l.from);
        for (int j = 0; j < kJoints; j++)
            if (masks(l.mask, (Joint)j)) p.rot[(size_t)j] = lerp3(p.rot[(size_t)j], q.rot[(size_t)j], w);
        if (l.mask == Mask::All) p.offset = lerp3(p.offset, q.offset, w);
    }
    return p;
}

// ------------------------------------------------------------------ face timing
FaceTrack& FaceTrack::expression(double at, const std::string& n, double fade) {
    expressions_.push_back({ at, n, fade });
    std::stable_sort(expressions_.begin(), expressions_.end(), [](const Key& a, const Key& b) { return a.at < b.at; });
    return *this;
}
FaceTrack& FaceTrack::look(double at, Vector2 gaze, double fade) {
    looks_.push_back({ at, gaze, fade });
    std::stable_sort(looks_.begin(), looks_.end(), [](const LookKey& a, const LookKey& b) { return a.at < b.at; });
    return *this;
}
FaceTrack& FaceTrack::blink(double at) {
    blinks_.push_back(at);
    std::sort(blinks_.begin(), blinks_.end());
    return *this;
}
FaceTrack& FaceTrack::mouths(std::vector<std::pair<double, char>> cues) {
    mouths_.insert(mouths_.end(), cues.begin(), cues.end());
    std::stable_sort(mouths_.begin(), mouths_.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    return *this;
}
FaceTrack& FaceTrack::say(double from, double to, const std::string& text) { return mouths(visemes(text, from, to)); }

FaceTrack& FaceTrack::rhubarb(const std::string& file, double offset) {
    std::ifstream in(media::resolve(file));
    if (!in) throw std::runtime_error("rig: cannot read lip-sync cues: " + file);
    std::vector<std::pair<double, char>> cues;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        double t;
        std::string shape;
        if (ls >> t >> shape && !shape.empty()) cues.push_back({ t + offset, (char)std::toupper((unsigned char)shape[0]) });
    }
    return mouths(std::move(cues));
}

namespace {
// How shut the eyes are `d` seconds after a blink starts.
float blinkCurve(double d) {
    if (d < 0 || d > 0.17) return 0;
    if (d < 0.06) return (float)(d / 0.06);
    if (d < 0.09) return 1;
    return (float)(1 - (d - 0.09) / 0.08);
}
} // namespace

FaceState FaceTrack::at(double s) const {
    FaceState f;
    if (!expressions_.empty()) {
        size_t i = 0;
        bool found = false;
        for (size_t k = 0; k < expressions_.size(); k++) if (expressions_[k].at <= s) { i = k; found = true; }
        if (found) {
            const Key& k = expressions_[i];
            f.expression = k.name;
            f.previous = i > 0 ? expressions_[i - 1].name : "neutral";
            f.mix = k.fade > 0 ? smooth01((s - k.at) / k.fade) : 1.f;
        }
    }
    {
        Vector2 prev{ 0, 0 };
        for (const LookKey& k : looks_) {
            if (k.at > s) break;
            const float u = k.fade > 0 ? smooth01((s - k.at) / k.fade) : 1.f;
            prev = { lerpf(prev.x, k.gaze.x, u), lerpf(prev.y, k.gaze.y, u) };
        }
        f.look = prev;
    }
    for (size_t k = 0; k < mouths_.size(); k++) {
        if (mouths_[k].first > s) break;
        f.previousMouth = k > 0 ? mouths_[k - 1].second : 'X';
        f.mouth = mouths_[k].second;
        f.mouthMix = smooth01((s - mouths_[k].first) / 0.07);
    }
    float b = 0;
    for (double t : blinks_) b = std::max(b, blinkCurve(s - t));
    if (autoBlink) {
        double t = rrange(seed, 0, 0.8, 2.5);
        for (unsigned long long k = 1; t <= s + 0.01 && k < 100000; k++) {
            b = std::max(b, blinkCurve(s - t));
            t += rrange(seed, k, 2.0, 5.0);
        }
    }
    f.blink = std::clamp(b, 0.f, 1.f);
    return f;
}

std::vector<std::pair<double, char>> visemes(const std::string& text, double from, double to) {
    std::vector<std::pair<char, float>> seq;   // shape, relative duration
    auto push = [&](char c, float w) {
        if (!seq.empty() && seq.back().first == c) seq.back().second += w;
        else seq.push_back({ c, w });
    };
    for (size_t i = 0; i < text.size(); i++) {
        const char ch = (char)std::tolower((unsigned char)text[i]);
        const char nx = i + 1 < text.size() ? (char)std::tolower((unsigned char)text[i + 1]) : 0;
        if (ch == 't' && nx == 'h') { push('B', 0.7f); i++; continue; }
        if ((ch == 'o' && nx == 'o') || (ch == 'o' && nx == 'u')) { push('F', 1.1f); i++; continue; }
        if (ch == 'e' && nx == 'e') { push('B', 1.0f); i++; continue; }
        switch (ch) {
        case 'a': push('D', 1.0f); break;
        case 'e': push('C', 0.9f); break;
        case 'i': case 'y': push('B', 0.8f); break;
        case 'o': push('E', 1.0f); break;
        case 'u': case 'w': case 'q': push('F', 0.9f); break;
        case 'm': case 'b': case 'p': push('A', 0.7f); break;
        case 'f': case 'v': push('G', 0.7f); break;
        case 'l': push('H', 0.6f); break;
        case 'r': push('E', 0.5f); break;
        default:
            if (std::isalpha((unsigned char)ch)) push('B', 0.55f);
            else if (ch == ',' || ch == '.' || ch == '!' || ch == '?' || ch == ';') push('X', 1.2f);
            else if (ch == ' ') push('X', 0.25f);
        }
    }
    std::vector<std::pair<double, char>> out;
    float total = 0;
    for (auto& [c, w] : seq) total += w;
    if (total <= 0 || to <= from) return { { from, 'X' } };
    double t = from;
    for (auto& [c, w] : seq) {
        out.push_back({ t, c });
        t += (to - from) * w / total;
    }
    out.push_back({ to, 'X' });
    return out;
}

// ------------------------------------------------------------------ outfits
Rectangle region(Region r) {
    switch (r) {
    case Region::Torso: return { 0, 0, 1, 0.3125f };
    case Region::Hips: return { 0, 0.3125f, 1, 0.125f };
    case Region::ArmL: return { 0, 0.4375f, 0.25f, 0.5625f };
    case Region::ArmR: return { 0.25f, 0.4375f, 0.25f, 0.5625f };
    case Region::LegL: return { 0.5f, 0.4375f, 0.25f, 0.5625f };
    case Region::LegR: return { 0.75f, 0.4375f, 0.25f, 0.5625f };
    default: return { 0, 0, 1, 1 };
    }
}
Rectangle front(Region r) {
    const Rectangle g = region(r);
    return { g.x + g.width * 0.375f, g.y, g.width * 0.25f, g.height };
}

Outfit Outfit::basic(Color top, Color bottom, Color skin, float sleeve, float trouser) {
    Outfit o;
    o.id = nextOutfitId();
    o.fallback = top;
    auto colorAt = [=](Region r, float v) {
        switch (r) {
        case Region::Torso: return top;
        case Region::Hips: return bottom;
        case Region::ArmL: case Region::ArmR: return v <= sleeve ? top : skin;
        default: return v <= trouser ? bottom : skin;
        }
    };
    o.sample = [colorAt](Region r, float, float v) { return colorAt(r, v); };
    o.paint = [=](const Ctx& c, int, float size) {
        auto px = [&](Rectangle u) { return Rectangle{ u.x * size, u.y * size, u.width * size, u.height * size }; };
        const Color hem = lerpc(top, BLACK, 0.25f), cuff = lerpc(bottom, BLACK, 0.25f);
        vg::fill(c, vg::Path::rect(px(region(Region::Torso))), top);
        vg::fill(c, vg::Path::rect(px(region(Region::Hips))), bottom);
        for (Region r : { Region::ArmL, Region::ArmR, Region::LegL, Region::LegR }) {
            const bool arm = r == Region::ArmL || r == Region::ArmR;
            const float cover = std::clamp(arm ? sleeve : trouser, 0.f, 1.f);
            const Rectangle g = px(region(r));
            vg::fill(c, vg::Path::rect(g), skin);
            if (cover > 0) {
                vg::fill(c, vg::Path::rect({ g.x, g.y, g.width, g.height * cover }), arm ? top : bottom);
                if (cover < 1)
                    vg::fill(c, vg::Path::rect({ g.x, g.y + g.height * cover - size * 0.008f, g.width, size * 0.008f }),
                             arm ? hem : cuff);
            }
        }
        // A collar band and a waistband: just enough for the eye to read a garment. (No skin
        // dip at the collar: the neck is its own part, and skin painted on the chest reads as
        // a second neck in front of it.)
        const Rectangle tr = px(region(Region::Torso));
        vg::fill(c, vg::Path::rect({ tr.x, tr.y, tr.width, size * 0.012f }), hem);
        const Rectangle hp = px(region(Region::Hips));
        vg::fill(c, vg::Path::rect({ hp.x, hp.y, hp.width, hp.height * 0.18f }), cuff);
    };
    return o;
}

namespace {
struct SvgOutfit {
    vg::Drawing drawing;
    std::vector<std::vector<vg::Polyline>> outlines;   // per shape, for point tests
};
bool inside(const std::vector<vg::Polyline>& polys, Vector2 p, vg::FillRule rule) {
    int winding = 0;
    for (const vg::Polyline& pl : polys) {
        const size_t n = pl.pts.size();
        for (size_t i = 0; i < n; i++) {
            const Vector2 a = pl.pts[i], b = pl.pts[(i + 1) % n];
            if (a.y <= p.y) {
                if (b.y > p.y && (b.x - a.x) * (p.y - a.y) - (p.x - a.x) * (b.y - a.y) > 0) winding++;
            } else if (b.y <= p.y && (b.x - a.x) * (p.y - a.y) - (p.x - a.x) * (b.y - a.y) < 0) winding--;
        }
    }
    return rule == vg::FillRule::EvenOdd ? (winding % 2) != 0 : winding != 0;
}
} // namespace

Outfit Outfit::svg(const std::string& file, const Outfit& under) {
    auto data = std::make_shared<SvgOutfit>();
    data->drawing = vg::Drawing::load(file);
    for (const vg::Shape& s : data->drawing.shapes) data->outlines.push_back(s.path.flatten(1.0f));
    Outfit o;
    o.id = nextOutfitId();
    o.fallback = under.fallback;
    auto base = under.paint;
    o.paint = [data, base](const Ctx& c, int t, float size) {
        if (base) base(c, t, size);
        vg::draw(c, data->drawing, { 0, 0, size, size });
    };
    auto baseSample = under.sample;
    const Color fb = under.fallback;
    o.sample = [data, baseSample, fb](Region r, float u, float v) {
        const Rectangle g = region(r);
        const float side = std::max(data->drawing.width, data->drawing.height);
        const float ox = (side - data->drawing.width) / 2, oy = (side - data->drawing.height) / 2;
        const Vector2 p{ (g.x + u * g.width) * side - ox, (g.y + v * g.height) * side - oy };
        for (size_t i = data->drawing.shapes.size(); i-- > 0;) {
            const vg::Style& st = data->drawing.shapes[i].style;
            if (st.fill.a > 0 && inside(data->outlines[i], p, st.rule)) return st.fill;
        }
        return baseSample ? baseSample(r, u, v) : fb;
    };
    return o;
}

Outfit& Outfit::over(std::function<void(const Ctx& c, int t, float size)> layer) {
    auto base = paint;
    paint = [base, layer](const Ctx& c, int t, float size) {
        if (base) base(c, t, size);
        layer(c, t, size);
    };
    id = nextOutfitId();
    return *this;
}

// ------------------------------------------------------------------ looks
const char* name(Slot s) {
    switch (s) {
    case Slot::Head: return "head";
    case Slot::Hair: return "hair";
    case Slot::Hat: return "hat";
    case Slot::Torso: return "torso";
    case Slot::Hips: return "hips";
    case Slot::Arm: return "arm";
    case Slot::Hand: return "hand";
    case Slot::Leg: return "leg";
    case Slot::Foot: return "foot";
    case Slot::Extra: return "extra";
    }
    return "?";
}

Look Look::toon() { return Look{}; }

Look Look::block() {
    Look l;
    l.build = Build::block();
    l.head = "head.block"; l.hair = "hair.block"; l.torso = "torso.block"; l.hips = "hips.block";
    l.arms("arm.block").hands("hand.clip").legs("leg.block").feet("");
    l.skin = { 246, 205, 46, 255 };
    l.hairColor = { 92, 56, 30, 255 };
    l.outfit = Outfit::basic({ 200, 40, 40, 255 }, { 30, 70, 150, 255 }, l.skin, 1.0f, 1.0f);
    FaceStyle fs;
    fs.eyes = FaceStyle::Eyes::Dots;
    fs.ink = { 24, 22, 20, 255 };
    l.face = simpleFace(fs);
    l.faceBox = { 0.2f, 0.28f, 0.6f, 0.5f };
    l.finish = Finish::Plastic;
    return l;
}

Look Look::avatar() {
    Look l;
    l.build = Build::toon();
    l.build.head = 0.34f;
    l.build.torso = { 0.40f, 0.40f, 0.20f };
    l.build.armRadius = 0.075f;
    l.build.legRadius = 0.085f;
    l.build.hipWidth = 0.20f;
    l.build.thigh = 0.28f; l.build.shin = 0.26f;
    l.head = "head.box"; l.hair = "hair.cap"; l.torso = "torso.box"; l.hips = "hips.box";
    l.arms("arm.box").hands("hand.box").legs("leg.box").feet("foot.block");
    l.skin = { 250, 214, 160, 255 };
    l.outfit = Outfit::basic({ 60, 160, 90, 255 }, { 40, 40, 60, 255 }, l.skin, 0.4f, 1.0f);
    FaceStyle fs;
    fs.eyes = FaceStyle::Eyes::Dots;
    fs.eyeSize = 1.35f;
    fs.lineWeight = 1.4f;
    l.face = simpleFace(fs);
    l.finish = Finish::Plastic;
    return l;
}

Look Look::robot() {
    Look l;
    l.build = Build::toon();
    l.build.head = 0.34f;
    l.head = "head.box"; l.hair = ""; l.hat = "hat.antenna"; l.torso = "torso.box"; l.hips = "hips.box";
    l.arms("arm.robot").hands("hand.claw").legs("leg.robot").feet("foot.boot");
    l.skin = { 176, 186, 198, 255 };
    l.shoes = { 60, 64, 74, 255 };
    l.outfit = Outfit::basic({ 150, 162, 176, 255 }, { 80, 86, 98, 255 }, l.skin, 1, 1);
    FaceStyle fs;
    fs.eyes = FaceStyle::Eyes::Dots;
    fs.ink = { 40, 220, 240, 255 };
    fs.mouth = { 20, 40, 50, 255 };
    fs.brows = false;
    l.face = simpleFace(fs);
    l.finish = Finish::Plastic;
    return l;
}

std::vector<std::pair<std::string, std::string>> slots(const Look& l) {
    return { { "head", l.head }, { "hair", l.hair }, { "hat", l.hat }, { "torso", l.torso }, { "hips", l.hips },
             { "armL", l.armL }, { "armR", l.armR }, { "handL", l.handL }, { "handR", l.handR },
             { "legL", l.legL }, { "legR", l.legR }, { "footL", l.footL }, { "footR", l.footR },
             { "extra", l.extra } };
}

// ------------------------------------------------------------------ characters
namespace {
// Everything that makes a slot look as it does: its shapes with their colours resolved,
// plus the outfit and face they show. Equal signatures need no swap.
std::string signature(const Look& l, const std::string& slot) {
    std::ostringstream o;
    o.precision(5);
    const auto b = l.build;
    o << b.head << ',' << b.neck << ',' << b.torso.x << ',' << b.torso.y << ',' << b.torso.z << ',' << b.hips << ','
      << b.hipWidth << ',' << b.upperArm << ',' << b.forearm << ',' << b.armRadius << ',' << b.thigh << ','
      << b.shin << ',' << b.legRadius << ',' << b.foot << ',' << b.ankle << ';' << (int)l.finish << ';';
    for (const Prim& p : prims(l, slot)) {
        o << (int)p.kind << p.name << (int)p.joint << ':' << p.at.x << ',' << p.at.y << ',' << p.at.z << ','
          << p.size.x << ',' << p.size.y << ',' << p.size.z << ',' << p.squareV << ',' << p.squareH << ','
          << p.softness << ',' << (int)p.paint << ',' << p.model << ';';
        Color c{};
        switch (p.paint) {
        case Paint::Skin: c = l.skin; break;
        case Paint::Hair: c = l.hairColor; break;
        case Paint::Shoes: c = l.shoes; break;
        case Paint::Accent: c = l.accent; break;
        case Paint::Metal: c = l.metal; break;
        case Paint::Dark: c = l.dark; break;
        case Paint::Fixed: c = p.color; break;
        case Paint::Outfit: o << "outfit" << l.outfit.id; break;
        case Paint::Face: c = l.skin; o << "face" << (const void*)l.face.get() << l.faceBox.x << l.faceBox.y; break;
        }
        o << (int)c.r << '.' << (int)c.g << '.' << (int)c.b << '.' << (int)c.a << ';';
    }
    return o.str();
}
float backOut(float u) {
    if (u >= 1) return 1;
    if (u <= 0) return 0;
    const float c1 = 1.70158f, c3 = c1 + 1, x = u - 1;
    return 1 + c3 * x * x * x + c1 * x * x;
}
} // namespace

namespace {
std::map<std::string, std::string> signatures(const Look& l) {
    std::map<std::string, std::string> out;
    for (const auto& [slot, part] : slots(l)) out[slot] = signature(l, slot);
    return out;
}
} // namespace

Character::Character(Look look) {
    sigs_.push_back(signatures(look));
    looks_.push_back(std::move(look));
    times_.push_back(-1e30);
    pops_.push_back(0);
}

Character& Character::wear(double at, Look look, double pop) {
    size_t i = 1;
    while (i < times_.size() && times_[i] <= at) i++;
    sigs_.insert(sigs_.begin() + (long)i, signatures(look));
    looks_.insert(looks_.begin() + (long)i, std::move(look));
    times_.insert(times_.begin() + (long)i, at);
    pops_.insert(pops_.begin() + (long)i, std::max(0.0, pop));
    return *this;
}

const Look& Character::lookAt(double s) const {
    size_t i = 0;
    while (i + 1 < times_.size() && times_[i + 1] <= s) i++;
    return looks_[i];
}

int Character::lookIndex(const std::string& slot, double s) const {
    size_t i = 0;
    while (i + 1 < times_.size() && times_[i + 1] <= s) i++;
    while (i > 0 && sigs_[i].at(slot) == sigs_[i - 1].at(slot)) i--;
    return (int)i;
}

float Character::popAt(const std::string& slot, double s) const {
    const int k = lookIndex(slot, s);
    if (k == 0 || pops_[(size_t)k] <= 0) return 1;
    return backOut((float)((s - times_[(size_t)k]) / pops_[(size_t)k]));
}

Pose Character::poseAt(double s) const {
    Pose p = motion.at(s);
    // Each limb's locks come from the look whose part is showing in that slot.
    for (const char* slot : { "armL", "armR", "handL", "handR", "legL", "legR", "footL", "footR", "torso", "hips",
                              "head", "hair", "hat", "extra" }) {
        const Look& l = looks_[(size_t)lookIndex(slot, s)];
        for (const auto& [key, part] : slots(l)) {
            if (key != slot || part.empty()) continue;
            const PartDef& def = parts::get(part);
            const bool right = key.back() == 'R';
            for (const auto& [j, r] : def.locks) {
                Joint jj = j;
                if (right) {
                    switch (j) {
                    case Joint::ShoulderL: jj = Joint::ShoulderR; break;
                    case Joint::ElbowL: jj = Joint::ElbowR; break;
                    case Joint::WristL: jj = Joint::WristR; break;
                    case Joint::HipL: jj = Joint::HipR; break;
                    case Joint::KneeL: jj = Joint::KneeR; break;
                    case Joint::AnkleL: jj = Joint::AnkleR; break;
                    default: break;
                    }
                }
                p[jj] = r;
            }
        }
    }
    return p;
}

namespace detail {
namespace {
bool same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

// The most common colour on a grid of us x vs samples over v0..v1.
Color dominant(const Outfit& o, Region r, float v0, float v1, int us, int vs) {
    std::vector<std::pair<Color, int>> votes;
    for (int i = 0; i < us; i++)
        for (int j = 0; j < vs; j++) {
            const Color c = o.sample(r, (i + 0.5f) / us, v0 + (j + 0.5f) / vs * (v1 - v0));
            auto it = std::find_if(votes.begin(), votes.end(), [&](const std::pair<Color, int>& v) { return same(v.first, c); });
            if (it == votes.end()) votes.push_back({ c, 1 });
            else it->second++;
        }
    Color best = o.fallback;
    int most = 0;
    for (const auto& [c, n] : votes) if (n > most) { most = n; best = c; }
    return best;
}

std::mutex outfitMu;
std::map<std::tuple<unsigned, int, float, float, int>, std::vector<Color>> outfitCache;

// Cached by outfit id; hand-made outfits (id 0) are recomputed.
template <class F>
std::vector<Color> cachedRows(const Outfit& o, Region r, float v0, float v1, int n, F make) {
    const auto key = std::make_tuple(o.id, (int)r, v0, v1, n);
    if (o.id) {
        std::lock_guard<std::mutex> lock(outfitMu);
        auto it = outfitCache.find(key);
        if (it != outfitCache.end()) return it->second;
    }
    std::vector<Color> rows = make();
    if (o.id) {
        std::lock_guard<std::mutex> lock(outfitMu);
        outfitCache[key] = rows;
    }
    return rows;
}
} // namespace

Color outfitColor(const Outfit& o, Region r, float v0, float v1) {
    if (!o.sample) return o.fallback;
    // n = 0 keys the whole-strip colour apart from any row split.
    return cachedRows(o, r, v0, v1, 0, [&] { return std::vector<Color>{ dominant(o, r, v0, v1, 16, 7) }; })[0];
}

std::vector<Color> outfitRows(const Outfit& o, Region r, float v0, float v1, int n) {
    if (!o.sample || n <= 0) return std::vector<Color>((size_t)std::max(n, 0), o.fallback);
    return cachedRows(o, r, v0, v1, n, [&] {
        std::vector<Color> rows;
        for (int k = 0; k < n; k++)
            rows.push_back(dominant(o, r, v0 + (v1 - v0) * k / n, v0 + (v1 - v0) * (k + 1) / n, 16, 1));
        return rows;
    });
}
} // namespace detail

} // namespace rig
} // namespace rmtn
