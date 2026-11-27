# Weather and water release plan

Status: 0.14. Approved 2026-11-26 with all 12 items and fully volumetric clouds (W2); built in
the order below. Release order (set 2026-11-19, amended
2026-11-20): leftovers (0.13, released 2026-11-23), **weather and water (this plan)**, zooming
from space to street, speaking characters, audio and subtitles.

The space-to-street release that follows is also the showcase for this one: "like a sim from a
spaceship to a flight sim". So this plan must hold up from the ground, from an aircraft
and from orbit. That means clouds you can fly through and a sea that reaches the horizon.

## What exists already

- `g3d::Sky`: a gradient, the sun, stars and a **flat** cloud layer painted into the sky
  image. It lights the scene and gives fog its colour. Presets by hour, and `Sky::mix`.
- Fog: linear and exponential, height fog, fog that takes the sky's colour.
- `g3d::Water`: one **endless** plane, a sum of up to 8 Gerstner-like waves, Fresnel
  reflection of the **sky only**, glints, crest foam, an underwater view, absorption and
  caustics. `height`/`slope` on the CPU, so a boat can ride it.
- `fx::rain3d`/`snow3d`: **CPU** particles, at most 20 000, inside a fixed box.
- Wind: grass `Field` and flora trees each have their own wind settings.
- `globe::Weather`: cloud cover and cyclones on the globe; 2D `fx::rain`/`snow`.

What's missing for weather:
- nothing ties sky, fog, wind and rain into one weather state;
- clouds have no volume, so they can't be seen from above or flown through, and they cast no
  shadows;
- rain can't fill a valley, and it falls under roofs;
- surfaces don't get wet, and snow doesn't settle;
- there is no lightning.

What's missing for water:
- it reflects nothing in the scene;
- there are no lakes, rivers or pools with shores;
- nothing leaves a wake;
- the sea tiles visibly long before the horizon.

## Rules for every item

- Frames are pure functions of `t`. Rain, ripples, wakes, snow depth and lightning come from
  time and seeds, never from state carried between frames. (Snow depth is a function of
  time since the snow began, not an accumulated buffer.)
- One wind. Everything that moves in the wind reads the same field (W1).
- Tests check for the visible defect (a seam, a pop, rain under a roof, a reflection the wrong
  way up), not for averages. 0.13's lesson was that averaged metrics passed while the images
  showed swirls.
- Tests stay cheap under Mesa (the release Linux job already skips scene3d for time). New
  heavy tests go in their own executable with the `gl` label.
- Script bindings and docs ship with each item, not at the end.

## W1. One weather state

`g3d::Weather` on the scene drives everything else:
- cloud cover and type;
- precipitation (none/rain/snow/sleet) and its intensity;
- wind direction, speed, gust strength and gust speed;
- wetness and snow cover;
- haze;
- lightning rate.

It also has presets (`clear`, `overcast`, `rain`, `storm`, `snow`, `fog`) and `Weather::mix(a,
b, t)`, so a clear morning can turn into a storm across a shot.

- With `weather.enabled`, the grass `Field`, flora trees, water waves, rain slant and cloud
  drift take their wind from `Scene::weather`. Each has an `ownWind` flag to keep its own
  settings. With the weather off (the default), nothing changes.
- With the weather on, it also sets the sky's cloud cover and greyness, haze/fog and the sun
  light's strength under cloud. Each of these can be pinned the same way.
- **Test:** with one wind direction set on the weather, grass lean, tree sway, the wave
  heading and rain slant all point the same way, within 5°.

**Done 2026-11-26.** `g3d::Weather` (include/rmtn/weather.hpp) and `Scene::weather`, with the
presets, `mix` (the angle the short way round) and `windRun` for clouds under a changing wind.
The opt-outs are `ownWind` on Field, ForestSpec, Water and Emitter3D, `ownClouds`/`ownLight` on
Sky and `ownFog` on Scene. Waves under the weather are fetch-limited: `Water::fetch` and
JONSWAP give Hs = min(0.0016·U·√(F/g), 0.21·U²/g), the peak wavelength comes from the peak
period, and steepness is capped at 0.12, so a 17 m pond in a gale gets ~5 cm of chop and the open
sea metres. The water shader now band-limits each wave by the pixel footprint and moves the lost
slope variance into roughness (glints and Fresnel), so short waves no longer alias into a
crosshatch. `Water::filter` switches this off for comparison. Script `weather` object, example
`weather_mix.as`, docs/weather.md.

Tests (`weather_tests`, 320×180, about 1 s): one wind within 5° in every system at 30° and
200° (worst: grass 3.2°); off and all opt-outs pixel-identical; `mix` steps of range/200 and
no pop across 0.5 (checked against the step at 0.46); overcast dims and greys; purity;
fetch-limited heights from a pond to the open sea; no periodic peak in the autocorrelation
of grazing water (filter on 2.3, off 4.7), with the distant mean conserved against 4×
supersampling. Not driven yet: a tree's baked flutter amplitude, the gust phase under a
changing wind, and the fixed box of `fx::rain3d` (W3, `Scene::precipitation`, replaces it for large scenes). `mix` is continuous: no frame-
  to-frame jump larger than the rate of change. A preset change with no weather set changes
  no pixels (old scenes render unchanged).

## W2. Volumetric clouds

Raymarched clouds in a layer (base and top heights), shaped by type: cumulus, stratus,
cumulonimbus, and a cloud deck for flying above. Sunlit, with self-shadow, silver linings and
a dark base. They are drawn:

- from below, as part of the sky;
- from above, a sea of cloud with the ground showing through the gaps;
- from inside, where the view greys out and the cloud thins as the camera leaves it.

Clouds cast moving shadows on the ground. They drift with the W1 wind and grow or decay over
time by a seed. A quality setting trades steps for speed, and preview runs at low steps.

- The flat `Sky::clouds` layer stays as the cheap option.
- **Tests:**
  - From below, the covered fraction of sky matches `cover` within 0.08.
  - Seen from above at 3 000 m, ground shows through the gaps where cover is under 1.
  - Flying the camera up through the base gives no frame-to-frame jump in mean or per-cell
    brightness larger than a set bound: no pop at the layer edges.
  - A cloud's shadow on flat ground moves by drift × time.
  - The same `t` gives the same pixels.
  - A crop of the edge, enlarged, is checked for banding. Step noise must be blue-noise
    jittered, not rings.

**Done 2026-11-27 (cumulus, stratus, deck); the cumulonimbus look is open, to finish with W5.**
`g3d::Clouds` on the scene. Shaders are in src/engine/clouds_glsl.hpp.

- **Shell:** a raymarched shell round a 6 371 km planet.
- **Noise:** Perlin-Worley shape and Worley detail baked lazily into 2D atlases (raylib has no
  3D textures), with the two slices lerped.
- **Coverage:** a rank-equalised map read through a cubic B-spline. Eased bilinear left
  world-fixed straight lines along the texel centres, which was the seam.
- **Shape:** the 3D shape noise is remapped by coverage and given a per-type height profile, with
  a soft density toe and detail erosion that is stronger near the camera.
- **Lighting:** Beer and powder with multi-scatter octaves, a dual-lobe HG silver lining,
  ambient from the sky, and haze towards the horizon.
- **Noise and filtering:** blue-noise jitter spread over the whole coarse step, a per-step
  lattice offset, and a 5×5 depth/alpha bilateral. No temporal accumulation.
- **Weather and shadows:** cover, type and drift follow the weather. Ground shadows come from a
  per-frame transmittance map.
- **Cost:** Film about 13-15 ms at 1080p.

Tests (weather_tests, about 4 s):
- cover from below within 0.08;
- ground through the gaps from 3 km;
- no pop through the layer (frame-refinement ratio);
- shadow drift 60 m at 41° for 60 m at 40°;
- depth occlusion and purity; off means off;
- curvature from 10 km;
- isotropy (no world-fixed lines) and yaw-joint column excess (no screen-fixed seam);
- soft edges (a 27 px ramp);
- stipple at most 46 (clean 35);
- no banding against the High frame;
- near detail at least 0.7 of the far detail;
- cumulus rounded at the top, and the Cb anvil wider than its tower (1.89).

Open: the Cb shape is connected (tower and anvil) but reads as a symmetric mushroom cap rather
than a flat anvil spread downwind, and neighbouring towers look cloned. Finish it with W5's
storm. Clouds do not render with the camera above about 30 km; the space-to-street release needs
that.

## W3. Rain and snow at scale

GPU precipitation, built as the grass `Field` is: drops come from a hash in the vertex shader,
in a volume that follows the camera, so rain can fill a valley.

- Streaks are as long as the shutter (they match motion blur), slanted by the W1 wind, and lit
  by the scene's lights (a drop in a lamp's cone glints).
- **Occlusion:** a top-down depth map stops rain under roofs, trees and bridges.
- Splashes on surfaces where drops land.
- Snow drifts with turbulence and settles (W4).
- Intensity comes from the weather, from drizzle to a downpour (≥ 200 000 drops in view).
- `fx::rain3d`/`snow3d` stay as they are.
- **Tests:**
  - No drop is drawn inside a covered box (a roof over a porch) while the open ground beside
    it has rain.
  - Drop density in view scales with intensity.
  - Streak angle follows the wind.
  - Moving the camera does not change the rain's world positions: the volume follows the camera
    but drops stay put.
  - Determinism.

**Done 2026-11-26 (W4's settling snow and wet ground are still open).** `Scene::precipitation`
(`g3d::Downfall`): rain, snow and sleet from a hash in fixed world tiles, streaks of speed x shutter
slanted by the wind, lit by the scene's lights, a far layer for curtains, a top-down occlusion map
along the fall, and splashes. Script object `precipitation`; `examples/scripts/porch_rain.as`;
checks in tests/weather_tests.cpp. Left out: drops do not refract the background, lightning does not
light them, fields (grass) and particles do not shelter rain, and the GPU drop count is predicted
by the CPU mirror, not read back.


**Done 2026-11-26 (W4):** `Scene::ground`, wet surfaces with screen-space reflection, puddles and rain rings, snow cover, per-material `weathering`; see "Wet ground and snow cover" in docs/weather.md.



- **Wetness:**
  - Surfaces darken, roughness drops and reflections strengthen.
  - Puddles form where the ground is flat and low (a noise mask with a slope test), with rings
    from raindrops on them and on open water (procedural ripple normals from hashed
    impact times).
  - It dries from the top of slopes first.
- **Snow cover** builds on faces that point up, deepening with time since the snow began, and
  on trees and grass (flora tints by cover). Steep faces and the undersides of overhangs stay
  bare.
- Per material, `weathering` lets a surface opt out (glass, indoors).
- **Tests:**
  - A flat plane and a 60° slope under the same wetness: the flat one shows a puddle, and the
    slope doesn't.
  - Ripple rings are round and spaced by the impact rate.
  - Snow at t = 0 is zero and grows monotonically; the vertical faces of a cube stay bare while
    its top whitens.
  - Old scenes are unchanged with no weather set.

## W5. Lightning, storm light and rainbows

- **Lightning:**
  - Branching bolts, generated from a seed and drawn as emissive lines with glow.
  - A flash that lights the scene and the clouds from inside (W2).
  - Strike times come from the weather's lightning rate, or are given exactly.
- `thunderDelay(distance)` returns the delay for the audio track (the voice-over pipeline
  can place a thunder clip).
- **Rainbow:** when it rains with the sun behind the camera, a bow at 42° (and a faint
  secondary bow at 51°) opposite the sun.
- 2D: `fx::lightning` for 2D compositions.
- **Tests:**
  - The same seed gives the same bolt; a different seed, a different one.
  - The flash raises the scene's brightness only during the strike's frames.
  - The rainbow's ring is centred on the anti-solar point, at 42° ± 1°.
  - There is no rainbow when the sun is in front of the camera.

**Done (W5).** `rmtn/lightning.hpp` + `src/engine/lightning.cpp` (bolt, profile, strike list, rate, thunder cues, rainbow
frame), `g3d::Lightning` and the `Weather::Rainbow` on the scene, ribbons and the rainbow pass in
`src/engine/lightning_glsl.hpp`, the cloud's inner glow in the march, `fx::lightning`, the Cb as a few storm cells with a continuous anvil and per-cell variety
(`Clouds::anvil`, `storms`, `towerVariety`) and the storm sea (`Water::Spectrum::storm`); script keys `lightning`, `weather.rainbow`,
`clouds.anvil`, `clouds.storms`, `water.spectrum.storm` and the 2D `{"lightning"}` emitter; docs/weather.md; `thunderstorm.as` and
`lightning_2d.as`; checks in tests/weather_tests.cpp (`RMTN_W5_ONLY=1`). Left out: the supernumerary bows, bolts and the
rainbow in the water's mirrored reflection, a strike's light in shadows (the flash light casts none), thunder audio itself
(the cues are listed; the clip is the film's). Open: the 18 m/s storm sea still shows a few large
flat foam sheets (the foam map's patch scale), and the W9 artefact checks pass with thin margins
(Laplacian 3.97 of 4.0, foam-edge steps at the limit); storm towers read somewhat soft and grainy.

## W6. Water that reflects the scene

- A planar reflection pass mirrors the scene about the water level. The wave normals distort
  it, and it fades by Fresnel into the existing sky reflection.
- Objects under the surface are seen refracted from above, instead of only tinted.
- A quality setting picks the reflection pass's resolution, or turns it off (sky only, as today).
- **Tests:**
  - A red pole at the shore shows red directly below its base in the water, upside down: its
    top lies lower on screen than its base. With calm water the reflection is sharp; with waves
    it breaks up and keeps the same mean colour.
  - A white plate one metre down is visible from above and shifted by refraction in the
    expected direction.
  - Turning reflections off restores today's pixels.

**Done 2026-11-27.** `Water::reflections` (Off by default, or Scene, Half, Quarter),
`reflectionDistort` and `refraction` (off by default).

- **Reflection pass:** the scene is mirrored about the water level. Lights, shadows and world
  positions stay real, and fragments below the water are discarded. It includes the volumetric
  clouds, is culled with the mirrored frustum, and is skipped when no water is on screen.
- **Sampling:** the band-limited wave normal bends the reflection, blended by Fresnel into the
  sky reflection. The blur level follows both the footprint and the bend, which took out the
  ringed cloud contours on rough water.
- **Calm water stays a mirror:** the distortion fades in with significant wave height (1-10 cm),
  a documented perceptual choice.
- **Refraction:** a screen-space Snell march with absorption over the real path. A ray that
  leaves the picture keeps the edge colour, absorbed over its path, so the screen edge leaves no
  dark band. The opaque pass's straight-line tint is skipped when refraction is on; before that,
  absorption was applied twice.
- **Presets:** calm, choppy and storm now spread their waves within ±40° at irregular lengths,
  with the same Hs and peak length, so they no longer alias into a grid. **This changes the look
  of existing water scenes**, e.g. the Bundaran HI banner and voxel_splats showcases.
- **Cost at 1080p:** reflections roughly double the draw cost of the scene (1 200 draws: 16 → 27
  ms). Refraction costs about 0.6 ms.

Tests (weather_tests):
- off means off (0.005 levels);
- the pole upside down and in place (length ratio 0.96);
- waves break it up but keep its colour;
- no leak below the water;
- the refraction offset as predicted;
- clouds in the water (correlation 0.88);
- purity;
- no far shimmer;
- a calm lake is a mirror: a cone's reflection spans 0.93 of its height, flip correlation 0.94,
  cloud 0.75, and the same scene at 8 m/s breaks it;
- no refraction edge (row step 0.10);
- no rings (on/off 7.6% against 6.4%);
- preset spread within 45°.

Open: the screen-space march leaves a hatched band on the top rim of a submerged rock. Line art,
billboard particles and splats are not reflected.

## W7. Lakes, rivers and pools

`Water` bodies beyond the one endless sea:
- a polygon, or a mesh with a level (lake, pool);
- a path with a width and flow (river).

- **Shore:** foam and a soft edge where the water is shallow, from the depth to the terrain
  under it, read from the depth buffer.
- **River:** the surface scrolls along the flow with a flow-map method, so it never shows
  seams or stretches.
- `city::` and `geo3d::` water polygons become real water bodies when asked.
- **Tests:**
  - A pool in a box: water inside, none outside.
  - Shore foam appears only where depth < the foam width.
  - River ripples move downstream at the flow speed, with no visible reset seam over 10 s
    (compare frame pairs across the flow-map's phase switch).
  - A lake polygon from geo3d sits at its level.

**Done 2026-11-26.** `Scene::waterBodies` (`g3d::WaterBody`: lake, pool, river), soft shore and lace foam from the scene depth, two-phase flow map, `city::Look::realWater`, script `waterBodies`; see "Lakes, rivers and pools" in docs/weather.md. geo3d has no water polygons, so only city water converts.

## W8. Wakes, ripples and floating objects

**Done 2026-11-27.** `Scene::wakes` (`g3d::Wake`, the Kelvin V from a windowed sum of deep-water rings, foam, propwash, `wakeSpray`), `Scene::ripples` (`g3d::Ripple`, capillary-gravity rings, `splash`), `g3d::ride` and script `wakes`, `ripples`, `ride`; see "Wakes, ripples and floating objects" in docs/weather.md. Shading only (no mesh displacement), 4 wakes and 16 ripples.

- **Wakes:** a Kelvin wake (a 19.5° V) behind any object moving along a path on the water,
  computed from the path and `t`, not from a simulation. Plus bow spray.
- **Ripples:** from impacts at given times and places: a stone, a splash, a raindrop field.
- **`ride(object, water)`:** sets an object's height, pitch and roll on the waves each frame,
  from `height`/`slope`.
- Splash particles reuse `fx::Emitter3D`.
- **Tests:**
  - A boat moving at constant speed leaves a wake whose arms are 19.5° ± 2° from its track.
  - A ripple's ring radius grows with time and is round.
  - A floating box rises and falls with the surface height under it.
  - Determinism.

## W9. Ocean to the horizon

A sea that holds up from a beach and from 10 km up:
- a spectrum of waves (JONSWAP-shaped, driven by the W1 wind and fetch), in several cascades
  at different scales so no tiling shows;
- distance level of detail;
- a horizon that meets the sky (curvature from orbit hands over to the globe in the space-to-
  street release);
- sea spray and whitecaps that follow wind speed.

It stays a pure function of `t`.

- **Tests:**
  - The FFT of a 2 km × 2 km top-down render shows no tiling peak at the cascade periods.
  - Wave height scales with wind speed as the spectrum says.
  - `height` on the CPU matches the drawn surface within 2 cm at five points.
  - No visible seams between cascades in an enlarged crop.

**Done 2026-11-27.** `Water::spectrum` (src/engine/ocean.cpp, `ocean.hpp`), see "Ocean to the horizon" in docs/weather.md. Left open: breaking-wave shape, a flat-mirror reflection on the curved sea, foam that reads as lace at grazing view, and a storm look at 18 m/s that is not yet convincing.

## W10. Light in the air

- God rays: sun shafts through gaps in clouds and canopies and through windows, from the
  shadow map, marched in the fog (volumetric fog lit by the sun, with a quality setting).
- Aerial perspective: distant land fades blue by altitude.
- **Tests:**
  - With fog and a slit in an opaque wall, a bright shaft lies on the line from the slit
    along the sun's direction, and the fog off that line stays darker.
  - Off by default, so old scenes are unchanged.

## W11. Camera weather

- Filters for what weather does to a lens:
  - raindrops on the glass (refracting the image, running down as the rain gets heavier);
  - frost and condensation creeping from the edges;
  - snowflakes landing near the lens;
  - a windscreen-wiper sweep for cockpit and car shots.
- They are 2D layer filters, so they also work over 2D compositions.
- **Tests:**
  - Drops stay put on the lens and slide down at a deterministic speed.
  - The wiper clears the swept area in its frames.
  - Off means no pixel change.

## W12. Showcase, prefabs and documentation

- Prefabs: `weather`, `water body`, `rain`, `lightning`.
- Script bindings for W1-W11.
- `docs/weather.md` and `docs/water.md`.
- A demo film for the release, as `far-and-near` was for 0.13: a weather day over one place
  (dawn mist, cumulus building, a squall with rain and lightning crossing a lake, a rainbow,
  then a calm evening with reflections). Built to the authoring standard (brief, reveal and
  metaphor maps, QC). Not released.

## Order and orchestration

W1 first: everything reads it. Then W2 and W6 (the largest look changes), W3-W4, W7-W9, W5,
W10, W11, W12. Each item goes to a Sonnet agent (Haiku for bindings and docs) with a
self-contained brief. I review the diff, re-render the images myself, enlarge crops, and
require the tests to fail on the defect before committing. One agent at a time in the main
checkout.

Release only after asking. If the time budget runs short, these could move to a 0.14.x point
release (only after asking): W10, W11, and the cloud types beyond cumulus and the deck.
