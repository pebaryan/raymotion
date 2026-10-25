// charts.cpp -- "charts": plot::'s statistical and business charts, two to a scene.
//   title          a histogram skyline rises under the title
//   distributions  the same four groups as box plots and as violins
//   histogram      a histogram with its density curve; measurements with error bars and a fit
//   heatmap        traffic by weekday and hour
//   bubbles        three variables at once: x, y and area
//   radar/gauges   two products compared on six axes; three KPI gauges
//   waterfall      from revenue to profit; a sign-up funnel
//   hierarchy      one budget as a sunburst and as a treemap
//   sankey         energy from sources through carriers to uses
//   chord/stream   travel between five regions; listening by genre over a year
//   contour/parallel  a landscape's contour map; thirty cars across five measures
//   areas/candles  stacked areas; a candlestick price chart
// The data is made up for the demo. The score (110 bpm on F-C-Dm-Bb) is synthesised into
// out/ on first use.
//   raymotion render charts
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <cstdint>
#include <cstdio>
#include <filesystem>

namespace rmtn {
namespace {

constexpr int XF = 20;
const std::string SCORE = "out/charts-assets-v1/score.wav";
const Color BG = mkColor(0x14171f), INK = mkColor(0xf4f1ea), MUTED = mkColor(0x8a90a2);
const Color SKY = mkColor(0x58c4dd), SUN = mkColor(0xffd166), CORAL = mkColor(0xfc6255), MINT = mkColor(0x83c167),
            ORCHID = mkColor(0xc38fd8), TEAL = mkColor(0x5cd0b3);

Font bold() { return fonts::ui(true); }
Font regular() { return fonts::ui(false); }
Font mono() {
    static bool missing = false;
    if (!missing) {
        try { return fonts::load({ "consola.ttf", "CascadiaMono.ttf", "DejaVuSansMono.ttf", "Menlo.ttc" }); }
        catch (const std::exception&) { missing = true; }
    }
    return fonts::ui(false);
}

float ramp(int t, double a, double b, Easing e = ease::inOut(ease::cubic)) {
    return (float)interpolate(t, { a, b }, { 0, 1 }, clamped(e));
}

void backdrop(TNode* n, int len) {
    n->add(0, len)->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1920, 1080, BG); };
}

void caption(TNode* n, int len, const char* title, const char* code) {
    TNode* card = n->add(12, len - 12);
    card->up = [len](int t, NodeProps& p) {
        const float in = ramp(t, 0, 18, ease::out(ease::cubic)), out = 1 - ramp(t, len - 40.0, len - 22.0, ease::linear);
        p.opacity = in * out;
        p.pos = { 0, 20 * (1 - in) };
    };
    const std::string tt = title, cc = code;
    card->fn = [tt, cc](const Ctx& cx, int) {
        const float w = std::max(MeasureTextEx(mono(), cc.c_str(), 26, 0).x, MeasureTextEx(bold(), tt.c_str(), 44, 0).x) + 72;
        dr::roundRect(cx, 60, 1080 - 56 - 132, w, 132, 16, mkColor(0x222737, 235));
        dr::text(bold(), cx, tt.c_str(), { 96, 1080 - 56 - 112 }, 44, 0, INK, 0, 0);
        dr::text(mono(), cx, cc.c_str(), { 96, 1080 - 56 - 50 }, 26, 0, SUN, 0, 0);
    };
}

// Deterministic samples: a small LCG and Box-Muller.
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed * 2654435761u + 1) {}
    double uniform() { s = s * 1664525u + 1013904223u; return ((s >> 8) + 0.5) / 16777216.0; }
    double normal(double mean, double sd) {
        const double u1 = uniform(), u2 = uniform();
        return mean + sd * std::sqrt(-2 * std::log(u1)) * std::cos(6.283185307179586 * u2);
    }
};

std::vector<plot::Series> trial() {
    struct G { const char* name; double mean, sd; };
    const G gs[] = { { "Control", 52, 8 }, { "Dose A", 58, 7 }, { "Dose B", 66, 9 }, { "Dose C", 61, 13 } };
    std::vector<plot::Series> out;
    Rng r(7);
    for (const G& g : gs) {
        plot::Series s{ g.name, {}, { 0, 0, 0, 0 } };
        for (int i = 0; i < 140; i++) s.values.push_back(r.normal(g.mean, g.sd));
        out.push_back(s);
    }
    out[0].values.push_back(91);   // a couple of outliers
    out[1].values.push_back(27);
    return out;
}

// ------------------------------------------------------------------ scenes

void titleScene(TNode* n, int len) {
    backdrop(n, len);
    std::vector<double> data;
    Rng r(3);
    for (int i = 0; i < 900; i++) data.push_back(i % 3 ? r.normal(0, 1) : r.normal(2.2, 0.6));
    const plot::Bins b = plot::bin(data, 48, -3.5, 4);
    const plot::Axes ax({ 120, 560, 1680, 380 }, { b.lo, b.hi() }, { 0, b.max() });
    n->add(0, len)->fn = [ax, b](const Ctx& cx, int t) {
        plot::histogram(cx, ax, b, withAlpha(SKY, 0.55f), ramp(t, 0, 60, ease::linear));
        const float in = ramp(t, 20, 42);
        dr::text(bold(), cx, "Charts", { 960, 250 - 20 * (1 - in) }, 140, 0, withAlpha(INK, in), 0.5, 0.5);
        dr::text(regular(), cx, "for science and business: distributions, grids, hierarchies and flows", { 960, 380 }, 40, 0,
                 withAlpha(MUTED, ramp(t, 44, 66)), 0.5, 0.5);
    };
}

void distributionsScene(TNode* n, int len) {
    backdrop(n, len);
    const std::vector<plot::Series> g = trial();
    n->add(0, len)->fn = [g](const Ctx& cx, int t) {
        plot::ChartStyle s;
        s.box = { 180, 130, 720, 600 };
        s.y = plot::Range(20, 100, 20);
        s.palette = { MUTED, SKY, MINT, SUN };
        s.axes.yLabel = R"(\text{score})";
        s.textSize = 28;
        plot::boxplot(cx, g, s, ramp(t, 0, 70, ease::linear));
        s.box.x = 1100;
        s.axes.yLabel.clear();
        plot::violin(cx, g, s, ramp(t, 40, 110, ease::linear));
    };
    caption(n, len, "Box plots and violins", "plot::boxplot(cx, groups, style, u)   plot::violin(cx, groups, style, u)");
}

void histogramScene(TNode* n, int len) {
    backdrop(n, len);
    std::vector<double> data;
    Rng r(11);
    for (int i = 0; i < 600; i++) data.push_back(i % 3 ? r.normal(170, 7) : r.normal(186, 5));
    const plot::Bins b = plot::bin(data);
    const plot::Axes ha({ 170, 140, 760, 580 }, plot::nice(b.lo, b.hi()), { 0, plot::nice(0, b.max() * 1.1).max });
    const plot::Fn f = plot::density(data);
    const double scale = data.size() * b.width;
    const vg::Path curve = plot::graph(ha, [f, scale](double x) { return f(x) * scale; });
    // measurements: y = 2.4 x + 3 with noise, and their uncertainty
    std::vector<Vector2> pts;
    std::vector<double> err;
    Rng q(5);
    for (int i = 1; i <= 8; i++) {
        pts.push_back({ (float)i, (float)(2.4 * i + 3 + q.normal(0, 1.4)) });
        err.push_back(1.2 + 0.25 * i);
    }
    const plot::Axes ea({ 1160, 140, 640, 580 }, { 0, 9, 1 }, { 0, 28, 4 });
    const vg::Path fit = plot::graph(ea, [](double x) { return 2.4 * x + 3; }, 0.3, 8.7);
    n->add(0, len)->fn = [ha, b, curve, ea, pts, err, fit](const Ctx& cx, int t) {
        plot::AxesStyle st;
        st.framed = true;
        st.arrows = false;
        st.xLabel = R"(\text{height (cm)})";
        plot::axes(cx, ha, st, ramp(t, 0, 30));
        plot::histogram(cx, ha, b, withAlpha(SKY, 0.8f), ramp(t, 10, 70, ease::linear));
        vg::stroke(cx, curve.trim(0, ramp(t, 60, 110)), vg::Stroke(SUN, 5));
        st.xLabel = "x";
        st.yLabel = "y";
        plot::axes(cx, ea, st, ramp(t, 20, 50));
        vg::Stroke dashed(withAlpha(MUTED, 0.9f), 3);
        dashed.dash = { 12, 10 };
        vg::stroke(cx, fit.trim(0, ramp(t, 100, 140)), dashed);
        plot::errorBars(cx, ea, pts, err, CORAL, 3, 10, ramp(t, 70, 110));
        plot::scatter(cx, ea, pts, CORAL, 9, ramp(t, 50, 90));
        plot::label(cx, R"(\hat y = 2.4x + 3)", ea.map(4.6, 4), { 0, 0.5f }, 40, withAlpha(INK, ramp(t, 130, 146)));
    };
    caption(n, len, "Histograms, densities and error bars",
            "plot::histogram(cx, ax, plot::bin(data), c, u)   plot::density(data)   plot::errorBars(...)");
}

void heatmapScene(TNode* n, int len) {
    backdrop(n, len);
    plot::Table t;
    t.categories = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
    const double day[] = { 1.0, 1.05, 1.08, 1.1, 0.95, 0.55, 0.45 };
    for (int h = 6; h <= 22; h += 2) {
        plot::Series s{ std::to_string(h) + ":00", {}, { 0, 0, 0, 0 } };
        for (int d = 0; d < 7; d++) {
            const double work = std::exp(-std::pow((h - 9) / 2.2, 2)) + 0.9 * std::exp(-std::pow((h - 17.5) / 2.0, 2));
            const double leisure = 0.6 * std::exp(-std::pow((h - 14) / 4.0, 2));
            const double v = d < 5 ? day[d] * (20 + 80 * work) : 25 + 70 * leisure * day[d] * 2;
            s.values.push_back(std::round(v));
        }
        t.series.push_back(s);
    }
    n->add(0, len)->fn = [t](const Ctx& cx, int f) {
        plot::HeatmapStyle s;
        s.box = { 300, 140, 1240, 600 };
        s.textSize = 28;
        plot::heatmap(cx, t, s, ramp(f, 0, 70, ease::linear));
    };
    caption(n, len, "Heatmaps", "plot::heatmap(cx, table, style, u)   style.colors = plot::viridis");
}

void bubblesScene(TNode* n, int len) {
    backdrop(n, len);
    const std::vector<plot::Bubble> b = {
        { 4, 58, 210, "Ardel", SUN }, { 9, 63, 120, "Brisa", SUN }, { 14, 66, 40, "", SUN },
        { 18, 71, 95, "Calto", MINT }, { 24, 73, 60, "Dorin", MINT }, { 29, 76, 140, "Elmar", MINT },
        { 34, 79, 30, "", SKY }, { 41, 80, 70, "Fenn", SKY }, { 47, 82, 55, "Gaval", SKY }, { 54, 83, 18, "", SKY },
        { 11, 69, 22, "", CORAL }, { 22, 68, 35, "", CORAL }, { 38, 77, 160, "Harrow", ORCHID },
    };
    const plot::Axes ax({ 230, 130, 1460, 600 }, { 0, 60, 10 }, { 40, 90, 10 });
    n->add(0, len)->fn = [ax, b](const Ctx& cx, int t) {
        plot::AxesStyle st;
        st.framed = true;
        st.arrows = false;
        st.yGrid = st.xGrid = true;
        st.xLabel = R"(\text{income per person (k)})";
        st.yLabel = R"(\text{life expectancy})";
        plot::axes(cx, ax, st, ramp(t, 0, 30));
        plot::BubbleStyle bs;
        bs.maxRadius = 90;
        plot::bubbles(cx, ax, b, bs, ramp(t, 20, 100, ease::linear));
    };
    caption(n, len, "Bubble charts", "plot::bubbles(cx, ax, { { x, y, size, \"label\" }, ... }, style, u)");
}

void radarGaugeScene(TNode* n, int len) {
    backdrop(n, len);
    const plot::Table t = plot::Table::parseCsv(
        "metric, Model X, Model Y\nSpeed, 8, 6\nBattery, 5, 9\nCamera, 9, 6\nPrice, 4, 8\nDisplay, 8, 7\nWeight, 6, 8\n");
    n->add(0, len)->fn = [t](const Ctx& cx, int f) {
        plot::RadarStyle rs;
        rs.r = plot::Range(0, 10, 2);
        rs.palette = { SKY, SUN };
        plot::radar(cx, t, { 540, 400 }, 270, rs, ramp(f, 0, 80, ease::linear));
        struct K { const char* title; double v; plot::Range r; const char* unit; Vector2 at; };
        const K ks[] = { { "uptime", 99.2, { 95, 100 }, R"(\%)", { 1260, 290 } }, { "CPU load", 63, { 0, 100 }, R"(\%)", { 1640, 290 } },
                         { "p95 latency", 182, { 0, 500 }, R"(\,\text{ms})", { 1450, 620 } } };
        for (int i = 0; i < 3; i++) {
            plot::GaugeStyle gs;
            gs.range = ks[i].r;
            gs.unit = ks[i].unit;
            gs.title = ks[i].title;
            gs.textSize = 56;
            gs.decimals = i == 0 ? 1 : 0;
            gs.zones = i == 2 ? std::vector<std::pair<double, Color>>{ { 0, MINT }, { 150, SUN }, { 300, CORAL } }
                              : std::vector<std::pair<double, Color>>{ { ks[i].r.min, i == 0 ? CORAL : MINT }, { i == 0 ? 99 : 75, i == 0 ? MINT : SUN } };
            plot::gauge(cx, ks[i].v, ks[i].at, 150, gs, ramp(f, 40 + i * 14, 110 + i * 14));
        }
    };
    caption(n, len, "Radar charts and gauges", "plot::radar(cx, table, center, r, style, u)   plot::gauge(cx, 63, center, r, style, u)");
}

void waterfallScene(TNode* n, int len) {
    backdrop(n, len);
    const plot::Table w = plot::Table::parseCsv("item, change\nRevenue, 120\nCost of sales, -48\nMarketing, -18\nR&D, -22\nOther, 6\nTax, -9\n");
    const plot::Table fu = plot::Table::parseCsv("stage, users\nVisits, 12000\nSign-ups, 5200\nTrials, 2600\nPaid, 1100\nRenewed, 780\n");
    n->add(0, len)->fn = [w, fu](const Ctx& cx, int t) {
        plot::WaterfallStyle s;
        s.box = { 170, 170, 900, 520 };
        s.y = plot::Range(0, 140, 20);
        s.textSize = 26;
        s.totalLabel = "Profit";
        s.axes.yLabel = R"(\text{\$M})";
        plot::waterfall(cx, w, s, ramp(t, 0, 80, ease::linear));
        plot::FunnelStyle fs;
        fs.palette = { SKY, TEAL, MINT, SUN, CORAL };
        plot::funnel(cx, fu, { 1200, 140, 600, 580 }, fs, ramp(t, 50, 120, ease::linear));
    };
    caption(n, len, "Waterfalls and funnels", "plot::waterfall(cx, changes, style, u)   plot::funnel(cx, stages, box, style, u)");
}

void hierarchyScene(TNode* n, int len) {
    backdrop(n, len);
    plot::Tree budget = plot::Tree::parse(
        "Engineering/Platform, 34\nEngineering/Apps, 26\nEngineering/Data, 14\nSales/Field, 22\nSales/Partners, 9\n"
        "Marketing/Brand, 8\nMarketing/Growth, 12\nOperations/Facilities, 10\nOperations/IT, 7\nOperations/Legal, 4\n"
        "Support/Tier 1, 9\nSupport/Tier 2, 5\n");
    budget.name = "Budget";
    n->add(0, len)->fn = [budget](const Ctx& cx, int t) {
        plot::SunburstStyle ss;
        ss.palette = { SKY, CORAL, SUN, MINT, ORCHID };
        ss.inner = 0.3f;
        plot::sunburst(cx, budget, { 500, 420 }, 320, ss, ramp(t, 0, 80));
        plot::TreemapStyle ts;
        ts.palette = ss.palette;
        plot::treemap(cx, budget, { 980, 110, 860, 640 }, ts, ramp(t, 40, 120, ease::linear));
    };
    caption(n, len, "Sunbursts and treemaps", "plot::Tree::parse(\"Sales/Field, 22 ...\")   plot::sunburst(...)   plot::treemap(...)");
}

void sankeyScene(TNode* n, int len) {
    backdrop(n, len);
    const std::vector<plot::Flow> flows = plot::parseFlows(
        "from, to, TWh\nCoal, Electricity, 30\nGas, Electricity, 25\nGas, Heat, 18\nSolar, Electricity, 14\nWind, Electricity, 16\n"
        "Oil, Transport, 34\nElectricity, Homes, 33\nElectricity, Industry, 30\nElectricity, Losses, 22\nHeat, Homes, 12\n"
        "Heat, Industry, 6\nTransport, Services, 12\nTransport, Losses, 22\n");
    n->add(0, len)->fn = [flows](const Ctx& cx, int t) {
        plot::SankeyStyle s;
        s.palette = { mkColor(0x9aa0b0), SKY, CORAL, SUN, MINT, ORCHID, mkColor(0xff8c42), TEAL, SKY, mkColor(0x9aa0b0), mkColor(0xf08fb0), SUN };
        plot::sankey(cx, flows, { 200, 110, 1520, 650 }, s, ramp(t, 0, 110, ease::linear));
    };
    caption(n, len, "Sankey diagrams", "plot::sankey(cx, plot::flowsCsv(\"energy.csv\"), box, style, u)");
}

void chordStreamScene(TNode* n, int len) {
    backdrop(n, len);
    const std::vector<plot::Flow> trips = plot::parseFlows(
        "North, South, 18\nNorth, East, 9\nNorth, West, 6\nSouth, North, 12\nSouth, Centre, 14\nEast, West, 11\n"
        "East, North, 7\nWest, Centre, 10\nWest, South, 5\nCentre, North, 13\nCentre, East, 8\n");
    const plot::Table genres = plot::Table::parseCsv(
        "month, Pop, Rock, Hip-hop, Jazz, Electronic\nJan, 30, 18, 22, 6, 10\nFeb, 32, 17, 24, 7, 12\nMar, 28, 19, 27, 6, 15\n"
        "Apr, 26, 21, 30, 8, 18\nMay, 29, 20, 34, 7, 24\nJun, 35, 18, 33, 9, 30\nJul, 40, 16, 31, 8, 34\nAug, 38, 17, 29, 10, 31\n"
        "Sep, 33, 22, 28, 12, 25\nOct, 31, 24, 30, 11, 20\nNov, 34, 21, 33, 13, 16\nDec, 42, 19, 35, 15, 14\n");
    n->add(0, len)->fn = [trips, genres](const Ctx& cx, int t) {
        plot::ChordStyle cs;
        cs.palette = { SKY, CORAL, SUN, MINT, ORCHID };
        plot::chord(cx, trips, { 470, 420 }, 270, cs, ramp(t, 0, 80, ease::linear));
        plot::ChartStyle ss;
        ss.box = { 960, 130, 880, 600 };
        ss.palette = { CORAL, ORCHID, SUN, TEAL, SKY };
        ss.textSize = 26;
        plot::stream(cx, genres, ss, ramp(t, 30, 110, ease::linear));
    };
    caption(n, len, "Chord diagrams and stream graphs", "plot::chord(cx, flows, center, r, style, u)   plot::stream(cx, table, style, u)");
}

void contourParallelScene(TNode* n, int len) {
    backdrop(n, len);
    const plot::Axes ax({ 170, 130, 760, 600 }, { -3, 3, 1 }, { -2.4, 2.4, 1 });
    const plot::ContourMap map = plot::contour(ax, [](double x, double y) {
        return 3 * std::exp(-((x - 1) * (x - 1) + (y - 0.6) * (y - 0.6)) / 0.9) + 2 * std::exp(-((x + 1.3) * (x + 1.3) + (y + 0.7) * (y + 0.7)) / 0.6)
             - 1.5 * std::exp(-((x + 0.8) * (x + 0.8) + (y - 1.3) * (y - 1.3)) / 0.5) + 0.15 * x;
    }, { -1, -0.5, 0, 0.5, 1, 1.5, 2, 2.5 });
    plot::Table cars;
    cars.series = { { "power (kW)", {}, { 0, 0, 0, 0 } }, { "weight (t)", {}, { 0, 0, 0, 0 } }, { "0-100 (s)", {}, { 0, 0, 0, 0 } },
                    { "range (km)", {}, { 0, 0, 0, 0 } }, { "price (k)", {}, { 0, 0, 0, 0 } } };
    Rng r(17);
    for (int i = 0; i < 30; i++) {
        const double p = 60 + 280 * r.uniform(), w = 1.1 + p / 400 + r.normal(0, 0.12);
        cars.categories.push_back("car " + std::to_string(i + 1));
        const double vals[] = { p, w, std::max(3.0, 14 - p / 32 + r.normal(0, 0.8)), 300 + 400 * r.uniform() - p * 0.3, 18 + p * 0.22 + r.normal(0, 8) };
        for (int j = 0; j < 5; j++) cars.series[(size_t)j].values.push_back(std::round(vals[j] * 10) / 10);
    }
    n->add(0, len)->fn = [ax, map, cars](const Ctx& cx, int t) {
        plot::AxesStyle st;
        st.framed = true;
        st.arrows = false;
        plot::axes(cx, ax, st, ramp(t, 0, 30));
        plot::contours(cx, map, {}, ramp(t, 10, 100, ease::linear));
        plot::ParallelStyle ps;
        ps.colors = [](float v) { return mixC(SKY, CORAL, v); };
        ps.highlight = t > 120 ? 7 : -1;
        plot::parallel(cx, cars, { 1130, 170, 680, 540 }, ps, ramp(t, 40, 120, ease::linear));
    };
    caption(n, len, "Contour maps and parallel coordinates", "plot::contours(cx, plot::contour(ax, f), style, u)   plot::parallel(cx, table, box, style, u)");
}

void areasCandlesScene(TNode* n, int len) {
    backdrop(n, len);
    const plot::Table t = plot::Table::parseCsv(
        "month, Search, Social, Direct\nJan, 20, 12, 8\nFeb, 24, 14, 9\nMar, 23, 18, 10\nApr, 28, 21, 10\nMay, 31, 20, 12\n"
        "Jun, 35, 24, 13\nJul, 33, 29, 15\nAug, 38, 31, 16\n");
    std::vector<plot::Candle> k;
    Rng r(21);
    double p = 100;
    for (int i = 0; i < 36; i++) {
        const double o = p, c = o + r.normal(0.35, 2.4);
        k.push_back({ o, std::max(o, c) + std::fabs(r.normal(0, 1.2)), std::min(o, c) - std::fabs(r.normal(0, 1.2)), c });
        p = c;
    }
    double lo = 1e9, hi = -1e9;
    for (const plot::Candle& x : k) { lo = std::min(lo, x.low); hi = std::max(hi, x.high); }
    const plot::Axes ca({ 1130, 160, 680, 540 }, { 0, 36, 6 }, plot::nice(lo, hi, 0.05));
    n->add(0, len)->fn = [t, k, ca](const Ctx& cx, int f) {
        plot::ChartStyle s;
        s.box = { 180, 170, 780, 530 };
        s.stacked = true;
        s.palette = { SKY, ORCHID, SUN };
        s.textSize = 26;
        plot::areas(cx, t, s, ramp(f, 0, 80, ease::linear));
        plot::AxesStyle st;
        st.framed = true;
        st.arrows = false;
        st.yGrid = true;
        st.ticks = false;
        st.xLabel = R"(\text{day})";
        plot::axes(cx, ca, st, ramp(f, 30, 60));
        plot::candles(cx, ca, k, {}, ramp(f, 50, 130, ease::linear));
    };
    caption(n, len, "Stacked areas and candlesticks", "plot::areas(cx, table, style, u)   plot::candles(cx, ax, ohlc, style, u)");
}

// The score: a pad and arpeggio on F-C-Dm-Bb (110 bpm, a bar is 2.18 s), a bell at each
// chapter and a lift into the Sankey diagram.
void genScore(const std::string& path, double end, const std::vector<double>& chapters) {
    using procgen::Score;
    Score s(end);
    struct Chord { int root; int pad[3]; int arp[4]; };
    const Chord f{ 41, { 60, 65, 69 }, { 72, 77, 81, 77 } }, c{ 48, { 60, 64, 67 }, { 72, 76, 79, 76 } },
                dm{ 50, { 62, 65, 69 }, { 74, 77, 81, 77 } }, bb{ 46, { 58, 62, 65 }, { 70, 74, 77, 74 } };
    const Chord prog[4] = { f, c, dm, bb };
    const double bar = 4 * 60.0 / 110, beat = bar / 4, stop = end - 2.5;
    int i = 0;
    for (double t = 0; t < stop; t += bar, i++) {
        const Chord& ch = prog[i % 4];
        const bool drums = t >= chapters[0], full = t >= chapters[1];
        for (int p : ch.pad) s.pad(t, bar * 0.95, p, 0.08f, full ? 1700 : 1100);
        if (t >= bar)
            for (int e = 0; e < 8; e++) s.pluck(t + e * beat / 2, ch.arp[e % 4], e % 4 == 0 ? 0.06f : 0.035f, e % 2 ? 0.3f : 0.7f);
        for (int e = 0; e < 4; e++) s.bass(t + e * beat, beat * 0.8, ch.root, drums ? 0.24f : 0.16f);
        if (drums) for (int b = 0; b < 4; b++) s.kick(t + b * beat, full ? 0.5f : 0.38f);
        if (full) { s.clap(t + beat, 0.18f); s.clap(t + 3 * beat, 0.18f); }
        if (t >= bar * 2) for (int h = 0; h < 8; h++) s.hat(t + h * beat / 2, h % 2 ? 0.08f : 0.045f, h % 2 ? 0.35f : 0.65f);
    }
    for (size_t k = 0; k < chapters.size(); k++) s.bell(chapters[k], 77 + (int)(k % 2) * 5, 0.09f, 0.5f);
    if (chapters.size() > 7) {
        s.riser(chapters[7] - 1.4, chapters[7], 0.08f);
        s.impact(chapters[7], 0.16f);
    }
    for (int p : f.pad) { s.pad(stop, 2.6, p, 0.12f, 2000, 0.35f); s.pad(stop, 2.6, p - 12, 0.08f, 1200, 0.65f); }
    s.bass(stop, 2.6, f.root - 12, 0.24f);
    for (int k = 0; k < 4; k++) s.bell(stop + k * 0.4, f.arp[k] + 12, 0.08f, 0.2f + 0.2f * k);
    media::writeWav(path, s.mix(), Score::RATE);
}

} // namespace

void registerCharts(Registry& out) {
    Comp c;
    c.name = "charts";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    struct S { int len; void (*make)(TNode*, int); };
    const S scenes[] = { { 110, titleScene }, { 170, distributionsScene }, { 180, histogramScene }, { 150, heatmapScene },
                         { 150, bubblesScene }, { 170, radarGaugeScene }, { 170, waterfallScene }, { 170, hierarchyScene },
                         { 170, sankeyScene }, { 170, chordStreamScene },
                         { 170, contourParallelScene }, { 180, areasCandlesScene } };
    std::vector<double> chapters;
    bool first = true;
    for (const S& s : scenes) {
        if (!first) { ts.transition(Transition::fade(XF)); chapters.push_back((ts.end() - XF) / 30.0); }
        first = false;
        s.make(ts.add(s.len), s.len);
    }
    c.duration = ts.end();
    const double seconds = c.duration / 30.0;
    c.assets = [seconds, chapters](const Comp&, const Json&) {
        if (std::filesystem::exists(SCORE)) return;
        std::filesystem::create_directories(std::filesystem::path(SCORE).parent_path());
        genScore(SCORE, seconds, chapters);
    };
    media::audio(addRoot(c.roots, 0, c.duration), SCORE);
    out.push_back(std::move(c));
}

} // namespace rmtn
