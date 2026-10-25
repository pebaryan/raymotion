// A raymotion project: two scenes joined by a transition. Replace the scenes with your own.
//   myvideo list                         compositions in this project
//   myvideo still intro --at 45 --png out/check.png
//   myvideo render intro --draft         half size, to check timing
//   myvideo render intro                 out/intro.mp4
#include "rmtn/raymotion.hpp"

using namespace rmtn;

namespace {

const Color BG = mkColor(0x101420), INK = mkColor(0xf2f4ff), ACCENT = mkColor(0x47dad8);

// Scene 1: a title that springs in and a subtitle that fades in after it.
void titleScene(TNode* scene, int len) {
    scene->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1920, 1080, BG); };
    TNode* title = scene->add(0, len);
    title->props.pos = { 960, 480 };
    title->up = [](int t, NodeProps& p) { p.scale = (float)spring(t, { 30.0 }); };
    title->fn = [](const Ctx& cx, int) {
        dr::text(fonts::ui(true), cx, "Your title", { 0, 0 }, 140, 0, INK, 0.5, 0.5);
    };
    TNode* sub = scene->add(15, len - 15);
    sub->props.pos = { 960, 600 };
    sub->up = [](int t, NodeProps& p) {
        p.opacity = (float)interpolate(t, { 0, 20 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        p.pos.y = 600 + 20 * (1 - p.opacity);
    };
    sub->fn = [](const Ctx& cx, int) {
        dr::text(fonts::ui(false), cx, "a subtitle that explains it", { 0, 0 }, 44, 0, ACCENT, 0.5, 0.5);
    };
}

// Scene 2: three points, one after another, wrapped into boxes.
void pointsScene(TNode* scene, int len) {
    scene->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1920, 1080, mkColor(0x182038)); };
    const char* points[3] = { "First point", "Second point", "Third point" };
    const auto rows = layout::column(layout::inset(layout::fill(1920, 1080), 160), { layout::flex(), layout::flex(), layout::flex() }, 32);
    for (int i = 0; i < 3; i++) {
        TNode* item = scene->add(10 + i * 20, len - 10 - i * 20);
        item->up = [](int t, NodeProps& p) {
            p.opacity = (float)interpolate(t, { 0, 15 }, { 0, 1 }, clamped());
            p.pos.x = 60 * (1 - p.opacity);
        };
        const Rectangle box = rows[(size_t)i];
        const std::string text = points[i];
        item->fn = [box, text](const Ctx& cx, int) {
            dr::roundRect(cx, box.x, box.y, box.width, box.height, 24, Color{ 255, 255, 255, 24 });
            TextStyle style;
            style.font = fonts::ui(true); style.size = 64; style.color = INK;
            style.align = Align::Start; style.valign = Align::Center;
            dr::textBox(cx, text, layout::inset(box, 48), style);
        };
    }
}

void registerComps(Registry& out) {
    Comp c;
    c.name = "intro";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    titleScene(ts.add(90), 90);
    ts.transition(Transition::slide(20, Direction::FromRight));
    pointsScene(ts.add(150), 150);
    c.duration = ts.end();
    out.push_back(std::move(c));
}

} // namespace

int main(int argc, const char** argv) { return runCli(argc, argv, registerComps); }
