# Smart-grid CityJSON prefab collection

## Purpose and provenance

Small, independent CityJSON 2.0 models for raymotion illustrations of a low-voltage smart-grid settlement. Each file is centred near `(0, 0, 0)` in metres, with ground at `z = 0`, so a scene can place and rotate copies. These are original, schematic models, **not** Siemens BIFROST assets or an export of the paper's simulation.

The 2020 paper's Figure 1 and Section II.B describe five charging stations between residential buildings, two commercial buildings, a factory roof with PV, and a distribution substation/transformer. The prefabs cover those visible roles. Siemens' current BIFROST documentation suggests keeping structure types, power-grid equipment and network connections separately selectable; this collection supplies the structures and visual equipment. Power cables and dynamic state belong in the consuming raymotion scene, since these files are geometry assets rather than a simulator.

Sources:

- Peb R. Aryan et al., *Simulation Support for Explainable Cyber-Physical Energy Systems*, MSCPES 2020, §II and Figure 1, DOI 10.1109/MSCPES49613.2020.9133700. Local paper: `D:/code/phd/papers/2020-simulation-support-explainable-cpes/paper.pdf`.
- Siemens, [BIFROST overview](https://bifrost.siemens.com/en) and [Welcome to BIFROST](https://bifrost.siemens.com/en/welcome-to-bifrost): visual settlement building, domain layers and smart-grid scenario presentation. The current BIFROST ZERO interface postdates the paper.
- Siemens, [BIFROST directory](https://bifrost.siemens.com/en/directory): structure types, domains and visual flavors.
- [Vestas wind plant components](https://www.vestas.com/content/dam/vestas-com/global/en/sustainability/reports-and-ratings/lcas/LCA_V802MW_version1.pdf.coredownload.inline.pdf): tower, nacelle, hub and blade roles.
- [Siemens Energy SGT-300](https://www.siemens-energy.com/global/en/home/products-services/product/sgt-300.html), [Caterpillar enclosed generator sets](https://www.cat.com/en_US/products/new/power-systems/electric-power/diesel-generator-sets/1000028917.html), and [Siemens Energy power transformers](https://assets.siemens-energy.com/dam/6813d44e-8a11-47a5-bef3-b13a00f783df/Power-Transformers-Brochure_SE-Final-pdf_Original%20file.pdf): visual equipment categories for the later generator and transformer extensions. Their dimensions and layouts here are illustrative.
- [CityJSON 2.0 specification](https://www.cityjson.org/specs/2.0.2/): object hierarchy and surface semantics.

## Deliverables

`assets/data/city/smartgrid/` contains one `.city.json` per asset, plus `manifest.json`. The Python source of the authored geometry is `tools/build_smartgrid_cityjson_prefabs.py` and can regenerate the collection deterministically.

| Prefab | Role | Visual cue |
| --- | --- | --- |
| `residential-detached` | single household | pitched roof, front door, windows |
| `residential-terrace` | attached housing | repeated bays and entrances |
| `residential-apartment` | multifamily demand | taller flat-roof block and floor bands |
| `commercial-office` | commercial load | glass façade and roof plant |
| `commercial-office-tower` | dense commercial load | ten-storey tower, vertical glazing and podium |
| `city-hall` | civic building | symmetrical wings, clock tower and public entrance |
| `exhibition-event-hall` | public event load | wide-span faceted roof, glass foyer and entrance canopy |
| `commercial-shop` | commercial load | storefront and awning |
| `industrial-factory-pv` | production + load | industrial hall with rooftop PV array |
| `industrial-warehouse` | industrial load | wide pitched hall and loading doors |
| `industrial-assembly-factory` | industrial load | roof monitors, loading bays and process exhaust |
| `data-center` | technical load | window-light shell, louvres and rooftop cooling units |
| `power-station-turbine-hall` | generation site building | large hall, clerestory and equipment doors |
| `power-station-control-building` | generation site building | two-storey operations block, windows and antenna |
| `wind-turbine` | wind generation | tapered tower, nacelle, hub and three blades |
| `diesel-generator` | dispatchable generation cue | enclosed skid, air intake, radiator and exhaust |
| `gas-turbine-generator` | dispatchable generation cue | filter house, turbine, generator, controls and exhaust |
| `hv-transformer` | high-voltage equipment | tank, cooling radiators, bushings and conservator |
| `ev-charging-pole` | one charging point | slim post, screen, cable hook, pad |
| `ev-charging-hub-five-bay` | Figure 1 charging row | five separate posts and marked parking bays |
| `e-car` | mobile demand | compact vehicle body, cabin, wheels |
| `distribution-substation` | low-voltage source/transformer | fenced pad, dark enclosure and equipment |
| `lv-distribution-cabinet` | street-side distribution | freestanding cabinet on utility pad |
| `battery-storage` | storage | ventilated container with inverter cabinet |
| `battery-substation` | grid-scale storage cue | three battery modules, inverter, switchgear, transformer and fenced utility yard |
| `solar-carport` | PV feeder | canopy, posts and panel surface |
| `grid-pole` | overhead network cue | pole, crossarm and insulators |
| `road-straight` | settlement context | roadway, footpaths and markings |
| `foliage-deciduous-tree` | shade tree | lobed broadleaf crown and visible trunk |
| `foliage-conifer-tree` | evergreen tree | three tapered foliage tiers |
| `foliage-street-tree` | urban tree | raised crown and square tree pit |
| `foliage-orchard-tree` | village planting | low fruit tree with colored fruit accents |
| `foliage-shrub-cluster` | low planting | five overlapping bushes |
| `foliage-hedge-segment` | boundary planting | eight-metre clipped hedge segment |
| `foliage-grass-patch` | ground cover | four-metre grass tile with visible tufts |
| `foliage-flower-bed` | ornamental planting | bordered soil bed with flowers |
| `foliage-planter-tree` | plaza planting | small tree in raised planter |
| `street-traffic-light` | road signal | three-lamp head, pole and control box |
| `street-light` | road lighting | tall cantilevered luminaire |
| `solar-street-light` | road lighting + PV | luminaire, tilted PV panel and battery cabinet |
| `traffic-sign-stop` | road sign | red octagonal face without jurisdiction-specific lettering |
| `traffic-sign-warning` | road sign | triangular amber caution marker |
| `traffic-sign-direction` | wayfinding | blue sign with white arrow on two posts |
| `street-pedestrian-crossing` | street marking | zebra-striped street tile and sidewalks |
| `bus-stop-shelter` | public transport | roof, transparent panels, seat and marker |
| `park-bench` | public furniture | wood slats and metal frame |
| `public-waste-bin` | public furniture | lidded freestanding bin |
| `street-bollard` | protective furniture | short post with reflective band |
| `bicycle-rack` | public mobility | three parking loops on a paved strip |

The `ev-charging-hub-five-bay` prefab encodes **five posts** because the paper names five charging stations. Its exact spacing, vehicles, styles and dimensions are illustrative. The `distribution-substation` carries `scenarioRatingKVA: 630` as a reference to the paper's described Figure 1 device, not as a certified physical design or electrical model. The battery substation, office tower, city hall, event hall, assembly factory, data center, power station buildings, generators, wind turbine and HV transformer are user-requested extensions beyond Figure 1. No prefab contains simulated power values or a functional grid connection.

## Format contract

- CityJSON `version: "2.0"`; local metre coordinates with no CRS claim. Each file represents one stand-alone asset. Detailed building fittings use `BuildingInstallation` with `parents` and reciprocal `children` where applicable.
- Every visible object has `attributes.smartGridRole` and `attributes.paletteRole` to support `smartgrid3d::cityLook()`, which assigns per-part Standard PBR roughness, metalness, opacity and base colour in raymotion. Building faces include `GroundSurface`, `WallSurface` and `RoofSurface`; road surfaces include `TrafficArea`, `AuxiliaryTrafficArea` and `Marking`.
- `metadata.title`, `metadata.description`, and `metadata.referenceBasis` identify the asset. `metadata.illustrative` is always true.
- The separate manifest gives dimensions, object counts, and expected role. The generator fails if a prefab contains duplicate IDs or out-of-range vertex references.
- Accessories may deliberately overlap a parent façade or pad by a few centimetres for visibility. This is illustration geometry, not watertight engineering CAD.

## Using in raymotion

```cpp
const auto charger = city::load("data/city/smartgrid/ev-charging-pole.city.json");
const auto factory = city::load("data/city/smartgrid/industrial-factory-pv.city.json");
auto scene = std::make_shared<g3d::Scene>();
const city::Look look = smartgrid3d::cityLook();
auto* pole = city::add(*scene, charger, look);
pole->position = { 18, 0, -5 }; // raymotion axes: x east, y up, z south
auto* plant = city::add(*scene, factory, look);
plant->position = { -16, 0, 12 };
```

The [raymotion PBR catalog sheet](images/prefabs/smartgrid-cityjson-pbr-sheet.png) shows all 49 prefabs with this look; the [foliage sheet](images/prefabs/smartgrid-foliage-pbr-sheet.png) shows the nine plant assets and the [street furniture sheet](images/prefabs/smartgrid-street-furniture-pbr-sheet.png) shows the twelve public-infrastructure assets at larger size. The separate flat-color geometry sheet remains useful for inspecting silhouettes. The CityJSON files store geometry and role labels; PBR finishes are assigned by raymotion through `smartgrid3d::cityLook()` and are not embedded as portable CityJSON appearance data. Model positions and physical cables are authored by the consumer. To show a scenario state such as charging-mode reduction, animate material color or add a separate label/diagram; do not treat the geometry as a measured simulation result. Sign shapes and markings are illustrative, not jurisdiction-specific traffic controls.

## Validation

Regenerate with `python tools/build_smartgrid_cityjson_prefabs.py`. Run the city test executable to parse and triangulate all 49 files and check the five-post hub, parent/child hierarchy, local origin and semantic roles. Inspect `out/smartgrid-cityjson-prefabs-sheet.png` and `out/smartgrid-ev-charging-hub-detail.png` after geometry changes. To rebuild the PBR sheets, run `raymotion_smartgrid3d_tests --preview out/smartgrid-pbr-previews`, then `python tools/render_smartgrid_pbr_sheet.py` with no flag, `--foliage`, and `--street`.
