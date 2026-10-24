// Minimal raymotion project: one composition, rendered through the engine CLI.
//   starter list | preview hello | render hello | still hello --at 45 | props promo | prefabs
#include "rmtn/raymotion.hpp"

using namespace rmtn;

// A props-driven composition: three scenes joined by transitions, with springs, layers,
// a mask reveal and a flex/grid layout. Its length follows the props.
//   starter props promo
//   starter render promo --props "{\"title\": \"Launch day\", \"features\": [\"Fast\", \"Small\"]}"
//   starter render promo --props @promo.json
static void buildPromo(Comp& c, const Json& props) {
    const Color accent = props.color("accent", mkColor(0x47dad8));
    const Color ink = mkColor(0xf2f4ff), bg = c.background;
    const std::string title = props.str("title", "raymotion");
    const std::string tagline = props.str("tagline", "");
    std::vector<std::string> features;
    for (const Json& f : props["features"].asArray()) features.push_back(f.asString());
    const int scene = std::max(1, (int)std::lround(props.num("secondsPerScene", 3) * c.fps));
    const float W = (float)c.w, H = (float)c.h;
    const int fps = c.fps;
    const Rectangle frame = layout::inset(layout::fill(W, H), 72);

    c.roots.clear();
    TransitionSeries ts(c.roots, W, H);
    auto backdrop = [](Color col) { return [col](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1e5f, 1e5f, col); }; };

    // 1. Title: a soft additive glow behind a title revealed left to right by a mask.
    TNode* intro = ts.add(scene);
    intro->fn = backdrop(bg);
    TNode* glow = intro->add(0, scene);
    glow->props.pos = layout::center(frame);
    glow->isolate().blend = Blend::Add;
    glow->up = [fps](int t, NodeProps& p) { p.scale = (float)spring(t, { (double)fps }); };
    glow->fn = [accent](const Ctx& cx, int) {
        for (int i = 6; i >= 1; i--) dr::circle(cx, { 0, 0 }, 60.f * i, Fade(accent, 0.05f));
    };
    TNode* heading = intro->add(6, scene - 6);
    Layer& reveal = heading->isolate();
    reveal.mask = [fps, frame](const Ctx& cx, int t) {
        float k = (float)interpolate(t, { 0, 0.6 * fps }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        dr::rect(cx, frame.x, frame.y, frame.width * k, frame.height, WHITE);
    };
    heading->fn = [=](const Ctx& cx, int) {
        TextStyle big;
        big.font = fonts::ui(true); big.size = 112; big.color = ink;
        big.align = Align::Center; big.valign = Align::Center;
        auto rows = layout::column(frame, { layout::flex(3), layout::flex(1) }, 16);
        dr::textBox(cx, title, rows[0], big);
        TextStyle small = big;
        small.font = fonts::ui(false); small.size = 40; small.color = accent; small.valign = Align::Start;
        dr::textBox(cx, tagline, rows[1], small);
    };

    // 2. Features: one card per entry in a grid, springing in one after another. Each
    //    card is an instance of the "feature-card" prefab registered below.
    ts.transition(Transition::slide(18, Direction::FromRight));
    TNode* list = ts.add(scene);
    list->fn = backdrop(mkColor(0x0c1220));
    const int n = std::max(1, (int)features.size());
    const int cols = n <= 3 ? n : 2, gridRows = (n + cols - 1) / cols;
    auto cells = layout::grid(frame, cols, gridRows, 32);
    for (int i = 0; i < (int)features.size(); i++) {
        const Rectangle cell = cells[(size_t)i];
        Json card;
        card.set("text", features[(size_t)i]);
        card.set("delay", 10 + i * 5);
        card.set("w", (double)cell.width);
        card.set("h", (double)cell.height);
        card.set("fps", fps);
        prefab::use(list, 0, scene, "feature-card", std::move(card))->props.pos = layout::center(cell);
    }

    // 3. Outro: the tagline under a ring, wiped in, fading out at the end.
    ts.transition(Transition::wipe(20, Direction::FromLeft));
    TNode* outro = ts.add(scene);
    outro->fn = backdrop(accent);
    TNode* content = outro->add(0, scene);
    content->up = [scene](int t, NodeProps& p) {
        p.opacity = (float)interpolate(t, { scene - 15.0, (double)scene }, { 1, 0 }, clamped());
    };
    content->fn = [=](const Ctx& cx, int t) {
        Vector2 mid = layout::center(frame);
        dr::ring(cx, { mid.x, mid.y - 90 }, 70, 12, bg);
        dr::arc(cx, { mid.x, mid.y - 90 }, 100, 8, t * 4.f, t * 4.f + 140, bg);
        TextStyle st;
        st.font = fonts::ui(true); st.size = 56; st.color = bg; st.align = Align::Center;
        dr::textBox(cx, tagline.empty() ? title : tagline, { frame.x, mid.y + 40, frame.width, 200 }, st);
    };

    c.duration = ts.end();
}

static void registerComps(Registry& out) {
    // A reusable building block: one feature card with a spring entrance, parameterized
    // by props (text, delay, size, colours). Registered once, instantiated by "promo"
    // once per feature, and listed by `starter prefabs`.
    prefab::add({
        "feature-card",
        Json::parse(R"({
            "text": "", "delay": 0, "fps": 30, "w": 400, "h": 200,
            "panel": "#1b2638", "ink": "#f2f4ff", "accent": "#47dad8"
        })"),
        [](TNode& card, const Json& p) {
            const float w = (float)p.num("w", 400), h = (float)p.num("h", 200);
            const int delay = p.integer("delay", 0), fps = p.integer("fps", 30);
            const Color panel = p.color("panel", mkColor(0x1b2638));
            const Color ink = p.color("ink", mkColor(0xf2f4ff));
            const Color accent = p.color("accent", mkColor(0x47dad8));
            card.up = [fps, delay](int t, NodeProps& pr) {
                SpringOptions o;
                o.fps = fps; o.delay = delay; o.config.damping = 12;
                double k = spring(t, o);
                pr.scale = (float)k;
                pr.opacity = (float)std::min(1.0, k);
            };
            card.fn = [w, h, panel, ink, accent, text = p.str("text", "")](const Ctx& cx, int) {
                dr::roundRect(cx, -w / 2, -h / 2, w, h, 24, panel);
                dr::rect(cx, -w / 2, -h / 2, 10, h, accent);
                TextStyle st;
                st.font = fonts::ui(true); st.size = 44; st.color = ink;
                st.align = Align::Center; st.valign = Align::Center;
                dr::textBox(cx, text, layout::inset({ -w / 2, -h / 2, w, h }, 24), st);
            };
        },
    });

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

    Comp promo;
    promo.name = "promo"; promo.w = 1280; promo.h = 720; promo.fps = 30;
    promo.background = mkColor(0x101826);
    promo.defaultProps = Json::parse(R"({
        "title": "raymotion",
        "tagline": "Motion graphics in C++",
        "accent": "#47dad8",
        "features": ["Springs and easing", "Layers and masks", "Transitions", "Props from JSON"],
        "secondsPerScene": 3
    })");
    promo.build = buildPromo;
    out.push_back(std::move(promo));
}

int main(int argc, const char** argv) { return runCli(argc, argv, registerComps); }
