// rmtn/plot.hpp -- plots and charts (plot::): axes and number lines with TeX-set numbers,
// function graphs, parametric curves, areas, Riemann rectangles, tangents and vector
// fields; animated bar, line, area, scatter and pie charts from data or CSV; statistics
// (box plots, violins, histograms, densities, error bars, heatmaps); and business charts
// (bubbles, radar, gauges, waterfalls, funnels, candles, sunbursts, treemaps, Sankey and
// chord diagrams, stream graphs, parallel coordinates); and contour maps. Everything is
// drawn as vector paths and takes a reveal parameter u = 0..1.
//
//   plot::Axes ax({ 260, 140, 1400, 760 }, { -4, 4 }, { -2, 2 });   // box (node px), x range, y range
//   const vg::Path sine = plot::graph(ax, [](double x) { return std::sin(x); });
//   node->fn = [ax, sine](const Ctx& cx, int t) {
//       plot::axes(cx, ax, {}, ramp(t, 0, 30));                     // axes draw on
//       vg::stroke(cx, sine.trim(0, ramp(t, 20, 80)), vg::Stroke(SKYBLUE, 5));
//   };
//
// Charts read a Table (categories x series), built in code or from CSV, and grow in with u.
// To animate between two datasets, draw Table::mix(a, b, w).
#pragma once
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "paths.hpp"
#include "timeline.hpp"

namespace rmtn {
namespace plot {

// A data interval. step 0 picks a round step (1, 2 or 5 times a power of ten) for about
// six to ten ticks; axes() widens an automatic step further if its ticks would crowd.
struct Range {
    Range() = default;
    Range(double lo, double hi, double tickStep = 0) : min(lo), max(hi), step(tickStep) {}
    double min = 0, max = 1, step = 0;
    double tick() const;              // the step in use
    std::vector<double> ticks() const;   // multiples of tick() inside [min, max]
};

// Where a data range lands on screen. y grows upwards in data, downwards on screen.
struct Axes {
    Axes() = default;
    Axes(Rectangle area, Range xr, Range yr) : box(area), x(xr), y(yr) {}
    Rectangle box{ 160, 120, 1600, 800 };   // node px covered by [x.min, x.max] x [y.min, y.max]
    Range x{ -5, 5 }, y{ -3, 3 };
    Vector2 map(double dx, double dy) const;   // data -> node px
    Vector2 map(Vector2 d) const { return map(d.x, d.y); }
    Vector2 unmap(Vector2 px) const;           // node px -> data
    float sx() const;                 // px per x unit
    float sy() const;                 // px per y unit
    Vector2 origin() const;           // where the axes cross: data (0, 0), clamped into the ranges
};

// A horizontal number line: Axes with a flat box (y is ignored).
Axes numberLine(Vector2 left, float length, Range x);

struct AxesStyle {
    Color color{ 200, 204, 214, 255 };
    float width = 3;
    bool xAxis = true, yAxis = true;
    bool arrows = true;               // arrow tips at the positive ends
    bool framed = false;              // axes along the bottom and left edges instead of through the origin
    bool ticks = true;
    float tickLength = 14;
    bool xNumbers = true, yNumbers = true;   // tick values, typeset with the math font
    float numberSize = 28;
    Color numberColor{ 200, 204, 214, 255 };
    bool skipZero = true;             // no "0" where the axes cross (unless framed)
    bool xGrid = false, yGrid = false;   // lines across the box at every x / y tick
    Color gridColor{ 120, 130, 150, 70 };
    float gridWidth = 1.5f;
    std::string xLabel, yLabel;       // TeX, at the ends of the axes ("x", "f(x)", "\\text{time (s)}")
    float labelSize = 36;
};
// Grid, then axis lines, ticks and numbers; u = 0..1 draws them on (lines grow from the
// origin, numbers fade in behind them).
void axes(const Ctx& c, const Axes& a, const AxesStyle& s = {}, float u = 1);
// Typesets `tex` with the math font (cached) and draws it with `anchor` on `at`.
void label(const Ctx& c, const std::string& tex, Vector2 at, Vector2 anchor = { 0.5f, 0.5f }, float size = 32,
           Color color = { 255, 255, 255, 255 });

// ---- graphs, as vg::Paths in node px (build once, then stroke, trim or morph them)
using Fn = std::function<double(double)>;
// y = f(x) over [from, to] (default: the x range), sampled adaptively. The curve breaks at
// NaN, infinities and jumps (tan x, 1/x) and is clipped to the axes' box.
vg::Path graph(const Axes& a, const Fn& f, double from = NAN, double to = NAN);
// (x(t), y(t)) for t in [t0, t1], clipped to the box.
vg::Path parametric(const Axes& a, const std::function<Vector2(double)>& p, double t0, double t1, int samples = 600);
// r = f(theta) for theta in [t0, t1] (radians), clipped to the box.
vg::Path polar(const Axes& a, const Fn& r, double t0 = 0, double t1 = 6.283185307179586, int samples = 600);
// The region between f and g (default: the x axis) over [from, to], closed for filling.
vg::Path area(const Axes& a, const Fn& f, double from, double to, const Fn& g = nullptr);
// n rectangles under f over [from, to]; `at` 0 = left sums, 0.5 = midpoint, 1 = right sums.
std::vector<vg::Path> riemann(const Axes& a, const Fn& f, double from, double to, int n, float at = 0);
// The tangent to f at x, `length` px long.
vg::Path tangent(const Axes& a, const Fn& f, double x, float length = 400);

struct FieldStyle {
    float spacing = 70;               // px between arrows
    float maxLength = 0;              // px for the strongest arrow; 0 = 0.85 * spacing
    float width = 3;
    Color slow{ 88, 196, 221, 255 }, fast{ 252, 98, 85, 255 };   // coloured by magnitude
};
// Arrows of the vector field v(x, y) on a grid over the axes' box; u = 0..1 grows them.
void field(const Ctx& c, const Axes& a, const std::function<Vector2(Vector2)>& v, const FieldStyle& s = {}, float u = 1);
// Dots at data points; u reveals them in order.
void scatter(const Ctx& c, const Axes& a, const std::vector<Vector2>& pts, Color color, float radius = 8, float u = 1);

// ---- charts
struct Series {
    std::string name;
    std::vector<double> values;       // one per category
    Color color{ 0, 0, 0, 0 };        // transparent: from the palette
};
struct Table {
    std::vector<std::string> categories;
    std::vector<Series> series;
    // CSV: a header row (first cell ignored, then series names), then one row per category
    // (its name, then a number per series). Resolved like media files; throws on bad input.
    static Table csv(const std::string& file);
    static Table parseCsv(const std::string& text);
    // Values blended a -> b (same shape; categories and names from b), for animated updates.
    static Table mix(const Table& a, const Table& b, float u);
};

struct ChartStyle {
    Rectangle box{ 240, 140, 1440, 720 };   // the plotting area; labels go around it
    Range y{ 0, 0 };                  // min == max: from the data (bars, lines, areas from 0)
    std::vector<Color> palette;       // empty: a built-in eight-colour palette
    AxesStyle axes;                   // grid on by default for charts
    float gap = 0.3f;                 // bars: free space between categories, as a fraction of a slot
    bool stacked = false;             // bars: series on top of each other
    bool values = true;               // value labels on bars, and on points when a line chart has one series
    int decimals = 0;                 // value label decimals
    bool legend = true;               // when there is more than one series
    float textSize = 30;
    Color textColor{ 220, 224, 232, 255 };
    float lineWidth = 5, pointRadius = 9;   // line charts
    ChartStyle() { axes.yGrid = true; axes.xNumbers = false; axes.arrows = false; axes.framed = true; axes.ticks = false; }
};
// The axes a chart of `t` uses (categories along x at 0.5, 1.5, ...).
Axes chartAxes(const Table& t, const ChartStyle& s);
// Bars grow from zero, category by category; u = 0..1.
void bars(const Ctx& c, const Table& t, const ChartStyle& s = {}, float u = 1);
// Lines draw on left to right, points pop in as the line reaches them; u = 0..1.
void lines(const Ctx& c, const Table& t, const ChartStyle& s = {}, float u = 1);

struct PieStyle {
    float inner = 0;                  // donut hole radius as a fraction of the radius
    float gapDeg = 1.2f;              // between slices
    float startDeg = -90;             // first slice starts at 12 o'clock and runs clockwise
    std::vector<Color> palette;
    bool labels = true;               // category names outside, percentages inside
    float textSize = 30;
    Color textColor{ 220, 224, 232, 255 };
    Color valueColor{ 20, 23, 31, 255 };   // the percentages, on the slices
    int explode = -1;                 // a slice pushed out by `explodeBy` px
    float explodeBy = 24;
};
// The first series of `t` as a pie; u = 0..1 sweeps it round.
void pie(const Ctx& c, const Table& t, Vector2 center, float radius, const PieStyle& s = {}, float u = 1);

// The built-in palette.
Color palette(int i);

// ---- statistics

// [lo, hi] padded by `pad` of the span, then rounded out to whole ticks.
Range nice(double lo, double hi, double pad = 0);
// Five-number summary: whiskers at the furthest data within 1.5 IQR of the box.
struct Summary {
    double lo = 0, q1 = 0, median = 0, q3 = 0, hi = 0, mean = 0;
    std::vector<double> outliers;
};
Summary summarize(std::vector<double> values);
// A kernel density estimate (Gaussian; bandwidth 0 = Silverman's rule). Graph it with graph().
Fn density(const std::vector<double>& values, double bandwidth = 0);
// Counts in equal bins over [lo, hi] (NaN: the data's extent, rounded out). bins 0 = automatic.
struct Bins {
    double lo = 0, width = 1;
    std::vector<double> counts;
    double hi() const { return lo + width * counts.size(); }
    double max() const;
};
Bins bin(const std::vector<double>& values, int bins = 0, double lo = NAN, double hi = NAN);

// ---- numeric-axis charts (draw the axes yourself with axes())

// Histogram bars rising from y = 0; u = 0..1.
void histogram(const Ctx& c, const Axes& a, const Bins& b, Color color, float u = 1);
// Vertical error bars of +-err[i] about pts[i], with caps; u grows them.
void errorBars(const Ctx& c, const Axes& a, const std::vector<Vector2>& pts, const std::vector<double>& err, Color color,
               float width = 3, float cap = 10, float u = 1);

struct Bubble {
    double x = 0, y = 0, size = 1;    // the area is proportional to size
    std::string label;                // plain text, drawn inside when it fits
    Color color{ 0, 0, 0, 0 };        // transparent: palette by index
};
struct BubbleStyle {
    float maxRadius = 70;             // px for the largest size
    double maxSize = 0;               // the size drawn at maxRadius; 0 = the largest
    float opacity = 0.75f;
    float outline = 2;                // a brighter ring; 0 for none
    std::vector<Color> palette;
    bool labels = true;
    float textSize = 26;
    Color textColor{ 20, 23, 31, 255 };
};
// Bubbles pop in one after another (largest drawn first); u = 0..1.
void bubbles(const Ctx& c, const Axes& a, const std::vector<Bubble>& b, const BubbleStyle& s = {}, float u = 1);

struct Candle { double open = 0, high = 0, low = 0, close = 0; };
struct CandleStyle {
    Color up{ 131, 193, 103, 255 }, down{ 252, 98, 85, 255 };
    float width = 0.6f;               // body width as a fraction of one x unit
    float wick = 2;
};
// Candle i is centred on x = i + 0.5; they grow from their open price left to right.
void candles(const Ctx& c, const Axes& a, const std::vector<Candle>& k, const CandleStyle& s = {}, float u = 1);

// ---- category charts (ChartStyle: box, axes, palette, labels)

// Stacked (s.stacked) or overlapping filled areas, drawn on left to right.
void areas(const Ctx& c, const Table& t, const ChartStyle& s = {}, float u = 1);
// One box per group (Series::name labels it, values are the raw samples). Range: from the data.
void boxplot(const Ctx& c, const std::vector<Series>& groups, const ChartStyle& s = {}, float u = 1);
// Mirrored density per group with a slim box plot inside.
void violin(const Ctx& c, const std::vector<Series>& groups, const ChartStyle& s = {}, float u = 1);

struct WaterfallStyle : ChartStyle {
    Color rise{ 131, 193, 103, 255 }, fall{ 252, 98, 85, 255 }, sum{ 88, 196, 221, 255 };
    bool total = true;                // a final bar with the running total
    std::string totalLabel = "Total";
    bool connectors = true;           // dashed lines from bar to bar
};
// The first series as changes: each bar starts where the previous one ended.
void waterfall(const Ctx& c, const Table& t, const WaterfallStyle& s = {}, float u = 1);

// ---- grids of values

// Colour maps for t = 0..1.
Color viridis(float t);
Color diverging(float t);             // blue - near white - red; 0.5 is the midpoint
struct HeatmapStyle {
    Rectangle box{ 300, 140, 1320, 760 };
    Range v{ 0, 0 };                  // value range for the colours; min == max: from the data
    std::function<Color(float)> colors = viridis;
    float gap = 3;
    bool values = true;               // cell values, dark or light to suit the cell
    int decimals = 0;
    bool colorbar = true;             // a scale to the right
    float textSize = 26;
    Color textColor{ 220, 224, 232, 255 };
};
// Rows are the table's categories, columns its series; cells sweep in diagonally.
void heatmap(const Ctx& c, const Table& t, const HeatmapStyle& s = {}, float u = 1);

// ---- keys

struct ColorbarStyle {
    std::function<Color(float)> colors = viridis;
    std::string title;                // above the bar
    int decimals = -1;                // tick numbers; -1: from the tick step
    std::string suffix;               // after each number ("%", " t")
    float textSize = 22;
    Color textColor{ 220, 224, 232, 255 };
};
// A colour scale for v.min..v.max in `box`: upright (low at the bottom) when the box is
// taller than wide, else across (low at the left), ticks at v's steps. It grows in with u.
void colorbar(const Ctx& c, Rectangle box, Range v, const ColorbarStyle& s = {}, float u = 1);

struct LegendItem {
    std::string name;
    Color color;
};
struct LegendStyle {
    float textSize = 22;
    float swatch = 18;                // square size
    float gap = 10;                   // between rows (or items, across)
    bool across = false;              // in a row instead of a column
    bool round = false;               // circles instead of squares
    Color textColor{ 220, 224, 232, 255 };
};
// Swatches and names from `at` (top left); items fade in one after another with u.
void legend(const Ctx& c, Vector2 at, const std::vector<LegendItem>& items, const LegendStyle& s = {}, float u = 1);

// ---- round charts

struct RadarStyle {
    Range r{ 0, 0 };                  // min == max: 0 to the data's maximum, rounded up
    std::vector<Color> palette;
    float fillAlpha = 0.25f, lineWidth = 4, pointRadius = 6;
    Color gridColor{ 120, 130, 150, 110 };
    bool labels = true, numbers = true, legend = true;
    float textSize = 28;
    Color textColor{ 220, 224, 232, 255 };
    float startDeg = -90;             // first spoke straight up, then clockwise
};
// Categories are the spokes, each series a polygon; the web draws on, then the series grow out.
void radar(const Ctx& c, const Table& t, Vector2 center, float radius, const RadarStyle& s = {}, float u = 1);

struct GaugeStyle {
    Range range{ 0, 100 };
    float sweepDeg = 240;             // centred on 12 o'clock
    float thickness = 0.16f;          // as a fraction of the radius
    Color track{ 52, 58, 76, 255 }, fill{ 88, 196, 221, 255 };
    std::vector<std::pair<double, Color>> zones;   // fill colour from the last threshold reached
    bool ticks = true;
    int decimals = 0;
    std::string unit;                 // TeX after the number ("\\%", "\\,\\text{ms}")
    std::string title;                // plain text under the number
    float textSize = 80;
    Color textColor{ 236, 239, 244, 255 };
};
// The arc fills and the number counts up to `value`; u = 0..1.
void gauge(const Ctx& c, double value, Vector2 center, float radius, const GaugeStyle& s = {}, float u = 1);

// ---- hierarchies and flows

struct Tree {
    std::string name;
    double value = 0;                 // leaves; a parent's size is the sum of its children
    std::vector<Tree> children;
    Color color{ 0, 0, 0, 0 };        // transparent: from the palette (children shade their parent's)
    double total() const;
    // "Europe/France/Paris, 12" per line; parents are made as needed. Throws on bad input.
    static Tree parse(const std::string& text, char sep = '/');
    static Tree csv(const std::string& file, char sep = '/');
};
struct SunburstStyle {
    float inner = 0.22f;              // centre disc (the root) as a fraction of the radius; 0 = none
    float gapDeg = 0.5f, ringGap = 2;
    int depth = 0;                    // rings shown; 0 = all
    std::vector<Color> palette;
    bool labels = true;               // names on segments wide enough to hold them
    float textSize = 24;
    Color textColor{ 20, 23, 31, 255 }, centerColor{ 236, 239, 244, 255 };
    float startDeg = -90;
};
// Rings of a hierarchy, sweeping round with each ring a little behind the one inside it.
void sunburst(const Ctx& c, const Tree& t, Vector2 center, float radius, const SunburstStyle& s = {}, float u = 1);

struct TreemapStyle {
    float gap = 3;
    int depth = 0;                    // levels shown; 0 = all
    std::vector<Color> palette;
    bool labels = true, values = true;   // on tiles big enough
    int decimals = 0;
    float textSize = 26;
    Color textColor{ 20, 23, 31, 255 }, headerColor{ 220, 224, 232, 255 };
};
// Squarified tiles, areas proportional to size; top-level groups get a header. Tiles grow in.
void treemap(const Ctx& c, const Tree& t, Rectangle box, const TreemapStyle& s = {}, float u = 1);

struct Flow {
    std::string from, to;
    double value = 0;
};
// "from, to, value" per line (a header line is skipped). Throws on bad input.
std::vector<Flow> parseFlows(const std::string& text);
std::vector<Flow> flowsCsv(const std::string& file);
struct SankeyStyle {
    float nodeWidth = 22, nodePad = 26;
    std::vector<Color> palette;       // per node, in order of first appearance
    float linkAlpha = 0.42f;
    bool labels = true, values = true;
    int decimals = 0;
    float textSize = 28;
    Color textColor{ 220, 224, 232, 255 };
};
// Nodes in columns (sources left, sinks right), ribbons as wide as their flows. Column by
// column, nodes grow and the ribbons pour out of them; u = 0..1.
void sankey(const Ctx& c, const std::vector<Flow>& flows, Rectangle box, const SankeyStyle& s = {}, float u = 1);

struct FunnelStyle {
    std::vector<Color> palette;
    float gap = 8;                    // px between stages
    bool labels = true, percent = true;   // percent of the first stage
    int decimals = 0;
    float textSize = 30;
    Color textColor{ 220, 224, 232, 255 }, valueColor{ 20, 23, 31, 255 };
};
// The first series as stages narrowing down the box; stages drop in top to bottom.
void funnel(const Ctx& c, const Table& t, Rectangle box, const FunnelStyle& s = {}, float u = 1);

struct ChordStyle {
    float arcWidth = 26, padDeg = 3;  // the group arcs round the circle, and the space between them
    std::vector<Color> palette;       // per group, in order of first appearance
    float ribbonAlpha = 0.6f;
    bool labels = true;
    float textSize = 28;
    Color textColor{ 220, 224, 232, 255 };
    float startDeg = -90;
};
// Flows between groups round a circle: each group's arc is as long as everything it sends
// and receives, and each flow is a ribbon as wide as its value at both ends. The arcs sweep
// round, then the ribbons fade in; u = 0..1.
void chord(const Ctx& c, const std::vector<Flow>& flows, Vector2 center, float radius, const ChordStyle& s = {}, float u = 1);

// ---- contours

// Contour lines and filled bands of f(x, y) over the axes' box, built once (marching
// squares on a grid of `cell` px). Levels default to round values inside f's range.
struct ContourMap {
    std::vector<double> levels;
    std::vector<vg::Path> lines;      // one per level
    std::vector<vg::Path> bands;      // levels.size() + 1 regions, lowest first
    Rectangle box{ 0, 0, 0, 0 };      // the axes' box it covers
};
ContourMap contour(const Axes& a, const std::function<double(double, double)>& f, std::vector<double> levels = {}, float cell = 6);
struct ContourStyle {
    std::function<Color(float)> colors = viridis;   // by level, lowest = 0
    bool fill = true;                 // shade the bands
    float fillAlpha = 0.9f;
    float width = 2;
    Color lineColor{ 255, 255, 255, 110 };   // with fill; without, lines take the colour map
    bool labels = true;               // level values on the lines
    int decimals = -1;                // -1: as many as the levels need
    float textSize = 22;
    Color textColor{ 236, 239, 244, 255 };
};
// The lines draw on together, then the bands fade in behind them; u = 0..1.
void contours(const Ctx& c, const ContourMap& m, const ContourStyle& s = {}, float u = 1);

// ---- many dimensions and streams

struct ParallelStyle {
    int colorBy = 0;                  // colour rows by this dimension's value; -1: palette by row
    std::function<Color(float)> colors = viridis;
    std::vector<Color> palette;
    float lineWidth = 2.5f, lineAlpha = 0.55f;
    int highlight = -1;               // a row drawn on top at full strength, the rest dimmed
    Color axisColor{ 200, 204, 214, 255 };
    float textSize = 26;
    Color textColor{ 220, 224, 232, 255 };
};
// Parallel coordinates: each series is a vertical axis with its own range (from the data),
// each category a line across them. The axes rise, then the lines draw left to right.
void parallel(const Ctx& c, const Table& t, Rectangle box, const ParallelStyle& s = {}, float u = 1);

// A stream graph: smooth stacked areas around a centre line (no y axis); the series names
// sit inside their streams where they fit. Draws on left to right; u = 0..1.
void stream(const Ctx& c, const Table& t, const ChartStyle& s = {}, float u = 1);

} } // rmtn::plot
