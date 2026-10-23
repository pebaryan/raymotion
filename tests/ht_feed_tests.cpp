#include <raylib.h>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>
#include "../src/rmtn/ht_feed_layout.hpp"
namespace staging {
using namespace rmtn::feedLayout;
struct Triangle { Vector2 a,b,c; Color color; };
struct Line { Vector2 a,b; float width; };
struct Text { std::string text; Vector2 at; Vector2 size; };
std::vector<Triangle> triangles;
std::vector<Line> lines;
std::vector<Text> texts;
std::vector<Rectangle> badges;
float tokenLeft=1080,tokenRight=0; bool tokenActive=false;
void bounds(float left,float right){if(tokenActive){tokenLeft=std::min(tokenLeft,left);tokenRight=std::max(tokenRight,right);}}
void triangle(Vector2 a,Vector2 b,Vector2 c,Color color){triangles.push_back({a,b,c,color});bounds(std::min({a.x,b.x,c.x}),std::max({a.x,b.x,c.x}));DrawTriangle(a,b,c,color);}
void line(Vector2 a,Vector2 b,float width,Color color){lines.push_back({a,b,width});bounds(std::min(a.x,b.x)-width/2,std::max(a.x,b.x)+width/2);DrawLineEx(a,b,width,color);if(a.y==REJECT_TOP && b.y==REJECT_BOTTOM)tokenActive=true;}
void ellipse(int x,int y,float horizontal,float vertical,Color color){bounds(x-horizontal,x+horizontal);DrawEllipse(x,y,horizontal,vertical,color);}
void text(Font font,const char* value,Vector2 at,float size,float spacing,Color color){tokenActive=false;texts.push_back({value,at,MeasureTextEx(font,value,size,spacing)});DrawTextEx(font,value,at,size,spacing,color);}
void rounded(Rectangle rect,float roundness,int segments,Color color){if(rect.x==BADGE_X && rect.width==BADGE_W && rect.height==BADGE_H)badges.push_back(rect);DrawRectangleRounded(rect,roundness,segments,color);}
void reset(){triangles.clear();lines.clear();texts.clear();badges.clear();tokenLeft=1080;tokenRight=0;tokenActive=false;}
}
#define DrawEllipse staging::ellipse
#define DrawTriangle staging::triangle
#define DrawLineEx staging::line
#define DrawTextEx staging::text
#define DrawRectangleRounded staging::rounded
#include "../src/ht_feed.cpp"
#undef DrawEllipse
#undef DrawTriangle
#undef DrawLineEx
#undef DrawTextEx
#undef DrawRectangleRounded
int main(){
    std::vector<rmtn::Comp> comps; rmtn::register_ht_feed(comps);
    bool ok=comps.size()==1 && comps[0].name=="here-there-feed" && comps[0].w==1080 && comps[0].h==1350 && comps[0].duration==2640 && comps[0].fps==30 && !comps[0].music;
    std::printf("feed registration=%d\n",ok);
    bool semantic=false;
#ifdef RAYMOTION_FEED_STATE
    semantic=true;
    auto expect=[&](int second,unsigned h,unsigned t,bool fact,bool probe){
        auto s=rmtn::feedState(second*30); semantic &= s.h==h && s.t==t && s.fact==fact && s.probe==probe;
    };
    expect(17,2,2,false,false); expect(31,0,2,false,false);
    expect(41,0,1,false,false); expect(45,0,3,false,false);
    expect(49,1,3,false,false); expect(53,3,3,false,false);
    expect(65,0,2,false,false); expect(74,2,3,true,true);
    expect(78,1,3,true,true); expect(81,3,3,true,false);
    for(int f=0;f<2640;++f){
        auto s=rmtn::feedState(f); semantic &= !(s.h & ~s.t);
        if(!rmtn::ht::theory(s.h,s.t,s.fact)) semantic &= s.probe || (f>=1200 && f<1680);
        if(s.probe) semantic &= s.t==3 && s.fact && !rmtn::ht::theory(s.h,s.t,true);
        if(f>=1860 && f<2100) semantic &= s.t==2 && s.h==0 && rmtn::ht::theory(s.h,s.t);
    }
    for(int phase=0;phase<4;++phase){
        auto s=rmtn::feedState((41+4*phase)*30);
        semantic &= rmtn::ht::implication(s.h,s.t,1,2)==(phase==1 || phase==3);
    }
    semantic &= rmtn::ht::theory(0,2) && !rmtn::ht::equilibrium(2) && rmtn::ht::equilibrium(0) && rmtn::ht::equilibrium(3,true);
#endif
    std::printf("feed storyboard semantics=%d\n",semantic);
    bool rendered=false;
#ifdef RAYMOTION_FEED_RENDER
    SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN); InitWindow(540,675,"HT feed in-memory tests");
    auto canvas=LoadRenderTexture(1080,1350);
    rmtn::feedLayoutErrors=0; rmtn::feedTextCalls=0;
    bool badgeClear=true; int badgeFrames=0;
    bool backedUpAttached=true; int backedUpFrames=0;
    bool rejectedOutside=true; int rejectedFrames=0;
    for(int f=0;f<2640;++f){
        staging::reset();
        BeginTextureMode(canvas); comps[0].roots[0]->fn(rmtn::Ctx{},f); EndTextureMode();
        if(f>=70*30){
            ++badgeFrames; bool titleFound=false,tagFound=false; float titleTop=1350;
            for(const auto& text:staging::texts)if(text.text=="HERE" || text.text=="THERE"){titleFound=true;titleTop=std::min(titleTop,text.at.y);}
            badgeClear &= titleFound && staging::badges.size()==1;
            for(const auto& badge:staging::badges){
                badgeClear &= badge.y>375 && badge.y+badge.height<titleTop;
                for(const auto& text:staging::texts)if(text.text=="GIVEN: FIRE"){
                    tagFound=true;
                    badgeClear &= text.at.x>=badge.x && text.at.x+text.size.x<=badge.x+badge.width && text.at.y>=badge.y && text.at.y+text.size.y<=badge.y+badge.height;
                }
            }
            badgeClear &= tagFound;
        }
        if(f>=66*30 && f<70*30){
            ++backedUpFrames; bool tagFound=false,thereFound=false,hereFound=false;
            staging::Text tag{},there{},here{};
            for(const auto& text:staging::texts){
                if(text.text=="NOT BACKED UP"){tag=text;tagFound=true;}
                if(text.text=="THERE"){there=text;thereFound=true;}
                if(text.text=="HERE"){here=text;hereFound=true;}
            }
            backedUpAttached &= tagFound && thereFound && hereFound && tag.at.x>=there.at.x && tag.at.x+tag.size.x<here.at.x && tag.at.y>there.at.y;
        }
        if(f>=34*30 && f<37*30){
            ++rejectedFrames; bool boundaryFound=false,flameFound=false; float boundary=0;
            for(const auto& line:staging::lines)if(line.a.y==rmtn::feedLayout::REJECT_TOP && line.b.y==rmtn::feedLayout::REJECT_BOTTOM){boundary=line.a.x;boundaryFound=true;}
            for(const auto& tri:staging::triangles)if(std::abs(tri.c.y-(rmtn::feedLayout::REJECT_FLAME_Y-rmtn::feedLayout::REJECT_FLAME_SIZE*1.20f))<.01f){
                flameFound=true;
                rejectedOutside &= std::min({tri.a.x,tri.b.x,tri.c.x})>boundary && std::max({tri.a.x,tri.b.x,tri.c.x})<1080;
                // Bounds captured from every actual triangle, ellipse and thick line.
                rejectedOutside &= staging::tokenLeft>boundary && staging::tokenRight<1080;
            }
            rejectedOutside &= boundaryFound && flameFound;
        }
    }
    std::printf("GIVEN FIRE badge clear of caption and world titles=%d frames=%d\n",badgeClear,badgeFrames);
    std::printf("NOT BACKED UP attached to THERE=%d frames=%d\n",backedUpAttached,backedUpFrames);
    std::printf("rejected flame outside HERE and inside canvas=%d frames=%d\n",rejectedOutside,rejectedFrames);
    auto read=[&](int f){ BeginTextureMode(canvas); comps[0].roots[0]->fn(rmtn::Ctx{},f); EndTextureMode(); return LoadImageFromTexture(canvas.texture); };
    Image a=read(2240),b=read(120),c=read(2240);
    bool same=a.width==1080 && a.height==1350 && a.format==c.format;
    int bytes=GetPixelDataSize(a.width,a.height,a.format); bool changes=false;
    auto* p=static_cast<unsigned char*>(a.data); auto* q=static_cast<unsigned char*>(c.data); auto* r=static_cast<unsigned char*>(b.data);
    for(int i=0;i<bytes;++i){same &= p[i]==q[i]; changes |= p[i]!=r[i];}
    rendered=badgeClear && badgeFrames==540 && backedUpAttached && backedUpFrames==120 && rejectedOutside && rejectedFrames==90 && same && changes && rmtn::feedLayoutErrors==0 && rmtn::feedTextCalls>10000 && rmtn::feedFont().texture.id!=GetFontDefault().texture.id;
    std::printf("all-frame feed layout overflows=%d textCalls=%d deterministic=%d changing=%d\n",rmtn::feedLayoutErrors,rmtn::feedTextCalls,same,changes);
    UnloadImage(a); UnloadImage(b); UnloadImage(c); UnloadRenderTexture(canvas); CloseWindow();
#endif
    std::printf("feed real-GL render=%d\n",rendered);
    return ok && semantic && rendered?0:1;
}
