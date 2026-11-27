// rmtn/lens.hpp -- what weather does to a camera's glass (fx::Lens), as one layer filter:
// raindrops that refract the picture and run down, frost or condensation creeping in from the
// edges, snowflakes that land and melt, and a windscreen wiper that clears the drops it sweeps.
//
//     fx::Lens lens;
//     lens.rain.rate = 0.7f;  lens.rain.blurBehind = 10;
//     lens.wiper.period = 2.4f;
//     node->isolate().filters.push_back(fx::lens(lens));         // a 2D layer, or a 3D scene's node
//     lens.rain.rateAt = fx::rainOf(scene->weather);              // follow the scene's rain
//
// Everything is a pure function of the frame and the seeds: the drops are not simulated, each one
// is worked out from a hash of its cell and its life cycle, so any frame renders on its own.
// The picture is cut into square cells (CELL_OF_HEIGHT of the layer's height). Each cell holds a
// few slots; a slot lives in cycles of 7-15 s, and in each cycle it may spawn one drop (the chance
// follows the rate at the moment of spawning). A drop grows from nothing to its radius in about
// a second, stays, and shrinks away at the end of its life. A big one (radius over 0.27 cell)
// starts to slide after a delay, at a speed of  slide * 3.667 * radius  px per second, with a
// small wiggle, and leaves a trail of tiny drops; it stops after 0.7-1.9 cells.
// The CPU functions below (lensDrops, lensFlakes) return the same drops the shader draws.
#pragma once
#include "timeline.hpp"
#include <raylib.h>
#include <cstdint>
#include <functional>
#include <vector>

namespace rmtn {
namespace g3d { struct Weather; }
namespace fx {

constexpr float LENS_CELL_OF_HEIGHT = 0.085f;    // the raindrop cell, as a share of the layer's height
constexpr float LENS_SNOW_CELL_OF_HEIGHT = 0.16f;

// Raindrops on the glass.
struct LensRain {
    float rate = 0;              // 0..1: how many of the slots spawn a drop (0: none; 1: nearly all); steady
    std::function<float(double)> rateAt;   // film seconds -> rate; replaces `rate` when set (see rainOf)
    float size = 1;              // times the drops' radii (0.3..1.5)
    float slide = 1;             // times the speed of the sliding drops; 0: nothing slides
    float blurBehind = 0;        // px: the picture behind the drops is out of focus by this much (a disc blur
                                 // that blooms bright lights); the drops show it sharp
    int seed = 1;
};
// Frost (crystals) or condensation (a fog with clear streaks where drops ran) on the glass.
struct LensFrost {
    enum class Kind { Frost, Condensation };
    float amount = 0;            // 0..1: how much of the glass is covered (0: none)
    bool fromEdges = true;       // creeps in from the edges and corners; false: patches over all of it
    Kind kind = Kind::Frost;
    int startFrame = 0, endFrame = 0;      // endFrame > startFrame: `amount` creeps from 0 to its value between them
    std::vector<Vector3> clears;           // up to 4 cleared patches: x, y, radius (px of the layer)
    int seed = 1;
};
// Snowflakes that land on the glass at hashed times, sit as soft white blobs with a crystal's six arms
// and melt into small water drops.
struct LensSnow {
    float rate = 0;              // 0..1: how many slots land a flake
    std::function<float(double)> rateAt;
    int seed = 1;
};
// A windscreen wiper: the arm sweeps from `from` to `to` degrees (0 = straight up, positive to the
// right) and back, then rests. Each stroke takes 0.3 of the period, the rest 0.4.
struct Wiper {
    float period = 0;            // seconds; 0: no wiper
    Vector2 pivot{ 0.5f, 1.18f };          // of the layer's width and height (below the frame)
    float length = 1.06f;        // of the layer's height
    float from = -55, to = 55;   // degrees
    bool drawBlade = true;       // a dark arm and blade
    float bladeWidth = 0.012f;   // of the layer's height
};
struct Lens {
    LensRain rain;
    LensFrost frost;
    LensSnow snow;
    Wiper wiper;
};

// The filter for a layer (Layer::filters). With rain.rate, frost.amount and snow.rate at 0 and no
// wiper, it leaves every pixel as it was.
Filter lens(const Lens& l);
// Convenience: one effect each (use one Lens when the wiper must clear the rain).
Filter lensRain(const LensRain& r);
Filter lensFrost(const LensFrost& f);
Filter lensSnow(const LensSnow& s);
Filter wiper(const Wiper& w);
// The rain / snow of a scene's weather as a rate over film seconds, for LensRain::rateAt.
std::function<float(double)> rainOf(const g3d::Weather& w, float gain = 1.f);
std::function<float(double)> snowOf(const g3d::Weather& w, float gain = 1.f);

// ---- the CPU twin: the same drops the shader draws (tests, and anything that wants to know where they are)
struct LensDrop {
    enum class Kind { Rain, Trail, Melt };
    Kind kind = Kind::Rain;
    Vector2 pos{ 0, 0 };         // px of the layer (y down), where the drop is at this time
    float radius = 0;            // px, at this time
    bool sliding = false;        // moving at this moment
    float speed = 0;             // px/s while sliding
    double born = 0;             // film seconds
    uint64_t id = 0;             // the same drop has the same id at every frame
};
// Every drop on a w x h layer at `seconds` (after the wiper has cleared what it swept).
std::vector<LensDrop> lensDrops(const Lens& l, double seconds, float w, float h);
// The same without the wiper's clearing.
std::vector<LensDrop> lensDropsUnwiped(const Lens& l, double seconds, float w, float h);
// One landed snowflake: `melt` 0 while it is a flake, rising to 1 as it becomes a drop.
struct LensFlake { Vector2 pos{ 0, 0 }; float radius = 0, melt = 0, alpha = 0; };
std::vector<LensFlake> lensFlakes(const Lens& l, double seconds, float w, float h);
// The blade's angle in degrees at `seconds` (0 = straight up), and the sector it sweeps.
float wiperAngle(const Wiper& w, double seconds);
// The sliding speed in px/s of a drop of this final radius on a layer `h` px high.
float lensSlideSpeed(const LensRain& r, float radius);
// Frost's amount at layer frame t (the creep between startFrame and endFrame).
float lensFrostAmount(const LensFrost& f, int t);

} // namespace fx

// Used by applyFilter for Filter::Type::Lens.
void applyLens(const Filter& f, const RenderTexture& src, const RenderTexture& dst, float scale, int t);

} // namespace rmtn
