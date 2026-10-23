// rmtn/synth.hpp -- procedural chiptune-style WAV generation. No audio assets: notes are sine sums.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <fstream>

namespace rmtn {

struct SongEvent { double t0, dur, freq, amp; bool pluck; };
using Song = std::vector<SongEvent>;

static inline double envAt(const SongEvent& e, double g) {
    double u = (g - e.t0) / e.dur;
    if (u <= 0 || u >= 1) return 0.0;
    if (e.pluck) return std::exp(-4.5 * u);
    double ramp = std::min(u / 0.15, 1.0), tail = std::min((1.0 - u) / 0.35, 1.0);
    return std::min(ramp, tail);
}

static std::vector<float> renderSong(const Song& song, double totalSec, int rate) {
    std::vector<float> out((size_t)(totalSec * rate) + 4, 0.f);
    for (const auto& e : song) {
        size_t a = (size_t)(e.t0 * rate), b = ((size_t)((e.t0 + e.dur) * rate)) + 2;
        for (size_t i = a; i < b && i < out.size(); i++) {
            double g = (double)i / rate;
            double v = envAt(e, g) * e.amp;
            if (v <= 0.002) continue;
            double tau = g - e.t0;
            double f = e.freq;
            double s = 0.62 * std::sin(2*M_PI*f*tau)
                     + 0.26 * std::sin(2*M_PI*(3*f)*tau + 0.7)
                     + 0.12 * std::sin(2*M_PI*(5*f)*tau + 1.9);
            out[i] += (float)(v * s);
        }
    }
    float peak = 0; for (float x : out) if (std::abs(x) > peak) peak = (float)std::abs(x);
    if (peak > 0) { float k = 0.85f / peak; for (auto& x : out) x *= k; }
    return out;
}

static bool writeWavMono(const std::string& path, int rate, const std::vector<float>& f) {
    std::vector<int16_t> s(f.size());
    for (size_t i = 0; i < f.size(); i++) s[i] = (int16_t)(f[i] * 32767.0f);
    std::ofstream o(path, std::ios::binary);
    if (!o) return false;
    uint32_t dataBytes = (uint32_t)(s.size() * 2), riff = 36 + dataBytes;
    auto w32 = [&o](uint32_t v) { char b[4] = { (char)v,(char)(v>>8),(char)(v>>16),(char)(v>>24) }; o.write(b,4); };
    auto w16 = [&o](uint16_t v) { char b[2] = { (char)v,(char)(v>>8) }; o.write(b,2); };
    o.write("RIFF", 4); w32(riff); o.write("WAVEfmt ", 8); w32(16);
    w16(1); w16(1); w32(rate); w32(rate * 2); w16(2); w16(16);
    o.write("data", 4); w32(dataBytes);
    o.write((const char*)s.data(), dataBytes);
    return (bool)o;
}

} // namespace rmtn
