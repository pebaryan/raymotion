// diagram_draw.cpp -- themes, drawing (shapes, BPMN markers, edges, groups, labels) and
// the Diagram script: reveals, highlights, traces, data flow, camera moves and morphs,
// all evaluated from the frame number alone.
#include "diagram_internal.hpp"
#include "rmtn/anim.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/fonts.hpp"
#include "rmtn/render.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace rmtn {
namespace dg {

// ------------------------------------------------------------------ themes
namespace {
std::vector<Color> hexes(std::initializer_list<unsigned> list) {
    std::vector<Color> out;
    for (unsigned h : list) out.push_back(mkColor(h));
    return out;
}
}

Theme Theme::light() {
    Theme t;
    t.palette = hexes({ 0xdbeafe, 0xdcfce7, 0xfef3c7, 0xfce7f3, 0xede9fe, 0xccfbf1, 0xffedd5, 0xe0e7ff, 0xf1f5f9 });
    return t;
}
Theme Theme::dark() {
    Theme t;
    t.fill = mkColor(0x1f2433); t.stroke = mkColor(0x8b93b0); t.text = mkColor(0xe8ebf5);
    t.edge = mkColor(0x8b93b0); t.edgeText = mkColor(0xc9cee0); t.edgeLabelFill = mkColor(0x151926, 235);
    t.groupFill = mkColor(0x171b27); t.groupStroke = mkColor(0x394058); t.groupText = mkColor(0xa7aec6);
    t.literalFill = mkColor(0x2c2a22); t.highlight = mkColor(0xffb020);
    t.palette = hexes({ 0x1e3a5f, 0x1f4d3a, 0x5a4214, 0x5b2140, 0x3d2f6b, 0x134e4a, 0x5c3317, 0x2e3a6e });
    return t;
}
Theme Theme::blueprint() {
    Theme t;
    t.fill = mkColor(0x1d5aa0, 120); t.stroke = mkColor(0xdce9ff); t.text = mkColor(0xf2f7ff);
    t.edge = mkColor(0xbcd4ff); t.edgeText = mkColor(0xdce9ff); t.edgeLabelFill = mkColor(0x14427a, 230);
    t.groupFill = mkColor(0x2a6ab4, 60); t.groupStroke = mkColor(0x9fc2f5); t.groupText = mkColor(0xcfe0fb);
    t.literalFill = mkColor(0x14427a, 160); t.highlight = mkColor(0xffd166);
    t.strokeWidth = 2; t.edgeWidth = 2; t.corner = 4;
    t.colorKinds = false;
    return t;
}
Theme Theme::bpmn() {
    Theme t;
    t.fill = WHITE; t.stroke = mkColor(0x22242a); t.text = mkColor(0x22242a);
    t.edge = mkColor(0x22242a); t.edgeText = mkColor(0x22242a); t.edgeLabelFill = mkColor(0xffffff, 0);
    t.groupFill = WHITE; t.groupStroke = mkColor(0x22242a); t.groupText = mkColor(0x22242a);
    t.literalFill = WHITE; t.highlight = mkColor(0x10ad73);
    t.fontSize = 13; t.padX = 8; t.padY = 6; t.maxLabelWidth = 100;
    t.strokeWidth = 2; t.edgeWidth = 1.6f; t.corner = 10; t.arrowSize = 11;
    t.colorKinds = false;
    return t;
}

namespace {
// ------------------------------------------------------------------ drawing primitives
using detail::outline;
using detail::pathLength;
using detail::pointAt;
using detail::samplePath;

Color fade(Color c, float a) { return withAlpha(c, std::clamp(a, 0.f, 1.f)); }

// A polyline as one miter-joined strip (no overlaps at gentle joints, so translucent
// strokes stay even). Width in diagram units.
void strokePath(const Ctx& c, const std::vector<Vector2>& pts, bool closed, float w, Color col) {
    const size_t n = pts.size();
    if (n < 2 || col.a == 0 || w <= 0) return;
    const Color k = dr::TA(c, col);
    if (k.a == 0) return;
    std::vector<Vector2> m(n);
    for (size_t i = 0; i < n; i++) m[i] = mapP(c, pts[i]);
    const float hw = std::max(0.35f, w * c.s * 0.5f);
    auto dirOf = [&](size_t a, size_t b) {
        float dx = m[b].x - m[a].x, dy = m[b].y - m[a].y;
        const float l = std::sqrt(dx * dx + dy * dy);
        return l > 1e-6f ? Vector2{ dx / l, dy / l } : Vector2{ 0, 0 };
    };
    std::vector<Vector2> L(n), R(n);
    for (size_t i = 0; i < n; i++) {
        Vector2 d0, d1;
        if (closed) { d0 = dirOf((i + n - 1) % n, i); d1 = dirOf(i, (i + 1) % n); }
        else { d0 = i > 0 ? dirOf(i - 1, i) : dirOf(0, 1); d1 = i + 1 < n ? dirOf(i, i + 1) : d0; }
        if (d0.x == 0 && d0.y == 0) d0 = d1;
        if (d1.x == 0 && d1.y == 0) d1 = d0;
        Vector2 nm{ -(d0.y + d1.y), d0.x + d1.x };
        float l = std::sqrt(nm.x * nm.x + nm.y * nm.y);
        if (l < 1e-6f) { nm = { -d0.y, d0.x }; l = 1; }
        nm = { nm.x / l, nm.y / l };
        const float cosHalf = nm.x * -d0.y + nm.y * d0.x;
        const float miter = hw / std::max(0.35f, cosHalf);
        L[i] = { m[i].x + nm.x * miter, m[i].y + nm.y * miter };
        R[i] = { m[i].x - nm.x * miter, m[i].y - nm.y * miter };
    }
    const size_t segs = closed ? n : n - 1;
    for (size_t i = 0; i < segs; i++) {
        const size_t j = (i + 1) % n;
        DrawTriangle(L[i], R[i], R[j], k);
        DrawTriangle(L[i], R[j], L[j], k);
    }
}

// Dashes along a polyline (lengths in diagram units); dotted uses round dots.
void strokeDashed(const Ctx& c, const std::vector<Vector2>& pts, float w, Color col, bool dotted, float scale) {
    const float total = pathLength(pts);
    if (total <= 0) return;
    const float dash = dotted ? 0.01f : 9 * scale, gap = dotted ? 5.5f * scale : 6 * scale;
    for (float s = 0; s < total; s += dash + gap) {
        if (dotted) { dr::circle(c, pointAt(pts, s), w * 0.62f, col); continue; }
        std::vector<Vector2> piece;
        const float e = std::min(total, s + dash);
        piece.push_back(pointAt(pts, s));
        // Include the corners inside the dash so it bends with the path.
        float acc = 0;
        for (size_t i = 1; i < pts.size(); i++) {
            acc += std::hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
            if (acc > s && acc < e) piece.push_back(pts[i]);
        }
        piece.push_back(pointAt(pts, e));
        strokePath(c, piece, false, w, col);
    }
}

void strokeStyled(const Ctx& c, const std::vector<Vector2>& pts, bool closed, float w, Color col, Stroke s) {
    if (s == Stroke::Solid) { strokePath(c, pts, closed, w, col); return; }
    std::vector<Vector2> p = pts;
    if (closed && !p.empty()) p.push_back(p.front());
    strokeDashed(c, p, w, col, s == Stroke::Dotted, std::max(1.f, w / 2.5f));
}

// The first `len` (diagram units) of a polyline.
std::vector<Vector2> prefix(const std::vector<Vector2>& pts, float len) {
    std::vector<Vector2> out;
    if (pts.empty()) return out;
    out.push_back(pts[0]);
    float acc = 0;
    for (size_t i = 1; i < pts.size(); i++) {
        const float l = std::hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
        if (acc + l >= len) {
            const float u = l > 1e-6f ? (len - acc) / l : 0;
            out.push_back({ pts[i - 1].x + (pts[i].x - pts[i - 1].x) * u, pts[i - 1].y + (pts[i].y - pts[i - 1].y) * u });
            return out;
        }
        acc += l;
        out.push_back(pts[i]);
    }
    return out;
}
std::vector<Vector2> reversed(std::vector<Vector2> p) { std::reverse(p.begin(), p.end()); return p; }

// Resamples a polyline to `n` points evenly spaced by arc length (for morphing routes).
std::vector<Vector2> resample(const std::vector<Vector2>& pts, int n) {
    std::vector<Vector2> out;
    const float total = pathLength(pts);
    for (int i = 0; i < n; i++) out.push_back(pointAt(pts, total * i / (n - 1)));
    return out;
}

Vector2 lerpV(Vector2 a, Vector2 b, float u) { return { a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u }; }

Color darker(Color c) {
    const Vector3 hsv = ColorToHSV(c);
    return ColorFromHSV(hsv.x, std::min(1.f, hsv.y * 2.2f + 0.25f), hsv.z * 0.55f);
}

struct Fonts { Font regular, bold; };

void textLines(const Ctx& c, const Font& f, const std::vector<std::string>& lines, Vector2 center, float size,
               float lineHeight, Color col, double anchorX = 0.5) {
    if (lines.empty() || col.a == 0) return;
    const float lh = size * lineHeight, top = center.y - lh * lines.size() / 2;
    for (size_t i = 0; i < lines.size(); i++)
        dr::text(f, c, lines[i].c_str(), { center.x, top + lh * i + (lh - size) / 2 }, size, 0, col, anchorX, 0);
}

// ------------------------------------------------------------------ BPMN and shape details
void envelope(const Ctx& c, Vector2 at, float w, float h, Color ink, Color paper, bool filled, float sw) {
    std::vector<Vector2> box{ { at.x - w / 2, at.y - h / 2 }, { at.x + w / 2, at.y - h / 2 }, { at.x + w / 2, at.y + h / 2 }, { at.x - w / 2, at.y + h / 2 } };
    dr::fillPoly(c, box.data(), 4, filled ? ink : paper);
    strokePath(c, box, true, sw, ink);
    strokePath(c, { box[0], { at.x, at.y + h * 0.05f }, box[1] }, false, sw, filled ? paper : ink);
}

void eventMarker(const Ctx& c, const Node& n, Color ink, Color paper, float sw) {
    const Vector2 p = n.pos;
    const float r = std::min(n.size.x, n.size.y) / 2;
    const bool filled = n.kind.find("throw") != std::string::npos;
    const std::string& m = n.marker;
    if (m == "message") { envelope(c, p, r * 1.05f, r * 0.72f, ink, paper, filled, sw * 0.6f); return; }
    if (m == "timer") {
        dr::circle(c, p, r * 0.62f, paper);
        dr::ring(c, p, r * 0.62f, sw * 0.7f, ink);
        for (int i = 0; i < 12; i++) {
            const float a = i * 30 * DEG2RAD;
            strokePath(c, { { p.x + std::cos(a) * r * 0.48f, p.y + std::sin(a) * r * 0.48f }, { p.x + std::cos(a) * r * 0.58f, p.y + std::sin(a) * r * 0.58f } }, false, sw * 0.4f, ink);
        }
        strokePath(c, { p, { p.x + r * 0.3f, p.y - r * 0.25f } }, false, sw * 0.55f, ink);
        strokePath(c, { p, { p.x - r * 0.05f, p.y - r * 0.42f } }, false, sw * 0.55f, ink);
        return;
    }
    if (m == "terminate") { dr::circle(c, p, r * 0.6f, ink); return; }
    std::vector<Vector2> poly;
    if (m == "error") poly = { { p.x - r * 0.45f, p.y + r * 0.42f }, { p.x - r * 0.18f, p.y - r * 0.4f }, { p.x + r * 0.08f, p.y + r * 0.05f },
                               { p.x + r * 0.45f, p.y - r * 0.42f }, { p.x + r * 0.18f, p.y + r * 0.4f }, { p.x - r * 0.08f, p.y - r * 0.05f } };
    else if (m == "signal") poly = { { p.x, p.y - r * 0.5f }, { p.x + r * 0.46f, p.y + r * 0.32f }, { p.x - r * 0.46f, p.y + r * 0.32f } };
    else if (m == "escalation") poly = { { p.x, p.y - r * 0.5f }, { p.x + r * 0.36f, p.y + r * 0.42f }, { p.x, p.y + r * 0.1f }, { p.x - r * 0.36f, p.y + r * 0.42f } };
    else if (m == "link") poly = { { p.x - r * 0.42f, p.y - r * 0.18f }, { p.x + r * 0.05f, p.y - r * 0.18f }, { p.x + r * 0.05f, p.y - r * 0.4f },
                                   { p.x + r * 0.45f, p.y }, { p.x + r * 0.05f, p.y + r * 0.4f }, { p.x + r * 0.05f, p.y + r * 0.18f }, { p.x - r * 0.42f, p.y + r * 0.18f } };
    else if (m == "compensation") {
        for (float dx : { -0.42f, 0.02f }) {
            std::vector<Vector2> tri{ { p.x + r * dx, p.y }, { p.x + r * (dx + 0.4f), p.y - r * 0.3f }, { p.x + r * (dx + 0.4f), p.y + r * 0.3f } };
            if (filled) dr::fillPoly(c, tri.data(), 3, ink);
            strokePath(c, tri, true, sw * 0.6f, ink);
        }
        return;
    } else if (m == "conditional") {
        std::vector<Vector2> box{ { p.x - r * 0.38f, p.y - r * 0.48f }, { p.x + r * 0.38f, p.y - r * 0.48f }, { p.x + r * 0.38f, p.y + r * 0.48f }, { p.x - r * 0.38f, p.y + r * 0.48f } };
        dr::fillPoly(c, box.data(), 4, paper);
        strokePath(c, box, true, sw * 0.55f, ink);
        for (int i = 0; i < 4; i++) {
            const float y = p.y - r * 0.3f + i * r * 0.2f;
            strokePath(c, { { p.x - r * 0.26f, y }, { p.x + r * 0.26f, y } }, false, sw * 0.45f, ink);
        }
        return;
    } else if (m == "cancel") {
        strokePath(c, { { p.x - r * 0.35f, p.y - r * 0.35f }, { p.x + r * 0.35f, p.y + r * 0.35f } }, false, sw * 1.4f, ink);
        strokePath(c, { { p.x + r * 0.35f, p.y - r * 0.35f }, { p.x - r * 0.35f, p.y + r * 0.35f } }, false, sw * 1.4f, ink);
        return;
    }
    if (poly.empty()) return;
    if (filled) dr::fillPoly(c, poly.data(), (int)poly.size(), ink);
    strokePath(c, poly, true, sw * 0.6f, ink);
}

void gatewayMarker(const Ctx& c, const Node& n, Color ink, Color paper, float sw) {
    const Vector2 p = n.pos;
    const float s = std::min(n.size.x, n.size.y) * 0.5f;
    const std::string& m = n.marker;
    const float w = sw * 1.9f;
    if (m == "exclusive") {
        strokePath(c, { { p.x - s * 0.3f, p.y - s * 0.3f }, { p.x + s * 0.3f, p.y + s * 0.3f } }, false, w, ink);
        strokePath(c, { { p.x + s * 0.3f, p.y - s * 0.3f }, { p.x - s * 0.3f, p.y + s * 0.3f } }, false, w, ink);
    } else if (m == "parallel" || m == "complex") {
        strokePath(c, { { p.x, p.y - s * 0.45f }, { p.x, p.y + s * 0.45f } }, false, w, ink);
        strokePath(c, { { p.x - s * 0.45f, p.y }, { p.x + s * 0.45f, p.y } }, false, w, ink);
        if (m == "complex") {
            strokePath(c, { { p.x - s * 0.32f, p.y - s * 0.32f }, { p.x + s * 0.32f, p.y + s * 0.32f } }, false, w, ink);
            strokePath(c, { { p.x + s * 0.32f, p.y - s * 0.32f }, { p.x - s * 0.32f, p.y + s * 0.32f } }, false, w, ink);
        }
    } else if (m == "inclusive") {
        dr::ring(c, p, s * 0.42f, w * 0.9f, ink);
    } else if (m == "event") {
        dr::ring(c, p, s * 0.5f, sw * 0.6f, ink);
        dr::ring(c, p, s * 0.42f, sw * 0.6f, ink);
        std::vector<Vector2> pent;
        for (int i = 0; i < 5; i++) {
            const float a = (-90 + i * 72) * DEG2RAD;
            pent.push_back({ p.x + std::cos(a) * s * 0.26f, p.y + std::sin(a) * s * 0.26f });
        }
        strokePath(c, pent, true, sw * 0.6f, ink);
    }
    (void)paper;
}

void taskMarker(const Ctx& c, const Node& n, Color ink, Color paper, float sw) {
    const std::string& m = n.marker;
    const Vector2 o{ n.pos.x - n.size.x / 2 + 15, n.pos.y - n.size.y / 2 + 15 };
    const float k = 9, lw = sw * 0.55f;
    if (m == "user") {
        dr::circle(c, { o.x, o.y - k * 0.35f }, k * 0.38f, paper);
        dr::ring(c, { o.x, o.y - k * 0.35f }, k * 0.38f, lw, ink);
        std::vector<Vector2> body;
        for (int i = 0; i <= 12; i++) {
            const float a = (float)PI + (float)PI * i / 12;
            body.push_back({ o.x + std::cos(a) * k * 0.8f, o.y + k * 0.75f + std::sin(a) * k * 0.65f });
        }
        strokePath(c, body, true, lw, ink);
    } else if (m == "service") {
        for (int i = 0; i < 8; i++) {
            const float a = i * 45 * DEG2RAD;
            strokePath(c, { { o.x + std::cos(a) * k * 0.45f, o.y + std::sin(a) * k * 0.45f }, { o.x + std::cos(a) * k * 0.85f, o.y + std::sin(a) * k * 0.85f } }, false, lw * 2.2f, ink);
        }
        dr::circle(c, o, k * 0.6f, ink);
        dr::circle(c, o, k * 0.28f, paper);
    } else if (m == "script") {
        std::vector<Vector2> page{ { o.x - k * 0.55f, o.y - k * 0.75f }, { o.x + k * 0.65f, o.y - k * 0.75f }, { o.x + k * 0.55f, o.y + k * 0.75f }, { o.x - k * 0.65f, o.y + k * 0.75f } };
        strokePath(c, page, true, lw, ink);
        for (int i = 0; i < 3; i++) {
            const float y = o.y - k * 0.35f + i * k * 0.35f;
            strokePath(c, { { o.x - k * 0.3f, y }, { o.x + k * 0.3f, y } }, false, lw, ink);
        }
    } else if (m == "send" || m == "receive") {
        envelope(c, o, k * 1.6f, k * 1.1f, ink, paper, m == "send", lw);
    } else if (m == "manual") {
        std::vector<Vector2> hand{ { o.x - k * 0.8f, o.y - k * 0.1f }, { o.x + k * 0.2f, o.y - k * 0.6f }, { o.x + k * 0.9f, o.y - k * 0.6f },
                                   { o.x + k * 0.9f, o.y + k * 0.6f }, { o.x - k * 0.8f, o.y + k * 0.6f } };
        strokePath(c, hand, true, lw, ink);
        for (int i = 0; i < 2; i++) strokePath(c, { { o.x + k * 0.1f, o.y - k * 0.2f + i * k * 0.35f }, { o.x + k * 0.9f, o.y - k * 0.2f + i * k * 0.35f } }, false, lw, ink);
    } else if (m == "businessRule") {
        std::vector<Vector2> box{ { o.x - k, o.y - k * 0.7f }, { o.x + k, o.y - k * 0.7f }, { o.x + k, o.y + k * 0.7f }, { o.x - k, o.y + k * 0.7f } };
        dr::fillPoly(c, box.data(), 4, paper);
        dr::rect(c, o.x - k, o.y - k * 0.7f, 2 * k, k * 0.45f, fade(ink, 0.35f));
        strokePath(c, box, true, lw, ink);
        strokePath(c, { { o.x - k, o.y - k * 0.25f }, { o.x + k, o.y - k * 0.25f } }, false, lw, ink);
        strokePath(c, { { o.x - k, o.y + k * 0.22f }, { o.x + k, o.y + k * 0.22f } }, false, lw, ink);
        strokePath(c, { { o.x - k * 0.4f, o.y - k * 0.25f }, { o.x - k * 0.4f, o.y + k * 0.7f } }, false, lw, ink);
    }
    if (m == "subProcess") {
        const Vector2 b{ n.pos.x, n.pos.y + n.size.y / 2 - 9 };
        std::vector<Vector2> box{ { b.x - 7, b.y - 7 }, { b.x + 7, b.y - 7 }, { b.x + 7, b.y + 7 }, { b.x - 7, b.y + 7 } };
        strokePath(c, box, true, lw, ink);
        strokePath(c, { { b.x - 4, b.y }, { b.x + 4, b.y } }, false, lw, ink);
        strokePath(c, { { b.x, b.y - 4 }, { b.x, b.y + 4 } }, false, lw, ink);
    }
}

// Arrowhead at `tip` pointing along `dir` (unit); size in diagram units.
void arrowhead(const Ctx& c, Arrow a, Vector2 tip, Vector2 dir, float size, float w, Color col, Color paper) {
    if (a == Arrow::None || col.a == 0) return;
    const Vector2 nrm{ -dir.y, dir.x };
    const Vector2 back{ tip.x - dir.x * size, tip.y - dir.y * size };
    switch (a) {
    case Arrow::Normal: {
        const Vector2 p[3] = { tip, { back.x + nrm.x * size * 0.42f, back.y + nrm.y * size * 0.42f }, { back.x - nrm.x * size * 0.42f, back.y - nrm.y * size * 0.42f } };
        dr::fillPoly(c, p, 3, col);
        break;
    }
    case Arrow::Open:
        strokePath(c, { { back.x + nrm.x * size * 0.45f, back.y + nrm.y * size * 0.45f }, tip,
                        { back.x - nrm.x * size * 0.45f, back.y - nrm.y * size * 0.45f } }, false, w, col);
        break;
    case Arrow::Circle: case Arrow::OpenCircle: {
        const float r = size * 0.36f;
        const Vector2 ctr{ tip.x - dir.x * r, tip.y - dir.y * r };
        if (a == Arrow::Circle) dr::circle(c, ctr, r, col);
        else { dr::circle(c, ctr, r, paper); dr::ring(c, ctr, r, w * 0.8f, col); }
        break;
    }
    case Arrow::Cross: {
        const Vector2 m{ tip.x - dir.x * size * 0.6f, tip.y - dir.y * size * 0.6f };
        const float s = size * 0.42f;
        strokePath(c, { { m.x + (nrm.x + dir.x) * s, m.y + (nrm.y + dir.y) * s }, { m.x - (nrm.x + dir.x) * s, m.y - (nrm.y + dir.y) * s } }, false, w, col);
        break;
    }
    case Arrow::Diamond: case Arrow::OpenDiamond: {
        const Vector2 mid{ tip.x - dir.x * size * 0.55f, tip.y - dir.y * size * 0.55f };
        const Vector2 p[4] = { tip, { mid.x + nrm.x * size * 0.32f, mid.y + nrm.y * size * 0.32f }, { tip.x - dir.x * size * 1.1f, tip.y - dir.y * size * 1.1f },
                               { mid.x - nrm.x * size * 0.32f, mid.y - nrm.y * size * 0.32f } };
        dr::fillPoly(c, p, 4, a == Arrow::Diamond ? col : paper);
        if (a == Arrow::OpenDiamond) strokePath(c, { p[0], p[1], p[2], p[3] }, true, w * 0.8f, col);
        break;
    }
    case Arrow::None: break;
    }
}
// How far an arrowhead eats into the line.
float arrowInset(Arrow a, float size) {
    switch (a) {
    case Arrow::Normal: return size * 0.85f;
    case Arrow::Circle: case Arrow::OpenCircle: return size * 0.72f;
    case Arrow::Diamond: case Arrow::OpenDiamond: return size * 1.1f;
    default: return 0;
    }
}
} // namespace

// ------------------------------------------------------------------ the script
struct Diagram::Action {
    enum Type { Reveal, Hide, Highlight, Unhighlight, Dim, Trace, Flow, Focus };
    Type type = Reveal;
    int at = 0, dur = 1;
    std::vector<std::string> ids;     // as given
    std::vector<std::string> keys;    // resolved: "n:id", "e:id", "g:id"
    bool resolved = false;
    Color color = BLANK;
    float zoom = 2;
    int count = 3;
};

struct Diagram::Impl {
    std::vector<Graph> states;
    std::vector<int> stateAt, stateDur;
    std::vector<Action> actions;
    bool ready = false;
    std::unordered_set<std::string> hiddenAtStart;
    std::vector<std::string> allKeys;
    std::map<std::string, size_t> kindColor;
};

Diagram::Diagram(Graph g, Theme th, LayoutOptions lo) : theme(std::move(th)), layoutOptions(lo), impl_(std::make_unique<Impl>()) {
    impl_->states.push_back(std::move(g));
    impl_->stateAt.push_back(0);
    impl_->stateDur.push_back(0);
}
Diagram::~Diagram() = default;
std::shared_ptr<Diagram> Diagram::make(Graph g, Theme th, LayoutOptions lo) { return std::make_shared<Diagram>(std::move(g), std::move(th), lo); }
int Diagram::states() const { return (int)impl_->states.size(); }

namespace {
Diagram::Action act(Diagram::Action::Type type, int at, int dur, std::vector<std::string> ids) {
    Diagram::Action a;
    a.type = type; a.at = at; a.dur = std::max(1, dur); a.ids = std::move(ids);
    return a;
}
}

Diagram& Diagram::build(int at, int stagger, int duration) {
    const Graph& g = impl_->states.back();
    // Breadth first from the sources along edge direction, then anything left.
    std::unordered_map<std::string, std::vector<std::string>> out;
    std::unordered_map<std::string, int> indeg;
    for (const Edge& e : g.edges) {
        if (e.kind == "invisible") continue;
        out[e.from].push_back(e.to);
        if (e.from != e.to) indeg[e.to]++;
    }
    std::vector<std::string> order;
    std::unordered_set<std::string> seen;
    auto bfs = [&](const std::string& s) {
        std::queue<std::string> q;
        q.push(s); seen.insert(s);
        while (!q.empty()) {
            const std::string v = q.front(); q.pop();
            order.push_back(v);
            for (const std::string& w : out[v]) if (g.node(w) && !seen.count(w)) { seen.insert(w); q.push(w); }
        }
    };
    for (const Node& n : g.nodes) if (!indeg[n.id] && !seen.count(n.id)) bfs(n.id);
    for (const Node& n : g.nodes) if (!seen.count(n.id)) bfs(n.id);
    std::unordered_map<std::string, int> when;
    for (size_t i = 0; i < order.size(); i++) {
        when[order[i]] = at + (int)i * stagger;
        Action a = act(Action::Reveal, when[order[i]], duration, { order[i] });
        a.keys = { "n:" + order[i] }; a.resolved = true;
        impl_->actions.push_back(a);
    }
    for (const Edge& e : g.edges) {
        if (e.kind == "invisible") continue;
        const int tu = when.count(e.from) ? when[e.from] : at, tv = when.count(e.to) ? when[e.to] : tu;
        int start = tu + duration / 3, end = std::max(start + duration, tv + duration / 3);
        if (tv < tu) { start = tu + duration / 3; end = start + duration; }
        Action a = act(Action::Reveal, start, end - start, { e.id });
        a.keys = { "e:" + e.id }; a.resolved = true;
        impl_->actions.push_back(a);
    }
    for (const Group& gr : g.groups) {
        int first = -1;
        for (const Node& n : g.nodes)
            if (g.inGroup(n.id, gr.id) && when.count(n.id)) first = first < 0 ? when[n.id] : std::min(first, when[n.id]);
        Action a = act(Action::Reveal, std::max(at, (first < 0 ? at : first) - duration / 2), duration, { gr.id });
        a.keys = { "g:" + gr.id }; a.resolved = true;
        impl_->actions.push_back(a);
    }
    return *this;
}
Diagram& Diagram::reveal(int at, int duration, std::vector<std::string> ids) { impl_->actions.push_back(act(Action::Reveal, at, duration, std::move(ids))); return *this; }
Diagram& Diagram::hide(int at, int duration, std::vector<std::string> ids) { impl_->actions.push_back(act(Action::Hide, at, duration, std::move(ids))); return *this; }
Diagram& Diagram::highlight(int at, int duration, std::vector<std::string> ids, Color color) {
    Action a = act(Action::Highlight, at, duration, std::move(ids));
    a.color = color;
    impl_->actions.push_back(a);
    return *this;
}
Diagram& Diagram::unhighlight(int at, int duration, std::vector<std::string> ids) { impl_->actions.push_back(act(Action::Unhighlight, at, duration, std::move(ids))); return *this; }
Diagram& Diagram::dim(int at, int duration, std::vector<std::string> keep) { impl_->actions.push_back(act(Action::Dim, at, duration, std::move(keep))); return *this; }
Diagram& Diagram::trace(int at, int duration, std::vector<std::string> path, Color color) {
    Action a = act(Action::Trace, at, duration, std::move(path));
    a.color = color;
    impl_->actions.push_back(a);
    return *this;
}
Diagram& Diagram::flow(int at, int duration, std::vector<std::string> edges, int count, Color color) {
    Action a = act(Action::Flow, at, duration, std::move(edges));
    a.count = std::max(1, count);
    a.color = color;
    impl_->actions.push_back(a);
    return *this;
}
Diagram& Diagram::focus(int at, int duration, std::vector<std::string> ids, float zoom) {
    Action a = act(Action::Focus, at, duration, std::move(ids));
    a.zoom = zoom;
    impl_->actions.push_back(a);
    return *this;
}
Diagram& Diagram::morph(int at, int duration, Graph next) {
    impl_->states.push_back(std::move(next));
    impl_->stateAt.push_back(at);
    impl_->stateDur.push_back(std::max(1, duration));
    impl_->ready = false;
    return *this;
}

namespace {
// Element keys named by `name` in graph g (see the header for the rules).
void resolveIn(const Graph& g, const std::string& name, bool flowEdges, std::vector<std::string>& out) {
    auto push = [&](const std::string& k) { if (std::find(out.begin(), out.end(), k) == out.end()) out.push_back(k); };
    bool found = false;
    if (g.node(name)) {
        if (flowEdges) { for (const Edge& e : g.edges) if (e.from == name) push("e:" + e.id); }
        else push("n:" + name);
        found = true;
    }
    if (g.edge(name)) { push("e:" + name); found = true; }
    if (g.group(name)) { push("g:" + name); found = true; }
    if (found) return;
    const size_t arrow = name.find("->");
    if (arrow != std::string::npos) {
        std::string a = detail::trim(name.substr(0, arrow)), b = detail::trim(name.substr(arrow + 2));
        auto idOf = [&](const std::string& s) {
            if (g.node(s)) return s;
            for (const Node& n : g.nodes) if (n.label == s) return n.id;
            return s;
        };
        a = idOf(a); b = idOf(b);
        for (const Edge& e : g.edges) if ((e.from == a && e.to == b) || (e.from == b && e.to == a)) push("e:" + e.id);
        return;
    }
    for (const Node& n : g.nodes)
        if (n.label == name) {
            if (flowEdges) { for (const Edge& e : g.edges) if (e.from == n.id) push("e:" + e.id); }
            else push("n:" + n.id);
            return;
        }
    for (const Group& gr : g.groups) if (gr.label == name) { push("g:" + gr.id); return; }
}

const Node* nodeByKey(const Graph& g, const std::string& key) { return key.size() > 2 && key[0] == 'n' ? g.node(std::string_view(key).substr(2)) : nullptr; }
}

void Diagram::prepare() {
    Impl& I = *impl_;
    if (I.ready) return;
    for (size_t i = 0; i < I.states.size(); i++) {
        sizeNodes(I.states[i], theme);
        layout(I.states[i], layoutOptions, i > 0 ? &I.states[i - 1] : nullptr);
    }
    std::stable_sort(I.actions.begin(), I.actions.end(), [](const Action& a, const Action& b) { return a.at < b.at; });
    for (Action& a : I.actions) {
        if (a.resolved) continue;
        a.keys.clear();
        for (const std::string& id : a.ids) {
            const size_t before = a.keys.size();
            for (const Graph& g : I.states) resolveIn(g, id, a.type == Action::Flow, a.keys);
            if (a.keys.size() == before && a.type != Action::Flow)
                throw std::runtime_error("diagram: no node, edge or group named \"" + id + "\"");
        }
        a.resolved = true;
    }
    I.allKeys.clear();
    std::unordered_set<std::string> keys;
    for (const Graph& g : I.states) {
        for (const Node& n : g.nodes) if (keys.insert("n:" + n.id).second) I.allKeys.push_back("n:" + n.id);
        for (const Edge& e : g.edges) if (keys.insert("e:" + e.id).second) I.allKeys.push_back("e:" + e.id);
        for (const Group& gr : g.groups) if (keys.insert("g:" + gr.id).second) I.allKeys.push_back("g:" + gr.id);
    }
    I.hiddenAtStart.clear();
    std::unordered_set<std::string> decided;
    for (const Action& a : I.actions) {
        if (a.type != Action::Reveal && a.type != Action::Hide) continue;
        for (const std::string& k : a.keys) {
            if (decided.insert(k).second && a.type == Action::Reveal) I.hiddenAtStart.insert(k);
        }
    }
    I.kindColor.clear();
    for (const Graph& g : I.states)
        for (const Node& n : g.nodes)
            if (!n.kind.empty() && n.kind[0] != '@' && !I.kindColor.count(n.kind)) {
                const size_t idx = I.kindColor.size();
                I.kindColor[n.kind] = idx;
            }
    I.ready = true;
}

const Graph& Diagram::graph(int state) {
    prepare();
    return impl_->states.at((size_t)state);
}

namespace {
struct ElemState {
    float vis = 1, hl = 0, sweep = 0, dim = 0, pop = 0;
    bool sweepBack = false;
    Color hlColor = BLANK;
};

// The graph at frame t, morphing between states; `presence` scales each element in or out.
Graph frameGraph(const std::vector<Graph>& states, const std::vector<int>& at, const std::vector<int>& dur, int t,
                 std::unordered_map<std::string, float>& presence) {
    size_t k = 0;
    while (k + 1 < states.size() && t >= at[k + 1]) k++;
    // Before state k+1 starts we are in state k; during [at, at + dur) of state k we morph into it.
    if (k == 0 || t >= at[k] + dur[k]) return states[k];
    const Graph& A = states[k - 1];
    const Graph& B = states[k];
    const float u = (float)ease::inOut(ease::cubic)(std::clamp((t - at[k]) / (float)dur[k], 0.f, 1.f));
    Graph g = B;
    g.nodes.clear(); g.edges.clear(); g.groups.clear();
    auto mixColor = [&](const std::optional<Color>& a, const std::optional<Color>& b) -> std::optional<Color> {
        if (a && b) return mixC(*a, *b, u);
        return u < 0.5f ? a : b;
    };
    auto mixStyle = [&](const Style& a, const Style& b) {
        Style s = u < 0.5f ? a : b;
        s.fill = mixColor(a.fill, b.fill); s.stroke = mixColor(a.stroke, b.stroke); s.text = mixColor(a.text, b.text);
        return s;
    };
    for (const Node& a : A.nodes) {
        const Node* b = B.node(a.id);
        Node n = (b && u >= 0.5f) ? *b : a;
        if (b) {
            n.pos = lerpV(a.pos, b->pos, u);
            n.size = lerpV(a.size, b->size, u);
            n.style = mixStyle(a.style, b->style);
            presence["n:" + a.id] = 1;
        } else presence["n:" + a.id] = 1 - u;
        g.nodes.push_back(std::move(n));
    }
    for (const Node& b : B.nodes) if (!A.node(b.id)) { g.nodes.push_back(b); presence["n:" + b.id] = u; }
    for (const Edge& a : A.edges) {
        const Edge* b = B.edge(a.id);
        Edge e = (b && u >= 0.5f) ? *b : a;
        if (b) {
            const auto pa = resample(samplePath(a.points, a.curved), 48), pb = resample(samplePath(b->points, b->curved), 48);
            e.points.clear();
            for (int i = 0; i < 48; i++) e.points.push_back(lerpV(pa[i], pb[i], u));
            e.curved = false;
            if (a.labelPos && b->labelPos) e.labelPos = lerpV(*a.labelPos, *b->labelPos, u);
            e.style = mixStyle(a.style, b->style);
            presence["e:" + a.id] = 1;
        } else presence["e:" + a.id] = 1 - u;
        g.edges.push_back(std::move(e));
    }
    for (const Edge& b : B.edges) if (!A.edge(b.id)) { g.edges.push_back(b); presence["e:" + b.id] = u; }
    for (const Group& a : A.groups) {
        const Group* b = B.group(a.id);
        Group gr = (b && u >= 0.5f) ? *b : a;
        if (b) {
            gr.rect = { a.rect.x + (b->rect.x - a.rect.x) * u, a.rect.y + (b->rect.y - a.rect.y) * u,
                        a.rect.width + (b->rect.width - a.rect.width) * u, a.rect.height + (b->rect.height - a.rect.height) * u };
            presence["g:" + a.id] = 1;
        } else presence["g:" + a.id] = 1 - u;
        g.groups.push_back(std::move(gr));
    }
    for (const Group& b : B.groups) if (!A.group(b.id)) { g.groups.push_back(b); presence["g:" + b.id] = u; }
    return g;
}

struct Pulse { Vector2 pos; Color color; float size, alpha; };
}

void Diagram::draw(const Ctx& c, int t, Rectangle box) {
    prepare();
    Impl& I = *impl_;
    if (box.width <= 0 || box.height <= 0) {
        const Vector2 cs = canvasSize();
        if (cs.x <= 0) return;
        // The composition, in the node's local space.
        box = { 0, 0, cs.x / std::max(1e-6f, c.s), cs.y / std::max(1e-6f, c.s) };
        box.x = -c.o.x / c.s; box.y = -c.o.y / c.s;
    }
    std::unordered_map<std::string, float> presence;
    const Graph g = frameGraph(I.states, I.stateAt, I.stateDur, t, presence);
    auto pres = [&](const std::string& k) { auto it = presence.find(k); return it == presence.end() ? 1.f : it->second; };

    // Sampled routes.
    std::unordered_map<std::string, std::vector<Vector2>> paths;
    for (const Edge& e : g.edges) paths[e.id] = samplePath(e.points, e.curved);

    // Element states from the script.
    std::unordered_map<std::string, ElemState> st;
    for (const std::string& k : I.allKeys) { ElemState s; s.vis = I.hiddenAtStart.count(k) ? 0.f : 1.f; st[k] = s; }
    std::vector<Pulse> pulses;
    const Easing eo = ease::out(ease::cubic), eio = ease::inOut(ease::cubic);
    auto camFit = [&](Rectangle r, float maxScale) {
        const float s = std::min({ box.width / std::max(1.f, r.width + 2 * margin), box.height / std::max(1.f, r.height + 2 * margin), maxScale });
        return std::pair<Vector2, float>{ { r.x + r.width / 2, r.y + r.height / 2 }, s };
    };
    const Rectangle all = g.bounds();
    auto cam = camFit(all, maxZoom);
    auto rectOf = [&](const std::vector<std::string>& keys) {
        Rectangle r{};
        bool any = false;
        auto grow = [&](Rectangle b) {
            if (!any) { r = b; any = true; return; }
            const float x0 = std::min(r.x, b.x), y0 = std::min(r.y, b.y);
            r = { x0, y0, std::max(r.x + r.width, b.x + b.width) - x0, std::max(r.y + r.height, b.y + b.height) - y0 };
        };
        for (const std::string& k : keys) {
            const std::string id = k.substr(2);
            if (k[0] == 'n') if (const Node* n = g.node(id)) {
                grow({ n->pos.x - n->size.x / 2, n->pos.y - n->size.y / 2, n->size.x, n->size.y });
                if (!n->label.empty() && n->labelPlace != LabelPlace::Inside) grow(detail::labelRect(*n));
            }
            if (k[0] == 'e') for (Vector2 p : paths[id]) grow({ p.x, p.y, 0, 0 });
            if (k[0] == 'g') if (const Group* gr = g.group(id)) grow(gr->rect);
        }
        return any ? r : all;
    };
    auto pathOfEdge = [&](const std::string& id) -> const std::vector<Vector2>& { return paths[id]; };
    for (const Action& a : I.actions) {
        if (t < a.at) break;
        const float u = std::clamp((t - a.at) / (float)a.dur, 0.f, 1.f);
        const Color col = a.color.a ? a.color : theme.highlight;
        switch (a.type) {
        case Action::Reveal: case Action::Hide: {
            const float target = a.type == Action::Reveal ? 1.f : 0.f;
            for (const std::string& k : a.keys) {
                ElemState& s = st[k];
                // Linear here; the drawing eases it per element type.
                s.vis = target > 0 ? s.vis + (1 - s.vis) * u : s.vis * (1 - u);
            }
            break;
        }
        case Action::Highlight:
            for (const std::string& k : a.keys) {
                ElemState& s = st[k];
                s.hl = std::max(s.hl, (float)eo(u));
                s.sweep = std::max(s.sweep, (float)eio(u));
                s.sweepBack = false;
                s.hlColor = col;
            }
            break;
        case Action::Unhighlight: {
            const std::vector<std::string>& keys = a.keys.empty() ? I.allKeys : a.keys;
            for (const std::string& k : keys) {
                ElemState& s = st[k];
                s.hl *= 1 - (float)eo(u);
                if (u >= 1) { s.hl = 0; s.sweep = 0; }
            }
            break;
        }
        case Action::Dim: {
            std::unordered_set<std::string> keep(a.keys.begin(), a.keys.end());
            if (!keep.empty())
                for (const Edge& e : g.edges)
                    if (keep.count("n:" + e.from) && keep.count("n:" + e.to)) keep.insert("e:" + e.id);
            for (const std::string& k : I.allKeys) {
                bool kept = keep.empty() || keep.count(k);
                if (!kept && k[0] == 'g') {                   // a group stays when something inside does
                    for (const std::string& kk : a.keys)
                        if (kk[0] == 'n' && g.inGroup(kk.substr(2), k.substr(2))) { kept = true; break; }
                }
                ElemState& s = st[k];
                s.dim += ((kept ? 0.f : 1.f) - s.dim) * (float)eo(u);
            }
            break;
        }
        case Action::Trace: {
            // Edges between consecutive path nodes, their lengths, and the pulse's distance.
            std::vector<std::string> nodes;
            for (const std::string& k : a.keys) if (k[0] == 'n') nodes.push_back(k.substr(2));
            struct Leg { std::string edge; bool back; float len; };
            std::vector<Leg> legs;
            float total = 0;
            for (size_t i = 0; i + 1 < nodes.size(); i++) {
                Leg leg{ "", false, 0 };
                for (const Edge& e : g.edges) {
                    if (e.from == nodes[i] && e.to == nodes[i + 1]) { leg = { e.id, false, 0 }; break; }
                    if (e.from == nodes[i + 1] && e.to == nodes[i] && leg.edge.empty()) leg = { e.id, true, 0 };
                }
                if (!leg.edge.empty()) leg.len = std::max(1.f, pathLength(pathOfEdge(leg.edge)));
                else {
                    const Node* p = g.node(nodes[i]);
                    const Node* q = g.node(nodes[i + 1]);
                    leg.len = p && q ? std::hypot(p->pos.x - q->pos.x, p->pos.y - q->pos.y) : 1;
                }
                legs.push_back(leg);
                total += leg.len;
            }
            const float D = total * (float)eio(u);
            float acc = 0;
            auto light = [&](const std::string& key, float since) {
                ElemState& s = st[key];
                s.hl = 1;
                s.hlColor = col;
                s.pop = std::max(s.pop, since >= 0 && since < 14 ? std::sin(since / 14 * (float)PI) : 0.f);
            };
            const float speed = total / std::max(1, a.dur);   // units per frame, to time node pops
            if (!nodes.empty()) light("n:" + nodes[0], (t - a.at) * 1.f);
            for (size_t i = 0; i < legs.size(); i++) {
                const Leg& L = legs[i];
                if (!L.edge.empty()) {
                    ElemState& s = st["e:" + L.edge];
                    const float f = std::clamp((D - acc) / L.len, 0.f, 1.f);
                    if (f > 0) { s.hl = 1; s.hlColor = col; s.sweep = std::max(s.sweep, f); s.sweepBack = L.back; }
                }
                acc += L.len;
                if (D >= acc) light("n:" + nodes[i + 1], speed > 0 ? (D - acc) / speed : 99.f);
            }
            if (u > 0 && u < 1 && !legs.empty()) {
                float d = D;
                for (const Leg& L : legs) {
                    if (d <= L.len) {
                        Vector2 p;
                        if (!L.edge.empty()) {
                            const auto& pts = pathOfEdge(L.edge);
                            p = pointAt(pts, L.back ? L.len - d : d);
                        } else {
                            const size_t i = &L - &legs[0];
                            const Node* A = g.node(nodes[i]);
                            const Node* B = g.node(nodes[i + 1]);
                            p = A && B ? lerpV(A->pos, B->pos, d / L.len) : Vector2{ 0, 0 };
                        }
                        pulses.push_back({ p, col, theme.edgeWidth * 3.2f, 1 });
                        break;
                    }
                    d -= L.len;
                }
            }
            break;
        }
        case Action::Flow: {
            if (u >= 1) break;
            const float envelope = std::min({ 1.f, (t - a.at) / 8.f, (a.at + a.dur - t) / 8.f });
            for (const std::string& k : a.keys) {
                if (k[0] != 'e') continue;
                const auto& pts = pathOfEdge(k.substr(2));
                const float len = pathLength(pts);
                if (len <= 0) continue;
                const float travel = std::max(24.f, len / 7.f);              // frames per traversal
                for (int j = 0; j < a.count; j++) {
                    float ph = (t - a.at) / travel + (float)j / a.count;
                    ph -= std::floor(ph);
                    pulses.push_back({ pointAt(pts, ph * len), col, theme.edgeWidth * 2.4f, envelope * std::min(1.f, std::min(ph, 1 - ph) * 8) });
                }
            }
            break;
        }
        case Action::Focus: {
            const auto target = a.keys.empty() ? camFit(all, maxZoom) : camFit(rectOf(a.keys), a.zoom);
            const float e = (float)eio(u);
            cam.first = lerpV(cam.first, target.first, e);
            cam.second = std::exp(std::log(cam.second) + (std::log(target.second) - std::log(cam.second)) * e);
            break;
        }
        }
    }

    // Diagram units -> the node's local space -> canvas.
    Ctx cc = c;
    const float s = cam.second;
    cc.o = mapP(c, { box.x + box.width / 2 - cam.first.x * s, box.y + box.height / 2 - cam.first.y * s });
    cc.s = c.s * s;

    const Font font = theme.font.texture.id ? theme.font : fonts::ui(false);
    const Font bold = theme.bold.texture.id ? theme.bold : fonts::ui(true);
    auto alphaOf = [&](const std::string& k) {
        const ElemState& e = st[k];
        return pres(k) * (1 - 0.82f * e.dim);
    };
    auto fillOf = [&](const Node& n) {
        if (n.style.fill) return *n.style.fill;
        if (n.kind == "@literal") return theme.literalFill;
        if (n.kind == "@class") return theme.groupFill;
        if (theme.colorKinds && !theme.palette.empty() && !n.kind.empty()) {
            auto it = I.kindColor.find(n.kind);
            if (it != I.kindColor.end()) return theme.palette[it->second % theme.palette.size()];
        }
        return theme.fill;
    };
    auto strokeOf = [&](const Node& n) {
        if (n.style.stroke) return *n.style.stroke;
        if (!n.style.fill && theme.colorKinds && !theme.palette.empty() && !n.kind.empty() && n.kind[0] != '@' && I.kindColor.count(n.kind))
            return darker(fillOf(n));
        return theme.stroke;
    };

    // ---- groups (outermost first)
    std::vector<const Group*> groups;
    for (const Group& gr : g.groups) groups.push_back(&gr);
    auto depth = [&](const Group* gr) { int d = 0; for (const Group* p = gr; p && !p->parent.empty() && d < 32; p = g.group(p->parent)) d++; return d; };
    std::stable_sort(groups.begin(), groups.end(), [&](const Group* a, const Group* b) { return depth(a) < depth(b); });
    for (const Group* gr : groups) {
        const std::string key = "g:" + gr->id;
        const ElemState& es = st[key];
        const float a = alphaOf(key) * (float)eo(es.vis);
        if (a <= 0.003f || gr->rect.width <= 0) continue;
        const Rectangle r = gr->rect;
        const Color fill = fade(gr->style.fill.value_or(theme.groupFill), a);
        const Color ink = fade(es.hl > 0 ? mixC(gr->style.stroke.value_or(theme.groupStroke), es.hlColor, es.hl) : gr->style.stroke.value_or(theme.groupStroke), a);
        const Color text = fade(gr->style.text.value_or(theme.groupText), a);
        const float sw = gr->style.strokeWidth.value_or(theme.strokeWidth * (gr->kind == Group::Kind::Cluster ? 0.8f : 0.75f)) * (1 + es.hl * 0.6f);
        if (gr->kind == Group::Kind::Cluster) {
            Node shape;
            shape.pos = { r.x + r.width / 2, r.y + r.height / 2 };
            shape.size = { r.width, r.height };
            shape.shape = Shape::Rounded;
            const auto o = outline(shape, theme.corner * 1.2f);
            if (fill.a) dr::fillPoly(cc, o.data(), (int)o.size(), fill);
            strokeStyled(cc, o, true, sw, ink, gr->style.dash.value_or(Stroke::Solid));
            if (!gr->label.empty())
                dr::text(bold, cc, gr->label.c_str(), { r.x + 14, r.y + 10 }, theme.fontSize * 0.86f, 0, text, 0, 0);
            continue;
        }
        // Pools and lanes: a box with a label band (vertical text when horizontal).
        const std::vector<Vector2> o{ { r.x, r.y }, { r.x + r.width, r.y }, { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } };
        if (fill.a) dr::fillPoly(cc, o.data(), 4, fill);
        strokePath(cc, o, true, sw, ink);
        const float band = gr->kind == Group::Kind::Pool ? 30.f : 30.f;
        if (gr->kind == Group::Kind::Pool) {
            if (gr->horizontal) strokePath(cc, { { r.x + band, r.y }, { r.x + band, r.y + r.height } }, false, sw, ink);
            else strokePath(cc, { { r.x, r.y + band }, { r.x + r.width, r.y + band } }, false, sw, ink);
        }
        if (!gr->label.empty()) {
            const float fs = theme.fontSize;
            if (gr->horizontal) {
                Ctx tc = cc;
                tc.o = mapP(cc, { r.x + band / 2, r.y + r.height / 2 });
                tc.rot = cc.rot - 90;
                dr::text(font, tc, gr->label.c_str(), { 0, 0 }, fs, 0, text, 0.5, 0.5);
            } else {
                dr::text(font, cc, gr->label.c_str(), { r.x + r.width / 2, r.y + band / 2 }, fs, 0, text, 0.5, 0.5);
            }
        }
    }

    // ---- edges
    for (const Edge& e : g.edges) {
        if (e.kind == "invisible") continue;
        const std::string key = "e:" + e.id;
        const ElemState& es = st[key];
        const float a = alphaOf(key);
        const auto& full = paths[e.id];
        if (a <= 0.003f || es.vis <= 0 || full.size() < 2) continue;
        const float reveal = (float)eio(es.vis);
        const float sw = e.style.strokeWidth.value_or(theme.edgeWidth) * e.width;
        const Color base = e.style.stroke.value_or(theme.edge);
        const float size = theme.arrowSize * std::max(1.f, std::sqrt(e.width));
        // Trim the line under arrowheads.
        const float total = pathLength(full);
        const float headIn = arrowInset(e.head, size), tailIn = arrowInset(e.tail, size);
        std::vector<Vector2> line = full;
        if (headIn > 0 && total > headIn * 1.5f) line = prefix(line, total - headIn);
        if (tailIn > 0 && total > tailIn * 1.5f) line = reversed(prefix(reversed(line), pathLength(line) - tailIn));
        const float lineLen = pathLength(line);
        const Stroke stroke = e.style.dash.value_or(e.stroke);
        const std::vector<Vector2> shown = reveal >= 1 ? line : prefix(line, lineLen * reveal);
        const Color ink = fade(base, a);
        strokeStyled(cc, shown, false, sw, ink, stroke);
        // Highlight overlay sweeping along the edge (from whichever end the trace entered).
        Color headInk = ink;
        if (es.hl > 0.003f && es.sweep > 0) {
            const Color hc = fade(es.hlColor, a * es.hl);
            const float lit = std::min(es.sweep, reveal) * lineLen;
            std::vector<Vector2> over = es.sweepBack ? reversed(prefix(reversed(line), lit)) : prefix(line, lit);
            strokePath(cc, over, false, sw * 3.2f, fade(hc, 0.22f));
            strokePath(cc, over, false, sw * 1.6f, hc);
            if (es.sweep >= 0.999f) headInk = hc;
        }
        const Color paper = theme.fill;
        if (reveal > 0.85f) {
            const float ha = (reveal - 0.85f) / 0.15f;
            Vector2 dir;
            const Vector2 tip = pointAt(full, total, &dir);
            arrowhead(cc, e.head, tip, dir, size, sw, fade(headInk, ha), fade(paper, a * ha));
        }
        if (e.tail != Arrow::None) {
            Vector2 dir;
            const std::vector<Vector2> back = reversed(full);
            const Vector2 tip = pointAt(back, total, &dir);
            arrowhead(cc, e.tail, tip, dir, size, sw, fade(ink, std::min(1.f, reveal * 3)), fade(paper, a));
        }
    }
    // ---- edge labels
    for (const Edge& e : g.edges) {
        if (e.label.empty() || !e.labelPos || e.kind == "invisible") continue;
        const std::string key = "e:" + e.id;
        const ElemState& es = st[key];
        const float a = alphaOf(key) * std::clamp((es.vis - 0.4f) / 0.6f, 0.f, 1.f);
        if (a <= 0.003f) continue;
        const Vector2 p = *e.labelPos;
        const Vector2 sz = e.labelSize;
        if (theme.edgeLabelFill.a) dr::roundRect(cc, p.x - sz.x / 2, p.y - sz.y / 2, sz.x, sz.y, std::min(sz.y / 2, 8.f), fade(theme.edgeLabelFill, a));
        const Color tc = es.hl > 0 ? mixC(e.style.text.value_or(theme.edgeText), es.hlColor, es.hl * 0.7f) : e.style.text.value_or(theme.edgeText);
        textLines(cc, font, e.labelLines, p, theme.fontSize * 0.82f, theme.lineHeight, fade(tc, a));
    }

    // ---- nodes
    for (const Node& n : g.nodes) {
        const std::string key = "n:" + n.id;
        const ElemState& es = st[key];
        const float a = alphaOf(key) * std::min(1.f, es.vis * 1.6f);
        if (a <= 0.003f) continue;
        // Pop in with a little overshoot; traced nodes bump as the pulse arrives.
        const float grow = es.vis >= 1 ? 1.f : (float)std::max(0.0, ease::out(ease::back(1.6))(es.vis));
        const float sc = std::max(0.f, grow * pres(key) + (1 - pres(key)) * 0.6f * (pres(key) > 0)) * (1 + 0.12f * es.pop);
        Ctx nc = cc;
        nc.o = mapP(cc, { n.pos.x * (1 - sc), n.pos.y * (1 - sc) });
        nc.s = cc.s * sc;
        Color fill = fillOf(n), ink = strokeOf(n), text = n.style.text.value_or(theme.text);
        if (es.hl > 0) {
            fill = mixC(fill, es.hlColor, 0.14f * es.hl);
            ink = mixC(ink, es.hlColor, es.hl);
        }
        float sw = n.style.strokeWidth.value_or(theme.strokeWidth) * (1 + 0.5f * es.hl);
        if (n.shape == Shape::Event && n.kind.rfind("endEvent", 0) == 0) sw *= 2.2f;
        if (n.marker == "call") sw *= 2.1f;
        const Stroke dash = n.style.dash.value_or(n.kind.find("nonInterrupting") != std::string::npos ? Stroke::Dashed : Stroke::Solid);
        const Color F = fade(fill, a), K = fade(ink, a), T = fade(text, a);
        const Color paper = fade(theme.fill.a ? theme.fill : WHITE, a);
        if (n.shape != Shape::Text && n.shape != Shape::Annotation) {
            const auto o = outline(n, n.shape == Shape::Task ? 10.f : theme.corner);
            if (es.hl > 0.003f) {                       // glow
                strokePath(nc, o, true, sw + 9, fade(es.hlColor, a * es.hl * 0.12f));
                strokePath(nc, o, true, sw + 4.5f, fade(es.hlColor, a * es.hl * 0.22f));
            }
            if (n.shape == Shape::Point || n.shape == Shape::StartState) dr::fillPoly(nc, o.data(), (int)o.size(), K);
            else {
                if (F.a) dr::fillPoly(nc, o.data(), (int)o.size(), F);
                strokeStyled(nc, o, true, sw, K, dash);
            }
        }
        // Shape details.
        const float w = n.size.x, h = n.size.y;
        const Vector2 p = n.pos;
        switch (n.shape) {
        case Shape::DoubleCircle: {
            Node inner = n;
            inner.size = { w - 10, h - 10 };
            strokePath(nc, outline(inner, 0), true, sw, K);
            break;
        }
        case Shape::EndState: {
            Node inner = n;
            inner.size = { w * 0.6f, h * 0.6f };
            const auto o = outline(inner, 0);
            dr::fillPoly(nc, o.data(), (int)o.size(), K);
            break;
        }
        case Shape::Subroutine:
            strokePath(nc, { { p.x - w / 2 + 9, p.y - h / 2 }, { p.x - w / 2 + 9, p.y + h / 2 } }, false, sw * 0.8f, K);
            strokePath(nc, { { p.x + w / 2 - 9, p.y - h / 2 }, { p.x + w / 2 - 9, p.y + h / 2 } }, false, sw * 0.8f, K);
            break;
        case Shape::Cylinder: case Shape::DataStore: {
            const float ry = std::min(h * 0.12f, w * 0.18f);
            std::vector<Vector2> lip;
            for (int i = 0; i <= 24; i++) {
                const float ang = (float)PI * i / 24;
                lip.push_back({ p.x - std::cos(ang) * w / 2, p.y - h / 2 + ry + std::sin(ang) * ry });
            }
            strokePath(nc, lip, false, sw * 0.9f, K);
            if (n.shape == Shape::DataStore)
                for (int k = 1; k <= 2; k++) {
                    std::vector<Vector2> ring;
                    for (Vector2 q : lip) ring.push_back({ q.x, q.y + k * ry * 0.7f });
                    strokePath(nc, ring, false, sw * 0.7f, K);
                }
            break;
        }
        case Shape::Note: case Shape::DataObject: {
            const float f = n.shape == Shape::DataObject ? w * 0.32f : std::min(w, h) * 0.22f;
            strokePath(nc, { { p.x + w / 2 - f, p.y - h / 2 }, { p.x + w / 2 - f, p.y - h / 2 + f }, { p.x + w / 2, p.y - h / 2 + f } }, false, sw * 0.8f, K);
            break;
        }
        case Shape::Annotation:
            strokePath(nc, { { p.x - w / 2 + 12, p.y - h / 2 }, { p.x - w / 2, p.y - h / 2 }, { p.x - w / 2, p.y + h / 2 }, { p.x - w / 2 + 12, p.y + h / 2 } }, false, sw * 0.8f, K);
            break;
        case Shape::Event: {
            if (n.kind.rfind("intermediate", 0) == 0 || n.kind.rfind("boundaryEvent", 0) == 0) {
                Node inner = n;
                inner.size = { w - 6, h - 6 };
                strokeStyled(nc, outline(inner, 0), true, sw * 0.7f, K, dash);
            }
            eventMarker(nc, n, K, paper, theme.strokeWidth);
            break;
        }
        case Shape::Gateway: gatewayMarker(nc, n, K, paper, theme.strokeWidth); break;
        case Shape::Task: taskMarker(nc, n, K, paper, theme.strokeWidth); break;
        default: break;
        }
        // Label.
        if (!n.labelLines.empty()) {
            const bool small = n.shape == Shape::Point || n.shape == Shape::Event || n.shape == Shape::Gateway ||
                               n.shape == Shape::DataObject || n.shape == Shape::DataStore;
            const float fs = theme.fontSize * (small ? 0.8f : 1.f);
            const Rectangle lr = detail::labelRect(n);
            if (n.labelAngle != 0 && n.labelPlace != LabelPlace::Inside) {
                const bool left = n.labelPlace == LabelPlace::Left;
                Ctx tc = nc;
                tc.o = mapP(nc, p);
                tc.rot = nc.rot + n.labelAngle;
                const float off = w / 2 + 8;
                textLines(tc, font, n.labelLines, { left ? -off : off, 0 }, fs, theme.lineHeight, T, left ? 1.0 : 0.0);
            } else {
                const double ax = n.labelPlace == LabelPlace::Right ? 0.0 : n.labelPlace == LabelPlace::Left ? 1.0 : 0.5;
                const Vector2 at{ ax == 0.0 ? lr.x : ax == 1.0 ? lr.x + lr.width : lr.x + lr.width / 2, lr.y + lr.height / 2 };
                textLines(nc, font, n.labelLines, at, fs, theme.lineHeight, T, ax);
            }
        }
    }

    // ---- pulses
    for (const Pulse& p : pulses) {
        if (p.alpha <= 0) continue;
        dr::circle(cc, p.pos, p.size * 2.2f, fade(p.color, p.alpha * 0.18f));
        dr::circle(cc, p.pos, p.size * 1.4f, fade(p.color, p.alpha * 0.35f));
        dr::circle(cc, p.pos, p.size * 0.8f, fade(p.color, p.alpha));
        dr::circle(cc, p.pos, p.size * 0.38f, fade(WHITE, p.alpha * 0.9f));
    }
}

Rectangle Diagram::nodeRect(const std::string& id, int t, Rectangle box) {
    prepare();
    Impl& I = *impl_;
    std::unordered_map<std::string, float> presence;
    const Graph g = frameGraph(I.states, I.stateAt, I.stateDur, t, presence);
    const Node* n = g.node(id);
    if (!n) for (const Node& m : g.nodes) if (m.label == id) { n = &m; break; }
    if (!n) throw std::runtime_error("diagram: no node named \"" + id + "\"");
    // The camera as draw() places it (fit, then focus actions).
    const Rectangle all = g.bounds();
    auto fit = [&](Rectangle r, float maxScale) {
        const float s = std::min({ box.width / std::max(1.f, r.width + 2 * margin), box.height / std::max(1.f, r.height + 2 * margin), maxScale });
        return std::pair<Vector2, float>{ { r.x + r.width / 2, r.y + r.height / 2 }, s };
    };
    auto cam = fit(all, maxZoom);
    for (const Action& a : I.actions) {
        if (t < a.at) break;
        if (a.type != Action::Focus) continue;
        const float u = std::clamp((t - a.at) / (float)a.dur, 0.f, 1.f);
        Rectangle r = all;
        if (!a.keys.empty()) {
            bool any = false;
            for (const std::string& k : a.keys) {
                const Node* m = nodeByKey(g, k);
                if (!m) continue;
                const Rectangle b{ m->pos.x - m->size.x / 2, m->pos.y - m->size.y / 2, m->size.x, m->size.y };
                if (!any) { r = b; any = true; continue; }
                const float x0 = std::min(r.x, b.x), y0 = std::min(r.y, b.y);
                r = { x0, y0, std::max(r.x + r.width, b.x + b.width) - x0, std::max(r.y + r.height, b.y + b.height) - y0 };
            }
        }
        const auto target = a.keys.empty() ? fit(all, maxZoom) : fit(r, a.zoom);
        const float e = (float)ease::inOut(ease::cubic)(u);
        cam.first = lerpV(cam.first, target.first, e);
        cam.second = std::exp(std::log(cam.second) + (std::log(target.second) - std::log(cam.second)) * e);
    }
    const float s = cam.second;
    const Vector2 o{ box.x + box.width / 2 - cam.first.x * s, box.y + box.height / 2 - cam.first.y * s };
    return { o.x + (n->pos.x - n->size.x / 2) * s, o.y + (n->pos.y - n->size.y / 2) * s, n->size.x * s, n->size.y * s };
}

} // namespace dg
} // namespace rmtn
