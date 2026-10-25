// lowerthird.cpp -- "lowerthird": a name tag with no background, made to be laid over
// footage in an editor. The panel is translucent, the accent bar wipes in under motion blur
// and the text reveals and leaves with txt::.
//   raymotion render lowerthird --alpha                  ProRes 4444 with alpha (.mov)
//   raymotion render lowerthird --alpha --format webm    VP9 with alpha (.webm)
#include "rmtn/raymotion.hpp"

namespace rmtn {
namespace {

const Color ACCENT = mkColor(0xffd166), PANEL = mkColor(0x101420, 215), INK = mkColor(0xf6f3ec), SUB = mkColor(0xb9c0d0);

float ramp(double t, double a, double b, Easing e = ease::inOut(ease::cubic)) {
    return (float)interpolate(t, { a, b }, { 0, 1 }, clamped(e));
}

} // namespace

void registerLowerThird(Registry& out) {
    Comp c;
    c.name = "lowerthird";
    c.w = 1920; c.h = 1080; c.fps = 30; c.duration = 180;
    c.background = BLANK;                    // nothing behind the tag (black in mp4 and preview)
    const int leave = 140;
    const float x = 120, y = 800, h = 150;
    const auto texts = txt::later([] {
        TextStyle name;
        name.font = fonts::ui(true);
        name.size = 64;
        name.color = INK;
        TextStyle role = name;
        role.font = fonts::ui(false);
        role.size = 32;
        role.color = SUB;
        return std::make_pair(txt::Text::make("Ada Lovelace", name), txt::Text::make("Mathematician  \xC2\xB7  wrote the first program", role));
    });
    // the panel grows out of the accent bar; the bar whips in, blurred
    TNode* panel = addRoot(c.roots, 0, c.duration);
    panel->fn = [=](const Ctx& cx, int t) {
        const float w = 820 * ramp(frameTime(t), 8, 26, ease::out(ease::expo)) * (1 - ramp(frameTime(t), leave + 8, leave + 26, ease::in(ease::expo)));
        if (w > 1) dr::roundRect(cx, x, y, w, h, 14, PANEL);
    };
    TNode* bar = addRoot(c.roots, 0, c.duration);
    bar->isolate().motionBlur = { 12, 0.5f };
    bar->fn = [=](const Ctx& cx, int t) {
        const double ft = frameTime(t);
        const float in = ramp(ft, 0, 12, ease::out(ease::expo)), away = ramp(ft, leave + 20, leave + 32, ease::in(ease::expo));
        dr::rect(cx, x - 400 * (1 - in) - 600 * away, y, 10, h, withAlpha(ACCENT, in * (1 - away)));
    };
    TNode* words = addRoot(c.roots, 0, c.duration);
    words->fn = [=](const Ctx& cx, int t) {
        const auto& [name, role] = texts();
        txt::Reveal r;
        r.stagger = 1.2f;
        r.duration = 16;
        r.offset = { 0, 24 };
        txt::Reveal gone = r;
        gone.out = true;
        gone.stagger = 0.6f;
        gone.offset = { 0, -16 };
        gone.ease = ease::in(ease::cubic);
        txt::Reveal w = r;
        w.by = txt::Unit::Word;
        w.stagger = 4;
        if (t < leave) {
            txt::draw(cx, name, { x + 44, y + 22 }, t - 16, r);
            txt::draw(cx, role, { x + 46, y + 98 }, t - 30, w);
        } else {
            txt::draw(cx, name, { x + 44, y + 22 }, t - leave, gone);
            txt::draw(cx, role, { x + 46, y + 98 }, t - leave, gone);
        }
    };
    out.push_back(std::move(c));
}

} // namespace rmtn
