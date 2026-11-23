# Leftovers release plan

Status: 0.13. Approved 2026-11-19 with nothing cut; built in the suggested order. The owner set the order of the next releases on
2026-11-19:

1. the leftovers (this plan);
2. weather and water;
3. zooming from space to street (added 2026-11-20). A smooth 3D camera flies from the globe
   down to a region and a city, through data at several scales and with buildings and terrain.
   It is also the showcase for weather and water, like a flight from a spaceship into a flight
   simulator;
4. speaking characters (lip-sync facial animation from the voice-over) and character
   variation for dialog-based films;
5. audio and subtitle management.

This plan gathers what earlier releases left open. Each item has its own test.

## L1. Cube-face skybox

Today `Scene::environment` takes one equirectangular image, but most skybox packs ship as
six faces.

- `Scene::environmentFaces`: six files in the order +x, -x, +y, -y, +z, -z, or a single
  horizontal-cross image.
- On load, the faces are converted on the CPU to an equirectangular image of the same
  detail. Image-based lighting, `showEnvironment`, `environmentRotation` and `fogFromSky`
  then work unchanged.
- Script: an `environment` object with `faces` or `cross`.
- **Test:** give each face a different solid colour. Looking along each axis must show
  that face's colour, edges between faces must have no seams, and lighting must match the
  same sky given as an equirectangular image.

**Done 2026-11-19.** `Scene::environmentFaces` (six files, or one cross) is converted on the CPU
into an equirectangular image, cached with the other textures. Sky packs are labelled for
left-handed engines, so the ±x faces swap sides, as three.js does; that keeps the sky
unmirrored and continuous. Script: `"environment": {"faces": [...]}` or `{"cross": ...}`.
Example: `cube_skybox.as`, with six 512 px faces under `assets/textures/skybox-example/`.

Tests (`scene3d_tests`, `cubeSkybox`):
- Each axis shows its face: exact colours.
- Mirroring: asymmetric markers land on the left and right as drawn on all four side faces,
  and the top and bottom faces meet +z.
- Seams: across 18 views, mean 0.12 levels and worst max 2 against the same sky as an
  equirectangular image. Lighting differs by a mean of 0.03.
- The cross equals the six files.
- HDR faces stay linear above 1.
- Errors name the problem.

`test_script.py`: `test_environment_faces`.

**Found:** `prepareTexture` repeats every texture in v. So straight up or down, an
equirectangular environment file or generated environment blends its top and bottom rows
(the cube path clamps v). Fix this in L9 with a test. The texture cache is shared with
material maps, so the clamp must apply only to environment use.

## L2. Impostors seen from above

Today impostors turn only about the vertical, so from above they read as side views.

- Bake views at three elevations (0°, 35° and 70°) as well as around the tree.
- Blend by azimuth and elevation, between the four nearest views.
- `TreeBuild::elevations` (default 3).
- **Test:** measure first how well an impostor's silhouette matches the full tree from 60°
  above. Then require the overlap there to rise to at least 0.75. The 0.80–0.82 overlap
  from the side must not drop.

**Done 2026-11-20.** `TreeBuild::elevations` (default 3; 1 is the old impostor exactly). Each
picture is centred on the crown's middle. Quads face the camera fully, and four pictures blend
by the screen door with a new `Instance::fadeFrom` band. Elevations blend over the middle half
between rows, because a wider blend ghosts.

The overlap from 60 degrees rose from 0.48 / 0.71 / 0.47 to 0.85 / 0.77 / 0.80 (oak / birch /
pine). From the side, it is unchanged.

Brightness had been 1.4–4.2 times the full tree's. There were two causes:
- Leaf pictures left clear texels black, so the full tree's leaf mipmaps darkened with
  distance. Clear texels now take the nearest colour, then the mean.
- The pictures lacked the crown's inner shade. A per-texel visibility term is now baked into
  the normal map's alpha (`Material::normalAlphaShade`).

The luminance ratio is now 0.85–1.17 across three species at 0, 30, 60 and 80 degrees, with a
test band of 0.80–1.25. The atlas is baked lazily at the first draw, so building a tree and
`raymotion list` don't pay for it.

## L3. Impostor shadows

- In the shadow pass, an impostor faces the light instead of the camera.
- It uses the view nearest the light's direction.
- `impostorShadows` becomes true by default.
- **Test:** an impostor's shadow area must be within 15 % of the full tree's, from two sun
  heights.

**Done 2026-11-20.** Each impostor gets a shadow-only copy (`Material::shadowOnly`). The copy
faces the first shadow-casting light and uses the picture nearest the light's direction. It is
only submitted within that light's `shadowExtent`.

Shadow area against the full tree, with the sun at 25 and 55 degrees:
- oak 1.00 / 0.99;
- birch 0.92 / 0.84;
- pine 1.13 / 0.95.

Centroids are within 2.5%. The test holds the oak to 15% and the others to 30%. The purity
test passes.

Limit: while it is on, a visible impostor doesn't receive shadows, since its own copy would
shade it. Cost: best-of frames for 10,884 trees at 1080p rose from 24–29 ms to 31–46 ms, so
the docs say to turn it off where far shadows don't show.

## L4. Far canopy

The tier past the impostors. A forest's ground is split into cells, and each cell with
trees gets a canopy patch: a height field just over the crowns, coloured from them, with
a noise-broken edge. The patches cross-fade in past `ForestSpec::canopyFrom`, and trees
beyond that distance stop being drawn.

- **Tests:**
  - Instances drawn at the far distance drop by at least 80 %.
  - At the switch-over distance, the picture changes by less than the impostor blend does
    today.
  - Frames stay pure.

**Done 2026-11-20, redone after two reviews.** `ForestSpec::canopyBelow` (px; hand-over where a cell's
trees are under that many pixels tall, about 7; 0 = off), `canopyFrom` (metres, floor), `canopyCell`
(128 m), `canopyDetail` (3 m), `canopyBlend` (half a cell), `canopyShadows` (false), `canopyShade`
(0 = by view and sun; > 0 fixed, for calibrating); script keys the same except `canopyShade`. Existing
films' far pictures don't change. A cell well inside the forest (it and its eight neighbours hold a
quarter of the median tree count) gets a height-field patch; boundary and clearing cells keep their
trees. Crowns are domes and cones with noise outlines and relief, nearly flat normals; colour half the
tree's crown colour and half the forest mean, a non-repeating grain picture; quads triangulated with a
hashed diagonal. A cell's trees and patch share one screen door by distance; each cell's hand-over is
jittered +-25% so the front is ragged. Patches are lit by `Instance::shade` from a measured table over
view elevation x sun elevation x angle between sun and camera (`CANOPY_SHADE`, calibrated by
`RMTN_L4_CALIBRATE=1`, times 1.08). Patches are built lazily (`raymotion list` unchanged within noise).
Round 3 (2026-11-20): the 30-degree look did NOT pass, so the canopy is a low-camera feature. Cause found by
ablation: no per-cell constant (colour, shade, height mean, interior set) explains the blotches; the rest is a smooth
mismatch between a patch and the trees it replaces that depends on view, sun and the sky's diffuse light (canopy/trees
luma 0.74-1.0 at sun 25 with a sky, 0.99-1.10 at sun 55; block means off by up to 25-40%), and a fitted smooth
model of the shade table leaves about 15% rms. `canopyMaxElevation` (default 20 degrees, 0 = no limit): when the
camera seen from the forest's middle is higher, every cell keeps its trees (checked: 59 px of 921,600 differ).
Below it, the low shots at sun 25 and 55 pass. The 30-degree study stays in `canopyTests` behind `RMTN_L4_HIGH=1`
(its checks fail by design). Images: `out/l4/v5`.

What the tests measure (`canopyTests`, `RMTN_CANOPY_ONLY=1`):
- Trees 5-8 px tall, 7 / 30 / 60 degrees, sun 25: linear luma canopy/trees 1.12 / 1.05 / 1.06;
  sun 55: 1.10 / 1.13 / 1.09 (bound 0.85-1.15). Grain x0.80-0.92 at sun 25, x0.68-0.81 at sun 55.
  Strongest repeat 0.13 / 0.03 / 0.04 (sun 25) against 0.08 / 0.04 / 0.10 for trees.
- Hand-over strip (f), 1280x720, 30 degrees up, sun 25, trees frame against canopy frame on the same
  pixels: 16-row windows 0.914-1.014, far half 0.954, biggest step between neighbouring windows 0.029
  (bounds 0.9-1.1, step < 0.06). With the old single gain the same check read 0.82-1.00, far half
  0.86, step 0.089, and failed.
- Seams, close: x0.96 / 1.01 against lines inside cells. Purity: 0 of 7,924 px differ.
- Far forest, 76,954 trees: bodies 145,094 (impostors) -> 16,053 (64 m cells, -89%) or 20,259 (128 m,
  -86%); triangles x4.35 of the impostors, x0.0031 of reduced trees.
- Switch-over, 2-4 m dolly: canopy excess -0.012 mean / worst cell 2.8; the impostor switch 34.6.
- Bench (39,245 trees, 720p, very noisy): trees 29-43 ms, canopy 25-47, shadows 27-42, 64 m 30-62.

Limits: in the 30-degree high shots a few cells still read darker (far left) or greener than the trees
around them, and the far rows are a touch dark; the table is for one rig and the fudge is empirical;
clearings smaller than a cell are filled in; no clear frame-time gain at 720p, only fewer bodies and
triangles. The jitter and the boundary-cell rule were not break-tested. A cell's trees and patch change
together. Patches may be missing in a reflection that sees cells the main camera does not.

## L5. Grass that shadows itself

- `Field::castShadows`: blades within the shadow extent are drawn into the shadow map, at
  a lower blade density there.
- Off by default, since it costs time.
- **Tests:**
  - Grass behind a tall tuft is darker on its side away from the sun.
  - Time per frame is measured and recorded.

**Done 2026-11-20.** `Field::castShadows` (on, as before) and `shadowDensity` (1); script keys the same.
Grass already cast shadows before this (every tile within `shadowDistance` of the camera, at the
main pass's density), so the defaults keep that: with them the shadow pass is the old path, no
pick and no widening. `castShadows` false stops the grass casting (and skips it in the shadow
passes). A tile now also has to be inside a directional light's `shadowExtent`. `shadowDensity`
below 1 is the new saving: a random share of the blades (a hash of the instance, not the first n:
the first n showed the placement sequence's lattice as a weave on the ground), widened 1/density
(at most 3x). Tiles kept only for the shadow they cast into view are no longer drawn in the main
pass. The same wind and t reach both passes. Tuft behind/sun side luma 0.55 on, 1.01 off; shadow
moved 0.57 m with blades 0.43 m (0.01 m with the shadow pass's wind frozen). 1.47 M blades at
1080p: 9.8 ms off, 10.5 ms on at 1, 9.9 ms at 0.35 (best of 12, noisy); shadow-pass blades
230,400 at 1 and 80,640 at 0.35.
Limits: below 1, the soil seen between blades from 1.5 m up shows a blocky weave of blade shadows
in the nearest metres under a low sun (shadow-map texels, 1.7 per blade at extent 14 / 4096;
absent at 1); no acne on the blades themselves in a blade-level close-up.

## L6. Map forests at a distance

`geo3d::forest` moves to an impostor level by the tree's distance on screen, reusing
L2–L3. Then the default 100,000 trees hold their frame rate when zoomed out.

- **Tests:**
  - Time per frame at 100,000 trees, before and after.
  - The upright check (every tree along the surface's up at morph 0, 0.5 and 1) still
    passes.

**Done 2026-11-20.** `ForestCover::impostorBelow` (10 px), `blend`, `mergeBelow` (12 px),
`pictureSpeckle` (0.35) and `dropBelow` (0); script keys the same. A tree under `impostorBelow`
px is one flora impostor picture, a quad aimed with the new `Instance::aim`/`aimUp` so it
stands along the surface's up (the elevation and direction are read in the tree's tilted
frame): it faces the camera, and from 55 to 75 degrees up lies flat along the surface, so
from straight above trees overlap by their heights alone. The default trees bake four rows
(0, 30, 60, 90 degrees; `elevations` of 4 or more reach 90) and no trunk above 45 degrees.
Trees under `mergeBelow` px are thinned by rank and enlarged to keep their area, the quad
centred at the unenlarged crown's middle (an enlargement that raised the quad made a radial
swirl) and the fade band a share of the kept trees (a fixed band left the 7-radii patch faint
and dotted). One picture a tree: the four-way blend left dark spokes in dense forests seen
from above. Impostors cast no shadow here. Surface points are cached per morph. Counters
`g3d::instanceBodiesSubmitted/TrianglesSubmitted` and `drawingViewportHeight()` are new.
100,000 trees at 1280x720: about 62-70 ms every tree a mesh, 7-8 ms at 7 radii and 21-24 ms
at 3.5 (CPU-bound, on a loaded machine; triangles fall 230 times, bodies 19 times at 7 radii).
From 1.2 radii and closer (18 px and more) the default forest is meshes. Seen from above and
from 60 degrees, 3.5 to 1.5 radii, the tests measure orange and brown pixels (0 against 0),
luminance rings round the point below the camera, the darkest box, the radial structure of the
strokes (0.005-0.030 against 0.005-0.037 for meshes), blotchiness and grain.
Limits: a picture is smoother than the faceted mesh (3x3 contrast 0.58-0.7 of the mesh's at
3.5-2.0 radii, 0.89 at 7) and its crowns read as blotches (8 px cell spread 1.4-1.7 of the
mesh's); `pictureSpeckle` trades one for the other. The 7-radii patch's silhouette is a few
pixels larger and lumpier than the meshes' (the merged quads are 4 times as big).
For a film where that texture change shows in a zoom, set `impostorBelow` and `mergeBelow` to
0: every tree is then a mesh, as before.

## L7. 2D vegetation

- `flora2d::meadow` takes a `vg::Path` shape as well as a polygon.
- Wind reads time as `frameTime(t)`, so the engine's motion blur samples between frames.
  That gives per-blade motion blur with no new code path.
- **Tests:**
  - A path meadow's roots stay inside its shape, including its curves.
  - With motion blur, a gusting frame differs from the plain frame only among the tips.

**Done 2026-11-20.** `flora2d::meadow` takes a `vg::Path` with a `vg::FillRule`, and so
does the script meadow item (`"path"` with SVG data, `"rule"`). Paths are flattened to
0.1 px, and edges are bucketed by y. `flora2d::meadowRoots` returns the roots without
drawing them. `bendAt` reads `frameTime(t)`, so `motionBlur` blurs the swaying tips. On
whole frames the output is unchanged. Example: `flora_2d_path.as`.

Tests:
- `flora_tests`:
  - Path meadow: 21,339 roots, worst 0.046 px outside the curved edge, none in the hole.
  - The polygon meadow gives the same roots and sums as before.
  - `bendAt` on whole frames matches the formula exactly.
- `flora2d_render_tests`:
  - With blur, 4,892 px change in the tip band, 0 at the roots and 0 in the ground.
  - A still wind renders the same with blur on and off.
- `test_script.py`, `test_flora_2d_path_meadow`: 572 px inside, 0 outside, 0 in the hole;
  a bad path, rule or key is refused.

## L8. C++ showcase for the 2D map helpers

`maps-2d`: an engine showcase, not a film, like `flora`. It shows:

- `geo::base` and `choropleth` with a colour bar;
- a `geo::Series` time-series choropleth;
- `route` and `along`;
- `Projection::fly`;
- the `geo-route` and `geo-flyto` prefabs.

Data comes from the bundled Natural Earth set, and anything else is labelled illustrative.

**Done 2026-11-20.** `maps-2d` (`examples/showcase/maps_2d.cpp`), 32 s, five chapters:
1. GDP per person from Natural Earth's `gdp_md / pop`, with a colour bar marked "60,000
   and over".
2. A `geo::Series` over eight Southeast Asian countries, labelled illustrative and
   clipped to a panel with `isolate().mask`.
3. Jakarta–Dubai–Vienna–New York along great circles, with the distance from
   `geo::length` (17,611 km).
4. A globe flying to Kenya, which is then highlighted and named.
5. The `geo-route` and `geo-flyto` prefabs.

There is no audio. The map helpers don't clip to a box, so side-by-side maps need a mask.

## L9. Bindings, examples, docs and release

- Script keys for L1–L7, landing with each item.
- `docs/flora.md` updated (its "What is not done" list shrinks).
- `docs/scene3d` notes for the cube skybox.
- README, then a release.

**Done 2026-11-20 (everything but the release).**
- **Pole fix.** `prepareTexture` still repeats; environments use `prepareEnvironmentTexture`
  (repeat in u, clamp in v). A file used as an environment has its own cache key (`env:path`), so
  the same file as a material map still repeats. The generated environment, the cube path and the
  procedural sky target use it too. Test `environmentPoles` (`RMTN_POLES_ONLY=1`): the worst
  distance from the top or bottom colour over the whole image looking straight up or down was 53
  levels before the fix and is 0 after, for a file and a generator; a material quad with u 0..2
  reads u 1.3 as 0.3 (same colour) and 1.7 as 0.7.
- **Docs.** `docs/flora.md`: "What is not done" rewritten against the code (canopy for low
  cameras only, map forests do have pictures, impostor shadows' cost); a misplaced line in the
  canopy section fixed; the impostor brightness range corrected to 0.85-1.17. Cube skybox and
  pole notes in `skills/raymotion/references/3d.md` (there is no `docs/scene3d*.md`; scene3d is
  documented in the README and that reference). README: map forest timings, 2D path meadow and
  motion blur.
- **Examples.** `test_script.py` now uses `canopyBelow`, `canopyMaxElevation`, `elevations` (and
  its error), `impostorShadows`, grass `castShadows` and `shadowDensity`, and the map forest's
  `pictureSpeckle`. The cube-skybox and path-meadow keys already had examples and tests.
- **Release.** Notes are written on the GitHub release (generated notes are only the changelog
  link; earlier releases carry a hand-written body). The version lives in
  `skills/raymotion/templates/CMakeLists.txt` (`GIT_TAG v0.12.0`); the root `CMakeLists.txt` has no
  version. Not bumped.

## Order and size

L1 and L7 are small. L2, L3 and L6 share code, so they go together. L4 is the largest.
L5 is independent. L8 can be done at any point.

Suggested order: L1, L7, L8, L2, L3, L6, L4, L5.
