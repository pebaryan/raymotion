// Source-grounded, caption-led introduction to Here-and-There and equilibrium.
// See docs/here-there-explainer.md for references, script and exact scope.
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include "rmtn/timeline.hpp"
#include "rmtn/here_there.hpp"

namespace rmtn { namespace {
constexpr int FPS=30, CHAPTER=12*FPS, CHAPTERS=9;
const Color BG{12,20,33,255}, PANEL{21,34,51,255}, INK{234,240,246,255};
const Color DIM{153,174,194,255}, CYAN{71,218,216,255}, HTGOLD{255,195,101,255};
const Color HTGREEN{115,226,167,255}, REDC{255,120,139,255};

void text(const char* s,float x,float y,float size,Color color,float width=1140.f){
    Font font=GetFontDefault();
    float measured=MeasureTextEx(font,s,size,1.f).x;
    if(measured>width) size*=width/measured;
    DrawTextEx(font,s,{x,y},size,1.f,color);
}
void center(const char* s,float x,float y,float size,Color color){
    Vector2 m=MeasureTextEx(GetFontDefault(),s,size,1.f);
    text(s,x-m.x*.5f,y,size,color);
}
void panel(float x,float y,float w,float h,Color edge=Color{48,67,85,255}){
    DrawRectangleRec({x,y,w,h},PANEL);
    DrawRectangleRec({x,y,w,3},edge);
}
void arrow(Vector2 a,Vector2 b,Color color,float thickness=3){
    DrawLineEx(a,b,thickness,color);
    float angle=std::atan2(b.y-a.y,b.x-a.x);
    Vector2 v1{b.x-13*std::cos(angle-.55f),b.y-13*std::sin(angle-.55f)};
    Vector2 v2{b.x-13*std::cos(angle+.55f),b.y-13*std::sin(angle+.55f)};
    DrawTriangle(b,v1,v2,color);
}
const char* atoms(unsigned mask){
    switch(mask){case 0:return "{ }";case ht::FIRE:return "{ fire }";
        case ht::SMOKE:return "{ smoke }";default:return "{ fire, smoke }";}
}
void atomChip(const char* name,float x,float y,bool present,Color color,float width=185){
    DrawRectangleRec({x,y,width,54},present?withAlpha(color,.15f):BG);
    DrawCircleV({x+21,y+27},6,present?color:DIM);
    text(name,x+42,y+12,29,present?INK:DIM,width-48);
    if(!present) DrawLineEx({x+17,y+27},{x+25,y+27},2,BG);
}
void world(float x,const char* label,unsigned mask,Color color){
    panel(x,258,500,222,color);
    text(label,x+24,279,29,color,452);
    atomChip("fire",x+24,336,(mask&ht::FIRE)!=0,color);
    atomChip("smoke",x+244,336,(mask&ht::SMOKE)!=0,color);
    text(atoms(mask),x+24,414,26,INK,452);
}
void worlds(unsigned h,unsigned t){
    world(64,"HERE / H",h,CYAN);
    world(716,"THERE / T",t,HTGOLD);
    arrow({580,365},{700,365},DIM);
    center("subset",640,327,19,DIM);
}
void callout(const char* a,const char* b){
    DrawRectangleRec({64,550,1152,118},Color{17,30,46,255});
    DrawRectangleRec({64,550,4,118},CYAN);
    text(a,86,570,27,INK,1100);
    text(b,86,612,25,DIM,1100);
}
void status(const char* label,bool ok,float x,float y){
    Color c=ok?HTGREEN:REDC;
    DrawCircleV({x+9,y+12},6,c);
    text(label,x+28,y,24,c,520);
}

void explain(int frame){
    int chapter=std::min(frame/CHAPTER,CHAPTERS-1), local=frame%CHAPTER;
    float sec=(float)local/FPS;
    float enter=(float)easeFn(E_OUT_CUBIC,std::min(1.f,sec/1.1f));
    ClearBackground(BG);
    // Restrained procedural background: no external images, fonts or audio.
    for(int x=0;x<1280;x+=48) for(int y=0;y<720;y+=48)
        DrawPixel(x,y,Color{30,44,60,255});
    text("RAYMOTION / LOGIC EXPLAINED",64,32,20,DIM);
    char counter[48]; std::snprintf(counter,sizeof counter,"%02d / %02d",chapter+1,CHAPTERS);
    text(counter,1100,32,20,CYAN,130);
    const char* titles[]={"Why ordinary implication is not enough",
        "One instant. Two levels of truth.","An atom has three possible states",
        "Implication must pass BOTH checks","Not established is not the same as false",
        "Equilibrium: try to remove assumptions","A fact changes which model survives",
        "Now add a separate TIME axis","The route from HT to temporal stable models"};
    text(titles[chapter],64,86+(1-enter)*14,40,INK,1152);
    DrawRectangle(64,151,(int)(1152*enter),2,Color{40,63,81,255});

    switch(chapter){
    case 0:{
        text("Cabalar's example: a rule should not invent its own facts.",64,177,27,DIM);
        center("fire -> smoke",640,238,40,CYAN);
        const unsigned candidates[]={0,ht::SMOKE,ht::FIRE|ht::SMOKE};
        for(int i=0;i<3;++i){
            float x=64.f+i*392.f;
            panel(x,321,368,160,i==0?HTGREEN:HTGOLD);
            text("CLASSICAL MODEL",x+20,344,20,DIM,328);
            center(atoms(candidates[i]),x+184,390,29,INK);
            if(sec>2+i*.7f) status("satisfies rule",ht::theory(candidates[i],candidates[i]),x+36,438);
        }
        callout("All three sets satisfy the classical implication.",
                "But where did smoke or fire come from if no fact states them?");
        break;
    }
    case 1:{
        text("An HT interpretation is a pair (H, T), with H a subset of T.",64,177,27,DIM);
        worlds(0,ht::FIRE|ht::SMOKE);
        text("established / stronger truth",88,492,25,CYAN,500);
        text("assumed true",740,492,25,HTGOLD,450);
        callout("Anything true Here must also be true There. The converse need not hold.",
                "These worlds are NOT now and later. H is not an automatic proof procedure.");
        break;
    }
    case 2:{
        text("For a single atom p, H subset T leaves exactly three possibilities.",64,177,27,DIM);
        const char* names[]={"FALSE","ASSUMED ONLY","ESTABLISHED"};
        const char* detail[]={"p absent from T","p in T, not in H","p in both H and T"};
        for(int i=0;i<3;++i){
            float x=64.f+i*392;
            panel(x,252,368,237,i==0?DIM:(i==1?HTGOLD:CYAN));
            text(names[i],x+20,276,26,i==0?DIM:(i==1?HTGOLD:CYAN),330);
            text("H",x+55,336,29,CYAN); text("T",x+250,336,29,HTGOLD);
            DrawCircleV({x+64,405},24,i==2?CYAN:BG);
            DrawCircleLinesV({x+64,405},24,CYAN);
            DrawCircleV({x+260,405},24,i>=1?HTGOLD:BG);
            DrawCircleLinesV({x+260,405},24,HTGOLD);
            text(detail[i],x+20,451,22,DIM,330);
        }
        callout("The missing fourth case - p in H but not T - is forbidden.",
                "The middle state is neither established truth nor established negation.");
        break;
    }
    case 3:{
        text("For fire -> smoke: whenever fire holds, smoke must hold too.",64,177,27,DIM);
        int phase=std::min((int)(sec/3),3);
        unsigned h=phase>=2?ht::FIRE:0, t=phase==0?ht::FIRE:(ht::FIRE|ht::SMOKE);
        if(phase==3) h|=ht::SMOKE;
        worlds(h,t);
        bool here=!(h&ht::FIRE)||(h&ht::SMOKE), there=!(t&ht::FIRE)||(t&ht::SMOKE);
        status(here?"H check: pass":"H check: FAIL",here,88,496);
        status(there?"T check: pass":"T check: FAIL",there,740,496);
        const char* a[]={"H is empty, so its local check passes. But T violates the rule.",
            "Add smoke to T. Both checks now pass, even while H stays empty.",
            "Add fire to H without smoke. Now the H check fails.",
            "Add smoke to H too. Both worlds satisfy the implication."};
        callout(a[phase],"Implication is checked at (H,T) AND at the total world (T,T).");
        break;
    }
    case 4:{
        text("Negation is defined by implication:  not p  means  p -> false.",64,177,27,DIM);
        panel(64,251,550,252,HTGOLD); panel(646,251,570,252,CYAN);
        text("THE MIDDLE STATE",88,275,24,HTGOLD);
        text("H = { }       T = { p }",88,332,31,INK,505);
        text("p is not true Here.",88,391,27,DIM,505);
        text("But p is still assumed There.",88,438,25,HTGOLD,505);
        text("CHECK NEGATION AT THERE",670,275,24,CYAN,520);
        text("not p is true iff p is absent from T.",670,332,27,INK,520);
        status("p is not true Here",false,670,391);
        status("not p is not true Here either",false,670,438);
        callout("In this interpretation, p OR not p is not true Here.",
                "So excluded middle is not valid in HT. No contradiction is being asserted.");
        break;
    }
    case 5:{
        text("Start with a TOTAL model (T,T). Keep T fixed; shrink only H.",64,177,27,DIM);
        bool shrink=sec>=3;
        worlds(shrink?0:ht::SMOKE,ht::SMOKE);
        status("fire -> smoke still holds",ht::theory(shrink?0:ht::SMOKE,ht::SMOKE),88,496);
        text(shrink?"T stays fixed":"candidate: smoke alone",740,496,25,HTGOLD,450);
        callout(shrink?"The smaller H = { } still satisfies the rule: reject this candidate.":
                           "Can smoke be removed from H while the same T is held fixed?",
                "A total model is equilibrium only if NO proper smaller H remains a model.");
        break;
    }
    case 6:{
        text("Compare the equilibrium models of two tiny theories.",64,177,27,DIM);
        panel(64,252,550,257,CYAN); panel(646,252,570,257,HTGOLD);
        text("RULE ONLY",88,276,24,CYAN);
        text("fire -> smoke",88,328,34,INK);
        text("stable model: { }",88,402,31,HTGREEN,490);
        text("No facts require either atom.",88,456,25,DIM,500);
        text("ADD THE FACT fire",670,276,24,HTGOLD);
        text("fire AND (fire -> smoke)",670,328,30,INK,520);
        text("stable: { fire, smoke }",670,402,30,HTGREEN,520);
        text("Removing either atom violates the theory.",670,456,24,DIM,520);
        callout("The fact forces fire Here. The implication then forces smoke Here.",
                "HT supplies the model semantics; equilibrium supplies the selection criterion.");
        break;
    }
    case 7:{
        text("Temporal Here-and-There uses a pair of traces, not just a pair of sets.",64,177,27,DIM);
        text("H",69,304,32,CYAN); text("T",69,415,32,HTGOLD);
        for(int i=0;i<3;++i){
            float x=160.f+i*330;
            char label[32]; std::snprintf(label,sizeof label,"time %d",i);
            text(label,x+43,235,24,DIM,235);
            panel(x,292,250,70,CYAN); panel(x,402,250,70,HTGOLD);
            const char* value=i==0?"{ fire }":i==1?"{ smoke }":"{ }";
            center(value,x+125,312,29,INK); center(value,x+125,422,29,INK);
            arrow({x+125,392},{x+125,372},DIM,2);
            if(i<2){ arrow({x+259,327},{x+317,327},CYAN); arrow({x+259,437},{x+317,437},HTGOLD); }
        }
        text("...",1167,313,35,DIM,80); text("...",1167,423,35,DIM,80);
        text("time moves right; H_i is a subset of T_i at every instant",160,493,25,CYAN,1050);
        callout("X = next; G = always. Example: fire AND G(fire -> X smoke).",
                "TEL keeps the T trace fixed and rejects it if a strictly smaller H trace works.");
        break;
    }
    default:{
        text("Two worlds -> HT models -> equilibrium -> temporal stable models",64,177,27,CYAN,1152);
        panel(64,247,1152,269,CYAN);
        text("START WITH THE AUTHOR'S INTRODUCTION",88,271,23,CYAN);
        text("Pedro Cabalar (2021), section 2",88,313,29,INK);
        text("Temporal ASP: from logical foundations to practical use with telingo",88,357,26,INK,1090);
        text("www.dc.fi.udc.es/~cabalar/RW2021.pdf",88,400,25,HTGOLD);
        text("Also: Aguado, Cabalar, Dieguez, Perez & Vidal (2013), sections 2-3",88,449,24,DIM,1090);
        text("www.dc.fi.udc.es/~cabalar/tel-dp60.pdf",88,483,23,HTGOLD);
        callout("H and T are truth components; equilibrium is a minimality test with T fixed.",
                "The temporal diagram shows an infinite-trace prefix. Visual examples are our own.");
        break;
    }
    }
    // A paced reveal/transition wash, rather than abrupt cuts. All timing uses frame number.
    float fade=1.f-std::min(1.f,(float)local/12.f);
    if(local>CHAPTER-13) fade=std::max(fade,(float)(local-(CHAPTER-13))/12.f);
    if(fade>0) DrawRectangleRec({0,75,1280,600},withAlpha(BG,fade));
    DrawRectangle(64,694,1152,4,Color{35,52,69,255});
    DrawRectangle(64,694,(int)(1152.f*frame/(CHAPTERS*CHAPTER-1)),4,CYAN);
    text("Based on Cabalar 2021, sec. 2 / Aguado et al. 2013, secs. 2-3",64,673,16,DIM,1030);
}
} // anonymous namespace

void register_ht_explainer(std::vector<Comp>& out){
    Comp c; c.name="here-there-explainer"; c.w=1280; c.h=720;
    c.fps=FPS; c.duration=CHAPTERS*CHAPTER; c.music=true;
    auto* root=addRoot(c.roots,0,c.duration);
    root->fn=[](const Ctx&,int frame){ explain(frame); };
    // Quiet, fully procedural tonal bed; the explanation is in paced on-screen captions.
    for(int chapter=0;chapter<CHAPTERS;++chapter){
        double start=chapter*12.;
        c.song.push_back({start,11.9,chapter%2?146.83:130.81,.11,false});
        c.song.push_back({start+1,10.7,chapter%2?220.:196.,.055,false});
        for(int i=0;i<3;++i) c.song.push_back({start+.3+i*.35,.7,392.*(i==1?1.25:i==2?1.5:1.),.04,true});
    }
    out.push_back(std::move(c));
}
} // namespace rmtn
