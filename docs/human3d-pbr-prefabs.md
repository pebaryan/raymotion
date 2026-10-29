# Native PBR human role prefabs for city scenes

`human3d` provides twelve reusable native `g3d::Object3D` figures sized in metres for the smart-grid CityJSON collection. These are stylized human models built from named geometric parts with Standard PBR skin, fabric, rubber, metal and equipment materials. The figures are native raymotion geometry; they are not CityJSON models.

```cpp
#include "rmtn/raymotion.hpp"

auto scene = std::make_shared<g3d::Scene>();
const auto look = smartgrid3d::cityLook();
auto charger = city::load("data/city/smartgrid/ev-charging-pole.city.json");
auto* pole = city::add(*scene, charger, look);
pole->position = { 2.0f, 0, 0 };

auto* driver = human3d::add(*scene, human3d::Role::EvDriver);
driver->position = { 0, 0, 0 };
auto* technician = human3d::add(*scene, human3d::Role::ChargingTechnician);
technician->position = { 3.5f, 0, 0 };

scene->lights.push_back(g3d::Light::directional({ -3, 6, 5 }, WHITE, 1.5f, true));
// Use g3d::draw(scene) in a raymotion render node.
```

The local origin is between the feet at ground level; `+y` is up and `+z` faces forward. Adults are roughly 1.8–2.0 m tall including hats, and the student is about 1.5 m. Each `add` call returns an independent named group that can be positioned, rotated and scaled. `human3d::add` also accepts a `g3d::Object3D&` parent. `human3d::all()` enumerates the roles; `human3d::name(role)` returns a stable slug. The `left-arm` and `right-arm` groups can be posed or animated with absolute rotations from `t`.

| Role | Visible cue | Example context |
| --- | --- | --- |
| `Resident` | everyday clothes, scarf and satchel | housing, parks |
| `Commuter` | backpack and phone | bus stop, crossing |
| `EvDriver` | charging connector | e-car, charging bay |
| `ChargingTechnician` | hard hat, reflective vest, tool case | charging hub |
| `GridEngineer` | hard hat, vest and tablet | substation, transformer |
| `ConstructionWorker` | hard hat, vest and tool belt | building and road works |
| `OfficeWorker` | shirt, tie and satchel | office, city hall |
| `HealthcareWorker` | clinical coat and stethoscope | hospital |
| `Student` | shorter figure, backpack and book | school |
| `Teacher` | cardigan and book | school |
| `RetailWorker` | apron, badge and parcel | shop, supermarket |
| `TransitOperator` | uniform cap and ticket reader | bus stop |

These are illustration figures, not anatomical scans or safety-equipment specifications. Materials use procedural PBR values without external textures. Clothing and skin colors are authored variations and can be changed on the named parts. The [role catalog](images/prefabs/human3d-pbr-prefabs-sheet.png) shows all twelve, and the [city-scale preview](images/prefabs/human3d-previews-city-scale-context.png) places the EV driver and technician beside the existing CityJSON e-car and charging pole.

To rebuild the stills:

```powershell
cmake --build build --target raymotion_human3d_tests --config Release
.\build\raymotion_human3d_tests.exe --preview out\human3d-previews
python tools/render_human3d_pbr_sheet.py
```
