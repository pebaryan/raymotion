// npr.cpp -- "npr-styles": one still life drawn in eight illustration styles, then all of
// them at once. Shows the non-photorealistic side of the engine:
//   g3d::Material::shading   Toon, Gooch and Hatch shading (Standard for comparison)
//   g3d::Scene::lines        screen-space ink lines: silhouettes, contours, creases, wobble
//   Layer::filters           watercolour, paper grain, halftone, posterize, ink edges
//   raymotion render npr-styles
#include "rmtn/raymotion.hpp"

namespace rmtn {
namespace {

constexpr int FPS = 30, LEN = 180, XF = 24;
const Color INK = mkColor(0x1c1a26), PAPER = mkColor(0xf6f0e2), CREAM = mkColor(0xfbf6ea);

Font bold() { return fonts::ui(true); }
Font regular() { return fonts::ui(false); }
Font mono() {
    static bool missing = false;
    if (!missing) {
        try { return fonts::load({ "consola.ttf", "CascadiaMono.ttf", "DejaVuSansMono.ttf", "Menlo.ttc" }); }
        catch (const std::exception&) { missing = true; }
    }
    return fonts::ui(false);
}

enum class Style { Standard, Toon, ToonInk, Gooch, Hatch, Watercolor, Comic, Poster };

// The five objects' base colours for the colourful styles.
const Color HUES[5] = { mkColor(0xff6b5a), mkColor(0xffc94a), mkColor(0x3fc1b0), mkColor(0x8a6cf0), mkColor(0x5aa8ff) };
const Color PASTEL[5] = { mkColor(0xf08f80), mkColor(0xf3cf6e), mkColor(0x7cc8b4), mkColor(0xa996e6), mkColor(0x86b8ea) };

// A still life on a plinth: knot, sphere, box, cone and torus under one key light.
g3d::ScenePtr stillLife(Style style) {
    auto s = std::make_shared<g3d::Scene>();
    auto mat = [&](int i) {
        g3d::Material m;
        m.roughness = 0.35f;
        switch (style) {
        case Style::Standard:
            m.color = HUES[i]; m.metalness = i == 0 ? 0.5f : 0.1f; m.roughness = i == 0 ? 0.25f : 0.45f;
            break;
        case Style::Poster:                                         // smooth gradients posterize into clean bands
            m.color = HUES[i]; m.roughness = 0.6f;
            break;
        case Style::Toon: case Style::ToonInk: case Style::Comic:
            m.color = HUES[i];
            m.shading = g3d::Shading::Toon;
            m.toon.bands = 2;
            m.toon.rim = Color{ 255, 255, 255, 110 };
            break;
        case Style::Watercolor:
            m.color = PASTEL[i];
            m.shading = g3d::Shading::Toon;
            m.toon.bands = 3; m.toon.softness = 0.12f; m.toon.highlight = 0.25f;
            break;
        case Style::Gooch:
            m.color = ColorLerp(WHITE, HUES[i], 0.25f);
            m.shading = g3d::Shading::Gooch;
            break;
        case Style::Hatch:
            m.color = ColorLerp(CREAM, HUES[i], 0.12f);
            m.shading = g3d::Shading::Hatch;
            m.hatch.ink = INK;
            m.hatch.angle = 35.f + i * 9.f;
            break;
        }
        return m;
    };
    g3d::Material ground = mat(0), plinth = mat(0);
    ground.color = style == Style::Hatch ? CREAM : style == Style::Gooch ? mkColor(0xeeeae0) : mkColor(0xe9dcc4);
    ground.metalness = 0; ground.roughness = 0.9f;
    ground.lineWidth = 0;                                           // the ground's far edge is not a line
    plinth.color = style == Style::Hatch ? CREAM : mkColor(0xd8c8ac);
    plinth.metalness = 0; plinth.roughness = 0.8f;
    ground.toon.rim = plinth.toon.rim = BLANK;                      // rim light is for the objects
    ground.toon.highlight = plinth.toon.highlight = 0;
    s->add(g3d::Geometry::plane(60, 60), ground);
    s->add(g3d::Geometry::cylinder(3.4f, 0.6f, 72), plinth)->position = { 0, 0.2f, 0 };   // sunk 0.1 so its base does not z-fight the ground

    g3d::Object3D* knot = s->add(g3d::Geometry::knot(0.9f, 0.3f), mat(0));
    knot->up = [](int t, g3d::Object3D& o) { o.position = { 0, 2.1f, 0 }; o.rotation = { 25, t * 1.2f, 10 }; };
    s->add(g3d::Geometry::sphere(0.7f), mat(1))->position = { -2.1f, 1.2f, 1.2f };
    g3d::Object3D* box = s->add(g3d::Geometry::box(1.1f, 1.1f, 1.1f), mat(2));
    box->position = { 2.0f, 1.05f, -0.9f };
    box->rotation = { 0, 30, 0 };
    s->add(g3d::Geometry::cone(0.6f, 1.4f), mat(3))->position = { 1.6f, 1.2f, 1.7f };
    g3d::Object3D* ring = s->add(g3d::Geometry::torus(0.75f, 0.22f), mat(4));
    ring->position = { -1.6f, 1.25f, -1.6f };
    ring->rotation = { 0, 40, 0 };

    s->lights.push_back(g3d::Light::directional({ -5, 9, 6 }, WHITE, style == Style::Standard ? 1.6f : 1.0f, true));
    s->lights.back().shadowExtent = 7;
    s->lights.back().shadowBias = 0.06f;
    s->skyColor = mkColor(0xc8d8ff);
    s->groundColor = mkColor(0x8a7a6a);
    s->ambient = style == Style::Hatch ? 0.25f : 0.45f;
    if (style == Style::Standard) {
        s->environmentGenerator = g3d::studioEnvironment(mkColor(0x6a7aa8), mkColor(0x3a3f55), mkColor(0x1a1820), 4);
        s->environmentIntensity = 0.8f;
    }

    if (style == Style::ToonInk || style == Style::Comic || style == Style::Gooch || style == Style::Hatch) {
        s->lines.enabled = true;
        s->lines.color = style == Style::Gooch ? mkColor(0x1d2440) : INK;
        s->lines.width = style == Style::Comic ? 4.f : style == Style::Gooch ? 2.f : 3.f;
        if (style == Style::ToonInk || style == Style::Hatch) { s->lines.wobble = 1.6f; s->lines.boilEvery = 3; }
    }
    s->camera.fov = 36;
    s->camera.up = [](int t, g3d::Camera& cam) {
        const float a = 0.75f + t * 0.0035f;
        cam.position = { 10.5f * std::cos(a), 4.6f, 10.5f * std::sin(a) };
        cam.target = { 0, 1.4f, 0 };
    };
    return s;
}

Color backdrop(Style s) {
    switch (s) {
    case Style::Standard: return mkColor(0x1a1d2b);
    case Style::Poster: return mkColor(0x2b3a66);
    case Style::Toon: case Style::ToonInk: return mkColor(0x9fd3f5);
    case Style::Comic: return mkColor(0xffe28a);
    case Style::Gooch: return mkColor(0xf4f2ec);
    default: return PAPER;
    }
}

std::vector<Filter> filtersFor(Style s) {
    switch (s) {
    case Style::Hatch: return { Filter::paperGrain(0.45f) };
    case Style::Watercolor: return { Filter::watercolor(7, 0.8f, 0.6f), Filter::paperGrain(0.5f) };
    case Style::Comic: return { Filter::halftone(7, 45, true) };
    case Style::Poster: return { Filter::posterize(4), Filter::inkEdges(2.5f, INK, 0.12f) };
    default: return {};
    }
}

// The style's backdrop and scene inside `box` (empty = the whole frame), with its filters.
void addStyled(TNode* parent, int len, Style style, Rectangle box = {}) {
    TNode* group = parent->add(0, len);
    const auto filters = filtersFor(style);
    if (!filters.empty()) group->isolate().filters = filters;
    const Color bg = backdrop(style);
    group->add(0, len)->fn = [bg, box](const Ctx& cx, int) {
        if (box.width > 0) dr::rect(cx, box.x, box.y, box.width, box.height, bg);
        else dr::rect(cx, 0, 0, 1920, 1080, bg);
    };
    if (style == Style::Watercolor) {                              // a painted sun: 2D shapes take the wash too
        group->add(0, len)->fn = [box](const Ctx& cx, int) {
            const float k = box.width > 0 ? box.width / 1920 : 1;
            const Vector2 o{ box.x, box.y };
            dr::circle(cx, { o.x + 1500 * k, o.y + 230 * k }, 120 * k, mkColor(0xf6b25a));
        };
    }
    group->add(0, len)->fn = g3d::draw(stillLife(style), box);
}

struct Chapter { Style style; const char* title; const char* note; const char* code; };
const Chapter CHAPTERS[] = {
    { Style::Standard, "Standard", "physically based: the reference", "m.shading = g3d::Shading::Standard;" },
    { Style::Toon, "Toon", "flat light bands, hard highlight, rim light", "m.shading = g3d::Shading::Toon;  m.toon.bands = 2;" },
    { Style::ToonInk, "Toon + ink lines", "silhouettes, contours and creases that boil on threes", "scene->lines.enabled = true;  scene->lines.wobble = 1.6f;" },
    { Style::Gooch, "Gooch", "technical illustration: cool to warm", "m.shading = g3d::Shading::Gooch;" },
    { Style::Hatch, "Hatching", "pen strokes by tone, crossed in shadow, on paper", "m.shading = g3d::Shading::Hatch;  layer.filters = { Filter::paperGrain() };" },
    { Style::Watercolor, "Watercolour", "bleeding edges, darkened rims, pigment granulation", "layer.filters = { Filter::watercolor(7), Filter::paperGrain() };" },
    { Style::Comic, "Comic halftone", "toon and ink, printed in CMYK dots", "layer.filters = { Filter::halftone(7, 45, true) };" },
    { Style::Poster, "Poster", "posterized colour with ink edges", "layer.filters = { Filter::posterize(4), Filter::inkEdges(2.5f) };" },
};
constexpr int NCH = (int)(sizeof(CHAPTERS) / sizeof(CHAPTERS[0]));

// Caption card: style name, a note and the line of code that makes it.
void caption(TNode* n, int len, const Chapter& ch, bool dark) {
    TNode* card = n->add(10, len - 10);
    card->up = [len](int t, NodeProps& p) {
        const float in = (float)interpolate(t, { 0, 18 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        const float out = (float)interpolate(t, { len - 34.0, len - 16.0 }, { 1, 0 }, clamped());
        p.opacity = in * out;
        p.pos = { 0, 24 * (1 - in) };
    };
    const Color fg = dark ? mkColor(0xf2f4ff) : INK, sub = dark ? mkColor(0xa8b0cc) : mkColor(0x5a5468);
    const Color panel = dark ? Color{ 10, 12, 20, 170 } : Color{ 255, 252, 244, 200 };
    card->fn = [ch, fg, sub, panel](const Ctx& cx, int) {
        const float w = std::max(MeasureTextEx(mono(), ch.code, 26, 0).x, MeasureTextEx(bold(), ch.title, 64, 0).x) + 80;
        dr::roundRect(cx, 64, 1080 - 64 - 190, w, 190, 18, panel);
        dr::text(bold(), cx, ch.title, { 104, 1080 - 64 - 158 }, 64, 0, fg, 0, 0);
        dr::text(regular(), cx, ch.note, { 104, 1080 - 64 - 86 }, 28, 0, sub, 0, 0);
        dr::text(mono(), cx, ch.code, { 104, 1080 - 64 - 44 }, 26, 0, fg, 0, 0);
    };
}

void titleScene(TNode* n, int len) {
    addStyled(n, len, Style::ToonInk);
    TNode* t = n->add(8, len - 8);
    t->up = [](int f, NodeProps& p) {
        p.opacity = (float)interpolate(f, { 0, 20 }, { 0, 1 }, clamped());
        p.pos = { 0, 30 * (1 - p.opacity) };
    };
    t->fn = [](const Ctx& cx, int) {
        dr::roundRect(cx, 64, 64, 1020, 250, 22, Color{ 255, 252, 244, 225 });
        dr::text(bold(), cx, "Non-photorealistic", { 104, 96 }, 92, 0, INK, 0, 0);
        dr::text(bold(), cx, "rendering", { 104, 186 }, 92, 0, mkColor(0xe2533f), 0, 0);
        dr::text(regular(), cx, "eight illustration styles, one scene  ·  raymotion", { 108, 276 }, 30, 0, mkColor(0x5a5468), 0, 0);
    };
}

// All styles at once, each in its own viewport.
void gridScene(TNode* n, int len) {
    n->add(0, len)->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1920, 1080, mkColor(0x14131c)); };
    const Style cells[6] = { Style::Toon, Style::ToonInk, Style::Gooch, Style::Hatch, Style::Watercolor, Style::Comic };
    const char* names[6] = { "Toon", "Toon + ink", "Gooch", "Hatching", "Watercolour", "Comic" };
    const float gw = 584, gh = 400, gap = 24, x0 = (1920 - 3 * gw - 2 * gap) / 2, y0 = 120;
    for (int i = 0; i < 6; i++) {
        const Rectangle box{ x0 + (i % 3) * (gw + gap), y0 + (i / 3) * (gh + gap + 44), gw, gh };
        TNode* cell = n->add(i * 6, len - i * 6);
        cell->up = [](int t, NodeProps& p) { p.opacity = (float)interpolate(t, { 0, 14 }, { 0, 1 }, clamped()); };
        addStyled(cell, len, cells[i], box);
        const std::string name = names[i];
        cell->add(0, len)->fn = [box, name](const Ctx& cx, int) {
            dr::text(regular(), cx, name.c_str(), { box.x, box.y + box.height + 10 }, 28, 0, mkColor(0xd8dcef), 0, 0);
        };
    }
    n->add(0, len)->fn = [](const Ctx& cx, int) {
        dr::text(bold(), cx, "Shading  ·  Scene::lines  ·  Layer::filters", { 960, 52 }, 40, 0, mkColor(0xf2f4ff), 0.5, 0);
    };
}

} // namespace

void registerNpr(Registry& out) {
    Comp c;
    c.name = "npr-styles";
    c.w = 1920; c.h = 1080; c.fps = FPS;
    c.background = INK;
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    titleScene(ts.add(150), 150);
    for (int i = 0; i < NCH; i++) {
        ts.transition(i % 2 ? Transition::slide(XF, Direction::FromRight) : Transition::wipe(XF, Direction::FromLeft));
        TNode* s = ts.add(LEN);
        addStyled(s, LEN, CHAPTERS[i].style);
        caption(s, LEN, CHAPTERS[i], CHAPTERS[i].style == Style::Standard || CHAPTERS[i].style == Style::Poster);
    }
    ts.transition(Transition::fade(XF));
    gridScene(ts.add(240), 240);
    c.duration = ts.end();
    out.push_back(std::move(c));
}

} // namespace rmtn
