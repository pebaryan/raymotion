// scene3d_tests.cpp -- GPU 3D scenes: placement, depth, lighting, shadows, viewports,
// transparency, hierarchy, models (OBJ and a generated skinned glTF), determinism.
#include "rmtn/raymotion.hpp"
#include <raymath.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <cstring>
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

int main() try {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(W, H, "raymotion 3d");
    if (!IsWindowReady()) return 77;
    // Beside the test (ctest runs it in the build directory), not in the system temp
    // folder, which a sandboxed shell may not let it write to.
    const fs::path dir = fs::current_path() / "raymotion-scene3d-test";
    fs::create_directories(dir);
    geometryExtents();
    placementAndDepth();
    supersampledPlacement();
    lighting();
    shadows();
    viewportsAndOpacity();
    transparencyAndHierarchy();
    moreShadows();
    culling();
    environment(dir);
    skyAndFog();
    depthOfField();
    determinism();
    toonShading();
    goochShading();
    hatchShading();
    inkLines();
    models(dir);
    particles3d();
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
