// icon_tests.cpp -- social icons and the raymotion logo: glyph data and lookups, the
// prefabs and their props errors (no window), then rendering: badge colours, Instagram's
// gradient, glyph and mono looks, the logo, entrances and determinism (GL).
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <stdexcept>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

template <class F> static bool throws(F f) {
    try { f(); } catch (const std::runtime_error&) { return true; }
    return false;
}

static void data() {
    CHECK(icons::brands().size() == 8);
    for (const std::string& b : icons::brands()) {
        const vg::Path& g = icons::glyph(b);
        const Rectangle r = g.bounds();
        CHECK(!g.empty());
        CHECK(r.x >= -12.01f && r.y >= -12.01f && r.x + r.width <= 12.01f && r.y + r.height <= 12.01f);
        CHECK(std::max(r.width, r.height) > 20);            // each mark fills its box
    }
    CHECK(icons::title("youtube") == "YouTube" && icons::title("github") == "GitHub");
    CHECK(icons::title("twitch") == "Twitch");
    const Color red = icons::brandColor("youtube");
    CHECK(red.r == 255 && red.g == 0 && red.b == 0);
    CHECK(throws([] { icons::glyph("myspace"); }));
    CHECK(icons::parseStyle("outline") == icons::Style::Outline && icons::parseShape("circle") == icons::Shape::Circle);
    CHECK(throws([] { icons::parseStyle("shiny"); }) && throws([] { icons::parseShape("hexagon"); }));
}

static void prefabs() {
    icons::registerPrefabs();
    icons::registerPrefabs();                                // twice is fine
    const auto names = prefab::names();
    auto has = [&](const std::string& n) { return std::find(names.begin(), names.end(), n) != names.end(); };
    for (const std::string& b : icons::brands()) CHECK(has("social-" + b));
    CHECK(has("social-row") && has("raymotion-logo"));
    CHECK(has("social-twitch"));
    flags::registerPrefabs();
    CHECK(flags::countries().size() == 29);
    const auto flagNames = prefab::names();
    for (const std::string& code : flags::countries())
        CHECK(std::find(flagNames.begin(), flagNames.end(), "flag-" + code) != flagNames.end());
    CHECK(std::find(flagNames.begin(), flagNames.end(), "flag-row") != flagNames.end());
    CHECK(throws([] { flags::title("zz"); }));
    Nodes roots;
    CHECK(throws([&] { prefab::use(roots, 0, 30, "social-x", Json::parse(R"({"in": "wobble"})")); }));
    CHECK(throws([&] { prefab::use(roots, 0, 30, "social-x", Json::parse(R"({"style": "shiny"})")); }));
    CHECK(throws([&] { prefab::use(roots, 0, 30, "social-row", Json::parse(R"({"brands": ["x", "myspace"]})")); }));
    CHECK(throws([&] { prefab::use(roots, 0, 30, "raymotion-logo", Json::parse(R"({"layout": "diagonal"})")); }));
    CHECK(throws([&] { prefab::use(roots, 0, 30, "flag-row", Json::parse(R"({"countries": ["at", "zz"]})")); }));
}

// ---------------------------------------------------------------- rendering

struct Frame {
    Image img{};
    Color at(int x, int y) const { return GetImageColor(img, x, y); }
    int count(const std::function<bool(Color)>& f) const {
        int n = 0;
        for (int y = 0; y < img.height; y++)
            for (int x = 0; x < img.width; x++) n += f(at(x, y));
        return n;
    }
};

static const Color BG = { 20, 24, 40, 255 };

static Frame render(const std::function<void(const Ctx&, int)>& fn, int frame = 0, int w = 400, int h = 400) {
    Comp c;
    c.w = w; c.h = h; c.fps = 30; c.duration = 200; c.background = BG;
    addRoot(c.roots, 0, 200)->fn = fn;
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, frame, canvas);
    Frame f;
    f.img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&f.img);
    canvas.unload();
    return f;
}

static Frame renderPrefab(const std::string& name, const Json& props, int frame, int w = 400, int h = 400) {
    Comp c;
    c.w = w; c.h = h; c.fps = 30; c.duration = 200; c.background = BG;
    TNode* n = prefab::use(c.roots, 0, 200, name, props);
    n->props.pos = { w / 2.f, h / 2.f };
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, frame, canvas);
    Frame f;
    f.img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&f.img);
    canvas.unload();
    return f;
}

static bool near(Color a, Color b, int tol = 40) {
    return std::abs(a.r - b.r) <= tol && std::abs(a.g - b.g) <= tol && std::abs(a.b - b.b) <= tol;
}
static bool isBg(Color c) { return near(c, BG, 6); }

static void drawing() {
    // YouTube's badge: red around a white play triangle.
    Frame yt = render([](const Ctx& cx, int) { icons::draw(cx, "youtube", { 200, 200 }, 200); });
    CHECK(near(yt.at(120, 120), { 255, 0, 0, 255 }));
    CHECK(near(yt.at(205, 200), WHITE));
    CHECK(isBg(yt.at(20, 20)) && isBg(yt.at(200, 312)));
    UnloadImage(yt.img);

    Frame twitch = render([](const Ctx& cx, int) { icons::draw(cx, "twitch", {200,200}, 200); });
    CHECK(near(twitch.at(120,120), icons::brandColor("twitch")));
    CHECK(twitch.count([](Color c) { return near(c, WHITE, 25); }) > 1000);
    UnloadImage(twitch.img);

    Frame japan = render([](const Ctx& cx, int) { flags::draw(cx, "jp", {200,200}, 200, false); });
    CHECK(near(japan.at(200,200), mkColor(0xbc002d)));
    CHECK(near(japan.at(120,160), WHITE));
    UnloadImage(japan.img);

    // Instagram's gradient runs warm at the bottom left to violet-blue at the top right.
    Frame ig = render([](const Ctx& cx, int) { icons::draw(cx, "instagram", { 200, 200 }, 200); });
    const Color low = ig.at(118, 282), high = ig.at(282, 118);
    CHECK(low.r > 200 && low.b < 140);
    CHECK(high.b > high.g && high.b > 150);
    UnloadImage(ig.img);

    // Glyph look: dark brands use Look::color; Mono draws any brand in it.
    icons::Look glyph{ icons::Style::Glyph };
    glyph.color = { 255, 255, 0, 255 };
    Frame x = render([glyph](const Ctx& cx, int) { icons::draw(cx, "x", { 200, 200 }, 200, glyph); });
    CHECK(x.count([](Color c) { return near(c, { 255, 255, 0, 255 }, 20); }) > 2000);
    UnloadImage(x.img);
    Frame wa = render([glyph](const Ctx& cx, int) { icons::draw(cx, "whatsapp", { 200, 200 }, 200, glyph); });
    CHECK(wa.count([](Color c) { return near(c, icons::brandColor("whatsapp"), 20); }) > 2000);
    UnloadImage(wa.img);

    // A circle badge leaves the corners empty; reveal 0 draws nothing.
    icons::Look circle;
    circle.shape = icons::Shape::Circle;
    Frame tg = render([circle](const Ctx& cx, int) { icons::draw(cx, "telegram", { 200, 200 }, 200, circle); });
    CHECK(isBg(tg.at(108, 108)) && near(tg.at(200, 120), icons::brandColor("telegram")));
    UnloadImage(tg.img);
    Frame none = render([](const Ctx& cx, int) { icons::draw(cx, "github", { 200, 200 }, 200, {}, 0); });
    CHECK(none.count(isBg) == 400 * 400);
    UnloadImage(none.img);

    // The logo: teal at the top of its ring, the white triangle inside, nothing before it reveals.
    Frame logo = render([](const Ctx& cx, int t) { icons::logo(cx, { 200, 200 }, 300, t); });
    CHECK(near(logo.at(200, 200 - (int)(300 * 118 / 400.f)), mkColor(0x6cc4e4), 70));
    CHECK(near(logo.at(205, 200), mkColor(0xf2f4ff)));
    UnloadImage(logo.img);
}

static void entrances() {
    Frame flag0 = renderPrefab("flag-at", Json(), 0);
    CHECK(flag0.count(isBg) == 400 * 400);
    UnloadImage(flag0.img);
    Frame flag60 = renderPrefab("flag-at", Json(), 60);
    CHECK(near(flag60.at(200,200), WHITE));
    CHECK(near(flag60.at(200,175), mkColor(0xed2939)));
    UnloadImage(flag60.img);

    // social-row pops in: nothing at frame 0, every icon by frame 60, the same each time.
    const Json row = Json::parse(R"({"brands": ["x", "youtube", "github", "instagram", "tiktok", "whatsapp", "telegram"], "size": 80, "gap": 20, "handle": "@raymotion"})");
    Frame f0 = renderPrefab("social-row", row, 0, 1000, 300);
    CHECK(f0.count(isBg) == 1000 * 300);
    UnloadImage(f0.img);
    Frame a = renderPrefab("social-row", row, 60, 1000, 300), b = renderPrefab("social-row", row, 60, 1000, 300);
    CHECK(near(a.at(500 - 200 - 30, 150 - 30), { 255, 0, 0, 255 }));     // the second icon, YouTube, at x = 300
    CHECK(near(a.at(500 + 3 * 100 - 10, 150 - 30), icons::brandColor("telegram")));
    CHECK(a.count([](Color c) { return c.r > 200 && c.g > 200 && c.b > 200; }) > 1500);   // marks and handle
    CHECK(std::equal((Color*)a.img.data, (Color*)a.img.data + 1000 * 300, (Color*)b.img.data,
                     [](Color p, Color q) { return p.r == q.r && p.g == q.g && p.b == q.b && p.a == q.a; }));
    UnloadImage(a.img);
    UnloadImage(b.img);

    // raymotion-logo draws on: empty at frame 0, the wordmark in place by frame 60.
    Frame l0 = renderPrefab("raymotion-logo", Json(), 0, 800, 300);
    CHECK(l0.count(isBg) == 800 * 300);
    UnloadImage(l0.img);
    Frame l60 = renderPrefab("raymotion-logo", Json(), 60, 800, 300);
    int right = 0;                                           // ink pixels right of the mark
    for (int y = 0; y < 300; y++)
        for (int x = 400; x < 800; x++) right += near(l60.at(x, y), mkColor(0xf2f4ff), 30);
    CHECK(right > 1500);
    UnloadImage(l60.img);
}

int main() {
    data();
    prefabs();
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 180, "icons");
    if (!IsWindowReady()) {
        std::puts("no window: icon rendering skipped");
        return failures ? 1 : 77;
    }
    drawing();
    entrances();
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d icon check(s) failed\n", failures); return 1; }
    std::puts("icon tests passed");
    return 0;
}
