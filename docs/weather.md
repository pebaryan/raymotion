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
| `clouds.type` | enum | `Cumulus`, `Stratus`, `Cumulonimbus`, `Deck`: for volumetric clouds, not drawn yet |
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
* **Particles in the box.** `rain3d` and `snow3d` fill a fixed box; a strong wind carries the drops
  sideways out of it. Give the emitter a wider area than the view.
* **Objects that move in the wind on their own** (cloth and rope in `physics::`, 2D `flora2d::`
  and `fx::rain`) have their own wind settings and are not driven.
* **The CPU `Water::height` of `scene.water`** is the water you wrote. Use `g3d::waterOf(scene)`
  for the surface that is drawn (a boat riding the waves).

## Script

The scene descriptor's `weather` object (docs/scripting.md) takes a `preset`, optionally `to` and
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
