// PBR styling for the illustrative smart-grid CityJSON prefab collection.
#pragma once
#include "city.hpp"

namespace rmtn::smartgrid3d {

// Maps paletteRole and surface semantics to base colour, roughness, metalness and opacity.
// Use with city::add(*scene, city::load("data/city/smartgrid/..."), cityLook()).
city::Look cityLook();

} // namespace rmtn::smartgrid3d
