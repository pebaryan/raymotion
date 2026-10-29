// Native PBR computer parts: scene structure and raymotion preview renders.
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

struct Stats { int meshes = 0, metallic = 0, transparent = 0; };

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
    }
    for (const auto& child : node.children) inspect(*child, stats);
}

static void structure() {
    std::set<std::string> names;
    int metalParts = 0, clearParts = 0;
    for (computer3d::Kind kind : computer3d::all()) {
        auto scene = std::make_shared<g3d::Scene>();
        auto* root = computer3d::add(*scene, kind);
        CHECK(root != nullptr && root->name == computer3d::name(kind));
        CHECK(names.insert(root->name).second);
        std::set<std::string> partNames;
        for (const auto& part : root->children)
            CHECK(partNames.insert(part->name).second);
        Stats stats;
        inspect(*root, stats);
        CHECK(stats.meshes >= 4);
        metalParts += stats.metallic;
        clearParts += stats.transparent;
        CHECK(scene->find(root->name) == root);
    }
    CHECK(names.size() == computer3d::all().size());
    CHECK(metalParts >= 20 && clearParts >= 1);
    g3d::Object3D parent;
    auto* a = computer3d::add(parent, computer3d::Kind::GraphicsCard);
    auto* b = computer3d::add(parent, computer3d::Kind::GraphicsCard);
    a->position.x = 1;
    CHECK(a != b && b->position.x == 0 && parent.children.size() == 2);
}

static Image renderOne(computer3d::Kind kind, float width, float height) {
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
    scene->camera.orthoHeight = std::max({ .055f, height * 1.65f, width * 1.15f });
    scene->camera.position = { width * 1.4f, std::max(height, width) * 1.1f,
                               width * 1.85f };
    scene->camera.target = { 0, height * .48f, 0 };

    g3d::Material floor;
    floor.color = { 68, 81, 91, 255 };
    floor.roughness = .92f;
    floor.metalness = .02f;
    floor.shading = g3d::Shading::Standard;
    auto* base = scene->add(g3d::Geometry::box(std::max(.075f, width * 1.35f),
                                               std::max(.001f, height * .012f),
                                               std::max(.055f, width * .8f)), floor);
    base->position.y = -std::max(.001f, height * .012f);
    computer3d::add(*scene, kind);

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
    InitWindow(480, 400, "computer3d prefab preview");
    if (!IsWindowReady()) return false;
    const float widths[] = { .045f, .135f, .310f, .165f, .235f, .130f,
                             .310f, .083f, .105f, .150f, .125f, .260f };
    const float heights[] = { .009f, .043f, .135f, .090f, .460f, .140f,
                              .245f, .007f, .010f, .031f, .125f, .130f };
    int i = 0;
    for (computer3d::Kind kind : computer3d::all()) {
        Image image = renderOne(kind, widths[i], heights[i]);
        auto path = dir / (std::string(computer3d::name(kind)) + ".png");
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
    if (failures) { std::fprintf(stderr, "%d computer3d check(s) failed\n", failures); return 1; }
    std::puts("computer3d tests passed");
    return 0;
}
