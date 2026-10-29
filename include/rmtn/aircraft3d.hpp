// Native PBR aircraft prefab for g3d scenes.
// Local origin is ground centre, +y up, +z toward the nose. Dimensions approximate metres.
#pragma once

#include "scene3d.hpp"

namespace rmtn::aircraft3d {

// Twin-engine narrowbody passenger airplane, landing gear deployed.
// Returns a fresh group whose named child parts may be restyled or animated.
g3d::Object3D* addPassengerJet(g3d::Object3D& parent);
g3d::Object3D* addPassengerJet(g3d::Scene& scene);

} // namespace rmtn::aircraft3d
