// examples/kgnesy/main.cpp -- "kgnesy-memory": promotion/explainer short for
// "Structured Knowledge Memory for Personal Research Agents" (KG-NeSy 2026 @ ISWC).
// Diagram grammar: one API bus on top, three store platforms below, pipes joining them.
// Every visual and audio element is computed from numbers; nothing carries state between frames.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <memory>
#include <algorithm>
#include "rmtn/raymotion.hpp"
#include "../showcase/procgen.hpp"

namespace rmtn {
namespace {

const int FPS = 30;
const int DUR = 1680;                       // 56 s
const int W = 1920, H = 1080;
const std::string ASSETS = "out/kgnesy-assets";

// Night-family palette, one meaning per colour:
// teal = structured/graph truth, amber = semantic retrieval only, parchment = readable text layer,
// rose = rejection and mis-attribution states only.
const Color FIELD = mkColor(0x0b1020), FIELD_UP = mkColor(0x161c36);
const Color INK   = mkColor(0xf2f4ff), QUIET = mkColor(0x8ea0b8);
const Color TEAL  = mkColor(0x47dad8), AMBER = mkColor(0xffc365), ROSE = mkColor(0xff4fa3);
const Color PAPER = mkColor(0xcdd6e6);
const Color STONE = mkColor(0x3a4156);
const Color PANEL_BG = mkColor(0x0a1120);
const Color PLAT_TEXT = mkColor(0x2d3a52), PLAT_RDF = mkColor(0x25455a), PLAT_VEC = mkColor(0x464968);
const Color DEAD    = mkColor(0x4a4a48);     // demoted candidates and inactive entities

// Beat boundaries (frames).
const int B1 = 0, B2 = 270, B3 = 540, B4 = 840, B5 = 1200, B6 = 1410;
const int END_FROM = DUR - 180;             // end card takes over after the provenance settle

// One bus, three stores, three pipes: the layout is horizontal so every store stays visible
// and every connector is literal.
const float PX_TEXT = -2.4f, PX_RDF = 1.2f, PX_VEC = 4.4f;
const float PY_TEXT = 3.7f, PY_RDF = 2.8f, PY_VEC = 1.4f;
const float BUS_Y = 5.0f, GATE_Y = 4.25f, FRONT_Z = -2.0f;
const float PLAT_W = 5.0f, PLAT_D = 3.6f, PLAT_H = 0.18f;

// RDF lattice: two rows on aligned columns, so relations run along x or along z.
struct Cell { float x, z; };
std::vector<Cell> latticeCells() {
    std::vector<Cell> v;                                 // row A: 7 entities
    for (int i = 0; i < 7; i++) v.push_back({ PX_RDF - 2.1f + i * 0.7f, -0.9f });
    for (int i = 0; i < 4; i++) v.push_back({ PX_RDF - 1.4f + i * 0.7f, 0.9f });   // row B: aligned
    return v;
}
const int FC = 3;                                  // stored eval result of the paper's example query
const float FC_X = PX_RDF, FC_Z = -0.9f;          // the entity the surviving candidate resolves to
const float SURV_Y = PY_VEC + 0.8f;         // survivor lifts off its own platform before crossing
const int RUNNING[4] = { 1, 4, 7, 9 };            // status = running subset
const int CAND[4] = { 3, 11, 24, 41 };            // embedding candidates; CAND[2] survives filtering
const int EDGES[][2] = { {0,1},{1,2},{2,3},{3,4},{4,5},{5,6},{7,8},{8,9},{9,10},
                         {1,7},{2,8},{3,9},{4,10} };   // axis-aligned relations

void genScore(const std::string& path) {
    procgen::Score s((double)DUR / FPS);
    const double secs = (double)DUR / FPS;
    // Low bed under the whole film; a swell into the retrieval beat; an impact on the payoff.
    s.pad(0.0, secs, 41, 0.14f, 220.f, 0.45f);
    s.pad(0.0, secs, 53, 0.07f, 320.f, 0.62f);
    s.bass(1.2, 2.2, 29, 0.30f);
    s.bass(9.0, 3.0, 34, 0.26f);
    for (int i = 0; i < 14; i++) s.hat(1.0 + i * 0.5, 0.06f, i % 2 ? 0.7f : 0.3f);
    const double notes[7] = { 57, 64, 69, 72, 67, 74, 76 };
    for (int i = 0; i < 7; i++) s.pluck(2.0 + i * 5.4, notes[i], 0.16f, 0.35f + 0.3f * (i % 2));
    s.riser(24.0, 27.6, 0.14f);
    s.impact(27.8, 0.34f);           // the graph answers
    s.tick(28.1, 0.12f);
    s.riser(43.5, 46.4, 0.12f);
    s.impact(46.6, 0.30f);           // provenance payoff
    for (int i = 0; i < 6; i++) s.bell(47.0 + i * 1.3, notes[i % 7] + 12, 0.09f, 0.5f);
    media::writeWav(path, s.mix(), procgen::Score::RATE);
}

bool assetsReady = false;
void ensureAssets() {
    static const std::filesystem::path dir{ ASSETS };
    if (assetsReady) return;
    std::filesystem::create_directories(dir);
    std::filesystem::path wav = dir / "score.wav";
    if (!std::filesystem::exists(wav)) genScore(wav.string());
    assetsReady = true;
}

double seg(int t, int a, int b) {                       // eased ramp over [a,b]
    return interpolate((double)t, { (double)a, (double)b }, { 0.0, 1.0 }, clamped(ease::out(ease::cubic)));
}
double ramp(int t, int a, int b, int c, int d) {        // rise a->b, hold, fall c->d
    return seg(t, a, b) * (1.0 - seg(t, c, d));
}
unsigned char cl8(float v) { return v > 255.f ? 255 : (v < 0.f ? 0 : (unsigned char)v); }
Color glow(Color base, float k) {   // emissive lift that keeps the hue; never a translucent blob
    return Color{ cl8(base.r * 0.4f + 150 * k), cl8(base.g * 0.4f + 140 * k), cl8(base.b * 0.4f + 120 * k), 255 };
}

Vector3 focalAt(int t) {
    // One continuous focus path: text store -> bus -> gate -> cloud -> resolved entity -> trace.
    return {
        (float)interpolate((double)t, { B1, B2, B3, B4, B5, B6, DUR },
                           { PX_TEXT, PX_RDF, PX_RDF, PX_VEC, FC_X, PX_RDF + 1.7f, PX_RDF },
                           clamped(ease::inOut(ease::cubic))),
        (float)interpolate((double)t, { B1, B2, B3, B4, B5, B6, DUR },
                           { PY_TEXT + 0.2f, BUS_Y + 0.1f, GATE_Y, SURV_Y, PY_RDF + 0.14f, PY_RDF + 0.14f, PY_RDF + 0.6f },
                           clamped(ease::inOut(ease::cubic))),
        (float)interpolate((double)t, { B1, B2, B3, B4, B5, B6, DUR },
                           { 0.0, 0.0, FRONT_Z, FC_Z, FC_Z, FC_Z, 0.0 },
                           clamped(ease::inOut(ease::cubic))) };
}

// Bottom-left statement block on a solid panel, so scene geometry never competes with copy.
void caption(Nodes& roots, int from, int dur, const std::string& kick, const std::string& body,
             const std::string& sub) {
    TNode* n = addRoot(roots, from, dur);
    n->up = [dur](int t, NodeProps& p) {
        float fade = (float)(seg(t, 6, 26) * (1.0 - seg(t, dur - 26, dur - 6)));
        p.opacity = fade;
        p.pos.y = (1.0f - fade) * 14.0f;                 // rises into place instead of just appearing
    };
    n->fn = [kick, body, sub](const Ctx& cx, int) {
        dr::roundRect(cx, 60.f, H - 360.f, 880.f, 250.f, 24.f, PANEL_BG);
        TextStyle k;  k.font = fonts::ui(true); k.size = 32; k.spacing = 1; k.color = TEAL;
        dr::textBox(cx, kick, { 96.f, H - 352.f, 780.f, 34.f }, k);
        TextStyle b;  b.font = fonts::ui(true); b.size = 52; b.color = INK; b.lineHeight = 1.2f;
        dr::textBox(cx, body, { 96.f, H - 314.f, 820.f, 140.f }, b);
        TextStyle s;  s.font = fonts::ui(false); s.size = 28; s.color = withAlpha(PAPER, 0.9f); s.lineHeight = 1.3f;
        dr::textBox(cx, sub, { 96.f, H - 176.f, 820.f, 60.f }, s);
    };
}

void chip(TNode* parent, int from, int dur, const std::string& text, Color accent, Vector2 pos) {
    TNode* n = parent->add(from, dur);
    n->up = [](int t, NodeProps& p) {
        p.scale = (float)spring((double)t, { FPS, {}, 0.0, 1.0, 0 });
        p.opacity = (float)seg(t, 4, 16);
    };
    n->fn = [text, accent](const Ctx& cx, int) {
        Font f = fonts::ui(true);
        float w = dr::measure(f, text.c_str(), 34.f, 1.f).x + 40.f;
        dr::roundRect(cx, -w / 2, -28.f, w, 56.f, 18.f, mkColor(0x141b2a, 235));
        dr::roundRect(cx, -w / 2, -28.f, 8.f, 56.f, 4.f, accent);
        dr::text(f, cx, text.c_str(), { 0.f, 0.f }, 34.f, 1.f, INK, 0.5, 0.5);
    };
    n->props.pos = pos;
}

} // namespace

void registerMemoryPromo(Registry& out) {
    Comp c;
    c.name = "kgnesy-memory"; c.w = W; c.h = H; c.fps = FPS; c.duration = DUR;
    c.supersample = 2;
    c.background = FIELD;
    c.assets = [](const Comp&, const Json&) { ensureAssets(); };
    c.build = [](Comp& comp, const Json&) {
        comp.roots.clear();
        media::audio(addRoot(comp.roots, 0, DUR), ASSETS + "/score.wav");
        if (!assetsReady) return;   // metadata only for `list`: no generated files to read yet

        auto scene = std::make_shared<g3d::Scene>();
        scene->environmentGenerator = g3d::studioEnvironment(mkColor(0x3a4670), mkColor(0x1a2036), mkColor(0x0c0e18), 4);
        scene->environmentIntensity = 1.05f;
        scene->skyColor = mkColor(0xcfe0ff), scene->groundColor = mkColor(0x26232f);
        scene->ambient = 0.4f;
        scene->background = BLANK;                  // the 2D gradient backdrop shows through
        scene->fogColor = FIELD; scene->fogNear = 16; scene->fogFar = 52;
        scene->shadowMapSize = 1024;

        g3d::Material floorMat; floorMat.color = STONE; floorMat.roughness = 0.9f; floorMat.metalness = 0.f;
        floorMat.castShadow = false;
        scene->add(g3d::Geometry::plane(36, 36), floorMat);

        // Three stores in three distinct tones; they receive light but cast nothing.
        auto platform = [&](float x, float y, Color base, Color lit, int litFrom, int litTo) {
            g3d::Material m; m.color = base; m.roughness = 0.85f; m.metalness = 0.05f; m.castShadow = false;
            g3d::Object3D* p = scene->add(g3d::Geometry::box(PLAT_W, PLAT_H, PLAT_D), m);
            p->position = { x, y, 0.f };
            p->up = [base, lit, litFrom, litTo](int t, g3d::Object3D& o) {
                float k = ramp(t, litFrom, litTo, DUR - 40, DUR) * 0.8f + seg(t, B2, B2 + 90) * 0.25f;
                o.material.color = mixC(base, lit, k);
            };
        };
        platform(PX_TEXT, PY_TEXT, PLAT_TEXT, mkColor(0x4d6088), B1 + 40, B1 + 120);
        platform(PX_RDF, PY_RDF, PLAT_RDF, mkColor(0x3a6b7c), B2 + 30, B2 + 110);
        platform(PX_VEC, PY_VEC, PLAT_VEC, mkColor(0x66648a), B2 + 60, B2 + 140);

        // Legs ground every platform on the floor, so nothing reads as a floating slab.
        const float px[3] = { PX_TEXT, PX_RDF, PX_VEC };
        const float py[3] = { PY_TEXT, PY_RDF, PY_VEC };
        for (int i = 0; i < 3; i++) {
            g3d::Material lm; lm.color = mkColor(0x2e3b52); lm.roughness = 0.7f; lm.metalness = 0.2f;
            lm.castShadow = false;
            g3d::Object3D* legNode = scene->add(g3d::Geometry::cylinder(0.12f, py[i]), lm);
            legNode->position = { px[i], py[i] / 2, 0.f };
            legNode->name = "leg-" + std::to_string(i);
            const int delay = 30 + i * 18;
            legNode->up = [delay](int t, g3d::Object3D& o) {
                float k = (float)spring((double)t, { FPS, {}, 0.0, 1.0, B2 + delay });
                o.scale = { k, k, k };
            };
        }
        for (int i = 0; i < 3; i++) {
            g3d::Material pm; pm.color = mkColor(0x35707e); pm.roughness = 0.5f; pm.metalness = 0.35f;
            pm.castShadow = false;
            g3d::Object3D* pipe = scene->add(g3d::Geometry::cylinder(0.18f, BUS_Y - py[i]), pm);
            pipe->position = { px[i], (BUS_Y + py[i]) / 2, 0.f };
            pipe->name = "pipe-" + std::to_string(i);
            const int delay = i * 22;
            pipe->up = [delay](int t, g3d::Object3D& o) {
                float k = (float)seg(t, B2 + delay, B2 + delay + 60);
                o.scale = { k, 1.f, k };
                o.material.emissive = glow(TEAL, 0.7f * k);
            };
        }

        // The unified API bus caps the whole diagram.
        g3d::Material busMat; busMat.color = TEAL; busMat.roughness = 0.2f; busMat.metalness = 0.6f;
        busMat.castShadow = false;
        g3d::Object3D* bus = scene->add(g3d::Geometry::box(11.0f, 0.26f, 0.6f), busMat);
        bus->position = { 1.2f, BUS_Y, 0.f };
        bus->name = "api-bus";
        bus->up = [](int t, g3d::Object3D& o) {
            float k = (float)spring((double)t, { FPS, {}, 0.0, 1.0, B2 });
            o.scale = { k, k, k };
            o.material.emissive = glow(TEAL, 0.6f * k);
        };
        // Shape-check gate, standing on the middle pipe between bus and store.
        g3d::Material gateMat; gateMat.color = QUIET; gateMat.roughness = 0.4f; gateMat.castShadow = false;
        g3d::Object3D* gate = scene->add(g3d::Geometry::box(0.22f, 1.1f, 0.9f), gateMat);
        gate->position = { PX_RDF, GATE_Y, 0.f };
        gate->up = [](int t, g3d::Object3D& o) {
            float rej = ramp(t, B3 + 40, B3 + 60, B3 + 130, B3 + 170);
            o.material.emissive = glow(ROSE, 1.2f * rej);   // reacts without becoming a failure surface
        };

        // Store 1: readable notes in parchment; three lookalikes pulse during the hook beat.
        for (int i = 0; i < 6; i++) {
            g3d::Material noteMat; noteMat.color = mkColor(0x6f7a8d); noteMat.roughness = 0.85f;
            g3d::Object3D* line = scene->add(g3d::Geometry::box(2.2f, 0.07f, 0.16f), noteMat);
            line->position = { PX_TEXT - 0.3f, PY_TEXT + 0.14f, -1.2f + i * 0.5f };
            line->up = [i](int t, g3d::Object3D& o) {
                float hook = ramp(t, B1 + 30, B1 + 70, B2 - 20, B2 + 10);
                float lookalike = (i >= 2 && i <= 4) ? 0.5f + 0.5f * (float)std::sin(t * 0.08f + i) : 0.f;
                o.material.color = mixC(mkColor(0x6f7a8d), PAPER, hook * (0.6f + 0.4f * lookalike));
            };
        }

        // Store 2: RDF entities with visible relations. During enrichment the resolved entity
        // brightens and enlarges while siblings desaturate, so one node reads as the answer.
        std::vector<Cell> cells = latticeCells();
        for (size_t i = 0; i < cells.size(); i++) {
            g3d::Material m; m.color = mkColor(0x2f7f7d); m.roughness = 0.3f; m.metalness = 0.45f;
            g3d::Object3D* nd = scene->add(g3d::Geometry::sphere(0.14f), m);
            const Cell cell = cells[i];
            const int idx = (int)i;
            nd->position = { cell.x, PY_RDF + 0.14f, cell.z };
            nd->name = "entity-" + std::to_string(idx);
            nd->up = [idx](int t, g3d::Object3D& o) {
                float pop = (float)spring((double)t, { FPS, {}, 0.0, 1.0, B2 + idx * 4 });
                bool isFocus = (idx == FC);
                bool isRun = std::find(std::begin(RUNNING), std::end(RUNNING), idx) != std::end(RUNNING);
                float reveal = seg(t, B2, B2 + 90) * 0.25f;
                float enrich = (float)ramp(t, B4 + 60, B4 + 150, DUR - 60, DUR);   // graph narrows the set
                float exact = ramp(t, B5 + 20, B5 + 80, B6 - 40, B6) * (isRun ? 1.f : 0.f);
                float k = std::min(1.f, 0.35f + reveal + exact + (isFocus ? enrich : 0.2f * enrich));
                o.material.color = mixC(mixC(mkColor(0x2f7f7d), DEAD, isFocus ? 0.f : 0.45f * enrich), TEAL, k);
                if (isFocus) o.material.color = mixC(o.material.color, INK, enrich * 0.7f);
                o.material.emissive = glow(TEAL, k * (isFocus ? 1.15f : 0.4f));
                float sc = pop * (1.f + 0.9f * enrich * (isFocus ? 1.f : 0.f) - 0.25f * enrich * (isFocus ? 0.f : 1.f));
                o.scale = { sc, sc, sc };
                o.position.y = PY_RDF + 0.14f + 0.16f * enrich * (isFocus ? 1.f : 0.f);
            };
        }
        g3d::Material haloMat; haloMat.color = INK; haloMat.roughness = 0.3f; haloMat.metalness = 0.2f;
        haloMat.emissive = glow(TEAL, 0.95f); haloMat.castShadow = false;
        g3d::Object3D* halo = scene->add(g3d::Geometry::torus(0.5f, 0.045f, 64, 20), haloMat);
        halo->position = { FC_X, PY_RDF + 0.34f, FC_Z };
        halo->rotation = Vector3{ 90.f, 0.f, 0.f };
        halo->visible = false;
        halo->up = [](int t, g3d::Object3D& o) {
            float k = (float)ramp(t, B4 + 230, B4 + 310, DUR - 30, DUR);   // appears once the token lands
            o.visible = k > 0.02f;
            o.scale = { k, k, k };
            o.rotation.z = (float)(t * 0.6);
        };
        for (auto& pr : EDGES) {
            const Cell ca = cells[pr[0]], cb = cells[pr[1]];
            bool alongX = std::fabs(ca.z - cb.z) < 0.2f;
            float len = alongX ? std::fabs(cb.x - ca.x) : std::fabs(cb.z - ca.z);
            g3d::Material em; em.color = mkColor(0x35767a); em.roughness = 0.4f; em.metalness = 0.3f;
            em.castShadow = false;
            g3d::Object3D* e = scene->add(g3d::Geometry::cylinder(0.045f, len), em);
            e->position = { (ca.x + cb.x) / 2, PY_RDF + 0.14f, (ca.z + cb.z) / 2 };
            e->rotation = alongX ? Vector3{ 0.f, 0.f, 90.f } : Vector3{ 90.f, 0.f, 0.f };
            e->up = [a = pr[0], b = pr[1]](int t, g3d::Object3D& o) {
                bool touches = (a == FC || b == FC);
                float fanout = ramp(t, B4 + 240, B4 + 330, DUR - 40, DUR) * (touches ? 1.f : 0.15f);
                float exact = ramp(t, B5 + 30, B5 + 90, B6 - 30, B6) * 0.5f;
                float k = std::min(1.f, 0.35f + fanout + exact);
                o.material.color = mixC(mkColor(0x35767a), TEAL, k);
                o.material.emissive = glow(TEAL, k * (touches ? 0.85f : 0.15f));
            };
        }

        // Store 3: embedding cloud. Amber belongs only to semantic retrieval; the survivor stays
        // bright while wrong lookalikes desaturate and shrink.
        for (int i = 0; i < 70; i++) {
            Vector2 p = rvec2(9001ULL, 7ULL, (unsigned long long)i);
            bool cand = std::find(std::begin(CAND), std::end(CAND), i) != std::end(CAND);
            bool survivor = cand && i == CAND[2];
            g3d::Material m; m.color = mkColor(0x5c5145); m.roughness = 0.6f; m.castShadow = false;
            g3d::Object3D* pt = scene->add(g3d::Geometry::sphere(survivor ? 0.18f : (cand ? 0.13f : 0.09f)), m);
            // Keep a clear corridor around the survivor's column so the promotion rod reads cleanly.
            float cx = PX_VEC - 2.0f + p.x * 3.6f;
            if (!survivor && std::fabs(cx - PX_VEC) < 0.7f) cx -= 1.1f;
            float cz = -1.4f + (float)(i % 5) * 0.7f;
            if (!survivor && std::fabs(cz - FC_Z) < 0.55f) cz += 0.85f;
            Vector3 home = survivor ? Vector3{ PX_VEC, SURV_Y, FC_Z }
                                   : Vector3{ cx, PY_VEC + 0.45f + p.y * 0.6f, cz };
            pt->up = [i, home, cand, survivor](int t, g3d::Object3D& o) {
                float drift = survivor ? 0.f : (float)std::sin(t * 0.03f + i) * 0.05f;
                float candK = ramp(t, B4 + 20, B4 + 70, B6, B6 + 40) * (cand ? 1.f : 0.f);
                float wrong = ramp(t, B4 + 150, B4 + 210, B6, B6 + 40) * (cand && !survivor ? 1.f : 0.f);
                float k = candK * (1.f - wrong);   // amber only ever marks retrieval results
                o.position = { home.x + drift, home.y + drift * 0.5f, home.z - drift };
                o.material.color = mixC(mixC(mkColor(0x5c5145), AMBER, k), DEAD, wrong);
                o.material.emissive = glow(AMBER, k * 1.0f);
                float sc = (survivor ? 1.6f : 1.f) * (1.f + 0.5f * candK * (1.f - wrong)) * (1.f - 0.35f * wrong);
                o.scale = { sc, sc, sc };
            };
        }

        // Promotion path: the survivor rises out of its store, then crosses to the resolved entity.
        // Two axis-aligned legs make the path literal; the rod appears once the token lands.
        g3d::Material legMat; legMat.color = TEAL; legMat.roughness = 0.4f; legMat.metalness = 0.3f;
        legMat.castShadow = false;
        g3d::Object3D* riseLeg = scene->add(g3d::Geometry::cylinder(0.12f, PY_RDF + 0.14f - SURV_Y), legMat);
        riseLeg->position = { PX_VEC, (SURV_Y + PY_RDF + 0.14f) / 2, FC_Z };
        riseLeg->visible = false;
        g3d::Object3D* crossLeg = scene->add(g3d::Geometry::cylinder(0.12f, std::fabs(PX_VEC - FC_X)), legMat);
        crossLeg->position = { (PX_VEC + FC_X) / 2, PY_RDF + 0.14f, FC_Z };
        crossLeg->rotation = Vector3{ 0.f, 0.f, 90.f };
        crossLeg->visible = false;
        auto rodUp = [](int t, g3d::Object3D& o) {
            float arrive = (float)seg(t, B4 + 230, B4 + 320);
            o.visible = arrive > 0.02f;
            o.material.emissive = glow(TEAL, 0.85f * arrive);
            o.scale = { arrive, arrive, arrive };
        };
        riseLeg->up = rodUp; crossLeg->up = rodUp;
        g3d::Material promoMat; promoMat.color = AMBER; promoMat.roughness = 0.3f; promoMat.metalness = 0.2f;
        promoMat.castShadow = false;
        g3d::Object3D* promo = scene->add(g3d::Geometry::sphere(0.2f), promoMat);
        promo->name = "promoted-candidate";
        promo->visible = false;
        promo->up = [](int t, g3d::Object3D& o) {
            o.visible = (t >= B4 + 110 && t <= B4 + 320);
            if (!o.visible) return;
            float u = (float)seg(t, B4 + 110, B4 + 230);
            float y = SURV_Y + (PY_RDF + 0.14f - SURV_Y) * std::min(1.f, u * 1.6f);
            float x = PX_VEC + (FC_X - PX_VEC) * std::max(0.f, (u - 0.6f) / 0.4f);
            o.position = { x, y, FC_Z };
            o.material.color = mixC(AMBER, TEAL, u);          // semantic hit becomes a structured fact
            o.material.emissive = glow(u > 0.5f ? TEAL : AMBER, 1.1f);
        };

        // Write path down the middle pipe: a malformed candidate halts above the bus and never
        // reaches a store; a conforming one passes the gate and forks through the other pipes.
        g3d::Material roseTok; roseTok.color = ROSE; roseTok.roughness = 0.3f; roseTok.metalness = 0.3f;
        roseTok.castShadow = false;
        g3d::Object3D* rejected = scene->add(g3d::Geometry::sphere(0.26f), roseTok);
        rejected->visible = false;
        rejected->up = [](int t, g3d::Object3D& o) {
            o.visible = (t >= B3 + 20 && t <= B3 + 180);
            if (!o.visible) return;
            float u = (float)seg(t, B3 + 20, B3 + 110);
            o.position = { PX_RDF, 7.4f - (7.4f - (BUS_Y + 0.85f)) * u, FRONT_Z };
            o.material.emissive = glow(ROSE, 0.8f);
        };
        g3d::Material accMat; accMat.color = TEAL; accMat.roughness = 0.3f; accMat.metalness = 0.3f;
        accMat.castShadow = false;
        g3d::Object3D* accepted = scene->add(g3d::Geometry::sphere(0.26f), accMat);
        accepted->visible = false;
        accepted->up = [](int t, g3d::Object3D& o) {
            o.visible = (t >= B3 + 130 && t <= B3 + 240);
            if (!o.visible) return;
            float u = (float)seg(t, B3 + 130, B3 + 215);
            o.position = { PX_RDF, 7.4f - (7.4f - (PY_RDF + 0.35f)) * u, FRONT_Z + (0.f - FRONT_Z) * std::min(1.f, u * 2.f) };
            o.material.color = u > 0.55f ? TEAL : AMBER;      // accepted becomes structured after validation
            o.material.emissive = glow(u > 0.55f ? TEAL : AMBER, 0.9f);
        };
        for (int i = 0; i < 3; i++) {
            if (i == 1) continue;                             // the middle pipe already got the write
            const int s = B3 + 225 + i * 20, e = B3 + 300 + i * 20;
            g3d::Object3D* fork = scene->add(g3d::Geometry::sphere(0.2f), accMat);
            fork->visible = false;
            fork->up = [s, e, tx = px[i], ty = py[i]](int t, g3d::Object3D& o) {
                o.visible = (t >= s && t <= e + 60);
                if (!o.visible) return;
                float x = (float)interpolate((double)t, { (double)s, (double)s + 40, (double)e },
                                             { PX_RDF, PX_RDF, tx }, clamped(ease::inOut(ease::cubic)));
                float y = (float)interpolate((double)t, { (double)s, (double)s + 40, (double)e },
                                             { BUS_Y - 0.2f, BUS_Y - 0.2f, ty + 0.3f }, clamped(ease::inOut(ease::cubic)));
                o.position = { x, y, 0.f };
                float sc = 1.f - 0.3f * seg(t, s, e);
                o.scale = { sc, sc, sc };
                o.material.emissive = glow(TEAL, 0.85f);
            };
        }

        // Provenance trace: the stored result links back through a tool node to its activity.
        const float TOOL_X = PX_RDF + 1.2f, ACT_X = PX_RDF + 2.2f;
        g3d::Material actMat; actMat.color = PAPER; actMat.roughness = 0.25f; actMat.metalness = 0.4f;
        actMat.castShadow = false;
        g3d::Object3D* act = scene->add(g3d::Geometry::sphere(0.26f), actMat);
        act->position = { ACT_X, PY_RDF + 0.14f, FC_Z };
        act->name = "activity";
        g3d::Material toolMat; toolMat.color = mkColor(0x9fb6c4); toolMat.roughness = 0.35f; toolMat.castShadow = false;
        g3d::Object3D* toolNode = scene->add(g3d::Geometry::sphere(0.18f), toolMat);
        toolNode->position = { TOOL_X, PY_RDF + 0.14f, FC_Z };
        toolNode->name = "tool-call";
        auto traceUp = [](int t, g3d::Object3D& o) {
            float k = (float)seg(t, B6 + 20, B6 + 90);
            o.visible = k > 0.02f;
            o.scale = { k, k, k };
            o.material.emissive = glow(PAPER, 0.5f * k);
        };
        act->visible = false; toolNode->visible = false;
        act->up = traceUp; toolNode->up = traceUp;
        auto leg = [&](float len, float x0, float x1) {
            g3d::Material lm; lm.color = ROSE; lm.roughness = 0.4f; lm.castShadow = false;
            g3d::Object3D* l = scene->add(g3d::Geometry::cylinder(0.055f, len), lm);
            l->position = { (x0 + x1) / 2, PY_RDF + 0.14f, FC_Z };
            l->rotation = Vector3{ 0.f, 0.f, 90.f };
            l->visible = false;
            l->up = [](int t, g3d::Object3D& o) {
                float appear = (float)seg(t, B6 + 60, B6 + 130);
                float flash = ramp(t, B6 + 130, B6 + 160, B6 + 230, B6 + 290);   // mis-attributions surface
                float fixed = (float)seg(t, B6 + 230, B6 + 300);                 // corrected by provenance
                o.visible = appear > 0.02f;
                o.material.color = mixC(mixC(ROSE, mkColor(0xffd9a0), flash), TEAL, fixed);
                o.material.emissive = glow(fixed > 0.5f ? TEAL : ROSE, appear * 0.7f);
                o.scale = { appear, appear, appear };
            };
        };
        leg(std::fabs(ACT_X - TOOL_X), TOOL_X, ACT_X);
        leg(std::fabs(TOOL_X - FC_X), FC_X, TOOL_X);

        // Lights: key with shadows across the diagram, a restrained amber accent on the cloud only,
        // and a tight rose alarm light that stays on the gate instead of washing the stores.
        scene->lights.push_back(g3d::Light::directional({ -7, 12, 7 }, mkColor(0xdfe6ff), 0.9f, true));
        scene->lights.back().shadowExtent = 8;
        g3d::Light sweep = g3d::Light::spot({ PX_VEC + 2.6f, 3.4f, 3.2f }, { PX_VEC, PY_VEC + 0.6f, 0 }, AMBER, 0.85f, 34.f, false);
        sweep.range = 7;      // amber stays on the embedding store; it must not tint the others
        sweep.up = [](int t, g3d::Light& l) {
            float a = (float)(t * 0.014);
            l.target = { PX_VEC + 1.6f * std::cos(a), PY_VEC + 0.6f, 1.2f * std::sin(a) };
        };
        scene->lights.push_back(sweep);
        g3d::Light alarm = g3d::Light::point({ PX_RDF, GATE_Y + 0.4f, FRONT_Z }, ROSE, 0.f, 3.5f);
        alarm.up = [](int t, g3d::Light& l) {
            float rej = ramp(t, B3 + 40, B3 + 70, B3 + 130, B3 + 180);
            float fix = ramp(t, B6 + 130, B6 + 160, B6 + 230, B6 + 290);
            l.intensity = 0.9f * std::max(rej, fix);
        };
        scene->lights.push_back(alarm);

        scene->camera.fov = 46;
        scene->camera.dof.aperture = 0.3f; scene->camera.dof.maxBlur = 4;
        scene->camera.up = [](int t, g3d::Camera& cam) {
            float dist = (float)interpolate(spring((double)t, { FPS, { 1, 80, 9 } }), { 0, 1 }, { 24, 17 });
            float a = (float)interpolate((double)t, { 0, DUR }, { -0.35, 0.5 }, clamped(ease::inOut(ease::sine)));
            float y = (float)interpolate((double)t, { 0, DUR }, { 6.0, 5.0 }, clamped(ease::inOut(ease::cubic)));
            Vector3 f = focalAt(t);
            cam.position = { dist * std::cos(a), y + 0.12f * std::sin(t * 0.012f), dist * std::sin(a) };
            cam.target = { f.x * 0.35f + 1.1f, f.y * 0.55f + 0.9f, f.z * 0.35f };
            Vector3 d{ cam.position.x - f.x, cam.position.y - f.y, cam.position.z - f.z };
            cam.dof.focusDistance = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        };

        // ---------------- timeline ----------------
        addRoot(comp.roots, 0, DUR)->fn = [=](const Ctx& cx, int) {
            dr::rectGrad4(cx, 0, 0.f, (float)W, (float)H, FIELD_UP, FIELD_UP, FIELD, FIELD);
        };
        addRoot(comp.roots, 0, DUR)->fn = g3d::draw(scene);

        TNode* caps = addRoot(comp.roots, 0, DUR);
        caption(caps->kids, B1, B2 - B1 + 20, "", "Your AI agent forgets when the chat ends.",
                "Ask again: same experiment, three lookalike answers.");
        caption(caps->kids, B2, B3 - B2 + 20, "ONE MEMORY, THREE LAYERS", "Keep facts as facts, not prose.",
                "text  ·  graph  ·  embeddings  —  behind one API");
        caption(caps->kids, B3, B4 - B3 + 20, "WRITE PATH", "A shape check runs first.",
                "no confidence  ->  rejected before any layer stores it");
        caption(caps->kids, B4, B5 - B4 + 20, "RETRIEVAL", "Search finds candidates. The graph answers.",
                "semantic find  ->  graph enrich");
        caption(caps->kids, B5, B6 - B5 + 20, "EXACT VS FUZZY", "Typed queries answer precisely.",
                "\"which experiments are running?\"  ->  one exact set");
        caption(caps->kids, B6, END_FROM - B6 - 40, "PROVENANCE", "Every fact remembers who wrote it.",
                "three wrong attributions caught and corrected");

        chip(caps, B3 + 60, 120, "missing confidence", ROSE, { 1450.f, 150.f });
        chip(caps, B4 + 120, B5 - B4 - 80, "PPL 7.541  ·  step 406000", TEAL, { 1500.f, 220.f });
        chip(caps, B5 + 40, 140, "status = running", TEAL, { 1500.f, 220.f });

        // End card: one centred stack over a calm scrim.
        TNode* end = addRoot(comp.roots, END_FROM, DUR - END_FROM);
        end->up = [](int t, NodeProps& p) { p.opacity = (float)seg(t, 70, 130); };
        end->isolate();
        end->fn = [=](const Ctx& cx, int) {
            dr::rect(cx, 0, 0.f, (float)W, (float)H, mkColor(0x080d18, 200));
            Rectangle outer = layout::place(layout::fill(W, H), { 1200.f, 290.f }, Align::Center, Align::Center);
            auto stack = layout::column(outer, { layout::px(30.f), layout::px(150.f), layout::px(44.f) }, 18.f);
            TextStyle meta; meta.font = fonts::ui(false); meta.size = 26; meta.color = mkColor(0xdfe7f2, 240);
            meta.align = Align::Center;
            dr::textBox(cx, "single-user deployment snapshot: 83 entities, 921 statements, ~5 MB", stack[0], meta);
            TextStyle big; big.font = fonts::ui(true); big.size = 46; big.color = INK;
            big.align = Align::Center; big.valign = Align::Center; big.lineHeight = 1.2f;
            dr::textBox(cx, "Structured Knowledge Memory for Personal Research Agents", stack[1], big);
            TextStyle venue; venue.font = fonts::ui(false); venue.size = 30; venue.color = TEAL; venue.align = Align::Center;
            dr::textBox(cx, "KG-NeSy 2026  ·  co-located with ISWC 2026  ·  Bari  ·  Oct 25-26", stack[2], venue);
        };
    };
    out.push_back(std::move(c));
}

} // namespace rmtn

int main(int argc, const char** argv) { return rmtn::runCli(argc, argv, rmtn::registerMemoryPromo); }
