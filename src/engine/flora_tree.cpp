// flora:: trees -- skeletons, bark and leaf meshes, baked wind, impostors and forests
// (docs/flora-plan.md, M3). Everything here runs on the CPU and is a function of the spec.
#include "rmtn/flora.hpp"
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace rmtn {
namespace flora {

namespace {

constexpr float TAU = 6.28318531f;

struct Rng {
    uint64_t s;
    uint64_t next() {
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    float unit() { return (float)(next() >> 40) / 16777216.f; }                // [0, 1)
    float range(float a, float b) { return a + (b - a) * unit(); }
    Vector3 ball() {                                                               // in the unit ball
        for (;;) {
            const Vector3 p{ range(-1, 1), range(-1, 1), range(-1, 1) };
            if (Vector3LengthSqr(p) <= 1) return p;
        }
    }
};

float hashUnit(uint32_t a, uint32_t b) {
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return (float)(h >> 8) / 16777216.f;
}

float smoothstep(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}

Color mixColor(Color a, Color b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    auto m = [t](unsigned char x, unsigned char y) { return (unsigned char)std::lround(x + (y - x) * t); };
    return { m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), 255 };
}
Color scaleColor(Color c, float k) {
    auto s = [k](unsigned char x) { return (unsigned char)std::clamp(std::lround(x * k), 0l, 255l); };
    return { s(c.r), s(c.g), s(c.b), c.a };
}

// ---- pictures (grey, so the vertex colour carries the hue), RGBA8 -------------------------
struct Picture {
    int w = 0, h = 0;
    std::vector<unsigned char> px;     // rgba
    Picture(int w_, int h_) : w(w_), h(h_), px((size_t)w_ * h_ * 4, 0) {}
    unsigned char* at(int x, int y) { return &px[((size_t)y * w + x) * 4]; }
    const unsigned char* at(int x, int y) const { return &px[((size_t)y * w + x) * 4]; }
    // Bilinear, wrapping (uv in texture space, v down).
    Vector4 sample(float u, float v) const {
        const float x = u * w - 0.5f, y = v * h - 0.5f;
        const int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
        const float fx = x - x0, fy = y - y0;
        Vector4 out{ 0, 0, 0, 0 };
        for (int j = 0; j < 2; j++)
            for (int i = 0; i < 2; i++) {
                const int xi = ((x0 + i) % w + w) % w, yi = ((y0 + j) % h + h) % h;
                const float k = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
                const unsigned char* p = at(xi, yi);
                out.x += k * p[0]; out.y += k * p[1]; out.z += k * p[2]; out.w += k * p[3];
            }
        return { out.x / 255, out.y / 255, out.z / 255, out.w / 255 };
    }
};

// A texture made once from a picture (the GPU copy is made on first use).
g3d::DynamicTexturePtr texture(std::shared_ptr<const Picture> pic) {
    auto t = std::make_shared<g3d::DynamicTexture>();
    t->every = 1 << 30;
    t->make = [pic](int) {
        Image img{};
        img.width = pic->w; img.height = pic->h; img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        img.data = MemAlloc((unsigned)pic->px.size());
        std::memcpy(img.data, pic->px.data(), pic->px.size());
        return img;
    };
    return t;
}

// Distance from p to the segment ab, and where along it (0..1).
float segment(Vector2 p, Vector2 a, Vector2 b, float& along) {
    const Vector2 ab = Vector2Subtract(b, a);
    along = std::clamp(Vector2DotProduct(Vector2Subtract(p, a), ab) / std::max(Vector2DotProduct(ab, ab), 1e-6f), 0.f, 1.f);
    return Vector2Distance(p, Vector2Add(a, Vector2Scale(ab, along)));
}

// Paints a picture from a coverage-and-value function, 2x2 supersampled.
template <class F> std::shared_ptr<Picture> paint(int w, int h, F f) {
    auto pic = std::make_shared<Picture>(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float cover = 0, value = 0;
            for (int j = 0; j < 2; j++)
                for (int i = 0; i < 2; i++) {
                    float v = 0;
                    if (f((x + (i + 0.5f) / 2) / w, (y + (j + 0.5f) / 2) / h, v)) { cover += 1; value += v; }
                }
            unsigned char* p = pic->at(x, y);
            const float g = cover > 0 ? value / cover : 0;
            p[0] = p[1] = p[2] = (unsigned char)std::clamp(std::lround(g * 255), 0l, 255l);
            p[3] = (unsigned char)std::lround(cover / 4 * 255);
        }
    // Spread colour into the clear texels so mipmaps don't darken the edges.
    for (int pass = 0; pass < 8; pass++) {
        std::vector<unsigned char> next = pic->px;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                if (pic->at(x, y)[3] > 0 || next[((size_t)y * w + x) * 4 + 1] > 0) continue;
                int sum = 0, n = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        const int xx = x + dx, yy = y + dy;
                        if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                        const unsigned char* q = pic->at(xx, yy);
                        if (q[1] > 0) { sum += q[1]; n++; }
                    }
                if (n) { const unsigned char g = (unsigned char)(sum / n); unsigned char* o = &next[((size_t)y * w + x) * 4]; o[0] = o[1] = o[2] = g; }
            }
        pic->px.swap(next);
    }
    return pic;
}

// Tapered strokes (needles, leaflets, twigs): precomputed, so painting is a bounds test and a
// segment distance per stroke.
struct Stroke { Vector2 a, b; float r0, r1, tone; Vector2 lo, hi; };
Stroke stroke(Vector2 a, Vector2 b, float r0, float r1, float tone) {
    const float r = std::max(r0, r1);
    return { a, b, r0, r1, tone, { std::min(a.x, b.x) - r, std::min(a.y, b.y) - r }, { std::max(a.x, b.x) + r, std::max(a.y, b.y) + r } };
}
bool inStrokes(const std::vector<Stroke>& list, float u, float v, float& value) {
    for (const Stroke& s : list) {
        if (u < s.lo.x || v < s.lo.y || u > s.hi.x || v > s.hi.y) continue;
        float along;
        if (segment({ u, v }, s.a, s.b, along) < s.r0 + (s.r1 - s.r0) * along) { value = s.tone; return true; }
    }
    return false;
}

std::shared_ptr<Picture> leafPicture(LeafShape shape, uint32_t seed) {
    Rng r{ seed * 7919ull + 3 };
    if (shape == LeafShape::Broad) {
        // A twig up the middle with leaves on alternate sides and one at the tip.
        struct Leaf { Vector2 base, axis; float length, width, tone; Vector2 lo, hi; };
        std::vector<Leaf> leaves;
        auto leaf = [&](Vector2 base, float angle, float length, float width, float tone) {
            const Vector2 axis{ std::sin(angle), -std::cos(angle) };
            const Vector2 tip = Vector2Add(base, Vector2Scale(axis, length));
            const float m = width * length * 0.5f;
            leaves.push_back({ base, axis, length, width, tone, { std::min(base.x, tip.x) - m, std::min(base.y, tip.y) - m },
                               { std::max(base.x, tip.x) + m, std::max(base.y, tip.y) + m } });
        };
        for (int i = 0; i < 6; i++) {
            const float t = 0.18f + 0.13f * i, side = i % 2 ? 1.f : -1.f;
            leaf({ 0.5f, 1 - t * 0.85f }, side * r.range(0.6f, 1.0f), r.range(0.28f, 0.36f), r.range(0.38f, 0.46f), r.range(0.84f, 1.0f));
        }
        leaf({ 0.5f, 0.22f }, r.range(-0.15f, 0.15f), 0.21f, 0.44f, 0.95f);
        std::vector<Stroke> twig{ stroke({ 0.5f, 1 }, { 0.5f, 0.22f }, 0.014f, 0.006f, 0.5f) };
        // The leaf outline, a function of the way along (0..1): round shouldered, pointed.
        std::array<float, 65> outline{};
        for (int i = 0; i <= 64; i++) outline[(size_t)i] = 0.5f * std::pow(std::sin(3.14159f * std::pow(i / 64.f, 0.8f)), 0.85f);
        return paint(256, 256, [leaves, twig, outline](float u, float v, float& value) {
            for (const Leaf& l : leaves) {
                if (u < l.lo.x || v < l.lo.y || u > l.hi.x || v > l.hi.y) continue;
                const Vector2 d{ u - l.base.x, v - l.base.y };
                const float a = Vector2DotProduct(d, l.axis) / l.length, b = (d.x * l.axis.y - d.y * l.axis.x) / l.length;
                if (a < 0 || a > 1) continue;
                const float half = l.width * outline[(size_t)std::lround(a * 64)];
                if (std::fabs(b) > half) continue;
                const float vein = std::fabs(b) < 0.016f ? 0.8f : 1.f;
                value = l.tone * vein * (b > 0 ? 1.f : 0.9f) * (1 - 0.12f * std::fabs(b) / std::max(half, 1e-3f));
                return true;
            }
            return inStrokes(twig, u, v, value);
        });
    }
    std::vector<Stroke> strokes;
    if (shape == LeafShape::Needles) {
        // A twig with needles both sides, angled towards its tip.
        strokes.push_back(stroke({ 0.5f, 1 }, { 0.5f, 0.06f }, 0.012f, 0.006f, 0.55f));
        for (int i = 0; i < 40; i++) {
            const float t = 0.06f + 0.88f * i / 40.f, side = i % 2 ? 1.f : -1.f;
            const Vector2 a{ 0.5f + r.range(-0.01f, 0.01f), 1 - t }, dir = Vector2Normalize({ side * r.range(0.6f, 0.8f), -0.7f });
            const float len = r.range(0.17f, 0.27f) * (1 - 0.5f * std::max(0.f, t - 0.7f) / 0.3f);
            strokes.push_back(stroke(a, Vector2Add(a, Vector2Scale(dir, len)), 0.0075f, 0.003f, r.range(0.74f, 0.96f)));
        }
    } else {
        // Frond: a rachis up the middle (base at the bottom) and leaflets slanting to the tip.
        strokes.push_back(stroke({ 0.5f, 1 }, { 0.5f, 0 }, 0.016f, 0.008f, 0.62f));
        for (int i = 0; i < 52; i++) {
            const float s = (i + 0.5f) / 52.f, side = i % 2 ? 1.f : -1.f;
            const Vector2 a{ 0.5f, 1 - s };
            const float len = 0.5f * std::sin(3.14159f * std::pow(s, 0.7f)) * 0.95f;
            const Vector2 b{ 0.5f + side * len * 0.93f, 1 - s - len * 0.35f };
            strokes.push_back(stroke(a, b, 0.013f, 0.004f, r.range(0.78f, 0.95f)));
        }
    }
    return paint(256, 256, [strokes](float u, float v, float& value) { return inStrokes(strokes, u, v, value); });
}

std::shared_ptr<Picture> barkPicture(const TreeSpec& s) {
    const uint32_t seed = s.seed;
    const float marks = s.barkMarks, rings = s.barkRings;
    return paint(128, 256, [seed, marks, rings](float u, float v, float& value) {
        const float a = u * TAU;
        const float wob = 1.4f * noise(std::cos(a) * 3 + v * 6, std::sin(a) * 3, 1.5f, seed);
        float g = 0.66f + 0.2f * std::sin(7 * a + wob * 3) - 0.1f * noise(std::cos(a) * 9, std::sin(a) * 9 + v * 30, 2, seed + 1);
        if (rings > 0) g *= 1 - rings * 0.45f * std::pow(0.5f + 0.5f * std::cos(v * TAU * 6 + wob), 6.f);
        if (marks > 0) {
            const float row = v * 14, band = std::fabs(row - std::floor(row) - 0.5f);
            const float lenticel = noise(std::cos(a) * 2, std::sin(a) * 2 + std::floor(row) * 7.3f, 1, seed + 2);
            if (band < 0.06f && lenticel > 0.55f) g = 0.3f;
            const float patch = noise(std::cos(a) * 4 + v * 3, std::sin(a) * 4, 1.2f, seed + 3);
            if (patch > 0.78f) g = std::min(g, 0.35f);
            g = 0.25f + g * (0.75f + 0.25f * (1 - marks));
        }
        value = std::clamp(g, 0.f, 1.f);
        return true;
    });
}

// ---- the skeleton -------------------------------------------------------------------------
struct Node {
    Vector3 p;
    int parent = -1;
    float radius = 0;
    int children = 0;
    int branch = 0;          // which first branch it hangs from (0 = the stem)
    float reach = 0;         // metres along its branch from the stem
    bool stem = false;
};

struct Skeleton {
    std::vector<Node> nodes;
    float H = 1;
    Vector3 crownCentre{ 0, 0, 0 };
    Vector3 crownRadii{ 1, 1, 1 };
};

int addNode(Skeleton& sk, Vector3 p, int parent) {
    Node n;
    n.p = p;
    n.parent = parent;
    if (parent >= 0) sk.nodes[(size_t)parent].children++;
    sk.nodes.push_back(n);
    return (int)sk.nodes.size() - 1;
}

// How far from the axis the crown may reach at height y (broadleaf envelope).
bool inCrown(const TreeSpec& s, const Skeleton& sk, Vector3 p) {
    const float yb = s.crownBase * s.height, top = s.height;
    if (p.y < yb || p.y > top) return false;
    const float t = (p.y - yb) / std::max(top - yb, 1e-3f);                 // 0 crown base .. 1 top
    if (s.crownLength > 0)
        return std::fabs(p.x) <= s.crownLength * 0.5f && std::fabs(p.z) <= s.crownRadius;
    const float egg = std::sqrt(std::max(0.f, 1 - (2 * t - 1) * (2 * t - 1))) * (1 - 0.15f * t);
    const float cone = std::min(1.f, (1 - t) * 1.15f) * std::min(1.f, t * 6 + 0.25f);
    const float umbrella = std::sqrt(std::max(0.f, t)) * std::min(1.f, (1 - t) * 3.5f + 0.1f);
    const float k = s.crownShape >= 0 ? (1 - s.crownShape) * egg + s.crownShape * cone
                                      : (1 + s.crownShape) * egg - s.crownShape * umbrella;
    (void)sk;
    return std::hypot(p.x, p.z) <= s.crownRadius * k;
}

void growBroadleaf(const TreeSpec& s, Skeleton& sk, Rng& r) {
    const float H = s.height, yb = s.crownBase * H;
    const float step = std::max(0.05f, std::min(H, 2 * s.crownRadius + s.crownLength) / 32.f);
    // Attractors fill the crown.
    std::vector<Vector3> att;
    const int want = (int)std::clamp(900 * s.branchDensity * (s.crownLength > 0 ? 1.6f : 1.f), 40.f, 4000.f);
    const float boxX = s.crownLength > 0 ? s.crownLength * 0.5f : s.crownRadius;
    for (int tries = 0; (int)att.size() < want && tries < want * 60; tries++) {
        const Vector3 p{ r.range(-boxX, boxX), r.range(yb, H), r.range(-s.crownRadius, s.crownRadius) };
        if (inCrown(s, sk, p)) att.push_back(p);
    }
    // Stems: straight-ish from the ground to the crown base (spread for several).
    const int stems = std::max(1, s.stems);
    std::vector<int> tips;
    for (int k = 0; k < stems; k++) {
        Vector3 base{ 0, 0, 0 }, dir{ 0, 1, 0 };
        if (stems > 1) {
            const float a = TAU * (k + r.range(-0.3f, 0.3f)) / stems;
            if (s.crownLength > 0) base = { s.crownLength * ((k + 0.5f) / stems - 0.5f), 0, r.range(-0.1f, 0.1f) };
            else base = { std::cos(a) * 0.1f * s.crownRadius, 0, std::sin(a) * 0.1f * s.crownRadius };
            dir = Vector3Normalize({ base.x * 0.6f + r.range(-0.15f, 0.15f), 1, base.z * 0.6f + r.range(-0.15f, 0.15f) });
            if (s.crownLength > 0) dir = Vector3Normalize({ r.range(-0.15f, 0.15f), 1, r.range(-0.15f, 0.15f) });
        }
        int at = addNode(sk, base, -1);
        sk.nodes[(size_t)at].stem = true;
        Vector3 p = base;
        const float reachY = std::max(yb, step) * (stems > 1 ? r.range(0.7f, 1.0f) : 1.f);
        while (p.y < reachY) {
            dir = Vector3Normalize(Vector3Add(dir, Vector3Scale(r.ball(), 0.08f * s.gnarl)));
            dir.y = std::max(dir.y, 0.7f);
            dir = Vector3Normalize(dir);
            p = Vector3Add(p, Vector3Scale(dir, step));
            at = addNode(sk, p, at);
            sk.nodes[(size_t)at].stem = true;
        }
        tips.push_back(at);
    }
    // Space colonisation: each attractor pulls its nearest node within reach; nodes step
    // towards the mean pull; attractors close to a node are used up.
    const float influence = std::max(s.crownRadius, s.crownLength * 0.25f) * 0.9f + 4 * step, kill = 1.6f * step;
    for (int iter = 0; iter < 160 && !att.empty() && sk.nodes.size() < 7000; iter++) {
        std::vector<Vector3> pull(sk.nodes.size(), { 0, 0, 0 });
        std::vector<int> pulled(sk.nodes.size(), 0);
        bool any = false;
        for (const Vector3& a : att) {
            int best = -1;
            float bd = influence * influence;
            for (size_t i = 0; i < sk.nodes.size(); i++) {
                const float d = Vector3DistanceSqr(a, sk.nodes[i].p);
                if (d < bd) { bd = d; best = (int)i; }
            }
            if (best < 0) continue;
            pull[(size_t)best] = Vector3Add(pull[(size_t)best], Vector3Normalize(Vector3Subtract(a, sk.nodes[(size_t)best].p)));
            pulled[(size_t)best]++;
            any = true;
        }
        if (!any) {                                    // the stems have not reached the crown yet
            for (int& tip : tips) {
                const Vector3 p = Vector3Add(sk.nodes[(size_t)tip].p, { 0, step, 0 });
                tip = addNode(sk, p, tip);
                sk.nodes[(size_t)tip].stem = true;
            }
            continue;
        }
        const size_t n = sk.nodes.size();
        for (size_t i = 0; i < n; i++) {
            if (!pulled[i]) continue;
            Vector3 d = Vector3Add(Vector3Normalize(pull[i]), { 0, 0.18f, 0 });
            d = Vector3Add(d, Vector3Scale(r.ball(), 0.35f * s.gnarl));
            const Vector3 p = Vector3Add(sk.nodes[i].p, Vector3Scale(Vector3Normalize(d), step));
            bool near = false;                         // no two nodes on one spot
            for (size_t j = 0; j < sk.nodes.size() && !near; j++) near = Vector3DistanceSqr(p, sk.nodes[j].p) < step * step * 0.04f;
            if (!near) addNode(sk, p, (int)i);
        }
        att.erase(std::remove_if(att.begin(), att.end(), [&](const Vector3& a) {
                      for (size_t i = 0; i < sk.nodes.size(); i++)
                          if (Vector3DistanceSqr(a, sk.nodes[i].p) < kill * kill) return true;
                      return false;
                  }), att.end());
    }
    sk.crownCentre = { 0, (yb + H) * 0.5f, 0 };
    sk.crownRadii = { boxX, std::max((H - yb) * 0.5f, 0.1f), s.crownRadius };
}

void growConifer(const TreeSpec& s, Skeleton& sk, Rng& r) {
    const float H = s.height, yb = s.crownBase * H;
    const int segs = 24;
    const Vector2 wander{ r.range(0, TAU), r.range(0, TAU) };
    std::vector<int> stem;
    int at = -1;
    for (int i = 0; i <= segs; i++) {
        const float y = H * i / segs;
        const float u = (float)i / segs;
        at = addNode(sk, { 0.012f * H * u * std::sin(u * 7 + wander.x), y, 0.012f * H * u * std::sin(u * 6 + wander.y) }, at);
        sk.nodes[(size_t)at].stem = true;
        stem.push_back(at);
    }
    const int whorls = std::clamp(s.whorls, 1, 60);
    for (int w = 0; w < whorls; w++) {
        const float t = (w + r.range(0.f, 0.6f)) / whorls;                    // 0 crown base .. 1 top
        const float y = yb + (H * 0.97f - yb) * t;
        const float prof = s.crownShape >= 0.5f ? (1 - t) * 0.95f + 0.05f                          // cone
                                                : std::pow(std::sin(3.14159f * std::min(1.f, t * 0.95f + 0.05f)), 0.6f);  // rounded
        const float len = s.crownRadius * prof * r.range(0.85f, 1.1f);
        if (len < 0.15f) continue;
        const int arms = 4 + (int)(r.unit() * 3);
        const float turn = r.range(0, TAU);
        const int hostIdx = std::clamp((int)std::lround(y / H * segs), 0, segs);
        for (int a = 0; a < arms; a++) {
            const float ang = turn + TAU * (a + r.range(-0.15f, 0.15f)) / arms;
            const Vector3 out{ std::cos(ang), 0, std::sin(ang) };
            Vector3 p = { sk.nodes[(size_t)stem[(size_t)hostIdx]].p.x, y, sk.nodes[(size_t)stem[(size_t)hostIdx]].p.z };
            int prev = stem[(size_t)hostIdx];
            const int pieces = 5;
            for (int k = 1; k <= pieces; k++) {
                const float u = (float)k / pieces;
                const float rise = 0.18f * (1 - s.droop) - s.droop * u * 0.9f;
                p = Vector3Add(p, Vector3Scale(Vector3Normalize({ out.x, rise, out.z }), len / pieces));
                p = Vector3Add(p, Vector3Scale(r.ball(), 0.04f * len * s.gnarl));
                prev = addNode(sk, p, prev);
                // Side shoots along the outer part.
                if (k >= 2 && k < pieces) {
                    for (int side = -1; side <= 1; side += 2) {
                        const Vector3 sideDir = Vector3Normalize({ -out.z * side + out.x * 0.6f, rise * 0.5f, out.x * side + out.z * 0.6f });
                        const float sl = len * 0.28f * (1 - u * 0.5f);
                        int q = addNode(sk, Vector3Add(p, Vector3Scale(sideDir, sl * 0.5f)), prev);
                        addNode(sk, Vector3Add(sk.nodes[(size_t)q].p, Vector3Scale(sideDir, sl * 0.5f)), q);
                    }
                }
            }
        }
    }
    sk.crownCentre = { 0, (yb + H) * 0.5f, 0 };
    sk.crownRadii = { s.crownRadius, std::max((H - yb) * 0.5f, 0.1f), s.crownRadius };
}

struct Frond { std::vector<Vector3> spine; Vector3 across; int node, base; };

void growPalm(const TreeSpec& s, Skeleton& sk, Rng& r, std::vector<Frond>& fronds) {
    const float H = s.height;
    const int segs = 16;
    const float bendDir = r.range(0, TAU);
    int at = -1;
    for (int i = 0; i <= segs; i++) {
        const float u = (float)i / segs;
        const float off = s.lean * H * u * u;
        at = addNode(sk, { std::cos(bendDir) * off, H * 0.92f * u, std::sin(bendDir) * off }, at);
        sk.nodes[(size_t)at].stem = true;
    }
    const Vector3 top = sk.nodes[(size_t)at].p;
    const int count = std::max(4, (int)std::lround(22 * s.leafDensity));
    for (int f = 0; f < count; f++) {
        const float ang = f * 2.39996f + r.range(-0.2f, 0.2f);                  // golden angle
        const float el = (f < count / 3 ? 0.9f : 0.35f) + r.range(-0.2f, 0.2f);  // young fronds stand up
        const float L = s.crownRadius * r.range(0.85f, 1.1f);
        const Vector3 h{ std::cos(ang), 0, std::sin(ang) };
        Frond fr;
        fr.across = { -h.z, 0, h.x };
        int prev = at;
        const int pieces = 9;
        for (int k = 0; k <= pieces; k++) {
            const float u = (float)k / pieces;
            const float x = L * u * std::cos(el * 0.6f), y = L * (std::sin(el) * u - (0.4f + s.droop) * u * u);
            const Vector3 p = Vector3Add(top, { h.x * x, y, h.z * x });
            fr.spine.push_back(p);
            if (k > 0) prev = addNode(sk, p, prev);
        }
        fr.node = prev;
        fr.base = at;
        fronds.push_back(fr);
    }
    sk.crownCentre = top;
    sk.crownRadii = { s.crownRadius, s.crownRadius * 0.5f, s.crownRadius };
}

// Radii by the pipe model (a parent's area is about its children's), scaled so the stem's
// base has the spec's radius; then which first branch each node hangs from, and how far out.
void finish(const TreeSpec& s, Skeleton& sk) {
    std::vector<float> acc(sk.nodes.size(), 0.f);
    for (size_t i = sk.nodes.size(); i-- > 0;) {
        Node& n = sk.nodes[i];
        if (n.children == 0) acc[i] = 1;
        n.radius = std::pow(acc[i], 1 / 2.4f);
        if (n.parent >= 0) acc[(size_t)n.parent] += acc[i];
    }
    float base = 0;
    for (const Node& n : sk.nodes) if (n.parent < 0) base = std::max(base, n.radius);
    const float k = s.trunkRadius / std::max(base, 1e-6f) * (s.stems > 1 ? std::sqrt((float)s.stems) : 1.f);
    const float twig = std::max(0.004f, s.trunkRadius * 0.03f);
    for (Node& n : sk.nodes) n.radius = std::max(n.radius * k, twig);
    if (s.form == TreeForm::Conifer || s.form == TreeForm::Palm) {     // a stem that tapers evenly
        for (Node& n : sk.nodes)
            if (n.stem) n.radius = s.trunkRadius * std::max(0.08f, 1 - 0.85f * n.p.y / s.height) * (s.form == TreeForm::Palm ? (n.p.y < 0.05f * s.height ? 1.3f : 0.85f) : 1.f);
        for (Node& n : sk.nodes)
            if (!n.stem && n.parent >= 0) n.radius = std::min(n.radius, sk.nodes[(size_t)n.parent].radius * 0.75f);
    }
    int nextBranch = 1;
    for (size_t i = 0; i < sk.nodes.size(); i++) {
        Node& n = sk.nodes[i];
        if (n.stem || n.parent < 0) { n.branch = 0; n.reach = 0; continue; }
        const Node& p = sk.nodes[(size_t)n.parent];
        if (p.stem) { n.branch = nextBranch++; n.reach = Vector3Distance(n.p, p.p); }
        else { n.branch = p.branch; n.reach = p.reach + Vector3Distance(n.p, p.p); }
    }
}

// ---- meshes -------------------------------------------------------------------------------
// Light reaching a point inside the crown: darker towards its middle (a stand-in for the
// leaves shading each other). In the vertex colours, so the impostors carry it too.
float occlusion(const Skeleton& sk, Vector3 p) {
    const Vector3 q = Vector3Subtract(p, sk.crownCentre);
    const float d = std::sqrt(q.x * q.x / (sk.crownRadii.x * sk.crownRadii.x) + q.y * q.y / (sk.crownRadii.y * sk.crownRadii.y) +
                              q.z * q.z / (sk.crownRadii.z * sk.crownRadii.z));
    return 0.45f + 0.55f * smoothstep(0.1f, 1.0f, d);
}

struct Vert {
    Vector3 p, n;
    Vector2 uv;
    Color c;
    // Wind: what bends it.
    float branch = 0, reach = 0;
    Vector3 anchor{ 0, 0, 0 }, flutterAxis{ 0, 0, 0 };
    float flutter = 0;      // > 0 for leaves: phase
    // Growth: the node it rides on, the node whose size scales it, the point it scales about,
    // and (leaves) when it starts to unfold.
    int node = 0, sizer = 0;
    Vector3 pivot{ 0, 0, 0 };
    float appear = -1;
};

struct Soup {
    std::vector<Vert> v;
    std::vector<unsigned short> idx;
    bool full() const { return v.size() > 64000; }
};

// Bark tubes along chains of nodes. Chains start at a root or where a node branches; the
// thickest child continues its parent's chain.
void woodMesh(const TreeSpec& s, const Skeleton& sk, Soup& out, float minRadius, int maxSides) {
    const size_t n = sk.nodes.size();
    std::vector<std::vector<int>> kids(n);
    for (size_t i = 0; i < n; i++)
        if (sk.nodes[i].parent >= 0) kids[(size_t)sk.nodes[i].parent].push_back((int)i);
    std::vector<int> cont(n, -1);
    for (size_t i = 0; i < n; i++) {
        float best = -1;
        for (int c : kids[i]) if (sk.nodes[(size_t)c].radius > best) { best = sk.nodes[(size_t)c].radius; cont[i] = c; }
    }
    Rng r{ s.seed * 31ull + 5 };
    for (size_t start = 0; start < n; start++) {
        const Node& sn = sk.nodes[start];
        const bool starts = sn.parent < 0 || cont[(size_t)sn.parent] != (int)start;
        if (!starts || sn.radius < minRadius) continue;
        std::vector<int> chain;
        if (sn.parent >= 0) chain.push_back(sn.parent);
        for (int at = (int)start; at >= 0; at = cont[(size_t)at]) {
            if (sk.nodes[(size_t)at].radius < minRadius * 0.6f) break;
            chain.push_back(at);
        }
        if (chain.size() < 2 || out.full()) continue;
        const int sides = std::clamp(sn.radius > 0.15f ? 8 : sn.radius > 0.05f ? 6 : sn.radius > 0.015f ? 4 : 3, 3, maxSides);
        Vector3 ref = std::fabs(Vector3Subtract(sk.nodes[(size_t)chain[1]].p, sk.nodes[(size_t)chain[0]].p).y) > 0.9f *
                              Vector3Length(Vector3Subtract(sk.nodes[(size_t)chain[1]].p, sk.nodes[(size_t)chain[0]].p))
                          ? Vector3{ 1, 0, 0 } : Vector3{ 0, 1, 0 };
        const Color tone = scaleColor(s.bark, r.range(0.9f, 1.06f));
        float length = 0;
        const size_t first = out.v.size();
        for (size_t k = 0; k < chain.size(); k++) {
            const Node& nd = sk.nodes[(size_t)chain[k]];
            const Vector3 a = sk.nodes[(size_t)chain[k == 0 ? 0 : k - 1]].p, b = sk.nodes[(size_t)chain[std::min(k + 1, chain.size() - 1)]].p;
            const Vector3 dir = Vector3Normalize(Vector3Subtract(b, a));
            Vector3 side = Vector3Normalize(Vector3CrossProduct(dir, ref));
            const Vector3 up = Vector3CrossProduct(side, dir);
            ref = up;                                                 // carried along (no twist)
            if (k > 0) length += Vector3Distance(nd.p, sk.nodes[(size_t)chain[k - 1]].p);
            // The first ring sits in the parent, at this branch's own size.
            const float rad = k == 0 && sn.parent >= 0 ? sn.radius : nd.radius;
            const Node& owner = k == 0 && sn.parent >= 0 ? sn : nd;
            for (int j = 0; j <= sides; j++) {
                const float a2 = TAU * j / sides;
                const Vector3 nrm = Vector3Add(Vector3Scale(side, std::cos(a2)), Vector3Scale(up, std::sin(a2)));
                Vert v;
                v.p = Vector3Add(nd.p, Vector3Scale(nrm, rad));
                v.n = nrm;
                v.uv = { (float)j / sides, length / std::max(0.5f, TAU * std::max(rad, 0.05f) * 2.f) };
                v.c = scaleColor(tone, occlusion(sk, nd.p));
                v.branch = (float)(k == 0 && sn.parent >= 0 ? sk.nodes[(size_t)sn.parent].branch : owner.branch);
                v.reach = k == 0 && sn.parent >= 0 ? sk.nodes[(size_t)sn.parent].reach : owner.reach;
                v.node = chain[k];
                v.sizer = k == 0 && sn.parent >= 0 ? (int)start : chain[k];
                v.pivot = nd.p;
                out.v.push_back(v);
            }
        }
        for (size_t k = 0; k + 1 < chain.size(); k++)
            for (int j = 0; j < sides; j++) {
                const unsigned short a = (unsigned short)(first + k * (sides + 1) + j), b = (unsigned short)(a + 1);
                const unsigned short c = (unsigned short)(a + sides + 1), d = (unsigned short)(c + 1);
                for (unsigned short q : { a, c, b, b, c, d }) out.idx.push_back(q);     // outward
            }
    }
}

struct Card { Vector3 at, up, normal; float size; Color c; float phase; int node; float rank; };

// The season's colour of one card and whether it is still on the tree.
bool seasonal(const TreeSpec& s, float h1, float h2, Color& c) {
    const float season = std::clamp(s.season, 0.f, 1.f);
    Color base = scaleColor(s.leafColor, 1 + s.colorVariation * (h1 * 2 - 1));
    const Color fresh = mixColor(base, { 168, 200, 90, 255 }, 0.45f);
    if (season < 0.35f) base = mixColor(fresh, base, season / 0.35f);
    if (!s.deciduous) { c = scaleColor(base, 1 - 0.12f * smoothstep(0.5f, 1, season)); return true; }
    const float turn = 0.48f + 0.25f * h2;                      // when this card turns
    const Color autumn = scaleColor(s.autumnColor, 0.85f + 0.3f * h1);
    c = mixColor(base, autumn, smoothstep(turn, turn + 0.1f, season));
    return season < 0.78f + 0.2f * h1 * h2 + 0.02f;           // and when it falls
}

std::vector<Card> cards(const TreeSpec& s, const Skeleton& sk) {
    std::vector<Card> out;
    Rng r{ s.seed * 131ull + 17 };
    const size_t n = sk.nodes.size();
    std::vector<int> kids(n, 0);
    for (const Node& nd : sk.nodes) if (nd.parent >= 0) kids[(size_t)nd.parent]++;
    const float twig = std::max(0.004f, s.trunkRadius * 0.03f);
    for (size_t i = 0; i < n; i++) {
        const Node& nd = sk.nodes[i];
        if (nd.stem || nd.parent < 0) continue;
        // Broadleaves: at the ends and the thin wood next to them; conifers: along each branch.
        int per = 0;
        if (s.form == TreeForm::Broadleaf) per = kids[i] == 0 ? 5 : (nd.radius < twig * 3.5f ? 2 : 0);
        else if (s.form == TreeForm::Conifer) per = 2;
        const float want = per * s.leafDensity;
        int count = (int)want + (r.unit() < want - (int)want ? 1 : 0);
        const Vector3 grow = Vector3Normalize(Vector3Subtract(nd.p, sk.nodes[(size_t)nd.parent].p));
        Vector3 outward = Vector3Subtract(nd.p, sk.crownCentre);
        outward = { outward.x / sk.crownRadii.x, outward.y / sk.crownRadii.y, outward.z / sk.crownRadii.z };
        if (s.form == TreeForm::Conifer) outward = { nd.p.x, 0.6f, nd.p.z };
        outward = Vector3Normalize(outward);
        for (int k = 0; k < count; k++) {
            Card c;
            c.node = (int)i;
            c.size = s.leafSize * r.range(0.8f, 1.2f);
            if (s.form == TreeForm::Conifer) {
                c.up = Vector3Normalize(Vector3Add(grow, Vector3Scale(r.ball(), 0.3f)));
                c.normal = Vector3Normalize(Vector3Add({ 0, 1, 0 }, Vector3Scale(r.ball(), 0.7f)));
                c.at = Vector3Lerp(sk.nodes[(size_t)nd.parent].p, nd.p, r.unit());
            } else {
                c.up = Vector3Normalize(Vector3Add(Vector3Add(grow, Vector3Scale(outward, 0.6f)), Vector3Scale(r.ball(), 0.7f)));
                c.normal = Vector3Normalize(Vector3Add(outward, Vector3Scale(r.ball(), 0.9f)));
                c.at = Vector3Add(nd.p, Vector3Scale(r.ball(), c.size * 0.4f));
            }
            // Make the card's frame square: up in its plane.
            c.up = Vector3Normalize(Vector3Subtract(c.up, Vector3Scale(c.normal, Vector3DotProduct(c.up, c.normal))));
            if (!std::isfinite(c.up.x)) c.up = { 0, 1, 0 };
            const float h1 = r.unit(), h2 = r.unit();
            c.phase = r.range(0, TAU);
            c.rank = r.unit();
            if (!seasonal(s, h1, h2, c.c)) continue;
            out.push_back(c);
        }
    }
    return out;
}

void leafMesh(const TreeSpec& s, const Skeleton& sk, const std::vector<Card>& cs, float keep, Soup& out) {
    const float grow = 1 / std::sqrt(std::max(keep, 0.05f));
    for (const Card& c : cs) {
        if (c.rank >= keep || out.full()) continue;
        const Node& nd = sk.nodes[(size_t)c.node];
        const float size = c.size * grow;
        const Vector3 right = Vector3CrossProduct(c.up, c.normal);            // front face: normal
        Vector3 shade = Vector3Subtract(c.at, sk.crownCentre);
        shade = Vector3Normalize({ shade.x / sk.crownRadii.x, shade.y / sk.crownRadii.y, shade.z / sk.crownRadii.z });
        const Vector3 n = Vector3Normalize(Vector3Add(Vector3Scale(c.normal, 0.35f), Vector3Scale(shade, 0.65f)));
        const size_t first = out.v.size();
        const Vector2 uv[4] = { { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 } };
        const float xs[4] = { -0.5f, 0.5f, 0.5f, -0.5f }, ys[4] = { 0, 0, 1, 1 };
        for (int k = 0; k < 4; k++) {
            Vert v;
            v.p = Vector3Add(c.at, Vector3Add(Vector3Scale(right, xs[k] * size), Vector3Scale(c.up, ys[k] * size)));
            v.n = n;
            v.uv = uv[k];
            v.c = scaleColor(c.c, occlusion(sk, c.at));
            v.branch = (float)nd.branch;
            v.reach = nd.reach;
            v.anchor = c.at;
            v.flutterAxis = right;
            v.flutter = c.phase + 1e-3f;
            v.node = v.sizer = c.node;
            v.pivot = c.at;
            v.appear = 0.6f + 0.26f * c.rank;
            out.v.push_back(v);
        }
        for (unsigned short q : { 0, 1, 2, 0, 2, 3 }) out.idx.push_back((unsigned short)(first + q));
    }
    (void)s;
}

void frondMesh(const TreeSpec& s, const Skeleton& sk, const std::vector<Frond>& fronds, Soup& out) {
    Rng r{ s.seed * 977ull + 1 };
    for (const Frond& f : fronds) {
        Color c;
        if (!seasonal(s, r.unit(), r.unit(), c)) continue;
        const float width = s.leafSize, phase = r.range(0, TAU), unfold = r.unit();
        const Node& tip = sk.nodes[(size_t)f.node];
        const size_t first = out.v.size();
        const size_t m = f.spine.size();
        float total = 0;
        for (size_t k = 1; k < m; k++) total += Vector3Distance(f.spine[k], f.spine[k - 1]);
        float along = 0;
        for (size_t k = 0; k < m; k++) {
            if (k > 0) along += Vector3Distance(f.spine[k], f.spine[k - 1]);
            const float u = along / std::max(total, 1e-3f);
            const Vector3 dir = Vector3Normalize(Vector3Subtract(f.spine[std::min(k + 1, m - 1)], f.spine[k == 0 ? 0 : k - 1]));
            const Vector3 nrm = Vector3Normalize(Vector3CrossProduct(f.across, dir));
            const float w = width * (0.25f + 0.75f * std::sin(3.14159f * std::min(1.f, u * 0.9f + 0.1f)));
            for (int side = 0; side < 2; side++) {
                Vert v;
                // A shallow V: the leaflets droop from the rachis.
                v.p = Vector3Add(f.spine[k], Vector3Add(Vector3Scale(f.across, (side ? 0.5f : -0.5f) * w), Vector3Scale(nrm, -0.12f * w)));
                v.n = Vector3Normalize(Vector3Add(nrm, { 0, 0.6f, 0 }));
                v.uv = { (float)side, 1 - u };
                v.c = c;
                v.branch = (float)tip.branch;
                v.reach = tip.reach * u;
                v.anchor = f.spine[k];
                v.flutterAxis = dir;
                v.flutter = phase + 1e-3f;
                v.node = v.sizer = f.base;
                v.pivot = f.spine[0];
                v.appear = 0.6f + 0.26f * unfold;
                out.v.push_back(v);
            }
        }
        for (size_t k = 0; k + 1 < m; k++) {
            const unsigned short a = (unsigned short)(first + k * 2), b = (unsigned short)(a + 1), c2 = (unsigned short)(a + 2), d = (unsigned short)(a + 3);
            for (unsigned short q : { a, b, c2, b, d, c2 }) out.idx.push_back(q);
        }
    }
}

// The wind loop: frame f of `frames` moves a vertex by the stem's bend (a function of
// height), its branch's lag (a phase per branch, more further out) and its card's flutter.
Vector3 windAt(const TreeSpec& s, const Vert& v, float f, int frames) {
    const float ph = TAU * f / frames, H = std::max(s.height, 0.1f);
    const float h = std::max(v.p.y, 0.f) / H;
    const float bend = 0.014f * s.sway * H * h * h;
    Vector3 d{ bend * std::sin(ph), 0, 0.55f * bend * std::sin(2 * ph + 1.3f) };
    const float bp = v.branch * 2.399f;
    const float lag = 0.022f * s.sway * v.reach * (v.branch > 0 ? 1.f : 0.f);
    d = Vector3Add(d, { lag * std::sin(2 * ph + bp), 0.4f * lag * std::sin(3 * ph + bp * 1.7f), lag * std::cos(2 * ph + bp) });
    if (v.flutter > 0) {
        const Vector3 arm = Vector3Subtract(v.p, v.anchor);
        const float a = 0.16f * s.sway * std::sin(3 * ph + v.flutter);
        d = Vector3Add(d, Vector3Scale(Vector3RotateByAxisAngle(arm, v.flutterAxis, a), 1));
        d = Vector3Subtract(d, arm);
    }
    return d;
}

g3d::GeometryPtr geometryOf(std::shared_ptr<const std::vector<Vert>> verts, std::shared_ptr<const std::vector<unsigned short>> idx) {
    return std::make_shared<g3d::Geometry>([verts, idx] {
        Mesh m{};
        m.vertexCount = (int)verts->size();
        m.triangleCount = (int)idx->size() / 3;
        m.vertices = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
        m.normals = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
        m.texcoords = (float*)MemAlloc((unsigned)(m.vertexCount * 2 * sizeof(float)));
        m.colors = (unsigned char*)MemAlloc((unsigned)(m.vertexCount * 4));
        m.indices = (unsigned short*)MemAlloc((unsigned)(idx->size() * sizeof(unsigned short)));
        for (int k = 0; k < m.vertexCount; k++) {
            const Vert& v = (*verts)[(size_t)k];
            m.vertices[k * 3] = v.p.x; m.vertices[k * 3 + 1] = v.p.y; m.vertices[k * 3 + 2] = v.p.z;
            m.normals[k * 3] = v.n.x; m.normals[k * 3 + 1] = v.n.y; m.normals[k * 3 + 2] = v.n.z;
            m.texcoords[k * 2] = v.uv.x; m.texcoords[k * 2 + 1] = v.uv.y;
            m.colors[k * 4] = v.c.r; m.colors[k * 4 + 1] = v.c.g; m.colors[k * 4 + 2] = v.c.b; m.colors[k * 4 + 3] = 255;
        }
        std::memcpy(m.indices, idx->data(), idx->size() * sizeof(unsigned short));
        UploadMesh(&m, false);
        return m;
    });
}

std::shared_ptr<g3d::AnimatedMesh> bake(const TreeSpec& s, const Soup& soup, int frames) {
    auto am = std::make_shared<g3d::AnimatedMesh>();
    if (soup.v.empty()) return am;
    am->frames = frames;
    am->vertices = (int)soup.v.size();
    am->positions.resize((size_t)frames * soup.v.size() * 3);
    am->normals.resize(am->positions.size());
    Vector3 lo{ 1e9f, 1e9f, 1e9f }, hi{ -1e9f, -1e9f, -1e9f };
    for (int f = 0; f < frames; f++)
        for (size_t i = 0; i < soup.v.size(); i++) {
            const Vert& v = soup.v[i];
            // Relative to frame 0, so frame 0 is the tree at rest (and a still forest is exact).
            const Vector3 p = Vector3Add(v.p, Vector3Subtract(windAt(s, v, (float)f, frames), windAt(s, v, 0, frames)));
            float* q = &am->positions[((size_t)f * soup.v.size() + i) * 3];
            q[0] = p.x; q[1] = p.y; q[2] = p.z;
            float* n = &am->normals[((size_t)f * soup.v.size() + i) * 3];
            n[0] = v.n.x; n[1] = v.n.y; n[2] = v.n.z;
            lo = Vector3Min(lo, p); hi = Vector3Max(hi, p);
        }
    am->centre = Vector3Scale(Vector3Add(lo, hi), 0.5f);
    am->radius = Vector3Distance(lo, hi) * 0.5f + 0.05f;
    am->rest = geometryOf(std::make_shared<std::vector<Vert>>(soup.v), std::make_shared<std::vector<unsigned short>>(soup.idx));
    return am;
}

// ---- impostors: the hero tree drawn on the CPU from `views` directions round it -----------
struct Baked { std::shared_ptr<Picture> colour, normal; float side = 1; };

Baked bakeViews(const Soup& wood, const Soup& leaves, const Picture& bark, const Picture& leaf, int views, int size, float top, float reach) {
    const int S = size, SS = size * 2;
    Baked b;
    b.side = std::max(top, 2 * reach) * 1.06f;
    b.colour = std::make_shared<Picture>(S * views, S);
    b.normal = std::make_shared<Picture>(S * views, S);
    for (int k = 0; k < views; k++) {
        const float th = TAU * k / views;
        const Vector3 dir{ std::sin(th), 0, std::cos(th) }, up{ 0, 1, 0 };
        const Vector3 right = Vector3CrossProduct(Vector3Negate(dir), up);
        std::vector<float> depth((size_t)SS * SS, -1e30f);
        std::vector<Vector4> col((size_t)SS * SS, { 0, 0, 0, 0 });
        std::vector<Vector3> nor((size_t)SS * SS, { 0, 0, 0 });
        auto toScreen = [&](Vector3 p) {
            return Vector3{ (Vector3DotProduct(p, right) / b.side + 0.5f) * SS, (1 - p.y / b.side) * SS, Vector3DotProduct(p, dir) };
        };
        auto raster = [&](const Soup& soup, const Picture& pic, bool cut) {
            for (size_t t = 0; t + 2 < soup.idx.size(); t += 3) {
                const Vert& A = soup.v[soup.idx[t]]; const Vert& B = soup.v[soup.idx[t + 1]]; const Vert& C = soup.v[soup.idx[t + 2]];
                const Vector3 a = toScreen(A.p), bq = toScreen(B.p), c = toScreen(C.p);
                const float area = (bq.x - a.x) * (c.y - a.y) - (bq.y - a.y) * (c.x - a.x);
                if (std::fabs(area) < 1e-9f) continue;
                // Facing: as the GPU flips a back face's normal towards the viewer.
                const Vector3 fn = Vector3CrossProduct(Vector3Subtract(B.p, A.p), Vector3Subtract(C.p, A.p));
                const float facing = Vector3DotProduct(fn, dir) >= 0 ? 1.f : -1.f;
                const int x0 = std::max(0, (int)std::floor(std::min({ a.x, bq.x, c.x }))), x1 = std::min(SS - 1, (int)std::ceil(std::max({ a.x, bq.x, c.x })));
                const int y0 = std::max(0, (int)std::floor(std::min({ a.y, bq.y, c.y }))), y1 = std::min(SS - 1, (int)std::ceil(std::max({ a.y, bq.y, c.y })));
                for (int y = y0; y <= y1; y++)
                    for (int x = x0; x <= x1; x++) {
                        const float px = x + 0.5f, py = y + 0.5f;
                        const float w0 = ((bq.x - px) * (c.y - py) - (bq.y - py) * (c.x - px)) / area;
                        const float w1 = ((c.x - px) * (a.y - py) - (c.y - py) * (a.x - px)) / area;
                        const float w2 = 1 - w0 - w1;
                        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                        const float z = w0 * a.z + w1 * bq.z + w2 * c.z;
                        const size_t at = (size_t)y * SS + x;
                        if (z <= depth[at]) continue;
                        const float u = w0 * A.uv.x + w1 * B.uv.x + w2 * C.uv.x, v = w0 * A.uv.y + w1 * B.uv.y + w2 * C.uv.y;
                        const Vector4 tex = pic.sample(u, v);
                        if (cut && tex.w < 0.5f) continue;
                        depth[at] = z;
                        col[at] = { tex.x * (w0 * A.c.r + w1 * B.c.r + w2 * C.c.r) / 255, tex.y * (w0 * A.c.g + w1 * B.c.g + w2 * C.c.g) / 255,
                                    tex.z * (w0 * A.c.b + w1 * B.c.b + w2 * C.c.b) / 255, 1 };
                        Vector3 n = Vector3Normalize(Vector3Add(Vector3Add(Vector3Scale(A.n, w0), Vector3Scale(B.n, w1)), Vector3Scale(C.n, w2)));
                        if (cut && facing < 0) n = Vector3Negate(n);
                        nor[at] = n;
                    }
            }
        };
        raster(wood, bark, false);
        raster(leaves, leaf, true);
        // 2x2 down to the cell; colour averaged over what is covered, coverage as alpha.
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                Vector4 sum{ 0, 0, 0, 0 };
                Vector3 ns{ 0, 0, 0 };
                for (int j = 0; j < 2; j++)
                    for (int i = 0; i < 2; i++) {
                        const size_t at = (size_t)(y * 2 + j) * SS + (x * 2 + i);
                        if (col[at].w <= 0) continue;
                        sum = { sum.x + col[at].x, sum.y + col[at].y, sum.z + col[at].z, sum.w + 1 };
                        ns = Vector3Add(ns, nor[at]);
                    }
                unsigned char* c = b.colour->at(k * S + x, y);
                unsigned char* nn = b.normal->at(k * S + x, y);
                // Tangent space of the quad: u along `right`, v down, out towards the viewer.
                const Vector3 n = sum.w > 0 ? Vector3Normalize(ns) : dir;
                const Vector3 tsn{ Vector3DotProduct(n, right), -n.y, Vector3DotProduct(n, dir) };
                nn[0] = (unsigned char)std::lround((tsn.x * 0.5f + 0.5f) * 255);
                nn[1] = (unsigned char)std::lround((tsn.y * 0.5f + 0.5f) * 255);
                nn[2] = (unsigned char)std::lround((tsn.z * 0.5f + 0.5f) * 255);
                nn[3] = 255;
                if (sum.w > 0) {
                    c[0] = (unsigned char)std::lround(sum.x / sum.w * 255);
                    c[1] = (unsigned char)std::lround(sum.y / sum.w * 255);
                    c[2] = (unsigned char)std::lround(sum.z / sum.w * 255);
                }
                c[3] = (unsigned char)std::lround(sum.w / 4 * 255);
            }
    }
    // Spread colours and normals into the clear texels (mipmaps), within each cell.
    for (int pass = 0; pass < 6; pass++) {
        Picture cn = *b.colour, nn = *b.normal;
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S * views; x++) {
                if (b.colour->at(x, y)[3] > 0 || cn.at(x, y)[0] + cn.at(x, y)[1] + cn.at(x, y)[2] > 0) continue;
                int sc[3] = { 0, 0, 0 }, sn[3] = { 0, 0, 0 }, n = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        const int xx = x + dx, yy = y + dy;
                        if (yy < 0 || yy >= S || xx < 0 || xx >= S * views || xx / S != x / S) continue;
                        const unsigned char* q = b.colour->at(xx, yy);
                        if (q[0] + q[1] + q[2] == 0) continue;
                        for (int ch = 0; ch < 3; ch++) { sc[ch] += q[ch]; sn[ch] += b.normal->at(xx, yy)[ch]; }
                        n++;
                    }
                if (!n) continue;
                for (int ch = 0; ch < 3; ch++) { cn.at(x, y)[ch] = (unsigned char)(sc[ch] / n); nn.at(x, y)[ch] = (unsigned char)(sn[ch] / n); }
            }
        *b.colour = std::move(cn);
        *b.normal = std::move(nn);
    }
    return b;
}

std::shared_ptr<g3d::AnimatedMesh> quad(float side, float u0, float u1) {
    auto am = std::make_shared<g3d::AnimatedMesh>();
    am->frames = 1;
    am->vertices = 4;
    const float h = side * 0.5f;
    am->positions = { -h, 0, 0, h, 0, 0, h, side, 0, -h, side, 0 };
    am->normals = { 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 };
    am->centre = { 0, h, 0 };
    am->radius = side * 0.72f;
    const std::vector<float> pos = am->positions;
    am->rest = std::make_shared<g3d::Geometry>([pos, u0, u1] {
        Mesh m{};
        m.vertexCount = 4;
        m.triangleCount = 2;
        m.vertices = (float*)MemAlloc(12 * sizeof(float));
        m.normals = (float*)MemAlloc(12 * sizeof(float));
        m.texcoords = (float*)MemAlloc(8 * sizeof(float));
        m.colors = (unsigned char*)MemAlloc(16);
        m.indices = (unsigned short*)MemAlloc(6 * sizeof(unsigned short));
        std::copy(pos.begin(), pos.end(), m.vertices);
        for (int k = 0; k < 4; k++) { m.normals[k * 3] = 0; m.normals[k * 3 + 1] = 0; m.normals[k * 3 + 2] = 1; }
        const float uv[8] = { u0, 1, u1, 1, u1, 0, u0, 0 };
        std::copy(uv, uv + 8, m.texcoords);
        std::memset(m.colors, 255, 16);
        const unsigned short id[6] = { 0, 1, 2, 0, 2, 3 };
        std::copy(id, id + 6, m.indices);
        UploadMesh(&m, false);
        return m;
    });
    return am;
}

}  // namespace

// ---- growth ------------------------------------------------------------------------------
// The skeleton as growth sees it: each node's rest position, parent, distance from the root
// along the wood, and the length of wood beyond it; plus the full tree's vertices.
struct TreeGrowth {
    TreeSpec spec;
    int frames = 1;
    std::vector<Vector3> rest;
    std::vector<int> parent;
    std::vector<float> dist, beyond;
    float longest = 1;
    std::shared_ptr<const std::vector<Vert>> wood, leaves;
    std::shared_ptr<const std::vector<unsigned short>> woodIdx, leafIdx;
};

namespace {

constexpr float WOOD_SPAN = 0.7f;      // the wood grows over u in [0, 0.7]; leaves unfold after 0.6
constexpr float LEAF_SPAN = 0.12f;     // a card's unfolding

// Per node at growth u: how far it is displaced from rest (it rides on its parent, and is
// pulled back towards it until the front has passed), how big its wood is, and when the
// front reached it.
struct NodeGrowth { std::vector<Vector3> offset; std::vector<float> size, born; };

void growNodes(const TreeGrowth& g, float u, NodeGrowth& out) {
    const size_t n = g.rest.size();
    out.offset.assign(n, { 0, 0, 0 });
    out.size.assign(n, 0.f);
    out.born.assign(n, 0.f);
    const float front = std::clamp(u / WOOD_SPAN, 0.f, 1.f) * g.longest;
    const float start = std::clamp(u / 0.04f, 0.f, 1.f);              // nothing at all at u = 0
    for (size_t i = 0; i < n; i++) {
        const int p = g.parent[i];
        float e = 1;
        if (p >= 0) {
            const float len = g.dist[i] - g.dist[(size_t)p];
            e = len > 1e-6f ? std::clamp((front - g.dist[(size_t)p]) / len, 0.f, 1.f) : (front >= g.dist[i] ? 1.f : 0.f);
            out.offset[i] = Vector3Add(out.offset[(size_t)p], Vector3Scale(Vector3Subtract(g.rest[i], g.rest[(size_t)p]), e - 1));
        }
        // Thickening behind the front: full once the wood beyond this node has all grown.
        float k = front >= g.dist[i] + g.beyond[i] ? 1.f : g.beyond[i] > 1e-6f ? std::clamp((front - g.dist[i]) / g.beyond[i], 0.f, 1.f) : 0.f;
        const float sz = front >= g.dist[i] ? 0.12f + 0.88f * std::pow(k, 0.7f) : 0.12f * e;
        out.size[i] = sz * start;
        out.born[i] = g.dist[i] / std::max(g.longest, 1e-6f) * WOOD_SPAN;
    }
}

// A vertex at growth u. At u = 1 every term below is exactly zero, so the rest pose comes back.
Vector3 grownVertex(const NodeGrowth& ng, const Vert& v, float u) {
    float scale = ng.size[(size_t)v.sizer];
    if (v.appear >= 0) {
        const float from = std::max(v.appear, ng.born[(size_t)v.node]);
        scale = smoothstep(from, from + LEAF_SPAN, u);
    }
    return Vector3Add(Vector3Add(v.p, ng.offset[(size_t)v.node]), Vector3Scale(Vector3Subtract(v.p, v.pivot), scale - 1));
}

}  // namespace

TreePose grownPositions(const Tree& tree, float u) {
    TreePose pose;
    if (!tree.growth) return pose;
    const TreeGrowth& g = *tree.growth;
    u = std::isfinite(u) ? std::clamp(u, 0.f, 1.f) : 1.f;
    NodeGrowth ng;
    growNodes(g, u, ng);
    for (const Vert& v : *g.wood) pose.wood.push_back(grownVertex(ng, v, u));
    for (const Vert& v : *g.leaves) pose.leaves.push_back(grownVertex(ng, v, u));
    return pose;
}

// ---- presets -----------------------------------------------------------------------------
TreeSpec oak() {
    TreeSpec s;
    s.height = 13; s.trunkRadius = 0.42f; s.crownBase = 0.26f; s.crownRadius = 6.2f; s.crownShape = -0.2f;
    s.gnarl = 0.6f; s.leafSize = 1.1f; s.leafDensity = 1.1f;
    s.bark = { 88, 78, 66, 255 }; s.leafColor = { 62, 96, 40, 255 }; s.autumnColor = { 160, 104, 40, 255 };
    return s;
}
TreeSpec birch() {
    TreeSpec s;
    s.height = 15; s.trunkRadius = 0.17f; s.crownBase = 0.3f; s.crownRadius = 2.8f; s.crownShape = 0.35f;
    s.gnarl = 0.35f; s.leafSize = 0.75f; s.leafDensity = 1.2f;
    s.bark = { 226, 222, 210, 255 }; s.barkMarks = 1;
    s.leafColor = { 104, 138, 56, 255 }; s.autumnColor = { 222, 182, 52, 255 };
    s.sway = 1.4f;
    return s;
}
TreeSpec pine() {
    TreeSpec s;
    s.form = TreeForm::Conifer; s.leaf = LeafShape::Needles; s.deciduous = false;
    s.height = 18; s.trunkRadius = 0.3f; s.crownBase = 0.58f; s.crownRadius = 3.6f; s.crownShape = 0;
    s.whorls = 10; s.droop = 0.05f; s.gnarl = 0.5f; s.leafSize = 1.0f; s.leafDensity = 1.3f;
    s.bark = { 132, 84, 58, 255 }; s.leafColor = { 54, 82, 44, 255 };
    s.sway = 0.8f;
    return s;
}
TreeSpec spruce() {
    TreeSpec s;
    s.form = TreeForm::Conifer; s.leaf = LeafShape::Needles; s.deciduous = false;
    s.height = 16; s.trunkRadius = 0.28f; s.crownBase = 0.06f; s.crownRadius = 3.1f; s.crownShape = 1;
    s.whorls = 26; s.droop = 0.35f; s.gnarl = 0.2f; s.leafSize = 0.9f; s.leafDensity = 1.4f;
    s.bark = { 90, 70, 58, 255 }; s.leafColor = { 44, 76, 50, 255 };
    s.sway = 0.7f;
    return s;
}
TreeSpec palm() {
    TreeSpec s;
    s.form = TreeForm::Palm; s.leaf = LeafShape::Frond; s.deciduous = false;
    s.height = 11; s.trunkRadius = 0.22f; s.crownRadius = 5.2f; s.lean = 0.14f; s.droop = 0.45f;
    s.leafSize = 2.1f; s.leafDensity = 1;
    s.bark = { 140, 120, 96, 255 }; s.barkRings = 1;
    s.leafColor = { 76, 116, 44, 255 };
    s.sway = 1.3f;
    return s;
}
TreeSpec tropical() {
    TreeSpec s;
    s.height = 16; s.trunkRadius = 0.4f; s.crownBase = 0.55f; s.crownRadius = 8; s.crownShape = -1;
    s.gnarl = 0.45f; s.leafSize = 1.2f; s.leafDensity = 1.1f; s.deciduous = false;
    s.bark = { 150, 140, 122, 255 }; s.leafColor = { 46, 100, 40, 255 };
    return s;
}
TreeSpec shrub() {
    TreeSpec s;
    s.height = 2; s.trunkRadius = 0.04f; s.crownBase = 0.08f; s.crownRadius = 1.3f; s.crownShape = -0.3f; s.stems = 5;
    s.gnarl = 0.6f; s.leafSize = 0.45f; s.leafDensity = 1.4f; s.branchDensity = 0.7f;
    s.bark = { 96, 82, 66, 255 }; s.leafColor = { 66, 98, 44, 255 };
    s.sway = 1.6f;
    return s;
}
TreeSpec hedge() {
    TreeSpec s;
    s.height = 1.8f; s.trunkRadius = 0.035f; s.crownBase = 0.05f; s.crownRadius = 0.55f; s.crownLength = 4; s.stems = 7;
    s.gnarl = 0.5f; s.leafSize = 0.4f; s.leafDensity = 2.2f; s.branchDensity = 0.9f;
    s.bark = { 96, 82, 66, 255 }; s.leafColor = { 48, 84, 38, 255 }; s.colorVariation = 0.1f;
    s.sway = 0.6f;
    return s;
}
TreeSpec tree(const std::string& name) {
    if (name == "oak") return oak();
    if (name == "birch") return birch();
    if (name == "pine") return pine();
    if (name == "spruce") return spruce();
    if (name == "palm") return palm();
    if (name == "tropical") return tropical();
    if (name == "shrub") return shrub();
    if (name == "hedge") return hedge();
    throw std::invalid_argument("flora::tree: unknown preset '" + name +
                                "' (oak, birch, pine, spruce, palm, tropical, shrub, hedge)");
}

// ---- building ----------------------------------------------------------------------------
TreePtr makeTree(const TreeSpec& specIn, const TreeBuild& buildIn) {
    TreeSpec s = specIn;
    TreeBuild b = buildIn;
    if (!(s.height > 0.1f) || !std::isfinite(s.height) || s.height > 200 || !(s.crownRadius > 0.05f) || s.crownRadius > 100)
        throw std::invalid_argument("flora::makeTree: height must be 0.1..200 m and crownRadius 0.05..100 m");
    s.trunkRadius = std::clamp(s.trunkRadius, 0.005f, s.height * 0.2f);
    s.crownBase = std::clamp(s.crownBase, 0.f, 0.95f);
    s.crownShape = std::clamp(s.crownShape, -1.f, 1.f);
    s.crownLength = std::clamp(s.crownLength, 0.f, 100.f);
    s.stems = std::clamp(s.stems, 1, 16);
    s.gnarl = std::clamp(s.gnarl, 0.f, 1.f);
    s.branchDensity = std::clamp(s.branchDensity, 0.1f, 4.f);
    s.leafDensity = std::clamp(s.leafDensity, 0.f, 6.f);
    s.leafSize = std::clamp(s.leafSize, 0.05f, 10.f);
    b.frames = std::clamp(b.frames, 1, 64);
    b.views = std::clamp(b.views, 1, 32);
    b.viewSize = std::clamp(b.viewSize, 16, 512);
    b.reducedLeaves = std::clamp(b.reducedLeaves, 0.05f, 1.f);
    if (!(b.loopSeconds > 0.05f)) b.loopSeconds = 4;

    Skeleton sk;
    sk.H = s.height;
    Rng r{ (uint64_t)s.seed * 0x2545F4914F6CDD1Dull + 11 };
    std::vector<Frond> fronds;
    if (s.form == TreeForm::Conifer) growConifer(s, sk, r);
    else if (s.form == TreeForm::Palm) growPalm(s, sk, r, fronds);
    else growBroadleaf(s, sk, r);
    finish(s, sk);

    const auto barkPic = barkPicture(s);
    const auto leafPic = leafPicture(s.leaf, s.seed);
    const std::vector<Card> cs = s.form == TreeForm::Palm ? std::vector<Card>{} : cards(s, sk);
    const float twig = std::max(0.004f, s.trunkRadius * 0.03f);

    Soup heroWood, heroLeaves, lowWood, lowLeaves;
    woodMesh(s, sk, heroWood, s.form == TreeForm::Palm ? s.trunkRadius * 0.5f : twig * 1.5f, 8);
    woodMesh(s, sk, lowWood, s.form == TreeForm::Palm ? s.trunkRadius * 0.5f : std::max(twig * 4, s.trunkRadius * 0.16f), 5);
    if (s.form == TreeForm::Palm) { frondMesh(s, sk, fronds, heroLeaves); lowLeaves = heroLeaves; }
    else { leafMesh(s, sk, cs, 1, heroLeaves); leafMesh(s, sk, cs, b.reducedLeaves, lowLeaves); }

    auto t = std::make_shared<Tree>();
    t->spec = s;
    t->build = b;
    t->hero = { bake(s, heroWood, b.frames), bake(s, heroLeaves, b.frames) };
    t->reduced = { bake(s, lowWood, b.frames), bake(s, lowLeaves, b.frames) };
    t->heroTriangles = (long long)(heroWood.idx.size() + heroLeaves.idx.size()) / 3;
    t->reducedTriangles = (long long)(lowWood.idx.size() + lowLeaves.idx.size()) / 3;
    for (const Soup* so : { &heroWood, &heroLeaves })
        for (const Vert& v : so->v) { t->top = std::max(t->top, v.p.y); t->radius = std::max(t->radius, std::hypot(v.p.x, v.p.z)); }
    {
        auto g = std::make_shared<TreeGrowth>();
        g->spec = s;
        g->frames = b.frames;
        const size_t n = sk.nodes.size();
        g->rest.resize(n); g->parent.resize(n); g->dist.assign(n, 0.f); g->beyond.assign(n, 0.f);
        for (size_t i = 0; i < n; i++) {
            const Node& nd = sk.nodes[i];
            g->rest[i] = nd.p;
            g->parent[i] = nd.parent;
            if (nd.parent >= 0) g->dist[i] = g->dist[(size_t)nd.parent] + Vector3Distance(nd.p, sk.nodes[(size_t)nd.parent].p);
        }
        std::vector<float> farthest(g->dist);
        for (size_t i = n; i-- > 0;)
            if (g->parent[i] >= 0) farthest[(size_t)g->parent[i]] = std::max(farthest[(size_t)g->parent[i]], farthest[i]);
        for (size_t i = 0; i < n; i++) { g->beyond[i] = farthest[i] - g->dist[i]; g->longest = std::max(g->longest, farthest[i]); }
        g->wood = std::make_shared<const std::vector<Vert>>(heroWood.v);
        g->leaves = std::make_shared<const std::vector<Vert>>(heroLeaves.v);
        g->woodIdx = std::make_shared<const std::vector<unsigned short>>(heroWood.idx);
        g->leafIdx = std::make_shared<const std::vector<unsigned short>>(heroLeaves.idx);
        t->growth = g;
    }

    const Baked baked = bakeViews(heroWood, heroLeaves, *barkPic, *leafPic, b.views, b.viewSize, t->top, t->radius);
    for (int k = 0; k < b.views; k++) {
        // Half a texel in from each cell's edge, so mipmaps of one view don't take the next's.
        const float pad = 1.5f / (b.viewSize * b.views);
        t->views.push_back(quad(baked.side, (float)k / b.views + pad, (float)(k + 1) / b.views - pad));
    }

    g3d::Material wood;
    wood.color = WHITE; wood.roughness = 0.95f; wood.lineWidth = 0;
    wood.dynamicMap = texture(barkPic);
    g3d::Material leaves;
    leaves.color = WHITE; leaves.roughness = 0.75f; leaves.lineWidth = 0;
    leaves.dynamicMap = texture(leafPic);
    leaves.alphaCutoff = 0.5f; leaves.doubleSided = true;
    leaves.translucency = 0.55f; leaves.translucencyColor = { 255, 245, 190, 255 };
    g3d::Material imp = leaves;
    imp.dynamicMap = texture(baked.colour);
    imp.dynamicNormalMap = texture(baked.normal);
    imp.translucency = 0.4f;
    t->wood = wood;
    t->leaves = leaves;
    t->impostor = imp;
    return t;
}

// ---- forests -----------------------------------------------------------------------------
namespace {

struct ForestState {
    float yawSign = 1;
    std::vector<Placement> at;
    std::vector<TreePtr> trees;
    ForestSpec spec;
    std::vector<size_t> first;      // each kind's first child
};

// Children per tree kind, in this order.
constexpr int HERO_WOOD = 0, HERO_LEAVES = 1, LOW_WOOD = 2, LOW_LEAVES = 3, VIEWS = 4;

float gustAt(const ForestSpec& f, Vector2 wind, Vector3 p, float seconds) {
    const float x = (p.x * wind.x + p.z * wind.y - f.gustSpeed * seconds) / std::max(f.gustScale, 1.f);
    const float wave = 0.5f + 0.5f * std::sin(TAU * x) * (0.6f + 0.4f * noise(p.x, p.z, f.gustScale * 2, 9));
    return wave * wave * wave;
}

// A tree's wind for a frame: the loop's position (its phase travels downwind) and the lean
// as instance pitch and bank (in its own frame, before its yaw). Wind 0 holds it at rest.
struct Sway { float frame = 0, pitch = 0, bank = 0; };
Sway swayOf(const ForestSpec& f, Vector2 wind, const Placement& p, const Tree& tree, float seconds) {
    Sway out;
    if (f.wind == 0) return out;
    const float bankX = Vector3Transform({ 0, 1, 0 }, MatrixRotateXYZ({ 0, 0, 0.1f })).x > 0 ? 1.f : -1.f;
    const float pitchZ = Vector3Transform({ 0, 1, 0 }, MatrixRotateXYZ({ 0.1f, 0, 0 })).z > 0 ? 1.f : -1.f;
    const float travel = (p.position.x * wind.x + p.position.z * wind.y) / std::max(f.gustScale, 1.f);
    const float frames = (float)tree.build.frames;
    out.frame = (seconds / tree.build.loopSeconds - travel * 0.5f) * frames + hashUnit(p.id, 3) * frames;
    const float lean = f.wind * tree.spec.sway * (1.2f + 4.f * f.gust * gustAt(f, wind, p.position, seconds));
    const Vector3 local = Vector3Transform({ wind.x, 0, wind.y }, MatrixRotateXYZ({ 0, -p.yaw * DEG2RAD, 0 }));
    out.pitch = lean * local.z * pitchZ;
    out.bank = lean * local.x * bankX;
    return out;
}

void place(g3d::Object3D& group, const std::shared_ptr<ForestState>& st, int t) {
    const ForestSpec& f = st->spec;
    const Vector3 eye = g3d::drawingCamera().position;
    const float seconds = t / std::max(f.fps, 1e-3f);
    Vector2 wind = f.windDirection;
    if (Vector2Length(wind) < 1e-6f) wind = { 1, 0 };
    wind = Vector2Normalize(wind);
    const size_t kinds = st->trees.size();
    for (auto& child : group.children) child->instances->list.clear();
    const float blend = std::max(f.blend, 0.01f);
    for (const Placement& p : st->at) {
        const size_t kind = (size_t)std::max(p.variant, 0) % kinds;
        const Tree& tree = *st->trees[kind];
        const size_t base = st->first[kind];
        const Vector3 mid{ p.position.x, p.position.y + tree.top * p.scale * 0.5f, p.position.z };
        const float d = Vector3Distance(eye, mid);
        // Coverage of each level: hero, reduced, impostor, and what is left after hiding.
        auto ramp = [blend](float d, float at) { return std::clamp((d - (at - blend * 0.5f)) / blend, 0.f, 1.f); };
        const float toLow = ramp(d, f.reducedFrom), toFar = ramp(d, f.impostorFrom);
        const float shown = f.hideFrom > 0 ? 1 - ramp(d, f.hideFrom) : 1;
        if (shown <= 0) continue;
        const Sway sway = swayOf(f, wind, p, tree, seconds);
        g3d::Instance in;
        in.position = p.position;
        in.yaw = p.yaw;
        in.scale = p.scale;
        in.frame = sway.frame;
        in.clipLength = tree.build.frames;
        in.pitch = sway.pitch;
        in.bank = sway.bank;
        // Screen doors: hero keeps [0, a), reduced [a, b), impostor [b, shown).
        const float a = 1 - toLow, bnd = std::max(a, 1 - toFar);
        auto put = [&](int child, float from, float to, const g3d::Instance& i0) {
            const float lo = std::min(from, shown), hi = std::min(to, shown);
            if (hi - lo < 1.f / 64) return;
            g3d::Instance i = i0;
            // One screen door per pair of neighbours: a band [lo, hi) is either [0, hi) or [lo, 1).
            if (lo <= 1e-4f) i.fade = hi;
            else if (hi >= 1 - 1e-4f) i.fade = -(1 - lo);
            else i.fade = hi;                         // three-way overlaps are short: favour the nearer
            group.children[base + (size_t)child]->instances->list.push_back(i);
        };
        if (a > 0) { put(HERO_WOOD, 0, a, in); put(HERO_LEAVES, 0, a, in); }
        if (bnd > a) { put(LOW_WOOD, a, bnd, in); put(LOW_LEAVES, a, bnd, in); }
        if (bnd < 1) {
            // Impostor: faces the camera; the two nearest views share it by a screen door.
            const Vector3 to = Vector3Subtract(eye, p.position);
            const float face = st->yawSign * std::atan2(to.x, to.z) * RAD2DEG;
            const Vector3 seen = Vector3Transform(to, MatrixRotateXYZ({ 0, -p.yaw * DEG2RAD, 0 }));
            const float local = std::atan2(seen.x, seen.z) * RAD2DEG;            // round the tree
            const int views = tree.build.views;
            float pos = local / 360.f * views;
            pos -= std::floor(pos / views) * views;
            const int v0 = (int)std::floor(pos) % views, v1 = (v0 + 1) % views;
            const float w = pos - std::floor(pos);
            g3d::Instance im = in;
            im.yaw = face; im.pitch = 0; im.bank = 0; im.frame = 0; im.clipLength = 1;
            const float lo = bnd, hi = shown;
            if (lo <= 1e-4f && hi >= 1 - 1e-4f) {        // wholly impostor: split by the view
                if (w < 1.f / 64) put(VIEWS + v0, 0, 1, im);
                else if (w > 63.f / 64) put(VIEWS + v1, 0, 1, im);
                else { put(VIEWS + v0, 0, 1 - w, im); put(VIEWS + v1, 1 - w, 1, im); }
            } else put(VIEWS + (w < 0.5f ? v0 : v1), lo, hi, im);
        }
    }
}

g3d::Object3D* forestInto(g3d::Object3D* group, const std::vector<Placement>& at, std::vector<TreePtr> trees, ForestSpec spec) {
    trees.erase(std::remove(trees.begin(), trees.end(), nullptr), trees.end());
    if (trees.empty()) throw std::invalid_argument("flora::addForest: no trees");
    auto st = std::make_shared<ForestState>();
    st->at = at;
    st->trees = trees;
    st->spec = spec;
    // Which way Instance::yaw turns: +z towards +x or towards -x.
    st->yawSign = Vector3Transform({ 0, 0, 1 }, MatrixRotateXYZ({ 0, 90 * DEG2RAD, 0 })).x > 0 ? 1.f : -1.f;
    for (const TreePtr& tr : trees) {
        st->first.push_back(group->children.size());
        auto add = [&](const std::shared_ptr<g3d::AnimatedMesh>& mesh, const g3d::Material& m) {
            auto in = std::make_shared<g3d::Instances>();
            in->mesh = mesh && mesh->frames > 0 ? mesh : tr->views[0];
            g3d::Object3D* o = group->add(in, m);
            if (!mesh || mesh->frames <= 0) o->visible = false;          // nothing at this level
        };
        add(tr->hero.wood, tr->wood);
        add(tr->hero.leaves, tr->leaves);
        add(tr->reduced.wood, tr->wood);
        add(tr->reduced.leaves, tr->leaves);
        g3d::Material imp = tr->impostor;
        imp.castShadow = spec.impostorShadows;
        for (const auto& v : tr->views) add(v, imp);
    }
    group->up = [st](int t, g3d::Object3D& self) { place(self, st, t); };
    return group;
}

}  // namespace

g3d::Object3D* addForest(g3d::Scene& scene, const std::vector<Placement>& at, std::vector<TreePtr> trees, ForestSpec spec) {
    return forestInto(scene.add(), at, std::move(trees), spec);
}
g3d::Object3D* addForest(g3d::Object3D& parent, const std::vector<Placement>& at, std::vector<TreePtr> trees, ForestSpec spec) {
    return forestInto(parent.add(), at, std::move(trees), spec);
}
g3d::Object3D* addTree(g3d::Scene& scene, TreePtr tree, Vector3 position, float yaw, float scale, ForestSpec spec) {
    Placement p{ position, yaw, scale, 0, 1 };
    return addForest(scene, { p }, { std::move(tree) }, spec);
}

namespace {

struct Growing {
    std::vector<Placement> at;
    std::vector<TreePtr> trees;
    std::function<float(int, const Placement&)> grow;
    ForestSpec spec;
    struct Body { g3d::Object3D *holder, *tilt; g3d::GeometryPtr wood, leaves; size_t kind; };
    std::vector<Body> bodies;
};

// Rewrites a growing tree's meshes for growth u and wind loop position `frame`.
void reshape(const g3d::GeometryPtr& geo, std::shared_ptr<const TreeGrowth> g, bool leaves, float u, float frame, bool still) {
    if (!geo) return;
    geo->update([g, leaves, u, frame, still](Mesh& m) {
        NodeGrowth ng;
        growNodes(*g, u, ng);
        const std::vector<Vert>& vs = leaves ? *g->leaves : *g->wood;
        const int count = std::min(m.vertexCount, (int)vs.size());
        for (int i = 0; i < count; i++) {
            const Vert& v = vs[(size_t)i];
            Vector3 p = grownVertex(ng, v, u);
            if (!still) p = Vector3Add(p, Vector3Scale(Vector3Subtract(windAt(g->spec, v, frame, g->frames), windAt(g->spec, v, 0, g->frames)), u));
            m.vertices[i * 3] = p.x; m.vertices[i * 3 + 1] = p.y; m.vertices[i * 3 + 2] = p.z;
        }
    });
}

void growAll(const std::shared_ptr<Growing>& st, int t) {
    const ForestSpec& f = st->spec;
    const float seconds = t / std::max(f.fps, 1e-3f);
    Vector2 wind = f.windDirection;
    if (Vector2Length(wind) < 1e-6f) wind = { 1, 0 };
    wind = Vector2Normalize(wind);
    for (size_t i = 0; i < st->bodies.size(); i++) {
        Growing::Body& b = st->bodies[i];
        const Placement& p = st->at[i];
        float u = st->grow ? st->grow(t, p) : 1.f;
        u = std::isfinite(u) ? std::clamp(u, 0.f, 1.f) : 1.f;
        b.holder->visible = u > 0;
        if (u <= 0) continue;
        const Tree& tree = *st->trees[b.kind];
        const Sway sway = swayOf(f, wind, p, tree, seconds);
        b.tilt->rotation = { sway.pitch * u, 0, sway.bank * u };
        reshape(b.wood, tree.growth, false, u, sway.frame, f.wind == 0);
        reshape(b.leaves, tree.growth, true, u, sway.frame, f.wind == 0);
    }
}

g3d::Object3D* growingInto(g3d::Object3D* group, const std::vector<Placement>& at, std::vector<TreePtr> trees,
                           std::function<float(int, const Placement&)> grow, ForestSpec spec) {
    trees.erase(std::remove(trees.begin(), trees.end(), nullptr), trees.end());
    if (trees.empty()) throw std::invalid_argument("flora::addGrowingTrees: no trees");
    if (at.size() > 256) throw std::invalid_argument("flora::addGrowingTrees: at most 256 trees (use addForest for more)");
    auto st = std::make_shared<Growing>();
    st->at = at;
    st->trees = trees;
    st->grow = std::move(grow);
    st->spec = spec;
    for (const Placement& p : at) {
        Growing::Body b;
        b.kind = (size_t)std::max(p.variant, 0) % trees.size();
        const Tree& tree = *trees[b.kind];
        const TreeGrowth& g = *tree.growth;
        b.holder = group->add();
        b.holder->position = p.position;
        b.holder->rotation = { 0, p.yaw, 0 };
        b.holder->scale = { p.scale, p.scale, p.scale };
        b.tilt = b.holder->add();
        if (!g.wood->empty()) { b.wood = geometryOf(g.wood, g.woodIdx); b.tilt->add(b.wood, tree.wood); }
        if (!g.leaves->empty()) { b.leaves = geometryOf(g.leaves, g.leafIdx); b.tilt->add(b.leaves, tree.leaves); }
        st->bodies.push_back(b);
    }
    group->up = [st](int t, g3d::Object3D&) { growAll(st, t); };
    return group;
}

}  // namespace

g3d::Object3D* addGrowingTrees(g3d::Scene& scene, const std::vector<Placement>& at, std::vector<TreePtr> trees,
                               std::function<float(int t, const Placement& p)> grow, ForestSpec wind) {
    return growingInto(scene.add(), at, std::move(trees), std::move(grow), wind);
}
g3d::Object3D* addGrowingTrees(g3d::Object3D& parent, const std::vector<Placement>& at, std::vector<TreePtr> trees,
                               std::function<float(int t, const Placement& p)> grow, ForestSpec wind) {
    return growingInto(parent.add(), at, std::move(trees), std::move(grow), wind);
}
g3d::Object3D* addGrowingTree(g3d::Scene& scene, TreePtr tree, Vector3 position, float yaw, float scale,
                              std::function<float(int t)> grow, ForestSpec wind) {
    Placement p{ position, yaw, scale, 0, 1 };
    std::function<float(int, const Placement&)> g;
    if (grow) g = [grow](int t, const Placement&) { return grow(t); };
    return addGrowingTrees(scene, { p }, { std::move(tree) }, std::move(g), wind);
}

}  // namespace flora
}  // namespace rmtn
