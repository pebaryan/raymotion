// rmtn/globe.hpp -- 3D globes (globe::): the Earth as a g3d sphere, with maps, heat maps,
// weather and routes laid on it.
//
//     auto scene = std::make_shared<g3d::Scene>();
//     globe::Map map(4096, mkColor(0x1d3557));                       // an equirectangular image
//     map.fill(geo::land().merged(), mkColor(0x7fb069)).stroke(geo::countries().merged(), WHITE, 1.5f);
//     g3d::Material earth;
//     earth.dynamicMap = map.texture();                              // painted once
//     scene->add(globe::sphere(), earth);
//     globe::route(*scene, geo::cities().get("London").labelPoint(), geo::cities().get("Tokyo").labelPoint(),
//                  [](int t) { return t / 90.f; });                  // a flight path drawing itself on
//     scene->camera.up = [](int t, g3d::Camera& cam) { globe::look(cam, { 20 + t * 0.2, 30 }, 3.2f); };
//
// Positions: a point at longitude `lon`, latitude `lat` (degrees) is on the sphere of the
// given radius, centred on the origin, with the north pole up (+y), (0, 0) facing +z and
// 90 degrees east facing +x. Images are equirectangular (2:1, west edge at -180, top at
// the north pole), which is how globe::sphere() maps its texture.
#pragma once
#include <functional>
#include <vector>
#include "geo.hpp"
#include "scene3d.hpp"

namespace rmtn {
namespace globe {

using geo::LonLat;

Vector3 point(LonLat p, float radius = 1);
LonLat lonLat(Vector3 p);                       // the place under a point (any distance out)
Vector3 north(LonLat p);                        // unit tangents at a place: towards the north pole,
Vector3 east(LonLat p);                         // and towards the east

// A sphere whose texture coordinates map an equirectangular image onto it.
g3d::GeometryPtr sphere(float radius = 1, int segments = 128);
// Part of a sphere, the lon/lat box `b` (west to east, south to north), its texture filling
// the box: a sharper overlay for one region (pair with Map(width, b) / Field(width, b)).
g3d::GeometryPtr patch(geo::Bounds b, float radius = 1, int segments = 64);

// Points `camera` at the centre from above `p`, `distance` from the centre (in radii of a
// unit globe; 3 or so shows the whole hemisphere).
void look(g3d::Camera& camera, LonLat p, float distance);

// Puts `o` on the globe at `p`, `altitude` above `radius`, standing up (+y along the
// vertical), its front (+z) towards `heading` degrees clockwise from north.
void place(g3d::Object3D& o, LonLat p, float altitude = 0, float heading = 0, float radius = 1);

// ---- images laid on the globe

// An equirectangular RGBA image drawn on the CPU (straight alpha, composited "over"): for a
// globe's base texture or an overlay shell. Covers the whole world, or the box `b` (then
// width:height follows the box's degrees).
class Map {
public:
    explicit Map(int width, Color background = BLANK);
    Map(int width, geo::Bounds b, Color background = BLANK);

    Map& fill(const geo::Geometry& g, Color c);                   // polygons (nonzero rule)
    Map& stroke(const geo::Geometry& g, Color c, float width);    // lines and polygon outlines; width in px at the equator
    Map& graticule(double step, Color c, float width);            // meridians and parallels every `step` degrees
    Map& disc(LonLat p, double radiusKm, Color c, float soft = 0.3f);   // soft: fraction of the radius that fades
    Map& gradient(Color north, Color equator, Color south);        // a latitude gradient, over what is there
    Map& over(const Map& top);                                    // another map of the same size and box, on top

    int width() const { return w_; }
    int height() const { return h_; }
    geo::Bounds bounds() const { return b_; }
    Color at(LonLat p) const;                                     // nearest pixel (BLANK outside)
    Color& pixel(int x, int y) { return px_[(size_t)y * w_ + x]; }
    LonLat place(float x, float y) const;                         // a pixel position's place
    Vector2 pixelOf(LonLat p) const;                              // and back (x unwrapped)
    Image image() const;                                          // a copy (UnloadImage it)
    g3d::DynamicTexturePtr texture() const;                       // a texture of this image as it is now

private:
    int w_, h_;
    geo::Bounds b_;
    std::vector<Color> px_;
    void blend(int x, int y, Color c, float a);
};

// A scalar field over the globe (or a box), built from soft spots, for heat maps.
class Field {
public:
    explicit Field(int width);
    Field(int width, geo::Bounds b);
    void clear();
    // A Gaussian bump `weight` high at `p`, falling to 1/e^2 (about 14%) at `radiusKm`.
    void add(LonLat p, double radiusKm, float weight);
    float at(LonLat p) const;
    float max() const;
    int width() const { return w_; }
    int height() const { return h_; }
    // Colours each value v / `scale` (clamped 0..1) by `palette` (default heat()).
    Image image(float scale = 1, const std::function<Color(float)>& palette = {}) const;
    const std::vector<float>& values() const { return v_; }

private:
    int w_, h_;
    geo::Bounds b_;
    std::vector<float> v_;
};

// See-through to dark purple, red, orange, pale yellow, for 0..1 (alpha grows with v).
Color heat(float v);

// ---- weather

// A tropical cyclone: clouds wound into a spiral round an eye.
struct Storm {
    LonLat centre;
    double radiusKm = 900;
    float strength = 1;                 // 0..1: how tight the spiral and how solid the cover
    bool south = false;                 // turns clockwise (southern hemisphere)
};

struct Weather {
    float cover = 0.5f;                 // 0..1: how much of the sky has cloud
    float softness = 0.18f;             // edge width of cloud banks
    float scale = 4.5f;                 // size of features (larger: smaller clouds)
    float evolve = 0.05f;               // how fast the shapes change, per second
    float opacity = 0.92f;
    unsigned seed = 7;
    // Wind, degrees of longitude per second at a latitude (east positive). The default
    // carries trade winds westward near the equator and westerlies eastward at mid-latitudes.
    std::function<float(float lat)> wind;
    std::vector<Storm> storms;
};

// Cloud cover as a white RGBA equirectangular image `width` wide at `seconds` (laid on a
// shell a little above the globe, opacity just under 1). Seamless at the antimeridian and
// the poles.
Image clouds(int width, double seconds, const Weather& w);
// One cloud density 0..1 at a place (what clouds() draws).
float cloud(LonLat p, double seconds, const Weather& w);

// ---- routes

// The point a fraction u along the great circle from a to b, lifted off the surface by
// `height` (in radii) at the middle. height < 0: by how far apart they are.
Vector3 arc(LonLat a, LonLat b, float u, float height = -1, float radius = 1);

struct RouteStyle {
    float height = -1;                  // see arc()
    float width = 0.006f;               // in radii
    Color color{ 255, 200, 87, 255 };
    bool unlit = true;
    int segments = 64;
};
// A line along the arc from a to b, drawn from fraction tail(t) to head(t) (no tail:
// from the start). One child per segment.
g3d::Object3D* route(g3d::Scene& scene, LonLat a, LonLat b, std::function<float(int t)> head,
                     const RouteStyle& style = {}, std::function<float(int t)> tail = {});
g3d::Object3D* route(g3d::Object3D& parent, LonLat a, LonLat b, std::function<float(int t)> head,
                     const RouteStyle& style = {}, std::function<float(int t)> tail = {});
// The same along any curve (u = 0..1 to a point in the parent's space), e.g. a storm track.
g3d::Object3D* path(g3d::Object3D& parent, std::function<Vector3(float u)> curve, std::function<float(int t)> head,
                    const RouteStyle& style = {}, std::function<float(int t)> tail = {});
// Sets `o` on the arc at u, facing along it (+z forward, +y away from the centre).
void fly(g3d::Object3D& o, LonLat a, LonLat b, float u, float height = -1, float radius = 1);

} // namespace globe
} // namespace rmtn
