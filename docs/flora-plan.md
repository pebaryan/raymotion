# 0.12 plan: vegetation (`flora::`)

Status: M0 done 2026-11-19 (see below); M1 next. Set 2026-11-18; owner's decisions the same day:

1. **Realistic first, then stylised.** Cards with alpha cutout, translucency and PBR leaves
   come first; low-poly / toon presets follow on the same scatter and wind.
2. **2D vegetation is in 0.12.**
3. **Landscape scale:** tens of thousands of trees and about a million grass blades in
   view, so far trees need impostors and grass needs a GPU path (below).

## Goal

Grass, shrubs and trees as one-call parts of a 3D (and 2D) scene, scattered over ground,
a mesh, a map region or a CityJSON model, moving in the wind, and able to **grow on
screen**, since raymotion films reveal things. Every frame stays a pure function of `t`.

Today there is none of this. Films that needed trees made their own:
`examples/showcase/rooftop_dusk.cpp:271` (`addTrees`, eight-faced crowns merged into one
mesh), `bundaran_hi_banner.cpp` (trees along OSM roads, kept off buildings) and `one_sky.cpp`.

## Non-goals

- Botanically accurate species, SpeedTree import, or wind simulated over time (wind is a
  function of position and `t`, never integrated).

## What exists, and where it stops

| Piece | Where | Use / limit |
|---|---|---|
| `g3d::Instances` + `AnimatedMesh` | `scene3d.hpp:186-216` | baked looping clip, `frame` per instance from `t`. **Limit:** `up` refills the list and the CPU culls and builds a matrix per instance every frame (`scene3d.cpp:1377`). Fine for trees (10^4), not for a million blades |
| `crowd::addInstanced` | `crowd.hpp:165` | the pattern for instanced trees |
| `splat::` | `splat.hpp` | soft canopies, distant forest |
| `Geometry::update`, `meshData` | `scene3d.hpp` | hero trees that grow |
| `physics::Wind` | `physics.hpp:22` | the wind vocabulary to reuse (not its integration) |
| `Object3D::cullRadius`, `Scene::cullFloor` | `scene3d.hpp:338,530` | cull per tile / per object, shadows included |
| `city::` vegetation colour | `city.hpp:93` | CityJSON vegetation drawn as flat solids today |
| `vg::`, `dr::` | 2D | 2D vegetation |

## M0. Engine prerequisites

1. **`Material::alphaCutoff`**: lit and shadow fragment shaders discard below it, for
   meshes and instances. With alpha-to-coverage when MSAA is on, so card edges don't crawl.
2. **`Material::translucency`** (+ colour): light through thin leaves and blades, a
   wrapped back-light term. Required for the realistic look (backlit grass at dusk).
3. **`Instance::tint`**: per-instance colour, next to `shade`.
4. **Normal and roughness maps** on `Material` if not already supported (leaf cards).
   Check first.
5. Procedural leaf/blade/bark textures generated in code through `c.assets` (no
   bundled photo textures unless licensed; record the source if any are added).

**Done 2026-11-19:** 1-4. `Material::alphaCutoff` (lit, shadow and ink-line passes; alpha
raised by mip level instead of alpha-to-coverage, since the 3D path supersamples rather than
multisamples), `translucency` + `translucencyColor` (Standard shading only),
`Instance::tint` (packed with `shade` into the instance matrix, 0..254 a channel for 0..2,
so 1.0 is exact), `normalMap` + `normalScale` (derivative-built frame, no tangents) and
`roughnessMap` (glTF metallic-roughness). Script JSON keys, `Material3D` methods, instance
`tint`, README and scripting docs; `examples/scripts/leaf_cards.as`. Tests in
`scene3d_tests` (`vegetationMaterials`): cut-out picture and shadow, translucency levels,
tint and shade exact to 2 levels, normal map towards/away/scale 0, roughness map
identity and highlight. Item 5 moves to M1/M3, where the textures are needed.

**Found during M0:** a directional light about 10° above the horizon gives shadow acne
(horizontal bands) on a flat ground plane. It was already there before M0 (the same picture
from the 17 Nov build is pixel-identical). Low suns are the point of backlit grass, so M1
must fix it: a normal-offset bias in `shadowAt` scaled by the light's grazing angle, with a
test at 5-10° elevation.

## M1. Grass at landscape scale: `g3d::Field`

A new scene primitive, because a CPU instance list cannot carry a million blades per frame.

- The field is a grid of **tiles**. The CPU culls tiles against the view and shadow
  footprint and picks a **level per tile** from distance. Nothing per blade is on the CPU.
- The vertex shader makes each blade from (tile, `gl_InstanceID`): position from a hash
  (deterministic), height from a **height function baked to a texture**, density and
  colour from **mask textures**, slope/altitude limits, and wind from `t` as a uniform:
  bend = base sway + gust wave `f(dot(p, windDir) - speed * t)`, stiffer at the root.
- Levels: near = full curved blades (several segments); mid = fewer segments, wider; far =
  **cards / textured ground blend**, so the field fades into the ground colour without
  popping (density cross-fade over a band, not a cut).
- Shadows: near tiles cast, far tiles only receive. Self-shadow approximated by root
  darkening.
- `flora::addGrass(scene, area, GrassStyle, seed)` builds the field. Same seed → same
  field, any frame order.
- Budget to measure and state in the docs: 1M blades in view at 1080p on the dev GPU,
  with the time per frame, and under Mesa on CI at a reduced count.

## M2. Scattering and areas (shared by everything)

- `flora::Area`: rectangle, polygon footprint, height function, existing mesh (upward
  faces by slope), `geo::` region by ISO code/name on a `geo3d::Surface`, `city::`
  surfaces by type.
- `flora::scatter(area, ScatterSpec, seed)` → placements: Poisson-disk spacing, density
  function/map, clumping noise, exclusion polygons, keep-clear lists (buildings, roads),
  slope/altitude limits. Chunked by tile so 50k trees scatter quickly.
- The same masks feed the grass field, so grass stops at roads and under buildings.

## M3. Realistic trees and shrubs

- `flora::TreeSpec` → mesh: seeded recursive branching (space colonisation for crowns),
  bark-mapped tubes, leaf **cards** in clusters (alpha cutout, translucency, M0).
- Presets, realistic: `oak`, `birch`, `pine`, `spruce`, `palm`, `tropical`, `shrub`, `hedge`.
- Wind baked into an `AnimatedMesh` loop: trunk bend, branch lag, leaf flutter. Phase and
  gust from position and `t`.
- **Tiers by distance**, chosen per tree per frame from the camera (a function of `t`):
  hero mesh → reduced mesh → **impostor** (billboard rendered from the mesh at build time
  through `c.assets`, several view angles, picked by angle, with normals so it is lit) →
  part of the forest canopy blend. Cross-fade between tiers by dithering, not popping.
- `flora::addForest(scene, placements, specs, seed)`: instanced per tier and variant.
- `season` (spring → autumn → bare) as a colour ramp and leaf-card fraction.

## M4. Growth

- A hero tree, or a patch, with `grow(u)` 0..1: trunk extends, branches sprout in rank
  order, leaves unfold last. Fixed topology with a growth rank per vertex; positions set
  from `u` each frame through `Geometry::update` (pure).
- Grass field reveal: density radius from a point, a line or a `geo` region, grown by `u`
  (a uniform; blades rise rather than pop).
- The same `u` reveal convention as `plot::` and `geo::`.

## M5. Stylised presets

On the same scatter, wind and field code: low-poly crowns (as in `rooftop_dusk`),
toon-friendly blades with flat colour bands, splat canopies, paper-cut cards. Checked
under `Shading::Toon`, `Gooch` and `Hatch` and with scene ink lines.

## M6. 2D vegetation

- `flora2d::` (or `vg::`-based helpers): grass strokes along a ground line, tufts, shrubs
  and tree silhouettes (branching drawn with `vg::` paths), each swaying from `t`, each with
  a `u` draw-on/grow reveal.
- Layers for parallax (near/mid/far colour and blur), and a fill-a-shape meadow.
- Works on flat compositions and on `g3d::Surface`.

## M7. Integration

- `city::`: `SolitaryVegetationObject` as trees (height/crown attributes when present),
  `PlantCover` as grass or shrub scatter, opt-in through `city::Style`.
- `geo3d`: forest cover on a map or globe from regions (impostors/splats at map scale).
- Check with `Sky`, fog, tilt-shift, `Filter::film`; no new code expected.

## M8. Scripts, examples, docs

- AngelScript bindings for all of it, landed with each milestone, not saved for the end.
- Examples with the header `tests/test_script.py` enforces: `flora_meadow.as`,
  `flora_forest.as`, `flora_grow.as`, `flora_2d.as`, `flora_stylised.as`.
- A C++ engine showcase `flora` (not a film): wind across a meadow, a landscape fly-over,
  a tree that grows, a 2D scene.
- Prefabs: `flora-meadow`, `flora-tree-grow`, `flora-2d-hill`.
- `docs/flora.md`: API, measured budgets, what is and is not verified.

## Tests

- Determinism: same seed → identical placements and pixels; frame N alone equals frame N
  after N-1.
- Alpha cutout and translucency: cut pixels and cut shadows; a backlit card is brighter
  behind than an opaque one.
- Field: blade count per level matches the formula; no blades outside masks; tile culling
  keeps shadows that fall into view.
- Tiers: a tree crossing a tier boundary changes coverage by less than a set amount
  between frames (no popping).
- Sway: tip moves, root does not; a gust arrives later downwind.
- Growth: `u = 0` draws nothing; `u = 1` equals the full tree.
- 2D: sway and reveal are pure in `t`.
- Benchmarks printed, not thresholds; pixel thresholds with Mesa margins.

## Order and release

M0 → M1 → M2 → M3 → M4 → M5 → M6 → M7, with bindings, tests and examples as each lands.
A release needs M0-M4 and M6 (realistic landscape, growth, 2D); M5 (stylised) and M7
(city/geo) can move to 0.12.x if the release runs long. Ask before cutting.

Risks: the grass field is new renderer code (largest item); impostor baking needs an
off-screen render at build time; a million blades under Mesa on CI must run at a reduced
count.
