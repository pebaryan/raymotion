# Runtime compositions

## Scope

The `.as` backend is the embedded scripting surface for Raymotion. Its intended role is a
full authoring path across Raymotion's public engine features, while preserving deterministic
frame rendering. Bindings cover vector art, SVG, charts, diagrams, chemistry, 3D scenes,
animation, layout, audio/video and asset generation. Native C++ remains the reference API while
parity work continues; the binding notes below describe supported script entry points and
their limits. See the [API parity audit](reviews/script-api-parity.md) for coverage status and the field-by-field plan.

## Embedded scripts

Build the host once, then edit and run scripts without rebuilding raymotion.
`examples/scripts/hello.as` is a complete example (two overlapping scenes, props, easing
and seeded randomness); release ZIPs include it as `scripts/hello.as`:

```sh
build/raymotion_script preview hello-script --script examples/scripts/hello.as
```

For your own script:

```sh
cmake -S . -B build -DRAYMOTION_BUILD_EXAMPLES=OFF
cmake --build build --config Release --target raymotion_script
build/raymotion_script list --script path/to/scene.as
build/raymotion_script preview my-scene --script path/to/scene.as
build/raymotion_script still my-scene --script path/to/scene.as --at 30
build/raymotion_script render my-scene --script path/to/scene.as --jobs 2
```

On multi-configuration Windows builds use `build/Release/raymotion_script.exe`.
The showcase and other applications using `runCli` accept the same `--script` option.
Repeat the option to load multiple files; composition names must be unique.
Relative paths resolve from the current working directory, including worker processes.
Each invocation reads the current source. An open `preview` window reloads when a script
(or a `--props @file.json`) is saved: the composition is loaded again in the same window
and the playhead stays where it is, also when the size, duration or soundtrack changed.
A script that does not compile, or throws while drawing, is reported on a band in the
window and on stderr; the last version that loaded keeps playing until the next save.
`--no-reload` turns this off. Changed images, fonts and other assets are not picked up
(they are cached by path); restart the preview for those. A native `.cpp` composition is
compiled once and does not reload.

The first backend uses [AngelScript 2.38.0](https://www.angelcode.com/angelscript/),
an embedded, statically typed language with C++-like syntax. Scripts compile to VM
bytecode in memory; no external compiler, linker or DLL is needed at runtime.
This is a supported C++-like subset API, not an interpreter for arbitrary existing
C++ files. Native compositions keep the complete C++ API.

## Script contract

Each `.as` file defines `void configure(Composition &inout c)` and either
`void draw(int frame)` or named scene functions registered by
`scene(start, duration, "functionName")` in `configure`.

`Composition` exposes `string name`, `int w`, `h`, `fps`, `duration`, `supersample`,
`statementBudget`, `uint background` (RGB hex, e.g. `0x101820`), and `int backgroundAlpha` (0..255, default 255). Set a name and
positive duration.
Names allow letters, digits, hyphens and underscores. Supersample is 1, 2 or 4.
Scenes must lie within the composition duration. Overlaps draw in registration
order. A scene receives its **local frame**: frame 0 at its start. If no scenes are
registered, `draw` spans the whole composition.

`configure` is CPU only, runs during registration, and runs again before use with
input props. It must produce stable names. Drawing from `configure` raises a script
exception. `list` needs no graphics context and creates no assets or output files.

Scripts support typed locals, expressions, conditionals, loops, functions, strings,
enums, and AngelScript's local objects. There are no C++ includes, macros, STL
templates, raw pointers, or automatic access to native engine headers. Global
variables (including global constants) are disabled so frames cannot accumulate
script state. Use local constants, helper functions or enums instead.

## Bindings

All coordinates and lengths are in composition pixels; colors are `uint` RGB hex.
Drawing respects the native node context. Optional alpha defaults to 1.

```cpp
draw::rect(x, y, w, h, color, alpha = 1);
draw::roundRect(x, y, w, h, radius, color, alpha = 1);
draw::circle(x, y, radius, color, alpha = 1);
draw::arc(x, y, radius, width, fromDegrees, toDegrees, color, alpha = 1);
draw::ring(x, y, radius, width, color, alpha = 1);
draw::sector(x, y, radius, fromDegrees, toDegrees, color, alpha = 1);
draw::blob(x, y, radius, amplitude, lobes, phaseDegrees, color, alpha = 1);
draw::star(x, y, outerRadius, innerRadius, points, rotationDegrees, color, alpha = 1);
draw::line(x1, y1, x2, y2, width, color, alpha = 1);
draw::gradient(x, y, w, h, topLeft, topRight, bottomRight, bottomLeft, alpha = 1);
draw::fillPoly(array<float> points, color, alpha = 1); // alternating x,y values
draw::image(file, x, y, w, h, alpha = 1);             // uses media::resolve search paths
draw::push(x = 0, y = 0, rotation = 0, scale = 1, alpha = 1);
draw::pop();                                           // must match a push
draw::text(text, x, y, size, color, alpha = 1, anchorX = 0, anchorY = 0,
           face = FONT_UI);
```

`draw::push` nests translation, rotation, uniform scale and opacity for following
calls; `draw::pop` restores the previous transform. `draw::offscreen(alpha, functionName)` renders a named `void function(int frame)` into a transparent full-canvas layer and composites it over the scene at `alpha`. `draw::frameRate()`, `draw::canvasScale()`, and `draw::canvasSize(widthHeight)` query the active render context; the size output is `[width, height]` in composition pixels. Polygons accept 3–1024 vertices. `draw::fillWinding(addTriangles, subtractTriangles, color, evenOdd)` fills the nonzero or even-odd winding of flattened triangle vertex arrays; each triangle is six floats (`x,y` per vertex). Either list may be empty, and each is limited to 100,000 triangles.
Images load through the engine's image resolver on first draw. Font choices are
`draw::FONT_UI`, `draw::FONT_UI_BOLD`, `draw::FONT_MONO` and
`draw::FONT_MONO_BOLD`; fonts load inside the drawing callback. Anchors range from
0 (left/top) to 1 (right/bottom).

`examples/scripts/lower_third.as` is a complete composition using transforms, a four
corner gradient, an SVG image, a filled polygon, and UI/monospace font choices.
Its source art is `assets/images/script-lower-third.svg`.

Props are accessible in configure and drawing:

```cpp
props::number("width", 1280);  // double
props::text("title", "Hello"); // string
props::flag("visible", true);  // bool
```

Pass them with `--props '{"title":"New title"}'` or `--props @props.json`.
Missing keys use the supplied fallback; wrong types produce a diagnostic.
`props::defaults(jsonObject)` publishes composition defaults in the same way as native
`Comp.defaultProps`; user overrides are merged over them before `configure` runs. Fallback
arguments remain useful for optional props. The CLI `props` command reports the published object.

Math includes the standard AngelScript math addon (`sin`, `cos`, `sqrt`, `pow`,
etc.), plus `clamp01(t)`, `ease(kind, t)`, `rnd01(seed, index)` and
`rrange(seed, index, low, high)`. Easing constants match native `E_LINEAR` through
`E_BOUNCE`. `anim::spring` provides the native spring solver and
`anim::interpolate` maps values across key ranges with extend, clamp, identity or wrap
extrapolation; mapping forms require matching input/output arrays and a strictly increasing input range. Matching `array<double>` ranges preserve native double precision; the float-array overload remains available. `anim::interpolateCurve` accepts named or parameterized curves for the same mapping, including matching double-array ranges. `anim::interpolateFunction` also accepts double-array ranges. The `interpolateColors` overloads and their callback variants accept double input ranges. `anim::interpolateFunction` accepts a named `double easing(double t)` callback for custom native `Easing` behavior; call it from a draw callback, and keep its output finite. Callback execution uses the composition statement budget. `anim::curve` exposes in/out/in-out quad, cubic, sine, circle, expo and bounce
curves plus parameterized poly, back and elastic curves; `anim::bezier` evaluates CSS cubic
beziers. `anim::interpolateColors` writes RGBA channels to a float array. Its `array<uint>`
overload accepts RGB hex colors; its `array<float>` overload accepts flat `[r,g,b,a,...]`
channels for transparent color keys. Both overloads accept an easing constant. `anim::interpolateColorsFunction` provides the same overloads with a named `double easing(double t)` callback, so custom easing also works for RGBA color interpolation.
`randomVector2(seed, salt, index, xy)` exposes native deterministic `rvec2`; `mixVector2(ax, ay, bx, by, amount, xy)` exposes native vector interpolation. `mixRgba(a, b, amount)` and `rgbaWithAlpha(rgba, multiplier)` expose native color interpolation and alpha scaling. Both functions accept and return packed RGBA `0xRRGGBBAA`; interpolation amounts may extrapolate like the native helpers. `anim::trackFloat`, `trackVector2` and `trackColor` expose native per-key `TrackFloat`, `TrackVec` and `TrackColor` sampling. Supply matching float or double `times` and `easingIndices`; double times preserve the native key-time precision. The easing at key i shapes the segment ending at key i, and the first easing entry is unused as in the native track structs. Scalar/vector values use float arrays; colors use packed RGBA `array<uint>`. Keys are capped at 4096 and times must be strictly increasing. Empty tracks return the native defaults (0, [0,0], or opaque white). `anim::measureSpring` reports settle frames. `layout::row` and `layout::column` accept alternating fixed-size/flex-weight
pairs; `layout::grid` accepts column and row counts. Each writes flattened
x,y,width,height tuples into a caller-provided `array<float>`. `layout::fill`, `inset`,
`center` and `place` provide the native rectangle helpers; `layout::wrapText` returns UTF-8
safe wrapped lines, `layout::measureText` measures the block and `draw::textBox` draws it with
JSON font, alignment and line-height options.
Use seeded randomness and absolute expressions of the frame. JSON seed fields preserve the native width: uint32 seeds accept exact numeric values from 0 through 4294967295 or decimal strings; uint64 seeds accept exact JSON integers through 9007199254740991 or decimal strings for the full range.
Scenes can be connected with `sceneTransition(fromIndex, toIndex, frames, kind, direction,
ease)`, matching native fade, slide, and wipe transitions with selectable built-in easing.
`sceneTransitionFunction(fromIndex, toIndex, frames, callback, kind, direction)` uses a named
`double easing(double progress)` callback to match native arbitrary `Transition::easing`; its
output must be finite and it runs under the composition statement budget. See
`examples/scripts/transition_authoring.as`.

Vector paths support `vg::path`, `parse`, `line`, `polyline`, `smooth`, `regular`, `rect`,
`circle`, `ellipse`, `arc` and `star`, plus path building, affine copy transforms, trim,
reverse, bounds and measurement. `Path.contourCount`, `contourStartX/Y`, `contourClosed`, `segmentCount`, and indexed segment kind/control/end-point queries expose the native contour model. `vg::morph` creates deterministic path morphs, and
`flatten` exports contour point ranges; `vg::strokeDashed` exposes dash patterns, offset,
joins, caps and miter limits. Paths still
support `transformed(xFormula,yFormula)` with the same x/y math expressions used by plots.
The formulas run on each path control point and must return finite coordinates. `Path.mapped(callback)` also accepts a named `string fn(double x, double y)` callback returning JSON `[x,y]`; it runs synchronously on each control point, including during `configure`, and must return finite coordinates within the standard callback bounds. Paths also
support direct fill/stroke/write plus `PathStyle`-based composite drawing and reveal. Styles
can be built with `vg::style`, `filled`, or `outlined`, animated with `vg::mix`, and configured
for fill rule, stroke join/cap, miter limit, and dash pattern. `vg::loadSvg` and `parseSvg` expose drawing
dimensions, shape ids, paths and fill/stroke styling for per-shape composition; `SvgDrawing.dash(index, pattern)` reads a shape's exact stroke dash lengths, and `dashOffset(index)` reads its offset. See
`examples/scripts/path_authoring.as` and `examples/scripts/svg_drawing_data.as`.
`space::hyperbolic::path(xyPoints, callback, closed, clipX, clipY, clipW, clipH)` also maps sampled disk or half-plane points into a clipped vector `Path` through a named `string fn(double x, double y)` returning JSON `[x,y]`. It accepts 2..32,768 points; a zero clip size disables clipping. See `examples/scripts/hyperbolic_path.as`.
`draw::arc`, `ring`, `sector`,
`blob`, `star` and `polyStroke` cover the procedural primitives.

Charts use `charts::table()`, `charts::csv(file)`, or `charts::parseCsv(text)`. `ChartData`
exposes category and series counts/names, series color, individual values, and `axes()` for
the table's computed plot ranges; `charts::mix`
uses the native table interpolation for changing between matching datasets. Add labels with
`charts::category`, data with `charts::series`, then draw bars, lines, areas, stream graphs, heatmaps, funnels,
waterfalls, pie charts or radar charts. Chart styles can be supplied as JSON with
`charts::style`, `pieStyle`, `radarStyle`, `waterfallStyle` and `heatmapStyle`; common options
include plot box, y range, palette, axes, labels, grid, legend, values and text sizing.
Statistical helpers expose `summarize` (five-number summary, mean and outliers), sampled
Gaussian `density`, automatic or fixed-range `bin`, `niceRange`, `ticks`, and native color
lookups: categorical `paletteColor(index)` and continuous `viridis(progress)` / `diverging(progress)`.
Use the `array<double>` overloads of `summarize`, `density`, `bin`, `series` and `candles`,
plus the double error-array overload of `errorBars`, to preserve
native double precision; the original float overloads remain available. Numeric sample/query
arrays are capped at 8,192 values and chart series at 2,048 values. Density returns one value
per query point. Bin output is `[low, width, count, ...bin values]` and can be `array<float>`
or `array<double>` with the float-input overload; the double-input overload returns doubles.
`charts::boxplot`, `charts::violin` and `charts::parallel` draw statistical and multivariate
views from the same table. `charts::gauge` draws a standalone numeric gauge. Hierarchy
strings (`path, value` per line) drive `charts::sunburst` and `treemap`; flow strings
(`from, to, value` per line) drive `charts::sankey` and `chord`. Their native layout and
label options are available through each call's style JSON. See the chart authoring,
extended and hierarchy/flow examples in `examples/scripts/`. Numeric axes expose
`charts::histogram`, `bubbles` (point objects as JSON), `candles` (flat OHLC groups), and
`errorBars` (flat x/y pairs plus errors); axis ranges and native style values are set with
style JSON. See `examples/scripts/chart_numeric.as`.
`charts::graph`, `parametric`, `polar`, `contour`, `field`, `area`, `riemann` and `tangent`
accept formula strings; `scatter` takes flat x/y pairs. Formulas support
`x`, `y`, `t`/`theta`, `pi`, `e`, arithmetic, powers, and common trigonometric, logarithmic,
rounding, min/max functions. Their style JSON can instead name script functions: `function`
uses `double fn(double x)` for graph, polar, area, Riemann and tangent; area can also set
`lowerFunction`. Parametric uses `function` with `string fn(double t)` returning JSON `[x,y]`;
contour uses `function2D` with `double fn(double x, double y)`; field uses `vectorFunction`
with `string fn(double x, double y)` returning `[vx,vy]`. Callback names are limited to 128
characters and executions use the composition statement budget. Scalar finite results must be
within +/-1e9; 1D scalar callbacks may return NaN or infinity, which graph uses to break a curve.
Vector results must be finite and within +/-1e9. Formula arguments can be empty when the matching callback is
specified. Axis ranges, contour levels and reveal settings use the same style JSON.
See `examples/scripts/chart_functions.as` and `examples/scripts/plot_calculus.as`.
`charts::axes(x,y,width,height,xmin,xmax,ymin,ymax)` and `charts::numberLine(x,y,length,xmin,xmax)`
return native `Axes` handles. Their `mapX/mapY` and `unmapX/unmapY` methods convert between
data coordinates and composition pixels; `sx`, `sy` and `originX/originY` expose the native
scale and clamped origin. Range limits, automatic tick spacing, tick counts and indexed tick
values are also available through `xMin/xMax`, `yMin/yMax`, `xTick/yTick`, `xTickCount/yTickCount`
and `xTickAt/yTickAt`.
Call `Axes.draw(progress, styleJson)` from a draw callback to render those same axes with native
ticks, labels, grids, arrows and reveal behavior.
`charts::label(tex,x,y,anchorX,anchorY,size,color)` exposes standalone native TeX math labels.
`charts::colorbar` renders a numeric color scale, and `charts::legend` accepts a JSON array of
`{"name":"...","color":"#RRGGBB"}` entries. Heatmap, colorbar, contour and parallel style JSON
accept `"colorFunction":"name"` to use a global `uint name(float normalizedValue)` callback returning packed `0xRRGGBB`, or `"colorFunctionRgba":"name"` to use a global `string name(double normalizedValue)` callback returning `#RRGGBB` or `#RRGGBBAA`. Inputs are clamped to 0..1. The callbacks run during drawing under the normal script statement budget; the RGBA form preserves alpha for native `Color(float)` palettes. `sankeyCsv` and `chordCsv` load native flow CSV
files directly, with the same style options as their inline flow variants.
Diagram input supports the native parser formats
through `diagram::draw(source, format, box, frame, theme)`. `diagram::newGraph()` creates a mutable
`DiagramGraph`; `diagram::parseGraph(source, format)` or `loadGraph(file)` creates one from native
source or a file, while `parseGraphAdvanced(..., optionsJson)` accepts RDF parser options. Graph
handles expose node, edge and group counts and indexed fields, add operations, common
label/shape/position/color/arrow/stroke/layout edits, node and route data, and `toJson()`.
`setNodeStyleJson`, `setEdgeStyleJson`, and `setGroupStyleJson` apply partial native style
overrides (`fill`, `stroke`, `text`, `strokeWidth`, `dash`); a field set to `null` clears that
override and restores the diagram theme value. `nodeGroup`, `nodeKind`, `edgeKind` and the
`*StyleJson` getters read the mutable graph values; each style getter returns RGBA channel arrays
and explicit `null` for unset overrides. `edgeLabelPosition(index, xy)` returns whether the native
optional label position is set and writes its coordinates when present. In
the drawing callback, `sizeLabels(theme, styleJson)` and `layoutNodes(optionsJson, hint)` expose native
text sizing and graph layout; pass a prior `DiagramGraph` as `hint` to seed matching node positions
when relaying an updated graph. Query positions, label sizes, group bounds and route points, then
call `freezeLayout()` to preserve the result in `toJson()`. Pass `graph.toJson()` to
`diagram::drawAdvanced(..., "json", ...)` to render a graph assembled by script. JSON graph
parsing accepts all native node shapes, arrow kinds, edge stroke kinds and group definitions.
`DiagramShape` exposes every native shape as a `SHAPE_*` enum, including alternate
parallelogram/trapezoid, start/end state, task, event, gateway, data object/store and annotation;
JSON graph nodes accept the corresponding lower-case shape names.
`diagram::stateCount(source, format, optionsJson)`, `stateGraph(...)` and
`nodeRect(source, format, id, frame, x, y, w, h, out rect, optionsJson)` query timeline morph states and
frame-specific node geometry from the same advanced action script.
`diagram::drawAdvanced` accepts
an options JSON object with native theme presets and style overrides, layout controls, and
ordered timeline actions: build, reveal, hide, highlight, unhighlight, dim, trace, flow, focus,
and morph. RDF formats accept `rdfOptions` (`literalNodes`, `typeEdges`, `maxLiteralLength`,
`language`) for Turtle, N-Triples, and JSON-LD parsing. `examples/scripts/diagram_advanced.as` exercises the animated graph path. The
first-class `diagram::make(graph, optionsJson)` returns a native `Diagram` handle. Its `build`,
`reveal`, `hide`, `highlight`, `unhighlight`, `dim`, `trace`, `flow`, `focus`, and `morph` methods
edit the timeline; `states`, `graph(state)`, and `nodeRect` query it. Draw it with
`diagram::draw(handle, frame, x, y, w, h)`. The constructor options use the same `theme`,
`themeStyle`, `layout`, `maxZoom`, and `margin` fields as `drawAdvanced`; timeline actions are
added through methods on the handle. `themeStyle.fontFile` / `fontCandidates` and
`boldFontFile` / `boldFontCandidates` select custom regular and bold faces. Optional
`fontBaseSize` and `boldFontBaseSize` set raster sizes (16..512); diagram labels are included
as the glyph sample so non-Latin text is rasterised. Custom font loading requires calling
`diagram::make` from a draw callback with an active graphics context.
chemistry namespace exposes `createMolecule`, `smiles`, `molfile`, `loadMolecule`, `pdb`, and `loadProtein`
constructors. `createMolecule(name, coords3d)` makes an empty editable molecule; `setName` and
`setCoords3d` change its metadata, `addAtom`/`setAtom` cover element, position, charge, isotope,
hydrogens, aromaticity and PDB atom/residue fields, and `addBond`/`setBond` cover endpoints,
order, aromaticity and stereo. `addAtom` returns the new zero-based atom index; `addBond` returns
the bond index. Atom and bond counts are bounded at 100,000 and 200,000, respectively, and
coordinates are limited to finite values within +/-1e7. `ChemMolecule` provides name, formula, mass, radius, atom/bond properties, neighbors,
rings, substructure matches, hydrogen expansion, 2D depiction layout, 3D embedding, and
`atomAt(index, x, y, w, h, out)` for positioning callouts at a fitted atom coordinate.
`chem::depict(molecule, optionsJson)` creates a reusable `ChemDepiction` from any molecule
handle, including loaded MOL/SDF or PDB data. Its `build`, `highlight`, `unhighlight`,
`setRotation`, `setFont`, `draw(frame, x, y, w, h)`, and `atomAt` methods expose the native
depiction actions directly; draw it from a composition draw callback. The options accept the
same `style` fields as `drawSmilesAdvanced`, and `setFont(FontAsset@)` selects a loaded custom
face (load the font inside a draw callback). `molecule()` returns a snapshot of the processed
molecule with folded hydrogens and depiction coordinates.
`ChemProtein` exposes identity, residue metadata, and conversion to a molecule. `createProtein(id, title)`
creates an editable protein record; `setIdentity`, `addAtom`/`setAtom`, and
`addResidue`/`setResidue` expose the native protein fields, including coordinates, PDB atom
metadata, residue sequence/chain/secondary structure, and C-alpha/oxygen atom indices. Atom and
residue counts are each limited to 100,000; residue atom indices must reference an existing atom
or be `-1`. Convert the result with `addChemProtein` to build a 3D cartoon. Chemical
structures can also be drawn with `chem::drawSmiles(smiles, box, frame)`. JSON 3D scenes accept
`chem::drawSmilesAdvanced(smiles, x, y, w, h, frame, optionsJson)` for native `Style2D`
controls and timeline actions. Its `style` object accepts `bondLength`, `lineWidth`, `fontSize`,
`ink`, `colorAtoms`, `carbonLabels`, `aromaticCircles`, and `highlight`; top-level `rotation`
rotates the fitted structure. `actions` supports `build`, `highlight` (atom indices and optional
color), and `unhighlight`, each with `at` and `duration`.
`chemMolecule` objects with either `smiles` or a MOL/SDF/SMILES/PDB `file`, selectable
ball-and-stick, space-filling, or sticks mode, and hydrogen/material controls. `chemProtein`
objects load a PDB file and draw a native cartoon with rainbow, chain, or structure coloring,
ligands, and geometry controls. See `examples/scripts/chemistry_scene.as`. These handles can
also build models into directly authored 3D scenes with `Scene3D.addChemMolecule` /
`addChemProtein` or the equivalent `Object3D` parent methods. Style JSON accepts the same
controls as `chemMolecule` / `chemProtein` scene descriptors. `chem::buildIn(Object3D@ model,
at, stagger, duration)` animates a built molecule or protein's child geometry in sequence.
Subtitle scripts can use `subtitles::when` / `frameOf` to align scene starts to spoken phrases and
`drawFileStyled(file, seconds, styleJson)` to control typography, alignment, outline, backing,
fade and karaoke colors. The default `drawFile` remains available. See
`examples/scripts/subtitle_style_card.as`. `media::video` and `media::audio`
add timed timeline cues during `configure`; video options include source trim, playback
rate, looping, volume and mute. `media::videoEnvelope` and `media::audioEnvelope` add a
piecewise-linear gain envelope using local-frame/gain pairs, with gain multipliers from
0 to 1. The `volumeFunction` optional last argument on audio/video cues accepts a global
`float name(int localFrame)` callback and matches native `volumeAt`; it cannot be combined
with frame/gain pairs. The callback runs during audio mixing with a fresh script statement budget
and must return a finite multiplier. `media::nestCue(parentSceneIndex, cueIndex)` attaches a
previously declared audio or video cue beneath a scene; its start and duration are local to that
scene, and it inherits the scene's transforms and timeline remapping. The cue index is its
zero-based declaration order among `media::audio` and video cue calls.
`media::editAudioCue(cueIndex, optionsJson)` updates a previously declared audio cue during
`configure()`, matching the native mutable `AudioClip` fields: `file`, `startFrom`, `endAt`,
`playbackRate`, `loop`, `volume`, and `volumeFunction`. Use `volumeKeys` for an envelope instead
of `volumeFunction`; those two dynamic gain forms are mutually exclusive. The index uses the same
zero-based declaration order as `nestCue`, and must refer to an audio cue.
`media::videoStyled` and
`media::videoEnvelopeStyled` also accept the image fit
mode, alignment, and tint; `media::probe` returns width, height, fps, duration, and audio
presence through output parameters. `media::resolve`, `addSearchPath`, `ffmpeg`, and
`setFfmpeg` expose the media resolver and decoder configuration. `media::decodeAudio(file,
sampleRate, stereo)` fills an output `array<float>` with cached interleaved stereo PCM for
custom analysis or DSP; supported rates are 8 kHz to 192 kHz, with a 64 Mi-float output cap.
`media::writeWav(file, stereo, sampleRate)` writes finite interleaved stereo samples as 16-bit PCM from `assets()` only, keeping generated audio in the asset phase.
`media::videoAsset(file)` creates a reusable decoder handle with width, height, fps, duration,
and audio metadata. `VideoAsset.drawAt` selects a frame by seconds and supports fill/contain/cover/
natural sizing; `drawRegionAt` draws an explicit source rectangle for custom frame treatments.
`media::image(file)` returns a cached `ImageAsset` with source path, dimensions, vector/GIF
metadata, loop duration, and `frameAt(seconds)`. `ImageAsset.drawRegion(srcRect, dstRect, tint,
seconds)` exposes native texture-region cropping for stills, GIFs and SVGs. See
`examples/scripts/image_style_card.as`.
`media::loadPixels(file, svgWidth = 0)` decodes an image into an `ImagePixels` handle for
procedural image work. `pixel(x, y, rgba)` writes one RGBA pixel and `pixels(rgba)` writes the
flattened row-major image into `array<float>`; RGBA8 channels are normalized to 0..1, while HDR
channels retain their floating-point values. Decoding is capped at 16 Mi pixels and bulk output
at 4 Mi pixels.
An optional `void assets()` hook runs before build during still/preview/render (never for
`list`). `assets::writeText` and `assets::writeImage` create generated text/vector or RGBA
image files from script code. See `examples/scripts/generated_assets.as`.
`json::parse`, `object`, `array`, `nullValue`, `boolValue`, `number`, and `string` provide native JSON values without hand-built JSON strings. `json::Value` supports typed access, key/index lookup, ordered key iteration, object `set`, array `push`, recursive `merged`, equality and `dump`; `json::parseColor` parses native color syntax and returns packed RGBA. Parsing is limited to 16 MiB. Use `dump()` when passing dynamically assembled descriptors to APIs that accept JSON text.
The `synth` namespace adds procedural notes to the composition soundtrack in `configure`:
call `synth::enable()` and then add note events with start/duration in seconds, frequency,
amplitude, and an optional pluck envelope. This uses the native deterministic sine-partial
song renderer and requires no generated audio file. See `examples/scripts/synth_card.as`.
`subtitles::create()` makes an empty track in `configure()`; `track.add(startSeconds, endSeconds, text, italic = false)` inserts a cue in time order and returns its index; `setCue`, `removeCue`, and `clear` edit the track. This supports generated tracks without an intermediate SRT file. `subtitles::parse`/`load` continue to read existing SRT or WebVTT tracks. `tts::speak(lines, startFrame, volume, command, cacheDir, gap,
lead, wordsPerSecond, captions)` declares lines during `configure` and returns the voiceover cue
index. `tts::lineCount`, `lineText`, `lineFile`, `lineStart`, `lineSeconds`, `lineSpoken`,
`seconds`, `frames`, and `trackSrt` expose the native TTS script's line metadata, measured timing,
and subtitle track. Times are relative to the voiceover start and include the configured lead;
cached audio durations are measured, while uncached takes use `wordsPerSecond` estimates and
report `lineSpoken == false`; `lineFile` gives the deterministic cache path in either case.
Queries are available during `configure` and do not start speech synthesis. `frames` accepts
1..1000 fps; line and cue indices are zero-based and checked. A composition can declare up to
32 voiceover cues, each with 1..256 lines of at most 16 KiB. The script asset hook runs
the configured command (or the system voice), caches WAV takes, measures them, attaches audio
to the normal timeline, and optionally draws captions using the measured cue timing. A command
can use `{textfile}` and `{out}` placeholders. `list` does not synthesize audio.
`tts::nestCue(parentSceneIndex, cueIndex)` attaches a declared voiceover beneath a scene;
the cue index is the zero-based `tts::speak` declaration order, its start frame is parent-local,
and the measured voice/caption duration must fit that scene.
See `examples/scripts/tts_voiceover.as`.
`aud::analyze(file, options...)` returns an `AudioAnalysis` handle. It exposes level, bands,
frequency-range energy, onset, beat lookup/count, pulse, BPM, frame count and analysis
options (`fps`, `startFrom`, `bands`, `minHz`, `maxHz`, `range`, `attack`, and `release`). `spectrum(frame, out)` and `waveform(frame, points, window, out)` return the sampled
arrays; `beatTotal()` and `beatFrame(index)` expose the detected beat list. The handle can draw
waveform, spectrum bars, or radial spectrum; `aud::fromSamples` accepts interleaved stereo
floats for already-decoded material. The array overloads `aud::bars(values, ...)`,
`aud::wave(samples, ...)`, and `aud::radial(values, ...)` draw any script-authored sample or
spectrum arrays using the native drawing primitives (up to 16,384 values per call). See
`examples/scripts/audio_analysis_card.as`.

`rig::draw` accepts JSON looks, outfit and face styling, view settings, named clips, expression,
gaze and speech tracks, plus timed `wear` cues that swap the look with a configurable pop.
Named-shape SVG faces can be loaded from `faceSvg` or supplied inline with `faceSvgText`
(up to 2 MiB), using the native eye, pupil, brow, mouth and expression shape IDs.
Face descriptors also accept `blinks:[seconds,...]` and exact `mouths:[{"time":seconds,
"shape":"A"},...]` cues, where mouth shapes are `A` through `H` or `X`. Set `autoBlink:false`
when only authored blink cues should control the eyes. `faceSeed` accepts an exact uint32 value from 0 through 4294967295 or a decimal string. These feed the native `FaceTrack`.
Set `faceOverlay` to replace the built-in/SVG facial drawing with a procedural script callback. Declare `void drawFace(int frame, float x, float y, float width, float height, const string &in expression, const string &in previousExpression, float expressionMix, int mouth, int previousMouth, float mouthMix, float blink, float gazeX, float gazeY, uint skinRGBA)` (using the chosen function name). It runs in the puppet drawing context with the face box in composition coordinates. Mouth values are ASCII codes for `A`–`H` or `X`; `skinRGBA` packs bytes from most to least significant. The callback draws face features only; the rig still draws the head and skin.
Set `outfitOverlay` to a script function name to draw custom artwork over the procedural outfit texture. The callback signature is `void drawOutfit(int frame, float textureSize)`; draw commands run in the outfit texture context, with size in texture pixels. This maps the native `Outfit::over` layer and is available when rendering the 3D rig. Set `outfitSampleFunction` to a global `uint sampleOutfit(int region, float u, float v)` callback to provide native `Outfit::sample` colors for 2D puppet garments and 3D outfit textures. For alpha, use `outfitSampleFunctionRgba` with `string sampleOutfit(int region, float u, float v)` returning a hex color such as `"#RRGGBBAA"`; it is mutually exclusive with the RGB option. `outfitSize` sets the texture resolution (16..2048; use a smaller value when a script sampler is used). Region indices are torso=0, hips=1, left/right arms=2/3, and left/right legs=4/5; UV coordinates are in 0..1. Return packed `0xRRGGBB` from the RGB form. `softnessFormula` animates the native per-character softness callback; its formula receives
seconds as `x` and must return a value in `0..1`. `softnessFunction` accepts a named `double softness(double seconds)` callback instead; the two options are mutually exclusive and callback results must remain finite in `0..1`.
Clip cues use the same native action factory as `RigBuild.clipAction`, so clip specific
options such as `rate`, `energy`, `period`, `gait`, `seat`, and `bar` work in character
descriptors as well. Clip tracks can also use `{"name":"keys","keys":[...]}` with keyframe
times, joint rotation triples in degrees, and root offsets; joint names match the native rig
names (for example `head`, `shoulder-R`, `knee-L`). Function clips are available as
`{"name":"function","callback":"poseAt"}` in character descriptors or through
`RigMotion.playAction` / `layerAction` with options JSON `{"callback":"poseAt"}`. The global
callback signature is `void poseAt(const rig::RigBuild@ build, double seconds, array<float> &out
values)`, where `seconds` is clip-local time. Fill `3 + 3*build.jointCount()` values: root
offset xyz first, then Euler rotation xyz triples in `RigBuild` joint-index order. Values must
be finite and within +/-10000; execution uses the composition statement budget. See
`examples/scripts/rig_function_clip.as`. The direct `rig::RigBuild` / `rig::RigPose` API exposes body presets,
scaling, per-dimension proportions and build mixing, joint names and rest offsets, pose
edits/mixing/mirroring, named clip sampling,
forward-kinematics joint positions, palm positions, and native reach/plant/orient/hand/limb IK.
`RigPose.add(other)` composes additive rotations and root offsets; `scaled(weight)` applies a
weighted pose, matching the native `Pose` arithmetic for procedural overlays. `offset(x,y,z)`
sets root translation and `rootOffset(xyz)` reads it back.
Builds expose readable proportions, custom plan kind/variant/dimensions/traits, per-joint
parent index, right-side and grounded queries, native `walkSpeed` and `runSpeed` for matching stride travel, and `maskIncludes(mask, joint)` to query the native layered-motion masks.
`rig::motion(build)` creates a native `RigMotion` timeline: `followCrowd(simulation, agent)` applies the native crowd-speed-matched locomotion timeline to that build; `empty()` reports whether its base timeline has any clips; `play(clip, from, fade)` crossfades
base clips, `layer(clip, from, to, mask, fade)` composes a masked overlay, and `at(seconds)`
samples the blended result as a `RigPose`. `playAction` / `layerAction` accept JSON clip options;
`RigBuild.clipAction` samples an action directly. The full native clip catalog is available:
rest, hold, idle, walk, run, shamble, wave, point, cheer, jump, sit, shrug, seated, typing,
texting, calling, tablet, protest, shield-line, skate, scooter and keys. Options include
`energy` for idle, `joints` and `offset` for hold, plus `side`, `rate`, `period`, `seat`, `keys`,
`beat`, `drop`, `advance`, and `loop`; keyframe clips take `keys` entries with
`time`, optional joint rotation triples in `joints`, and optional root `offset`. Masks are
`all`, `upper`, `lower`, `left-arm`, `right-arm`, `arms`, and `head`.
`rig::animalBuild(species, specJson)` constructs custom quadruped, biped, snake, or fish
skeletons from native `QuadSpec`, `BipedSpec`, and `ChainSpec` measurements; for example,
`rig::animalBuild("horse", "{\"bodyLength\":0.8,\"neckPitch\":55}")`. `rig::wings(pose,
spread, flap, sweep)` applies native wing posing to a biped pose. Supported species are dog,
cat, horse, cow, giraffe, turtle, chicken, duck, trex, snake, and fish; `rig::animals(out)`
returns the native species list for data-driven tools.
`rig::skeletonBuild(planJson, basePreset = "toon")` builds a `RigBuild` around a custom
`Skeleton`: `planJson` has `kind`, optional `variant`, `height`, ordered `joints` and optional
numeric `dims` / `traits` maps. Each joint has `name`, `parent` (earlier joint index; root is
0), `rest:[x,y,z]`, and optional `right` / `grounded` flags. There may be 1..48 uniquely named
joints. The result works with native pose, IK, clip and motion queries. Character descriptors
can also put the same plan object under `build.plan`, alongside the usual build measurements;
custom body plans should register matching parts for their joints.
Animal builds also support `clipAction` / `RigMotion` clips `stand`, `gait`, `knuckle-walk`,
`hang`, `brachiate`, `ride`, `fly`, `glide`, and `paddle`. For `gait`, set `gait` in the options
JSON to `walk`, `trot`, `canter`, or `gallop`; `rate` sets cadence for gait and rate based clips.
`hang` accepts `bar` and `swing`, while `brachiate` accepts `bar` and `spacing`. Build queries
`gaitSpeed(gait, cadence)`, `gaitCadence(gait)`, `brachiateRate()`, and `flapRate()` expose the
native motion scale for coordinating travel with in-place animation.
`rig::drawPose(characterJson, pose, seconds, x, y, w, h)` renders a computed pose with the
character look and view settings from JSON. `rig::addToScene(scene, characterJson, pose, fps)`
adds the same computed pose as a native animated 3D character subtree. See
`examples/scripts/rig_card.as`. For a complete character timeline with its clips, timed look
changes, face tracks, outfit textures, and mounted carriers, use
`Scene3D.addCharacter(characterJson, fps)` or `Object3D.addCharacter(characterJson, fps)`;
both attach the native animated rig and return its root object handle.
For GPU instancing of a procedural rig, `Scene3D.addRigInstances` and `Object3D.addRigInstances` bake a JSON look and an array of named clips directly with native `rig::bakeAnimated`, then attach repeated instances without supplying vertex arrays. Each clip object has `name`, `frames`, `seconds`, and optional `options`; each instance has `position`, optional orientation, scale, shade, and either a `clip` index or explicit `clipStart` / `clipLength`. Clip indices map to their baked frame ranges automatically. `coarse` defaults to true for smaller meshes; use false for the full native tessellation. The native baker omits model-backed and translucent parts.
Custom model-backed rig parts can be registered during `configure` with
`rig::registerModelPart(name, slot, file, jointIndex, x, y, z, rotationX, rotationY,
rotationZ, scale, svg, width, height)`. The optional SVG and size provide the 2D puppet
fallback; the part can then be selected by name in a character descriptor. `rig::hasPart`
and `rig::partNames(slot, out)` inspect the shared part catalog. `rig::primitiveCount(lookJson, slot)` and `rig::primitive(lookJson, slot, index, fields, strings)` expose the exact native primitive decomposition for a look (empty slot means all parts). `strings` is `[name, jointName, modelPath, svgFallback, label]`; `fields` has 35 numbers in this order: kind, joint index, at xyz, rotation xyz, size xyz, squareV, squareH, taper, from, to, paint, color RGBA, region, v0, v1, tube joint indices a/b/c, radii r0/r1, softness, flat xy, roughness, depth2d, from2d. This exposes native part geometry and paint data for procedural tools. Slots are `head`, `hair`,
`hat`, `torso`, `hips`, `arm`, `hand`, `leg`, `foot`, `extra`, `hold`, and `ride`; joint
indices can be queried from `RigBuild.jointIndex` for custom animal skeletons.
`rig::registerModelPartLocked` adds the same model-backed part with fixed joint locks. Pass
parallel `lockJoints` (joint indices, obtainable from `RigBuild.jointIndex`) and `lockAngles`
(xyz Euler angle triples, in degrees) arrays; the right-side part mirrors the corresponding left-side lock joints as native rig
parts do. The arrays may contain at most `kMaxJoints` entries, and every angle must be finite
and within +/-3600 degrees. Dynamic `PartDef.make` factories that generate arbitrary
primitives from `(Build, Side, softness)` are scriptable with `rig::registerPart(name, slot,
callback)` or `rig::registerPartLocked(name, slot, callback, lockJoints, lockAngles)`. The
global callback signature is `void callback(const rig::RigBuild@ build, int side, float softness,
array<float> &out fields, array<string> &out strings)`. `side` is 0 for left and 1 for right.
Return zero to 256 native primitive records: `fields` contains 35 numbers per primitive and
`strings` contains five strings per primitive, using the exact order documented for
`rig::primitive`. The callback is evaluated when the part is decomposed for rendering, uses
the composition statement budget, and must be a pure function of its inputs. Registry entries
fail with a diagnostic if their script program has been destroyed.
See `examples/scripts/rig_custom_part.as` for a complete primitive factory.
`rig::outfitRegion(region, xywh)` and `rig::outfitFront(region, xywh)` return the native
normalized texture rectangles for `torso`, `hips`, `armL`, `armR`, `legL`, and `legR`.
`rig::characterPose(characterJson, seconds)` returns the native `RigPose` for the character timeline at that time, including mounted motion and joint locks. `rig::lockedPose(characterJson, seconds, pose)` applies the current look part locks to a manually composed pose. `rig::frameAt(characterJson, seconds, joint, matrix16)` returns the native joint transform
as 16 raylib matrix values, including look changes, pose, and mounts, for positioning props
or other scene objects against an animated character. `rig::project(viewJson, x, y, z, xy)`
projects a character-space point into the 2D puppet view; `viewJson` accepts `yaw`, `pitch`,
`scale`, `originX`, and `originY`, matching the view fields used by `rig::draw`.
`rig::visemes(text, from, to, times, shapes)` returns native approximate mouth-shape cues
as parallel time and shape arrays, so scripts can author exact face tracks.
`rig::faceState(characterJson, seconds, labels, values, skinRGBA)` samples the native face track.
`labels` is `[expression, previousExpression]`; `values` is
`[mix, mouthCode, previousMouthCode, mouthMix, blink, gazeX, gazeY]`; `skinRGBA` contains one
packed RGBA value. `rig::faceWeight(characterJson, seconds, expression)` returns the crossfade
weight for a named expression.
`rig::lookCount`, `lookChangeTime`, `lookIndex(characterJson, slot, seconds)`, and
`popAt(characterJson, slot, seconds)` expose the native look-change timeline and per-slot pop
progress. `rig::lookSlots(characterJson, seconds, slotKeys, partNames)` returns parallel arrays of resolved slot keys and active part names.
Custom skinned GLB rigs are exposed by `rig::loadSkin(file)`: its `build()` and `bindPose()`
provide the fitted native rig, and `matrices(pose, fields)` writes flattened raylib bone matrices
for `Object3D.poseBones(fields)`. `Scene3D.addSkinnedCharacter(characterJson, skin, fps)` uses
the native skinned character attachment path and animates the skin from the character's full
motion, look and mount timeline; the character build must match `skin.build()`.
Character descriptors can attach `mounts` cues. Each cue provides a nested `carrier`
character descriptor plus `type:"joint"` with a carrier `joint`, optional `at`, `rot`,
`upright`, `from`, `until`, `blend`, and `grips`; grips map `handL`, `handR`, `footL`, or
`footR` to a carrier joint and point. Use `type:"saddle"` for the native quadruped saddle
placement and rider grips. Mount carriers can have their own clips and face tracks.
`rig::addToScene` adds the nested carrier characters to the 3D scene with the rider; for 2D,
draw each carrier descriptor into the same viewport as the mounted character.

`math::tex(source, optionsJson)` exposes the native TeX-subset typesetter. Formula handles
provide width/ascent/descent, source text, named-part checks and bounds, a whole-formula outline `Path`, `piecePath(index)` for an individual glyph or rule outline,
anchored origin, and placed part bounds. They also expose the typesetter's ordered pieces with
glyph/rule kind, position, size, RGBA color, and nested part names for custom rendering.
`math::draw` renders a formula, `math::write` reveals its strokes, and
`math::transform(from, to)` plus `math::draw(transform, ...)` animates one formula into another.
Drawing accepts a look JSON with whole-formula `alpha` and per-part `colors` and `opacity` maps;
formula options accept `size`, `color`, `display`, and a MATH-table `font` path.

Additional bindings include `icons::draw` / `icons::logo` plus `icons::brands`,
`icons::languages`, `icons::glyph`, `icons::brandColor`, and `icons::title`; `flags::draw`
is paired with `flags::countries` and `flags::title`. The glyph query returns an editable
`Path`. `icons::draw` exposes the complete native look, including rounded/circle/square badge
shape, mark color, reveal, and corner radius. `icons::logo` accepts double frame values for
subframe spin timing. Deterministic
2D particle presets (`fx::drawPreset`) and configurable 2D emitters (`fx::draw` JSON: birth,
motion, area, lifetime, look, colour runs and additive blending). `fx::particleCount` and
`fx::particle` expose deterministic per-particle state for custom script-drawn looks; each sample
is `[index,age,life,u,x,y,vx,vy,size,rotation,alpha,r,g,b,a,random]`, and an out-of-range index
returns false with an empty array. Iterate the returned particles inside your scene callback to implement the native emitter `draw` callback
with script drawing primitives. For a direct native-style custom look, set `drawFunction` in the emitter JSON to a function name; the callback signature is `void drawParticle(int index, float age, float life, float u, float x, float y, float velocityX, float velocityY, float size, float rotation, float alpha, uint rgba, float random)`. It runs once per live particle in the emitter draw context and replaces the built-in look. See
`examples/scripts/particle_authoring.as`. Both 2D and 3D emitter descriptors also accept
`originFunction`, naming a global `string source(double birthFrame)` callback that returns JSON
coordinates (`[x,y]` or `[x,y,z]`). It computes the source at each birth in emitter-local frames
and is mutually exclusive with `origin` and `originKeys`, allowing arbitrary motion instead of
linear key interpolation. Calendar date parsing, arithmetic, weekday/month
formatting, frame-to-day interpolation (`cal::dayAt` with float or double frame/day pairs), and styled pages/months. Use `array<double>` when native double precision matters.
See `examples/scripts/calendar_authoring.as`. Also available are subtitle file playback,
audio waveform analysis, and 3D scenes described with JSON through `g3d::draw`. A 3D
scene can define camera, lights, primitive or model objects, materials, shadows, fog,
procedural sky, water, ink lines, and physics driven rope/cloth objects with constraints,
colliders, wind and solver parameters. Materials accept Standard, Toon, Gooch, and Hatch
shading, including their detailed lighting controls, object cut planes with a glowing edge, and
per-material ink lines. The scene descriptor exposes the full procedural sky, custom water
wave stack, depth of field, light shadow settings, scene ink controls, and generated studio
environments. Materials can also define a frame-addressed `dynamicMap` with `width`, `height`,
`every`, and `frames` containing `width*height` color strings or RGB(A) channel arrays. For a
vector canvas, use `dynamicMap: {width,height,every,supersample,drawFunction}` and name a global
`void paint(int frame)` function with `drawFunction`. That callback runs in the active script
drawing invocation and can use the `draw::`/`vg::` APIs to paint the texture. The raster form
backs native `DynamicTexture.make`; the vector callback backs `DynamicTexture.draw`. For
algorithmic pixels, use `pixelsFunction` instead of `frames` or `drawFunction` and name a global
`void pixels(int frame, array<uint> &out rgba)` callback. Fill exactly `width*height` entries in
row-major order, each packed as `0xRRGGBBAA` (six-digit `0xRRGGBB` values are also accepted as
opaque pixels). `every` controls how often the callback regenerates the image; this form also
backs native `DynamicTexture.make`. See
`examples/scripts/material_controls.as` and `examples/scripts/material_handle_config.as`. For direct native scene construction, `g3d::scene()`
returns a `Scene3D` handle; `g3d::box/sphere/plane/cylinder/cone/torus/knot` create reusable
`Geometry3D` handles, `g3d::material()` creates a configurable `Material3D`; `Material3D.configure(json)` applies the complete native material descriptor, including RGBA subfield colors, and scene/group
objects accept frame-derived transforms before `Scene3D.render(frame, viewport)`. Scene handles
also expose `environmentFunction(callback, width = 512)`, and JSON scene descriptors accept
`environmentFunction` plus `environmentWidth`. This maps native `Scene::environmentGenerator`
with a 2:1 image. The callback uses the same `void pixels(int frame, array<uint> &out rgba)`
signature as dynamic maps, receives frame 0, and must fill exactly `width*(width/2)` packed
pixels. It runs once when the environment is first generated; its width must be even and 16..2048.
Do not combine the JSON callback with an `environment` path or `studioEnvironment` object.
Scene handles also expose `modelAnimationCount(file)`, `modelAnimationIndex(file, name)` and
`modelAnimationFrames(file, index)` for choosing clips on `Object3D` models; query these in draw
callbacks, where models are loaded with the graphics context active. `Object3D.lookAt` applies
the native forward/up look rotation; `g3d::lookRotation(forwardX, forwardY, forwardZ, eulerXYZ,
upX = 0, upY = 1, upZ = 0)` returns the same Euler XYZ angles as a three-value output array
without mutating an object.
`Scene3D.visibleSphere` queries the frame camera frustum. When a draw uses a custom camera state with `Scene3D.renderCamera`, `Scene3D.projectCamera(cameraState, ...)` and `Scene3D.visibleSphereCamera(cameraState, ...)` run the matching native projection and frustum queries against that same 18-value state. Projection writes `[x, y]` to its output array and returns false for points behind the camera; visibility takes viewport aspect and a sphere in world coordinates.
Direct `Object3D` handles expose readback for position, rotation, scale, visibility, current-frame culling state (`isCulled()`), culling
sphere, name and model animation state, plus model-material and dual-quaternion flags. This lets
scripts inspect and update objects created from primitives, models, or nested groups.
`Object3D.up(functionName)` installs a frame-addressed native update callback for a
`void functionName(int frame)` script function. Inside it, `g3d::upObject()` returns the
currently updating node, so the callback can set absolute position, rotation, scale, visibility,
material and model pose from the supplied local frame. `geometry(handle)`, `model(file)`,
`particles(emitterJson)`, `instances(geometry, descriptorJson)`, and `splats(cloud, lookJson)`
also update the native content fields on that same object. `upObject()` is valid only during that
callback; derive values from the frame instead of accumulating state. `Scene3D.up(functionName)`
uses the same callback signature, and `g3d::upScene()` provides the active scene handle inside
it, allowing frame-derived scene and camera settings.
`modelPath()` reads a model object's source path. In draw callbacks, `modelMaterialCount()`, `modelMeshCount()` and `modelMeshMaterial(meshIndex)` inspect imported material slots and submesh assignments; `modelMaterialBaseColor(index)` reads a slot's diffuse RGBA color. `modelMaterialTint(index)` reads the per-object multiplier (white by default), and `modelMaterialTint(index, rgba)` sets it without changing the shared model asset or its texture. The tint combines with the imported diffuse color and the object's material tint while `usesModelMaterials()` is true. `materialSnapshot()` returns a detached `Material3D` value copy so scripts can inspect the material assigned to an object.
`childCount()` and `childAt(index)` inspect and traverse object subtrees. `contentFlags()` reports whether an object contains geometry, a model, particles, instances, or splats; `geometryHandle()` returns its reusable geometry handle when present.
`Material3D.colorValue`, `roughnessValue`, `metalnessValue`, `emissiveValue`, `opacityValue`,
`shadingName`, `mapPath`, `hasDynamicMap`, `isUnlit`, `isDoubleSided`, `castsShadow`, and
`receivesShadow` read base controls. `toonState`, `goochColors`, `hatchState`, `clipState`, and
`lineState` return detailed native settings in numeric and color arrays. `Scene3D.find(name)` returns a nested named object handle or null. `Scene3D.cameraState`
returns `[position xyz, target xyz, up xyz, fov, orthographic flag, ortho height, near, far,
focus distance, aperture, max blur, tilt]. `Scene3D.cameraFunction(name)` binds the native camera animation callback to a global `double name(double frame, double field)` function; it is called once per field, in the order above. `cameraAt` and `project` therefore evaluate the same scripted camera used during rendering. `Scene3D.renderCamera(frame, x, y, w, h, values)` renders through an 18-value camera state, so one scene can be shown through multiple independently animated views in the same draw callback. `lightCount` and `lightState` query each native
light's type, vectors, intensity, range, cone, shadow settings and color. A light state is
`[type, position xyz, target xyz, intensity, range, inner angle, outer angle, casts shadow,
shadow extent, shadow bias]`, with its packed RGB color in a one-element array.
`Scene3D.lightConfig` sets those fields by index for frame-derived light animation; type values
are 0 directional, 1 point and 2 spot. `Scene3D.lightFunction(index, name)` maps native
`Light::up` to a global `double name(double frame, double field)` callback. Fields 0..13 follow
`lightState`'s numeric array; field 14 is packed RGB. The callback runs after `Scene3D.up` and
can animate the light's type, position, target, intensity, range, cone, shadow settings and color.
The separate painter-style `d3` software renderer is available for custom triangle scenes:
`icosphere`, `octahedron`, `torusKnot`, `sphere` and `triangles` create editable `D3Mesh`
handles; `convexHull` builds a triangle hull from script-supplied vertices. `camera`, `project`,
`transform`, `options`, `grid`, `drawMesh`, `frameBegin` and
`frameEnd` expose its native projection and painter pipeline. Call drawing operations inside a
script draw callback. `D3Options` maps the native key color, ambient/diffuse/specular lighting,
rainbow shading, alpha and wireframe fields through setters or JSON keys `key`, `ambient`,
`diffuse`, `specular`, `rainbow`, `alpha` and `wire`. `transform` returns a row-major matrix; its Euler angles are radians.
`identity`, `multiply`, `translation`, `rotation`, `uniformScale` and `transformPoint` expose
the native row-major matrix operations; `addVectors`, `subtractVectors`, `scaleVector`, `dot`,
`cross`, `length` and `normalize` expose vector math. `D3Camera.state` and `set` read or change
its eye, look point and FOV. `D3Mesh.faceCount`,
`faceVertex` and `faceColor` inspect and edit generated triangles. See
the native showcase use in `examples/showcase/fx_present.cpp` and `scenes.cpp`.
`renderState`, `skyState`, and `waterState` expose configured scene, sky and water controls as
numeric/color arrays; `waterWaveState(index)` reads each individual wave. `environmentPath()`
returns the environment map path (empty when the scene uses a generated studio environment).
`linesState` returns scene ink width, crease threshold, depth threshold, wobble, wobble scale,
boil interval and its packed color.
`renderState` numeric values are `[ambient, environment intensity, environment rotation,
fog near, fog far, fog density, fog height, fog falloff, fog mode, cull floor, shadow map size,
show environment, fog from sky, sky enabled, water enabled, ink lines enabled]`; its colors are
`[background, sky, ground, fog]` as packed RGB values.
Model objects expose `boneCount()`, `boneName(index)`, `dualQuaternion(enabled)`,
`poseBones(matrixFields)` and `clearBones()`; the bone queries load the model in draw callbacks.
`matrixFields` is a flat array of sixteen native `Matrix` fields per bone in raylib field order.
Scene handles
also expose cameras and projection, lights, environment, fog, procedural sky, wave-controlled
water (including custom wave stacks), per-light shadow extent/bias and spot-cone controls,
`skyPreset("day"|"dawn"|"dusk"|"night")` selects each exact native preset;
`skyAt(hour)` animates through the day, and `skyBlend(fromHour, toHour, amount)` blends
the time-based skies. `skyBlend(fromSkyJson, toSkyJson, amount)` maps native `Sky::mix`
between arbitrary sky states.
Custom sky JSON uses the same fields as the 3D scene descriptor (`zenith`, `horizon`, `ground`,
sun, haze, stars, clouds, drift, seed, resolution and lighting). `skySunColor(color)`
for sun tint, and `waterSubmerged(x, y, z, seconds)` alongside water height/slope queries,
camera clipping planes, ink lines, depth of field, and the aircraft/computer/human/interior/lab prefab
families. `g3d::triangles(cacheKey, xyz[, normals])` creates custom triangle-soup geometry; the four-argument overload accepts `uv` as two values per vertex and creates a textured mesh buffer. `g3d::meshData(cacheKey, xyz, normals, uv, uv2, tangents, colorsRgba, boneCount, boneIndices, boneWeights, indices)` creates arbitrary fixed-topology meshes: optional float arrays may be empty, colors use packed `0xRRGGBBAA`, bone indices and triangle indices are `array<uint>`, and indices may be empty for non-indexed triangle soup. Set `boneCount` to zero and pass empty bone arrays for rigid geometry; skinned geometry supplies four byte-range bone indices and four 0..1 weights per vertex. Missing normals are generated.
`Geometry3D.vertices()` and `triangles()` report mesh sizes. In draw callbacks, `vertex(index, xyz)`, `normal(index, xyz)`, `texcoord(index, uv, secondary)`, `tangent(index, xyzw)`, and `colorRgba(index)` read per-vertex CPU attributes; `indexCount()` / `indexAt(index)` inspect indexed topology (`indexCount()` is zero for non-indexed meshes). `boneCount()`, `boneIndex(vertex, influence)` and `boneWeight(vertex, influence)` inspect four bone influences per vertex, and `animatedVertex` / `animatedNormal` read CPU-skinned buffers when present. Queries for absent buffers throw a descriptive error. `updateVertices(xyz[, normals])` queues fixed-topology position/normal updates for any geometry handle. `updateTexcoords(uv, secondary)` updates primary or secondary UVs, `updateTangents(xyzw)` updates per-vertex tangents, `updateColors(rgb)` accepts one packed `0xRRGGBB` color per vertex, while `updateColorsRgba(rgba)` accepts packed `0xRRGGBBAA`; `updateIndices(indices)` replaces indices while preserving the triangle count. Each buffer must match the existing geometry topology; edits to the same geometry in one frame are composed and the changed GPU buffers are synchronized before rendering. `updateTriangles` remains the custom triangle-buffer entry point. Primitive
geometry is cached across frame callbacks. Direct `Material3D.dynamicMap(descriptorJson)` accepts
the same JSON dynamic texture descriptor, and `clearDynamicMap()` removes it. Add a `SplatCloud`
directly with `Scene3D.addSplats(cloud[, lookJson])`; the optional look JSON accepts `scale`,
`opacity`, `shDegree`, `clipNormal`, `clipOffset`, `clipBand`, and `scatter`. `Object3D.splatLook(lookJson)` updates these fields on an attached cloud from its frame callback; `splatLook()` reads the current look as JSON, so scale, opacity, SH degree and clipping can animate without rebuilding samples. Add 3D emitters
with `Scene3D.addParticles(emitterJson[, geometry, material])`, using the same particle config
fields as descriptor scenes. Attach physics handles directly with `Scene3D.addRope(rope, radius,
material)` and `Scene3D.addCloth(cloth, material)`; their meshes update from the selected frame.
`Scene3D.addInstances(geometry, descriptorJson[, material])` exposes the baked instanced-mesh
path, including per-instance positions, clips, shade and frame selection. `Object3D` group
handles can nest geometry, models, splats, particles and instances under transformed parents;
physics rope and cloth handles attach directly to `Scene3D`.
A root `city`
descriptor loads CityJSON/CityJSONSeq
data, sets semantic surface colors and LOD, optionally frames the camera, and animates
buildings rising by distance. `city.look: "smartgrid"` selects the native smart-grid material
and semantic-palette callbacks. A root `cityTrail` descriptor reveals a path from CityJSON-axis
point triples; `city::length` and `city::along(pathXYZ, fraction, out)` expose route length,
position and heading to scripts. `along` writes `[x, y, z, headingDegrees]` in CityJSON axes,
with heading clockwise from north to match native `city::along`.
`city::toScenePoint(x, y, z, out)` and `city::toSceneBounds(minMax, out)` convert CityJSON axes
`(x east, y north, z up)` to scene axes `(x east, y up, z south)`; bounds are reordered after
the axis swap. `city::parseModel`/`city::loadModel` return a `CityModel` handle with
object count, id/type/attribute and model metadata JSON, origin, LOD enumeration, `filterType`,
`filterIds`, and `filterFunction(callback)`. The latter calls a global
`bool keepCityObject(string objectJson)` for every object, where the JSON contains the complete
object metadata and geometry, and preserves native `Model.filter` order and model metadata. Its
callback name is limited to 128 characters and each object payload to 4 MiB.
Queries expose each object's bounds, height and base; geometry types and LODs; face, line and
point counts; face semantics, ring vertices, triangulated indices and `faceNormal(...)` (zero
for degenerate faces), and `faceColor(...)` resolved
through the same semantic look as native CityJSON meshes; and line/point coordinates.
`CityModel.world` has a double-output overload for file coordinates, and `CityModel.local`
accepts double world coordinates before returning local scene coordinates, preserving the
native double precision of large projected reference coordinates. Float-array overloads remain.
`city::objectInfo` returns hierarchy and attribute metadata for an object id; direct `parentCount` / `parentId` and `childCount` / `childId` methods expose each relationship by index. See
`CityModel.mesh(objectIndex, lookJson)` creates reusable per-object `Geometry3D`, and
`meshAll(lookJson)` creates one combined model mesh; the look JSON accepts a `preset` (`default`,
`smartgrid`, or `lab`), `lod`, and role colors such as `roof`, `wall`, `road`, `water`, and `vegetation`.
`faceColors` can override individual semantic kinds such as `WallSurface` or `RoofSurface`;
unlisted kinds keep the preset or role color. `typeMaterials` and `objectMaterials` accept
maps from CityObject type or object ID to a regular 3D material JSON object; object ID rules
take precedence over type rules. For computed styling, `colorFunction` names a global
`string faceColor(string inputJson)` callback that receives `{object,face}`; `object` carries
ID, type, attributes, hierarchy and geometry (including coordinate arrays), and `face` carries
semantic kind, semantic attributes and rings. Return a color such as `"#e8a060"`, or `"null"`
to use the preset fallback. `materialFunction` names a global
`string objectMaterial(string inputJson)` callback with `{object}` and returns a material JSON
object to replace that object's material or `"null"` to leave it unchanged. Each callback payload is limited to
4 MiB, callback names to 128 characters, and callback results to 1 MiB. The named callbacks run
inside the current script invocation. See `examples/scripts/city_card.as`.
`city::add(model, scene, lookJson, material)` adds a named object group to the scene, and
`city::add(model, parentObject, lookJson, material)` nests it under an `Object3D` group;
`city::rise(group, delay, stagger, length)` animates its buildings, while `city::frame(model,
scene, azimuth, elevation, margin, aspect)` fits the scene camera. `city::frameBounds` provides
the same camera framing for an explicit `[minX,minY,minZ,maxX,maxY,maxZ]` box. `city::trail`
accepts a scene or parent object, flattened path XYZ triples, a reveal formula in frame variable
`t` (clamped to 0..1), and optional style JSON (`width`, `thickness`, `lift`, `color`, `unlit`).
`city::trailFunction` provides the native callback equivalent: `float reveal(int frame)`, also
clamped to 0..1, with a scene or parent object overload.
Meshes can also be
attached to `Scene3D` or nested `Object3D` groups like other geometry.
See `examples/scripts/city_card.as`.
Object positions, rotations and scales are values
in the scene description, so scripts can build the JSON from the current frame for
absolute animation. Primitive geometry descriptors expose the native tessellation controls
(`rings`, `slices`, plane `resX`/`resZ`, torus `radialSegments`/`sides`, and knot
`tubularSegments`/`radialSegments`/`p`/`q`). Objects also accept `cullRadius` and
`cullCentre`, and scenes accept `cullFloor`, matching the native shadow-aware culling fields.
Geometry objects can use an `instances` descriptor with `items` carrying position, yaw,
pitch, bank, scale, baked `frame`, clip start/length, and shade. Optional `frames`, `positions`
and `normals` arrays expose native baked vertex animation (frame-major xyz triples); omitted
arrays use the primitive geometry as a static one-frame mesh. See the instanced swarm in
`examples/scripts/material_controls.as` and `examples/scripts/physics_scene.as`. Within a
draw callback, `g3d::geometryData(geometryJson, vertices, normals)` samples a native primitive's
CPU mesh as flattened xyz triples, so scripts can derive baked deformation frames without
guessing the tessellator's vertex order.
The `physics` namespace exposes native Rope and Cloth solver handles, `PhysicsParams`, Sphere/Plane/Capsule/Box colliders, seeded wind, pins, bending/stretching, tearing, self collision and frame sampling. `physics::rope` and `physics::cloth` accept an optional params handle; omitting it uses the native defaults. See `examples/scripts/physics_scene.as` for the descriptor
path and `examples/scripts/physics_callbacks.as` for direct callback-driven handles.
`PhysicsParams.state(values)` returns
`[gravityX,gravityY,gravityZ,damping,substeps,iterations,fps,friction,thickness]`;
`PhysicsWind.state(values)` returns `[directionX,directionY,directionZ,strength,gust,gustHz]`
and `seedValue()` returns its integer seed. Pin and collider drives can use their existing
piecewise-linear tracks or named callbacks: `pinFunction`, `pinEndFunction`, and `driveFunction`
call a global `string positionAt(double seconds)` that returns JSON `[x,y,z]`. Set these from a
draw callback; physics passes absolute simulation seconds in double precision. Callback JSON is
limited to 256 bytes and each coordinate must be finite and within +/-1e7.
`PhysicsCollider.state(values)` returns
`[kind,aX,aY,aZ,bX,bY,bZ,halfX,halfY,halfZ,radius]` (`kind`: sphere 0, plane 1,
capsule 2, box 3), while `hasDrive()` reports whether it has an animated track.
Rope and cloth `frame()` sampling, plus cloth `liveConstraints()`, accept frames 0..18000.
This bounds lazy simulation cache growth while covering ten minutes at 30 fps. Direct handles
allow 1..4096 rope segments, cloth dimensions up to 128 per side (16,384 vertices), solver
substeps 1..64, iterations 1..128 and fps 1..1000. The compact `physics` scene descriptor
keeps tighter render-time bounds: rope segments 2..512, at most 16 colliders, substeps 1..16,
iterations 1..32 and fps up to 240. Use direct handles when authoring simulations that need
the larger ranges. `physics::ropeMesh(rope, frame, radius, sides)` and
`physics::clothMesh(cloth, frame, doubleSided)` expose the native snapshot mesh builders as
`Geometry3D` handles, which can be inspected, edited where supported, or attached with
`Scene3D.add`. Mesh sampling is bounded to frames 0..18000; rope sides are capped at 128
(native values below 3 still clamp to 3), and rope radius must be finite within +/-1,000,000.
3D particle objects accept the full emitter configuration in `particleConfig`, including
emission timing, volume, motion, floor response, color runs, particle look, and additive
rendering. `fx::count3D`, `particle3D`, and `positionAt3D` expose deterministic samples and
streak tails for custom 3D effects; `particle3D` samples are
`[index,birth,age,life,u,x,y,z,vx,vy,vz,size,rotation,axisX,axisY,axisZ,alpha,r,g,b,a,random]`
and an out-of-range index returns false with an empty array. Both emitter forms accept `originKeys` arrays of
`[frame,x,y]` or `[frame,x,y,z]` values for deterministic animated source paths; use
`originFunction` for arbitrary source motion. See
`examples/scripts/particle3d_authoring.as`. Both 2D and 3D emitter descriptors accept `seed` as an exact JSON integer from 0 through 9007199254740991 or as a decimal string for the full unsigned 64-bit range.
3D objects can also define a procedural `voxel` grid with dimensions, palette, individual
cells, box/sphere/line primitives and greedy-mesh ambient occlusion. `voxelFrames` accepts
procedural grids or a multi-model `.vox` file, then switches visible meshes at a configured
hold interval. The script `VoxelGrid` API exposes create/load/save, cell editing, palette
RGBA palette colors, merged-quad/AO inspection, and `descriptor()` serialization for embedding the
authored grid in a `g3d::draw` object's `voxel` field. Palette colors serialize as
`#RRGGBBAA`, preserving transparency through descriptor round-trips. `voxel::mesh(grid, optionsJson, stats)`
returns reusable `Geometry3D` and writes native voxel/face/quad/triangle totals; options include
cell `size`, `origin`, `centre`, `ao`, and `aoStrength`. `Scene3D.addVoxelFrames` adds the native
looping animation from a `VoxelGrid` array. `voxel::loadModels` returns each `.vox` model chunk;
`voxel::loadScene` returns scene-graph models with their placement offsets. `voxel::save` writes
only during the script's `assets()` callback so generated files stay under `c.assets`. See
`examples/scripts/voxel_animation.as`.
A procedural `splat`
cloud can be a seeded sphere surface, box volume, voxel surface, or explicit sample list;
PLY splats remain loadable through `splatPly`. See
`examples/scripts/voxel_splat_scene.as`.
`splat::sphereFunction` and `boxFunction` preserve the native seeded placement builders while calling a named `uint fn(float x, float y, float z, int index)` for each sample color (`0xRRGGBBAA`). They support up to 250,000 samples and share the script statement budget.
`splat::fromFunction(count, callback)` additionally calls `string callback(int index)` once per sample; return `{"position":[x,y,z],"sigma":[sx,sy,sz],"rgba":packedRGBA,"opacity":0..1}`. It uses the native identity orientation and validates every sample. Counts are limited to 50,000 and each JSON result to 4 KiB.
`splat::cloud()` creates an empty editable `SplatCloud` for incremental authoring with `add`; call `bound()` after the final add or edit to recompute its center and 3-sigma radius. `SplatCloud` also exposes sample inspection and editing, covariance and degree-1 SH data,
deterministic sphere/box/voxel builders, merge, and PLY load/save. `splat::voxels(grid, meshOptionsJson, perFace, jitter, seed)` accepts the native voxel mesh options object (`size`, `origin:[x,y,z]`, `centre`, `ao`, `aoStrength`) when placing voxel-derived splats; the shorter overload uses default options. Its `descriptor()` can be
placed in a `g3d::draw` object's `splat` field and preserves each sample's quaternion and SH
coefficients. `sample(index, out)` and `setSample(index, data)` use `[x,y,z,sx,sy,sz,qw,qx,qy,qz,opacity,r,g,b]`; `setSample` normalizes the quaternion, validates positive sigma and 0..1 opacity/RGB, and refreshes bounds. `savePly` is restricted to `assets()`.
The `globe` namespace exposes `point(lon, lat, radius, xyz)`, `lonLat(xyz, out)`, `north(lon, lat, xyz)`, `east(lon, lat, xyz)`, and `arc(lonA, latA, lonB, latB, progress, height, radius, xyz)`; `lonLat` supports both `array<float>` and `array<double>` output. Point vectors use `array<float>` XYZ triples, geographic coordinates are in degrees, and arc progress is 0..1. It also exposes sphere and regional patch geometry, camera positioning, object placement and revealable routes (`route`, `fly`). `heat(value)` exposes the native heat-map color
as packed RGBA. `GlobeMap` supports global or regional maps; `map(width)` and `regionMap(...)` start transparent, while RGB overloads start opaque and `mapRgba`/`regionMapRgba` accept explicit alpha. Fill, stroke, graticule and disc methods accept optional alpha; `gradientRgba` sets alpha per latitude band. Pixel access uses packed RRGGBBAA. The map supports geographic layer or custom `GeoGeometry` fill/stroke, coordinate conversion, compositing, RGBA export and direct use as a `Material3D` map. `GlobeField` builds Gaussian heat fields with spot addition, sampling, maximum and raw-value access. `rgba(scale, callback, pixels)` accepts an opaque `uint palette(float normalizedValue)` callback; `rgbaRgba` accepts `string palette(double normalizedValue)` returning `#RRGGBB` or `#RRGGBBAA` to preserve the native `Color` alpha. Custom exports are limited to 1 Mi pixels. Fields can also be used directly as a `Material3D` globe texture. `GlobeWeather` provides seeded cloud controls with getter/setter
overloads for `cover`, `softness`, `scale`, `evolve`, `opacity` and `seed`; constant,
latitude-sampled, or named `windFunction` wind (`float callback(float latitude)` in degrees longitude/second, bounded to +/-1000), storm-cell creation and inspection; `density()` samples the cloud field and `cloudImage()` exports an
equirectangular RGBA texture from `assets()` for `assets::writeImage`. The JSON globe descriptor
remains available for compact declarative scenes. `globe::route` adds great-circle routes to a
scene or nested object group; route styles accept `headFunction` and `tailFunction` callbacks (`float fn(int frame)`). `globe::path` evaluates parametric x/y/z formulas, while `globe::pathFunction(parent, callback, ...)` accepts a global `string fn(double u)` returning JSON `[x,y,z]` to match native custom curves. Callback coordinates must be finite and within +/-1e6; callback names are limited to 128 characters.
See `examples/scripts/globe_map_handles.as` for a procedural map texture and directly assembled
3D globe.
Kinetic text helpers `txt::typewriter`, `txt::reveal` and
`txt::scramble` provide frame-addressed string animation. A runnable cross-section is in
`examples/scripts/feature_atlas.as`. `txt::revealStyled` and `scrambleStyled` expose the
native letter/word/line units, forward/backward/center/edge/random order, offsets, scale,
rotation, fade and easing. Reveal JSON may set `easeFunction` to a named `double easing(double progress)` callback, exclusive with the named `ease` field; the callback must return finite values and runs under the composition statement budget. `TextMorph.drawFunction` accepts the same callback style for native morph easing. `txt::wave`, `highlight` and `morph` expose native glyph styling
and text-to-text morphing; their text style and reveal configuration use JSON.
`txt::morphKept(fromText,toText,size,color,styleJson)` reports the number of glyphs preserved
by the native morph matching algorithm.
`txt::layout(text,size,color,width,styleJson)` creates a reusable `TextLayout` handle with
glyph positions, advances, word/line membership, full text bounds, reveal length and per-glyph
reveal progress. It can draw the
resting or revealed layout. `txt::morphLayout(from,to)` creates a reusable `TextMorph` handle
with `kept`, `fromLayout`, `toLayout`, and animated `draw` methods with selectable native easing.
`callbackStyler("styleGlyph")` creates a reusable per-glyph script styler. Its callback signature is `string styleGlyph(int index, int word, int line, int inWord, float x, float y, float advance, bool space, int frame)`. `callbackStylerText("styleGlyph")` adds the native glyph text as a final `const string &in glyphText` parameter, so callbacks can branch on the actual character. The callback runs for each visible, non-space glyph while drawing; positions are in the text layout's local pixels, and `frame` is the drawing callback's local frame. It shares the composition statement budget. Return a JSON object with optional `offsetX`, `offsetY` (or `offset: [x,y]`), `scale`, `rotate`, `alpha`, `color`, and `replace` fields. `waveStyler`, `highlightStyler`, and `chainStylers` create reusable native `TextStyler` handles;
pass a chain to `TextLayout.drawStyled` to compose the glyph effects in order.
Subtitle parsing is also scriptable through `subtitles::parse` / `load`, the `SubtitleTrack`
cue and phrase methods (including `cueAt(seconds)`, which returns the active cue index or -1),
`spans` (the tail frame count defaults to zero), `toSrt`, `writeSrt(path, track)` from `assets()` only,
and individual `drawCue` / `drawCueStyled` calls.
See `examples/scripts/subtitle_tracks.as`.
`space::draw(json, frame, x, y, w, h)` renders a frame-addressed 3D math scene from JSON.
It accepts a flat, tilted or custom view; axes, guides, depth-sorted points/lines/arrows/
curves/faces/planes/labels; and sphere, torus, Möbius, Klein bottle and sheet-to-torus
surface meshes. Surface colors, wireframes, and the camera are script-controlled. See
`examples/scripts/space_card.as`. Its `hyperbolic` descriptor adds bounded Poincaré tilings
and geodesics; see `examples/scripts/hyperbolic_card.as`.
The native calculation API is also available: `space::rotate4` and `project4` work on
four-dimensional points, `tesseract4` returns its 16 `xyzw` vertices followed by 32 edge
index pairs, and `projectOnPlane` / `projectOnLine` return three-coordinate results.
`space::lerp` returns the native linear interpolation between two 3D points.
`space::hyperbolic` exposes disk/half-plane conversion, distance, isometry, reflection,
geodesic segments, complete lines, polygons, circles and tilings. Point lists are flattened
as `x,y` pairs; tilings are concatenated variable records `[centerX,centerY,parity,ring,
vertexCount,x0,y0,...]`. `diskMap` and `halfPlaneMap` map complex coordinates to pixel coordinates
using explicit center/origin and scale values, matching the native `Disk.map` and `HalfPlane.map`.
`space::sphere` exposes great-circle arcs, circles, triangle angles,
stereographic projection and its inverse; sampled points are flattened as `x,y,z` triples.
All output-array formats are exercised in `examples/scripts/space_native_math.as`.
`space::SpaceMesh` exposes mesh dimensions, wrapping flags, indexed and flattened point access, and `setPoint(row, col, x, y, z)` for bounded finite coordinate edits.
`space::grid` and `space::graph` build meshes from script-supplied arrays; `space::mapped`
and `space::mix` call the native mapping and topology interpolation operations. Callback
forms `space::gridFunction(callback, rows, cols, wrapColumns, wrapRows)` (`string callback(float
u, float v)` returns JSON `[x,y,z]`), `space::graphFunction(callback, xMin, xMax, yMin, yMax,
rows, cols)` (`double callback(double x, double y)`), and `space::mappedFunction(mesh,
callback)` (`string callback(float x, float y, float z)` returns JSON `[x,y,z]`) match the
native function builders. Grid coordinates are normalized to 0..1; graph callbacks receive
sampled world x/y. Callback mesh dimensions are limited to 2..256 per axis, and returned
coordinates/scalars to finite values within +/-1,000,000. These callbacks run immediately
under the active script statement budget to build the mesh; the mesh does not retain them. The
`sphereMesh`, `torusMesh`, `glueTorusMesh`, `moebiusMesh` and `kleinMesh` factories expose
all native surface builders. `space::drawMesh` renders any of these with the native depth
sorting, shading, back-face color and wireframe controls; its view and style use the same
JSON fields as `space::draw`. Style JSON also accepts `colorFunction` naming a global
`uint fn(float x, float y, float z)` callback for each face center, or `cellColorFunction`
naming `uint fn(int row, int col)` for each zero-based grid cell. The RGBA variants
`colorFunctionRgba` and `cellColorFunctionRgba` use the same parameters and return a color
string such as `"#RRGGBB"` or `"#RRGGBBAA"`. Each RGBA option is mutually exclusive with
its RGB counterpart; cell coloring takes precedence over face coloring when both kinds are
provided. RGB callbacks return packed `0xRRGGBB`. `SpaceView` also exposes native
flat/tilted/custom creation, field access and mutation, interpolation, camera-space conversion,
projection, depth and perspective scale queries. It can be passed directly to the
`drawMesh(mesh, view, styleJson)` overload. See `examples/scripts/space_native_math.as`.
The 3D scene object form also accepts `prefab: {type, kind}` for the native `aircraft`,
`computer`, `human`, `interior`, and `lab` builders. `kind` uses the canonical native name
(for example `human-resident`, `pc-case`, `office-desk`, `lab-fume-hood`, or
`passengerJet` for the aircraft). These objects take the same position, rotation and scale
fields as geometry objects. `g3d::prefabKinds(type, kinds)` lists every supported kind name
for one family. `examples/scripts/prefab_card.as` demonstrates a composed set.
For hierarchical authoring with native handles, `Scene3D.addPrefab(type, kind)` creates a
root prefab and `Object3D.addPrefab(type, kind)` attaches one beneath a parent group; both
accept the same type and canonical kind strings.
An optional root `globe` descriptor creates a textured world with configurable map colors,
borders and graticules, camera framing, animated great-circle routes, and procedural cloud
shells with storm systems. Cloud generation is frame-addressed and deterministic. See
`examples/scripts/globe_card.as`.
`geo3d` data scenes can morph a geographic surface from globe to projection, apply
country choropleths, place scaled columns/spikes/bubbles/discs, extrude country regions,
and animate flow arcs from JSON values. Native frame callbacks are available for growth and flow reveals: use `growFunction` on marks/extrusions and `flowHeadFunction` / `flowTailFunction` on the descriptor, each naming a global `float callback(int frame)`. Direct `geo3d::marks` / `extrude` options accept `growFunction`; direct `geo3d::flows` options accept `headFunction` / `tailFunction`. Outputs must be finite and are clamped to 0..1. Every geo3d scale descriptor (`choropleth`, `marks`, `extrusions`, and `flowScale`) also accepts `colorFunction`, naming a global `uint fn(float normalizedValue)` that returns packed `0xRRGGBB`; callback names are limited to 128 characters and execution uses the script statement budget. The matching native value callbacks are also scriptable: `marks.valueFunction` names `double fn(int frame, double baseValue, double longitude, double latitude, string key, uint rgba)`, and `extrusions.valueFunction` names `double fn(int frame, double baseValue, string key)`. Callback names are limited to 128 characters; mark `rgba` is packed as `0xRRGGBBAA`. Value callbacks must return finite doubles and are exclusive with `valueFormula`. `examples/scripts/geo3d_card.as` shows the data,
marks, and flow interface.
Root `crowd` descriptors bake deterministic ground, air, or water agents from bounded
group settings, obstacles, named predators, and time-keyed goal/path positions. The cached
simulation is sampled by scene frame and rendered with the native locomotion rig; looks can
be varied people, species herds, or seeded zombies. Group steering, spawn ranges, size and
speed variation, and facial detail count are configurable. Waypoint arrays use
`{time, at:[x,y,z]}` objects. For continuously moving targets, `goalFormula:[fx,fy,fz]`
evaluates each coordinate with formula variables x=seconds and y=agent index; `goalFan`
is still applied afterward. Predator `pathFormula:[fx,fy,fz]` uses x=seconds and y=0.
World `seed` and `Scene3D.addCrowd` descriptor `lookSeed` accept decimal strings for the
full unsigned 64-bit range. JSON numeric values are accepted only from 0 through
9007199254740991, where they remain exactly representable; use strings for larger seeds.
Formula outputs must stay finite and within +/-1e6. Each formula field replaces its
matching waypoint array. A group may set `goalFunction` to a global `string goalAt(double seconds, int index)` that returns a JSON vector `[x,y,z]`; `goalFan` is added after the callback. A predator may set `pathFunction` to `string predatorAt(double seconds)`, also returning `[x,y,z]`. Each callback is exclusive with its formula and waypoint alternatives. Callback names are limited to 128 characters; returned vectors are limited to 256 bytes and finite coordinates within +/-1e6. These callbacks run during deterministic crowd baking. Set `lookFunction` on a crowd descriptor to call a global
`string makeLook(int index)` for each agent within its group; it returns a rig look JSON object
such as the ones accepted by `rig::draw`. The callback runs while the crowd's cached characters
are first built, so the same simulation cache reuses those looks across frames. Its name is
limited to 128 characters and each returned descriptor to 256 KiB. The direct
`crowd::bake(worldJson)` API returns a
`crowd::Simulation` handle with group lookup, agent index/size, resting and travelled queries,
and `state(agent, seconds, out)` as `[px,py,pz,vx,vy,vz,yaw,pitch,bank,speed]` for custom script
visualizations. `worldInfoJson`, `groupInfoJson`, and `obstacleInfoJson` expose the baked configuration; `groupIndex`, `goalAt`, and predator name/position methods expose the native group and path queries. `crowd::bakeWalker(lookJson, ...)` and `crowd::bakeBat(...)` create cached
`crowd::BakedBody` handles; `Simulation.walkCycle` / `flapCycle` expose their phase. Baked body
handles also expose the native still/move clip ranges, stride, still duration, center/radius,
rest geometry, and per-frame position/normal buffers (`positions(frame, out)` and
`normals(frame, out)`, each capped at one million vertices), while
`Scene3D.addInstancedCrowd` and `addFlock` attach the native batched render paths.
`Scene3D.addCrowd(simulation, group, lookFunction, style, seed, faces, fps)` attaches
full rig characters with their per-agent face and outfit details. Use `style` `person`,
`zombie` or `herd` for built-in looks, or provide a global
`string makeLook(int localIndex)` callback returning a rig look JSON object. The 2D
`crowd::draw` helper renders seeded person, herd or zombie cut-outs with a shared view JSON;
it draws faces by default, with the optional `faces` count limiting how many keep them. Its
optional final `lookFunction` names a global `string makeLook(int localIndex)` callback for
per-agent rig look JSON, matching the 3D crowd route. See
`examples/scripts/crowd_card.as`.
The `rig::draw(characterJson, seconds, x, y, w, h)` helper builds a cached 2D puppet from
toon/block/avatar/robot or animal looks, lets scripts choose proportions, parts, palette,
outfit and face style, and evaluates clip, expression, gaze and speech cue arrays at the
requested time. The same character descriptor works as a `g3d::draw` object's
`prefab:{type:'character',kind:'robot'}` for a lit 3D rig. Cues use `{from,name,fade}` for
clips, `{time,name,fade}` for expressions, `{time,x,y,fade}` for gaze, and
`{from,to,text}` for speech. See `examples/scripts/rig_card.as`.
`font::addSearchPath` and `font::find` expose font discovery. `draw::textFile` accepts a
single custom font or an ordered candidate array, with optional `rasterSize`; custom font
atlases include the actual UTF-8 text being drawn. `TextLayout` exposes the stored style size, spacing, line height, packed RGBA color, and horizontal/vertical alignment alongside its glyph and word queries. Text effects, text boxes, measurement/wrapping and styled subtitles
also accept `fontCandidates` in style JSON. See `examples/scripts/font_fallback.as`.
For reusable fonts, `font::load(candidates, baseSize)`, `font::loadFor(candidates, sample, baseSize)`
and `font::ui(bold, baseSize)` return `font::FontAsset` handles. Load them inside a scene callback;
then use `measure(text, size, spacing)` for width, `measureBounds(text, size, spacing, xy)` for width and height, and `font::draw(handle, text, x, y, size, color, alpha, spacing, anchorX, anchorY)`. The output array is resized to `[width, height]`.
`draw::imageStyled` exposes native image fitting (`fill`, `contain`, `cover`, `none`),
alignment, tint, opacity, and frame time for animated GIFs; the shorter `draw::image`
keeps its compact default form.
Map drawing helpers take a projection, a reveal `progress` (0..1) and a style as JSON whose
keys are the native style fields (`rmtn/geomap.hpp`). They are only available inside draw
callbacks, except the data helpers (`layer`, `fly`, `focus`, `along`, `length`,
`routePath`, `places`, `names`, `valuesAt`):

| Call | What it draws or returns |
| --- | --- |
| `geo::layer(name)` | A bundled layer: `countries`, `land`, `lakes`, `rivers`, `cities`. |
| `geo::base(map, progress, style)` | Ocean, graticule, land, borders. Style: `ocean`, `land`, `coast`, `border`, `graticule`, their widths, `graticuleStep`. |
| `geo::choropleth(map, valuesJson, progress, style, layer)` | Fills features by value. `valuesJson` is `{"IDN": 12, "Kenya": 3}`. Style: `min`, `max`, `colors` (two or more `"#rrggbb"` stops) or `colorFunction`, `missing`, `border`, `borderWidth`, `stagger`. |
| `geo::colorbar(x, y, w, h, valuesJson, progress, style)` | The key. Give it the same `min`, `max` and `colors`; also `title`, `suffix`, `decimals`, `textSize`. |
| `geo::valuesAt(csv, time)` | Values at one time from a table of `code,2000,2010,...` rows, blended between columns, as `valuesJson`. |
| `geo::highlight(map, geometry, progress, style)` | One region picked out. Style: `fill`, `stroke`, `width`, `dim`. |
| `geo::markers(map, itemsJson, progress, style)` | Dots with labels. Items: `{"lon", "lat", "label", "value", "color"}`. Style: `radius`, `sized`, `color`, `ring`, `ringWidth`, `labels`, `textSize`, `textColor`, `halo`, `bold`, `stagger`. |
| `geo::labels(map, itemsJson, progress, style)` | Names that keep clear of each other, earliest first. Items: `{"lon", "lat", "text"}`. Style: `size`, `color`, `halo`, `bold`, `offset`, `margin`, `stagger`. |
| `geo::places(layer, limit, valueProperty)`, `geo::names(layer, limit)` | Items JSON for `markers` and `labels` from a layer. |
| `geo::route(map, lonLatPairs, progress, style)` | A great-circle route that draws itself on. Style: `color`, `width`, `dash`, `ghost`, `head`, `arrow`, `headColor`, `stops`, `tail`. |
| `geo::routePath(map, lonLatPairs, from, to)` | The same route as a `Path`, clipped to the map. |
| `geo::along(lonLatPairs, progress, out)`, `geo::length(lonLatPairs)` | `[lon, lat, bearing]` at a fraction of the route; its length in km. |
| `geo::fly(a, b, progress, zoomOut)` | The view between two projections: great-circle pan, zoom by ratio, a zoom-out on long trips (`zoomOut` 0 picks it, 1 turns it off). |
| `geo::focus(base, geometry, x, y, w, h, padding)` | `base` reframed on a geometry in a box (padding in px). |

`geo-route` and `geo-flyto` are prefabs for `prefab::use`; their props are in
[geo-prefabs.md](geo-prefabs.md). Examples: `map_choropleth.as`, `map_route.as`,
`map_timeline.as`, `map_prefabs.as`.

`geo::drawWorld` draws bundled countries, land, lakes, rivers or city layers, or a
GeoJSON/TopoJSON file using `file:path`, with any native map projection, a rotating globe
center, graticules and optional feature highlight. `geo::distance`, `bearing` and `angle` provide great-circle measurements in
kilometres/degrees. `geo::parse` / `load` return reusable layers with feature lookup,
properties, `featureId(index)` IDs, type-preserving `propertyJson(index, key)` values, and case-insensitive lookup, exact property filtering, containment, area, label coordinates, and `geo::metrics` (centroid, bounds, area). `metrics` and great-circle `interpolate` accept both `array<float>` and `array<double>` outputs; use doubles to retain native geographic precision. During drawing, `GeoLayer.filterFunction(name)` calls a global `bool name(int index, const string &in id, const string &in featureName, const string &in propertiesJson)` callback for arbitrary predicates. `geo::interpolate` samples a great-circle route. `geo::geometry`, `geo::point`, `geo::line` and `geo::polygon` create reusable `GeoGeometry` values; polygon inputs are flattened lon/lat pairs plus cumulative point counts in `ringEnds` (outer ring first, followed by holes). Geometry handles support `empty()`, append, coordinate/count queries, bounds, centroid, area and containment; layers expose `geometry(index)` and `merged()`. `GeoProjection.fit` and `path` / `lines` accept either a layer or geometry. `geo::projection` exposes native projection configuration,
  fitting (including `fitSphere` for a whole-globe orthographic view), visible and unclipped point projection plus feature/graticule/outline drawing; `geo::mix` animates between
projections. `GeoProjection.path(layer[, featureIndex])`, `lines(layer[, featureIndex])`,
`projectUnclipped(lon, lat, pixel)` also returns screen coordinates for points hidden by clipping, matching native `Projection::operator()`. `outline()` and `graticule(step, polar)` return clipped native vector `Path` handles for custom
fills, strokes, trimming and morphing. Projection kind, roll and scale are queryable after configuration. See
`examples/scripts/geo_projection.as`. `geo3d::surface(projection, centerLon, centerLat, radius, width)`
returns a reusable `geo3d::Surface`; animate `morph`, swap projections with `setMap`, retrieve
its fitted flat projection with `mapProjection()`, query
`point`, `up`, `north`, `onGlobe`, and `onMap` into float arrays, and attach its base mesh with
`geo3d::base(parent, surface, material, segments)`. `geo3d::values(json)` loads key/value data;
`data().add(lon, lat, value, key, rgb, alpha)` builds arbitrary point data (alpha defaults to 255; `0xffffffff` keeps the native scale-color fallback). `Data.set(index, lon, lat, value, key, rgb, alpha)` replaces every field on a positioned datum and keeps the source value record in sync. Datum color is readable with `color(index)` and `colorAlpha(index)`, and can also be edited alone with `setColor(index, rgb, alpha)`; indexed datum methods apply after `Data.add` or `byCountry` has produced positioned datums. `byCountry` resolves locations through a geographic layer. `marks`, `extrude`, and `flows` add
native 3D data geometry; `choropleth` colors a `globe::GlobeMap`; `Scale` supports `over`, `t`, `color(value)` for packed RGB, `rgba(value)` for packed RRGGBBAA, and fixed color stops. Use `colorsRgba(stops, rgb, alpha)` or the style JSON `alphas` array when stop colors need transparency. Named palettes use `colorFunction` (`uint fn(float normalizedValue)`, opaque RGB) or `colorFunctionRgba` (`string fn(double normalizedValue)` returning `#RRGGBB` or `#RRGGBBAA`), either on the handle or in style JSON; fixed stops and callbacks are mutually exclusive. `screen` projects a surface location through a scene camera/frame or, in its camera-state overload, the same 18-value custom state accepted by `Scene3D.renderCamera`. Their
style JSON accepts scale (`min`, `max`, `low`, `high`, or matching numeric `stops`/`colors`),
reveal timing (`progress` or `start`/`duration`), and formula callbacks (`growFormula`,
`valueFormula`, `headFormula`, `tailFormula`) evaluated from frame `x` and data value `y`.
Flow routes use `{from:[lon,lat],to:[lon,lat],value,color}`.
`code::parse(source, language, tabSize)` returns a `CodeBlock` exposing normalized lines, line widths, syntax token text/kind/column, character counts, and native find ranges; line numbers are 1-based and token indices 0-based. `code::find` follows the same 1-based line convention.
`code::draw` parses and renders syntax-highlighted source with theme,
line reveal, character reveal, caret and title-bar options; parsed blocks are cached per
script worker. `code::inspect` returns columns, character count and line count; `code::find` returns a selected
occurrence's half-open column range; `code::bounds`, `code::cell` and `code::cellWidth` expose
native layout coordinates; and `code::morphCounts` reports added/removed lines. `code::drawAdvanced` accepts JSON for
full `code::Style` layout and `code::Look` focus, focus blending, dimming and marks; `themeColors`
accepts the card and UI colors plus `syntax`, an array of 13 colors in `CodeTokenKind` order.
`code::morph(from, to, language, tabSize)` returns a `CodeMorph` handle exposing each matched/fading piece, both token kinds, source and destination positions, line-number markers, and added/removed counts. Morph line positions are 1-based; columns are 0-based; `-1` marks a piece missing from that side. `code::drawMorph(from, to, language, x, y, size, progress, optionsJson)`
exposes native token-preserving `code::Morph` animation, including added/removed lines and the diff
gutter. `code::font`'s bundled JetBrains Mono faces are also loadable through `font::load` with
`JetBrainsMono-Regular.ttf` or `JetBrainsMono-Bold.ttf` candidates when scripts need the same
typeface outside the code-card renderer. `examples/scripts/code_card.as` demonstrates the code-card renderer.
The `cam` helpers represent a shot as `[centerX, centerY, zoom, rotation]`; `fit`, `mix`,
`fly` and seeded `shake` write shot arrays, while `apply`/`restore` transform the drawing
context around world content and leave later HUD drawing in frame space. See
`examples/scripts/camera_card.as`. `cam::track(frame, keys, frameW, frameH, shot)` accepts
up to 64 eight-value rows: `[frame,cx,cy,zoom,rotation,easingIndex,flyFlag,rho]`; the first
row must use frame 0, matching native `Track::start`, and subsequent rows mirror native
`Track::to`/`Track::fly`; frame times can repeat or go backward; native `Track` methods clamp each key to at least the preceding frame. Script key times are limited to -10,000,000..10,000,000. Set `easingIndex` to `-1` on a later row with `flyFlag` 0 to
mirror `Track::hold(frame)`; the row's shot columns must repeat the previous shot, which is held.
The same `-1` marker works in `cam::world` keyframe rows.
`cam::trackCurves(frame, keys, names, parameters, frameW, frameH, shot)` accepts seven-value rows `[frame,cx,cy,zoom,rotation,flyFlag,rho]`, plus one easing name and parameter per segment; names use the same `in-`, `out-` and `inOut-` curves as `anim::curve`, while the name `hold` with `flyFlag` 0 maps `Track::hold`. `cam::trackFunctions(frame, keys, callbacks, frameW, frameH, shot)` uses the same seven-value rows and one global `double name(double progress)` callback per moving segment; an empty callback name with `flyFlag` 0 maps `Track::hold`. All three track forms clamp repeated or backward key times to the preceding frame, matching native `Track::to`, `fly`, and `hold`; script key times are limited to -10,000,000..10,000,000. Callbacks run under the composition statement budget and must return finite values. `cam::world(keyframes, frameW, frameH, parallax)` installs one composition-wide native camera node around every timeline item, including audio and video. It accepts the same eight-value rows as `cam::track`. `cam::worldFunction(name, frameW, frameH, parallax)` accepts a global `string name(int localFrame)` callback returning `[centerX,centerY,zoom,rotation]` as JSON; it runs under the composition statement budget. Both camera world forms apply to media, layers and drawing calls. See `examples/scripts/camera_world.as` and `camera_world_function.as`. `cam::toFrame`, `toCanvas` and `visible` expose the native camera coordinate helpers.

`prefab::use(start, duration, name, propsJson)` adds a registered native prefab to the
composition timeline during `configure`. It can use prefabs registered by the host app,
including social badges, flags, rig components, and project-specific prefabs.
`prefab::nestCue(parentSceneIndex, cueIndex)` attaches a registered prefab cue under a scene;
the cue index is the zero-based `prefab::use` declaration order, and its timing is local to that
parent. Script-defined prefabs are scenes and use `nestScene`.
`prefab::define(name, drawFunction, defaultPropsJson)` defines a composition-local prefab from a
script `void callback(int frame)` function. Define it before calling `prefab::use`; each instance
calls the function on its own timeline with merged default and instance props available through
`props::*`. `prefab::count()`,
`prefab::name(index)`, and `prefab::defaultsJson(name)` inspect the host-registered prefab catalog.

```cpp
prefab::use(0, 90, "flag-id", "{}");
prefab::use(90, 90, "social-youtube", "{\"handle\":\"@raymotion\"}");
```

Timeline helpers `sequence`, `loop(start, every, times, function)`, and
`freeze(start, duration, localFrame, function)` define frame remapping for scripted scenes.
`series(startFrame)` returns a `TimelineSeries` scheduler: `add(duration, offset)` returns
the next scene's start and advances `end()`, matching native `Series` placement. Pass that
start to `sequence` or `scene`:

```cpp
TimelineSeries@ timing = series();
sequence(timing.add(90), 90, "opening");
sequence(timing.add(60, -12), 60, "turn");
int afterLastScene = timing.end();
```

To reproduce native `TransitionSeries`, schedule the entering scene with a negative offset
equal to `min(transitionFrames, previousDuration, enteringDuration)`, then connect the adjacent
scene indices with `sceneTransition` (or `sceneTransitionFunction` for a callback easing).
`sceneTransition` implements native `TransitionSeries.connect` directly.

`nestScene(parentIndex, childIndex)` attaches a previously declared scene beneath another scene;
indices use declaration order, and the child's start/duration are local to the parent's duration.
Nested scenes share the parent transform and can themselves contain children; loops, freezes,
layers and transitions keep their native timeline behavior. `renderer(functionName)` selects a
global `void function(int frame)` called once per output frame with the canvas bound; it replaces
timeline-tree drawing and uses the composition statement budget.
`motionBlur(samples, shutter)` configures composition-wide temporal sampling (1..32 samples,
shutter 0..1). See `examples/scripts/timeline_card.as`.
Animation callbacks can call `frameTime(frame)` to get the current fractional frame, including
the renderer's shutter sample offset, or `subframe()` to read just that offset. Use `frameTime`
as the input to interpolation, spring, or procedural motion calculations so scripted drawings
participate in temporal blur. The animation-curves example demonstrates this with eight samples.

Scripted scenes can also use the native isolated-layer pipeline. `layer(sceneIndex, opacity,
offsetX, offsetY, blend, maskMode, invertMask, blurSamples, shutter, maskFunction)` configures
an already-declared scene. Blend is `normal`, `add`, `multiply`, or `screen`; mask mode is
`alpha` or `luma`. Leave `maskFunction` empty for an unmasked layer, or name a draw function
whose output supplies the mask. `layerFilter(sceneIndex, filterJson)` appends a native filter
to that scene's isolated layer; pass one JSON object per call. Supported types are `posterize`
(`levels`), `halftone` (`size`, `angle`, `colored`, `paper`), `paper` (`size`, `strength`),
`inkEdges` (`size`, `ink`, `threshold`), `watercolor` (`size`, `strength`, `granulation`), and
`tiltShift` (`position`, `band`, `size`, `angle`, `strength`, `falloff`), and `film` (`exposure`,
`bloom`, `size`, `threshold`, `glow`, `contrast`, `saturation`, `shadows`, `highlights`, `toning`,
`vignette`). Layers allow up to 16 filters.
`sceneAnimate(sceneIndex, stateFunction)` attaches a `string stateFunction(int frame)` callback to animate a scene node without creating an isolated layer. Return `{"node":{"x":...,"y":...,"scale":...,"rotation":...,"opacity":...}}`; omitted node fields use the scene base values. The callback is evaluated from the scene-local remapped frame and resets from base values each frame. `layerAnimate(sceneIndex, stateFunction)` attaches a frame callback declared as
`string stateFunction(int frame)`. Return a JSON object with optional `opacity`, `offset`,
`blend`, `maskMode`, `invertMask`, `enabled`, `blurSamples`, `shutter`, `filters`, or
`maskFunction` fields to set absolute layer state for that frame. A `node` object can animate
the parent node's `x`, `y`, `scale`, `rotation`, and `opacity`; omitted fields keep their static
values. This callback runs on the unremapped scene frame, matching native `layerUp` timing.
See `examples/scripts/layer_card.as`.

Existing render options such as supersampling, PNG frames, alpha, formats and worker
jobs continue to operate through the normal renderer. Some lower-level and specialized
module features still need direct `.as` bindings; native C++ remains the complete interface
until that coverage work is finished.

The direct diagram handle now covers the native animated `dg::Diagram` actions and graph/state
queries. Other specialized native modules still need a full field-by-field parity audit.

Source errors report the filename, line and column. Runtime exceptions report the
function and line. Each callback defaults to a one-million statement-callback budget.
Set `c.statementBudget` to a value from 1 through 100,000,000 for heavier drawing
loops. Runaway callbacks abort with a diagnostic. This is a responsiveness guard,
not a security sandbox or a memory quota. Programs are used on the renderer thread,
not concurrently.

Scripts are trusted code, like the C++ compositions: they can load any path they name,
write files with `writeText`, and use as much memory as they like. Run only scripts
you would run as a program; raymotion does not sandbox them.

## Additional languages

`include/rmtn/script.hpp` is independent of AngelScript. A backend implements:

- `Program::configure(props) -> Description`: metadata and named scenes, without GL.
- `Program::draw(function, ctx, localFrame, props, statementBudget)`: drawing inside
  the native renderer with per-composition immutable props and a bounded callback budget.
- A loader returning `shared_ptr<Program>` for a source path.

Register it once before `runCli` with
`script::registerBackend(".lua", loader)` (or `.py`, etc.). The registry handles
validation, props rebuilds, timeline nodes and callback ownership. The same
`--script` CLI works for every registered extension, including worker processes
when the executable registers that backend at startup. Backends should enforce
deterministic frames and translate interpreter failures into C++ exceptions.
VM state must remain alive until all native callbacks have been destroyed.

Lua, Python and ActionScript backends are extension points, not shipped runtimes.
Each will need its own language bindings and dependency. In particular, ActionScript
would need an appropriate runtime; it is a different language from AngelScript.

`RAYMOTION_SCRIPTING=OFF` omits the vendored AngelScript backend while retaining the
backend interface, host and CLI. AngelScript source is vendored under
`third_party/angelscript`, so configuring scripting does not fetch a new dependency
from the network. raylib can still use its normal vendored, system or populated
FetchContent mirror.
