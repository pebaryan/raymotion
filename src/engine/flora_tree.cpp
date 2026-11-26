// flora:: trees -- skeletons, bark and leaf meshes, baked wind, impostors and forests
// (docs/flora-plan.md, M3). Everything here runs on the CPU and is a function of the spec.
#include "rmtn/flora.hpp"
#include "rmtn/splat.hpp"
#include "rmtn/geo3d.hpp"
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
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

struct LazyBake;
// Defined after Baked and LazyBake: a texture whose picture is one of a lazy bake's.
g3d::DynamicTexturePtr lazyTexture(std::shared_ptr<LazyBake> lazy, bool normal);

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
    // Spread colour into every clear texel, so mipmaps (a far tree's leaves) don't darken: a
    // clear texel left black drags each coarser level's average down by the empty share of the card.
    // Eight passes spread the nearest colour past the edges; the texels still clear after that
    // take the covered texels' mean, which is what the coarse levels average towards anyway.
    for (int pass = 0; pass < 8; pass++) {
        bool changed = false;
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
                if (n) { const unsigned char g = (unsigned char)std::max(1, sum / n); unsigned char* o = &next[((size_t)y * w + x) * 4]; o[0] = o[1] = o[2] = g; changed = true; }
            }
        pic->px.swap(next);
        if (!changed) break;
    }
    double sum = 0, n = 0;
    for (size_t i = 0; i < pic->px.size(); i += 4)
        if (pic->px[i + 3] > 0) { sum += pic->px[i + 1] * (double)pic->px[i + 3]; n += pic->px[i + 3]; }
    const unsigned char mean = (unsigned char)std::clamp(n > 0 ? std::lround(sum / n) : 1l, 1l, 255l);
    for (size_t i = 0; i < pic->px.size(); i += 4)
        if (pic->px[i + 1] == 0 && pic->px[i + 3] == 0) pic->px[i] = pic->px[i + 1] = pic->px[i + 2] = mean;
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

// ---- stylised crowns --------------------------------------------------------------------
// One flat-shaded triangle (its own three vertices), facing away from `inside`.
void facet(Soup& out, Vector3 a, Vector3 b, Vector3 c, Vector3 inside, Color col, const Vert& like) {
    Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
    const Vector3 mid = Vector3Scale(Vector3Add(Vector3Add(a, b), c), 1.f / 3);
    if (Vector3DotProduct(n, Vector3Subtract(mid, inside)) < 0) { std::swap(b, c); n = Vector3Negate(n); }
    const size_t first = out.v.size();
    for (Vector3 p : { a, b, c }) {
        Vert v = like;
        v.p = p;
        v.n = n;
        v.uv = { 0.5f, 0.5f };
        v.c = col;
        out.v.push_back(v);
    }
    for (int k = 0; k < 3; k++) out.idx.push_back((unsigned short)(first + k));
}

// Low-poly broadleaf crowns: the leaf cards gathered into clumps (farthest-point centres,
// each card to its nearest), each clump a jittered icosahedron in the cards' colour.
void clumpMesh(const TreeSpec& s, const Skeleton& sk, const std::vector<Card>& cs, Soup& out) {
    if (cs.empty()) return;
    const int K = std::clamp((int)cs.size() / 45, 5, 18);
    std::vector<int> centre;
    std::vector<float> far(cs.size(), 1e30f);
    size_t next = 0;
    for (size_t i = 1; i < cs.size(); i++) if (cs[i].at.y > cs[next].at.y) next = i;
    for (int k = 0; k < K && k < (int)cs.size(); k++) {
        centre.push_back((int)next);
        for (size_t i = 0; i < cs.size(); i++) far[i] = std::min(far[i], Vector3DistanceSqr(cs[i].at, cs[next].at));
        next = (size_t)(std::max_element(far.begin(), far.end()) - far.begin());
    }
    const size_t k = centre.size();
    std::vector<Vector3> sum(k, { 0, 0, 0 });
    std::vector<float> spread(k, 0.f), r(k, 0.f), g(k, 0.f), bl(k, 0.f);
    std::vector<int> count(k, 0), owner(cs.size(), 0);
    for (size_t i = 0; i < cs.size(); i++) {
        size_t best = 0;
        for (size_t j = 1; j < k; j++)
            if (Vector3DistanceSqr(cs[i].at, cs[(size_t)centre[j]].at) < Vector3DistanceSqr(cs[i].at, cs[(size_t)centre[best]].at)) best = j;
        owner[i] = (int)best;
        sum[best] = Vector3Add(sum[best], cs[i].at);
        r[best] += cs[i].c.r; g[best] += cs[i].c.g; bl[best] += cs[i].c.b;
        count[best]++;
    }
    std::vector<Vector3> mid(k);
    for (size_t j = 0; j < k; j++) mid[j] = Vector3Scale(sum[j], 1.f / std::max(count[j], 1));
    for (size_t i = 0; i < cs.size(); i++) spread[(size_t)owner[i]] += Vector3DistanceSqr(cs[i].at, mid[(size_t)owner[i]]);
    // The icosahedron.
    const float p = 1.618034f;
    const Vector3 ico[12] = { { -1, p, 0 }, { 1, p, 0 }, { -1, -p, 0 }, { 1, -p, 0 }, { 0, -1, p }, { 0, 1, p },
                              { 0, -1, -p }, { 0, 1, -p }, { p, 0, -1 }, { p, 0, 1 }, { -p, 0, -1 }, { -p, 0, 1 } };
    const int faces[20][3] = { { 0, 11, 5 }, { 0, 5, 1 }, { 0, 1, 7 }, { 0, 7, 10 }, { 0, 10, 11 }, { 1, 5, 9 }, { 5, 11, 4 },
                               { 11, 10, 2 }, { 10, 7, 6 }, { 7, 1, 8 }, { 3, 9, 4 }, { 3, 4, 2 }, { 3, 2, 6 }, { 3, 6, 8 },
                               { 3, 8, 9 }, { 4, 9, 5 }, { 2, 4, 11 }, { 6, 2, 10 }, { 8, 6, 7 }, { 9, 8, 1 } };
    Rng rng{ s.seed * 613ull + 29 };
    for (size_t j = 0; j < k; j++) {
        if (!count[j]) continue;
        const float radius = std::sqrt(spread[j] / count[j]) * 1.25f + s.leafSize * 0.45f;
        const Color tone{ (unsigned char)(r[j] / count[j]), (unsigned char)(g[j] / count[j]), (unsigned char)(bl[j] / count[j]), 255 };
        // The node nearest the clump carries it in the wind and in growth.
        int node = cs[(size_t)centre[j]].node;
        float best = 1e30f;
        for (size_t i = 0; i < cs.size(); i++)
            if (owner[i] == (int)j && Vector3DistanceSqr(cs[i].at, mid[j]) < best) { best = Vector3DistanceSqr(cs[i].at, mid[j]); node = cs[i].node; }
        Vert like;
        like.branch = (float)sk.nodes[(size_t)node].branch;
        like.reach = sk.nodes[(size_t)node].reach;
        like.node = like.sizer = node;
        like.pivot = mid[j];
        like.appear = 0.6f + 0.26f * rng.unit();
        Vector3 pts[12];
        for (int q = 0; q < 12; q++) {
            const Vector3 d = Vector3Normalize(ico[q]);
            const float jit = rng.range(0.82f, 1.15f);
            pts[q] = Vector3Add(mid[j], { d.x * radius * jit, d.y * radius * jit * 0.85f, d.z * radius * jit });
        }
        for (const auto& f : faces) {
            const Vector3 fc = Vector3Scale(Vector3Add(Vector3Add(pts[f[0]], pts[f[1]]), pts[f[2]]), 1.f / 3);
            facet(out, pts[f[0]], pts[f[1]], pts[f[2]], mid[j], scaleColor(tone, rng.range(0.9f, 1.07f) * (0.8f + 0.2f * occlusion(sk, fc))), like);
        }
    }
}

// Low-poly conifers: stacked seven-sided cones up the stem, each closed underneath.
void coneMesh(const TreeSpec& s, const Skeleton& sk, Soup& out) {
    const float H = s.height, yb = s.crownBase * H;
    const int tiers = std::clamp(s.whorls / 5, 3, 5), sides = 7;
    Rng rng{ s.seed * 389ull + 3 };
    for (int i = 0; i < tiers; i++) {
        const float t = (float)i / tiers;
        const float y0 = yb + (H - yb) * t * 0.9f, y1 = std::min(H, y0 + (H - yb) / tiers * 1.8f);
        const float prof = s.crownShape >= 0.5f ? (1 - t) * 0.95f + 0.05f : std::pow(std::sin(3.14159f * std::min(1.f, t * 0.95f + 0.05f)), 0.6f);
        const float radius = std::max(s.crownRadius * prof, s.crownRadius * 0.2f);
        int node = 0;                                            // the stem node at the tier's foot
        float best = 1e30f;
        for (size_t n = 0; n < sk.nodes.size(); n++)
            if (sk.nodes[n].stem && std::fabs(sk.nodes[n].p.y - y0) < best) { best = std::fabs(sk.nodes[n].p.y - y0); node = (int)n; }
        const Vector3 foot{ sk.nodes[(size_t)node].p.x, y0, sk.nodes[(size_t)node].p.z };
        const Vector3 apex = Vector3Add(foot, { 0, y1 - y0, 0 });
        Vert like;
        like.node = like.sizer = node;
        like.pivot = foot;
        like.appear = 0.6f + 0.26f * t;
        const Color tone = scaleColor(s.leafColor, rng.range(0.92f, 1.08f) * (1.1f + 0.2f * t));   // flat fills read darker
        const float turn = rng.range(0, TAU);
        for (int k = 0; k < sides; k++) {
            const float a0 = turn + TAU * k / sides, a1 = turn + TAU * (k + 1) / sides;
            const Vector3 p0 = Vector3Add(foot, { std::cos(a0) * radius, 0, std::sin(a0) * radius });
            const Vector3 p1 = Vector3Add(foot, { std::cos(a1) * radius, 0, std::sin(a1) * radius });
            const Vector3 in = Vector3Add(foot, { 0, (y1 - y0) * 0.3f, 0 });
            facet(out, p0, p1, apex, in, scaleColor(tone, rng.range(0.92f, 1.06f)), like);
            facet(out, p0, p1, foot, Vector3Add(foot, { 0, 1, 0 }), scaleColor(tone, 0.7f), like);      // underneath
        }
    }
}

// A soft canopy: two flat Gaussian splats per leaf card, lying in the card, lit by a sun
// from above and darkened inside the crown.
std::shared_ptr<const splat::Cloud> canopyOf(const TreeSpec& s, const Skeleton& sk, const std::vector<Card>& cs) {
    auto cloud = std::make_shared<splat::Cloud>();
    Rng rng{ s.seed * 977ull + 41 };
    for (const Card& c : cs) {
        const Vector3 n = Vector3Normalize(c.normal), up = Vector3Normalize(c.up), right = Vector3Normalize(Vector3CrossProduct(up, n));
        const Matrix m{ right.x, up.x, n.x, 0, right.y, up.y, n.y, 0, right.z, up.z, n.z, 0, 0, 0, 0, 1 };
        const Quaternion q = QuaternionNormalize(QuaternionFromMatrix(m));
        const float light = (0.62f + 0.38f * occlusion(sk, c.at)) * (0.9f + 0.2f * std::max(n.y, 0.f));
        for (int k = 0; k < 2; k++) {
            const Vector3 at = Vector3Add(c.at, Vector3Add(Vector3Scale(right, rng.range(-0.3f, 0.3f) * c.size),
                                                           Vector3Scale(up, rng.range(-0.3f, 0.3f) * c.size)));
            const float r = c.size * rng.range(0.3f, 0.42f);
            cloud->add(at, { r, r, r * 0.25f }, { q.w, q.x, q.y, q.z }, scaleColor(c.c, light * rng.range(0.92f, 1.08f)), 0.85f);
        }
    }
    cloud->bound();
    return cloud;
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

// ---- impostors: the hero tree drawn on the CPU from `views` directions round it, at each of
// `elevations` heights above the horizon ---------------------------------------------------
// The atlas is `views` pictures wide and `elevations` tall (row 0, the horizon, at the top).
// Every picture is an orthographic view centred on the point (0, pivot, 0), with image up
// being the world's up projected on the view plane, so a quad turned to face the viewer fully
// (its centre on that point) shows it undistorted there. The pivot is the crown's middle, where
// the pictures of one tree agree best (a tall bare trunk below it is what they disagree on).
// With one row the picture is as it always was: the ground at its foot, the pivot at side / 2.
struct LazyBake;
struct Baked {
    std::shared_ptr<Picture> colour, normal;
    float side = 1;                     // the horizon row's quad side
    float pivot = 0;                    // height of the point every picture is centred on
    std::vector<float> rowSide;         // each row's quad side
    std::shared_ptr<LazyBake> lazy;   // the pictures, if they are made on first use
};

// The pictures of a Baked made on first use. call_once makes it once however many threads ask;
// what it makes depends only on what the tree was built from, not on who asks first.
struct LazyBake {
    std::once_flag once;
    std::function<Baked()> run;
    Baked result;
    const Baked& get() { std::call_once(once, [this] { result = run(); run = nullptr; }); return result; }
};

g3d::DynamicTexturePtr lazyTexture(std::shared_ptr<LazyBake> lazy, bool normal) {
    auto t = std::make_shared<g3d::DynamicTexture>();
    t->every = 1 << 30;
    t->make = [lazy, normal](int) {
        const Baked& b = lazy->get();
        const Picture& pic = normal ? *b.normal : *b.colour;
        Image img{};
        img.width = pic.w; img.height = pic.h; img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        img.data = MemAlloc((unsigned)pic.px.size());
        std::memcpy(img.data, pic.px.data(), pic.px.size());
        return img;
    };
    return t;
}

float impostorElevation(int row, int rows) {
    if (rows <= 1) return 0;
    return (rows == 2 ? 50.f : rows == 3 ? 70.f : 90.f) * row / (rows - 1);
}


// The light a picture of the crown would lack. A full tree is shadowed inside by the shadow
// map; a picture of it is not, so the bake carries it as visibility. It is what the shadow map
// would say, not an estimate of it: the crown (cards cut out by their pictures, wood solid) is
// drawn as seen from each of a fixed set of directions to the light, a depth map per direction.
// A sample's visibility is the weighted share of those maps that see it lit, over the directions
// within FAN of the sky the viewer looks from (the sun is on the viewer's side more often than
// not); a texel averages its covered samples. It is sun-independent: a mean over suns.
// A card's side in the world: the leaves are two triangles each, so the mean triangle is half a card.
float cardSide(const Soup& leaves) {
    double tri = 0;
    for (size_t t = 0; t + 2 < leaves.idx.size(); t += 3)
        tri += 0.5f * Vector3Length(Vector3CrossProduct(Vector3Subtract(leaves.v[leaves.idx[t + 1]].p, leaves.v[leaves.idx[t]].p), Vector3Subtract(leaves.v[leaves.idx[t + 2]].p, leaves.v[leaves.idx[t]].p)));
    return (float)std::sqrt(2 * tri / std::max<size_t>(1, leaves.idx.size() / 3));
}

// The engine's cut-out test (CUTOUT_FS) at a distance. A texture seen from afar is read at a coarser
// mip, and its alpha is grown by 1 + mip / 4 before it meets the cutoff, so far cards keep their cover:
// the picture a far tree is baked from, and the shadows it is baked with, must be cut the same way or
// they show deeper, darker leaves through gaps the engine fills.
struct CutOut {
    std::vector<Picture> mips;
    float card = 1;
    CutOut(const Picture& leaf, float cardWorld) : card(std::max(cardWorld, 1e-3f)) {
        mips.push_back(leaf);
        while (mips.back().w > 1 && mips.back().h > 1) {
            const Picture& up = mips.back();
            Picture dn(up.w / 2, up.h / 2);
            for (int y = 0; y < dn.h; y++)
                for (int x = 0; x < dn.w; x++)
                    for (int ch = 0; ch < 4; ch++)
                        dn.at(x, y)[ch] = (unsigned char)((up.at(2 * x, 2 * y)[ch] + up.at(2 * x + 1, 2 * y)[ch] + up.at(2 * x, 2 * y + 1)[ch] + up.at(2 * x + 1, 2 * y + 1)[ch] + 2) / 4);
            mips.push_back(std::move(dn));
        }
    }
    // The mip level for samples `footprint` apart in the world.
    float mipFor(float footprint) const { return std::max(0.f, std::log2(std::max(1e-3f, footprint * (float)mips[0].w / card))); }
    bool away(float u, float v, float mip) const {
        return mips[(size_t)std::min<int>((int)std::lround(mip), (int)mips.size() - 1)].sample(u, v).w * (1 + 0.25f * mip) < 0.5f;
    }
};
constexpr float VIS_ONE = 127.f;   // the normal picture's alpha holds the light a texel gets, 127 for 1 and up to 255 for twice that
constexpr float kPictureSpread = 0.8f;

struct SunMaps {
    static constexpr float FAN = 50.f;                           // degrees: how far from the axis a direction still counts
    static constexpr float LEAN = 0.3f;                          // the axis tips this much (of the horizontal view direction, against 1 up) towards the viewer
    Vector3 centre{ 0, 0, 0 };
    float radius = 1, texel = 1, depthTol = 0, cutMip = 0;
    const CutOut* cutOut = nullptr;
    int N = 0;
    struct Map { Vector3 to, right, up; std::vector<float> depth; };
    std::vector<Map> maps;

    SunMaps(const Soup& wood, const Soup& leaves, const CutOut& cuts, int pixels) : N(pixels), cutOut(&cuts) {
        if (leaves.v.empty() || pixels <= 0) { N = 0; return; }
        Vector3 lo = leaves.v[0].p, hi = lo;
        for (const Soup* s : { &wood, &leaves }) for (const Vert& v : s->v) { lo = Vector3Min(lo, v.p); hi = Vector3Max(hi, v.p); }
        centre = Vector3Scale(Vector3Add(lo, hi), 0.5f);
        radius = 0.5f * Vector3Length(Vector3Subtract(hi, lo)) * 1.01f + 1e-3f;
        texel = 2 * radius / N;
        depthTol = 2 * texel;                                    // as the engine's shadows' bias is a texel or two
        cutMip = cuts.mipFor(texel);                    // the shadow pass reads the picture at the mip a shadow-map texel spans
        // The directions: rings of elevation, evenly spaced round, and straight up.
        const float ring[4] = { 15.f, 35.f, 55.f, 75.f };
        const int count[4] = { 12, 10, 7, 4 };
        auto add = [&](Vector3 to) {
            Map m;
            m.to = to;
            m.right = Vector3Normalize(Vector3CrossProduct({ 0.31f, 0.9f, 0.3f }, to));
            m.up = Vector3CrossProduct(to, m.right);
            m.depth.assign((size_t)N * N, -1e30f);
            for (const Soup* s : { &wood, &leaves }) draw(m, *s, s == &leaves);
            maps.push_back(std::move(m));
        };
        for (int r = 0; r < 4; r++)
            for (int k = 0; k < count[r]; k++) {
                const float e = ring[r] * DEG2RAD, a = TAU * (k + 0.5f * (r & 1)) / count[r];
                add({ std::cos(e) * std::sin(a), std::sin(e), std::cos(e) * std::cos(a) });
            }
        add({ 0, 1, 0 });
    }
    // Nearest to the light, per pixel.
    void draw(Map& m, const Soup& soup, bool cut) const {
        auto toMap = [&](Vector3 p) {
            const Vector3 d = Vector3Subtract(p, centre);
            return Vector3{ (Vector3DotProduct(d, m.right) / radius * 0.5f + 0.5f) * N, (Vector3DotProduct(d, m.up) / radius * 0.5f + 0.5f) * N, Vector3DotProduct(d, m.to) };
        };
        for (size_t t = 0; t + 2 < soup.idx.size(); t += 3) {
            const Vert &A = soup.v[soup.idx[t]], &B = soup.v[soup.idx[t + 1]], &C = soup.v[soup.idx[t + 2]];
            const Vector3 a = toMap(A.p), b = toMap(B.p), c = toMap(C.p);
            const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
            if (std::fabs(area) < 1e-9f) continue;
            const int x0 = std::max(0, (int)std::floor(std::min({ a.x, b.x, c.x }))), x1 = std::min(N - 1, (int)std::ceil(std::max({ a.x, b.x, c.x })));
            const int y0 = std::max(0, (int)std::floor(std::min({ a.y, b.y, c.y }))), y1 = std::min(N - 1, (int)std::ceil(std::max({ a.y, b.y, c.y })));
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++) {
                    const float px = x + 0.5f, py = y + 0.5f;
                    const float w0 = ((b.x - px) * (c.y - py) - (b.y - py) * (c.x - px)) / area;
                    const float w1 = ((c.x - px) * (a.y - py) - (c.y - py) * (a.x - px)) / area;
                    const float w2 = 1 - w0 - w1;
                    if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                    const float z = w0 * a.z + w1 * b.z + w2 * c.z;
                    float& at = m.depth[(size_t)y * N + x];
                    if (z <= at) continue;
                    if (cut && cutOut->away(w0 * A.uv.x + w1 * B.uv.x + w2 * C.uv.x, w0 * A.uv.y + w1 * B.uv.y + w2 * C.uv.y, cutMip)) continue;
                    at = z;
                }
        }
    }
    bool lit(const Map& m, Vector3 p) const {
        const Vector3 d = Vector3Subtract(p, centre);
        const int x = (int)std::floor((Vector3DotProduct(d, m.right) / radius * 0.5f + 0.5f) * N), y = (int)std::floor((Vector3DotProduct(d, m.up) / radius * 0.5f + 0.5f) * N);
        if (x < 0 || y < 0 || x >= N || y >= N) return true;
        return Vector3DotProduct(d, m.to) >= m.depth[(size_t)y * N + x] - depthTol;
    }
    // The directions that count for a viewer looking from `to` (unit, from the tree), with their weights.
    std::vector<std::pair<int, float>> fan(Vector3 to) const {
        const Vector3 axis = Vector3Normalize(Vector3Add(Vector3Scale({ to.x, 0, to.z }, LEAN), { 0, 1, 0 }));
        std::vector<std::pair<int, float>> out;
        const float cosFan = std::cos(FAN * DEG2RAD);
        for (size_t i = 0; i < maps.size(); i++) {
            const float c = Vector3DotProduct(axis, maps[i].to);
            if (c > cosFan) out.push_back({ (int)i, (c - cosFan) / (1 - cosFan) });
        }
        return out;
    }
    // The mean of max(n.l, 0) over a cone of normals about a mean normal whose cosine with l is c, the
    // cone's half-angle acos(cosPsi): n.l = A + B cos(phi), A = cosPsi c, B = sinPsi sin(c). The same as
    // the lit shader's coneNdl, which is how an impostor texel is lit.
    static float cone(float c, float cosPsi) {
        const float sinPsi = std::sqrt(std::max(1 - cosPsi * cosPsi, 0.f));
        const float A = cosPsi * c, B = sinPsi * std::sqrt(std::max(1 - c * c, 0.f));
        if (B < 1e-4f) return std::max(A, 0.f);
        if (A >= B) return A;
        if (A <= -B) return 0.f;
        return (A * std::acos(-A / B) + std::sqrt(B * B - A * A)) / 3.14159265f;
    }
    // What a texel's baked visibility is: the factor that makes the lit shader's light on the texel (the
    // cone of normals about its mean `mean`, `covered` samples of it, each weighted by its colour's
    // luminance since the picture holds the mean colour) the light its samples would get here (each
    // sample's own cosine, if the shadow maps see it lit). Over the fan of directions, so what it
    // folds in is shadowing and the cone's error alike; it can exceed 1 where the cone underlights.
    template <class S> float visibility(const std::vector<std::pair<int, float>>& f, const S* subs, int covered, Vector3 mean) const {
        const float len = std::min(Vector3Length(mean), 1.f);
        const Vector3 m = len > 1e-4f ? Vector3Scale(mean, 1 / Vector3Length(mean)) : Vector3{ 0, 1, 0 };
        float lums = 0;
        for (int k = 0; k < covered; k++) lums += subs[k].lum;
        float num = 0, den = 0, wsum = 0;
        for (const auto& [i, w] : f) {
            const Map& map = maps[(size_t)i];
            for (int k = 0; k < covered; k++)
                num += w * subs[k].lum * std::max(Vector3DotProduct(subs[k].n, map.to), 0.f) * (lit(map, subs[k].p) ? 1.f : 0.f);
            den += w * lums * cone(Vector3DotProduct(m, map.to), len);
            wsum += w;
        }
        const float floor = 0.02f * wsum * lums;                  // facing away from every light: neither is worth much
        return wsum > 0 ? (num + floor) / (den + floor) : 1.f;
    }
};

// Where the pictures stand, without the pictures: the horizon row's quad side, the point every
// picture is centred on, and each row's quad side.
Baked impostorLayout(float top, float reach, int elevations, float crownMiddle) {
    Baked b;
    b.side = std::max(top, 2 * reach) * 1.06f;
    const bool one = elevations == 1;                            // the picture the engine always had
    b.pivot = one || crownMiddle <= 0 ? b.side * 0.5f : crownMiddle;
    for (int row = 0; row < elevations; row++) {
        const float elev = impostorElevation(row, elevations) * DEG2RAD;
        // The outline's reach from the pivot: sideways the tree's reach, up and down the picture its
        // height range times cos plus its reach times sin.
        float side = b.side;
        if (!one) {
            const float far = std::max(b.pivot, top - b.pivot);
            side = 2 * 1.06f * std::max(reach, far * std::cos(elev) + reach * std::sin(elev));
        }
        b.rowSide.push_back(side);
    }
    return b;
}

Baked bakeViews(const Soup& wood, const Soup& leaves, const Picture& bark, const Picture& leaf, int views, int size, float top, float reach,
                bool cutLeaves = true, int elevations = 1, float crownMiddle = 0, bool shade = false, float trunkBelow = 1e9f) {
    const int S = size, SS = size * 2;
    Baked b = impostorLayout(top, reach, elevations, crownMiddle);
    const bool one = elevations == 1;
    b.colour = std::make_shared<Picture>(S * views, S * elevations);
    b.normal = std::make_shared<Picture>(S * views, S * elevations);
    const Vector3 pivot{ 0, b.pivot, 0 };
    const CutOut cutOut(leaf, cardSide(leaves));
    const std::unique_ptr<SunMaps> sun = shade ? std::make_unique<SunMaps>(wood, leaves, cutOut, std::clamp(S * 3, 192, 512)) : nullptr;
    for (int row = 0; row < elevations; row++) {
    const float elev = std::min(impostorElevation(row, elevations), 89.5f) * DEG2RAD;   // 90 is taken from 89.5: image up stays defined
    const float side = b.rowSide[(size_t)row];
    for (int k = 0; k < views; k++) {
        const float th = TAU * k / views;
        const float ce = std::cos(elev), se = std::sin(elev);
        const Vector3 dir{ std::sin(th) * ce, se, std::cos(th) * ce }, world{ 0, 1, 0 };
        Vector3 right = Vector3CrossProduct(Vector3Negate(dir), world);
        Vector3 up = world;                                       // image up in the world
        if (!one) { right = Vector3Normalize(right); up = Vector3CrossProduct(dir, right); }
        std::vector<float> depth((size_t)SS * SS, -1e30f);
        std::vector<Vector4> col((size_t)SS * SS, { 0, 0, 0, 0 });
        std::vector<Vector3> nor((size_t)SS * SS, { 0, 0, 0 });
        std::vector<Vector3> world3((size_t)SS * SS, { 0, 0, 0 });   // where each sample is, for its visibility
        auto toScreen = [&](Vector3 p) {
            const float v = one ? p.y / side : Vector3DotProduct(Vector3Subtract(p, pivot), up) / side + 0.5f;   // the horizon row as it always was
            return Vector3{ (Vector3DotProduct(p, right) / side + 0.5f) * SS, (1 - v) * SS, Vector3DotProduct(p, dir) };
        };
        const float bakeMip = cutOut.mipFor(side / (float)SS);   // the engine's cut at this picture's scale
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
                        if (cut && cutOut.away(u, v, bakeMip)) continue;
                        depth[at] = z;
                        col[at] = { tex.x * (w0 * A.c.r + w1 * B.c.r + w2 * C.c.r) / 255, tex.y * (w0 * A.c.g + w1 * B.c.g + w2 * C.c.g) / 255,
                                    tex.z * (w0 * A.c.b + w1 * B.c.b + w2 * C.c.b) / 255, 1 };
                        Vector3 n = Vector3Normalize(Vector3Add(Vector3Add(Vector3Scale(A.n, w0), Vector3Scale(B.n, w1)), Vector3Scale(C.n, w2)));
                        if (cut && facing < 0) n = Vector3Negate(n);
                        nor[at] = n;
                        world3[at] = Vector3Add(Vector3Add(Vector3Scale(A.p, w0), Vector3Scale(B.p, w1)), Vector3Scale(C.p, w2));
                    }
            }
        };
        if (elev * RAD2DEG < trunkBelow) raster(wood, bark, false);   // low-poly trees seen from above: crown only
        raster(leaves, leaf, cutLeaves);
        const std::vector<std::pair<int, float>> fan = sun ? sun->fan(dir) : std::vector<std::pair<int, float>>{};
        // 2x2 down to the cell; colour averaged over what is covered, coverage as alpha.
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                Vector4 sum{ 0, 0, 0, 0 };
                Vector3 ns{ 0, 0, 0 };
                struct Sub { Vector3 n, p; float lum; } subs[4];
                int covered = 0;
                for (int j = 0; j < 2; j++)
                    for (int i = 0; i < 2; i++) {
                        const size_t at = (size_t)(y * 2 + j) * SS + (x * 2 + i);
                        if (col[at].w <= 0) continue;
                        sum = { sum.x + col[at].x, sum.y + col[at].y, sum.z + col[at].z, sum.w + 1 };
                        ns = Vector3Add(ns, nor[at]);
                        subs[covered++] = { nor[at], world3[at], 0.2126f * col[at].x + 0.7152f * col[at].y + 0.0722f * col[at].z + 1e-4f };
                    }
                unsigned char* c = b.colour->at(k * S + x, row * S + y);
                unsigned char* nn = b.normal->at(k * S + x, row * S + y);
                // Tangent space of the quad: u along `right`, v down, out towards the viewer.
                // Many to a texel: more than one way the surface faces. The mean keeps its length (the spread) when there are rows.
                const Vector3 n = sum.w <= 0 ? dir : one ? Vector3Normalize(ns) : Vector3Scale(ns, 1.f / sum.w);
                // A low-poly picture's spread is kept at kPictureSpread of what the facets say: the picture
                // is lit as a spread, and fully so it would be more sensitive to the sun than the crown is.
                const float spreadK = cutLeaves ? 1.f : kPictureSpread;
                const Vector3 tsn{ spreadK * Vector3DotProduct(n, right), -spreadK * (one ? n.y : Vector3DotProduct(n, up)), spreadK * Vector3DotProduct(n, dir) };
                nn[0] = (unsigned char)std::lround((tsn.x * 0.5f + 0.5f) * 255);
                nn[1] = (unsigned char)std::lround((tsn.y * 0.5f + 0.5f) * 255);
                nn[2] = (unsigned char)std::lround((tsn.z * 0.5f + 0.5f) * 255);
                float vis = 1;
                if (sun && covered > 0) vis = sun->visibility(fan, subs, covered, n);
                nn[3] = sum.w > 0 ? (unsigned char)std::lround(std::clamp(vis, 0.f, 2.f) * VIS_ONE) : (unsigned char)VIS_ONE;
                if (sum.w > 0) {
                    c[0] = (unsigned char)std::lround(sum.x / sum.w * 255);
                    c[1] = (unsigned char)std::lround(sum.y / sum.w * 255);
                    c[2] = (unsigned char)std::lround(sum.z / sum.w * 255);
                }
                c[3] = (unsigned char)std::lround(sum.w / 4 * 255);
            }
    }
    }
    // Spread colours and normals into the clear texels (mipmaps), within each cell.
    for (int pass = 0; pass < 6; pass++) {
        Picture cn = *b.colour, nn = *b.normal;
        for (int y = 0; y < S * elevations; y++)
            for (int x = 0; x < S * views; x++) {
                if (b.colour->at(x, y)[3] > 0 || cn.at(x, y)[0] + cn.at(x, y)[1] + cn.at(x, y)[2] > 0) continue;
                int sc[3] = { 0, 0, 0 }, sn[4] = { 0, 0, 0, 0 }, n = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        const int xx = x + dx, yy = y + dy;
                        if (yy < 0 || yy >= S * elevations || xx < 0 || xx >= S * views || xx / S != x / S || yy / S != y / S) continue;
                        const unsigned char* q = b.colour->at(xx, yy);
                        if (q[0] + q[1] + q[2] == 0) continue;
                        for (int ch = 0; ch < 3; ch++) { sc[ch] += q[ch]; sn[ch] += b.normal->at(xx, yy)[ch]; }
                        sn[3] += b.normal->at(xx, yy)[3];
                        n++;
                    }
                if (!n) continue;
                for (int ch = 0; ch < 3; ch++) { cn.at(x, y)[ch] = (unsigned char)(sc[ch] / n); nn.at(x, y)[ch] = (unsigned char)(sn[ch] / n); }
                nn.at(x, y)[3] = (unsigned char)(sn[3] / n);          // and the light they get: a clear texel is not fully lit
            }
        *b.colour = std::move(cn);
        *b.normal = std::move(nn);
    }
    // What the passes did not reach takes its cell's mean (of the texels the trees covers): a far tree's
    // mips average a whole cell, and a clear texel left black, or facing the viewer with all the light,
    // drags the average down (or away) by the share of the cell the tree does not fill.
    for (int row = 0; row < elevations; row++)
        for (int k = 0; k < views; k++) {
            double c[3] = { 0, 0, 0 }, n[4] = { 0, 0, 0, 0 };
            int covered = 0;
            for (int y = 0; y < S; y++)
                for (int x = 0; x < S; x++) {
                    const unsigned char* q = b.colour->at(k * S + x, row * S + y);
                    if (q[3] == 0) continue;
                    const unsigned char* m = b.normal->at(k * S + x, row * S + y);
                    for (int ch = 0; ch < 3; ch++) c[ch] += q[ch];
                    for (int ch = 0; ch < 4; ch++) n[ch] += m[ch];
                    covered++;
                }
            if (!covered) continue;
            for (int y = 0; y < S; y++)
                for (int x = 0; x < S; x++) {
                    unsigned char* q = b.colour->at(k * S + x, row * S + y);
                    unsigned char* m = b.normal->at(k * S + x, row * S + y);
                    if (q[3] > 0 || q[0] + q[1] + q[2] > 0) continue;
                    for (int ch = 0; ch < 3; ch++) q[ch] = (unsigned char)std::lround(c[ch] / covered);
                    for (int ch = 0; ch < 4; ch++) m[ch] = (unsigned char)std::lround(n[ch] / covered);
                }
        }
    return b;
}

std::shared_ptr<g3d::AnimatedMesh> quad(float side, float u0, float u1, float v0 = 0, float v1 = 1) {
    auto am = std::make_shared<g3d::AnimatedMesh>();
    am->frames = 1;
    am->vertices = 4;
    const float h = side * 0.5f;
    am->positions = { -h, 0, 0, h, 0, 0, h, side, 0, -h, side, 0 };
    am->normals = { 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 };
    am->centre = { 0, h, 0 };
    am->radius = side * 0.72f;
    const std::vector<float> pos = am->positions;
    am->rest = std::make_shared<g3d::Geometry>([pos, u0, u1, v0, v1] {
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
        const float uv[8] = { u0, v1, u1, v1, u1, v0, u0, v0 };
        std::copy(uv, uv + 8, m.texcoords);
        std::memset(m.colors, 255, 16);
        const unsigned short id[6] = { 0, 1, 2, 0, 2, 3 };
        std::copy(id, id + 6, m.indices);
        UploadMesh(&m, false);
        return m;
    });
    return am;
}

// Paper cut-out: the front view in flat fills: two tones of leaf (lit and shaded, from the
// picture's brightness smoothed into big shapes) and one of bark, with a dark cut edge.
std::shared_ptr<Picture> paperPicture(const Picture& baked, const TreeSpec& s) {
    const int W = baked.w, H = baked.h;
    auto hue = [](float r, float g, float b) { const float t = std::max(r + g + b, 1e-3f); return Vector3{ r / t, g / t, b / t }; };
    const Vector3 leafHue = hue(s.leafColor.r, s.leafColor.g, s.leafColor.b), autumnHue = hue(s.autumnColor.r, s.autumnColor.g, s.autumnColor.b);
    const Vector3 barkHue = hue(s.bark.r, s.bark.g, s.bark.b);
    std::vector<float> lum((size_t)W * H, 0.f), leafy((size_t)W * H, 0.f);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const unsigned char* q = baked.at(x, y);
            if (q[3] < 128) continue;
            const Vector3 h = hue(q[0], q[1], q[2]);
            lum[(size_t)y * W + x] = 0.3f * q[0] + 0.59f * q[1] + 0.11f * q[2];
            leafy[(size_t)y * W + x] = std::min(Vector3Distance(h, leafHue), Vector3Distance(h, autumnHue)) < Vector3Distance(h, barkHue) ? 1.f : 0.f;
        }
    // Smooth brightness and leafiness over a few percent of the picture (two box passes).
    auto blur = [W, H](std::vector<float> v, int r) {
        std::vector<float> t(v.size());
        for (int pass = 0; pass < 2; pass++) {
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    float sum = 0; int n = 0;
                    for (int k = std::max(0, x - r); k <= std::min(W - 1, x + r); k++) { sum += v[(size_t)y * W + k]; n++; }
                    t[(size_t)y * W + x] = sum / n;
                }
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    float sum = 0; int n = 0;
                    for (int k = std::max(0, y - r); k <= std::min(H - 1, y + r); k++) { sum += t[(size_t)k * W + x]; n++; }
                    v[(size_t)y * W + x] = sum / n;
                }
        }
        return v;
    };
    std::vector<float> cover((size_t)W * H);
    for (size_t i = 0; i < cover.size(); i++) cover[i] = baked.px[i * 4 + 3] >= 128 ? 1.f : 0.f;
    const int r = std::max(1, W / 40);
    const std::vector<float> sl = blur(lum, r), sc = blur(cover, r), sf = blur(leafy, std::max(1, r / 3));
    double sum = 0;
    int n = 0;
    for (size_t i = 0; i < cover.size(); i++)
        if (cover[i] > 0 && leafy[i] > 0) { sum += sl[i] / std::max(sc[i], 1e-3f); n++; }
    const float split = n ? (float)(sum / n) : 128;
    Color lit = scaleColor(s.leafColor, 1.3f), shade = scaleColor(s.leafColor, 0.72f);
    if (s.deciduous && s.season > 0.5f) { lit = scaleColor(s.autumnColor, 1.2f); shade = scaleColor(s.autumnColor, 0.7f); }
    const Color bark = scaleColor(s.bark, 0.9f), edge = scaleColor(s.leafColor, 0.3f);
    const int e = std::max(1, W / 200);                                          // the cut edge, px
    auto pic = std::make_shared<Picture>(W, H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const size_t i = (size_t)y * W + x;
            unsigned char* o = pic->at(x, y);
            Color c = lit;
            bool onEdge = false;
            if (cover[i] > 0)
                for (int dy = -e; dy <= e && !onEdge; dy++)
                    for (int dx = -e; dx <= e; dx++) {
                        const int xx = x + dx, yy = y + dy;
                        if (xx < 0 || yy < 0 || xx >= W || yy >= H || cover[(size_t)yy * W + xx] == 0) { onEdge = true; break; }
                    }
            if (cover[i] == 0) c = shade;                                         // under the cut (mipmaps)
            else if (onEdge) c = edge;
            else if (sf[i] < 0.5f) c = bark;
            else c = sl[i] / std::max(sc[i], 1e-3f) > split ? lit : shade;
            o[0] = c.r; o[1] = c.g; o[2] = c.b;
            o[3] = cover[i] > 0 ? 255 : 0;
        }
    return pic;
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
    b.elevations = std::clamp(b.elevations, 1, 8);
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
    std::shared_ptr<Picture> paper;
    float paperSide = 1;
    std::shared_ptr<const splat::Cloud> canopy;
    if (b.style == TreeStyle::Splat) {
        woodMesh(s, sk, heroWood, std::max(s.trunkRadius * 0.2f, s.height * 0.008f), 6);
        woodMesh(s, sk, lowWood, s.trunkRadius * 0.45f, 4);
        if (s.form == TreeForm::Palm) { frondMesh(s, sk, fronds, heroLeaves); lowLeaves = heroLeaves; }
        else canopy = canopyOf(s, sk, cs);
    } else if (b.style == TreeStyle::LowPoly) {
        woodMesh(s, sk, heroWood, std::max(s.trunkRadius * 0.3f, s.height * 0.015f), 5);   // limbs only: the clumps hide the rest
        woodMesh(s, sk, lowWood, s.trunkRadius * 0.45f, 4);
        if (s.form == TreeForm::Conifer) coneMesh(s, sk, heroLeaves);
        else if (s.form == TreeForm::Palm) frondMesh(s, sk, fronds, heroLeaves);
        else clumpMesh(s, sk, cs, heroLeaves);
        lowLeaves = heroLeaves;
    } else {
        woodMesh(s, sk, heroWood, s.form == TreeForm::Palm ? s.trunkRadius * 0.5f : twig * 1.5f, 8);
        woodMesh(s, sk, lowWood, s.form == TreeForm::Palm ? s.trunkRadius * 0.5f : std::max(twig * 4, s.trunkRadius * 0.16f), 5);
        if (s.form == TreeForm::Palm) { frondMesh(s, sk, fronds, heroLeaves); lowLeaves = heroLeaves; }
        else { leafMesh(s, sk, cs, 1, heroLeaves); leafMesh(s, sk, cs, b.reducedLeaves, lowLeaves); }
    }
    if (b.style == TreeStyle::PaperCut) {
        // The realistic tree's front view, flattened, on one card that stands where the tree does.
        float top = 0, reach = 0;
        for (const Soup* so : { &heroWood, &heroLeaves })
            for (const Vert& v : so->v) { top = std::max(top, v.p.y); reach = std::max(reach, std::hypot(v.p.x, v.p.z)); }
        const Baked front = bakeViews(heroWood, heroLeaves, *barkPic, *leafPic, 1, std::min(b.viewSize * 2, 512), top, reach);
        paper = paperPicture(*front.colour, s);
        paperSide = front.side;
        const float h = paperSide * 0.5f;
        Soup card;
        const Vector3 corners[4] = { { -h, 0, 0 }, { h, 0, 0 }, { h, paperSide, 0 }, { -h, paperSide, 0 } };
        const Vector2 uvs[4] = { { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 } };
        for (int k = 0; k < 4; k++) {
            Vert v;
            v.p = corners[k]; v.n = { 0, 0, 1 }; v.uv = uvs[k]; v.c = WHITE;
            card.v.push_back(v);
        }
        card.idx = { 0, 1, 2, 0, 2, 3 };
        heroWood = card; lowWood = card;
        heroLeaves = Soup{}; lowLeaves = Soup{};
    }

    auto t = std::make_shared<Tree>();
    t->spec = s;
    t->build = b;
    t->canopy = canopy;
    t->hero = { bake(s, heroWood, b.frames), bake(s, heroLeaves, b.frames) };
    t->reduced = { bake(s, lowWood, b.frames), bake(s, lowLeaves, b.frames) };
    t->heroTriangles = (long long)(heroWood.idx.size() + heroLeaves.idx.size()) / 3;
    t->reducedTriangles = (long long)(lowWood.idx.size() + lowLeaves.idx.size()) / 3;
    for (const Soup* so : { &heroWood, &heroLeaves })
        for (const Vert& v : so->v) { t->top = std::max(t->top, v.p.y); t->radius = std::max(t->radius, std::hypot(v.p.x, v.p.z)); }
    {   // The crown's colour from above: leaf vertex colours x the leaf picture's opaque texels.
        double vc[3] = { 0, 0, 0 }, tx[3] = { 0, 0, 0 };
        for (const Vert& v : heroLeaves.v) { vc[0] += v.c.r; vc[1] += v.c.g; vc[2] += v.c.b; }
        double solid = 0;
        for (size_t px = 0; px + 3 < leafPic->px.size(); px += 4)
            if (leafPic->px[px + 3] >= 128) { tx[0] += leafPic->px[px]; tx[1] += leafPic->px[px + 1]; tx[2] += leafPic->px[px + 2]; solid++; }
        if (!heroLeaves.v.empty() && b.style != TreeStyle::PaperCut) {
            const double n = (double)heroLeaves.v.size();
            const bool plain = b.style == TreeStyle::LowPoly;      // flat colours: no picture
            for (int k = 0; k < 3; k++) {
                const double tex = plain || solid < 1 ? 1.0 : tx[k] / solid / 255.0;
                (&t->crownColour.r)[k] = (unsigned char)std::clamp(std::lround(vc[k] / n * tex), 0l, 255l);
            }
        } else t->crownColour = mixColor(s.leafColor, s.bark, s.deciduous && s.season > 0.9f ? 1.f : 0.f);
        t->crownColour.a = 255;
    }
    if (canopy)
        for (size_t i = 0; i < canopy->size(); i++) {
            const float* p = &canopy->position[i * 3];
            t->top = std::max(t->top, p[1]);
            t->radius = std::max(t->radius, std::hypot(p[0], p[2]));
        }
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

    Picture white(1, 1);
    white.px = { 255, 255, 255, 255 };
    Baked baked;
    if (b.style != TreeStyle::PaperCut && b.style != TreeStyle::Splat) {   // a paper card is its own picture; splats stay near
        const bool solid = b.style == TreeStyle::LowPoly;
        // The crown's middle: the mean height of its leaf vertices (half the height if bare).
        float crown = 0;
        for (const Vert& v : heroLeaves.v) crown += v.p.y;
        crown = heroLeaves.v.empty() ? t->top * 0.5f : crown / (float)heroLeaves.v.size();
        // The pictures are made when a texture first needs them (a draw), not here: building a tree
        // for a list or a prepare costs nothing for them. They depend only on what is captured.
        baked = impostorLayout(t->top, t->radius, b.elevations, crown);
        auto lazy = std::make_shared<LazyBake>();
        auto hw = std::make_shared<const Soup>(heroWood), hl = std::make_shared<const Soup>(heroLeaves);
        const float top = t->top, reach = t->radius;
        lazy->run = [=] {
            Picture one(1, 1);
            one.px = { 255, 255, 255, 255 };
            return bakeViews(*hw, *hl, solid ? one : *barkPic, solid ? one : *leafPic, b.views, b.viewSize, top, reach, !solid,
                             b.elevations, crown, b.elevations > 1,
                             b.style == TreeStyle::LowPoly ? 45.f : 1e9f);
        };
        baked.lazy = lazy;
        t->viewPivot = baked.pivot;
        for (int row = 0; row < b.elevations; row++) {
            t->viewElevation.push_back(impostorElevation(row, b.elevations));
            t->viewSide.push_back(baked.rowSide[(size_t)row]);
            for (int k = 0; k < b.views; k++) {
                // Half a texel in from each cell's edge, so mipmaps of one view don't take the next's.
                // (Vertically only where there are rows to take from: one row keeps its whole height.)
                const float pad = 1.5f / (b.viewSize * b.views), padV = b.elevations > 1 ? 1.5f / (b.viewSize * b.elevations) : 0.f;
                t->views.push_back(quad(baked.rowSide[(size_t)row], (float)k / b.views + pad, (float)(k + 1) / b.views - pad,
                                        (float)row / b.elevations + padV, (float)(row + 1) / b.elevations - padV));
            }
        }
    }

    g3d::Material wood;
    wood.color = WHITE; wood.roughness = 0.95f; wood.lineWidth = 0;
    wood.dynamicMap = texture(barkPic);
    g3d::Material leaves;
    leaves.color = WHITE; leaves.roughness = 0.75f; leaves.lineWidth = 0;
    leaves.dynamicMap = texture(leafPic);
    leaves.alphaCutoff = 0.5f; leaves.doubleSided = true;
    leaves.translucency = 0.55f; leaves.translucencyColor = { 255, 245, 190, 255 };
    if (b.style == TreeStyle::LowPoly) {                       // flat colours; ink lines welcome
        wood = {}; wood.color = WHITE; wood.roughness = 1;
        leaves = {}; leaves.color = WHITE; leaves.roughness = 0.9f; leaves.doubleSided = s.form == TreeForm::Palm;
        leaves.lineWidth = 0.6f;                 // ink: lighter inside a crown
        leaves.translucency = 0.2f; leaves.translucencyColor = { 255, 245, 190, 255 };
    }
    g3d::Material imp = leaves;
    if (baked.lazy) {
        imp.dynamicMap = lazyTexture(baked.lazy, false);
        imp.dynamicNormalMap = lazyTexture(baked.lazy, true);
        imp.alphaCutoff = 0.5f; imp.doubleSided = true; imp.lineWidth = 0; imp.normalAlphaShade = b.elevations > 1;
        imp.translucency = 0.4f;
    }
    if (b.style == TreeStyle::PaperCut) {                      // one cut-out card, lit flat
        wood = {}; wood.color = WHITE; wood.roughness = 1;
        wood.dynamicMap = texture(paper);
        wood.alphaCutoff = 0.5f; wood.doubleSided = true;
    }
    for (g3d::Material* m : { &wood, &leaves, &imp }) {
        m->shading = b.shading;
        m->toon.highlight = 0.12f;                 // foliage and bark: no glossy spots
    }
    t->wood = wood;
    t->leaves = leaves;
    t->impostor = imp;
    return t;
}

// ---- forests -----------------------------------------------------------------------------
namespace {

// One square of the forest's ground with trees in it, and its canopy patch (made on first need).
struct CanopyCell {
    int ix = 0, iz = 0;
    std::vector<uint32_t> trees;            // placement indices, ascending
    Vector3 centre{ 0, 0, 0 };              // the trees' mean (mid-height): distances are taken to it
    float height = 0;                       // the trees' mean height (for the size they show at)
    bool interior = false;                  // has a patch: it and its eight neighbours are well wooded
    Vector3 sphere{ 0, 0, 0 };              // a sphere holding the patch (for culling and shadow reach)
    float sphereRadius = 0;
    std::shared_ptr<g3d::Instances> inst;
    g3d::Object3D* child = nullptr;
    bool built = false;
};

struct ForestState {
    float yawSign = 1;
    std::vector<Placement> at;
    std::vector<TreePtr> trees;
    ForestSpec spec;
    std::vector<size_t> first;      // each kind's first child
    std::vector<g3d::Object3D*> canopies;   // per placement: its splat crown's tilt (or null)
    float pitchSign = 1;            // Instance::pitch's sign that tips a quad's top towards +z
    // Far canopy (ForestSpec::canopyFrom > 0): the cells, and the grid they sit on.
    std::vector<CanopyCell> cells;
    std::vector<int> cellAt;        // cellsX * cellsZ: index into `cells`, or -1
    int cellsX = 0, cellsZ = 0;
    float cellSize = 64, minX = 0, minZ = 0;
    bool splats = false;            // some tree is a splat crown (its tilt object must be hidden with it)
};

// ---- far canopy: a height field over each cell's crowns ------------------------------------
// Each tree is a crown: a dome (broadleaf, palm: the upper half of an ellipsoid from the crown's
// base to its top, vertical at its edge) or a cone (conifer), its outline pushed in and out by
// noise. A node's height is the highest crown over it, its crown colour that tree's, darker
// towards the crown base; between crowns it lies at the crown base. Past 1.15 crown radii the
// forest thins out: the surface slopes to the ground and its alpha falls through the material's
// cut-out, so the forest's edge is a noisy line. Everything is a function of the node's world
// position and the trees near it, so neighbouring cells meet exactly.
constexpr float CANOPY_REACH = 2.7f;        // crown radii from a tree's stem that can still be under its influence
constexpr float CANOPY_TILE = 24.f;          // metres a repeat of the patches' grain picture covers

// The grain picture of the patches, a darkening multiplier (mean about 0.75) in clumps of leaves
// from half a metre up, and an alpha that only ever roughens the cut-out near a forest's edge
// (it stays above 0.6). Tileable; made when a draw first needs it.
g3d::DynamicTexturePtr canopyTexture() {
    auto t = std::make_shared<g3d::DynamicTexture>();
    t->every = 1 << 30;
    t->make = [](int) {
        const int S = 512;
        auto octave = [S](int x, int y, int period, uint32_t seed) {
            const float fx = (float)x / S * period, fy = (float)y / S * period;
            const int i = (int)std::floor(fx), j = (int)std::floor(fy);
            float u = fx - i, v = fy - j;
            u = u * u * (3 - 2 * u); v = v * v * (3 - 2 * v);
            auto at = [&](int a, int b) { return hashUnit((uint32_t)(((a % period) + period) % period) * 7919u + 13u, (uint32_t)(((b % period) + period) % period) + seed * 104729u); };
            return (at(i, j) * (1 - u) + at(i + 1, j) * u) * (1 - v) + (at(i, j + 1) * (1 - u) + at(i + 1, j + 1) * u) * v;
        };
        Image img{};
        img.width = img.height = S; img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        img.data = MemAlloc((unsigned)(S * S * 4));
        unsigned char* d = (unsigned char*)img.data;
        for (int y = 0; y < S; y++)
            for (int x = 0; x < S; x++) {
                const float n = 0.15f * octave(x, y, 24, 2) + 0.35f * octave(x, y, 48, 3) + 0.35f * octave(x, y, 96, 4) + 0.15f * octave(x, y, 192, 5);
                const float k = smoothstep(0.44f, 0.56f, n);
                const unsigned char g = (unsigned char)std::lround(255.f * (0.28f + 0.72f * k));
                unsigned char* q = d + ((size_t)y * S + x) * 4;
                q[0] = q[1] = q[2] = g;
                q[3] = (unsigned char)std::lround(255.f * (0.62f + 0.38f * octave(x, y, 96, 6)));
            }
        return img;
    };
    return t;
}
constexpr float CANOPY_GAIN = 1.8f;        // the patch's colour against the crown colour (the trees' own crowns are lit and shaded)
// How bright a patch must be (Instance::shade) to match the crowns it stands for. A field of
// crowns is a field of lit spheres: lighter looking down-sun, darker into it, darker still under a low
// sun, where crowns shade each other; a flat patch is lit by the sun's height alone. The factor is
// measured, not modelled: canopyTests with RMTN_L4_CALIBRATE=1 renders trees and patches at two shades over a grid of view
// heights, sun heights and the angle between the sun and the camera, solves for the shade that matches the mean linear
// luminance of the trees in the cells that have patches, and prints this table. Blended in log space.
// Axes: view elevation (7 20 35 50 70), sun elevation (15 25 40 55 70), angle between sun and camera seen from above (0 45 90 135 180).
constexpr float CANOPY_SHADE[5][5][5] = {
    {
        { 1.06f, 1.05f, 1.02f, 1.05f, 1.05f },
        { 1.08f, 1.09f, 1.07f, 1.00f, 0.94f },
        { 0.94f, 0.94f, 0.89f, 0.74f, 0.62f },
        { 0.81f, 0.78f, 0.72f, 0.60f, 0.52f },
        { 0.69f, 0.67f, 0.61f, 0.55f, 0.51f },
    },
    {
        { 1.02f, 1.06f, 1.10f, 1.10f, 1.01f },
        { 1.15f, 1.14f, 1.10f, 1.01f, 0.99f },
        { 1.08f, 1.05f, 0.95f, 0.82f, 0.76f },
        { 0.98f, 0.94f, 0.86f, 0.74f, 0.69f },
        { 0.88f, 0.86f, 0.79f, 0.73f, 0.70f },
    },
    {
        { 1.09f, 1.11f, 1.12f, 1.08f, 0.98f },
        { 1.15f, 1.13f, 1.10f, 1.05f, 1.06f },
        { 1.08f, 1.04f, 0.96f, 0.89f, 0.87f },
        { 0.97f, 0.94f, 0.87f, 0.80f, 0.78f },
        { 0.89f, 0.87f, 0.82f, 0.77f, 0.75f },
    },
    {
        { 1.11f, 1.12f, 1.12f, 1.09f, 1.01f },
        { 1.15f, 1.12f, 1.09f, 1.07f, 1.08f },
        { 1.05f, 1.01f, 0.95f, 0.90f, 0.90f },
        { 0.95f, 0.92f, 0.86f, 0.81f, 0.80f },
        { 0.87f, 0.85f, 0.81f, 0.78f, 0.77f },
    },
    {
        { 1.13f, 1.14f, 1.12f, 1.09f, 1.04f },
        { 1.11f, 1.10f, 1.07f, 1.08f, 1.10f },
        { 1.00f, 0.98f, 0.94f, 0.95f, 0.96f },
        { 0.91f, 0.90f, 0.86f, 0.86f, 0.86f },
        { 0.85f, 0.84f, 0.83f, 0.82f, 0.82f },
    },
};
// `toSun` and `toEye` are unit vectors from the patch.
float canopyShade(Vector3 toSun, Vector3 toEye) {
    static const float viewAt[5] = { 7, 20, 35, 50, 70 }, sunAt[5] = { 15, 25, 40, 55, 70 }, relAt[5] = { 0, 45, 90, 135, 180 };
    auto locate = [](const float* axis, float v, int& i, float& t) {
        v = std::clamp(v, axis[0], axis[4]);
        i = 0;
        while (i < 3 && v > axis[i + 1]) i++;
        t = (v - axis[i]) / (axis[i + 1] - axis[i]);
    };
    const float view = std::asin(std::clamp(toEye.y, -1.f, 1.f)) / DEG2RAD, sunEl = std::asin(std::clamp(toSun.y, -1.f, 1.f)) / DEG2RAD;
    const float hs = std::hypot(toSun.x, toSun.z), he = std::hypot(toEye.x, toEye.z);
    float rel = 0;
    if (hs > 1e-4f && he > 1e-4f) rel = std::acos(std::clamp((toSun.x * toEye.x + toSun.z * toEye.z) / (hs * he), -1.f, 1.f)) / DEG2RAD;
    int vi, si, ri;
    float vt, st, rt;
    locate(viewAt, view, vi, vt);
    locate(sunAt, sunEl, si, st);
    locate(relAt, rel, ri, rt);
    float s = 0;
    for (int a = 0; a < 2; a++)
        for (int b = 0; b < 2; b++)
            for (int c = 0; c < 2; c++)
                s += (a ? vt : 1 - vt) * (b ? st : 1 - st) * (c ? rt : 1 - rt) * std::log(CANOPY_SHADE[vi + a][si + b][ri + c]);
    return 1.08f * std::exp(s);   // the table is measured over cells of 5 px trees; a 1280x720 30-degree shot of 3 km of forest (canopy test f) wants a little more
}


float canopyReach(const ForestState& st) {
    float r = 0;
    for (const Placement& p : st.at) {
        const Tree& tr = *st.trees[(size_t)std::max(p.variant, 0) % st.trees.size()];
        r = std::max(r, std::max(tr.spec.crownRadius, 0.8f) * p.scale * CANOPY_REACH);
    }
    return r;
}

void buildPatch(ForestState& st, CanopyCell& cell) {
    Color meanCrown{ 0, 0, 0, 255 };
    {
        float r = 0, g = 0, b = 0;
        for (const TreePtr& t : st.trees) { r += t->crownColour.r; g += t->crownColour.g; b += t->crownColour.b; }
        const float n = (float)std::max<size_t>(st.trees.size(), 1);
        meanCrown = { (unsigned char)(r / n), (unsigned char)(g / n), (unsigned char)(b / n), 255 };
    }
    const ForestSpec& f = st.spec;
    const float C = st.cellSize;
    const int n = std::clamp((int)std::ceil(C / std::max(f.canopyDetail, 0.25f)), 4, 120);
    const float step = C / (float)n;
    const int N = n + 3;                                    // the nodes of the cell, and one more each side for normals
    const long gx0 = (long)cell.ix * n - 1, gz0 = (long)cell.iz * n - 1;     // global node numbers of node 0
    auto nodeX = [&](int i) { return st.minX + (float)(gx0 + i) * step; };
    auto nodeZ = [&](int j) { return st.minZ + (float)(gz0 + j) * step; };
    const size_t total = (size_t)N * N;
    std::vector<float> wob(total), bump(total), tint(total);
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++) {
            const float x = nodeX(i), z = nodeZ(j);
            const size_t k = (size_t)j * N + i;
            wob[k] = 1 + 0.5f * (noise(x, z, 6.f, 71) * 2 - 1);
            bump[k] = (noise(x, z, std::max(1.6f * step, 4.f), 72) * 2 - 1);
            tint[k] = noise(x, z, 24.f, 73);
        }
    // The trees that can reach this cell's nodes: its own and the eight round it.
    std::vector<uint32_t> near;
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            const int cx = cell.ix + dx, cz = cell.iz + dz;
            if (cx < 0 || cz < 0 || cx >= st.cellsX || cz >= st.cellsZ) continue;
            const int c = st.cellAt[(size_t)cz * st.cellsX + cx];
            if (c >= 0) near.insert(near.end(), st.cells[(size_t)c].trees.begin(), st.cells[(size_t)c].trees.end());
        }
    std::sort(near.begin(), near.end());
    constexpr float NONE = -1e30f;
    std::vector<float> h(total, NONE), e(total, 0.f), rel(total, 0.f), gSum(total, 0.f), gWeight(total, 0.f);
    std::vector<int> owner(total, -1);
    float groundMin = 1e30f;
    for (uint32_t id : near) {
        const Placement& p = st.at[id];
        const Tree& tr = *st.trees[(size_t)std::max(p.variant, 0) % st.trees.size()];
        const float s = p.scale, R = std::max(tr.spec.crownRadius, 0.8f) * s;
        const float top = p.position.y + tr.top * s;
        const float base = p.position.y + std::clamp(tr.spec.crownBase, 0.f, 0.9f) * tr.top * s;
        const float span = std::max(top - base, 0.2f), gap = base + 0.62f * span;   // between crowns the canopy closes at 0.62 of the crown
        const bool cone = tr.spec.form == TreeForm::Conifer;
        groundMin = std::min(groundMin, p.position.y);
        const float reach = R * CANOPY_REACH;
        const int i0 = std::max(0, (int)std::floor((p.position.x - reach - nodeX(0)) / step)), i1 = std::min(N - 1, (int)std::ceil((p.position.x + reach - nodeX(0)) / step));
        const int j0 = std::max(0, (int)std::floor((p.position.z - reach - nodeZ(0)) / step)), j1 = std::min(N - 1, (int)std::ceil((p.position.z + reach - nodeZ(0)) / step));
        for (int j = j0; j <= j1; j++)
            for (int i = i0; i <= i1; i++) {
                const size_t k = (size_t)j * N + i;
                const float dx = nodeX(i) - p.position.x, dz = nodeZ(j) - p.position.z;
                const float u = std::sqrt(dx * dx + dz * dz) / (R * std::max(wob[k], 0.3f));
                if (u > 0.85f) continue;
                const float presence = 1 - smoothstep(0.4f, 0.85f, u);
                float hc;
                if (u >= 1) hc = gap;
                else if (cone) hc = gap + (top - gap) * (1 - std::pow(u, 1.4f));
                else hc = gap + (top - gap) * std::pow(1 - std::pow(u, 2.4f), 0.6f);
                const float relative = std::clamp((hc - base) / span, 0.f, 1.f);
                hc += bump[k] * span * (u < 1 ? 0.3f : 0.1f);
                if (hc > h[k]) { h[k] = hc; rel[k] = relative; owner[k] = (int)id; }
                e[k] = std::max(e[k], presence);
                gSum[k] += p.position.y * presence;
                gWeight[k] += presence;
            }
    }
    if (groundMin > 1e29f) groundMin = 0;
    // Surface height, alpha and colour per node.
    std::vector<float> surface(total), alpha(total);
    for (size_t k = 0; k < total; k++) {
        if (gWeight[k] <= 0 || h[k] <= NONE) { surface[k] = groundMin; alpha[k] = 0; continue; }
        const float ground = gSum[k] / gWeight[k];
        const float lift = smoothstep(0.05f, 0.85f, e[k]);
        surface[k] = ground + (h[k] - ground) * (0.5f + 0.5f * lift);   // the edge hangs at about the crowns' base, over the trunks
        alpha[k] = std::clamp((e[k] - 0.04f) / 0.25f, 0.f, 1.f);
    }
    // The mesh: the cell's own (n + 1)^2 nodes; quads that can show only.
    const int V = n + 1;
    std::vector<float> pos((size_t)V * V * 3), nor((size_t)V * V * 3);
    std::vector<unsigned char> col((size_t)V * V * 4);
    Vector3 lo{ 1e30f, 1e30f, 1e30f }, hi{ -1e30f, -1e30f, -1e30f };
    for (int j = 0; j < V; j++)
        for (int i = 0; i < V; i++) {
            const size_t k = (size_t)(j + 1) * N + (size_t)(i + 1);
            const size_t o = ((size_t)j * V + i);
            const Vector3 p{ nodeX(i + 1), surface[k], nodeZ(j + 1) };
            const float dhx = (surface[k + 1] - surface[k - 1]) / (2 * step), dhz = (surface[k + N] - surface[k - N]) / (2 * step);
            const Vector3 nv = Vector3Normalize({ -dhx * 0.12f, 1, -dhz * 0.12f });   // crowns are rougher than the nodes: lit a little flatter
            pos[o * 3] = p.x; pos[o * 3 + 1] = p.y; pos[o * 3 + 2] = p.z;
            nor[o * 3] = nv.x; nor[o * 3 + 1] = nv.y; nor[o * 3 + 2] = nv.z;
            lo = Vector3Min(lo, p); hi = Vector3Max(hi, p);
            Color c = { 60, 90, 40, 255 };
            if (owner[k] >= 0) {
                const Placement& q = st.at[(size_t)owner[k]];
                const Tree& tr = *st.trees[(size_t)std::max(q.variant, 0) % st.trees.size()];
                const float shade = 0.25f + 0.75f * std::pow(rel[k], 1.2f);
                const float vary = 0.88f + 0.24f * hashUnit(q.id, 5), patch = 0.9f + 0.2f * tint[k];
                const Color cc = mixColor(tr.crownColour, meanCrown, 0.5f);
                const float warm = 0.94f + 0.12f * hashUnit(q.id, 6);
                const float steep = 1 - 0.4f * smoothstep(0.6f, 2.5f, std::hypot(dhx, dhz));
                const float m = shade * vary * patch * steep * CANOPY_GAIN;
                c = { (unsigned char)std::clamp(std::lround(cc.r * m * warm), 0l, 255l),
                      (unsigned char)std::clamp(std::lround(cc.g * m), 0l, 255l),
                      (unsigned char)std::clamp(std::lround(cc.b * m / warm), 0l, 255l), 255 };
            }
            col[o * 4] = c.r; col[o * 4 + 1] = c.g; col[o * 4 + 2] = c.b;
            col[o * 4 + 3] = (unsigned char)std::lround(alpha[k] * 255.f);
        }
    std::vector<unsigned short> idx;
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            float a = 0;
            for (int dj = 0; dj < 2; dj++)
                for (int di = 0; di < 2; di++) a = std::max(a, alpha[(size_t)(j + 1 + dj) * N + (size_t)(i + 1 + di)]);
            if (a < 0.45f) continue;
            const unsigned short a0 = (unsigned short)(j * V + i), a1 = (unsigned short)(a0 + 1), b0 = (unsigned short)(a0 + V), b1 = (unsigned short)(b0 + 1);
            const bool flip = (((unsigned)(i * 73856093) ^ (unsigned)(j * 19349663) ^ (unsigned)(cell.ix * 83492791) ^ (unsigned)(cell.iz * 2654435761u)) >> 7) & 1u;
            if (flip) { for (unsigned short v : { a0, b0, b1, a0, b1, a1 }) idx.push_back(v); }
            else for (unsigned short v : { a0, b0, a1, a1, b0, b1 }) idx.push_back(v);   // counter-clockwise from above, the diagonal either way
        }
    cell.built = true;
    if (idx.empty()) return;
    auto am = std::make_shared<g3d::AnimatedMesh>();
    am->frames = 1;
    am->vertices = V * V;
    am->positions = pos;
    am->normals = nor;
    am->centre = Vector3Scale(Vector3Add(lo, hi), 0.5f);
    am->radius = Vector3Distance(lo, hi) * 0.5f + 0.05f;
    am->rest = std::make_shared<g3d::Geometry>([pos, nor, col, idx] {
        Mesh m{};
        m.vertexCount = (int)(pos.size() / 3);
        m.triangleCount = (int)idx.size() / 3;
        m.vertices = (float*)MemAlloc((unsigned)(pos.size() * sizeof(float)));
        m.normals = (float*)MemAlloc((unsigned)(nor.size() * sizeof(float)));
        m.texcoords = (float*)MemAlloc((unsigned)(m.vertexCount * 2 * sizeof(float)));
        m.colors = (unsigned char*)MemAlloc((unsigned)col.size());
        m.indices = (unsigned short*)MemAlloc((unsigned)(idx.size() * sizeof(unsigned short)));
        std::copy(pos.begin(), pos.end(), m.vertices);
        std::copy(nor.begin(), nor.end(), m.normals);
        for (int k = 0; k < m.vertexCount; k++) { m.texcoords[k * 2] = (pos[(size_t)k * 3] + 0.7f * pos[(size_t)k * 3 + 1]) / CANOPY_TILE; m.texcoords[k * 2 + 1] = (pos[(size_t)k * 3 + 2] - 0.43f * pos[(size_t)k * 3 + 1]) / CANOPY_TILE; }
        std::copy(col.begin(), col.end(), m.colors);
        std::copy(idx.begin(), idx.end(), m.indices);
        UploadMesh(&m, false);
        return m;
    });
    cell.inst->mesh = am;
}

// Children per tree kind, in this order: the four levels, then one child per impostor picture
// (VIEWS + row * views + direction), then, with impostorShadows, the same again for the
// shadow-only copies that face the light (VIEWS + pictures + the same index).
constexpr int HERO_WOOD = 0, HERO_LEAVES = 1, LOW_WOOD = 2, LOW_LEAVES = 3, VIEWS = 4;

// Which pictures of an impostor a direction falls between: the two nearest directions round
// the tree (v0, v1; weight w on v1), the two nearest elevation rows (r0, r1; weight e on r1),
// and the direction's own elevation in degrees.
struct ViewPick { int v0 = 0, v1 = 0, r0 = 0, r1 = 0; float w = 0, e = 0, elevation = 0; };

// `to` points from the tree to the viewer (or the light), in the forest's space; `turn` is the
// cosine and sine of the tree's own yaw, which the pictures turn with.
ViewPick pickView(const Tree& tree, Vector3 to, Vector2 turn) {
    ViewPick k;
    const float seenX = to.x * turn.x - to.z * turn.y, seenZ = to.x * turn.y + to.z * turn.x;   // `to` turned by -yaw
    const float local = std::atan2(seenX, seenZ) * RAD2DEG;             // round the tree
    const int views = tree.build.views;
    float pos = local / 360.f * views;
    pos -= std::floor(pos / views) * views;
    k.v0 = (int)std::floor(pos) % views;
    k.v1 = (k.v0 + 1) % views;
    k.w = pos - std::floor(pos);
    const int rows = (int)tree.viewElevation.size();
    k.elevation = std::max(0.f, std::atan2(to.y, std::hypot(to.x, to.z)) * RAD2DEG);
    k.r0 = k.r1 = 0;
    for (int r = 0; r + 1 < rows; r++)
        if (k.elevation >= tree.viewElevation[(size_t)r]) {
            k.r0 = r;
            k.r1 = r + 1;
            // Between two rows the pictures disagree more than neighbouring directions do (a tree's
            // height shows differently from each), so their blend is narrowed to the middle half of
            // the way, smoothly: near a row it shows that row alone, which is also fewer draws.
            const float u = std::clamp((k.elevation - tree.viewElevation[(size_t)r]) / (tree.viewElevation[(size_t)r + 1] - tree.viewElevation[(size_t)r]), 0.f, 1.f);
            const float t = std::clamp((u - 0.25f) / 0.5f, 0.f, 1.f);
            k.e = t * t * (3 - 2 * t);
        }
    return k;
}

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
    const g3d::Scene* weatherScene = g3d::drawingScene();
    const bool inWind = weatherScene && weatherScene->weather.enabled && !st->spec.ownWind;     // a copy, for this frame only
    const ForestSpec windy = inWind ? inWeather(st->spec, weatherScene->weather) : ForestSpec{};
    const ForestSpec& f = inWind ? windy : st->spec;
    const Vector3 eye = g3d::drawingCamera().position;
    const float seconds = t / std::max(f.fps, 1e-3f);
    Vector2 wind = f.windDirection;
    if (Vector2Length(wind) < 1e-6f) wind = { 1, 0 };
    wind = Vector2Normalize(wind);
    const size_t kinds = st->trees.size();
    for (auto& child : group.children) if (child->instances) child->instances->list.clear();
    const float blend = std::max(f.blend, 0.01f);
    // The light the impostors' shadows are drawn for: the first that casts one.
    struct { bool on = false, directional = true; Vector3 toward{ 0, 1, 0 }, at{ 0, 0, 0 }, target{ 0, 0, 0 }; float extent = 0; } caster;
    if (f.impostorShadows || (f.canopyShadows && (f.canopyFrom > 0 || f.canopyBelow > 0)))
        if (const g3d::Scene* scene = g3d::drawingScene())
            for (const g3d::Light& l : scene->lights) {
                if (!l.castShadow) continue;
                caster.on = true;
                caster.directional = l.type == g3d::Light::Type::Directional;
                caster.at = l.position;
                caster.target = l.target;
                caster.extent = l.shadowExtent;
                const Vector3 away = Vector3Subtract(l.position, l.target);
                caster.toward = Vector3Length(away) > 1e-6f ? Vector3Normalize(away) : Vector3{ 0, 1, 0 };
                break;
            }
    // Coverage of each level: hero, reduced, impostor, and what is left after hiding.
    auto ramp = [blend](float d, float at) { return std::clamp((d - (at - blend * 0.5f)) / blend, 0.f, 1.f); };
    // One tree: `cap` is the share of the screen door its cell's canopy leaves it (1 = all).
    auto one = [&](const Placement& p, float cap) {
        const size_t kind = (size_t)std::max(p.variant, 0) % kinds;
        const Tree& tree = *st->trees[kind];
        const size_t base = st->first[kind];
        const Vector3 mid{ p.position.x, p.position.y + tree.top * p.scale * 0.5f, p.position.z };
        const float d = Vector3Distance(eye, mid);
        const bool paper = tree.build.style == TreeStyle::PaperCut;     // one card, no impostors
        const bool near = paper || tree.views.empty();                   // nothing to stand in far away
        const float toLow = paper ? 0.f : ramp(d, f.reducedFrom), toFar = near ? 0.f : ramp(d, f.impostorFrom);
        const float shown = std::min(f.hideFrom > 0 ? 1 - ramp(d, f.hideFrom) : 1, cap);
        if (shown <= 0) {
            if (tree.canopy) st->canopies[(size_t)(&p - st->at.data())]->visible = false;
            return;
        }
        const Sway sway = swayOf(f, wind, p, tree, seconds);
        if (tree.canopy) {                                       // the splat crown leans with the wood
            g3d::Object3D* tilt = st->canopies[(size_t)(&p - st->at.data())];
            tilt->rotation = { sway.pitch, 0, sway.bank };
            tilt->visible = shown > 0;
        }
        g3d::Instance in;
        in.position = p.position;
        in.yaw = p.yaw;
        in.scale = p.scale;
        in.frame = sway.frame;
        in.clipLength = tree.build.frames;
        in.pitch = sway.pitch;
        in.bank = sway.bank;
        if (paper) {                                             // the card turns to face the camera
            const Vector3 to = Vector3Subtract(eye, p.position);
            in.yaw = st->yawSign * std::atan2(to.x, to.z) * RAD2DEG;
        }
        // Screen doors: hero keeps [0, a), reduced [a, b), impostor [b, shown).
        const float a = 1 - toLow, bnd = std::max(a, 1 - toFar);
        auto put = [&](int child, float from, float to, const g3d::Instance& i0) {
            const float lo = std::min(from, shown), hi = std::min(to, shown);
            if (hi - lo < 1.f / 64) return;
            g3d::Instance i = i0;
            // A band [lo, hi) of the screen door: [0, hi), [lo, 1), or between (Instance::fadeFrom).
            if (lo <= 1e-4f) i.fade = hi;
            else if (hi >= 1 - 1e-4f) i.fade = -(1 - lo);
            else { i.fade = hi; i.fadeFrom = lo; }
            group.children[base + (size_t)child]->instances->list.push_back(i);
        };
        if (a > 0) { put(HERO_WOOD, 0, a, in); put(HERO_LEAVES, 0, a, in); }
        if (bnd > a) { put(LOW_WOOD, a, bnd, in); put(LOW_LEAVES, a, bnd, in); }
        if (bnd < 1) {
            // Impostor: faces the camera (fully, from above); the four nearest pictures share it.
            const int views = tree.build.views, rows = (int)tree.viewElevation.size(), pictures = views * rows;
            const Vector3 pivot{ p.position.x, p.position.y + tree.viewPivot * p.scale, p.position.z };
            const Vector2 turn{ std::cos(p.yaw * DEG2RAD), std::sin(p.yaw * DEG2RAD) };
            // Turning a picture's quad to face along a direction `to` (from the tree): the yaw, the
            // tilt (none with one row: it only turns about the vertical, as it always did) and where
            // the quad's up axis points after both, which moves its middle onto the tree's pivot.
            struct Pose { float yaw = 0, pitch = 0; Vector3 up{ 0, 1, 0 }; };
            auto poseFor = [&](Vector3 to, float elevation) {
                Pose q;
                q.yaw = st->yawSign * std::atan2(to.x, to.z) * RAD2DEG;
                if (rows > 1) {
                    q.pitch = -st->pitchSign * elevation;
                    // The top leans back from the viewer: up is (0, cos e, -sin e) turned by the yaw
                    // (Instance turns about y: (x, z) -> (z sin yaw, z cos yaw) for x = 0).
                    const float back = -std::sin(elevation * DEG2RAD);
                    q.up = { back * std::sin(q.yaw * DEG2RAD), std::cos(elevation * DEG2RAD), back * std::cos(q.yaw * DEG2RAD) };
                }
                return q;
            };
            // The picture on row `row`, its centre on the pivot (moved by `shift`).
            auto standIn = [&](int row, const Pose& q, Vector3 shift) {
                g3d::Instance im = in;
                im.yaw = q.yaw;
                im.pitch = q.pitch; im.bank = 0; im.frame = 0; im.clipLength = 1;
                // The quad stands on its bottom edge and tilts about it.
                if (rows > 1) im.position = Vector3Subtract(Vector3Add(pivot, shift), Vector3Scale(q.up, 0.5f * tree.viewSide[(size_t)row] * p.scale));
                return im;
            };
            const Vector3 to = Vector3Subtract(eye, pivot);
            const ViewPick k = pickView(tree, to, turn);
            const Pose face = poseFor(to, k.elevation);
            const float lo = bnd, hi = shown;
            if (lo <= 1e-4f && hi >= 1 - 1e-4f) {        // wholly impostor: split by the pictures
                if (rows == 1) {
                    const float w = k.w;
                    if (w < 1.f / 64) put(VIEWS + k.v0, 0, 1, standIn(0, face, {}));
                    else if (w > 63.f / 64) put(VIEWS + k.v1, 0, 1, standIn(0, face, {}));
                    else { put(VIEWS + k.v0, 0, 1 - w, standIn(0, face, {})); put(VIEWS + k.v1, 1 - w, 1, standIn(0, face, {})); }
                } else {
                    struct Pic { int row, v; float weight; };
                    const Pic pic[4] = { { k.r0, k.v0, (1 - k.w) * (1 - k.e) }, { k.r0, k.v1, k.w * (1 - k.e) },
                                         { k.r1, k.v0, (1 - k.w) * k.e }, { k.r1, k.v1, k.w * k.e } };
                    // Bands in order, their edges on the screen door's 16 levels so neighbours tile; a
                    // picture with less than a level to itself is left out.
                    float sum = 0, from = 0;
                    for (int i = 0; i < 4; i++) {
                        sum += pic[i].weight;
                        const float edge = i == 3 ? 1.f : std::min(1.f, std::round(sum * 16.f) / 16.f);
                        if (edge - from < 1.f / 64) continue;
                        put(VIEWS + pic[i].row * views + pic[i].v, from, edge, standIn(pic[i].row, face, {}));
                        from = edge;
                    }
                }
            } else put(VIEWS + (k.e < 0.5f ? k.r0 : k.r1) * views + (k.w < 0.5f ? k.v0 : k.v1), lo, hi, standIn(k.e < 0.5f ? k.r0 : k.r1, face, {}));
            // Its shadow: a copy that faces the light and shows the picture nearest the light's
            // direction, centred on the same pivot (the picture drawn does not receive shadows, so
            // the copy cannot shadow it). Only where the light's map reaches: a directional light's
            // covers shadowExtent round its target.
            // (Only where the light's map reaches: a directional light's covers shadowExtent round its target.)
            const Vector3 rel = Vector3Subtract(pivot, caster.target);
            if (f.impostorShadows && caster.on && (!caster.directional || Vector3Length(Vector3Subtract(rel, Vector3Scale(caster.toward, Vector3DotProduct(rel, caster.toward)))) <=
                                                       caster.extent * 1.4143f + tree.radius * p.scale + tree.top * p.scale)) {
                const Vector3 toLight = caster.directional ? caster.toward : Vector3Normalize(Vector3Subtract(caster.at, pivot));
                const ViewPick s = pickView(tree, toLight, turn);
                const int row = s.e < 0.5f ? s.r0 : s.r1, v = s.w < 0.5f ? s.v0 : s.v1;
                put(VIEWS + pictures + row * views + v, lo, hi, standIn(row, poseFor(toLight, s.elevation), {}));
            }
        }
    };
    if (st->cells.empty()) {
        for (const Placement& p : st->at) one(p, 1.f);
        return;
    }
    // With canopy: by cell. A cell's trees and its patch share the screen door: the trees keep
    // [0, 1 - c), the patch the rest, c being how far the cell's trees are into the canopy band.
    const float aspect = 4.f;                                 // wider than any view: only to skip what cannot show
    const float cBlend = std::max(f.canopyBlend > 0 ? f.canopyBlend : 0.5f * st->cellSize, 0.01f);   // a whole cell fades together
    const g3d::Camera& cam = g3d::drawingCamera();
    // The sun the patches are lit by: the first directional light (toward it, unit).
    bool haveSun = false;
    Vector3 toSun{ 0, 1, 0 };
    if (f.canopyShade <= 0)
        if (const g3d::Scene* scene = g3d::drawingScene())
            for (const g3d::Light& l : scene->lights)
                if (l.type == g3d::Light::Type::Directional) {
                    const Vector3 away = Vector3Subtract(l.position, l.target);
                    if (Vector3Length(away) > 1e-6f) { toSun = Vector3Normalize(away); haveSun = true; }
                    break;
                }
    const float rows = g3d::drawingViewportHeight();
    // Pixels a metre of height covers at a metre's distance (0: unknown, or an orthographic camera).
    const float perMetre = !cam.orthographic && rows > 0 && cam.fov > 0.1f ? rows / (2.f * std::tan(cam.fov * 0.5f * DEG2RAD)) : 0.f;
    // Patches are tuned for cameras that look across a forest from low: above `canopyMaxElevation` (the camera's
    // elevation as seen from the forest's middle) a patch cannot match the trees' brightness (their light depends on
    // the view and the sky), so every cell keeps its trees.
    bool canopyAllowed = true;
    if (f.canopyMaxElevation > 0 && !st->cells.empty()) {
        const Vector3 mid = Vector3Scale(Vector3Add(st->cells.front().centre, st->cells.back().centre), 0.5f);
        const Vector3 d = Vector3Subtract(eye, mid);
        canopyAllowed = std::atan2(d.y, std::hypot(d.x, d.z)) * RAD2DEG <= f.canopyMaxElevation;
    }
    for (CanopyCell& cell : st->cells) {
        const float dCell = Vector3Distance(eye, cell.centre);
        // Where this cell's patch takes over: `canopyFrom` metres, or the distance at which its trees are
        // `canopyBelow` px tall, whichever is the more distant.
        float from = f.canopyFrom;
        if (f.canopyBelow > 0 && perMetre > 0) from = std::max(from, cell.height * perMetre / f.canopyBelow);
        from *= 1.f + 0.25f * (2.f * hashUnit((uint32_t)(cell.ix * 7919 + 13), (uint32_t)cell.iz * 104729u + 99u) - 1.f);   // a ragged front, not a line
        const float c = canopyAllowed && cell.interior && from > 0 ? std::clamp((dCell - (from - cBlend * 0.5f)) / cBlend, 0.f, 1.f) : 0.f;
        const float hide = f.hideFrom > 0 ? 1 - ramp(dCell, f.hideFrom) : 1.f;
        if (c < 1)
            for (uint32_t i : cell.trees) one(st->at[i], 1.f - c);
        else if (st->splats)
            for (uint32_t i : cell.trees)
                if (g3d::Object3D* tilt = st->canopies[i]) tilt->visible = false;
        if (!cell.child) continue;
        const float lo = 1.f - c, hi = std::min(1.f, hide);
        cell.child->visible = false;
        if (c <= 0 || hi - lo < 1.f / 64) continue;
        const bool inView = g3d::visibleSphere(cam, aspect, cell.sphere, cell.sphereRadius);
        const Vector3 rel = Vector3Subtract(cell.sphere, caster.target);
        const bool inReach = caster.on && f.canopyShadows &&
            (!caster.directional || Vector3Length(Vector3Subtract(rel, Vector3Scale(caster.toward, Vector3DotProduct(rel, caster.toward)))) <=
                                        caster.extent * 1.4143f + cell.sphereRadius);
        if (!inView && !inReach) continue;
        if (!cell.built) buildPatch(*st, cell);
        cell.child->visible = true;
        g3d::Instance in;
        if (lo <= 1e-4f) in.fade = hi;
        else if (hi >= 1 - 1e-4f) in.fade = -(1 - lo);
        else { in.fade = hi; in.fadeFrom = lo; }
        if (f.canopyShade > 0) in.shade = f.canopyShade;
        else if (haveSun) {
            in.shade = canopyShade(toSun, Vector3Normalize(Vector3Subtract(eye, cell.centre)));
        }
        cell.child->instances->list.push_back(in);
    }
}

// The cells of a forest with canopy, each with its (empty until needed) patch object. Only the
// placements are looked at: no meshes are made here.
void setUpCanopy(ForestState& st, g3d::Object3D& group) {
    const ForestSpec& f = st.spec;
    if (st.at.empty()) return;
    st.cellSize = std::max({ f.canopyCell, canopyReach(st) * 1.05f + 1.f, 8.f });
    const float C = st.cellSize;
    float maxX = -1e30f, maxZ = -1e30f;
    st.minX = st.minZ = 1e30f;
    for (const Placement& p : st.at) {
        st.minX = std::min(st.minX, p.position.x); maxX = std::max(maxX, p.position.x);
        st.minZ = std::min(st.minZ, p.position.z); maxZ = std::max(maxZ, p.position.z);
    }
    st.cellsX = (int)std::floor((maxX - st.minX) / C) + 1;
    st.cellsZ = (int)std::floor((maxZ - st.minZ) / C) + 1;
    if ((double)st.cellsX * st.cellsZ > 4e6) throw std::invalid_argument("flora::addForest: canopyCell is too small for the forest");
    std::vector<std::vector<uint32_t>> members((size_t)st.cellsX * st.cellsZ);
    for (size_t i = 0; i < st.at.size(); i++) {
        const int cx = (int)std::floor((st.at[i].position.x - st.minX) / C), cz = (int)std::floor((st.at[i].position.z - st.minZ) / C);
        members[(size_t)cz * st.cellsX + cx].push_back((uint32_t)i);
    }
    // The patches' material: the first tree's leaves, cut out by the vertex alpha, with no picture.
    g3d::Material m = st.trees[0]->leaves;
    m.dynamicMap.reset(); m.map.clear(); m.dynamicNormalMap.reset(); m.normalMap.clear();
    m.color = WHITE;
    m.alphaCutoff = 0.5f; m.doubleSided = true;
    m.roughness = 1.f; m.translucency = 0.15f;
    m.dynamicMap = canopyTexture();
    m.castShadow = f.canopyShadows;
    m.receiveShadow = false;                                 // the relief shades itself in its colours; its own shadow map would speckle it
    st.cellAt.assign(members.size(), -1);
    for (int cz = 0; cz < st.cellsZ; cz++)
        for (int cx = 0; cx < st.cellsX; cx++) {
            std::vector<uint32_t>& ids = members[(size_t)cz * st.cellsX + cx];
            if (ids.empty()) continue;
            CanopyCell cell;
            cell.ix = cx; cell.iz = cz;
            cell.trees = std::move(ids);
            Vector3 sum{ 0, 0, 0 };
            float lo = 1e30f, hi = -1e30f;
            for (uint32_t i : cell.trees) {
                const Placement& p = st.at[i];
                const Tree& tr = *st.trees[(size_t)std::max(p.variant, 0) % st.trees.size()];
                sum = Vector3Add(sum, { p.position.x, p.position.y + tr.top * p.scale * 0.5f, p.position.z });
                lo = std::min(lo, p.position.y); hi = std::max(hi, p.position.y + tr.top * p.scale);
            }
            cell.centre = Vector3Scale(sum, 1.f / (float)cell.trees.size());
            cell.sphere = { st.minX + (cx + 0.5f) * C, 0.5f * (lo + hi), st.minZ + (cz + 0.5f) * C };
            cell.sphereRadius = std::hypot(C * 0.7072f, 0.5f * (hi - lo) + 1.f);
            float tall = 0;
            for (uint32_t i : cell.trees) tall += st.trees[(size_t)std::max(st.at[i].variant, 0) % st.trees.size()]->top * st.at[i].scale / (float)cell.trees.size();
            cell.height = tall;
            st.cellAt[(size_t)cz * st.cellsX + cx] = (int)st.cells.size();
            st.cells.push_back(std::move(cell));
        }
    // Only cells well inside the forest get a patch: it and its eight neighbours hold at least half
    // the median number of trees. The boundary of the forest and of its clearings keep their trees,
    // so from the side the edge is trees, not the edge of a height field.
    std::vector<size_t> counts;
    for (const CanopyCell& c : st.cells) counts.push_back(c.trees.size());
    std::sort(counts.begin(), counts.end());
    const size_t enough = (counts[counts.size() / 2] + 3) / 4;
    auto wooded = [&](int cx, int cz) {
        if (cx < 0 || cz < 0 || cx >= st.cellsX || cz >= st.cellsZ) return false;
        const int k = st.cellAt[(size_t)cz * st.cellsX + cx];
        return k >= 0 && st.cells[(size_t)k].trees.size() >= enough;
    };
    for (CanopyCell& cell : st.cells) {
        bool all = true;
        for (int dz = -1; dz <= 1; dz++)
            for (int dx = -1; dx <= 1; dx++) all = all && wooded(cell.ix + dx, cell.iz + dz);
        if (!all) continue;
        cell.interior = true;
        cell.inst = std::make_shared<g3d::Instances>();
        cell.child = group.add(cell.inst, m);
        cell.child->visible = false;
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
    st->pitchSign = Vector3Transform({ 0, 1, 0 }, MatrixRotateXYZ({ 0.1f, 0, 0 })).z > 0 ? 1.f : -1.f;
    for (const TreePtr& tr : trees) {
        st->first.push_back(group->children.size());
        auto add = [&](const std::shared_ptr<g3d::AnimatedMesh>& mesh, const g3d::Material& m) {
            auto in = std::make_shared<g3d::Instances>();
            in->mesh = mesh && mesh->frames > 0 ? mesh : tr->views.empty() ? tr->hero.wood : tr->views[0];
            g3d::Object3D* o = group->add(in, m);
            if (!mesh || mesh->frames <= 0) o->visible = false;          // nothing at this level
        };
        add(tr->hero.wood, tr->wood);
        add(tr->hero.leaves, tr->leaves);
        add(tr->reduced.wood, tr->wood);
        add(tr->reduced.leaves, tr->leaves);
        g3d::Material imp = tr->impostor;
        imp.castShadow = false;                                  // what is seen casts nothing
        // With copies that cast, the picture would be shadowed by its own copy (they cross at the
        // pivot), so it receives no shadow; without them it is as impostors always were.
        if (spec.impostorShadows) imp.receiveShadow = false;
        for (const auto& v : tr->views) add(v, imp);
        if (spec.impostorShadows) {                              // the copies that face the light
            imp.castShadow = true;
            imp.shadowOnly = true;
            for (const auto& v : tr->views) add(v, imp);
        }
    }
    // Splat crowns: one object a tree, after every kind's instanced children.
    size_t crowns = 0;
    for (const Placement& p : at) crowns += st->trees[(size_t)std::max(p.variant, 0) % trees.size()]->canopy ? 1 : 0;
    if (crowns > 256) throw std::invalid_argument("flora::addForest: at most 256 splat trees");
    for (const Placement& p : at) {
        const Tree& tree = *st->trees[(size_t)std::max(p.variant, 0) % trees.size()];
        if (!tree.canopy) { st->canopies.push_back(nullptr); continue; }
        g3d::Object3D* holder = group->add();
        holder->position = p.position;
        holder->rotation = { 0, p.yaw, 0 };
        holder->scale = { p.scale, p.scale, p.scale };
        g3d::Object3D* tilt = holder->add();
        auto splats = std::make_shared<g3d::Splats>();
        splats->cloud = tree.canopy;
        splats->look.shDegree = 0;
        tilt->add(splats);
        st->canopies.push_back(tilt);
        st->splats = true;
    }
    if (spec.canopyFrom > 0 || spec.canopyBelow > 0) setUpCanopy(*st, *group);
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
    float yawSign = 1;
    struct Body { g3d::Object3D *holder, *tilt; g3d::GeometryPtr wood, leaves; size_t kind; std::shared_ptr<g3d::Splats> crown; };
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
    const g3d::Scene* weatherScene = g3d::drawingScene();
    const bool inWind = weatherScene && weatherScene->weather.enabled && !st->spec.ownWind;     // a copy, for this frame only
    const ForestSpec windy = inWind ? inWeather(st->spec, weatherScene->weather) : ForestSpec{};
    const ForestSpec& f = inWind ? windy : st->spec;
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
        if (b.crown) {                                     // a splat crown swells in as the leaves would
            const float k = std::clamp((u - 0.62f) / 0.3f, 0.f, 1.f), e = k * k * (3 - 2 * k);
            b.crown->look.scale = 0.15f + 0.85f * e;
            b.crown->look.opacity = e;
        }
        if (tree.build.style == TreeStyle::PaperCut) {     // a paper cut-out turns to the camera
            const Vector3 to = Vector3Subtract(g3d::drawingCamera().position, p.position);
            b.holder->rotation.y = st->yawSign * std::atan2(to.x, to.z) * RAD2DEG;
        }
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
    st->yawSign = Vector3Transform({ 0, 0, 1 }, MatrixRotateXYZ({ 0, 90 * DEG2RAD, 0 })).x > 0 ? 1.f : -1.f;
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
        if (tree.canopy) {
            b.crown = std::make_shared<g3d::Splats>();
            b.crown->cloud = tree.canopy;
            b.crown->look.shDegree = 0;
            b.tilt->add(b.crown);
        }
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

ForestSpec inWeather(const ForestSpec& spec, const g3d::Weather& w) {
    ForestSpec out = spec;
    if (!w.enabled || spec.ownWind) return out;
    out.windDirection = w.windVector();
    out.wind = w.treeWind();
    out.gust = w.treeGust();
    out.gustSpeed = w.wind.gustSpeed;
    out.gustScale = w.wind.gustScale;
    return out;
}

}  // namespace flora

namespace geo3d {

constexpr float kPictureGain = 1.03f;       // the picture tint and spread below darken a little
g3d::Object3D* forest(g3d::Object3D& parent, SurfacePtr s, const std::vector<geo::Geometry>& regions, const ForestCover& st) {
    if (!s) throw std::invalid_argument("geo3d::forest: no surface");
    std::vector<flora::TreePtr> trees = st.trees;
    trees.erase(std::remove(trees.begin(), trees.end(), nullptr), trees.end());
    if (trees.empty()) {
        flora::TreeBuild b;
        b.style = flora::TreeStyle::LowPoly;
        b.frames = 1;
        b.views = 8;                                  // the pictures far trees are drawn as, baked at the first draw
        b.viewSize = 40;
        b.elevations = 4;                             // 0, 30, 60 and 90 degrees: straight above shows the crown, not the trunk
        for (const char* n : { "oak", "birch", "pine" }) trees.push_back(flora::makeTree(flora::tree(n), b));
    }
    // Where: every region's trees, in longitude (x) and latitude (z).
    struct Tree { geo::LonLat at; float scale; int kind; float late; float rank; };
    auto list = std::make_shared<std::vector<Tree>>();
    const float spacing = std::max(st.spacing, 1e-3f);
    float west = 1e30f, east = -1e30f;
    for (size_t r = 0; r < regions.size(); r++) {
        if (regions[r].polygons.empty()) continue;
        const flora::Area area = flora::region(regions[r], [](geo::LonLat p) { return Vector2{ (float)p.lon, (float)p.lat }; });
        flora::ScatterSpec sc;
        sc.spacing = spacing;
        sc.scaleMin = 1 - st.heightVariation * 0.5f;
        sc.scaleMax = 1 + st.heightVariation * 0.5f;
        sc.variants = (int)trees.size();
        for (const flora::Placement& p : flora::scatter(area, sc, st.seed + (uint32_t)r * 7919u)) {
            const size_t kind = (size_t)p.variant % trees.size();
            list->push_back({ { p.position.x, p.position.z }, st.height * p.scale / std::max(trees[kind]->top, 0.01f), (int)kind, 0, 0 });
            west = std::min(west, p.position.x);
            east = std::max(east, p.position.x);
        }
    }
    if ((int)list->size() > std::max(st.limit, 1)) {               // thin evenly, keeping order
        std::vector<Tree> kept;
        const double step = (double)list->size() / std::max(st.limit, 1);
        for (double i = 0; i < (double)list->size() && (int)kept.size() < st.limit; i += step) kept.push_back((*list)[(size_t)i]);
        *list = std::move(kept);
    }
    for (size_t i = 0; i < list->size(); i++) {
        Tree& t = (*list)[i];
        t.late = east > west ? std::clamp(st.stagger, 0.f, 0.95f) * (float)(t.at.lon - west) / (east - west) : 0;
        t.rank = flora::hashUnit((uint32_t)i, st.seed + 41u);          // which trees stay when far ones are merged
    }

    // Instance::pitch and bank that turn the tree's +y to a unit direction. Which axis turns
    // first and the signs are raymath's (MatrixRotateXYZ), found by trying each on probes.
    struct Tilt {
        bool zFirst; float sa, sb;
        Vector2 of(Vector3 u) const {                  // (pitch, bank), radians
            if (zFirst) return { sa * std::atan2(u.z, u.y), sb * std::asin(std::clamp(u.x, -1.f, 1.f)) };
            return { sa * std::asin(std::clamp(u.z, -1.f, 1.f)), sb * std::atan2(u.x, u.y) };
        }
    };
    Tilt tilt{ false, 1, 1 };
    float best = -1e9f;
    for (bool zFirst : { false, true })
        for (float sa : { 1.f, -1.f })
            for (float sb : { 1.f, -1.f }) {
                const Tilt c{ zFirst, sa, sb };
                float score = 0;
                for (Vector3 probe : { Vector3{ 0.3f, 0.8f, 0.5f }, Vector3{ -0.6f, -0.5f, 0.62f }, Vector3{ 0.5f, -0.7f, -0.5f } }) {
                    probe = Vector3Normalize(probe);
                    const Vector2 ab = c.of(probe);
                    score += Vector3DotProduct(Vector3Transform({ 0, 1, 0 }, MatrixRotateXYZ({ ab.x, 0, ab.y })), probe);
                }
                if (score > best) { best = score; tilt = c; }
            }

    g3d::Object3D* group = parent.add();
    group->name = "forest";
    // Per tree kind: the full tree's two meshes, and the pictures that stand in for it (one child
    // each, at row * views + direction, like flora::addForest's).
    struct Kind {
        std::shared_ptr<g3d::Instances> wood, leaves;
        std::vector<std::shared_ptr<g3d::Instances>> pictures;
        int views = 1, rows = 1;
    };
    auto kinds = std::make_shared<std::vector<Kind>>(trees.size());
    for (size_t k = 0; k < trees.size(); k++) {
        Kind& kd = (*kinds)[k];
        for (int leaves = 0; leaves < 2; leaves++) {
            const auto& mesh = leaves ? trees[k]->hero.leaves : trees[k]->hero.wood;
            if (!mesh || mesh->frames <= 0) continue;
            auto in = std::make_shared<g3d::Instances>();
            in->mesh = mesh;
            group->add(in, leaves ? trees[k]->leaves : trees[k]->wood);
            (leaves ? kd.leaves : kd.wood) = in;
        }
        const flora::Tree& tree = *trees[k];
        kd.views = std::max(tree.build.views, 1);
        kd.rows = std::max<int>((int)tree.viewElevation.size(), 1);
        if (tree.views.empty() || (int)tree.views.size() != kd.views * kd.rows || tree.viewSide.size() != (size_t)kd.rows) continue;
        g3d::Material imp = tree.impostor;
        imp.castShadow = false;                                   // see ForestCover::impostorBelow
        for (const auto& v : tree.views) {
            auto in = std::make_shared<g3d::Instances>();
            in->mesh = v;
            group->add(in, imp);
            kd.pictures.push_back(in);
        }
    }
    const auto grow = st.grow;
    const float below = std::max(st.impostorBelow, 0.f), band = std::clamp(st.blend, 0.01f, 1.9f), drop = std::max(st.dropBelow, 0.f);
    const float merge = std::max(st.mergeBelow, 0.f), speckle = std::clamp(st.pictureSpeckle, 0.f, 0.9f);
    // Where every tree stands for the surface's morph and map: remade only when those change (the
    // surface's point, up and the tilt that follows are the costly part of placing 100,000 trees).
    struct Spot { Vector3 base, up; Vector2 ab; };
    struct Cache { float morph = 0; int version = -1; std::vector<Spot> at; };
    auto cache = std::make_shared<Cache>();
    group->up = [s, list, kinds, trees, grow, tilt, below, band, drop, merge, speckle, cache](int t, g3d::Object3D& self) {
        const float u = grow ? std::clamp(grow(t), 0.f, 1.f) : 1.f;
        for (auto& child : self.children) if (child->instances) child->instances->list.clear();
        if (cache->at.size() != list->size() || cache->version != s->version() || !(cache->morph == s->morph)) {
            cache->at.resize(list->size());
            for (size_t i = 0; i < list->size(); i++) {
                Spot& sp = cache->at[i];
                sp.up = s->up((*list)[i].at);
                sp.base = s->point((*list)[i].at, 0);
                sp.ab = tilt.of(sp.up);
            }
            cache->morph = s->morph;
            cache->version = s->version();
        }
        const g3d::Camera& cam = g3d::drawingCamera();
        const float screenH = g3d::drawingViewportHeight();          // 0 outside a draw: every tree stays a mesh
        const Vector3 eye = cam.position;
        const Vector3 along = Vector3Normalize(Vector3Subtract(cam.target, cam.position));   // orthographic: every tree sees the same direction
        const float focal = screenH <= 0 ? 0.f : cam.orthographic ? screenH / std::max(cam.orthoHeight, 1e-3f)
                                                                  : screenH / (2 * std::tan(std::clamp(cam.fov, 1.f, 170.f) * DEG2RAD * 0.5f));
        for (size_t index = 0; index < list->size(); index++) {
            const Tree& tr = (*list)[index];
            const float k = std::clamp((u - tr.late) / std::max(1 - tr.late, 1e-3f), 0.f, 1.f);
            if (k <= 0) continue;
            const float e = k * k * (3 - 2 * k);
            const Spot& spot = cache->at[index];
            const Vector3 up = spot.up, base = spot.base;
            const Vector2 ab = spot.ab;
            const flora::Tree& tree = *(trees)[(size_t)tr.kind];
            Kind& kd = (*kinds)[(size_t)tr.kind];
            // The grown tree's height on screen, in composition px (growing does not change its level).
            float px = 1e9f;
            if (focal > 0) {
                const float height = tree.top * tr.scale;
                const float dist = cam.orthographic ? 1.f : Vector3Distance(eye, Vector3Add(base, Vector3Scale(up, height * 0.5f)));
                px = height * focal / std::max(dist, 1e-4f);
            }
            if (px < drop) continue;
            // Merging: under mergeBelow px a fraction (px / mergeBelow)^2 of the trees stay, by rank,
            // each larger by the inverse of that root, so the area they cover is the same. A tree
            // fades in or out over a band of rank (the screen door), so none pops.
            const float grown = tr.scale * e;                         // before merging enlarges it
            float scale = grown, shown = 1;
            if (merge > 0 && px < merge) {
                const float x = px / merge;
                const float keep = x * x;                                // the share of trees that stay
                shown = std::clamp((1.3f * keep - tr.rank) / (0.3f * keep), 0.f, 1.f);   // the fade band is a share of it: not every kept tree half faded
                if (shown < 1.f / 64) continue;
                scale /= std::max(x, 1e-3f) * 1.07f;                   // 1.07 = the root of the 1.15 more trees the fade band keeps
            }
            float toFar = kd.pictures.empty() || below <= 0 ? 0.f : std::clamp((below * (1 + band * 0.5f) - px) / (below * band), 0.f, 1.f);
            float a = 1 - toFar;                                      // the mesh keeps [0, a) of the screen door, the picture [a, 1)
            if (a < 1.f / 64) a = 0;
            // A band [lo, hi) of the screen door: [0, hi), [lo, 1), or between (Instance::fadeFrom).
            auto put = [shown](const std::shared_ptr<g3d::Instances>& to, float lo, float hi, const g3d::Instance& i0) {
                hi = std::min(hi, shown);
                if (!to || hi - lo < 1.f / 64) return;
                g3d::Instance i = i0;
                if (lo <= 1e-4f) i.fade = hi;
                else if (hi >= 1 - 1e-4f) i.fade = -(1 - lo);
                else { i.fade = hi; i.fadeFrom = lo; }
                to->list.push_back(i);
            };
            if (a > 0) {
                g3d::Instance in;
                in.position = base;
                in.pitch = ab.x * RAD2DEG;
                in.bank = ab.y * RAD2DEG;
                in.scale = scale;
                put(kd.wood, 0, a, in);
                put(kd.leaves, 0, a, in);
            }
            if (a >= 1) continue;
            // The picture: its quad faces the viewer (fully, from above) and its top leans along the
            // surface's up, not the world's. The tree's own axes give which direction is which.
            const Matrix turned = MatrixRotateXYZ({ ab.x, 0, ab.y });
            const Vector3 right = Vector3Transform({ 1, 0, 0 }, turned), front = Vector3Transform({ 0, 0, 1 }, turned);
            // The quad is centred where the unenlarged crown's middle is: enlarging merged trees grows the quad, not
            // its height, so from above the trees overlap by their own heights and not by how far they are (a radial pattern).
            const Vector3 pivot = Vector3Add(base, Vector3Scale(up, tree.viewPivot * grown));
            Vector3 to = cam.orthographic ? Vector3Negate(along) : Vector3Subtract(eye, pivot);
            to = Vector3Normalize(to);
            // Straight down the surface's up has no direction round it, so every tree would turn its
            // picture by where it lies from the point below the camera (a swirl). Between 50 and 75 degrees
            // above the horizon, the direction the picture is taken from and its top turn to the camera's
            // own (the screen's up), the same for every tree. At 50 degrees or lower nothing changes.
            const float sinElev = std::clamp(Vector3DotProduct(to, up), -1.f, 1.f);
            const float elevDeg = std::asin(sinElev) * RAD2DEG;
            const float topDown = std::clamp((elevDeg - 50.f) / 25.f, 0.f, 1.f), s = topDown * topDown * (3 - 2 * topDown);
            Vector3 camUp = Vector3Subtract(cam.upVector, Vector3Scale(up, Vector3DotProduct(cam.upVector, up)));
            camUp = Vector3LengthSqr(camUp) < 1e-8f ? front : Vector3Normalize(camUp);
            Vector3 viewed = to;                                      // the direction the picture is chosen for
            if (s > 0) {
                Vector3 flat = Vector3Subtract(to, Vector3Scale(up, sinElev));
                flat = Vector3LengthSqr(flat) < 1e-10f ? Vector3Negate(camUp) : Vector3Normalize(flat);
                Vector3 h = Vector3Add(Vector3Scale(flat, 1 - s), Vector3Scale(Vector3Negate(camUp), s));
                h = Vector3LengthSqr(h) < 1e-8f ? Vector3Negate(camUp) : Vector3Normalize(h);
                viewed = Vector3Add(Vector3Scale(h, std::sqrt(std::max(0.f, 1 - sinElev * sinElev))), Vector3Scale(up, sinElev));
            }
            const flora::ViewPick pick = flora::pickView(tree, { Vector3DotProduct(viewed, right), Vector3DotProduct(viewed, up), Vector3DotProduct(viewed, front) }, { 1, 0 });
            const bool tilted = kd.rows > 1;
            Vector3 aim = to, lift = up;
            if (!tilted) {                                            // one row: turns about the surface's up only
                aim = Vector3Subtract(to, Vector3Scale(up, sinElev));
                if (Vector3LengthSqr(aim) < 1e-8f) aim = front;
            } else if (s > 0) {
                lift = Vector3Add(Vector3Scale(up, 1 - s), Vector3Scale(camUp, s));
                if (Vector3LengthSqr(lift) < 1e-8f) lift = camUp;
                // Near the zenith the quad lies flat (along the surface) instead of facing the camera. Facing
                // it tilts each quad by how far it is from the point below the camera, so the outer edge of
                // every tree is nearer than the middle and covers its outward neighbour: strokes along the
                // radius. Flat, trees overlap by their heights alone, as the crowns do. A crown seen from
                // above is the same picture either way (the foreshortening is under 3% within 15 degrees).
                const float flat0 = std::clamp((elevDeg - 55.f) / 20.f, 0.f, 1.f), lay = flat0 * flat0 * (3 - 2 * flat0);
                if (lay > 0) {
                    aim = Vector3Add(Vector3Scale(to, 1 - lay), Vector3Scale(up, lay));
                    aim = Vector3LengthSqr(aim) < 1e-8f ? up : Vector3Normalize(aim);
                }
            }
            Vector3 quadUp = Vector3Subtract(lift, Vector3Scale(Vector3Normalize(aim), Vector3DotProduct(lift, Vector3Normalize(aim))));
            quadUp = Vector3LengthSqr(quadUp) < 1e-8f ? up : Vector3Normalize(quadUp);
            // Seen from above, a faceted crown's picture is a little brighter than the crown (its facets
            // average out in the picture but not in the mesh): about 1.2 times at 85 degrees, measured as
            // the mean over twelve directions of the three default trees. Dimmed to match (shade acts on
            // gamma values, so 0.9 is about 0.83 in light).
            float pictureShade = 1;
            Color pictureTint = WHITE;
            if (tree.build.style == flora::TreeStyle::LowPoly) {
                // Fitted, not derived: a picture's brightness against the crown's, as seen over a map forest
                // (ring means of the map-forest test: RMTN_MAPFOREST_ONLY=1 RMTN_TOPDOWN_ONLY=1
                // ./raymotion_scene3d_tests, straight down and 60 degrees, 1.5 to 3.5 radii, default light) and
                // for one tree (the same test's "vs its picture" lines: 0, 30, 60, 85, 90 degrees). Shade by
                // camera elevation, 0 / 30 / 60 / 90 degrees; re-measure after any change to the bake's shading.
                // Where the two disagree the forest wins: at 60 degrees a forest of pictures is 5% over its
                // meshes while one tree's picture is 0.71-0.81 of its crown (a known limit, noted in the tests).
                constexpr float kShade[4] = { 0.85f, 1.0f, 0.82f, 0.96f };
                const float el = pick.elevation;
                auto smooth = [](float u) { u = std::clamp(u, 0.f, 1.f); return u * u * (3 - 2 * u); };
                const int k = el < 30.f ? 0 : el < 60.f ? 1 : 2;
                const float u = smooth((el - 30.f * k) / 30.f);
                pictureShade = kShade[k] + (kShade[k + 1] - kShade[k]) * u;
                // A picture is smoother and less yellow than the crown's lit facets; a per-tree spread of
                // brightness gives the forest back its speckle, and the tint its yellow-green.
                const float tone = flora::hashUnit((uint32_t)index, 97u);
                pictureShade *= kPictureGain * (1 - speckle + 2 * speckle * tone);
                pictureTint = { 250, 255, 190, 255 };
            }
            auto standIn = [&](int row) {
                g3d::Instance im;
                im.scale = scale;
                im.shade = pictureShade;
                im.tint = pictureTint;
                im.aim = aim;
                im.aimUp = lift;
                im.position = tilted ? Vector3Subtract(pivot, Vector3Scale(quadUp, 0.5f * tree.viewSide[(size_t)row] * scale)) : base;
                return im;
            };
            auto picture = [&](int row, int v) { return kd.pictures[(size_t)(row * kd.views + v)]; };
            // The nearest picture alone. (flora::addForest blends the four nearest by screen door; on a
            // map the trees are dense and seen from above, and the cut-out holes where two pictures
            // differ show the quads under them as dark spokes, so a tree takes one picture, and pops
            // from one to the next as the view turns, small as it is.)
            const int row = pick.e < 0.5f ? pick.r0 : pick.r1;
            put(picture(row, pick.w < 0.5f ? pick.v0 : pick.v1), a, 1, standIn(row));
        }
    };
    return group;
}

}  // namespace geo3d
}  // namespace rmtn
