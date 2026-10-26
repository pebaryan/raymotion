// globe.cpp -- globe::, the Earth in g3d: positions on the sphere, sphere and patch meshes
// with equirectangular texture coordinates, CPU-drawn maps and fields, procedural clouds
// with storms, and routes along great circles.
#include "rmtn/globe.hpp"
#include <raymath.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <memory>

namespace rmtn {
namespace globe {

namespace {

constexpr double kPi = 3.14159265358979323846, RAD = kPi / 180, DEG = 180 / kPi, EARTH_KM = 6371.0;

double spanOf(const geo::Bounds& b) { return b.east > b.west ? b.east - b.west : b.east + 360 - b.west; }
bool wholeWorld(const geo::Bounds& b) { return spanOf(b) >= 359.999 && b.south <= -89.999 && b.north >= 89.999; }

float smooth(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}

Color lerpColor(Color a, Color b, float t) {
    auto m = [t](unsigned char x, unsigned char y) { return (unsigned char)std::lround(x + (y - x) * t); };
    return { m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a) };
}

// `top` over `bottom`, straight alpha.
Color composite(Color top, float ta, Color bottom) {
    const float a = std::clamp(ta, 0.f, 1.f), ba = bottom.a / 255.f, oa = a + ba * (1 - a);
    if (oa <= 0) return BLANK;
    auto ch = [&](unsigned char t, unsigned char b) { return (unsigned char)std::lround((t * a + b * ba * (1 - a)) / oa); };
    return { ch(top.r, bottom.r), ch(top.g, bottom.g), ch(top.b, bottom.b), (unsigned char)std::lround(oa * 255) };
}

// Rows and columns of an equirectangular grid over a box.
struct Grid {
    int w, h;
    geo::Bounds b;
    double span() const { return spanOf(b); }
    double lonAt(double x) const { return b.west + x / w * span(); }
    double latAt(double y) const { return b.north - y / h * (b.north - b.south); }
    double xOf(double lon) const {
        double d = lon - b.west;
        if (!wholeWorld(b)) { while (d < -180) d += 360; while (d > span() + 180) d -= 360; }
        return d / span() * w;
    }
    double yOf(double lat) const { return (b.north - lat) / (b.north - b.south) * h; }
    bool wraps() const { return wholeWorld(b); }
};

int heightFor(int w, const geo::Bounds& b) { return std::max(1, (int)std::lround(w * (b.north - b.south) / spanOf(b))); }

// ---- rasterising on the grid: coverage 0..1 per pixel

// Nonzero-rule polygon fill, 4 samples down each pixel and exact coverage across.
void fillCoverage(const Grid& g, const geo::Geometry& geom, std::vector<float>& cov) {
    struct Edge { double x0, y0, x1, y1; int dir; };
    std::vector<Edge> edges;
    const double W = g.wraps() ? g.w : 0;
    auto addEdge = [&](double x0, double y0, double x1, double y1) {
        if (y0 == y1) return;
        for (int k = -1; k <= 1; k++) {
            if (k != 0 && W == 0) continue;
            edges.push_back({ x0 + k * W, y0, x1 + k * W, y1, y1 > y0 ? 1 : -1 });
        }
    };
    for (const auto& poly : geom.polygons)
        for (const geo::Ring& ring : poly) {
            if (ring.size() < 3) continue;
            // Unwrap so each step is the short way round; a ring that circles a pole comes back
            // 360 degrees off, and is closed along that pole back to where it began.
            std::vector<std::pair<double, double>> pts;
            double lon = ring[0].lon;
            for (size_t i = 0; i < ring.size(); i++) {
                if (i) {
                    double d = ring[i].lon - ring[i - 1].lon;
                    while (d > 180) d -= 360;
                    while (d < -180) d += 360;
                    lon += d;
                }
                pts.push_back({ lon, ring[i].lat });
            }
            double back = ring[0].lon - ring.back().lon;
            while (back > 180) back -= 360;
            while (back < -180) back += 360;
            const double net = lon + back - ring[0].lon;
            if (std::fabs(net) > 180) {
                double mean = 0;
                for (const auto& p : pts) mean += p.second;
                const double pole = mean >= 0 ? 90 : -90;
                pts.push_back({ pts.back().first, pole });
                pts.push_back({ pts.front().first, pole });
            }
            for (size_t i = 0; i < pts.size(); i++) {
                const auto& a = pts[i];
                const auto& b = pts[(i + 1) % pts.size()];
                const double ax = (a.first - g.b.west) / g.span() * g.w, bx = (b.first - g.b.west) / g.span() * g.w;
                addEdge(ax, g.yOf(a.second), bx, g.yOf(b.second));
            }
        }
    if (edges.empty()) return;
    // Bucket the edges by row.
    std::vector<std::vector<int>> rows(g.h);
    for (size_t i = 0; i < edges.size(); i++) {
        const Edge& e = edges[i];
        const int r0 = std::max(0, (int)std::floor(std::min(e.y0, e.y1))), r1 = std::min(g.h - 1, (int)std::floor(std::max(e.y0, e.y1)));
        for (int r = r0; r <= r1; r++) rows[r].push_back((int)i);
    }
    constexpr int SS = 4;
    std::vector<std::pair<double, int>> xs;
    for (int r = 0; r < g.h; r++) {
        if (rows[r].empty()) continue;
        float* line = &cov[(size_t)r * g.w];
        for (int s = 0; s < SS; s++) {
            const double y = r + (s + 0.5) / SS;
            xs.clear();
            for (int i : rows[r]) {
                const Edge& e = edges[i];
                if ((e.y0 <= y && y < e.y1) || (e.y1 <= y && y < e.y0))
                    xs.push_back({ e.x0 + (y - e.y0) / (e.y1 - e.y0) * (e.x1 - e.x0), e.dir });
            }
            std::sort(xs.begin(), xs.end());
            int wind = 0;
            for (size_t i = 0; i + 1 < xs.size(); i++) {
                wind += xs[i].second;
                if (wind == 0) continue;
                const double xa = std::max(0.0, xs[i].first), xb = std::min((double)g.w, xs[i + 1].first);
                for (int x = std::max(0, (int)std::floor(xa)); x < g.w && x < xb; x++) {
                    const double c = std::min(xb, x + 1.0) - std::max(xa, (double)x);
                    if (c > 0) line[x] += (float)(c / SS);
                }
            }
        }
    }
    for (float& c : cov) c = std::min(c, 1.f);
}

// An antialiased segment `r` px from its centre line (horizontal px stretched by 1/cos(lat),
// so it is as wide on the globe everywhere), max-combined into `cov`.
void segmentCoverage(const Grid& g, double ax, double ay, double bx, double by, double r, std::vector<float>& cov) {
    const int y0 = std::max(0, (int)std::floor(std::min(ay, by) - r - 1)), y1 = std::min(g.h - 1, (int)std::ceil(std::max(ay, by) + r + 1));
    for (int y = y0; y <= y1; y++) {
        const double py = y + 0.5;
        const double c = std::max(0.02, std::cos(g.latAt(py) * RAD));
        const double rx = std::min(g.w / 2.0, (r + 1) / c);
        const int x0 = (int)std::floor(std::min(ax, bx) - rx), x1 = (int)std::ceil(std::max(ax, bx) + rx);
        for (int xx = x0; xx <= x1; xx++) {
            int x = xx;
            if (g.wraps()) x = ((x % g.w) + g.w) % g.w;
            else if (x < 0 || x >= g.w) continue;
            const double px = xx + 0.5;
            // distance in the stretched metric
            const double dx = (bx - ax) * c, dy = by - ay, qx = (px - ax) * c, qy = py - ay;
            const double len2 = dx * dx + dy * dy;
            const double u = len2 > 0 ? std::clamp((qx * dx + qy * dy) / len2, 0.0, 1.0) : 0;
            const double ex = qx - u * dx, ey = qy - u * dy, d = std::sqrt(ex * ex + ey * ey);
            const float a = (float)std::clamp(r - d + 0.5, 0.0, 1.0);
            float& cv = cov[(size_t)y * g.w + x];
            cv = std::max(cv, a);
        }
    }
}

void strokeCoverage(const Grid& g, const geo::Geometry& geom, double r, std::vector<float>& cov) {
    auto line = [&](const std::vector<LonLat>& pts, bool closed) {
        const size_t n = pts.size();
        if (n < 2) return;
        for (size_t i = 0; i + (closed ? 0 : 1) < n; i++) {
            const LonLat a = pts[i], b = pts[(i + 1) % n];
            double d = b.lon - a.lon;
            while (d > 180) d -= 360;
            while (d < -180) d += 360;
            const double ax = g.xOf(a.lon), bx = ax + d / g.span() * g.w;
            segmentCoverage(g, ax, g.yOf(a.lat), bx, g.yOf(b.lat), r, cov);
        }
    };
    for (const auto& poly : geom.polygons)
        for (const geo::Ring& ring : poly) line(ring, true);
    for (const auto& l : geom.lines) line(l, false);
}

// ---- noise for clouds: Perlin's improved gradient noise in 3D

struct Noise {
    int p[512];
    explicit Noise(unsigned seed) {
        int q[256];
        for (int i = 0; i < 256; i++) q[i] = i;
        unsigned s = seed * 2654435761u + 1;
        for (int i = 255; i > 0; i--) {
            s = s * 1664525u + 1013904223u;
            std::swap(q[i], q[(s >> 8) % (unsigned)(i + 1)]);
        }
        for (int i = 0; i < 512; i++) p[i] = q[i & 255];
    }
    static float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    static float grad(int h, float x, float y, float z) {
        const int k = h & 15;
        const float u = k < 8 ? x : y, v = k < 4 ? y : (k == 12 || k == 14 ? x : z);
        return ((k & 1) ? -u : u) + ((k & 2) ? -v : v);
    }
    float operator()(float x, float y, float z) const {
        const float fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
        const int X = (int)fx & 255, Y = (int)fy & 255, Z = (int)fz & 255;
        x -= fx; y -= fy; z -= fz;
        const float u = fade(x), v = fade(y), w = fade(z);
        const int A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z, B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;
        auto L = [](float t, float a, float b) { return a + t * (b - a); };
        return L(w, L(v, L(u, grad(p[AA], x, y, z), grad(p[BA], x - 1, y, z)), L(u, grad(p[AB], x, y - 1, z), grad(p[BB], x - 1, y - 1, z))),
                 L(v, L(u, grad(p[AA + 1], x, y, z - 1), grad(p[BA + 1], x - 1, y, z - 1)),
                   L(u, grad(p[AB + 1], x, y - 1, z - 1), grad(p[BB + 1], x - 1, y - 1, z - 1))));
    }
    float fbm(Vector3 q, int octaves = 5) const {
        float sum = 0, amp = 0.5f;
        for (int i = 0; i < octaves; i++) {
            sum += amp * (*this)(q.x, q.y, q.z);
            q = Vector3Scale(q, 2.03f);
            q = { q.x + 17.1f, q.y - 9.3f, q.z + 4.7f };
            amp *= 0.5f;
        }
        return sum;
    }
};

const Noise& noiseFor(unsigned seed) {
    static thread_local std::unique_ptr<Noise> cached;
    static thread_local unsigned cachedSeed = ~0u;
    if (!cached || cachedSeed != seed) { cached = std::make_unique<Noise>(seed); cachedSeed = seed; }
    return *cached;
}

float defaultWind(float lat) {
    const float a = std::fabs(lat);
    return -3.f * std::exp(-(lat / 18) * (lat / 18)) + 4.5f * std::exp(-((a - 45) / 14) * ((a - 45) / 14)) -
           1.5f * std::exp(-((a - 75) / 10) * ((a - 75) / 10));
}

// More cloud along the equator and the storm tracks, less over the subtropics.
float coverBias(float lat) {
    const float a = std::fabs(lat);
    return 0.12f * std::exp(-((lat - 6) / 8) * ((lat - 6) / 8)) - 0.12f * std::exp(-((a - 25) / 9) * ((a - 25) / 9)) +
           0.08f * std::exp(-((a - 55) / 12) * ((a - 55) / 12));
}

Vector3 rotateAbout(Vector3 v, Vector3 axis, float angle) {   // Rodrigues
    const float c = std::cos(angle), s = std::sin(angle);
    return Vector3Add(Vector3Add(Vector3Scale(v, c), Vector3Scale(Vector3CrossProduct(axis, v), s)),
                      Vector3Scale(axis, Vector3DotProduct(axis, v) * (1 - c)));
}

Mesh uploadGrid(int nx, int ny, const std::function<void(int i, int j, Vector3& pos, Vector3& normal, Vector2& uv)>& at) {
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
            Vector3 p, n;
            Vector2 uv;
            at(i, j, p, n, uv);
            std::memcpy(&m.vertices[k * 3], &p, sizeof p);
            std::memcpy(&m.normals[k * 3], &n, sizeof n);
            m.texcoords[k * 2] = uv.x;
            m.texcoords[k * 2 + 1] = uv.y;
        }
    int t = 0;
    for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx; i++) {
            const unsigned short a = (unsigned short)(j * (nx + 1) + i), b = (unsigned short)(a + 1),
                                 c = (unsigned short)(a + nx + 1), d = (unsigned short)(c + 1);
            const unsigned short tri[6] = { a, c, b, b, c, d };   // anticlockwise seen from outside
            std::memcpy(&m.indices[t], tri, sizeof tri);
            t += 6;
        }
    UploadMesh(&m, false);
    return m;
}

} // namespace

// ================================================================ places

Vector3 point(LonLat p, float radius) {
    const double la = p.lat * RAD, lo = p.lon * RAD;
    return { (float)(radius * std::cos(la) * std::sin(lo)), (float)(radius * std::sin(la)), (float)(radius * std::cos(la) * std::cos(lo)) };
}

LonLat lonLat(Vector3 p) {
    const double r = std::sqrt((double)p.x * p.x + (double)p.y * p.y + (double)p.z * p.z);
    if (r <= 0) return {};
    return { std::atan2((double)p.x, (double)p.z) * DEG, std::asin(std::clamp(p.y / r, -1.0, 1.0)) * DEG };
}

Vector3 north(LonLat p) {
    const double la = p.lat * RAD, lo = p.lon * RAD;
    return { (float)(-std::sin(la) * std::sin(lo)), (float)std::cos(la), (float)(-std::sin(la) * std::cos(lo)) };
}

Vector3 east(LonLat p) {
    const double lo = p.lon * RAD;
    return { (float)std::cos(lo), 0, (float)-std::sin(lo) };
}

g3d::GeometryPtr sphere(float radius, int segments) {
    const int nx = std::max(8, segments), ny = std::max(4, segments / 2);
    return std::make_shared<g3d::Geometry>([=] {
        return uploadGrid(nx, ny, [&](int i, int j, Vector3& pos, Vector3& n, Vector2& uv) {
            const LonLat p{ -180 + 360.0 * i / nx, 90 - 180.0 * j / ny };
            n = point(p, 1);
            pos = Vector3Scale(n, radius);
            uv = { (float)i / nx, (float)j / ny };
        });
    });
}

g3d::GeometryPtr patch(geo::Bounds b, float radius, int segments) {
    const double span = spanOf(b), tall = b.north - b.south;
    const int nx = std::max(2, segments), ny = std::max(2, (int)std::lround(segments * tall / span));
    return std::make_shared<g3d::Geometry>([=] {
        return uploadGrid(nx, ny, [&](int i, int j, Vector3& pos, Vector3& n, Vector2& uv) {
            const LonLat p{ b.west + span * i / nx, b.north - tall * j / ny };
            n = point(p, 1);
            pos = Vector3Scale(n, radius);
            uv = { (float)i / nx, (float)j / ny };
        });
    });
}

void look(g3d::Camera& camera, LonLat p, float distance) {
    camera.position = point(p, distance);
    camera.target = { 0, 0, 0 };
    camera.upVector = { 0, 1, 0 };
}

void place(g3d::Object3D& o, LonLat p, float altitude, float heading, float radius) {
    const double h = heading * RAD;
    const Vector3 up = point(p, 1);
    const Vector3 fwd = Vector3Add(Vector3Scale(north(p), (float)std::cos(h)), Vector3Scale(east(p), (float)std::sin(h)));
    o.position = Vector3Scale(up, radius + altitude);
    o.rotation = g3d::lookRotation(fwd, up);
}

// ================================================================ Map

Map::Map(int width, Color background) : Map(width, geo::Bounds{ -180, -90, 180, 90 }, background) {}

Map::Map(int width, geo::Bounds b, Color background)
    : w_(std::max(2, width)), h_(heightFor(std::max(2, width), b)), b_(b), px_((size_t)w_ * h_, background) {}

void Map::blend(int x, int y, Color c, float a) {
    if (a <= 0) return;
    Color& d = px_[(size_t)y * w_ + x];
    d = composite(c, a * c.a / 255.f, d);
}

Map& Map::fill(const geo::Geometry& g, Color c) {
    std::vector<float> cov((size_t)w_ * h_, 0.f);
    fillCoverage(Grid{ w_, h_, b_ }, g, cov);
    for (int y = 0; y < h_; y++)
        for (int x = 0; x < w_; x++) blend(x, y, c, cov[(size_t)y * w_ + x]);
    return *this;
}

Map& Map::stroke(const geo::Geometry& g, Color c, float width) {
    std::vector<float> cov((size_t)w_ * h_, 0.f);
    strokeCoverage(Grid{ w_, h_, b_ }, g, width / 2.0, cov);
    for (int y = 0; y < h_; y++)
        for (int x = 0; x < w_; x++) blend(x, y, c, cov[(size_t)y * w_ + x]);
    return *this;
}

Map& Map::graticule(double step, Color c, float width) {
    geo::Geometry g;
    if (step <= 0) return *this;
    for (double lon = -180; lon < 180 - 1e-9; lon += step) {
        std::vector<LonLat> l;
        const double reach = 90 - std::min(step, 10.0);       // stop short of the pole, where they all meet
        for (double lat = -reach; lat <= reach + 1e-9; lat += 1) l.push_back({ lon, lat });
        g.lines.push_back(l);
    }
    for (double lat = -90 + step; lat < 90 - 1e-9; lat += step) {
        std::vector<LonLat> l;
        for (double lon = -180; lon <= 180 + 1e-9; lon += 2) l.push_back({ lon, lat });
        g.lines.push_back(l);
    }
    return stroke(g, c, width);
}

Map& Map::disc(LonLat p, double radiusKm, Color c, float soft) {
    const Grid g{ w_, h_, b_ };
    const double R = radiusKm / EARTH_KM, Rdeg = R * DEG;
    const double s1 = std::sin(p.lat * RAD), c1 = std::cos(p.lat * RAD);
    const int y0 = std::max(0, (int)std::floor(g.yOf(p.lat + Rdeg)) - 1), y1 = std::min(h_ - 1, (int)std::ceil(g.yOf(p.lat - Rdeg)) + 1);
    for (int y = y0; y <= y1; y++) {
        const double lat = g.latAt(y + 0.5), s2 = std::sin(lat * RAD), c2 = std::cos(lat * RAD);
        const double half = std::min(180.0, Rdeg / std::max(0.01, c2) + 1);
        const double xc = g.xOf(p.lon), hx = half / g.span() * w_ + 1;
        for (int xx = (int)std::floor(xc - hx); xx <= (int)std::ceil(xc + hx); xx++) {
            int x = xx;
            if (g.wraps()) x = ((x % w_) + w_) % w_;
            else if (x < 0 || x >= w_) continue;
            const double lon = g.lonAt(x + 0.5);
            const double d = std::acos(std::clamp(s1 * s2 + c1 * c2 * std::cos((lon - p.lon) * RAD), -1.0, 1.0));
            const double edge = std::max(1e-9, R * soft);
            const float a = (float)std::clamp((R - d) / edge, 0.0, 1.0);
            blend(x, y, c, soft > 0 ? smooth(0, 1, a) : (d <= R ? 1.f : 0.f));
        }
    }
    return *this;
}

Map& Map::gradient(Color northC, Color equator, Color southC) {
    const Grid g{ w_, h_, b_ };
    for (int y = 0; y < h_; y++) {
        const double lat = g.latAt(y + 0.5);
        const Color under = lerpColor(equator, lat >= 0 ? northC : southC, (float)(std::fabs(lat) / 90));
        for (int x = 0; x < w_; x++) {
            Color& d = px_[(size_t)y * w_ + x];
            d = composite(d, d.a / 255.f, under);
        }
    }
    return *this;
}

Map& Map::over(const Map& top) {
    if (top.w_ != w_ || top.h_ != h_) return *this;
    for (size_t i = 0; i < px_.size(); i++) {
        const Color t = top.px_[i];
        if (t.a) px_[i] = composite(t, t.a / 255.f, px_[i]);
    }
    return *this;
}

Color Map::at(LonLat p) const {
    const Vector2 q = pixelOf(p);
    int x = (int)std::floor(q.x), y = (int)std::floor(q.y);
    if (wholeWorld(b_)) x = ((x % w_) + w_) % w_;
    if (x < 0 || x >= w_ || y < 0 || y >= h_) return BLANK;
    return px_[(size_t)y * w_ + x];
}

LonLat Map::place(float x, float y) const {
    const Grid g{ w_, h_, b_ };
    double lon = g.lonAt(x);
    if (lon > 180) lon -= 360;
    return { lon, g.latAt(y) };
}

Vector2 Map::pixelOf(LonLat p) const {
    const Grid g{ w_, h_, b_ };
    return { (float)g.xOf(p.lon), (float)g.yOf(p.lat) };
}

Image Map::image() const {
    Image img{};
    img.data = MemAlloc((unsigned)(px_.size() * 4));
    std::memcpy(img.data, px_.data(), px_.size() * 4);
    img.width = w_;
    img.height = h_;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return img;
}

g3d::DynamicTexturePtr Map::texture() const {
    auto copy = std::make_shared<Map>(*this);
    auto tex = std::make_shared<g3d::DynamicTexture>();
    tex->make = [copy](int) { return copy->image(); };
    tex->every = INT_MAX;                                   // made once
    return tex;
}

// ================================================================ Field

Field::Field(int width) : Field(width, geo::Bounds{ -180, -90, 180, 90 }) {}

Field::Field(int width, geo::Bounds b) : w_(std::max(2, width)), h_(heightFor(std::max(2, width), b)), b_(b), v_((size_t)w_ * h_, 0.f) {}

void Field::clear() { std::fill(v_.begin(), v_.end(), 0.f); }

void Field::add(LonLat p, double radiusKm, float weight) {
    if (weight == 0 || radiusKm <= 0) return;
    const Grid g{ w_, h_, b_ };
    const double R = radiusKm / EARTH_KM, reach = 1.5 * R, reachDeg = reach * DEG;
    const double inv = 2 / (R * R);                         // exp(-2 (d/R)^2)
    const double s1 = std::sin(p.lat * RAD), c1 = std::cos(p.lat * RAD);
    const int y0 = std::max(0, (int)std::floor(g.yOf(p.lat + reachDeg))), y1 = std::min(h_ - 1, (int)std::ceil(g.yOf(p.lat - reachDeg)));
    const double xc = g.xOf(p.lon);
    for (int y = y0; y <= y1; y++) {
        const double lat = g.latAt(y + 0.5), s2 = std::sin(lat * RAD), c2 = std::cos(lat * RAD);
        const double half = std::min(180.0, reachDeg / std::max(0.01, std::min(c2, 1.0)) + 1);
        const double hx = half / g.span() * w_ + 1;
        const int xa = (int)std::floor(xc - hx), xb = (int)std::ceil(xc + hx);
        for (int xx = xa; xx <= xb && xx - xa < w_; xx++) {
            int x = xx;
            if (g.wraps()) x = ((x % w_) + w_) % w_;
            else if (x < 0 || x >= w_) continue;
            const double lon = g.lonAt(x + 0.5);
            const double cd = std::clamp(s1 * s2 + c1 * c2 * std::cos((lon - p.lon) * RAD), -1.0, 1.0);
            const double d = std::acos(cd);
            if (d > reach) continue;
            v_[(size_t)y * w_ + x] += weight * (float)std::exp(-d * d * inv);
        }
    }
}

float Field::at(LonLat p) const {
    const Grid g{ w_, h_, b_ };
    int x = (int)std::floor(g.xOf(p.lon)), y = (int)std::floor(g.yOf(p.lat));
    if (g.wraps()) x = ((x % w_) + w_) % w_;
    if (x < 0 || x >= w_ || y < 0 || y >= h_) return 0;
    return v_[(size_t)y * w_ + x];
}

float Field::max() const { return v_.empty() ? 0 : *std::max_element(v_.begin(), v_.end()); }

Image Field::image(float scale, const std::function<Color(float)>& palette) const {
    Image img{};
    img.data = MemAlloc((unsigned)(v_.size() * 4));
    img.width = w_;
    img.height = h_;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    Color* out = (Color*)img.data;
    const float k = scale > 0 ? 1 / scale : 1;
    for (size_t i = 0; i < v_.size(); i++) {
        const float v = std::clamp(v_[i] * k, 0.f, 1.f);
        out[i] = v <= 0 ? BLANK : palette ? palette(v) : heat(v);
    }
    return img;
}

Color heat(float v) {
    v = std::clamp(v, 0.f, 1.f);
    if (v <= 0.002f) return BLANK;
    struct Stop { float at; Color c; };
    static const Stop stops[] = {
        { 0.00f, { 43, 11, 87, 255 } },  { 0.25f, { 122, 29, 110, 255 } }, { 0.50f, { 210, 54, 90, 255 } },
        { 0.72f, { 244, 128, 43, 255 } }, { 0.88f, { 252, 210, 90, 255 } }, { 1.00f, { 255, 251, 208, 255 } },
    };
    Color c = stops[5].c;
    for (int i = 0; i < 5; i++)
        if (v <= stops[i + 1].at) {
            c = lerpColor(stops[i].c, stops[i + 1].c, (v - stops[i].at) / (stops[i + 1].at - stops[i].at));
            break;
        }
    c.a = (unsigned char)std::lround(255 * (0.2f + 0.72f * smooth(0, 0.35f, v)) * smooth(0, 0.06f, v));
    return c;
}

// ================================================================ weather

float cloud(LonLat p, double seconds, const Weather& w) {
    const Noise& n = noiseFor(w.seed);
    const float lat = (float)p.lat;
    const float shift = (w.wind ? w.wind(lat) : defaultWind(lat)) * (float)seconds;
    Vector3 q = point({ p.lon - shift, p.lat }, 1);
    float density = 0;
    const Vector3 here = point(p, 1);
    for (const Storm& s : w.storms) {
        const Vector3 c = point(s.centre, 1);
        const float d = std::acos(std::clamp(Vector3DotProduct(here, c), -1.f, 1.f));
        const float R = (float)(s.radiusKm / EARTH_KM);
        if (d > 1.8f * R) continue;
        const float k = smooth(1.8f * R, 0.7f * R, d);
        const float dir = s.south ? -1.f : 1.f;
        // Inner air turns faster: a pattern carried round for a while winds into arms.
        const float omega = s.strength * 1.2f / (d / R + 0.45f);
        const float angle = -dir * omega * (2.4f + 0.35f * (float)seconds) * k;
        q = rotateAbout(point(lonLat(q), 1), c, angle);
        const float x = d / R;
        density += k * s.strength * (0.55f * std::exp(-x * x * 1.8f) + 0.08f);
        density -= s.strength * 1.4f * std::exp(-(x / 0.07f) * (x / 0.07f));   // the eye
    }
    const float t = (float)seconds * w.evolve;
    const Vector3 at = Vector3Add(Vector3Scale(q, w.scale), Vector3{ 0.31f * t, 0.53f * t, 0.79f * t });
    density += n.fbm(at) + coverBias(lat);
    const float edge = (0.5f - std::clamp(w.cover, 0.f, 1.f)) * 0.6f;
    return smooth(edge, edge + std::max(0.01f, w.softness), density);
}

Image clouds(int width, double seconds, const Weather& w) {
    const int W = std::max(2, width), H = std::max(1, W / 2);
    Image img{};
    img.data = MemAlloc((unsigned)(W * H * 4));
    img.width = W;
    img.height = H;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    Color* out = (Color*)img.data;
    for (int y = 0; y < H; y++) {
        const double lat = 90 - (y + 0.5) * 180.0 / H;
        for (int x = 0; x < W; x++) {
            const double lon = -180 + (x + 0.5) * 360.0 / W;
            const float c = cloud({ lon, lat }, seconds, w);
            const unsigned char shade = (unsigned char)std::lround(255 - 40 * (1 - c));   // thin cloud a touch grey
            out[(size_t)y * W + x] = { shade, shade, (unsigned char)std::min(255, shade + 4), (unsigned char)std::lround(255 * c * w.opacity) };
        }
    }
    return img;
}

// ================================================================ routes

Vector3 arc(LonLat a, LonLat b, float u, float height, float radius) {
    if (height < 0) height = 0.03f + 0.25f * (float)(geo::angle(a, b) / 180);
    const LonLat p = geo::interpolate(a, b, std::clamp(u, 0.f, 1.f));
    return point(p, radius * (1 + height * (float)std::sin(kPi * std::clamp(u, 0.f, 1.f))));
}

g3d::Object3D* path(g3d::Object3D& parent, std::function<Vector3(float)> curve, std::function<float(int)> head,
                    const RouteStyle& st, std::function<float(int)> tail) {
    g3d::Object3D* group = parent.add();
    group->name = "route";
    if (!curve || !head) return group;
    auto unit = g3d::Geometry::box(1, 1, 1);
    g3d::Material m;
    m.color = st.color;
    m.unlit = st.unlit;
    m.castShadow = false;
    const int n = std::max(1, st.segments);
    auto shared = std::make_shared<std::function<Vector3(float)>>(std::move(curve));
    auto h = std::make_shared<std::function<float(int)>>(std::move(head));
    auto tl = std::make_shared<std::function<float(int)>>(std::move(tail));
    for (int k = 0; k < n; k++) {
        const float u0 = (float)k / n, u1 = (float)(k + 1) / n;
        g3d::Object3D* seg = group->add(unit, m);
        seg->up = [shared, h, tl, u0, u1, st](int t, g3d::Object3D& self) {
            const float hu = std::clamp((*h)(t), 0.f, 1.f), tu = *tl ? std::clamp((*tl)(t), 0.f, 1.f) : 0.f;
            const float a = std::max(u0, tu), b = std::min(u1, hu);
            self.visible = b - a > 1e-5f;
            if (!self.visible) return;
            const Vector3 p0 = (*shared)(a), p1 = (*shared)(b);
            const Vector3 mid = Vector3Lerp(p0, p1, 0.5f);
            const float len = Vector3Distance(p0, p1);
            if (len < 1e-7f) { self.visible = false; return; }
            self.position = mid;
            self.rotation = g3d::lookRotation(Vector3Subtract(p1, p0), Vector3Length(mid) > 1e-6f ? mid : Vector3{ 0, 1, 0 });
            self.scale = { st.width, st.width * 0.5f, len + st.width * 0.5f };   // a little overlap closes the joints
        };
    }
    return group;
}

g3d::Object3D* route(g3d::Object3D& parent, LonLat a, LonLat b, std::function<float(int)> head, const RouteStyle& st,
                     std::function<float(int)> tail) {
    const float height = st.height;
    return path(parent, [a, b, height](float u) { return arc(a, b, u, height); }, std::move(head), st, std::move(tail));
}

g3d::Object3D* route(g3d::Scene& scene, LonLat a, LonLat b, std::function<float(int)> head, const RouteStyle& st,
                     std::function<float(int)> tail) {
    return route(*scene.add(), a, b, std::move(head), st, std::move(tail));
}

void fly(g3d::Object3D& o, LonLat a, LonLat b, float u, float height, float radius) {
    const float du = 0.002f, ua = std::max(0.f, u - du), ub = std::min(1.f, u + du);
    const Vector3 p = arc(a, b, u, height, radius);
    const Vector3 fwd = Vector3Subtract(arc(a, b, ub, height, radius), arc(a, b, ua, height, radius));
    o.position = p;
    o.rotation = g3d::lookRotation(Vector3Length(fwd) > 1e-9f ? fwd : north(lonLat(p)), p);
}

} // namespace globe
} // namespace rmtn
