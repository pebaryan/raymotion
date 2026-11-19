// rmtn/flora.hpp -- vegetation (flora::): grass fields at landscape scale with realistic
// looks to start from, areas (where things grow), scattering (where each plant stands), and
// trees and shrubs drawn at the detail their distance needs (flora::makeTree, addForest).
// All of it can grow in by a reveal u (Field::grow, addGrowingTree), and trees and grass come
// in stylised looks (TreeStyle, toonGrass). 2D vegetation follows
// (docs/flora-plan.md).
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
g3d::Field toonGrass();     // stylised: fewer, wider blades in three flat colour bands (for Shading::Toon)

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

// ------------------------------------------------------------------ trees and shrubs
// A tree is grown from a spec and a seed: a branch skeleton (space colonisation of the crown
// for broadleaves and shrubs, whorls for conifers, a curved stem and fronds for palms), bark
// tubes, and leaf cards (pictures of a few leaves, cut out, light showing through). Wind is
// baked into a loop: the trunk bends, branches lag, leaves flutter. Every texture is made in
// code. Heights are metres; the tree stands at its origin.
enum class TreeForm { Broadleaf, Conifer, Palm };
// How a tree is drawn: Realistic (bark and leaf cards), LowPoly (faceted crowns in flat
// colours: clumps, stacked cones; good under Shading::Toon and ink lines) or PaperCut (the
// front view in two flat leaf tones and one of bark, on a single cut-out card) or Splat (limbs
// under a soft canopy of Gaussian splats; one object per tree, so at most 256 in a forest).
enum class TreeStyle { Realistic, LowPoly, PaperCut, Splat };
enum class LeafShape { Broad, Needles, Frond };

struct TreeSpec {
    TreeForm form = TreeForm::Broadleaf;
    float height = 12;              // ground to crown top
    float trunkRadius = 0.25f;      // at the ground
    float crownBase = 0.35f;        // where the crown starts, a fraction of height
    float crownRadius = 4;          // horizontal half width of the crown
    float crownShape = 0;           // broadleaf: 0 egg, 1 cone (narrow top), -1 umbrella (flat top)
    float crownLength = 0;          // > 0: a box crown this long in x (a hedge)
    int stems = 1;                  // several from the ground (shrubs, hedges)
    float gnarl = 0.3f;             // 0..1, how much branches wander
    float branchDensity = 1;        // more or fewer branches
    int whorls = 12;                // conifers: tiers of branches
    float droop = 0.2f;             // conifer branch droop, palm frond arch
    float lean = 0;                 // palms: how far the stem curves, a fraction of height
    LeafShape leaf = LeafShape::Broad;
    float leafSize = 0.8f;          // a card (a spray of leaves), metres
    float leafDensity = 1;          // more or fewer cards
    Color bark{ 92, 76, 62, 255 };
    float barkMarks = 0;            // 0..1 dark dashes across (birch)
    float barkRings = 0;            // 0..1 rings round the stem (palm)
    Color leafColor{ 70, 104, 46, 255 };
    Color autumnColor{ 196, 120, 40, 255 };
    float colorVariation = 0.15f;   // per card
    bool deciduous = true;          // drops its leaves by season 1
    // 0 spring (fresh, lighter), 0.35 summer, 0.6..0.8 autumn (cards turn at different
    // times), 1 bare (deciduous). Evergreens only darken a little.
    float season = 0.35f;
    float sway = 1;                 // wind amplitude
    uint32_t seed = 1;
};

// Realistic presets: start from one and change what you need.
TreeSpec oak();         // broad, gnarled crown on a thick trunk
TreeSpec birch();       // slender, white bark with dark marks, small light leaves
TreeSpec pine();        // tall bare trunk, rounded crown of long needles high up
TreeSpec spruce();      // dense dark cone to the ground, drooping branches
TreeSpec palm();        // curved ringed stem and arching fronds
TreeSpec tropical();    // wide umbrella crown high on a smooth trunk
TreeSpec shrub();       // a rounded bush of several stems
TreeSpec hedge();       // a 4 m length of clipped hedge along x
// A preset by name (the functions above); throws std::invalid_argument for others.
TreeSpec tree(const std::string& name);

// What a tree is built into: two meshes per detail level (wood, leaves) with the wind baked
// into `frames` frames looping every `loopSeconds`, and impostors (pictures of the tree from
// `views` directions around it, with normals so they are lit) for the distance.
struct TreeBuild {
    int frames = 24;
    float loopSeconds = 4;
    int views = 8;                  // impostor directions
    int viewSize = 192;             // impostor pixels a side
    float reducedLeaves = 0.3f;     // the reduced level keeps this fraction of cards, enlarged
    TreeStyle style = TreeStyle::Realistic;
    g3d::Shading shading = g3d::Shading::Standard;   // for the wood, leaf and impostor materials
};
struct TreeGrowth;
struct Tree {
    TreeSpec spec;
    TreeBuild build;
    struct Level { std::shared_ptr<g3d::AnimatedMesh> wood, leaves; };
    Level hero, reduced;
    std::vector<std::shared_ptr<g3d::AnimatedMesh>> views;   // impostor quads, one per direction
    g3d::Material wood, leaves, impostor;
    float top = 0, radius = 0;      // highest point; widest reach from the stem
    long long heroTriangles = 0, reducedTriangles = 0;
    std::shared_ptr<const TreeGrowth> growth;          // what growing needs (flora::addGrowingTree)
    std::shared_ptr<const splat::Cloud> canopy;        // TreeStyle::Splat: the crown, in the tree's space
};
using TreePtr = std::shared_ptr<const Tree>;
// Builds a tree (CPU only: may run before a window exists). Same spec, same tree.
TreePtr makeTree(const TreeSpec& spec, const TreeBuild& build = {});

// How a forest is drawn. Each tree picks its level from its distance to the camera of the view
// being drawn (g3d::drawingCamera, so a function of t): the full tree, the reduced one, then
// the impostor, cross-faded over `blend` metres by a screen door (Instance::fade), never
// popping. Impostors cross-fade between their two nearest directions too. Distances are
// measured in the forest's own space: leave its object untransformed.
struct ForestSpec {
    float reducedFrom = 30, impostorFrom = 80;
    float blend = 6;
    float hideFrom = 0;             // > 0: trees fade out from here over `blend`
    Vector2 windDirection{ 1, 0 };
    float wind = 1;                 // lean and sway together; 0 holds the trees still, at rest
    float gust = 0.6f;              // gust fronts lean the trees further as they pass
    float gustSpeed = 7, gustScale = 50;
    float fps = 30;                 // frames a second, for the wind's clock
    bool impostorShadows = false;   // impostors face the camera, so their shadows are poor
};
// Trees at the placements (Placement::variant picks from `trees`, wrapping round). Returns
// the forest's group; its children are the instanced levels.
g3d::Object3D* addForest(g3d::Scene& scene, const std::vector<Placement>& at, std::vector<TreePtr> trees,
                         ForestSpec spec = {});
g3d::Object3D* addForest(g3d::Object3D& parent, const std::vector<Placement>& at, std::vector<TreePtr> trees,
                         ForestSpec spec = {});
// One tree (a forest of one, so it changes level with distance and sways the same way).
g3d::Object3D* addTree(g3d::Scene& scene, TreePtr tree, Vector3 position, float yaw = 0, float scale = 1,
                       ForestSpec spec = {});


// ------------------------------------------------------------------ growth
// Everything grows by a reveal u = 0..1, as in plot:: and geo:: (0 = nothing, 1 = all).
//
// Grass: set g3d::Field::arrival to when each spot comes up (0..1) and Field::grow to u (it
// may follow t); blades rise rather than pop. The usual fronts:
std::function<float(float x, float z)> arrivalFrom(Vector2 centre, float reach);     // out from a point
std::function<float(float x, float z)> arrivalAlong(std::vector<Vector2> line, float reach);   // out from a line
std::function<float(float x, float z)> arrivalInto(Area area, float depth);          // in from an area's edges

// Trees: the trunk extends first, branches sprout as the growth front reaches them (so in
// order out from the stem) and thicken behind it, and the leaves unfold last (from u 0.6).
// Fixed topology: every vertex is a function of u, so any u can be drawn alone.
struct TreePose { std::vector<Vector3> wood, leaves; };     // the full tree's vertices
TreePose grownPositions(const Tree& tree, float u);            // u = 1 is the tree at rest

// Trees that grow at the placements (each its own mesh, updated from `grow(t, placement)`
// every frame, so keep it to a patch: at most 256). Always the full tree; it sways in the
// spec's wind like a forest (`wind` = 0 holds it still).
g3d::Object3D* addGrowingTrees(g3d::Scene& scene, const std::vector<Placement>& at, std::vector<TreePtr> trees,
                               std::function<float(int t, const Placement& p)> grow, ForestSpec wind = {});
g3d::Object3D* addGrowingTrees(g3d::Object3D& parent, const std::vector<Placement>& at, std::vector<TreePtr> trees,
                               std::function<float(int t, const Placement& p)> grow, ForestSpec wind = {});
g3d::Object3D* addGrowingTree(g3d::Scene& scene, TreePtr tree, Vector3 position, float yaw, float scale,
                              std::function<float(int t)> grow, ForestSpec wind = {});
}  // namespace flora
}  // namespace rmtn
