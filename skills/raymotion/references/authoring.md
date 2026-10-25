# Authoring 2D compositions

Everything is in namespace `rmtn` (`#include "rmtn/raymotion.hpp"`). Frames, durations
and offsets are integers in frames; positions are composition pixels.

## Timeline

```cpp
TNode* n = addRoot(c.roots, 30, 90);        // frames 30..119 of the composition
TNode* kid = n->add(10, 40);                // frames 10..49 of n (40..89 overall)
kid->props.pos = { 200, 100 };              // relative to the parent's origin
kid->up = [](int t, NodeProps& p) { p.rot = t * 2.f; };   // pos, scale, rot (degrees), opacity
kid->fn = [](const Ctx& cx, int t) { dr::circle(cx, { 0, 0 }, 40, RED); };

Series s(c.roots);                          // back to back
s.add(60)->fn = intro;
s.add(90, -10)->fn = body;                  // starts 10 frames before the previous one ends
loop(c.roots, 0, 30, 4)->fn = pulse;        // local t = 0..29, four times
freeze(c.roots, 200, 40, 15)->fn = body;    // shows body's frame 15 for 40 frames

TransitionSeries ts(c.roots, (float)c.w, (float)c.h);    // scenes with overlapping transitions
ts.add(90);
ts.transition(Transition::fade(20));        // also slide(frames, Direction::FromRight|FromLeft|FromTop|FromBottom), wipe(...)
ts.add(120);
c.duration = ts.end();
```

## Animation

```cpp
// interpolate(x, input range, output range, options): extend by default, clamped() to clamp.
float o = (float)interpolate(t, { 0, 20 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
float y = (float)interpolate(t, { 0, 30, 60 }, { 400, 300, 400 });   // several stops
Color col = interpolateColors(t, { 0, 30 }, { RED, BLUE });
// Easings: ease::linear, quad, cubic, sine, circle, expo, bounce, poly(n), back(s),
// elastic(b), bezier(x1, y1, x2, y2); wrap with ease::in / out / inOut.
double sc = spring(t, { 30.0 });            // fps; settles from 0 to 1 with overshoot
SpringOptions so; so.fps = 30; so.config = { 1, 180, 12 }; so.from = 0.5; so.to = 1; so.delay = 10;
double sc2 = spring(t, so);
// Keyframe tracks (ease per key: E_LINEAR, E_OUT_CUBIC, E_IN_OUT_CUBIC, E_BACK, E_ELASTIC, E_BOUNCE, ...).
TrackFloat tr; tr.add(0, 0.f); tr.add(20, 1.f, E_OUT_CUBIC); tr.add(80, 1.f); tr.add(100, 0.f);
float v = tr.at(t);                         // also TrackVec (Vector2) and TrackColor
// Deterministic randomness: same inputs, same value, on every frame and every machine.
double r = rnd01(7, i);                     // seed, index -> [0, 1)
double w = rrange(7, i, -50, 50);
```

Colours: `mkColor(0xRRGGBB)`, `withAlpha(c, 0.5f)` (multiplies alpha), `mixC(a, b, u)`,
raylib's named colours (`WHITE`, `RED`, ...) and `Fade(c, alpha)`. raylib's colours are
macros, so never name your own constants `GOLD`, `LIME`, `ORANGE`, `PINK`, `SKYBLUE` and so on.

## Drawing (`dr::`, through the node's `Ctx`)

```cpp
dr::rect(cx, x, y, w, h, col);
dr::roundRect(cx, x, y, w, h, radius, col);
dr::rectGrad4(cx, x, y, w, h, topLeft, topRight, bottomRight, bottomLeft);
dr::circle(cx, { x, y }, r, col);
dr::ring(cx, { x, y }, r, width, col);
dr::arc(cx, { x, y }, r, width, fromDeg, toDeg, col);      // stroke; sector(...) fills
dr::strokeSeg(cx, a, b, width, col);
dr::polyStroke(cx, pts, n, closed, width, col);
dr::fillPoly(cx, pts, n, col);                             // any simple polygon
dr::star(cx, { x, y }, rOuter, rInner, points, rotDeg, col);
dr::blob(cx, { x, y }, base, amp, freq, phaseDeg, col);
dr::text(font, cx, "Text", { x, y }, size, spacing, col, anchorX, anchorY);   // anchor 0.5 = centred
Vector2 m = dr::measure(font, "Text", size, spacing);
```

Shapes and text inherit the node's opacity automatically. Drawing outside the canvas
is fine (it is clipped).

## Text and layout

```cpp
Font f = fonts::ui(true);                   // bundled Inter; ui(false) is regular. Call inside fn.
Font mono = fonts::load({ "consola.ttf", "DejaVuSansMono.ttf", "Menlo.ttc" });   // first found; throws if none

Rectangle page = layout::inset(layout::fill(1920, 1080), 80);
auto cols = layout::row(page, { layout::px(480), layout::flex() }, 32);      // fixed + flexible
auto rows = layout::column(cols[1], { layout::flex(2), layout::flex(1) }, 24);
auto cells = layout::grid(page, 3, 2, 24);                                    // 3 columns, 2 rows
Rectangle r = layout::place(page, { 400, 200 }, Align::End, Align::Center);
Vector2 mid = layout::center(page);

TextStyle st;                               // font, size, spacing, lineHeight, color, align, valign
st.font = fonts::ui(false); st.size = 40; st.color = WHITE; st.align = Align::Center;
dr::textBox(cx, "Wraps inside the box; \\n breaks the line", rows[0], st);
Vector2 size = measureTextBox("text", st, 800);
```

`−` (minus) and `…` (ellipsis) are in the bundled font. Check other symbols
and non-Latin scripts in a still; load a system font with `fonts::load` if they are missing.

## Layers

```cpp
Layer& l = node->isolate();                 // the subtree renders offscreen, then composites
l.opacity = 0.5f;                           // group opacity, no seams between children
l.blend = Blend::Add;                       // Normal, Add, Multiply, Screen
l.mask = [](const Ctx& cx, int t) { dr::circle(cx, { 960, 540 }, 12.f * t, WHITE); };   // reveal
l.maskMode = MaskMode::Luma; l.invertMask = true;
l.filters = { Filter::watercolor(6), Filter::paperGrain() };
// Filter::posterize(levels), halftone(cell, angle, colored), paperGrain(strength, scale),
// inkEdges(width, ink, threshold), watercolor(bleed, edgeDarkening, granulation)
```

Each layer costs an offscreen pass; use them for effects, not for every node.

## Vector paths (`vg::`, `rmtn/paths.hpp`)

Bézier paths for line art, icons, curves that draw themselves on, shape morphs and SVG
files. Coordinates, stroke widths and dash lengths are in node units (they scale with the node).

```cpp
vg::Path wave = vg::Path::parse("M 200 600 C 500 200 900 1000 1200 600");   // SVG path data
vg::Path shape = vg::Path::rect({ 300, 300, 400, 240 }, 24);   // also circle, ellipse, arc, regular, star,
                                                               // polyline, smooth(points), line
vg::Path built;                                                // or build it: moveTo, lineTo, quadTo,
built.moveTo({ 0, 0 }).cubicTo({ 50, -80 }, { 150, -80 }, { 200, 0 }).close();   // cubicTo, arcTo, close
vg::Path moved = built.rotated(30).scaled(2, 2).translated({ 960, 540 });

vg::Stroke s;                                 // color, width, join (Miter, Round, Bevel), cap (Butt, Round, Square)
s.color = mkColor(0x58c4dd);
s.width = 6;
s.dash = { 18, 12 };                          // optional; dashOffset = -t marches them
vg::Style st = vg::Style::filled(mkColor(0xffd166));   // or Style::outlined(color, width); st.stroke = s;
st.rule = vg::FillRule::EvenOdd;              // NonZero (default) or EvenOdd, for holes

vg::Morph morph(vg::Path::circle({ 960, 540 }, 200), vg::Path::star({ 960, 540 }, 240, 100, 5));   // build once
auto art = std::make_shared<vg::Drawing>();   // SVG file: loaded on first draw
node->fn = [wave, shape, st, s, morph, art](const Ctx& cx, int t) {
    const float u = (float)interpolate(t, { 0, 60 }, { 0, 1 }, clamped());
    vg::stroke(cx, wave.trim(0, u), s);       // draw-on; trim(a, b, true) cuts every contour at once
    dr::circle(cx, wave.pointAt(u), 10, WHITE);   // also wave.angleAt(u) in degrees, length(), bounds()
    vg::write(cx, shape, st, u);              // outline draws on, then the fill fades in
    vg::draw(cx, morph.at(u), st);            // or vg::fill / vg::stroke
    if (art->shapes.empty()) *art = vg::Drawing::load("data/landscape.svg");
    vg::write(cx, *art, { 1100, 100, 700, 400 }, u);   // fitted into the box; vg::draw(cx, *art, box) for still
};
```

Move something along a path with `pointAt(u)`, and turn it to face along the path with
`angleAt(u)`: `arrow.rotated(p.angleAt(u)).translated(p.pointAt(u))`. Build `Morph`s and
parse paths while registering, not in `fn`.

## Equations (`math::`, `rmtn/math.hpp`)

TeX math typeset in-engine with the bundled Latin Modern Math font (no LaTeX needed) and
drawn as vector paths. Typeset while registering: `tex` throws on bad input, and the
message gives the offset.

```cpp
math::Options o(96);                          // px per em; also o.color, o.display = false (inline style), o.font
auto f = math::Formula::tex(R"(\id{lhs}{(a+b)^2} = \id{rhs}{a^2 + 2ab + b^2})", o);
auto g = math::Formula::tex(R"(x = \frac{-b \pm \sqrt{b^2 - 4ac}}{2a})", o);
math::Transform step(f, g);                   // build once
node->fn = [f, step](const Ctx& cx, int t) {
    const Vector2 at{ 960, 540 }, anchor{ 0.5f, 0.5f };   // anchor: fractions of the formula box
    math::Look look;                          // per-part colours and opacity, plus alpha for everything
    look.colors["lhs"] = mkColor(0xffd166);
    math::write(cx, f, at, anchor, (float)interpolate(t, { 0, 60 }, { 0, 1 }, clamped()), look);
    const Rectangle r = math::place(f, at, anchor, "rhs");   // a part's ink box in node units
    vg::stroke(cx, vg::Path::rect({ r.x - 12, r.y - 12, r.width + 24, r.height + 24 }, 12), vg::Stroke(WHITE, 4));
    // later: step.draw(cx, at, anchor, u, look);   // matched glyphs glide, the rest fade
    // or math::draw(cx, f, at, anchor, look) for a still formula
};
```

Supported: `^ _ '`, `\frac \dfrac \tfrac \binom \sqrt[n]{}`, `\sum \prod \int \oint \lim`
(with `\limits/\nolimits`), `\left( \middle| \right)`, `\big..\Bigg`, accents (`\hat \bar \vec
\dot \tilde \widehat`), `\overline \underline`, `\text \mathrm \mathbf \mathbb \mathcal
\mathfrak \operatorname{}`, Greek, ~200 symbols, `\, \: \; \! \quad \qquad`, environments
`matrix pmatrix bmatrix vmatrix Bmatrix cases aligned gathered`, a top-level `\\` (with `&`
to align), `\color{name|#rrggbb}`, `\textcolor{c}{..}`, `\phantom`, and `\id{name}{..}`
for named parts. `Transform` matches parts with the same `\id` first, so give the terms that
should travel matching ids in both formulas. Each formula holds `pieces` (glyphs and rules
in formula px), `width`, `ascent` and `descent`, and `path()` gives its outline as a
`vg::Path`.

## Plots and charts (`plot::`, `rmtn/plot.hpp`)

Axes, graphs and data charts drawn as vector paths, with numbers set in the math font.
Every drawing call takes a reveal `u` (0..1). Build paths and tables while registering.

```cpp
plot::Axes ax({ 260, 120, 1400, 700 }, { -6.5, 6.5 }, { -2, 2 });   // box in node px, x range, y range
// Range(min, max, step); step 0 = automatic. ax.map(x, y) -> px, ax.unmap(px) -> data
auto f = [](double x) { return std::sin(x); };
const vg::Path g = plot::graph(ax, f);       // adaptive; breaks at poles/holes; clipped to the box
const vg::Path c = plot::parametric(ax, [](double t) { return Vector2{ (float)std::cos(3 * t), (float)std::sin(2 * t) }; }, 0, 6.2832);
const vg::Path fill = plot::area(ax, f, 0, 3.14159);          // also polar(ax, r), riemann(ax, f, a, b, n), tangent(ax, f, x)
const plot::Table data = plot::Table::parseCsv("year, Solar, Wind\n2022, 33, 41\n2023, 45, 44\n");   // or Table::csv("data/x.csv")
node->fn = [ax, g, c, fill, f, data](const Ctx& cx, int t) {
    const float u = (float)interpolate(t, { 0, 60 }, { 0, 1 }, clamped());
    plot::AxesStyle st;                       // framed, arrows, ticks, xNumbers/yNumbers, xGrid/yGrid,
    st.xLabel = "x";                          // xLabel/yLabel (TeX), colours and sizes
    plot::axes(cx, ax, st, u);                // grows out of the origin
    vg::fill(cx, fill, withAlpha(mkColor(0x58c4dd), 0.35f));
    vg::stroke(cx, g.trim(0, u), vg::Stroke(mkColor(0x58c4dd), 5));
    vg::stroke(cx, c.trim(0, u), vg::Stroke(mkColor(0xffd166), 4));
    plot::label(cx, R"(y = \sin x)", ax.map(-6, 1.6), { 0, 0.5f }, 44);   // any TeX, anchored
    plot::field(cx, ax, [](Vector2 p) { return Vector2{ -p.y, p.x }; }, {}, u);   // arrows on a grid
    plot::ChartStyle cs;                      // box, y range, palette, stacked, values, legend, text size
    cs.box = { 1100, 600, 700, 300 };
    plot::bars(cx, data, cs, u);              // also plot::lines(cx, data, cs, u)
    plot::PieStyle ps;                        // inner (donut), explode, labels, palette
    plot::pie(cx, data, { 400, 750 }, 150, ps, u);   // the first series
};
```

`plot::numberLine({ x, y }, length, { -5, 5, 1 })` gives a flat `Axes` for number lines.
To animate a chart to new data, draw `plot::Table::mix(before, after, w)` with `w` going
0 to 1. Charts use the first eight `plot::palette(i)` colours unless `Series::color` or
the style's `palette` sets them.

Statistical, business, hierarchy and flow charts follow the same pattern: build the
data once, then draw it with a reveal `u`:

```cpp
const std::vector<plot::Series> groups = { { "Control", samplesA }, { "Dose", samplesB } };   // raw samples
const plot::Tree budget = plot::Tree::parse("Sales/Field, 22\nSales/Partners, 9\nOps/IT, 7\n");   // or Tree::csv(file)
const auto flows = plot::parseFlows("from, to, TWh\nGas, Power, 25\nPower, Homes, 18\n");      // or flowsCsv(file)
const plot::Bins bins = plot::bin(samplesA);                // automatic bin width
node->fn = [=](const Ctx& cx, int t) {
    const float u = (float)interpolate(t, { 0, 90 }, { 0, 1 }, clamped());
    plot::ChartStyle cs;                                    // box, y range (auto from data), palette
    plot::boxplot(cx, groups, cs, u);                       // also plot::violin(cx, groups, cs, u)
    plot::sunburst(cx, budget, { 500, 420 }, 300, {}, u);   // also plot::treemap(cx, budget, box, {}, u)
    plot::sankey(cx, flows, { 200, 120, 1500, 640 }, {}, u);
    plot::histogram(cx, ax, bins, SKYBLUE, u);              // ax: an Axes you draw with plot::axes
};
```

| Call | Data | Notes |
| --- | --- | --- |
| `areas(cx, table, ChartStyle, u)` | `Table` | `style.stacked` stacks the series |
| `waterfall(cx, table, WaterfallStyle, u)` | first series = changes | `total`, `totalLabel`, `rise`/`fall`/`sum` colours |
| `boxplot` / `violin(cx, groups, ChartStyle, u)` | `vector<Series>` of samples | whiskers at 1.5 IQR; `summarize(v)` gives the numbers |
| `histogram(cx, axes, bin(v), color, u)` | `Bins` | `density(v)` returns a function to `graph` (scale by `n * bins.width`) |
| `errorBars(cx, axes, pts, err, color, width, cap, u)` | points, ± errors | pair with `scatter` |
| `heatmap(cx, table, HeatmapStyle, u)` | rows = categories, columns = series | `colors = plot::viridis` / `plot::diverging` / any `Color(float)` |
| `bubbles(cx, axes, { { x, y, size, "label" } }, BubbleStyle, u)` | `vector<Bubble>` | area ∝ size, `maxRadius` |
| `candles(cx, axes, { { open, high, low, close } }, CandleStyle, u)` | `vector<Candle>` | candle i at x = i + 0.5 |
| `radar(cx, table, center, radius, RadarStyle, u)` | categories = spokes | `r` range, legend under the web |
| `gauge(cx, value, center, radius, GaugeStyle, u)` | one number | `range`, `zones` (threshold, colour), `unit` (TeX), `title` |
| `funnel(cx, table, box, FunnelStyle, u)` | first series = stages | percent of the first stage |
| `sunburst` / `treemap` | `Tree` | parents sum their children; `depth` limits the levels |
| `sankey(cx, flows, box, SankeyStyle, u)` | `vector<Flow>` | columns from the flow graph; ribbons coloured by source |
| `chord(cx, flows, center, radius, ChordStyle, u)` | `vector<Flow>` | arcs as long as sent + received |
| `contours(cx, contour(axes, f, levels, cell), ContourStyle, u)` | `f(x, y)` | build the `ContourMap` once; `fill`, `labels`, `colors` |
| `parallel(cx, table, box, ParallelStyle, u)` | rows = categories, axes = series | `colorBy` a dimension, `highlight` one row |
| `stream(cx, table, ChartStyle, u)` | `Table` | centred, smoothed stacks; names inside the streams |

`plot::nice(lo, hi, pad)` rounds a data extent out to whole ticks for your own `Axes`.
Text labels are plain text: names go through `\text{}`, so `&`, `$`, `%` are fine.

## Spaces and geometry (`space::`, `rmtn/space.hpp`)

3D and non-Euclidean scenes drawn as vector paths inside 2D nodes. A `View` projects; a
`Scene` collects items and draws them back to front (labels included, so surfaces hide
them). Build meshes once while registering.

```cpp
const space::Mesh hill = space::Mesh::graph([](double x, double y) { return std::exp(-(x * x + y * y) / 4); }, { -3, 3 }, { -3, 3 }, 40, 40);
node->fn = [hill](const Ctx& cx, int t) {
    const float k = (float)interpolate(t, { 30, 90 }, { 0, 1 }, clamped());
    // flat({centre}, px per unit) is a 2D plot; tilted(centre, scale, yaw, pitch, perspective) has z up
    space::View v = space::View::mix(space::View::flat({ 960, 540 }, 110), space::View::tilted({ 960, 560 }, 100, -32, -64, 0.3f), k);
    space::Scene s(v);
    space::Axes3 a;                            // x, y, z plot::Ranges
    space::Axes3Style st;                      // numbers, gridXY/XZ/YZ, labels (TeX), colours
    space::axes(s, a, st, { 1, 1, k });        // how far each axis has grown: 1D -> 2D -> 3D
    space::MeshStyle ms;                       // color, back, colorAt(p), cellColor(r, c), opacity, shade, wire
    ms.colorAt = [](Vector3 p) { return plot::viridis(p.z); };
    s.mesh(hill.mapped([k](Vector3 p) { return Vector3{ p.x, p.y, p.z * k }; }), ms);
    const Vector3 p{ 2, 1.5f, 1.2f * k };
    s.arrow({ 0, 0, 0 }, p, mkColor(0xffd166), 5);
    space::guides(s, p, mkColor(0x8a90a2));    // dashed drops to the planes
    s.line(p, space::projectOnPlane(p, { 0, 0, 0 }, { 0, 0, 1 }), mkColor(0x83c167));
    s.label(p, R"(\vec v)", 40);               // also curve(pts, stroke), point, face, plane
    s.draw(cx);
};
```

* `surfaces::sphere`, `torus`, `glueTorus(roll, bend)` (sheet -> tube -> torus, both 0..1),
  `moebius(join)`, `klein()`; `Mesh::grid(rows, cols, f(u, v), wrapCols, wrapRows)` for
  your own, `Mesh::mix(a, b, u)` to morph between two of the same shape.
* 4D: `rotate4(v, axisA, axisB, deg)` (0 = x .. 3 = w), `project4(v, distance)`,
  `tesseract()` (16 vertices, 32 edges).
* Hyperbolic (`space::hyperbolic`, points are `std::complex<double>` in the unit disk):
  `tiling(p, q)` gives `Tile`s (`vertices`, `center`, `parity`, `ring`); `polygon`,
  `geodesic`, `line`, `circle`, `reflect`, `distance`, `isometry(z, a, deg)` (moves a to
  0: animate `a` to glide the plane); `toHalfPlane` / `toDisk`. Draw with
  `path(pts, map, closed, clip)` where map is `Disk{ centre, radius }.map` or
  `[](C z) { return HalfPlane{}.map(toHalfPlane(z)); }`.
* Spherical (`space::sphere`): `arc(a, b, radius)`, `circle(pole, radius, latitude)`,
  `angle(a, b, c)` (radians, at b), `stereographic(p, r)` / `fromPlane(q, r)` (from the
  north pole onto z = 0; points near the pole go far).

## Camera (`cam::`, `rmtn/camera.hpp`)

A 2D camera over a canvas bigger than the frame. Put the canvas under a `cam::world` node
and draw it in canvas px; nodes outside stay fixed (captions, HUDs).

```cpp
cam::Track tr;                                                   // tr.frame defaults to 1920x1080
tr.start(cam::Shot::fit({ 0, 0, 6000, 3400 }))                    // fit(rect, frame, margin, turnDeg)
  .hold(30)
  .to(100, cam::Shot::fit({ 400, 2100, 1000, 700 }))              // ease defaults to inOut(cubic)
  .fly(220, cam::Shot::fit({ 4600, 600, 1000, 700 }))             // zooms out across a long pan
  .to(280, cam::Shot{ { 5000, 900 }, 8, -10 }, ease::out(ease::cubic));   // centre, zoom, turn (deg)
auto shot = [tr](int t) { return cam::shake(tr.at(t), t, t > 200 ? 6.f : 0.f); };   // optional wobble
cam::world(c.roots, 0, 300, shot, { 1920, 1080 }, 0.35f)->fn = [](const Ctx& cx, int) { /* far layer */ };
cam::world(c.roots, 0, 300, shot)->fn = [](const Ctx& cx, int t) { /* the canvas, canvas px */ };
addRoot(c.roots, 0, 300)->fn = [tr](const Ctx& cx, int t) {
    const Rectangle seen = cam::visible(tr.at(t));                // canvas rect in view: a mini-map
    const Vector2 onScreen = cam::toFrame(tr.at(t), { 5000, 900 });   // pin a caption to a canvas point
    (void)seen; (void)onScreen;
};
```

`cam::world(parent, from, len, track)` nests a camera inside a scene. `Shot::mix(a, b, u)`
and `Shot::fly(a, b, u, frame)` give the blends directly. Vector paths, formulas and plots
stay sharp at any zoom; raylib text (`dr::text`) and images go soft past about 2x.

## Code (`code::`, `rmtn/code.hpp`)

Highlighted code cards for programming explainers. Lines are 1-based, columns 0-based.
Parse blocks and build morphs while registering.

```cpp
const code::Block v1 = code::Block::parse(R"(
def area(w, h):
    return w * h
)", "python");                                   // cpp c csharp java js ts python rust go glsl json bash sql text
const code::Block v2 = code::Block::parse(R"(
def area(w, h, d=1):
    """Volume, or area when d = 1."""
    return w * h * d
)", "python");
const code::Morph change(v1, v2);                // diff once: kept tokens glide, the rest fades
node->fn = [=](const Ctx& cx, int t) {
    code::Style st;                              // theme (Theme::dark()/light()), size, lineHeight, title,
    st.title = "area.py";                        // lineNumbers, card, padding, radius, columns, diffGutter
    code::Look look;
    look.reveal = (t - 10) * 2.0;                // typewriter: characters shown (< 0: all)
    look.cursor = true;
    look.t = t;                                  // caret blink once typing stops
    // look.lines = t / 8.f;                     // or line by line
    // look.focus = { { 2, 2 } }; look.focusFrom = { { 1, 1 } }; look.focusMix = u; look.dim = 0.25f;
    // look.marks.push_back(code::Mark::text(v1, 2, "w * h", GOLD));   // .u grows it, .underline
    const Vector2 at{ 960 - code::bounds(v2, st).width / 2, 200 };    // bounds(): the card's box
    if (t < 120) code::draw(cx, v1, at, st, look);
    else code::draw(cx, change, at, st, (t - 120) / 50.f);            // 0 = v1, 1 = v2
    const Vector2 p = code::cell(v1, st, at, 2, 11);   // top-left of a character, for notes and arrows
    (void)p;
};
```

`Block::chars()` is the reveal count for the whole block (a newline counts one), and
`Block::find(line, "text")` gives a column range. For a morph, give the card a fixed
width with `st.columns` so it does not resize, or let it grow from one size to the other.

## Text animation (`txt::`, `rmtn/textfx.hpp`)

Lay text out once, then animate each glyph. Fonts load with the window, after
registration, so wrap `Text::make` (and `Morph`) in `txt::later`, which makes it on first
use; call it inside the node fn.

```cpp
const auto texts = txt::later([] {
    TextStyle st;                                // font, size, spacing, lineHeight, color, align
    st.font = fonts::ui(true);
    st.size = 84;
    st.color = WHITE;
    st.align = Align::Center;                    // for several lines or a width
    struct T { txt::Text title, line; txt::Morph swap; };
    const txt::Text a = txt::Text::make("listen", st), b = txt::Text::make("silent", st);
    return T{ txt::Text::make("Every frame\nis a function", st, 1200),   // width: wraps
              txt::Text::make("Any glyph can be styled", st), txt::Morph(a, b) };
});
node->fn = [texts](const Ctx& cx, int t) {
    const auto& [title, line, swap] = texts();
    const Vector2 mid{ 0.5f, 0.5f };             // anchor: `at` is the block's centre
    txt::Reveal r;                               // by Letter (default), Word or Line
    r.order = txt::Order::Forward;               // Backward, Center, Edges, Random (r.seed)
    r.stagger = 2; r.duration = 14;              // frames between units, frames per unit
    r.ease = ease::out(ease::cubic);
    r.fade = 1; r.offset = { 0, 30 }; r.scale = 1; r.rotate = 0;   // where each unit starts from
    // r.out = true;                             // the same, leaving
    txt::draw(cx, title, { 960, 300 }, t - 10, r, mid);           // txt::length(title, r): frames to finish
    txt::draw(cx, line, { 960, 600 }, txt::chain({ txt::wave(t, 12),
        txt::highlight(line.word(2).first, line.word(2).second, GOLD, 1, 10, 1.1f) }), mid);
    // custom stylers: [&](const txt::Glyph& g, txt::GlyphLook& l) { l.alpha *= ...; l.offset.y += ...; }
    // txt::typewriter(cx, text, at, (t - 20) * 0.8, true, t);  // chars shown, caret, blink clock
    // txt::scramble(cx, text, at, t, r);                         // random characters settle per unit
    swap.draw(cx, { 960, 850 }, (float)interpolate(t, { 60, 90 }, { 0, 1 }, clamped()), mid);
};
```

`Glyph` has `index` (among visible glyphs), `word`, `line`, `inWord` and `pos`, for
stylers. Times are plain doubles, so pass `frameTime(t)` to have them follow motion blur.

## Motion blur

```cpp
node->isolate().motionBlur = { 16, 0.5f };      // samples, shutter in frames (0.5 = 180 degrees)
node->up = [](int t, NodeProps& p) { p.pos.x = (float)interpolate(frameTime(t), { 0, 12 }, { 0, 1400 }, clamped()); };
node->fn = [](const Ctx& cx, int t) {
    dr::circle(cx, { 200, (float)(540 + 300 * std::sin(frameTime(t) * 0.3)) }, 50, GOLD);   // blurs
    dr::text(fonts::ui(true), cx, "label", { 200, 300 }, 40, 0, WHITE, 0.5, 0.5);         // t: stays sharp
};
// comp.motionBlur = { 8, 0.5f };               // the whole picture (not with a custom `render`)
```

Only what reads `frameTime(t)` (or `t + subframe()`) moves between samples; `up` and
`layerUp` run again at each sample. Camera tracks and `fx::fn` particles already use it.
Each sample redraws the subtree: 8 to 16 is typical, 32 for whip pans. Blur the layer
that moves rather than the whole comp when you can.

## Particles (`fx::`, `rmtn/particles.hpp`)

Emitters are computed per frame from a seed (no simulation state), so they follow the
pure-frame rule automatically. Positions are relative to the node; speeds are px per
frame, gravity px per frame squared.

```cpp
fx::Emitter boom = fx::burst(150, { mkColor(0xffc365), mkColor(0xff6b8a) });   // preset, then adjust
boom.burstAt = 20;                          // frame of the node's local time
TNode* n = addRoot(c.roots, 0, 120);
n->props.pos = { 960, 400 };
n->fn = fx::fn(boom);                       // or fx::draw(cx, boom, t) inside your own fn

fx::Emitter e;                              // from scratch
e.rate = 2;                                 // particles per frame; or burst = N (+ burstAt, burstSpread)
e.prewarm = true;                           // already full on the first frame (weather, ambience)
e.area = fx::Area::Line; e.extent = { 1920, 0 };   // Point, Line, Rect, Circle, Ring
e.life = { 40, 80 };                        // fx::Range: random per particle in [lo, hi]
e.speed = { 1, 3 }; e.angle = -90; e.spread = 30;  // degrees: -90 up, 90 down; or outward = true
e.gravity = { 0, 0.1f }; e.drag = 0.03f; e.turbulence = 20;
e.look = fx::Look::Glow;                    // Dot, Glow, Streak, Square, Confetti, Star, Ring
e.size = { 6, 14 }; e.sizeEnd = 0.2f;       // size multiplier at death
e.colors = { WHITE };                       // one per particle; or overLife = { ... } as a gradient
e.additive = true;                          // light adds up: glows, fire, sparks
e.origin = [](float f) { return Vector2{ 10 * f, 0 }; };   // moving source, sampled at each birth
e.draw = [](const Ctx& cx, const fx::Particle& p) {        // custom look (p.pos, p.size, p.alpha, p.u, p.random)
    dr::circle(cx, p.pos, p.size / 2, withAlpha(p.color, p.alpha));
};
```

Presets: `burst`, `sparks`, `fountain`, `confetti(width)`, `rain(width, height)`,
`snow(width, height)`, `fire(width)`, `smoke(width)`, `embers(width, height)`. Put
confetti, rain and snow at the top edge (`{ 960, -40 }`) and fire and smoke at their base.
Vary `seed` between emitters that would otherwise look identical.

## Images, video and audio (`media::`)

```cpp
auto logo = media::image("logo.svg");       // PNG, JPG, GIF (animated), SVG (stays sharp), ...
node->fn = media::imageFn(logo, { 0, 0, 400, 200 });               // or inside your own fn:
media::drawImage(cx, *logo, { 0, 0, 400, 200 }, { media::Fit::Cover });   // Fill, Contain, Cover, None

media::VideoOptions vo; vo.startFrom = 2.0; vo.volume = 0.5f;      // seconds into the clip
media::video(addRoot(c.roots, 30, 150), "clip.mp4", { 0, 0, 1920, 1080 }, vo);   // needs ffmpeg

media::AudioOptions ao; ao.volume = 0.6f;
ao.volumeAt = [](int t) { return (float)interpolate(t, { 0, 30 }, { 0, 1 }, clamped()); };   // fade in
media::audio(addRoot(c.roots, 0, c.duration), "music.mp3", ao);    // WAV, OGG, MP3 (others via ffmpeg)
```

Files are found as given, then in `media::addSearchPath` directories,
`$RAYMOTION_ASSETS`, `<exe dir>/assets` and the engine's bundled assets.

Procedural music (no audio files): write a WAV in `c.assets` and play it. `renderSong`
(`rmtn/synth.hpp`) sums sines per note; `pluck` notes decay. It returns mono samples;
`media::writeWav` takes interleaved stereo.

```cpp
c.assets = [](const Comp&, const Json&) {
    Song song;                              // SongEvent {start s, duration s, frequency Hz, amplitude, pluck}
    for (int i = 0; i < 16; i++) song.push_back({ i * 0.5, 0.45, 220.0 * (1 + i % 4) / 2, 0.3, true });
    const std::vector<float> mono = renderSong(song, 8.0, 48000);
    std::vector<float> stereo(mono.size() * 2);
    for (size_t i = 0; i < mono.size(); i++) stereo[2 * i] = stereo[2 * i + 1] = mono[i];
    std::filesystem::create_directories("out");
    media::writeWav("out/music.wav", stereo, 48000);
};
media::audio(addRoot(c.roots, 0, c.duration), "out/music.wav");
```

## Subtitles and voice-over (`sub::`, `tts::`, `rmtn/subtitles.hpp`)

```cpp
sub::Track track = sub::load("voiceover.srt");            // SRT or WebVTT; or sub::parse(text)
sub::Style style = sub::defaultStyle();                   // white, dark outline, bottom centre
style.box = withAlpha(BLACK, 0.6f);                       // optional backing box
addRoot(c.roots, 0, c.duration)->fn = sub::captions(track, style);

double s = track.when("the optimizer");                   // seconds: cue start + share of its characters
int f = track.frameOf("Finally", c.fps);
auto spans = track.spans({ "First,", "Then the optimizer", "Finally" }, c.fps, 30);   // {from, length} per scene
sub::writeSrt("out/voiceover.srt", track);
```

- Phrases match ignoring case, punctuation and line breaks; a missing phrase throws.
  `when(phrase, after)` searches from `after` seconds for repeated words.
- Style: `text` (font, size, colour), `maxWidth`, `bottom`, `align` / `margin`,
  `outline` / `outlineWidth`, `box` / `boxPadding` / `boxRadius`, `fade` (frames).
  Karaoke: set `upcoming` to the colour of words not yet spoken.
- `sub::draw(cx, cue, seconds, style)` draws a cue anywhere in your own fn (for example
  big and mid-screen); `track.at(seconds)` finds the cue showing.

Text to speech: `tts::Script(voice, lines, gap, lead)` speaks lines one after another.
`generate()` runs the engine for takes not yet cached (call it from `c.assets`; it
throws when the engine fails, so catch it to fall back), and `measure()` reads cached
durations without speaking (call it from `c.build`). Before the takes exist, durations
are estimated at `wordsPerSecond`. `track()` gives a cue per line, and
`attach(node, fps, volume)` plays the takes.

```cpp
tts::Voice voice = tts::Voice::system();                  // SAPI / say / espeak-ng
voice.command = "piper --model en_US-amy-medium.onnx --output_file {out} < {textfile}";   // or any engine
auto script = std::make_shared<tts::Script>(voice, lines, 0.45, 0.4);
c.assets = [script](const Comp&, const Json&) {
    try { script->generate(); } catch (const std::exception& e) { fprintf(stderr, "note: %s\n", e.what()); }
};
c.build = [script](Comp& comp, const Json&) { script->measure(); /* lay out from script->track() */ };
```

## Props, prefabs and generated assets

```cpp
c.defaultProps = Json::parse(R"({"title": "Hello", "accent": "#47dad8", "items": ["a", "b"]})");
c.build = [](Comp& c, const Json& props) {   // runs with defaults merged with --props
    c.roots.clear();
    const std::string title = props.str("title", "");
    const Color accent = props.color("accent", WHITE);
    for (const Json& it : props["items"].asArray()) { /* it.asString() */ }
    c.duration = 90 + 30 * (int)props["items"].asArray().size();   // may change length or size
};
c.assets = [](const Comp& c, const Json& props) { /* write generated files */ };

prefab::add({ "card", Json::parse(R"({"text": "", "delay": 0})"),
              [](TNode& n, const Json& p) { /* set n.up, n.fn, add kids */ } });
prefab::use(parent, 30, 60, "card", Json::parse(R"({"text": "Fast"})"));
```

Json lookups take a fallback: `str`, `num`, `integer`, `flag`, `color`; also `props["key"]`,
`asArray()`, `asString()`, `asNumber()`, `has(key)`, `set(key, value)`, `dump()`.
