// rmtn/paths.hpp -- vector paths (vg::): Bézier paths from code or SVG path data, filled by
// SVG's rules, stroked with joins, caps and dashes, measured by length, trimmed for
// draw-on animation, morphed into one another, and whole SVG files as drawings.
//
//   vg::Path p = vg::Path::parse("M 0 0 C 100 -80 200 80 300 0");   // or moveTo/lineTo/cubicTo...
//   vg::Stroke s; s.color = WHITE; s.width = 6;
//   node->fn = [p, s](const Ctx& cx, int t) {
//       const float u = (float)interpolate(t, { 0, 40 }, { 0, 1 }, clamped());
//       vg::stroke(cx, p.trim(0, u), s);                            // draws itself on
//   };
//
// Coordinates are the node's (composition px unless scaled); stroke widths and dash lengths
// are in the same units, so they scale with the node. Curves are flattened at draw time to
// a fraction of an output pixel, so paths stay smooth at any scale.
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "timeline.hpp"

namespace rmtn {
namespace vg {

// One piece of a contour, ending at `to` (it starts where the previous one ends).
struct Seg {
    enum class Kind { Line, Quad, Cubic };
    Kind kind = Kind::Line;
    Vector2 c1{ 0, 0 }, c2{ 0, 0 };   // control points (Quad uses c1)
    Vector2 to{ 0, 0 };
};

// A run of segments from `start`. Closed contours end with a segment back to `start`.
struct Contour {
    Vector2 start{ 0, 0 };
    std::vector<Seg> segs;
    bool closed = false;
};

// A flattened contour (see Path::flatten).
struct Polyline {
    std::vector<Vector2> pts;
    bool closed = false;
};

class Path {
public:
    std::vector<Contour> contours;

    // ---- building (SVG semantics; arcs become cubics)
    Path& moveTo(Vector2 p);
    Path& lineTo(Vector2 p);
    Path& quadTo(Vector2 c, Vector2 p);
    Path& cubicTo(Vector2 c1, Vector2 c2, Vector2 p);
    // SVG elliptical arc to p: radii, x-axis rotation in degrees, large-arc and sweep flags.
    Path& arcTo(Vector2 radii, float rotationDeg, bool largeArc, bool sweep, Vector2 p);
    Path& close();
    Path& append(const Path& other);
    bool empty() const;

    // ---- shapes
    static Path parse(const std::string& svgPathData);   // "M10 10 h 80 a 20 20 0 0 1 ..."; throws on bad data
    static Path line(Vector2 a, Vector2 b);
    static Path polyline(const std::vector<Vector2>& pts, bool closed = false);
    // A smooth curve through every point (Catmull-Rom as cubics).
    static Path smooth(const std::vector<Vector2>& pts, bool closed = false);
    static Path rect(Rectangle r, float radius = 0);
    static Path circle(Vector2 c, float r);
    static Path ellipse(Vector2 c, float rx, float ry);
    static Path arc(Vector2 c, float r, float fromDeg, float toDeg);   // 0 deg = +x, clockwise on screen
    static Path regular(Vector2 c, float r, int sides, float rotationDeg = -90);
    static Path star(Vector2 c, float rOuter, float rInner, int points, float rotationDeg = -90);

    // ---- transforms (applied to the control points)
    Path translated(Vector2 d) const;
    Path scaled(float sx, float sy, Vector2 about = { 0, 0 }) const;
    Path rotated(float deg, Vector2 about = { 0, 0 }) const;
    Path transformed(const std::function<Vector2(Vector2)>& f) const;
    Path reversed() const;

    // ---- measuring, by arc length over all contours in order
    Rectangle bounds() const;
    float length() const;
    Vector2 pointAt(float u) const;   // u = 0..1 of the total length
    float angleAt(float u) const;     // direction of travel there, degrees
    // The part from `from` to `to` (0..1). By default the length runs through the contours
    // in order; with `eachContour` every contour is trimmed by the same fraction at once.
    Path trim(float from, float to, bool eachContour = false) const;

    // Curves as polylines, each within `tolerance` of the true curve.
    std::vector<Polyline> flatten(float tolerance = 0.25f) const;
};

enum class Join { Miter, Round, Bevel };
enum class Cap { Butt, Round, Square };
enum class FillRule { NonZero, EvenOdd };

struct Stroke {
    Stroke() = default;
    Stroke(Color c, float w) : color(c), width(w) {}
    Color color{ 255, 255, 255, 255 };
    float width = 2;
    Join join = Join::Round;
    Cap cap = Cap::Round;
    float miterLimit = 4;
    std::vector<float> dash;          // on, off, on, ... lengths (empty: solid)
    float dashOffset = 0;             // animate it to march the dashes
};

struct Style {
    Color fill{ 0, 0, 0, 0 };         // transparent: no fill
    Stroke stroke{ { 0, 0, 0, 0 }, 0 };   // zero width or transparent: no stroke
    FillRule rule = FillRule::NonZero;
    static Style filled(Color c) { Style s; s.fill = c; return s; }
    static Style outlined(Color c, float width) { Style s; s.stroke.color = c; s.stroke.width = width; return s; }
};

// Translucent fills and strokes cover each pixel once (no darker overlaps at joins or
// where contours cross). Fills with several contours, holes or self-crossings use the GPU
// winding fill (exact by `rule`); a lone simple contour is triangulated directly.
void fill(const Ctx& c, const Path& p, Color color, FillRule rule = FillRule::NonZero);
void stroke(const Ctx& c, const Path& p, const Stroke& s);
void draw(const Ctx& c, const Path& p, const Style& s);    // fill, then stroke
// Handwriting-style reveal, u = 0..1: every contour's outline draws on (to u = 0.6), then
// the fill fades in (from 0.5). Without a stroke the outline uses the fill colour and fades.
void write(const Ctx& c, const Path& p, const Style& s, float u);

// Shape morphing: contours are matched (largest to largest; extra ones grow from or shrink
// to a point), resampled to the same number of points and aligned to travel the least.
// Build once, then sample at any u.
class Morph {
public:
    Morph() = default;
    Morph(const Path& from, const Path& to, int samplesPerContour = 160);
    Path at(float u) const;
private:
    struct Pair { std::vector<Vector2> a, b; bool closedA = true, closedB = true; };
    std::vector<Pair> pairs_;
};
Style mix(const Style& a, const Style& b, float u);

// ---- SVG drawings (parsed by nanosvg: paths, basic shapes, groups, transforms, styles;
// gradients are drawn as the average of their stops)
struct Shape {
    std::string id;
    Path path;
    Style style;
};
struct Drawing {
    float width = 0, height = 0;      // the SVG's size in its own units
    std::vector<Shape> shapes;        // in paint order
    static Drawing load(const std::string& file);   // resolved like media files; throws if unreadable
    static Drawing parse(const std::string& svgText);
    const Shape* find(const std::string& id) const;
};
// The context that maps drawing units into `box` (scaled to fit, centred).
Ctx fit(const Ctx& c, const Drawing& d, Rectangle box);
void draw(const Ctx& c, const Drawing& d, Rectangle box);
// Shapes write themselves one after another; `overlap` 0..1 is how much each overlaps the next.
void write(const Ctx& c, const Drawing& d, Rectangle box, float u, float overlap = 0.6f);

} } // rmtn::vg
