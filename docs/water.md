# Water

Still water in a 3D scene: a lake, a pool or a river that mirrors the sky and the shore, with a soft
edge, lace foam and ripples that follow the wind. This page is the short guide. The full state, with
every field, is in [weather.md](weather.md) ("Water that reflects the scene", "Lakes, rivers and
pools", "Wakes, ripples and floating objects", "Ocean to the horizon"); rain, wet ground and storms
are there too.

## One line

The `water-body` prefab draws a lake in a meadow, lit and reflecting a sky, with a camera looking
over it:

```
prefab::use(0, 120, "water-body", "{\"box\":[0,0,1280,720],\"look\":\"breezy\"}");
```

`raymotion prefabs` lists its props. `look` is `calm` (a mirror), `breezy` (a rippled lake) or
`choppy` (a bay with waves of a few decimetres). `type` is `lake`, `pool` or `river`.
`examples/scripts/weather_prefabs.as` uses it with the other three weather prefabs.

| Prop | Default | Meaning |
| --- | --- | --- |
| `box` | `[0,0,1920,1080]` | where the picture goes, pixels `[x, y, w, h]` |
| `type` | `lake` | `lake`, `pool` (a small one near the camera) or `river` (along `path`; a set bend if none is given) |
| `look` | `calm` | `calm`, `breezy`, `choppy` |
| `shore` | none | the outline, `[[x, z], ...]` metres, three points or more (the default is an irregular ellipse) |
| `path` | none | a river's course, `[[x, z], ...]` metres, two points or more |
| `width` | 6 | river width, metres |
| `flow` | 1.2 | river speed, metres per second along the path |
| `reflections` | true | the scene mirrored in the surface, and the bed seen through it; false is the sky only |
| `hour` | 15.5 | the sun's place (0..24) |
| `cover` | 0.4 | cloud cover, 0..1 |
| `size` | 60 | the landscape's width, metres |
| `trees`, `grass` | 8, `lawn` | trees round the shore; the grass look (`lawn`, `meadow`, `none`) |
| `orbit` | 0 | the camera's turn round the water, radians a second |
| `fov` | 55 | the camera's field of view, degrees |
| `quality` | `film` | the sky's quality: `preview`, `film`, `high` |
| `seed` | 1 | the outline, the trees and the clouds |
| `ground` | `#5d6b3c` | the land's colour |

Water is flat at its level: waves are shading and `height()`, so a boat or a float rides them but the
shore line does not move. Heights are metres, times seconds.

## From C++

`rmtn/weather_prefabs.hpp` (namespace `rmtn::wx`) makes the same body without a prefab, to put in any
scene:

```cpp
#include "rmtn/raymotion.hpp"
using namespace rmtn;

void addLake(g3d::Scene& scene) {
    wx::WaterSpec spec;
    spec.look = wx::Surface::Breezy;              // calm, breezy or choppy
    spec.centre = { 0, -20 };                     // metres, x and z
    spec.radius = { 26, 14 };                     // the ellipse the shore wanders round
    spec.seed = 3;                                // the shore's irregularity (up to 25 per cent)
    scene.waterBodies.push_back(wx::waterBody(spec));
}
```

A river sets `path` (two points or more), `width` (metres) and `flow` (metres a second):
`spec.path = { {0, -5}, {12, -14}, {26, -12} };`. `spec.shore` replaces the ellipse with your own
outline. `spec.reflections = false` leaves the sky-only look; `spec.level` lifts the surface
(metres). For the scene's other water settings, set `Scene::water` (the sea) or edit the returned
`g3d::WaterBody` (`look`, `edgeSoft`, `foamWidth`, `shoreFoam`, `flowPeriod`, `narrowFaster`).

## From a script

A scene descriptor's `waterBodies` array takes the same shapes without the prefab:

```
{ "waterBodies": [ { "type": "lake", "outline": [[-20,-8],[20,-8],[20,-30],[-20,-30]], "level": 0 } ] }
```

The keys of a body (`type`, `outline`, `holes`, `path`, `width`, `level`, `fall`, `flowSpeed`, the water look keys such as `preset` and `reflections`,
`edgeSoft`, `foamWidth` and `shoreFoam`) are in [scripting.md](scripting.md). `wakes` and
`ripples` arrays, and an object's `ride` key, put boats, stones and floats on the same water.
`Scene3D.surfaceHeight(x, z, seconds)` returns the surface height at a point and time (the sea
and the bodies), so a script can place something on it.

## What the mirror and the bed show

* **The mirror shows only what stands above the water.** The reflection pass mirrors the scene about
  the level of the body being drawn (`WaterBody::level`, not the sea's `Water::level`) and the lit shader
  discards everything under that level, for meshes, instances, grass and trees alike. A pile's
  reflection starts at the waterline and its submerged part is never mirrored into the open.
* **What lies under the water shows through the surface**, not in the mirror: the march of the refracted
  ray reads the opaque scene, and the water absorbs it over the length of that ray.
* **A ray that finds no bed stands in the same way.** The march is in screen space and steps over
  anything that stands above the surface, so a bed hidden behind a deck or its rails, or beyond the
  march, used to fall back to the straight view absorbed over the much longer view path: a darker
  copy of the jetty on the lake and one hard line across it, where the marched hits stopped. A miss
  now takes the bed colour at the point the refracted ray would reach (or the straight view's when
  that point is hidden) and absorbs it over the refracted ray's length, as a hit would. A hit beside
  the edge of something above the water is a miss, so rails leave no brown fringe.
  Tests: `noJettyGhost`, `noSeamAcrossTheLake` (tests/weather_tests.cpp).
  A pool seen at a grazing angle used to show two tones either side of the line; it is one now.

## Knobs

| Knob | Unit | Default | Where |
| --- | --- | --- | --- |
| `WaterBody::level` | m | 0 | lake height |
| `WaterBody::edgeSoft` | m | 0.3 | the surface fades out this far above the bed |
| `WaterBody::foamWidth` | m | 0.4 | the foam band's depth reach (the band is 0.4 to 1 times this with the energy) |
| `WaterBody::shoreFoam` | 0..1 | 1 | scales the foam |
| `WaterBody::flowSpeed` | m/s | 1 | river current along the path |
| `WaterBody::flowPeriod` | s | 1 | the flow's cycle |
| `WaterBody::fetch` | m | the body's size | the wave field's reach; a pond does not grow ocean waves |
| `Water::reflections` | enum | `Scene` (prefab) | `Off`, `Scene`, `Half`, `Quarter`: the share of the frame the mirror pass draws |
| `Water::reflectionDistort` | 1 | 1 | how far the waves bend the reflection (0 a flat mirror) |

Wave height of the prefab's looks (peak to trough over a 400-point scan, from the tests): calm about
0.22 m, breezy 0.53 m, choppy 0.72 m.

## Open limits

These are from the plan's Done notes for W6 to W9:

* The screen-space reflection march leaves a hatched band on the top rim of a submerged rock. Line
  art, billboard particles and splats are not reflected.
* Bodies are flat at their level, seen from above the water only, at most 32 per scene. The sea
  writes no depth, so an aerial pass over it needs the sea's own haze.
* The storm sea (about 18 m/s) still shows a few large flat foam sheets and is not yet convincing.
  Breaking-wave shape, a flat-mirror reflection on the curved sea, and foam that reads as lace at
  grazing view are open.
* geo3d has no water polygons, so only city water (`city::Look::realWater`) converts to bodies.
* The prefab draws one lake in front of the camera. A scene with several bodies, an island, a harbour
  or boats is built from the engine parts above; see `examples/scripts/river_valley.as`,
  `harbour_wake.as` and `lake_reflections.as`.
