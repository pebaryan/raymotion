// path_render_tests.cpp -- vg:: drawing read back from the canvas: fill rules and holes,
// translucent strokes without overlap seams, caps, joins, dashes, draw-on trims, SVG
// drawings fitted to a box, and determinism. Needs OpenGL.
#include "rmtn/raymotion.hpp"
#include <cstdio>
#include <cstdlib>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static Image render(Comp& c, int frame = 0, int ss = 1) {
    Canvas canvas;
    canvas.load(c.w, c.h, ss);
    presentFrame(c, frame, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}
static Comp comp(std::function<void(const Ctx&, int)> fn, int w = 200, int h = 200) {
    Comp c;
    c.w = w; c.h = h; c.duration = 10; c.background = BLACK;
    addRoot(c.roots, 0, 10)->fn = std::move(fn);
    return c;
}
static Color px(const Image& img, int x, int y) { return GetImageColor(img, x, y); }
static bool lit(Color c) { return c.r > 200 && c.g > 200 && c.b > 200; }
static bool dark(Color c) { return c.r < 20 && c.g < 20 && c.b < 20; }

static void fillRules() {
    // Outer and inner circles drawn the same way round: nonzero fills the middle, even-odd cuts it.
    const vg::Path same = vg::Path::circle({ 100, 100 }, 80).append(vg::Path::circle({ 100, 100 }, 40));
    for (int ss : { 1, 2 }) {
        Comp nz = comp([same](const Ctx& cx, int) { vg::fill(cx, same, WHITE, vg::FillRule::NonZero); });
        Image a = render(nz, 0, ss);
        CHECK(lit(px(a, 100, 100)) && lit(px(a, 100, 40)) && dark(px(a, 5, 5)));
        UnloadImage(a);
        Comp eo = comp([same](const Ctx& cx, int) { vg::fill(cx, same, WHITE, vg::FillRule::EvenOdd); });
        Image b = render(eo, 0, ss);
        CHECK(dark(px(b, 100, 100)) && lit(px(b, 100, 40)) && dark(px(b, 5, 5)));
        UnloadImage(b);
    }
    // A hole wound the other way is cut under nonzero too (letters, SVG icons).
    const vg::Path hole = vg::Path::circle({ 100, 100 }, 80).append(vg::Path::circle({ 100, 100 }, 40).reversed());
    Comp h = comp([hole](const Ctx& cx, int) { vg::fill(cx, hole, WHITE); });
    Image c = render(h);
    CHECK(dark(px(c, 100, 100)) && lit(px(c, 100, 40)));
    UnloadImage(c);
    // A lone concave contour (fast path): a star's centre is in, the gap between arms is out.
    const vg::Path star = vg::Path::star({ 100, 100 }, 90, 30, 5);
    Comp s = comp([star](const Ctx& cx, int) { vg::fill(cx, star, WHITE); });
    Image d = render(s);
    CHECK(lit(px(d, 100, 100)) && lit(px(d, 100, 20)) && dark(px(d, 100 + 40, 100 - 50)));
    UnloadImage(d);
    // Translucent fill over a self-crossing path is uniform (no double coverage).
    const vg::Path bow = vg::Path::polyline({ { 20, 20 }, { 180, 180 }, { 180, 20 }, { 20, 180 } }, true);
    Comp t = comp([bow](const Ctx& cx, int) { vg::fill(cx, bow, Color{ 255, 255, 255, 128 }); });
    Image e = render(t);
    const Color l = px(e, 40, 100), r = px(e, 160, 100);                // the two lobes wind opposite ways
    CHECK(std::abs(l.r - 128) <= 3 && std::abs(r.r - 128) <= 3 && dark(px(e, 100, 40)));
    UnloadImage(e);
}

static void strokes() {
    // A translucent stroke crossing itself covers the crossing once.
    const vg::Path x = vg::Path::polyline({ { 20, 20 }, { 180, 180 }, { 180, 20 }, { 20, 180 } });
    vg::Stroke s;
    s.color = Color{ 255, 255, 255, 128 };
    s.width = 16;
    Comp c = comp([x, s](const Ctx& cx, int) { vg::stroke(cx, x, s); });
    Image a = render(c);
    const Color cross = px(a, 100, 100), plain = px(a, 60, 60), corner = px(a, 180, 22);
    CHECK(std::abs(cross.r - plain.r) <= 3 && std::abs(plain.r - 128) <= 3 && std::abs(corner.r - 128) <= 3);
    UnloadImage(a);
    // Caps: round reaches past the end, butt stops at it, square reaches as far as round.
    const vg::Path line = vg::Path::line({ 50, 100 }, { 150, 100 });
    for (vg::Cap cap : { vg::Cap::Butt, vg::Cap::Round, vg::Cap::Square }) {
        vg::Stroke st;
        st.width = 30;
        st.cap = cap;
        Comp k = comp([line, st](const Ctx& cx, int) { vg::stroke(cx, line, st); });
        Image b = render(k);
        CHECK(lit(px(b, 100, 110)) && dark(px(b, 100, 120)));
        CHECK(lit(px(b, 157, 100)) == (cap != vg::Cap::Butt));
        CHECK(lit(px(b, 162, 112)) == (cap == vg::Cap::Square));      // the square's corner
        UnloadImage(b);
    }
    // Joins on a right angle: miter fills the outer corner, bevel and round cut it.
    const vg::Path corner2 = vg::Path::polyline({ { 40, 160 }, { 40, 40 }, { 160, 40 } });
    for (vg::Join j : { vg::Join::Miter, vg::Join::Bevel, vg::Join::Round }) {
        vg::Stroke st;
        st.width = 30;
        st.join = j;
        st.cap = vg::Cap::Butt;
        Comp k = comp([corner2, st](const Ctx& cx, int) { vg::stroke(cx, corner2, st); });
        Image b = render(k);
        CHECK(lit(px(b, 27, 27)) == (j == vg::Join::Miter));
        CHECK(lit(px(b, 40, 40)) && lit(px(b, 32, 100)));
        UnloadImage(b);
    }
    // Dashes 20 on, 20 off along a line; the offset shifts them.
    for (float off : { 0.f, 20.f }) {
        vg::Stroke st;
        st.width = 10;
        st.cap = vg::Cap::Butt;
        st.dash = { 20, 20 };
        st.dashOffset = off;
        const vg::Path l2 = vg::Path::line({ 0, 100 }, { 200, 100 });
        Comp k = comp([l2, st](const Ctx& cx, int) { vg::stroke(cx, l2, st); });
        Image b = render(k);
        const bool first = off == 0;
        CHECK(lit(px(b, 10, 100)) == first && lit(px(b, 30, 100)) == !first && lit(px(b, 50, 100)) == first);
        UnloadImage(b);
    }
    // Draw-on: half the line is drawn.
    Comp tr = comp([line](const Ctx& cx, int) { vg::Stroke st; st.width = 10; st.cap = vg::Cap::Butt; vg::stroke(cx, line.trim(0, 0.5f), st); });
    Image d = render(tr);
    CHECK(lit(px(d, 60, 100)) && lit(px(d, 95, 100)) && dark(px(d, 110, 100)));
    UnloadImage(d);
    // Strokes follow the node transform: a scaled node scales the width too.
    Comp sc = comp([](const Ctx& cx, int) {
        Ctx big = cx;
        big.s = 2;
        vg::Stroke st;
        st.width = 10;
        vg::stroke(big, vg::Path::line({ 10, 50 }, { 90, 50 }), st);
    });
    Image e = render(sc);
    CHECK(lit(px(e, 100, 108)) && dark(px(e, 100, 114)));
    UnloadImage(e);
}

static void drawings() {
    const vg::Drawing d = vg::Drawing::parse(R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 50">
      <rect x="0" y="0" width="50" height="50" fill="#ff0000"/>
      <path fill-rule="evenodd" fill="#00ff00" d="M50 0 h50 v50 h-50 z M65 15 h20 v20 h-20 z"/>
    </svg>)svg");
    // Fitted into the whole 200x200 frame: 200x100, centred vertically.
    Comp c = comp([d](const Ctx& cx, int) { vg::draw(cx, d, { 0, 0, 200, 200 }); });
    Image a = render(c);
    const Color red = px(a, 50, 100), green = px(a, 110, 60), hole = px(a, 150, 100), above = px(a, 50, 30);
    CHECK(red.r > 240 && red.g < 15);
    CHECK(green.g > 240 && green.r < 15);
    CHECK(dark(hole) && dark(above));
    UnloadImage(a);
    // write(): nothing at 0, everything at 1.
    Comp w0 = comp([d](const Ctx& cx, int t) { vg::write(cx, d, { 0, 0, 200, 200 }, t / 9.f); });
    Image b = render(w0, 0), e = render(w0, 9);
    CHECK(dark(px(b, 50, 100)) && px(e, 50, 100).r > 240 && px(e, 110, 60).g > 240);
    UnloadImage(b);
    UnloadImage(e);
}

static void determinism() {
    const vg::Path p = vg::Path::parse("M20 180 C 20 20 180 20 180 180 Z M60 120 h80 v40 h-80 z");
    vg::Style st = vg::Style::filled(Color{ 80, 160, 255, 200 });
    st.stroke = { WHITE, 6 };
    st.stroke.dash = { 12, 6 };
    Comp c = comp([p, st](const Ctx& cx, int) { vg::draw(cx, p, st); });
    Image a = render(c, 0, 2), b = render(c, 0, 2);
    bool same = true;
    for (int y = 0; y < 200 && same; y++)
        for (int x = 0; x < 200 && same; x++) {
            const Color u = px(a, x, y), v = px(b, x, y);
            same = u.r == v.r && u.g == v.g && u.b == v.b;
        }
    CHECK(same);
    UnloadImage(a);
    UnloadImage(b);
}

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(200, 200, "raymotion paths");
    if (!IsWindowReady()) return 77;
    fillRules();
    strokes();
    drawings();
    determinism();
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d path rendering check(s) failed\n", failures); return 1; }
    std::printf("path rendering tests passed\n");
    return 0;
}
