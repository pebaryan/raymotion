// rmtn/geo.hpp -- maps (geo::): geographic data and map projections.
//
//     const geo::Layer& world = geo::countries();             // bundled Natural Earth 1:110m
//     auto map = geo::Projection::equalEarth().fit({ 60, 60, 1800, 960 }, world);
//     node->fn = [map](const Ctx& cx, int) {
//         vg::fill(cx, map.path(geo::land().merged()), LAND, vg::FillRule::EvenOdd);
//         vg::stroke(cx, map.lines(geo::countries().merged()), vg::Stroke(BORDER, 1));
//         vg::fill(cx, map.path(geo::countries().get("KEN").geometry), ACCENT, vg::FillRule::EvenOdd);
//     };
//
//     auto globe = geo::Projection::orthographic().rotate({ 37, 0 }).fit({ 560, 140, 800, 800 }, geo::Sphere);
//
// Coordinates are degrees: longitude east, latitude north. A projection turns them into
// node px; path() clips shapes to what the projection can show (cut at the antimeridian,
// or at the horizon of a globe) and closes them along that edge, so fills stay correct
// however the globe is turned. Fill map paths with FillRule::EvenOdd (lakes and enclaves
// are holes).
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "json.hpp"
#include "paths.hpp"

namespace rmtn {
namespace geo {

struct LonLat {
    double lon = 0, lat = 0;
};
using Ring = std::vector<LonLat>;   // closed implicitly (the last point joins the first)

// Polygons (each an outer ring then its holes), lines and points, in any mix.
struct Geometry {
    std::vector<std::vector<Ring>> polygons;
    std::vector<std::vector<LonLat>> lines;
    std::vector<LonLat> points;
    bool empty() const { return polygons.empty() && lines.empty() && points.empty(); }
    Geometry& append(const Geometry& other);
    static Geometry point(LonLat p);
    static Geometry line(std::vector<LonLat> pts);
};

// Stands for the whole globe in fit() (as does any empty geometry): the projection's outline.
extern const Geometry Sphere;

struct Bounds {
    double west = 0, south = 0, east = 0, north = 0;   // west > east when it spans the antimeridian
};

struct Feature {
    std::string id;
    Json properties;
    Geometry geometry;
    std::string name() const;           // "name", "NAME" or "admin", else the id
    // A good place for a label: the "label" property ([lon, lat]) when present, else the
    // centroid of the largest polygon, else the centroid.
    LonLat labelPoint() const;
};

class Layer {
public:
    std::vector<Feature> features;
    // By id, ISO code (iso_a2 / iso_a3) or name, ignoring case; nullptr when absent.
    const Feature* find(const std::string& key) const;
    const Feature& get(const std::string& key) const;   // throws std::runtime_error when absent
    Layer filter(const std::function<bool(const Feature&)>& keep) const;
    Geometry merged() const;                              // every feature's geometry
    size_t size() const { return features.size(); }
};

// GeoJSON (a FeatureCollection, a Feature or a bare geometry) or TopoJSON (every object,
// or just `object`). Rings are normalised: outer rings anticlockwise, holes clockwise.
// Throws std::runtime_error on malformed input.
Layer parse(const std::string& text, const std::string& object = "");
Layer load(const std::string& file, const std::string& object = "");   // resolved like media files

// Bundled Natural Earth 1:110m data (public domain), loaded on first use.
const Layer& countries();   // 177 countries: name, name_long, iso_a2, iso_a3, continent, region, subregion, pop, gdp_md, label
const Layer& land();
const Layer& lakes();
const Layer& rivers();
const Layer& cities();      // 243 populated places, largest first: name, country, iso_a2, pop, capital

// ---- on the sphere (radius 6371 km)
constexpr double EARTH_RADIUS_KM = 6371.0;
double distance(LonLat a, LonLat b);                  // great-circle distance, km
double angle(LonLat a, LonLat b);                     // the same in degrees of arc
LonLat interpolate(LonLat a, LonLat b, double t);     // along the great circle, t = 0..1
double bearing(LonLat from, LonLat to);               // initial heading, degrees clockwise from north
LonLat centroid(const Geometry& g);                   // area-weighted for polygons
Bounds bounds(const Geometry& g);
double area(const Geometry& g);                       // km^2 (polygons)
bool contains(const Geometry& g, LonLat p);           // inside a polygon (holes excluded)

// ---- projections
enum class Kind {
    Equirectangular, Mercator, EqualEarth, NaturalEarth, Robinson, Mollweide,
    Orthographic, Stereographic, AzimuthalEquidistant, AzimuthalEqualArea, Gnomonic,
    ConicEqualArea, ConicConformal, ConicEquidistant,
};

class Projection {
public:
    static Projection equirectangular();
    static Projection mercator();
    static Projection equalEarth();
    static Projection naturalEarth();
    static Projection robinson();
    static Projection mollweide();
    static Projection orthographic();         // a globe: shows the hemisphere facing you
    static Projection stereographic();
    static Projection azimuthalEquidistant();
    static Projection azimuthalEqualArea();
    static Projection gnomonic();
    static Projection conicEqualArea();       // Albers; set parallels() for the region
    static Projection conicConformal();       // Lambert
    static Projection conicEquidistant();
    static Projection make(Kind kind);
    // Projections in between: every point moves on a straight line from where `a` puts it
    // to where `b` does (a globe unrolling into a flat map). The clipping widens from a's
    // to b's as t goes from 0 to 1, and the rotation, scale and position blend too.
    static Projection mix(const Projection& a, const Projection& b, double t);

    // Spins the globe: `centre` moves to the middle of the projection (and to the front of
    // a globe); `roll` turns it about that point, degrees.
    Projection& rotate(LonLat centre, double roll = 0);
    Projection& scale(double pxPerRadian);
    Projection& translate(Vector2 px);         // where the centre lands
    Projection& parallels(double a, double b); // the two standard parallels of the conics
    Projection& clipAngle(double degrees);     // show only within this angle of the centre (0: the default)
    Projection& precision(double degrees);     // longest edge before it is subdivided (default 1)
    // Scale and position so `g` (or geo::Sphere, the whole globe) fills `box`, less `padding`.
    Projection& fit(Rectangle box, const Geometry& g, float padding = 0);
    Projection& fit(Rectangle box, const Layer& layer, float padding = 0) { return fit(box, layer.merged(), padding); }

    Kind kind() const { return kind_; }
    LonLat rotation() const { return rotate_; }
    double roll() const { return roll_; }
    double scaleValue() const { return scale_; }
    Vector2 translation() const { return translate_; }

    // Node px of a point; false when the projection does not show it (behind the globe).
    bool project(LonLat p, Vector2& out) const;
    Vector2 operator()(LonLat p) const;        // ignores clipping

    vg::Path path(const Geometry& g) const;    // polygons, clipped and closed (fill EvenOdd)
    vg::Path lines(const Geometry& g) const;   // lines and polygon rings as open strokes
    vg::Path outline() const;                  // the edge of the map (or of the globe's disc)
    // Meridians and parallels every `step` degrees (meridians stop `polar` degrees short
    // of the poles, as on most maps).
    vg::Path graticule(double step = 15, double polar = 10) const;

private:
    struct Raw;
    Vector2 rawProject(double lambda, double phi) const;   // rotated radians -> unit plane
    Vector2 place(Vector2 unit) const;
    Vector2 screen(double lambda, double phi) const;        // rotated radians -> px
    double cutAngle() const;                                // circle clip in radians (pi: none)
    bool cuts() const;                                      // cut at the antimeridian
    Kind kind_ = Kind::Equirectangular;
    LonLat rotate_{};
    double roll_ = 0, scale_ = 150, clip_ = 0, precision_ = 1;
    Vector2 translate_{ 480, 250 };
    double p1_ = 29.5, p2_ = 45.5;
    std::shared_ptr<const Projection> a_, b_;   // set by mix()
    double t_ = 0;
};

} // namespace geo
} // namespace rmtn
