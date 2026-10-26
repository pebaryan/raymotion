// geo3d.cpp -- data on a 3D map: a surface that unrolls from a globe into a projected
// map, the base map on it, marks, extruded countries and flow arcs that follow it.
#include "rmtn/geo3d.hpp"
#include "rmtn/plot.hpp"
#include <earcut/earcut.hpp>
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <numeric>

namespace rmtn {
namespace geo3d {
namespace {

constexpr double RAD = 3.14159265358979323846 / 180.0;

float clamp01(float v) { return std::clamp(v, 0.f, 1.f); }

// What a shape's vertices were made for, so they are only recomputed when it changes.
struct Stamp {
    float morph = -1, height = -1;
    int version = -1;
    bool same(const Surface& s, float h) const { return morph == s.morph && version == s.version() && height == h; }
    void set(const Surface& s, float h) { morph = s.morph; version = s.version(); height = h; }
};

Mesh newMesh(int vertices, bool uv) {
    Mesh m{};
    m.vertexCount = vertices;
    m.triangleCount = vertices / 3;
    m.vertices = (float*)MemAlloc((unsigned)(vertices * 3 * sizeof(float)));
    m.normals = (float*)MemAlloc((unsigned)(vertices * 3 * sizeof(float)));
    if (uv) m.texcoords = (float*)MemAlloc((unsigned)(vertices * 2 * sizeof(float)));
    return m;
}
void put(float* a, int i, Vector3 v) { std::memcpy(&a[i * 3], &v, sizeof v); }

// The order of `values` from largest to smallest: each one's rank.
std::vector<int> ranks(const std::vector<double>& values) {
    std::vector<int> order(values.size()), rank(values.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return values[a] > values[b]; });
    for (size_t i = 0; i < order.size(); i++) rank[order[i]] = (int)i;
    return rank;
}

float grown(const std::function<float(int)>& grow, int t, float delay) {
    if (!grow) return 1;
    return clamp01(grow((int)std::lround(t - delay)));
}

// ---- countries as prisms

// A country's outline and inside, in lon/lat: the top's triangles and the walls' edges.
struct Shape {
    std::vector<LonLat> top;                        // triangles, anticlockwise from above
    std::vector<std::array<LonLat, 2>> edges;       // ring edges, outside on the right
};

// A longitude moved by whole turns into [c - 180, c + 180): the map's window round its centre.
double wrapNear(double lon, double c) {
    double d = std::fmod(lon - c + 180, 360.0);
    if (d < 0) d += 360;
    return c - 180 + d;
}

// Inside a polygon (outer ring then holes), even-odd, in plain lon/lat.
bool inside(const std::vector<geo::Ring>& polygon, LonLat p) {
    bool in = false;
    for (const geo::Ring& r : polygon)
        for (size_t i = 0, j = r.size() - 1; i < r.size(); j = i++)
            if ((r[i].lat > p.lat) != (r[j].lat > p.lat) &&
                p.lon < (r[j].lon - r[i].lon) * (p.lat - r[i].lat) / (r[j].lat - r[i].lat) + r[i].lon)
                in = !in;
    return in;
}

// Ring points no further apart than `step` degrees.
std::vector<LonLat> densify(const geo::Ring& r, double step) {
    std::vector<LonLat> out;
    for (size_t i = 0; i < r.size(); i++) {
        const LonLat a = r[i], b = r[(i + 1) % r.size()];
        const int n = std::max(1, (int)std::ceil(std::max(std::fabs(b.lon - a.lon), std::fabs(b.lat - a.lat)) / step));
        for (int k = 0; k < n; k++) out.push_back({ a.lon + (b.lon - a.lon) * k / n, a.lat + (b.lat - a.lat) * k / n });
    }
    return out;
}

// Longitudes are kept within the map's window round `centre` (so a piece of Russia east of
// the antimeridian stays beside the rest when the map is centred elsewhere); where the
// window's edge cuts a polygon, its smaller side is pressed against that edge.
Shape shapeOf(const geo::Geometry& g, double step, double centre) {
    Shape s;
    for (auto polygon : g.polygons) {
        if (polygon.empty() || polygon[0].size() < 3) continue;
        double lo = 1e9, hi = -1e9;
        for (geo::Ring& r : polygon)
            for (LonLat& p : r) {
                p.lon = wrapNear(p.lon, centre);
                lo = std::min(lo, p.lon);
                hi = std::max(hi, p.lon);
            }
        if (hi - lo > 180) {
            size_t east = 0, all = 0;
            for (const geo::Ring& r : polygon)
                for (const LonLat& p : r) { east += p.lon > centre; all++; }
            const bool keepEast = east * 2 >= all;
            const double edge = keepEast ? centre + 180 - 1e-6 : centre - 180 + 1e-6;
            for (geo::Ring& r : polygon)
                for (LonLat& p : r)
                    if ((p.lon > centre) != keepEast) p.lon = edge;
        }
        std::vector<std::vector<std::array<double, 2>>> poly;
        std::vector<LonLat> flat;
        for (const geo::Ring& ring : polygon) {
            const std::vector<LonLat> d = densify(ring, step);
            std::vector<std::array<double, 2>> pts;
            for (size_t i = 0; i < d.size(); i++) {
                pts.push_back({ d[i].lon, d[i].lat });
                s.edges.push_back({ d[i], d[(i + 1) % d.size()] });
            }
            flat.insert(flat.end(), d.begin(), d.end());
            poly.push_back(std::move(pts));
        }
        // Inner points on a grid (earcut takes one-point holes as Steiner points), so big
        // countries bend with the globe instead of cutting through it.
        double west = 1e9, east = -1e9, south = 1e9, north = -1e9;
        for (const LonLat& p : polygon[0]) {
            west = std::min(west, p.lon); east = std::max(east, p.lon);
            south = std::min(south, p.lat); north = std::max(north, p.lat);
        }
        for (double lat = std::ceil(south / step) * step; lat < north; lat += step)
            for (double lon = std::ceil(west / step) * step; lon < east; lon += step)
                if (inside(polygon, { lon, lat })) {
                    poly.push_back({ { lon, lat } });
                    flat.push_back({ lon, lat });
                }
        const std::vector<uint32_t> idx = mapbox::earcut<uint32_t>(poly);
        for (size_t i = 0; i + 2 < idx.size(); i += 3) {
            LonLat a = flat[idx[i]], b2 = flat[idx[i + 1]], c = flat[idx[i + 2]];
            const double cr = (b2.lon - a.lon) * (c.lat - a.lat) - (b2.lat - a.lat) * (c.lon - a.lon);
            if (cr < 0) std::swap(b2, c);
            s.top.insert(s.top.end(), { a, b2, c });
        }
    }
    return s;
}

} // namespace

// ================================================================ the surface

Surface::Surface(geo::Projection map, LonLat centre, float radius, float width)
    : centre_(centre), radius_(radius), width_(width > 0 ? width : 2 * PI * radius) {
    // Turn the globe so `centre` faces +z: about y by its longitude, then about x by its latitude.
    spin_ = MatrixMultiply(MatrixRotateY((float)(-centre.lon * RAD)), MatrixRotateX((float)(centre.lat * RAD)));
    setMap(map);
    version_ = 0;
}

void Surface::setMap(const geo::Projection& map) {
    map_ = map;
    map_.rotate(centre_, map.roll());
    // Fitted to 1000 px across (the box is tall enough that the width decides), centred on 0.
    map_.fit({ -500, -50000, 1000, 100000 }, geo::Sphere);
    perPx_ = width_ / 1000.f;
    version_++;
}

Vector3 Surface::onGlobe(LonLat p, float height) const {
    return Vector3Transform(globe::point(p, radius_ + height), spin_);
}

Vector3 Surface::onMap(LonLat p, float height) const {
    const Vector2 px = map_(p);
    return { px.x * perPx_, -px.y * perPx_, radius_ + height };
}

Vector3 Surface::point(LonLat p, float height) const {
    if (morph <= 0) return onGlobe(p, height);
    if (morph >= 1) return onMap(p, height);
    return Vector3Lerp(onGlobe(p, height), onMap(p, height), morph);
}

Vector3 Surface::up(LonLat p) const {
    const Vector3 g = Vector3Transform(globe::point(p, 1), spin_);
    if (morph <= 0) return g;
    const Vector3 u = Vector3Lerp(g, { 0, 0, 1 }, clamp01(morph));
    const float l = Vector3Length(u);
    return l > 1e-6f ? Vector3Scale(u, 1 / l) : Vector3{ 0, 0, 1 };
}

Vector3 Surface::north(LonLat p) const {
    const double d = 0.05;
    const LonLat a{ p.lon, std::max(-90.0, p.lat - d) }, b{ p.lon, std::min(90.0, p.lat + d) };
    Vector3 n = Vector3Subtract(point(b), point(a));
    const Vector3 u = up(p);
    n = Vector3Subtract(n, Vector3Scale(u, Vector3DotProduct(n, u)));   // along the surface
    const float l = Vector3Length(n);
    return l > 1e-7f ? Vector3Scale(n, 1 / l) : Vector3{ 0, 1, 0 };
}

// ================================================================ the base map

g3d::Object3D* base(g3d::Object3D& parent, SurfacePtr s, const g3d::Material& material, int segments) {
    const int nx = std::max(8, segments), ny = std::max(4, segments / 2);
    const double west = s->centre().lon - 180;
    auto place = [nx, ny, west](int i, int j) { return LonLat{ west + 360.0 * i / nx, 90 - 180.0 * j / ny }; };
    auto geometry = std::make_shared<g3d::Geometry>([=] {
        Mesh m{};
        m.vertexCount = (nx + 1) * (ny + 1);
        m.triangleCount = nx * ny * 2;
        m.vertices = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
        m.normals = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
        m.texcoords = (float*)MemAlloc((unsigned)(m.vertexCount * 2 * sizeof(float)));
        m.indices = (unsigned short*)MemAlloc((unsigned)(m.triangleCount * 3 * sizeof(unsigned short)));
        for (int j = 0; j <= ny; j++)
            for (int i = 0; i <= nx; i++) {
                const int k = j * (nx + 1) + i;
                const LonLat p = place(i, j);
                put(m.vertices, k, s->point(p));
                put(m.normals, k, s->up(p));
                m.texcoords[k * 2] = (float)((p.lon + 180) / 360);   // past 1 beyond the antimeridian: the map repeats
                m.texcoords[k * 2 + 1] = (float)j / ny;
            }
        int t = 0;
        for (int j = 0; j < ny; j++)
            for (int i = 0; i < nx; i++) {
                const unsigned short a = (unsigned short)(j * (nx + 1) + i), b = (unsigned short)(a + 1),
                                     c = (unsigned short)(a + nx + 1), d = (unsigned short)(c + 1);
                const unsigned short tri[6] = { a, c, b, b, c, d };
                std::memcpy(&m.indices[t], tri, sizeof tri);
                t += 6;
            }
        UploadMesh(&m, true);
        return m;
    });
    g3d::Object3D* o = parent.add(geometry, material);
    o->name = "base";
    auto stamp = std::make_shared<Stamp>();
    stamp->set(*s, 0);
    o->up = [s, geometry, stamp, nx, ny, place](int, g3d::Object3D&) {
        if (stamp->same(*s, 0)) return;
        stamp->set(*s, 0);
        geometry->update([s, nx, ny, place](Mesh& m) {
            for (int j = 0; j <= ny; j++)
                for (int i = 0; i <= nx; i++) {
                    const int k = j * (nx + 1) + i;
                    const LonLat p = place(i, j);
                    put(m.vertices, k, s->point(p));
                    put(m.normals, k, s->up(p));
                }
        });
    };
    return o;
}

// ================================================================ data

std::vector<Datum> byCountry(const Values& values, const geo::Layer& layer) {
    std::vector<Datum> out;
    for (const auto& [key, v] : values)
        if (const geo::Feature* f = layer.find(key)) out.push_back({ f->labelPoint(), v, key, BLANK });
    return out;
}

Scale Scale::over(const std::vector<double>& values) const {
    Scale s = *this;
    if (s.min == s.max) {
        double lo = 0, hi = 0;
        bool any = false;
        for (double v : values)
            if (std::isfinite(v)) {
                lo = any ? std::min(lo, v) : v;
                hi = any ? std::max(hi, v) : v;
                any = true;
            }
        s.min = std::min(0.0, lo);
        s.max = hi > s.min ? hi : s.min + 1;
    }
    return s;
}

float Scale::t(double v) const { return max > min ? clamp01((float)((v - min) / (max - min))) : 0.f; }

Color Scale::operator()(double v) const { return colors ? colors(t(v)) : plot::viridis(t(v)); }

// ================================================================ marks

g3d::Object3D* marks(g3d::Object3D& parent, SurfacePtr s, const std::vector<Datum>& data, const MarkStyle& st) {
    g3d::Object3D* group = parent.add();
    group->name = "marks";
    std::vector<double> vals;
    for (const Datum& d : data) vals.push_back(d.value);
    const Scale scale = st.scale.over(vals);
    const std::vector<int> rank = ranks(vals);
    g3d::GeometryPtr shape;
    switch (st.mark) {
        case Mark::Column: shape = g3d::Geometry::box(1, 1, 1); break;
        case Mark::Spike: shape = g3d::Geometry::cone(0.5f, 1, 24); break;
        case Mark::Bubble: shape = g3d::Geometry::sphere(0.5f, 24, 32); break;
        case Mark::Disc: shape = g3d::Geometry::cylinder(0.5f, 1, 48); break;
    }
    const double top = std::max(std::fabs(scale.max), std::fabs(scale.min));
    for (size_t i = 0; i < data.size(); i++) {
        const Datum d = data[i];
        g3d::Object3D* holder = group->add();
        holder->name = d.key;
        g3d::Material m;
        m.color = d.color.a ? d.color : scale(d.value);
        m.roughness = st.roughness;
        m.unlit = st.unlit;
        g3d::Object3D* body = holder->add(shape, m);
        body->position = { 0, 0.5f, 0 };
        const float delay = rank[i] * st.stagger;
        holder->up = [s, d, st, scale, top, delay](int t, g3d::Object3D& self) {
            const double v = st.value ? st.value(t, d) : d.value;
            const float k = grown(st.grow, t, delay), f = top > 0 ? (float)(std::fabs(v) / top) : 0.f;
            self.visible = k > 0 && f > 0;
            if (!self.visible) return;
            if (!d.color.a) self.children[0]->material.color = scale(v);
            self.position = s->point(d.at);
            self.rotation = g3d::lookRotation(s->north(d.at), s->up(d.at));
            switch (st.mark) {
                case Mark::Column:
                case Mark::Spike: self.scale = { st.size, std::max(1e-4f, st.height * f * k), st.size }; break;
                case Mark::Bubble: {
                    const float dia = st.size * std::sqrt(f) * k;
                    self.scale = { dia, dia, dia };
                    break;
                }
                case Mark::Disc: {
                    const float dia = st.size * std::sqrt(f) * k;
                    self.scale = { dia, st.size * 0.12f, dia };
                    break;
                }
            }
        };
    }
    return group;
}

// ================================================================ areas

void choropleth(globe::Map& map, const Values& values, const Scale& scale, const geo::Layer& layer) {
    std::vector<double> vals;
    for (const auto& kv : values) vals.push_back(kv.second);
    const Scale sc = scale.over(vals);
    for (const auto& [key, v] : values)
        if (const geo::Feature* f = layer.find(key)) map.fill(f->geometry, sc(v));
}

g3d::Object3D* extrude(g3d::Object3D& parent, SurfacePtr s, const Values& values, const ExtrudeStyle& st,
                       const geo::Layer& layer) {
    g3d::Object3D* group = parent.add();
    group->name = "extrude";
    std::vector<double> vals;
    for (const auto& kv : values) vals.push_back(kv.second);
    const Scale scale = st.scale.over(vals);
    const std::vector<int> rank = ranks(vals);
    const double top = std::max(std::fabs(scale.max), std::fabs(scale.min));
    const double step = std::max(0.25, (double)st.detail);
    for (size_t i = 0; i < values.size(); i++) {
        const geo::Feature* f = layer.find(values[i].first);
        if (!f) continue;
        auto shape = std::make_shared<const Shape>(shapeOf(f->geometry, step, s->centre().lon));
        if (shape->top.empty()) continue;
        const bool walls = st.height > 0;
        const int count = (int)shape->top.size() * (walls ? 2 : 1) + (walls ? (int)shape->edges.size() * 6 : 0);
        // Vertices: tops, then bottoms (under the tops, facing down: seen from a flat map's
        // back or inside the globe), then the walls.
        auto fill = [s, shape, walls](Mesh& m, float lift, float height) {
            int k = 0;
            const size_t n = shape->top.size();
            for (size_t j = 0; j < n; j++, k++) {
                put(m.vertices, k, s->point(shape->top[j], lift + height));
                put(m.normals, k, s->up(shape->top[j]));
            }
            if (!walls) return;
            for (size_t j = 0; j < n; j += 3)
                for (int q : { 0, 2, 1 }) {
                    const LonLat p = shape->top[j + q];
                    put(m.vertices, k, s->point(p, lift));
                    put(m.normals, k++, Vector3Negate(s->up(p)));
                }
            for (const auto& e : shape->edges) {
                const Vector3 a0 = s->point(e[0], lift), b0 = s->point(e[1], lift);
                const Vector3 a1 = s->point(e[0], lift + height), b1 = s->point(e[1], lift + height);
                Vector3 nrm = Vector3CrossProduct(Vector3Subtract(b0, a0), Vector3Subtract(a1, a0));
                const float l = Vector3Length(nrm);
                nrm = l > 1e-12f ? Vector3Scale(nrm, 1 / l) : s->up(e[0]);
                for (Vector3 v : { a0, b0, b1, a0, b1, a1 }) {
                    put(m.vertices, k, v);
                    put(m.normals, k++, nrm);
                }
            }
        };
        const float lift = st.lift;
        auto geometry = std::make_shared<g3d::Geometry>([fill, count, lift] {
            Mesh m = newMesh(count, false);
            fill(m, lift, 1e-4f);
            UploadMesh(&m, true);
            return m;
        });
        g3d::Material m;
        m.color = scale(values[i].second);
        m.roughness = st.roughness;
        m.unlit = st.unlit;
        g3d::Object3D* o = group->add(geometry, m);
        o->name = values[i].first;
        const std::string key = values[i].first;
        const double v0 = values[i].second;
        const float delay = rank[i] * st.stagger;
        auto stamp = std::make_shared<Stamp>();
        o->up = [s, st, scale, top, delay, key, v0, geometry, fill, stamp, lift](int t, g3d::Object3D& self) {
            const double v = st.value ? st.value(t, key) : v0;
            const float k = grown(st.grow, t, delay);
            self.visible = k > 0;
            if (!self.visible) return;
            self.material.color = scale(v);
            const float h = std::max(1e-4f, top > 0 ? st.height * (float)(std::fabs(v) / top) * k : 0.f);
            if (stamp->same(*s, h)) return;
            stamp->set(*s, h);
            geometry->update([fill, lift, h](Mesh& mesh) { fill(mesh, lift, h); });
        };
    }
    return group;
}

// ================================================================ flows

g3d::Object3D* flows(g3d::Object3D& parent, SurfacePtr s, const std::vector<Flow>& data, const FlowStyle& st) {
    g3d::Object3D* group = parent.add();
    group->name = "flows";
    std::vector<double> vals;
    for (const Flow& f : data) vals.push_back(f.value);
    const Scale scale = st.scale.over(vals);
    const double top = std::max(std::fabs(scale.max), std::fabs(scale.min));
    for (size_t i = 0; i < data.size(); i++) {
        const Flow f = data[i];
        globe::RouteStyle rs;
        rs.width = st.width * (top > 0 ? 0.35f + 0.65f * (float)(std::fabs(f.value) / top) : 1.f);
        rs.color = f.color.a ? f.color : scale(f.value);
        rs.unlit = st.unlit;
        rs.segments = st.segments;
        const float delay = i * st.stagger;
        auto head = [h = st.head, delay](int t) { return h ? h((int)std::lround(t - delay)) : 1.f; };
        std::function<float(int)> tail;
        if (st.tail) tail = [tl = st.tail, delay](int t) { return tl((int)std::lround(t - delay)); };
        // A great circle, unless it would cross the map's edge: then the way across the map.
        const double c = s->centre().lon;
        bool crosses = false;
        for (int k = 1; k <= 64 && !crosses; k++)
            crosses = std::fabs(wrapNear(geo::interpolate(f.from, f.to, k / 64.0).lon, c) -
                                wrapNear(geo::interpolate(f.from, f.to, (k - 1) / 64.0).lon, c)) > 180;
        const LonLat a{ wrapNear(f.from.lon, c), f.from.lat }, b{ wrapNear(f.to.lon, c), f.to.lat };
        const double span = crosses ? std::min(180.0, std::hypot(b.lon - a.lon, b.lat - a.lat)) : geo::angle(f.from, f.to);
        const float rise = st.height * (float)std::sqrt(span / 180.0);
        g3d::Object3D* arc = globe::path(
            *group,
            [s, f, rise, crosses, a, b](float u) {
                const LonLat p = crosses ? LonLat{ a.lon + (b.lon - a.lon) * u, a.lat + (b.lat - a.lat) * u }
                                         : geo::interpolate(f.from, f.to, u);
                return s->point(p, rise * std::sin(PI * u));
            },
            head, rs, tail);
        arc->name = "flow";
    }
    return group;
}

// ================================================================ labels

bool screen(const g3d::Camera& camera, const Surface& s, LonLat p, float height, Rectangle viewport, Vector2& out) {
    const Vector3 at = s.point(p, height);
    const Vector3 toEye = Vector3Subtract(camera.position, s.point(p));
    if (!camera.orthographic && Vector3DotProduct(s.up(p), toEye) < 0) return false;
    if (camera.orthographic &&
        Vector3DotProduct(s.up(p), Vector3Subtract(camera.position, camera.target)) < 0) return false;
    return g3d::project(camera, at, viewport, out);
}

} // namespace geo3d
} // namespace rmtn
