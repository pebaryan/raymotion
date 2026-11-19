// flora2d.cpp -- 2D vegetation: blades, tufts, meadows, shrubs and branching trees from code,
// pure in t (wind) and u (growth). See rmtn/flora2d.hpp.
#include "rmtn/flora2d.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace rmtn {
namespace flora2d {
namespace {

constexpr float TAU2 = 6.28318531f;

uint64_t mix64(uint64_t z) {
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
// Deterministic 0..1 for (seed, index, channel).
float unit(uint32_t seed, uint64_t i, uint32_t k) {
    return (float)(mix64(mix64(((uint64_t)seed << 32) ^ k) ^ i) >> 40) / (float)(1ull << 24);
}
float smooth(float e) { e = std::clamp(e, 0.f, 1.f); return e * e * (3 - 2 * e); }
Color scale(Color c, float k) {
    return { (unsigned char)std::clamp(c.r * k, 0.f, 255.f), (unsigned char)std::clamp(c.g * k, 0.f, 255.f),
             (unsigned char)std::clamp(c.b * k, 0.f, 255.f), c.a };
}
Color lerp(Color a, Color b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    return { (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t), (unsigned char)(a.b + (b.b - a.b) * t),
             (unsigned char)(a.a + (b.a - a.a) * t) };
}
Color faded(const Ctx& c, Color k) { k.a = (unsigned char)std::clamp(k.a * c.a, 0.f, 255.f); return k; }

// One triangle in node space, any winding, a colour per corner.
void tri(const Ctx& c, Vector2 a, Vector2 b, Vector2 d, Color ka, Color kb, Color kd) {
    Vector2 A = mapP(c, a), B = mapP(c, b), D = mapP(c, d);
    const float cross = (B.x - A.x) * (D.y - A.y) - (B.y - A.y) * (D.x - A.x);
    if (std::fabs(cross) < 1e-9f) return;
    if (cross < 0) { std::swap(B, D); std::swap(kb, kd); }
    DrawTriangleGradient(A, B, D, faded(c, ka), faded(c, kb), faded(c, kd));
}
// A filled ellipse (node space).
void ellipse(const Ctx& c, Vector2 at, float rx, float ry, Color k, int sides = 12) {
    for (int i = 0; i < sides; i++) {
        const float a0 = TAU2 * i / sides, a1 = TAU2 * (i + 1) / sides;
        tri(c, at, { at.x + rx * std::cos(a0), at.y + ry * std::sin(a0) }, { at.x + rx * std::cos(a1), at.y + ry * std::sin(a1) }, k, k, k);
    }
}
// A tapered limb from a to b, widths wa and wb.
void limb(const Ctx& c, Vector2 a, Vector2 b, float wa, float wb, Color k) {
    Vector2 d{ b.x - a.x, b.y - a.y };
    const float len = std::max(std::hypot(d.x, d.y), 1e-5f);
    const Vector2 n{ -d.y / len, d.x / len };
    const Vector2 a0{ a.x + n.x * wa * 0.5f, a.y + n.y * wa * 0.5f }, a1{ a.x - n.x * wa * 0.5f, a.y - n.y * wa * 0.5f };
    const Vector2 b0{ b.x + n.x * wb * 0.5f, b.y + n.y * wb * 0.5f }, b1{ b.x - n.x * wb * 0.5f, b.y - n.y * wb * 0.5f };
    tri(c, a0, b0, b1, k, k, k);
    tri(c, a0, b1, a1, k, k, k);
    if (wb > 1.5f) ellipse(c, b, wb * 0.5f, wb * 0.5f, k, 8);       // a round joint
}

// One blade: root, height, root width, the lean plus bend at the tip (radians).
void blade(const Ctx& c, Vector2 root, float h, float w, float lean, float bend, Color kr, Color kt, int segs) {
    if (h < 0.3f) return;
    segs = std::clamp(segs, 1, 8);
    Vector2 p = root, prevL{}, prevR{};
    Color prevK = kr;
    for (int k = 0; k <= segs; k++) {
        const float v = (float)k / segs;
        if (k > 0) {
            const float a = lean + bend * (v - 0.5f / segs) * (v - 0.5f / segs) * 1.6f;
            p = { p.x + h / segs * std::sin(a), p.y - h / segs * std::cos(a) };
        }
        const float a = lean + bend * v * v * 1.6f;
        const float half = 0.5f * w * std::pow(1 - v, 0.9f);
        const Vector2 n{ std::cos(a), std::sin(a) };
        const Vector2 L{ p.x - n.x * half, p.y - n.y * half }, R{ p.x + n.x * half, p.y + n.y * half };
        const Color kv = lerp(kr, kt, v);
        if (k > 0) {
            tri(c, prevL, prevR, R, prevK, prevK, kv);
            if (k < segs) tri(c, prevL, R, L, prevK, kv, kv);
        }
        prevL = L; prevR = R; prevK = kv;
    }
}

struct BladeAt { Vector2 root; float h, w, lean, phase, cr, f; uint64_t i; };

void drawBlades(const Ctx& c, const std::vector<BladeAt>& bs, const Grass& g, int t, float u, uint32_t seed, const Wind& w) {
    const float soft = 0.15f;
    for (const BladeAt& b : bs) {
        const float grown = u >= 1 ? 1.f : smooth((u * (1 + soft) - b.f) / soft);
        if (grown <= 0) continue;
        const float bend = bendAt(w, b.root.x, t, b.phase) * g.flex * (0.6f + 0.4f * unit(seed, b.i, 9));
        const Color kr = scale(g.root, b.cr), kt = scale(g.tip, b.cr * (0.9f + 0.2f * unit(seed, b.i, 8)));
        blade(c, b.root, b.h * grown, g.width * b.w * (0.4f + 0.6f * grown), b.lean, bend * grown, kr, kt, g.segments);
    }
}

BladeAt bladeOf(const Grass& g, uint32_t seed, uint64_t i, Vector2 root, float f, float size = 1) {
    BladeAt b;
    b.root = root;
    b.i = i;
    b.f = std::clamp(f, 0.f, 1.f);
    b.h = g.height * size * (1 - g.heightVariation * unit(seed, i, 1));
    b.w = size * (0.75f + 0.5f * unit(seed, i, 2));
    b.lean = g.lean * (unit(seed, i, 3) * 2 - 1);
    b.phase = unit(seed, i, 4);
    b.cr = 1 + g.colorVariation * (unit(seed, i, 5) * 2 - 1);
    return b;
}

bool inside(const std::vector<Vector2>& poly, Vector2 p) {
    bool in = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
        if ((poly[i].y > p.y) != (poly[j].y > p.y) &&
            p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)
            in = !in;
    return in;
}

}  // namespace

float bendAt(const Wind& w, float x, int t, float phase) {
    if (w.strength == 0) return 0;
    const float s = t / std::max(w.fps, 1e-3f), dir = w.direction < 0 ? -1.f : 1.f;
    const float lean = 0.12f;
    const float sway = w.sway > 0 ? 0.14f * std::sin(TAU2 * (w.sway * s + phase) + x * 0.004f) : 0.f;   // 0: no sway
    const float xp = x - dir * w.gustSpeed * s, period = std::max(w.gustWidth, 1.f) * 3;
    const float gust = std::pow(0.5f + 0.5f * std::cos(TAU2 * xp / period), 8.f) * w.gust;
    const float flutter = 0.05f * gust * std::sin(TAU2 * (3 * s + phase));
    return w.strength * (dir * (lean + 0.55f * gust) + sway + flutter);
}

Grass lawn() { Grass g; g.density = 1.6f; g.height = 18; g.heightVariation = 0.2f; g.width = 2.4f; g.lean = 0.1f; g.flex = 0.5f; return g; }
Grass meadow() { return Grass{}; }
Grass tallGrass() {
    Grass g; g.density = 0.7f; g.height = 90; g.heightVariation = 0.5f; g.width = 3.6f; g.lean = 0.3f; g.flex = 1.4f; g.segments = 6;
    g.root = { 46, 78, 38, 255 }; g.tip = { 168, 190, 100, 255 };
    return g;
}
Grass dryGrass() {
    Grass g; g.height = 52; g.root = { 110, 96, 54, 255 }; g.tip = { 222, 198, 128, 255 }; g.colorVariation = 0.2f;
    return g;
}
Grass grass(const char* name) {
    const std::string n = name ? name : "";
    if (n == "lawn") return lawn();
    if (n == "meadow") return meadow();
    if (n == "tall") return tallGrass();
    if (n == "dry") return dryGrass();
    throw std::invalid_argument("flora2d: no grass \"" + n + "\" (lawn, meadow, tall, dry)");
}

void grass(const Ctx& c, const std::vector<Vector2>& ground, const Grass& g, int t, float u, uint32_t seed, const Wind& w,
           const Arrival& arrival) {
    if (ground.size() < 2 || u <= 0 || g.density <= 0) return;
    float total = 0;
    for (size_t i = 1; i < ground.size(); i++) total += Vector2Distance(ground[i - 1], ground[i]);
    const int n = std::min((int)(total * g.density), 200000);
    std::vector<BladeAt> bs;
    bs.reserve((size_t)n);
    size_t seg = 1;
    float before = 0;
    for (int i = 0; i < n; i++) {
        const float at = (i + unit(seed, (uint64_t)i, 0)) / g.density;
        while (seg + 1 < ground.size() && before + Vector2Distance(ground[seg - 1], ground[seg]) < at) {
            before += Vector2Distance(ground[seg - 1], ground[seg]);
            seg++;
        }
        const float len = std::max(Vector2Distance(ground[seg - 1], ground[seg]), 1e-5f);
        const Vector2 p = Vector2Lerp(ground[seg - 1], ground[seg], std::clamp((at - before) / len, 0.f, 1.f));
        bs.push_back(bladeOf(g, seed, (uint64_t)i, { p.x, p.y + 1 }, arrival ? arrival(p) : at / std::max(total, 1e-3f)));
    }
    drawBlades(c, bs, g, t, u, seed, w);
}

void tuft(const Ctx& c, Vector2 at, const Grass& g, int count, int t, float u, uint32_t seed, const Wind& w) {
    if (u <= 0) return;
    Grass fan = g;
    std::vector<BladeAt> bs;
    for (int i = 0; i < std::clamp(count, 1, 400); i++) {
        BladeAt b = bladeOf(fan, seed, (uint64_t)i, { at.x + (unit(seed, (uint64_t)i, 6) - 0.5f) * g.width * 3, at.y }, 0);
        b.lean = ((i + 0.5f) / count - 0.5f) * 1.3f + 0.15f * (unit(seed, (uint64_t)i, 3) - 0.5f);
        b.h *= 1 - 0.45f * std::fabs(b.lean);
        b.f = unit(seed, (uint64_t)i, 7) * 0.5f;
        bs.push_back(b);
    }
    drawBlades(c, bs, g, t, u, seed, w);
}

void meadow(const Ctx& c, const std::vector<Vector2>& shape, const Grass& g, int t, float u, uint32_t seed, const Wind& w,
            float perspective, const Arrival& arrival) {
    if (shape.size() < 3 || u <= 0 || g.density <= 0) return;
    float x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f;
    for (const Vector2& p : shape) { x0 = std::min(x0, p.x); x1 = std::max(x1, p.x); y0 = std::min(y0, p.y); y1 = std::max(y1, p.y); }
    perspective = std::clamp(perspective, 0.f, 0.95f);
    std::vector<BladeAt> bs;
    uint64_t i = 0;
    // Rows back to front, each about a fifth of a blade high apart (scaled with distance).
    for (float y = y0; y <= y1 && bs.size() < 200000;) {
        const float depth = y1 > y0 ? (y - y0) / (y1 - y0) : 1;           // 0 back .. 1 front
        const float size = 1 - perspective * (1 - depth);
        const float gap = 1 / (g.density * size);
        for (float x = x0 + unit(seed, i, 10) * gap; x <= x1; x += gap) {
            const Vector2 p{ x + (unit(seed, i, 11) - 0.5f) * gap, y + (unit(seed, i, 12) - 0.5f) * g.height * size * 0.12f };
            if (inside(shape, p)) bs.push_back(bladeOf(g, seed, i, p, arrival ? arrival(p) : (x1 > x0 ? (p.x - x0) / (x1 - x0) : 0), size));
            i++;
        }
        y += std::max(g.height * size * 0.14f, 1.f);
    }
    drawBlades(c, bs, g, t, u, seed, w);
}

void shrub(const Ctx& c, Vector2 base, const Shrub& s, int t, float u, uint32_t seed, const Wind& w) {
    if (u <= 0) return;
    const float bend = bendAt(w, base.x, t, unit(seed, 0, 1)) * s.flex;
    const float wood = smooth(u / 0.5f);
    auto sway = [&](Vector2 p) {                                    // the higher, the more it moves
        const float k = std::clamp((base.y - p.y) / std::max(s.height, 1.f), 0.f, 1.5f);
        return Vector2{ p.x + bend * s.height * 0.25f * k * k, p.y };
    };
    for (int i = 0; i < std::clamp(s.stems, 1, 16); i++) {
        const float a = ((i + 0.5f) / s.stems - 0.5f) * 1.2f + 0.2f * (unit(seed, (uint64_t)i, 2) - 0.5f);
        const float len = s.height * (0.6f + 0.3f * unit(seed, (uint64_t)i, 3)) * wood;
        const Vector2 tip = sway({ base.x + std::sin(a) * len, base.y - std::cos(a) * len });
        limb(c, base, tip, std::max(1.5f, s.size * 0.05f) * (0.4f + 0.6f * wood), 1, s.stem);
    }
    // Leaf blobs on a dome, back (dark) to front (light), each swelling in from u 0.4.
    const int n = std::clamp(s.leaves, 1, 400);
    for (int layer = 0; layer < 3; layer++) {
        const Color k = layer == 0 ? s.dark : layer == 1 ? s.mid : s.light;
        for (int i = 0; i < n; i++) {
            if ((int)(unit(seed, (uint64_t)i, 4) * 3) != layer) continue;
            const float ang = unit(seed, (uint64_t)i, 5) * 3.14159f, rr = std::sqrt(unit(seed, (uint64_t)i, 6));
            const Vector2 p{ base.x + std::cos(ang) * rr * s.size * 0.5f, base.y - s.height * 0.35f - std::sin(ang) * rr * s.height * 0.6f -
                                                                               layer * s.height * 0.06f };
            const float grown = smooth((u - 0.4f - 0.3f * unit(seed, (uint64_t)i, 7)) / 0.3f);
            if (grown <= 0) continue;
            const float r = s.size * s.leafSize * (0.7f + 0.6f * unit(seed, (uint64_t)i, 8)) * grown * (layer == 2 ? 0.75f : 1);
            ellipse(c, sway(p), r, r * 0.85f, k);
        }
    }
}

Tree oak() { return Tree{}; }
Tree birch() {
    Tree s; s.height = 360; s.trunkWidth = 13; s.spread = 0.42f; s.branchLength = 0.7f; s.gnarl = 0.15f;
    s.bark = { 222, 218, 206, 255 }; s.dark = { 70, 110, 48, 255 }; s.mid = { 110, 150, 66, 255 }; s.light = { 168, 196, 96, 255 };
    s.leafSize = 21; s.leafDensity = 1.6f;
    return s;
}
Tree pine() {
    Tree s; s.form = TreeForm::Conifer; s.height = 380; s.trunkWidth = 16; s.bark = { 84, 58, 42, 255 };
    s.dark = { 24, 52, 40, 255 }; s.mid = { 36, 76, 52, 255 }; s.light = { 62, 104, 66, 255 };
    return s;
}
Tree tree(const char* name) {
    const std::string n = name ? name : "";
    if (n == "oak") return oak();
    if (n == "birch") return birch();
    if (n == "pine") return pine();
    throw std::invalid_argument("flora2d: no tree \"" + n + "\" (oak, birch, pine)");
}

namespace {
// A branch of the skeleton, in its parent's frame: angle from the parent's direction and length.
struct Branch { int parent; float angle, length, width0, width1, from, to; int level; bool tip; uint32_t id; };

std::vector<Branch> skeleton(const Tree& s, uint32_t seed) {
    std::vector<Branch> out;
    const float H = std::max(s.height, 4.f);
    if (s.form == TreeForm::Conifer) {
        // A straight stem in pieces, side branches at each joint, shorter up the stem.
        const int pieces = 14;
        int prev = -1;
        float dist = 0;
        for (int i = 0; i < pieces; i++) {
            const float v = (float)i / pieces, len = H / pieces;
            Branch b{ prev, 0.04f * s.gnarl * (unit(seed, (uint64_t)i, 1) - 0.5f), len, s.trunkWidth * (1 - v), s.trunkWidth * (1 - v - 1.f / pieces),
                      dist, dist + len, 0, i == pieces - 1, (uint32_t)i };
            dist += len;
            out.push_back(b);
            const int stem = (int)out.size() - 1;
            if (v < 0.22f) { prev = stem; continue; }
            for (int side = -1; side <= 1; side += 2) {
                const float reach = H * 0.36f * std::pow(1 - v, 0.9f) * (0.8f + 0.4f * unit(seed, (uint64_t)i * 2 + side + 7, 2));
                out.push_back({ stem, side * (1.25f + 0.2f * unit(seed, (uint64_t)i, 3 + side + 1)), reach, std::max(1.f, s.trunkWidth * 0.25f * (1 - v)), 1,
                                dist, dist + reach, 1, true, (uint32_t)(1000 + i * 2 + (side > 0)) });
            }
            prev = stem;
        }
        return out;
    }
    // Broadleaf: a trunk, then each branch splits in two or three, down to `depth` levels.
    const float trunk = H * 0.36f;
    out.push_back({ -1, 0.06f * s.gnarl * (unit(seed, 0, 1) - 0.5f), trunk, s.trunkWidth, s.trunkWidth * 0.72f, 0, trunk, 0, false, 0 });
    std::vector<int> open{ 0 };
    const int depth = std::clamp(s.depth, 1, 9);
    uint32_t id = 1;
    for (int level = 1; level <= depth && out.size() < 4000; level++) {
        std::vector<int> next;
        for (int pi : open) {
            const Branch p = out[(size_t)pi];
            const int kids = 2 + (unit(seed, id, 9) < 0.3f ? 1 : 0);
            for (int k = 0; k < kids; k++, id++) {
                const float side = kids == 2 ? (k ? 1.f : -1.f) : (float)(k - 1);
                const float a = side * s.spread * (0.7f + 0.6f * unit(seed, id, 2)) + s.gnarl * 0.5f * (unit(seed, id, 3) - 0.5f);
                const float len = p.length * s.branchLength * (0.8f + 0.4f * unit(seed, id, 4)) * (level == 1 ? 1.1f : 1);
                const float w0 = p.width1 * (kids == 2 ? 0.78f : 0.66f);
                Branch b{ pi, a, len, w0, w0 * 0.7f, p.to, p.to + len, level, level >= depth - 2, id };
                out.push_back(b);
                next.push_back((int)out.size() - 1);
            }
        }
        open = next;
    }
    // Scale the lengths so the crown's top, at rest, is the tree's height.
    std::vector<float> dir(out.size(), 0.f), y(out.size(), 0.f);
    float top = 0;
    for (size_t i = 0; i < out.size(); i++) {
        const Branch& b = out[i];
        dir[i] = (b.parent >= 0 ? dir[(size_t)b.parent] * 0.8f : 0) + b.angle;
        y[i] = (b.parent >= 0 ? y[(size_t)b.parent] : 0) + std::cos(dir[i]) * b.length;
        top = std::max(top, y[i]);
    }
    const float k = top > 1e-3f ? H * 0.88f / top : 1;              // leaves make up the rest
    for (Branch& b : out) { b.length *= k; b.from *= k; b.to *= k; }
    return out;
}
}  // namespace

void tree(const Ctx& c, Vector2 base, const Tree& s, int t, float u, uint32_t seed, const Wind& w) {
    if (u <= 0) return;
    const std::vector<Branch> sk = skeleton(s, seed);
    float longest = 0;
    for (const Branch& b : sk) longest = std::max(longest, b.to);
    const float front = std::min(u / 0.7f, 1.f) * longest * 1.0001f;
    const float bend = bendAt(w, base.x, t, unit(seed, 0, 5)) * s.flex;
    // Positions this frame: each branch turns from its parent's direction, plus the wind
    // (more on thin, outer branches), and grows out as the front passes it.
    struct Pose { Vector2 a, b; float dir, grown; };
    std::vector<Pose> pose(sk.size());
    const Color bark = s.silhouette ? s.dark : s.bark;
    for (size_t i = 0; i < sk.size(); i++) {
        const Branch& b = sk[i];
        const Pose* p = b.parent >= 0 ? &pose[(size_t)b.parent] : nullptr;
        const float wind = bend * 0.06f * (b.level + 1) * (s.form == TreeForm::Conifer && b.level == 0 ? 0.4f : 1.f)
                         + bendAt(w, base.x, t, unit(seed, b.id, 6)) * 0.03f * b.level * s.flex;
        // Broadleaf branches turn back toward the light: a child keeps 80% of its parent's lean.
        const float keep = s.form == TreeForm::Broadleaf ? 0.8f : 1.f;
        const float dir = (p ? p->dir * keep : 0) + b.angle + wind;
        const float grown = std::clamp((front - b.from) / std::max(b.length, 1e-3f), 0.f, 1.f);
        const Vector2 a = p ? Vector2Lerp(p->a, p->b, 1) : base;
        const float len = b.length * grown;
        pose[i] = { a, { a.x + std::sin(dir) * len, a.y - std::cos(dir) * len }, dir, grown };
    }
    // Wood: thicker as the wood beyond it grows (the 3D pipe model's spirit, simply).
    const float thick = 0.25f + 0.75f * smooth(u / 0.7f);
    for (size_t i = 0; i < sk.size(); i++) {
        const Branch& b = sk[i];
        if (pose[i].grown <= 0) continue;
        limb(c, pose[i].a, pose[i].b, std::max(b.width0 * thick, 1.f), std::max(b.width1 * thick * pose[i].grown, 0.8f), bark);
    }
    // Leaves after 0.6, by rank: back clumps dark, then mid, then light toward the top-left sun.
    float top = base.y, mid = base.x;
    for (const Pose& p : pose) top = std::min(top, p.b.y);
    for (int layer = 0; layer < 3; layer++) {
        const Color k = s.silhouette ? s.dark : layer == 0 ? s.dark : layer == 1 ? s.mid : s.light;
        for (size_t i = 0; i < sk.size(); i++) {
            const Branch& b = sk[i];
            if (!b.tip) continue;
            const bool last = s.form == TreeForm::Conifer || b.level >= std::clamp(s.depth, 1, 9);
            const int inner = std::clamp(s.depth, 1, 9) - 2;             // the level below the last two: a few clumps
            const int clumps = std::max(1, (int)std::round(s.leafDensity * (s.form == TreeForm::Conifer ? 3 : last ? 2 : b.level == inner ? 1 : 3)));
            for (int q = 0; q < clumps; q++) {
                const uint64_t key = (uint64_t)b.id * 8 + (uint64_t)q;
                const float rank = unit(seed, key, 11);
                const float grown = u >= 1 ? 1.f : smooth((u - 0.6f - 0.26f * rank) / 0.12f) * pose[i].grown;
                if (grown <= 0) continue;
                const float along = 0.35f + 0.65f * unit(seed, key, 12);
                Vector2 at = Vector2Lerp(pose[i].a, pose[i].b, along);
                at.x += (unit(seed, key, 13) - 0.5f) * s.leafSize;
                at.y += (unit(seed, key, 14) - 0.5f) * s.leafSize * 0.8f;
                // Which tone: lit toward the top and the sun's side, shaded below.
                const float lit = 0.55f * (base.y - at.y) / std::max(base.y - top, 1.f) + 0.25f * (at.x < mid ? 1 : 0) + 0.3f * unit(seed, key, 15);
                const int tone = lit < 0.42f ? 0 : lit < 0.7f ? 1 : 2;
                if (tone != layer) continue;
                const float r = s.leafSize * (0.7f + 0.5f * unit(seed, key, 16)) * grown * (layer == 2 ? 0.8f : 1);
                if (s.form == TreeForm::Conifer) {                         // a drooping needle tier
                    const float half = std::max(std::fabs(pose[i].b.x - pose[i].a.x), r) * 0.6f;
                    tri(c, { at.x - half, at.y + r * 0.35f }, { at.x + half, at.y + r * 0.35f }, { at.x, at.y - r * 0.8f }, k, k, k);
                } else ellipse(c, at, r, r * 0.82f, k);
            }
        }
    }
}

std::vector<Vector2> groundLine(float x0, float x1, float y, float amplitude, float wavelength, uint32_t seed, int points) {
    points = std::clamp(points, 2, 4096);
    std::vector<Vector2> out;
    const float wl = std::max(wavelength, 1.f), p1 = unit(seed, 0, 1) * TAU2, p2 = unit(seed, 0, 2) * TAU2;
    for (int i = 0; i < points; i++) {
        const float x = x0 + (x1 - x0) * i / (points - 1);
        out.push_back({ x, y - amplitude * (0.65f * std::sin(TAU2 * x / wl + p1) + 0.35f * std::sin(TAU2 * x / (wl * 0.43f) + p2)) });
    }
    return out;
}

float heightAt(const std::vector<Vector2>& line, float x) {
    if (line.empty()) return 0;
    if (x <= line.front().x) return line.front().y;
    for (size_t i = 1; i < line.size(); i++)
        if (x <= line[i].x) {
            const float span = line[i].x - line[i - 1].x;
            return span > 1e-6f ? line[i - 1].y + (line[i].y - line[i - 1].y) * (x - line[i - 1].x) / span : line[i].y;
        }
    return line.back().y;
}

void ground(const Ctx& c, const std::vector<Vector2>& line, float bottom, Color k) {
    for (size_t i = 1; i < line.size(); i++) {
        const Vector2 a = line[i - 1], b = line[i];
        tri(c, a, b, { b.x, bottom }, k, k, k);
        tri(c, a, { b.x, bottom }, { a.x, bottom }, k, k, k);
    }
}

Color hazed(Color c, float depth, Color haze) { return lerp(c, { haze.r, haze.g, haze.b, c.a }, std::clamp(depth, 0.f, 1.f) * haze.a / 255.f); }

Ctx parallax(const Ctx& c, float depth, Vector2 camera) {
    Ctx out = c;
    const float k = 1 - std::clamp(depth, 0.f, 1.f);
    const Vector2 shift = mapP({ { 0, 0 }, c.s, c.rot, 1 }, { -camera.x * k, -camera.y * k });
    out.o = { c.o.x + shift.x, c.o.y + shift.y };
    return out;
}

Filter softness(float px) {
    // Tilt-shift with its sharp band above the picture: blurred everywhere, saturation kept.
    Filter f = Filter::tiltShift(-10, 0, std::max(px, 0.f), 0, 0);
    f.falloff = 0.001f;
    return f;
}

}  // namespace flora2d
}  // namespace rmtn
