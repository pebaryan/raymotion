// layer_tests.cpp -- offscreen layers: group opacity, blend modes, masks, offsets,
// nesting, the wipe transition, text boxes and illustration filters, read back from the canvas.
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static bool close(Color a, Color b, int tol = 2) {
    return std::abs(a.r - b.r) <= tol && std::abs(a.g - b.g) <= tol && std::abs(a.b - b.b) <= tol && std::abs(a.a - b.a) <= tol;
}
static void report(const char* what, Color got, Color want) {
    if (!close(got, want)) std::fprintf(stderr, "  %s: got %d,%d,%d,%d want %d,%d,%d,%d\n", what,
                                        got.r, got.g, got.b, got.a, want.r, want.g, want.b, want.a);
}

// Renders `c` at `frame` and returns the output image (top row first).
static Image render(Comp& c, int frame, int ss = 1) {
    Canvas canvas;
    canvas.load(c.w, c.h, ss);
    presentFrame(c, frame, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}
static Comp comp(Color bg) {
    Comp c;
    c.w = 100; c.h = 100; c.duration = 10; c.background = bg;
    return c;
}
static std::function<void(const Ctx&, int)> rect(float x, float y, float w, float h, Color col) {
    return [=](const Ctx& c, int) { dr::rect(c, x, y, w, h, col); };
}
static bool allOpaque(const Image& img) {
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++)
            if (GetImageColor(img, x, y).a != 255) return false;
    return true;
}
#define EXPECT_PX(img, x, y, want) do { Color got_ = GetImageColor(img, x, y); report(#img " (" #x "," #y ")", got_, want); CHECK(close(got_, want)); } while (0)

static void groupOpacity(int ss) {
    // Two overlapping white squares at 50%: as a layer the overlap matches the rest.
    for (bool layered : { true, false }) {
        Comp c = comp(BLACK);
        TNode* g = addRoot(c.roots, 0, 10);
        if (layered) g->isolate().opacity = 0.5f;
        else g->props.opacity = 0.5f;
        g->add(0, 10)->fn = rect(0, 0, 60, 100, WHITE);
        g->add(0, 10)->fn = rect(40, 0, 60, 100, WHITE);
        Image img = render(c, 0, ss);
        Color single = GetImageColor(img, 10, 50), overlap = GetImageColor(img, 50, 50);
        CHECK(std::abs(single.r - 128) <= 2);
        if (layered) CHECK(std::abs(overlap.r - single.r) <= 1);
        else CHECK(overlap.r > single.r + 40);                // inline: the halves stack
        CHECK(allOpaque(img));
        UnloadImage(img);
    }
}

// Layer content must land where it was drawn at every supersampling level.
static void geometry(int ss) {
    Comp c = comp(BLACK);
    TNode* n = addRoot(c.roots, 0, 10);
    n->isolate().offset = { 10, 20 };
    n->fn = rect(10, 10, 30, 20, WHITE);                       // lands at x 20..50, y 30..50
    Image img = render(c, 0, ss);
    EXPECT_PX(img, 25, 35, WHITE);
    EXPECT_PX(img, 45, 45, WHITE);
    EXPECT_PX(img, 55, 40, BLACK);
    EXPECT_PX(img, 35, 55, BLACK);
    EXPECT_PX(img, 15, 25, BLACK);
    UnloadImage(img);
}

static void blendModes() {
    struct Case { Blend mode; Color bg, fg, want; };
    const Case cases[] = {
        { Blend::Normal,   { 50, 50, 50, 255 },    { 100, 0, 0, 255 },     { 100, 0, 0, 255 } },
        { Blend::Add,      { 50, 50, 50, 255 },    { 100, 0, 0, 255 },     { 150, 50, 50, 255 } },
        { Blend::Multiply, { 200, 200, 200, 255 }, { 128, 255, 0, 255 },   { 100, 200, 0, 255 } },
        { Blend::Screen,   { 100, 100, 100, 255 }, { 100, 100, 100, 255 }, { 161, 161, 161, 255 } },
    };
    for (const Case& k : cases) {
        Comp c = comp(k.bg);
        TNode* n = addRoot(c.roots, 0, 10);
        n->isolate().blend = k.mode;
        n->fn = rect(0, 0, 50, 100, k.fg);
        Image img = render(c, 0);
        EXPECT_PX(img, 25, 50, k.want);
        EXPECT_PX(img, 75, 50, k.bg);                          // outside the layer's content
        CHECK(allOpaque(img));
        UnloadImage(img);
    }
}

static void masks() {
    const Color bg{ 0, 0, 255, 255 };
    // Alpha mask over the left half; inverted shows the right half.
    for (bool invert : { false, true }) {
        Comp c = comp(bg);
        TNode* n = addRoot(c.roots, 0, 10);
        Layer& l = n->isolate();
        l.mask = rect(0, 0, 50, 100, WHITE);
        l.invertMask = invert;
        n->fn = rect(0, 0, 100, 100, RED);
        Image img = render(c, 0);
        EXPECT_PX(img, 20, 50, invert ? bg : RED);
        EXPECT_PX(img, 80, 50, invert ? RED : bg);
        CHECK(allOpaque(img));
        UnloadImage(img);
    }
    // Luma mask: opaque black hides, opaque white shows, mid grey shows half.
    Comp c = comp(BLACK);
    TNode* n = addRoot(c.roots, 0, 10);
    Layer& l = n->isolate();
    l.maskMode = MaskMode::Luma;
    l.mask = [](const Ctx& cx, int) {
        dr::rect(cx, 0, 0, 30, 100, BLACK);
        dr::rect(cx, 30, 0, 40, 100, Color{ 128, 128, 128, 255 });
        dr::rect(cx, 70, 0, 30, 100, WHITE);
    };
    n->fn = rect(0, 0, 100, 100, WHITE);
    Image img = render(c, 0);
    EXPECT_PX(img, 15, 50, BLACK);
    EXPECT_PX(img, 50, 50, (Color{ 128, 128, 128, 255 }));
    EXPECT_PX(img, 85, 50, WHITE);
    UnloadImage(img);
}

static void offsetsAndNesting() {
    Comp c = comp(BLACK);
    TNode* outer = addRoot(c.roots, 0, 10);
    outer->isolate().opacity = 0.5f;
    TNode* inner = outer->add(0, 10);
    Layer& il = inner->isolate();
    il.opacity = 0.5f;
    il.offset = { 50, 0 };
    inner->fn = rect(0, 0, 50, 100, WHITE);                   // lands on the right half
    Image img = render(c, 0);
    EXPECT_PX(img, 25, 50, BLACK);
    EXPECT_PX(img, 75, 50, (Color{ 64, 64, 64, 255 }));
    CHECK(allOpaque(img));
    UnloadImage(img);

    // A layer whose opacity is zero draws nothing, and a disabled layer draws inline.
    Comp z = comp(BLACK);
    TNode* hidden = addRoot(z.roots, 0, 10);
    hidden->isolate().opacity = 0;
    hidden->fn = rect(0, 0, 100, 100, WHITE);
    TNode* off = addRoot(z.roots, 0, 10);
    off->isolate().enabled = false;
    off->layer->blend = Blend::Add;
    off->fn = rect(50, 0, 50, 100, RED);
    img = render(z, 0);
    EXPECT_PX(img, 25, 50, BLACK);
    EXPECT_PX(img, 75, 50, RED);
    UnloadImage(img);
}

// Regression: creating layer targets (cold pool) must not drop what the parent batched.
static void coldPool() {
    shutdownGraphics();                                       // empties the target pool
    Comp c = comp(BLACK);
    TNode* parent = addRoot(c.roots, 0, 10);
    parent->fn = rect(0, 0, 50, 100, WHITE);
    TNode* child = parent->add(0, 10);
    child->isolate().mask = rect(50, 0, 50, 100, WHITE);
    child->fn = rect(0, 0, 100, 100, RED);
    Image img = render(c, 0);
    EXPECT_PX(img, 25, 50, WHITE);
    EXPECT_PX(img, 75, 50, RED);
    UnloadImage(img);
}

static void wipe() {
    Comp c = comp(BLACK);
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    ts.add(20)->fn = rect(0, 0, 100, 100, RED);
    ts.transition(Transition::wipe(10, Direction::FromLeft, ease::linear));
    ts.add(20)->fn = rect(0, 0, 100, 100, GREEN);
    Image img = render(c, 15);                                // halfway: left half revealed
    EXPECT_PX(img, 20, 50, GREEN);
    EXPECT_PX(img, 80, 50, RED);
    UnloadImage(img);
    img = render(c, 21);                                      // after: B alone
    EXPECT_PX(img, 80, 50, GREEN);
    UnloadImage(img);
}

static void textBoxes() {
    TextStyle st;
    st.font = fonts::ui(true);
    st.size = 20;
    auto lines = wrapText("one two three four five six", st, 80);
    CHECK(lines.size() >= 3);
    for (auto& l : lines) CHECK(MeasureTextEx(st.font, l.c_str(), st.size, 0).x <= 80);
    CHECK(wrapText("a\n\nb", st, 500).size() == 3);
    auto broken = wrapText("Supercalifragilistic", st, 40);
    CHECK(broken.size() >= 3);
    std::string joined;
    for (auto& l : broken) joined += l;
    CHECK(joined == "Supercalifragilistic");
    Vector2 m = measureTextBox("one two three four five six", st, 80);
    CHECK(m.x <= 80 && m.y == lines.size() * 25.f);

    // Right-aligned text inks only the right side; lines past the box are dropped.
    Comp c = comp(BLACK);
    int shown = 0;
    addRoot(c.roots, 0, 10)->fn = [&](const Ctx& cx, int) {
        TextStyle s = st;
        s.align = Align::End;
        shown = dr::textBox(cx, "one two three four five six seven", { 0, 0, 100, 60 }, s);
    };
    Image img = render(c, 0);
    CHECK(shown == 2);
    int leftInk = 0, rightInk = 0, belowInk = 0;
    for (int y = 0; y < 100; y++)
        for (int x = 0; x < 100; x++) {
            if (GetImageColor(img, x, y).r < 100) continue;
            if (y >= 60) belowInk++;
            else if (x < 20) leftInk++;
            else rightInk++;
        }
    CHECK(rightInk > 50 && belowInk == 0);
    UnloadImage(img);
    (void)leftInk;
}

// ---- illustration filters
// `content` drawn inside a layer with `filters`, over `bg`.
static Image filtered(std::vector<Filter> filters, std::function<void(const Ctx&, int)> content, Color bg = BLACK, int ss = 1) {
    Comp c = comp(bg);
    TNode* g = addRoot(c.roots, 0, 10);
    g->isolate().filters = std::move(filters);
    g->fn = std::move(content);
    return render(c, 0, ss);
}
static int luma(Color c) { return (c.r * 2126 + c.g * 7152 + c.b * 722) / 10000; }

static void filters() {
    // A horizontal grey ramp, for tone-dependent filters.
    auto ramp = [](const Ctx& c, int) {
        for (int x = 0; x < 100; x++) dr::rect(c, (float)x, 0, 1, 100, Color{ (unsigned char)(x * 255 / 99), (unsigned char)(x * 255 / 99), (unsigned char)(x * 255 / 99), 255 });
    };

    // Posterize: a few brightness steps, and the hue kept.
    Image p = filtered({ Filter::posterize(4) }, ramp);
    std::vector<int> levels;
    for (int x = 0; x < 100; x++) {
        int l = GetImageColor(p, x, 50).r;
        if (std::find(levels.begin(), levels.end(), l) == levels.end()) levels.push_back(l);
    }
    CHECK(levels.size() <= 5);                            // 4 steps (+ black where the ramp is 0)
    UnloadImage(p);
    Image orange = filtered({ Filter::posterize(3) }, rect(0, 0, 100, 100, Color{ 200, 100, 0, 255 }));
    Color o = GetImageColor(orange, 50, 50);
    CHECK(o.g * 2 >= o.r - 4 && o.g * 2 <= o.r + 4 && o.b == 0);   // still orange (r = 2g), only brighter or darker
    UnloadImage(orange);

    // Halftone (mono): pixels are paper or ink, and the share of ink follows the tone.
    Filter ht = Filter::halftone(8, 45);
    ht.ink = BLACK; ht.paper = WHITE;
    Image h = filtered({ ht }, ramp);
    int binary = 0, darkInk = 0, lightInk = 0;
    for (int y = 0; y < 100; y++)
        for (int x = 0; x < 100; x++) {
            int l = luma(GetImageColor(h, x, y));
            binary += l < 40 || l > 215;
            if (x < 25) darkInk += l < 128;
            if (x >= 75) lightInk += l < 128;
        }
    CHECK(binary > 8500);                                 // mostly hard dots, antialiased only at their rims
    CHECK(darkInk > 1600 && lightInk < 600);
    UnloadImage(h);

    // Ink edges: a line around a flat square, nothing inside it or far outside.
    Image e = filtered({ Filter::inkEdges(3, RED) }, rect(30, 30, 40, 40, WHITE));
    EXPECT_PX(e, 50, 50, WHITE);
    EXPECT_PX(e, 5, 5, BLACK);
    CHECK(close(GetImageColor(e, 30, 50), RED, 40) || close(GetImageColor(e, 29, 50), RED, 40));
    UnloadImage(e);

    // Paper: a fixed grain that darkens a little (never lightens), the same every render.
    Image a = filtered({ Filter::paperGrain(0.5f) }, rect(0, 0, 100, 100, WHITE));
    Image b = filtered({ Filter::paperGrain(0.5f) }, rect(0, 0, 100, 100, WHITE));
    CHECK(std::memcmp(a.data, b.data, 100 * 100 * 4) == 0);
    long sum = 0; int lo = 255, hi = 0;
    for (int y = 0; y < 100; y++)
        for (int x = 0; x < 100; x++) { int l = luma(GetImageColor(a, x, y)); sum += l; lo = std::min(lo, l); hi = std::max(hi, l); }
    CHECK(hi - lo > 10);                                  // there is a texture
    CHECK(sum / 10000 > 180 && sum / 10000 < 250);        // but the page stays light
    UnloadImage(a); UnloadImage(b);

    // Watercolour: the wash darkens toward its rim, stays clear far outside, and the
    // layer stays transparent there (the background shows through).
    Image w = filtered({ Filter::watercolor(5, 1.0f, 0.0f) }, rect(20, 20, 60, 60, Color{ 80, 140, 220, 255 }), WHITE);
    EXPECT_PX(w, 3, 3, WHITE);
    CHECK(luma(GetImageColor(w, 23, 50)) < luma(GetImageColor(w, 50, 50)) - 8);
    UnloadImage(w);

    // Filters chain in order and hold at 2x supersampling (sizes are composition px).
    Image one = filtered({ Filter::posterize(4), Filter::inkEdges(3, RED) }, rect(30, 30, 40, 40, WHITE));
    Image two = filtered({ Filter::posterize(4), Filter::inkEdges(3, RED) }, rect(30, 30, 40, 40, WHITE), BLACK, 2);
    // Ink as total "redness" (r - g), which antialiased edge pixels count in part.
    long redOne = 0, redTwo = 0;
    for (int y = 0; y < 100; y++)
        for (int x = 0; x < 100; x++) {
            Color p1 = GetImageColor(one, x, y), p2 = GetImageColor(two, x, y);
            redOne += std::max(0, p1.r - p1.g); redTwo += std::max(0, p2.r - p2.g);
        }
    redOne /= 189; redTwo /= 189;                        // RED's r - g: whole red pixels
    CHECK(redOne > 300 && std::labs(redOne - redTwo) < redOne / 4);
    UnloadImage(one); UnloadImage(two);
}

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(100, 100, "raymotion layers");
    if (!IsWindowReady()) return 77;
    coldPool();
    groupOpacity(1);
    groupOpacity(2);
    geometry(1);
    geometry(2);
    geometry(4);
    blendModes();
    masks();
    offsetsAndNesting();
    wipe();
    textBoxes();
    filters();
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    std::puts("layer tests passed");
    return 0;
}
