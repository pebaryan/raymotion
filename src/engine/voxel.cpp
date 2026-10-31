// voxel.cpp -- voxel grids, MagicaVoxel .vox in and out, and the greedy mesher.
#include "rmtn/voxel.hpp"
#include "rmtn/timeline.hpp"
#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace rmtn {
namespace voxel {

// ------------------------------------------------------------------ the grid
namespace {
// A palette for a grid with none: a coarse colour cube, so every index shows something.
void defaultPalette(std::array<Color, 256>& p) {
    p[0] = { 0, 0, 0, 0 };
    for (int i = 1; i < 256; i++) {
        const int k = i - 1;
        if (k < 216) p[(size_t)i] = { (unsigned char)((k / 36) * 51), (unsigned char)(((k / 6) % 6) * 51), (unsigned char)((k % 6) * 51), 255 };
        else {
            const unsigned char g = (unsigned char)((k - 216) * 255 / 39);
            p[(size_t)i] = { g, g, g, 255 };
        }
    }
}
} // namespace

Grid::Grid() { defaultPalette(palette); }
Grid::Grid(int x, int y, int z) : sx(std::max(x, 0)), sy(std::max(y, 0)), sz(std::max(z, 0)) {
    cells.assign((size_t)sx * (size_t)sy * (size_t)sz, 0);
    defaultPalette(palette);
}
uint8_t Grid::get(int x, int y, int z) const {
    return inside(x, y, z) ? cells[(size_t)x + (size_t)sx * ((size_t)y + (size_t)sy * (size_t)z)] : 0;
}
void Grid::set(int x, int y, int z, uint8_t c) {
    if (inside(x, y, z)) cells[(size_t)x + (size_t)sx * ((size_t)y + (size_t)sy * (size_t)z)] = c;
}
void Grid::fill(uint8_t c) { std::fill(cells.begin(), cells.end(), c); }
void Grid::box(int x0, int y0, int z0, int x1, int y1, int z1, uint8_t c) {
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    if (z0 > z1) std::swap(z0, z1);
    for (int z = std::max(z0, 0); z <= std::min(z1, sz - 1); z++)
        for (int y = std::max(y0, 0); y <= std::min(y1, sy - 1); y++)
            for (int x = std::max(x0, 0); x <= std::min(x1, sx - 1); x++) set(x, y, z, c);
}
void Grid::sphere(float cx, float cy, float cz, float r, uint8_t c) {
    const int x0 = (int)std::floor(cx - r), x1 = (int)std::ceil(cx + r);
    const int y0 = (int)std::floor(cy - r), y1 = (int)std::ceil(cy + r);
    const int z0 = (int)std::floor(cz - r), z1 = (int)std::ceil(cz + r);
    for (int z = z0; z <= z1; z++)
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                const float dx = x + 0.5f - cx, dy = y + 0.5f - cy, dz = z + 0.5f - cz;
                if (dx * dx + dy * dy + dz * dz <= r * r) set(x, y, z, c);
            }
}
void Grid::line(int x0, int y0, int z0, int x1, int y1, int z1, uint8_t c) {
    const int n = std::max({ std::abs(x1 - x0), std::abs(y1 - y0), std::abs(z1 - z0), 1 });
    for (int i = 0; i <= n; i++) {
        const float t = (float)i / n;
        set((int)std::lround(x0 + (x1 - x0) * t), (int)std::lround(y0 + (y1 - y0) * t), (int)std::lround(z0 + (z1 - z0) * t), c);
    }
}
int Grid::count() const {
    int n = 0;
    for (uint8_t c : cells) n += c != 0;
    return n;
}

// ------------------------------------------------------------------ .vox
namespace {

struct Reader {
    const std::vector<unsigned char>& d;
    size_t at = 0;
    explicit Reader(const std::vector<unsigned char>& data) : d(data) {}
    size_t left() const { return d.size() - at; }
    void need(size_t n, const char* what) const {
        if (n > left()) throw std::runtime_error(std::string("voxel: the .vox file ends inside ") + what);
    }
    int32_t i32(const char* what = "a value") {
        need(4, what);
        int32_t v;
        std::memcpy(&v, &d[at], 4);
        at += 4;
        return v;
    }
    uint8_t u8(const char* what = "a value") {
        need(1, what);
        return d[at++];
    }
    std::string str(const char* what = "a string") {
        const int32_t n = i32(what);
        if (n < 0) throw std::runtime_error("voxel: a negative string length in the .vox file");
        need((size_t)n, what);
        std::string s((const char*)&d[at], (size_t)n);
        at += (size_t)n;
        return s;
    }
    std::map<std::string, std::string> dict() {
        std::map<std::string, std::string> m;
        const int32_t n = i32("a dictionary");
        if (n < 0 || n > 4096) throw std::runtime_error("voxel: a bad dictionary size in the .vox file");
        for (int i = 0; i < n; i++) {
            std::string k = str("a dictionary key");
            m[k] = str("a dictionary value");
        }
        return m;
    }
};

struct RawModel { int sx = 0, sy = 0, sz = 0; std::vector<std::array<uint8_t, 4>> voxels; };   // file axes, z up
struct Node {
    enum Kind { None, Trn, Grp, Shp } kind = None;
    int child = -1;                              // Trn
    std::vector<int> children;                   // Grp
    std::vector<int> models;                     // Shp
    int tx = 0, ty = 0, tz = 0;                  // Trn frame 0
    uint8_t rot = 4;                             // Trn frame 0: the packed rotation (4 = identity)
    bool hidden = false;
};
struct VoxFile {
    std::vector<RawModel> models;
    std::map<int, Node> nodes;
    std::array<Color, 256> palette;
    bool hasPalette = false;
};

std::vector<unsigned char> slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("voxel: cannot open " + path);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void parseTransform(const std::map<std::string, std::string>& frame, Node& n) {
    auto r = frame.find("_r");
    if (r != frame.end()) n.rot = (uint8_t)std::atoi(r->second.c_str());
    auto t = frame.find("_t");
    if (t != frame.end()) {
        std::istringstream ss(t->second);
        ss >> n.tx >> n.ty >> n.tz;
    }
}

VoxFile parse(const std::string& path) {
    const std::vector<unsigned char> data = slurp(path);
    Reader r(data);
    if (data.size() < 8 || std::memcmp(data.data(), "VOX ", 4) != 0) throw std::runtime_error("voxel: " + path + " is not a .vox file");
    r.at = 4;
    const int32_t version = r.i32("the header");
    if (version < 150 || version > 400) throw std::runtime_error("voxel: unsupported .vox version " + std::to_string(version));
    VoxFile f;
    // The chunks: a header (id, bytes of content, bytes of children), content, children. MAIN
    // holds the rest as children, so reading continues straight through.
    auto chunk = [&](auto&& self, size_t end) -> void {
        while (r.at + 12 <= end) {
            char id[5] = {};
            std::memcpy(id, &data[r.at], 4);
            r.at += 4;
            const int32_t content = r.i32("a chunk header"), children = r.i32("a chunk header");
            if (content < 0 || children < 0 || (size_t)content > r.left()) throw std::runtime_error("voxel: a chunk of the .vox file runs past its end");
            const size_t body = r.at, bodyEnd = body + (size_t)content;
            const std::string name = id;
            if (name == "SIZE") {
                RawModel m;
                m.sx = r.i32("SIZE"); m.sy = r.i32("SIZE"); m.sz = r.i32("SIZE");
                if (m.sx <= 0 || m.sy <= 0 || m.sz <= 0 || m.sx > 4096 || m.sy > 4096 || m.sz > 4096)
                    throw std::runtime_error("voxel: a model of unreasonable size in the .vox file");
                f.models.push_back(std::move(m));
            } else if (name == "XYZI") {
                if (f.models.empty()) throw std::runtime_error("voxel: XYZI before SIZE in the .vox file");
                const int32_t n = r.i32("XYZI");
                if (n < 0 || (size_t)n * 4 > bodyEnd - r.at) throw std::runtime_error("voxel: XYZI holds fewer voxels than it says");
                RawModel& m = f.models.back();
                m.voxels.resize((size_t)n);
                for (int32_t i = 0; i < n; i++) {
                    m.voxels[(size_t)i] = { r.u8(), r.u8(), r.u8(), r.u8() };
                    const auto& v = m.voxels[(size_t)i];
                    if (v[0] >= m.sx || v[1] >= m.sy || v[2] >= m.sz) throw std::runtime_error("voxel: a voxel lies outside its model in the .vox file");
                }
            } else if (name == "RGBA") {
                if (content < 1024) throw std::runtime_error("voxel: the palette of the .vox file is short");
                f.palette[0] = { 0, 0, 0, 0 };
                for (int i = 1; i < 256; i++) {            // entry i of the file is colour index i + 1
                    const unsigned char c[4] = { r.u8(), r.u8(), r.u8(), r.u8() };
                    f.palette[(size_t)i] = { c[0], c[1], c[2], 255 };
                }
                f.hasPalette = true;
            } else if (name == "nTRN") {
                Node n; n.kind = Node::Trn;
                const int id2 = r.i32("nTRN");
                const auto attrs = r.dict();
                n.hidden = attrs.count("_hidden") && attrs.at("_hidden") == "1";
                n.child = r.i32("nTRN");
                r.i32("nTRN"); r.i32("nTRN");              // reserved, layer
                const int frames = r.i32("nTRN");
                for (int i = 0; i < frames; i++) {
                    const auto fr = r.dict();
                    if (i == 0) parseTransform(fr, n);
                }
                f.nodes[id2] = n;
            } else if (name == "nGRP") {
                Node n; n.kind = Node::Grp;
                const int id2 = r.i32("nGRP");
                const auto attrs = r.dict();
                n.hidden = attrs.count("_hidden") && attrs.at("_hidden") == "1";
                const int count = r.i32("nGRP");
                if (count < 0 || count > 100000) throw std::runtime_error("voxel: a bad group size in the .vox file");
                for (int i = 0; i < count; i++) n.children.push_back(r.i32("nGRP"));
                f.nodes[id2] = n;
            } else if (name == "nSHP") {
                Node n; n.kind = Node::Shp;
                const int id2 = r.i32("nSHP");
                r.dict();
                const int count = r.i32("nSHP");
                if (count < 0 || count > 100000) throw std::runtime_error("voxel: a bad shape size in the .vox file");
                for (int i = 0; i < count; i++) {
                    n.models.push_back(r.i32("nSHP"));
                    r.dict();
                }
                f.nodes[id2] = n;
            }
            r.at = bodyEnd;                                // skip what was not read (and unknown chunks)
            if (children > 0) {
                if ((size_t)children > r.left()) throw std::runtime_error("voxel: the children of a chunk run past the end of the .vox file");
                self(self, r.at + (size_t)children);
            }
        }
    };
    defaultPalette(f.palette);
    chunk(chunk, data.size());
    if (f.models.empty()) throw std::runtime_error("voxel: the .vox file holds no model");
    return f;
}

// A packed rotation byte as a 3x3 integer matrix (rows), so world = M * local.
std::array<std::array<int, 3>, 3> rotationOf(uint8_t r) {
    const int i0 = r & 3, i1 = (r >> 2) & 3;
    if (i0 > 2 || i1 > 2 || i0 == i1) throw std::runtime_error("voxel: an invalid rotation in the .vox file");
    const int i2 = 3 - i0 - i1;
    std::array<std::array<int, 3>, 3> m{};
    m[0][(size_t)i0] = (r & 16) ? -1 : 1;
    m[1][(size_t)i1] = (r & 32) ? -1 : 1;
    m[2][(size_t)i2] = (r & 64) ? -1 : 1;
    return m;
}

// File axes (x, y, z up, right-handed) to ours (x, y up, z): (x, z, -y).
struct Cell { int x, y, z; };
Cell toScene(int fx, int fy, int fz) { return { fx, fz, -fy }; }

// Every model instance of the scene graph, as integer-placed voxel lists in our axes.
struct Instance { int model; int rot; int tx, ty, tz; };
void walk(const VoxFile& f, int id, std::array<std::array<int, 3>, 3> m, int tx, int ty, int tz, int depth, std::vector<std::pair<int, std::pair<std::array<std::array<int, 3>, 3>, std::array<int, 3>>>>& out) {
    if (depth > 64) throw std::runtime_error("voxel: the scene graph of the .vox file is too deep");
    auto it = f.nodes.find(id);
    if (it == f.nodes.end()) return;
    const Node& n = it->second;
    if (n.hidden) return;
    if (n.kind == Node::Trn) {
        // This node's rotation applies to everything below it, and its translation is in the parent's frame.
        const auto rm = rotationOf(n.rot);
        std::array<std::array<int, 3>, 3> nm{};
        for (size_t i = 0; i < 3; i++)
            for (size_t j = 0; j < 3; j++)
                for (size_t k = 0; k < 3; k++) nm[i][j] += m[i][k] * rm[k][j];
        const int ntx = tx + m[0][0] * n.tx + m[0][1] * n.ty + m[0][2] * n.tz;
        const int nty = ty + m[1][0] * n.tx + m[1][1] * n.ty + m[1][2] * n.tz;
        const int ntz = tz + m[2][0] * n.tx + m[2][1] * n.ty + m[2][2] * n.tz;
        walk(f, n.child, nm, ntx, nty, ntz, depth + 1, out);
    } else if (n.kind == Node::Grp) {
        for (int c : n.children) walk(f, c, m, tx, ty, tz, depth + 1, out);
    } else if (n.kind == Node::Shp && !n.models.empty()) {
        out.push_back({ n.models[0], { m, { tx, ty, tz } } });     // the first model: later ones are animation frames
    }
}

} // namespace

std::vector<Grid> loadModels(const std::string& path) {
    const VoxFile f = parse(path);
    std::vector<Grid> out;
    for (const RawModel& m : f.models) {
        Grid g(m.sx, m.sz, m.sy);                              // file (x, y, z) -> (x, z, y flipped)
        g.palette = f.palette;
        for (const auto& v : m.voxels) g.set(v[0], v[2], m.sy - 1 - v[1], v[3]);
        out.push_back(std::move(g));
    }
    return out;
}

std::vector<Placed> loadScene(const std::string& path) {
    const VoxFile f = parse(path);
    // The root is the node nothing else points to; files without a graph place each model at the origin.
    std::vector<std::pair<int, std::pair<std::array<std::array<int, 3>, 3>, std::array<int, 3>>>> placed;
    if (!f.nodes.empty()) {
        std::map<int, bool> child;
        for (const auto& [id, n] : f.nodes) {
            if (n.kind == Node::Trn) child[n.child] = true;
            for (int c : n.children) child[c] = true;
        }
        std::array<std::array<int, 3>, 3> identity{};
        identity[0][0] = identity[1][1] = identity[2][2] = 1;
        for (const auto& [id, n] : f.nodes)
            if (!child.count(id)) walk(f, id, identity, 0, 0, 0, 0, placed);
    } else {
        std::array<std::array<int, 3>, 3> identity{};
        identity[0][0] = identity[1][1] = identity[2][2] = 1;
        for (size_t i = 0; i < f.models.size(); i++) placed.push_back({ (int)i, { identity, { 0, 0, 0 } } });
    }
    std::vector<Placed> out;
    for (const auto& [model, tr] : placed) {
        if (model < 0 || (size_t)model >= f.models.size()) throw std::runtime_error("voxel: a shape of the .vox file names a model that is not there");
        const RawModel& m = f.models[(size_t)model];
        const auto& rm = tr.first;
        const auto& t = tr.second;
        // A voxel's centre sits at its cell + 0.5; the model's pivot is its size / 2 (integer division).
        std::vector<std::pair<Cell, uint8_t>> cells;
        cells.reserve(m.voxels.size());
        Cell lo{ 1 << 30, 1 << 30, 1 << 30 }, hi{ -(1 << 30), -(1 << 30), -(1 << 30) };
        auto place = [&](int fx, int fy, int fz) {
            const int lx = fx - m.sx / 2, ly = fy - m.sy / 2, lz = fz - m.sz / 2;
            return toScene(rm[0][0] * lx + rm[0][1] * ly + rm[0][2] * lz + t[0],
                           rm[1][0] * lx + rm[1][1] * ly + rm[1][2] * lz + t[1],
                           rm[2][0] * lx + rm[2][1] * ly + rm[2][2] * lz + t[2]);
        };
        for (const auto& v : m.voxels) cells.push_back({ place(v[0], v[1], v[2]), v[3] });
        if (cells.empty()) continue;
        // The grid covers the whole model (its corners), not only the cells that are set.
        for (int cx : { 0, m.sx - 1 })
            for (int cy : { 0, m.sy - 1 })
                for (int cz : { 0, m.sz - 1 }) {
                    const Cell c = place(cx, cy, cz);
                    lo = { std::min(lo.x, c.x), std::min(lo.y, c.y), std::min(lo.z, c.z) };
                    hi = { std::max(hi.x, c.x), std::max(hi.y, c.y), std::max(hi.z, c.z) };
                }
        Placed p;
        p.grid = Grid(hi.x - lo.x + 1, hi.y - lo.y + 1, hi.z - lo.z + 1);
        p.grid.palette = f.palette;
        p.ox = lo.x; p.oy = lo.y; p.oz = lo.z;
        for (const auto& [c, idx] : cells) p.grid.set(c.x - lo.x, c.y - lo.y, c.z - lo.z, idx);
        out.push_back(std::move(p));
    }
    return out;
}

Grid load(const std::string& path) {
    const std::vector<Placed> parts = loadScene(path);
    if (parts.empty()) throw std::runtime_error("voxel: the .vox file places no voxels");
    int x0 = parts[0].ox, y0 = parts[0].oy, z0 = parts[0].oz, x1 = x0, y1 = y0, z1 = z0;
    for (const Placed& p : parts) {
        x0 = std::min(x0, p.ox); y0 = std::min(y0, p.oy); z0 = std::min(z0, p.oz);
        x1 = std::max(x1, p.ox + p.grid.sx); y1 = std::max(y1, p.oy + p.grid.sy); z1 = std::max(z1, p.oz + p.grid.sz);
    }
    Grid g(x1 - x0, y1 - y0, z1 - z0);
    g.palette = parts[0].grid.palette;
    for (const Placed& p : parts)
        for (int z = 0; z < p.grid.sz; z++)
            for (int y = 0; y < p.grid.sy; y++)
                for (int x = 0; x < p.grid.sx; x++)
                    if (const uint8_t c = p.grid.get(x, y, z)) g.set(p.ox + x - x0, p.oy + y - y0, p.oz + z - z0, c);
    return g;
}

void save(const std::string& path, const Grid& g) {
    if (g.sx <= 0 || g.sy <= 0 || g.sz <= 0) throw std::runtime_error("voxel: cannot save an empty grid");
    if (g.sx > 256 || g.sy > 256 || g.sz > 256) throw std::runtime_error("voxel: a .vox model is at most 256 cells a side");
    std::vector<unsigned char> body;
    auto put32 = [&](std::vector<unsigned char>& b, int32_t v) {
        const unsigned char* p = (const unsigned char*)&v;
        b.insert(b.end(), p, p + 4);
    };
    auto chunk = [&](const char* id, const std::vector<unsigned char>& content) {
        body.insert(body.end(), id, id + 4);
        put32(body, (int32_t)content.size());
        put32(body, 0);
        body.insert(body.end(), content.begin(), content.end());
    };
    std::vector<unsigned char> size, xyzi, rgba;
    put32(size, g.sx); put32(size, g.sz); put32(size, g.sy);   // file (x, y, z up) = ours (x, z flipped, y)
    std::vector<std::array<uint8_t, 4>> voxels;
    for (int z = 0; z < g.sz; z++)
        for (int y = 0; y < g.sy; y++)
            for (int x = 0; x < g.sx; x++)
                if (const uint8_t c = g.get(x, y, z)) voxels.push_back({ (uint8_t)x, (uint8_t)(g.sz - 1 - z), (uint8_t)y, c });
    put32(xyzi, (int32_t)voxels.size());
    for (const auto& v : voxels) xyzi.insert(xyzi.end(), v.begin(), v.end());
    for (int i = 1; i <= 256; i++) {
        const Color c = i < 256 ? g.palette[(size_t)i] : Color{ 0, 0, 0, 255 };
        rgba.insert(rgba.end(), { c.r, c.g, c.b, (unsigned char)255 });
    }
    // MAIN's children are the three chunks.
    std::vector<unsigned char> kids;
    std::swap(body, kids);
    chunk("SIZE", size);
    chunk("XYZI", xyzi);
    chunk("RGBA", rgba);
    std::swap(body, kids);                                     // kids = the three chunks
    std::vector<unsigned char> file = { 'V', 'O', 'X', ' ' };
    put32(file, 150);
    file.insert(file.end(), { 'M', 'A', 'I', 'N' });
    put32(file, 0);
    put32(file, (int32_t)kids.size());
    file.insert(file.end(), kids.begin(), kids.end());
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("voxel: cannot write " + path);
    out.write((const char*)file.data(), (std::streamsize)file.size());
}

// ------------------------------------------------------------------ the greedy mesh
std::vector<Quad> quads(const Grid& g, bool ambient) {
    std::vector<Quad> out;
    const int dims[3] = { g.sx, g.sy, g.sz };
    for (int d = 0; d < 3; d++) {
        const int u = (d + 1) % 3, v = (d + 2) % 3;          // u x v = d, so (u0v0, u1v0, u1v1, u0v1) runs counter-clockwise from +d
        const int nu = dims[u], nv = dims[v];
        std::vector<uint32_t> mask((size_t)nu * (size_t)nv);
        for (int dir = 0; dir < 2; dir++) {                  // 0: faces towards -d, 1: towards +d
            for (int layer = 0; layer < dims[d]; layer++) {
                std::fill(mask.begin(), mask.end(), 0u);
                auto at = [&](int ld, int lu, int lv) {
                    int p[3];
                    p[d] = ld; p[u] = lu; p[v] = lv;
                    return g.get(p[0], p[1], p[2]);
                };
                for (int j = 0; j < nv; j++)
                    for (int i = 0; i < nu; i++) {
                        const uint8_t c = at(layer, i, j);
                        if (!c) continue;
                        const int front = layer + (dir ? 1 : -1);
                        if (at(front, i, j)) continue;       // hidden between two solid cells
                        uint8_t ao[4] = { 3, 3, 3, 3 };
                        if (ambient) {
                            static const int su[4] = { -1, 1, 1, -1 }, sv[4] = { -1, -1, 1, 1 };
                            for (int k = 0; k < 4; k++) {
                                const int s1 = at(front, i + su[k], j) != 0, s2 = at(front, i, j + sv[k]) != 0;
                                const int cc = at(front, i + su[k], j + sv[k]) != 0;
                                ao[k] = (uint8_t)((s1 && s2) ? 0 : 3 - (s1 + s2 + cc));
                            }
                        }
                        const bool flat = ao[0] == ao[1] && ao[1] == ao[2] && ao[2] == ao[3];
                        // Merge only where the corners are occluded alike; the rest stay single cells.
                        uint32_t key = 1u << 24 | (uint32_t)c << 8;
                        key |= flat ? (1u << 25 | ao[0]) : (uint32_t)(ao[0] | ao[1] << 2 | ao[2] << 4 | ao[3] << 6);
                        mask[(size_t)j * (size_t)nu + (size_t)i] = key;
                    }
                for (int j = 0; j < nv; j++)
                    for (int i = 0; i < nu;) {
                        const uint32_t key = mask[(size_t)j * (size_t)nu + (size_t)i];
                        if (!key) { i++; continue; }
                        int w = 1, h = 1;
                        if (key & (1u << 25)) {
                            while (i + w < nu && mask[(size_t)j * (size_t)nu + (size_t)(i + w)] == key) w++;
                            for (bool grow = true; grow && j + h < nv;) {
                                for (int k = 0; k < w; k++)
                                    if (mask[(size_t)(j + h) * (size_t)nu + (size_t)(i + k)] != key) { grow = false; break; }
                                if (grow) h++;
                            }
                        }
                        for (int jj = 0; jj < h; jj++)
                            for (int ii = 0; ii < w; ii++) mask[(size_t)(j + jj) * (size_t)nu + (size_t)(i + ii)] = 0;
                        Quad q;
                        q.axis = d;
                        q.positive = dir == 1;
                        q.colour = (uint8_t)((key >> 8) & 255);
                        const float plane = (float)(layer + (dir ? 1 : 0));
                        const float us[4] = { (float)i, (float)(i + w), (float)(i + w), (float)i };
                        const float vs[4] = { (float)j, (float)j, (float)(j + h), (float)(j + h) };
                        uint8_t ao[4];
                        if (key & (1u << 25)) ao[0] = ao[1] = ao[2] = ao[3] = (uint8_t)(key & 3);
                        else for (int k = 0; k < 4; k++) ao[k] = (uint8_t)((key >> (2 * k)) & 3);
                        for (int k = 0; k < 4; k++) {
                            // Faces towards -d run the other way round to stay counter-clockwise from outside.
                            const int s = dir ? k : (4 - k) % 4;
                            float p[3];
                            p[d] = plane; p[u] = us[s]; p[v] = vs[s];
                            q.corner[(size_t)k] = { p[0], p[1], p[2] };
                            q.ao[(size_t)k] = ao[s];
                        }
                        out.push_back(q);
                        i += w;
                    }
            }
        }
    }
    return out;
}

namespace {
// Vertex colour of a palette entry darkened by corner occlusion.
Color shaded(Color c, int ao, float strength) {
    static const float level[4] = { 0.0f, 0.45f, 0.72f, 1.0f };
    const float k = 1.f - strength * (1.f - level[std::clamp(ao, 0, 3)]);
    return { (unsigned char)std::lround(c.r * k), (unsigned char)std::lround(c.g * k), (unsigned char)std::lround(c.b * k), c.a };
}
} // namespace

g3d::GeometryPtr mesh(const Grid& grid, MeshOptions opts, MeshStats* stats) {
    auto g = std::make_shared<Grid>(grid);                    // the geometry builds later, so it keeps its own copy
    const std::vector<Quad> qs = quads(*g, opts.ao);
    if (stats) {
        *stats = {};
        stats->voxels = g->count();
        stats->quads = (int)qs.size();
        stats->triangles = (int)qs.size() * 2;
        int faces = 0;
        for (int z = 0; z < g->sz; z++)
            for (int y = 0; y < g->sy; y++)
                for (int x = 0; x < g->sx; x++) {
                    if (!g->get(x, y, z)) continue;
                    faces += !g->get(x - 1, y, z) + !g->get(x + 1, y, z) + !g->get(x, y - 1, z) + !g->get(x, y + 1, z) +
                             !g->get(x, y, z - 1) + !g->get(x, y, z + 1);
                }
        stats->faces = faces;
    }
    auto list = std::make_shared<std::vector<Quad>>(qs);
    return std::make_shared<g3d::Geometry>([g, list, opts] {
        const std::vector<Quad>& q = *list;
        const bool indexed = q.size() * 4 <= 65535;
        Mesh m = {};
        m.vertexCount = (int)q.size() * (indexed ? 4 : 6);
        m.triangleCount = (int)q.size() * 2;
        if (m.vertexCount == 0) {                              // an empty grid: one degenerate triangle keeps it valid
            m.vertexCount = 3; m.triangleCount = 1;
            m.vertices = (float*)MemAlloc(9 * sizeof(float)); m.normals = (float*)MemAlloc(9 * sizeof(float));
            m.texcoords = (float*)MemAlloc(6 * sizeof(float)); m.colors = (unsigned char*)MemAlloc(12);
            std::memset(m.vertices, 0, 9 * sizeof(float)); std::memset(m.normals, 0, 9 * sizeof(float));
            std::memset(m.texcoords, 0, 6 * sizeof(float)); std::memset(m.colors, 0, 12);
            UploadMesh(&m, false);
            return m;
        }
        m.vertices = (float*)MemAlloc((unsigned)m.vertexCount * 3 * sizeof(float));
        m.normals = (float*)MemAlloc((unsigned)m.vertexCount * 3 * sizeof(float));
        m.texcoords = (float*)MemAlloc((unsigned)m.vertexCount * 2 * sizeof(float));
        m.colors = (unsigned char*)MemAlloc((unsigned)m.vertexCount * 4);
        if (indexed) m.indices = (unsigned short*)MemAlloc((unsigned)m.triangleCount * 3 * sizeof(unsigned short));
        const Vector3 shift{ opts.origin.x - (opts.centre ? g->sx * opts.size * 0.5f : 0.f), opts.origin.y,
                             opts.origin.z - (opts.centre ? g->sz * opts.size * 0.5f : 0.f) };
        int vi = 0, ii = 0;
        auto vertex = [&](const Quad& qd, int k) {
            const Vector3 p = qd.corner[(size_t)k];
            m.vertices[vi * 3] = shift.x + p.x * opts.size;
            m.vertices[vi * 3 + 1] = shift.y + p.y * opts.size;
            m.vertices[vi * 3 + 2] = shift.z + p.z * opts.size;
            float n[3] = { 0, 0, 0 };
            n[qd.axis] = qd.positive ? 1.f : -1.f;
            m.normals[vi * 3] = n[0]; m.normals[vi * 3 + 1] = n[1]; m.normals[vi * 3 + 2] = n[2];
            m.texcoords[vi * 2] = (k == 1 || k == 2) ? 1.f : 0.f;
            m.texcoords[vi * 2 + 1] = (k >= 2) ? 1.f : 0.f;
            const Color c = shaded(g->palette[qd.colour], opts.ao ? qd.ao[(size_t)k] : 3, opts.aoStrength);
            m.colors[vi * 4] = c.r; m.colors[vi * 4 + 1] = c.g; m.colors[vi * 4 + 2] = c.b; m.colors[vi * 4 + 3] = c.a;
            return vi++;
        };
        for (const Quad& qd : q) {
            // The diagonal joins the corners whose occlusion adds up higher, so the dark corner stays in one triangle.
            const bool diag02 = qd.ao[0] + qd.ao[2] > qd.ao[1] + qd.ao[3];
            const int order[6] = { diag02 ? 0 : 1, diag02 ? 1 : 2, diag02 ? 2 : 3, diag02 ? 0 : 1, diag02 ? 2 : 3, diag02 ? 3 : 0 };
            if (indexed) {
                const int base = vi;
                for (int k = 0; k < 4; k++) vertex(qd, k);
                for (int k = 0; k < 6; k++) m.indices[ii++] = (unsigned short)(base + order[k]);
            } else {
                for (int k = 0; k < 6; k++) vertex(qd, order[k]);
            }
        }
        UploadMesh(&m, false);
        return m;
    });
}

g3d::Object3D* addFrames(g3d::Scene& scene, const std::vector<Grid>& frames, const g3d::Material& m, int hold, MeshOptions opts) {
    if (frames.empty()) throw std::runtime_error("voxel::addFrames: no frames");
    g3d::Object3D* group = scene.add();
    std::vector<g3d::Object3D*> kids;
    for (const Grid& f : frames) kids.push_back(group->add(mesh(f, opts), m));
    const int n = (int)frames.size(), step = std::max(hold, 1);
    group->up = [kids, n, step](int t, g3d::Object3D&) {
        const int shown = ((std::max(t, 0) / step) % n + n) % n;
        for (size_t i = 0; i < kids.size(); i++) kids[i]->visible = (int)i == shown;
    };
    return group;
}

} // namespace voxel
} // namespace rmtn
