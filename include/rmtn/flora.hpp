// rmtn/flora.hpp -- vegetation (flora::): grass fields at landscape scale with realistic
// looks to start from, areas (where things grow) and scattering (where each plant stands).
// Trees, growth and 2D vegetation follow (docs/flora-plan.md).
//
//   flora::Area land = flora::rect({ -100, -100 }, { 100, 100 });
//   land.ground = [](float x, float z) { return 6 * flora::noise(x, z, 60, 3); };      // hills
//   land.strips.push_back({ { { -100, 0 }, { 100, 20 } }, 5 });                          // a road
//   scene->add(flora::terrain(land), groundMaterial);
//   flora::addGrass(*scene, land, flora::meadow(), 7);                  // stops at the road
//   for (const flora::Placement& p : flora::scatter(land, { 6 }, 7)) ...  // trees, off the road
//
// The grass is a g3d::Field: every blade is made on the GPU each frame from its tile and its
// number, so a million blades in view cost what their pixels cost. Same seed, same field,
// any frame order. Wind is a function of position and time (g3d::Field::sway, gust, ...).
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "geo.hpp"
#include "scene3d.hpp"

namespace rmtn {
namespace flora {

// Realistic grass looks: start from one and change what you need.
g3d::Field lawn();          // short, dense, even and green: mown grass
g3d::Field meadow();        // calf-high mixed grass with some straw
g3d::Field tallGrass();     // waist-high, leaning and swaying more
g3d::Field dryGrass();      // late-summer or savanna grass: straw with a little green

// What grass is drawn with: thin blades that let a light behind them through (translucency),
// a matt surface, and no ink lines (a million outlines would be noise).
g3d::Material grassMaterial();

// A field over x in [min.x, max.x] and z in [min.y, max.y] of the scene (or of `parent`),
// with the look `style` and the seed `seed` (which replaces style.seed). The returned
// object's `field` can be changed further.
g3d::Object3D* addGrass(g3d::Scene& scene, Vector2 min, Vector2 max, g3d::Field style = meadow(), uint32_t seed = 1);
g3d::Object3D* addGrass(g3d::Object3D& parent, Vector2 min, Vector2 max, g3d::Field style = meadow(), uint32_t seed = 1);

// A named look ("lawn", "meadow", "tall", "dry"); throws std::invalid_argument otherwise.
g3d::Field grass(const std::string& name);

// ------------------------------------------------------------------ areas
// Where plants may grow, in x and z of the scene (or a parent's) space: a rectangle, or
// polygons, less what is excluded, over a ground, thinned by a density, by slope and by
// altitude. weight(x, z) says how much may grow at a point (0..1); the grass field and
// scatter both read it, so grass stops at the same roads the trees keep off.
struct Circle { Vector2 centre{ 0, 0 }; float radius = 1; };
struct Strip { std::vector<Vector2> points; float width = 4; };     // a road, path or river: its centre line and width

struct Area {
    Vector2 min{ -10, -10 }, max{ 10, 10 };       // bounds; with no polygons, the area is this rectangle
    // When present, inside means inside an odd number of these rings (so a hole is a ring
    // inside another). Set min and max to cover them (polygon() and region() do).
    std::vector<std::vector<Vector2>> polygons;
    std::vector<std::vector<Vector2>> exclude;      // nothing inside any of these (buildings, ponds)
    std::vector<Circle> clearings;                  // nothing within these
    std::vector<Strip> strips;                      // nothing on these
    float edge = 0;                                 // metres over which growth fades in from every edge (0 = a hard edge)
    std::function<float(float x, float z)> ground;  // height (null = 0)
    std::function<float(float x, float z)> density; // 0..1 (null = 1)
    float maxSlope = 90;                            // degrees: steeper ground thins out over the last 4
    float minAltitude = -1e30f, maxAltitude = 1e30f;

    float height(float x, float z) const;           // the ground, or 0
    float slope(float x, float z) const;            // degrees, from the ground's gradient
    bool inside(float x, float z) const;            // in the bounds and polygons, outside every exclusion
    float edgeDistance(float x, float z) const;     // metres to the nearest boundary of any kind
    float weight(float x, float z) const;           // 0..1
};

Area rect(Vector2 min, Vector2 max);
Area polygon(std::vector<Vector2> ring);
// A geo region (a country's geometry, say) through `project`, which places a longitude and
// latitude in x and z: a geo::Projection to pixels, or a laid-down geo3d::Surface's map.
Area region(const geo::Geometry& g, const std::function<Vector2(geo::LonLat)>& project);
// The upward faces of a mesh no steeper than maxSlope: ground is the highest such face
// over a point, and only where there is one may anything grow. `triangles` are three
// points each, already in the space the plants go in.
Area onMesh(const std::vector<Vector3>& triangles, float maxSlope = 35);
Area onMesh(const Mesh& mesh, Matrix transform, float maxSlope = 35);

// Smooth value noise in 0..1 (three octaves), `scale` metres across a feature: for hills
// (`ground`), patchy density and clumps.
float noise(float x, float z, float scale, uint32_t seed = 1);

// The area's ground as a mesh over its bounds, `res` quads a side (at most 254), uv 0..1
// over the bounds: put it under the grass and trees so they stand on it.
g3d::GeometryPtr terrain(const Area& area, int resX = 128, int resZ = 128);

// ------------------------------------------------------------------ scattering
struct ScatterSpec {
    float spacing = 3;              // the least distance between two placements (Poisson disk), metres
    float clumping = 0;             // 0..1: groves and clearings from noise (0 = even)
    float clumpScale = 25;          // metres across a grove
    float scaleMin = 0.8f, scaleMax = 1.25f;
    int variants = 1;               // Placement::variant is in 0..variants-1
};
struct Placement {
    Vector3 position{ 0, 0, 0 };    // on the ground
    float yaw = 0;                  // degrees
    float scale = 1;
    int variant = 0;
    uint32_t id = 0;                // stable for the bounds, spacing and seed
};
// Poisson-disk points over the area's bounds (Bridson 2007), each kept with the area's
// weight there (and the clumping): placements depend only on the bounds, spacing and seed,
// so changing a road or a density removes or restores plants without moving the others.
// Throws std::invalid_argument when the bounds would hold more than 5 million points.
std::vector<Placement> scatter(const Area& area, const ScatterSpec& spec = {}, uint32_t seed = 1);

// Grass over an area: the field covers its bounds, grows by its weight and stands on its
// ground (mapResolution raised to about two samples a metre, up to 1024).
g3d::Object3D* addGrass(g3d::Scene& scene, const Area& area, g3d::Field style = meadow(), uint32_t seed = 1);
g3d::Object3D* addGrass(g3d::Object3D& parent, const Area& area, g3d::Field style = meadow(), uint32_t seed = 1);
// The field's mask, ground and size from an area (what addGrass(area) does to `style`).
void fit(g3d::Field& field, const Area& area);

}  // namespace flora
}  // namespace rmtn
