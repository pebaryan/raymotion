// space_geometry.cpp -- space::hyperbolic (the Poincare disk and the upper half-plane) and
// space::sphere (great circles, spherical angles, stereographic projection).
#include "rmtn/space.hpp"
#include "plot_util.hpp"
#include <algorithm>
#include <cmath>
#include <map>

namespace rmtn {
namespace space {
namespace hyperbolic {
namespace {

constexpr double kPi = 3.14159265358979323846;
const C I{ 0, 1 };

// The circle through three points (centre).
C circumcentre(C a, C b, C c) {
    const C ab = b - a, ac = c - a;
    const double d = 2 * (ab.real() * ac.imag() - ab.imag() * ac.real());
    const double ab2 = std::norm(ab), ac2 = std::norm(ac);
    return a + C((ac.imag() * ab2 - ab.imag() * ac2) / d, (ab.real() * ac2 - ac.real() * ab2) / d);
}

// The geodesic through a and b: a diameter (straight) or a circle orthogonal to the boundary.
struct Geo {
    bool straight = false;
    C dir{ 1, 0 };                    // straight: unit direction
    C centre{ 0, 0 };
    double radius = 0;
};
Geo through(C a, C b) {
    Geo g;
    const double cr = a.real() * b.imag() - a.imag() * b.real();
    if (std::fabs(cr) < 1e-10 * std::max(1.0, std::abs(a) * std::abs(b))) {
        g.straight = true;
        const C d = std::abs(a) > std::abs(b) ? a : b;
        g.dir = std::abs(d) > 0 ? d / std::abs(d) : C(1, 0);
        return g;
    }
    const C x = std::abs(a) > std::abs(b) ? a : b;
    const C inv = x / std::norm(x);   // the inverse point lies on the same orthogonal circle
    g.centre = circumcentre(a, b, inv);
    g.radius = std::abs(a - g.centre);
    return g;
}

std::vector<C> arcAround(C centre, double radius, C from, C to, int samples) {
    const double a0 = std::arg(from - centre);
    double da = std::arg(to - centre) - a0;
    while (da > kPi) da -= 2 * kPi;
    while (da < -kPi) da += 2 * kPi;
    std::vector<C> out;
    for (int k = 0; k <= samples; k++) out.push_back(centre + std::polar(radius, a0 + da * k / samples));
    return out;
}

} // namespace

C toHalfPlane(C z) {
    if (std::abs(1.0 - z) < 1e-12) z = C(1 - 1e-12, 0);
    return I * (1.0 + z) / (1.0 - z);
}
C toDisk(C w) { return (w - I) / (w + I); }

double distance(C a, C b) {
    const double t = std::abs((a - b) / (1.0 - std::conj(a) * b));
    return 2 * std::atanh(std::min(t, 1 - 1e-15));
}

C isometry(C z, C a, double degrees) {
    return std::polar(1.0, degrees * kPi / 180) * (z - a) / (1.0 - std::conj(a) * z);
}

std::vector<C> geodesic(C a, C b, int samples) {
    samples = std::max(samples, 1);
    const Geo g = through(a, b);
    if (g.straight) {
        std::vector<C> out;
        for (int k = 0; k <= samples; k++) out.push_back(a + (b - a) * ((double)k / samples));
        return out;
    }
    return arcAround(g.centre, g.radius, a, b, samples);
}

std::vector<C> line(C a, C b, int samples) {
    const Geo g = through(a, b);
    if (g.straight) return geodesic(-g.dir, g.dir, samples);
    const double gamma = std::arg(g.centre), beta = std::acos(std::min(1.0, 1 / std::abs(g.centre)));
    return arcAround(g.centre, g.radius, std::polar(1.0, gamma - beta), std::polar(1.0, gamma + beta), samples);
}

std::vector<C> polygon(const std::vector<C>& v, int samplesPerEdge) {
    std::vector<C> out;
    for (size_t i = 0; i < v.size(); i++) {
        std::vector<C> e = geodesic(v[i], v[(i + 1) % v.size()], samplesPerEdge);
        out.insert(out.end(), e.begin(), e.end() - 1);
    }
    return out;
}

std::vector<C> circle(C c, double r, int samples) {
    const double R = std::tanh(r / 2);
    std::vector<C> out;
    for (int k = 0; k < samples; k++) {
        const C z = std::polar(R, 2 * kPi * k / samples);
        out.push_back((z + c) / (1.0 + std::conj(c) * z));
    }
    return out;
}

C reflect(C z, C a, C b) {
    const Geo g = through(a, b);
    if (g.straight) return g.dir * g.dir * std::conj(z);
    return g.centre + g.radius * g.radius / std::conj(z - g.centre);
}

std::vector<Tile> tiling(int p, int q, int maxTiles, double minSize) {
    std::vector<Tile> out;
    if (p < 3 || q < 3 || 1.0 / p + 1.0 / q >= 0.5) return out;   // not hyperbolic
    // centre to vertex: cosh R = cot(pi/p) cot(pi/q); in the disk, tanh(R/2)
    const double R = std::acosh(1 / (std::tan(kPi / p) * std::tan(kPi / q))), r = std::tanh(R / 2);
    Tile first;
    for (int k = 0; k < p; k++) first.vertices.push_back(std::polar(r, kPi / 2 + kPi / p + 2 * kPi * k / p));
    first.center = 0;
    std::map<std::pair<long long, long long>, bool> seen;
    auto key = [](C c) { return std::make_pair(std::llround(c.real() * 1e7), std::llround(c.imag() * 1e7)); };
    seen[key(first.center)] = true;
    out.push_back(first);
    for (size_t i = 0; i < out.size() && (int)out.size() < maxTiles; i++) {
        const Tile t = out[i];
        for (int k = 0; k < p && (int)out.size() < maxTiles; k++) {
            const C a = t.vertices[(size_t)k], b = t.vertices[(size_t)(k + 1) % p];
            Tile n;
            n.center = reflect(t.center, a, b);
            if (seen.count(key(n.center))) continue;
            seen[key(n.center)] = true;
            double size = 0;
            for (C v : t.vertices) {
                const C w = reflect(v, a, b);
                n.vertices.push_back(w);
                size = std::max(size, std::abs(w - n.center));
            }
            if (size < minSize) continue;
            n.parity = t.parity ^ 1;
            n.ring = t.ring + 1;
            out.push_back(n);
        }
    }
    return out;
}

vg::Path path(const std::vector<C>& pts, const std::function<Vector2(C)>& map, bool closed, Rectangle clip) {
    std::vector<Vector2> s;
    for (C z : pts) {
        Vector2 v = map(z);
        if (!std::isfinite(v.x) || !std::isfinite(v.y)) continue;
        v.x = std::clamp(v.x, -1e6f, 1e6f);
        v.y = std::clamp(v.y, -1e6f, 1e6f);
        s.push_back(v);
    }
    vg::Path out;
    if (s.size() < 2) return out;
    const bool clipping = clip.width > 0 && clip.height > 0;
    if (!clipping) return vg::Path::polyline(s, closed);
    if (closed) {
        // Sutherland-Hodgman against the four edges
        auto edge = [&](std::vector<Vector2> in, int side) {
            std::vector<Vector2> res;
            auto inside = [&](Vector2 v) {
                switch (side) {
                case 0: return v.x >= clip.x;
                case 1: return v.x <= clip.x + clip.width;
                case 2: return v.y >= clip.y;
                default: return v.y <= clip.y + clip.height;
                }
            };
            auto cut = [&](Vector2 a, Vector2 b) {
                float t;
                switch (side) {
                case 0: t = (clip.x - a.x) / (b.x - a.x); break;
                case 1: t = (clip.x + clip.width - a.x) / (b.x - a.x); break;
                case 2: t = (clip.y - a.y) / (b.y - a.y); break;
                default: t = (clip.y + clip.height - a.y) / (b.y - a.y); break;
                }
                return Vector2{ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
            };
            for (size_t i = 0; i < in.size(); i++) {
                const Vector2 a = in[i], b = in[(i + 1) % in.size()];
                if (inside(a)) res.push_back(a);
                if (inside(a) != inside(b)) res.push_back(cut(a, b));
            }
            return res;
        };
        for (int side = 0; side < 4 && s.size() >= 3; side++) s = edge(s, side);
        if (s.size() >= 3) out = vg::Path::polyline(s, true);
        return out;
    }
    bool open = false;
    for (size_t i = 1; i < s.size(); i++) {
        Vector2 a = s[i - 1], b = s[i];
        if (!plot::detail::clipSegment(a, b, clip)) { open = false; continue; }
        if (!open) { out.moveTo(a); open = true; }
        out.lineTo(b);
        if (b.x != s[i].x || b.y != s[i].y) open = false;
    }
    return out;
}

} // namespace hyperbolic

namespace sphere {
namespace {
Vector3 add(Vector3 a, Vector3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vector3 mul(Vector3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
float dot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vector3 cross(Vector3 a, Vector3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
Vector3 unit(Vector3 a) { const float l = std::sqrt(dot(a, a)); return l > 1e-9f ? mul(a, 1 / l) : a; }
} // namespace

std::vector<Vector3> arc(Vector3 a, Vector3 b, float radius, int samples) {
    a = unit(a);
    b = unit(b);
    const float om = std::acos(std::clamp(dot(a, b), -1.f, 1.f));
    std::vector<Vector3> out;
    for (int k = 0; k <= samples; k++) {
        const float t = (float)k / samples;
        Vector3 p;
        if (om < 1e-5f) p = add(mul(a, 1 - t), mul(b, t));
        else p = add(mul(a, std::sin((1 - t) * om) / std::sin(om)), mul(b, std::sin(t * om) / std::sin(om)));
        out.push_back(mul(unit(p), radius));
    }
    return out;
}

std::vector<Vector3> circle(Vector3 n, float radius, float latitude, int samples) {
    n = unit(n);
    const Vector3 ref = std::fabs(n.z) < 0.9f ? Vector3{ 0, 0, 1 } : Vector3{ 1, 0, 0 };
    const Vector3 e1 = unit(cross(ref, n)), e2 = cross(n, e1);
    const float ph = latitude * 0.017453292519943295f;
    std::vector<Vector3> out;
    for (int k = 0; k < samples; k++) {
        const float t = 6.2831853f * k / samples;
        out.push_back(mul(add(mul(add(mul(e1, std::cos(t)), mul(e2, std::sin(t))), std::cos(ph)), mul(n, std::sin(ph))), radius));
    }
    return out;
}

double angle(Vector3 a, Vector3 b, Vector3 c) {
    a = unit(a); b = unit(b); c = unit(c);
    const Vector3 ta = add(a, mul(b, -dot(b, a))), tc = add(c, mul(b, -dot(b, c)));
    const double d = std::sqrt((double)dot(ta, ta) * dot(tc, tc));
    return d > 1e-12 ? std::acos(std::clamp(dot(ta, tc) / d, -1.0, 1.0)) : 0;
}

Vector3 stereographic(Vector3 p, float radius) {
    const float t = radius / std::max(radius - p.z, 1e-6f);
    return { p.x * t, p.y * t, 0 };
}

Vector3 fromPlane(Vector3 q, float radius) {
    const float s = q.x * q.x + q.y * q.y, r2 = radius * radius;
    return { 2 * r2 * q.x / (s + r2), 2 * r2 * q.y / (s + r2), radius * (s - r2) / (s + r2) };
}

} // namespace sphere
} } // rmtn::space
