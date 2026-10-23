#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#include "rmtn/timeline.hpp"
#include "rmtn/here_there.hpp"
#include "rmtn/fonts.hpp"

namespace rmtn { namespace {
constexpr int PLAIN_FPS=30, PLAIN_SHOT=12*PLAIN_FPS, PLAIN_SHOTS=9;
const Color PB{15,24,36,255}, PC{26,40,57,255}, PT{244,246,249,255};
const Color PM{184,199,214,255}, PA{255,202,117,255}, PH{100,224,209,255};
const Color POK{146,231,170,255}, PNO{255,155,160,255};
int plainLayoutErrors=0;

Font plainFont(bool bold=false){
    static Font regular{}, heading{};
    Font& font=bold?heading:regular;
    if(!font.texture.id){
        font=LoadFontEx(systemFontPath(bold?"segoeuib.ttf":"segoeui.ttf").c_str(),64,nullptr,0);
        if(!IsFontValid(font) || font.texture.id==GetFontDefault().texture.id)
            throw std::runtime_error("Segoe UI was not loaded; refusing unreadable default font fallback");
        SetTextureFilter(font.texture,TEXTURE_FILTER_BILINEAR);
    }
    return font;
}
std::vector<std::string> wrapPlain(const std::string& message,float size,float width,bool bold=false){
    std::vector<std::string> lines;
    std::string line,word;
    auto flushWord=[&](){
        if(word.empty()) return;
        std::string next=line.empty()?word:line+" "+word;
        if(!line.empty() && MeasureTextEx(plainFont(bold),next.c_str(),size,0).x>width){
            lines.push_back(line); line=word;
        } else line=next;
        word.clear();
    };
    for(char ch:message){
        if(ch==' ' || ch=='\n'){
            flushWord();
            if(ch=='\n'){ lines.push_back(line); line.clear(); }
        }else word+=ch;
    }
    flushWord(); if(!line.empty()) lines.push_back(line);
    return lines;
}
float plainText(const char* message,float x,float y,float width,float size,Color color,bool bold=false,float bottom=680){
    // Keep type size fixed. Add lines instead of silently shrinking a sentence.
    for(const auto& line:wrapPlain(message,size,width,bold)){
        if(y+size>bottom || MeasureTextEx(plainFont(bold),line.c_str(),size,0).x>width+.1f) ++plainLayoutErrors;
        DrawTextEx(plainFont(bold),line.c_str(),{x,y},size,0,color);
        y+=size*1.25f;
    }
    return y;
}
void plainPanel(float x,float y,float w,float h,Color accent){
    DrawRectangleRec({x,y,w,h},PC); DrawRectangleRec({x,y,w,4},accent);
}
void plainCaption(const char* line){
    DrawRectangleRec({56,570,1168,108},Color{22,36,51,255});
    DrawRectangle(56,570,5,108,PH);
    plainText(line,80,588,1112,32,PT,false,674);
}
void plainToken(const char* label,float x,float y,bool on,Color accent){
    DrawRectangleRec({x,y,190,58},on?withAlpha(accent,.17f):PB);
    DrawCircleV({x+20,y+29},7,on?accent:PM);
    plainText(label,x+40,y+9,140,32,on?PT:PM);
}
void plainList(float x,const char* label,unsigned mask,Color color){
    plainPanel(x,248,548,254,color);
    plainText(label,x+24,270,500,30,color,true);
    plainToken("Fire",x+24,342,(mask&ht::FIRE)!=0,color);
    plainToken("Smoke",x+278,342,(mask&ht::SMOKE)!=0,color);
    const char* state=mask==0?"Nothing on this list":mask==ht::FIRE?"Fire only":mask==ht::SMOKE?"Smoke only":"Fire and smoke";
    plainText(state,x+24,435,500,32,PT);
}
void plainPair(unsigned guess,unsigned kept){
    plainList(56,"OUR GUESS",guess,PA);
    plainList(676,"WHAT WE KEEP",kept,PH);
}
void plainStatus(const char* label,bool pass,float x,float y){
    DrawCircleV({x+9,y+20},7,pass?POK:PNO);
    plainText(label,x+30,y,510,30,pass?POK:PNO);
}
// An instruction to the viewer, not a pass/fail verdict.
void plainPrompt(const char* label,float x,float y){
    DrawCircleV({x+9,y+20},7,PA);
    plainText(label,x+30,y,510,30,PA);
}
void plainArrow(float x,float y,Color c){
    DrawLineEx({x,y},{x+90,y},3,c);
    DrawLineEx({x+90,y},{x+77,y-9},3,c);
    DrawLineEx({x+90,y},{x+77,y+9},3,c);
}

void drawPlain(int frame){
    int shot=std::min(frame/PLAIN_SHOT,PLAIN_SHOTS-1), local=frame%PLAIN_SHOT;
    float seconds=(float)local/PLAIN_FPS;
    float ease=(float)easeFn(E_OUT_CUBIC,std::min(1.f,seconds/.8f));
    ClearBackground(PB);
    plainText("A LITTLE LOGIC. NO MATH REQUIRED.",56,25,980,22,PM);
    char number[24]; std::snprintf(number,sizeof number,"%d / %d",shot+1,PLAIN_SHOTS);
    plainText(number,1120,25,110,22,PH);
    const char* title[]={"A rule is not a reason to invent a fire.","Separate a guess from what we keep.",
        "There is a middle ground.","Check the rule on BOTH lists.","Not established does not mean false.",
        "Try taking an unsupported claim away.","Now give the story a real starting fact.",
        "The same idea can follow a story over time.","That is the idea behind equilibrium."};
    plainText(title[shot],56,88+(1-ease)*12,1168,44,PT,true,210);
    DrawRectangle(56,208,(int)(1168*ease),2,Color{50,69,88,255});
    switch(shot){
    case 0:{
        plainText("Our only rule: if there is fire, there must be smoke.",56,235,1168,34,PH);
        const char* labels[]={"Neither","Smoke only","Fire and smoke"};
        const char* lines[]={"No fire, so the rule is not broken.","The rule does not forbid smoke on its own.","Fire has the smoke it needs."};
        for(int i=0;i<3;++i){
            float x=56.f+i*398;
            plainPanel(x,324,372,208,i==0?PH:PA);
            plainText(labels[i],x+20,346,332,32,PT,true);
            plainText(lines[i],x+20,403,332,30,PM,false,530);
        }
        plainCaption("Ordinary logic allows all three. But our rule alone gives us no reason to add fire or smoke.");
        break;
    }
    case 1:{
        plainPair(ht::FIRE|ht::SMOKE,0);
        plainText("Assume these for the moment.",80,515,520,30,PA);
        plainText("Try keeping fewer claims.",700,515,500,30,PH);
        plainCaption("The kept list can only contain claims from the guess. These are two views of ONE moment, not now and later.");
        break;
    }
    case 2:{
        const char* labels[]={"Not in the guess","Only a guess","Also established"};
        const char* detail[]={"We treat the claim as false in this scenario.","It is assumed, but not on the kept list.","The claim is on both lists."};
        for(int i=0;i<3;++i){
            float x=56.f+i*398;
            plainPanel(x,261,372,271,i==0?PM:i==1?PA:PH);
            plainText(labels[i],x+20,284,332,32,PT,true);
            plainText(detail[i],x+20,359,332,32,PM,false,529);
        }
        plainCaption("These are three logical statuses, not percentages. A claim cannot be kept if it was never in our guess.");
        break;
    }
    case 3:{
        int step=std::min((int)(seconds/4),2);
        unsigned guess=step==0?ht::FIRE:ht::FIRE|ht::SMOKE, kept=step==2?ht::FIRE:0;
        plainPair(guess,kept);
        bool left=!(guess&ht::FIRE)||(guess&ht::SMOKE), right=!(kept&ht::FIRE)||(kept&ht::SMOKE);
        plainStatus(left?"Our guess obeys the rule":"Our guess BREAKS the rule",left,80,515);
        plainStatus(right?"Kept list obeys the rule":"Kept list BREAKS the rule",right,700,515);
        const char* caption[]={"Guessing fire without smoke breaks our rule, even when the kept list is empty.",
            "Add smoke to the guess: both lists now obey the rule. That alone does not make this a stable explanation.",
            "Keep fire but not smoke: now the kept list breaks the rule. Both lists must obey it."};
        plainCaption(caption[step]); break;
    }
    case 4:{
        plainPair(ht::FIRE,0);
        plainText("Fire is still in the guess.",80,515,520,30,PA);
        plainText("Fire is not established here.",700,515,500,30,PH);
        plainCaption("We cannot call fire false just because it is not established. To say 'not fire', it must be absent from the guess too.");
        break;
    }
    case 5:{
        bool removed=seconds>=4;
        plainPair(ht::SMOKE,removed?0:ht::SMOKE);
        plainStatus("Guess stays unchanged",true,80,515);
        if(removed) plainStatus("Rule still obeyed after removal",true,700,515);
        else plainPrompt("Try removing smoke",700,515);
        plainCaption(removed?"The rule still works without keeping smoke. Reject the padded guess: smoke was not required.":
            "Keep the guess fixed. Can we keep fewer claims while still obeying every rule?");
        break;
    }
    case 6:{
        plainPanel(56,261,548,271,PA); plainPanel(676,261,548,271,PH);
        plainText("RULE ALONE",80,282,500,30,PA,true);
        plainText("Nothing needs to happen.",80,347,500,34,PT,true);
        plainText("No fire. No smoke.",80,430,500,32,PM);
        plainText("ADD A FACT: THERE IS FIRE",700,282,500,30,PH,true);
        plainText("Now smoke is required.",700,347,500,34,PT,true);
        plainText("Fire and smoke both stay.",700,430,500,32,PM);
        plainCaption("Removing fire would break the fact. Removing smoke would break the rule. Now both claims have to stay.");
        break;
    }
    case 7:{
        plainText("A new rule: fire must be followed by smoke at the next moment.",56,235,1168,32,PM);
        const char* time[]={"START","NEXT MOMENT","AFTER THAT"};
        const char* event[]={"Fire","Smoke","Nothing"};
        for(int i=0;i<3;++i){
            float x=56.f+i*398;
            plainPanel(x,314,372,188,i==0?PA:PH);
            plainText(time[i],x+20,335,332,27,PM,true);
            plainText(event[i],x+20,394,332,40,PT,true);
            if(i<2) plainArrow(x+270,468,PH);
        }
        plainText("The story continues ...",56,515,1168,30,PM);
        plainCaption("Start with a fire. Smoke must follow. Keep the whole guessed story fixed, and test whether any claims can be dropped.");
        break;
    }
    default:{
        plainPanel(56,251,1168,292,PH);
        plainText("HERE-AND-THERE",80,272,1100,30,PH,true);
        plainText("The kept list and the guess are two levels of truth, not a proof algorithm.",80,317,1100,32,PT);
        plainText("EQUILIBRIUM",80,414,1100,30,PA,true);
        plainText("Accept a complete guess only if no smaller kept list still obeys the theory.",80,459,1100,32,PT,false,541);
        plainCaption("For temporal equilibrium, compare whole stories. A smaller kept story differs at one or more moments, not necessarily all.");
        break;
    }
    }
    // Sources are a small footer, never mixed into the lesson's body copy.
    plainText("Pedro Cabalar: RW2021.pdf, section 2 | TEL survey: tel-dp60.pdf, sections 2-3",56,688,1168,18,PM,false,720);
    DrawRectangle(56,682,(int)(1168.f*frame/(PLAIN_SHOTS*PLAIN_SHOT-1)),3,PH);
    float fade=1-std::min(1.f,local/10.f);
    if(local>PLAIN_SHOT-11) fade=std::max(fade,(local-(PLAIN_SHOT-11))/10.f);
    if(fade>0) DrawRectangleRec({0,75,1280,605},withAlpha(PB,fade));
}
} // anonymous namespace
void register_ht_plain(std::vector<Comp>& out){
    Comp c; c.name="here-there-plain"; c.w=1280; c.h=720; c.fps=PLAIN_FPS;
    c.duration=PLAIN_SHOTS*PLAIN_SHOT; c.music=false;
    auto* root=addRoot(c.roots,0,c.duration);
    root->fn=[](const Ctx&,int frame){ drawPlain(frame); };
    out.push_back(std::move(c));
}
}
