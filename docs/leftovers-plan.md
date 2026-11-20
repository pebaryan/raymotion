# Leftovers release plan

Status: 0.13. Approved 2026-11-19 with nothing cut; built in the suggested order. The owner set the order of the next releases on
2026-11-19:

1. the leftovers (this plan);
2. weather and water;
3. speaking characters (lip-sync facial animation from the voice-over) and character
   variation for dialog-based films;
4. audio and subtitle management.

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

## L5. Grass that shadows itself

- `Field::castShadows`: blades within the shadow extent are drawn into the shadow map, at
  a lower blade density there.
- Off by default, since it costs time.
- **Tests:**
  - Grass behind a tall tuft is darker on its side away from the sun.
  - Time per frame is measured and recorded.

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

## Order and size

L1 and L7 are small. L2, L3 and L6 share code, so they go together. L4 is the largest.
L5 is independent. L8 can be done at any point.

Suggested order: L1, L7, L8, L2, L3, L6, L4, L5.
