// audioviz_tests.cpp -- aud:: analysis on synthetic audio: a tone's band and loudness,
// silence, beats and tempo of a click track, startFrom, pulses, the waveform, caching by
// file. Pure computation (no window).
#include "rmtn/raymotion.hpp"
#include <cmath>
#include <cstdio>
#include <filesystem>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

constexpr int RATE = 48000;

// `seconds` of stereo audio from f(seconds).
template <class F> static std::vector<float> make(double seconds, F f) {
    const size_t n = (size_t)(seconds * RATE);
    std::vector<float> s(n * 2);
    for (size_t i = 0; i < n; i++) s[2 * i] = s[2 * i + 1] = (float)f((double)i / RATE);
    return s;
}

// A kick-like click (a decaying 60 Hz thump plus a noise tick) every `period` seconds.
static double clicks(double t, double period) {
    const double u = std::fmod(t, period);
    const double noise = std::sin(t * 12345.678) * std::sin(t * 9876.54321);
    return u < 0.15 ? std::exp(-u * 30) * (0.7 * std::sin(2 * PI * 60 * u) + 0.3 * noise) : 0.0;
}

static void tone() {
    // A second of silence, then three of 440 Hz.
    auto a = aud::analyze(make(4, [](double t) { return t < 1 ? 0.0 : 0.5 * std::sin(2 * PI * 440 * t); }), RATE);
    CHECK(a->frames() == 120);
    CHECK(a->level(10) < 0.05f && a->level(60) > 0.9f);
    const auto spec = a->spectrum(60);
    CHECK(spec.size() == 32);
    size_t loudest = 0;
    for (size_t b = 0; b < spec.size(); b++) if (spec[b] > spec[loudest]) loudest = b;
    // Band edges are log-spaced from 30 Hz to 16 kHz: 440 Hz is in band floor(32 * log(440/30) / log(16000/30)).
    CHECK((int)loudest == (int)(32 * std::log(440.0 / 30) / std::log(16000.0 / 30)));
    CHECK(a->energy(60, 400, 500) > 0.8f && a->energy(60, 4000, 8000) < 0.3f);
    // Before frame 0 and past the end of the audio, everything is 0.
    CHECK(a->level(500) == 0 && a->level(-5) == 0);
    // Fractional frames interpolate.
    const float l29 = a->level(29), l30 = a->level(30), mid = a->level(29.5);
    CHECK(mid >= std::min(l29, l30) - 1e-6f && mid <= std::max(l29, l30) + 1e-6f);
    // The waveform is normalised to the track's peak.
    const auto w = a->waveform(60, 200, 2);
    float peak = 0;
    for (float v : w) peak = std::max(peak, std::fabs(v));
    CHECK(w.size() == 200 && peak > 0.9f && peak <= 1.0001f);
    CHECK(a->waveform(10, 50)[25] == 0);
}

static void beats() {
    auto a = aud::analyze(make(8, [](double t) { return clicks(t, 0.5); }), RATE);
    const auto& b = a->beats();
    CHECK(b.size() >= 15 && b.size() <= 17);
    for (size_t i = 1; i < b.size(); i++) CHECK(std::fabs(b[i] - b[i - 1] - 15) <= 1.01);
    CHECK(std::fabs(a->bpm() - 120) < 3);
    if (!b.empty()) {
        const int first = (int)b[0];
        CHECK(a->beat(first) && !a->beat(first + 7));
        CHECK(std::fabs(a->pulse(b[0]) - 1) < 1e-6 && std::fabs(a->pulse(b[0] + 4, 8) - 0.25f) < 1e-4);
        CHECK(a->pulse(b[0] + 9, 8) == 0 && a->pulse(b[0] - 1) == 0);
        CHECK(a->beatCount(b[0]) == 1 && a->beatCount(1000) == (int)b.size());
        CHECK(a->onset(b[0]) > 0.5f);
    }
    // startFrom: frame 0 is a second into the file, so every beat comes 30 frames earlier.
    aud::Options o;
    o.startFrom = 1;
    auto s = aud::analyze(make(8, [](double t) { return clicks(t, 0.5); }), RATE, o);
    CHECK(s->beats().size() == b.size());
    for (size_t i = 0; i < b.size() && i < s->beats().size(); i++) CHECK(std::fabs(s->beats()[i] - (b[i] - 30)) < 1e-9);
    // A slower track.
    auto slow = aud::analyze(make(10, [](double t) { return clicks(t, 0.75); }), RATE);
    CHECK(std::fabs(slow->bpm() - 80) < 3);
}

static void files() {
    const auto path = (std::filesystem::temp_directory_path() / "raymotion-audioviz.wav").string();
    const auto pcm = make(3, [](double t) { return clicks(t, 0.5) + 0.2 * std::sin(2 * PI * 220 * t); });
    CHECK(media::writeWav(path, pcm, RATE));
    auto a = aud::analyze(path), b = aud::analyze(path);
    CHECK(a == b);                                          // shared per file and options
    aud::Options o;
    o.bands = 8;
    auto c = aud::analyze(path, o);
    CHECK(c != a && c->spectrum(20).size() == 8);
    auto d = aud::analyze(pcm, RATE);                      // the 16-bit file analyses like the samples
    CHECK(std::fabs(a->level(40) - d->level(40)) < 0.02f && a->beats().size() == d->beats().size());
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

int main() {
    tone();
    beats();
    files();
    if (failures) { std::fprintf(stderr, "%d audio check(s) failed\n", failures); return 1; }
    std::puts("audioviz tests passed");
    return 0;
}
