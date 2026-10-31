// splat_tests.cpp -- Gaussian splat clouds (builders, covariance, .ply in and out) and their rendering:
// a splat's Gaussian, back-to-front blending, hiding behind opaque objects, the sweeping front,
// view-dependent colour.
#include "rmtn/raymotion.hpp"
#include <raymath.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

using namespace rmtn;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
static bool approx(float a, float b, float tol = 1e-4f) { return std::fabs(a - b) <= tol; }
static bool throws(const std::function<void()>& f) {
    try { f(); } catch (const std::exception&) { return true; }
    return false;
}

// ---- the cloud, no GPU
static void clouds() {
    {   // covariance is R S S^T R^T: diagonal for no rotation, x and y swapped by a quarter turn about z
        splat::Cloud c;
        c.add({ 0, 0, 0 }, { 1, 2, 3 }, { 1, 0, 0, 0 }, WHITE);
        const float h = std::sqrt(0.5f);
        c.add({ 0, 0, 0 }, { 1, 2, 3 }, { h, 0, 0, h }, WHITE);
        c.add({ 0, 0, 0 }, { 1, 2, 3 }, { 5, 0, 0, 0 }, WHITE);          // not unit: normalised
        float v[6];
        c.covariance(0, v);
        CHECK(approx(v[0], 1) && approx(v[3], 4) && approx(v[5], 9) && approx(v[1], 0) && approx(v[2], 0) && approx(v[4], 0));
        c.covariance(1, v);
        CHECK(approx(v[0], 4) && approx(v[3], 1) && approx(v[5], 9) && approx(v[1], 0, 1e-5f));
        c.covariance(2, v);
        CHECK(approx(v[0], 1) && approx(v[3], 4));
        CHECK(c.size() == 3 && !c.hasSH());
        // A tilt makes the off-diagonal terms: 45 degrees about z of (4, 1) gives xy = (4 - 1) / 2.
        splat::Cloud d;
        d.add({ 0, 0, 0 }, { 2, 1, 1 }, { std::cos(PI / 8), 0, 0, std::sin(PI / 8) }, WHITE);
        d.covariance(0, v);
        CHECK(approx(v[1], 1.5f, 1e-3f) && approx(v[0], 2.5f, 1e-3f));
    }
    {   // a sphere of splats lies on its sphere; the bound holds them all
        auto c = splat::sphereSurface({ 1, 2, 3 }, 2.f, 500, [](Vector3, int i) { return Color{ (unsigned char)(i % 256), 0, 0, 255 }; }, 4);
        CHECK(c.size() == 500);
        bool onSphere = true, inBound = true;
        for (size_t i = 0; i < c.size(); i++) {
            const Vector3 p{ c.position[i * 3], c.position[i * 3 + 1], c.position[i * 3 + 2] };
            onSphere = onSphere && approx(Vector3Distance(p, { 1, 2, 3 }), 2.f, 1e-3f);
            inBound = inBound && Vector3Distance(p, c.centre) <= c.radius;
        }
        CHECK(onSphere && inBound);
        auto again = splat::sphereSurface({ 1, 2, 3 }, 2.f, 500, [](Vector3, int) { return WHITE; }, 4);
        CHECK(again.position == c.position);                              // the same seed, the same cloud
        auto other = splat::sphereSurface({ 1, 2, 3 }, 2.f, 500, [](Vector3, int) { return WHITE; }, 5);
        CHECK(other.position != c.position);
    }
    {   // splats from voxels: one per face (or `perFace`), on the faces
        voxel::Grid g(3, 3, 3);
        g.set(1, 1, 1, 1);
        auto one = splat::fromVoxels(g, {}, 1, 0.f);
        CHECK(one.size() == 6);
        auto four = splat::fromVoxels(g, {}, 4, 0.f);
        CHECK(four.size() == 24);
        bool onFaces = true;
        for (size_t i = 0; i < one.size(); i++) {
            const Vector3 p{ one.position[i * 3] - 1.5f, one.position[i * 3 + 1] - 1.5f, one.position[i * 3 + 2] - 1.5f };
            onFaces = onFaces && approx(std::max({ std::fabs(p.x), std::fabs(p.y), std::fabs(p.z) }), 0.5f, 1e-4f);
        }
        CHECK(onFaces);
        voxel::MeshOptions o;
        o.size = 2; o.centre = true;
        auto big = splat::fromVoxels(g, o, 1, 0.f);
        CHECK(approx(big.centre.x, 0, 1e-3f) && big.radius > 0);
        auto both = splat::merge(one, four);
        CHECK(both.size() == 30);
    }
    {   // a cloud with no splats
        splat::Cloud e;
        e.bound();
        CHECK(e.size() == 0 && e.radius == 0);
        CHECK(splat::sphereSurface({ 0, 0, 0 }, 1, 0, [](Vector3, int) { return WHITE; }).size() == 0);
    }
}

// ---- .ply
static void put(std::vector<unsigned char>& b, const void* p, size_t n) { b.insert(b.end(), (const unsigned char*)p, (const unsigned char*)p + n); }
static void writeBytes(const fs::path& p, const std::string& header, const std::vector<unsigned char>& body) {
    std::ofstream out(p, std::ios::binary);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)body.data(), (std::streamsize)body.size());
}

static void plyFiles(const fs::path& dir) {
    {   // save then load: the same cloud, spherical harmonics included
        splat::Cloud c;
        for (int i = 0; i < 40; i++)
            c.add({ (float)i * 0.1f, std::sin((float)i), -(float)i }, { 0.05f + i * 0.001f, 0.1f, 0.02f }, { 0.9f, 0.1f * (i % 3), 0.2f, 0.3f }, Color{ (unsigned char)(i * 6), 100, (unsigned char)(255 - i * 6), 255 }, 0.2f + 0.02f * i);
        c.sh.resize(c.size() * 9);
        for (size_t i = 0; i < c.sh.size(); i++) c.sh[i] = std::sin((float)i) * 0.3f;
        c.bound();
        const fs::path p = dir / "round.ply";
        splat::savePly(p.string(), c);
        const splat::Cloud d = splat::loadPly(p.string());
        auto same = [](const std::vector<float>& a, const std::vector<float>& b, float tol) {
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); i++) if (std::fabs(a[i] - b[i]) > tol) return false;
            return true;
        };
        CHECK(d.size() == c.size());
        CHECK(same(c.position, d.position, 1e-6f) && same(c.scale, d.scale, 1e-5f) && same(c.opacity, d.opacity, 1e-4f));
        CHECK(same(c.colour, d.colour, 1e-5f) && same(c.sh, d.sh, 1e-6f) && d.hasSH());
        CHECK(same(c.rotation, d.rotation, 1e-5f));
    }
    {   // a file in another order, with extra properties and other types: read by name
        std::string header = "ply\nformat binary_little_endian 1.0\ncomment made by hand\nelement vertex 2\n"
                             "property uchar flag\nproperty float rot_0\nproperty double z\nproperty float rot_1\nproperty float rot_2\nproperty float rot_3\n"
                             "property float opacity\nproperty float y\nproperty float scale_2\nproperty float x\nproperty float scale_0\nproperty float scale_1\n"
                             "property float f_dc_0\nproperty float f_dc_1\nproperty float f_dc_2\nproperty float nx\nend_header\n";
        std::vector<unsigned char> b;
        for (int i = 0; i < 2; i++) {
            const unsigned char flag = 7;
            const float rot[4] = { 2, 0, 0, 0 };                     // not unit
            const double z = 3.5 + i;
            const float op = i == 0 ? 0.f : 2.f, y = 2, s2 = std::log(0.3f), x = 1, s0 = std::log(0.1f), s1 = std::log(0.2f);
            const float dc[3] = { 1.f, 0.f, -1.f }, nx = 9;
            put(b, &flag, 1); put(b, &rot[0], 4); put(b, &z, 8); put(b, &rot[1], 4); put(b, &rot[2], 4); put(b, &rot[3], 4);
            put(b, &op, 4); put(b, &y, 4); put(b, &s2, 4); put(b, &x, 4); put(b, &s0, 4); put(b, &s1, 4);
            put(b, dc, 12); put(b, &nx, 4);
        }
        const fs::path p = dir / "odd.ply";
        writeBytes(p, header, b);
        const splat::Cloud c = splat::loadPly(p.string());
        CHECK(c.size() == 2 && !c.hasSH());
        CHECK(approx(c.position[0], 1) && approx(c.position[1], 2) && approx(c.position[2], 3.5f) && approx(c.position[5], 4.5f));
        CHECK(approx(c.scale[0], 0.1f, 1e-5f) && approx(c.scale[1], 0.2f, 1e-5f) && approx(c.scale[2], 0.3f, 1e-5f));
        CHECK(approx(c.rotation[0], 1) && approx(c.rotation[1], 0));                       // normalised
        CHECK(approx(c.opacity[0], 0.5f) && approx(c.opacity[1], 1.f / (1.f + std::exp(-2.f)), 1e-5f));   // sigmoid
        CHECK(approx(c.colour[0], 0.5f + 0.2820948f, 1e-5f) && approx(c.colour[1], 0.5f) && approx(c.colour[2], 0.5f - 0.2820948f, 1e-5f));
    }
    {   // plain points with 8-bit colour become small round splats
        std::string header = "ply\nformat binary_little_endian 1.0\nelement vertex 1\nproperty float x\nproperty float y\nproperty float z\n"
                             "property uchar red\nproperty uchar green\nproperty uchar blue\nend_header\n";
        std::vector<unsigned char> b;
        const float xyz[3] = { 1, 2, 3 };
        put(b, xyz, 12);
        b.insert(b.end(), { 255, 128, 0 });
        const fs::path p = dir / "points.ply";
        writeBytes(p, header, b);
        const splat::Cloud c = splat::loadPly(p.string());
        CHECK(c.size() == 1 && approx(c.colour[0], 1) && approx(c.colour[1], 128 / 255.f, 1e-4f) && approx(c.colour[2], 0) && approx(c.opacity[0], 1));
    }
    {   // what cannot be read says so
        const std::string good = "ply\nformat binary_little_endian 1.0\nelement vertex 2\nproperty float x\nproperty float y\nproperty float z\nend_header\n";
        std::vector<unsigned char> body(12 * 2, 0), shortBody(12 + 5, 0);
        writeBytes(dir / "ok.ply", good, body);
        writeBytes(dir / "short.ply", good, shortBody);
        writeBytes(dir / "ascii.ply", "ply\nformat ascii 1.0\nelement vertex 1\nproperty float x\nproperty float y\nproperty float z\nend_header\n", {});
        writeBytes(dir / "big.ply", "ply\nformat binary_big_endian 1.0\nelement vertex 1\nproperty float x\nproperty float y\nproperty float z\nend_header\n", std::vector<unsigned char>(12, 0));
        writeBytes(dir / "nox.ply", "ply\nformat binary_little_endian 1.0\nelement vertex 1\nproperty float y\nproperty float z\nend_header\n", std::vector<unsigned char>(8, 0));
        writeBytes(dir / "notply.ply", "hello\n", {});
        CHECK(splat::loadPly((dir / "ok.ply").string()).size() == 2);
        CHECK(throws([&] { splat::loadPly((dir / "short.ply").string()); }));
        CHECK(throws([&] { splat::loadPly((dir / "ascii.ply").string()); }));
        CHECK(throws([&] { splat::loadPly((dir / "big.ply").string()); }));
        CHECK(throws([&] { splat::loadPly((dir / "nox.ply").string()); }));
        CHECK(throws([&] { splat::loadPly((dir / "notply.ply").string()); }));
        CHECK(throws([&] { splat::loadPly((dir / "missing.ply").string()); }));
    }
}

// ---- drawn
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
static Comp comp(g3d::ScenePtr scene, Color bg = BLACK) {
    Comp c;
    c.w = W; c.h = H; c.duration = 20; c.background = bg;
    addRoot(c.roots, 0, 600)->fn = g3d::draw(scene, {});
    return c;
}
static g3d::ScenePtr scene(Vector3 eye = { 0, 0, 5 }) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = eye;
    s->camera.target = { 0, 0, 0 };
    s->camera.fov = 40;
    return s;
}
static g3d::Splats* addCloud(g3d::Scene& s, splat::Cloud c) {
    auto sp = std::make_shared<g3d::Splats>();
    sp->cloud = std::make_shared<const splat::Cloud>(std::move(c));
    s.add(sp);
    return sp.get();
}
static Color px(const Image& i, int x, int y) { return GetImageColor(i, x, y); }
static void dump(const char* name, const Image& img) {
    if (const char* dir = std::getenv("RMTN_SPLAT_DUMP")) {
        fs::create_directories(dir);
        ExportImage(img, (fs::path(dir) / name).string().c_str());
    }
}

static void drawn() {
    {   // one splat is a Gaussian: full at its centre, 0.61 at one sigma, nothing at three
        auto s = scene();
        splat::Cloud c;
        c.add({ 0, 0, 0 }, { 0.3f, 0.3f, 0.3f }, { 1, 0, 0, 0 }, WHITE, 1);
        c.bound();
        addCloud(*s, std::move(c));
        Comp cm = comp(s);
        Image img = render(cm, 0);
        dump("one.png", img);
        const float focalPx = (H / 2.f) / std::tan(20 * DEG2RAD);              // pixels per metre at 1 m
        const float sigmaPx = focalPx * 0.3f / 5.f;                             // one sigma at 5 m
        const int cx = W / 2, cy = H / 2;
        CHECK(px(img, cx, cy).r > 240);
        const int at1 = px(img, cx + (int)std::lround(sigmaPx), cy).r;
        CHECK(at1 > 130 && at1 < 180);                                         // 255 * exp(-0.5) = 155
        const int at2 = px(img, cx + (int)std::lround(2 * sigmaPx), cy).r;
        CHECK(at2 > 15 && at2 < 60);                                           // 255 * exp(-2) = 35
        CHECK(px(img, cx + (int)std::lround(3.6f * sigmaPx), cy).r < 8);
        CHECK(px(img, cx, cy - (int)std::lround(sigmaPx)).r > 130);            // round, not a streak
        UnloadImage(img);
    }
    {   // an elongated splat, turned: long along the screen's y when rolled a quarter turn
        auto s = scene();
        splat::Cloud c;
        const float h = std::sqrt(0.5f);
        c.add({ 0, 0, 0 }, { 0.8f, 0.1f, 0.1f }, { h, 0, 0, h }, WHITE, 1);     // long axis x turned to y
        c.bound();
        addCloud(*s, std::move(c));
        Comp cm = comp(s);
        Image img = render(cm, 0);
        int up = 0, side = 0;
        for (int d = 1; d < 70; d++) { up += px(img, W / 2, H / 2 - d).r > 100; side += px(img, W / 2 + d, H / 2).r > 100; }
        CHECK(up > side * 3);
        UnloadImage(img);
    }
    {   // blending is back to front: the nearer splat covers the farther, and which is nearer follows the camera
        for (int from = 0; from < 2; from++) {
            auto s = scene({ 0, 0, from ? -5.f : 5.f });
            splat::Cloud c;
            c.add({ 0, 0, 1 }, { 0.6f, 0.6f, 0.6f }, { 1, 0, 0, 0 }, RED, 0.8f);       // z = +1
            c.add({ 0, 0, -1 }, { 0.6f, 0.6f, 0.6f }, { 1, 0, 0, 0 }, BLUE, 0.8f);     // z = -1
            c.bound();
            addCloud(*s, std::move(c));
            Comp cm = comp(s);
            Image img = render(cm, 0);
            const Color p = px(img, W / 2, H / 2);
            if (from == 0) CHECK(p.r > p.b + 60); else CHECK(p.b > p.r + 60);
            UnloadImage(img);
        }
    }
    {   // opaque things hide splats behind them and not in front
        for (int boxFront = 0; boxFront < 2; boxFront++) {
            auto s = scene();
            splat::Cloud c;
            c.add({ 0, 0, 0 }, { 0.6f, 0.6f, 0.6f }, { 1, 0, 0, 0 }, GREEN, 1);
            c.bound();
            addCloud(*s, std::move(c));
            g3d::Material m;
            m.unlit = true;
            m.color = { 0, 0, 255, 255 };
            s->add(g3d::Geometry::box(3, 3, 0.2f), m)->position = { 0, 0, boxFront ? 2.f : -2.f };
            Comp cm = comp(s);
            Image img = render(cm, 0);
            const Color p = px(img, W / 2, H / 2);
            if (boxFront) CHECK(p.b > 200 && p.g < 40); else CHECK(p.g > 200 && p.b < 60);
            UnloadImage(img);
        }
    }
    {   // the same frame twice is the same picture
        auto s = scene();
        addCloud(*s, splat::sphereSurface({ 0, 0, 0 }, 1.2f, 3000, [](Vector3 p, int) { return Color{ (unsigned char)(128 + 100 * p.x), (unsigned char)(128 + 100 * p.y), 180, 255 }; }, 3));
        Comp cm = comp(s);
        Image a = render(cm, 4), b = render(cm, 4);
        CHECK(std::memcmp(a.data, b.data, (size_t)W * H * 4) == 0);
        dump("sphere.png", a);
        long lit = 0;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) lit += px(a, x, y).b > 60;
        CHECK(lit > 1500);
        UnloadImage(a); UnloadImage(b);
    }
    {   // a sweeping front: splats past it are gone, those behind it stay
        auto s = scene();
        splat::Cloud c;
        c.add({ -1, 0, 0 }, { 0.3f, 0.3f, 0.3f }, { 1, 0, 0, 0 }, WHITE, 1);
        c.add({ 1, 0, 0 }, { 0.3f, 0.3f, 0.3f }, { 1, 0, 0, 0 }, WHITE, 1);
        c.bound();
        g3d::Splats* sp = addCloud(*s, std::move(c));
        sp->look.clipNormal = { 1, 0, 0 };
        sp->look.clipOffset = 0;
        sp->look.clipBand = 0.2f;
        Comp cm = comp(s);
        Image img = render(cm, 0);
        const float focalPx = (H / 2.f) / std::tan(20 * DEG2RAD) / 5.f;
        CHECK(px(img, W / 2 - (int)std::lround(focalPx), H / 2).r > 240);
        CHECK(px(img, W / 2 + (int)std::lround(focalPx), H / 2).r < 8);
        UnloadImage(img);
        sp->look.clipOffset = 1.1f;                          // the right one is inside the band now: fading
        Image mid = render(cm, 0);
        const int r = px(mid, W / 2 + (int)std::lround(focalPx), H / 2).r;
        CHECK(r > 8 && r < 240);
        UnloadImage(mid);
        sp->look.opacity = 0.5f;
        sp->look.clipOffset = 1e9f;
        Image half = render(cm, 0);
        const int h2 = px(half, W / 2 - (int)std::lround(focalPx), H / 2).r;
        CHECK(h2 > 100 && h2 < 150);
        UnloadImage(half);
    }
    {   // colour that depends on where you look from
        splat::Cloud c;
        c.add({ 0, 0, 0 }, { 0.4f, 0.4f, 0.4f }, { 1, 0, 0, 0 }, Color{ 128, 128, 128, 255 }, 1);
        c.sh.assign(9, 0.f);
        c.sh[1] = 1.f;                                       // red, the term along z
        c.bound();
        for (int from = 0; from < 3; from++) {
            auto s = scene({ 0, 0, from == 1 ? -5.f : 5.f });
            auto sp = std::make_shared<g3d::Splats>();
            sp->cloud = std::make_shared<const splat::Cloud>(c);
            sp->look.shDegree = from == 2 ? 0 : 1;
            s->add(sp);
            Comp cm = comp(s);
            Image img = render(cm, 0);
            const Color p = px(img, W / 2, H / 2);
            if (from == 0) CHECK(p.r < 40);                  // looking along -z: 0.5 - 0.49
            if (from == 1) CHECK(p.r > 200);                 // from the other side: 0.5 + 0.49
            if (from == 2) CHECK(p.r > 110 && p.r < 150);    // degree 0: flat
            UnloadImage(img);
        }
    }
    {   // a mesh cut by a plane: the part beyond it is gone and its edge glows
        auto s = scene();
        s->ambient = 1.0f;
        g3d::Material m;
        m.color = { 200, 200, 200, 255 };
        m.clip.normal = { 1, 0, 0 };
        m.clip.offset = 0;
        m.clip.edgeWidth = 0.5f;
        m.clip.edgeColor = { 255, 120, 0, 255 };
        s->add(g3d::Geometry::box(3, 2, 1), m);
        s->camera.position = { 0, 0, 6 };
        Comp cm = comp(s);
        Image img = render(cm, 0);
        const Color left = px(img, W / 2 - 25, H / 2), right = px(img, W / 2 + 25, H / 2), edge = px(img, W / 2 - 4, H / 2);
        CHECK(left.r > 60);
        CHECK(right.r < 10 && right.g < 10 && right.b < 10);
        CHECK(edge.r > left.r && edge.r > edge.b + 40);                      // the glow is orange
        dump("clip.png", img);
        UnloadImage(img);
    }
    {   // a million splats draw, and the time they take is recorded
        auto s = scene({ 0, 0.5f, 6 });
        auto c = splat::fromFunction(1000000, [](int i) {
            const double u = rnd01(11, (unsigned long long)i * 3), v = rnd01(11, (unsigned long long)i * 3 + 1), w = rnd01(11, (unsigned long long)i * 3 + 2);
            splat::Sample sm;
            const double a = u * 2 * PI, y = v * 2 - 1, r = std::sqrt(1 - y * y);
            const double rad = 1.5 + 0.2 * w;
            sm.position = { (float)(rad * r * std::cos(a)), (float)(rad * y), (float)(rad * r * std::sin(a)) };
            sm.sigma = { 0.012f, 0.012f, 0.012f };
            sm.colour = { (unsigned char)(120 + 100 * y), (unsigned char)(150 + 80 * r), 220, 255 };
            sm.opacity = 0.6f;
            return sm;
        });
        addCloud(*s, std::move(c));
        Comp cm = comp(s);
        render(cm, 0);                                       // builds the texture and buffers
        const auto t0 = std::chrono::steady_clock::now();
        const int frames = 6;
        for (int f = 1; f <= frames; f++) {
            s->camera.position = { 6 * std::sin(f * 0.3f), 0.5f, 6 * std::cos(f * 0.3f) };   // a new view each time: a new sort
            Image img = render(cm, f);
            UnloadImage(img);
        }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / frames;
        std::printf("[splat] 1,000,000 splats, %dx%d: %.1f ms a frame including readback (sort every frame)\n", W, H, ms);
        s->camera.position = { 0, 0.5f, 6 };
        Image img = render(cm, 99);
        dump("million.png", img);
        long lit = 0;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) lit += px(img, x, y).b > 40;
        CHECK(lit > 2000);
        UnloadImage(img);
    }
}

int main() try {
    clouds();
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(W, H, "raymotion splat");
    if (!IsWindowReady()) return 77;
    const fs::path dir = fs::current_path() / "raymotion-splat-test";
    fs::create_directories(dir);
    plyFiles(dir);
    drawn();
    shutdownGraphics();
    CloseWindow();
    std::error_code ec;
    fs::remove_all(dir, ec);
    if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    std::puts("splat tests passed");
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "uncaught: %s\n", e.what());
    return 2;
}
