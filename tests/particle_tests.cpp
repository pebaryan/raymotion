// particle_tests.cpp -- fx:: emitters: counts, birth windows, prewarm, determinism,
// closed-form motion against a stepped simulation, and the presets. Pure logic (no GL).
#include "rmtn/particles.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

static void bursts() {
    fx::Emitter e;
    e.burst = 50;
    e.burstAt = 10;
    e.life = { 20, 20 };
    CHECK(fx::particles(e, 9).empty());
    CHECK(fx::particles(e, 10).size() == 50);
    CHECK(fx::particles(e, 29).size() == 50);
    CHECK(fx::particles(e, 30).empty());             // age 20 == life: gone
    e.burstSpread = 10;                              // births spread over 10..20
    const size_t early = fx::particles(e, 12).size();
    CHECK(early > 5 && early < 45);
    CHECK(fx::particles(e, 20).size() == 50);
    for (const fx::Particle& p : fx::particles(e, 15)) CHECK(p.age >= 0 && p.age < p.life && p.u >= 0 && p.u < 1);
}

static void streams() {
    fx::Emitter e;
    e.rate = 2;                                      // two per frame
    e.life = { 30, 30 };
    CHECK(fx::particles(e, 0).size() <= 2);
    const size_t steady = fx::particles(e, 100).size();
    CHECK(steady >= 58 && steady <= 61);             // rate * life
    e.until = 50;
    CHECK(fx::particles(e, 79).size() <= 2);         // the last births fade out by 80
    CHECK(fx::particles(e, 81).empty());
    e.until = -1;
    e.from = 40;
    e.prewarm = true;
    const size_t warm = fx::particles(e, 40).size();
    CHECK(warm >= 58 && warm <= 61);                 // full at the first frame
    e.prewarm = false;
    CHECK(fx::particles(e, 39).empty());
    e.rate = 1000;
    e.maxParticles = 500;
    CHECK(fx::particles(e, 200).size() <= 500);
    // Indices are unique and the stream comes after the burst.
    fx::Emitter m;
    m.burst = 5;
    m.rate = 1;
    m.life = { 40, 40 };
    auto ps = fx::particles(m, 20);
    for (size_t i = 0; i < ps.size(); i++)
        for (size_t j = i + 1; j < ps.size(); j++) CHECK(ps[i].index != ps[j].index);
    CHECK(ps.size() > 5 && ps[0].index == 0 && ps[5].index >= 5);
}

static void determinism() {
    fx::Emitter e = fx::burst(40);
    e.rate = 1.5f;
    e.turbulence = 20;
    const auto a = fx::particles(e, 37);
    fx::particles(e, 5);                             // other frames in between change nothing
    fx::particles(e, 90);
    const auto b = fx::particles(e, 37);
    CHECK(a.size() == b.size());
    for (size_t i = 0; i < a.size() && i < b.size(); i++)
        CHECK(a[i].index == b[i].index && a[i].pos.x == b[i].pos.x && a[i].pos.y == b[i].pos.y && a[i].size == b[i].size);
    // Changing the colours does not move anything.
    fx::Emitter c = e;
    c.colors = { RED, GREEN, BLUE };
    const auto d = fx::particles(c, 37);
    for (size_t i = 0; i < a.size() && i < d.size(); i++) CHECK(a[i].pos.x == d[i].pos.x && a[i].pos.y == d[i].pos.y);
    // A different seed does.
    fx::Emitter s = e;
    s.seed = 99;
    const auto q = fx::particles(s, 37);
    int same = 0;
    for (size_t i = 0; i < a.size() && i < q.size(); i++) same += a[i].pos.x == q[i].pos.x;
    CHECK(same < 3);
}

// The closed form against a finely stepped integration of dv/dt = g - k v.
static void motion() {
    fx::Emitter e;
    e.burst = 1;
    e.life = { 200, 200 };
    e.speed = { 10, 10 };
    e.angle = -60;
    e.spread = 0;
    e.gravity = { 0.05f, 0.3f };
    e.fadeIn = e.fadeOut = 0;
    for (float drag : { 0.f, 0.04f }) {
        e.drag = drag;
        const double k = drag > 0 ? -std::log(1.0 - drag) : 0;
        double px = 0, py = 0, vx = 10 * std::cos(-60 * PI / 180), vy = 10 * std::sin(-60 * PI / 180);
        const int steps = 1000;
        for (int f = 1; f <= 120; f++) {
            for (int s = 0; s < steps; s++) {
                const double dt = 1.0 / steps;
                vx += (e.gravity.x - k * vx) * dt;
                vy += (e.gravity.y - k * vy) * dt;
                px += vx * dt;
                py += vy * dt;
            }
            if (f % 30 == 0) {
                const fx::Particle p = fx::particles(e, f)[0];
                CHECK(near(p.pos.x, (float)px, 0.05f) && near(p.pos.y, (float)py, 0.05f));
                CHECK(near(p.vel.x, (float)vx, 0.01f) && near(p.vel.y, (float)vy, 0.01f));
            }
        }
    }
    // Drag alone: the particle coasts to a stop at v0 / k.
    e.gravity = { 0, 0 };
    e.drag = 0.1f;
    e.angle = 0;
    const fx::Particle far = fx::particles(e, 199)[0];
    CHECK(near(far.pos.x, (float)(10 / -std::log(0.9)), 0.5f));
    // Turbulence starts from no offset and stays within its amplitude.
    e.drag = 0;
    e.speed = { 0, 0 };
    e.turbulence = 30;
    CHECK(near(fx::particles(e, 0)[0].pos.x, 0, 1e-4f));
    for (int f = 0; f < 200; f += 7) {
        const fx::Particle p = fx::particles(e, f)[0];
        CHECK(std::fabs(p.pos.x) <= 61 && std::fabs(p.pos.y) <= 61);
    }
}

static void areasAndLooks() {
    fx::Emitter e;
    e.burst = 200;
    e.speed = { 0, 0 };
    e.area = fx::Area::Ring;
    e.extent = { 100, 0 };
    for (const fx::Particle& p : fx::particles(e, 0)) CHECK(near(std::hypot(p.pos.x, p.pos.y), 100, 0.01f));
    e.area = fx::Area::Rect;
    e.extent = { 300, 50 };
    for (const fx::Particle& p : fx::particles(e, 0)) CHECK(std::fabs(p.pos.x) <= 150 && std::fabs(p.pos.y) <= 25);
    e.area = fx::Area::Circle;
    e.extent = { 80, 0 };
    e.speed = { 5, 5 };
    e.outward = true;
    e.spread = 0;
    for (const fx::Particle& p : fx::particles(e, 0))
        CHECK(p.pos.x * p.vel.x + p.pos.y * p.vel.y >= 0);   // moving away from the centre
    // A moving origin is sampled at each birth; particles keep their own path afterwards.
    fx::Emitter m;
    m.rate = 1;
    m.speed = { 0, 0 };
    m.life = { 100, 100 };
    m.origin = [](float f) { return Vector2{ 10 * f, 0 }; };
    for (const fx::Particle& p : fx::particles(m, 50)) CHECK(near(p.pos.x, 10 * (50 - p.age), 0.01f));
    // Size and colour over life.
    fx::Emitter s;
    s.burst = 1;
    s.size = { 10, 10 };
    s.sizeEnd = 0;
    s.life = { 100, 100 };
    s.overLife = { RED, BLUE };
    s.fadeIn = s.fadeOut = 0;
    const fx::Particle half = fx::particles(s, 50)[0];
    CHECK(near(half.size, 5, 0.01f));
    CHECK(std::abs(half.color.r - (RED.r + BLUE.r) / 2) <= 2 && std::abs(half.color.b - (RED.b + BLUE.b) / 2) <= 2 && half.alpha == 1);
}

static void presets() {
    const std::pair<const char*, fx::Emitter> all[] = {
        { "burst", fx::burst() }, { "sparks", fx::sparks() }, { "fountain", fx::fountain() },
        { "confetti", fx::confetti() }, { "rain", fx::rain() }, { "snow", fx::snow() },
        { "fire", fx::fire() }, { "smoke", fx::smoke() }, { "embers", fx::embers() } };
    for (const auto& [name, e] : all) {
        const size_t n = fx::particles(e, 60).size();
        if (n < 5 || n > 5000) { std::fprintf(stderr, "  preset %s: %zu particles at frame 60\n", name, n); failures++; }
    }
    // Prewarmed weather fills the frame from the first frame.
    CHECK(fx::particles(fx::rain(), 0).size() > 100);
    CHECK(fx::particles(fx::snow(), 0).size() > 100);
    // Rain falls through the whole height within its life.
    float lowest = 0;
    for (const fx::Particle& p : fx::particles(fx::rain(1920, 1080), 0)) lowest = std::max(lowest, p.pos.y);
    CHECK(lowest > 1000);
}

int main() {
    bursts();
    streams();
    determinism();
    motion();
    areasAndLooks();
    presets();
    if (failures) { std::fprintf(stderr, "%d particle check(s) failed\n", failures); return 1; }
    std::printf("particle tests passed\n");
    return 0;
}
