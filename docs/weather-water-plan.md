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
changing wind, and the fixed box of `fx::rain3d` (W3 replaces it). `mix` is continuous: no frame-
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

## W4. Wet ground and settling snow

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

## W8. Wakes, ripples and floating objects

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
