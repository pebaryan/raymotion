// own_compute.cpp -- "own-your-compute": a 66 s story film about running AI on your own
// machine, and a tour of what raymotion can do. Seven acts:
//
//   1  the cloud      GPU 3D: a fogged aisle of server racks, blinking unlit LEDs, a bill
//   2  the strings    2D glitch: slammed stamps, RGB split, tearing, screen shake
//   3  the turn       additive particle morph: a cloud of points pours into a chip
//   4  the machine    GPU 3D: a procedural graphics card, spinning fans, DOF, shadows
//   5  the model      2D: a neural net firing, a weights download
//   6  offline        2D: a terminal streaming tokens with the network off
//   7  the home       GPU 3D: generated island terrain, a dawn sky, a privacy dome
//
// Nothing is loaded from disk that this file did not make: meshes are built vertex by
// vertex, textures (PCB, wood, rack panels) and the sky are computed per pixel, and the
// score is synthesised here (saws, filters, drums, reverb) into a WAV under out/.
//   raymotion render own-your-compute
#include "rmtn/raymotion.hpp"
#include <rlgl.h>
#include <raymath.h>
#include <cstdio>
#include <filesystem>

namespace rmtn {
namespace {

constexpr int FPS = 30;
constexpr int DURATION = 1980;                        // 66 s
// Generated assets live here; bump the version when a generator changes.
const std::string ASSETS = "out/oyc-assets-v2";

// Act boundaries (frames). The score's sections land on the same seconds.
constexpr int A1 = 0, A2 = 240, A3 = 480, A4 = 660, A5 = 960, A6 = 1200, A7 = 1440;

const Color NIGHT = mkColor(0x05060c), INK = mkColor(0xf2f4ff), COLD = mkColor(0x8fa6ff),
            ALARM = mkColor(0xff4a4a), TEAL = mkColor(0x47dad8), EMBER = mkColor(0xffb347),
            DIM = mkColor(0x8ea0b8);

// ------------------------------------------------------------------ noise
float vnoise(float x, float y, unsigned long long seed) {
    const float fx = std::floor(x), fy = std::floor(y);
    const long long ix = (long long)fx, iy = (long long)fy;
    auto h = [seed](long long a, long long b) { return (float)rnd01(seed, (unsigned long long)(a * 73856093LL ^ b * 19349663LL)); };
    float ux = x - fx, uy = y - fy;
    ux = ux * ux * (3 - 2 * ux); uy = uy * uy * (3 - 2 * uy);
    const float a = h(ix, iy), b = h(ix + 1, iy), c = h(ix, iy + 1), d = h(ix + 1, iy + 1);
    return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
}
float fbm(float x, float y, unsigned long long seed, int octaves = 5) {
    float sum = 0, amp = 0.5f, norm = 0;
    for (int i = 0; i < octaves; i++) {
        sum += amp * vnoise(x, y, seed + i * 101);
        norm += amp; amp *= 0.5f; x *= 2.03f; y *= 2.03f;
    }
    return sum / norm;
}
float smooth(float a, float b, float x) {
    float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}

// ------------------------------------------------------------------ pixel canvas
// A CPU RGBA buffer with the few raster ops the texture generators need.
struct Pix {
    int w, h;
    std::vector<Color> px;
    Pix(int w_, int h_, Color fill) : w(w_), h(h_), px((size_t)w_ * h_, fill) {}
    Color& at(int x, int y) { return px[(size_t)y * w + x]; }
    void blend(int x, int y, Color c, float a) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        a *= c.a / 255.f;
        Color& d = at(x, y);
        d = { (unsigned char)(d.r + (c.r - d.r) * a), (unsigned char)(d.g + (c.g - d.g) * a),
              (unsigned char)(d.b + (c.b - d.b) * a), 255 };
    }
    void rect(int x, int y, int rw, int rh, Color c, float a = 1) {
        for (int j = y; j < y + rh; j++) for (int i = x; i < x + rw; i++) blend(i, j, c, a);
    }
    void disc(float cx, float cy, float r, Color c, float a = 1) {
        for (int j = (int)(cy - r - 1); j <= (int)(cy + r + 1); j++)
            for (int i = (int)(cx - r - 1); i <= (int)(cx + r + 1); i++) {
                float d = std::sqrt((i + 0.5f - cx) * (i + 0.5f - cx) + (j + 0.5f - cy) * (j + 0.5f - cy));
                blend(i, j, c, a * std::clamp(r - d + 0.5f, 0.f, 1.f));
            }
    }
    void line(float x0, float y0, float x1, float y1, float wdt, Color c) {
        const int n = (int)(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0)) + 1);
        for (int i = 0; i <= n; i++) disc(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, wdt / 2, c);
    }
    Image image() const {          // a copy the caller (or raylib) owns
        Image img{};
        img.width = w; img.height = h; img.mipmaps = 1; img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        img.data = MemAlloc((unsigned)(px.size() * sizeof(Color)));
        std::memcpy(img.data, px.data(), px.size() * sizeof(Color));
        return img;
    }
    void save(const std::string& path) const {
        Image img = image();
        ExportImage(img, path.c_str());
        UnloadImage(img);
    }
};

// A printed circuit board: solder mask, copper traces routed at right angles and 45s,
// vias, and pads for the parts that sit on it.
void genPcb(const std::string& path) {
    Pix p(1024, 512, mkColor(0x0b2e22));
    for (int y = 0; y < p.h; y++)
        for (int x = 0; x < p.w; x++) {
            float n = fbm(x * 0.02f, y * 0.02f, 7);
            p.at(x, y) = mixC(mkColor(0x08241b), mkColor(0x12402f), n);
        }
    const Color trace = mkColor(0x2f8a5c), copper = mkColor(0xd6a84a), pad = mkColor(0xc9c3b0);
    for (int k = 0; k < 140; k++) {
        float x = (float)rrange(11, k * 3, 0, p.w), y = (float)rrange(11, k * 3 + 1, 0, p.h);
        int dir = (int)rrange(11, k * 3 + 2, 0, 4);
        for (int seg = 0; seg < 5; seg++) {
            const float len = (float)rrange(12, k * 7 + seg, 20, 140);
            const float dx[8] = { 1, 0, -1, 0, 0.707f, -0.707f, 0.707f, -0.707f };
            const float dy[8] = { 0, 1, 0, -1, 0.707f, 0.707f, -0.707f, -0.707f };
            const float nx = x + dx[dir] * len, ny = y + dy[dir] * len;
            p.line(x, y, nx, ny, 3, trace);
            x = nx; y = ny;
            dir = rnd01(13, k * 9 + seg) < 0.5 ? (int)rrange(14, k * 5 + seg, 0, 8) : dir;
        }
        p.disc(x, y, 5, copper);
        p.disc(x, y, 2, mkColor(0x050505));
    }
    // Memory chips around the GPU die, and an edge connector.
    for (int i = 0; i < 4; i++) {
        p.rect(300 + i * 110, 60, 80, 60, mkColor(0x111111));
        p.rect(300 + i * 110, 392, 80, 60, mkColor(0x111111));
    }
    p.rect(400, 170, 220, 170, mkColor(0x1a1a1c));
    p.rect(440, 205, 140, 100, mkColor(0x6d7078));
    for (int i = 0; i < 40; i++) p.rect(60 + i * 22, 490, 14, 22, copper);
    for (int i = 0; i < 60; i++)                                   // small passives
        p.rect((int)rrange(15, i, 40, 980), (int)rrange(16, i, 20, 470), 8, 4, pad);
    p.save(path);
}

// Oak: growth rings bent by low-frequency noise, plus fine grain.
void genWood(const std::string& path) {
    Pix p(1024, 1024, BLACK);
    const Color light = mkColor(0x9a6a3e), dark = mkColor(0x5a3820);
    for (int y = 0; y < p.h; y++)
        for (int x = 0; x < p.w; x++) {
            float warp = fbm(x * 0.006f, y * 0.004f, 21, 4) * 14;
            float ring = 0.5f + 0.5f * std::sin((y * 0.16f + warp) * 2.f);
            ring = std::pow(ring, 2.f);
            float grain = fbm(x * 0.02f, y * 0.6f, 23, 3);
            Color c = mixC(light, dark, ring * 0.35f + grain * 0.4f);
            p.at(x, y) = c;
        }
    p.save(path);
}

// A server's front panel: drive sleds with vents and handle bars (LEDs are separate objects).
void genRack(const std::string& path) {
    Pix p(256, 512, mkColor(0x14161b));
    for (int u = 0; u < 12; u++) {
        const int y = 10 + u * 41;
        p.rect(10, y, 236, 36, mkColor(0x1e2129));
        for (int s = 0; s < 6; s++) {
            p.rect(16 + s * 38, y + 5, 32, 26, mkColor(0x0d0e12));
            for (int v = 0; v < 5; v++) p.rect(20 + s * 38, y + 8 + v * 5, 24, 2, mkColor(0x2a2e38));
        }
    }
    p.rect(0, 0, 10, 512, mkColor(0x2b2f38));
    p.rect(246, 0, 10, 512, mkColor(0x2b2f38));
    p.save(path);
}

// Dawn sky, equirectangular in the engine's convention (u = atan2(z, x), v = acos(y)):
// a warm horizon, a sun disc with a halo in direction `sun`, and fbm clouds lit from it.
std::function<Image()> dawnSky(Vector3 sun) {
    return [sun] {
        Pix p(1024, 512, BLACK);
        const Vector3 s = Vector3Normalize(sun);
        for (int y = 0; y < p.h; y++) {
            const float th = (y + 0.5f) / p.h * PI;
            for (int x = 0; x < p.w; x++) {
                const float az = ((x + 0.5f) / p.w - 0.5f) * 2 * PI;
                const Vector3 d{ std::cos(az) * std::sin(th), std::cos(th), std::sin(az) * std::sin(th) };
                const float up = d.y, cosSun = Vector3DotProduct(d, s);
                Color c;
                if (up >= 0) {
                    c = mixC(mkColor(0xffb070), mkColor(0x6f8fd0), std::pow(up, 0.45f));
                    c = mixC(c, mkColor(0x26326a), smooth(0.35f, 1.f, up));
                } else {
                    c = mixC(mkColor(0xb07a68), mkColor(0x1a2030), std::pow(-up, 0.5f));
                }
                const float glow = std::pow(std::max(0.f, cosSun), 6.f);
                c = mixC(c, mkColor(0xffd9a0), glow * 0.75f);
                if (up > 0.02f) {                          // clouds on a dome above the horizon
                    const float cx = d.x / (up + 0.15f), cz = d.z / (up + 0.15f);
                    float cl = fbm(cx * 1.4f + 3, cz * 1.4f, 31, 5);
                    cl = smooth(0.5f, 0.78f, cl) * smooth(0.02f, 0.25f, up);
                    const Color lit = mixC(mkColor(0xe9a0a0), mkColor(0xffe2b8), std::max(0.f, cosSun));
                    c = mixC(c, lit, cl * 0.8f);
                }
                c = mixC(c, mkColor(0xfff6de), smooth(0.9985f, 0.9993f, cosSun));
                p.at(x, y) = c;
            }
        }
        return p.image();
    };
}

// ------------------------------------------------------------------ meshes
// Flat- or smooth-shaded triangles with vertex colours; tri() faces each triangle along
// `out` (any vector pointing to its front), so callers need not track winding.
struct MeshBuilder {
    std::vector<float> v, n, uv;
    std::vector<unsigned char> col;
    std::vector<unsigned short> idx;
    unsigned short vert(Vector3 p, Vector3 nn, Vector2 t, Color c) {
        v.insert(v.end(), { p.x, p.y, p.z });
        n.insert(n.end(), { nn.x, nn.y, nn.z });
        uv.insert(uv.end(), { t.x, t.y });
        col.insert(col.end(), { c.r, c.g, c.b, c.a });
        return (unsigned short)(v.size() / 3 - 1);
    }
    void tri(Vector3 a, Vector3 b, Vector3 c, Color k, Vector3 out) {
        Vector3 nn = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
        if (Vector3DotProduct(nn, out) < 0) { std::swap(b, c); nn = Vector3Negate(nn); }
        idx.push_back(vert(a, nn, { 0, 0 }, k));
        idx.push_back(vert(b, nn, { 1, 0 }, k));
        idx.push_back(vert(c, nn, { 0, 1 }, k));
    }
    void quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color k, Vector3 out) {
        tri(a, b, c, k, out); tri(a, c, d, k, out);
    }
    Mesh build() {
        Mesh m{};
        m.vertexCount = (int)(v.size() / 3);
        m.triangleCount = (int)(idx.size() / 3);
        auto copy = [](auto& src) {
            using T = typename std::decay_t<decltype(src)>::value_type;
            T* dst = (T*)MemAlloc((unsigned)(src.size() * sizeof(T)));
            std::memcpy(dst, src.data(), src.size() * sizeof(T));
            return dst;
        };
        m.vertices = copy(v); m.normals = copy(n); m.texcoords = copy(uv);
        m.colors = copy(col); m.indices = copy(idx);
        UploadMesh(&m, false);
        return m;
    }
};

// A pitched roof: a triangular prism along x, its ridge at height h.
g3d::GeometryPtr roofGeometry(float w, float d, float h) {
    return std::make_shared<g3d::Geometry>([=] {
        MeshBuilder b;
        const Color c = WHITE;
        const Vector3 l0{ -w / 2, 0, -d / 2 }, l1{ -w / 2, 0, d / 2 }, lt{ -w / 2, h, 0 };
        const Vector3 r0{ w / 2, 0, -d / 2 }, r1{ w / 2, 0, d / 2 }, rt{ w / 2, h, 0 };
        b.tri(l0, l1, lt, c, { -1, 0, 0 });
        b.tri(r0, r1, rt, c, { 1, 0, 0 });
        b.quad(l1, r1, rt, lt, c, { 0, d / 2, h });
        b.quad(l0, r0, rt, lt, c, { 0, d / 2, -h });
        b.quad(l0, r0, r1, l1, c, { 0, -1, 0 });
        return b.build();
    });
}

// An open hemisphere (the dome), smooth-shaded, open at y = 0.
g3d::GeometryPtr domeGeometry(float r, int rings, int slices) {
    return std::make_shared<g3d::Geometry>([=] {
        MeshBuilder b;
        for (int j = 0; j <= rings; j++)
            for (int i = 0; i <= slices; i++) {
                const float th = (float)j / rings * PI / 2, ph = (float)i / slices * 2 * PI;
                const Vector3 n{ std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph) };
                b.vert(Vector3Scale(n, r), n, { (float)i / slices, (float)j / rings }, WHITE);
            }
        for (int j = 0; j < rings; j++)
            for (int i = 0; i < slices; i++) {
                const unsigned short a = (unsigned short)(j * (slices + 1) + i), c = (unsigned short)(a + 1);
                const unsigned short d = (unsigned short)(a + slices + 1), e = (unsigned short)(d + 1);
                b.idx.insert(b.idx.end(), { a, c, d, c, e, d });
            }
        return b.build();
    });
}

// The island: fbm hills under a radial falloff, a ridge behind, a flat lot at the origin.
float islandHeight(float x, float z) {
    const float r = std::sqrt(x * x + z * z);
    const float island = 1 - smooth(14, 46, r);
    float h = (fbm(x * 0.045f + 5, z * 0.045f, 41) * 8.5f + 1.2f) * island - 1.4f;
    h += 11 * std::exp(-((x + 6) * (x + 6) + (z + 30) * (z + 30)) / 170) * island;
    return h + (2.0f - h) * smooth(15, 6, r);       // the house lot sits at y = 2
}
g3d::GeometryPtr terrainGeometry(float size, int n) {
    return std::make_shared<g3d::Geometry>([=] {
        MeshBuilder b;
        const float step = size / n, e = 0.05f;
        for (int j = 0; j <= n; j++)
            for (int i = 0; i <= n; i++) {
                const float x = -size / 2 + i * step, z = -size / 2 + j * step, y = islandHeight(x, z);
                const Vector3 nn = Vector3Normalize({ (islandHeight(x - e, z) - islandHeight(x + e, z)) / (2 * e), 1,
                                                      (islandHeight(x, z - e) - islandHeight(x, z + e)) / (2 * e) });
                const float g = fbm(x * 0.3f, z * 0.3f, 43, 3);
                Color c = mixC(mkColor(0x3f6b2e), mkColor(0x6f8f3a), g);                   // grass
                c = mixC(c, mkColor(0x6e6a62), smooth(0.86f, 0.7f, nn.y));                  // rock on steep
                c = mixC(c, mkColor(0xd9c896), smooth(0.9f, 0.3f, y));                      // sand at the shore
                c = mixC(c, mkColor(0xeef0f6), smooth(8.f, 9.5f, y) * smooth(0.6f, 0.8f, nn.y));   // snow
                b.vert({ x, y, z }, nn, { (float)i / n, (float)j / n }, c);
            }
        for (int j = 0; j < n; j++)
            for (int i = 0; i < n; i++) {
                const unsigned short a = (unsigned short)(j * (n + 1) + i), r = (unsigned short)(a + 1);
                const unsigned short d = (unsigned short)(a + n + 1), q = (unsigned short)(d + 1);
                b.idx.insert(b.idx.end(), { a, d, r, r, d, q });          // counter-clockwise from above
            }
        return b.build();
    });
}

// ------------------------------------------------------------------ score
// A small offline synthesiser: polyBLEP saws through state-variable filters, noise drums,
// a ping-pong delay and a Schroeder reverb, all summed into stereo at 48 kHz.
struct Svf {                               // Chamberlin state-variable filter
    float low = 0, band = 0;
    float lp(float x, float f, float q, int rate) {
        const float k = 2 * std::sin(PI * std::min(f, rate / 6.f) / rate);
        low += k * band;
        const float high = x - low - q * band;
        band += k * high;
        return low;
    }
    float hp(float x, float f, float q, int rate) { lp(x, f, q, rate); return x - low - q * band; }
    float bp(float x, float f, float q, int rate) { lp(x, f, q, rate); return band; }
};
float polyBlep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
}
double midiHz(double m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }

struct Score {
    static constexpr int RATE = 48000;
    int n;
    std::vector<float> dryL, dryR, sendL, sendR, delayL, delayR, duck;
    unsigned long long noiseState = 0x1234567ULL;
    explicit Score(double seconds) : n((int)(seconds * RATE)),
        dryL(n), dryR(n), sendL(n), sendR(n), delayL(n), delayR(n), duck(n, 1.f) {}
    float noise() { noiseState ^= noiseState << 13; noiseState ^= noiseState >> 7; noiseState ^= noiseState << 17;
                    return (float)((noiseState >> 11) * (1.0 / 9007199254740992.0)) * 2 - 1; }
    void put(int i, float x, float pan, float rev, float dly) {
        if (i < 0 || i >= n) return;
        const float l = x * std::sqrt(1 - pan) , r = x * std::sqrt(pan);
        dryL[i] += l; dryR[i] += r;
        sendL[i] += l * rev; sendR[i] += r * rev;
        delayL[i] += l * dly; delayR[i] += r * dly;
    }
    // Sustained detuned-saw chord voice with a slow filter swell.
    void pad(double t0, double dur, double midi, float amp, float cutoff, float pan = 0.5f) {
        const int a = (int)(t0 * RATE), len = (int)((dur + 1.2) * RATE);
        Svf f1, f2;
        const double det[3] = { -0.09, 0, 0.11 };
        float ph[3] = { 0.1f, 0.4f, 0.7f };
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            const float env = (float)(std::min(1.0, t / 0.7) * (t > dur ? std::exp(-(t - dur) * 3.5) : 1.0));
            float s = 0;
            for (int o = 0; o < 3; o++) {
                const float dt = (float)(midiHz(midi + det[o]) / RATE);
                ph[o] += dt; if (ph[o] >= 1) ph[o] -= 1;
                s += (2 * ph[o] - 1) - polyBlep(ph[o], dt);
            }
            const float cut = cutoff * (0.75f + 0.25f * (float)std::sin(t * 1.3 + midi));
            s = f2.lp(f1.lp(s / 3, cut, 0.7f, RATE), cut, 0.9f, RATE);
            put(a + k, s * env * amp * duck[std::min(n - 1, a + k)], pan, 0.45f, 0);
        }
    }
    void bass(double t0, double dur, double midi, float amp) {
        const int a = (int)(t0 * RATE), len = (int)((dur + 0.05) * RATE);
        Svf f; float ph = 0;
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            const float dt = (float)(midiHz(midi) / RATE);
            ph += dt; if (ph >= 1) ph -= 1;
            float s = (2 * ph - 1) - polyBlep(ph, dt);
            s += 0.6f * (float)std::sin(TAU * midiHz(midi - 12) * t);
            const float cut = 180 + 1400 * (float)std::exp(-t * 9);
            const float env = (float)(std::min(1.0, t / 0.004) * std::exp(-t * 2.5) * std::min(1.0, (dur + 0.05 - t) / 0.03));
            put(a + k, f.lp(s, cut, 0.6f, RATE) * env * amp, 0.5f, 0.05f, 0);
        }
    }
    void pluck(double t0, double midi, float amp, float pan, float decay = 5) {
        const int a = (int)(t0 * RATE), len = (int)(1.4 * RATE);
        const double f = midiHz(midi);
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            const float env = (float)(std::min(1.0, t / 0.003) * std::exp(-t * decay));
            const float s = (float)(std::sin(TAU * f * t) + 0.3 * std::sin(TAU * 2 * f * t) * std::exp(-t * 9)
                                    + 0.12 * std::sin(TAU * 3 * f * t) * std::exp(-t * 14));
            put(a + k, s * env * amp * duck[std::min(n - 1, a + k)], pan, 0.3f, 0.45f);
        }
    }
    void bell(double t0, double midi, float amp, float pan) {
        const int a = (int)(t0 * RATE), len = (int)(4.5 * RATE);
        const double f = midiHz(midi), parts[4] = { 1, 2.76, 5.4, 8.93 }, gains[4] = { 1, 0.45, 0.25, 0.12 };
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            double s = 0;
            for (int p = 0; p < 4; p++) s += gains[p] * std::sin(TAU * f * parts[p] * t) * std::exp(-t * (0.9 + p * 0.9));
            put(a + k, (float)(s * std::min(1.0, t / 0.002)) * amp, pan, 0.6f, 0.25f);
        }
    }
    void kick(double t0, float amp) {
        const int a = (int)(t0 * RATE), len = (int)(0.5 * RATE);
        double ph = 0;
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            ph += TAU * (44 + 120 * std::exp(-t * 32)) / RATE;
            const float s = (float)(std::sin(ph) * std::exp(-t * 7) + 0.3 * std::exp(-t * 300) * noise());
            put(a + k, s * amp, 0.5f, 0.02f, 0);
            if (a + k < n) duck[a + k] = std::min(duck[a + k], 1 - 0.55f * (float)std::exp(-t * 7));
        }
    }
    void clap(double t0, float amp) {
        const int a = (int)(t0 * RATE), len = (int)(0.3 * RATE);
        Svf f;
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            const double burst = t < 0.03 ? (std::fmod(t, 0.01) < 0.004 ? 1.0 : 0.3) : std::exp(-(t - 0.03) * 18);
            put(a + k, f.bp(noise(), 1300, 0.5f, RATE) * (float)burst * amp, 0.55f, 0.35f, 0);
        }
    }
    void hat(double t0, float amp, float pan) {
        const int a = (int)(t0 * RATE), len = (int)(0.08 * RATE);
        Svf f;
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            put(a + k, f.hp(noise(), 7500, 0.6f, RATE) * (float)std::exp(-t * 60) * amp, pan, 0.08f, 0);
        }
    }
    // A bitcrushed square stab at a random-ish pitch: the "error" sound.
    void glitch(double t0, float amp, int seed) {
        const int a = (int)(t0 * RATE), len = (int)(0.22 * RATE);
        float held = 0;
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            const double f = 110 * std::pow(2.0, std::floor(rnd01(91, seed * 7 + (int)(t * 40)) * 24) / 12);
            if (k % 12 == 0) held = std::sin(TAU * f * t) > 0 ? 1.f : -1.f;
            put(a + k, held * (float)std::exp(-t * 14) * amp, 0.3f + 0.4f * (float)rnd01(92, seed), 0.15f, 0.2f);
        }
    }
    void tick(double t0, float amp) {
        const int a = (int)(t0 * RATE), len = (int)(0.03 * RATE);
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            put(a + k, (float)(std::sin(TAU * 3200 * t) * std::exp(-t * 200)) * amp, 0.7f, 0.1f, 0);
        }
    }
    // Filtered noise sweeping up from t0 to t1, cut dead at t1 (the drop).
    void riser(double t0, double t1, float amp) {
        const int a = (int)(t0 * RATE), len = (int)((t1 - t0) * RATE);
        Svf f;
        for (int k = 0; k < len; k++) {
            const double u = (double)k / len;
            const float cut = 250 * (float)std::pow(40.0, u);
            put(a + k, f.bp(noise(), cut, 0.25f, RATE) * (float)(u * u) * amp, 0.5f + 0.3f * (float)std::sin(u * 30), 0.4f, 0);
        }
    }
    void impact(double t0, float amp) {
        const int a = (int)(t0 * RATE), len = (int)(3.0 * RATE);
        double ph = 0;
        Svf f;
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            ph += TAU * (30 + 60 * std::exp(-t * 6)) / RATE;
            const float boom = (float)(std::sin(ph) * std::exp(-t * 1.6));
            const float crash = f.hp(noise(), 3000, 0.7f, RATE) * (float)std::exp(-t * 2.2);
            put(a + k, (boom + 0.35f * crash) * amp, 0.5f, 0.5f, 0);
        }
    }

    std::vector<float> mix() {
        // Ping-pong delay, 3/8 of a beat at 120 bpm.
        const int dl = (int)(0.375 * RATE);
        std::vector<float> outL(dryL), outR(dryR);
        std::vector<float> echoL(n), echoR(n);
        for (int i = 0; i < n; i++) {
            const float inL = delayL[i] + (i >= dl ? echoR[i - dl] * 0.42f : 0);
            const float inR = delayR[i] + (i >= dl ? echoL[i - dl] * 0.42f : 0);
            echoL[i] = inR; echoR[i] = inL;                       // swap sides each repeat
            outL[i] += echoL[i] * 0.5f; outR[i] += echoR[i] * 0.5f;
            sendL[i] += echoL[i] * 0.3f; sendR[i] += echoR[i] * 0.3f;
        }
        // Schroeder reverb: four damped combs into two allpasses, per side.
        auto reverb = [&](const std::vector<float>& in, int spread) {
            const int combs[4] = { 1557, 1617, 1491, 1422 }, alls[2] = { 556, 441 };
            std::vector<float> wet(n, 0.f);
            for (int c : combs) {
                const int len = (c + spread) * RATE / 44100;
                std::vector<float> buf(len, 0.f);
                float store = 0;
                for (int i = 0, p = 0; i < n; i++, p = (p + 1) % len) {
                    const float y = buf[p];
                    store = y * 0.7f + store * 0.3f;
                    buf[p] = in[i] * 0.015f + store * 0.86f;
                    wet[i] += y;
                }
            }
            for (int a : alls) {
                const int len = (a + spread) * RATE / 44100;
                std::vector<float> buf(len, 0.f);
                for (int i = 0, p = 0; i < n; i++, p = (p + 1) % len) {
                    const float b = buf[p];
                    buf[p] = wet[i] + b * 0.5f;
                    wet[i] = b - wet[i];
                }
            }
            return wet;
        };
        const std::vector<float> wl = reverb(sendL, 0), wr = reverb(sendR, 23);
        std::vector<float> stereo((size_t)n * 2);
        float peak = 0;
        for (int i = 0; i < n; i++) {
            const float l = std::tanh((outL[i] + wl[i] * 3.2f) * 0.9f), r = std::tanh((outR[i] + wr[i] * 3.2f) * 0.9f);
            stereo[2 * i] = l; stereo[2 * i + 1] = r;
            peak = std::max({ peak, std::fabs(l), std::fabs(r) });
        }
        const int fade = RATE * 3;
        for (int i = 0; i < n; i++) {
            const float g = (peak > 0 ? 0.9f / peak : 1) * std::min(1.f, (float)(n - i) / fade);
            stereo[2 * i] *= g; stereo[2 * i + 1] *= g;
        }
        return stereo;
    }
};

// The film's score, timed to the acts: a cold pad under the cloud (0-8 s), glitch stabs
// on each stamp (8-16), a riser into silence (16-22), an impact and a 120 bpm groove on
// Dm-Bb-F-C (22-48), then Bb-C-Dm-Bb-C resolving to D major under the title (48-66).
void genScore(const std::string& path) {
    Score s(DURATION / (double)FPS);
    const double beat = 0.5;
    // Act 1-2: D minor drone, meter ticks.
    s.pad(0, 7.8, 50, 0.22f, 600, 0.4f); s.pad(0, 7.8, 57, 0.16f, 600, 0.6f); s.pad(0, 7.8, 62, 0.10f, 500);
    s.pad(8, 7.8, 46, 0.20f, 500, 0.4f); s.pad(8, 7.8, 53, 0.15f, 500, 0.6f); s.pad(8, 7.8, 61, 0.10f, 450);
    for (double t = 0.5; t < 16; t += 0.25) s.tick(t, t < 8 ? 0.05f : 0.08f);
    for (double t = 8; t < 16; t += beat) s.bass(t, 0.22, 38, 0.28f);
    for (int i = 0; i < 5; i++) { s.glitch(8 + i, 0.35f, i); s.kick(8 + i, 0.5f); }
    s.glitch(13.5, 0.25f, 9); s.glitch(14.25, 0.25f, 10); s.glitch(15.0, 0.3f, 11);
    // Act 3: the turn.
    s.riser(16, 22, 0.28f);
    for (int i = 0; i < 6; i++) s.bell(16.2 + i, 74 + (i % 2) * 5, 0.10f, 0.3f + 0.08f * i);
    s.impact(22, 0.9f);
    // Acts 4-6: the groove. Chords as (bass root, pad notes, arp notes).
    struct Chord { int root; int pad[3]; int arp[4]; };
    const Chord dm{ 38, { 62, 65, 69 }, { 74, 77, 81, 77 } }, bb{ 34, { 62, 65, 70 }, { 70, 74, 77, 74 } },
                f{ 41, { 60, 65, 69 }, { 72, 77, 81, 77 } }, c{ 36, { 60, 64, 67 }, { 72, 76, 79, 76 } },
                dmaj{ 38, { 62, 66, 69 }, { 74, 78, 81, 86 } };
    const Chord groove[4] = { dm, bb, f, c };
    for (double t = 22; t < 48; t += 2) {
        for (int b = 0; b < 4; b++) s.kick(t + b * beat, 0.85f);
    }
    for (double t = 22; t < 48; t += 2) {
        const Chord& ch = groove[(int)((t - 22) / 2) % 4];
        for (int p : ch.pad) s.pad(t, 1.9, p, 0.10f, 1400);
        for (int e = 0; e < 8; e++) s.bass(t + e * 0.25, 0.2, ch.root + (e % 4 == 3 ? 12 : 0), 0.30f);
        for (int e = 0; e < 8; e++) s.pluck(t + e * 0.25, ch.arp[e % 4], 0.11f, e % 2 ? 0.3f : 0.7f);
        if (t >= 32) { s.clap(t + 0.5, 0.35f); s.clap(t + 1.5, 0.35f); }
        for (int h = 0; h < (t >= 40 ? 8 : 4); h++)
            s.hat(t + (t >= 40 ? h * 0.25 : h * 0.5 + 0.25), 0.16f, h % 2 ? 0.35f : 0.65f);
    }
    // Act 7: the lift, then D major.
    const Chord lift[5] = { bb, c, dm, bb, c };
    for (int i = 0; i < 5; i++) {
        const double t = 48 + i * 2;
        const Chord& ch = lift[i];
        for (int b = 0; b < 4; b++) s.kick(t + b * beat, 0.7f);
        s.clap(t + 0.5, 0.3f); s.clap(t + 1.5, 0.3f);
        for (int h = 0; h < 8; h++) s.hat(t + h * 0.25, 0.13f, h % 2 ? 0.35f : 0.65f);
        for (int p : ch.pad) s.pad(t, 1.9, p, 0.12f, 1800);
        for (int e = 0; e < 8; e++) s.bass(t + e * 0.25, 0.2, ch.root, 0.28f);
        for (int e = 0; e < 8; e++) s.pluck(t + e * 0.25, ch.arp[e % 4] + (e >= 4 ? 12 : 0), 0.10f, e % 2 ? 0.3f : 0.7f);
    }
    s.riser(56.5, 58, 0.16f);
    s.impact(58, 0.6f);
    for (int p : dmaj.pad) { s.pad(58, 6.5, p, 0.14f, 2200, 0.35f); s.pad(58, 6.5, p - 12, 0.10f, 1200, 0.65f); }
    s.bass(58, 4, 38, 0.3f);
    for (int i = 0; i < 4; i++) s.bell(58 + i * 0.5, dmaj.arp[i], 0.14f, 0.2f + 0.2f * i);
    s.bell(61, 86, 0.08f, 0.5f);
    media::writeWav(path, s.mix(), Score::RATE);
}

// Generates every asset once per ASSETS version.
void ensureAssets() {
    std::filesystem::create_directories(ASSETS);
    auto make = [](const std::string& file, void (*gen)(const std::string&)) {
        const std::string path = ASSETS + "/" + file;
        if (!std::filesystem::exists(path)) gen(path);
    };
    make("pcb.png", genPcb);
    make("wood.png", genWood);
    make("rack.png", genRack);
    make("score.wav", genScore);
}

// ------------------------------------------------------------------ 2D helpers
void glow(const Ctx& cx, Vector2 p, float r, Color col, float a = 1) {
    DrawCircleGradient(mapP(cx, p), r * cx.s, withAlpha(col, cx.a * a), withAlpha(col, 0));
}
Font mono() {
    static bool missing = false;
    if (!missing) {
        try { return fonts::load({ "consola.ttf", "CascadiaMono.ttf", "DejaVuSansMono.ttf", "Menlo.ttc" }); }
        catch (const std::exception&) { missing = true; }
    }
    return fonts::ui(false);
}
float fadeWindow(int t, int t0, int t1, int in = 15, int out = 15) {
    return (float)(interpolate(t, { (double)t0, (double)t0 + in }, { 0, 1 }, clamped(ease::out(ease::cubic))) *
                   interpolate(t, { (double)t1 - out, (double)t1 }, { 1, 0 }, clamped(ease::in(ease::cubic))));
}
std::string money(double v) {
    char buf[32];
    snprintf(buf, sizeof buf, "%.2f", v);
    std::string s = buf, out;
    const size_t dot = s.find('.');
    for (size_t i = 0; i < dot; i++) {
        if (i && (dot - i) % 3 == 0) out += ',';
        out += s[i];
    }
    return "$" + out + s.substr(dot);
}
std::string withCommas(long long v) {
    std::string s = std::to_string(v), out;
    for (size_t i = 0; i < s.size(); i++) {
        if (i && (s.size() - i) % 3 == 0) out += ',';
        out += s[i];
    }
    return out;
}
// The cloud bill, running through acts 1-2: it climbs faster the longer you use it.
double billAt(int t) { return 0.004 * std::pow((double)t, 2.1); }

// Fades a scene node in over `in` frames at its start (crossfades: the next scene sits on top).
void fadeIn(TNode* n, int in) {
    n->up = [in](int t, NodeProps& p) { p.opacity = (float)interpolate(t, { 0, (double)in }, { 0, 1 }, clamped()); };
}

// ================================================================== act 1: the cloud
void actCloud(TNode* root) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->environmentGenerator = g3d::studioEnvironment(mkColor(0x1c2440), mkColor(0x0a0d18), mkColor(0x040508), 2);
    scene->environmentIntensity = 0.6f;
    scene->fogColor = NIGHT; scene->fogNear = 4; scene->fogFar = 26;

    g3d::Material floor;
    floor.color = mkColor(0x1a1d26); floor.roughness = 0.28f; floor.metalness = 0.3f;
    scene->add(g3d::Geometry::plane(40, 80), floor)->position = { 0, 0, -20 };

    auto rackBox = g3d::Geometry::box(1.0f, 2.4f, 1.1f);
    auto led = g3d::Geometry::box(0.03f, 0.035f, 0.09f);
    g3d::Material rackMat;
    rackMat.map = ASSETS + "/rack.png"; rackMat.roughness = 0.5f; rackMat.metalness = 0.4f;
    for (int side = -1; side <= 1; side += 2)
        for (int i = 0; i < 16; i++) {
            const float z = 2 - i * 1.15f, x = side * 1.9f;
            g3d::Object3D* rack = scene->add(rackBox, rackMat);
            rack->position = { x, 1.2f, z };
            // Six blinking LEDs per rack on the aisle face; unlit, so they read through fog.
            for (int k = 0; k < 6; k++) {
                g3d::Material m;
                m.unlit = true; m.castShadow = false;
                g3d::Object3D* l = scene->add(led, m);
                l->position = { x - side * 0.52f, 0.35f + k * 0.38f, z + (float)rrange(51, i * 13 + k + side * 500, -0.35, 0.35) };
                const unsigned long long seed = (unsigned long long)(i * 31 + k * 7 + (side + 1) * 1000);
                const Color base = k % 3 == 0 ? mkColor(0x4cff9a) : k % 3 == 1 ? mkColor(0x5aa8ff) : mkColor(0xffa040);
                const float dist = std::fabs(z - 2) / 18;
                l->up = [seed, base, dist](int t, g3d::Object3D& o) {
                    const bool on = rnd01(seed, (unsigned long long)(t / 4)) > 0.35;
                    o.material.color = withAlpha(mixC(on ? base : mkColor(0x101010), NIGHT, std::min(0.85f, dist)), 1);
                };
            }
        }
    // Cold strip lights overhead, and a red alarm at the far end that pulses with the beat.
    for (int i = 0; i < 4; i++)
        scene->lights.push_back(g3d::Light::point({ 0, 3.2f, 1 - i * 5.f }, mkColor(0x9fb4ff), 1.3f, 7));
    g3d::Light alarm = g3d::Light::point({ 0, 2.6f, -17 }, ALARM, 3, 14);
    alarm.up = [](int t, g3d::Light& l) { l.intensity = 1.5f + 2.5f * (float)std::pow(0.5 + 0.5 * std::sin(t * 2 * PI / 30), 4); };
    scene->lights.push_back(alarm);
    g3d::Light key = g3d::Light::spot({ 0, 5, 3 }, { 0, 0, -4 }, mkColor(0xc8d4ff), 1.2f, 35, true);
    key.range = 20;
    scene->lights.push_back(key);

    scene->camera.fov = 52;
    scene->camera.up = [](int t, g3d::Camera& cam) {
        const float z = (float)interpolate(t, { 0, 255 }, { 5, -3.5 }, clamped(ease::inOut(ease::sine)));
        cam.position = { 0.25f * std::sin(t * 0.02f), 1.55f + 0.05f * std::sin(t * 0.05f), z };
        cam.target = { 0, 1.5f, z - 6 };
        cam.dof = { 7, 0.6f, 8 };
    };

    TNode* n = root->add(0, A2 - A1);
    n->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1920, 1080, NIGHT); };
    n->add(0, A2 - A1)->fn = g3d::draw(scene);

    // Data leaving home: packets streaming from the viewer into the aisle's vanishing point.
    TNode* packets = n->add(0, A2 - A1);
    packets->isolate().blend = Blend::Add;
    packets->fn = [](const Ctx& cx, int t) {
        const Vector2 vp{ 960, 470 };
        for (int i = 0; i < 70; i++) {
            const double born = i * 3.3;
            double u = std::fmod(t - born, 90.0) / 90.0;
            if (t < born || u < 0) continue;
            u = ease::in(ease::quad)(u);
            const Vector2 from{ (float)rrange(61, i, 300, 1620), (float)rrange(62, i, 1000, 1180) };
            const Vector2 p = mixV(from, vp, u);
            const float size = (float)(10 * (1 - u) + 1.5);
            glow(cx, p, size * 3, COLD, (float)(0.5 * (1 - u)));
            dr::rect(cx, p.x - size / 2, p.y - size / 2, size, size, withAlpha(mkColor(0xcfe0ff), (float)(1 - u)));
        }
    };
}

// The bill, top left, through acts 1 and 2.
void hudBill(TNode* root) {
    TNode* n = root->add(30, A3 - 30);
    n->fn = [](const Ctx& cx, int lt) {
        const int t = lt + 30;
        Ctx m = cx;
        m.a *= fadeWindow(t, 30, A3, 20, 6);
        const bool alarm = t >= A2;
        dr::roundRect(m, 64, 64, 420, 128, 18, withAlpha(mkColor(0x0b0e18), 0.78f));
        dr::roundRect(m, 64, 64, 8, 128, 4, alarm ? ALARM : COLD);
        dr::text(fonts::ui(true), m, "CLOUD BILL  /  THIS MONTH", { 96, 84 }, 22, 3, DIM);
        const double v = billAt(t);
        Color col = alarm ? (t / 8 % 2 ? ALARM : INK) : INK;
        dr::text(fonts::ui(true), m, money(v).c_str(), { 96, 114 }, 52, 1, col);
        const std::string tokens = "tokens sent  " + withCommas((long long)(v * 3100));
        dr::text(fonts::ui(false), m, tokens.c_str(), { 96, 168 }, 18, 1, DIM);
    };
}

// ================================================================== act 2: the strings
void actStrings(TNode* root) {
    struct Stamp { const char* text; Vector2 pos; float rot; };
    static const Stamp stamps[] = {
        { "429  RATE LIMITED", { 600, 330 }, -8 }, { "MODEL RETIRED", { 1330, 270 }, 6 },
        { "TERMS UPDATED", { 760, 560 }, 4 }, { "SERVICE OUTAGE", { 1290, 610 }, -5 },
        { "YOUR DATA: RETAINED", { 990, 440 }, -2 },
    };
    TNode* n = root->add(0, A3 - A2);
    // Screen shake after each slam.
    n->up = [](int t, NodeProps& p) {
        float amp = 0;
        for (int i = 0; i < 5; i++) { const int d = t - i * 30; if (d >= 0 && d < 10) amp = 26.f * (1 - d / 10.f); }
        p.pos = { amp * (float)(rnd01(71, t) - 0.5), amp * (float)(rnd01(72, t) - 0.5) };
    };
    n->fn = [](const Ctx& cx, int t) {
        dr::rectGrad4(cx, -40, -40, 2000, 1160, mkColor(0x22060a), mkColor(0x22060a), mkColor(0x050106), mkColor(0x050106));
        // A perspective grid floor scrolling toward the viewer.
        const Vector2 vp{ 960, 600 };
        for (int i = -14; i <= 14; i++) dr::strokeSeg(cx, vp, { 960 + i * 180.f, 1120 }, 2, withAlpha(ALARM, 0.22f));
        for (int i = 0; i < 12; i++) {
            const float z = std::fmod(i + t * 0.05f, 12.f) + 0.6f;
            const float y = vp.y + 520 / z;
            if (y > 1100) continue;
            dr::strokeSeg(cx, { -40, y }, { 1960, y }, 2, withAlpha(ALARM, 0.25f * std::min(1.f, 3 / z)));
        }
        glow(cx, vp, 900, mkColor(0x7a0d18), 0.45f);
    };
    TNode* st = n->add(0, A3 - A2);
    st->fn = [](const Ctx& cx, int t) {
        const bool glitchy = rnd01(73, t) > 0.82 || (t % 30) < 3;
        for (int i = 0; i < 5; i++) {
            const int land = i * 30, d = t - land;
            if (d < -6) continue;
            const float k = d < 0 ? (float)interpolate(d, { -6, 0 }, { 2.6, 1 }, clamped(ease::in(ease::quad)))
                                  : 1 + 0.06f * std::exp(-d * 0.3f) * std::sin(d * 1.2f);
            Ctx m = cx;
            m.o = mapP(cx, stamps[i].pos);
            m.rot += stamps[i].rot;
            m.s *= k;
            m.a *= d < 0 ? (float)interpolate(d, { -6, 0 }, { 0, 1 }) : 1;
            const Font f = fonts::ui(true);
            const float w = dr::measure(f, stamps[i].text, 58, 4).x + 80;
            dr::roundRect(m, -w / 2 - 6, -56, w + 12, 112, 16, ALARM);
            dr::roundRect(m, -w / 2, -50, w, 100, 12, mkColor(0x1a0507));
            if (glitchy) {
                const float dx = 6 + 10 * (float)rnd01(74, t * 5 + i);
                dr::text(f, m, stamps[i].text, { -dx, 2 }, 58, 4, withAlpha(mkColor(0x00f0ff), 0.7f), 0.5, 0.5);
                dr::text(f, m, stamps[i].text, { dx, -2 }, 58, 4, withAlpha(mkColor(0xff0040), 0.7f), 0.5, 0.5);
            }
            dr::text(f, m, stamps[i].text, { 0, 0 }, 58, 4, mkColor(0xff6a6a), 0.5, 0.5);
        }
        if (glitchy) {                                          // tearing bars
            for (int b = 0; b < 4; b++) {
                const float y = (float)rrange(75, t * 9 + b, 0, 1080), h = (float)rrange(76, t * 9 + b, 3, 34);
                const Color col = b % 2 ? mkColor(0x00f0ff) : mkColor(0xff2050);
                dr::rect(cx, (float)rrange(77, t * 9 + b, -200, 400), y, 1920, h, withAlpha(col, 0.18f));
            }
        }
    };
}

// ================================================================== act 3: the turn
void actTurn(TNode* root) {
    // Target points on a chip: the package outline, pins on every side, a die grid.
    auto targets = std::make_shared<std::vector<Vector2>>();
    const Vector2 C{ 960, 470 };
    for (int i = 0; i < 160; i++) {                              // outline
        const float u = i / 160.f * 4;
        const int side = (int)u;
        const float f = u - side, h = 170;
        const Vector2 corner[4] = { { -h, -h }, { h, -h }, { h, h }, { -h, h } };
        const Vector2 a = corner[side], b = corner[(side + 1) % 4];
        targets->push_back({ C.x + a.x + (b.x - a.x) * f, C.y + a.y + (b.y - a.y) * f });
    }
    for (int s = 0; s < 4; s++)                                  // pins
        for (int i = 0; i < 9; i++)
            for (int k = 0; k < 6; k++) {
                const float along = -136 + i * 34, out = 180 + k * 9;
                const Vector2 v = s == 0 ? Vector2{ along, -out } : s == 1 ? Vector2{ out, along }
                                : s == 2 ? Vector2{ along, out } : Vector2{ -out, along };
                targets->push_back({ C.x + v.x, C.y + v.y });
            }
    for (int j = 0; j < 12; j++)                                 // die
        for (int i = 0; i < 12; i++) targets->push_back({ C.x - 88 + i * 16.f, C.y - 88 + j * 16.f });

    TNode* n = root->add(0, A4 - A3);
    n->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1920, 1080, mkColor(0x020205)); };

    TNode* fx = n->add(0, A4 - A3);
    fx->isolate().blend = Blend::Add;
    fx->fn = [targets, C](const Ctx& cx, int t) {
        // The ember: one warm point of light that the chip forms around.
        const float breathe = 0.6f + 0.4f * (float)std::sin(t * 0.15);
        glow(cx, C, 60 + 30 * breathe, EMBER, 0.6f);
        dr::circle(cx, C, 5, withAlpha(mkColor(0xfff0d0), 1));

        const int NP = (int)targets->size() * 2;
        const float cloudIn = (float)interpolate(t, { 20, 60 }, { 0, 1 }, clamped());
        for (int i = 0; i < NP; i++) {
            const int lobe = i % 5;
            const Vector2 lc{ 960 + (lobe - 2) * 230.f, 190.f + (lobe % 2) * 50 };
            const float ang = (float)rrange(81, i, 0, 2 * PI), rr = (float)std::sqrt(rnd01(82, i));
            Vector2 s{ lc.x + std::cos(ang) * rr * 240, lc.y + std::sin(ang) * rr * 95 };
            s.x += 8 * (float)std::sin(t * 0.05 + i); s.y += 5 * (float)std::cos(t * 0.04 + i * 0.7);
            const Vector2 g = (*targets)[i % targets->size()];
            const double delay = 70 + rnd01(83, i) * 55;
            const double e = interpolate(t, { delay, delay + 48 }, { 0, 1 }, clamped(ease::inOut(ease::cubic)));
            // Swirl sideways mid-flight, so the pour reads as a current rather than a lerp.
            const float swirl = (float)(std::sin(e * PI) * rrange(84, i, -160, 160));
            Vector2 p = mixV(s, g, e);
            p.x += swirl;
            const Color col = mixC(mkColor(0x7d8aa8), e > 0.98 ? TEAL : EMBER, e);
            const float a = cloudIn * (i % 2 && e >= 1 ? 0.35f : 1);
            glow(cx, p, 9, col, 0.22f * a);
            dr::circle(cx, p, 1.8f, withAlpha(col, a));
        }
        // Once formed: traces race out from the pins to the edges of the frame.
        const float traceK = (float)interpolate(t, { 135, 175 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        if (traceK > 0) {
            for (int i = 0; i < 28; i++) {
                const int s = i % 4;
                const float along = (float)rrange(85, i, -130, 130), bend = (float)rrange(86, i, 60, 260);
                const Vector2 dir = s == 0 ? Vector2{ 0, -1 } : s == 1 ? Vector2{ 1, 0 } : s == 2 ? Vector2{ 0, 1 } : Vector2{ -1, 0 };
                const Vector2 side{ -dir.y, dir.x };
                const float sideSign = along < 0 ? -1.f : 1.f;          // fan outwards, never crossing
                const Vector2 p0{ C.x + dir.x * 230 + side.x * along, C.y + dir.y * 230 + side.y * along };
                const Vector2 p1{ p0.x + dir.x * bend, p0.y + dir.y * bend };
                const Vector2 p2{ p1.x + (side.x * sideSign + dir.x) * 400, p1.y + (side.y * sideSign + dir.y) * 400 };
                const float k1 = std::min(1.f, traceK * 2), k2 = std::max(0.f, traceK * 2 - 1);
                const Vector2 e1 = mixV(p0, p1, k1), e2 = mixV(p1, p2, k2);
                dr::strokeSeg(cx, p0, e1, 3, withAlpha(TEAL, 0.8f));
                if (k2 > 0) dr::strokeSeg(cx, p1, e2, 3, withAlpha(TEAL, 0.8f));
                glow(cx, k2 > 0 ? e2 : e1, 18, TEAL, 0.8f);
            }
        }
        // Rings pulse out on completion, then a white-out into the next act.
        for (int r = 0; r < 3; r++) {
            const int d = t - 130 - r * 12;
            if (d < 0 || d > 50) continue;
            dr::ring(cx, C, 200 + d * 14.f, 3, withAlpha(TEAL, 0.8f * (1 - d / 50.f)));
        }
        const float white = (float)interpolate(t, { 166, 180 }, { 0, 1 }, clamped(ease::in(ease::quad)));
        if (white > 0) dr::rect(cx, 0, 0, 1920, 1080, withAlpha(WHITE, white));
    };
}

// ================================================================== act 4: the machine
void actMachine(TNode* root, int len) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->environmentGenerator = g3d::studioEnvironment(mkColor(0x4a3a30), mkColor(0x1c1714), mkColor(0x0a0807), 4);
    scene->environmentIntensity = 0.8f;
    scene->fogColor = mkColor(0x0c0a09); scene->fogNear = 9; scene->fogFar = 22;

    g3d::Material wood;
    wood.map = ASSETS + "/wood.png"; wood.roughness = 0.55f;
    scene->add(g3d::Geometry::box(14, 0.3f, 8), wood)->position = { 0, -0.15f, 0 };

    g3d::Object3D* card = scene->add();
    card->up = [](int t, g3d::Object3D& o) {
        o.position = { 0, 0.55f + 0.04f * std::sin(t * 0.04f), 0 };
        o.rotation = { -8, -18 + t * 0.06f, 0 };
    };
    g3d::Material pcb; pcb.map = ASSETS + "/pcb.png"; pcb.roughness = 0.45f;
    card->add(g3d::Geometry::box(4.2f, 0.07f, 1.7f), pcb);
    g3d::Material back; back.color = mkColor(0x1b1d22); back.roughness = 0.35f; back.metalness = 0.8f;
    card->add(g3d::Geometry::box(4.2f, 0.04f, 1.7f), back)->position = { 0, -0.06f, 0 };
    g3d::Material alu; alu.color = mkColor(0xc4c8d0); alu.roughness = 0.28f; alu.metalness = 0.95f;
    auto fin = g3d::Geometry::box(0.025f, 0.32f, 1.45f);
    for (int i = 0; i < 64; i++) card->add(fin, alu)->position = { -1.95f + i * 0.0615f, 0.2f, 0 };
    g3d::Material shroud; shroud.color = mkColor(0x24262c); shroud.roughness = 0.3f; shroud.metalness = 0.7f;
    card->add(g3d::Geometry::box(4.3f, 0.06f, 1.75f), shroud)->position = { 0, 0.39f, 0 };
    card->add(g3d::Geometry::box(0.12f, 0.9f, 1.9f), alu)->position = { 2.2f, 0.2f, 0 };      // bracket
    // The light strip along the edge: emissive teal that breathes.
    g3d::Material strip; strip.unlit = true; strip.color = TEAL; strip.castShadow = false;
    g3d::Object3D* lightbar = card->add(g3d::Geometry::box(3.6f, 0.035f, 0.035f), strip);
    lightbar->position = { -0.1f, 0.39f, 0.885f };
    lightbar->up = [](int t, g3d::Object3D& o) { o.material.color = mixC(mkColor(0x0b3a39), TEAL, 0.5 + 0.5 * std::sin(t * 0.08)); };

    // Two fans: a torus frame, a hub and nine pitched blades that spin up from rest.
    g3d::Material dark; dark.color = mkColor(0x101114); dark.roughness = 0.45f;
    g3d::Material bladeMat; bladeMat.color = mkColor(0x3a3e48); bladeMat.roughness = 0.35f; bladeMat.metalness = 0.3f;
    auto blade = g3d::Geometry::box(0.6f, 0.014f, 0.24f);
    for (int f = 0; f < 2; f++) {
        g3d::Object3D* frame = card->add(g3d::Geometry::torus(0.74f, 0.045f, 64, 12), shroud);
        frame->position = { -1.0f + f * 2.0f, 0.47f, 0 };
        frame->rotation = { 90, 0, 0 };
        g3d::Object3D* rotor = card->add();
        rotor->position = { -1.0f + f * 2.0f, 0.5f, 0 };
        rotor->up = [f](int t, g3d::Object3D& o) {
            const double tau = 40, w = 24;                    // degrees per frame at speed
            o.rotation.y = (float)(w * (t - tau * (1 - std::exp(-t / tau)))) * (f ? -1.f : 1.f);
        };
        rotor->add(g3d::Geometry::cylinder(0.2f, 0.06f, 32), dark);
        for (int b = 0; b < 9; b++) {
            g3d::Object3D* pivot = rotor->add();
            pivot->rotation = { 0, b * 40.f, 0 };
            g3d::Object3D* bl = pivot->add(blade, bladeMat);
            bl->position = { 0.42f, 0, 0 };
            bl->rotation = { 24, 0, 0 };
        }
    }

    g3d::Light key = g3d::Light::spot({ -4, 7, 4 }, { 0, 0.4f, 0 }, mkColor(0xffd2a0), 3.0f, 32, true);
    key.range = 20;
    scene->lights.push_back(key);
    scene->lights.push_back(g3d::Light::point({ 2, 3.5f, -4 }, TEAL, 1.4f, 9));
    scene->lights.push_back(g3d::Light::point({ -3, 2, -2.5f }, mkColor(0xff9a5a), 1.2f, 8));

    scene->camera.fov = 34;
    scene->camera.up = [](int t, g3d::Camera& cam) {
        const float a = (float)interpolate(t, { 0, 315 }, { -0.5, 0.55 }, clamped(ease::inOut(ease::sine)));
        const float dist = (float)interpolate(t, { 0, 120, 315 }, { 5.0, 7.6, 7.0 }, clamped(ease::inOut(ease::cubic)));
        const float y = (float)interpolate(t, { 0, 315 }, { 2.4, 4.0 }, clamped(ease::inOut(ease::sine)));
        cam.position = { dist * std::sin(a), y, dist * std::cos(a) };
        cam.target = { 0, 0.5f, 0 };
        cam.dof = { Vector3Length(Vector3Subtract(cam.position, cam.target)), 1.4f, 12 };
    };

    TNode* n = root->add(0, len);
    n->fn = [](const Ctx& cx, int) { dr::rectGrad4(cx, 0, 0, 1920, 1080, mkColor(0x1a1411), mkColor(0x1a1411), mkColor(0x060505), mkColor(0x060505)); };
    n->add(0, len)->fn = g3d::draw(scene);
    // Out of the white-out from act 3.
    n->add(0, 20)->fn = [](const Ctx& cx, int t) {
        dr::rect(cx, 0, 0, 1920, 1080, withAlpha(WHITE, (float)interpolate(t, { 0, 20 }, { 1, 0 }, clamped(ease::out(ease::quad)))));
    };
    // Spec chips stack in on the left.
    n->add(150, len - 150)->fn = [](const Ctx& cx, int t) {
        const char* specs[3] = { "24 GB of VRAM on your desk", "0 network hops", "unlimited queries" };
        for (int i = 0; i < 3; i++) {
            const float k = (float)spring(t - i * 10, { (double)FPS, { 1, 120, 14 } });
            if (t - i * 10 < 0) continue;
            Ctx m = cx;
            m.a *= std::min(1.f, k);
            const float x = 96 - (1 - k) * 80, y = 120 + i * 84.f;
            const float w = dr::measure(fonts::ui(true), specs[i], 32, 0).x + 60;
            dr::roundRect(m, x, y, w, 64, 14, withAlpha(mkColor(0x0c0a09), 0.72f));
            dr::circle(m, { x + 26, y + 32 }, 7, TEAL);
            dr::text(fonts::ui(true), m, specs[i], { x + 46, y + 32 }, 32, 0, INK, 0, 0.5);
        }
    };
}

// ================================================================== act 5: the model
void actModel(TNode* root, int len) {
    TNode* n = root->add(0, len);
    fadeIn(n, 15);
    n->fn = [](const Ctx& cx, int t) {
        dr::rectGrad4(cx, 0, 0, 1920, 1080, mkColor(0x07121a), mkColor(0x0b1424), mkColor(0x050810), mkColor(0x04070c));
        // Faint dot grid drifting up.
        for (int j = 0; j < 14; j++)
            for (int i = 0; i < 25; i++) {
                const float y = std::fmod(j * 80 - t * 0.6f + 2000, 1120.f) - 20;
                dr::circle(cx, { 40 + i * 80.f, y }, 1.6f, withAlpha(DIM, 0.18f));
            }
    };
    // The network: four layers whose edges draw in, then carry pulses left to right.
    TNode* net = n->add(0, len);
    net->isolate().blend = Blend::Add;
    net->fn = [](const Ctx& cx, int t) {
        const int counts[4] = { 5, 8, 8, 4 };
        const float xs[4] = { 520, 820, 1120, 1420 };
        auto node = [&](int l, int i) { return Vector2{ xs[l], 560 + (i - (counts[l] - 1) / 2.f) * 68 }; };
        double fire[4][8] = {};                                 // flash level per node
        for (int l = 0; l < 3; l++)
            for (int i = 0; i < counts[l]; i++)
                for (int j = 0; j < counts[l + 1]; j++) {
                    const Vector2 a = node(l, i), b = node(l + 1, j);
                    const unsigned long long id = (unsigned long long)(l * 100 + i * 10 + j);
                    const float drawK = (float)interpolate(t, { 10.0 + l * 14 + rnd01(91, id) * 10, 40.0 + l * 14 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
                    if (drawK <= 0) continue;
                    dr::strokeSeg(cx, a, mixV(a, b, drawK), 1.5f, withAlpha(COLD, 0.16f));
                    // Pulses: each edge carries a pulse every 30 frames, phase-shifted per layer.
                    const double period = 30, phase = std::fmod(t - 60 - l * 10 - rnd01(92, id) * 6, period);
                    if (t < 60 || phase < 0 || phase > 12 || rnd01(93, id * 7 + (unsigned long long)((t - 60) / 30)) < 0.55) continue;
                    const float u = (float)(phase / 12);
                    const Vector2 p = mixV(a, b, u);
                    const Color col = mixC(TEAL, EMBER, l / 2.0);
                    glow(cx, p, 14, col, 0.7f);
                    dr::circle(cx, p, 2.5f, col);
                    if (u > 0.8f) fire[l + 1][j] = std::max(fire[l + 1][j], 1.0);
                }
        for (int l = 0; l < 4; l++)
            for (int i = 0; i < counts[l]; i++) {
                const float k = (float)spring(t - l * 8 - i * 2, { (double)FPS, { 1, 140, 12 } });
                if (t - l * 8 - i * 2 < 0) continue;
                const Vector2 p = node(l, i);
                const double f = l == 0 ? (t >= 60 && (t - 60) % 30 < 6 ? 1.0 : 0.0) : fire[l][i];
                const Color col = l == 3 ? EMBER : TEAL;
                glow(cx, p, 36 * k, col, (float)(0.25 + 0.6 * f));
                dr::circle(cx, p, 13 * k, withAlpha(mkColor(0x0d1b26), 1));
                dr::ring(cx, p, 13 * k, 3, col);
                if (f > 0) dr::circle(cx, p, 7 * k, withAlpha(WHITE, (float)f));
            }
    };
    // The weights download card at the top.
    n->add(0, len)->fn = [](const Ctx& cx, int t) {
        const float k = (float)spring(t - 6, { (double)FPS, { 1, 120, 14 } });
        if (t < 6) return;
        Ctx m = cx;
        m.a *= std::min(1.f, k);
        m.o = mapP(cx, { 960, 150 - (1 - k) * 40 });
        dr::roundRect(m, -420, -70, 840, 140, 20, withAlpha(mkColor(0x0d1826), 0.92f));
        const double p = interpolate(t, { 20, 95 }, { 0, 1 }, clamped(ease::inOut(ease::quad)));
        dr::text(mono(), m, "open-weights-model.gguf", { -380, -38 }, 30, 0, INK);
        char size[32];
        snprintf(size, sizeof size, "%.1f / 4.7 GB", p * 4.7);
        dr::text(mono(), m, size, { 380, -38 }, 26, 0, DIM, 1, 0);
        dr::roundRect(m, -380, 18, 760, 18, 9, withAlpha(DIM, 0.25f));
        dr::roundRect(m, -380, 18, (float)(760 * p), 18, 9, p >= 1 ? TEAL : COLD);
        if (p >= 1) {
            const float d = (float)interpolate(t, { 96, 108 }, { 0, 1 }, clamped(ease::out(ease::back())));
            dr::text(fonts::ui(true), m, "saved to ~/models  -  yours now", { 0, 60 }, 24, 2, withAlpha(TEAL, d), 0.5, 0);
        }
    };
}

// ================================================================== act 6: offline
void actOffline(TNode* root, int len) {
    TNode* n = root->add(0, len);
    fadeIn(n, 15);
    n->fn = [](const Ctx& cx, int) {
        dr::rectGrad4(cx, 0, 0, 1920, 1080, mkColor(0x0b0d14), mkColor(0x0b0d14), mkColor(0x050609), mkColor(0x050609));
    };
    n->add(0, len)->fn = [](const Ctx& cx, int t) {
        // Terminal window.
        const Rectangle win{ 160, 150, 1180, 620 };
        dr::roundRect(cx, win.x + 10, win.y + 18, win.width, win.height, 22, withAlpha(BLACK, 0.5f));
        dr::roundRect(cx, win.x, win.y, win.width, win.height, 22, mkColor(0x11141c));
        dr::roundRect(cx, win.x, win.y, win.width, 56, 22, mkColor(0x1b1f2a));
        dr::rect(cx, win.x, win.y + 34, win.width, 22, mkColor(0x1b1f2a));
        const Color dots[3] = { mkColor(0xff5f57), mkColor(0xfebc2e), mkColor(0x28c840) };
        for (int i = 0; i < 3; i++) dr::circle(cx, { win.x + 34 + i * 30.f, win.y + 28 }, 9, dots[i]);
        dr::text(fonts::ui(false), cx, "~/  local", { win.x + win.width / 2, win.y + 28 }, 22, 1, DIM, 0.5, 0.5);

        struct Line { int at; const char* text; Color col; bool typed; };
        static const Line lines[] = {
            { 8, "$ network off", INK, true },
            { 30, "  wifi: disconnected   ethernet: unplugged", DIM, false },
            { 46, "$ ask \"what did I keep putting off this week?\"", INK, true },
        };
        const Font f = mono();
        float y = win.y + 90;
        for (const Line& l : lines) {
            if (t < l.at) break;
            std::string s = l.text;
            if (l.typed) s = s.substr(0, std::min(s.size(), (size_t)((t - l.at) * 1.6)));
            dr::text(f, cx, s.c_str(), { win.x + 40, y }, 30, 0, l.col);
            y += 50;
        }
        // The answer streams in word by word, a token every ~2 frames.
        static const char* answer =
            "You rescheduled the dentist twice and left the garden plan in drafts. "
            "Both came up on days you said you felt stuck. "
            "Ten minutes on the garden plan tomorrow morning might unstick the rest.";
        const int start = 100;
        std::string shown;
        int tokens = 0;
        if (t >= start) {
            const std::string all = answer;
            const int want = (t - start) / 2;
            size_t pos = 0;
            while (pos < all.size() && tokens < want) {
                size_t sp = all.find(' ', pos + 1);
                if (sp == std::string::npos) sp = all.size();
                pos = sp; tokens++;
            }
            shown = all.substr(0, pos);
        }
        TextStyle st;
        st.font = f; st.size = 30; st.color = mkColor(0xb8f0ec); st.lineHeight = 1.45f;
        dr::textBox(cx, shown, { win.x + 40, y + 10, win.width - 80, 300 }, st);
        if (t / 15 % 2 == 0 || (t >= start && tokens < 40)) {              // cursor
            const std::vector<std::string> wrapped = wrapText(shown, st, win.width - 80);
            const std::string last = wrapped.empty() ? "" : wrapped.back();
            const float cxp = win.x + 40 + dr::measure(f, last.c_str(), 30, 0).x + 6;
            const float cyp = y + 10 + (wrapped.empty() ? 0 : (wrapped.size() - 1) * 30 * 1.45f);
            if (t >= start) dr::rect(cx, cxp, cyp + 2, 16, 32, TEAL);
        }
        // Status bar.
        dr::rect(cx, win.x, win.y + win.height - 54, win.width, 54, mkColor(0x0c0f15));
        char status[128];
        const int tps = t >= start && tokens < 40 ? 38 + (int)(rnd01(95, t / 6) * 9) : 0;
        snprintf(status, sizeof status, "model: local   |   %2d tok/s   |   cost: $0.00   |   network: none", tps);
        dr::text(f, cx, status, { win.x + 40, win.y + win.height - 27 }, 22, 0, DIM, 0, 0.5);

        // The wifi icon going dark, and the bill, now zero.
        const Vector2 ic{ 1640, 330 };
        const float off = (float)interpolate(t, { 20, 34 }, { 0, 1 }, clamped());
        for (int r = 0; r < 3; r++)
            dr::arc(cx, { ic.x, ic.y + 50 }, 40 + r * 38.f, 14, 225, 315, mixC(INK, mkColor(0x2a2f3a), off));
        dr::circle(cx, { ic.x, ic.y + 50 }, 12, mixC(INK, mkColor(0x2a2f3a), off));
        if (off > 0) {
            const float k = (float)interpolate(t, { 30, 42 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
            const Vector2 a{ ic.x - 90, ic.y - 70 }, b{ ic.x + 90, ic.y + 90 };
            dr::strokeSeg(cx, a, mixV(a, b, k), 14, ALARM);
        }
        dr::text(fonts::ui(true), cx, "OFFLINE", { ic.x, ic.y + 120 }, 28, 6, DIM, 0.5, 0);
        dr::text(fonts::ui(true), cx, "CLOUD BILL", { ic.x, ic.y + 230 }, 22, 4, DIM, 0.5, 0);
        dr::text(fonts::ui(true), cx, "$0.00", { ic.x, ic.y + 262 }, 76, 0, TEAL, 0.5, 0);
    };
}

// ================================================================== act 7: the home
void actHome(TNode* root, int len) {
    const Vector3 sunDir = Vector3Normalize({ -0.8f, 0.13f, -0.6f });
    auto scene = std::make_shared<g3d::Scene>();
    scene->environmentGenerator = dawnSky(sunDir);
    scene->environmentIntensity = 1.5f;
    scene->showEnvironment = true;
    scene->fogColor = mkColor(0xe6a68a); scene->fogNear = 50; scene->fogFar = 420;
    scene->shadowMapSize = 2048;

    g3d::Material ground; ground.roughness = 0.9f;
    scene->add(terrainGeometry(110, 180), ground);
    g3d::Material sea; sea.color = mkColor(0x2a5470); sea.roughness = 0.08f; sea.metalness = 0.5f; sea.receiveShadow = false;
    g3d::Object3D* water = scene->add(g3d::Geometry::plane(4000, 4000), sea);
    water->up = [](int t, g3d::Object3D& o) { o.position.y = 0.05f * std::sin(t * 0.03f); };

    // The house.
    g3d::Material wall; wall.color = mkColor(0xf1e6d6); wall.roughness = 0.8f;
    g3d::Material roof; roof.color = mkColor(0x8c3b2e); roof.roughness = 0.6f;
    g3d::Object3D* house = scene->add();
    house->position = { 0, 2, 0 };
    house->rotation = { 0, -40, 0 };
    house->add(g3d::Geometry::box(2.2f, 1.4f, 1.7f), wall)->position = { 0, 0.7f, 0 };
    house->add(roofGeometry(2.5f, 2.0f, 0.9f), roof)->position = { 0, 1.4f, 0 };
    house->add(g3d::Geometry::box(0.3f, 0.7f, 0.3f), roof)->position = { 0.6f, 1.8f, 0.3f };    // chimney
    g3d::Material window; window.unlit = true; window.color = mkColor(0xffcc70); window.castShadow = false;
    for (int i = 0; i < 2; i++) house->add(g3d::Geometry::box(0.4f, 0.4f, 0.04f), window)->position = { -0.55f + i * 1.1f, 0.85f, 0.86f };
    g3d::Material door; door.color = mkColor(0x4a2c1e); door.roughness = 0.7f;
    house->add(g3d::Geometry::box(0.42f, 0.8f, 0.04f), door)->position = { 0, 0.4f, 0.86f };
    // A little server humming beside it, the same teal as the card's light bar.
    g3d::Material box; box.color = mkColor(0x22252c); box.metalness = 0.6f; box.roughness = 0.4f;
    g3d::Object3D* server = house->add(g3d::Geometry::box(0.5f, 0.7f, 0.5f), box);
    server->position = { 1.55f, 0.35f, 0.3f };
    g3d::Material ledMat; ledMat.unlit = true; ledMat.color = TEAL; ledMat.castShadow = false;
    server->add(g3d::Geometry::box(0.32f, 0.04f, 0.02f), ledMat)->position = { 0, 0.15f, 0.26f };
    scene->lights.push_back(g3d::Light::point({ 0, 2.9f, 1.4f }, mkColor(0xffb060), 1.4f, 5));

    // Trees on the gentler grass slopes.
    g3d::Material leaf; leaf.color = mkColor(0x2f5a2a); leaf.roughness = 0.85f;
    g3d::Material bark; bark.color = mkColor(0x5a3d28); bark.roughness = 0.9f;
    auto crown = g3d::Geometry::cone(0.7f, 1.8f, 12), trunk = g3d::Geometry::cylinder(0.12f, 0.6f, 8);
    int planted = 0;
    for (int i = 0; i < 400 && planted < 70; i++) {
        const float x = (float)rrange(101, i, -34, 34), z = (float)rrange(102, i, -34, 30);
        const float y = islandHeight(x, z), r = std::sqrt(x * x + z * z);
        if (y < 1.0f || y > 7.5f || r < 5.5f) continue;
        const float s = (float)rrange(103, i, 0.7, 1.4);
        g3d::Object3D* tr = scene->add();
        tr->position = { x, y, z };
        tr->scale = { s, s, s };
        tr->add(trunk, bark)->position = { 0, 0.3f, 0 };
        tr->add(crown, leaf)->position = { 0, 1.4f, 0 };
        planted++;
    }

    // The privacy dome: a translucent shield that rises over the house, with data orbiting
    // inside it, never leaving.
    g3d::Material domeMat;
    domeMat.color = TEAL; domeMat.opacity = 0.16f; domeMat.emissive = mkColor(0x0c3a3a);
    domeMat.roughness = 0.1f; domeMat.doubleSided = true; domeMat.castShadow = false; domeMat.receiveShadow = false;
    g3d::Object3D* dome = scene->add(domeGeometry(3.2f, 24, 64), domeMat);
    dome->up = [](int t, g3d::Object3D& o) {
        const float k = (float)spring(t - 20, { (double)FPS, { 1, 60, 10 } });
        o.visible = t >= 20;
        o.scale = { k, k * 0.82f, k };
        o.position = { 0, 2, 0 };
    };
    g3d::Material orbMat; orbMat.unlit = true; orbMat.color = mkColor(0xb8fff6); orbMat.castShadow = false;
    auto orbGeo = g3d::Geometry::sphere(0.07f, 8, 12);
    for (int i = 0; i < 14; i++) {
        g3d::Object3D* orb = scene->add(orbGeo, orbMat);
        orb->up = [i](int t, g3d::Object3D& o) {
            const float k = (float)interpolate(t, { 40.0 + i * 3, 60.0 + i * 3 }, { 0, 1 }, clamped());
            const float a = i * 0.9f + t * (0.025f + 0.004f * (i % 4)), tilt = i * 0.7f;
            const float r = 2.3f * k;
            o.visible = k > 0;
            o.position = { r * std::cos(a), 2.0f + 1.2f + 0.9f * std::sin(a * 1.3f + tilt), r * std::sin(a) * std::cos(tilt * 0.3f) };
        };
    }

    g3d::Light sun = g3d::Light::directional(Vector3Scale(sunDir, 60), mkColor(0xffd6a8), 2.8f, true);
    sun.target = { 0, 0, 0 };
    sun.shadowExtent = 26;
    sun.shadowBias = 0.05f;
    scene->lights.push_back(sun);
    // Skylight: a soft, shadowless fill from high on the sun's side, so the hollows aren't black.
    scene->lights.push_back(g3d::Light::directional({ -30, 50, -20 }, mkColor(0xffe2c8), 0.7f));

    scene->camera.fov = 42;
    scene->camera.farPlane = 5000;
    scene->camera.up = [len](int t, g3d::Camera& cam) {
        // Close orbit around the house, then a long crane out to the whole island at dawn.
        const float a = (float)interpolate(t, { 0, 220 }, { -1.2, -0.25 }, clamped(ease::inOut(ease::sine)));   // sunlit side
        const Vector3 near{ 8.5f * std::sin(a), 4.2f, 8.5f * std::cos(a) };
        const Vector3 far{ 26, 17, 40 };
        const float k = (float)interpolate(t, { 190, (double)len - 40 }, { 0, 1 }, clamped(ease::inOut(ease::cubic)));
        cam.position = Vector3Lerp(near, far, k);
        cam.target = Vector3Lerp({ 0, 2.6f, 0 }, { -4, 6.5f, -12 }, k);
        const float focus = Vector3Length(Vector3Subtract(cam.position, { 0, 2.6f, 0 }));
        cam.dof = { focus, 0.8f * (1 - k), 8 };
    };

    TNode* n = root->add(0, len);
    fadeIn(n, 20);
    n->add(0, len)->fn = g3d::draw(scene);

    // Title: letters spring up one by one with a warm glow behind, then the tagline.
    TNode* title = n->add(300, len - 300);
    title->fn = [len](const Ctx& cx, int t) {
        const char* word = "OWN YOUR COMPUTE";
        const Font f = fonts::ui(true, 160);
        const float size = 140, spacing = 10;
        const float total = dr::measure(f, word, size, spacing).x;
        float x = 960 - total / 2;
        Ctx m = cx;
        m.a *= fadeWindow(t + 300, 300, len, 1, 30);
        DrawCircleGradient(mapP(m, { 960, 420 }), 760 * m.s, withAlpha(mkColor(0x120a06), 0.55f * m.a), withAlpha(BLACK, 0));
        for (int i = 0; word[i]; i++) {
            const char ch[2] = { word[i], 0 };
            const float w = dr::measure(f, ch, size, 0).x;
            const float k = (float)spring(t - i * 2, { (double)FPS, { 1, 160, 12 } });
            if (t - i * 2 >= 0 && word[i] != ' ') {
                Ctx l = m;
                l.a *= std::min(1.f, k);
                dr::text(f, l, ch, { x + w / 2 + 3, 424 + (1 - k) * 60 + 4 }, size, 0, withAlpha(BLACK, 0.45f), 0.5, 0.5);
                dr::text(f, l, ch, { x + w / 2, 420 + (1 - k) * 60 }, size, 0, INK, 0.5, 0.5);
            }
            x += w + spacing;
        }
        // A light sweep across the letters.
        const float sweep = (float)interpolate(t, { 40, 80 }, { -300, 2200 }, clamped(ease::inOut(ease::quad)));
        if (t > 40 && t < 80) {
            BeginBlendMode(BLEND_ADDITIVE);
            DrawCircleGradient(mapP(m, { sweep, 420 }), 220 * m.s, withAlpha(mkColor(0xffd9a0), 0.35f * m.a), withAlpha(BLACK, 0));
            restoreBlend();
        }
        const float sub = (float)interpolate(t, { 50, 75 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        Ctx s = m; s.a *= sub;
        dr::text(fonts::ui(true), s, "LOCAL AI.  YOURS TO KEEP.", { 960, 540 + (1 - sub) * 20 }, 44, 12, mkColor(0xffe3bf), 0.5, 0.5);
        const float cred = (float)interpolate(t, { 110, 140 }, { 0, 1 }, clamped());
        Ctx cr = m; cr.a *= cred * 0.85f;
        dr::text(fonts::ui(false), cr, "every mesh, texture and note in this film was generated from code  -  made with raymotion",
                 { 960, 990 }, 24, 1, INK, 0.5, 0.5);
    };
}

// ------------------------------------------------------------------ captions
struct Caption { int from, to; const char* kicker; const char* line; Color accent; };
const Caption CAPTIONS[] = {
    { 20, 115, "THE CLOUD", "Every prompt you write", COLD },
    { 120, 232, nullptr, "is processed on machines you'll never see.", COLD },
    { A3 + 14, A3 + 110, nullptr, "What if it lived with you?", EMBER },
    { A4 + 30, A4 + 140, "OWN THE MACHINE", "Your hardware.", TEAL },
    { A4 + 150, A4 + 292, nullptr, "Models that answer only to you.", TEAL },
    { A5 + 120, A5 + 236, "OWN THE MODEL", "Open weights. Downloaded once. Yours to keep.", TEAL },
    { A6 + 130, A6 + 236, "OWN THE DATA", "No signal. No meter. Still thinking.", TEAL },
    { A7 + 30, A7 + 200, nullptr, "Your data never leaves the room.", TEAL },
};

void captions(Nodes& roots) {
    addRoot(roots, 0, DURATION)->fn = [](const Ctx& cx, int t) {
        for (const Caption& c : CAPTIONS) {
            if (!c.line || t < c.from || t >= c.to) continue;
            const float k = fadeWindow(t, c.from, c.to, 18, 14);
            const float rise = (1 - (float)interpolate(t, { (double)c.from, c.from + 22.0 }, { 0, 1 }, clamped(ease::out(ease::cubic)))) * 26;
            Ctx m = cx;
            m.a *= k;
            dr::rectGrad4(m, 0, 760, 1920, 320, withAlpha(BLACK, 0), withAlpha(BLACK, 0), withAlpha(BLACK, 0.55f), withAlpha(BLACK, 0.55f));
            if (c.kicker) {
                const float w = dr::measure(fonts::ui(true), c.kicker, 24, 8).x;
                dr::text(fonts::ui(true), m, c.kicker, { 960, 850 + rise }, 24, 8, c.accent, 0.5, 0.5);
                dr::rect(m, 960 - w / 2 - 70, 849 + rise, 48, 2, c.accent);
                dr::rect(m, 960 + w / 2 + 22, 849 + rise, 48, 2, c.accent);
            }
            dr::text(fonts::ui(true), m, c.line, { 962, 914 + rise }, 60, 0, withAlpha(BLACK, 0.5f), 0.5, 0.5);
            dr::text(fonts::ui(true), m, c.line, { 960, 910 + rise }, 60, 0, INK, 0.5, 0.5);
        }
    };
}
// The "Powerful. But never really yours." beat in act 2 gets its own, louder treatment.
void strapline(Nodes& roots) {
    addRoot(roots, A2 + 150, 88)->fn = [](const Ctx& cx, int t) {
        const float k = fadeWindow(t, 0, 88, 10, 10);
        Ctx m = cx; m.a *= k;
        dr::rect(m, 0, 820, 1920, 200, withAlpha(BLACK, 0.7f));
        dr::text(fonts::ui(true), m, "Powerful.", { 960, 870 }, 52, 2, INK, 0.5, 0.5);
        const float b = (float)interpolate(t, { 22, 34 }, { 0, 1 }, clamped(ease::out(ease::back())));
        Ctx n = m; n.a *= std::min(1.f, b);
        dr::text(fonts::ui(true), n, "But never really yours.", { 960, 950 }, 64, 2, mkColor(0xff6a6a), 0.5, 0.5);
    };
}

// Film finish over everything: vignette, thin letterbox bars and the fade to black.
void finish(Nodes& roots) {
    addRoot(roots, 0, DURATION)->fn = [](const Ctx& cx, int t) {
        const Vector2 c = mapP(cx, { 960, 540 });
        for (int i = 0; i < 40; i++) {                       // a smooth falloff from thin rings
            const float r0 = (760 + i * 22.f) * cx.s;
            DrawRing(c, r0, r0 + 22 * cx.s + 0.5f, 0, 360, 128, withAlpha(BLACK, cx.a * 0.05f * smooth(0, 40, (float)i)));
        }
        restoreBlend();
        dr::rect(cx, 0, 0, 1920, 44, BLACK);
        dr::rect(cx, 0, 1036, 1920, 44, BLACK);
        const float out = (float)interpolate(t, { DURATION - 36.0, DURATION - 4.0 }, { 0, 1 }, clamped(ease::inOut(ease::quad)));
        if (out > 0) dr::rect(cx, 0, 0, 1920, 1080, withAlpha(BLACK, out));
    };
}

} // namespace

void registerOwnCompute(Registry& out) {
    Comp c;
    c.name = "own-your-compute"; c.w = 1920; c.h = 1080; c.fps = FPS; c.duration = DURATION;
    c.background = NIGHT;
    c.build = [](Comp& c, const Json&) {
        ensureAssets();
        c.roots.clear();
        TNode* film = addRoot(c.roots, 0, DURATION);
        actCloud(film->add(A1, A2 - A1));
        actStrings(film->add(A2, A3 - A2));
        actTurn(film->add(A3, A4 - A3));
        actMachine(film->add(A4, A5 - A4 + 15), A5 - A4 + 15);
        actModel(film->add(A5, A6 - A5 + 15), A6 - A5 + 15);
        actOffline(film->add(A6, A7 - A6 + 20), A7 - A6 + 20);
        actHome(film->add(A7, DURATION - A7), DURATION - A7);
        hudBill(film);
        captions(c.roots);
        strapline(c.roots);
        finish(c.roots);
        media::audio(addRoot(c.roots, 0, DURATION), ASSETS + "/score.wav");
    };
    out.push_back(std::move(c));
}

} // namespace rmtn
