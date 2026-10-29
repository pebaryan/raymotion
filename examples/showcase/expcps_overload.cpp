// The Overload Has a Backstory: a source-grounded explainer of Aryan et al. (2021).
// Brief, script, and source ledger: docs/expcps-2021-explainer-brief.md.
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace rmtn { namespace {

constexpr int W = 1920, H = 1080, FPS = 30, N = 7;
const Color BG = mkColor(0x0b1220), PANEL = mkColor(0x16243a), LINE = mkColor(0x344a61);
const Color INK = mkColor(0xf2f5fc), DIM = mkColor(0xa3b5c9), ALARM = mkColor(0xff6570);
const Color AMBER = mkColor(0xffc365), WEATHER = mkColor(0x69bbeb), TEAL = mkColor(0x56d6bd);

const char* const IDS[N] = { "01-alarm", "02-price", "03-rain", "04-stitch", "05-walk", "06-result", "07-close" };
const char* const HEAD[N] = {
    "WHY IS THE TRANSFORMER LOADED?", "A PRICE SIGNAL CHANGES DEMAND", "RAIN CUTS SOLAR OUTPUT",
    "A RULE BECOMES A LOCAL LINK", "FROM ALARM TO PRIOR EVENTS", "WHAT THE PROTOTYPE SHOWED",
    "AN ALARM HAS A BACKSTORY"
};
const char* const CAP[N][4] = {
    { "An alarm is a symptom.", "It does not tell us why.", "Scenario threshold: 60% load.", "" },
    { "One trail starts outside the power grid.", "A price drop triggers a message.", "Two homes use more electricity.", "" },
    { "Another trail starts on a roof.", "Rain reduces solar generation.", "The battery is empty; grid draw rises.", "Both trails reach the transformer." },
    { "ExpCPS joins facts in a knowledge graph.", "Experts provide a general rule.", "Topology grounds it in actual devices.", "" },
    { "Start at the alarm.", "Find earlier events on connected devices.", "Build a candidate explanation tree.", "" },
    { "Data-grid messages appear in the result.", "Related paths still need grouping.", "A demonstrated prototype, not a complete account.", "" },
    { "From warning to traceable evidence.", "Aryan et al. / MSCPES 2021", "", "" }
};

struct Chapter {
    int start = 0, dur = 0, voiceFrames = 0;
    std::vector<int> cue;
    std::vector<double> cueSeconds;
    std::vector<double> cueDurations;
    std::vector<std::string> sentences;
};
struct Plan { std::array<Chapter, N> ch; int total = 0; };

std::string timingFile() {
    try { return media::resolve("vo/expcps-overload/timing.json"); }
    catch (...) { return ""; }  // list still reports metadata when generated voice is absent
}

Plan loadPlan() {
    Plan p;
    const int fallback[N] = { 13, 14, 14, 16, 15, 18, 12 };
    const std::string file = timingFile();
    Json data;
    if (!file.empty()) {
        std::ifstream in(file);
        const std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (!raw.empty()) data = Json::parse(raw);
    }
    const Json& chapters = data["chapters"];
    for (int i = 0; i < N; ++i) {
        Chapter& ch = p.ch[i];
        const Json& src = chapters[(size_t)i];
        const double voice = src.num("duration", fallback[i]);
        ch.voiceFrames = std::max(1, (int)std::ceil(voice * FPS));
        ch.dur = ch.voiceFrames + (i == N - 1 ? 56 : 38);
        ch.start = p.total;
        p.total += ch.dur;
        if (src["cues"].isArray()) {
            for (const Json& cue : src["cues"].asArray()) {
                ch.cueSeconds.push_back(cue.num("start", 0));
                ch.cueDurations.push_back(cue.num("duration", 0));
                ch.cue.push_back((int)std::round(ch.cueSeconds.back() * FPS));
                ch.sentences.push_back(cue.str("text", ""));
            }
        }
        if (ch.cue.empty()) ch.cue = { 0, ch.voiceFrames / 3, 2 * ch.voiceFrames / 3 };
    }
    return p;
}

float k(int t, int from, int span = 16) {
    const float u = std::clamp((t - from) / (float)std::max(1, span), 0.f, 1.f);
    return u * u * (3 - 2 * u);
}
int cueAt(const Chapter& ch, int t) {
    int active = 0;
    for (int i = 0; i < (int)ch.cue.size(); ++i) if (t >= ch.cue[i]) active = i;
    return active;
}
Ctx faded(const Ctx& c, float alpha) { Ctx x = c; x.a *= std::clamp(alpha, 0.f, 1.f); return x; }
void label(const Ctx& c, const char* s, Vector2 p, int size, Color col, double ax = 0, double ay = 0.5) {
    dr::text(fonts::ui(false), c, s, p, (float)size, 0, col, ax, ay);
}
void bold(const Ctx& c, const char* s, Vector2 p, int size, Color col, double ax = 0, double ay = 0.5) {
    dr::text(fonts::ui(true), c, s, p, (float)size, 0, col, ax, ay);
}
void trace(const Ctx& c, Vector2 a, Vector2 b, Color col, float u, float width = 7) {
    if (u <= 0) return;
    dr::strokeSeg(c, a, mixV(a, b, std::clamp(u, 0.f, 1.f)), width, col);
}
void dot(const Ctx& c, Vector2 p, Color col, float r = 10) { dr::circle(c, p, r, col); }
void panel(const Ctx& c, float x, float y, float w, float h) {
    dr::roundRect(c, x, y, w, h, 24, PANEL);
}
void house(const Ctx& c, Vector2 at, const char* id, Color roof = DIM, float scale = 1) {
    Ctx m = c;
    m.o = mapP(c, at);
    m.s *= scale;
    dr::rect(m, -52, -22, 104, 75, mkColor(0x25364d));
    Vector2 r[3] = { {-62,-22}, {0,-68}, {62,-22} };
    dr::fillPoly(m, r, 3, roof);
    dr::rect(m, -14, 13, 28, 40, mkColor(0x0e192b));
    dr::rect(m, -40, -4, 18, 18, WEATHER);
    label(m, id, {0,76}, 26, INK, 0.5);
}
void transformer(const Ctx& c, Vector2 at, float scale = 1) {
    Ctx m = c; m.o = mapP(c, at); m.s *= scale;
    dr::roundRect(m, -61, -58, 122, 116, 16, mkColor(0x31455a));
    dr::rect(m, -18, -67, 36, 17, ALARM);
    for (int j = -1; j <= 1; ++j) dr::strokeSeg(m, {-40.f, (float)j * 22}, {40.f, (float)j * 22}, 5, INK);
}
void frame(const Ctx& c, int chapter, int t, const Chapter& ch, int total) {
    dr::rect(c, 0, 0, W, H, BG);
    dr::rect(c, 96, 52, 1728, 3, LINE);
    dr::rect(c, 96, 52, 1728.f * (ch.start + t) / total, 3, TEAL);
    const std::string no = "0" + std::to_string(chapter + 1) + " / 07";
    label(c, no.c_str(), {105,115}, 25, TEAL);
    bold(c, HEAD[chapter], {105,190}, chapter == 6 ? 68 : 62, INK);
    label(c, chapter < 4 ? "SIMULATED FLEX SCENARIO" : chapter < 6 ? "EXPCPS METHOD / 2021 PAPER" : "ARYAN ET AL. / MSCPES 2021",
          {1810,115}, 25, DIM, 1);
    const int ci = std::min(3, cueAt(ch, t));
    const int cueStart = ci < (int)ch.cue.size() ? ch.cue[ci] : 0;
    Ctx cap = faded(c, k(t, cueStart, 9));
    dr::roundRect(cap, 104, 868, 1712, 133, 18, mkColor(0x20334a));
    bold(cap, CAP[chapter][ci], {145,934}, 43, INK);
    label(c, "EXPLAINABLE CYBER-PHYSICAL ENERGY SYSTEMS BASED ON KNOWLEDGE GRAPH / DOI 10.1145/3470481.3472704",
          {106,1040}, 21, DIM);
    const float exit = 1.f - k(t, ch.dur - 14, 14);
    if (exit > 0) dr::rect(c, 0, 0, W, H, withAlpha(BG, 1.f - exit));
}

g3d::ScenePtr flexCity(const city::Model& model, bool rising) {
    auto g = std::make_shared<g3d::Scene>();
    g->skyColor = mkColor(0x182b45);
    g->groundColor = mkColor(0x263c48);
    g->background = mkColor(0x16243a);
    g->ambient = 0.75f;
    g3d::Light sun = g3d::Light::directional({ 95, 150, 75 }, mkColor(0xdbefff), 1.1f, true);
    sun.shadowExtent = 150;
    g->lights.push_back(sun);
    city::Look look;
    look.terrain = mkColor(0x31564d);
    look.ground = mkColor(0x31564d);
    look.road = mkColor(0x455666);
    look.roof = mkColor(0x8199aa);
    look.wall = mkColor(0xb8c9d4);
    look.window = mkColor(0x83c4d9);
    look.furniture = mkColor(0x72899b);
    look.color = [](const city::Object& o, const city::Face& f) {
        if (o.id == "lane-markings") return mkColor(0xb5ad84);
        if (o.id == "walks" || o.id == "utility-walk") return mkColor(0x8b9c9b);
        if (o.id == "T1") return ALARM;
        if (o.id.rfind("T1-",0) == 0) return mkColor(0xaabcc2);
        if (o.id.rfind("fence-",0) == 0) return mkColor(0x8ea7a8);
        if (o.id.rfind("tree-crown",0) == 0) return mkColor(0x5a987c);
        if (o.id.rfind("tree-trunk",0) == 0) return mkColor(0x695a4e);
        if (o.id == "solar-H4") return mkColor(0x174767);
        if (o.id.rfind("floorbands",0) == 0) return mkColor(0x638499);
        if (o.id.rfind("windows-",0) == 0) return mkColor(0x75b7d0);
        if (o.id == "service-unit") return mkColor(0x436f7c);
        if (o.type != "Building" || f.kind == "GroundSurface") return BLANK;
        const std::string kind = o.attributes.str("buildingType", "");
        if (kind == "apartment") return f.kind == "RoofSurface" ? mkColor(0x708498) : mkColor(0xb9c7c6);
        if (kind == "service") return f.kind == "RoofSurface" ? mkColor(0x5c7f86) : mkColor(0x98b5ae);
        const Color roof = (o.id == "H1" || o.id == "H2") ? AMBER : o.id == "H4" ? WEATHER : mkColor(0xc07767);
        return f.kind == "RoofSurface" ? roof : mkColor(0xe1d6c0);
    };
    g3d::Material flat; flat.roughness = 0.95f; flat.castShadow = false;
    g->add(city::mesh(model.filter([](const city::Object& o) { return o.type == "LandUse" || o.type == "Road"; }),look),flat);
    g3d::Material m; m.roughness = 0.8f;
    auto* group = city::add(*g,model.filter([](const city::Object& o) { return o.type != "LandUse" && o.type != "Road"; }),look,m);
    if (rising) city::rise(group, 8, 2.1f, 28);
    const BoundingBox bounds = model.bounds();
    g->camera.fov = 34;
    g->camera.up = [bounds](int t, g3d::Camera& cam) {
        city::frame(cam,bounds,213 + t * 0.035f,39,-0.08f,1.75f);
        cam.position = Vector3Add(cam.target,Vector3Scale(Vector3Subtract(cam.position,cam.target),0.86f));
    };
    if (!rising) {
        const Vector3 destination = city::base(model.get("T1"));
        const char* homes[3] = {"H1","H2","H4"};
        const float lanes[3] = {-7.4f,-6.2f,-4.7f};
        const float turns[3] = {60.5f,61.5f,62.5f};
        for (int i=0;i<3;++i) {
            const Vector3 from=city::base(model.get(homes[i]));
            const std::vector<Vector3> path{
                {from.x,from.y,0.3f}, {from.x,lanes[i],0.3f},
                {turns[i],lanes[i],0.3f}, {turns[i],-11.2f,0.3f},
                {destination.x,-11.2f,0.3f}, {destination.x,destination.y,0.3f}
            };
            city::TrailStyle route;
            route.color = i == 2 ? WEATHER : AMBER;
            route.width=i == 2 ? 1.05f : 0.82f; route.thickness=0.10f; route.lift=0.28f;
            city::trail(*g,path,[i](int t) { return k(t,10+i*10,105); },route);
        }
    }
    return g;
}

void alarmScene(const Ctx& c, int t, const Chapter& ch, const g3d::ScenePtr& cityScene) {
    panel(c, 105, 285, 760, 525);
    bold(c, "T1 / TRANSFORMER", {150,340}, 31, ALARM);
    transformer(c, {340,535}, 1.4f);
    label(c, "LOAD", {545,430}, 28, DIM);
    const float fill = 310.f * (0.28f + 0.46f * k(t, ch.cue[2], 40));
    dr::roundRect(c, 545, 485, 310, 42, 20, mkColor(0x304055));
    dr::roundRect(c, 545, 485, fill, 42, 20, ALARM);
    const float threshold = 545 + 310 * 0.6f;
    dr::strokeSeg(c, {threshold,465}, {threshold,555}, 4, INK);
    label(c, "60%", {threshold,580}, 26, INK, 0.5);
    label(c, "scenario threshold", {545,650}, 27, DIM);
    panel(c, 925, 285, 890, 525);
    g3d::render(*cityScene,t,c,{950,365,840,345});
    label(c, "FLEX / 10 BUILDINGS", {970,332}, 29, INK);
    label(c, "PITCHED HOUSES / FLAT APARTMENTS", {1785,336}, 19, DIM, 1);
    dot(c,{991,740},AMBER,8); label(c,"H1 / H2  PRICE",{1012,740},23,DIM);
    dot(c,{1282,740},WEATHER,8); label(c,"H4  SOLAR",{1303,740},23,DIM);
    dot(c,{1520,740},ALARM,8); label(c,"T1  ALARM",{1541,740},23,DIM);
    label(c, "ILLUSTRATIVE CITYJSON MODEL", {1784,785}, 21, DIM, 1);
}

void priceScene(const Ctx& c, int t, const Chapter& ch) {
    const float msg = k(t, ch.cue[1], 28), demand = k(t, ch.cue[2], 32);
    panel(c, 105, 340, 395, 360); panel(c, 650, 340, 430, 360); panel(c, 1240, 305, 575, 440);
    label(c, "ENERGY MARKET", {302,394}, 28, DIM, 0.5);
    bold(c, "PRICE", {302,494}, 40, INK, 0.5);
    bold(c, "DOWN", {302,560}, 68, AMBER, 0.5);
    trace(c, {500,520}, {650,520}, AMBER, msg, 8);
    label(c, "DATA GRID", {575,475}, 25, AMBER, 0.5);
    label(c, "AGGREGATOR", {865,394}, 28, DIM, 0.5);
    dr::roundRect(c, 763,465,205,125,20,mkColor(0x42536a));
    bold(c, "FLEX", {865,528}, 43, INK, 0.5);
    trace(c, {1080,520}, {1240,520}, AMBER, msg, 8);
    house(c, {1395,472}, "H1", AMBER, 0.72f);
    house(c, {1655,472}, "H2", AMBER, 0.72f);
    for (int i = 0; i < 2; ++i) {
        const float x = 1315 + 260 * i;
        label(c, "USE", {x,632}, 23, DIM);
        dr::rect(c,x+72,620,96,17,mkColor(0x344454));
        dr::rect(c,x+72,620,96 * (0.35f + 0.6f * demand),17,AMBER);
    }
    trace(c, {1525,745}, {1525,800}, AMBER, demand, 8);
    label(c, "MORE POWER FROM THE GRID", {1525,800}, 27, AMBER, 0.5);
    label(c, "SCENARIO EVENTS e1 / e3 / e4 / e5", {116,806}, 23, DIM);
}

void rainScene(const Ctx& c, int t, const Chapter& ch) {
    const float rain = k(t, ch.cue[1], 25), grid = k(t, ch.cue[2], 25), join = k(t, ch.cue[3], 25);
    panel(c, 105, 305, 1710, 500);
    house(c, {495,555}, "H4 / SOLAR HOUSE", WEATHER, 1.52f);
    for (int i = 0; i < 9; ++i) {
        const float x = 285 + (i % 5) * 90.f, y = 365 + (i / 5) * 41.f;
        trace(faded(c, rain), {x,y}, {x-18,y+36}, WEATHER, rain, 3);
    }
    label(c, "SOLAR", {870,410}, 26, DIM);
    dr::rect(c, 870,450,270,24,mkColor(0x31465d));
    dr::rect(c, 870,450,270 * (1.f - 0.78f * rain),24,WEATHER);
    label(c, "BATTERY", {870,545}, 26, DIM);
    dr::roundRect(c, 870,585,270,95,12,mkColor(0x334458));
    bold(c, "EMPTY", {1005,633}, 39, INK, 0.5);
    trace(c, {1160,630}, {1390,630}, WEATHER, grid, 8);
    transformer(c, {1490,605}, 1.2f);
    label(c, "GRID DRAW", {1490,730}, 28, WEATHER, 0.5);
    trace(c, {1460,740}, {1560,740}, AMBER, join, 5);
    label(c, "TWO TRAILS / ONE TRANSFORMER", {116,805}, 25, DIM);
}

void stitchScene(const Ctx& c, int t, const Chapter& ch) {
    const float rule = k(t, ch.cue[1], 24), topo = k(t, ch.cue[2], 28);
    panel(c, 105, 310, 485, 450); panel(c, 710, 310, 505, 450); panel(c, 1350, 310, 465, 450);
    label(c, "GENERAL RULE", {150,375}, 26, DIM);
    bold(c, "POWER", {347,475}, 48, INK, 0.5);
    trace(c, {270,540}, {425,540}, TEAL, rule, 7);
    bold(c, "LOAD", {347,615}, 48, INK, 0.5);
    label(c, "EXPERT KNOWLEDGE", {150,710}, 23, TEAL);
    label(c, "ACTUAL TOPOLOGY", {755,375}, 26, DIM);
    house(c, {835,555}, "METER", DIM, 0.7f);
    transformer(c, {1085,555}, 0.8f);
    trace(c, {895,555}, {1028,555}, TEAL, topo, 6);
    label(c, "CONNECTED?", {964,675}, 24, TEAL, 0.5);
    label(c, "LOCAL LINK", {1395,375}, 26, DIM);
    dot(c, {1445,540}, TEAL, 18);
    trace(c, {1465,540}, {1690,540}, TEAL, topo, 8);
    dot(c, {1710,540}, TEAL, 18);
    bold(c, "ACCEPTED", {1580,660}, 39, TEAL, 0.5);
    label(c, "RULE  x  TOPOLOGY  =  DEVICE-SPECIFIC RELATION", {960,817}, 29, INK, 0.5);
}

dg::Theme eventTheme() {
    dg::Theme th = dg::Theme::dark();
    th.fontSize=27; th.padX=18; th.padY=12;
    th.fill=mkColor(0x274057); th.stroke=mkColor(0x5f8b9a); th.text=INK;
    th.edge=TEAL; th.edgeWidth=4; th.arrowSize=14; th.highlight=TEAL;
    th.colorKinds=false;
    return th;
}

dg::Node& eventNode(dg::Graph& g, const char* id, const char* title, float x, float y, Color fill) {
    auto& n = g.addNode(id,title,dg::Shape::Rounded);
    n.fixed=true; n.pos={x,y}; n.size={250,92};
    n.style.fill=fill; n.style.stroke=mixC(fill,INK,0.35f); n.style.text=INK;
    return n;
}

dg::DiagramPtr walkDiagram(const Chapter& ch) {
    dg::Graph g; g.layout=dg::LayoutKind::None; g.direction=dg::Direction::RL;
    eventNode(g,"overload","e7 / OVERLOAD",1370,205,mkColor(0x6d3548));
    eventNode(g,"transformer","T1 / POWER",1040,90,mkColor(0x31556a));
    eventNode(g,"home","HOME POWER",710,205,mkColor(0x31556a));
    eventNode(g,"message","FLEX MESSAGE",380,90,mkColor(0x846344));
    for (auto [from,to] : {std::pair{"overload","transformer"}, {"transformer","home"}, {"home","message"}}) {
        const auto* a=g.node(from); const auto* b=g.node(to);
        const float start=a->pos.x-a->size.x/2, end=b->pos.x+b->size.x/2, mid=(start+end)/2;
        auto& e=g.addEdge(from,to); e.style.stroke=TEAL; e.width=1.5f; e.curved=false; e.fixed=true;
        e.points={{start,a->pos.y},{mid,a->pos.y},{mid,b->pos.y},{end,b->pos.y}};
    }
    auto d=dg::Diagram::make(std::move(g),eventTheme());
    d->maxZoom=1.0f; d->margin=10;
    d->build(8,24,18);
    d->trace(ch.cue[1],std::max(64,ch.cue[2]-ch.cue[1]),{"overload","transformer","home","message"},TEAL);
    return d;
}

dg::DiagramPtr resultDiagram(const Chapter& ch) {
    dg::Graph g; g.layout=dg::LayoutKind::None; g.direction=dg::Direction::TB;
    auto& root=eventNode(g,"load","TRANSFORMER LOAD",450,55,mkColor(0x6d3548)); root.size={295,82};
    auto& a=eventNode(g,"powerA","POWER A",240,190,mkColor(0x31556a)); a.size={190,72};
    auto& b=eventNode(g,"powerB","POWER B",660,190,mkColor(0x31556a)); b.size={190,72};
    auto& c=eventNode(g,"flexA","FLEX MESSAGE",190,320,mkColor(0x846344)); c.size={235,72};
    auto& e=eventNode(g,"flexB","FLEX MESSAGE",710,320,mkColor(0x846344)); e.size={235,72};
    for (auto [from,to] : {std::pair{"load","powerA"}, {"load","powerB"}, {"powerA","flexA"}, {"powerB","flexB"}}) {
        const auto* upper=g.node(from); const auto* lower=g.node(to);
        const float start=upper->pos.y+upper->size.y/2, end=lower->pos.y-lower->size.y/2, mid=(start+end)/2;
        auto& edge=g.addEdge(from,to); edge.style.stroke=TEAL; edge.width=1.3f; edge.curved=false; edge.fixed=true;
        edge.points={{upper->pos.x,start},{upper->pos.x,mid},{lower->pos.x,mid},{lower->pos.x,end}};
    }
    auto d=dg::Diagram::make(std::move(g),eventTheme());
    d->maxZoom=1.0f; d->margin=8;
    d->build(12,16,16);
    d->trace(ch.cue[0]+58,90,{"load","powerA","flexA"},AMBER);
    return d;
}

void walkScene(const Ctx& c, int t, const Chapter& ch, const dg::DiagramPtr& diagram) {
    panel(c, 105, 310, 1710, 450);
    label(c, "EXPLANATION SEARCH / BACKWARD IN TIME", {155,365}, 28, DIM);
    diagram->draw(c,t,{140,410,1630,270});
    label(c, "EARLIER EVENTS", {300,710}, 23, DIM, 0.5);
    label(c, "ALARM EVENT", {1500,710}, 23, ALARM, 0.5);
    dr::strokeSeg(c, {335,750}, {1535,750}, 3, LINE);
    label(c, "POTENTIAL CAUSES / TIME FILTERED", {960,818}, 27, DIM, 0.5);
}

void resultScene(const Ctx& c, int t, const Chapter& ch, const dg::DiagramPtr& diagram) {
    const float caveat = k(t, ch.cue[1], 20), last = k(t, ch.cue[2], 20);
    panel(c, 105, 305, 930, 525); panel(c, 1075, 305, 740, 525);
    label(c, "RECREATED FROM PAPER FIGURE 9", {150,353}, 26, DIM);
    diagram->draw(c,t,{155,395,820,365});
    label(c, "PROTOTYPE RESULT", {1125,353}, 26, DIM);
    bold(c, "R1  +  DATA GRID", {1130,455}, 39, TEAL);
    bold(c, "R2  +  DEEPER TRACE", {1130,548}, 39, TEAL);
    Ctx m = faded(c, caveat);
    bold(m, "R3  ... GROUPING", {1130,641}, 39, AMBER);
    label(m, "still needs post-processing", {1135,698}, 28, DIM);
    label(faded(c,last), "PROTOTYPE / NOT A COMPLETE CAUSAL ACCOUNT", {1450,797}, 25, DIM, 0.5);
}

void closeScene(const Ctx& c, int t, const Chapter& ch, const g3d::ScenePtr& cityScene) {
    const float source = k(t, ch.cue[1], 24);
    panel(c, 155, 305, 1610, 500);
    g3d::render(*cityScene,t,c,{182,332,1556,390});
    dr::roundRect(c,190,733,1540,60,14,mkColor(0x20334a));
    dot(c,{284,762},AMBER,10); label(c,"PRICE TRAIL / H1 + H2",{306,762},27,INK);
    dot(c,{794,762},WEATHER,10); label(c,"RAIN TRAIL / H4",{818,762},27,INK);
    dot(c,{1258,762},ALARM,10); label(c,"T1 / ALARM",{1282,762},27,INK);
    Ctx m = faded(c, source);
    bold(m, "ExpCPS", {310,399}, 72, TEAL);
    label(m, "ILLUSTRATIVE CITYJSON MODEL / 2021 RESEARCH PROTOTYPE", {1708,834}, 22, DIM, 1);
}

void drawChapter(const Ctx& c, int i, int t, const Chapter& ch, int total, const g3d::ScenePtr& cityScene, const dg::DiagramPtr& diagram) {
    frame(c, i, t, ch, total);
    Ctx m = faded(c, k(t,0,10));
    switch (i) {
        case 0: alarmScene(m,t,ch,cityScene); break;
        case 1: priceScene(m,t,ch); break;
        case 2: rainScene(m,t,ch); break;
        case 3: stitchScene(m,t,ch); break;
        case 4: walkScene(m,t,ch,diagram); break;
        case 5: resultScene(m,t,ch,diagram); break;
        case 6: closeScene(m,t,ch,cityScene); break;
    }
    if (t > ch.dur-14) dr::rect(c,0,0,W,H,withAlpha(BG,k(t,ch.dur-14,14)));
}

std::string scorePath(const Plan& p) { return "out/expcps-overload-assets/score-" + std::to_string(p.total) + ".wav"; }

void generateScore(const Plan& p, const std::string& path) {
    procgen::Score s(p.total / (double)FPS + 0.2);
    const int root[N] = {45, 52, 48, 50, 47, 45, 52};
    for (int i=0; i<N; ++i) {
        const double start = p.ch[i].start / (double)FPS;
        const double dur = p.ch[i].dur / (double)FPS;
        s.pad(start, dur, root[i], 0.07f, 580, 0.5f);
        s.pad(start, dur, root[i]+7, 0.045f, 700, i%2 ? 0.72f : 0.28f);
        s.bass(start, std::min(1.7,dur), root[i]-12, 0.12f);
        for (int j=0; j<(int)p.ch[i].cue.size(); ++j) {
            const double when = start + p.ch[i].cue[j] / (double)FPS;
            s.tick(when, i == 4 ? 0.07f : 0.045f);
            if (i == 1 || i == 3 || i == 5) s.pluck(when, root[i]+24+j*2, 0.026f, j%2 ? 0.75f : 0.25f, 4.0f);
        }
        if (i == 0 || i == 2 || i == 6) s.impact(start+0.3, 0.20f);
    }
    const double end = p.total / (double)FPS;
    s.bell(end-2.3, 76, 0.05f, 0.3f);
    s.bell(end-2.0, 83, 0.04f, 0.7f);
    auto mix = s.mix();
    for (float& x : mix) x = std::tanh(x * 1.2f) * 0.58f;
    if (!media::writeWav(path, mix, procgen::Score::RATE)) throw std::runtime_error("Could not write ExpCPS score");
}

void writeSrt(const Plan& p) {
    sub::Track tr;
    for (const auto& ch : p.ch) {
        for (size_t i=0; i<ch.sentences.size(); ++i) {
            const double start = (ch.start + ch.cue[i]) / (double)FPS;
            const double end = std::min((ch.start + ch.voiceFrames) / (double)FPS,
                                        start + ch.cueDurations[i]);
            tr.cues.push_back({start,end,ch.sentences[i],false});
        }
    }
    if (!sub::writeSrt("out/expcps-overload.srt", tr)) throw std::runtime_error("Could not write ExpCPS SRT");
}

} // namespace

void registerExpCpsOverload(Registry& out) {
    const Plan p = loadPlan();
    const city::Model flex = city::load("data/expcps/flex.city.json");
    Comp c;
    c.name = "expcps-overload"; c.w=W; c.h=H; c.fps=FPS; c.duration=p.total; c.background=BG;
    c.assets = [p](const Comp&, const Json&) {
        std::filesystem::create_directories("out/expcps-overload-assets");
        const std::string score = scorePath(p);
        if (!std::filesystem::exists(score)) generateScore(p,score);
        writeSrt(p);
    };
    for (int i=0;i<N;++i) {
        const Chapter ch = p.ch[i];
        TNode* scene = addRoot(c.roots,ch.start,ch.dur);
        g3d::ScenePtr cityScene = (i == 0 || i == 6) ? flexCity(flex,i == 0) : nullptr;
        dg::DiagramPtr diagram = i == 4 ? walkDiagram(ch) : i == 5 ? resultDiagram(ch) : nullptr;
        scene->fn = [i,ch,total=p.total,cityScene,diagram](const Ctx& cx,int t) { drawChapter(cx,i,t,ch,total,cityScene,diagram); };
        media::AudioOptions vo; vo.volume=0.95f;
        media::audio(scene,"vo/expcps-overload/" + std::string(IDS[i]) + ".wav",vo);
    }
    TNode* bed = addRoot(c.roots,0,p.total);
    media::AudioOptions opt; opt.volume=0.52f;
    opt.volumeAt = [p](int t) {
        float gain=1.f;
        for (const Chapter& ch:p.ch) {
            const float a=k(t,ch.start-12,12), b=1.f-k(t,ch.start+ch.voiceFrames,12);
            gain=std::min(gain,1.f-0.63f*a*b);
        }
        return gain;
    };
    media::audio(bed,scorePath(p),opt);
    out.push_back(std::move(c));
}

} // namespace rmtn

