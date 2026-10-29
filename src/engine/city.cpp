// city.cpp -- CityJSON reading (1.0 to 2.0, CityJSONSeq, geometry templates), polygon
// triangulation with holes, and g3d meshes and helpers for city models.
#include "rmtn/city.hpp"
#include "rmtn/media.hpp"
#include <earcut/earcut.hpp>
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace rmtn {
namespace city {

namespace {

using D3 = std::array<double, 3>;

std::string lodName(const Json& l) {
    if (l.isString()) return l.asString();
    if (l.isNumber()) {
        char b[32];
        std::snprintf(b, sizeof b, "%g", l.asNumber());
        return b;
    }
    return "";
}

const Json::Array& arr(const Json& j, const char* what) {
    if (!j.isArray()) throw std::runtime_error(std::string("cityjson: ") + what + " must be an array");
    return j.asArray();
}

// The vertices of one CityJSON (or CityJSONFeature) document, in file coordinates.
struct Vertices {
    std::vector<D3> v;
    void read(const Json& list, const Json& transform) {
        D3 s{ 1, 1, 1 }, t{ 0, 0, 0 };
        if (transform.isObject()) {
            for (int i = 0; i < 3; i++) {
                s[i] = transform["scale"][i].asNumber();
                t[i] = transform["translate"][i].asNumber();
            }
        }
        if (list.isNull()) return;
        for (const Json& p : arr(list, "vertices")) {
            if (!p.isArray() || p.size() < 3) throw std::runtime_error("cityjson: a vertex needs [x, y, z]");
            v.push_back({ p[0].asNumber() * s[0] + t[0], p[1].asNumber() * s[1] + t[1], p[2].asNumber() * s[2] + t[2] });
        }
    }
};

struct Templates {
    std::vector<const Json*> geometry;
    std::vector<D3> vertices;
};

struct Reader {
    Model& model;
    D3 origin{ 0, 0, 0 };
    const Templates* templates = nullptr;

    Vector3 at(const Vertices& vs, const Json& index) const {
        const double i = index.asNumber();
        if (i < 0 || i >= (double)vs.v.size()) throw std::runtime_error("cityjson: vertex index out of range");
        const D3& p = vs.v[(size_t)i];
        return { (float)(p[0] - origin[0]), (float)(p[1] - origin[1]), (float)(p[2] - origin[2]) };
    }

    std::vector<Vector3> ring(const Vertices& vs, const Json& r) const {
        std::vector<Vector3> out;
        for (const Json& i : arr(r, "a ring")) out.push_back(at(vs, i));
        return out;
    }

    // One surface (rings) with its semantic value (an index into `surfaces`, or null).
    void surface(const Vertices& vs, const Json& s, const Json& value, const Json& surfaces, Geometry& g) const {
        Face f;
        for (const Json& r : arr(s, "a surface")) {
            auto k = ring(vs, r);
            if (k.size() >= 3) f.rings.push_back(std::move(k));
        }
        if (f.rings.empty()) return;
        if (value.isNumber() && surfaces.isArray()) {
            const size_t i = (size_t)value.asNumber();
            if (i < surfaces.size()) {
                f.semantic = surfaces[i];
                if (f.semantic["type"].isString()) f.kind = f.semantic["type"].asString();
            }
        }
        g.faces.push_back(std::move(f));
    }

    void shell(const Vertices& vs, const Json& sh, const Json& values, const Json& surfaces, Geometry& g) const {
        const auto& list = arr(sh, "a shell");
        for (size_t i = 0; i < list.size(); i++) surface(vs, list[i], values.isArray() ? values[i] : Json(), surfaces, g);
    }

    Geometry geometry(const Vertices& vs, const Json& j) const {
        Geometry g;
        g.type = j["type"].isString() ? j["type"].asString() : "";
        g.lod = lodName(j["lod"]);
        const Json& b = j["boundaries"];
        const Json& sem = j["semantics"];
        const Json& surfaces = sem.isObject() ? sem["surfaces"] : Json();
        const Json& values = sem.isObject() ? sem["values"] : Json();
        auto valueAt = [](const Json& v, size_t i) { return v.isArray() && i < v.size() ? v[i] : Json(); };
        if (g.type == "MultiPoint") {
            for (const Json& i : arr(b, "boundaries")) g.points.push_back(at(vs, i));
        } else if (g.type == "MultiLineString") {
            for (const Json& l : arr(b, "boundaries")) g.lines.push_back(ring(vs, l));
        } else if (g.type == "MultiSurface" || g.type == "CompositeSurface") {
            const auto& list = arr(b, "boundaries");
            for (size_t i = 0; i < list.size(); i++) surface(vs, list[i], valueAt(values, i), surfaces, g);
        } else if (g.type == "Solid") {
            const auto& list = arr(b, "boundaries");
            for (size_t i = 0; i < list.size(); i++) shell(vs, list[i], valueAt(values, i), surfaces, g);
        } else if (g.type == "MultiSolid" || g.type == "CompositeSolid") {
            const auto& list = arr(b, "boundaries");
            for (size_t i = 0; i < list.size(); i++) {
                const Json v = valueAt(values, i);
                const auto& shells = arr(list[i], "a solid");
                for (size_t k = 0; k < shells.size(); k++) shell(vs, shells[k], valueAt(v, k), surfaces, g);
            }
        } else if (g.type == "GeometryInstance") {
            return instance(vs, j);
        } else {
            throw std::runtime_error("cityjson: unknown geometry type '" + g.type + "'");
        }
        return g;
    }

    // A template placed at a reference vertex, through a 4x4 row-major matrix.
    Geometry instance(const Vertices& vs, const Json& j) const {
        if (!templates || templates->geometry.empty()) throw std::runtime_error("cityjson: GeometryInstance without geometry-templates");
        const double ti = j["template"].asNumber();
        if (ti < 0 || ti >= (double)templates->geometry.size()) throw std::runtime_error("cityjson: template index out of range");
        const auto& b = arr(j["boundaries"], "GeometryInstance boundaries");
        if (b.empty()) throw std::runtime_error("cityjson: GeometryInstance needs a reference vertex");
        const Vector3 anchor = at(vs, b[0]);
        double m[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        const Json& tm = j["transformationMatrix"];
        if (tm.isArray() && tm.size() == 16)
            for (int i = 0; i < 16; i++) m[i] = tm[(size_t)i].asNumber();
        Vertices placed;
        for (const D3& p : templates->vertices) {
            const double x = m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3];
            const double y = m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7];
            const double z = m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11];
            placed.v.push_back({ x + anchor.x + origin[0], y + anchor.y + origin[1], z + anchor.z + origin[2] });
        }
        const Json& tj = *templates->geometry[(size_t)ti];
        if (tj["type"].isString() && tj["type"].asString() == "GeometryInstance") throw std::runtime_error("cityjson: nested GeometryInstance");
        return geometry(placed, tj);
    }

    void objects(const Vertices& vs, const Json& cityObjects) {
        if (cityObjects.isNull()) return;
        if (!cityObjects.isObject()) throw std::runtime_error("cityjson: CityObjects must be an object");
        for (const auto& [id, o] : cityObjects.asObject()) {
            Object obj;
            obj.id = id;
            obj.type = o["type"].isString() ? o["type"].asString() : "";
            if (obj.type.empty()) throw std::runtime_error("cityjson: city object '" + id + "' has no type");
            obj.attributes = o["attributes"].isObject() ? o["attributes"] : Json(Json::Object{});
            for (const char* k : { "parents", "children" })
                if (o[k].isArray())
                    for (const Json& s : o[k].asArray())
                        if (s.isString()) (k[0] == 'p' ? obj.parents : obj.children).push_back(s.asString());
            if (o["geometry"].isArray())
                for (const Json& g : o["geometry"].asArray()) obj.geometry.push_back(geometry(vs, g));
            model.objects.push_back(std::move(obj));
        }
    }
};

void readTemplates(const Json& j, Templates& t) {
    const Json& gt = j["geometry-templates"];
    if (!gt.isObject()) return;
    for (const Json& g : arr(gt["templates"], "templates")) t.geometry.push_back(&g);
    for (const Json& p : arr(gt["vertices-templates"], "vertices-templates")) {
        if (!p.isArray() || p.size() < 3) throw std::runtime_error("cityjson: a template vertex needs [x, y, z]");
        t.vertices.push_back({ p[0].asNumber(), p[1].asNumber(), p[2].asNumber() });
    }
}

// The middle of the vertices' extent, at their lowest point.
D3 originOf(const std::vector<const Vertices*>& all) {
    D3 lo{ 1e300, 1e300, 1e300 }, hi{ -1e300, -1e300, -1e300 };
    bool any = false;
    for (const Vertices* vs : all)
        for (const D3& p : vs->v) {
            any = true;
            for (int i = 0; i < 3; i++) {
                lo[i] = std::min(lo[i], p[i]);
                hi[i] = std::max(hi[i], p[i]);
            }
        }
    if (!any) return { 0, 0, 0 };
    return { (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2] };
}

void header(const Json& j, Model& m) {
    const std::string type = j["type"].isString() ? j["type"].asString() : "";
    if (type != "CityJSON") throw std::runtime_error("cityjson: not a CityJSON file (type is '" + type + "')");
    m.version = j["version"].isString() ? j["version"].asString() : "";
    m.metadata = j["metadata"].isObject() ? j["metadata"] : Json(Json::Object{});
    const Json& rs = m.metadata["referenceSystem"];
    if (rs.isString()) m.crs = rs.asString();
}

// ---------------------------------------------------------------- triangulation

// Newell's normal: robust for any planar (or nearly planar) ring; length is twice its area.
Vector3 newell(const std::vector<Vector3>& r) {
    double x = 0, y = 0, z = 0;
    for (size_t i = 0, n = r.size(); i < n; i++) {
        const Vector3 a = r[i], b = r[(i + 1) % n];
        x += ((double)a.y - b.y) * ((double)a.z + b.z);
        y += ((double)a.z - b.z) * ((double)a.x + b.x);
        z += ((double)a.x - b.x) * ((double)a.y + b.y);
    }
    return { (float)x, (float)y, (float)z };
}

} // namespace

std::vector<int> triangles(const Face& f, Vector3* normalOut) {
    std::vector<int> out;
    if (normalOut) *normalOut = { 0, 0, 0 };
    if (f.rings.empty() || f.rings[0].size() < 3) return out;
    const Vector3 n = Vector3Normalize(newell(f.rings[0]));
    if (Vector3Length(n) < 0.5f) return out;   // degenerate (no area)
    if (normalOut) *normalOut = n;
    // A right-handed basis (u, w, n) in the face's plane: anticlockwise there is the face's
    // own winding. Coordinates are taken about the first point, for precision.
    const Vector3 ref = std::fabs(n.z) < 0.9f ? Vector3{ 0, 0, 1 } : Vector3{ 1, 0, 0 };
    const Vector3 u = Vector3Normalize(Vector3CrossProduct(ref, n)), w = Vector3CrossProduct(n, u);
    const Vector3 o = f.rings[0][0];
    std::vector<std::vector<std::array<double, 2>>> poly;
    std::vector<std::array<double, 2>> flat;
    for (const auto& r : f.rings) {
        std::vector<std::array<double, 2>> p;
        for (const Vector3& q : r) {
            const Vector3 d = Vector3Subtract(q, o);
            p.push_back({ (double)Vector3DotProduct(d, u), (double)Vector3DotProduct(d, w) });
        }
        flat.insert(flat.end(), p.begin(), p.end());
        poly.push_back(std::move(p));
    }
    const std::vector<uint32_t> idx = mapbox::earcut<uint32_t>(poly);
    out.reserve(idx.size());
    for (size_t i = 0; i + 2 < idx.size(); i += 3) {
        const auto& a = flat[idx[i]];
        const auto& b = flat[idx[i + 1]];
        const auto& c = flat[idx[i + 2]];
        const double cr = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
        if (cr >= 0) out.insert(out.end(), { (int)idx[i], (int)idx[i + 1], (int)idx[i + 2] });
        else out.insert(out.end(), { (int)idx[i], (int)idx[i + 2], (int)idx[i + 1] });
    }
    return out;
}


// ================================================================ the model

const Geometry* Object::at(const std::string& lod) const {
    const Geometry* best = nullptr;
    for (const Geometry& g : geometry) {
        if (!lod.empty() && g.lod == lod) return &g;
        if (!best || std::atof(g.lod.c_str()) > std::atof(best->lod.c_str())) best = &g;
    }
    return best;
}

BoundingBox Object::bounds(const std::string& lod) const {
    BoundingBox b{ { 1e30f, 1e30f, 1e30f }, { -1e30f, -1e30f, -1e30f } };
    bool any = false;
    auto add = [&](Vector3 p) {
        any = true;
        b.min = Vector3Min(b.min, p);
        b.max = Vector3Max(b.max, p);
    };
    if (const Geometry* g = at(lod)) {
        for (const Face& f : g->faces)
            for (const auto& r : f.rings)
                for (Vector3 p : r) add(p);
        for (const auto& l : g->lines)
            for (Vector3 p : l) add(p);
        for (Vector3 p : g->points) add(p);
    }
    return any ? b : BoundingBox{};
}

float Object::height(const std::string& lod) const {
    const BoundingBox b = bounds(lod);
    return b.max.z - b.min.z;
}

const Object* Model::find(const std::string& id) const {
    for (const Object& o : objects)
        if (o.id == id) return &o;
    return nullptr;
}

const Object& Model::get(const std::string& id) const {
    if (const Object* o = find(id)) return *o;
    throw std::runtime_error("cityjson: no city object '" + id + "'");
}

Model Model::filter(const std::function<bool(const Object&)>& keep) const {
    Model m;
    m.version = version;
    m.crs = crs;
    m.origin = origin;
    m.metadata = metadata;
    for (const Object& o : objects)
        if (keep(o)) m.objects.push_back(o);
    return m;
}

std::vector<std::string> Model::lods() const {
    std::vector<std::string> out;
    for (const Object& o : objects)
        for (const Geometry& g : o.geometry)
            if (std::find(out.begin(), out.end(), g.lod) == out.end()) out.push_back(g.lod);
    std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) { return std::atof(a.c_str()) < std::atof(b.c_str()); });
    return out;
}

BoundingBox Model::bounds(const std::string& lod) const {
    BoundingBox b{ { 1e30f, 1e30f, 1e30f }, { -1e30f, -1e30f, -1e30f } };
    bool any = false;
    for (const Object& o : objects) {
        if (!o.at(lod)) continue;
        const BoundingBox ob = o.bounds(lod);
        if (ob.min.x > ob.max.x) continue;
        b.min = Vector3Min(b.min, ob.min);
        b.max = Vector3Max(b.max, ob.max);
        any = true;
    }
    return any ? b : BoundingBox{};
}

std::array<double, 3> Model::world(Vector3 p) const { return { p.x + origin[0], p.y + origin[1], p.z + origin[2] }; }

Vector3 Model::local(const std::array<double, 3>& w) const {
    return { (float)(w[0] - origin[0]), (float)(w[1] - origin[1]), (float)(w[2] - origin[2]) };
}

Model parse(const std::string& text) {
    Model m;
    // CityJSONSeq: one JSON document per line, the first a CityJSON header.
    size_t first = text.find_first_not_of(" \t\r\n\xEF\xBB\xBF");
    if (first == std::string::npos) throw std::runtime_error("cityjson: empty input");
    std::vector<Json> docs;
    {
        bool seq = false;
        size_t end = text.find('\n', first);
        if (end != std::string::npos && text.find_first_not_of(" \t\r\n", end) != std::string::npos) {
            try {
                Json head = Json::parse(std::string_view(text).substr(first, end - first));
                seq = head["type"].isString() && head["type"].asString() == "CityJSON";
                if (seq) docs.push_back(std::move(head));
            } catch (const std::runtime_error&) {
                seq = false;   // a pretty-printed single document
            }
        }
        if (seq) {
            std::istringstream in(text.substr(end + 1));
            std::string line;
            while (std::getline(in, line)) {
                if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
                docs.push_back(Json::parse(line));
            }
        } else {
            docs.push_back(Json::parse(text));
        }
    }
    header(docs[0], m);
    const Json& transform = docs[0]["transform"];
    Templates templates;
    readTemplates(docs[0], templates);

    std::vector<Vertices> verts(docs.size());
    for (size_t i = 0; i < docs.size(); i++) {
        if (i > 0) {
            const std::string t = docs[i]["type"].isString() ? docs[i]["type"].asString() : "";
            if (t != "CityJSONFeature") throw std::runtime_error("cityjson: line " + std::to_string(i + 1) + " is not a CityJSONFeature");
        }
        verts[i].read(docs[i]["vertices"], transform);
    }
    std::vector<const Vertices*> all;
    for (const Vertices& v : verts) all.push_back(&v);
    const D3 o = originOf(all);
    m.origin = o;

    Reader r{ m, o, &templates };
    for (size_t i = 0; i < docs.size(); i++) r.objects(verts[i], docs[i]["CityObjects"]);
    return m;
}

Model load(const std::string& file) {
    const std::string path = media::resolve(file);
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    if (!in) throw std::runtime_error("cityjson: cannot read " + path);
    std::stringstream s;
    s << in.rdbuf();
    try {
        return parse(s.str());
    } catch (const std::runtime_error& e) {
        throw std::runtime_error(path + ": " + e.what());
    }
}

// ================================================================ meshes

BoundingBox toScene(BoundingBox b) {
    const Vector3 a = toScene(b.min), c = toScene(b.max);
    return { Vector3Min(a, c), Vector3Max(a, c) };
}

Color colorOf(const Look& look, const Object& o, const Face& f) {
    if (look.color) {
        const Color c = look.color(o, f);
        if (c.a > 0) return c;
    }
    const std::string& k = f.kind;
    if (k == "RoofSurface") return look.roof;
    if (k == "WallSurface" || k == "OuterCeilingSurface" || k == "OuterFloorSurface" || k == "ClosureSurface") return look.wall;
    if (k == "GroundSurface") return look.ground;
    if (k == "Window") return look.window;
    if (k == "Door") return look.door;
    if (k == "WaterSurface" || k == "WaterGroundSurface" || k == "WaterClosureSurface") return look.water;
    if (k == "TrafficArea" || k == "AuxiliaryTrafficArea" || k == "TransportationMarking") return look.road;
    const std::string& t = o.type;
    if (t == "Road" || t == "Railway" || t == "TransportSquare" || t == "Waterway" || t == "Bridge" || t == "BridgePart")
        return t == "Waterway" ? look.water : look.road;
    if (t == "WaterBody") return look.water;
    if (t == "PlantCover" || t == "SolitaryVegetationObject") return look.vegetation;
    if (t == "CityFurniture") return look.furniture;
    if (t == "TINRelief" || t == "LandUse") return look.terrain;
    if (t == "Building" || t == "BuildingPart" || t == "BuildingInstallation" || t == "BuildingConstructiveElement") {
        // Without semantics: roofs by the face's slope.
        if (!f.rings.empty()) {
            const Vector3 n = Vector3Normalize(newell(f.rings[0]));
            if (n.z > 0.3f) return look.roof;
            if (n.z < -0.3f) return look.ground;
        }
        return look.wall;
    }
    return look.other;
}

namespace {

struct Buffer {
    std::vector<float> v, n;
    std::vector<unsigned char> c;
    void add(const Object& o, const Look& look, Vector3 offset) {
        const Geometry* g = o.at(look.lod);
        if (!g) return;
        for (const Face& f : g->faces) {
            if (f.rings.empty() || f.rings[0].size() < 3) continue;
            Vector3 normal{};
            const std::vector<int> tris = triangles(f, &normal);
            if (tris.empty()) continue;
            std::vector<Vector3> pts;
            for (const auto& r : f.rings) pts.insert(pts.end(), r.begin(), r.end());
            const Color col = colorOf(look, o, f);
            const Vector3 sn = toScene(normal);
            for (int i : tris) {
                const Vector3 p = toScene(Vector3Subtract(pts[(size_t)i], offset));
                v.insert(v.end(), { p.x, p.y, p.z });
                n.insert(n.end(), { sn.x, sn.y, sn.z });
                c.insert(c.end(), { col.r, col.g, col.b, col.a });
            }
        }
    }
    Mesh upload() const {
        Mesh m{};
        m.vertexCount = (int)(v.size() / 3);
        m.triangleCount = m.vertexCount / 3;
        if (m.vertexCount == 0) {   // an empty mesh still needs a valid buffer
            m.vertexCount = 3;
            m.triangleCount = 1;
        }
        m.vertices = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
        m.normals = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
        m.colors = (unsigned char*)MemAlloc((unsigned)(m.vertexCount * 4));
        if (!v.empty()) {
            std::copy(v.begin(), v.end(), m.vertices);
            std::copy(n.begin(), n.end(), m.normals);
            std::copy(c.begin(), c.end(), m.colors);
        }
        UploadMesh(&m, false);
        return m;
    }
};

g3d::GeometryPtr geometryOf(Buffer b) {
    auto shared = std::make_shared<Buffer>(std::move(b));
    return std::make_shared<g3d::Geometry>([shared] { return shared->upload(); });
}

} // namespace

g3d::GeometryPtr mesh(const Object& o, const Look& look, Vector3 offset) {
    Buffer b;
    b.add(o, look, offset);
    return geometryOf(std::move(b));
}

g3d::GeometryPtr mesh(const Model& m, const Look& look) {
    Buffer b;
    for (const Object& o : m.objects) b.add(o, look, { 0, 0, 0 });
    return geometryOf(std::move(b));
}

Vector3 base(const Object& o, const std::string& lod) {
    const BoundingBox b = o.bounds(lod);
    return { (b.min.x + b.max.x) / 2, (b.min.y + b.max.y) / 2, b.min.z };
}

namespace {

g3d::Object3D* fill(g3d::Object3D* group, const Model& m, const Look& look, const g3d::Material& material) {
    group->name = "city";
    for (const Object& o : m.objects) {
        const Geometry* g = o.at(look.lod);
        if (!g || g->faces.empty()) continue;
        const Vector3 at = base(o, look.lod);
        g3d::Material partMaterial = material;
        if (look.material) look.material(o, partMaterial);
        g3d::Object3D* child = group->add(mesh(o, look, at), partMaterial);
        child->name = o.id;
        child->position = toScene(at);
    }
    return group;
}

g3d::Material defaults(g3d::Material m) {
    m.doubleSided = true;   // many files have some faces the wrong way round
    return m;
}

} // namespace

g3d::Object3D* add(g3d::Scene& scene, const Model& m, const Look& look, g3d::Material material) {
    return fill(scene.add(), m, look, defaults(material));
}

g3d::Object3D* add(g3d::Object3D& parent, const Model& m, const Look& look, g3d::Material material) {
    return fill(parent.add(), m, look, defaults(material));
}

void rise(g3d::Object3D* group, int delay, float stagger, int length, const Vector3* from) {
    if (!group || group->children.empty()) return;
    Vector3 centre{ 0, 0, 0 };
    if (from) {
        centre = *from;
    } else {
        Vector3 lo{ 1e30f, 1e30f, 1e30f }, hi{ -1e30f, -1e30f, -1e30f };
        for (const auto& c : group->children) {
            lo = Vector3Min(lo, c->position);
            hi = Vector3Max(hi, c->position);
        }
        centre = Vector3Scale(Vector3Add(lo, hi), 0.5f);
    }
    std::vector<std::pair<float, g3d::Object3D*>> order;
    for (const auto& c : group->children) {
        const Vector3 d = Vector3Subtract(c->position, centre);
        order.push_back({ std::hypot(d.x, d.z), c.get() });
    }
    std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    length = std::max(1, length);
    for (size_t i = 0; i < order.size(); i++) {
        const float start = delay + stagger * (float)i;
        order[i].second->up = [start, length](int t, g3d::Object3D& self) {
            const float u = std::clamp((t - start) / (float)length, 0.f, 1.f);
            const float e = 1 - std::pow(1 - u, 3.f);   // ease out (cubic)
            self.visible = u > 0;
            self.scale = { 1, std::max(0.001f, e), 1 };
        };
    }
}

void frame(g3d::Camera& camera, const Model& m, float azimuth, float elevation, float margin, float aspect) {
    frame(camera, m.bounds(), azimuth, elevation, margin, aspect);
}

void frame(g3d::Camera& camera, BoundingBox local, float azimuth, float elevation, float margin, float aspect) {
    const BoundingBox b = toScene(local);
    if (Vector3Length(Vector3Subtract(b.max, b.min)) <= 0) return;   // nothing to frame
    const Vector3 centre = Vector3Scale(Vector3Add(b.min, b.max), 0.5f);
    const float radius = std::max(1.f, Vector3Length(Vector3Subtract(b.max, b.min)) / 2);
    const float vfov = camera.fov * DEG2RAD;
    const float hfov = 2 * std::atan(std::tan(vfov / 2) * aspect);
    const float dist = radius * (1 + margin) / std::sin(std::min(vfov, hfov) / 2);
    const float az = azimuth * DEG2RAD, el = elevation * DEG2RAD;
    // Azimuth clockwise from north (-z in scene axes): north = (0, 0, -1), east = (1, 0, 0).
    const Vector3 dir{ std::sin(az) * std::cos(el), std::sin(el), -std::cos(az) * std::cos(el) };
    camera.target = centre;
    camera.position = Vector3Add(centre, Vector3Scale(dir, dist));
    camera.upVector = { 0, 1, 0 };
    camera.orthoHeight = 2 * radius * (1 + margin);
    camera.farPlane = std::max(camera.farPlane, dist + radius * 4);
    camera.nearPlane = std::max(0.05f, dist / 1000);
}

// ================================================================ paths

float length(const std::vector<Vector3>& path) {
    float s = 0;
    for (size_t i = 1; i < path.size(); i++) s += Vector3Distance(path[i - 1], path[i]);
    return s;
}

Vector3 along(const std::vector<Vector3>& path, float u, float* heading) {
    if (path.empty()) return { 0, 0, 0 };
    if (path.size() == 1) {
        if (heading) *heading = 0;
        return path[0];
    }
    const float want = std::clamp(u, 0.f, 1.f) * length(path);
    float s = 0;
    for (size_t i = 1; i < path.size(); i++) {
        const float d = Vector3Distance(path[i - 1], path[i]);
        if (d <= 0) continue;
        if (s + d >= want || i + 1 == path.size()) {
            const Vector3 dir = Vector3Subtract(path[i], path[i - 1]);
            if (heading) *heading = std::fmod(std::atan2(dir.x, dir.y) * RAD2DEG + 360.f, 360.f);
            return Vector3Lerp(path[i - 1], path[i], std::clamp((want - s) / d, 0.f, 1.f));
        }
        s += d;
    }
    return path.back();
}

namespace {

g3d::Object3D* fillTrail(g3d::Object3D* group, const std::vector<Vector3>& path, std::function<float(int)> reveal,
                         const TrailStyle& st) {
    group->name = "trail";
    if (path.size() < 2 || !reveal) return group;
    auto unit = g3d::Geometry::box(1, 1, 1);
    g3d::Material m;
    m.color = st.color;
    m.unlit = st.unlit;
    m.castShadow = false;
    const float total = std::max(1e-6f, length(path));
    auto shared = std::make_shared<std::function<float(int)>>(std::move(reveal));
    float s = 0;
    for (size_t i = 1; i < path.size(); i++) {
        const Vector3 a = path[i - 1], b = path[i];
        const float d = Vector3Distance(a, b);
        if (d <= 0) continue;
        const Vector3 dir = Vector3Scale(Vector3Subtract(b, a), 1 / d);
        const float yaw = std::atan2(dir.x, -dir.y) * RAD2DEG;   // local +z along the segment, in scene axes
        const float d0 = s, d1 = s + d;
        g3d::Object3D* seg = group->add(unit, m);
        seg->up = [shared, a, dir, d0, d1, total, st, yaw](int t, g3d::Object3D& self) {
            const float r = std::clamp((*shared)(t), 0.f, 1.f) * total;
            const float len = std::min(r, d1) - d0;
            self.visible = len > 1e-3f;
            if (!self.visible) return;
            Vector3 mid = Vector3Add(a, Vector3Scale(dir, len / 2));
            mid.z += st.lift + st.thickness / 2;
            self.position = toScene(mid);
            self.rotation = { 0, yaw, 0 };
            self.scale = { st.width, st.thickness, len };
        };
        if (i + 1 < path.size()) {   // a square joint fills the corner once the trail reaches it
            g3d::Object3D* joint = group->add(unit, m);
            Vector3 at = b;
            at.z += st.lift + st.thickness / 2;
            joint->position = toScene(at);
            joint->scale = { st.width, st.thickness, st.width };
            joint->rotation = { 0, yaw, 0 };
            joint->up = [shared, d1, total](int t, g3d::Object3D& self) { self.visible = std::clamp((*shared)(t), 0.f, 1.f) * total >= d1; };
        }
        s = d1;
    }
    return group;
}

} // namespace

g3d::Object3D* trail(g3d::Scene& scene, const std::vector<Vector3>& path, std::function<float(int)> reveal, const TrailStyle& style) {
    return fillTrail(scene.add(), path, std::move(reveal), style);
}

g3d::Object3D* trail(g3d::Object3D& parent, const std::vector<Vector3>& path, std::function<float(int)> reveal, const TrailStyle& style) {
    return fillTrail(parent.add(), path, std::move(reveal), style);
}

} // namespace city
} // namespace rmtn
