// Here-and-There: a source-grounded, text-only explainer for 1920x1080.
// The script, cue map and source ledger are in docs/here-there-explainer-v2-*.md.
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
using dr::V2;
constexpr int FPS = 30, W = 1920, H = 1080, N = 9, XF = 10;
constexpr float WPS = 2.5f;
constexpr const char* ASSETS = "out/ht-v2-assets";
constexpr const char* SRT_PATH = "out/here-there-explainer-v2.srt";

const Color BG{ 11, 20, 33, 255 }, PANEL{ 21, 37, 54, 255 }, INK{ 234, 240, 246, 255 };
const Color DIM{ 154, 174, 194, 255 }, CYAN{ 71, 218, 216, 255 }, HTGOLD{ 255, 195, 101, 255 };
const Color HTGREEN{ 115, 226, 167, 255 }, CORAL{ 255, 120, 139, 255 };
const Color PALE_CYAN{ 178, 244, 241, 255 }, PALE_GOLD{ 255, 226, 182, 255 };

struct Line { const char* text; };
struct Chapter { const char* title; const char* sub; const Line* lines; int n; float tail; };

const Line L1[] = {
    { "A rule says: if fire, then smoke." },
    { "Classical logic accepts three models: empty, smoke alone, fire and smoke." },
    { "All three satisfy the rule. But two invent atoms it never required." },
};
const Line L2[] = {
    { "Here-and-There evaluates one instant at two levels." },
    { "HERE holds what is established; THERE holds what is assumed." },
    { "HERE is a subset of THERE. The reverse need not hold." },
};
const Line L3[] = {
    { "For one atom, H subset T leaves three states." },
    { "Absent, assumed only, or established." },
    { "The fourth case, in HERE but not THERE, is impossible." },
};
const Line L4[] = {
    { "Check fire -> smoke in HERE and in THERE." },
    { "H empty, T fire: HERE passes; THERE fails." },
    { "Add smoke: both pass. Move fire into HERE: HERE fails." },
    { "Add smoke to HERE: both pass." },
};
const Line L5[] = {
    { "Not p means p implies false." },
    { "H empty, T = {p}: p is not true HERE." },
    { "p is in THERE, so not p is false HERE too." },
    { "So p or not p fails HERE." },
};
const Line L6[] = {
    { "Equilibrium starts with a total model (T,T)." },
    { "Keep T fixed; shrink HERE." },
    { "Take T = {smoke}. H empty still satisfies the rule." },
    { "A smaller HERE works, so {smoke} is rejected." },
};
const Line L7[] = {
    { "The rule alone has one equilibrium model: empty." },
    { "Add the fact fire." },
    { "Remove fire and the fact breaks; remove smoke and the rule breaks." },
    { "Only {fire, smoke} survives." },
};
const Line L8[] = {
    { "Time adds a second axis." },
    { "At each instant, HERE is a subset of THERE." },
    { "X means next; G means always." },
    { "Fire now, always fire -> next smoke, yields smoke next." },
};
const Line L9[] = {
    { "Two levels of truth, a minimality test, a time axis." },
    { "That is the route from HT to temporal stable models." },
};

const Chapter CH[N] = {
    { "A rule should not invent facts.", "Cabalar's fire -> smoke example: which models are allowed?", L1, 3, 0.8f },
    { "One instant, two levels.", "HERE is established; THERE is assumed. The pair is not now and later.", L2, 3, 0.8f },
    { "One atom, three states.", "H subset T is an exhaustive constraint on a single atom.", L3, 3, 0.8f },
    { "Both checks.", "The rule has to pass at (H,T) and at the total world (T,T).", L4, 4, 0.8f },
    { "Not established is not false.", "Negation is p -> false; exclusions are checked at THERE.", L5, 4, 0.8f },
    { "Equilibrium: no smaller HERE.", "Keep T fixed; reject a total model if a proper smaller H still works.", L6, 4, 0.8f },
    { "A fact changes the model.", "The rule alone and the rule plus fire have different stable models.", L7, 4, 0.8f },
    { "Add time.", "H and T become traces; X is next and G is always.", L8, 4, 0.8f },
    { "The route.", "HT semantics -> equilibrium -> temporal stable models.", L9, 2, 1.2f },
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
std::array<int, N> makeStarts(const std::array<int, N>& d) {
    std::array<int, N> s{};
    for (int i = 1; i < N; ++i) s[i] = s[i - 1] + d[i - 1] - XF;
    return s;
}
const std::array<int, N> ST = makeStarts(DUR);
const int TOTAL = ST[N - 1] + DUR[N - 1];

int lineStartLocal(int ch, int j) {
    int words = 0;
    for (int k = 0; k < j; ++k) words += wordsIn(CH[ch].lines[k].text);
    return (int)std::lround(words / WPS * FPS);
}

// ------------------------------------------------------------------ small tools
float reveal(int t, int start, int len) {
    if (len <= 0) return t >= start ? 1.f : 0.f;
    return (float)interpolate(t, { (double)start, (double)(start + len) }, { 0, 1 },
                              clamped(ease::out(ease::cubic)));
}
float clamp01(float x) { return std::max(0.f, std::min(1.f, x)); }
Color ink(float a) { return withAlpha(INK, a); }

void txt(const Ctx& c, const char* s, float x, float y, float size, Color col,
         double ax = 0, double ay = 0, bool bold = false) {
    dr::text(fonts::ui(bold), c, s, { x, y }, size, 0, col, ax, ay);
}
void txt(const Ctx& c, const std::string& s, float x, float y, float size, Color col,
         double ax = 0, double ay = 0, bool bold = false) {
    txt(c, s.c_str(), x, y, size, col, ax, ay, bold);
}
void center(const Ctx& c, const char* s, float x, float y, float size, Color col, bool bold = false) {
    txt(c, s, x, y, size, col, 0.5, 0.5, bold);
}
void center(const Ctx& c, const std::string& s, float x, float y, float size, Color col, bool bold = false) {
    txt(c, s, x, y, size, col, 0.5, 0.5, bold);
}
void panel(const Ctx& c, float x, float y, float w, float h, Color edge) {
    dr::rect(c, x, y, w, h, withAlpha(PANEL, 0.94f));
    dr::rect(c, x, y, w, 3, withAlpha(edge, 0.95f));
}
void outline(const Ctx& c, float x, float y, float w, float h, Color edge, float a = 1) {
    dr::rect(c, x, y, w, h, withAlpha(edge, 0.18f * a));
    dr::rect(c, x, y, w, 3, withAlpha(edge, a));
}
void arrow(const Ctx& c, V2 a, V2 b, Color col, float thick = 2.5f) {
    dr::strokeSeg(c, a, b, thick, col);
    const float ang = std::atan2(b.y - a.y, b.x - a.x), hl = 13;
    const V2 p1{ b.x - hl * std::cos(ang - 0.52f), b.y - hl * std::sin(ang - 0.52f) };
    const V2 p2{ b.x - hl * std::cos(ang + 0.52f), b.y - hl * std::sin(ang + 0.52f) };
    const V2 tri[3] = { b, p1, p2 };
    dr::fillPoly(c, tri, 3, col);
}
void check(const Ctx& c, float x, float y, float s, Color col) {
    dr::strokeSeg(c, { x - s, y }, { x - s * .25f, y + s * .65f }, 3.2f, col);
    dr::strokeSeg(c, { x - s * .25f, y + s * .65f }, { x + s, y - s * .8f }, 3.2f, col);
}
void cross(const Ctx& c, float x, float y, float s, Color col) {
    dr::strokeSeg(c, { x - s, y - s }, { x + s, y + s }, 3.2f, col);
    dr::strokeSeg(c, { x - s, y + s }, { x + s, y - s }, 3.2f, col);
}
void chip(const Ctx& c, float x, float y, float w, float h, const char* label,
          bool present, Color col) {
    dr::roundRect(c, x, y, w, h, 8, present ? withAlpha(col, 0.15f) : withAlpha(BG, 0.55f));
    dr::roundRect(c, x, y, w, 2.5f, 1.2f, present ? withAlpha(col, 0.7f) : withAlpha(DIM, 0.22f));
    dr::circle(c, { x + 20, y + h * .5f }, 6, present ? col : withAlpha(DIM, .55f));
    txt(c, label, x + 38, y + h * .5f, 22, present ? INK : withAlpha(DIM, .7f), 0, .5f);
    if (!present) {
        dr::strokeSeg(c, { x + 15, y + h * .5f }, { x + 25, y + h * .5f }, 2, BG);
    }
}
void status(const Ctx& c, float x, float y, const char* label, bool ok) {
    Color col = ok ? HTGREEN : CORAL;
    if (ok) check(c, x + 8, y + 8, 8, col);
    else cross(c, x + 8, y + 8, 7, col);
    txt(c, label, x + 28, y, 21, col, 0, 0, true);
}
void setLabel(const Ctx& c, const char* name, unsigned mask, float x, float y, float size, Color col) {
    std::string s = std::string(name) + " = ";
    if (mask == 0) s += "{ }";
    else if (mask == ht::FIRE) s += "{ fire }";
    else if (mask == ht::SMOKE) s += "{ smoke }";
    else s += "{ fire, smoke }";
    txt(c, s, x, y, size, col, 0, .5f);
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
std::vector<std::vector<std::string>> wrapWords(Font font, const std::vector<std::string>& words,
                                                  float size, float maxW) {
    std::vector<std::vector<std::string>> lines;
    std::vector<std::string> line;
    float space = dr::measure(font, " ", size, 0).x;
    float w = 0;
    for (const auto& word : words) {
        const float ww = dr::measure(font, word.c_str(), size, 0).x;
        const float add = (line.empty() ? 0 : space) + ww;
        if (!line.empty() && w + add > maxW) {
            lines.push_back(line); line.clear(); w = 0;
        }
        if (!line.empty()) w += space;
        line.push_back(word); w += ww;
    }
    if (!line.empty()) lines.push_back(line);
    if (lines.empty()) lines.push_back({});
    return lines;
}
void drawCaptions(const Ctx& cx, int t, const std::array<int, N>& st) {
    int ch = 0;
    while (ch + 1 < N && t >= st[ch + 1]) ++ch;
    const int local = t - st[ch];
    const Chapter& c = CH[ch];
    int li = 0;
    for (int j = 0; j < c.n; ++j) if (local >= lineStartLocal(ch, j)) li = j;
    const int from = lineStartLocal(ch, li);
    const int next = (li + 1 < c.n) ? lineStartLocal(ch, li + 1)
                                    : (ch + 1 < N ? st[ch + 1] - st[ch] : TOTAL - st[ch]);
    const float a = reveal(local, from, 5);
    if (a <= 0 || local >= next + 5) return;
    const std::vector<std::string> words = splitWords(c.lines[li].text);
    const Font f = fonts::ui(true);
    const float size = 30, maxW = 1580;
    const auto lines = wrapWords(f, words, size, maxW);
    const float lineH = size * 1.25f;
    const float y0 = 820 - (lines.size() - 1) * lineH * .5f;
    const int spoken = (int)std::floor(std::max(0.f, (local - from - 2) / (float)FPS) * WPS);
    int idx = 0;
    Ctx m = cx;
    m.a *= a;
    for (size_t k = 0; k < lines.size(); ++k) {
        const auto& line = lines[k];
        float total = -dr::measure(f, " ", size, 0).x;
        for (const auto& word : line) total += dr::measure(f, word.c_str(), size, 0).x + dr::measure(f, " ", size, 0).x;
        float x = 960 - total * .5f;
        const float y = y0 + (float)k * lineH;
        for (const auto& word : line) {
            const float ww = dr::measure(f, word.c_str(), size, 0).x;
            const Color col = idx < spoken ? INK : withAlpha(INK, .32f);
            txt(m, word, x, y, size, col, 0, .5f, true);
            x += ww + dr::measure(f, " ", size, 0).x;
            ++idx;
        }
    }
    // A quiet text-only marker; no caption box or per-word highlight.
    dr::rect(m, 96, 884, 34, 2, withAlpha(CYAN, .55f));
    txt(m, "TEXT-ONLY CUT", 142, 876, 16, withAlpha(DIM, .8f));
}

// ------------------------------------------------------------------ chrome
void backdrop(const Ctx& cx) {
    dr::rectGrad4(cx, 0, 0, W, H, withAlpha(BG, 1), withAlpha(BG, 1),
                  mkColor(0x07101a), mkColor(0x07101a));
    // A restrained paper-grid: static, low contrast, never moving.
    for (int y = 80; y < H; y += 64) {
        dr::strokeSeg(cx, { 0, (float)y }, { (float)W, (float)y }, 1, withAlpha(DIM, .025f));
    }
    for (int x = 80; x < W; x += 64) {
        dr::strokeSeg(cx, { (float)x, 0 }, { (float)x, (float)H }, 1, withAlpha(DIM, .025f));
    }
}
void drawChrome(const Ctx& cx, int t, const std::array<int, N>& st) {
    int ch = 0;
    while (ch + 1 < N && t >= st[ch + 1]) ++ch;
    const int local = t - st[ch];
    const float a = reveal(local, 0, 12);
    Ctx m = cx; m.a *= a;
    txt(m, "HERE-AND-THERE / LOGIC EXPLAINED", 96, 52, 21, withAlpha(DIM, .95f));
    char counter[32]; std::snprintf(counter, sizeof counter, "%02d / %02d", ch + 1, N);
    txt(m, counter, 1824, 52, 21, CYAN, 1, 0);
    dr::rect(cx, 96, 87, 1728, 2, withAlpha(DIM, .18f));
    const float progress = std::max(0.f, std::min(1.f, (float)t / (TOTAL - 1)));
    dr::rect(cx, 96, 87, 1728 * progress, 2, CYAN);
    txt(m, CH[ch].title, 96, 112 + (1 - a) * 12, 54, INK, 0, 0, true);
    txt(m, CH[ch].sub, 96, 182, 25, withAlpha(DIM, .95f));
    dr::rect(m, 96, 232, 86, 3, withAlpha(CYAN, .7f));
    // Source line: the lecture's provenance is always visible.
    txt(cx, "Based on Cabalar 2021, sec. 2 / Aguado et al. 2013, secs. 2-3", 96, 1000, 18,
        withAlpha(DIM, .78f));
    txt(cx, "raymotion / explainer", 1824, 1000, 18, withAlpha(DIM, .6f), 1, 0);
}

// ------------------------------------------------------------------ diagrams
void rulePill(const Ctx& cx, float x, float y, float w, float a) {
    Ctx m = cx; m.a *= a;
    dr::roundRect(m, x, y, w, 78, 39, withAlpha(CYAN, .10f));
    dr::roundRect(m, x, y, w, 3, 1.5f, withAlpha(CYAN, .8f));
    txt(m, "THE RULE", x + 26, y + 18, 18, withAlpha(CYAN, .9f), 0, 0, true);
    center(m, "fire -> smoke", x + w * .5f, y + 53, 34, INK, true);
}
void pairPanel(const Ctx& cx, float x, float y, float w, const char* label, Color col,
               unsigned mask, float a, const char* note = nullptr) {
    Ctx m = cx; m.a *= a;
    panel(m, x, y, w, 156, col);
    txt(m, label, x + 24, y + 22, 23, col, 0, 0, true);
    chip(m, x + 250, y + 50, 200, 52, "fire", (mask & ht::FIRE) != 0, col);
    chip(m, x + 470, y + 50, 200, 52, "smoke", (mask & ht::SMOKE) != 0, col);
    if (w >= 1000)
        setLabel(m, label[0] == 'H' ? "H" : "T", mask, x + w - 280, y + 76, 28, INK);
    if (note) txt(m, note, x + w - 280, y + 130, 17, withAlpha(DIM, .8f));
}
void ch1(const Ctx& cx, int t) {
    rulePill(cx, 700, 264, 520, reveal(t, 0, 14));
    const char* sets[3] = { "{ }", "{ smoke }", "{ fire, smoke }" };
    const char* tags[3] = { "no extra atoms", "invents smoke", "invents fire + smoke" };
    for (int i = 0; i < 3; ++i) {
        const float a = reveal(t, 12 + i * 8, 16);
        if (a <= 0) continue;
        const float x = 120.f + i * 600.f, y = 390 + (1 - a) * 14;
        Ctx m = cx; m.a *= a;
        panel(m, x, y, 480, 274, i == 0 ? HTGREEN : HTGOLD);
        txt(m, "CLASSICAL MODEL", x + 24, y + 24, 18, withAlpha(DIM, .9f));
        char idx[16]; std::snprintf(idx, sizeof idx, "%d", i + 1);
        txt(m, idx, x + 456, y + 24, 18, withAlpha(DIM, .8f), 1, 0);
        center(m, sets[i], x + 240, y + 128, 36, INK, true);
        if (t > 90) status(m, x + 24, y + 208, "satisfies the rule", true);
        if (t > 240) {
            const Color tc = i == 0 ? HTGREEN : CORAL;
            const float tw = dr::measure(fonts::ui(true), tags[i], 18, 0).x;
            dr::roundRect(m, x + 456 - tw - 20, y + 250, tw + 20, 24, 6, withAlpha(tc, .14f));
            txt(m, tags[i], x + 446, y + 253, 18, tc, 1, 0);
        }
    }
    txt(cx, "All three satisfy. Which model should a rule allow?", 960, 730, 22,
        withAlpha(DIM, .85f), .5, 0);
}
void ch2(const Ctx& cx, int t) {
    const float aH = reveal(t, 0, 16), aT = reveal(t, 6, 16);
    pairPanel(cx, 200, 292, 1520, "HERE / H", CYAN, ht::FIRE, aH, "established / stronger truth");
    pairPanel(cx, 200, 520, 1520, "THERE / T", HTGOLD, ht::FIRE | ht::SMOKE, aT, "assumed true");
    const float ab = reveal(t, 84, 16);
    if (ab > 0) {
        Ctx m = cx; m.a *= ab;
        dr::roundRect(m, 770, 472, 380, 40, 20, withAlpha(BG, .82f));
        dr::roundRect(m, 770, 472, 380, 2, 1, withAlpha(DIM, .35f));
        center(m, "H subset T", 960, 492, 24, INK, true);
        arrow(m, { 960, 512 }, { 960, 492 }, withAlpha(DIM, .7f), 2);
    }
    const float an = reveal(t, 204, 16);
    if (an > 0) {
        Ctx m = cx; m.a *= an;
        txt(m, "one instant, not now / later", 960, 735, 23, withAlpha(DIM, .9f), .5, 0, true);
    }
}
void stateCard(const Ctx& cx, float x, float y, const char* title, const char* detail,
               Color col, bool forbidden, float a) {
    if (a <= 0) return;
    Ctx m = cx; m.a *= a;
    panel(m, x, y, 420, 208, forbidden ? CORAL : col);
    txt(m, title, x + 22, y + 22, 24, forbidden ? CORAL : col, 0, 0, true);
    txt(m, detail, x + 22, y + 78, 22, INK);
    txt(m, forbidden ? "H present, T absent" : "", x + 22, y + 116, 18, withAlpha(DIM, .8f));
    if (forbidden) {
        cross(m, x + 348, y + 165, 22, CORAL);
        txt(m, "forbidden", x + 250, y + 165, 18, CORAL, .5, .5, true);
    } else {
        dr::circle(m, { x + 42, y + 170 }, 7, col);
        txt(m, "allowed", x + 60, y + 159, 18, withAlpha(DIM, .9f));
    }
}
void ch3(const Ctx& cx, int t) {
    txt(cx, "T: p absent", 690, 270, 22, withAlpha(HTGOLD, .8f), .5, 0);
    txt(cx, "T: p present", 1290, 270, 22, withAlpha(HTGOLD, .8f), .5, 0);
    txt(cx, "H: p absent", 330, 370, 22, withAlpha(CYAN, .8f), 1, .5);
    txt(cx, "H: p present", 330, 620, 22, withAlpha(CYAN, .8f), 1, .5);
    stateCard(cx, 480, 300, "FALSE", "p in neither H nor T", DIM, false, reveal(t, 12, 14));
    stateCard(cx, 1080, 300, "ASSUMED ONLY", "p in T, not in H", HTGOLD, false, reveal(t, 24, 14));
    stateCard(cx, 480, 550, "IMPOSSIBLE", "p in H, not in T", CORAL, true, reveal(t, 168, 14));
    stateCard(cx, 1080, 550, "ESTABLISHED", "p in both H and T", CYAN, false, reveal(t, 108, 14));
}
void caseCard(const Ctx& cx, float x, float y, float w, const char* name, unsigned h, unsigned tt,
              float a, bool key) {
    if (a <= 0) return;
    Ctx m = cx; m.a *= a;
    panel(m, x, y, w, 356, key ? CORAL : DIM);
    txt(m, name, x + 20, y + 20, 18, key ? CORAL : withAlpha(DIM, .9f), 0, 0, true);
    setLabel(m, "H", h, x + 20, y + 78, 24, CYAN);
    setLabel(m, "T", tt, x + 20, y + 124, 24, HTGOLD);
    const bool hc = !(h & ht::FIRE) || (h & ht::SMOKE);
    const bool tc = !(tt & ht::FIRE) || (tt & ht::SMOKE);
    status(m, x + 20, y + 208, hc ? "H check: PASS" : "H check: FAIL", hc);
    status(m, x + 20, y + 252, tc ? "T check: PASS" : "T check: FAIL", tc);
    txt(m, key ? "key failure" : "one case", x + 20, y + 312, 17,
        withAlpha(key ? CORAL : DIM, .85f));
}
void ch4(const Ctx& cx, int t) {
    struct C { const char* name; unsigned h, t; };
    const C cases[4] = {
        { "CASE 1", 0, ht::FIRE },
        { "CASE 2", 0, ht::FIRE | ht::SMOKE },
        { "CASE 3", ht::FIRE, ht::FIRE | ht::SMOKE },
        { "CASE 4", ht::FIRE | ht::SMOKE, ht::FIRE | ht::SMOKE },
    };
    for (int i = 0; i < 4; ++i) {
        const int start = i == 0 ? 0 : (i == 1 ? 108 : (i == 2 ? 204 : 324));
        caseCard(cx, 120.f + i * 430.f, 286, 396, cases[i].name, cases[i].h, cases[i].t,
                 reveal(t, start, 12), i == 2);
    }
    txt(cx, "The two checks are (H,T) and the total world (T,T).", 960, 714, 21,
        withAlpha(DIM, .82f), .5, 0);
}
void truthRow(const Ctx& cx, float x, float y, const char* label, const char* value,
              bool ok, float a) {
    if (a <= 0) return;
    Ctx m = cx; m.a *= a;
    dr::roundRect(m, x, y, 740, 58, 9, withAlpha(PANEL, .8f));
    txt(m, label, x + 22, y + 29, 23, INK, 0, .5f, true);
    txt(m, value, x + 500, y + 29, 22, ok ? HTGREEN : CORAL, 0, .5f, true);
    if (ok) check(m, x + 690, y + 29, 9, HTGREEN);
    else cross(m, x + 690, y + 29, 8, CORAL);
}
void ch5(const Ctx& cx, int t) {
    const float al = reveal(t, 0, 14);
    if (al > 0) {
        Ctx m = cx; m.a *= al;
        panel(m, 120, 286, 760, 362, HTGOLD);
        txt(m, "THE MIDDLE STATE", 146, 310, 20, HTGOLD, 0, 0, true);
        txt(m, "H = { }", 146, 370, 34, CYAN, 0, .5f, true);
        txt(m, "T = { p }", 146, 424, 34, HTGOLD, 0, .5f, true);
        txt(m, "p is not true HERE.", 146, 510, 25, INK);
        txt(m, "But p is still assumed THERE.", 146, 552, 23, withAlpha(HTGOLD, .9f));
        dr::rect(m, 146, 604, 160, 2, withAlpha(HTGOLD, .45f));
        txt(m, "no contradiction", 146, 616, 18, withAlpha(DIM, .8f));
    }
    const float ar = reveal(t, 24, 14);
    if (ar > 0) {
        Ctx m = cx; m.a *= ar;
        panel(m, 960, 286, 840, 362, CYAN);
        txt(m, "CHECK AT HERE", 986, 310, 20, CYAN, 0, 0, true);
        truthRow(m, 986, 356, "p", "FALSE", false, reveal(t, 72, 14));
        truthRow(m, 986, 430, "not p", "FALSE", false, reveal(t, 192, 14));
        truthRow(m, 986, 504, "p or not p", "FALSE", false, reveal(t, 324, 14));
        txt(m, "not p holds only when p is absent from THERE.", 986, 594, 20,
            withAlpha(DIM, .85f));
    }
}
void ch6(const Ctx& cx, int t) {
    const float al = reveal(t, 0, 14);
    if (al > 0) {
        Ctx m = cx; m.a *= al;
        panel(m, 120, 286, 740, 390, HTGOLD);
        txt(m, "TOTAL MODEL (T,T)", 146, 310, 20, HTGOLD, 0, 0, true);
        setLabel(m, "H", ht::SMOKE, 146, 382, 32, CYAN);
        setLabel(m, "T", ht::SMOKE, 146, 434, 32, HTGOLD);
        chip(m, 146, 492, 194, 50, "smoke", true, HTGOLD);
        txt(m, "T is fixed; H is tested.", 146, 592, 23, INK);
        dr::roundRect(m, 610, 310, 214, 38, 8, withAlpha(HTGOLD, .14f));
        txt(m, "T FIXED", 624, 319, 18, HTGOLD, 0, 0, true);
    }
    const float ar = reveal(t, 144, 14);
    if (ar > 0) {
        Ctx m = cx; m.a *= ar;
        panel(m, 1040, 286, 760, 390, CYAN);
        txt(m, "SMALLER H", 1066, 310, 20, CYAN, 0, 0, true);
        setLabel(m, "H", 0, 1066, 382, 32, CYAN);
        setLabel(m, "T", ht::SMOKE, 1066, 434, 32, HTGOLD);
        chip(m, 1066, 492, 194, 50, "fire", false, CYAN);
        chip(m, 1278, 492, 194, 50, "smoke", true, HTGOLD);
        if (t >= 264) {
            const bool ok = ht::theory(0, ht::SMOKE);
            status(m, 1066, 574, ok ? "rule still holds" : "rule fails", ok);
            dr::roundRect(m, 1066, 616, 260, 42, 8, withAlpha(CORAL, .13f));
            txt(m, "REJECT {smoke}", 1086, 626, 20, CORAL, 0, 0, true);
        }
    }
    arrow(cx, { 908, 470 }, { 1020, 470 }, withAlpha(DIM, .7f), 2.2f);
    txt(cx, "shrink H", 964, 442, 18, withAlpha(DIM, .8f), .5, 0);
    txt(cx, "No proper smaller H remains = equilibrium.", 960, 724, 22,
        withAlpha(DIM, .85f), .5, 0);
}
void ch7(const Ctx& cx, int t) {
    const float al = reveal(t, 0, 14);
    if (al > 0) {
        Ctx m = cx; m.a *= al;
        panel(m, 120, 286, 780, 410, CYAN);
        txt(m, "RULE ONLY", 146, 310, 20, CYAN, 0, 0, true);
        txt(m, "fire -> smoke", 146, 356, 32, INK, 0, 0, true);
        txt(m, "stable model", 146, 438, 19, withAlpha(DIM, .85f));
        txt(m, "{ }", 146, 480, 48, HTGREEN, 0, .5f, true);
        txt(m, "No facts require either atom.", 146, 570, 22, withAlpha(DIM, .9f));
    }
    const float ar = reveal(t, 96, 14);
    if (ar > 0) {
        Ctx m = cx; m.a *= ar;
        panel(m, 1020, 286, 780, 410, HTGOLD);
        txt(m, "RULE + FACT fire", 1046, 310, 20, HTGOLD, 0, 0, true);
        txt(m, "fire -> smoke", 1046, 356, 32, INK, 0, 0, true);
        txt(m, "fact fire", 1046, 402, 24, HTGOLD, 0, 0, true);
        txt(m, "stable model", 1046, 438, 19, withAlpha(DIM, .85f));
        txt(m, "{ fire, smoke }", 1046, 480, 40, HTGREEN, 0, .5f, true);
        if (t >= 144) {
            const bool ok = ht::equilibrium(ht::FIRE | ht::SMOKE, true);
            status(m, 1046, 550, "only {fire, smoke} survives", ok);
            txt(m, "remove fire -> fact fails", 1046, 590, 19, withAlpha(CORAL, .9f));
            txt(m, "remove smoke -> rule fails", 1046, 620, 19, withAlpha(CORAL, .9f));
        }
        dr::roundRect(m, 1460, 310, 310, 34, 7, withAlpha(HTGOLD, .13f));
        txt(m, "worked example", 1474, 318, 17, HTGOLD, 0, 0, true);
    }
    txt(cx, "The fact picks the second stable model.", 960, 724, 22,
        withAlpha(DIM, .85f), .5, 0);
}
void ch8(const Ctx& cx, int t) {
    const float a = reveal(t, 0, 14);
    Ctx m = cx; m.a *= a;
    txt(m, "G (always)", 180, 256, 20, withAlpha(HTGOLD, .8f));
    dr::strokeSeg(m, { 180, 286 }, { 1720, 286 }, 2, withAlpha(HTGOLD, .35f));
    for (int i = 0; i < 3; ++i) {
        const float x = 180.f + i * 580.f;
        char time[20]; std::snprintf(time, sizeof time, "time %d", i);
        txt(m, time, x, 312, 20, withAlpha(DIM, .85f));
        panel(m, x, 352, 400, 104, CYAN);
        panel(m, x, 502, 400, 104, HTGOLD);
        txt(m, "H", x + 20, 404, 24, CYAN, 0, .5f, true);
        txt(m, "T", x + 20, 554, 24, HTGOLD, 0, .5f, true);
        const char* val = i == 0 ? "{ fire }" : (i == 1 ? "{ smoke }" : "{ }");
        center(m, val, x + 240, 404, 28, INK, true);
        center(m, val, x + 240, 554, 28, INK, true);
        if (i == 1 && t >= 240) check(m, x + 368, 554, 10, HTGREEN);
        if (i < 2) {
            arrow(m, { x + 410, 618 }, { x + 540, 618 }, withAlpha(CYAN, .75f), 2.2f);
            txt(m, "X", x + 475, 586, 20, CYAN, .5, 0, true);
        }
    }
    txt(m, "...", 1700, 606, 34, withAlpha(DIM, .8f));
    txt(m, "H subset T at every instant; the trace continues.", 960, 690, 22,
        withAlpha(DIM, .85f), .5, 0);
    txt(m, "schematic: three-position prefix of an infinite trace", 960, 760, 19,
        withAlpha(DIM, .6f), .5, 0);
}
void routeCard(const Ctx& cx, float x, const char* kicker, const char* label, Color col, float a) {
    if (a <= 0) return;
    Ctx m = cx; m.a *= a;
    outline(m, x, 312, 360, 168, col, .7f);
    txt(m, kicker, x + 22, 334, 17, withAlpha(col, .9f), 0, 0, true);
    txt(m, label, x + 22, 382, 27, INK, 0, 0, true);
    dr::circle(m, { x + 326, 338 }, 7, col);
}
void ch9(const Ctx& cx, int t) {
    routeCard(cx, 120, "01 / SETS", "HT worlds", CYAN, reveal(t, 0, 14));
    routeCard(cx, 540, "02 / MODELS", "HT models", CYAN, reveal(t, 10, 14));
    routeCard(cx, 960, "03 / SELECT", "equilibrium", HTGOLD, reveal(t, 20, 14));
    routeCard(cx, 1380, "04 / TIME", "temporal stable", HTGOLD, reveal(t, 30, 14));
    for (int i = 0; i < 3; ++i) arrow(cx, { 492.f + i * 420, 396 }, { 528.f + i * 420, 396 },
                                     withAlpha(DIM, .6f), 2.2f);
    const float as = reveal(t, 72, 14);
    if (as > 0) {
        Ctx m = cx; m.a *= as;
        panel(m, 96, 540, 1728, 238, CYAN);
        txt(m, "PRIMARY SOURCES", 120, 562, 20, CYAN, 0, 0, true);
        txt(m, "Pedro Cabalar (2021), Temporal ASP, section 2.", 120, 604, 25, INK);
        txt(m, "www.dc.fi.udc.es/~cabalar/RW2021.pdf", 120, 642, 22, withAlpha(HTGOLD, .9f));
        txt(m, "Aguado, Cabalar, Dieguez, Perez & Vidal (2013), TEL survey, sections 2-3.", 120, 686, 23, INK);
        txt(m, "www.dc.fi.udc.es/~cabalar/tel-dp60.pdf", 120, 722, 22, withAlpha(HTGOLD, .9f));
        txt(m, "Diagrams and worked examples are our own.", 120, 758, 18, withAlpha(DIM, .8f));
    }
}
void drawChapter(const Ctx& cx, int ch, int t) {
    switch (ch) {
    case 0: ch1(cx, t); break;
    case 1: ch2(cx, t); break;
    case 2: ch3(cx, t); break;
    case 3: ch4(cx, t); break;
    case 4: ch5(cx, t); break;
    case 5: ch6(cx, t); break;
    case 6: ch7(cx, t); break;
    case 7: ch8(cx, t); break;
    default: ch9(cx, t); break;
    }
}

// ------------------------------------------------------------------ score / assets
void genScore(const std::string& path, const std::array<int, N>& st) {
    procgen::Score s(TOTAL / (double)FPS);
    auto at = [&](int i, double f) { return (st[i] + f) / FPS; };
    const int roots[N] = { 45, 45, 41, 43, 41, 38, 43, 45, 45 };
    for (int i = 0; i < N; ++i) {
        s.pad(at(i, 0), (DUR[i] / (double)FPS) - 0.25, roots[i], 0.145f, 620, 0.45f);
        s.pad(at(i, 0.5), (DUR[i] / (double)FPS) - 0.8, roots[i] + 7, 0.070f, 900, 0.55f);
    }
    // Chapter marks and line reveals.
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < CH[i].n; ++j) {
            const double t = at(i, lineStartLocal(i, j));
            if (j == 0) s.pluck(t, 69 + (i % 3) * 5, 0.06f, 0.5f, 4.5f);
            else s.tick(t, 0.035f);
        }
    }
    s.tick(0, 0.08f);
    s.pluck(at(1, 0), 74, 0.07f, 0.4f, 4.5f);
    s.pluck(at(1, 3.2), 79, 0.055f, 0.55f, 4.5f);
    s.glitch(at(2, 5.6), 0.16f, 3);
    s.bell(at(3, 3.2), 76, 0.05f, 0.4f);
    s.bell(at(3, 6.8), 79, 0.05f, 0.45f);
    s.impact(at(3, 9.0), 0.28f);
    s.bell(at(3, 11.2), 83, 0.05f, 0.5f);
    s.impact(at(4, 10.8), 0.22f);
    s.pluck(at(4, 0), 67, 0.05f, 0.4f, 5.0f);
    s.pluck(at(5, 0), 62, 0.06f, 0.4f, 5.0f);
    s.tick(at(5, 2.8), 0.07f);
    s.impact(at(5, 8.8), 0.22f);
    s.impact(at(6, 3.2), 0.26f);
    s.tick(at(6, 4.8), 0.06f);
    s.tick(at(6, 10.0), 0.06f);
    s.pluck(at(6, 10.0), 81, 0.05f, 0.55f, 4.5f);
    s.tick(at(7, 0), 0.07f);
    s.tick(at(7, 2.0), 0.05f);
    s.tick(at(7, 5.6), 0.05f);
    s.bell(at(7, 8.0), 79, 0.045f, 0.45f);
    s.impact(at(8, 0), 0.24f);
    s.pluck(at(8, 4.0), 81, 0.05f, 0.4f, 4.5f);
    s.bell(at(8, 5.0), 84, 0.045f, 0.55f);

    std::vector<float> mix = s.mix();
    for (float& x : mix) x = std::tanh(x * 1.35f);
    float peak = 0;
    for (float x : mix) peak = std::max(peak, std::fabs(x));
    const float g = peak > 0 ? 0.78f / peak : 1.f;
    for (float& x : mix) x *= g;
    media::writeWav(path, mix, procgen::Score::RATE);
}
void writeSrtFile(const std::array<int, N>& st) {
    sub::Track tr;
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < CH[i].n; ++j) {
            const double from = (st[i] + lineStartLocal(i, j)) / (double)FPS;
            const double to = (j + 1 < CH[i].n)
                                  ? (st[i] + lineStartLocal(i, j + 1)) / (double)FPS
                                  : ((i + 1 < N ? st[i + 1] : TOTAL) / (double)FPS);
            tr.cues.push_back({ from, to, CH[i].lines[j].text, false });
        }
    }
    sub::writeSrt(SRT_PATH, tr);
}
bool assetsReady = false;
void ensureAssets(const std::array<int, N>& st) {
    std::filesystem::create_directories(ASSETS);
    const std::string score = std::string(ASSETS) + "/score.wav";
    if (!std::filesystem::exists(score)) genScore(score, st);
    writeSrtFile(st);
    assetsReady = true;
}

// ------------------------------------------------------------------ film
void buildFilm(Comp& c, const std::array<int, N>& st) {
    TNode* bg = addRoot(c.roots, 0, TOTAL);
    bg->fn = [](const Ctx& cx, int) { backdrop(cx); };

    TransitionSeries ts(c.roots, W, H);
    for (int i = 0; i < N; ++i) {
        if (i) ts.transition(Transition::fade(XF));
        TNode* n = ts.add(DUR[i]);
        n->fn = [i](const Ctx& cx, int t) { drawChapter(cx, i, t); };
        if (n->start != st[i])
            std::fprintf(stderr, "ht-v2 scene %d start %d expected %d\n", i, n->start, st[i]);
    }
    if (ts.end() != TOTAL)
        std::fprintf(stderr, "ht-v2 duration %d expected %d\n", ts.end(), TOTAL);

    // A short dip through the background hides the crossfade between two diagrams.
    addRoot(c.roots, 0, TOTAL)->fn = [st](const Ctx& cx, int t) {
        for (int i = 1; i < N; ++i) {
            const int d = t > st[i] ? t - st[i] : st[i] - t;
            if (d >= 12) continue;
            const float a = d <= 5 ? 1.f : 1.f - (d - 5) / 7.f;
            dr::rect(cx, 0, 0, W, H, withAlpha(BG, a));
        }
    };
    addRoot(c.roots, 0, TOTAL)->fn = [st](const Ctx& cx, int t) { drawChrome(cx, t, st); };
    addRoot(c.roots, 0, TOTAL)->fn = [st](const Ctx& cx, int t) { drawCaptions(cx, t, st); };

    TNode* logo = prefab::use(c.roots, st[8] + 120, TOTAL - (st[8] + 120), "raymotion-logo",
        Json::parse(R"({"size": 120, "layout": "horizontal", "tagline": "video as code", "delay": 4})"));
    logo->props.pos = { 1640, 900 };
}

void buildThumb(Comp& c) {
    addRoot(c.roots, 0, c.duration)->fn = [](const Ctx& cx, int) {
        backdrop(cx);
        dr::rectGrad4(cx, 0, 0, W, H, withAlpha(BG, .05f), withAlpha(BG, .05f),
                      withAlpha(mkColor(0x07101a), .65f), withAlpha(mkColor(0x07101a), .65f));
        txt(cx, "HERE-AND-THERE / EXPLAINER", 96, 82, 22, withAlpha(DIM, .9f));
        txt(cx, "A RULE SHOULD NOT", 96, 218, 82, INK, 0, 0, true);
        txt(cx, "INVENT FACTS.", 96, 316, 82, CYAN, 0, 0, true);
        txt(cx, "fire -> smoke", 96, 470, 42, INK, 0, 0, true);
        txt(cx, "one instant, two levels of truth", 96, 530, 24, withAlpha(DIM, .9f));
        pairPanel(cx, 1040, 286, 760, "HERE / H", CYAN, ht::FIRE, 1, "established");
        pairPanel(cx, 1040, 510, 760, "THERE / T", HTGOLD, ht::FIRE | ht::SMOKE, 1, "assumed");
        dr::roundRect(cx, 1290, 474, 260, 36, 18, withAlpha(BG, .85f));
        center(cx, "H subset T", 1420, 492, 22, INK, true);
        txt(cx, "Cabalar 2021 / Aguado et al. 2013", 96, 986, 20, withAlpha(DIM, .8f));
        txt(cx, "raymotion", 1824, 986, 20, withAlpha(DIM, .65f), 1, 0, true);
    };
}

} // namespace

void registerHtExplainerV2(Registry& out) {
    const std::array<int, N> st = ST;
    Comp c;
    c.name = "here-there-explainer-v2";
    c.w = W; c.h = H; c.fps = FPS; c.duration = TOTAL;
    c.background = BG;
    c.assets = [st](const Comp&, const Json&) { ensureAssets(st); };
    c.build = [st](Comp& c, const Json&) {
        c.roots.clear();
        media::audio(addRoot(c.roots, 0, TOTAL), std::string(ASSETS) + "/score.wav");
        if (!assetsReady) return;
        buildFilm(c, st);
    };
    out.push_back(std::move(c));

    Comp t;
    t.name = "here-there-explainer-v2-thumb";
    t.w = W; t.h = H; t.fps = FPS; t.duration = 60;
    t.background = BG;
    t.build = [](Comp& c, const Json&) {
        c.roots.clear();
        buildThumb(c);
    };
    out.push_back(std::move(t));
}
} // namespace rmtn
