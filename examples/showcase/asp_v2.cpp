// asp_v2.cpp -- "asp-feed-v2": a 45 s vertical explainer about answer set programming.
// One concrete proof: a 2x2 map with two colors has exactly two valid colorings; adding
// the constraint "A is not red" leaves exactly one. The captions are the script (text
// only); a voice can be attached later at the same cue map.
//
//   raymotion render asp-feed-v2
//   raymotion still  asp-feed-v2 --at 760 --png out/asp-760.png
//
// See docs/asp-feed-v2-brief.md and docs/asp-feed-v2-script.md.
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace rmtn {
namespace {

constexpr float W = 1080.f, H = 1920.f;
constexpr int FPS = 30, TOTAL = 1350, XF = 10;
constexpr int DUR[9] = { 140, 140, 175, 125, 160, 190, 205, 125, 170 };

const std::string ASSETS = "out/asp-v2-assets";
const std::string SRT = "out/asp-feed-v2.srt";

const Color BG = mkColor(0x0c1220), PANEL = mkColor(0x171e32), INK = mkColor(0xf2f4ff),
            DIM = mkColor(0x8ea0b8), TEAL = mkColor(0x47dad8), AMBER = mkColor(0xffc365),
            CORAL = mkColor(0xff6b5a), SKY = mkColor(0x5aa8ff), LINE = mkColor(0x2c3a55);

const char* const CAPTIONS[9] = {
    "You know how coding means writing every step?",
    "Answer set programming: describe the solution, not the steps.",
    "Color four regions with two colors. Neighbours must differ.",
    "The regions and borders are facts.",
    "Every region gets one color. Neighbours cannot match.",
    "The solver returns every valid coloring. There are two.",
    "Add one constraint: A is not red. One world disappears.",
    "Schedules, plans, puzzles: you describe, it solves.",
    "Describe the goal. Skip the steps.",
};
const char* const KICKERS[9] = {
    "THE OLD WAY", "THE SHIFT", "THE PROBLEM", "FACTS", "RULES",
    "ANSWER SETS", "THE CONSTRAINT", "WHY CARE", "DESCRIBE THE GOAL",
};

std::array<int, 9> sceneStarts() {
    std::array<int, 9> s{};
    int end = 0;
    for (int i = 0; i < 9; i++) { s[i] = end; end += DUR[i] - (i < 8 ? XF : 0); }
    return s;
}

// ------------------------------------------------------------------ small helpers
void text(const Ctx& cx, const std::string& s, Vector2 at, float size, Color col,
          float spacing = 0, float ax = 0, float ay = 0.5f, Font f = Font{}) {
    const Font font = f.texture.id ? f : fonts::ui(false);
    dr::text(font, cx, s.c_str(), at, size, spacing, col, ax, ay);
}
void centerText(const Ctx& cx, const std::string& s, float y, float size, Color col,
                float spacing = 0, Font f = Font{}) {
    text(cx, s, { W / 2, y }, size, col, spacing, 0.5f, 0.5f, f);
}
void chip(const Ctx& cx, const std::string& s, Vector2 at, float size, Color accent,
          Color fill = PANEL, float k = 1, bool mono = false) {
    if (k <= 0.01f) return;
    const Font f = mono ? code::font(false) : fonts::ui(false);
    const float tw = dr::measure(f, s.c_str(), size, 0).x;
    const float w = tw + size * 1.1f, h = size * 1.75f;
    Ctx m = cx;
    m.o = mapP(cx, at);
    m.s *= 0.75f + 0.25f * k;
    m.a *= std::min(1.f, k);
    dr::roundRect(m, -w / 2, -h / 2, w, h, size * 0.28f, fill);
    dr::roundRect(m, -w / 2, -h / 2, 8, h, 4, accent);
    dr::text(f, m, s.c_str(), { 0, 0 }, size, 0, INK, 0.5f, 0.5f);
}
float popAt(int f, int at) {
    SpringOptions o;
    o.fps = FPS;
    o.delay = at;
    o.config.damping = 11;
    return (float)spring((double)f, o);
}
float slideAt(int f, int at, float dur = 14.f) {
    return (float)interpolate((double)f - at, { 0.0, (double)dur }, { 0.0, 1.0 },
                              clamped(ease::out(ease::cubic)));
}
float fadeWindow(int t, int a, int b, int in = 12, int out = 12) {
    return (float)(interpolate(t, { (double)a, (double)a + in }, { 0, 1 },
                               clamped(ease::out(ease::cubic))) *
                   interpolate(t, { (double)b - out, (double)b }, { 1, 0 },
                               clamped(ease::in(ease::cubic))));
}
void backdrop(const Ctx& cx) {
    dr::rectGrad4(cx, 0, 0, W, H, mkColor(0x0d1526), mkColor(0x0a101c),
                  mkColor(0x070b13), mkColor(0x070b13));
    // a faint dot field, deterministic
    for (int j = 0; j < 24; j++)
        for (int i = 0; i < 14; i++) {
            const float x = 40 + i * 77.f + (j % 2) * 18.f;
            const float y = 170 + j * 72.f;
            dr::circle(cx, { x, y }, 1.4f, withAlpha(DIM, 0.10f + 0.05f * (j % 3)));
        }
}
void kicker(const Ctx& cx, int i, int t) {
    const float k = fadeWindow(t, 0, (int)DUR[i], 12, 12);
    Ctx m = cx;
    m.a *= k;
    dr::text(fonts::ui(true), m, "ANSWER SET PROGRAMMING", { 72, 86 }, 24, 3, TEAL, 0, 0.5f);
    char n[8];
    std::snprintf(n, sizeof n, "%02d / 09", i + 1);
    dr::text(fonts::ui(false), m, n, { W - 72, 86 }, 22, 2, DIM, 1, 0.5f);
    const float p = (float)((t + 0.5) / (double)DUR[i]);
    dr::rect(m, 72, 54, W - 144, 2, withAlpha(LINE, 0.9f));
    dr::rect(m, 72, 54, (W - 144) * std::min(1.f, p), 2, TEAL);
}
void sourceLine(const Ctx& cx, float alpha = 1) {
    Ctx m = cx;
    m.a *= alpha;
    dr::text(fonts::ui(false), m, "ASP / clingo  -  potassco.org/clingo", { 72, 1848 },
             18, 1, withAlpha(DIM, 0.75f), 0, 0.5f);
    dr::text(fonts::ui(false), m, "made with raymotion", { W - 72, 1848 }, 18, 1,
             withAlpha(DIM, 0.55f), 1, 0.5f);
}

// ------------------------------------------------------------------ map
void mapTiles(const Ctx& cx, Vector2 c, float tile, const Color* colors, bool labels,
              float alpha = 1, int t = 0) {
    Ctx m = cx;
    m.a *= alpha;
    const float gap = tile * 0.045f, total = tile * 2 + gap;
    const Vector2 p[4] = { { c.x - total / 2 + tile / 2, c.y - total / 2 + tile / 2 },
                           { c.x + total / 2 - tile / 2, c.y - total / 2 + tile / 2 },
                           { c.x - total / 2 + tile / 2, c.y + total / 2 - tile / 2 },
                           { c.x + total / 2 - tile / 2, c.y + total / 2 - tile / 2 } };
    const char* nm[4] = { "A", "B", "C", "D" };
    for (int i = 0; i < 4; i++) {
        Color fill = PANEL;
        if (colors && colors[i].a > 0) fill = colors[i];
        dr::roundRect(m, p[i].x - tile / 2, p[i].y - tile / 2, tile, tile, tile * 0.08f, fill);
        dr::roundRect(m, p[i].x - tile / 2, p[i].y - tile / 2, tile, tile * 0.035f,
                      tile * 0.02f, withAlpha(colors ? colors[i] : LINE, colors ? 0.95f : 0.6f));
        if (labels) {
            const Color lc = (colors && colors[i].a > 0 &&
                              (colors[i].r + colors[i].g + colors[i].b > 520)) ? BG : INK;
            dr::text(fonts::ui(true), m, nm[i], { p[i].x, p[i].y }, tile * 0.18f, 0, lc, 0.5f, 0.5f);
        }
    }
    // The four borders: one vertical, one horizontal.
    dr::strokeSeg(m, { c.x, c.y - total / 2 }, { c.x, c.y + total / 2 }, 4, withAlpha(INK, 0.8f));
    dr::strokeSeg(m, { c.x - total / 2, c.y }, { c.x + total / 2, c.y }, 4, withAlpha(INK, 0.8f));
}
void colorChips(const Ctx& cx, float y, float k) {
    if (k <= 0.01f) return;
    Ctx m = cx;
    m.a *= k;
    chip(m, "RED", { W / 2 - 80, y }, 27, CORAL, PANEL, k);
    chip(m, "BLUE", { W / 2 + 80, y }, 27, SKY, PANEL, k);
}

// ------------------------------------------------------------------ scenes
void stepStack(const Ctx& cx, int t, float alpha = 1) {
    Ctx m = cx;
    m.a *= alpha;
    const char* steps[4] = { "step 1", "step 2", "step 3", "step 4" };
    for (int i = 0; i < 4; i++) {
        const float k = popAt(t, 6 + i * 12);
        if (k <= 0.01f) continue;
        const float y = 760 + i * 104.f;
        Ctx c = m;
        c.o = mapP(m, { 300, y });
        c.s *= 0.75f + 0.25f * k;
        c.a *= std::min(1.f, k);
        dr::roundRect(c, -170, -42, 340, 84, 18, PANEL);
        dr::roundRect(c, -170, -42, 8, 84, 4, DIM);
        dr::text(fonts::ui(false), c, steps[i], { 0, 0 }, 34, 1, INK, 0.5f, 0.5f);
    }
    const float g = popAt(t, 44);
    if (g > 0.01f) {
        Ctx c = m;
        c.o = mapP(m, { 760, 870 });
        c.s *= 0.75f + 0.25f * g;
        c.a *= std::min(1.f, g);
        dr::roundRect(c, -170, -62, 340, 124, 20, PANEL);
        dr::roundRect(c, -170, -62, 8, 124, 4, TEAL);
        dr::text(fonts::ui(true), c, "the goal", { 0, 0 }, 40, 1, TEAL, 0.5f, 0.5f);
    }
}
float kOut(int t, int at);
void mapScene(const Ctx& cx, int t, float alpha = 1) {
    Ctx m = cx;
    m.a *= alpha;
    const float k = slideAt(t, 8, 26);
    colorChips(m, 560, kOut(t, 24));
    mapTiles(m, { W / 2, 900 }, 250, nullptr, true, k);
}
float kOut(int t, int at) {
    return (float)easeFn(E_OUT_CUBIC, std::clamp((t - at) / 20.0, 0.0, 1.0));
}
void factsScene(const Ctx& cx, int t) {
    mapScene(cx, t, 1);
    Ctx m = cx;
    const float k = kOut(t, 8);
    m.a *= k;
    const char* facts[4] = { "region A", "region B", "region C", "region D" };
    for (int i = 0; i < 4; i++)
        chip(m, facts[i], { 250.f + (i % 2) * 260, 1290.f + (i / 2) * 86 }, 23, AMBER, PANEL,
             kOut(t, 8 + i * 6));
    chip(m, "borders", { W / 2 + 210, 1290 }, 23, TEAL, PANEL, kOut(t, 38));
    chip(m, "A-B  A-C  B-D  C-D", { W / 2 + 100, 1372 }, 19, AMBER, PANEL, kOut(t, 48));
}
void rulesScene(const Ctx& cx, int t) {
    mapScene(cx, t, 1);
    const float k = kOut(t, 10);
    if (k <= 0.01f) return;
    Ctx m = cx;
    m.a *= k;
    dr::roundRect(m, 100, 1200, W - 200, 190, 22, PANEL);
    dr::roundRect(m, 100, 1200, W - 200, 6, 3, TEAL);
    dr::text(fonts::ui(false), m, "RULES", { 136, 1230 }, 20, 4, TEAL, 0, 0.5f);
    dr::text(fonts::ui(false), m, "one color per region", { 136, 1280 }, 34, 0, INK, 0, 0.5f);
    dr::text(fonts::ui(false), m, "neighbours differ", { 136, 1330 }, 34, 0, INK, 0, 0.5f);
    const float hk = kOut(t, 44);
    if (hk > 0.01f) {
        Ctx h = cx;
        h.a *= hk;
        // The A-B border is the neighbour that must differ.
        dr::strokeSeg(h, { W / 2, 900 - 250 - 6 }, { W / 2, 900 }, 9, withAlpha(AMBER, 0.95f));
        dr::text(fonts::ui(true), h, "neighbours differ", { W / 2, 610 }, 22, 3, AMBER, 0.5f, 0.5f);
    }
}
void answerMap(const Ctx& cx, Vector2 c, float tile, const Color* colors, const char* label,
               float k, bool crossed = false, float alpha = 1) {
    if (k <= 0.01f) return;
    Ctx m = cx;
    m.a *= alpha;
    mapTiles(m, c, tile, colors, false, k);
    dr::text(fonts::ui(true), m, label, { c.x, c.y - tile * 1.55f }, 26, 2, TEAL, 0.5f, 0.5f);
    if (crossed) {
        Ctx x = m;
        dr::strokeSeg(x, { c.x - tile, c.y - tile }, { c.x + tile, c.y + tile }, 10, CORAL);
        dr::strokeSeg(x, { c.x + tile, c.y - tile }, { c.x - tile, c.y + tile }, 10, CORAL);
    }
}
void answerSetsScene(const Ctx& cx, int t, bool constraint = false) {
    const Color as1[4] = { CORAL, SKY, SKY, CORAL };
    const Color as2[4] = { SKY, CORAL, CORAL, SKY };
    if (constraint) {
        const float k = kOut(t, 8);
        Ctx m = cx;
        m.a *= k;
        dr::roundRect(m, 100, 360, W - 200, 190, 22, PANEL);
        dr::roundRect(m, 100, 360, W - 200, 6, 3, CORAL);
        dr::text(fonts::ui(false), m, "ONE CONSTRAINT", { 136, 390 }, 20, 4, CORAL, 0, 0.5f);
        dr::text(fonts::ui(true), m, "A is not red", { 136, 448 }, 46, 0, INK, 0, 0.5f);
        const float cross = kOut(t, 36);
        answerMap(m, { 300, 900 }, 185, as1, "ANSWER SET 1", kOut(t, 14), cross > 0.01f,
                  1.f - 0.7f * cross);
        answerMap(m, { 780, 900 }, 185, as2, "ANSWER SET 2", kOut(t, 20), false, 1.f);
        if (kOut(t, 58) > 0.01f) {
            Ctx c = cx;
            c.a *= kOut(t, 58);
            dr::text(fonts::ui(true), c, "one answer set", { W / 2, 1180 }, 34, 2, TEAL, 0.5f, 0.5f);
        }
    } else {
        answerMap(cx, { 300, 900 }, 185, as1, "ANSWER SET 1", kOut(t, 12));
        answerMap(cx, { 780, 900 }, 185, as2, "ANSWER SET 2", kOut(t, 24));
        Ctx m = cx;
        m.a *= kOut(t, 44);
        dr::text(fonts::ui(true), m, "two valid colorings", { W / 2, 1180 }, 34, 2, AMBER, 0.5f, 0.5f);
    }
}
void usesScene(const Ctx& cx, int t) {
    const char* uses[3] = { "schedules", "plans", "puzzles" };
    for (int i = 0; i < 3; i++) {
        const float k = kOut(t, 8 + i * 12);
        if (k <= 0.01f) continue;
        const float x = 210.f + i * 330.f;
        Ctx m = cx;
        m.a *= k;
        m.o = mapP(cx, { x, 820 });
        m.s *= 0.8f + 0.2f * k;
        dr::roundRect(m, -130, -170, 260, 340, 22, PANEL);
        dr::roundRect(m, -130, -170, 260, 8, 4, i == 1 ? AMBER : TEAL);
        dr::text(fonts::ui(true), m, uses[i], { 0, -26 }, 30, 0, INK, 0.5f, 0.5f);
        // small map motif
        for (int j = 0; j < 4; j++)
            dr::roundRect(m, -58 + (j % 2) * 60, 40 + (j / 2) * 60, 54, 54, 8,
                          withAlpha(j == 0 ? CORAL : j == 3 ? CORAL : SKY, 0.8f));
    }
    Ctx m = cx;
    m.a *= kOut(t, 54);
    centerText(m, "same pattern: facts, rules, constraints", 1180, 26, DIM);
}

// ------------------------------------------------------------------ captions
std::vector<std::string> splitWords(const std::string& s) {
    std::vector<std::string> out;
    for (size_t i = 0; i < s.size();) {
        while (i < s.size() && s[i] == ' ') i++;
        const size_t j = s.find(' ', i);
        if (j == std::string::npos) { if (i < s.size()) out.push_back(s.substr(i)); break; }
        if (j > i) out.push_back(s.substr(i, j - i));
        i = j + 1;
    }
    return out;
}
std::vector<std::vector<std::string>> wrapWords(const std::vector<std::string>& words, Font f,
                                                float size, float maxW) {
    std::vector<std::vector<std::string>> lines;
    std::vector<std::string> line;
    const float space = dr::measure(f, " ", size, 0).x;
    float w = 0;
    for (const std::string& word : words) {
        const float ww = dr::measure(f, word.c_str(), size, 0).x;
        if (!line.empty() && w + space + ww > maxW) {
            lines.push_back(line); line.clear(); w = 0;
        }
        if (!line.empty()) w += space;
        line.push_back(word);
        w += ww;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}
void captions(const Ctx& cx, int t, const std::array<int, 9>& st) {
    for (int i = 0; i < 8; i++) {
        const int from = st[i] + (i ? XF : 0);
        const int to = (i < 8 ? st[i + 1] : TOTAL);
        if (t < from || t >= to) continue;
        const float k = fadeWindow(t, from, to, 12, 12);
        Ctx m = cx;
        m.a *= k;
        dr::rectGrad4(m, 0, 1410, W, 510, withAlpha(BG, 0), withAlpha(BG, 0),
                      withAlpha(BG, 0.72f), withAlpha(BG, 0.72f));
        if (t < from + 20) {
            const float r = 1.f - (float)interpolate(t, { (double)from, from + 20.0 }, { 0, 1 },
                                                     clamped(ease::out(ease::cubic)));
            Ctx kk = m;
            dr::text(fonts::ui(true), kk, KICKERS[i], { W / 2, 1490 - 10 * r }, 24, 5, TEAL, 0.5f, 0.5f);
        } else {
            dr::text(fonts::ui(true), m, KICKERS[i], { W / 2, 1490 }, 24, 5, TEAL, 0.5f, 0.5f);
        }
        const std::string line = CAPTIONS[i];
        const std::vector<std::string> words = splitWords(line);
        const Font f = fonts::ui(true);
        const size_t n = std::strlen(line.c_str());
        const float size = n > 80 ? 42.f : (n > 55 ? 48.f : 54.f);
        const auto lines = wrapWords(words, f, size, 900.f);
        const int nw = (int)words.size();
        const int spoken = std::min(nw, (int)std::floor(
            std::max(0.f, (t - from) / (float)FPS - 0.15f) * 2.5f));
        const float space = dr::measure(f, " ", size, 0).x;
        const float lineH = size * 1.22f;
        float y = 1550;
        int idx = 0;
        for (const auto& ln : lines) {
            float total = -space;
            for (const auto& w : ln) total += dr::measure(f, w.c_str(), size, 0).x + space;
            float x = W / 2 - total / 2;
            for (const auto& w : ln) {
                const float ww = dr::measure(f, w.c_str(), size, 0).x;
                const Color col = idx < spoken ? INK : (idx == spoken ? TEAL : withAlpha(INK, 0.35f));
                dr::text(f, m, w.c_str(), { x + 2, y + 2 }, size, 0, withAlpha(BLACK, 0.5f), 0, 0);
                dr::text(f, m, w.c_str(), { x, y }, size, 0, col, 0, 0);
                x += ww + space;
                idx++;
            }
            y += lineH;
        }
    }
}

// ------------------------------------------------------------------ score / assets
void genScore(const std::string& path, const std::array<int, 9>& st) {
    procgen::Score s(TOTAL / (double)FPS);
    auto at = [&](int i, double f) { return (st[i] + f) / FPS; };
    // Step stack / hook.
    s.pad(0, 4.2, 50, 0.20f, 600, 0.4f);
    s.pad(0, 4.2, 57, 0.14f, 600, 0.6f);
    for (double t = 0.5; t < 4.2; t += 0.5) s.tick(t, 0.05f);
    // Shift / map reveal.
    s.riser(4.3, 7.8, 0.24f); s.impact(8.7, 0.7f);
    s.pad(8.7, 9.3, 46, 0.16f, 700, 0.5f);
    s.pluck(at(2, 10), 69, 0.06f, 0.6f, 4);
    // Facts and rules.
    for (int i = 0; i < 4; i++) s.tick(at(3, 8 + i * 6), 0.05f);
    s.tick(at(4, 10), 0.08f);
    s.pluck(at(4, 44), 74, 0.06f, 0.5f, 4);
    // The two answer sets.
    for (int i = 0; i < 2; i++) {
        const int f = i == 0 ? 12 : 24;
        for (int j = 0; j < 3; j++) s.bell(at(5, f + j * 5), 72 + j * 4, 0.055f, 0.25f + 0.2f * j);
    }
    // Constraint / cross / one remains.
    s.impact(at(6, 36), 0.55f);
    s.bell(at(6, 58), 79, 0.07f, 0.5f);
    s.pad(at(6, 10), 5.0, 41, 0.10f, 500, 0.5f);
    // Why care.
    for (int i = 0; i < 3; i++) s.tick(at(7, 8 + i * 12), 0.06f);
    // Outro.
    s.riser(at(8, 10), at(8, 40), 0.12f);
    s.impact(at(8, 40), 0.4f);
    for (int m : { 50, 54, 57, 62 }) s.pad(at(8, 40), 4.5, m, 0.09f, 1400, 0.5f);
    s.bass(at(8, 40), 4.0, 38, 0.25f);
    for (int j = 0; j < 4; j++) s.bell(at(8, 42 + j * 0.5), 74 + j * 4, 0.06f, 0.3f + 0.1f * j);
    // Soft limiter and gain for -16 LUFS / <= -1 dBTP.
    std::vector<float> mix = s.mix();
    for (float& x : mix) x = std::tanh(x * 1.5f);
    float peak = 0;
    for (float x : mix) peak = std::max(peak, std::fabs(x));
    const float g = peak > 0 ? 0.70f / peak : 1.f;
    for (float& x : mix) x *= g;
    media::writeWav(path, mix, procgen::Score::RATE);
}
void writeSrtFile(const std::array<int, 9>& st) {
    sub::Track tr;
    for (int i = 0; i < 9; i++) {
        const double from = (st[i] + (i ? XF : 0)) / (double)FPS;
        const double to = ((i < 8 ? st[i + 1] : TOTAL) - 1) / (double)FPS;
        tr.cues.push_back({ from, to, CAPTIONS[i], false });
    }
    sub::writeSrt(SRT, tr);
}
bool assetsReady = false;
void ensureAssets(const std::array<int, 9>& st) {
    std::filesystem::create_directories(ASSETS);
    const std::string score = ASSETS + "/score.wav";
    if (!std::filesystem::exists(score)) genScore(score, st);
    writeSrtFile(st);
    assetsReady = true;
}

// ------------------------------------------------------------------ film
void buildFilm(Comp& c, const std::array<int, 9>& st) {
    TNode* bg = addRoot(c.roots, 0, TOTAL);
    bg->fn = [](const Ctx& cx, int) { backdrop(cx); };

    TransitionSeries ts(c.roots, W, H);
    auto add = [&](int i, auto fn) {
        if (i) ts.transition(Transition::fade(XF));
        TNode* n = ts.add(DUR[i]);
        n->fn = fn;
        if (n->start != st[i]) std::fprintf(stderr, "asp scene %d start %d expected %d\n", i, n->start, st[i]);
    };
    add(0, [](const Ctx& cx, int t) {
        stepStack(cx, t);
        centerText(cx, "four steps", 540, 24, DIM);
    });
    add(1, [](const Ctx& cx, int t) {
        const float out = 1.f - slideAt(t, 6, 22);
        stepStack(cx, t, out);
        Ctx m = cx; m.a *= slideAt(t, 18, 30);
        mapTiles(m, { W / 2, 900 }, 250, nullptr, true, 1, t);
        colorChips(m, 560, kOut(t, 36));
    });
    add(2, [](const Ctx& cx, int t) { mapScene(cx, t); });
    add(3, [](const Ctx& cx, int t) { factsScene(cx, t); });
    add(4, [](const Ctx& cx, int t) { rulesScene(cx, t); });
    add(5, [](const Ctx& cx, int t) { answerSetsScene(cx, t, false); });
    add(6, [](const Ctx& cx, int t) { answerSetsScene(cx, t, true); });
    add(7, [](const Ctx& cx, int t) { usesScene(cx, t); });
    add(8, [](const Ctx& cx, int t) {
        const float a = kOut(t, 8);
        Ctx m = cx; m.a *= a;
        dr::text(fonts::ui(true), m, "DESCRIBE THE GOAL.", { W / 2, 700 }, 72, 2, INK, 0.5f, 0.5f);
        dr::text(fonts::ui(true), m, "SKIP THE STEPS.", { W / 2, 800 }, 72, 2, TEAL, 0.5f, 0.5f);
        dr::text(fonts::ui(false), m, "answer set programming  /  clingo", { W / 2, 930 }, 30, 0, DIM, 0.5f, 0.5f);
        dr::text(fonts::ui(false), m, "one concrete example: four regions, two colors, two answer sets", { W / 2, 1000 }, 22, 0, withAlpha(DIM, 0.75f), 0.5f, 0.5f);
    });

    // Captions and chrome are roots, not scenes.
    addRoot(c.roots, 0, TOTAL)->fn = [st](const Ctx& cx, int t) {
        const int i = [&] { int k = 0; while (k + 1 < 9 && t >= st[k + 1]) k++; return k; }();
        kicker(cx, i, t - st[i]);
        captions(cx, t, st);
        sourceLine(cx);
    };

    TNode* logo = prefab::use(c.roots, 1220, 130, "raymotion-logo",
        Json::parse(R"({"size": 104, "layout": "horizontal", "tagline": "video as code", "delay": 4})"));
    logo->props.pos = { W / 2, H * 0.75f };
}

void buildThumb(Comp& c) {
    addRoot(c.roots, 0, c.duration)->fn = [](const Ctx& cx, int) {
        backdrop(cx);
        dr::rectGrad4(cx, 0, 0, W, H, withAlpha(BG, 0.2f), withAlpha(BG, 0.2f),
                      withAlpha(BG, 0.8f), withAlpha(BG, 0.8f));
        dr::text(fonts::ui(true), cx, "ANSWER SET", { 72, 300 }, 28, 6, TEAL, 0, 0.5f);
        dr::text(fonts::ui(true), cx, "PROGRAMMING", { 72, 360 }, 68, 1, INK, 0, 0.5f);
        dr::text(fonts::ui(false), cx, "two solutions. one constraint.", { 72, 460 }, 32, 0, DIM, 0, 0.5f);
        const Color as1[4] = { CORAL, SKY, SKY, CORAL };
        const Color as2[4] = { SKY, CORAL, CORAL, SKY };
        answerMap(cx, { W / 2 - 190, 900 }, 170, as1, "ANSWER SET 1", 1);
        answerMap(cx, { W / 2 + 190, 900 }, 170, as2, "ANSWER SET 2", 1);
        dr::text(fonts::ui(false), cx, "ASP / clingo  -  potassco.org/clingo", { 72, H - 120 }, 22, 0, DIM, 0, 0.5f);
    };
}

} // namespace

void registerAspFeedV2(Registry& out) {
    const std::array<int, 9> st = sceneStarts();
    Comp c;
    c.name = "asp-feed-v2";
    c.w = (int)W; c.h = (int)H; c.fps = FPS;
    c.duration = TOTAL;
    c.background = BG;
    c.assets = [st](const Comp&, const Json&) { ensureAssets(st); };
    c.build = [st](Comp& c, const Json&) {
        c.roots.clear();
        media::audio(addRoot(c.roots, 0, TOTAL), ASSETS + "/score.wav");
        if (!assetsReady) return;
        buildFilm(c, st);
    };
    out.push_back(std::move(c));

    Comp th;
    th.name = "asp-feed-v2-thumb";
    th.w = (int)W; th.h = (int)H; th.fps = FPS; th.duration = 60;
    th.background = BG;
    buildThumb(th);
    out.push_back(std::move(th));
}

} // namespace rmtn
