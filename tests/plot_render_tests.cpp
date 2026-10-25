// plot_render_tests.cpp -- plot:: drawn and read back: axes grow from the origin, bars land
// in their slots with their colours and heights, pies sweep clockwise from 12 o'clock,
// line charts draw on left to right, fields draw arrows, and frames are deterministic.
// Needs OpenGL.
#include "rmtn/raymotion.hpp"
#include <cstdio>
#include <cstdlib>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static Image render(std::function<void(const Ctx&, int)> fn) {
    Comp c;
    c.w = 800; c.h = 500; c.duration = 10; c.background = BLACK;
    addRoot(c.roots, 0, 10)->fn = std::move(fn);
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, 0, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}
static Color px(const Image& img, Vector2 p) { return GetImageColor(img, (int)p.x, (int)p.y); }
static bool lit(Color c) { return c.r + c.g + c.b > 150; }
static bool like(Color c, Color want) { return std::abs(c.r - want.r) < 30 && std::abs(c.g - want.g) < 30 && std::abs(c.b - want.b) < 30; }
static int count(const Image& img, Rectangle r) {
    int n = 0;
    for (int y = (int)r.y; y < (int)(r.y + r.height); y++)
        for (int x = (int)r.x; x < (int)(r.x + r.width); x++) n += lit(GetImageColor(img, x, y));
    return n;
}

static void axesGrow() {
    const plot::Axes a({ 100, 50, 600, 400 }, { -3, 3 }, { -2, 2 });
    Image half = render([a](const Ctx& cx, int) { plot::axes(cx, a, {}, 0.4f); });
    Image full = render([a](const Ctx& cx, int) { plot::axes(cx, a, {}, 1); });
    const Vector2 o = a.map(0, 0);
    CHECK(lit(px(half, { o.x + 20, o.y })) && lit(px(half, { o.x - 20, o.y })));   // grows out of the origin
    CHECK(!lit(px(half, { 110, o.y })) && lit(px(full, { 110, o.y })));
    CHECK(count(full, { 100, 50, 600, 420 }) > count(half, { 100, 50, 600, 420 }) * 1.5);
    UnloadImage(half);
    UnloadImage(full);
}

static void barsAndPies() {
    const plot::Table t = plot::Table::parseCsv("k, v\na, 10\nb, 40\n");
    plot::ChartStyle s;
    s.box = { 100, 60, 600, 360 };
    s.values = false;
    Image img = render([t, s](const Ctx& cx, int) { plot::bars(cx, t, s, 1); });
    const plot::Axes a = plot::chartAxes(t, s);
    const Color c0 = plot::palette(0);
    CHECK(like(px(img, a.map(0.5, 5)), c0) && like(px(img, a.map(1.5, 35)), c0));
    CHECK(!like(px(img, a.map(0.5, 15)), c0));           // the short bar stops at 10
    UnloadImage(img);
    Image none = render([t, s](const Ctx& cx, int) { plot::bars(cx, t, s, 0); });
    CHECK(count(none, { 0, 0, 800, 500 }) == 0);
    UnloadImage(none);
    // pie: quarters, clockwise from 12 o'clock; at u small only the first slice has started
    const plot::Table q = plot::Table::parseCsv("k, v\na, 1\nb, 1\nc, 1\nd, 1\n");
    plot::PieStyle ps;
    ps.labels = false;
    Image p = render([q, ps](const Ctx& cx, int) { plot::pie(cx, q, { 400, 250 }, 200, ps, 1); });
    CHECK(like(px(p, { 500, 150 }), plot::palette(0)) && like(px(p, { 500, 350 }), plot::palette(1)) &&
          like(px(p, { 300, 350 }), plot::palette(2)) && like(px(p, { 300, 150 }), plot::palette(3)));
    UnloadImage(p);
    Image early = render([q, ps](const Ctx& cx, int) { plot::pie(cx, q, { 400, 250 }, 200, ps, 0.05f); });
    CHECK(like(px(early, { 420, 110 }), plot::palette(0)) && !lit(px(early, { 300, 350 })));
    UnloadImage(early);
}

static void linesAndFields() {
    const plot::Table t = plot::Table::parseCsv("k, v\na, 1\nb, 2\nc, 3\nd, 4\n");
    plot::ChartStyle s;
    s.box = { 100, 60, 600, 360 };
    s.legend = false;
    Image half = render([t, s](const Ctx& cx, int) { plot::lines(cx, t, s, 0.5f); });
    const plot::Axes a = plot::chartAxes(t, s);
    CHECK(like(px(half, a.map(0.5, 1)), plot::palette(0)) && !lit(px(half, a.map(3.5, 4))));
    UnloadImage(half);
    const plot::Axes f({ 100, 50, 600, 400 }, { -3, 3 }, { -2, 2 });
    Image fld = render([f](const Ctx& cx, int) { plot::field(cx, f, [](Vector2 p) { return Vector2{ -p.y, p.x }; }); });
    CHECK(count(fld, f.box) > 1500);
    Image again = render([f](const Ctx& cx, int) { plot::field(cx, f, [](Vector2 p) { return Vector2{ -p.y, p.x }; }); });
    bool same = true;
    for (int y = 0; y < 500 && same; y += 3)
        for (int x = 0; x < 800 && same; x += 3) {
            const Color p = GetImageColor(fld, x, y), q = GetImageColor(again, x, y);
            same = p.r == q.r && p.g == q.g && p.b == q.b;
        }
    CHECK(same);
    UnloadImage(fld);
    UnloadImage(again);
}

static void moreCharts() {
    // heatmap: the lowest cell takes the colour map's start, the highest its end
    const plot::Table h = plot::Table::parseCsv("r, a, b\nx, 0, 5\ny, 10, 5\n");
    plot::HeatmapStyle hs;
    hs.box = { 200, 100, 400, 300 };
    hs.values = false;
    hs.colorbar = false;
    hs.gap = 0;
    Image img = render([h, hs](const Ctx& cx, int) { plot::heatmap(cx, h, hs, 1); });
    CHECK(like(px(img, { 300, 175 }), plot::viridis(0)) && like(px(img, { 300, 325 }), plot::viridis(1)));
    CHECK(like(px(img, { 500, 175 }), plot::viridis(0.5f)));
    UnloadImage(img);
    // treemap: tiles cover the box in proportion (3:1 side by side)
    const plot::Tree t = plot::Tree::parse("a, 3\nb, 1\n");
    plot::TreemapStyle ts;
    ts.labels = false;
    ts.gap = 0;
    ts.palette = { RED, BLUE };
    img = render([t, ts](const Ctx& cx, int) { plot::treemap(cx, t, { 100, 100, 400, 200 }, ts, 1); });
    CHECK(like(px(img, { 150, 200 }), RED) && like(px(img, { 380, 200 }), RED) && like(px(img, { 450, 200 }), BLUE));
    UnloadImage(img);
    // waterfall: a fall hangs from the running total
    const plot::Table w = plot::Table::parseCsv("k, v\nup, 40\ndown, -10\n");
    plot::WaterfallStyle ws;
    ws.box = { 100, 60, 600, 360 };
    ws.values = false;
    ws.y = plot::Range(0, 40, 10);
    img = render([w, ws](const Ctx& cx, int) { plot::waterfall(cx, w, ws, 1); });
    const plot::Axes wa({ 100, 60, 600, 360 }, { 0, 3, 1 }, { 0, 40, 10 });
    CHECK(like(px(img, wa.map(1.5, 35)), ws.fall) && !like(px(img, wa.map(1.5, 20)), ws.fall));
    CHECK(like(px(img, wa.map(2.5, 20)), ws.sum) && like(px(img, wa.map(0.5, 20)), ws.rise));
    UnloadImage(img);
    // sankey: two sources pour into one sink; nothing before u starts, ribbons by the end
    const auto flows = plot::parseFlows("a, c, 1\nb, c, 1\n");
    plot::SankeyStyle ss;
    ss.labels = false;
    Image none = render([flows, ss](const Ctx& cx, int) { plot::sankey(cx, flows, { 100, 50, 600, 400 }, ss, 0); });
    Image full = render([flows, ss](const Ctx& cx, int) { plot::sankey(cx, flows, { 100, 50, 600, 400 }, ss, 1); });
    CHECK(count(none, { 0, 0, 800, 500 }) == 0);
    CHECK(count(full, { 300, 50, 200, 400 }) > 20000);   // ribbons across the middle
    UnloadImage(none);
    UnloadImage(full);
    // gauge: the arc fills clockwise from the lower left
    plot::GaugeStyle gs;
    gs.ticks = false;
    gs.fill = GREEN;
    img = render([gs](const Ctx& cx, int) { plot::gauge(cx, 50, { 400, 260 }, 200, gs, 1); });
    CHECK(like(px(img, { 400 - 184, 260 }), GREEN) && !like(px(img, { 400 + 184, 260 }), GREEN));
    UnloadImage(img);
}

static void diagrams() {
    // chord: two groups, each half the circle (first on the right, sweeping clockwise from the top)
    const auto flows = plot::parseFlows("a, b, 1\nb, a, 1\n");
    plot::ChordStyle cs;
    cs.labels = false;
    Image img = render([flows, cs](const Ctx& cx, int) { plot::chord(cx, flows, { 400, 250 }, 180, cs, 1); });
    CHECK(like(px(img, { 400 + 193, 250 }), plot::palette(0)) && like(px(img, { 400 - 193, 250 }), plot::palette(1)));
    CHECK(count(img, { 220, 70, 360, 360 }) > 20000);   // ribbons inside the ring
    UnloadImage(img);
    // contours: the inner band takes the map's start, the outer its end
    const plot::Axes a({ 100, 0, 500, 500 }, { -2.5, 2.5 }, { -2.5, 2.5 });
    const plot::ContourMap m = plot::contour(a, [](double x, double y) { return x * x + y * y; }, { 1, 4 });
    plot::ContourStyle st;
    st.fillAlpha = 1;
    st.labels = false;
    img = render([m, st](const Ctx& cx, int) { plot::contours(cx, m, st, 1); });
    CHECK(like(px(img, { 350, 250 }), plot::viridis(0)) && like(px(img, { 120, 20 }), plot::viridis(1)));
    UnloadImage(img);
    // parallel coordinates: two rows crossing in the middle
    const plot::Table t = plot::Table::parseCsv("k, p, q\nr1, 0, 10\nr2, 10, 0\n");
    plot::ParallelStyle ps;
    ps.lineAlpha = 1;
    ps.lineWidth = 6;
    img = render([t, ps](const Ctx& cx, int) { plot::parallel(cx, t, { 200, 100, 400, 300 }, ps, 1); });
    CHECK(lit(px(img, { 400, 250 })) && !lit(px(img, { 400, 130 })));
    UnloadImage(img);
    // stream: one steady series is a band through the middle
    const plot::Table s1 = plot::Table::parseCsv("k, v\na, 1\nb, 1\nc, 1\n");
    plot::ChartStyle ss;
    ss.box = { 100, 50, 600, 400 };
    img = render([s1, ss](const Ctx& cx, int) { plot::stream(cx, s1, ss, 1); });
    CHECK(like(px(img, { 400, 160 }), plot::palette(0)) && !lit(px(img, { 400, 60 })));
    UnloadImage(img);
}

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(200, 200, "raymotion plots");
    if (!IsWindowReady()) return 77;
    axesGrow();
    barsAndPies();
    linesAndFields();
    moreCharts();
    diagrams();
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d plot rendering check(s) failed\n", failures); return 1; }
    std::printf("plot rendering tests passed\n");
    return 0;
}
