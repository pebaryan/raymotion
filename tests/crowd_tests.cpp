// crowd_tests.cpp -- crowds: the bake is deterministic and sampling is pure; agents keep
// their distance, flock, go round obstacles, stay in bounds, flee, turn within their limits;
// gaits keep the feet planted; flyers keep their band and bank into turns. No window.
#include "rmtn/raymotion.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static float dist2d(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }

static crowd::Group walkers(int n) {
    crowd::Group g;
    g.name = "people";
    g.count = n;
    g.spawn = { 0, 0, -8 };
    g.spawnSize = { 8, 0, 3 };
    g.cruise = 1.3f;
    return g;
}

static void determinism() {
    crowd::World w;
    crowd::Group g = walkers(20);
    g.goal = [](double s, int i) { return Vector3{ (float)(i % 5) - 2, 0, s < 4 ? 8.f : -8.f }; };
    w.groups.push_back(g);
    w.obstacles.push_back({ { 0, 0, 0 }, 1.5f });
    w.to = 8;
    const auto a = crowd::bake(w), b = crowd::bake(w);
    bool same = true;
    for (int k = 0; k < a->size(); k++)
        for (double s : { 7.3, 0.0, 2.51, 5.0, 2.51 }) {           // out of order, and twice
            const crowd::State x = a->at(k, s), y = b->at(k, s);
            same = same && std::memcmp(&x, &y, sizeof x) == 0 && a->travelled(k, s) == b->travelled(k, s);
        }
    CHECK(same);
    // A sample between steps lies between its neighbours (smooth, not stepped).
    const crowd::State p = a->at(3, 2.0), q = a->at(3, 2.0 + 1.0 / 120), r = a->at(3, 2.0 + 1.0 / 60);
    CHECK(dist2d(p.pos, q.pos) <= dist2d(p.pos, r.pos) + 1e-5f);
}

static void separationAndObstacles() {
    crowd::World w;
    crowd::Group g = walkers(30);
    g.goal = [](double, int i) { return Vector3{ (float)(i % 6) * 1.2f - 3, 0, 8 + (float)(i / 6) * 1.2f }; };   // a grid of places
    w.groups.push_back(g);
    w.obstacles.push_back({ { 0, 0, 0 }, 1.5f });
    w.to = 30;
    const auto c = crowd::bake(w);
    float closest = 1e9f, inside = 1e9f, out = 0;
    for (double s = 2; s <= 30; s += 0.1)
        for (int a = 0; a < c->size(); a++) {
            const Vector3 p = c->at(a, s).pos;
            inside = std::min(inside, dist2d(p, { 0, 0, 0 }));
            out = std::max({ out, std::fabs(p.x) - 20, std::fabs(p.z) - 20 });
            for (int b = a + 1; b < c->size(); b++) closest = std::min(closest, dist2d(p, c->at(b, s).pos));
        }
    std::printf("[crowd] 30 walkers: closest pair %.2f m (personal %.2f), nearest the fountain %.2f m (r 1.5)\n", closest,
                g.steer.personal, inside);
    CHECK(closest >= 0.8f * g.steer.personal);
    CHECK(inside >= 1.5f);
    CHECK(out <= 0);
    // Everyone got round the fountain to their own place.
    int arrived = 0;
    for (int a = 0; a < c->size(); a++) arrived += dist2d(c->at(a, 30).pos, g.goal(30, a)) < 1;
    std::printf("[crowd] %d of %d walkers within 1 m of their place at 30 s\n", arrived, c->size());
    CHECK(arrived == c->size());
}

// Two streams walk head-on through each other round a fountain: they pass, not jam.
static void counterflow() {
    crowd::World w;
    w.from = -3;
    w.to = 14;
    w.obstacles.push_back({ { 0, 0, 0 }, 1.6f });
    for (int dir : { 1, -1 }) {
        crowd::Group g = walkers(20);
        g.name = dir > 0 ? "north" : "south";
        g.spawn = { 0, 0, -9.5f * dir };
        g.spawnSize = { 9, 0, 4 };
        g.heading0 = { 0, 0, (float)dir };
        g.headingSpread = 20;
        g.goal = [dir](double, int i) { return Vector3{ (float)(i % 5 - 2) * 1.8f, 0, 18.f * dir }; };
        w.groups.push_back(g);
    }
    const auto c = crowd::bake(w);
    // Free walking covers ~1.3 m/s * 11 s; give the crossing a fair margin.
    int through = 0;
    double slow = 0;
    for (int a = 0; a < c->size(); a++) {
        const float dir = c->group(a) == 0 ? 1.f : -1.f;
        through += c->at(a, 11).pos.z * dir > 4;
        slow += (c->travelled(a, 11) - c->travelled(a, 0)) / 11;
    }
    slow /= c->size();
    std::printf("[crowd] counterflow: %d of %d past the fountain at 11 s, mean speed %.2f m/s\n", through, c->size(), slow);
    CHECK(through == c->size());
    CHECK(slow > 0.8);
}

static void groundTurning() {
    crowd::World w;
    crowd::Group g = walkers(12);
    g.maxTurn = 120;
    g.goal = [](double s, int) { return Vector3{ s < 3 ? 8.f : -8.f, 0, 0 }; };   // a sharp change of mind
    w.groups.push_back(g);
    w.to = 8;
    const auto c = crowd::bake(w);
    float worstRate = 0, worstSide = 0;
    const double dt = w.dt;
    for (int a = 0; a < c->size(); a++)
        for (double s = 0; s + dt <= 8; s += dt) {
            const crowd::State x = c->at(a, s), y = c->at(a, s + dt);
            worstRate = std::max(worstRate, std::fabs(y.yaw - x.yaw) / (float)dt);
            if (x.speed > 0.05f) {
                const Vector3 h{ std::sin(x.yaw * DEG2RAD), 0, std::cos(x.yaw * DEG2RAD) };
                const Vector3 v = Vector3Scale(x.vel, 1 / x.speed);
                worstSide = std::max(worstSide, std::fabs(h.x * v.z - h.z * v.x));   // sideways share of motion
            }
        }
    std::printf("[crowd] ground turning: fastest %.0f deg/s (limit 120), sideways share %.4f\n", worstRate, worstSide);
    CHECK(worstRate <= 120.5f);
    CHECK(worstSide < 1e-3f);
}

static float polarisation(const crowd::Crowd& c, double s) {
    Vector3 sum{};
    for (int a = 0; a < c.size(); a++) {
        const crowd::State st = c.at(a, s);
        sum = Vector3Add(sum, Vector3Scale(st.vel, 1 / std::max(st.speed, 1e-4f)));
    }
    return Vector3Length(sum) / c.size();
}

static void flocking() {
    crowd::World w;
    crowd::Group g;
    g.name = "birds";
    g.medium = crowd::Medium::Air;
    g.count = 25;
    g.spawn = { 0, 8, 0 };
    g.spawnSize = { 8, 3, 8 };
    g.headingSpread = 360;
    g.cruise = 6;
    g.maxSpeed = 9;
    g.maxAccel = 6;
    g.maxTurn = 120;
    g.height0 = 6;
    g.height1 = 12;
    g.steer.personal = 1;
    g.steer.neighbour = 10;
    w.groups.push_back(g);
    w.boundsMin = { -40, 0, -40 };
    w.boundsMax = { 40, 30, 40 };
    w.to = 20;
    const auto c = crowd::bake(w);
    const float p0 = polarisation(*c, 0), p1 = polarisation(*c, 12);
    float lowest = 1e9f, highest = -1e9f, bankWrong = 0;
    int turning = 0;
    for (int a = 0; a < c->size(); a++)
        for (double s = 4; s <= 20; s += 0.05) {
            const crowd::State x = c->at(a, s), y = c->at(a, s + 0.05);
            lowest = std::min(lowest, x.pos.y);
            highest = std::max(highest, x.pos.y);
            const float rate = (y.yaw - x.yaw) / 0.05f;
            if (std::fabs(rate) > 30) {
                turning++;
                if (rate * x.bank > 0) bankWrong++;               // a left turn (yaw up) banks negative
            }
        }
    std::printf("[crowd] flock: polarisation %.2f -> %.2f; height %.1f..%.1f (band 6..12); %d turning samples, %.0f%% banked wrong\n",
                p0, p1, lowest, highest, turning, turning ? 100.0 * bankWrong / turning : 0.0);
    CHECK(p1 > 0.8f && p1 > p0 + 0.3f);
    CHECK(lowest > 5 && highest < 13);
    CHECK(turning > 0 && bankWrong <= 0.1f * turning);
}

static void fleeing() {
    crowd::World w;
    crowd::Group g;
    g.name = "sheep";
    g.count = 15;
    g.spawn = { 0, 0, 0 };
    g.spawnSize = { 5, 0, 5 };
    g.cruise = 0.5f;
    g.maxSpeed = 3;
    g.rest = 0.5f;
    g.flees = { "dog" };
    w.groups.push_back(g);
    w.predators.push_back({ "dog", [](double s) { return Vector3{ -14 + 2.5f * (float)s, 0, 0 }; } });   // crosses at ~5.6 s
    w.boundsMin = { -30, 0, -30 };
    w.boundsMax = { 30, 10, 30 };
    w.to = 10;
    const auto c = crowd::bake(w);
    float calm = 0, alarmed = 0, nearest = 1e9f;
    for (int a = 0; a < c->size(); a++) {
        calm = std::max(calm, c->at(a, 2).speed);
        for (double s = 5; s <= 7; s += 0.1) {
            alarmed = std::max(alarmed, c->at(a, s).speed);
            nearest = std::min(nearest, dist2d(c->at(a, s).pos, { -14 + 2.5f * (float)s, 0, 0 }));
        }
    }
    bool rested = false;
    for (int a = 0; a < c->size(); a++)
        for (double s = 0; s < 4; s += 0.25) rested = rested || c->resting(a, s);
    std::printf("[crowd] flee: calm top speed %.2f, alarmed %.2f m/s; closest to the dog %.2f m\n", calm, alarmed, nearest);
    CHECK(rested);
    CHECK(alarmed > 2 * calm && alarmed > 1.5f);
    CHECK(nearest > 1);
}

// A school in a pond: a predator driving straight through it doesn't push a fish out.
static void pen() {
    crowd::World w;
    crowd::Group g;
    g.name = "fish";
    g.medium = crowd::Medium::Water;
    g.count = 25;
    g.spawn = { 2, 0.3f, 0 };
    g.spawnSize = { 1.5f, 0.2f, 1.5f };
    g.cruise = 1.1f;
    g.maxSpeed = 3.5f;
    g.maxAccel = 7;
    g.height0 = 0.15f;
    g.height1 = 0.45f;
    g.steer.personal = 0.3f;
    g.steer.neighbour = 1.4f;
    g.steer.panic = 1.6f;
    g.steer.flee = 6;
    g.flees = { "pike" };
    g.penRadius = 4;
    w.groups.push_back(g);
    // Outward from the middle through where the school starts, then on past the rim.
    w.predators.push_back({ "pike", [](double s) { return Vector3{ -1 + 3 * (float)s, 0.3f, 0 }; } });
    w.boundsMin = { -30, 0, -30 };
    w.boundsMax = { 30, 10, 30 };
    w.to = 6;
    const auto c = crowd::bake(w);
    float far = 0, fled = 0;
    for (int a = 0; a < c->size(); a++)
        for (double s = 0; s <= 6; s += 1.0 / 30) {
            far = std::max(far, dist2d(c->at(a, s).pos, { 0, 0, 0 }));
            fled = std::max(fled, c->at(a, s).speed);
        }
    std::printf("[crowd] pen: farthest from the middle %.2f m (radius 4); top speed %.2f m/s\n", far, fled);
    CHECK(far <= 4.001f);
    CHECK(fled > 2);
}

// The world position of a character joint for a crowd agent (the node: yaw then position).
static Vector3 worldJoint(const crowd::Crowd& c, int agent, const rig::Character& ch, double s, rig::Joint j) {
    const crowd::State st = c.at(agent, s);
    const rig::Build& b = ch.lookAt(s).build;
    const Matrix node = MatrixMultiply(MatrixRotateY(st.yaw * DEG2RAD), MatrixTranslate(st.pos.x, st.pos.y, st.pos.z));
    return Vector3Transform(rig::position(rig::solve(b, ch.poseAt(s)), j), node);
}

// The worst drift of a foot while it is down: each stance (the foot within 1.5 cm of its
// lowest) measured from touchdown to lift-off, horizontally.
static float band = 0.003f;               // down: within 3 mm of the lowest (planted feet sit exactly)
static float footSlide(const crowd::Crowd& c, int agent, const rig::Character& ch, rig::Joint j, double from, double to) {
    std::vector<Vector3> p;
    for (double s = from; s <= to; s += 1.0 / 120) p.push_back(worldJoint(c, agent, ch, s, j));
    float low = 1e9f;
    for (const Vector3& q : p) low = std::min(low, q.y);
    float worst = 0;
    for (size_t i = 0; i < p.size();) {
        if (p[i].y > low + band) { i++; continue; }
        size_t k = i;
        while (k + 1 < p.size() && p[k + 1].y <= low + band) k++;
        if (k > i) worst = std::max(worst, dist2d(p[i], p[k]));
        i = k + 1;
    }
    return worst;
}

static void feet() {
    // A walker and a horse at steady speeds, then a horse speeding up through its gaits.
    for (int which = 0; which < 3; which++) {
        crowd::World w;
        crowd::Group g;
        g.name = "a";
        g.count = 1;
        g.spawnSize = { 0, 0, 0 };
        g.speedJitter = g.sizeJitter = 0;
        g.steer.wander = 0;
        rig::Look look = which == 0 ? crowd::person(0) : crowd::herd("horse", 0);
        const float walk = which == 0 ? rig::walkSpeed(look.build, 1) : rig::gaitSpeed(look.build, rig::Gait::Walk);
        g.cruise = which == 2 ? 2.5f * walk : walk;
        g.maxSpeed = g.cruise;
        g.maxAccel = which == 2 ? 0.6f : 2;               // a slow build-up through the gaits
        g.spawn = { 0, 0, -60 };
        g.goal = [](double, int) { return Vector3{ 0, 0, 200 }; };
        w.groups.push_back(g);
        w.boundsMin = { -100, 0, -100 };
        w.boundsMax = { 100, 10, 300 };
        w.to = 12;
        const auto c = crowd::bake(w);
        const auto ch = crowd::cast(c, "a", [look](int) { return look; })[0];
        const rig::Build& b = ch->lookAt(0).build;
        const char* label[3] = { "walker", "horse walk", "horse speeding up" };
        float worst = 0;
        if (which == 0) {
            worst = std::max(footSlide(*c, 0, *ch, rig::Joint::AnkleL, 3, 9), footSlide(*c, 0, *ch, rig::Joint::AnkleR, 3, 9));
        } else {
            for (const char* paw : { "paw-hind-L", "paw-front-L", "paw-front-R", "paw-hind-R" })
                worst = std::max(worst, footSlide(*c, 0, *ch, rig::joint(b, paw), which == 2 ? 1 : 3, 11));
        }
        std::printf("[crowd] %s: feet drift at most %.1f cm while down (speed %.2f m/s at 6 s)\n", label[which], worst * 100,
                    c->at(0, 6).speed);
        CHECK(worst < 0.015f);
    }
}

int main(int argc, char** argv) try {
    if (argc > 1) band = (float)std::atof(argv[1]);
    determinism();
    separationAndObstacles();
    counterflow();
    groundTurning();
    flocking();
    fleeing();
    pen();
    feet();
    if (failures) std::fprintf(stderr, "%d failure(s)\n", failures);
    else std::printf("crowd tests passed\n");
    return failures ? 1 : 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "uncaught: %s\n", e.what());
    return 2;
}
