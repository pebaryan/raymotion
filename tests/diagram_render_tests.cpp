// diagram_render_tests.cpp -- diagrams and molecules on a canvas: build-up timing,
// highlights and traces, BPMN with its own coordinates, skeletal formulas, 3D ball-and-
// stick and protein cartoons. Pixel checks against a plain background.
#include "rmtn/raymotion.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

constexpr int W = 480, H = 320;

static Image render(Comp& c, int frame) {
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, frame, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}
static Comp comp(std::function<void(const Ctx&, int)> fn, int duration = 200) {
    Comp c;
    c.w = W; c.h = H; c.duration = duration; c.background = WHITE;
    addRoot(c.roots, 0, duration)->fn = std::move(fn);
    return c;
}
// RMTN_DUMP=<dir> saves the checked frames for a look.
static void dump(const Image& img, const char* name) {
    const char* dir = std::getenv("RMTN_DUMP");
    if (dir) ExportImage(img, (std::string(dir) + "/" + name + ".png").c_str());
}
// Pixels that differ from white, inside `r` (whole image when empty).
static int inked(const Image& img, Rectangle r = {}) {
    if (r.width <= 0) r = { 0, 0, (float)img.width, (float)img.height };
    int n = 0;
    for (int y = (int)r.y; y < (int)(r.y + r.height) && y < img.height; y++)
        for (int x = (int)r.x; x < (int)(r.x + r.width) && x < img.width; x++) {
            const Color c = GetImageColor(img, x, y);
            n += c.r < 235 || c.g < 235 || c.b < 235;
        }
    return n;
}
// Pixels close to `want`.
static int colored(const Image& img, Color want, int tol = 40) {
    int n = 0;
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++) {
            const Color c = GetImageColor(img, x, y);
            n += std::abs(c.r - want.r) < tol && std::abs(c.g - want.g) < tol && std::abs(c.b - want.b) < tol;
        }
    return n;
}

static void flowchart() {
    auto d = dg::Diagram::make(dg::mermaid(R"(graph LR
        A([Start]) --> B{Check} -->|ok| C[Done]
        B --> D[Retry] --> B)"));
    d->build(0, 6, 12);
    d->highlight(80, 10, { "C" }, Color{ 230, 40, 40, 255 });
    d->trace(110, 30, { "A", "B", "C" });
    Comp c = comp(dg::draw(d));
    Image empty = render(c, 0);
    Image half = render(c, 14);
    Image full = render(c, 70);
    const int e = inked(empty), h = inked(half), f = inked(full);
    CHECK(e < 50);                                         // nothing before the build
    CHECK(h > e + 200 && f > h + 200);                     // grows as elements appear
    // The fitted diagram fills most of the width.
    Rectangle a = d->nodeRect("A", 70, { 0, 0, W, H }), cr = d->nodeRect("C", 70, { 0, 0, W, H });
    CHECK(a.x < W * 0.25f && cr.x + cr.width > W * 0.6f && a.y > 0 && a.y + a.height < H);
    CHECK(inked(full, a) > a.width * a.height * 0.1f);
    Image lit = render(c, 100);
    CHECK(colored(lit, { 230, 40, 40, 255 }) > colored(full, { 230, 40, 40, 255 }) + 100);   // highlight
    Image traced = render(c, 125);
    dump(half, "flow-half"); dump(full, "flow-full"); dump(lit, "flow-lit"); dump(traced, "flow-traced");
    CHECK(inked(traced) >= inked(full) - 50);
    // Frames are independent of render order.
    Image again = render(c, 14);
    CHECK(inked(again) == h);
    for (Image* i : { &empty, &half, &full, &lit, &traced, &again }) UnloadImage(*i);
}

static void morphAndFocus() {
    auto d = dg::Diagram::make(dg::dot("digraph { a -> b -> c }"));
    d->morph(20, 20, dg::dot("digraph { a -> b -> c; b -> d; d -> e }"));
    d->focus(60, 20, { "e" }, 2.5f);
    CHECK(d->states() == 2);
    Comp c = comp(dg::draw(d));
    Image before = render(c, 10);
    Image after = render(c, 50);
    Rectangle e = d->nodeRect("e", 50, { 0, 0, W, H });
    CHECK(e.width > 0);
    CHECK(inked(before, e) < inked(after, e));             // the new node arrives
    Image zoom = render(c, 90);
    dump(before, "morph-before"); dump(after, "morph-after"); dump(zoom, "morph-zoom");
    CHECK(inked(zoom) > inked(after));                     // closer: bigger shapes and text
    for (Image* i : { &before, &after, &zoom }) UnloadImage(*i);
}

static const char* BPMN = R"(<?xml version="1.0"?>
<definitions xmlns="http://www.omg.org/spec/BPMN/20100524/MODEL" xmlns:bpmndi="http://www.omg.org/spec/BPMN/20100524/DI"
    xmlns:dc="http://www.omg.org/spec/DD/20100524/DC" xmlns:di="http://www.omg.org/spec/DD/20100524/DI">
  <process id="p"><startEvent id="s"/><task id="t" name="Work"/><endEvent id="e"/>
    <sequenceFlow id="f1" sourceRef="s" targetRef="t"/><sequenceFlow id="f2" sourceRef="t" targetRef="e"/></process>
  <bpmndi:BPMNDiagram><bpmndi:BPMNPlane bpmnElement="p">
    <bpmndi:BPMNShape bpmnElement="s"><dc:Bounds x="0" y="22" width="36" height="36"/></bpmndi:BPMNShape>
    <bpmndi:BPMNShape bpmnElement="t"><dc:Bounds x="100" y="0" width="100" height="80"/></bpmndi:BPMNShape>
    <bpmndi:BPMNShape bpmnElement="e"><dc:Bounds x="264" y="22" width="36" height="36"/></bpmndi:BPMNShape>
    <bpmndi:BPMNEdge bpmnElement="f1"><di:waypoint x="36" y="40"/><di:waypoint x="100" y="40"/></bpmndi:BPMNEdge>
    <bpmndi:BPMNEdge bpmnElement="f2"><di:waypoint x="200" y="40"/><di:waypoint x="264" y="40"/></bpmndi:BPMNEdge>
  </bpmndi:BPMNPlane></bpmndi:BPMNDiagram>
</definitions>)";

static void bpmnDiagram() {
    auto d = dg::Diagram::make(dg::bpmn(BPMN));
    d->theme = dg::Theme::bpmn();
    d->flow(0, 60, { "f1", "f2" }, 1);
    Comp c = comp(dg::draw(d, { 40, 40, 400, 240 }));
    Image img = render(c, 30);
    dump(img, "bpmn");
    // The kept proportions: the task is 100 / 300 of the width, centred vertically.
    Rectangle s = d->nodeRect("s", 30, { 40, 40, 400, 240 }), t = d->nodeRect("t", 30, { 40, 40, 400, 240 });
    CHECK(std::abs(t.width / s.width - 100.f / 36) < 0.1f);
    CHECK(std::abs((t.y + t.height / 2) - (s.y + s.height / 2)) < 1);
    CHECK(inked(img, t) > 200 && inked(img, s) > 50);
    UnloadImage(img);
}

static void skeletal() {
    auto m = chem::Depiction::make(chem::smiles("CN1C=NC2=C1C(=O)N(C(=O)N2C)C"));
    m->build(0, 2, 8);
    m->highlight(60, 10, chem::find(m->molecule, "C=O")[0], Color{ 20, 160, 60, 255 });
    Comp c = comp(chem::draw(m, { 40, 20, 400, 280 }));
    Image start = render(c, 0), built = render(c, 50), lit = render(c, 80);
    dump(built, "skeletal"); dump(lit, "skeletal-lit");
    CHECK(inked(start) < 50 && inked(built) > 1500);
    CHECK(colored(built, { 230, 30, 30, 255 }, 50) > 30);      // oxygens in red
    auto greenish = [](const Image& img) {             // the highlight glow is translucent
        int n = 0;
        for (int y = 0; y < img.height; y++)
            for (int x = 0; x < img.width; x++) {
                const Color c = GetImageColor(img, x, y);
                n += c.g > c.r + 25 && c.g > c.b + 15;
            }
        return n;
    };
    CHECK(greenish(lit) > greenish(built) + 300);
    for (int i = 0; i < (int)m->molecule.atoms.size(); i++) {
        const Vector2 p = m->atomAt(i, { 40, 20, 400, 280 });
        CHECK(p.x >= 40 && p.x <= 440 && p.y >= 20 && p.y <= 300);
    }
    for (Image* i : { &start, &built, &lit }) UnloadImage(*i);
}

static void molecules3d() {
    auto scene = std::make_shared<g3d::Scene>();
    scene->camera.position = { 0, 0, 14 };
    scene->camera.target = { 0, 0, 0 };
    scene->lights.push_back(g3d::Light::directional({ 3, 5, 4 }, WHITE, 1.2f));
    g3d::Object3D* mol = chem::addMolecule(*scene, chem::loadMolecule("data/caffeine.sdf"));
    CHECK(mol && mol->children.size() == 24 + 25);
    chem::buildIn(mol, 0, 1, 6);
    Comp c = comp(g3d::draw(scene), 100);
    Image early = render(c, 0), done = render(c, 90);
    dump(done, "caffeine3d");
    CHECK(inked(early) < inked(done) && inked(done) > 3000);
    UnloadImage(early);
    UnloadImage(done);

    auto prot = std::make_shared<g3d::Scene>();
    prot->camera.position = { 0, 0, 55 };
    prot->lights.push_back(g3d::Light::directional({ 3, 5, 4 }, WHITE, 1.2f));
    const chem::Protein crn = chem::loadProtein("data/1crn.pdb");
    g3d::Object3D* cart = chem::addCartoon(*prot, crn);
    CHECK(cart && cart->children.size() >= crn.residues.size());
    chem::buildIn(cart, 0, 1, 4);
    Comp pc = comp(g3d::draw(prot), 100);
    Image half = render(pc, 20), all = render(pc, 90);
    dump(half, "cartoon-half"); dump(all, "cartoon");
    CHECK(inked(half) < inked(all) && inked(all) > 4000);
    UnloadImage(half);
    UnloadImage(all);
}

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(W, H, "raymotion diagrams");
    if (!IsWindowReady()) return 77;
    try {
        flowchart();
        morphAndFocus();
        bpmnDiagram();
        skeletal();
        molecules3d();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: unexpected error: %s\n", e.what());
        failures++;
    }
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    std::printf("diagram render tests passed\n");
    return 0;
}
