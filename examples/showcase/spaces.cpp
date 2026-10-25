// spaces.cpp -- "spaces": space::, from a number line to curved and four-dimensional space.
//   dimensions  a point on a number line gains a y and then a z as the view tips into 3D;
//               its vector and the vector's shadow on the floor
//   projection  a helix and its shadows; seen from above it is a circle, from the side a wave
//   surface     z = f(x, y) rises out of the plane, coloured by height
//   gluing      a sheet rolls into a tube and the tube bends into a torus
//   moebius     a strip joins with a half twist; the Klein bottle turns
//   sphere      a triangle with three right angles, and stereographic projection to the plane
//   hyperbolic  the {7,3} tiling of the Poincare disk moves by an isometry, then opens out
//               into the upper half-plane
//   tesseract   a hypercube turning in four dimensions, projected to three, then to the screen
// The score (100 bpm on Dm-Bb-F-C) is synthesised into out/ on first use.
//   raymotion render spaces
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <cstdio>
#include <filesystem>

namespace rmtn {
namespace {

constexpr int XF = 20;
const std::string SCORE = "out/spaces-assets-v1/score.wav";
const Color BG = mkColor(0x14171f), INK = mkColor(0xf4f1ea), MUTED = mkColor(0x8a90a2);
const Color SKY = mkColor(0x58c4dd), SUN = mkColor(0xffd166), CORAL = mkColor(0xfc6255), MINT = mkColor(0x83c167),
            ORCHID = mkColor(0xc38fd8), TEAL = mkColor(0x5cd0b3);

Font bold() { return fonts::ui(true); }
Font mono() {
    static bool missing = false;
    if (!missing) {
        try { return fonts::load({ "consola.ttf", "CascadiaMono.ttf", "DejaVuSansMono.ttf", "Menlo.ttc" }); }
        catch (const std::exception&) { missing = true; }
    }
    return fonts::ui(false);
}

float ramp(int t, double a, double b, Easing e = ease::inOut(ease::cubic)) {
    return (float)interpolate(t, { a, b }, { 0, 1 }, clamped(e));
}

void backdrop(TNode* n, int len) {
    n->add(0, len)->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1920, 1080, BG); };
}

void caption(TNode* n, int len, const char* title, const char* code) {
    TNode* card = n->add(12, len - 12);
    card->up = [len](int t, NodeProps& p) {
        const float in = ramp(t, 0, 18, ease::out(ease::cubic)), out = 1 - ramp(t, len - 40.0, len - 22.0, ease::linear);
        p.opacity = in * out;
        p.pos = { 0, 20 * (1 - in) };
    };
    const std::string tt = title, cc = code;
    card->fn = [tt, cc](const Ctx& cx, int) {
        const float w = std::max(MeasureTextEx(mono(), cc.c_str(), 26, 0).x, MeasureTextEx(bold(), tt.c_str(), 44, 0).x) + 72;
        dr::roundRect(cx, 60, 1080 - 56 - 132, w, 132, 16, mkColor(0x222737, 235));
        dr::text(bold(), cx, tt.c_str(), { 96, 1080 - 56 - 112 }, 44, 0, INK, 0, 0);
        dr::text(mono(), cx, cc.c_str(), { 96, 1080 - 56 - 50 }, 26, 0, SUN, 0, 0);
    };
}

std::string fixed(double v, int d) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.*f", d, v);
    return buf;
}

// ------------------------------------------------------------------ scenes

void dimensionsScene(TNode* n, int len) {
    backdrop(n, len);
    n->add(0, len)->fn = [](const Ctx& cx, int t) {
        const float gx = ramp(t, 0, 30), gy = ramp(t, 60, 95), k3 = ramp(t, 120, 175);
        const float lift = ramp(t, 140, 185), vec = ramp(t, 200, 230), shadow = ramp(t, 225, 255);
        space::View v = space::View::mix(space::View::flat({ 960, 470 }, 120), space::View::tilted({ 960, 520 }, 105), k3);
        v.yaw += 14 * ramp(t, 170, 300, ease::linear);
        space::Scene s(v);
        space::Axes3 a;
        a.x = plot::Range(-4, 4, 1);
        a.y = plot::Range(-3, 3, 1);
        a.z = plot::Range(-1, 3, 1);
        space::Axes3Style st;
        st.gridXY = true;
        space::axes(s, a, st, { gx, gy, k3 });
        const Vector3 p{ 2, 1.5f * ramp(t, 70, 100), 2 * lift };
        const float pin = ramp(t, 25, 40);
        if (pin > 0) {
            space::guides(s, p, SUN, gy > 0 ? (lift > 0 ? 3 : 2) : 1);
            s.point(p, withAlpha(SUN, pin), 11);
            const std::string tex = lift > 0.02f ? "(2,\\ 1.5,\\ " + fixed(p.z, 1) + ")" : gy > 0.02f ? "(2,\\ " + fixed(p.y, 1) + ")" : "x = 2";
            s.label(p, tex, 40, withAlpha(INK, pin), { 0, 1 }, { 16, -12 });
        }
        if (vec > 0) {
            s.arrow({ 0, 0, 0 }, space::lerp({ 0, 0, 0 }, p, vec), CORAL, 5);
            s.label(space::lerp({ 0, 0, 0 }, p, 0.55f), R"(\vec v)", 40, withAlpha(CORAL, vec), { 1, 1 }, { -12, -6 });
        }
        if (shadow > 0) {
            const Vector3 foot = space::projectOnPlane(p, { 0, 0, 0 }, { 0, 0, 1 });
            s.arrow({ 0, 0, 0 }, space::lerp({ 0, 0, 0 }, foot, shadow), withAlpha(MINT, 0.9f), 5);
            s.label(foot, R"(\operatorname{proj}_{xy}\vec v)", 36, withAlpha(MINT, shadow), { 0, 0 }, { 14, 10 });
        }
        s.draw(cx);
    };
    caption(n, len, "From 1D to 2D to 3D", "space::View::mix(View::flat(), View::tilted(), k)   space::axes(s, ax, style, { 1, 1, k })");
}

void projectionScene(TNode* n, int len) {
    backdrop(n, len);
    std::vector<Vector3> helix;
    for (int i = 0; i <= 240; i++) {
        const float th = i / 240.f * 4 * 3.14159265f;
        helix.push_back({ 1.6f * std::cos(th), 1.6f * std::sin(th), -2 + 4 * i / 240.f });
    }
    n->add(0, len)->fn = [helix](const Ctx& cx, int t) {
        const float draw = ramp(t, 0, 60), shade = ramp(t, 50, 80), top = ramp(t, 95, 135), side = ramp(t, 160, 200);
        space::View tilt = space::View::tilted({ 960, 470 }, 95, -35, -66, 0.3f);
        space::View above = space::View::flat({ 960, 450 }, 120);
        space::View front = space::View::tilted({ 960, 450 }, 110, 0, -90, 0);
        space::View v = space::View::mix(space::View::mix(tilt, above, top), front, side);
        space::Scene s(v);
        space::Axes3 a;
        a.x = plot::Range(-3, 3, 1);
        a.y = plot::Range(-3, 3, 1);
        a.z = plot::Range(-3, 3, 1);
        space::Axes3Style st;
        st.numbers = false;
        st.color = withAlpha(MUTED, 0.9f);
        space::axes(s, a, st);
        const size_t shown = (size_t)(draw * (helix.size() - 1)) + 1;
        std::vector<Vector3> part(helix.begin(), helix.begin() + shown);
        // shadows: on the floor (a circle) and on the back wall (a wave)
        std::vector<Vector3> floor, wall;
        for (Vector3 p : part) {
            floor.push_back(space::projectOnPlane(p, { 0, 0, -3 }, { 0, 0, 1 }));
            wall.push_back(space::projectOnPlane(p, { 0, 3, 0 }, { 0, 1, 0 }));
        }
        if (shade > 0) {
            s.curve(floor, withAlpha(MINT, shade), 4);
            s.curve(wall, withAlpha(ORCHID, shade), 4);
            vg::Stroke ray(withAlpha(MUTED, 0.5f * shade * (1 - top)), 1.5f);
            ray.dash = { 6, 6 };
            for (size_t i = 0; i < part.size(); i += 12) s.line(part[i], floor[i], ray);
        }
        s.curve(part, SKY, 6);
        if (!part.empty()) s.point(part.back(), SUN, 10);
        s.draw(cx);
        plot::label(cx, R"(\text{from above: } (\cos\theta,\ \sin\theta))", { 1500, 160 }, { 0.5f, 0.5f }, 40, withAlpha(MINT, top * (1 - side)));
        plot::label(cx, R"(\text{from the side: } (\cos\theta,\ z))", { 1500, 160 }, { 0.5f, 0.5f }, 40, withAlpha(ORCHID, side));
    };
    caption(n, len, "Projection is a point of view", "space::projectOnPlane(p, point, normal)   View::mix(tilted, flat, k)");
}

void surfaceScene(TNode* n, int len) {
    backdrop(n, len);
    const space::Mesh m = space::Mesh::graph([](double x, double y) {
        return 1.6 * std::sin(x) * std::cos(y * 0.8) * std::exp(-(x * x + y * y) / 30);
    }, { -4, 4 }, { -4, 4 }, 44, 44);
    n->add(0, len)->fn = [m, len](const Ctx& cx, int t) {
        const float rise = ramp(t, 20, 90);
        space::View v = space::View::tilted({ 960, 500 }, 82, -40 + 50 * ramp(t, 0, len, ease::linear), -60, 0.12f);
        space::Scene s(v);
        space::Axes3 a;
        a.x = plot::Range(-4, 4, 2);
        a.y = plot::Range(-4, 4, 2);
        a.z = plot::Range(-2, 2, 1);
        space::Axes3Style st;
        st.numbers = false;
        space::axes(s, a, st);
        space::MeshStyle ms;
        ms.colorAt = [rise](Vector3 p) { return plot::viridis(0.5f + p.z / (3.2f * std::max(rise, 0.05f))); };
        ms.wireColor = { 255, 255, 255, 50 };
        s.mesh(m.mapped([rise](Vector3 p) { return Vector3{ p.x, p.y, p.z * rise }; }), ms);
        s.draw(cx);
        plot::label(cx, R"(z = 1.6\,\sin x\,\cos 0.8y\; e^{-(x^2+y^2)/30})", { 1460, 140 }, { 0.5f, 0.5f }, 40, withAlpha(INK, ramp(t, 60, 80)));
    };
    caption(n, len, "Surfaces", "space::Mesh::graph(f, xRange, yRange)   scene.mesh(m, { .colorAt = height })");
}

void glueScene(TNode* n, int len) {
    backdrop(n, len);
    n->add(0, len)->fn = [len](const Ctx& cx, int t) {
        const float roll = ramp(t, 40, 100), bend = ramp(t, 120, 190);
        const space::Mesh m = space::surfaces::glueTorus(roll, bend, 3.6f, 9, 48, 28);
        space::View v = space::View::tilted({ 960, 470 }, 92 + 36 * bend, -30 + 40 * ramp(t, 0, len, ease::linear), -62, 0.3f);
        space::Scene s(v);
        space::MeshStyle ms;
        ms.cellColor = [](int r, int c) { return ((r / 4 + c / 4) % 2) ? mkColor(0x3c6e8f) : mkColor(0x58c4dd); };
        ms.back = mkColor(0x8f6a9c);
        ms.wire = 0;
        s.mesh(m, ms);
        // the edges that meet: left and right (coral) close the tube, top and bottom (sun) the torus
        std::vector<Vector3> left, right, top, bottom;
        for (int r = 0; r < m.rows; r++) { left.push_back(m.at(r, 0)); right.push_back(m.at(r, m.cols - 1)); }
        for (int c = 0; c < m.cols; c++) { top.push_back(m.at(0, c)); bottom.push_back(m.at(m.rows - 1, c)); }
        const float fade = 1 - ramp(t, 200, 230);
        s.curve(left, withAlpha(CORAL, fade), 6);
        s.curve(right, withAlpha(CORAL, fade), 6);
        s.curve(top, withAlpha(SUN, fade), 6);
        s.curve(bottom, withAlpha(SUN, fade), 6);
        s.draw(cx);
        const char* stage = bend > 0.5f ? R"(\text{a torus: } S^1 \times S^1)" : roll > 0.5f ? R"(\text{a cylinder: } S^1 \times [0,1])"
                                                                               : R"(\text{a square: } [0,1]^2)";
        plot::label(cx, stage, { 1480, 150 }, { 0.5f, 0.5f }, 42, INK);
    };
    caption(n, len, "Gluing a torus", "space::surfaces::glueTorus(roll, bend)");
}

void moebiusKleinScene(TNode* n, int len) {
    backdrop(n, len);
    const space::Mesh klein = space::surfaces::klein(48, 64);
    n->add(0, len)->fn = [klein, len](const Ctx& cx, int t) {
        const float join = ramp(t, 20, 110);
        space::Scene a(space::View::tilted({ 520, 470 }, 52, -25 + 30 * ramp(t, 0, len, ease::linear), -55, 0.3f));
        space::MeshStyle ms;
        ms.color = CORAL;
        ms.back = SUN;
        ms.wireColor = { 255, 255, 255, 60 };
        ms.wireEvery = 4;
        a.mesh(space::surfaces::moebius(join, 12, 2.2f, 1, 12, 96), ms);
        a.draw(cx);
        space::Scene b(space::View::tilted({ 1400, 480 }, 105, 20 + 120 * ramp(t, 0, len, ease::linear), -70, 0.3f));
        space::MeshStyle ks;
        ks.color = TEAL;
        ks.back = ORCHID;
        ks.opacity = 0.92f;
        ks.wireColor = { 255, 255, 255, 45 };
        ks.wireEvery = 3;
        b.mesh(klein, ks);
        b.draw(cx);
        plot::label(cx, R"(\text{Möbius band})", { 520, 130 }, { 0.5f, 0.5f }, 42, withAlpha(INK, ramp(t, 100, 120)));
        plot::label(cx, R"(\text{Klein bottle})", { 1400, 130 }, { 0.5f, 0.5f }, 42, withAlpha(INK, ramp(t, 10, 30)));
    };
    caption(n, len, "One-sided surfaces", "space::surfaces::moebius(join)   space::surfaces::klein()");
}

void sphereScene(TNode* n, int len) {
    backdrop(n, len);
    const space::Mesh ball = space::surfaces::sphere(2, 24, 48);
    n->add(0, len)->fn = [ball, len](const Ctx& cx, int t) {
        const float tri = ramp(t, 20, 70), lift = ramp(t, 90, 115), proj = ramp(t, 120, 170);
        space::View v = space::View::tilted({ 960, 430 + 40 * lift }, 105 - 25 * lift, -30 + 30 * ramp(t, 0, len, ease::linear), -66, 0.3f);
        v.target = { 0, 0, 1.2f * lift };
        space::Scene s(v);
        if (lift > 0) s.plane({ 0, 0, 0 }, { 0, 0, 1 }, 10, withAlpha(mkColor(0x222a3a), lift), 10, { 255, 255, 255, (unsigned char)(40 * lift) });
        const Vector3 lifted{ 0, 0, 2 * lift };
        space::MeshStyle ms;
        ms.color = mkColor(0x2c3550);
        ms.opacity = 0.85f - 0.5f * proj;
        ms.wireColor = { 255, 255, 255, 40 };
        ms.wireEvery = 2;
        s.mesh(ball.mapped([lifted](Vector3 p) { return Vector3{ p.x, p.y, p.z + lifted.z }; }), ms);
        // three mutually perpendicular directions: every angle of the triangle is a right angle
        // ... then the sphere turns over so the triangle sits underneath, where its projection
        // from the north pole stays close (from up top it would fly off to infinity)
        const float flip = 3.14159265f * lift, cf = std::cos(flip), sf = std::sin(flip);
        auto turn = [cf, sf](Vector3 p) { return Vector3{ p.x, p.y * cf - p.z * sf, p.y * sf + p.z * cf }; };
        const Vector3 corners[3] = { turn({ 4 / 3.f, 4 / 3.f, 2 / 3.f }), turn({ -4 / 3.f, 2 / 3.f, 4 / 3.f }), turn({ 2 / 3.f, -4 / 3.f, 4 / 3.f }) };
        auto up = [lifted](std::vector<Vector3> pts) { for (Vector3& p : pts) p.z += lifted.z; return pts; };
        for (int i = 0; i < 3; i++) {
            std::vector<Vector3> arc = space::sphere::arc(corners[i], corners[(i + 1) % 3], 2, 48);
            arc.resize((size_t)(tri * 48) + 1);
            s.curve(up(arc), CORAL, 6);
        }
        for (Vector3 c : corners) s.point({ c.x, c.y, c.z + lifted.z }, withAlpha(SUN, tri), 10);
        if (proj > 0) {
            // stereographic: rays from the north pole through the triangle to the plane
            const Vector3 north{ 0, 0, 2 + lifted.z };
            vg::Stroke ray(withAlpha(MUTED, 0.6f * proj), 1.6f);
            ray.dash = { 6, 6 };
            for (int i = 0; i < 3; i++) {
                const std::vector<Vector3> arc = space::sphere::arc(corners[i], corners[(i + 1) % 3], 2, 48);
                std::vector<Vector3> flat;
                for (size_t k = 0; k < arc.size(); k++) {
                    // project from the lifted sphere: its centre sits at z = lifted.z, the plane at z = 0
                    Vector3 q = space::sphere::stereographic({ arc[k].x, arc[k].y, arc[k].z }, 2);
                    // the sphere is lifted by 2 (its south pole touches the plane): scale from the pole by 4/2
                    q = { q.x * 2, q.y * 2, 0 };
                    if (k % 12 == 0) s.line(north, q, ray);
                    flat.push_back(q);
                }
                const size_t shown = (size_t)(proj * (flat.size() - 1)) + 1;
                flat.resize(shown);
                s.curve(flat, withAlpha(MINT, proj), 5);
            }
            s.point(north, SUN, 9);
        }
        s.draw(cx);
        plot::label(cx, R"(\alpha + \beta + \gamma = 270^\circ)", { 1500, 150 }, { 0.5f, 0.5f }, 44, withAlpha(CORAL, ramp(t, 60, 80) * (1 - lift)));
        plot::label(cx, R"(\text{stereographic projection: circles stay circles})", { 1440, 150 }, { 0.5f, 0.5f }, 38, withAlpha(MINT, proj));
    };
    caption(n, len, "Spherical geometry", "space::sphere::arc(a, b)   space::sphere::stereographic(p)");
}

void hyperbolicScene(TNode* n, int len) {
    backdrop(n, len);
    using space::hyperbolic::C;
    const std::vector<space::hyperbolic::Tile> tiles = space::hyperbolic::tiling(7, 3, 900, 0.004);
    n->add(0, len)->fn = [tiles](const Ctx& cx, int t) {
        namespace hy = space::hyperbolic;
        const float grow = ramp(t, 0, 50), move = ramp(t, 50, 130), open = ramp(t, 150, 210);
        const hy::Disk disk{ { 960, 470 }, 380 };
        const hy::HalfPlane half{ { 960, 820 }, 230 };
        const C a = std::polar(0.55 * move, 0.6);
        const Rectangle clip{ 0, 0, 1920, 860 };
        auto map = [&](C z) {
            const Vector2 d = disk.map(z);
            if (open <= 0) return d;
            const Vector2 h = half.map(hy::toHalfPlane(z));
            return Vector2{ d.x + (h.x - d.x) * open, d.y + (h.y - d.y) * open };
        };
        if (open < 1) vg::fill(cx, vg::Path::circle(disk.center, disk.radius), withAlpha(mkColor(0x1e2433), 1 - open));
        const Color c0 = mkColor(0x58c4dd), c1 = mkColor(0x2d4a63);
        for (const hy::Tile& tl : tiles) {
            if (tl.ring > grow * 12) continue;
            std::vector<C> v;
            for (C z : tl.vertices) v.push_back(hy::isometry(z, a, 0));
            const vg::Path p = hy::path(hy::polygon(v, 16), map, true, clip);
            if (p.empty()) continue;
            vg::fill(cx, p, tl.parity ? c1 : c0);
            vg::stroke(cx, p, vg::Stroke(mkColor(0x14171f), 1.5f));
        }
        if (open < 1) vg::stroke(cx, vg::Path::circle(disk.center, disk.radius), vg::Stroke(withAlpha(INK, 1 - open), 3));
        if (open > 0) vg::stroke(cx, vg::Path::line({ 0, half.origin.y }, { 1920, half.origin.y }), vg::Stroke(withAlpha(INK, open), 3));
        plot::label(cx, R"(\{7,3\}\ \text{in the Poincaré disk})", { 1600, 120 }, { 0.5f, 0.5f }, 40, withAlpha(INK, grow * (1 - open)));
        plot::label(cx, R"(w = i\,\frac{1+z}{1-z})", { 1640, 120 }, { 0.5f, 0.5f }, 44, withAlpha(SUN, open));
    };
    caption(n, len, "Hyperbolic geometry", "space::hyperbolic::tiling(7, 3)   isometry(z, a)   toHalfPlane(z)");
}

void tesseractScene(TNode* n, int len) {
    backdrop(n, len);
    const space::Polytope4 cube = space::tesseract(1);
    n->add(0, len)->fn = [cube, len](const Ctx& cx, int t) {
        const float in = ramp(t, 0, 40);
        const double xw = 160 * ramp(t, 20, len, ease::linear), yz = 50 * ramp(t, 0, len, ease::linear);
        std::vector<Vector3> p;
        std::vector<double> w;
        for (space::Vec4 v : cube.vertices) {
            v = space::rotate4(space::rotate4(v, 0, 3, xw), 1, 2, yz);
            w.push_back(v.w);
            p.push_back(space::project4(v, 3));
        }
        space::Scene s(space::View::tilted({ 960, 460 }, 150, -30, -68, 0.3f));
        for (auto e : cube.edges) {
            const float depth = (float)((w[(size_t)e.first] + w[(size_t)e.second]) / 2 + 1) / 2;   // 0 far in w .. 1 near
            s.line(p[(size_t)e.first], p[(size_t)e.second], withAlpha(mixC(ORCHID, SKY, depth), in), 3 + 3 * depth);
        }
        for (size_t i = 0; i < p.size(); i++) s.point(p[i], withAlpha(mixC(ORCHID, SUN, (float)(w[i] + 1) / 2), in), 7 + 4 * (float)(w[i] + 1) / 2);
        s.draw(cx);
        plot::label(cx, R"(\mathbb{R}^4 \to \mathbb{R}^3 \to \mathbb{R}^2)", { 1560, 140 }, { 0.5f, 0.5f }, 46, withAlpha(INK, ramp(t, 30, 50)));
    };
    caption(n, len, "Four dimensions", "space::rotate4(v, x, w, deg)   space::project4(v, distance)   space::tesseract()");
}

// The score, 100 bpm on Dm-Bb-F-C: a soft pad and arpeggio, a bell at each chapter, a lift
// into the hyperbolic plane.
void genScore(const std::string& path, double end, const std::vector<double>& chapters) {
    using procgen::Score;
    Score s(end);
    struct Chord { int root; int pad[3]; int arp[4]; };
    const Chord dm{ 50, { 62, 65, 69 }, { 74, 77, 81, 77 } }, bb{ 46, { 58, 62, 65 }, { 70, 74, 77, 74 } },
                f{ 41, { 60, 65, 69 }, { 72, 77, 81, 77 } }, c{ 48, { 60, 64, 67 }, { 72, 76, 79, 76 } };
    const Chord prog[4] = { dm, bb, f, c };
    const double bar = 4 * 60.0 / 100, beat = bar / 4, stop = end - 2.5;
    int i = 0;
    for (double t = 0; t < stop; t += bar, i++) {
        const Chord& ch = prog[i % 4];
        const bool drums = t >= chapters[0], full = t >= chapters[2];
        for (int p : ch.pad) s.pad(t, bar * 0.95, p, 0.08f, full ? 1600 : 1000);
        if (t >= bar)
            for (int e = 0; e < 8; e++) s.pluck(t + e * beat / 2, ch.arp[e % 4], e % 4 == 0 ? 0.055f : 0.032f, e % 2 ? 0.3f : 0.7f);
        for (int e = 0; e < 4; e++) s.bass(t + e * beat, beat * 0.85, ch.root, drums ? 0.22f : 0.15f);
        if (drums) { s.kick(t, 0.45f); s.kick(t + 2 * beat, 0.4f); }
        if (full) { s.clap(t + beat, 0.15f); s.clap(t + 3 * beat, 0.15f); }
        if (t >= bar * 2) for (int h = 0; h < 8; h++) s.hat(t + h * beat / 2, h % 2 ? 0.07f : 0.04f, h % 2 ? 0.35f : 0.65f);
    }
    for (size_t k = 0; k < chapters.size(); k++) s.bell(chapters[k], 74 + (int)(k % 3) * 3, 0.09f, 0.5f);
    if (chapters.size() > 5) { s.riser(chapters[5] - 1.4, chapters[5], 0.07f); s.impact(chapters[5], 0.14f); }
    for (int p : dm.pad) { s.pad(stop, 2.6, p, 0.12f, 2000, 0.35f); s.pad(stop, 2.6, p - 12, 0.08f, 1200, 0.65f); }
    s.bass(stop, 2.6, dm.root - 12, 0.22f);
    for (int k = 0; k < 4; k++) s.bell(stop + k * 0.4, dm.arp[k] + 12, 0.08f, 0.2f + 0.2f * k);
    media::writeWav(path, s.mix(), Score::RATE);
}

} // namespace

void registerSpaces(Registry& out) {
    Comp c;
    c.name = "spaces";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    struct S { int len; void (*make)(TNode*, int); };
    const S scenes[] = { { 300, dimensionsScene }, { 230, projectionScene }, { 170, surfaceScene }, { 240, glueScene },
                         { 190, moebiusKleinScene }, { 200, sphereScene }, { 240, hyperbolicScene }, { 180, tesseractScene } };
    std::vector<double> chapters;
    bool first = true;
    for (const S& s : scenes) {
        if (!first) { ts.transition(Transition::fade(XF)); chapters.push_back((ts.end() - XF) / 30.0); }
        first = false;
        s.make(ts.add(s.len), s.len);
    }
    c.duration = ts.end();
    const double seconds = c.duration / 30.0;
    c.assets = [seconds, chapters](const Comp&, const Json&) {
        if (std::filesystem::exists(SCORE)) return;
        std::filesystem::create_directories(std::filesystem::path(SCORE).parent_path());
        genScore(SCORE, seconds, chapters);
    };
    media::audio(addRoot(c.roots, 0, c.duration), SCORE);
    out.push_back(std::move(c));
}

} // namespace rmtn
