// plots.cpp -- "plots": the plot:: module, one idea per scene.
//   title       a sine wave draws itself across growing axes
//   graphs      sin and cos with labels; a tangent rides the curve, its slope read out
//   integral    Riemann rectangles refine (4, 8, 16, 32) and settle into the area
//   curves      a Lissajous figure and a polar rose, parametric and polar
//   field       a vector field grows; a trajectory spirals through it
//   number line a point walks the line, its value typeset as it goes
//   bars        energy by source, growing in, then updating to the next dataset
//   lines/pie   the same data as lines, then the latest year's mix as a donut
// The score (bright 120 bpm on C-G-Am-F) is synthesised into out/ on first use.
//   raymotion render plots
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <cstdio>
#include <filesystem>

namespace rmtn {
namespace {

constexpr int XF = 20;
const std::string SCORE = "out/plots-assets-v1/score.wav";
const Color BG = mkColor(0x14171f), INK = mkColor(0xf4f1ea), MUTED = mkColor(0x8a90a2);
const Color SKY = mkColor(0x58c4dd), SUN = mkColor(0xffd166), CORAL = mkColor(0xfc6255), MINT = mkColor(0x83c167),
            ORCHID = mkColor(0xc38fd8);

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

std::string fixed(double v, int decimals) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
    std::string s = buf;
    return s == "-0.00" || s == "-0.0" ? s.substr(1) : s;
}

// The upper area of the frame, above the caption card.
const Rectangle STAGE{ 220, 90, 1480, 690 };

// ------------------------------------------------------------------ scenes

void titleScene(TNode* n, int len) {
    backdrop(n, len);
    const plot::Axes ax({ 160, 380, 1600, 420 }, { 0, 12.566 }, { -1.3, 1.3 });
    const vg::Path wave = plot::graph(ax, [](double x) { return std::sin(x) * std::exp(-x / 14); });
    n->add(0, len)->fn = [ax, wave](const Ctx& cx, int t) {
        plot::AxesStyle st;
        st.xNumbers = st.yNumbers = false;
        st.ticks = false;
        st.color = withAlpha(MUTED, 0.7f);
        plot::axes(cx, ax, st, ramp(t, 0, 30));
        vg::stroke(cx, wave.trim(0, ramp(t, 14, 76)), vg::Stroke(SKY, 7));
        const float in = ramp(t, 24, 46);
        dr::text(bold(), cx, "Plots and charts", { 960, 220 - 20 * (1 - in) }, 120, 0, withAlpha(INK, in), 0.5, 0.5);
        dr::text(regular(), cx, "axes, graphs, fields, number lines and animated charts", { 960, 900 }, 40, 0,
                 withAlpha(MUTED, ramp(t, 50, 72)), 0.5, 0.5);
    };
}

void graphsScene(TNode* n, int len) {
    backdrop(n, len);
    const plot::Axes ax(STAGE, { -7, 7 }, { -1.6, 1.6 });
    auto f = [](double x) { return std::sin(x); };
    const vg::Path s = plot::graph(ax, f), c = plot::graph(ax, [](double x) { return std::cos(x); });
    n->add(0, len)->fn = [ax, s, c, f](const Ctx& cx, int t) {
        plot::AxesStyle st;
        st.xLabel = "x";
        st.yLabel = "y";
        st.xGrid = st.yGrid = true;
        plot::axes(cx, ax, st, ramp(t, 0, 30));
        vg::stroke(cx, s.trim(0, ramp(t, 20, 60)), vg::Stroke(SKY, 5));
        vg::stroke(cx, c.trim(0, ramp(t, 34, 74)), vg::Stroke(CORAL, 5));
        plot::label(cx, R"(y = \sin x)", ax.map(-6.6, 1.35), { 0, 0.5f }, 46, withAlpha(SKY, ramp(t, 50, 64)));
        plot::label(cx, R"(y = \cos x)", ax.map(3.7, 1.35), { 0, 0.5f }, 46, withAlpha(CORAL, ramp(t, 64, 78)));
        // a tangent rides sin x
        const float k = ramp(t, 84, 84 + 120.0);
        if (t < 80) return;
        const float fade = ramp(t, 80, 92);
        const double x = -5.5 + 11 * k;
        vg::Stroke ts(withAlpha(SUN, fade), 4);
        vg::stroke(cx, plot::tangent(ax, f, x, 420), ts);
        vg::fill(cx, vg::Path::circle(ax.map(x, f(x)), 11), withAlpha(SUN, fade));
        plot::label(cx, R"(\text{slope} = )" + fixed(std::cos(x), 2), ax.map(x, f(x) + 0.42), { 0.5f, 1 }, 36, withAlpha(SUN, fade));
    };
    caption(n, len, "Graphs of functions", "plot::graph(axes, [](double x) { return std::sin(x); })  +  plot::tangent(...)");
}

void integralScene(TNode* n, int len) {
    backdrop(n, len);
    const plot::Axes ax(STAGE, { 0, 4.2 }, { 0, 4.5 });
    auto f = [](double x) { return 0.25 * x * x * x - 1.5 * x * x + 2.4 * x + 1.2; };
    const vg::Path g = plot::graph(ax, f), area = plot::area(ax, f, 0.5, 3.8);
    std::vector<std::vector<vg::Path>> sums;
    for (int k : { 4, 8, 16, 32 }) sums.push_back(plot::riemann(ax, f, 0.5, 3.8, k, 0.5f));
    n->add(0, len)->fn = [ax, g, area, sums](const Ctx& cx, int t) {
        plot::AxesStyle st;
        st.xLabel = "x";
        plot::axes(cx, ax, st, ramp(t, 0, 26));
        // the sums: each stage fades in, holds, and gives way to the next; then the exact area
        for (size_t i = 0; i < sums.size(); i++) {
            const int at = 30 + (int)i * 28;
            const float in = ramp(t, at, at + 12), out = i + 1 < sums.size() ? ramp(t, at + 28, at + 38) : ramp(t, 160, 176);
            const float a = in * (1 - out);
            if (a <= 0) continue;
            for (const vg::Path& r : sums[i]) {
                vg::Style s = vg::Style::filled(withAlpha(MINT, 0.45f * a));
                s.stroke = vg::Stroke(withAlpha(MINT, a), 2);
                vg::draw(cx, r, s);
            }
            plot::label(cx, "n = " + std::to_string(4 << i), { STAGE.x + STAGE.width - 20, STAGE.y + 30 }, { 1, 0.5f }, 44,
                        withAlpha(MINT, a));
        }
        vg::fill(cx, area, withAlpha(SKY, 0.4f * ramp(t, 160, 180)));
        vg::stroke(cx, g.trim(0, ramp(t, 10, 40)), vg::Stroke(SUN, 5));
        plot::label(cx, R"(\int_{0.5}^{3.8} f(x)\,dx)", { STAGE.x + STAGE.width - 20, STAGE.y + 40 }, { 1, 0.5f }, 52,
                    withAlpha(SKY, ramp(t, 166, 184)));
    };
    caption(n, len, "Riemann sums and areas", "plot::riemann(axes, f, a, b, n, 0.5f)  ->  plot::area(axes, f, a, b)");
}

void curvesScene(TNode* n, int len) {
    backdrop(n, len);
    const plot::Axes left({ 160, 110, 760, 640 }, { -1.25, 1.25 }, { -1.05, 1.05 });
    const plot::Axes right({ 1000, 110, 760, 640 }, { -1.25, 1.25 }, { -1.05, 1.05 });
    const vg::Path lissajous = plot::parametric(left, [](double t) { return Vector2{ (float)std::sin(3 * t), (float)std::sin(4 * t) }; }, 0, 6.2832, 900);
    const vg::Path rose = plot::polar(right, [](double t) { return std::cos(4 * t); }, 0, 6.2832, 900);
    n->add(0, len)->fn = [left, right, lissajous, rose](const Ctx& cx, int t) {
        plot::AxesStyle st;
        st.xNumbers = st.yNumbers = false;
        st.color = withAlpha(MUTED, 0.6f);
        plot::axes(cx, left, st, ramp(t, 0, 24));
        plot::axes(cx, right, st, ramp(t, 6, 30));
        const float a = ramp(t, 20, 110, ease::inOut(ease::quad)), b = ramp(t, 30, 120, ease::inOut(ease::quad));
        vg::stroke(cx, lissajous.trim(0, a), vg::Stroke(ORCHID, 5));
        vg::stroke(cx, rose.trim(0, b), vg::Stroke(SUN, 5));
        if (a > 0 && a < 1) vg::fill(cx, vg::Path::circle(lissajous.pointAt(a), 10), INK);
        if (b > 0 && b < 1) vg::fill(cx, vg::Path::circle(rose.pointAt(b), 10), INK);
        plot::label(cx, R"((\sin 3t,\ \sin 4t))", { 540, 70 }, { 0.5f, 0.5f }, 44, withAlpha(ORCHID, ramp(t, 30, 46)));
        plot::label(cx, R"(r = \cos 4\theta)", { 1380, 70 }, { 0.5f, 0.5f }, 44, withAlpha(SUN, ramp(t, 40, 56)));
    };
    caption(n, len, "Parametric and polar curves", "plot::parametric(axes, [](double t) { ... }, 0, 2*PI)   plot::polar(axes, r, 0, 2*PI)");
}

void fieldScene(TNode* n, int len) {
    backdrop(n, len);
    const plot::Axes ax(STAGE, { -4, 4 }, { -2, 2 });
    auto v = [](Vector2 p) { return Vector2{ p.y, -p.x - 0.35f * p.y }; };   // a damped oscillator
    // a trajectory, integrated once while registering
    std::vector<Vector2> traj;
    Vector2 p{ -2.4f, 0 };                       // its orbit stays inside the y range
    for (int i = 0; i < 2400; i++) {
        traj.push_back(p);
        const float h = 0.01f;
        const Vector2 k1 = v(p), k2 = v({ p.x + h / 2 * k1.x, p.y + h / 2 * k1.y }), k3 = v({ p.x + h / 2 * k2.x, p.y + h / 2 * k2.y }),
                      k4 = v({ p.x + h * k3.x, p.y + h * k3.y });
        p = { p.x + h / 6 * (k1.x + 2 * k2.x + 2 * k3.x + k4.x), p.y + h / 6 * (k1.y + 2 * k2.y + 2 * k3.y + k4.y) };
    }
    std::vector<Vector2> screen;
    for (Vector2 q : traj) screen.push_back(ax.map(q));
    const vg::Path path = vg::Path::polyline(screen);
    n->add(0, len)->fn = [ax, v, path](const Ctx& cx, int t) {
        plot::AxesStyle st;
        st.color = withAlpha(MUTED, 0.6f);
        st.numberColor = withAlpha(MUTED, 0.8f);
        plot::axes(cx, ax, st, ramp(t, 0, 24));
        plot::field(cx, ax, v, {}, ramp(t, 10, 50));
        const float u = ramp(t, 50, 150, ease::linear);
        vg::stroke(cx, path.trim(0, u), vg::Stroke(SUN, 5));
        if (u > 0) vg::fill(cx, vg::Path::circle(path.pointAt(u), 12), INK);
    };
    caption(n, len, "Vector fields", "plot::field(axes, [](Vector2 p) { return Vector2{ p.y, -p.x - 0.35f * p.y }; })");
}

void numberLineScene(TNode* n, int len) {
    backdrop(n, len);
    const plot::Axes line = plot::numberLine({ 260, 420 }, 1400, { -5, 5, 1 });
    const plot::Axes unit = plot::numberLine({ 260, 640 }, 1400, { 0, 1, 0.25 });
    n->add(0, len)->fn = [line, unit](const Ctx& cx, int t) {
        plot::AxesStyle st;
        st.numberSize = 34;
        plot::axes(cx, line, st, ramp(t, 0, 30));
        plot::AxesStyle us;
        us.arrows = false;
        us.numberSize = 30;
        plot::axes(cx, unit, us, ramp(t, 10, 40));
        // a point walks -3 -> 2.5 -> -1.25, its value following it
        const double x = -3 + 5.5 * ramp(t, 40, 80) - 3.75 * ramp(t, 96, 130);
        const float a = ramp(t, 30, 40);
        const Vector2 p = line.map(x, 0);
        vg::fill(cx, vg::Path::circle(p, 14), withAlpha(SUN, a));
        plot::label(cx, "x = " + fixed(x, 2), { p.x, p.y - 34 }, { 0.5f, 1 }, 42, withAlpha(SUN, a));
        const double q = ramp(t, 50, 120);
        const Vector2 pq = unit.map(q, 0);
        vg::stroke(cx, vg::Path::line(unit.map(0, 0), pq), vg::Stroke(withAlpha(SKY, a), 8));
        plot::label(cx, fixed(q, 2), { pq.x, pq.y - 30 }, { 0.5f, 1 }, 40, withAlpha(SKY, a));
    };
    caption(n, len, "Number lines", "plot::numberLine({ 260, 420 }, 1400, { -5, 5, 1 })");
}

plot::Table energy(int year) {
    // Illustrative numbers (TWh), not a real dataset.
    if (year == 0) return plot::Table::parseCsv("source, 2019\nSolar, 12\nWind, 30\nHydro, 41\nNuclear, 28\nGas, 52\n");
    return plot::Table::parseCsv("source, 2023\nSolar, 45\nWind, 44\nHydro, 41\nNuclear, 25\nGas, 31\n");
}

void barsScene(TNode* n, int len) {
    backdrop(n, len);
    const plot::Table a = energy(0), b = energy(1);
    n->add(0, len)->fn = [a, b](const Ctx& cx, int t) {
        plot::ChartStyle s;
        s.box = { 260, 150, 1400, 600 };
        s.y = plot::Range(0, 60, 10);
        s.axes.yLabel = R"(\text{TWh})";
        const float w = ramp(t, 100, 140);
        plot::Table now = plot::Table::mix(a, b, w);
        now.series[0].color = mixC(SKY, SUN, w);
        plot::bars(cx, now, s, ramp(t, 0, 60, ease::linear));
        plot::label(cx, w < 0.5f ? R"(\text{2019})" : R"(\text{2023})", { 1660, 110 }, { 1, 0.5f }, 52,
                    withAlpha(INK, std::fabs(w - 0.5f) * 2 * ramp(t, 20, 36)));
    };
    caption(n, len, "Bar charts from data", "plot::bars(cx, plot::Table::mix(y2019, y2023, w), style, u)");
}

void linesPieScene(TNode* n, int len) {
    backdrop(n, len);
    const plot::Table t = plot::Table::parseCsv(
        "year, Solar, Wind, Gas\n2019, 12, 30, 52\n2020, 18, 34, 47\n2021, 25, 37, 42\n2022, 33, 41, 36\n2023, 45, 44, 31\n");
    const plot::Table mix = energy(1);
    n->add(0, len)->fn = [t, mix](const Ctx& cx, int f) {
        plot::ChartStyle s;
        s.box = { 200, 160, 900, 580 };
        s.palette = { SKY, MINT, CORAL };
        s.textSize = 28;
        plot::lines(cx, t, s, ramp(f, 0, 70, ease::linear));
        plot::PieStyle ps;
        ps.inner = 0.55f;
        ps.palette = { SKY, MINT, mkColor(0x5cd0b3), ORCHID, CORAL };
        ps.explode = 0;
        ps.textSize = 30;
        plot::pie(cx, mix, { 1480, 450 }, 230, ps, ramp(f, 60, 120));
        plot::label(cx, R"(\text{2023})", { 1480, 450 }, { 0.5f, 0.5f }, 52, withAlpha(INK, ramp(f, 110, 126)));
    };
    caption(n, len, "Line and pie charts", "plot::lines(cx, table, style, u)   plot::pie(cx, table, center, r, donut, u)");
}

// The score, in seconds: scenes start at 3.0 (graphs), 8.67 (integral), 14.33 (curves),
// 19.33 (field), 24.0 (number line), 28.33 (bars) and 34.33 (lines and pie).
void genScore(const std::string& path, double end) {
    using procgen::Score;
    Score s(end);
    struct Chord { int root; int pad[3]; int arp[4]; };
    const Chord c{ 48, { 60, 64, 67 }, { 72, 76, 79, 76 } }, g{ 43, { 59, 62, 67 }, { 71, 74, 79, 74 } },
                am{ 45, { 60, 64, 69 }, { 72, 76, 81, 76 } }, f{ 41, { 60, 65, 69 }, { 72, 77, 81, 77 } };
    const Chord prog[4] = { c, g, am, f };
    const double stop = end - 2.5;
    for (double t = 0; t < stop; t += 2) {
        const Chord& ch = prog[(int)(t / 2) % 4];
        const bool beat = t >= 8, full = t >= 14 && !(t >= 24 && t < 28);
        for (int p : ch.pad) s.pad(t, 1.9, p, 0.08f, full ? 1700 : 1100);
        if (t >= 2)
            for (int e = 0; e < 8; e++) s.pluck(t + e * 0.25, ch.arp[e % 4], e % 4 == 0 ? 0.065f : 0.04f, e % 2 ? 0.3f : 0.7f);
        for (int e = 0; e < 4; e++) s.bass(t + e * 0.5, 0.4, ch.root, beat ? 0.24f : 0.16f);
        if (beat) for (int b = 0; b < 4; b++) s.kick(t + b * 0.5, full ? 0.55f : 0.4f);
        if (full) { s.clap(t + 0.5, 0.2f); s.clap(t + 1.5, 0.2f); }
        if (t >= 4) for (int h = 0; h < 8; h++) s.hat(t + h * 0.25, h % 2 ? 0.09f : 0.05f, h % 2 ? 0.35f : 0.65f);
    }
    const double chapters[] = { 3.0, 8.67, 14.33, 19.33, 24.0, 28.33, 34.33 };
    for (int i = 0; i < 7; i++) s.bell(chapters[i], 79 + (i % 2) * 5, 0.09f, 0.5f);
    const int climb[8] = { 72, 74, 76, 79, 81, 84, 86, 88 };
    // Riemann refinements, then the area.
    for (int i = 0; i < 4; i++) s.bell(8.67 + 1.0 + i * 0.93, climb[i * 2], 0.07f, 0.3f + 0.13f * i);
    s.riser(13.0, 14.0, 0.08f);
    // the number-line walk and the bar update
    s.riser(25.0, 26.3, 0.07f);
    s.riser(30.6, 32.0, 0.09f);
    s.impact(32.0, 0.18f);
    // the donut sweep
    for (int i = 0; i < 5; i++) s.bell(36.3 + i * 0.35, climb[i + 2] + 7, 0.06f, 0.2f + 0.15f * i);
    // landing on C
    for (int p : c.pad) { s.pad(stop, 2.6, p, 0.12f, 2000, 0.35f); s.pad(stop, 2.6, p - 12, 0.08f, 1200, 0.65f); }
    s.bass(stop, 2.6, c.root - 12, 0.24f);
    for (int i = 0; i < 4; i++) s.bell(stop + i * 0.4, c.arp[i] + 12, 0.08f, 0.2f + 0.2f * i);
    media::writeWav(path, s.mix(), Score::RATE);           // (mix fades the last 3 s)
}

} // namespace

void registerPlots(Registry& out) {
    Comp c;
    c.name = "plots";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    struct S { int len; void (*make)(TNode*, int); };
    const S scenes[] = { { 110, titleScene }, { 190, graphsScene }, { 190, integralScene }, { 170, curvesScene },
                         { 160, fieldScene }, { 150, numberLineScene }, { 200, barsScene }, { 190, linesPieScene } };
    bool first = true;
    for (const S& s : scenes) {
        if (!first) ts.transition(Transition::fade(XF));
        first = false;
        s.make(ts.add(s.len), s.len);
    }
    c.duration = ts.end();
    const double seconds = c.duration / 30.0;
    c.assets = [seconds](const Comp&, const Json&) {
        if (std::filesystem::exists(SCORE)) return;
        std::filesystem::create_directories(std::filesystem::path(SCORE).parent_path());
        genScore(SCORE, seconds);
    };
    media::audio(addRoot(c.roots, 0, c.duration), SCORE);
    out.push_back(std::move(c));
}

} // namespace rmtn
