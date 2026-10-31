// splat.cpp -- Gaussian splat clouds: builders, and 3DGS .ply in and out.
#include "rmtn/splat.hpp"
#include "rmtn/timeline.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace rmtn {
namespace splat {

namespace {
constexpr float SH_C0 = 0.28209479177387814f;

// A quaternion (w, x, y, z) turning +z into the unit vector n.
Vector4 turnZTo(Vector3 n) {
    const float d = n.z;
    if (d > 0.99999f) return { 1, 0, 0, 0 };
    if (d < -0.99999f) return { 0, 1, 0, 0 };                // half a turn about x
    const Vector3 a{ -n.y, n.x, 0 };                         // z cross n
    const float w = 1 + d, l = std::sqrt(w * w + a.x * a.x + a.y * a.y);
    return { w / l, a.x / l, a.y / l, 0 };
}
} // namespace

void Cloud::add(Vector3 at, Vector3 sigma, Vector4 q, Color c, float o) {
    const float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    const Vector4 u = l > 1e-8f ? Vector4{ q.x / l, q.y / l, q.z / l, q.w / l } : Vector4{ 1, 0, 0, 0 };   // (w, x, y, z)
    position.insert(position.end(), { at.x, at.y, at.z });
    scale.insert(scale.end(), { std::max(sigma.x, 1e-5f), std::max(sigma.y, 1e-5f), std::max(sigma.z, 1e-5f) });
    rotation.insert(rotation.end(), { u.x, u.y, u.z, u.w });
    opacity.push_back(std::clamp(o, 0.f, 1.f));
    colour.insert(colour.end(), { c.r / 255.f, c.g / 255.f, c.b / 255.f });
    if (!sh.empty()) sh.insert(sh.end(), 9, 0.f);
}

void Cloud::bound() {
    const size_t n = size();
    if (!n) { centre = { 0, 0, 0 }; radius = 0; return; }
    Vector3 lo{ 1e30f, 1e30f, 1e30f }, hi{ -1e30f, -1e30f, -1e30f };
    for (size_t i = 0; i < n; i++) {
        const Vector3 p{ position[i * 3], position[i * 3 + 1], position[i * 3 + 2] };
        lo = Vector3Min(lo, p);
        hi = Vector3Max(hi, p);
    }
    centre = Vector3Scale(Vector3Add(lo, hi), 0.5f);
    float r = 0;
    for (size_t i = 0; i < n; i++) {
        const Vector3 p{ position[i * 3], position[i * 3 + 1], position[i * 3 + 2] };
        const float s = std::max({ scale[i * 3], scale[i * 3 + 1], scale[i * 3 + 2] });
        r = std::max(r, Vector3Distance(centre, p) + 3 * s);
    }
    radius = r;
}

void Cloud::covariance(size_t i, float out[6]) const {
    const float w = rotation[i * 4], x = rotation[i * 4 + 1], y = rotation[i * 4 + 2], z = rotation[i * 4 + 3];
    const float m[3][3] = {                                  // R, rows
        { 1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y) },
        { 2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x) },
        { 2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y) } };
    const float s[3] = { scale[i * 3] * scale[i * 3], scale[i * 3 + 1] * scale[i * 3 + 1], scale[i * 3 + 2] * scale[i * 3 + 2] };
    auto at = [&](int a, int b) { return m[a][0] * s[0] * m[b][0] + m[a][1] * s[1] * m[b][1] + m[a][2] * s[2] * m[b][2]; };
    out[0] = at(0, 0); out[1] = at(0, 1); out[2] = at(0, 2); out[3] = at(1, 1); out[4] = at(1, 2); out[5] = at(2, 2);
}

// ------------------------------------------------------------------ builders
Cloud sphereSurface(Vector3 centre, float radius, int count, const std::function<Color(Vector3, int)>& colour, unsigned seed) {
    Cloud c;
    if (count <= 0 || radius <= 0) return c;
    const float sigma = std::sqrt(4 * PI * radius * radius / count / PI) * 0.85f;
    for (int i = 0; i < count; i++) {
        const float u = (float)rrange(seed, (unsigned long long)i * 2, -1, 1), a = (float)rrange(seed, (unsigned long long)i * 2 + 1, 0, 2 * PI);
        const float r = std::sqrt(std::max(0.f, 1 - u * u));
        const Vector3 n{ r * std::cos(a), r * std::sin(a), u };
        const Vector3 p = Vector3Add(centre, Vector3Scale(n, radius));
        c.add(p, { sigma, sigma, sigma * 0.25f }, turnZTo(n), colour(p, i), 1);
    }
    c.bound();
    return c;
}

Cloud box(Vector3 centre, Vector3 size, int count, const std::function<Color(Vector3, int)>& colour, unsigned seed) {
    Cloud c;
    if (count <= 0) return c;
    const float vol = std::max(size.x * size.y * size.z, 1e-9f), sigma = std::cbrt(vol / count) * 0.5f;
    for (int i = 0; i < count; i++) {
        const Vector3 p{ centre.x + size.x * ((float)rnd01(seed, (unsigned long long)i * 3) - 0.5f),
                         centre.y + size.y * ((float)rnd01(seed, (unsigned long long)i * 3 + 1) - 0.5f),
                         centre.z + size.z * ((float)rnd01(seed, (unsigned long long)i * 3 + 2) - 0.5f) };
        c.add(p, { sigma, sigma, sigma }, { 1, 0, 0, 0 }, colour(p, i), 1);
    }
    c.bound();
    return c;
}

Cloud fromFunction(int count, const std::function<Sample(int)>& fn) {
    Cloud c;
    for (int i = 0; i < count; i++) {
        const Sample s = fn(i);
        c.add(s.position, s.sigma, { 1, 0, 0, 0 }, s.colour, s.opacity);
    }
    c.bound();
    return c;
}

Cloud fromVoxels(const voxel::Grid& g, voxel::MeshOptions o, int perFace, float jitter, unsigned seed) {
    Cloud c;
    perFace = std::max(perFace, 1);
    const Vector3 shift{ o.origin.x - (o.centre ? g.sx * o.size * 0.5f : 0.f), o.origin.y, o.origin.z - (o.centre ? g.sz * o.size * 0.5f : 0.f) };
    const float inPlane = o.size * 0.5f / std::sqrt((float)perFace) * 0.85f, thin = o.size * 0.12f;
    const int side = perFace > 1 ? (int)std::ceil(std::sqrt((float)perFace)) : 1;
    unsigned long long k = 0;
    static const int nx[6] = { -1, 1, 0, 0, 0, 0 }, ny[6] = { 0, 0, -1, 1, 0, 0 }, nz[6] = { 0, 0, 0, 0, -1, 1 };
    for (int z = 0; z < g.sz; z++)
        for (int y = 0; y < g.sy; y++)
            for (int x = 0; x < g.sx; x++) {
                const uint8_t cell = g.get(x, y, z);
                if (!cell) continue;
                const Color col = g.palette[cell];
                for (int f = 0; f < 6; f++) {
                    if (g.get(x + nx[f], y + ny[f], z + nz[f])) continue;
                    const Vector3 n{ (float)nx[f], (float)ny[f], (float)nz[f] };
                    const Vector3 t1 = f < 2 ? Vector3{ 0, 1, 0 } : Vector3{ 1, 0, 0 }, t2 = Vector3CrossProduct(n, t1);
                    for (int s = 0; s < perFace; s++, k++) {
                        const float a = ((float)rnd01(seed, k * 2) - 0.5f) * jitter, b = ((float)rnd01(seed, k * 2 + 1) - 0.5f) * jitter;
                        // The face's centre, then spread over it: one splat sits at the centre, several on a grid.
                        float u = 0, v = 0;
                        if (perFace > 1) {
                            u = ((s % side) + 0.5f) / side - 0.5f;
                            v = ((s / side) + 0.5f) / side - 0.5f;
                        }
                        const Vector3 centreOfFace{ x + 0.5f + n.x * 0.5f, y + 0.5f + n.y * 0.5f, z + 0.5f + n.z * 0.5f };
                        Vector3 p = Vector3Add(centreOfFace, Vector3Add(Vector3Scale(t1, u + a / side), Vector3Scale(t2, v + b / side)));
                        p = { shift.x + p.x * o.size, shift.y + p.y * o.size, shift.z + p.z * o.size };
                        c.add(p, { inPlane, inPlane, thin }, turnZTo(n), col, 0.96f);
                    }
                }
            }
    c.bound();
    return c;
}

Cloud merge(const Cloud& a, const Cloud& b) {
    Cloud c = a;
    auto cat = [](std::vector<float>& x, const std::vector<float>& y) { x.insert(x.end(), y.begin(), y.end()); };
    const bool sh = a.hasSH() || b.hasSH();
    if (sh) {                                                // both carry coefficients, zero where one had none
        if (!a.hasSH()) c.sh.assign(a.size() * 9, 0.f);
    }
    cat(c.position, b.position); cat(c.scale, b.scale); cat(c.rotation, b.rotation);
    cat(c.opacity, b.opacity); cat(c.colour, b.colour);
    if (sh) {
        if (b.hasSH()) cat(c.sh, b.sh); else c.sh.insert(c.sh.end(), b.size() * 9, 0.f);
    }
    c.bound();
    return c;
}

// ------------------------------------------------------------------ .ply
namespace {

struct Prop { std::string name; int size = 4; char kind = 'f'; size_t offset = 0; };   // kind: f float, i signed, u unsigned

bool typeOf(const std::string& t, int& size, char& kind) {
    if (t == "float" || t == "float32") { size = 4; kind = 'f'; return true; }
    if (t == "double" || t == "float64") { size = 8; kind = 'f'; return true; }
    if (t == "char" || t == "int8") { size = 1; kind = 'i'; return true; }
    if (t == "uchar" || t == "uint8") { size = 1; kind = 'u'; return true; }
    if (t == "short" || t == "int16") { size = 2; kind = 'i'; return true; }
    if (t == "ushort" || t == "uint16") { size = 2; kind = 'u'; return true; }
    if (t == "int" || t == "int32") { size = 4; kind = 'i'; return true; }
    if (t == "uint" || t == "uint32") { size = 4; kind = 'u'; return true; }
    return false;
}
double valueOf(const unsigned char* p, const Prop& pr) {
    switch (pr.kind) {
        case 'f': if (pr.size == 4) { float v; std::memcpy(&v, p, 4); return v; } else { double v; std::memcpy(&v, p, 8); return v; }
        case 'i':
            if (pr.size == 1) return (int8_t)p[0];
            if (pr.size == 2) { int16_t v; std::memcpy(&v, p, 2); return v; }
            { int32_t v; std::memcpy(&v, p, 4); return v; }
        default:
            if (pr.size == 1) return p[0];
            if (pr.size == 2) { uint16_t v; std::memcpy(&v, p, 2); return v; }
            { uint32_t v; std::memcpy(&v, p, 4); return v; }
    }
}
float sigmoid(double x) { return (float)(1.0 / (1.0 + std::exp(-x))); }
} // namespace

Cloud loadPly(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("splat: cannot open " + path);
    std::string line;
    std::getline(in, line);
    if (line.substr(0, 3) != "ply") throw std::runtime_error("splat: " + path + " is not a .ply file");
    std::string format;
    size_t count = 0;
    bool inVertex = false, haveVertex = false;
    std::vector<Prop> props;
    size_t row = 0;
    bool headerEnded = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "end_header") { headerEnded = true; break; }
        std::istringstream ss(line);
        std::string word;
        ss >> word;
        if (word == "format") ss >> format;
        else if (word == "element") {
            std::string name;
            long long n = 0;
            ss >> name >> n;
            inVertex = name == "vertex" && !haveVertex;
            if (inVertex) { count = (size_t)std::max(n, 0LL); haveVertex = true; }
        } else if (word == "property" && inVertex) {
            std::string type, name;
            ss >> type;
            if (type == "list") throw std::runtime_error("splat: a list property in the vertex element of " + path);
            ss >> name;
            Prop p;
            p.name = name;
            if (!typeOf(type, p.size, p.kind)) throw std::runtime_error("splat: unknown property type '" + type + "' in " + path);
            p.offset = row;
            row += (size_t)p.size;
            props.push_back(p);
        }
    }
    if (!headerEnded) throw std::runtime_error("splat: " + path + " has no end_header");
    if (format != "binary_little_endian") throw std::runtime_error("splat: only binary_little_endian .ply files load (this one is '" + format + "')");
    if (!haveVertex) throw std::runtime_error("splat: " + path + " has no vertex element");
    std::map<std::string, Prop> byName;
    for (const Prop& p : props) byName[p.name] = p;
    for (const char* need : { "x", "y", "z" })
        if (!byName.count(need)) throw std::runtime_error(std::string("splat: the .ply has no '") + need + "' property");
    const bool full = byName.count("scale_0") && byName.count("rot_0") && byName.count("opacity");
    const bool dc = byName.count("f_dc_0") && byName.count("f_dc_1") && byName.count("f_dc_2");
    const bool rgb = byName.count("red") && byName.count("green") && byName.count("blue");
    int rest = 0;
    while (byName.count("f_rest_" + std::to_string(rest))) rest++;
    const int perChannel = rest / 3;
    const bool sh = dc && perChannel >= 3;

    Cloud c;
    c.position.reserve(count * 3); c.scale.reserve(count * 3); c.rotation.reserve(count * 4);
    c.opacity.reserve(count); c.colour.reserve(count * 3);
    if (sh) c.sh.reserve(count * 9);
    std::vector<unsigned char> block;
    const size_t blockRows = 4096;
    auto get = [&](const unsigned char* r, const char* name, double fallback) {
        auto it = byName.find(name);
        return it == byName.end() ? fallback : valueOf(r + it->second.offset, it->second);
    };
    for (size_t done = 0; done < count;) {
        const size_t n = std::min(blockRows, count - done);
        block.resize(n * row);
        in.read((char*)block.data(), (std::streamsize)block.size());
        if ((size_t)in.gcount() != block.size()) throw std::runtime_error("splat: the .ply ends before its " + std::to_string(count) + " vertices");
        for (size_t i = 0; i < n; i++) {
            const unsigned char* r = &block[i * row];
            c.position.insert(c.position.end(), { (float)get(r, "x", 0), (float)get(r, "y", 0), (float)get(r, "z", 0) });
            if (full) {
                c.scale.insert(c.scale.end(), { (float)std::exp(get(r, "scale_0", -4)), (float)std::exp(get(r, "scale_1", -4)), (float)std::exp(get(r, "scale_2", -4)) });
                float q[4] = { (float)get(r, "rot_0", 1), (float)get(r, "rot_1", 0), (float)get(r, "rot_2", 0), (float)get(r, "rot_3", 0) };
                const float l = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
                if (l > 1e-8f) for (float& v : q) v /= l; else { q[0] = 1; q[1] = q[2] = q[3] = 0; }
                c.rotation.insert(c.rotation.end(), { q[0], q[1], q[2], q[3] });
                c.opacity.push_back(sigmoid(get(r, "opacity", 4)));
            } else {                                         // a plain point cloud: small round splats
                c.scale.insert(c.scale.end(), { 0.01f, 0.01f, 0.01f });
                c.rotation.insert(c.rotation.end(), { 1, 0, 0, 0 });
                c.opacity.push_back(1);
            }
            if (dc) {
                c.colour.insert(c.colour.end(), { (float)(0.5 + SH_C0 * get(r, "f_dc_0", 0)), (float)(0.5 + SH_C0 * get(r, "f_dc_1", 0)),
                                                  (float)(0.5 + SH_C0 * get(r, "f_dc_2", 0)) });
            } else if (rgb) {
                c.colour.insert(c.colour.end(), { (float)(get(r, "red", 255) / 255), (float)(get(r, "green", 255) / 255), (float)(get(r, "blue", 255) / 255) });
            } else c.colour.insert(c.colour.end(), { 0.8f, 0.8f, 0.8f });
            if (sh)
                for (int ch = 0; ch < 3; ch++)
                    for (int k = 0; k < 3; k++) c.sh.push_back((float)get(r, ("f_rest_" + std::to_string(ch * perChannel + k)).c_str(), 0));
        }
        done += n;
    }
    c.bound();
    return c;
}

void savePly(const std::string& path, const Cloud& c) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("splat: cannot write " + path);
    const bool sh = c.hasSH();
    out << "ply\nformat binary_little_endian 1.0\nelement vertex " << c.size() << "\n";
    for (const char* p : { "x", "y", "z", "nx", "ny", "nz", "f_dc_0", "f_dc_1", "f_dc_2" }) out << "property float " << p << "\n";
    if (sh) for (int i = 0; i < 9; i++) out << "property float f_rest_" << i << "\n";
    for (const char* p : { "opacity", "scale_0", "scale_1", "scale_2", "rot_0", "rot_1", "rot_2", "rot_3" }) out << "property float " << p << "\n";
    out << "end_header\n";
    auto put = [&](float v) { out.write((const char*)&v, 4); };
    for (size_t i = 0; i < c.size(); i++) {
        for (int k = 0; k < 3; k++) put(c.position[i * 3 + (size_t)k]);
        for (int k = 0; k < 3; k++) put(0.f);
        for (int k = 0; k < 3; k++) put((c.colour[i * 3 + (size_t)k] - 0.5f) / SH_C0);
        if (sh) for (int k = 0; k < 9; k++) put(c.sh[i * 9 + (size_t)k]);
        const float o = std::clamp(c.opacity[i], 1e-4f, 1 - 1e-4f);
        put(std::log(o / (1 - o)));
        for (int k = 0; k < 3; k++) put(std::log(c.scale[i * 3 + (size_t)k]));
        for (int k = 0; k < 4; k++) put(c.rotation[i * 4 + (size_t)k]);
    }
}

} // namespace splat
} // namespace rmtn
