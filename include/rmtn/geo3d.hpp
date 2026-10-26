// rmtn/geo3d.hpp -- data on a 3D map (geo3d::): columns, spikes, bubbles, extruded and
// flat choropleths and flow arcs, standing on a globe, on a flat projected map, or on
// anything between the two while one unrolls into the other.
//
//     auto surface = std::make_shared<geo3d::Surface>(geo::Projection::equalEarth());
//     auto scene = std::make_shared<g3d::Scene>();
//     g3d::Object3D* world = scene->add();
//     globe::Map map(4096, OCEAN);
//     map.fill(geo::land().merged(), LAND);
//     g3d::Material paper;
//     paper.dynamicMap = map.texture();
//     geo3d::base(*world, surface, paper);
//     geo3d::MarkStyle cols;
//     cols.grow = [](int t) { return t / 45.f; };
//     geo3d::marks(*world, surface, geo3d::byCountry({ { "IDN", 281 }, { "IND", 1438 }, { "USA", 335 } }), cols);
//     scene->up = [surface](int t, g3d::Scene&) { surface->morph = std::clamp((t - 120) / 60.f, 0.f, 1.f); };
//
// Where things are: at morph 0 a place sits on a globe of `radius` round the origin (as
// globe::point, but turned so `centre` faces +z and north is up); at morph 1 it sits on the
// map, the projection centred on `centre`, `width` across, in the plane z = radius facing
// +z (north up +y), touching the globe at `centre`. Between, every point and its "up"
// move in a straight line: the globe opens out into the map. Heights are in world units
// along that up. Everything is built in the space of the parent object you give it, so
// turn or move the parent to lay the map down or tilt the globe.
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "geo.hpp"
#include "globe.hpp"
#include "scene3d.hpp"

namespace rmtn {
namespace geo3d {

using geo::LonLat;

class Surface {
public:
    // width 0: 2 * pi * radius (the equator keeps its length as the globe unrolls).
    explicit Surface(geo::Projection map = geo::Projection::equirectangular(), LonLat centre = {}, float radius = 1,
                     float width = 0);

    float morph = 0;                        // 0: the globe, 1: the flat map

    // A new flat map (also mid-animation, e.g. geo::Projection::mix): refits it.
    void setMap(const geo::Projection& map);
    const geo::Projection& map() const { return map_; }   // fitted: node px (see onMap)
    LonLat centre() const { return centre_; }
    float radius() const { return radius_; }
    float width() const { return width_; }
    int version() const { return version_; }              // changes with setMap

    Vector3 point(LonLat p, float height = 0) const;
    Vector3 up(LonLat p) const;                           // unit
    Vector3 north(LonLat p) const;                        // unit, along the surface
    Vector3 onGlobe(LonLat p, float height = 0) const;    // morph 0
    Vector3 onMap(LonLat p, float height = 0) const;      // morph 1 (ignores clipping)

private:
    geo::Projection map_;
    LonLat centre_;
    float radius_, width_, perPx_ = 1;
    Matrix spin_;
    int version_ = 0;
};
using SurfacePtr = std::shared_ptr<Surface>;

// The base map: an equirectangular image (material.map or .dynamicMap, e.g. a
// globe::Map's texture) on a grid that follows the surface. Its seam is opposite `centre`.
g3d::Object3D* base(g3d::Object3D& parent, SurfacePtr s, const g3d::Material& material, int segments = 192);

// ---- data

struct Datum {
    LonLat at;
    double value = 0;
    std::string key;                        // an id (byCountry: the code you gave)
    Color color = BLANK;                    // BLANK: from the scale
};
using Values = std::vector<std::pair<std::string, double>>;   // by ISO code (a2 or a3), id or name
// A datum per value whose key `layer` knows, at its label point; unknown keys are skipped.
std::vector<Datum> byCountry(const Values& values, const geo::Layer& layer = geo::countries());

// Values to colours. min == max: from the data (from 0 when nothing is negative).
struct Scale {
    double min = 0, max = 0;
    std::function<Color(float)> colors;     // default plot::viridis
    Scale over(const std::vector<double>& values) const;   // with the range filled in
    float t(double v) const;                // 0..1
    Color operator()(double v) const;
};

// ---- marks at places

enum class Mark { Column, Spike, Bubble, Disc };

struct MarkStyle {
    Mark mark = Mark::Column;
    float size = 0.03f;                     // column width, spike base, the largest bubble's or disc's diameter
    float height = 0.6f;                    // the largest value's height (columns and spikes)
    Scale scale;                            // colours, and the range heights and sizes follow
    std::function<float(int t)> grow;       // 0..1 for every mark (default: 1)
    float stagger = 0;                      // frames between marks starting, largest first
    std::function<double(int t, const Datum& d)> value;   // values over time (default: d.value)
    float roughness = 0.55f;
    bool unlit = false;
};
// One object per datum (named by its key), standing up from the surface.
g3d::Object3D* marks(g3d::Object3D& parent, SurfacePtr s, const std::vector<Datum>& data, const MarkStyle& st = {});

// ---- areas

// Fills each country of `layer` in `values` on `map` in its scale colour (a choropleth
// laid on the base map's image).
void choropleth(globe::Map& map, const Values& values, const Scale& scale = {}, const geo::Layer& layer = geo::countries());

struct ExtrudeStyle {
    float height = 0.3f;                    // the largest value's height; 0: flat shapes (a choropleth in 3D)
    float lift = 0.003f;                    // above the surface, clear of the base map
    Scale scale;
    std::function<float(int t)> grow;       // 0..1 (default: 1)
    float stagger = 0;                      // frames between countries starting, largest first
    std::function<double(int t, const std::string& key)> value;   // values over time
    float detail = 3;                       // degrees: finer edges and inner points, to follow the globe's curve
    float roughness = 0.7f;
    bool unlit = false;
};
// Each country in `values` as a prism on the surface (named by its key), height and
// colour from its value.
g3d::Object3D* extrude(g3d::Object3D& parent, SurfacePtr s, const Values& values, const ExtrudeStyle& st = {},
                       const geo::Layer& layer = geo::countries());

// ---- flows

struct Flow {
    LonLat from, to;
    double value = 1;
    Color color = BLANK;                    // BLANK: from the scale
};
struct FlowStyle {
    float height = 0.35f;                   // rise at the middle of a half-world arc (shorter ones rise less)
    float width = 0.012f;                   // the largest value's width (the smallest gets 35% of it)
    Scale scale;
    std::function<float(int t)> head;       // 0..1 along each arc (default: 1)
    std::function<float(int t)> tail;       // where it starts (default: 0)
    float stagger = 0;                      // frames between arcs starting, in the order given
    int segments = 48;
    bool unlit = true;
};
// Arcs along great circles, lifted off the surface. On the flat map, keep arcs clear of
// the map's edge (the seam opposite `centre`).
g3d::Object3D* flows(g3d::Object3D& parent, SurfacePtr s, const std::vector<Flow>& data, const FlowStyle& st = {});

// ---- labels

// Where `p`, `height` above the surface, is on screen through `camera` (g3d::cameraAt for
// the frame) in `viewport`; false when behind the camera or round the back of the globe.
// For a parent at the origin, untransformed.
bool screen(const g3d::Camera& camera, const Surface& s, LonLat p, float height, Rectangle viewport, Vector2& out);

} // namespace geo3d
} // namespace rmtn
