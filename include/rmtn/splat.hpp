// rmtn/splat.hpp -- Gaussian splats (splat::): a cloud of soft, oriented 3D ellipsoids, each
// with a colour and an opacity, as in "3D Gaussian Splatting for Real-Time Radiance Field
// Rendering" (Kerbl et al., 2023). Make one in code, or load a 3DGS `.ply`, then draw it with
// g3d::Splats (see scene3d.hpp).
//
//   splat::Cloud c = splat::sphereSurface({ 0, 1, 0 }, 1.0f, 20000, [](Vector3 p, int i) { return TEAL; });
//   auto cloud = std::make_shared<const splat::Cloud>(std::move(c));
//   auto splats = std::make_shared<g3d::Splats>();
//   splats->cloud = cloud;
//   scene->add(splats);
//
//   auto scan = std::make_shared<const splat::Cloud>(splat::loadPly("assets/scan.ply"));
//
// Units are the scene's. A splat's `scale` is the standard deviation of its Gaussian along
// each of its own axes (rotated by `rotation`); opacity is 0..1. Colour is what the splat shows
// head-on (0..1, as displayed); `sh` adds a view-dependent part (degree 1 of the spherical
// harmonics, 3 coefficients per channel) when present.
#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <vector>
#include <raylib.h>
#include "voxel.hpp"

namespace rmtn {
namespace splat {

struct Cloud {
    std::vector<float> position;     // 3 a splat
    std::vector<float> scale;        // 3: standard deviations along the splat's own axes
    std::vector<float> rotation;     // 4: a unit quaternion (w, x, y, z)
    std::vector<float> opacity;      // 1: 0..1
    std::vector<float> colour;       // 3: 0..1, the degree-0 colour as displayed
    std::vector<float> sh;           // 9 a splat (R, G, B coefficients 1..3 of degree 1), or empty

    size_t size() const { return opacity.size(); }
    bool hasSH() const { return !sh.empty(); }
    Vector3 centre{ 0, 0, 0 };       // a sphere holding every splat's 3-sigma extent (see bound())
    float radius = 0;

    void add(Vector3 at, Vector3 sigma, Vector4 quaternionWXYZ, Color colour, float opacity = 1);
    void bound();                    // recompute centre and radius; the builders call it
    // The 3D covariance of splat i: R S S^T R^T, as (xx, xy, xz, yy, yz, zz).
    void covariance(size_t i, float out[6]) const;
};

// ---- builders (deterministic: `seed` fixes every random choice)
// Splats scattered over a sphere's surface, flat to it, `count` of them, coloured by `colour`.
Cloud sphereSurface(Vector3 centre, float radius, int count, const std::function<Color(Vector3 p, int i)>& colour,
                    unsigned seed = 1);
// Splats filling an axis-aligned box.
Cloud box(Vector3 centre, Vector3 size, int count, const std::function<Color(Vector3 p, int i)>& colour, unsigned seed = 1);
// `count` splats from a function of the index: position, sigma and colour are yours to give.
struct Sample { Vector3 position{ 0, 0, 0 }; Vector3 sigma{ 0.05f, 0.05f, 0.05f }; Color colour = WHITE; float opacity = 1; };
Cloud fromFunction(int count, const std::function<Sample(int i)>& fn);
// One or more splats per exposed voxel face (flat discs lying on the face, as a surface scan
// would be), in the grid's world placement given by `size` and `origin` as for voxel::mesh.
// `perFace` splats are scattered inside each face, jittered by `jitter` (0..1 of a cell).
Cloud fromVoxels(const voxel::Grid& g, voxel::MeshOptions opts = {}, int perFace = 2, float jitter = 0.5f, unsigned seed = 1);
Cloud merge(const Cloud& a, const Cloud& b);

// ---- files
// A 3DGS `.ply` (binary little-endian, as the reference trainer writes): properties are read
// by name (x y z, f_dc_0..2, f_rest_*, opacity, scale_0..2, rot_0..3; the opacity goes
// through a sigmoid, the scale through exp, the rotation is normalised, the colour is
// 0.5 + 0.2820948 * f_dc). A file with only x y z (and red green blue) loads as a point cloud
// of small round splats. Throws std::runtime_error on an ASCII or big-endian file, or a short one.
Cloud loadPly(const std::string& path);
// Writes a cloud in the same format (degree-1 coefficients in `f_rest`), so loadPly(savePly(c)) is c.
void savePly(const std::string& path, const Cloud& c);

} // namespace splat
} // namespace rmtn
