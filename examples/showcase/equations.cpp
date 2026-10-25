// equations.cpp -- "equations": the math:: typesetter, one idea per scene.
//   title       Euler's identity writes itself on
//   write       the quadratic formula, glyph by glyph
//   gallery     sums, integrals, matrices, cases and roots, laid out by TeX's rules
//   parts       \id{...} names parts: recoloured, boxed and labelled from their placed bounds
//   solve       a quadratic solved step by step; shared symbols glide between steps
//   identity    Pythagoras rearranged, its parts matched by name
// The score (calm 120 bpm on Am-F-C-G) is synthesised into out/ on first use.
//   raymotion render equations
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <filesystem>

namespace rmtn {
namespace {

constexpr int XF = 20;
const std::string SCORE = "out/equations-assets-v1/score.wav";
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

// Formulas are typeset on first draw (the font loads then) and shared by every frame.
struct Tex {
    std::string src;
    math::Options o;
    std::shared_ptr<math::Formula> f = std::make_shared<math::Formula>();
    std::shared_ptr<bool> built = std::make_shared<bool>(false);
    const math::Formula& operator()() const {
        if (!*built) { *f = math::Formula::tex(src, o); *built = true; }
        return *f;
    }
};
Tex tex(const char* s, float size = 72, Color c = INK) {
    Tex t;
    t.src = s;
    t.o.size = size;
    t.o.color = c;
    return t;
}
struct Morph {
    Tex a, b;
    std::shared_ptr<math::Transform> t = std::make_shared<math::Transform>();
    std::shared_ptr<bool> built = std::make_shared<bool>(false);
    const math::Transform& operator()() const {
        if (!*built) { *t = math::Transform(a(), b()); *built = true; }
        return *t;
    }
};

// ------------------------------------------------------------------ scenes

void titleScene(TNode* n, int len) {
    backdrop(n, len);
    const Tex euler = tex(R"(e^{i\pi} + 1 = 0)", 150);
    n->add(0, len)->fn = [euler](const Ctx& cx, int t) {
        const float in = ramp(t, 0, 20);
        dr::text(bold(), cx, "Equations", { 960, 330 - 20 * (1 - in) }, 120, 0, withAlpha(INK, in), 0.5, 0.5);
        math::write(cx, euler(), { 960, 600 }, { 0.5f, 0.5f }, ramp(t, 16, 80, ease::linear));
        const float sub = ramp(t, 60, 84);
        dr::text(regular(), cx, "TeX math, typeset in-engine and drawn as vector paths", { 960, 830 }, 40, 0,
                 withAlpha(MUTED, sub), 0.5, 0.5);
    };
}

void writeScene(TNode* n, int len) {
    backdrop(n, len);
    const Tex q = tex(R"(x = \frac{-b \pm \sqrt{b^2 - 4ac}}{2a})", 130);
    n->add(0, len)->fn = [q](const Ctx& cx, int t) {
        math::write(cx, q(), { 960, 430 }, { 0.5f, 0.5f }, ramp(t, 10, 110, ease::linear));
    };
    caption(n, len, "Write it on", R"(math::write(cx, Formula::tex("x = \frac{-b \pm ..."), at, anchor, u))");
}

void galleryScene(TNode* n, int len) {
    backdrop(n, len);
    const Tex items[] = {
        tex(R"(\sum_{n=1}^{\infty} \frac{1}{n^2} = \frac{\pi^2}{6})", 60),
        tex(R"(\int_{-\infty}^{\infty} e^{-x^2}\,dx = \sqrt{\pi})", 60),
        tex(R"(\begin{pmatrix} \cos\theta & -\sin\theta \\ \sin\theta & \cos\theta \end{pmatrix})", 60),
        tex(R"(|x| = \begin{cases} x & x \geq 0 \\ -x & x < 0 \end{cases})", 60),
        tex(R"(\nabla \times \mathbf{B} = \mu_0 \mathbf{J} + \mu_0 \varepsilon_0 \frac{\partial \mathbf{E}}{\partial t})", 60),
        tex(R"(\binom{n}{k} = \frac{n!}{k!\,(n-k)!} \quad \sqrt[3]{\frac{a}{b}})", 60),
    };
    for (int i = 0; i < 6; i++) {
        const Tex f = items[i];
        const Vector2 at{ i % 2 == 0 ? 500.f : 1420.f, 170.f + (i / 2) * 250 };
        const int start = 8 + i * 12;
        n->add(0, len)->fn = [f, at, start](const Ctx& cx, int t) {
            const float u = ramp(t, start, start + 24.0, ease::out(ease::cubic));
            math::Look look;
            look.alpha = u;
            math::draw(cx, f(), { at.x, at.y + 24 * (1 - u) }, { 0.5f, 0.5f }, look);
        };
    }
    caption(n, len, "Laid out by TeX's rules", "limits, stretchy delimiters, radicals, matrices and cases from the font's MATH table");
}

void partsScene(TNode* n, int len) {
    backdrop(n, len);
    const Tex f = tex(R"(\id{e}{E} = \id{m}{m}\id{c}{c^2})", 200);
    struct Callout { const char* part; const char* text; Color color; int at; };
    const Callout calls[] = { { "e", "energy", SUN, 20 }, { "m", "mass", MINT, 50 }, { "c", "speed of light, squared", SKY, 80 } };
    const Vector2 at{ 960, 420 };
    n->add(0, len)->fn = [f, at](const Ctx& cx, int t) {
        math::Look look;
        const float a = ramp(t, 20, 36), b = ramp(t, 50, 66), c = ramp(t, 80, 96);
        look.colors["e"] = mixC(INK, SUN, a);
        look.colors["m"] = mixC(INK, MINT, b);
        look.colors["c"] = mixC(INK, SKY, c);
        math::draw(cx, f(), at, { 0.5f, 0.5f }, look);
    };
    for (const Callout& k : calls) {
        const Callout call = k;
        n->add(0, len)->fn = [f, at, call](const Ctx& cx, int t) {
            const float u = ramp(t, call.at, call.at + 20.0);
            if (u <= 0) return;
            Rectangle r = math::place(f(), at, { 0.5f, 0.5f }, call.part);
            r = { r.x - 14, r.y - 14, r.width + 28, r.height + 28 };
            vg::Stroke s(call.color, 5);
            vg::stroke(cx, vg::Path::rect(r, 14).trim(0, u), s);
            const bool above = call.part[0] == 'c';     // the c label goes over, clear of m's
            const float x = r.x + r.width / 2, from = above ? r.y : r.y + r.height, to = above ? 250.f : 590.f;
            vg::stroke(cx, vg::Path::line({ x, from }, { x, to }).trim(0, u), vg::Stroke(withAlpha(call.color, 0.6f), 3));
            dr::text(regular(), cx, call.text, { x, to + (above ? -30.f : 30.f) }, 40, 0, withAlpha(call.color, u), 0.5, 0.5);
        };
    }
    caption(n, len, "Name the parts", R"(\id{m}{m} ... math::place(f, at, anchor, "m"), Look::colors["m"])");
}

void solveScene(TNode* n, int len) {
    backdrop(n, len);
    const Tex steps[] = {
        tex(R"(x^2 - 5x + 6 = 0)", 110),
        tex(R"((x - 2)(x - 3) = 0)", 110),
        tex(R"(x - 2 = 0 \quad \text{or} \quad x - 3 = 0)", 110),
        tex(R"(x = 2 \quad \text{or} \quad x = 3)", 110),
    };
    const int hold = 34, move = 36;
    for (int i = 0; i < 3; i++) {
        const Morph m{ steps[i], steps[i + 1] };
        const int from = 10 + i * (hold + move) + hold;
        n->add(0, len)->fn = [m, from, hold, move, i](const Ctx& cx, int t) {
            if (i > 0 && t < from - hold) return;      // the previous step is still on screen
            if (t >= from + move && i < 2) return;
            const float u = ramp(t, from, from + move);
            m().draw(cx, { 960, 440 }, { 0.5f, 0.5f }, u);
        };
    }
    caption(n, len, "Transform one into the next", "math::Transform(a, b).draw(cx, at, anchor, u)  -- shared glyphs glide");
}

void identityScene(TNode* n, int len) {
    backdrop(n, len);
    const Tex a = tex(R"(\id{a}{\textcolor{#fc6255}{a^2}} + \id{b}{\textcolor{#58c4dd}{b^2}} = \id{c}{\textcolor{#ffd166}{c^2}})", 120);
    const Tex b = tex(R"(\id{c}{\textcolor{#ffd166}{c}} = \sqrt{\id{a}{\textcolor{#fc6255}{a^2}} + \id{b}{\textcolor{#58c4dd}{b^2}}})", 120);
    const Morph m{ a, b };
    n->add(0, len)->fn = [m](const Ctx& cx, int t) {
        math::Look look;
        look.alpha = ramp(t, 0, 14);
        const float u = ramp(t, 40, 90) - ramp(t, 120, 170);
        m().draw(cx, { 960, 440 }, { 0.5f, 0.5f }, u, look);
    };
    caption(n, len, "Parts travel by name", R"(\id{a}{a^2} + \id{b}{b^2} = \id{c}{c^2}  ->  \id{c}{c} = \sqrt{\id{a}{a^2} + \id{b}{b^2}})");
}

// The score, in seconds: scenes start at 3.67 (write), 8.33 (gallery), 13.33 (parts), 18.33
// (solve) and 25.67 (identity). Glyphs writing on get plucks, gallery entries and callouts
// get bells, and each transform gets a short swell that lands on a bell.
void genScore(const std::string& path) {
    using procgen::Score;
    const double END = 32.4;
    Score s(END);
    struct Chord { int root; int pad[3]; int arp[4]; };
    const Chord am{ 45, { 60, 64, 69 }, { 69, 72, 76, 72 } }, f{ 41, { 60, 65, 69 }, { 65, 69, 72, 77 } },
                c{ 48, { 60, 64, 67 }, { 67, 72, 76, 79 } }, g{ 43, { 59, 62, 67 }, { 67, 71, 74, 79 } };
    const Chord prog[4] = { am, f, c, g };
    // Bars of 2 s; the band joins in layers as the film goes on and thins for the finale.
    for (double t = 0; t < 30; t += 2) {
        const Chord& ch = prog[(int)(t / 2) % 4];
        const bool beat = t >= 14 && t < 26, hats = t >= 8 && t < 26, full = t >= 18 && t < 26;
        for (int p : ch.pad) s.pad(t, 1.9, p, 0.09f, full ? 1700 : 1100);
        if (t >= 4)
            for (int e = 0; e < 8; e++) s.pluck(t + e * 0.25, ch.arp[e % 4], e % 4 == 0 ? 0.07f : 0.045f, e % 2 ? 0.3f : 0.7f);
        for (int e = 0; e < (t >= 8 ? 4 : 1); e++) s.bass(t + e * 0.5, t >= 8 ? 0.4 : 1.9, ch.root, beat ? 0.24f : 0.17f);
        if (beat) for (int b = 0; b < 4; b++) s.kick(t + b * 0.5, full ? 0.55f : 0.42f);
        if (full) { s.clap(t + 0.5, 0.2f); s.clap(t + 1.5, 0.2f); }
        if (hats) for (int h = 0; h < 8; h++) s.hat(t + h * 0.25, h % 2 ? 0.09f : 0.05f, h % 2 ? 0.35f : 0.65f);
    }
    // Title: Euler's identity writes on.
    const int climb[8] = { 72, 74, 76, 79, 81, 84, 86, 88 };
    for (int i = 0; i < 7; i++) s.pluck(0.6 + i * 0.3, climb[i] + 12, 0.06f, 0.3f + 0.06f * i);
    // Chapter bells.
    const double chapters[] = { 3.67, 8.33, 13.33, 18.33, 25.67 };
    for (int i = 0; i < 5; i++) s.bell(chapters[i], 79 + (i % 2) * 5, 0.09f, 0.5f);
    // The quadratic formula writes on.
    for (int i = 0; i < 16; i++) s.pluck(4.05 + i * 0.2, climb[i % 8] + (i >= 8 ? 12 : 0), 0.045f, 0.2f + 0.04f * i);
    // Gallery entries, one bell each.
    for (int i = 0; i < 6; i++) s.bell(8.6 + i * 0.4, climb[i] + 7, 0.06f, i % 2 ? 0.75f : 0.25f);
    // E = mc^2 callouts.
    for (int i = 0; i < 3; i++) s.bell(14.0 + i, 84 + i * 2, 0.08f, 0.3f + 0.2f * i);
    // Solving: three steps.
    for (int i = 0; i < 3; i++) {
        const double from = 19.8 + i * 2.333;
        s.riser(from, from + 1.0, 0.07f);
        s.bell(from + 1.2, 79 + i * 3, 0.08f, 0.5f);
    }
    // Pythagoras rearranged, there and back.
    s.riser(27.0, 28.5, 0.09f);
    s.impact(28.67, 0.18f);
    s.bell(28.67, 88, 0.08f, 0.5f);
    s.riser(29.67, 31.2, 0.07f);
    // Landing on C.
    for (int p : c.pad) { s.pad(30, 2.4, p, 0.12f, 2000, 0.35f); s.pad(30, 2.4, p - 12, 0.08f, 1200, 0.65f); }
    s.bass(30, 2.4, c.root - 12, 0.24f);
    for (int i = 0; i < 4; i++) s.bell(30 + i * 0.4, c.arp[i] + 12, 0.08f, 0.2f + 0.2f * i);
    media::writeWav(path, s.mix(), Score::RATE);           // (mix fades the last 3 s)
}

} // namespace

void registerEquations(Registry& out) {
    Comp c;
    c.name = "equations";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    struct S { int len; void (*make)(TNode*, int); };
    const S scenes[] = { { 130, titleScene }, { 160, writeScene }, { 170, galleryScene }, { 170, partsScene },
                         { 240, solveScene }, { 200, identityScene } };
    bool first = true;
    for (const S& s : scenes) {
        if (!first) ts.transition(Transition::fade(XF));
        first = false;
        s.make(ts.add(s.len), s.len);
    }
    c.duration = ts.end();
    c.assets = [](const Comp&, const Json&) {
        if (std::filesystem::exists(SCORE)) return;
        std::filesystem::create_directories(std::filesystem::path(SCORE).parent_path());
        genScore(SCORE);
    };
    media::audio(addRoot(c.roots, 0, c.duration), SCORE);
    out.push_back(std::move(c));
}

} // namespace rmtn
