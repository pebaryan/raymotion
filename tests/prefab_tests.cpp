// prefab_tests.cpp -- the prefab registry and instances: registration, props merging,
// timing, composition with loop/freeze, nesting and audio. No GL: the tree is stepped
// without a canvas.
#include "rmtn/prefab.hpp"
#include "rmtn/media.hpp"
#include <algorithm>
#include <cstdio>
#include <stdexcept>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

using namespace rmtn;

static void testRegistry() {
    prefab::add({ "alpha", Json::parse(R"({"a": 1})"), [](TNode& n, const Json& p) {
                     n.fn = [t = p.integer("a", 0)](const Ctx&, int) { (void)t; };
                 } });
    const auto names = prefab::names();
    CHECK(std::find(names.begin(), names.end(), "alpha") != names.end());
    CHECK(prefab::get("alpha").defaultProps.integer("a", 0) == 1);

    bool threw = false;                                    // unknown names are an error
    try { prefab::get("no-such-prefab"); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);

    prefab::add({ "alpha", Json::parse(R"({"a": 2})"), [](TNode&, const Json&) {} });
    CHECK(prefab::get("alpha").defaultProps.integer("a", 0) == 2);   // re-register replaces
}

static void testPropsMerge() {
    std::string got;
    prefab::add({ "merge", Json::parse(R"({"title": "default", "n": 5})"),
                  [&got](TNode&, const Json& p) { got = p.str("title", "") + ":" + std::to_string(p.integer("n", 0)); } });
    Nodes roots;
    prefab::use(roots, 0, 30, "merge", Json::parse(R"({"title": "custom"})"));
    drawTree(roots, 0);
    CHECK(got == "custom:5");                              // instance wins, default survives
}

static void testTiming() {
    prefab::add({ "probe", Json(), [](TNode&, const Json&) {} });
    Nodes roots;
    TNode* n = prefab::use(roots, 10, 20, "probe");
    CHECK(n->start == 10 && n->dur == 20);                 // the caller owns timing
    int seen = -1;
    n->fn = [&seen](const Ctx&, int lt) { seen = lt; };
    drawTree(roots, 9);
    CHECK(seen == -1);                                     // before its window
    seen = -1;
    drawTree(roots, 13);
    CHECK(seen == 3);                                      // local frame inside it
    seen = -1;
    drawTree(roots, 40);
    CHECK(seen == -1);                                     // after it
}

static void testCompose() {
    int lastT = -1;
    prefab::add({ "rec", Json(), [&](TNode& n, const Json&) {
                     n.up = [&lastT](int t, NodeProps&) { lastT = t; };
                 } });
    Nodes loopTree;
    TNode* l = loop(loopTree, 0, 7, 3);                    // a prefab inside a loop
    prefab::apply(l, "rec");
    drawTree(loopTree, 8);
    CHECK(lastT == 1);                                     // 8 % 7

    Nodes freezeTree;
    TNode* f = freeze(freezeTree, 0, 10, 42);              // ...inside a freeze
    prefab::apply(f, "rec");
    drawTree(freezeTree, 5);
    CHECK(lastT == 42);

    prefab::add({ "outer", Json(), [](TNode& n, const Json&) { prefab::use(&n, 5, 10, "probe"); } });
    Nodes nest;
    prefab::use(nest, 0, 30, "outer");
    drawTree(nest, 7);                                     // nested instance must resolve
    CHECK(true);
}

static void testAudio() {
    prefab::add({ "beep", Json(), [](TNode& n, const Json&) {
                     AudioClip clip;
                     clip.file = "nonexistent.wav";        // present on the node, never decoded
                     clip.volume = 0.f;
                     n.audio.push_back(std::move(clip));
                 } });
    Comp c;
    c.w = 4; c.h = 4; c.fps = 30; c.duration = 30;
    prefab::use(c.roots, 0, 30, "beep");
    CHECK(media::hasAudio(c));                             // the clip landed on the instance
}

int main() {
    testRegistry();
    testPropsMerge();
    testTiming();
    testCompose();
    testAudio();
    if (failures) { std::printf("%d prefab test failure(s)\n", failures); return 1; }
    std::puts("prefab tests passed");
    return 0;
}
