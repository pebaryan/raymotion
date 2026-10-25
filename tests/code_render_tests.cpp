// code_render_tests.cpp -- code:: drawn and read back: tokens land in their cells in their
// theme colours, focus dims the other lines, the typewriter stops where it should, and a
// morph starts and ends exactly on its two blocks. Needs OpenGL.
#include "rmtn/raymotion.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static Image render(std::function<void(const Ctx&, int)> fn) {
    Comp c;
    c.w = 800; c.h = 400; c.duration = 10; c.background = BLACK;
    addRoot(c.roots, 0, 10)->fn = std::move(fn);
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, 0, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}
static bool like(Color c, Color want, int tol = 40) { return std::abs(c.r - want.r) < tol && std::abs(c.g - want.g) < tol && std::abs(c.b - want.b) < tol; }
static int count(const Image& img, Rectangle r, Color want) {
    int n = 0;
    for (int y = (int)r.y; y < (int)(r.y + r.height); y++)
        for (int x = (int)r.x; x < (int)(r.x + r.width); x++) n += like(GetImageColor(img, x, y), want);
    return n;
}
static int lit(const Image& img, Rectangle r) {
    int n = 0;
    for (int y = (int)r.y; y < (int)(r.y + r.height); y++)
        for (int x = (int)r.x; x < (int)(r.x + r.width); x++) { const Color c = GetImageColor(img, x, y); n += c.r + c.g + c.b > 200; }
    return n;
}

static code::Style plain() {
    code::Style s;
    s.card = false;
    s.lineNumbers = false;
    s.size = 40;
    s.padding = { 0, 0 };
    return s;
}
// the box of columns [c0, c1) on a line
static Rectangle span(const code::Block& b, const code::Style& s, Vector2 at, int line, int c0, int c1) {
    const Vector2 p = code::cell(b, s, at, line, c0);
    return { p.x, p.y, (c1 - c0) * code::cellWidth(s), s.size * s.lineHeight };
}

static void colours() {
    const code::Block b = code::Block::parse("int n = 42; // hi", "cpp");
    const code::Style s = plain();
    const Vector2 at{ 20, 20 };
    Image img = render([&](const Ctx& cx, int) { code::draw(cx, b, at, s); });
    const code::Theme th = s.theme;
    CHECK(count(img, span(b, s, at, 1, 0, 3), th.of(code::Kind::Type)) > 20);       // int
    CHECK(count(img, span(b, s, at, 1, 8, 10), th.of(code::Kind::Number)) > 20);    // 42
    CHECK(count(img, span(b, s, at, 1, 12, 17), th.of(code::Kind::Comment)) > 20);  // the comment
    CHECK(count(img, span(b, s, at, 1, 0, 3), th.of(code::Kind::Function)) == 0);
    UnloadImage(img);
    // the card and its title bar
    code::Style card = s;
    card.card = true;
    card.title = "a.cpp";
    card.padding = { 30, 20 };
    Image c2 = render([&](const Ctx& cx, int) { code::draw(cx, b, at, card); });
    const Rectangle r = code::bounds(b, card, at);
    CHECK(like(GetImageColor(c2, (int)(r.x + r.width - 8), (int)(r.y + r.height - 20)), th.background, 6));
    CHECK(like(GetImageColor(c2, (int)(r.x + r.width - 30), (int)(r.y + 10)), th.header, 6));
    CHECK(!like(GetImageColor(c2, (int)(r.x + r.width + 8), (int)(r.y + 30)), th.background, 6));
    UnloadImage(c2);
}

static void focusAndReveal() {
    const code::Block b = code::Block::parse("aaaaaa\nbbbbbb\ncccccc", "text");
    const code::Style s = plain();
    const Vector2 at{ 20, 20 };
    code::Look look;
    look.focus = { { 2, 2 } };
    look.dim = 0.2f;
    Image img = render([&](const Ctx& cx, int) { code::draw(cx, b, at, s, look); });
    const int on = lit(img, span(b, s, at, 2, 0, 6)), off = lit(img, span(b, s, at, 1, 0, 6));
    CHECK(on > 50 && off < on / 4);
    UnloadImage(img);
    // halfway through a focus move both lines are partly lit
    look.focusFrom = { { 1, 1 } };
    look.focusMix = 0.5f;
    Image mid = render([&](const Ctx& cx, int) { code::draw(cx, b, at, s, look); });
    const int l1 = lit(mid, span(b, s, at, 1, 0, 6)), l2 = lit(mid, span(b, s, at, 2, 0, 6)), l3 = lit(mid, span(b, s, at, 3, 0, 6));
    CHECK(std::abs(l1 - l2) < l1 / 5 + 5 && l3 < l1 / 2);
    UnloadImage(mid);
    // typewriter: 9 characters = all of line 1 (6), the newline, 2 of line 2
    code::Look typed;
    typed.reveal = 9;
    Image tw = render([&](const Ctx& cx, int) { code::draw(cx, b, at, s, typed); });
    CHECK(lit(tw, span(b, s, at, 1, 0, 6)) > 50);
    CHECK(lit(tw, span(b, s, at, 2, 0, 2)) > 10 && lit(tw, span(b, s, at, 2, 2, 6)) == 0);
    CHECK(lit(tw, span(b, s, at, 3, 0, 6)) == 0);
    UnloadImage(tw);
}

static bool same(const Image& a, const Image& b) {
    int diff = 0;
    for (int y = 0; y < a.height; y++)
        for (int x = 0; x < a.width; x++) diff += !like(GetImageColor(a, x, y), GetImageColor(b, x, y), 3);
    return diff == 0;
}

static void morphEnds() {
    const code::Block a = code::Block::parse("int f(int x) {\n    return x;\n}", "cpp");
    const code::Block b = code::Block::parse("// doubled\nint f(int x) {\n    return 2 * x;\n}", "cpp");
    const code::Morph m(a, b);
    code::Style s;
    s.size = 30;
    s.title = "f.cpp";
    const Vector2 at{ 40, 30 };
    Image da = render([&](const Ctx& cx, int) { code::draw(cx, a, at, s); });
    Image db = render([&](const Ctx& cx, int) { code::draw(cx, b, at, s); });
    Image m0 = render([&](const Ctx& cx, int) { code::draw(cx, m, at, s, 0); });
    Image m1 = render([&](const Ctx& cx, int) { code::draw(cx, m, at, s, 1); });
    CHECK(same(da, m0));
    CHECK(same(db, m1));
    CHECK(!same(da, db));
    UnloadImage(da); UnloadImage(db); UnloadImage(m0); UnloadImage(m1);
}

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(200, 200, "raymotion code");
    if (!IsWindowReady()) return 77;
    colours();
    focusAndReveal();
    morphEnds();
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d code rendering check(s) failed\n", failures); return 1; }
    std::printf("code rendering tests passed\n");
    return 0;
}
