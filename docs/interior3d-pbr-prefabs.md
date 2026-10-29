# Native PBR interior prefabs

`interior3d` provides 36 reusable `g3d::Object3D` builders for hospital, school, office, IT infrastructure, household and restaurant/supermarket scenes. Each creates native raymotion geometry with named parts and Standard PBR materials. The palette distinguishes enamel, stainless steel, polymer, rubber, upholstery, fabric, wood, laminate, glass and display surfaces through base colour, roughness, metalness and opacity.

```cpp
#include "rmtn/raymotion.hpp"

auto scene = std::make_shared<g3d::Scene>();
auto* bed = interior3d::add(*scene, interior3d::Kind::HospitalBed);
bed->position = { -2.0f, 0, 0 };

auto* desk = interior3d::add(*scene, interior3d::Kind::StudentDesk);
desk->position = { 1.0f, 0, 0 };

auto* chair = interior3d::add(*scene, interior3d::Kind::ErgonomicChair);
chair->position = { 2.5f, 0, 0 };

auto* rack = interior3d::add(*scene, interior3d::Kind::ServerRack);
rack->position = { 4.0f, 0, 0 };

auto* checkout = interior3d::add(*scene, interior3d::Kind::Checkout);
checkout->position = { 6.0f, 0, 0 };

scene->lights.push_back(g3d::Light::directional({ -3, 6, 5 }, WHITE, 1.5f, true));
// Use g3d::draw(scene) in a raymotion render node.
```

`interior3d::add` also accepts a `g3d::Object3D&` parent. Each call creates an independent named group that can be positioned, rotated, scaled and animated. The local origin is the footprint centre at floor level, with `+y` up and `+z` at the front. `interior3d::all()` enumerates the kinds, and `interior3d::name(kind)` returns a stable slug. Parts can be found by name for selective styling or animation. Raymotion creates GL geometry lazily during drawing.

| Group | Kinds |
| --- | --- |
| Hospital | `HospitalBed`, `BedsideCabinet`, `IvStand`, `ExamTable`, `Wheelchair`, `MedicalCart` |
| School | `StudentDesk`, `ClassroomChair`, `TeacherDesk`, `Whiteboard`, `Bookshelf`, `SchoolLockers` |
| Office | `OfficeDesk`, `ErgonomicChair`, `MeetingTable`, `FilingCabinet`, `CubiclePartition`, `Printer` |
| IT infrastructure | `ServerRack`, `NetworkCabinet`, `UpsCabinet`, `Workstation`, `CoolingUnit`, `SecurityCamera` |
| Household | `Sofa`, `DiningTable`, `Refrigerator`, `KitchenRange`, `WashingMachine`, `FloorLamp` |
| Restaurant/supermarket | `CafeTable`, `RestaurantChair`, `ServiceCounter`, `MarketShelf`, `Checkout`, `RefrigeratedCase` |

These are schematic props in approximate metres, intended for medium and wide illustrative scenes. They have editable geometric parts and procedural material values; no external texture maps or manufacturer geometry are required.

The [full catalog](images/prefabs/interior3d-pbr-prefabs-sheet.png) and individual [hospital](images/prefabs/interior3d-hospital-pbr-sheet.png), [school](images/prefabs/interior3d-school-pbr-sheet.png), [office](images/prefabs/interior3d-office-pbr-sheet.png), [IT](images/prefabs/interior3d-it-pbr-sheet.png), [home](images/prefabs/interior3d-home-pbr-sheet.png) and [retail](images/prefabs/interior3d-retail-pbr-sheet.png) sheets are assembled from raymotion renders. To rebuild them:

```powershell
cmake --build build --target raymotion_interior3d_tests --config Release
.\build\raymotion_interior3d_tests.exe --preview out\interior3d-previews
python tools/render_interior3d_pbr_sheet.py
python tools/render_interior3d_pbr_sheet.py --group hospital
python tools/render_interior3d_pbr_sheet.py --group school
python tools/render_interior3d_pbr_sheet.py --group office
python tools/render_interior3d_pbr_sheet.py --group it
python tools/render_interior3d_pbr_sheet.py --group home
python tools/render_interior3d_pbr_sheet.py --group retail
```
