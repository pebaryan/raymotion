// rmtn/city.hpp -- 3D city models (city::): CityJSON files as g3d meshes.
//
//     const city::Model town = city::load("data/city/town.city.json");
//     auto scene = std::make_shared<g3d::Scene>();
//     g3d::Object3D* blocks = city::add(*scene, town);              // one child per city object
//     city::rise(blocks, 0, 2, 24);                                 // buildings grow, one after another
//     city::frame(scene->camera, town, 225, 30);                    // look at it all from the south-west
//     addRoot(c.roots, 0, 300)->fn = g3d::draw(scene);
//
// CityJSON (1.0, 1.1, 2.0, and CityJSONSeq .city.jsonl) keeps buildings, roads, terrain,
// water and plants as surfaces in metres, in a projected reference system. Coordinates
// here are metres from the model's `origin` (the middle of its extent, at its lowest
// point), in CityJSON's axes: x east, y north, z up. Meshes and the scene helpers turn them
// into g3d's axes: x east, y up, z south.
#pragma once
#include <array>
#include <functional>
#include <string>
#include <vector>
#include "json.hpp"
#include "scene3d.hpp"

namespace rmtn {
namespace city {

// A planar polygon: its outer ring, then any holes. `kind` is its semantic surface type
// ("RoofSurface", "WallSurface", "GroundSurface", "Window", "TrafficArea", ...) or empty;
// `semantic` holds that surface's other attributes.
struct Face {
    std::vector<std::vector<Vector3>> rings;
    std::string kind;
    Json semantic;
};

// One geometry of an object, at one level of detail ("1", "1.2", "2.2", ...).
struct Geometry {
    std::string type;                       // MultiSurface, Solid, MultiLineString, ...
    std::string lod;
    std::vector<Face> faces;                // every surface (solid shells flattened)
    std::vector<std::vector<Vector3>> lines;
    std::vector<Vector3> points;
};

struct Object {
    std::string id, type;                   // type: Building, BuildingPart, Road, TINRelief, ...
    Json attributes;
    std::vector<std::string> parents, children;
    std::vector<Geometry> geometry;
    // The geometry at `lod`; with an empty `lod` (or none matching), the most detailed.
    // nullptr when the object has no geometry (e.g. a Building made of BuildingParts).
    const Geometry* at(const std::string& lod = "") const;
    BoundingBox bounds(const std::string& lod = "") const;   // CityJSON axes; empty box when no geometry
    float height(const std::string& lod = "") const;         // top minus bottom (z)
};

struct Model {
    std::string version;                    // "2.0", "1.1", ...
    std::string crs;                        // e.g. "https://www.opengis.net/def/crs/EPSG/0/7415", or empty
    std::array<double, 3> origin{ 0, 0, 0 };   // the file's coordinates of local (0, 0, 0)
    Json metadata;
    std::vector<Object> objects;            // in file order

    const Object* find(const std::string& id) const;   // nullptr when absent
    const Object& get(const std::string& id) const;    // throws std::runtime_error when absent
    Model filter(const std::function<bool(const Object&)>& keep) const;
    std::vector<std::string> lods() const;  // every level of detail present, sorted
    BoundingBox bounds(const std::string& lod = "") const;
    // The file's coordinates of a local point, and back.
    std::array<double, 3> world(Vector3 local) const;
    Vector3 local(const std::array<double, 3>& world) const;
};

// Reads CityJSON or CityJSONSeq (one CityJSON header line, then CityJSONFeature lines).
// Geometry templates are expanded. Throws std::runtime_error on malformed input.
Model parse(const std::string& text);
Model load(const std::string& file);        // resolved like media files

// A face as triangles: index triples into its rings taken in order (outer ring, then each
// hole), holes cut out, wound like the outer ring. `normal` (optional) gets the face's unit
// normal (zero for a face with no area, which gives no triangles).
std::vector<int> triangles(const Face& f, Vector3* normal = nullptr);

// ---- into g3d

// CityJSON axes (x east, y north, z up) to g3d's (x east, y up, z south).
inline Vector3 toScene(Vector3 p) { return { p.x, p.z, -p.y }; }
BoundingBox toScene(BoundingBox b);

// Colours for the faces of a mesh, by semantic surface, else by object type.
struct Look {
    Color roof{ 196, 92, 74, 255 }, wall{ 236, 228, 214, 255 }, ground{ 120, 116, 110, 255 };
    Color window{ 120, 170, 210, 255 }, door{ 110, 80, 60, 255 };
    Color road{ 92, 96, 108, 255 }, water{ 70, 140, 200, 255 }, vegetation{ 110, 168, 96, 255 };
    Color terrain{ 170, 160, 130, 255 }, furniture{ 78, 82, 94, 255 }, other{ 200, 200, 200, 255 };
    std::string lod;                        // empty: each object's most detailed geometry
    // Optional: a colour per face (return BLANK to fall back to the rules above).
    std::function<Color(const Object&, const Face&)> color;
};
Color colorOf(const Look& look, const Object& o, const Face& f);

// A mesh of one object (g3d axes), positioned relative to `offset` (CityJSON axes) -- pass
// the object's base to grow it from the ground with scale.y. Faces are triangulated
// (holes included) with flat normals; vertex colours come from `look`.
g3d::GeometryPtr mesh(const Object& o, const Look& look = {}, Vector3 offset = { 0, 0, 0 });
// One mesh of every object (cheapest to draw; no per-object animation).
g3d::GeometryPtr mesh(const Model& m, const Look& look = {});
// Middle of an object's footprint at its lowest point (CityJSON axes).
Vector3 base(const Object& o, const std::string& lod = "");

// A group holding one child per object with geometry (named by its id, placed at its
// base, so scaling a child grows it from the ground). Children of a BuildingPart-style
// hierarchy are flat siblings. The group itself sits at the scene origin.
g3d::Object3D* add(g3d::Scene& scene, const Model& m, const Look& look = {}, g3d::Material material = {});
g3d::Object3D* add(g3d::Object3D& parent, const Model& m, const Look& look = {}, g3d::Material material = {});

// Grows the group's children from the ground (scale.y 0 to 1, eased), starting at frame
// `delay`, `stagger` frames apart in order of distance from `from` (scene axes; default
// the group's middle), each taking `length` frames. Overwrites the children's `up`.
void rise(g3d::Object3D* group, int delay = 0, float stagger = 2, int length = 24,
          const Vector3* from = nullptr);

// Points `camera` at the model from `azimuth` degrees (clockwise from north: 180 looks
// from the south) and `elevation` degrees above the horizon, far enough back that the
// whole model fits the camera's view with `margin` (a fraction) to spare.
void frame(g3d::Camera& camera, const Model& m, float azimuth = 200, float elevation = 35,
           float margin = 0.1f, float aspect = 16.f / 9.f);
// The same for a box in CityJSON axes (e.g. m.bounds() kept for an orbit in camera.up).
void frame(g3d::Camera& camera, BoundingBox local, float azimuth = 200, float elevation = 35,
           float margin = 0.1f, float aspect = 16.f / 9.f);

// ---- paths through the model (CityJSON axes; e.g. a MultiLineString's line, a route)

float length(const std::vector<Vector3>& path);                  // metres
// The point at fraction u (0..1) of the path's length; `heading` (optional) gets the
// direction of travel there in degrees clockwise from north. A g3d object whose front is
// +z faces along the path with rotation.y = 180 - heading.
Vector3 along(const std::vector<Vector3>& path, float u, float* heading = nullptr);

struct TrailStyle {
    float width = 3, thickness = 0.3f;      // metres
    float lift = 0.2f;                      // above the path
    Color color{ 255, 200, 87, 255 };
    bool unlit = true;                      // a flat, glowing colour (false: lit and shadowed)
};
// A ribbon along `path` that draws itself on: at frame t it runs from the start to
// fraction reveal(t) of the length (corners filled). One child per segment and corner.
g3d::Object3D* trail(g3d::Scene& scene, const std::vector<Vector3>& path, std::function<float(int t)> reveal,
                     const TrailStyle& style = {});
g3d::Object3D* trail(g3d::Object3D& parent, const std::vector<Vector3>& path, std::function<float(int t)> reveal,
                     const TrailStyle& style = {});

} // namespace city
} // namespace rmtn
