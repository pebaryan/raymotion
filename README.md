# raymotion

[![CI](https://github.com/pebaryan/raymotion/actions/workflows/ci.yml/badge.svg)](https://github.com/pebaryan/raymotion/actions/workflows/ci.yml)

[![raymotion: the introduction video on YouTube](docs/images/raymotion-promo.jpg)](https://youtu.be/tcALbxoO9x0)

Remotion-style motion graphics / video engine — pure C++17 on **raylib + ffmpeg**, no
web stack. Compositions are code: a timeline tree of nodes with keyframe/easing tracks
and transforms, drawn deterministically per frame, supersampled for clean edges, and
piped straight into ffmpeg.

▶ **[Watch the 3.5-minute introduction on YouTube](https://youtu.be/tcALbxoO9x0)**: how it works,
the technology underneath, and every feature demonstrated, all rendered by raymotion itself.

It is built for **technical and scientific explainers**: GPU 3D scenes with illustration
styles, and diagrams, process models, knowledge graphs, molecules and proteins loaded
from their own file formats and animated step by step. A single native binary often renders
1080p faster than real time, and coding agents can write videos with the
[raymotion agent skill](#agent-skill).

## Build

**Windows binary:** each [release](https://github.com/pebaryan/raymotion/releases) has a
`raymotion-<version>-windows-x64.zip` with the showcase executable and its assets; it needs
ffmpeg on `PATH`, an OpenGL 3.3 driver and the Visual C++ runtime. To write your own
compositions, build from source:

Needs CMake 3.21+, a C++17 compiler, and ffmpeg on `PATH` for MP4 output.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

raylib 6.2 comes from one of (`-DRAYMOTION_RAYLIB=...`):

| mode | |
|---|---|
| `auto` (default) | `vendored` with 64-bit MSVC, otherwise `fetch` |
| `vendored` | prebuilt MSVC lib in `third_party/raylib` (release `/MD`) |
| `fetch` | downloads and builds raylib 6.2 from source (Linux needs the X11/GL dev packages listed in `.github/workflows/ci.yml`) |
| `system` | `find_package(raylib 6.2)` |

Pushing a `v*` tag runs `.github/workflows/release.yml`, which builds, tests and attaches
the Windows zip (`tools/package-windows.ps1`) to that tag's release.
CI builds Linux (full suite under Xvfb + Mesa), Windows (both raylib modes) and macOS.
Building on the original stripped-VS development machine is described in
[docs/local-toolchain.md](docs/local-toolchain.md).

## Agent skill

`skills/raymotion/` teaches coding agents (Claude Code, Codex, Cursor and others that read
[Agent Skills](https://agentskills.io)) to set up a project, write compositions, check
them with stills and render them:

```sh
npx skills add pebaryan/raymotion
```

It covers the authoring API, 3D and illustration styles, diagrams, molecules and
proteins, and the rules that keep frames deterministic, with a project template in
`skills/raymotion/templates/`.

## Usage

```
build/raymotion list
build/raymotion preview <name>                 # live window; esc or close quits
build/raymotion render  <name> [--crf N] [--draft] [--no-music] [--png-frames] [--ss N]
                               [--format mp4|prores|webm] [--alpha] [--jobs N]
build/raymotion still   <name> --at FRAME [--png PATH] [--ss N] [--alpha]
build/raymotion props   <name>                 # print the default props as JSON
build/raymotion prefabs                        # list prefabs and their default props
# any command:  --props '{"title": "Hi"}'  or  --props @props.json
```

* `render` draws every frame into a composition-sized canvas and pipes raw RGBA into
  ffmpeg -> `out/<name>.mp4` (+ AAC soundtrack from the procedural synth for `music=1`
  comps). Frames are read back asynchronously and written to ffmpeg from a separate
  thread, so drawing, readback and encoding overlap. `--encoder NAME` picks any ffmpeg
  encoder (default `h264`, i.e. libx264; `h264_nvenc`/`hevc_nvenc` on NVIDIA GPUs) and
  `--preset NAME` its speed; `--crf N` maps to each encoder's own quality setting.
  `--png-frames` writes `out/<name>_frames/fNNNNN.png` instead and needs no ffmpeg.
* **Transparent video for editors.** `--alpha` renders with a transparent background
  (whatever the composition's `background`) into ProRes 4444 with alpha,
  `out/<name>.mov`, which Premiere, Resolve and Final Cut import as a clip with
  transparency. `--format webm` writes VP9 with alpha instead (browsers, OBS), and
  `--format prores` without `--alpha` gives an opaque ProRes 4444 master. `--alpha` also
  works with `still` and `--png-frames` (RGBA PNGs). The engine composites in
  premultiplied alpha and hands the encoder straight alpha, so antialiased and
  translucent edges keep their colour over any footage. Anything the composition paints
  itself (a full-frame backdrop) stays opaque, so leave the background to `--alpha`.
  Without `--alpha`, every output is opaque and a transparent `background` shows as
  black.
* **Parallel chunks.** `--jobs N` splits the frames into N contiguous ranges, renders
  each in its own process with the final codec, then joins them by stream copy and
  muxes the soundtrack once. Frames carry no state, so the result is the same video
  (ProRes chunks join bit for bit). It pays off when the CPU is the limit: PNG
  sequences (3.5x faster with 4 jobs on a 12-core machine), VP9/WebM, CPU-heavy
  compositions. A plain H.264 render is usually limited by moving 1080p RGBA frames
  from the GPU into ffmpeg, which processes share, so extra jobs gain little there;
  hardware encoders also cap concurrent sessions.
* ffmpeg lookup: `--ffmpeg PATH`, `RAYMOTION_FFMPEG`, a winget Gyan.FFmpeg install,
  `C:/Program Files/ffmpeg`, `C:/ffmpeg`, then `PATH`.
* Output is exactly the composition size regardless of the desktop. `--draft` renders
  half size (even dimensions) without supersampling.
* Frames carry no state: `still --at N` is identical to frame N of a full render.
* `--props` is merged (objects key by key) over the composition's `defaultProps`; the
  composition rebuilds from the result, so props can change its length or size too.

## Writing a project

The engine is the `raymotion::engine` library; a project registers compositions and
hands them to the CLI. `examples/starter/main.cpp` is a complete one:

```cpp
#include "rmtn/raymotion.hpp"
using namespace rmtn;

static void registerComps(Registry& out) {
    Comp c; c.name = "hello"; c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    TNode* title = addRoot(c.roots, 0, 90);
    title->props.pos = { 640, 360 };
    title->up = [](int t, NodeProps& p) {
        TrackFloat o; o.add(0, 0.f); o.add(18, 1.f, E_OUT_CUBIC); p.opacity = o.at(t);
    };
    title->fn = [](const Ctx& cx, int) {
        dr::text(fonts::ui(true), cx, "Hello, raymotion", { 0, 0 }, 64, 0, WHITE, 0.5, 0.5);
    };
    out.push_back(std::move(c));
}
int main(int argc, const char** argv) { return runCli(argc, argv, registerComps); }
```

```cmake
include(FetchContent)
FetchContent_Declare(raymotion GIT_REPOSITORY https://github.com/pebaryan/raymotion.git GIT_TAG main)
FetchContent_MakeAvailable(raymotion)
add_executable(myvideo main.cpp)
target_link_libraries(myvideo PRIVATE raymotion::engine)
```

### Authoring API

Remotion's building blocks, as C++ (the starter's `promo` composition uses all of them):

```cpp
// Timing: sequences, loops, freezes, back-to-back series.
Series s(c.roots);
s.add(60)->fn = intro;
s.add(90, -10)->fn = body;                          // overlaps the intro by 10 frames
loop(c.roots, 0, 30, 4)->fn = pulse;                // local t = 0..29, four times
freeze(c.roots, 200, 40, 15)->fn = body;            // holds the body's frame 15

// Values: interpolate (Extend / Clamp / Identity / Wrap), colours, springs, easings.
p.opacity = interpolate(t, {0, 20}, {0, 1}, clamped(ease::out(ease::cubic)));
p.scale = spring(t, {30.0});                        // fps; also config {mass, stiffness, damping}, from/to, delay
Color c = interpolateColors(t, {0, 30}, {RED, BLUE});
Easing css = ease::bezier(0.25, 0.1, 0.25, 1);

// Layers: the subtree renders offscreen, then composites as one group.
Layer& l = node->isolate();
l.opacity = 0.5f;                                   // no seams where children overlap
l.blend = Blend::Screen;                            // Normal, Add, Multiply, Screen
l.mask = [](const Ctx& cx, int t) { dr::circle(cx, {640, 360}, 10.f * t, WHITE); };
l.filters = { Filter::watercolor(6), Filter::paperGrain() };   // illustration looks, see below

// Transitions between scenes (fade / slide / wipe, with a direction and easing).
TransitionSeries ts(c.roots, c.w, c.h);
ts.add(90)->fn = sceneA;
ts.transition(Transition::slide(20, Direction::FromRight));
ts.add(90)->fn = sceneB;
c.duration = ts.end();

// Layout: flex rows/columns, grids, wrapped and aligned text boxes.
auto cols = layout::row(layout::inset(layout::fill(c.w, c.h), 64), {layout::px(320), layout::flex()}, 24);
dr::textBox(cx, "Wraps to the box, aligned", cols[1], {fonts::ui(true), 48});

// Props: defaults + a build step that runs with the merged input props.
c.defaultProps = Json::parse(R"({"title": "Hello", "accent": "#47dad8"})");
c.build = [](Comp& c, const Json& props) {
    /* read props.str("title", ""), props.color("accent", WHITE), ...; fill c.roots */
};
// Files the build reads (generated textures, models, audio): made only before frames are
// drawn, never by `list`, so build must still report size, duration and audio without them.
c.assets = [](const Comp& c, const Json& props) { /* write out/my-assets/... */ };

// Prefabs: named, parameterized subtree builders, registered once and instantiated
// anywhere (Remotion-style components). The caller owns timing; props merge over the
// prefab's defaults exactly like a composition's.
prefab::add({ "card", Json::parse(R"({"accent": "#47dad8", "delay": 0})"),
              [](TNode& n, const Json& p) {
                  n.up = [d = p.integer("delay", 0)](int t, NodeProps& pr) {
                      pr.opacity = (float)interpolate(t - d, { 0, 15 }, { 0, 1 }, clamped());
                  };
                  /* fill n.kids, add audio, isolate() a layer, ... */
              } });
prefab::use(root, 30, 60, "card", Json{{"title", "Fast"}});   // root->add(30, 60) + apply
```

### GPU 3D

`rmtn/scene3d.hpp` puts Three.js-style scenes on the timeline (like `@remotion/three`):

```cpp
auto scene = std::make_shared<g3d::Scene>();
scene->camera.position = {0, 3, 8};
scene->add(g3d::Geometry::plane(20, 20), {mkColor(0x30364a), 0.9f});      // color, roughness
g3d::Object3D* knot = scene->add(g3d::Geometry::knot(1, 0.35f), {mkColor(0x47dad8), 0.25f});
knot->up = [](int t, g3d::Object3D& o) { o.position.y = 1.5f; o.rotation.y = t * 2.f; };
scene->lights.push_back(g3d::Light::directional({-6, 12, 5}, WHITE, 1.1f, /*shadow*/ true));
scene->lights.push_back(g3d::Light::spot({6, 7, 6}, {0, 0, 0}, ORANGE, 2, 24));
scene->fogNear = 14; scene->fogFar = 34;
scene->environmentGenerator = g3d::studioEnvironment(mkColor(0x3a4670), mkColor(0x1a2036), BLACK);
scene->camera.dof = {/*focusDistance*/ 12, /*aperture*/ 1.2f, /*maxBlur*/ 10};

addRoot(c.roots, 0, 240)->fn = g3d::draw(scene);                          // full frame
addRoot(c.roots, 60, 180)->fn = g3d::draw(scene, {1376, 716, 480, 300}, topCamera);  // inset view
```

* Shapes (box, sphere, plane, cylinder, cone, torus, torus knot) and model files
  (`g3d::loadModel`: glTF/GLB, OBJ, IQM, M3D, VOX) with their textures; glTF skeletal
  animations play on the GPU (`object->animation`, `animationFrame`).
* Materials: colour/texture, roughness, metalness, emissive, opacity (sorted
  transparency), unlit, double-sided. Lighting is in linear space with sRGB output.
* Lights: up to 8 directional/point/spot lights with range falloff. Any of them can cast
  PCF shadows; they share one shadow atlas (`shadowMapSize` per tile, a tile per
  directional/spot light and six per point light, 16 tiles), with `shadowBias` in world
  units on top of an automatic slope bias.
* Ambient light is a sky/ground hemisphere, or image-based: `scene->environment` (an
  equirectangular PNG/JPG/HDR) or `environmentGenerator` (e.g. `g3d::studioEnvironment`,
  a gradient with soft light boxes) lights diffuse surfaces from its blurred mips and
  gives reflections that blur with roughness; `showEnvironment` draws it as a skybox.
* A procedural sky (`scene->sky`): a gradient from zenith to horizon to ground, a sun
  disc and glow (`sunLight` points it along a light), stars that twinkle, and fbm clouds
  that drift. It is remade each frame, so all of it can be animated, and it is also the
  scene's environment, so it lights and reflects. Presets `Sky::day()`, `dawn()`,
  `dusk()`, `night()`, `Sky::mix(a, b, t)`, and `Sky::at(hour)` for a whole day.
* Fog: linear (`fogNear`/`fogFar`), or `FogMode::Exponential`/`Exponential2` with
  `fogDensity`, thinning with height above `fogHeight` by `fogFalloff` (ground fog in a
  valley). With `fogFromSky` it takes the sky's colour in each direction, so far hills
  melt into the horizon behind them.
* Depth of field per camera (`camera.dof`): a golden-angle gather blur from the depth
  buffer, sized in composition pixels so it looks the same at every supersampling level.
  `dof.tilt` (degrees) tilts the plane of focus like a tilt-shift lens: positive keeps a
  receding ground sharp, negative leaves a thin sharp band (the miniature look).
* Object hierarchy with Euler transforms; every object, light and camera has an `up`
  callback driven by the node's local frame.
* A scene renders offscreen at canvas resolution (so it is supersampled), then
  composites over the 2D content under it with the node's opacity, in any viewport.
  `gpu-3d` renders 240 frames of 1080p with IBL, two shadow casters and DOF in about 4 s.

### Non-photorealistic rendering

Illustration styles come from three independent pieces that combine freely: how a 3D
material is shaded, ink lines around 3D objects, and image filters on any layer.

```cpp
g3d::Material m{mkColor(0xff6b5a)};
m.shading = g3d::Shading::Toon;               // Standard, Toon, Gooch, Hatch
m.toon.bands = 2;                             // flat light steps per light
m.toon.rim = Color{255, 255, 255, 110};       // rim light; alpha is its strength

scene->lines.enabled = true;                  // silhouettes, inner contours, creases
scene->lines.width = 3;                       // composition px, at any supersampling
scene->lines.wobble = 1.6f;                   // hand-drawn waver that re-rolls every
scene->lines.boilEvery = 3;                   // 3 frames ("boiling" lines)
ground.lineWidth = 0;                         // per material: no lines; or lineColor

TNode* art = addRoot(c.roots, 0, 300);
art->isolate().filters = { Filter::watercolor(7), Filter::paperGrain() };
art->add(0, 300)->fn = g3d::draw(scene);      // 2D drawn in `art` gets the wash too
```

* **Shading** (`g3d::Shading`): `Toon` quantizes each light into `bands` with soft edges
  (`softness`), adds a hard highlight and a rim light, and keeps shadows; `Gooch` shades
  from a cool to a warm colour as the key (first) light comes round, for technical
  illustration; `Hatch` draws pen strokes over the base colour, denser and crossed as the
  light falls off (`ink`, `spacing`, `angle`, `weight`; screen-space strokes).
* **Ink lines** (`Scene::lines`): a pass over object ids, view-space normals and depth
  finds outer silhouettes, contours where a surface passes in front of itself (a depth
  jump the surface's slope does not explain, `depthThreshold`) and creases
  (`creaseAngle`). Lines have a constant width in composition px for any mesh, the nearer
  object owns an edge (its `lineColor`/`lineWidth`), and where two objects touch the one
  with lines draws the contact. They are drawn before depth of field and respect
  viewports. With `wobble` they waver like a hand-drawn line and boil every `boilEvery`
  frames, deterministically.
* **Layer filters** (`Layer::filters`, run in order on the flattened layer): `posterize`
  (brightness steps, hue kept), `halftone` (mono or CMYK dot screens), `paperGrain`,
  `inkEdges` (dark lines where colour or alpha changes) and `watercolor` (bleeding edges,
  darkened rims and pigment granulation, after Bousseau et al.) and `tiltShift` (a sharp
  band at `position` and `angle`, blurring and more saturated beyond it: a miniature in
  2D, for any layer, not just 3D). Sizes are composition
  px and the patterns are fixed to the canvas, so a look holds at any supersampling and
  does not crawl between frames.

`npr-styles` shows one still life in eight styles (Standard, Toon, Toon + ink, Gooch,
Hatching, Watercolour, Comic halftone, Poster) and then six of them side by side.

### Diagrams and structures

Existing diagrams and structures load from their own file formats, lay themselves out
(built in, no Graphviz needed) and animate on the timeline.

```cpp
auto d = dg::Diagram::make(dg::load("data/support.mmd"));   // .mmd .dot .bpmn .ttl .jsonld .json .nwk
d->build(0, 6, 18);                           // nodes pop in flow order, edges draw on
d->trace(90, 60, {"report", "triage", "fix", "close"});   // a pulse runs the path, lights it
d->flow(160, 90, {"fix"}, 3);                 // packets along a node's out-edges
d->dim(260, 20, {"triage", "fix"});          // fade everything else back
d->focus(260, 40, {"triage"}, 2);             // the camera glides in; {} returns
d->morph(320, 40, dg::load("data/v2.mmd"));   // matching ids move, new ones fade in
addRoot(c.roots, 0, 400)->fn = dg::draw(d, {80, 40, 1760, 760});

auto mol = chem::Depiction::make(chem::smiles("CN1C=NC2=C1C(=O)N(C(=O)N2C)C"));
mol->build(0).highlight(60, 20, chem::find(mol->molecule, "C=O")[0]);
g3d::Object3D* ball = chem::addMolecule(*scene, chem::loadMolecule("data/caffeine.sdf"));
g3d::Object3D* prot = chem::addCartoon(*scene, chem::loadProtein("data/1crn.pdb"));
chem::buildIn(prot, 0, 2, 10);                // grows residue by residue
```

* **Formats** (`dg::`): Mermaid flowcharts (every node shape, link styles and labels,
  subgraphs, `classDef`/`class`/`style`/`linkStyle`) and state diagrams; Graphviz DOT
  (attributes, default scopes, clusters, `rankdir`, `pos`); BPMN 2.0 XML (pools and lanes,
  events with their definitions, gateways, typed tasks, sub-processes, data objects,
  message flows), drawn where its diagram interchange puts it; RDF as Turtle, N-Triples
  or JSON-LD (labels from `rdfs:label` and friends, colours by `rdf:type`, literals as
  nodes or data); plain JSON and Cytoscape graphs; Newick trees.
* **Layout** (`dg::layout`): layered (Sugiyama: cycle breaking, crossing reduction,
  groups kept together, edge labels in their own slots, curved routes) for flowcharts;
  stress-majorisation force layout for graphs; tidy and radial trees with branch lengths
  for phylogenies. Positions from the file (BPMN, DOT `pos`) are kept.
* **Themes**: `light`, `dark`, `blueprint` and `bpmn` (bpmn.io's look), or set fonts,
  colours, stroke widths and a palette for node kinds yourself.
* **Chemistry** (`chem::`): SMILES (aromatic rings kekulised, implicit hydrogens), MOL /
  SDF V2000 and V3000, PDB; ring perception, formulas, masses and substructure search;
  2D skeletal formulas (ring templates, coloured heteroatoms, wedges) that build and
  highlight; 3D ball-and-stick, sticks and space-filling models (with a rough embedding
  when the file has no 3D coordinates); protein cartoons with helix ribbons, strand
  arrows and coil tubes, coloured as a rainbow, by chain or by structure.

`diagrams` walks through a Mermaid flowchart, a DOT graph morphing into a bigger one, a
BPMN process with a token run, a knowledge graph with focus and dimming, a phylogeny,
caffeine in 2D and 3D and the crambin protein. The sample files are in `assets/data/`
(crambin from the RCSB PDB and caffeine from PubChem, both public domain).

### Vector paths

```cpp
vg::Path curve = vg::Path::parse("M 100 500 C 400 100 800 900 1100 500");   // or moveTo/lineTo/cubicTo/arcTo
vg::Stroke s;
s.color = SKYBLUE;
s.width = 8;
node->fn = [curve, s](const Ctx& cx, int t) {
    const float u = (float)interpolate(t, { 0, 60 }, { 0, 1 }, clamped());
    vg::stroke(cx, curve.trim(0, u), s);                   // draws itself on
    dr::circle(cx, curve.pointAt(u), 10, GOLD);            // a marker riding the curve
};
```

`vg::` is Bézier geometry plus a renderer for it:

* **Paths:** lines, quadratic and cubic curves, SVG elliptical arcs, and full SVG path data
  (`Path::parse`, every command, relative or absolute). Shapes: rect (rounded), circle,
  ellipse, arc, regular polygon, star, polyline, and a smooth curve through points.
  Paths can be translated, scaled, rotated, reversed or put through any point function.
* **Measuring:** `length`, `pointAt(u)` and `angleAt(u)` by arc length (for motion along a
  path), `bounds`, and `trim(from, to)`, which cuts the curves exactly. Trims can run through
  the contours in order or cut every contour at once.
* **Fills** follow SVG's nonzero and even-odd rules exactly, with holes and self-crossing
  contours. The GPU counts each pixel's winding number in an offscreen pass, so translucent
  fills never double up.
* **Strokes:** miter, round and bevel joins; butt, round and square caps; dash patterns
  with an offset (marching ants, dotted lines). Translucent strokes cover each pixel once,
  even where they cross themselves.
* **Animation helpers:** `vg::write` (the outline draws on, then the fill fades in) and
  `vg::Morph`, which matches contours, resamples them and aligns them so shapes turn
  into one another with the least travel. `vg::mix` blends two styles.
* **SVG files:** `vg::Drawing::load("art.svg")` reads paths, basic shapes, groups,
  transforms and styles (through nanosvg; gradients are drawn as their average colour).
  `vg::draw` and `vg::write` fit the drawing into a box, and `write` reveals the shapes
  one after another.

`vector-paths` shows draw-on with a marker, joins, caps and dashes, both fill rules, a
morph sequence, an SVG landscape writing itself on (`assets/data/landscape.svg`) and
motion along a path.

### Equations

```cpp
auto f = math::Formula::tex(R"(\id{lhs}{e^{i\pi}} + 1 = 0)", { 96 });   // typeset once, while registering
node->fn = [f](const Ctx& cx, int t) {
    math::Look look;
    look.colors["lhs"] = GOLD;                                           // colour a named part
    math::write(cx, f, { 960, 540 }, { 0.5f, 0.5f }, t / 60.f, look);    // writes itself on
};
```

`math::` typesets a TeX subset inside the engine. There is no LaTeX install and no
external tool. It reads the OpenType MATH table of a math font (Latin Modern Math is
bundled) and follows TeX's layout rules: atom spacing, script shifts and cramped styles,
fraction and stack gaps, radicals, and limits on big operators. Delimiters stretch through
the font's size variants and glyph assemblies. Every glyph and rule comes out as a vector
path, so formulas stay sharp at any scale and draw through `vg::`.

* **Input:** math italic letters, Greek, upright digits and ~200 symbols; `^ _` and primes;
  `\frac \dfrac \tfrac \binom \sqrt[n]{}`; `\sum \prod \int \oint \lim \max ...` with
  `\limits/\nolimits`; `\left \middle \right` and `\big ... \Bigg`; accents (`\hat \bar \vec
  \dot \tilde \widehat ...`), `\overline \underline`; `\text \mathrm \mathbf \mathbb \mathcal
  \mathfrak \operatorname`; spaces `\, \: \; \! \quad \qquad`; `matrix pmatrix bmatrix
  vmatrix Bmatrix cases aligned gathered`, and a top-level `\\` (with `&` to align).
  Unknown commands throw with their position.
* **Parts:** `\id{name}{...}` names a piece of the formula. `Look` recolours parts or fades
  them; `place` and `bounds(part)` give a part's ink box for highlight boxes, braces and
  labels. `\color` and `\textcolor` colour the source directly.
* **Animation:** `write` reveals the glyphs left to right, each outline drawing on and then
  filling. `math::Transform(a, b)` turns one formula into another: pieces in parts of the
  same name pair up first, then equal glyphs in reading order. Matched glyphs glide and
  rescale, and the rest fade out or in.

`equations` writes Euler's identity and the quadratic formula on, shows a gallery of
layouts, labels the parts of E = mc², solves a quadratic step by step and rearranges
Pythagoras by named parts.

### Plots and charts

```cpp
plot::Axes ax({ 260, 140, 1400, 760 }, { -6.5, 6.5 }, { -2, 2 });   // box (node px), x range, y range
const vg::Path sine = plot::graph(ax, [](double x) { return std::sin(x); });   // build once
node->fn = [ax, sine](const Ctx& cx, int t) {
    plot::AxesStyle st;
    st.xLabel = "x";
    st.yLabel = R"(\sin x)";
    plot::axes(cx, ax, st, t / 30.f);                                  // grows from the origin
    vg::stroke(cx, sine.trim(0, (t - 20) / 60.f), vg::Stroke(SKYBLUE, 5));
};
```

`plot::` covers mathematical plots and data charts. Everything is drawn as vector paths,
numbers and labels are set in the math font, and every drawing call takes a reveal
parameter `u`:

* **Axes:** `Axes` maps a data range onto a box; `axes()` draws axes through the origin
  or framed at the edges, with arrows, ticks, numbers, grids and TeX axis labels. Tick steps
  are picked automatically (1, 2 or 5 × 10ⁿ) and widen until labels fit. `numberLine()`
  is a flat `Axes`.
* **Graphs** come back as `vg::Path`s you can stroke, trim, measure or morph:
  `graph(axes, f)` samples adaptively, breaks at poles and holes (tan x, 1/x, √x), and
  clips to the box. Also `parametric`, `polar`, `area` (between two functions), `riemann`
  rectangles and `tangent` lines.
* **Fields:** `field(axes, v)` draws arrows on a grid, coloured and sized by magnitude.
  `scatter` places dots.
* **Charts:** a `Table` (categories × series, from code or a CSV file) feeds `bars`
  (grouped or stacked), `lines` and `pie` (or a donut). They grow in with `u`.
  `Table::mix(a, b, w)` blends two datasets, so a chart can animate to new data.
  `areas` stacks or overlaps filled series, and `waterfall` runs changes into a total.
* **Statistics:** `boxplot` and `violin` take raw samples per group; `summarize`,
  `density` (a kernel estimate you can `graph`) and `bin` do the numbers, `histogram` and
  `errorBars` draw them. `heatmap` colours a table's cells with `viridis` or `diverging`
  (or any map) and adds a colour bar.
* **Keys:** `colorbar` draws a colour scale (upright or across, with ticks and a title)
  and `legend` a column or row of swatches, for charts of your own (3D ones included).
* **Business charts:** `bubbles` (area ∝ size), `radar`, KPI `gauge`s with colour zones,
  `funnel`s and `candles` (OHLC).
* **Hierarchies and flows:** a `Tree` (built in code or parsed from `"Sales/Field, 22"`
  lines) feeds `sunburst` and squarified `treemap`s. `Flow`s (`"from, to, value"`) feed
  `sankey`, which lays the nodes out in columns and orders them to keep ribbons short, and
  `chord`, which sets the groups round a circle with ribbons between them.
* **More dimensions:** `contour(axes, f)` builds contour lines and filled bands of
  f(x, y) once (marching squares), and `contours` draws them with level labels.
  `parallel` draws parallel coordinates (one axis per series, a line per row, coloured by
  a dimension, with an optional highlighted row), and `stream` draws a stream graph.

`plots` draws sin and cos with a tangent riding the curve, refines Riemann sums into an
integral, traces a Lissajous figure and a polar rose, sends a trajectory through a vector
field, walks a point along a number line, and animates bar, line and donut charts.
`charts` shows the rest: box plots and violins, a histogram with its density, error bars,
a heatmap, bubbles, radar and gauges, a waterfall and a funnel, a sunburst and treemap of
one budget, a Sankey and a chord diagram, a stream graph, a contour map, parallel
coordinates, stacked areas and candlesticks.

### Spaces and geometry

```cpp
node->fn = [](const Ctx& cx, int t) {
    const float k = std::clamp((t - 60) / 60.f, 0.f, 1.f);         // 0: flat, 1: 3D
    space::View v = space::View::mix(space::View::flat({ 960, 540 }, 110),
                                     space::View::tilted({ 960, 560 }, 100), k);
    space::Scene s(v);
    space::axes(s, {}, {}, { 1, 1, k });                            // the z axis grows in
    s.arrow({ 0, 0, 0 }, { 2, 1.5f, 1.8f * k }, SKYBLUE, 5);
    space::guides(s, { 2, 1.5f, 1.8f * k }, GRAY);
    s.draw(cx);                                                       // depth-sorted, back to front
};
```

`space::` draws 3D and non-Euclidean scenes as vector paths, so they sit in 2D
compositions, take any `vg::Stroke` and blend with everything else:

* **Views:** a `View` projects 3D points (orthographic, or with some perspective). `flat()`
  is a 2D plot, `tilted()` a 3D view with z up, and `View::mix` blends any two, which is how
  a plot goes from 1D to 2D to 3D or turns to look along an axis.
* **Scenes:** a `Scene` collects lines, curves (dashes run on across pieces), arrows,
  points, faces, meshes, planes and TeX labels, then draws them back to front, so a surface
  in front hides a label behind it. `axes()` draws 3D axes with ticks, numbers, grids and
  labels, each growing out on its own; `guides()` drops dashed lines to the planes.
  `projectOnPlane` and `projectOnLine` give shadows and projections.
* **Surfaces:** `Mesh::graph(f)` (z = f(x, y)) or `Mesh::grid(u, v)`, coloured per face or
  per cell, shaded, with a wireframe. `Mesh::mix` morphs one into another.
  `surfaces::` has a sphere, a torus, `glueTorus(roll, bend)` (a sheet rolled into a tube
  and bent round into a torus), `moebius(join)` and a Klein bottle.
* **Four dimensions:** `rotate4` in any of the six planes, `project4` to 3D, `tesseract()`.
* **Hyperbolic geometry:** the Poincaré disk and the upper half-plane (`toHalfPlane`,
  `toDisk`), distances, `isometry` (move a point to the centre and turn), geodesic
  segments and lines, polygons, circles, reflections, and regular `{p, q}` `tiling`s.
  `path()` maps the sampled points to screen paths, clipped if you like.
* **Spherical geometry:** great-circle `arc`s, small `circle`s, spherical `angle`s and
  `stereographic` projection (and back).

`spaces` walks a point from a number line into 3D, shows a helix's shadows from above and
from the side, raises a surface, glues a torus, joins a Möbius band beside a turning Klein
bottle, draws a triangle with three right angles and projects it stereographically, moves
the `{7,3}` tiling by an isometry and opens it into the half-plane, and turns a tesseract.

### Camera

```cpp
cam::Track track;
track.start(cam::Shot::fit({ 0, 0, 6000, 3400 }))                 // the whole canvas
     .to(90, cam::Shot::fit({ 400, 2100, 1000, 700 }))            // glide in
     .fly(200, cam::Shot::fit({ 4600, 600, 1000, 700 }))          // out, across and back in
     .to(260, cam::Shot{ { 5000, 900 }, 8, -10 });                // zoom 8, turned 10 degrees
cam::world(c.roots, 0, 300, track, 0.35f)->fn = stars;            // a far layer: parallax
cam::world(c.roots, 0, 300, track)->fn = canvas;                  // the canvas, in canvas px
addRoot(c.roots, 0, 300)->fn = hud;                               // fixed to the frame
```

`cam::` is a 2D camera that pans, zooms and turns over a canvas bigger than the frame.
Everything under a `cam::world` node moves with it (shapes, text, paths, formulas, plots,
diagrams, 3D scenes); nodes outside stay put, like a HUD. A `Shot` is the canvas point at
the middle of the frame, a zoom and a turn; `Shot::fit` frames a rectangle. `Shot::mix`
blends two shots so the motion stays steady on screen (geometric zoom), and `Shot::fly`
is the Van Wijk–Nuij smooth zoom, which pulls back during a long pan. A `Track` keys
shots with `to`, `fly` and `hold`. `parallax` below 1 makes a far layer, above 1 a near
one. `shake` adds a seeded handheld wobble, and `toFrame`, `toCanvas` and `visible`
convert between the two spaces (for a mini-map, say).

`camera` tours a 6000 × 3400 canvas: it glides, flies, turns, shakes at a burst, zooms 11×
into an inscription and pulls back out, over a parallax starfield and under a fixed
mini-map.

### Code

```cpp
const code::Block before = code::Block::parse(R"(
function load(url) {
  return fetch(url).then(res => res.json());
})", "ts");
const code::Block after = code::Block::parse(R"(
async function load(url) {
  return (await fetch(url)).json();
})", "ts");
const code::Morph change(before, after);                           // diff once
node->fn = [=](const Ctx& cx, int t) {
    code::Style st;                                                 // theme, size, title bar, line numbers
    st.title = "load.ts";
    code::Look look;
    look.reveal = t * 2.5;                                          // typed in
    look.cursor = true;
    if (t < 150) code::draw(cx, before, { 200, 160 }, st, look);
    else code::draw(cx, change, { 200, 160 }, st, (t - 150) / 60.f);
};
```

`code::` draws code blocks for programming explainers, set in the bundled JetBrains Mono:

* **Highlighting:** C, C++, C#, Java, JavaScript, TypeScript, Python, Rust, Go, GLSL,
  JSON, shell and SQL. Strings and comments that span lines, raw strings, decorators and
  attributes, calls, members and constants all get their own colour. There are dark (One
  Dark-like) and light (GitHub-like) themes, or you can write your own `Theme`.
* **Cards:** a rounded panel with an optional title bar and line numbers. `bounds()` gives
  its box and `cell(line, col)` the position of any character, for arrows and notes.
* **Reveals:** `look.reveal` types characters in, with a caret; `look.lines` brings lines
  in one by one.
* **Focus:** `look.focus` keeps some lines lit and dims the rest behind a highlight band.
  `focusFrom` and `focusMix` glide the focus from one range to the next. `Mark`s box or
  underline a span (`Mark::text(block, line, "mid", colour)`), growing in with `u`.
* **Changes:** `Morph(before, after)` diffs two versions. Unchanged lines glide to their
  new places (re-indented lines slide sideways), edited lines keep their common tokens,
  and the rest fades out and in, with a red or green flash in the gutter. At u = 0 and 1 it
  draws exactly the two blocks.

`code` types a function in, shows six languages and the light theme, walks through a
binary search with line focus and notes, and refactors promises into async/await and
then try/catch.

### Text animation

```cpp
TextStyle st;
st.size = 84;
// Texts need their font, which loads with the window: lay them out on first use.
const auto title = txt::later([st] { return txt::Text::make("Every frame is a function of time", st); });
node->fn = [=](const Ctx& cx, int t) {
    txt::Reveal r;                                   // letter by letter, 2 frames apart
    r.by = txt::Unit::Word;                          // or Letter, Line
    r.order = txt::Order::Center;                    // Forward, Backward, Center, Edges, Random
    r.offset = { 0, 40 };                            // each word rises as it fades in
    txt::draw(cx, title(), { 960, 540 }, t, r, { 0.5f, 0.5f });
};
```

`txt::` lays text out once into glyphs (with wrapping and alignment from a `TextStyle`) and
animates each one:

* **Reveals:** by letter, word or line, with a stagger, a duration, an easing and an order.
  Each unit starts faded, offset, scaled or rotated and eases into place. `r.out` plays
  it backwards, so text leaves the same way.
* **Typing and decoding:** `typewriter` shows the first n characters, with a caret that
  blinks once typing stops. `scramble` cycles random characters until each one settles.
* **Styling:** a `Styler` changes any glyph's offset, scale, rotation, alpha, colour or
  character. `wave` and `highlight` are built in, and `chain` runs several.
* **Morphs:** `Morph(from, to)` glides the letters the two texts share to their new places
  and fades the rest out and in. At u = 0 and 1 it draws exactly the two texts.

`text` shows reveals in each unit and order, a typed terminal command, a decoded headline,
a wave and a walking highlight, and anagram morphs.

### Motion blur

```cpp
node->isolate().motionBlur = { 16, 0.5f };           // 16 samples, a 180-degree shutter
node->fn = [](const Ctx& cx, int t) {
    const double x = interpolate(frameTime(t), { 0, 20 }, { 100, 1700 });   // read time as frameTime(t)
    dr::circle(cx, { (float)x, 540 }, 60, GOLD);
};
// or the whole picture: comp.motionBlur = { 8, 0.5f };
```

Every frame is a pure function of time, so a frame can be drawn at any moment in between.
Motion blur draws `samples` moments spread over a `shutter` (in frames, centred on the
frame) and averages them in a float buffer. The blur is the real path of the motion, not
a smear guessed from velocity, and anything that holds still stays exactly sharp.

Code takes part by reading time as `frameTime(t)`; code that uses `t` as it is draws the
same at every sample and stays sharp, which suits labels. Node `up` callbacks are
evaluated again at each sample, so animated props blur too. Camera tracks and particles
already use `frameTime`. Blur a layer for one node and its children, or a `Comp` for
everything (not with a custom `render`). Each sample is a full draw of that subtree, so
use 8 to 16 samples, and 32 for very fast moves.

`motionblur` compares sharp and blurred motion side by side, sweeps the shutter angle, and
whips a strip of cards past with a title flying in.

### Particles

```cpp
fx::Emitter sparks = fx::burst(150, { GOLD, ORANGE });   // a preset, then adjust it
sparks.burstAt = 30;
sparks.gravity = { 0, 0.08f };
node->fn = fx::fn(sparks);                              // or fx::draw(cx, e, t) inside your own fn
```

`fx::` emitters are computed, not simulated. Each particle's birth frame, lifetime and
starting values come from the seed and its index, and its position at any age follows
from closed-form motion (velocity, gravity, drag, smooth turbulence). Any frame renders
alone, out of order or twice with the same result, and costs only its live particles.

* **Emission:** a `burst` at a frame (optionally spread over a few frames), a `rate`
  stream between `from` and `until`, `prewarm` to start full, and an `origin(frame)`
  function for a moving source (trails, rockets, comets).
* **Area and direction:** point, line, rect, circle or ring; an `angle` and a `spread`,
  or `outward` from the centre.
* **Looks:** `Dot`, `Glow`, `Streak` (tapered along the path), `Square`, `Confetti`
  (spins and flutters), `Star`, `Ring`, or your own `draw(cx, particle)`. Size and colour
  can change over each particle's life, and `additive` adds light for glows and fire.
* **Presets:** `burst`, `sparks`, `fountain`, `confetti`, `rain`, `snow`, `fire`,
  `smoke`, `embers`. Each returns an ordinary `Emitter` you can adjust.
* `fx::particles(e, t)` / `fx::each(e, t, f)` hand you the live particles (position,
  velocity, age, size, colour) for anything else you want to draw with them.

`particles` shows fireworks, a campfire, rain, snow, a comet and a custom look.

**In 3D scenes**, `fx::Emitter3D` is the same emitter in world units, added to a `g3d`
scene or object (its particles live in that object's space and move with it):

```cpp
scene->add(fx::fire3d(0.25f))->position = { -3, 0, 0 };          // flames, smoke3d, embers3d...
fx::Emitter3D shell = fx::burst3d(260, { PINK, GOLD });
shell.burstAt = 160;
scene->add(shell)->position = { 0, 7, -5 };
scene->add(fx::debris3d(40), g3d::Geometry::box(1, 1, 1), crate);  // mesh particles
```

* They are depth-tested against the scene's meshes (hidden behind them) and fogged with
  them. Dots, glows, stars and rings face the camera, streaks lie along their motion, and
  squares and confetti are paper quads that tumble in 3D.
* Birth volumes: point, line, box, sphere, shell, and a disc or ring on the ground; a
  cone of directions (`direction`, `spread` up to 360 for all round) or `outward`.
* **Mesh particles:** with a geometry and material, each particle is a lit instance, with
  shadows and ink lines. `floor` makes particles land on a plane and rest there, no longer
  spinning (still closed-form: the landing is found by bisection).
* Presets: `burst3d`, `sparks3d`, `fountain3d`, `confetti3d`, `snow3d`, `rain3d`,
  `fire3d`, `smoke3d`, `embers3d`, `debris3d`.

`particles-3d` shows a campfire lighting a night yard, a fountain, a crate bursting into
chunks that land, fireworks and confetti, under an orbiting camera.

### Subtitles and voice-over

```cpp
sub::Track track = sub::load("voiceover.srt");            // SRT or WebVTT
addRoot(c.roots, 0, c.duration)->fn = sub::captions(track);   // burned in, white with an outline

// Scenes timed to what is said: span i runs from phrase i to phrase i + 1.
auto spans = track.spans({ "First,", "Then the optimizer", "Finally" }, c.fps, 30);
addRoot(c.roots, spans[1].from, spans[1].length)->fn = optimizerScene;
c.duration = spans.back().from + spans.back().length;
```

* `sub::parse` / `sub::load` read SRT and WebVTT (BOM, CRLF, VTT cue settings, markup
  stripped, `<i>` kept as italic); `toSrt` / `writeSrt` write them back.
* `track.when(phrase)` is the moment a phrase starts being spoken: its cue's start plus
  its share of the cue's characters, matched ignoring case and punctuation.
  `frameOf`, `spans` and `shifted` build on it.
* `sub::Style`: font and size, wrap width, bottom gap, alignment, outline, a backing
  box, fades, and karaoke (`upcoming` colours the words not yet spoken).
  `sub::draw(cx, cue, seconds, style)` draws one cue inside your own fn.

**Text to speech.** `tts::Script` speaks lines with any engine that runs from a shell
(the command gets `{textfile}` and `{out}`), caches each take by a hash of command and
text, and lays the lines out one after another:

```cpp
auto script = std::make_shared<tts::Script>(tts::Voice::system(), lines, 0.45);
c.assets = [script](const Comp&, const Json&) { script->generate(); };   // speaks new lines only
c.build = [script](Comp& comp, const Json&) {
    script->measure();                                   // real durations once the takes exist
    const sub::Track track = script->track();            // a cue per line
    script->attach(addRoot(comp.roots, 0, comp.duration), comp.fps);
    ...
};
```

`tts::Voice::system()` is the operating system's voice (Windows SAPI through PowerShell,
macOS `say`, else `espeak-ng`). Set `command` for anything else, such as piper or a
cloned voice. Before the takes exist, durations are estimated from the word count, so
a composition lays out either way.

`captions` is narrated by the system voice: scenes start on their phrases, captions are
burned in, one scene lights words up karaoke style, and the timings are saved to
`out/captions.srt`.

### Audio-reactive visuals

```cpp
auto music = aud::analyze("music.ogg");                 // decoded and analysed on first use
media::audio(addRoot(c.roots, 0, c.duration), "music.ogg");
node->up = [music](int t, NodeProps& p) { p.scale = 1 + 0.15f * music->pulse(frameTime(t)); };
node->fn = [music](const Ctx& cx, int t) {
    aud::bars(cx, { 160, 700, 1600, 260 }, music->spectrum(frameTime(t)), TEAL);
};
```

The whole file is analysed once (cached per file and options) into per-frame tables, so
any frame renders alone and the same each time. Times are node-local frames, and
fractional values interpolate (motion blur sees in-between values).

* `level` (loudness), `band(i)` / `spectrum` (log-spaced bands), `energy(lo, hi)` for a
  frequency range, and `onset` (how sharply the sound rises), all 0..1 relative to the
  track's own loud passages.
* `beats()` (onset peaks), `beat(t)`, `beatCount`, `pulse(t, decay)` (1 on each beat,
  falling off), `bpm()` (tempo from the beats' rhythm), and `waveform(t, points)`.
* `aud::Options`: band count and range, dB range, attack and release smoothing, and
  `startFrom` to line frame 0 up with a point in the file.
* Drawing helpers: `aud::bars` (optionally mirrored), `aud::wave`, `aud::radial`.

`audio-reactive` plays a generated 124 bpm track (intro, drums, riser, drop) under a
pulsing disc ringed by its spectrum, particle bursts on every beat, spectrum bars, an
oscilloscope, level meters and the detected tempo.

### Social icons and the logo

Built-in prefabs, registered by the CLI before your project's own (a prefab of yours with
the same name replaces one):

```cpp
prefab::use(root, 0, 90, "social-youtube", Json::parse(R"({"handle": "@raymotion"})"))->props.pos = { 960, 540 };
prefab::use(root, 30, 120, "social-row", Json::parse(R"({"brands": ["x", "github", "twitch"], "style": "outline"})"));
prefab::use(root, 0, 150, "raymotion-logo", Json::parse(R"({"tagline": "video as code", "layout": "stacked"})"));
```

* `social-x`, `social-youtube`, `social-github`, `social-instagram`, `social-tiktok`,
  `social-whatsapp`, `social-telegram` and `social-twitch`: the icon, with an optional `handle` beside it.
  `social-row` shows several, one after another, with a shared handle beneath.
* Props: `size`; `style` (`badge` in the brand colour, Instagram's gradient included;
  `glyph`, the mark alone in its colour; `mono` and `outline` in `color`); `shape`
  (`rounded`, `circle`, `square`); `in` (`pop`, `draw` to write the mark on, `fade`,
  `none`) and `delay`. `raymotion prefabs` lists every default.
* `raymotion-logo`: the mark (gradient ring, play triangle, orbiting play-head arc),
  the wordmark beside it or `stacked` beneath, an optional `tagline` and `badge`. It draws
  itself on by default.
* In your own fns: `icons::draw(cx, "github", centre, size, look, reveal)`,
  `icons::logo(cx, centre, size, t)`, and `icons::glyph(brand)` for the raw path.

The glyphs are [Simple Icons](https://simpleicons.org) paths (CC0). The logos are
trademarks of their owners, so follow each brand's guidelines when you use them.

`social-icons` shows the logo drawing on, a row of every brand, the four looks and two
shapes, icons with handles, and a stacked logo end card.

### Country flags

Built-in flag prefabs use lower-case ISO country codes. Each flag is a 3:2 drawing on a
rounded square badge; set `badge` to `false` for the flag alone.

```cpp
prefab::use(root, 0, 90, "flag-at", Json::parse(R"({"size": 160, "label": "Austria"})"));
prefab::use(root, 0, 90, "flag-row", Json::parse(R"({"countries": ["at", "de", "fr", "gb"], "stagger": 5})"));
// In a drawing function: flags::draw(cx, "jp", {960, 540}, 160);
```

Available codes: `at`, `be`, `br`, `ca`, `ch`, `cn`, `cz`, `de`, `dk`, `ee`, `fi`,
`fr`, `gb`, `hu`, `id`, `ie`, `in`, `it`, `jp`, `lu`, `mc`, `nl`, `no`, `pl`, `ro`,
`ru`, `se`, `ua`, `us`. `flags::countries()` returns the supported codes. Prefabs
accept `size`, `badge`, `label`, `ink`, `in` (`pop`, `fade`, `none`), `delay` and
`fps`. `flag-row` also accepts `gap` and `stagger`.

Render the 13-second showcase, including its synth music and timed sound effects, with
`build/raymotion render flag-showcase`. The video is written to `out/flag-showcase.mp4`.

`build/raymotion render austria-through-time` creates a National Day history reel with
a continuous camera journey, a 3D globe and alpine terrain, PBR and toon models,
a projected Austria map, a data chart, blurred time jumps, narration with captions,
procedural music and generated sound effects.
Its historical references are listed in `docs/austria-time-travel-sources.md`.

`build/raymotion render austria-time-travel` travels 5,000 years in 140 seconds: a year
dial that rolls back from 2026, Ötzi's copper axe in a glacier, Hallstatt by its lake,
the Roman Danube frontier on a projected map, the 996 Ostarrîchi deed typed on parchment,
Austria-Hungary extruded from a table map, 66,000 lights for the Austrian Jews murdered in
the Holocaust, a cloth flag for neutrality and fireworks for the National Day. The narration
is a voice clone of the author, and the score is synthesised from Strauss's *Blue Danube*
and Mozart's *Eine kleine Nachtmusik* and *Rondo alla Turca*. Script and sources:
`docs/austria-time-travel-script.md`.

### Maps

`geo::` reads map data and projects it. Natural Earth's 1:110m countries, land, lakes,
rivers and cities are bundled (public domain), and GeoJSON or TopoJSON files of your own
load the same way:

```cpp
const geo::Layer& world = geo::countries();
auto map = geo::Projection::equalEarth().fit({ 60, 60, 1800, 960 }, geo::Sphere);
node->fn = [map](const Ctx& cx, int) {
    vg::fill(cx, map.outline(), OCEAN);
    vg::stroke(cx, map.graticule(15), vg::Stroke(GRID, 1));
    vg::fill(cx, map.path(geo::land().merged()), LAND, vg::FillRule::EvenOdd);
    vg::stroke(cx, map.lines(geo::countries().merged()), vg::Stroke(BORDER, 1));
    vg::fill(cx, map.path(geo::countries().get("KEN").geometry), ACCENT, vg::FillRule::EvenOdd);
};
```

* Data: `geo::countries()` (177, with name, ISO codes, continent, region, population,
  GDP and a label point), `land()`, `lakes()`, `rivers()`, `cities()` (243, largest
  first). `layer.get("KEN")` finds a feature by id, ISO code or name; `filter` and
  `merged` pick and combine. `geo::load(file)` reads GeoJSON or TopoJSON (`object` picks
  one TopoJSON object), resolved like media files.
* Projections: equirectangular, Mercator, Equal Earth, Natural Earth, Robinson,
  Mollweide, orthographic (a globe), stereographic, azimuthal equidistant and equal-area,
  gnomonic, and the Albers, Lambert and equidistant conics (`parallels`). `rotate` turns
  the globe, `fit` frames a geometry (or `geo::Sphere`, the whole world) in a box.
* `path` gives fillable polygons, cut at the antimeridian or at a globe's horizon and
  closed along that edge, so fills stay right however the map is turned. `lines` gives
  strokes, `outline` the map's edge, `graticule` the meridians and parallels, and
  `project` or `map(lonLat)` single points.
* `Projection::mix(a, b, t)` moves every point between two projections, so a globe
  can open out into a flat map.
* On the sphere: `distance`, `angle`, `interpolate` (along the great circle), `bearing`,
  `centroid`, `bounds`, `area` and `contains`.

`maps` shows a turning globe, one world morphing through seven projections, Africa on
an Albers conic with Kenya picked out by its ISO code, and the bundled cities.

### 3D city models

`city::` reads [CityJSON](https://www.cityjson.org) (1.0 to 2.0, and CityJSONSeq
`.city.jsonl`) and draws it in a `g3d` scene:

```cpp
const city::Model town = city::load("data/city/town.city.json");
auto scene = std::make_shared<g3d::Scene>();
g3d::Object3D* blocks = city::add(*scene, town);    // one child per city object, named by its id
city::rise(blocks, 0, 2, 24);                      // buildings grow from the ground, middle first
city::frame(scene->camera, town, 210, 35);         // from the south-south-west, 35 degrees up
addRoot(c.roots, 0, 300)->fn = g3d::draw(scene);
```

* `city::Model` has the objects (`id`, `type`, `attributes`, `parents`/`children`, and a
  geometry per level of detail), `get(id)`, `filter`, `lods()` and `bounds()`. Faces carry
  their semantic surface (`RoofSurface`, `WallSurface`, `Window`, `TrafficArea`, ...).
  Geometry templates are expanded; solids, multi-surfaces, lines and points are all read.
* Coordinates are metres from `model.origin` (the middle of the extent at its lowest
  point), so big projected coordinates stay precise; `world()` and `local()` convert.
  There is no reprojection: a model is drawn in its own metres.
* `city::Look` colours faces by semantic surface, then by object type (roads, water,
  plants, terrain), with `look.color(object, face)` for your own rules (by an attribute,
  say), and `look.lod` to pick a level of detail.
* `city::add` gives one child per object, placed at its base, so `scale.y` grows it from
  the ground (`city::rise` does that in order of distance). `city::mesh(model)` is one
  mesh for the whole model, the cheapest to draw. `city::triangles(face)` triangulates a
  face with holes (mapbox earcut).
* Not only buildings: roads, water, plants and city furniture (street lamps, benches)
  are objects too, so `scene->find("avenue-1")` can light up a road like any building.
* Paths: a line geometry (a route, a tram line, a GPS track) is points in metres.
  `city::along(path, u, &heading)` is the point at fraction `u` of its length and the
  direction of travel there, to move a vehicle; `city::trail(scene, path, reveal)` is a
  ribbon that draws itself on as `reveal(t)` goes from 0 to 1.
* Labels that follow 3D points: `g3d::project(g3d::cameraAt(scene->camera, t), point,
  viewport, out)` gives the screen position, for 2D tags and callouts that always face
  the viewer.

The bundled `data/city/town.city.json` is a made-up town built by
`tools/make_city_sample.py`: houses, blocks, a two-part tower, roads, a canal, a park,
trees and street lamps (from geometry templates), and a bus route with its stops (a line
and points). Real cities: [3D BAG](https://3dbag.nl) (every building in
the Netherlands) and the [CityJSON datasets page](https://www.cityjson.org/datasets/).

`city-models` raises the town under an orbiting camera, colours it by construction year
with a legend, puts LoD 1.2 beside LoD 2.2, picks out the tower by its id with a callout
pinned to its top, lights two streets and their lamps one by one at dusk, and draws the
bus route on as a bus drives it, stopping at each stop.

### 3D globes

`globe::` puts the Earth in a `g3d` scene, with maps, heat maps, weather and routes laid
on it:

```cpp
auto scene = std::make_shared<g3d::Scene>();
globe::Map map(4096, mkColor(0x1d3557));                      // an equirectangular image, drawn on the CPU
map.fill(geo::land().merged(), mkColor(0x7fb069)).stroke(geo::countries().merged(), WHITE, 1.5f);
g3d::Material earth;
earth.dynamicMap = map.texture();
scene->add(globe::sphere(), earth);
globe::route(*scene, geo::cities().get("London").labelPoint(), geo::cities().get("Tokyo").labelPoint(),
             [](int t) { return t / 90.f; });                 // a flight path drawing itself on
scene->camera.up = [](int t, g3d::Camera& cam) { globe::look(cam, { 20 + t * 0.2, 30 }, 5); };
```

* Places: `globe::point(lonLat, radius)` and back with `lonLat(p)`; `north`/`east`
  tangents; `place(object, lonLat, altitude, heading)` stands a model on the surface;
  `look(camera, lonLat, distance)` puts the camera above a place.
* Textures: `globe::sphere()` maps an equirectangular image onto the globe and
  `globe::patch(box)` a lon/lat box (a sharper close-up, or a regional overlay).
  `globe::Map` fills, strokes (as wide on the ground everywhere), graticules, discs in km
  and gradients, handling the antimeridian and the poles.
* Textures that change: `g3d::Material::dynamicMap` takes a `g3d::DynamicTexture`, an
  image made per frame (or every N frames) and updated on the GPU in place. A see-through
  shell a little above the ground (opacity just under 1) lays it over the globe.
* Heat maps: `globe::Field` sums soft spots (`add(lonLat, radiusKm, weight)`) and
  `image(scale)` colours them with `globe::heat` (or your palette).
* Weather: `globe::clouds(width, seconds, weather)` draws procedural cloud cover, carried
  by trade winds and westerlies, seamless at the antimeridian and the poles, with
  `Storm`s wound into spirals round a clear eye.
* Routes: `globe::arc(a, b, u)` lifts a great circle off the ground; `route(...)` draws it
  on (and off, with a tail); `fly(object, a, b, u)` moves a plane along it, nose forward;
  `path(...)` does the same along any curve.
* `g3d::lookRotation(forward, up)` gives `Object3D::rotation` for any heading.

`earth` turns a globe painted from Natural Earth under drifting clouds; flies every leg
of President Prabowo Subianto's trips abroad since October 2024 out of Jakarta and back
(from Wikipedia's list), a desk calendar tearing off the days; spreads a simulated outbreak
from Dubai as a heat map; shows the 2026 fires in Sumatra and Kalimantan close up, their
haze drifting to Singapore and Sarawak, with the hotspot counts and air-quality readings
as reported and a month calendar keeping the date; and winds up a simulated typhoon over
the Philippine Sea. Hotspot positions and the smoke are illustrative.

### Data on 3D maps

`geo3d::` stands data on a globe, on a flat projected map, or on anything between while
the one unrolls into the other:

```cpp
auto surface = std::make_shared<geo3d::Surface>(geo::Projection::equalEarth(), geo::LonLat{ 10, 0 });
g3d::Object3D* world = scene->add();
geo3d::base(*world, surface, paper);                           // the map image (a Material) on the surface
geo3d::MarkStyle cols;
cols.grow = [](int t) { return t / 45.f; };
geo3d::marks(*world, surface, geo3d::byCountry({ { "CHN", 1398 }, { "IND", 1366 }, { "USA", 328 } }), cols);
const auto at = [](const char* iso) { return geo::countries().get(iso).labelPoint(); };
geo3d::flows(*world, surface, { { at("MEX"), at("USA"), 11 }, { at("SYR"), at("TUR"), 3.9 } });
scene->up = [surface](int t, g3d::Scene&) { surface->morph = std::clamp((t - 120) / 60.f, 0.f, 1.f); };
```

* `Surface`: a globe (morph 0) and a map from any `geo::Projection` (morph 1), centred on
  one place, the map touching the globe there; every point and its "up" move in a straight
  line between the two. `setMap` changes the projection at any time (also a
  `Projection::mix`).
* Marks: columns, spikes, bubbles and discs (`Mark`), sized by value, coloured by a
  `Scale`, growing in one after another, values animated per frame if you like.
* Areas: `extrude` raises countries as prisms (height 0 gives a flat choropleth in 3D),
  bending with the globe; `choropleth` paints them on a `globe::Map` instead.
* Flows: arcs along great circles (or across the map, for ones that would cross its
  edge), as wide as their value, drawing themselves on.
* Data by ISO code, id or name (`byCountry`); labels through `geo3d::screen`, which hides
  places round the back of the globe; keys with `plot::colorbar` and `plot::legend`.
* `g3d::Geometry::update(edit)` reshapes a mesh from an `up` callback; geo3d's shapes use
  it to follow the morph.

`map-charts` stands population columns on a turning globe, unrolls it into an Equal Earth
map with the world's largest migration corridors arcing across it (IOM World Migration
Report 2024), then lays the map on a table as a tilt-shift miniature of GDP per person.

### Dates and calendars

`cal::` counts days (since 1970-01-01, proleptic Gregorian), formats them, maps frames to
dates and draws calendars that animate as the days go by:

```cpp
cal::Clock clock;
clock.key(0, cal::Date{ 2024, 10, 20 }).key(300, cal::parse("2026-09-13"));  // linear between keys, held outside
node->fn = [clock](const Ctx& c, int t) {
    cal::page(c, { 1600, 760, 190, 230 }, clock.day(t));   // a tear-off desk calendar
    cal::month(c, { 1500, 700, 360, 300 }, clock.day(t));  // a month card, today ringed
    dr::text(fonts::ui(true), c, cal::format(clock.date(t), "%A %e %B %Y").c_str(), { 80, 80 }, 40, 0, WHITE);
};
```

* `days(Date)`, `date(days)`, `weekday` (0 = Monday), `daysInMonth`, `parse("YYYY-MM-DD")`,
  `monthName`, `weekdayName`, and `format` with `%Y %m %d %e %b %B %a %A %j %%`.
* A day is a number: the fraction animates. Past `Style::turn` (0.55) the page tears off
  and flies away, or the ring glides to the next day, or the next month rises in, so the
  new day is up at the whole number. Key a clock to land at half days to keep it still.
* `cal::Style` sets paper, ink, accent, header, muted and shadow colours, `weekday` (the
  weekday under the page's number) and `firstWeekday` (0 = Monday, 6 = Sunday).

### Engine pieces

* `rmtn/timeline.hpp` — `Comp`, the node tree (start/duration, `up` animates props,
  `fn` draws), easings, float/vec/color keyframe tracks, transform ctx, per-frame RNG.
* `rmtn/draw.hpp` — primitives through the node ctx: quads, gradients, round rects,
  rings/arcs/sectors, stars, blobs, any simple polygon (ear-clipped), text.
* `rmtn/render.hpp` — `Canvas`: supersampled antialiasing. `Comp::supersample` (default
  2, or `--ss 1|2|4`) draws at N x size and box-filters down, so edges, lines and glyphs
  are smooth while you keep working in composition coordinates. `Comp::render` replaces
  the tree with a custom multi-pass renderer; `onGraphicsShutdown` releases its GL objects.
* `rmtn/fonts.hpp` — `fonts::ui()` is the bundled Inter (SIL OFL); `fonts::load({...})`
  finds files in project assets, `RAYMOTION_ASSETS`, then system font dirs, caches them
  with mipmapped filtering, and throws instead of silently substituting.
* `rmtn/anim.hpp` — `interpolate`, `interpolateColors`, `spring`/`measureSpring`, `ease::`.
* `rmtn/transitions.hpp` — `TransitionSeries` with fade, slide and wipe.
* `rmtn/layout.hpp` — `layout::` rects, rows, columns, grids; `wrapText`, `dr::textBox`.
* `rmtn/scene3d.hpp` — `g3d::` GPU scenes: geometry, models, materials (physically based
  or toon/Gooch/hatch; textures from files or remade per frame), lights, shadows,
  environments, ink lines, cameras with depth of field, particle emitters.
* `rmtn/diagram.hpp` — `dg::` graphs from Mermaid, DOT, BPMN, RDF, JSON and Newick;
  layered, force and tree layouts; `Diagram` animations (build, trace, flow, morph, focus).
* `rmtn/chem.hpp` — `chem::` molecules (SMILES, MOL/SDF, PDB), skeletal formulas, 3D
  ball-and-stick and protein cartoons.
* `rmtn/particles.hpp` — `fx::` closed-form particle emitters, looks and presets.
* `rmtn/paths.hpp` — `vg::` Bézier paths and SVG path data, fills by winding rule,
  strokes with joins, caps and dashes, trims, morphs and SVG drawings.
* `rmtn/math.hpp` — `math::` TeX formulas typeset with an OpenType math font, drawn as
  paths, written on, coloured by part and transformed into one another.
* `rmtn/plot.hpp` — `plot::` axes and number lines, function, parametric and polar
  graphs, areas, Riemann sums, vector fields; bar, line, area, pie and waterfall charts
  from data or CSV; box plots, violins, histograms, densities, error bars and heatmaps;
  bubbles, radar, gauges, funnels and candles; sunbursts, treemaps, Sankey and chord
  diagrams, stream graphs, parallel coordinates and contour maps.
* `rmtn/space.hpp` — `space::` 3D views that blend from flat to tilted, depth-sorted
  scenes, 3D axes, surfaces and meshes, the torus, Möbius band and Klein bottle, 4D
  rotation and projection, the hyperbolic disk and half-plane with `{p, q}` tilings, and
  spherical geometry.
* `rmtn/camera.hpp` — `cam::` shots, fits, smooth zooms, keyframed tracks, parallax,
  shake and camera-driven world nodes.
* `rmtn/code.hpp` — `code::` highlighted code blocks in 13 languages, typewriter and
  line-by-line reveals, line focus, marks, and morphs between versions.
* `rmtn/textfx.hpp` — `txt::` text animation: staggered reveals by letter, word or line,
  typing, decoding, per-glyph stylers (waves, highlights), and morphs between texts.
* Motion blur (`MotionBlur`, `frameTime` in `rmtn/timeline.hpp`) — samples within the
  shutter averaged in a float buffer, per layer or for the whole composition.
* `rmtn/json.hpp` — `Json` values for props (parse, typed lookups with fallbacks, merge).
* `rmtn/prefab.hpp` — `prefab::add/use`: registered, parameterized subtree builders.
* `rmtn/synth.hpp` — sine-sum note events + envelopes -> WAV soundtrack.
* `rmtn/cli.hpp` — `runCli`: list / preview / render / still / props / prefabs.

## Showcase

`build/raymotion` runs `examples/showcase`:

```
probe                  1280x720  @30fps    2.0s  music=0
demo-neon              1920x1080 @30fps    7.5s  music=1
geo-orbit              1920x1032 @30fps    9.0s  music=1   (procedural CPU 3D)
fx-1-overlays          1920x1032 @30fps   10.0s  music=1   (overlay shader)
fx-2-bloom             1920x1032 @30fps   10.0s  music=1   (+ CPU quarter-res bloom)
fx-3-gpulit            1920x1032 @30fps   10.0s  music=1   (+ CPU painter-lit meshes; historical name)
fx-4-max               1920x1032 @30fps   10.0s  music=1   (full composite)
gpu-3d                 1920x1080 @30fps    8.0s  music=0   (GPU scene: IBL, shadows, DOF, fog, inset camera)
here-there-explainer   1280x720  @30fps  108.0s  music=1   (logic explainer)
here-there-plain       1280x720  @30fps  108.0s  music=0   (plain-language cut, Segoe UI)
here-there-feed        1080x1350 @30fps   88.0s  music=0   (portrait feed cut, Segoe UI)
own-your-compute       1920x1080 @30fps   66.0s  audio=1   (story film: GPU 3D, particles, glitch; all assets from code)
raymotion-promo        1920x1080 @60fps  210.0s  audio=1   (YouTube introduction: how it works, 16 features, credits)
npr-styles             1920x1080 @30fps   53.8s  audio=0   (non-photorealistic rendering: eight illustration styles)
diagrams               1920x1080 @30fps   56.0s  audio=1   (flowcharts, DOT, BPMN, knowledge graphs, trees, molecules, proteins)
particles              1920x1080 @30fps   29.3s  audio=0   (fireworks, campfire, rain, snow, comet, custom looks, confetti)
particles-3d           1920x1080 @30fps   14.0s  audio=0   (particles in a 3D scene: campfire, fountain, landing debris, fireworks, confetti)
vector-paths           1920x1080 @30fps   34.3s  audio=0   (draw-on, joins and dashes, fill rules, morphs, SVG, motion paths)
equations              1920x1080 @30fps   32.3s  audio=1   (TeX formulas: write-on, layout gallery, named parts, step-by-step transforms)
plots                  1920x1080 @30fps   40.7s  audio=1   (graphs, tangents, Riemann sums, curves, vector fields, number lines, charts)
charts                 1920x1080 @30fps   58.0s  audio=1   (box plots, violins, histograms, heatmaps, bubbles, radar, gauges, waterfalls, funnels, sunbursts, treemaps, Sankey, chord, streams, contours, parallel coordinates, candles)
spaces                 1920x1080 @30fps   53.7s  audio=1   (1D to 2D to 3D, projections, surfaces, a torus glued, Möbius and Klein, spherical and hyperbolic geometry, a tesseract)
camera                 1920x1080 @30fps   28.0s  audio=1   (a 2D camera: glide, fly, turn, shake and zoom over a big canvas, parallax, a HUD mini-map)
code                   1920x1080 @30fps   35.3s  audio=1   (code blocks: typing, highlighting in many languages, line focus and marks, morphs between versions)
text                   1920x1080 @30fps   32.0s  audio=1   (text animation: staggered reveals, typing, decoding, waves and highlights, morphs)
motionblur             1920x1080 @30fps   23.0s  audio=1   (motion blur: sharp against blurred, shutter angles, whip pans and a flying title)
sparql                 1920x1080 @30fps  113.0s  audio=1   (a narrated explainer of SPARQL query processing: knowledge graph, query, algebra, optimizer, matching, joins)
lowerthird             1920x1080 @30fps    6.0s  audio=0   (a name tag with no background: render with --alpha for ProRes 4444 or --format webm)
audio-reactive         1920x1080 @30fps   24.0s  audio=1   (visuals driven by a music track: spectrum, beats, tempo, waveform, levels)
captions               1920x1080 @30fps   26.1s  audio=1   (narrated by the system voice: scenes timed to phrases, burned-in and karaoke captions, an SRT sidecar)
social-icons           1920x1080 @30fps   19.5s  audio=0   (the built-in icon prefabs: the raymotion logo, social badges in four looks, handles)
flag-showcase          1920x1080 @30fps   13.0s  audio=1   (the built-in flag prefabs: 29 country flags, badge or plain, labels, a staggered flag-row)
maps                   1920x1080 @30fps   24.4s  audio=1   (geo:: maps: a turning globe, seven projections morphing, a conic with a country by ISO code, cities)
city-models            1920x1080 @30fps   41.5s  audio=1   (CityJSON in 3D: a town rising, coloured by year built, LoD 1.2 beside 2.2, a building by id, streets and lamps at dusk, a bus route)
earth                  1920x1080 @30fps   65.2s  audio=1   (globe:: and cal:: in 3D: a painted Earth under clouds, Prabowo's trips abroad, an outbreak heat map, the 2026 Indonesian fires and haze, a typhoon)
atmosphere             1920x1080 @30fps   20.0s  audio=1   (g3d::Sky and fog: a valley from before dawn to after dusk, ground fog burning off, stars, lit windows)
atmosphere-miniature   1920x1080 @30fps   10.0s  audio=1   (tilt-shift: the valley's hamlet as a miniature, through camera.dof.tilt and as Filter::tiltShift)
map-charts             1920x1080 @30fps   31.2s  audio=1   (geo3d:: population columns on a globe, the globe unrolling into a map of migration flows, GDP per person as a tilt-shift miniature)
austria-time-travel    1920x1080 @30fps  140.0s  audio=1   (Austria through 5,000 years for the National Day: glacier, salt, Rome, Ostarrîchi, empire, neutrality; voice clone and a Strauss/Mozart score)
```

`raymotion-promo` is the 3.5-minute introduction to the engine
([watch it on YouTube](https://youtu.be/tcALbxoO9x0)), at 60 fps: the
idea (a video is a function of its frame number), the pipeline from C++ to MP4, the
timeline tree, then sixteen features demonstrated one by one beside the code that drives
them, the GitHub repository and the credits. Its score, and the texture, photo, SVG, GIF,
video clip and OBJ model its media and 3D chapters show, are generated into
`out/promo-assets-v1/` on first still, preview or render (the GIF and the clip need ffmpeg).

The Here-and-There pieces introduce Pedro Cabalar's treatment of HT and equilibrium
logic; sources, script and scope notes are in `docs/here-there-explainer.md`, and the
worked examples are checked by the `ht_semantics` test.

`own-your-compute` is a 66 s short about running AI on your own machine, built to show
the engine off: a fogged server aisle with blinking LEDs, glitch stamps with RGB split and
screen shake, a particle cloud pouring into a chip, a procedural graphics card with
spinning fans and depth of field, a neural net firing, an offline terminal, and a dawn
island under a privacy dome. Every mesh (terrain, roof, dome), texture (PCB, wood, rack
panels), the sky and the score (saw pads, drums, delay and reverb, synthesised in
`examples/showcase/own_compute.cpp`) is generated by code into `out/oyc-assets-v2/` on
first still, preview or render. The plain and feed cuts are laid
out for Segoe UI and refuse to substitute another font (their tests skip without it).

## Tests

`ctest` runs: HT semantics and the authoring API (interpolation, springs, timing,
transitions, layout, JSON; pure logic); layer compositing (group opacity, blend modes,
masks, offsets at every supersampling level, wipes, text boxes, illustration filters); GPU 3D (placement and
depth, lighting, directional/spot/point and multi-light shadows, environment lighting and
skybox, depth of field, viewports, transparency, hierarchy, OBJ and a generated skinned
glTF, determinism, toon/Gooch/hatch shading, ink lines in layers, viewports and at 2x,
boiling wobble, particles: placement, occlusion, mesh particles, additive light, fog);
renderer regressions (supersampling edge levels
and coordinates, concave fills, fonts, FX shader uniforms, determinism, shutdown);
diagram parsing and layout (Mermaid, DOT, BPMN, Turtle, JSON-LD, JSON, Newick; layered,
force and tree layouts; SMILES, SDF and PDB, rings, formulas, substructures, depictions;
no window) and diagram rendering (build-up, highlights, traces, morphs, BPMN, skeletal
formulas, 3D molecules and cartoons); particles (burst and stream counts, prewarm,
determinism, closed-form motion against a stepped simulation, areas, presets; in 3D the
shared schedule, cones, volumes, floor landings and presets; no window);
vector paths (SVG path data, arcs, lengths, trims, transforms, morphs, SVG files; no window)
and their rendering (fill rules and holes, translucent overlaps, caps, joins, dashes,
draw-on, fitted SVG drawings, determinism); equations (parsing and its errors, TeX
spacing, scripts and primes, fractions, limits, delimiters, radicals, matrices and
alignment, parts, transform matching; no window) and their rendering (placement, part
colours and opacity, write-on order, transform end frames, determinism); plots (tick
ranges, mapping, graphs with poles and holes, clipping, curves, areas, Riemann sums,
tangents, CSV tables, chart ranges, summaries, densities, bins, trees, flows, colour
maps, contour rings and bands; no window) and their rendering (axes growing from the origin, bar slots and heights,
pie order, line draw-on, fields, heatmap colours, treemap areas, waterfall steps, Sankey
ribbons, gauge fill, chord arcs, contour bands, parallel lines, streams, determinism);
spaces and the camera (flat and tilted views, depth, projections, meshes, gluing, the
Möbius closure, the tesseract, `{p, q}` tilings, isometries, geodesics, spherical angles,
stereographic projection, shots, fits, blends, smooth zooms, tracks, shake; no window) and
their rendering (depth order, hidden labels, a camera world with a fixed HUD);
code (highlighting in each language, tabs and columns, finding text, reveal counts, morph
matching; no window) and its rendering (token colours, cards, focus, typewriter, morph
end frames);
text animation (layout, wrapping and alignment, reveal timing in every order, end frames
identical to plain text, typewriter cut-offs, highlights, morph end frames); motion blur
(still frames unchanged, smear width and ramp, layer blur on its own node only, props
from `up`, particles);
subtitles (SRT and WebVTT parsing, markup and errors, phrase timing and scene spans, SRT
round trips, text-to-speech scripts through a stand-in engine with caching and failures)
and burned-in captions; audio analysis (a tone's band and loudness, interpolation, the
waveform, beats and tempo of click tracks, pulses, `startFrom`, caching by file; no window);
icons (glyph data, the prefabs and their props errors; badge colours, Instagram's
gradient, glyph and mono looks, the logo, entrances, determinism);
maps (the bundled layers and lookups, GeoJSON and TopoJSON parsing and errors, distances,
areas and containment, projection formulas, fitting, clipping at the antimeridian and the
horizon measured by area, and mixes; no window);
city models (the sample town's objects, levels of detail, semantics, parts and templates,
CityJSON 1.0, CityJSONSeq and errors, triangulation areas, holes and winding, street lamps,
the route and stops, `along`, trails drawing on and camera projection; a rendered
town in semantic colours, rising, and framed; set `RMTN_CITYJSON_DIR` to also check every
file in a folder of real data);
globes (places, tangents and lookRotation, map fills against the real share of land and
across the antimeridian and poles, strokes, discs, fields, the heat palette, cloud cover,
seams, motion and storms, arcs, routes and flying; a rendered globe from two sides, a
texture remade per frame, and an overlay shell);
calendars (day counts both ways, leap years, weekdays, parsing and formatting, the frame
clock; a page and a month card changing as the day turns);
Here-There layout checks; and a Python CLI suite (exact sizes, draft scaling, stale-frame
cleanup, a real MP4 render, transparent stills and ProRes/WebM renders with alpha, the
starter project, props). Tests needing OpenGL carry the `gl`
label (`ctest -LE gl` skips them on headless machines).

## License

raymotion is licensed under the [Apache License 2.0](LICENSE). Videos you render with it
are yours: the license covers the engine, not its output.

Third-party parts keep their own licenses (see [NOTICE](NOTICE)): raylib and nanosvg are
zlib-licensed, stb_image is public domain or MIT, and Inter is © The Inter Project
Authors under the SIL Open Font License 1.1 (`assets/fonts/Inter-OFL.txt`). The sample
structures in `assets/data/` are public domain (RCSB PDB, PubChem).
