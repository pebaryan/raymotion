// flora.cpp -- vegetation: grass looks and helpers over g3d::Field.
#include "rmtn/flora.hpp"
#include <algorithm>
#include <stdexcept>

namespace rmtn {
namespace flora {

g3d::Field lawn() {
    g3d::Field f;
    f.density = 1200;
    f.height = 0.07f; f.heightVariation = 0.2f; f.width = 0.007f; f.lean = 0.12f; f.segments = 2;
    f.root = { 34, 70, 26, 255 }; f.tip = { 92, 150, 54, 255 };
    f.colorVariation = 0.2f; f.dryness = 0.03f;
    f.sway = 0.04f; f.gust = 0.08f;
    f.detailDistance = 8; f.fadeStart = 15; f.fadeEnd = 35; f.shadowDistance = 10;
    return f;
}

g3d::Field meadow() {
    g3d::Field f;                       // the Field defaults are a meadow
    f.density = 500;
    return f;
}

g3d::Field tallGrass() {
    g3d::Field f;
    f.density = 260;
    f.height = 0.8f; f.heightVariation = 0.3f; f.width = 0.014f; f.lean = 0.35f; f.segments = 5;
    f.root = { 44, 68, 26, 255 }; f.tip = { 150, 170, 72, 255 };
    f.colorVariation = 0.3f; f.dryness = 0.2f;
    f.sway = 0.18f; f.gust = 0.5f;
    f.detailDistance = 18; f.fadeStart = 40; f.fadeEnd = 80; f.shadowDistance = 22;
    return f;
}

g3d::Field dryGrass() {
    g3d::Field f;
    f.density = 360;
    f.height = 0.45f; f.heightVariation = 0.4f; f.width = 0.01f; f.lean = 0.3f;
    f.root = { 96, 88, 46, 255 }; f.tip = { 210, 188, 118, 255 }; f.dry = { 228, 208, 150, 255 };
    f.colorVariation = 0.35f; f.dryness = 0.5f;
    f.sway = 0.14f; f.gust = 0.4f;
    return f;
}

g3d::Field grass(const std::string& name) {
    if (name == "lawn") return lawn();
    if (name == "meadow") return meadow();
    if (name == "tall") return tallGrass();
    if (name == "dry") return dryGrass();
    throw std::invalid_argument("flora: no grass look \"" + name + "\" (lawn, meadow, tall, dry)");
}

g3d::Material grassMaterial() {
    g3d::Material m;
    m.color = WHITE;
    m.roughness = 0.85f;
    m.translucency = 0.7f;
    m.translucencyColor = { 255, 240, 180, 255 };
    m.doubleSided = true;
    m.lineWidth = 0;
    return m;
}

namespace {
std::shared_ptr<g3d::Field> place(Vector2 min, Vector2 max, g3d::Field style, uint32_t seed) {
    style.min = { std::min(min.x, max.x), std::min(min.y, max.y) };
    style.max = { std::max(min.x, max.x), std::max(min.y, max.y) };
    style.seed = seed;
    return std::make_shared<g3d::Field>(std::move(style));
}
}  // namespace

g3d::Object3D* addGrass(g3d::Scene& scene, Vector2 min, Vector2 max, g3d::Field style, uint32_t seed) {
    return scene.add(place(min, max, std::move(style), seed), grassMaterial());
}
g3d::Object3D* addGrass(g3d::Object3D& parent, Vector2 min, Vector2 max, g3d::Field style, uint32_t seed) {
    return parent.add(place(min, max, std::move(style), seed), grassMaterial());
}

}  // namespace flora
}  // namespace rmtn
