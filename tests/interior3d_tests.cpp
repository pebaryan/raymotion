// Native PBR interior prefab structure and preview renders.
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

struct Stats { int meshes = 0, metallic = 0, transparent = 0, rough = 0; };

static void inspect(const g3d::Object3D& node, Stats& stats) {
    if (node.geometry) {
        ++stats.meshes;
        CHECK(!node.name.empty());
        CHECK(node.material.shading == g3d::Shading::Standard);
        CHECK(node.material.roughness >= 0 && node.material.roughness <= 1);
        CHECK(node.material.metalness >= 0 && node.material.metalness <= 1);
        CHECK(node.material.opacity > 0 && node.material.opacity <= 1);
        if (node.material.metalness >= .7f) ++stats.metallic;
        if (node.material.opacity < 1) ++stats.transparent;
        if (node.material.roughness >= .7f) ++stats.rough;
    }
    for (const auto& child : node.children) inspect(*child, stats);
}

static void structure() {
    std::set<std::string> names;
    int metalParts = 0, softParts = 0, clearParts = 0;
    for (interior3d::Kind kind : interior3d::all()) {
        auto scene = std::make_shared<g3d::Scene>();
        auto* root = interior3d::add(*scene, kind);
        CHECK(root != nullptr && root->name == interior3d::name(kind));
        CHECK(names.insert(root->name).second);
        CHECK(root->children.size() >= 3);
        std::set<std::string> childNames;
        for (const auto& child : root->children)
            CHECK(childNames.insert(child->name).second);
        Stats stats;
        inspect(*root, stats);
        CHECK(stats.meshes >= 3);
        metalParts += stats.metallic;
        softParts += stats.rough;
        clearParts += stats.transparent;
        CHECK(scene->find(root->name) == root);
    }
    CHECK(names.size() == interior3d::all().size());
    CHECK(metalParts >= 30 && softParts >= 30 && clearParts >= 1);

    g3d::Object3D parent;
    auto* a = interior3d::add(parent, interior3d::Kind::HospitalBed);
    auto* b = interior3d::add(parent, interior3d::Kind::HospitalBed);
    a->position.x = 2;
    CHECK(a != b && parent.children.size() == 2 && b->position.x == 0);
}

static Image renderOne(interior3d::Kind kind, float width, float height) {
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
    scene->camera.orthoHeight = std::max({ 1.0f, height * 1.55f, width * .95f });
    scene->camera.position = { 3.2f, 2.4f, 4.7f };
    scene->camera.target = { 0, height * .46f, 0 };

    g3d::Material floor;
    floor.color = { 68, 81, 91, 255 };
    floor.roughness = .92f;
    floor.metalness = .02f;
    auto* base = scene->add(g3d::Geometry::box(std::max(1.0f, width * 1.3f), .035f,
                                               std::max(.9f, width * .7f)), floor);
    base->position.y = -.035f;
    interior3d::add(*scene, kind);

    Comp c;
    c.w = 480; c.h = 400; c.fps = 30; c.duration = 1;
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

static bool preview(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(480, 400, "interior3d prefab preview");
    if (!IsWindowReady()) return false;
    const float widths[] = { 1.1f, .61f, .95f, 1.35f, .95f, .95f,
                             1.15f, .50f, 1.60f, 2.48f, 1.32f, 1.65f,
                             1.70f, .90f, 2.42f, .58f, 2.15f, .67f,
                             .82f, .76f, .78f, 1.0f, .91f, 1.12f,
                             2.20f, 1.60f, .80f, .78f, .72f, .56f,
                             1.0f, .52f, 2.15f, 1.66f, 2.35f, 1.70f };
    const float heights[] = { 1.18f, .80f, 1.98f, .82f, 1.40f, 1.15f,
                              .80f, 1.02f, .80f, 1.98f, 1.88f, 1.94f,
                              .80f, 1.44f, .80f, 1.24f, 1.46f, .44f,
                              2.10f, 1.80f, 1.55f, .96f, 2.03f, 1.82f,
                              1.13f, .81f, 1.82f, .94f, .88f, 1.82f,
                              .79f, 1.02f, 1.25f, 1.98f, 1.17f, 1.75f };
    int i = 0;
    for (interior3d::Kind kind : interior3d::all()) {
        Image image = renderOne(kind, widths[i], heights[i]);
        const auto path = dir / (std::string(interior3d::name(kind)) + ".png");
        CHECK(ExportImage(image, path.string().c_str()));
        UnloadImage(image);
        ++i;
    }
    shutdownGraphics();
    CloseWindow();
    return true;
}

int main(int argc, char** argv) {
    structure();
    if (argc == 3 && std::string(argv[1]) == "--preview") CHECK(preview(argv[2]));
    if (failures) { std::fprintf(stderr, "%d interior3d check(s) failed\n", failures); return 1; }
    std::puts("interior3d tests passed");
    return 0;
}
