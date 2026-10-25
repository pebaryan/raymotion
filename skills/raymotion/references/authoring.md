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
