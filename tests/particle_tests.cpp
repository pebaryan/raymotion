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

static float len3(Vector3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

// 3D emitters share the 2D schedule: the same settings give the same particle counts.
static void schedule3d() {
    fx::Emitter e2;
    fx::Emitter3D e3;
    e2.burst = e3.burst = 30;
    e2.burstSpread = e3.burstSpread = 6;
    e2.rate = e3.rate = 1.5f;
    e2.life = e3.life = { 20, 40 };
    e2.until = e3.until = 80;
    for (int t : { 0, 3, 10, 50, 79, 85, 119, 125 }) {
        const size_t n3 = fx::particles(e3, t).size(), n2 = fx::particles(e2, t).size();
        CHECK(n3 > 0 ? std::abs((int)n3 - (int)n2) <= 6 : n2 <= 6);    // births jitter by their own seeds
    }
    CHECK(fx::particles(e3, 121).empty());
    // Determinism, and colours do not move anything.
    fx::Emitter3D d = fx::burst3d(60);
    d.turbulence = 0.3f;
    const auto a = fx::particles(d, 30);
    fx::particles(d, 2);
    const auto b = fx::particles(d, 30);
    CHECK(a.size() == b.size() && a.size() == 60);
    d.colors = { RED, GREEN };
    const auto c = fx::particles(d, 30);
    for (size_t i = 0; i < a.size() && i < b.size() && i < c.size(); i++) {
        CHECK(a[i].pos.x == b[i].pos.x && a[i].pos.y == b[i].pos.y && a[i].pos.z == b[i].pos.z);
        CHECK(a[i].pos.z == c[i].pos.z);
    }
}

static void cones3d() {
    fx::Emitter3D e;
    e.burst = 400;
    e.speed = { 1, 1 };
    e.direction = { 1, 0, 0 };
    e.spread = 60;                                   // within 30 degrees of +x
    for (const fx::Particle3D& p : fx::particles(e, 0)) CHECK(p.vel.x / len3(p.vel) >= std::cos(30 * PI / 180) - 1e-4);
    e.spread = 0;
    for (const fx::Particle3D& p : fx::particles(e, 0)) CHECK(near(p.vel.x, 1, 1e-4f));
    e.spread = 360;                                  // every direction, evenly
    Vector3 mean{ 0, 0, 0 };
    int up = 0;
    for (const fx::Particle3D& p : fx::particles(e, 0)) {
        mean.x += p.vel.x / 400; mean.y += p.vel.y / 400; mean.z += p.vel.z / 400;
        up += p.vel.y > 0;
    }
    CHECK(len3(mean) < 0.12f && up > 160 && up < 240);
}

static void volumes3d() {
    fx::Emitter3D e;
    e.burst = 300;
    e.speed = { 0, 0 };
    e.extent = { 2, 1, 3 };
    e.area = fx::Volume::Box;
    for (const fx::Particle3D& p : fx::particles(e, 0)) CHECK(std::fabs(p.pos.x) <= 1 && std::fabs(p.pos.y) <= 0.5f && std::fabs(p.pos.z) <= 1.5f);
    e.area = fx::Volume::Shell;
    for (const fx::Particle3D& p : fx::particles(e, 0)) CHECK(near(len3(p.pos), 2, 1e-3f));
    e.area = fx::Volume::Sphere;
    int inner = 0;
    for (const fx::Particle3D& p : fx::particles(e, 0)) { CHECK(len3(p.pos) <= 2.001f); inner += len3(p.pos) < 1; }
    CHECK(inner > 15 && inner < 75);                 // uniform in volume: 1/8 within half the radius
    e.area = fx::Volume::Disc;
    for (const fx::Particle3D& p : fx::particles(e, 0)) CHECK(p.pos.y == 0 && std::hypot(p.pos.x, p.pos.z) <= 2.001f);
    e.area = fx::Volume::Ring;
    e.speed = { 1, 1 };
    e.outward = true;
    e.spread = 0;
    for (const fx::Particle3D& p : fx::particles(e, 0)) {
        CHECK(near(std::hypot(p.pos.x, p.pos.z), 2, 1e-3f));
        CHECK(p.pos.x * p.vel.x + p.pos.z * p.vel.z > 0.99f * 2);
    }
    fx::Emitter3D m;
    m.rate = 1;
    m.speed = { 0, 0 };
    m.life = { 100, 100 };
    m.origin = [](float f) { return Vector3{ 0, 0, 0.5f * f }; };
    for (const fx::Particle3D& p : fx::particles(m, 40)) CHECK(near(p.pos.z, 0.5f * (40 - p.age), 1e-3f));
}

// The same closed form as in 2D, per axis; and the floor.
static void motion3d() {
    fx::Emitter3D e;
    e.burst = 1;
    e.life = { 200, 200 };
    e.speed = { 0.2f, 0.2f };
    e.direction = { 0, 1, 1 };
    e.spread = 0;
    e.gravity = { 0.001f, -0.004f, 0 };
    e.drag = 0.03f;
    const double k = -std::log(1.0 - 0.03), v0 = 0.2 / std::sqrt(2.0);
    for (int t : { 10, 60, 150 }) {
        auto axis = [&](double v, double g) { const double term = g / k, dec = std::exp(-k * t); return term * t + (v - term) * (1 - dec) / k; };
        const fx::Particle3D p = fx::particles(e, t)[0];
        CHECK(near(p.pos.x, (float)axis(0, 0.001), 1e-3f) && near(p.pos.y, (float)axis(v0, -0.004), 1e-3f) && near(p.pos.z, (float)axis(v0, 0), 1e-3f));
    }
    // A streak's tail is where the particle was.
    const fx::Particle3D p = fx::particles(e, 60)[0], q = fx::particles(e, 57)[0];
    const Vector3 tail = fx::positionAt(e, p, 3);
    CHECK(near(tail.x, q.pos.x, 1e-4f) && near(tail.y, q.pos.y, 1e-4f) && near(tail.z, q.pos.z, 1e-4f));

    // Debris lands on the floor and stays put, no longer spinning.
    fx::Emitter3D d = fx::debris3d(30);
    for (int t = 0; t < 150; t += 5)
        for (const fx::Particle3D& r : fx::particles(d, t)) CHECK(r.pos.y >= -1e-4f);
    const auto late = fx::particles(d, 120), later = fx::particles(d, 140);
    CHECK(late.size() == 30 && later.size() == 30);
    for (size_t i = 0; i < late.size() && i < later.size(); i++) {
        CHECK(late[i].pos.y == 0 && later[i].pos.x == late[i].pos.x && later[i].rotation == late[i].rotation);
        CHECK(late[i].vel.x == 0 && late[i].vel.y == 0);
    }
    float peak = 0;
    for (const fx::Particle3D& r : fx::particles(d, 12)) peak = std::max(peak, r.pos.y);
    CHECK(peak > 0.3f);                              // thrown up first
}

static void presets3d() {
    const std::pair<const char*, fx::Emitter3D> all[] = {
        { "burst3d", fx::burst3d() }, { "sparks3d", fx::sparks3d() }, { "fountain3d", fx::fountain3d() },
        { "confetti3d", fx::confetti3d() }, { "snow3d", fx::snow3d() }, { "rain3d", fx::rain3d() },
        { "fire3d", fx::fire3d() }, { "smoke3d", fx::smoke3d() }, { "embers3d", fx::embers3d() }, { "debris3d", fx::debris3d() } };
    for (const auto& [name, e] : all) {
        const size_t n = fx::particles(e, 60).size();
        if (n < 5 || n > 8000) { std::fprintf(stderr, "  preset %s: %zu particles at frame 60\n", name, n); failures++; }
    }
    CHECK(fx::particles(fx::snow3d(), 0).size() > 100);
    // Weather falls through its height; fire and smoke rise.
    float lowest = 0;
    for (const fx::Particle3D& p : fx::particles(fx::rain3d(10, 10, 5), 0)) lowest = std::min(lowest, p.pos.y);
    CHECK(lowest < -4.5f);
    for (const fx::Particle3D& p : fx::particles(fx::fire3d(), 60)) CHECK(p.pos.y >= 0);
    double rise = 0;
    const auto smoke = fx::particles(fx::smoke3d(), 200);
    for (const fx::Particle3D& p : smoke) rise += p.pos.y / smoke.size();
    CHECK(rise > 0.5);
}

int main() {
    bursts();
    streams();
    determinism();
    motion();
    areasAndLooks();
    presets();
    schedule3d();
    cones3d();
    volumes3d();
    motion3d();
    presets3d();
    if (failures) { std::fprintf(stderr, "%d particle check(s) failed\n", failures); return 1; }
    std::printf("particle tests passed\n");
    return 0;
}
