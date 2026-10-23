// rmtn/timeline.hpp -- Remotion-style core: timeline tree, easing, keyframe tracks,
// composable transforms, deterministic frame-driven randomness. No state carries between frames.
#pragma once
#include <raylib.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include "synth.hpp"

namespace rmtn {

// NOTE: PI comes from raylib.h (`#define PI`) -- do not redeclare.

// ------------------------------------------------------------------ colors
struct ColorF { float r, g, b, a; };
inline Color mkColor(unsigned int hex, unsigned char alpha = 255) {
    return { (unsigned char)((hex >> 16) & 0xFF), (unsigned char)((hex >> 8) & 0xFF),
             (unsigned char)(hex & 0xFF), alpha };
}
inline Color withAlpha(Color c, float mul) {
    int a = (int)(c.a * mul + 0.5f); if (a > 255) a = 255; if (a < 0) a = 0;
    return { c.r, c.g, c.b, (unsigned char)a };
}
inline Color mixC(Color a, Color b, double u) {
    auto L = [u](int x, int y) { return (unsigned char)(x + (y - x) * u + 0.5); };
    return { L(a.r,b.r), L(a.g,b.g), L(a.b,b.b), L(a.a,b.a) };
}
inline Vector2 mixV(Vector2 a, Vector2 b, double u) {
    return { (float)(a.x + (b.x-a.x)*u), (float)(a.y + (b.y-a.y)*u) };
}

// ------------------------------------------------------------------ easing
enum EaseIdx {
    E_LINEAR, E_IN_QUAD, E_OUT_QUAD, E_IN_OUT_QUAD,
    E_IN_CUBIC, E_OUT_CUBIC, E_IN_OUT_CUBIC,
    E_IN_EXPO, E_OUT_EXPO, E_IN_OUT_EXPO,
    E_BACK, E_ELASTIC, E_BOUNCE, E_COUNT
};
static const char* EASE_NAMES[E_COUNT] = {
    "linear","in-quad","out-quad","in-out-quad",
    "in-cubic","out-cubic","in-out-cubic",
    "in-expo","out-expo","in-out-expo",
    "back","elastic","bounce" };
static inline int easeFromName(const std::string& n) {
    for (int i = 0; i < E_COUNT; i++) if (n == EASE_NAMES[i]) return i;
    return E_LINEAR;
}
static double clamp01(double t) { return t < 0 ? 0 : (t > 1 ? 1 : t); }
static double powIn(double t, double p)  { return std::pow(clamp01(t), p); }
static double powOut(double t, double p) { return 1.0 - std::pow(1.0 - clamp01(t), p); }
static double powInOut(double t, double p) {
    t = clamp01(t);
    return t < 0.5 ? std::pow(2*t, p)/2.0 : 1.0 - std::pow(2-2*t, p)/2.0;
}
static inline double easeFn(int e, double t) {
    t = clamp01(t);
    switch (e) {
        case E_IN_QUAD:      return t*t;
        case E_OUT_QUAD:     return t*(2-t);
        case E_IN_OUT_QUAD:  return powInOut(t,2);
        case E_IN_CUBIC:     return t*t*t;
        case E_OUT_CUBIC:    return powOut(t,3);
        case E_IN_OUT_CUBIC: return powInOut(t,3);
        case E_IN_EXPO:      return t <= 0 ? 0 : std::pow(2.0, 10*t - 10);
        case E_OUT_EXPO:     return t >= 1 ? 1 : 1 - std::pow(2.0, -10*t);
        case E_IN_OUT_EXPO:  return powInOut(t, 8);
        case E_BACK: {
            const double c1 = 1.70158, c3 = c1 + 1;
            return c3*t*t*t - c1*t;   // overshoots: do NOT clamp result
        }
        case E_ELASTIC: {
            if (t <= 0) return 0; if (t >= 1) return 1;
            const double c4 = (2*PI)/3;
            return std::pow(2.0,-10*t) * std::sin((10*t - 0.75)*c4) + 1;
        }
        case E_BOUNCE: {                              // standard ease-out bounce, stays in [0,1]
            const double n = 7.5625, d = 2.75;
            if (t < 1/d)   return n*t*t;
            if (t < 2/d)   { t -= 1.5/d;   return n*t*t + 0.75; }
            if (t < 2.5/d) { t -= 2.25/d;  return n*t*t + 0.9375; }
            t -= 2.625/d;  return n*t*t + 0.984375;
        }
        default: return t;
    }
}

// ------------------------------------------------------------------ deterministic rng
static inline unsigned long long h64(unsigned long long x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}
static inline double rnd01(unsigned long long seed, unsigned long long idx) {
    return (double)(h64(seed ^ h64(idx + 1)) >> 11) * (1.0 / 9007199254740992.0);
}
static inline double rrange(unsigned long long seed, unsigned long long idx, double a, double b) {
    return a + (b - a) * rnd01(seed, idx);
}
static inline Vector2 rvec2(unsigned long long seed, unsigned long long salt, unsigned long long idx) {
    return { (float)rnd01(seed, salt*1000003ULL + 11*idx + 1), (float)rnd01(seed, salt*999983ULL + 23*idx + 7) };
}

// ------------------------------------------------------------------ keyframe tracks (frames as x-axis)
struct TrackFloat {
    struct K { double t; float v; int ease; };
    std::vector<K> keys;
    void add(double t, float v, int ease = E_LINEAR) { keys.push_back({ t, v, ease }); }
    float at(double f) const {
        if (keys.empty()) return 0.f;
        if (f <= keys.front().t) return keys.front().v;
        for (size_t i = 1; i < keys.size(); i++) {
            if (f <= keys[i].t) {
                const K& a = keys[i-1], & b = keys[i];
                double u = (f - a.t) / (b.t - a.t);
                return (float)(a.v + (b.v - a.v) * easeFn(b.ease, u));
            }
        }
        return keys.back().v;
    }
};
struct TrackVec {
    struct K { double t; Vector2 v; int ease; };
    std::vector<K> keys;
    void add(double t, Vector2 v, int ease = E_LINEAR) { keys.push_back({ t, v, ease }); }
    Vector2 at(double f) const {
        if (keys.empty()) return { 0, 0 };
        if (f <= keys.front().t) return keys.front().v;
        for (size_t i = 1; i < keys.size(); i++) {
            if (f <= keys[i].t) {
                const K& a = keys[i-1], & b = keys[i];
                double u = (f - a.t) / (b.t - a.t);
                return mixV(a.v, b.v, easeFn(b.ease, u));
            }
        }
        return keys.back().v;
    }
};
struct TrackColor {
    struct K { double t; Color v; int ease; };
    std::vector<K> keys;
    void add(double t, Color v, int ease = E_LINEAR) { keys.push_back({ t, v, ease }); }
    Color at(double f) const {
        if (keys.empty()) return { 255,255,255,255 };
        if (f <= keys.front().t) return keys.front().v;
        for (size_t i = 1; i < keys.size(); i++) {
            if (f <= keys[i].t) {
                const K& a = keys[i-1], & b = keys[i];
                double u = (f - a.t) / (b.t - a.t);
                return mixC(a.v, b.v, easeFn(b.ease, u));
            }
        }
        return keys.back().v;
    }
};

// ------------------------------------------------------------------ node transform context
struct Ctx {
    Vector2 o{ 0, 0 };   // node origin in canvas pixels
    float s = 1.0f;      // accumulated uniform scale
    float rot = 0.0f;    // accumulated rotation, degrees
    float a = 1.0f;      // accumulated opacity
};
static inline Vector2 mapP(const Ctx& c, Vector2 p) {
    double th = (double)c.rot * (PI / 180.0);
    double co = std::cos(th), si = std::sin(th);
    double x = c.o.x + (double)c.s * ((double)p.x * co - (double)p.y * si);
    double y = c.o.y + (double)c.s * ((double)p.x * si + (double)p.y * co);
    return { (float)x, (float)y };
}

struct NodeProps {
    Vector2 pos{ 0, 0 };  // local offset from parent origin
    float scale = 1.0f;
    float rot = 0.0f;     // degrees
    float opacity = 1.0f;
};

// ------------------------------------------------------------------ timeline tree
struct TNode {
    int start = 0, dur = 60;                 // relative to parent's start, in frames
    NodeProps props;
    std::function<void(int t, NodeProps& p)> up;                    // animate props at local frame t
    std::function<void(const Ctx& c, int t)> fn;                    // draw leaves at local frame t
    std::vector<std::unique_ptr<TNode>> kids;

    TNode* add(int off, int duration) {
        kids.push_back(std::make_unique<TNode>());
        TNode* n = kids.back().get();
        n->start = off; n->dur = duration;
        return n;
    }
};

static void stepNode(TNode& n, int parentLocal, const Ctx& parentCtx) {
    int t = parentLocal - n.start;
    if (t < 0 || t >= n.dur) return;
    if (n.up) n.up(t, n.props);
    Ctx c;
    c.o = mapP(parentCtx, n.props.pos);
    c.s = parentCtx.s * n.props.scale;
    c.rot = parentCtx.rot + n.props.rot;
    c.a = parentCtx.a * n.props.opacity;
    if (n.fn) n.fn(c, t);
    for (auto& k : n.kids) stepNode(*k, t, c);
}
static void drawTree(const std::vector<std::unique_ptr<TNode>>& roots, int frame) {
    Ctx root{ { 0, 0 }, 1.f, 0.f, 1.f };
    for (auto& r : roots) stepNode(*r, frame, root);
}

// composition
struct Comp {
    std::string name;
    int w = 1920, h = 1080, fps = 30;
    bool music = false;
    int fx = 0;   // FX tier used by fx_present.cpp (0 = legacy path)
    int duration = 0;                        // total frames
    Song song;                               // procedural soundtrack (used when music=true)
    std::vector<std::unique_ptr<TNode>> roots;
};

// sugar: append a root node [off, off+dur) to a composition and return it
static inline TNode* addRoot(std::vector<std::unique_ptr<TNode>>& roots, int off, int dur) {
    roots.push_back(std::make_unique<TNode>());
    TNode* n = roots.back().get();
    n->start = off; n->dur = dur;
    return n;
}

// registry implemented in scenes.cpp
void register_scenes(std::vector<Comp>& out);

} // namespace rmtn
