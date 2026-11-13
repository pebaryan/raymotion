// rmtn/geomap.hpp -- drawing 2D maps (geo::): a base map, choropleths with a colour key,
// highlighted regions, markers, labels that keep clear of each other, great-circle routes
// that draw themselves on, and flights from one view to another.
//
//     auto world = geo::Projection::equalEarth().fit({ 60, 60, 1800, 960 }, geo::Sphere);
//     const geo::Values gdp = { { "IDN", 1371 }, { "IND", 3550 }, { "DEU", 4456 } };
//     const std::vector<geo::LonLat> trip = { { 106.8, -6.2 }, { 16.4, 48.2 } };
//     node->fn = [=](const Ctx& cx, int t) {
//         geo::base(cx, world, {}, ramp(t, 0, 40));               // coastlines draw on, land fades in
//         geo::choropleth(cx, world, gdp, {}, ramp(t, 30, 70));
//         geo::route(cx, world, trip, {}, ramp(t, 60, 120));
//     };
//
// Everything takes a reveal u = 0..1, like plot::, and draws through whatever projection it
// is given, so the same calls work on a flat map, on a globe and on a projection in flight:
//
//     const auto europe = geo::focus(world, box, geo::countries().get("AUT").geometry, 80);
//     geo::base(cx, geo::Projection::fly(world, europe, ramp(t, 0, 60)));
//
// Call these inside drawing callbacks: labels need the fonts. (ramp(t, a, b) above stands
// for your own 0..1 ramp between two frames.)
#pragma once
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "geo.hpp"
#include "plot.hpp"

namespace rmtn {
namespace geo {

// ---- data

// Values by ISO code (a2 or a3), id or name, as Layer::find reads them.
using Values = std::vector<std::pair<std::string, double>>;

// Values to colours. min == max: from the data (from 0 when nothing is negative).
struct Scale {
    double min = 0, max = 0;
    std::function<Color(float)> colors;     // default plot::viridis
    Scale over(const std::vector<double>& values) const;   // with the range filled in
    Scale over(const Values& values) const;
    float t(double v) const;                // 0..1
    Color operator()(double v) const;
};

// Values that change: a row per key, a column per time.
struct Series {
    std::vector<double> times;              // ascending (years, say)
    std::vector<std::pair<std::string, std::vector<double>>> rows;
    // A table with a category per key and a series per time ("2000", "2010", ...: series
    // names that are not numbers count 0, 1, 2, ...), as plot::Table::csv reads:
    //     code,2000,2010,2020
    //     IDN,214,244,274
    static Series from(const plot::Table& table);
    Values at(double time) const;           // between two columns: blended; outside: the nearest
    Scale scale(Scale s = {}) const;        // one range for every column, so colours keep their meaning
};

// ---- the base map

struct BaseStyle {
    Color ocean{ 22, 30, 44, 255 };         // the map's own shape (transparent: none)
    Color land{ 58, 72, 92, 255 };
    Color coast{ 150, 168, 190, 255 };
    Color border{ 110, 126, 148, 150 };     // between countries
    Color graticule{ 120, 136, 160, 60 };
    float coastWidth = 1.5f, borderWidth = 1, graticuleWidth = 1;
    double graticuleStep = 15;              // degrees; 0: no graticule
};
// Ocean, graticule, land, borders (bundled Natural Earth 1:110m). With u the ocean fades in,
// the graticule and the coastlines draw themselves on, then the land fills and the borders
// appear.
void base(const Ctx& c, const Projection& map, const BaseStyle& s = {}, float u = 1);

// ---- areas

struct ChoroplethStyle {
    Scale scale;
    Color missing{ 0, 0, 0, 0 };            // features of the layer with no value (transparent: left alone)
    Color border{ 16, 22, 32, 200 };
    float borderWidth = 1;
    float stagger = 0.5f;                   // 0: all at once ... 1: one after another, largest value first
};
// Every feature of `layer` in `values`, filled in its scale colour.
void choropleth(const Ctx& c, const Projection& map, const Values& values, const ChoroplethStyle& s = {}, float u = 1,
                const Layer& layer = countries());
// The key for a scale (give it scale.over(values), as choropleth uses): plot::colorbar.
void colorbar(const Ctx& c, Rectangle box, const Scale& scale, plot::ColorbarStyle s = {}, float u = 1);

struct HighlightStyle {
    Color fill{ 255, 180, 84, 255 };
    Color stroke{ 255, 232, 190, 255 };
    float width = 2.5f;
    Color dim{ 0, 0, 0, 0 };                // laid over the rest of the map (say { 8, 12, 20, 150 })
};
// One region picked out: the rest dims, its outline draws on, its fill fades in.
void highlight(const Ctx& c, const Projection& map, const Geometry& region, const HighlightStyle& s = {}, float u = 1);

// ---- points

struct Marker {
    LonLat at;
    std::string label;                      // empty: none
    double value = 1;                       // sizes the dot when the style is `sized`
    Color color{ 0, 0, 0, 0 };              // transparent: the style's
};
struct MarkerStyle {
    float radius = 7;                       // px; `sized`: the largest value's (areas follow the values)
    bool sized = false;
    Color color{ 255, 120, 95, 255 };
    Color ring{ 255, 255, 255, 230 };       // an outline (transparent or zero width: none)
    float ringWidth = 1.5f;
    float stagger = 0.6f;                   // 0: all at once ... 1: one after another, in the order given
    bool labels = true;
    float textSize = 24;
    Color textColor{ 232, 236, 242, 255 };
    Color halo{ 12, 16, 24, 210 };          // round the letters, to read over the map (transparent: none)
    bool bold = false;
    Font font{};                            // default fonts::ui(bold); give one for other scripts
    std::vector<Rectangle> avoid;           // node px the labels must keep clear of (a legend, a route)
};
// Dots that pop in, each with its label beside it. A label that would run into one placed
// before it tries the other sides of its dot, then is left out; the dot stays. Markers the
// projection does not show (round the back of a globe) are skipped.
void markers(const Ctx& c, const Projection& map, const std::vector<Marker>& items, const MarkerStyle& s = {}, float u = 1);
// A marker per feature of a layer of points (geo::cities(), say): its name, and its
// `valueProperty` ("pop") as the value. limit 0: all of them.
std::vector<Marker> places(const Layer& layer, size_t limit = 0, const std::string& valueProperty = "pop");

struct Label {
    LonLat at;
    std::string text;
};
struct LabelStyle {
    float size = 24;
    Color color{ 232, 236, 242, 255 };
    Color halo{ 12, 16, 24, 210 };
    bool bold = false;
    Font font{};
    float offset = 0;                       // px from the point to the text; 0: centred on the point
    float margin = 4;                       // kept clear round every label
    float stagger = 0.5f;
    std::vector<Rectangle> avoid;           // node px the labels must keep clear of
};
// Names on the map, earliest first: one that would run into a label already placed is left
// out (with an offset it tries right, left, above and below first). So give the important
// ones first. On a moving map a label can come and go as its neighbours shift; keep the
// list short enough that they do not touch.
void labels(const Ctx& c, const Projection& map, const std::vector<Label>& items, const LabelStyle& s = {}, float u = 1);
// A label per feature, at its label point, in the layer's order. limit 0: all of them.
std::vector<Label> names(const Layer& layer, size_t limit = 0);

// ---- routes

// Where a traveller is after fraction u of the way through `stops` along great circles
// (by distance), and the heading there (degrees clockwise from north).
LonLat along(const std::vector<LonLat>& stops, double u, double* bearing = nullptr);
double length(const std::vector<LonLat>& stops);    // km
// The route from fraction `from` to `to` as strokes, clipped like Projection::lines (so it
// may be in several pieces: do not trim it by length, give the fractions here instead).
vg::Path route(const Projection& map, const std::vector<LonLat>& stops, double from = 0, double to = 1);

struct RouteStyle {
    Color color{ 255, 196, 92, 255 };
    float width = 3.5f;
    std::vector<float> dash;                // on, off, ... (empty: solid)
    Color ghost{ 0, 0, 0, 0 };              // the whole route underneath, from the start (transparent: none)
    float head = 6;                         // the traveller: a dot of this radius (0: none)
    bool arrow = false;                     // an arrowhead pointing along the route instead of a dot
    Color headColor{ 255, 255, 255, 255 };
    float stops = 4;                        // a dot at each stop once it is reached (0: none)
    float tail = 0;                         // > 0: only this fraction of the route behind the head is drawn (a comet)
};
// The route draws itself on from the first stop; at u it reaches along(stops, u).
void route(const Ctx& c, const Projection& map, const std::vector<LonLat>& stops, const RouteStyle& s = {}, float u = 1);

// ---- views

// `base` reframed on `g` in `box`. Globes and other azimuthal maps turn to face it, conics
// turn to its longitude; the world maps (Equal Earth, Mercator, ...) keep north up and
// their shape, and only zoom and move.
Projection focus(const Projection& base, Rectangle box, const Geometry& g, float padding = 0);

// ---- prefabs

// Registers "geo-route" (a base map, framed on a route that draws itself on, with a dot
// and an optional label at each stop) and "geo-flyto" (a map that flies from one place to
// another and picks the destination out). runCli calls this; their props and defaults are
// in docs/geo-prefabs.md.
void registerPrefabs();

} // namespace geo
} // namespace rmtn
