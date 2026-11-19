# 0.12 plan: vegetation (`flora::`)

Status: M0-M7 done 2026-11-19 (see below); M8 done the same day. All milestones done. Set 2026-11-18; owner's decisions the same day:

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
test at 5-10° elevation. **Fixed in M1:** the lookup point moves 1.5 shadow texels times
sin(angle to the light) along the geometric normal; 929 and 3960 darkened pixels at 5 and
10 degrees before, 0 after, and a box's shadow still starts at its foot.

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

**Done 2026-11-19.** `g3d::Field` (`Object3D::field`, `Scene::add(field, material)`) drawn
through the lit, depth and g-buffer programs in a field mode (`FIELD_VS`), so lighting,
shadows, translucency, fog and the M0 materials apply unchanged. Blade k of a tile sits at
point k of the R2 low-discrepancy sequence (shifted per tile by a hash of tile and seed), so
any first n blades cover the tile evenly: far tiles draw fewer, kept by rank, with the last
12 % shrinking away instead of popping, and the ones left widen (up to 3x) to keep the cover
until the fade, where the field gives way to the ground's colour. That replaces the planned
far cards: the ground under the grass is the far level. Levels: full blades (`segments`)
inside `detailDistance`, one segment beyond. `mask`, `ground` and `tall` are functions
baked into one float texture per field (`revision` to remake), with per-tile maxima for
culling and blade counts; the cache holds fields weakly and drops dead ones each frame.
Wind: resting lean, steady sway with flutter, gust fronts travelling downwind at
`gustSpeed`. `flora::` (`rmtn/flora.hpp`): looks `lawn`, `meadow`, `tallGrass`,
`dryGrass`, `grassMaterial()` (translucent, no ink lines), `addGrass`. Script: a `grass`
object descriptor with grids for the maps; `examples/scripts/flora_meadow.as`.
Measured on the dev GPU: about 1.47 M blades submitted at 1080p with shadows, 12-13 ms a
frame including readback (`RMTN_FIELD_BENCH=1 raymotion_scene3d_tests`); the default test
runs a 480x270 version (about 0.3 M blades) so software GL on CI stays quick.
Tests (`scene3d_tests`, `grassField`; `test_script.py`, `test_grass_field`): placement
spread and seeds, mask, fade (nothing submitted beyond it), determinism and purity, sway
(tips move, roots stay), a gust reaching a patch 4 m downwind 30 frames later (measured
30), an off-screen tile's shadow landing in view; the low-sun acne test (`lowSunShadows`,
5 and 10 degrees) fails without the normal offset.
Not done in M1: self-shadow beyond root darkening, a terrain mesh from the same `ground`
grid (the ground plane is the caller's; comes with M2's height areas), ink lines on fields
(off by default; checked in M5).

## M2. Scattering and areas (shared by everything)

- `flora::Area`: rectangle, polygon footprint, height function, existing mesh (upward
  faces by slope), `geo::` region by ISO code/name on a `geo3d::Surface`, `city::`
  surfaces by type.
- `flora::scatter(area, ScatterSpec, seed)` → placements: Poisson-disk spacing, density
  function/map, clumping noise, exclusion polygons, keep-clear lists (buildings, roads),
  slope/altitude limits. Chunked by tile so 50k trees scatter quickly.
- The same masks feed the grass field, so grass stops at roads and under buildings.

**Done 2026-11-19.** `flora::Area` (rect, polygons with even-odd holes, `exclude` rings,
`clearings`, `strips`, `edge` fade, `ground`, `density`, `maxSlope` with a 4-degree fade,
altitude limits; `weight(x, z)`), built by `rect`, `polygon`, `region` (geo rings through
any projection callable, so a 2D projection or a laid-down `geo3d::Surface` both work) and
`onMesh` (upward faces under a slope limit, binned for the highest face over a point).
`flora::noise` (value noise, three octaves) for hills, patches and clumps.
`flora::terrain(area)` makes the ground mesh. `flora::scatter`: Bridson's Poisson disk over
the bounds, then each point kept by the area's weight times a clump mask, by its own hash,
so placements depend only on bounds, spacing and seed (an edit only removes or restores).
About 33k placements over 1 km² in 0.12 s, so no chunking was needed. `addGrass(area)` and
`fit(field, area)` feed the weight and ground to the grass maps. Script: `area`
descriptors for grass, `instances.scatter`, the `terrain` geometry;
`examples/scripts/flora_scatter.as`. Tests: `flora_tests` (new, no window: areas, holes,
exclusions, edges, slope, altitude, density, noise, spacing, determinism, edits that only
remove, density and clumps, the 1 km² timing, mesh faces, France by geo region, `fit`),
`scene3d_tests` (grass keeps off a road and stands on a raised terrain), `test_script.py`
(`test_flora_area_scatter_terrain`). **Moved to M7:** `city::` surfaces by type as areas
(CityJSON is z-up in its own space; with M7's city integration), and a globe-surface area
(`region` covers the flat map).

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

**Done 2026-11-19.** `flora::TreeSpec` + `makeTree` (CPU only): broadleaf crowns by space
colonisation in an egg, cone, umbrella or box envelope (several stems for shrubs and
hedges), conifer whorls with side shoots, palm stems with arching frond ribbons. Radii by
the pipe model; bark tubes along chains, with the thickest child continuing; leaf cards
(broad sprays, needle twigs, fronds) painted in code, cut out, translucent, normals
blended towards the crown's outside, and a crown-occlusion term in the vertex colours.
Presets: oak, birch, pine, spruce, palm, tropical, shrub, hedge (9-29k triangles at full
detail, 0.1-1 s to build). Wind is baked into a loop (trunk bend by height, branch lag by
branch phase and reach, card flutter); `addForest` adds each tree's phase travelling
downwind and a gust lean (instance pitch/bank) from position and t. Levels: hero, then
reduced (thicker wood only, 30 % of the cards enlarged), then an impostor. The impostor is
the hero rasterised on the CPU (in memory, so no files and no `c.assets`) from 8 directions into a colour and tangent-space normal
atlas, shown on a camera-facing quad, with the two nearest views cross-faded. The camera
comes from the new `g3d::drawingCamera()`, which works inside `up`.
Cross-fades use the new `Instance::fade` (a 4x4 Bayer screen door in the lit, depth and
g-buffer passes; f and -(1-f) tile exactly). Also new: `Material::dynamicNormalMap`.
Script `forest` object; `examples/scripts/flora_forest.as`. Tests: `flora_tests` (presets
under the 16-bit index limit, determinism, the wind moves the crown and not the foot,
seasons), `scene3d_tests` (screen door 6320 = 6320 px, a walk across both level changes
steps by at most 756 px against 1636 px unblended, impostor silhouettes overlap the full
tree by 0.80-0.82 from three sides, the crown leans downwind, frames are pure; timing
10,884 trees at 1080p in 26 ms), `test_script.py` (`test_flora_forest`).
**Not done:** the "forest canopy blend" tier past the impostors (fog covers it; `hideFrom`
fades trees out). Impostors only turn about the vertical, so they read as side views from
above. Impostors cast no shadow by default (`impostorShadows`), because a camera-facing
quad's shadow is wrong.

## M4. Growth

- A hero tree, or a patch, with `grow(u)` 0..1: trunk extends, branches sprout in rank
  order, leaves unfold last. Fixed topology with a growth rank per vertex; positions set
  from `u` each frame through `Geometry::update` (pure).
- Grass field reveal: density radius from a point, a line or a `geo` region, grown by `u`
  (a uniform; blades rise rather than pop).
- The same `u` reveal convention as `plot::` and `geo::`.

**Done 2026-11-19.** Trees: `flora::grownPositions(tree, u)` and `addGrowingTree(s)` (at
most 256, each its own mesh rewritten by `Geometry::update` in `up`). A growth front runs
out along the wood over u 0..0.7. A node rides on its parent until the front passes, and
its wood thickens as the wood beyond it grows. Each leaf card unfolds over 0.12 of u from
0.6..0.86 (by rank), never before the front reaches its twig. Every term is zero at u = 1,
so the full tree comes back bit for bit. The wind is scaled by u, and a forest's `wind` = 0
now holds trees at rest: baked frames are relative to frame 0. Grass: `Field::arrival`
(the maps' fourth channel) and `Field::grow`; a blade's height follows a smoothstep over
0.15 of `grow` past its arrival, and tiles whose earliest arrival is not reached are
skipped. `flora::arrivalFrom`, `arrivalAlong`, `arrivalInto`. Script: grass `grow` /
`arrival`, forest `grow` / `growFrom` / `growReach`; `examples/scripts/flora_growth.as`.
Tests: `flora_tests` (u = 1 bit-exact, u = 0 collapsed, stem before crown, leaves after
0.6, arrival fronts), `scene3d_tests` (0 px at u = 0, coverage rising with u, u = 1 vs the
still full tree 0 px different, pure frames; grass 0 px at grow 0, rising in steps under a
third of the total), `test_script.py` (`test_flora_growth`: grow 0 is the bare picture
byte for byte).

## M5. Stylised presets

On the same scatter, wind and field code: low-poly crowns (as in `rooftop_dusk`),
toon-friendly blades with flat colour bands, splat canopies, paper-cut cards. Checked
under `Shading::Toon`, `Gooch` and `Hatch` and with scene ink lines.

**Done 2026-11-19.** `flora::TreeStyle` on `TreeBuild::style`, with `TreeBuild::shading`
applied to the wood, leaf and impostor materials. `LowPoly`: leaf cards gathered into
5..18 clumps (farthest-point centres), each a jittered flat-shaded icosahedron, and
conifers as 3..5 stacked seven-sided cones; limbs only; lighter ink inside crowns; impostors
baked solid. `PaperCut`: the realistic front view, its brightness smoothed into two flat
leaf tones plus bark, with a dark cut edge, on one card that turns to the camera in forests
and while growing. `Splat`: limbs under two flat Gaussian splats a leaf card, lit from
above and darkened inside; the crown leans with the wind and swells in while growing; no
impostors and no shadow. `flora::toonGrass()` / `"toon"` and `g3d::Field::colorBands`.
Ink lines: low-poly crowns use `lineWidth` 0.6 and grass 0; a ground plane under inked
grass wants `lineWidth` 0 too. Fixed on the way: a scene drawn after one with a forest lost
its sky (the shared lit program was left instanced). Tests: `flora_tests` (triangles
realistic / low-poly / paper: oak 14,276 / 860 / 2, spruce 26,086 / 200 / 2; low-poly growth
exact at u = 1; 3,796 splats for an oak; shading on every material), `scene3d_tests` (a
paper oak covers 5,576 px from 0, 90 and 200 degrees and growing; low-poly toon oak draws;
splat oak 6,839 px still, 608 at u 0.5, 6,830 at u 1; the sky after a forest matches the sky
alone, and is black without the fix), `test_script.py` (`test_flora_styles`: five looks,
five different pictures; bad names refused). `examples/scripts/flora_styles.as`.

## M6. 2D vegetation

- `flora2d::` (or `vg::`-based helpers): grass strokes along a ground line, tufts, shrubs
  and tree silhouettes (branching drawn with `vg::` paths), each swaying from `t`, each with
  a `u` draw-on/grow reveal.
- Layers for parallax (near/mid/far colour and blur), and a fill-a-shape meadow.
- Works on flat compositions and on `g3d::Surface`.

**Done 2026-11-19.** `rmtn/flora2d.hpp`, drawn with the node's `Ctx`, so the same calls work
on a flat composition and on a `g3d::Surface`. Blades are tapered strips with a root-to-tip
gradient, bent by `bendAt` (a lean, a sway and gusts that travel along x; pure in t). Grass
along a polyline, tufts, and a meadow filling a polygon in rows back to front (smaller
toward the back by `perspective`). Shrubs: stems and three tones of leaf blobs on a dome.
Trees: a seeded skeleton (broadleaf: a trunk splitting two or three ways per level, each
child keeping 80% of its parent's lean so crowns stay up, lengths scaled to the stated
height; conifer: a stem with side branches shortening up it and needle tiers), wood run out
from the root over u 0..0.7 and thickening behind the front, leaf clumps from u 0.6 in three
tones lit from the top left. Layers: `parallax`, `hazed`, `groundLine`, `heightAt`, `ground`
and `softness` (tilt-shift with no sharp band: a uniform blur). Script: `flora2d::draw`
with `examples/scripts/flora_2d.as`. Not done: a `vg::Path` shape for the meadow (it takes
a polygon) and per-blade motion blur. Tests: `flora2d_render_tests` (u = 0 draws nothing
and u = 1 equals no u; tree coverage rising 39 → 8,228 px; grass at u 0.5 3,194 px on the
left, 0 on the right; wind: 7,790 px change among the tips, 11 at the roots, a still wind
still, the same frame twice identical, a gust peaking at x 0 on frame 2 and at x 300 on
frame 33; a meadow's roots inside its shape; parallax and haze; `softness` 12 grey px
across an edge; 12,041 px on a surface), `flora_tests` (ground lines, presets, wind
direction), `test_script.py` (`test_flora_2d`).

## M7. Integration

- `city::`: `SolitaryVegetationObject` as trees (height/crown attributes when present),
  `PlantCover` as grass or shrub scatter, opt-in through `city::Style`.
- `geo3d`: forest cover on a map or globe from regions (impostors/splats at map scale).
- Check with `Sky`, fog, tilt-shift, `Filter::film`; no new code expected.

**Done 2026-11-19.** `city::addPlants` (opt-in, beside `city::add`; `city::Plants`):
SolitaryVegetationObjects become an oak, birch, pine or spruce (conifers when "species",
"class", "function" or "name" says so), one at each point of a MultiPoint, as tall as the
"height" attribute (numbers or "18 m"), else the geometry, else `treeHeight`. PlantCover
becomes grass over its flat faces (either winding), or shrubs scattered there when its
"averageHeight" is over `shrubHeight`; the PlantCover surfaces stay as the ground under the
grass and the trees' own solids are filtered out (`isPlant`, or by type). `geo3d::forest`
(`geo3d::ForestCover`): Poisson-disk trees in longitude and latitude over `geo::Geometry`
regions, low-poly and instanced, repositioned every frame from `Surface::point` and `up` so
they follow the morph. The pitch and bank that stand a tree along the surface's up are
found by trying raymath's axis order and signs on probes. Growth rises in, with a stagger
from west to east. Sky, fog, tilt-shift and `Filter::film` checked on
`flora_growth.as`; no code needed. Script: city `"plants"`, geo3d `"forest"` and
`geo3d::forest`; `examples/scripts/city_plants.as` and `geo_forest.as`. Tests:
`scene3d_tests` (a city with a building, a species-named 18 m pine, a three-point row,
a lawn and a 2 m shrub bed: one grass field, two forests, the pine taller than the row;
a map forest: 463 tree parts, every one along the surface's up at morph 0, 0.5 and 1
(worst cosine 1.0000), none at grow 0, 3,545 green px on the globe), `test_script.py`
(`test_flora_city_and_map`). Not done: map-scale impostors or splats (low-poly instances
are cheap enough up to the 100,000-tree default limit).

## M8. Scripts, examples, docs

- AngelScript bindings for all of it, landed with each milestone, not saved for the end.
- Examples with the header `tests/test_script.py` enforces: `flora_meadow.as`,
  `flora_forest.as`, `flora_grow.as`, `flora_2d.as`, `flora_stylised.as`.
- A C++ engine showcase `flora` (not a film): wind across a meadow, a landscape fly-over,
  a tree that grows, a 2D scene.
- Prefabs: `flora-meadow`, `flora-tree-grow`, `flora-2d-hill`.
- `docs/flora.md`: API, measured budgets, what is and is not verified.

**Done 2026-11-19.** Bindings, examples and docs landed with each milestone (the examples
are `flora_meadow`, `flora_scatter`, `flora_forest`, `flora_growth`, `flora_styles`,
`flora_2d`, `city_plants`, `geo_forest` and `flora_prefabs`). Prefabs in
`src/engine/flora_prefabs.cpp` (`flora::registerPrefabs`, called by `runCli`):
`flora-meadow`, `flora-tree-grow` (the camera framed on the grown tree, a lawn on ground
out to the haze) and `flora-2d-hill`. The engine showcase `flora`
(`examples/showcase/flora.cpp`): a meadow in gusts, a valley fly-over of 5,025 trees (the
caption prints the scatter's count), a growing oak, a 2D hillside. It is a feature demo,
not a film: no brief or QC. `docs/flora.md` has the API, the prefab props, measured budgets
and what is and is not verified. Test: `test_script.py` (`test_flora_prefabs`: each prefab
draws and changes as it grows; unknown trees, grass and an empty box are refused by name).

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
