// skin_view.cpp -- a body baked by tools/skin_bake.py, worn by a rig::Character and posed by
// clips: front and side views of rest, walk, run and wave, in linear-blend and
// dual-quaternion skinning. Also checks that the bind pose leaves the mesh where it is.
//
//     raymotion_skin_view body.glb out_dir
#include "rmtn/raymotion.hpp"
#include "rmtn/render.hpp"
#include <raymath.h>
#include <cmath>
#include <cstdio>
#include <filesystem>

using namespace rmtn;

static Image frame(Comp& c, int t) {
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, t, canvas);
    Image image = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&image);
    canvas.unload();
    return image;
}

int main(int argc, char** argv) {
    if (argc < 3) { std::printf("usage: raymotion_skin_view body.glb out_dir [pose_matrices.txt]\n"); return 2; }
    try {
    std::filesystem::create_directories(argv[2]);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(640, 640, "skin view");
    if (!IsWindowReady()) return 3;

    const rig::Skin skin = rig::loadSkin(argv[1]);
    float worst = 0;
    for (const Matrix& m : rig::skinMatrices(skin.build, skin.bind, skin.bind)) {
        const Matrix d = MatrixSubtract(m, MatrixIdentity());
        for (const float* f = &d.m0; f < &d.m0 + 16; f++) worst = std::fmax(worst, std::fabs(*f));
    }
    std::printf("[skin] bind round trip: worst %.6f\n", worst);
    if (worst > 1e-4f) return 4;

    struct Shot { const char* name; rig::Clip clip; double at; };
    const Shot shots[] = { { "rest", rig::clips::rest(), 0 }, { "walk", rig::clips::walk(), 0.3 },
                           { "run", rig::clips::run(), 0.2 }, { "wave", rig::clips::wave(), 0.6 },
                           { "sit", rig::clips::sit(0.35f), 1.0 } };
    const float H = skin.build.height();
    // Optional: the skin matrices of each shot's pose (17 x 16 floats, raymath m0..m15) to
    // argv[3], for checking the deformation outside the renderer.
    if (argc > 3) {
        FILE* f = std::fopen(argv[3], "w");
        for (const Shot& s : shots) {
            std::fprintf(f, "%s\n", s.name);
            for (const Matrix& m : rig::skinMatrices(skin.build, skin.bind, s.clip(s.at))) {
                for (const float* v = &m.m0; v < &m.m0 + 16; v++) std::fprintf(f, "%g ", *v);
                std::fprintf(f, "\n");
            }
        }
        std::fclose(f);
    }
    for (const Shot& s : shots)
        for (int view = 0; view < 2; view++)
            for (int dq = 0; dq < 2; dq++) {
                Comp c;
                c.w = 640; c.h = 640; c.fps = 30; c.duration = 1;
                c.background = { 228, 230, 234, 255 };
                auto scene = std::make_shared<g3d::Scene>();
                const float d = H * 1.9f;
                scene->camera.position = view == 0 ? Vector3{ 0, H * 0.55f, d } : Vector3{ d, H * 0.55f, 0 };
                scene->camera.target = { 0, H * 0.5f, 0 };
                scene->camera.fov = 34;
                scene->ambient = 0.55f;
                scene->lights.push_back(g3d::Light::directional({ -2, 5, 4 }, { 255, 246, 232, 255 }, 1.5f, true));
                scene->lights.push_back(g3d::Light::directional({ 3, 2, -3 }, { 180, 200, 255, 255 }, 0.5f));
                g3d::Material floor;
                floor.color = { 200, 204, 210, 255 };
                scene->add(g3d::Geometry::plane(6, 6), floor);
                auto ch = std::make_shared<rig::Character>([&] { rig::Look l; l.build = skin.build; return l; }());
                ch->motion.play(s.clip, -s.at);
                skin.model->dualQuaternion = dq == 1;
                g3d::Object3D* body = rig::add(*scene, ch, skin, 30);
                body->children.front()->material.doubleSided = true;
                addRoot(c.roots, 0, 1)->fn = g3d::draw(scene);
                Image im = frame(c, 0);
                ExportImage(im, TextFormat("%s/%s_%s_%s.png", argv[2], s.name, view ? "side" : "front", dq ? "dq" : "lbs"));
                UnloadImage(im);
            }
    std::printf("[skin] wrote %d views to %s\n", (int)(sizeof shots / sizeof shots[0]) * 4, argv[2]);
    } catch (const std::exception& e) {
        FILE* f = std::fopen("skin_view_err.txt", "w");
        if (f) { std::fprintf(f, "EXCEPTION: %s\n", e.what()); std::fclose(f); }
        std::printf("[skin] EXCEPTION: %s\n", e.what());
        return 9;
    }
    return 0;
    CloseWindow();
    return 0;
}
