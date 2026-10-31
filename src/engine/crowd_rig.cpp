// crowd_rig.cpp -- crowds wearing rig characters: gaits that follow the baked speed, looks
// from a shared palette, and the 3D and 2D bindings.
#include "rmtn/crowd.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>

namespace rmtn {
namespace crowd {
namespace {

// One way of moving: its clip (played at clip time) and the ground it covers per second of
// clip time (0: in place -- standing, or flying where the body is carried by the node).
struct Gait {
    rig::Clip clip;
    float speed = 0;
    float lockCadence = 0;                   // > 0: a humanoid walk at this cadence, feet locked
};

enum class Kind { Humanoid, Quadruped, Biped, Chain };
Kind kindOf(const rig::Build& b) {
    if (!b.plan) return Kind::Humanoid;
    if (b.plan->kind == "biped") return Kind::Biped;
    if (b.plan->kind == "chain") return Kind::Chain;
    return Kind::Quadruped;
}

// Speed-picked state with hysteresis: moving gaits sorted by speed, switching between
// neighbours at the geometric mean of their speeds (8% either side), standing below a crawl.
// The first entry is the still state.
int pick(const std::vector<Gait>& gaits, int cur, float v, bool rest) {
    const float slowest = gaits.size() > 1 ? gaits[1].speed : 1;
    if (rest || (cur == 0 && v < slowest * 0.25f) || v < slowest * 0.12f) return 0;
    int k = std::max(cur, 1);
    while (k + 1 < (int)gaits.size() && v > std::sqrt(gaits[(size_t)k].speed * gaits[(size_t)k + 1].speed) * 1.08f) k++;
    while (k > 1 && v < std::sqrt(gaits[(size_t)k - 1].speed * gaits[(size_t)k].speed) * 0.92f) k--;
    return k;
}

std::vector<Gait> groundGaits(const rig::Build& b) {
    std::vector<Gait> g;
    switch (kindOf(b)) {
    case Kind::Humanoid:
        g.push_back({ rig::clips::idle(0.6f), 0 });
        g.push_back({ rig::clips::walk(1), rig::walkSpeed(b, 1), 1 });
        g.push_back({ rig::clips::run(1.4f), rig::runSpeed(b, 1.4f) });
        break;
    case Kind::Quadruped:
        g.push_back({ rig::clips::stand(b), 0 });
        for (rig::Gait k : { rig::Gait::Walk, rig::Gait::Trot, rig::Gait::Canter, rig::Gait::Gallop })
            g.push_back({ rig::clips::gait(b, k), rig::gaitSpeed(b, k) });
        break;
    case Kind::Biped:
    case Kind::Chain:
        g.push_back({ rig::clips::stand(b), 0 });
        for (rig::Gait k : { rig::Gait::Walk, rig::Gait::Trot })
            g.push_back({ rig::clips::gait(b, k), rig::gaitSpeed(b, k) });
        break;
    }
    return g;
}

// A palette entry: the outfit (built once and shared, so equal colours are one texture --
// rig caches outfit textures by id) and the skin it was painted with.
struct Wear { rig::Outfit outfit; Color skin; };
const Wear& paletteWear(int k) {
    static std::mutex mu;
    static std::map<int, Wear> cache;
    struct Entry { Color top, bottom, skin; float sleeve, trouser; };
    static const Entry kEntries[] = {
        { { 196, 64, 58, 255 }, { 46, 58, 92, 255 }, { 232, 186, 150, 255 }, 0.45f, 1 },
        { { 52, 120, 168, 255 }, { 220, 212, 192, 255 }, { 176, 122, 84, 255 }, 0.3f, 1 },
        { { 238, 190, 70, 255 }, { 60, 64, 72, 255 }, { 120, 80, 56, 255 }, 0.45f, 1 },
        { { 70, 140, 96, 255 }, { 88, 70, 56, 255 }, { 240, 200, 168, 255 }, 1.0f, 1 },
        { { 232, 230, 222, 255 }, { 40, 44, 60, 255 }, { 200, 150, 110, 255 }, 0.3f, 1 },
        { { 140, 84, 160, 255 }, { 36, 36, 42, 255 }, { 96, 64, 46, 255 }, 1.0f, 1 },
        { { 230, 128, 92, 255 }, { 210, 196, 168, 255 }, { 224, 176, 136, 255 }, 0.2f, 0.6f },
        { { 40, 44, 52, 255 }, { 120, 132, 150, 255 }, { 156, 108, 76, 255 }, 1.0f, 1 },
    };
    constexpr int n = (int)(sizeof kEntries / sizeof kEntries[0]);
    k = ((k % n) + n) % n;
    std::lock_guard<std::mutex> lock(mu);
    auto it = cache.find(k);
    if (it == cache.end()) {
        const Entry& e = kEntries[k];
        rig::Outfit o = rig::Outfit::basic(e.top, e.bottom, e.skin, e.sleeve, e.trouser);
        o.size = 256;                         // small on screen: a quarter of the default
        it = cache.emplace(k, Wear{ o, e.skin }).first;
    }
    return it->second;
}

Color tint(Color c, float k) {
    auto ch = [k](unsigned char v) { return (unsigned char)std::clamp((int)std::lround(v * k), 0, 255); };
    return { ch(c.r), ch(c.g), ch(c.b), c.a };
}

// rig2d's view space (rig::project's, with depth): towards the viewer is +z.
Vector3 viewSpace(const rig::View2D& v, Vector3 p) {
    const float y = -v.yaw * DEG2RAD, pt = v.pitch * DEG2RAD;
    const Vector3 a{ p.x * std::cos(y) + p.z * std::sin(y), p.y, -p.x * std::sin(y) + p.z * std::cos(y) };
    return { a.x, a.y * std::cos(pt) - a.z * std::sin(pt), a.y * std::sin(pt) + a.z * std::cos(pt) };
}

// The agent's node at time s: yaw, then position (as add() places it).
Matrix nodeAt(const Crowd& c, int agent, double s) {
    const State st = c.at(agent, s);
    return MatrixMultiply(MatrixRotateY(st.yaw * DEG2RAD), MatrixTranslate(st.pos.x, st.pos.y, st.pos.z));
}

// rig::clips::walk is a sine cycle in place: its feet drift while down and skim the floor as
// they land. Crowd walkers place their feet instead. Each foot's cycle follows the clip's
// phase: down while its leg swings back (phase pi/2..3pi/2 for the left, half a cycle later
// for the right), then swinging. A stance holds the ankle where it landed; a swing carries
// it on an arc from that footprint to the next. The landing times come from the clip time
// (the distance travelled is monotonic, so it inverts) and the footprints from the clip's
// ankle then, so both ends meet the clip's own pose. The leg reaches by IK; the clip's
// ankle height and the body's bob are kept. Still a pure function of time.
rig::Pose lockFeet(const Crowd& c, int agent, const rig::Build& b, const Gait& g, double phase, double segFrom, double s,
                   double tau, rig::Pose p) {
    const double period = 1.0 / g.lockCadence;
    const double d0 = c.travelled(agent, segFrom), dEnd = c.travelled(agent, s + 3);
    const Matrix toLocal = MatrixInvert(nodeAt(c, agent, s));
    const float lift = 0.09f * b.height() / 1.75f;
    // The time the clip reached clip time `t` (distance (t - phase) * speed), or -1 if never.
    auto when = [&](double t) {
        const double d = (t - phase) * g.speed;
        if (d < d0 || d > dEnd) return -1.0;
        double lo = segFrom, hi = s + 3;
        for (int k = 0; k < 40; k++) {
            const double mid = 0.5 * (lo + hi);
            (c.travelled(agent, mid) < d ? lo : hi) = mid;
        }
        return hi;
    };
    rig::Pose out = p;
    for (rig::Side side : { rig::Side::Left, rig::Side::Right }) {
        const rig::Joint ankle = side == rig::Side::Left ? rig::Joint::AnkleL : rig::Joint::AnkleR;
        auto print = [&](double t, double at) {                   // the clip's ankle in the world
            return Vector3Transform(rig::position(rig::solve(b, g.clip(t)), ankle), nodeAt(c, agent, at));
        };
        const double ph = 6.283185307 * g.lockCadence * tau + (side == rig::Side::Right ? 3.14159265 : 0);
        const double u = std::fmod(std::fmod((ph - 1.5707963) / 6.283185307, 1.0) + 1.0, 1.0);   // 0 = landing
        const double tDown = tau - u * period;
        const double sDown = when(tDown);
        Vector3 target;
        float k = 1;
        if (u < 0.5) {                                            // down
            if (sDown < 0) continue;                              // it landed before this gait began
            target = print(tDown, sDown);
        } else {                                                  // swinging to the next footprint
            const double tNext = tDown + period, sNext = when(tNext);
            if (sNext < 0) continue;                              // it won't land in this gait
            const float v = (float)((u - 0.5) / 0.5), e = v * v * (3 - 2 * v);
            const Vector3 to = print(tNext, sNext);
            Vector3 from = to;
            if (sDown >= 0) from = print(tDown, sDown);
            else k = e;                                           // no footprint yet: ease into the lock
            target = Vector3Lerp(from, to, e);
            target.y += lift * std::sin(3.14159265f * v);
        }
        const rig::Pose placed = rig::plant(b, out, side, Vector3Transform(target, toLocal));
        out = k >= 1 ? placed : rig::Pose::mix(out, placed, k);
    }
    return out;
}

} // namespace

void locomote(rig::Motion& m, CrowdPtr c, int agent, const rig::Build& b) {
    const World& w = c->world();
    const Group& g = c->groupOf(agent);
    const double phase = rnd01(w.seed ^ 0xa11ce5ULL, (unsigned long long)agent) * 2;   // out of step
    const bool flying = g.medium == Medium::Air && kindOf(b) == Kind::Biped;

    // The states, then a timeline of them sampled at 15 Hz, each held at least 0.5 s.
    std::vector<Gait> gaits;
    if (flying) {
        const float rate = rig::flapRate(b) * (float)(0.9 + 0.2 * rnd01(w.seed ^ 0xf1a9ULL, (unsigned long long)agent));
        gaits = { { rig::clips::glide(b), 0 }, { rig::clips::fly(b, rate), 0 } };
    } else {
        gaits = groundGaits(b);
    }
    struct Seg { int state; double from; };
    std::vector<Seg> segs;
    int cur = -1;
    double since = -1e9;
    for (double s = w.from; s <= w.to + 1e-9; s += 1.0 / 15) {
        const State st = c->at(agent, s);
        int next;
        if (flying) {
            // Glide while descending, and in seeded spells when cruising level; flap otherwise.
            const double win = std::floor((s + phase * 3) / 2.5);
            const bool spell = rnd01(w.seed ^ 0x611deULL, (unsigned long long)agent * 131ULL + (unsigned long long)(long long)win) < 0.3;
            const float vy = st.vel.y;
            next = (cur == 0 ? vy < 0.05f : vy < -0.25f) || (spell && std::fabs(vy) < 0.2f) ? 0 : 1;
        } else {
            next = pick(gaits, cur < 0 ? 0 : cur, st.speed, c->resting(agent, s));
            if (cur < 0 && st.speed > 0.05f && next == 0) next = 1;
        }
        if (next != cur && s - since >= 0.5) {
            segs.push_back({ next, s });
            cur = next;
            since = s;
        }
    }
    for (size_t i = 0; i < segs.size(); i++) {
        const Gait gt = gaits[(size_t)segs[i].state];
        const double from = i == 0 ? w.from - 10 : segs[i].from;   // the first covers any time before
        rig::Clip clip;
        if (gt.lockCadence > 0 && g.medium == Medium::Ground) {
            const double segFrom = std::max(from, w.from);
            clip = [c, agent, from, segFrom, gt, phase, b](double local) {
                const double s = from + local, tau = c->travelled(agent, s) / gt.speed + phase;
                return lockFeet(*c, agent, b, gt, phase, segFrom, s, tau, gt.clip(tau));
            };
        } else if (gt.speed > 0) {
            // Clip time follows the ground covered: one stride per stride length at any speed.
            clip = [c, agent, from, gt, phase](double local) {
                return gt.clip(c->travelled(agent, from + local) / gt.speed + phase);
            };
        } else {
            clip = [gt, from, phase](double local) { return gt.clip(from + local + phase); };
        }
        m.play(clip, from, i == 0 ? 0 : 0.35);
    }
}

std::vector<rig::CharacterPtr> cast(CrowdPtr c, const std::string& group, std::function<rig::Look(int i)> look, int faces) {
    const int a0 = c->first(group);
    const int n = c->groupOf(a0).count;
    std::vector<rig::CharacterPtr> out;
    out.reserve((size_t)n);
    for (int i = 0; i < n; i++) {
        rig::Look l = look(i);
        l.build = l.build.scaled(c->sizeFactor(a0 + i));
        if (i >= faces) l.face = nullptr;
        auto ch = std::make_shared<rig::Character>(l);
        locomote(ch->motion, c, a0 + i, l.build);
        out.push_back(ch);
    }
    return out;
}

rig::Look person(int i, unsigned long long seed) {
    auto r = [&](int k) { return rnd01(seed, (unsigned long long)i * 31ULL + (unsigned long long)k); };
    static const char* const kHair[] = { "hair.short", "hair.bob", "hair.bun", "hair.ponytail", "hair.long", "hair.spiky", "hair.cap" };
    static const Color kHairColor[] = { { 40, 30, 26, 255 }, { 92, 58, 34, 255 }, { 182, 140, 86, 255 },
                                        { 150, 70, 40, 255 }, { 120, 116, 112, 255 } };
    rig::Look l = rig::Look::toon();
    l.build = rig::Build::adult().scaled((float)(0.92 + 0.12 * r(1)));
    l.build.head = 0.34f * l.build.height() / 1.75f;
    l.hair = kHair[(int)(r(2) * 7) % 7];
    l.hairColor = kHairColor[(int)(r(3) * 5) % 5];
    const Wear& wear = paletteWear((int)(r(4) * 8));
    l.outfit = wear.outfit;
    l.skin = wear.skin;
    l.feet(r(5) < 0.5 ? "foot.boot" : "foot.shoe");
    return l;
}

rig::Look herd(const std::string& species, int i, unsigned long long seed) {
    auto r = [&](int k) { return (float)rnd01(seed, (unsigned long long)i * 37ULL + (unsigned long long)k); };
    rig::Look l = rig::animal(species);
    if (species == "horse") {
        // Bay, chestnut, black, grey, palomino.
        static const Color kCoat[] = { { 138, 82, 46, 255 }, { 168, 92, 46, 255 }, { 44, 38, 36, 255 },
                                       { 186, 182, 176, 255 }, { 214, 176, 112, 255 } };
        l.skin = kCoat[(int)(r(1) * 5) % 5];
    } else {
        l.skin = tint(l.skin, 0.88f + 0.2f * r(1));
    }
    l.hairColor = tint(l.hairColor, 0.85f + 0.3f * r(2));
    l.outfit.fallback = l.skin;
    return l;
}

rig::Look zombie(int i, unsigned long long seed) {
    auto r = [&](int k) { return (float)rnd01(seed, (unsigned long long)i * 41ULL + (unsigned long long)k); };
    static const char* const kHair[] = { "hair.spiky", "hair.short", "hair.long", "hair.bun" };
    rig::Look l = rig::Look::toon();
    l.build = rig::Build::adult().scaled(0.94f + 0.1f * r(1));
    l.build.head = 0.34f * l.build.height() / 1.75f;
    l.hair = kHair[(int)(r(2) * 4) % 4];
    l.hairColor = { (unsigned char)(34 + 30 * r(3)), (unsigned char)(30 + 24 * r(3)), 26, 255 };
    // Skin the grey-green of the dead; rags in the faded colours of whatever they wore.
    const Color skin{ (unsigned char)(118 + 30 * r(4)), (unsigned char)(140 + 26 * r(4)), (unsigned char)(104 + 20 * r(4)), 255 };
    static const Color kTop[] = { { 96, 70, 62, 255 }, { 72, 84, 92, 255 }, { 110, 104, 80, 255 }, { 84, 62, 84, 255 } };
    static const Color kBottom[] = { { 50, 52, 62, 255 }, { 66, 58, 48, 255 }, { 44, 60, 56, 255 } };
    l.outfit = rig::Outfit::basic(kTop[(int)(r(5) * 4) % 4], kBottom[(int)(r(6) * 3) % 3], skin, 0.15f + 0.4f * r(7), 0.8f);
    l.skin = skin;
    l.feet("foot.shoe");
    return l;
}

namespace {
// One body of a baked mesh's data, built a frame at a time with a fixed topology.
struct Soup {
    std::vector<float> pos, nrm;
    std::vector<unsigned short> idx;
    std::vector<unsigned char> col;
    void vertex(Vector3 p, Vector3 n, Color c) {
        pos.insert(pos.end(), { p.x, p.y, p.z });
        nrm.insert(nrm.end(), { n.x, n.y, n.z });
        col.insert(col.end(), { c.r, c.g, c.b, c.a });
    }
    unsigned short count() const { return (unsigned short)(pos.size() / 3); }
    // An ellipsoid with radii `r` at `at`, `lat` rings by `lon` slices.
    void ellipsoid(Vector3 at, Vector3 r, int lat, int lon, Color c, bool topology) {
        const unsigned short base = count();
        for (int a = 0; a <= lat; a++) {
            const float phi = PI * a / lat - PI / 2;
            for (int b = 0; b <= lon; b++) {
                const float th = 2 * PI * b / lon;
                const Vector3 u{ std::cos(phi) * std::cos(th), std::sin(phi), std::cos(phi) * std::sin(th) };
                const Vector3 n = Vector3Normalize({ u.x / r.x, u.y / r.y, u.z / r.z });
                vertex(Vector3Add(at, { u.x * r.x, u.y * r.y, u.z * r.z }), n, c);
            }
        }
        if (!topology) return;
        for (int a = 0; a < lat; a++)
            for (int b = 0; b < lon; b++) {
                const auto i0 = (unsigned short)(base + a * (lon + 1) + b), i1 = (unsigned short)(i0 + 1);
                const auto i2 = (unsigned short)(i0 + lon + 1), i3 = (unsigned short)(i2 + 1);
                idx.insert(idx.end(), { i0, i1, i3, i0, i3, i2 });
            }
    }
    // A flat triangle, facing the side its winding does and then the other, so a membrane
    // shows from both (the shader culls back faces).
    void membrane(Vector3 a, Vector3 b, Vector3 c, Color col3, bool topology) {
        const Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
        const unsigned short base = count();
        vertex(a, n, col3); vertex(b, n, col3); vertex(c, n, col3);
        const Vector3 m = Vector3Negate(n);
        vertex(a, m, col3); vertex(c, m, col3); vertex(b, m, col3);
        if (topology) idx.insert(idx.end(), { base, (unsigned short)(base + 1), (unsigned short)(base + 2),
                                              (unsigned short)(base + 3), (unsigned short)(base + 4), (unsigned short)(base + 5) });
    }
};

// A bat at a moment of its wingbeat (phase 0..1): lying along +z, wings out along +-x.
void batPose(Soup& s, float phase, Color body, Color membrane, bool topology) {
    const float beat = std::sin(2 * PI * phase);
    s.ellipsoid({ 0, 0, 0 }, { 0.032f, 0.028f, 0.065f }, 5, 8, body, topology);              // body
    s.ellipsoid({ 0, 0.008f, 0.085f }, { 0.024f, 0.022f, 0.026f }, 4, 6, body, topology);    // head
    for (int side = -1; side <= 1; side += 2) {                                               // ears
        const float sx = (float)side;
        s.membrane({ sx * 0.010f, 0.025f, 0.088f }, { sx * 0.026f, 0.058f, 0.082f }, { sx * 0.022f, 0.022f, 0.096f }, body, topology);
    }
    for (int side = -1; side <= 1; side += 2) {
        const float sx = (float)side;
        // Shoulder, wrist (the elbow's lift), three finger tips lagging the beat, the hip.
        const float lift = 52 * DEG2RAD * beat, lag = 52 * DEG2RAD * std::sin(2 * PI * phase - 0.9f) - 0.18f;
        const Vector3 sh{ sx * 0.03f, 0.008f, 0.025f }, hip{ sx * 0.028f, -0.004f, -0.06f };
        const Vector3 wr = Vector3Add(sh, { sx * 0.12f * std::cos(lift), 0.12f * std::sin(lift), -0.01f });
        auto tip = [&](float len, float spread, float extra) {
            const float e = lag + extra;
            return Vector3Add(wr, { sx * len * std::cos(e) * std::cos(spread), len * std::sin(e), -len * std::sin(spread) - 0.01f });
        };
        const Vector3 t1 = tip(0.19f, 0.28f, 0.05f), t2 = tip(0.17f, 0.75f, -0.02f), t3 = tip(0.12f, 1.2f, -0.08f);
        s.membrane(sh, wr, t1, membrane, topology);
        s.membrane(sh, t1, t2, membrane, topology);
        s.membrane(sh, t2, t3, membrane, topology);
        s.membrane(sh, t3, hip, membrane, topology);
    }
}

// A sphere (in the agent's own space) round everything the character does during the bake:
// its joints over every pose it takes on, sampled every 0.15 s, then padded for the limbs'
// thickness and what falls between samples. Out of sight, the agent is culled whole.
void boundOf(const World& w, const rig::Character& ch, bool tilts, Vector3& centre, float& radius) {
    std::vector<Vector3> pts;
    Vector3 lo{ 1e9f, 1e9f, 1e9f }, hi{ -1e9f, -1e9f, -1e9f };
    for (double s = w.from; s <= w.to + 1e-9; s += 0.15) {
        const rig::Build& b = ch.lookAt(s).build;
        const rig::Frames f = rig::solve(b, ch.poseAt(s));
        for (int i = 0; i < rig::jointCount(b); i++) {
            const Vector3 p = rig::position(f, (rig::Joint)i);
            pts.push_back(p);
            lo = Vector3Min(lo, p);
            hi = Vector3Max(hi, p);
        }
    }
    centre = Vector3Scale(Vector3Add(lo, hi), 0.5f);
    float r = 0;
    for (const Vector3& p : pts) r = std::max(r, Vector3Distance(centre, p));
    radius = r * 1.1f + 0.3f;
    // A flyer or swimmer pitches and banks about its origin, which carries the centre round it.
    if (tilts) {
        radius += Vector3Length(centre);
        centre = { 0, 0, 0 };
    }
}
} // namespace

g3d::Object3D* add(g3d::Scene& scene, CrowdPtr c, int agent, rig::CharacterPtr ch, int fps) {
    g3d::Object3D* holder = scene.add();
    holder->name = "crowd-agent";
    g3d::Object3D* tilt = holder->add();
    rig::add(*tilt, ch, fps);
    const bool tilts = c->groupOf(agent).medium != Medium::Ground;
    boundOf(c->world(), *ch, tilts, holder->cullCentre, holder->cullRadius);
    holder->up = [c, agent, fps](int t, g3d::Object3D& o) {
        const State st = c->at(agent, t / (double)fps);
        o.position = st.pos;
        o.rotation = { 0, st.yaw, 0 };
    };
    if (tilts)
        tilt->up = [c, agent, fps](int t, g3d::Object3D& o) {
            const State st = c->at(agent, t / (double)fps);
            o.rotation = { st.pitch, 0, st.bank };
        };
    return holder;
}

Baked bakeWalker(const rig::Look& look, rig::Clip walk, double walkSeconds, rig::Clip idle, double idleSeconds,
                 float stride, int walkFrames, int idleFrames) {
    Baked b;
    std::vector<rig::BakedClip> where;
    b.mesh = rig::bakeAnimated(look, { { std::move(idle), idleFrames, idleSeconds }, { std::move(walk), walkFrames, walkSeconds } }, &where);
    b.still = where[0];
    b.move = where[1];
    b.stillSeconds = idleSeconds;
    b.stride = stride;
    return b;
}

Baked bakeBat(int frames, Color bodyColor, Color membraneColor) {
    Baked b;
    auto am = std::make_shared<g3d::AnimatedMesh>();
    auto topo = std::make_shared<Soup>();
    Vector3 lo{ 1e9f, 1e9f, 1e9f }, hi{ -1e9f, -1e9f, -1e9f };
    for (int f = 0; f < frames; f++) {
        Soup s;
        batPose(s, (float)f / frames, bodyColor, membraneColor, f == 0);
        if (f == 0) *topo = s;
        am->vertices = (int)(s.pos.size() / 3);
        am->positions.insert(am->positions.end(), s.pos.begin(), s.pos.end());
        am->normals.insert(am->normals.end(), s.nrm.begin(), s.nrm.end());
        for (size_t i = 0; i + 2 < s.pos.size(); i += 3) {
            lo = Vector3Min(lo, { s.pos[i], s.pos[i + 1], s.pos[i + 2] });
            hi = Vector3Max(hi, { s.pos[i], s.pos[i + 1], s.pos[i + 2] });
        }
    }
    am->frames = frames;
    am->centre = Vector3Scale(Vector3Add(lo, hi), 0.5f);
    am->radius = Vector3Distance(lo, hi) * 0.5f + 0.02f;
    am->rest = std::make_shared<g3d::Geometry>([topo] {
        Mesh m{};
        m.vertexCount = (int)topo->pos.size() / 3;
        m.triangleCount = (int)topo->idx.size() / 3;
        m.vertices = (float*)MemAlloc((unsigned)(topo->pos.size() * sizeof(float)));
        m.normals = (float*)MemAlloc((unsigned)(topo->nrm.size() * sizeof(float)));
        m.texcoords = (float*)MemAlloc((unsigned)(m.vertexCount * 2 * sizeof(float)));
        m.colors = (unsigned char*)MemAlloc((unsigned)topo->col.size());
        m.indices = (unsigned short*)MemAlloc((unsigned)(topo->idx.size() * sizeof(unsigned short)));
        std::memcpy(m.vertices, topo->pos.data(), topo->pos.size() * sizeof(float));
        std::memcpy(m.normals, topo->nrm.data(), topo->nrm.size() * sizeof(float));
        std::memset(m.texcoords, 0, (size_t)m.vertexCount * 2 * sizeof(float));
        std::memcpy(m.colors, topo->col.data(), topo->col.size());
        std::memcpy(m.indices, topo->idx.data(), topo->idx.size() * sizeof(unsigned short));
        UploadMesh(&m, true);
        return m;
    });
    b.mesh = am;
    b.still = { 0, frames };
    b.move = { 0, frames };
    b.stillSeconds = 1;
    return b;
}

Cycle walkCycle(CrowdPtr c, const std::string& group, int index, double s, const Baked& body) {
    const int a = c->first(group) + index;
    const State st = c->at(a, s);
    if (st.speed < 0.12f || c->resting(a, s)) {
        const double u = std::fmod(s + 1.7 * rnd01(78, (unsigned long long)index), body.stillSeconds) / body.stillSeconds;
        return { (float)(u - std::floor(u)), true };
    }
    const double cycles = c->travelled(a, s) / (double)(body.stride * c->sizeFactor(a)) + rnd01(79, (unsigned long long)index);
    return { (float)(cycles - std::floor(cycles)), false };
}

float flapCycle(CrowdPtr c, const std::string& group, int index, double s, float flapsPerSecond) {
    (void)c; (void)group;
    const double cycles = s * flapsPerSecond * (0.9 + 0.2 * rnd01(81, (unsigned long long)index)) + rnd01(82, (unsigned long long)index);
    return (float)(cycles - std::floor(cycles));
}

g3d::Object3D* addInstanced(g3d::Scene& scene, CrowdPtr c, const std::string& group, const std::vector<Baked>& bodies,
                            g3d::Material material, int fps) {
    if (bodies.empty()) throw std::runtime_error("crowd::addInstanced: no bodies");
    const int a0 = c->first(group), n = c->groupOf(a0).count;
    g3d::Object3D* holder = scene.add();
    holder->name = "crowd-instances";
    for (size_t v = 0; v < bodies.size(); v++) {
        const Baked body = bodies[v];
        auto set = std::make_shared<g3d::Instances>();
        set->mesh = body.mesh;
        g3d::Object3D* o = holder->add(set, material);
        o->name = "crowd-bodies";
        // Agents are dealt out by a hash of their index, so each body is spread through the crowd.
        std::vector<int> mine;
        for (int i = 0; i < n; i++)
            if ((size_t)(rnd01(77, (unsigned long long)i) * (double)bodies.size()) % bodies.size() == v) mine.push_back(i);
        o->up = [c, set, body, mine, a0, group, fps](int t, g3d::Object3D&) {
            const double s = t / (double)fps;
            set->list.clear();
            set->list.reserve(mine.size());
            for (const int i : mine) {
                const State st = c->at(a0 + i, s);
                g3d::Instance in;
                in.position = st.pos;
                in.yaw = st.yaw;
                in.scale = c->sizeFactor(a0 + i);
                const Cycle cy = walkCycle(c, group, i, s, body);
                const rig::BakedClip& clip = cy.still ? body.still : body.move;
                in.clipStart = clip.start;
                in.clipLength = clip.length;
                in.frame = cy.phase * clip.length;
                in.shade = 0.88f + 0.24f * (float)rnd01(80, (unsigned long long)i);
                set->list.push_back(in);
            }
        };
    }
    return holder;
}

g3d::Object3D* addFlock(g3d::Scene& scene, CrowdPtr c, const std::string& group, const Baked& body, float flapsPerSecond,
                        g3d::Material material, int fps) {
    const int a0 = c->first(group), n = c->groupOf(a0).count;
    auto set = std::make_shared<g3d::Instances>();
    set->mesh = body.mesh;
    g3d::Object3D* o = scene.add(set, material);
    o->name = "crowd-flock";
    const Baked b = body;
    o->up = [c, set, b, a0, n, group, flapsPerSecond, fps](int t, g3d::Object3D&) {
        const double s = t / (double)fps;
        set->list.clear();
        set->list.reserve((size_t)n);
        for (int i = 0; i < n; i++) {
            const State st = c->at(a0 + i, s);
            g3d::Instance in;
            in.position = st.pos;
            in.yaw = st.yaw;
            in.pitch = st.pitch;
            in.bank = st.bank;
            in.scale = c->sizeFactor(a0 + i);
            in.clipStart = b.move.start;
            in.clipLength = b.move.length;
            in.frame = flapCycle(c, group, i, s, flapsPerSecond) * b.move.length;
            in.shade = 0.85f + 0.3f * (float)rnd01(83, (unsigned long long)i);
            set->list.push_back(in);
        }
    };
    return o;
}

void addAll(g3d::Scene& scene, CrowdPtr c, const std::string& group, const std::vector<rig::CharacterPtr>& chars, int fps) {
    const int a0 = c->first(group);
    for (size_t k = 0; k < chars.size(); k++) add(scene, c, a0 + (int)k, chars[k], fps);
}

void draw(const Ctx& cx, CrowdPtr c, const std::string& group, const std::vector<rig::CharacterPtr>& chars, double s,
          const rig::View2D& view) {
    const int a0 = c->first(group);
    struct Item { float depth; size_t k; State st; };
    std::vector<Item> items;
    for (size_t k = 0; k < chars.size(); k++) {
        const State st = c->at(a0 + (int)k, s);
        items.push_back({ viewSpace(view, st.pos).z, k, st });
    }
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.depth < b.depth; });
    for (const Item& it : items) {
        rig::View2D v = view;
        v.origin = rig::project(view, it.st.pos);
        v.yaw = view.yaw - it.st.yaw;
        rig::draw(cx, *chars[it.k], s, v);
    }
}

} // namespace crowd
} // namespace rmtn
