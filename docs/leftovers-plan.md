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

## L3. Impostor shadows

- In the shadow pass, an impostor faces the light instead of the camera.
- It uses the view nearest the light's direction.
- `impostorShadows` becomes true by default.
- **Test:** an impostor's shadow area must be within 15 % of the full tree's, from two sun
  heights.

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

## L7. 2D vegetation

- `flora2d::meadow` takes a `vg::Path` shape as well as a polygon.
- Wind reads time as `frameTime(t)`, so the engine's motion blur samples between frames.
  That gives per-blade motion blur with no new code path.
- **Tests:**
  - A path meadow's roots stay inside its shape, including its curves.
  - With motion blur, a gusting frame differs from the plain frame only among the tips.

## L8. C++ showcase for the 2D map helpers

`maps-2d`: an engine showcase, not a film, like `flora`. It shows:

- `geo::base` and `choropleth` with a colour bar;
- a `geo::Series` time-series choropleth;
- `route` and `along`;
- `Projection::fly`;
- the `geo-route` and `geo-flyto` prefabs.

Data comes from the bundled Natural Earth set, and anything else is labelled illustrative.

## L9. Bindings, examples, docs and release

- Script keys for L1–L7, landing with each item.
- `docs/flora.md` updated (its "What is not done" list shrinks).
- `docs/scene3d` notes for the cube skybox.
- README, then a release.

## Order and size

L1 and L7 are small. L2, L3 and L6 share code, so they go together. L4 is the largest.
L5 is independent. L8 can be done at any point.

Suggested order: L1, L7, L8, L2, L3, L6, L4, L5.
