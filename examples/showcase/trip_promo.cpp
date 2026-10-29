// trip_promo.cpp -- "trip_promo": the vertical promo, three scenes back to back.
//
//   0. THE FOOTAGE. The MatteAnyone test clip (480x742, 30 fps, 393 frames) as it is, full
//      frame, with its own soundtrack.
//   1. INTRO. "code / C++ / with / Raymotion / [logo] / to / make / this" -- one word per
//      line, centred, popping in one after another with the raymotion logo animation in
//      its own line.
//   2. THE TRIPS GLOBE. The Prabowo itinerary (trips_v fit: 1080x1670, 180 s), faded in.
//   3. END CARD. The GitHub badge "pebaryan/raymotion" and "2026".
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <cmath>

namespace rmtn {
TNode* addTripsScene(TransitionSeries& ts, int w, int h);   // trips.cpp
void addTripsSfx(TNode* node);                              // trips.cpp

namespace {

constexpr int W = 1080, H = 1670;
const Color BG = mkColor(0x05070f), INK = mkColor(0xeef0ff), DIM = mkColor(0x8a91b4), AQUA = mkColor(0x47dad8),
            GOLDEN = mkColor(0xffc857);

float smooth01(float x) {
    x = std::clamp(x, 0.f, 1.f);
    return x * x * (3 - 2 * x);
}

unsigned hash(unsigned x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x;
}
float rnd(unsigned a, unsigned b = 0) { return (hash(a * 9781u + b * 6271u + 17u) & 0xffffff) / 16777216.f; }

// The same dark sky as the globe scenes, so the cuts read as one film.
void sky(const Ctx& cx, int t) {
    dr::rect(cx, 0, 0, (float)W, (float)H, BG);
    for (unsigned i = 0; i < 420; i++) {
        const float x = rnd(i, 1) * W, y = rnd(i, 2) * H, s = 0.6f + 1.6f * rnd(i, 3) * rnd(i, 4);
        const float tw = 0.55f + 0.45f * std::sin(t * 0.05f + i);
        dr::circle(cx, { x, y }, s, withAlpha(mixC(INK, AQUA, rnd(i, 5) * 0.5f), (0.25f + 0.6f * rnd(i, 6)) * tw));
    }
}

TNode* place(TNode* parent, int from, int dur, const char* name, const char* props, Vector2 at) {
    TNode* n = prefab::use(parent, from, dur, name, Json::parse(props));
    n->props.pos = at;
    return n;
}

} // namespace

void registerTripPromo(Registry& out) {
    Comp comp;
    comp.name = "trip_promo";
    comp.w = W; comp.h = H; comp.fps = 30;
    comp.background = BG;
    TransitionSeries ts(comp.roots, (float)W, (float)H);

    // ---------------------------------------------------------------- 0. the footage
    // Same aspect as the frame (480/742 = 0.6469, 1080/1670 = 0.6467), so Cover crops
    // under a pixel and the clip sits edge to edge. media::video brings its soundtrack in.
    const int CLIP = 393;
    TNode* s0 = ts.add(CLIP);
    media::VideoOptions vo;
    vo.style.fit = media::Fit::Cover;
    vo.style.align = { 0.5f, 0.5f };
    media::video(s0, "D:/downloads/UsB-WWXP06vrya3k.mp4", { 0, 0, (float)W, (float)H }, vo);

    // ---------------------------------------------------------------- 1. intro
    ts.transition(Transition::fade(16));
    const int INTRO = 176;
    TNode* s1 = ts.add(INTRO);
    s1->add(0, INTRO)->fn = [](const Ctx& cx, int t) { sky(cx, t); };

    // One word per line, centred. The logo takes the fifth line, in its own node, so the
    // prefab's draw-on plays inside the stack. y runs down from the block's top.
    struct Line {
        const char* text;
        int at;
        Color color;
        bool logo;
        float y;      // filled in below
    };
    Line lines[] = {
        { "code",      6, INK,    false, 0 },
        { "C++",       24, AQUA,   false, 0 },
        { "with",      42, INK,    false, 0 },
        { "Raymotion", 60, GOLDEN, false, 0 },
        { "",          80, INK,    true,  0 },
        { "to",        108, INK,   false, 0 },
        { "make",      124, INK,   false, 0 },
        { "this",      140, AQUA,  false, 0 },
    };
    const float wordSz = 96.f, lineGap = 24.f, logoH = 208.f;
    float total = 0;
    for (const Line& l : lines) total += (l.logo ? logoH : wordSz * 1.16f) + lineGap;
    total -= lineGap;
    float y = (H - total) / 2.f;
    for (Line& l : lines) { l.y = y; y += (l.logo ? logoH : wordSz * 1.16f) + lineGap; }

    s1->add(0, INTRO)->fn = [lines, wordSz](const Ctx& cx, int t) {
        for (const Line& l : lines) {
            if (l.logo) continue;
            const int lt = t - l.at;
            if (lt < 0) continue;
            const float k = smooth01(lt / 12.f);
            Ctx c = cx;
            c.a *= k;
            dr::text(fonts::ui(true), c, l.text, { W / 2.f, l.y + 26.f * (1 - k) }, wordSz, 0, l.color, 0.5f, 0);
        }
    };
    const float logoY = lines[4].y + logoH / 2.f;
    place(s1, 80, INTRO - 80, "raymotion-logo",
          R"({"size": 200, "wordmark": true, "layout": "horizontal", "in": "draw", "length": 42, "accent": "#47dad8", "delay": 0})",
          { W / 2.f, logoY });

    // ---------------------------------------------------------------- 2. the trips globe
    ts.transition(Transition::fade(20));
    TNode* tripsNode = addTripsScene(ts, W, H);

    // ---------------------------------------------------------------- 3. end card
    ts.transition(Transition::fade(20));
    const int OUTRO = 130;
    TNode* s3 = ts.add(OUTRO);
    s3->add(0, OUTRO)->fn = [](const Ctx& cx, int t) { sky(cx, t); };
    place(s3, 6, OUTRO - 6, "social-github",
          R"({"size": 210, "style": "badge", "shape": "rounded", "handle": "pebaryan/raymotion", "in": "pop", "delay": 4})",
          { W / 2.f, 700.f });
    s3->add(24, OUTRO - 24)->fn = [](const Ctx& cx, int t) {
        const float k = smooth01((t - 24) / 14.f);
        Ctx c = cx;
        c.a *= k;
        dr::text(fonts::ui(true), c, "2026", { W / 2.f, 880.f + 20.f * (1 - k) }, 150, 2, INK, 0.5f, 0);
        dr::text(code::font(false), c, "C++ / raylib / ffmpeg", { W / 2.f, 1080.f }, 30, 0, DIM, 0.5f, 0);
    };

    comp.duration = ts.end();

    // ---------------------------------------------------------------- sound
    // The bed runs the whole cut (Am-F-C-G arpeggio, 76 BPM) and ducks while the footage's
    // own soundtrack plays. One-shots mark the cuts, the intro words and the end card.
    const std::string SFX = "D:/code/raymotion/assets/sfx/";
    auto sfx = [&SFX](TNode* parent, int at, int dur, const char* file, float vol, double rate) {
        TNode* n = parent->add(at, dur);
        media::AudioOptions o;
        o.volume = vol;
        o.playbackRate = rate;
        media::audio(n, SFX + file, o);
    };
    sfx(s0, CLIP - 34, 34, "whoosh.wav", 0.45f, 1.0);          // footage -> intro
    int wi = 0;
    for (const Line& l : lines)
        if (!l.logo) sfx(s1, l.at - 3, 14, "pop.wav", 0.34f, 0.84 + 0.06 * wi++);
    sfx(s1, 78, 44, "whoosh.wav", 0.28f, 0.82);                // the logo draws on
    sfx(s1, INTRO - 30, 30, "whoosh.wav", 0.50f, 0.94);        // intro -> globe
    addTripsSfx(tripsNode);                                   // wind, landings, takeoffs
    sfx(tripsNode, 2, 46, "boom.wav", 0.55f, 1.0);
    sfx(tripsNode, 5411 - 26, 26, "whoosh.wav", 0.45f, 0.96);  // globe -> end card
    sfx(s3, 2, 46, "boom.wav", 0.40f, 1.0);
    sfx(s3, 16, 70, "chime.wav", 0.50f, 1.0);

    TNode* bed = addRoot(comp.roots, 0, comp.duration);
    media::AudioOptions bo;
    bo.volume = 0.70f;
    media::audio(bed, SFX + "bed_news.wav", bo).volumeAt = [](int t) {
        const float up = smooth01((t - 372.f) / 66.f);        // low under the clip, full after
        return 0.24f + 0.76f * up;
    };

    out.push_back(std::move(comp));
}

} // namespace rmtn
