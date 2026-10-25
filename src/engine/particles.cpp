// particles.cpp -- closed-form particle emitters (see rmtn/particles.hpp).
#include "rmtn/particles.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/render.hpp"
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <memory>

namespace rmtn {
namespace fx {
namespace {

constexpr double DEG = PI / 180.0;

float pick(const Range& r, double u) { return (float)(r.lo + (r.hi - r.lo) * u); }

// Per-particle random slots. Each draw from the seed has its own slot, so changing one
// setting (say, the colours) leaves every other property of every particle unchanged.
enum Slot { BIRTH, LIFE, P1, P2, DIR, SPEED, SIZE, COLOR, ROT, SPIN, PH1, PH2, PH3, PH4, FREE, SLOTS };
double rnd(const Emitter& e, int index, Slot s) { return rnd01(e.seed, (unsigned long long)index * SLOTS + s); }

// What a particle is born with; its state at any age follows from these.
struct Seeded {
    int index;
    float birth, life;
    Vector2 p0, v0;
};

Seeded seedParticle(const Emitter& e, int index, float birth) {
    Seeded s;
    s.index = index;
    s.birth = birth;
    s.life = std::max(1.f, pick(e.life, rnd(e, index, LIFE)));
    const double a = rnd(e, index, P1), b = rnd(e, index, P2);
    Vector2 p{ 0, 0 };
    switch (e.area) {
        case Area::Point: break;
        case Area::Line: p = { (float)((a - 0.5) * e.extent.x), (float)((a - 0.5) * e.extent.y) }; break;
        case Area::Rect: p = { (float)((a - 0.5) * e.extent.x), (float)((b - 0.5) * e.extent.y) }; break;
        case Area::Circle: case Area::Ring: {
            const double r = e.area == Area::Ring ? e.extent.x : e.extent.x * std::sqrt(a), th = 2 * PI * b;
            p = { (float)(r * std::cos(th)), (float)(r * std::sin(th)) };
            break;
        }
    }
    double dir = e.angle;
    if (e.outward && (p.x != 0 || p.y != 0)) dir = std::atan2(p.y, p.x) / DEG;
    dir = (dir + (rnd(e, index, DIR) - 0.5) * e.spread) * DEG;
    const float v = pick(e.speed, rnd(e, index, SPEED));
    s.v0 = { (float)(std::cos(dir) * v), (float)(std::sin(dir) * v) };
    if (e.origin) {
        const Vector2 o = e.origin(birth);
        p.x += o.x; p.y += o.y;
    }
    s.p0 = p;
    return s;
}

// Exact motion under constant gravity g and drag: dv/dt = g - k v with k = -ln(1 - drag),
// so the velocity relaxes towards the terminal velocity g / k.
void axisMotion(double drag, double p0, double v0, double g, double age, float& p, float& v) {
    const double d = std::clamp(drag, 0.0, 0.999);
    if (d <= 0) {
        v = (float)(v0 + g * age);
        p = (float)(p0 + v0 * age + 0.5 * g * age * age);
        return;
    }
    const double k = -std::log(1 - d), term = g / k, decay = std::exp(-k * age);
    v = (float)(term + (v0 - term) * decay);
    p = (float)(p0 + term * age + (v0 - term) * (1 - decay) / k);
}

// Two sines with random phases: smooth, and starting from no offset.
double wander(double age, double f, double a, double b, double pa, double pb) {
    return 0.7 * (std::sin(age * f * a + pa) - std::sin(pa)) + 0.3 * (std::sin(age * f * b + pb) - std::sin(pb));
}

void motion(const Emitter& e, const Seeded& s, float age, Vector2& pos, Vector2& vel) {
    axisMotion(e.drag, s.p0.x, s.v0.x, e.gravity.x, age, pos.x, vel.x);
    axisMotion(e.drag, s.p0.y, s.v0.y, e.gravity.y, age, pos.y, vel.y);
    if (e.turbulence > 0) {
        const double f = e.turbulenceFreq, ramp = std::min(1.0, age * f);
        const double p1 = rnd(e, s.index, PH1) * 2 * PI, p2 = rnd(e, s.index, PH2) * 2 * PI;
        const double p3 = rnd(e, s.index, PH3) * 2 * PI, p4 = rnd(e, s.index, PH4) * 2 * PI;
        pos.x += (float)(e.turbulence * ramp * wander(age, f, 1, 2.13, p1, p2));
        pos.y += (float)(e.turbulence * ramp * wander(age, f, 0.87, 1.91, p3, p4));
    }
}

Color gradient(const std::vector<Color>& stops, double u) {
    if (stops.size() == 1) return stops[0];
    const double x = std::clamp(u, 0.0, 1.0) * (stops.size() - 1);
    const size_t i = std::min((size_t)x, stops.size() - 2);
    return mixC(stops[i], stops[i + 1], x - i);
}

float smooth(float x) {
    x = std::clamp(x, 0.f, 1.f);
    return x * x * (3 - 2 * x);
}

template <class E> float fade(const E& e, float u) {
    const float in = e.fadeIn > 0 ? smooth(u / e.fadeIn) : 1.f, out = e.fadeOut > 0 ? smooth((1 - u) / e.fadeOut) : 1.f;
    return in * out;
}

template <class E> Color colorOf(const E& e, double u, double pick01) {
    if (!e.overLife.empty()) return gradient(e.overLife, u);
    if (!e.colors.empty()) return e.colors[std::min(e.colors.size() - 1, (size_t)(pick01 * e.colors.size()))];
    return WHITE;
}

Particle evaluate(const Emitter& e, const Seeded& s, float age) {
    Particle p;
    p.index = s.index;
    p.age = age;
    p.life = s.life;
    p.u = age / s.life;
    motion(e, s, age, p.pos, p.vel);
    p.size = pick(e.size, rnd(e, s.index, SIZE)) * (1 + (e.sizeEnd - 1) * p.u);
    p.rotation = pick(e.rotation, rnd(e, s.index, ROT)) + pick(e.spin, rnd(e, s.index, SPIN)) * age;
    p.color = colorOf(e, p.u, rnd(e, s.index, COLOR));
    p.alpha = fade(e, p.u);
    p.random = (float)rnd(e, s.index, FREE);
    return p;
}

// The stream's first birth frame, and its birth frame for index k.
template <class E> float streamStart(const E& e) { return (float)e.from - (e.prewarm ? std::ceil(std::max(e.life.lo, e.life.hi)) : 0.f); }

// Calls f(index, birth) for every particle that may be alive at t, for either emitter;
// `jitter(index)` is that particle's random birth offset in [0, 1).
template <class E, class J, class F>
void forEachBirth(const E& e, double t, J&& jitter, F&& f) {
    const float maxLife = std::max(e.life.lo, e.life.hi);
    for (int i = 0; i < e.burst; i++) {
        const float birth = e.burstAt + (e.burstSpread > 0 ? (float)(jitter(i) * e.burstSpread) : 0.f);
        if (birth > t || t - birth > maxLife) continue;
        f(i, birth);
    }
    if (e.rate <= 0) return;
    const double start = streamStart(e), rate = e.rate;
    long long kHi = (long long)std::floor((t - start) * rate);
    if (e.until >= 0) kHi = std::min(kHi, (long long)std::ceil((e.until - start) * rate) - 1);
    long long kLo = std::max(0LL, (long long)std::floor((t - maxLife - start) * rate) - 1);
    if (kHi - kLo >= e.maxParticles) kLo = kHi - e.maxParticles + 1;
    for (long long k = kLo; k <= kHi; k++) {
        const int index = e.burst + (int)k;
        // Births are evenly spaced with a jitter of up to one spacing, so the stream is steady
        // without looking mechanical.
        const float birth = (float)(start + (k + jitter(index)) / rate);
        if (birth > t || (e.until >= 0 && birth >= e.until)) continue;
        f(index, birth);
    }
}

template <class F>
void forEachSeeded(const Emitter& e, double t, F&& f) {
    forEachBirth(e, t, [&](int i) { return rnd(e, i, BIRTH); }, [&](int i, float birth) { f(seedParticle(e, i, birth)); });
}

void drawOne(const Ctx& c, const Emitter& e, const Seeded& s, const Particle& p) {
    const Color col = withAlpha(p.color, p.alpha);
    const Color k = dr::TA(c, col);     // for raylib calls that bypass dr::
    const float r = p.size / 2;
    switch (e.look) {
        case Look::Dot: dr::circle(c, p.pos, r, col); break;
        case Look::Glow: {
            const Vector2 m = mapP(c, p.pos);
            DrawCircleGradient(m, r * c.s, k, Color{ k.r, k.g, k.b, 0 });
            if (e.additive) {   // a hot centre, fading into the halo
                const Color hot = mixC(k, Color{ 255, 255, 255, k.a }, 0.6);
                DrawCircleGradient(m, r * c.s * 0.35f, hot, Color{ hot.r, hot.g, hot.b, 0 });
            }
            break;
        }
        case Look::Streak: {
            Vector2 tail, v;
            motion(e, s, std::max(0.f, p.age - e.stretch), tail, v);
            const Vector2 a = mapP(c, tail), b = mapP(c, p.pos);
            float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy);
            const float w = r * c.s;
            if (len > 1e-3f) {
                const float nx = -dy / len * w, ny = dx / len * w;
                DrawTriangleGradient(a, { b.x + nx, b.y + ny }, { b.x - nx, b.y - ny }, Color{ k.r, k.g, k.b, 0 }, k, k);
            }
            DrawCircleV(b, w, k);
            break;
        }
        case Look::Square: case Look::Confetti: {
            float hw = r, hh = r;
            Color fill = col;
            if (e.look == Look::Confetti) {
                // The paper turns over as it falls: its width follows a cosine, and its
                // back is a little darker.
                const float flip = std::cos((float)(p.rotation * 1.7 * DEG + p.random * 2 * PI));
                hw = r * std::max(0.08f, std::fabs(flip));
                hh = r * 0.55f;
                fill = mixC(col, Color{ 0, 0, 0, col.a }, flip < 0 ? 0.22 : 0.0);
            }
            const float ca = std::cos((float)(p.rotation * DEG)), sa = std::sin((float)(p.rotation * DEG));
            auto corner = [&](float x, float y) { return Vector2{ p.pos.x + x * ca - y * sa, p.pos.y + x * sa + y * ca }; };
            dr::quad(c, corner(-hw, -hh), corner(hw, -hh), corner(hw, hh), corner(-hw, hh), fill);
            break;
        }
        case Look::Star: dr::star(c, p.pos, r, r * 0.42f, 5, p.rotation, col); break;
        case Look::Ring: dr::ring(c, p.pos, r, std::max(1.f, p.size * 0.12f), col); break;
    }
}

} // namespace

void each(const Emitter& e, double t, const std::function<void(const Particle&)>& f) {
    forEachSeeded(e, t, [&](const Seeded& s) {
        const float age = (float)(t - s.birth);
        if (age < s.life) f(evaluate(e, s, age));
    });
}

std::vector<Particle> particles(const Emitter& e, double t) {
    std::vector<Particle> out;
    each(e, t, [&](const Particle& p) { out.push_back(p); });
    return out;
}

void draw(const Ctx& c, const Emitter& e, double t) {
    if (e.additive) {
        // Colour adds up; alpha composites as usual, so the layer stays well-formed.
        rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    }
    forEachSeeded(e, t, [&](const Seeded& s) {
        const float age = (float)(t - s.birth);
        if (age >= s.life) return;
        const Particle p = evaluate(e, s, age);
        if (p.alpha * p.color.a < 0.5f || p.size <= 0) return;
        if (e.draw) e.draw(c, p);
        else drawOne(c, e, s, p);
    });
    if (e.additive) {
        if (canvasSize().x > 0) restoreBlend();
        else EndBlendMode();
    }
}

std::function<void(const Ctx& c, int t)> fn(Emitter e) {
    auto sp = std::make_shared<const Emitter>(std::move(e));
    return [sp](const Ctx& c, int t) { draw(c, *sp, frameTime(t)); };
}

// ------------------------------------------------------------------ presets

Emitter burst(int count, std::vector<Color> colors) {
    Emitter e;
    e.burst = count;
    e.burstSpread = 2;
    e.life = { 40, 75 };
    e.speed = { 3, 12 };
    e.drag = 0.045f;
    e.gravity = { 0, 0.05f };
    e.look = Look::Streak;
    e.stretch = 4;
    e.size = { 2.5f, 4.5f };
    e.sizeEnd = 0.4f;
    e.colors = std::move(colors);
    e.fadeIn = 0;
    e.fadeOut = 0.5f;
    e.additive = true;
    return e;
}

Emitter sparks(float rate, Color color) {
    Emitter e;
    e.rate = rate;
    e.life = { 12, 26 };
    e.speed = { 6, 16 };
    e.spread = 70;
    e.gravity = { 0, 0.45f };
    e.drag = 0.02f;
    e.look = Look::Streak;
    e.stretch = 2.5f;
    e.size = { 1.5f, 3 };
    e.colors = { color, mixC(color, WHITE, 0.6) };
    e.fadeIn = 0;
    e.fadeOut = 0.4f;
    e.additive = true;
    return e;
}

Emitter fountain(float rate, Color color) {
    Emitter e;
    e.rate = rate;
    e.life = { 55, 70 };
    e.speed = { 10, 12.5f };
    e.spread = 12;
    e.gravity = { 0, 0.33f };
    e.size = { 4, 9 };
    e.sizeEnd = 0.6f;
    e.colors = { withAlpha(color, 0.85f), withAlpha(mixC(color, WHITE, 0.5), 0.85f) };
    e.fadeIn = 0.02f;
    e.fadeOut = 0.25f;
    return e;
}

Emitter confetti(float width, std::vector<Color> colors) {
    Emitter e;
    e.rate = 1.6f * width / 1920;
    e.area = Area::Line;
    e.extent = { width, 0 };
    e.life = { 230, 300 };
    e.speed = { 1, 3 };
    e.angle = 90;
    e.spread = 40;
    e.gravity = { 0, 0.15f };
    e.drag = 0.03f;
    e.turbulence = 40;
    e.turbulenceFreq = 0.05f;
    e.look = Look::Confetti;
    e.size = { 12, 20 };
    e.spin = { -8, 8 };
    e.colors = std::move(colors);
    e.fadeIn = 0;
    e.fadeOut = 0.1f;
    return e;
}

Emitter rain(float width, float height) {
    Emitter e;
    e.rate = 6 * width / 1920;
    e.prewarm = true;
    e.area = Area::Line;
    e.extent = { width * 1.3f, 0 };
    e.speed = { 22, 28 };
    e.angle = 100;
    e.spread = 3;
    e.life = { height / 25, height / 22 };
    e.look = Look::Streak;
    e.stretch = 1.4f;
    e.size = { 1.2f, 2.2f };
    e.colors = { { 170, 190, 230, 150 } };
    e.fadeIn = 0;
    e.fadeOut = 0.05f;
    return e;
}

Emitter snow(float width, float height) {
    Emitter e;
    e.rate = 1.2f * width / 1920;
    e.prewarm = true;
    e.area = Area::Line;
    e.extent = { width * 1.1f, 0 };
    e.speed = { 0.8f, 1.8f };
    e.angle = 90;
    e.spread = 30;
    e.turbulence = 30;
    e.turbulenceFreq = 0.03f;
    e.life = { height / 1.4f, height / 1.1f };
    e.size = { 3, 8 };
    e.colors = { { 255, 255, 255, 220 } };
    e.fadeIn = 0.02f;
    e.fadeOut = 0.05f;
    return e;
}

Emitter fire(float width) {
    Emitter e;
    e.rate = 3;
    e.area = Area::Line;
    e.extent = { width, 0 };
    e.life = { 22, 40 };
    e.speed = { 1.5f, 3.5f };
    e.spread = 16;
    e.gravity = { 0, -0.16f };
    e.turbulence = 10;
    e.turbulenceFreq = 0.15f;
    e.look = Look::Glow;
    e.size = { width * 0.35f, width * 0.6f };
    e.sizeEnd = 0.25f;
    e.overLife = { { 255, 230, 160, 150 }, { 255, 140, 40, 140 }, { 210, 50, 20, 110 }, { 80, 20, 10, 0 } };
    e.fadeIn = 0.1f;
    e.fadeOut = 0.5f;
    e.additive = true;
    return e;
}

Emitter smoke(float width) {
    Emitter e;
    e.rate = 0.8f;
    e.area = Area::Line;
    e.extent = { width, 0 };
    e.life = { 100, 150 };
    e.speed = { 1.5f, 2.5f };
    e.spread = 30;
    e.gravity = { 0, -0.015f };
    e.turbulence = 30;
    e.turbulenceFreq = 0.04f;
    e.look = Look::Glow;
    e.size = { width * 0.6f, width * 0.9f };
    e.sizeEnd = 2.4f;
    e.colors = { { 120, 120, 130, 70 } };
    e.fadeIn = 0.15f;
    e.fadeOut = 0.6f;
    return e;
}

Emitter embers(float width, float height) {
    Emitter e;
    e.rate = 0.5f * (width * height) / (1920.f * 1080.f);
    e.prewarm = true;
    e.area = Area::Rect;
    e.extent = { width, height };
    e.life = { 120, 240 };
    e.speed = { 0.2f, 0.8f };
    e.spread = 60;
    e.turbulence = 25;
    e.turbulenceFreq = 0.04f;
    e.look = Look::Glow;
    e.size = { 6, 16 };
    e.colors = { { 255, 170, 80, 255 }, { 255, 120, 60, 255 } };
    e.fadeIn = 0.3f;
    e.fadeOut = 0.4f;
    e.additive = true;
    return e;
}

// ------------------------------------------------------------------ 3D
namespace {

// The 3D slots are their own table, so 2D emitters keep every random value they had.
enum Slot3 { B3, L3, Q1, Q2, Q3, D1, D2, S3, SZ3, C3, R3, SP3, A1, A2, T1, T2, T3, T4, T5, T6, F3, SLOTS3 };
double rnd(const Emitter3D& e, int index, Slot3 s) { return rnd01(e.seed, (unsigned long long)index * SLOTS3 + s); }

struct Seeded3 {
    int index;
    float birth, life;
    Vector3 p0, v0;
};

Vector3 v3(double x, double y, double z) { return { (float)x, (float)y, (float)z }; }
Vector3 unit(Vector3 v, Vector3 fallback) {
    const float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return l > 1e-6f ? Vector3{ v.x / l, v.y / l, v.z / l } : fallback;
}
// A uniformly distributed direction from two uniform numbers.
Vector3 sphereDir(double a, double b) {
    const double z = 2 * a - 1, r = std::sqrt(std::max(0.0, 1 - z * z)), th = 2 * PI * b;
    return v3(r * std::cos(th), z, r * std::sin(th));
}
// A direction in the cone of full angle `spread` degrees around `axis`, uniform over its cap.
Vector3 coneDir(Vector3 axis, float spread, double a, double b) {
    const double half = std::clamp((double)spread, 0.0, 360.0) * 0.5 * DEG;
    const double ct = 1 - a * (1 - std::cos(half)), st = std::sqrt(std::max(0.0, 1 - ct * ct)), ph = 2 * PI * b;
    const Vector3 ref = std::fabs(axis.y) < 0.9f ? Vector3{ 0, 1, 0 } : Vector3{ 1, 0, 0 };
    Vector3 u{ axis.y * ref.z - axis.z * ref.y, axis.z * ref.x - axis.x * ref.z, axis.x * ref.y - axis.y * ref.x };
    u = unit(u, { 1, 0, 0 });
    const Vector3 w{ axis.y * u.z - axis.z * u.y, axis.z * u.x - axis.x * u.z, axis.x * u.y - axis.y * u.x };
    const double cu = st * std::cos(ph), cw = st * std::sin(ph);
    return v3(axis.x * ct + u.x * cu + w.x * cw, axis.y * ct + u.y * cu + w.y * cw, axis.z * ct + u.z * cu + w.z * cw);
}

Seeded3 seedParticle(const Emitter3D& e, int index, float birth) {
    Seeded3 s;
    s.index = index;
    s.birth = birth;
    s.life = std::max(1.f, pick(e.life, rnd(e, index, L3)));
    const double a = rnd(e, index, Q1), b = rnd(e, index, Q2), c = rnd(e, index, Q3);
    const Vector3 x = e.extent;
    Vector3 p{ 0, 0, 0 };
    switch (e.area) {
        case Volume::Point: break;
        case Volume::Line: p = v3((a - 0.5) * x.x, (a - 0.5) * x.y, (a - 0.5) * x.z); break;
        case Volume::Box: p = v3((a - 0.5) * x.x, (b - 0.5) * x.y, (c - 0.5) * x.z); break;
        case Volume::Sphere: case Volume::Shell: {
            const Vector3 d = sphereDir(b, c);
            const double r = e.area == Volume::Shell ? x.x : x.x * std::cbrt(a);
            p = v3(d.x * r, d.y * r, d.z * r);
            break;
        }
        case Volume::Disc: case Volume::Ring: {
            const double r = e.area == Volume::Ring ? x.x : x.x * std::sqrt(a), th = 2 * PI * b;
            p = v3(r * std::cos(th), 0, r * std::sin(th));
            break;
        }
    }
    Vector3 axis = unit(e.direction, { 0, 1, 0 });
    if (e.outward) axis = unit(p, axis);
    const Vector3 d = coneDir(axis, e.spread, rnd(e, index, D1), rnd(e, index, D2));
    const float v = pick(e.speed, rnd(e, index, S3));
    s.v0 = { d.x * v, d.y * v, d.z * v };
    if (e.origin) {
        const Vector3 o = e.origin(birth);
        p.x += o.x; p.y += o.y; p.z += o.z;
    }
    s.p0 = p;
    return s;
}

// Free flight (gravity, drag, turbulence) at `age`, ignoring the floor.
void flight(const Emitter3D& e, const Seeded3& s, double age, Vector3& pos, Vector3& vel) {
    axisMotion(e.drag, s.p0.x, s.v0.x, e.gravity.x, age, pos.x, vel.x);
    axisMotion(e.drag, s.p0.y, s.v0.y, e.gravity.y, age, pos.y, vel.y);
    axisMotion(e.drag, s.p0.z, s.v0.z, e.gravity.z, age, pos.z, vel.z);
    if (e.turbulence > 0) {
        const double f = e.turbulenceFreq, ramp = std::min(1.0, age * f), k = e.turbulence * ramp;
        auto ph = [&](Slot3 sl) { return rnd(e, s.index, sl) * 2 * PI; };
        pos.x += (float)(k * wander(age, f, 1, 2.13, ph(T1), ph(T2)));
        pos.y += (float)(k * wander(age, f, 0.87, 1.91, ph(T3), ph(T4)));
        pos.z += (float)(k * wander(age, f, 0.93, 2.37, ph(T5), ph(T6)));
    }
}

// With a floor: the age at which the particle first drops below it (searched in steps,
// then bisected), or `age` if it has not landed by then.
double landing(const Emitter3D& e, const Seeded3& s, double age) {
    if (!e.floor) return age;
    Vector3 p, v;
    auto below = [&](double a) { flight(e, s, a, p, v); return p.y < e.floorY; };
    const double step = std::max(1.0, s.life / 64.0);
    for (double hi = step, lo = 0; lo < age; lo = hi, hi += step) {
        hi = std::min(hi, age);
        if (!below(hi)) continue;
        for (int i = 0; i < 24; i++) {
            const double mid = (lo + hi) / 2;
            (below(mid) ? hi : lo) = mid;
        }
        return lo;
    }
    return age;
}

void motion(const Emitter3D& e, const Seeded3& s, double age, Vector3& pos, Vector3& vel) {
    const double land = landing(e, s, age);
    flight(e, s, land, pos, vel);
    if (land < age) {                    // resting on the floor
        pos.y = e.floorY;
        vel = { 0, 0, 0 };
    }
}

Particle3D evaluate(const Emitter3D& e, const Seeded3& s, float age) {
    Particle3D p;
    p.index = s.index;
    p.birth = s.birth;
    p.age = age;
    p.life = s.life;
    p.u = age / s.life;
    motion(e, s, age, p.pos, p.vel);
    p.size = pick(e.size, rnd(e, s.index, SZ3)) * (1 + (e.sizeEnd - 1) * p.u);
    const double turning = e.floor ? landing(e, s, age) : age;     // a landed chunk stops spinning
    p.rotation = (float)(pick(e.rotation, rnd(e, s.index, R3)) + pick(e.spin, rnd(e, s.index, SP3)) * turning);
    p.axis = sphereDir(rnd(e, s.index, A1), rnd(e, s.index, A2));
    p.color = colorOf(e, p.u, rnd(e, s.index, C3));
    p.alpha = fade(e, p.u);
    p.random = (float)rnd(e, s.index, F3);
    return p;
}

} // namespace

void each(const Emitter3D& e, double t, const std::function<void(const Particle3D&)>& f) {
    forEachBirth(e, t, [&](int i) { return rnd(e, i, B3); }, [&](int i, float birth) {
        const Seeded3 s = seedParticle(e, i, birth);
        const float age = (float)(t - birth);
        if (age < s.life) f(evaluate(e, s, age));
    });
}

std::vector<Particle3D> particles(const Emitter3D& e, double t) {
    std::vector<Particle3D> out;
    each(e, t, [&](const Particle3D& p) { out.push_back(p); });
    return out;
}

Vector3 positionAt(const Emitter3D& e, const Particle3D& p, float ago) {
    Vector3 pos, vel;
    motion(e, seedParticle(e, p.index, p.birth), std::max(0.f, p.age - ago), pos, vel);
    return pos;
}

// ---- 3D presets

Emitter3D burst3d(int count, std::vector<Color> colors) {
    Emitter3D e;
    e.burst = count;
    e.burstSpread = 2;
    e.life = { 40, 75 };
    e.speed = { 0.04f, 0.12f };
    e.drag = 0.045f;
    e.gravity = { 0, -0.0006f, 0 };
    e.look = Look::Streak;
    e.stretch = 4;
    e.size = { 0.03f, 0.05f };
    e.sizeEnd = 0.4f;
    e.colors = std::move(colors);
    e.fadeIn = 0;
    e.fadeOut = 0.5f;
    e.additive = true;
    return e;
}

Emitter3D sparks3d(float rate, Color color) {
    Emitter3D e;
    e.rate = rate;
    e.life = { 12, 26 };
    e.speed = { 0.08f, 0.2f };
    e.spread = 70;
    e.gravity = { 0, -0.006f, 0 };
    e.drag = 0.02f;
    e.look = Look::Streak;
    e.stretch = 2.5f;
    e.size = { 0.012f, 0.025f };
    e.colors = { color, mixC(color, WHITE, 0.6) };
    e.fadeIn = 0;
    e.fadeOut = 0.4f;
    e.additive = true;
    return e;
}

Emitter3D fountain3d(float rate, Color color) {
    Emitter3D e;
    e.rate = rate;
    e.life = { 55, 70 };
    e.speed = { 0.1f, 0.115f };
    e.spread = 12;
    e.gravity = { 0, -0.0035f, 0 };
    e.size = { 0.03f, 0.06f };
    e.sizeEnd = 0.6f;
    e.colors = { withAlpha(color, 0.85f), withAlpha(mixC(color, WHITE, 0.5), 0.85f) };
    e.fadeIn = 0.02f;
    e.fadeOut = 0.25f;
    return e;
}

Emitter3D confetti3d(float width, float depth, std::vector<Color> colors) {
    Emitter3D e;
    e.rate = 1.6f * width * depth / 36;
    e.area = Volume::Box;
    e.extent = { width, 0, depth };
    e.life = { 150, 220 };
    e.speed = { 0.005f, 0.015f };
    e.direction = { 0, -1, 0 };
    e.spread = 40;
    e.gravity = { 0, -0.001f, 0 };
    e.drag = 0.04f;
    e.turbulence = 0.4f;
    e.turbulenceFreq = 0.05f;
    e.look = Look::Confetti;
    e.size = { 0.08f, 0.13f };
    e.spin = { -8, 8 };
    e.colors = std::move(colors);
    e.fadeIn = 0;
    e.fadeOut = 0.1f;
    return e;
}

Emitter3D snow3d(float width, float depth, float height) {
    Emitter3D e;
    e.rate = 0.04f * width * depth;
    e.prewarm = true;
    e.area = Volume::Box;
    e.extent = { width, 0, depth };
    e.speed = { 0.008f, 0.016f };
    e.direction = { 0, -1, 0 };
    e.spread = 30;
    e.turbulence = 0.25f;
    e.turbulenceFreq = 0.03f;
    e.life = { height / 0.016f, height / 0.011f };
    e.size = { 0.03f, 0.07f };
    e.colors = { { 255, 255, 255, 220 } };
    e.fadeIn = 0.02f;
    e.fadeOut = 0.05f;
    return e;
}

Emitter3D rain3d(float width, float depth, float height) {
    Emitter3D e;
    e.rate = 0.25f * width * depth;
    e.prewarm = true;
    e.area = Volume::Box;
    e.extent = { width, 0, depth };
    e.speed = { 0.28f, 0.34f };
    e.direction = { 0.05f, -1, 0 };
    e.spread = 3;
    e.life = { height / 0.34f, height / 0.3f };
    e.look = Look::Streak;
    e.stretch = 1.4f;
    e.size = { 0.008f, 0.014f };
    e.colors = { { 170, 190, 230, 150 } };
    e.fadeIn = 0;
    e.fadeOut = 0.05f;
    return e;
}

Emitter3D fire3d(float radius) {
    Emitter3D e;
    e.rate = 4;
    e.area = Volume::Disc;
    e.extent = { radius, 0, 0 };
    e.life = { 22, 40 };
    e.speed = { 0.015f, 0.035f };
    e.spread = 16;
    e.gravity = { 0, 0.0016f, 0 };
    e.turbulence = radius * 0.4f;
    e.turbulenceFreq = 0.15f;
    e.look = Look::Glow;
    e.size = { radius * 1.4f, radius * 2.4f };
    e.sizeEnd = 0.25f;
    e.overLife = { { 255, 230, 160, 150 }, { 255, 140, 40, 140 }, { 210, 50, 20, 110 }, { 80, 20, 10, 0 } };
    e.fadeIn = 0.1f;
    e.fadeOut = 0.5f;
    e.additive = true;
    return e;
}

Emitter3D smoke3d(float radius) {
    Emitter3D e;
    e.rate = 0.8f;
    e.area = Volume::Disc;
    e.extent = { radius, 0, 0 };
    e.life = { 100, 150 };
    e.speed = { 0.01f, 0.02f };
    e.spread = 30;
    e.gravity = { 0, 0.00015f, 0 };
    e.turbulence = radius * 1.5f;
    e.turbulenceFreq = 0.04f;
    e.look = Look::Glow;
    e.size = { radius * 2.4f, radius * 3.6f };
    e.sizeEnd = 2.4f;
    e.colors = { { 120, 120, 130, 70 } };
    e.fadeIn = 0.15f;
    e.fadeOut = 0.6f;
    return e;
}

Emitter3D embers3d(float radius) {
    Emitter3D e;
    e.rate = 0.6f;
    e.area = Volume::Disc;
    e.extent = { radius, 0, 0 };
    e.life = { 60, 120 };
    e.speed = { 0.01f, 0.03f };
    e.spread = 50;
    e.gravity = { 0, 0.0003f, 0 };
    e.turbulence = 0.15f;
    e.turbulenceFreq = 0.06f;
    e.look = Look::Glow;
    e.size = { 0.02f, 0.05f };
    e.colors = { { 255, 170, 80, 255 }, { 255, 120, 60, 255 } };
    e.fadeIn = 0.2f;
    e.fadeOut = 0.4f;
    e.additive = true;
    return e;
}

Emitter3D debris3d(int count) {
    Emitter3D e;
    e.burst = count;
    e.burstSpread = 1;
    e.life = { 150, 150 };
    e.speed = { 0.06f, 0.14f };
    e.spread = 100;
    e.gravity = { 0, -0.006f, 0 };
    e.floor = true;
    e.size = { 0.06f, 0.14f };
    e.spin = { -12, 12 };
    e.look = Look::Square;
    e.fadeIn = 0;
    e.fadeOut = 0.15f;
    return e;
}

} } // rmtn::fx
