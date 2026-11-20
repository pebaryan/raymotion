# Vegetation (flora)

Grass, shrubs and trees for 3D and 2D scenes. Plants are scattered over ground, a mesh, a
map region or a CityJSON model. They move in the wind and can grow on screen. Every frame
is a function of `t`: any frame can be drawn alone, and the same seed gives the same plants.

- `rmtn/flora.hpp`: 3D grass (`g3d::Field`), areas and scattering, trees and forests,
  growth and styles.
- `rmtn/flora2d.hpp`: 2D vegetation drawn with the node's `Ctx`.
- `city::addPlants` and `geo3d::forest`: city vegetation and map forest cover.
- Three prefabs: `flora-meadow`, `flora-tree-grow` and `flora-2d-hill`.
- An engine showcase: `raymotion render flora`.

The plan, with each milestone's decisions and test numbers, is in
[flora-plan.md](flora-plan.md). Script keys are in [scripting.md](scripting.md).

## Grass

```cpp
flora::Area land = flora::rect({ -40, -40 }, { 40, 40 });
land.ground = [](float x, float z) { return 1.5f * flora::noise(x, z, 30, 1); };
scene->add(flora::terrain(land), groundMaterial);
flora::addGrass(*scene, land, flora::meadow(), /*seed*/ 1);
```

A `g3d::Field` is grass made on the GPU. Blades are placed in tiles by a low-discrepancy
sequence, so far tiles draw fewer blades without the cover thinning unevenly. Near the
camera blades are full (`segments`); beyond `detailDistance` they have one segment. At the
fade distance the field gives way to the ground's colour.

- **Looks:** `lawn`, `meadow`, `tallGrass`, `dryGrass` and `toonGrass`, or
  `flora::grass(name)`.
- **Wind:** a resting lean, sway with flutter, and gust fronts that travel downwind at
  `gustSpeed`.
- **Maps over the area:** `mask` (density), `ground` (height) and `tall` (height
  multiplier). Bump `revision` after changing them.

## Areas and scattering

A `flora::Area` says where plants go. It can be built as:

- `rect` or `polygon` (polygons can have holes);
- `region`: a geo geometry through any projection;
- `onMesh`: the upward faces of a mesh, under a slope limit.

On top of that it can carry `exclude` rings, `clearings`, `strips` (roads and paths), an
`edge` fade, `ground`, `density`, `maxSlope`, and altitude limits.

`flora::scatter` returns Poisson-disk placements. Each point is kept by the area's weight
and an optional clump mask. Placements depend only on the bounds, spacing and seed, so
editing a road or a density only removes or restores plants; the others don't move.
`flora::terrain(area)` makes the ground mesh.

## Trees and forests

```cpp
std::vector<flora::TreePtr> kinds = { flora::makeTree(flora::oak()), flora::makeTree(flora::birch()) };
flora::ScatterSpec sc; sc.spacing = 6; sc.clumping = 0.5f; sc.variants = 2;
flora::addForest(*scene, flora::scatter(land, sc, 7), kinds);
```

**Presets:** oak, birch, pine, spruce, palm, tropical, shrub and hedge, or
`flora::tree(name)`.

**`makeTree`:**
- Runs on the CPU, so it works before a window exists.
- Builds:
  - bark tubes;
  - painted leaf cards (cut out and translucent);
  - wind baked into a loop;
  - three levels: the full tree, a reduced tree, and an impostor (pictures of the tree
    from 8 directions, with normals so they are lit).

**`addForest`:**
- Picks a level for each tree by its distance from the camera being drawn, and
  cross-fades between levels with a screen door, so trees never pop.
- Adds each tree's own phase in the wind and a gust lean.
- `ForestSpec` sets:
  - the level distances (`reducedFrom`, `impostorFrom`, `blend`, `hideFrom`);
  - the wind;
  - `impostorShadows`.

## Growth

Growth uses a reveal `u` from 0 to 1, the same convention as `plot::` and `geo::`. At `u = 1`
the tree is exactly the full tree.

**Trees:**
- The trunk extends first.
- Branches sprout as the growth front reaches them, and thicken behind it.
- Leaves unfold from `u` 0.6.
- Calls: `addGrowingTree` and `addGrowingTrees`, at most 256 trees, each with its own mesh.

**Grass:**
- Set `Field::arrival` to when each spot comes up, and `Field::grow` to `u`.
- `arrivalFrom`, `arrivalAlong` and `arrivalInto` give the usual fronts: out from a point,
  out from a line, and in from an area's edges.

## Styles

Set `TreeBuild::style`, plus `TreeBuild::shading` for toon, Gooch or hatching:

- `Realistic`: the default.
- `LowPoly`: faceted leaf clumps, and cones for conifers.
- `PaperCut`: one flat card that turns to face the camera.
- `Splat`: a Gaussian-splat crown. At most 256 trees, no shadows.

Use `toonGrass()` with `Field::colorBands` for flat grass.

## 2D

`flora2d::grass`, `tuft`, `meadow`, `shrub` and `tree` draw with the node's `Ctx`, so they
work on a flat composition and on a `g3d::Surface`. They take:

- a frame `t`;
- a growth `u`;
- a seed;
- a `flora2d::Wind`, whose gusts travel along x.

Layer helpers: `groundLine`, `heightAt`, `ground`, `parallax`, `hazed` and `softness`.

**Meadows from a path.** `meadow` takes a `vg::Path` as well as a polygon, with an optional
`vg::FillRule` (default `NonZero`, as `vg::fill`; with `EvenOdd` any inner contour is a
hole, with `NonZero` a hole is wound the other way). Curves are flattened to 0.1 px, so a
root is never more than that outside a curved edge. `meadowRoots` returns where the blades
root, for tests or for placing things among them. In scripts a meadow item takes `"path"`
(SVG path data) and `"rule"` instead of `"shape"`. See `examples/scripts/flora_2d_path.as`.

**Motion blur.** The wind reads time as `frameTime(t)`, so with `Comp::motionBlur` (or a
layer's) the blades are drawn at each sampled moment and their tips blur; roots stay
sharp, as they barely move. Without motion blur `subframe()` is 0 and the wind reads `t`
as before. Growth is set by the caller's `u`, so it blurs only if `u` is itself computed
from `frameTime`.

## Cities and maps

**`city::addPlants(scene, model, plants)`:**
- `SolitaryVegetationObject`s become trees, as tall as their `height` attribute. Conifer
  species give pines or spruces.
- `PlantCover` becomes grass, or shrubs when its `averageHeight` is over `shrubHeight`.
- Filter the trees' solids out of `city::add` yourself.
- In scripts, `"plants": true` on a city does both.

**`geo3d::forest(parent, surface, regions, cover)`:**
- Scatters low-poly trees over geo regions.
- The trees stand along the surface's up on a globe, on the map and in between.
- They grow in from west to east.
- Script: the `"forest"` key on a geo3d descriptor, or `geo3d::forest`.

## Prefabs

`runCli` registers these prefabs (`flora::registerPrefabs()`), so they work in C++
compositions and in scripts:

```angelscript
prefab::use(0, 120, "flora-tree-grow", "{\"box\":[0,0,1280,720],\"tree\":\"birch\"}");
```

Each prefab fills `box` (`[x, y, width, height]` in node px; default `[0, 0, 1920, 1080]`).
`delay` and `duration` are frames, local to the instance. If a prop names a tree or grass
that doesn't exist, the prefab throws an error naming it.
`examples/scripts/flora_prefabs.as` uses all three.

### flora-meadow

A meadow on gentle hills with trees in the wind; the camera circles it slowly.

| Prop | Default | Meaning |
| --- | --- | --- |
| `grass` | `"meadow"` | `lawn`, `meadow`, `tall`, `dry`, `toon` |
| `size` | `40` | half-width of the land, metres |
| `hills` | `1.5` | metres of rise and fall |
| `trees` | `10` | how many, at most (the camera's circle is kept clear) |
| `kinds` | `["oak", "birch"]` | tree presets |
| `wind` | `1` | scales the sway and gusts; 0 is still |
| `orbit` | `0.08` | degrees a frame at 30 fps |
| `distance`, `height` | `15`, `2.2` | the camera's circle and eye height, metres |
| `ground` | `"#5d6b3c"` | ground colour |
| `fps`, `seed` | `30`, `1` | |

### flora-tree-grow

One tree grows from a sapling: trunk, then branches, then leaves. A lawn comes up around
it, and the camera circles slowly. The camera is framed on the grown tree.

| Prop | Default | Meaning |
| --- | --- | --- |
| `tree` | `"oak"` | any tree preset |
| `grass` | `"lawn"` | the lawn round it |
| `delay`, `duration` | `0`, `150` | the growth, frames |
| `wind` | `0.6` | |
| `orbit` | `0.1` | degrees a frame at 30 fps |
| `ground` | `"#55663a"` | |
| `fps`, `seed` | `30`, `1` | |

### flora-2d-hill

A 2D hillside: sky, a hazed far ridge with silhouette trees, and a near hill with trees,
shrubs and grass. Everything grows in.

| Prop | Default | Meaning |
| --- | --- | --- |
| `top`, `horizon` | `"#8fb8d8"`, `"#efe4cc"` | the sky gradient |
| `far`, `near` | `"#8aa58f"`, `"#5b7d3c"` | the ridge and the hill |
| `grass` | `"meadow"` | `lawn`, `meadow`, `tall`, `dry` |
| `trees` | `4` | on the near hill, 0..40 |
| `delay`, `duration` | `0`, `90` | the growth, frames |
| `wind` | `1` | |
| `fps`, `seed` | `30`, `1` | |

## Measured budgets

These numbers were measured on the development GPU unless a line says otherwise. Each
milestone's tests print them, so they can be measured again.

- **Grass:** about 1.47 million blades submitted at 1080p with shadows, 12–13 ms a frame
  including readback (`RMTN_FIELD_BENCH=1 raymotion_scene3d_tests`).
- **Forest:** 10,884 trees at 1080p in 26 ms a frame, with levels and impostors.
- **Scatter:** about 33,000 placements over 1 km² in 0.12 s, measured on the CPU.
- **Tree build:** 0.1–1 s per preset. A full tree is 9,000–29,000 triangles; the low-poly
  oak is 860 and the paper oak is 2.
- **Limits:**
  - Growing trees and splat trees: at most 256 per call, because each has its own mesh or
    cloud.
  - `geo3d::forest`: at most 100,000 trees by default.
- **The `flora` showcase fly-over:** 5,025 trees with grass over a 640 m square.

## What is verified

Tests: `flora_tests`, `scene3d_tests`, `flora2d_render_tests` and `test_script.py` (the
`test_flora_*` tests).

- **Determinism:**
  - The same seed gives the same placements and pixels.
  - Frames are pure: frame N alone equals frame N drawn after N−1.
  - Edits to an area only remove or restore plants.
- **Materials:**
  - Alpha cut-outs and their shadows.
  - Translucency levels.
  - Instance tint, exact to 2 levels.
  - Normal maps and roughness maps.
  - Low-sun shadow acne: gone at 5 and 10 degrees.
- **Grass:**
  - Spread, mask, fade and purity.
  - Tips move and roots stay.
  - A gust arrives 4 m downwind 30 frames later.
- **Forests:**
  - A level change steps by at most 756 px, against 1,636 px without the blend.
  - Impostor silhouettes overlap the full tree by 0.80–0.82.
  - Crowns lean downwind.
- **Growth:**
  - `u = 1` gives the full tree bit for bit.
  - `u = 0` draws nothing.
  - Coverage rises steadily.
  - Grass rises in steps under a third of the total.
- **Styles:** each look draws differently; the sky survives a forest drawn earlier.
- **2D:** growth, wind, gust travel, parallax, haze and softness.
- **2D paths and blur:**
  - Path meadow roots stay within 0.05 px of the shape (limit 0.5), with none in a hole.
  - The polygon meadow's roots match values recorded before the path overload.
  - With motion blur the tips differ and the roots and ground don't.
  - A still wind is unchanged by blur.
- **Cities and maps:**
  - A species-named pine is taller than its street trees.
  - Lawns become grass and shrub beds become shrubs.
  - Every map tree stands along the surface's up at morph 0, 0.5 and 1 (worst cosine
    1.0000).
- **Prefabs:** each one draws, changes as it grows, and refuses unknown names.
- **Post effects:** sky, fog, tilt-shift and `Filter::film` together were checked by eye
  on `flora_growth.as`, not by a test.

## What is not done

- Past the impostors, nothing replaces far trees (no canopy blend). Fog covers that range,
  or `hideFrom` fades trees out.
- Impostors only turn about the vertical axis, so from high above they read as side views.
- Impostors cast no shadow by default.
- Grass doesn't shadow itself beyond the darkening at its roots. It has no ink lines unless
  you ask for them.
- Map forests have no impostors or splats. Low-poly instances are cheap enough up to the
  default limit.
- The `flora` showcase is an engine demo, not a film: it has no brief, script or QC review.
