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
- **Shadows:** `castShadows` (on, as before) draws the blades into the shadow maps, so a
  tall tuft shades the grass behind it and the ground, and the lawn's blades shade the soil
  between them under a low sun. Only tiles nearer than `shadowDistance` to the camera and
  inside a directional light's `shadowExtent` are drawn, with the same wind at the same t as
  the main pass. `shadowDensity` (1: every blade, as always) is the saving: below 1 a random
  share of the blades is drawn, each widened by 1 / `shadowDensity` (at most 3 times) so the
  shadow keeps its weight. At 0.25 the shadow pass costs a quarter of the blades, and under a
  low sun the soil seen between blades from 1.5 m up shows a blocky weave of blade shadows
  in the nearest metres; leave it at 1 where that shows. `castShadows` false stops the grass
  casting at all. A high camera (aerial shots) needs `shadowDistance` above its height.

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
    from 8 directions at each of 3 elevations, 0, 35 and 70 degrees above the horizon (4 or more: up to 90), with
    normals so they are lit). `TreeBuild::elevations` sets the rows: 1 is the horizon only,
    2 is 0 and 50, 4 is 0, 30, 60 and 90. The atlas grows with the rows: the default oak's is 2.25 MiB with one row
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
- **Far canopy** (`ForestSpec::canopyBelow` in pixels, or `canopyFrom` in metres; off by
  default): where a cell's trees are only a few pixels tall they give way to one canopy
  patch, and the trees there are not drawn.
  - The hand-over follows screen size, as the map forests do: a cell's patch takes over where
    its trees' mean height is under `canopyBelow` px (`height x drawingViewportHeight / (2 d tan(fov/2))`;
    7 is a good value: a 15 m tree at 1080 px and 40 degrees is 7 px at about 1.5 km). `canopyFrom`
    stays as a distance in metres (and a floor when both are set). At 25-30 px the impostors are
    right and a height field is not.
  - The ground is cut into square cells of `canopyCell` metres (default 128). Only cells well
    inside the forest get a patch: the cell and its eight neighbours must hold a quarter of the
    median tree count. The forest's boundary and its clearings keep their trees whatever the
    distance, so the edge a viewer sees is trees, never the edge of a height field.
  - A patch is a height field sampled every `canopyDetail` metres (3): a broadleaf crown is a
    dome, a conifer a cone, outlines pushed in and out by noise, with relief on top; between
    crowns the surface closes at 0.62 of the crown's height. Quads are triangulated with the
    diagonal either way by a hash, so the grid leaves no regular pattern.
  - Colour is the crown colour of each node's tallest tree (`Tree::crownColour`), darker towards
    the crown base and on steep sides, with a grain picture of non-repeating noise (a 512 px
    picture covering 24 m, its uv sheared by height so sides do not streak). Normals are
    nearly flat, the patch receives no shadows and casts none unless `canopyShadows` (default
    false: at 128 m cells it made the canopy as slow as trees, and far cells are mostly outside
    `shadowExtent`).
  - The patches match the trees only from low views. A camera higher than `canopyMaxElevation`
    (default 20 degrees, seen from the forest's middle; 0 = no limit) keeps every tree, since
    from above the patches show as blotches against the trees around them.
  - The cell's trees and patch share one screen door by the distance to the cell's trees over
    `canopyBlend` metres (default half a cell); a whole cell fades together.
  - Brightness follows the view and the sun. A field of crowns behaves like a field of lit
    spheres: lighter looking down-sun, darker into it, darker still under a low sun, where
    crowns shade each other; a flat patch is lit by the sun's height alone, so one gain cannot
    fit both. Each patch's `Instance::shade` is looked up per frame from a table over view
    elevation, sun elevation and the angle between sun and camera (5 x 5 x 5, blended in log
    space), measured once by `canopyTests` with `RMTN_L4_CALIBRATE=1` (trees against patches at
    two shades over the grid, one refinement) and hard-coded as `CANOPY_SHADE` in
    `flora_tree.cpp`. It uses the first directional light; with none, shade is 1. `canopyShade`
    (> 0) fixes it for calibrating. Patch normals are nearly flat (0.12 of the slope), so a
    cone's dark side no longer shows by azimuth; colour is half the tree's crown colour, half
    the forest's mean, so one dark pine does not darken a whole cell.
  - Each cell's hand-over distance is jittered by a stable hash (+-25%), so cells in a row cross
    over in a ragged front, not a line.
  - Patches are built the first frame a cell is wanted; `addForest` only makes empty cells, so
    building a forest and `raymotion list` pay nothing. A frame is still a function of `t`.
  - Cost: each patch is one draw. A 77,000-tree forest 0.8-4 km away in a 480x270 picture
    submits 145,094 bodies as impostors, 16,053 as canopy in 64 m cells (x0.11) and 20,259 in
    128 m cells (x0.14); triangles 290,188 / 1,263,398 against 403,980,860 as reduced trees.
    Frame time in a 720p bench (39,245 trees, 1.2 x 3 km): trees 29-43 ms best-of-8, canopy 25-47,
    canopy with shadows 27-42, 64 m cells 30-62 (very noisy run to run): no clear gain at that size, because the draw
    count, the impostor pass and the trees nearer than the hand-over dominate. It is a gain in
    bodies and triangles, not yet in milliseconds.
- `ForestSpec` sets:
  - the level distances (`reducedFrom`, `impostorFrom`, `blend`, `hideFrom`);
  - the far canopy (`canopyBelow`, `canopyFrom`, `canopyCell`, `canopyDetail`, `canopyBlend`, `canopyShadows`, `canopyMaxElevation`);
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
- **Levels by size on screen.** Each tree's height in pixels (composition px, from the camera
  being drawn: perspective or orthographic) picks its level:
  - at or above `impostorBelow` (default 10) it is the low-poly mesh;
  - below it, it is a picture of itself: a quad of the flora impostor atlas (8 directions
    at 4 heights, 0, 30, 60 and 90 degrees, baked lazily at the first draw) that faces the camera and stands along
    the surface's up, not the world's, so it is right on the globe, on the map and
    between. The picture's height and direction are read against the surface's
    up and the tree's own tilt. One picture a tree (the nearest): the four-way blend of
    `flora::addForest` leaves cut-out holes that show the quads below as dark spokes in a
    dense forest seen from above, so the picture changes in steps as the view turns;
  - the two cross-fade by screen door over `blend` (0.3: a band of 30% of `impostorBelow`,
    centred on it), so nothing pops;
  - `impostorBelow: 0` keeps every tree a mesh, as before. Trees whose build brings no pictures
    (paper cut-outs, splats) stay meshes.
- **Merging.** Under `mergeBelow` px (default 12) only a fraction `(px / mergeBelow)^2` of the
  trees stay, chosen by a stable rank, each drawn larger by the inverse root so the area they
  cover is the same (and the ground's tint under the forest). At 3 px, 6% of the trees are
  drawn at 12 px. A tree fades by screen door as its rank passes, over a band of 30% of the
  kept share (a fixed band would leave every tree of the 6% half transparent: a faint, dotted
  patch at 7 radii). The enlarged quad stays centred where the unenlarged crown's middle is, so
  from above trees overlap by their own heights, not by how far from the camera they are.
  `mergeBelow: 0` turns it off. `dropBelow` (default 0) drops trees under a height outright;
  that lightens the forest's colour, which merging does not.
- **Shadows.** Impostors cast none (maps seldom use shadow maps, and a shadow only a few
  pixels long is not worth a second set of bodies). Where a light does cast shadows, a tree's
  shadow fades out as it becomes a picture. Pictures do receive shadows.
- The level is a function of the camera's position and the frame, never of the frames before.
  The forest's `up` must not sit under a moved parent: it reads the camera in the forest's own
  space (as `flora::addForest` does).
- A map forest's pictures are matched to the mesh forest's look (`LowPoly` trees only): dimmed a
  little from above (a faceted crown's picture is brighter than the crown seen from 60 to 90
  degrees), tinted a little yellower, and each picture's brightness spread by a stable per-tree
  factor, `pictureSpeckle` (0.30 = 1 +- 30%). The default trees bake four rows (0, 30, 60 and
  90 degrees, the top row taken from 89.5), and bake no trunk above 45 degrees: from above
  only a crown shows. Seen from straight above:
  - from 50 to 75 degrees up, the direction the picture is taken from and its top turn to the
    camera's own (the screen's up), the same for every tree; from 55 to 75 the quad lies flat
    along the surface instead of facing the camera (a quad that faces it is tilted by how far
    the tree is from the point below the camera, so each tree's outer edge covers its outward
    neighbour: strokes along the radius, a swirl round a dark spot);
  - what is measured (`mapForestTests`, top-down and from 60 degrees, 3.5 to 1.5 radii): orange
    (red above green) and brown pixels, luminance in five rings round the middle, the darkest
    9 px box, the radial structure of the strokes, blotchiness and grain.
  The known difference: a picture is smoother at the pixel scale than the facets of a mesh a
  few pixels high (local 3x3 contrast 0.52 of the mesh's top-down and 0.45 from 60 degrees, at
  3.5 and 2.6 radii) and its crowns read as rounded blotches (8 px cell spread about 1.5 of the
  mesh's). Pictures are read about 1.5 mipmap levels finer than their footprint (a mesh that
  small aliases, one facet a pixel), which gives back part of the grain. From 60 degrees a
  forest of pictures is also about 5% brighter than its meshes while one tree's picture alone is
  0.71 to 0.81 of its crown: one shade by camera elevation serves the forest (a known limit of
  0.13; the tests say so where they allow it). Normal mipmaps that keep one facet's normal
  instead of the mean were tried and changed nothing measurable, so the missing contrast is in
  the 60-degree picture itself. `impostorBelow` is 10 px so the pictures stand only where a tree
  is under a dozen pixels and the difference is the least visible. At 2 radii and closer the
  forest is meshes.

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
- **Grass shadows (`castShadows`)**, the same 1.47 million blades at 1920x1080, a 28 m shadow
  map, best of 12 frames with readback (a noisy GPU: the mean is 1 ms more): off 9.8 ms, 0
  blades in the shadow pass; on at `shadowDensity` 1, 10.5 ms and 230,400 blades; at 0.35,
  9.9 ms and 80,640. The shadow-pass counts are exact (`fieldShadowBladesSubmitted()`); the
  tests check them and print the times. Darkness behind a 1.5 m tuft at a 25-degree sun
  against the sun side, 480x270, azimuth 40: 0.56 at density 1, 0.55 at 0.5, 0.35 and 0.2,
  0.63 at 0.1. Off, the same boxes read 1.01. The default stays 1 (the old path: no random pick, no widening).
- **Forest:** 10,884 trees at 1080p in 26 ms a frame, with levels and impostors. The GPU
  timings are noisy; the best frames of two runs (`RMTN_FIELD_BENCH=1`) were 24–27 ms with
  one row of impostor pictures, 28–29 ms with three rows, and 31–46 ms with three rows and
  impostor shadows. So three rows cost about 10%, and impostor shadows up to about 50% more
  on top; turn `impostorShadows` off where the far trees' shadows don't show. The test prints
  all three.
- **Impostors from above:** the silhouette of an impostor against the full tree (intersection
  over union, oak / birch / pine, mean of three turns) from 60 degrees above the horizon
  rose from 0.48 / 0.71 / 0.47 with one row to 0.85 / 0.77 / 0.80 with three. From the
  side it stays 0.87 / 0.90 / 0.65 (the pine's is low because the thin trunk and the
  sparse crown dominate the outline).
- **Impostor shadows:** from a sun at 25 and 55 degrees the shadowed ground under an oak
  at impostor distance is 1.00 and 0.99 of the full tree's, its centroid within 0.1 m;
  birch 0.92 and 0.84, pine 1.13 and 0.95.
- **Map forest, 100,000 trees at 1280x720 (flat map, readback included):** bodies drawn and
  triangles, then best-of-18 frame times in a quiet run (a loaded machine reads 1.5 times
  higher; the test prints the mean too). The frame is CPU-bound here (about 0.15 to 0.3 µs a
  body to gather and draw), so triangles fall 230 times but ms only follow the body count.

  | camera (globe radii up; tree height) | every tree a mesh | meshes and pictures | and merged (default) |
  | --- | --- | --- | --- |
  | 7 (3 px) | 166,679 bodies, 45.9 M tris, 48-53 ms | 100,000, 0.2 M, 61-69 ms | 8,763, 0.018 M, 6.8-8.4 ms |
  | 3.5 (6 px) | 166,679, 45.9 M, 63-70 ms | 100,000, 0.2 M, 57-64 ms | 35,384, 0.07 M, 21-24 ms |
  | 1.2 (18 px) | 166,679, 45.9 M, 62-73 ms | 100,000, 0.2 M, 53-60 ms | 166,679 meshes, 45.9 M, 61-73 ms |
  | 0.35 (62 px) | 120,809, 33.6 M, 61-63 ms | about the same | unchanged (all meshes) |

  Best-of-18 times over three runs on a loaded machine (a quiet run read 5.6 ms at 7 radii and 14
  at 3.5; this one is noisy: the mean columns ran 30% higher). The "meshes and pictures"
  column is every tree a picture below 36 px with no merging, to show what merging alone
  buys: pictures cut triangles 230 times and bodies 1.7 times, not the time; merging cuts the
  bodies (19 times at 7 radii, 4.7 at 3.5) and with them the frame. At 1.2 radii and closer
  the default forest is meshes (the picture threshold is 10 px), so nothing is saved there, by
  choice. The tests check the counts, never a time.
- **Map forest, one tree against its picture** (the three default trees, morph 0, 0.5 and 1,
  six directions, 0 to 90 degrees up): silhouette overlap 0.72 to 0.87, luminance 0.88 to
  1.18 of the mesh's, the same at every morph. The mean colour of a pictures-only forest over
  a mesh forest, at 13 to 60 px: rgb 65 to 70, 93 to 98, 42 to 46 against 72 to 73, 101, 46.
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
  - `castShadows`: a 1.5 m tuft darkens the grass 2.2 m behind it to 0.55 of the sun side's
    luma at two sun azimuths, and not across the sun (1.0); off, 1.01. The shadow's mean
    position moves 0.57 m in a gust that moves the blades' 0.43 m; with the shadow pass's wind
    frozen it moved 0.01 m. The shadow-pass blade counts are exact, the frame is pure.
- **Forests:**
  - A level change steps by at most 756 px, against 1,636 px without the blend.
  - Impostor silhouettes overlap the full tree by 0.80–0.83 from the side, at least 0.75 from 60 degrees above.
  - An impostor's shadow on the ground is within 15% of the full oak's at two sun heights.
  - Crowns lean downwind.
  - Far canopy against the trees it replaces where they are 5-8 px tall (canopyBelow 7), from 7, 30 and 60 degrees, sun 25 and 55: luminance 1.12 / 1.05 / 1.06 and 1.10 / 1.13 / 1.09; the 1280x720 30-degree shot under a 25-degree sun: canopy / trees 0.91-1.01 by 16-row window (far half 0.95), biggest step between windows 0.029; strongest repeat under 0.35 (hatch measure checked on a synthetic hatch: 0.76); bodies -89% in a far forest.
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

- Far canopy is for low cameras only. Above `canopyMaxElevation` (20 degrees by default) every
  tree is kept, because in 30-degree high shots a few cells read darker or greener than the
  trees around them (the mismatch depends on view, sun and the sky's light, and no per-cell
  correction removed it; that study stays in `canopyTests` behind `RMTN_L4_HIGH=1` and its checks
  fail by design). Clearings smaller than a cell are filled in. The shade table is measured for
  one forest and rig (5 px trees) with a 1.08 fudge, so a very different forest or ambient may
  need recalibrating (`RMTN_L4_CALIBRATE=1`). It saves bodies and triangles (-89%) but no clear
  frame time at 720p. See docs/leftovers-plan.md L4.
- Where the canopy is off, is not allowed (a high camera), or does not cover a cell (a forest's
  edge, a clearing), nothing replaces far trees but the impostors. Fog covers that range, or
  `hideFrom` fades trees out.
- Impostor brightness follows the full tree to about 20%, pine side-on to 30% (mean linear
  luminance of the pixels both cover, default sun with shadows: oak 0.97 to 1.06, birch 0.81 to
  0.98, pine 0.78 to 1.29 at 0, 30, 60 and 80 degrees; `tests/scene3d_tests.cpp` prints all
  twelve). A picture has no shadow inside the crown, nor the spread of facings a texel stands
  for. The bake draws the crown (cards cut out exactly as the engine cuts them) into depth maps
  from a fan of directions to the light and stores, in the normal map's alpha, the share of them
  that light each texel (`Material::normalAlphaShade`); it scales only lights that cast shadows.
  The mean normal keeps its length, and the light treats it as a cone of facings (the average of
  the clamped cosine over the cone, not the mean's cosine). The sun's direction is not known at
  bake time, so the shade is an average over the fan; a low or backlit sun will show it off, and
  a dense pine seen side-on most. A picture is of a tree at one elevation, so between rows a
  tall trunk shows doubled (the crown is aligned, the trunk is not).
- Until 0.13 the lit shaders read screen derivatives and mipmapped textures after a `discard`
  (undefined in GLSL): NVIDIA gave convenient values, Mesa did not, and impostors were shaded
  differently per driver. Derivatives are now taken before any discard; the impostor, canopy
  and map-picture calibrations were measured again on that.
- Grass shadows (`castShadows`) are only as fine as the shadow map: a blade is about 1.7
  texels wide at `shadowExtent` 14 and a 4096 map, so under a low sun the soil seen between
  blades from 1.5 m up shows a blocky basket-weave of blade shadows in the nearest metres
  (the map's texels; it gets worse as `shadowDensity` falls below 1 and is gone at 1). A smaller
  `shadowExtent` or a bigger `shadowMapSize` sharpens it. Only directional lights' footprints
  are tested; a spot or point light draws every tile within `shadowDistance`. Grass has no ink
  lines unless you ask for them.
- Map forests use impostor pictures but no far canopy and no splats, and their impostors cast
  no shadow (a tree's shadow fades out as it becomes a picture). A picture is smoother than the
  faceted mesh and its crowns read as blotches (see Cities and maps); set `impostorBelow` and
  `mergeBelow` to 0 for meshes only. Time follows the number of bodies: 100,000 trees at 3.5
  globe radii still take 21-24 ms at 720p.
- Impostor shadows cost frame time (up to about 50% more on a large forest), and an impostor
  receives no shadow while they are on.
- The `flora` showcase is an engine demo, not a film: it has no brief, script or QC review.
