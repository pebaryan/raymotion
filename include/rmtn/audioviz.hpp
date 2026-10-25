// rmtn/audioviz.hpp -- audio-reactive animation (aud::): loudness, spectrum, beats and the
// waveform of a music file, per frame, to drive anything.
//
//     auto music = aud::analyze("music.ogg");                 // analysed on first use
//     media::audio(addRoot(c.roots, 0, c.duration), "music.ogg");
//     node->up = [music](int t, NodeProps& p) {
//         p.scale = 1 + 0.15f * music->pulse(frameTime(t));   // kick on every beat
//     };
//     node->fn = [music](const Ctx& cx, int t) {
//         aud::bars(cx, { 160, 700, 1600, 260 }, music->spectrum(frameTime(t)), TEAL);
//     };
//
// The whole file is analysed once (cached per file and options) and every query reads the
// tables, so any frame renders alone and the same each time. Times are frames of the
// node's local timeline (fractional is fine, so motion blur sees in-between values);
// frame 0 is `startFrom` seconds into the file. Values are 0..1 relative to the track's
// own loud passages, so quiet and loud recordings drive animations alike.
#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "timeline.hpp"

namespace rmtn {
namespace aud {

struct Options {
    int fps = 30;                   // the composition's frame rate
    double startFrom = 0;           // seconds into the file at frame 0
    int bands = 32;                 // spectrum bands, log-spaced...
    float minHz = 30, maxHz = 16000;   // ...over this range
    float range = 30;               // dB below the loud passages that map to 0
    float attack = 1, release = 5;  // smoothing of level and bands, frames (0: none)
};

class Analysis {
public:
    Analysis(std::string file, Options o);
    Analysis(std::vector<float> stereo, int rate, Options o);   // already decoded audio

    float level(double t) const;                    // loudness (RMS)
    float band(int i, double t) const;
    std::vector<float> spectrum(double t) const;    // every band, low to high
    // Mean of the bands overlapping [loHz, hiHz): bass ~ (20, 150), highs ~ (4000, 16000).
    float energy(double t, float loHz, float hiHz) const;
    float onset(double t) const;                    // how sharply the sound rises
    // Beats: peaks of the onset curve, at least a fifth of a second apart, as frames.
    const std::vector<double>& beats() const;
    bool beat(int t) const;                         // a beat falls within frame t
    int beatCount(double t) const;                  // beats at or before t
    // 1 on each beat, falling off over `decay` frames (0 before the first beat).
    float pulse(double t, float decay = 8) const;
    double bpm() const;                             // tempo from the beats' rhythm (0: none)
    // `points` mono samples (-1..1) spread over `window` frames centred on t.
    std::vector<float> waveform(double t, int points, double window = 1) const;
    int frames() const;                             // length in frames
    const Options& options() const { return o_; }
    struct Data;
private:
    const Data& data() const;
    std::string file_;
    Options o_;
    mutable std::shared_ptr<Data> d_;
    mutable std::vector<float> pending_;
    mutable int pendingRate_ = 0;
    mutable std::once_flag once_;
};
using AnalysisPtr = std::shared_ptr<const Analysis>;

// Shared per (file, options); the file is decoded and analysed on the first query, so
// compositions can be built before an asset hook has written it.
AnalysisPtr analyze(const std::string& file, Options o = {});
AnalysisPtr analyze(std::vector<float> stereo, int rate, Options o = {});

// ---- drawing (node-local units)
// Vertical bars across `box`, rising from its bottom (or from its middle, both ways, with
// `mirror`); `gap` is the share of each slot left empty.
void bars(const Ctx& c, Rectangle box, const std::vector<float>& values, Color color, float gap = 0.3f,
          bool mirror = false, float radius = 4);
// A waveform line across `box` (samples -1..1 fill its height).
void wave(const Ctx& c, Rectangle box, const std::vector<float>& samples, Color color, float thickness = 3);
// Bars standing out of a circle of `radius`, up to `length` long; `turn` rotates them (degrees).
void radial(const Ctx& c, Vector2 centre, float radius, float length, const std::vector<float>& values, Color color,
            float width = 6, float turn = -90);

} // namespace aud
} // namespace rmtn
