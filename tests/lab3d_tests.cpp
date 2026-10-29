// Native laboratory PBR object builders: scene graph, materials and optional still renders.
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

struct Stats { int meshes = 0, metal = 0, transparent = 0; };

static void inspect(const g3d::Object3D& o, Stats& s) {
    if (o.geometry) {
        ++s.meshes;
        CHECK(!o.name.empty());
        CHECK(o.material.shading == g3d::Shading::Standard);
        CHECK(o.material.roughness >= 0 && o.material.roughness <= 1);
        CHECK(o.material.metalness >= 0 && o.material.metalness <= 1);
        CHECK(o.material.opacity > 0 && o.material.opacity <= 1);
        if (o.material.metalness >= .7f) ++s.metal;
        if (o.material.opacity < 1) ++s.transparent;
    }
    for (const auto& child : o.children) inspect(*child, s);
}

static void structure() {
    std::set<std::string> names;
    int metalParts = 0, transparentParts = 0;
    for (lab3d::Kind kind : lab3d::all()) {
        auto scene = std::make_shared<g3d::Scene>();
        auto* root = lab3d::add(*scene, kind);
        CHECK(root != nullptr && root->name == lab3d::name(kind));
        CHECK(names.insert(root->name).second);
        CHECK(!root->children.empty());
        std::set<std::string> childNames;
        for (const auto& child : root->children)
            CHECK(childNames.insert(child->name).second);
        Stats stats;
        inspect(*root, stats);
        CHECK(stats.meshes >= 3);
        metalParts += stats.metal;
        transparentParts += stats.transparent;
        if (kind == lab3d::Kind::FumeHood || kind == lab3d::Kind::RotaryEvaporator ||
            kind == lab3d::Kind::AnalyticalBalance || kind == lab3d::Kind::GlasswareRack)
            CHECK(stats.transparent >= 1);
        CHECK(scene->find(root->name) == root);
    }
    CHECK(names.size() == lab3d::all().size());
    CHECK(metalParts >= 12 && transparentParts >= 8);

    g3d::Object3D parent;
    auto* first = lab3d::add(parent, lab3d::Kind::FumeHood);
    auto* second = lab3d::add(parent, lab3d::Kind::FumeHood);
    CHECK(first != second && parent.children.size() == 2);
    first->position = { 1, 0, 2 };
    CHECK(second->position.x == 0 && second->position.z == 0);
}

static Image renderOne(lab3d::Kind kind, float width, float height, bool cityModel) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->background = { 20, 31, 43, 255 };
    scene->skyColor = { 206, 221, 232, 255 };
    scene->groundColor = { 50, 60, 69, 255 };
    scene->ambient = .62f;
    scene->environmentGenerator = g3d::studioEnvironment(
        { 225, 235, 242, 255 }, { 150, 179, 196, 255 }, { 45, 57, 66, 255 }, 3);
    scene->environmentIntensity = .8f;
    scene->lights.push_back(g3d::Light::directional({ -3, 6, 5 }, { 255, 244, 226, 255 }, 1.5f, true));
    scene->lights.push_back(g3d::Light::directional({ 3, 4, -3 }, { 158, 206, 245, 255 }, .65f));
    scene->camera.orthographic = true;
    scene->camera.orthoHeight = std::max({ 1.15f, height * 1.43f, width * .95f });
    scene->camera.position = { 3.2f, 2.4f, 4.7f };
    scene->camera.target = { 0, height * .46f, 0 };

    g3d::Material floor;
    floor.color = { 68, 81, 91, 255 };
    floor.roughness = .92f;
    floor.metalness = .02f;
    auto* base = scene->add(g3d::Geometry::box(std::max(1.1f, width * 1.3f), .035f,
                                               std::max(.9f, width * .6f)), floor);
    base->position.y = -.035f;
    if (cityModel) {
        const auto model = city::load(std::string("data/city/chemistry-lab/") +
                                      lab3d::name(kind) + ".city.json");
        city::add(*scene, model, lab3d::cityLook());
    } else {
        lab3d::add(*scene, kind);
    }

    Comp c;
    c.w = 480; c.h = 400; c.fps = 30; c.duration = 1;
    c.background = { 20, 31, 43, 255 };
    addRoot(c.roots, 0, 1)->fn = g3d::draw(scene);
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, 0, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}

static bool preview(const std::filesystem::path& dir, bool cityModel) {
    std::filesystem::create_directories(dir);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(480, 400, "lab3d prefab preview");
    if (!IsWindowReady()) return false;
    const float widths[] = { 2.8f, 2.0f, 1.3f, .64f, .52f, 1.3f,
                             .86f, .86f, .96f, 1.0f, 1.6f, .84f };
    const float heights[] = { 1.25f, 2.84f, 1.95f, .52f, .42f, 1.48f,
                              1.61f, .48f, 1.12f, 1.98f, 1.55f, .66f };
    int i = 0;
    for (lab3d::Kind kind : lab3d::all()) {
        Image img = renderOne(kind, widths[i], heights[i], cityModel);
        const auto path = dir / (std::string(lab3d::name(kind)) + ".png");
        CHECK(ExportImage(img, path.string().c_str()));
        UnloadImage(img);
        ++i;
    }
    shutdownGraphics();
    CloseWindow();
    return true;
}

int main(int argc, char** argv) {
    structure();
    if (argc == 3 && (std::string(argv[1]) == "--preview" ||
                      std::string(argv[1]) == "--preview-city")) {
        CHECK(preview(argv[2], std::string(argv[1]) == "--preview-city"));
    }
    if (failures) { std::fprintf(stderr, "%d lab3d check(s) failed\n", failures); return 1; }
    std::puts("lab3d tests passed");
    return 0;
}
