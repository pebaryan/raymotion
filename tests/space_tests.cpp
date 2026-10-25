// space_tests.cpp -- space:: and cam:: without a GPU: flat views agree with 2D, tilted
// views order depth, meshes count and close up when glued, the {p,q} tilings, geodesics
// meeting the boundary at right angles, isometries, spherical triangles and stereographic
// projection, the tesseract; camera shots round-trip, fit, blend, fly and keyframe.
#include "rmtn/space.hpp"
#include "rmtn/camera.hpp"
#include <cmath>
#include <cstdio>

using namespace rmtn;
using C = space::hyperbolic::C;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static bool near(double a, double b, double eps = 1e-3) { return std::fabs(a - b) <= eps; }
static double dist3(Vector3 a, Vector3 b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }
static double norm3(Vector3 a) { return dist3(a, { 0, 0, 0 }); }

static void views() {
    // flat: a 2D plot, y up
    const space::View f = space::View::flat({ 960, 540 }, 100);
    const Vector2 p = f.project({ 1, 2, 5 });
    CHECK(near(p.x, 1060) && near(p.y, 340));
    // tilted, z up: higher points sit higher on screen, and nearer the viewer is larger
    const space::View t = space::View::tilted({ 960, 540 }, 100, -30, -64, 0.4f);
    CHECK(t.project({ 0, 0, 1 }).y < t.project({ 0, 0, 0 }).y);
    const Vector2 o = t.project({ 0, 0, 0 });
    CHECK(near(o.x, 960) && near(o.y, 540));
    // straight down the pitch: the back of the floor (+y) is further away than the front
    CHECK(t.depth({ 0, -3, 0 }) > t.depth({ 0, 3, 0 }));
    CHECK(t.scaleAt({ 0, -3, 0 }) > t.scaleAt({ 0, 3, 0 }));
    // mix: the ends are the views themselves
    const space::View m0 = space::View::mix(f, t, 0), m1 = space::View::mix(f, t, 1);
    CHECK(near(m0.project({ 1, 1, 1 }).x, f.project({ 1, 1, 1 }).x) && near(m1.project({ 1, 1, 1 }).y, t.project({ 1, 1, 1 }).y));
    // projections onto planes and lines
    const Vector3 q = space::projectOnPlane({ 1, 2, 3 }, { 0, 0, -1 }, { 0, 0, 2 });
    CHECK(near(q.x, 1) && near(q.y, 2) && near(q.z, -1));
    const Vector3 l = space::projectOnLine({ 3, 4, 0 }, { 0, 0, 0 }, { 1, 0, 0 });
    CHECK(near(l.x, 3) && near(l.y, 0));
}

static void meshes() {
    const space::Mesh g = space::Mesh::graph([](double x, double y) { return x + y; }, { -1, 1 }, { -2, 2 }, 5, 7);
    CHECK(g.rows == 5 && g.cols == 7 && g.pts.size() == 35);
    CHECK(near(g.at(0, 0).x, -1) && near(g.at(0, 6).x, 1));
    for (Vector3 p : g.pts) CHECK(near(p.z, p.x + p.y));
    // gluing: flat, then the tube closes, then the torus
    const space::Mesh sheet = space::surfaces::glueTorus(0, 0, 4, 10, 9, 9);
    for (Vector3 p : sheet.pts) CHECK(near(p.z, 0));
    CHECK(near(sheet.at(0, 8).x - sheet.at(0, 0).x, 4) && near(sheet.at(8, 0).y - sheet.at(0, 0).y, 10));
    const space::Mesh tube = space::surfaces::glueTorus(1, 0, 4, 10, 9, 9);
    for (int r = 0; r < 9; r++) CHECK(dist3(tube.at(r, 0), tube.at(r, 8)) < 1e-3);
    CHECK(dist3(tube.at(0, 0), tube.at(8, 0)) > 9.9);   // the ends are still apart
    const space::Mesh torus = space::surfaces::glueTorus(1, 1, 4, 10, 9, 9);
    for (int c = 0; c < 9; c++) CHECK(dist3(torus.at(0, c), torus.at(8, c)) < 1e-3);
    // the torus lies flat: the tube's circle is 4 round, the ring 10 round
    double zmax = 0;
    for (Vector3 p : torus.pts) zmax = std::max(zmax, (double)std::fabs(p.z));
    CHECK(zmax < 4 / 3.14159 + 0.01);
    // the Moebius band closes with its width flipped
    const space::Mesh mb = space::surfaces::moebius(1, 12, 2, 1, 5, 33);
    CHECK(dist3(mb.at(0, 0), mb.at(4, 32)) < 1e-3 && dist3(mb.at(4, 0), mb.at(0, 32)) < 1e-3);
    // a sphere's points sit at its radius
    for (Vector3 p : space::surfaces::sphere(2, 8, 16).pts) CHECK(near(norm3(p), 2));
}

static void fourD() {
    const space::Polytope4 t = space::tesseract(1);
    CHECK(t.vertices.size() == 16 && t.edges.size() == 32);
    for (auto e : t.edges) {
        const space::Vec4 a = t.vertices[(size_t)e.first], b = t.vertices[(size_t)e.second];
        CHECK(near(std::fabs(a.x - b.x) + std::fabs(a.y - b.y) + std::fabs(a.z - b.z) + std::fabs(a.w - b.w), 2));
    }
    const space::Vec4 r = space::rotate4({ 1, 0, 0, 0 }, 0, 3, 90);
    CHECK(near(r.x, 0) && near(std::fabs(r.w), 1));
    // orthographic drops w; perspective shrinks the far cube (w < 0)
    const Vector3 o = space::project4({ 1, 2, 3, 4 }, 0);
    CHECK(near(o.x, 1) && near(o.y, 2) && near(o.z, 3));
    CHECK(norm3(space::project4({ 1, 1, 1, -1 }, 3)) < norm3(space::project4({ 1, 1, 1, 1 }, 3)));
}

static void hyperbolic() {
    namespace h = space::hyperbolic;
    // {7,3}: the centre heptagon, its vertices at tanh(R/2) with cosh R = cot(pi/7) cot(pi/3)
    const auto tiles = h::tiling(7, 3, 200);
    CHECK(tiles.size() == 200 && tiles[0].vertices.size() == 7);
    const double R = std::acosh(1 / (std::tan(3.14159265358979 / 7) * std::tan(3.14159265358979 / 3)));
    CHECK(near(std::abs(tiles[0].vertices[0]), std::tanh(R / 2), 1e-9));
    CHECK(tiles[1].ring == 1 && tiles[1].parity == 1);
    // every tile is congruent: the centre to a vertex is R hyperbolically
    for (size_t i = 0; i < 40; i++) CHECK(near(h::distance(tiles[i].center, tiles[i].vertices[0]), R, 1e-6));
    CHECK(h::tiling(4, 4).empty());   // Euclidean, not hyperbolic
    // isometries keep distances and move a to the centre
    const C a{ 0.3, 0.2 }, b{ -0.1, 0.5 };
    CHECK(near(std::abs(h::isometry(a, a, 40)), 0, 1e-12));
    CHECK(near(h::distance(h::isometry(a, a, 40), h::isometry(b, a, 40)), h::distance(a, b), 1e-9));
    // a geodesic line meets the boundary at right angles: its circle is orthogonal to |z| = 1
    const auto ln = h::line(a, b);
    CHECK(near(std::abs(ln.front()), 1, 1e-6) && near(std::abs(ln.back()), 1, 1e-6));
    const auto seg = h::geodesic(a, b, 32);
    CHECK(std::abs(seg.front() - a) < 1e-9 && std::abs(seg.back() - b) < 1e-9);
    // the geodesic is the shortest way: the distances along it add up
    double along = 0;
    for (size_t i = 1; i < seg.size(); i++) along += h::distance(seg[i - 1], seg[i]);
    CHECK(near(along, h::distance(a, b), 1e-4));
    // reflections are involutions; the Cayley map round-trips and sends the disk up
    CHECK(std::abs(h::reflect(h::reflect(C(0.1, -0.2), a, b), a, b) - C(0.1, -0.2)) < 1e-9);
    CHECK(std::abs(h::toDisk(h::toHalfPlane(a)) - a) < 1e-9 && h::toHalfPlane(a).imag() > 0);
    CHECK(std::abs(h::toHalfPlane(0) - C(0, 1)) < 1e-12);
    // circles: every point at hyperbolic distance r
    for (C z : h::circle(a, 1.5, 12)) CHECK(near(h::distance(z, a), 1.5, 1e-6));
    // clipping keeps paths inside the rectangle
    const vg::Path p = h::path(h::line(C(0, 0), C(0.5, 0)), [](C z) { return Vector2{ (float)z.real() * 1000, 100 }; }, false, { 0, 0, 300, 200 });
    for (const auto& pl : p.flatten()) for (Vector2 q : pl.pts) CHECK(q.x >= -0.5f && q.x <= 300.5f);
}

static void spherical() {
    namespace s = space::sphere;
    // the octant triangle has three right angles
    const Vector3 A{ 2, 0, 0 }, B{ 0, 2, 0 }, Cc{ 0, 0, 2 };
    CHECK(near(s::angle(A, B, Cc), 3.14159265 / 2, 1e-5) && near(s::angle(B, Cc, A), 3.14159265 / 2, 1e-5));
    for (Vector3 p : s::arc(A, B, 2, 16)) CHECK(near(norm3(p), 2, 1e-4));
    CHECK(dist3(s::arc(A, B, 2, 16).back(), B) < 1e-4);
    // stereographic: the equator stays put, the south pole goes to the origin, round trip
    const Vector3 e = s::stereographic({ 2, 0, 0 }, 2), sp = s::stereographic({ 0, 0, -2 }, 2);
    CHECK(near(e.x, 2) && near(sp.x, 0) && near(sp.y, 0));
    const Vector3 p{ 1.2f, -0.8f, std::sqrt(4 - 1.44f - 0.64f) };
    CHECK(dist3(s::fromPlane(s::stereographic(p, 2), 2), p) < 1e-4);
}

static void cameras() {
    const Vector2 F{ 1920, 1080 };
    const cam::Shot s{ { 3000, 1200 }, 2.5f, 20 };
    // canvas <-> frame round trip; the centre lands mid-frame
    const Vector2 c = cam::toFrame(s, { 3000, 1200 }, F);
    CHECK(near(c.x, 960) && near(c.y, 540));
    const Vector2 back = cam::toCanvas(s, cam::toFrame(s, { 3100, 1170 }, F), F);
    CHECK(near(back.x, 3100) && near(back.y, 1170));
    // a canvas step of 10 px is zoom * 10 px on screen
    const Vector2 a = cam::toFrame(s, { 0, 0 }, F), b = cam::toFrame(s, { 10, 0 }, F);
    CHECK(near(std::hypot(b.x - a.x, b.y - a.y), 25));
    // fit: the rectangle fills the frame less the margin along its tighter side
    const cam::Shot fit = cam::Shot::fit({ 1000, 1000, 960, 270 }, F, 0);
    CHECK(near(fit.zoom, 2) && near(fit.center.x, 1480) && near(fit.center.y, 1135));
    const Rectangle v = cam::visible(fit, F);
    CHECK(near(v.x, 1000) && near(v.width, 960) && near(v.y, 865) && near(v.height, 540));
    // props: a node with these props draws canvas p where toFrame says
    const NodeProps np = cam::props(s, F);
    const Ctx cx{ np.pos, np.scale, np.rot, 1 };
    const Vector2 m = mapP(cx, { 3100, 1170 }), want = cam::toFrame(s, { 3100, 1170 }, F);
    CHECK(near(m.x, want.x, 0.05) && near(m.y, want.y, 0.05));
    // parallax 0 is the identity; 1 is the shot
    const NodeProps still = cam::props(s, F, 0);
    CHECK(near(still.scale, 1) && near(still.pos.x, 0) && near(still.pos.y, 0) && near(still.rot, 0));
    // mix and fly start and end on their shots; mix zooms geometrically
    const cam::Shot p{ { 500, 500 }, 0.5f, 0 }, q{ { 4000, 2000 }, 8, 30 };
    for (float u : { 0.f, 1.f }) {
        const cam::Shot m1 = cam::Shot::mix(p, q, u), f1 = cam::Shot::fly(p, q, u, F);
        const cam::Shot& want1 = u == 0 ? p : q;
        CHECK(near(m1.center.x, want1.center.x, 0.05) && near(m1.zoom, want1.zoom) && near(m1.rot, want1.rot));
        CHECK(near(f1.center.x, want1.center.x, 0.05) && near(f1.center.y, want1.center.y, 0.05) && near(f1.zoom, want1.zoom, 1e-3));
    }
    CHECK(near(cam::Shot::mix(p, q, 0.5f).zoom, 2));
    // fly pulls back on a long move: halfway it is zoomed out further than either end
    CHECK(cam::Shot::fly({ { 0, 0 }, 1 }, { { 20000, 0 }, 1 }, 0.5f, F).zoom < 0.5f);
    // a track: holds, arrives on its keys, clamps past the end
    cam::Track tr;
    tr.start(p).hold(10).to(40, q).fly(80, p);
    CHECK(tr.end() == 80);
    CHECK(near(tr.at(5).zoom, p.zoom) && near(tr.at(40).zoom, q.zoom) && near(tr.at(500).center.x, p.center.x, 0.05));
    CHECK(tr.at(25).zoom > p.zoom && tr.at(25).zoom < q.zoom);
    // shake is deterministic and small
    const cam::Shot k1 = cam::shake(s, 12, 8), k2 = cam::shake(s, 12, 8);
    CHECK(k1.center.x == k2.center.x && k1.rot == k2.rot);
    const Vector2 moved = cam::toFrame(k1, s.center, F);
    CHECK(std::hypot(moved.x - 960, moved.y - 540) <= 8 * 1.01 + 5);   // the offset plus the small turn
}

int main() {
    views();
    meshes();
    fourD();
    hyperbolic();
    spherical();
    cameras();
    if (failures) { std::fprintf(stderr, "%d space/camera check(s) failed\n", failures); return 1; }
    std::printf("space and camera tests passed\n");
    return 0;
}
