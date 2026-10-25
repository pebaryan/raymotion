// diagram_layout.cpp -- the graph model, node outlines, label sizing and the built-in
// layouts: layered (Sugiyama), stress (force), tidy and radial trees, circle.
#include "diagram_internal.hpp"
#include "rmtn/fonts.hpp"
#include "rmtn/layout.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <unordered_map>

namespace rmtn {
namespace dg {

// ------------------------------------------------------------------ graph model
Node* Graph::node(std::string_view id) {
    for (Node& n : nodes) if (n.id == id) return &n;
    return nullptr;
}
const Node* Graph::node(std::string_view id) const { return const_cast<Graph*>(this)->node(id); }
Edge* Graph::edge(std::string_view id) {
    for (Edge& e : edges) if (e.id == id) return &e;
    return nullptr;
}
const Edge* Graph::edge(std::string_view id) const { return const_cast<Graph*>(this)->edge(id); }
Group* Graph::group(std::string_view id) {
    for (Group& gr : groups) if (gr.id == id) return &gr;
    return nullptr;
}
const Group* Graph::group(std::string_view id) const { return const_cast<Graph*>(this)->group(id); }

Node& Graph::addNode(const std::string& id, const std::string& label, Shape shape) {
    if (Node* n = node(id)) return *n;
    Node n;
    n.id = id;
    n.label = label.empty() ? id : label;
    n.shape = shape;
    nodes.push_back(std::move(n));
    return nodes.back();
}
Edge& Graph::addEdge(const std::string& from, const std::string& to, const std::string& label) {
    if (!node(from)) addNode(from);
    if (!node(to)) addNode(to);
    Edge e;
    e.from = from; e.to = to; e.label = label;
    e.id = from + "->" + to;
    for (int k = 2; edge(e.id); k++) e.id = from + "->" + to + "#" + std::to_string(k);
    edges.push_back(std::move(e));
    return edges.back();
}
Group& Graph::addGroup(const std::string& id, const std::string& label, const std::string& parent) {
    if (Group* gr = group(id)) return *gr;
    Group gr;
    gr.id = id; gr.label = label; gr.parent = parent;
    groups.push_back(std::move(gr));
    return groups.back();
}
bool Graph::inGroup(std::string_view nodeId, std::string_view gid) const {
    const Node* n = node(nodeId);
    if (!n) return false;
    std::string cur = n->group;
    for (int guard = 0; !cur.empty() && guard < 64; guard++) {
        if (cur == gid) return true;
        const Group* gr = group(cur);
        cur = gr ? gr->parent : "";
    }
    return false;
}

namespace {
void grow(Rectangle& r, bool& any, Rectangle b) {
    if (!any) { r = b; any = true; return; }
    const float x0 = std::min(r.x, b.x), y0 = std::min(r.y, b.y);
    const float x1 = std::max(r.x + r.width, b.x + b.width), y1 = std::max(r.y + r.height, b.y + b.height);
    r = { x0, y0, x1 - x0, y1 - y0 };
}
}

Rectangle Graph::bounds() const {
    Rectangle r{};
    bool any = false;
    for (const Node& n : nodes) {
        grow(r, any, { n.pos.x - n.size.x / 2, n.pos.y - n.size.y / 2, n.size.x, n.size.y });
        if (!n.label.empty() && n.labelPlace != LabelPlace::Inside) grow(r, any, detail::labelRect(n));
    }
    for (const Edge& e : edges) {
        for (Vector2 p : e.points) grow(r, any, { p.x, p.y, 0, 0 });
        if (!e.label.empty() && e.labelPos)
            grow(r, any, { e.labelPos->x - e.labelSize.x / 2, e.labelPos->y - e.labelSize.y / 2, e.labelSize.x, e.labelSize.y });
    }
    for (const Group& gr : groups) if (gr.rect.width > 0) grow(r, any, gr.rect);
    return r;
}

namespace detail {

// ------------------------------------------------------------------ outlines
namespace {
void ellipse(std::vector<Vector2>& out, float cx, float cy, float rx, float ry, float a0, float a1, int segs) {
    for (int i = 0; i <= segs; i++) {
        const float a = a0 + (a1 - a0) * i / segs;
        out.push_back({ cx + rx * std::cos(a), cy + ry * std::sin(a) });
    }
}
void roundedRect(std::vector<Vector2>& out, float w, float h, float r) {
    r = std::max(0.f, std::min({ r, w / 2, h / 2 }));
    if (r <= 0.01f) { out = { { -w / 2, -h / 2 }, { w / 2, -h / 2 }, { w / 2, h / 2 }, { -w / 2, h / 2 } }; return; }
    const float P = (float)PI;
    ellipse(out, -w / 2 + r, -h / 2 + r, r, r, P, 1.5f * P, 8);
    ellipse(out, w / 2 - r, -h / 2 + r, r, r, 1.5f * P, 2 * P, 8);
    ellipse(out, w / 2 - r, h / 2 - r, r, r, 0, 0.5f * P, 8);
    ellipse(out, -w / 2 + r, h / 2 - r, r, r, 0.5f * P, P, 8);
}
}

std::vector<Vector2> outline(const Node& n, float corner) {
    const float w = std::max(1.f, n.size.x), h = std::max(1.f, n.size.y), P = (float)PI;
    std::vector<Vector2> o;
    switch (n.shape) {
    case Shape::Rect: case Shape::Subroutine: case Shape::Text: case Shape::Annotation:
        o = { { -w / 2, -h / 2 }, { w / 2, -h / 2 }, { w / 2, h / 2 }, { -w / 2, h / 2 } }; break;
    case Shape::Rounded: case Shape::Task: roundedRect(o, w, h, corner); break;
    case Shape::Stadium: roundedRect(o, w, h, h / 2); break;
    case Shape::Circle: case Shape::DoubleCircle: case Shape::Ellipse: case Shape::Point:
    case Shape::StartState: case Shape::EndState: case Shape::Event:
        ellipse(o, 0, 0, w / 2, h / 2, 0, 2 * P, 64); o.pop_back(); break;
    case Shape::Diamond: case Shape::Gateway:
        o = { { 0, -h / 2 }, { w / 2, 0 }, { 0, h / 2 }, { -w / 2, 0 } }; break;
    case Shape::Hexagon: {
        const float k = std::min(h * 0.28f, w / 4);
        o = { { -w / 2, 0 }, { -w / 2 + k, -h / 2 }, { w / 2 - k, -h / 2 }, { w / 2, 0 }, { w / 2 - k, h / 2 }, { -w / 2 + k, h / 2 } };
        break;
    }
    case Shape::Parallelogram: case Shape::ParallelogramAlt: {
        const float k = std::min(h * 0.3f, w / 4);
        if (n.shape == Shape::Parallelogram) o = { { -w / 2 + k, -h / 2 }, { w / 2, -h / 2 }, { w / 2 - k, h / 2 }, { -w / 2, h / 2 } };
        else o = { { -w / 2, -h / 2 }, { w / 2 - k, -h / 2 }, { w / 2, h / 2 }, { -w / 2 + k, h / 2 } };
        break;
    }
    case Shape::Trapezoid: case Shape::TrapezoidAlt: {
        const float k = std::min(h * 0.3f, w / 4);
        if (n.shape == Shape::Trapezoid) o = { { -w / 2 + k, -h / 2 }, { w / 2 - k, -h / 2 }, { w / 2, h / 2 }, { -w / 2, h / 2 } };
        else o = { { -w / 2, -h / 2 }, { w / 2, -h / 2 }, { w / 2 - k, h / 2 }, { -w / 2 + k, h / 2 } };
        break;
    }
    case Shape::Cylinder: case Shape::DataStore: {
        const float ry = std::min(h * 0.12f, w * 0.18f);
        ellipse(o, 0, -h / 2 + ry, w / 2, ry, P, 2 * P, 24);
        ellipse(o, 0, h / 2 - ry, w / 2, ry, 0, P, 24);
        break;
    }
    case Shape::Flag: {
        const float k = std::min(h * 0.3f, w / 4);
        o = { { -w / 2, -h / 2 }, { w / 2, -h / 2 }, { w / 2, h / 2 }, { -w / 2, h / 2 }, { -w / 2 + k, 0 } };
        break;
    }
    case Shape::Note: case Shape::DataObject: {
        const float f = n.shape == Shape::DataObject ? w * 0.32f : std::min(w, h) * 0.22f;
        o = { { -w / 2, -h / 2 }, { w / 2 - f, -h / 2 }, { w / 2, -h / 2 + f }, { w / 2, h / 2 }, { -w / 2, h / 2 } };
        break;
    }
    case Shape::Document: {
        const float a = h * 0.08f;
        o = { { -w / 2, -h / 2 }, { w / 2, -h / 2 } };
        for (int i = 0; i <= 24; i++) {
            const float u = (float)i / 24, x = w / 2 - u * w;
            o.push_back({ x, h / 2 - a + a * std::sin(u * 2 * P) });
        }
        break;
    }
    }
    for (Vector2& p : o) { p.x += n.pos.x; p.y += n.pos.y; }
    return o;
}

Vector2 boundaryPoint(const Node& n, Vector2 toward, float corner) {
    const Vector2 c = n.pos;
    const float dx = toward.x - c.x, dy = toward.y - c.y;
    if (std::fabs(dx) < 1e-4f && std::fabs(dy) < 1e-4f) return c;
    if (n.shape == Shape::Point && n.size.x < 1) return c;
    const std::vector<Vector2> o = outline(n, corner);
    float best = 2;
    for (size_t i = 0; i < o.size(); i++) {
        const Vector2 a = o[i], b = o[(i + 1) % o.size()];
        const float ex = b.x - a.x, ey = b.y - a.y;
        const float den = dx * ey - dy * ex;
        if (std::fabs(den) < 1e-9f) continue;
        const float u = ((a.x - c.x) * ey - (a.y - c.y) * ex) / den;   // along the ray
        const float v = ((a.x - c.x) * dy - (a.y - c.y) * dx) / den;   // along the edge
        if (u >= 0 && v >= -1e-4f && v <= 1 + 1e-4f && u < best) best = u;
    }
    if (best > 1) return toward;                       // `toward` lies inside the node
    return { c.x + dx * best, c.y + dy * best };
}

std::vector<Vector2> straightRoute(const Node& a, const Node& b, float bend, float corner) {
    const Vector2 ca = a.pos, cb = b.pos;
    if (std::fabs(bend) < 0.01f)
        return { boundaryPoint(a, cb, corner), boundaryPoint(b, ca, corner) };
    float dx = cb.x - ca.x, dy = cb.y - ca.y;
    const float len = std::max(1e-3f, std::sqrt(dx * dx + dy * dy));
    dx /= len; dy /= len;
    const Vector2 mid{ (ca.x + cb.x) / 2 - dy * bend, (ca.y + cb.y) / 2 + dx * bend };
    return { boundaryPoint(a, mid, corner), mid, boundaryPoint(b, mid, corner) };
}

std::vector<Vector2> selfLoop(const Node& n, int index) {
    const float w = n.size.x, h = n.size.y, k = 1 + 0.6f * index;
    const float x = n.pos.x + w / 2, y = n.pos.y - h / 2;
    const float r = std::max(18.f, std::min(w, h) * 0.45f) * k;
    return { { x - std::min(w * 0.2f, r * 0.6f), y }, { x - r * 0.1f, y - r * 0.9f }, { x + r * 0.9f, y - r * 0.6f },
             { x + r * 0.9f, y + r * 0.35f }, { x, y + std::min(h * 0.3f, r * 0.7f) } };
}

Rectangle labelRect(const Node& n) {
    const Vector2 s = n.labelSize;
    const float gap = 8;
    switch (n.labelPlace) {
    case LabelPlace::Inside: return { n.pos.x - s.x / 2, n.pos.y - s.y / 2, s.x, s.y };
    case LabelPlace::Below: return { n.pos.x - s.x / 2, n.pos.y + n.size.y / 2 + gap * 0.6f, s.x, s.y };
    case LabelPlace::Above: return { n.pos.x - s.x / 2, n.pos.y - n.size.y / 2 - gap * 0.6f - s.y, s.x, s.y };
    case LabelPlace::Right: {
        if (n.labelAngle != 0) {                       // rotated about the node: a box around the reach
            const float a = n.labelAngle * DEG2RAD, reach = n.size.x / 2 + gap + s.x;
            const Vector2 tip{ n.pos.x + std::cos(a) * reach, n.pos.y + std::sin(a) * reach };
            return { std::min(n.pos.x, tip.x) - s.y / 2, std::min(n.pos.y, tip.y) - s.y / 2,
                     std::fabs(tip.x - n.pos.x) + s.y, std::fabs(tip.y - n.pos.y) + s.y };
        }
        return { n.pos.x + n.size.x / 2 + gap, n.pos.y - s.y / 2, s.x, s.y };
    }
    case LabelPlace::Left: {
        if (n.labelAngle != 0) {
            const float a = (n.labelAngle + 180) * DEG2RAD, reach = n.size.x / 2 + gap + s.x;
            const Vector2 tip{ n.pos.x + std::cos(a) * reach, n.pos.y + std::sin(a) * reach };
            return { std::min(n.pos.x, tip.x) - s.y / 2, std::min(n.pos.y, tip.y) - s.y / 2,
                     std::fabs(tip.x - n.pos.x) + s.y, std::fabs(tip.y - n.pos.y) + s.y };
        }
        return { n.pos.x - n.size.x / 2 - gap - s.x, n.pos.y - s.y / 2, s.x, s.y };
    }
    }
    return {};
}

void fitGroups(Graph& g, float pad) {
    std::unordered_map<std::string, int> depth;
    std::function<int(const std::string&)> depthOf = [&](const std::string& id) -> int {
        auto it = depth.find(id);
        if (it != depth.end()) return it->second;
        depth[id] = 0;                                 // cycles stop here
        const Group* gr = g.group(id);
        const int d = gr && !gr->parent.empty() ? depthOf(gr->parent) + 1 : 0;
        depth[id] = d;
        return d;
    };
    std::vector<int> order(g.groups.size());
    std::iota(order.begin(), order.end(), 0);
    for (const Group& gr : g.groups) depthOf(gr.id);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return depth[g.groups[a].id] > depth[g.groups[b].id]; });
    for (int gi : order) {
        Group& gr = g.groups[gi];
        if (gr.fixed) continue;
        Rectangle r{};
        bool any = false;
        for (const Node& n : g.nodes) {
            if (n.group != gr.id) continue;
            grow(r, any, { n.pos.x - n.size.x / 2, n.pos.y - n.size.y / 2, n.size.x, n.size.y });
            if (!n.label.empty() && n.labelPlace != LabelPlace::Inside) grow(r, any, labelRect(n));
        }
        for (const Group& kid : g.groups) if (kid.parent == gr.id && kid.rect.width > 0) grow(r, any, kid.rect);
        if (!any) continue;
        const float top = gr.label.empty() ? pad : pad * 0.6f + gr.labelSize.y + pad * 0.5f;
        r = { r.x - pad, r.y - top, r.width + 2 * pad, r.height + top + pad };
        if (r.width < gr.labelSize.x + 2 * pad) {
            const float extra = gr.labelSize.x + 2 * pad - r.width;
            r.x -= extra / 2; r.width += extra;
        }
        gr.rect = r;
    }
}

float pathLength(const std::vector<Vector2>& pts) {
    float s = 0;
    for (size_t i = 1; i < pts.size(); i++) s += std::hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
    return s;
}

Vector2 pointAt(const std::vector<Vector2>& pts, float s, Vector2* tangent) {
    if (pts.empty()) return { 0, 0 };
    if (pts.size() == 1) { if (tangent) *tangent = { 1, 0 }; return pts[0]; }
    for (size_t i = 1; i < pts.size(); i++) {
        const float dx = pts[i].x - pts[i - 1].x, dy = pts[i].y - pts[i - 1].y, l = std::hypot(dx, dy);
        if (s <= l || i + 1 == pts.size()) {
            const float u = l > 1e-6f ? std::clamp(s / l, 0.f, 1.f) : 0.f;
            if (tangent) *tangent = l > 1e-6f ? Vector2{ dx / l, dy / l } : Vector2{ 1, 0 };
            return { pts[i - 1].x + dx * u, pts[i - 1].y + dy * u };
        }
        s -= l;
    }
    return pts.back();
}

std::vector<Vector2> samplePath(const std::vector<Vector2>& pts, bool curved) {
    if (!curved || pts.size() < 3) return pts;
    std::vector<Vector2> out;
    const size_t n = pts.size();
    auto P = [&](long i) { return pts[(size_t)std::clamp(i, 0L, (long)n - 1)]; };
    for (size_t i = 0; i + 1 < n; i++) {
        // Centripetal Catmull-Rom through pts[i]..pts[i+1]: no cusps or overshoot loops.
        const Vector2 p0 = P((long)i - 1), p1 = P((long)i), p2 = P((long)i + 1), p3 = P((long)i + 2);
        auto knot = [](Vector2 a, Vector2 b) { return std::max(1e-4f, std::sqrt(std::hypot(b.x - a.x, b.y - a.y))); };
        const float t0 = 0, t1 = t0 + (i == 0 ? 1e-4f + knot(p1, p2) : knot(p0, p1)), t2 = t1 + knot(p1, p2),
                    t3 = t2 + (i + 2 >= n ? 1e-4f + knot(p1, p2) : knot(p2, p3));
        const int segs = std::clamp((int)(std::hypot(p2.x - p1.x, p2.y - p1.y) / 6), 4, 32);
        for (int k = (i == 0 ? 0 : 1); k <= segs; k++) {
            const float t = t1 + (t2 - t1) * k / segs;
            auto lerp = [](Vector2 a, Vector2 b, float ta, float tb, float tt) {
                const float u = (tt - ta) / (tb - ta);
                return Vector2{ a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u };
            };
            Vector2 q0 = p0, q3 = p3;
            if (i == 0) q0 = { 2 * p1.x - p2.x, 2 * p1.y - p2.y };
            if (i + 2 >= n) q3 = { 2 * p2.x - p1.x, 2 * p2.y - p1.y };
            const Vector2 A1 = lerp(q0, p1, t0, t1, t), A2 = lerp(p1, p2, t1, t2, t), A3 = lerp(p2, q3, t2, t3, t);
            const Vector2 B1 = lerp(A1, A2, t0, t2, t), B2 = lerp(A2, A3, t1, t3, t);
            out.push_back(lerp(B1, B2, t1, t2, t));
        }
    }
    return out;
}

// ------------------------------------------------------------------ stress majorization
void stress(std::vector<double>& x, int n, int dim, const std::vector<double>& d, int iterations,
            bool init, const std::vector<double>* wts) {
    x.resize((size_t)n * dim, 0.0);
    if (n == 0) return;
    if (n == 1) { std::fill(x.begin(), x.end(), 0.0); return; }
    auto D = [&](int i, int j) { return d[(size_t)i * n + j]; };
    if (!init) {
        // Classical MDS: top `dim` eigenvectors of the double-centred squared distances.
        std::vector<double> B((size_t)n * n), row(n, 0.0);
        double all = 0, maxd = 0;
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) maxd = std::max(maxd, D(i, j));
        auto dd = [&](int i, int j) { const double v = D(i, j) < 0 ? maxd * 1.5 : D(i, j); return v * v; };
        for (int i = 0; i < n; i++) { for (int j = 0; j < n; j++) row[i] += dd(i, j); row[i] /= n; all += row[i]; }
        all /= n;
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) B[(size_t)i * n + j] = -0.5 * (dd(i, j) - row[i] - row[j] + all);
        std::vector<std::vector<double>> vecs;
        for (int k = 0; k < dim; k++) {
            std::vector<double> v(n), nv(n);
            for (int i = 0; i < n; i++) v[i] = std::sin(i * 1.618 + k * 2.3 + 0.5) + 0.01 * i;
            double lambda = 0;
            for (int it = 0; it < 120; it++) {
                for (const auto& u : vecs) {                 // deflate: stay orthogonal to earlier ones
                    double p = 0; for (int i = 0; i < n; i++) p += v[i] * u[i];
                    for (int i = 0; i < n; i++) v[i] -= p * u[i];
                }
                for (int i = 0; i < n; i++) { double s = 0; for (int j = 0; j < n; j++) s += B[(size_t)i * n + j] * v[j]; nv[i] = s; }
                double norm = 0; for (double q : nv) norm += q * q;
                norm = std::sqrt(norm);
                if (norm < 1e-12) break;
                lambda = norm;
                for (int i = 0; i < n; i++) v[i] = nv[i] / norm;
            }
            vecs.push_back(v);
            const double s = std::sqrt(std::max(lambda, 1e-6));
            for (int i = 0; i < n; i++) x[(size_t)i * dim + k] = v[i] * s;
        }
        // Break exact ties (symmetric graphs) deterministically.
        for (int i = 0; i < n; i++) for (int k = 0; k < dim; k++) x[(size_t)i * dim + k] += 1e-3 * std::sin(i * 12.9898 + k * 78.233);
    }
    std::vector<double> num(dim);
    for (int it = 0; it < iterations; it++) {
        for (int i = 0; i < n; i++) {
            std::fill(num.begin(), num.end(), 0.0);
            double den = 0;
            for (int j = 0; j < n; j++) {
                if (j == i) continue;
                const double dij = D(i, j);
                if (dij <= 0) continue;
                double w = 1.0 / (dij * dij);
                if (wts) w *= (*wts)[(size_t)i * n + j];
                double dist = 0;
                for (int k = 0; k < dim; k++) { const double q = x[(size_t)i * dim + k] - x[(size_t)j * dim + k]; dist += q * q; }
                dist = std::sqrt(dist);
                if (dist < 1e-9) dist = 1e-9;
                for (int k = 0; k < dim; k++) {
                    const double xi = x[(size_t)i * dim + k], xj = x[(size_t)j * dim + k];
                    num[k] += w * (xj + dij * (xi - xj) / dist);
                }
                den += w;
            }
            if (den > 0) for (int k = 0; k < dim; k++) x[(size_t)i * dim + k] = num[k] / den;
        }
    }
}

} // namespace detail

// ------------------------------------------------------------------ sizing
void sizeNodes(Graph& g, const Theme& th) {
    const Font font = th.font.texture.id ? th.font : fonts::ui(false);
    const Font bold = th.bold.texture.id ? th.bold : fonts::ui(true);
    TextStyle st;
    st.font = font; st.size = th.fontSize; st.lineHeight = th.lineHeight;
    auto block = [&](const std::string& text, const TextStyle& s, float maxW, std::vector<std::string>& lines) {
        lines.clear();
        if (text.empty()) return Vector2{ 0, 0 };
        lines = wrapText(text, s, maxW);
        float w = 0;
        for (const std::string& l : lines) w = std::max(w, MeasureTextEx(s.font, l.c_str(), s.size, s.spacing).x);
        return Vector2{ w, lines.size() * s.size * s.lineHeight };
    };
    for (Node& n : g.nodes) {
        TextStyle ls = st;
        if (n.shape == Shape::Point || n.shape == Shape::Event || n.shape == Shape::Gateway ||
            n.shape == Shape::DataObject || n.shape == Shape::DataStore) ls.size = th.fontSize * 0.8f;
        const float maxW = n.shape == Shape::Text ? th.maxLabelWidth * 1.4f : th.maxLabelWidth;
        const Vector2 t = block(n.label, ls, maxW, n.labelLines);
        n.labelSize = t;
        if (n.size.x > 0 && n.size.y > 0) continue;
        const float px = th.padX, py = th.padY;
        const float tw = std::max(t.x, th.fontSize), thh = std::max(t.y, th.fontSize * th.lineHeight);
        Vector2 s{ tw + 2 * px, thh + 2 * py };
        switch (n.shape) {
        case Shape::Stadium: s.x += s.y * 0.4f; break;
        case Shape::Circle: case Shape::DoubleCircle: {
            const float d = std::sqrt(tw * tw + thh * thh) + py * 1.5f + (n.shape == Shape::DoubleCircle ? 10 : 0);
            s = { d, d }; break;
        }
        case Shape::Ellipse: s = { (tw + px) * 1.35f, (thh + py) * 1.45f }; break;
        case Shape::Diamond: {
            float w = 2 * tw, h = 2 * thh;
            if (h < 0.5f * w) { w = tw + thh / 0.5f; h = 0.5f * w; }
            s = { w + px, h + py }; break;
        }
        case Shape::Hexagon: s.x += s.y * 0.5f; break;
        case Shape::Parallelogram: case Shape::ParallelogramAlt: case Shape::Trapezoid: case Shape::TrapezoidAlt:
            s.x += s.y * 0.6f; break;
        case Shape::Cylinder: s.y += 22; break;
        case Shape::Subroutine: s.x += 20; break;
        case Shape::Flag: s.x += s.y * 0.3f; break;
        case Shape::Note: case Shape::Document: s.y += 8; break;
        case Shape::Point: s = { 12, 12 }; break;
        case Shape::Text: s = { tw + 8, thh + 4 }; break;
        case Shape::StartState: s = { 26, 26 }; break;
        case Shape::EndState: s = { 30, 30 }; break;
        case Shape::Task: s = { std::max(120.f, s.x), std::max(80.f, s.y) }; break;
        case Shape::Event: s = { 40, 40 }; break;
        case Shape::Gateway: s = { 54, 54 }; break;
        case Shape::DataObject: s = { 40, 52 }; break;
        case Shape::DataStore: s = { 56, 54 }; break;
        case Shape::Annotation: s = { tw + 2 * px, thh + 2 * py }; break;
        default: break;
        }
        if (n.size.x <= 0) n.size.x = s.x;
        if (n.size.y <= 0) n.size.y = s.y;
    }
    TextStyle es = st;
    es.size = th.fontSize * 0.82f;
    for (Edge& e : g.edges) {
        e.labelSize = block(e.label, es, th.maxLabelWidth, e.labelLines);
        if (e.labelSize.x > 0) e.labelSize = { e.labelSize.x + 12, e.labelSize.y + 6 };
    }
    TextStyle gs = st;
    gs.font = bold; gs.size = th.fontSize * 0.86f;
    for (Group& gr : g.groups) {
        std::vector<std::string> lines;
        gr.labelSize = block(gr.label, gs, 1e6f, lines);
    }
}

// ------------------------------------------------------------------ layouts
namespace {
using detail::boundaryPoint;

// Where a port line leaves the node: from `from` (inside, offset across the rank) towards
// `to`, the last crossing of the outline; the plain boundary point when the offset line
// misses the shape.
Vector2 portPoint(const Node& n, Vector2 from, Vector2 to, float corner) {
    if (n.shape == Shape::Point && n.size.x < 1) return n.pos;
    const std::vector<Vector2> o = detail::outline(n, corner);
    const float dx = to.x - from.x, dy = to.y - from.y;
    float best = -1;
    for (size_t i = 0; i < o.size(); i++) {
        const Vector2 a = o[i], b = o[(i + 1) % o.size()];
        const float ex = b.x - a.x, ey = b.y - a.y;
        const float den = dx * ey - dy * ex;
        if (std::fabs(den) < 1e-9f) continue;
        const float u = ((a.x - from.x) * ey - (a.y - from.y) * ex) / den;
        const float v = ((a.x - from.x) * dy - (a.y - from.y) * dx) / den;
        if (u >= 0 && u <= 1 && v >= -1e-4f && v <= 1 + 1e-4f && u > best) best = u;
    }
    if (best < 0) return boundaryPoint(n, to, corner);
    return { from.x + dx * best, from.y + dy * best };
}

// Distance between two points.
float dist(Vector2 a, Vector2 b) { return std::hypot(a.x - b.x, a.y - b.y); }

// Edges between the same unordered pair, numbered so parallel ones can bend apart.
void routeStraight(Graph& g, float corner, bool onlyUnrouted) {
    std::map<std::pair<std::string, std::string>, int> count, seen;
    for (const Edge& e : g.edges) if (e.from != e.to) count[std::minmax(e.from, e.to)]++;
    std::unordered_map<std::string, int> loops;
    for (Edge& e : g.edges) {
        if (onlyUnrouted && !e.points.empty()) continue;
        if (e.fixed) continue;
        const Node* a = g.node(e.from);
        const Node* b = g.node(e.to);
        if (!a || !b) continue;
        if (e.from == e.to) { e.points = detail::selfLoop(*a, loops[e.from]++); e.curved = true; continue; }
        const auto key = std::minmax(e.from, e.to);
        const int m = count[key], k = seen[key]++;
        float bend = (k - (m - 1) / 2.f) * 46.f;
        if (e.from > e.to) bend = -bend;               // the same side whichever way it points
        e.points = detail::straightRoute(*a, *b, bend, corner);
        e.curved = true;
        e.labelPos.reset();
    }
}

void setLabelPositions(Graph& g) {
    for (Edge& e : g.edges) {
        if (e.label.empty() || e.labelPos || e.points.empty()) continue;
        const std::vector<Vector2> path = detail::samplePath(e.points, e.curved);
        e.labelPos = detail::pointAt(path, detail::pathLength(path) / 2);
    }
}

// Weighted isotonic regression with minimum gaps: x[i+1] - x[i] >= sep[i], minimising
// sum w[i] (x[i] - want[i])^2 (pool adjacent violators).
void placeRow(std::vector<double>& x, const std::vector<double>& want, const std::vector<double>& w,
              const std::vector<double>& sep) {
    const size_t n = want.size();
    std::vector<double> off(n, 0.0);
    for (size_t i = 1; i < n; i++) off[i] = off[i - 1] + sep[i - 1];
    struct Block { double v, w; size_t count; };
    std::vector<Block> st;
    for (size_t i = 0; i < n; i++) {
        st.push_back({ want[i] - off[i], std::max(1e-6, w[i]), 1 });
        while (st.size() >= 2 && st[st.size() - 2].v > st.back().v) {
            Block b = st.back(); st.pop_back();
            Block& a = st.back();
            a.v = (a.v * a.w + b.v * b.w) / (a.w + b.w);
            a.w += b.w; a.count += b.count;
        }
    }
    x.resize(n);
    size_t i = 0;
    for (const Block& b : st) for (size_t k = 0; k < b.count; k++, i++) x[i] = b.v + off[i];
}

struct LNode {
    int node = -1;                    // graph node index, -1 for a dummy
    int edge = -1;                    // dummies: the edge they belong to
    bool label = false;               // dummy carrying the edge label
    float across = 0, along = 0, realAlong = 0;
    int rank = 0;
    double x = 0;
    std::vector<int> path;            // enclosing groups, outermost first
};

void layered(Graph& g, const LayoutOptions& o, Direction dir, float corner) {
    const bool lr = dir == Direction::LR || dir == Direction::RL;
    std::unordered_map<std::string, int> gidx;
    for (size_t i = 0; i < g.groups.size(); i++) gidx[g.groups[i].id] = (int)i;
    auto groupPath = [&](const std::string& start) {
        std::vector<int> p;
        std::string cur = start;
        for (int guard = 0; !cur.empty() && guard < 64; guard++) {
            auto it = gidx.find(cur);
            if (it == gidx.end()) break;
            p.push_back(it->second);
            cur = g.groups[it->second].parent;
        }
        std::reverse(p.begin(), p.end());
        return p;
    };

    std::vector<LNode> ln;
    std::unordered_map<std::string, int> idx;
    for (size_t i = 0; i < g.nodes.size(); i++) {
        const Node& n = g.nodes[i];
        if (n.fixed) continue;
        LNode l;
        l.node = (int)i;
        l.across = lr ? n.size.y : n.size.x;
        l.along = l.realAlong = lr ? n.size.x : n.size.y;
        if (!n.label.empty() && n.labelPlace != LabelPlace::Inside) {
            const bool sideways = n.labelPlace == LabelPlace::Right || n.labelPlace == LabelPlace::Left;
            if (sideways == lr) l.along += (lr ? n.labelSize.x : n.labelSize.y) + 8;
            else l.across = std::max(l.across, lr ? n.labelSize.y : n.labelSize.x);
        }
        l.path = groupPath(n.group);
        idx[n.id] = (int)ln.size();
        ln.push_back(std::move(l));
    }
    const int N = (int)ln.size();
    if (N == 0) return;

    struct LE { int u, v, minlen, edge; bool reversed = false; };
    std::vector<LE> E;
    bool labels = false;
    for (size_t i = 0; i < g.edges.size(); i++) {
        const Edge& e = g.edges[i];
        if (e.fixed || e.from == e.to) continue;
        auto a = idx.find(e.from), b = idx.find(e.to);
        if (a == idx.end() || b == idx.end()) continue;
        E.push_back({ a->second, b->second, std::max(1, e.minLength), (int)i });
        if (!e.label.empty()) labels = true;
    }
    const int mult = labels ? 2 : 1;
    for (LE& e : E) e.minlen *= mult;

    // Break cycles: depth first from the sources, reversing edges back onto the stack.
    {
        std::vector<std::vector<int>> out(N);
        std::vector<int> indeg(N, 0);
        for (size_t k = 0; k < E.size(); k++) { out[E[k].u].push_back((int)k); indeg[E[k].v]++; }
        std::vector<int> state(N, 0), starts;
        for (int i = 0; i < N; i++) if (indeg[i] == 0) starts.push_back(i);
        for (int i = 0; i < N; i++) if (indeg[i] != 0) starts.push_back(i);
        for (int s : starts) {
            if (state[s]) continue;
            std::vector<std::pair<int, size_t>> st{ { s, 0 } };
            state[s] = 1;
            while (!st.empty()) {
                auto& [v, k] = st.back();
                if (k < out[v].size()) {
                    LE& e = E[out[v][k++]];
                    if (state[e.v] == 1) e.reversed = true;
                    else if (state[e.v] == 0) { state[e.v] = 1; st.push_back({ e.v, 0 }); }
                } else { state[v] = 2; st.pop_back(); }
            }
        }
    }
    auto src = [&](const LE& e) { return e.reversed ? e.v : e.u; };
    auto dst = [&](const LE& e) { return e.reversed ? e.u : e.v; };

    // Ranks: longest path from the sources, then sources pulled down next to their targets.
    std::vector<int> topo;
    {
        std::vector<std::vector<int>> out(N);
        std::vector<int> indeg(N, 0);
        for (size_t k = 0; k < E.size(); k++) { out[src(E[k])].push_back((int)k); indeg[dst(E[k])]++; }
        std::vector<int> q;
        for (int i = 0; i < N; i++) if (indeg[i] == 0) q.push_back(i);
        for (size_t h = 0; h < q.size(); h++) {
            const int v = q[h];
            topo.push_back(v);
            for (int k : out[v]) {
                const int w = dst(E[k]);
                ln[w].rank = std::max(ln[w].rank, ln[v].rank + E[k].minlen);
                if (--indeg[w] == 0) q.push_back(w);
            }
        }
        std::vector<int> indegAll(N, 0);
        for (const LE& e : E) indegAll[dst(e)]++;
        for (auto it = topo.rbegin(); it != topo.rend(); ++it) {
            const int v = *it;
            if (indegAll[v] != 0 || out[v].empty()) continue;
            int r = std::numeric_limits<int>::max();
            for (int k : out[v]) r = std::min(r, ln[dst(E[k])].rank - E[k].minlen);
            ln[v].rank = r;
        }
        int lo = std::numeric_limits<int>::max();
        for (const LNode& l : ln) lo = std::min(lo, l.rank);
        for (LNode& l : ln) l.rank -= lo;
    }

    // Dummies along edges spanning several ranks; the middle one of a labelled edge carries it.
    auto lca = [](const std::vector<int>& a, const std::vector<int>& b) {
        std::vector<int> p;
        for (size_t i = 0; i < a.size() && i < b.size() && a[i] == b[i]; i++) p.push_back(a[i]);
        return p;
    };
    std::vector<std::vector<int>> chains(E.size());
    for (size_t k = 0; k < E.size(); k++) {
        const int s = src(E[k]), t = dst(E[k]);
        const Edge& ge = g.edges[E[k].edge];
        const int span = ln[t].rank - ln[s].rank;
        chains[k].push_back(s);
        for (int r = ln[s].rank + 1; r < ln[t].rank; r++) {
            LNode d;
            d.edge = (int)k;
            d.rank = r;
            d.label = !ge.label.empty() && r == ln[s].rank + span / 2;
            if (d.label) {
                d.across = lr ? ge.labelSize.y : ge.labelSize.x;
                d.along = d.realAlong = lr ? ge.labelSize.x : ge.labelSize.y;
            }
            d.path = lca(ln[s].path, ln[t].path);
            chains[k].push_back((int)ln.size());
            ln.push_back(std::move(d));
        }
        chains[k].push_back(t);
    }
    const int M = (int)ln.size();
    int maxRank = 0;
    for (const LNode& l : ln) maxRank = std::max(maxRank, l.rank);
    // Adjacent-rank segments, weighted to keep long edges straight.
    std::vector<std::vector<std::pair<int, double>>> up(M), down(M);
    for (const auto& ch : chains)
        for (size_t i = 0; i + 1 < ch.size(); i++) {
            const int a = ch[i], b = ch[i + 1];
            const int dummies = (ln[a].node < 0) + (ln[b].node < 0);
            const double w = dummies == 2 ? 8 : dummies == 1 ? 2 : 1;
            down[a].push_back({ b, w });
            up[b].push_back({ a, w });
        }

    // Initial order: breadth first over the ranks, in source order.
    std::vector<std::vector<int>> layers(maxRank + 1);
    {
        std::vector<char> seen(M, 0);
        std::vector<int> starts(N);
        std::iota(starts.begin(), starts.end(), 0);
        std::stable_sort(starts.begin(), starts.end(), [&](int a, int b) { return ln[a].rank < ln[b].rank; });
        for (int s : starts) {
            if (seen[s]) continue;
            std::queue<int> q;
            q.push(s); seen[s] = 1;
            while (!q.empty()) {
                const int v = q.front(); q.pop();
                layers[ln[v].rank].push_back(v);
                for (auto [w, _] : down[v]) if (!seen[w]) { seen[w] = 1; q.push(w); }
                for (auto [w, _] : up[v]) if (!seen[w]) { seen[w] = 1; q.push(w); }
            }
        }
    }
    std::vector<int> pos(M, 0);
    auto index = [&] { for (auto& L : layers) for (size_t i = 0; i < L.size(); i++) pos[L[i]] = (int)i; };
    index();

    // Sorts a layer by barycentre, keeping each group's members together (recursively).
    std::vector<double> bary(M, 0.0);
    std::function<void(std::vector<int>&, size_t)> groupSort = [&](std::vector<int>& items, size_t depth) {
        struct Part { int key; std::vector<int> items; double mean = 0; };
        std::vector<Part> parts;
        std::unordered_map<int, size_t> where;
        for (int it : items) {
            const int key = ln[it].path.size() > depth ? ln[it].path[depth] : -1 - it;
            auto f = where.find(key);
            if (f == where.end()) { where[key] = parts.size(); parts.push_back({ key, { it } }); }
            else parts[f->second].items.push_back(it);
        }
        for (Part& p : parts) {
            for (int it : p.items) p.mean += bary[it];
            p.mean /= p.items.size();
            if (p.key >= 0) groupSort(p.items, depth + 1);
        }
        std::stable_sort(parts.begin(), parts.end(), [](const Part& a, const Part& b) { return a.mean < b.mean; });
        items.clear();
        for (const Part& p : parts) items.insert(items.end(), p.items.begin(), p.items.end());
    };
    auto crossings = [&]() {
        long long total = 0;
        for (int r = 0; r + 1 <= maxRank; r++) {
            std::vector<std::pair<int, int>> segs;
            for (int a : layers[r]) for (auto [b, _] : down[a]) segs.push_back({ pos[a], pos[b] });
            std::sort(segs.begin(), segs.end());
            std::vector<int> bit(layers[r + 1].size() + 2, 0);
            long long seenCount = 0;
            for (auto [a, b] : segs) {
                int le = 0;                              // inserted with b' <= b
                for (int i = b + 1; i > 0; i -= i & -i) le += bit[i];
                total += seenCount - le;
                for (int i = b + 1; i < (int)bit.size(); i += i & -i) bit[i]++;
                seenCount++;
            }
        }
        return total;
    };
    {
        auto best = layers;
        long long bestC = crossings();
        for (int it = 0; it < 24 && bestC > 0; it++) {
            const bool goingDown = it % 2 == 0;
            for (int s = 0; s <= maxRank; s++) {
                const int r = goingDown ? s : maxRank - s;
                auto& L = layers[r];
                for (int v : L) {
                    const auto& nb = goingDown ? up[v] : down[v];
                    if (nb.empty()) { bary[v] = pos[v]; continue; }
                    double sum = 0, ws = 0;
                    for (auto [w, wt] : nb) { sum += pos[w] * wt; ws += wt; }
                    bary[v] = sum / ws;
                }
                groupSort(L, 0);
                for (size_t i = 0; i < L.size(); i++) pos[L[i]] = (int)i;
            }
            // Transpose: swap neighbours (in the same group) when that removes crossings.
            for (int pass = 0; pass < 2; pass++)
                for (int r = 0; r <= maxRank; r++) {
                    auto& L = layers[r];
                    for (size_t i = 0; i + 1 < L.size(); i++) {
                        const int a = L[i], b = L[i + 1];
                        if (ln[a].path != ln[b].path) continue;
                        auto cross = [&](int x, int y) {        // crossings with x before y
                            long long c = 0;
                            for (auto [p, _] : up[x]) for (auto [q, __] : up[y]) c += pos[p] > pos[q];
                            for (auto [p, _] : down[x]) for (auto [q, __] : down[y]) c += pos[p] > pos[q];
                            return c;
                        };
                        if (cross(b, a) < cross(a, b)) {
                            std::swap(L[i], L[i + 1]);
                            pos[L[i]] = (int)i; pos[L[i + 1]] = (int)i + 1;
                        }
                    }
                }
            const long long c = crossings();
            if (c < bestC) { bestC = c; best = layers; }
        }
        layers = best;
        index();
    }

    // Across coordinates: each rank placed as near its neighbours as the gaps allow.
    const float pad = 18;
    const bool labelAcross = lr, labelStart = dir == Direction::TB, labelEnd = dir == Direction::BT;
    auto groupLabel = [&](int gi) { return g.groups[gi].label.empty() ? 0.f : g.groups[gi].labelSize.y + 6; };
    auto sepOf = [&](int a, int b) {
        const LNode& A = ln[a];
        const LNode& B = ln[b];
        const bool realA = A.node >= 0 || A.label, realB = B.node >= 0 || B.label;
        double gap = realA && realB ? o.nodeSpacing : (realA || realB) ? o.nodeSpacing * 0.5 : o.nodeSpacing * 0.3;
        size_t k = 0;
        while (k < A.path.size() && k < B.path.size() && A.path[k] == B.path[k]) k++;
        gap += pad * (A.path.size() - k) + pad * (B.path.size() - k);
        if (labelAcross) for (size_t i = k; i < B.path.size(); i++) gap += groupLabel(B.path[i]);
        return (A.across + B.across) / 2 + gap;
    };
    for (auto& L : layers) {
        double x = 0;
        for (size_t i = 0; i < L.size(); i++) {
            if (i > 0) x += sepOf(L[i - 1], L[i]);
            ln[L[i]].x = x;
        }
    }
    auto relax = [&](int r, bool useUp, bool useDown) {
        auto& L = layers[r];
        if (L.empty()) return;
        std::vector<double> want(L.size()), w(L.size()), sep(L.size() > 0 ? L.size() - 1 : 0), x;
        for (size_t i = 0; i < L.size(); i++) {
            const int v = L[i];
            double sum = 0, ws = 0;
            if (useUp) for (auto [u, wt] : up[v]) { sum += ln[u].x * wt; ws += wt; }
            if (useDown) for (auto [u, wt] : down[v]) { sum += ln[u].x * wt; ws += wt; }
            want[i] = ws > 0 ? sum / ws : ln[v].x;
            w[i] = ws > 0 ? ws : 0.2;
            if (i > 0) sep[i - 1] = sepOf(L[i - 1], L[i]);
        }
        placeRow(x, want, w, sep);
        for (size_t i = 0; i < L.size(); i++) ln[L[i]].x = x[i];
    };
    for (int it = 0; it < 10; it++) {
        for (int r = 1; r <= maxRank; r++) relax(r, true, false);
        for (int r = maxRank - 1; r >= 0; r--) relax(r, false, true);
    }
    for (int it = 0; it < 4; it++) for (int r = 0; r <= maxRank; r++) relax(r, true, true);

    // Along coordinates: rank thickness, gaps and room for group padding and labels.
    std::vector<int> gStart(g.groups.size(), std::numeric_limits<int>::max()), gEnd(g.groups.size(), -1);
    for (const LNode& l : ln)
        if (l.node >= 0) for (int gi : l.path) { gStart[gi] = std::min(gStart[gi], l.rank); gEnd[gi] = std::max(gEnd[gi], l.rank); }
    std::vector<float> thick(maxRank + 1, 0), startX(maxRank + 1, 0), endX(maxRank + 1, 0), at(maxRank + 1, 0);
    for (const LNode& l : ln) {
        thick[l.rank] = std::max(thick[l.rank], l.along);
        if (l.node < 0) continue;
        float s = 0, e = 0;
        for (int gi : l.path) {
            if (gStart[gi] == l.rank) s += pad + (labelStart ? groupLabel(gi) : 0);
            if (gEnd[gi] == l.rank) e += pad + (labelEnd ? groupLabel(gi) : 0);
        }
        startX[l.rank] = std::max(startX[l.rank], s);
        endX[l.rank] = std::max(endX[l.rank], e);
    }
    const float gap = o.rankSpacing / mult;
    float y = startX[0];
    for (int r = 0; r <= maxRank; r++) {
        at[r] = y + thick[r] / 2;
        y += thick[r] + endX[r] + gap + (r < maxRank ? startX[r + 1] : 0);
    }

    auto toFinal = [&](double x, double yy) -> Vector2 {
        switch (dir) {
        case Direction::TB: return { (float)x, (float)yy };
        case Direction::BT: return { (float)x, (float)-yy };
        case Direction::LR: return { (float)yy, (float)x };
        case Direction::RL: return { (float)-yy, (float)x };
        }
        return { (float)x, (float)yy };
    };
    for (const LNode& l : ln)
        if (l.node >= 0) {
            // Outside labels made the slot longer than the shape: keep the shape at the slot's start.
            const float shift = (l.along - l.realAlong) / 2;
            const Node& n = g.nodes[l.node];
            const bool after = n.labelPlace == LabelPlace::Below || n.labelPlace == LabelPlace::Right;
            g.nodes[l.node].pos = toFinal(l.x, at[l.rank] + (after ? -shift : shift));
        }

    // Routes: out of the source's far side, through the dummies, into the target's near side.
    std::vector<std::vector<std::pair<int, int>>> outs(M), ins(M);   // (chain, neighbour)
    for (size_t k = 0; k < chains.size(); k++) {
        outs[chains[k][0]].push_back({ (int)k, chains[k][1] });
        ins[chains[k].back()].push_back({ (int)k, chains[k][chains[k].size() - 2] });
    }
    std::vector<double> outOff(chains.size(), 0), inOff(chains.size(), 0);
    auto spread = [&](std::vector<std::pair<int, int>>& list, std::vector<double>& off, float across) {
        std::stable_sort(list.begin(), list.end(), [&](auto a, auto b) { return ln[a.second].x < ln[b.second].x; });
        const double step = list.size() > 1 ? std::min(16.0, across * 0.7 / (list.size() - 1)) : 0;
        for (size_t i = 0; i < list.size(); i++) off[list[i].first] = (i - (list.size() - 1) / 2.0) * step;
    };
    for (int v = 0; v < N; v++) { spread(outs[v], outOff, ln[v].across); spread(ins[v], inOff, ln[v].across); }
    const float stub = std::min(gap * 0.45f, 22.f);
    for (size_t k = 0; k < chains.size(); k++) {
        const auto& ch = chains[k];
        const LNode& s = ln[ch.front()];
        const LNode& t = ln[ch.back()];
        Edge& e = g.edges[E[k].edge];
        const Node& sn = g.nodes[s.node];
        const Node& tn = g.nodes[t.node];
        const float sAt = (float)(at[s.rank] + 0), tAt = at[t.rank];
        std::vector<Vector2> pts;
        const Vector2 sOut = toFinal(s.x + outOff[k], sAt + s.realAlong / 2 + stub);
        const Vector2 tIn = toFinal(t.x + inOff[k], tAt - t.realAlong / 2 - stub);
        const Vector2 sAim = toFinal(s.x + outOff[k], sAt);
        const Vector2 tAim = toFinal(t.x + inOff[k], tAt);
        pts.push_back(portPoint(sn, sAim, sOut, corner));
        pts.push_back(sOut);
        for (size_t i = 1; i + 1 < ch.size(); i++) {
            const LNode& d = ln[ch[i]];
            pts.push_back(toFinal(d.x, at[d.rank]));
            if (d.label) e.labelPos = pts.back();
        }
        pts.push_back(tIn);
        pts.push_back(portPoint(tn, tAim, tIn, corner));
        // A stub that overshoots the far end of a short edge would make it kink.
        if (ch.size() == 2 && dist(sOut, tIn) < stub) { pts.erase(pts.begin() + 1); pts.erase(pts.begin() + 1); }
        if (E[k].reversed) std::reverse(pts.begin(), pts.end());
        e.points = pts;
        e.curved = true;
    }
    std::unordered_map<std::string, int> loops;
    for (Edge& e : g.edges)
        if (e.from == e.to && !e.fixed)
            if (const Node* n = g.node(e.from)) { e.points = detail::selfLoop(*n, loops[e.from]++); e.curved = true; }
    routeStraight(g, corner, true);
}

// Stress layout of each connected component, then the components packed in rows.
void force(Graph& g, const LayoutOptions& o, const Graph* hint) {
    const int N = (int)g.nodes.size();
    std::unordered_map<std::string, int> idx;
    for (int i = 0; i < N; i++) idx[g.nodes[i].id] = i;
    std::vector<std::vector<int>> adj(N);
    for (const Edge& e : g.edges) {
        auto a = idx.find(e.from), b = idx.find(e.to);
        if (a == idx.end() || b == idx.end() || a->second == b->second) continue;
        adj[a->second].push_back(b->second);
        adj[b->second].push_back(a->second);
    }
    std::vector<int> comp(N, -1);
    std::vector<std::vector<int>> comps;
    for (int i = 0; i < N; i++) {
        if (comp[i] >= 0) continue;
        comps.push_back({});
        std::vector<int> st{ i };
        comp[i] = (int)comps.size() - 1;
        while (!st.empty()) {
            const int v = st.back(); st.pop_back();
            comps.back().push_back(v);
            for (int w : adj[v]) if (comp[w] < 0) { comp[w] = comp[i]; st.push_back(w); }
        }
        std::sort(comps.back().begin(), comps.back().end());
    }
    auto radius = [&](int i) { return 0.5f * std::hypot(g.nodes[i].size.x, g.nodes[i].size.y); };
    struct Placed { std::vector<int> nodes; Rectangle box; };
    std::vector<Placed> placed;
    for (const auto& c : comps) {
        const int n = (int)c.size();
        std::unordered_map<int, int> local;
        for (int i = 0; i < n; i++) local[c[i]] = i;
        std::vector<double> d((size_t)n * n, 0.0);
        for (int i = 0; i < n; i++) {                  // hop distances, breadth first
            std::vector<int> hop(n, -1);
            std::queue<int> q;
            hop[i] = 0; q.push(i);
            while (!q.empty()) {
                const int v = q.front(); q.pop();
                for (int w : adj[c[v]]) {
                    const int lw = local[w];
                    if (hop[lw] < 0) { hop[lw] = hop[v] + 1; q.push(lw); }
                }
            }
            for (int j = 0; j < n; j++)
                d[(size_t)i * n + j] = i == j ? 0 : hop[j] * o.edgeLength + 0.6 * (radius(c[i]) + radius(c[j]));
        }
        std::vector<double> x((size_t)n * 2, 0.0);
        bool seeded = false;
        if (hint) {
            int found = 0;
            std::vector<char> has(n, 0);
            for (int i = 0; i < n; i++)
                if (const Node* h = hint->node(g.nodes[c[i]].id)) { x[i * 2] = h->pos.x; x[i * 2 + 1] = h->pos.y; has[i] = 1; found++; }
            if (found > 0) {
                seeded = true;
                for (int pass = 0; pass < n; pass++) {   // new nodes start beside a placed neighbour
                    bool changed = false;
                    for (int i = 0; i < n; i++) {
                        if (has[i]) continue;
                        for (int w : adj[c[i]]) {
                            const int lw = local[w];
                            if (!has[lw]) continue;
                            const double a = i * 2.39996;
                            x[i * 2] = x[lw * 2] + std::cos(a) * o.edgeLength;
                            x[i * 2 + 1] = x[lw * 2 + 1] + std::sin(a) * o.edgeLength;
                            has[i] = 1; changed = true; break;
                        }
                    }
                    if (!changed) break;
                }
                for (int i = 0; i < n; i++)
                    if (!has[i]) { x[i * 2] = std::cos(i * 2.39996) * o.edgeLength * 2; x[i * 2 + 1] = std::sin(i * 2.39996) * o.edgeLength * 2; }
            }
        }
        int iters = o.iterations;
        if (n > 400) iters = std::max(30, (int)(iters * (400.0 / n) * (400.0 / n)));
        detail::stress(x, n, 2, d, iters, seeded, nullptr);
        if (!seeded && n > 2) {
            // Principal axis horizontal; the first node on the left.
            double mx = 0, my = 0;
            for (int i = 0; i < n; i++) { mx += x[i * 2]; my += x[i * 2 + 1]; }
            mx /= n; my /= n;
            double sxx = 0, syy = 0, sxy = 0;
            for (int i = 0; i < n; i++) {
                const double ax = x[i * 2] - mx, ay = x[i * 2 + 1] - my;
                sxx += ax * ax; syy += ay * ay; sxy += ax * ay;
            }
            const double th = 0.5 * std::atan2(2 * sxy, sxx - syy), cs = std::cos(-th), sn = std::sin(-th);
            for (int i = 0; i < n; i++) {
                const double ax = x[i * 2] - mx, ay = x[i * 2 + 1] - my;
                x[i * 2] = ax * cs - ay * sn; x[i * 2 + 1] = ax * sn + ay * cs;
            }
            if (x[0] > 0) for (int i = 0; i < n; i++) x[i * 2] = -x[i * 2];
        }
        // Push overlapping boxes apart along their smaller overlap.
        for (int it = 0; it < 60; it++) {
            bool moved = false;
            for (int i = 0; i < n; i++)
                for (int j = i + 1; j < n; j++) {
                    const Node& a = g.nodes[c[i]];
                    const Node& b = g.nodes[c[j]];
                    const double ox = (a.size.x + b.size.x) / 2 + 16 - std::fabs(x[i * 2] - x[j * 2]);
                    const double oy = (a.size.y + b.size.y) / 2 + 12 - std::fabs(x[i * 2 + 1] - x[j * 2 + 1]);
                    if (ox <= 0 || oy <= 0) continue;
                    moved = true;
                    if (ox < oy) {
                        const double s = (x[i * 2] < x[j * 2] || (x[i * 2] == x[j * 2] && i < j)) ? -1 : 1;
                        x[i * 2] += s * ox / 2; x[j * 2] -= s * ox / 2;
                    } else {
                        const double s = (x[i * 2 + 1] < x[j * 2 + 1] || (x[i * 2 + 1] == x[j * 2 + 1] && i < j)) ? -1 : 1;
                        x[i * 2 + 1] += s * oy / 2; x[j * 2 + 1] -= s * oy / 2;
                    }
                }
            if (!moved) break;
        }
        Placed p;
        p.nodes = c;
        bool any = false;
        for (int i = 0; i < n; i++) {
            Node& nd = g.nodes[c[i]];
            nd.pos = { (float)x[i * 2], (float)x[i * 2 + 1] };
            grow(p.box, any, { nd.pos.x - nd.size.x / 2, nd.pos.y - nd.size.y / 2, nd.size.x, nd.size.y });
        }
        placed.push_back(std::move(p));
    }
    if (!hint && placed.size() > 1) {
        // Largest first, in rows about as wide as a 16:9 frame of the whole.
        std::stable_sort(placed.begin(), placed.end(), [](const Placed& a, const Placed& b) { return a.nodes.size() > b.nodes.size(); });
        double area = 0;
        for (const Placed& p : placed) area += (p.box.width + 60) * (p.box.height + 60);
        const float rowW = std::max(placed[0].box.width, (float)std::sqrt(area * 16 / 9));
        float cx = 0, cy = 0, rowH = 0;
        for (Placed& p : placed) {
            if (cx > 0 && cx + p.box.width > rowW) { cx = 0; cy += rowH + 60; rowH = 0; }
            const float dx = cx - p.box.x, dy = cy - p.box.y;
            for (int i : p.nodes) { g.nodes[i].pos.x += dx; g.nodes[i].pos.y += dy; }
            cx += p.box.width + 60;
            rowH = std::max(rowH, p.box.height);
        }
    }
}

// Children of each node along a spanning tree (edges in order), the root, and leaf order.
struct Tree {
    std::vector<std::vector<int>> kids;
    std::vector<int> parent;
    std::vector<int> roots;
};
Tree spanningTree(const Graph& g) {
    const int N = (int)g.nodes.size();
    std::unordered_map<std::string, int> idx;
    for (int i = 0; i < N; i++) idx[g.nodes[i].id] = i;
    Tree t;
    t.kids.resize(N);
    t.parent.assign(N, -1);
    std::vector<int> indeg(N, 0);
    std::vector<std::vector<int>> out(N);
    for (const Edge& e : g.edges) {
        auto a = idx.find(e.from), b = idx.find(e.to);
        if (a == idx.end() || b == idx.end() || a->second == b->second) continue;
        out[a->second].push_back(b->second);
        indeg[b->second]++;
    }
    std::vector<char> seen(N, 0);
    auto grow = [&](int r) {
        std::queue<int> q;
        q.push(r); seen[r] = 1;
        while (!q.empty()) {
            const int v = q.front(); q.pop();
            for (int w : out[v]) if (!seen[w]) { seen[w] = 1; t.parent[w] = v; t.kids[v].push_back(w); q.push(w); }
        }
    };
    for (int i = 0; i < N; i++) if (indeg[i] == 0 && !seen[i]) { t.roots.push_back(i); grow(i); }
    for (int i = 0; i < N; i++) if (!seen[i]) { t.roots.push_back(i); grow(i); }
    return t;
}

void tree(Graph& g, const LayoutOptions& o, Direction dir, bool radial, float corner) {
    const int N = (int)g.nodes.size();
    if (N == 0) return;
    const Tree t = spanningTree(g);
    const bool lr = dir == Direction::LR || dir == Direction::RL;
    bool lengths = o.branchLengths;
    for (const Node& n : g.nodes) if (!n.data["length"].isNumber() && t.parent[&n - &g.nodes[0]] >= 0) { lengths = false; break; }
    bool dendro = true;                               // phylogenetic trees: tips and dots
    for (const Node& n : g.nodes) if (n.shape != Shape::Point) { dendro = false; break; }

    std::vector<double> depth(N, 0), across(N, 0);
    std::vector<int> level(N, 0);
    std::vector<int> order;
    std::function<void(int)> walk = [&](int v) {
        order.push_back(v);
        for (int w : t.kids[v]) {
            level[w] = level[v] + 1;
            depth[w] = depth[v] + (lengths ? std::max(0.0, g.nodes[w].data["length"].asNumber()) : 1.0);
            walk(w);
        }
    };
    for (int r : t.roots) walk(r);
    int maxLevel = 0;
    double maxDepth = 0;
    for (int i = 0; i < N; i++) { maxLevel = std::max(maxLevel, level[i]); maxDepth = std::max(maxDepth, depth[i]); }
    // Rank thickness per level (non-dendrogram trees space by the largest node).
    std::vector<float> thick(maxLevel + 1, 0);
    for (int i = 0; i < N; i++) thick[level[i]] = std::max(thick[level[i]], lr ? g.nodes[i].size.x : g.nodes[i].size.y);
    std::vector<float> levelAt(maxLevel + 1, 0);
    for (int l = 1; l <= maxLevel; l++) levelAt[l] = levelAt[l - 1] + thick[l - 1] / 2 + o.rankSpacing + thick[l] / 2;
    const double extent = std::max(420.0, (double)levelAt[maxLevel] * (dendro ? 1.0 : 1.0));
    auto along = [&](int v) {
        if (lengths && maxDepth > 0) return depth[v] / maxDepth * (dendro ? std::max(extent, 600.0) : extent);
        return (double)levelAt[level[v]];
    };

    // Leaves get consecutive slots; parents centre over their first and last child.
    double next = 0;
    std::vector<int> leaves;
    std::function<void(int)> place = [&](int v) {
        if (t.kids[v].empty()) {
            const Node& n = g.nodes[v];
            float size = lr ? n.size.y : n.size.x;
            if (dendro) size = std::max(size, lr ? n.labelSize.y : n.labelSize.y * 1.2f);
            if (!leaves.empty()) {
                const Node& prev = g.nodes[leaves.back()];
                float ps = lr ? prev.size.y : prev.size.x;
                if (dendro) ps = std::max(ps, lr ? prev.labelSize.y : prev.labelSize.y * 1.2f);
                next += (ps + size) / 2 + (dendro ? 10 : o.nodeSpacing);
            }
            across[v] = next;
            leaves.push_back(v);
            return;
        }
        for (int w : t.kids[v]) place(w);
        across[v] = (across[t.kids[v].front()] + across[t.kids[v].back()]) / 2;
    };
    for (int r : t.roots) { if (!leaves.empty()) next += o.nodeSpacing * 2; place(r); }

    if (radial) {
        const double span = 2 * PI * leaves.size() / std::max<size_t>(1, leaves.size() + (leaves.size() > 2 ? 0 : 1));
        const double total = std::max(1.0, next);
        double rmax = 0;
        for (int i = 0; i < N; i++) rmax = std::max(rmax, along(i));
        // Radius so neighbouring tips are not closer than their spacing.
        const double circumference = next + (leaves.empty() ? 0 : 30);
        const double scale = std::max(1.0, circumference / (2 * PI) / std::max(1.0, rmax));
        for (int i = 0; i < N; i++) {
            Node& n = g.nodes[i];
            const double a = across[i] / total * span - PI / 2, r = along(i) * scale;
            n.pos = { (float)(std::cos(a) * r), (float)(std::sin(a) * r) };
            if (t.kids[i].empty() && t.parent[i] >= 0) {
                double deg = a * RAD2DEG;
                const bool flip = std::cos(a) < 0;
                n.labelPlace = flip ? LabelPlace::Left : LabelPlace::Right;
                n.labelAngle = (float)(flip ? deg - 180 : deg);
                if (n.labelAngle == 0) n.labelAngle = 0.001f;
            }
        }
        for (Edge& e : g.edges) {
            if (e.fixed) continue;
            const Node* a = g.node(e.from);
            const Node* b = g.node(e.to);
            if (!a || !b) continue;
            const int ia = (int)(a - &g.nodes[0]), ib = (int)(b - &g.nodes[0]);
            if (t.parent[ib] != ia) continue;
            const double ra = along(ia) * scale, a0 = across[ia] / total * span - PI / 2, a1 = across[ib] / total * span - PI / 2;
            std::vector<Vector2> pts;
            const int segs = std::max(1, (int)(std::fabs(a1 - a0) * ra / 10));
            for (int k = 0; k <= segs; k++) {
                const double aa = a0 + (a1 - a0) * k / segs;
                pts.push_back({ (float)(std::cos(aa) * ra), (float)(std::sin(aa) * ra) });
            }
            pts.push_back(b->pos);
            if (!dendro) { pts.front() = boundaryPoint(*a, pts[std::min<size_t>(1, pts.size() - 1)], corner); pts.back() = boundaryPoint(*b, pts[pts.size() - 2], corner); }
            e.points = pts;
            e.curved = false;
            e.head = dendro ? Arrow::None : e.head;
        }
        routeStraight(g, corner, true);
        return;
    }

    auto toFinal = [&](double x, double y) -> Vector2 {
        switch (dir) {
        case Direction::TB: return { (float)x, (float)y };
        case Direction::BT: return { (float)x, (float)-y };
        case Direction::LR: return { (float)y, (float)x };
        case Direction::RL: return { (float)-y, (float)x };
        }
        return { (float)x, (float)y };
    };
    for (int i = 0; i < N; i++) g.nodes[i].pos = toFinal(across[i], along(i));
    for (Edge& e : g.edges) {
        if (e.fixed) continue;
        const Node* a = g.node(e.from);
        const Node* b = g.node(e.to);
        if (!a || !b) continue;
        const int ia = (int)(a - &g.nodes[0]), ib = (int)(b - &g.nodes[0]);
        if (t.parent[ib] != ia) continue;
        if (dendro) {                                   // phylogram elbow: across at the parent, then along
            e.points = { a->pos, toFinal(across[ib], along(ia)), b->pos };
        } else {
            const float sa = (float)along(ia), sb = (float)along(ib);
            const float halfA = (lr ? a->size.x : a->size.y) / 2, halfB = (lr ? b->size.x : b->size.y) / 2;
            const float mid = (sa + halfA + sb - halfB) / 2;
            e.points = { toFinal(across[ia], sa + halfA), toFinal(across[ia], mid), toFinal(across[ib], mid), toFinal(across[ib], sb - halfB) };
        }
        e.curved = false;
    }
    routeStraight(g, corner, true);
}

void circle(Graph& g, float corner) {
    const int N = (int)g.nodes.size();
    float perimeter = 0;
    for (const Node& n : g.nodes) perimeter += std::hypot(n.size.x, n.size.y) + 30;
    const float r = std::max(120.f, perimeter / (2 * (float)PI));
    for (int i = 0; i < N; i++) {
        const float a = (float)(2 * PI * i / std::max(1, N) - PI / 2);
        g.nodes[i].pos = { std::cos(a) * r, std::sin(a) * r };
    }
    routeStraight(g, corner, false);
}
} // namespace

void layout(Graph& g, const LayoutOptions& o, const Graph* hint) {
    const float corner = 10;
    LayoutKind kind = o.kind == LayoutKind::Auto ? g.layout : o.kind;
    if (kind == LayoutKind::Auto) kind = g.directed ? LayoutKind::Layered : LayoutKind::Force;
    const Direction dir = o.direction.value_or(g.direction);
    for (Edge& e : g.edges) if (!e.fixed) { e.points.clear(); e.labelPos.reset(); }
    switch (kind) {
    case LayoutKind::Layered: layered(g, o, dir, corner); break;
    case LayoutKind::Force: force(g, o, hint); routeStraight(g, corner, false); break;
    case LayoutKind::Tree: tree(g, o, dir, false, corner); break;
    case LayoutKind::Radial: tree(g, o, dir, true, corner); break;
    case LayoutKind::Circle: circle(g, corner); break;
    case LayoutKind::None: case LayoutKind::Auto: routeStraight(g, corner, true); break;
    }
    setLabelPositions(g);
    detail::fitGroups(g, 22);
}

} // namespace dg
} // namespace rmtn
