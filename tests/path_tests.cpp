// path_tests.cpp -- vg:: path geometry: SVG path data, arcs, lengths, points and angles,
// trimming, transforms, shapes, morphs and SVG drawings. Pure logic (no GL).
#include "rmtn/paths.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }
static bool nearV(Vector2 a, Vector2 b, float tol) { return std::hypot(a.x - b.x, a.y - b.y) <= tol; }
static Vector2 endOf(const vg::Path& p) {
    const vg::Contour& c = p.contours.back();
    return c.segs.empty() ? c.start : c.segs.back().to;
}

static void pathData() {
    vg::Path tri = vg::Path::parse("M0 0 L100 0 L100 100 Z");
    CHECK(tri.contours.size() == 1 && tri.contours[0].closed && tri.contours[0].segs.size() == 3);
    CHECK(near(tri.length(), 200 + 100 * std::sqrt(2.f), 0.01f));

    vg::Path box = vg::Path::parse("m10 10 h20 v20 h-20 z");
    const Rectangle b = box.bounds();
    CHECK(near(b.x, 10, 1e-3f) && near(b.y, 10, 1e-3f) && near(b.width, 20, 1e-3f) && near(b.height, 20, 1e-3f));
    CHECK(near(box.length(), 80, 1e-3f));

    vg::Path rep = vg::Path::parse("M0 0 10 0 10 10");                // implicit lines after M
    CHECK(rep.contours[0].segs.size() == 2 && near(rep.length(), 20, 1e-4f));

    vg::Path tight = vg::Path::parse("M0-1.5.5 1e1");                // numbers without separators
    CHECK(nearV(tight.contours[0].start, { 0, -1.5f }, 1e-6f) && nearV(endOf(tight), { 0.5f, 10 }, 1e-6f));

    // S reflects the previous control point: a symmetric S-curve.
    vg::Path s = vg::Path::parse("M0 0 C 0 50 100 50 100 0 S 200 -50 200 0");
    CHECK(nearV(s.contours[0].segs[1].c1, { 100, -50 }, 1e-4f));
    vg::Path t = vg::Path::parse("M0 0 Q 50 50 100 0 T 200 0");
    CHECK(nearV(t.contours[0].segs[1].c1, { 150, -50 }, 1e-4f));

    // After Z, drawing continues from the subpath's start.
    vg::Path two = vg::Path::parse("M0 0 h10 v10 z l 5 5");
    CHECK(two.contours.size() == 2 && nearV(two.contours[1].start, { 0, 0 }, 1e-6f) && nearV(endOf(two), { 5, 5 }, 1e-6f));

    for (const char* bad : { "M 0 0 L 10", "X 1 2", "10 10", "M0 0 Z 5", "M 0 0 A 1 1 0 2 0 5 5" }) {
        bool threw = false;
        try { vg::Path::parse(bad); } catch (const std::runtime_error&) { threw = true; }
        if (!threw) { std::fprintf(stderr, "  parse(\"%s\") did not throw\n", bad); failures++; }
    }
}

static void arcs() {
    const float circ = 2 * (float)PI * 50;
    vg::Path c = vg::Path::parse("M-50 0 A50 50 0 1 0 50 0 A50 50 0 1 0 -50 0");
    CHECK(near(c.length(), circ, circ * 0.001f));
    for (float u = 0; u < 1; u += 0.07f) CHECK(near(std::hypot(c.pointAt(u).x, c.pointAt(u).y), 50, 0.05f));
    vg::Path half = vg::Path::parse("M0 0 a25 25 0 1150 0");          // flags written together
    CHECK(near(half.length(), (float)PI * 25, 0.05f));
    CHECK(half.pointAt(0.5f).y < -24);                                 // sweep 1 bulges up on screen
    vg::Path small = vg::Path::parse("M0 0 A 1 1 0 0 1 100 0");       // radii too small: scaled up
    CHECK(near(small.length(), (float)PI * 50, 0.1f));
    vg::Path circle = vg::Path::circle({ 0, 0 }, 100);
    CHECK(near(circle.length(), 2 * (float)PI * 100, 0.2f));
    for (float u = 0; u < 1; u += 0.05f) CHECK(near(std::hypot(circle.pointAt(u).x, circle.pointAt(u).y), 100, 0.03f));
    vg::Path quarter = vg::Path::arc({ 0, 0 }, 10, 0, 90);
    CHECK(nearV(endOf(quarter), { 0, 10 }, 1e-4f) && near(quarter.length(), 5 * (float)PI, 0.01f));
}

static void measuring() {
    vg::Path l = vg::Path::line({ 0, 0 }, { 100, 0 });
    CHECK(nearV(l.pointAt(0.3f), { 30, 0 }, 1e-3f));
    CHECK(near(vg::Path::line({ 0, 0 }, { 0, 10 }).angleAt(0.5f), 90, 1e-3f));
    vg::Path mid = l.trim(0.25f, 0.75f);
    CHECK(nearV(mid.contours[0].start, { 25, 0 }, 1e-3f) && nearV(endOf(mid), { 75, 0 }, 1e-3f) && near(mid.length(), 50, 1e-3f));
    CHECK(l.trim(0.5f, 0.5f).empty() && l.trim(0.7f, 0.2f).empty());

    vg::Path sq = vg::Path::rect({ 0, 0, 100, 100 });
    vg::Path h = sq.trim(0, 0.5f);
    CHECK(!h.contours[0].closed && near(h.length(), 200, 1e-2f) && nearV(endOf(h), { 100, 100 }, 1e-2f));
    CHECK(sq.trim(0, 1).contours[0].closed);

    // Curves: the pieces add up, meet, and end where pointAt says.
    vg::Path cur = vg::Path::parse("M0 0 C 0 120 200 -120 200 0 Q 300 100 400 0");
    const float L = cur.length();
    const vg::Path a = cur.trim(0, 0.37f), b = cur.trim(0.37f, 1);
    CHECK(near(a.length() + b.length(), L, L * 1e-4f));
    CHECK(nearV(endOf(a), b.contours[0].start, 0.01f));
    CHECK(nearV(endOf(a), cur.pointAt(0.37f), 0.01f));
    CHECK(near(a.length(), 0.37f * L, L * 1e-3f));

    // Several contours: in order, or each by the same fraction.
    vg::Path rings = vg::Path::circle({ 0, 0 }, 10).append(vg::Path::circle({ 100, 0 }, 20));
    const float rl = rings.length();
    CHECK(rings.trim(0, 0.2f).contours.size() == 1);
    const vg::Path each = rings.trim(0, 0.5f, true);
    CHECK(each.contours.size() == 2 && near(each.length(), rl / 2, rl * 1e-3f));

    vg::Path r = cur.reversed();
    CHECK(near(r.length(), L, L * 1e-4f) && nearV(r.contours[0].start, { 400, 0 }, 1e-4f) && nearV(endOf(r), { 0, 0 }, 1e-4f));
    CHECK(nearV(r.pointAt(0.3f), cur.pointAt(0.7f), 0.05f));
}

static void shapesAndTransforms() {
    vg::Path rr = vg::Path::rect({ 0, 0, 200, 100 }, 20);
    CHECK(near(rr.length(), 2 * (200 + 100) - 8 * 20 + 2 * (float)PI * 20, 0.1f));
    vg::Path st = vg::Path::star({ 0, 0 }, 100, 40, 5);
    CHECK(st.contours[0].segs.size() == 10 && nearV(st.contours[0].start, { 0, -100 }, 1e-3f));
    const std::vector<Vector2> pts{ { 0, 0 }, { 50, 40 }, { 100, -20 }, { 150, 30 } };
    vg::Path sm = vg::Path::smooth(pts);
    CHECK(sm.contours[0].segs.size() == 3);
    for (size_t i = 1; i < pts.size(); i++) CHECK(nearV(sm.contours[0].segs[i - 1].to, pts[i], 1e-5f));
    vg::Path rot = vg::Path::line({ 10, 0 }, { 20, 0 }).rotated(90);
    CHECK(nearV(rot.contours[0].start, { 0, 10 }, 1e-4f));
    vg::Path sc = vg::Path::circle({ 0, 0 }, 10).scaled(2, 2).translated({ 5, 5 });
    const Rectangle b = sc.bounds();
    CHECK(near(b.x, -15, 0.01f) && near(b.width, 40, 0.02f));
    // Flattening stays within tolerance of the true circle.
    for (const vg::Polyline& pl : vg::Path::circle({ 0, 0 }, 100).flatten(0.5f))
        for (size_t i = 0; i < pl.pts.size(); i++) {
            const Vector2 m{ (pl.pts[i].x + pl.pts[(i + 1) % pl.pts.size()].x) / 2, (pl.pts[i].y + pl.pts[(i + 1) % pl.pts.size()].y) / 2 };
            CHECK(std::hypot(m.x, m.y) > 100 - 0.55f);
        }
}

static void morphs() {
    const vg::Path square = vg::Path::rect({ -50, -50, 100, 100 });
    const vg::Path circle = vg::Path::circle({ 0, 0 }, 60);
    vg::Morph m(square, circle, 64);
    const vg::Path a = m.at(0), b = m.at(1);
    CHECK(a.contours.size() == 1 && b.contours.size() == 1 && a.contours[0].closed);
    for (const vg::Polyline& pl : a.flatten())
        for (Vector2 v : pl.pts) CHECK(near(std::max(std::fabs(v.x), std::fabs(v.y)), 50, 0.05f));
    for (const vg::Polyline& pl : b.flatten())
        for (Vector2 v : pl.pts) CHECK(near(std::hypot(v.x, v.y), 60, 0.2f));
    // Points travel little: square corners go to the nearest part of the circle.
    for (const vg::Polyline& pl : m.at(0.5f).flatten())
        for (Vector2 v : pl.pts) CHECK(std::hypot(v.x, v.y) > 45 && std::hypot(v.x, v.y) < 68);
    // An extra contour grows from a point.
    vg::Morph grow(circle, vg::Path(circle).append(vg::Path::circle({ 200, 0 }, 10)), 32);
    const vg::Path g0 = grow.at(0);
    CHECK(g0.contours.size() == 2);
    vg::Path extra;
    extra.contours = { g0.contours[1] };
    const Rectangle gb = extra.bounds();
    CHECK(gb.width < 1e-3f && near(gb.x, 200, 0.5f));
    const vg::Style s = vg::mix(vg::Style::filled(RED), vg::Style::filled(BLUE), 0.5f);
    CHECK(std::abs(s.fill.r - (RED.r + BLUE.r) / 2) <= 1);
}

static void drawings() {
    const vg::Drawing d = vg::Drawing::parse(R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 100" width="200" height="100">
      <defs><linearGradient id="g"><stop offset="0" stop-color="#ff0000"/><stop offset="1" stop-color="#0000ff"/></linearGradient></defs>
      <rect id="box" x="10" y="10" width="80" height="40" fill="#336699"/>
      <circle id="dot" cx="150" cy="50" r="30" fill="none" stroke="#ff8800" stroke-width="6" stroke-dasharray="4 2" stroke-linecap="round"/>
      <path id="donut" fill-rule="evenodd" fill="url(#g)" d="M0 0 h50 v50 h-50 z M10 10 h30 v30 h-30 z"/>
      <rect id="hidden" x="0" y="0" width="5" height="5" fill="black" opacity="0.5"/>
    </svg>)svg");
    CHECK(near(d.width, 200, 1e-3f) && near(d.height, 100, 1e-3f));
    CHECK(d.shapes.size() == 4);
    const vg::Shape* box = d.find("box");
    CHECK(box && box->style.fill.r == 0x33 && box->style.fill.g == 0x66 && box->style.fill.b == 0x99 && box->style.fill.a == 255);
    CHECK(box && box->style.stroke.width == 0);
    const vg::Shape* dot = d.find("dot");
    CHECK(dot && dot->style.fill.a == 0 && near(dot->style.stroke.width, 6, 1e-3f) && dot->style.stroke.cap == vg::Cap::Round);
    CHECK(dot && dot->style.stroke.dash.size() == 2 && near(dot->path.length(), 2 * (float)PI * 30, 0.2f));
    const vg::Shape* donut = d.find("donut");
    CHECK(donut && donut->style.rule == vg::FillRule::EvenOdd && donut->path.contours.size() == 2);
    CHECK(donut && donut->style.fill.r > 100 && donut->style.fill.b > 100);   // gradient: average of its stops
    const vg::Shape* half = d.find("hidden");
    CHECK(half && half->style.fill.a > 120 && half->style.fill.a < 135);
    CHECK(!d.find("nope"));
}

int main() {
    pathData();
    arcs();
    measuring();
    shapesAndTransforms();
    morphs();
    drawings();
    if (failures) { std::fprintf(stderr, "%d path check(s) failed\n", failures); return 1; }
    std::printf("path tests passed\n");
    return 0;
}
