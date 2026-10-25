// paths.cpp -- "vector-paths": the vg:: module, one idea per scene.
//   title       a hand-drawn underline draws itself on
//   draw-on     a curve traced by trim(), with a marker at pointAt()/angleAt()
//   strokes     miter, round and bevel joins; marching dashes; dotted lines
//   fills       a self-crossing star by the even-odd and nonzero rules
//   morph       circle, rounded square, star and heart turn into one another
//   svg         an SVG drawing writes itself on, shape by shape
//   motion      a paper plane flies along a path, leaving a trail
//   raymotion render vector-paths
#include "rmtn/raymotion.hpp"

namespace rmtn {
namespace {

constexpr int XF = 20;
const Color BG = mkColor(0x14171f), INK = mkColor(0xf4f1ea), MUTED = mkColor(0x8a90a2);
const Color SKY = mkColor(0x58c4dd), SUN = mkColor(0xffd166), CORAL = mkColor(0xfc6255), MINT = mkColor(0x83c167),
            ORCHID = mkColor(0xc38fd8);

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

float ramp(int t, double a, double b, Easing e = ease::inOut(ease::cubic)) {
    return (float)interpolate(t, { a, b }, { 0, 1 }, clamped(e));
}

void backdrop(TNode* n, int len) {
    n->add(0, len)->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1920, 1080, BG); };
}

void caption(TNode* n, int len, const char* title, const char* code) {
    TNode* card = n->add(12, len - 12);
    card->up = [len](int t, NodeProps& p) {
        const float in = ramp(t, 0, 18, ease::out(ease::cubic)), out = 1 - ramp(t, len - 40.0, len - 22.0, ease::linear);
        p.opacity = in * out;
        p.pos = { 0, 20 * (1 - in) };
    };
    const std::string tt = title, cc = code;
    card->fn = [tt, cc](const Ctx& cx, int) {
        const float w = std::max(MeasureTextEx(mono(), cc.c_str(), 26, 0).x, MeasureTextEx(bold(), tt.c_str(), 44, 0).x) + 72;
        dr::roundRect(cx, 60, 1080 - 56 - 132, w, 132, 16, mkColor(0x222737, 235));
        dr::text(bold(), cx, tt.c_str(), { 96, 1080 - 56 - 112 }, 44, 0, INK, 0, 0);
        dr::text(mono(), cx, cc.c_str(), { 96, 1080 - 56 - 50 }, 26, 0, SUN, 0, 0);
    };
}

void label(const Ctx& cx, const char* s, Vector2 at, Color c = MUTED, float size = 32) {
    dr::text(regular(), cx, s, at, size, 0, c, 0.5, 0.5);
}

// ------------------------------------------------------------------ scenes

void titleScene(TNode* n, int len) {
    backdrop(n, len);
    const vg::Path under = vg::Path::smooth({ { 600, 640 }, { 760, 612 }, { 960, 630 }, { 1160, 606 }, { 1330, 628 } });
    n->add(0, len)->fn = [under](const Ctx& cx, int t) {
        const float in = ramp(t, 0, 24);
        dr::text(bold(), cx, "Vector paths", { 960, 520 - 20 * (1 - in) }, 150, 0, withAlpha(INK, in), 0.5, 0.5);
        vg::Stroke s;
        s.color = SUN;
        s.width = 12;
        vg::stroke(cx, under.trim(0, ramp(t, 16, 56)), s);
        const float sub = ramp(t, 40, 64);
        dr::text(regular(), cx, "Bézier curves, SVG, strokes, fills and morphs", { 960, 720 }, 40, 0, withAlpha(MUTED, sub), 0.5, 0.5);
    };
}

void drawOnScene(TNode* n, int len) {
    backdrop(n, len);
    std::vector<Vector2> pts;
    for (int i = 0; i < 90; i++) {
        const float a = 2 * (float)PI * i / 90;
        pts.push_back({ 960 + 430 * std::sin(3 * a), 470 + 300 * std::sin(2 * a) });
    }
    const vg::Path curve = vg::Path::smooth(pts, true);
    const vg::Path arrow = vg::Path::polyline({ { 22, 0 }, { -12, -14 }, { -6, 0 }, { -12, 14 } }, true);
    n->add(0, len)->fn = [curve, arrow](const Ctx& cx, int t) {
        const float u = ramp(t, 14, 124, ease::inOut(ease::sine));
        vg::stroke(cx, curve, vg::Stroke{ withAlpha(MUTED, 0.25f), 2 });
        vg::Stroke s;
        s.color = SKY;
        s.width = 7;
        vg::stroke(cx, curve.trim(0, u), s);
        const Vector2 p = curve.pointAt(u);
        vg::fill(cx, arrow.rotated(curve.angleAt(u)).translated(p), SUN);
        char buf[48];
        std::snprintf(buf, sizeof buf, "u = %.2f", u);
        dr::text(mono(), cx, buf, { 120, 110 }, 40, 0, INK, 0, 0);
    };
    caption(n, len, "Draw-on", "vg::stroke(cx, curve.trim(0, u), s);   marker at curve.pointAt(u), curve.angleAt(u)");
}

void strokesScene(TNode* n, int len) {
    backdrop(n, len);
    n->add(0, len)->fn = [](const Ctx& cx, int t) {
        const vg::Join joins[3] = { vg::Join::Miter, vg::Join::Round, vg::Join::Bevel };
        const char* names[3] = { "Join::Miter", "Join::Round", "Join::Bevel" };
        const Color cols[3] = { CORAL, SKY, MINT };
        for (int i = 0; i < 3; i++) {
            const float x = 520 + 440.f * i;
            const vg::Path zig = vg::Path::polyline({ { x - 120, 600 }, { x - 40, 330 }, { x + 40, 600 }, { x + 120, 330 } });
            vg::Stroke s;
            s.color = cols[i];
            s.width = 34;
            s.join = joins[i];
            s.cap = vg::Cap::Butt;
            s.miterLimit = 10;
            vg::stroke(cx, zig.trim(0, ramp(t, 8 + 8 * i, 48 + 8 * i)), s);
            label(cx, names[i], { x, 680 }, INK);
        }
        vg::Stroke ants;
        ants.color = withAlpha(INK, 0.55f);
        ants.width = 3;
        ants.dash = { 18, 14 };
        ants.dashOffset = -1.5f * t;                    // marching
        vg::stroke(cx, vg::Path::rect({ 240, 250, 1440, 480 }, 28), ants);
        vg::Stroke dots;
        dots.color = SUN;
        dots.width = 12;
        dots.dash = { 0, 26 };                          // zero-length dashes with round caps: dots
        vg::stroke(cx, vg::Path::line({ 560, 800 }, { 1360, 800 }).trim(0, ramp(t, 40, 90)), dots);
    };
    caption(n, len, "Strokes", "s.join = vg::Join::Bevel;  ants.dash = { 18, 14 };  ants.dashOffset = -1.5f * t;");
}

void fillsScene(TNode* n, int len) {
    backdrop(n, len);
    // The {7/3} star polygon: one contour that crosses itself.
    auto star = [](Vector2 c, float r) {
        std::vector<Vector2> v;
        for (int k = 0; k < 7; k++) {
            const float a = (-90 + 360.f * ((k * 3) % 7) / 7) * (float)PI / 180;
            v.push_back({ c.x + r * std::cos(a), c.y + r * std::sin(a) });
        }
        return vg::Path::polyline(v, true);
    };
    const vg::Path left = star({ 620, 460 }, 250), right = star({ 1300, 460 }, 250);
    n->add(0, len)->fn = [left, right](const Ctx& cx, int t) {
        const float u = ramp(t, 6, 60);
        vg::Stroke edge;
        edge.color = INK;
        edge.width = 4;
        edge.join = vg::Join::Miter;
        edge.miterLimit = 10;
        vg::stroke(cx, left.trim(0, u), edge);
        vg::stroke(cx, right.trim(0, u), edge);
        const float f = ramp(t, 56, 80);
        vg::fill(cx, left, withAlpha(CORAL, 0.85f * f), vg::FillRule::EvenOdd);
        vg::fill(cx, right, withAlpha(SKY, 0.85f * f), vg::FillRule::NonZero);
        label(cx, "FillRule::EvenOdd", { 620, 770 }, INK);
        label(cx, "FillRule::NonZero", { 1300, 770 }, INK);
    };
    caption(n, len, "Fill rules", "vg::fill(cx, star, color, vg::FillRule::EvenOdd);  // holes, letters, SVG icons");
}

void morphScene(TNode* n, int len) {
    backdrop(n, len);
    const Vector2 c{ 960, 450 };
    const vg::Path heart = vg::Path::parse("M0 30 C -60 -10 -45 -72 0 -42 C 45 -72 60 -10 0 30 Z").scaled(5, 5).translated({ c.x, c.y + 70 });
    const std::vector<vg::Path> shapes = { vg::Path::circle(c, 230), vg::Path::rect({ c.x - 210, c.y - 210, 420, 420 }, 48),
                                           vg::Path::star(c, 260, 115, 5), heart, vg::Path::circle(c, 230) };
    const std::vector<Color> cols = { SKY, MINT, SUN, CORAL, SKY };
    std::vector<vg::Morph> morphs;
    for (size_t i = 0; i + 1 < shapes.size(); i++) morphs.emplace_back(shapes[i], shapes[i + 1], 200);
    n->add(0, len)->fn = [morphs, cols](const Ctx& cx, int t) {
        const int step = 36, start = 14;
        const int k = std::clamp((t - start) / step, 0, (int)morphs.size() - 1);
        const float u = ramp(t, start + k * step, start + k * step + 26);
        vg::Style s = vg::mix(vg::Style::filled(withAlpha(cols[k], 0.9f)), vg::Style::filled(withAlpha(cols[k + 1], 0.9f)), u);
        s.stroke = { INK, 5 };
        vg::draw(cx, morphs[k].at(u), s);
    };
    caption(n, len, "Morph", "vg::Morph m(circle, star);  vg::draw(cx, m.at(u), vg::mix(a, b, u));");
}

void svgScene(TNode* n, int len) {
    backdrop(n, len);
    auto art = std::make_shared<vg::Drawing>();         // loaded on first draw (a cache, same every frame)
    n->add(0, len)->fn = [art](const Ctx& cx, int t) {
        if (art->shapes.empty()) *art = vg::Drawing::load("data/landscape.svg");
        vg::write(cx, *art, { 260, 60, 1400, 788 }, ramp(t, 8, 150, ease::linear), 0.55f);
    };
    caption(n, len, "SVG drawings", "vg::write(cx, vg::Drawing::load(\"landscape.svg\"), box, u);");
}

void motionScene(TNode* n, int len) {
    backdrop(n, len);
    const vg::Path route = vg::Path::smooth({ { 150, 760 }, { 430, 360 }, { 780, 640 }, { 1130, 280 }, { 1480, 600 }, { 1780, 240 } });
    const vg::Path plane = vg::Path::polyline({ { 34, 0 }, { -22, -22 }, { -10, 0 }, { -22, 22 } }, true);
    n->add(0, len)->fn = [route, plane](const Ctx& cx, int t) {
        vg::Stroke guide;
        guide.color = withAlpha(MUTED, 0.6f);
        guide.width = 3;
        guide.dash = { 14, 12 };
        vg::stroke(cx, route, guide);
        const float u = ramp(t, 10, 140, ease::inOut(ease::quad));
        vg::Stroke trail;
        trail.color = ORCHID;
        trail.width = 6;
        vg::stroke(cx, route.trim(std::max(0.f, u - 0.35f), u), trail);
        vg::Style body = vg::Style::filled(INK);
        body.stroke = { ORCHID, 3 };
        body.stroke.join = vg::Join::Miter;
        vg::draw(cx, plane.rotated(route.angleAt(u)).translated(route.pointAt(u)), body);
    };
    caption(n, len, "Motion along a path", "plane.rotated(route.angleAt(u)).translated(route.pointAt(u))");
}

} // namespace

void registerPaths(Registry& out) {
    Comp c;
    c.name = "vector-paths";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    struct S { int len; void (*make)(TNode*, int); };
    const S scenes[] = { { 110, titleScene }, { 170, drawOnScene }, { 160, strokesScene }, { 150, fillsScene },
                         { 190, morphScene }, { 200, svgScene }, { 170, motionScene } };
    bool first = true;
    for (const S& s : scenes) {
        if (!first) ts.transition(Transition::fade(XF));
        first = false;
        s.make(ts.add(s.len), s.len);
    }
    c.duration = ts.end();
    out.push_back(std::move(c));
}

} // namespace rmtn
