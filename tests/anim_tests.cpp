// anim_tests.cpp -- interpolate, easings, springs, sequencing and layout maths. No GL:
// the tree is stepped without a canvas, so layers draw inline.
#include "rmtn/anim.hpp"
#include "rmtn/layout.hpp"
#include "rmtn/transitions.hpp"
#include <cstdio>
#include <map>
#include <stdexcept>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }

using namespace rmtn;

static void testInterpolate() {
    CHECK(near(interpolate(15, { 0, 30 }, { 0, 1 }), 0.5));
    CHECK(near(interpolate(45, { 0, 30 }, { 0, 1 }), 1.5));                       // extend
    CHECK(near(interpolate(45, { 0, 30 }, { 0, 1 }, clamped()), 1.0));
    CHECK(near(interpolate(-10, { 0, 30 }, { 0, 1 }, clamped()), 0.0));
    CHECK(near(interpolate(45, { 0, 30 }, { 0, 1 }, { ease::linear, Extrapolate::Clamp, Extrapolate::Identity }), 45));
    CHECK(near(interpolate(40, { 0, 30 }, { 0, 3 }, { ease::linear, Extrapolate::Wrap, Extrapolate::Wrap }), 1.0));
    CHECK(near(interpolate(-5, { 0, 30 }, { 0, 3 }, { ease::linear, Extrapolate::Wrap, Extrapolate::Wrap }), 2.5));
    // multi-segment with a descending output
    CHECK(near(interpolate(75, { 0, 50, 100 }, { 0, 1, 0 }), 0.5));
    CHECK(near(interpolate(25, { 0, 50, 100 }, { 0, 1, 0 }, { ease::quad }), 0.25));
    bool threw = false;
    try { interpolate(0, { 0, 0 }, { 0, 1 }); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { interpolate(0, { 0, 1, 2 }, { 0, 1 }); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);

    Color c = interpolateColors(5, { 0, 10 }, { Color{ 0, 0, 0, 255 }, Color{ 200, 100, 50, 255 } });
    CHECK(c.r == 100 && c.g == 50 && c.b == 25 && c.a == 255);
    c = interpolateColors(99, { 0, 10 }, { Color{ 0, 0, 0, 0 }, Color{ 10, 20, 30, 40 } });
    CHECK(c.r == 10 && c.a == 40);
}

static void testEasings() {
    const std::vector<std::pair<const char*, Easing>> all = {
        { "linear", ease::linear }, { "quad", ease::quad }, { "cubic", ease::cubic }, { "sine", ease::sine },
        { "circle", ease::circle }, { "expo", ease::expo }, { "bounce", ease::bounce }, { "poly4", ease::poly(4) },
        { "back", ease::back() }, { "elastic", ease::elastic() }, { "outCubic", ease::out(ease::cubic) },
        { "inOutSine", ease::inOut(ease::sine) }, { "bezier", ease::bezier(0.25, 0.1, 0.25, 1) },
        { "index", ease::fromIndex(E_OUT_CUBIC) },
    };
    for (auto& [name, e] : all) {
        if (!near(e(0), 0, 1e-3) || !near(e(1), 1, 1e-3)) { std::fprintf(stderr, "FAIL easing %s endpoints\n", name); failures++; }
    }
    // CSS "ease" at x = 0.5 is ~0.8024.
    CHECK(near(ease::bezier(0.25, 0.1, 0.25, 1)(0.5), 0.8024, 2e-3));
    CHECK(near(ease::bezier(0, 0, 1, 1)(0.3), 0.3, 1e-4));                       // identity curve
    CHECK(near(ease::inOut(ease::cubic)(0.5), 0.5));
    CHECK(ease::back()(0.2) < 0);                                                  // anticipation
}

static void testSpring() {
    CHECK(near(spring(0), 0));
    CHECK(near(spring(-5), 0));
    CHECK(near(spring(300), 1, 1e-4));
    double peak = 0, clampedPeak = 0;
    SpringOptions clamp;
    clamp.config.overshootClamping = true;
    for (int f = 0; f < 120; f++) {
        peak = std::max(peak, spring(f));
        clampedPeak = std::max(clampedPeak, spring(f, clamp));
    }
    CHECK(peak > 1.05);                                       // default config overshoots
    CHECK(clampedPeak <= 1.0 + 1e-9);
    CHECK(near(spring(100, clamp), 1));

    SpringOptions o;
    o.from = 10; o.to = -10; o.delay = 5;
    CHECK(near(spring(5, o), 10));
    CHECK(spring(8, o) < 10);
    CHECK(near(spring(400, o), -10, 1e-3));

    const int settle = measureSpring(30);
    CHECK(settle > 10 && settle < 90);
    for (int f = settle; f < settle + 60; f++) CHECK(std::fabs(spring(f) - 1) < 0.005);

    SpringOptions stretched;
    stretched.durationInFrames = 60;
    CHECK(std::fabs(spring(60, stretched) - 1) < 0.005);
    CHECK(near(spring(30, stretched), spring(settle / 2.0)));            // time scaled by settle/60
    SpringConfig heavy{ 1, 100, 40, false };                 // overdamped: never overshoots
    SpringOptions h; h.config = heavy;
    for (int f = 0; f < 200; f++) CHECK(spring(f, h) <= 1.0 + 1e-9);
    CHECK(measureSpring(30, heavy) > settle);
}

// Records the local frame (and opacity/offset) each node's fn saw at a given global frame.
struct Probe {
    std::map<int, int> t;
    std::map<int, float> a, x;
    std::function<void(const Ctx&, int)> fn(int id) {
        return [this, id](const Ctx& c, int lt) { t[id] = lt; a[id] = c.a; x[id] = c.o.x; };
    }
    void step(const Nodes& roots, int frame) { t.clear(); a.clear(); x.clear(); drawTree(roots, frame); }
};

static void testSequencing() {
    Nodes roots;
    Probe p;
    sequence(roots, 10, 20)->fn = p.fn(1);
    loop(roots, 0, 7, 3)->fn = p.fn(2);
    freeze(roots, 5, 10, 42)->fn = p.fn(3);
    TNode* parent = sequence(roots, 100, 100);
    Series s(parent, 5);
    s.add(10)->fn = p.fn(4);
    s.add(10, -3)->fn = p.fn(5);                              // overlaps the previous by 3
    s.add(10, 4)->fn = p.fn(6);                               // gap of 4
    CHECK(s.end() == 5 + 10 + 7 + 4 + 10);

    p.step(roots, 9);
    CHECK(!p.t.count(1) && p.t[2] == 2 && p.t[3] == 42);
    p.step(roots, 10);
    CHECK(p.t[1] == 0 && p.t[2] == 3);
    p.step(roots, 29);
    CHECK(p.t[1] == 19 && !p.t.count(2) && !p.t.count(3));    // loop ran 21 frames
    p.step(roots, 30);
    CHECK(!p.t.count(1));
    p.step(roots, 20);
    CHECK(p.t[2] == 6);
    p.step(roots, 100 + 13);
    CHECK(p.t[4] == 8 && p.t[5] == 1 && !p.t.count(6));
    p.step(roots, 100 + 25);
    CHECK(!p.t.count(5) && !p.t.count(6));                    // in the gap
    p.step(roots, 100 + 26);
    CHECK(p.t[6] == 0);
}

static void testTransitionTiming() {
    Nodes roots;
    Probe p;
    TransitionSeries ts(roots, 1000, 500);
    ts.add(60)->fn = p.fn(1);
    ts.transition(Transition::fade(20, ease::linear));
    ts.add(60)->fn = p.fn(2);
    ts.transition(Transition::slide(10, Direction::FromRight, ease::linear));
    ts.add(50)->fn = p.fn(3);
    CHECK(ts.end() == 60 + 40 + 40);

    p.step(roots, 39);                                        // before the fade
    CHECK(p.t.count(1) && !p.t.count(2));
    p.step(roots, 50);                                        // halfway through the fade
    CHECK(p.t[1] == 50 && p.t[2] == 10);
    CHECK(near(p.a[1], 1) && near(p.a[2], 0.5f, 1e-4));
    p.step(roots, 60);                                        // fade done, A gone
    CHECK(!p.t.count(1) && near(p.a[2], 1));
    p.step(roots, 95);                                        // halfway through the slide
    CHECK(p.t[2] == 55 && p.t[3] == 5);
    CHECK(near(p.x[2], -500, 1e-2) && near(p.x[3], 500, 1e-2));
    p.step(roots, 100);
    CHECK(!p.t.count(2) && near(p.x[3], 0));

    // Transitions never overlap more than either scene lasts.
    Nodes short_;
    TransitionSeries t2(short_, 100, 100);
    t2.add(5);
    t2.transition(Transition::wipe(30));
    TNode* n = t2.add(40);
    CHECK(n->start == 0 && t2.end() == 40);
}

static void testLayout() {
    Rectangle r{ 10, 20, 400, 100 };
    auto cols = layout::row(r, { layout::px(100), layout::flex(1), layout::flex(3) }, 20);
    CHECK(cols.size() == 3);
    CHECK(near(cols[0].x, 10) && near(cols[0].width, 100));
    CHECK(near(cols[1].x, 130) && near(cols[1].width, 65));
    CHECK(near(cols[2].x, 215) && near(cols[2].width, 195) && near(cols[2].height, 100));
    auto rows = layout::column(r, { layout::flex(), layout::flex() }, 10);
    CHECK(near(rows[1].y, 75) && near(rows[1].height, 45) && near(rows[1].width, 400));
    auto cells = layout::grid({ 0, 0, 300, 200 }, 3, 2, 0);
    CHECK(cells.size() == 6 && near(cells[4].x, 100) && near(cells[4].y, 100));
    Rectangle in = layout::inset(r, 10);
    CHECK(near(in.x, 20) && near(in.width, 380) && near(in.height, 80));
    Rectangle pl = layout::place(r, { 50, 40 }, Align::End, Align::Center);
    CHECK(near(pl.x, 360) && near(pl.y, 50));
}

static bool parseFails(const char* text) {
    try { Json::parse(text); } catch (const std::runtime_error&) { return true; }
    return false;
}

static void testJson() {
    Json j = Json::parse(R"( {"title": "Hi \u00e9\ud83d\ude00", "n": -1.5e2, "on": true, "none": null,
                              "list": [1, [2, {"k": "v"}]], "accent": "#ff8000", "rgba": [1, 2, 3, 4]} )");
    CHECK(j.str("title", "") == "Hi \xc3\xa9\xf0\x9f\x98\x80");
    CHECK(j.num("n", 0) == -150 && j.integer("n", 0) == -150);
    CHECK(j.flag("on", false) && j["none"].isNull() && j.num("none", 7) == 7);
    CHECK(j.num("missing", 3) == 3 && j.str("missing", "x") == "x");
    CHECK(j["list"].size() == 2 && j["list"][1][1]["k"].asString() == "v");
    CHECK(j["list"][9].isNull() && j["title"]["x"].isNull());
    Color a = j.color("accent", BLACK), b = j.color("rgba", BLACK);
    CHECK(a.r == 255 && a.g == 128 && a.b == 0 && a.a == 255);
    CHECK(b.r == 1 && b.a == 4);
    CHECK(parseColor("#abc").g == 0xbb && parseColor("#11223380").a == 0x80);

    bool threw = false;
    try { j.num("title", 0); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);                                              // wrong type is an error, not a fallback
    CHECK(parseFails("{\"a\": 1,}") && parseFails("[1 2]") && parseFails("01") && parseFails("\"\\x\"") &&
          parseFails("{} x") && parseFails("") && parseFails("'a'") && parseFails("[1.]") && parseFails("\"\\ud800\""));
    try { Json::parse("{\n  \"a\": tru\n}"); } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find("line 2") != std::string::npos);
    }

    // Round trip and deep merge (objects merge, everything else replaces).
    Json round = Json::parse(j.dump());
    CHECK(round == j);
    // (raw strings kept out of CHECK: MSVC cannot stringize them)
    const std::string escaped = R"({"s":"a\"b\\c\n"})";
    const bool escapesRoundTrip = Json::parse(escaped).dump() == escaped;
    CHECK(escapesRoundTrip);
    Json base = Json::parse(R"({"title": "A", "theme": {"bg": "#000", "fg": "#fff"}, "items": [1, 2, 3]})");
    Json merged = base.merged(Json::parse(R"({"theme": {"fg": "#f00"}, "items": [9], "extra": 1})"));
    const bool mergedOk = merged.dump() == R"({"title":"A","theme":{"bg":"#000","fg":"#f00"},"items":[9],"extra":1})";
    CHECK(mergedOk);
    Json built;
    built.set("x", 1).set("y", Json::Array{ 2, "z" });
    const bool builtOk = built.dump() == R"({"x":{"y":[2,"z"]}})";
    CHECK(builtOk);
}

int main() {
    testInterpolate();
    testEasings();
    testSpring();
    testSequencing();
    testTransitionTiming();
    testLayout();
    testJson();
    if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    std::puts("anim tests passed");
    return 0;
}
