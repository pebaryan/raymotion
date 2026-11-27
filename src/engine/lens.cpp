// lens.cpp -- camera weather (rmtn/lens.hpp): raindrops, frost and condensation, snowflakes and a
// wiper as one layer filter. The drops are hashed from cells and life cycles, so a frame is a pure
// function of the time. The slot logic below exists twice, as C++ (the CPU twin that tests and
// callers read) and as GLSL (what is drawn); keep them in step.
#include "rmtn/lens.hpp"
#include "rmtn/render.hpp"
#include "rmtn/weather.hpp"
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace rmtn {
namespace fx {
namespace {

using u32 = uint32_t;
constexpr float kInf = 1e9f;
constexpr int kCurve = 66;            // samples of a rate curve, 0.25 s apart: 16.5 s back from now
constexpr float kCurveStep = 0.25f;

// ---------------------------------------------------------------- hashing (same in the shader)
u32 hash32(u32 x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
float u01(u32 h, u32 salt) { return (float)(hash32(h + salt * 0x9e3779b9u) >> 8) * (1.0f / 16777216.0f); }
u32 hcell(int seed, int a, int b, int c) {
    u32 h = hash32((u32)seed * 0x9e3779b1u + 0x1234567u);
    h = hash32(h ^ ((u32)(a + 8192) * 0x85ebca6bu));
    h = hash32(h ^ ((u32)(b + 8192) * 0xc2b2ae35u));
    h = hash32(h ^ ((u32)c * 0x27d4eb2fu + 1u));
    return h;
}
float smooth01(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

struct Curve {
    float t0 = 0;
    float v[kCurve] = {};
    float at(float s) const {
        const float x = std::clamp((s - t0) / kCurveStep, 0.f, (float)(kCurve - 1));
        const int i = std::min((int)x, kCurve - 2);
        const float f = x - (float)i;
        return v[i] + (v[i + 1] - v[i]) * f;
    }
};
Curve bake(const std::function<float(double)>& fn, float constant, float now) {
    Curve c;
    c.t0 = now - (kCurve - 1) * kCurveStep;
    for (int i = 0; i < kCurve; i++) c.v[i] = fn ? fn((double)(c.t0 + i * kCurveStep)) : constant;
    return c;
}

// ---------------------------------------------------------------- rain slots
struct Slot {
    bool alive = false;
    float cx = 0, cy = 0, rf = 0, tSpawn = 0, life = 0, tS = 0, v = 0, maxT = 0, ph = 0;
    bool canSlide = false;
    u32 hn = 0;
};
constexpr float kSlideRadius = 0.27f;      // cells: a drop above this radius slides
constexpr float kSlideGain = 3.667f;       // px/s per px of radius, at slide 1

Slot rainSlot(const LensRain& r, const Curve& cv, int ix, int iy, int k, float sec, float S) {
    Slot s;
    const u32 h = hcell(r.seed * 7919 + 11, ix, iy, k);
    const float P = 7.f + 8.f * u01(h, 1), phase = P * u01(h, 2);
    const float n = std::floor((sec + phase) / P);
    s.hn = hash32(h ^ ((u32)(n + 100000.f) * 0x632be5abu));
    const float c0 = n * P - phase, a = 0.55f * P * u01(s.hn, 3);
    s.tSpawn = c0 + a;
    s.life = std::min(2.5f + 0.5f * P * u01(s.hn, 4), P - a);
    const float age = sec - s.tSpawn;
    if (age < 0 || age > s.life) return s;
    const float rate = std::clamp(cv.at(s.tSpawn), 0.f, 1.f);
    if (!(u01(s.hn, 5) < 0.9f * rate)) return s;
    s.cx = ((float)ix + 0.2f + 0.6f * u01(s.hn, 6)) * S;
    s.cy = ((float)iy + 0.2f + 0.6f * u01(s.hn, 7)) * S;
    s.rf = S * r.size * (0.10f + 0.34f * std::pow(u01(s.hn, 8), 1.8f));
    const float rs = kSlideRadius * S;
    s.canSlide = r.slide > 0 && s.rf > rs * 1.02f;
    if (s.canSlide) {
        s.tS = -0.7f * std::log(1.f - rs / s.rf) + 0.4f + 1.5f * u01(s.hn, 9);
        s.v = r.slide * kSlideGain * s.rf;
        s.maxT = (0.7f + 1.2f * u01(s.hn, 10)) * S;
    }
    s.ph = 6.2831853f * u01(s.hn, 11);
    s.alive = true;
    return s;
}
float travelled(const Slot& s, float age) { return s.canSlide ? std::min(std::max(age - s.tS, 0.f) * s.v, s.maxT) : 0.f; }
Vector2 posAt(const Slot& s, float age) {
    const float tr = travelled(s, age);
    return { s.cx + 0.12f * s.rf * std::sin(tr * 0.11f + s.ph), s.cy + tr };
}
float evap(float life, float age) { return 1.f - smooth01(life - 1.2f, life, age); }
float radiusAt(const Slot& s, float age) { return s.rf * (1.f - std::exp(-age / 0.7f)) * evap(s.life, age); }
// Trail drop j (1, 2, ...): spacing, position, radius, and the second it appeared.
float trailSpacing(const Slot& s) { return 0.55f * s.rf + 4.f; }
bool trailDrop(const Slot& s, int j, float sec, Vector2& pos, float& rad, float& since) {
    const float sp = trailSpacing(s), tj = (float)j * sp;
    if (tj > s.maxT) return false;
    const u32 hj = hash32(s.hn ^ ((u32)j * 0x9e3779b1u + 77u));
    if (!(u01(hj, 4) < 0.7f)) return false;
    since = s.tSpawn + s.tS + (tj + 1.2f * s.rf) / s.v;
    if (sec < since) return false;
    pos = { s.cx + 0.12f * s.rf * std::sin(tj * 0.11f + s.ph) + (u01(hj, 1) - 0.5f) * 0.5f * s.rf,
            s.cy + tj + (u01(hj, 2) - 0.5f) * sp * 0.4f };
    rad = s.rf * 0.13f * (0.6f + 0.8f * u01(hj, 3)) * smooth01(0.f, 0.4f, sec - since) * evap(s.life, sec - s.tSpawn);
    return rad > 0.f;
}

// ---------------------------------------------------------------- the wiper
struct Wipe {
    bool on = false;
    float px = 0, py = 0, L = 0, th0 = 0, th1 = 0, P = 1;
};
Wipe wipeOf(const Wiper& w, float W, float H) {
    Wipe g;
    g.on = w.period > 0;
    g.px = w.pivot.x * W; g.py = w.pivot.y * H; g.L = w.length * H;
    g.th0 = w.from * DEG2RAD; g.th1 = w.to * DEG2RAD;
    g.P = std::max(w.period, 1e-3f);
    return g;
}
// Where the blade is, 0 (parked at `from`) .. 1 (at `to`): up for 0.3 of the period, down for 0.3, rest.
float bladeU(const Wipe& g, float sec) {
    const float tau = sec - std::floor(sec / g.P) * g.P, T = 0.3f * g.P;
    if (tau < T) return 0.5f - 0.5f * std::cos(PI * tau / T);
    if (tau < 2 * T) return 0.5f + 0.5f * std::cos(PI * (tau - T) / T);
    return 0.f;
}
// The first moment after `after` that the blade touches a disc (pos, r); kInf when its arc never does.
float touchTime(const Wipe& g, Vector2 pos, float r, float after) {
    const float dx = pos.x - g.px, dy = pos.y - g.py, rho = std::sqrt(dx * dx + dy * dy);
    if (rho - r > g.L) return kInf;
    const float phi = std::atan2(dx, -dy), dl = std::asin(std::min(r / std::max(rho, 1e-3f), 1.f));
    const float span = g.th1 - g.th0, u = (phi - g.th0) / span, du = dl / std::fabs(span);
    if (u + du < 0 || u - du > 1) return kInf;
    const float uU = std::max(u - du, 0.f), uD = std::min(u + du, 1.f), T = 0.3f * g.P;
    const float upT = T * std::acos(std::clamp(1.f - 2.f * uU, -1.f, 1.f)) / PI;
    const float dnT = T + T * std::acos(std::clamp(2.f * uD - 1.f, -1.f, 1.f)) / PI;
    const float m0 = std::floor(after / g.P);
    float best = kInf;
    for (int i = 0; i < 2; i++) {
        const float base = (m0 + (float)i) * g.P, c1 = base + upT, c2 = base + dnT;
        if (c1 > after) best = std::min(best, c1);
        if (c2 > after) best = std::min(best, c2);
    }
    return best;
}
// When the blade clears a (possibly sliding) drop: the position it has at the moment it is touched.
float killTime(const Wipe& g, const Slot& s) {
    if (!g.on) return kInf;
    float T = s.tSpawn;
    for (int i = 0; i < 3; i++) {
        T = touchTime(g, posAt(s, std::max(T - s.tSpawn, 0.f)), s.rf, s.tSpawn);
        if (T >= kInf) return kInf;
    }
    return T;
}

// ---------------------------------------------------------------- snow slots
struct FSlot {
    bool alive = false;
    float cx = 0, cy = 0, rf = 0, tL = 0, hold = 0, end = 0, ph = 0;
    u32 hn = 0;
};
constexpr float kMelt = 0.9f;
FSlot flakeSlot(const LensSnow& sn, const Curve& cv, int ix, int iy, float sec, float S2) {
    FSlot s;
    const u32 h = hcell(sn.seed * 104729 + 5, ix, iy, 0);
    const float P = 5.f + 7.f * u01(h, 1), phase = P * u01(h, 2);
    const float n = std::floor((sec + phase) / P);
    s.hn = hash32(h ^ ((u32)(n + 100000.f) * 0x632be5abu));
    const float c0 = n * P - phase, a = 0.5f * P * u01(s.hn, 3);
    s.tL = c0 + a;
    s.hold = 1.2f + 2.0f * u01(s.hn, 4);
    s.end = std::min(s.tL + s.hold + kMelt + 3.5f + 5.f * u01(s.hn, 5), c0 + P);
    if (sec < s.tL || sec > s.end) return s;
    if (!(u01(s.hn, 6) < 0.5f * std::clamp(cv.at(s.tL), 0.f, 1.f))) return s;
    s.rf = S2 * (0.18f + 0.27f * u01(s.hn, 7));
    s.cx = ((float)ix + 0.25f + 0.5f * u01(s.hn, 8)) * S2;
    s.cy = ((float)iy + 0.25f + 0.5f * u01(s.hn, 9)) * S2;
    s.ph = 6.2831853f * u01(s.hn, 12);
    s.alive = true;
    return s;
}
float flakeAlpha(const FSlot& s, float sec) {
    const float age = sec - s.tL;
    return smooth01(0.f, 0.15f, age) * (age < s.hold ? 1.f : 1.f - std::clamp((age - s.hold) / kMelt, 0.f, 1.f));
}
float meltRadius(const FSlot& s, float sec) {
    const float age = sec - s.tL;
    return 0.45f * s.rf * smooth01(s.hold, s.hold + kMelt, age) * evap(s.end - s.tL, age);
}

bool snowOn(const Lens& l) { return l.snow.rate > 0 || (bool)l.snow.rateAt; }
bool rainOn(const Lens& l) { return l.rain.rate > 0 || (bool)l.rain.rateAt; }

std::vector<LensDrop> collect(const Lens& l, double seconds, float W, float H, bool wipe) {
    std::vector<LensDrop> out;
    const float sec = (float)seconds, S = LENS_CELL_OF_HEIGHT * H, S2 = LENS_SNOW_CELL_OF_HEIGHT * H;
    const Wipe g = wipeOf(l.wiper, W, H);
    auto push = [&](LensDrop::Kind kind, Vector2 pos, float rad, bool sliding, float speed, float born, u32 hn, int j) {
        LensDrop d;
        d.kind = kind; d.pos = pos; d.radius = rad; d.sliding = sliding; d.speed = speed; d.born = born;
        d.id = ((uint64_t)hn << 16) | ((uint64_t)(int)kind << 12) | (uint64_t)(j & 0xfff);
        out.push_back(d);
    };
    if (rainOn(l)) {
        const Curve cv = bake(l.rain.rateAt, l.rain.rate, sec);
        for (int iy = -3; iy <= (int)std::ceil(H / S) + 2; iy++)
            for (int ix = -1; ix <= (int)std::ceil(W / S) + 1; ix++)
                for (int k = 0; k < 2; k++) {
                    const Slot s = rainSlot(l.rain, cv, ix, iy, k, sec, S);
                    if (!s.alive) continue;
                    if (wipe && g.on && killTime(g, s) <= sec) continue;
                    const float age = sec - s.tSpawn, tr = travelled(s, age);
                    const float rad = radiusAt(s, age);
                    if (rad > 0.3f)
                        push(LensDrop::Kind::Rain, posAt(s, age), rad, s.canSlide && age > s.tS && tr < s.maxT, s.v, s.tSpawn, s.hn, 0);
                    if (!s.canSlide || tr <= 0) continue;
                    for (int j = 1; (float)j * trailSpacing(s) <= tr; j++) {
                        Vector2 p; float rt, since;
                        if (!trailDrop(s, j, sec, p, rt, since)) continue;
                        if (wipe && g.on && touchTime(g, p, rt, since) <= sec) continue;
                        push(LensDrop::Kind::Trail, p, rt, false, 0, since, s.hn, j);
                    }
                }
    }
    if (snowOn(l)) {
        const Curve cv = bake(l.snow.rateAt, l.snow.rate, sec);
        for (int iy = -1; iy <= (int)std::ceil(H / S2) + 1; iy++)
            for (int ix = -1; ix <= (int)std::ceil(W / S2) + 1; ix++) {
                const FSlot f = flakeSlot(l.snow, cv, ix, iy, sec, S2);
                if (!f.alive) continue;
                if (wipe && g.on && touchTime(g, { f.cx, f.cy }, f.rf, f.tL) <= sec) continue;
                const float rad = meltRadius(f, sec);
                if (rad > 0.3f) push(LensDrop::Kind::Melt, { f.cx, f.cy }, rad, false, 0, f.tL + f.hold, f.hn, 0);
            }
    }
    return out;
}

} // namespace

std::vector<LensDrop> lensDrops(const Lens& l, double seconds, float w, float h) { return collect(l, seconds, w, h, true); }
std::vector<LensDrop> lensDropsUnwiped(const Lens& l, double seconds, float w, float h) { return collect(l, seconds, w, h, false); }

std::vector<LensFlake> lensFlakes(const Lens& l, double seconds, float w, float h) {
    std::vector<LensFlake> out;
    if (!snowOn(l)) return out;
    const float sec = (float)seconds, S2 = LENS_SNOW_CELL_OF_HEIGHT * h;
    const Wipe g = wipeOf(l.wiper, w, h);
    const Curve cv = bake(l.snow.rateAt, l.snow.rate, sec);
    for (int iy = -1; iy <= (int)std::ceil(h / S2) + 1; iy++)
        for (int ix = -1; ix <= (int)std::ceil(w / S2) + 1; ix++) {
            const FSlot f = flakeSlot(l.snow, cv, ix, iy, sec, S2);
            if (!f.alive || (g.on && touchTime(g, { f.cx, f.cy }, f.rf, f.tL) <= sec)) continue;
            const float a = flakeAlpha(f, sec);
            if (a <= 0) continue;
            out.push_back({ { f.cx, f.cy }, f.rf, smooth01(f.hold, f.hold + kMelt, sec - f.tL), a });
        }
    return out;
}

float wiperAngle(const Wiper& w, double seconds) {
    const Wipe g = wipeOf(w, 1, 1);
    if (!g.on) return w.from;
    return w.from + (w.to - w.from) * bladeU(g, (float)seconds);
}
float lensSlideSpeed(const LensRain& r, float radius) { return r.slide * kSlideGain * radius; }
float lensFrostAmount(const LensFrost& f, int t) {
    if (f.endFrame <= f.startFrame) return std::clamp(f.amount, 0.f, 1.f);
    return std::clamp(f.amount, 0.f, 1.f) * smooth01((float)f.startFrame, (float)f.endFrame, (float)t);
}

Filter lens(const Lens& l) {
    Filter f;
    f.type = Filter::Type::Lens;
    f.lens = std::make_shared<const Lens>(l);
    return f;
}
Filter lensRain(const LensRain& r) { Lens l; l.rain = r; return lens(l); }
Filter lensFrost(const LensFrost& fr) { Lens l; l.frost = fr; return lens(l); }
Filter lensSnow(const LensSnow& s) { Lens l; l.snow = s; return lens(l); }
Filter wiper(const Wiper& w) { Lens l; l.wiper = w; return lens(l); }
std::function<float(double)> rainOf(const g3d::Weather& w, float gain) {
    return [w, gain](double s) { return w.rainAt(s) * gain; };
}
std::function<float(double)> snowOf(const g3d::Weather& w, float gain) {
    return [w, gain](double s) { return w.snowAt(s) * gain; };
}

} // namespace fx

// ================================================================ the shader
namespace {

const char* LENS_FS = R"(#version 330
in vec2 fragTexCoord; in vec4 fragColor;
uniform sampler2D texture0;
uniform vec2 texel; uniform float pxScale; uniform vec2 dims;
uniform float sec; uniform float S; uniform float S2;
uniform int rainOn; uniform float rSize; uniform float rSlide; uniform float rBlur; uniform int rSeed;
uniform float rateCurve[66]; uniform float snowCurve[66]; uniform float curveT0;
uniform int snowOn; uniform int sSeed;
uniform int frostOn; uniform float fAmount; uniform int fKind; uniform int fEdges; uniform int fSeed;
uniform vec3 fClear[4]; uniform int fNClear;
uniform int wipOn; uniform vec2 wPivot; uniform float wLen; uniform float wTh0; uniform float wTh1;
uniform float wPeriod; uniform float wBladeW; uniform int wDraw; uniform float wAng;
out vec4 finalColor;

uint hash32(uint x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
float u01(uint h, uint salt) { return float(hash32(h + salt * 0x9e3779b9u) >> 8) * (1.0 / 16777216.0); }
uint hcell(int seed, int a, int b, int c) {
    uint h = hash32(uint(seed) * 0x9e3779b1u + 0x1234567u);
    h = hash32(h ^ (uint(a + 8192) * 0x85ebca6bu));
    h = hash32(h ^ (uint(b + 8192) * 0xc2b2ae35u));
    h = hash32(h ^ (uint(c) * 0x27d4eb2fu + 1u));
    return h;
}
float sm(float a, float b, float x) { float t = clamp((x - a) / (b - a), 0.0, 1.0); return t * t * (3.0 - 2.0 * t); }
float curveRate(float s) {
    float x = clamp((s - curveT0) / 0.25, 0.0, 65.0);
    int i = min(int(x), 64);
    return mix(rateCurve[i], rateCurve[i + 1], x - float(i));
}
float curveSnow(float s) {
    float x = clamp((s - curveT0) / 0.25, 0.0, 65.0);
    int i = min(int(x), 64);
    return mix(snowCurve[i], snowCurve[i + 1], x - float(i));
}
float hash21(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1, 0)), f.x), mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) { s += a * vnoise(p); p = p * 2.03 + 17.1; a *= 0.5; }
    return s / 0.97;
}
float luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
vec3 unpre(vec4 c) { return c.a > 0.0 ? c.rgb / c.a : vec3(0.0); }
// The layer's content at a point in composition px, y down.
vec2 uvOf(vec2 q) { return clamp(vec2(q.x, dims.y - q.y) * pxScale * texel, 0.5 * texel, 1.0 - 0.5 * texel); }
vec4 tex(vec2 q) { return texture(texture0, uvOf(q)); }
vec4 texL(vec2 q, float lod) { return textureLod(texture0, uvOf(q), lod); }

// ---- rain slots (the C++ twin is rainSlot in lens.cpp)
struct Slot { bool alive; float cx, cy, rf, tSpawn, life, tS, v, maxT, ph; bool canSlide; uint hn; };
Slot rainSlot(int ix, int iy, int k) {
    Slot s; s.alive = false; s.cx = 0.0; s.cy = 0.0; s.rf = 0.0; s.tSpawn = 0.0; s.life = 0.0; s.tS = 0.0; s.v = 0.0; s.maxT = 0.0; s.ph = 0.0; s.canSlide = false;
    uint h = hcell(rSeed * 7919 + 11, ix, iy, k);
    float P = 7.0 + 8.0 * u01(h, 1u), phase = P * u01(h, 2u);
    float n = floor((sec + phase) / P);
    s.hn = hash32(h ^ (uint(n + 100000.0) * 0x632be5abu));
    float c0 = n * P - phase, a = 0.55 * P * u01(s.hn, 3u);
    s.tSpawn = c0 + a;
    s.life = min(2.5 + 0.5 * P * u01(s.hn, 4u), P - a);
    float age = sec - s.tSpawn;
    if (age < 0.0 || age > s.life) return s;
    float rate = clamp(curveRate(s.tSpawn), 0.0, 1.0);
    if (!(u01(s.hn, 5u) < 0.9 * rate)) return s;
    s.cx = (float(ix) + 0.2 + 0.6 * u01(s.hn, 6u)) * S;
    s.cy = (float(iy) + 0.2 + 0.6 * u01(s.hn, 7u)) * S;
    s.rf = S * rSize * (0.10 + 0.34 * pow(u01(s.hn, 8u), 1.8));
    float rs = 0.27 * S;
    s.canSlide = rSlide > 0.0 && s.rf > rs * 1.02;
    if (s.canSlide) {
        s.tS = -0.7 * log(1.0 - rs / s.rf) + 0.4 + 1.5 * u01(s.hn, 9u);
        s.v = rSlide * 3.667 * s.rf;
        s.maxT = (0.7 + 1.2 * u01(s.hn, 10u)) * S;
    }
    s.ph = 6.2831853 * u01(s.hn, 11u);
    s.alive = true;
    return s;
}
float travelled(Slot s, float age) { return s.canSlide ? min(max(age - s.tS, 0.0) * s.v, s.maxT) : 0.0; }
vec2 posAt(Slot s, float age) { float tr = travelled(s, age); return vec2(s.cx + 0.12 * s.rf * sin(tr * 0.11 + s.ph), s.cy + tr); }
float evap(float life, float age) { return 1.0 - sm(life - 1.2, life, age); }
float radiusAt(Slot s, float age) { return s.rf * (1.0 - exp(-age / 0.7)) * evap(s.life, age); }
float trailSpacing(Slot s) { return 0.55 * s.rf + 4.0; }
bool trailDrop(Slot s, int j, out vec2 pos, out float rad, out float since) {
    pos = vec2(0.0); rad = 0.0; since = 0.0;
    float sp = trailSpacing(s), tj = float(j) * sp;
    if (tj > s.maxT) return false;
    uint hj = hash32(s.hn ^ (uint(j) * 0x9e3779b1u + 77u));
    if (!(u01(hj, 4u) < 0.7)) return false;
    since = s.tSpawn + s.tS + (tj + 1.2 * s.rf) / s.v;
    if (sec < since) return false;
    pos = vec2(s.cx + 0.12 * s.rf * sin(tj * 0.11 + s.ph) + (u01(hj, 1u) - 0.5) * 0.5 * s.rf, s.cy + tj + (u01(hj, 2u) - 0.5) * sp * 0.4);
    rad = s.rf * 0.13 * (0.6 + 0.8 * u01(hj, 3u)) * sm(0.0, 0.4, sec - since) * evap(s.life, sec - s.tSpawn);
    return rad > 0.0;
}

)" R"(
// ---- the wiper
float touchTime(vec2 pos, float r, float after) {
    vec2 d = pos - wPivot;
    float rho = length(d);
    if (rho - r > wLen) return 1e9;
    float phi = atan(d.x, -d.y), dl = asin(min(r / max(rho, 1e-3), 1.0));
    float span = wTh1 - wTh0, u = (phi - wTh0) / span, du = dl / abs(span);
    if (u + du < 0.0 || u - du > 1.0) return 1e9;
    float uU = max(u - du, 0.0), uD = min(u + du, 1.0), T = 0.3 * wPeriod;
    float upT = T * acos(clamp(1.0 - 2.0 * uU, -1.0, 1.0)) / 3.14159265;
    float dnT = T + T * acos(clamp(2.0 * uD - 1.0, -1.0, 1.0)) / 3.14159265;
    float m0 = floor(after / wPeriod);
    float best = 1e9;
    for (int i = 0; i < 2; i++) {
        float base = (m0 + float(i)) * wPeriod, c1 = base + upT, c2 = base + dnT;
        if (c1 > after) best = min(best, c1);
        if (c2 > after) best = min(best, c2);
    }
    return best;
}
float killTime(Slot s) {
    if (wipOn == 0) return 1e9;
    float T = s.tSpawn;
    for (int i = 0; i < 3; i++) {
        T = touchTime(posAt(s, max(T - s.tSpawn, 0.0)), s.rf, s.tSpawn);
        if (T >= 1e9) return 1e9;
    }
    return T;
}

// ---- snow slots
struct FSlot { bool alive; float cx, cy, rf, tL, hold, end, ph, op; };
FSlot flakeSlot(int ix, int iy) {
    FSlot s; s.alive = false; s.cx = 0.0; s.cy = 0.0; s.rf = 0.0; s.tL = 0.0; s.hold = 0.0; s.end = 0.0; s.ph = 0.0; s.op = 1.0;
    uint h = hcell(sSeed * 104729 + 5, ix, iy, 0);
    float P = 5.0 + 7.0 * u01(h, 1u), phase = P * u01(h, 2u);
    float n = floor((sec + phase) / P);
    uint hn = hash32(h ^ (uint(n + 100000.0) * 0x632be5abu));
    float c0 = n * P - phase, a = 0.5 * P * u01(hn, 3u);
    s.tL = c0 + a;
    s.hold = 1.2 + 2.0 * u01(hn, 4u);
    s.end = min(s.tL + s.hold + 0.9 + 3.5 + 5.0 * u01(hn, 5u), c0 + P);
    if (sec < s.tL || sec > s.end) return s;
    if (!(u01(hn, 6u) < 0.5 * clamp(curveSnow(s.tL), 0.0, 1.0))) return s;
    s.rf = S2 * (0.18 + 0.27 * u01(hn, 7u)); s.op = 0.45 + 0.5 * u01(hn, 13u);
    s.cx = (float(ix) + 0.25 + 0.5 * u01(hn, 8u)) * S2;
    s.cy = (float(iy) + 0.25 + 0.5 * u01(hn, 9u)) * S2;
    s.ph = 6.2831853 * u01(hn, 12u);
    s.alive = true;
    return s;
}

// ---- out-of-focus background: a disc of taps from the mip chain, bright lights bloom
vec4 discBlur(vec2 p, float R, float bokeh) {
    if (R < 0.4) return tex(p);
    float lod = max(log2(R * pxScale * 1.5 / 4.9), 0.0);
    float rot = hash21(gl_FragCoord.xy) * 6.2832;
    vec4 sum = vec4(0.0); float ws = 0.0;
    for (int i = 0; i < 24; i++) {
        float a = float(i) * 2.39996323 + rot, rr = R * sqrt((float(i) + 0.5) / 24.0);
        vec4 s = texL(p + vec2(cos(a), sin(a)) * rr, lod);
        float w = 1.0 + bokeh * sm(0.45, 0.95, luma(unpre(s)));
        sum += s * w; ws += w;
    }
    return sum / ws;
}

)" R"(
// ---- frost
float ridge(vec2 q) {
    float s = 0.0, a = 1.0, f = 1.0; vec2 w = vec2(0.0);
    for (int i = 0; i < 5; i++) {
        vec2 qq = q * f + w;
        float n = vnoise(qq);
        float r = 1.0 - abs(2.0 * n - 1.0);
        s += a * r * r * r;
        w = (vec2(vnoise(qq + 7.3), vnoise(qq + 19.1)) - 0.5) * 1.6;
        f *= 2.03; a *= 0.5;
    }
    return s / 1.9;
}
vec2 inwardOf(vec2 p) {
    float k = 0.12 * dims.y;
    float dl = p.x, dr = dims.x - p.x, dt = p.y, db = dims.y - p.y;
    vec2 v = vec2(1.0 / (dl + k) - 1.0 / (dr + k), 1.0 / (dt + k) - 1.0 / (db + k));
    return normalize(v + 1e-6);
}
float edgeDist(vec2 p) { return min(min(p.x, dims.x - p.x), min(p.y, dims.y - p.y)) / (0.5 * dims.y); }
// How much of the glass is covered here, 0..1, at amount A.
float frostCover(vec2 p, float A) {
    float n = fbm(p / (0.16 * dims.y) + float(fSeed) * 3.7);
    float c;
    if (fEdges == 1) {
        float dl = p.x, dr = dims.x - p.x, dt = p.y, db = dims.y - p.y;
        float k = 0.22 * dims.y;
        float corner = max(0.0, 1.0 - length(vec2(min(dl, dr), min(dt, db))) / (0.9 * dims.y));
        float e = edgeDist(p) - 0.55 * corner * corner;
        float reach = A * 1.45;
        c = sm(0.0, 0.18, reach - e + (n - 0.5) * 0.55);
    } else {
        c = sm(0.0, 0.25, A * 1.25 - 0.55 + (n - 0.5) * 1.1 + 0.3);
    }
    for (int i = 0; i < 4; i++) {
        if (i >= fNClear) break;
        float rr = fClear[i].z * (0.85 + 0.3 * vnoise(p / (0.35 * fClear[i].z + 1.0) + float(i) * 5.0));
        c *= sm(rr * 0.78, rr * 1.12, length(p - fClear[i].xy));
    }
    return c * step(0.001, A);
}
// The crystal pattern: 0 (smooth glass) .. 1 (a bright ridge).
vec2 hash22(vec2 p) { return vec2(hash21(p), hash21(p + vec2(19.3, 7.7))); }
// Frost grows as dendrites: a stem from a nucleation point on the glass's edge running inward, branches off it at 60 degrees,
// sub-branches off those, and finer barbs. Everything is a distance to a line segment, anti-aliased with a smoothstep one pixel wide.
// x runs along an axis from its root, y across it; a branch is entered by returning its own (l, m) frame.
float branchOf(float x, float y, float lam, float len, out float l, out float m, out float lb) {
    float u = abs(y), sg = y < 0.0 ? -1.0 : 1.0;
    float k0 = floor((x - u * 1.192) / lam + 0.5);
    float best = 1e3; l = 0.0; m = 0.0; lb = 0.0;
    for (int dk = -1; dk <= 1; dk++) {
        float k = k0 + float(dk);
        float t = k * lam;
        float hk = hash21(vec2(k * 1.37 + lam, len));
        if (k < 1.0 || t > len * 0.97 || hk < 0.22) continue;
        float bl = (0.30 + 0.40 * hk) * len * pow(max(sin(3.14159265 * t / len), 0.0), 0.55) + 0.8 * lam;
        float ll = (x - t) * 0.766 + u * 0.643, mm = ((x - t) * 0.643 - u * 0.766) * sg;
        float d = length(vec2(ll - clamp(ll, 0.0, bl), mm));
        if (d < best) { best = d; l = ll; m = mm; lb = bl; }
    }
    return best;
}
float dendLayer(float a, float s, float edgeId, float C, float salt, float aa, float lamK, float wk) {
    float W = C * wk;
    float i0 = floor(s / W);
    float v = 0.0;
    for (int di = -1; di <= 1; di++) {
        float i = i0 + float(di);
        vec2 h = hash22(vec2(i + edgeId * 131.0, salt + float(fSeed) * 3.1));
        float s0 = (i + 0.15 + 0.7 * h.x) * W;
        float L = C * (0.40 + 0.60 * h.y);
        float x = a, y = s - s0 + 0.03 * C * sin(a / C * 6.0 + h.x * 6.28);
        if (x < -3.0 || x > L * 1.45 + 3.0) continue;
        float lam = C * lamK;
        float dS = length(vec2(y, max(x - L, 0.0)));
        float w0 = max(0.6, 0.11 * min(lam, 0.011 * dims.y) * (1.0 - 0.7 * clamp(x / L, 0.0, 1.0)));
        v = max(v, 1.0 - sm(w0 - aa, w0 + aa, dS));
        float l1, m1, b1;
        float d1 = branchOf(x, y, lam, L, l1, m1, b1);
        float w1 = max(0.5, 0.045 * lam * (1.0 - 0.75 * clamp(l1 / max(b1, 1e-3), 0.0, 1.0)));
        v = max(v, 0.8 * (1.0 - sm(w1 - aa, w1 + aa, d1)));
        if (d1 < 0.9 * b1 + 2.0) {
            float l2, m2, b2;
            float d2 = branchOf(l1, m1, lam * 0.5, b1, l2, m2, b2);
            float w2 = max(0.5, 0.032 * lam * (1.0 - 0.75 * clamp(l2 / max(b2, 1e-3), 0.0, 1.0)));
            v = max(v, 0.55 * (1.0 - sm(w2 - aa, w2 + aa, d2)));
            if (d2 < 0.9 * b2 + 2.0) {
                float l3, m3, b3;
                float d3 = branchOf(l2, m2, lam * 0.25, b2, l3, m3, b3);
                float w3 = max(0.45, 0.02 * lam);
                v = max(v, 0.34 * (1.0 - sm(w3 - aa, w3 + aa, d3)));
            }
        }
    }
    return v;
}
// 0 (smooth glass) .. 1 (crystal). The frame is that of the nearest edge: a is the distance in from it, s the position along it,
// so every frond runs inward and the pattern follows the creeping front.
float frondsOf(float a, float s, float id, float aa, bool full) {
    float H = dims.y;
    float v = dendLayer(a, s, id, 0.34 * H, 1.0, aa, 0.11, 0.5);
    v = max(v, 0.9 * dendLayer(a, s, id, 0.16 * H, 4.1, aa, 0.13, 0.5));
    if (!full) return v;
    v = max(v, 0.5 * dendLayer(a, s, id, 0.075 * H, 9.3, aa, 0.16, 0.5));
    v = max(v, 0.35 * dendLayer(a, s, id, 0.035 * H, 13.7, aa, 0.22, 0.5));
    v = max(v, 0.8 * dendLayer(a, s, id, 0.32 * H, 17.9, aa, 4.0, 0.016));       // long fine hairs running straight in
    return v;
}
float crystal(vec2 p) {
    float H = dims.y;
    float d[4] = float[4](p.x, dims.x - p.x, p.y, dims.y - p.y);
    float dmin = min(min(d[0], d[1]), min(d[2], d[3]));
    float aa = fwidth(p.x) * 1.15 + 0.2;
    // the lines are not ruled: bend them with two scales of noise (the warp is a function of position only)
    vec2 wp = vec2(vnoise(p / (0.07 * H)), vnoise(p / (0.07 * H) + 8.3)) - 0.5;
    vec2 wq = vec2(vnoise(p / (0.018 * H) + 3.1), vnoise(p / (0.018 * H) + 11.7)) - 0.5;
    vec2 wd = wp * 0.032 * H + wq * 0.006 * H;
    float v = 0.0;
    // every edge grows its own fronds, weighted by how near it is: at a corner two sets interpenetrate, with no seam
    for (int e = 0; e < 4; e++) {
        float w = exp(-(d[e] - dmin) / (0.06 * H));
        if (w < 0.06) continue;
        float a = d[e], s = e < 2 ? p.y : p.x;
        if (e < 2) { a += 0.2 * wd.x * (e == 0 ? 1.0 : -1.0); s += wd.y; } else { a += 0.2 * wd.y * (e == 2 ? 1.0 : -1.0); s += wd.x; }
        v = max(v, w * frondsOf(a, s, float(e), aa, true));
    }
    // and each corner nucleates fronds that grow along its diagonal
    vec2 cc = vec2(p.x < 0.5 * dims.x ? 0.0 : dims.x, p.y < 0.5 * dims.y ? 0.0 : dims.y);
    float wc = 0.85 * exp(-length(p - cc) / (0.22 * H));
    if (wc > 0.06) {
        vec2 dir = normalize(vec2(cc.x == 0.0 ? 1.0 : -1.0, cc.y == 0.0 ? 1.0 : -1.0));
        vec2 dq = p - cc + wd * 0.8;
        float cid = 4.0 + (cc.x == 0.0 ? 0.0 : 1.0) + (cc.y == 0.0 ? 0.0 : 2.0);
        v = max(v, wc * frondsOf(dot(dq, dir), dot(dq, vec2(-dir.y, dir.x)), cid, aa, false));
    }
    return clamp(v, 0.0, 1.0);
}

)" R"(
// ---- the drop's look: what it sees, its rim and its highlight
// The drop's outline: the offset from its centre bent so that the drop is an irregular blob (a vertical ellipse with a wobble) and,
// when it has slid, a teardrop with a narrower top. Every factor is at least 1, so the drop never leaves the disc of radius r.
vec2 shapeVec(vec2 dd, float r, uint h, float tear) {
    float el = 1.0 + 0.50 * u01(h, 20u);
    vec2 q = vec2(dd.x * el, dd.y);
    float up = clamp(-q.y / r, 0.0, 1.0);
    q.x *= 1.0 + tear * 0.9 * up;
    float th = atan(q.y, q.x);
    float w = 0.16 * (0.5 + 0.5 * sin(2.0 * th + 6.28 * u01(h, 21u)))
            + 0.20 * (0.5 + 0.5 * sin(3.0 * th + 6.28 * u01(h, 22u)))
            + 0.05 * (0.5 + 0.5 * sin(5.0 * th + 6.28 * u01(h, 23u)));
    return q * (1.0 + w);
}
// A drop on glass is a low lens: almost clear, so inside it is the sharp picture, flipped and squeezed (about 3.6 radii of it
// into the drop, so a whole lamp fits), with a thin dark rim where the surface is steep and a glint only where a bright light
// lies in the reflected direction.
vec4 dropLook(vec2 p, vec2 c, float r, uint h, float tear, float cov, vec4 base) {
    vec2 dd = shapeVec(p - c, r, h, tear);
    float len = length(dd);
    float q = len / r;
    vec2 u = dd / r;
    float hgt = sqrt(max(1.0 - q * q, 0.0));
    vec2 nxy = u / max(hgt, 0.5) * 0.25;                      // the slope of a low spherical cap
    vec2 look = c - (u * 3.6 + nxy * 0.8) * r;
    vec2 ca = u * r * 0.04;
    vec4 s;
    s.r = tex(look + ca).r; s.g = tex(look).g; s.b = tex(look - ca).b; s.a = tex(look).a;
    float inside = r - len;                                    // px from the edge
    float big = sm(3.5, 8.0, r);                               // a drop under about 5 px is only a faint lens: no dark arc
    float rim = (1.0 - sm(0.0, clamp(0.14 * r, 1.0, 5.0), inside)) * big;
    vec2 dir = u / max(length(u), 1e-3);
    float lit = dot(dir, vec2(0.55, 0.83));
    s.rgb = s.rgb * (1.0 - 0.45 * rim * (1.0 - max(lit, 0.0))) + vec3(0.62, 0.66, 0.74) * 0.13 * rim * max(lit, 0.0) * s.a;   // dark on the upper side, a faint gathered glow low
    vec4 refl = tex(c + dir * r * 4.5);
    float glint = sm(0.5, 0.95, q) * max(luma(unpre(refl)) - 0.5, 0.0) * 1.3;
    s.rgb += glint * s.a;
    return mix(base, s, cov);
}

)" R"(
void main() {
    vec2 p = vec2(gl_FragCoord.x, dims.y * pxScale - gl_FragCoord.y) / pxScale;
    vec4 c0 = texelFetch(texture0, ivec2(gl_FragCoord.xy), 0);
    if (rainOn == 0 && snowOn == 0 && frostOn == 0 && wipOn == 0) { finalColor = c0; return; }

    // ---- the drops that cover this pixel: the deepest one wins
    float bestDepth = -1e9; vec2 bc = vec2(0.0); float br = 0.0; uint bh = 0u; float bt = 0.0;
    float lane = 0.0;
    if (rainOn == 1) {
        int cx = int(floor(p.x / S)), cy = int(floor(p.y / S));
        for (int iy = cy - 2; iy <= cy + 1; iy++)
            for (int ix = cx - 1; ix <= cx + 1; ix++)
                for (int k = 0; k < 2; k++) {
                    Slot s = rainSlot(ix, iy, k);
                    if (!s.alive) continue;
                    if (killTime(s) <= sec) continue;
                    float age = sec - s.tSpawn, tr = travelled(s, age);
                    vec2 c = posAt(s, age);
                    float r = radiusAt(s, age);
                    if (r > 0.3) {
                        float tear = (s.canSlide && age > s.tS) ? 1.0 : 0.0;
                        float depth = r - length(shapeVec(p - c, r, s.hn, tear));
                        if (depth > bestDepth) { bestDepth = depth; bc = c; br = r; bh = s.hn; bt = tear; }
                    }
                    if (!s.canSlide || tr <= 0.0) continue;
                    float ly = p.y - s.cy;                       // the lane the drop has swept clear (condensation)
                    if (ly > -s.rf && ly < tr) {
                        float lx = s.cx + 0.12 * s.rf * sin(clamp(ly, 0.0, tr) * 0.11 + s.ph);
                        lane = max(lane, (1.0 - sm(0.55 * s.rf, 0.8 * s.rf, abs(p.x - lx))) * sm(-s.rf, 0.0, ly));
                    }
                    float sp = trailSpacing(s);
                    int jn = int(floor(ly / sp + 0.5));
                    for (int j = jn - 1; j <= jn + 1; j++) {
                        if (j < 1) continue;
                        vec2 tp; float rt, since;
                        if (!trailDrop(s, j, tp, rt, since)) continue;
                        if (wipOn == 1 && touchTime(tp, rt, since) <= sec) continue;
                        uint hj2 = s.hn ^ uint(j * 7919);
                        float depth = rt - length(shapeVec(p - tp, rt, hj2, 0.0));
                        if (depth > bestDepth) { bestDepth = depth; bc = tp; br = rt; bh = hj2; bt = 0.0; }
                    }
                }
    }
    // landed snowflakes; each melts into a small drop
    float flake = 0.0; vec3 flakeCol = vec3(0.0);
    if (snowOn == 1) {
        int cx = int(floor(p.x / S2)), cy = int(floor(p.y / S2));
        for (int iy = cy - 1; iy <= cy + 1; iy++)
            for (int ix = cx - 1; ix <= cx + 1; ix++) {
                FSlot f = flakeSlot(ix, iy);
                if (!f.alive) continue;
                if (wipOn == 1 && touchTime(vec2(f.cx, f.cy), f.rf, f.tL) <= sec) continue;
                float age = sec - f.tL;
                float a = f.op * sm(0.0, 0.15, age) * (age < f.hold ? 1.0 : 1.0 - clamp((age - f.hold) / 0.9, 0.0, 1.0));
                vec2 d = p - vec2(f.cx, f.cy);
                float ang = atan(d.y, d.x) - f.ph, len = length(d);
                // close to the glass and out of focus: a soft translucent disc with a faint six-fold structure; melting, it
                // shrinks to an irregular wet blotch (the drop it becomes is listed in the drop pass)
                float m = age < f.hold ? 0.0 : clamp((age - f.hold) / 0.9, 0.0, 1.0);
                float arm = pow(abs(cos(3.0 * ang)), 4.0);
                float wet = 0.75 + 0.5 * vnoise(vec2(ang * 1.6 + f.ph, f.rf));
                float R = f.rf * (0.98 + 0.05 * arm) * mix(1.0, 0.6 * wet, m);
                float x = len / R;
                float body = 1.0 - sm(0.05, 1.2, x);
                float spine = pow(abs(cos(3.0 * ang)), 6.0) * (1.0 - sm(0.2, 0.95, x)) * 0.06 * (1.0 - m);
                float v = clamp(body * 0.72 + spine, 0.0, 1.0) * a;
                flake = max(flake, v);
                if (age >= f.hold) {
                    float mr = 0.45 * f.rf * sm(f.hold, f.hold + 0.9, age) * evap(f.end - f.tL, age);
                    uint hb = hash32(uint(ix + 8192) * 73856093u ^ uint(iy + 8192) * 19349663u);
                    float depth = mr - length(shapeVec(d, mr, hb, 0.0));
                    if (mr > 0.3 && depth > bestDepth) { bestDepth = depth; bc = vec2(f.cx, f.cy); br = mr; bh = hb; bt = 0.0; }
                }
            }
    }

    // ---- the picture behind the drops: out of focus, then frosted or fogged
    float bokeh = 7.0;
    vec4 col = rainOn == 1 && rBlur > 0.0 ? discBlur(p, rBlur, bokeh) : c0;
    if (frostOn == 1) {
        float cover = frostCover(p, fAmount);
        if (fKind == 1) {                                          // condensation: a fog with clear streaks
            float fog = cover * (0.82 + 0.18 * (1.0 - edgeDist(p))) * (1.0 - lane);
            float st = 0.88 + 0.12 * vnoise(p / 1.8) + 0.12 * (vnoise(p / 5.0) - 0.5);
            float R = fog * 0.016 * dims.y;
            vec4 b = discBlur(p, sqrt(R * R + rBlur * rBlur * float(rainOn)), 2.0);
            vec3 fogc = vec3(0.80, 0.84, 0.89) * (0.62 + 0.45 * luma(unpre(b)));
            col = mix(col, vec4(fogc * b.a, b.a) * 0.85 + b * 0.15, clamp(fog * 0.85 * st, 0.0, 1.0));
            col = mix(col, b, 0.5 * fog * (1.0 - 0.0));
        } else if (cover > 0.002) {                                // frost: crystals scatter and whiten what is behind
            float cr = crystal(p);
            float op = cover * (0.24 + 0.62 * cr);                // milky where there is only a film, whiter where crystals stand
            float R = cover * (0.006 + 0.010 * (1.0 - cr)) * dims.y;
            vec4 b = discBlur(p, sqrt(R * R + rBlur * rBlur * float(rainOn)), 1.5);
            float rim = 4.0 * cr * (1.0 - cr);                    // a thin bright edge, only on a crystal's boundary
            vec3 ice = vec3(0.90, 0.95, 1.0);
            vec3 mixed = mix(b.rgb, ice * max(b.a, cover), clamp(op * 0.80, 0.0, 0.92)) + vec3(0.8, 0.88, 1.0) * rim * 0.10 * cover;
            col = vec4(mixed, max(b.a, op * 0.9 * (b.a > 0.01 ? 1.0 : 0.0)));
        }
    }
    // ---- drops
    if (bestDepth > -1.0) {
        float cov = clamp(bestDepth * pxScale + 0.5, 0.0, 1.0);
        col = dropLook(p, bc, br, bh, bt, cov, col);
        // a thin dark line round the drop, so it reads on a bright ground
        float ring = sm(-1.4, -0.3, bestDepth) * (1.0 - sm(-0.3, 0.7, bestDepth)) * 0.10 * sm(3.5, 8.0, br);
        col.rgb *= 1.0 - ring;
    }
    // ---- flakes on top: soft white, they sit on the glass
    if (flake > 0.0) {
        vec3 w = vec3(0.93, 0.96, 1.0);
        col = vec4(mix(col.rgb, w * max(col.a, flake), flake * 0.75), max(col.a, flake));
    }
    // ---- the blade
    if (wipOn == 1 && wDraw == 1) {
        vec2 dir = vec2(sin(wAng), -cos(wAng));
        vec2 d = p - wPivot;
        float along = dot(d, dir), across = abs(d.x * dir.y - d.y * dir.x);
        float bw = wBladeW * dims.y;
        float onBlade = step(0.0, along) * step(along, wLen);
        float armW = 0.28 * bw, bladeW = 0.5 * bw;
        float isBlade = step(0.30 * wLen, along);
        float w = mix(armW, bladeW, isBlade);
        float m = (1.0 - sm(w - 1.0 / pxScale, w + 1.0 / pxScale, across)) * onBlade;
        float edge = (1.0 - sm(w * 0.6, w, across)) * 0.0;
        vec3 rubber = vec3(0.025, 0.027, 0.032) + vec3(0.05) * (1.0 - sm(0.0, w, across)) * isBlade;
        col = vec4(mix(col.rgb, rubber, m * 0.96), max(col.a, m));
    }
    finalColor = col;
}
)";

Shader shader{};
int Lm[40];
enum { L_texel, L_pxScale, L_dims, L_sec, L_S, L_S2, L_rainOn, L_rSize, L_rSlide, L_rBlur, L_rSeed, L_rateCurve, L_snowCurve, L_curveT0,
       L_snowOn, L_sSeed, L_frostOn, L_fAmount, L_fKind, L_fEdges, L_fSeed, L_fClear, L_fNClear, L_wipOn, L_wPivot, L_wLen,
       L_wTh0, L_wTh1, L_wPeriod, L_wBladeW, L_wDraw, L_wAng, L_COUNT };

void ensureShader() {
    if (shader.id) return;
    shader = LoadShaderFromMemory(nullptr, LENS_FS);
    const char* names[L_COUNT] = { "texel", "pxScale", "dims", "sec", "S", "S2", "rainOn", "rSize", "rSlide", "rBlur", "rSeed", "rateCurve",
        "snowCurve", "curveT0", "snowOn", "sSeed", "frostOn", "fAmount", "fKind", "fEdges", "fSeed", "fClear", "fNClear", "wipOn", "wPivot",
        "wLen", "wTh0", "wTh1", "wPeriod", "wBladeW", "wDraw", "wAng" };
    for (int i = 0; i < L_COUNT; i++) Lm[i] = GetShaderLocation(shader, names[i]);
    onGraphicsShutdown([] { UnloadShader(shader); shader = Shader{}; });
}
template <class T> void setv(int id, const T& v, int type) { SetShaderValue(shader, Lm[id], &v, type); }

} // namespace

void applyLens(const Filter& f, const RenderTexture& src, const RenderTexture& dst, float scale, int t) {
    ensureShader();
    static const fx::Lens none;
    const fx::Lens& l = f.lens ? *f.lens : none;
    const float w = (float)src.texture.width, h = (float)src.texture.height;
    const float cw = w / scale, ch = h / scale;
    const int fps = std::max(frameRate(), 1);
    const float sec = (float)t / (float)fps;
    const bool rain = fx::rainOn(l), snow = fx::snowOn(l);
    const float amount = fx::lensFrostAmount(l.frost, t);
    const bool frost = amount > 0, wip = l.wiper.period > 0;
    const Vector2 texel{ 1.f / w, 1.f / h }, dims{ cw, ch };
    setv(L_texel, texel, SHADER_UNIFORM_VEC2);
    setv(L_pxScale, scale, SHADER_UNIFORM_FLOAT);
    setv(L_dims, dims, SHADER_UNIFORM_VEC2);
    setv(L_sec, sec, SHADER_UNIFORM_FLOAT);
    const float S = fx::LENS_CELL_OF_HEIGHT * ch, S2 = fx::LENS_SNOW_CELL_OF_HEIGHT * ch;
    setv(L_S, S, SHADER_UNIFORM_FLOAT);
    setv(L_S2, S2, SHADER_UNIFORM_FLOAT);
    const int iRain = rain, iSnow = snow, iFrost = frost, iWip = wip;
    setv(L_rainOn, iRain, SHADER_UNIFORM_INT);
    setv(L_snowOn, iSnow, SHADER_UNIFORM_INT);
    setv(L_frostOn, iFrost, SHADER_UNIFORM_INT);
    setv(L_wipOn, iWip, SHADER_UNIFORM_INT);
    const float rSize = std::clamp(l.rain.size, 0.3f, 1.5f), rSlide = std::max(l.rain.slide, 0.f), rBlur = std::max(l.rain.blurBehind, 0.f);
    setv(L_rSize, rSize, SHADER_UNIFORM_FLOAT);
    setv(L_rSlide, rSlide, SHADER_UNIFORM_FLOAT);
    setv(L_rBlur, rBlur, SHADER_UNIFORM_FLOAT);
    setv(L_rSeed, l.rain.seed, SHADER_UNIFORM_INT);
    setv(L_sSeed, l.snow.seed, SHADER_UNIFORM_INT);
    const fx::Curve rc = fx::bake(l.rain.rateAt, l.rain.rate, sec), sc = fx::bake(l.snow.rateAt, l.snow.rate, sec);
    SetShaderValueV(shader, Lm[L_rateCurve], rc.v, SHADER_UNIFORM_FLOAT, fx::kCurve);
    SetShaderValueV(shader, Lm[L_snowCurve], sc.v, SHADER_UNIFORM_FLOAT, fx::kCurve);
    setv(L_curveT0, rc.t0, SHADER_UNIFORM_FLOAT);
    const int kind = l.frost.kind == fx::LensFrost::Kind::Condensation ? 1 : 0, edges = l.frost.fromEdges ? 1 : 0;
    setv(L_fAmount, amount, SHADER_UNIFORM_FLOAT);
    setv(L_fKind, kind, SHADER_UNIFORM_INT);
    setv(L_fEdges, edges, SHADER_UNIFORM_INT);
    setv(L_fSeed, l.frost.seed, SHADER_UNIFORM_INT);
    float clears[12] = {};
    const int nc = (int)std::min<size_t>(l.frost.clears.size(), 4);
    for (int i = 0; i < nc; i++) { clears[i * 3] = l.frost.clears[i].x; clears[i * 3 + 1] = l.frost.clears[i].y; clears[i * 3 + 2] = l.frost.clears[i].z; }
    SetShaderValueV(shader, Lm[L_fClear], clears, SHADER_UNIFORM_VEC3, 4);
    setv(L_fNClear, nc, SHADER_UNIFORM_INT);
    const fx::Wipe g = fx::wipeOf(l.wiper, cw, ch);
    const Vector2 pivot{ g.px, g.py };
    const float ang = g.th0 + (g.th1 - g.th0) * fx::bladeU(g, sec), bw = l.wiper.bladeWidth;
    const int draw = l.wiper.drawBlade ? 1 : 0;
    setv(L_wPivot, pivot, SHADER_UNIFORM_VEC2);
    setv(L_wLen, g.L, SHADER_UNIFORM_FLOAT);
    setv(L_wTh0, g.th0, SHADER_UNIFORM_FLOAT);
    setv(L_wTh1, g.th1, SHADER_UNIFORM_FLOAT);
    setv(L_wPeriod, g.P, SHADER_UNIFORM_FLOAT);
    setv(L_wBladeW, bw, SHADER_UNIFORM_FLOAT);
    setv(L_wDraw, draw, SHADER_UNIFORM_INT);
    setv(L_wAng, ang, SHADER_UNIFORM_FLOAT);

    const bool mips = (rain && rBlur > 0.4f) || frost;     // a blur reads the mip chain
    Texture2D chain = src.texture;
    SetTextureWrap(src.texture, TEXTURE_WRAP_CLAMP);
    if (mips) {
        GenTextureMipmaps(&chain);
        SetTextureFilter(chain, TEXTURE_FILTER_TRILINEAR);
    } else SetTextureFilter(src.texture, TEXTURE_FILTER_BILINEAR);
    BeginTextureMode(dst);
    rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM);
    BeginShaderMode(shader);
    DrawTexturePro(chain, { 0, 0, w, -h }, { 0, 0, w, h }, { 0, 0 }, 0, WHITE);
    EndShaderMode();
    EndBlendMode();
    EndTextureMode();
    SetTextureFilter(src.texture, TEXTURE_FILTER_POINT);
}

} // namespace rmtn
