// Native city-scale human role prefabs and raymotion preview renders.
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

struct Stats { int meshes = 0, fabric = 0, metal = 0; };

static void inspect(const g3d::Object3D& node, Stats& s, std::set<std::string>& names) {
    CHECK(!node.name.empty());
    CHECK(names.insert(node.name).second);
    if (node.geometry) {
        ++s.meshes;
        CHECK(node.material.shading == g3d::Shading::Standard);
        CHECK(node.material.roughness >= 0 && node.material.roughness <= 1);
        CHECK(node.material.metalness >= 0 && node.material.metalness <= 1);
        CHECK(node.material.opacity > 0 && node.material.opacity <= 1);
        if (node.material.roughness > .8f) ++s.fabric;
        if (node.material.metalness > .6f) ++s.metal;
    }
    for (const auto& child : node.children) inspect(*child, s, names);
}

static void structure() {
    std::set<std::string> roles;
    int fabricParts = 0, metalParts = 0;
    for (human3d::Role role : human3d::all()) {
        auto scene = std::make_shared<g3d::Scene>();
        auto* root = human3d::add(*scene, role);
        CHECK(root != nullptr && root->name == human3d::name(role));
        CHECK(roles.insert(root->name).second);
        CHECK(root->scale.y >= .80f && root->scale.y <= 1.1f);
        Stats s;
        std::set<std::string> names;
        inspect(*root, s, names);
        CHECK(s.meshes >= 20);
        CHECK(scene->find("head") != nullptr);
        CHECK(scene->find("left-arm") != nullptr);
        CHECK(scene->find("right-arm") != nullptr);
        fabricParts += s.fabric;
        metalParts += s.metal;
    }
    CHECK(roles.size() == human3d::all().size());
    CHECK(fabricParts > 100 && metalParts >= 5);
    g3d::Object3D parent;
    auto* a = human3d::add(parent, human3d::Role::Resident);
    auto* b = human3d::add(parent, human3d::Role::Resident);
    a->position.x = 2;
    CHECK(a != b && b->position.x == 0 && parent.children.size() == 2);
}

static std::shared_ptr<g3d::Scene> studio() {
    auto scene = std::make_shared<g3d::Scene>();
    scene->background = { 20, 31, 43, 255 };
    scene->skyColor = { 206, 221, 232, 255 };
    scene->groundColor = { 50, 60, 69, 255 };
    scene->ambient = .62f;
    scene->environmentGenerator = g3d::studioEnvironment(
        { 225, 235, 242, 255 }, { 150, 179, 196, 255 }, { 45, 57, 66, 255 }, 3);
    scene->environmentIntensity = .8f;
    scene->lights.push_back(g3d::Light::directional(
        { -3, 6, 5 }, { 255, 244, 226, 255 }, 1.5f, true));
    scene->lights.push_back(g3d::Light::directional(
        { 3, 4, -3 }, { 158, 206, 245, 255 }, .65f));
    scene->camera.orthographic = true;
    return scene;
}

static Image frame(std::shared_ptr<g3d::Scene> scene, int width, int height) {
    Comp c;
    c.w = width; c.h = height; c.fps = 30; c.duration = 1;
    c.background = { 20, 31, 43, 255 };
    addRoot(c.roots, 0, 1)->fn = g3d::draw(scene);
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, 0, canvas);
    Image image = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&image);
    canvas.unload();
    return image;
}

static void floor(g3d::Scene& scene, float width, float depth) {
    g3d::Material m;
    m.color = { 68, 81, 91, 255 };
    m.roughness = .92f;
    m.metalness = .02f;
    auto* base = scene.add(g3d::Geometry::box(width, .035f, depth), m);
    base->position.y = -.035f;
}

static bool preview(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(900, 500, "human3d prefab preview");
    if (!IsWindowReady()) return false;
    for (human3d::Role role : human3d::all()) {
        auto scene = studio();
        scene->camera.orthoHeight = 2.45f;
        scene->camera.position = { 2.5f, 1.9f, 4.8f };
        scene->camera.target = { 0, .91f, 0 };
        floor(*scene, 1.15f, .85f);
        human3d::add(*scene, role);
        Image image = frame(scene, 480, 400);
        auto path = dir / (std::string(human3d::name(role)) + ".png");
        CHECK(ExportImage(image, path.string().c_str()));
        UnloadImage(image);
    }

    auto scene = studio();
    scene->camera.orthoHeight = 5.8f;
    scene->camera.position = { 8.0f, 5.4f, 10.5f };
    scene->camera.target = { .3f, 1.25f, 0 };
    floor(*scene, 12.0f, 6.0f);
    const auto look = smartgrid3d::cityLook();
    auto* car = city::add(*scene, city::load("data/city/smartgrid/e-car.city.json"), look);
    car->position = { -.5f, 0, 0 };
    auto* pole = city::add(*scene, city::load("data/city/smartgrid/ev-charging-pole.city.json"), look);
    pole->position = { 3.0f, 0, -.5f };
    human3d::add(*scene, human3d::Role::EvDriver)->position = { -3.25f, 0, 1.0f };
    human3d::add(*scene, human3d::Role::ChargingTechnician)->position = { 4.45f, 0, .5f };
    Image context = frame(scene, 900, 500);
    CHECK(ExportImage(context, (dir / "city-scale-context.png").string().c_str()));
    UnloadImage(context);
    shutdownGraphics();
    CloseWindow();
    return true;
}

int main(int argc, char** argv) {
    structure();
    if (argc == 3 && std::string(argv[1]) == "--preview") CHECK(preview(argv[2]));
    if (failures) { std::fprintf(stderr, "%d human3d check(s) failed\n", failures); return 1; }
    std::puts("human3d tests passed");
    return 0;
}
