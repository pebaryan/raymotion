#include "../src/fx_present.cpp"
#include "../src/rmtn/preview.hpp"
#include <cstdio>

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
    InitWindow(320, 240, "raymotion regression");
    rmtn::Comp c; c.w=320; c.h=240; c.duration=10;
    int calls=0;
    auto* n=rmtn::addRoot(c.roots,0,10);
    n->fn=[&](const rmtn::Ctx&,int){ ++calls; ClearBackground(RED); };
    BeginDrawing(); rmtn::presentFrame(c,0); EndDrawing();
    // Read the actual GL uniform state, not merely whether the shader compiled.
    rmtn::fxEnsure(320,240);
    rmtn::grainOverlay(0.25f,320,240,0.35f,3);
    rlDrawRenderBatchActive();
    auto getFloat=(void (*)(unsigned int,int,float*))rlGetProcAddress("glGetUniformfv");
    auto getInt=(void (*)(unsigned int,int,int*))rlGetProcAddress("glGetUniformiv");
    float resolution[2]={}; int mode=0;
    getFloat(rmtn::SHX.grain.id,rmtn::gRes,resolution);
    getInt(rmtn::SHX.grain.id,rmtn::gMode,&mode);
    bool uniforms=resolution[0]==320 && resolution[1]==240 && mode==3;
    RenderTexture glowTarget=LoadRenderTexture(320,240);
    BeginTextureMode(glowTarget); ClearBackground(BLACK);
    rmtn::scene2D(0,320,240,1);
    EndTextureMode();
    Image glow=LoadImageFromTexture(glowTarget.texture);
    ImageFlipVertical(&glow);
    Color center=GetImageColor(glow,160,190);
    bool alpha=center.r<200 && center.g<200 && center.b<200;
    UnloadImage(glow); UnloadRenderTexture(glowTarget);
    // Every tier must overwrite prior contents and keep its HUD in the final target.
    RenderTexture output=LoadRenderTexture(640,360);
    rmtn::Comp fx; fx.w=640; fx.h=360; fx.duration=300;
    bool deterministic=true, hud=true;
    for(int tier=1;tier<=4;++tier){
        fx.fx=tier;
        BeginDrawing(); rmtn::presentFrame(fx,100,output); EndDrawing();
        Image first=LoadImageFromTexture(output.texture);
        BeginTextureMode(output); ClearBackground(MAGENTA); EndTextureMode();
        BeginDrawing(); rmtn::presentFrame(fx,100,output); EndDrawing();
        Image second=LoadImageFromTexture(output.texture);
        deterministic=deterministic && std::memcmp(first.data,second.data,640*360*4)==0;
        ImageFlipVertical(&second);
        int white=0;
        for(int y=36;y<60;++y) for(int x=36;x<180;++x){
            Color pixel=GetImageColor(second,x,y);
            if(pixel.r>150 && pixel.g>150 && pixel.b>150) ++white;
        }
        hud=hud && white>20;
        std::printf("tier %d HUD pixels=%d deterministic=%d\n",tier,white,(int)deterministic);
        UnloadImage(first); UnloadImage(second);
    }
    UnloadRenderTexture(output);
    rmtn::releaseFrameResources();
    CloseWindow();
    if(!deterministic || !hud){ std::fprintf(stderr,"FAIL: frame clearing/HUD\n"); return 1; }
    if(!alpha){ std::fprintf(stderr,"FAIL: translucent glow became opaque: %d,%d,%d\n",center.r,center.g,center.b); return 1; }
    if(!uniforms){ std::fprintf(stderr,"FAIL: shader resolution=%g,%g mode=%d\n",resolution[0],resolution[1],mode); return 1; }
    if(calls!=1){ std::fprintf(stderr,"FAIL: legacy timeline callback not executed\n"); return 1; }
    std::puts("legacy dispatch passed");
    return 0;
}
