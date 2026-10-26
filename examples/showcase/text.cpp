// text.cpp -- "text": txt::, text animation helpers.
//   reveals   letter by letter, word by word from the centre, line by line, then out at random
//   typing    a command typed into a terminal, and a headline decoded from scrambled letters
//   styling   a travelling wave and a highlight that walks from word to word
//   morphs    anagrams whose shared letters glide to their new places
// The score (96 bpm on Am-F-C-G) is synthesised into out/ on first use.
//   raymotion render text
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <filesystem>

namespace rmtn {
namespace {

constexpr int XF = 20;
const std::string SCORE = "out/text-assets-v1/score.wav";
const Color BG = mkColor(0x101418), INK = mkColor(0xf4f1ea), MUTED = mkColor(0x8a90a2), SUN = mkColor(0xffd166),
            SKY = mkColor(0x6ec6ff), ROSE = mkColor(0xff7a90);

Font bold() { return fonts::ui(true); }

float ramp(double t, double a, double b, Easing e = ease::inOut(ease::cubic)) {
    return (float)interpolate(t, { a, b }, { 0, 1 }, clamped(e));
}

TextStyle style(float size, Color c = INK, Font f = fonts::ui(true)) {
    TextStyle s;
    s.font = f;
    s.size = size;
    s.color = c;
    return s;
}

void backdrop(TNode* n, int len) {
    n->add(0, len)->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1920, 1080, BG); };
}

void caption(TNode* n, int len, const char* title, const char* api) {
    TNode* card = n->add(12, len - 12);
    card->up = [len](int t, NodeProps& p) {
        const float in = ramp(t, 0, 18, ease::out(ease::cubic)), out = 1 - ramp(t, len - 40.0, len - 22.0, ease::linear);
        p.opacity = in * out;
        p.pos = { 0, 20 * (1 - in) };
    };
    const std::string tt = title, aa = api;
    card->fn = [tt, aa](const Ctx& cx, int) {
        const float w = std::max(MeasureTextEx(code::font(), aa.c_str(), 24, 0).x, MeasureTextEx(bold(), tt.c_str(), 44, 0).x) + 72;
        dr::roundRect(cx, 60, 1080 - 56 - 132, w, 132, 16, mkColor(0x222737, 235));
        dr::text(bold(), cx, tt.c_str(), { 96, 1080 - 56 - 112 }, 44, 0, INK, 0, 0);
        dr::text(code::font(), cx, aa.c_str(), { 96, 1080 - 56 - 50 }, 24, 0, SUN, 0, 0);
    };
}

// Draws a reveal starting at `start`; from `leave` on, the text goes out again with `out`.
void inThenOut(const Ctx& cx, const txt::Text& text, Vector2 at, int t, int start, const txt::Reveal& in,
               int leave, txt::Reveal out, Vector2 anchor) {
    if (t < leave) txt::draw(cx, text, at, t - start, in, anchor);
    else { out.out = true; txt::draw(cx, text, at, t - leave, out, anchor); }
}

// ------------------------------------------------------------------ scenes

void revealsScene(TNode* n, int len) {
    backdrop(n, len);
    struct Texts { txt::Text title, words, lines; };
    const auto texts = txt::later([] {
        TextStyle para = style(40, MUTED, fonts::ui(false));
        para.align = Align::Center;
        return Texts{ txt::Text::make("Every frame is a function of time", style(84)),
                      txt::Text::make("word by word, from the centre out", style(56, SKY)),
                      txt::Text::make("Lay the text out once.\nReveal it by letter, word or line,\nin any order, with any easing.", para) };
    });
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        const auto& [title, words, lines] = texts();
        const Vector2 mid{ 0.5f, 0.5f };
        const int leave = len - 70;
        txt::Reveal a;                                   // letters rise 30 px, two frames apart
        txt::Reveal gone;
        gone.order = txt::Order::Random;
        gone.stagger = 0.6f;
        gone.duration = 16;
        gone.offset = { 0, -24 };
        gone.ease = ease::in(ease::cubic);
        inThenOut(cx, title, { 960, 250 }, t, 10, a, leave, gone, mid);
        txt::Reveal b;
        b.by = txt::Unit::Word;
        b.order = txt::Order::Center;
        b.stagger = 5;
        b.duration = 20;
        b.offset = { 0, 0 };
        b.scale = 0.4f;
        b.ease = ease::out(ease::back());
        inThenOut(cx, words, { 960, 400 }, t, 80, b, leave, gone, mid);
        txt::Reveal c;
        c.by = txt::Unit::Line;
        c.stagger = 12;
        c.duration = 24;
        c.offset = { -60, 0 };
        inThenOut(cx, lines, { 960, 600 }, t, 130, c, leave, gone, mid);
    };
    caption(n, len, "Staggered reveals", "txt::Reveal r; r.by = txt::Unit::Word; r.order = txt::Order::Center;   txt::draw(cx, text, at, t, r)");
}

void typingScene(TNode* n, int len) {
    backdrop(n, len);
    struct Texts { txt::Text cmd, done, head; };
    const auto texts = txt::later([] {
        return Texts{ txt::Text::make("$ raymotion render text --out text.mp4", style(40, INK, code::font())),
                      txt::Text::make("rendered 812 frames in 9.4 s", style(40, mkColor(0x8bd17c), code::font())),
                      txt::Text::make("DECODING THE SIGNAL", style(120, SUN)) };
    });
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        const auto& [cmd, done, head] = texts();
        const float in = ramp(t, 0, 20);
        const Rectangle card{ 960 - 520, 170, 1040, 210 };
        dr::roundRect(cx, card.x, card.y + 20 * (1 - in), card.width, card.height, 18, withAlpha(mkColor(0x1c2230), in));
        for (int k = 0; k < 3; k++) dr::circle(cx, { card.x + 32 + k * 26, card.y + 28 + 20 * (1 - in) }, 8, withAlpha(k == 0 ? ROSE : k == 1 ? SUN : mkColor(0x8bd17c), in));
        const bool typed = t >= 20 + cmd.letters / 0.9;
        txt::typewriter(cx, cmd, { card.x + 40, card.y + 70 }, (t - 20) * 0.9, !typed, t);
        if (typed) txt::typewriter(cx, done, { card.x + 40, card.y + 130 }, (t - 20 - cmd.letters / 0.9 - 10) * 2.5, true, t);
        txt::Reveal r;
        r.order = txt::Order::Random;
        r.stagger = 2.5f;
        r.duration = 26;
        r.offset = { 0, 0 };
        r.fade = 0.6f;
        r.seed = 4;
        txt::scramble(cx, head, { 960, 640 }, t - 100, r, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#%&*+=?", 2, { 0.5f, 0.5f });
    };
    caption(n, len, "Typing and decoding", "txt::typewriter(cx, text, at, chars, caret, t);   txt::scramble(cx, text, at, t, reveal)");
}

void stylingScene(TNode* n, int len) {
    backdrop(n, len);
    struct Texts { txt::Text wave, line; };
    const auto texts = txt::later([] {
        return Texts{ txt::Text::make("riding a travelling wave", style(96, SKY)),
                      txt::Text::make("Any glyph can be styled on its own", style(72)) };
    });
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        const Texts& tx = texts();   // not a structured binding: lambdas below capture these (C++17)
        const txt::Text& wave = tx.wave;
        const txt::Text& line = tx.line;
        const Vector2 mid{ 0.5f, 0.5f };
        txt::Reveal r;
        r.offset = { 0, 60 };
        r.stagger = 1.5f;
        const float k = ramp(t, 0, 30);
        txt::draw(cx, wave, { 960, 290 }, txt::chain({ txt::wave(t, 22 * k, 9, 0.3f),
            [&](const txt::Glyph& g, txt::GlyphLook& l) { l.alpha *= txt::progress(wave, g, t - 5, r);
                                                         l.color = mixC(SKY, ROSE, 0.5f + 0.5f * std::sin(g.index * 0.5f + t * 0.12f)); } }), mid);
        // a highlight that walks over the words, one at a time
        std::vector<txt::Styler> marks;
        for (int w = 0; w < line.words; w++) {
            const double at = 50 + w * 18.0;
            const float u = ramp(t, at, at + 10) * (1 - ramp(t, at + 18, at + 28));
            const auto [first, last] = line.word(w);
            marks.push_back(txt::highlight(first, last, SUN, u, 14, 1.12f));
        }
        txt::Reveal fadeIn;
        fadeIn.by = txt::Unit::Word;
        fadeIn.stagger = 4;
        marks.insert(marks.begin(), [&](const txt::Glyph& g, txt::GlyphLook& l) { const float p = txt::progress(line, g, t - 25, fadeIn); l.alpha *= p; l.offset.y += 30 * (1 - p); });
        txt::draw(cx, line, { 960, 560 }, txt::chain(marks), mid);
    };
    caption(n, len, "Per-glyph styling", "txt::draw(cx, text, at, txt::chain({ txt::wave(t), txt::highlight(first, last, SUN, u, 14) }))");
}

void morphsScene(TNode* n, int len) {
    backdrop(n, len);
    struct Morphs { std::vector<txt::Morph> chain; txt::Morph hello; };
    const auto morphs = txt::later([] {
        const char* words[] = { "listen", "silent", "enlist", "tinsel", "inlets" };
        std::vector<txt::Text> big;
        for (const char* w : words) big.push_back(txt::Text::make(w, style(200, INK)));
        std::vector<txt::Morph> chain;
        for (size_t i = 0; i + 1 < big.size(); i++) chain.emplace_back(big[i], big[i + 1]);
        return Morphs{ chain, txt::Morph(txt::Text::make("Hello, world", style(64, SKY, code::font())),
                                         txt::Text::make("Hello, raymotion", style(64, SKY, code::font()))) };
    });
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        const auto& [chain, hello] = morphs();
        const Vector2 mid{ 0.5f, 0.5f };
        const int hold = 22, move = 30, step = hold + move;
        const float in = ramp(t, 0, 20);
        Ctx c = cx;
        c.a *= in;
        const int k = std::min((int)chain.size() - 1, std::max(0, (t - hold) / step));
        const float u = ramp(t - hold - k * step, 0, move, ease::linear);
        chain[k].draw(c, { 960, 380 }, u, mid);
        const float h = ramp(t, 60, 100, ease::linear) * (1 - ramp(t, 160, 200, ease::linear));
        hello.draw(c, { 960, 640 }, h, mid);
    };
    caption(n, len, "Morphing", "const txt::Morph m(from, to);   m.draw(cx, at, u, anchor)");
}

// The score, 96 bpm on Am-F-C-G: soft pads, a plucked arpeggio, a bell at each chapter.
void genScore(const std::string& path, double end, const std::vector<double>& chapters) {
    using procgen::Score;
    Score s(end);
    struct Chord { int root; int pad[3]; int arp[4]; };
    const Chord am{ 45, { 57, 60, 64 }, { 69, 72, 76, 72 } }, f{ 41, { 57, 60, 65 }, { 69, 72, 77, 72 } },
                c{ 48, { 55, 60, 64 }, { 67, 72, 76, 72 } }, g{ 43, { 55, 59, 62 }, { 67, 71, 74, 71 } };
    const Chord prog[4] = { am, f, c, g };
    const double bar = 4 * 60.0 / 96, beat = bar / 4, stop = end - 2.4;
    int i = 0;
    for (double t = 0; t < stop; t += bar, i++) {
        const Chord& ch = prog[i % 4];
        const bool drums = t >= chapters[0], full = t >= chapters[1];
        for (int p : ch.pad) s.pad(t, bar * 0.95, p, 0.07f, full ? 1600 : 900);
        for (int e = 0; e < 8; e++) s.pluck(t + e * beat / 2, ch.arp[(e * 3) % 4] + (e % 4 ? 0 : 12), e % 4 == 0 ? 0.04f : 0.026f, e % 2 ? 0.3f : 0.7f, 6);
        for (int e = 0; e < 4; e++) s.bass(t + e * beat, beat * 0.85, ch.root, drums ? 0.2f : 0.12f);
        if (drums) { s.kick(t, 0.4f); s.kick(t + 2.5 * beat, 0.32f); }
        if (full) { s.clap(t + beat, 0.12f); s.clap(t + 3 * beat, 0.12f); for (int h = 0; h < 8; h++) s.hat(t + h * beat / 2, h % 2 ? 0.06f : 0.035f, h % 2 ? 0.35f : 0.65f); }
    }
    for (size_t k = 0; k < chapters.size(); k++) s.bell(chapters[k], 76 + (int)(k % 3) * 3, 0.09f, 0.5f);
    for (int p : am.pad) { s.pad(stop, 2.4, p, 0.12f, 2000, 0.35f); s.pad(stop, 2.4, p - 12, 0.08f, 1200, 0.65f); }
    s.bass(stop, 2.4, am.root - 12, 0.22f);
    media::writeWav(path, s.mix(), Score::RATE);
}

} // namespace

void registerText(Registry& out) {
    Comp c;
    c.name = "text";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    struct S { int len; void (*make)(TNode*, int); };
    const S scenes[] = { { 300, revealsScene }, { 230, typingScene }, { 230, stylingScene }, { 260, morphsScene } };
    std::vector<double> chapters;
    bool first = true;
    for (const S& s : scenes) {
        if (!first) { ts.transition(Transition::fade(XF)); chapters.push_back((ts.end() - XF) / 30.0); }
        first = false;
        s.make(ts.add(s.len), s.len);
    }
    c.duration = ts.end();
    const double seconds = c.duration / 30.0;
    c.assets = [seconds, chapters](const Comp&, const Json&) {
        if (std::filesystem::exists(SCORE)) return;
        std::filesystem::create_directories(std::filesystem::path(SCORE).parent_path());
        genScore(SCORE, seconds, chapters);
    };
    media::audio(addRoot(c.roots, 0, c.duration), SCORE);
    out.push_back(std::move(c));
}

} // namespace rmtn
