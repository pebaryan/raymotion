// flags.cpp -- short feature reel for the built-in country flag prefabs.
//   raymotion still flag-showcase --at 170 --png out/flag-check.png
//   raymotion render flag-showcase
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <cmath>

namespace rmtn {
namespace {
constexpr int FPS = 30;
constexpr int A = 90, B = 105, C = 105, D = 90, TOTAL = A+B+C+D;
const Color BG = mkColor(0x0a1020), PANEL = mkColor(0x141f34), INK = mkColor(0xf4f6ff),
            DIM = mkColor(0x9baac4), AQUA = mkColor(0x55ded5), WARM = mkColor(0xffca72);

float easeIn(int t, int at, int len = 16) {
    return (float)interpolate(t-at, {0,(double)len}, {0,1}, clamped(ease::out(ease::cubic)));
}

void backdrop(TNode* scene, const char* chapter, const char* title, const char* note) {
    scene->fn = [=](const Ctx& c, int t) {
        dr::rect(c,0,0,1920,1080,BG);
        dr::circle(c,{1550,160},560,withAlpha(AQUA,0.035f));
        dr::circle(c,{260,960},490,withAlpha(WARM,0.025f));
        dr::rect(c,88,82,48,7,AQUA);
        const float a = easeIn(t,0);
        Ctx k=c; k.a*=a;
        dr::text(fonts::ui(true),k,chapter,{152,78},28,2,AQUA);
        dr::text(fonts::ui(true),k,title,{88,135},75,0,INK);
        dr::text(fonts::ui(false),k,note,{91,234},30,0,DIM);
        dr::rect(c,88,1026,1744,2,PANEL);
        dr::rect(c,88,1026,1744.f*std::min(1.f,(float)t/scene->dur),2,AQUA);
    };
}

TNode* flag(TNode* scene, int at, int duration, const std::string& name, Vector2 pos,
            const Json& props = Json()) {
    TNode* n = prefab::use(scene,at,duration,name,props);
    n->props.pos = pos;
    return n;
}

void marker(TNode* scene, int at, Vector2 pos, const char* code, std::string name) {
    scene->add(at,scene->dur-at)->fn = [=](const Ctx& c,int t) {
        Ctx k=c; k.a*=easeIn(t,0,12);
        dr::text(fonts::ui(true),k,name.c_str(),{pos.x,pos.y+113},28,0,INK,0.5,0.5);
        dr::text(fonts::ui(false),k,code,{pos.x,pos.y+154},20,2,DIM,0.5,0.5);
    };
}

void addSound(Comp& c, int frame, const char* name, float volume) {
    TNode* n=addRoot(c.roots,frame,45);
    media::AudioOptions o; o.volume=volume;
    media::audio(n,std::string("assets/audio/flags/")+name+".wav",o);
}
} // namespace

void registerFlags(Registry& out) {
    Comp c;
    c.name="flag-showcase"; c.w=1920; c.h=1080; c.fps=FPS; c.duration=TOTAL;
    c.background=BG;
    c.music=true;

    // BGM: a soft four-chord synth bed with a bright repeating eighth-note motif.
    struct Chord { double bass, notes[4]; };
    const Chord chords[] = {
        {110.00,{220.00,261.63,329.63,392.00}},
        {87.31,{174.61,220.00,261.63,349.23}},
        {130.81,{261.63,329.63,392.00,523.25}},
        {98.00,{196.00,246.94,293.66,392.00}},
    };
    const double seconds = TOTAL/(double)FPS;
    for (int bar=0; bar*2<seconds; ++bar) {
        const double t=bar*2.;
        const Chord& chord=chords[bar%4];
        c.song.push_back({t,2.08,chord.bass,0.105,false});
        for (int i=0;i<8;++i)
            c.song.push_back({t+i*0.25,0.32,chord.notes[(i*3+i/4)%4],0.065,true});
    }

    // 1. Three large badges establish the motif.
    TNode* intro=addRoot(c.roots,0,A);
    backdrop(intro,"01  /  BUILT-IN PREFABS","FLAGS IN MOTION","29 country flags, ready for your composition.");
    struct Hero { const char* code; const char* name; float x; };
    const Hero heroes[]={{"at","AUSTRIA",510},{"jp","JAPAN",960},{"br","BRAZIL",1410}};
    for (int i=0;i<3;++i) {
        const Hero& h=heroes[i];
        flag(intro,8+i*9,A-8-i*9,std::string("flag-")+h.code,{h.x,574},
             Json::parse(R"({"size":270})"));
        marker(intro,17+i*9,{h.x,622},h.code,h.name);
    }
    intro->add(6,A-6)->fn=[](const Ctx& c,int t) {
        Ctx k=c; k.a*=easeIn(t,0);
        dr::text(fonts::ui(false),k,"29 FLAGS  /  ONE API",{960,886},27,3,AQUA,0.5,0.5);
    };

    // 2. A staggered grid makes the coverage and rhythm visible.
    TNode* grid=addRoot(c.roots,A,B);
    backdrop(grid,"02  /  COLLECTION","A WORLD OF COLOUR","Choose a country code. Place a badge anywhere.");
    const char* codes[]={"ca","us","gb","fr","de","it","se","no","in","cn","ua","ch"};
    for (int i=0;i<12;++i) {
        const int col=i%6,row=i/6;
        const Vector2 pos{250.f+col*284.f,435.f+row*286.f};
        const int at=7+i*4;
        flag(grid,at,B-at,std::string("flag-")+codes[i],pos,Json::parse(R"({"size":152})"));
        marker(grid,at+8,pos,codes[i],flags::title(codes[i]));
    }

    // 3. The same country as a badge, a plain flag and a labelled prefab.
    TNode* looks=addRoot(c.roots,A+B,C);
    backdrop(looks,"03  /  CUSTOMISE","YOUR FLAG, YOUR FORMAT","Badge or plain flag. Add a label when you need one.");
    flag(looks,12,C-12,"flag-jp",{505,565},Json::parse(R"({"size":275})"));
    flag(looks,20,C-20,"flag-jp",{960,565},Json::parse(R"({"size":275,"badge":false})"));
    flag(looks,28,C-28,"flag-jp",{1440,565},Json::parse(R"({"size":200,"label":"Japan"})"));
    looks->add(30,C-30)->fn=[](const Ctx& c,int t) {
        Ctx k=c; k.a*=easeIn(t,0);
        for (int i=0;i<3;++i) {
            const float x=505.f+i*455.f;
            dr::roundRect(k,x-134,807,268,52,15,PANEL);
        }
        dr::text(fonts::ui(false),k,"BADGE",{505,833},22,2,AQUA,0.5,0.5);
        dr::text(fonts::ui(false),k,"PLAIN",{960,833},22,2,AQUA,0.5,0.5);
        dr::text(fonts::ui(false),k,"LABEL",{1415,833},22,2,AQUA,0.5,0.5);
    };

    // 4. Closing flag-row and a compact call to action.
    TNode* close=addRoot(c.roots,A+B+C,D);
    close->fn=[](const Ctx& k,int t) {
        dr::rect(k,0,0,1920,1080,BG);
        dr::circle(k,{960,500},600,withAlpha(AQUA,0.045f));
        Ctx c=k; c.a*=easeIn(t,0);
        dr::text(fonts::ui(true),c,"MAKE IT MOVE.",{960,230},92,0,INK,0.5,0.5);
        dr::text(fonts::ui(false),c,"Country flags, ready for your timeline.",{960,335},35,0,DIM,0.5,0.5);
        dr::text(fonts::ui(true),c,"raymotion  /  flag-row",{960,880},31,1,AQUA,0.5,0.5);
    };
    flag(close,10,D-10,"flag-row",{960,590},
         Json::parse(R"({"countries":["at","fr","gb","us","jp","br"],"size":170,"gap":37,"stagger":6})"));

    // Short accents accompany the visual entrances and cuts; BGM comes from c.song.
    for (int f : {9,18,27,A+8,A+20,A+32,A+44,A+56,A+68,A+B+12,A+B+20,A+B+28,A+B+C+12})
        addSound(c,f,"pop",0.34f);
    for (int f : {A-3,A+B-3,A+B+C-3}) addSound(c,f,"whoosh",0.28f);
    addSound(c,A+B+C+54,"impact",0.32f);
    out.push_back(std::move(c));
}

} // namespace rmtn
