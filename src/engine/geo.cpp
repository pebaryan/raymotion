// geo.cpp -- map data (GeoJSON, TopoJSON, the bundled Natural Earth layers), geometry on
// the sphere, and map projections with clipping.
#include "rmtn/geo.hpp"
#include "rmtn/media.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace rmtn {
namespace geo {

namespace {

// raylib's PI is a float literal; the maths here needs doubles.
constexpr double kPi = 3.14159265358979323846;
constexpr double RAD = kPi / 180.0, DEG = 180.0 / kPi, TAU = 2 * kPi;

struct V3 {
    double x = 0, y = 0, z = 0;
};
V3 operator+(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
V3 operator-(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
V3 operator*(V3 a, double k) { return { a.x * k, a.y * k, a.z * k }; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
double norm(V3 a) { return std::sqrt(dot(a, a)); }
V3 unit(V3 a) {
    const double n = norm(a);
    return n > 0 ? a * (1 / n) : V3{ 1, 0, 0 };
}
V3 toV3(double lambda, double phi) { return { std::cos(phi) * std::cos(lambda), std::cos(phi) * std::sin(lambda), std::sin(phi) }; }
V3 toV3(LonLat p) { return toV3(p.lon * RAD, p.lat * RAD); }
LonLat toLonLat(V3 v) { return { std::atan2(v.y, v.x) * DEG, std::asin(std::clamp(v.z, -1.0, 1.0)) * DEG }; }

V3 slerp(V3 a, V3 b, double t) {
    const double w = std::acos(std::clamp(dot(a, b), -1.0, 1.0));
    if (w < 1e-12) return a;
    const double s = std::sin(w);
    return a * (std::sin((1 - t) * w) / s) + b * (std::sin(t * w) / s);
}

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// ---------------------------------------------------------------- rings

// Longitudes made continuous (no jumps over 180 degrees); returns the net change.
double unwrapped(const Ring& r, std::vector<double>& lon) {
    lon.resize(r.size());
    if (r.empty()) return 0;
    lon[0] = r[0].lon;
    for (size_t i = 1; i < r.size(); i++) {
        double d = r[i].lon - r[i - 1].lon;
        while (d > 180) d -= 360;
        while (d < -180) d += 360;
        lon[i] = lon[i - 1] + d;
    }
    double d = r[0].lon - r.back().lon;
    while (d > 180) d -= 360;
    while (d < -180) d += 360;
    return lon.back() + d - lon[0];
}

// True when the ring's inside lies on its left (anticlockwise in the lon/lat plane, or,
// for a ring around a pole, travelling with the pole on its left).
bool leftHanded(const Ring& r) {
    std::vector<double> lon;
    const double net = unwrapped(r, lon);
    if (std::fabs(net) > 180) {
        double meanLat = 0;
        for (const LonLat& p : r) meanLat += p.lat;
        const bool north = meanLat > 0;
        return (net > 0) == north;   // eastward has the north on its left
    }
    double a = 0;
    for (size_t i = 0, n = r.size(); i < n; i++) {
        const size_t j = (i + 1) % n;
        const double lj = lon[j] + (j == 0 ? net : 0);
        a += lon[i] * r[j].lat - lj * r[i].lat;
    }
    return a > 0;
}

void orient(std::vector<Ring>& polygon) {
    for (size_t k = 0; k < polygon.size(); k++) {
        Ring& r = polygon[k];
        if (r.size() < 3) continue;
        if (leftHanded(r) != (k == 0)) std::reverse(r.begin(), r.end());
    }
}

// ---------------------------------------------------------------- GeoJSON

LonLat position(const Json& p) {
    if (!p.isArray() || p.size() < 2) throw std::runtime_error("geo: a position needs [lon, lat]");
    return { p[0].asNumber(), p[1].asNumber() };
}

Ring ring(const Json& coords) {
    Ring r;
    for (const Json& p : coords.asArray()) r.push_back(position(p));
    if (r.size() > 1 && r.front().lon == r.back().lon && r.front().lat == r.back().lat) r.pop_back();
    return r;
}

std::vector<Ring> polygon(const Json& coords) {
    std::vector<Ring> out;
    for (const Json& r : coords.asArray()) {
        Ring k = ring(r);
        if (k.size() >= 3) out.push_back(std::move(k));
    }
    orient(out);
    return out;
}

void geometry(const Json& g, Geometry& out) {
    if (g.isNull()) return;
    const std::string type = g["type"].isString() ? g["type"].asString() : "";
    const Json& c = g["coordinates"];
    if (type == "Point") out.points.push_back(position(c));
    else if (type == "MultiPoint") for (const Json& p : c.asArray()) out.points.push_back(position(p));
    else if (type == "LineString") {
        std::vector<LonLat> l;
        for (const Json& p : c.asArray()) l.push_back(position(p));
        out.lines.push_back(std::move(l));
    } else if (type == "MultiLineString") {
        for (const Json& line : c.asArray()) {
            std::vector<LonLat> l;
            for (const Json& p : line.asArray()) l.push_back(position(p));
            out.lines.push_back(std::move(l));
        }
    } else if (type == "Polygon") {
        auto p = polygon(c);
        if (!p.empty()) out.polygons.push_back(std::move(p));
    } else if (type == "MultiPolygon") {
        for (const Json& poly : c.asArray()) {
            auto p = polygon(poly);
            if (!p.empty()) out.polygons.push_back(std::move(p));
        }
    } else if (type == "GeometryCollection") {
        for (const Json& sub : g["geometries"].asArray()) geometry(sub, out);
    } else {
        throw std::runtime_error("geo: unknown geometry type '" + type + "'");
    }
}

std::string idOf(const Json& id) {
    if (id.isString()) return id.asString();
    if (id.isNumber()) {
        std::ostringstream s;
        s << id.asNumber();
        return s.str();
    }
    return "";
}

void geoJson(const Json& j, Layer& out) {
    const std::string type = j["type"].isString() ? j["type"].asString() : "";
    if (type == "FeatureCollection") {
        for (const Json& f : j["features"].asArray()) geoJson(f, out);
    } else if (type == "Feature") {
        Feature f;
        f.id = idOf(j["id"]);
        f.properties = j["properties"].isObject() ? j["properties"] : Json(Json::Object{});
        geometry(j["geometry"], f.geometry);
        out.features.push_back(std::move(f));
    } else {
        Feature f;
        f.properties = Json(Json::Object{});
        geometry(j, f.geometry);
        out.features.push_back(std::move(f));
    }
}

// ---------------------------------------------------------------- TopoJSON

struct Topology {
    std::vector<std::vector<LonLat>> arcs;
    double sx = 1, sy = 1, tx = 0, ty = 0;
    bool quantized = false;

    LonLat point(const Json& p) const {
        const LonLat q = position(p);
        return quantized ? LonLat{ q.lon * sx + tx, q.lat * sy + ty } : q;
    }
    // Joins arcs (a negative index ~i is arc i reversed), dropping each shared endpoint.
    std::vector<LonLat> stitch(const Json& refs) const {
        std::vector<LonLat> out;
        for (const Json& r : refs.asArray()) {
            const long i = (long)r.asNumber();
            const size_t k = (size_t)(i < 0 ? ~i : i);
            if (k >= arcs.size()) throw std::runtime_error("geo: TopoJSON arc index out of range");
            std::vector<LonLat> a = arcs[k];
            if (i < 0) std::reverse(a.begin(), a.end());
            for (size_t n = out.empty() ? 0 : 1; n < a.size(); n++) out.push_back(a[n]);
        }
        return out;
    }
    std::vector<Ring> polygon(const Json& rings) const {
        std::vector<Ring> out;
        for (const Json& r : rings.asArray()) {
            Ring k = stitch(r);
            if (k.size() > 1 && k.front().lon == k.back().lon && k.front().lat == k.back().lat) k.pop_back();
            if (k.size() >= 3) out.push_back(std::move(k));
        }
        orient(out);
        return out;
    }
    void geometry(const Json& g, Geometry& out) const {
        const std::string type = g["type"].isString() ? g["type"].asString() : "";
        if (type == "Point") out.points.push_back(point(g["coordinates"]));
        else if (type == "MultiPoint") for (const Json& p : g["coordinates"].asArray()) out.points.push_back(point(p));
        else if (type == "LineString") out.lines.push_back(stitch(g["arcs"]));
        else if (type == "MultiLineString") for (const Json& l : g["arcs"].asArray()) out.lines.push_back(stitch(l));
        else if (type == "Polygon") {
            auto p = polygon(g["arcs"]);
            if (!p.empty()) out.polygons.push_back(std::move(p));
        } else if (type == "MultiPolygon") {
            for (const Json& poly : g["arcs"].asArray()) {
                auto p = polygon(poly);
                if (!p.empty()) out.polygons.push_back(std::move(p));
            }
        } else if (!type.empty() && type != "GeometryCollection") {
            throw std::runtime_error("geo: unknown TopoJSON geometry type '" + type + "'");
        }
    }
    void features(const Json& g, Layer& out) const {
        if (g["type"].isString() && g["type"].asString() == "GeometryCollection") {
            for (const Json& sub : g["geometries"].asArray()) features(sub, out);
            return;
        }
        Feature f;
        f.id = idOf(g["id"]);
        f.properties = g["properties"].isObject() ? g["properties"] : Json(Json::Object{});
        geometry(g, f.geometry);
        out.features.push_back(std::move(f));
    }
};

void topoJson(const Json& j, const std::string& object, Layer& out) {
    Topology t;
    const Json& tr = j["transform"];
    if (tr.isObject()) {
        t.quantized = true;
        t.sx = tr["scale"][0].asNumber();
        t.sy = tr["scale"][1].asNumber();
        t.tx = tr["translate"][0].asNumber();
        t.ty = tr["translate"][1].asNumber();
    }
    for (const Json& arc : j["arcs"].asArray()) {
        std::vector<LonLat> a;
        double x = 0, y = 0;
        for (const Json& p : arc.asArray()) {
            if (t.quantized) {
                x += p[0].asNumber();
                y += p[1].asNumber();
                a.push_back({ x * t.sx + t.tx, y * t.sy + t.ty });
            } else {
                a.push_back(position(p));
            }
        }
        t.arcs.push_back(std::move(a));
    }
    bool found = object.empty();
    for (const auto& [name, g] : j["objects"].asObject()) {
        if (!object.empty() && name != object) continue;
        found = true;
        t.features(g, out);
    }
    if (!found) throw std::runtime_error("geo: no TopoJSON object named '" + object + "'");
}

const Layer& bundled(const char* file) {
    static std::mutex m;
    static std::vector<std::pair<std::string, std::unique_ptr<Layer>>> cache;
    std::lock_guard<std::mutex> lock(m);
    for (auto& [name, layer] : cache)
        if (name == file) return *layer;
    cache.emplace_back(file, std::make_unique<Layer>(load(std::string("data/geo/") + file)));
    return *cache.back().second;
}

// ---------------------------------------------------------------- sphere measures

// Signed area (steradians) of a ring, positive when its inside is on its left.
double ringArea(const Ring& r) {
    std::vector<double> lon;
    const double net = unwrapped(r, lon);
    double s = 0;
    const size_t n = r.size();
    for (size_t i = 0; i < n; i++) {
        const size_t j = (i + 1) % n;
        const double l1 = lon[i] * RAD, l2 = (lon[j] + (j == 0 ? net : 0)) * RAD;
        s += (l2 - l1) * (2 + std::sin(r[i].lat * RAD) + std::sin(r[j].lat * RAD));
    }
    // The formula measures the area between the ring and the south pole; a ring around the
    // north pole travelling east encloses the rest of the sphere's northern cap.
    double a = -s / 2;
    if (std::fabs(net) > 180) a = net > 0 ? a + 4 * kPi : a;
    return a;
}

V3 ringCentroid(const Ring& r, double& weight) {
    V3 sum{};
    weight = 0;
    if (r.size() < 3) return sum;
    const V3 a = toV3(r[0]);
    for (size_t i = 1; i + 1 < r.size(); i++) {
        const V3 b = toV3(r[i]), c = toV3(r[i + 1]);
        const double w = dot(cross(b - a, c - a), unit(a + b + c)) / 2;   // signed triangle area
        sum = sum + (a + b + c) * (w / 3);
        weight += w;
    }
    return sum;
}

bool inRing(const Ring& r, LonLat p) {
    bool in = false;
    for (size_t i = 0, j = r.size() - 1; i < r.size(); j = i++) {
        const LonLat a = r[i], b = r[j];
        if ((a.lat > p.lat) != (b.lat > p.lat) && p.lon < (b.lon - a.lon) * (p.lat - a.lat) / (b.lat - a.lat) + a.lon) in = !in;
    }
    return in;
}

} // namespace

// ================================================================ data

const Geometry Sphere{};

Geometry& Geometry::append(const Geometry& o) {
    polygons.insert(polygons.end(), o.polygons.begin(), o.polygons.end());
    lines.insert(lines.end(), o.lines.begin(), o.lines.end());
    points.insert(points.end(), o.points.begin(), o.points.end());
    return *this;
}

Geometry Geometry::point(LonLat p) {
    Geometry g;
    g.points.push_back(p);
    return g;
}

Geometry Geometry::line(std::vector<LonLat> pts) {
    Geometry g;
    g.lines.push_back(std::move(pts));
    return g;
}

std::string Feature::name() const {
    for (const char* k : { "name", "NAME", "admin", "ADMIN" })
        if (properties[k].isString()) return properties[k].asString();
    return id;
}

LonLat Feature::labelPoint() const {
    const Json& l = properties["label"];
    if (l.isArray() && l.size() >= 2 && l[0].isNumber()) return { l[0].asNumber(), l[1].asNumber() };
    if (!geometry.polygons.empty()) {
        size_t best = 0;
        double most = -1;
        for (size_t i = 0; i < geometry.polygons.size(); i++) {
            const double a = std::fabs(ringArea(geometry.polygons[i][0]));
            if (a > most) { most = a; best = i; }
        }
        Geometry g;
        g.polygons.push_back(geometry.polygons[best]);
        return centroid(g);
    }
    return centroid(geometry);
}

const Feature* Layer::find(const std::string& key) const {
    const std::string k = lower(key);
    for (const Feature& f : features) {
        if (lower(f.id) == k) return &f;
        for (const char* p : { "iso_a3", "iso_a2", "ISO_A3", "ISO_A2" })
            if (f.properties[p].isString() && lower(f.properties[p].asString()) == k) return &f;
    }
    for (const Feature& f : features) {
        if (lower(f.name()) == k) return &f;
        if (f.properties["name_long"].isString() && lower(f.properties["name_long"].asString()) == k) return &f;
    }
    return nullptr;
}

const Feature& Layer::get(const std::string& key) const {
    if (const Feature* f = find(key)) return *f;
    throw std::runtime_error("geo: no feature '" + key + "'");
}

Layer Layer::filter(const std::function<bool(const Feature&)>& keep) const {
    Layer out;
    for (const Feature& f : features)
        if (keep(f)) out.features.push_back(f);
    return out;
}

Geometry Layer::merged() const {
    Geometry g;
    for (const Feature& f : features) g.append(f.geometry);
    return g;
}

Layer parse(const std::string& text, const std::string& object) {
    const Json j = Json::parse(text);
    Layer out;
    if (j["type"].isString() && j["type"].asString() == "Topology") topoJson(j, object, out);
    else geoJson(j, out);
    return out;
}

Layer load(const std::string& file, const std::string& object) {
    const std::string path = media::resolve(file);
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    if (!in) throw std::runtime_error("geo: cannot read " + path);
    std::stringstream s;
    s << in.rdbuf();
    try {
        return parse(s.str(), object);
    } catch (const std::runtime_error& e) {
        throw std::runtime_error(path + ": " + e.what());
    }
}

const Layer& countries() { return bundled("countries-110m.geojson"); }
const Layer& land() { return bundled("land-110m.geojson"); }
const Layer& lakes() { return bundled("lakes-110m.geojson"); }
const Layer& rivers() { return bundled("rivers-110m.geojson"); }
const Layer& cities() { return bundled("cities-110m.geojson"); }

// ================================================================ the sphere

double angle(LonLat a, LonLat b) {
    const double p1 = a.lat * RAD, p2 = b.lat * RAD, dp = p2 - p1, dl = (b.lon - a.lon) * RAD;
    const double h = std::sin(dp / 2) * std::sin(dp / 2) + std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
    return 2 * std::asin(std::min(1.0, std::sqrt(h))) * DEG;
}

double distance(LonLat a, LonLat b) { return angle(a, b) * RAD * EARTH_RADIUS_KM; }

LonLat interpolate(LonLat a, LonLat b, double t) { return toLonLat(slerp(toV3(a), toV3(b), t)); }

double bearing(LonLat from, LonLat to) {
    const double p1 = from.lat * RAD, p2 = to.lat * RAD, dl = (to.lon - from.lon) * RAD;
    const double y = std::sin(dl) * std::cos(p2), x = std::cos(p1) * std::sin(p2) - std::sin(p1) * std::cos(p2) * std::cos(dl);
    return std::fmod(std::atan2(y, x) * DEG + 360, 360);
}

LonLat centroid(const Geometry& g) {
    V3 sum{};
    double total = 0;
    for (const auto& poly : g.polygons)
        for (const Ring& r : poly) {
            double w = 0;
            sum = sum + ringCentroid(r, w);
            total += w;
        }
    if (std::fabs(total) > 1e-12) return toLonLat(sum);
    sum = {};
    for (const auto& l : g.lines)
        for (size_t i = 0; i + 1 < l.size(); i++) {
            const V3 a = toV3(l[i]), b = toV3(l[i + 1]);
            sum = sum + unit(a + b) * norm(b - a);
        }
    if (norm(sum) > 1e-12) return toLonLat(sum);
    for (const LonLat& p : g.points) sum = sum + toV3(p);
    return norm(sum) > 1e-12 ? toLonLat(sum) : LonLat{};
}

Bounds bounds(const Geometry& g) {
    Bounds b{ 180, 90, -180, -90 };
    auto add = [&](LonLat p) {
        b.west = std::min(b.west, p.lon);
        b.east = std::max(b.east, p.lon);
        b.south = std::min(b.south, p.lat);
        b.north = std::max(b.north, p.lat);
    };
    for (const auto& poly : g.polygons)
        for (const Ring& r : poly)
            for (const LonLat& p : r) add(p);
    for (const auto& l : g.lines)
        for (const LonLat& p : l) add(p);
    for (const LonLat& p : g.points) add(p);
    return b;
}

double area(const Geometry& g) {
    double a = 0;
    for (const auto& poly : g.polygons)
        for (const Ring& r : poly) a += ringArea(r);
    return a * EARTH_RADIUS_KM * EARTH_RADIUS_KM;
}

bool contains(const Geometry& g, LonLat p) {
    for (const auto& poly : g.polygons) {
        bool in = false;
        for (const Ring& r : poly)
            if (inRing(r, p)) in = !in;
        if (in) return true;
    }
    return false;
}

// ================================================================ projections

namespace {

// Robinson's table: parallel length and distance from the equator every 5 degrees.
const double ROBINSON[19][2] = {
    { 1.0000, 0.0000 }, { 0.9986, 0.0620 }, { 0.9954, 0.1240 }, { 0.9900, 0.1860 }, { 0.9822, 0.2480 },
    { 0.9730, 0.3100 }, { 0.9600, 0.3720 }, { 0.9427, 0.4340 }, { 0.9216, 0.4958 }, { 0.8962, 0.5571 },
    { 0.8679, 0.6176 }, { 0.8350, 0.6769 }, { 0.7986, 0.7346 }, { 0.7597, 0.7903 }, { 0.7186, 0.8435 },
    { 0.6732, 0.8936 }, { 0.6213, 0.9394 }, { 0.5722, 0.9761 }, { 0.5322, 1.0000 },
};

double robinsonAt(int column, double latDeg) {
    const double u = std::min(std::fabs(latDeg), 90.0) / 5;
    const int i = std::min(17, (int)u);
    const double t = u - i;
    auto at = [&](int k) { return ROBINSON[std::clamp(k, 0, 18)][column]; };
    const double p0 = at(i - 1), p1 = at(i), p2 = at(i + 1), p3 = at(i + 2);   // Catmull-Rom
    return 0.5 * (2 * p1 + (p2 - p0) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (3 * p1 - p0 - 3 * p2 + p3) * t * t * t);
}

bool azimuthal(Kind k) {
    return k == Kind::Orthographic || k == Kind::Stereographic || k == Kind::AzimuthalEquidistant ||
           k == Kind::AzimuthalEqualArea || k == Kind::Gnomonic;
}

double defaultClip(Kind k) {
    switch (k) {
    case Kind::Orthographic: return 90;
    case Kind::Stereographic: return 142;
    case Kind::Gnomonic: return 60;
    case Kind::AzimuthalEquidistant:
    case Kind::AzimuthalEqualArea: return 179.5;
    default: return 180;
    }
}

// The visible part of a run of rotated points, split into pieces that start where the
// run enters the region and end where it leaves. `in` says whether a point is shown,
// `cross` finds the edge point where an edge leaves or enters.
template <class P, class In, class Cross>
void runs(const std::vector<P>& pts, bool closed, In in, Cross cross, std::vector<std::vector<P>>& pieces,
          std::vector<std::pair<bool, bool>>& ends, bool& whole) {
    // ends[i] = { starts on the boundary, ends on the boundary }
    whole = false;
    const size_t n = pts.size();
    if (n == 0) return;
    size_t start = 0;
    if (closed) {
        // Begin at a point after which the visibility changes, so no piece wraps around.
        bool any = false;
        for (size_t i = 0; i < n; i++)
            if (in(pts[i]) != in(pts[(i + 1) % n])) { start = (i + 1) % n; any = true; break; }
        if (!any) {
            whole = in(pts[0]);
            return;
        }
    }
    std::vector<P> cur;
    bool fromEdge = false;
    const size_t steps = closed ? n : n - 1;
    if (in(pts[start])) cur.push_back(pts[start]);
    for (size_t s = 0; s < steps; s++) {
        const P& a = pts[(start + s) % n];
        const P& b = pts[(start + s + 1) % n];
        const bool ia = in(a), ib = in(b);
        if (ia && ib) {
            cur.push_back(b);
        } else if (ia && !ib) {
            cur.push_back(cross(a, b));
            pieces.push_back(std::move(cur));
            ends.push_back({ fromEdge, true });
            cur.clear();
        } else if (!ia && ib) {
            cur.clear();
            cur.push_back(cross(b, a));
            cur.push_back(b);
            fromEdge = true;
        }
    }
    if (closed && !cur.empty() && !pieces.empty()) {
        // The run began just after it re-entered: that entry point opens the first piece.
        pieces[0].insert(pieces[0].begin(), cur.front());
        ends[0].first = true;
    } else if (!closed && cur.size() > 1) {
        pieces.push_back(std::move(cur));
        ends.push_back({ fromEdge, false });
    }
}

// Joins clipped pieces (each from a boundary entry to a boundary exit) into closed rings:
// from each exit, follow the boundary anticlockwise to the nearest entry. `param` places a
// boundary point on [0, period); `along` appends the boundary from s1 to s2 (anticlockwise).
template <class P, class Param, class Along>
void rejoin(std::vector<std::vector<P>>& pieces, double period, Param param, Along along, std::vector<std::vector<P>>& rings) {
    const size_t n = pieces.size();
    std::vector<double> s0(n), s1(n);
    for (size_t i = 0; i < n; i++) {
        s0[i] = param(pieces[i].front());
        s1[i] = param(pieces[i].back());
    }
    std::vector<bool> used(n, false);
    for (size_t first = 0; first < n; first++) {
        if (used[first]) continue;
        std::vector<P> ring;
        size_t cur = first;
        for (size_t guard = 0; guard <= n; guard++) {
            used[cur] = true;
            ring.insert(ring.end(), pieces[cur].begin(), pieces[cur].end());
            size_t next = n;
            double best = std::numeric_limits<double>::max();
            for (size_t j = 0; j < n; j++) {
                if (used[j] && j != first) continue;
                double d = std::fmod(s0[j] - s1[cur], period);
                if (d < 0) d += period;
                if (d < best) { best = d; next = j; }
            }
            if (next == n) break;
            along(s1[cur], s1[cur] + best, ring);
            if (next == first) break;
            cur = next;
        }
        if (ring.size() >= 3) rings.push_back(std::move(ring));
    }
}

struct LL {
    double l, p;   // rotated longitude and latitude, radians
};

} // namespace

struct Projection::Raw {};

Projection Projection::make(Kind kind) {
    Projection p;
    p.kind_ = kind;
    return p;
}
Projection Projection::equirectangular() { return make(Kind::Equirectangular); }
Projection Projection::mercator() { return make(Kind::Mercator); }
Projection Projection::equalEarth() { return make(Kind::EqualEarth); }
Projection Projection::naturalEarth() { return make(Kind::NaturalEarth); }
Projection Projection::robinson() { return make(Kind::Robinson); }
Projection Projection::mollweide() { return make(Kind::Mollweide); }
Projection Projection::orthographic() { return make(Kind::Orthographic); }
Projection Projection::stereographic() { return make(Kind::Stereographic); }
Projection Projection::azimuthalEquidistant() { return make(Kind::AzimuthalEquidistant); }
Projection Projection::azimuthalEqualArea() { return make(Kind::AzimuthalEqualArea); }
Projection Projection::gnomonic() { return make(Kind::Gnomonic); }
Projection Projection::conicEqualArea() { return make(Kind::ConicEqualArea); }
Projection Projection::conicConformal() { return make(Kind::ConicConformal); }
Projection Projection::conicEquidistant() { return make(Kind::ConicEquidistant); }

Projection Projection::mix(const Projection& a, const Projection& b, double t) {
    Projection m;
    m.kind_ = t < 0.5 ? a.kind_ : b.kind_;
    m.a_ = std::make_shared<Projection>(a);
    m.b_ = std::make_shared<Projection>(b);
    m.t_ = std::clamp(t, 0.0, 1.0);
    double dl = b.rotate_.lon - a.rotate_.lon;
    while (dl > 180) dl -= 360;
    while (dl < -180) dl += 360;
    m.rotate_ = { a.rotate_.lon + dl * m.t_, a.rotate_.lat + (b.rotate_.lat - a.rotate_.lat) * m.t_ };
    m.roll_ = a.roll_ + (b.roll_ - a.roll_) * m.t_;
    m.precision_ = std::min(a.precision_, b.precision_);
    return m;
}

Projection& Projection::rotate(LonLat centre, double roll) {
    rotate_ = centre;
    roll_ = roll;
    return *this;
}
Projection& Projection::scale(double k) {
    scale_ = k;
    return *this;
}
Projection& Projection::translate(Vector2 px) {
    translate_ = px;
    return *this;
}
Projection& Projection::parallels(double a, double b) {
    p1_ = a;
    p2_ = b;
    return *this;
}
Projection& Projection::clipAngle(double degrees) {
    clip_ = degrees;
    return *this;
}
Projection& Projection::precision(double degrees) {
    precision_ = std::max(0.05, degrees);
    return *this;
}

double Projection::cutAngle() const {
    if (a_) return (a_->cutAngle() + (b_->cutAngle() - a_->cutAngle()) * t_);
    return (clip_ > 0 ? std::min(clip_, 180.0) : defaultClip(kind_)) * RAD;
}

bool Projection::cuts() const {
    if (a_) return a_->cuts() || b_->cuts();
    return !azimuthal(kind_);
}

Vector2 Projection::rawProject(double l, double p) const {
    switch (kind_) {
    case Kind::Equirectangular: return { (float)l, (float)p };
    case Kind::Mercator: {
        const double lim = 85.05112878 * RAD, q = std::clamp(p, -lim, lim);
        return { (float)l, (float)std::log(std::tan(kPi / 4 + q / 2)) };
    }
    case Kind::EqualEarth: {
        const double A1 = 1.340264, A2 = -0.081106, A3 = 0.000893, A4 = 0.003796, M = std::sqrt(3.0) / 2;
        const double t = std::asin(M * std::sin(p)), t2 = t * t, t6 = t2 * t2 * t2;
        return { (float)(l * std::cos(t) / (M * (A1 + 3 * A2 * t2 + t6 * (7 * A3 + 9 * A4 * t2)))),
                 (float)(t * (A1 + A2 * t2 + t6 * (A3 + A4 * t2))) };
    }
    case Kind::NaturalEarth: {
        const double p2 = p * p, p4 = p2 * p2;
        return { (float)(l * (0.8707 - 0.131979 * p2 + p4 * (-0.013791 + p4 * (0.003971 * p2 - 0.001529 * p4)))),
                 (float)(p * (1.007226 + p2 * (0.015085 + p4 * (-0.044475 + 0.028874 * p2 - 0.005916 * p4)))) };
    }
    case Kind::Robinson: {
        const double d = p * DEG;
        return { (float)(0.8487 * robinsonAt(0, d) * l), (float)(1.3523 * robinsonAt(1, d) * (d < 0 ? -1 : 1)) };
    }
    case Kind::Mollweide: {
        double th = p;
        const double target = kPi * std::sin(p);
        for (int i = 0; i < 20; i++) {
            const double f = 2 * th + std::sin(2 * th) - target, df = 2 + 2 * std::cos(2 * th);
            if (std::fabs(df) < 1e-12) break;
            const double step = f / df;
            th -= step;
            if (std::fabs(step) < 1e-10) break;
        }
        return { (float)(2 * std::sqrt(2.0) / kPi * l * std::cos(th)), (float)(std::sqrt(2.0) * std::sin(th)) };
    }
    case Kind::Orthographic: {
        const double x = std::cos(p) * std::sin(l), y = std::sin(p), cc = std::cos(p) * std::cos(l);
        if (cc >= 0) return { (float)x, (float)y };
        // The far side, which only a mix shows: unfolded outward (radius 2 - sin c rather
        // than folding back to sin c), so a globe opens smoothly into a flat map.
        const double r = std::hypot(x, y), k = r > 1e-12 ? (2 - r) / r : 0;
        return { (float)(x * k), (float)(y * k) };
    }
    case Kind::Stereographic: {
        const double k = 1 / (1 + std::cos(p) * std::cos(l));
        return { (float)(k * std::cos(p) * std::sin(l)), (float)(k * std::sin(p)) };
    }
    case Kind::Gnomonic: {
        const double k = 1 / std::max(1e-9, std::cos(p) * std::cos(l));
        return { (float)(k * std::cos(p) * std::sin(l)), (float)(k * std::sin(p)) };
    }
    case Kind::AzimuthalEquidistant: {
        const double c = std::acos(std::clamp(std::cos(p) * std::cos(l), -1.0, 1.0)), k = c < 1e-9 ? 1 : c / std::sin(c);
        return { (float)(k * std::cos(p) * std::sin(l)), (float)(k * std::sin(p)) };
    }
    case Kind::AzimuthalEqualArea: {
        const double k = std::sqrt(2 / std::max(1e-12, 1 + std::cos(p) * std::cos(l)));
        return { (float)(k * std::cos(p) * std::sin(l)), (float)(k * std::sin(p)) };
    }
    case Kind::ConicEqualArea: {
        const double f1 = p1_ * RAD, f2 = p2_ * RAD, n = (std::sin(f1) + std::sin(f2)) / 2;
        if (std::fabs(n) < 1e-9) return { (float)(l * std::cos(f1)), (float)(std::sin(p) / std::cos(f1)) };
        const double C = std::cos(f1) * std::cos(f1) + 2 * n * std::sin(f1);
        const double r = std::sqrt(std::max(0.0, C - 2 * n * std::sin(p))) / n, r0 = std::sqrt(C) / n;
        return { (float)(r * std::sin(n * l)), (float)(r0 - r * std::cos(n * l)) };
    }
    case Kind::ConicConformal: {
        const double f1 = p1_ * RAD, f2 = p2_ * RAD;
        auto tn = [](double f) { return std::tan(kPi / 4 + f / 2); };
        const double n = std::fabs(f1 - f2) < 1e-9 ? std::sin(f1) : std::log(std::cos(f1) / std::cos(f2)) / std::log(tn(f2) / tn(f1));
        if (std::fabs(n) < 1e-9) return { (float)l, (float)std::log(tn(std::clamp(p, -1.5, 1.5))) };
        const double F = std::cos(f1) * std::pow(tn(f1), n) / n;
        const double q = n > 0 ? std::max(p, -kPi / 2 + 1e-6) : std::min(p, kPi / 2 - 1e-6);
        const double r = F / std::pow(tn(q), n);
        return { (float)(r * std::sin(n * l)), (float)(F - r * std::cos(n * l)) };
    }
    case Kind::ConicEquidistant: {
        const double f1 = p1_ * RAD, f2 = p2_ * RAD;
        const double n = std::fabs(f1 - f2) < 1e-9 ? std::sin(f1) : (std::cos(f1) - std::cos(f2)) / (f2 - f1);
        if (std::fabs(n) < 1e-9) return { (float)l, (float)p };
        const double G = std::cos(f1) / n + f1, r = G - p;
        return { (float)(r * std::sin(n * l)), (float)(G - r * std::cos(n * l)) };
    }
    }
    return { (float)l, (float)p };
}

Vector2 Projection::place(Vector2 u) const {
    return { translate_.x + (float)scale_ * u.x, translate_.y - (float)scale_ * u.y };
}

Vector2 Projection::screen(double l, double p) const {
    if (a_) {
        const Vector2 A = a_->screen(l, p), B = b_->screen(l, p);
        const float t = (float)t_;
        return { A.x + (B.x - A.x) * t, A.y + (B.y - A.y) * t };
    }
    return place(rawProject(l, p));
}

namespace {

// The rotation that brings `centre` to (lon 0, lat 0), then rolls about it.
struct Rotation {
    double l0, cp, sp, cr, sr;
    bool identity;
    Rotation(LonLat c, double roll)
        : l0(c.lon * RAD), cp(std::cos(c.lat * RAD)), sp(std::sin(c.lat * RAD)), cr(std::cos(roll * RAD)),
          sr(std::sin(roll * RAD)), identity(c.lon == 0 && c.lat == 0 && roll == 0) {}
    V3 operator()(LonLat p) const {
        const V3 v = toV3(p.lon * RAD - l0, p.lat * RAD);
        const V3 w{ v.x * cp + v.z * sp, v.y, -v.x * sp + v.z * cp };
        return { w.x, w.y * cr - w.z * sr, w.y * sr + w.z * cr };
    }
};

LL toLL(V3 v) { return { std::atan2(v.y, v.x), std::asin(std::clamp(v.z, -1.0, 1.0)) }; }

// Rotated points of a run, with long edges subdivided along their great circles.
std::vector<V3> densified(const std::vector<LonLat>& pts, bool closed, const Rotation& rot, double step) {
    std::vector<V3> out;
    const size_t n = pts.size();
    if (!n) return out;
    out.reserve(n * 2);
    V3 prev = rot(pts[0]);
    out.push_back(prev);
    const size_t edges = closed ? n : n - 1;
    for (size_t i = 1; i <= edges; i++) {
        const V3 cur = rot(pts[i % n]);
        const double w = std::acos(std::clamp(dot(prev, cur), -1.0, 1.0));
        const int k = (int)std::ceil(w / step);
        for (int j = 1; j < k; j++) out.push_back(slerp(prev, cur, (double)j / k));
        if (i < n) out.push_back(cur);
        prev = cur;
    }
    return out;
}

// For an identity rotation, the input coordinates exactly (so points on the antimeridian
// stay on their side), still subdivided.
std::vector<LL> exactLL(const std::vector<LonLat>& pts, bool closed, double step) {
    std::vector<LL> out;
    const size_t n = pts.size();
    if (!n) return out;
    const size_t edges = closed ? n : n - 1;
    out.push_back({ pts[0].lon * RAD, pts[0].lat * RAD });
    for (size_t i = 1; i <= edges; i++) {
        const LonLat a = pts[i - 1], b = pts[i % n];
        const double dl = std::fabs(b.lon - a.lon) > 180 ? 0 : b.lon - a.lon;
        const double span = std::max(std::fabs(dl), std::fabs(b.lat - a.lat)) * RAD;
        const int k = (int)std::ceil(span / step);
        for (int j = 1; j < k; j++) {
            const double t = (double)j / k;
            out.push_back({ (a.lon + dl * t) * RAD, (a.lat + (b.lat - a.lat) * t) * RAD });
        }
        if (i < n) out.push_back({ b.lon * RAD, b.lat * RAD });
    }
    return out;
}

// ---- the circle clip: points within `theta` of the centre (1, 0, 0)

struct Circle {
    double theta, c, s;
    explicit Circle(double t) : theta(t), c(std::cos(t)), s(std::sin(t)) {}
    bool in(V3 v) const { return v.x >= c; }
    V3 cross(V3 a, V3 b) const {   // a inside, b outside
        const double t = (c - a.x) / (b.x - a.x);
        V3 p = unit(a + (b - a) * t);
        const double r = std::hypot(p.y, p.z);
        return r > 1e-12 ? V3{ c, p.y / r * s, p.z / r * s } : V3{ c, s, 0 };
    }
    double param(V3 v) const {
        double a = std::atan2(v.z, v.y);
        return a < 0 ? a + TAU : a;
    }
    V3 at(double a) const { return { c, s * std::cos(a), s * std::sin(a) }; }
};

void circleAlong(const Circle& k, double s1, double s2, double step, std::vector<V3>& out) {
    const int n = std::max(1, (int)std::ceil((s2 - s1) / step));
    for (int i = 1; i < n; i++) out.push_back(k.at(s1 + (s2 - s1) * i / n));
}

// Winding of a ring about the centre, in turns (positive anticlockwise seen from outside).
double winding(const std::vector<V3>& r) {
    double w = 0;
    for (size_t i = 0; i < r.size(); i++) {
        const V3 a = r[i], b = r[(i + 1) % r.size()];
        double d = std::atan2(b.z, b.y) - std::atan2(a.z, a.y);
        while (d > kPi) d -= TAU;
        while (d < -kPi) d += TAU;
        w += d;
    }
    return w / TAU;
}

// ---- the antimeridian cut: longitudes in [-pi, pi]

bool jumps(LL a, LL b) { return std::fabs(b.l - a.l) > kPi; }

// The latitude where the edge a -> b crosses the antimeridian.
double crossLat(LL a, LL b) {
    const double bl = b.l + (a.l > 0 ? TAU : -TAU);
    const double d = bl - a.l;
    const double t = std::fabs(d) < 1e-12 ? 0.5 : ((a.l > 0 ? kPi : -kPi) - a.l) / d;
    return a.p + (b.p - a.p) * t;
}

double rectParam(LL q) {
    if (q.l >= 0) return 2 * kPi + (q.p + kPi / 2);   // the east edge, going north
    double s = 5 * kPi + (kPi / 2 - q.p);              // the west edge, going south
    return s >= 6 * kPi - 1e-12 ? s - 6 * kPi : s;
}

LL rectAt(double s) {
    s = std::fmod(s, 6 * kPi);
    if (s < 0) s += 6 * kPi;
    if (s < 2 * kPi) return { -kPi + s, -kPi / 2 };
    if (s < 3 * kPi) return { kPi, s - 2 * kPi - kPi / 2 };
    if (s < 5 * kPi) return { kPi - (s - 3 * kPi), kPi / 2 };
    return { -kPi, kPi / 2 - (s - 5 * kPi) };
}

void rectAlong(double s1, double s2, double step, std::vector<LL>& out) {
    const int n = std::max(1, (int)std::ceil((s2 - s1) / step));
    for (int i = 1; i < n; i++) out.push_back(rectAt(s1 + (s2 - s1) * i / n));
}

// Splits a run at the antimeridian into pieces; the crossing points sit on +-pi.
void cutRun(const std::vector<LL>& pts, bool closed, std::vector<std::vector<LL>>& pieces, bool& whole) {
    whole = false;
    const size_t n = pts.size();
    if (!n) return;
    size_t start = 0;
    if (closed) {
        bool any = false;
        for (size_t i = 0; i < n; i++)
            if (jumps(pts[i], pts[(i + 1) % n])) { start = (i + 1) % n; any = true; break; }
        if (!any) { whole = true; return; }
    }
    std::vector<LL> cur{ pts[start] };
    const size_t steps = closed ? n : n - 1;
    for (size_t s = 0; s < steps; s++) {
        const LL a = pts[(start + s) % n], b = pts[(start + s + 1) % n];
        if (jumps(a, b)) {
            const double p = crossLat(a, b);
            cur.push_back({ a.l > 0 ? kPi : -kPi, p });
            pieces.push_back(std::move(cur));
            cur = { { a.l > 0 ? -kPi : kPi, p } };
        }
        cur.push_back(b);
    }
    if (closed) {
        // The last edge was the crossing before `start`: its far side opens the first piece.
        pieces[0].insert(pieces[0].begin(), cur.front());
    } else if (cur.size() > 1) {
        pieces.push_back(std::move(cur));
    }
}

} // namespace

bool Projection::project(LonLat p, Vector2& out) const {
    const Rotation rot(rotate_, roll_);
    const V3 v = rot(p);
    if (cutAngle() < kPi - 1e-9 && v.x < std::cos(cutAngle())) return false;
    out = (*this)(p);
    return true;
}

Vector2 Projection::operator()(LonLat p) const {
    const Rotation rot(rotate_, roll_);
    if (rot.identity) return screen(p.lon * RAD, p.lat * RAD);   // keeps lon 180 on the east edge
    const LL q = toLL(rot(p));
    return screen(q.l, q.p);
}

vg::Path Projection::path(const Geometry& g) const {
    vg::Path out;
    const Rotation rot(rotate_, roll_);
    const double step = precision_ * RAD, theta = cutAngle();
    const bool circle = theta < kPi - 1e-9, cut = cuts();
    const Circle k(theta);
    auto emit = [&](const std::vector<LL>& ring) {
        if (ring.size() < 3) return;
        out.moveTo(screen(ring[0].l, ring[0].p));
        for (size_t i = 1; i < ring.size(); i++) out.lineTo(screen(ring[i].l, ring[i].p));
        out.close();
    };
    auto cutRings = [&](std::vector<std::vector<LL>>& rings) {
        if (!cut) {
            for (auto& r : rings) emit(r);
            return;
        }
        std::vector<std::vector<LL>> pieces;
        for (auto& r : rings) {
            bool whole = false;
            std::vector<std::vector<LL>> mine;
            cutRun(r, true, mine, whole);
            if (whole) emit(r);
            else for (auto& m : mine) pieces.push_back(std::move(m));
        }
        if (pieces.empty()) return;
        std::vector<std::vector<LL>> joined;
        rejoin(pieces, 6 * kPi, rectParam, [&](double s1, double s2, std::vector<LL>& o) { rectAlong(s1, s2, step, o); }, joined);
        for (auto& r : joined) emit(r);
    };
    for (const auto& poly : g.polygons) {
        std::vector<std::vector<LL>> rings;
        if (!circle && rot.identity) {
            for (const Ring& r : poly) rings.push_back(exactLL(r, true, step));
            cutRings(rings);
            continue;
        }
        std::vector<std::vector<V3>> pieces, kept;
        for (const Ring& r : poly) {
            std::vector<V3> v = densified(r, true, rot, step);
            if (!circle) { kept.push_back(std::move(v)); continue; }
            std::vector<std::vector<V3>> mine;
            std::vector<std::pair<bool, bool>> ends;
            bool whole = false;
            runs(v, true, [&](V3 p) { return k.in(p); }, [&](V3 a, V3 b) { return k.cross(a, b); }, mine, ends, whole);
            if (whole) {
                kept.push_back(std::move(v));
            } else if (mine.empty()) {
                // Hidden entirely: it still matters when its inside covers the visible disc
                // (it runs anticlockwise round the centre, seen from the front).
                if (winding(v) > 0.5) {
                    std::vector<V3> disc;
                    const int n = std::max(16, (int)std::ceil(TAU / step));
                    for (int i = 0; i < n; i++) disc.push_back(k.at(TAU * i / n));
                    kept.push_back(std::move(disc));
                }
            } else {
                for (auto& m : mine) pieces.push_back(std::move(m));
            }
        }
        if (!pieces.empty())
            rejoin(pieces, TAU, [&](V3 p) { return k.param(p); },
                   [&](double s1, double s2, std::vector<V3>& o) { circleAlong(k, s1, s2, step, o); }, kept);
        for (auto& r : kept) {
            std::vector<LL> ll;
            ll.reserve(r.size());
            for (const V3& p : r) ll.push_back(toLL(p));
            rings.push_back(std::move(ll));
        }
        cutRings(rings);
    }
    return out;
}

vg::Path Projection::lines(const Geometry& g) const {
    vg::Path out;
    const Rotation rot(rotate_, roll_);
    const double step = precision_ * RAD, theta = cutAngle();
    const bool circle = theta < kPi - 1e-9, cut = cuts();
    const Circle k(theta);
    auto emit = [&](const std::vector<LL>& run, bool closed) {
        if (run.size() < 2) return;
        out.moveTo(screen(run[0].l, run[0].p));
        for (size_t i = 1; i < run.size(); i++) out.lineTo(screen(run[i].l, run[i].p));
        if (closed) out.close();
    };
    auto cutEmit = [&](const std::vector<LL>& run, bool closed) {
        if (!cut) { emit(run, closed); return; }
        std::vector<std::vector<LL>> pieces;
        bool whole = false;
        cutRun(run, closed, pieces, whole);
        if (whole) emit(run, true);
        else for (auto& p : pieces) emit(p, false);
    };
    auto one = [&](const std::vector<LonLat>& pts, bool closed) {
        if (!circle && rot.identity) { cutEmit(exactLL(pts, closed, step), closed); return; }
        std::vector<V3> v = densified(pts, closed, rot, step);
        auto asLL = [](const std::vector<V3>& r) {
            std::vector<LL> ll;
            ll.reserve(r.size());
            for (const V3& p : r) ll.push_back(toLL(p));
            return ll;
        };
        if (!circle) { cutEmit(asLL(v), closed); return; }
        std::vector<std::vector<V3>> pieces;
        std::vector<std::pair<bool, bool>> ends;
        bool whole = false;
        runs(v, closed, [&](V3 p) { return k.in(p); }, [&](V3 a, V3 b) { return k.cross(a, b); }, pieces, ends, whole);
        if (whole) { cutEmit(asLL(v), closed); return; }
        for (auto& p : pieces) cutEmit(asLL(p), false);
    };
    for (const auto& poly : g.polygons)
        for (const Ring& r : poly) one(r, true);
    for (const auto& l : g.lines) one(l, false);
    return out;
}

vg::Path Projection::outline() const {
    const double theta = cutAngle(), step = precision_ * RAD;
    const Circle k(theta);
    std::vector<LL> loop;
    if (!cuts()) {
        const int n = std::max(32, (int)std::ceil(TAU / step));
        for (int i = 0; i < n; i++) loop.push_back(toLL(k.at(TAU * i / n)));
    } else {
        // The rectangle round the cut (anticlockwise). With a clip circle as well (a mix of
        // a flat map and an azimuthal one) the hole round the antipode, radius r, is taken
        // out of both edges along the circle.
        const double r = theta < kPi - 1e-9 ? kPi - theta : 0;
        auto edge = [&](LL a, LL b) {
            const int n = std::max(1, (int)std::ceil(std::max(std::fabs(b.l - a.l), std::fabs(b.p - a.p)) / step));
            for (int i = 0; i < n; i++) loop.push_back({ a.l + (b.l - a.l) * i / n, a.p + (b.p - a.p) * i / n });
        };
        auto arc = [&](double from, double to, double side) {
            const int n = std::max(2, (int)std::ceil(std::fabs(to - from) / step));
            for (int i = 0; i < n; i++) {
                LL q = toLL(k.at(from + (to - from) * i / n));
                if (q.l * side < 0) q.l += side * TAU;
                loop.push_back(q);
            }
        };
        const double h = kPi / 2;
        edge({ -kPi, -h }, { kPi, -h });
        if (r > 0) {
            edge({ kPi, -h }, { kPi, -r });
            arc(-h, h, 1);
            edge({ kPi, r }, { kPi, h });
        } else {
            edge({ kPi, -h }, { kPi, h });
        }
        edge({ kPi, h }, { -kPi, h });
        if (r > 0) {
            edge({ -kPi, h }, { -kPi, r });
            arc(h, 3 * h, -1);
            edge({ -kPi, -r }, { -kPi, -h });
        } else {
            edge({ -kPi, h }, { -kPi, -h });
        }
    }
    vg::Path out;
    for (size_t i = 0; i < loop.size(); i++) {
        const Vector2 p = screen(loop[i].l, loop[i].p);
        if (i == 0) out.moveTo(p);
        else out.lineTo(p);
    }
    out.close();
    return out;
}

vg::Path Projection::graticule(double step, double polar) const {
    Geometry g;
    step = std::max(1.0, step);
    for (double lon = -180; lon < 180 - 1e-9; lon += step) g.lines.push_back({ { lon, -90 + polar }, { lon, 90 - polar } });
    for (double lat = -90 + step; lat < 90 - 1e-9; lat += step) {
        std::vector<LonLat> l;
        for (double lon = -180; lon <= 180 + 1e-9; lon += 2.5) l.push_back({ lon, lat });
        g.lines.push_back(std::move(l));
    }
    return lines(g);
}

Projection& Projection::fit(Rectangle box, const Geometry& g, float padding) {
    vg::Path p = g.empty() ? outline() : path(g);
    p.append(lines(Geometry{ {}, g.lines, {} }));
    for (const LonLat& q : g.points) {
        Vector2 at;
        if (project(q, at)) p.moveTo(at).lineTo(at);
    }
    const Rectangle b = p.bounds();
    if (b.width <= 0 && b.height <= 0) return *this;
    const float w = box.width - 2 * padding, h = box.height - 2 * padding;
    const double f = std::min(b.width > 0 ? w / b.width : 1e9, b.height > 0 ? h / b.height : 1e9);
    const Vector2 from{ b.x + b.width / 2, b.y + b.height / 2 }, to{ box.x + box.width / 2, box.y + box.height / 2 };
    // Every screen position is an affine image of the projected one, so scaling about the
    // old centre and moving it to the new one fits (for a mix, both ends get the same move).
    std::function<void(Projection&)> apply = [&](Projection& q) {
        if (q.a_) {
            auto a = std::make_shared<Projection>(*q.a_), b2 = std::make_shared<Projection>(*q.b_);
            apply(*a);
            apply(*b2);
            q.a_ = a;
            q.b_ = b2;
            return;
        }
        q.scale_ *= f;
        q.translate_ = { to.x + (float)f * (q.translate_.x - from.x), to.y + (float)f * (q.translate_.y - from.y) };
    };
    apply(*this);
    return *this;
}

} // namespace geo
} // namespace rmtn
