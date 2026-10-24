// procgen.hpp -- procedural asset helpers shared by the showcase films: value noise, a CPU
// pixel canvas for generated textures, and a small offline synthesiser for scores.
#pragma once
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace rmtn {
namespace procgen {

// ------------------------------------------------------------------ noise
inline float vnoise(float x, float y, unsigned long long seed) {
    const float fx = std::floor(x), fy = std::floor(y);
    const long long ix = (long long)fx, iy = (long long)fy;
    auto h = [seed](long long a, long long b) { return (float)rnd01(seed, (unsigned long long)(a * 73856093LL ^ b * 19349663LL)); };
    float ux = x - fx, uy = y - fy;
    ux = ux * ux * (3 - 2 * ux); uy = uy * uy * (3 - 2 * uy);
    const float a = h(ix, iy), b = h(ix + 1, iy), c = h(ix, iy + 1), d = h(ix + 1, iy + 1);
    return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
}
inline float fbm(float x, float y, unsigned long long seed, int octaves = 5) {
    float sum = 0, amp = 0.5f, norm = 0;
    for (int i = 0; i < octaves; i++) {
        sum += amp * vnoise(x, y, seed + i * 101);
        norm += amp; amp *= 0.5f; x *= 2.03f; y *= 2.03f;
    }
    return sum / norm;
}
inline float smooth(float a, float b, float x) {
    float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}

// ------------------------------------------------------------------ pixel canvas
// A CPU RGBA buffer with the few raster ops the texture generators need.
struct Pix {
    int w, h;
    std::vector<Color> px;
    Pix(int w_, int h_, Color fill) : w(w_), h(h_), px((size_t)w_ * h_, fill) {}
    Color& at(int x, int y) { return px[(size_t)y * w + x]; }
    void blend(int x, int y, Color c, float a) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        a *= c.a / 255.f;
        Color& d = at(x, y);
        d = { (unsigned char)(d.r + (c.r - d.r) * a), (unsigned char)(d.g + (c.g - d.g) * a),
              (unsigned char)(d.b + (c.b - d.b) * a), 255 };
    }
    void rect(int x, int y, int rw, int rh, Color c, float a = 1) {
        for (int j = y; j < y + rh; j++) for (int i = x; i < x + rw; i++) blend(i, j, c, a);
    }
    void disc(float cx, float cy, float r, Color c, float a = 1) {
        for (int j = (int)(cy - r - 1); j <= (int)(cy + r + 1); j++)
            for (int i = (int)(cx - r - 1); i <= (int)(cx + r + 1); i++) {
                float d = std::sqrt((i + 0.5f - cx) * (i + 0.5f - cx) + (j + 0.5f - cy) * (j + 0.5f - cy));
                blend(i, j, c, a * std::clamp(r - d + 0.5f, 0.f, 1.f));
            }
    }
    void line(float x0, float y0, float x1, float y1, float wdt, Color c) {
        const int n = (int)(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0)) + 1);
        for (int i = 0; i <= n; i++) disc(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, wdt / 2, c);
    }
    Image image() const {          // a copy the caller (or raylib) owns
        Image img{};
        img.width = w; img.height = h; img.mipmaps = 1; img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        img.data = MemAlloc((unsigned)(px.size() * sizeof(Color)));
        std::memcpy(img.data, px.data(), px.size() * sizeof(Color));
        return img;
    }
    void save(const std::string& path) const {
        Image img = image();
        ExportImage(img, path.c_str());
        UnloadImage(img);
    }
};

// ------------------------------------------------------------------ score
// A small offline synthesiser: polyBLEP saws through state-variable filters, noise drums,
// a ping-pong delay and a Schroeder reverb, all summed into stereo at 48 kHz.
struct Svf {                               // Chamberlin state-variable filter
    float low = 0, band = 0;
    float lp(float x, float f, float q, int rate) {
        const float k = 2 * std::sin(PI * std::min(f, rate / 6.f) / rate);
        low += k * band;
        const float high = x - low - q * band;
        band += k * high;
        return low;
    }
    float hp(float x, float f, float q, int rate) { lp(x, f, q, rate); return x - low - q * band; }
    float bp(float x, float f, float q, int rate) { lp(x, f, q, rate); return band; }
};
inline float polyBlep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
}
inline double midiHz(double m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }

struct Score {
    static constexpr int RATE = 48000;
    int n;
    std::vector<float> dryL, dryR, sendL, sendR, delayL, delayR, duck;
    unsigned long long noiseState = 0x1234567ULL;
    explicit Score(double seconds) : n((int)(seconds * RATE)),
        dryL(n), dryR(n), sendL(n), sendR(n), delayL(n), delayR(n), duck(n, 1.f) {}
    float noise() { noiseState ^= noiseState << 13; noiseState ^= noiseState >> 7; noiseState ^= noiseState << 17;
                    return (float)((noiseState >> 11) * (1.0 / 9007199254740992.0)) * 2 - 1; }
    void put(int i, float x, float pan, float rev, float dly) {
        if (i < 0 || i >= n) return;
        const float l = x * std::sqrt(1 - pan) , r = x * std::sqrt(pan);
        dryL[i] += l; dryR[i] += r;
        sendL[i] += l * rev; sendR[i] += r * rev;
        delayL[i] += l * dly; delayR[i] += r * dly;
    }
    // Sustained detuned-saw chord voice with a slow filter swell.
    void pad(double t0, double dur, double midi, float amp, float cutoff, float pan = 0.5f) {
        const int a = (int)(t0 * RATE), len = (int)((dur + 1.2) * RATE);
        Svf f1, f2;
        const double det[3] = { -0.09, 0, 0.11 };
        float ph[3] = { 0.1f, 0.4f, 0.7f };
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            const float env = (float)(std::min(1.0, t / 0.7) * (t > dur ? std::exp(-(t - dur) * 3.5) : 1.0));
            float s = 0;
            for (int o = 0; o < 3; o++) {
                const float dt = (float)(midiHz(midi + det[o]) / RATE);
                ph[o] += dt; if (ph[o] >= 1) ph[o] -= 1;
                s += (2 * ph[o] - 1) - polyBlep(ph[o], dt);
            }
            const float cut = cutoff * (0.75f + 0.25f * (float)std::sin(t * 1.3 + midi));
            s = f2.lp(f1.lp(s / 3, cut, 0.7f, RATE), cut, 0.9f, RATE);
            put(a + k, s * env * amp * duck[std::min(n - 1, a + k)], pan, 0.45f, 0);
        }
    }
    void bass(double t0, double dur, double midi, float amp) {
        const int a = (int)(t0 * RATE), len = (int)((dur + 0.05) * RATE);
        Svf f; float ph = 0;
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            const float dt = (float)(midiHz(midi) / RATE);
            ph += dt; if (ph >= 1) ph -= 1;
            float s = (2 * ph - 1) - polyBlep(ph, dt);
            s += 0.6f * (float)std::sin(TAU * midiHz(midi - 12) * t);
            const float cut = 180 + 1400 * (float)std::exp(-t * 9);
            const float env = (float)(std::min(1.0, t / 0.004) * std::exp(-t * 2.5) * std::min(1.0, (dur + 0.05 - t) / 0.03));
            put(a + k, f.lp(s, cut, 0.6f, RATE) * env * amp, 0.5f, 0.05f, 0);
        }
    }
    void pluck(double t0, double midi, float amp, float pan, float decay = 5) {
        const int a = (int)(t0 * RATE), len = (int)(1.4 * RATE);
        const double f = midiHz(midi);
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            const float env = (float)(std::min(1.0, t / 0.003) * std::exp(-t * decay));
            const float s = (float)(std::sin(TAU * f * t) + 0.3 * std::sin(TAU * 2 * f * t) * std::exp(-t * 9)
                                    + 0.12 * std::sin(TAU * 3 * f * t) * std::exp(-t * 14));
            put(a + k, s * env * amp * duck[std::min(n - 1, a + k)], pan, 0.3f, 0.45f);
        }
    }
    void bell(double t0, double midi, float amp, float pan) {
        const int a = (int)(t0 * RATE), len = (int)(4.5 * RATE);
        const double f = midiHz(midi), parts[4] = { 1, 2.76, 5.4, 8.93 }, gains[4] = { 1, 0.45, 0.25, 0.12 };
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            double s = 0;
            for (int p = 0; p < 4; p++) s += gains[p] * std::sin(TAU * f * parts[p] * t) * std::exp(-t * (0.9 + p * 0.9));
            put(a + k, (float)(s * std::min(1.0, t / 0.002)) * amp, pan, 0.6f, 0.25f);
        }
    }
    void kick(double t0, float amp) {
        const int a = (int)(t0 * RATE), len = (int)(0.5 * RATE);
        double ph = 0;
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            ph += TAU * (44 + 120 * std::exp(-t * 32)) / RATE;
            const float s = (float)(std::sin(ph) * std::exp(-t * 7) + 0.3 * std::exp(-t * 300) * noise());
            put(a + k, s * amp, 0.5f, 0.02f, 0);
            if (a + k < n) duck[a + k] = std::min(duck[a + k], 1 - 0.55f * (float)std::exp(-t * 7));
        }
    }
    void clap(double t0, float amp) {
        const int a = (int)(t0 * RATE), len = (int)(0.3 * RATE);
        Svf f;
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            const double burst = t < 0.03 ? (std::fmod(t, 0.01) < 0.004 ? 1.0 : 0.3) : std::exp(-(t - 0.03) * 18);
            put(a + k, f.bp(noise(), 1300, 0.5f, RATE) * (float)burst * amp, 0.55f, 0.35f, 0);
        }
    }
    void hat(double t0, float amp, float pan) {
        const int a = (int)(t0 * RATE), len = (int)(0.08 * RATE);
        Svf f;
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            put(a + k, f.hp(noise(), 7500, 0.6f, RATE) * (float)std::exp(-t * 60) * amp, pan, 0.08f, 0);
        }
    }
    // A bitcrushed square stab at a random-ish pitch: the "error" sound.
    void glitch(double t0, float amp, int seed) {
        const int a = (int)(t0 * RATE), len = (int)(0.22 * RATE);
        float held = 0;
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            const double f = 110 * std::pow(2.0, std::floor(rnd01(91, seed * 7 + (int)(t * 40)) * 24) / 12);
            if (k % 12 == 0) held = std::sin(TAU * f * t) > 0 ? 1.f : -1.f;
            put(a + k, held * (float)std::exp(-t * 14) * amp, 0.3f + 0.4f * (float)rnd01(92, seed), 0.15f, 0.2f);
        }
    }
    void tick(double t0, float amp) {
        const int a = (int)(t0 * RATE), len = (int)(0.03 * RATE);
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            put(a + k, (float)(std::sin(TAU * 3200 * t) * std::exp(-t * 200)) * amp, 0.7f, 0.1f, 0);
        }
    }
    // Filtered noise sweeping up from t0 to t1, cut dead at t1 (the drop).
    void riser(double t0, double t1, float amp) {
        const int a = (int)(t0 * RATE), len = (int)((t1 - t0) * RATE);
        Svf f;
        for (int k = 0; k < len; k++) {
            const double u = (double)k / len;
            const float cut = 250 * (float)std::pow(40.0, u);
            put(a + k, f.bp(noise(), cut, 0.25f, RATE) * (float)(u * u) * amp, 0.5f + 0.3f * (float)std::sin(u * 30), 0.4f, 0);
        }
    }
    void impact(double t0, float amp) {
        const int a = (int)(t0 * RATE), len = (int)(3.0 * RATE);
        double ph = 0;
        Svf f;
        for (int k = 0; k < len; k++) {
            const double t = (double)k / RATE;
            ph += TAU * (30 + 60 * std::exp(-t * 6)) / RATE;
            const float boom = (float)(std::sin(ph) * std::exp(-t * 1.6));
            const float crash = f.hp(noise(), 3000, 0.7f, RATE) * (float)std::exp(-t * 2.2);
            put(a + k, (boom + 0.35f * crash) * amp, 0.5f, 0.5f, 0);
        }
    }

    std::vector<float> mix() {
        // Ping-pong delay, 3/8 of a beat at 120 bpm.
        const int dl = (int)(0.375 * RATE);
        std::vector<float> outL(dryL), outR(dryR);
        std::vector<float> echoL(n), echoR(n);
        for (int i = 0; i < n; i++) {
            const float inL = delayL[i] + (i >= dl ? echoR[i - dl] * 0.42f : 0);
            const float inR = delayR[i] + (i >= dl ? echoL[i - dl] * 0.42f : 0);
            echoL[i] = inR; echoR[i] = inL;                       // swap sides each repeat
            outL[i] += echoL[i] * 0.5f; outR[i] += echoR[i] * 0.5f;
            sendL[i] += echoL[i] * 0.3f; sendR[i] += echoR[i] * 0.3f;
        }
        // Schroeder reverb: four damped combs into two allpasses, per side.
        auto reverb = [&](const std::vector<float>& in, int spread) {
            const int combs[4] = { 1557, 1617, 1491, 1422 }, alls[2] = { 556, 441 };
            std::vector<float> wet(n, 0.f);
            for (int c : combs) {
                const int len = (c + spread) * RATE / 44100;
                std::vector<float> buf(len, 0.f);
                float store = 0;
                for (int i = 0, p = 0; i < n; i++, p = (p + 1) % len) {
                    const float y = buf[p];
                    store = y * 0.7f + store * 0.3f;
                    buf[p] = in[i] * 0.015f + store * 0.86f;
                    wet[i] += y;
                }
            }
            for (int a : alls) {
                const int len = (a + spread) * RATE / 44100;
                std::vector<float> buf(len, 0.f);
                for (int i = 0, p = 0; i < n; i++, p = (p + 1) % len) {
                    const float b = buf[p];
                    buf[p] = wet[i] + b * 0.5f;
                    wet[i] = b - wet[i];
                }
            }
            return wet;
        };
        const std::vector<float> wl = reverb(sendL, 0), wr = reverb(sendR, 23);
        std::vector<float> stereo((size_t)n * 2);
        float peak = 0;
        for (int i = 0; i < n; i++) {
            const float l = std::tanh((outL[i] + wl[i] * 3.2f) * 0.9f), r = std::tanh((outR[i] + wr[i] * 3.2f) * 0.9f);
            stereo[2 * i] = l; stereo[2 * i + 1] = r;
            peak = std::max({ peak, std::fabs(l), std::fabs(r) });
        }
        const int fade = RATE * 3;
        for (int i = 0; i < n; i++) {
            const float g = (peak > 0 ? 0.9f / peak : 1) * std::min(1.f, (float)(n - i) / fade);
            stereo[2 * i] *= g; stereo[2 * i + 1] *= g;
        }
        return stereo;
    }
};

} // namespace procgen
} // namespace rmtn
