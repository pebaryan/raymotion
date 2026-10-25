// camera.cpp -- cam:: shots, smooth zooms, tracks and camera-driven world nodes (see
// rmtn/camera.hpp).
#include "rmtn/camera.hpp"
#include <algorithm>
#include <cmath>
#include <complex>

namespace rmtn {
namespace cam {
namespace {

using Cx = std::complex<double>;
constexpr double kDeg = 0.017453292519943295;

Vector2 mid(Vector2 frame) { return { frame.x / 2, frame.y / 2 }; }
Cx cx(Vector2 v) { return { v.x, v.y }; }
Vector2 v2(Cx c) { return { (float)c.real(), (float)c.imag() }; }
// Canvas -> frame is f(p) = mid + A (p - centre) with A = zoom * e^{-i rot} (y down, so a
// positive rot turns the view clockwise and the canvas the other way).
Cx scaleRot(const Shot& s) { return std::polar((double)s.zoom, -s.rot * kDeg); }

} // namespace

Shot Shot::fit(Rectangle r, Vector2 frame, float margin, float rot) {
    Shot s;
    s.center = { r.x + r.width / 2, r.y + r.height / 2 };
    const float room = std::max(0.f, 1 - 2 * margin);
    const float zx = r.width > 0 ? frame.x * room / r.width : 1e9f, zy = r.height > 0 ? frame.y * room / r.height : 1e9f;
    s.zoom = std::min(zx, zy);
    if (s.zoom >= 1e9f) s.zoom = 1;
    s.rot = rot;
    return s;
}

Shot Shot::mix(const Shot& a, const Shot& b, float u) {
    // a spiral similarity: the one canvas point that sits at the same frame spot in both
    // shots stays put, the zoom is geometric and the turn linear
    Shot s;
    s.zoom = (float)(std::pow((double)a.zoom, 1.0 - u) * std::pow((double)b.zoom, (double)u));
    s.rot = a.rot + (b.rot - a.rot) * u;
    const Cx A = scaleRot(a), B = scaleRot(b), S = scaleRot(s);
    if (std::abs(A - B) < 1e-6 * std::abs(A)) {
        s.center = { a.center.x + (b.center.x - a.center.x) * u, a.center.y + (b.center.y - a.center.y) * u };
        return s;
    }
    const Cx P = (A * cx(a.center) - B * cx(b.center)) / (A - B);
    s.center = v2(P - A * (P - cx(a.center)) / S);
    return s;
}

Shot Shot::fly(const Shot& a, const Shot& b, float u, Vector2 frame, float rho) {
    // van Wijk & Nuij, "Smooth and efficient zooming and panning" (as d3.interpolateZoom),
    // in terms of the visible width w = frame width / zoom
    const double r = std::max((double)rho, 1e-3), r2 = r * r, r4 = r2 * r2;
    const double w0 = frame.x / std::max(a.zoom, 1e-6f), w1 = frame.x / std::max(b.zoom, 1e-6f);
    const double dx = (double)b.center.x - a.center.x, dy = (double)b.center.y - a.center.y, d2 = dx * dx + dy * dy;
    Shot s;
    s.rot = a.rot + (b.rot - a.rot) * u;
    double f, w;
    if (d2 < 1e-12) {
        const double S = std::log(w1 / w0) / r;
        f = u;
        w = w0 * std::exp(r * u * S);
    } else {
        const double d1 = std::sqrt(d2);
        const double b0 = (w1 * w1 - w0 * w0 + r4 * d2) / (2 * w0 * r2 * d1);
        const double b1 = (w1 * w1 - w0 * w0 - r4 * d2) / (2 * w1 * r2 * d1);
        const double r0 = std::log(std::sqrt(b0 * b0 + 1) - b0), rr1 = std::log(std::sqrt(b1 * b1 + 1) - b1);
        const double S = (rr1 - r0) / r, sv = u * S, ch = std::cosh(r0);
        f = w0 / (r2 * d1) * (ch * std::tanh(r * sv + r0) - std::sinh(r0));
        w = w0 * ch / std::cosh(r * sv + r0);
    }
    if (u >= 1) { f = 1; w = w1; }
    s.center = { (float)(a.center.x + f * dx), (float)(a.center.y + f * dy) };
    s.zoom = (float)(frame.x / w);
    return s;
}

Vector2 toFrame(const Shot& s, Vector2 p, Vector2 frame) {
    return v2(cx(mid(frame)) + scaleRot(s) * (cx(p) - cx(s.center)));
}

Vector2 toCanvas(const Shot& s, Vector2 f, Vector2 frame) {
    return v2(cx(s.center) + (cx(f) - cx(mid(frame))) / scaleRot(s));
}

Rectangle visible(const Shot& s, Vector2 frame) {
    const Vector2 c[4] = { toCanvas(s, { 0, 0 }, frame), toCanvas(s, { frame.x, 0 }, frame), toCanvas(s, frame, frame),
                           toCanvas(s, { 0, frame.y }, frame) };
    float x0 = c[0].x, x1 = c[0].x, y0 = c[0].y, y1 = c[0].y;
    for (const Vector2& p : c) { x0 = std::min(x0, p.x); x1 = std::max(x1, p.x); y0 = std::min(y0, p.y); y1 = std::max(y1, p.y); }
    return { x0, y0, x1 - x0, y1 - y0 };
}

NodeProps props(const Shot& shot, Vector2 frame, float parallax) {
    Shot s = shot;
    if (parallax != 1) {
        const Vector2 m = mid(frame);
        s.center = { m.x + (shot.center.x - m.x) * parallax, m.y + (shot.center.y - m.y) * parallax };
        s.zoom = (float)std::pow((double)std::max(shot.zoom, 1e-6f), (double)parallax);
        s.rot = shot.rot * parallax;
    }
    // the node maps p to pos + zoom * R(-rot) p; the shot's centre must land mid-frame
    NodeProps p;
    p.scale = s.zoom;
    p.rot = -s.rot;
    p.pos = v2(cx(mid(frame)) - scaleRot(s) * cx(s.center));
    return p;
}

Shot shake(Shot s, int t, float amount, float freq, unsigned seed) {
    auto phase = [seed](unsigned k) {
        unsigned h = seed * 2654435761u + k * 40503u;
        h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
        return (h & 0xffff) / 65536.0 * 6.283185307179586;
    };
    const double w = 6.283185307179586 * freq * t / 30.0;
    const double ox = amount * (0.6 * std::sin(w + phase(1)) + 0.4 * std::sin(2.31 * w + phase(2)));
    const double oy = amount * (0.6 * std::sin(1.13 * w + phase(3)) + 0.4 * std::sin(2.71 * w + phase(4)));
    // a frame-px offset, in canvas units
    const Cx off = Cx(ox, oy) / scaleRot(s);
    s.center = { s.center.x + (float)off.real(), s.center.y + (float)off.imag() };
    s.rot += (float)(amount / 8 * std::sin(0.87 * w + phase(5)));
    return s;
}

// ------------------------------------------------------------------ Track

Track& Track::start(const Shot& s) {
    keys_.clear();
    keys_.push_back({ 0, s, ease::linear, false, 1.414f });
    return *this;
}

Track& Track::to(int f, const Shot& s, Easing e) {
    if (keys_.empty()) keys_.push_back({ 0, s, ease::linear, false, 1.414f });
    keys_.push_back({ std::max(f, keys_.back().frame), s, std::move(e), false, 1.414f });
    return *this;
}

Track& Track::fly(int f, const Shot& s, Easing e, float rho) {
    if (keys_.empty()) keys_.push_back({ 0, s, ease::linear, false, rho });
    keys_.push_back({ std::max(f, keys_.back().frame), s, std::move(e), true, rho });
    return *this;
}

Track& Track::hold(int f) {
    if (keys_.empty()) keys_.push_back({ 0, Shot{}, ease::linear, false, 1.414f });
    const Shot last = keys_.back().shot;
    keys_.push_back({ std::max(f, keys_.back().frame), last, ease::linear, false, 1.414f });
    return *this;
}

Shot Track::at(double t) const {
    if (keys_.empty()) return Shot{};
    if (t <= keys_.front().frame) return keys_.front().shot;
    for (size_t i = 1; i < keys_.size(); i++) {
        const Key& a = keys_[i - 1];
        const Key& b = keys_[i];
        if (t > b.frame) continue;
        if (b.frame == a.frame) return b.shot;
        double u = (t - a.frame) / (double)(b.frame - a.frame);
        if (b.ease) u = b.ease(u);
        return b.fly ? Shot::fly(a.shot, b.shot, (float)u, frame, b.rho) : Shot::mix(a.shot, b.shot, (float)u);
    }
    return keys_.back().shot;
}

// ------------------------------------------------------------------ world nodes

namespace {
TNode* follow(TNode* n, std::function<Shot(int)> shot, Vector2 frame, float parallax) {
    n->up = [shot = std::move(shot), frame, parallax](int t, NodeProps& p) {
        const NodeProps q = props(shot(t), frame, parallax);
        p.pos = q.pos;
        p.scale = q.scale;
        p.rot = q.rot;
    };
    return n;
}
} // namespace

TNode* world(Nodes& list, int from, int duration, std::function<Shot(int)> shot, Vector2 frame, float parallax) {
    return follow(addRoot(list, from, duration), std::move(shot), frame, parallax);
}
TNode* world(Nodes& list, int from, int duration, const Track& track, float parallax) {
    return world(list, from, duration, [track](int t) { return track.at(t); }, track.frame, parallax);
}
TNode* world(TNode* parent, int from, int duration, std::function<Shot(int)> shot, Vector2 frame, float parallax) {
    return follow(parent->add(from, duration), std::move(shot), frame, parallax);
}
TNode* world(TNode* parent, int from, int duration, const Track& track, float parallax) {
    return world(parent, from, duration, [track](int t) { return track.at(t); }, track.frame, parallax);
}

} } // rmtn::cam
