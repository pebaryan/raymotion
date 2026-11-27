# Weather

`g3d::Weather` (include/rmtn/weather.hpp, `Scene::weather`) is one weather state for a 3D scene:
a wind, cloud cover, haze, precipitation, wet ground and snow, lightning and a rainbow. While `weather.enabled` is
true, the scene's grass, trees, water, rain and snow, sky and fog read it, so they all lean, roll,
slant and grey the same way. With `enabled` false (the default) nothing reads it and no pixel
changes: old scenes render as they did. For a ready-made look, see "Prefabs" below; for water on its own, [water.md](water.md).

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
| `wetness` | 0..1 | pin on the ground's wetness (-1: accumulate from the history); see Wet ground and snow cover |
| `temperature` | degrees C | decides snow against rain on the ground (default from the preset) |
| `accumulationScale` | x | runs the ground's clock faster than the film's |
| `snowCover` | 0..1 | pin on the settled snow (-1: accumulate) |
| `haze` | 0..1 | 0 clear air, 1 thick fog; sets the fog |
| `lightningRate` | strikes / minute | strikes from the rate when `Scene::lightning` has none listed (see "Lightning, storm light and rainbows") |

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
a cloud type or precipitation kind therefore changes at the middle of a blend (the `weather` prefab fades a rain that only one end has in from zero instead; `wx::mix`). `enabled` is true when either is.
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

Light under a thick sky (W12): the sky light a sample gets is dimmed by the optical depth of the whole column above it
(four taps to the top of the layer, floor 10 per cent), so the underside of a thick deck or storm is dark; and the sun is cut
by up to 75 per cent as `cover` goes from 0.7 to 1, because the deck blocks it in a way the local march cannot see, so the
lobes under a heavy cover do not shine white. A thick deck seen from below with the sun high on its far side has a base at
0.44 of the clear horizon's luminance and nothing brighter than 1.16 of it (before: 0.94 and 1.42; `weather_prefabs` test).

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
* **Lightning.** `Scene::lightning` (below) lights the cloud from inside along the strike's channel, dimmed by the
  cloud between; `Clouds::flash` (position, colour, intensity, radius) is the older hand-set point light inside the
  march and still works.
* **Cumulonimbus.** With `Clouds::anvil` (default 1) the layer holds a few storm cells, not a field of towers. A cell is a
  broad lumpy column about as wide as it is tall (`radius` 4.5 to 7 km, so a core of 9 to 14 km) that narrows slowly into a
  domed top, and an anvil that grows out of the column's top: a wedge, thick against the tower (its underside at 0.66 of the
  layer, with soft mammatus-like bumps) and thinner and higher downwind (0.90), widening and fraying out along the wind
  (`drift`, or the weather's wind) and at its edges, with a smooth fibrous texture stretched along the wind. The column fills
  the height up to the anvil, so there is no stem and no gap. Cells come from the seed (one possible cell in each 40 km
  square, present with probability 0.3 to 0.8 growing with `cover`: about 2 in 60 x 60 km, at most 4) or from the list
  `Clouds::storms` (script `clouds.storms`: `[{"x","z","radius","top"}]` in the pattern's frame, which drifts with the
  clouds; `top` in metres above the ground, 0: 0.93 of the layer). Between the cells the lowest fifth of the layer holds
  ordinary cumulus, more of it with more `cover`. `g3d::stormCells` lists the cells in a box. `towerVariety` (0..1, default 1)
  makes the cells differ: radius, the anvil's length, width and turn off the wind, the lumps, and where each looks into the
  shape noise. `anvil = 0` restores the older dense field of sheared towers. Cumulus, stratus and deck are not touched.

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
  A lightning flash lights the drops (they take the flash's point light with the rest). Rain does not enter the water's reflection pass.

`fx::rain3d` and `fx::snow3d` are unchanged and still fill a fixed box. Use `precipitation` for rain
that has to fill a valley or follow a camera; `examples/scripts/porch_rain.as` is the reference.

## Wet ground and snow cover

`Scene::ground` (`g3d::Ground`) is what rain and snow leave behind. It is two numbers that are pure
functions of the time: `wetness` (0..1) and `snowDepth` (m). With `Scene::weather` enabled they come from
`Weather::ground(seconds)`; set `ground.wetness` or `ground.snowCover` (a pin, 0..1 of 10 cm) and the pin
wins. `ground.ownWeather` keeps a scene's pins against a `weather` object. Without a `ground` object and
with no weather precipitation, no pixel changes.

**Accumulation.** The weather integrates its precipitation history in fixed steps, so frame N is the
same however you reach it. Wetness soaks in over about 150 weather seconds of rain and dries with an
exponential (time constant 1500 s in calm air, shorter with wind and unclouded sun). Snow adds 5 cm an hour at intensity 1 below
freezing and melts at 0.5 cm an hour per degree above 0 (faster in sun); `temperature` (degrees C, default from the
preset) decides which. `accumulationScale` runs the ground's clock faster than the film's (500 turns a
20 s clip into 2 h 45 min). `history` is a list of keys `{seconds, kind, intensity}` for rain that
started earlier, or changes kind mid-shot; the ground follows the keys.

**Wet surfaces.** A wet surface is darker (x0.55 in albedo), smoother, and mirrors more sky at grazing
angles (a thin film, Fresnel from 0.02). Slopes dry first (a 60 degree slope gets about a quarter of the
effect of flat ground). Where `reflections` is on, a screen-space march adds the scene's own reflection
on wet ground and puddles: a second draw over a copy of the opaque picture, off when the frame is dry.

**Puddles.** Low-noise, flat-only (up-facing within about 10 degrees), growing with wetness as the water
level rises through a hashed height field (`puddleSize` metres, `puddles` 0..2 for the share). The edges
are warped, so they are irregular rather than round. Rain rings (`rippleRate`, rings a m2 a second at
intensity 1) run on puddles and on open `water`: circular, expanding at 0.4 m/s, six slots a 0.75 m cell
with one ring a slot every 1.1 s; `Ground::rings()` is the CPU twin.

**Snow.** Up-facing faces only (a smoothstep of the normal from 0.5 to 0.9), with two octaves of noise so
the cover first lies in patches and closes by about 5 cm. Grass tips whiten first; trees take cover on
their upward-facing tiers. Sheltered ground (under a roof or a tree) stays bare when the precipitation
has `occlusion` on, since the rain's top-down depth map decides shelter.

**Opt-out.** `Material::weathering = false` (`"weathering": false` in a script) leaves a material alone:
lit windows, screens, emissive things. Unlit materials are always left alone.

**Limits.** Shelter needs `precipitation.occlusion`; without it the ground is open everywhere. The
rings are not at the exact splash points of the falling drops. The SSR sees only what is on screen (a
ray leaving the picture finds the sky), and it adds a second pass over the opaque scene. Grass is not
shortened by snow, only whitened.

**Script.** `"ground": {"wetness", "snowCover", "puddles", "puddleSize", "rippleRate", "reflections",
"snowOnGrass", "ownWeather", "seed"}`; weather keys `temperature`, `accumulationScale`, `history`.
`examples/scripts/porch_rain.as` (puddles and rings on the path) and `first_snow.as` (settling snow).

## Lakes, rivers and pools

(A short guide, with the `water-body` prefab, is in [water.md](water.md).)

`Scene::waterBodies` holds `g3d::WaterBody` shapes of water beside (or instead of) the endless sea
`Scene::water`. A **lake or pool** is a polygon (`WaterBody::lake(outline, level)`, optional `holes`
for islands); a **river** is a path with a width per point and a level or a fall
(`WaterBody::river(path, width, level, flowSpeed)`). Polygons are ear-clipped, rivers are a strip
whose uv is (across, arc length). Every body has its own `Water` look (`look`), shares the sea's
shader and gets its reflection (the planar pass is shared by bodies at the level of the sea or of
the first reflecting body; bodies at other levels reflect the sky only), refraction, band-limiting,
the weather's wind and rain rings. `fetch` defaults to the body's own size: a pond does not grow
ocean waves.

* **Shore.** The surface fades out where the bed (read from the depth of the opaque scene) comes
  within `edgeSoft` metres of it, so there is no hard waterline. Within `foamWidth` metres of depth it
  grows lace foam: thin white filaments (the cell edges of a Worley field, bent and broken into patches)
  drawn on top of the water, never opaque on a calm shore. How much foam there is follows the body's
  wave energy, from the significant wave height of the waves it is drawn with (the weather's wind and the
  body's fetch included), and for a river from the current: a calm pond keeps a few thin broken
  threads, a windy lake a net, a fast river whitewater. The band's width is `foamWidth` times 0.4 to 1
  with the energy, and `shoreFoam` (0 to 1) scales it. The depth is the vertical one along the view ray, so
  foam follows the terrain, not the polygon.
* **Small water.** A body's waves wander in phase and come and go in patches, so no single wavelength
  stripes a small lake.
* **Flow.** Two ripple layers advect along the river's arc length, half a period apart, with
  weights that cross-fade through a triangle and a jitter per cycle, so a phase switch never shows.
  `flowSpeed` is metres per second along the path; `narrowFaster` speeds it up where the river
  narrows; `flowPeriod` is the cycle length in seconds.
* **Limits.** Bodies are flat at their level (waves are in the shading and in `height()`), seen
  from above water only, at most 32 per scene.
* **City.** `city::Look::realWater` (off by default) turns CityJSON water polygons into bodies at
  their level (`city::waterBodies(model, look)` returns them); off, water stays a flat face of the
  mesh as before. `geo3d::` has no water polygons, so there is nothing to convert there.
* **Script.** `waterBodies: [{ "type": "lake"|"pool"|"river", ... }]`, see docs/scripting.md.
  `examples/scripts/river_valley.as`.

Tests (weather ctest): a pool in a box (water inside the polygon, none outside), foam only where
the depth is under the foam width, no step in a column across the shoreline, ripples moving at the
flow speed within 15 %, no reset seam over 10 s, a city lake at its level, foam following the wind, no
spectral stripes on a lake, the hill mirrored in it, a sea at level 2 equal to level 0 lifted, no change without
bodies, purity, and the lake reflecting a pole.

## Wakes, ripples and floating objects

Everything here is worked out from the time alone: the wake is not simulated, so a frame is a pure function
of `t` and a scrubbed or sampled frame equals a played one.

**Wakes** (`Scene::wakes`, `g3d::Wake`): a boat follows `path` (a Catmull-Rom curve through the points) at `speed`
from second `start`, `length` and `beam` metres. Each earlier moment of the boat shed a ring of deep-water waves
(omega^2 = g k) that has spread for as long as it has been since. The shader adds up 64 of these rings, with a
phase window that keeps only the ones that add coherently, so the arms of the Kelvin V lie 19.5 degrees off the
track at any speed (measured 19.7 and 20.5 degrees at 80 m behind at 4 and 7 m/s), the crests are feathered, the height follows the beam and the Froude number (U / sqrt(g L): little below 0.4, most near hull speed, saturating when planing) times `strength`, the amplitude falls with
1/sqrt(distance) and with age, and nothing lies ahead of the bow. Waves longer than the transverse ones (2 pi U^2 / g) are
not made. White foam follows the first crests and the bow's lines (broken into dashes), and a propwash trail follows the stern for
`trail` seconds: about a beam wide, translucent (alpha at most 0.5), torn into streaks along the track and fading over a few tens of metres. The sum uses twice the 64 emission points (the middle of each pair) so the crests stay smooth, and the hull only cuts waves shorter than about a beam (exp(-(k beam / 12)^2)), so the short divergent crests ("feathers") show along the arms of a boat at displacement speed; a boat at planing speed makes long waves and few of them. The waves change the shading normal only; the mesh is not displaced.
`g3d::wakeSpray(wake, fps)` is the bow spray, an `fx::Emitter3D` (more droplets and higher the faster the boat goes).

**Ripples** (`Scene::ripples`, `g3d::Ripple`): a stone of `size` metres makes a dip of `strength` metres at `time`.
The rings are the capillary-gravity dispersion (omega^2 = g k + 7.3e-5 k^3) of that dip: the long waves run ahead, the
fine ones trail, so the train is a few round crests about 0.25 m/s outward for a 4 cm stone, that fade over `life`
seconds. `g3d::splash(ripple, fps)` is the splash.

**Floating objects:** `g3d::ride(object, scene, seconds, how)` sets an object's height, pitch and roll from the water
under it: the mean of a grid of samples over the hull's footprint (`Ride::length`, `beam`, `along`, `across`) for the height and a
least-squares plane through them for the tilt, less `draft`. A long hull therefore does not jitter on waves shorter than
itself. It reads the sea, any water body, and the wakes and ripples on them (`surfaceHeight`, `surfaceSlope`).
Set `Ride::skipWake` so a boat does not ride its own bow wave.

**Limits.** At most 4 wakes and 16 ripples are drawn (the first ones). They shade the surface; they do not move its
mesh, so a grazing view of a tall wake shows a flat silhouette. The wake is a deep-water theory (no shallow-water
shoaling). The foam is procedural noise, not a simulation. `ride` reads the surface at a time you give it; a
script's `ride` does this from the water's `fps`.

Tests (weather ctest, `waterWakes`): the V half angle from the CPU twin at 4 and 7 m/s (19.5 +- 2) and the two
agreeing within 1.5 degrees; the GPU slope against the CPU twin and nothing ahead of the bow; a ripple's ring radius at the group
speed (+-20 %), growing, and round (spread under 12 % over 16 directions); a floating box following the footprint mean, its pitch
taking the slope's sign, and the footprint calming the jitter; the dinghy not moving before a wave can reach it;
the propwash under 0.25 alpha at 30 m behind and under 2.5 beams wide at 10 m; a lit view (sky reflection, sun to the side, no foam) where the V changes the luma of columns by over 8 levels; at least 5 crests along each arm over 3 boat lengths in a lit oblique view of a 2 m/s boat; purity; and a boat and stone that have not come yet changing nothing (difference 0).

## Ocean to the horizon

`Water::spectrum` (off by default; old scenes are pixel-identical) replaces the sum of waves with a wave spectrum:
a JONSWAP sea (gamma 3.3, Mitsuyasu cos^2s spreading, one-sided downwind) of the weather's wind speed and heading and the
water's fetch, rescaled so its significant height is the W1 value (`Weather::waveHeight(fetch)`) exactly. It is turned into
up to four cascades of 128 x 128 grids (an inverse FFT per cascade per frame on the CPU, a pure function of time), with patches
about 7.3, 7.6 and 7.1 times apart (not whole numbers; about 8 times the peak wavelength for the largest) and a band per cascade with an
energy-preserving cross-fade, so no wave is counted twice. Each cascade is looked up through a small smooth warp
(`decorrelate`) so a patch never repeats on the sea. Cascades of 8 m and more move the mesh (height and choppy
displacement); finer ones only shade. The mesh is rings from 0.2 m to past the horizon (`Water::height`, `slope` and
`ride()` read the same grids, so a boat sits on what is drawn). Far away the shader picks a mip level from the pixel's
footprint and puts the slope variance the level removed into the glint's roughness, so the far sea goes to a mean
roughness and does not shimmer (`Water::filter`).

The sea follows the planet: the drop is d^2/2R (R = 6371 km), so the horizon of a camera at height h lies atan(sqrt(2h/R))
below horizontal (0.10 degrees at 10 m, 3.2 at 10 km). The far plane of the camera is raised to reach it (on a copy: your camera is
untouched). Haze fades the far sea to the sky's own horizon colour over `visibility` metres (60 km); the
backdrop fills the sliver down to the dipped horizon so there is no seam. Whitecaps are the Jacobian of the choppy displacement with
three lags (0, 0.9, 2 s) so foam decays after the crest; their area follows Monahan's law (about 0 below 4 m/s, 1.5 % at 8,
5 % at 12, 14 % at 18 m/s, scaled down by a short fetch). From 12 m/s spray streaks along the wind come in, up to full at 20 m/s.

What is left out: no breaking wave shape (the foam is texture over smooth crests), no shoreline or shallow-water
dispersion on the spectrum (water bodies keep their own waves), no spray particles, one wind heading for the whole sea,
and the first frame of a scene costs about 8 ms for the FFTs. The planar reflection pass is still a flat mirror about the
mean level, so on the curved sea it is correct only near the camera. Cost at 1080p on the development machine: 5 ms
for the old sea and 16 ms for the spectrum sea (low or at 10 km), of which about 10 ms of CPU per new time (FFTs and foam).

Roughness and whitecaps (added after review). The grids stop at about 5 cm, so the sea's slope variance above them is added to
the glint's roughness as `lost` does for a mip level: the short-wave branch of Elfouhaily et al. (a curvature spectrum that rises
with the friction velocity) integrated from the finest grid down to 5 mm. Grids plus that tail give a mean square slope of 0.033, 0.063 and
0.111 at 5, 10 and 18 m/s against Cox and Munk's 0.029, 0.054 and 0.095 (`Water::shadingSlopeVariance()`); the grids alone are 55 to 64 % under at
10 and 18 m/s. The foam comes from the finest cascade that still has a patch of 30 m or more (the waves of a few metres to a dozen), its Jacobian
patches are drawn out along the crest (a line of texels about 6 m each side, so patches are about 4 times longer than wide, across the wind) and
blurred a little; the shader sharpens the edge, breaks it into lace with three scales of noise that fade out at the pixel size, and
thins it with a slow mask so a tile's few patches do not make a lattice from above. In a fresh breeze the area is raised up to twice the
law, so white horses are seen at 10 m/s. A reflected ray that points into the water is folded up smoothly and tinted by the water's
colour (the old crease drew thin rings round pits on a steep sea). The nested contour rings of a dark storm sea were the sky: all ocean
textures are RGBA32F with trilinear mips from float data and the roughness taps are smooth, but a steep surface magnifies a tiny patch of
the 8-bit sky image many times, and its cloud noise shows as terraces. The sea now reads the sky at a mip level that grows with its slope
variance (`oceanSkyLod`, up to 5), which is also what a rough sea mirrors. The foam edge is widened by `fwidth` of its own value so it is
anti-aliased rather than a stair of map texels. What remains: faint rings in the pits of the 3x crop, and the foam of a short-fetch sea is
held down less than the law says. Test `oceanArtefacts` reads a 3 m storm view: the dark water's mean Laplacian (3.79 against 4.23 with
the sky at full resolution, bound 4.0) and the share of foam edges crossed with at most one in-between pixel (1.0 % against 2.1 %,
bound 1.5 %).

Debug read-outs for tests (`Scene::ground.debug`): 3 draws the foam alone, 4 packs the surface height into two bytes.

Tests (weather ctest, `ocean`): the slope picture of 2 km and 400 m shows no comb at the cascade periods and their harmonics
(and does with one cascade or with `decorrelate` off); Hs at 5, 10 and 15 m/s within 20 % of the W1 prediction and
rising as U^2 when fully developed; purity (a frame alone equals the frame after others, two runs identical, CPU height after a
flushed cache identical); the spectrum off is pixel-identical whatever its fields hold; the horizon row at 10 m and 10 km
against the dip (1.5 px; a flat sea misses it); foam about 0 at 3 m/s and rising at 8, 12 and 18; the far third of a
grazing view changing little as the camera creeps (and far more with `filter` off); the CPU height within 2 cm of the
height the mesh draws, `ride()` following the surface; the shading slope variance within 30 % of Cox and Munk at 5, 10 and 18 m/s; and foam patches at 12 m/s with a median aspect ratio of 2 or more, within 30 degrees of across the wind.

## Lightning, storm light and rainbows

`Scene::lightning` (`include/rmtn/lightning.hpp` for the bolt itself, `docs/scripting.md` for the keys) is off until
`enabled`.

* **The bolt.** `bolt::make(start, end, seed, shape)` is a main channel by recursive midpoint displacement (seven splits:
  128 pieces) with branches: a vertex of the main channel branches with `branchChance`, a branch of level L with that times
  `branchFall^L`, each shorter (`branchLength`, times 0.55 per level) and thinner, leaving by about 38 degrees and turning
  towards the strike's end. The same seed, start and end always give the same bolt.
* **The time profile** (`bolt::profile(seed, tau)`) is all from the seed: a leader that grows the channel over about
  35 ms, a return stroke at full brightness held for at least `minVisible` (70 ms: two frames of a 30 fps film whatever the
  phase), two to four restrikes over 0.2 to 0.5 s with the channel glowing faintly between them, then nothing (a strike is
  over by 0.75 s at the latest). It is a pure function of the time, so motion blur samples it correctly.
* **Drawing.** Camera-facing ribbons of emissive light, a hot core (white, `width` metres, never thinner than
  `minPixels` at 1080 lines) inside a wide soft glow in `color`, thinner along branches, with rounded ends and a MAX blend
  so joints and crossings do not beat. A bolt that ends at the ground is drawn whole; one that stays in cloud (`end.y` more
  than 50 m above `groundY`) is a faint channel and its light is the cloud's glow. Bolts are drawn after the clouds, against
  the opaque depth (a hill hides the foot of the channel) and are not in the water's mirrored pass.
* **What a flash lights.** For the strongest strike of the frame: a blue-white point light (`light`, range `lightRange`) a
  third of the way up the channel, which the lit objects, the sea and the rain streaks take with the rest of the lights
  (the eighth light is replaced when all 8 are used); and the cloud's inner glow (`cloudLight`): in the march each sample
  takes the light of the nearest point of the channel dimmed by four density taps towards it, so a cloud shines round the
  bolt and thin edges glow while a thick cloud keeps its dark heart.
* **Where strikes come from.** The list `strikes` ({time, start, end, seed, intensity}), plus, when `rate` (or, at -1, the
  weather's `lightningRate`) is above 0, strikes from a hash of 0.5 s time slots: each slot holds a strike with probability
  `rate * 0.5 / 60`, placed in the disc (`area`, `radius`) under the cloud base, a share (`groundShare`) reaching the ground.
  `followClouds` carries both along with `Clouds::travel`. A rate that changes during a strike's 0.75 s can make it pop in
  or out; keep the rate steady or give the strikes in a list.
* **Thunder.** `bolt::thunderDelay(metres)` is distance / 343. `g3d::thunderCues(scene, frames, fps)` lists each strike
  of the clip: `strikeTime`, `distance` from the camera to the nearest of the channel's ends and middle, `arrives`
  (`strikeTime + distance / 343`), `position`, `toGround` and `seed`. Put a thunder clip on the soundtrack at each
  `arrives` with the existing audio cues: in a script `media::audio(startFrame, durationFrames, file, startFrom, endAt,
  rate, loop, volume)` during `configure` (start frame = `arrives * fps`; the volume can fall with `distance`), in C++ a
  node with an `AudioClip` (`timeline.hpp`) starting at that frame. Nothing is added to the audio for you.
* **2D.** `fx::lightning(ctx, bolt, frame)` and the node fn `fx::lightning(bolt)` draw the same generator in a 2D
  composition (five additive layers from a wide glow to a white core, and a screen flash); from a script,
  `fx::draw("{\"lightning\": {...}}", frame)`. `examples/scripts/lightning_2d.as`.

**The rainbow** (`Weather::rainbow`, drawn as a pass over the picture after the clouds when the weather is on):

* The bow is a circle round the point opposite the sun (the sky's sun, or the light `sunLight` names). Light of each of
  12 wavelengths is put at Descartes' angle for water's index at that wavelength: the primary at 42.3 degrees (red) to
  40.6 (violet), red outside; the secondary at 50.4 (red) to 53.7 (violet), reversed and 30% as bright, with its own
  wider falloff. The result goes through the CIE colour matching functions to RGB, so the arc is a continuous
  spectrum with a soft edge on the dark side and a long fade on the lit side (supernumerary bows are not drawn).
* Alexander's dark band between the two is a darkening of the sky there; the sky inside the primary is a little lighter.
* How much shows: the weather's rain (`precipitation.intensity` for rain and sleet, or `rainbow.distantRain`, rain out of
  sight, whichever is more, through a soft curve) times how much sun gets through the cloud (`rainbowSun`: gone at cover
  0.73 and above) times a fade as the sun sets, and in `Auto` a fade between 40 and 46 degrees (the bow sinks below the
  horizon). `mode = On` shows it whatever the rain and the sun's height, `Off` never. With the sun in front of the camera
  the ring is behind the view and nothing is drawn.
* Geometry in front of the rain (nearer than a third of `rainDistance`, default 2.5 km) hides it; the sea plane counts;
  things farther than `rainDistance` have it over them (a hill in the rain's distance is behind the bow).
* Not drawn in the water's reflection. `strength` (0..2) scales it.

**Storm look.** The cumulonimbus storm cells is under "Volumetric clouds". The sea has `Water::Spectrum::storm` (0..1; -1 takes it
from the wind: 0 at 12 m/s, 1 at 20): the water scatters back the grey sky (darker and greyer, by up to 40 % and 55 %), crests
are sharper (choppiness x 1.5 under a `Weather`; a hand-set sea keeps its choppiness), foam lies in long, narrow filaments along the wind that wander sideways, vary in width and break, down the troughs as well as in whitecaps (broken into smaller patches),
spray streaks are on from `storm` rather than from 12 m/s, and the sea fades into the
horizon haze 45 % sooner. At 18 m/s it is `storm` 0.84.

`examples/scripts/thunderstorm.as` is a whole storm: towers with anvils over a stormy sea, flashes in the cloud and bolts to
the water, rain that moves off, the sun behind the camera and the rainbow.

## Light in the air

Two post passes over the finished opaque picture, both off by default (with them off the picture is the old one, bit for bit).

**Sun shafts** (`Scene::volumetric`, quality Off / Low / Medium / High). The fog is the scene's own: `fogMode`,
`fogDensity`, `fogHeight`/`fogFalloff`, or the weather's haze, or the exponential fog `Volumetric::density` (with
`heightFalloff` and `heightBase`) sets for the frame. Each view ray is marched, at a share of the picture's size, from the eye
to the opaque surface under the pixel, or to where the fog is all but opaque. Every step takes an equal share of the ray's fog
opacity (where the fog has a closed form) and asks how lit that bit of air is:

    fog colour x (ambient + (1 - ambient) x mix(1, sunlit x phase, sun))

* `sunlit` is the directional light's shadow-map tile read at that point in the air (four taps) times the clouds' ground
  shadow (W2), so shafts form through gaps in canopies, windows and clouds. The sun is the sky's `sunLight`, else the
  directional light along the sky's sun, else the first directional light. A light that casts no shadow lights all the fog alike.
* `phase` is 4 pi times a Henyey-Greenstein lobe of `anisotropy` (default 0.7), compressed to a third of its swing and capped at 8
  so the glow round the sun does not blow out. It is 1 everywhere for `anisotropy` 0, and its mean over all directions is 1, so
  the fog is brightest toward the sun, dimmest away from it, and the same on average.
* `sun` is the sun's elevation (the shafts fade out as it sets) times the weather's `sunScale`.
* `ambient` (0.35) is the share of the fog's brightness that does not come from the sun: fog in shade. Low in a dark barn.
* With `scatterLights` spot and point lights light the fog too: their cones show, with the shadows they cast (a point light's
  six tiles included). Radiance is half the light's colour x intensity x its falloff x the phase function.

The march writes what the fog adds to the eye and how much of the scene behind it gets through; the composite brings that up with a
depth-aware 5 x 5 filter (the march's jitter, interleaved gradient noise on the low-resolution pixel grid, is the same in every
frame, and the filter averages it away; there is no temporal accumulation) and gives the sum `scene x through + added`.
While it is on the lit shader draws no fog (the analytic fog is replaced, never added to it: an 8-bit picture could not take a
bright fog off again). In the unshadowed, isotropic limit the sum is exactly the analytic fog (checked: mean 0.23 levels of
difference). Costs at 1080p, over a 400-box street: Low 0.3 ms, Medium 0.5, High 1.2 (16 steps at 0.35 of the width and
height, 32 at 0.5, 48 at 0.75), copy of the scene included; aerial perspective adds about 0.3 ms.

What it does not do: transparent surfaces and particles are fogged as if at the opaque depth behind them (the sea, which has no
depth, as if at its plane); the water's mirrored reflection carries the analytic fog, with no shafts; a ground-hugging fog
(`fogFalloff`) is stepped evenly in distance, not in opacity, so a camera high above a thin layer wants High; a camera under the
sea draws no shafts.

**Aerial perspective** (`Scene::aerial`). Distant opaque things lose their own light, blue first, and the air in between adds the
sky's horizon colour in that direction (the colour the fog takes with `fogFromSky` and that the spectrum sea fades to, so land,
sea and sky meet without a seam). The optical depth is `strength / 30 km` of sea-level air (green; red 0.77 times that, blue 1.7
times) along the ray, weighted by exp(-altitude / `scaleHeight`) (2500 m): a ridge 30 km off is paler and bluer, and from 3 km up
the same distance is hazier less. A grey box (40) at 1, 5, 15 and 40 km goes (48, 52, 60), (66, 79, 99), (90, 110, 142) and (112,
139, 182) toward the sky's (133, 169, 225) with the blue/red ratio rising 1.26, 1.49, 1.58, 1.62 (off: 1.00); the red haze share of
a box 15 km off is 0.45 from the ground and 0.33 from 3 km up. It touches what is in the depth buffer: not the sky, not the
sea (W9's haze does that), not the clouds (`Clouds::haze` does). Land under the water (the sea writes no depth) is not hazed through the sea: where the sea plane is in front of the depth the pixel is sea. The haze never goes past the horizon colour.

`examples/scripts/forest_light.as` (morning mist, the sun low behind the trees; its `ambient` is 0.05, because a high one lifts the shade and the shafts die), `sunset_sea.as` (the seam case: a headland hazed into a sunset sea), `barn_shafts.as` (dust in a dark barn with a
high window) and `blue_ridges.as` (six ridges to 35 km).

## Camera weather

What weather does to a lens, as a layer filter (`fx::Lens`, `rmtn/lens.hpp`; `Filter::Type::Lens`). It works on any layer: a 2D
composition, or a 3D scene's whole image (put the filter on the node that draws the scene). It reads the layer's pixels and the
frame; it never touches the scene, so a camera move does not move a drop. Off means off: with `rain.rate`, `frost.amount` and
`snow.rate` at 0 and no wiper, every pixel is as it was.

```cpp
fx::Lens lens;
lens.rain.rate = 0.7f; lens.rain.blurBehind = 12;        // raindrops, the background out of focus
lens.rain.rateAt = fx::rainOf(scene->weather);           // or follow the scene's rain over film time
lens.wiper.period = 3.2f;                                // a windscreen wiper
node->isolate().filters.push_back(fx::lens(lens));       // also fx::lensRain, lensFrost, lensSnow, wiper
```

**Raindrops.** The layer is cut into square cells (0.085 of its height); each holds two slots, and a slot spawns at most one drop
in each cycle of 7-15 s. Whether it spawns follows `rate` (or `rateAt`) at the moment of spawning. A drop grows to its radius
(0.10-0.44 of a cell, times `size`) with a time constant of 0.7 s, stays, and shrinks away over its last 1.2 s. Everything is a hash
of the cell, the slot and the cycle, so a frame is a pure function of `t` and `seed`. A drop above 0.27 cell starts to slide after
0.4-1.9 s, at `slide * 3.667 * radius` px/s (a 28 px drop at slide 1: 100 px/s), with a wiggle of 0.12 radius, for 0.7-1.9 cells,
then it sticks. It leaves a trail of tiny drops behind it. Drops that melt from snowflakes behave as small drops.

Each drop is a low lens, nearly clear. Its outline is irregular (a per-drop stretch plus a low-frequency wobble; a sliding drop is a
teardrop with a narrower top). The picture inside it is the sharp picture sampled at `c - (3.6 u + n * 0.8) r`, with `u` the offset from
the centre in radii and `n` the slope of a low spherical cap (contact angle about 30-60 degrees): about 3.6 radii around are flipped and
squeezed in (a ramp seen through it is minified about 4-6 times, reversed), so a whole lamp fits in a drop, at the brightness of the
background (no darkening, no tint). Only a rim a few px wide is shaded (darker on the upper side, a faint gathered glow low), the
colour fringe is 4% of the radius, and a small sharp glint appears only where a bright light lies in the reflected direction.
`blurBehind` blurs the picture outside the drops by a disc of that many px, with bright lights blooming into bokeh; the drops show
the sharp picture. The CPU functions `fx::lensDrops` and `fx::lensFlakes` return the same drops the shader draws (id, position,
radius, sliding, speed, birth time).

**Frost and condensation** (`frost`). `amount` 0..1 is how much of the glass is covered; `startFrame` and `endFrame` make it creep
from 0 to `amount` between them. With `fromEdges` it spreads from the edges and corners inwards (the corners first), else in patches. The
crystals are dendrites grown from nucleation points along each edge, in that edge's frame, so every frond runs inward along the
creeping front: a stem, branches off it at 40 degrees, sub-branches and barbs off those (spacing, length and skipped branches hashed per
frond), at four scales, plus long fine hairs running straight in, all bent by a warp that is mostly along the edge. The pattern is a
distance to line segments, anti-aliased with a smoothstep one pixel wide (no hard thresholds, no emboss). The four edges are weighted by exp(-(d - dmin) / 0.06 H), so at a corner two sets of fronds interpenetrate with no seam, and each corner also nucleates fronds along its diagonal. A
film of milky frost covers the glass; crystals scatter and blur what is behind, lift it toward white, and have a thin
bright edge only on their boundary. `kind: "condensation"` is a fog instead, blurred and greyed, with clear streaks where drops ran
(when there is rain) and the same cleared patches. `clears` (up to four `[x, y, radius]`) wipe round patches with a ragged edge.

**Snowflakes** (`snow`). A flake lands at a hashed time and sits for 1.2-3.2 s as a close, out-of-focus, partly transparent soft white
disc (radius 0.029-0.072 of the layer height, hashed opacity, about half as many as before) with a six-fold hint of under 10% of its brightness; then, over 0.9 s, the white shrinks into an irregular wet blotch while a transparent drop (0.45 of
its radius) grows, which behaves like a small drop and evaporates.

**Wiper** (`wiper`). An arm from a pivot (default 0.5, 1.18 of the layer, below the frame) of `length` 1.06 of the height
sweeps from `from` to `to` degrees (0 is straight up, positive to the right) and back, then rests: each stroke takes 0.3 of
`period`, the rest 0.4 of it, with a cosine ease. It is drawn as a dark arm and blade (`drawBlade`, `bladeWidth`). The wiper clears
drops: a drop is removed at the first moment after it was born that the blade touches any part of it (its position at that moment, for a sliding
drop), and so are the trail drops and flakes. The set of survivors is worked out from spawn times and sweep times, so it is a pure
function of `t`; the swept sector refills at the spawn rate.

Script: `layerFilter(scene, json)` with `"type": "lens"` and the sub-objects `rain`, `frost`, `snow` and `wiper` (docs/scripting.md), or the
types `lensRain`, `lensFrost`, `lensSnow` and `wiper` with the keys at the top level. Put the wiper in the same `lens` as the rain it
must clear. `examples/scripts/windscreen.as` (a night drive in the rain, a 3D scene) and `frosted_window.as` (a 2D evening with frost
creeping in and a wiped patch).

Left out: drops that merge, a wiper that smears the water it moves, frost that regrows after a wipe, and drops that follow gravity
on a tilted lens (the slide is straight down the layer).

## Prefabs

Four prefabs set the pieces above up for you: a landscape (terrain, a meadow, cone trees, optionally a lake), the sky and weather, and a camera,
drawn into a box like the flora prefabs. They are thin: each one fills the `Scene` fields documented on this page, and `rmtn/weather_prefabs.hpp`
(namespace `rmtn::wx`) makes the same state without a prefab for a scene of your own. Water has its own page, [water.md](water.md). `raymotion prefabs`
lists the props. Everything is a pure function of the frame, so any frame renders alone.

```
prefab::use(0, 150, "weather",   "{\"box\":[0,0,1280,720],\"preset\":\"clear\",\"to\":\"thunderstorm\",\"start\":20,\"end\":120}");
prefab::use(0, 150, "rain",      "{\"box\":[0,0,1280,720],\"intensity\":0.8,\"delay\":0.5,\"lens\":0.6}");
prefab::use(0, 150, "lightning", "{\"box\":[0,0,1280,720],\"count\":7,\"span\":4,\"seed\":2,\"clip\":\"thunder.wav\"}");
prefab::use(0, 150, "water-body", "{\"box\":[0,0,1280,720],\"look\":\"breezy\"}");
```

All four take `box`, `size` (landscape width, metres), `trees`, `orbit` (camera turn, radians a second), `fov` (degrees), `quality`
(`preview`, `film`, `high`, the clouds' march), `seed`, `fps` and `ground` (the land's colour). The ones below are theirs alone.

**`weather`**: `preset` (one of the eight below), `to` (a second preset to blend towards; empty for none), `start` and `end` (the frames the blend
runs between; 0 and 0 for none), `lake` (a lake ahead of the camera, default true), `grass` (`auto` is a meadow, bare under settled snow; or a
`flora::grass` look, or `none`), `shafts` (`auto`: only the presets that want them; `off`, `low`, `medium`, `high`) and `pitch` (camera, degrees).
The blend is `wx::mix`: numbers interpolate, a rain or snow that only one end has fades in from zero for the whole blend rather than switching
on halfway, the shafts and lightning follow whichever end has them. The camera looks towards the sun's azimuth plus the preset's `viewFromSun`.

| Preset | hour | cloud cover | wind m/s | precipitation | wetness | snow | notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `clear` | 11 | 0.08 | 3 | none | 0 | 0 | high thin cumulus, full aerial perspective |
| `cumulus day` | 13.5 | 0.45 | 5 | none | 0 | 0 | the default |
| `overcast` | 12 | 0.90 | 5 | none | 0.1 | 0 | stratus deck |
| `squall` | 15 | 0.98 | 13 (gust 0.75) | rain 0.9 | 0.75 | 0 | low dark base |
| `thunderstorm` | 15 | 0.62 | 16 | rain 1.0 | 0.9 | 0 | cumulonimbus to 11 km on a 600 m base, slate sky (`gloom`), a tower and anvil ahead (`Clouds::storms`), 6 strikes a minute, a choppy lake, light shafts, the sun behind the camera |
| `snowfall` | 10.5 | 0.92 | 4 | snow 0.6 | 0 | 1 | bright, flat light |
| `fog dawn` | 6.3 | 0.50 | 1 | none | 0.2 | 0 | haze 0.8, shafts at medium |
| `golden evening` | 17.6 | 0.35 | 3 | none | 0.3 | 0 | low sun, shafts, the rain that has passed as a rainbow |

`wx::applyPreset(scene, name)` (also `wx::apply(scene, look)`, `wx::applyBlend(scene, a, b, amount)`, `wx::look(name)` and `wx::mix`) sets the weather,
clouds, precipitation, ground, lightning, light shafts, aerial perspective, ambient light and sky (`Sky::at(hour)`) of a scene, and puts light 0 at the sun
(never under the horizon) with the sky's own sun light off.

**`rain`**: `intensity` (0..1; drizzle to downpour, 0.6), `wind` (m/s, 7), `windDirection` (degrees, 17), `delay` and `ramp` (seconds before it starts
and to build up; 0 and 1), `wetRate` (how many weather seconds one film second soaks the ground, 60; 0 keeps it dry), `lens` (0..1 drops on the
camera glass, 0: none) and `cover` (0.95). It draws a shelter, a lamp and barrels. `wx::addRain(scene, RainSpec)` sets the weather, the
precipitation, the ground and a `Scene::up` step that sets the intensity from t (`wx::rainIntensity`); `wx::rainLens(spec)` makes the lens filter
with a rate that follows the same history as the ground, so the glass and the puddles agree.

**`lightning`**: `count` strikes (6) falling one in each of `count` equal parts of `[start, start + span)` (0 and 10 seconds) in a disc of `radius`
(1500 m) at `area` `[x, z]` (default `[0, -3000]`), above `cloudBase` (1200 m), `groundShare` of them (0.5) reaching the ground; `rain` (0.8) falls
under it, `light` (1) scales the storm light; `clip` is the path of a thunder sound and `volume` its level (1). With a `clip` it places one audio node
at the time each strike's sound arrives (strike time plus distance over 343 m/s), and none past the end of the film. `wx::strikeSchedule(StrikeSpec)`
lists the strikes, `wx::addLightning(scene, spec)` sets `Scene::lightning` to them (no rate, not following the clouds), and
`wx::thunderCues(spec, listener)` lists the thunder, sorted by arrival, without building a scene. The nearest of a channel's ends and middle is the distance.

Seeded: the same props always give the same strikes and shapes. Open: the prefab landscapes are plain (cone trees, flat meadow); a film builds its own.

## Script

The `weatherPreset` key of a scene descriptor, and the `lightningSchedule` key and the lightning shape keys, are in docs/scripting.md. The water's `spectrum` object and `examples/scripts/open_ocean.as` are in docs/scripting.md. The scene descriptor's `clouds` object (docs/scripting.md) takes the `Clouds` fields by name. Its `weather` object takes a `preset`, optionally `to` and
`amount` to mix towards a second preset, and per-field overrides; `ownWind`, `ownClouds`, `ownLight`
and `ownFog` are keys on the objects above. `examples/scripts/weather_mix.as` is a meadow with
grass, trees, a pond and rain, mixed from clear into a storm across the clip. The `wakes` and `ripples` arrays and an
object's `ride` key are in docs/scripting.md; `examples/scripts/harbour_wake.as` is a launch, a dinghy and a stone in a harbour.

## Tests

W11 (`RMTN_W11_ONLY=1`; 640 x 360 layers, read back from the canvas): all four filters at zero are pixel-identical (diff 0) to the
layer without them, at three frames; static drops keep their place over 0.5 s (316 kept, 0 moved) and a sliding drop of 13.4 px
moves at the documented 59.7 px/s in the twin and in the pixels (centroid of the changed pixels within 0.5 px of the twin; twice the
`slide` is twice the speed); 99 of 99 drops the twin lists are drawn, and no pixel away from every drop differs from the source (0); a
3D scene panned between two cameras has the same drop footprint (Jaccard 0.995); a wiper clears the swept sector: 226 drops in it
without the wiper, 42 just after the sweep, all born since it began, the 116 drops it never touches all remain, 186 495 sector pixels
away from the survivors are identical to the drop-free image (22 090 would differ without the wiper), and the sector holds 119 drops two
seconds later; the blade is drawn at the angle the twin gives; inside a 19 px drop a ramp of 1.5 levels per px is reversed
(correlation with the offset about -1.0 in x and y); frost changes the edge band more than the centre
and grows with the amount and with time, with a cleared patch left identical; a flake sits as a bright blob and melts into a listed drop;
a frame alone, after others, in a new canvas and twice is identical; `rateAt` from a weather's history raises the drop count with the
rain. Each was broken to see it fail: the slide made frame-dependent (4 checks), the wiper ignored in the shader (the pixel check) and
in the twin (4 checks), a flat disc instead of the refraction (2), frost without the edges (6), a tint at zero (3), the filter made
impure (1). Not tested by breaking: drops in world space (the filter has no access to the scene, so it cannot).
Look tests (the round-1 review): over a flat grey 128 the inside of every drop of 10 px or more (16 drops) has mean luminance 128.0, within 0.0%
(bound 10%; with the interior darkened by 25% it reads 96.0 and fails), and the mean circularity P^2/4piA of their drawn outlines
(16 rays) is 1.12 (bound 1.08; round drops give 1.04, which fails); the refraction check now sees the drop minified 5.1 and 4.5
times (-0.999 / -0.999). Frost over grey: the share of 4-neighbour pairs that jump 20 levels or more is 12% (bound 18%; with hard thresholds
instead of anti-aliasing 26%, and the earlier fern pattern 48%), none jumps 60 or more (the earlier pattern: 14%); the orientation of the
stems, from the gradient tensor of the picture blurred over 5 px, runs inward in each edge band (0.40, 0.39, 0.38, 0.36; bound
0.3; the pattern rotated by 37 degrees gives 0.11, the earlier ferns 0.00).
Round 2: a drop under about 5 px has no dark arc (rim and outline fade in between 3.5 and 8 px), so over grey 128 nothing inside one falls below 118
(92%; bound 80%; the earlier look reached 79, 62%). The sitting flakes are 10-26 px in radius on a 360 px layer, and their six-fold harmonic at 0.6 r is at
most 0.082 of the disc (bound 0.10; the earlier flakes 0.131). Frost corners: across the diagonal the mean |difference| to the mirror pixel is 11.7 against
12.2 across a parallel line 8 px away (it must be no larger, and at least 0.88 of it; the nearest-edge split gave 12.9 against 17.2, structure dying
out along the seam). The camera-pan check now compares the changed pixels with the twin's drops (0 of 4085 away from every drop; 34 of 34 drops over bright picture seen
in both cameras), since a clear drop over smooth picture changes nothing.

W10 (`RMTN_W10_ONLY=1`, timing `RMTN_W10_TIMING=1`): a roof with a 3 m slot over fog, the sun above it and a shadow map on the sun: the
fog on the line of the shaft is 2.7 times brighter (linear light) than 6 m to either side at Low, Medium and High, and 0.99 times
when the sun has no shadow (and when the march ignores the shadow map); a spot's cone is 45 times brighter than the fog beside it with
`scatterLights` and 0.98 without; looking 20 degrees from the sun the fog is 3.3 times brighter than looking away, and 1.00 with
`anisotropy` 0; volumetric Off and aerial disabled with every other field set are pixel-identical (0) with a scene that never set them; the marched fog
differs from the analytic one by 0.23 levels on average (p99 1.3; 0.23 for height fog), and 11 levels with the analytic fog left on (double
fog); aerial perspective (above); a frame alone, after others and twice is identical, and so are two frames of a static scene; the noise
in a flat part of the shaft is 0.5-0.6 levels about its local mean (2.3 at Low with the filter off). A card of 0.3 m clear and solid stripes over half the shaft: the fog between its edges is as clean as the free half (high-frequency rms 0.67 / 0.66 / 0.50 levels against 0.78 / 0.83 / 0.64 at Low / Medium / High, ratio under 1); a canopy of one alpha-tested card with a 3 m clear strip makes a shaft 3.7 times brighter than the fog beside it (1.10 when the card casts no shadow); sea pixels over a submerged shelf are identical (0.0 levels) with the aerial perspective on and off (14.8 with the sea clip removed). Each was broken to see it fail:
the shadow map ignored, the phase made 1, the fog drawn twice, no altitude dependence, no filter, the cards not casting, the sea clip removed.

W5 (`RMTN_W5_ONLY=1`): same seed same bolt (768 segments, same-seed pixel diff 0), another seed another; the frame is
brighter only inside the strike's window and equal to the dark scene before the leader and after the last restrike, with at
least two bright frames per strike; rate-based strikes deterministic and within 20% of the rate over 600 s; thunder at
distance / 343; an in-cloud strike brightens the cloud pixels near it several times more than far ones (and not with
`cloudLight` 0); the rainbow's primary fitted as a circle round the anti-solar point (radius 42 +- 1 degrees, centre within
0.5), the secondary at 51 +- 1.5, red outside then reversed (the bow is light added in proportion to the sky behind it, translucent, the secondary 0.4 of the primary and paler; the Alexander band darkens); no bow with the sun in front, no rain or the sun at 60 degrees
(pixel-identical with it off); one storm cell: the top wider than the middle, shifted downwind (and the other way in the opposite
wind), core width over height at least 0.5, cloud in the core column up to the anvil at least 95%, at most 4 cells in 60 x 60 km (each with a synthetic pillar-and-plate or the removed 9 km layout as the control that fails), cells of different seeds with different silhouettes; a frame alone equals the frame after others and off means off.

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
nearest overhead cloud carries no more detail than a cloud at 5 km (the Laplacian
measure is 0.04 against 0.06 and the detail switch does not change it); and the row-over-column energy check
for bands in height barely separates its jitter-off control. The extinction is 0.045 per metre at density 1; the
density has a no-slope toe, `d * d * (2.5 - 1.5 d)`, so edges fade over metres instead of a pixel; each fine
step samples at its own offset, so the step lattice does not ring; the coverage map is read through an eased
bilinear fetch and at no coarser than level 2.

