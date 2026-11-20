// scene3d_tests.cpp -- GPU 3D scenes: placement, depth, lighting, shadows, viewports,
// transparency, hierarchy, models (OBJ and a generated skinned glTF), determinism.
#include "rmtn/raymotion.hpp"
#include <raymath.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace rmtn;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

constexpr int W = 240, H = 160;

static Image render(Comp& c, int frame, int ss = 1) {
    Canvas canvas;
    canvas.load(c.w, c.h, ss);
    presentFrame(c, frame, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}
// A composition whose only node renders `scene` over a 2D background.
static Comp sceneComp(g3d::ScenePtr scene, Color bg = BLACK, Rectangle viewport = {}) {
    Comp c;
    c.w = W; c.h = H; c.duration = 10; c.background = bg;
    addRoot(c.roots, 0, 10)->fn = g3d::draw(scene, viewport);
    return c;
}
// Screen pixel of a world point for the scene's camera over the whole composition.
static Vector2 project(const g3d::Camera& cam, Vector3 p, int w = W, int h = H) {
    Camera3D rc{ cam.position, cam.target, cam.upVector, cam.fov, CAMERA_PERSPECTIVE };
    return GetWorldToScreenEx(p, rc, w, h);
}
static Color px(const Image& img, Vector2 p) { return GetImageColor(img, (int)p.x, (int)p.y); }
static int luma(Color c) { return (c.r * 2126 + c.g * 7152 + c.b * 722) / 10000; }
static bool close(Color a, Color b, int tol = 2) {
    return std::abs(a.r - b.r) <= tol && std::abs(a.g - b.g) <= tol && std::abs(a.b - b.b) <= tol && std::abs(a.a - b.a) <= tol;
}
static bool allOpaque(const Image& img) {
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++)
            if (GetImageColor(img, x, y).a != 255) return false;
    return true;
}
static g3d::Material unlit(Color c) { g3d::Material m; m.color = c; m.unlit = true; return m; }

// Shapes follow Three.js conventions: centred, torus/knot sized by radius and tube.
static void geometryExtents() {
    auto near = [](float a, float b) { return std::fabs(a - b) < 0.02f; };
    BoundingBox t = GetMeshBoundingBox(g3d::Geometry::torus(1, 0.25f)->mesh());
    CHECK(near(t.max.x, 1.25f) && near(t.min.y, -1.25f) && near(t.max.z, 0.25f));
    // a thin ring keeps its tube (raylib's GenMeshTorus clamped tube / radius to >= 0.1)
    BoundingBox thin = GetMeshBoundingBox(g3d::Geometry::torus(80, 0.5f, 64, 12)->mesh());
    CHECK(near(thin.max.z, 0.5f) && std::fabs(thin.max.x - 80.5f) < 0.05f);
    BoundingBox cy = GetMeshBoundingBox(g3d::Geometry::cylinder(0.5f, 2)->mesh());
    CHECK(near(cy.min.y, -1) && near(cy.max.y, 1) && near(cy.max.x, 0.5f));
    BoundingBox co = GetMeshBoundingBox(g3d::Geometry::cone(0.5f, 2)->mesh());
    CHECK(near(co.min.y, -1) && near(co.max.y, 1));
    auto knot = g3d::Geometry::knot(1, 0.3f);
    BoundingBox k = GetMeshBoundingBox(knot->mesh());
    // The (2,3) curve spans radius * 1.5 in x/y and radius * 0.5 in z, plus the tube.
    CHECK(k.max.x > 1.5f && k.max.x < 1.85f && near(k.max.z, 0.8f));
    CHECK(&knot->mesh() == &knot->mesh());                                     // generated once
}

static void placementAndDepth() {
    for (bool nearFirst : { true, false }) {
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 0, 5 };
        auto box = g3d::Geometry::box(1, 1, 1);
        auto addNear = [&] { s->add(box, unlit(GREEN))->position = { 0, 0, 1 }; };
        auto addFar = [&] { s->add(g3d::Geometry::box(3, 3, 1), unlit(RED))->position = { 0, 0, -1 }; };
        if (nearFirst) { addNear(); addFar(); } else { addFar(); addNear(); }
        Comp c = sceneComp(s, BLUE);
        Image img = render(c, 0);
        CHECK(close(px(img, { W / 2.f, H / 2.f }), GREEN));                       // nearer wins either way
        CHECK(close(px(img, project(s->camera, { 1.2f, 0, -1 })), RED));
        CHECK(close(GetImageColor(img, 2, 2), BLUE));                              // 2D background shows through
        CHECK(allOpaque(img));
        UnloadImage(img);
    }
}

static void supersampledPlacement() {
    // The box covers the same pixels at ss 1 and 2 (within edge antialiasing).
    int widths[2];
    for (int k = 0; k < 2; k++) {
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 0, 5 };
        s->add(g3d::Geometry::box(1, 1, 1), unlit(WHITE));
        Comp c = sceneComp(s);
        Image img = render(c, 0, k ? 2 : 1);
        int n = 0;
        for (int x = 0; x < W; x++) n += GetImageColor(img, x, H / 2).r > 127;
        widths[k] = n;
        CHECK(GetImageColor(img, W / 2, H / 2).r > 250);
        UnloadImage(img);
    }
    CHECK(widths[0] > 20 && std::abs(widths[0] - widths[1]) <= 2);
}

static void lighting() {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 0, 5 };
    g3d::Material m;
    m.color = Color{ 180, 180, 180, 255 };
    m.roughness = 1;
    s->add(g3d::Geometry::sphere(1), m);
    s->lights.push_back(g3d::Light::directional({ 10, 0, 0 }));                 // from +x
    Comp c = sceneComp(s);
    Image img = render(c, 0);
    int right = luma(px(img, project(s->camera, { 0.7f, 0, 0.7f })));
    int left = luma(px(img, project(s->camera, { -0.7f, 0, 0.7f })));
    CHECK(right > left + 60);
    CHECK(left > 10);                                                              // hemisphere ambient
    UnloadImage(img);

    // Point light with a range: bright under it, ambient only beyond the range.
    auto p = std::make_shared<g3d::Scene>();
    p->camera.position = { 0, 6, 6 };
    p->add(g3d::Geometry::plane(20, 20), m);
    p->lights.push_back(g3d::Light::point({ 0, 1, 0 }, WHITE, 2, 3));
    Comp pc = sceneComp(p);
    img = render(pc, 0);
    int under = luma(px(img, project(p->camera, { 0, 0, 0 })));
    int beyond = luma(px(img, project(p->camera, { 3.5f, 0, 0 })));
    int farther = luma(px(img, project(p->camera, { -4.5f, 0, 1 })));
    CHECK(under > beyond + 60);
    CHECK(std::abs(beyond - farther) <= 3);
    UnloadImage(img);

    // Spot light: lit inside the cone, ambient outside.
    auto sp = std::make_shared<g3d::Scene>();
    sp->camera.position = { 0, 6, 6 };
    sp->add(g3d::Geometry::plane(20, 20), m);
    sp->lights.push_back(g3d::Light::spot({ 0, 4, 0 }, { 0, 0, 0 }, WHITE, 2, 20));
    Comp sc = sceneComp(sp);
    img = render(sc, 0);
    int inside = luma(px(img, project(sp->camera, { 0.3f, 0, 0 })));
    int outside = luma(px(img, project(sp->camera, { 2.5f, 0, 0 })));            // 32 degrees off-axis
    CHECK(inside > outside + 60);
    UnloadImage(img);
}

static void shadows() {
    int lit[2], open[2];
    for (int k = 0; k < 2; k++) {
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 6, 6 };
        g3d::Material m;
        m.roughness = 1;
        s->add(g3d::Geometry::plane(12, 12), m);
        s->add(g3d::Geometry::box(1, 1, 1), m)->position = { 0, 1.5f, 0 };
        s->lights.push_back(g3d::Light::directional({ 4, 8, 0 }, WHITE, 1.5f, k == 1));
        Comp c = sceneComp(s);
        Image img = render(c, 0);
        lit[k] = luma(px(img, project(s->camera, { -0.75f, 0, 0 })));            // where the box's shadow falls
        open[k] = luma(px(img, project(s->camera, { 2, 0, 2 })));
        UnloadImage(img);
    }
    CHECK(lit[1] < lit[0] - 60);                                                   // shadowed
    CHECK(std::abs(open[1] - open[0]) <= 2);                                       // no acne in open ground
}

// A sun 5 and 10 degrees above the horizon over flat ground (backlit grass at dusk): open
// ground must match the unshadowed render (acne used to draw horizontal bands here), and a
// box's long shadow must still start at its foot.
static void lowSunShadows() {
    for (float elevation : { 5.f, 10.f }) {
        Image img[2];
        g3d::ScenePtr s;
        for (int k = 0; k < 2; k++) {
            s = std::make_shared<g3d::Scene>();
            s->camera.position = { 0, 1.6f, 5 };
            s->camera.target = { 0, 0.6f, 0 };
            g3d::Material m;
            m.roughness = 1;
            m.color = { 120, 150, 90, 255 };
            s->add(g3d::Geometry::plane(40, 40), m);
            s->add(g3d::Geometry::box(1, 1, 1), m)->position = { -1.5f, 0.5f, -1 };
            const float e = elevation * DEG2RAD;
            g3d::Light l = g3d::Light::directional({ 0, 10 * std::sin(e), -10 * std::cos(e) }, WHITE, 2, k == 1);
            l.shadowExtent = 12;
            s->lights.push_back(l);
            Comp c = sceneComp(s);
            img[k] = render(c, 0);
        }
        int banded = 0, ground = 0;
        for (int y = H / 2 + 8; y < H; y++)
            for (int x = W / 2; x < W; x++) {                                       // right half: open ground
                ground++;
                banded += luma(GetImageColor(img[1], x, y)) < luma(GetImageColor(img[0], x, y)) - 6;
            }
        const int foot[2] = { luma(px(img[0], project(s->camera, { -1.5f, 0, -0.35f }))),
                              luma(px(img[1], project(s->camera, { -1.5f, 0, -0.35f }))) };
        const int tail[2] = { luma(px(img[0], project(s->camera, { -1.5f, 0, 2 }))),
                              luma(px(img[1], project(s->camera, { -1.5f, 0, 2 }))) };
        std::printf("[scene3d] low sun %.0f deg: %d of %d open-ground pixels darkened, foot %d -> %d, tail %d -> %d\n",
                    elevation, banded, ground, foot[0], foot[1], tail[0], tail[1]);
        CHECK(banded * 100 < ground);                                               // under 1 %
        CHECK(foot[1] < foot[0] - 15 && tail[1] < tail[0] - 15);
        UnloadImage(img[0]);
        UnloadImage(img[1]);
    }
}

static void viewportsAndOpacity() {
    auto s = std::make_shared<g3d::Scene>();
    s->background = BLUE;
    s->camera.position = { 0, 0, 5 };
    s->add(g3d::Geometry::box(1, 1, 1), unlit(RED));
    Comp c = sceneComp(s, BLACK, { W / 2.f, 0, W / 2.f, (float)H });
    Image img = render(c, 0);
    CHECK(close(GetImageColor(img, 20, 20), BLACK));
    CHECK(close(GetImageColor(img, W / 2 + 10, 10), BLUE));
    CHECK(close(GetImageColor(img, W * 3 / 4, H / 2), RED));                       // centred in its viewport
    CHECK(close(GetImageColor(img, W / 4, H / 2), BLACK));
    UnloadImage(img);

    // Node opacity fades the whole rendered scene.
    auto w = std::make_shared<g3d::Scene>();
    w->camera.position = { 0, 0, 5 };
    w->add(g3d::Geometry::box(1, 1, 1), unlit(WHITE));
    Comp f = sceneComp(w);
    f.roots[0]->props.opacity = 0.5f;
    img = render(f, 0);
    CHECK(std::abs(GetImageColor(img, W / 2, H / 2).r - 128) <= 2);
    CHECK(allOpaque(img));
    UnloadImage(img);
}

static void transparencyAndHierarchy() {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 0, 5 };
    g3d::Material glass = unlit(Color{ 255, 0, 0, 255 });
    glass.opacity = 0.5f;
    s->add(g3d::Geometry::box(1, 1, 0.2f), glass)->position = { 0, 0, 1 };      // added first, drawn last
    s->add(g3d::Geometry::box(2, 2, 0.2f), unlit(Color{ 0, 0, 255, 255 }));
    // A child 2 units along the parent's x, parent turned 90 degrees about y: world (0, 0, -2).
    g3d::Object3D* parent = s->add();
    parent->rotation = { 0, 90, 0 };
    parent->add(g3d::Geometry::sphere(0.3f), unlit(GREEN))->position = { 2, 1.5f, 0 };
    Comp c = sceneComp(s);
    Image img = render(c, 0);
    Color mid = GetImageColor(img, W / 2, H / 2);
    CHECK(std::abs(mid.r - 128) <= 3 && mid.g <= 2 && std::abs(mid.b - 127) <= 3);
    CHECK(close(px(img, project(s->camera, { 0, 1.5f, -2 })), GREEN));
    UnloadImage(img);
}

// Shadowed vs unshadowed luma at each probe point, for a scene built with and without shadows.
static std::vector<std::pair<int, int>> shadowProbe(const std::function<void(g3d::Scene&, bool)>& build,
                                                    const std::vector<Vector3>& points) {
    std::vector<std::pair<int, int>> out(points.size());
    for (int k = 0; k < 2; k++) {
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 9, 9 };
        g3d::Material m;
        m.roughness = 1;
        s->add(g3d::Geometry::plane(30, 30), m);
        build(*s, k == 1);
        Comp c = sceneComp(s);
        Image img = render(c, 0);
        for (size_t i = 0; i < points.size(); i++) {
            int l = luma(px(img, project(s->camera, points[i])));
            (k ? out[i].first : out[i].second) = l;
        }
        UnloadImage(img);
    }
    return out;
}

static void moreShadows() {
    g3d::Material m;
    m.roughness = 1;
    // Two directional lights from opposite sides: both shadows at once.
    auto two = shadowProbe([&](g3d::Scene& s, bool on) {
        s.add(g3d::Geometry::box(1, 1, 1), m)->position = { 0, 1.5f, 0 };
        s.lights.push_back(g3d::Light::directional({ 4, 8, 0 }, WHITE, 0.4f, on));
        s.lights.push_back(g3d::Light::directional({ -4, 8, 0 }, WHITE, 0.4f, on));
    }, { { -0.75f, 0, 0 }, { 0.75f, 0, 0 }, { 3, 0, 3 } });
    CHECK(two[0].first < two[0].second - 30 && two[1].first < two[1].second - 30);
    CHECK(std::abs(two[2].first - two[2].second) <= 2);

    // A point light above four boxes: shadows on four different cube faces.
    auto point = shadowProbe([&](g3d::Scene& s, bool on) {
        for (Vector3 p : { Vector3{ 2, 2, 0 }, Vector3{ -2, 2, 0 }, Vector3{ 0, 2, -2 }, Vector3{ 0, 2, 2 } })
            s.add(g3d::Geometry::box(0.8f, 0.8f, 0.8f), m)->position = p;
        g3d::Light l = g3d::Light::point({ 0, 4, 0 }, WHITE, 1.5f, 20);
        l.castShadow = on;
        s.lights.push_back(l);
    }, { { 4, 0, 0 }, { -4, 0, 0 }, { 0, 0, 4 }, { 2.8f, 0, 2.8f } });          // +x, -x and +z faces
    for (int i = 0; i < 3; i++) CHECK(point[(size_t)i].first < point[(size_t)i].second - 30);
    CHECK(std::abs(point[3].first - point[3].second) <= 2);

    // A spot light's shadow.
    auto spot = shadowProbe([&](g3d::Scene& s, bool on) {
        s.add(g3d::Geometry::box(1, 1, 1), m)->position = { 0, 2.5f, 0 };
        s.lights.push_back(g3d::Light::spot({ 0, 5, 0.01f }, { 0, 0, 0 }, WHITE, 2, 40, on));
    }, { { 0.3f, 0, 0.3f }, { 2.5f, 0, 0 } });
    CHECK(spot[0].first < spot[0].second - 30);
    CHECK(std::abs(spot[1].first - spot[1].second) <= 2);
}

static void environment(const fs::path& dir) {
    // Equirectangular: red sky above the horizon, blue ground below.
    Image eq = GenImageColor(256, 128, RED);
    ImageDrawRectangle(&eq, 0, 64, 256, 64, BLUE);
    const fs::path path = dir / "env.png";
    ExportImage(eq, path.string().c_str());
    UnloadImage(eq);

    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 0, 5 };
    s->environment = path.string();
    g3d::Material chrome;
    chrome.metalness = 1;
    chrome.roughness = 0;
    s->add(g3d::Geometry::sphere(1, 48, 64), chrome);
    Comp c = sceneComp(s);
    Image img = render(c, 0);
    Color up = px(img, project(s->camera, { 0, 0.8f, 0.6f })), down = px(img, project(s->camera, { 0, -0.8f, 0.6f }));
    CHECK(close(up, RED, 3) && close(down, BLUE, 3));             // the mirror reflects sky and ground
    CHECK(close(GetImageColor(img, 3, 3), BLACK));                // no backdrop unless asked
    UnloadImage(img);

    // The environment as backdrop, turned 180 degrees about y: unchanged, as it is symmetric.
    auto sky = std::make_shared<g3d::Scene>();
    sky->camera.position = { 0, 0, 5 };
    sky->environment = path.string();
    sky->showEnvironment = true;
    sky->environmentRotation = 180;
    Comp sc = sceneComp(sky);
    img = render(sc, 0);
    CHECK(close(GetImageColor(img, W / 2, 3), RED, 3) && close(GetImageColor(img, W / 2, H - 3), BLUE, 3));
    UnloadImage(img);

    // A generated environment: used as given, generated once for any number of frames.
    auto gen = std::make_shared<g3d::Scene>();
    gen->camera.position = { 0, 0, 5 };
    gen->showEnvironment = true;
    int calls = 0;
    auto studio = g3d::studioEnvironment(RED, GRAY, BLUE, 0);
    gen->environmentGenerator = [&calls, studio] { calls++; return studio(); };
    Comp gc = sceneComp(gen);
    for (int f = 0; f < 2; f++) {
        img = render(gc, f);
        // The frame edges are ~22 degrees off the horizon: mostly top/bottom, part gray horizon.
        Color top = GetImageColor(img, W / 2, 0), bottom = GetImageColor(img, W / 2, H - 1);
        CHECK(top.r > top.b + 60 && bottom.b > bottom.r + 60);
        UnloadImage(img);
    }
    CHECK(calls == 1);

    // Rough surfaces see the blurred environment: a matte sphere's top is lit red, not blue.
    auto r = std::make_shared<g3d::Scene>();
    r->camera.position = { 0, 0, 5 };
    r->environment = path.string();
    g3d::Material matte;
    matte.roughness = 1;
    r->add(g3d::Geometry::sphere(1, 48, 64), matte);
    Comp rc = sceneComp(r);
    img = render(rc, 0);
    Color mt = px(img, project(r->camera, { 0, 0.9f, 0.4f }));
    CHECK(mt.r > mt.b + 40);
    UnloadImage(img);
}

// ---- cube-face skybox (L1)
namespace cube {
using DirFn = std::function<Color(Vector3)>;
// The direction through face `f` at (s, t) in 0..1 by the OpenGL cube-map table.
static Vector3 faceDirGl(int f, float s, float t) {
    const float sc = 2 * s - 1, tc = 2 * t - 1;
    Vector3 d;
    switch (f) {
    case 0: d = { 1, -tc, -sc }; break;
    case 1: d = { -1, -tc, sc }; break;
    case 2: d = { sc, 1, tc }; break;
    case 3: d = { sc, -1, -tc }; break;
    case 4: d = { sc, -tc, 1 }; break;
    default: d = { -sc, -tc, -1 }; break;
    }
    return Vector3Normalize(d);
}
// The WORLD direction: x negated, because raymotion is right-handed (the +x face lies toward -x).
static Vector3 faceDir(int f, float s, float t) {
    Vector3 d = faceDirGl(f, s, t);
    d.x = -d.x;
    return d;
}
static Image faceImage(int f, int n, const DirFn& fn) {
    Image img = GenImageColor(n, n, BLACK);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) ImageDrawPixel(&img, x, y, fn(faceDir(f, (x + 0.5f) / n, (y + 0.5f) / n)));
    return img;
}
static std::vector<std::string> writeFaces(const fs::path& dir, const std::string& name, int n, const DirFn& fn) {
    static const char* tags[6] = { "px", "nx", "py", "ny", "pz", "nz" };
    std::vector<std::string> files;
    for (int f = 0; f < 6; f++) {
        Image img = faceImage(f, n, fn);
        files.push_back((dir / (name + "-" + tags[f] + ".png")).string());
        ExportImage(img, files.back().c_str());
        UnloadImage(img);
    }
    return files;
}
static std::string writeCross(const fs::path& dir, const std::string& name, int n, const DirFn& fn) {
    const int col[6] = { 2, 0, 1, 1, 1, 3 }, row[6] = { 1, 1, 0, 2, 1, 1 };
    Image cross = GenImageColor(4 * n, 3 * n, BLACK);
    for (int f = 0; f < 6; f++) {
        Image img = faceImage(f, n, fn);
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) ImageDrawPixel(&cross, col[f] * n + x, row[f] * n + y, GetImageColor(img, x, y));
        UnloadImage(img);
    }
    const std::string path = (dir / (name + "-cross.png")).string();
    ExportImage(cross, path.c_str());
    UnloadImage(cross);
    return path;
}
// The same function as an equirectangular image, in the shader's mapping.
static std::string writeEquirect(const fs::path& dir, const std::string& name, int w, const DirFn& fn) {
    Image img = GenImageColor(w, w / 2, BLACK);
    for (int y = 0; y < w / 2; y++)
        for (int x = 0; x < w; x++) {
            const float phi = ((x + 0.5f) / w - 0.5f) * 2 * PI, theta = (y + 0.5f) / (w / 2) * PI;
            ImageDrawPixel(&img, x, y, fn({ std::cos(phi) * std::sin(theta), std::cos(theta), std::sin(phi) * std::sin(theta) }));
        }
    const std::string path = (dir / (name + "-eq.png")).string();
    ExportImage(img, path.c_str());
    UnloadImage(img);
    return path;
}
static Color gradient(Vector3 d) {
    auto q = [](float v) { return (unsigned char)std::clamp((int)(128 + 120 * v + 0.5f), 0, 255); };
    return { q(d.x), q(d.y), q(d.z), 255 };
}
// A camera at the origin looking along `dir`.
static std::shared_ptr<g3d::Scene> skyScene(Vector3 dir) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 0, 0 };
    s->camera.target = Vector3Normalize(dir);
    s->camera.upVector = std::fabs(dir.y) > 0.9f * Vector3Length(dir) ? Vector3{ 0, 0, 1 } : Vector3{ 0, 1, 0 };
    s->camera.fov = 60;
    s->showEnvironment = true;
    return s;
}
struct Diff { double mean = 0; int max = 0, bandMax = 0; };
// Mean and maximum absolute difference of two images; bandMax over the centre 5 columns and rows.
static Diff diff(const Image& a, const Image& b) {
    Diff d;
    long long sum = 0;
    for (int y = 0; y < a.height; y++)
        for (int x = 0; x < a.width; x++) {
            Color p = GetImageColor(a, x, y), q = GetImageColor(b, x, y);
            const int m = std::max({ std::abs(p.r - q.r), std::abs(p.g - q.g), std::abs(p.b - q.b) });
            sum += std::abs(p.r - q.r) + std::abs(p.g - q.g) + std::abs(p.b - q.b);
            d.max = std::max(d.max, m);
            if (std::abs(x - a.width / 2) <= 2 || std::abs(y - a.height / 2) <= 2) d.bandMax = std::max(d.bandMax, m);
        }
    d.mean = sum / (3.0 * a.width * a.height);
    return d;
}
static Image look(std::shared_ptr<g3d::Scene> s) { Comp c = sceneComp(s); return render(c, 0); }

static std::string errorOf(std::shared_ptr<g3d::Scene> s) {
    try { Image i = look(s); UnloadImage(i); } catch (const std::runtime_error& e) { return e.what(); }
    return "";
}
} // namespace cube

static void cubeSkybox(const fs::path& dir) {
    using namespace cube;
    // 1. Six solid colours: each axis shows its face's colour at the centre.
    const Color solid[6] = { { 220, 40, 40, 255 }, { 40, 200, 200, 255 }, { 40, 200, 40, 255 },
                             { 200, 40, 200, 255 }, { 40, 40, 220, 255 }, { 220, 220, 40, 255 } };
    const Vector3 axes[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    const char* names[6] = { "+x", "-x", "+y", "-y", "+z", "-z" };
    std::vector<std::string> solidFiles;
    for (int f = 0; f < 6; f++) {
        Image img = GenImageColor(64, 64, solid[f]);
        solidFiles.push_back((dir / (std::string("cube-solid") + std::to_string(f) + ".png")).string());
        ExportImage(img, solidFiles.back().c_str());
        UnloadImage(img);
    }
    for (int f = 0; f < 6; f++) {
        auto s = skyScene(axes[f]);
        s->environmentFaces = solidFiles;
        Image img = look(s);
        Color c = GetImageColor(img, W / 2, H / 2);
        const int face = f < 2 ? 1 - f : f;         // the faces labelled +x and -x swap sides
        std::printf("[scene3d] cube world %s shows face %s: centre rgb %d %d %d (face colour %d %d %d)\n", names[f], names[face],
                    c.r, c.g, c.b, solid[face].r, solid[face].g, solid[face].b);
        CHECK(close(c, solid[face], 6));
        UnloadImage(img);
    }

    // 1b. Orientation: the four side faces carry a left-third red / right-third blue marker and
    // must show red on the screen's left, blue on its right (unmirrored). +y has a red band along
    // its image bottom (the edge that meets +z), -y along its image top (also meets +z).
    {
        auto marked = [&](int f, int n) {
            Image img = GenImageColor(n, n, Color{ 128, 128, 128, 255 });
            for (int y = 0; y < n; y++)
                for (int x = 0; x < n; x++) {
                    Color c = { 128, 128, 128, 255 };
                    if (f < 2 || f > 3) { if (x < n / 3) c = RED; else if (x >= n - n / 3) c = BLUE; }
                    else if (f == 2 && y >= n - n / 6) c = RED;
                    else if (f == 3 && y < n / 6) c = RED;
                    ImageDrawPixel(&img, x, y, c);
                }
            return img;
        };
        static const char* tags[6] = { "px", "nx", "py", "ny", "pz", "nz" };
        std::vector<std::string> files;
        for (int f = 0; f < 6; f++) {
            Image img = marked(f, 96);
            files.push_back((dir / (std::string("cube-mark-") + tags[f] + ".png")).string());
            ExportImage(img, files.back().c_str());
            UnloadImage(img);
        }
        auto centroid = [](const Image& img, bool wantRed, float& cx, float& cy) {
            double sx = 0, sy = 0; int n = 0;
            for (int y = 0; y < img.height; y++)
                for (int x = 0; x < img.width; x++) {
                    Color c = GetImageColor(img, x, y);
                    const bool hit = wantRed ? (c.r > 200 && c.g < 60 && c.b < 60) : (c.b > 200 && c.r < 60 && c.g < 150);
                    if (hit) { sx += x; sy += y; n++; }
                }
            cx = n ? (float)(sx / n) : -1; cy = n ? (float)(sy / n) : -1;
            return n;
        };
        const int sides[4] = { 0, 1, 4, 5 };
        for (int f : sides) {
            auto s = skyScene(f == 0 ? Vector3{ -1, 0, 0 } : f == 1 ? Vector3{ 1, 0, 0 } : axes[f]);   // where each face lies
            s->environmentFaces = files;
            Image img = look(s);
            float rx, ry, bx, by;
            const int nr = centroid(img, true, rx, ry), nb = centroid(img, false, bx, by);
            std::printf("[scene3d] cube face %s: red x %.1f (%d px), blue x %.1f (%d px), screen centre %d\n", names[f], rx, nr, bx, nb, W / 2);
            CHECK(nr > 100 && nb > 100 && rx < W / 2 - 10 && bx > W / 2 + 10);
            UnloadImage(img);
        }
        for (int f : { 2, 3 }) {
            auto s = skyScene({ 0, f == 2 ? 1.f : -1.f, 0.6f });
            s->environmentFaces = files;
            Image img = look(s);
            float rx, ry;
            const int nr = centroid(img, true, rx, ry);
            std::printf("[scene3d] cube face %s tilted toward +z: red band y %.1f (%d px), screen centre %d; expected %s\n", names[f], ry, nr, H / 2,
                        f == 2 ? "below centre (+z is down the screen when looking up)" : "above centre (+z is up the screen when looking down)");
            CHECK(nr > 100);
            CHECK(f == 2 ? ry > H / 2 + 10 : ry < H / 2 - 10);
            UnloadImage(img);
        }
    }

    // 1c. HDR faces stay linear: a Radiance .hdr face of 4.0 (rgbe 128,128,128,131) at
    // intensity 0.15 shows 0.6 linear (about 203 in sRGB); clamped to 1 first it would be ~107.
    {
        std::vector<std::string> hdr;
        for (int f = 0; f < 6; f++) {
            const std::string path = (dir / (std::string("cube-hdr") + std::to_string(f) + ".hdr")).string();
            std::ofstream out(path, std::ios::binary);
            out << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 4 +X 4\n";
            for (int i = 0; i < 16; i++) { const unsigned char px4[4] = { 128, 128, 128, 131 }; out.write((const char*)px4, 4); }
            hdr.push_back(path);
        }
        auto s = skyScene({ 0, 0, 1 });
        s->environmentFaces = hdr;
        s->environmentIntensity = 0.15f;
        Image img = look(s);
        Color c = GetImageColor(img, W / 2, H / 2);
        std::printf("[scene3d] cube HDR face 4.0 at intensity 0.15: centre rgb %d %d %d\n", c.r, c.g, c.b);
        CHECK(c.r > 170 && c.r < 235 && std::abs(c.r - c.g) <= 3);
        UnloadImage(img);
    }

    // 2. A smooth sky as faces vs as an equirectangular image of the same detail: no seams.
    const int n = 256;
    auto faces = writeFaces(dir, "cube-grad", n, gradient);
    const std::string eq = writeEquirect(dir, "cube-grad", 4 * n, gradient);
    const Vector3 views[] = { { 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 1 }, { -1, 0, 1 }, { 1, 0, -1 }, { -1, 0, -1 },
                              { 1, 1, 0 }, { 0, 1, 1 }, { -1, -1, 0 }, { 0, -1, -1 }, { 1, 1, 1 }, { -1, 1, 1 }, { 1, -1, -1 }, { -1, -1, 1 },
                              { 0.9f, 0.2f, 0.3f }, { -0.3f, 0.5f, -0.8f } };
    double meanSum = 0;
    int worstMax = 0, worstBand = 0, count = 0;
    for (const Vector3& v : views) {
        auto a = skyScene(v), b = skyScene(v);
        a->environmentFaces = faces;
        b->environment = eq;
        Image ia = look(a), ib = look(b);
        Diff d = diff(ia, ib);
        meanSum += d.mean; worstMax = std::max(worstMax, d.max); worstBand = std::max(worstBand, d.bandMax); count++;
        UnloadImage(ia); UnloadImage(ib);
    }
    std::printf("[scene3d] cube seams: %d views, mean abs diff %.3f levels, worst max %d, worst centre-band max %d\n",
                count, meanSum / count, worstMax, worstBand);
    CHECK(meanSum / count < 1.5);
    CHECK(worstBand <= 8);          // no seam spike where the view crosses an edge or corner
    CHECK(worstMax <= 10);

    // 3. A matte sphere lit by the sky: faces vs equirectangular.
    double litDiff = 0;
    for (float yaw : { 0.f, 70.f, 200.f }) {
        Diff d;
        Image imgs[2];
        for (int k = 0; k < 2; k++) {
            auto s = std::make_shared<g3d::Scene>();
            s->camera.position = { 0, 0.5f, 4 };
            if (k == 0) s->environmentFaces = faces; else s->environment = eq;
            s->environmentRotation = yaw;
            g3d::Material m;
            m.color = Color{ 200, 200, 200, 255 };
            m.roughness = 1;
            s->add(g3d::Geometry::sphere(1, 48, 64), m);
            imgs[k] = look(s);
        }
        d = diff(imgs[0], imgs[1]);
        litDiff = std::max(litDiff, d.mean);
        std::printf("[scene3d] cube lighting yaw %.0f: mean abs diff %.3f max %d (sphere luma %d)\n", yaw, d.mean, d.max,
                    luma(GetImageColor(imgs[0], W / 2, H / 2)));
        CHECK(luma(GetImageColor(imgs[0], W / 2, H / 2)) > 20);          // it is lit, not black
        UnloadImage(imgs[0]); UnloadImage(imgs[1]);
    }
    CHECK(litDiff < 2.0);

    // 4. The horizontal cross gives the same picture as the six files.
    const std::string cross = writeCross(dir, "cube-grad", n, gradient);
    int crossMax = 0;
    double crossMean = 0;
    for (const Vector3& v : views) {
        auto a = skyScene(v), b = skyScene(v);
        a->environmentFaces = faces;
        b->environmentFaces = { cross };
        Image ia = look(a), ib = look(b);
        Diff d = diff(ia, ib);
        crossMax = std::max(crossMax, d.max); crossMean += d.mean / (double)(sizeof(views) / sizeof(views[0]));
        UnloadImage(ia); UnloadImage(ib);
    }
    std::printf("[scene3d] cube cross vs six files: mean abs diff %.4f, max %d\n", crossMean, crossMax);
    CHECK(crossMax <= 1);

    // 5. Precedence: a file beats faces, and faces beat the generator.
    {
        auto s = skyScene({ 1, 0, 0 });
        s->environmentFaces = solidFiles;
        s->environment = eq;
        Image i = look(s);
        Color c = GetImageColor(i, W / 2, H / 2);
        CHECK(!close(c, solid[1], 20));
        UnloadImage(i);
        auto g = skyScene({ 1, 0, 0 });
        g->environmentFaces = solidFiles;
        g->environmentGenerator = g3d::studioEnvironment(GREEN, GREEN, GREEN, 0);
        i = look(g);
        CHECK(close(GetImageColor(i, W / 2, H / 2), solid[1], 6));
        UnloadImage(i);
    }

    // 6. Errors name the problem.
    auto five = skyScene({ 1, 0, 0 });
    five->environmentFaces = std::vector<std::string>(solidFiles.begin(), solidFiles.begin() + 5);
    const std::string e5 = errorOf(five);
    Image rect = GenImageColor(32, 16, WHITE);
    const std::string rectPath = (dir / "cube-rect.png").string();
    ExportImage(rect, rectPath.c_str());
    UnloadImage(rect);
    auto nonSquare = skyScene({ 1, 0, 0 });
    nonSquare->environmentFaces = solidFiles;
    nonSquare->environmentFaces[3] = rectPath;
    const std::string eRect = errorOf(nonSquare);
    auto missing = skyScene({ 1, 0, 0 });
    missing->environmentFaces = solidFiles;
    missing->environmentFaces[2] = (dir / "no-such-face.png").string();
    const std::string eMissing = errorOf(missing);
    std::printf("[scene3d] cube errors:\n  five: %s\n  non-square: %s\n  missing: %s\n", e5.c_str(), eRect.c_str(), eMissing.c_str());
    CHECK(e5.find("six") != std::string::npos);
    CHECK(eRect.find("square") != std::string::npos && eRect.find("cube-rect") != std::string::npos);
    CHECK(eMissing.find("no-such-face") != std::string::npos);
}

// A box that glows white (emissive, so it shows without lights) and takes fog.
static g3d::Material glowing() {
    g3d::Material m;
    m.color = BLACK;
    m.emissive = WHITE;
    m.roughness = 1;
    return m;
}

static void skyAndFog() {
    // Presets: the sun is up at noon, down at midnight, low at dawn; mixing goes halfway.
    CHECK(g3d::Sky::at(12).sunDirection.y > 0.6f && g3d::Sky::at(0).sunDirection.y < -0.5f);
    CHECK(g3d::Sky::at(6.5f).sunDirection.x > 0.8f && g3d::Sky::at(17.5f).sunDirection.x < -0.8f);
    CHECK(g3d::Sky::at(0).stars > 0.9f && g3d::Sky::at(12).stars < 0.1f && g3d::Sky::at(12).enabled);
    const g3d::Sky half = g3d::Sky::mix(g3d::Sky::day(), g3d::Sky::night(), 0.5f);
    CHECK(std::abs(half.zenith.b - (g3d::Sky::day().zenith.b + g3d::Sky::night().zenith.b) / 2) <= 1);

    // The backdrop: bluer up, dark ground down, the sun where light 0 shines from.
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 1, 5 };
    s->camera.target = { 0, 1, 0 };
    s->sky = g3d::Sky::day();
    s->sky.clouds = 0;
    s->sky.sunLight = 0;
    s->lights.push_back(g3d::Light::directional({ 8, 12, -60 }, WHITE, 1));
    Comp c = sceneComp(s);
    Image img = render(c, 0);
    const Color top = GetImageColor(img, W / 2, 1), bottom = GetImageColor(img, W / 2, H - 2);
    CHECK(top.b > top.r + 30 && luma(bottom) + 40 < luma(top));
    const Vector3 toSun = Vector3Normalize({ 8, 12, -60 });
    const Vector2 sunPx = project(s->camera, Vector3Add(s->camera.position, Vector3Scale(toSun, 50)));
    CHECK(sunPx.x > 0 && sunPx.x < W && sunPx.y > 0 && sunPx.y < H);
    CHECK(luma(px(img, sunPx)) > 235 && luma(px(img, { W - 4.f, H / 2.f - 30 })) < 225);
    CHECK(allOpaque(img));
    UnloadImage(img);
    // With depth of field the sky still fills the frame (it must leave the 2D matrices as it found them).
    s->camera.dof = { 5, 0.5f, 6 };
    img = render(c, 0);
    for (Vector2 q : { Vector2{ 2, 2 }, Vector2{ W - 3.f, 2 }, Vector2{ 2, H - 3.f }, Vector2{ W - 3.f, H - 3.f } })
        CHECK(!close(px(img, q), BLACK, 12));
    CHECK(px(img, { W / 2.f, 1 }).b > px(img, { W / 2.f, 1 }).r + 30);
    UnloadImage(img);

    // It lights the scene: a matte sphere with no lights at all is lit from above, sky blue.
    auto lit = std::make_shared<g3d::Scene>();
    lit->camera.position = { 0, 0, 5 };
    lit->sky = g3d::Sky::day();
    lit->sky.show = false;
    g3d::Material matte;
    matte.roughness = 1;
    lit->add(g3d::Geometry::sphere(1, 48, 64), matte);
    Comp lc = sceneComp(lit);
    img = render(lc, 0);
    const Color crown = px(img, project(lit->camera, { 0, 0.9f, 0.4f })), belly = px(img, project(lit->camera, { 0, -0.9f, 0.4f }));
    CHECK(luma(crown) > 60 && crown.b > crown.r && luma(crown) > luma(belly) + 20);
    CHECK(close(GetImageColor(img, 3, 3), BLACK));               // show = false: no backdrop
    UnloadImage(img);

    // Remade each frame: clouds drift.
    auto cl = std::make_shared<g3d::Scene>();
    cl->camera.position = { 0, 0, 0 };
    cl->camera.target = { 0, 1, -0.3f };
    cl->sky = g3d::Sky::day();
    cl->sky.clouds = 0.6f;
    cl->sky.cloudDrift = { 0.01f, 0 };
    Comp cc = sceneComp(cl);
    Image a = render(cc, 0), b = render(cc, 6);
    int changed = 0;
    for (int y = 0; y < H; y += 4)
        for (int x = 0; x < W; x += 4) changed += !close(GetImageColor(a, x, y), GetImageColor(b, x, y), 6);
    CHECK(changed > 50);
    UnloadImage(a);
    UnloadImage(b);

    // Exponential fog: a far glowing box is red with fog, a near one still white.
    auto f = std::make_shared<g3d::Scene>();
    f->camera.position = { 0, 0, 0 };
    f->camera.target = { 0, 0, -1 };
    f->fogMode = g3d::FogMode::Exponential;
    f->fogDensity = 0.12f;
    f->fogColor = RED;
    f->add(g3d::Geometry::box(0.5f, 0.5f, 0.5f), glowing())->position = { -0.6f, 0, -2 };
    f->add(g3d::Geometry::box(5, 5, 5), glowing())->position = { 12, 0, -30 };
    Comp fc = sceneComp(f);
    img = render(fc, 0);
    const Color nearC = px(img, project(f->camera, { -0.6f, 0, -1.75f })), farC = px(img, project(f->camera, { 12, 0, -27.5f }));
    CHECK(nearC.g > 170 && farC.r > farC.g + 150);
    UnloadImage(img);

    // Exponential squared: clearer near, as thick far.
    f->fogMode = g3d::FogMode::Exponential2;
    img = render(fc, 0);
    const Color near2 = px(img, project(f->camera, { -0.6f, 0, -1.75f }));
    CHECK(near2.g > nearC.g);
    UnloadImage(img);

    // Height fog: thick low down, thin up high, at the same distance.
    auto hf = std::make_shared<g3d::Scene>();
    hf->camera.position = { 0, 0, 0 };
    hf->camera.target = { 0, 0, -1 };
    hf->fogMode = g3d::FogMode::Exponential;
    hf->fogDensity = 1;
    hf->fogHeight = -2;
    hf->fogFalloff = 1.5f;
    hf->fogColor = RED;
    hf->add(g3d::Geometry::box(1, 1, 1), glowing())->position = { 0, -2.2f, -9 };
    hf->add(g3d::Geometry::box(1, 1, 1), glowing())->position = { 0, 2.2f, -9 };
    Comp hc = sceneComp(hf);
    img = render(hc, 0);
    const Color low = px(img, project(hf->camera, { 0, -2.2f, -8.5f })), high = px(img, project(hf->camera, { 0, 2.2f, -8.5f }));
    CHECK(low.r > low.g + 100 && high.g > 150);
    UnloadImage(img);

    // Fog from the sky: a far box melts into the backdrop around it.
    auto fs3 = std::make_shared<g3d::Scene>();
    fs3->camera.position = { 0, 0, 0 };
    fs3->camera.target = { 0, 0, -1 };
    fs3->sky = g3d::Sky::dusk();
    fs3->sky.clouds = 0;
    fs3->fogMode = g3d::FogMode::Exponential;
    fs3->fogDensity = 0.08f;
    fs3->fogFromSky = true;
    fs3->fogColor = GREEN;                                        // not used
    fs3->add(g3d::Geometry::box(20, 6, 1), glowing())->position = { 0, 0, -120 };
    Comp sc = sceneComp(fs3);
    img = render(sc, 0);
    const Vector2 mid = project(fs3->camera, { 0, 0, -119.5f });
    CHECK(close(px(img, mid), px(img, { mid.x, mid.y - 30 }), 18));
    CHECK(px(img, mid).g < 200 || px(img, mid).r > 150);         // sky colours, not green
    UnloadImage(img);
}

static void depthOfField() {
    // Counts partially covered (blurred) pixels across a box's edges along one row.
    auto softness = [](const Image& img, int y, int x0, int x1) {
        int n = 0;
        for (int x = x0; x < x1; x++) { int v = GetImageColor(img, x, y).r; n += v > 25 && v < 230; }
        return n;
    };
    int nearSoft[2], farSoft[2];
    for (int k = 0; k < 2; k++) {
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 0, 5 };
        s->camera.target = { 0, 0, 0 };
        if (k) s->camera.dof = { 3, 1, 8 };                                     // focus on the near box
        s->add(g3d::Geometry::box(0.6f, 0.6f, 0.6f), unlit(WHITE))->position = { -0.9f, 0, 2 };
        s->add(g3d::Geometry::box(3, 3, 1), unlit(WHITE))->position = { 7, 0, -10 };
        Comp c = sceneComp(s);
        Image img = render(c, 0);
        const int nearY = (int)project(s->camera, { -0.9f, 0, 2 }).y, farY = (int)project(s->camera, { 7, 0, -10 }).y;
        nearSoft[k] = softness(img, nearY, 0, W / 2);
        farSoft[k] = softness(img, farY, W / 2, W);
        CHECK(allOpaque(img));
        UnloadImage(img);
    }
    CHECK(farSoft[0] <= 2 && farSoft[1] >= 6);                                  // far edges blur
    CHECK(nearSoft[1] <= nearSoft[0] + 2);                                      // the focused box stays sharp

    // A tilted plane of focus: a far box high in the frame and a near one low are both off
    // the focus distance, so both blur, until the plane leans back through the two of them.
    int topSoft[2], lowSoft[2];
    for (int k = 0; k < 2; k++) {
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 0, 0 };
        s->camera.target = { 0, 0, -1 };
        s->camera.dof = { 13.33f, 3, 10, k ? 48.f : 0.f };
        s->add(g3d::Geometry::box(2, 2, 0.1f), unlit(WHITE))->position = { 0, 6, -20 };
        s->add(g3d::Geometry::box(1, 1, 0.1f), unlit(WHITE))->position = { 0, -3, -10 };
        Comp c = sceneComp(s);
        Image img = render(c, 0);
        topSoft[k] = softness(img, (int)project(s->camera, { 0, 6, -20 }).y, 0, W);
        lowSoft[k] = softness(img, (int)project(s->camera, { 0, -3, -10 }).y, 0, W);
        UnloadImage(img);
    }
    CHECK(topSoft[0] >= 6 && lowSoft[0] >= 6);
    CHECK(topSoft[1] <= 3 && lowSoft[1] <= 3);
}

static void determinism() {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 3, 6 };
    g3d::Object3D* knot = s->add(g3d::Geometry::knot(1, 0.35f), g3d::Material{ ORANGE, 0.3f });
    knot->up = [](int t, g3d::Object3D& o) { o.rotation = { t * 7.f, t * 3.f, 0 }; };
    s->lights.push_back(g3d::Light::directional({ 3, 5, 2 }, WHITE, 1.2f, true));
    Comp c = sceneComp(s);
    Image a = render(c, 4), b = render(c, 7), a2 = render(c, 4);
    CHECK(std::memcmp(a.data, a2.data, (size_t)W * H * 4) == 0);
    CHECK(std::memcmp(a.data, b.data, (size_t)W * H * 4) != 0);
    UnloadImage(a); UnloadImage(b); UnloadImage(a2);
}

// ---- stylised shading and ink lines
// A sphere lit from the upper left, in `m`'s shading; returns the frame.
static Image litSphere(g3d::Material m, int frame = 0) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 0, 5 };
    s->add(g3d::Geometry::sphere(1.4f, 48, 64), m);
    s->lights.push_back(g3d::Light::directional({ -4, 3, 3 }, WHITE, 1.0f));
    s->ambient = 0.3f;
    Comp c = sceneComp(s);
    return render(c, frame);
}

static void toonShading() {
    g3d::Material m{ mkColor(0x3080ff) };
    m.shading = g3d::Shading::Toon;
    m.toon.bands = 2; m.toon.softness = 0; m.toon.highlight = 0;
    Image toon = litSphere(m);
    m.shading = g3d::Shading::Standard;
    Image smooth = litSphere(m);
    // Across the sphere's equator: a few flat bands instead of a continuous ramp.
    auto distinct = [](const Image& img) {
        std::vector<int> seen;
        for (int x = W / 2 - 60; x <= W / 2 + 60; x++) {
            int l = luma(GetImageColor(img, x, H / 2));
            if (std::find(seen.begin(), seen.end(), l) == seen.end()) seen.push_back(l);
        }
        return seen.size();
    };
    CHECK(distinct(toon) <= 6);                         // 3 levels plus edge pixels between them
    CHECK(distinct(smooth) > 20);
    // The lit side is uniform within a band; the far side is the darker ambient tone.
    Color a = GetImageColor(toon, W / 2 - 30, H / 2 - 10), b = GetImageColor(toon, W / 2 - 22, H / 2 - 4);
    CHECK(close(a, b, 1));
    CHECK(luma(GetImageColor(toon, W / 2 + 50, H / 2 + 10)) < luma(a));
    UnloadImage(toon); UnloadImage(smooth);

    // A rim light brightens the silhouette.
    m.shading = g3d::Shading::Toon;
    m.toon.rim = Color{ 255, 255, 255, 255 }; m.toon.rimWidth = 0.3f;
    Image rim = litSphere(m);
    m.toon.rim = BLANK;
    Image plain = litSphere(m);
    const int edge = W / 2 + 54;                        // just inside the right silhouette (radius ~56 px)
    CHECK(luma(GetImageColor(rim, edge, H / 2)) > luma(GetImageColor(plain, edge, H / 2)) + 40);
    UnloadImage(rim); UnloadImage(plain);
}

static void goochShading() {
    g3d::Material m{ GRAY };
    m.shading = g3d::Shading::Gooch;
    Image img = litSphere(m);
    Color lit = GetImageColor(img, W / 2 - 35, H / 2 - 20), dark = GetImageColor(img, W / 2 + 40, H / 2 + 25);
    CHECK(lit.r > lit.b);                                // warm toward the light
    CHECK(dark.b > dark.r);                              // cool away from it
    CHECK(luma(dark) > 30);                              // the far side keeps a tone, not black
    UnloadImage(img);
}

static void hatchShading() {
    g3d::Material m{ WHITE };
    m.shading = g3d::Shading::Hatch;
    m.hatch.ink = BLACK; m.hatch.spacing = 5;
    Image img = litSphere(m);
    // Ink coverage in a 20 x 20 patch: little toward the light, much more in shadow.
    auto ink = [&](int cx, int cy) {
        int n = 0;
        for (int y = cy - 10; y < cy + 10; y++)
            for (int x = cx - 10; x < cx + 10; x++) n += luma(GetImageColor(img, x, y)) < 128;
        return n;
    };
    const int litInk = ink(W / 2 - 25, H / 2 - 18), darkInk = ink(W / 2 + 32, H / 2 + 22);
    CHECK(litInk < 60);
    CHECK(darkInk > 100 && darkInk > 3 * litInk);
    CHECK(close(GetImageColor(img, 3, 3), BLACK));      // the background is not hatched
    UnloadImage(img);
}

// A white unlit box on blue with red lines; `wrap` puts the scene node inside a layer.
static Image inkedBox(Rectangle viewport, bool wrap, float lineWidth = 1, int frame = 0, float wobble = 0, int ss = 1,
                      Filter filter = Filter::posterize(16)) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 0, 5 };
    g3d::Material m = unlit(WHITE);
    m.lineWidth = lineWidth;
    s->add(g3d::Geometry::box(1.5f, 1.5f, 1.5f), m)->rotation = { 25, 35, 0 };
    s->lines.enabled = true;
    s->lines.color = RED;
    s->lines.width = 4;
    s->lines.wobble = wobble;
    Comp c;
    c.w = W; c.h = H; c.duration = 10; c.background = BLUE;
    TNode* n = addRoot(c.roots, 0, 10);
    if (wrap) n->isolate().filters = { filter };
    n->add(0, 10)->fn = g3d::draw(s, viewport);
    return render(c, frame, ss);
}
static int countColor(const Image& img, Color want, int tol = 40) {
    int n = 0;
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++) n += close(GetImageColor(img, x, y), want, tol);
    return n;
}

static void inkLines() {
    Image img = inkedBox({}, false);
    CHECK(countColor(img, RED) > 400);                  // silhouette plus the visible creases
    CHECK(close(px(img, { W / 2.f + 4, H / 2.f + 12 }), WHITE, 40) || close(px(img, { W / 2.f + 4, H / 2.f + 12 }), RED, 40));
    CHECK(close(GetImageColor(img, 3, 3), BLUE));       // far from the box: untouched
    // The silhouette: walking left from the centre, red comes before blue.
    int x = W / 2;
    while (x > 0 && !close(GetImageColor(img, x, H / 2), BLUE, 40)) x--;
    bool redBeforeBlue = false;
    for (int k = x; k < x + 8; k++) redBeforeBlue |= close(GetImageColor(img, k, H / 2), RED, 60);
    CHECK(redBeforeBlue);
    const int full = countColor(img, RED);
    UnloadImage(img);

    // lineWidth 0 opts an object out.
    Image none = inkedBox({}, false, 0);
    CHECK(countColor(none, RED) == 0);
    UnloadImage(none);

    // Inside a layer, and inside a viewport, the lines are still drawn; a viewport's edge
    // is not a line even where it cuts the box.
    Image layered = inkedBox({}, true);
    CHECK(countColor(layered, RED) > full * 9 / 10);
    UnloadImage(layered);
    Image half = inkedBox({ 0, 0, (float)W, (float)H }, false);
    CHECK(countColor(half, RED) > full * 9 / 10);
    UnloadImage(half);
    Image cut = inkedBox({ W / 2.f, 0, W / 2.f, (float)H }, false);
    CHECK(countColor(cut, RED) > 100);
    for (int y = 10; y < H - 10; y++) CHECK(!close(GetImageColor(cut, W / 2 + 1, y), RED, 40));
    UnloadImage(cut);

    // Wobble boils on twos: frames 0 and 1 match, frame 2 moves, and a frame re-renders the same.
    Image w0 = inkedBox({}, false, 1, 0, 3), w1 = inkedBox({}, false, 1, 1, 3), w2 = inkedBox({}, false, 1, 2, 3),
          again = inkedBox({}, false, 1, 2, 3);
    CHECK(std::memcmp(w0.data, w1.data, (size_t)W * H * 4) == 0);
    CHECK(std::memcmp(w0.data, w2.data, (size_t)W * H * 4) != 0);
    CHECK(std::memcmp(w2.data, again.data, (size_t)W * H * 4) == 0);
    UnloadImage(w0); UnloadImage(w1); UnloadImage(w2); UnloadImage(again);

    // At 2x supersampling (widths stay in composition px), in a layer and in an inset viewport.
    for (int mode = 0; mode < 3; mode++) {
        Rectangle vp = mode == 2 ? Rectangle{ 20, 10, (float)W - 40, (float)H - 20 } : Rectangle{};
        Image ss = inkedBox(vp, mode == 1, 1, 0, 0, 2);
        CHECK(countColor(ss, RED) > full * 7 / 10);
        UnloadImage(ss);
    }
    // A layer's filters run on the inked result.
    for (Filter f : { Filter::paperGrain(0.01f), Filter::inkEdges(0.5f, BLACK, 5) }) {
        Image inked = inkedBox({}, true, 1, 0, 0, 1, f);
        CHECK(countColor(inked, RED, 80) > full * 9 / 10);
        UnloadImage(inked);
    }
}

// ---- models
static void writeObjCube(const fs::path& path) {
    std::ofstream f(path);
    f << "v -0.5 -0.5 0.5\nv 0.5 -0.5 0.5\nv 0.5 0.5 0.5\nv -0.5 0.5 0.5\n"
         "v -0.5 -0.5 -0.5\nv 0.5 -0.5 -0.5\nv 0.5 0.5 -0.5\nv -0.5 0.5 -0.5\n"
         "f 1 2 3 4\nf 6 5 8 7\nf 5 1 4 8\nf 2 6 7 3\nf 4 3 7 8\nf 5 6 2 1\n";
}

static std::string base64(const std::vector<unsigned char>& d) {
    static const char* A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < d.size(); i += 3) {
        unsigned v = d[i] << 16 | (i + 1 < d.size() ? d[i + 1] << 8 : 0) | (i + 2 < d.size() ? d[i + 2] : 0);
        out += A[v >> 18 & 63]; out += A[v >> 12 & 63];
        out += i + 1 < d.size() ? A[v >> 6 & 63] : '=';
        out += i + 2 < d.size() ? A[v & 63] : '=';
    }
    return out;
}

// A 0.5 x 2 bar standing on the origin, skinned to a root joint and a knee joint at
// y = 1; one animation bends the knee 90 degrees about z over one second.
static void writeSkinnedBar(const fs::path& path) {
    std::vector<unsigned char> buf;
    auto put = [&](const void* p, size_t n) { auto b = (const unsigned char*)p; buf.insert(buf.end(), b, b + n); };
    auto align = [&] { while (buf.size() % 4) buf.push_back(0); };
    Json::Array views, accessors;
    auto view = [&](size_t offset, size_t length) {
        views.push_back(Json::Object{ { "buffer", 0 }, { "byteOffset", (int)offset }, { "byteLength", (int)length } });
        return (int)views.size() - 1;
    };
    auto accessor = [&](int v, int componentType, int count, const char* type, Json extra = Json()) {
        Json a = Json::Object{ { "bufferView", v }, { "componentType", componentType }, { "count", count }, { "type", type } };
        if (extra.isObject()) for (auto& [k, val] : extra.asObject()) a.set(k, val);
        accessors.push_back(a);
        return (int)accessors.size() - 1;
    };
    const int rows = 5;
    std::vector<float> pos, nrm, weights;
    std::vector<unsigned char> joints;
    for (int r = 0; r < rows; r++)
        for (int col = 0; col < 2; col++) {
            float y = r * 0.5f;
            pos.insert(pos.end(), { col ? 0.25f : -0.25f, y, 0 });
            nrm.insert(nrm.end(), { 0, 0, 1 });
            float w1 = y <= 0.5f ? 0.f : y >= 1.5f ? 1.f : 0.5f;
            joints.insert(joints.end(), { 0, 1, 0, 0 });
            weights.insert(weights.end(), { 1 - w1, w1, 0, 0 });
        }
    std::vector<unsigned short> idx;
    for (int r = 0; r + 1 < rows; r++) {
        unsigned short a = (unsigned short)(r * 2), b = (unsigned short)(a + 1), c2 = (unsigned short)(a + 2), d = (unsigned short)(a + 3);
        idx.insert(idx.end(), { a, b, d, a, d, c2 });
    }
    const int n = rows * 2;
    size_t o = buf.size(); put(pos.data(), pos.size() * 4); align();
    int aPos = accessor(view(o, pos.size() * 4), 5126, n, "VEC3",
                        Json::Object{ { "min", Json::Array{ -0.25, 0, 0 } }, { "max", Json::Array{ 0.25, 2, 0 } } });
    o = buf.size(); put(nrm.data(), nrm.size() * 4); align();
    int aNrm = accessor(view(o, nrm.size() * 4), 5126, n, "VEC3");
    o = buf.size(); put(joints.data(), joints.size()); align();
    int aJoints = accessor(view(o, joints.size()), 5121, n, "VEC4");
    o = buf.size(); put(weights.data(), weights.size() * 4); align();
    int aWeights = accessor(view(o, weights.size() * 4), 5126, n, "VEC4");
    o = buf.size(); put(idx.data(), idx.size() * 2); align();
    int aIdx = accessor(view(o, idx.size() * 2), 5123, (int)idx.size(), "SCALAR");
    const float ibm[32] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1,      1,0,0,0, 0,1,0,0, 0,0,1,0, 0,-1,0,1 };
    o = buf.size(); put(ibm, sizeof ibm);
    int aIbm = accessor(view(o, sizeof ibm), 5126, 2, "MAT4");
    const float times[2] = { 0, 1 };
    o = buf.size(); put(times, sizeof times);
    int aTimes = accessor(view(o, sizeof times), 5126, 2, "SCALAR",
                          Json::Object{ { "min", Json::Array{ 0 } }, { "max", Json::Array{ 1 } } });
    const float s45 = 0.70710678f, rots[8] = { 0, 0, 0, 1, 0, 0, s45, s45 };
    o = buf.size(); put(rots, sizeof rots);
    int aRots = accessor(view(o, sizeof rots), 5126, 2, "VEC4");

    Json g = Json::Object{
        { "asset", Json::Object{ { "version", "2.0" } } },
        { "scene", 0 },
        { "scenes", Json::Array{ Json::Object{ { "nodes", Json::Array{ 0, 1 } } } } },
        { "nodes", Json::Array{
            Json::Object{ { "mesh", 0 }, { "skin", 0 } },
            Json::Object{ { "name", "root" }, { "children", Json::Array{ 2 } } },
            Json::Object{ { "name", "knee" }, { "translation", Json::Array{ 0, 1, 0 } } } } },
        { "meshes", Json::Array{ Json::Object{ { "primitives", Json::Array{ Json::Object{
            { "attributes", Json::Object{ { "POSITION", aPos }, { "NORMAL", aNrm }, { "JOINTS_0", aJoints }, { "WEIGHTS_0", aWeights } } },
            { "indices", aIdx } } } } } } },
        { "skins", Json::Array{ Json::Object{ { "inverseBindMatrices", aIbm }, { "joints", Json::Array{ 1, 2 } } } } },
        { "animations", Json::Array{ Json::Object{
            { "name", "bend" },
            { "channels", Json::Array{ Json::Object{ { "sampler", 0 },
                { "target", Json::Object{ { "node", 2 }, { "path", "rotation" } } } } } },
            { "samplers", Json::Array{ Json::Object{ { "input", aTimes }, { "output", aRots }, { "interpolation", "LINEAR" } } } } } } },
        { "buffers", Json::Array{ Json::Object{ { "byteLength", (int)buf.size() },
            { "uri", "data:application/octet-stream;base64," + base64(buf) } } } },
    };
    g.set("bufferViews", views);
    g.set("accessors", accessors);
    std::ofstream(path, std::ios::binary) << g.dump();
}

static void models(const fs::path& dir) {
    const fs::path obj = dir / "cube.obj", gltf = dir / "bar.gltf";
    writeObjCube(obj);
    writeSkinnedBar(gltf);

    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 0, 5 };
    g3d::Object3D* cube = s->add(g3d::loadModel(obj.string()));
    cube->modelMaterials = false;
    cube->material = unlit(RED);
    cube->material.doubleSided = true;
    Comp c = sceneComp(s);
    Image img = render(c, 0);
    CHECK(close(GetImageColor(img, W / 2, H / 2), RED));
    UnloadImage(img);

    auto bar = g3d::loadModel(gltf.string());
    auto k = std::make_shared<g3d::Scene>();
    k->camera.position = { 0, 1, 5 };
    k->camera.target = { 0, 1, 0 };
    g3d::Object3D* o = k->add(bar);
    o->modelMaterials = false;
    o->material = unlit(GREEN);
    o->material.doubleSided = true;
    o->up = [bar](int t, g3d::Object3D& self) {
        self.animation = t == 0 ? -1 : 0;
        self.animationFrame = (float)(bar->animationFrames(0) - 1);           // fully bent
    };
    Comp kc = sceneComp(k);
    Image rest = render(kc, 0), bent = render(kc, 1);
    CHECK(bar->animationCount() == 1 && bar->animationIndex("bend") == 0 && bar->animationIndex("x") == -1);
    CHECK(bar->animationFrames(0) > 1);
    const Vector2 top = project(k->camera, { 0, 1.8f, 0 }), side = project(k->camera, { -0.8f, 1.0f, 0 });
    const Vector2 shin = project(k->camera, { 0, 0.25f, 0 });
    CHECK(close(px(rest, top), GREEN) && close(px(rest, side), BLACK));
    CHECK(close(px(bent, top), BLACK) && close(px(bent, side), GREEN));         // the top half bends left
    CHECK(close(px(rest, shin), GREEN) && close(px(bent, shin), GREEN));        // the root half stays
    UnloadImage(rest); UnloadImage(bent);

    // A missing file is an error at first draw, not a silent empty model.
    auto missing = std::make_shared<g3d::Scene>();
    missing->add(g3d::loadModel((dir / "nope.glb").string()));
    Comp mc = sceneComp(missing);
    bool threw = false;
    try { Image i = render(mc, 0); UnloadImage(i); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
}

// One still particle of `look` at `at`, `size` across.
static fx::Emitter3D still(Vector3 at, float size, Color c, fx::Look look = fx::Look::Dot) {
    fx::Emitter3D e;
    e.burst = 1;
    e.life = { 100, 100 };
    e.speed = { 0, 0 };
    e.size = { size, size };
    e.colors = { c };
    e.fadeIn = e.fadeOut = 0;
    e.look = look;
    e.origin = [at](float) { return at; };
    return e;
}

static void particles3d() {
    // A billboard lands where its centre projects, depth-tested against the meshes.
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 0, 5 };
    s->add(still({ -1, 0, 0 }, 0.6f, GREEN));                    // in the open
    s->add(still({ 1, 0, -1 }, 0.6f, YELLOW));                   // behind the box
    s->add(g3d::Geometry::box(1, 1, 0.2f), unlit(RED))->position = { 1.2f, 0, 0 };
    Comp c = sceneComp(s, BLUE);
    Image img = render(c, 5);
    CHECK(close(px(img, project(s->camera, { -1, 0, 0 })), GREEN, 6));
    CHECK(close(px(img, project(s->camera, { 1, 0, -1 })), RED));
    const Vector2 edge = project(s->camera, { -1.45f, 0, 0 });       // outside the 0.3 radius
    CHECK(close(px(img, edge), BLUE));
    UnloadImage(img);

    // Mesh particles: lit instances of the geometry; the emitter object itself is not drawn.
    auto m = std::make_shared<g3d::Scene>();
    m->camera.position = { 0, 0, 5 };
    fx::Emitter3D chunks = still({ 1, 0, 0 }, 0.8f, WHITE);
    chunks.rotation = { 0, 0 };
    g3d::Object3D* em = m->add(chunks, g3d::Geometry::box(1, 1, 1), unlit(ORANGE));
    em->position = { -0.5f, 0, 0 };                              // particles live in the object's space
    Comp mc = sceneComp(m, BLUE);
    img = render(mc, 5);
    CHECK(close(px(img, project(m->camera, { 0.5f, 0, 0 })), ORANGE));
    CHECK(close(px(img, project(m->camera, { -0.5f, 0, 0 })), BLUE));
    UnloadImage(img);

    // Additive glows add light; alpha-blended ones paint over.
    auto g = std::make_shared<g3d::Scene>();
    g->camera.position = { 0, 0, 5 };
    fx::Emitter3D glow = still({ 0, 0, 0 }, 1, Color{ 120, 60, 20, 255 }, fx::Look::Glow);
    glow.additive = true;
    g->add(glow);
    g->add(glow);
    Comp gc = sceneComp(g, Color{ 60, 60, 60, 255 });
    img = render(gc, 5);
    const Color centre = px(img, { W / 2.f, H / 2.f });
    CHECK(centre.r > 200 && centre.g > 150 && centre.b > 80);   // two glows and their hot cores over grey
    UnloadImage(img);

    // Fog takes far particles towards the fog colour, like the meshes.
    auto f = std::make_shared<g3d::Scene>();
    f->camera.position = { 0, 0, 5 };
    f->fogColor = BLUE;
    f->fogNear = 1;
    f->fogFar = 3;
    f->add(still({ 0, 0, -2 }, 1.2f, RED));
    Comp fc = sceneComp(f, BLACK);
    img = render(fc, 5);
    CHECK(close(px(img, { W / 2.f, H / 2.f }), BLUE, 3));
    UnloadImage(img);

    // Streaks, stars, rings and confetti draw something, and a frame renders the same twice.
    auto mix = std::make_shared<g3d::Scene>();
    mix->camera.position = { 0, 1, 6 };
    mix->add(fx::burst3d(80));
    mix->add(fx::confetti3d(4, 4))->position = { 0, 2, 0 };
    fx::Emitter3D stars = fx::burst3d(30);
    stars.look = fx::Look::Star;
    stars.size = { 0.2f, 0.3f };
    mix->add(stars);
    Comp xc = sceneComp(mix, BLACK);
    Image a = render(xc, 8), b = render(xc, 8);
    int lit = 0;
    bool same = true;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const Color p = GetImageColor(a, x, y), q = GetImageColor(b, x, y);
            lit += luma(p) > 40;
            same &= p.r == q.r && p.g == q.g && p.b == q.b;
        }
    CHECK(lit > 150 && same);
    UnloadImage(a);
    UnloadImage(b);
}

// Culling (Object3D::cullRadius): a subtree out of sight is not animated or drawn, yet the
// picture is the same pixel for pixel, shadows of what is out of frame included.
static void culling() {
    struct Ups { int near = 0, shadow = 0, far = 0; };
    auto run = [](bool cull, Ups& ups) {
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 2, 10 };
        s->camera.target = { 0, 1, 0 };
        s->camera.fov = 40;
        g3d::Material m;
        m.roughness = 1;
        s->add(g3d::Geometry::plane(60, 40), m);
        // A low sun from the left: the box left of the frame casts its shadow into it.
        s->lights.push_back(g3d::Light::directional({ -10, 3, 0 }, WHITE, 1.6f, true));
        s->lights.back().shadowExtent = 25;
        auto group = [&](Vector3 at, int* count) {
            g3d::Object3D* g = s->add();
            g->position = at;
            if (cull) { g->cullRadius = 1.8f; g->cullCentre = { 0, 1, 0 }; }
            g3d::Object3D* b = g->add(g3d::Geometry::box(1.4f, 2, 1.4f), m);
            b->position = { 0, 1, 0 };
            b->up = [count](int, g3d::Object3D&) { (*count)++; };
        };
        group({ 0, 0, 0 }, &ups.near);                 // in frame
        group({ -9.5f, 0, 0 }, &ups.shadow);           // out of frame, its shadow in
        group({ 40, 0, -20 }, &ups.far);               // out of frame, its shadow too
        Comp c = sceneComp(s);
        return render(c, 0);
    };
    Ups all, culled;
    Image a = run(false, all), b = run(true, culled);
    CHECK(a.width == b.width && std::memcmp(a.data, b.data, (size_t)a.width * a.height * 4) == 0);
    CHECK(all.near == 1 && all.shadow == 1 && all.far == 1);
    CHECK(culled.near == 1);                           // seen: animated
    CHECK(culled.shadow == 1);                         // unseen, but its shadow is: kept
    CHECK(culled.far == 0);                            // nothing of it reaches the view: skipped
    UnloadImage(a);
    UnloadImage(b);
}

// Instancing: bodies drawn from a baked mesh match the same bodies as ordinary objects, in
// the lit pass and in the shadow maps, with an unseen one still casting its shadow into view.
static void instancing() {
    auto bake = [] {
        auto am = std::make_shared<g3d::AnimatedMesh>();
        am->rest = g3d::Geometry::box(1, 1, 1);
        const Mesh& m = am->rest->mesh();
        am->frames = 2;
        am->vertices = m.vertexCount;
        for (int f = 0; f < 2; f++)
            for (int v = 0; v < m.vertexCount; v++) {
                for (int k = 0; k < 3; k++) {
                    float p = m.vertices[v * 3 + k];
                    if (f == 1 && k == 1) p *= 1.5f;                // frame 1: half again as tall
                    am->positions.push_back(p);
                    am->normals.push_back(m.normals[v * 3 + k]);
                }
            }
        am->centre = { 0, 0, 0 };
        am->radius = 1.2f;                                          // a unit box, grown by half in y
        return am;
    };
    struct Body { Vector3 at; float yaw, scale; };
    const std::vector<Body> bodies = { { { -1.5f, 0.6f, 0 }, 0, 1 }, { { 0, 0.5f, 1 }, 30, 1 }, { { 1.6f, 0.9f, -1 }, 65, 1.4f },
                                       { { 0.4f, 0.5f, -2.5f }, 110, 0.8f }, { { -9.5f, 0.5f, 0 }, 20, 1 } };   // the last: off screen
    auto run = [&](bool instanced, float tall, bool cull) {
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 2.5f, 8 };
        s->camera.target = { 0, 0.5f, 0 };
        s->camera.fov = 40;
        g3d::Material m;
        m.roughness = 0.8f;
        m.color = { 200, 120, 90, 255 };
        s->add(g3d::Geometry::plane(60, 40), m);
        s->lights.push_back(g3d::Light::directional({ -10, 3, 0 }, WHITE, 1.6f, true));
        s->lights.back().shadowExtent = 25;
        if (instanced) {
            auto set = std::make_shared<g3d::Instances>();
            set->mesh = bake();
            for (const Body& b : bodies) {
                g3d::Instance i;
                i.position = b.at;
                i.yaw = b.yaw;
                i.scale = b.scale;
                i.frame = tall;                                     // 0 or 1: one baked frame, no blend
                i.clipLength = 2;
                set->list.push_back(i);
            }
            g3d::Object3D* o = s->add(set, m);
            (void)o;
        } else {
            for (const Body& b : bodies) {
                g3d::Object3D* o = s->add(g3d::Geometry::box(1, 1 + 0.5f * tall, 1), m);
                o->position = b.at;
                o->rotation = { 0, b.yaw, 0 };
                o->scale = { b.scale, b.scale, b.scale };
            }
        }
        (void)cull;
        Comp c = sceneComp(s);
        return render(c, 0);
    };
    for (int tall = 0; tall < 2; tall++) {
        Image a = run(false, (float)tall, false), b = run(true, (float)tall, true);
        long differ = 0;
        for (int i = 0; i < a.width * a.height * 4; i++)
            if (std::abs((int)((unsigned char*)a.data)[i] - (int)((unsigned char*)b.data)[i]) > 6) differ++;
        std::printf("[scene3d] instancing, frame %d: %ld of %d channel values differ by more than 6\n", tall, differ, a.width * a.height * 4);
        CHECK(a.width == b.width && differ < a.width * a.height * 4 / 2000);
        UnloadImage(a);
        UnloadImage(b);
    }
}

// ---- materials for vegetation: cut-out cards, light through leaves, per-instance colour,
// normal and roughness maps.
static std::string writePng(const fs::path& dir, const char* name, int w, int h, Color (*at)(int x, int y)) {
    Image img = GenImageColor(w, h, BLANK);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) ImageDrawPixel(&img, x, y, at(x, y));
    const std::string path = (dir / name).string();
    ExportImage(img, path.c_str());
    UnloadImage(img);
    return path;
}

static void vegetationMaterials(const fs::path& dir) {
    // A card whose left half is clear: with a cut-off the clear half shows what is behind it
    // and casts no shadow; the solid half draws and shadows as before.
    const std::string half = writePng(dir, "half.png", 64, 64, [](int x, int) { return x < 32 ? Color{ 255, 255, 255, 0 } : WHITE; });
    {
        // The picture: an unlit card facing the camera over a red backdrop card.
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 0, 6 };
        s->add(g3d::Geometry::box(8, 8, 0.1f), unlit({ 200, 30, 30, 255 }))->position = { 0, 0, -2 };
        g3d::Material card = unlit({ 40, 220, 40, 255 });
        card.map = half;
        card.alphaCutoff = 0.5f;
        card.doubleSided = true;
        s->add(g3d::Geometry::plane(3, 3), card)->rotation = { 90, 0, 0 };
        Comp c = sceneComp(s);
        Image img = render(c, 0);
        const Color clear = px(img, project(s->camera, { -0.8f, 0, 0 })), solid = px(img, project(s->camera, { 0.8f, 0, 0 }));
        std::printf("[scene3d] cut-out picture: clear half %d,%d,%d, solid half %d,%d,%d\n",
                    clear.r, clear.g, clear.b, solid.r, solid.g, solid.b);
        CHECK(close(clear, { 200, 30, 30, 255 }, 3));                   // the backdrop through the clear half
        CHECK(close(solid, { 40, 220, 40, 255 }, 3));                   // the card, solid
        UnloadImage(img);

        // The shadow: the same card lying flat 2 m up, lit from straight above, seen from
        // below its height so it hides none of the ground.
        auto run = [&](float cutoff) {
            auto sh = std::make_shared<g3d::Scene>();
            sh->camera.position = { 0, 1.2f, 9 };
            sh->camera.target = { 0, 0, 0 };
            sh->camera.fov = 40;
            g3d::Material ground;
            ground.color = { 200, 200, 200, 255 };
            ground.roughness = 1;
            sh->add(g3d::Geometry::plane(40, 40), ground);
            g3d::Material leaf;
            leaf.color = { 90, 200, 90, 255 };
            leaf.map = half;
            leaf.alphaCutoff = cutoff;
            leaf.doubleSided = true;
            sh->add(g3d::Geometry::plane(4, 4), leaf)->position = { 0, 2, 0 };
            sh->lights.push_back(g3d::Light::directional({ 0, 10, 0.01f }, WHITE, 1.5f, true));
            sh->lights.back().shadowExtent = 12;
            Comp cc = sceneComp(sh);
            Image im = render(cc, 0);
            const Color l = px(im, project(sh->camera, { -1.2f, 0, -0.5f })), r = px(im, project(sh->camera, { 1.2f, 0, -0.5f }));
            UnloadImage(im);
            return std::pair<int, int>{ luma(l), luma(r) };
        };
        const auto [cutL, cutR] = run(0.5f);
        const auto [fullL, fullR] = run(0);
        std::printf("[scene3d] cut-out shadow: ground under clear half %d, under solid half %d (no cut-off: %d, %d)\n",
                    cutL, cutR, fullL, fullR);
        CHECK(cutL > cutR + 40);                                          // the clear half casts no shadow
        CHECK(std::abs(cutR - fullR) <= 2);                               // the solid half shadows as before
        CHECK(fullL < cutL - 40);                                         // without the cut-off the whole card shadows
    }

    // Light through a leaf: a card between the camera and the light is dark without
    // translucency and bright with it, brightest looking into the light.
    {
        auto run = [&](float translucency) {
            auto s = std::make_shared<g3d::Scene>();
            s->camera.position = { 0, 0, 5 };
            s->camera.target = { 0, 0, 0 };
            s->camera.fov = 40;
            s->skyColor = BLACK;
            s->groundColor = BLACK;
            g3d::Material leaf;
            leaf.color = { 120, 200, 80, 255 };
            leaf.doubleSided = true;
            leaf.translucency = translucency;
            leaf.translucencyColor = { 255, 240, 160, 255 };
            g3d::Object3D* o = s->add(g3d::Geometry::plane(3, 3), leaf);
            o->rotation = { 90, 0, 0 };                                   // stands up, facing the camera
            s->lights.push_back(g3d::Light::directional({ 0, 0, -10 }, WHITE, 0.4f, false));   // behind the leaf, dim enough not to clip
            Comp c = sceneComp(s);
            Image img = render(c, 0);
            const Color mid = GetImageColor(img, W / 2, H / 2);
            UnloadImage(img);
            return mid;
        };
        const Color opaque = run(0), through = run(1), half = run(0.5f);
        std::printf("[scene3d] translucency: 0 -> %d, 0.5 -> %d, 1 -> %d\n", luma(opaque), luma(half), luma(through));
        CHECK(luma(opaque) < 12);
        CHECK(luma(through) > 90 && luma(half) > luma(opaque) + 30 && luma(through) > luma(half) + 15);
        CHECK(through.g > through.b);                                   // tinted by leaf and translucency colour
    }

    // Instance tint and shade: unlit white boxes come out in each instance's colour.
    {
        auto am = std::make_shared<g3d::AnimatedMesh>();
        am->rest = g3d::Geometry::box(1, 1, 1);
        const Mesh& m = am->rest->mesh();
        am->frames = 1;
        am->vertices = m.vertexCount;
        am->positions.assign(m.vertices, m.vertices + m.vertexCount * 3);
        am->normals.assign(m.normals, m.normals + m.vertexCount * 3);
        am->radius = 1;
        auto set = std::make_shared<g3d::Instances>();
        set->mesh = am;
        const Color tints[3] = { { 255, 0, 0, 255 }, { 255, 255, 255, 255 }, { 40, 120, 200, 255 } };
        const float shades[3] = { 1, 0.5f, 1 };
        for (int k = 0; k < 3; k++) {
            g3d::Instance i;
            i.position = { -1.6f + 1.6f * k, 0, 0 };
            i.tint = tints[k];
            i.shade = shades[k];
            set->list.push_back(i);
        }
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 0, 6 };
        s->add(set, unlit(WHITE));
        Comp c = sceneComp(s);
        Image img = render(c, 0);
        const Color a = px(img, project(s->camera, { -1.6f, 0, 0.5f })), b = px(img, project(s->camera, { 0, 0, 0.5f })),
                    d = px(img, project(s->camera, { 1.6f, 0, 0.5f }));
        std::printf("[scene3d] instance tint: %d,%d,%d  %d,%d,%d  %d,%d,%d\n", a.r, a.g, a.b, b.r, b.g, b.b, d.r, d.g, d.b);
        CHECK(close(a, { 255, 0, 0, 255 }, 2));
        CHECK(close(b, { 128, 128, 128, 255 }, 2));
        CHECK(close(d, { 40, 120, 200, 255 }, 2));
        UnloadImage(img);
    }

    // Normal map: a floor lit at a grazing angle from +x looks brighter where the map leans
    // its normals towards the light, darker where it leans them away, and normalScale 0 is
    // the flat floor exactly.
    {
        const std::string towards = writePng(dir, "n_plus.png", 8, 8, [](int, int) { return Color{ 217, 128, 217, 255 }; });
        const std::string away = writePng(dir, "n_minus.png", 8, 8, [](int, int) { return Color{ 38, 128, 217, 255 }; });
        auto run = [&](const std::string& map, float scale) {
            auto s = std::make_shared<g3d::Scene>();
            s->camera.position = { 0, 6, 0.01f };
            s->camera.target = { 0, 0, 0 };
            s->camera.fov = 40;
            s->skyColor = BLACK;
            s->groundColor = BLACK;
            g3d::Material floor;
            floor.color = { 200, 200, 200, 255 };
            floor.roughness = 1;
            floor.normalMap = map;
            floor.normalScale = scale;
            s->add(g3d::Geometry::plane(6, 6), floor);
            s->lights.push_back(g3d::Light::directional({ 10, 3, 0 }, WHITE, 1.5f, false));
            Comp c = sceneComp(s);
            Image img = render(c, 0);
            const int l = luma(GetImageColor(img, W / 2, H / 2));
            UnloadImage(img);
            return l;
        };
        const int flat = run("", 1), plus = run(towards, 1), minus = run(away, 1), zero = run(towards, 0);
        std::printf("[scene3d] normal map: flat %d, towards %d, away %d, scale 0 %d\n", flat, plus, minus, zero);
        CHECK(plus > flat + 20 && minus < flat - 10);
        CHECK(std::abs(zero - flat) <= 1);
    }

    // Roughness map: green multiplies roughness. A white map changes nothing; a black one
    // makes a rough floor a mirror-sharp one, so the light's highlight shows.
    {
        const std::string white = writePng(dir, "mr_white.png", 4, 4, [](int, int) { return WHITE; });
        const std::string smooth = writePng(dir, "mr_smooth.png", 4, 4, [](int, int) { return Color{ 255, 0, 0, 255 }; });
        auto run = [&](const std::string& map) {
            auto s = std::make_shared<g3d::Scene>();
            s->camera.position = { 0, 3, 5 };
            s->camera.target = { 0, 0, 0 };
            s->camera.fov = 40;
            g3d::Material floor;
            floor.color = { 90, 90, 90, 255 };
            floor.roughness = 1;
            floor.roughnessMap = map;
            s->add(g3d::Geometry::plane(10, 10), floor);
            s->lights.push_back(g3d::Light::directional({ 0, 3, -5 }, WHITE, 1.5f, false));   // mirrors into the camera
            Comp c = sceneComp(s);
            return render(c, 0);
        };
        Image none = run(""), whiteMap = run(white), smoothMap = run(smooth);
        int maxNone = 0, maxSmooth = 0;
        long differ = 0;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                const Color a = GetImageColor(none, x, y), b = GetImageColor(whiteMap, x, y);
                if (!close(a, b, 1)) differ++;
                maxNone = std::max(maxNone, luma(a));
                maxSmooth = std::max(maxSmooth, luma(GetImageColor(smoothMap, x, y)));
            }
        std::printf("[scene3d] roughness map: white map differs at %ld px; brightest rough %d, smooth %d\n", differ, maxNone, maxSmooth);
        CHECK(differ == 0);
        CHECK(maxSmooth > maxNone + 60);
        UnloadImage(none);
        UnloadImage(whiteMap);
        UnloadImage(smoothMap);
    }
}

// ---- water: the CPU's surface, the surface seen from above and below, the water's colour on
// what is under it, and caustics that move the light about without changing its sum.
static g3d::ScenePtr waterScene(bool water, float caustics, Vector3 eye, Vector3 target) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = eye;
    s->camera.target = target;
    s->camera.fov = 55;
    s->sky = g3d::Sky::day();
    s->sky.sunLight = 0;
    s->lights.push_back(g3d::Light::directional({ 3, 9, 2 }, WHITE, 1.5f, false));
    g3d::Material sand;
    sand.color = { 214, 196, 150, 255 };
    sand.roughness = 0.9f;
    sand.receiveShadow = false;
    s->add(g3d::Geometry::plane(60, 60), sand)->position = { 0, -3, 0 };
    g3d::Material rock;
    rock.color = { 130, 128, 124, 255 };
    s->add(g3d::Geometry::sphere(0.8f), rock)->position = { 1.5f, -2.4f, -1 };
    s->add(g3d::Geometry::box(0.6f, 3, 0.6f), rock)->position = { -2, -1.5f, -2 };
    if (water) {
        s->water = g3d::Water::calm();
        s->water.caustics = caustics;
    }
    return s;
}
// Like sceneComp, but long enough to render later frames.
static Comp longComp(g3d::ScenePtr scene, int w = W, int h = H) {
    Comp c;
    c.w = w; c.h = h; c.duration = 20; c.background = BLACK;
    addRoot(c.roots, 0, 600)->fn = g3d::draw(scene, {});
    return c;
}
static void dumpWater(const char* name, const Image& img) {
    if (const char* dir = std::getenv("RMTN_WATER_DUMP")) {
        fs::create_directories(dir);
        ExportImage(img, (fs::path(dir) / name).string().c_str());
    }
}
static void waterTest() {
    // The CPU surface: slope is the derivative of height, the level is the mean, time moves it.
    {
        g3d::Water w = g3d::Water::choppy();
        w.level = 2;
        double meanH = 0, maxErr = 0;
        int n = 0;
        for (float x = -20; x < 20; x += 1.37f)
            for (float z = -20; z < 20; z += 1.91f) {
                const float e = 0.002f;
                const Vector2 g = w.slope(x, z, 3.0);
                const float dx = (w.height(x + e, z, 3.0) - w.height(x - e, z, 3.0)) / (2 * e);
                const float dz = (w.height(x, z + e, 3.0) - w.height(x, z - e, 3.0)) / (2 * e);
                maxErr = std::max({ maxErr, (double)std::fabs(dx - g.x), (double)std::fabs(dz - g.y) });
                meanH += w.height(x, z, 3.0);
                n++;
            }
        CHECK(maxErr < 0.01);
        CHECK(std::fabs(meanH / n - 2.0) < 0.35);
        CHECK(std::fabs(w.height(3, 4, 0.0) - w.height(3, 4, 1.0)) > 1e-3f);
        CHECK(w.height(5, 5, 2.5) == w.height(5, 5, 2.5));
        CHECK(w.submerged({ 0, w.level - 5, 0 }, 1.0) && !w.submerged({ 0, w.level + 5, 0 }, 1.0));
        g3d::Water off;
        CHECK(!off.submerged({ 0, -100, 0 }, 0.0));
    }
    const Vector3 above{ 0, 2.5f, 7 }, aim{ 0, -1.5f, 0 };
    // Seen from above, the surface reflects the sky over the floor it hides: the picture changes.
    {
        Comp dry = longComp(waterScene(false, 0, above, aim)), wet = longComp(waterScene(true, 1, above, aim));
        Image a = render(dry, 5), b = render(wet, 5);
        long diff = 0;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) diff += std::abs(luma(GetImageColor(a, x, y)) - luma(GetImageColor(b, x, y)));
        CHECK(diff > (long)W * H * 3);
        CHECK(allOpaque(b));
        dumpWater("above_dry.png", a); dumpWater("above_wet.png", b);
        Image b2 = render(wet, 5);
        CHECK(std::memcmp(b.data, b2.data, (size_t)W * H * 4) == 0);   // the same frame twice is the same picture
        UnloadImage(a); UnloadImage(b); UnloadImage(b2);
    }
    // Under the surface: the sand is bluer and darker than dry, and looking up shows the window.
    const Vector3 diver{ 0, -1.2f, 6 }, ahead{ 0, -2.2f, 0 };
    {
        Comp dry = longComp(waterScene(false, 0, diver, ahead)), wet = longComp(waterScene(true, 0, diver, ahead));
        Image a = render(dry, 5), b = render(wet, 5);
        const Color ca = GetImageColor(a, W / 2, H - 20), cb = GetImageColor(b, W / 2, H - 20);   // sand near the diver
        CHECK(ca.r > ca.b);                                  // dry sand is warm
        CHECK(cb.b >= cb.r);                                 // wet: red is gone first
        CHECK(luma(cb) < luma(ca));
        dumpWater("under.png", b);
        UnloadImage(a); UnloadImage(b);
        Comp up = longComp(waterScene(true, 1, diver, { 0, 6, 0 }));
        Image c = render(up, 5);
        const Color top = GetImageColor(c, W / 2, 10);
        CHECK(top.r + top.g + top.b > 60);                   // not black: the sky through the window
        UnloadImage(c);
    }
    // Caustics: the sunlit floor gets light and dark patches with the same overall brightness.
    {
        const Vector3 look{ 0, -1.0f, 4 }, floorAt{ 0, -3, 0 };
        Comp flat = longComp(waterScene(true, 0, look, floorAt)), caus = longComp(waterScene(true, 1, look, floorAt));
        Image a = render(flat, 5), b = render(caus, 5), b2 = render(caus, 40);
        auto stats = [](const Image& img, double& mean, double& sd) {
            double sum = 0, sum2 = 0;
            int n = 0;
            for (int y = H * 3 / 4; y < H - 4; y++)          // the near floor, clear of the rocks
                for (int x = W / 4; x < W * 3 / 4; x++) {
                    const double l = luma(GetImageColor(img, x, y));
                    sum += l; sum2 += l * l; n++;
                }
            mean = sum / n;
            sd = std::sqrt(std::max(0.0, sum2 / n - mean * mean));
        };
        double m0, s0, m1, s1;
        stats(a, m0, s0); stats(b, m1, s1);
        CHECK(s1 > s0 * 1.5 + 1);                            // a pattern where there was none
        CHECK(m1 > m0 * 0.6 && m1 < m0 * 1.6);               // the light is moved, not made
        long moved = 0;
        for (int y = H * 3 / 4; y < H - 4; y++)
            for (int x = W / 4; x < W * 3 / 4; x++)
                moved += std::abs(luma(GetImageColor(b, x, y)) - luma(GetImageColor(b2, x, y)));
        CHECK(moved > 200);                                  // it moves with time
        dumpWater("caustics.png", b);
        UnloadImage(a); UnloadImage(b); UnloadImage(b2);
    }
    if (std::getenv("RMTN_WATER_DUMP")) {          // pictures to look at, not checks
        for (int k = 0; k < 4; k++) {
            auto sea = std::make_shared<g3d::Scene>();
            sea->sky = k < 3 ? g3d::Sky::at(k == 2 ? 17.6f : 10.5f) : g3d::Sky::day();
            sea->sky.sunLight = 0;
            sea->lights.push_back(g3d::Light::directional({ -3, k == 2 ? 2.2f : 7, -9 }, { 255, 244, 224, 255 }, 2.2f, false));
            sea->water = k == 0 ? g3d::Water::choppy() : g3d::Water::storm();
            if (k == 3) sea->water = g3d::Water::calm();
            sea->water.openDepth = k == 3 ? 0 : 40;
            sea->camera.position = k == 3 ? g3d::Camera{}.position : Vector3{ 0, k == 1 ? 7.f : 3.f, 12 };
            sea->camera.target = k == 3 ? Vector3{ 0, 0, 0 } : Vector3{ 0, 1.2f, -20 };
            sea->camera.fov = 60;
            if (k == 3) {                              // from below: looking up at the window
                sea->camera.position = { 0, -3, 4 };
                sea->camera.target = { 0, 3, -2 };
            }
            sea->camera.farPlane = 4000;
            Comp c = longComp(sea, 960, 540);
            Image img = render(c, 60 + k * 7);
            dumpWater(("sea" + std::to_string(k) + ".png").c_str(), img);
            UnloadImage(img);
        }
    }
    {
        Comp wet = longComp(waterScene(true, 1, above, aim));
        Image b = render(wet, 40);
        dumpWater("surface.png", b);
        UnloadImage(b);
    }
}

// 3D lines: the same width in px near and far, hidden behind a box (not when onTop), trimmed
// by from/to, arrowheads wider than the stroke, dashes measured on screen, and a line that
// runs past the camera clipped rather than blown up.
static void lines3d() {
    // the run of lit pixels down column x, through y0
    auto thickness = [](const Image& img, int x, int y0) {
        int n = 0;
        for (int y = std::max(0, y0 - 30); y < std::min(H, y0 + 30); y++) n += GetImageColor(img, x, y).g > 128;
        return n;
    };
    for (float dist : { 4.f, 12.f }) {
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 0, dist };
        s->camera.target = { 0, 0, 0 };
        auto l = std::make_shared<g3d::Lines>();
        l->line({ -10, 0, 0 }, { 10, 0, 0 }, GREEN, 6);
        s->add(l);
        Comp c = sceneComp(s, BLACK);
        Image img = render(c, 0);
        const int t = thickness(img, W / 2, H / 2);
        CHECK(t >= 5 && t <= 7);
        if (t < 5 || t > 7) std::fprintf(stderr, "  line at %.0f m: %d px, want 6\n", dist, t);
        UnloadImage(img);
    }
    {   // behind a box, then on top of it; trimmed; with a head
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 0, 6 };
        s->add(g3d::Geometry::box(1, 1, 1), unlit(RED))->position = { 0, 0, 1 };
        auto l = std::make_shared<g3d::Lines>();
        l->line({ -3, 0, 0 }, { 3, 0, 0 }, GREEN, 6);
        s->add(l);
        Comp c = sceneComp(s, BLACK);
        Image img = render(c, 0);
        const Vector2 mid = project(s->camera, { 0, 0, 1.5f }), side = project(s->camera, { -2, 0, 0 });
        CHECK(close(px(img, mid), RED, 6));                  // the box hides it
        CHECK(close(px(img, side), GREEN, 6));
        UnloadImage(img);
        l->onTop = true;
        img = render(c, 0);
        CHECK(close(px(img, mid), GREEN, 6));                // over everything
        UnloadImage(img);
        l->onTop = false;
        l->paths[0].from = 0.5f;                              // only the right half
        img = render(c, 0);
        CHECK(close(px(img, side), BLACK, 6));
        CHECK(close(px(img, project(s->camera, { 2, 0, 0 })), GREEN, 6));
        UnloadImage(img);
        l->paths[0].from = 0;
        l->paths[0].head = g3d::Lines::Head::End;
        l->paths[0].headSize = 4;
        img = render(c, 0);
        const Vector2 tip = project(s->camera, { 3, 0, 0 });
        const int nearTip = thickness(img, (int)tip.x - 18, (int)tip.y), body = thickness(img, (int)side.x, (int)side.y);
        CHECK(nearTip > body + 4);                            // the head is wider than the stroke
        CHECK(close(px(img, { tip.x + 4, tip.y }), BLACK, 6)); // and stops at the tip
        UnloadImage(img);
    }
    {   // dashes: 10 px on, 10 px off along a 200 px line gives about 10 dashes
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 0, 5 };
        auto l = std::make_shared<g3d::Lines>();
        const Vector3 a{ -2, 0, 0 }, b{ 2, 0, 0 };
        g3d::Lines::Path& p = l->line(a, b, GREEN, 3);
        p.dash = 10;
        p.gap = 10;
        s->add(l);
        Comp c = sceneComp(s, BLACK);
        Image img = render(c, 0);
        const Vector2 pa = project(s->camera, a), pb = project(s->camera, b);
        int runs = 0;
        bool on = false;
        for (int x = (int)pa.x - 3; x <= (int)pb.x + 3; x++) {
            const bool lit = GetImageColor(img, x, (int)pa.y).g > 128;
            if (lit && !on) runs++;
            on = lit;
        }
        const int want = (int)std::round((pb.x - pa.x) / 20);
        CHECK(std::abs(runs - want) <= 1);
        if (std::abs(runs - want) > 1) std::fprintf(stderr, "  dashes: %d, want %d\n", runs, want);
        UnloadImage(img);
    }
    {   // a line running from in front of the camera to behind it: clipped, still drawn, sane width
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 0, 5 };
        auto l = std::make_shared<g3d::Lines>();
        l->line({ 0, -0.5f, 0 }, { 0, -0.5f, 20 }, GREEN, 4);
        s->add(l);
        Comp c = sceneComp(s, BLACK);
        Image img = render(c, 0);
        int lit = 0;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) lit += GetImageColor(img, x, y).g > 128;
        CHECK(lit > 50 && lit < W * H / 3);
        UnloadImage(img);
    }
    if (const char* dir = std::getenv("RMTN_LINES_DUMP")) {   // a picture to look at, not a check
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 3.2f, 2.4f, 4.2f };
        s->camera.target = { 0, 0.4f, 0 };
        s->lights.push_back(g3d::Light::directional({ -2, 5, 3 }, WHITE, 1.4f, true));
        g3d::Material grey;
        grey.color = { 70, 76, 86, 255 };
        s->add(g3d::Geometry::box(6, 0.1f, 6), grey)->position = { 0, -0.05f, 0 };
        g3d::Material cube;
        cube.color = { 230, 120, 60, 255 };
        s->add(g3d::Geometry::box(0.8f, 0.8f, 0.8f), cube)->position = { 0.6f, 0.4f, 0.2f };
        auto l = std::make_shared<g3d::Lines>();
        l->box({ -1.2f, 0, -1.2f }, { -0.2f, 1, -0.2f }, { 120, 200, 255, 255 }, 2);
        l->arrow({ 0, 0.01f, 0 }, { 1.6f, 0.01f, 0 }, { 240, 80, 80, 255 }, 4);
        l->arrow({ 0, 0.01f, 0 }, { 0, 1.6f, 0 }, { 90, 220, 120, 255 }, 4);
        l->arrow({ 0, 0.01f, 0 }, { 0, 0.01f, 1.6f }, { 90, 140, 255, 255 }, 4);
        g3d::Lines::Path& orbit = l->circle({ 0.6f, 0.4f, 0.2f }, { 0, 1, 0 }, 0.9f, { 240, 240, 240, 255 }, 2);
        orbit.dash = 10;
        orbit.gap = 7;
        l->arrow({ -1.6f, 1.5f, 1.2f }, { 0.6f, 0.4f, -0.3f }, { 255, 210, 80, 255 }, 3);   // a leader that runs behind the cube
        s->add(l);
        Comp c = longComp(s, 960, 540);
        Image img = render(c, 0);
        fs::create_directories(dir);
        ExportImage(img, (fs::path(dir) / "lines.png").string().c_str());
        UnloadImage(img);
    }
}

// Device prefabs: the screen keeps its picture's proportions, the cut from a phone screen
// filling the frame to the same drawing in 2D is invisible (rounded corners included), and a
// closed laptop lid hides its screen.
static void devices() {
    auto app = [](const Ctx& c, int) {
        dr::rect(c, 0, 0, 390, 844, { 238, 241, 245, 255 });
        dr::rect(c, 0, 0, 390, 250, { 43, 108, 246, 255 });
        dr::roundRect(c, 20, 300, 350, 200, 18, { 240, 164, 60, 255 });
        dr::circle(c, { 195, 650 }, 80, { 20, 32, 43, 255 });
    };
    {
        auto s = std::make_shared<g3d::Scene>();
        s->lights.push_back(g3d::Light::directional({ 1, 2, 3 }, WHITE, 1, false));
        auto ui = std::make_shared<g3d::Surface>();
        ui->pixels = { 390, 844 };
        ui->draw = app;
        g3d::Device phone = g3d::addDevice(*s, g3d::DeviceKind::Phone, ui);
        phone.root->position = { 0.1f, 0.2f, -0.3f };
        phone.root->rotation = { -20, 15, 0 };
        CHECK(std::fabs(ui->size.x - 0.0715f) < 1e-5f && std::fabs(ui->size.y / ui->size.x - 844.f / 390) < 1e-4f);
        s->camera.fov = 40;
        s->camera.nearPlane = 0.02f;
        // At 960x540: in the 240x160 test frame the 844 px screen is drawn at 0.19x, where its
        // one-px edges are a large share of the picture.
        static constexpr int FW = 960, FH = 540;
        s->camera = g3d::fillCamera(s->camera, *s, *phone.screen, 0, (float)FW / FH);
        Comp c = longComp(s, FW, FH);
        c.background = BLACK;
        Image img = render(c, 0);
        Comp flat;
        flat.w = FW; flat.h = FH; flat.duration = 10; flat.background = BLACK;
        addRoot(flat.roots, 0, 10)->fn = [ui](const Ctx& cx, int t) { ui->draw(g3d::fillCtx(*ui, FW, FH), t); };
        Image ref = render(flat, 0);
        // inside the screen's rectangle, less a 2 px border (outside it the 3D shows the phone's body)
        const Ctx f = g3d::fillCtx(*ui, FW, FH);
        long long sum = 0;
        int n = 0;
        for (int y = 2; y < FH - 2; y++)
            for (int x = (int)f.o.x + 2; x < (int)(f.o.x + 390 * f.s) - 2; x++) {
                const Color a = GetImageColor(img, x, y), b = GetImageColor(ref, x, y);
                sum += std::max({ std::abs(a.r - b.r), std::abs(a.g - b.g), std::abs(a.b - b.b) });
                n++;
            }
        const double mean = (double)sum / std::max(1, n);
        CHECK(mean < 1.0);
        if (mean >= 1.0) std::fprintf(stderr, "  phone fill vs direct: mean %.2f\n", mean);
        if (const char* dir = std::getenv("RMTN_DEVICES_DUMP")) {
            fs::create_directories(dir);
            ExportImage(img, (fs::path(dir) / "phone3d.png").string().c_str());
            ExportImage(ref, (fs::path(dir) / "phone2d.png").string().c_str());
        }
        // the top-left corner is the bezel's colour in both, not the app's blue
        const Vector2 corner{ f.o.x + 3 * f.s, 3 * f.s };
        CHECK(GetImageColor(ref, (int)corner.x, (int)corner.y).b < 40);
        CHECK(GetImageColor(img, (int)corner.x, (int)corner.y).b < 40);
        UnloadImage(img);
        UnloadImage(ref);
    }
    {   // a laptop seen from above and in front: open, the screen shows; closed, it does not
        auto s = std::make_shared<g3d::Scene>();
        s->lights.push_back(g3d::Light::directional({ 1, 3, 2 }, WHITE, 1, false));
        auto ui = std::make_shared<g3d::Surface>();
        ui->pixels = { 1440, 900 };
        ui->background = { 0, 255, 0, 255 };
        g3d::Device lap = g3d::addDevice(*s, g3d::DeviceKind::Laptop, ui);
        CHECK(lap.lid != nullptr && std::fabs(ui->size.y / ui->size.x - 900.f / 1440) < 1e-4f);
        s->camera.position = { 0, 0.45f, 0.55f };
        s->camera.target = { 0, 0.08f, -0.05f };
        s->camera.nearPlane = 0.02f;
        Comp c = sceneComp(s, BLACK);
        auto green = [](const Image& img) {
            int k = 0;
            for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { const Color p = GetImageColor(img, x, y); k += p.g > 200 && p.r < 60; }
            return k;
        };
        Image open = render(c, 0);
        lap.lid->rotation.x = 90;                             // closed
        Image shut = render(c, 0);
        CHECK(green(open) > W * H / 20);
        CHECK(green(shut) == 0);
        UnloadImage(open);
        UnloadImage(shut);
    }
    if (const char* dir = std::getenv("RMTN_DEVICES_DUMP")) {   // a picture to look at, not a check
        auto s = std::make_shared<g3d::Scene>();
        s->ambient = 0.55f;
        s->lights.push_back(g3d::Light::directional({ -1, 2.5f, 2 }, { 255, 240, 225, 255 }, 1.6f, true));
        s->lights.back().shadowExtent = 1.2f;
        s->add(g3d::Geometry::box(4, 0.02f, 3), [] { g3d::Material m; m.color = { 70, 56, 46, 255 }; m.roughness = 0.8f; return m; }())->position = { 0, -0.01f, 0 };
        auto page = [](Vector2 px, Color top) {
            auto u = std::make_shared<g3d::Surface>();
            u->pixels = px;
            u->draw = [px, top](const Ctx& c, int) {
                dr::rect(c, 0, 0, px.x, px.y, { 245, 246, 248, 255 });
                dr::rect(c, 0, 0, px.x, px.y * 0.22f, top);
                for (int i = 0; i < 4; i++) dr::roundRect(c, px.x * 0.06f, px.y * (0.3f + i * 0.16f), px.x * 0.88f, px.y * 0.12f, px.y * 0.02f, { 222, 226, 233, 255 });
            };
            return u;
        };
        g3d::DeviceStyle silver;
        silver.body = { 196, 199, 204, 255 };
        g3d::Device mon = g3d::addDevice(*s, g3d::DeviceKind::Monitor, page({ 1920, 1080 }, { 43, 108, 246, 255 }), {});
        mon.root->position = { -0.05f, 0, -0.45f };
        g3d::Device lap = g3d::addDevice(*s, g3d::DeviceKind::Laptop, page({ 1440, 900 }, { 47, 196, 178, 255 }), silver);
        lap.root->position = { 0.42f, 0, -0.05f };
        lap.root->rotation = { 0, -25, 0 };
        g3d::Device tab = g3d::addDevice(*s, g3d::DeviceKind::Tablet, page({ 834, 1194 }, { 240, 164, 60, 255 }), silver);
        tab.root->position = { -0.42f, 0.0035f, 0.05f };
        tab.root->rotation = { -90, 0, 20 };
        g3d::Device ph = g3d::addDevice(*s, g3d::DeviceKind::Phone, page({ 390, 844 }, { 200, 70, 90, 255 }));
        ph.root->position = { -0.12f, 0.004f, 0.18f };
        ph.root->rotation = { -90, 0, -10 };
        g3d::Device web = g3d::addDevice(*s, g3d::DeviceKind::Browser, page({ 1280, 800 }, { 20, 32, 43, 255 }));
        web.root->position = { 0.62f, 0.42f, -0.55f };
        web.root->rotation = { 0, -22, 0 };
        web.root->scale = { 0.5f, 0.5f, 0.5f };
        s->camera.position = { 0.15f, 0.65f, 1.15f };
        s->camera.target = { 0, 0.18f, -0.1f };
        s->camera.fov = 40;
        s->camera.nearPlane = 0.02f;
        Comp c = longComp(s, 1280, 720);
        Image img = render(c, 0);
        fs::create_directories(dir);
        ExportImage(img, (fs::path(dir) / "devices.png").string().c_str());
        UnloadImage(img);
    }
}

// 2D surfaces: picture the right way up, the fill camera matching a direct 2D draw, texture
// density following the camera, constant-size billboards hidden by what is in front,
// see-through backgrounds and the surface's own clock.
static void surfaces() {
    auto picture = [](const Ctx& c, int) {
        dr::rect(c, 0, 0, 120, 40, RED);
        dr::rect(c, 0, 40, 60, 40, GREEN);
        dr::rect(c, 60, 40, 60, 40, BLUE);
        dr::circle(c, { 60, 40 }, 17, WHITE);
    };
    {
        auto s = std::make_shared<g3d::Scene>();
        auto ui = std::make_shared<g3d::Surface>();
        ui->pixels = { 120, 80 };
        ui->size = { 1.5f, 1.0f };
        ui->draw = picture;
        g3d::Object3D* holder = s->add();                                  // on a moving parent
        holder->up = [](int t, g3d::Object3D& h) { h.rotation = { 0, 20.f + t, 0 }; h.position = { 0.2f, 0, 0 }; };
        g3d::Object3D* o = holder->add(ui);
        o->position = { 0.3f, 0.2f, -1 };
        o->rotation = { 10, 5, 0 };
        s->camera = g3d::fillCamera(s->camera, *s, *o, 0, (float)W / H);
        Comp c = sceneComp(s, BLACK);
        Image img = render(c, 0);
        CHECK(close(px(img, { W * 0.25f, H * 0.2f }), RED, 6));          // top is up
        CHECK(close(px(img, { W * 0.15f, H * 0.8f }), GREEN, 6));        // left is left
        CHECK(close(px(img, { W * 0.85f, H * 0.8f }), BLUE, 6));
        CHECK(std::fabs(ui->resolution - 2) < 0.01f);                     // 160 px tall for 80 design px
        {
            g3d::Camera far = s->camera;
            far.position = Vector3Add(far.position, { 3, 2, 5 });
            const g3d::Camera at1 = g3d::mix(far, s->camera, 1), half = g3d::mix(far, s->camera, 0.5f);
            CHECK(Vector3Distance(at1.position, s->camera.position) < 1e-5f && Vector3Distance(at1.target, s->camera.target) < 1e-5f);
            CHECK(Vector3Distance(half.position, Vector3Lerp(far.position, s->camera.position, 0.5f)) < 1e-5f);
        }
        // The same picture drawn straight into the frame, as the cut after a push-in would.
        Comp flat;
        flat.w = W; flat.h = H; flat.duration = 10; flat.background = BLACK;
        addRoot(flat.roots, 0, 10)->fn = [ui, picture](const Ctx& c, int t) { picture(g3d::fillCtx(*ui, W, H), t); };
        Image ref = render(flat, 0);
        long long sum = 0;
        int worst = 0;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                const Color a = GetImageColor(img, x, y), b = GetImageColor(ref, x, y);
                const int d = std::max({ std::abs(a.r - b.r), std::abs(a.g - b.g), std::abs(a.b - b.b) });
                sum += d;
                worst = std::max(worst, d);
            }
        const double mean = (double)sum / (W * H);
        CHECK(mean < 1.0);
        if (mean >= 1.0) std::fprintf(stderr, "  fill vs direct: mean %.2f, worst %d\n", mean, worst);
        UnloadImage(ref);
        UnloadImage(img);
        // Further away it is drawn smaller.
        s->camera.position = Vector3Add(s->camera.target, Vector3Scale(Vector3Subtract(s->camera.position, s->camera.target), 4));
        img = render(c, 0);
        CHECK(ui->resolution > 0.49f && ui->resolution < 0.6f);
        UnloadImage(img);
    }
    {   // A label: square to the camera, the same size on screen near and far, hidden behind a box.
        for (float dist : { 4.f, 12.f }) {
            auto s = std::make_shared<g3d::Scene>();
            s->camera.position = { 0, 0, dist };
            auto label = std::make_shared<g3d::Surface>();
            label->pixels = { 40, 20 };
            label->size = { 1, 0.5f };
            label->background = WHITE;
            label->billboard = true;
            label->pixelSize = 1;
            s->add(label)->position = { -1, 0.5f, 0 };
            s->add(g3d::Geometry::box(0.6f, 0.6f, 0.6f), unlit(RED))->position = { 1, -1, 1 };
            auto hidden = std::make_shared<g3d::Surface>();
            hidden->pixels = { 40, 20 };
            hidden->size = { 1, 0.5f };
            hidden->background = WHITE;
            hidden->billboard = true;
            hidden->pixelSize = 1;
            s->add(hidden)->position = Vector3Add(s->camera.position, Vector3Scale(Vector3Subtract({ 1, -1, 1 }, s->camera.position), 1.6f));   // right behind the box
            Comp c = sceneComp(s, BLACK);
            Image img = render(c, 0);
            const Vector2 at = project(s->camera, { -1, 0.5f, 0 });
            int wide = 0;
            for (int x = 0; x < W; x++) wide += GetImageColor(img, x, (int)at.y).r > 200;
            CHECK(std::abs(wide - 40) <= 2);
            if (std::abs(wide - 40) > 2) std::fprintf(stderr, "  label at %.0f m: %d px wide\n", dist, wide);
            CHECK(close(px(img, project(s->camera, { 1, -1, 1 })), RED));    // the box hides the other label
            UnloadImage(img);
        }
    }
    {   // See-through background, and the surface's own clock.
        auto s = std::make_shared<g3d::Scene>();
        s->camera.position = { 0, 0, 4 };
        s->add(g3d::Geometry::box(3, 3, 0.1f), unlit(RED))->position = { 0, 0, -1 };
        auto ui = std::make_shared<g3d::Surface>();
        ui->pixels = { 100, 100 };
        ui->size = { 2, 2 };
        ui->start = 5;
        ui->draw = [](const Ctx& c, int t) { dr::rect(c, 30, 30, 40, 40, t == 0 ? GREEN : BLUE); };
        s->add(ui);
        Comp c = sceneComp(s, BLACK);
        Image img = render(c, 5);
        CHECK(close(px(img, { W / 2.f, H / 2.f }), GREEN, 6));               // local frame 0 at scene frame 5
        CHECK(close(px(img, project(s->camera, { -0.8f, 0.8f, 0 })), RED, 6)); // the empty corner shows what is behind
        UnloadImage(img);
        img = render(c, 6);
        CHECK(close(px(img, { W / 2.f, H / 2.f }), BLUE, 6));
        UnloadImage(img);
    }
}

// ---- grass fields
static void dumpField(const char* name, const Image& img) {
    if (const char* dir = std::getenv("RMTN_FIELD_DUMP")) {
        fs::create_directories(dir);
        ExportImage(img, (fs::path(dir) / name).string().c_str());
    }
}
// A meadow on magenta ground (so blades are easy to count), lit from behind by a low sun.
static g3d::ScenePtr meadow(std::function<void(g3d::Field&)> edit = {}, bool withField = true) {
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 1.4f, 6 };
    s->camera.target = { 0, 0.4f, 0 };
    s->ambient = 0.35f;
    g3d::Material ground;
    ground.color = { 200, 40, 200, 255 };
    ground.roughness = 1;
    s->add(g3d::Geometry::plane(80, 80), ground);
    auto f = std::make_shared<g3d::Field>();
    f->min = { -20, -30 }; f->max = { 20, 8 };
    f->density = 150;
    if (edit) edit(*f);
    g3d::Material grass;
    grass.roughness = 0.8f;
    grass.translucency = 0.8f;
    grass.translucencyColor = { 255, 236, 160, 255 };
    grass.lineWidth = 0;
    if (withField) s->add(f, grass);
    g3d::Light sun = g3d::Light::directional({ 3, 2.5f, -10 }, { 255, 226, 180, 255 }, 2.2f, true);
    sun.shadowExtent = 14;
    s->lights.push_back(sun);
    return s;
}
static void grassField() {
    {   // placement: a tile holds density * size^2 blades; any first n of them spread evenly
        g3d::Field f;
        f.density = 400; f.tileSize = 4; f.min = { 0, 0 }; f.max = { 10, 6 };
        CHECK(f.bladesPerTile() == 6400 && f.tilesX() == 3 && f.tilesZ() == 2);
        int cells[16] = {};
        bool inside = true;
        for (uint32_t k = 0; k < 160; k++) {
            const Vector2 p = f.bladeRoot(1, 1, k);
            inside = inside && p.x >= 4 && p.x < 8 && p.y >= 4 && p.y < 8;
            cells[std::clamp((int)(p.x - 4), 0, 3) + 4 * std::clamp((int)(p.y - 4), 0, 3)]++;
        }
        CHECK(inside);
        CHECK(*std::min_element(cells, cells + 16) >= 6 && *std::max_element(cells, cells + 16) <= 14);   // 10 each, evenly
        const Vector2 a = f.bladeRoot(0, 0, 5), b = f.bladeRoot(1, 0, 5);
        CHECK(std::fabs(a.x + 4 - b.x) > 1e-3f || std::fabs(a.y - b.y) > 1e-3f);                 // tiles differ
        g3d::Field g = f;
        g.seed = 2;
        CHECK(Vector2Distance(g.bladeRoot(1, 1, 7), f.bladeRoot(1, 1, 7)) > 1e-3f);             // seeds differ
    }
    // Blade pixels: grass is yellow-green, the meadow's ground magenta (blue above green).
    auto bladePixels = [](const Image& img, int x0, int x1, int y0, int y1) {
        int n = 0;
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++) {
                const Color c = GetImageColor(img, x, y);
                n += c.g > c.b + 8;
            }
        return n;
    };
    auto same = [](const Image& a, const Image& b) {
        for (int y = 0; y < a.height; y++)
            for (int x = 0; x < a.width; x++)
                if (!close(GetImageColor(a, x, y), GetImageColor(b, x, y), 0)) return false;
        return true;
    };
    {   // a look at it (dumped when RMTN_FIELD_DUMP is set); deterministic, and frames are pure
        Comp c = longComp(meadow(), 960, 540);
        Image img = render(c, 40);
        dumpField("meadow.png", img);
        if (std::getenv("RMTN_FIELD_DUMP")) {
            auto s = meadow([](g3d::Field& f) { f.density = 900; });
            s->objects[0]->material.color = { 52, 64, 30, 255 };
            s->camera.position = { 0, 0.35f, 2 };
            s->camera.target = { 0.3f, 0.2f, 0 };
            Comp close = longComp(s, 960, 540);
            Image ci = render(close, 40);
            dumpField("closeup.png", ci);
            UnloadImage(ci);
        }
        Comp again = longComp(meadow());
        Image before = render(again, 39), after = render(again, 40);
        Comp fresh = longComp(meadow());
        Image alone = render(fresh, 40);
        CHECK(same(after, alone));
        CHECK(!same(before, after));                                    // the wind moves it
        UnloadImage(before); UnloadImage(after); UnloadImage(alone);
        UnloadImage(img);
    }
    {   // the mask: no blades where it is 0
        Comp c = longComp(meadow([](g3d::Field& f) { f.mask = [](float x, float) { return x < 0 ? 0.f : 1.f; }; }));
        Image img = render(c, 10);
        const g3d::Camera cam = meadow()->camera;
        const int mid = (int)project(cam, { 0, 0, 2 }).x;
        const int y0 = (int)project(cam, { 0, 0, -2 }).y, y1 = (int)project(cam, { 0, 0, 4 }).y;
        const int left = bladePixels(img, 0, mid - 12, y0, y1), right = bladePixels(img, mid + 12, W, y0, y1);
        std::printf("[scene3d] grass mask: %d blade pixels where 0, %d where 1\n", left, right);
        CHECK(left == 0 && right > 400);
        UnloadImage(img);
    }
    {   // beyond fadeEnd nothing is drawn or submitted
        Comp c = longComp(meadow([](g3d::Field& f) { f.min = { -20, -60 }; f.max = { 20, -30 }; f.fadeStart = 10; f.fadeEnd = 25; }));
        Image img = render(c, 10);
        CHECK(bladePixels(img, 0, W, 0, H) == 0 && g3d::fieldBladesSubmitted() == 0);
        UnloadImage(img);
        Comp near = longComp(meadow([](g3d::Field& f) { f.fadeStart = 10; f.fadeEnd = 25; }));
        Image seen = render(near, 10);
        CHECK(bladePixels(seen, 0, W, 0, H) > 1000 && g3d::fieldBladesSubmitted() > 0);
        UnloadImage(seen);
    }
    {   // sway: tips move between frames, roots stay; seen from the side, against the sky
        auto side = [](std::function<void(g3d::Field&)> edit) {
            auto s = meadow([&](g3d::Field& f) {
                f.min = { -3, -0.3f }; f.max = { 3, 0.3f }; f.density = 200; f.height = 0.5f; f.heightVariation = 0;
                f.lean = 0; f.tileSize = 1;
                edit(f);
            });
            s->objects[0]->visible = false;                                      // no ground: blades against black
            s->camera.position = { 0, 0.25f, 5 };
            s->camera.target = { 0, 0.25f, 0 };
            return s;
        };
        auto s = side([](g3d::Field& f) { f.sway = 0.3f; f.gust = 0.3f; });
        Comp c = longComp(s);
        Image a = render(c, 0), b = render(c, 8);
        const int rootY = (int)project(s->camera, { 0, 0.03f, 0 }).y, tipY = (int)project(s->camera, { 0, 0.4f, 0 }).y;
        int rootDiff = 0, tipDiff = 0;
        for (int x = 0; x < W; x++)
            for (int d = -2; d <= 2; d++) {
                rootDiff += std::abs(luma(GetImageColor(a, x, rootY + d)) - luma(GetImageColor(b, x, rootY + d)));
                tipDiff += std::abs(luma(GetImageColor(a, x, tipY + d)) - luma(GetImageColor(b, x, tipY + d)));
            }
        std::printf("[scene3d] grass sway: root band changes %d, tip band %d\n", rootDiff, tipDiff);
        CHECK(tipDiff > 4 * rootDiff + 500);
        UnloadImage(a); UnloadImage(b);

        // A gust reaches a patch 4 m downwind later: with only the gust bending the blades,
        // the frame each patch leans furthest comes about 4 m / gustSpeed after the other's.
        auto g = side([](g3d::Field& f) { f.sway = 0; f.gust = 0.6f; f.gustSpeed = 4; f.gustScale = 10; f.windDirection = { 1, 0 }; });
        g->camera.position = { 0, 0.25f, 7 };
        Comp gc = longComp(g);
        const int period = 75;                                                    // 10 m / 4 m/s at 30 fps
        const int band = (int)project(g->camera, { 0, 0.3f, 0 }).y;
        const int leftX = (int)project(g->camera, { -2, 0.3f, 0 }).x, rightX = (int)project(g->camera, { 2, 0.3f, 0 }).x;
        std::vector<double> lean[2];
        for (int f = 0; f < period; f++) {
            Image img = render(gc, f);
            for (int p = 0; p < 2; p++) {                                         // mean x of blade pixels in the patch
                const int cx = p ? rightX : leftX;
                double sx = 0, n = 0;
                for (int y = band - 2; y <= band + 2; y++)
                    for (int x = cx - 30; x < cx + 30; x++)
                        if (luma(GetImageColor(img, x, y)) > 20) { sx += x - cx; n++; }
                lean[p].push_back(n > 0 ? sx / n : 0);
            }
            UnloadImage(img);
        }
        for (auto& series : lean) {                                               // around its mean
            double m = 0;
            for (double v : series) m += v / period;
            for (double& v : series) v -= m;
        }
        int lag = 0;
        double bestCorr = -1e300;
        for (int d = 0; d < period; d++) {                                        // downwind(t + d) ~ upwind(t)
            double sum = 0;
            for (int f = 0; f < period; f++) sum += lean[0][(size_t)f] * lean[1][(size_t)((f + d) % period)];
            if (sum > bestCorr) { bestCorr = sum; lag = d; }
        }
        std::printf("[scene3d] grass gust: downwind patch follows the upwind one by %d frames (expect 30)\n", lag);
        CHECK(lag >= 24 && lag <= 36);
    }
    {   // a tile out of view still shadows the ground in view
        auto build = [](bool grass) {
            auto s = meadow([](g3d::Field& f) { f.min = { -4, -2 }; f.max = { -2, 2 }; f.tileSize = 1; f.height = 1.6f;
                                                f.density = 1500; f.heightVariation = 0; }, grass);
            s->camera.position = { 3, 3, 4 };
            s->camera.target = { 3, 0, 0 };
            s->lights[0] = g3d::Light::directional({ -14, 5, 0 }, WHITE, 2, true);
            s->lights[0].target = { 0, 0, 0 };
            s->lights[0].shadowExtent = 8;
            return s;
        };
        auto with = build(true), without = build(false);
        CHECK(project(with->camera, { -2, 1.6f, 0 }).x < 0);                     // the grass is off screen
        Comp a = longComp(with), b = longComp(without);
        Image ia = render(a, 0), ib = render(b, 0);
        const Vector2 probe = project(with->camera, { 1, 0, 0 });
        const int shaded = luma(px(ia, probe)), open = luma(px(ib, probe));
        std::printf("[scene3d] grass shadow from off screen: %d vs %d unshadowed\n", shaded, open);
        CHECK(probe.x > 0 && shaded < open - 15);
        UnloadImage(ia); UnloadImage(ib);
    }
    {   // an area: grass stands on its terrain and keeps off its road
        flora::Area land = flora::rect({ -12, -12 }, { 12, 12 });
        land.ground = [](float x, float z) { return 3 + 0.0f * x * z; };   // a plateau 3 m up
        land.strips.push_back({ { { -12, -2 }, { 12, 2 } }, 3 });
        auto build = [&](bool fromAbove) {
            auto s = std::make_shared<g3d::Scene>();
            g3d::Material ground;
            ground.color = { 200, 40, 200, 255 };
            ground.roughness = 1;
            s->add(flora::terrain(land, 24, 24), ground);
            g3d::Field look = flora::meadow();
            look.density = 300;
            flora::addGrass(*s, land, look, 4);
            s->lights.push_back(g3d::Light::directional({ 3, 10, 4 }, WHITE, 1.5f, false));
            if (fromAbove) { s->camera.position = { 0, 14, 0.01f }; s->camera.target = { 0, 3, 0 }; }
            else { s->camera.position = { 0, 3.6f, 18 }; s->camera.target = { 0, 3, 0 }; }
            return s;
        };
        auto above = build(true);
        Comp a = longComp(above);
        Image top = render(a, 5);
        dumpField("area_top.png", top);
        const Vector2 r0 = project(above->camera, { -4, 3, -0.5f }), r1 = project(above->camera, { 4, 3, 0.5f });
        const Vector2 g0 = project(above->camera, { -4, 3, 4 }), g1 = project(above->camera, { 4, 3, 6 });
        const int onRoad = bladePixels(top, (int)std::min(r0.x, r1.x), (int)std::max(r0.x, r1.x), (int)std::min(r0.y, r1.y), (int)std::max(r0.y, r1.y));
        const int offRoad = bladePixels(top, (int)std::min(g0.x, g1.x), (int)std::max(g0.x, g1.x), (int)std::min(g0.y, g1.y), (int)std::max(g0.y, g1.y));
        std::printf("[scene3d] grass area: %d blade pixels on the road, %d beside it\n", onRoad, offRoad);
        CHECK(onRoad == 0 && offRoad > 50);
        UnloadImage(top);
        auto side = build(false);
        Comp b = longComp(side);
        Image img = render(b, 5);
        dumpField("area_side.png", img);
        int lowest = -1;                                                      // the lowest row with a blade in it
        for (int y = 0; y < H; y++)
            if (bladePixels(img, 0, W, y, y + 1) > 0) lowest = y;
        const int plateau = (int)project(side->camera, { 0, 3, 12 }).y, below = (int)project(side->camera, { 0, 2.7f, 12 }).y;
        std::printf("[scene3d] grass area: lowest blade row %d, the plateau's near edge at %d, 0.3 m below it at %d\n", lowest, plateau, below);
        CHECK(lowest >= plateau - 2 && lowest < below);
        UnloadImage(img);
    }
    {   // landscape: a 400 m square; time printed, not checked. Full size (600 blades a square
        // metre at 1080p) with RMTN_FIELD_BENCH set; smaller by default, for software GL on CI.
        const bool bench = std::getenv("RMTN_FIELD_BENCH") != nullptr;
        auto s = meadow([bench](g3d::Field& f) { f.min = { -200, -200 }; f.max = { 200, 200 }; f.density = bench ? 600.f : 120.f; });
        s->camera.position = { 0, 1.7f, 6 };
        s->camera.target = { 0, 1.2f, -10 };
        s->objects[0]->material.color = { 70, 92, 40, 255 };
        Comp c = longComp(s, bench ? 1920 : 480, bench ? 1080 : 270);
        Image first = render(c, 30);
        dumpField("landscape.png", first);
        UnloadImage(first);
        const int frames = bench ? 5 : 2;
        const auto t0 = std::chrono::steady_clock::now();
        for (int f = 31; f < 31 + frames; f++) UnloadImage(render(c, f));
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / frames;
        std::printf("[scene3d] grass landscape %dx%d: %lld blades submitted, %.1f ms a frame (with readback)\n",
                    c.w, c.h, g3d::fieldBladesSubmitted(), ms);
    }
}

// ---- trees (flora::makeTree, addForest): levels by distance, impostors, wind
static g3d::ScenePtr treeStage(Color ground = { 200, 40, 200, 255 }) {
    auto s = std::make_shared<g3d::Scene>();
    s->ambient = 0.4f;
    g3d::Material g;
    g.color = ground;
    g.roughness = 1;
    s->add(g3d::Geometry::plane(400, 400), g);
    g3d::Light sun = g3d::Light::directional({ 20, 30, 14 }, { 255, 240, 220, 255 }, 2.2f, true);
    sun.shadowExtent = 40;
    s->lights.push_back(sun);
    return s;
}
static void trees() {
    const char* names[] = { "oak", "birch", "pine", "spruce", "palm", "tropical", "shrub", "hedge" };
    std::vector<flora::TreePtr> built;
    for (const char* n : names) {
        flora::TreeSpec spec = flora::tree(n);
        const auto t0 = std::chrono::steady_clock::now();
        built.push_back(flora::makeTree(spec));
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        std::printf("[scene3d] tree %-8s %6lld / %5lld triangles (hero / reduced), %5d leaf and %5d wood vertices, %.0f ms to build\n", n,
                    built.back()->heroTriangles, built.back()->reducedTriangles, built.back()->hero.leaves->vertices,
                    built.back()->hero.wood->vertices, ms);
    }
    // Tree pixels: neither the magenta ground nor the black sky.
    auto isTree = [](Color c) { return !(c.r > 1.3f * c.g && c.b > 1.3f * c.g) && c.r + c.g + c.b > 12; };
    auto coverage = [&](const Image& img, int* sumX = nullptr) {
        int n = 0;
        long long sx = 0;
        for (int y = 0; y < img.height; y++)
            for (int x = 0; x < img.width; x++)
                if (isTree(GetImageColor(img, x, y))) { n++; sx += x; }
        if (sumX) *sumX = n ? (int)(sx / n) : -1;
        return n;
    };
    auto noShadows = [](g3d::ScenePtr s) { s->lights[0].castShadow = false; return s; };
    {   // Instance::fade: f and -(1 - f) cover each pixel exactly once, f alone about f of them
        auto boxes = [&](float a, float b) {
            auto s = noShadows(treeStage());
            s->camera.position = { 0, 1, 6 };
            s->camera.target = { 0, 1, 0 };
            auto in = std::make_shared<g3d::Instances>();
            g3d::Material m;
            m.color = { 30, 200, 60, 255 };
            in->mesh = flora::makeTree(flora::shrub(), { 1, 4, 1, 16 })->views[0];   // a quad
            auto add = [&](float f, Color tint) {
                g3d::Instance i;
                i.scale = 0.6f; i.fade = f; i.tint = tint;
                in->list.push_back(i);
            };
            if (a != 0) add(a, WHITE);
            if (b != 0) add(b, WHITE);
            m.alphaCutoff = 0;
            m.dynamicMap.reset();
            s->add(in, m);
            Comp c = longComp(s, 320, 180);
            return render(c, 3);
        };
        Image whole = boxes(1, 0), part = boxes(0.25f, 0), both = boxes(0.25f, -0.75f);
        const int all = coverage(whole), quarter = coverage(part), joined = coverage(both);
        std::printf("[scene3d] screen door: %d pixels whole, %d at fade 0.25, %d at 0.25 and -0.75\n", all, quarter, joined);
        CHECK(all > 2000);
        CHECK(std::abs(quarter - all / 4) < all / 40);
        CHECK(std::abs(joined - all) <= all / 200);
        UnloadImage(whole); UnloadImage(part); UnloadImage(both);
    }
    {   // levels by distance: walking away changes the tree's coverage smoothly (no popping)
        // through hero -> reduced -> impostor; with a 0.05 m blend the same walk pops.
        auto walk = [&](float blend, int& worst, int& at) {
            worst = 0; at = 0;
            int before = -1;
            for (int k = 0; k <= 60; k++) {
                const float d = 18 + 0.5f * k;                      // across 25 and 40 m
                auto s = noShadows(treeStage());
                s->camera.position = { 0, 6, d };
                s->camera.target = { 0, 6, 0 };
                s->camera.fov = 40.f * 30 / d;                     // the tree keeps its size on screen
                flora::ForestSpec fs;
                fs.reducedFrom = 25; fs.impostorFrom = 40; fs.blend = blend; fs.wind = 0; fs.gust = 0;
                flora::addTree(*s, built[0], { 0, 0, 0 }, 0, 1, fs);
                Comp c = longComp(s, 240, 240);
                Image img = render(c, 0);
                const int n = coverage(img);
                UnloadImage(img);
                if (before >= 0 && std::abs(n - before) > worst) { worst = std::abs(n - before); at = k; }
                before = n;
            }
        };
        int smoothWorst, smoothAt, popWorst, popAt;
        walk(6, smoothWorst, smoothAt);
        walk(0.05f, popWorst, popAt);
        std::printf("[scene3d] tree levels: largest step in coverage %d px blended (at %.1f m), %d px with no blend (at %.1f m)\n",
                    smoothWorst, 18 + 0.5f * smoothAt, popWorst, 18 + 0.5f * popAt);
        CHECK(smoothWorst * 2 < popWorst);
    }
    {   // the impostor is the tree: its silhouette overlaps the full tree's from any side
        for (float yaw : { 0.f, 70.f, 200.f }) {
            auto shot = [&](bool far) {
                auto s = noShadows(treeStage());
                s->camera.position = { 0, 6, 30 };
                s->camera.target = { 0, 6, 0 };
                flora::ForestSpec fs;
                fs.wind = 0; fs.gust = 0;
                if (far) { fs.reducedFrom = 1; fs.impostorFrom = 2; fs.blend = 0.1f; }
                flora::addTree(*s, built[0], { 0, 0, 0 }, yaw, 1, fs);
                Comp c = longComp(s, 240, 240);
                return render(c, 0);
            };
            Image a = shot(false), b = shot(true);
            int both = 0, either = 0;
            for (int y = 0; y < a.height; y++)
                for (int x = 0; x < a.width; x++) {
                    const bool p = isTree(GetImageColor(a, x, y)), q = isTree(GetImageColor(b, x, y));
                    both += p && q; either += p || q;
                }
            const float iou = either ? (float)both / either : 0;
            std::printf("[scene3d] oak impostor at yaw %.0f: silhouette overlap %.2f\n", yaw, iou);
            CHECK(iou > 0.6f);
            UnloadImage(a); UnloadImage(b);
        }
    }
    // Silhouette overlap (intersection over union of tree pixels) of a tree forced to its
    // impostor against the same tree at full detail, from `elev` degrees above the horizon.
    auto overlapFrom = [&](const flora::TreePtr& tree, float yaw, float elev) {
        auto shot = [&](bool far) {
            auto s = noShadows(treeStage());
            const float e = elev * DEG2RAD;
            s->camera.position = { 0, 6 + 30 * std::sin(e), 30 * std::cos(e) };
            s->camera.target = { 0, 6, 0 };
            flora::ForestSpec fs;
            fs.wind = 0; fs.gust = 0;
            if (far) { fs.reducedFrom = 1; fs.impostorFrom = 2; fs.blend = 0.1f; }
            else { fs.reducedFrom = 200; fs.impostorFrom = 300; }
            flora::addTree(*s, tree, { 0, 0, 0 }, yaw, 1, fs);
            Comp c = longComp(s, 240, 240);
            return render(c, 0);
        };
        Image a = shot(false), b = shot(true);
        int both = 0, either = 0;
        for (int y = 0; y < a.height; y++)
            for (int x = 0; x < a.width; x++) {
                const Color ca = GetImageColor(a, x, y), cb = GetImageColor(b, x, y);
                const bool p = isTree(ca), q = isTree(cb);
                both += p && q; either += p || q;
            }
        UnloadImage(a); UnloadImage(b);
        return either ? (float)both / either : 0.f;
    };
    {   // from above: the impostor still stands for the tree (mean over three turns of the tree)
        const float elevs[] = { 0, 30, 45, 60, 80 };
        for (int k : { 0, 1, 2 }) {                    // oak, birch, pine
            std::printf("[scene3d] %-5s impostor overlap by elevation:", names[k]);
            float worst60 = 1;
            for (float elev : elevs) {
                float sum = 0, low = 1;
                for (float yaw : { 0.f, 70.f, 200.f }) {
                    const float iou = overlapFrom(built[(size_t)k], yaw, elev);
                    sum += iou; low = std::min(low, iou);
                }
                std::printf("  %.0f deg: mean %.2f (min %.2f)", elev, sum / 3, low);
                if (elev == 0) CHECK(sum / 3 > (k == 0 ? 0.84f : k == 1 ? 0.86f : 0.61f));   // the side view: at least what it was
                if (elev == 60) worst60 = low;
            }
            std::printf("\n");
            CHECK(worst60 >= 0.75f);
            // The same tree with one row of pictures (what impostors were before): 60 degrees, and the side.
            flora::TreeBuild one;
            one.elevations = 1;
            const flora::TreePtr flat = flora::makeTree(flora::tree(names[k]), one);
            float flat0 = 0, flat60 = 0;
            for (float yaw : { 0.f, 70.f, 200.f }) { flat0 += overlapFrom(flat, yaw, 0) / 3; flat60 += overlapFrom(flat, yaw, 60) / 3; }
            std::printf("[scene3d] %-5s with elevations = 1: overlap %.2f from the side, %.2f from 60 deg\n", names[k], flat0, flat60);
            CHECK(flat60 + 0.05f < worst60);
            // Atlas memory: colour and normal pictures, 4 bytes a texel, views x elevations cells of viewSize^2.
            const auto atlasBytes = [](const flora::Tree& t) {
                return (long long)t.build.views * t.build.elevations * t.build.viewSize * t.build.viewSize * 4 * 2;
            };
            std::printf("[scene3d] %-5s impostor atlas: %lld KiB with one row, %lld KiB with %d\n", names[k], atlasBytes(*flat) / 1024,
                        atlasBytes(*built[(size_t)k]) / 1024, built[(size_t)k]->build.elevations);
        }
    }
    {   // brightness: under the default sun (shadows on) the crown pixels both a full tree and its
        // impostor cover have about the same mean linear luminance, and about the same colour.
        auto linear = [](unsigned char c) { return std::pow(c / 255.0, 2.2); };
        for (int k : { 0, 1, 2 }) {
            std::printf("[scene3d] %-5s impostor / full crown luminance:", names[k]);
            for (float elev : { 0.f, 30.f, 60.f, 80.f }) {
                double ratio = 0;
                double rgbA[3] = { 0, 0, 0 }, rgbB[3] = { 0, 0, 0 };
                int n = 0;
                for (float yaw : { 0.f, 70.f, 200.f }) {
                    auto shot = [&](bool far) {
                        auto s = treeStage();                                // the default sun, with shadows
                        const float e = elev * DEG2RAD;
                        s->camera.position = { 0, 6 + 30 * std::sin(e), 30 * std::cos(e) };
                        s->camera.target = { 0, 6, 0 };
                        flora::ForestSpec fs;
                        fs.wind = 0; fs.gust = 0;
                        if (far) { fs.reducedFrom = 1; fs.impostorFrom = 2; fs.blend = 0.1f; }
                        else { fs.reducedFrom = 200; fs.impostorFrom = 300; }
                        flora::addTree(*s, built[(size_t)k], { 0, 0, 0 }, yaw, 1, fs);
                        Comp c = longComp(s, 240, 240);
                        return render(c, 0);
                    };
                    Image a = shot(false), b = shot(true);
                    double ya = 0, yb = 0;
                    for (int y = 0; y < a.height; y++)
                        for (int x = 0; x < a.width; x++) {
                            const Color ca = GetImageColor(a, x, y), cb = GetImageColor(b, x, y);
                            if (!isTree(ca) || !isTree(cb)) continue;
                            ya += 0.2126 * linear(ca.r) + 0.7152 * linear(ca.g) + 0.0722 * linear(ca.b);
                            yb += 0.2126 * linear(cb.r) + 0.7152 * linear(cb.g) + 0.0722 * linear(cb.b);
                            rgbA[0] += ca.r; rgbA[1] += ca.g; rgbA[2] += ca.b; rgbB[0] += cb.r; rgbB[1] += cb.g; rgbB[2] += cb.b;
                            n++;
                        }
                    ratio += (ya > 0 ? yb / ya : 0) / 3;
                    UnloadImage(a); UnloadImage(b);
                }
                std::printf("  %.0f deg %.2f (rgb full %.0f %.0f %.0f, impostor %.0f %.0f %.0f)", elev, ratio, rgbA[0] / std::max(n, 1), rgbA[1] / std::max(n, 1),
                            rgbA[2] / std::max(n, 1), rgbB[0] / std::max(n, 1), rgbB[1] / std::max(n, 1), rgbB[2] / std::max(n, 1));
                CHECK(ratio > 0.75 && ratio < 1.3);
            }
            std::printf("\n");
        }
    }
    {   // impostor shadows: a tree at impostor distance casts about the shadow of the full tree,
        // in the same place, from a low and a higher sun. Seen from straight above (orthographic),
        // the shadowed ground is what is darker than the same ground with no tree, outside both
        // trees' own pixels.
        struct Shadow { long long px = 0; double cx = 0, cz = 0; int x0 = 1 << 30, x1 = -1, y0 = 1 << 30, y1 = -1; };
        const int N = 600;
        const float span = 60;                                       // metres across the picture
        auto ground = [&](const flora::TreePtr& tree, float sunElevation, bool far, bool withTree) {
            auto s = treeStage();
            const float az = 35 * DEG2RAD, el = sunElevation * DEG2RAD;
            s->lights[0].position = { 60 * std::cos(az) * std::cos(el), 60 * std::sin(el), 60 * std::sin(az) * std::cos(el) };
            s->lights[0].shadowExtent = 40;
            s->camera.position = { -9, 60, -5.99f };
            s->camera.target = { -9, 0, -6 };
            s->camera.orthographic = true;
            s->camera.orthoHeight = span;
            flora::ForestSpec fs;
            fs.wind = 0; fs.gust = 0;
            if (far) { fs.reducedFrom = 1; fs.impostorFrom = 2; fs.blend = 0.1f; }
            else { fs.reducedFrom = 500; fs.impostorFrom = 600; }
            if (withTree) flora::addTree(*s, tree, { 0, 0, 0 }, 20, 1, fs);
            Comp c = longComp(s, N, N);
            return render(c, 0);
        };
        auto lum = [](Color c) { return c.r + c.g + c.b; };
        auto shadowOf = [&](const Image& bare, const Image& a, const Image& b, bool useB) {
            Shadow out;
            for (int y = 0; y < N; y++)
                for (int x = 0; x < N; x++) {
                    const Color base = GetImageColor(bare, x, y), ca = GetImageColor(a, x, y), cb = GetImageColor(b, x, y);
                    if (isTree(ca) || isTree(cb)) continue;                       // under either tree's own pixels
                    if (lum(useB ? cb : ca) < 0.6f * lum(base)) {
                        out.px++; out.cx += x; out.cz += y;
                        out.x0 = std::min(out.x0, x); out.x1 = std::max(out.x1, x); out.y0 = std::min(out.y0, y); out.y1 = std::max(out.y1, y);
                    }
                }
            if (out.px) { out.cx /= out.px; out.cz /= out.px; }
            return out;
        };
        const float px2 = (span / N) * (span / N);                     // m^2 a pixel
        for (int k : { 0, 1, 2 }) {                                     // oak, birch, pine
            for (float sun : { 25.f, 55.f }) {
                Image bare = ground(built[(size_t)k], sun, false, false);
                Image full = ground(built[(size_t)k], sun, false, true), imp = ground(built[(size_t)k], sun, true, true);
                const Shadow f = shadowOf(bare, full, imp, false), i = shadowOf(bare, full, imp, true);
                const double diag = std::hypot(f.x1 - f.x0, f.y1 - f.y0);
                const double off = std::hypot(f.cx - i.cx, f.cz - i.cz);
                std::printf("[scene3d] %-5s shadow, sun %2.0f deg: full %.1f m2, impostor %.1f m2 (ratio %.2f); centroid %.2f m apart (%.1f%% of the shadow's %.1f m)\n",
                            names[k], sun, f.px * px2, i.px * px2, f.px ? (double)i.px / f.px : 0., off * span / N, diag ? 100 * off / diag : 0., diag * span / N);
                if (k == 0) {
                    CHECK(f.px * px2 > 20);
                    CHECK(std::fabs((double)i.px / std::max(1LL, f.px) - 1) < 0.15);
                    CHECK(off < 0.06 * diag);
                } else CHECK(std::fabs((double)i.px / std::max(1LL, f.px) - 1) < 0.30);
                UnloadImage(bare); UnloadImage(full); UnloadImage(imp);
            }
        }
    }
    {   // impostor forest frames are pure (shadows included): a frame drawn alone is the frame after another
        auto s = noShadows(treeStage());
        s->lights[0].castShadow = true;
        s->camera.position = { 0, 25, 60 };
        s->camera.target = { 0, 4, 0 };
        flora::ForestSpec fs;
        fs.reducedFrom = 1; fs.impostorFrom = 2; fs.blend = 0.1f; fs.wind = 3; fs.gust = 0.5f;
        std::vector<flora::Placement> at;
        for (int i = 0; i < 12; i++) at.push_back({ { -18.f + 7 * (i % 6), 0, -6.f + 12 * (i / 6) }, 30.f * i, 1, i % 3, (uint32_t)i + 1 });
        flora::addForest(*s, at, { built[0], built[1], built[2] }, fs);
        Comp a = longComp(s, 320, 240), b = longComp(s, 320, 240);
        Image alone = render(a, 12);
        UnloadImage(render(b, 11));
        Image after = render(b, 12);
        int differ = 0, tree = 0;
        for (int y = 0; y < alone.height; y++)
            for (int x = 0; x < alone.width; x++) {
                const Color p = GetImageColor(alone, x, y), q = GetImageColor(after, x, y);
                differ += p.r != q.r || p.g != q.g || p.b != q.b;
                tree += isTree(p);
            }
        std::printf("[scene3d] impostor forest: %d tree px, %d differ between a frame alone and after another\n", tree, differ);
        CHECK(tree > 500);
        CHECK(differ == 0);
        UnloadImage(alone); UnloadImage(after);
    }
    {   // wind: the crown leans downwind, frames are pure, and the wind moves it
        auto windy = [&](Vector2 dir) {
            auto s = noShadows(treeStage());
            s->camera.position = { 0, 6, 30 };
            s->camera.target = { 0, 6, 0 };
            flora::ForestSpec fs;
            fs.windDirection = dir; fs.wind = 3; fs.gust = 0;
            flora::addTree(*s, built[1], { 0, 0, 0 }, 37, 1, fs);
            return longComp(s, 240, 240);
        };
        Comp east = windy({ 1, 0 }), west = windy({ -1, 0 });
        Image e = render(east, 12), w = render(west, 12);
        int ex = 0, wx = 0;
        coverage(e, &ex); coverage(w, &wx);
        std::printf("[scene3d] birch in wind: crown centre x %d blown east, %d blown west\n", ex, wx);
        CHECK(ex > wx + 2);
        Comp again = windy({ 1, 0 });
        Image prev = render(again, 11), next = render(again, 12);
        bool same = true, moved = false;
        for (int y = 0; y < e.height; y++)
            for (int x = 0; x < e.width; x++) {
                const Color p = GetImageColor(e, x, y), q = GetImageColor(next, x, y), o = GetImageColor(prev, x, y);
                same = same && p.r == q.r && p.g == q.g && p.b == q.b;
                moved = moved || p.r != o.r || p.g != o.g || p.b != o.b;
            }
        CHECK(same);
        CHECK(moved);
        UnloadImage(e); UnloadImage(w); UnloadImage(prev); UnloadImage(next);
    }
    {   // growth: u = 0 draws nothing, u = 1 is the still tree, coverage rises with u, frames pure
        auto grown = [&](float u, bool growing) {
            auto s = noShadows(treeStage());
            s->camera.position = { 0, 6, 30 };
            s->camera.target = { 0, 6, 0 };
            flora::ForestSpec still;
            still.wind = 0;
            still.reducedFrom = 200; still.impostorFrom = 300;
            if (growing) flora::addGrowingTree(*s, built[0], { 0, 0, 0 }, 20, 1, [u](int) { return u; }, still);
            else flora::addTree(*s, built[0], { 0, 0, 0 }, 20, 1, still);
            Comp c = longComp(s, 240, 240);
            return render(c, 5);
        };
        Image none = grown(0, true), whole = grown(1, true), reference = grown(1, false);
        dumpField("grown-1.png", whole);
        dumpField("still-tree.png", reference);
        int differ = 0;
        for (int y = 0; y < whole.height; y++)
            for (int x = 0; x < whole.width; x++) {
                const Color a = GetImageColor(whole, x, y), b = GetImageColor(reference, x, y);
                differ += std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b) > 6;
            }
        std::vector<int> cover;
        for (float u : { 0.1f, 0.3f, 0.5f, 0.7f, 0.85f, 1.f }) {
            Image img = grown(u, true);
            cover.push_back(coverage(img));
            UnloadImage(img);
        }
        std::printf("[scene3d] oak growing: %d px at u 0; coverage %d %d %d %d %d %d at u 0.1..1; %d px differ from the still tree\n",
                    coverage(none), cover[0], cover[1], cover[2], cover[3], cover[4], cover[5], differ);
        CHECK(coverage(none) == 0);
        bool rising = true;
        for (size_t i = 1; i < cover.size(); i++) rising = rising && cover[i] >= cover[i - 1];
        CHECK(rising && cover[0] > 0);
        CHECK(differ < whole.width * whole.height / 200);
        UnloadImage(none); UnloadImage(whole); UnloadImage(reference);

        auto s = noShadows(treeStage());
        s->camera.position = { 0, 6, 30 };
        s->camera.target = { 0, 6, 0 };
        flora::addGrowingTree(*s, built[1], { 0, 0, 0 }, 0, 1, [](int t) { return t / 20.f; });
        Comp c = longComp(s, 240, 240);
        Image prev = render(c, 11), next = render(c, 12);
        Comp fresh = longComp(s, 240, 240);
        Image alone = render(fresh, 12);
        bool same = true;
        for (int y = 0; y < next.height && same; y++)
            for (int x = 0; x < next.width && same; x++) {
                const Color p = GetImageColor(next, x, y), q = GetImageColor(alone, x, y);
                same = p.r == q.r && p.g == q.g && p.b == q.b;
            }
        CHECK(same);
        UnloadImage(prev); UnloadImage(next); UnloadImage(alone);
    }
    {   // styles: a paper card turns to face any camera; low-poly toon trees draw; a sky after a forest
        flora::TreeBuild paperBuild{ 4, 4, 4, 64 }, lowBuild = paperBuild;
        paperBuild.style = flora::TreeStyle::PaperCut;
        lowBuild.style = flora::TreeStyle::LowPoly;
        lowBuild.shading = g3d::Shading::Toon;
        const flora::TreePtr paper = flora::makeTree(flora::oak(), paperBuild), low = flora::makeTree(flora::oak(), lowBuild);
        auto seen = [&](const flora::TreePtr& t, float azimuth, bool growing) {
            auto s = noShadows(treeStage());
            s->camera.position = { 30 * std::sin(azimuth * DEG2RAD), 7, 30 * std::cos(azimuth * DEG2RAD) };
            s->camera.target = { 0, 7, 0 };
            flora::ForestSpec still;
            still.wind = 0;
            if (growing) flora::addGrowingTree(*s, t, { 0, 0, 0 }, 0, 1, [](int) { return 1.f; }, still);
            else flora::addTree(*s, t, { 0, 0, 0 }, 0, 1, still);
            Comp c = longComp(s, 200, 200);
            Image img = render(c, 3);
            const int n = coverage(img);
            UnloadImage(img);
            return n;
        };
        const int front = seen(paper, 0, false), side = seen(paper, 90, false), back = seen(paper, 200, false),
                  growSide = seen(paper, 90, true), lowFront = seen(low, 0, false), lowSide = seen(low, 90, false);
        std::printf("[scene3d] paper oak: %d / %d / %d px from 0 / 90 / 200 degrees, %d growing from 90; low-poly toon oak %d / %d px\n",
                    front, side, back, growSide, lowFront, lowSide);
        CHECK(front > 2000 && std::abs(side - front) < front / 10 && std::abs(back - front) < front / 10);
        CHECK(std::abs(growSide - front) < front / 10);
        CHECK(lowFront > 2000 && lowSide > 2000);
        // Splat crowns: drawn whole when grown and still; while growing, they come in after the wood.
        flora::TreeBuild splatBuild = paperBuild;
        splatBuild.style = flora::TreeStyle::Splat;
        const flora::TreePtr soft = flora::makeTree(flora::oak(), splatBuild);
        auto grownSoft = [&](float u) {
            auto s = noShadows(treeStage());
            s->camera.position = { 0, 7, 30 };
            s->camera.target = { 0, 7, 0 };
            flora::ForestSpec still;
            still.wind = 0;
            flora::addGrowingTree(*s, soft, { 0, 0, 0 }, 0, 1, [u](int) { return u; }, still);
            Comp c = longComp(s, 200, 200);
            Image img = render(c, 3);
            const int n = coverage(img);
            UnloadImage(img);
            return n;
        };
        const int softStill = seen(soft, 0, false), softHalf = grownSoft(0.5f), softFull = grownSoft(1);
        std::printf("[scene3d] splat oak: %d px still, %d px growing at u 0.5, %d at u 1\n", softStill, softHalf, softFull);
        CHECK(softStill > 2000 && std::abs(softFull - softStill) < softStill / 10 && softHalf < softFull / 2);

        // A forest leaves the shared program instanced; the next scene's sky must still draw.
        auto sky = [&](bool forestFirst) {
            if (forestFirst) {
                auto s = noShadows(treeStage());
                flora::addTree(*s, low, { 0, 0, 0 }, 0, 1);
                Comp c = longComp(s, 64, 64);
                UnloadImage(render(c, 1));
            }
            auto s = std::make_shared<g3d::Scene>();
            s->camera.position = { 0, 2, 10 };
            s->camera.target = { 0, 4, 0 };
            s->sky.enabled = true;
            s->sky.show = true;
            Comp c = longComp(s, 64, 64);
            Image img = render(c, 1);
            const Color top = GetImageColor(img, 32, 4);
            UnloadImage(img);
            return top;
        };
        const Color clean = sky(false), after = sky(true);
        std::printf("[scene3d] sky alone (%d %d %d), after a forest (%d %d %d)\n", clean.r, clean.g, clean.b, after.r, after.g, after.b);
        CHECK(clean.r + clean.g + clean.b > 150 && std::abs(after.r - clean.r) + std::abs(after.g - clean.g) + std::abs(after.b - clean.b) < 6);
    }
    {   // city plants: trees from SolitaryVegetationObjects (height, species, points), cover as grass or shrubs
        const city::Model town = city::parse(R"({"type":"CityJSON","version":"2.0","transform":{"scale":[1,1,1],"translate":[0,0,0]},
          "CityObjects":{
            "house":{"type":"Building","geometry":[{"type":"MultiSurface","lod":"1","boundaries":[[[0,1,2,3]]]}]},
            "pine":{"type":"SolitaryVegetationObject","attributes":{"species":"Pinus sylvestris","height":"18 m"},
                    "geometry":[{"type":"MultiPoint","lod":"1","boundaries":[4]}]},
            "row":{"type":"SolitaryVegetationObject","geometry":[{"type":"MultiPoint","lod":"1","boundaries":[5,6,7]}]},
            "lawn":{"type":"PlantCover","attributes":{"averageHeight":0.3},
                    "geometry":[{"type":"MultiSurface","lod":"1","boundaries":[[[8,9,10,11]]]}]},
            "bed":{"type":"PlantCover","attributes":{"averageHeight":2},
                   "geometry":[{"type":"MultiSurface","lod":"1","boundaries":[[[12,13,14,15]]]}]}},
          "vertices":[[30,30,0],[34,30,0],[34,34,0],[30,34,0],
                      [-12,0,0],[4,-2,0],[10,-2,0],[16,-2,0],
                      [-20,-14,0],[-4,-14,0],[-4,-6,0],[-20,-6,0],
                      [8,-16,0],[18,-16,0],[18,-10,0],[8,-10,0]]})");
        int plants = 0;
        for (const city::Object& o : town.objects) plants += city::isPlant(o);
        CHECK(plants == 4);
        auto s = noShadows(treeStage());
        s->camera.position = { 0, 9, 46 };
        s->camera.target = { 0, 7, 0 };
        city::Plants p;
        p.forest.wind = 0;
        p.build = { 4, 4, 4, 32 };
        g3d::Object3D* green = city::addPlants(*s, town, p);
        int fields = 0, forests = 0;
        for (const auto& ch : green->children) { fields += ch->field != nullptr; forests += !ch->children.empty(); }
        Comp c = longComp(s, 320, 200);
        Image img = render(c, 3);
        dumpField("city-plants.png", img);
        // The pine (x -12, 18 m) stands taller than the 10 m street trees (x 4..16).
        auto topOf = [&](int x0, int x1) {
            for (int y = 0; y < img.height; y++)
                for (int x = x0; x < x1; x++) if (isTree(GetImageColor(img, x, y))) return y;
            return img.height;
        };
        const int pineTop = topOf(0, 90), rowTop = topOf(110, 250);           // the left third; the middle
        std::printf("[scene3d] city plants: %d grass field(s), %d forest(s); pine top at y %d, street trees at y %d; %d tree px\n",
                    fields, forests, pineTop, rowTop, coverage(img));
        CHECK(fields == 1 && forests == 2);
        CHECK(pineTop < rowTop - 10 && rowTop < img.height);
        UnloadImage(img);
    }
    {   // geo3d forest cover: trees stand along the surface's up, globe or map, and grow in
        auto surface = std::make_shared<geo3d::Surface>(geo::Projection::equirectangular(), geo::LonLat{ 20, 55 }, 1);
        geo::Geometry patch;
        patch.polygons.push_back({ { { 5, 45 }, { 35, 45 }, { 35, 65 }, { 5, 65 }, { 5, 45 } } });
        auto s = std::make_shared<g3d::Scene>();
        s->lights.push_back(g3d::Light::directional({ 1, 2, 3 }, WHITE, 1.4f, false));
        g3d::Object3D* world = s->add();
        g3d::Material ground;
        ground.color = { 200, 40, 200, 255 };
        geo3d::base(*world, surface, ground, 64);
        float u = 1;
        geo3d::ForestCover cover;
        cover.spacing = 1.2f;
        cover.height = 0.05f;
        cover.grow = [&u](int) { return u; };
        g3d::Object3D* trees = geo3d::forest(*world, surface, { patch }, cover);
        auto standing = [&](float morph) {
            surface->morph = morph;
            trees->up(0, *trees);
            float worst = 1;
            size_t n = 0;
            for (const auto& ch : trees->children)
                for (const g3d::Instance& in : ch->instances->list) {
                    const Vector3 dir = Vector3Transform({ 0, 1, 0 }, MatrixRotateXYZ(Vector3Scale({ in.pitch, 0, in.bank }, DEG2RAD)));
                    // the tree's base is where the surface is; compare its +y with the surface's up there
                    float bestDot = -2;
                    for (float lon = 5; lon <= 35; lon += 0.25f)
                        for (float lat = 45; lat <= 65; lat += 0.25f)
                            if (Vector3DistanceSqr(surface->point({ lon, lat }), in.position) < 1e-4f)
                                bestDot = std::max(bestDot, Vector3DotProduct(dir, surface->up({ lon, lat })));
                    if (bestDot > -2) worst = std::min(worst, bestDot);
                    n++;
                }
            return std::make_pair(n, worst);
        };
        const auto globe = standing(0), half = standing(0.5f), flat = standing(1);
        u = 0;
        const auto none = standing(0);
        u = 1;
        std::printf("[scene3d] map forest: %zu tree parts; up vs surface up, worst cosine %.4f globe, %.4f half, %.4f map; %zu at grow 0\n",
                    globe.first, globe.second, half.second, flat.second, none.first);
        CHECK(globe.first > 300 && none.first == 0);
        CHECK(globe.second > 0.995f && half.second > 0.995f && flat.second > 0.995f);
        surface->morph = 0;
        s->camera.position = Vector3Scale(surface->onGlobe({ 20, 55 }), 2.6f);
        s->camera.target = { 0, 0, 0 };
        Comp c = longComp(s, 320, 240);
        Image img = render(c, 2);
        dumpField("map-forest.png", img);
        int green = 0;
        for (int y = 0; y < img.height; y++)
            for (int x = 0; x < img.width; x++) {
                const Color q = GetImageColor(img, x, y);
                green += q.g > q.r && q.g > q.b && q.g > 30;
            }
        std::printf("[scene3d] map forest on the globe: %d green px\n", green);
        CHECK(green > 800);
        UnloadImage(img);
    }
    {   // grass growth: blades come up from a point outwards; none at grow 0, rising, not popping
        auto field = [&](float grow) {
            auto s = meadow([grow](g3d::Field& f) {
                f.arrival = flora::arrivalFrom({ 0, -4 }, 12);
                f.grow = grow;
                f.gust = 0; f.sway = 0;
            });
            s->camera.position = { 0, 9, 8 };
            s->camera.target = { 0, 0, -4 };
            Comp c = longComp(s, 240, 160);
            return render(c, 4);
        };
        auto blades = [](const Image& img, int* nearCentre = nullptr) {
            int n = 0, near = 0;
            for (int y = 0; y < img.height; y++)
                for (int x = 0; x < img.width; x++) {
                    const Color c = GetImageColor(img, x, y);
                    if (c.g > c.r && c.g > c.b) { n++; if (std::abs(x - img.width / 2) < 30 && std::abs(y - img.height / 2) < 30) near++; }
                }
            if (nearCentre) *nearCentre = near;
            return n;
        };
        std::vector<int> counts;
        int nearHalf = 0;
        for (int k = 0; k <= 10; k++) {
            Image img = field(k / 10.f);
            counts.push_back(blades(img, k == 4 ? &nearHalf : nullptr));
            UnloadImage(img);
        }
        int worst = 0;
        for (size_t i = 1; i < counts.size(); i++) worst = std::max(worst, std::abs(counts[i] - counts[i - 1]));
        std::printf("[scene3d] grass growing: %d blade px at 0, %d at 0.4 (%d near the start), %d at 1; largest step %d\n",
                    counts[0], counts[4], nearHalf, counts[10], worst);
        CHECK(counts[0] == 0);
        CHECK(counts[4] > 0 && counts[4] < counts[10] && nearHalf > 0);
        CHECK(worst < counts[10] / 3);
    }
    {   // a forest: time printed, not checked. 1 km square, ~10^4 trees, with RMTN_FIELD_BENCH.
        const bool bench = std::getenv("RMTN_FIELD_BENCH") != nullptr;
        const float half = bench ? 500.f : 120.f;
        const auto placed = flora::scatter(flora::rect({ -half, -half }, { half, half }), { 7.f, 0.3f, 60, 0.8f, 1.2f, 4 }, 3);
        // Impostors as they were (one row of pictures, no shadows) against the new ones without and with shadows.
        flora::TreeBuild oneRow;
        oneRow.elevations = 1;
        std::vector<flora::TreePtr> legacy;
        for (int k = 0; k < 4; k++) legacy.push_back(flora::makeTree(flora::tree(names[k]), oneRow));
        struct Variant { const char* name; bool old; bool shadows; };
        for (const Variant v : { Variant{ "one row, no shadows (as before)", true, false }, Variant{ "three rows, impostor shadows off", false, false },
                                 Variant{ "three rows, impostor shadows on", false, true } }) {
            const bool shadows = v.shadows;
            auto s = treeStage({ 70, 92, 40, 255 });
            s->camera.position = { 0, 30, half + 20 };
            s->camera.target = { 0, 6, half - 60 };
            s->camera.fov = 55;
            flora::ForestSpec fs;
            fs.impostorShadows = shadows;
            const std::vector<flora::TreePtr>& kinds = v.old ? legacy : std::vector<flora::TreePtr>{ built[0], built[1], built[2], built[3] };
            flora::addForest(*s, placed, kinds, fs);
            Comp c = longComp(s, bench ? 1920 : 480, bench ? 1080 : 270);
            Image first = render(c, 30);
            if (shadows) dumpField("forest.png", first);
            UnloadImage(first);
            const int frames = bench ? 12 : 2;
            double total = 0, best = 1e30;
            for (int f = 31; f < 31 + frames; f++) {
                const auto t0 = std::chrono::steady_clock::now();
                UnloadImage(render(c, f));
                const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
                total += ms;
                best = std::min(best, ms);
            }
            std::printf("[scene3d] forest %dx%d: %zu trees, %.1f ms a frame (best %.1f, with readback), %s\n", c.w, c.h,
                        placed.size(), total / frames, best, v.name);
        }
    }
    if (std::getenv("RMTN_FIELD_DUMP")) {     // the presets in a row, near enough for the full trees
        auto s = treeStage({ 92, 104, 60, 255 });
        s->camera.position = { 0, 9, 52 };
        s->camera.target = { 0, 7, 0 };
        s->camera.fov = 50;
        std::vector<flora::Placement> row;
        for (int i = 0; i < 8; i++) row.push_back({ { -35.f + 10 * i, 0, 0 }, 0, 1, i, (uint32_t)i + 1 });
        flora::ForestSpec fs;
        fs.reducedFrom = 200; fs.impostorFrom = 300;
        flora::addForest(*s, row, built, fs);
        Comp c = longComp(s, 1600, 600);
        Image img = render(c, 10, 2);
        dumpField("trees.png", img);
        UnloadImage(img);
        fs.reducedFrom = 1; fs.impostorFrom = 2;               // the same row as impostors
        s->objects.pop_back();
        flora::addForest(*s, row, built, fs);
        Image far = render(c, 10, 2);
        dumpField("trees-impostors.png", far);
        UnloadImage(far);
        s->objects.pop_back();                                  // close: oak, birch, pine, palm
        fs.reducedFrom = 200; fs.impostorFrom = 300;
        std::vector<flora::Placement> four;
        const int pick[4] = { 0, 1, 2, 4 };
        for (int i = 0; i < 4; i++) four.push_back({ { -21.f + 14 * i, 0, 0 }, 30.f * i, 1, pick[i], (uint32_t)i + 1 });
        flora::addForest(*s, four, built, fs);
        s->camera.position = { 0, 6, 30 };
        s->camera.target = { 0, 7, 0 };
        s->camera.fov = 60;
        s->sky = g3d::Sky::day();
        Image near = render(c, 10, 2);
        dumpField("trees-close.png", near);
        UnloadImage(near);
        s->objects.pop_back();                                  // an oak growing, u 0.15 .. 1
        std::vector<flora::Placement> stages;
        for (int i = 0; i < 6; i++) stages.push_back({ { -30.f + 12 * i, 0, 0 }, 0, 0.8f, i % 2 ? 0 : 3, (uint32_t)i });
        flora::addGrowingTrees(*s, stages, built, [](int, const flora::Placement& p) { return 0.15f + 0.17f * p.id; });
        s->camera.position = { 0, 7, 34 };
        Image strip = render(c, 10, 2);
        dumpField("growth.png", strip);
        UnloadImage(strip);
    }
}

// ---- map forests at a distance (geo3d::forest levels by size on screen)
struct MapForestRig {
    std::shared_ptr<geo3d::Surface> surface;
    g3d::ScenePtr scene;
    g3d::Object3D* trees = nullptr;
};
// ~100,000 trees over a 40 x 25 degree patch, the globe seen from `distance` (in globe radii).
static MapForestRig mapForestRig(float spacing, float morph, float impostorBelow = 10, float mergeBelow = 12, float speckle = 0.35f, bool shadows = false) {
    MapForestRig r;
    r.surface = std::make_shared<geo3d::Surface>(geo::Projection::equirectangular(), geo::LonLat{ 20, 55 }, 1);
    r.surface->morph = morph;
    geo::Geometry patch;
    patch.polygons.push_back({ { { 0, 42 }, { 40, 42 }, { 40, 67 }, { 0, 67 }, { 0, 42 } } });
    r.scene = std::make_shared<g3d::Scene>();
    r.scene->ambient = 0.35f;
    r.scene->lights.push_back(g3d::Light::directional({ 1, 2, 3 }, { 255, 244, 224, 255 }, 1.6f, shadows));
    g3d::Object3D* world = r.scene->add();
    g3d::Material ground;
    ground.color = { 70, 96, 60, 255 };
    ground.roughness = 1;
    geo3d::base(*world, r.surface, ground, 96);
    geo3d::ForestCover cover;
    cover.spacing = spacing;
    cover.impostorBelow = impostorBelow;
    cover.mergeBelow = mergeBelow;
    cover.pictureSpeckle = speckle;
    r.trees = geo3d::forest(*world, r.surface, { patch }, cover);
    return r;
}
static void aimAtPatch(MapForestRig& r, float distance) {
    const Vector3 centre = r.surface->point({ 20, 55 }, 0);
    const Vector3 up = r.surface->up({ 20, 55 });
    r.scene->camera.position = Vector3Add(centre, Vector3Scale(up, distance));
    r.scene->camera.target = centre;
    r.scene->camera.upVector = { 0, 0, -1 };
    if (std::fabs(Vector3DotProduct(up, { 0, 0, -1 })) > 0.9f) r.scene->camera.upVector = { 0, 1, 0 };
}
static void mapForestTests() {
    const bool onlyTop = std::getenv("RMTN_TOPDOWN_ONLY") != nullptr;
    const char* dumpDir = std::getenv("RMTN_L6_DUMP");
    auto dump = [&](const char* name, const Image& img) {
        if (!dumpDir) return;
        fs::create_directories(dumpDir);
        ExportImage(img, (fs::path(dumpDir) / name).string().c_str());
    };
    if (!onlyTop) {   // 100,000 trees zoomed out: every tree a mesh (impostorBelow 0, what it always did) against
        // levels by screen size. Time is printed (best of several, and the mean), never checked: a
        // GPU's is noisy. What is checked is what is submitted, which is exact.
        struct Run { double best = 1e9, mean = 0; long long bodies = 0, triangles = 0; };
        auto run = [](MapForestRig& r, float dist) {
            aimAtPatch(r, dist);
            Comp c = longComp(r.scene, 1280, 720);
            Canvas canvas;
            canvas.load(c.w, c.h, 1);
            Run out;
            const int runs = 6, per = 3;
            presentFrame(c, 1, canvas);                           // warm: bakes pictures, uploads
            for (int i = 0; i < runs; i++) {
                const auto t0 = std::chrono::steady_clock::now();
                for (int f = 0; f < per; f++) presentFrame(c, 2 + i * per + f, canvas);
                Image sync = LoadImageFromTexture(canvas.output().texture);   // waits for the GPU
                const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / per;
                UnloadImage(sync);
                out.best = std::min(out.best, ms);
                out.mean += ms / runs;
            }
            out.bodies = g3d::instanceBodiesSubmitted();
            out.triangles = g3d::instanceTrianglesSubmitted();
            canvas.unload();
            return out;
        };
        MapForestRig meshes = mapForestRig(0.0545f, 1, 0, 0), pictures = mapForestRig(0.0545f, 1, 36, 0), merged = mapForestRig(0.0545f, 1);
        std::printf("[scene3d] map forest bench: 100,000 trees, 1280x720, a flat map; bodies, triangles, ms best/mean\n");
        std::printf("[scene3d]   %-8s | %-38s | %-38s | %-38s\n", "radii up", "every tree a mesh (before)", "meshes and pictures", "and far trees merged (default)");
        for (float dist : { 7.f, 3.5f, 1.2f, 0.35f }) {
            const Run a = run(meshes, dist), b = run(pictures, dist), c = run(merged, dist);
            auto cell = [](const Run& r) {
                char s[96];
                std::snprintf(s, sizeof s, "%7lld %10lld %5.1f/%5.1f", r.bodies, r.triangles, r.best, r.mean);
                return std::string(s);
            };
            std::printf("[scene3d]   %-8.2f | %-38s | %-38s | %-38s\n", dist, cell(a).c_str(), cell(b).c_str(), cell(c).c_str());
            if (dist > 3) {                                                             // zoomed out: far fewer bodies and triangles
                CHECK(b.triangles * 20 < a.triangles && c.triangles * 20 < a.triangles && c.triangles > 0);
                CHECK(c.bodies * 4 < a.bodies && c.bodies < b.bodies);
            }
            if (dist < 0.5f) CHECK(c.triangles * 2 > a.triangles);                      // close: hardly any change
        }
    }
    if (!onlyTop) {   // the picture does not change when trees become pictures and the far ones merge: same mean
        // brightness, and the same shapes at 8 px cells (where trees can no longer be told apart)
        auto grab = [&](MapForestRig& r, float dist, const char* name) {
            aimAtPatch(r, dist);
            Comp c = longComp(r.scene, 960, 540);
            Image img = render(c, 3);
            dump(name, img);
            return img;
        };
        auto cells = [](const Image& img) {                        // 8 x 8 px cell luminances
            std::vector<float> out;
            for (int y = 0; y + 8 <= img.height; y += 8)
                for (int x = 0; x + 8 <= img.width; x += 8) {
                    float sum = 0;
                    for (int j = 0; j < 8; j++)
                        for (int i = 0; i < 8; i++) sum += luma(GetImageColor(img, x + i, y + j));
                    out.push_back(sum / 64.f);
                }
            return out;
        };
        MapForestRig meshes = mapForestRig(0.0545f, 1, 0, 0), merged = mapForestRig(0.0545f, 1), allPictures = mapForestRig(0.0545f, 1, 1e6f, 0);
        for (float dist : { 7.f, 3.5f, 2.6f, 2.0f, 1.5f, 1.2f, 0.6f, 0.3f }) {
            char n0[48], n1[48];
            std::snprintf(n0, sizeof n0, "look_%.2f_meshes.png", dist);
            std::snprintf(n1, sizeof n1, "look_%.2f_levels.png", dist);
            Image a = grab(meshes, dist, n0), b = grab(merged, dist, n1);
            const std::vector<float> ca = cells(a), cb = cells(b);
            double ma = 0, mb = 0, diff = 0;
            for (size_t i = 0; i < ca.size(); i++) { ma += ca[i]; mb += cb[i]; diff += std::fabs(ca[i] - cb[i]); }
            ma /= ca.size(); mb /= ca.size(); diff /= ca.size();
            std::printf("[scene3d] map forest look at %.2f radii: mean luma %.1f meshes, %.1f levels (ratio %.3f); mean 8px-cell difference %.2f\n",
                        dist, ma, mb, mb / ma, diff);
            {   // grain and colour over the middle of the patch: the mean 3x3-box local contrast of luma, and
                // mean rgb with its saturation (max - min), meshes against the default
                auto grain = [](const Image& img) {
                    double s = 0;
                    int n = 0;
                    for (int y = 245; y < 295; y++)
                        for (int x = 450; x < 510; x++) {
                            double m = 0;
                            for (int j = -2; j <= 2; j++)
                                for (int i = -2; i <= 2; i++) m += luma(GetImageColor(img, x + i, y + j));
                            s += std::fabs(luma(GetImageColor(img, x, y)) - m / 25);
                            n++;
                        }
                    return s / n;
                };
                auto rgbOf = [](const Image& img, double out[4]) {
                    out[0] = out[1] = out[2] = out[3] = 0;
                    for (int y = 245; y < 295; y++)
                        for (int x = 450; x < 510; x++) {
                            const Color c = GetImageColor(img, x, y);
                            out[0] += c.r / 3000.0; out[1] += c.g / 3000.0; out[2] += c.b / 3000.0;
                            out[3] += (std::max({ c.r, c.g, c.b }) - std::min({ c.r, c.g, c.b })) / 3000.0;
                        }
                };
                double ra[4], rb[4];
                rgbOf(a, ra); rgbOf(b, rb);
                const double ga = grain(a), gb = grain(b);
                std::printf("[scene3d] map forest grain at %.2f radii: local contrast %.2f meshes, %.2f levels (x%.2f); rgb %.0f %.0f %.0f sat %.0f meshes, %.0f %.0f %.0f sat %.0f levels\n",
                            dist, ga, gb, gb / std::max(ga, 1e-3), ra[0], ra[1], ra[2], ra[3], rb[0], rb[1], rb[2], rb[3]);
                CHECK(gb / std::max(ga, 1e-3) > 0.6 && gb / std::max(ga, 1e-3) < 1.5);
                CHECK(std::fabs(ra[3] - rb[3]) < 10);
                if (dist >= 0.5f && dist <= 1.3f) CHECK(diff < 2.0);
            }
            {   // all pictures, whatever the size: its contrast (luma spread over 8 px cells) against the meshes'
                char n2[48];
                std::snprintf(n2, sizeof n2, "look_%.2f_pictures.png", dist);
                Image p = grab(allPictures, dist, n2);
                auto spread = [](const std::vector<float>& v) {
                    double m = 0, s = 0;
                    for (float x : v) m += x;
                    m /= v.size();
                    for (float x : v) s += (x - m) * (x - m);
                    return std::sqrt(s / v.size());
                };
                const std::vector<float> cp = cells(p);
                double mp = 0;
                for (float x : cp) mp += x / cp.size();
                std::printf("[scene3d] map forest all pictures at %.2f radii: mean luma %.1f (meshes %.1f), 8px-cell spread %.2f (meshes %.2f)\n", dist, mp, ma, spread(cp), spread(ca));
                if (dist <= 1.5f) {        // the patch's middle: the pictures' mean colour is the crowns' (within 16 of 255)
                    double rgbP[3] = { 0, 0, 0 }, rgbM[3] = { 0, 0, 0 };
                    for (int y = 200; y < 340; y++)
                        for (int x = 400; x < 560; x++) {
                            const Color cp0 = GetImageColor(p, x, y), cm0 = GetImageColor(a, x, y);
                            rgbP[0] += cp0.r; rgbP[1] += cp0.g; rgbP[2] += cp0.b;
                            rgbM[0] += cm0.r; rgbM[1] += cm0.g; rgbM[2] += cm0.b;
                        }
                    const double n = 160.0 * 140.0;
                    std::printf("[scene3d] map forest all pictures at %.2f radii: mean rgb %.0f %.0f %.0f (meshes %.0f %.0f %.0f)\n", dist,
                                rgbP[0] / n, rgbP[1] / n, rgbP[2] / n, rgbM[0] / n, rgbM[1] / n, rgbM[2] / n);
                    for (int ch = 0; ch < 3; ch++) CHECK(std::fabs(rgbP[ch] - rgbM[ch]) / n < 16);
                }
                UnloadImage(p);
            }
            CHECK(mb / ma > 0.9 && mb / ma < 1.1);
            CHECK(diff < 0.08 * ma);
            UnloadImage(a);
            UnloadImage(b);
        }
    }
    {   // seen from straight above and from 60 degrees: the default forest has no brown (trunk) speckle the
        // meshes lack, no dark spot or ring round the point below the camera, and no radial pattern in
        // the strokes (a swirl). Counts and means over the patch (pixels that are not the flat ground).
        MapForestRig meshes = mapForestRig(0.0545f, 1, 0, 0), merged = mapForestRig(0.0545f, 1);
        struct Stats { double patch = 0, brown = 0, orange = 0, ring[5] = { 0, 0, 0, 0, 0 }, mean = 0, centre = 0, darkest = 1e9, swirl = 0, spread = 0, grain = 0; };
        auto measure = [&](const Image& img, bool slanted) {
            Stats st;
            const Color bg = GetImageColor(img, img.width / 2, img.height - 3);
            auto inPatch = [&](Color c) { return std::abs(c.r - bg.r) + std::abs(c.g - bg.g) + std::abs(c.b - bg.b) > 24; };
            std::vector<char> mask((size_t)img.width * img.height, 0);
            {   // close the gaps between trees: a pixel within 3 px of a tree pixel in every direction is the patch
                const int W2 = img.width, H2 = img.height;
                std::vector<char> raw((size_t)W2 * H2), grown((size_t)W2 * H2, 0);
                for (int y = 0; y < H2; y++) for (int x = 0; x < W2; x++) raw[(size_t)y * W2 + x] = (!slanted || y > H2 / 4) && inPatch(GetImageColor(img, x, y));
                for (int y = 3; y < H2 - 3; y++) for (int x = 3; x < W2 - 3; x++) {
                    bool any = false;
                    for (int j = -3; j <= 3 && !any; j++) for (int i = -3; i <= 3; i++) if (raw[(size_t)(y + j) * W2 + x + i]) { any = true; break; }
                    grown[(size_t)y * W2 + x] = any;
                }
                for (int y = 3; y < H2 - 3; y++) for (int x = 3; x < W2 - 3; x++) {
                    bool all = true;
                    for (int j = -3; j <= 3 && all; j++) for (int i = -3; i <= 3; i++) if (!grown[(size_t)(y + j) * W2 + x + i]) { all = false; break; }
                    mask[(size_t)y * W2 + x] = all;
                }
            }
            int x0 = img.width, x1 = 0, y0 = img.height, y1 = 0;
            for (int y = 0; y < img.height; y++)
                for (int x = 0; x < img.width; x++)
                    if (mask[(size_t)y * img.width + x]) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
            const double cx = 0.5 * (x0 + x1), cy = 0.5 * (y0 + y1), rmax = 0.42 * std::min(x1 - x0, y1 - y0);
            double cnt[5] = { 0, 0, 0, 0, 0 }, cc = 0;
            std::vector<float> l((size_t)img.width * img.height, -1.f);
            for (int y = 0; y < img.height; y++)
                for (int x = 0; x < img.width; x++) {
                    const Color c = GetImageColor(img, x, y);
                    if (!mask[(size_t)y * img.width + x]) continue;
                    const double r = std::hypot(x - cx, y - cy) / rmax;
                    st.patch++;
                    st.mean += luma(c);
                    if (c.r > c.g + 4) st.orange++;                 // red above green: trunk and orange, never leaves
                    if (c.r >= c.g || (c.r > 0.85 * c.g && c.r > c.b + 25)) st.brown++;   // and the wider net: red not below green, or a yellow-brown hue
                    if (r < 1) { const int k = std::min(4, (int)(r * 5)); st.ring[k] += luma(c); cnt[k]++; }
                    if (r < 0.1) { st.centre += luma(c); cc++; }
                    l[(size_t)y * img.width + x] = (float)luma(c);
                }
            st.mean /= std::max(st.patch, 1.0);
            st.centre /= std::max(cc, 1.0);
            for (int k = 0; k < 5; k++) st.ring[k] /= std::max(cnt[k], 1.0);
            // the darkest 9x9 box mean inside the middle 60% of the patch
            for (int y = (int)(cy - 0.3 * (y1 - y0)); y < (int)(cy + 0.3 * (y1 - y0)); y += 2)
                for (int x = (int)(cx - 0.3 * (x1 - x0)); x < (int)(cx + 0.3 * (x1 - x0)); x += 2) {
                    double s = 0; int n = 0;
                    for (int j = -4; j <= 4; j++) for (int i = -4; i <= 4; i++) { const float v = l[(size_t)(y + j) * img.width + x + i]; if (v >= 0) { s += v; n++; } }
                    if (n == 81) st.darkest = std::min(st.darkest, s / n);
                }
            // radial structure: gradient direction against the direction from the centre (doubled angle),
            // weighted by gradient strength; 0 for texture without a pattern
            double sc = 0, ss = 0, sw = 0;
            for (int y = y0 + 3; y < y1 - 3; y++)
                for (int x = x0 + 3; x < x1 - 3; x++) {
                    const float a = l[(size_t)y * img.width + x + 1], b = l[(size_t)y * img.width + x - 1], c = l[(size_t)(y + 1) * img.width + x], d = l[(size_t)(y - 1) * img.width + x];
                    if (a < 0 || b < 0 || c < 0 || d < 0 || std::hypot(x - cx, y - cy) < 0.15 * rmax || std::hypot(x - cx, y - cy) > 1.1 * rmax) continue;
                    const double gx = a - b, gy = c - d, w = gx * gx + gy * gy;
                    const double th = std::atan2(gy, gx) - std::atan2(y - cy, x - cx);
                    sc += w * std::cos(2 * th); ss += w * std::sin(2 * th); sw += w;
                }
            st.swirl = std::hypot(sc, ss) / std::max(sw, 1e-9);
            {   // blotchiness: the spread of 8 px cell means over the patch's cells, and the mean 3x3 local contrast
                std::vector<double> cell;
                for (int y = y0; y + 8 <= y1; y += 8)
                    for (int x = x0; x + 8 <= x1; x += 8) {
                        double sum = 0; bool ok = true;
                        for (int j = 0; j < 8 && ok; j++) for (int i = 0; i < 8; i++) { const float v = l[(size_t)(y + j) * img.width + x + i]; if (v < 0) { ok = false; break; } sum += v; }
                        if (ok) cell.push_back(sum / 64);
                    }
                double m = 0, v2 = 0;
                for (double c : cell) m += c / std::max<size_t>(cell.size(), 1);
                for (double c : cell) v2 += (c - m) * (c - m) / std::max<size_t>(cell.size(), 1);
                st.spread = std::sqrt(v2);
                double g = 0; int n = 0;
                for (int y = y0 + 2; y < y1 - 2; y++)
                    for (int x = x0 + 2; x < x1 - 2; x++) {
                        double s = 0; bool ok = true;
                        for (int j = -1; j <= 1 && ok; j++) for (int i = -1; i <= 1; i++) { const float v = l[(size_t)(y + j) * img.width + x + i]; if (v < 0) { ok = false; break; } s += v; }
                        if (ok) { g += std::fabs(l[(size_t)y * img.width + x] - s / 9); n++; }
                    }
                st.grain = g / std::max(n, 1);
            }
            return st;
        };
        if (dumpDir && std::getenv("RMTN_L6_DIAG")) {   // which part makes what: pictures alone, merging alone, both
            struct V { const char* name; float below, merge; };
            for (const V& v : { V{ "diag_pictures", 1e6f, 0.f }, V{ "diag_merge", 0.f, 12.f }, V{ "diag_both", 1e6f, 12.f } }) {
                MapForestRig r = mapForestRig(0.0545f, 1, v.below, v.merge);
                aimAtPatch(r, 2.0f);
                Comp c = longComp(r.scene, 960, 540);
                Image im = render(c, 3);
                const Stats s = measure(im, false);
                std::printf("[scene3d] diag %s: brown %.0f, rings %.0f %.0f %.0f %.0f %.0f, radial structure %.3f\n", v.name, s.brown, s.ring[0], s.ring[1], s.ring[2], s.ring[3], s.ring[4], s.swirl);
                dump((std::string(v.name) + ".png").c_str(), im);
                UnloadImage(im);
            }
        }
        for (int slant = 0; slant < 2; slant++)
            for (float dist : { 3.5f, 2.6f, 2.0f, 1.5f }) {
                Image img[2];
                for (int k = 0; k < 2; k++) {
                    MapForestRig& r = k ? merged : meshes;
                    aimAtPatch(r, dist);
                    if (slant) {
                        const geo::LonLat at{ 20, 55 };
                        const Vector3 centre = r.surface->point(at, 0), up = r.surface->up(at), north = r.surface->north(at);
                        r.scene->camera.position = Vector3Add(centre, Vector3Add(Vector3Scale(up, dist * 0.866f), Vector3Scale(north, -dist * 0.5f)));
                        r.scene->camera.upVector = north;
                    }
                    Comp c = longComp(r.scene, 960, 540);
                    img[k] = render(c, 3);
                    char name[64];
                    std::snprintf(name, sizeof name, "top_%s_%.2f_%s.png", slant ? "60deg" : "down", dist, k ? "levels" : "meshes");
                    dump(name, img[k]);
                }
                const Stats a = measure(img[0], slant != 0), b = measure(img[1], slant != 0);
                std::printf("[scene3d] map forest %s at %.2f radii: brown (r>=g or yellow-brown) %.0f meshes, %.0f default, orange (r>g+4) %.0f/%.0f, of %.0f px; rings (luma) meshes %.0f %.0f %.0f %.0f %.0f, default %.0f %.0f %.0f %.0f %.0f; centre %.0f/%.0f (mean %.0f/%.0f); darkest box %.0f/%.0f; radial structure %.3f meshes, %.3f default\n",
                            slant ? "60 degrees" : "straight down", dist, a.brown, b.brown, a.orange, b.orange, a.patch,
                            a.ring[0], a.ring[1], a.ring[2], a.ring[3], a.ring[4], b.ring[0], b.ring[1], b.ring[2], b.ring[3], b.ring[4],
                            a.centre, b.centre, a.mean, b.mean, a.darkest, b.darkest, a.swirl, b.swirl);
                std::printf("[scene3d]   blotches (8 px cell spread) %.2f meshes, %.2f default; grain (3x3) %.2f meshes, %.2f default (x%.2f)\n", a.spread, b.spread, a.grain, b.grain, b.grain / std::max(a.grain, 1e-3));
                CHECK(b.spread < 1.6 * a.spread + 0.5 && b.spread > 0.5 * a.spread);   // not much blotchier than the facets
                CHECK(b.grain > 0.5 * a.grain && b.grain < 1.5 * a.grain);       // a picture is smoother than facets, but not by half
                CHECK(b.brown <= 1.5 * a.brown + 0.002 * a.patch);
                CHECK(b.orange <= 1.5 * a.orange + 0.002 * a.patch);
                for (int k = 0; k < 5; k++) CHECK(std::fabs(b.ring[k] / std::max(a.ring[k], 1.0) - 1) < 0.10);
                CHECK(std::fabs(b.centre / std::max(b.mean, 1.0) - 1) < 0.10);
                CHECK(b.darkest > 0.80 * a.darkest);
                CHECK(b.swirl < a.swirl + 0.04);
                UnloadImage(img[0]); UnloadImage(img[1]);
            }
    }
    if (!onlyTop) {   // one tree, close and as its picture, on the globe, half way and the map, from several heights:
        // the silhouettes overlap and the colour agrees. (The same pictures the flora tests judge, here
        // standing along the surface's up instead of the world's.)
        auto isTree = [](Color c) { return !(c.r > 1.3f * c.g && c.b > 1.3f * c.g) && c.r + c.g + c.b > 12; };
        auto linear = [](unsigned char c) { return std::pow(c / 255.0, 2.2); };
        auto shot = [&](const flora::TreePtr& kind, float morph, float elev, float azimuth, bool picture) {
            auto surface = std::make_shared<geo3d::Surface>(geo::Projection::equirectangular(), geo::LonLat{ 20, 55 }, 1);
            surface->morph = morph;
            geo::Geometry patch;
            patch.polygons.push_back({ { { 19, 54 }, { 21, 54 }, { 21, 56 }, { 19, 56 }, { 19, 54 } } });
            auto s = std::make_shared<g3d::Scene>();
            s->ambient = 0.4f;
            s->lights.push_back(g3d::Light::directional({ 1, 2, 3 }, { 255, 240, 220, 255 }, 2.0f, false));
            g3d::Object3D* world = s->add();
            g3d::Material ground;
            ground.color = { 200, 40, 200, 255 };
            ground.roughness = 1;
            geo3d::base(*world, surface, ground, 96);
            geo3d::ForestCover cover;
            cover.spacing = 50;                                   // one tree
            cover.limit = 1;
            cover.height = 0.1f;
            cover.heightVariation = 0;
            cover.impostorBelow = picture ? 1e6f : 0;
            cover.mergeBelow = 0;
            cover.pictureSpeckle = 0;                             // one tree: no per-tree spread
            cover.blend = 0.01f;
            cover.trees = { kind };
            geo3d::forest(*world, surface, { patch }, cover);
            // The camera on a sphere of 0.22 round the tree's middle, at `elev` above the surface there.
            const geo::LonLat at{ 20, 55 };
            const Vector3 up = surface->up(at), north = surface->north(at);
            const Vector3 east = Vector3CrossProduct(north, up);
            const Vector3 side = Vector3Add(Vector3Scale(north, std::cos(azimuth * DEG2RAD)), Vector3Scale(east, std::sin(azimuth * DEG2RAD)));
            const Vector3 mid = Vector3Add(surface->point(at, 0), Vector3Scale(up, 0.05f));
            const float e = elev * DEG2RAD;
            s->camera.position = Vector3Add(mid, Vector3Add(Vector3Scale(side, 0.22f * std::cos(e)), Vector3Scale(up, 0.22f * std::sin(e))));
            s->camera.target = mid;
            s->camera.upVector = elev > 80 ? north : up;
            s->camera.nearPlane = 0.02f;
            Comp c = longComp(s, 240, 240);
            return render(c, 0);
        };
        flora::TreeBuild tb;                                      // the forest's default trees, one at a time
        tb.style = flora::TreeStyle::LowPoly; tb.frames = 1; tb.views = 8; tb.viewSize = 40;
        const char* kindNames[] = { "oak", "birch", "pine" };
        for (const char* kindName : kindNames) {
        const flora::TreePtr kind = flora::makeTree(flora::tree(kindName), tb);
        for (float morph : { 0.f, 0.5f, 1.f }) {
            std::printf("[scene3d] map forest %s vs its picture, morph %.1f:", kindName, morph);
            for (float elev : { 0.f, 30.f, 60.f, 85.f, 90.f }) {
                double overlap = 0, ratio = 0;
                const std::vector<float> azimuths = std::getenv("RMTN_L6_AZ") ? std::vector<float>{ 0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330 } : std::vector<float>{ 10, 70, 130, 190, 250, 310 };
                for (float az : azimuths) {
                    Image a = shot(kind, morph, elev, az, false), b = shot(kind, morph, elev, az, true);
                    int both = 0, either = 0;
                    double ya = 0, yb = 0;
                    for (int y = 0; y < a.height; y++)
                        for (int x = 0; x < a.width; x++) {
                            const Color ca = GetImageColor(a, x, y), cb = GetImageColor(b, x, y);
                            const bool p = isTree(ca), q = isTree(cb);
                            both += p && q; either += p || q;
                            if (p) ya += 0.2126 * linear(ca.r) + 0.7152 * linear(ca.g) + 0.0722 * linear(ca.b);
                            if (q) yb += 0.2126 * linear(cb.r) + 0.7152 * linear(cb.g) + 0.0722 * linear(cb.b);
                        }
                    overlap += (either ? (double)both / either : 0) / (double)azimuths.size();
                    ratio += (ya > 0 ? yb / ya : 0) / (double)azimuths.size();
                    if (std::getenv("RMTN_L6_AZ") && morph == 0) std::printf("\n     %s e%.0f az%.0f: x%.2f", kindName, elev, az, ya > 0 ? yb / ya : 0.0);
                    if (az == 100.f) {
                        char n0[64], n1[64];
                        std::snprintf(n0, sizeof n0, "tree_%s_m%.0f_e%.0f_full.png", kindName, morph * 10, elev);
                        std::snprintf(n1, sizeof n1, "tree_%s_m%.0f_e%.0f_picture.png", kindName, morph * 10, elev);
                        dump(n0, a);
                        dump(n1, b);
                    }
                    UnloadImage(a);
                    UnloadImage(b);
                }
                std::printf("  %.0f deg: overlap %.2f, luminance x%.2f;", elev, overlap, ratio);
                CHECK(overlap > 0.65);
                CHECK(ratio > 0.75 && ratio < 1.3);
            }
            std::printf("\n");
        }
        }
    }
    if (!onlyTop) {   // pictures stand along the surface's up: on the globe, half way and the map, each picture's quad
        // up, projected on screen, points where the surface's up does at its tree (and its quad faces
        // the camera). Seen from a slant, so that neither is degenerate.
        for (float morph : { 0.f, 0.5f, 1.f }) {
            MapForestRig r = mapForestRig(0.35f, morph, 1e6f, 0);       // every tree a picture
            const Vector3 centre = r.surface->point({ 20, 55 }, 0), up = r.surface->up({ 20, 55 });
            const Vector3 north = r.surface->north({ 20, 55 });
            r.scene->camera.position = Vector3Add(centre, Vector3Add(Vector3Scale(up, 1.2f), Vector3Scale(north, -2.1f)));   // 30 deg up, to the south
            r.scene->camera.target = centre;
            r.scene->camera.upVector = up;
            r.scene->camera.farPlane = 50;
            Comp c = longComp(r.scene, 640, 360);
            { Image shotImg = render(c, 0); char nm[48]; std::snprintf(nm, sizeof nm, "upright_m%.0f.png", morph * 10); dump(nm, shotImg); UnloadImage(shotImg); }
            size_t pictures = 0, hero = 0;
            double worstUp = 1, worstFace = 1, worstRadial = 1;
            for (const auto& ch : r.trees->children) {
                if (!ch->instances) continue;
                for (const g3d::Instance& in : ch->instances->list) {
                    if (Vector3LengthSqr(in.aim) < 1e-9f) { hero++; continue; }
                    pictures++;
                    // The quad's own axes, as the engine builds them.
                    const Vector3 z = Vector3Normalize(in.aim);
                    Vector3 y = Vector3Subtract(in.aimUp, Vector3Scale(z, Vector3DotProduct(in.aimUp, z)));
                    y = Vector3Normalize(y);
                    const Vector3 lift = Vector3Normalize(in.aimUp);
                    if (morph == 0) worstRadial = std::min(worstRadial, (double)Vector3DotProduct(lift, Vector3Normalize(in.position)));   // on the globe up is outward
                    // The picture's foot: its middle less half its side along y; screen directions at its middle.
                    const Vector2 p0 = project(r.scene->camera, in.position, 640, 360), p1 = project(r.scene->camera, Vector3Add(in.position, Vector3Scale(y, 0.01f)), 640, 360);
                    const Vector2 q1 = project(r.scene->camera, Vector3Add(in.position, Vector3Scale(lift, 0.01f)), 640, 360);
                    const Vector2 a{ p1.x - p0.x, p1.y - p0.y }, b{ q1.x - p0.x, q1.y - p0.y };
                    const float la = Vector2Length(a), lb = Vector2Length(b);
                    if (la > 1e-4f && lb > 1e-4f) worstUp = std::min(worstUp, (double)Vector2DotProduct(a, b) / (la * lb));
                    const Vector3 toCam = Vector3Normalize(Vector3Subtract(r.scene->camera.position, in.position));
                    worstFace = std::min(worstFace, (double)Vector3DotProduct(z, toCam));
                }
            }
            std::printf("[scene3d] map forest pictures at morph %.1f: %zu pictures (%zu meshes); quad up vs surface up on screen, worst cosine %.4f; quad faces the camera, worst cosine %.4f; lift against the globe's radius (morph 0) %.5f\n",
                        morph, pictures, hero, worstUp, worstFace, worstRadial);
            CHECK(pictures > 1000 && hero == 0);
            CHECK(worstUp > 0.99 && worstFace > 0.99 && worstRadial > 0.999);
        }
    }
    if (!onlyTop) {   // purity: a frame is the same drawn alone as drawn after another (the levels and the cross-fades
        // are functions of the camera and t, never of what was drawn before)
        auto frame = [&](int first, int then) {
            MapForestRig r = mapForestRig(0.12f, 0.5f);
            const Vector3 centre = r.surface->point({ 20, 55 }, 0), up = r.surface->up({ 20, 55 });
            const Vector3 north = r.surface->north({ 20, 55 });
            r.scene->camera.position = Vector3Add(centre, Vector3Add(Vector3Scale(up, 0.25f), Vector3Scale(north, -0.55f)));   // low and long: every level shows
            r.scene->camera.target = centre;
            r.scene->camera.upVector = up;
            Comp c = longComp(r.scene, 480, 270);
            if (first >= 0) UnloadImage(render(c, first));
            return render(c, then);
        };
        Image a = frame(-1, 7), b = frame(12, 7), d = frame(7, 7);
        long long diff = 0;
        for (int y = 0; y < a.height; y++)
            for (int x = 0; x < a.width; x++) {
                const Color ca = GetImageColor(a, x, y), cb = GetImageColor(b, x, y), cd = GetImageColor(d, x, y);
                diff += std::abs(ca.r - cb.r) + std::abs(ca.g - cb.g) + std::abs(ca.b - cb.b) + std::abs(ca.r - cd.r) + std::abs(ca.g - cd.g) + std::abs(ca.b - cd.b);
            }
        std::printf("[scene3d] map forest purity: frame 7 alone against frame 7 after frame 12 and after itself, summed difference %lld\n", diff);
        CHECK(diff == 0);
        dump("purity.png", a);
        UnloadImage(a); UnloadImage(b); UnloadImage(d);
    }
}

int main() try {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(W, H, "raymotion 3d");
    if (!IsWindowReady()) return 77;
    // Beside the test (ctest runs it in the build directory), not in the system temp
    // folder, which a sandboxed shell may not let it write to.
    const fs::path dir = fs::current_path() / "raymotion-scene3d-test";
    fs::create_directories(dir);
    if (std::getenv("RMTN_MAPFOREST_ONLY")) {
        mapForestTests();
        shutdownGraphics();
        CloseWindow();
        return failures ? 1 : 0;
    }
    if (std::getenv("RMTN_TREES_ONLY")) {            // while working on trees: just those
        trees();
        shutdownGraphics();
        CloseWindow();
        return failures ? 1 : 0;
    }
    geometryExtents();
    placementAndDepth();
    supersampledPlacement();
    lighting();
    shadows();
    lowSunShadows();
    viewportsAndOpacity();
    transparencyAndHierarchy();
    moreShadows();
    culling();
    instancing();
    vegetationMaterials(dir);
    waterTest();
    environment(dir);
    cubeSkybox(dir);
    skyAndFog();
    depthOfField();
    determinism();
    toonShading();
    goochShading();
    hatchShading();
    inkLines();
    models(dir);
    particles3d();
    surfaces();
    lines3d();
    grassField();
    trees();
    mapForestTests();
    devices();
    shutdownGraphics();
    CloseWindow();
    std::error_code ec;
    fs::remove_all(dir, ec);
    if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    std::puts("scene3d tests passed");
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "uncaught: %s\n", e.what());
    return 2;
}
