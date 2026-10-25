// audioreactive.cpp -- "audio-reactive": visuals driven by a music track through aud::.
// A generated 124 bpm track (calm intro, drums, a riser and a drop) drives a pulsing disc
// with a radial spectrum, mirrored spectrum bars, an oscilloscope, level meters, the
// detected tempo and a particle ring on every beat.
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <filesystem>

namespace rmtn {
namespace {

const char* TRACK = "out/audio-reactive-assets-v1/track.wav";
constexpr double BPM = 124, SECONDS = 24;
const Color BG = mkColor(0x07080f), AQUA = mkColor(0x47dad8), ROSE = mkColor(0xff5fa2), AMBER = mkColor(0xffc365),
            INK = mkColor(0xeef0ff), DIM = mkColor(0x7d84a8);

void genTrack(const std::string& path) {
    using procgen::Score;
    Score s(SECONDS);
    const double beat = 60 / BPM, bar = 4 * beat;
    const int prog[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 55, 59, 62 }, { 52, 55, 59 } };
    const int roots[4] = { 45, 41, 43, 40 };
    const double drums = 3 * bar, rise = 9 * bar, drop = 10 * bar, end = SECONDS - 2;
    for (int i = 0; i * bar < end; i++) {
        const double t = i * bar;
        const int k = i % 4;
        for (int p : prog[k]) s.pad(t, bar * 0.95, p, 0.06f, t < drums ? 700 : 1500);
        for (int h = 0; h < 8; h++) s.hat(t + h * beat / 2, h % 2 ? 0.05f : 0.03f, h % 2 ? 0.35f : 0.65f);
        if (t < drums || (t >= rise && t < drop)) continue;
        for (int b = 0; b < 4; b++) {
            s.kick(t + b * beat, 0.42f);
            s.bass(t + b * beat, beat * 0.8, roots[k] - (b == 3 ? 5 : 0), 0.2f);
        }
        s.clap(t + beat, 0.13f);
        s.clap(t + 3 * beat, 0.13f);
        for (int e = 0; e < 8; e++) s.pluck(t + e * beat / 2, prog[k][(e * 2) % 3] + 12, 0.03f, e % 2 ? 0.3f : 0.7f);
    }
    s.riser(rise, drop, 0.2f);
    s.impact(drop, 0.5f);
    media::writeWav(path, s.mix(), Score::RATE);
}

// Low to high and back, squared so loud passages keep some contrast.
std::vector<float> mirrored(std::vector<float> v) {
    for (float& x : v) x *= x;
    std::vector<float> out(v.rbegin(), v.rend());
    out.insert(out.end(), v.begin(), v.end());
    return out;
}

} // namespace

void registerAudioReactive(Registry& out) {
    Comp c;
    c.name = "audio-reactive";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.duration = (int)(SECONDS * c.fps);
    c.background = BG;
    c.assets = [](const Comp&, const Json&) {
        if (std::filesystem::exists(TRACK)) return;
        std::filesystem::create_directories(std::filesystem::path(TRACK).parent_path());
        genTrack(TRACK);
    };
    media::audio(addRoot(c.roots, 0, c.duration), TRACK);

    aud::Options wide;
    wide.bands = 48;
    wide.release = 4;
    const auto music = aud::analyze(TRACK, wide);
    aud::Options fine = wide;
    fine.bands = 24;
    fine.release = 7;
    const auto slow = aud::analyze(TRACK, fine);

    // Backdrop: a glow that breathes with the level and flashes on the kick.
    addRoot(c.roots, 0, c.duration)->fn = [music](const Ctx& cx, int t) {
        const double ft = frameTime(t);
        const float glow = 0.35f * music->level(ft) + 0.4f * music->pulse(ft, 10) * music->energy(ft, 30, 140);
        const Color centre = mixC(BG, mkColor(0x2a1840), glow);
        DrawCircleGradient(mapP(cx, { 960, 520 }), 1100 * cx.s, dr::TA(cx, centre), dr::TA(cx, BG));
    };
    // Centre: a disc pulsing on each beat, ringed by the spectrum, and a burst per beat.
    addRoot(c.roots, 0, c.duration)->fn = [music](const Ctx& cx, int t) {
        const double ft = frameTime(t);
        const float pulse = music->pulse(ft, 9);
        const float r = 150 * (1 + 0.14f * pulse + 0.1f * music->level(ft));
        const auto& beats = music->beats();
        const int count = music->beatCount(ft);
        for (int k = std::max(0, count - 2); k < count; k++) {
            fx::Emitter ring = fx::burst(70, { k % 4 == 0 ? AMBER : AQUA, ROSE });
            ring.seed = (unsigned long long)k + 1;
            ring.burstAt = (int)std::floor(beats[k]);
            ring.area = fx::Area::Ring;
            ring.extent = { r, 0 };
            ring.outward = true;
            ring.spread = 20;
            ring.speed = { 6, 14 };
            ring.life = { 18, 30 };
            Ctx at = cx;
            at.o = mapP(cx, { 960, 520 });
            fx::draw(at, ring, ft);
        }
        aud::radial(cx, { 960, 520 }, r + 18, 190, mirrored(music->spectrum(ft)), withAlpha(AQUA, 0.9f), 9);
        dr::circle(cx, { 960, 520 }, r, mixC(mkColor(0x1a1f3a), ROSE, 0.25 + 0.5 * pulse));
        dr::ring(cx, { 960, 520 }, r, 5, withAlpha(INK, 0.7f + 0.3f * pulse));
        char bpm[32];
        snprintf(bpm, sizeof bpm, "%.0f", music->bpm());
        Font bold = fonts::ui(true), light = fonts::ui(false);
        dr::text(bold, cx, bpm, { 960, 488 }, 84, 0, INK, 0.5, 0.5);
        dr::text(light, cx, "BPM", { 960, 556 }, 30, 4, DIM, 0.5, 0.5);
    };
    // Bottom: mirrored spectrum bars; top: the oscilloscope.
    addRoot(c.roots, 0, c.duration)->fn = [slow, music](const Ctx& cx, int t) {
        const double ft = frameTime(t);
        aud::bars(cx, { 360, 880, 1200, 150 }, mirrored(slow->spectrum(ft)), withAlpha(ROSE, 0.85f), 0.35f, true, 6);
        aud::wave(cx, { 360, 60, 1200, 180 }, music->waveform(ft, 300, 1), withAlpha(AMBER, 0.9f), 3);
    };
    // Left: meters for level, bass and treble, and a beat counter.
    addRoot(c.roots, 0, c.duration)->fn = [music](const Ctx& cx, int t) {
        const double ft = frameTime(t);
        const std::pair<const char*, float> meters[] = { { "LEVEL", music->level(ft) },
                                                          { "BASS", music->energy(ft, 30, 160) },
                                                          { "TREBLE", music->energy(ft, 4000, 16000) } };
        Font light = fonts::ui(false), bold = fonts::ui(true);
        for (int i = 0; i < 3; i++) {
            const float y = 380 + i * 110;
            dr::text(light, cx, meters[i].first, { 120, y }, 26, 3, DIM);
            dr::roundRect(cx, 120, y + 40, 260, 14, 7, mkColor(0x1a1f3a));
            dr::roundRect(cx, 120, y + 40, 260 * meters[i].second, 14, 7, i == 1 ? AMBER : AQUA);
        }
        char beats[32];
        snprintf(beats, sizeof beats, "%d", music->beatCount(ft));
        dr::text(light, cx, "BEATS", { 1800, 380 }, 26, 3, DIM, 1, 0);
        dr::text(bold, cx, beats, { 1800, 416 }, 72, 0, music->beat(t) ? AMBER : INK, 1, 0);
    };
    out.push_back(std::move(c));
}

} // namespace rmtn
