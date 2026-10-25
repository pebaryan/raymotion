// diagrams.cpp -- "diagrams": existing diagrams and structures, loaded from their own
// file formats and animated. Shows the dg:: and chem:: sides of the engine:
//   dg::load        Mermaid, Graphviz DOT, BPMN 2.0, Turtle / JSON-LD, Newick
//   dg::Diagram     build-up, traces, token flows, morphs, focus and dimming
//   chem::          SMILES / SDF skeletal formulas, 3D ball-and-stick, PDB protein cartoons
// The score (soft 120 bpm pop on Fmaj7-G-Em7-Am) is synthesised into out/ on first use.
//   raymotion render diagrams
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <filesystem>

namespace rmtn {
namespace {

constexpr int FPS = 30, XF = 20;
const Color INK = mkColor(0x1e2233), PAPER = mkColor(0xf7f5ef), SUB = mkColor(0x5d6274), ACCENT = mkColor(0xe2533f);
const Rectangle STAGE{ 80, 40, 1760, 760 };    // above the caption card
const std::string SCORE = "out/diagrams-assets-v1/score.wav";

Font bold() { return fonts::ui(true); }
Font regular() { return fonts::ui(false); }
Font mono() {
    static bool missing = false;
    if (!missing) {
        try { return fonts::load({ "consola.ttf", "CascadiaMono.ttf", "DejaVuSansMono.ttf", "Menlo.ttc" }); }
        catch (const std::exception&) { missing = true; }
    }
    return fonts::ui(false);
}

void paper(TNode* n, int len) {
    n->add(0, len)->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1920, 1080, PAPER); };
}

// Caption card: what is on screen, where it came from, and the line of code that does it.
void caption(TNode* n, int len, const char* title, const char* note, const char* code) {
    TNode* card = n->add(10, len - 10);
    card->up = [len](int t, NodeProps& p) {
        const float in = (float)interpolate(t, { 0, 18 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        const float out = (float)interpolate(t, { len - 34.0, len - 16.0 }, { 1, 0 }, clamped());
        p.opacity = in * out;
        p.pos = { 0, 24 * (1 - in) };
    };
    const std::string t = title, nt = note, c = code;
    card->fn = [t, nt, c](const Ctx& cx, int) {
        const float w = std::max({ MeasureTextEx(mono(), c.c_str(), 26, 0).x, MeasureTextEx(bold(), t.c_str(), 56, 0).x,
                                   MeasureTextEx(regular(), nt.c_str(), 28, 0).x }) + 80;
        dr::roundRect(cx, 64, 1080 - 48 - 180, w, 180, 18, Color{ 255, 255, 255, 250 });
        dr::text(bold(), cx, t.c_str(), { 104, 1080 - 48 - 152 }, 56, 0, INK, 0, 0);
        dr::text(regular(), cx, nt.c_str(), { 104, 1080 - 48 - 84 }, 28, 0, SUB, 0, 0);
        dr::text(mono(), cx, c.c_str(), { 104, 1080 - 48 - 44 }, 26, 0, INK, 0, 0);
    };
}

void titleScene(TNode* n, int len) {
    paper(n, len);
    chem::Style2D big;
    big.bondLength = 110;
    auto mol = chem::Depiction::make(chem::smiles("CN1C=NC2=C1C(=O)N(C(=O)N2C)C"), big);
    mol->build(10, 3, 14);
    mol->rotation = -12;
    n->add(0, len)->fn = chem::draw(mol, { 1080, 160, 760, 760 });
    TNode* t = n->add(6, len - 6);
    t->up = [](int f, NodeProps& p) {
        p.opacity = (float)interpolate(f, { 0, 20 }, { 0, 1 }, clamped());
        p.pos = { 0, 30 * (1 - p.opacity) };
    };
    t->fn = [](const Ctx& cx, int) {
        dr::text(bold(), cx, "Diagrams &", { 120, 330 }, 110, 0, INK, 0, 0);
        dr::text(bold(), cx, "structures", { 120, 440 }, 110, 0, ACCENT, 0, 0);
        dr::text(regular(), cx, "flowcharts, BPMN, knowledge graphs, trees, molecules, proteins", { 124, 580 }, 32, 0, SUB, 0, 0);
        dr::text(mono(), cx, "dg::load(\"order.bpmn\")   chem::loadProtein(\"1crn.pdb\")", { 124, 636 }, 28, 0, INK, 0, 0);
    };
}

void flowchartScene(TNode* n, int len) {
    paper(n, len);
    auto d = dg::Diagram::make(dg::load("data/support.mmd"));
    d->build(8, 7, 16);
    d->trace(120, 70, { "report", "triage", "sev", "fix", "review", "deploy", "close" });
    d->highlight(150, 14, { "Engineering" }, mkColor(0x3d7be0));
    n->add(0, len)->fn = dg::draw(d, STAGE);
    caption(n, len, "Mermaid flowchart", "shapes, subgraphs, classDef and labelled links, laid out in layers",
            "dg::Diagram::make(dg::load(\"support.mmd\"))->build(8).trace(120, 70, path);");
}

void dotScene(TNode* n, int len) {
    paper(n, len);
    dg::Theme theme = dg::Theme::blueprint();
    auto d = dg::Diagram::make(dg::load("data/pipeline.dot"), theme);
    d->build(8, 8, 16);
    d->morph(70, 40, dg::load("data/pipeline-full.dot"));
    d->flow(130, 90, { "checkout", "lint", "build", "test", "itest", "stage" }, 2);
    n->add(0, len)->fn = [d](const Ctx& cx, int t) {
        dr::rect(cx, 0, 0, 1920, 1080, mkColor(0x14305a));
        d->draw(cx, t, STAGE);
    };
    caption(n, len, "Graphviz DOT, morphing", "a second version of the graph: kept nodes glide, new ones arrive",
            "d->morph(70, 40, dg::load(\"pipeline-full.dot\"));  d->flow(130, 90, stages, 2);");
}

void bpmnScene(TNode* n, int len) {
    paper(n, len);
    auto d = dg::Diagram::make(dg::load("data/order.bpmn"), dg::Theme::bpmn());
    d->build(8, 4, 14);
    d->trace(110, 100, { "received", "check", "stock", "split", "pack", "join", "ship", "shipped" }, mkColor(0x1f9d6b));
    d->flow(110, 100, { "split" }, 1, mkColor(0x1f9d6b));
    n->add(0, len)->fn = dg::draw(d, STAGE);
    caption(n, len, "BPMN 2.0", "pools, lanes, events, gateways and tasks, placed by the file's own diagram interchange",
            "auto d = dg::Diagram::make(dg::load(\"order.bpmn\"), dg::Theme::bpmn());");
}

void knowledgeScene(TNode* n, int len) {
    paper(n, len);
    auto d = dg::Diagram::make(dg::load("data/curie.ttl"));
    d->build(8, 4, 14);
    const std::vector<std::string> marie = { "Marie Curie", "Pierre Curie", "Polonium", "Radium", "Warsaw", "Sorbonne",
                                             "Nobel Physics 1903", "Nobel Chemistry 1911", "Irène Joliot-Curie" };
    d->dim(110, 20, marie);
    d->focus(110, 36, { "Marie Curie" }, 1.6f);
    d->highlight(120, 16, { "Marie Curie" });
    d->focus(200, 36, {});
    d->dim(200, 20, {});
    n->add(0, len)->fn = dg::draw(d, STAGE);
    caption(n, len, "Knowledge graph (Turtle)", "labels from rdfs:label, colours by rdf:type, force-directed layout",
            "d->dim(110, 20, neighbours).focus(110, 36, { \"Marie Curie\" }, 1.6f);");
}

void treeScene(TNode* n, int len) {
    paper(n, len);
    auto d = dg::Diagram::make(dg::load("data/primates.nwk"));
    d->build(8, 3, 12);
    d->highlight(120, 16, { "Human", "Chimpanzee", "Bonobo" }, ACCENT);
    n->add(0, len)->fn = dg::draw(d, STAGE);
    caption(n, len, "Phylogenetic tree (Newick)", "branch lengths along the axis, tips in order",
            "dg::Diagram::make(dg::load(\"primates.nwk\"))->build(8, 3);");
}

void moleculeScene(TNode* n, int len) {
    paper(n, len);
    chem::Style2D big;
    big.bondLength = 96;
    auto flat = chem::Depiction::make(chem::smiles("CN1C=NC2=C1C(=O)N(C(=O)N2C)C caffeine"), big);
    flat->build(8, 3, 12);
    for (const auto& m : chem::find(flat->molecule, "C=O")) flat->highlight(110, 16, m);
    n->add(0, len)->fn = chem::draw(flat, { 80, 60, 820, 740 });

    auto scene = std::make_shared<g3d::Scene>();
    scene->camera.position = { 0, 0, 15 };
    scene->lights.push_back(g3d::Light::directional({ 4, 6, 8 }, WHITE, 1.3f));
    g3d::Object3D* mol = chem::addMolecule(*scene, chem::loadMolecule("data/caffeine.sdf"));
    chem::buildIn(mol, 20, 2, 12);
    mol->up = [](int t, g3d::Object3D& o) { o.rotation = { 18, t * 0.9f, 0 }; };
    n->add(0, len)->fn = g3d::draw(scene, { 960, 40, 900, 780 });
    n->add(0, len)->fn = [](const Ctx& cx, int) {
        dr::text(regular(), cx, "SMILES, 2D", { 490, 790 }, 28, 0, SUB, 0.5f, 0);
        dr::text(regular(), cx, "SDF, 3D", { 1410, 790 }, 28, 0, SUB, 0.5f, 0);
    };
    caption(n, len, "Molecules", "skeletal formula from SMILES, ball-and-stick from an SDF file, built atom by atom",
            "chem::addMolecule(*scene, chem::loadMolecule(\"caffeine.sdf\"));  chem::buildIn(mol, 20);");
}

void proteinScene(TNode* n, int len) {
    n->add(0, len)->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1920, 1080, mkColor(0x101420)); };
    auto scene = std::make_shared<g3d::Scene>();
    scene->camera.position = { 0, 0, 34 };
    scene->lights.push_back(g3d::Light::directional({ 6, 8, 10 }, WHITE, 1.3f));
    scene->skyColor = mkColor(0xb8c4e8);
    scene->groundColor = mkColor(0x302838);
    const chem::Protein crn = chem::loadProtein("data/1crn.pdb");
    g3d::Object3D* cart = chem::addCartoon(*scene, crn);
    chem::buildIn(cart, 10, 2, 10);
    cart->up = [](int t, g3d::Object3D& o) { o.rotation = { 10, t * 0.7f, 0 }; };
    n->add(0, len)->fn = g3d::draw(scene, { 0, 0, 1920, 860 });
    TNode* card = n->add(10, len - 10);
    card->up = [len](int t, NodeProps& p) {
        p.opacity = (float)(interpolate(t, { 0, 18 }, { 0, 1 }, clamped()) * interpolate(t, { len - 34.0, len - 16.0 }, { 1, 0 }, clamped()));
    };
    card->fn = [](const Ctx& cx, int) {
        dr::text(bold(), cx, "Protein cartoon (PDB)", { 104, 1080 - 48 - 152 }, 56, 0, mkColor(0xf2f4ff), 0, 0);
        dr::text(regular(), cx, "crambin, 1CRN: helices as ribbons, strands as arrows, N- to C-terminus in rainbow",
                 { 104, 1080 - 48 - 84 }, 28, 0, mkColor(0xa8b0cc), 0, 0);
        dr::text(mono(), cx, "chem::addCartoon(*scene, chem::loadProtein(\"1crn.pdb\"));", { 104, 1080 - 48 - 44 }, 26, 0, mkColor(0xf2f4ff), 0, 0);
    };
}

// The score, in seconds: chapters start at 3.3 (flowchart), 11.3 (DOT), 18.7 (BPMN), 26.3
// (knowledge graph), 34.7 (tree), 40.7 (molecules) and 48 (protein); each gets a bell, and
// the traces and the morph get their own figures.
void genScore(const std::string& path) {
    using procgen::Score;
    const double END = 56;
    Score s(END);
    struct Chord { int root; int pad[3]; int arp[4]; };
    const Chord fmaj7{ 41, { 64, 69, 72 }, { 65, 69, 72, 76 } }, g{ 43, { 62, 67, 71 }, { 67, 71, 74, 79 } },
                em7{ 40, { 62, 67, 71 }, { 64, 67, 71, 74 } }, am{ 45, { 60, 64, 69 }, { 69, 72, 76, 72 } },
                c{ 36, { 60, 64, 67 }, { 72, 76, 79, 84 } };
    const Chord prog[4] = { fmaj7, g, em7, am };
    // Intro: pads, and plucks as caffeine's atoms appear.
    for (int p : fmaj7.pad) s.pad(0, 1.9, p, 0.10f, 900);
    for (int p : g.pad) s.pad(2, 1.9, p, 0.10f, 1000);
    for (int i = 0; i < 12; i++) s.pluck(0.35 + i * 0.25, prog[i / 8].arp[i % 4] + 12, 0.07f, i % 2 ? 0.3f : 0.7f);
    // Bars from 4 s to 48 s; the band joins in layers.
    for (double t = 4; t < 48; t += 2) {
        const Chord& ch = prog[(int)(t / 2) % 4];
        const bool drums = t >= 12, full = t >= 20;
        for (int p : ch.pad) s.pad(t, 1.9, p, 0.08f, full ? 1600 : 1100);
        for (int e = 0; e < 8; e++) s.pluck(t + e * 0.25, ch.arp[e % 4], e % 4 == 0 ? 0.09f : 0.06f, e % 2 ? 0.3f : 0.7f);
        for (int e = 0; e < 4; e++) s.bass(t + e * 0.5, 0.4, ch.root + (e == 3 ? 12 : 0), drums ? 0.26f : 0.18f);
        if (drums) for (int b = 0; b < 4; b++) s.kick(t + b * 0.5, full ? 0.6f : 0.45f);
        if (full) { s.clap(t + 0.5, 0.22f); s.clap(t + 1.5, 0.22f); }
        if (t >= 8) for (int h = 0; h < 8; h++) s.hat(t + h * 0.25, h % 2 ? 0.10f : 0.06f, h % 2 ? 0.35f : 0.65f);
    }
    // A bell on each chapter change.
    const double chapters[] = { 3.33, 11.33, 18.67, 26.33, 34.67, 40.67 };
    for (int i = 0; i < 6; i++) s.bell(chapters[i], 79 + (i % 2) * 5, 0.10f, 0.5f);
    // Flowchart trace: seven nodes, climbing.
    const int up[7] = { 72, 74, 76, 79, 81, 84, 88 };
    for (int i = 0; i < 7; i++) s.bell(7.33 + i * 0.39, up[i], 0.06f, 0.3f + 0.07f * i);
    // DOT morph: a short swell into the new graph.
    s.riser(12.9, 13.67, 0.12f);
    // BPMN token: eight steps.
    for (int i = 0; i < 8; i++) s.bell(22.33 + i * 0.42, up[i % 7] - 5 + (i == 7 ? 12 : 0), 0.05f, 0.7f - 0.05f * i);
    // Knowledge graph: the camera dives in on Marie Curie.
    s.riser(29.2, 30, 0.10f);
    s.impact(30, 0.25f);
    // Protein: drums drop out, a lift to C major under the growing chain.
    const Chord lift[2] = { fmaj7, g };
    for (int i = 0; i < 2; i++) {
        const double t = 48 + i * 2;
        for (int p : lift[i].pad) { s.pad(t, 1.9, p, 0.11f, 2000); }
        s.bass(t, 1.9, lift[i].root, 0.22f);
        for (int e = 0; e < 8; e++) s.pluck(t + e * 0.25, lift[i].arp[e % 4] + (e >= 4 ? 12 : 0), 0.07f, e % 2 ? 0.3f : 0.7f);
    }
    s.bell(48, 84, 0.10f, 0.5f);
    for (int p : c.pad) { s.pad(52, 2.6, p, 0.13f, 2200, 0.35f); s.pad(52, 2.6, p - 12, 0.09f, 1200, 0.65f); }
    s.bass(52, 3, c.root, 0.26f);
    for (int i = 0; i < 4; i++) s.bell(52 + i * 0.5, c.arp[i], 0.09f, 0.2f + 0.2f * i);
    media::writeWav(path, s.mix(), Score::RATE);           // (mix fades the last 3 s)
}

} // namespace

void registerDiagrams(Registry& out) {
    Comp c;
    c.name = "diagrams";
    c.w = 1920; c.h = 1080; c.fps = FPS;
    c.background = PAPER;
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    titleScene(ts.add(120), 120);
    const std::pair<void (*)(TNode*, int), int> scenes[] = {
        { flowchartScene, 260 }, { dotScene, 240 }, { bpmnScene, 250 }, { knowledgeScene, 270 },
        { treeScene, 200 }, { moleculeScene, 240 }, { proteinScene, 240 },
    };
    int i = 0;
    for (const auto& [make, len] : scenes) {
        ts.transition(i++ % 2 ? Transition::slide(XF, Direction::FromRight) : Transition::wipe(XF, Direction::FromLeft));
        make(ts.add(len), len);
    }
    c.duration = ts.end();
    c.assets = [](const Comp&, const Json&) {
        if (std::filesystem::exists(SCORE)) return;
        std::filesystem::create_directories(std::filesystem::path(SCORE).parent_path());
        genScore(SCORE);
    };
    media::audio(addRoot(c.roots, 0, c.duration), SCORE);
    out.push_back(std::move(c));
}

} // namespace rmtn
