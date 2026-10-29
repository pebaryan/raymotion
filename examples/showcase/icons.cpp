// icons.cpp -- "social-icons": the built-in icon prefabs. The raymotion logo draws itself
// on, a row of social badges pops in, the four looks side by side, handles beside their
// icons, and the logo again, stacked on its badge, as an end card.
#include "rmtn/raymotion.hpp"

namespace rmtn {
namespace {

const Color BG = mkColor(0x0d1020), PANEL = mkColor(0x141a30), INK = mkColor(0xeef0ff), DIM = mkColor(0x8a91b4),
            AQUA = mkColor(0x47dad8);

void caption(TNode* scene, const char* kicker, const char* title) {
    scene->add(0, scene->dur)->fn = [kicker, title](const Ctx& cx, int t) {
        const float k = (float)interpolate(t, { 0, 14 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        Ctx c = cx;
        c.a *= k;
        dr::text(fonts::ui(false), c, kicker, { 120, 96 + 12 * (1 - k) }, 28, 4, AQUA);
        dr::text(fonts::ui(true), c, title, { 120, 136 + 12 * (1 - k) }, 64, 0, INK);
    };
}

TNode* place(TNode* parent, int from, int dur, const char* name, const char* props, Vector2 at) {
    TNode* n = prefab::use(parent, from, dur, name, Json::parse(props));
    n->props.pos = at;
    return n;
}

} // namespace

static Comp makeBadgeSheet() {
    Comp c;
    c.name = "social-badge-sheet";
    c.w = 2400; c.h = 2200; c.fps = 30; c.duration = 1;
    c.background = BG;

    TNode* sheet = addRoot(c.roots, 0, 1);
    const std::vector<std::string> brands = icons::brands();
    sheet->fn = [count = brands.size()](const Ctx& cx, int) {
        dr::text(fonts::ui(true), cx, "SOCIAL BADGES", { 130, 112 }, 76, 1, INK, 0, 0.5);
        const std::string sub = std::to_string(count) + " built-in prefabs";
        dr::text(fonts::ui(false), cx, sub.c_str(), { 2270, 112 }, 34, 1, DIM, 1, 0.5);
    };
    for (size_t i = 0; i < brands.size(); ++i) {
        const int row = (int)i / 5;
        const int col = (int)i % 5;
        const int rowCount = std::min(5, (int)brands.size() - row * 5);
        const float x = 1200.f + (col - (rowCount - 1) / 2.f) * 440.f;
        const float y = 425.f + row * 500.f;
        const std::string brand = brands[i];
        const std::string label = icons::title(brand);
        const std::string id = "social-" + brand;
        sheet->add(0, 1)->fn = [x, y, label, id](const Ctx& cx, int) {
            dr::roundRect(cx, x - 190, y - 195, 380, 390, 28, PANEL);
            dr::text(fonts::ui(true), cx, label.c_str(), { x, y + 105 }, 34, 0, INK, 0.5, 0.5);
            dr::text(fonts::ui(false), cx, id.c_str(), { x, y + 151 }, 25, 0, DIM, 0.5, 0.5);
        };
        TNode* badge = prefab::use(sheet, 0, 1, id, Json::parse(R"({"size": 190, "in": "none"})"));
        badge->props.pos = { x, y - 45 };
    }
    return c;
}

static Comp makeLanguageBadgeSheet() {
    Comp c;
    c.name = "programming-language-badge-sheet";
    c.w = 2400; c.h = 2200; c.fps = 30; c.duration = 1;
    c.background = BG;
    TNode* sheet = addRoot(c.roots, 0, 1);
    const auto& langs = icons::languages();
    sheet->fn = [count = langs.size()](const Ctx& cx, int) {
        dr::text(fonts::ui(true), cx, "PROGRAMMING LANGUAGES", { 130, 112 }, 76, 1, INK, 0, 0.5);
        const std::string sub = std::to_string(count) + " badge prefabs";
        dr::text(fonts::ui(false), cx, sub.c_str(), { 2270, 112 }, 34, 1, DIM, 1, 0.5);
    };
    for (size_t i = 0; i < langs.size(); ++i) {
        const float x = 275.f + (float)(i % 6) * 370.f;
        const float y = 480.f + (float)(i / 6) * 620.f;
        const std::string name = langs[i], title = icons::title(name), id = "language-" + name;
        sheet->add(0, 1)->fn = [x, y, title, id](const Ctx& cx, int) {
            dr::roundRect(cx, x - 165, y - 225, 330, 450, 28, PANEL);
            dr::text(fonts::ui(true), cx, title.c_str(), { x, y + 122 }, 34, 0, INK, 0.5, 0.5);
            dr::text(fonts::ui(false), cx, id.c_str(), { x, y + 172 }, 24, 0, DIM, 0.5, 0.5);
        };
        TNode* badge = prefab::use(sheet, 0, 1, id, Json::parse(R"({"size": 190, "in": "none"})"));
        badge->props.pos = { x, y - 55 };
    }
    return c;
}

void registerIcons(Registry& out) {
    out.push_back(makeBadgeSheet());
    out.push_back(makeLanguageBadgeSheet());
    Comp c;
    c.name = "social-icons";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    auto scene = [&](int dur) {
        TNode* s = ts.add(dur);
        s->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1920, 1080, BG); };
        return s;
    };

    // 1. The logo draws itself on.
    TNode* s1 = scene(120);
    place(s1, 0, 120, "raymotion-logo", R"({"size": 300, "tagline": "VIDEO AS CODE", "delay": 8})", { 960, 540 });

    // 2. Every brand, popping in one after another.
    ts.transition(Transition::fade(16));
    TNode* s2 = scene(120);
    caption(s2, "PREFABS", "social-row");
    place(s2, 10, 110, "social-row", R"({"size": 150, "gap": 48, "handle": "@raymotion", "stagger": 5})", { 960, 560 });

    // 3. The four looks, and the shapes.
    ts.transition(Transition::fade(16));
    TNode* s3 = scene(150);
    caption(s3, "STYLE AND SHAPE", "badge, glyph, mono, outline");
    const char* rows[] = {
        R"({"size": 92, "gap": 54, "style": "badge", "shape": "rounded", "stagger": 3})",
        R"({"size": 92, "gap": 54, "style": "badge", "shape": "circle", "stagger": 3, "delay": 8})",
        R"({"size": 92, "gap": 54, "style": "glyph", "stagger": 3, "in": "draw", "delay": 16})",
        R"({"size": 92, "gap": 54, "style": "mono", "color": "#8a91b4", "stagger": 3, "in": "fade", "delay": 24})",
        R"({"size": 92, "gap": 54, "style": "outline", "shape": "circle", "color": "#47dad8", "stagger": 3, "delay": 32})",
    };
    const char* labels[] = { "badge", "circle", "glyph", "mono", "outline" };
    for (int i = 0; i < 5; i++) {
        const float y = 330 + i * 140.f;
        place(s3, 6, 144, "social-row", rows[i], { 1080, y });
        s3->add(6 + 8 * i, 144 - 8 * i)->fn = [label = labels[i], y](const Ctx& cx, int t) {
            Ctx k = cx;
            k.a *= (float)interpolate(t, { 0, 12 }, { 0, 1 }, clamped());
            dr::text(code::font(false), k, label, { 120, y }, 30, 0, DIM, 0, 0.5);
        };
    }

    // 4. Single icons with handles, drawn on.
    ts.transition(Transition::fade(16));
    TNode* s4 = scene(140);
    caption(s4, "ONE PER BRAND", "social-youtube, social-github, ...");
    const char* singles[][2] = {
        { "social-youtube", R"({"size": 110, "handle": "@raymotion", "in": "draw"})" },
        { "social-github", R"({"size": 110, "handle": "pebaryan/raymotion", "in": "draw", "delay": 10})" },
        { "social-instagram", R"({"size": 110, "handle": "@raymotion.video", "shape": "circle", "delay": 20})" },
        { "social-tiktok", R"({"size": 110, "handle": "@raymotion", "shape": "circle", "delay": 30})" },
    };
    for (int i = 0; i < 4; i++)
        place(s4, 0, 140, singles[i][0], singles[i][1], { i % 2 ? 1380.f : 600.f, i < 2 ? 470.f : 700.f });

    // 5. End card: the logo stacked on its badge.
    ts.transition(Transition::fade(16));
    TNode* s5 = scene(120);
    place(s5, 0, 120, "raymotion-logo",
          R"({"size": 260, "layout": "stacked", "badge": true, "tagline": "github.com/pebaryan/raymotion", "in": "pop", "delay": 6})",
          { 960, 540 });

    c.duration = ts.end();
    out.push_back(std::move(c));
}

} // namespace rmtn
