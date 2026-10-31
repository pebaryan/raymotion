// voxel_tests.cpp -- voxel grids, .vox files, the greedy mesher, and meshes drawn by the renderer.
#include "rmtn/raymotion.hpp"
#include <raymath.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <string>

using namespace rmtn;
using Set3 = std::set<std::array<int, 3>>;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static int quadCount(const voxel::Grid& g, bool ao = false) { return (int)voxel::quads(g, ao).size(); }
static int exposedFaces(const voxel::Grid& g) {
    voxel::MeshStats s;
    voxel::mesh(g, {}, &s);
    return s.faces;
}

// ---- the grid and the greedy mesher (no GPU)
static void meshing() {
    {   // one voxel: six faces
        voxel::Grid g(3, 3, 3);
        g.set(1, 1, 1, 1);
        CHECK(quadCount(g) == 6 && exposedFaces(g) == 6 && g.count() == 1);
    }
    {   // a solid block is six quads however big
        voxel::Grid g(9, 7, 5);
        g.fill(2);
        CHECK(quadCount(g) == 6 && exposedFaces(g) == 2 * (9 * 7 + 9 * 5 + 7 * 5));
    }
    {   // two neighbours: the face between them is gone, the rest merge
        voxel::Grid g(4, 3, 3);
        g.set(1, 1, 1, 1);
        g.set(2, 1, 1, 1);
        CHECK(exposedFaces(g) == 10 && quadCount(g) == 6);
    }
    {   // two colours do not merge
        voxel::Grid g(4, 3, 3);
        g.set(1, 1, 1, 1);
        g.set(2, 1, 1, 2);
        CHECK(exposedFaces(g) == 10 && quadCount(g) == 10);
    }
    {   // a hollow shell keeps its cavity's faces and loses none it should not
        voxel::Grid g(6, 6, 6);
        g.box(0, 0, 0, 5, 5, 5, 1);
        g.box(1, 1, 1, 4, 4, 4, 0);
        CHECK(quadCount(g) == 12);
    }
    {   // faces are counter-clockwise from outside, their area is the cell faces', and the axis and side are right
        voxel::Grid g(8, 8, 8);
        g.sphere(4, 4, 4, 3.2f, 3);
        g.box(0, 0, 0, 1, 7, 1, 4);
        double area = 0;
        bool wound = true;
        for (const voxel::Quad& q : voxel::quads(g, true)) {
            const Vector3 n = Vector3CrossProduct(Vector3Subtract(q.corner[1], q.corner[0]), Vector3Subtract(q.corner[2], q.corner[0]));
            const float along = n.x * (q.axis == 0) + n.y * (q.axis == 1) + n.z * (q.axis == 2);
            wound = wound && (q.positive ? along > 0 : along < 0);
            area += std::fabs(along);
        }
        CHECK(wound);
        CHECK(std::fabs(area - exposedFaces(g)) < 1e-3);
    }
    {   // ambient occlusion: a pillar on a floor darkens the floor beside it, and splits what would merge
        voxel::Grid g(7, 4, 7);
        g.box(0, 0, 0, 6, 0, 6, 1);
        g.set(3, 1, 3, 1);
        CHECK(quadCount(g, true) > quadCount(g, false));
        bool dark = false, lit = false;
        for (const voxel::Quad& q : voxel::quads(g, true))
            for (uint8_t a : q.ao) { dark = dark || a < 3; lit = lit || a == 3; }
        CHECK(dark && lit);
        // The floor's top face by the pillar: its corner at the pillar's foot is occluded by it.
        bool found = false;
        for (const voxel::Quad& q : voxel::quads(g, true))
            if (q.axis == 1 && q.positive && q.corner[0].y == 1 && q.ao[0] + q.ao[1] + q.ao[2] + q.ao[3] < 12 && q.corner[0].x >= 2 && q.corner[2].x <= 5) found = true;
        CHECK(found);
    }
    {   // helpers
        voxel::Grid g(5, 5, 5);
        g.box(3, 3, 3, 1, 1, 1, 1);
        CHECK(g.count() == 27);
        g.fill(0);
        g.line(0, 0, 0, 4, 0, 0, 1);
        CHECK(g.count() == 5 && g.get(2, 0, 0) == 1);
        g.set(9, 9, 9, 1);                                   // outside: ignored
        CHECK(g.get(9, 9, 9) == 0 && g.count() == 5);
    }
}

// ---- .vox
static void put32(std::vector<unsigned char>& b, int32_t v) { const unsigned char* p = (const unsigned char*)&v; b.insert(b.end(), p, p + 4); }
static void putStr(std::vector<unsigned char>& b, const std::string& s) { put32(b, (int32_t)s.size()); b.insert(b.end(), s.begin(), s.end()); }
static void putDict(std::vector<unsigned char>& b, const std::vector<std::pair<std::string, std::string>>& d) {
    put32(b, (int32_t)d.size());
    for (const auto& [k, v] : d) { putStr(b, k); putStr(b, v); }
}
static void putChunk(std::vector<unsigned char>& out, const char* id, const std::vector<unsigned char>& content) {
    out.insert(out.end(), id, id + 4);
    put32(out, (int32_t)content.size());
    put32(out, 0);
    out.insert(out.end(), content.begin(), content.end());
}
static void writeFile(const fs::path& p, const std::vector<unsigned char>& d) {
    std::ofstream(p, std::ios::binary).write((const char*)d.data(), (std::streamsize)d.size());
}
// MAIN holding `kids`.
static std::vector<unsigned char> voxFile(const std::vector<unsigned char>& kids, int version = 150) {
    std::vector<unsigned char> f = { 'V', 'O', 'X', ' ' };
    put32(f, version);
    f.insert(f.end(), { 'M', 'A', 'I', 'N' });
    put32(f, 0);
    put32(f, (int32_t)kids.size());
    f.insert(f.end(), kids.begin(), kids.end());
    return f;
}
static void modelChunks(std::vector<unsigned char>& kids, int sx, int sy, int sz, const std::vector<std::array<int, 4>>& v) {
    std::vector<unsigned char> size, xyzi;
    put32(size, sx); put32(size, sy); put32(size, sz);
    putChunk(kids, "SIZE", size);
    put32(xyzi, (int32_t)v.size());
    for (const auto& c : v) xyzi.insert(xyzi.end(), { (unsigned char)c[0], (unsigned char)c[1], (unsigned char)c[2], (unsigned char)c[3] });
    putChunk(kids, "XYZI", xyzi);
}
static bool throws(const std::function<void()>& f) {
    try { f(); } catch (const std::exception&) { return true; }
    return false;
}

static void voxFiles(const fs::path& dir) {
    {   // save then load gives the same grid, palette included
        voxel::Grid g(17, 9, 13);
        for (int i = 1; i < 256; i++) g.palette[(size_t)i] = { (unsigned char)i, (unsigned char)(255 - i), (unsigned char)(i * 7), 255 };
        for (int z = 0; z < 13; z++)
            for (int y = 0; y < 9; y++)
                for (int x = 0; x < 17; x++)
                    if (rnd01(7, (unsigned long long)(x + 17 * (y + 9 * z))) < 0.3) g.set(x, y, z, (uint8_t)(1 + (x * 5 + y * 3 + z) % 200));
        // Corners set, so the grid's size is what is read back
        g.set(0, 0, 0, 9); g.set(16, 8, 12, 9);
        const fs::path p = dir / "round.vox";
        voxel::save(p.string(), g);
        const voxel::Grid h = voxel::load(p.string());
        CHECK(h.sx == g.sx && h.sy == g.sy && h.sz == g.sz && h.cells == g.cells);
        bool same = true;
        for (int i = 1; i < 256; i++) same = same && h.palette[(size_t)i].r == g.palette[(size_t)i].r && h.palette[(size_t)i].b == g.palette[(size_t)i].b;
        CHECK(same);
        CHECK(voxel::loadModels(p.string()).size() == 1);
    }
    {   // a scene graph: two models placed by translation, one rotated; animation models are separate
        std::vector<unsigned char> kids;
        modelChunks(kids, 3, 3, 3, { { 1, 1, 1, 5 }, { 2, 1, 1, 6 } });      // model 0: a voxel at the pivot, one beside it
        modelChunks(kids, 2, 2, 2, { { 0, 0, 0, 7 } });                      // model 1
        auto node = [&](const char* id, const std::function<void(std::vector<unsigned char>&)>& f) {
            std::vector<unsigned char> c; f(c); putChunk(kids, id, c);
        };
        node("nTRN", [&](auto& c) { put32(c, 0); putDict(c, {}); put32(c, 1); put32(c, -1); put32(c, -1); put32(c, 1); putDict(c, {}); });
        node("nGRP", [&](auto& c) { put32(c, 1); putDict(c, {}); put32(c, 2); put32(c, 2); put32(c, 4); });
        node("nTRN", [&](auto& c) { put32(c, 2); putDict(c, {}); put32(c, 3); put32(c, -1); put32(c, -1); put32(c, 1); putDict(c, { { "_t", "10 0 0" }, { "_r", "17" } }); });
        node("nSHP", [&](auto& c) { put32(c, 3); putDict(c, {}); put32(c, 1); put32(c, 0); putDict(c, {}); });
        node("nTRN", [&](auto& c) { put32(c, 4); putDict(c, {}); put32(c, 5); put32(c, -1); put32(c, -1); put32(c, 1); putDict(c, { { "_t", "0 6 0" } }); });
        node("nSHP", [&](auto& c) { put32(c, 5); putDict(c, {}); put32(c, 1); put32(c, 1); putDict(c, {}); });
        const fs::path p = dir / "scene.vox";
        writeFile(p, voxFile(kids));
        CHECK(voxel::loadModels(p.string()).size() == 2);
        const std::vector<voxel::Placed> placed = voxel::loadScene(p.string());
        CHECK(placed.size() == 2);
        // A set cell's place in the scene, by its palette index.
        using P3 = std::array<int, 3>;
        std::map<int, std::set<P3>> where;
        for (const voxel::Placed& q : placed)
            for (int z = 0; z < q.grid.sz; z++)
                for (int y = 0; y < q.grid.sy; y++)
                    for (int x = 0; x < q.grid.sx; x++)
                        if (const uint8_t c = q.grid.get(x, y, z)) where[c].insert({ q.ox + x, q.oy + y, q.oz + z });
        // Rotation byte 17 = 1 | 16: the first row takes -y, the second +x, the third +z: (x, y, z) -> (-y, x, z).
        // Model 0's cell (1,1,1) is local (0,0,0) and (2,1,1) is local (1,0,0) -> world (0,1,0); both then move by (10,0,0).
        // The file's (x, y, z) shows here as (x, z, -y).
        CHECK(where[5] == Set3({ { 10, 0, 0 } }));
        CHECK(where[6] == Set3({ { 10, 0, -1 } }));
        // Model 1's cell (0,0,0) is local (-1,-1,-1), then (0,6,0): file (-1,5,-1) -> here (-1,-1,-5).
        CHECK(where[7] == Set3({ { -1, -1, -5 } }));
        const voxel::Grid merged = voxel::load(p.string());
        CHECK(merged.count() == 3);
    }
    {   // bad files say so
        const fs::path junk = dir / "junk.vox", cut = dir / "cut.vox", ver = dir / "ver.vox", empty = dir / "empty.vox", stray = dir / "stray.vox";
        writeFile(junk, { 'n', 'o', 'p', 'e', 1, 2, 3, 4, 5, 6 });
        std::vector<unsigned char> kids;
        modelChunks(kids, 2, 2, 2, { { 0, 0, 0, 1 } });
        std::vector<unsigned char> whole = voxFile(kids);
        writeFile(cut, std::vector<unsigned char>(whole.begin(), whole.end() - 6));
        writeFile(ver, voxFile(kids, 1));
        writeFile(empty, voxFile({}));
        std::vector<unsigned char> outside;
        modelChunks(outside, 2, 2, 2, { { 5, 0, 0, 1 } });
        writeFile(stray, voxFile(outside));
        CHECK(throws([&] { voxel::load(junk.string()); }));
        CHECK(throws([&] { voxel::load(cut.string()); }));
        CHECK(throws([&] { voxel::load(ver.string()); }));
        CHECK(throws([&] { voxel::load(empty.string()); }));
        CHECK(throws([&] { voxel::load(stray.string()); }));
        CHECK(throws([&] { voxel::load((dir / "missing.vox").string()); }));
        CHECK(throws([&] { voxel::save((dir / "x.vox").string(), voxel::Grid(0, 3, 3)); }));
    }
}

// ---- the renderer
constexpr int W = 240, H = 160;
static Image render(Comp& c, int frame) {
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, frame, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}
static Comp sceneComp(g3d::ScenePtr scene) {
    Comp c;
    c.w = W; c.h = H; c.duration = 20; c.background = BLACK;
    addRoot(c.roots, 0, 600)->fn = g3d::draw(scene, {});
    return c;
}
static g3d::ScenePtr baseScene() {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 3.5f, 3.2f, 6 };
    s->camera.target = { 0, 0.6f, 0 };
    s->camera.fov = 45;
    s->lights.push_back(g3d::Light::directional({ 4, 8, 3 }, WHITE, 2.0f, true));
    g3d::Light& sun = s->lights.back();
    sun.shadowExtent = 6;
    s->ambient = 0.3f;
    return s;
}
static void dump(const char* name, const Image& img) {
    if (const char* dir = std::getenv("RMTN_VOXEL_DUMP")) {
        fs::create_directories(dir);
        ExportImage(img, (fs::path(dir) / name).string().c_str());
    }
}
static long diff(const Image& a, const Image& b, int tol = 3) {
    long n = 0;
    for (int y = 0; y < a.height; y++)
        for (int x = 0; x < a.width; x++) {
            const Color p = GetImageColor(a, x, y), q = GetImageColor(b, x, y);
            n += std::abs(p.r - q.r) > tol || std::abs(p.g - q.g) > tol || std::abs(p.b - q.b) > tol;
        }
    return n;
}

static void drawn() {
    g3d::Material mat;
    mat.color = WHITE;
    mat.roughness = 0.8f;
    {   // a voxel block lights, shadows and shades exactly as the box it fills
        voxel::Grid g(8, 6, 4);
        g.palette[1] = { 200, 120, 60, 255 };
        g.fill(1);
        voxel::MeshOptions o;
        o.size = 0.25f; o.centre = true; o.ao = false;
        g3d::Material box = mat;
        box.color = { 200, 120, 60, 255 };
        auto a = baseScene(), b = baseScene();
        g3d::Material floor;
        floor.color = { 150, 150, 150, 255 };
        a->add(g3d::Geometry::plane(14, 14), floor)->position = { 0, -0.001f, 0 };
        b->add(g3d::Geometry::plane(14, 14), floor)->position = { 0, -0.001f, 0 };
        a->add(voxel::mesh(g, o), mat);
        b->add(g3d::Geometry::box(2, 1.5f, 1), box)->position = { 0, 0.75f, 0 };
        Comp ca = sceneComp(a), cb = sceneComp(b);
        Image ia = render(ca, 0), ib = render(cb, 0);
        CHECK(diff(ia, ib) < 40);
        // The block really is there: it is not the floor's colour, and its shadow darkens the floor beside it.
        const Color top = GetImageColor(ia, W / 2, H / 2);
        CHECK(top.r > top.b + 30);
        UnloadImage(ia); UnloadImage(ib);
    }
    {   // ambient occlusion darkens the foot of a pillar; without it the floor is even
        voxel::Grid g(9, 4, 9);
        g.palette[1] = { 220, 220, 220, 255 };
        g.box(0, 0, 0, 8, 0, 8, 1);
        g.box(4, 1, 4, 4, 3, 4, 1);
        voxel::MeshOptions on, off;
        on.size = off.size = 0.3f; on.centre = off.centre = true; off.ao = false;
        auto a = baseScene(), b = baseScene();
        a->lights.clear(); b->lights.clear();
        a->ambient = b->ambient = 0.9f;
        a->add(voxel::mesh(g, on), mat);
        b->add(voxel::mesh(g, off), mat);
        a->camera.position = b->camera.position = { 0, 5, 2.2f };
        a->camera.target = b->camera.target = { 0, 0, 0 };
        Comp ca = sceneComp(a), cb = sceneComp(b);
        Image ia = render(ca, 0), ib = render(cb, 0);
        auto lumaAt = [](const Image& i, int x, int y) { const Color c = GetImageColor(i, x, y); return (int)c.r + c.g + c.b; };
        int minOn = 1 << 30, minOff = 1 << 30;
        for (int y = 50; y < 115; y++)
            for (int x = 90; x < 160; x++) { minOn = std::min(minOn, lumaAt(ia, x, y)); minOff = std::min(minOff, lumaAt(ib, x, y)); }
        dump("ao_on.png", ia); dump("ao_off.png", ib);
        CHECK(minOn < minOff - 25);
        UnloadImage(ia); UnloadImage(ib);
    }
    {   // the frames of an animation show one at a time, each for `hold` frames
        voxel::Grid red(3, 3, 3), blue(3, 3, 3);
        red.palette[1] = { 230, 40, 40, 255 };
        blue.palette[1] = { 40, 40, 230, 255 };
        red.fill(1); blue.fill(1);
        auto s = baseScene();
        s->lights.clear();
        s->ambient = 1.2f;
        voxel::MeshOptions o;
        o.size = 0.5f; o.centre = true; o.ao = false;
        voxel::addFrames(*s, { red, blue }, mat, 10, o);
        s->camera.position = { 0, 1.2f, 4 };
        s->camera.target = { 0, 0.7f, 0 };
        Comp c = sceneComp(s);
        Image a = render(c, 3), b = render(c, 13), a2 = render(c, 23);
        const Color pa = GetImageColor(a, W / 2, H / 2), pb = GetImageColor(b, W / 2, H / 2);
        CHECK(pa.r > pa.b + 40 && pb.b > pb.r + 40);
        CHECK(diff(a, a2) == 0);                             // looping: frame 23 is frame 3 again
        UnloadImage(a); UnloadImage(b); UnloadImage(a2);
    }
    {   // a grid too big for 16-bit indices still draws
        voxel::Grid g(60, 60, 60);
        g.palette[1] = { 90, 200, 90, 255 };
        for (int z = 0; z < 60; z++)
            for (int y = 0; y < 60; y++)
                for (int x = 0; x < 60; x++)
                    if (((x + y + z) & 1) == 0) g.set(x, y, z, 1);        // a checkerboard: every face exposed
        voxel::MeshStats st;
        voxel::MeshOptions o;
        o.size = 0.05f; o.centre = true;
        auto s = baseScene();
        s->add(voxel::mesh(g, o, &st), mat);
        CHECK(st.quads * 4 > 65535);
        s->camera.position = { 0, 2, 5 };
        s->camera.target = { 0, 1.5f, 0 };
        Comp c = sceneComp(s);
        Image img = render(c, 0);
        dump("big.png", img);
        int green = 0;                                       // the checkerboard has gaps: count its green, not one pixel
        for (int y = H / 3; y < H * 2 / 3; y++)
            for (int x = W / 3; x < W * 2 / 3; x++) {
                const Color c = GetImageColor(img, x, y);
                green += c.g > c.r + 20;
            }
        CHECK(green > 300);
        UnloadImage(img);
    }
}

int main() try {
    meshing();
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(W, H, "raymotion voxel");
    if (!IsWindowReady()) return 77;
    const fs::path dir = fs::current_path() / "raymotion-voxel-test";
    fs::create_directories(dir);
    voxFiles(dir);
    drawn();
    shutdownGraphics();
    CloseWindow();
    std::error_code ec;
    fs::remove_all(dir, ec);
    if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    std::puts("voxel tests passed");
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "uncaught: %s\n", e.what());
    return 2;
}
