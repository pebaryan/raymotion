// asp.cpp -- "asp-feed": a casual, vertical (1080x1920) social explainer about answer
// set programming. Nine short scenes, one idea each, with pops on the key terms.
//   raymotion render asp-feed
#include "rmtn/raymotion.hpp"

namespace rmtn {

static Comp makeAspFeed() {
    Comp c;
    c.name = "asp-feed"; c.w = 1080; c.h = 1920; c.fps = 30;
    c.background = mkColor(0x0c1220);
    c.music = true;
    c.song = { { 0, 52, 220, 0.35, true } };
    const float W = (float)c.w, H = (float)c.h;
    const Color BG = c.background, INK = mkColor(0xf2f4ff), ACCENT = mkColor(0x47dad8),
                PANEL = mkColor(0x1b2638), GOLDEN = mkColor(0xffc365);

    const Rectangle box = layout::inset(layout::fill(W, H), 110);
    const int SC = 96;                                    // base scene length

    c.roots.clear();
    TransitionSeries ts(c.roots, W, H);
    auto backdrop = [](Color col) { return [col](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1e5f, 1e5f, col); }; };

    // A caption scene: kicker (progress) up top, the line in the middle, an optional
    // highlighted phrase that pops under it.
    int scene = 0, total = 9;
    auto caption = [&](const std::string& big, const std::string& pop, Color popColor) {
        TNode* s = ts.add(SC + (pop.empty() ? 0 : 30));
        scene++;
        s->fn = backdrop(BG);
        const int idx = scene;
        TNode* t = s->add(0, s->dur);
        t->up = [fps = c.fps](int f, NodeProps& p) {       // the whole card eases in
            p.opacity = (float)interpolate(f, { 0, 12 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
            p.pos.y = 40 * (1 - (float)interpolate(f, { 0, 12 }, { 0, 1 }, clamped(ease::out(ease::cubic))));
        };
        t->fn = [big, pop, popColor, idx, total, box, INK, ACCENT, PANEL](const Ctx& cx, int f) {
            TextStyle kick;
            kick.font = fonts::ui(true); kick.size = 40; kick.color = ACCENT;
            kick.align = Align::Center;
            dr::textBox(cx, "ANSWER SET PROGRAMMING  -  " + std::to_string(idx) + "/" + std::to_string(total),
                        { box.x, box.y - 60, box.width, 70 }, kick);

            TextStyle st;
            st.font = fonts::ui(true); st.size = 88; st.color = INK; st.lineHeight = 1.3f;
            st.align = Align::Center; st.valign = Align::Center;
            const float popH = pop.empty() ? 0.f : 190.f;
            const Rectangle body{ box.x, box.y - popH / 2, box.width, box.height - popH };
            dr::textBox(cx, big, body, st);

            if (!pop.empty()) {
                SpringOptions so;
                so.fps = 30; so.config.damping = 10;
                const float k = (float)spring(f, so);
                TextStyle big2 = st;
                big2.size = 104; big2.color = popColor;
                Ctx m = cx;
                m.o = mapP(cx, { body.x + body.width / 2, body.y + body.height + 40 });
                m.s *= 0.8f + 0.2f * (float)k;
                m.a *= (float)std::min(1.0, (double)k);
                const float pw = dr::measure(fonts::ui(true), pop.c_str(), 84, 1).x + 90;
                dr::roundRect(m, -pw / 2, -80, pw, 160, 28, PANEL);
                dr::text(fonts::ui(true), m, pop.c_str(), { 0, 0 }, 84, 1, popColor, 0.5, 0.5);
            }
        };
        return s;
    };

    // 1. Hook.
    caption("You know how coding means writing EVERY step?", "", ACCENT);
    ts.transition(Transition::slide(16, Direction::FromRight));
    // 2. The pitch.
    caption("Answer set programming says: nah. Just describe what a solution LOOKS like.", "describe, don't script", ACCENT);
    ts.transition(Transition::fade(14));
    // 3. Facts.
    caption("You write facts - little true things:", "bob is a farmer", GOLDEN);
    ts.transition(Transition::fade(14));
    // 4. Rules.
    caption("And rules: IF someone is a farmer, they own land. The computer connects the dots.", "if ... then ...", ACCENT);
    ts.transition(Transition::slide(16, Direction::FromLeft));
    // 5. Answer sets.
    caption("Then a solver (hi, clingo) hands you every stable world that fits. Each one is an", "answer set", GOLDEN);
    ts.transition(Transition::fade(14));
    // 6. Choice.
    caption("Choices are easy too: pick eggs OR pancakes for breakfast. Two worlds, both valid.", "eggs or pancakes", ACCENT);
    ts.transition(Transition::fade(14));
    // 7. Constraints.
    caption("Constraints are just nope. Fish with coffee? That world gets deleted.", "nope", GOLDEN);
    ts.transition(Transition::slide(16, Direction::FromRight));
    // 8. Why care.
    caption("Schedules, plans, puzzles - you describe the goal, the solver does the grinding.", "you describe, it searches", ACCENT);
    ts.transition(Transition::fade(18));
    // 9. Outro with confetti.
    {
        TNode* s = ts.add(SC + 30);
        scene++;
        s->fn = backdrop(BG);
        TNode* conf = s->add(0, s->dur);
        conf->props.pos = { W / 2, H / 2 };
        prefab::apply(conf, "confetti", Json::parse(R"({"count": 50, "seed": 11, "h": 1920})"));
        TNode* t = s->add(6, s->dur);
        t->up = [fps = c.fps](int f, NodeProps& p) {
            SpringOptions o;
            o.fps = fps; o.config.damping = 12;
            p.scale = (float)spring(f, o);
        };
        t->fn = [box, INK, ACCENT](const Ctx& cx, int) {
            TextStyle st;
            st.font = fonts::ui(true); st.size = 96; st.color = INK;
            st.align = Align::Center; st.valign = Align::Center; st.lineHeight = 1.3f;
            dr::textBox(cx, "Describe the goal. Skip the steps.", box, st);
            TextStyle small = st;
            small.size = 42; small.color = ACCENT;
            dr::textBox(cx, "answer set programming", { box.x, box.y + box.height - 40, box.width, 90 }, small);
        };
    }

    c.duration = ts.end();
    return c;
}

void registerAspFeed(Registry& out) { out.push_back(makeAspFeed()); }

} // namespace rmtn
