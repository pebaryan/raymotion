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
  units on top of an automatic slope bias. Linear fog.
* Ambient light is a sky/ground hemisphere, or image-based: `scene->environment` (an
  equirectangular PNG/JPG/HDR) or `environmentGenerator` (e.g. `g3d::studioEnvironment`,
  a gradient with soft light boxes) lights diffuse surfaces from its blurred mips and
  gives reflections that blur with roughness; `showEnvironment` draws it as a skybox.
* Depth of field per camera (`camera.dof`): a golden-angle gather blur from the depth
  buffer, sized in composition pixels so it looks the same at every supersampling level.
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
  darkened rims and pigment granulation, after Bousseau et al.). Sizes are composition
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
  or toon/Gooch/hatch), lights, shadows, environments, ink lines, cameras with depth of field.
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
boiling wobble); renderer regressions (supersampling edge levels
and coordinates, concave fills, fonts, FX shader uniforms, determinism, shutdown);
diagram parsing and layout (Mermaid, DOT, BPMN, Turtle, JSON-LD, JSON, Newick; layered,
force and tree layouts; SMILES, SDF and PDB, rings, formulas, substructures, depictions;
no window) and diagram rendering (build-up, highlights, traces, morphs, BPMN, skeletal
formulas, 3D molecules and cartoons); particles (burst and stream counts, prewarm,
determinism, closed-form motion against a stepped simulation, areas, presets; no window);
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
