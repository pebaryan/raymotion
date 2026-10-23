// Minimal raymotion project: one composition, rendered through the engine CLI.
//   starter list | preview hello | render hello | still hello --at 45
#include "rmtn/raymotion.hpp"

using namespace rmtn;

static void registerComps(Registry& out) {
    Comp c;
    c.name = "hello"; c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = mkColor(0x101826);

    // A ring that scales in with an overshoot, then spins.
    TNode* ring = addRoot(c.roots, 0, 90);
    ring->props.pos = { c.w / 2.f, c.h / 2.f - 40 };
    ring->up = [](int t, NodeProps& p) {
        TrackFloat s; s.add(0, 0.f); s.add(24, 1.f, E_BACK);
        p.scale = s.at(t);
        p.rot = t * 2.f;
    };
    ring->fn = [](const Ctx& cx, int) {
        dr::ring(cx, { 0, 0 }, 120, 10, mkColor(0x47dad8));
        dr::arc(cx, { 0, 0 }, 150, 6, 0, 120, mkColor(0xffc365));
    };

    // Title in the bundled Inter face, fading up from below.
    TNode* title = addRoot(c.roots, 15, 75);
    title->props.pos = { c.w / 2.f, c.h * 0.78f };
    title->up = [H = c.h](int t, NodeProps& p) {
        TrackFloat o; o.add(0, 0.f); o.add(18, 1.f, E_OUT_CUBIC);
        p.opacity = o.at(t);
        p.pos.y = H * 0.78f + 30 * (1 - o.at(t));
    };
    title->fn = [](const Ctx& cx, int) {
        dr::text(fonts::ui(true), cx, "Hello, raymotion", { 0, 0 }, 64, 0, mkColor(0xf2f4ff), 0.5, 0.5);
    };

    out.push_back(std::move(c));
}

int main(int argc, const char** argv) { return runCli(argc, argv, registerComps); }
