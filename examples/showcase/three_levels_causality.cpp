// One causal thread through property rules, connected devices, and timed events.
// Brief and source ledger: docs/three-levels-causality-brief.md.
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#undef BLUE
#undef RED

namespace rmtn { namespace {

constexpr int W = 1920, H = 1080, FPS = 30, N = 6;
const Color BG = mkColor(0x0b1421), PANEL = mkColor(0x17283b);
const Color LINE = mkColor(0x385065), INK = mkColor(0xf1f5f4), DIM = mkColor(0xaabdc9);
const Color BLUE = mkColor(0x6bbde8), AMBER = mkColor(0xefc16c);
const Color RED = mkColor(0xf27575), TEAL = mkColor(0x66d7c0);

struct Cue { int at; const char* text; };
struct Chapter {
    int start = 0, dur = 0;
    const char* head = "";
    const char* level = "";
    std::vector<Cue> cues;
};
struct Plan { std::array<Chapter,N> ch; int total = 0; };

Plan makePlan() {
    Plan p;
    p.ch[0] = { 0, 120, "ONE SYSTEM. THREE CAUSAL LEVELS.", "PROLOGUE",
                { {0,"One system."}, {36,"Three causal levels."},
                  {72,"Property. Device. Event."} } };
    p.ch[1] = { 0, 330, "01 / PROPERTY RULES", "GENERAL RELATION",
                { {0,"A rule links properties."}, {66,"Weather changes solar intensity."},
                  {156,"Solar intensity affects generated power."},
                  {240,"Consumed power affects transformer loading."} } };
    p.ch[2] = { 0, 390, "02 / CONNECTED DEVICES", "GROUNDED RELATION",
                { {0,"The rule meets a real topology."},
                  {90,"One device carries several properties."},
                  {186,"Connected devices make the link local."},
                  {276,"Power reaches the charger and car."} } };
    p.ch[3] = { 0, 300, "03 / EVENTS IN TIME", "OBSERVATION + ACTION",
                { {0,"Now the system records events."},
                  {63,"Rain lowers solar output."},
                  {150,"Transformer load exceeds the scenario threshold."},
                  {234,"Charging current is reduced."} } };
    p.ch[4] = { 0, 420, "04 / THE KNOWLEDGE GRAPH", "RULE → TOPOLOGY → EVIDENCE",
                { {0,"The graph keeps the three levels together."},
                  {90,"A rule names source, target and context."},
                  {180,"Topology grounds it in connected devices."},
                  {270,"An observation belongs to a sensor and device."},
                  {360,"The overload event references that observation."} } };
    p.ch[5] = { 0, 120, "THREE LEVELS / ONE KNOWLEDGE GRAPH", "SYNTHESIS",
                { {0,"Three levels. One knowledge graph."},
                  {54,"Aryan et al. / MSCPES 2020 and 2021."} } };
    for (auto& c : p.ch) { c.start = p.total; p.total += c.dur; }
    if (p.total != 1680) throw std::runtime_error("three-level cue plan has wrong duration");
    return p;
}

float easeIn(int t, int start, int span = 18) {
    const float u = std::clamp((t-start)/(float)std::max(1,span),0.f,1.f);
    return u*u*(3.f-2.f*u);
}
Ctx faded(const Ctx& c, float a) { Ctx x=c; x.a*=std::clamp(a,0.f,1.f); return x; }
void text(const Ctx& c, const std::string& s, Vector2 p, int size, Color color,
          double ax=0, double ay=.5) {
    dr::text(fonts::ui(false),c,s.c_str(),p,(float)size,0,color,ax,ay);
}
void bold(const Ctx& c, const std::string& s, Vector2 p, int size, Color color,
          double ax=0, double ay=.5) {
    dr::text(fonts::ui(true),c,s.c_str(),p,(float)size,0,color,ax,ay);
}
void box(const Ctx& c, float x, float y, float w, float h, Color fill) {
    dr::roundRect(c,x,y,w,h,18,fill);
}
void segment(const Ctx& c, Vector2 a, Vector2 b, Color color, float width, float u=1) {
    if (u>0) dr::strokeSeg(c,a,{a.x+(b.x-a.x)*std::clamp(u,0.f,1.f),
                               a.y+(b.y-a.y)*std::clamp(u,0.f,1.f)},width,color);
}
void elbow(const Ctx& c, Vector2 a, Vector2 b, float turnY, Color color, float u) {
    const std::array<Vector2,4> pts{a,{a.x,turnY},{b.x,turnY},b};
    const float lengths[3] = {std::abs(turnY-a.y),std::abs(b.x-a.x),std::abs(b.y-turnY)};
    const float total=lengths[0]+lengths[1]+lengths[2];
    float left=std::clamp(u,0.f,1.f)*total;
    for(int i=0;i<3;++i){
        const float part=lengths[i]>0?std::clamp(left/lengths[i],0.f,1.f):1.f;
        segment(c,pts[i],pts[i+1],color,6,part);
        left-=lengths[i];
    }
    if(u>=1) dr::circle(c,b,8,color);
}

int activeCue(const Chapter& ch,int t) {
    int j=0;
    for(int i=0;i<(int)ch.cues.size();++i) if(t>=ch.cues[i].at) j=i;
    return j;
}

void chrome(const Ctx& c,const Chapter& ch,int chapter,int t,int total) {
    dr::rect(c,0,0,W,H,BG);
    segment(c,{100,61},{1820,61},LINE,3);
    segment(c,{100,61},{100+1720.f*(ch.start+t)/total,61},TEAL,4);
    text(c,ch.level,{106,119},25,TEAL);
    bold(c,ch.head,{106,206},chapter==0?72:64,INK);
    text(c,chapter==0?"EXPCPS / THREE CAUSAL LEVELS":chapter==5?"2020 + 2021 PAPERS":"SIMULATED FLEX SCENARIO",
         {1812,119},23,DIM,1);
    const int ci=activeCue(ch,t);
    Ctx cap=faded(c,.38f+.62f*easeIn(t,ch.cues[ci].at,10));
    box(cap,108,883,1704,122,mkColor(0x20374d));
    bold(cap,ch.cues[ci].text,{150,943},40,INK);
    text(c,"ILLUSTRATIVE SCENARIO / PROPERTY RULES AND EVENT ORDER FROM ARYAN ET AL. 2020–2021",
         {111,1044},20,DIM);
}

dg::Theme diagramTheme() {
    dg::Theme th=dg::Theme::dark();
    th.fontSize=27; th.padX=16; th.padY=13;
    th.fill=PANEL; th.stroke=LINE; th.text=INK;
    th.edge=TEAL; th.edgeWidth=4; th.arrowSize=16; th.highlight=TEAL;
    th.colorKinds=false;
    return th;
}
dg::Node& fixedNode(dg::Graph& g,const char* id,const char* title,float x,float y,
                    float w,float h,Color fill) {
    auto& n=g.addNode(id,title,dg::Shape::Rounded);
    n.fixed=true; n.pos={x,y}; n.size={w,h};
    n.style.fill=fill; n.style.stroke=mixC(fill,INK,.34f); n.style.text=INK;
    return n;
}
void fixedEdge(dg::Graph& g,const char* from,const char* to,
               std::vector<Vector2> points,Color color) {
    auto& e=g.addEdge(from,to); e.fixed=true; e.curved=false;
    e.points=std::move(points); e.style.stroke=color; e.width=1.4f;
}
void predicateEdge(dg::Graph& g,const char* from,const char* to,const char* predicate,
                   std::vector<Vector2> points,Vector2 label,Color color) {
    auto& e=g.addEdge(from,to,predicate);
    e.fixed=true; e.curved=false; e.points=std::move(points);
    e.labelPos=label; e.style.stroke=color; e.width=1.35f;
}
dg::DiagramPtr propertyDiagram(const Chapter& ch) {
    dg::Graph g; g.layout=dg::LayoutKind::None; g.direction=dg::Direction::LR;
    fixedNode(g,"weather","WEATHER",175,105,265,100,mkColor(0x254962));
    fixedNode(g,"sun","SOLAR INTENSITY",540,105,300,100,mkColor(0x24546a));
    fixedNode(g,"pv","GENERATED POWER",930,105,315,100,mkColor(0x24546a));
    fixedNode(g,"use","CONSUMED POWER",930,350,315,100,mkColor(0x675337));
    fixedNode(g,"load","LOADING LEVEL",1330,230,305,110,mkColor(0x604047));
    fixedEdge(g,"weather","sun",{{307,105},{390,105}},BLUE);
    fixedEdge(g,"sun","pv",{{690,105},{772,105}},BLUE);
    fixedEdge(g,"pv","load",{{1087,105},{1330,105},{1330,175}},TEAL);
    fixedEdge(g,"use","load",{{1087,350},{1330,350},{1330,285}},AMBER);
    auto d=dg::Diagram::make(std::move(g),diagramTheme());
    d->maxZoom=1.f; d->margin=8;
    d->build(10,36,15);
    d->trace(ch.cues[1].at,72,{"weather","sun","pv","load"},BLUE);
    d->trace(ch.cues[3].at,54,{"use","load"},AMBER);
    return d;
}

dg::DiagramPtr eventDiagram(const Chapter& ch) {
    dg::Graph g; g.layout=dg::LayoutKind::None; g.direction=dg::Direction::LR;
    fixedNode(g,"rain","RAIN / WEATHER",170,120,265,110,mkColor(0x254962));
    fixedNode(g,"solar","PV OUTPUT FALLS",520,120,280,110,mkColor(0x24546a));
    fixedNode(g,"load","LOAD EVENT / T1",900,120,310,110,mkColor(0x693e49));
    fixedNode(g,"response","CHARGE CURRENT DOWN",1330,120,345,110,mkColor(0x675337));
    fixedEdge(g,"rain","solar",{{302,120},{380,120}},BLUE);
    fixedEdge(g,"solar","load",{{660,120},{745,120}},TEAL);
    fixedEdge(g,"load","response",{{1055,120},{1157,120}},AMBER);
    auto d=dg::Diagram::make(std::move(g),diagramTheme());
    d->maxZoom=1.f; d->margin=10;
    d->build(8,30,12);
    d->trace(ch.cues[1].at,52,{"rain","solar"},BLUE);
    d->trace(ch.cues[2].at-24,57,{"solar","load"},TEAL);
    d->trace(ch.cues[3].at,46,{"load","response"},AMBER);
    return d;
}

dg::Theme kgTheme() {
    auto th=diagramTheme();
    th.fontSize=28; th.edgeText=INK; th.edgeLabelFill=mkColor(0x294054);
    th.maxLabelWidth=350; th.edgeWidth=3.5f;
    return th;
}
dg::DiagramPtr kgRuleDiagram() {
    dg::Graph g; g.layout=dg::LayoutKind::None; g.direction=dg::Direction::LR;
    fixedNode(g,"rule","Causality_2",250,235,300,82,mkColor(0x234d52));
    fixedNode(g,"source","ACTIVE_POWER_3P",1180,70,400,82,mkColor(0x254962));
    fixedNode(g,"target","LOADING",1180,235,350,82,mkColor(0x604047));
    fixedNode(g,"context","TrafoGridCondition",1180,400,440,82,mkColor(0x675337));
    predicateEdge(g,"rule","source","expcps:source",{{400,205},{755,70},{980,70}},
                  {780,108},BLUE);
    predicateEdge(g,"rule","target","expcps:target",{{400,235},{790,235},{1005,235}},
                  {780,201},TEAL);
    predicateEdge(g,"rule","context","expcps:context",{{400,265},{755,400},{960,400}},
                  {790,360},AMBER);
    auto d=dg::Diagram::make(std::move(g),kgTheme());
    d->maxZoom=1.f; d->margin=20; d->build(4,20,18);
    d->trace(90,82,{"rule","source"},BLUE);
    d->trace(122,75,{"rule","target"},TEAL);
    d->trace(150,70,{"rule","context"},AMBER);
    return d;
}
dg::DiagramPtr kgTopologyDiagram() {
    dg::Graph g; g.layout=dg::LayoutKind::None; g.direction=dg::Direction::LR;
    fixedNode(g,"srcdev","?src_dev",190,235,260,82,mkColor(0x254962));
    fixedNode(g,"srcfeat","?src_feat / POWERGRID_CONNECTOR",675,235,535,82,mkColor(0x24546a));
    fixedNode(g,"tgtfeat","?tgt_feat / LV_TRANSFORMER",1360,235,470,82,mkColor(0x24574e));
    fixedNode(g,"tgtdev","?tgt_dev",1360,420,260,82,mkColor(0x675337));
    predicateEdge(g,"srcdev","srcfeat","expcps:placedAt",{{320,235},{407,235}},
                  {365,145},BLUE);
    predicateEdge(g,"srcfeat","tgtfeat","expcps:hasConnection",{{942,235},{1125,235}},
                  {1036,145},TEAL);
    predicateEdge(g,"tgtdev","tgtfeat","expcps:placedAt",{{1360,379},{1360,276}},
                  {1510,326},AMBER);
    auto d=dg::Diagram::make(std::move(g),kgTheme());
    d->maxZoom=1.f; d->margin=20; d->build(0,12,15);
    d->trace(35,60,{"srcfeat","tgtfeat"},TEAL);
    return d;
}
dg::DiagramPtr kgEvidenceDiagram() {
    dg::Graph g; g.layout=dg::LayoutKind::None; g.direction=dg::Direction::LR;
    fixedNode(g,"trafo","LV_TRANSFORMER_1",765,75,400,76,mkColor(0x24574e));
    fixedNode(g,"sensor","LOADING_1",765,255,340,76,mkColor(0x675337));
    fixedNode(g,"property","LOADING",1450,255,300,76,mkColor(0x604047));
    fixedNode(g,"event","OverloadEvent_1",260,435,410,76,mkColor(0x693e49));
    fixedNode(g,"observation","LOADING_1_obs_1",950,435,410,76,mkColor(0x24546a));
    predicateEdge(g,"sensor","trafo","expcps:placedAt",{{765,217},{765,113}},
                  {575,165},AMBER);
    predicateEdge(g,"sensor","property","sosa:observes",{{935,255},{1300,255}},
                  {1115,219},BLUE);
    predicateEdge(g,"event","observation","expcps:hasReference",{{465,435},{745,435}},
                  {605,400},RED);
    predicateEdge(g,"observation","sensor","sosa:madeBySensor",{{950,397},{950,293}},
                  {1170,345},BLUE);
    auto d=dg::Diagram::make(std::move(g),kgTheme());
    d->maxZoom=1.f; d->margin=18; d->build(0,12,15);
    d->trace(20,58,{"observation","sensor","property"},BLUE);
    d->trace(90,50,{"event","observation"},RED);
    return d;
}

g3d::ScenePtr cityScene() {
    auto scene=std::make_shared<g3d::Scene>();
    scene->background=mkColor(0x142332);
    scene->skyColor=mkColor(0xc9dce7);
    scene->groundColor=mkColor(0x455963);
    scene->ambient=.65f;
    scene->environmentGenerator=g3d::studioEnvironment(
        mkColor(0xe1edf0),mkColor(0x9cb5c3),mkColor(0x3d4b54),3);
    scene->environmentIntensity=.74f;
    auto sun=g3d::Light::directional({-30,45,40},mkColor(0xfff1dd),1.4f,true);
    sun.shadowExtent=65; scene->lights.push_back(sun);
    scene->camera.orthographic=true; scene->camera.orthoHeight=16.5f;
    scene->camera.position={38,27,47}; scene->camera.target={1.1f,2.2f,0};
    g3d::Material ground; ground.color=mkColor(0x50616b); ground.roughness=.92f;
    auto* floor=scene->add(g3d::Geometry::box(62,.07f,20),ground);
    floor->position.y=-.06f;
    const auto look=smartgrid3d::cityLook();
    const auto put=[&](const char* id,Vector3 at) {
        auto* n=city::add(*scene,city::load(std::string("data/city/smartgrid/")+id+".city.json"),look);
        n->position=at;
    };
    put("solar-carport",{-18,0,0});
    put("distribution-substation",{2.5f,0,0});
    put("ev-charging-pole",{19,0,-.3f});
    put("e-car",{25,0,0});
    auto* person=human3d::add(*scene,human3d::Role::EvDriver);
    person->position={28,0,1.8f};
    return scene;
}

g3d::ScenePtr chargingDetail() {
    auto scene=std::make_shared<g3d::Scene>();
    scene->background=mkColor(0x142332);
    scene->skyColor=mkColor(0xc9dce7);
    scene->groundColor=mkColor(0x455963);
    scene->ambient=.68f;
    scene->environmentGenerator=g3d::studioEnvironment(
        mkColor(0xe1edf0),mkColor(0x9cb5c3),mkColor(0x3d4b54),3);
    scene->environmentIntensity=.8f;
    scene->lights.push_back(g3d::Light::directional({-8,12,10},mkColor(0xfff1dd),1.4f,true));
    scene->camera.orthographic=true; scene->camera.orthoHeight=5.8f;
    scene->camera.position={7,5.8f,8}; scene->camera.target={.8f,1,0};
    g3d::Material floor; floor.color=mkColor(0x5a6b71); floor.roughness=.92f;
    auto* base=scene->add(g3d::Geometry::box(10,.07f,6),floor);
    base->position.y=-.06f;
    const auto look=smartgrid3d::cityLook();
    auto* pole=city::add(*scene,city::load("data/city/smartgrid/ev-charging-pole.city.json"),look);
    pole->position={-2.8f,0,0};
    auto* car=city::add(*scene,city::load("data/city/smartgrid/e-car.city.json"),look);
    car->position={.8f,0,0};
    human3d::add(*scene,human3d::Role::EvDriver)->position={3.8f,0,1.2f};
    return scene;
}

void prologue(const Ctx& c,int t) {
    const Color col[3]={BLUE,TEAL,AMBER};
    const char* names[3]={"PROPERTY","DEVICE","EVENT"};
    for(int i=0;i<3;++i){
        Ctx m=faded(c,easeIn(t,i*35,18));
        const float x=190+i*515.f, y=355+i*38.f;
        box(m,x,y,445,245,mkColor(i==0?0x1e4359:i==1?0x1e4d4b:0x534330));
        dr::rect(m,x+25,y+26,8,191,col[i]);
        bold(m,names[i],{x+60,y+85},40,INK);
        text(m,i==0?"A general rule":i==1?"A connected object":"A change in time",
             {x+60,y+155},28,DIM);
        if(i<2) segment(m,{x+445,y+125},{x+515,y+125},col[i],7,easeIn(t,i*35+20,18));
    }
    text(c,"The same thread, seen at three scales of meaning.",{960,779},32,DIM,.5);
}

void properties(const Ctx& c,int t,const Chapter& ch,const dg::DiagramPtr& d) {
    box(c,106,305,1708,515,PANEL);
    text(c,"GENERAL KNOWLEDGE / NO DEVICE IDS",{150,354},27,DIM);
    d->draw(c,t,{145,393,1625,366});
    text(c,"PAPER 2020 / FIG. 4  •  PAPER 2021 / §3.3",{1755,788},22,DIM,1);
}

void deviceChip(const Ctx& c,int t,int start,float x,const char* name,
                const char* p1,const char* p2,Color color) {
    Ctx m=faded(c,easeIn(t,start,16));
    box(m,x,329,350,100,mkColor(0x24394a));
    dr::rect(m,x,329,8,100,color);
    bold(m,name,{x+24,354},25,INK);
    text(m,p1,{x+24,385},21,color);
    text(m,p2,{x+24,410},21,DIM);
}

void devices(const Ctx& c,int t,const Chapter& ch,const g3d::ScenePtr& scene,
             const g3d::ScenePtr& detail) {
    box(c,106,305,1708,520,PANEL);
    g3d::render(*scene,t,c,{124,443,1172,285});
    g3d::render(*detail,t,c,{1313,443,485,285});
    text(c,"GRID CONTEXT",{145,470},20,INK);
    text(c,"CHARGING DETAIL",{1333,470},20,INK);
    deviceChip(c,t,ch.cues[1].at,138,"PV SITE","solar intensity","generated power",BLUE);
    deviceChip(c,t,ch.cues[1].at+12,553,"TRANSFORMER","active power","loading level",RED);
    deviceChip(c,t,ch.cues[1].at+24,968,"EV CHARGER","charging mode","consumed power",AMBER);
    deviceChip(c,t,ch.cues[1].at+36,1383,"E-CAR","charge session","energy received",TEAL);
    const float u=easeIn(t,ch.cues[2].at,44);
    elbow(c,{328,735},{735,735},770,TEAL,u);
    elbow(c,{735,735},{1170,735},770,TEAL,easeIn(t,ch.cues[2].at+17,42));
    elbow(c,{1170,735},{1585,735},770,AMBER,easeIn(t,ch.cues[3].at,35));
    text(c,"PV OUTPUT",{330,807},22,BLUE,.5);
    text(c,"GRID FEED",{955,807},22,TEAL,.5);
    text(c,"CHARGING",{1580,807},22,AMBER,.5);
    text(c,"CITYJSON + NATIVE PBR PREFABS / ILLUSTRATIVE PLACEMENT",{1768,792},19,DIM,1);
}

void events(const Ctx& c,int t,const Chapter& ch,const dg::DiagramPtr& d) {
    box(c,106,305,1708,520,PANEL);
    text(c,"EARLIER",{160,353},25,DIM);
    text(c,"LATER",{1747,353},25,DIM,1);
    d->draw(c,t,{137,399,1645,266});
    const float alert=easeIn(t,ch.cues[2].at,22);
    box(c,164,704,604,97,mkColor(0x233b4d));
    dr::rect(c,164,704,7,97,AMBER);
    bold(c,"EV CHARGING ACTIVE",{190,741},28,INK);
    text(c,"session context / no overload-cause arrow",{190,780},22,DIM);
    text(c,"TRANSFORMER LOADING",{846,714},23,DIM);
    dr::roundRect(c,846,752,790,27,12,mkColor(0x34495a));
    dr::roundRect(c,846,752,474,27,12,RED);
    const float threshold=846+790*.6f;
    segment(c,{threshold,739},{threshold,800},INK,4);
    dr::circle(faded(c,alert),{threshold+19,765},11,RED);
    text(c,"SCENARIO THRESHOLD 60%",{threshold,817},23,INK,.5);
    Ctx m=faded(c,alert);
    bold(m,"EXCEEDED",{1662,765},25,RED);
    text(c,"ONE CANDIDATE PATH",{1050,353},21,DIM,.5);
}

void knowledgeGraph(const Ctx& c,int t,const dg::DiagramPtr& rule,
                    const dg::DiagramPtr& topology,const dg::DiagramPtr& evidence) {
    box(c,106,305,1708,520,PANEL);
    Ctx a=faded(c,1.f-easeIn(t,176,20));
    text(a,"ABSTRACT RULE / PAPER FIG. 6",{149,354},25,BLUE);
    rule->draw(a,t,{144,388,1634,385});
    Ctx b=faded(c,easeIn(t,176,20)*(1.f-easeIn(t,266,18)));
    text(b,"TOPOLOGY CONDITION / SPARQL VARIABLES / PAPER FIG. 7",{149,354},25,TEAL);
    topology->draw(b,std::max(0,t-180),{144,389,1634,393});
    Ctx e=faded(c,easeIn(t,266,18));
    text(e,"OBSERVATION + EVENT / SELECTED RDF LINKS / PAPER FIGS. 5, 8",
         {149,354},25,TEAL);
    evidence->draw(e,std::max(0,t-270),{144,389,1634,393});
    text(c,t<185?"RULE: SOURCE + TARGET + CONTEXT":t<275?
         "CONDITION: CONNECTED FEATURE PAIR":"DEVICE  /  SENSOR  /  OBSERVATION  /  EVENT",
         {1760,796},21,DIM,1);
}

void synthesis(const Ctx& c,int t) {
    const char* names[3]={"RULE","DEVICE","EVENT"};
    const char* details[3]={"solar intensity → generated power",
                            "PV site → transformer → charger",
                            "load event → current reduced"};
    const Color cols[3]={BLUE,TEAL,AMBER};
    for(int i=0;i<3;++i){
        Ctx m=faded(c,easeIn(t,i*13,16));
        const float y=315+i*151.f;
        box(m,220,y,1480,116,mkColor(0x1c3144));
        dr::rect(m,220,y,9,116,cols[i]);
        bold(m,names[i],{265,y+50},37,cols[i]);
        text(m,details[i],{658,y+51},34,INK);
        if(i<2) segment(m,{572,y+116},{572,y+151},cols[i],5);
    }
    Ctx frame=faded(c,easeIn(t,33,16));
    dr::roundRect(frame,198,296,1524,490,20,withAlpha(TEAL,.065f));
    segment(frame,{202,789},{1718,789},TEAL,4);
    text(frame,"LINKED IN THE KNOWLEDGE GRAPH",{960,773},23,TEAL,.5);
    Ctx src=faded(c,easeIn(t,54,20));
    text(src,"ARYAN ET AL. / MSCPES 2020 + 2021",{240,805},26,DIM);
    text(src,"DOI 10.1109/MSCPES49613.2020.9133700  /  10.1145/3470481.3472704",
         {240,846},21,DIM);
}

void drawChapter(const Ctx& c,int index,int t,const Chapter& ch,int total,
                 const g3d::ScenePtr& city,const g3d::ScenePtr& detail,
                 const dg::DiagramPtr& diagram,const dg::DiagramPtr& diagram2,
                 const dg::DiagramPtr& diagram3,
                 const std::string& cityError) {
    chrome(c,ch,index,t,total);
    Ctx m=faded(c,easeIn(t,0,12));
    switch(index){
    case 0: prologue(m,t); break;
    case 1: properties(m,t,ch,diagram); break;
    case 2:
        if(!city) throw std::runtime_error("three-level city assets missing: "+cityError);
        if(!detail) throw std::runtime_error("three-level charging assets missing: "+cityError);
        devices(m,t,ch,city,detail); break;
    case 3: events(m,t,ch,diagram); break;
    case 4: knowledgeGraph(m,t,diagram,diagram2,diagram3); break;
    case 5: synthesis(m,t); break;
    }
    if(t>ch.dur-14) dr::rect(c,0,0,W,H,withAlpha(BG,easeIn(t,ch.dur-14,14)));
}

std::string scorePath(const Plan& p) {
    return "out/three-levels-causality-assets/score-"+std::to_string(p.total)+".wav";
}
void generateScore(const Plan& p,const std::string& path) {
    procgen::Score s(p.total/(double)FPS+.2);
    const int notes[N]={62,74,55,43,67,62};
    for(int i=0;i<N;++i){
        const auto& ch=p.ch[i];
        const double start=ch.start/(double)FPS, dur=ch.dur/(double)FPS;
        if(i==1) s.pad(start,dur,notes[i]-12,.055f,720,.46f);
        if(i==2) s.pad(start,dur,notes[i]-12,.065f,430,.54f);
        if(i==3) s.pad(start,dur,notes[i]-12,.055f,270,.48f);
        if(i==4) s.pad(start,dur,notes[i]-12,.05f,540,.40f);
        for(size_t j=0;j<ch.cues.size();++j){
            const double when=start+ch.cues[j].at/(double)FPS;
            s.tick(when,i==3?.085f:.052f);
            s.pluck(when,notes[i]+(int)j*3,i==3?.045f:.055f,
                    j%2?.67f:.33f,i==3?2.3f:4.f);
            if(i==2 && j>0) s.bass(when,.43,notes[i]-24,.075f);
        }
        if(i==3){
            for(int j=0;j<18;++j) s.tick(start+2.1+j*.16,.013f);
            s.impact(start+5.0,.18f);
        }
    }
    const double end=p.total/(double)FPS;
    s.bell(end-3.4,74,.07f,.30f);
    s.bell(end-2.9,81,.055f,.70f);
    s.bell(end-2.4,86,.045f,.48f);
    auto mix=s.mix();
    for(float& x:mix) x=std::tanh(x*1.05f)*.53f;
    if(!media::writeWav(path,mix,procgen::Score::RATE))
        throw std::runtime_error("Could not write three-level causality score");
}
void writeSrt(const Plan& p) {
    sub::Track tr;
    for(const auto& ch:p.ch) for(size_t j=0;j<ch.cues.size();++j){
        const double from=(ch.start+ch.cues[j].at)/(double)FPS;
        const double to=(ch.start+(j+1<ch.cues.size()?ch.cues[j+1].at:ch.dur))/(double)FPS;
        tr.cues.push_back({from,to,ch.cues[j].text,false});
    }
    if(!sub::writeSrt("out/three-levels-causality.srt",tr))
        throw std::runtime_error("Could not write three-level causality SRT");
}

} // namespace

void registerThreeLevelsCausality(Registry& out) {
    const Plan p=makePlan();
    g3d::ScenePtr scene;
    g3d::ScenePtr detail;
    std::string cityError;
    try { scene=cityScene(); detail=chargingDetail(); }
    catch(const std::exception& e) { cityError=e.what(); } // `list` still works without render assets
    Comp c;
    c.name="three-levels-causality"; c.w=W; c.h=H; c.fps=FPS;
    c.duration=p.total; c.background=BG;
    c.assets=[p](const Comp&,const Json&){
        std::filesystem::create_directories("out/three-levels-causality-assets");
        const auto score=scorePath(p);
        if(!std::filesystem::exists(score)) generateScore(p,score);
        writeSrt(p);
    };
    for(int i=0;i<N;++i){
        const Chapter ch=p.ch[i];
        TNode* node=addRoot(c.roots,ch.start,ch.dur);
        auto diagram=i==1?propertyDiagram(ch):i==3?eventDiagram(ch):i==4?kgRuleDiagram():nullptr;
        auto diagram2=i==4?kgTopologyDiagram():nullptr;
        auto diagram3=i==4?kgEvidenceDiagram():nullptr;
        node->fn=[i,ch,total=p.total,scene,detail,diagram,diagram2,diagram3,cityError](const Ctx& cx,int t){
            drawChapter(cx,i,t,ch,total,scene,detail,diagram,diagram2,diagram3,cityError);
        };
    }
    TNode* bed=addRoot(c.roots,0,p.total);
    media::AudioOptions opt; opt.volume=2.10f;
    media::audio(bed,scorePath(p),opt);
    out.push_back(std::move(c));
}

} // namespace rmtn
