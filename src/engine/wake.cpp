// Wakes, ripples and floating objects: the CPU side. WakeField::sum and Ripple::radial are the twins of
// wakeField and rippleField in wake_glsl.hpp: keep the two in step (the tests compare them).
// No GL here.
#include "rmtn/scene3d.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace rmtn {
namespace g3d {

namespace {

constexpr float kG = 9.81f;
constexpr float kSigma = 7.28e-5f;      // surface tension over density, m^3/s^2
constexpr float kTwoPi = 6.2831853f;

float smoothstepf(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}

Vector2 catmull(Vector2 a, Vector2 b, Vector2 c, Vector2 d, float t) {
    const float t2 = t * t, t3 = t2 * t;
    auto f = [&](float p0, float p1, float p2, float p3) {
        return 0.5f * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
    };
    return { f(a.x, b.x, c.x, d.x), f(a.y, b.y, c.y, d.y) };
}

// A path smoothed and measured: points a few decimetres apart with their distance along the curve.
struct Course {
    std::vector<Vector2> p;
    std::vector<float> s;
    float total = 0;
    bool valid() const { return p.size() >= 2 && total > 1e-4f; }
    Vector2 at(float arc) const {
        arc = std::clamp(arc, 0.f, total);
        const size_t i = std::min((size_t)(std::upper_bound(s.begin(), s.end(), arc) - s.begin()), s.size() - 1);
        if (i == 0) return p[0];
        const float u = (arc - s[i - 1]) / std::max(s[i] - s[i - 1], 1e-6f);
        return { p[i - 1].x + (p[i].x - p[i - 1].x) * u, p[i - 1].y + (p[i].y - p[i - 1].y) * u };
    }
    // Unit heading at `arc`, from the curve half a metre either side (so it is smooth at the table's corners).
    Vector2 heading(float arc) const {
        const Vector2 a = at(std::max(arc - 0.6f, 0.f)), b = at(std::min(arc + 0.6f, total));
        const float l = std::hypot(b.x - a.x, b.y - a.y);
        return l > 1e-6f ? Vector2{ (b.x - a.x) / l, (b.y - a.y) / l } : Vector2{ 1, 0 };
    }
};

Course courseOf(const Wake& w) {
    Course c;
    const size_t n = w.path.size();
    if (n < 2) return c;
    c.p.push_back(w.path[0]);
    for (size_t i = 0; i + 1 < n; i++) {
        const Vector2 p0 = w.path[i > 0 ? i - 1 : 0], p1 = w.path[i], p2 = w.path[i + 1], p3 = w.path[std::min(i + 2, n - 1)];
        const float len = std::hypot(p2.x - p1.x, p2.y - p1.y);
        const int k = std::clamp((int)std::ceil(len / 0.4f), 2, 400);
        for (int j = 1; j <= k; j++) c.p.push_back(catmull(p0, p1, p2, p3, (float)j / (float)k));
    }
    c.s.assign(c.p.size(), 0.f);
    for (size_t i = 1; i < c.p.size(); i++) c.s[i] = c.s[i - 1] + std::hypot(c.p[i].x - c.p[i - 1].x, c.p[i].y - c.p[i - 1].y);
    c.total = c.s.back();
    return c;
}

// The age of emission point j (0 now, `life` for the last) and the span of time it stands for.
float sampleAge(int j, float life) {
    const float u = (float)j / (float)(Wake::kSamples - 1);
    return life * u * u;
}

} // namespace

// ------------------------------------------------------------------ Wake
float Wake::arcLength() const { return courseOf(*this).total; }

bool Wake::at(double seconds, Vector2* position, Vector2* heading) const {
    const Course c = courseOf(*this);
    if (!c.valid()) return false;
    const double run = seconds - start;
    if (run < 0) return false;
    const float arc = (float)std::min(run * speed, (double)c.total);
    if (position) *position = c.at(arc);
    if (heading) *heading = c.heading(arc);
    return true;
}

void Wake::emissions(double seconds, Vector4* out) const {
    const Course c = courseOf(*this);
    for (int j = 0; j < kSamples; j++) {
        out[j] = { 0, 0, 0, 0 };
        if (!c.valid()) continue;
        const double run = seconds - sampleAge(j, life) - start;
        if (run < 0 || run * speed > c.total + 1e-4) continue;
        const float arc = (float)std::min(run * speed, (double)c.total);
        const Vector2 p = c.at(arc), h = c.heading(arc);
        out[j] = { p.x, p.y, h.x, h.y };
    }
}

float Wake::amplitude() const {
    // Scaled by hull and Froude number F = U / sqrt(g L): a hull below F of about 0.4 makes little wave, one near hull
    // speed (F = 0.5 .. 0.9) makes the biggest, and the height saturates when it planes. The beam sets the size.
    const float F = speed / std::sqrt(kG * std::max(length, 0.5f)), f2 = F * F;
    return strength * 0.6f * beam * f2 / (0.2f + f2);
}

WakeField::WakeField(const Wake& wake, double seconds) {
    wake.emissions(seconds, table_);
    life_ = std::max(wake.life, 1.f);
    speed_ = wake.speed;
    beam_ = std::max(wake.beam, 0.05f);
    amp_ = wake.amplitude();
    present_ = wake.at(seconds, &pos_, &dir_);
}

// The sum of the rings of the last `life` seconds, windowed so that only the samples whose phase changes slowly
// from one sample to the next count (those near the stationary point of the emission time: the Kelvin crests).
float WakeField::sum(float x, float z, float foot, Vector2* slope) const {
    float h = 0;
    Vector2 g{ 0, 0 };
    const float eps = 0.5f * beam_ + 0.3f, eps2 = eps * eps;
    const float k0 = kG / (speed_ * speed_);
    // Twice as many emission points as the table has: a point halfway between two neighbours (their positions and
    // headings averaged), so the sum is smooth where the phase moves a radian or more from one table entry to the next.
    for (int jj = 0; jj < 2 * Wake::kSamples - 1; jj++) {
        const int j = jj / 2;
        Vector4 e = table_[j];
        if (e.z == 0 && e.w == 0) continue;
        if (jj & 1) {
            const Vector4 n = table_[j + 1];
            if (n.z == 0 && n.w == 0) continue;
            const float hx = e.z + n.z, hz = e.w + n.w, hl = std::max(std::sqrt(hx * hx + hz * hz), 1e-6f);
            e = { 0.5f * (e.x + n.x), 0.5f * (e.y + n.y), hx / hl, hz / hl };
        }
        const float pos = 0.5f * (float)jj, last = (float)(Wake::kSamples - 1);
        const float uu = pos / last, un = std::min(pos + 0.5f, last) / last, up = std::max(pos - 0.5f, 0.f) / last;
        const float T = life_ * uu * uu, dtau = 0.5f * life_ * (un * un - up * up);
        const float dx = x - e.x, dz = z - e.y;
        const float a = dx * e.z + dz * e.w;
        const float D2 = dx * dx + dz * dz + eps2, D = std::sqrt(D2);
        const float k = kG * T * T / (4 * D2), phase = k * D;
        const float dphi = kG * T * (speed_ * T * a - 2 * D2) / (4 * D2 * D);        // d phase / d emission time
        const float step = dphi * dtau;
        float c = amp_ * std::exp(-2.8f * step * step) * dtau / D;
        const float kb = k * beam_ / 12.f;
        c *= std::exp(-kb * kb) * std::exp(-3 * T / life_);
        c *= smoothstepf(0.6f * k0, 0.95f * k0, k);                                   // no waves longer than the transverse ones (2 pi U^2 / g)
        if (foot > 0) c *= smoothstepf(3.f, 12.f, kTwoPi / std::max(k * foot, 1e-6f));
        h += c * std::cos(phase);
        if (slope) {
            const float s = c * k * std::sin(phase) / D;
            g.x += s * dx;
            g.y += s * dz;
        }
    }
    if (slope) *slope = g;
    return h;
}

float Wake::height(float x, float z, double seconds, float foot) const { return WakeField(*this, seconds).height(x, z, foot); }
Vector2 Wake::slope(float x, float z, double seconds, float foot) const { return WakeField(*this, seconds).slope(x, z, foot); }

// ------------------------------------------------------------------ Ripple
namespace {

float omegaOf(float k) { return std::sqrt(kG * k + kSigma * k * k * k); }
float groupOf(float k) { return (kG + 3 * kSigma * k * k) / (2 * omegaOf(k)); }
// d (group speed) / dk.
float groupSlope(float k) {
    const float w = omegaOf(k), top = kG + 3 * kSigma * k * k;
    return 3 * kSigma * k / w - top * top / (4 * w * w * w);
}
// The wave number where the group speed is smallest (about 144 rad/m, 0.178 m/s): 3 s^2 k^4 + 6 s g k^2 - g^2 = 0.
const float kSlowest = std::sqrt(kG * (std::sqrt(48.f) - 6.f) / (6 * kSigma));

// The wave number of group speed v on the gravity branch (k below kSlowest, the group speed falling with k) or the
// capillary branch (above it, rising), by bisection in log k; 0 when there is none.
float rootOf(float v, bool capillary) {
    float lo = capillary ? kSlowest : 0.02f, hi = capillary ? 4000.f : kSlowest;
    if (!capillary && v <= groupOf(hi)) return 0;
    if (capillary && v <= groupOf(lo)) return 0;
    if (!capillary && v >= groupOf(lo)) return lo;
    if (capillary && v >= groupOf(hi)) return hi;
    for (int i = 0; i < 18; i++) {
        const float mid = std::sqrt(lo * hi);
        const bool above = groupOf(mid) > v;
        if (capillary ? above : !above) hi = mid; else lo = mid;
    }
    return std::sqrt(lo * hi);
}

} // namespace

float Ripple::groupSpeed(float k) { return groupOf(std::max(k, 1e-3f)); }

namespace {
// The ring's height (slope == false) or its slope along the radius (true), as in rippleField (wake_glsl.hpp).
float radial(const Ripple& r, float dist, double age, float foot, bool wantSlope) {
    if (age <= 0 || age > 3.0 * r.life) return 0;
    const float T = (float)age, a = std::max(r.size, 0.005f);
    const float D = std::sqrt(dist * dist + 0.25f * a * a);
    const float v = D / T;
    float acc = 0;
    for (int branch = 0; branch < 2; branch++) {
        const float k = rootOf(v, branch == 1);
        if (k <= 0) continue;
        const float S = std::exp(-0.5f * k * a * k * a);
        if (S < 1e-4f) continue;
        const float q = std::fabs(groupSlope(k)) * T + 0.5f * a * a;
        float amp = S * std::sqrt(k / (D * q));
        if (foot > 0) amp *= smoothstepf(3.f, 12.f, kTwoPi / std::max(k * foot, 1e-6f));
        const float phase = k * D - omegaOf(k) * T - 0.7853982f + (branch == 0 ? 0.7853982f : -0.7853982f);
        acc += wantSlope ? -k * amp * std::sin(phase) : amp * std::cos(phase);
    }
    const float fade = std::exp(-2 * T / std::max(r.life, 0.1f)) * smoothstepf(0.f, 0.15f, T);
    return r.strength * a * a * fade * acc;
}
} // namespace

float Ripple::height(float rr, double age, float foot) const { return radial(*this, rr, age, foot, false); }
float Ripple::slope(float rr, double age, float foot) const { return radial(*this, rr, age, foot, true); }
float Ripple::height(float px, float pz, double seconds, float foot) const {
    return radial(*this, std::hypot(px - x, pz - z), seconds - time, foot, false);
}
Vector2 Ripple::slope(float px, float pz, double seconds, float foot) const {
    const float dx = px - x, dz = pz - z, d = std::max(std::hypot(dx, dz), 1e-4f);
    const float s = radial(*this, d, seconds - time, foot, true);
    return { dx / d * s, dz / d * s };
}

// ------------------------------------------------------------------ the surface, and riding it
namespace {
constexpr size_t kMaxWakes = 4, kMaxRipples = 16;

// The scene's surface at one moment: the water under a point, and every wake (built once) and ripple on it.
struct SurfaceNow {
    const Scene& scene;
    double seconds;
    Water sea;
    std::vector<Water> bodies;
    std::vector<WakeField> wakes;
    explicit SurfaceNow(const Scene& s, double sec, int skipWake = -1) : scene(s), seconds(sec) {
        if (s.water.enabled) sea = waterOf(s);
        for (const WaterBody& b : s.waterBodies) bodies.push_back(waterOf(s, b));
        size_t n = 0;
        for (size_t i = 0; i < s.wakes.size() && n < kMaxWakes; i++, n++)
            if ((int)i != skipWake) wakes.emplace_back(s.wakes[i], sec);
    }
    // The water's own waves under the point: the first body that contains it, else the sea. Returns false for neither.
    bool water(float x, float z, float* h, Vector2* g) const {
        for (size_t i = 0; i < scene.waterBodies.size(); i++) {
            const WaterBody& b = scene.waterBodies[i];
            if (!(b.isRiver() || b.outline.size() >= 3) || !b.contains(x, z)) continue;
            Water w = bodies[i];
            w.level = b.levelAt(x, z);
            if (h) *h = w.height(x, z, seconds);
            if (g) *g = w.slope(x, z, seconds);
            return true;
        }
        if (scene.water.enabled) {
            if (h) *h = sea.height(x, z, seconds);
            if (g) *g = sea.slope(x, z, seconds);
            return true;
        }
        return false;
    }
    float height(float x, float z) const {
        float h = 0;
        water(x, z, &h, nullptr);
        for (const WakeField& w : wakes) h += w.height(x, z);
        size_t n = 0;
        for (const Ripple& r : scene.ripples) { if (n++ >= kMaxRipples) break; h += r.height(x, z, seconds, 0.f); }
        return h;
    }
    Vector2 slope(float x, float z) const {
        Vector2 g{ 0, 0 };
        water(x, z, nullptr, &g);
        for (const WakeField& w : wakes) { const Vector2 d = w.slope(x, z); g.x += d.x; g.y += d.y; }
        size_t n = 0;
        for (const Ripple& r : scene.ripples) { if (n++ >= kMaxRipples) break; const Vector2 d = r.slope(x, z, seconds, 0.f); g.x += d.x; g.y += d.y; }
        return g;
    }
};
} // namespace

float surfaceHeight(const Scene& s, float x, float z, double seconds) { return SurfaceNow(s, seconds).height(x, z); }
Vector2 surfaceSlope(const Scene& s, float x, float z, double seconds) { return SurfaceNow(s, seconds).slope(x, z); }

Vector2 ride(Object3D& o, const Scene& scene, double seconds, const Ride& how) {
    float heading = how.heading;
    Vector2 f;
    if (std::isnan(heading)) f = { std::sin(o.rotation.y * DEG2RAD), std::cos(o.rotation.y * DEG2RAD) };
    else f = { std::cos(heading * DEG2RAD), std::sin(heading * DEG2RAD) };
    const float fl = std::max(std::hypot(f.x, f.y), 1e-6f);
    f = { f.x / fl, f.y / fl };
    const Vector2 side{ -f.y, f.x };
    const int na = std::clamp(how.along, 2, 9), nc = std::clamp(how.across, 2, 9);
    // The footprint grid, centred on the object: heights at (u along, v across), then the least-squares plane.
    const SurfaceNow surf(scene, seconds, how.skipWake);
    double sum = 0, su = 0, sv = 0, uu = 0, vv = 0;
    const int n = na * nc;
    for (int i = 0; i < na; i++)
        for (int j = 0; j < nc; j++) {
            const float u = (((float)i + 0.5f) / (float)na - 0.5f) * how.length, v = (((float)j + 0.5f) / (float)nc - 0.5f) * how.beam;
            const float h = surf.height(o.position.x + f.x * u + side.x * v, o.position.z + f.y * u + side.y * v);
            sum += h; su += (double)u * h; sv += (double)v * h; uu += (double)u * u; vv += (double)v * v;
        }
    const float mean = (float)(sum / n);
    const float pitch = uu > 0 ? (float)(su / uu) : 0.f, roll = vv > 0 ? (float)(sv / vv) : 0.f;       // slopes along and across
    o.position.y = mean - how.draft;
    const float t = std::clamp(how.tilt, 0.f, 1.f);
    const Vector3 up = Vector3Normalize({ -(pitch * f.x + roll * side.x) * t, 1, -(pitch * f.y + roll * side.y) * t });
    const Vector3 forward{ f.x, pitch * t, f.y };
    o.rotation = lookRotation(forward, up);
    return f;
}

// ------------------------------------------------------------------ particles
fx::Emitter3D wakeSpray(const Wake& w, float fps) {
    fx::Emitter3D e;
    const float f = std::max(fps, 1.f);
    const float v = std::max(w.speed, 0.f);
    const Course c = courseOf(w);
    e.seed = 77;
    e.from = (int)std::floor(w.start * f);
    e.until = c.valid() ? (int)std::ceil((w.start + w.duration()) * f) : -1;
    const float q = std::clamp(v / 4.f, 0.f, 3.f);
    e.rate = 2.7f * q * q * w.foam * 30.f / f;                        // droplets a frame: the spray goes with speed squared
    e.life = { 0.35f * f, 0.8f * f };
    e.area = fx::Volume::Disc;
    e.extent = { 0.3f * std::max(w.beam, 0.2f), 0, 0 };
    const Wake copy = w;
    e.origin = [copy, f](float frame) {
        Vector2 p, h;
        if (!copy.at(frame / f, &p, &h)) return Vector3{ 0, -1000, 0 };
        const float bow = 0.5f * copy.length;
        return Vector3{ p.x + h.x * bow, copy.level + 0.08f, p.y + h.y * bow };
    };
    e.direction = { 0, 1, 0 };
    e.spread = 70;
    const float up = (0.6f + 0.35f * v) / f;                          // metres a frame
    e.speed = { 0.35f * up, up };
    e.gravity = { 0, -kG / (f * f), 0 };
    e.size = { 0.025f, 0.06f };
    e.sizeEnd = 0.5f;
    e.colors = { Color{ 238, 246, 250, 255 }, Color{ 255, 255, 255, 255 } };
    e.fadeIn = 0.05f;
    e.fadeOut = 0.5f;
    return e;
}

fx::Emitter3D splash(const Ripple& r, float fps) {
    fx::Emitter3D e;
    const float f = std::max(fps, 1.f);
    e.seed = 91;
    e.burst = 26;
    e.burstAt = (int)std::lround(r.time * f);
    e.burstSpread = 3;
    e.life = { 0.3f * f, 0.65f * f };
    e.area = fx::Volume::Disc;
    e.extent = { 0.4f * std::max(r.size, 0.01f), 0, 0 };
    const float x = r.x, y = r.level + 0.03f, z = r.z;
    e.origin = [x, y, z](float) { return Vector3{ x, y, z }; };
    e.direction = { 0, 1, 0 };
    e.spread = 55;
    const float up = (0.9f + 10.f * r.size) / f;
    e.speed = { 0.4f * up, up };
    e.gravity = { 0, -kG / (f * f), 0 };
    e.size = { 0.012f, 0.03f };
    e.sizeEnd = 0.5f;
    e.colors = { Color{ 235, 245, 250, 255 } };
    e.fadeIn = 0.05f;
    e.fadeOut = 0.5f;
    return e;
}

} // namespace g3d
} // namespace rmtn
