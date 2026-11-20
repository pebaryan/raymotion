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
    from 8 directions at each of 3 elevations, 0, 35 and 70 degrees above the horizon, with
    normals so they are lit). `TreeBuild::elevations` sets the rows: 1 is the horizon only,
    2 is 0 and 50. The atlas grows with the rows: the default oak's is 2.25 MiB with one row
    and 6.75 MiB with three (colour and normals, before mipmaps).

**`addForest`:**
- Picks a level for each tree by its distance from the camera being drawn, and
  cross-fades between levels with a screen door, so trees never pop.
- Adds each tree's own phase in the wind and a gust lean.
- An impostor turns to face the camera fully, from above too, with its picture centred
  on the middle of the crown, and shows the four nearest pictures (two directions, two
  elevations) by a screen door. Directions blend linearly; elevations blend over the
  middle half of the way between two rows only, because two rows disagree more (a tall
  trunk shows differently from each) than neighbouring directions do.
- With `impostorShadows` (on by default), each impostor also casts its shadow from a copy
  that faces the first shadow-casting light and shows the picture nearest the light's
  direction and elevation. The copy is only drawn into the shadow maps. It is submitted
  only inside a directional light's `shadowExtent`, so a forest far outside the map costs
  nothing extra. Set `shadowExtent` wide enough to reach the impostors you want shadows from.
  The impostor itself then receives no shadows (its own copy would shadow half of it); turn
  `impostorShadows` off to have far trees receive them again.
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
- **Forest:** 10,884 trees at 1080p in 26 ms a frame, with levels and impostors. The GPU
  timings are noisy; the best frames of two runs (`RMTN_FIELD_BENCH=1`) were 24–27 ms with
  one row of impostor pictures, 28–29 ms with three rows, and 31–46 ms with three rows and
  impostor shadows. So three rows cost about 10%, and impostor shadows up to about 50% more
  on top; turn `impostorShadows` off where the far trees' shadows don't show. The test prints
  all three.
- **Impostors from above:** the silhouette of an impostor against the full tree (intersection
  over union, oak / birch / pine, mean of three turns) from 60 degrees above the horizon
  rose from 0.48 / 0.71 / 0.47 with one row to 0.85 / 0.78 / 0.80 with three. From the
  side it stays 0.87 / 0.90 / 0.65 (the pine's is low because the thin trunk and the
  sparse crown dominate the outline).
- **Impostor shadows:** from a sun at 25 and 55 degrees the shadowed ground under an oak
  at impostor distance is 1.00 and 0.99 of the full tree's, its centroid within 0.1 m;
  birch 0.92 and 0.84, pine 1.13 and 0.95.
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
  - Impostor silhouettes overlap the full tree by 0.80–0.83 from the side, at least 0.75 from 60 degrees above.
  - An impostor's shadow on the ground is within 15% of the full oak's at two sun heights.
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
- Impostor brightness follows the full tree to about 15 to 20% (mean linear luminance of the
  pixels both cover, default sun with shadows: 0.85 to 1.21 for oak, birch and pine at 0, 30,
  60 and 80 degrees; `tests/scene3d_tests.cpp` prints all twelve). Two things were behind it.
  The leaf pictures left clear texels black, so a full tree's leaf mipmaps darkened with
  distance (fixed: colour is spread into every clear texel, which also makes full trees at a
  distance a little lighter than before). And a picture has no shadow inside the crown, nor
  the spread of facings a texel stands for: the bake stores, in the normal map's alpha, how
  much direct light reaches each texel through the crown (`Material::normalAlphaShade`), and
  keeps the length of the mean normal, which scales direct light. Ambient is not touched. The
  sun's direction is not known at bake time, so the shade is an average one (a fan of sky
  directions leaning to the viewer); a low or backlit sun will show it off. A picture is of a
  tree at one elevation, so between rows a
  tall trunk shows doubled (the crown is aligned, the trunk is not).
- Grass doesn't shadow itself beyond the darkening at its roots. It has no ink lines unless
  you ask for them.
- Map forests have no impostors or splats. Low-poly instances are cheap enough up to the
  default limit.
- The `flora` showcase is an engine demo, not a film: it has no brief, script or QC review.
