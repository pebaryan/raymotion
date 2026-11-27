// Water bodies (lakes, pools, rivers): the shapes, their meshes and the CPU side of their surface.
// No GL here: the engine uploads the mesh when it first draws (scene3d.cpp).
#include "rmtn/scene3d.hpp"
#include <earcut/earcut.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace rmtn {
namespace g3d {

namespace {

// One point of a river's smoothed path.
struct Sample {
    Vector2 p;
    float at = 0;           // position along the authored path: segment index + fraction
    float arc = 0;          // metres from the start
    float width = 4, drop = 0, speed = 1, heading = 0;
};

float lerpAt(const std::vector<float>& v, float at, float fallback) {
    if (v.empty()) return fallback;
    if (v.size() == 1) return v[0];
    const float c = std::clamp(at, 0.f, (float)v.size() - 1.f);
    const size_t i = std::min((size_t)c, v.size() - 2);
    return v[i] + (v[i + 1] - v[i]) * (c - (float)i);
}

Vector2 catmull(Vector2 a, Vector2 b, Vector2 c, Vector2 d, float t) {
    const float t2 = t * t, t3 = t2 * t;
    auto f = [&](float p0, float p1, float p2, float p3) {
        return 0.5f * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
    };
    return { f(a.x, b.x, c.x, d.x), f(a.y, b.y, c.y, d.y) };
}

constexpr size_t kMaxSamples = 20000;       // two vertices each, to stay inside 16-bit indices

// The river's path smoothed and measured: positions, arc length, width, drop, speed and heading.
std::vector<Sample> smoothPath(const WaterBody& b) {
    std::vector<Sample> out;
    const size_t n = b.path.size();
    if (n < 2) return out;
    float chord = 0;
    for (size_t i = 0; i + 1 < n; i++) chord += std::hypot(b.path[i + 1].x - b.path[i].x, b.path[i + 1].y - b.path[i].y);
    const float step = std::max(1.5f, chord / (float)kMaxSamples * 1.5f);
    for (size_t i = 0; i + 1 < n; i++) {
        const Vector2 p0 = b.path[i > 0 ? i - 1 : 0], p1 = b.path[i], p2 = b.path[i + 1], p3 = b.path[std::min(i + 2, n - 1)];
        const float len = std::hypot(p2.x - p1.x, p2.y - p1.y);
        const int k = std::max(1, (int)std::ceil(len / step));
        for (int j = 0; j < k; j++) {
            Sample s;
            s.at = (float)i + (float)j / (float)k;
            s.p = catmull(p0, p1, p2, p3, (float)j / (float)k);
            out.push_back(s);
        }
    }
    Sample last;
    last.at = (float)n - 1;
    last.p = b.path.back();
    out.push_back(last);
    float meanW = 0;
    for (size_t i = 0; i < n; i++) meanW += lerpAt(b.width, (float)i, 4.f);
    meanW /= (float)n;
    float prevHeading = 0;
    for (size_t i = 0; i < out.size(); i++) {
        Sample& s = out[i];
        if (i > 0) s.arc = out[i - 1].arc + std::hypot(s.p.x - out[i - 1].p.x, s.p.y - out[i - 1].p.y);
        const Vector2 a = out[i > 0 ? i - 1 : 0].p, c = out[std::min(i + 1, out.size() - 1)].p;
        float h = std::atan2(c.y - a.y, c.x - a.x);
        if (i > 0) h += 2 * PI * std::round((prevHeading - h) / (2 * PI));      // continuous: no jump at +-pi
        s.heading = prevHeading = h;
        s.width = std::max(lerpAt(b.width, s.at, 4.f), 0.05f);
        s.drop = b.levels.size() >= 2 && b.levels.size() == n ? lerpAt(b.levels, s.at, b.level) - b.level : -b.fall * s.arc;
        s.speed = b.flowSpeed * (b.narrowFaster ? std::clamp(meanW / s.width, 0.4f, 2.5f) : 1.f);
    }
    return out;
}

bool inRing(const std::vector<Vector2>& r, float x, float z) {
    bool in = false;
    for (size_t i = 0, j = r.size() - 1; i < r.size(); j = i++)
        if ((r[i].y > z) != (r[j].y > z) && x < (r[j].x - r[i].x) * (z - r[i].y) / (r[j].y - r[i].y) + r[i].x) in = !in;
    return in;
}

} // namespace

WaterBody WaterBody::lake(std::vector<Vector2> outline, float level) {
    WaterBody b;
    b.outline = std::move(outline);
    b.level = level;
    b.look = Water::calm();
    b.look.reflections = Water::Reflections::Scene;
    b.look.refraction = true;
    b.look.caustics = 0.5f;
    return b;
}

WaterBody WaterBody::river(std::vector<Vector2> path, float width, float level, float flowSpeed) {
    WaterBody b;
    b.path = std::move(path);
    b.width = { width };
    b.level = level;
    b.flowSpeed = flowSpeed;
    b.look = Water::calm();
    b.look.waves.clear();               // the flow's ripples are its detail
    b.look.reflections = Water::Reflections::Scene;
    b.look.refraction = true;
    b.look.caustics = 0.4f;
    b.look.foam = 0;
    return b;
}

Vector4 WaterBody::extent() const {
    float x0 = 1e30f, z0 = 1e30f, x1 = -1e30f, z1 = -1e30f;
    auto add = [&](Vector2 p, float pad) {
        x0 = std::min(x0, p.x - pad); x1 = std::max(x1, p.x + pad);
        z0 = std::min(z0, p.y - pad); z1 = std::max(z1, p.y + pad);
    };
    if (isRiver()) {
        float w = 0;
        for (float v : width) w = std::max(w, v);
        if (width.empty()) w = 4;
        for (const Vector2& p : path) add(p, w * 0.5f);
    } else {
        for (const Vector2& p : outline) add(p, 0);
    }
    if (x0 > x1) return { 0, 0, 0, 0 };
    return { x0, z0, x1, z1 };
}

float WaterBody::levelAt(float x, float z) const {
    if (!isRiver() || (fall == 0 && levels.empty())) return level;
    const std::vector<Sample> s = smoothPath(*this);
    float best = 1e30f, drop = 0;
    for (const Sample& q : s) {
        const float d = (q.p.x - x) * (q.p.x - x) + (q.p.y - z) * (q.p.y - z);
        if (d < best) { best = d; drop = q.drop; }
    }
    return level + drop;
}

namespace {
Water bodyWater(const WaterBody& body, const Weather& weather) {
    Water w = body.look;
    w.enabled = true;
    w.level = body.level;
    const Vector4 e = body.extent();
    w.fetch = body.fetch > 0 ? body.fetch : std::max(std::max(e.z - e.x, e.w - e.y), 1.f);
    return inWeather(w, weather);
}
} // namespace

float WaterBody::height(float x, float z, double seconds, const Weather* weather) const {
    Water w = bodyWater(*this, weather ? *weather : Weather{});
    w.level = levelAt(x, z);
    return w.height(x, z, seconds);
}

Water waterOf(const Scene& scene, const WaterBody& body) { return bodyWater(body, scene.weather); }

bool WaterBody::contains(float x, float z) const {
    if (isRiver()) {
        for (const Sample& s : smoothPath(*this))
            if (std::hypot(s.p.x - x, s.p.y - z) <= s.width * 0.5f) return true;
        return false;
    }
    if (outline.size() < 3 || !inRing(outline, x, z)) return false;
    for (const auto& h : holes)
        if (h.size() >= 3 && inRing(h, x, z)) return false;
    return true;
}

float WaterBody::speedAt(float arc) const {
    const std::vector<Sample> s = smoothPath(*this);
    if (s.empty()) return flowSpeed;
    for (const Sample& q : s)
        if (q.arc >= arc) return q.speed;
    return s.back().speed;
}

WaterBodyMesh waterBodyMesh(const WaterBody& b) {
    WaterBodyMesh m;
    if (b.isRiver()) {
        const std::vector<Sample> s = smoothPath(b);
        for (const Sample& q : s) {
            const Vector2 n{ -std::sin(q.heading), std::cos(q.heading) };      // to u = 1
            for (int side = 0; side < 2; side++) {
                const float o = (side ? 0.5f : -0.5f) * q.width;
                m.position.insert(m.position.end(), { q.p.x + n.x * o, q.drop, q.p.y + n.y * o });
                m.normal.insert(m.normal.end(), { q.speed, q.heading, q.width });
                m.uv.insert(m.uv.end(), { (float)side, q.arc });
            }
        }
        for (size_t i = 0; i + 1 < s.size(); i++) {
            const unsigned short a = (unsigned short)(i * 2), c = (unsigned short)(i * 2 + 2);
            m.index.insert(m.index.end(), { a, (unsigned short)(a + 1), c, (unsigned short)(a + 1), (unsigned short)(c + 1), c });
        }
        return m;
    }
    if (b.outline.size() < 3) return m;
    // Earcut works about a local origin: world coordinates in the hundreds of thousands lose precision.
    const Vector4 e = b.extent();
    const double cx = 0.5 * (e.x + e.z), cz = 0.5 * (e.y + e.w);
    std::vector<std::vector<std::array<double, 2>>> poly;
    std::vector<Vector2> flat;
    auto ring = [&](const std::vector<Vector2>& r) {
        std::vector<std::array<double, 2>> p;
        for (const Vector2& v : r) {
            p.push_back({ (double)v.x - cx, (double)v.y - cz });
            flat.push_back(v);
        }
        poly.push_back(std::move(p));
    };
    ring(b.outline);
    for (const auto& h : b.holes)
        if (h.size() >= 3) ring(h);
    if (flat.size() > 60000) return m;
    const std::vector<uint32_t> idx = mapbox::earcut<uint32_t>(poly);
    for (const Vector2& v : flat) {
        m.position.insert(m.position.end(), { v.x, 0.f, v.y });
        m.normal.insert(m.normal.end(), { 0.f, 0.f, 0.f });
        m.uv.insert(m.uv.end(), { v.x, v.y });
    }
    for (uint32_t i : idx) m.index.push_back((unsigned short)i);
    return m;
}

uint64_t waterBodyKey(const WaterBody& b) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](const void* p, size_t n) {
        const unsigned char* c = (const unsigned char*)p;
        for (size_t i = 0; i < n; i++) { h ^= c[i]; h *= 1099511628211ull; }
    };
    auto f = [&](float v) { mix(&v, sizeof v); };
    auto ring = [&](const std::vector<Vector2>& r) { f((float)r.size()); for (const Vector2& p : r) { f(p.x); f(p.y); } };
    ring(b.outline);
    for (const auto& r : b.holes) ring(r);
    f(-1.f);
    ring(b.path);
    for (float v : b.width) f(v);
    f(-2.f);
    for (float v : b.levels) f(v);
    f(b.level); f(b.fall); f(b.flowSpeed); f(b.narrowFaster ? 1.f : 0.f);
    return h;
}

} // namespace g3d
} // namespace rmtn
