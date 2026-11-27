// ocean.hpp -- internal: the FFT ocean behind Water::spectrum (W9). The renderer uploads a Frame as
// textures; Water::height and slope read the same grids on the CPU.
#pragma once
#include "rmtn/scene3d.hpp"
#include <memory>
#include <vector>

namespace rmtn {
namespace g3d {
namespace ocean {

constexpr int kN = 128;                 // grid points a side, every cascade
constexpr int kMaxCascades = 4;
constexpr int kLevels = 8;              // mip levels of a 128 grid, 128 down to 1
constexpr float kGeometryPatch = 8.f;   // cascades with a patch at least this large move the mesh and the CPU height

// The slow sine bend of a cascade's lookup coordinate: q.x = p.x + amp sin(a p.y + p1), q.z = p.z + amp sin(b p.x + p2).
struct Warp { float amp = 0, a = 0, b = 0, p1 = 0, p2 = 0; };
Warp warpOf(float patch, int cascade, bool on);
// q for p, and the two off-diagonal terms of the bend's Jacobian: alpha = dq.x / dp.z, beta = dq.z / dp.x.
Vector2 warpPoint(const Warp& w, Vector2 p, float* alpha, float* beta);

// One moment of the sea. Per cascade, N x N x 4 floats, row = z, column = x, a texel's centre on grid node (column, row):
//   a: (Dx, h, Dz, foam)   Dx and Dz are the displacement per unit of choppiness (position = node + lambda D)
//   b: (Sx, Sz, 0, 0)      slopes dh/dq with respect to the cascade's own lookup coordinate
struct Frame {
    int cascades = 0, geometry = 0;
    double seconds = 0;
    float patch[kMaxCascades] = {};
    Warp warp[kMaxCascades];
    float lambda = 1;
    float hs = 0;                       // the spectrum's Hs
    float tail = 0;                     // mean square slope above the finest cascade (unresolved roughness)
    float resolved = 0;                 // mean square slope (both axes) in the grids
    float cover = 0;                    // whitecap area asked for
    float jacobianThreshold = 1;        // the J below which foam shows (the quantile that gives `cover`)
    std::vector<float> a[kMaxCascades], b[kMaxCascades];
    // The mean square slope (|grad h|^2) the mip level l has averaged away from level 0, per cascade.
    float lost[kMaxCascades][kLevels] = {};
    uint64_t id = 0;                    // changes with every new frame
};

// The sea at `seconds` (a small cache keeps the last few; the result is shared).
std::shared_ptr<const Frame> frameAt(const Water& w, double seconds);

// The CPU surface: height of the geometry cascades relative to the level (no curvature), and the gradient
// (dh/dx, dh/dz), at world (x, z). The choppy displacement is inverted by four fixed-point steps.
float heightAt(const Frame& f, float x, float z, Vector2* slope = nullptr);

} // namespace ocean
} // namespace g3d
} // namespace rmtn
