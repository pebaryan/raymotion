// textfx_tests.cpp -- txt:: layout (letters, words, lines, wrapping, alignment), reveal
// timing in every order, and drawing read back: hidden at the start, identical to the
// plain text at the end, typewriter cut-offs, decoded text settling, morph end frames.
// Needs OpenGL (fonts live on the GPU).
#include "rmtn/raymotion.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
static bool near(double a, double b, double eps = 1e-3) { return std::fabs(a - b) <= eps; }

static Image render(std::function<void(const Ctx&, int)> fn) {
    Comp c;
    c.w = 800; c.h = 300; c.duration = 10; c.background = BLACK;
    addRoot(c.roots, 0, 10)->fn = std::move(fn);
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, 0, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}
static int lit(const Image& img, Rectangle r) {
    int n = 0;
    for (int y = std::max(0, (int)r.y); y < std::min(img.height, (int)(r.y + r.height)); y++)
        for (int x = std::max(0, (int)r.x); x < std::min(img.width, (int)(r.x + r.width)); x++) {
            const Color c = GetImageColor(img, x, y);
            n += c.r + c.g + c.b > 200;
        }
    return n;
}
static bool same(const Image& a, const Image& b) {
    for (int y = 0; y < a.height; y++)
        for (int x = 0; x < a.width; x++) {
            const Color p = GetImageColor(a, x, y), q = GetImageColor(b, x, y);
            if (std::abs(p.r - q.r) > 3 || std::abs(p.g - q.g) > 3 || std::abs(p.b - q.b) > 3) return false;
        }
    return true;
}

static TextStyle style(float size = 48) {
    TextStyle s;
    s.font = fonts::ui(true);
    s.size = size;
    s.color = WHITE;
    return s;
}
static Rectangle box(const txt::Text& t, int index, Vector2 at) {
    for (const txt::Glyph& g : t.glyphs)
        if (!g.space && g.index == index) return { at.x + g.pos.x + 2, at.y + g.pos.y, g.width - 4, t.style.size };
    return { 0, 0, 0, 0 };
}

static void layouts() {
    const txt::Text t = txt::Text::make("Hello big world\nagain", style());
    CHECK(t.letters == 18 && t.words == 4 && t.lines == 2);
    CHECK(t.word(1) == std::make_pair(5, 7) && t.word(3) == std::make_pair(13, 17) && t.word(9).first == -1);
    // left to right, then the second line below
    float x = -1;
    for (const txt::Glyph& g : t.glyphs) if (g.line == 0) { CHECK(g.pos.x > x); x = g.pos.x; }
    CHECK(t.glyphs.back().line == 1 && t.glyphs.back().pos.y > t.glyphs.front().pos.y);
    CHECK(near(t.size.y, 2 * 48 * 1.25f));
    // the advances agree with raylib's own measurement of the line
    const Vector2 m = MeasureTextEx(fonts::ui(true), "Hello big world", 48, 0);
    CHECK(near(t.size.x, m.x, 0.5));
    // wrapping and centring
    TextStyle c = style();
    c.align = Align::Center;
    const txt::Text w = txt::Text::make("one two three four five six", c, 300);
    CHECK(w.lines >= 2 && near(w.size.x, 300));
    for (int line = 0; line < w.lines; line++) {
        float lo = 1e9f, hi = -1e9f;
        for (const txt::Glyph& g : w.glyphs) if (g.line == line && !g.space) { lo = std::min(lo, g.pos.x); hi = std::max(hi, g.pos.x + g.width); }
        CHECK(near(lo, 300 - hi, 1.0));   // centred
    }
    CHECK(w.bounds({ 400, 100 }, { 0.5f, 0.5f }).x == 250);
    // later(): made once, on first use, shared by copies
    int made = 0;
    const auto once = txt::later([&] { made++; return txt::Text::make("ab", style()); });
    CHECK(made == 0);
    const auto copy = once;
    CHECK(once().letters == 2 && copy().letters == 2 && made == 1);
}

static void timing() {
    const txt::Text t = txt::Text::make("abcd efgh", style());
    txt::Reveal r;
    r.stagger = 2;
    r.duration = 10;
    r.ease = ease::linear;
    CHECK(near(txt::length(t, r), 7 * 2 + 10));
    auto glyphAt = [&](int index) -> const txt::Glyph& { for (const auto& g : t.glyphs) if (!g.space && g.index == index) return g; return t.glyphs[0]; };
    CHECK(near(txt::progress(t, glyphAt(0), 5, r), 0.5) && near(txt::progress(t, glyphAt(3), 5, r), 0));
    r.order = txt::Order::Backward;
    CHECK(near(txt::progress(t, glyphAt(7), 5, r), 0.5) && near(txt::progress(t, glyphAt(0), 5, r), 0));
    r.order = txt::Order::Center;
    CHECK(txt::progress(t, glyphAt(3), 2, r) > txt::progress(t, glyphAt(0), 2, r));
    r.order = txt::Order::Forward;
    r.by = txt::Unit::Word;
    CHECK(near(txt::length(t, r), 1 * 2 + 10) && near(txt::progress(t, glyphAt(3), 5, r), 0.5) && near(txt::progress(t, glyphAt(4), 5, r), 0.3));
    r.out = true;
    CHECK(near(txt::progress(t, glyphAt(0), 0, r), 1) && near(txt::progress(t, glyphAt(0), 12, r), 0));
    r.out = false;
    r.order = txt::Order::Random;
    float sum = 0;
    for (const auto& g : t.glyphs) if (!g.space) sum += txt::progress(t, g, 1000, r);
    CHECK(near(sum, 8));
}

static void drawing() {
    const txt::Text t = txt::Text::make("Motion", style(64));
    const Vector2 at{ 100, 100 };
    Image plain = render([&](const Ctx& cx, int) { txt::draw(cx, t, at); });
    CHECK(lit(plain, t.bounds(at)) > 200);
    txt::Reveal r;
    Image start = render([&](const Ctx& cx, int) { txt::draw(cx, t, at, 0.0, r); });
    CHECK(lit(start, { 0, 0, 800, 300 }) == 0);
    Image end = render([&](const Ctx& cx, int) { txt::draw(cx, t, at, txt::length(t, r), r); });
    CHECK(same(plain, end));
    Image decoded = render([&](const Ctx& cx, int) { txt::scramble(cx, t, at, txt::length(t, r), r); });
    CHECK(same(plain, decoded));
    // the typewriter: three letters, no more
    Image typed = render([&](const Ctx& cx, int) { txt::typewriter(cx, t, at, 3, false); });
    CHECK(lit(typed, box(t, 2, at)) > 20 && lit(typed, box(t, 3, at)) == 0 && lit(typed, box(t, 5, at)) == 0);
    // a highlight recolours just its glyphs
    Image hl = render([&](const Ctx& cx, int) { txt::draw(cx, t, at, txt::highlight(0, 0, RED)); });
    int red = 0, white = 0;
    const Rectangle b0 = box(t, 0, at), b1 = box(t, 1, at);
    for (int y = (int)b0.y; y < (int)(b0.y + b0.height); y++)
        for (int x = (int)b0.x; x < (int)(b0.x + b0.width); x++) { const Color c = GetImageColor(hl, x, y); red += c.r > 150 && c.g < 80; }
    for (int y = (int)b1.y; y < (int)(b1.y + b1.height); y++)
        for (int x = (int)b1.x; x < (int)(b1.x + b1.width); x++) { const Color c = GetImageColor(hl, x, y); white += c.r > 150 && c.g > 150; }
    CHECK(red > 30 && white > 30);
    for (Image* i : { &plain, &start, &end, &decoded, &typed, &hl }) UnloadImage(*i);
}

static void morphs() {
    const txt::Text a = txt::Text::make("listen", style(60)), b = txt::Text::make("silent", style(60));
    const txt::Morph m(a, b);
    CHECK(m.kept() >= 3);
    const Vector2 at{ 400, 150 }, mid{ 0.5f, 0.5f };
    Image da = render([&](const Ctx& cx, int) { txt::draw(cx, a, at, {}, mid); });
    Image db = render([&](const Ctx& cx, int) { txt::draw(cx, b, at, {}, mid); });
    Image m0 = render([&](const Ctx& cx, int) { m.draw(cx, at, 0, mid); });
    Image m1 = render([&](const Ctx& cx, int) { m.draw(cx, at, 1, mid); });
    CHECK(same(da, m0) && same(db, m1) && !same(da, db));
    for (Image* i : { &da, &db, &m0, &m1 }) UnloadImage(*i);
    const txt::Morph none(txt::Text::make("abc", style()), txt::Text::make("xyz", style()));
    CHECK(none.kept() == 0);
}

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(200, 200, "raymotion text");
    if (!IsWindowReady()) return 77;
    layouts();
    timing();
    drawing();
    morphs();
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d text check(s) failed\n", failures); return 1; }
    std::printf("text animation tests passed\n");
    return 0;
}
