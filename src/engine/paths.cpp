// paths.cpp -- vector paths: building, SVG path data, flattening, measuring, trimming,
// fills, strokes, dashes, morphs and SVG drawings (see rmtn/paths.hpp).
#include "rmtn/paths.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/media.hpp"
#include "rmtn/render.hpp"
#include "nanosvg/nanosvg.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace rmtn {
namespace vg {
namespace {

constexpr float DEG = (float)(PI / 180.0);

Vector2 add(Vector2 a, Vector2 b) { return { a.x + b.x, a.y + b.y }; }
Vector2 sub(Vector2 a, Vector2 b) { return { a.x - b.x, a.y - b.y }; }
Vector2 mul(Vector2 a, float k) { return { a.x * k, a.y * k }; }
Vector2 lerp(Vector2 a, Vector2 b, float t) { return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t }; }
float dot(Vector2 a, Vector2 b) { return a.x * b.x + a.y * b.y; }
float cross(Vector2 a, Vector2 b) { return a.x * b.y - a.y * b.x; }
float len(Vector2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
float dist(Vector2 a, Vector2 b) { return len(sub(a, b)); }
Vector2 norm(Vector2 a) {
    const float l = len(a);
    return l > 1e-12f ? Vector2{ a.x / l, a.y / l } : Vector2{ 0, 0 };
}

// Every segment as a cubic from p0 (lines and quads are elevated exactly).
struct Cubic { Vector2 p0, p1, p2, p3; };
Cubic asCubic(Vector2 p0, const Seg& s) {
    switch (s.kind) {
        case Seg::Kind::Line: return { p0, lerp(p0, s.to, 1.f / 3), lerp(p0, s.to, 2.f / 3), s.to };
        case Seg::Kind::Quad: return { p0, lerp(p0, s.c1, 2.f / 3), lerp(s.to, s.c1, 2.f / 3), s.to };
        default: return { p0, s.c1, s.c2, s.to };
    }
}
Vector2 at(const Cubic& c, float t) {
    const float u = 1 - t;
    return { u * u * u * c.p0.x + 3 * u * u * t * c.p1.x + 3 * u * t * t * c.p2.x + t * t * t * c.p3.x,
             u * u * u * c.p0.y + 3 * u * u * t * c.p1.y + 3 * u * t * t * c.p2.y + t * t * t * c.p3.y };
}
Vector2 tangent(const Cubic& c, float t) {
    const float u = 1 - t;
    Vector2 d = add(add(mul(sub(c.p1, c.p0), 3 * u * u), mul(sub(c.p2, c.p1), 6 * u * t)), mul(sub(c.p3, c.p2), 3 * t * t));
    if (len(d) < 1e-9f) d = sub(c.p3, c.p0);       // degenerate ends (control point on the end point)
    return d;
}
void split(const Cubic& c, float t, Cubic& l, Cubic& r) {
    const Vector2 a = lerp(c.p0, c.p1, t), b = lerp(c.p1, c.p2, t), d = lerp(c.p2, c.p3, t);
    const Vector2 ab = lerp(a, b, t), bd = lerp(b, d, t), m = lerp(ab, bd, t);
    l = { c.p0, a, ab, m };
    r = { m, bd, d, c.p3 };
}
Cubic subCubic(const Cubic& c, float t0, float t1) {
    Cubic l = c, r;
    if (t1 < 1) split(c, t1, l, r);
    if (t0 <= 0 || t1 <= 0) return l;
    Cubic a, b;
    split(l, t0 / t1, a, b);
    return b;
}

float pointLineDist(Vector2 p, Vector2 a, Vector2 b) {
    const Vector2 ab = sub(b, a);
    const float l = len(ab);
    if (l < 1e-9f) return dist(p, a);
    return std::fabs(cross(ab, sub(p, a))) / l;
}

// Appends the points after p0 (and their parameters) within `tol` of the curve.
void flattenCubic(const Cubic& c, float t0, float t1, float tol, int depth, std::vector<Vector2>& out,
                  std::vector<float>* ts) {
    const float d = std::max(pointLineDist(c.p1, c.p0, c.p3), pointLineDist(c.p2, c.p0, c.p3));
    if (depth >= 18 || d <= tol) {
        out.push_back(c.p3);
        if (ts) ts->push_back(t1);
        return;
    }
    Cubic l, r;
    split(c, 0.5f, l, r);
    const float tm = (t0 + t1) / 2;
    flattenCubic(l, t0, tm, tol, depth + 1, out, ts);
    flattenCubic(r, tm, t1, tol, depth + 1, out, ts);
}
void flattenSeg(Vector2 p0, const Seg& s, float tol, std::vector<Vector2>& out, std::vector<float>* ts = nullptr) {
    if (s.kind == Seg::Kind::Line) {
        out.push_back(s.to);
        if (ts) ts->push_back(1);
        return;
    }
    flattenCubic(asCubic(p0, s), 0, 1, tol, 0, out, ts);
}

Vector2 segStart(const Contour& c, size_t i) { return i == 0 ? c.start : c.segs[i - 1].to; }

// Arc-length tables for measuring: per segment, cumulative length at sampled parameters.
struct SegTable { std::vector<float> ts, ls; float length = 0; };
struct ContourTable { std::vector<SegTable> segs; float length = 0; };

float measureTolerance(const Path& p) {
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    auto grow = [&](Vector2 v) { x0 = std::min(x0, v.x); y0 = std::min(y0, v.y); x1 = std::max(x1, v.x); y1 = std::max(y1, v.y); };
    for (const Contour& c : p.contours) {
        grow(c.start);
        for (const Seg& s : c.segs) { grow(s.c1); grow(s.c2); grow(s.to); }
    }
    if (x1 < x0) return 1e-3f;
    return std::max(1e-4f, std::hypot(x1 - x0, y1 - y0) * 2e-5f);
}

ContourTable measure(const Contour& c, float tol) {
    ContourTable t;
    for (size_t i = 0; i < c.segs.size(); i++) {
        SegTable st;
        std::vector<Vector2> pts{ segStart(c, i) };
        std::vector<float> ts{ 0 };
        flattenSeg(pts[0], c.segs[i], tol, pts, &ts);
        st.ts = ts;
        st.ls.push_back(0);
        for (size_t k = 1; k < pts.size(); k++) st.ls.push_back(st.ls.back() + dist(pts[k - 1], pts[k]));
        st.length = st.ls.back();
        t.length += st.length;
        t.segs.push_back(std::move(st));
    }
    return t;
}

// The segment and parameter at length `l` along a contour.
void locate(const ContourTable& t, float l, size_t& seg, float& param) {
    seg = 0;
    while (seg + 1 < t.segs.size() && l > t.segs[seg].length) { l -= t.segs[seg].length; seg++; }
    const SegTable& st = t.segs[seg];
    l = std::clamp(l, 0.f, st.length);
    const size_t k = (size_t)(std::upper_bound(st.ls.begin(), st.ls.end(), l) - st.ls.begin());
    if (k == 0) { param = 0; return; }
    if (k >= st.ls.size()) { param = 1; return; }
    const float span = st.ls[k] - st.ls[k - 1];
    const float f = span > 0 ? (l - st.ls[k - 1]) / span : 0;
    param = st.ts[k - 1] + (st.ts[k] - st.ts[k - 1]) * f;
}

Seg subSeg(Vector2 p0, const Seg& s, float t0, float t1) {
    if (s.kind == Seg::Kind::Line) return Seg{ Seg::Kind::Line, {}, {}, lerp(p0, s.to, t1) };
    const Cubic c = subCubic(asCubic(p0, s), t0, t1);
    return Seg{ Seg::Kind::Cubic, c.p1, c.p2, c.p3 };
}

Contour trimContour(const Contour& c, const ContourTable& t, float a, float b) {
    if (a <= 1e-6f * std::max(1.f, t.length) && b >= t.length * (1 - 1e-6f)) return c;
    size_t sa, sb;
    float ta, tb;
    locate(t, a, sa, ta);
    locate(t, b, sb, tb);
    Contour out;
    out.start = at(asCubic(segStart(c, sa), c.segs[sa]), ta);
    if (sa == sb) {
        out.segs.push_back(subSeg(segStart(c, sa), c.segs[sa], ta, tb));
        return out;
    }
    out.segs.push_back(subSeg(segStart(c, sa), c.segs[sa], ta, 1));
    for (size_t i = sa + 1; i < sb; i++) out.segs.push_back(c.segs[i]);
    out.segs.push_back(subSeg(segStart(c, sb), c.segs[sb], 0, tb));
    return out;
}

// ---- SVG path data
struct Lexer {
    const std::string& s;
    size_t i = 0;
    void skip() { while (i < s.size() && (std::isspace((unsigned char)s[i]) || s[i] == ',')) i++; }
    bool atNumber() {
        skip();
        return i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i] == '-' || s[i] == '+' || s[i] == '.');
    }
    [[noreturn]] void fail() const {
        throw std::runtime_error("vg::Path::parse: bad path data at offset " + std::to_string(i) + ": \"" +
                                 s.substr(i, 24) + "\"");
    }
    float number() {
        if (!atNumber()) fail();
        const size_t b = i;
        if (s[i] == '-' || s[i] == '+') i++;
        bool digits = false;
        while (i < s.size() && std::isdigit((unsigned char)s[i])) { i++; digits = true; }
        if (i < s.size() && s[i] == '.') {
            i++;
            while (i < s.size() && std::isdigit((unsigned char)s[i])) { i++; digits = true; }
        }
        if (!digits) { i = b; fail(); }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            size_t e = i + 1;
            if (e < s.size() && (s[e] == '-' || s[e] == '+')) e++;
            if (e < s.size() && std::isdigit((unsigned char)s[e])) {
                i = e;
                while (i < s.size() && std::isdigit((unsigned char)s[i])) i++;
            }
        }
        return std::strtof(s.substr(b, i - b).c_str(), nullptr);
    }
    bool flag() {                                       // arc flags may be written without separators
        skip();
        if (i < s.size() && (s[i] == '0' || s[i] == '1')) return s[i++] == '1';
        fail();
    }
    Vector2 point() { const float x = number(); return { x, number() }; }
};

// ---- drawing helpers
float drawTolerance(const Ctx& c) { return 0.2f / std::max(1e-6f, c.s * canvasScale()); }

std::vector<Polyline> mapped(const Ctx& c, std::vector<Polyline> polys) {
    for (Polyline& p : polys)
        for (Vector2& v : p.pts) v = mapP(c, v);
    return polys;
}

bool segmentsCross(Vector2 a, Vector2 b, Vector2 c, Vector2 d) {
    const float d1 = cross(sub(b, a), sub(c, a)), d2 = cross(sub(b, a), sub(d, a));
    const float d3 = cross(sub(d, c), sub(a, c)), d4 = cross(sub(d, c), sub(b, c));
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0)) && d1 != 0 && d2 != 0 && d3 != 0 && d4 != 0;
}
bool isSimple(const std::vector<Vector2>& p) {
    const size_t n = p.size();
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 2; j < n; j++) {
            if (i == 0 && j == n - 1) continue;         // adjacent through the closing edge
            if (segmentsCross(p[i], p[(i + 1) % n], p[j], p[(j + 1) % n])) return false;
        }
    return true;
}

const Ctx IDENTITY{ { 0, 0 }, 1, 0, 1 };

void circleFan(Vector2 c, float r, std::vector<Vector2>& tris) {
    const int n = std::clamp((int)(r * canvasScale() * 1.2f) + 8, 8, 72);
    for (int j = 0; j < n; j++) {
        const float a0 = 2 * (float)PI * j / n, a1 = 2 * (float)PI * (j + 1) / n;
        tris.push_back(c);
        tris.push_back({ c.x + r * std::cos(a0), c.y + r * std::sin(a0) });
        tris.push_back({ c.x + r * std::cos(a1), c.y + r * std::sin(a1) });
    }
}
void quad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, std::vector<Vector2>& tris) {
    tris.insert(tris.end(), { a, b, c, a, c, d });
}

// Triangles covering a stroke of half-width h along one polyline (screen coordinates).
// Pieces overlap; the caller draws them opaque or as a union, so overlaps never show.
void strokeTris(Polyline pl, float h, const Stroke& s, std::vector<Vector2>& tris) {
    std::vector<Vector2>& p = pl.pts;
    p.erase(std::unique(p.begin(), p.end(), [](Vector2 a, Vector2 b) { return dist(a, b) < 1e-4f; }), p.end());
    if (pl.closed && p.size() > 1 && dist(p.front(), p.back()) < 1e-4f) p.pop_back();
    const size_t n = p.size();
    if (n == 0) return;
    if (n == 1) {                                       // a zero-length dash: a dot or a square
        if (s.cap == Cap::Round) circleFan(p[0], h, tris);
        else if (s.cap == Cap::Square) quad({ p[0].x - h, p[0].y - h }, { p[0].x + h, p[0].y - h }, { p[0].x + h, p[0].y + h }, { p[0].x - h, p[0].y + h }, tris);
        return;
    }
    const bool closed = pl.closed && n > 2;
    const size_t segs = closed ? n : n - 1;
    for (size_t i = 0; i < segs; i++) {
        const Vector2 a = p[i], b = p[(i + 1) % n];
        const Vector2 d = norm(sub(b, a)), nn{ -d.y * h, d.x * h };
        quad(add(a, nn), add(b, nn), sub(b, nn), sub(a, nn), tris);
    }
    for (size_t i = closed ? 0 : 1; i < (closed ? n : n - 1); i++) {
        const Vector2 prev = p[(i + n - 1) % n], cur = p[i], next = p[(i + 1) % n];
        const Vector2 d0 = norm(sub(cur, prev)), d1 = norm(sub(next, cur));
        const float cr = cross(d0, d1), dt = dot(d0, d1);
        if (std::fabs(cr) < 1e-6f && dt > 0) continue;  // straight on
        if (s.join == Join::Round) { circleFan(cur, h, tris); continue; }
        const float side = cr > 0 ? -1.f : 1.f;          // the outer side of the turn
        const Vector2 n0{ -d0.y * h * side, d0.x * h * side }, n1{ -d1.y * h * side, d1.x * h * side };
        const Vector2 a = add(cur, n0), b = add(cur, n1);
        tris.insert(tris.end(), { cur, a, b });
        if (s.join == Join::Miter) {
            const Vector2 bis = norm(add(n0, n1));
            const float cosHalf = dot(bis, mul(n0, 1 / h));
            if (cosHalf > 1e-4f && 1 / cosHalf <= s.miterLimit) tris.insert(tris.end(), { a, add(cur, mul(bis, h / cosHalf)), b });
        }
    }
    if (!closed) {
        const Vector2 ends[2][2] = { { p[0], norm(sub(p[0], p[1])) }, { p[n - 1], norm(sub(p[n - 1], p[n - 2])) } };
        for (const auto& e : ends) {                    // e[1] points out of the line
            if (s.cap == Cap::Round) circleFan(e[0], h, tris);
            else if (s.cap == Cap::Square) {
                const Vector2 nn{ -e[1].y * h, e[1].x * h }, ext = mul(e[1], h);
                quad(add(e[0], nn), add(add(e[0], nn), ext), add(sub(e[0], nn), ext), sub(e[0], nn), tris);
            }
        }
    }
}

// Splits polylines into dashes (in path units). The pattern restarts on every contour.
std::vector<Polyline> dashed(const std::vector<Polyline>& in, std::vector<float> pattern, float offset) {
    if (pattern.size() % 2) pattern.insert(pattern.end(), pattern.begin(), pattern.end());
    float total = 0;
    for (float& d : pattern) { d = std::max(0.f, d); total += d; }
    if (total <= 0) return in;
    std::vector<Polyline> out;
    for (const Polyline& pl : in) {
        std::vector<Vector2> pts = pl.pts;
        if (pts.empty()) continue;
        if (pl.closed) pts.push_back(pts.front());
        float pos = std::fmod(offset, total);
        if (pos < 0) pos += total;
        size_t k = 0;
        while (pos >= pattern[k]) { pos -= pattern[k]; k = (k + 1) % pattern.size(); }
        float remaining = pattern[k] - pos;
        bool on = k % 2 == 0;
        Polyline cur;
        if (on) cur.pts.push_back(pts[0]);
        for (size_t i = 1; i < pts.size(); i++) {
            const Vector2 a = pts[i - 1], b = pts[i];
            const float L = dist(a, b);
            float t = 0;
            while (L - t > remaining) {
                t += remaining;
                const Vector2 q = lerp(a, b, L > 0 ? t / L : 0);
                if (on) { cur.pts.push_back(q); out.push_back(cur); cur.pts.clear(); }
                else cur.pts = { q };
                on = !on;
                k = (k + 1) % pattern.size();
                remaining = pattern[k];
            }
            remaining -= L - t;
            if (on) cur.pts.push_back(b);
        }
        if (on && !cur.pts.empty()) out.push_back(cur);
    }
    return out;
}

void drawTris(const std::vector<Vector2>& tris, Color k) {
    for (size_t i = 0; i + 2 < tris.size(); i += 3) {
        // raylib wants counter-clockwise triangles when culling is on (outside the canvas)
        if (cross(sub(tris[i + 1], tris[i]), sub(tris[i + 2], tris[i])) < 0) DrawTriangle(tris[i], tris[i + 1], tris[i + 2], k);
        else DrawTriangle(tris[i], tris[i + 2], tris[i + 1], k);
    }
}

// ---- morph helpers
float polyLength(const std::vector<Vector2>& p, bool closed) {
    float l = 0;
    for (size_t i = 1; i < p.size(); i++) l += dist(p[i - 1], p[i]);
    if (closed && p.size() > 1) l += dist(p.back(), p.front());
    return l;
}
float signedArea(const std::vector<Vector2>& p) {
    float a = 0;
    for (size_t i = 0; i < p.size(); i++) a += cross(p[i], p[(i + 1) % p.size()]);
    return a / 2;
}
std::vector<Vector2> resample(const std::vector<Vector2>& p, bool closed, int n) {
    std::vector<Vector2> pts = p;
    if (closed) pts.push_back(p.front());
    const float total = polyLength(p, closed);
    std::vector<Vector2> out;
    size_t seg = 1;
    float walked = 0;
    for (int i = 0; i < n; i++) {
        const float target = total * i / (closed ? n : std::max(1, n - 1));
        while (seg + 1 < pts.size() && walked + dist(pts[seg - 1], pts[seg]) < target) { walked += dist(pts[seg - 1], pts[seg]); seg++; }
        const float L = dist(pts[seg - 1], pts[seg]);
        out.push_back(lerp(pts[seg - 1], pts[seg], L > 0 ? std::clamp((target - walked) / L, 0.f, 1.f) : 0));
    }
    return out;
}
Vector2 centroid(const std::vector<Vector2>& p) {
    Vector2 c{ 0, 0 };
    for (Vector2 v : p) c = add(c, v);
    return p.empty() ? c : mul(c, 1.f / p.size());
}
float cost(const std::vector<Vector2>& a, const std::vector<Vector2>& b, size_t shift) {
    float s = 0;
    for (size_t i = 0; i < a.size(); i++) {
        const Vector2 d = sub(a[i], b[(i + shift) % b.size()]);
        s += d.x * d.x + d.y * d.y;
    }
    return s;
}

Color unpack(unsigned int c, float opacity) {
    return { (unsigned char)(c & 0xff), (unsigned char)((c >> 8) & 0xff), (unsigned char)((c >> 16) & 0xff),
             (unsigned char)std::clamp((float)((c >> 24) & 0xff) * opacity + 0.5f, 0.f, 255.f) };
}
Color paint(const NSVGpaint& p, float opacity) {
    if (p.type == NSVG_PAINT_COLOR) return unpack(p.color, opacity);
    if ((p.type == NSVG_PAINT_LINEAR_GRADIENT || p.type == NSVG_PAINT_RADIAL_GRADIENT) && p.gradient && p.gradient->nstops > 0) {
        float r = 0, g = 0, b = 0, a = 0;
        for (int i = 0; i < p.gradient->nstops; i++) {
            const Color s = unpack(p.gradient->stops[i].color, 1);
            r += s.r; g += s.g; b += s.b; a += s.a;
        }
        const float n = (float)p.gradient->nstops;
        return { (unsigned char)(r / n), (unsigned char)(g / n), (unsigned char)(b / n), (unsigned char)(a / n * opacity) };
    }
    return { 0, 0, 0, 0 };
}

} // namespace

// ------------------------------------------------------------------ building

Path& Path::moveTo(Vector2 p) {
    contours.push_back(Contour{ p, {}, false });
    return *this;
}
static Contour& current(Path& path, Vector2 fallback) {
    if (path.contours.empty()) path.moveTo(fallback);
    else if (path.contours.back().closed) path.moveTo(path.contours.back().start);   // SVG: continue from the closed start
    return path.contours.back();
}
static Vector2 currentPoint(const Path& path) {
    if (path.contours.empty()) return { 0, 0 };
    const Contour& c = path.contours.back();
    if (c.closed || c.segs.empty()) return c.start;
    return c.segs.back().to;
}
Path& Path::lineTo(Vector2 p) {
    current(*this, p).segs.push_back(Seg{ Seg::Kind::Line, {}, {}, p });
    return *this;
}
Path& Path::quadTo(Vector2 c, Vector2 p) {
    current(*this, c).segs.push_back(Seg{ Seg::Kind::Quad, c, {}, p });
    return *this;
}
Path& Path::cubicTo(Vector2 c1, Vector2 c2, Vector2 p) {
    current(*this, c1).segs.push_back(Seg{ Seg::Kind::Cubic, c1, c2, p });
    return *this;
}
Path& Path::arcTo(Vector2 radii, float rotationDeg, bool largeArc, bool sweep, Vector2 p) {
    const Vector2 p1 = currentPoint(*this);
    if (dist(p1, p) < 1e-9f) return *this;
    double rx = std::fabs(radii.x), ry = std::fabs(radii.y);
    if (rx < 1e-9 || ry < 1e-9) return lineTo(p);
    const double phi = rotationDeg * PI / 180, cs = std::cos(phi), sn = std::sin(phi);
    const double dx2 = (p1.x - p.x) / 2.0, dy2 = (p1.y - p.y) / 2.0;
    const double x1 = cs * dx2 + sn * dy2, y1 = -sn * dx2 + cs * dy2;
    const double lambda = x1 * x1 / (rx * rx) + y1 * y1 / (ry * ry);
    if (lambda > 1) { rx *= std::sqrt(lambda); ry *= std::sqrt(lambda); }
    const double num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
    const double den = rx * rx * y1 * y1 + ry * ry * x1 * x1;
    const double coef = (largeArc == sweep ? -1 : 1) * std::sqrt(std::max(0.0, den > 0 ? num / den : 0));
    const double cxp = coef * rx * y1 / ry, cyp = -coef * ry * x1 / rx;
    const double cx = cs * cxp - sn * cyp + (p1.x + p.x) / 2.0, cy = sn * cxp + cs * cyp + (p1.y + p.y) / 2.0;
    auto angle = [](double ux, double uy, double vx, double vy) { return std::atan2(ux * vy - uy * vx, ux * vx + uy * vy); };
    const double th1 = angle(1, 0, (x1 - cxp) / rx, (y1 - cyp) / ry);
    double dth = angle((x1 - cxp) / rx, (y1 - cyp) / ry, (-x1 - cxp) / rx, (-y1 - cyp) / ry);
    if (!sweep && dth > 0) dth -= 2 * PI;
    if (sweep && dth < 0) dth += 2 * PI;
    const int n = std::max(1, (int)std::ceil(std::fabs(dth) / (PI / 2) - 1e-9));
    const double delta = dth / n, k = 4.0 / 3.0 * std::tan(delta / 4);
    auto map = [&](double x, double y) { return Vector2{ (float)(cx + cs * rx * x - sn * ry * y), (float)(cy + sn * rx * x + cs * ry * y) }; };
    for (int i = 0; i < n; i++) {
        const double a0 = th1 + i * delta, a1 = a0 + delta;
        const Vector2 q1 = map(std::cos(a0) - k * std::sin(a0), std::sin(a0) + k * std::cos(a0));
        const Vector2 q2 = map(std::cos(a1) + k * std::sin(a1), std::sin(a1) - k * std::cos(a1));
        cubicTo(q1, q2, i == n - 1 ? p : map(std::cos(a1), std::sin(a1)));
    }
    return *this;
}
Path& Path::close() {
    if (contours.empty() || contours.back().closed) return *this;
    Contour& c = contours.back();
    if (!c.segs.empty() && dist(c.segs.back().to, c.start) > 1e-6f) c.segs.push_back(Seg{ Seg::Kind::Line, {}, {}, c.start });
    else if (!c.segs.empty()) c.segs.back().to = c.start;
    c.closed = true;
    return *this;
}
Path& Path::append(const Path& other) {
    contours.insert(contours.end(), other.contours.begin(), other.contours.end());
    return *this;
}
bool Path::empty() const {
    for (const Contour& c : contours)
        if (!c.segs.empty()) return false;
    return true;
}

// ------------------------------------------------------------------ shapes

Path Path::parse(const std::string& d) {
    Path p;
    Lexer lx{ d };
    char cmd = 0;
    Vector2 cur{ 0, 0 }, subStart{ 0, 0 }, lastCubic{ 0, 0 }, lastQuad{ 0, 0 };
    char prev = 0;
    while (true) {
        lx.skip();
        if (lx.i >= d.size()) break;
        const char ch = d[lx.i];
        if (std::isalpha((unsigned char)ch)) { cmd = ch; lx.i++; }
        else if (!cmd || !lx.atNumber()) lx.fail();
        const bool rel = std::islower((unsigned char)cmd) != 0;
        const Vector2 base = rel ? cur : Vector2{ 0, 0 };
        auto P = [&](Vector2 v) { return add(v, base); };
        switch (std::toupper((unsigned char)cmd)) {
            case 'M':
                cur = P(lx.point());
                p.moveTo(cur);
                subStart = cur;
                cmd = rel ? 'l' : 'L';                  // further pairs are lines
                break;
            case 'L': cur = P(lx.point()); p.lineTo(cur); break;
            case 'H': { const float x = lx.number(); cur = { rel ? cur.x + x : x, cur.y }; p.lineTo(cur); break; }
            case 'V': { const float y = lx.number(); cur = { cur.x, rel ? cur.y + y : y }; p.lineTo(cur); break; }
            case 'C': {
                const Vector2 c1 = P(lx.point()), c2 = P(lx.point()), e = P(lx.point());
                p.cubicTo(c1, c2, e);
                lastCubic = c2;
                cur = e;
                break;
            }
            case 'S': {
                const bool follows = prev == 'C' || prev == 'S';
                const Vector2 c1 = follows ? sub(mul(cur, 2), lastCubic) : cur;
                const Vector2 c2 = P(lx.point()), e = P(lx.point());
                p.cubicTo(c1, c2, e);
                lastCubic = c2;
                cur = e;
                break;
            }
            case 'Q': {
                const Vector2 c = P(lx.point()), e = P(lx.point());
                p.quadTo(c, e);
                lastQuad = c;
                cur = e;
                break;
            }
            case 'T': {
                const bool follows = prev == 'Q' || prev == 'T';
                const Vector2 c = follows ? sub(mul(cur, 2), lastQuad) : cur;
                const Vector2 e = P(lx.point());
                p.quadTo(c, e);
                lastQuad = c;
                cur = e;
                break;
            }
            case 'A': {
                const float rx = lx.number(), ry = lx.number(), rot = lx.number();
                const bool large = lx.flag(), sweep = lx.flag();
                const Vector2 e = P(lx.point());
                p.arcTo({ rx, ry }, rot, large, sweep, e);
                cur = e;
                break;
            }
            case 'Z':
                p.close();
                cur = subStart;
                break;
            default: lx.fail();
        }
        prev = (char)std::toupper((unsigned char)cmd);
        if (prev == 'Z') {
            lx.skip();
            if (lx.atNumber()) lx.fail();               // numbers cannot follow Z
        }
    }
    return p;
}

Path Path::line(Vector2 a, Vector2 b) { return Path().moveTo(a).lineTo(b); }
Path Path::polyline(const std::vector<Vector2>& pts, bool closed) {
    Path p;
    if (pts.empty()) return p;
    p.moveTo(pts[0]);
    for (size_t i = 1; i < pts.size(); i++) p.lineTo(pts[i]);
    if (closed) p.close();
    return p;
}
Path Path::smooth(const std::vector<Vector2>& pts, bool closed) {
    Path p;
    const size_t n = pts.size();
    if (n < 3) return polyline(pts, closed);
    auto P = [&](long i) {
        if (closed) return pts[(size_t)((i % (long)n + (long)n) % (long)n)];
        return pts[(size_t)std::clamp(i, 0L, (long)n - 1)];
    };
    p.moveTo(pts[0]);
    const long segs = closed ? (long)n : (long)n - 1;
    for (long i = 0; i < segs; i++) {
        const Vector2 c1 = add(P(i), mul(sub(P(i + 1), P(i - 1)), 1.f / 6));
        const Vector2 c2 = sub(P(i + 1), mul(sub(P(i + 2), P(i)), 1.f / 6));
        p.cubicTo(c1, c2, P(i + 1));
    }
    if (closed) p.close();
    return p;
}
Path Path::rect(Rectangle r, float radius) {
    radius = std::clamp(radius, 0.f, std::min(r.width, r.height) / 2);
    if (radius <= 0) return polyline({ { r.x, r.y }, { r.x + r.width, r.y }, { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } }, true);
    const Vector2 rr{ radius, radius };
    Path p;
    p.moveTo({ r.x + radius, r.y });
    p.lineTo({ r.x + r.width - radius, r.y }).arcTo(rr, 0, false, true, { r.x + r.width, r.y + radius });
    p.lineTo({ r.x + r.width, r.y + r.height - radius }).arcTo(rr, 0, false, true, { r.x + r.width - radius, r.y + r.height });
    p.lineTo({ r.x + radius, r.y + r.height }).arcTo(rr, 0, false, true, { r.x, r.y + r.height - radius });
    p.lineTo({ r.x, r.y + radius }).arcTo(rr, 0, false, true, { r.x + radius, r.y });
    return p.close();
}
Path Path::circle(Vector2 c, float r) { return ellipse(c, r, r); }
Path Path::ellipse(Vector2 c, float rx, float ry) {
    const float k = 0.5522847498f;
    Path p;
    p.moveTo({ c.x + rx, c.y });
    p.cubicTo({ c.x + rx, c.y + k * ry }, { c.x + k * rx, c.y + ry }, { c.x, c.y + ry });
    p.cubicTo({ c.x - k * rx, c.y + ry }, { c.x - rx, c.y + k * ry }, { c.x - rx, c.y });
    p.cubicTo({ c.x - rx, c.y - k * ry }, { c.x - k * rx, c.y - ry }, { c.x, c.y - ry });
    p.cubicTo({ c.x + k * rx, c.y - ry }, { c.x + rx, c.y - k * ry }, { c.x + rx, c.y });
    return p.close();
}
Path Path::arc(Vector2 c, float r, float fromDeg, float toDeg) {
    Path p;
    const float span = toDeg - fromDeg;
    const int n = std::max(1, (int)std::ceil(std::fabs(span) / 90 - 1e-6f));
    const float d = span / n * DEG, k = 4.f / 3 * std::tan(d / 4);
    float a = fromDeg * DEG;
    p.moveTo({ c.x + r * std::cos(a), c.y + r * std::sin(a) });
    for (int i = 0; i < n; i++, a += d) {
        const float b = a + d;
        p.cubicTo({ c.x + r * (std::cos(a) - k * std::sin(a)), c.y + r * (std::sin(a) + k * std::cos(a)) },
                  { c.x + r * (std::cos(b) + k * std::sin(b)), c.y + r * (std::sin(b) - k * std::cos(b)) },
                  { c.x + r * std::cos(b), c.y + r * std::sin(b) });
    }
    return p;
}
Path Path::regular(Vector2 c, float r, int sides, float rotationDeg) {
    std::vector<Vector2> pts;
    for (int i = 0; i < std::max(3, sides); i++) {
        const float a = (rotationDeg + 360.f * i / std::max(3, sides)) * DEG;
        pts.push_back({ c.x + r * std::cos(a), c.y + r * std::sin(a) });
    }
    return polyline(pts, true);
}
Path Path::star(Vector2 c, float rOuter, float rInner, int points, float rotationDeg) {
    std::vector<Vector2> pts;
    points = std::max(2, points);
    for (int i = 0; i < points * 2; i++) {
        const float a = (rotationDeg + 180.f * i / points) * DEG, r = i % 2 ? rInner : rOuter;
        pts.push_back({ c.x + r * std::cos(a), c.y + r * std::sin(a) });
    }
    return polyline(pts, true);
}

// ------------------------------------------------------------------ transforms

Path Path::transformed(const std::function<Vector2(Vector2)>& f) const {
    Path p = *this;
    for (Contour& c : p.contours) {
        c.start = f(c.start);
        for (Seg& s : c.segs) {
            s.to = f(s.to);
            if (s.kind != Seg::Kind::Line) s.c1 = f(s.c1);
            if (s.kind == Seg::Kind::Cubic) s.c2 = f(s.c2);
        }
    }
    return p;
}
Path Path::translated(Vector2 d) const { return transformed([d](Vector2 v) { return add(v, d); }); }
Path Path::scaled(float sx, float sy, Vector2 o) const {
    return transformed([=](Vector2 v) { return Vector2{ o.x + (v.x - o.x) * sx, o.y + (v.y - o.y) * sy }; });
}
Path Path::rotated(float deg, Vector2 o) const {
    const float cs = std::cos(deg * DEG), sn = std::sin(deg * DEG);
    return transformed([=](Vector2 v) {
        const Vector2 d = sub(v, o);
        return Vector2{ o.x + d.x * cs - d.y * sn, o.y + d.x * sn + d.y * cs };
    });
}
Path Path::reversed() const {
    Path p;
    for (const Contour& c : contours) {
        Contour r;
        r.closed = c.closed;
        r.start = c.segs.empty() ? c.start : c.segs.back().to;
        for (size_t i = c.segs.size(); i-- > 0;) {
            const Seg& s = c.segs[i];
            Seg q = s;
            q.to = segStart(c, i);
            if (s.kind == Seg::Kind::Cubic) { q.c1 = s.c2; q.c2 = s.c1; }
            r.segs.push_back(q);
        }
        p.contours.push_back(r);
    }
    return p;
}

// ------------------------------------------------------------------ measuring

std::vector<Polyline> Path::flatten(float tolerance) const {
    std::vector<Polyline> out;
    tolerance = std::max(tolerance, 1e-5f);
    for (const Contour& c : contours) {
        Polyline pl;
        pl.closed = c.closed;
        pl.pts.push_back(c.start);
        for (size_t i = 0; i < c.segs.size(); i++) flattenSeg(segStart(c, i), c.segs[i], tolerance, pl.pts);
        if (pl.closed && pl.pts.size() > 1 && dist(pl.pts.front(), pl.pts.back()) < 1e-6f) pl.pts.pop_back();
        out.push_back(std::move(pl));
    }
    return out;
}
Rectangle Path::bounds() const {
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    for (const Polyline& pl : flatten(measureTolerance(*this) * 10))
        for (Vector2 v : pl.pts) { x0 = std::min(x0, v.x); y0 = std::min(y0, v.y); x1 = std::max(x1, v.x); y1 = std::max(y1, v.y); }
    if (x1 < x0) return { 0, 0, 0, 0 };
    return { x0, y0, x1 - x0, y1 - y0 };
}
float Path::length() const {
    const float tol = measureTolerance(*this);
    float l = 0;
    for (const Contour& c : contours) l += measure(c, tol).length;
    return l;
}

// The contour, segment and parameter at fraction u of the whole length.
static bool find(const Path& p, float u, size_t& ci, size_t& si, float& t) {
    const float tol = measureTolerance(p);
    std::vector<ContourTable> tables;
    float total = 0;
    for (const Contour& c : p.contours) { tables.push_back(measure(c, tol)); total += tables.back().length; }
    float l = std::clamp(u, 0.f, 1.f) * total;
    for (ci = 0; ci < tables.size(); ci++) {
        if (p.contours[ci].segs.empty()) continue;
        if (l <= tables[ci].length || ci + 1 == tables.size()) {
            locate(tables[ci], std::min(l, tables[ci].length), si, t);
            return true;
        }
        l -= tables[ci].length;
    }
    // trailing empty contours: fall back to the last contour with segments
    for (ci = p.contours.size(); ci-- > 0;)
        if (!p.contours[ci].segs.empty()) { si = p.contours[ci].segs.size() - 1; t = 1; return true; }
    return false;
}
Vector2 Path::pointAt(float u) const {
    size_t ci, si;
    float t;
    if (!find(*this, u, ci, si, t)) return contours.empty() ? Vector2{ 0, 0 } : contours[0].start;
    return at(asCubic(segStart(contours[ci], si), contours[ci].segs[si]), t);
}
float Path::angleAt(float u) const {
    size_t ci, si;
    float t;
    if (!find(*this, u, ci, si, t)) return 0;
    const Vector2 d = tangent(asCubic(segStart(contours[ci], si), contours[ci].segs[si]), t);
    return std::atan2(d.y, d.x) / DEG;
}
Path Path::trim(float from, float to, bool eachContour) const {
    from = std::clamp(from, 0.f, 1.f);
    to = std::clamp(to, 0.f, 1.f);
    Path out;
    if (to <= from) return out;
    const float tol = measureTolerance(*this);
    std::vector<ContourTable> tables;
    float total = 0;
    for (const Contour& c : contours) { tables.push_back(measure(c, tol)); total += tables.back().length; }
    float offset = 0;
    for (size_t i = 0; i < contours.size(); i++) {
        const float L = tables[i].length;
        if (contours[i].segs.empty() || L <= 0) { offset += L; continue; }
        float a, b;
        if (eachContour) { a = from * L; b = to * L; }
        else { a = std::max(from * total - offset, 0.f); b = std::min(to * total - offset, L); }
        offset += L;
        if (b - a <= 1e-7f * std::max(1.f, L)) continue;
        out.contours.push_back(trimContour(contours[i], tables[i], a, b));
    }
    return out;
}

// ------------------------------------------------------------------ drawing

void fill(const Ctx& c, const Path& p, Color color, FillRule rule) {
    const Color k = dr::TA(c, color);
    if (k.a == 0) return;
    std::vector<Polyline> polys = mapped(c, p.flatten(drawTolerance(c)));
    polys.erase(std::remove_if(polys.begin(), polys.end(), [](const Polyline& pl) { return pl.pts.size() < 3; }), polys.end());
    if (polys.empty()) return;
    if (polys.size() == 1 && polys[0].pts.size() <= 256 && isSimple(polys[0].pts)) {
        dr::fillPoly(IDENTITY, polys[0].pts.data(), (int)polys[0].pts.size(), k);
        return;
    }
    // A fan per contour: the signed triangles sum to each point's winding number.
    std::vector<Vector2> plus, minus;
    for (const Polyline& pl : polys) {
        const Vector2 o = pl.pts[0];
        for (size_t i = 1; i + 1 < pl.pts.size(); i++) {
            const float a = cross(sub(pl.pts[i], o), sub(pl.pts[i + 1], o));
            if (a == 0) continue;
            std::vector<Vector2>& dst = a > 0 ? plus : minus;
            dst.insert(dst.end(), { o, pl.pts[i], pl.pts[i + 1] });
        }
    }
    if (fillWinding(plus, minus, k, rule == FillRule::EvenOdd)) return;
    for (const Polyline& pl : polys) dr::fillPoly(IDENTITY, pl.pts.data(), (int)pl.pts.size(), k);   // no canvas: holes not cut
}

void stroke(const Ctx& c, const Path& p, const Stroke& s) {
    const Color k = dr::TA(c, s.color);
    if (k.a == 0 || s.width <= 0) return;
    std::vector<Polyline> polys = p.flatten(drawTolerance(c));
    if (!s.dash.empty()) polys = dashed(polys, s.dash, s.dashOffset);
    const float h = std::max(s.width * c.s / 2, 0.35f);
    std::vector<Vector2> tris;
    for (const Polyline& pl : mapped(c, std::move(polys))) strokeTris(pl, h, s, tris);
    if (tris.empty()) return;
    if (k.a == 255 || !fillWinding(tris, {}, k, false)) drawTris(tris, k);
}

void draw(const Ctx& c, const Path& p, const Style& s) {
    if (s.fill.a > 0) fill(c, p, s.fill, s.rule);
    if (s.stroke.width > 0 && s.stroke.color.a > 0) stroke(c, p, s.stroke);
}

void write(const Ctx& c, const Path& p, const Style& s, float u) {
    u = std::clamp(u, 0.f, 1.f);
    if (u <= 0) return;
    const bool hasStroke = s.stroke.width > 0 && s.stroke.color.a > 0;
    const float drawn = std::min(1.f, u / 0.6f);
    float fade = std::clamp((u - 0.5f) / 0.5f, 0.f, 1.f);
    fade = fade * fade * (3 - 2 * fade);
    if (s.fill.a > 0 && fade > 0) fill(c, p, withAlpha(s.fill, fade), s.rule);
    Stroke o = s.stroke;
    if (!hasStroke) {
        if (s.fill.a == 0) return;
        o.color = withAlpha(s.fill, 1 - fade);
        o.width = 2.f / std::max(c.s, 1e-4f);           // two output px
    }
    stroke(c, drawn >= 1 ? p : p.trim(0, drawn, true), o);
}

// ------------------------------------------------------------------ morphing

Morph::Morph(const Path& from, const Path& to, int samples) {
    samples = std::max(8, samples);
    struct Item { std::vector<Vector2> pts; bool closed; float key; };
    auto items = [](const Path& p) {
        std::vector<Item> v;
        for (Polyline& pl : p.flatten(measureTolerance(p) * 20)) {
            if (pl.pts.size() < 2) continue;
            const float key = (pl.closed ? std::fabs(signedArea(pl.pts)) : 0) + polyLength(pl.pts, pl.closed) * 1e-3f;
            v.push_back({ std::move(pl.pts), pl.closed, key });
        }
        std::stable_sort(v.begin(), v.end(), [](const Item& a, const Item& b) { return a.key > b.key; });
        return v;
    };
    const std::vector<Item> A = items(from), B = items(to);
    for (size_t i = 0; i < std::max(A.size(), B.size()); i++) {
        Pair pr;
        if (i < A.size()) { pr.a = resample(A[i].pts, A[i].closed, samples); pr.closedA = A[i].closed; }
        if (i < B.size()) { pr.b = resample(B[i].pts, B[i].closed, samples); pr.closedB = B[i].closed; }
        if (pr.a.empty()) { pr.a.assign((size_t)samples, centroid(pr.b)); pr.closedA = pr.closedB; }
        else if (pr.b.empty()) { pr.b.assign((size_t)samples, centroid(pr.a)); pr.closedB = pr.closedA; }
        else if (pr.closedA && pr.closedB) {
            if ((signedArea(pr.a) > 0) != (signedArea(pr.b) > 0)) std::reverse(pr.b.begin(), pr.b.end());
            size_t best = 0;
            float bestCost = 1e30f;
            for (size_t k = 0; k < pr.b.size(); k++) {
                const float cst = cost(pr.a, pr.b, k);
                if (cst < bestCost) { bestCost = cst; best = k; }
            }
            std::rotate(pr.b.begin(), pr.b.begin() + (long)best, pr.b.end());
        } else {
            std::vector<Vector2> rev(pr.b.rbegin(), pr.b.rend());
            if (cost(pr.a, rev, 0) < cost(pr.a, pr.b, 0)) pr.b = rev;
        }
        pairs_.push_back(std::move(pr));
    }
}

Path Morph::at(float u) const {
    Path p;
    for (const Pair& pr : pairs_) {
        std::vector<Vector2> pts(pr.a.size());
        for (size_t i = 0; i < pts.size(); i++) pts[i] = lerp(pr.a[i], pr.b[i], u);
        p.append(Path::polyline(pts, u < 0.5f ? pr.closedA : pr.closedB));
    }
    return p;
}

Style mix(const Style& a, const Style& b, float u) {
    Style s = u < 0.5f ? a : b;
    s.fill = mixC(a.fill, b.fill, u);
    s.stroke.color = mixC(a.stroke.color, b.stroke.color, u);
    s.stroke.width = a.stroke.width + (b.stroke.width - a.stroke.width) * u;
    return s;
}

// ------------------------------------------------------------------ SVG drawings

Drawing Drawing::parse(const std::string& svgText) {
    std::vector<char> buf(svgText.begin(), svgText.end());
    buf.push_back('\0');
    NSVGimage* img = nsvgParse(buf.data(), "px", 96);
    if (!img) throw std::runtime_error("vg::Drawing: could not parse the SVG");
    Drawing d;
    d.width = img->width;
    d.height = img->height;
    for (NSVGshape* sh = img->shapes; sh; sh = sh->next) {
        if (!(sh->flags & NSVG_FLAGS_VISIBLE)) continue;
        Shape s;
        s.id = sh->id;
        for (NSVGpath* np = sh->paths; np; np = np->next) {
            s.path.moveTo({ np->pts[0], np->pts[1] });
            for (int i = 0; i < np->npts - 1; i += 3) {
                const float* q = &np->pts[i * 2];
                s.path.cubicTo({ q[2], q[3] }, { q[4], q[5] }, { q[6], q[7] });
            }
            if (np->closed) s.path.close();
        }
        s.style.fill = paint(sh->fill, sh->opacity);
        s.style.rule = sh->fillRule == NSVG_FILLRULE_EVENODD ? FillRule::EvenOdd : FillRule::NonZero;
        s.style.stroke.color = paint(sh->stroke, sh->opacity);
        s.style.stroke.width = sh->stroke.type == NSVG_PAINT_NONE ? 0 : sh->strokeWidth;
        s.style.stroke.join = sh->strokeLineJoin == NSVG_JOIN_ROUND ? Join::Round : sh->strokeLineJoin == NSVG_JOIN_BEVEL ? Join::Bevel : Join::Miter;
        s.style.stroke.cap = sh->strokeLineCap == NSVG_CAP_ROUND ? Cap::Round : sh->strokeLineCap == NSVG_CAP_SQUARE ? Cap::Square : Cap::Butt;
        s.style.stroke.miterLimit = sh->miterLimit;
        s.style.stroke.dash.assign(sh->strokeDashArray, sh->strokeDashArray + sh->strokeDashCount);
        s.style.stroke.dashOffset = sh->strokeDashOffset;
        d.shapes.push_back(std::move(s));
    }
    nsvgDelete(img);
    if (d.width <= 0 || d.height <= 0) {
        float x1 = 0, y1 = 0;
        for (const Shape& s : d.shapes) {
            const Rectangle b = s.path.bounds();
            x1 = std::max(x1, b.x + b.width);
            y1 = std::max(y1, b.y + b.height);
        }
        d.width = x1;
        d.height = y1;
    }
    return d;
}

Drawing Drawing::load(const std::string& file) {
    const std::string path = media::resolve(file);
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("vg::Drawing: cannot read " + file);
    std::stringstream ss;
    ss << in.rdbuf();
    return parse(ss.str());
}

const Shape* Drawing::find(const std::string& id) const {
    for (const Shape& s : shapes)
        if (s.id == id) return &s;
    return nullptr;
}

Ctx fit(const Ctx& c, const Drawing& d, Rectangle box) {
    const float k = (d.width > 0 && d.height > 0) ? std::min(box.width / d.width, box.height / d.height) : 1.f;
    const Vector2 o{ box.x + (box.width - d.width * k) / 2, box.y + (box.height - d.height * k) / 2 };
    Ctx r = c;
    r.o = mapP(c, o);
    r.s = c.s * k;
    return r;
}

void draw(const Ctx& c, const Drawing& d, Rectangle box) {
    const Ctx f = fit(c, d, box);
    for (const Shape& s : d.shapes) draw(f, s.path, s.style);
}

void write(const Ctx& c, const Drawing& d, Rectangle box, float u, float overlap) {
    const Ctx f = fit(c, d, box);
    const size_t n = d.shapes.size();
    if (!n) return;
    const float lag = 1 - std::clamp(overlap, 0.f, 1.f), dur = 1 / (1 + (n - 1) * lag);
    for (size_t i = 0; i < n; i++) write(f, d.shapes[i].path, d.shapes[i].style, (u - i * lag * dur) / dur);
}

} } // rmtn::vg
