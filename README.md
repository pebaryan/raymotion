# raymotion

[![CI](https://github.com/pebaryan/raymotion/actions/workflows/ci.yml/badge.svg)](https://github.com/pebaryan/raymotion/actions/workflows/ci.yml)

[![raymotion: the introduction video on YouTube](docs/images/raymotion-promo.jpg)](https://youtu.be/tcALbxoO9x0)

Remotion-style motion graphics / video engine — pure C++17 on **raylib + ffmpeg**, no
web stack. Compositions are code: a timeline tree of nodes with keyframe/easing tracks
and transforms, drawn deterministically per frame, supersampled for clean edges, and
piped straight into ffmpeg.

▶ **[Watch the 3.5-minute introduction on YouTube](https://youtu.be/tcALbxoO9x0)**: how it works,
the technology underneath, and every feature demonstrated, all rendered by raymotion itself.

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

## Usage

```
build/raymotion list
build/raymotion preview <name>                 # live window; esc or close quits
build/raymotion render  <name> [--crf N] [--draft] [--no-music] [--png-frames] [--ss N]
build/raymotion still   <name> --at FRAME [--png PATH] [--ss N]
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
formulas, 3D molecules and cartoons); Here-There layout checks; and a Python CLI suite (exact sizes, draft scaling, stale-frame
cleanup, a real MP4 render, the starter project, props). Tests needing OpenGL carry the `gl`
label (`ctest -LE gl` skips them on headless machines).

Inter is © The Inter Project Authors, SIL Open Font License 1.1
(`assets/fonts/Inter-OFL.txt`). raylib is zlib-licensed.
