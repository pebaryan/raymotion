// space.cpp -- space:: views, the depth-sorted draw list, axes, guides, projections,
// meshes, topology surfaces and 4D polytopes (see rmtn/space.hpp).
#include "rmtn/space.hpp"
#include <algorithm>
#include <cmath>

namespace rmtn {
namespace space {
namespace {

constexpr float kDeg = 0.017453292519943295f;
constexpr float kPi = 3.14159265358979f;

Vector3 add(Vector3 a, Vector3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vector3 sub(Vector3 a, Vector3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vector3 mul(Vector3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
float dot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vector3 cross(Vector3 a, Vector3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
float length(Vector3 a) { return std::sqrt(dot(a, a)); }
Vector3 unit(Vector3 a) { const float l = length(a); return l > 1e-9f ? mul(a, 1 / l) : a; }
float clamp01(float v) { return std::clamp(v, 0.f, 1.f); }
float mixF(float a, float b, float u) { return a + (b - a) * u; }

} // namespace

Vector3 lerp(Vector3 a, Vector3 b, float u) { return add(a, mul(sub(b, a), u)); }

// ------------------------------------------------------------------ View

View View::flat(Vector2 center, float scale) {
    View v;
    v.center = center;
    v.scale = scale;
    return v;
}

View View::tilted(Vector2 center, float scale, float yaw, float pitch, float perspective) {
    View v;
    v.center = center;
    v.scale = scale;
    v.yaw = yaw;
    v.pitch = pitch;
    v.perspective = perspective;
    return v;
}

View View::mix(const View& a, const View& b, float u) {
    View v;
    v.center = { mixF(a.center.x, b.center.x, u), mixF(a.center.y, b.center.y, u) };
    v.scale = mixF(a.scale, b.scale, u);
    v.yaw = mixF(a.yaw, b.yaw, u);
    v.pitch = mixF(a.pitch, b.pitch, u);
    v.perspective = mixF(a.perspective, b.perspective, u);
    v.distance = mixF(a.distance, b.distance, u);
    v.target = lerp(a.target, b.target, u);
    return v;
}

Vector3 View::toView(Vector3 p) const {
    p = sub(p, target);
    const float cy = std::cos(yaw * kDeg), sy = std::sin(yaw * kDeg);
    const Vector3 q{ p.x * cy - p.y * sy, p.x * sy + p.y * cy, p.z };
    const float cp = std::cos(pitch * kDeg), sp = std::sin(pitch * kDeg);
    return { q.x, q.y * cp - q.z * sp, q.y * sp + q.z * cp };
}

float View::scaleAt(Vector3 p) const {
    if (perspective <= 0) return scale;
    const float z = toView(p).z, d = std::max(distance, 0.1f);
    const float f = d / std::max(d - z, d * 0.05f);
    return scale * (1 + perspective * (f - 1));
}

Vector2 View::project(Vector3 p) const {
    const Vector3 v = toView(p);
    const float k = scaleAt(p);
    return { center.x + v.x * k, center.y - v.y * k };
}

float View::depth(Vector3 p) const { return toView(p).z; }

// ------------------------------------------------------------------ Mesh

Mesh Mesh::grid(int rows, int cols, const std::function<Vector3(float, float)>& f, bool wrapCols, bool wrapRows) {
    Mesh m;
    m.rows = std::max(rows, 2);
    m.cols = std::max(cols, 2);
    m.wrapCols = wrapCols;
    m.wrapRows = wrapRows;
    m.pts.reserve((size_t)m.rows * m.cols);
    for (int r = 0; r < m.rows; r++)
        for (int c = 0; c < m.cols; c++) {
            const float u = wrapCols ? (float)c / m.cols : (float)c / (m.cols - 1);
            const float v = wrapRows ? (float)r / m.rows : (float)r / (m.rows - 1);
            m.pts.push_back(f(u, v));
        }
    return m;
}

Mesh Mesh::graph(const std::function<double(double, double)>& f, plot::Range x, plot::Range y, int rows, int cols) {
    return grid(rows, cols, [&](float u, float v) {
        const double px = x.min + (x.max - x.min) * u, py = y.max - (y.max - y.min) * v;
        double z = f(px, py);
        if (!std::isfinite(z)) z = 0;
        return Vector3{ (float)px, (float)py, (float)z };
    });
}

Mesh Mesh::mapped(const std::function<Vector3(Vector3)>& f) const {
    Mesh m = *this;
    for (Vector3& p : m.pts) p = f(p);
    return m;
}

Mesh Mesh::mix(const Mesh& a, const Mesh& b, float u) {
    Mesh m = b;
    for (size_t i = 0; i < m.pts.size() && i < a.pts.size(); i++) m.pts[i] = lerp(a.pts[i], b.pts[i], u);
    return m;
}

// ------------------------------------------------------------------ Scene

Scene::Scene(const View& v) : view_(v) {}

Color Scene::shadeOf(Color c, Vector3 a, Vector3 b, Vector3 cc, bool* back) const {
    const Vector3 n = unit(cross(sub(view_.toView(b), view_.toView(a)), sub(view_.toView(cc), view_.toView(a))));
    if (back) *back = n.z < 0;
    const Vector3 light = unit({ -0.45f, 0.55f, 0.7f });
    const float lit = 0.42f + 0.58f * std::fabs(dot(n, light));
    return { (unsigned char)(c.r * lit), (unsigned char)(c.g * lit), (unsigned char)(c.b * lit), c.a };
}

void Scene::line(Vector3 a, Vector3 b, const vg::Stroke& s) { curve({ a, b }, s, false); }

void Scene::curve(const std::vector<Vector3>& pts, const vg::Stroke& s, bool closed) {
    if (pts.size() < 2 || s.width <= 0 || s.color.a == 0) return;
    std::vector<Vector3> p = pts;
    if (closed) p.push_back(pts.front());
    // pieces of a few segments, each sorted on its own; long segments are split so a line
    // through a surface is hidden only where it is behind
    std::vector<Vector3> fine{ p[0] };
    for (size_t i = 1; i < p.size(); i++) {
        const float px = std::hypot(view_.project(p[i]).x - view_.project(p[i - 1]).x, view_.project(p[i]).y - view_.project(p[i - 1]).y);
        const int n = std::clamp((int)(px / 40), 1, 64);
        for (int k = 1; k <= n; k++) fine.push_back(lerp(p[i - 1], p[i], (float)k / n));
    }
    float run = 0;
    const size_t per = 3;
    for (size_t i = 0; i + 1 < fine.size(); i += per) {
        Piece pc;
        pc.stroke = s;
        float depth = 0;
        const size_t end = std::min(i + per, fine.size() - 1);
        for (size_t k = i; k <= end; k++) {
            pc.pts.push_back(view_.project(fine[k]));
            depth += view_.depth(fine[k]);
        }
        depth /= (float)(end - i + 1);
        if (!s.dash.empty()) pc.stroke.dashOffset = s.dashOffset + run;
        for (size_t k = 1; k < pc.pts.size(); k++) run += std::hypot(pc.pts[k].x - pc.pts[k - 1].x, pc.pts[k].y - pc.pts[k - 1].y);
        items_.push_back({ depth, 0, pieces_.size() });
        pieces_.push_back(std::move(pc));
    }
}

void Scene::arrow(Vector3 from, Vector3 to, Color c, float width, float head) {
    const Vector2 a = view_.project(from), b = view_.project(to);
    const float len = std::hypot(b.x - a.x, b.y - a.y);
    if (len < 0.5f) return;
    const float h = std::min(head, len * 0.6f), f = 1 - h / len;
    vg::Stroke st(c, width);
    st.cap = vg::Cap::Butt;
    curve({ from, lerp(from, to, f) }, st);
    const Vector2 d{ (b.x - a.x) / len, (b.y - a.y) / len }, n{ -d.y, d.x };
    const Vector2 base{ b.x - d.x * h, b.y - d.y * h };
    Face fc;
    fc.pts = { b, { base.x + n.x * h * 0.5f, base.y + n.y * h * 0.5f }, { base.x - n.x * h * 0.5f, base.y - n.y * h * 0.5f } };
    fc.fill = c;
    items_.push_back({ view_.depth(to) + 1e-3f, 1, faces_.size() });
    faces_.push_back(std::move(fc));
}

void Scene::point(Vector3 p, Color c, float radius) {
    if (c.a == 0 || radius <= 0) return;
    items_.push_back({ view_.depth(p) + 1e-3f, 2, dots_.size() });
    dots_.push_back({ view_.project(p), radius * view_.scaleAt(p) / std::max(view_.scale, 1e-3f), c });
}

void Scene::face(const std::vector<Vector3>& pts, Color c, bool shade) {
    if (pts.size() < 3 || c.a == 0) return;
    Face fc;
    float depth = 0;
    for (Vector3 p : pts) { fc.pts.push_back(view_.project(p)); depth += view_.depth(p); }
    fc.fill = shade ? shadeOf(c, pts[0], pts[1], pts[2], nullptr) : c;
    items_.push_back({ depth / pts.size(), 1, faces_.size() });
    faces_.push_back(std::move(fc));
}

void Scene::mesh(const Mesh& m, const MeshStyle& s) {
    if (m.rows < 2 || m.cols < 2 || s.opacity <= 0) return;
    const int rmax = m.wrapRows ? m.rows : m.rows - 1, cmax = m.wrapCols ? m.cols : m.cols - 1;
    const int every = std::max(1, s.wireEvery);
    std::vector<Vector2> scr(m.pts.size());
    std::vector<float> dep(m.pts.size());
    for (size_t i = 0; i < m.pts.size(); i++) { scr[i] = view_.project(m.pts[i]); dep[i] = view_.depth(m.pts[i]); }
    for (int r = 0; r < rmax; r++)
        for (int c = 0; c < cmax; c++) {
            const int r1 = (r + 1) % m.rows, c1 = (c + 1) % m.cols;
            const size_t i00 = (size_t)r * m.cols + c, i01 = (size_t)r * m.cols + c1, i11 = (size_t)r1 * m.cols + c1, i10 = (size_t)r1 * m.cols + c;
            Face fc;
            fc.pts = { scr[i00], scr[i01], scr[i11], scr[i10] };
            const Vector3 mid = mul(add(add(m.pts[i00], m.pts[i01]), add(m.pts[i11], m.pts[i10])), 0.25f);
            Color base = s.cellColor ? s.cellColor(r, c) : s.colorAt ? s.colorAt(mid) : s.color;
            bool back = false;
            // the diagonal normal is steadier than one corner's on bent quads
            const Vector3 n = unit(cross(sub(view_.toView(m.pts[i11]), view_.toView(m.pts[i00])), sub(view_.toView(m.pts[i10]), view_.toView(m.pts[i01]))));
            back = n.z < 0;
            if (back && s.back.a) base = s.back;
            if (s.shade) {
                const float lit = 0.42f + 0.58f * std::fabs(dot(n, unit({ -0.45f, 0.55f, 0.7f })));
                base = { (unsigned char)(base.r * lit), (unsigned char)(base.g * lit), (unsigned char)(base.b * lit), base.a };
            }
            base.a = (unsigned char)(base.a * clamp01(s.opacity));
            fc.fill = base;
            if (s.wire > 0 && s.wireColor.a) {
                fc.edge = vg::Stroke(s.wireColor, s.wire);
                // every face draws its own edges, so a nearer face never thins a line it shares
                if (r % every == 0) fc.edges.moveTo(scr[i00]).lineTo(scr[i01]);
                if (c % every == 0) fc.edges.moveTo(scr[i00]).lineTo(scr[i10]);
                if (r1 % every == 0 || (!m.wrapRows && r + 1 == rmax)) fc.edges.moveTo(scr[i10]).lineTo(scr[i11]);
                if (c1 % every == 0 || (!m.wrapCols && c + 1 == cmax)) fc.edges.moveTo(scr[i01]).lineTo(scr[i11]);
            }
            items_.push_back({ (dep[i00] + dep[i01] + dep[i11] + dep[i10]) / 4, 1, faces_.size() });
            faces_.push_back(std::move(fc));
        }
}

void Scene::plane(Vector3 at, Vector3 normal, float size, Color c, int gridLines, Color gridColor) {
    const Vector3 n = unit(normal);
    const Vector3 ref = std::fabs(n.z) < 0.9f ? Vector3{ 0, 0, 1 } : Vector3{ 1, 0, 0 };
    const Vector3 e1 = unit(cross(ref, n)), e2 = cross(n, e1);
    const float h = size / 2;
    auto P = [&](float a, float b) { return add(at, add(mul(e1, a), mul(e2, b))); };
    if (gridLines <= 0) { face({ P(-h, -h), P(h, -h), P(h, h), P(-h, h) }, c, false); return; }
    // as a mesh, so lines on it sort with it
    Mesh m = Mesh::grid(gridLines + 1, gridLines + 1, [&](float u, float v) { return P(-h + size * u, -h + size * v); });
    MeshStyle st;
    st.color = c;
    st.shade = false;
    st.wireColor = gridColor;
    st.wire = 1.2f;
    mesh(m, st);
}

void Scene::label(Vector3 at, const std::string& tex, float size, Color c, Vector2 anchor, Vector2 offset) {
    if (c.a == 0 || tex.empty()) return;
    const Vector2 p = view_.project(at);
    items_.push_back({ view_.depth(at) + 2e-3f, 3, texts_.size() });   // a hair in front of what it labels
    texts_.push_back({ { p.x + offset.x, p.y + offset.y }, tex, size, c, anchor });
}

void Scene::draw(const Ctx& c) const {
    std::vector<Item> order = items_;
    std::stable_sort(order.begin(), order.end(), [](const Item& a, const Item& b) { return a.depth < b.depth; });
    for (const Item& it : order) {
        if (it.kind == 0) {
            const Piece& p = pieces_[it.index];
            vg::stroke(c, vg::Path::polyline(p.pts), p.stroke);
        } else if (it.kind == 1) {
            const Face& f = faces_[it.index];
            if (f.fill.a) vg::fill(c, vg::Path::polyline(f.pts, true), f.fill);
            if (!f.edges.empty()) vg::stroke(c, f.edges, f.edge);
        } else if (it.kind == 2) {
            const Dot& d = dots_[it.index];
            vg::fill(c, vg::Path::circle(d.at, d.r), d.color);
        } else {
            const Text& t = texts_[it.index];
            plot::label(c, t.tex, t.at, t.anchor, t.size, t.color);
        }
    }
}

// ------------------------------------------------------------------ axes and guides

void axes(Scene& s, const Axes3& a, const Axes3Style& st, Vector3 grow) {
    const View& v = s.view();
    const float g[3] = { clamp01(grow.x), clamp01(grow.y), clamp01(grow.z) };
    const plot::Range* R[3] = { &a.x, &a.y, &a.z };
    auto axisPoint = [](int k, double t) {
        Vector3 p{ 0, 0, 0 };
        (k == 0 ? p.x : k == 1 ? p.y : p.z) = (float)t;
        return p;
    };
    // grids in the coordinate planes: lines of axis i spaced along axis j, shown as both have grown
    const bool grids[3] = { st.gridYZ, st.gridXZ, st.gridXY };   // indexed by the missing axis
    for (int miss = 0; miss < 3; miss++) {
        if (!grids[miss]) continue;
        const int i = (miss + 1) % 3, j = (miss + 2) % 3;
        const float k = std::min(g[i], g[j]);
        if (k <= 0) continue;
        vg::Stroke gs(withAlpha(st.gridColor, k), 1.5f);
        for (int pass = 0; pass < 2; pass++) {
            const int along = pass ? i : j, across = pass ? j : i;
            for (double t : R[across]->ticks()) {
                if (t == 0) continue;
                const Vector3 o = axisPoint(across, t);
                s.line(add(o, axisPoint(along, R[along]->min * g[along])), add(o, axisPoint(along, R[along]->max * g[along])), gs);
            }
        }
    }
    const std::string names[3] = { st.xLabel, st.yLabel, st.zLabel };
    for (int k = 0; k < 3; k++) {
        if (g[k] <= 0) continue;
        const Vector3 lo = axisPoint(k, R[k]->min * g[k]), hi = axisPoint(k, R[k]->max * g[k]);
        if (st.arrows) {
            s.line(lo, axisPoint(k, 0), vg::Stroke(st.color, st.width));
            s.arrow(axisPoint(k, 0), hi, st.color, st.width, 16);
        } else s.line(lo, hi, vg::Stroke(st.color, st.width));
        // ticks across the axis (x and z ticks along y, y ticks along x), numbers beside them
        const int across = k == 1 ? 0 : 1;
        const plot::Range r = *R[k];
        const int dec = [&] {
            const double step = r.tick();
            for (int d = 0; d < 6; d++) if (std::fabs(step * std::pow(10, d) - std::round(step * std::pow(10, d))) < 1e-6) return d;
            return 6;
        }();
        for (double t : r.ticks()) {
            if (t == 0 || std::fabs(t) > std::fabs((t < 0 ? r.min : r.max) * g[k]) + 1e-9 || t >= r.max) continue;
            const float tl = 7 / std::max(v.scaleAt(axisPoint(k, t)), 1.f);
            const Vector3 p = axisPoint(k, t);
            if (st.ticks) s.line(sub(p, axisPoint(across, tl)), add(p, axisPoint(across, tl)), vg::Stroke(st.color, 2));
            if (st.numbers) {
                char buf[32];
                std::snprintf(buf, sizeof buf, "%.*f", dec, t);
                const Vector2 anchor = k == 0 ? Vector2{ 0.5f, 0 } : Vector2{ 1, 0.5f };
                const Vector2 off = k == 0 ? Vector2{ 0, 12 } : Vector2{ -14, 0 };
                s.label(p, buf, st.numberSize, withAlpha(st.numberColor, g[k]), anchor, off);
            }
        }
        if (!names[k].empty())
            s.label(hi, names[k], st.labelSize, withAlpha(st.color, g[k]), k == 1 ? Vector2{ 0, 1 } : Vector2{ 0, 0.5f },
                    k == 1 ? Vector2{ 10, -6 } : Vector2{ 16, 0 });
    }
}

void guides(Scene& s, Vector3 p, Color c, int dims, float width) {
    vg::Stroke st(withAlpha(c, 0.8f), width);
    st.dash = { 9, 8 };
    st.cap = vg::Cap::Butt;
    if (dims <= 1) { s.line(p, { p.x, 0, 0 }, st); return; }
    if (dims == 2) {
        s.line(p, { p.x, 0, p.z }, st);
        s.line(p, { 0, p.y, p.z }, st);
        return;
    }
    const Vector3 foot{ p.x, p.y, 0 };
    s.line(p, foot, st);
    s.line(foot, { p.x, 0, 0 }, st);
    s.line(foot, { 0, p.y, 0 }, st);
}

// ------------------------------------------------------------------ projections

Vector3 projectOnPlane(Vector3 p, Vector3 planePoint, Vector3 normal) {
    const Vector3 n = unit(normal);
    return sub(p, mul(n, dot(sub(p, planePoint), n)));
}

Vector3 projectOnLine(Vector3 p, Vector3 linePoint, Vector3 direction) {
    const Vector3 d = unit(direction);
    return add(linePoint, mul(d, dot(sub(p, linePoint), d)));
}

// ------------------------------------------------------------------ surfaces

namespace surfaces {

Mesh sphere(float radius, int rows, int cols) {
    return Mesh::grid(rows, cols, [radius](float u, float v) {
        const float th = u * 2 * kPi, ph = v * kPi;
        return Vector3{ radius * std::sin(ph) * std::cos(th), radius * std::sin(ph) * std::sin(th), radius * std::cos(ph) };
    }, true, false);
}

Mesh torus(float ring, float tube, int rows, int cols) {
    return Mesh::grid(rows, cols, [ring, tube](float u, float v) {
        const float th = u * 2 * kPi, ph = v * 2 * kPi;
        const float r = ring + tube * std::cos(ph);
        return Vector3{ r * std::cos(th), r * std::sin(th), tube * std::sin(ph) };
    }, true, true);
}

Mesh glueTorus(float roll, float bend, float w, float h, int rows, int cols) {
    roll = clamp01(roll);
    bend = clamp01(bend);
    return Mesh::grid(rows, cols, [=](float u, float v) {
        // roll: the sheet's x wraps round a tube about an axis parallel to y, below the sheet
        float x = w * (u - 0.5f), z = 0, axisZ = 0;
        if (roll > 1e-4f) {
            // the axis sits R1 below the sheet's middle; shifted up by half the curl's depth so
            // the shape stays centred as it closes
            const float R1 = w / (2 * kPi * roll), th = 2 * kPi * roll * (u - 0.5f);
            axisZ = -R1 * (1 + std::cos(kPi * roll)) / 2;
            x = R1 * std::sin(th);
            z = axisZ + R1 * std::cos(th);
        }
        float y = h * (v - 0.5f);
        if (bend > 1e-4f) {
            // bend: the tube's axis curves round a circle in the xy plane (centre towards -x),
            // so the torus ends up lying flat
            const float R2 = h / (2 * kPi * bend), ps = 2 * kPi * bend * (v - 0.5f);
            const float r = R2 + x;           // distance from the centre of curvature
            x = -R2 + r * std::cos(ps) + R2 * (1 - std::cos(kPi * bend)) / 2;   // recentred
            y = r * std::sin(ps);
        }
        return Vector3{ x, y, z };
    }, false, false);
}

Mesh moebius(float join, float length, float width, int twists, int rows, int cols) {
    join = clamp01(join);
    return Mesh::grid(rows, cols, [=](float u, float v) {
        const float s = u - 0.5f, across = width * (v - 0.5f);
        const float phi = join * twists * kPi * s;   // the twist so far along the strip
        if (join < 1e-4f) return Vector3{ length * s, across, 0 };
        const float R = length / (2 * kPi * join), th = 2 * kPi * join * s;
        // centre line: an arc in the xy plane through the origin, curving towards +y
        const Vector3 c{ R * std::sin(th), R * (1 - std::cos(th)), 0 };
        const Vector3 radial{ -std::sin(th), std::cos(th), 0 };   // towards the centre of curvature
        const Vector3 up{ 0, 0, 1 };
        // the width turns from the radial direction towards z
        const Vector3 dir = add(mul(radial, std::cos(phi)), mul(up, std::sin(phi)));
        const float centreY = R * (1 - std::cos(kPi * join)) / 2;
        return Vector3{ c.x + dir.x * across, c.y + dir.y * across - centreY, c.z + dir.z * across };
    }, false, false);
}

Mesh klein(int rows, int cols) {
    // u runs 0..pi (its ends meet as the same circle, reflected: the Klein gluing), v round
    Mesh m = Mesh::grid(rows, cols, [](float a, float b) {
        const double u = a * kPi, v = b * 2 * kPi;
        const double cu = std::cos(u), su = std::sin(u), cv = std::cos(v), sv = std::sin(v);
        const double c2 = cu * cu, c3 = c2 * cu, c4 = c2 * c2, c5 = c4 * cu, c6 = c4 * c2, c7 = c6 * cu;
        const double x = -2.0 / 15 * cu * (3 * cv - 30 * su + 90 * c4 * su - 60 * c6 * su + 5 * cu * cv * su);
        const double y = -1.0 / 15 * su * (3 * cv - 3 * c2 * cv - 48 * c4 * cv + 48 * c6 * cv - 60 * su + 5 * cu * cv * su
                                         - 5 * c3 * cv * su - 80 * c5 * cv * su + 80 * c7 * cv * su);
        const double z = 2.0 / 15 * (3 + 5 * cu * su) * sv;
        return Vector3{ (float)x, (float)z, (float)-y };   // stood on end
    }, false, true);
    Vector3 lo{ 1e9f, 1e9f, 1e9f }, hi{ -1e9f, -1e9f, -1e9f };
    for (Vector3 p : m.pts) {
        lo = { std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z) };
        hi = { std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z) };
    }
    const Vector3 mid = mul(add(lo, hi), 0.5f);
    return m.mapped([mid](Vector3 p) { return sub(p, mid); });
}

} // namespace surfaces

// ------------------------------------------------------------------ four dimensions

Vec4 rotate4(Vec4 p, int a, int b, double degrees) {
    double c[4] = { p.x, p.y, p.z, p.w };
    if (a < 0 || a > 3 || b < 0 || b > 3 || a == b) return p;
    const double r = degrees * 0.017453292519943295, ca = std::cos(r), sa = std::sin(r);
    const double pa = c[a], pb = c[b];
    c[a] = pa * ca - pb * sa;
    c[b] = pa * sa + pb * ca;
    return { c[0], c[1], c[2], c[3] };
}

Vector3 project4(Vec4 p, double distance) {
    if (distance <= 0) return { (float)p.x, (float)p.y, (float)p.z };
    const double k = distance / std::max(distance - p.w, distance * 0.05);
    return { (float)(p.x * k), (float)(p.y * k), (float)(p.z * k) };
}

Polytope4 tesseract(double half) {
    Polytope4 t;
    for (int i = 0; i < 16; i++)
        t.vertices.push_back({ i & 1 ? half : -half, i & 2 ? half : -half, i & 4 ? half : -half, i & 8 ? half : -half });
    for (int i = 0; i < 16; i++)
        for (int bit = 0; bit < 4; bit++) {
            const int j = i ^ (1 << bit);
            if (j > i) t.edges.push_back({ i, j });
        }
    return t;
}

} } // rmtn::space
