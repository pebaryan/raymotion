// plot.cpp -- plot:: axes, graphs, fields and charts (see rmtn/plot.hpp). Everything is
// built as vg:: paths in node px; numbers and labels are typeset with math::.
#include "rmtn/plot.hpp"
#include "rmtn/math.hpp"
#include "plot_util.hpp"
#include "rmtn/media.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace rmtn {
namespace plot {
namespace detail {

float clamp01(float v) { return std::clamp(v, 0.f, 1.f); }
float easeOut(float v) { v = clamp01(v); return 1 - (1 - v) * (1 - v) * (1 - v); }

// Decimals needed to write multiples of `step` exactly (up to 6).
int decimalsFor(double step) {
    for (int d = 0; d < 6; d++) {
        const double s = step * std::pow(10.0, d);
        if (std::fabs(s - std::round(s)) < 1e-6 * std::max(1.0, std::fabs(s))) return d;
    }
    return 6;
}
std::string number(double v, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
    std::string s = buf;
    if (s.find_first_not_of("-0.") == std::string::npos) s = decimals ? "0." + std::string(decimals, '0') : "0";
    return s;
}
// Plain text as TeX: \text{...} with the special characters escaped.
std::string textTex(const std::string& s) {
    std::string out = "\\text{";
    for (char ch : s) {
        if (std::string("{}$%&#_").find(ch) != std::string::npos) { out += '\\'; out += ch; }
        else if (ch == '\\' || ch == '^' || ch == '~') out += ' ';
        else out += ch;
    }
    return out + "}";
}

const math::Formula& formula(const std::string& tex, float size, Color color) {
    static std::mutex mu;
    static std::map<std::string, std::unique_ptr<math::Formula>> cache;
    char key[64];
    std::snprintf(key, sizeof key, "|%.2f|%02x%02x%02x", size, color.r, color.g, color.b);
    const std::string k = tex + key;
    std::lock_guard<std::mutex> lock(mu);
    auto it = cache.find(k);
    if (it == cache.end()) {
        math::Options o(size, Color{ color.r, color.g, color.b, 255 }, false);
        it = cache.emplace(k, std::make_unique<math::Formula>(math::Formula::tex(tex, o))).first;
    }
    return *it->second;
}

void arrowTip(const Ctx& c, Vector2 tip, Vector2 dir, float len, float halfWidth, Color col) {
    const Vector2 n{ -dir.y, dir.x }, base{ tip.x - dir.x * len, tip.y - dir.y * len };
    vg::fill(c, vg::Path::polyline({ tip, { base.x + n.x * halfWidth, base.y + n.y * halfWidth },
                                     { base.x - n.x * halfWidth, base.y - n.y * halfWidth } }, true), col);
}

// Liang-Barsky: clips the segment a-b to r; false when it misses.
bool clipSegment(Vector2& a, Vector2& b, Rectangle r) {
    float t0 = 0, t1 = 1;
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float p[4] = { -dx, dx, -dy, dy };
    const float q[4] = { a.x - r.x, r.x + r.width - a.x, a.y - r.y, r.y + r.height - a.y };
    for (int i = 0; i < 4; i++) {
        if (p[i] == 0) { if (q[i] < 0) return false; continue; }
        const float t = q[i] / p[i];
        if (p[i] < 0) t0 = std::max(t0, t);
        else t1 = std::min(t1, t);
        if (t0 > t1) return false;
    }
    const Vector2 a0 = a;
    a = { a0.x + dx * t0, a0.y + dy * t0 };
    b = { a0.x + dx * t1, a0.y + dy * t1 };
    return true;
}

// Polylines (runs) clipped to the box, joined into contours where they stay connected.
vg::Path clipRuns(const std::vector<std::vector<Vector2>>& runs, Rectangle box) {
    box = { box.x - 1, box.y - 1, box.width + 2, box.height + 2 };
    vg::Path out;
    for (const auto& run : runs) {
        bool open = false;
        Vector2 last{ 0, 0 };
        for (size_t i = 1; i < run.size(); i++) {
            Vector2 a = run[i - 1], b = run[i];
            if (!clipSegment(a, b, box)) { open = false; continue; }
            if (!open || std::fabs(a.x - last.x) > 1e-3f || std::fabs(a.y - last.y) > 1e-3f) { out.moveTo(a); open = true; }
            out.lineTo(b);
            last = b;
            if (std::fabs(b.x - run[i].x) > 1e-3f || std::fabs(b.y - run[i].y) > 1e-3f) open = false;   // left the box
        }
    }
    return out;
}

bool finite(double v) { return std::isfinite(v); }

// An automatic step widened (1, 2, 5, 10, ...) until ticks are at least minPx apart.
Range fitted(Range r, float px, float minPx) {
    if (r.step > 0 || px <= 0) return r;
    const double span = std::fabs(r.max - r.min);
    double step = r.tick();
    for (int i = 0; i < 30 && span > 0 && px * step / span < minPx; i++) {
        const double mag = std::pow(10.0, std::floor(std::log10(step) + 1e-9)), m = step / mag;
        step = (m < 1.5 ? 2 : m < 3.5 ? 5 : 10) * mag;
    }
    r.step = step;
    return r;
}

} // namespace detail
using namespace detail;

// ------------------------------------------------------------------ ranges and axes

double Range::tick() const {
    if (step > 0) return step;
    const double span = std::fabs(max - min);
    if (span <= 0) return 1;
    const double raw = span / 8, mag = std::pow(10.0, std::floor(std::log10(raw))), n = raw / mag;
    return (n < 1.5 ? 1 : n < 3.5 ? 2 : n < 7.5 ? 5 : 10) * mag;
}
std::vector<double> Range::ticks() const {
    std::vector<double> out;
    const double s = tick(), lo = std::min(min, max), hi = std::max(min, max);
    for (double k = std::ceil(lo / s - 1e-9); k * s <= hi + 1e-9 * s && out.size() < 1000; k++)
        out.push_back(std::fabs(k * s) < 1e-12 * s ? 0.0 : k * s);
    return out;
}

Vector2 Axes::map(double dx, double dy) const {
    const double xs = x.max - x.min, ys = y.max - y.min;
    const float px = box.x + (float)((dx - x.min) / (xs == 0 ? 1 : xs)) * box.width;
    const float py = ys == 0 ? box.y : box.y + box.height - (float)((dy - y.min) / ys) * box.height;
    return { px, py };
}
Vector2 Axes::unmap(Vector2 p) const {
    const double dx = x.min + (p.x - box.x) / std::max(box.width, 1e-6f) * (x.max - x.min);
    const double dy = box.height == 0 ? y.min : y.min + (box.y + box.height - p.y) / box.height * (y.max - y.min);
    return { (float)dx, (float)dy };
}
float Axes::sx() const { return x.max == x.min ? 0.f : box.width / (float)(x.max - x.min); }
float Axes::sy() const { return y.max == y.min ? 0.f : box.height / (float)(y.max - y.min); }
Vector2 Axes::origin() const {
    return map(std::clamp(0.0, std::min(x.min, x.max), std::max(x.min, x.max)),
               std::clamp(0.0, std::min(y.min, y.max), std::max(y.min, y.max)));
}

Axes numberLine(Vector2 left, float length, Range x) { return Axes({ left.x, left.y, length, 0 }, x, Range(0, 0)); }

void label(const Ctx& c, const std::string& tex, Vector2 at, Vector2 anchor, float size, Color color) {
    if (color.a == 0) return;
    math::Look look;
    look.alpha = color.a / 255.f;
    math::draw(c, formula(tex, size, color), at, anchor, look);
}

void axes(const Ctx& c, const Axes& given, const AxesStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0) return;
    Axes a = given;
    a.x = fitted(a.x, a.box.width, std::max(64.f, s.numberSize * 2.4f));
    a.y = fitted(a.y, a.box.height, std::max(40.f, s.numberSize * 1.5f));
    const bool flat = a.box.height == 0;
    const Vector2 o = a.origin();
    const float left = a.box.x, right = a.box.x + a.box.width, top = a.box.y, bottom = a.box.y + a.box.height;
    const float axisY = s.framed ? bottom : o.y, axisX = s.framed ? left : o.x;
    const float tipLen = s.arrows && !s.framed ? std::max(16.f, s.width * 5) : 0;
    const float over = tipLen > 0 ? tipLen * 0.8f : 0;         // the axes run a little past the box to their tips
    // grid
    vg::Path grid;
    if (s.xGrid) for (double t : a.x.ticks()) { const float px = a.map(t, 0).x; grid.moveTo({ px, top }).lineTo({ px, bottom }); }
    if (s.yGrid && !flat) for (double t : a.y.ticks()) { const float py = a.map(0, t).y; grid.moveTo({ left, py }).lineTo({ right, py }); }
    if (!grid.empty()) {
        vg::Stroke gs(withAlpha(s.gridColor, easeOut(u * 1.5f)), s.gridWidth);
        gs.cap = vg::Cap::Butt;
        vg::stroke(c, grid, gs);
    }
    vg::Stroke ls(s.color, s.width);
    ls.cap = vg::Cap::Butt;
    // An axis from p0 to p1 grows out of `from` (0..1 along it); returns how far it reaches.
    auto axis = [&](Vector2 p0, Vector2 p1, float from) {
        const vg::Path line = vg::Path::line(p0, p1);
        const float lo = from * (1 - u), hi = from + (1 - from) * u;
        vg::stroke(c, line.trim(lo, hi), ls);
        if (tipLen > 0 && hi > 0.98f) {
            const float len = std::hypot(p1.x - p0.x, p1.y - p0.y);
            const Vector2 dir{ (p1.x - p0.x) / len, (p1.y - p0.y) / len };
            arrowTip(c, { p1.x + dir.x * tipLen * 0.2f, p1.y + dir.y * tipLen * 0.2f }, dir, tipLen, tipLen * 0.42f, s.color);
        }
        return std::pair<float, float>{ lo, hi };
    };
    // How visible a feature at fraction f along an axis is, given the grown span.
    auto shown = [](float f, std::pair<float, float> span) {
        return clamp01(std::min(f - span.first, span.second - f) * 12 + 1);
    };
    const float xFrom = s.framed ? 0 : (o.x - left) / std::max(right + over - left, 1e-3f);
    const float yFrom = s.framed ? 0 : (bottom - o.y) / std::max(bottom - (top - over), 1e-3f);
    if (s.xAxis) {
        const auto span = axis({ left, axisY }, { right + over, axisY }, xFrom);
        const int dec = decimalsFor(a.x.tick());
        for (double t : a.x.ticks()) {
            if (s.skipZero && !s.framed && s.yAxis && !flat && t == 0) continue;
            const float px = a.map(t, 0).x, f = (px - left) / std::max(right + over - left, 1e-3f), k = shown(f, span);
            if (k <= 0) continue;
            if (s.ticks) vg::stroke(c, vg::Path::line({ px, axisY - s.tickLength / 2 }, { px, axisY + s.tickLength / 2 }),
                                    vg::Stroke(withAlpha(s.color, k), s.width * 0.8f));
            if (s.xNumbers)
                label(c, number(t, dec), { px, axisY + (s.ticks ? s.tickLength / 2 : 0) + 10 }, { 0.5f, 0 }, s.numberSize,
                      withAlpha(s.numberColor, k));
        }
        if (!s.xLabel.empty()) {
            const float k = clamp01((u - 0.7f) / 0.3f);
            if (s.framed) label(c, s.xLabel, { (left + right) / 2, bottom + s.numberSize * 1.6f + 24 }, { 0.5f, 0 }, s.labelSize, withAlpha(s.color, k));
            else label(c, s.xLabel, { right + over + 14, axisY }, { 0, 0.5f }, s.labelSize, withAlpha(s.color, k));
        }
    }
    if (s.yAxis && !flat) {
        const auto span = axis({ axisX, bottom }, { axisX, top - over }, yFrom);
        const int dec = decimalsFor(a.y.tick());
        float widest = 0;
        for (double t : a.y.ticks()) {
            if (s.skipZero && !s.framed && s.xAxis && t == 0) continue;
            const float py = a.map(0, t).y, f = (bottom - py) / std::max(bottom - (top - over), 1e-3f), k = shown(f, span);
            if (s.yNumbers) widest = std::max(widest, formula(number(t, dec), s.numberSize, s.numberColor).width);
            if (k <= 0) continue;
            if (s.ticks) vg::stroke(c, vg::Path::line({ axisX - s.tickLength / 2, py }, { axisX + s.tickLength / 2, py }),
                                    vg::Stroke(withAlpha(s.color, k), s.width * 0.8f));
            if (s.yNumbers)
                label(c, number(t, dec), { axisX - (s.ticks ? s.tickLength / 2 : 0) - 12, py }, { 1, 0.5f }, s.numberSize,
                      withAlpha(s.numberColor, k));
        }
        if (!s.yLabel.empty()) {
            const float k = clamp01((u - 0.7f) / 0.3f);
            if (s.framed) {                            // turned to run up the left edge
                Ctx r = c;
                r.o = mapP(c, { left - widest - 40, (top + bottom) / 2 });
                r.rot = c.rot - 90;
                label(r, s.yLabel, { 0, 0 }, { 0.5f, 1 }, s.labelSize, withAlpha(s.color, k));
            } else label(c, s.yLabel, { axisX, top - over - 12 }, { 0.5f, 1 }, s.labelSize, withAlpha(s.color, k));
        }
    }
}

// ------------------------------------------------------------------ graphs

vg::Path graph(const Axes& a, const Fn& f, double from, double to) {
    if (!std::isfinite(from)) from = a.x.min;
    if (!std::isfinite(to)) to = a.x.max;
    std::vector<std::vector<Vector2>> runs(1);
    const float jump = std::max(40.f, a.box.height * 0.5f);
    auto eval = [&](double x, double& y) { y = f(x); return finite(y); };
    // Adds the curve from (x0, y0) to (x1, y1), splitting until it is within half a pixel.
    std::function<void(double, double, double, double, int)> add = [&](double x0, double y0, double x1, double y1, int depth) {
        const Vector2 p0 = a.map(x0, y0), p1 = a.map(x1, y1);
        const double xm = (x0 + x1) / 2;
        double ym;
        const bool ok = eval(xm, ym);
        if (depth < 10) {
            if ((!ok || !finite(y0) || !finite(y1)) && (ok || finite(y0) || finite(y1))) {   // an edge of a hole: refine
                add(x0, y0, xm, ok ? ym : NAN, depth + 1);
                add(xm, ok ? ym : NAN, x1, y1, depth + 1);
                return;
            }
            const Vector2 pm = a.map(xm, ym), mid{ (p0.x + p1.x) / 2, (p0.y + p1.y) / 2 };
            if (std::hypot(pm.x - mid.x, pm.y - mid.y) > 0.5f || std::fabs(p1.y - p0.y) > jump) {
                add(x0, y0, xm, ym, depth + 1);
                add(xm, ym, x1, y1, depth + 1);
                return;
            }
        }
        if (!finite(y0) || !finite(y1) || std::fabs(p1.y - p0.y) > jump) {   // break the curve here
            if (!runs.back().empty()) runs.emplace_back();
            if (finite(y1)) runs.back().push_back(p1);
            return;
        }
        if (runs.back().empty()) runs.back().push_back(p0);
        runs.back().push_back(p1);
    };
    const int n = 64;
    double xPrev = from, yPrev;
    eval(from, yPrev);
    for (int i = 1; i <= n; i++) {
        const double x = from + (to - from) * i / n;
        double y;
        eval(x, y);
        add(xPrev, finite(yPrev) ? yPrev : NAN, x, finite(y) ? y : NAN, 0);
        xPrev = x;
        yPrev = y;
    }
    return clipRuns(runs, a.box);
}

vg::Path parametric(const Axes& a, const std::function<Vector2(double)>& p, double t0, double t1, int samples) {
    std::vector<std::vector<Vector2>> runs(1);
    samples = std::max(samples, 2);
    for (int i = 0; i <= samples; i++) {
        const Vector2 d = p(t0 + (t1 - t0) * i / samples);
        if (!std::isfinite(d.x) || !std::isfinite(d.y)) { if (!runs.back().empty()) runs.emplace_back(); continue; }
        runs.back().push_back(a.map(d));
    }
    return clipRuns(runs, a.box);
}

vg::Path polar(const Axes& a, const Fn& r, double t0, double t1, int samples) {
    return parametric(a, [&r](double t) { const double v = r(t); return Vector2{ (float)(v * std::cos(t)), (float)(v * std::sin(t)) }; },
                      t0, t1, samples);
}

vg::Path area(const Axes& a, const Fn& f, double from, double to, const Fn& g) {
    const int n = 300;
    const double ylo = std::min(a.y.min, a.y.max), yhi = std::max(a.y.min, a.y.max);
    auto at = [&](const Fn& fn, double x) {
        double y = fn ? fn(x) : 0.0;
        if (!finite(y)) y = 0;
        return a.map(x, std::clamp(y, ylo, yhi));
    };
    std::vector<Vector2> pts;
    for (int i = 0; i <= n; i++) pts.push_back(at(f, from + (to - from) * i / n));
    for (int i = n; i >= 0; i--) pts.push_back(at(g, from + (to - from) * i / n));
    return vg::Path::polyline(pts, true);
}

std::vector<vg::Path> riemann(const Axes& a, const Fn& f, double from, double to, int n, float at) {
    std::vector<vg::Path> out;
    const double w = (to - from) / std::max(n, 1);
    for (int i = 0; i < n; i++) {
        const double x0 = from + i * w, y = f(x0 + at * w);
        if (!finite(y)) continue;
        const Vector2 p = a.map(x0, 0), q = a.map(x0 + w, y);
        out.push_back(vg::Path::rect({ std::min(p.x, q.x), std::min(p.y, q.y), std::fabs(q.x - p.x), std::fabs(q.y - p.y) }));
    }
    return out;
}

vg::Path tangent(const Axes& a, const Fn& f, double x, float length) {
    const double h = std::max(1e-6, std::fabs(a.x.max - a.x.min) * 1e-5);
    const double y = f(x), slope = (f(x + h) - f(x - h)) / (2 * h);
    const Vector2 p = a.map(x, y);
    Vector2 d{ a.sx(), (float)(-slope * a.sy()) };
    const float len = std::hypot(d.x, d.y);
    if (!(len > 0) || !finite(y)) return {};
    d = { d.x / len * length / 2, d.y / len * length / 2 };
    return clipRuns({ { { p.x - d.x, p.y - d.y }, { p.x + d.x, p.y + d.y } } }, a.box);
}

void field(const Ctx& c, const Axes& a, const std::function<Vector2(Vector2)>& v, const FieldStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0) return;
    struct Arrow { Vector2 at, dir; float mag; };
    std::vector<Arrow> arrows;
    float maxMag = 0;
    const int nx = std::max(1, (int)(a.box.width / s.spacing)), ny = std::max(1, (int)(a.box.height / s.spacing));
    const float gx = a.box.width / nx, gy = a.box.height / ny;
    for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx; i++) {
            const Vector2 px{ a.box.x + (i + 0.5f) * gx, a.box.y + (j + 0.5f) * gy };
            const Vector2 d = v(a.unmap(px));
            const Vector2 sv{ d.x * a.sx(), -d.y * a.sy() };   // on screen
            const float m = std::hypot(sv.x, sv.y);
            if (!std::isfinite(m)) continue;
            arrows.push_back({ px, m > 0 ? Vector2{ sv.x / m, sv.y / m } : Vector2{ 0, 0 }, m });
            maxMag = std::max(maxMag, m);
        }
    if (maxMag <= 0) return;
    const float longest = s.maxLength > 0 ? s.maxLength : 0.85f * std::min(gx, gy);
    for (const Arrow& ar : arrows) {
        const float k = ar.mag / maxMag, len = longest * std::sqrt(k) * easeOut(u);
        if (len < 2) continue;
        const Color col = withAlpha(mixC(s.slow, s.fast, k), u);
        const float head = std::min(len * 0.45f, s.width * 4), hw = s.width * 1.7f, w = s.width / 2;
        const Vector2 d = ar.dir, n{ -d.y, d.x };
        const Vector2 tail{ ar.at.x - d.x * len / 2, ar.at.y - d.y * len / 2 }, tip{ ar.at.x + d.x * len / 2, ar.at.y + d.y * len / 2 };
        const Vector2 neck{ tip.x - d.x * head, tip.y - d.y * head };
        vg::fill(c, vg::Path::polyline({ { tail.x + n.x * w, tail.y + n.y * w }, { neck.x + n.x * w, neck.y + n.y * w },
                                         { neck.x + n.x * hw, neck.y + n.y * hw }, tip, { neck.x - n.x * hw, neck.y - n.y * hw },
                                         { neck.x - n.x * w, neck.y - n.y * w }, { tail.x - n.x * w, tail.y - n.y * w } }, true), col);
    }
}

void scatter(const Ctx& c, const Axes& a, const std::vector<Vector2>& pts, Color color, float radius, float u) {
    const float n = (float)pts.size();
    for (size_t i = 0; i < pts.size(); i++) {
        const float k = easeOut((u * (n + 4) - (float)i) / 5);
        if (k <= 0) break;
        const Vector2 p = a.map(pts[i]);
        if (p.x < a.box.x - 1 || p.x > a.box.x + a.box.width + 1 || p.y < a.box.y - 1 || p.y > a.box.y + a.box.height + 1) continue;
        vg::fill(c, vg::Path::circle(p, radius * k), withAlpha(color, k));
    }
}

// ------------------------------------------------------------------ tables

Table Table::parseCsv(const std::string& text) {
    auto cells = [](const std::string& line) {
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
    };
    Table t;
    std::istringstream in(text);
    std::string line;
    int row = 0;
    while (std::getline(in, line)) {
        row++;
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        const auto c = cells(line);
        if (t.series.empty()) {
            if (c.size() < 2) throw std::runtime_error("plot::Table: the CSV header needs a series column (line " + std::to_string(row) + ")");
            for (size_t i = 1; i < c.size(); i++) t.series.push_back({ c[i], {}, { 0, 0, 0, 0 } });
            continue;
        }
        t.categories.push_back(c[0]);
        for (size_t i = 0; i < t.series.size(); i++) {
            double v = 0;
            if (i + 1 < c.size() && !c[i + 1].empty()) {
                char* end = nullptr;
                v = std::strtod(c[i + 1].c_str(), &end);
                if (!end || *end) throw std::runtime_error("plot::Table: '" + c[i + 1] + "' is not a number (line " + std::to_string(row) + ")");
            }
            t.series[i].values.push_back(v);
        }
    }
    if (t.series.empty()) throw std::runtime_error("plot::Table: empty CSV");
    return t;
}

Table Table::csv(const std::string& file) {
    const std::string path = media::resolve(file);
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("plot::Table: cannot read " + path);
    std::stringstream ss;
    ss << in.rdbuf();
    return parseCsv(ss.str());
}

Table Table::mix(const Table& a, const Table& b, float u) {
    Table t = b;
    for (size_t s = 0; s < t.series.size() && s < a.series.size(); s++) {
        Series& o = t.series[s];
        const Series& p = a.series[s];
        for (size_t i = 0; i < o.values.size() && i < p.values.size(); i++) o.values[i] = p.values[i] + (o.values[i] - p.values[i]) * u;
        if (p.color.a && o.color.a) o.color = mixC(p.color, o.color, u);
    }
    return t;
}

Color palette(int i) {
    static const unsigned P[] = { 0x58c4dd, 0xfc6255, 0x83c167, 0xffd166, 0xc38fd8, 0xff8c42, 0x5cd0b3, 0xf08fb0 };
    return mkColor(P[((i % 8) + 8) % 8]);
}

namespace detail {

Color seriesColor(const Table& t, size_t i, const std::vector<Color>& pal) {
    if (i < t.series.size() && t.series[i].color.a) return t.series[i].color;
    return pal.empty() ? palette((int)i) : pal[i % pal.size()];
}

void categoryLabels(const Ctx& c, const std::vector<std::string>& names, const Axes& a, const ChartStyle& s, float u) {
    // shrunk (to 60% at most) so the widest name fits its slot
    const float slot = s.box.width / std::max<size_t>(names.size(), 1);
    float widest = 0;
    for (const std::string& n : names) widest = std::max(widest, formula(textTex(n), s.textSize, s.textColor).width);
    const float size = s.textSize * std::clamp((slot - 12) / std::max(widest, 1.f), 0.6f, 1.f);
    for (size_t i = 0; i < names.size(); i++) {
        const float k = clamp01(u * 3 - (float)i / std::max<size_t>(names.size(), 1));
        label(c, textTex(names[i]), { a.map(i + 0.5, 0).x, s.box.y + s.box.height + 14 }, { 0.5f, 0 }, size, withAlpha(s.textColor, k));
    }
}

void legend(const Ctx& c, const Table& t, const ChartStyle& s, float u) {
    if (!s.legend || t.series.size() < 2) return;
    float x = s.box.x;
    const float y = s.box.y - s.textSize * 1.4f, sq = s.textSize * 0.6f, k = clamp01(u * 2);
    for (size_t i = 0; i < t.series.size(); i++) {
        vg::fill(c, vg::Path::rect({ x, y - sq / 2, sq, sq }, sq * 0.2f), withAlpha(seriesColor(t, i, s.palette), k));
        const float fw = formula(textTex(t.series[i].name), s.textSize, s.textColor).width;
        label(c, textTex(t.series[i].name), { x + sq + 10, y }, { 0, 0.5f }, s.textSize, withAlpha(s.textColor, k));
        x += sq + 10 + fw + 36;
    }
}

} // namespace detail

Axes chartAxes(const Table& t, const ChartStyle& s) {
    Range y = s.y;
    if (y.min == y.max) {
        double lo = 0, hi = 0;
        for (size_t i = 0; i < t.categories.size(); i++) {
            double pos = 0, neg = 0;
            for (const Series& se : t.series) {
                const double v = i < se.values.size() ? se.values[i] : 0;
                if (s.stacked) (v >= 0 ? pos : neg) += v;
                else { hi = std::max(hi, v); lo = std::min(lo, v); }
            }
            if (s.stacked) { hi = std::max(hi, pos); lo = std::min(lo, neg); }
        }
        if (hi == lo) hi = lo + 1;
        const double step = Range(lo, hi).tick();
        y = Range(std::floor(lo / step + 1e-9) * step, std::ceil(hi / step - 1e-9) * step, s.y.step);
    }
    return Axes(s.box, Range(0, (double)std::max<size_t>(t.categories.size(), 1), 1), y);
}

void bars(const Ctx& c, const Table& t, const ChartStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0) return;
    const Axes a = chartAxes(t, s);
    axes(c, a, s.axes, clamp01(u * 3));
    categoryLabels(c, t.categories, a, s, u);
    legend(c, t, s, u);
    const size_t n = t.categories.size(), m = std::max<size_t>(t.series.size(), 1);
    const float slot = s.box.width / std::max<size_t>(n, 1), group = slot * (1 - s.gap);
    const int dec = s.decimals;
    for (size_t i = 0; i < n; i++) {
        const float start = 0.12f * (float)i / std::max<size_t>(n, 1);
        const float k = easeOut((u - start) / (1.f - start));
        if (k <= 0) continue;
        double pos = 0, neg = 0;
        for (size_t j = 0; j < t.series.size(); j++) {
            const double v = (i < t.series[j].values.size() ? t.series[j].values[i] : 0) * k;
            double base = 0;
            float x0 = s.box.x + i * slot + (slot - group) / 2, w = group;
            if (s.stacked) { base = v >= 0 ? pos : neg; (v >= 0 ? pos : neg) += v; }
            else { w = group / m; x0 += j * w; }
            const Vector2 p = a.map(0, base), q = a.map(0, base + v);
            const float y0 = std::min(p.y, q.y), h = std::fabs(q.y - p.y);
            const float inset = s.stacked || m == 1 ? 0 : std::min(3.f, w * 0.06f);
            if (h > 0.5f) vg::fill(c, vg::Path::rect({ x0 + inset, y0, w - 2 * inset, h }, std::min(4.f, std::min(w, h) / 3)),
                                   seriesColor(t, j, s.palette));
            if (s.values && !s.stacked) {
                const double full = i < t.series[j].values.size() ? t.series[j].values[i] : 0;
                label(c, number(full * k, dec), { x0 + w / 2, full >= 0 ? q.y - 8 : q.y + 8 }, { 0.5f, full >= 0 ? 1.f : 0.f },
                      s.textSize * (m > 2 ? 0.8f : 1.f), withAlpha(s.textColor, clamp01((k - 0.5f) * 2)));
            }
        }
        if (s.values && s.stacked && pos > 0)
            label(c, number(pos, dec), { s.box.x + i * slot + slot / 2, a.map(0, pos).y - 8 }, { 0.5f, 1 }, s.textSize,
                  withAlpha(s.textColor, clamp01((k - 0.5f) * 2)));
    }
}

void lines(const Ctx& c, const Table& t, const ChartStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0) return;
    const Axes a = chartAxes(t, s);
    axes(c, a, s.axes, clamp01(u * 3));
    categoryLabels(c, t.categories, a, s, u);
    legend(c, t, s, u);
    const float reveal = clamp01((u - 0.15f) / 0.85f);
    const size_t n = t.categories.size();
    for (size_t j = 0; j < t.series.size(); j++) {
        const Color col = seriesColor(t, j, s.palette);
        std::vector<Vector2> pts;
        for (size_t i = 0; i < n && i < t.series[j].values.size(); i++) pts.push_back(a.map(i + 0.5, t.series[j].values[i]));
        if (pts.empty()) continue;
        // draw-on by x, so every series moves together
        const float xEnd = pts.front().x + (pts.back().x - pts.front().x) * reveal;
        std::vector<Vector2> shown{ pts.front() };
        for (size_t i = 1; i < pts.size(); i++) {
            if (pts[i].x <= xEnd) { shown.push_back(pts[i]); continue; }
            const float f = (xEnd - pts[i - 1].x) / std::max(pts[i].x - pts[i - 1].x, 1e-3f);
            if (f > 0) shown.push_back({ pts[i - 1].x + (pts[i].x - pts[i - 1].x) * f, pts[i - 1].y + (pts[i].y - pts[i - 1].y) * f });
            break;
        }
        if (reveal > 0 && shown.size() > 1) {
            vg::Stroke st(col, s.lineWidth);
            st.join = vg::Join::Round;
            vg::stroke(c, vg::Path::polyline(shown), st);
        }
        for (size_t i = 0; i < pts.size(); i++) {
            const float k = easeOut((xEnd - pts[i].x) / 30 + 1);
            if (reveal <= 0 || k <= 0) continue;
            vg::fill(c, vg::Path::circle(pts[i], s.pointRadius * k), col);
            if (s.values && t.series.size() == 1)
                label(c, number(t.series[j].values[i], s.decimals), { pts[i].x, pts[i].y - s.pointRadius - 8 }, { 0.5f, 1 },
                      s.textSize * 0.85f, withAlpha(s.textColor, k));
        }
    }
}

void pie(const Ctx& c, const Table& t, Vector2 center, float radius, const PieStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0 || t.series.empty()) return;
    const std::vector<double>& v = t.series[0].values;
    double total = 0;
    for (double x : v) total += std::max(0.0, x);
    if (total <= 0) return;
    const float sweepEnd = s.startDeg + 360 * easeOut(u);
    double acc = 0;
    for (size_t i = 0; i < v.size(); i++) {
        const double val = std::max(0.0, v[i]);
        const float a0 = s.startDeg + (float)(acc / total * 360), a1 = s.startDeg + (float)((acc + val) / total * 360);
        acc += val;
        const float g = val > 0 && val < total ? s.gapDeg / 2 : 0;
        const float from = a0 + g, to = std::min(a1 - g, sweepEnd);
        if (to <= from) continue;
        const float mid = (a0 + a1) / 2 * DEG2RAD;
        Vector2 ctr = center;
        if ((int)i == s.explode) ctr = { center.x + std::cos(mid) * s.explodeBy, center.y + std::sin(mid) * s.explodeBy };
        const float rIn = radius * std::clamp(s.inner, 0.f, 0.95f);
        std::vector<Vector2> pts;
        const int steps = std::max(2, (int)((to - from) / 2));
        for (int k = 0; k <= steps; k++) {
            const float a = (from + (to - from) * k / steps) * DEG2RAD;
            pts.push_back({ ctr.x + std::cos(a) * radius, ctr.y + std::sin(a) * radius });
        }
        if (rIn > 0)
            for (int k = steps; k >= 0; k--) {
                const float a = (from + (to - from) * k / steps) * DEG2RAD;
                pts.push_back({ ctr.x + std::cos(a) * rIn, ctr.y + std::sin(a) * rIn });
            }
        else pts.push_back(ctr);
        const Color col = t.series[0].color.a && t.series.size() > 1 ? t.series[0].color
                        : s.palette.empty() ? palette((int)i) : s.palette[i % s.palette.size()];
        vg::fill(c, vg::Path::polyline(pts, true), col);
        if (!s.labels) continue;
        const float k = clamp01((sweepEnd - a1) / 20 + 1);   // once the sweep has passed the slice
        if (k <= 0) continue;
        const float rp = rIn > 0 ? (rIn + radius) / 2 : radius * 0.64f;
        char pct[32];
        std::snprintf(pct, sizeof pct, "%.0f\\%%", val / total * 100);
        if (val / total >= 0.04)
            label(c, pct, { ctr.x + std::cos(mid) * rp, ctr.y + std::sin(mid) * rp }, { 0.5f, 0.5f }, s.textSize * 0.9f,
                  withAlpha(s.valueColor, k));
        if (i < t.categories.size()) {
            const float cx = std::cos(mid), sy = std::sin(mid);
            label(c, textTex(t.categories[i]), { ctr.x + cx * (radius + 22), ctr.y + sy * (radius + 22) },
                  { cx > 0.25f ? 0.f : cx < -0.25f ? 1.f : 0.5f, sy > 0.25f ? 0.f : sy < -0.25f ? 1.f : 0.5f }, s.textSize,
                  withAlpha(s.textColor, k));
        }
    }
}

} } // rmtn::plot
