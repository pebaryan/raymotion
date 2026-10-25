// rmtn/subtitles.hpp -- subtitles (sub::) and text-to-speech voice-over (tts::).
//
// Burn an SRT or WebVTT file into the video:
//
//     sub::Track track = sub::load("voiceover.srt");
//     addRoot(c.roots, 0, c.duration)->fn = sub::captions(track);
//
// Time scenes to what is said: the frame where a phrase is spoken, and scene spans from
// one phrase to the next.
//
//     const int fps = 30;
//     auto spans = track.spans({ "First,", "Then the optimizer", "Finally" }, fps);
//     addRoot(c.roots, spans[1].from, spans[1].length)->fn = ...;
//     c.duration = track.frames(fps) + 30;
//
// Or speak a script with any text-to-speech engine and get the audio, the cues and an
// .srt sidecar from it (see tts:: below).
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "timeline.hpp"
#include "layout.hpp"

namespace rmtn {
namespace sub {

struct Cue {
    double start = 0, end = 0;      // seconds
    std::string text;               // lines joined by '\n', markup removed
    bool italic = false;            // the cue was wrapped in <i>...</i>
};

// A subtitle track: cues in time order.
struct Track {
    std::vector<Cue> cues;

    double seconds() const;                         // the end of the last cue
    int frames(int fps) const;                      // seconds() in frames, rounded up
    // The cue showing at `seconds`, or nullptr between cues.
    const Cue* at(double seconds) const;
    // When `phrase` starts being spoken: its cue's start plus its share of the cue's
    // characters, so a phrase in the middle of a long cue lands mid-cue. Searches cues
    // from `after` seconds, case-insensitively and ignoring punctuation and line breaks.
    // Throws std::runtime_error when the phrase is not in the track.
    double when(const std::string& phrase, double after = 0) const;
    int frameOf(const std::string& phrase, int fps, double after = 0) const;
    // Scenes keyed to phrases: span i runs from phrase i (the first from frame 0) to the
    // next phrase, and the last to the end of the track plus `tail` frames.
    struct Span { int from = 0, length = 0; };
    std::vector<Span> spans(const std::vector<std::string>& phrases, int fps, int tail = 0) const;
    // Every cue moved by `seconds` (a voice-over that starts later than frame 0).
    Track shifted(double seconds) const;
};

// Parses SRT or WebVTT (detected from the text): BOM, CRLF, comma or dot milliseconds,
// VTT cue settings and minutes-only times. Tags such as <b>, <font> and {\an8} are
// removed; <i> marks the cue italic. Throws std::runtime_error on a malformed time.
Track parse(const std::string& text);
// Reads and parses a file (resolved like media::resolve).
Track load(const std::string& file);
std::string toSrt(const Track& track);
bool writeSrt(const std::string& path, const Track& track);

// How burned-in captions look. Sizes are in composition px.
struct Style {
    TextStyle text;                 // font (default fonts::ui(true)), size, colour, line height
    float maxWidth = 1500;          // lines wrap to this width
    float bottom = 90;              // gap between the frame's bottom and the last line
    Align align = Align::Center;    // Start/End put the block at the left/right margin
    float margin = 120;             // side margin for Start/End
    Color outline{ 0, 0, 0, 230 };  // drawn around each glyph (alpha 0: none)
    float outlineWidth = 3;
    Color box{ 0, 0, 0, 0 };        // a rounded backing box (alpha 0: none)
    float boxPadding = 18, boxRadius = 12;
    float fade = 4;                 // frames of fade in and out
    // Karaoke: words already spoken in `color`, the rest in `upcoming`, timed by their
    // share of the cue's characters. Alpha 0 turns it off.
    Color upcoming{ 0, 0, 0, 0 };
};
Style defaultStyle();               // white with a dark outline, centred at the bottom

// A node fn that draws the cue showing at the node's local frame (frame / fps seconds),
// placed in the composition's frame.
std::function<void(const Ctx&, int)> captions(Track track, Style style = defaultStyle());
// Draws `cue` as of `seconds` (for your own fns).
void draw(const Ctx& c, const Cue& cue, double seconds, const Style& style = defaultStyle());

} // namespace sub

// ---------------------------------------------------------------- text to speech
// Any engine that can be run from a shell: the command gets {textfile} (a UTF-8 file with
// the line) and {out} (the WAV to write), e.g.
//   piper --model en_US-amy-medium.onnx --output_file {out} < {textfile}
//   python qwen_clone.py --text-file {textfile} --out {out}
// Takes are cached in `cacheDir` by a hash of the command and the text, so only new or
// changed lines are spoken again.
namespace tts {

struct Voice {
    std::string command;
    std::string cacheDir = "out/tts";
    double wordsPerSecond = 2.5;    // for estimates before the audio exists
    // The operating system's voice: Windows SAPI (PowerShell), macOS `say`, else espeak-ng.
    static Voice system();
};

struct Line {
    std::string text;
    std::string file;               // the cached WAV ("" until generated)
    double start = 0;               // seconds from the script's start
    double seconds = 0;             // measured, or estimated from the words
    bool spoken = false;            // the file exists and `seconds` is measured
};

// Lines spoken one after another with `gap` seconds between them.
class Script {
public:
    Script(Voice voice, std::vector<std::string> lines, double gap = 0.5, double lead = 0.3);
    // Speaks the lines whose takes are not cached yet (call it from Comp::assets; it runs
    // the engine), then measures every take. Throws when the engine fails on a line.
    void generate();
    // Measures cached takes without running the engine (call it from Comp::build, so the
    // layout follows the real durations whenever they exist). Until then, durations are
    // estimated from the words.
    void measure();
    const std::vector<Line>& lines() const { return lines_; }
    double seconds() const;         // lead + lines + gaps
    int frames(int fps) const;
    sub::Track track() const;       // a cue per line, for captions and scene timing
    // Plays every spoken line under `node` at its start (node-local).
    void attach(TNode* node, int fps, float volume = 1) const;
private:
    void layout();
    Voice voice_;
    std::vector<Line> lines_;
    double gap_, lead_;
};

} // namespace tts
} // namespace rmtn
