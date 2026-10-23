// rmtn/anim.hpp -- Remotion-style animation helpers: easing curves, interpolate(),
// interpolateColors(), spring(). Pure functions of the frame number.
#pragma once
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace rmtn {

// ------------------------------------------------------------------ easing
// An easing maps progress t in [0,1] to eased progress (which may overshoot).
using Easing = std::function<double(double)>;

namespace ease {
inline double linear(double t) { return t; }
inline double quad(double t) { return t * t; }
inline double cubic(double t) { return t * t * t; }
inline double sine(double t) { return 1 - std::cos(t * 3.14159265358979323846 / 2); }
inline double circle(double t) { return 1 - std::sqrt(std::max(0.0, 1 - t * t)); }
inline double expo(double t) { return t <= 0 ? 0 : std::pow(2.0, 10 * (t - 1)); }
inline double bounce(double t) {          // ease-in bounce, as Remotion's Easing.bounce
    const double n = 7.5625, d = 2.75;
    if (t < 1 / d) return n * t * t;
    if (t < 2 / d) { t -= 1.5 / d; return n * t * t + 0.75; }
    if (t < 2.5 / d) { t -= 2.25 / d; return n * t * t + 0.9375; }
    t -= 2.625 / d; return n * t * t + 0.984375;
}
inline Easing poly(double n) { return [n](double t) { return std::pow(t, n); }; }
// Pulls back before moving forward; s = 1.70158 is the classic amount.
inline Easing back(double s = 1.70158) { return [s](double t) { return t * t * ((s + 1) * t - s); }; }
// Spring-like oscillation; bounciness 1 ~ one overshoot.
inline Easing elastic(double bounciness = 1) {
    const double p = bounciness * 3.14159265358979323846;
    return [p](double t) { return 1 - std::pow(std::cos(t * 3.14159265358979323846 / 2), 3) * std::cos(t * p); };
}
// Wrappers turning an "in" curve into out / in-out variants.
inline Easing in(Easing f) { return f; }
inline Easing out(Easing f) { return [f](double t) { return 1 - f(1 - t); }; }
inline Easing inOut(Easing f) {
    return [f](double t) { return t < 0.5 ? f(t * 2) / 2 : 1 - f((1 - t) * 2) / 2; };
}
// CSS cubic-bezier(x1, y1, x2, y2); x1, x2 must lie in [0,1].
inline Easing bezier(double x1, double y1, double x2, double y2) {
    if (x1 < 0 || x1 > 1 || x2 < 0 || x2 > 1) throw std::invalid_argument("bezier x values must be in [0,1]");
    auto curve = [](double a, double b, double s) {   // one axis, P0 = 0, P3 = 1
        double u = 1 - s; return 3 * u * u * s * a + 3 * u * s * s * b + s * s * s;
    };
    auto slope = [](double a, double b, double s) {
        double u = 1 - s; return 3 * u * u * a + 6 * u * s * (b - a) + 3 * s * s * (1 - b);
    };
    return [=](double x) {
        if (x <= 0) return 0.0;
        if (x >= 1) return 1.0;
        double s = x;                                     // Newton, then bisection fallback
        for (int i = 0; i < 8; i++) {
            double err = curve(x1, x2, s) - x, d = slope(x1, x2, s);
            if (std::fabs(err) < 1e-9) return curve(y1, y2, s);
            if (std::fabs(d) < 1e-9) break;
            s -= err / d;
        }
        double lo = 0, hi = 1; s = x;
        for (int i = 0; i < 60; i++) {
            double v = curve(x1, x2, s);
            if (std::fabs(v - x) < 1e-9) break;
            if (v < x) lo = s; else hi = s;
            s = (lo + hi) / 2;
        }
        return curve(y1, y2, s);
    };
}
// The keyframe tracks' named easings (E_OUT_CUBIC etc.) as an Easing.
Easing fromIndex(int easeIdx);
} // namespace ease

// ------------------------------------------------------------------ interpolate
enum class Extrapolate {
    Extend,     // continue the first/last segment's line (Remotion's default)
    Clamp,      // hold the first/last output value
    Identity,   // return the input unchanged
    Wrap,       // repeat the input range
};
struct InterpOptions {
    Easing easing = ease::linear;
    Extrapolate left = Extrapolate::Extend, right = Extrapolate::Extend;
};
namespace detail {
// Index i >= 1 of the segment in[i-1]..in[i] that x falls in (end segments for outside x).
inline size_t segmentOf(double x, const std::vector<double>& in) {
    size_t seg = 1;
    while (seg < in.size() - 1 && x > in[seg]) seg++;
    return seg;
}
}
// Shorthand for the common clamped case: interpolate(t, {0, 30}, {0, 1}, clamped(ease::out(ease::cubic))).
inline InterpOptions clamped(Easing e = ease::linear) { return { std::move(e), Extrapolate::Clamp, Extrapolate::Clamp }; }

// Maps x through piecewise segments inRange[i]..inRange[i+1] -> outRange[i]..outRange[i+1],
// easing applied within each segment. inRange must be strictly increasing, same length
// as outRange, at least 2 entries.
inline double interpolate(double x, std::initializer_list<double> inRange,
                          std::initializer_list<double> outRange, const InterpOptions& o = {}) {
    const std::vector<double> in(inRange), out(outRange);
    if (in.size() < 2 || in.size() != out.size()) throw std::invalid_argument("interpolate: ranges need >= 2 equal-length entries");
    for (size_t i = 1; i < in.size(); i++)
        if (!(in[i] > in[i - 1])) throw std::invalid_argument("interpolate: inputRange must be strictly increasing");
    if (x < in.front() || x > in.back()) {
        Extrapolate mode = x < in.front() ? o.left : o.right;
        if (mode == Extrapolate::Identity) return x;
        if (mode == Extrapolate::Clamp) return x < in.front() ? out.front() : out.back();
        if (mode == Extrapolate::Wrap) {
            double span = in.back() - in.front();
            x = in.front() + std::fmod(std::fmod(x - in.front(), span) + span, span);
        }
    }
    size_t seg = detail::segmentOf(x, in);
    double t = (x - in[seg - 1]) / (in[seg] - in[seg - 1]);
    // Easing shapes the inside of the range; extension beyond it stays linear.
    double e = (t >= 0 && t <= 1) ? o.easing(t) : t;
    return out[seg - 1] + (out[seg] - out[seg - 1]) * e;
}

// Per-channel RGBA interpolation, clamped at both ends.
inline Color interpolateColors(double x, std::initializer_list<double> inRange,
                               std::initializer_list<Color> colors, Easing easing = ease::linear) {
    const std::vector<double> in(inRange);
    const std::vector<Color> cs(colors);
    if (in.size() < 2 || cs.size() != in.size()) throw std::invalid_argument("interpolateColors: ranges need >= 2 equal-length entries");
    x = std::min(std::max(x, in.front()), in.back());
    size_t seg = detail::segmentOf(x, in);
    double t = easing((x - in[seg - 1]) / (in[seg] - in[seg - 1]));
    auto channel = [&](unsigned char Color::*m) {
        double r = cs[seg - 1].*m + (cs[seg].*m - cs[seg - 1].*m) * t;
        return (unsigned char)std::lround(std::min(255.0, std::max(0.0, r)));
    };
    return { channel(&Color::r), channel(&Color::g), channel(&Color::b), channel(&Color::a) };
}

// ------------------------------------------------------------------ spring
struct SpringConfig {
    double mass = 1, stiffness = 100, damping = 10;
    bool overshootClamping = false;   // stop at `to` instead of overshooting
};
struct SpringOptions {
    double fps = 30;
    SpringConfig config{};
    double from = 0, to = 1;
    int delay = 0;                    // frames before the spring starts
    int durationInFrames = 0;         // > 0: time-stretch so the spring settles at this frame
};
// Closed-form damped harmonic oscillator released from `from` at rest, settling on `to`.
double spring(double frame, const SpringOptions& o = {});
// Frames until the spring stays within `threshold` (fraction of the distance) of its target.
int measureSpring(double fps, const SpringConfig& config = {}, double threshold = 0.005);

} // namespace rmtn
