// skin_anim.cpp -- a skinned body (tools/skin_bake.py + rig::loadSkin) animated by a rig:: clip,
// rendered to a PNG frame sequence for side-by-side comparison videos.
//
//     raymotion_skin_anim body.glb out_dir walk|run|wave|rest [frames] [camera]
// frames default 24 (one clip cycle, t sampled in [0,1)); camera: front|side|three
#include "rmtn/raymotion.hpp"
#include "rmtn/render.hpp"
#include <raymath.h>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

using namespace rmtn;

static Image frame(Comp& c, double t) {
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, float(t), canvas);
    Image image = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&image);
    canvas.unload();
    return image;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::printf("usage: raymotion_skin_anim body.glb out_dir walk|run|wave|rest [frames] [camera]\n");
        return 2;
    }
    try {
    std::filesystem::create_directories(argv[2]);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(720, 720, "skin anim");
    if (!IsWindowReady()) return 3;

    const std::string clipName = argv[3];
    const int frames = argc > 4 ? std::atoi(argv[4]) : 24;
    const std::string cam = argc > 5 ? argv[5] : "front";
    rig::Clip clip;
    if (clipName == "walk") clip = rig::clips::walk();
    else if (clipName == "run") clip = rig::clips::run();
    else if (clipName == "wave") clip = rig::clips::wave();
    else clip = rig::clips::rest();

    const rig::Skin skin = rig::loadSkin(argv[1]);
    const float H = skin.build.height();

    Comp c;
    c.w = 720; c.h = 720; c.fps = 30; c.duration = 1;
    c.background = { 235, 236, 240, 255 };
    auto scene = std::make_shared<g3d::Scene>();
    const float d = H * 1.55f;
    if (cam == "side") scene->camera.position = { d, H * 0.55f, 0 };
    else if (cam == "three") scene->camera.position = { d * 0.7f, H * 0.6f, d * 0.7f };
    else scene->camera.position = { 0, H * 0.55f, d };
    scene->camera.target = { 0, H * 0.5f, 0 };
    scene->camera.fov = 38;
    scene->ambient = 0.55f;
    scene->lights.push_back(g3d::Light::directional({ -2, 5, 4 }, { 255, 246, 232, 255 }, 1.5f, true));
    scene->lights.push_back(g3d::Light::directional({ 3, 2, -3 }, { 180, 200, 255, 255 }, 0.5f));
    g3d::Material floor;
    floor.color = { 202, 205, 212, 255 };
    scene->add(g3d::Geometry::plane(6, 6), floor);
    auto ch = std::make_shared<rig::Character>([&] { rig::Look l; l.build = skin.build; return l; }());
    ch->motion.play(clip, 0);
    skin.model->dualQuaternion = true;
    g3d::Object3D* body = rig::add(*scene, ch, skin, 30);
    body->children.front()->material.doubleSided = true;
    addRoot(c.roots, 0, 1)->fn = g3d::draw(scene);

    for (int i = 0; i < frames; i++) {
        const double t = double(i) / double(frames) * c.duration;
        Image im = frame(c, t);
        ExportImage(im, TextFormat("%s/anim_%03d.png", argv[2], i));
        UnloadImage(im);
    }
    std::printf("[anim] wrote %d %s frames to %s\n", frames, clipName.c_str(), argv[2]);
    } catch (const std::exception& e) {
        FILE* f = std::fopen("skin_anim_err.txt", "w");
        if (f) { std::fprintf(f, "EXCEPTION: %s\n", e.what()); std::fclose(f); }
        std::printf("[anim] EXCEPTION: %s\n", e.what());
        return 9;
    }
    CloseWindow();
    return 0;
}
