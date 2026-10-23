#include "../examples/showcase/fx_present.cpp"
#include "rmtn/preview.hpp"
#include "rmtn/fonts.hpp"
#include <cstdio>

// Distinct grey levels across rows [y0,y1): 2 for hard (aliased) edges; N x N supersampling
// can produce up to N*N+1 coverage levels along a slanted edge.
static int greyLevels(const Image& img,int y0,int y1){
    bool seen[256]={};
    int n=0;
    for(int y=y0;y<y1;++y) for(int x=0;x<img.width;++x){
        int v=GetImageColor(img,x,y).r; if(!seen[v]){ seen[v]=true; ++n; }
    }
    return n;
}
static Image readOutput(const rmtn::Canvas& canvas){
    Image img=LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    return img;
}

int main() {
    // Every easing must start at 0 and land on 1; bounce must never leave [0,1].
    for(int e=0;e<rmtn::E_COUNT;++e){
        if(std::fabs(rmtn::easeFn(e,0))>1e-9 || std::fabs(rmtn::easeFn(e,1)-1)>1e-9){
            std::fprintf(stderr,"FAIL: easing %s endpoints\n",rmtn::EASE_NAMES[e]); return 1;
        }
    }
    for(int i=0;i<=1000;++i){
        double v=rmtn::easeFn(rmtn::E_BOUNCE,i/1000.0);
        if(v<-1e-9 || v>1+1e-9){ std::fprintf(stderr,"FAIL: bounce out of range at %d: %g\n",i,v); return 1; }
    }
    Rectangle fit=rmtn::fitCanvas(1080,1350,640,480);
    if(std::fabs(fit.x-128)>0.01f || std::fabs(fit.y)>0.01f ||
       std::fabs(fit.width-384)>0.01f || std::fabs(fit.height-480)>0.01f){
        std::fprintf(stderr,"FAIL: portrait preview stretched or cropped\n"); return 1;
    }
    Rectangle landscape=rmtn::fitCanvas(1280,720,360,450);
    if(std::fabs(landscape.width-360)>0.01f || std::fabs(landscape.height-202.5f)>0.01f ||
       std::fabs(landscape.y-123.75f)>0.01f){
        std::fprintf(stderr,"FAIL: landscape preview not centered\n"); return 1;
    }
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 240, "raymotion regression");
    bool ok=true;
    auto expect=[&](bool cond,const char* what){ if(!cond){ std::fprintf(stderr,"FAIL: %s\n",what); ok=false; } };

    // Timeline dispatch through the canvas, in composition coordinates at every scale:
    // a white rect at x=[100,200) and a slanted white triangle on black.
    rmtn::Comp c; c.w=320; c.h=240; c.duration=10; c.background=BLACK;
    int calls=0;
    auto* n=rmtn::addRoot(c.roots,0,10);
    n->fn=[&](const rmtn::Ctx&,int){
        ++calls;
        DrawRectangle(100,20,100,40,WHITE);
        DrawTriangle({20,230},{300,120},{20,120},WHITE);
    };
    int levels[3]={};
    for(int ss : {1,2,4}){
        rmtn::Canvas canvas;
        expect(canvas.load(320,240,ss),"canvas load");
        expect(canvas.scale()==ss && canvas.drawTarget().texture.width==320*ss,"canvas scale");
        expect(canvas.output().texture.width==320 && canvas.output().texture.height==240,"canvas output size");
        rmtn::presentFrame(c,0,canvas);
        Image img=readOutput(canvas);
        expect(GetImageColor(img,99,40).r<8 && GetImageColor(img,100,40).r>247 &&
               GetImageColor(img,199,40).r>247 && GetImageColor(img,200,40).r<8,"rect stays at composition coordinates");
        expect(GetImageColor(img,150,40).a==255,"opaque alpha survives the resolve");
        levels[ss/2]=greyLevels(img,125,225);
        UnloadImage(img);
        canvas.unload();
    }
    std::printf("edge grey levels ss1=%d ss2=%d ss4=%d\n",levels[0],levels[1],levels[2]);
    expect(calls==3,"timeline callback executed once per frame");
    expect(levels[0]<=2,"ss=1 edge is hard");
    expect(levels[1]>=4 && levels[1]<=5 && levels[2]>levels[1],"supersampling antialiases edges");

    // Concave fills: every point of a star is filled, and a translucent blob is uniform
    // (non-overlapping triangles), whatever the vertex winding.
    {
        rmtn::Comp shapes; shapes.w=320; shapes.h=240; shapes.duration=1; shapes.background=BLACK;
        rmtn::addRoot(shapes.roots,0,1)->fn=[](const rmtn::Ctx& cx,int){
            rmtn::dr::star(cx,{100,120},90,36,5,-90,WHITE);
            rmtn::dr::blob(cx,{240,120},50,14,4,0,Color{255,255,255,128});
        };
        rmtn::Canvas canvas; canvas.load(320,240,1);
        rmtn::presentFrame(shapes,0,canvas);
        Image img=readOutput(canvas);
        int tips=0;
        for(int k=0;k<5;++k){
            double a=(-90+72*k)*PI/180;
            Color p=GetImageColor(img,(int)(100+70*std::cos(a)),(int)(120+70*std::sin(a)));
            tips+=p.r>250;
        }
        int lo=255,hi=0;
        for(int y=95;y<146;++y) for(int x=215;x<266;++x){ int v=GetImageColor(img,x,y).r; lo=std::min(lo,v); hi=std::max(hi,v); }
        std::printf("star tips filled=%d/5 blob interior range=%d..%d\n",tips,lo,hi);
        expect(tips==5,"concave star fully filled");
        expect(hi-lo<=1 && lo>100,"translucent blob uniform");
        UnloadImage(img); canvas.unload();
    }

    // Fonts: bundled Inter loads, is cached, and is not raylib's default bitmap font.
    Font inter=rmtn::fonts::ui();
    expect(inter.texture.id!=GetFontDefault().texture.id && inter.baseSize==96,"bundled Inter loads");
    expect(rmtn::fonts::ui().texture.id==inter.texture.id,"fonts are cached");
    expect(!rmtn::fonts::find("Inter-Bold.ttf").empty(),"bundled bold face found");
    bool threw=false;
    try { rmtn::fonts::load({"no-such-font-file.ttf"}); } catch(const std::exception&){ threw=true; }
    expect(threw,"missing font is an error, not a silent fallback");

    // Read the actual GL uniform state, not merely whether the shader compiled.
    rmtn::fxEnsure(320,240);
    rmtn::grainOverlay(0.25f,320,240,0.35f,3);
    rlDrawRenderBatchActive();
    auto getFloat=(void (*)(unsigned int,int,float*))rlGetProcAddress("glGetUniformfv");
    auto getInt=(void (*)(unsigned int,int,int*))rlGetProcAddress("glGetUniformiv");
    float resolution[2]={}; int mode=0;
    getFloat(rmtn::SHX.grain.id,rmtn::gRes,resolution);
    getInt(rmtn::SHX.grain.id,rmtn::gMode,&mode);
    expect(resolution[0]==320 && resolution[1]==240 && mode==3,"grain shader uniforms");
    RenderTexture glowTarget=LoadRenderTexture(320,240);
    BeginTextureMode(glowTarget); ClearBackground(BLACK);
    rmtn::scene2D(0,320,240,1);
    EndTextureMode();
    Image glow=LoadImageFromTexture(glowTarget.texture);
    ImageFlipVertical(&glow);
    Color center=GetImageColor(glow,160,190);
    expect(center.r<200 && center.g<200 && center.b<200,"translucent glow stays translucent");
    UnloadImage(glow); UnloadRenderTexture(glowTarget);

    // Every FX tier, through the Comp::render hook, must overwrite prior contents
    // and keep its HUD in the final target.
    rmtn::Registry fxComps; rmtn::registerFxSeries(fxComps);
    expect(fxComps.size()==4,"four FX tiers registered");
    rmtn::Canvas output; output.load(640,360,1);
    for(auto& fx : fxComps){
        fx.w=640; fx.h=360;
        rmtn::presentFrame(fx,100,output);
        Image first=LoadImageFromTexture(output.output().texture);
        BeginTextureMode(output.output()); ClearBackground(MAGENTA); EndTextureMode();
        rmtn::presentFrame(fx,100,output);
        Image second=LoadImageFromTexture(output.output().texture);
        bool same=std::memcmp(first.data,second.data,640*360*4)==0;
        ImageFlipVertical(&second);
        int white=0;
        for(int y=36;y<60;++y) for(int x=36;x<180;++x){
            Color pixel=GetImageColor(second,x,y);
            if(pixel.r>150 && pixel.g>150 && pixel.b>150) ++white;
        }
        std::printf("%s HUD pixels=%d deterministic=%d\n",fx.name.c_str(),white,(int)same);
        expect(same,"FX frame deterministic and fully overwritten");
        expect(white>20,"FX HUD visible");
        UnloadImage(first); UnloadImage(second);
    }
    output.unload();

    // Shutdown hooks release FX GL objects and cached fonts.
    rmtn::shutdownGraphics();
    expect(rmtn::SHX.grain.id==0 && rmtn::RTscene.id==0,"FX resources released");
    CloseWindow();
    if(ok) std::puts("render regressions passed");
    return ok?0:1;
}
