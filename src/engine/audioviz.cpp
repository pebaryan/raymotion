// audioviz.cpp -- per-frame audio analysis (level, spectrum, onsets, beats, tempo) and
// simple visualisers (see rmtn/audioviz.hpp).
#include "rmtn/audioviz.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/media.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <map>

namespace rmtn {
namespace aud {
namespace {

constexpr int FFT = 4096;           // 11.7 Hz bins at 48 kHz: enough to separate bass notes
constexpr int RATE = 48000;

void fft(std::vector<std::complex<float>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2 * PI / len;
        const std::complex<float> wl((float)std::cos(ang), (float)std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1, 0);
            for (size_t k = 0; k < len / 2; k++) {
                const std::complex<float> u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

float percentile(std::vector<float> v, double p) {
    if (v.empty()) return 0;
    const size_t k = std::min(v.size() - 1, (size_t)(p * (v.size() - 1)));
    std::nth_element(v.begin(), v.begin() + k, v.end());
    return v[k];
}

// Rises with a time constant of `attack` frames and falls with `release`.
void envelope(std::vector<float>& v, size_t stride, size_t offset, size_t n, float attack, float release) {
    const float a = attack > 0 ? 1 - std::exp(-1 / attack) : 1, r = release > 0 ? 1 - std::exp(-1 / release) : 1;
    float y = 0;
    for (size_t f = 0; f < n; f++) {
        float& x = v[f * stride + offset];
        y += (x - y) * (x > y ? a : r);
        x = y;
    }
}

} // namespace

struct Analysis::Data {
    double hop = 1;                 // samples per frame
    int n = 0, bandCount = 0;
    double shift = 0;               // startFrom in frames
    std::vector<float> mono;
    float peak = 1;
    std::vector<float> level, bands, onset, lo, hi;
    std::vector<double> beats;      // local frames
    double bpm = 0;
};

Analysis::Analysis(std::string file, Options o) : file_(std::move(file)), o_(o) {}
Analysis::Analysis(std::vector<float> stereo, int rate, Options o) : o_(o), pending_(std::move(stereo)), pendingRate_(rate) {}

const Analysis::Data& Analysis::data() const {
    std::call_once(once_, [this] {
        auto d = std::make_shared<Data>();
        std::vector<float> stereo;
        int rate = RATE;
        if (!file_.empty()) stereo = media::decodeAudio(file_, RATE);
        else { stereo.swap(pending_); rate = pendingRate_; }
        const Options& o = o_;
        const int fps = std::max(1, o.fps), B = std::max(1, o.bands);
        d->bandCount = B;
        d->shift = o.startFrom * fps;
        d->mono.resize(stereo.size() / 2);
        float peak = 1e-6f;
        for (size_t i = 0; i < d->mono.size(); i++) {
            d->mono[i] = 0.5f * (stereo[2 * i] + stereo[2 * i + 1]);
            peak = std::max(peak, std::fabs(d->mono[i]));
        }
        d->peak = peak;
        d->hop = (double)rate / fps;
        const int n = d->n = (int)std::ceil(d->mono.size() / d->hop);

        // Band edges, log-spaced, as FFT bins (at least one bin each).
        std::vector<int> k0(B), k1(B);
        const double nyquist = rate / 2.0, top = std::min<double>(o.maxHz, nyquist), bottom = std::max(1.f, o.minHz);
        for (int b = 0; b < B; b++) {
            const double f0 = bottom * std::pow(top / bottom, (double)b / B), f1 = bottom * std::pow(top / bottom, (double)(b + 1) / B);
            d->lo.push_back((float)f0);
            d->hi.push_back((float)f1);
            k0[b] = std::clamp((int)std::floor(f0 / rate * FFT), 1, FFT / 2 - 1);
            k1[b] = std::clamp((int)std::ceil(f1 / rate * FFT), k0[b] + 1, FFT / 2);
        }
        std::vector<float> window(FFT);
        for (int i = 0; i < FFT; i++) window[i] = (float)(0.5 - 0.5 * std::cos(2 * PI * i / (FFT - 1)));
        std::vector<float> levelDb(n), bandDb((size_t)n * B);
        std::vector<std::complex<float>> buf(FFT);
        for (int f = 0; f < n; f++) {
            const long long a = (long long)std::floor(f * d->hop), e = std::min<long long>((long long)d->mono.size(), (long long)std::floor((f + 1) * d->hop));
            double sum = 0;
            for (long long i = a; i < e; i++) sum += (double)d->mono[i] * d->mono[i];
            levelDb[f] = (float)(10 * std::log10(sum / std::max(1LL, e - a) + 1e-12));
            const long long start = (long long)std::llround((f + 0.5) * d->hop) - FFT / 2;
            for (int i = 0; i < FFT; i++) {
                const long long s = start + i;
                buf[i] = { s >= 0 && s < (long long)d->mono.size() ? d->mono[s] * window[i] : 0.f, 0.f };
            }
            fft(buf);
            for (int b = 0; b < B; b++) {
                double p = 0;
                for (int k = k0[b]; k < k1[b]; k++) p += std::norm(buf[k]);
                bandDb[(size_t)f * B + b] = (float)(10 * std::log10(p / ((double)FFT * FFT) + 1e-14));
            }
        }
        // 0..1 over `range` dB below the loud passages (the 98th percentile). Each band has
        // its own reference, so the treble moves as much as the bass, but never more than
        // 24 dB above the loudest band (a band that is always near silent stays low).
        const float range = std::max(1.f, o.range);
        const float levelRef = percentile(levelDb, 0.98);
        d->level.resize(n);
        for (int f = 0; f < n; f++) d->level[f] = std::clamp((levelDb[f] - (levelRef - range)) / range, 0.f, 1.f);
        const float global = percentile(bandDb, 0.98);
        d->bands.resize(bandDb.size());
        for (int b = 0; b < B; b++) {
            std::vector<float> col(n);
            for (int f = 0; f < n; f++) col[f] = bandDb[(size_t)f * B + b];
            const float ref = std::max(percentile(col, 0.98), global - 24);
            for (int f = 0; f < n; f++) d->bands[(size_t)f * B + b] = std::clamp((col[f] - (ref - range)) / range, 0.f, 1.f);
        }
        // Onsets: the spectral flux (summed rises of the bands), before smoothing.
        d->onset.assign(n, 0);
        for (int f = 1; f < n; f++) {
            double flux = 0;
            for (int b = 0; b < B; b++) flux += std::max(0.f, d->bands[(size_t)f * B + b] - d->bands[(size_t)(f - 1) * B + b]);
            d->onset[f] = (float)(flux / B);
        }
        const float onsetRef = std::max(1e-6f, percentile(d->onset, 0.99));
        for (float& v : d->onset) v = std::min(1.f, v / onsetRef);
        // Beats: local peaks clearly above the surrounding half second.
        const int w = std::max(2, fps / 2), gap = std::max(1, (int)std::lround(0.2 * fps));
        int last = -gap;
        for (int f = 0; f < n; f++) {
            const int a = std::max(0, f - w), e = std::min(n, f + w + 1);
            double mean = 0;
            for (int i = a; i < e; i++) mean += d->onset[i];
            mean /= (e - a);
            const float v = d->onset[f];
            bool peakHere = v > 0.2f && v > mean + 0.1;
            for (int k = 1; k <= 2 && peakHere; k++) {
                if (f - k >= 0 && d->onset[f - k] > v) peakHere = false;
                if (f + k < n && d->onset[f + k] >= v) peakHere = false;
            }
            if (peakHere && f - last >= gap) {
                d->beats.push_back(f - d->shift);
                last = f;
            }
        }
        // Tempo: the strongest repetition of the onset curve between 60 and 200 bpm.
        double best = 0;
        int bestLag = 0;
        const int lagLo = std::max(1, (int)std::floor(60.0 * fps / 200)), lagHi = (int)std::ceil(60.0 * fps / 60);
        std::vector<double> ac(lagHi + 2, 0);
        double meanOnset = 0;
        for (float v : d->onset) meanOnset += v / std::max(1, n);
        for (int lag = lagLo; lag <= lagHi + 1 && lag < n; lag++) {
            double s = 0;
            for (int f = lag; f < n; f++) s += (d->onset[f] - meanOnset) * (d->onset[f - lag] - meanOnset);
            ac[lag] = s / (n - lag);
        }
        // Rhythms repeat at multiples of the beat too (a clap every other beat), so tempos
        // far from 120 bpm are weighed down, by half an octave's width each way.
        for (int lag = lagLo; lag <= lagHi && lag < n; lag++) {
            const double octaves = std::log2(60.0 * fps / lag / 120), weighed = ac[lag] * std::exp(-0.5 * octaves * octaves / 0.5);
            if (ac[lag] > 0 && weighed > best) { best = weighed; bestLag = lag; }
        }
        if (bestLag > 0) {
            double l = bestLag;
            if (bestLag > lagLo && bestLag + 1 < (int)ac.size()) {       // parabolic refinement
                const double y0 = ac[bestLag - 1], y1 = ac[bestLag], y2 = ac[bestLag + 1], den = y0 - 2 * y1 + y2;
                if (den < 0) l += 0.5 * (y0 - y2) / den;
            }
            d->bpm = 60.0 * fps / l;
        }
        for (int b = 0; b < B; b++) envelope(d->bands, B, b, n, o.attack, o.release);
        envelope(d->level, 1, 0, n, o.attack, o.release);
        d_ = d;
    });
    return *d_;
}

namespace {
// Linear interpolation of series[f * stride + offset] at local frame t; 0 outside.
float sample(const std::vector<float>& v, int n, size_t stride, size_t offset, double x) {
    if (x < 0 || x > n - 1 || n == 0) return 0;
    const int i = (int)std::floor(x);
    const float u = (float)(x - i), a = v[(size_t)i * stride + offset];
    return i + 1 < n ? a + (v[(size_t)(i + 1) * stride + offset] - a) * u : a;
}
}

float Analysis::level(double t) const { const Data& d = data(); return sample(d.level, d.n, 1, 0, t + d.shift); }
float Analysis::band(int i, double t) const {
    const Data& d = data();
    if (i < 0 || i >= d.bandCount) return 0;
    return sample(d.bands, d.n, d.bandCount, i, t + d.shift);
}
std::vector<float> Analysis::spectrum(double t) const {
    const Data& d = data();
    std::vector<float> out(d.bandCount);
    for (int b = 0; b < d.bandCount; b++) out[b] = sample(d.bands, d.n, d.bandCount, b, t + d.shift);
    return out;
}
float Analysis::energy(double t, float loHz, float hiHz) const {
    const Data& d = data();
    double sum = 0;
    int count = 0;
    for (int b = 0; b < d.bandCount; b++)
        if (d.hi[b] > loHz && d.lo[b] < hiHz) { sum += sample(d.bands, d.n, d.bandCount, b, t + d.shift); count++; }
    return count ? (float)(sum / count) : 0.f;
}
float Analysis::onset(double t) const { const Data& d = data(); return sample(d.onset, d.n, 1, 0, t + d.shift); }
const std::vector<double>& Analysis::beats() const { return data().beats; }
bool Analysis::beat(int t) const {
    const auto& b = beats();
    const auto it = std::lower_bound(b.begin(), b.end(), (double)t);
    return it != b.end() && *it < t + 1;
}
int Analysis::beatCount(double t) const {
    const auto& b = beats();
    return (int)(std::upper_bound(b.begin(), b.end(), t) - b.begin());
}
float Analysis::pulse(double t, float decay) const {
    const auto& b = beats();
    const auto it = std::upper_bound(b.begin(), b.end(), t);
    if (it == b.begin()) return 0;
    const double since = t - *(it - 1);
    if (decay <= 0 || since >= decay) return 0;
    const float k = (float)(1 - since / decay);
    return k * k;
}
double Analysis::bpm() const { return data().bpm; }
int Analysis::frames() const { return data().n; }

std::vector<float> Analysis::waveform(double t, int points, double window) const {
    const Data& d = data();
    std::vector<float> out(std::max(0, points), 0.f);
    const double centre = (t + d.shift + 0.5) * d.hop, span = window * d.hop;
    for (int i = 0; i < points; i++) {
        const double s = centre - span / 2 + span * (i + 0.5) / points;
        if (s < 0 || s >= d.mono.size() - 1) continue;
        const size_t k = (size_t)s;
        const float u = (float)(s - k);
        out[i] = (d.mono[k] + (d.mono[k + 1] - d.mono[k]) * u) / d.peak;
    }
    return out;
}

AnalysisPtr analyze(const std::string& file, Options o) {
    static std::mutex m;
    static std::map<std::string, AnalysisPtr> cache;
    char key[256];
    snprintf(key, sizeof key, "|%d|%.6f|%d|%g|%g|%g|%g|%g", o.fps, o.startFrom, o.bands, o.minHz, o.maxHz, o.range, o.attack, o.release);
    std::lock_guard<std::mutex> lock(m);
    AnalysisPtr& a = cache[file + key];
    if (!a) a = std::make_shared<const Analysis>(file, o);
    return a;
}
AnalysisPtr analyze(std::vector<float> stereo, int rate, Options o) {
    return std::make_shared<const Analysis>(std::move(stereo), rate, o);
}

// ---- drawing

void bars(const Ctx& c, Rectangle box, const std::vector<float>& values, Color color, float gap, bool mirror, float radius) {
    const size_t n = values.size();
    if (!n) return;
    const float slot = box.width / n, w = slot * (1 - std::clamp(gap, 0.f, 0.95f));
    for (size_t i = 0; i < n; i++) {
        const float h = std::max(w * 0.5f, std::clamp(values[i], 0.f, 1.f) * box.height);
        const float x = box.x + slot * i + (slot - w) / 2, y = mirror ? box.y + (box.height - h) / 2 : box.y + box.height - h;
        dr::roundRect(c, x, y, w, h, std::min(radius, w / 2), color);
    }
}

void wave(const Ctx& c, Rectangle box, const std::vector<float>& samples, Color color, float thickness) {
    const size_t n = samples.size();
    if (n < 2) return;
    std::vector<Vector2> pts(n);
    for (size_t i = 0; i < n; i++)
        pts[i] = { box.x + box.width * i / (n - 1), box.y + box.height / 2 - std::clamp(samples[i], -1.f, 1.f) * box.height / 2 };
    dr::polyStroke(c, pts.data(), (int)n, false, thickness, color);
}

void radial(const Ctx& c, Vector2 centre, float radius, float length, const std::vector<float>& values, Color color,
            float width, float turn) {
    const size_t n = values.size();
    for (size_t i = 0; i < n; i++) {
        const double a = (turn + 360.0 * i / n) * PI / 180;
        const Vector2 dir{ (float)std::cos(a), (float)std::sin(a) };
        const float l = std::max(width * 0.5f, std::clamp(values[i], 0.f, 1.f) * length);
        dr::strokeSeg(c, { centre.x + dir.x * radius, centre.y + dir.y * radius },
                      { centre.x + dir.x * (radius + l), centre.y + dir.y * (radius + l) }, width, color);
    }
}

} // namespace aud
} // namespace rmtn
