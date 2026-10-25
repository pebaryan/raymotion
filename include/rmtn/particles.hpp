// rmtn/particles.hpp -- particle effects (fx::): bursts, sparks, fountains, rain, snow,
// confetti, fire, smoke, embers.
//
// Particles are not simulated step by step. Each one is a pure function of its index and
// the frame: its birth frame, lifetime and starting values come from the seed, and its
// position follows from closed-form motion (velocity, gravity, drag, turbulence). So any
// frame can be drawn alone, out of order, or twice, and always looks the same, and the
// cost per frame is the number of live particles.
//
//   fx::Emitter e = fx::burst(80, { GOLD, ORANGE });   // a preset, then adjust
//   e.gravity = { 0, 0.12f };
//   node->fn = fx::fn(e);                              // or fx::draw(cx, e, t) in your own fn
//
// Positions are relative to the node's origin, in composition px; speeds are px per frame,
// gravity px per frame squared, and every time is a frame of the node's local timeline.
#pragma once
#include <functional>
#include <vector>
#include "timeline.hpp"

namespace rmtn {
namespace fx {

// A value picked at random per particle, uniformly in [lo, hi]. A single number is fixed.
struct Range {
    float lo = 0, hi = 0;
    Range() = default;
    Range(float v) : lo(v), hi(v) {}
    Range(float a, float b) : lo(a), hi(b) {}
};

// Where particles are born, around the node origin (plus Emitter::origin if set).
enum class Area {
    Point,
    Line,      // the segment from -extent/2 to +extent/2
    Rect,      // a box `extent` wide and high, centred
    Circle,    // a disc of radius extent.x
    Ring,      // the edge of a circle of radius extent.x
};

enum class Look {
    Dot,       // filled circle, `size` across
    Glow,      // soft radial falloff `size` across with a bright core; best with `additive`
    Streak,    // a tapered line along the path, `stretch` frames long (sparks, rain)
    Square,    // rotated square
    Confetti,  // a paper rectangle that spins and flutters
    Star,      // five-pointed star
    Ring,      // outline circle
};

// One live particle at the frame asked for (what Emitter::draw and fx::each receive).
struct Particle {
    int index = 0;            // stable id: bursts first, then the stream in birth order
    float age = 0, life = 1;  // frames since birth, lifetime in frames
    float u = 0;              // age / life, 0..1
    Vector2 pos{ 0, 0 };      // node px
    Vector2 vel{ 0, 0 };      // px per frame (without turbulence)
    float size = 1;           // px, after `sizeEnd`
    float rotation = 0;       // degrees
    float alpha = 1;          // fade in/out envelope, 0..1
    Color color = WHITE;      // with its own alpha; draw with withAlpha(color, alpha)
    float random = 0;         // a fixed random value in [0, 1) for custom looks
};

struct Emitter {
    unsigned long long seed = 1;

    // ---- emission
    int burst = 0;            // particles born at `burstAt`...
    int burstAt = 0;
    int burstSpread = 0;      // ...spread over this many frames
    float rate = 0;           // stream: particles per frame (0.25 = one every 4 frames)
    int from = 0;             // the stream runs from this frame...
    int until = -1;           // ...to this one (-1: forever)
    bool prewarm = false;     // the stream is already full at `from`, as if running for a lifetime
    int maxParticles = 20000; // safety cap on the stream's live particles
    Range life{ 30, 60 };     // frames

    // ---- birth place
    Area area = Area::Point;
    Vector2 extent{ 0, 0 };
    std::function<Vector2(float frame)> origin;   // optional moving source, sampled at each birth

    // ---- motion
    Range speed{ 2, 6 };      // px per frame
    float angle = -90;        // degrees; -90 is up, 0 is right, 90 is down
    float spread = 360;       // width of the cone of directions, degrees
    bool outward = false;     // Circle/Ring/Rect: aim away from the centre instead (still `spread`)
    Vector2 gravity{ 0, 0 };  // px per frame squared
    float drag = 0;           // share of the velocity lost per frame (0.05 slows quickly)
    float turbulence = 0;     // px of smooth wandering
    float turbulenceFreq = 0.08f;   // how fast it wanders, cycles per frame times 2 pi

    // ---- look
    Look look = Look::Dot;
    Range size{ 4, 8 };       // px
    float sizeEnd = 1;        // size multiplier at the end of life
    std::vector<Color> colors{ WHITE };   // one picked per particle
    std::vector<Color> overLife;          // optional: colour runs through these from birth to death
    float fadeIn = 0.05f;     // share of the life spent fading in...
    float fadeOut = 0.35f;    // ...and out
    Range rotation{ 0, 360 }; // degrees at birth
    Range spin{ 0, 0 };       // degrees per frame
    float stretch = 3;        // Streak length, in frames of motion
    bool additive = false;    // add light instead of painting over (glows, fire, sparks)
    std::function<void(const Ctx& c, const Particle& p)> draw;   // custom look, replaces `look`
};

// Calls `f` for every particle alive at local frame t: the burst, then the stream in birth order.
void each(const Emitter& e, int t, const std::function<void(const Particle&)>& f);
std::vector<Particle> particles(const Emitter& e, int t);
// Draws the particles alive at frame t through the node context.
void draw(const Ctx& c, const Emitter& e, int t);
// A node fn that draws `e`.
std::function<void(const Ctx& c, int t)> fn(Emitter e);

// ---- presets: ordinary Emitters, ready to adjust
// Fireworks-style explosion from the origin at frame 0.
Emitter burst(int count = 120, std::vector<Color> colors = { { 255, 210, 120, 255 }, { 255, 120, 80, 255 } });
// A stream of fast, short streaks that fall (welding, grinding, impacts).
Emitter sparks(float rate = 4, Color color = { 255, 190, 90, 255 });
// Water-like jet that rises and falls back.
Emitter fountain(float rate = 3, Color color = { 120, 200, 255, 255 });
// Paper confetti falling from a `width` px line through the origin (put it above the frame).
Emitter confetti(float width = 1920, std::vector<Color> colors = { { 255, 107, 90, 255 }, { 255, 195, 101, 255 },
                                                                    { 71, 218, 216, 255 }, { 140, 120, 255, 255 } });
// Slanted rain across `width` px, falling `height` px from the origin.
Emitter rain(float width = 1920, float height = 1080);
// Drifting snow across `width` px, falling `height` px from the origin.
Emitter snow(float width = 1920, float height = 1080);
// Flames rising from a base `width` px wide.
Emitter fire(float width = 80);
// Soft grey smoke rising from a base `width` px wide.
Emitter smoke(float width = 60);
// Slow glowing motes rising over an area `width` x `height` centred on the origin.
Emitter embers(float width = 1920, float height = 1080);

} } // rmtn::fx
