// rmtn/lightning.hpp -- the shape and the timing of one lightning strike (rmtn::bolt). Pure functions of
// a seed and a time; no drawing here. g3d::Lightning (scene3d.hpp) draws it in 3D and fx::lightning
// (particles.hpp) in 2D, so both make the same bolt from the same seed.
//
//     bolt::Bolt b = bolt::make({ 0, 1800, 0 }, { 60, 0, 40 }, 7);       // a cloud-to-ground bolt
//     bolt::Profile p = bolt::profile(7, 0.05f);                          // 50 ms after it began
//     // p.visible: how much of the channel the leader has grown, p.flash: how bright it is (0..1)
#pragma once
#include <raylib.h>
#include <cstdint>
#include <vector>

namespace rmtn {
namespace bolt {

// One straight piece of the channel.
struct Segment {
    Vector3 a{ 0, 0, 0 }, b{ 0, 0, 0 };
    float width = 1;                // 1 on the main channel; less on branches, falling to the tip of each
    int level = 0;                  // 0 the main channel, 1 a branch of it, 2 a branch of a branch...
    float reachA = 0, reachB = 0;   // metres of channel from the strike's start to each end, along the path
};

// How jagged and how branched the bolt is.
struct Shape {
    float jag = 0.34f;              // sideways displacement of a midpoint, as a share of the piece's length (before the random factor)
    float roughness = 0.80f;        // each finer split keeps this share of the last split's relative displacement
    int depth = 7;                  // splits of the main channel: 2^depth pieces
    float branchChance = 0.07f;     // per vertex of the main channel; a branch of level L has it times branchFall^L
    float branchFall = 0.45f;
    float branchLength = 0.32f;     // of the main channel's length; times 0.55^L for a branch of level L
    int maxLevel = 3;               // levels 1 .. maxLevel - 1 branch (the main channel is level 0)
    float spread = 38;              // degrees a branch leaves its parent by (about; random)
    float downward = 0.35f;         // 0..1: how much a branch turns towards the strike's end (a bolt's branches run on, not back)
    bool planar = false;            // displace in the xy plane only (for 2D: z stays 0 when start and end have z = 0)
};

struct Bolt {
    std::vector<Segment> segments;
    Vector3 start{ 0, 0, 0 }, end{ 0, 0, 0 };
    float reach = 0;                // the longest path from the start to any tip, metres
    float length = 0;               // of the main channel along its path
};

// The bolt for `seed`: a main channel from `start` to `end` by recursive midpoint displacement, with branches
// that get shorter, rarer and thinner with depth. The same arguments always give the same bolt.
Bolt make(Vector3 start, Vector3 end, uint32_t seed, const Shape& shape = {});

// The state of a strike `tau` seconds after it began.
struct Profile {
    float visible = 0;              // 0..1: the share of the channel's reach the leader has grown (1 once it has landed)
    float flash = 0;                // 0..1+: brightness of the channel and of the light it makes (0 before and after)
    bool alive = false;             // tau is inside [0, duration)
};
// Timing, all from the seed: a leader that grows the channel over `leaderTime`, the return stroke (full
// brightness, held for at least `minVisible` seconds so a 24 or 30 fps film catches it), then 2 to 4 restrikes
// that flicker over 0.2 to 0.5 s with the channel glowing between them, then nothing.
Profile profile(uint32_t seed, float tau, float leaderTime = 0.035f, float minVisible = 0.07f);
// How long the strike lasts, seconds.
float duration(uint32_t seed, float leaderTime = 0.035f, float minVisible = 0.07f);

// Seconds for thunder to cover `distance` metres (the speed of sound, 343 m/s).
inline float thunderDelay(float distance) { return distance / 343.f; }

} // namespace bolt
} // namespace rmtn
