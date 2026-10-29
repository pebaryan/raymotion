// Smart-grid CityJSON prefab PBR look and optional raymotion still renders.
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static const char* ids[] = {
    "residential-detached", "residential-terrace", "residential-apartment",
    "commercial-office", "commercial-office-tower", "city-hall",
    "exhibition-event-hall", "commercial-shop", "industrial-factory-pv",
    "industrial-warehouse", "industrial-assembly-factory", "data-center",
    "power-station-turbine-hall", "power-station-control-building",
    "wind-turbine", "diesel-generator", "gas-turbine-generator", "hv-transformer",
    "ev-charging-pole", "ev-charging-hub-five-bay",
    "e-car", "distribution-substation", "lv-distribution-cabinet",
    "battery-storage", "battery-substation", "solar-carport",
    "grid-pole", "road-straight", "foliage-deciduous-tree",
    "foliage-conifer-tree", "foliage-street-tree", "foliage-orchard-tree",
    "foliage-shrub-cluster", "foliage-hedge-segment", "foliage-grass-patch",
    "foliage-flower-bed", "foliage-planter-tree",
    "street-traffic-light", "street-light", "solar-street-light",
    "traffic-sign-stop", "traffic-sign-warning", "traffic-sign-direction",
    "street-pedestrian-crossing", "bus-stop-shelter", "park-bench",
    "public-waste-bin", "street-bollard", "bicycle-rack",
};

static city::Model load(const char* id) {
    return city::load(std::string("data/city/smartgrid/") + id + ".city.json");
}

static void materials() {
    const city::Look look = smartgrid3d::cityLook();
    CHECK(bool(look.color) && bool(look.material));
    for (const char* id : ids) {
        const city::Model model = load(id);
        auto scene = std::make_shared<g3d::Scene>();
        auto* group = city::add(*scene, model, look);
        CHECK(group != nullptr && !group->children.empty());
        for (const auto& child : group->children) {
            CHECK(child->material.shading == g3d::Shading::Standard);
            CHECK(child->material.roughness >= 0 && child->material.roughness <= 1);
            CHECK(child->material.metalness >= 0 && child->material.metalness <= 1);
            CHECK(child->material.opacity > 0 && child->material.opacity <= 1);
        }
    }
    auto scene = std::make_shared<g3d::Scene>();
    city::add(*scene, load("industrial-factory-pv"), look);
    CHECK(scene->find("factory-pv") != nullptr);
    CHECK(scene->find("factory-pv")->material.metalness > .3f);
    scene = std::make_shared<g3d::Scene>();
    city::add(*scene, load("commercial-office"), look);
    CHECK(scene->find("office-windows") != nullptr);
    CHECK(scene->find("office-windows")->material.opacity < .5f);
    scene = std::make_shared<g3d::Scene>();
    city::add(*scene, load("distribution-substation"), look);
    CHECK(scene->find("transformer")->material.metalness > .5f);
    scene = std::make_shared<g3d::Scene>();
    city::add(*scene, load("road-straight"), look);
    CHECK(scene->find("road")->material.roughness > .9f);
    scene = std::make_shared<g3d::Scene>();
    city::add(*scene, load("hv-transformer"), look);
    CHECK(scene->find("hv-tank")->material.metalness > .6f);
    CHECK(scene->find("hv-bushing-1")->material.roughness < .3f);
    scene = std::make_shared<g3d::Scene>();
    city::add(*scene, load("wind-turbine"), look);
    CHECK(scene->find("rotor-blade-1")->material.metalness < .2f);
    scene = std::make_shared<g3d::Scene>();
    city::add(*scene, load("foliage-deciduous-tree"), look);
    CHECK(scene->find("main-crown")->material.roughness > .8f);
    CHECK(scene->find("tree-trunk")->material.roughness > .9f);
    scene = std::make_shared<g3d::Scene>();
    city::add(*scene, load("street-traffic-light"), look);
    CHECK(scene->find("signal-red")->material.emissive.r > 0);
    scene = std::make_shared<g3d::Scene>();
    city::add(*scene, load("solar-street-light"), look);
    CHECK(scene->find("street-pv-panel")->material.metalness > .3f);
    scene = std::make_shared<g3d::Scene>();
    city::add(*scene, load("bus-stop-shelter"), look);
    CHECK(scene->find("shelter-back")->material.opacity < .5f);
}

static Image renderOne(const char* id) {
    const city::Model model = load(id);
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
    city::frame(scene->camera, model, 210, 31, .13f, 480.f / 400.f);
    scene->camera.orthographic = true;
    const BoundingBox bounds = model.bounds();
    const float w = bounds.max.x - bounds.min.x, d = bounds.max.y - bounds.min.y;
    g3d::Material floor;
    floor.color = { 68, 81, 91, 255 };
    floor.roughness = .92f;
    floor.metalness = .02f;
    if (std::string(id) != "wind-turbine" && std::string(id).rfind("foliage-", 0) != 0) {
        auto* base = scene->add(g3d::Geometry::box(std::max(2.f, w * 1.17f), .035f,
                                                   std::max(2.f, d * 1.17f)), floor);
        base->position = { (bounds.min.x + bounds.max.x) / 2, -.035f,
                           -(bounds.min.y + bounds.max.y) / 2 };
    }
    city::add(*scene, model, smartgrid3d::cityLook());

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

static bool preview(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(480, 400, "smart-grid CityJSON PBR preview");
    if (!IsWindowReady()) return false;
    for (const char* id : ids) {
        Image img = renderOne(id);
        const auto path = dir / (std::string(id) + ".png");
        CHECK(ExportImage(img, path.string().c_str()));
        UnloadImage(img);
    }
    shutdownGraphics();
    CloseWindow();
    return true;
}

int main(int argc, char** argv) {
    materials();
    if (argc == 3 && std::string(argv[1]) == "--preview") CHECK(preview(argv[2]));
    if (failures) { std::fprintf(stderr, "%d smartgrid3d check(s) failed\n", failures); return 1; }
    std::puts("smartgrid3d tests passed");
    return 0;
}
