// motionblur_tests.cpp -- motion blur read back: a still frame is unchanged by blurring, a
// moving rect smears exactly as far as the shutter says, layer blur only touches its own
// node, props animated in `up` blur too, and subframe() is 0 outside sampling.
// Needs OpenGL.
#include "rmtn/raymotion.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static Image frame(Comp& c, int f) {
    Canvas canvas;
    canvas.load(c.w, c.h, c.supersample);
    presentFrame(c, f, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}
static Comp comp() {
    Comp c;
    c.w = 600; c.h = 200; c.duration = 20; c.background = BLACK; c.supersample = 1;
    return c;
}
static int red(const Image& img, int x, int y) { return GetImageColor(img, x, y).r; }
static bool same(const Image& a, const Image& b) {
    for (int y = 0; y < a.height; y++)
        for (int x = 0; x < a.width; x++) {
            const Color p = GetImageColor(a, x, y), q = GetImageColor(b, x, y);
            if (std::abs(p.r - q.r) > 1 || std::abs(p.g - q.g) > 1 || std::abs(p.b - q.b) > 1) return false;
        }
    return true;
}
// Columns along the row y whose red is strictly between 10 and 245: the smeared part.
static int partial(const Image& img, int y, int x0, int x1) {
    int n = 0;
    for (int x = x0; x < x1; x++) { const int r = red(img, x, y); n += r > 10 && r < 245; }
    return n;
}

static const Color PURE{ 255, 0, 0, 255 };
static double maxSub = 0;

// A 100 px rect moving 40 px a frame: x = 40 * frameTime(t).
static void moving(const Ctx& cx, int t) {
    maxSub = std::max(maxSub, std::fabs(subframe()));
    dr::rect(cx, (float)(40 * frameTime(t)), 50, 100, 100, PURE);
}

static void still() {
    Comp a = comp();
    addRoot(a.roots, 0, 20)->fn = [](const Ctx& cx, int) { dr::rect(cx, 100, 50, 120, 80, PURE); dr::rect(cx, 300, 60, 50, 50, WHITE); };
    Comp b = comp();
    addRoot(b.roots, 0, 20)->fn = a.roots[0]->fn;
    b.motionBlur = { 8, 0.5f };
    Image ia = frame(a, 5), ib = frame(b, 5);
    CHECK(same(ia, ib));
    UnloadImage(ia); UnloadImage(ib);
}

static void smear() {
    Comp sharp = comp();
    addRoot(sharp.roots, 0, 20)->fn = moving;
    Comp blurred = comp();
    addRoot(blurred.roots, 0, 20)->fn = moving;
    blurred.motionBlur = { 16, 1.0f };      // a whole frame: +-20 px around x = 200
    maxSub = 0;
    Image is = frame(sharp, 5);
    CHECK(maxSub == 0);
    Image ib = frame(blurred, 5);
    CHECK(maxSub > 0.4 && maxSub <= 0.5);
    CHECK(subframe() == 0);
    // sharp: solid from 200 to 300, nothing partial
    CHECK(red(is, 250, 100) > 245 && red(is, 190, 100) < 10 && partial(is, 100, 0, 600) == 0);
    // blurred: still solid in the middle, ramps 20 px either side of each edge
    CHECK(red(ib, 250, 100) > 245);
    CHECK(red(ib, 175, 100) < 10 && red(ib, 325, 100) < 10);
    const int left = partial(ib, 100, 170, 230), right = partial(ib, 100, 270, 330);
    CHECK(left >= 30 && left <= 42 && right >= 30 && right <= 42);
    CHECK(red(ib, 190, 100) > 40 && red(ib, 190, 100) < 90);    // a quarter of the shutter
    CHECK(red(ib, 210, 100) > 165 && red(ib, 210, 100) < 215);  // three quarters
    // the frame is still evenly bright: the smear keeps the rect's total amount of red
    long long sumS = 0, sumB = 0;
    for (int x = 0; x < 600; x++) { sumS += red(is, x, 100); sumB += red(ib, x, 100); }
    CHECK(std::llabs(sumS - sumB) < 255 * 3);
    UnloadImage(is); UnloadImage(ib);
}

static void layerOnly() {
    Comp c = comp();
    c.h = 400;
    TNode* blurred = addRoot(c.roots, 0, 20);
    blurred->fn = moving;
    blurred->isolate().motionBlur = { 12, 1.0f };
    TNode* plain = addRoot(c.roots, 0, 20);
    plain->props.pos = { 0, 200 };
    plain->fn = moving;
    Image img = frame(c, 5);
    CHECK(partial(img, 100, 150, 350) >= 30);
    CHECK(partial(img, 300, 150, 350) == 0 && red(img, 250, 300) > 245);
    UnloadImage(img);
    // props animated in `up` move per sample as well
    Comp u = comp();
    TNode* n = addRoot(u.roots, 0, 20);
    n->up = [](int t, NodeProps& p) { p.pos.x = (float)(40 * frameTime(t)); };
    n->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 50, 100, 100, PURE); };
    n->isolate().motionBlur = { 12, 1.0f };
    Image iu = frame(u, 5);
    CHECK(partial(iu, 100, 150, 350) >= 30 && red(iu, 250, 100) > 245);
    UnloadImage(iu);
}

static void particles() {
    // a stream drifting right: blurred frames differ, but keep the same overall light
    fx::Emitter e;
    e.rate = 2;
    e.speed = { 30, 30 };
    e.angle = 0;
    e.spread = 0;
    e.life = { 40, 40 };
    e.size = { 6, 6 };
    Comp a = comp(), b = comp();
    for (Comp* c : { &a, &b }) {
        TNode* n = addRoot(c->roots, 0, 20);
        n->props.pos = { 100, 100 };
        n->fn = fx::fn(e);
    }
    b.motionBlur = { 8, 1.0f };
    Image ia = frame(a, 10), ib = frame(b, 10);
    CHECK(!same(ia, ib));
    UnloadImage(ia); UnloadImage(ib);
}

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(200, 200, "raymotion blur");
    if (!IsWindowReady()) return 77;
    CHECK(subframe() == 0 && frameTime(7) == 7);
    still();
    smear();
    layerOnly();
    particles();
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d motion blur check(s) failed\n", failures); return 1; }
    std::printf("motion blur tests passed\n");
    return 0;
}
