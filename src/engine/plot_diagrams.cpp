// plot_diagrams.cpp -- plot:: chord diagrams, contour maps, parallel coordinates and stream
// graphs (see rmtn/plot.hpp).
#include "plot_util.hpp"
#include <algorithm>
#include <cstdint>
#include <map>
#include <unordered_map>

namespace rmtn {
namespace plot {
using namespace detail;

// ------------------------------------------------------------------ chord diagrams

void chord(const Ctx& c, const std::vector<Flow>& flows, Vector2 center, float radius, const ChordStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0) return;
    std::vector<std::string> names;
    std::map<std::string, size_t> index;
    auto node = [&](const std::string& n) {
        auto it = index.find(n);
        if (it != index.end()) return it->second;
        index[n] = names.size();
        names.push_back(n);
        return names.size() - 1;
    };
    struct Link { size_t a, b; double v; float s0 = 0, s1 = 0, t0 = 0, t1 = 0; };
    std::vector<Link> links;
    for (const Flow& f : flows)
        if (f.value > 0 && f.from != f.to) links.push_back({ node(f.from), node(f.to), f.value });
    const size_t n = names.size();
    if (n == 0) return;
    std::vector<double> size(n, 0);
    double total = 0;
    for (const Link& l : links) { size[l.a] += l.v; size[l.b] += l.v; total += 2 * l.v; }
    const float pad = n > 1 ? s.padDeg : 0, span = 360 - pad * n;
    if (total <= 0 || span <= 0) return;
    // group arcs, then each group's share: what it sends (by target), then what it receives (by source)
    std::vector<float> g0(n), g1(n);
    float at = s.startDeg + pad / 2;
    for (size_t i = 0; i < n; i++) {
        g0[i] = at;
        g1[i] = at + span * (float)(size[i] / total);
        at = g1[i] + pad;
    }
    std::vector<float> cursor(g0);
    const float perUnit = span / (float)total;
    for (size_t g = 0; g < n; g++) {
        std::vector<size_t> out, in;
        for (size_t li = 0; li < links.size(); li++) {
            if (links[li].a == g) out.push_back(li);
            if (links[li].b == g) in.push_back(li);
        }
        // targets clockwise after this group come first, so ribbons don't cross at the arc
        auto rank = [&](size_t other) { return (other + n - g) % n; };
        std::stable_sort(out.begin(), out.end(), [&](size_t p, size_t q) { return rank(links[p].b) > rank(links[q].b); });
        std::stable_sort(in.begin(), in.end(), [&](size_t p, size_t q) { return rank(links[p].a) > rank(links[q].a); });
        for (size_t li : out) { links[li].s0 = cursor[g]; cursor[g] += (float)links[li].v * perUnit; links[li].s1 = cursor[g]; }
        for (size_t li : in) { links[li].t0 = cursor[g]; cursor[g] += (float)links[li].v * perUnit; links[li].t1 = cursor[g]; }
    }
    auto pt = [&](float deg, float r) { return Vector2{ center.x + std::cos(deg * DEG2RAD) * r, center.y + std::sin(deg * DEG2RAD) * r }; };
    auto arcTo = [&](vg::Path& p, float from, float to, float r) {
        const int steps = std::max(2, (int)std::fabs(to - from));
        for (int k = 1; k <= steps; k++) p.lineTo(pt(from + (to - from) * k / steps, r));
    };
    // ribbons first, under the arcs
    const float rib = radius - 2;
    for (size_t li = 0; li < links.size(); li++) {
        const float k = stagger(clamp01((u - 0.4f) / 0.6f), li, links.size(), 0.5f);
        if (k <= 0) continue;
        const Link& l = links[li];
        vg::Path p;
        p.moveTo(pt(l.s0, rib));
        arcTo(p, l.s0, l.s1, rib);
        p.quadTo(center, pt(l.t0, rib));
        arcTo(p, l.t0, l.t1, rib);
        p.quadTo(center, pt(l.s0, rib));
        p.close();
        vg::fill(c, p, withAlpha(pick(s.palette, l.a), s.ribbonAlpha * k));
    }
    const float sweep = s.startDeg + 360 * easeOut(u / 0.5f);
    for (size_t i = 0; i < n; i++) {
        const float to = std::min(g1[i], sweep);
        if (to <= g0[i]) continue;
        vg::fill(c, sector(center, radius, radius + s.arcWidth, g0[i], to), pick(s.palette, i));
        if (!s.labels) continue;
        const float k = clamp01((sweep - g1[i]) / 20 + 1), mid = (g0[i] + g1[i]) / 2 * DEG2RAD;
        const Vector2 d{ std::cos(mid), std::sin(mid) };
        const float r = radius + s.arcWidth + 14;
        label(c, textTex(names[i]), { center.x + d.x * r, center.y + d.y * r }, outward(d.x, d.y), s.textSize, withAlpha(s.textColor, k));
    }
}

// ------------------------------------------------------------------ contours

namespace {

// Clips a polygon with per-vertex values to v >= lo (keepAbove) or v <= hi.
void clipBy(std::vector<Vector2>& pts, std::vector<double>& vals, double level, bool keepAbove) {
    std::vector<Vector2> op;
    std::vector<double> ov;
    const size_t n = pts.size();
    for (size_t i = 0; i < n; i++) {
        const size_t j = (i + 1) % n;
        const bool inI = keepAbove ? vals[i] >= level : vals[i] <= level, inJ = keepAbove ? vals[j] >= level : vals[j] <= level;
        if (inI) { op.push_back(pts[i]); ov.push_back(vals[i]); }
        if (inI != inJ) {
            const float t = (float)((level - vals[i]) / (vals[j] - vals[i]));
            op.push_back({ pts[i].x + (pts[j].x - pts[i].x) * t, pts[i].y + (pts[j].y - pts[i].y) * t });
            ov.push_back(level);
        }
    }
    pts.swap(op);
    vals.swap(ov);
}

} // namespace

ContourMap contour(const Axes& a, const std::function<double(double, double)>& f, std::vector<double> levels, float cell) {
    ContourMap m;
    const Rectangle b = a.box;
    cell = std::max(cell, 1.f);
    const int nx = std::max(1, (int)std::ceil(b.width / cell)), ny = std::max(1, (int)std::ceil(b.height / cell));
    const float dx = b.width / nx, dy = b.height / ny;
    std::vector<double> v((size_t)(nx + 1) * (ny + 1));
    double lo = INFINITY, hi = -INFINITY;
    for (int j = 0; j <= ny; j++)
        for (int i = 0; i <= nx; i++) {
            const Vector2 d = a.unmap({ b.x + i * dx, b.y + j * dy });
            double x = f(d.x, d.y);
            if (!std::isfinite(x)) x = 0;
            v[(size_t)j * (nx + 1) + i] = x;
            lo = std::min(lo, x);
            hi = std::max(hi, x);
        }
    if (levels.empty()) {
        for (double t : Range(lo, hi).ticks())
            if (t > lo && t < hi) levels.push_back(t);
    }
    std::sort(levels.begin(), levels.end());
    m.levels = levels;
    m.box = b;
    auto V = [&](int i, int j) { return v[(size_t)j * (nx + 1) + i]; };
    auto P = [&](int i, int j) { return Vector2{ b.x + i * dx, b.y + j * dy }; };
    // ---- lines: marching squares, segments joined through the grid edges they cross
    for (double L : levels) {
        // edge ids: horizontal (i, j)-(i+1, j) even, vertical (i, j)-(i, j+1) odd
        auto hEdge = [&](int i, int j) { return (int64_t)((int64_t)j * (nx + 1) + i) * 2; };
        auto vEdge = [&](int i, int j) { return (int64_t)((int64_t)j * (nx + 1) + i) * 2 + 1; };
        std::unordered_map<int64_t, Vector2> where;
        std::vector<std::pair<int64_t, int64_t>> segs;
        auto cross = [&](int64_t id, int i0, int j0, int i1, int j1) {
            if (!where.count(id)) {
                const double a0 = V(i0, j0), a1 = V(i1, j1);
                const float t = a1 != a0 ? (float)std::clamp((L - a0) / (a1 - a0), 0.0, 1.0) : 0.5f;
                const Vector2 p = P(i0, j0), q = P(i1, j1);
                where[id] = { p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t };
            }
            return id;
        };
        for (int j = 0; j < ny; j++)
            for (int i = 0; i < nx; i++) {
                const double tl = V(i, j), tr = V(i + 1, j), br = V(i + 1, j + 1), bl = V(i, j + 1);
                const int cs = (tl > L) | (tr > L) << 1 | (br > L) << 2 | (bl > L) << 3;
                if (cs == 0 || cs == 15) continue;
                auto T = [&] { return cross(hEdge(i, j), i, j, i + 1, j); };
                auto R = [&] { return cross(vEdge(i + 1, j), i + 1, j, i + 1, j + 1); };
                auto B = [&] { return cross(hEdge(i, j + 1), i, j + 1, i + 1, j + 1); };
                auto Lf = [&] { return cross(vEdge(i, j), i, j, i, j + 1); };
                const bool centre = (tl + tr + br + bl) / 4 > L;
                switch (cs) {
                case 1: case 14: segs.push_back({ Lf(), T() }); break;
                case 2: case 13: segs.push_back({ T(), R() }); break;
                case 3: case 12: segs.push_back({ Lf(), R() }); break;
                case 4: case 11: segs.push_back({ R(), B() }); break;
                case 6: case 9: segs.push_back({ T(), B() }); break;
                case 7: case 8: segs.push_back({ Lf(), B() }); break;
                case 5:
                    if (centre) { segs.push_back({ T(), R() }); segs.push_back({ B(), Lf() }); }
                    else { segs.push_back({ Lf(), T() }); segs.push_back({ R(), B() }); }
                    break;
                case 10:
                    if (centre) { segs.push_back({ Lf(), T() }); segs.push_back({ R(), B() }); }
                    else { segs.push_back({ T(), R() }); segs.push_back({ B(), Lf() }); }
                    break;
                }
            }
        std::unordered_map<int64_t, std::vector<size_t>> at;
        for (size_t k = 0; k < segs.size(); k++) { at[segs[k].first].push_back(k); at[segs[k].second].push_back(k); }
        std::vector<bool> used(segs.size(), false);
        vg::Path path;
        auto walk = [&](size_t first, int64_t from) {
            std::vector<int64_t> chain{ from };
            size_t k = first;
            int64_t end = from;
            while (true) {
                used[k] = true;
                end = segs[k].first == end ? segs[k].second : segs[k].first;
                chain.push_back(end);
                size_t next = segs.size();
                for (size_t o : at[end]) if (!used[o]) { next = o; break; }
                if (next == segs.size()) break;
                k = next;
            }
            const bool closed = chain.size() > 3 && chain.front() == chain.back();
            if (closed) chain.pop_back();
            path.moveTo(where[chain[0]]);
            for (size_t q = 1; q < chain.size(); q++) path.lineTo(where[chain[q]]);
            if (closed) path.close();
        };
        // open chains start at the box edge (an id used once), then the loops
        for (size_t k = 0; k < segs.size(); k++)
            for (int64_t e : { segs[k].first, segs[k].second })
                if (!used[k] && at[e].size() == 1) walk(k, e);
        for (size_t k = 0; k < segs.size(); k++)
            if (!used[k]) walk(k, segs[k].first);
        m.lines.push_back(path);
    }
    // ---- bands: whole cells merged into runs; mixed cells split into triangles and clipped
    const size_t nb = levels.size() + 1;
    m.bands.assign(nb, vg::Path());
    auto band = [&](double x) { return (size_t)(std::upper_bound(levels.begin(), levels.end(), x) - levels.begin()); };
    for (int j = 0; j < ny; j++) {
        int runStart = -1;
        size_t runBand = 0;
        auto flush = [&](int iEnd) {
            if (runStart < 0) return;
            const Vector2 p = P(runStart, j);
            m.bands[runBand].append(vg::Path::rect({ p.x, p.y, (iEnd - runStart) * dx, dy }));
            runStart = -1;
        };
        for (int i = 0; i < nx; i++) {
            const double tl = V(i, j), tr = V(i + 1, j), br = V(i + 1, j + 1), bl = V(i, j + 1);
            const size_t b0 = band(tl);
            if (band(tr) == b0 && band(br) == b0 && band(bl) == b0) {
                if (runStart >= 0 && runBand != b0) flush(i);
                if (runStart < 0) { runStart = i; runBand = b0; }
                continue;
            }
            flush(i);
            const Vector2 ptl = P(i, j), ptr = P(i + 1, j), pbr = P(i + 1, j + 1), pbl = P(i, j + 1);
            const Vector2 tri[2][3] = { { ptl, ptr, pbr }, { ptl, pbr, pbl } };
            const double val[2][3] = { { tl, tr, br }, { tl, br, bl } };
            for (int k = 0; k < 2; k++) {
                const size_t lo3 = std::min({ band(val[k][0]), band(val[k][1]), band(val[k][2]) });
                const size_t hi3 = std::max({ band(val[k][0]), band(val[k][1]), band(val[k][2]) });
                for (size_t q = lo3; q <= hi3; q++) {
                    std::vector<Vector2> pts(tri[k], tri[k] + 3);
                    std::vector<double> vs(val[k], val[k] + 3);
                    if (q > 0) clipBy(pts, vs, levels[q - 1], true);
                    if (q < levels.size() && pts.size() >= 3) clipBy(pts, vs, levels[q], false);
                    if (pts.size() >= 3) m.bands[q].append(vg::Path::polyline(pts, true));
                }
            }
        }
        flush(nx);
    }
    return m;
}

void contours(const Ctx& c, const ContourMap& m, const ContourStyle& s, float u) {
    u = clamp01(u);
    if (u <= 0) return;
    const auto colors = s.colors ? s.colors : std::function<Color(float)>(viridis);
    const size_t nl = m.levels.size();
    if (s.fill) {
        const float k = easeOut((u - 0.35f) / 0.65f);
        if (k > 0)
            for (size_t b = 0; b < m.bands.size(); b++)
                if (!m.bands[b].empty()) vg::fill(c, m.bands[b], withAlpha(colors(nl ? (float)b / nl : 0.5f), s.fillAlpha * k));
    }
    const float draw = easeOut(u / 0.6f);
    int dec = s.decimals;
    if (dec < 0) {
        dec = 0;
        for (size_t i = 1; i < nl; i++) dec = std::max(dec, decimalsFor(m.levels[i] - m.levels[i - 1]));
    }
    std::vector<Vector2> placed;   // labels so far, kept apart
    for (size_t i = 0; i < nl && i < m.lines.size(); i++) {
        if (m.lines[i].empty()) continue;
        const Color col = s.fill ? s.lineColor : colors(nl > 1 ? (float)(i + 0.5f) / nl : 0.5f);
        vg::stroke(c, m.lines[i].trim(0, draw, true), vg::Stroke(col, s.width));
        if (!s.labels || draw < 1) continue;
        // on the longest contour of each level, a third of the way along
        vg::Path best;
        float longest = 0;
        for (const vg::Contour& ct : m.lines[i].contours) {
            vg::Path p;
            p.contours.push_back(ct);
            const float len = p.length();
            if (len > longest) { longest = len; best = p; }
        }
        if (longest < 200) continue;
        const std::string txt = number(m.levels[i], dec);
        const float w = textWidth(txt, s.textSize) + 10, h = s.textSize * 1.1f;
        // the point on it furthest from the labels already placed (and clear of the box edge)
        const Rectangle bx = m.box;
        Vector2 at = best.pointAt(0.33f);
        float far = -1;
        for (int q = 0; q < 32; q++) {
            const Vector2 p = best.pointAt((q + 0.5f) / 32);
            if (p.x - w / 2 < bx.x || p.x + w / 2 > bx.x + bx.width || p.y - h / 2 < bx.y || p.y + h / 2 > bx.y + bx.height) continue;
            float d = 1e9f;
            for (Vector2 o : placed) d = std::min(d, std::hypot(p.x - o.x, p.y - o.y));
            if (d > far) { far = d; at = p; }
        }
        placed.push_back(at);
        const float k = easeOut((u - 0.6f) / 0.2f);
        const Color under = s.fill ? colors(nl ? (float)i / nl : 0.5f) : Color{ 20, 23, 31, 255 };
        vg::fill(c, vg::Path::rect({ at.x - w / 2, at.y - h / 2, w, h }, h / 2), withAlpha(under, k));
        label(c, txt, at, { 0.5f, 0.5f }, s.textSize, withAlpha(s.fill ? onFill(under) : s.textColor, k));
    }
}

// ------------------------------------------------------------------ parallel coordinates

void parallel(const Ctx& c, const Table& t, Rectangle box, const ParallelStyle& s, float u) {
    u = clamp01(u);
    const size_t m = t.series.size(), n = t.categories.size();
    if (u <= 0 || m < 2) return;
    std::vector<Range> ranges;
    for (const Series& se : t.series) {
        double lo = INFINITY, hi = -INFINITY;
        for (double v : se.values) { lo = std::min(lo, v); hi = std::max(hi, v); }
        if (!(lo <= hi)) { lo = 0; hi = 1; }
        ranges.push_back(nice(lo, hi));
    }
    auto xAt = [&](size_t j) { return box.x + box.width * (float)j / (float)(m - 1); };
    auto yAt = [&](size_t j, double v) {
        const Range& r = ranges[j];
        return box.y + box.height * (float)(1 - (v - r.min) / (r.max - r.min));
    };
    // axes rise from the bottom, with their names and a few values
    const float k0 = easeOut(u * 2.5f);
    vg::Stroke ax(s.axisColor, 2);
    ax.cap = vg::Cap::Butt;
    for (size_t j = 0; j < m; j++) {
        const float x = xAt(j);
        vg::stroke(c, vg::Path::line({ x, box.y + box.height }, { x, box.y + box.height * (1 - k0) }), ax);
        label(c, textTex(t.series[j].name), { x, box.y - 18 }, { 0.5f, 1 }, s.textSize, withAlpha(s.textColor, k0));
        const Range r = fitted(Range(ranges[j].min, ranges[j].max), box.height, 70);
        for (double v : r.ticks()) {
            const float y = yAt(j, v);
            vg::stroke(c, vg::Path::line({ x - 6, y }, { x + 6, y }), vg::Stroke(withAlpha(s.axisColor, k0), 2));
            label(c, number(v, decimalsFor(r.tick())), { x - 10, y }, { 1, 0.5f }, s.textSize * 0.72f,
                  withAlpha(s.textColor, k0 * 0.75f));
        }
    }
    const float reveal = clamp01((u - 0.25f) / 0.75f);
    if (reveal <= 0) return;
    const float xEnd = box.x + box.width * reveal;
    const int by = s.colorBy >= 0 && (size_t)s.colorBy < m ? s.colorBy : -1;
    auto rowColor = [&](size_t i) {
        if (by < 0) return pick(s.palette, i);
        const Range& r = ranges[(size_t)by];
        const double v = i < t.series[(size_t)by].values.size() ? t.series[(size_t)by].values[i] : r.min;
        return (s.colors ? s.colors : std::function<Color(float)>(viridis))((float)((v - r.min) / (r.max - r.min)));
    };
    auto rowPath = [&](size_t i) {
        std::vector<Vector2> pts;
        for (size_t j = 0; j < m; j++) pts.push_back({ xAt(j), yAt(j, i < t.series[j].values.size() ? t.series[j].values[i] : ranges[j].min) });
        return upToX(pts, xEnd);
    };
    const bool focus = s.highlight >= 0 && (size_t)s.highlight < n;
    for (size_t i = 0; i < n; i++) {
        if (focus && (int)i == s.highlight) continue;
        const auto pts = rowPath(i);
        if (pts.size() < 2) continue;
        vg::stroke(c, vg::Path::polyline(pts), vg::Stroke(withAlpha(rowColor(i), s.lineAlpha * (focus ? 0.4f : 1)), s.lineWidth));
    }
    if (focus) {
        const auto pts = rowPath((size_t)s.highlight);
        if (pts.size() >= 2) vg::stroke(c, vg::Path::polyline(pts), vg::Stroke(rowColor((size_t)s.highlight), s.lineWidth * 2.2f));
    }
}

// ------------------------------------------------------------------ stream graphs

namespace {

// Catmull-Rom through evenly spaced points, `per` samples per span.
std::vector<Vector2> smoothLine(const std::vector<Vector2>& p, int per) {
    if (p.size() < 3) return p;
    std::vector<Vector2> out;
    for (size_t i = 0; i + 1 < p.size(); i++) {
        const Vector2 p0 = p[i ? i - 1 : 0], p1 = p[i], p2 = p[i + 1], p3 = p[std::min(i + 2, p.size() - 1)];
        for (int k = 0; k < per; k++) {
            const float t = (float)k / per, t2 = t * t, t3 = t2 * t;
            auto cr = [&](float a, float b, float cc, float d) {
                return 0.5f * (2 * b + (cc - a) * t + (2 * a - 5 * b + 4 * cc - d) * t2 + (3 * b - a - 3 * cc + d) * t3);
            };
            out.push_back({ cr(p0.x, p1.x, p2.x, p3.x), cr(p0.y, p1.y, p2.y, p3.y) });
        }
    }
    out.push_back(p.back());
    return out;
}

} // namespace

void stream(const Ctx& c, const Table& t, const ChartStyle& s, float u) {
    u = clamp01(u);
    const size_t n = t.categories.size(), m = t.series.size();
    if (u <= 0 || n < 2 || m == 0) return;
    std::vector<double> total(n, 0);
    double peak = 0;
    for (size_t i = 0; i < n; i++) {
        for (const Series& se : t.series) total[i] += std::max(0.0, i < se.values.size() ? se.values[i] : 0);
        peak = std::max(peak, total[i]);
    }
    if (peak <= 0) return;
    const Axes a(s.box, Range(0, (double)n, 1), Range(-peak * 0.55, peak * 0.55));
    categoryLabels(c, t.categories, a, s, u);
    // boundaries: from -total/2 upwards, one per series edge
    std::vector<std::vector<Vector2>> edge(m + 1);
    for (size_t i = 0; i < n; i++) {
        double y = -total[i] / 2;
        edge[0].push_back(a.map(i + 0.5, y));
        for (size_t j = 0; j < m; j++) {
            y += std::max(0.0, i < t.series[j].values.size() ? t.series[j].values[i] : 0);
            edge[j + 1].push_back(a.map(i + 0.5, y));
        }
    }
    for (auto& e : edge) e = smoothLine(e, 16);
    const float reveal = clamp01((u - 0.1f) / 0.9f);
    const float x0 = edge[0].front().x, xEnd = x0 + (edge[0].back().x - x0) * reveal;
    for (size_t j = 0; j < m; j++) {
        std::vector<Vector2> top = upToX(edge[j + 1], xEnd), bottom = upToX(edge[j], xEnd);
        if (top.size() < 2) continue;
        top.insert(top.end(), bottom.rbegin(), bottom.rend());
        vg::fill(c, vg::Path::polyline(top, true), seriesColor(t, j, s.palette));
    }
    // names at each stream's thickest point, once the reveal has passed it
    for (size_t j = 0; j < m; j++) {
        const std::string tex = textTex(t.series[j].name);
        const float w0 = textWidth(tex, s.textSize) + 16;   // width scales with the size
        size_t best = edge[j].size();
        float thick = 0;
        for (size_t q = 0; q < edge[j].size(); q++) {
            const float h = edge[j][q].y - edge[j + 1][q].y, half = w0 * std::min(1.f, h * 0.6f / s.textSize) / 2;
            if (h > thick && edge[j][q].x - half >= edge[j].front().x && edge[j][q].x + half <= edge[j].back().x) { thick = h; best = q; }
        }
        const float size = std::min(s.textSize, thick * 0.6f), w = w0 * size / s.textSize;
        if (best == edge[j].size() || size < 14) continue;
        const Vector2 at{ edge[j][best].x, (edge[j][best].y + edge[j + 1][best].y) / 2 };
        const float k = clamp01((xEnd - (at.x - w / 2)) / std::max(w, 1.f));   // in as the reveal crosses it
        label(c, tex, at, { 0.5f, 0.5f }, size, withAlpha(onFill(seriesColor(t, j, s.palette)), k));
    }
}

} } // rmtn::plot
