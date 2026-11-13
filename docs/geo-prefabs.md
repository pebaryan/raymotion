# Map prefabs

Two built-in prefabs draw a whole map from props. They are registered by `runCli`
(`geo::registerPrefabs()`), so they work in C++ compositions and in scripts:

```cpp
prefab::use(roots, 0, 90, "geo-flyto", Json::parse(R"({"from": "world", "to": "KEN"})"));
```

```angelscript
prefab::use(0, 90, "geo-flyto", "{\"from\":\"world\",\"to\":\"KEN\"}");
```

Both fill `box` (node px) and draw the base map themselves. `delay` and `duration` are
frames, local to the instance. `ocean`, `land`, `coast`, `border` and `graticule` set the
base map's colours. `projection` is any name `geo::Projection::named` knows
(`equalEarth`, `orthographic`, `mercator`, `conicEqualArea`, ...).

`examples/scripts/map_prefabs.as` uses both.

## geo-route

A route along great circles that draws itself on, with a dot and an optional label at
each stop. Labels keep off the line.

| Prop | Default | Meaning |
| --- | --- | --- |
| `stops` | Jakarta, Vienna | `[[lon, lat], ...]`, at least two |
| `labels` | `[]` | a name per stop, in order; fewer is fine |
| `projection` | `"equalEarth"` | |
| `fit` | `"route"` | `"route"` frames the route, `"world"` shows the whole map |
| `box` | `[0, 0, 1920, 1080]` | `[x, y, width, height]` |
| `padding` | `160` | px kept clear round the route when `fit` is `"route"` |
| `delay`, `duration` | `0`, `60` | when the line starts and how long it takes |
| `color`, `width` | `"#ffc45c"`, `4` | the line and the dots |
| `dash` | `[]` | on/off lengths; empty is a solid line |
| `arrow` | `false` | an arrowhead at the tip instead of a dot |
| `textSize` | `28` | labels |

## geo-flyto

A flight from one place to another. The destination is then picked out and named.

| Prop | Default | Meaning |
| --- | --- | --- |
| `from`, `to` | `"world"`, `"AUT"` | `"world"`, or a country by ISO code or name |
| `projection` | `"equalEarth"` | a globe (`"orthographic"`) turns to face each place; a world map only zooms |
| `box` | `[0, 0, 1920, 1080]` | |
| `padding` | `260` | px kept clear round a country |
| `delay`, `duration` | `0`, `60` | the flight; the highlight and the name follow it |
| `zoomOut` | `0` | how far the map pulls back halfway: `0` picks it from the distance, `1` is none |
| `highlight` | `"#e25d4e"` | the destination's fill |
| `label`, `textSize` | `true`, `40` | the destination's name |

Unknown countries, projections and malformed props throw when the composition is built,
so `list` reports them.
