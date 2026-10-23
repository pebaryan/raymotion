// layer_tests.cpp -- offscreen layers: group opacity, blend modes, masks, offsets,
// nesting, the wipe transition and text boxes, read back from the canvas.
#include "rmtn/raymotion.hpp"
#include <cstdio>
#include <cstdlib>

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
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    std::puts("layer tests passed");
    return 0;
}
