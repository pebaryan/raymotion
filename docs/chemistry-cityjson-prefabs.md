# Chemistry laboratory CityJSON prefabs

Twelve independent, schematic 3D models for illustrating a chemistry laboratory in raymotion. Each file is CityJSON 2.0, uses metres, sits near the local origin with the floor at `z = 0`, and loads through `city::load`. These are original visual assets with no manufacturer-specific geometry or instrument performance data. For native `g3d::Object3D` builders with per-part PBR materials, use the [lab3d collection](lab3d-pbr-prefabs.md).

| Prefab | Visual parts |
| --- | --- |
| `lab-bench` | worktop, legs, drawers, sink and tap |
| `fume-hood` | base cabinet, work chamber, sliding sash and exhaust collar |
| `chemical-storage-cabinet` | double doors, handles and vent |
| `analytical-balance` | weighing pan, draft shield and display |
| `hotplate-stirrer` | heating plate, knob, beaker and illustrative liquid |
| `rotary-evaporator` | stand, drive, bath, flask, condenser and receiver |
| `hplc-stack` | four instrument modules, displays and solvent bottles |
| `bench-centrifuge` | housing, round lid and controls |
| `drying-oven` | chamber, window, handle and vent |
| `jacketed-reactor` | vessel, frame, lid, agitator, feed port and outlet |
| `glassware-rack` | two shelves, flasks and reagent bottles |
| `vacuum-pump` | motor, pump head and carrying handle |

Files are in `assets/data/city/chemistry-lab/`, with a `manifest.json` describing dimensions and object counts. Visible parts carry `attributes.labRole` and `attributes.paletteRole` so a `city::Look.color` callback can style them. The geometry is deliberately simplified for medium and wide illustrations. Glass is an opaque cyan cue in the inspection render; scenes can choose their own material treatment. The equipment is not a functional process or safety model.

```cpp
const auto hood = city::load("data/city/chemistry-lab/fume-hood.city.json");
auto scene = std::make_shared<g3d::Scene>();
auto* equipment = city::add(*scene, hood, lab3d::cityLook());
equipment->position = { 0, 0, -3 }; // raymotion x east, y up, z south
```

`lab3d::cityLook()` maps each `paletteRole` to a separate Standard PBR material. The CityJSON geometry remains coarser than the native object builders, but metal, glass, surfaces and screens now render with distinct roughness, metalness and opacity. See the [raymotion PBR sheet](images/prefabs/chemistry-cityjson-pbr-sheet.png) alongside the [native PBR sheet](images/prefabs/lab3d-pbr-prefabs-sheet.png).

Regenerate the files with `python tools/build_chemistry_cityjson_prefabs.py`. The older flat geometry inspection sheet comes from `python tools/render_chemistry_cityjson_sheet.py`. `raymotion_city_tests` loads and triangulates all twelve prefabs and checks characteristic parts of the larger instruments. To regenerate the PBR sheet, run `raymotion_lab3d_tests --preview-city out/city-pbr-previews` and `python tools/render_lab3d_pbr_sheet.py --city`.
