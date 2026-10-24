// asp.cpp -- "asp-feed": a casual, vertical (1080x1920) social explainer about answer
// set programming. Nine scenes, each with its own animated illustration above the
// caption: facts as chips, a rule wired by a drawing arrow, solver "worlds" as cards,
// a constraint crossing one out. Everything is deterministic (seeded randomness only).
//   raymotion render asp-feed
#include "rmtn/raymotion.hpp"

namespace rmtn {

static constexpr int SC = 108;                        // base scene length

static Comp makeAspFeed() {
    Comp c;
    c.name = "asp-feed"; c.w = 1080; c.h = 1920; c.fps = 30;
    c.background = mkColor(0x0c1220);
    c.music = true;
    c.song = { { 0, 52, 220, 0.35, true } };
    const float W = (float)c.w, H = (float)c.h;
    const Color BG = c.background, INK = mkColor(0xf2f4ff), ACCENT = mkColor(0x47dad8),
                PANEL = mkColor(0x1b2638), GOLDEN = mkColor(0xffc365), NOPE = mkColor(0xff5d5d),
                DIM = mkColor(0x8ea0b8);

    const Rectangle box = layout::inset(layout::fill(W, H), 90);
    const Vector2 art{ W / 2, 900 };                      // illustration centre

    c.roots.clear();
    TransitionSeries ts(c.roots, W, H);
    auto backdrop = [](Color col) { return [col](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1e5f, 1e5f, col); }; };

    // ---- small drawing helpers (all in scene-local units) --------------------
    auto chip = [](const Ctx& cx, const char* s, Vector2 pos, float k, Color col, Color bg) {
        if (k <= 0.01f) return;
        Ctx m = cx;
        m.o = mapP(cx, pos);
        m.s *= k;
        m.a *= std::min(1.f, k);
        const float w = dr::measure(fonts::ui(false), s, 46, 1).x + 64;
        dr::roundRect(m, -w / 2, -40, w, 80, 20, bg);
        dr::roundRect(m, -w / 2, -40, 10, 80, 5, col);
        dr::text(fonts::ui(false), m, s, { 0, 0 }, 46, 1, col, 0.5, 0.5);
    };
    auto card = [](const Ctx& cx, Vector2 pos, float w, float h, float k, Color bg, Color edge) {
        if (k <= 0.01f) return;
        Ctx m = cx;
        m.o = mapP(cx, pos);
        m.s *= k;
        m.a *= std::min(1.f, k);
        dr::roundRect(m, -w / 2, -h / 2, w, h, 26, bg);
        dr::roundRect(m, -w / 2, -h / 2, w, 12, 6, edge);
    };
    auto arrow = [](const Ctx& cx, Vector2 a, Vector2 b, float k, Color col) {   // draws a -> b
        if (k <= 0.01f) return;
        const Vector2 d{ b.x - a.x, b.y - a.y };
        const float len = std::sqrt(d.x * d.x + d.y * d.y);
        const Vector2 dir{ d.x / len, d.y / len };
        const Vector2 tip{ a.x + d.x * k, a.y + d.y * k };
        dr::strokeSeg(cx, a, tip, 8, col);
        if (k > 0.95f) {                                   // head
            const Vector2 p1{ b.x - dir.x * 34 - dir.y * 22, b.y - dir.y * 34 + dir.x * 22 };
            const Vector2 p2{ b.x - dir.x * 34 + dir.y * 22, b.y - dir.y * 34 - dir.x * 22 };
            const Vector2 pts[3] = { b, p1, p2 };
            dr::fillPoly(cx, pts, 3, col);
        }
    };
    // Spring scale released at frame `at` (0 -> 1, damped); slide = eased 0..1.
    auto popAt = [fps = c.fps](int f, int at) {
        SpringOptions o;
        o.fps = (double)fps; o.delay = at; o.config.damping = 11;
        return (float)spring((double)f, o);
    };
    auto slideAt = [](int f, int at) {
        return (float)interpolate((double)f - at, { 0, 14 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
    };
    auto sat = [](Color c, float k) { return withAlpha(WHITE, std::min(1.f, std::max(0.f, k))); };

    // A scene: kicker + caption up top, an illustration lambda in the middle.
    int scene = 0;
    const int total = 9;
    auto sceneCard = [&](const std::string& line, std::function<void(const Ctx&, int)> art2, int dur = SC) {
        TNode* s = ts.add(dur);
        scene++;
        s->fn = backdrop(BG);
        TNode* t = s->add(0, s->dur);
        t->up = [fps = c.fps](int f, NodeProps& p) {
            const float k = (float)interpolate(f, { 0, 12 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
            p.opacity = k;
            p.pos.y = 40 * (1 - k);
        };
        t->fn = [line, art2, idx = scene, total, box, INK, ACCENT, W, H](const Ctx& cx, int f) {
            TextStyle kick;
            kick.font = fonts::ui(true); kick.size = 38; kick.color = ACCENT; kick.align = Align::Center;
            dr::textBox(cx, "ANSWER SET PROGRAMMING  -  " + std::to_string(idx) + "/" + std::to_string(total),
                        { box.x, box.y - 40, box.width, 60 }, kick);
            TextStyle st;
            st.font = fonts::ui(true); st.size = 62; st.color = INK; st.lineHeight = 1.3f;
            st.align = Align::Center; st.valign = Align::Start;
            dr::textBox(cx, line, { box.x, box.y + 60, box.width, 480 }, st);
            Ctx m = cx;                                // illustrations live around the centre
            m.o.x += W / 2;
            m.o.y += H * 0.47f;
            art2(m, f);
        };
        return s;
    };

    // 1. Hook: a wobbling tower of "step" boxes stacks up; the goal just sits there.
    sceneCard("You know how coding means writing EVERY step?", [=](const Ctx& cx, int f) {
        const char* steps[4] = { "step 1", "step 2", "step 3", "step 4" };
        for (int i = 0; i < 4; i++) {
            const float k = popAt(f, 6 + i * 9);
            const float placed = (float)std::min(1.0, (double)slideAt(f, 6 + i * 9));
            const float wob = i == 3 ? (float)std::sin(f * 0.35) * 5 * placed : 0.f;
            card(cx, { -170 + wob, 250.f - i * 120 }, 300, 92, k, PANEL, DIM);
            dr::text(fonts::ui(false), cx, steps[i], { -170 + wob, 250.f - i * 120 }, 42, 1, sat(DIM, k), 0.5, 0.5);
        }
        const float g = popAt(f, 50);
        card(cx, { 260, 130 }, 330, 130, g, PANEL, ACCENT);
        dr::text(fonts::ui(true), cx, "the goal", { 260, 130 }, 52, 1, sat(ACCENT, g), 0.5, 0.5);
    });

    ts.transition(Transition::slide(16, Direction::FromRight));
    // 2. The pitch: HOW (a long script, struck through) vs WHAT (one card).
    sceneCard("Answer set programming: describe what a solution LOOKS like.", [=](const Ctx& cx, int f) {
        card(cx, { -250, 80 }, 380, 520, 1, PANEL, DIM);
        dr::text(fonts::ui(true), cx, "HOW", { -250, -130 }, 56, 1, DIM, 0.5, 0.5);
        for (int i = 0; i < 5; i++) {
            const float w = 260 - i * 34;
            const float k = slideAt(f, 8 + i * 6);
            dr::roundRect(cx, -250 - w / 2, -20.f + i * 64, w * k, 22, 11, withAlpha(DIM, 0.5f));
        }
        const float x = slideAt(f, 38);                    // the strikethrough
        dr::strokeSeg(cx, { -390, 200 }, { -390 + 280 * x, 200 + 50 * x }, 10, withAlpha(NOPE, x));
        const float g = popAt(f, 56);
        card(cx, { 250, 80 }, 380, 520, g, PANEL, ACCENT);
        dr::text(fonts::ui(true), cx, "WHAT", { 250, -100 }, 56, 1, sat(ACCENT, g), 0.5, 0.5);
        dr::text(fonts::ui(false), cx, "facts\nrules\nconstraints", { 250, 130 }, 46, 1, sat(INK, g), 0.5, 0.5);
    });

    ts.transition(Transition::fade(14));
    // 3. Facts: chips land one by one.
    sceneCard("You write facts - little true things:", [=](const Ctx& cx, int f) {
        chip(cx, "farmer(bob)", { 0, -100 }, popAt(f, 6), ACCENT, PANEL);
        chip(cx, "farmer(sue)", { 0, 40 }, popAt(f, 22), ACCENT, PANEL);
        chip(cx, "cat(tom)", { 0, 180 }, popAt(f, 38), GOLDEN, PANEL);
        dr::text(fonts::ui(false), cx, "just... true.", { 0, 320 }, 36, 1, sat(DIM, slideAt(f, 52)), 0.5, 0.5);
    });

    ts.transition(Transition::fade(14));
    // 4. Rules: the arrow draws itself from the fact to the conclusion.
    sceneCard("And rules: IF someone farms, they own land.", [=](const Ctx& cx, int f) {
        const float k = slideAt(f, 26);
        chip(cx, "farmer(X)", { -250, 0 }, popAt(f, 6), ACCENT, PANEL);
        arrow(cx, { -120, 0 }, { 120, 0 }, k, INK);
        dr::text(fonts::ui(true), cx, "IF", { 0, -70 }, 40, 1, sat(INK, k), 0.5, 0.5);
        chip(cx, "ownsLand(X)", { 250, 0 }, popAt(f, 44), GOLDEN, PANEL);
        dr::text(fonts::ui(false), cx, "the solver connects the dots", { 0, 180 }, 38, 1, sat(DIM, slideAt(f, 62)), 0.5, 0.5);
    });

    ts.transition(Transition::slide(16, Direction::FromLeft));
    // 5. Answer sets: clingo hands over every stable world.
    sceneCard("The solver (hi, clingo) hands you every stable world:", [=](const Ctx& cx, int f) {
        const float b = popAt(f, 4);
        dr::roundRect(cx, -110, -360, 220, 70, 18, PANEL);
        dr::text(fonts::ui(true), cx, "clingo", { 0, -325 }, 40, 1, sat(ACCENT, b), 0.5, 0.5);
        const char* worlds[3][2] = { { "bob farms", "bob owns" }, { "sue farms", "sue owns" }, { "tom naps", "..." } };
        for (int i = 0; i < 3; i++) {
            const float k = popAt(f, 18 + i * 14);
            const float x = -310.f + i * 310.f;
            card(cx, { x, 40 }, 280, 200, k, PANEL, i < 2 ? ACCENT : DIM);
            const float kk = (float)std::min(1.0, (double)k);
            dr::text(fonts::ui(true), cx, ("world " + std::to_string(i + 1)).c_str(), { x, -40 }, 38, 1, sat(INK, kk), 0.5, 0.5);
            for (int j = 0; j < 2; j++)
                dr::text(fonts::ui(false), cx, worlds[i][j], { x, 20.f + j * 50 }, 32, 1, sat(DIM, kk), 0.5, 0.5);
        }
        dr::text(fonts::ui(true), cx, "= answer sets", { 0, 260 }, 44, 1, sat(GOLDEN, slideAt(f, 64)), 0.5, 0.5);
    });

    ts.transition(Transition::fade(14));
    // 6. Choice: two worlds side by side, their edges ping-pong in turn.
    sceneCard("Choices are easy: pick eggs OR pancakes. Two worlds, both valid.", [=](const Ctx& cx, int f) {
        const char* items[2] = { "eggs", "pancakes" };
        for (int i = 0; i < 2; i++) {
            const float k = popAt(f, 6 + i * 16);
            const float x = -230.f + i * 460.f;
            const float hot = f > 36 ? 0.6f + 0.4f * (float)std::max(0.0, std::sin((f - 36) * 0.18 + i * 3.14f)) : 1.f;
            card(cx, { x, 40 }, 380, 280, k, PANEL, withAlpha(i == 0 ? ACCENT : GOLDEN, hot));
            const float kk = (float)std::min(1.0, (double)k);
            dr::circle(cx, { x, -30 }, 60 * kk, withAlpha(i == 0 ? ACCENT : GOLDEN, 0.25f * kk));
            dr::circle(cx, { x, -30 }, 34 * kk, i == 0 ? ACCENT : GOLDEN);
            dr::text(fonts::ui(true), cx, items[i], { x, 80 }, 46, 1, sat(INK, kk), 0.5, 0.5);
            dr::text(fonts::ui(false), cx, ("world " + std::to_string(i + 1)).c_str(), { x, 165 }, 30, 1, sat(DIM, kk), 0.5, 0.5);
        }
        dr::text(fonts::ui(true), cx, "or", { 0, 40 }, 52, 1, sat(INK, slideAt(f, 42)), 0.5, 0.5);
    });

    ts.transition(Transition::fade(14));
    // 7. Constraints: the fish-and-coffee world gets crossed out and ghosts out.
    sceneCard("Constraints are just nope. Fish AND coffee? That world is gone.", [=](const Ctx& cx, int f) {
        const char* items[3] = { "eggs + juice", "fish + coffee", "pancakes" };
        for (int i = 0; i < 3; i++) {
            const bool dead = i == 1;
            const float k = popAt(f, 6 + i * 14);
            const float xk = slideAt(f, 50);               // the cross draws in
            const float ghost = dead ? 1.f - 0.7f * xk : 1.f;
            const float x = -310.f + i * 310.f;
            card(cx, { x, 40 }, 280, 190, k * (dead ? (1.f - 0.3f * xk) : 1.f), PANEL, dead ? NOPE : ACCENT);
            const float kk = (float)std::min(1.0, (double)k) * ghost;
            dr::text(fonts::ui(true), cx, ("world " + std::to_string(i + 1)).c_str(), { x, 5 }, 36, 1, sat(INK, kk), 0.5, 0.5);
            dr::text(fonts::ui(false), cx, items[i], { x, 65 }, 30, 1, sat(DIM, kk), 0.5, 0.5);
            if (dead && xk > 0) {                          // the nope
                const Vector2 ctr{ x, 40 };
                const float r = 105;
                dr::strokeSeg(cx, { ctr.x - r * xk, ctr.y - r * xk }, { ctr.x + r * xk, ctr.y + r * xk }, 12, withAlpha(NOPE, xk));
                if (xk > 0.9f)
                    dr::strokeSeg(cx, { ctr.x - r, ctr.y + r }, { ctr.x + r, ctr.y - r }, 12, NOPE);
            }
        }
        dr::text(fonts::ui(true), cx, "nope", { 0, 260 }, 48, 1, sat(NOPE, slideAt(f, 62)), 0.5, 0.5);
    });

    ts.transition(Transition::slide(16, Direction::FromRight));
    // 8. Why care: three uses fly in over a little spinning "solver engine".
    sceneCard("Schedules, plans, puzzles - you describe, the solver grinds.", [=](const Ctx& cx, int f) {
        const char* uses[3] = { "schedules", "plans", "puzzles" };
        for (int i = 0; i < 3; i++) {
            const float k = popAt(f, 6 + i * 14);
            const float x = -300.f + i * 300.f;
            card(cx, { x, -60 }, 270, 170, k, PANEL, i == 1 ? GOLDEN : ACCENT);
            dr::text(fonts::ui(true), cx, uses[i], { x, -60 }, 40, 1, sat(INK, k), 0.5, 0.5);
        }
        const float e = popAt(f, 52);
        const float rot = f * 3.f;
        dr::ring(cx, { 0, 170 }, 60 * e, 8, withAlpha(ACCENT, 0.4f * e));
        for (int i = 0; i < 8; i++) {
            const float a = (rot + i * 45.f) * 3.14159f / 180;
            dr::strokeSeg(cx, { std::cos(a) * 70 * e, 170 + std::sin(a) * 70 * e },
                          { std::cos(a) * 88 * e, 170 + std::sin(a) * 88 * e }, 6, withAlpha(GOLDEN, e));
        }
        dr::text(fonts::ui(false), cx, "solving...", { 0, 170 }, 30, 1, sat(DIM, 0.8f * e), 0.5, 0.5);
    });

    ts.transition(Transition::fade(18));
    // 9. Outro with confetti.
    {
        TNode* s = ts.add(SC + 30);
        scene++;
        s->fn = backdrop(BG);
        TNode* conf = s->add(0, s->dur);
        conf->props.pos = { W / 2, H / 2 };
        prefab::apply(conf, "confetti", Json::parse(R"({"count": 60, "seed": 11, "h": 1920})"));
        TNode* t = s->add(6, s->dur);
        t->up = [fps = c.fps](int f, NodeProps& p) {
            SpringOptions o;
            o.fps = fps; o.config.damping = 12;
            p.scale = (float)spring(f, o);
        };
        t->fn = [box, INK, ACCENT](const Ctx& cx, int) {
            TextStyle st;
            st.font = fonts::ui(true); st.size = 92; st.color = INK;
            st.align = Align::Center; st.valign = Align::Center; st.lineHeight = 1.3f;
            dr::textBox(cx, "Describe the goal. Skip the steps.", box, st);
            TextStyle small = st;
            small.size = 40; small.color = ACCENT;
            dr::textBox(cx, "answer set programming", { box.x, box.y + box.height - 60, box.width, 80 }, small);
        };
    }

    c.duration = ts.end();
    return c;
}

void registerAspFeed(Registry& out) { out.push_back(makeAspFeed()); }

} // namespace rmtn
