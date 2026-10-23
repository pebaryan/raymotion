#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>
#include "rmtn/timeline.hpp"
#include "here_there.hpp"
#include "ht_feed_layout.hpp"
#include "rmtn/fonts.hpp"
namespace rmtn {
struct FeedState { unsigned h=2,t=2; bool fact=false,probe=false; };
FeedState feedState(int frame){
 float s=frame/30.f; FeedState v;
 if(s>=30 && s<40) v.h=s<34?0:2;
 if(s>=40 && s<56){ int phase=int((s-40)/4); v.t=phase==0?1:3; v.h=phase<2?0:phase==2?1:3; }
 if(s>=62 && s<70) v.h=0;
 if(s>=70){ v.h=v.t=3; v.fact=true;
   if(s>=73 && s<77){v.h=2;v.probe=true;}
   if(s>=77 && s<81){v.h=1;v.probe=true;}
 }
 return v;
}
namespace {
const Color paper{249,240,218,255}, ink{39,54,64,255}, amber{225,155,57,255}, teal{35,153,148,255}, coral{210,76,58,255}, muted{99,108,108,255};
int feedLayoutErrors=0, feedTextCalls=0;
// Designed and layout-tested for Segoe UI; fonts::load throws rather than substituting.
Font feedFont(bool bold=false){ return fonts::load({bold?"segoeuib.ttf":"segoeui.ttf"},64); }
void feedText(const std::string& text,float x,float y,float width,float size,Color color=ink,bool bold=false,float bottom=1320){
 std::vector<std::string> lines; std::string line,word;
 auto flush=[&](){if(word.empty())return; auto next=line.empty()?word:line+" "+word;
  if(!line.empty() && MeasureTextEx(feedFont(bold),next.c_str(),size,0).x>width){lines.push_back(line);line=word;}else line=next;word.clear();};
 for(char ch:text){if(ch==' '||ch=='\n'){flush();if(ch=='\n'){lines.push_back(line);line.clear();}}else word+=ch;} flush();if(!line.empty())lines.push_back(line);
 for(auto& s:lines){auto m=MeasureTextEx(feedFont(bold),s.c_str(),size,0); ++feedTextCalls;
  if(x<0||y<0||x+m.x>1080.1f||m.x>width+.1f||y+size>bottom||y+size>1350)++feedLayoutErrors;
  DrawTextEx(feedFont(bold),s.c_str(),{x,y},size,0,color);y+=size*1.16f;
 }
}
float smooth(float a){a=std::clamp(a,0.f,1.f);return a*a*(3-2*a);}
float mix(float a,float b,float u){return a+(b-a)*u;}
void check(float x,float y,bool pass,Color color){
 if(pass){DrawLineEx({x,y+13},{x+10,y+24},6,color);DrawLineEx({x+10,y+24},{x+32,y},6,color);}
 else {DrawLineEx({x,y},{x+24,y+24},6,coral);DrawLineEx({x+24,y},{x,y+24},6,coral);}
}
void flame(float x,float y,float size,float time,Color color=coral){
 // Cut-paper teardrops; their gently changing tips are absolute-frame functions.
 float lean=std::sin(time*3)*size*.065f;
 DrawEllipse(int(x+5),int(y+8),size*.40f,size*.13f,Color{70,52,39,35});
 DrawTriangle({x-size*.39f,y-size*.27f},{x+size*.35f,y-size*.23f},{x+lean,y-size*1.20f},color);
 DrawEllipse(int(x),int(y-size*.30f),size*.38f,size*.42f,color);
 DrawTriangle({x-size*.17f,y-size*.14f},{x+size*.18f,y-size*.12f},{x+size*.12f,y-size*.70f},amber);
 DrawLineEx({x-size*.38f,y+5},{x+size*.35f,y+14},9,ink);
 DrawLineEx({x-size*.35f,y+14},{x+size*.37f,y+4},9,ink);
}
void smoke(float x,float y,float size,float time,float alpha=1){
 Color shade=withAlpha(ink,alpha*.17f),c=withAlpha(Color{132,155,169,255},alpha);
 // One contiguous puff and a curling stem, not multiple smoke claims.
 DrawLineBezier({x+4,y+12},{x-7,y-size*.70f},size*.20f,shade);
 DrawCircleV({x+5,y-size*.65f},size*.34f,shade);
 DrawCircleV({x-size*.28f+std::sin(time)*3,y-size*.83f},size*.32f,c);
 DrawCircleV({x+size*.13f,y-size*1.04f},size*.39f,c);
 DrawCircleV({x+size*.40f,y-size*.76f},size*.31f,c);
 DrawEllipse(int(x+size*.02f),int(y-size*.61f),size*.53f,size*.27f,c);
 DrawLineBezier({x+size*.28f,y-size*.75f},{x-size*.07f,y-size*.96f},4,withAlpha(paper,alpha*.7f));
 DrawLineBezier({x-5,y+8},{x+size*.15f,y-size*.54f},size*.13f,c);
}
void ribbon(float x,float y,float width,unsigned mask,float t,bool trace=false){
 DrawTriangle({x-12,y+8},{x-12,y+82},{x+22,y+45},amber);
 DrawRectangleRounded({x,y,width,82},.18f,8,Color{237,220,179,255});
 flame(x+40,y+61,30,t); smoke(x+width-45,y+63,32,t);
 float start=x+83,end=x+width-94; bool fire=mask&1,pass=!(mask&1)||(mask&2);
 Color c=fire?(pass?teal:coral):muted;
 float stop=fire&&!pass?mix(start,end,.58f):end;
 DrawLineEx({start,y+39},{stop,y+39},5,c);
 if(fire&&!pass){DrawLineEx({stop+12,y+23},{stop+12,y+55},5,coral);}
 else {DrawTriangle({end+10,y+39},{end-5,y+29},{end-5,y+49},c);}
 if(fire||trace){float u=trace?smooth((t-3)/1.5f):std::fmod(t*.44f,1.f);DrawCircleV({mix(start,stop,u),y+39},7,teal);}
 if(!trace){check(x+width-34,y+105,pass,fire?teal:muted);feedText(!fire?"No fire to check":pass?"Rule obeyed":"Missing smoke",x,y+98,width-40,36,c,true,y+185);}
}
void world(float x,float w,unsigned mask,float t,bool there,Color accent,const char* label,const char* sub,bool pinned,float fireOffset=0,float smokeOffset=0){
 // Floating paper island, thought-cloud crown, and matching cut-paper scenery.
 DrawEllipse(int(x+w/2+7),939,int(w*.43f),23,Color{70,52,39,25});
 DrawTriangle({x+24,891},{x+w*.50f,960},{x+w-22,891},Color{189,155,108,255});
 DrawRectangleRounded({x,875,w,35},.5f,8,accent);
 DrawCircleV({x+44,864},22,accent);DrawCircleV({x+w-37,870},17,accent);
 DrawLineEx({x+45,855},{x+65,833},4,accent);DrawLineEx({x+w-42,850},{x+w-63,828},4,accent);
 DrawRectangleRounded({x+8,531,w-16,314},.35f,16,Color{255,251,239,255});
 DrawCircleV({x+91,854},15,Color{255,251,239,255});DrawCircleV({x+74,877},8,Color{255,251,239,255});
 feedText(label,x+16,443,w-20,44,accent,true,505);
 feedText(sub,x+16,497,w-20,30,ink,false,534);
 float fireScale=1,smokeScale=1;
 if(t>=40 && t<56){
  if(there && t<41)fireScale=.55f+.45f*smooth(t-40);
  if(!there && t>=48 && t<49)fireScale=.55f+.45f*smooth(t-48);
  if(there && t>=44 && t<45)smokeScale=smooth(t-44);
  if(!there && t>=52 && t<53)smokeScale=smooth(t-52);
 }
 if(mask&1)flame(x+w*.28f+fireOffset,825,79*fireScale,t);
 if(mask&2)smoke(x+w*.68f+smokeOffset,805,93*smokeScale,t);
 if(pinned){DrawLineEx({x+w-39,543},{x+w-50,584},7,ink);DrawCircleV({x+w-35,536},17,coral);}
 ribbon(x+8,977,w-16,mask,t);
}
struct Caption {float start; const char* text;};
const Caption captions[]={
 {0,"If nothing contradicts a claim, should you believe it?"},
 {5,"Our only rule: if there is fire, there is smoke."},
 {9,"Someone guesses: \"There's smoke.\""},
 {13,"Nothing breaks the rule. But what backs up the guess?"},
 {18,"Here-and-There gives us two views of the same claim."},
 {28,"There: what we assume is true."},{31,"Here: which of those claims we keep."},
 {34,"We can keep less. We cannot add something new."},{37,"Two views of one moment. Not now and later."},
 {40,"The rule has to work on BOTH sides."},
 {56,"Now test our original guess."},{60,"Keep the guess fixed. Try keeping less."},
 {66,"So the smoke was not required."},
 {70,"Now someone gives us a fact: there IS fire."},{76,"This time, both claims have to stay."},
 {82,"Nothing says it's wrong."},{85,"But what says it's right?"}
};
void drawFeed(int frame){
 float t=frame/30.f; auto s=feedState(frame); ClearBackground(paper);
 // Small reproducible paper flecks: decorative, not a stateful simulation.
 for(int i=0;i<75;++i){float x=float((i*167+23)%1080),y=float((i*223+71)%1350);DrawCircleV({x,y},1.4f,Color{169,141,100,26});}
 bool labelsOnly=std::getenv("RAYMOTION_FEED_LABELS_ONLY")!=nullptr;
 int ci=0;for(int i=1;i<int(sizeof(captions)/sizeof(*captions));++i)if(t>=captions[i].start)ci=i;
 float age=t-captions[ci].start; float opacity=ci==0?1.f:smooth(age/.25f);
 if(!labelsOnly)feedText(captions[ci].text,64,73,952,ci==0?80.f:62.f,withAlpha(ink,opacity),true,375);
 if(t<18){
  // Smoke first appears INSIDE a GUESS bubble; the ground remains empty.
  float bob=std::sin(t*1.1f)*5;
  DrawEllipse(550,910,355,29,Color{70,52,39,22});
  DrawRectangleRounded({150,462+bob,780,367},.32f,20,Color{255,251,239,255});
  DrawCircleV({337,853+bob},23,Color{255,251,239,255});DrawCircleV({305,889+bob},12,Color{255,251,239,255});
  feedText("GUESS",213,496+bob,500,48,amber,true);
  if(t>=9){float a=smooth((t-9)/1.2f);smoke(551,790+(1-a)*27+bob,145,t,a);}
  ribbon(210,940,660,0,t,true);
  Color edge=withAlpha(amber,.4f+.25f*std::sin(t*2));DrawRectangleRoundedLinesEx({228,1113,624,132},.1f,8,4,edge);
  feedText("GIVEN FACTS",266,1126,550,34,muted,true,1170);feedText("Nothing given",266,1174,550,40,ink,false,1245);
 }else{
  float split=smooth((t-18)/2.f);float lx=mix(150,45,split),rx=mix(150,595,split),w=mix(780,440,split);
  // During the unfold the two versions share an anchor and then separate.
  bool pin=t>=60&&t<70;
  unsigned hm=s.h;float hoFire=0,hoSmoke=0;
  if(t>=30&&t<34){hm|=2;hoSmoke=-354*(t<33?smooth((t-30)/.9f):1-smooth(t-33));}
  if(t>=62&&t<70){hm|=2;hoSmoke=-354*smooth((t-62)/1.4f);}
  if(t>=73&&t<77){hm|=1;float u=t<76?smooth((t-73)/.65f):1-smooth(t-76);hoFire=-178*u;}
  if(t>=77&&t<81){hm|=2;float u=t<80?smooth((t-77)/.65f):1-smooth(t-80);hoSmoke=-354*u;}
  // Off-stage claims go into a temporary tray between the paper islands.
  if(hoFire||hoSmoke){DrawRectangleRounded({490,650,100,215},.15f,8,Color{235,218,194,255});feedText("TEST",497,662,88,30,coral,true);}
  world(lx,w,s.t,t,true,amber,"THERE","our guess",pin);
  world(rx,w,hm,t,false,teal,"HERE","what we keep",false,hoFire,hoSmoke);
  // Repaint the HERE ribbon using the logical test state, not the token's travelling position.
  DrawRectangleRec({rx,969,w,184},paper);ribbon(rx+8,977,w-16,s.h,t);
  if(t>=18&&t<28){
   float pulse=.5f+.5f*std::sin(t*3);DrawLineEx({lx+w*.68f,636},{rx+w*.68f,636},3,withAlpha(teal,pulse*.6f));
   DrawCircleLines(int(lx+w*.68f),636,60,amber);DrawCircleLines(int(rx+w*.68f),636,60,teal);
  }
  if(t>=34&&t<37){
   // Rejected entrance never updates the semantic HERE state.
   // Keep the whole token (including thick logs and shadow) outside HERE.
   // The narrow remaining gutter permits only a small OUTWARD bounce.
   using namespace feedLayout;
   constexpr float size=REJECT_FLAME_SIZE,margin=.5f;
   float boundary=rx+w-10,leftExtent=size*.38f+4.5f,rightExtent=5+size*.40f;
   float start=boundary+leftExtent+margin;
   float travel=std::max(0.f,1080-margin-rightExtent-start);
   float u=(t-34)/3,x=start+travel*std::sin(u*3.14159265f);
   DrawLineEx({boundary,REJECT_TOP},{boundary,REJECT_BOTTOM},5,coral);flame(x,REJECT_FLAME_Y,size,t);
   feedText("Not in our guess",610,1167,420,44,coral,true,1285);
  }
  if(t>=40&&t<56){bool left=t<48;DrawRectangleRoundedLinesEx({left?lx-5:rx-5,538,w+10,598},.08f,8,4,left?amber:teal);}
  if(t>=56&&t<70){feedText("next test",65,386,450,34,muted);if(t>=66)feedText("NOT BACKED UP",lx+16,1170,w-20,44,coral,true,1295);}
  if(t>=70){
   float y=mix(377,381,smooth((t-70)/.8f));
   DrawRectangleRounded({feedLayout::BADGE_X,y,feedLayout::BADGE_W,feedLayout::BADGE_H},.1f,8,amber);feedText("GIVEN: FIRE",285,y+4,510,44,ink,true,y+feedLayout::BADGE_H);
   if(t<82){bool fail=s.probe&&!(s.h&1);check(662,1180,!fail,teal);feedText(fail?"Fact broken - return":"Given fact kept",607,1220,430,36,fail?coral:teal,true,1320);}
  }
  if(t>=18&&t<56){feedText("SAME MOMENT",354,388,580,34,muted,true,435);}
  if(t>=82){
   // Echo the initial puff, visibly separate from the supported final world.
   DrawRectangleRounded({45,1160,485,146},.1f,8,Color{238,223,197,255});smoke(110,1275,55,t,.6f);
   feedText("Only a guess",177,1180,335,40,muted,true,1280);
   if(!labelsOnly)feedText("HT helps us separate the guess from the test.",586,1168,450,42,ink,false,1330);
  }
 }
}
} // anonymous namespace
void register_ht_feed(std::vector<Comp>& out) {
 Comp c; c.name="here-there-feed"; c.w=1080; c.h=1350; c.fps=30; c.duration=2640; c.music=false;
 auto* root=addRoot(c.roots,0,c.duration); root->fn=[](const Ctx&,int frame){drawFeed(frame);};
 out.push_back(std::move(c));
} }
