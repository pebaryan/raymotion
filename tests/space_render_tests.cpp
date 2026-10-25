// space_render_tests.cpp -- space:: and cam:: drawn and read back: a near face hides a far
// one whichever order they were added in, a surface hides a label behind it, and a
// cam::world node puts canvas points where cam::toFrame says while the HUD stays put.
// Needs OpenGL.
#include "rmtn/raymotion.hpp"
#include <cstdio>
#include <cmath>
#include <cstdlib>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static Image render(Comp& c) {
    c.w = 800; c.h = 500; c.duration = 10; c.background = BLACK;
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, 0, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}
static Image render(std::function<void(const Ctx&, int)> fn) {
    Comp c;
    addRoot(c.roots, 0, 10)->fn = std::move(fn);
    return render(c);
}
static Color px(const Image& img, Vector2 p) { return GetImageColor(img, (int)p.x, (int)p.y); }
static bool near(float a, float b, float eps) { return std::fabs(a - b) <= eps; }
static bool like(Color c, Color want) { return std::abs(c.r - want.r) < 40 && std::abs(c.g - want.g) < 40 && std::abs(c.b - want.b) < 40; }

static void depthOrder() {
    // two squares facing the viewer, the red one nearer: red wins either way round
    const space::View v = space::View::tilted({ 400, 250 }, 60, 0, -90, 0.3f);   // looking along +y, z up
    auto square = [](float y) {
        return std::vector<Vector3>{ { -1, y, -1 }, { 1, y, -1 }, { 1, y, 1 }, { -1, y, 1 } };
    };
    for (int order = 0; order < 2; order++) {
        Image img = render([&](const Ctx& cx, int) {
            space::Scene s(v);
            if (order == 0) { s.face(square(-2), RED, false); s.face(square(2), BLUE, false); }
            else { s.face(square(2), BLUE, false); s.face(square(-2), RED, false); }
            s.draw(cx);
        });
        const Vector3 mid{ 0, -2, 0 };
        CHECK(v.depth(mid) > v.depth({ 0, 2, 0 }));
        CHECK(like(px(img, v.project(mid)), RED));
        UnloadImage(img);
    }
    // a label behind a surface is hidden; in front of it, shown
    for (float y : { 2.f, -2.f }) {
        Image img = render([&](const Ctx& cx, int) {
            space::Scene s(v);
            s.face(square(0), BLUE, false);
            s.label({ 0, y, 0 }, R"(\blacksquare)", 60, WHITE);
            s.draw(cx);
        });
        const Color c = px(img, v.project({ 0, y, 0 }));
        CHECK(y > 0 ? like(c, BLUE) : like(c, WHITE));
        UnloadImage(img);
    }
}

static void cameraWorld() {
    const Vector2 F{ 800, 500 };
    const cam::Shot shot{ { 2000, 1000 }, 3, 0 };
    Comp c;
    cam::world(c.roots, 0, 10, [shot](int) { return shot; }, F)->fn = [](const Ctx& cx, int) {
        vg::fill(cx, vg::Path::circle({ 2010, 1005 }, 2), GREEN);   // a dot 12 px across on screen
        vg::fill(cx, vg::Path::circle({ 100, 100 }, 50), RED);     // far off-screen
    };
    addRoot(c.roots, 0, 10)->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, 40, 40, BLUE); };   // a HUD
    Image img = render(c);
    const Vector2 at = cam::toFrame(shot, { 2010, 1005 }, F);
    CHECK(near(at.x, 430, 0.01f) && near(at.y, 265, 0.01f));
    CHECK(like(px(img, at), GREEN) && like(px(img, { at.x + 9, at.y }), BLACK) && like(px(img, { at.x, at.y - 9 }), BLACK));
    CHECK(like(px(img, { 20, 20 }), BLUE));
    UnloadImage(img);
}

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(200, 200, "raymotion spaces");
    if (!IsWindowReady()) return 77;
    depthOrder();
    cameraWorld();
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d space rendering check(s) failed\n", failures); return 1; }
    std::printf("space rendering tests passed\n");
    return 0;
}
