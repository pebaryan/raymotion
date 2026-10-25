// captions.cpp -- "captions": a narrated piece timed to its own voice-over. tts:: speaks
// the script with the system voice (cached in out/tts), each scene starts on the phrase
// that introduces it, captions are burned in, one scene lights words up as they are
// spoken, and the timings are saved to out/captions.srt. Without a system voice it falls
// back to estimated timings, silently.
#include "rmtn/raymotion.hpp"
#include <cstdio>

namespace rmtn {
namespace {

const Color BG = mkColor(0x0d1020), PANEL = mkColor(0x171c33), INK = mkColor(0xeef0ff), DIM = mkColor(0x8a91b4),
            AQUA = mkColor(0x47dad8), AMBER = mkColor(0xffc365), ROSE = mkColor(0xff6b8b);

const std::vector<std::string> LINES = {
    "Captions can come from a subtitle file, or from the voice-over itself.",
    "Each scene starts on the phrase that introduces it, so the picture follows the voice.",
    "Words can light up as they are spoken, karaoke style.",
    "And the same timings are saved as an S R T file, ready for any video player.",
};
const std::vector<std::string> PHRASES = { "Captions can come", "Each scene starts", "Words can light up", "And the same timings" };

float ramp(double t, double a, double b) { return (float)interpolate(t, { a, b }, { 0, 1 }, clamped(ease::out(ease::cubic))); }

void heading(const Ctx& cx, double t, const char* title, const char* kicker) {
    const float k = ramp(t, 0, 14);
    Ctx c = cx;
    c.a *= k;
    dr::text(fonts::ui(false), c, kicker, { 120, 96 + 12 * (1 - k) }, 28, 4, AQUA);
    dr::text(fonts::ui(true), c, title, { 120, 136 + 12 * (1 - k) }, 64, 0, INK);
}

// Scene 1: the SRT text appearing cue by cue.
void srtScene(const Ctx& cx, double t, const sub::Track& track, int fps) {
    heading(cx, t, "Subtitles in, captions out", "SRT AND WEBVTT");
    const std::string text = sub::toSrt(track);
    Font mono = code::font(false);
    dr::roundRect(cx, 120, 260, 1680, 540, 18, PANEL);
    float y = 292;
    int cue = 0;
    size_t a = 0;
    while (a < text.size()) {
        size_t e = text.find('\n', a);
        if (e == std::string::npos) e = text.size();
        const std::string line = text.substr(a, e - a);
        if (line.empty()) cue++;
        const float k = ramp(t, 10 + cue * 10, 22 + cue * 10);
        Ctx c = cx;
        c.a *= k;
        const bool timing = line.find("-->") != std::string::npos, number = !line.empty() && line.find_first_not_of("0123456789") == std::string::npos;
        dr::text(mono, c, line.c_str(), { 160 + 30 * (1 - k), y }, 28, 0, timing ? AMBER : number ? DIM : INK);
        y += line.empty() ? 12 : 36;
        a = e + 1;
    }
    (void)fps;
}

// Scene 2: the cues on a timeline, scene starts marked at their phrases, and a playhead.
void timelineScene(const Ctx& cx, double t, double now, const sub::Track& track, const std::vector<sub::Track::Span>& spans,
                   int fps) {
    heading(cx, t, "Scenes follow the voice", "TIMED TO PHRASES");
    const float x0 = 160, x1 = 1760, y = 470, total = (float)track.seconds();
    auto X = [&](double s) { return x0 + (x1 - x0) * (float)(s / total); };
    const float grow = ramp(t, 4, 30);
    dr::roundRect(cx, x0 - 20, y - 70, (x1 - x0 + 40) * grow, 260, 16, PANEL);
    for (size_t i = 0; i < track.cues.size(); i++) {
        const sub::Cue& q = track.cues[i];
        Ctx c = cx;
        c.a *= ramp(t, 10 + 6 * i, 22 + 6 * i);
        dr::roundRect(c, X(q.start), y, X(q.end) - X(q.start), 44, 8, now >= q.start && now < q.end ? AQUA : mkColor(0x2c3458));
        char label[16];
        snprintf(label, sizeof label, "cue %zu", i + 1);
        dr::text(fonts::ui(false), c, label, { X(q.start) + 12, y + 8 }, 24, 0, now >= q.start && now < q.end ? BG : DIM);
    }
    for (size_t i = 0; i < spans.size(); i++) {
        Ctx c = cx;
        c.a *= ramp(t, 30 + 8 * i, 42 + 8 * i);
        const float x = X(spans[i].from / (double)fps);
        dr::rect(c, x - 2, y - 40, 4, 150, AMBER);
        char label[16];
        snprintf(label, sizeof label, "scene %zu", i + 1);
        dr::text(fonts::ui(true), c, label, { x + 10, y + 70 }, 24, 0, AMBER);
    }
    const float px = X(now);
    dr::rect(cx, px - 1.5f, y - 60, 3, 230, ROSE);
    dr::circle(cx, { px, y - 60 }, 9, ROSE);
}

// Scene 3: the current line, big, lighting up word by word.
void karaokeScene(const Ctx& cx, double t, double now, const sub::Track& track) {
    heading(cx, t, "Word by word", "KARAOKE");
    sub::Style big = sub::defaultStyle();
    big.text.size = 92;
    big.text.color = AMBER;
    big.upcoming = withAlpha(INK, 0.35f);
    big.outline = BLANK;
    big.maxWidth = 1500;
    big.bottom = 400;
    if (const sub::Cue* q = track.at(now)) sub::draw(cx, *q, now, big);
}

// Scene 4: the saved file.
void fileScene(const Ctx& cx, double t, const sub::Track& track) {
    heading(cx, t, "Saved for any player", "OUT/CAPTIONS.SRT");
    const float k = ramp(t, 6, 24);
    Ctx c = cx;
    c.a *= k;
    const float x = 760, y = 300 + 30 * (1 - k);
    dr::roundRect(c, x, y, 400, 480, 20, PANEL);
    const Vector2 cut[3] = { { x + 300, y }, { x + 400, y }, { x + 400, y + 100 } };
    const Vector2 fold[3] = { { x + 300, y }, { x + 400, y + 100 }, { x + 300, y + 100 } };
    dr::fillPoly(c, cut, 3, BG);
    dr::fillPoly(c, fold, 3, mkColor(0x2c3458));
    dr::text(fonts::ui(true), c, "SRT", { x + 200, y + 200 }, 120, 0, AQUA, 0.5, 0);
    char info[64];
    snprintf(info, sizeof info, "%zu cues  \xC2\xB7  %.1f s", track.cues.size(), track.seconds());
    dr::text(fonts::ui(false), c, info, { x + 200, y + 360 }, 32, 0, DIM, 0.5, 0);
}

void build(Comp& c, const tts::Script& script) {
    const int fps = c.fps;
    const sub::Track track = script.track();
    const auto spans = track.spans(PHRASES, fps, 45);
    c.duration = spans.back().from + spans.back().length;
    c.roots.clear();
    script.attach(addRoot(c.roots, 0, c.duration), fps, 0.95f);
    const auto scene = [&](size_t i, std::function<void(const Ctx&, double, double)> draw) {
        TNode* n = addRoot(c.roots, spans[i].from, spans[i].length);
        const int from = spans[i].from, length = spans[i].length;
        n->up = [length](int t, NodeProps& p) {
            p.opacity = (float)(interpolate(t, { 0, 10 }, { 0, 1 }, clamped()) * interpolate(t, { length - 10.0, (double)length }, { 1, 0 }, clamped()));
        };
        n->fn = [draw, from, fps](const Ctx& cx, int t) {
            const double ft = frameTime(t);
            draw(cx, ft, (from + ft) / fps);
        };
    };
    scene(0, [track, fps](const Ctx& cx, double t, double) { srtScene(cx, t, track, fps); });
    scene(1, [track, spans, fps](const Ctx& cx, double t, double now) { timelineScene(cx, t, now, track, spans, fps); });
    scene(2, [track](const Ctx& cx, double t, double now) { karaokeScene(cx, t, now, track); });
    scene(3, [track](const Ctx& cx, double t, double) { fileScene(cx, t, track); });
    // Burned-in captions, except while the karaoke scene shows the line itself.
    sub::Style style = sub::defaultStyle();
    style.box = withAlpha(BG, 0.75f);
    style.outline = BLANK;
    style.text.size = 44;
    const sub::Track::Span k = spans[2];
    const auto captions = sub::captions(track, style);
    addRoot(c.roots, 0, c.duration)->fn = [captions, k](const Ctx& cx, int t) {
        if (t < k.from || t >= k.from + k.length) captions(cx, t);
    };
}

} // namespace

void registerCaptions(Registry& out) {
    Comp c;
    c.name = "captions";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    auto script = std::make_shared<tts::Script>(tts::Voice::system(), LINES, 0.45, 0.4);
    c.assets = [script](const Comp&, const Json&) {
        try {
            script->generate();
            sub::writeSrt("out/captions.srt", script->track());
        } catch (const std::exception& e) {
            std::fprintf(stderr, "note: no voice-over (%s); captions follow estimated timings\n", e.what());
        }
    };
    c.build = [script](Comp& comp, const Json&) {
        script->measure();
        build(comp, *script);
    };
    c.duration = 1;                   // set by build
    out.push_back(std::move(c));
}

} // namespace rmtn
