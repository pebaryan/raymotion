# Native PBR airplane prefab

`aircraft3d::addPassengerJet` creates a reusable twin-engine narrowbody passenger airplane as a native `g3d::Object3D` group. It uses procedural geometry and per-part Standard PBR materials; it is not CityJSON or an imported model.

```cpp
#include "rmtn/raymotion.hpp"

auto scene = std::make_shared<g3d::Scene>();
auto* jet = aircraft3d::addPassengerJet(*scene);
jet->position = { 0, 0, 0 };
jet->rotation.y = 25;  // choose the heading for the scene
```

The model points toward **+z**, with **+y** up and its origin at ground level. Approximate dimensions are **34 m long**, **35 m span**, and **9 m high**. The landing gear is deployed. `addPassengerJet` also accepts a `g3d::Object3D&` parent and returns a fresh group each time, so instances can be placed and animated independently.

The geometry includes a multi-section tapered fuselage, swept main wings, horizontal and vertical tail, winglets, underwing turbofans, cockpit and cabin windows, wingtip lights, and nose/main landing gear. Named child parts such as `fuselage-shell`, `left-main-wing`, `right-engine-nacelle`, and `nose-wheel-tire` support selective styling and animation. Paint, aluminum, rubber, engine intake and glazing have distinct base colours, roughness, metalness and opacity.

This is an illustrative airliner silhouette, not a manufacturer-specific aircraft or flight-engineering model. The PBR finishes use procedural material values without external texture maps.

Previews rendered by raymotion: [three-quarter](images/prefabs/aircraft3d-previews-passenger-jet-three-quarter.png) · [top](images/prefabs/aircraft3d-previews-passenger-jet-top.png).

```powershell
cmake --build build --target raymotion_aircraft3d_tests --config Release
.\build\raymotion_aircraft3d_tests.exe --preview out\aircraft3d-previews
```
