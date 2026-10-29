# Native PBR computer component prefabs

`computer3d` provides twelve reusable `g3d::Object3D` builders for computer hardware illustrations. They use native raymotion geometry and per-part Standard PBR materials for PCB, metal, contacts, plastic, glass and accent surfaces. No CityJSON or external model import is involved.

```cpp
#include "rmtn/raymotion.hpp"

auto scene = std::make_shared<g3d::Scene>();
auto* gpu = computer3d::add(*scene, computer3d::Kind::GraphicsCard);
gpu->position = { 0.4f, 0, 0 };

auto* ram = computer3d::add(*scene, computer3d::Kind::RamModule);
ram->position = { -0.25f, 0, 0 };

scene->lights.push_back(g3d::Light::directional({ -3, 6, 5 }, WHITE, 1.5f, true));
// Use g3d::draw(scene) in a raymotion render node.
```

`computer3d::add` also accepts a `g3d::Object3D&` parent. Each call returns a fresh named group, so copies can be transformed and animated independently. Child parts have stable names for selective styling. The local origin is the footprint centre at floor level, `+y` is up, and `+z` is the front. Dimensions are in approximate metres. `computer3d::all()` enumerates the kinds and `computer3d::name(kind)` returns a stable slug.

| Kind | Illustration details |
| --- | --- |
| `Cpu` | Substrate, metal heat spreader, edge contacts |
| `RamModule` | PCB, DRAM packages, contacts, heat spreader |
| `GraphicsCard` | Dual fan shroud, PCB, display bracket, PCIe contacts |
| `PowerSupply` | Housing, fan grille, power inlet, modular sockets |
| `PcCase` | Frame, transparent side, fan bank, PSU shroud |
| `CpuAirCooler` | Fin stack, heat pipes, fan |
| `Motherboard` | Socket, DIMM and PCIe slots, heatsinks, I/O and traces |
| `NvmeSsd` | M.2 PCB, NAND packages, controller, contacts |
| `SataSsd` | Drive shell, label, data and power connectors |
| `HardDrive` | Metal cover, platter boss, connectors |
| `CaseFan` | Frame, hub, seven blades, accent ring |
| `AioCooler` | Dual fan radiator, hoses, pump block |

These are schematic models for illustrative close and medium shots. They are not electrically accurate parts or manufacturer replicas. Materials are procedural values without texture maps.

The [catalog sheet](images/prefabs/computer3d-pbr-prefabs-sheet.png) uses stills rendered by raymotion. Rebuild it with:

```powershell
cmake --build build --target raymotion_computer3d_tests --config Release
.\build\raymotion_computer3d_tests.exe --preview out\computer3d-previews
python tools/render_computer3d_pbr_sheet.py
```
