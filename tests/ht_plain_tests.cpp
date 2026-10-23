#include "../examples/showcase/ht_plain.cpp"
#include <cstdio>
#include "rmtn/render.hpp"
int main(){
    // These layouts are designed for Segoe UI; skip (ctest SKIP_RETURN_CODE) where it is absent.
    if(rmtn::fonts::find("segoeui.ttf").empty() || rmtn::fonts::find("segoeuib.ttf").empty()){
        std::puts("SKIP: Segoe UI not installed"); return 77;
    }
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(640,360,"plain explainer test");
    auto lines=rmtn::wrapPlain("A guess is not the same thing as a conclusion.",32,260);
    bool wrap=lines.size()>1;
    bool font=rmtn::plainFont().texture.id!=GetFontDefault().texture.id;
    bool fits=true;
    for(const auto& line:lines) fits=fits && MeasureTextEx(rmtn::plainFont(),line.c_str(),32,0).x<=260;
    std::vector<rmtn::Comp> comps; rmtn::register_ht_plain(comps);
    bool scene=comps.size()==1;
    if(scene){
        RenderTexture canvas=LoadRenderTexture(1280,720);
        for(int frame=0;frame<comps[0].duration;++frame){
            BeginTextureMode(canvas);
            comps[0].roots[0]->fn(rmtn::Ctx{},frame);
            EndTextureMode();
        }
        fits=fits && rmtn::plainLayoutErrors==0;
        std::printf("all-frame layout overflow count=%d\n",rmtn::plainLayoutErrors);
        UnloadRenderTexture(canvas);
    }
    rmtn::shutdownGraphics();
    CloseWindow();
    std::printf("wrap=%d loadedTrueType=%d fits=%d sceneRegistered=%d\n",wrap,font,fits,scene);
    return wrap && font && fits && scene ? 0:1;
}
