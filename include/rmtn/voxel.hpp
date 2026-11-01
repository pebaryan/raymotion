// rmtn/voxel.hpp -- voxel models (voxel::): a grid of palette-coloured cells, MagicaVoxel
// `.vox` files in and out, and a greedy mesher that turns a grid into one ordinary mesh.
//
//   voxel::Grid g(64, 24, 64);                     // x, y (up), z cells
//   g.palette[1] = { 214, 196, 150, 255 };         // palette entries 1..255; 0 is empty
//   g.box(0, 0, 0, 63, 3, 63, 1);                  // inclusive corners, then a palette index
//   g.sphere(32, 8, 32, 6, 2);
//   voxel::MeshStats stats;
//   voxel::MeshOptions o; o.size = 0.25f; o.centre = true;
//   scene->add(voxel::mesh(g, o, &stats), material);
//
// The mesh is a normal Geometry, so it takes lights, shadows, fog, water, culling and
// instancing like any other. Faces hidden between two solid cells are dropped, coplanar faces
// of one colour are merged into larger quads (greedy meshing), and ambient occlusion is baked
// into the vertex colours the classic way (0..3 per corner, the quad's diagonal chosen so the
// shading does not streak). Cells whose corners are occluded unevenly are not merged.
//
// A grid is y-up like the rest of the scene. `.vox` files are z-up and right-handed: the
// loader maps (x, y, z) of the file to (x, z, -y) here, so a model stands the way it did in
// MagicaVoxel.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <raylib.h>
#include "scene3d.hpp"

namespace rmtn {
namespace voxel {

struct Grid {
    int sx = 0, sy = 0, sz = 0;                  // cells along x, y (up), z
    std::vector<uint8_t> cells;                  // 0 = empty, else a palette index (sx * sy * sz)
    std::array<Color, 256> palette;              // entry 0 is unused

    Grid();
    Grid(int sx, int sy, int sz);                // empty, with a default palette

    bool inside(int x, int y, int z) const { return x >= 0 && y >= 0 && z >= 0 && x < sx && y < sy && z < sz; }
    uint8_t get(int x, int y, int z) const;      // 0 outside the grid
    void set(int x, int y, int z, uint8_t c);    // ignored outside the grid
    void fill(uint8_t c);
    void box(int x0, int y0, int z0, int x1, int y1, int z1, uint8_t c);   // inclusive corners, any order
    void sphere(float cx, float cy, float cz, float r, uint8_t c);         // cells whose centres are within r
    void line(int x0, int y0, int z0, int x1, int y1, int z1, uint8_t c);
    int count() const;                           // solid cells
};

// A model of a `.vox` scene in place: the cells in their own grid, whose corner is at
// (ox, oy, oz) cells from the scene's origin.
struct Placed {
    Grid grid;
    int ox = 0, oy = 0, oz = 0;
};

// MagicaVoxel `.vox`. `loadScene` returns every model placed by the file's scene graph
// (translations and the 24 axis-aligned rotations; hidden nodes skipped; animation frames
// beyond the first are ignored); `load` merges them into one grid; `loadModels` returns each
// model chunk on its own in file order, for animations stored as a model per frame.
// Files without a palette get a default one. Throws std::runtime_error on a bad file.
std::vector<Placed> loadScene(const std::string& path);
Grid load(const std::string& path);
std::vector<Grid> loadModels(const std::string& path);
// Writes one model (version 150, no scene graph), so load(save(g)) is g.
void save(const std::string& path, const Grid& grid);

// A merged face: four corners in cells (counter-clockwise seen from outside), the palette
// index, and the occlusion at each corner (0 dark .. 3 open).
struct Quad {
    std::array<Vector3, 4> corner;
    std::array<uint8_t, 4> ao;
    uint8_t colour = 0;
    int axis = 0;                                // the face's normal axis: 0 x, 1 y, 2 z
    bool positive = true;                        // towards +axis
};
// The greedy mesh as quads, with no GPU involved.
std::vector<Quad> quads(const Grid& g, bool ambientOcclusion = true);

struct MeshOptions {
    float size = 1;                              // world size of one cell
    Vector3 origin{ 0, 0, 0 };                   // world position of the grid's corner (before `centre`)
    bool centre = false;                         // centre x and z on the origin; y starts at the origin
    bool ao = true;                              // bake ambient occlusion
    float aoStrength = 0.65f;                    // 0 = none .. 1 = darkest corners are black
};
struct MeshStats {
    int voxels = 0;                              // solid cells
    int faces = 0;                               // exposed cell faces before merging
    int quads = 0;                               // after merging
    int triangles = 0;
};
// One mesh of the whole grid (vertex colours from the palette, a flat normal per quad). The
// mesh is built on first draw. A grid too big for 16-bit indices is drawn unindexed.
g3d::GeometryPtr mesh(const Grid& g, MeshOptions opts = {}, MeshStats* stats = nullptr);

// An animation of grids: one object per grid in a group, the one for frame t shown, each
// held for `hold` film frames, looping. Returns the group (position it as you like).
g3d::Object3D* addFrames(g3d::Scene& scene, const std::vector<Grid>& frames, const g3d::Material& m,
                         int hold, MeshOptions opts = {});

} // namespace voxel
} // namespace rmtn
