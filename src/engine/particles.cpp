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
void motion(const Emitter& e, const Seeded& s, float age, Vector2& pos, Vector2& vel) {
    const double d = std::clamp((double)e.drag, 0.0, 0.999);
    auto axis = [&](double p0, double v0, double g, float& p, float& v) {
        if (d <= 0) {
            v = (float)(v0 + g * age);
            p = (float)(p0 + v0 * age + 0.5 * g * age * age);
            return;
        }
        const double k = -std::log(1 - d), term = g / k, decay = std::exp(-k * age);
        v = (float)(term + (v0 - term) * decay);
        p = (float)(p0 + term * age + (v0 - term) * (1 - decay) / k);
    };
    axis(s.p0.x, s.v0.x, e.gravity.x, pos.x, vel.x);
    axis(s.p0.y, s.v0.y, e.gravity.y, pos.y, vel.y);
    if (e.turbulence > 0) {
        // Two sines per axis with random phases: smooth, and starting from no offset.
        const double f = e.turbulenceFreq, ramp = std::min(1.0, age * f);
        const double p1 = rnd(e, s.index, PH1) * 2 * PI, p2 = rnd(e, s.index, PH2) * 2 * PI;
        const double p3 = rnd(e, s.index, PH3) * 2 * PI, p4 = rnd(e, s.index, PH4) * 2 * PI;
        pos.x += (float)(e.turbulence * ramp * (0.7 * (std::sin(age * f + p1) - std::sin(p1)) +
                                                0.3 * (std::sin(age * f * 2.13 + p2) - std::sin(p2))));
        pos.y += (float)(e.turbulence * ramp * (0.7 * (std::sin(age * f * 0.87 + p3) - std::sin(p3)) +
                                                0.3 * (std::sin(age * f * 1.91 + p4) - std::sin(p4))));
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

Particle evaluate(const Emitter& e, const Seeded& s, float age) {
    Particle p;
    p.index = s.index;
    p.age = age;
    p.life = s.life;
    p.u = age / s.life;
    motion(e, s, age, p.pos, p.vel);
    p.size = pick(e.size, rnd(e, s.index, SIZE)) * (1 + (e.sizeEnd - 1) * p.u);
    p.rotation = pick(e.rotation, rnd(e, s.index, ROT)) + pick(e.spin, rnd(e, s.index, SPIN)) * age;
    if (!e.overLife.empty()) p.color = gradient(e.overLife, p.u);
    else if (!e.colors.empty())
        p.color = e.colors[std::min(e.colors.size() - 1, (size_t)(rnd(e, s.index, COLOR) * e.colors.size()))];
    const float in = e.fadeIn > 0 ? smooth(p.u / e.fadeIn) : 1.f, out = e.fadeOut > 0 ? smooth((1 - p.u) / e.fadeOut) : 1.f;
    p.alpha = in * out;
    p.random = (float)rnd(e, s.index, FREE);
    return p;
}

// The stream's first birth frame, and its birth frame for index k.
float streamStart(const Emitter& e) { return (float)e.from - (e.prewarm ? std::ceil(std::max(e.life.lo, e.life.hi)) : 0.f); }

template <class F>
void forEachSeeded(const Emitter& e, double t, F&& f) {
    const float maxLife = std::max(e.life.lo, e.life.hi);
    for (int i = 0; i < e.burst; i++) {
        const float birth = e.burstAt + (e.burstSpread > 0 ? (float)(rnd(e, i, BIRTH) * e.burstSpread) : 0.f);
        if (birth > t || t - birth > maxLife) continue;
        f(seedParticle(e, i, birth));
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
        const float birth = (float)(start + (k + rnd(e, index, BIRTH)) / rate);
        if (birth > t || (e.until >= 0 && birth >= e.until)) continue;
        f(seedParticle(e, index, birth));
    }
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

} } // rmtn::fx
