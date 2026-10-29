// Native PBR airplane prefab structure and preview.
#include "rmtn/raymotion.hpp"
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>

using namespace rmtn;
static int failures=0;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); ++failures; } } while(0)

static void structure() {
    g3d::Scene scene;
    auto* jet=aircraft3d::addPassengerJet(scene);
    CHECK(jet && jet->name=="passenger-jet");
    CHECK(scene.find("passenger-jet")==jet);
    CHECK(jet->children.size()>=60);
    int transparent=0, metallic=0;
    std::set<std::string> names;
    for(const auto& p:jet->children) {
        CHECK(p->geometry!=nullptr);
        CHECK(names.insert(p->name).second);
        CHECK(p->material.shading==g3d::Shading::Standard);
        CHECK(p->material.roughness>=0 && p->material.roughness<=1);
        CHECK(p->material.metalness>=0 && p->material.metalness<=1);
        if(p->material.opacity<1) ++transparent;
        if(p->material.metalness>.7f) ++metallic;
    }
    CHECK(names.count("fuselage-shell") && names.count("left-main-wing") &&
          names.count("right-main-wing") && names.count("vertical-tail"));
    CHECK(names.count("left-engine-nacelle") && names.count("right-engine-nacelle"));
    CHECK(names.count("nose-wheel") == 0); // wheel is a named tire/hub pair
    CHECK(names.count("nose-wheel-tire") && names.count("left-main-wheel-tire"));
    CHECK(metallic>=7);
    CHECK(transparent>=2);
    g3d::Object3D parent;
    auto* a=aircraft3d::addPassengerJet(parent);
    auto* b=aircraft3d::addPassengerJet(parent);
    CHECK(a!=b && parent.children.size()==2);
    b->position.x=12;
    CHECK(a->position.x==0);
}

static Image render(Vector3 cameraPos,float orthoHeight) {
    auto scene=std::make_shared<g3d::Scene>();
    scene->background={19,32,45,255}; scene->skyColor={198,218,231,255};
    scene->groundColor={56,68,76,255}; scene->ambient=.63f;
    scene->environmentGenerator=g3d::studioEnvironment(
        {231,238,241,255},{156,185,204,255},{48,60,70,255},3);
    scene->environmentIntensity=.8f;
    scene->lights.push_back(g3d::Light::directional({-20,28,18},{255,246,227,255},1.55f,true));
    scene->lights.push_back(g3d::Light::directional({16,15,-20},{166,210,239,255},.75f));
    scene->camera.orthographic=true; scene->camera.orthoHeight=orthoHeight;
    scene->camera.position=cameraPos; scene->camera.target={0,3.1f,0};
    aircraft3d::addPassengerJet(*scene);
    Comp c; c.w=960; c.h=720; c.fps=30; c.duration=1; c.background=scene->background;
    addRoot(c.roots,0,1)->fn=g3d::draw(scene);
    Canvas canvas; canvas.load(c.w,c.h,1);
    presentFrame(c,0,canvas);
    Image image=LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&image);
    canvas.unload();
    return image;
}
static void preview(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);
    SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(960,720,"aircraft3d preview");
    CHECK(IsWindowReady());
    if(IsWindowReady()) {
        auto three=render({32,22,40},43);
        CHECK(ExportImage(three,(dir/"passenger-jet-three-quarter.png").string().c_str()));
        UnloadImage(three);
        auto top=render({0,65,0.01f},43);
        CHECK(ExportImage(top,(dir/"passenger-jet-top.png").string().c_str()));
        UnloadImage(top);
    }
    shutdownGraphics(); CloseWindow();
}
int main(int argc,char** argv) {
    structure();
    if(argc==3 && std::string(argv[1])=="--preview") preview(argv[2]);
    if(failures) { std::fprintf(stderr,"%d aircraft3d checks failed\n",failures); return 1; }
    std::puts("aircraft3d tests passed"); return 0;
}
