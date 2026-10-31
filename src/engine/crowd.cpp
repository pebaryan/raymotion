// crowd.cpp -- the crowd simulation: boids baked once at a fixed step, sampled per frame.
#include "rmtn/crowd.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rmtn {
namespace crowd {
namespace {

Vector3 flat(Vector3 v) { return { v.x, 0, v.z }; }
float len(Vector3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
Vector3 limit(Vector3 v, float m) {
    const float l = len(v);
    return l > m && l > 0 ? Vector3Scale(v, m / l) : v;
}
float wrap180(float d) {
    d = std::fmod(d + 180.f, 360.f);
    return (d < 0 ? d + 360.f : d) - 180.f;
}
float yawOf(Vector3 v) { return std::atan2(v.x, v.z) * RAD2DEG; }
Vector3 headingOf(float yaw) { return { std::sin(yaw * DEG2RAD), 0, std::cos(yaw * DEG2RAD) }; }

// Smooth noise in -1..1: seeded values at integer knots, smoothstepped between.
float noise(unsigned long long seed, unsigned long long stream, double x) {
    const double k = std::floor(x);
    const double u = x - k, w = u * u * (3 - 2 * u);
    const auto i = (unsigned long long)(long long)k;
    const double a = rnd01(seed, stream * 7919ULL + i), b = rnd01(seed, stream * 7919ULL + i + 1);
    return (float)(2 * (a + (b - a) * w) - 1);
}

// A calm agent's pauses (Group::rest): in each window of a few seconds, a seeded draw decides
// whether it spends the first part standing.
bool pauseAt(unsigned long long seed, int agent, float rest, double t) {
    if (rest <= 0) return false;
    constexpr double kWindow = 5;
    const double off = rnd01(seed ^ 0x9e3779b97f4a7c15ULL, (unsigned long long)agent) * kWindow;
    const double x = (t + off) / kWindow;
    const auto k = (unsigned long long)(long long)std::floor(x);
    return rnd01(seed ^ 0x5eedULL, (unsigned long long)agent * 1009ULL + k) < rest && x - std::floor(x) < 0.6;
}

bool lists(const std::vector<std::string>& names, const std::string& n) {
    return std::find(names.begin(), names.end(), n) != names.end();
}

} // namespace

int Crowd::first(const std::string& group) const {
    int a = 0;
    for (const Group& g : world_.groups) {
        if (g.name == group) return a;
        a += g.count;
    }
    throw std::runtime_error("crowd: no group named " + group);
}

State Crowd::at(int agent, double s) const {
    const auto& path = agents_[(size_t)agent].path;
    const double dt = world_.dt;
    const double u = std::clamp((s - world_.from) / dt, 0.0, (double)(steps_ - 1));
    const int i = std::min((int)u, std::max(0, steps_ - 2));
    const float f = steps_ > 1 ? (float)(u - i) : 0.f;
    const Sample& a = path[(size_t)i];
    const Sample& b = path[(size_t)std::min(i + 1, steps_ - 1)];
    // Cubic Hermite through the stored positions and velocities.
    const float f2 = f * f, f3 = f2 * f;
    const float h00 = 2 * f3 - 3 * f2 + 1, h10 = f3 - 2 * f2 + f, h01 = -2 * f3 + 3 * f2, h11 = f3 - f2;
    State st;
    st.pos = Vector3Add(Vector3Add(Vector3Scale(a.pos, h00), Vector3Scale(a.vel, h10 * (float)dt)),
                        Vector3Add(Vector3Scale(b.pos, h01), Vector3Scale(b.vel, h11 * (float)dt)));
    st.vel = Vector3Lerp(a.vel, b.vel, f);
    if (groupOf(agent).medium == Medium::Ground) st.pos.y = 0;
    st.yaw = a.yaw + (b.yaw - a.yaw) * f;
    st.pitch = a.pitch + (b.pitch - a.pitch) * f;
    st.bank = a.bank + (b.bank - a.bank) * f;
    st.speed = len(st.vel);
    return st;
}

double Crowd::travelled(int agent, double s) const {
    const auto& path = agents_[(size_t)agent].path;
    const double u = (s - world_.from) / world_.dt;
    if (u <= 0) return path.front().dist + len(path.front().vel) * u * world_.dt;   // before: extrapolated
    if (u >= steps_ - 1) return path.back().dist + len(path.back().vel) * (u - (steps_ - 1)) * world_.dt;
    const int i = (int)u;
    const double f = u - i;
    return path[(size_t)i].dist + (path[(size_t)i + 1].dist - path[(size_t)i].dist) * f;
}

bool Crowd::resting(int agent, double s) const {
    const auto& path = agents_[(size_t)agent].path;
    const double u = std::clamp((s - world_.from) / world_.dt, 0.0, (double)(steps_ - 1));
    return path[(size_t)std::lround(u)].rest;
}

CrowdPtr bake(const World& w) {
    auto c = std::make_shared<Crowd>();
    c->world_ = w;
    const unsigned long long seed = w.seed;

    // ---- agents
    struct Live {
        Vector3 p, v;
        float yaw = 0, speed = 0, bank = 0, threat = 0;
        double dist = 0;
        bool rest = false;
    };
    std::vector<Live> live;
    for (int gi = 0; gi < (int)w.groups.size(); gi++) {
        const Group& g = w.groups[(size_t)gi];
        for (int i = 0; i < g.count; i++) {
            const int a = (int)live.size();
            auto r = [&](int k) { return (float)rnd01(seed, (unsigned long long)a * 97ULL + (unsigned long long)k); };
            Crowd::Agent ag;
            ag.group = gi;
            ag.index = i;
            ag.cruise = g.cruise * (1 + g.speedJitter * (2 * r(1) - 1));
            ag.size = 1 + g.sizeJitter * (2 * r(2) - 1);
            c->agents_.push_back(ag);
            Live l;
            l.p = { g.spawn.x + (r(3) - 0.5f) * g.spawnSize.x, g.spawn.y + (r(4) - 0.5f) * g.spawnSize.y,
                    g.spawn.z + (r(5) - 0.5f) * g.spawnSize.z };
            if (g.medium == Medium::Ground) l.p.y = 0;
            else if (g.height1 > g.height0) l.p.y = std::clamp(l.p.y, g.height0, g.height1);
            l.yaw = yawOf(g.heading0) + (r(6) - 0.5f) * g.headingSpread;
            l.speed = g.medium == Medium::Ground ? ag.cruise * 0.8f : ag.cruise;
            l.v = Vector3Scale(headingOf(l.yaw), l.speed);
            live.push_back(l);
        }
    }
    const int n = (int)live.size();
    const double dt = w.dt;
    c->steps_ = std::max(2, (int)std::ceil((w.to - w.from) / dt) + 1);
    for (auto& ag : c->agents_) ag.path.reserve((size_t)c->steps_);

    std::vector<Vector3> acc((size_t)n);
    std::vector<float> alarmed((size_t)n);     // each agent's alarm at the start of the step
    for (int step = 0; step < c->steps_; step++) {
        const double t = w.from + step * dt;
        // Record the state at t.
        for (int a = 0; a < n; a++) {
            const Live& l = live[(size_t)a];
            const Group& g = w.groups[(size_t)c->agents_[(size_t)a].group];
            const float pitch = g.medium == Medium::Ground ? 0.f : -std::atan2(l.v.y, len(flat(l.v)) + 1e-6f) * RAD2DEG;
            c->agents_[(size_t)a].path.push_back({ l.p, l.v, l.yaw, pitch, l.bank, l.dist, l.rest });
        }
        if (step == c->steps_ - 1) break;

        // ---- steering, from the state at t (synchronous: the order of agents doesn't matter)
        for (int a = 0; a < n; a++) alarmed[(size_t)a] = live[(size_t)a].threat;
        for (int a = 0; a < n; a++) {
            const Live& me = live[(size_t)a];
            const Crowd::Agent& ag = c->agents_[(size_t)a];
            const Group& g = w.groups[(size_t)ag.group];
            const Steer& S = g.steer;
            const bool ground = g.medium == Medium::Ground;
            const Vector3 fwd = len(me.v) > 1e-4f ? Vector3Scale(me.v, 1 / len(me.v)) : headingOf(me.yaw);
            const float cosFov = std::cos(S.fov * 0.5f * DEG2RAD);

            // Where a walker is headed, if anywhere: neighbours in that direction are slid past.
            Vector3 toGoal{};
            if (ground && g.goal) {
                toGoal = flat(Vector3Subtract(g.goal(t, ag.index), me.p));
                const float gd = len(toGoal);
                toGoal = gd > 0.35f ? Vector3Scale(toGoal, 1 / gd) : Vector3{};
            }
            Vector3 sep{}, align{}, centre{}, flee{}, pass{};
            int flock = 0;
            float threat = 0, contagion = 0;
            for (int b = 0; b < n; b++) {
                if (b == a) continue;
                const Live& o = live[(size_t)b];
                const Group& og = w.groups[(size_t)c->agents_[(size_t)b].group];
                Vector3 d = Vector3Subtract(o.p, me.p);
                if (ground && og.medium == Medium::Ground) d.y = 0;
                const float dist = len(d);
                if (dist < 1e-5f) {                      // coincident: part along a seeded direction
                    sep = Vector3Add(sep, headingOf((float)rnd01(seed, (unsigned long long)(a * n + b)) * 360));
                    continue;
                }
                const Vector3 dir = Vector3Scale(d, 1 / dist);
                // Someone standing still only asks for room for their body, so a walker can
                // thread a standing crowd.
                const bool walkers = ground && og.medium == Medium::Ground;
                const bool still = walkers && len(o.v) < 0.1f;
                const float reach = still ? 0.5f * (S.personal + og.steer.personal) : S.personal + og.steer.personal * 0.5f;
                if (dist < reach) {
                    const float k = 1 - dist / reach;
                    sep = Vector3Subtract(sep, Vector3Scale(dir, k));
                    const float block = Vector3DotProduct(toGoal, dir);
                    if (walkers && block > 0.2f) {       // in the way: round them, on the side faced
                        Vector3 tan{ dir.z, 0, -dir.x };
                        float side = Vector3DotProduct(tan, toGoal);
                        if (std::fabs(side) < 0.05f) side = Vector3DotProduct(tan, fwd);
                        if (side < 0) tan = Vector3Negate(tan);
                        sep = Vector3Add(sep, Vector3Scale(tan, k * block));
                    }
                }
                if (c->agents_[(size_t)b].group == ag.group && dist < S.neighbour && Vector3DotProduct(fwd, dir) > cosFov) {
                    const float os = len(o.v);           // headings, not velocities; a walker
                    if (os > 1e-4f) align = Vector3Add(align, Vector3Scale(o.v, 1 / os));   // standing still
                    else if (!ground) align = Vector3Add(align, headingOf(o.yaw));          // leads nobody
                    centre = Vector3Add(centre, o.p);
                    flock++;
                    // Alarm spreads: a flockmate bolting is reason enough to bolt.
                    contagion = std::max(contagion, alarmed[(size_t)b] * 0.85f);
                }
                if (dist < S.panic && lists(g.flees, og.name)) {
                    const float k = 1 - dist / S.panic;
                    flee = Vector3Subtract(flee, Vector3Scale(dir, k));
                    threat = std::max(threat, std::min(1.f, 2 * k));   // full alarm at half the distance
                }
                // Walkers look ahead: someone on a collision course is passed, not bumped.
                if (ground && og.medium == Medium::Ground && dist < 5) {
                    const Vector3 dv = Vector3Subtract(o.v, me.v);
                    const float dv2 = Vector3DotProduct(dv, dv);
                    if (dv2 > 1e-4f) {
                        const float tca = -Vector3DotProduct(d, dv) / dv2;
                        if (tca > 0 && tca < 3) {
                            const Vector3 miss = Vector3Add(d, Vector3Scale(dv, tca));
                            const float md = len(miss), room = (S.personal + og.steer.personal) * 0.6f + 0.3f;
                            if (md < room) {
                                // Away from where the other will be; dead ahead, keep to the right.
                                const Vector3 away = md > 1e-3f ? Vector3Scale(miss, -1 / md)
                                                                : Vector3{ -fwd.z, 0, fwd.x };
                                pass = Vector3Add(pass, Vector3Scale(away, (1 - tca / 3) * (1 - md / room)));
                            }
                        }
                    }
                }
            }
            for (const Predator& pr : w.predators) {
                if (!lists(g.flees, pr.name) || !pr.at) continue;
                Vector3 d = Vector3Subtract(pr.at(t), me.p);
                if (ground) d.y = 0;
                const float dist = len(d);
                if (dist < S.panic && dist > 1e-5f) {
                    const float k = 1 - dist / S.panic;
                    flee = Vector3Subtract(flee, Vector3Scale(d, k / dist));
                    threat = std::max(threat, std::min(1.f, 2 * k));
                }
            }
            threat = std::max(threat, contagion);
            const float alarm = std::max(threat, me.threat);
            const bool pausing = alarm < 0.05f && pauseAt(seed, a, g.rest, t);
            const float base = pausing ? 0.f : ag.cruise;

            Vector3 f{};
            bool arrived = false;
            float flocking = 1;                          // walkers near their goal stop following
            if (g.goal) {
                Vector3 to = Vector3Subtract(g.goal(t, ag.index), me.p);
                if (ground) to.y = 0;
                const float d = len(to);
                // Slow on the approach; a walker that has arrived stops (it can't step sideways,
                // so chasing the last few centimetres would set it circling).
                // Just past it (a metre or less, the goal behind): stop rather than turn round.
                arrived = ground && (d < 0.35f || (d < 0.9f && Vector3DotProduct(to, fwd) < 0));
                if (ground) flocking = std::clamp((d - 1) / 4, 0.f, 1.f);
                const Vector3 want = d > 1e-4f && !arrived ? Vector3Scale(to, std::min(base, d * 0.6f) / d) : Vector3{};
                // A panicking agent forgets where it was going.
                f = Vector3Add(f, Vector3Scale(Vector3Subtract(want, me.v), S.goal * 1.2f * (1 - 0.85f * std::min(1.f, alarm))));
            } else {
                f = Vector3Add(f, Vector3Scale(Vector3Subtract(Vector3Scale(fwd, base), me.v), 0.8f));
            }
            if (flock && flocking > 0) {
                const float k = 1.f / flock;
                // Towards the neighbours' mean heading, at this agent's own pace.
                const float al = len(align);
                if (al > 1e-4f) {
                    const float pace = std::max(len(me.v), 0.5f * base);
                    f = Vector3Add(f, Vector3Scale(Vector3Subtract(Vector3Scale(align, pace / al), me.v), S.alignment * flocking));
                }
                f = Vector3Add(f, limit(Vector3Scale(Vector3Subtract(Vector3Scale(centre, k), me.p), S.cohesion * 0.25f * flocking), 2));
            }
            f = Vector3Add(f, Vector3Scale(sep, S.separation * 4));
            f = Vector3Add(f, Vector3Scale(pass, S.avoid * 1.5f));
            f = Vector3Add(f, Vector3Scale(flee, S.flee * 3));
            // Wander: the heading drifts, slowly and differently for each agent -- a turn rate
            // (wander 1 = up to 40 degrees a second), so it reads the same at any speed.
            const Vector3 side{ -fwd.z, 0, fwd.x };
            const float wob = noise(seed, (unsigned long long)a * 2, t / 1.7);
            if (!arrived) f = Vector3Add(f, Vector3Scale(side, wob * S.wander * 0.7f * std::max(len(me.v), 0.3f * base)));
            if (!ground) f.y += noise(seed, (unsigned long long)a * 2 + 1, t / 2.3) * S.wander * 0.6f;
            // Obstacles: steer round what lies ahead; shove out of what is too close.
            for (const Obstacle& ob : w.obstacles) {
                if (!ground && me.p.y > ob.at.y + ob.height + 0.5f) continue;
                const Vector3 rel = flat(Vector3Subtract(ob.at, me.p));
                const float dc = len(rel), clear = ob.radius + S.personal * 0.8f;
                const Vector3 heading = Vector3Normalize(flat(fwd).x == 0 && flat(fwd).z == 0 ? headingOf(me.yaw) : flat(fwd));
                const float ahead = 1 + std::max(len(flat(me.v)), 0.5f * ag.cruise) * 2;
                const float along = Vector3DotProduct(rel, heading);
                if (along > -ob.radius && along < ahead + ob.radius) {
                    const Vector3 lat = Vector3Subtract(rel, Vector3Scale(heading, along));
                    const float ld = len(lat);
                    if (ld < clear) {
                        const Vector3 away = ld > 1e-3f ? Vector3Scale(lat, -1 / ld) : Vector3{ heading.z, 0, -heading.x };
                        const float k = (1 - ld / clear) * (1 - std::max(0.f, along) / (ahead + ob.radius));
                        f = Vector3Add(f, Vector3Scale(away, k * S.avoid * 3));
                    }
                }
                if (dc < clear + 0.5f && dc > 1e-4f) {
                    const float push = (clear + 0.5f - dc) / dc * S.avoid * 4;
                    // Pressed against it with somewhere to be beyond: slide round, not stall
                    // where the push and the pull cancel. The side is the one already faced.
                    if (Vector3DotProduct(f, rel) > 0) {
                        Vector3 tan{ rel.z, 0, -rel.x };
                        if (Vector3DotProduct(tan, heading) < 0) tan = Vector3Negate(tan);
                        f = Vector3Add(f, Vector3Scale(tan, push));
                    }
                    f = Vector3Add(f, Vector3Scale(rel, -push));
                }
            }
            // Bounds: a soft wall a few metres in.
            constexpr float kMargin = 3;
            const Vector3 lo = w.boundsMin, hi = w.boundsMax;
            if (me.p.x < lo.x + kMargin) f.x += (lo.x + kMargin - me.p.x) * S.bounds;
            if (me.p.x > hi.x - kMargin) f.x -= (me.p.x - (hi.x - kMargin)) * S.bounds;
            if (me.p.z < lo.z + kMargin) f.z += (lo.z + kMargin - me.p.z) * S.bounds;
            if (me.p.z > hi.z - kMargin) f.z -= (me.p.z - (hi.z - kMargin)) * S.bounds;
            if (g.penRadius > 0) {
                // The pen: a soft wall a body length or two in, stiff enough to turn back a flight.
                const Vector3 rel = flat(Vector3Subtract(me.p, g.penAt));
                const float d = len(rel), wall = g.penRadius - std::min(1.5f, 0.3f * g.penRadius);
                if (d > wall) f = Vector3Subtract(f, Vector3Scale(rel, (d - wall) / d * S.bounds * 4 * (1 + 2 * alarm)));
            }
            if (!ground) {
                if (g.height1 > g.height0) {
                    // Pulled back towards the band before reaching its edge.
                    const float m = std::min(1.f, (g.height1 - g.height0) * 0.25f);
                    if (me.p.y < g.height0 + m) f.y += (g.height0 + m - me.p.y) * 4;
                    if (me.p.y > g.height1 - m) f.y -= (me.p.y - (g.height1 - m)) * 4;
                }
                f.y -= me.v.y;                           // level out
            } else {
                f.y = 0;
            }
            acc[(size_t)a] = limit(f, g.maxAccel * (1 + alarm));
            live[(size_t)a].threat = std::max(threat, me.threat - (float)dt * 0.4f);   // calms over ~2.5 s
            live[(size_t)a].rest = pausing;
        }

        // ---- integrate
        for (int a = 0; a < n; a++) {
            Live& l = live[(size_t)a];
            const Crowd::Agent& ag = c->agents_[(size_t)a];
            const Group& g = w.groups[(size_t)ag.group];
            const float cap = ag.cruise * 1.3f + (g.maxSpeed * ag.cruise / std::max(0.01f, g.cruise) - ag.cruise * 1.3f) * l.threat;
            const float yaw0 = l.yaw;
            Vector3 want = Vector3Add(l.v, Vector3Scale(acc[(size_t)a], (float)dt));
            if (g.medium == Medium::Ground) {
                // Turn towards where the steering points, no faster than maxTurn (slower when
                // nearly still); then move forward only, slowing while facing away.
                want.y = 0;
                const float ws = len(want);
                // The way to face: where the next velocity points, or -- standing still, when one
                // step's velocity is next to nothing -- where the steering pushes.
                Vector3 aim = want;
                if (ws <= 0.12f) { aim = acc[(size_t)a]; aim.y = 0; }
                float turn = 0;
                if (ws > 0.12f || len(aim) > 0.3f) {     // no turning towards a whisper of steering
                    const float k = std::max(std::clamp(0.6f + l.speed / std::max(0.1f, 0.5f * ag.cruise), 0.6f, 1.f),
                                             std::min(1.f, 0.6f + l.threat));   // startled: wheel round
                    turn = std::clamp(wrap180(yawOf(aim) - l.yaw), -g.maxTurn * k * (float)dt, g.maxTurn * k * (float)dt);
                }
                l.yaw += turn;
                const float target = std::min(cap, ws * std::max(0.f, std::cos(wrap180(yawOf(aim) - l.yaw) * DEG2RAD)));
                const float step = g.maxAccel * (1 + l.threat) * (float)dt;
                l.speed += std::clamp(target - l.speed, -step, step);
                l.v = Vector3Scale(headingOf(l.yaw), l.speed);
            } else {
                // Flyers and swimmers: turn no faster than maxTurn, never stall.
                const float vs = std::max(len(l.v), 1e-4f), ws = std::max(len(want), 1e-4f);
                Vector3 d0 = Vector3Scale(l.v, 1 / vs), d1 = Vector3Scale(want, 1 / ws);
                const float ang = std::acos(std::clamp(Vector3DotProduct(d0, d1), -1.f, 1.f));
                const float maxAng = g.maxTurn * DEG2RAD * (float)dt;
                if (ang > maxAng) {
                    const float u = maxAng / ang;               // rotate d0 towards d1 by maxAng
                    d1 = Vector3Normalize(Vector3Lerp(d0, d1, u));
                }
                const float lo = ag.cruise * (g.medium == Medium::Air ? 0.55f : 0.15f);
                l.speed = std::clamp(ws, lo, std::max(lo, cap));
                l.v = Vector3Scale(d1, l.speed);
                if (len(flat(l.v)) > 1e-3f) l.yaw = yaw0 + wrap180(yawOf(l.v) - yaw0);
            }
            l.p = Vector3Add(l.p, Vector3Scale(l.v, (float)dt));
            // Hard limits: out of obstacles, inside the bounds, on the ground or in the band.
            for (const Obstacle& ob : w.obstacles) {
                if (g.medium != Medium::Ground && l.p.y > ob.at.y + ob.height) continue;
                const Vector3 rel = flat(Vector3Subtract(l.p, ob.at));
                const float d = len(rel), r = ob.radius + 0.1f;
                if (d < r) l.p = Vector3Add({ ob.at.x, l.p.y, ob.at.z }, d > 1e-4f ? Vector3Scale(rel, r / d) : Vector3{ r, 0, 0 });
            }
            if (g.penRadius > 0) {
                const Vector3 rel = flat(Vector3Subtract(l.p, g.penAt));
                const float d = len(rel);
                if (d > g.penRadius) l.p = Vector3Add({ g.penAt.x, l.p.y, g.penAt.z }, Vector3Scale(rel, g.penRadius / d));
            }
            l.p.x = std::clamp(l.p.x, w.boundsMin.x, w.boundsMax.x);
            l.p.z = std::clamp(l.p.z, w.boundsMin.z, w.boundsMax.z);
            if (g.medium == Medium::Ground) l.p.y = 0;
            else l.p.y = std::clamp(l.p.y, w.boundsMin.y, w.boundsMax.y);
            l.dist += l.speed * dt;

            // Bank into turns (a coordinated turn: tan(bank) = v * yaw rate / g).
            if (g.medium != Medium::Ground) {
                const float rate = (l.yaw - yaw0) * DEG2RAD / (float)dt;
                float target = -std::atan(l.speed * rate / 9.81f) * RAD2DEG * (g.medium == Medium::Water ? 0.5f : 1.f);
                target = std::clamp(target, -50.f, 50.f);
                l.bank += (target - l.bank) * std::min(1.f, (float)dt * 4);
            }
        }
        // Walkers never overlap: pairs closer than their bodies allow are pushed apart, half
        // each (steering keeps this rare; it settles the jostle at goals and doorways).
        for (int pass = 0; pass < 2; pass++)
            for (int a = 0; a < n; a++) {
                const Group& ga = w.groups[(size_t)c->agents_[(size_t)a].group];
                if (ga.medium != Medium::Ground) continue;
                for (int b = a + 1; b < n; b++) {
                    const Group& gb = w.groups[(size_t)c->agents_[(size_t)b].group];
                    if (gb.medium != Medium::Ground) continue;
                    const float need = 0.425f * (ga.steer.personal + gb.steer.personal);
                    Vector3 d = Vector3Subtract(live[(size_t)b].p, live[(size_t)a].p);
                    d.y = 0;
                    const float dist = len(d);
                    if (dist >= need || dist < 1e-6f) continue;
                    const Vector3 push = Vector3Scale(d, 0.5f * (need - dist) / dist);
                    live[(size_t)a].p = Vector3Subtract(live[(size_t)a].p, push);
                    live[(size_t)b].p = Vector3Add(live[(size_t)b].p, push);
                }
            }
    }
    return c;
}

} // namespace crowd
} // namespace rmtn
