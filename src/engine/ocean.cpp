// ocean.cpp -- the FFT ocean of Water::spectrum (W9): a directional JONSWAP spectrum split into cascades,
// turned into heights, displacement, slopes and a whitecap map by inverse FFTs, as a pure function of time.
#include "ocean.hpp"
#include "rmtn/weather.hpp"
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>

namespace rmtn {
namespace g3d {

// ---------------------------------------------------------------- the public Water functions of the spectrum
namespace {
constexpr double kG = 9.81;
constexpr float kRatio[3] = { 7.3f, 7.6f, 7.1f };       // patch of cascade c over patch of c + 1 (not a whole number)
constexpr float kStretch = 4.f;                         // how much longer than wide a whitecap is drawn (its area grows by about this)

Weather windOf(const Water& w) {
    Weather wx;
    wx.wind.speed = w.spectrum.windSpeed;
    return wx;
}
} // namespace

float Water::spectrumHs() const { return windOf(*this).waveHeight(fetch); }

std::vector<float> Water::patchSizes() const {
    const int C = std::clamp(spectrum.cascades, 1, ocean::kMaxCascades);
    const float peak = windOf(*this).wavePeakLength(fetch);
    float L = spectrum.patch > 0 ? spectrum.patch : std::clamp(8.f * peak * (spectrum.swell > 0.01f ? 1.8f : 1.f), 120.f, 2000.f);
    std::vector<float> out;
    for (int c = 0; c < C; c++) {
        out.push_back(L);
        L /= kRatio[std::min(c, 2)];
    }
    return out;
}

float Water::whitecapCover() const {
    const float U = std::max(spectrum.windSpeed, 0.f);
    // Monahan and O'Muircheartaigh's whitecap law W = 3.84e-6 U^3.41, times three for the decaying foam (stage B)
    // and a little over: 1.2e-5 U^3.41 is 0.14 % at 4 m/s, 3 % at 10 and 23 % at 18 on a fully developed sea.
    // Raised in a fresh breeze (about twice at 10 m/s, back to the law by 18): at sea force 5 white horses are everywhere, and
    // the law counts only what is white at once, where an eye also counts the streaks and the thin foam.
    const float bt = std::clamp((U - 8.f) / 10.f, 0.f, 1.f), boost = 1.f + 1.2f * (1.f - bt * bt * (3.f - 2.f * bt));
    const float w = 1.2e-5f * std::pow(U, 3.41f) * boost;
    // A sea held down by a short fetch breaks less: scaled by Hs over the fully developed Hs.
    const float full = 0.21f * U * U / (float)kG;
    const float r = full > 1e-4f ? std::clamp(spectrumHs() / full, 0.f, 1.f) : 0.f;
    return std::clamp(w * r, 0.f, 0.5f);
}

float Water::horizonDip(float above) { return std::atan(std::sqrt(2.f * std::max(above, 0.f) / kPlanetRadius)); }

namespace ocean {

// ---------------------------------------------------------------- the warp
Warp warpOf(float patch, int c, bool on) {
    Warp w;
    if (!on) return w;
    w.amp = 0.15f * patch;
    w.a = 2 * PI / ((3.3f + 0.4f * c) * patch);
    w.b = 2 * PI / ((4.1f - 0.3f * c) * patch);
    w.p1 = 1.7f + 2.3f * c;
    w.p2 = 0.4f + 1.9f * c;
    return w;
}

Vector2 warpPoint(const Warp& w, Vector2 p, float* alpha, float* beta) {
    const float s1 = w.a * p.y + w.p1, s2 = w.b * p.x + w.p2;
    if (alpha) *alpha = w.amp * w.a * std::cos(s1);
    if (beta) *beta = w.amp * w.b * std::cos(s2);
    return { p.x + w.amp * std::sin(s1), p.y + w.amp * std::sin(s2) };
}

namespace {

// ---------------------------------------------------------------- complex numbers and the FFT
struct Cx { float r = 0, i = 0; };
inline Cx operator+(Cx a, Cx b) { return { a.r + b.r, a.i + b.i }; }
inline Cx operator*(Cx a, Cx b) { return { a.r * b.r - a.i * b.i, a.r * b.i + a.i * b.r }; }
inline Cx scale(Cx a, float s) { return { a.r * s, a.i * s }; }
inline Cx timesI(Cx a) { return { -a.i, a.r }; }
inline Cx conj(Cx a) { return { a.r, -a.i }; }

struct Plan {
    int rev[kN];
    float cs[kN], sn[kN];
    Plan() {
        for (int i = 0; i < kN; i++) {
            int r = 0;
            for (int b = 0, x = i; b < 7; b++, x >>= 1) r = (r << 1) | (x & 1);
            rev[i] = r;
            cs[i] = (float)std::cos(2 * 3.14159265358979323846 * i / kN);
            sn[i] = (float)std::sin(2 * 3.14159265358979323846 * i / kN);
        }
    }
};
const Plan& plan() { static const Plan p; return p; }

// In place, sum x[n] e^{+2 pi i n m / N}.
void fft1(Cx* x) {
    const Plan& P = plan();
    for (int i = 0; i < kN; i++) {
        const int j = P.rev[i];
        if (j > i) std::swap(x[i], x[j]);
    }
    for (int len = 2; len <= kN; len <<= 1) {
        const int half = len / 2, step = kN / len;
        for (int i = 0; i < kN; i += len)
            for (int j = 0; j < half; j++) {
                const Cx w{ P.cs[j * step], P.sn[j * step] };
                const Cx u = x[i + j], v = x[i + j + half] * w;
                x[i + j] = { u.r + v.r, u.i + v.i };
                x[i + j + half] = { u.r - v.r, u.i - v.i };
            }
    }
}
void fft2(std::vector<Cx>& a) {
    for (int y = 0; y < kN; y++) fft1(&a[(size_t)y * kN]);
    Cx col[kN];
    for (int x = 0; x < kN; x++) {
        for (int y = 0; y < kN; y++) col[y] = a[(size_t)y * kN + x];
        fft1(col);
        for (int y = 0; y < kN; y++) a[(size_t)y * kN + x] = col[y];
    }
}

// ---------------------------------------------------------------- deterministic randomness
uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
double uniform(uint32_t seed, uint32_t c, uint32_t jx, uint32_t jz, uint32_t salt) {
    uint32_t h = hash32(seed * 0x9E3779B1U + 0x85EBCA6BU);
    h = hash32(h ^ ((c + 1) * 0xC2B2AE35U));
    h = hash32(h + jx * 0x27D4EB2FU);
    h = hash32(h ^ (jz * 0x165667B1U + salt));
    return ((double)h + 0.5) / 4294967296.0;
}
// A standard normal pair (Box-Muller) for one mode.
void gaussian(uint32_t seed, uint32_t c, uint32_t jx, uint32_t jz, double* a, double* b) {
    const double u1 = uniform(seed, c, jx, jz, 1), u2 = uniform(seed, c, jx, jz, 2);
    const double r = std::sqrt(-2 * std::log(u1)), t = 2 * 3.14159265358979323846 * u2;
    *a = r * std::cos(t);
    *b = r * std::sin(t);
}

// ---------------------------------------------------------------- the spectrum
double jonswap(double k, double kp, double gamma) {      // S(omega) up to a constant, deep water
    const double w = std::sqrt(kG * k), wp = std::sqrt(kG * kp);
    const double sigma = w <= wp ? 0.07 : 0.09, d = w - wp;
    const double r = std::exp(-d * d / (2 * sigma * sigma * wp * wp));
    return std::pow(w, -5.0) * std::exp(-1.25 * std::pow(wp / w, 4.0)) * std::pow(gamma, r);
}
double spreading(double theta, double heading, double s) {   // cos^2s of half the angle off the heading (Mitsuyasu), downwind side only
    double d = std::fmod(theta - heading, 2 * 3.14159265358979323846);
    if (d > 3.14159265358979323846) d -= 2 * 3.14159265358979323846;
    if (d < -3.14159265358979323846) d += 2 * 3.14159265358979323846;
    if (std::fabs(d) >= 3.14159265358979323846 / 2) return 0;
    return std::pow(std::cos(d / 2), 2 * s);
}
float smooth01(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}

// What the grids are built from. Equal Params give the same model.
struct Params {
    int cascades = 3;
    float U = 10, dir = 0, gamma = 3.3f, spread = 8, swell = 0, swellDir = 30, patch = 0, fetch = 50000;
    unsigned seed = 1;
    bool operator==(const Params& o) const {
        return cascades == o.cascades && U == o.U && dir == o.dir && gamma == o.gamma && spread == o.spread && swell == o.swell &&
               swellDir == o.swellDir && patch == o.patch && fetch == o.fetch && seed == o.seed;
    }
};
Params paramsOf(const Water& w) {
    const Water::Spectrum& s = w.spectrum;
    return { std::clamp(s.cascades, 1, kMaxCascades), s.windSpeed, s.windDirection, s.gamma, s.spread, std::clamp(s.swell, 0.f, 1.f),
             s.swellDirection, s.patch, w.fetch, s.seed };
}

struct Model {
    Params p;
    int C = 0;
    float patch[kMaxCascades] = {};
    float hs = 0;
    double slopeResolved = 0, slopeTail = 0;     // mean square slope (both axes) in the grids, and what lies above them (analytic)
    std::vector<Cx> h0[kMaxCascades];
    std::vector<float> omega[kMaxCascades], kx[kMaxCascades], kz[kMaxCascades], ax[kMaxCascades], az[kMaxCascades];
};

std::shared_ptr<Model> buildModel(const Water& w) {
    auto m = std::make_shared<Model>();
    m->p = paramsOf(w);
    const std::vector<float> patches = w.patchSizes();
    m->C = (int)patches.size();
    m->hs = w.spectrumHs();
    const Weather wx = windOf(w);
    const double kp = 2 * 3.14159265358979323846 / wx.wavePeakLength(w.fetch);
    const double ks = kp / 1.8;                             // the swell: 1.8 times longer
    const double heading = w.spectrum.windDirection * 3.14159265358979323846 / 180;
    const double swellHeading = (w.spectrum.windDirection + w.spectrum.swellDirection) * 3.14159265358979323846 / 180;
    const double sw = std::clamp(w.spectrum.swell, 0.f, 1.f);
    const double gamma = std::max(w.spectrum.gamma, 1.f), spread = std::max(w.spectrum.spread, 0.f);
    const int C = m->C;
    // Upper edge of each cascade's band in wavenumber (rad/m), where its weight has fallen to zero.
    double top[kMaxCascades];
    for (int c = 0; c < C; c++) {
        m->patch[c] = patches[(size_t)c];
        top[c] = 2 * 3.14159265358979323846 * (c == C - 1 ? 40.0 : 16.0) / patches[(size_t)c];
    }
    const double topFade = 0.6;
    std::vector<double> vWind[kMaxCascades], vSwell[kMaxCascades];
    double sumWind = 0, sumSwell = 0;
    for (int c = 0; c < C; c++) {
        const double L = m->patch[c], dk = 2 * 3.14159265358979323846 / L;
        const size_t n = (size_t)kN * kN;
        vWind[c].assign(n, 0.0); vSwell[c].assign(n, 0.0);
        m->omega[c].assign(n, 0.f); m->kx[c].assign(n, 0.f); m->kz[c].assign(n, 0.f); m->ax[c].assign(n, 0.f); m->az[c].assign(n, 0.f);
        for (int jz = 0; jz < kN; jz++)
            for (int jx = 0; jx < kN; jx++) {
                const int nx = jx < kN / 2 ? jx : jx - kN, nz = jz < kN / 2 ? jz : jz - kN;
                const size_t idx = (size_t)jz * kN + jx;
                if ((nx == 0 && nz == 0) || std::abs(nx) == kN / 2 || std::abs(nz) == kN / 2) continue;
                const double kxv = dk * nx, kzv = dk * nz, k = std::hypot(kxv, kzv);
                m->kx[c][idx] = (float)kxv; m->kz[c][idx] = (float)kzv;
                m->ax[c][idx] = (float)(kxv / k); m->az[c][idx] = (float)(kzv / k);
                m->omega[c][idx] = (float)std::sqrt(kG * k);
                // The cascade's weight: cross-fade with its neighbours so the energies add to one spectrum.
                double wgt = 1;
                if (c > 0) wgt *= std::sin(0.5 * 3.14159265358979323846 * smooth01((float)(topFade * top[c - 1]), (float)top[c - 1], (float)k));
                wgt *= std::cos(0.5 * 3.14159265358979323846 * smooth01((float)((c == C - 1 ? 0.7 : topFade) * top[c]), (float)top[c], (float)k));
                if (wgt <= 0) continue;
                const double theta = std::atan2(kzv, kxv), jac = 0.5 * std::sqrt(kG / k) / k;       // d omega / d k, per k
                const double area = dk * dk * wgt * wgt;
                // Shorter waves spread wider than the peak does: the exponent falls as the square root of k / kp.
                const double dW = spreading(theta, heading, std::max(1.0, spread * std::min(1.0, std::sqrt(kp / k))));
                if (dW > 0) { vWind[c][idx] = jonswap(k, kp, gamma) * jac * dW * area; sumWind += vWind[c][idx]; }
                if (sw > 0) {
                    const double dS = spreading(theta, swellHeading, 20.0);
                    if (dS > 0) { vSwell[c][idx] = jonswap(k, ks, 6.0) * jac * dS * area; sumSwell += vSwell[c][idx]; }
                }
            }
    }
    // Every mode's variance, normalised to one for the whole spectrum, then drawn as a complex Gaussian.
    double real = 0;
    for (int c = 0; c < C; c++) {
        m->h0[c].assign((size_t)kN * kN, Cx{});
        for (int jz = 0; jz < kN; jz++)
            for (int jx = 0; jx < kN; jx++) {
                const size_t idx = (size_t)jz * kN + jx;
                double v = 0;
                if (sumWind > 0) v += (1 - sw) * vWind[c][idx] / sumWind;
                if (sumSwell > 0) v += sw * vSwell[c][idx] / sumSwell;
                else if (sw > 0 && sumWind > 0) v += sw * vWind[c][idx] / sumWind;
                if (v <= 0) continue;
                double g1, g2;
                gaussian(w.spectrum.seed, (uint32_t)c, (uint32_t)jx, (uint32_t)jz, &g1, &g2);
                const double amp = 0.5 * std::sqrt(v);          // E|h0|^2 = v / 2
                const Cx h{ (float)(amp * g1), (float)(amp * g2) };
                m->h0[c][idx] = h;
                real += 2.0 * ((double)h.r * h.r + (double)h.i * h.i);
            }
    }
    // The realisation is rescaled so its variance is exactly (Hs / 4)^2 (averaged over time).
    if (real > 0) {
        const float s = (float)(m->hs / 4 / std::sqrt(real));
        for (int c = 0; c < C; c++) for (Cx& h : m->h0[c]) h = scale(h, s);
    }
    // The mean square slope (both axes) the grids hold, and the tail above them: the spectrum goes on as k^-3 (the
    // equilibrium range) down to the capillary cut at 1.7 cm, and its slope variance there is unresolved roughness.
    {
        double ms = 0;
        for (int c = 0; c < C; c++)
            for (size_t i = 0; i < m->h0[c].size(); i++) {
                const double k2 = (double)m->kx[c][i] * m->kx[c][i] + (double)m->kz[c][i] * m->kz[c][i];
                ms += 2.0 * ((double)m->h0[c][i].r * m->h0[c][i].r + (double)m->h0[c][i].i * m->h0[c][i].i) * k2;
            }
        m->slopeResolved = ms;
        // The short-wave branch of Elfouhaily et al.: the curvature spectrum B(k) = 0.5 alpha_m (c_m / c) (the slope variance
        // per unit ln k), which rises with the friction velocity, down to 5 mm. Gravity-capillary phase speed c.
        const double k1 = 0.85 * top[C - 1], kmax = 2 * 3.14159265358979323846 / 0.005;
        const double U = std::max((double)m->p.U, 0.0), ustar = U * std::sqrt(0.0008 + 0.000065 * U), cm = 0.23;
        const double alphaM = ustar < cm ? 0.01 * (1 + std::log(std::max(ustar, 1e-3) / cm)) : 0.01 * (1 + 3 * std::log(ustar / cm));
        double tail = 0;
        if (kmax > k1) {
            const int steps = 200;
            const double lk = std::log(kmax / k1) / steps;
            for (int s = 0; s < steps; s++) {
                const double k = k1 * std::exp(lk * (s + 0.5)), c = std::sqrt(kG / k + 7.4e-5 * k);
                tail += 0.5 * std::max(alphaM, 0.0) * (cm / c) * lk;
            }
        }
        m->slopeTail = tail;
    }
    return m;
}

// h(k, t) = h0(k) e^{-i w t} + conj(h0(-k)) e^{+i w t}: the wave of mode k travels towards +k.
void hat(const Model& m, int c, double t, std::vector<Cx>& H) {
    H.resize((size_t)kN * kN);
    for (int jz = 0; jz < kN; jz++)
        for (int jx = 0; jx < kN; jx++) {
            const size_t idx = (size_t)jz * kN + jx, neg = (size_t)((kN - jz) % kN) * kN + (size_t)((kN - jx) % kN);
            const double phase = std::fmod((double)m.omega[c][idx] * t, 2 * 3.14159265358979323846);
            const float cs = (float)std::cos(phase), sn = (float)std::sin(phase);
            H[idx] = m.h0[c][idx] * Cx{ cs, -sn } + conj(m.h0[c][neg]) * Cx{ cs, sn };
        }
}

enum Kind { kH, kDx, kDz, kSx, kSz, kDxx, kDzz, kDxz, kNone };
Cx derive(const Model& m, int c, size_t idx, Kind kind, Cx H) {
    switch (kind) {
        case kH: return H;
        case kDx: return timesI(scale(H, m.ax[c][idx]));
        case kDz: return timesI(scale(H, m.az[c][idx]));
        case kSx: return timesI(scale(H, m.kx[c][idx]));
        case kSz: return timesI(scale(H, m.kz[c][idx]));
        case kDxx: return scale(H, -m.kx[c][idx] * m.ax[c][idx]);
        case kDzz: return scale(H, -m.kz[c][idx] * m.az[c][idx]);
        case kDxz: return scale(H, -m.kx[c][idx] * m.az[c][idx]);
        default: return Cx{};
    }
}
// Two real fields through one FFT: the real part of the result is field A, the imaginary part field B.
std::vector<Cx> transform(const Model& m, int c, Kind ka, const std::vector<Cx>& Ha, Kind kb, const std::vector<Cx>& Hb) {
    std::vector<Cx> out((size_t)kN * kN);
    for (size_t idx = 0; idx < out.size(); idx++) {
        const Cx a = derive(m, c, idx, ka, Ha[idx]), b = derive(m, c, idx, kb, Hb[idx]);
        out[idx] = { a.r - b.i, a.i + b.r };
    }
    fft2(out);
    return out;
}

std::shared_ptr<Frame> buildFrame(const Model& m, const Water& w, double seconds) {
    auto f = std::make_shared<Frame>();
    const int C = m.C;
    f->cascades = C;
    f->seconds = seconds;
    f->lambda = std::max(w.spectrum.choppiness, 0.f);
    f->hs = m.hs;
    f->tail = (float)m.slopeTail;
    f->resolved = (float)m.slopeResolved;
    f->cover = w.whitecapCover();
    const std::vector<Cx> none;
    // The whitecaps come from the finest cascade that still has patches of 30 m or more: waves of a few metres to a dozen.
    int foamC = 0;
    for (int c = 0; c < C; c++) if (m.patch[c] >= 30.f) foamC = c;
    static constexpr double lags[3] = { 0.0, 0.9, 2.0 };
    static constexpr float lagWeight[3] = { 1.f, 0.57f, 0.29f };
    for (int c = 0; c < C; c++) {
        f->patch[c] = m.patch[c];
        f->warp[c] = warpOf(m.patch[c], c, w.spectrum.decorrelate);
        if (m.patch[c] >= kGeometryPatch) f->geometry = c + 1;
        std::vector<Cx> H;
        hat(m, c, seconds, H);
        const std::vector<Cx> g1 = transform(m, c, kH, H, kDx, H), g2 = transform(m, c, kDz, H, kSx, H);
        const size_t n = (size_t)kN * kN;
        f->a[c].assign(n * 4, 0.f);
        f->b[c].assign(n * 4, 0.f);
        for (size_t i = 0; i < n; i++) {
            f->a[c][i * 4 + 0] = g1[i].i;       // Dx
            f->a[c][i * 4 + 1] = g1[i].r;       // h
            f->a[c][i * 4 + 2] = g2[i].r;       // Dz
            f->b[c][i * 4 + 0] = g2[i].i;       // Sx
        }
        std::vector<Cx> g3;
        if (c != foamC) {
            g3 = transform(m, c, kSz, H, kNone, H);
        } else {
            // The whitecap map: the Jacobian of the displacement at now and a few moments earlier, so foam stays
            // behind a crest and fades. A function of time, not a stored state.
            std::vector<Cx> H1, H2;
            hat(m, c, seconds - lags[1], H1);
            hat(m, c, seconds - lags[2], H2);
            g3 = transform(m, c, kSz, H, kDxz, H2);
            const std::vector<Cx> d0 = transform(m, c, kDxx, H, kDzz, H), d1 = transform(m, c, kDxz, H, kDxz, H1),
                                  d2 = transform(m, c, kDxx, H1, kDzz, H1), d3 = transform(m, c, kDxx, H2, kDzz, H2);
            // d1: real = Dxz(now), imaginary = Dxz(now - 0.9)  (both from the same FFT: field A from H, field B from H1)
            const float lam = f->lambda;
            std::vector<float> J[3];
            for (int i = 0; i < 3; i++) J[i].assign(n, 1.f);
            for (size_t i = 0; i < n; i++) {
                const float dxx[3] = { d0[i].r, d2[i].r, d3[i].r }, dzz[3] = { d0[i].i, d2[i].i, d3[i].i };
                const float dxz[3] = { d1[i].r, d1[i].i, g3[i].i };
                for (int l = 0; l < 3; l++) J[l][i] = (1 + lam * dxx[l]) * (1 + lam * dzz[l]) - lam * lam * dxz[l] * dxz[l];
            }
            // Builds the map for a share q of the area below the Jacobian threshold, and returns its mean.
            auto build = [&](float q) {
                float jt = -1e9f, jlo = -1e9f;
                if (q > 1e-6f) {
                    std::vector<float> sorted = J[0];
                    const size_t at = std::min(n - 1, (size_t)(q * (float)n));
                    std::nth_element(sorted.begin(), sorted.begin() + (long)at, sorted.end());
                    jt = sorted[at];
                    std::nth_element(sorted.begin(), sorted.begin() + (long)(at / 4), sorted.begin() + (long)at);
                    jlo = sorted[at / 4];
                }
                f->jacobianThreshold = jt;
                const float width = std::clamp(0.5f * (jt - jlo), 0.01f, 0.3f);      // full white in the heart of a patch, whatever the wind
                std::vector<float> src(n);
                for (size_t i = 0; i < n; i++) {
                    float foam = 0;
                    for (int l = 0; l < 3; l++) {
                        const float x = std::clamp((jt - J[l][i]) / width, 0.f, 1.f);
                        foam = std::max(foam, lagWeight[l] * x * x * (3 - 2 * x));
                    }
                    src[i] = foam;
                }
                // Whitecaps lie along the crest, not as dots: the foam is drawn out across the wind (a maximum over a line of
                // texels, tapering), so a patch is several times longer than wide.
                const double hd = w.spectrum.windDirection * 3.14159265358979323846 / 180;
                const double dx = -std::sin(hd), dz = std::cos(hd);
                const int S = std::clamp((int)std::lround(6.0 / (m.patch[c] / kN)), 4, 40);   // the crest line, about 6 m each side
                double sum = 0;
                int ox[81], oz[81], wrap = 0; (void)wrap;
                for (int s = -S; s <= S; s++) { ox[s + S] = (int)std::lround(s * dx); oz[s + S] = (int)std::lround(s * dz); }
                for (int jz = 0; jz < kN; jz++)
                    for (int jx = 0; jx < kN; jx++) {
                        float best = src[(size_t)jz * kN + jx];
                        for (int s = -S; s <= S; s++) {
                            if (!s) continue;
                            const int ax = (jx + ox[s + S]) & (kN - 1), az = (jz + oz[s + S]) & (kN - 1);
                            const float t = (float)s / (S + 1);
                            best = std::max(best, src[(size_t)az * kN + ax] * (1.f - 0.5f * t * t));
                        }
                        f->a[c][((size_t)jz * kN + jx) * 4 + 3] = best;
                        sum += best;
                    }
                // A light blur of the stretched map so the steps of the line of texels do not show up close.
                {
                    std::vector<float> g(n);
                    for (size_t i = 0; i < n; i++) g[i] = f->a[c][i * 4 + 3];
                    for (int jz = 0; jz < kN; jz++)
                        for (int jx = 0; jx < kN; jx++) {
                            float a = 0;
                            for (int oz = -1; oz <= 1; oz++)
                                for (int ox = -1; ox <= 1; ox++)
                                    a += g[(size_t)((jz + oz + kN) % kN) * kN + (size_t)((jx + ox + kN) % kN)] * (ox == 0 && oz == 0 ? 0.25f : (ox == 0 || oz == 0 ? 0.125f : 0.0625f));
                            f->a[c][((size_t)jz * kN + jx) * 4 + 3] = a;
                        }
                    sum = 0;
                    for (size_t i = 0; i < n; i++) sum += f->a[c][i * 4 + 3];
                }
                return sum / (double)n;
            };
            // The area the law gives (the mean foam is about 0.7 of it): one pass at the stretch's average, then a correction.
            float q = std::clamp(f->cover / 0.5f / 1.6f / kStretch, 0.f, 0.4f);
            if (q > 1e-6f) {
                const double mean = build(q), target = 0.7 * f->cover / 0.5;     // the shader's slow mask lets about half of the map through
                if (mean > 1e-9 && std::fabs(mean / target - 1) > 0.15) build(std::clamp((float)(q * target / mean), q / 8, std::min(q * 8, 0.4f)));
            } else build(0.f);
        }
        for (size_t i = 0; i < n; i++) f->b[c][i * 4 + 1] = g3[i].r;     // Sz
        // The slope variance each mip level averages away.
        std::vector<float> sx(n), sz(n);
        for (size_t i = 0; i < n; i++) { sx[i] = f->b[c][i * 4]; sz[i] = f->b[c][i * 4 + 1]; }
        int size = kN;
        double ms0 = 0;
        for (int l = 0; l < kLevels; l++) {
            double ms = 0;
            for (int i = 0; i < size * size; i++) ms += (double)sx[(size_t)i] * sx[(size_t)i] + (double)sz[(size_t)i] * sz[(size_t)i];
            ms /= (double)size * size;
            if (l == 0) ms0 = ms;
            f->lost[c][l] = (float)std::max(ms0 - ms, 0.0);
            if (size > 1) {
                const int h = size / 2;
                std::vector<float> nx((size_t)h * h), nz((size_t)h * h);
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < h; x++) {
                        auto at = [&](const std::vector<float>& a) {
                            return 0.25f * (a[(size_t)(2 * y) * size + 2 * x] + a[(size_t)(2 * y) * size + 2 * x + 1] +
                                            a[(size_t)(2 * y + 1) * size + 2 * x] + a[(size_t)(2 * y + 1) * size + 2 * x + 1]);
                        };
                        nx[(size_t)y * h + x] = at(sx);
                        nz[(size_t)y * h + x] = at(sz);
                    }
                sx.swap(nx); sz.swap(nz);
                size = h;
            }
        }
        f->lost[c][kLevels - 1] = (float)ms0;     // the whole tile averaged: all of it is gone
    }
    f->geometry = std::max(f->geometry, 1);       // the largest cascade always moves the mesh
    return f;
}

// ---------------------------------------------------------------- caches
struct FrameKey {
    Params p;
    float lambda;
    bool decorrelate;
    double seconds;
    float foam;
};

std::mutex g_lock;
std::vector<std::shared_ptr<Model>> g_models;
struct CachedFrame { FrameKey key; std::shared_ptr<const Frame> frame; };
std::vector<CachedFrame> g_frames;
uint64_t g_nextId = 1;

} // namespace

std::shared_ptr<const Frame> frameAt(const Water& w, double seconds) {
    std::lock_guard<std::mutex> lock(g_lock);
    const FrameKey key{ paramsOf(w), w.spectrum.choppiness, w.spectrum.decorrelate, seconds, w.foam };
    for (auto it = g_frames.begin(); it != g_frames.end(); ++it)
        if (it->key.p == key.p && it->key.lambda == key.lambda && it->key.decorrelate == key.decorrelate && it->key.seconds == key.seconds) {
            auto hit = *it;
            g_frames.erase(it);
            g_frames.push_back(hit);
            return hit.frame;
        }
    std::shared_ptr<Model> model;
    for (auto it = g_models.begin(); it != g_models.end(); ++it)
        if ((*it)->p == key.p) {
            model = *it;
            g_models.erase(it);
            g_models.push_back(model);
            break;
        }
    if (!model) {
        model = buildModel(w);
        g_models.push_back(model);
        if (g_models.size() > 3) g_models.erase(g_models.begin());
    }
    std::shared_ptr<Frame> f = buildFrame(*model, w, seconds);
    f->id = g_nextId++;
    g_frames.push_back({ key, f });
    if (g_frames.size() > 6) g_frames.erase(g_frames.begin());
    return f;
}

// ---------------------------------------------------------------- the CPU surface
namespace {
// Bilinear read of a periodic N x N x 4 grid at a continuous grid coordinate (nodes on the integers).
void bilerp(const std::vector<float>& a, float gx, float gz, float out[4]) {
    const float fx = std::floor(gx), fz = std::floor(gz);
    const float tx = gx - fx, tz = gz - fz;
    const int x0 = (((int)fx % kN) + kN) % kN, z0 = (((int)fz % kN) + kN) % kN, x1 = (x0 + 1) % kN, z1 = (z0 + 1) % kN;
    const float* p00 = &a[((size_t)z0 * kN + x0) * 4];
    const float* p10 = &a[((size_t)z0 * kN + x1) * 4];
    const float* p01 = &a[((size_t)z1 * kN + x0) * 4];
    const float* p11 = &a[((size_t)z1 * kN + x1) * 4];
    for (int i = 0; i < 4; i++)
        out[i] = (p00[i] * (1 - tx) + p10[i] * tx) * (1 - tz) + (p01[i] * (1 - tx) + p11[i] * tx) * tz;
}
} // namespace

float heightAt(const Frame& f, float x, float z, Vector2* slope) {
    const int G = std::max(f.geometry, 1);
    Vector2 p{ x, z };
    for (int it = 0; it < 4; it++) {
        float dx = 0, dz = 0;
        for (int c = 0; c < G; c++) {
            const Vector2 q = warpPoint(f.warp[c], p, nullptr, nullptr);
            float v[4];
            bilerp(f.a[c], q.x / f.patch[c] * kN, q.y / f.patch[c] * kN, v);
            dx += v[0]; dz += v[2];
        }
        p = { x - f.lambda * dx, z - f.lambda * dz };
    }
    float h = 0, gx = 0, gz = 0;
    for (int c = 0; c < G; c++) {
        float al, be;
        const Vector2 q = warpPoint(f.warp[c], p, &al, &be);
        float v[4], s[4];
        bilerp(f.a[c], q.x / f.patch[c] * kN, q.y / f.patch[c] * kN, v);
        h += v[1];
        if (slope) {
            bilerp(f.b[c], q.x / f.patch[c] * kN, q.y / f.patch[c] * kN, s);
            gx += s[0] + be * s[1];
            gz += al * s[0] + s[1];
        }
    }
    if (slope) *slope = { gx, gz };
    return h;
}

} // namespace ocean

float Water::shadingSlopeVariance(float* tail) const {
    if (!spectrum.enabled) { if (tail) *tail = 0; return 0; }
    const auto f = ocean::frameAt(*this, 0);
    if (tail) *tail = f->tail;
    return f->resolved + f->tail;
}

} // namespace g3d
} // namespace rmtn
