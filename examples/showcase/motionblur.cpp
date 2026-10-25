// motionblur.cpp -- "motionblur": exact motion blur, averaged from moments within a frame.
//   compare   the same propeller, ball and fountain, sharp on the left and blurred on the right
//   shutter   one fast disc at three shutter angles (90, 180 and 360 degrees)
//   whip      whip pans between cards (props animated in `up`) and a title that flies in
// Everything that should blur reads time as frameTime(t); labels that use t stay sharp.
// The score (120 bpm on Dm-Bb-F-C) is synthesised into out/ on first use.
//   raymotion render motionblur
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <cmath>
#include <filesystem>

namespace rmtn {
namespace {

constexpr int XF = 20;
constexpr int CARDS = 7, GAP = 520;   // the whip strip
const std::string SCORE = "out/motionblur-assets-v1/score.wav";
const Color BG = mkColor(0x0f1218), INK = mkColor(0xf4f1ea), MUTED = mkColor(0x8a90a2), SUN = mkColor(0xffd166),
            SKY = mkColor(0x6ec6ff), ROSE = mkColor(0xff7a90), MINT = mkColor(0x8bd17c);

Font bold() { return fonts::ui(true); }

float ramp(double t, double a, double b, Easing e = ease::inOut(ease::cubic)) {
    return (float)interpolate(t, { a, b }, { 0, 1 }, clamped(e));
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

// ------------------------------------------------------------------ scenes

// A propeller, a ball and a fountain in a 960-wide panel, all moving on frameTime(t).
void toys(const Ctx& cx, int t, const fx::Emitter& fountain) {
    const double ft = frameTime(t);
    const Vector2 hub{ 480, 300 };
    const float spin = (float)(ft * ramp(ft, 0, 60, ease::inOut(ease::quad)) * 24);   // spins up
    for (int b = 0; b < 3; b++) {
        const float a = (spin + b * 120) * DEG2RAD;
        dr::strokeSeg(cx, hub, { hub.x + 170 * std::cos(a), hub.y + 170 * std::sin(a) }, 30, b == 0 ? ROSE : SKY);
    }
    dr::circle(cx, hub, 22, INK);
    const float x = 480 + 330 * (float)std::sin(ft * 0.14);
    const float y = 600 - 120 * (float)std::fabs(std::cos(ft * 0.21));
    dr::circle(cx, { x, y }, 40, SUN);
    Ctx f = cx;
    f.o = mapP(cx, { 480, 880 });
    fx::draw(f, fountain, ft);
}

void compareScene(TNode* n, int len) {
    backdrop(n, len);
    fx::Emitter fountain = fx::fountain(5, MINT);
    fountain.speed = { 14, 20 };
    fountain.gravity = { 0, 0.6f };
    for (int side = 0; side < 2; side++) {
        TNode* p = n->add(0, len);
        p->props.pos = { side * 960.f, 0 };
        p->fn = [fountain](const Ctx& cx, int t) { toys(cx, t, fountain); };
        if (side == 1) p->isolate().motionBlur = { 16, 0.5f };
    }
    n->add(0, len)->fn = [](const Ctx& cx, int) {
        dr::rect(cx, 958, 60, 4, 800, mkColor(0x2a3040));
        dr::text(bold(), cx, "samples = 1", { 480, 70 }, 36, 0, MUTED, 0.5, 0);
        dr::text(bold(), cx, "samples = 16, shutter = 0.5", { 1440, 70 }, 36, 0, INK, 0.5, 0);
    };
    caption(n, len, "Motion blur, exactly", "node->isolate().motionBlur = { 16, 0.5f };   x = interpolate(frameTime(t), ...)");
}

void shutterScene(TNode* n, int len) {
    backdrop(n, len);
    const float shutters[] = { 0.25f, 0.5f, 1.0f };
    const char* names[] = { "shutter 0.25  (90 degrees)", "shutter 0.5  (180 degrees)", "shutter 1  (360 degrees)" };
    const Color colors[] = { SKY, SUN, ROSE };
    for (int r = 0; r < 3; r++) {
        TNode* row = n->add(0, len);
        row->props.pos = { 0, 160.f + r * 230 };
        row->isolate().motionBlur = { 24, shutters[r] };
        const std::string name = names[r];
        const Color col = colors[r];
        row->fn = [name, col](const Ctx& cx, int t) {
            // the label reads t as it is, so it stays sharp inside the blurred layer
            dr::text(bold(), cx, name.c_str(), { 140, 0 }, 32, 0, MUTED, 0, 0);
            dr::rect(cx, 140, 140, 1640, 3, mkColor(0x2a3040));
            const double ft = frameTime(t);
            const float x = 220 + 1480 * (0.5f - 0.5f * (float)std::cos(ft * 0.07));
            dr::circle(cx, { x, 110 }, 34, col);
        };
    }
    caption(n, len, "Shutter angle", "MotionBlur{ samples, shutter }: shutter is in frames, centred on the frame");
}

void whipScene(TNode* n, int len) {
    backdrop(n, len);
    const Color cards[] = { ROSE, SUN, MINT, SKY, mkColor(0xc49bff), ROSE, SUN };
    TNode* strip = n->add(0, len);
    strip->isolate().motionBlur = { 32, 0.5f };   // ~250 px per shutter: more samples, no steps
    strip->up = [](int t, NodeProps& p) {
        // hold, then whip to the next card in 10 frames
        const double ft = frameTime(t);
        double k = 0;
        for (int w = 0; w < 5; w++) k += ramp(ft, 40 + w * 30.0, 50 + w * 30.0, ease::inOut(ease::expo));
        p.pos.x = (float)(960 - 180 - k * GAP);
    };
    strip->fn = [cards](const Ctx& cx, int) {
        for (int i = 0; i < CARDS; i++) {
            dr::roundRect(cx, i * (float)GAP, 260, 360, 440, 28, cards[i]);
            const std::string label = std::to_string(i + 1);
            dr::text(bold(), cx, label.c_str(), { i * (float)GAP + 180, 480 }, 180, 0, BG, 0.5, 0.5);
        }
    };
    // a title flying in from below, letter by letter
    const auto title = txt::later([] {
        TextStyle s;
        s.font = bold();
        s.size = 96;
        s.color = INK;
        return txt::Text::make("blurred in flight", s);
    });
    TNode* fly = n->add(0, len);
    fly->isolate().motionBlur = { 16, 0.5f };
    fly->fn = [title](const Ctx& cx, int t) {
        txt::Reveal r;
        r.offset = { 0, 1000 };                      // from below the frame
        r.duration = 12;
        r.stagger = 1.5f;
        r.fade = 0;
        r.ease = ease::out(ease::expo);
        txt::draw(cx, title(), { 960, 140 }, frameTime(t) - 10, r, { 0.5f, 0.5f });
    };
    caption(n, len, "Moving props and text", "strip->up = [](int t, NodeProps& p) { p.pos.x = f(frameTime(t)); };");
}

// The score, 120 bpm on Dm-Bb-F-C: driving bass, a tight arpeggio, a whoosh-bell per chapter.
void genScore(const std::string& path, double end, const std::vector<double>& chapters) {
    using procgen::Score;
    Score s(end);
    struct Chord { int root; int pad[3]; int arp[4]; };
    const Chord dm{ 38, { 57, 62, 65 }, { 69, 74, 77, 74 } }, bb{ 46, { 58, 62, 65 }, { 70, 74, 77, 74 } },
                f{ 41, { 57, 60, 65 }, { 69, 72, 77, 72 } }, c{ 48, { 55, 60, 64 }, { 67, 72, 76, 72 } };
    const Chord prog[4] = { dm, bb, f, c };
    const double bar = 4 * 60.0 / 120, beat = bar / 4, stop = end - 2.4;
    int i = 0;
    for (double t = 0; t < stop; t += bar, i++) {
        const Chord& ch = prog[i % 4];
        const bool full = t >= chapters[0];
        for (int p : ch.pad) s.pad(t, bar * 0.95, p, 0.06f, full ? 1800 : 1000);
        for (int e = 0; e < 8; e++) s.pluck(t + e * beat / 2, ch.arp[(e * 3) % 4] + 12 * (e % 2), 0.03f, e % 2 ? 0.3f : 0.7f, 8);
        for (int e = 0; e < 8; e++) s.bass(t + e * beat / 2, beat * 0.4, ch.root, 0.16f);
        s.kick(t, 0.42f); s.kick(t + 2 * beat, 0.38f);
        if (full) { s.clap(t + beat, 0.13f); s.clap(t + 3 * beat, 0.13f); for (int h = 0; h < 8; h++) s.hat(t + h * beat / 2, h % 2 ? 0.07f : 0.04f, h % 2 ? 0.35f : 0.65f); }
    }
    for (size_t k = 0; k < chapters.size(); k++) s.bell(chapters[k], 74 + (int)(k % 3) * 3, 0.09f, 0.5f);
    for (int p : dm.pad) { s.pad(stop, 2.4, p, 0.12f, 2000, 0.35f); s.pad(stop, 2.4, p - 12, 0.08f, 1200, 0.65f); }
    s.bass(stop, 2.4, dm.root, 0.22f);
    media::writeWav(path, s.mix(), Score::RATE);
}

} // namespace

void registerMotionBlur(Registry& out) {
    Comp c;
    c.name = "motionblur";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    struct S { int len; void (*make)(TNode*, int); };
    const S scenes[] = { { 270, compareScene }, { 220, shutterScene }, { 240, whipScene } };
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
