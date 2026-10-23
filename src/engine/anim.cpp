// anim.cpp -- spring physics and easing lookups.
#include "rmtn/anim.hpp"
#include "rmtn/timeline.hpp"

namespace rmtn {

Easing ease::fromIndex(int easeIdx) {
    return [easeIdx](double t) { return easeFn(easeIdx, t); };
}

namespace {
// Displacement from the target at time t (seconds) for a unit initial displacement at rest.
double springDisplacement(double t, const SpringConfig& c) {
    const double w0 = std::sqrt(c.stiffness / c.mass);
    const double zeta = c.damping / (2 * std::sqrt(c.stiffness * c.mass));
    if (zeta < 1) {                                   // underdamped: decaying oscillation
        const double wd = w0 * std::sqrt(1 - zeta * zeta);
        return std::exp(-zeta * w0 * t) * (std::cos(wd * t) + zeta * w0 / wd * std::sin(wd * t));
    }
    if (zeta == 1) return std::exp(-w0 * t) * (1 + w0 * t);   // critically damped
    const double s = std::sqrt(zeta * zeta - 1);                // overdamped: two decays
    const double r1 = -w0 * (zeta - s), r2 = -w0 * (zeta + s);
    return (r2 * std::exp(r1 * t) - r1 * std::exp(r2 * t)) / (r2 - r1);
}
// Time (seconds) the spring first reaches its target; only underdamped springs ever do.
double firstCrossing(const SpringConfig& c) {
    const double w0 = std::sqrt(c.stiffness / c.mass);
    const double zeta = c.damping / (2 * std::sqrt(c.stiffness * c.mass));
    if (zeta >= 1) return INFINITY;
    const double wd = w0 * std::sqrt(1 - zeta * zeta);
    return (3.14159265358979323846 - std::atan(wd / (zeta * w0))) / wd;
}
}

int measureSpring(double fps, const SpringConfig& config, double threshold) {
    // Settled = within threshold from here on; check a window past the candidate frame
    // so a zero crossing of an oscillation is not mistaken for rest.
    const int limit = (int)(fps * 60);
    const int window = std::max(1, (int)fps);
    int quiet = 0;
    for (int f = 0; f <= limit; f++) {
        if (std::fabs(springDisplacement(f / fps, config)) < threshold) {
            if (++quiet > window) return f - window;
        } else quiet = 0;
    }
    return limit;
}

double spring(double frame, const SpringOptions& o) {
    double f = frame - o.delay;
    if (f <= 0) return o.from;
    if (o.durationInFrames > 0) f *= (double)measureSpring(o.fps, o.config) / o.durationInFrames;
    const double t = f / o.fps;
    if (o.config.overshootClamping && t >= firstCrossing(o.config)) return o.to;   // hold once reached
    return o.to + (o.from - o.to) * springDisplacement(t, o.config);
}

} // namespace rmtn
