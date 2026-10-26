// plot_charts.cpp -- plot:: statistics and the further charts (see rmtn/plot.hpp): histograms,
// error bars, bubbles, candles, areas, box plots, violins, waterfalls, heatmaps, radar,
// gauges, sunbursts, treemaps, Sankey diagrams and funnels.
#include "plot_util.hpp"
#include "rmtn/media.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace rmtn {
namespace plot {
using namespace detail;
namespace detail {

constexpr double kTau = 6.283185307179586;

// Staggered progress of item i of n: each takes `dur` of u and they start evenly over the rest.
float stagger(float u, size_t i, size_t n, float dur) {
    const float start = n > 1 ? (1 - dur) * (float)i / (float)(n - 1) : 0;
    return easeOut((u - start) / dur);
}

Color pick(const std::vector<Color>& pal, size_t i) { return pal.empty() ? palette((int)i) : pal[i % pal.size()]; }

float textWidth(const std::string& tex, float size) { return formula(tex, size, kWhite).width; }

// The prefix of a left-to-right polyline up to x = xEnd (interpolating the last point).
std::vector<Vector2> upToX(const std::vector<Vector2>& pts, float xEnd) {
    std::vector<Vector2> out;
    for (size_t i = 0; i < pts.size(); i++) {
        if (pts[i].x <= xEnd) { out.push_back(pts[i]); continue; }
        if (i > 0) {
            const float f = (xEnd - pts[i - 1].x) / std::max(pts[i].x - pts[i - 1].x, 1e-3f);
            if (f > 0) out.push_back({ pts[i - 1].x + (pts[i].x - pts[i - 1].x) * f, pts[i - 1].y + (pts[i].y - pts[i - 1].y) * f });
        }
        break;
    }
    return out;
}

// An annulus sector (rIn 0: a pie slice), angles in degrees clockwise from +x.
vg::Path sector(Vector2 c, float rIn, float rOut, float fromDeg, float toDeg) {
    std::vector<Vector2> pts;
    const int steps = std::max(2, (int)((toDeg - fromDeg) / 2));
    for (int k = 0; k <= steps; k++) {
        const float a = (fromDeg + (toDeg - fromDeg) * k / steps) * DEG2RAD;
        pts.push_back({ c.x + std::cos(a) * rOut, c.y + std::sin(a) * rOut });
    }
    if (rIn > 0)
        for (int k = steps; k >= 0; k--) {
            const float a = (fromDeg + (toDeg - fromDeg) * k / steps) * DEG2RAD;
            pts.push_back({ c.x + std::cos(a) * rIn, c.y + std::sin(a) * rIn });
        }
    else pts.push_back(c);
    return vg::Path::polyline(pts, true);
}

// Anchor for a label placed outward along direction (dx, dy).
Vector2 outward(float dx, float dy) {
    return { dx > 0.25f ? 0.f : dx < -0.25f ? 1.f : 0.5f, dy > 0.25f ? 0.f : dy < -0.25f ? 1.f : 0.5f };
}

// Dark text on light fills, light text on dark ones.
Color onFill(Color fill) {
    const float l = (0.2126f * fill.r + 0.7152f * fill.g + 0.0722f * fill.b) / 255;
    return l > 0.5f ? kDark : Color{ 240, 242, 246, 255 };
}

// A sibling's shade of its parent's colour: from a little darker to a little lighter.
Color shade(Color parent, size_t i, size_t n) {
    const float f = n > 1 ? (float)i / (float)(n - 1) : 0.5f;
    return f < 0.5f ? mixC(parent, kDark, (0.5f - f) * 0.5f) : mixC(parent, kWhite, (f - 0.5f) * 0.6f);
}

double quantile(const std::vector<double>& sorted, double q) {
    if (sorted.empty()) return 0;
    const double pos = q * (double)(sorted.size() - 1);
    const size_t i = (size_t)pos;
    const double f = pos - (double)i;
    return i + 1 < sorted.size() ? sorted[i] + (sorted[i + 1] - sorted[i]) * f : sorted[i];
}

std::vector<double> finiteSorted(const std::vector<double>& v) {
    std::vector<double> out;
    for (double x : v) if (std::isfinite(x)) out.push_back(x);
    std::sort(out.begin(), out.end());
    return out;
}

double silverman(const std::vector<double>& sorted) {
    const size_t n = sorted.size();
    if (n < 2) return 1;
    const double mean = std::accumulate(sorted.begin(), sorted.end(), 0.0) / (double)n;
    double var = 0;
    for (double x : sorted) var += (x - mean) * (x - mean);
    const double sd = std::sqrt(var / (double)(n - 1)), iqr = quantile(sorted, 0.75) - quantile(sorted, 0.25);
    double spread = iqr > 0 ? std::min(sd, iqr / 1.34) : sd;
    if (spread <= 0) spread = std::max(std::fabs(mean) * 0.1, 1e-3);
    return 0.9 * spread * std::pow((double)n, -0.2);
}

std::vector<std::string> splitCsv(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool quoted = false;
    for (char ch : line) {
        if (ch == '"') { quoted = !quoted; continue; }
        if (ch == ',' && !quoted) { out.push_back(cur); cur.clear(); continue; }
        if (ch != '\r') cur += ch;
    }
    out.push_back(cur);
    for (auto& s : out) {
        const size_t b = s.find_first_not_of(" \t"), e = s.find_last_not_of(" \t");
        s = b == std::string::npos ? "" : s.substr(b, e - b + 1);
    }
    return out;
}

bool toNumber(const std::string& s, double& v) {
    if (s.empty()) return false;
    char* end = nullptr;
    v = std::strtod(s.c_str(), &end);
    return end && !*end;
}

std::string readFile(const std::string& file, const char* who) {
    const std::string path = media::resolve(file);
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error(std::string(who) + ": cannot read " + path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

Color colorRamp(const unsigned* stops, int n, float t) {
    t = clamp01(t) * (float)(n - 1);
    const int i = std::min((int)t, n - 2);
    return mixC(mkColor(stops[i]), mkColor(stops[i + 1]), t - (float)i);
}

} // namespace detail

// ------------------------------------------------------------------ statistics

Range nice(double lo, double hi, double pad) {
    if (lo > hi) std::swap(lo, hi);
    const double span = hi - lo;
    lo -= span * pad;
    hi += span * pad;
    if (hi == lo) { lo -= 0.5; hi += 0.5; }
    const double step = Range(lo, hi).tick();
    return Range(std::floor(lo / step + 1e-9) * step, std::ceil(hi / step - 1e-9) * step);
}

Summary summarize(std::vector<double> values) {
    Summary s;
    const std::vector<double> v = finiteSorted(values);
    if (v.empty()) return s;
    s.q1 = quantile(v, 0.25);
    s.median = quantile(v, 0.5);
    s.q3 = quantile(v, 0.75);
    s.mean = std::accumulate(v.begin(), v.end(), 0.0) / (double)v.size();
    const double iqr = s.q3 - s.q1, fenceLo = s.q1 - 1.5 * iqr, fenceHi = s.q3 + 1.5 * iqr;
    s.lo = s.q1;
    s.hi = s.q3;
    for (double x : v) {
        if (x < fenceLo || x > fenceHi) { s.outliers.push_back(x); continue; }
        s.lo = std::min(s.lo, x);
        s.hi = std::max(s.hi, x);
    }
    return s;
}

Fn density(const std::vector<double>& values, double bandwidth) {
    const std::vector<double> v = finiteSorted(values);
    const double h = bandwidth > 0 ? bandwidth : silverman(v);
    const double norm = v.empty() ? 0 : 1 / ((double)v.size() * h * std::sqrt(kTau));
    return [v, h, norm](double x) {
        double sum = 0;
        for (double xi : v) {
            const double z = (x - xi) / h;
            if (z > -8 && z < 8) sum += std::exp(-0.5 * z * z);
        }
        return sum * norm;
    };
}

double Bins::max() const {
    double m = 0;
    for (double c : counts) m = std::max(m, c);
    return m;
}

Bins bin(const std::vector<double>& values, int bins, double lo, double hi) {
    const std::vector<double> v = finiteSorted(values);
    Bins b;
    if (std::isnan(lo)) lo = v.empty() ? 0 : v.front();
    if (std::isnan(hi)) hi = v.empty() ? 1 : v.back();
    if (hi <= lo) hi = lo + 1;
    if (bins <= 0) {
        const double k = std::ceil(std::log2(std::max<double>((double)v.size(), 1))) + 1;
        const double raw = (hi - lo) / k, mag = std::pow(10.0, std::floor(std::log10(raw))), m = raw / mag;
        b.width = (m < 1.5 ? 1 : m < 3.5 ? 2 : m < 7.5 ? 5 : 10) * mag;
        b.lo = std::floor(lo / b.width + 1e-9) * b.width;
        bins = std::max(1, (int)std::ceil((hi - b.lo) / b.width - 1e-9));
        if (b.lo + bins * b.width <= hi) bins++;   // hi itself needs a bin
    } else {
        b.lo = lo;
        b.width = (hi - lo) / bins;
    }
    b.counts.assign((size_t)bins, 0);
    for (double x : v) {
        if (x < b.lo || x > b.hi()) continue;
        const size_t i = std::min((size_t)((x - b.lo) / b.width), b.counts.size() - 1);
        b.counts[i] += 1;
    }
    return b;
}

// ------------------------------------------------------------------ numeric-axis charts

void histogram(const Ctx& c, const Axes& a, const Bins& b, Color color, float u) {
    u = clamp01(u);
    const size_t n = b.counts.size();
    for (size_t i = 0; i < n; i++) {
        const float k = stagger(u, i, n, 0.45f);
        if (k <= 0 || b.counts[i] <= 0) continue;
        const Vector2 p = a.map(b.lo + b.width * i, 0), q = a.map(b.lo + b.width * (i + 1), b.counts[i] * k);
        const float inset = std::min(1.5f, (q.x - p.x) * 0.1f);
        vg::fill(c, vg::Path::rect({ p.x + inset, q.y, q.x - p.x - 2 * inset, p.y - q.y }), color);
    }
}

void errorBars(const Ctx& c, const Axes& a, const std::vector<Vector2>& pts, const std::vector<double>& err, Color color,
               float width, float cap, float u) {
    u = clamp01(u);
    vg::Stroke st(color, width);
    st.cap = vg::Cap::Butt;
    for (size_t i = 0; i < pts.size() && i < err.size(); i++) {
        const float k = stagger(u, i, pts.size(), 0.4f);
        if (k <= 0) continue;
        const Vector2 lo = a.map(pts[i].x, pts[i].y - err[i] * k), hi = a.map(pts[i].x, pts[i].y + err[i] * k);
        const float w = cap * k;
        vg::Path p = vg::Path::line(lo, hi);
        p.moveTo({ lo.x - w, lo.y }).lineTo({ lo.x + w, lo.y });
        p.moveTo({ hi.x - w, hi.y }).lineTo({ hi.x + w, hi.y });
        vg::stroke(c, p, st);
    }
}

void bubbles(const Ctx& c, const Axes& a, const std::vector<Bubble>& b, const BubbleStyle& s, float u) {
    u = clamp01(u);
    double maxSize = s.maxSize;
    if (maxSize <= 0) for (const Bubble& x : b) maxSize = std::max(maxSize, x.size);
    if (maxSize <= 0) return;
    std::vector<size_t> order(b.size());
    std::iota(order.begin(), order.end(), size_t(0));
    std::stable_sort(order.begin(), order.end(), [&](size_t i, size_t j) { return b[i].size > b[j].size; });
    for (size_t i : order) {
        const float k = stagger(u, i, b.size(), 0.35f);
        if (k <= 0) continue;
        const Color col = b[i].color.a ? b[i].color : pick(s.palette, i);
        const float r = s.maxRadius * (float)std::sqrt(std::max(0.0, b[i].size) / maxSize) * k;
        const Vector2 p = a.map(b[i].x, b[i].y);
        vg::fill(c, vg::Path::circle(p, r), withAlpha(col, s.opacity * std::min(1.f, k * 1.5f)));
        if (s.outline > 0) vg::stroke(c, vg::Path::circle(p, r), vg::Stroke(withAlpha(mixC(col, kWhite, 0.35), k), s.outline));
        if (!s.labels || b[i].label.empty()) continue;
        const std::string tex = textTex(b[i].label);
        const float size = std::min(s.textSize, r * 0.7f);
        if (size >= 12 && textWidth(tex, size) < r * 1.8f) label(c, tex, p, { 0.5f, 0.5f }, size, withAlpha(s.textColor, k));
    }
}

void candles(const Ctx& c, const Axes& a, const std::vector<Candle>& k, const CandleStyle& s, float u) {
    u = clamp01(u);
    const float half = s.width / 2;
    for (size_t i = 0; i < k.size(); i++) {
        const float g = stagger(u, i, k.size(), 0.25f);
        if (g <= 0) continue;
        const Candle& d = k[i];
        const Color col = withAlpha(d.close >= d.open ? s.up : s.down, std::min(1.f, g * 2));
        const double x = i + 0.5;
        auto grow = [&](double v) { return d.open + (v - d.open) * g; };
        vg::Stroke wick(col, s.wick);
        wick.cap = vg::Cap::Butt;
        vg::stroke(c, vg::Path::line(a.map(x, grow(d.low)), a.map(x, grow(d.high))), wick);
        const Vector2 p = a.map(x - half, grow(d.open)), q = a.map(x + half, grow(d.close));
        const float y0 = std::min(p.y, q.y), h = std::max(std::fabs(q.y - p.y), 1.5f);
        vg::fill(c, vg::Path::rect({ p.x, y0, q.x - p.x, h }), col);
    }
}

// ------------------------------------------------------------------ category charts

void areas(const Ctx& c, const Table& t, const ChartStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0) return;
    const Axes a = chartAxes(t, s);
    axes(c, a, s.axes, clamp01(u * 3));
    categoryLabels(c, t.categories, a, s, u);
    legend(c, t, s, u);
    const size_t n = t.categories.size();
    if (n == 0) return;
    const float reveal = clamp01((u - 0.15f) / 0.85f);
    if (reveal <= 0) return;
    const float x0 = a.map(0.5, 0).x, xEnd = x0 + (a.map(n - 0.5, 0).x - x0) * reveal;
    std::vector<double> base(n, 0);
    std::vector<std::vector<Vector2>> tops;
    for (size_t j = 0; j < t.series.size(); j++) {
        std::vector<Vector2> top, bottom;
        for (size_t i = 0; i < n; i++) {
            const double v = i < t.series[j].values.size() ? t.series[j].values[i] : 0;
            const double b0 = s.stacked ? base[i] : 0;
            top.push_back(a.map(i + 0.5, b0 + v));
            bottom.push_back(a.map(i + 0.5, b0));
            if (s.stacked) base[i] += v;
        }
        top = upToX(top, xEnd);
        bottom = upToX(bottom, xEnd);
        tops.push_back(top);
        if (top.size() < 2) continue;
        std::vector<Vector2> poly = top;
        poly.insert(poly.end(), bottom.rbegin(), bottom.rend());
        vg::fill(c, vg::Path::polyline(poly, true), withAlpha(seriesColor(t, j, s.palette), s.stacked ? 0.85f : 0.35f));
    }
    vg::Stroke st({ 0, 0, 0, 0 }, s.lineWidth * (s.stacked ? 0.5f : 0.8f));
    for (size_t j = 0; j < tops.size(); j++) {
        if (tops[j].size() < 2) continue;
        st.color = s.stacked ? mixC(seriesColor(t, j, s.palette), kWhite, 0.3) : seriesColor(t, j, s.palette);
        vg::stroke(c, vg::Path::polyline(tops[j]), st);
    }
}

namespace {

struct Groups {
    std::vector<std::string> names;
    std::vector<Summary> stats;
    Axes axes;
};
Groups layoutGroups(const std::vector<Series>& groups, const ChartStyle& s, double pad) {
    Groups g;
    double lo = INFINITY, hi = -INFINITY;
    for (const Series& se : groups) {
        g.names.push_back(se.name);
        g.stats.push_back(summarize(se.values));
        for (double x : se.values)
            if (std::isfinite(x)) { lo = std::min(lo, x); hi = std::max(hi, x); }
    }
    if (!(lo <= hi)) { lo = 0; hi = 1; }
    const double span = std::max(hi - lo, 1e-9);
    const Range y = s.y.min == s.y.max ? nice(lo - span * pad, hi + span * pad) : s.y;
    g.axes = Axes(s.box, Range(0, (double)std::max<size_t>(groups.size(), 1), 1), y);
    return g;
}

void drawBox(const Ctx& c, const Axes& a, double x, float halfW, const Summary& m, Color col, float k, float lineW,
             Color medianColor, bool slim) {
    auto at = [&](double v) { return m.median + (v - m.median) * k; };
    vg::Stroke st(withAlpha(col, k), lineW);
    st.cap = vg::Cap::Butt;
    const Vector2 lo = a.map(x, at(m.lo)), q1 = a.map(x, at(m.q1)), q3 = a.map(x, at(m.q3)), hi = a.map(x, at(m.hi));
    vg::Path w = vg::Path::line(lo, q1);
    w.moveTo(q3).lineTo(hi);
    if (!slim) {
        w.moveTo({ lo.x - halfW * 0.4f, lo.y }).lineTo({ lo.x + halfW * 0.4f, lo.y });
        w.moveTo({ hi.x - halfW * 0.4f, hi.y }).lineTo({ hi.x + halfW * 0.4f, hi.y });
    }
    vg::stroke(c, w, st);
    const Rectangle r{ q3.x - halfW, q3.y, 2 * halfW, std::max(q1.y - q3.y, 1.f) };
    if (slim) vg::fill(c, vg::Path::rect(r, std::min(halfW, 3.f)), withAlpha(col, k));
    else {
        vg::fill(c, vg::Path::rect(r, 3), withAlpha(col, 0.35f * k));
        vg::stroke(c, vg::Path::rect(r, 3), vg::Stroke(withAlpha(col, k), lineW));
    }
    const Vector2 med = a.map(x, m.median);
    if (slim) vg::fill(c, vg::Path::circle(med, halfW * 0.9f), withAlpha(medianColor, k));
    else vg::stroke(c, vg::Path::line({ med.x - halfW, med.y }, { med.x + halfW, med.y }), vg::Stroke(withAlpha(medianColor, k), lineW * 1.4f));
}

} // namespace

void boxplot(const Ctx& c, const std::vector<Series>& groups, const ChartStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0) return;
    const Groups g = layoutGroups(groups, s, 0.06);
    const Axes& a = g.axes;
    axes(c, a, s.axes, clamp01(u * 3));
    categoryLabels(c, g.names, a, s, u);
    const float slot = s.box.width / std::max<size_t>(groups.size(), 1), halfW = slot * (1 - s.gap) * 0.35f;
    for (size_t i = 0; i < groups.size(); i++) {
        const float k = stagger(clamp01((u - 0.15f) / 0.85f), i, groups.size(), 0.5f);
        if (k <= 0) continue;
        const Color col = groups[i].color.a ? groups[i].color : pick(s.palette, i);
        const Summary& m = g.stats[i];
        drawBox(c, a, i + 0.5, halfW, m, col, k, std::max(2.f, s.lineWidth * 0.6f), s.textColor, false);
        for (double o : m.outliers) vg::fill(c, vg::Path::circle(a.map(i + 0.5, o), s.pointRadius * 0.55f * k), withAlpha(col, k));
        if (s.values)
            label(c, number(m.median, s.decimals), { a.map(i + 0.5, 0).x + halfW + 10, a.map(0, m.median).y }, { 0, 0.5f },
                  s.textSize * 0.8f, withAlpha(s.textColor, clamp01((k - 0.6f) * 2.5f)));
    }
}

void violin(const Ctx& c, const std::vector<Series>& groups, const ChartStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0) return;
    // densities over each group's extent, widened by a bandwidth
    struct Shape { double lo, hi; std::vector<double> d; };
    std::vector<Shape> shapes;
    std::vector<Series> padded;
    double peak = 0;
    for (const Series& se : groups) {
        const std::vector<double> v = finiteSorted(se.values);
        const double h = silverman(v);
        Shape sh{ v.empty() ? 0 : v.front() - h, v.empty() ? 1 : v.back() + h, {} };
        const Fn f = density(v, h);
        for (int k = 0; k <= 64; k++) sh.d.push_back(f(sh.lo + (sh.hi - sh.lo) * k / 64));
        for (double d : sh.d) peak = std::max(peak, d);
        shapes.push_back(sh);
        Series p = se;
        p.values = v;
        p.values.push_back(sh.lo);   // so the range covers the tails
        p.values.push_back(sh.hi);
        padded.push_back(p);
    }
    Groups g = layoutGroups(padded, s, 0.02);
    for (size_t i = 0; i < groups.size(); i++) g.stats[i] = summarize(groups[i].values);
    const Axes& a = g.axes;
    const double yLo = std::min(a.y.min, a.y.max), yHi = std::max(a.y.min, a.y.max);
    axes(c, a, s.axes, clamp01(u * 3));
    categoryLabels(c, g.names, a, s, u);
    const float slot = s.box.width / std::max<size_t>(groups.size(), 1), halfW = slot * (1 - s.gap) * 0.5f;
    for (size_t i = 0; i < groups.size(); i++) {
        const float k = stagger(clamp01((u - 0.15f) / 0.85f), i, groups.size(), 0.5f);
        if (k <= 0 || peak <= 0) continue;
        const Color col = groups[i].color.a ? groups[i].color : pick(s.palette, i);
        const Shape& sh = shapes[i];
        const float cx = a.map(i + 0.5, 0).x;
        std::vector<Vector2> right, left;
        for (size_t j = 0; j < sh.d.size(); j++) {
            const double v = sh.lo + (sh.hi - sh.lo) * j / (sh.d.size() - 1);
            if (v < yLo || v > yHi) continue;   // a fixed range clips the tails
            const float y = a.map(0, v).y, w = halfW * (float)(sh.d[j] / peak) * k;
            right.push_back({ cx + w, y });
            left.push_back({ cx - w, y });
        }
        if (right.size() < 2) continue;
        right.insert(right.end(), left.rbegin(), left.rend());
        const vg::Path body = vg::Path::polyline(right, true);
        vg::fill(c, body, withAlpha(col, 0.4f * k));
        vg::stroke(c, body, vg::Stroke(withAlpha(col, k), 2.5f));
        drawBox(c, a, i + 0.5, std::max(4.f, halfW * 0.07f), g.stats[i], col, k, 2.5f, s.textColor, true);
    }
}

void waterfall(const Ctx& c, const Table& t, const WaterfallStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0 || t.series.empty()) return;
    const std::vector<double>& v = t.series[0].values;
    std::vector<std::string> names = t.categories;
    names.resize(v.size());
    std::vector<double> from, to;
    double run = 0, lo = 0, hi = 0;
    for (double d : v) {
        from.push_back(run);
        run += d;
        to.push_back(run);
        lo = std::min(lo, run);
        hi = std::max(hi, run);
    }
    if (s.total) { from.push_back(0); to.push_back(run); names.push_back(s.totalLabel); }
    const size_t n = from.size();
    const Range y = s.y.min == s.y.max ? nice(lo, hi + (hi - lo) * 0.06) : s.y;
    const Axes a(s.box, Range(0, (double)std::max<size_t>(n, 1), 1), y);
    axes(c, a, s.axes, clamp01(u * 3));
    categoryLabels(c, names, a, s, u);
    const float slot = s.box.width / std::max<size_t>(n, 1), w = slot * (1 - s.gap);
    std::vector<float> ks(n);
    for (size_t i = 0; i < n; i++) ks[i] = stagger(clamp01((u - 0.1f) / 0.9f), i, n, 0.3f);
    for (size_t i = 0; i < n; i++) {
        const float k = ks[i];
        if (k <= 0) continue;
        const bool isTotal = s.total && i + 1 == n;
        const double d = to[i] - from[i], end = from[i] + d * k;
        const Color col = isTotal ? s.sum : d >= 0 ? s.rise : s.fall;
        const float x0 = s.box.x + i * slot + (slot - w) / 2;
        const Vector2 p = a.map(0, from[i]), q = a.map(0, end);
        const float y0 = std::min(p.y, q.y), h = std::fabs(q.y - p.y);
        if (h > 0.5f) vg::fill(c, vg::Path::rect({ x0, y0, w, h }, std::min(4.f, std::min(w, h) / 3)), col);
        if (s.connectors && i + 1 < n && ks[i + 1] > 0) {
            vg::Stroke st(withAlpha(s.axes.color, 0.6f * ks[i + 1]), 2);
            st.dash = { 6, 6 };
            st.cap = vg::Cap::Butt;
            const float yRun = a.map(0, to[i]).y;
            vg::stroke(c, vg::Path::line({ x0 + w, yRun }, { x0 + slot, yRun }), st);
        }
        if (s.values) {
            const std::string txt = (isTotal || d < 0 ? "" : "+") + number(isTotal ? to[i] : d, s.decimals);
            const bool up = isTotal ? to[i] >= 0 : d >= 0;
            label(c, txt, { x0 + w / 2, up ? std::min(p.y, q.y) - 8 : std::max(p.y, q.y) + 8 }, { 0.5f, up ? 1.f : 0.f },
                  s.textSize, withAlpha(s.textColor, clamp01((k - 0.5f) * 2)));
        }
    }
}

// ------------------------------------------------------------------ heatmaps

Color viridis(float t) {
    static const unsigned S[] = { 0x440154, 0x482878, 0x3e4989, 0x31688e, 0x26828e, 0x1f9e89, 0x35b779, 0x6ece58, 0xb5de2b, 0xfde725 };
    return colorRamp(S, 10, t);
}
Color diverging(float t) {
    static const unsigned S[] = { 0x3b4cc0, 0x6788ee, 0x9abbff, 0xc9d7f0, 0xedd1c2, 0xf7a889, 0xe26952, 0xb40426 };
    return colorRamp(S, 8, t);
}

void heatmap(const Ctx& c, const Table& t, const HeatmapStyle& s, float u) {
    u = clamp01(u);
    const size_t n = t.categories.size(), m = t.series.size();
    if (u <= 0 || n == 0 || m == 0) return;
    double lo = s.v.min, hi = s.v.max;
    if (lo == hi) {
        lo = INFINITY; hi = -INFINITY;
        for (const Series& se : t.series)
            for (double v : se.values) { lo = std::min(lo, v); hi = std::max(hi, v); }
        if (!(lo < hi)) hi = lo + 1;
    }
    const float cw = s.box.width / m, ch = s.box.height / n, k0 = clamp01(u * 2.5f);
    const auto colors = s.colors ? s.colors : std::function<Color(float)>(viridis);
    for (size_t i = 0; i < n; i++)
        label(c, textTex(t.categories[i]), { s.box.x - 14, s.box.y + ch * (i + 0.5f) }, { 1, 0.5f }, s.textSize, withAlpha(s.textColor, k0));
    for (size_t j = 0; j < m; j++)
        label(c, textTex(t.series[j].name), { s.box.x + cw * (j + 0.5f), s.box.y - 12 }, { 0.5f, 1 }, s.textSize, withAlpha(s.textColor, k0));
    const float vs = std::min(s.textSize, ch * 0.45f);
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < m; j++) {
            const float d = ((float)i / std::max<size_t>(n - 1, 1) + (float)j / std::max<size_t>(m - 1, 1)) / 2;
            const float k = easeOut((u * 1.7f - 0.15f - d) / 0.55f);
            if (k <= 0) continue;
            const double v = i < t.series[j].values.size() ? t.series[j].values[i] : 0;
            const Color col = colors((float)((v - lo) / (hi - lo)));
            const float w = (cw - s.gap) * (0.5f + 0.5f * k), h = (ch - s.gap) * (0.5f + 0.5f * k);
            const Vector2 mid{ s.box.x + cw * (j + 0.5f), s.box.y + ch * (i + 0.5f) };
            vg::fill(c, vg::Path::rect({ mid.x - w / 2, mid.y - h / 2, w, h }, std::min(4.f, std::min(w, h) / 4)), withAlpha(col, k));
            if (!s.values || vs < 12) continue;
            const std::string txt = number(v, s.decimals);
            if (textWidth(txt, vs) < cw - s.gap - 8) label(c, txt, mid, { 0.5f, 0.5f }, vs, withAlpha(onFill(col), clamp01((k - 0.5f) * 2)));
        }
    if (!s.colorbar) return;
    ColorbarStyle cs;
    cs.colors = colors;
    cs.textSize = s.textSize * 0.85f;
    cs.textColor = s.textColor;
    Ctx faded = c;
    faded.a *= k0;
    colorbar(faded, { s.box.x + s.box.width + 36, s.box.y, 26, s.box.height }, Range(lo, hi), cs);
}

// ------------------------------------------------------------------ keys

void colorbar(const Ctx& c, Rectangle box, Range v, const ColorbarStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0 || !(v.max > v.min)) return;
    const auto colors = s.colors ? s.colors : std::function<Color(float)>(viridis);
    const bool upright = box.height > box.width;
    const float k = easeOut(u), len = (upright ? box.height : box.width) * k;
    const int steps = 64;
    for (int i = 0; i < steps; i++) {
        const float a = len * i / steps, b = len / steps + 0.5f;
        const Rectangle r = upright ? Rectangle{ box.x, box.y + box.height - a - b, box.width, b }
                                    : Rectangle{ box.x + a, box.y, b, box.height };
        vg::fill(c, vg::Path::rect(r), colors((i + 0.5f) / steps * k));
    }
    const int dec = s.decimals >= 0 ? s.decimals : decimalsFor(v.tick());
    const Color tc = withAlpha(s.textColor, clamp01((u - 0.3f) / 0.5f));
    for (double x : v.ticks()) {
        const float f = (float)((x - v.min) / (v.max - v.min));
        if (f > k + 1e-4f) continue;
        const std::string txt = textTex(number(x, dec) + s.suffix);
        if (upright) label(c, txt, { box.x + box.width + 12, box.y + box.height * (1 - f) }, { 0, 0.5f }, s.textSize, tc);
        else label(c, txt, { box.x + box.width * f, box.y + box.height + 8 }, { 0.5f, 0 }, s.textSize, tc);
    }
    if (!s.title.empty()) label(c, textTex(s.title), { box.x, box.y - 12 }, { 0, 1 }, s.textSize, tc);
}

void legend(const Ctx& c, Vector2 at, const std::vector<LegendItem>& items, const LegendStyle& s, float u) {
    u = clamp01(u);
    const size_t n = items.size();
    if (u <= 0 || n == 0) return;
    Vector2 p = at;
    for (size_t i = 0; i < n; i++) {
        const float k = easeOut(u * (1 + 0.5f * n) - 0.5f * i);
        if (k > 0) {
            Ctx ci = c;
            ci.a *= k;
            const float sw = s.swatch, cy = p.y + std::max(sw, s.textSize) / 2;
            if (s.round) vg::fill(ci, vg::Path::circle({ p.x + sw / 2, cy }, sw / 2), items[i].color);
            else vg::fill(ci, vg::Path::rect({ p.x, cy - sw / 2, sw, sw }, sw / 6), items[i].color);
            label(ci, textTex(items[i].name), { p.x + sw + 10, cy }, { 0, 0.5f }, s.textSize, s.textColor);
        }
        if (s.across) p.x += s.swatch + 10 + textWidth(textTex(items[i].name), s.textSize) + s.gap * 2.5f;
        else p.y += std::max(s.swatch, s.textSize) + s.gap;
    }
}

// ------------------------------------------------------------------ round charts

void radar(const Ctx& c, const Table& t, Vector2 center, float radius, const RadarStyle& s, float u) {
    u = clamp01(u);
    const size_t n = t.categories.size(), m = t.series.size();
    if (u <= 0 || n < 3) return;
    Range r = s.r;
    if (r.min == r.max) {
        double hi = 0;
        for (const Series& se : t.series) for (double v : se.values) hi = std::max(hi, v);
        r = nice(0, hi > 0 ? hi : 1);
    }
    auto dir = [&](size_t i) {
        const float a = (s.startDeg + 360.f * i / n) * DEG2RAD;
        return Vector2{ std::cos(a), std::sin(a) };
    };
    auto at = [&](size_t i, double v, float k) {
        const float f = (float)std::clamp((v - r.min) / (r.max - r.min), 0.0, 1.0) * radius * k;
        const Vector2 d = dir(i);
        return Vector2{ center.x + d.x * f, center.y + d.y * f };
    };
    // the web
    const float w = easeOut(u * 2.5f);
    vg::Stroke grid(s.gridColor, 2);
    for (size_t i = 0; i < n; i++) vg::stroke(c, vg::Path::line(center, at(i, r.max, w)), grid);
    for (double tv : r.ticks()) {
        if (tv <= r.min) continue;
        std::vector<Vector2> ring;
        for (size_t i = 0; i < n; i++) ring.push_back(at(i, tv, 1));
        vg::stroke(c, vg::Path::polyline(ring, true).trim(0, w), grid);
        if (s.numbers)
            label(c, number(tv, decimalsFor(r.tick())), { at(0, tv, 1).x + 8, at(0, tv, 1).y }, { 0, 0.5f }, s.textSize * 0.7f,
                  withAlpha(s.textColor, w * 0.8f));
    }
    if (s.labels)
        for (size_t i = 0; i < n; i++) {
            const Vector2 d = dir(i);
            label(c, textTex(t.categories[i]), { center.x + d.x * (radius + 20), center.y + d.y * (radius + 20) }, outward(d.x, d.y),
                  s.textSize, withAlpha(s.textColor, w));
        }
    // the series
    const float lag = m > 1 ? std::min(0.24f, 0.1f * (m - 1)) : 0, dur = 0.7f - lag;
    for (size_t j = 0; j < m; j++) {
        const float k = easeOut((u - 0.3f - (m > 1 ? lag * j / (m - 1) : 0)) / dur);
        if (k <= 0) continue;
        const Color col = t.series[j].color.a ? t.series[j].color : pick(s.palette, j);
        std::vector<Vector2> pts;
        for (size_t i = 0; i < n; i++) pts.push_back(at(i, i < t.series[j].values.size() ? t.series[j].values[i] : 0, k));
        const vg::Path poly = vg::Path::polyline(pts, true);
        vg::fill(c, poly, withAlpha(col, s.fillAlpha));
        vg::stroke(c, poly, vg::Stroke(col, s.lineWidth));
        for (Vector2 p : pts) vg::fill(c, vg::Path::circle(p, s.pointRadius), col);
    }
    if (!s.legend || m < 2) return;
    float total = 0;
    const float sq = s.textSize * 0.6f;
    for (const Series& se : t.series) total += sq + 10 + textWidth(textTex(se.name), s.textSize) + 36;
    float x = center.x - (total - 36) / 2;
    const float y = center.y + radius + s.textSize * 2.6f;
    for (size_t j = 0; j < m; j++) {
        const Color col = t.series[j].color.a ? t.series[j].color : pick(s.palette, j);
        vg::fill(c, vg::Path::rect({ x, y - sq / 2, sq, sq }, sq * 0.2f), withAlpha(col, w));
        const std::string tex = textTex(t.series[j].name);
        label(c, tex, { x + sq + 10, y }, { 0, 0.5f }, s.textSize, withAlpha(s.textColor, w));
        x += sq + 10 + textWidth(tex, s.textSize) + 36;
    }
}

void gauge(const Ctx& c, double value, Vector2 center, float radius, const GaugeStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0) return;
    const float a0 = -90 - s.sweepDeg / 2, a1 = -90 + s.sweepDeg / 2;
    const float th = radius * s.thickness, rMid = radius - th / 2, k0 = easeOut(u * 3);
    const double span = s.range.max - s.range.min;
    vg::Stroke track(withAlpha(s.track, k0), th);
    track.cap = vg::Cap::Round;
    vg::stroke(c, vg::Path::arc(center, rMid, a0, a0 + (a1 - a0) * k0), track);
    const float k = easeOut((u - 0.15f) / 0.85f);
    const double shown = s.range.min + (std::clamp(value, s.range.min, s.range.max) - s.range.min) * k;
    const float f = span != 0 ? (float)((shown - s.range.min) / span) : 0;
    Color col = s.fill;
    for (const auto& z : s.zones) if (shown >= z.first) col = z.second;
    if (f > 0.002f) {
        vg::Stroke st(col, th);
        st.cap = vg::Cap::Round;
        vg::stroke(c, vg::Path::arc(center, rMid, a0, a0 + (a1 - a0) * f), st);
    }
    if (s.ticks) {
        const Range r = fitted(s.range, radius * s.sweepDeg * DEG2RAD, 60);
        vg::Stroke tk(withAlpha(s.textColor, 0.5f * k0), 2);
        tk.cap = vg::Cap::Butt;
        for (double v : r.ticks()) {
            const float a = (a0 + (a1 - a0) * (float)((v - s.range.min) / span)) * DEG2RAD;
            const Vector2 d{ std::cos(a), std::sin(a) };
            const float r0 = radius - th - 8, r1 = radius - th - 20;
            vg::stroke(c, vg::Path::line({ center.x + d.x * r0, center.y + d.y * r0 }, { center.x + d.x * r1, center.y + d.y * r1 }), tk);
        }
        const float ra = rMid, ta = s.textSize * 0.3f;
        for (float a : { a0, a1 }) {
            const Vector2 d{ std::cos(a * DEG2RAD), std::sin(a * DEG2RAD) };
            label(c, number(a == a0 ? s.range.min : s.range.max, decimalsFor(r.tick())), { center.x + d.x * ra, center.y + d.y * ra + th / 2 + 10 },
                  { 0.5f, 0 }, ta, withAlpha(s.textColor, 0.7f * k0));
        }
    }
    label(c, number(shown, s.decimals) + s.unit, center, { 0.5f, 0.55f }, s.textSize, withAlpha(s.textColor, k0));
    if (!s.title.empty())
        label(c, textTex(s.title), { center.x, center.y + s.textSize * 0.55f }, { 0.5f, 0 }, s.textSize * 0.36f, withAlpha(s.textColor, 0.8f * k0));
}

// ------------------------------------------------------------------ hierarchies

double Tree::total() const {
    if (children.empty()) return std::max(0.0, value);
    double sum = 0;
    for (const Tree& ch : children) sum += ch.total();
    return sum;
}

Tree Tree::parse(const std::string& text, char sep) {
    Tree root;
    std::istringstream in(text);
    std::string line;
    int row = 0;
    bool any = false;
    while (std::getline(in, line)) {
        row++;
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        const auto cells = splitCsv(line);
        double v = 0;
        if (cells.size() < 2 || !toNumber(cells[1], v)) {
            if (!any) { any = true; continue; }   // a header
            throw std::runtime_error("plot::Tree: expected \"path, value\" on line " + std::to_string(row));
        }
        any = true;
        Tree* node = &root;
        size_t start = 0;
        const std::string& path = cells[0];
        while (start <= path.size()) {
            size_t end = path.find(sep, start);
            if (end == std::string::npos) end = path.size();
            std::string part = path.substr(start, end - start);
            const size_t b = part.find_first_not_of(" \t"), e = part.find_last_not_of(" \t");
            part = b == std::string::npos ? "" : part.substr(b, e - b + 1);
            if (!part.empty()) {
                auto it = std::find_if(node->children.begin(), node->children.end(), [&](const Tree& ch) { return ch.name == part; });
                if (it == node->children.end()) { node->children.push_back(Tree{ part, 0, {}, { 0, 0, 0, 0 } }); node = &node->children.back(); }
                else node = &*it;
            }
            start = end + 1;
        }
        if (node == &root) throw std::runtime_error("plot::Tree: empty path on line " + std::to_string(row));
        node->value += v;
    }
    return root;
}

Tree Tree::csv(const std::string& file, char sep) { return parse(readFile(file, "plot::Tree"), sep); }

namespace {

int treeDepth(const Tree& t) {
    int d = 0;
    for (const Tree& ch : t.children) d = std::max(d, treeDepth(ch));
    return t.children.empty() ? 0 : d + 1;
}

Color childColor(const Tree& child, const Color* parent, size_t i, size_t n, const std::vector<Color>& pal) {
    if (child.color.a) return child.color;
    return parent ? shade(*parent, i, n) : pick(pal, i);
}

struct Ring { const Tree* node; int depth; float a0, a1; Color color; };
void sunLayout(const Tree& t, int depth, int maxDepth, float a0, float a1, const Color* col, const std::vector<Color>& pal,
               std::vector<Ring>& out) {
    if (depth >= maxDepth) return;
    const double total = t.total();
    if (total <= 0) return;
    float a = a0;
    for (size_t i = 0; i < t.children.size(); i++) {
        const Tree& ch = t.children[i];
        const float b = a + (a1 - a0) * (float)(ch.total() / total);
        const Color cc = childColor(ch, col, i, t.children.size(), pal);
        out.push_back({ &ch, depth + 1, a, b, cc });
        sunLayout(ch, depth + 1, maxDepth, a, b, &cc, pal, out);
        a = b;
    }
}

} // namespace

void sunburst(const Ctx& c, const Tree& t, Vector2 center, float radius, const SunburstStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0) return;
    const int depth = s.depth > 0 ? std::min(s.depth, treeDepth(t)) : treeDepth(t);
    if (depth == 0) return;
    std::vector<Ring> rings;
    sunLayout(t, 0, depth, s.startDeg, s.startDeg + 360, nullptr, s.palette, rings);
    const float r0 = radius * std::clamp(s.inner, 0.f, 0.9f), w = (radius - r0) / depth, lag = 0.15f;
    const float k0 = easeOut(u * 3);
    if (r0 > 0) {
        vg::fill(c, vg::Path::circle(center, (r0 - s.ringGap) * (0.6f + 0.4f * k0)), withAlpha(Color{ 38, 43, 58, 255 }, k0));
        const float ts = std::min(s.textSize * 1.1f, r0 * 0.4f);
        if (!t.name.empty()) label(c, textTex(t.name), { center.x, center.y - ts * 0.15f }, { 0.5f, 1 }, ts, withAlpha(s.centerColor, k0));
        label(c, number(t.total(), 0), { center.x, center.y + (t.name.empty() ? 0 : ts * 0.15f) }, { 0.5f, t.name.empty() ? 0.5f : 0.f },
              ts * (t.name.empty() ? 1.2f : 0.9f), withAlpha(s.centerColor, k0 * 0.85f));
    }
    for (const Ring& rg : rings) {
        const float sweep = s.startDeg + 360 * easeOut(u * (1 + lag * (depth - 1)) - lag * (rg.depth - 1));
        const float g = rg.a1 - rg.a0 < 359 ? s.gapDeg / 2 : 0;
        const float from = rg.a0 + g, to = std::min(rg.a1 - g, sweep);
        if (to <= from) continue;
        const float rIn = r0 + w * (rg.depth - 1) + (rg.depth > 1 || r0 > 0 ? s.ringGap / 2 : 0), rOut = r0 + w * rg.depth - s.ringGap / 2;
        vg::fill(c, sector(center, rIn, rOut, from, to), rg.color);
        if (!s.labels) continue;
        const float k = clamp01((sweep - rg.a1) / 20 + 1);
        const std::string tex = textTex(rg.node->name);
        const float rm = (rIn + rOut) / 2, mid = (rg.a0 + rg.a1) / 2 * DEG2RAD, ts = std::min(s.textSize, (rOut - rIn) * 0.6f);
        if (k <= 0 || ts < 12) continue;
        // the upright label's extent across the ring and along it must fit the segment
        const float tw = textWidth(tex, ts), th = ts * 0.9f, cs = std::fabs(std::cos(mid)), sn = std::fabs(std::sin(mid));
        if (tw * cs + th * sn > rOut - rIn - 6 || tw * sn + th * cs > rm * (rg.a1 - rg.a0 - 2 * g) * DEG2RAD - 8) continue;
        label(c, tex, { center.x + std::cos(mid) * rm, center.y + std::sin(mid) * rm }, { 0.5f, 0.5f }, ts,
              withAlpha(s.textColor.a ? s.textColor : onFill(rg.color), k));
    }
}

namespace {

// Squarified treemap (Bruls, Huizing & van Wijk): sizes (sorted descending) into r.
std::vector<Rectangle> squarify(const std::vector<double>& sizes, Rectangle r) {
    std::vector<Rectangle> out(sizes.size(), Rectangle{ r.x, r.y, 0, 0 });
    double total = 0;
    for (double v : sizes) total += v;
    if (total <= 0 || r.width <= 0 || r.height <= 0) return out;
    const double scale = (double)r.width * r.height / total;
    auto worst = [](double sum, double mn, double mx, double side) {
        const double s2 = sum * sum, w2 = side * side;
        return std::max(w2 * mx / s2, s2 / (w2 * mn));
    };
    size_t i = 0;
    while (i < sizes.size()) {
        const double side = std::min(r.width, r.height);
        size_t j = i;
        double sum = 0, mn = INFINITY, mx = 0, best = INFINITY;
        while (j < sizes.size()) {
            const double a = sizes[j] * scale;
            if (a <= 0) { j++; continue; }
            const double w = worst(sum + a, std::min(mn, a), std::max(mx, a), side);
            if (j > i && w > best) break;
            best = w; sum += a; mn = std::min(mn, a); mx = std::max(mx, a);
            j++;
        }
        if (sum <= 0) break;
        const bool column = r.width >= r.height;   // the row runs along the short side
        const float thick = (float)(sum / (column ? r.height : r.width));
        float at = column ? r.y : r.x;
        for (size_t k = i; k < j; k++) {
            const float len = (float)(sizes[k] * scale / thick);
            out[k] = column ? Rectangle{ r.x, at, thick, len } : Rectangle{ at, r.y, len, thick };
            at += len;
        }
        if (column) { r.x += thick; r.width -= thick; }
        else { r.y += thick; r.height -= thick; }
        i = j;
    }
    return out;
}

struct Tile { Rectangle r; Color color; const Tree* node; bool header; };
void tileLayout(const Tree& t, Rectangle r, int depth, int maxDepth, const Color* col, const TreemapStyle& s, std::vector<Tile>& out) {
    std::vector<size_t> order(t.children.size());
    std::iota(order.begin(), order.end(), size_t(0));
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return t.children[a].total() > t.children[b].total(); });
    std::vector<double> sizes;
    for (size_t i : order) sizes.push_back(t.children[i].total());
    const std::vector<Rectangle> rects = squarify(sizes, r);
    for (size_t k = 0; k < order.size(); k++) {
        const Tree& ch = t.children[order[k]];
        const Color cc = childColor(ch, col, order[k], t.children.size(), s.palette);
        Rectangle cr = rects[k];
        if (ch.children.empty() || depth + 1 >= maxDepth) { out.push_back({ cr, cc, &ch, false }); continue; }
        const float head = s.textSize * 1.4f;
        if (depth == 0 && cr.height > head * 3 && cr.width > head * 2) {
            out.push_back({ { cr.x, cr.y, cr.width, head }, cc, &ch, true });
            cr.y += head; cr.height -= head;
        }
        tileLayout(ch, { cr.x + s.gap, cr.y + s.gap, cr.width - 2 * s.gap, cr.height - 2 * s.gap }, depth + 1, maxDepth, &cc, s, out);
    }
}

} // namespace

void treemap(const Ctx& c, const Tree& t, Rectangle box, const TreemapStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0) return;
    const int maxDepth = s.depth > 0 ? s.depth : std::max(1, treeDepth(t));
    std::vector<Tile> tiles;
    tileLayout(t, box, 0, maxDepth, nullptr, s, tiles);
    size_t leaves = 0;
    for (const Tile& tl : tiles) leaves += !tl.header;
    size_t idx = 0;
    for (const Tile& tl : tiles) {
        const float k = stagger(u, idx, std::max<size_t>(leaves, 1), 0.4f);
        if (tl.header) {
            label(c, textTex(tl.node->name), { tl.r.x + 4, tl.r.y + tl.r.height * 0.5f }, { 0, 0.5f }, s.textSize,
                  withAlpha(s.headerColor, k));
            continue;
        }
        idx++;
        if (k <= 0) continue;
        const Rectangle r{ tl.r.x + s.gap / 2, tl.r.y + s.gap / 2, tl.r.width - s.gap, tl.r.height - s.gap };
        if (r.width <= 0 || r.height <= 0) continue;
        const float w = r.width * (0.4f + 0.6f * k), h = r.height * (0.4f + 0.6f * k);
        const Rectangle g{ r.x + (r.width - w) / 2, r.y + (r.height - h) / 2, w, h };
        vg::fill(c, vg::Path::rect(g, std::min(4.f, std::min(w, h) / 4)), withAlpha(tl.color, k));
        if (!s.labels) continue;
        const std::string name = textTex(tl.node->name), val = number(tl.node->total(), s.decimals);
        const float ts = s.textSize, vs = s.textSize * 0.8f, ka = clamp01((k - 0.6f) * 2.5f);
        const Color tc = s.textColor.a ? s.textColor : onFill(tl.color);
        if (r.width < textWidth(name, ts) + 16 || r.height < ts * 1.5f) continue;
        label(c, name, { r.x + 8, r.y + 6 }, { 0, 0 }, ts, withAlpha(tc, ka));
        if (s.values && r.height > ts * 1.3f + vs * 1.5f && r.width > textWidth(val, vs) + 16)
            label(c, val, { r.x + 8, r.y + 6 + ts * 1.25f }, { 0, 0 }, vs, withAlpha(tc, ka * 0.8f));
    }
}

// ------------------------------------------------------------------ flows

std::vector<Flow> parseFlows(const std::string& text) {
    std::vector<Flow> out;
    std::istringstream in(text);
    std::string line;
    int row = 0;
    bool any = false;
    while (std::getline(in, line)) {
        row++;
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        const auto cells = splitCsv(line);
        double v = 0;
        if (cells.size() < 3 || !toNumber(cells[2], v)) {
            if (!any) { any = true; continue; }   // a header
            throw std::runtime_error("plot::parseFlows: expected \"from, to, value\" on line " + std::to_string(row));
        }
        any = true;
        out.push_back({ cells[0], cells[1], v });
    }
    return out;
}

std::vector<Flow> flowsCsv(const std::string& file) { return parseFlows(readFile(file, "plot::flowsCsv")); }

void sankey(const Ctx& c, const std::vector<Flow>& flows, Rectangle box, const SankeyStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0 || flows.empty()) return;
    // nodes in order of first appearance
    std::vector<std::string> names;
    std::map<std::string, size_t> index;
    auto node = [&](const std::string& n) {
        auto it = index.find(n);
        if (it != index.end()) return it->second;
        index[n] = names.size();
        names.push_back(n);
        return names.size() - 1;
    };
    struct Link { size_t a, b; double v; float sy = 0, ty = 0; };
    std::vector<Link> links;
    for (const Flow& f : flows)
        if (f.value > 0 && f.from != f.to) links.push_back({ node(f.from), node(f.to), f.value });
    const size_t n = names.size();
    // columns: longest path from a source; sinks pushed to the last column
    std::vector<int> col(n, 0);
    for (size_t pass = 0; pass < n; pass++) {
        bool changed = false;
        for (const Link& l : links)
            if (col[l.b] < col[l.a] + 1 && col[l.a] + 1 < (int)n) { col[l.b] = col[l.a] + 1; changed = true; }
        if (!changed) break;
    }
    int cols = 0;
    for (int x : col) cols = std::max(cols, x + 1);
    std::vector<double> in(n, 0), out(n, 0);
    for (const Link& l : links) { out[l.a] += l.v; in[l.b] += l.v; }
    for (size_t i = 0; i < n; i++) if (out[i] == 0 && in[i] > 0) col[i] = cols - 1;
    std::vector<double> size(n);
    for (size_t i = 0; i < n; i++) size[i] = std::max(in[i], out[i]);
    // scale: the fullest column fits the box with its padding
    std::vector<std::vector<size_t>> column((size_t)cols);
    for (size_t i = 0; i < n; i++) column[(size_t)col[i]].push_back(i);
    double ky = INFINITY;
    for (const auto& cl : column) {
        double sum = 0;
        for (size_t i : cl) sum += size[i];
        if (sum > 0) ky = std::min(ky, (box.height - s.nodePad * (cl.size() - 1)) / sum);
    }
    if (!std::isfinite(ky) || ky <= 0) return;
    std::vector<float> y(n, 0), h(n, 0);
    for (size_t i = 0; i < n; i++) h[i] = (float)(size[i] * ky);
    auto stack = [&](std::vector<size_t>& cl) {
        float used = s.nodePad * (cl.size() - 1);
        for (size_t i : cl) used += h[i];
        float at = box.y + (box.height - used) / 2;
        for (size_t i : cl) { y[i] = at; at += h[i] + s.nodePad; }
    };
    for (auto& cl : column) stack(cl);
    // order each column by the mean height of its neighbours, a few sweeps each way
    for (int sweep = 0; sweep < 6; sweep++) {
        const bool forward = sweep % 2 == 0;
        for (int ci = forward ? 1 : cols - 2; forward ? ci < cols : ci >= 0; ci += forward ? 1 : -1) {
            auto& cl = column[(size_t)ci];
            std::vector<double> key(n, 0);
            for (size_t i : cl) {
                double wsum = 0, ysum = 0;
                for (const Link& l : links) {
                    const size_t other = forward ? (l.b == i ? l.a : n) : (l.a == i ? l.b : n);
                    if (other == n) continue;
                    wsum += l.v;
                    ysum += l.v * (y[other] + h[other] / 2);
                }
                key[i] = wsum > 0 ? ysum / wsum : y[i] + h[i] / 2;
            }
            std::stable_sort(cl.begin(), cl.end(), [&](size_t a, size_t b) { return key[a] < key[b]; });
            stack(cl);
        }
    }
    const float span = cols > 1 ? (box.width - s.nodeWidth) / (cols - 1) : 0;
    std::vector<float> x(n);
    for (size_t i = 0; i < n; i++) x[i] = cols > 1 ? box.x + span * col[i] : box.x + (box.width - s.nodeWidth) / 2;
    // where each ribbon leaves and enters: ordered by the other end's height
    std::vector<size_t> byOut(links.size()), byIn(links.size());
    std::iota(byOut.begin(), byOut.end(), size_t(0));
    std::iota(byIn.begin(), byIn.end(), size_t(0));
    std::stable_sort(byOut.begin(), byOut.end(), [&](size_t p, size_t q) { return y[links[p].b] < y[links[q].b]; });
    std::stable_sort(byIn.begin(), byIn.end(), [&](size_t p, size_t q) { return y[links[p].a] < y[links[q].a]; });
    std::vector<float> outAt(y), inAt(y);
    for (size_t li : byOut) { links[li].sy = outAt[links[li].a]; outAt[links[li].a] += (float)(links[li].v * ky); }
    for (size_t li : byIn) { links[li].ty = inAt[links[li].b]; inAt[links[li].b] += (float)(links[li].v * ky); }
    // timing: node column j grows during stage 2j, ribbons from it pour over the stages after
    const float pos = u * (2 * cols - 1);
    auto nodeColor = [&](size_t i) { return pick(s.palette, i); };
    for (const Link& l : links) {
        const int ca = col[l.a], cb = std::max(col[l.b], ca + 1);
        const float k = easeOut((pos - (2 * ca + 1)) / (float)(2 * (cb - ca) - 1));
        if (k <= 0) continue;
        const float x0 = x[l.a] + s.nodeWidth, x1 = x[l.b], th = (float)(l.v * ky), xm = (x0 + x1) / 2;
        auto bez = [&](float y0, float y1, float t) {
            const float m = 1 - t;
            return Vector2{ m * m * m * x0 + 3 * m * m * t * xm + 3 * m * t * t * xm + t * t * t * x1, y0 + (y1 - y0) * (3 * m * t * t + t * t * t) };
        };
        std::vector<Vector2> poly;
        const int steps = 40;
        for (int i = 0; i <= steps; i++) poly.push_back(bez(l.sy, l.ty, k * i / steps));
        for (int i = steps; i >= 0; i--) poly.push_back(bez(l.sy + th, l.ty + th, k * i / steps));
        vg::fill(c, vg::Path::polyline(poly, true), withAlpha(nodeColor(l.a), s.linkAlpha));
    }
    for (size_t i = 0; i < n; i++) {
        const float k = easeOut(pos - 2 * col[i]);
        if (k <= 0 || h[i] <= 0) continue;
        const float hh = h[i] * k;
        vg::fill(c, vg::Path::rect({ x[i], y[i] + (h[i] - hh) / 2, s.nodeWidth, std::max(hh, 1.f) }, 2), nodeColor(i));
        if (!s.labels) continue;
        std::string tex = textTex(names[i]);
        if (s.values) tex += "\\;\\;" + number(size[i], s.decimals);
        const bool left = cols > 1 && col[i] == cols - 1;
        label(c, tex, { left ? x[i] - 12 : x[i] + s.nodeWidth + 12, y[i] + h[i] / 2 }, { left ? 1.f : 0.f, 0.5f },
              std::min(s.textSize, std::max(h[i] + s.nodePad * 0.8f, 16.f)), withAlpha(s.textColor, k));
    }
}

void funnel(const Ctx& c, const Table& t, Rectangle box, const FunnelStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0 || t.series.empty()) return;
    const std::vector<double>& v = t.series[0].values;
    const size_t n = v.size();
    if (n == 0) return;
    double mx = 0;
    for (double x : v) mx = std::max(mx, x);
    if (mx <= 0) return;
    const float h = (box.height - s.gap * (n - 1)) / n, cx = box.x + box.width / 2;
    for (size_t i = 0; i < n; i++) {
        const float k = stagger(u, i, n, 0.4f);
        if (k <= 0) continue;
        const float top = box.width * (float)(std::max(0.0, v[i]) / mx) * k;
        const float bottom = i + 1 < n ? box.width * (float)(std::max(0.0, v[i + 1]) / mx) * k : top;
        const float y0 = box.y + i * (h + s.gap);
        const Color col = pick(s.palette, i);
        vg::fill(c, vg::Path::polyline({ { cx - top / 2, y0 }, { cx + top / 2, y0 }, { cx + bottom / 2, y0 + h }, { cx - bottom / 2, y0 + h } }, true),
                 withAlpha(col, std::min(1.f, k * 1.5f)));
        if (!s.labels) continue;
        const float ka = clamp01((k - 0.6f) * 2.5f);
        const std::string name = i < t.categories.size() ? textTex(t.categories[i]) : "";
        char pct[32];
        std::snprintf(pct, sizeof pct, "\\;(%.0f\\%%)", v[i] / std::max(v[0], 1e-12) * 100);
        const std::string val = number(v[i], s.decimals) + (s.percent && i > 0 ? pct : "");
        const float ts = std::min(s.textSize, h * 0.38f), vs = ts * 0.82f;
        const float room = std::min(top, bottom) - 24;
        const bool two = h > ts * 1.3f + vs * 1.4f;
        const float need = two ? std::max(textWidth(name, ts), textWidth(val, vs)) : textWidth(name, ts) + 20 + textWidth(val, vs);
        const bool inside = need < room;
        const Color tc = inside ? s.valueColor : s.textColor;
        const float mid = y0 + h / 2, lx = inside ? cx : cx + std::max(top, bottom) / 2 + 20;
        const float ax = inside ? 0.5f : 0.f;
        if (two) {
            label(c, name, { lx, mid - 2 }, { ax, 1 }, ts, withAlpha(tc, ka));
            label(c, val, { lx, mid + 4 }, { ax, 0 }, vs, withAlpha(tc, ka * 0.85f));
        } else label(c, name + "\\quad " + val, { lx, mid }, { ax, 0.5f }, ts, withAlpha(tc, ka));
    }
}

} } // rmtn::plot
