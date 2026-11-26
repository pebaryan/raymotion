# Weather

`g3d::Weather` (include/rmtn/weather.hpp, `Scene::weather`) is one weather state for a 3D scene:
a wind, cloud cover, haze and a few things later releases will draw. While `weather.enabled` is
true, the scene's grass, trees, water, rain and snow, sky and fog read it, so they all lean, roll,
slant and grey the same way. With `enabled` false (the default) nothing reads it and no pixel
changes: old scenes render as they did.

```cpp
scene->weather = g3d::Weather::storm();                 // everything follows a gale
scene->weather.wind.direction = 40;                      // ...blowing towards 40 degrees

scene->up = [](int t, g3d::Scene& s) {                   // a clear morning turns into a storm
    s.weather = g3d::Weather::mix(g3d::Weather::clear(), g3d::Weather::storm(), t / 300.f);
};
```

The wiring is pure. The values each system needs are worked out when it is drawn, from
`Scene::weather`, and put on a copy; your `Field`, `ForestSpec`, `Water`, `Sky`, emitter and light
are never changed, so a weather animated in `Scene::up` renders every frame on its own (frame 61
rendered alone equals frame 61 after other frames, pixel for pixel). The fog, the sun and the
ambient light are the one place a scene's own members are touched: `render` sets them for the
frame and puts them back when it ends.

## The state

| Field | Unit | Meaning |
| --- | --- | --- |
| `enabled` | | false: ignored |
| `fps` | frames / s | the film's rate, to turn m/s into per frame (as `Field::fps`) |
| `wind.direction` | degrees about +y | the way the wind blows **towards**: 0 = +x, 90 = +z (as `Water::Wave::direction`) |
| `wind.speed` | m/s | steady speed: 3 a light breeze, 8 fresh, 20 a gale |
| `wind.gust` | 0..1 | how hard gust fronts push on top of the steady wind |
| `wind.gustSpeed` | m/s | how fast the gust fronts travel downwind |
| `wind.gustScale` | m | distance between gust fronts |
| `windRun` | m | metres the wind has carried the clouds since frame 0; below 0 (default) means speed x time |
| `clouds.cover` | 0..1 | share of the sky under cloud |
| `clouds.type` | enum | `Cumulus`, `Stratus`, `Cumulonimbus`, `Deck`: read by the volumetric clouds (`Scene::clouds`) |. A `Cumulonimbus` raises the layer top to at least 12 km (a tower with a connected anvil that shears downwind with the wind; each is one shape round its own centre); cumulus edges and the weather map are smoothed with a cubic B-spline so no flat plateaus show, and the nearest clouds get detail contrast so they are not a blur
| `precipitation.kind` | enum | `None`, `Rain`, `Snow`, `Sleet`: for the rain and snow system, not drawn yet |
| `precipitation.intensity` | 0..1 | drizzle to downpour (not drawn yet) |
| `wetness` | 0..1 | how wet the ground is (not drawn yet) |
| `snowCover` | 0..1 | settled snow (not drawn yet) |
| `haze` | 0..1 | 0 clear air, 1 thick fog; sets the fog |
| `lightningRate` | strikes / minute | for lightning (not drawn yet) |

The wind direction is a heading (degrees) rather than an (x, z) vector, so that `mix` can turn it by
angle. `Weather::windVector()` gives the unit vector.

## Presets

All have `enabled` true and a wind towards 17 degrees.

| Preset | wind m/s | gust | gust m/s | cover | clouds | precipitation | wetness | snow | haze | strikes / min |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `clear()` | 3 | 0.2 | 4 | 0.15 | cumulus | none | 0 | 0 | 0.05 | 0 |
| `overcast()` | 5 | 0.25 | 5 | 0.90 | stratus | none | 0 | 0 | 0.15 | 0 |
| `rain()` | 7 | 0.4 | 7 | 0.95 | stratus | rain 0.5 | 0.8 | 0 | 0.25 | 0 |
| `storm()` | 20 | 0.8 | 14 | 1.00 | cumulonimbus | rain 1.0 | 1 | 0 | 0.30 | 6 |
| `snow()` | 4 | 0.2 | 4 | 0.90 | stratus | snow 0.5 | 0 | 0.6 | 0.35 | 0 |
| `fog()` | 1 | 0.05 | 2 | 0.60 | deck | none | 0.3 | 0 | 0.90 | 0 |

`Weather::mix(a, b, t)` interpolates every number. The wind direction goes by angle, the short
way round (350 to 10 degrees passes through 0, and a 0 to 170 degree turn is an even 0.85 degrees
a step over 200 steps). The kinds and types (cloud type, precipitation kind) switch at t = 0.5;
nothing draws them yet, so there is no pixel jump. `enabled` is true when either is.
When the wind changes over a shot, the clouds would swim if their position were speed x t, so give
`windRun` the integral of the speed (a function of t); `mix` interpolates it when both have one.
Gust fronts and flutter keep the plain speed x t clock of `Field` and `ForestSpec`, so they
accelerate with a rising wind.

## What each system reads

Speed `s` in m/s, `d` the unit vector (x, z) of the direction, `c` the cover.

| System | When weather is enabled | Opt-out |
| --- | --- | --- |
| `g3d::Field` (grass) | `windDirection` = `d` (turned into the field's object space), `sway` = 0.015 s (at most 0.6), `gust` = 0.7 x `wind.gust`, `gustSpeed` and `gustScale` as given | `Field::ownWind` |
| `flora::ForestSpec` (forests and growing trees) | `windDirection` = `d`, `wind` = s / 8 (at least 0.05, so the loop stays alive), `gust` = 1.2 x `wind.gust`, `gustSpeed` and `gustScale` as given (`flora::inWeather`) | `ForestSpec::ownWind` |
| `g3d::Water` | every wave's direction turned by one angle so that the amplitude-weighted mean heading is the wind's (the spread is kept); the waves are scaled uniformly so their significant height is Hs = min(0.0016 U sqrt(F / g), 0.21 U^2 / g) and the longest wavelength is g Tp^2 / 2 pi with Tp = min(1 / (3.5 (g / U) (gF / U^2)^-0.33), 7.16 U / g) (JONSWAP, capped at the fully developed sea; U the wind speed, F `Water::fetch`, g 9.81); each wave then clamped to steepness 2a / L <= 0.12 | `Water::ownWind` |
| `fx::Emitter3D` (`rain3d`, `snow3d`, anything with `windFollow` > 0) | a steady sideways velocity of `windFollow` x s / `fps` along `d` (`Emitter3D::drift`); the emitter's own sideways direction is dropped, so the wind sets the slant. `rain3d` follows 0.5 of the wind, `snow3d` 0.3 | `Emitter3D::ownWind` |
| `g3d::Sky` clouds | `clouds` = c; zenith, horizon, haze and cloud colours go to grey by 0.85 c^1.5; the sun's disc (size x (1 - c)^2), glow and stars fade; clouds drift downwind by 0.0012 sky-image units a metre (`Sky::cloudShift`; `cloudDrift` is replaced) | `Sky::ownClouds` |
| Light under cloud | `Sky::lighting` x (1 - 0.35 c); with `Sky::sunLight` set, that light's intensity x (1 - 0.75 c) and `Scene::ambient` x (1 + 0.35 c) | `Sky::ownLight` |
| Fog | exponential, density 0.065 x haze^2 per metre (haze 0.3: about 0.7 km visibility), colour from the sky (or `fogColor` with no sky or environment); `fogHeight` and `fogFalloff` are kept | `Scene::ownFog` |

Waves are limited by **fetch**, the metres of open water upwind (`Water::fetch`, script key `fetch`; default 50 km, `calm()` 2 km a lagoon, `choppy()` 10 km a bay, `storm()` 200 km open sea). At 20 m/s: a 20 m pond gets Hs 0.046 m with a 0.33 m longest wavelength (a fine ruffle), 1 km 0.32 m, 10 km 1.0 m, 200 km 4.6 m (144 m long), and the fully developed cap 8.6 m is reached only beyond about 700 km. Set `fetch` to the size of your pond or lake.

A few of these in plain terms:

* The grass sway, the trees' lean, the waves' heading, the rain's slant and the clouds' drift all
  point the same way: downwind. Gust fronts cross the grass and the trees together.
* A calm (`wind.speed` 0) holds the grass still, flattens the water to 5% of its amplitude and
  lets rain fall straight.
* The default `Sky::cloudDrift` (0.0004 a frame at 30 fps) is what a 10 m/s wind gives.
* Haze 0 is air with no fog at all; the weather's fog replaces a scene's own fog unless `ownFog`.

### What cannot follow the weather

* **Tree flutter and baked sway.** A tree's leaves flutter and its trunk bends in a loop baked
  into its meshes when it is made (`TreeBuild::frames`, `TreeSpec::sway`). Direction, strength and
  gusts act on top of the loop at draw time, as the lean (pitch and bank of each instance, or of a
  growing tree's tilt node) and as how fast the loop runs; the loop's own amplitude cannot change.
* **Gust fronts under a changing wind.** Their phase is gust speed x t in the shader, as for a
  `Field` whose `gustSpeed` is animated. Only the clouds have `windRun`.
* **Particles in the box.** `rain3d` and `snow3d` fill a fixed box (`Scene::precipitation` replaces them for large scenes); a strong wind carries the drops
  sideways out of it. Give the emitter a wider area than the view.
* **Objects that move in the wind on their own** (cloth and rope in `physics::`, 2D `flora2d::`
  and `fx::rain`) have their own wind settings and are not driven.
* **The CPU `Water::height` of `scene.water`** is the water you wrote. Use `g3d::waterOf(scene)`
  for the surface that is drawn (a boat riding the waves).

## Volumetric clouds

`Scene::clouds` (`g3d::Clouds`, off by default) draws a layer of clouds that can be flown through: a
raymarched shell between `base` and `top` metres above the ground under the origin, round a planet of
6 371 km radius, so the layer curves away (from 10 km up its far edge is 3 degrees below level; a flat layer
would hold at the horizon). `cover` is the share of the sky under cloud looking up (0.3 gives about 30% of
the pixels, measured in `weather_tests`), `type` is the W1 `CloudType` (a profile of height: cumulus puffs
with flat bases, a stratus sheet by the base, cumulonimbus towers with an anvil, a flat-topped deck),
`density`, `detail`, `seed`, `evolution` (the shape slides through the cloud at that many m/s, so it slowly
changes; the drift is separate), `haze` (aerial perspective), `quality` and `resolution`.

How it is drawn:

* The noise is baked on the first cloud frame (a few tens of ms, once per process; nothing at prepare
  time): a tileable Perlin-Worley 128^3 and a Worley 32^3, held as 2D atlases (raylib has no 3D
  textures), with the trilinear filter done in the shader; and a 256^2 coverage map in which cover `c` puts
  cloud on exactly share `c` of the map.
* The march starts at the camera when it is in the layer, stops at the opaque depth (and at the water's
  plane), and takes coarse steps that grow with distance and fine ones (about half an optical depth) once
  it finds cloud. The entry is refined by bisection so steps do not show as rings; each pixel's start
  is jittered by interleaved gradient noise, a function of the pixel only (no temporal accumulation, so
  a frame is still a pure function of `t`), and a depth-aware cubic upsample brings the low-resolution
  march (1/4, 1/2 or 3/4 of the frame) to full size without blurring across silhouettes.
* Light: a light march towards the sun (4, 6 or 8 steps), Beer's law with three octaves of multiple
  scattering, the powder darkening, a dual-lobe Henyey-Greenstein phase (the silver lining), and an
  ambient from the sky's zenith on the tops and the ground and haze under them. The sun's colour comes
  from the Sky (so dusk is orange). Distant clouds fade into the horizon colour.
* They are composited after the opaque objects and the sky, so a box in front of a cloud stays in front
  and a hill that reaches into the layer is fogged by it. Transparent objects and water behind a cloud
  are hidden by it; transparent objects in front of one are covered by it (a known limitation). They are
  not drawn for orthographic cameras or a camera under the water.
* With clouds on, the Sky's flat cloud layer is off. The image-based lighting (the Sky's cubemap) stays
  free of clouds: the sky light does not dim under them; the sun's light does, through the shadow map.
* **Ground shadows.** Each frame a map of the sun's transmittance through the layer on the plane y = 0 is
  rendered over `shadowExtent` metres round the view (128, 256 or 384 pixels square by quality), and the
  lit, grass, tree and water shaders multiply the sun's light by it (floored at 0.1; the light is the
  Sky's `sunLight`, or the first directional light within 14 degrees of the sun). A fragment above the
  ground is carried along the sun's rays to that plane first, so a hill is shadowed where its ray crosses a
  cloud. The shadows move with the drift.
* **Weather.** With `Scene::weather` enabled the cover and type come from it and the drift from its wind
  (`Clouds::travel`, `Weather::cloudTravel`, so a changing wind does not make them swim); `ownWeather`
  keeps the clouds' own. This is not `Sky::ownClouds`: a layer of volumetric clouds needs no Sky.
* **Lightning hook.** `Clouds::flash` (position, colour, intensity, radius) adds a point light inside the
  march. Nothing drives it yet.

| Quality | march steps | light steps | shadow map | default resolution |
| --- | --- | --- | --- | --- |
| Preview | 28 | 4 | 128 | 1/2 |
| Film | 96 | 6 | 256 | 1 (full frame) |
| High | 160 | 10 | 384 | 1 (full frame) |

At 1920 x 1080 on an RTX 5060 Ti the clouds cost about 4 ms at Preview and 11 ms at Film and High
(measured with the test scenes; `RMTN_CLOUD_TIMING=1 build/raymotion_weather_tests.exe` prints them). A
slower GPU costs proportionally more; the march is the fill-rate work, so `resolution` scales it.

`examples/scripts/cloud_flight.as` flies under cumulus over hills, up through the base and out over a
sea of cloud with gaps.

## Water that reflects the scene

`Water::reflections` (`Off`, `Scene`, `Half`, `Quarter`), `reflectionDistort` and `refraction`
(include/rmtn/scene3d.hpp; script keys `reflections`, `reflectionDistort`, `refraction` on `water`).

**Default: Off, and Off means Off.** With `reflections` Off and `refraction` false the pixels equal the
previous release's (the test compares a busy water scene with a recording made before W6: the worst cell
mean is off by 0.005 levels). Turning the default on would have changed every old water scene and cost
about 60% more per frame, so it is a deliberate choice you make per scene. `Scene` is the setting to
use for a finished shot; `Half` and `Quarter` trade sharpness for time.

* The pass mirrors the camera about `level` and draws the scene again into a target (full, half or
  quarter of the frame). Lights, shadows and world positions stay real; only the view is mirrored, and
  anything under the water is clipped. The mirrored view is culled against its own frustum, and the pass
  is skipped when no water is on screen. The sky box is not drawn into it (the analytic sky fills where
  nothing was drawn, so the sky and the clouds reflect); line art, billboard particles and splats are not
  in the reflection.
* The shader samples it at the fragment's own screen position bent by the band-limited normal (waves
  too fine for the pixel stop bending it and widen the blur instead, so it does not shimmer in the
  distance), blends it with the sky by Fresnel and blurs it by the water's roughness through a mip chain.
  `reflectionDistort` scales the bend (0 is a flat mirror, 1 the real slope).
* `refraction` copies the opaque colour and depth before the water is drawn and marches the Snell ray
  through them: the bed is found where the refracted ray meets it, shifted as it would be, and the
  absorption is applied over that real path, so a shallow plate is clearer than a deep one.
* How far the waves bend the reflection also follows how tall the sea is: `reflectionDistort` is scaled by
  smoothstep(1 cm, 10 cm) of the significant wave height Hs = 4 sqrt(sum a^2 / 2). A 1 m/s wind over 900 m
  (Hs 1.5 cm) leaves a mirror, 3 m/s and up breaks it. This is a deliberate choice, not optics: the weather
  scales the calm preset's short waves to very short wavelengths with slopes of about 0.1, which at a grazing
  camera would smear a far shore to half its height; a calm lake must not. Raise `reflectionDistort` to break
  it earlier. (Before the scaling, the 1 m/s lake reflected the far cones 0.53 of their height, correlation 0.55
  with the flipped cone, and no cloud; now 0.93, 0.94 and a cloud correlation of 0.75.)
* A refracted ray that leaves the picture before it meets anything goes on with the colour at the edge it crossed,
  absorbed over the length down to the depth seen there, so the bottom of the frame has no edge where rays that hit
  meet rays that left (the old fallback was deep colour: a ragged dark band that followed the waves; the sand-floor
  test measures a worst row step of 18.8 levels with it, 0.10 now). What is under the water is absorbed once, by the
  surface that looks through it; the opaque pass skips its own tint when refraction is on (applied twice, the floor was 16
  levels too dark).
* The reflection is blurred by how far the bend throws the sample (a quarter of it in texels, and the footprint of the
  bend on the target), so a heavy chop smears a reflected cloud edge into a glow instead of thin closed rings.
* The `calm()`, `choppy()` and `storm()` presets now keep every wave within 40 degrees of one heading (calm 20,
  choppy 15, storm 10) at lengths that are not simple ratios of each other; before, calm's eight waves were 140 degrees
  either side of its heading and the near water at a grazing camera read as a grid of dots. The Hs and the longest
  length of each preset are exactly what they were, so the fetch-limited scaling gives the same sea height. The look of
  every old scene that uses a preset changes (a busy choppy test scene moved by 68 levels in its worst 53 x 45 pixel
  cell mean); a scene that needs the old waves can list them in `Water::waves`.

Cost at 1920 x 1080 on an RTX 5060 Ti (best and mean of 10 frames; `RMTN_W6_TIMING=1
build/raymotion_weather_tests.exe` prints them):

| Scene (best / mean of 10, ms) | Off | Quarter | Half | Full | Refraction only | Full + refraction |
|---|---|---|---|---|---|---|
| 40 draws, no clouds | 5.0 / 5.2 | 5.4 / 5.9 | 5.4 / 5.8 | 5.4 / 5.7 | 5.4 / 5.5 | 5.9 / 6.3 |
| 600 trees (1 200 draws), no clouds | 15.6 / 16.9 | 26.9 / 29.0 | 27.3 / 29.4 | 27.1 / 30.0 | 16.2 / 16.8 | 27.0 / 29.9 |
| 600 trees and Film clouds | 24.4 / 25.7 | 39.0 / 41.6 | 40.6 / 43.2 | 39.1 / 41.2 | 24.6 / 25.8 | 39.9 / 41.9 |

The reflection costs a second walk over the scene, so it scales with the number of draws, not with the
target's size: Quarter is no cheaper than Full here because the frame is limited by submitting the
1 200 draws (a fill-bound scene would gain from Half and Quarter; none of these is). Refraction alone costs
about 0.3 ms. Culling against the mirrored frustum and skipping the pass when no water shows are why a
small scene pays under 1 ms.

`examples/scripts/lake_reflections.as` is a calm lake at dusk that breaks up as the wind rises.

## Rain and snow at scale

`Scene::precipitation` (`g3d::Downfall`, include/rmtn/scene3d.hpp) draws rain, snow or sleet on the
GPU the way the grass `Field` is drawn: nothing is stored. Space is cut into fixed world tiles
(5 m wide, 10 m tall; 30 m and 60 m for the far layer). A drop is a hash of its tile and its index:
a place on the tile's base, a size and a speed in the ranges below, and a phase. It falls from there
and wraps every `cell / speed` seconds, so a frame is a pure function of the frame number and the
camera. Only tiles within `radius` of the camera are drawn, but a drop's place does not depend on
where the camera is: move the camera 0.37 m and every drop stays where it was.

* **Kind, intensity, wind.** Taken from `Scene::weather` unless `ownWeather` (kind and intensity) or
  `ownWind` (direction and speed) is set. Rain follows the wind by 0.5 and snow by 0.3, as
  `fx::Emitter3D` does (`Weather::kRainFollow`, `kSnowFollow`); the slant is that share of the wind
  divided by the mean fall speed (4 m/s of wind slants rain by 14.9 degrees). Sleet uses the rain slant.
* **Rain** is a streak as long as speed x `exposure` (1/60 s, so it matches a 180 degree shutter),
  slanted by the wind, 0.5-4 mm wide before the pixel minimum, with a hashed spread of brightness. Each streak is lit by the
  ambient light and by every light in the scene: a forward lobe (a drop between the camera and a lamp glints) on an
  isotropic share, so a lamp behind the camera lights the drops too. A lamp's share falls as 1 / (1 + d^2) on top of its
  range, and thickens the streak, so a lamp in rain has a bright cloud of streaks round it. Streaks are fogged.
* **Snow** is a soft flake, 10-30 mm, falling at 0.5-1.5 m/s with a sideways flutter (`flutter`, in metres). It is lit, not
  glowing: albedo 0.9 under the sky it is seen against (lifted 1.7x because the baked sky is dimmer than the sky drawn; a quarter of
  its tint is kept) plus each light by a
  Lambert-sphere phase and a little forward translucency. Flakes nearer than 2.5 m are larger, softer and fainter (out of focus). A flake under a pixel keeps its coverage, not its size, and thinned far flakes get that coverage back (divided by the keep share), so distant snow is a veil.
  **Sleet** draws both.
* **Density.** `rainDensity` (90) and `snowDensity` (150) are the drops per cubic metre at
  intensity 1. A tile keeps its first `intensity x keep(distance)` share of drops by rank, so a lower
  intensity removes drops without moving the rest. `keep` falls as `(0.35 radius / d)^1.5` beyond 35%
  of the radius, which keeps the count per pixel about even. The far layer (`farLayer`, from `radius`
  to `farRadius`, `farDensity` 0.15 of the near density, longer and thinner streaks) gives curtains
  in a valley.
* **Occlusion.** A depth map of the opaque scene is rendered every frame from above, along the line of
  fall (so slanted by the wind), 1024 texels wide round the camera (`occlusionSize`). A drop that is
  deeper than the stored depth is dropped. A porch roof keeps what is under it dry, the dry strip shifts
  with the wind, and trees and bridges shelter what is below them. It needs the objects near the camera
  to be kept in the draw list even outside the frustum; the radius is `radius + 6 m`. It does not see
  fields (grass) or particles.
* **Splashes** (rain only, `splashes`, within `splashRadius` of the camera, measured to where the drop lands): each drop whose
  landing place is on a surface (found from the same depth map, or the water level) gets, for 0.2 s from the moment it lands, a
  thin faint expanding ring and five droplets thrown up and out on ballistic arcs. Each droplet is a tiny lit streak along its
  own velocity (the drops' shutter rule), at most 50% opaque; under a pixel wide it keeps its coverage as less alpha. The ring
  is hidden when the surface is above the eye (the top of a roof is not seen), and the droplets are dimmed there. They are pure
  functions of the same hash. Snow has none. Settling snow and wet ground are W4.
* **Cost.** At 1080p, on a porch-sized scene (a dozen draws), intensity 1: no rain 4.4 ms;
  near drops 5.3 ms (1.5 M drops submitted, about 245 000 of them in view after the thinning);
  with the occlusion map 5.4 ms; with the far layer 5.7 ms (2.4 M submitted); with splashes 6.1 ms
  (575 000 splash quads). Snow with everything on: 5.3 ms. Best of 12 frames, `RMTN_W3_TIMING=1
  build/raymotion_weather_tests`. The drop count in view is what the CPU mirror
  (`Downfall::drop`) predicts for that camera; the GPU's own count was not read back.
* **Limits.** Streaks are billboards, not refracting lenses, so drops do not distort the background.
  Lightning does not light drops yet. Rain does not enter the water's reflection pass.

`fx::rain3d` and `fx::snow3d` are unchanged and still fill a fixed box. Use `precipitation` for rain
that has to fill a valley or follow a camera; `examples/scripts/porch_rain.as` is the reference.

## Script

The scene descriptor's `clouds` object (docs/scripting.md) takes the `Clouds` fields by name. Its `weather` object takes a `preset`, optionally `to` and
`amount` to mix towards a second preset, and per-field overrides; `ownWind`, `ownClouds`, `ownLight`
and `ownFog` are keys on the objects above. `examples/scripts/weather_mix.as` is a meadow with
grass, trees, a pond and rain, mixed from clear into a storm across the clip.

## Tests

`tests/weather_tests.cpp` (the `weather` ctest, label `gl`) measures, at 320 x 180:

* one wind towards 30 and then 200 degrees: the grass lean (centroid of the bright tips from a
  top-down view, half the difference between this wind and the opposite one), the tilt a growing tree
  and each forest instance is drawn with, the wave heading (the axis of the picture's gradients,
  and the sign from the CPU surface), the cloud drift (the shift between two sky frames) and the
  rain's streak (the velocity of the particles the draw makes) all within 5 degrees of the wind;
  `ownWind` and `ownClouds` keep each system's own direction;
* off means off: a scene with grass, trees, water, rain and a sky is pixel-identical with the weather
  never set, set but disabled, and enabled with every opt-out;
* `mix` is continuous (no step of any field above 1.05 x range / 200), turns by the short way, and its
  frames across t = 0.5 change no more than frames at 0.3;
* overcast dims the sunlit ground, greys the sky, and the haze is the exponential fog of density
  0.065 x haze^2 to the pixel;
* a weather animated in `Scene::up` is pure: a frame alone equals the same frame after others, and the
  scene's own members are as left.

The clouds (small frames, Preview quality except where noted; each check has a negative control):

* from the ground, looking straight up over a 4 x 4 grid of places, the cloud fraction of cumulus and deck
  at cover 0.3 and 0.7 is within 0.08 of the cover; from 3 000 m, looking down, the ground shows through
  a 0.6 cumulus field and not through a full deck;
* climbing from 300 m under the base to 300 m over the top, the worst frame-to-frame step falls as the
  frames multiply (40 against 160), as a continuous change does; a switch that starts or stops the march
  at the base or the top makes a jump that keeps its size;
* the shadow on the ground moves with the wind: the cross-correlated shift between two frames 3 s apart is
  along the wind within 5 degrees and its length within 10% of wind x time; with the shadows off the ground
  does not change;
* a box in front of a cloud is pixel-identical with and without clouds; a box behind one is covered; a
  hill that reaches into a low layer is fogged only where it is inside;
* a frame is a pure function of `t` (alone, after other frames, twice); off means off (every setting
  changed but `enabled` false is pixel-identical with a scene that never touches `clouds`);
* no step rings at Film quality (the autocorrelation of a cloud edge crop has no peak at the step
  spacing), and with the jitter off the same crop does;
* curvature: from 10 km, the far edge of a full layer is 3.2 degrees below level (a flat world: 1.9).
* shape (Film, full-resolution march): the median isolated cumulus is narrower at 80% of its height than at
  30% (ratio 0.38, nine clouds), a cumulonimbus is wider at 90% than at 50% (1.73, anvil); the extruded
  2D-coverage shape (`Clouds::extruded = true`) fails both (0.88 for the tower, no isolated heaps);
* soft edges: the 10%-90% alpha ramp over 33 cumulus edges at 1080p is 25 px (median); the extruded shape
  gives 8 px;
* stipple: Laplacian variance over the cloud pixels is 139 with blue noise and the bilateral filter, 408 with
  white noise and no filter (`Clouds::denoise = false`);
* no seam: the worst column difference over the median of its neighbours in the from-12km frame is 2.59;
  a 6-level step added at one column gives 3.28;
* crisp sun: where no cloud is within 90 px the frame is pixel-identical with the clouds-off frame; the
  disc's edge ramp is 11 px with its glow (24 px from a sky baked at 256);
* haze: from 12 km clouds 3.3-5 degrees below the horizon differ from the sky by 175, those at 18-22
  degrees by 366 (haze off: 262 and 359).

Film quality marches at the full frame with 96 steps and 6 light steps (High 160 and 10; Preview 28 and 4
at half resolution), takes its start offset from a baked 64 x 64 void-and-cluster blue-noise texture, and a
5 x 5 depth- and alpha-aware bilateral filter removes the remaining grain; a pixel with no cloud in its 3 x 3
neighbourhood is never touched, so the sun and the sky stay crisp. The shape is the Perlin-Worley 3D noise
remapped by coverage and a per-type height profile (rounded cumulus with a soft base, a sheared tower with
an anvil for cumulonimbus, flat for stratus and deck), eroded by 3D detail noise that inverts near the base.

Not solved yet, and printed by the weather test as KNOWN FAILURE or KNOWN WEAKNESS rather than asserted: the
cumulonimbus anvil is a flat plate floating over a tower (the width ratio at 90% over 50% of the height is
0.66, not over 1.2); the nearest overhead cloud carries no more detail than a cloud at 5 km (the Laplacian
measure is 0.04 against 0.06 and the detail switch does not change it); and the row-over-column energy check
for bands in height barely separates its jitter-off control. The extinction is 0.045 per metre at density 1; the
density has a no-slope toe, `d * d * (2.5 - 1.5 d)`, so edges fade over metres instead of a pixel; each fine
step samples at its own offset, so the step lattice does not ring; the coverage map is read through an eased
bilinear fetch and at no coarser than level 2.

