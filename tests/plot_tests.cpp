// plot_tests.cpp -- plot:: without a GPU: tick ranges, data <-> screen mapping, function
// graphs (breaks at poles and holes, clipping), parametric and polar curves, areas,
// Riemann sums, tangents, CSV tables and chart ranges.
#include "rmtn/plot.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static bool near(double a, double b, double eps = 1e-3) { return std::fabs(a - b) <= eps; }
static bool inside(const vg::Path& p, Rectangle r, float slack = 1.5f) {
    for (const auto& pl : p.flatten())
        for (Vector2 q : pl.pts)
            if (q.x < r.x - slack || q.x > r.x + r.width + slack || q.y < r.y - slack || q.y > r.y + r.height + slack) return false;
    return true;
}

static void ranges() {
    CHECK(near(plot::Range(0, 10).tick(), 1));
    CHECK(near(plot::Range(-3, 3).tick(), 1));
    CHECK(near(plot::Range(0, 45).tick(), 5));
    CHECK(near(plot::Range(0, 1).tick(), 0.1));
    CHECK(near(plot::Range(0, 2000).tick(), 200));
    CHECK(near(plot::Range(0, 1, 0.25).tick(), 0.25));
    const auto t = plot::Range(-2.5, 2.5, 1).ticks();
    CHECK(t.size() == 5 && near(t.front(), -2) && near(t.back(), 2) && t[2] == 0.0);
    CHECK(plot::Range(0, 1, 0.1).ticks().size() == 11);   // both ends, despite rounding
}

static void mapping() {
    const plot::Axes a({ 100, 100, 800, 400 }, { -4, 4 }, { -2, 2 });
    const Vector2 o = a.map(0, 0), tr = a.map(4, 2), bl = a.map(-4, -2);
    CHECK(near(o.x, 500) && near(o.y, 300));
    CHECK(near(tr.x, 900) && near(tr.y, 100));                // y up in data, down on screen
    CHECK(near(bl.x, 100) && near(bl.y, 500));
    const Vector2 back = a.unmap(a.map(1.5, -0.75));
    CHECK(near(back.x, 1.5) && near(back.y, -0.75));
    CHECK(near(a.sx(), 100) && near(a.sy(), 100));
    const plot::Axes off({ 0, 0, 100, 100 }, { 2, 6 }, { 1, 3 });
    CHECK(near(off.origin().x, 0) && near(off.origin().y, 100));   // origin clamped into the ranges
    const plot::Axes nl = plot::numberLine({ 50, 300 }, 1000, { 0, 10 });
    CHECK(near(nl.map(5, 123).x, 550) && near(nl.map(5, 123).y, 300));
}

static void graphs() {
    const plot::Axes a({ 0, 0, 1000, 600 }, { -6.5, 6.5 }, { -3, 3 });
    // a smooth curve is one contour that follows the function
    const vg::Path s = plot::graph(a, [](double x) { return std::sin(x); });
    CHECK(s.contours.size() == 1 && inside(s, a.box));
    for (float u : { 0.1f, 0.5f, 0.9f }) {
        const Vector2 p = s.pointAt(u), d = a.unmap(p);
        CHECK(std::fabs(d.y - std::sin(d.x)) < 0.01f);
    }
    // tan x breaks at each pole in range (four poles -> five branches) and stays in the box
    const vg::Path t = plot::graph(a, [](double x) { return std::tan(x); });
    CHECK(t.contours.size() == 5 && inside(t, a.box));
    // 1/x: two branches, nothing drawn across the pole
    const vg::Path r = plot::graph(a, [](double x) { return 1 / x; });
    CHECK(r.contours.size() == 2);
    // sqrt starts at the edge of its domain
    const vg::Path q = plot::graph(a, [](double x) { return std::sqrt(x); });
    CHECK(q.contours.size() == 1 && a.unmap(q.contours[0].start).x < 0.01f);
    // a sub-interval
    const Rectangle sub = plot::graph(a, [](double x) { return x / 4; }, -1, 2).bounds();
    CHECK(near(a.unmap({ sub.x, 0 }).x, -1, 0.02) && near(a.unmap({ sub.x + sub.width, 0 }).x, 2, 0.02));
    // parametric circle and polar rose stay put
    const vg::Path circ = plot::parametric(a, [](double u) { return Vector2{ (float)std::cos(u), (float)std::sin(u) }; }, 0, 6.2832);
    const Rectangle cb = circ.bounds();
    CHECK(near(cb.width, 2 * a.sx(), 2) && near(cb.height, 2 * a.sy(), 2));
    CHECK(inside(plot::parametric(a, [](double u) { return Vector2{ (float)(u - 10), (float)u }; }, 0, 20), a.box));   // clipped
    const Rectangle rb = plot::polar(a, [](double) { return 2.0; }).bounds();
    CHECK(near(rb.width, 4 * a.sx(), 2));
    // area: between sin and the axis over [0, pi]
    const Rectangle ab = plot::area(a, [](double x) { return std::sin(x); }, 0, 3.14159).bounds();
    CHECK(near(ab.x, a.map(0, 0).x, 1) && near(ab.y + ab.height, a.map(0, 0).y, 1) && near(ab.y, a.map(0, 1).y, 1));
    // Riemann: n rectangles, left sums of x^2 sample the left edges
    const auto rects = plot::riemann(a, [](double x) { return x * x / 10; }, 0, 4, 8);
    CHECK(rects.size() == 8);
    CHECK(near(rects[2].bounds().height, (1.0 * 1.0 / 10) * a.sy(), 0.5));
    // tangent to x^2/4 at 2 has slope 1
    const vg::Path tg = plot::tangent(a, [](double x) { return x * x / 4; }, 2, 200);
    const Vector2 p0 = a.unmap(tg.contours[0].start), p1 = a.unmap(tg.contours[0].segs.back().to);
    CHECK(near((p1.y - p0.y) / (p1.x - p0.x), 1, 0.01));
}

static void tables() {
    const plot::Table t = plot::Table::parseCsv("year, a, \"b, c\"\n2020, 1, 2\n\n2021, 3.5, -4\n2022, , 6\n");
    CHECK(t.categories.size() == 3 && t.categories[1] == "2021");
    CHECK(t.series.size() == 2 && t.series[1].name == "b, c");
    CHECK(near(t.series[0].values[1], 3.5) && near(t.series[1].values[1], -4) && near(t.series[0].values[2], 0));
    bool threw = false;
    try { plot::Table::parseCsv("x, y\nrow, banana\n"); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    const plot::Table m = plot::Table::mix(t, plot::Table::parseCsv("year, a, b\nA, 3, 2\nB, 5.5, 0\nC, 2, 6\n"), 0.5f);
    CHECK(near(m.series[0].values[0], 2) && near(m.series[1].values[1], -2) && m.categories[0] == "A");
    // chart ranges: from zero to a round number above the data, or below for negatives
    plot::ChartStyle s;
    const plot::Axes ax = plot::chartAxes(plot::Table::parseCsv("k, v\na, 12\nb, 43\n"), s);
    CHECK(near(ax.y.min, 0) && near(ax.y.max, 45) && near(ax.x.max, 2));
    const plot::Axes neg = plot::chartAxes(t, s);
    CHECK(neg.y.min < -3.9 && neg.y.max >= 6);
    s.stacked = true;
    CHECK(plot::chartAxes(plot::Table::parseCsv("k, v, w\na, 12, 30\nb, 43, 10\n"), s).y.max >= 53);
}

static void statistics() {
    const plot::Range r = plot::nice(3.2, 47.9);
    CHECK(near(r.min, 0) && near(r.max, 50));
    const plot::Range p = plot::nice(0, 10, 0.1);   // padded, then rounded out
    CHECK(p.min < 0 && p.max > 10);
    // quartiles interpolate; 100 lies beyond 1.5 IQR
    const plot::Summary s = plot::summarize({ 1, 2, 3, 4, 5, 6, 7, 8, 9, 100, NAN });
    CHECK(near(s.median, 5.5) && near(s.q1, 3.25) && near(s.q3, 7.75));
    CHECK(near(s.lo, 1) && near(s.hi, 9) && s.outliers.size() == 1 && near(s.outliers[0], 100));
    // a density integrates to 1 and peaks near the data
    const plot::Fn f = plot::density({ -1, 0, 0, 0.5, 1, 2 });
    double area = 0;
    for (double x = -10; x < 10; x += 0.01) area += f(x) * 0.01;
    CHECK(near(area, 1, 0.01) && f(0.2) > f(3));
    // bins: everything counted once, including the maximum
    std::vector<double> d;
    for (int i = 0; i <= 100; i++) d.push_back(i * 0.37);
    const plot::Bins b = plot::bin(d);
    double total = 0;
    for (double c : b.counts) total += c;
    CHECK(total == 101 && b.lo <= 0 && b.hi() >= 37);
    const plot::Bins e = plot::bin({ 0, 1, 2, 3, 4 }, 2, 0, 4);
    CHECK(e.counts.size() == 2 && e.counts[0] == 2 && e.counts[1] == 3 && near(e.max(), 3));
}

static void hierarchies() {
    const plot::Tree t = plot::Tree::parse("path, value\nA/x, 2\nA/y, 3\nB, 4\nA/x, 1\n");
    CHECK(t.children.size() == 2 && t.children[0].name == "A" && t.children[0].children.size() == 2);
    CHECK(near(t.children[0].children[0].value, 3) && near(t.children[0].total(), 6) && near(t.total(), 10));
    bool threw = false;
    try { plot::Tree::parse("A, 1\nB, lots\n"); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    const auto f = plot::parseFlows("from, to, amount\nCoal, Power, 3\n\"A, b\", Power, 2.5\n");
    CHECK(f.size() == 2 && f[1].from == "A, b" && near(f[1].value, 2.5));
    threw = false;
    try { plot::parseFlows("a, b, 1\na, b\n"); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    // colour maps run between their ends
    const Color v0 = plot::viridis(0), v1 = plot::viridis(1), m = plot::diverging(0.5f);
    CHECK(v0.r == 0x44 && v0.b == 0x54 && v1.r == 0xfd && v1.g == 0xe7);
    CHECK(m.r > 200 && m.g > 190 && m.b > 190);
}

static void contourMaps() {
    const plot::Axes a({ 0, 0, 600, 600 }, { -3, 3 }, { -3, 3 });
    const auto bowl = [](double x, double y) { return x * x + y * y; };
    const plot::ContourMap m = plot::contour(a, bowl, { 1, 4 });
    CHECK(m.lines.size() == 2 && m.bands.size() == 3);
    // each level is one closed ring of the right length (radius 1 and 2 at 100 px per unit)
    CHECK(m.lines[0].contours.size() == 1 && m.lines[0].contours[0].closed);
    CHECK(near(m.lines[0].length(), 628.3, 8) && near(m.lines[1].length(), 1256.6, 12));
    const Rectangle inner = m.bands[0].bounds();
    CHECK(near(inner.width, 200, 4) && near(inner.x + inner.width / 2, 300, 2));
    CHECK(near(m.bands[2].bounds().width, 600, 1));   // the outer band reaches the box edges
    // automatic levels: round values strictly inside the range (0 to 18)
    const plot::ContourMap autoLv = plot::contour(a, bowl);
    CHECK(!autoLv.levels.empty() && autoLv.levels.front() > 0 && autoLv.levels.back() < 18);
    CHECK(autoLv.lines.size() == autoLv.levels.size());
}

int main() {
    ranges();
    contourMaps();
    mapping();
    graphs();
    tables();
    statistics();
    hierarchies();
    if (failures) { std::fprintf(stderr, "%d plot check(s) failed\n", failures); return 1; }
    std::printf("plot tests passed\n");
    return 0;
}
