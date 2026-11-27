// lightning.cpp -- the shape (midpoint displacement with branches) and the timing (leader, return stroke,
// restrikes) of a lightning strike. See rmtn/lightning.hpp. Everything is a function of the seed.
#include "rmtn/lightning.hpp"
#include "rmtn/scene3d.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace rmtn {
namespace bolt {

namespace {
struct Gen {
    uint64_t seed, n = 0;
    bool planar = false;
    explicit Gen(uint32_t s) : seed(0x9E3779B97F4A7C15ull * (s + 1ull)) {}
    float r() { return (float)rnd01(seed, n++); }          // 0..1
    float sr() { return r() * 2 - 1; }                      // -1..1
};

Vector3 unitPerpendicular(Gen& g, Vector3 d) {
    Vector3 v{ g.sr(), g.sr(), g.sr() };
    if (g.planar) {                                         // in the plane: the one perpendicular, on a random side
        v = { -d.y, d.x, 0 };
        const float l = std::sqrt(v.x * v.x + v.y * v.y);
        return l > 1e-6f ? Vector3Scale(v, (g.sr() < 0 ? -1.f : 1.f) / l) : Vector3{ 1, 0, 0 };
    }
    v = Vector3Subtract(v, Vector3Scale(d, Vector3DotProduct(v, d)));
    const float l = Vector3Length(v);
    if (l < 1e-4f) {
        const Vector3 alt = std::fabs(d.y) < 0.9f ? Vector3{ 0, 1, 0 } : Vector3{ 1, 0, 0 };
        v = Vector3CrossProduct(d, alt);
        return Vector3Normalize(v);
    }
    return Vector3Scale(v, 1.f / l);
}

// Splits a..b `depth` times, pushing every vertex after a (up to and including b) into `out`.
void split(Gen& g, Vector3 a, Vector3 b, float jag, float rough, int depth, std::vector<Vector3>& out) {
    if (depth <= 0) { out.push_back(b); return; }
    const Vector3 d = Vector3Subtract(b, a);
    const float len = Vector3Length(d);
    Vector3 mid = Vector3Scale(Vector3Add(a, b), 0.5f);
    if (len > 1e-4f) {
        const Vector3 u = Vector3Scale(d, 1.f / len), p = unitPerpendicular(g, u);
        mid = Vector3Add(mid, Vector3Scale(p, jag * len * 0.5f * (0.35f + 0.65f * g.r())));
        mid = Vector3Add(mid, Vector3Scale(u, jag * len * 0.12f * g.sr()));      // along, too: unequal pieces
    } else { g.r(); g.r(); }
    split(g, a, mid, jag * rough, rough, depth - 1, out);
    split(g, mid, b, jag * rough, rough, depth - 1, out);
}

void channel(Gen& g, Bolt& bolt, const Shape& sh, Vector3 start, Vector3 end, int level, float reach0, float width0, float lengthRef);

// Branches off the vertices of one channel.
void grow(Gen& g, Bolt& bolt, const Shape& sh, const std::vector<Vector3>& v, const std::vector<float>& reach, int level, float lengthRef, Vector3 bias) {
    if (level + 1 >= sh.maxLevel) return;
    const float chance = sh.branchChance * std::pow(sh.branchFall, (float)level);
    const int n = (int)v.size();
    for (int i = 2; i < n - 2; i++) {
        const float roll = g.r(), pick = g.r(), side = g.sr(), turn = g.r(), len = g.r(), axis = g.sr();
        if (roll > chance) continue;
        const Vector3 d = Vector3Normalize(Vector3Subtract(v[(size_t)i + 1], v[(size_t)i - 1]));
        Vector3 p = unitPerpendicular(g, d);
        if (side < 0) p = Vector3Negate(p);
        const float ang = (sh.spread * (0.55f + 0.9f * turn)) * DEG2RAD;
        Vector3 dir = Vector3Add(Vector3Scale(d, std::cos(ang)), Vector3Scale(p, std::sin(ang)));
        dir = Vector3Normalize(Vector3Add(Vector3Scale(dir, 1.f - sh.downward), Vector3Scale(bias, sh.downward)));
        (void)pick; (void)axis;
        const float along = 1.f - (float)i / (float)n;                                 // branches near the start are longer than near the end
        const float bl = lengthRef * sh.branchLength * std::pow(0.55f, (float)level) * (0.35f + 0.65f * len) * (0.45f + 0.55f * along);
        const Vector3 tip = Vector3Add(v[(size_t)i], Vector3Scale(dir, bl));
        channel(g, bolt, sh, v[(size_t)i], tip, level + 1, reach[(size_t)i], 0.55f, lengthRef);
    }
}

void channel(Gen& g, Bolt& bolt, const Shape& sh, Vector3 start, Vector3 end, int level, float reach0, float width0, float lengthRef) {
    std::vector<Vector3> v{ start };
    const int depth = std::max(2, sh.depth - 2 * level);
    split(g, start, end, sh.jag * (level == 0 ? 1.f : 0.9f), sh.roughness, depth, v);
    std::vector<float> reach(v.size(), reach0);
    for (size_t i = 1; i < v.size(); i++) reach[i] = reach[i - 1] + Vector3Distance(v[i - 1], v[i]);
    const float total = std::max(reach.back() - reach0, 1e-3f);
    for (size_t i = 1; i < v.size(); i++) {
        Segment s;
        s.a = v[i - 1]; s.b = v[i];
        s.level = level;
        s.reachA = reach[i - 1]; s.reachB = reach[i];
        const float u = level == 0 ? 0.f : 0.5f * ((reach[i - 1] + reach[i]) * 0.5f - reach0) / total;         // along a branch the width falls
        s.width = level == 0 ? 1.f : width0 * (1.f - 0.75f * (u * 2.f));
        bolt.segments.push_back(s);
        bolt.reach = std::max(bolt.reach, s.reachB);
    }
    if (level == 0) bolt.length = reach.back();
    const Vector3 bias = Vector3Length(Vector3Subtract(end, start)) > 1e-4f ? Vector3Normalize(Vector3Subtract(end, start)) : Vector3{ 0, -1, 0 };
    grow(g, bolt, sh, v, reach, level, lengthRef, bias);
}
} // namespace

Bolt make(Vector3 start, Vector3 end, uint32_t seed, const Shape& shape) {
    Gen g(seed);
    g.planar = shape.planar;
    Bolt b;
    b.start = start; b.end = end;
    channel(g, b, shape, start, end, 0, 0.f, 1.f, std::max(Vector3Distance(start, end), 1.f));
    return b;
}

namespace {
struct Timing {
    float leader, hold, tail;                // seconds: the leader's growth, the return stroke's plateau, the whole strike
    int restrikes;
    float at[4], amp[4];                     // seconds after the start, and heights (0..1)
};
Timing timingOf(uint32_t seed, float leaderTime, float minVisible) {
    Gen g(seed ^ 0x5bd1e995u);
    Timing t;
    t.leader = std::max(leaderTime, 0.004f);
    t.hold = std::max(minVisible, 0.f);
    t.restrikes = 2 + (int)std::floor(g.r() * 2.999f);          // 2..4
    float when = t.leader + t.hold + 0.02f;
    const float last = 0.22f + 0.2f * g.r();                     // the last restrike lands 0.22 to 0.42 s in
    for (int i = 0; i < t.restrikes; i++) {
        const float share = (float)(i + 1) / (float)t.restrikes;
        const float lo = when, hi = t.leader + t.hold + (last - t.leader - t.hold) * share;
        t.at[i] = lo + (hi - lo) * (0.3f + 0.7f * g.r());
        t.amp[i] = 0.45f + 0.5f * g.r();
        when = t.at[i] + 0.025f;
    }
    t.tail = t.at[t.restrikes - 1] + 0.09f;
    return t;
}
} // namespace

float duration(uint32_t seed, float leaderTime, float minVisible) { return timingOf(seed, leaderTime, minVisible).tail; }

Profile profile(uint32_t seed, float tau, float leaderTime, float minVisible) {
    Profile p;
    if (tau < 0) return p;
    const Timing t = timingOf(seed, leaderTime, minVisible);
    if (tau >= t.tail) return p;
    p.alive = true;
    if (tau < t.leader) {                                   // the leader: the channel grows down, dimly
        const float u = tau / t.leader;
        p.visible = u * u * (3 - 2 * u) * 0.85f + 0.15f * u;
        p.flash = 0.12f + 0.3f * u;
        return p;
    }
    p.visible = 1;
    const float s = tau - t.leader;                         // since the return stroke
    float b;
    if (s < t.hold) b = 1.f;                                // held, so a 30 fps film cannot step over it
    else b = 0.3f + 0.7f * std::exp(-(s - t.hold) / 0.05f);
    const float glow = 0.3f * (1.f - (tau - t.leader) / (t.tail - t.leader));        // the channel keeps glowing between the strokes
    b = std::max(b * (s < t.hold ? 1.f : 1.f), glow);
    for (int i = 0; i < t.restrikes; i++) {                 // restrikes: a fast rise, a quick fall
        const float d = tau - t.at[i];
        if (d < -0.006f) continue;
        const float pulse = d < 0 ? t.amp[i] * (1.f + d / 0.006f) : t.amp[i] * std::exp(-d / 0.03f);
        b = std::max(b, pulse);
    }
    const float fade = std::clamp((t.tail - tau) / 0.06f, 0.f, 1.f);
    p.flash = b * fade;
    return p;
}

} // namespace bolt
} // namespace rmtn

// ---------------------------------------------------------------- Scene::lightning
namespace rmtn {
namespace g3d {

namespace {
constexpr double kSlot = 0.5;                       // seconds: the rate's strikes live in slots of this length
constexpr double kMaxDuration = 0.75;               // longer than any strike lasts (bolt::duration is at most about 0.55)

float effectiveRate(const Scene& s) {
    const Lightning& l = s.lightning;
    return l.rate >= 0 ? l.rate : (s.weather.enabled ? std::max(s.weather.lightningRate, 0.f) : 0.f);
}
float effectiveBase(const Scene& s) {
    const Lightning& l = s.lightning;
    return l.cloudBase >= 0 ? l.cloudBase : (s.clouds.enabled ? s.clouds.base : 1500.f);
}
Vector2 driftAt(const Scene& s, double seconds) {
    return s.lightning.followClouds && s.clouds.enabled ? s.clouds.travel(s.weather, seconds) : Vector2{ 0, 0 };
}

// The strike the slot holds, if it holds one: the time, place and bolt all come from hashes of (seed, slot).
bool strikeOfSlot(const Scene& s, long long k, float rate, Lightning::Strike& out) {
    const Lightning& l = s.lightning;
    const uint64_t key = 0xC2B2AE3D27D4EB4Full * ((uint64_t)l.seed + 7ull);
    const uint64_t base = (uint64_t)(k + (1ll << 40)) * 16ull;
    auto r = [&](int i) { return (float)rnd01(key, base + (uint64_t)i); };
    const float chance = (float)(rate * kSlot / 60.0);
    if (r(0) >= chance) return false;
    out.time = (float)(((double)k + r(1)) * kSlot);
    const float ang = r(2) * 2 * PI, rad = l.radius * std::sqrt(r(3));
    const float cb = effectiveBase(s);
    Vector3 start{ l.area.x + rad * std::cos(ang), cb + 150.f + 500.f * r(4), l.area.y + rad * std::sin(ang) };
    Vector3 end;
    if (r(5) < l.groundShare) end = { start.x + 500.f * (r(6) - 0.5f), l.groundY, start.z + 500.f * (r(7) - 0.5f) };
    else {
        const float a2 = r(6) * 2 * PI, len = 700.f + 1800.f * r(7);
        end = { start.x + len * std::cos(a2), start.y - 350.f * r(8), start.z + len * std::sin(a2) };
    }
    out.start = start; out.end = end;
    out.seed = (uint32_t)(r(9) * 4294967040.0f) | 1u;
    out.intensity = 0.75f + 0.5f * r(10);
    return true;
}

void place(const Scene& s, Lightning::Strike& st) {
    const Vector2 d = driftAt(s, st.time);
    st.start.x += d.x; st.start.z += d.y;
    st.end.x += d.x; st.end.z += d.y;
}
} // namespace

std::vector<Lightning::Strike> lightningStrikes(const Scene& s, double from, double to) {
    std::vector<Lightning::Strike> out;
    const Lightning& l = s.lightning;
    if (!l.enabled || to <= from) return out;
    for (const Lightning::Strike& e : l.strikes)
        if (e.time >= from && e.time < to) { Lightning::Strike st = e; place(s, st); out.push_back(st); }
    const float rate = effectiveRate(s);
    if (rate > 0)
        for (long long k = (long long)std::floor(from / kSlot) - 1; k <= (long long)std::floor(to / kSlot); k++) {
            Lightning::Strike st;
            if (!strikeOfSlot(s, k, rate, st) || st.time < from || st.time >= to) continue;
            place(s, st);
            out.push_back(st);
        }
    std::stable_sort(out.begin(), out.end(), [](const Lightning::Strike& a, const Lightning::Strike& b) { return a.time < b.time; });
    return out;
}

std::vector<LightningActive> lightningAt(const Scene& s, double seconds) {
    std::vector<LightningActive> out;
    const Lightning& l = s.lightning;
    if (!l.enabled) return out;
    for (const Lightning::Strike& st : lightningStrikes(s, seconds - kMaxDuration, seconds + 1e-6)) {
        LightningActive a;
        a.strike = st;
        a.tau = (float)(seconds - st.time);
        a.profile = bolt::profile(st.seed, a.tau, l.leaderTime, l.minVisible);
        a.toGround = st.end.y <= l.groundY + 50.f;
        if (a.profile.alive) out.push_back(a);
    }
    return out;
}

std::vector<Thunder> thunderCues(Scene& s, int frames, float fps) {
    std::vector<Thunder> out;
    fps = std::max(fps, 1.f);
    for (int f = 0; f < frames; f++) {
        if (s.up) s.up(f, s);
        const Camera cam = cameraAt(s.camera, f);
        for (const Lightning::Strike& st : lightningStrikes(s, f / (double)fps, (f + 1) / (double)fps)) {
            const Vector3 mid = Vector3Scale(Vector3Add(st.start, st.end), 0.5f), pts[3] = { st.start, st.end, mid };
            Thunder t;
            t.distance = 1e30f;
            for (const Vector3& p : pts) {
                const float d = Vector3Distance(p, cam.position);
                if (d < t.distance) { t.distance = d; t.position = p; }
            }
            t.strikeTime = st.time;
            t.arrives = st.time + bolt::thunderDelay(t.distance);
            t.toGround = st.end.y <= s.lightning.groundY + 50.f;
            t.seed = st.seed;
            out.push_back(t);
        }
    }
    std::stable_sort(out.begin(), out.end(), [](const Thunder& a, const Thunder& b) { return a.arrives < b.arrives; });
    return out;
}

// ---------------------------------------------------------------- the rainbow
float rainbowAngle(float nm, int order) {
    const float um = nm * 1e-3f, n = 1.3248f + 0.00316f / (um * um);          // Cauchy: water, 1.3446 at 400 nm, 1.3313 at 700
    const float k = order >= 2 ? 2.f : 1.f;
    const float cosI = std::sqrt((n * n - 1.f) / (k * (k + 2.f)));
    const float i = std::acos(std::clamp(cosI, 0.f, 1.f)), r = std::asin(std::sin(i) / n);
    return order >= 2 ? (180.f + 2.f * i * RAD2DEG - 6.f * r * RAD2DEG) : (4.f * r - 2.f * i) * RAD2DEG;
}

RainbowFrame rainbowOf(const Scene& s, double seconds) {
    (void)seconds;
    RainbowFrame f;
    const Weather& w = s.weather;
    if (!w.enabled || w.rainbow.mode == Weather::Rainbow::Mode::Off) return f;
    bool have = false;
    Vector3 sun{ 0, 1, 0 };
    if (s.sky.enabled) {
        const Sky& k = s.sky;
        Vector3 d = k.sunDirection;
        if (k.sunLight >= 0 && k.sunLight < (int)s.lights.size()) {
            const Light& li = s.lights[(size_t)k.sunLight];
            d = li.type == Light::Type::Directional ? Vector3Subtract(li.position, li.target) : li.position;
        }
        if (Vector3Length(d) > 1e-6f) { sun = Vector3Normalize(d); have = true; }
    } else {
        for (const Light& li : s.lights)
            if (li.type == Light::Type::Directional) {
                const Vector3 d = Vector3Subtract(li.position, li.target);
                if (Vector3Length(d) > 1e-6f) { sun = Vector3Normalize(d); have = true; }
                break;
            }
    }
    f.sun = sun;
    f.antiSolar = Vector3Negate(sun);
    f.elevation = std::asin(std::clamp(sun.y, -1.f, 1.f)) * RAD2DEG;
    if (!have) return f;
    const float rain = w.rainbow.mode == Weather::Rainbow::Mode::On ? 1.f : w.rainbowRain();
    f.strength = std::clamp(w.rainbow.strength, 0.f, 2.f) * rain * w.rainbowSun(f.elevation);
    return f;
}

} // namespace g3d
} // namespace rmtn
