// flora2d_render_tests.cpp -- 2D vegetation read back from the canvas: u = 0 draws nothing
// and u = 1 is the plant, growth only adds, the wind moves tips and not roots, the reveal
// front, a meadow stays in its shape, parallax and haze, the softness blur, frames pure in
// t, and the same drawing on a g3d::Surface. Needs OpenGL.
#include "rmtn/raymotion.hpp"
#include "rmtn/flora2d.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static Image render(Comp& c, int frame = 0) {
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, frame, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}
static Comp comp(std::function<void(const Ctx&, int)> fn, int w = 320, int h = 200) {
    Comp c;
    c.w = w; c.h = h; c.duration = 100; c.background = BLACK;
    addRoot(c.roots, 0, 100)->fn = std::move(fn);
    return c;
}
static Image draw(std::function<void(const Ctx&, int)> fn, int frame = 0, int w = 320, int h = 200) {
    Comp c = comp(std::move(fn), w, h);
    return render(c, frame);
}
static bool on(Color c) { return c.r + c.g + c.b > 24; }
// Lit pixels in a rectangle (whole image by default).
static int cover(const Image& img, int x0 = 0, int y0 = 0, int x1 = -1, int y1 = -1) {
    if (x1 < 0) x1 = img.width;
    if (y1 < 0) y1 = img.height;
    int n = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) n += on(GetImageColor(img, x, y));
    return n;
}
static int differ(const Image& a, const Image& b, int y0 = 0, int y1 = -1) {
    if (y1 < 0) y1 = a.height;
    int n = 0;
    for (int y = y0; y < y1; y++)
        for (int x = 0; x < a.width; x++) {
            const Color p = GetImageColor(a, x, y), q = GetImageColor(b, x, y);
            n += std::abs(p.r - q.r) + std::abs(p.g - q.g) + std::abs(p.b - q.b) > 6;
        }
    return n;
}
static void dump(const char* name, const Image& img) {
    if (const char* dir = std::getenv("RMTN_FIELD_DUMP")) {
        std::filesystem::create_directories(dir);
        ExportImage(img, (std::filesystem::path(dir) / name).string().c_str());
    }
}

static const std::vector<Vector2> line{ { 0, 180 }, { 320, 180 } };

static void reveal() {
    // u = 0 draws nothing; u = 1 is the same as no u; a tree covers more as it grows.
    flora2d::Wind w;
    auto all = [&](float u) {
        return [u, w](const Ctx& c, int t) {
            flora2d::grass(c, line, {}, t, u, 1, w);
            flora2d::shrub(c, { 60, 180 }, {}, t, u, 2, w);
            flora2d::tuft(c, { 280, 180 }, {}, 12, t, u, 3, w);
            flora2d::tree(c, { 160, 180 }, [] { flora2d::Tree s; s.height = 150; s.trunkWidth = 8; s.leafSize = 12; return s; }(), t, u, 4, w);
        };
    };
    Image none = draw(all(0), 7), full = draw(all(1), 7);
    Image plain = draw([w](const Ctx& c, int t) {
        flora2d::grass(c, line, {}, t);
        flora2d::shrub(c, { 60, 180 }, {}, t, 1, 2);
        flora2d::tuft(c, { 280, 180 }, {}, 12, t, 1, 3);
        flora2d::tree(c, { 160, 180 }, [] { flora2d::Tree s; s.height = 150; s.trunkWidth = 8; s.leafSize = 12; return s; }(), t, 1, 4);
    }, 7);
    dump("flora2d-full.png", full);
    CHECK(cover(none) == 0);
    CHECK(differ(full, plain) == 0);
    std::vector<int> grown;
    for (float u : { 0.1f, 0.3f, 0.5f, 0.65f, 0.8f, 1.f }) {
        Image img = draw([u](const Ctx& c, int t) {
            flora2d::Tree s; s.height = 150; s.trunkWidth = 8; s.leafSize = 12;
            flora2d::tree(c, { 160, 190 }, s, t, u, 4, flora2d::Wind{ 0 });
        }, 3);
        grown.push_back(cover(img));
        UnloadImage(img);
    }
    std::printf("[flora2d] tree coverage %d %d %d %d %d %d px at u 0.1 .. 1; all %d px at u 1, %d at u 0\n", grown[0], grown[1], grown[2],
                grown[3], grown[4], grown[5], cover(full), cover(none));
    bool rising = grown[0] > 0;
    for (size_t i = 1; i < grown.size(); i++) rising = rising && grown[i] >= grown[i - 1];
    CHECK(rising && grown.back() > 3 * grown[2]);                      // the leaves are most of it
    UnloadImage(none); UnloadImage(full); UnloadImage(plain);

    // Grass at u 0.5: the left of the line is up, its right is bare.
    Image half = draw([](const Ctx& c, int t) { flora2d::grass(c, line, {}, t, 0.5f, 1, flora2d::Wind{ 0 }); });
    const int left = cover(half, 0, 0, 120), right = cover(half, 220, 0, 320);
    std::printf("[flora2d] grass at u 0.5: %d px on the left, %d on the right\n", left, right);
    CHECK(left > 1500 && right == 0);
    UnloadImage(half);
}

static void wind() {
    // Tips move with t, roots don't; a still wind is still; a frame is the same however reached.
    flora2d::Wind w;
    w.gust = 1;
    auto scene = [w](const Ctx& c, int t) { flora2d::grass(c, line, flora2d::tallGrass(), t, 1, 5, w); };
    Image a = draw(scene, 0), b = draw(scene, 20), b2 = draw(scene, 20);
    const int tips = differ(a, b, 100, 150), roots = differ(a, b, 176, 180);
    std::printf("[flora2d] wind: %d px change among the tips, %d at the roots between frames 0 and 20\n", tips, roots);
    CHECK(tips > 2000 && roots < tips / 40);
    CHECK(differ(b, b2) == 0);
    auto calm = [](const Ctx& c, int t) { flora2d::grass(c, line, flora2d::tallGrass(), t, 1, 5, flora2d::Wind{ 0 }); };
    Image s0 = draw(calm, 0), s1 = draw(calm, 33);
    CHECK(differ(s0, s1) == 0);
    // A gust travels: the bend at x arrives later further downwind.
    flora2d::Wind g;
    g.sway = 0; g.strength = 1; g.gust = 1;
    int peakNear = 0, peakFar = 0;
    float bestNear = -1, bestFar = -1;
    for (int t = 0; t < 200; t++) {
        const float n = flora2d::bendAt(g, 0, t), f = flora2d::bendAt(g, 300, t);
        if (n > bestNear) { bestNear = n; peakNear = t; }
        if (f > bestFar) { bestFar = f; peakFar = t; }
    }
    std::printf("[flora2d] a gust peaks at x 0 on frame %d and at x 300 on frame %d\n", peakNear, peakFar);
    CHECK(peakFar > peakNear);
    UnloadImage(a); UnloadImage(b); UnloadImage(b2); UnloadImage(s0); UnloadImage(s1);
}

static void meadowAndLayers() {
    // A meadow keeps its blades' roots in its shape (tips may rise above its top edge).
    const std::vector<Vector2> shape{ { 60, 90 }, { 260, 90 }, { 300, 190 }, { 20, 190 } };
    Image m = draw([shape](const Ctx& c, int t) { flora2d::meadow(c, shape, flora2d::lawn(), t, 1, 1, flora2d::Wind{ 0 }, 0.5f); });
    dump("flora2d-meadow.png", m);
    const int inside = cover(m, 60, 100, 260, 185), below = cover(m, 0, 193, 320, 200), sides = cover(m, 0, 100, 12, 200) + cover(m, 308, 100, 320, 200);
    std::printf("[flora2d] meadow: %d px inside, %d below, %d beside its shape\n", inside, below, sides);
    CHECK(inside > 15000 && below == 0 && sides == 0);               // of 17,000 in the box
    UnloadImage(m);

    // Parallax: depth 1 stays, depth 0 moves by the camera; haze pulls colours to the haze.
    auto at = [](float depth, Vector2 cam) {
        return draw([depth, cam](const Ctx& c, int) {
            flora2d::ground(flora2d::parallax(c, depth, cam), { { 100, 100 }, { 140, 100 } }, 140, WHITE);
        });
    };
    Image still = at(0, { 0, 0 }), far = at(1, { 50, 0 }), near = at(0, { 50, 0 });
    CHECK(differ(still, far) == 0 && on(GetImageColor(near, 70, 120)) && !on(GetImageColor(near, 120, 120)));
    UnloadImage(still); UnloadImage(far); UnloadImage(near);
    const Color h = flora2d::hazed({ 40, 80, 40, 255 }, 1, { 200, 210, 220, 255 }), k = flora2d::hazed({ 40, 80, 40, 255 }, 0, { 200, 210, 220, 255 });
    CHECK(h.r == 200 && h.b == 220 && k.r == 40 && k.g == 80);

    // softness(): a layer filter that blurs a hard edge everywhere in the picture.
    auto edge = [](float px) {
        Comp c = comp([](const Ctx& cx, int) { dr::rect(cx, 0, 0, 160, 200, WHITE); });
        if (px > 0) c.roots[0]->isolate().filters.push_back(flora2d::softness(px));
        return render(c);
    };
    Image hard = edge(0), soft = edge(8);
    auto ramp = [](const Image& img, int y) {
        int mid = 0;
        for (int x = 140; x < 180; x++) { const Color q = GetImageColor(img, x, y); mid += q.r > 20 && q.r < 235; }
        return mid;
    };
    std::printf("[flora2d] softness 8 px: %d / %d grey px across the edge (top / bottom), %d unblurred\n", ramp(soft, 20), ramp(soft, 180), ramp(hard, 100));
    CHECK(ramp(hard, 100) == 0 && ramp(soft, 20) >= 6 && ramp(soft, 180) >= 6);
    UnloadImage(hard); UnloadImage(soft);
}

static void onSurface() {
    // The same drawing on a g3d::Surface in a 3D scene.
    auto s = std::make_shared<g3d::Scene>();
    s->camera.position = { 0, 0, 3 };
    s->camera.target = { 0, 0, 0 };
    s->lights.push_back(g3d::Light::directional({ 0, 0, 1 }, WHITE, 1, false));
    auto panel = std::make_shared<g3d::Surface>();
    panel->pixels = { 320, 200 };
    panel->size = { 3.2f, 2.0f };
    panel->draw = [](const Ctx& c, int t) {
        flora2d::grass(c, line, {}, t, 1, 1);
        flora2d::tree(c, { 160, 180 }, [] { flora2d::Tree s; s.height = 150; s.trunkWidth = 8; s.leafSize = 12; return s; }(), t, 1, 4);
    };
    s->add(panel);
    Comp c;
    c.w = 320; c.h = 200; c.duration = 20; c.background = BLACK;
    addRoot(c.roots, 0, 20)->fn = g3d::draw(s, {});
    Image img = render(c, 4);
    dump("flora2d-surface.png", img);
    const int n = cover(img);
    std::printf("[flora2d] on a surface: %d px\n", n);
    CHECK(n > 4000);
    UnloadImage(img);
}

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 200, "raymotion flora2d");
    if (!IsWindowReady()) return 77;
    reveal();
    wind();
    meadowAndLayers();
    onSurface();
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d flora2d check(s) failed\n", failures); return 1; }
    std::printf("flora2d rendering tests passed\n");
    return 0;
}
