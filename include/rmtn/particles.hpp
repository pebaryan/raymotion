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
void each(const Emitter& e, double t, const std::function<void(const Particle&)>& f);
std::vector<Particle> particles(const Emitter& e, double t);
// Draws the particles alive at frame t (fractional is fine) through the node context.
void draw(const Ctx& c, const Emitter& e, double t);
// A node fn that draws `e` at frameTime(t), so it follows motion blur.
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

// ------------------------------------------------------------------ 3D
// The same emitters in a g3d scene, in world units: attach one with Scene::add or
// Object3D::add (rmtn/scene3d.hpp) and its particles live in that object's space, so they
// move with it. They are depth-tested against the scene's geometry and fogged with it.
// Dots, glows, stars and rings face the camera; streaks lie along their motion; squares
// and confetti are paper quads that tumble in 3D. Given a geometry, each particle is a lit
// instance of it instead (debris, pebbles), with shadows and ink lines.
//
//   fx::Emitter3D e = fx::fire3d(0.3f);
//   scene->add(e)->position = { 0, 0, 0 };
//   scene->add(fx::debris3d(40), g3d::Geometry::box(1, 1, 1), { GRAY, 0.8f });

// Where 3D particles are born, around the emitter's origin (plus `origin` if set).
enum class Volume {
    Point,
    Line,      // the segment from -extent/2 to +extent/2
    Box,       // a box of size `extent`, centred
    Sphere,    // a ball of radius extent.x
    Shell,     // the surface of a sphere of radius extent.x
    Disc,      // a disc of radius extent.x in the xz plane (the ground)
    Ring,      // the edge of that disc
};

struct Particle3D {
    int index = 0;
    float birth = 0;          // frame
    float age = 0, life = 1, u = 0;
    Vector3 pos{ 0, 0, 0 };   // in the emitter's object space
    Vector3 vel{ 0, 0, 0 };   // units per frame (without turbulence)
    float size = 1;           // world units, after `sizeEnd`
    float rotation = 0;       // degrees: about the view axis (billboards) or `axis` (quads, meshes)
    Vector3 axis{ 0, 1, 0 };  // a fixed random unit axis per particle
    float alpha = 1;
    Color color = WHITE;
    float random = 0;
};

struct Emitter3D {
    unsigned long long seed = 1;

    // ---- emission, as Emitter
    int burst = 0, burstAt = 0, burstSpread = 0;
    float rate = 0;
    int from = 0, until = -1;
    bool prewarm = false;
    int maxParticles = 20000;
    Range life{ 30, 60 };

    // ---- birth place
    Volume area = Volume::Point;
    Vector3 extent{ 0, 0, 0 };
    std::function<Vector3(float frame)> origin;   // optional moving source, sampled at each birth

    // ---- motion (units per frame; gravity per frame squared; +y is up)
    Range speed{ 0.02f, 0.06f };
    Vector3 direction{ 0, 1, 0 };   // the axis of the cone of directions
    float spread = 360;             // the cone's full angle, degrees (360: every direction)
    bool outward = false;           // aim away from the centre of the area instead
    Vector3 gravity{ 0, 0, 0 };
    float drag = 0;
    float turbulence = 0;           // units of smooth wandering
    float turbulenceFreq = 0.08f;
    bool floor = false;             // particles that fall to y = floorY (object space) stop there
    float floorY = 0;

    // ---- look (for mesh particles, give the emitter a geometry instead)
    Look look = Look::Dot;
    Range size{ 0.04f, 0.08f };     // world units across
    float sizeEnd = 1;
    std::vector<Color> colors{ WHITE };
    std::vector<Color> overLife;
    float fadeIn = 0.05f, fadeOut = 0.35f;
    Range rotation{ 0, 360 }, spin{ 0, 0 };
    float stretch = 3;
    bool additive = false;
};

void each(const Emitter3D& e, double t, const std::function<void(const Particle3D&)>& f);
std::vector<Particle3D> particles(const Emitter3D& e, double t);
// Where particle `p` was `ago` frames earlier (a streak's tail).
Vector3 positionAt(const Emitter3D& e, const Particle3D& p, float ago);

// ---- 3D presets (sizes in world units; a scene of about a metre per unit)
Emitter3D burst3d(int count = 150, std::vector<Color> colors = { { 255, 210, 120, 255 }, { 255, 120, 80, 255 } });
Emitter3D sparks3d(float rate = 4, Color color = { 255, 190, 90, 255 });
Emitter3D fountain3d(float rate = 4, Color color = { 120, 200, 255, 255 });
// Confetti falling from a `width` x `depth` area through the origin (put it overhead).
Emitter3D confetti3d(float width = 6, float depth = 6, std::vector<Color> colors = { { 255, 107, 90, 255 }, { 255, 195, 101, 255 },
                                                                                     { 71, 218, 216, 255 }, { 140, 120, 255, 255 } });
// Snow and rain over a `width` x `depth` area, falling `height` from the origin.
Emitter3D snow3d(float width = 10, float depth = 10, float height = 5);
Emitter3D rain3d(float width = 10, float depth = 10, float height = 5);
// Flames, smoke and embers rising from a disc of `radius` on the ground.
Emitter3D fire3d(float radius = 0.25f);
Emitter3D smoke3d(float radius = 0.2f);
Emitter3D embers3d(float radius = 0.4f);
// Chunks thrown up and out that land and rest on y = 0 (give it a geometry: Scene::add(e, geometry)).
Emitter3D debris3d(int count = 40);

} } // rmtn::fx
