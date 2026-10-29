// Here-and-There feed v2: a 1080x1350 paper-cut explanation with a smoke-only guess.
// Script, cue map and source ledger: docs/here-there-feed-v2-*.md
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include "here_there.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace rmtn { namespace {
constexpr int W = 1080, H = 1350, FPS = 30, N = 6;
constexpr float WPS = 2.5f;
constexpr const char* ASSETS = "out/ht-feed-v2-assets";
constexpr const char* SRT_PATH = "out/here-there-feed-v2.srt";

const Color paper{ 249, 240, 218, 255 }, ink{ 39, 54, 64, 255 };
const Color amber{ 225, 155, 57, 255 }, teal{ 35, 153, 148, 255 };
const Color coral{ 210, 76, 58, 255 }, muted{ 99, 108, 108, 255 };
const Color cream{ 255, 251, 239, 255 }, card{ 237, 220, 179, 255 };

struct Line { const char* text; };
struct Chapter { const char* title; const Line* lines; int n; float tail; };

const Line L1[] = { { "If nothing contradicts a claim, should you still believe it?" } };
const Line L2[] = {
    { "A guess says there is smoke. The given facts are empty." },
    { "There is what we assume. Here is what we keep. We can keep less, never add." },
};
const Line L3[] = {
    { "The only rule: whenever there is fire, there is smoke." },
    { "HERE is empty, so its check passes. THERE has smoke, so its check passes too." },
};
const Line L4[] = {
    { "A rule that is merely not contradicted is not yet supported." },
    { "Keep the guess fixed. Now remove smoke from HERE." },
    { "The rule still holds. The smoke was not required here." },
};
const Line L5[] = {
    { "Now add a fact: there is fire." },
    { "Remove fire and the given fact breaks; remove smoke and the rule breaks." },
    { "Both stay. Now the world is supported." },
};
const Line L6[] = { { "Nothing says it's wrong. But then what says it's right?" } };

const Chapter CH[N] = {
    { "The question", L1, 1, 3.0f },
    { "Two views", L2, 2, 0.8f },
    { "Both sides", L3, 2, 0.8f },
    { "Not backed up", L4, 3, 0.8f },
    { "The fact", L5, 3, 0.8f },
    { "Close", L6, 1, 2.0f },
};

int wordsIn(const char* s) {
    int n = 0; bool in = false;
    for (; s && *s; ++s) {
        if (std::isspace((unsigned char)*s)) in = false;
        else if (!in) { in = true; ++n; }
    }
    return n;
}
int chapterWords(int i) {
    int n = 0;
    for (int j = 0; j < CH[i].n; ++j) n += wordsIn(CH[i].lines[j].text);
    return n;
}
std::array<int, N> makeDur() {
    std::array<int, N> d{};
    for (int i = 0; i < N; ++i)
        d[i] = (int)std::lround((chapterWords(i) / WPS + CH[i].tail) * FPS);
    return d;
}
const std::array<int, N> DUR = makeDur();
std::array<int, N> makeStarts() {
    std::array<int, N> s{};
    for (int i = 1; i < N; ++i) s[i] = s[i - 1] + DUR[i - 1];
    return s;
}
const std::array<int, N> ST = makeStarts();
const int TOTAL = ST[N - 1] + DUR[N - 1];

int lineStartLocal(int ch, int j) {
    int words = 0;
    for (int k = 0; k < j; ++k) words += wordsIn(CH[ch].lines[k].text);
    return (int)std::lround(words / WPS * FPS);
}

// ------------------------------------------------------------------ paper tools
float smooth(float a) { a = std::clamp(a, 0.f, 1.f); return a * a * (3 - 2 * a); }
float mixf(float a, float b, float u) { return a + (b - a) * u; }
Font feedFont(bool bold = false) { return fonts::ui(bold); }

void feedText(const std::string& text, float x, float y, float width, float size,
              Color color = ink, bool bold = false, float bottom = 1320) {
    std::vector<std::string> lines;
    std::string line, word;
    auto flush = [&] {
        if (word.empty()) return;
        const std::string next = line.empty() ? word : line + " " + word;
        if (!line.empty() && MeasureTextEx(feedFont(bold), next.c_str(), size, 0).x > width) {
            lines.push_back(line); line = word;
        } else line = next;
        word.clear();
    };
    for (char ch : text) {
        if (ch == ' ' || ch == '\n') { flush(); if (ch == '\n') { lines.push_back(line); line.clear(); } }
        else word += ch;
    }
    flush();
    if (!line.empty()) lines.push_back(line);
    for (auto& s : lines) {
        DrawTextEx(feedFont(bold), s.c_str(), { x, y }, size, 0, color);
        y += size * 1.16f;
        if (y > bottom) break;
    }
}
void check(float x, float y, bool pass, Color color = teal) {
    if (pass) {
        DrawLineEx({ x, y + 13 }, { x + 10, y + 24 }, 6, color);
        DrawLineEx({ x + 10, y + 24 }, { x + 32, y }, 6, color);
    } else {
        DrawLineEx({ x, y }, { x + 24, y + 24 }, 6, coral);
        DrawLineEx({ x + 24, y }, { x, y + 24 }, 6, coral);
    }
}
void flame(float x, float y, float size, float time, Color color = coral) {
    const float lean = std::sin(time * 3) * size * .065f;
    DrawEllipse(int(x + 5), int(y + 8), size * .40f, size * .13f, Color{ 70, 52, 39, 35 });
    DrawTriangle({ x - size * .39f, y - size * .27f }, { x + size * .35f, y - size * .23f },
                 { x + lean, y - size * 1.20f }, color);
    DrawEllipse(int(x), int(y - size * .30f), size * .38f, size * .42f, color);
    DrawTriangle({ x - size * .17f, y - size * .14f }, { x + size * .18f, y - size * .12f },
                 { x + size * .12f, y - size * .70f }, amber);
    DrawLineEx({ x - size * .38f, y + 5 }, { x + size * .35f, y + 14 }, 9, ink);
    DrawLineEx({ x - size * .35f, y + 14 }, { x + size * .37f, y + 4 }, 9, ink);
}
void smoke(float x, float y, float size, float time, float alpha = 1) {
    const Color shade = withAlpha(ink, alpha * .17f), c = withAlpha(Color{ 132, 155, 169, 255 }, alpha);
    DrawLineBezier({ x + 4, y + 12 }, { x - 7, y - size * .70f }, size * .20f, shade);
    DrawCircleV({ x + 5, y - size * .65f }, size * .34f, shade);
    DrawCircleV({ x - size * .28f + std::sin(time) * 3, y - size * .83f }, size * .32f, c);
    DrawCircleV({ x + size * .13f, y - size * 1.04f }, size * .39f, c);
    DrawCircleV({ x + size * .40f, y - size * .76f }, size * .31f, c);
    DrawEllipse(int(x + size * .02f), int(y - size * .61f), size * .53f, size * .27f, c);
    DrawLineBezier({ x + size * .28f, y - size * .75f }, { x - size * .07f, y - size * .96f },
                   4, withAlpha(paper, alpha * .7f));
    DrawLineBezier({ x - 5, y + 8 }, { x + size * .15f, y - size * .54f }, size * .13f, c);
}
void ribbon(float x, float y, float width, unsigned mask, float t, bool trace = false) {
    feedText("RULE", x + width * .5f - 32, y - 33, 100, 24, withAlpha(muted, .8f), true, y);
    DrawTriangle({ x - 12, y + 8 }, { x - 12, y + 82 }, { x + 22, y + 45 }, amber);
    DrawRectangleRounded({ x, y, width, 82 }, .18f, 8, card);
    flame(x + 40, y + 61, 30, t);
    smoke(x + width - 45, y + 63, 32, t);
    const float start = x + 83, end = x + width - 94;
    const bool fire = mask & ht::FIRE, pass = !(mask & ht::FIRE) || (mask & ht::SMOKE);
    const Color c = fire ? (pass ? teal : coral) : muted;
    const float stop = fire && !pass ? mixf(start, end, .58f) : end;
    DrawLineEx({ start, y + 39 }, { stop, y + 39 }, 5, c);
    if (fire && !pass) DrawLineEx({ stop + 12, y + 23 }, { stop + 12, y + 55 }, 5, coral);
    else DrawTriangle({ end + 10, y + 39 }, { end - 5, y + 29 }, { end - 5, y + 49 }, c);
    if (fire || trace) {
        const float u = trace ? smooth((t - 3) / 1.5f) : std::fmod(t * .44f, 1.f);
        DrawCircleV({ mixf(start, stop, u), y + 39 }, 7, teal);
    }
    if (!trace) {
        check(x + width - 34, y + 105, pass, fire ? teal : muted);
        feedText(!fire ? "No fire to check" : pass ? "Rule obeyed" : "Missing smoke",
                 x, y + 98, width - 40, 36, c, true, y + 185);
    }
}
void world(float x, float w, unsigned mask, float t, bool there, Color accent,
           const char* label, const char* sub, bool pinned, float smokeOffset = 0) {
    DrawEllipse(int(x + w / 2 + 7), 939, int(w * .43f), 23, Color{ 70, 52, 39, 25 });
    DrawTriangle({ x + 24, 891 }, { x + w * .50f, 960 }, { x + w - 22, 891 }, Color{ 189, 155, 108, 255 });
    DrawRectangleRounded({ x, 875, w, 35 }, .5f, 8, accent);
    DrawCircleV({ x + 44, 864 }, 22, accent);
    DrawCircleV({ x + w - 37, 870 }, 17, accent);
    DrawLineEx({ x + 45, 855 }, { x + 65, 833 }, 4, accent);
    DrawLineEx({ x + w - 42, 850 }, { x + w - 63, 828 }, 4, accent);
    DrawRectangleRounded({ x + 8, 531, w - 16, 314 }, .35f, 16, cream);
    DrawCircleV({ x + 91, 854 }, 15, cream);
    DrawCircleV({ x + 74, 877 }, 8, cream);
    feedText(label, x + 16, 443, w - 20, 44, accent, true, 505);
    feedText(sub, x + 16, 497, w - 20, 30, ink, false, 534);
    if (mask & ht::FIRE) flame(x + w * .28f, 825, 79, t);
    if (mask & ht::SMOKE) smoke(x + w * .68f + smokeOffset, 805, 93, t);
    if (pinned) {
        DrawLineEx({ x + w - 39, 543 }, { x + w - 50, 584 }, 7, ink);
        DrawCircleV({ x + w - 35, 536 }, 17, coral);
    }
    ribbon(x + 8, 977, w - 16, mask, t);
}

// ------------------------------------------------------------------ captions
std::vector<std::string> splitWords(const std::string& s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && std::isspace((unsigned char)s[i])) ++i;
        size_t j = i;
        while (j < s.size() && !std::isspace((unsigned char)s[j])) ++j;
        if (j > i) out.push_back(s.substr(i, j - i));
        i = j;
    }
    return out;
}
std::vector<std::vector<std::string>> wrapWords(Font f, const std::vector<std::string>& words,
                                                 float size, float maxW) {
    std::vector<std::vector<std::string>> lines;
    std::vector<std::string> line;
    const float space = MeasureTextEx(f, " ", size, 0).x;
    float w = 0;
    for (const auto& word : words) {
        const float ww = MeasureTextEx(f, word.c_str(), size, 0).x;
        const float add = (line.empty() ? 0 : space) + ww;
        if (!line.empty() && w + add > maxW) { lines.push_back(line); line.clear(); w = 0; }
        if (!line.empty()) w += space;
        line.push_back(word); w += ww;
    }
    if (!line.empty()) lines.push_back(line);
    if (lines.empty()) lines.push_back({});
    return lines;
}
void caption(int ch, int local, float alpha = 1) {
    const Chapter& c = CH[ch];
    int li = 0;
    for (int j = 0; j < c.n; ++j) if (local >= lineStartLocal(ch, j)) li = j;
    const int from = lineStartLocal(ch, li);
    const std::vector<std::string> words = splitWords(c.lines[li].text);
    const Font f = feedFont(true);
    const float size = 58, maxW = 952;
    const auto lines = wrapWords(f, words, size, maxW);
    const float lineH = size * 1.16f;
    int spoken = (int)std::floor(std::max(0.f, (local - from) / (float)FPS) * WPS);
    int idx = 0;
    for (size_t k = 0; k < lines.size(); ++k) {
        const auto& line = lines[k];
        const float space = MeasureTextEx(f, " ", size, 0).x;
        float total = -space;
        for (const auto& word : line) total += MeasureTextEx(f, word.c_str(), size, 0).x + space;
        float x = 64;
        const float y = 64 + k * lineH;
        for (const auto& word : line) {
            const Color col = idx < spoken ? ink : withAlpha(ink, .32f);
            DrawTextEx(f, word.c_str(), { x, y }, size, 0, withAlpha(col, alpha));
            x += MeasureTextEx(f, word.c_str(), size, 0).x + space;
            ++idx;
        }
    }
}

// ------------------------------------------------------------------ film
void backdrop() {
    ClearBackground(paper);
    for (int i = 0; i < 75; ++i) {
        const float x = float((i * 167 + 23) % W);
        const float y = float((i * 223 + 71) % H);
        DrawCircleV({ x, y }, 1.4f, Color{ 169, 141, 100, 26 });
    }
}
void sourceLine() {
    feedText("Cabalar 2021, sec. 2 / Aguado et al. 2013, secs. 2-3", 64, 1288, 760, 22,
             withAlpha(muted, .9f), false, 1325);
    feedText("text-only cut", 818, 1288, 210, 22, withAlpha(muted, .65f), false, 1325);
}
void chapterChrome(int ch, float progress) {
    DrawRectangle(64, 40, 952, 3, withAlpha(muted, .16f));
    DrawRectangle(64, 40, int(952 * progress), 3, teal);
    char c[16]; std::snprintf(c, sizeof c, "%02d / %02d", ch + 1, N);
    feedText(c, 916, 16, 100, 24, withAlpha(muted, .8f), true, 44);
}
void drawGuess(float t) {
    const float bob = std::sin(t * 1.1f) * 5;
    DrawEllipse(550, 910, 355, 29, Color{ 70, 52, 39, 22 });
    DrawRectangleRounded({ 150, 462 + bob, 780, 367 }, .32f, 20, cream);
    DrawCircleV({ 337, 853 + bob }, 23, cream);
    DrawCircleV({ 305, 889 + bob }, 12, cream);
    feedText("GUESS", 213, 496 + bob, 500, 48, amber, true);
    if (t >= 2.5f) {
        const float a = smooth((t - 2.5f) / 1.2f);
        smoke(551, 790 + (1 - a) * 27 + bob, 145, t, a);
    }
    ribbon(210, 940, 660, 0, t, true);
    DrawRectangleRoundedLinesEx({ 228, 1113, 624, 132 }, .1f, 8, 4, withAlpha(amber, .45f));
    feedText("GIVEN FACTS", 266, 1126, 550, 34, muted, true, 1170);
    feedText("Nothing given", 266, 1174, 550, 40, ink, false, 1245);
}
void drawStage(int ch, float t, float local) {
    unsigned there = ht::SMOKE, here = 0;
    bool pin = false;
    float smokeOut = 0;
    if (ch == 3) {
        pin = t >= 29.4f;
        if (t >= 33.8f) {
            const float u = smooth((t - 33.8f) / 1.0f);
            here = ht::SMOKE; smokeOut = -300 * u;
        }
        if (t >= 37.4f) here = 0;
    }
    if (ch >= 4) { there = ht::FIRE | ht::SMOKE; here = ht::FIRE | ht::SMOKE; }
    if (ch == 4) {
        // GIVEN: FIRE sits between the two views.
        DrawRectangleRounded({ 254, 377, 572, 54 }, .1f, 8, amber);
        feedText("GIVEN: FIRE", 285, 381, 510, 44, ink, true, 431);
        feedText("worked example", 860, 384, 150, 20, withAlpha(amber, .8f), false, 430);
    }
    world(45, 440, there, t, true, amber, "THERE", "our guess", pin);
    world(595, 440, here, t, false, teal, "HERE", "what we keep", false, smokeOut);
    if (ch == 3 && t >= 37.4f) {
        feedText("NOT BACKED UP", 611, 1170, 400, 42, coral, true, 1290);
        feedText("the smoke was not required", 611, 1210, 400, 23, withAlpha(coral, .85f), false, 1290);
    }
    if (ch == 4) {
        feedText("remove fire -> fact fails", 611, 1180, 420, 24, withAlpha(coral, .9f), true, 1290);
        feedText("remove smoke -> rule fails", 611, 1214, 420, 24, withAlpha(coral, .9f), true, 1290);
        feedText("both stay", 611, 1250, 420, 24, teal, true, 1290);
    }
}
void drawFilm(int frame) {
    const float t = frame / (float)FPS;
    int ch = 0;
    while (ch + 1 < N && t >= ST[ch + 1] / (float)FPS) ++ch;
    const float local = t - ST[ch] / (float)FPS;
    backdrop();
    chapterChrome(ch, frame / (float)(TOTAL - 1));
    if (ch == 0) drawGuess(t);
    else drawStage(ch, t, local);
    // Current line; the first line starts at the chapter's local zero.
    const Chapter& c = CH[ch];
    int li = 0;
    for (int j = 0; j < c.n; ++j) if (local * FPS >= lineStartLocal(ch, j)) li = j;
    caption(ch, (int)(local * FPS));
    sourceLine();
    if (ch == 5) {
        feedText("raymotion", 820, 1250, 190, 24, withAlpha(muted, .7f), true, 1290);
    }
}

// ------------------------------------------------------------------ score / assets
void genScore(const std::string& path) {
    procgen::Score s(TOTAL / (double)FPS);
    const double st[N] = { 0, 7.0, 18.6, 29.4, 42.2, 53.8 };
    const int roots[N] = { 45, 45, 41, 43, 43, 45 };
    for (int i = 0; i < N; ++i)
        s.pad(st[i], (DUR[i] / (double)FPS) - .2, roots[i], .13f, 650, .45f);
    for (int i = 0; i < N; ++i) {
        const double t = st[i];
        if (i == 0) s.tick(.2, .07f);
        else if (i == 1) { s.riser(t - .35, t, .14f); s.impact(t, .25f); }
        else if (i == 2) { s.tick(t, .06f); s.tick(t + 4.0, .06f); }
        else if (i == 3) { s.tick(t, .07f); s.impact(t + 4.4, .18f); s.impact(t + 8.0, .25f); }
        else if (i == 4) { s.impact(t, .28f); s.tick(t + 2.8, .06f); s.tick(t + 8.0, .06f); }
        else { s.bell(t, 79, .05f, .5f); }
    }
    s.tick(2.5, .04f);
    s.bell(7.4, 69, .045f, .5f);
    s.bell(7.8, 76, .045f, .45f);
    s.bell(18.8, 72, .04f, .5f);
    s.bell(22.6, 76, .04f, .55f);
    s.bell(33.0, 74, .045f, .45f);
    s.bell(37.5, 79, .045f, .55f);
    s.impact(33.8, .16f);
    s.bell(42.2, 81, .04f, .5f);
    s.bell(46.0, 84, .04f, .5f);

    std::vector<float> mix = s.mix();
    for (float& x : mix) x = std::tanh(x * 1.35f);
    float peak = 0;
    for (float x : mix) peak = std::max(peak, std::fabs(x));
    const float g = peak > 0 ? 0.78f / peak : 1.f;
    for (float& x : mix) x *= g;
    // Soft limit the SFX peaks; keep the pad-driven loudness intact.
    const float lim = 0.56f, knee = 0.04f;
    for (float& x : mix) {
        const float a = std::fabs(x);
        if (a > lim) x = std::copysign(lim + knee * std::tanh((a - lim) / knee), x);
    }
    media::writeWav(path, mix, procgen::Score::RATE);
}
void writeSrtFile() {
    sub::Track tr;
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < CH[i].n; ++j) {
            const double from = (ST[i] + lineStartLocal(i, j)) / (double)FPS;
            const double to = (j + 1 < CH[i].n)
                                  ? (ST[i] + lineStartLocal(i, j + 1)) / (double)FPS
                                  : ((i + 1 < N ? ST[i + 1] : TOTAL) / (double)FPS);
            tr.cues.push_back({ from, to, CH[i].lines[j].text, false });
        }
    }
    sub::writeSrt(SRT_PATH, tr);
}
bool assetsReady = false;
void ensureAssets() {
    std::filesystem::create_directories(ASSETS);
    const std::string score = std::string(ASSETS) + "/score.wav";
    if (!std::filesystem::exists(score)) genScore(score);
    writeSrtFile();
    assetsReady = true;
}

// ------------------------------------------------------------------ thumbnail
void drawThumb() {
    backdrop();
    feedText("NOT", 64, 82, 800, 92, ink, true, 190);
    feedText("CONTRADICTED.", 64, 188, 900, 92, ink, true, 300);
    feedText("NOT BACKED UP.", 64, 292, 900, 76, coral, true, 390);
    feedText("A smoke-only guess passes the rule. It is not supported.", 64, 408, 830, 28, muted, false, 500);
    world(45, 440, ht::SMOKE, 0, true, amber, "THERE", "our guess", true);
    world(595, 440, 0, 0, false, teal, "HERE", "what we keep", false);
    feedText("Nothing contradicts it. That is not enough.", 64, 1180, 900, 30, ink, false, 1270);
    feedText("Cabalar 2021 / Aguado et al. 2013", 64, 1288, 760, 22, withAlpha(muted, .9f), false, 1325);
    feedText("raymotion", 820, 1288, 190, 22, withAlpha(muted, .7f), true, 1325);
}

// ------------------------------------------------------------------ registration
void buildFilm(Comp& c) {
    TNode* node = addRoot(c.roots, 0, TOTAL);
    node->fn = [](const Ctx&, int frame) { drawFilm(frame); };
}
void buildThumb(Comp& c) {
    TNode* node = addRoot(c.roots, 0, c.duration);
    node->fn = [](const Ctx&, int) { drawThumb(); };
}
} // namespace

void registerHtFeedV2(Registry& out) {
    Comp c;
    c.name = "here-there-feed-v2";
    c.w = W; c.h = H; c.fps = FPS; c.duration = TOTAL;
    c.background = paper;
    c.assets = [](const Comp&, const Json&) { ensureAssets(); };
    c.build = [](Comp& c, const Json&) {
        c.roots.clear();
        media::audio(addRoot(c.roots, 0, TOTAL), std::string(ASSETS) + "/score.wav");
        if (!assetsReady) return;
        buildFilm(c);
    };
    out.push_back(std::move(c));

    Comp t;
    t.name = "here-there-feed-v2-thumb";
    t.w = W; t.h = H; t.fps = FPS; t.duration = 60;
    t.background = paper;
    t.build = [](Comp& c, const Json&) { c.roots.clear(); buildThumb(c); };
    out.push_back(std::move(t));
}
} // namespace rmtn
