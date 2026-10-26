// rmtn/timeline.hpp -- Remotion-style core: timeline tree, easing, keyframe tracks,
// composable transforms, deterministic frame-driven randomness. No state carries between frames.
#pragma once
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include "json.hpp"
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

// ------------------------------------------------------------------ layers
enum class Blend { Normal, Add, Multiply, Screen };
enum class MaskMode { Alpha, Luma };

// An image filter on a layer's flattened content (illustration looks; see Layer::filters).
// Sizes are composition px, so a look holds at any supersampling. Patterns are fixed to
// the canvas and the same every frame.
struct Filter {
    enum class Type {
        Posterize,    // `levels` flat brightness steps; hue and saturation are kept
        Halftone,     // printed dots: `size` cell, `angle` screen; mono ink or CMYK (`colored`)
        Paper,        // paper grain and fibres darken the content by `strength` (`size` = grain scale)
        InkEdges,     // dark `ink` lines `size` wide where colour or alpha changes by > `threshold`
        Watercolor,   // bleeding edges (`size` px), edge darkening (`strength`), pigment granulation
        TiltShift,    // a sharp band (`position`, `band`, `angle`), blur up to `size` px beyond it
                      // (reached `falloff` further out), saturation raised by `strength`: a miniature
    };
    Type type = Type::Posterize;
    int levels = 4;
    float size = 8;
    float angle = 45;
    bool colored = false;
    float threshold = 0.12f;
    float strength = 0.5f;
    float granulation = 0.5f;
    Color ink{ 24, 22, 30, 255 };
    Color paper{ 250, 246, 236, 255 };    // halftone: colour between the dots
    float position = 0.5f;                // tilt-shift: the sharp line, a fraction of the layer's height from the top
    float band = 0.08f;                   // tilt-shift: half-height of the sharp band (fraction of the height)
    float falloff = 0.3f;                 // tilt-shift: how much further out the blur is full

    static Filter posterize(int levels = 4) { Filter f; f.type = Type::Posterize; f.levels = levels; return f; }
    static Filter halftone(float cell = 8, float angle = 45, bool colored = false) {
        Filter f; f.type = Type::Halftone; f.size = cell; f.angle = angle; f.colored = colored; return f;
    }
    static Filter paperGrain(float strength = 0.35f, float scale = 1) {
        Filter f; f.type = Type::Paper; f.strength = strength; f.size = scale; return f;
    }
    static Filter inkEdges(float width = 2, Color ink = { 24, 22, 30, 255 }, float threshold = 0.12f) {
        Filter f; f.type = Type::InkEdges; f.size = width; f.ink = ink; f.threshold = threshold; return f;
    }
    static Filter watercolor(float bleed = 6, float edgeDarkening = 0.6f, float granulation = 0.5f) {
        Filter f; f.type = Type::Watercolor; f.size = bleed; f.strength = edgeDarkening; f.granulation = granulation; return f;
    }
    static Filter tiltShift(float position = 0.55f, float band = 0.08f, float blur = 14, float angle = 0,
                            float saturation = 0.25f) {
        Filter f; f.type = Type::TiltShift; f.position = position; f.band = band; f.size = blur; f.angle = angle;
        f.strength = saturation; return f;
    }
};

// ------------------------------------------------------------------ motion blur
// Several moments within one frame, averaged: a shutter `shutter` frames long (0.5 is a
// 180-degree shutter) centred on the frame, sampled `samples` times (1: off). Set it on a
// Comp for the whole picture or on a Layer for one node and what is under it.
struct MotionBlur { int samples = 1; float shutter = 0.5f; };
// While a blurred frame or layer is being sampled, the offset in frames of the moment being
// drawn (0 otherwise). Animation code takes part by reading time as frameTime(t): for
// example interpolate(frameTime(t), { 0, 30 }, { 0, 800 }). Code that uses t as it is
// draws the same at every sample, so it stays sharp.
double subframe();
inline double frameTime(int t) { return t + subframe(); }

// A layer renders its node's subtree offscreen and composites the flattened result, so
// opacity applies to the group as a whole (no seams where children overlap) and blend
// modes and masks act on the group. Each layer costs one offscreen pass (two with a mask).
struct Layer {
    bool enabled = true;                             // false: draw inline, as if there were no layer
    float opacity = 1.0f;                            // times the node's accumulated opacity
    Vector2 offset{ 0, 0 };                          // shifts the flattened group (composition px)
    Blend blend = Blend::Normal;
    std::function<void(const Ctx& c, int t)> mask;   // drawn in the node's space; content shows where it covers
    MaskMode maskMode = MaskMode::Alpha;
    bool invertMask = false;
    std::vector<Filter> filters;                     // applied in order to the content, before the mask (one pass each)
    MotionBlur motionBlur;                           // samples > 1: the node and its content, blurred
};

// ------------------------------------------------------------------ audio
// A sound played from its node's local frame 0 while the node is active (see media.hpp).
struct AudioClip {
    std::string file;
    double startFrom = 0, endAt = 0;     // seconds into the file (endAt 0: its end)
    double playbackRate = 1;
    bool loop = false;
    float volume = 1;
    std::function<float(int t)> volumeAt;    // times `volume`, by node-local frame
};

// ------------------------------------------------------------------ timeline tree
struct TNode;
using Nodes = std::vector<std::unique_ptr<TNode>>;

struct TNode {
    int start = 0, dur = 60;                 // relative to parent's start, in frames
    int loopEvery = 0;                       // > 0: content sees t % loopEvery (see loop())
    int freezeAt = -1;                       // >= 0: content sees this frame (see freeze())
    NodeProps props;
    std::function<void(int t, NodeProps& p)> up;                    // animate props at local frame t
    std::function<void(const Ctx& c, int t)> fn;                    // draw leaves at local frame t
    Nodes kids;
    std::vector<AudioClip> audio;            // sounds (media::audio, media::video)
    std::unique_ptr<Layer> layer;            // set via isolate()
    // Animates the layer from the node's unremapped local frame (used by transitions).
    std::function<void(int t, NodeProps& p, Layer& l)> layerUp;

    TNode* add(int off, int duration) {
        kids.push_back(std::make_unique<TNode>());
        TNode* n = kids.back().get();
        n->start = off; n->dur = duration;
        return n;
    }
    // Turns this node into a layer (idempotent) and returns its settings.
    Layer& isolate() {
        if (!layer) layer = std::make_unique<Layer>();
        return *layer;
    }
};

// Engine hooks (render.cpp). drawLayer renders a layered node offscreen and composites it;
// it returns false when no canvas is active (the node is then drawn inline, with its
// layer opacity applied per primitive and no mask). restoreBlend re-applies the engine's
// 2D blend state after user drawing code, which may have changed it.
bool drawLayer(TNode& n, int t, const Ctx& c);
// A layer with motion blur: re-evaluates the node's props at each sample (lt: remapped
// local frame, t: unremapped, as stepNode has them) and composites the average.
bool drawLayerBlurred(TNode& n, int lt, int t, const Ctx& parentCtx);
void restoreBlend();

// The node's context from its parent's and its props.
inline Ctx nodeCtx(const Ctx& parent, const NodeProps& p);

void stepNode(TNode& n, int parentLocal, const Ctx& parentCtx);

// Draws a node's own leaves and children at (remapped) local frame t.
inline void drawNodeContent(TNode& n, int t, const Ctx& c) {
    if (n.fn) { n.fn(c, t); restoreBlend(); }
    for (auto& k : n.kids) stepNode(*k, t, c);
}

inline void stepNode(TNode& n, int parentLocal, const Ctx& parentCtx) {
    int t = parentLocal - n.start;
    if (t < 0 || t >= n.dur) return;
    int lt = n.freezeAt >= 0 ? n.freezeAt : n.loopEvery > 0 ? t % n.loopEvery : t;
    if (n.layer && n.layer->enabled && n.layer->motionBlur.samples > 1 && drawLayerBlurred(n, lt, t, parentCtx)) return;
    if (n.up) n.up(lt, n.props);
    if (n.layer && n.layerUp) n.layerUp(t, n.props, *n.layer);
    Ctx c = nodeCtx(parentCtx, n.props);
    if (n.layer && n.layer->enabled) {
        if (drawLayer(n, lt, c)) return;
        c.a *= n.layer->opacity;
        c.o.x += n.layer->offset.x; c.o.y += n.layer->offset.y;
    }
    drawNodeContent(n, lt, c);
}
inline Ctx nodeCtx(const Ctx& parent, const NodeProps& p) {
    Ctx c;
    c.o = mapP(parent, p.pos);
    c.s = parent.s * p.scale;
    c.rot = parent.rot + p.rot;
    c.a = parent.a * p.opacity;
    return c;
}
inline void drawTree(const Nodes& roots, int frame) {
    Ctx root{ { 0, 0 }, 1.f, 0.f, 1.f };
    for (auto& r : roots) stepNode(*r, frame, root);
}

// composition
struct Comp {
    std::string name;
    int w = 1920, h = 1080, fps = 30;
    bool music = false;
    int duration = 0;                        // total frames
    int supersample = 2;                     // antialiasing: draw at w*N x h*N, box-filter down (1, 2 or 4)
    Color background{ 4, 4, 10, 255 };       // cleared before the timeline tree draws
    MotionBlur motionBlur;                   // samples > 1: every frame averages that many moments (not with `render`)
    Song song;                               // procedural soundtrack (used when music=true)
    Nodes roots;
    // Optional custom renderer replacing the timeline tree (multi-pass effects etc.).
    // Called with `target` (w x h) already bound; it may bind its own targets but must
    // leave `target` bound on return. Supersampling does not apply to it.
    std::function<void(Comp& c, int frame, RenderTexture target)> render;

    // Input props (Remotion's defaultProps / inputProps / calculateMetadata). When `build`
    // is set, the CLI calls it once before use with defaultProps merged with --props; it
    // fills `roots` (or `render`) and may change duration, size, fps or background.
    Json defaultProps;
    std::function<void(Comp& c, const Json& props)> build;

    // Optional: creates the files `build` reads (generated textures, models, audio). The
    // CLI runs it just before `build`, and only when frames will be drawn (still, preview,
    // render). `list` builds without it, so `build` must still set size, duration and
    // audio when it has not run.
    std::function<void(const Comp& c, const Json& props)> assets;
};

// ------------------------------------------------------------------ sequencing
// Remotion's <Sequence>: a node active for [from, from + duration) of its parent.
inline TNode* addRoot(Nodes& list, int from, int duration) {
    list.push_back(std::make_unique<TNode>());
    TNode* n = list.back().get();
    n->start = from; n->dur = duration;
    return n;
}
inline TNode* sequence(Nodes& list, int from, int duration) { return addRoot(list, from, duration); }

// <Loop>: content repeats every `every` frames, `times` times.
inline TNode* loop(Nodes& list, int from, int every, int times) {
    TNode* n = addRoot(list, from, every * times);
    n->loopEvery = every;
    return n;
}
// <Freeze>: content held at its local `frame` for `duration` frames.
inline TNode* freeze(Nodes& list, int from, int duration, int frame) {
    TNode* n = addRoot(list, from, duration);
    n->freezeAt = frame;
    return n;
}

// <Series>: sequences placed back to back. A negative offset overlaps the previous one,
// a positive offset leaves a gap.
class Series {
public:
    explicit Series(Nodes& list, int from = 0) : list_(list), end_(from) {}
    explicit Series(TNode* parent, int from = 0) : Series(parent->kids, from) {}
    TNode* add(int duration, int offset = 0) {
        TNode* n = addRoot(list_, end_ + offset, duration);
        end_ = n->start + duration;
        return n;
    }
    int end() const { return end_; }      // frame after the last sequence
private:
    Nodes& list_;
    int end_;
};

// A project's composition list; handed to rmtn::runCli (cli.hpp).
using Registry = std::vector<Comp>;

} // namespace rmtn
