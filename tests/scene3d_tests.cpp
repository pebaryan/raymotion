// scene3d_tests.cpp -- GPU 3D scenes: placement, depth, lighting, shadows, viewports,
// transparency, hierarchy, models (OBJ and a generated skinned glTF), determinism.
#include "rmtn/raymotion.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

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

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(W, H, "raymotion 3d");
    if (!IsWindowReady()) return 77;
    const fs::path dir = fs::temp_directory_path() / "raymotion-scene3d-test";
    fs::create_directories(dir);
    geometryExtents();
    placementAndDepth();
    supersampledPlacement();
    lighting();
    shadows();
    viewportsAndOpacity();
    transparencyAndHierarchy();
    moreShadows();
    environment(dir);
    depthOfField();
    determinism();
    models(dir);
    shutdownGraphics();
    CloseWindow();
    std::error_code ec;
    fs::remove_all(dir, ec);
    if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    std::puts("scene3d tests passed");
    return 0;
}
