# Native PBR chemistry lab prefabs

The `lab3d` collection provides twelve reusable `g3d::Object3D` builders. They construct native raymotion geometry and assign a separate Standard PBR material to each part. Metal, enamel, ceramic, glass, rubber, display and liquid materials use distinct base colours, roughness, metalness and, where appropriate, opacity. No CityJSON loading is involved.

```cpp
#include "rmtn/raymotion.hpp"

auto scene = std::make_shared<g3d::Scene>();
auto* hood = lab3d::add(*scene, lab3d::Kind::FumeHood);
hood->position = { -2.5f, 0, 0 };

auto* bench = lab3d::add(*scene, lab3d::Kind::Bench);
bench->position = { 1.2f, 0, 0 };

scene->lights.push_back(g3d::Light::directional({ -3, 6, 5 }, WHITE, 1.5f, true));
// Use g3d::draw(scene) in a raymotion render node.
```

`lab3d::add` also accepts a `g3d::Object3D&` parent. Each call creates a fresh named group, so copies can be positioned, rotated, scaled and animated independently. The local origin is the footprint centre at floor level, with `+y` up and `+z` as the front. Child objects are named for selective styling or animation with `scene->find`. All geometry is created lazily by raymotion at draw time.

The twelve kinds are `Bench`, `FumeHood`, `ChemicalCabinet`, `AnalyticalBalance`, `HotplateStirrer`, `RotaryEvaporator`, `Hplc`, `Centrifuge`, `DryingOven`, `JacketedReactor`, `GlasswareRack` and `VacuumPump`. `lab3d::all()` enumerates them; `lab3d::name(kind)` returns a stable slug.

These are schematic equipment illustrations in approximate metres. Their physical details and proportions are intended for medium and wide shots, not equipment operation or engineering design. The earlier [CityJSON collection](chemistry-cityjson-prefabs.md) remains available for scenes that need that format.

The [native PBR catalog sheet](images/prefabs/lab3d-pbr-prefabs-sheet.png) is assembled from stills rendered by raymotion itself. To rebuild it:

```powershell
cmake --build build --target raymotion_lab3d_tests --config Release
.\build\raymotion_lab3d_tests.exe --preview out\lab3d-previews
python tools/render_lab3d_pbr_sheet.py
```
