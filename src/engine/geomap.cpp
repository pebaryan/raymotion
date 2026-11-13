// geomap.cpp -- drawing 2D maps: the base map, choropleths, highlights, markers, labels,
// routes and view changes, all through a geo::Projection.
#include "rmtn/geomap.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/fonts.hpp"
#include "rmtn/prefab.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <stdexcept>

namespace rmtn {
namespace geo {

namespace {

float unit(float v) { return v < 0 ? 0.f : (v > 1 ? 1.f : v); }
// 0..1 as u crosses [from, to].
float seg(float u, float from, float to) { return unit((u - from) / (to - from)); }
float easeOut(float v) { v = unit(v); return 1 - (1 - v) * (1 - v) * (1 - v); }
float easeBack(float v) {
    v = unit(v);
    const float c = 1.70158f, w = v - 1;
    return 1 + (c + 1) * w * w * w + c * w * w;
}
// Item `rank` of `n` in a reveal: with stagger 0 they all run the whole of u, with 1 each
// takes a quarter of it and they start evenly spread.
float staggered(float u, size_t rank, size_t n, float stagger) {
    const float len = 1 - 0.75f * unit(stagger);
    const float start = n > 1 ? (1 - len) * (float)rank / (float)(n - 1) : 0;
    return unit((u - start) / len);
}

const Geometry& landShape() {
    static const Geometry g = land().merged();
    return g;
}
const Geometry& countryShapes() {
    static const Geometry g = countries().merged();
    return g;
}

Font fontOf(Font f, bool bold) { return f.texture.id ? f : fonts::ui(bold); }

// Text with its top left at `at`, with a halo of the same letters behind it.
void haloText(const Ctx& c, Font font, const std::string& text, Vector2 at, float size, Color color, Color halo) {
    if (halo.a > 0) {
        const float r = std::max(1.5f, size * 0.09f);
        static const float ring[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { .7f, .7f }, { -.7f, .7f }, { .7f, -.7f }, { -.7f, -.7f } };
        for (const auto& o : ring) dr::text(font, c, text.c_str(), { at.x + o[0] * r, at.y + o[1] * r }, size, 0, halo);
    }
    dr::text(font, c, text.c_str(), at, size, 0, color);
}

// Keeps labels off each other: rectangles already taken, in node px.
struct Placer {
    std::vector<Rectangle> taken;
    float margin = 4;
    bool free(Rectangle r) const {
        for (const Rectangle& o : taken)
            if (r.x < o.x + o.width + margin && o.x < r.x + r.width + margin && r.y < o.y + o.height + margin &&
                o.y < r.y + r.height + margin)
                return false;
        return true;
    }
    // The first free place for a w x h box beside `p`: right, left, above, below when
    // `offset` > 0, else centred on p. False when none is free.
    bool place(Vector2 p, float w, float h, float offset, Rectangle& out) {
        Rectangle tries[4];
        int n = 1;
        if (offset > 0) {
            tries[0] = { p.x + offset, p.y - h / 2, w, h };
            tries[1] = { p.x - offset - w, p.y - h / 2, w, h };
            tries[2] = { p.x - w / 2, p.y - offset - h, w, h };
            tries[3] = { p.x - w / 2, p.y + offset, w, h };
            n = 4;
        } else {
            tries[0] = { p.x - w / 2, p.y - h / 2, w, h };
        }
        for (int i = 0; i < n; i++)
            if (free(tries[i])) {
                taken.push_back(tries[i]);
                out = tries[i];
                return true;
            }
        return false;
    }
};

std::vector<double> cumulative(const std::vector<LonLat>& stops) {
    std::vector<double> at(stops.size(), 0.0);
    for (size_t i = 1; i < stops.size(); i++) at[i] = at[i - 1] + distance(stops[i - 1], stops[i]);
    return at;
}

} // namespace

// ================================================================ data

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

Scale Scale::over(const Values& values) const {
    std::vector<double> v;
    v.reserve(values.size());
    for (const auto& kv : values) v.push_back(kv.second);
    return over(v);
}

float Scale::t(double v) const { return max > min ? unit((float)((v - min) / (max - min))) : 0.f; }

Color Scale::operator()(double v) const { return colors ? colors(t(v)) : plot::viridis(t(v)); }

Series Series::from(const plot::Table& table) {
    Series s;
    for (size_t j = 0; j < table.series.size(); j++) {
        const std::string& name = table.series[j].name;
        char* end = nullptr;
        const double v = std::strtod(name.c_str(), &end);
        const bool number = !name.empty() && end && *end == 0;
        s.times.push_back(number ? v : (double)j);
    }
    for (size_t i = 0; i < table.categories.size(); i++) {
        std::vector<double> row;
        for (const plot::Series& col : table.series) row.push_back(i < col.values.size() ? col.values[i] : 0.0);
        s.rows.push_back({ table.categories[i], std::move(row) });
    }
    return s;
}

Values Series::at(double time) const {
    Values out;
    if (times.empty()) return out;
    size_t hi = 0;
    while (hi < times.size() && times[hi] < time) hi++;
    size_t lo = hi > 0 ? hi - 1 : 0;
    if (hi >= times.size()) hi = lo = times.size() - 1;
    else if (hi == 0) lo = 0;
    const double span = times[hi] - times[lo], w = span > 0 ? std::clamp((time - times[lo]) / span, 0.0, 1.0) : 0;
    for (const auto& [key, row] : rows) {
        const double a = lo < row.size() ? row[lo] : 0, b = hi < row.size() ? row[hi] : a;
        out.push_back({ key, a + (b - a) * w });
    }
    return out;
}

Scale Series::scale(Scale s) const {
    std::vector<double> all;
    for (const auto& kv : rows) all.insert(all.end(), kv.second.begin(), kv.second.end());
    return s.over(all);
}

// ================================================================ the base map

void base(const Ctx& c, const Projection& map, const BaseStyle& s, float u) {
    u = unit(u);
    if (u <= 0) return;
    const float ocean = easeOut(seg(u, 0, 0.25f)), grid = seg(u, 0.05f, 0.55f), coast = seg(u, 0.1f, 0.75f);
    const float land = easeOut(seg(u, 0.5f, 0.9f)), border = easeOut(seg(u, 0.7f, 1));
    if (s.ocean.a > 0) vg::fill(c, map.outline(), withAlpha(s.ocean, ocean));
    if (s.graticuleStep > 0 && s.graticule.a > 0 && s.graticuleWidth > 0 && grid > 0) {
        const vg::Path g = map.graticule(s.graticuleStep);
        vg::stroke(c, grid < 1 ? g.trim(0, grid, true) : g, vg::Stroke(s.graticule, s.graticuleWidth));
    }
    if (s.land.a > 0 && land > 0) vg::fill(c, map.path(landShape()), withAlpha(s.land, land), vg::FillRule::EvenOdd);
    if (s.border.a > 0 && s.borderWidth > 0 && border > 0)
        vg::stroke(c, map.lines(countryShapes()), vg::Stroke(withAlpha(s.border, border), s.borderWidth));
    if (s.coast.a > 0 && s.coastWidth > 0 && coast > 0) {
        const vg::Path l = map.lines(landShape());
        vg::stroke(c, coast < 1 ? l.trim(0, coast, true) : l, vg::Stroke(s.coast, s.coastWidth));
    }
}

// ================================================================ areas

void choropleth(const Ctx& c, const Projection& map, const Values& values, const ChoroplethStyle& s, float u, const Layer& layer) {
    u = unit(u);
    if (u <= 0) return;
    const Scale scale = s.scale.over(values);
    struct Item {
        const Feature* f;
        double v;
    };
    std::vector<Item> items;
    for (const auto& [key, v] : values)
        if (const Feature* f = layer.find(key))
            if (std::isfinite(v)) items.push_back({ f, v });
    if (s.missing.a > 0) {
        Geometry rest;
        for (const Feature& f : layer.features) {
            bool has = false;
            for (const Item& it : items) has = has || it.f == &f;
            if (!has) rest.append(f.geometry);
        }
        vg::fill(c, map.path(rest), withAlpha(s.missing, easeOut(u)), vg::FillRule::EvenOdd);
        if (s.border.a > 0 && s.borderWidth > 0) vg::stroke(c, map.lines(rest), vg::Stroke(withAlpha(s.border, easeOut(u)), s.borderWidth));
    }
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.v > b.v; });
    for (size_t i = 0; i < items.size(); i++) {
        const float k = easeOut(staggered(u, i, items.size(), s.stagger));
        if (k <= 0) continue;
        vg::fill(c, map.path(items[i].f->geometry), withAlpha(scale(items[i].v), k), vg::FillRule::EvenOdd);
        if (s.border.a > 0 && s.borderWidth > 0)
            vg::stroke(c, map.lines(items[i].f->geometry), vg::Stroke(withAlpha(s.border, k), s.borderWidth));
    }
}

void colorbar(const Ctx& c, Rectangle box, const Scale& scale, plot::ColorbarStyle s, float u) {
    s.colors = scale.colors ? scale.colors : std::function<Color(float)>(plot::viridis);
    plot::colorbar(c, box, plot::Range(scale.min, scale.max), s, u);
}

void highlight(const Ctx& c, const Projection& map, const Geometry& region, const HighlightStyle& s, float u) {
    u = unit(u);
    if (u <= 0) return;
    const vg::Path shape = map.path(region);
    if (s.dim.a > 0) {
        vg::Path veil = map.outline();
        veil.append(shape);
        vg::fill(c, veil, withAlpha(s.dim, easeOut(seg(u, 0, 0.5f))), vg::FillRule::EvenOdd);
    }
    if (s.fill.a > 0) vg::fill(c, shape, withAlpha(s.fill, easeOut(seg(u, 0.3f, 1))), vg::FillRule::EvenOdd);
    if (s.stroke.a > 0 && s.width > 0) {
        const float k = seg(u, 0, 0.7f);
        const vg::Path edge = map.lines(region);
        vg::stroke(c, k < 1 ? edge.trim(0, k, true) : edge, vg::Stroke(s.stroke, s.width));
    }
}

// ================================================================ points

void markers(const Ctx& c, const Projection& map, const std::vector<Marker>& items, const MarkerStyle& s, float u) {
    u = unit(u);
    if (u <= 0 || items.empty()) return;
    double top = 0;
    for (const Marker& m : items) top = std::max(top, std::fabs(m.value));
    struct Dot {
        Vector2 at;
        float r, k;
        const Marker* m;
    };
    std::vector<Dot> dots;
    Placer placer;
    placer.taken = s.avoid;
    for (size_t i = 0; i < items.size(); i++) {
        Vector2 at;
        if (!map.project(items[i].at, at)) continue;
        const float r = s.sized && top > 0 ? s.radius * (float)std::sqrt(std::fabs(items[i].value) / top) : s.radius;
        dots.push_back({ at, r, staggered(u, i, items.size(), s.stagger), &items[i] });
        placer.taken.push_back({ at.x - r, at.y - r, 2 * r, 2 * r });
    }
    for (const Dot& d : dots) {
        if (d.k <= 0) continue;
        const float r = d.r * easeBack(d.k);
        const Color col = d.m->color.a > 0 ? d.m->color : s.color;
        if (s.ring.a > 0 && s.ringWidth > 0) vg::fill(c, vg::Path::circle(d.at, r + s.ringWidth), withAlpha(s.ring, unit(d.k * 2)));
        vg::fill(c, vg::Path::circle(d.at, r), withAlpha(col, unit(d.k * 2)));
    }
    if (!s.labels) return;
    const Font font = fontOf(s.font, s.bold);
    for (const Dot& d : dots) {
        if (d.m->label.empty()) continue;
        const Vector2 m = dr::measure(font, d.m->label.c_str(), s.textSize, 0);
        Rectangle r;
        if (!placer.place(d.at, m.x, m.y, d.r + s.ringWidth + 6, r)) continue;
        const float k = easeOut(seg(d.k, 0.4f, 1));
        if (k <= 0) continue;
        Ctx ci = c;
        ci.a *= k;
        haloText(ci, font, d.m->label, { r.x, r.y }, s.textSize, s.textColor, s.halo);
    }
}

std::vector<Marker> places(const Layer& layer, size_t limit, const std::string& valueProperty) {
    std::vector<Marker> out;
    for (const Feature& f : layer.features) {
        if (limit && out.size() >= limit) break;
        Marker m;
        m.at = f.geometry.points.empty() ? f.labelPoint() : f.geometry.points[0];
        m.label = f.name();
        if (f.properties.isObject() && f.properties[valueProperty].isNumber()) m.value = f.properties[valueProperty].asNumber();
        out.push_back(std::move(m));
    }
    return out;
}

void labels(const Ctx& c, const Projection& map, const std::vector<Label>& items, const LabelStyle& s, float u) {
    u = unit(u);
    if (u <= 0 || items.empty()) return;
    const Font font = fontOf(s.font, s.bold);
    Placer placer;
    placer.margin = s.margin;
    placer.taken = s.avoid;
    for (size_t i = 0; i < items.size(); i++) {
        Vector2 at;
        if (items[i].text.empty() || !map.project(items[i].at, at)) continue;
        const Vector2 m = dr::measure(font, items[i].text.c_str(), s.size, 0);
        Rectangle r;
        if (!placer.place(at, m.x, m.y, s.offset, r)) continue;
        const float k = easeOut(staggered(u, i, items.size(), s.stagger));
        if (k <= 0) continue;
        Ctx ci = c;
        ci.a *= k;
        haloText(ci, font, items[i].text, { r.x, r.y }, s.size, s.color, s.halo);
    }
}

std::vector<Label> names(const Layer& layer, size_t limit) {
    std::vector<Label> out;
    for (const Feature& f : layer.features) {
        if (limit && out.size() >= limit) break;
        out.push_back({ f.geometry.points.empty() ? f.labelPoint() : f.geometry.points[0], f.name() });
    }
    return out;
}

// ================================================================ routes

double length(const std::vector<LonLat>& stops) {
    double sum = 0;
    for (size_t i = 1; i < stops.size(); i++) sum += distance(stops[i - 1], stops[i]);
    return sum;
}

LonLat along(const std::vector<LonLat>& stops, double u, double* heading) {
    if (heading) *heading = 0;
    if (stops.empty()) return {};
    if (stops.size() == 1) return stops[0];
    const std::vector<double> at = cumulative(stops);
    const double total = at.back(), d = std::clamp(u, 0.0, 1.0) * total;
    size_t i = 1;
    while (i + 1 < stops.size() && at[i] < d) i++;
    const double leg = at[i] - at[i - 1], w = leg > 0 ? std::clamp((d - at[i - 1]) / leg, 0.0, 1.0) : 1;
    const LonLat p = interpolate(stops[i - 1], stops[i], w);
    // The heading where it stands; at the very end of a leg, the one it arrived on.
    if (heading) *heading = w < 1 - 1e-6 ? bearing(p, stops[i]) : bearing(interpolate(stops[i - 1], stops[i], 1 - 1e-4), stops[i]);
    return p;
}

vg::Path route(const Projection& map, const std::vector<LonLat>& stops, double from, double to) {
    from = std::clamp(from, 0.0, 1.0);
    to = std::clamp(to, 0.0, 1.0);
    if (stops.size() < 2 || to <= from) return {};
    const std::vector<double> at = cumulative(stops);
    const double total = at.back();
    if (total <= 0) return {};
    const double d0 = from * total, d1 = to * total, step = EARTH_RADIUS_KM * 3.14159265358979323846 / 180;   // a degree of arc
    std::vector<LonLat> pts;
    for (size_t i = 1; i < stops.size(); i++) {
        const double a = std::max(d0, at[i - 1]), b = std::min(d1, at[i]), leg = at[i] - at[i - 1];
        if (b <= a || leg <= 0) continue;
        const int n = std::max(1, (int)std::ceil((b - a) / step));
        for (int j = pts.empty() ? 0 : 1; j <= n; j++)
            pts.push_back(interpolate(stops[i - 1], stops[i], (a + (b - a) * j / n - at[i - 1]) / leg));
    }
    if (pts.size() < 2) return {};
    return map.lines(Geometry::line(std::move(pts)));
}

void route(const Ctx& c, const Projection& map, const std::vector<LonLat>& stops, const RouteStyle& s, float u) {
    u = unit(u);
    if (u <= 0 || stops.size() < 2) return;
    const std::vector<double> at = cumulative(stops);
    const double total = at.back();
    if (total <= 0) return;
    if (s.ghost.a > 0) {
        vg::Stroke g(withAlpha(s.ghost, unit(u * 4)), s.width);
        g.dash = s.dash;
        vg::stroke(c, route(map, stops), g);
    }
    vg::Stroke line(s.color, s.width);
    line.dash = s.dash;
    vg::stroke(c, route(map, stops, s.tail > 0 ? std::max(0.f, u - s.tail) : 0, u), line);
    if (s.stops > 0)
        for (size_t i = 0; i < stops.size(); i++) {
            const float k = easeBack(seg(u, (float)(at[i] / total) - (i ? 0.03f : 0), (float)(at[i] / total) + (i ? 0.01f : 0.04f)));
            Vector2 p;
            if (k > 0 && map.project(stops[i], p)) vg::fill(c, vg::Path::circle(p, s.stops * k), s.color);
        }
    if (s.head <= 0) return;
    Vector2 p;
    if (!map.project(along(stops, u), p)) return;
    if (!s.arrow) {
        vg::fill(c, vg::Path::circle(p, s.head), s.headColor);
        return;
    }
    // The direction on screen, from a point just behind (just ahead, at the very start).
    const double eps = 0.004, other = u > eps ? u - eps : u + eps;
    const Vector2 q = map(along(stops, other));
    float dx = (p.x - q.x) * (u > eps ? 1.f : -1.f), dy = (p.y - q.y) * (u > eps ? 1.f : -1.f);
    const float len = std::hypot(dx, dy);
    if (len < 1e-4f) { dx = 1; dy = 0; } else { dx /= len; dy /= len; }
    const float h = s.head;
    vg::Path tip;
    tip.moveTo({ p.x + dx * h * 1.6f, p.y + dy * h * 1.6f })
        .lineTo({ p.x - dx * h - dy * h, p.y - dy * h + dx * h })
        .lineTo({ p.x - dx * h * 0.4f, p.y - dy * h * 0.4f })
        .lineTo({ p.x - dx * h + dy * h, p.y - dy * h - dx * h })
        .close();
    vg::fill(c, tip, s.headColor);
}

// ================================================================ views

Projection focus(const Projection& base, Rectangle box, const Geometry& g, float padding) {
    Projection p = base;
    if (!g.empty()) {
        const LonLat mid = centroid(g);
        switch (p.kind()) {
        case Kind::Orthographic: case Kind::Stereographic: case Kind::AzimuthalEquidistant: case Kind::AzimuthalEqualArea:
        case Kind::Gnomonic:
            p.rotate(mid, p.roll());
            break;
        case Kind::ConicEqualArea: case Kind::ConicConformal: case Kind::ConicEquidistant:
            p.rotate({ mid.lon, p.rotation().lat }, p.roll());
            break;
        default: break;
        }
    }
    p.fit(box, g, padding);
    return p;
}

// ================================================================ prefabs

namespace {

Rectangle boxOf(const Json& p) {
    const Json& b = p["box"];
    if (!b.isArray() || b.size() != 4) throw std::runtime_error("geo prefab: box must be [x, y, width, height]");
    const Rectangle r{ (float)b[0].asNumber(), (float)b[1].asNumber(), (float)b[2].asNumber(), (float)b[3].asNumber() };
    if (!(r.width > 0) || !(r.height > 0)) throw std::runtime_error("geo prefab: box needs a positive width and height");
    return r;
}

// "world", or a country by ISO code or name.
Geometry placeOf(const Json& v, const char* key) {
    if (!v.isString()) throw std::runtime_error(std::string("geo prefab: ") + key + " must be \"world\" or a country code or name");
    if (v.asString() == "world") return Sphere;
    return countries().get(v.asString()).geometry;
}

BaseStyle baseOf(const Json& p) {
    BaseStyle s;
    s.ocean = p.color("ocean", s.ocean);
    s.land = p.color("land", s.land);
    s.coast = p.color("coast", s.coast);
    s.border = p.color("border", s.border);
    s.graticule = p.color("graticule", s.graticule);
    return s;
}

float easeInOut(float v) {
    v = unit(v);
    return v * v * (3 - 2 * v);
}

} // namespace

void registerPrefabs() {
    prefab::add({ "geo-route",
        Json::parse(R"({"stops":[[106.85,-6.21],[16.37,48.21]],"labels":[],"projection":"equalEarth","fit":"route",
                        "box":[0,0,1920,1080],"padding":160,"delay":0,"duration":60,"color":"#ffc45c","width":4,
                        "arrow":false,"textSize":28,"dash":[]})"),
        [](TNode& n, const Json& p) {
            std::vector<LonLat> stops;
            for (const Json& s : p["stops"].asArray()) {
                if (!s.isArray() || s.size() != 2) throw std::runtime_error("geo-route: stops must be [lon, lat] pairs");
                stops.push_back({ s[0].asNumber(), s[1].asNumber() });
            }
            if (stops.size() < 2) throw std::runtime_error("geo-route: needs at least two stops");
            std::vector<Marker> dots;
            const Json& names = p["labels"];
            for (size_t i = 0; i < stops.size(); i++) {
                Marker m;
                m.at = stops[i];
                if (names.isArray() && i < names.size()) m.label = names[i].asString();
                dots.push_back(std::move(m));
            }
            const Rectangle box = boxOf(p);
            const std::string fit = p.str("fit", "route");
            if (fit != "route" && fit != "world") throw std::runtime_error("geo-route: fit must be \"route\" or \"world\"");
            const float padding = (float)p.num("padding", 160);
            Projection map = Projection::named(p.str("projection", "equalEarth"));
            // The route as it will be drawn (along great circles), so the frame holds its bulge.
            Geometry line = Geometry::line(stops);
            {
                std::vector<LonLat> dense;
                for (int i = 0; i <= 64; i++) dense.push_back(along(stops, i / 64.0));
                line = Geometry::line(std::move(dense));
            }
            map = fit == "world" ? focus(map, box, Sphere, 0) : focus(map, box, line, std::min(padding, std::min(box.width, box.height) * 0.45f));
            const BaseStyle base = baseOf(p);
            RouteStyle rs;
            rs.color = p.color("color", rs.color);
            rs.width = (float)p.num("width", rs.width);
            rs.arrow = p.flag("arrow", false);
            rs.head = rs.width * 1.8f;
            rs.stops = 0;
            if (p["dash"].isArray())
                for (const Json& d : p["dash"].asArray()) rs.dash.push_back((float)d.asNumber());
            MarkerStyle ms;
            ms.color = rs.color;
            ms.radius = rs.width * 1.6f;
            ms.textSize = (float)p.num("textSize", 28);
            ms.bold = true;
            ms.stagger = 1;
            // Labels keep off the line: a string of small boxes along it.
            {
                const vg::Path whole = geo::route(map, stops);
                const float r = rs.width + 4;
                if (!whole.empty())
                    for (int i = 2; i <= 46; i++) {
                        const Vector2 q = whole.pointAt(i / 48.f);
                        ms.avoid.push_back({ q.x - r, q.y - r, 2 * r, 2 * r });
                    }
            }
            const double delay = p.num("delay", 0), duration = std::max(1.0, p.num("duration", 60));
            n.fn = [=](const Ctx& c, int frame) {
                const float u = easeInOut((float)((frameTime(frame) - delay) / duration));
                geo::base(c, map, base);
                geo::route(c, map, stops, rs, u);
                // Each dot as the line reaches it: the first at once, the last on arrival.
                geo::markers(c, map, dots, ms, unit(0.25f + u));
            };
        } });

    prefab::add({ "geo-flyto",
        Json::parse(R"({"from":"world","to":"AUT","projection":"equalEarth","box":[0,0,1920,1080],"padding":260,
                        "delay":0,"duration":60,"zoomOut":0,"highlight":"#e25d4e","label":true,"textSize":40})"),
        [](TNode& n, const Json& p) {
            const Rectangle box = boxOf(p);
            const float padding = std::min((float)p.num("padding", 260), std::min(box.width, box.height) * 0.45f);
            const Projection kind = Projection::named(p.str("projection", "equalEarth"));
            const Geometry start = placeOf(p["from"], "from"), target = placeOf(p["to"], "to");
            const Projection a = focus(kind, box, start, start.empty() ? 0 : padding), b = focus(kind, box, target, target.empty() ? 0 : padding);
            const BaseStyle base = baseOf(p);
            HighlightStyle hs;
            hs.fill = p.color("highlight", hs.fill);
            hs.stroke = mixC(hs.fill, WHITE, 0.7);
            std::vector<Label> name;
            if (p.flag("label", true) && p["to"].asString() != "world") {
                const Feature& f = countries().get(p["to"].asString());
                name.push_back({ f.labelPoint(), f.name() });
            }
            LabelStyle ls;
            ls.size = (float)p.num("textSize", 40);
            ls.bold = true;
            const double delay = p.num("delay", 0), duration = std::max(1.0, p.num("duration", 60)), zoomOut = p.num("zoomOut", 0);
            n.fn = [=](const Ctx& c, int frame) {
                const double t = (frameTime(frame) - delay) / duration;
                const Projection view = Projection::fly(a, b, easeInOut((float)t), zoomOut);
                geo::base(c, view, base);
                if (!target.empty()) geo::highlight(c, view, target, hs, unit((float)(t - 0.6) / 0.5f));
                geo::labels(c, view, name, ls, unit((float)(t - 0.85) / 0.3f));
            };
        } });
}

} // namespace geo
} // namespace rmtn
