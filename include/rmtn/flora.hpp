// rmtn/flora.hpp -- vegetation (flora::): grass fields at landscape scale, with realistic
// looks to start from. Trees, scattering, growth and 2D vegetation follow (docs/flora-plan.md).
//
//   auto grass = flora::addGrass(*scene, { -100, -100 }, { 100, 100 }, flora::meadow(), 7);
//   grass->field->mask = [](float x, float z) { return std::fabs(x) < 2 ? 0.f : 1.f; };   // a path
//   grass->field->revision++;                                         // after changing a map
//
// The grass is a g3d::Field: every blade is made on the GPU each frame from its tile and its
// number, so a million blades in view cost what their pixels cost. Same seed, same field,
// any frame order. Wind is a function of position and time (g3d::Field::sway, gust, ...).
#pragma once
#include <cstdint>
#include <string>
#include "scene3d.hpp"

namespace rmtn {
namespace flora {

// Realistic grass looks: start from one and change what you need.
g3d::Field lawn();          // short, dense, even and green: mown grass
g3d::Field meadow();        // calf-high mixed grass with some straw
g3d::Field tallGrass();     // waist-high, leaning and swaying more
g3d::Field dryGrass();      // late-summer or savanna grass: straw with a little green

// What grass is drawn with: thin blades that let a light behind them through (translucency),
// a matt surface, and no ink lines (a million outlines would be noise).
g3d::Material grassMaterial();

// A field over x in [min.x, max.x] and z in [min.y, max.y] of the scene (or of `parent`),
// with the look `style` and the seed `seed` (which replaces style.seed). The returned
// object's `field` can be changed further.
g3d::Object3D* addGrass(g3d::Scene& scene, Vector2 min, Vector2 max, g3d::Field style = meadow(), uint32_t seed = 1);
g3d::Object3D* addGrass(g3d::Object3D& parent, Vector2 min, Vector2 max, g3d::Field style = meadow(), uint32_t seed = 1);

// A named look ("lawn", "meadow", "tall", "dry"); throws std::invalid_argument otherwise.
g3d::Field grass(const std::string& name);

}  // namespace flora
}  // namespace rmtn
