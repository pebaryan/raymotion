// math_render_tests.cpp -- math:: formulas read back from the canvas: ink lands inside the
// placed box, parts take their Look colours, write and transforms end on the drawn
// formula, and frames are deterministic. Needs OpenGL.
#include "rmtn/raymotion.hpp"
#include <cstdio>
#include <cstdlib>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static Image render(std::function<void(const Ctx&, int)> fn, int ss = 1) {
    Comp c;
    c.w = 400; c.h = 200; c.duration = 10; c.background = BLACK;
    addRoot(c.roots, 0, 10)->fn = std::move(fn);
    Canvas canvas;
    canvas.load(c.w, c.h, ss);
    presentFrame(c, 0, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}
struct Ink { int count = 0, x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1; long r = 0, g = 0, b = 0; };
static Ink ink(const Image& img, Rectangle within = { 0, 0, 1e9f, 1e9f }) {
    Ink k;
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++) {
            if (x < within.x || y < within.y || x >= within.x + within.width || y >= within.y + within.height) continue;
            const Color c = GetImageColor(img, x, y);
            if (c.r + c.g + c.b < 120) continue;
            k.count++;
            k.r += c.r; k.g += c.g; k.b += c.b;
            k.x0 = std::min(k.x0, x); k.y0 = std::min(k.y0, y); k.x1 = std::max(k.x1, x); k.y1 = std::max(k.y1, y);
        }
    return k;
}
static bool same(const Image& a, const Image& b) {
    long diff = 0;
    for (int y = 0; y < a.height; y++)
        for (int x = 0; x < a.width; x++) {
            const Color p = GetImageColor(a, x, y), q = GetImageColor(b, x, y);
            diff += std::abs(p.r - q.r) + std::abs(p.g - q.g) + std::abs(p.b - q.b);
        }
    return diff < 2000;
}

static void placement() {
    const math::Formula f = math::Formula::tex(R"(\id{l}{x^2} = \frac{a}{b})", { 48 });
    const Vector2 at{ 200, 100 };
    Image img = render([f, at](const Ctx& cx, int) { math::draw(cx, f, at); });
    const Ink k = ink(img);
    const Rectangle box = math::place(f, at, { 0.5f, 0.5f }, "");
    CHECK(k.count > 500);
    CHECK(k.x0 >= box.x - 3 && k.x1 <= box.x + box.width + 3 && k.y0 >= box.y - 3 && k.y1 <= box.y + box.height + 3);
    // a part's ink box holds its ink
    const Rectangle lb = math::place(f, at, { 0.5f, 0.5f }, "l");
    CHECK(ink(img, lb).count > 50);
    UnloadImage(img);
}

static void colours() {
    const math::Formula f = math::Formula::tex(R"(\id{l}{x} + \textcolor{#00ff00}{y})", { 64 });
    math::Look look;
    look.colors["l"] = Color{ 255, 0, 0, 255 };
    Image img = render([f, look](const Ctx& cx, int) { math::draw(cx, f, { 200, 100 }, { 0.5f, 0.5f }, look); });
    const Rectangle l = math::place(f, { 200, 100 }, { 0.5f, 0.5f }, "l");
    const Ink red = ink(img, l);
    CHECK(red.count > 50 && red.r > red.g * 4);
    const Ink right = ink(img, { l.x + l.width + 70, 0, 400, 200 });
    CHECK(right.count > 50 && right.g > right.r * 4);
    UnloadImage(img);
    // opacity per part
    math::Look hidden;
    hidden.opacity["l"] = 0;
    Image h = render([f, hidden](const Ctx& cx, int) { math::draw(cx, f, { 200, 100 }, { 0.5f, 0.5f }, hidden); });
    CHECK(ink(h, l).count == 0);
    UnloadImage(h);
}

static void animation() {
    const math::Formula a = math::Formula::tex(R"(a^2 + b^2 = c^2)", { 48 }), b = math::Formula::tex(R"(c = \sqrt{a^2 + b^2})", { 48 });
    Image drawn = render([a](const Ctx& cx, int) { math::draw(cx, a, { 200, 100 }); });
    Image none = render([a](const Ctx& cx, int) { math::write(cx, a, { 200, 100 }, { 0.5f, 0.5f }, 0); });
    Image half = render([a](const Ctx& cx, int) { math::write(cx, a, { 200, 100 }, { 0.5f, 0.5f }, 0.5f); });
    Image done = render([a](const Ctx& cx, int) { math::write(cx, a, { 200, 100 }, { 0.5f, 0.5f }, 1); });
    CHECK(ink(none).count == 0);
    const Ink hk = ink(half), dk = ink(drawn);
    CHECK(hk.count > 0 && hk.count < dk.count && hk.x1 < dk.x1 - 20);   // writes left to right
    CHECK(same(done, drawn));
    const math::Transform t(a, b);
    Image t0 = render([t](const Ctx& cx, int) { t.draw(cx, { 200, 100 }, { 0.5f, 0.5f }, 0); });
    Image t1 = render([t](const Ctx& cx, int) { t.draw(cx, { 200, 100 }, { 0.5f, 0.5f }, 1); });
    Image bDrawn = render([b](const Ctx& cx, int) { math::draw(cx, b, { 200, 100 }); });
    CHECK(same(t0, drawn) && same(t1, bDrawn));
    // deterministic mid-transform frames
    Image m1 = render([t](const Ctx& cx, int) { t.draw(cx, { 200, 100 }, { 0.5f, 0.5f }, 0.5f); });
    Image m2 = render([t](const Ctx& cx, int) { t.draw(cx, { 200, 100 }, { 0.5f, 0.5f }, 0.5f); });
    CHECK(ink(m1).count > 100 && same(m1, m2));
    for (Image* i : { &drawn, &none, &half, &done, &t0, &t1, &bDrawn, &m1, &m2 }) UnloadImage(*i);
}

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(200, 200, "raymotion math");
    if (!IsWindowReady()) return 77;
    placement();
    colours();
    animation();
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d math rendering check(s) failed\n", failures); return 1; }
    std::printf("math rendering tests passed\n");
    return 0;
}
