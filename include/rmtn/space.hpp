// rmtn/space.hpp -- space:: diagrams in 1, 2, 3 (and 4) dimensions, drawn as crisp vector
// paths like plot::, and non-Euclidean geometry.
//
//   * View: a camera for math diagrams. View::flat() looks straight down z, so the xy plane
//     is an ordinary 2D plot; View::tilted() is a 3D view with z up. View::mix eases between
//     them, so a diagram can go 1D -> 2D -> 3D in one move.
//   * Scene: a per-frame draw list in world units (lines, curves, arrows, points, faces,
//     meshes, labels), sorted back to front and drawn with vg::.
//   * axes(): x, y, z axes that grow one dimension at a time; guides() reads a point's
//     coordinates off with dashed lines; projectOnPlane / projectOnLine for projections.
//   * Mesh: grids of points (surfaces z = f(x, y), parametric surfaces, topology morphs:
//     sheet -> cylinder -> torus, strip -> Moebius band, the Klein bottle), and 4D polytopes.
//   * hyperbolic:: the Poincare disk (geodesics, circles, regular tilings, isometries) and
//     the upper half-plane; sphere:: great circles and stereographic projection.
//
//   node->fn = [](const Ctx& cx, int t) {
//       const float k = ramp(t, 60, 120);
//       space::Scene s(space::View::mix(space::View::flat({ 960, 540 }, 90), space::View::tilted({ 960, 600 }, 90), k));
//       space::axes(s, {}, {}, { 1, 1, k });                        // the z axis rises as the view tilts
//       s.point({ 2, 1.5f, 2 * k }, SKYBLUE);
//       space::guides(s, { 2, 1.5f, 2 * k }, SKYBLUE);
//       s.draw(cx);
//   };
//
// World coordinates are right-handed with z up (towards the viewer in the flat view).
// Every call is a pure function of its arguments, so frames render independently.
#pragma once
#include <complex>
#include <functional>
#include <string>
#include <vector>
#include "paths.hpp"
#include "plot.hpp"

namespace rmtn {
namespace space {

// ------------------------------------------------------------------ the camera

struct View {
    Vector2 center{ 960, 540 };       // where `target` lands (node px)
    float scale = 100;                // px per world unit (at the target, in perspective)
    float yaw = 0;                    // degrees about the world z axis
    float pitch = 0;                  // degrees: 0 looks straight down z; negative tips the xy plane back (z up)
    float perspective = 0;            // 0 orthographic .. 1 full perspective
    float distance = 16;              // camera distance from the target, world units (perspective)
    Vector3 target{ 0, 0, 0 };
    // Looking down z: x right, y up, as a 2D plot.
    static View flat(Vector2 center = { 960, 540 }, float scale = 100);
    // A 3D view with z up, turned and tipped back.
    static View tilted(Vector2 center = { 960, 580 }, float scale = 100, float yaw = -32, float pitch = -64, float perspective = 0.35f);
    static View mix(const View& a, const View& b, float u);   // every field blended
    Vector3 toView(Vector3 p) const;  // camera space: x right, y up, z towards the viewer
    Vector2 project(Vector3 p) const; // node px
    float depth(Vector3 p) const;     // larger = nearer the viewer
    float scaleAt(Vector3 p) const;   // px per unit at p
};

// ------------------------------------------------------------------ meshes

// A grid of points (rows x cols, row-major); quads join neighbours. wrapCols / wrapRows
// close the grid across its last column / row (a cylinder, a torus).
struct Mesh {
    int rows = 0, cols = 0;
    std::vector<Vector3> pts;
    bool wrapCols = false, wrapRows = false;
    Vector3 at(int r, int c) const { return pts[(size_t)r * cols + c]; }
    // f(u, v) with u = 0..1 across columns and v = 0..1 down rows (wrapped: the last
    // column / row stops one step short of 1).
    static Mesh grid(int rows, int cols, const std::function<Vector3(float u, float v)>& f, bool wrapCols = false, bool wrapRows = false);
    // z = f(x, y) over the ranges.
    static Mesh graph(const std::function<double(double, double)>& f, plot::Range x, plot::Range y, int rows = 40, int cols = 40);
    Mesh mapped(const std::function<Vector3(Vector3)>& f) const;
    static Mesh mix(const Mesh& a, const Mesh& b, float u);   // same shape; wraps from b
};

struct MeshStyle {
    Color color{ 88, 196, 221, 255 };
    Color back{ 0, 0, 0, 0 };         // the far side's colour; transparent: same as the front
    std::function<Color(Vector3)> colorAt;   // per face (at its centre), overrides `color`
    std::function<Color(int row, int col)> cellColor;   // per grid cell, overrides both
    float opacity = 1;
    bool shade = true;                // light from the upper left
    float wire = 1.2f;                // grid lines, px; 0 for none
    Color wireColor{ 255, 255, 255, 70 };
    int wireEvery = 1;                // a line every n rows / columns
};

// ------------------------------------------------------------------ the draw list

class Scene {
public:
    explicit Scene(const View& v);
    const View& view() const { return view_; }

    void line(Vector3 a, Vector3 b, const vg::Stroke& s);
    void line(Vector3 a, Vector3 b, Color c, float width = 3) { line(a, b, vg::Stroke(c, width)); }
    // A polyline, split into depth-sorted pieces; dashes run on across them.
    void curve(const std::vector<Vector3>& pts, const vg::Stroke& s, bool closed = false);
    void curve(const std::vector<Vector3>& pts, Color c, float width = 3, bool closed = false) { curve(pts, vg::Stroke(c, width), closed); }
    void arrow(Vector3 from, Vector3 to, Color c, float width = 4, float head = 18);
    void point(Vector3 p, Color c, float radius = 9);   // radius in px, shrinking with distance
    // A planar polygon, shaded by its facing unless shade is false.
    void face(const std::vector<Vector3>& pts, Color c, bool shade = true);
    void mesh(const Mesh& m, const MeshStyle& s = {});
    // A square of side `size` through `at`, facing `normal`, with an optional grid.
    void plane(Vector3 at, Vector3 normal, float size, Color c, int gridLines = 0, Color gridColor = { 255, 255, 255, 40 });
    // TeX at a 3D point, nudged by `offset` px; sorted by depth like the rest, so a surface in
    // front hides it.
    void label(Vector3 at, const std::string& tex, float size = 34, Color c = { 236, 239, 244, 255 }, Vector2 anchor = { 0.5f, 0.5f },
               Vector2 offset = { 0, 0 });
    void draw(const Ctx& c) const;    // back to front

private:
    struct Item { float depth; int kind; size_t index; };
    struct Piece { std::vector<Vector2> pts; vg::Stroke stroke; };
    struct Face { std::vector<Vector2> pts; Color fill; vg::Path edges; vg::Stroke edge; };
    struct Dot { Vector2 at; float r; Color color; };
    struct Text { Vector2 at; std::string tex; float size; Color color; Vector2 anchor; };
    View view_;
    std::vector<Item> items_;
    std::vector<Piece> pieces_;
    std::vector<Face> faces_;
    std::vector<Dot> dots_;
    std::vector<Text> texts_;
    Color shadeOf(Color c, Vector3 a, Vector3 b, Vector3 cc, bool* back) const;
};

// ------------------------------------------------------------------ axes and guides

struct Axes3 {
    plot::Range x{ -4, 4 }, y{ -3, 3 }, z{ -3, 3 };
};
struct Axes3Style {
    Color color{ 200, 204, 214, 255 };
    float width = 3;
    bool arrows = true, ticks = true, numbers = true;
    float numberSize = 26;
    Color numberColor{ 200, 204, 214, 255 };
    bool gridXY = false, gridXZ = false, gridYZ = false;   // grids in the coordinate planes
    Color gridColor{ 120, 130, 150, 60 };
    std::string xLabel = "x", yLabel = "y", zLabel = "z";   // TeX, at the positive ends
    float labelSize = 38;
};
// grow = how far each axis has drawn out from the origin (0..1): animate x, then y, then z
// for 1D -> 2D -> 3D. Grids, ticks and numbers follow their axes.
void axes(Scene& s, const Axes3& a = {}, const Axes3Style& st = {}, Vector3 grow = { 1, 1, 1 });
// Dashed lines from p to where it meets the coordinate planes and axes (dims 2: x and y
// only; dims 1: straight down to the x axis).
void guides(Scene& s, Vector3 p, Color c, int dims = 3, float width = 2.5f);

// ------------------------------------------------------------------ projections

Vector3 projectOnPlane(Vector3 p, Vector3 planePoint, Vector3 normal);   // orthogonal
Vector3 projectOnLine(Vector3 p, Vector3 linePoint, Vector3 direction);
Vector3 lerp(Vector3 a, Vector3 b, float u);

// ------------------------------------------------------------------ surfaces and topology

namespace surfaces {
Mesh sphere(float radius = 2, int rows = 24, int cols = 48);
Mesh torus(float ring = 2.4f, float tube = 0.9f, int rows = 32, int cols = 64);
// A w x h sheet in the xy plane glued in two steps: roll = 0..1 rolls it into a tube along
// y (joining its left and right edges), bend = 0..1 bends the tube round into a torus lying
// in the xy plane (joining the ends). Rows run along h, columns along w.
Mesh glueTorus(float roll, float bend, float w = 3.6f, float h = 9, int rows = 48, int cols = 32);
// A strip of length `length` and width `width` along x, bent round into a loop with a half
// twist as join = 0..1 (1: the Moebius band). twists counts the half twists.
Mesh moebius(float join, float length = 12, float width = 2, int twists = 1, int rows = 16, int cols = 96);
// The Klein bottle (the classic "bottle" immersion), about 4 units tall, standing on z.
Mesh klein(int rows = 48, int cols = 64);
}

// ------------------------------------------------------------------ four dimensions

struct Vec4 { double x = 0, y = 0, z = 0, w = 0; };
// Rotate in the plane of axes a and b (0 = x, 1 = y, 2 = z, 3 = w).
Vec4 rotate4(Vec4 p, int a, int b, double degrees);
// Perspective from w: the eye at w = distance looks towards the origin; orthographic when distance <= 0.
Vector3 project4(Vec4 p, double distance = 3);
struct Polytope4 {
    std::vector<Vec4> vertices;
    std::vector<std::pair<int, int>> edges;
};
Polytope4 tesseract(double half = 1);

// ------------------------------------------------------------------ hyperbolic geometry

namespace hyperbolic {
using C = std::complex<double>;

// The Poincare disk on screen.
struct Disk {
    Vector2 center{ 960, 540 };
    float radius = 400;
    Vector2 map(C z) const { return { center.x + (float)z.real() * radius, center.y - (float)z.imag() * radius }; }
};
// The upper half-plane on screen: w = x + iy with y > 0, `unit` px per unit, `origin` where 0 lands.
struct HalfPlane {
    Vector2 origin{ 960, 900 };
    float unit = 200;
    Vector2 map(C w) const { return { origin.x + (float)w.real() * unit, origin.y - (float)w.imag() * unit }; }
};
C toHalfPlane(C z);                   // Cayley: disk -> half-plane
C toDisk(C w);                        // half-plane -> disk

double distance(C a, C b);            // hyperbolic distance in the disk
// The isometry moving a to 0 and then turning by `degrees`; with a = 0 a pure rotation.
C isometry(C z, C a, double degrees = 0);
// The geodesic segment from a to b, sampled (points in the disk).
std::vector<C> geodesic(C a, C b, int samples = 48);
// The whole geodesic through a and b, boundary to boundary.
std::vector<C> line(C a, C b, int samples = 96);
// A closed polygon with geodesic edges.
std::vector<C> polygon(const std::vector<C>& vertices, int samplesPerEdge = 24);
// The circle of hyperbolic radius r about c.
std::vector<C> circle(C c, double r, int samples = 96);
// Reflection in the geodesic through a and b.
C reflect(C z, C a, C b);

struct Tile {
    std::vector<C> vertices;
    C center;
    int parity = 0;                   // flips with each reflection (for two-colouring)
    int ring = 0;                     // reflections from the centre tile
};
// The regular tiling {p, q} (p-gons, q at each vertex; 1/p + 1/q < 1/2), grown out from a
// tile centred at 0 until tiles are smaller than minSize (Euclidean) or maxTiles is reached.
std::vector<Tile> tiling(int p, int q, int maxTiles = 1500, double minSize = 0.01);

// Screen paths from sampled points; `map` is Disk::map, HalfPlane::map after toHalfPlane,
// or anything else. Points outside `clip` (when it has an area) are clipped away.
vg::Path path(const std::vector<C>& pts, const std::function<Vector2(C)>& map, bool closed = false, Rectangle clip = { 0, 0, 0, 0 });
}

// ------------------------------------------------------------------ spherical geometry

namespace sphere {
// The great-circle arc from a to b on the sphere of `radius` about the origin.
std::vector<Vector3> arc(Vector3 a, Vector3 b, float radius = 2, int samples = 64);
// A whole great circle with pole n, or a small circle (latitude, degrees) about n.
std::vector<Vector3> circle(Vector3 n, float radius = 2, float latitude = 0, int samples = 128);
// The angle at b of the spherical triangle a, b, c (radians).
double angle(Vector3 a, Vector3 b, Vector3 c);
// Stereographic projection from the north pole of the sphere (radius r, centred at the
// origin) onto the plane z = 0, and back.
Vector3 stereographic(Vector3 p, float radius = 2);
Vector3 fromPlane(Vector3 q, float radius = 2);
}

} } // rmtn::space
