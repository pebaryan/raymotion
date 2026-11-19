// flora.cpp -- vegetation: grass looks and helpers over g3d::Field.
#include "rmtn/flora.hpp"
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace rmtn {
namespace flora {

g3d::Field lawn() {
    g3d::Field f;
    f.density = 1200;
    f.height = 0.07f; f.heightVariation = 0.2f; f.width = 0.007f; f.lean = 0.12f; f.segments = 2;
    f.root = { 34, 70, 26, 255 }; f.tip = { 92, 150, 54, 255 };
    f.colorVariation = 0.2f; f.dryness = 0.03f;
    f.sway = 0.04f; f.gust = 0.08f;
    f.detailDistance = 8; f.fadeStart = 15; f.fadeEnd = 35; f.shadowDistance = 10;
    return f;
}

g3d::Field meadow() {
    g3d::Field f;                       // the Field defaults are a meadow
    f.density = 500;
    return f;
}

g3d::Field tallGrass() {
    g3d::Field f;
    f.density = 260;
    f.height = 0.8f; f.heightVariation = 0.3f; f.width = 0.014f; f.lean = 0.35f; f.segments = 5;
    f.root = { 44, 68, 26, 255 }; f.tip = { 150, 170, 72, 255 };
    f.colorVariation = 0.3f; f.dryness = 0.2f;
    f.sway = 0.18f; f.gust = 0.5f;
    f.detailDistance = 18; f.fadeStart = 40; f.fadeEnd = 80; f.shadowDistance = 22;
    return f;
}

g3d::Field dryGrass() {
    g3d::Field f;
    f.density = 360;
    f.height = 0.45f; f.heightVariation = 0.4f; f.width = 0.01f; f.lean = 0.3f;
    f.root = { 96, 88, 46, 255 }; f.tip = { 210, 188, 118, 255 }; f.dry = { 228, 208, 150, 255 };
    f.colorVariation = 0.35f; f.dryness = 0.5f;
    f.sway = 0.14f; f.gust = 0.4f;
    return f;
}

g3d::Field grass(const std::string& name) {
    if (name == "lawn") return lawn();
    if (name == "meadow") return meadow();
    if (name == "tall") return tallGrass();
    if (name == "dry") return dryGrass();
    throw std::invalid_argument("flora: no grass look \"" + name + "\" (lawn, meadow, tall, dry)");
}

g3d::Material grassMaterial() {
    g3d::Material m;
    m.color = WHITE;
    m.roughness = 0.85f;
    m.translucency = 0.7f;
    m.translucencyColor = { 255, 240, 180, 255 };
    m.doubleSided = true;
    m.lineWidth = 0;
    return m;
}

namespace {
std::shared_ptr<g3d::Field> place(Vector2 min, Vector2 max, g3d::Field style, uint32_t seed) {
    style.min = { std::min(min.x, max.x), std::min(min.y, max.y) };
    style.max = { std::max(min.x, max.x), std::max(min.y, max.y) };
    style.seed = seed;
    return std::make_shared<g3d::Field>(std::move(style));
}
}  // namespace

g3d::Object3D* addGrass(g3d::Scene& scene, Vector2 min, Vector2 max, g3d::Field style, uint32_t seed) {
    return scene.add(place(min, max, std::move(style), seed), grassMaterial());
}
g3d::Object3D* addGrass(g3d::Object3D& parent, Vector2 min, Vector2 max, g3d::Field style, uint32_t seed) {
    return parent.add(place(min, max, std::move(style), seed), grassMaterial());
}

// ---- noise -----------------------------------------------------------------------------
namespace {
uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}
float lattice(int i, int j, uint32_t seed) {
    return hash32(((uint32_t)i * 73856093u) ^ ((uint32_t)j * 19349663u) ^ (seed * 83492791u)) / 4294967295.f;
}
// splitmix64: a small deterministic generator, the same on every platform.
struct Rng {
    uint64_t s;
    uint64_t next() {
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    float unit() { return (float)(next() >> 40) / 16777216.f; }    // [0, 1)
};
float smooth(float a, float b, float x) {
    if (b <= a) return x < a ? 0.f : 1.f;
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
}  // namespace

float noise(float x, float z, float scale, uint32_t seed) {
    float fx = x / std::max(scale, 1e-4f), fz = z / std::max(scale, 1e-4f);
    float sum = 0, amp = 1, norm = 0;
    for (int octave = 0; octave < 3; octave++) {
        const float flx = std::floor(fx), flz = std::floor(fz);
        const int i = (int)flx, j = (int)flz;
        float u = fx - flx, v = fz - flz;
        u = u * u * (3 - 2 * u);
        v = v * v * (3 - 2 * v);
        const float a = lattice(i, j, seed), b = lattice(i + 1, j, seed);
        const float c = lattice(i, j + 1, seed), d = lattice(i + 1, j + 1, seed);
        sum += amp * ((a * (1 - u) + b * u) * (1 - v) + (c * (1 - u) + d * u) * v);
        norm += amp;
        amp *= 0.5f;
        fx = fx * 2.03f + 17.1f;
        fz = fz * 2.03f - 9.7f;
        seed += 1013;
    }
    return sum / norm;
}

// ---- areas -----------------------------------------------------------------------------
namespace {
bool insideRing(const std::vector<Vector2>& ring, float x, float z) {
    bool in = false;
    for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const Vector2 a = ring[i], b = ring[j];
        if ((a.y > z) != (b.y > z) && x < (b.x - a.x) * (z - a.y) / (b.y - a.y) + a.x) in = !in;
    }
    return in;
}
float segmentDistance(Vector2 p, Vector2 a, Vector2 b) {
    const float dx = b.x - a.x, dz = b.y - a.y, len2 = dx * dx + dz * dz;
    const float t = len2 > 0 ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dz) / len2, 0.f, 1.f) : 0.f;
    const float ex = a.x + t * dx - p.x, ez = a.y + t * dz - p.y;
    return std::sqrt(ex * ex + ez * ez);
}
float ringDistance(const std::vector<Vector2>& ring, Vector2 p) {
    float d = 1e30f;
    for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) d = std::min(d, segmentDistance(p, ring[j], ring[i]));
    return d;
}
float stripDistance(const Strip& s, Vector2 p) {        // to the centre line
    if (s.points.size() == 1) return Vector2Distance(p, s.points[0]);
    float d = 1e30f;
    for (size_t i = 1; i < s.points.size(); i++) d = std::min(d, segmentDistance(p, s.points[i - 1], s.points[i]));
    return d;
}
}  // namespace

float Area::height(float x, float z) const {
    if (!ground) return 0;
    const float h = ground(x, z);
    return std::isfinite(h) ? h : 0.f;
}

float Area::slope(float x, float z) const {
    if (!ground) return 0;
    const float e = 0.25f;
    const float gx = (height(x + e, z) - height(x - e, z)) / (2 * e), gz = (height(x, z + e) - height(x, z - e)) / (2 * e);
    return std::atan(std::sqrt(gx * gx + gz * gz)) * RAD2DEG;
}

bool Area::inside(float x, float z) const {
    if (!(x >= min.x && x <= max.x && z >= min.y && z <= max.y)) return false;
    if (!polygons.empty()) {
        bool in = false;
        for (const auto& ring : polygons)
            if (ring.size() >= 3 && insideRing(ring, x, z)) in = !in;
        if (!in) return false;
    }
    for (const auto& ring : exclude)
        if (ring.size() >= 3 && insideRing(ring, x, z)) return false;
    const Vector2 p{ x, z };
    for (const Circle& c : clearings)
        if (Vector2Distance(p, c.centre) < c.radius) return false;
    for (const Strip& s : strips)
        if (!s.points.empty() && stripDistance(s, p) < s.width * 0.5f) return false;
    return true;
}

float Area::edgeDistance(float x, float z) const {
    const Vector2 p{ x, z };
    float d = 1e30f;
    if (polygons.empty()) d = std::min({ x - min.x, max.x - x, z - min.y, max.y - z });
    for (const auto& ring : polygons)
        if (ring.size() >= 2) d = std::min(d, ringDistance(ring, p));
    for (const auto& ring : exclude)
        if (ring.size() >= 2) d = std::min(d, ringDistance(ring, p));
    for (const Circle& c : clearings) d = std::min(d, std::fabs(Vector2Distance(p, c.centre) - c.radius));
    for (const Strip& s : strips)
        if (!s.points.empty()) d = std::min(d, std::fabs(stripDistance(s, p) - s.width * 0.5f));
    return std::max(d, 0.f);
}

float Area::weight(float x, float z) const {
    if (!inside(x, z)) return 0;
    float w = 1;
    if (density) {
        const float v = density(x, z);
        w = std::isfinite(v) ? std::clamp(v, 0.f, 1.f) : 0.f;
        if (w <= 0) return 0;
    }
    if (maxSlope < 90) {
        w *= 1 - smooth(maxSlope - 4, maxSlope, slope(x, z));
        if (w <= 0) return 0;
    }
    if (minAltitude > -1e29f || maxAltitude < 1e29f) {
        const float h = height(x, z);
        if (h < minAltitude || h > maxAltitude) return 0;
    }
    if (edge > 0) w *= smooth(0, edge, edgeDistance(x, z));
    return w;
}

Area rect(Vector2 min, Vector2 max) {
    Area a;
    a.min = { std::min(min.x, max.x), std::min(min.y, max.y) };
    a.max = { std::max(min.x, max.x), std::max(min.y, max.y) };
    return a;
}

namespace {
void fitBounds(Area& a) {
    Vector2 lo{ 1e30f, 1e30f }, hi{ -1e30f, -1e30f };
    for (const auto& ring : a.polygons)
        for (Vector2 p : ring) {
            lo = { std::min(lo.x, p.x), std::min(lo.y, p.y) };
            hi = { std::max(hi.x, p.x), std::max(hi.y, p.y) };
        }
    if (lo.x > hi.x) lo = hi = { 0, 0 };
    a.min = lo;
    a.max = hi;
}
}  // namespace

Area polygon(std::vector<Vector2> ring) {
    Area a;
    a.polygons.push_back(std::move(ring));
    fitBounds(a);
    return a;
}

Area region(const geo::Geometry& g, const std::function<Vector2(geo::LonLat)>& project) {
    Area a;
    for (const auto& poly : g.polygons)
        for (const geo::Ring& ring : poly) {
            std::vector<Vector2> out;
            out.reserve(ring.size());
            for (const geo::LonLat& ll : ring) out.push_back(project(ll));
            if (out.size() >= 3) a.polygons.push_back(std::move(out));
        }
    fitBounds(a);
    return a;
}

namespace {
// Upward faces binned on a grid in x, z, for the highest face over a point.
struct Faces {
    std::vector<std::array<Vector3, 3>> tris;
    Vector2 lo{ 0, 0 };
    float cell = 1;
    int nx = 1, nz = 1;
    std::vector<std::vector<int>> bins;
    bool top(float x, float z, float& y) const {
        const int i = (int)std::floor((x - lo.x) / cell), j = (int)std::floor((z - lo.y) / cell);
        if (i < 0 || j < 0 || i >= nx || j >= nz) return false;
        bool hit = false;
        for (int t : bins[(size_t)j * nx + i]) {
            const auto& f = tris[(size_t)t];
            const float d = (f[1].z - f[2].z) * (f[0].x - f[2].x) + (f[2].x - f[1].x) * (f[0].z - f[2].z);
            if (std::fabs(d) < 1e-12f) continue;
            const float a = ((f[1].z - f[2].z) * (x - f[2].x) + (f[2].x - f[1].x) * (z - f[2].z)) / d;
            const float b = ((f[2].z - f[0].z) * (x - f[2].x) + (f[0].x - f[2].x) * (z - f[2].z)) / d;
            const float c = 1 - a - b;
            if (a < -1e-5f || b < -1e-5f || c < -1e-5f) continue;
            const float h = a * f[0].y + b * f[1].y + c * f[2].y;
            if (!hit || h > y) y = h;
            hit = true;
        }
        return hit;
    }
};
}  // namespace

Area onMesh(const std::vector<Vector3>& triangles, float maxSlope) {
    auto faces = std::make_shared<Faces>();
    const float minUp = std::cos(std::clamp(maxSlope, 0.f, 89.9f) * DEG2RAD);
    Vector2 lo{ 1e30f, 1e30f }, hi{ -1e30f, -1e30f };
    double extent = 0;
    for (size_t i = 0; i + 2 < triangles.size(); i += 3) {
        const Vector3 a = triangles[i], b = triangles[i + 1], c = triangles[i + 2];
        const Vector3 n = Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a));
        const float len = Vector3Length(n);
        if (len < 1e-12f || std::fabs(n.y) / len < minUp) continue;    // either winding: steep or edge-on faces are left out
        faces->tris.push_back({ a, b, c });
        for (Vector3 p : { a, b, c }) {
            lo = { std::min(lo.x, p.x), std::min(lo.y, p.z) };
            hi = { std::max(hi.x, p.x), std::max(hi.y, p.z) };
        }
        extent += std::max({ std::fabs(b.x - a.x), std::fabs(c.x - a.x), std::fabs(b.z - a.z), std::fabs(c.z - a.z) });
    }
    Area area;
    if (faces->tris.empty()) {
        area.min = area.max = { 0, 0 };
        area.density = [](float, float) { return 0.f; };
        return area;
    }
    area.min = lo;
    area.max = hi;
    const float span = std::max(hi.x - lo.x, hi.y - lo.y);
    faces->cell = std::max({ (float)(extent / faces->tris.size()), span / 512.f, 1e-3f });
    faces->lo = lo;
    faces->nx = std::max(1, (int)std::ceil((hi.x - lo.x) / faces->cell) + 1);
    faces->nz = std::max(1, (int)std::ceil((hi.y - lo.y) / faces->cell) + 1);
    faces->bins.assign((size_t)faces->nx * faces->nz, {});
    for (size_t t = 0; t < faces->tris.size(); t++) {
        const auto& f = faces->tris[t];
        const float x0 = std::min({ f[0].x, f[1].x, f[2].x }), x1 = std::max({ f[0].x, f[1].x, f[2].x });
        const float z0 = std::min({ f[0].z, f[1].z, f[2].z }), z1 = std::max({ f[0].z, f[1].z, f[2].z });
        const int i0 = std::max(0, (int)std::floor((x0 - lo.x) / faces->cell)), i1 = std::min(faces->nx - 1, (int)std::floor((x1 - lo.x) / faces->cell));
        const int j0 = std::max(0, (int)std::floor((z0 - lo.y) / faces->cell)), j1 = std::min(faces->nz - 1, (int)std::floor((z1 - lo.y) / faces->cell));
        for (int j = j0; j <= j1; j++)
            for (int i = i0; i <= i1; i++) faces->bins[(size_t)j * faces->nx + i].push_back((int)t);
    }
    area.ground = [faces](float x, float z) { float y = 0; return faces->top(x, z, y) ? y : 0.f; };
    area.density = [faces](float x, float z) { float y = 0; return faces->top(x, z, y) ? 1.f : 0.f; };
    return area;
}

Area onMesh(const Mesh& mesh, Matrix transform, float maxSlope) {
    std::vector<Vector3> tris;
    if (!mesh.vertices) return onMesh(tris, maxSlope);
    auto vertex = [&](int i) {
        return Vector3Transform({ mesh.vertices[i * 3], mesh.vertices[i * 3 + 1], mesh.vertices[i * 3 + 2] }, transform);
    };
    if (mesh.indices)
        for (int i = 0; i < mesh.triangleCount * 3; i++) tris.push_back(vertex(mesh.indices[i]));
    else
        for (int i = 0; i < mesh.vertexCount; i++) tris.push_back(vertex(i));
    return onMesh(tris, maxSlope);
}

g3d::GeometryPtr terrain(const Area& area, int resX, int resZ) {
    resX = std::clamp(resX, 1, 254);
    resZ = std::clamp(resZ, 1, 254);
    const int nx = resX + 1, nz = resZ + 1;
    auto data = std::make_shared<std::vector<float>>((size_t)nx * nz * 8);   // position, normal, uv
    const float w = area.max.x - area.min.x, d = area.max.y - area.min.y;
    std::vector<float> h((size_t)nx * nz);
    for (int j = 0; j < nz; j++)
        for (int i = 0; i < nx; i++) h[(size_t)j * nx + i] = area.height(area.min.x + w * i / resX, area.min.y + d * j / resZ);
    const float dx = std::max(w / resX, 1e-6f), dz = std::max(d / resZ, 1e-6f);
    for (int j = 0; j < nz; j++)
        for (int i = 0; i < nx; i++) {
            auto at = [&](int a, int b) { return h[(size_t)std::clamp(b, 0, nz - 1) * nx + std::clamp(a, 0, nx - 1)]; };
            const float gx = (at(i + 1, j) - at(i - 1, j)) / (dx * (float)(std::min(i + 1, nx - 1) - std::max(i - 1, 0)));
            const float gz = (at(i, j + 1) - at(i, j - 1)) / (dz * (float)(std::min(j + 1, nz - 1) - std::max(j - 1, 0)));
            const Vector3 n = Vector3Normalize({ -gx, 1, -gz });
            float* v = &(*data)[((size_t)j * nx + i) * 8];
            v[0] = area.min.x + w * i / resX; v[1] = at(i, j); v[2] = area.min.y + d * j / resZ;
            v[3] = n.x; v[4] = n.y; v[5] = n.z;
            v[6] = (float)i / resX; v[7] = (float)j / resZ;
        }
    return std::make_shared<g3d::Geometry>([data, nx, nz, resX, resZ]() {
        Mesh m{};
        m.vertexCount = nx * nz;
        m.triangleCount = resX * resZ * 2;
        m.vertices = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
        m.normals = (float*)MemAlloc((unsigned)(m.vertexCount * 3 * sizeof(float)));
        m.texcoords = (float*)MemAlloc((unsigned)(m.vertexCount * 2 * sizeof(float)));
        m.indices = (unsigned short*)MemAlloc((unsigned)(m.triangleCount * 3 * sizeof(unsigned short)));
        for (int k = 0; k < m.vertexCount; k++) {
            const float* v = &(*data)[(size_t)k * 8];
            std::copy(v, v + 3, m.vertices + k * 3);
            std::copy(v + 3, v + 6, m.normals + k * 3);
            std::copy(v + 6, v + 8, m.texcoords + k * 2);
        }
        int t = 0;
        for (int j = 0; j < resZ; j++)
            for (int i = 0; i < resX; i++) {                        // both triangles face +y
                const unsigned short a = (unsigned short)(j * nx + i), b = (unsigned short)(a + 1);
                const unsigned short c = (unsigned short)(a + nx), e = (unsigned short)(c + 1);
                for (unsigned short k : { a, c, b, b, c, e }) m.indices[t++] = k;
            }
        UploadMesh(&m, false);
        return m;
    });
}

// ---- scattering ------------------------------------------------------------------------
std::vector<Placement> scatter(const Area& area, const ScatterSpec& spec, uint32_t seed) {
    std::vector<Placement> out;
    const float r = std::max(spec.spacing, 1e-3f);
    const float w = area.max.x - area.min.x, d = area.max.y - area.min.y;
    if (!(w > 0 && d > 0)) return out;
    if ((double)w * d / ((double)r * r * 0.6) > 5e6)
        throw std::invalid_argument("flora::scatter: the bounds would hold more than 5 million points; widen the spacing or shrink the area");
    // Bridson's Poisson disk over the bounds: no two points closer than r.
    const float cell = r / std::sqrt(2.f);
    const int gx = std::max(1, (int)std::ceil(w / cell)), gz = std::max(1, (int)std::ceil(d / cell));
    std::vector<int> grid((size_t)gx * gz, -1);
    std::vector<Vector2> pts;
    std::vector<int> active;
    Rng rng{ (uint64_t)seed * 0x9E3779B97F4A7C15ull + 0x1234567ull };
    auto cellOf = [&](Vector2 p) {
        return std::pair<int, int>{ std::min(gx - 1, (int)((p.x - area.min.x) / cell)), std::min(gz - 1, (int)((p.y - area.min.y) / cell)) };
    };
    auto put = [&](Vector2 p) {
        const auto [i, j] = cellOf(p);
        grid[(size_t)j * gx + i] = (int)pts.size();
        active.push_back((int)pts.size());
        pts.push_back(p);
    };
    put({ area.min.x + rng.unit() * w, area.min.y + rng.unit() * d });
    while (!active.empty()) {
        const size_t pick = (size_t)(rng.next() % active.size());
        const Vector2 p = pts[(size_t)active[pick]];
        bool placed = false;
        for (int k = 0; k < 30 && !placed; k++) {
            const float a = rng.unit() * 2 * PI, rad = r * (1 + rng.unit());
            const Vector2 q{ p.x + std::cos(a) * rad, p.y + std::sin(a) * rad };
            if (q.x < area.min.x || q.x >= area.max.x || q.y < area.min.y || q.y >= area.max.y) continue;
            const auto [ci, cj] = cellOf(q);
            bool clear = true;
            for (int j = std::max(0, cj - 2); j <= std::min(gz - 1, cj + 2) && clear; j++)
                for (int i = std::max(0, ci - 2); i <= std::min(gx - 1, ci + 2); i++) {
                    const int other = grid[(size_t)j * gx + i];
                    if (other >= 0 && Vector2Distance(pts[(size_t)other], q) < r) { clear = false; break; }
                }
            if (clear) { put(q); placed = true; }
        }
        if (!placed) { active[pick] = active.back(); active.pop_back(); }
    }
    // Each point kept with the area's weight (and the clumps), by its own number.
    const float clumping = std::clamp(spec.clumping, 0.f, 1.f);
    const float threshold = 0.2f + 0.45f * clumping;
    const int variants = std::max(1, spec.variants);
    for (size_t i = 0; i < pts.size(); i++) {
        Rng own{ ((uint64_t)seed << 32) ^ (uint64_t)hash32((uint32_t)i * 2654435761u + seed) ^ (uint64_t)i * 0xD1B54A32D192ED03ull };
        const float keep = own.unit(), yaw = own.unit(), size = own.unit(), variant = own.unit();
        const Vector2 p = pts[i];
        float wgt = area.weight(p.x, p.y);
        if (wgt > 0 && clumping > 0)
            wgt *= smooth(threshold - 0.1f, threshold + 0.1f, noise(p.x, p.y, spec.clumpScale, seed ^ 0xC1A5u));
        if (keep >= wgt) continue;
        Placement pl;
        pl.position = { p.x, area.height(p.x, p.y), p.y };
        pl.yaw = yaw * 360;
        pl.scale = spec.scaleMin + (spec.scaleMax - spec.scaleMin) * size;
        pl.variant = std::min(variants - 1, (int)(variant * variants));
        pl.id = (uint32_t)i;
        out.push_back(pl);
    }
    return out;
}

std::function<float(float, float)> arrivalFrom(Vector2 centre, float reach) {
    const float r = std::max(std::isfinite(reach) ? reach : 1.f, 1e-3f);
    return [centre, r](float x, float z) { return std::min(Vector2Distance({ x, z }, centre) / r, 1.f); };
}
std::function<float(float, float)> arrivalAlong(std::vector<Vector2> line, float reach) {
    const float r = std::max(std::isfinite(reach) ? reach : 1.f, 1e-3f);
    if (line.empty()) line.push_back({ 0, 0 });
    const Strip s{ std::move(line), 0 };
    return [s, r](float x, float z) { return std::min(stripDistance(s, { x, z }) / r, 1.f); };
}
std::function<float(float, float)> arrivalInto(Area area, float depth) {
    const float d = std::max(std::isfinite(depth) ? depth : 1.f, 1e-3f);
    return [area = std::move(area), d](float x, float z) { return std::min(area.edgeDistance(x, z) / d, 1.f); };
}

void fit(g3d::Field& field, const Area& area) {
    auto shared = std::make_shared<Area>(area);
    field.min = area.min;
    field.max = area.max;
    field.mask = [shared](float x, float z) { return shared->weight(x, z); };
    field.ground = area.ground ? std::function<float(float, float)>([shared](float x, float z) { return shared->height(x, z); })
                               : std::function<float(float, float)>();
    const float span = std::max(area.max.x - area.min.x, area.max.y - area.min.y);
    field.mapResolution = std::clamp(std::max(field.mapResolution, (int)std::ceil(span * 2)), 2, 1024);
    field.revision++;
}

g3d::Object3D* addGrass(g3d::Scene& scene, const Area& area, g3d::Field style, uint32_t seed) {
    style.seed = seed;
    fit(style, area);
    return scene.add(std::make_shared<g3d::Field>(std::move(style)), grassMaterial());
}
g3d::Object3D* addGrass(g3d::Object3D& parent, const Area& area, g3d::Field style, uint32_t seed) {
    style.seed = seed;
    fit(style, area);
    return parent.add(std::make_shared<g3d::Field>(std::move(style)), grassMaterial());
}

}  // namespace flora
}  // namespace rmtn
