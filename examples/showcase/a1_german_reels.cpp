// a1_german_reels.cpp -- voice-free A1 pilot: 24 caption-led reels, BGM + SFX only.
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace rmtn {
namespace {

constexpr int W = 1080, H = 1920, FPS = 30, TOTAL = 900;
constexpr float TX = 96, TW = 888;

const Color BG = mkColor(0x101820), INK = mkColor(0xf3ebdd), MUTED = mkColor(0x8fa6a0),
            CREAM = mkColor(0xf3ebdd), DARK = mkColor(0x121f24), DEEP = mkColor(0x101820),
            RULE = mkColor(0x2f4b50);
const Color TRACK_COLOR[3] = { mkColor(0x7fe0c3), mkColor(0xffb45c), mkColor(0xff8a7a) };

struct Reel {
    std::string id, level, track, title, hook, exercise, takeaway, source;
    std::vector<std::string> demo;
};

float ramp(int t, float a, float b) {
    const float u = std::clamp((t - a) / std::max(0.001f, b - a), 0.f, 1.f);
    return u * u * (3 - 2 * u);
}
Color trackColor(const std::string& t) {
    if (t == "vocabulary") return TRACK_COLOR[1];
    if (t == "situations") return TRACK_COLOR[2];
    return TRACK_COLOR[0];
}
std::string upperTrack(std::string t) {
    for (char& c : t) c = (char)std::toupper((unsigned char)c);
    return t;
}
std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
std::string clean(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), '*'), s.end());
    return s;
}

Font titleFont() { return fonts::load({ "ArchivoBlack-Regular.ttf", "Inter-Bold.ttf" }, 96); }
Font bodyFont() { return fonts::load({ "SpaceGrotesk-Variable.ttf", "Inter-Regular.ttf" }, 96); }
Font monoFont() { return fonts::load({ "JetBrainsMono-Bold.ttf" }, 64); }

void text(const Ctx& cx, Font f, const std::string& s, Vector2 at, float size, Color c,
          float ax = 0, float ay = 0.5f) {
    dr::text(f, cx, s.c_str(), at, size, 0, c, ax, ay);
}

void boxText(const Ctx& cx, const std::string& s, Rectangle box, float size, Color c,
             Align align = Align::Center) {
    TextStyle st;
    st.font = bodyFont();
    st.size = size;
    st.lineHeight = 1.18f;
    st.color = c;
    st.align = align;
    st.valign = Align::Center;
    dr::textBox(cx, s, box, st);
}

void backdrop(const Ctx& cx, const Reel& r) {
    const Color a = trackColor(r.track);
    dr::rectGrad4(cx, 0, 0, W, H, mixC(BG, a, 0.06f), BG, mkColor(0x080d12), mkColor(0x080d12));
    for (int j = 0; j < 22; j++)
        for (int i = 0; i < 13; i++)
            dr::circle(cx, { 36.f + i * 80.f + (j % 2) * 20.f, 160.f + j * 80.f }, 1.1f,
                       withAlpha(a, 0.06f + 0.02f * (j % 3)));
}

void hud(const Ctx& cx, const Reel& r, int t) {
    const Color a = trackColor(r.track);
    const Font m = monoFont();
    char page[24];
    std::snprintf(page, sizeof page, "%s / 24", r.id.c_str());
    dr::rect(cx, 72, 54, W - 144, 2, withAlpha(RULE, 0.9f));
    dr::rect(cx, 72, 54, (W - 144) * (t + 1.f) / TOTAL, 2, a);
    text(cx, m, r.level + " · " + upperTrack(r.track), { 72, 92 }, 20, a, 0, 0.5f);
    text(cx, m, page, { W - 72, 92 }, 17, MUTED, 1, 0.5f);
    text(cx, m, r.level + " · supplied brief · language review pending", { 72, 1850 }, 15,
         withAlpha(MUTED, 0.8f), 0, 0.5f);
}

void header(const Ctx& cx, const Reel& r) {
    const Color a = trackColor(r.track);
    boxText(cx, r.title, { TX, 160, TW, 130 }, 44, INK);
    dr::roundRect(cx, TX, 300, 6, 74, 3, a);
    boxText(cx, r.hook, { TX + 28, 294, TW - 40, 92 }, 31, INK, Align::Start);
}

void grammarCard(const Ctx& cx, const std::string& s, int i, float k, Color a) {
    if (k <= 0.001f) return;
    const float y = 480 + i * 190;
    Ctx m = cx;
    m.a *= std::min(1.f, k);
    m.o.y += (1 - k) * 24;
    dr::roundRect(m, TX, y, TW, 150, 20, DARK);
    dr::roundRect(m, TX, y, 8, 150, 4, a);
    char idx[8]; std::snprintf(idx, sizeof idx, "%02d", i + 1);
    text(m, monoFont(), idx, { TX + 34, y + 36 }, 18, a, 0, 0.5f);
    boxText(m, s, { TX + 90, y + 20, TW - 120, 110 }, 38, INK, Align::Start);
}

void vocabCard(const Ctx& cx, const std::string& s, int i, float k, Color a) {
    if (k <= 0.001f) return;
    const float y = 480 + i * 190;
    Ctx m = cx;
    m.a *= std::min(1.f, k);
    m.o.y += (1 - k) * 24;
    dr::roundRect(m, TX, y, TW, 150, 20, CREAM);
    dr::roundRect(m, TX, y, TW, 6, 3, a);
    char idx[8]; std::snprintf(idx, sizeof idx, "%02d", i + 1);
    text(m, monoFont(), idx, { TX + 30, y + 34 }, 17, MUTED, 0, 0.5f);
    boxText(m, s, { TX + 100, y + 18, TW - 130, 114 }, 42, DEEP, Align::Start);
}

void speechBubble(const Ctx& cx, const std::string& s, int i, float k, Color a) {
    if (k <= 0.001f) return;
    const float w = 700, x = (i % 2 == 0) ? 90 : W - 90 - w;
    const float y = 490 + i * 185;
    Ctx m = cx;
    m.a *= std::min(1.f, k);
    m.o.y += (1 - k) * 22;
    dr::roundRect(m, x, y, w, 138, 24, CREAM);
    dr::roundRect(m, x + (i % 2 == 0 ? 0 : w - 8), y + 18, 8, 102, 4, a);
    boxText(m, s, { x + 30, y + 14, w - 56, 110 }, 34, DEEP, Align::Start);
}

void practice(const Ctx& cx, const Reel& r, int t) {
    const float k = ramp(t, 570, 600);
    if (k <= 0.001f) return;
    const Color a = trackColor(r.track);
    Ctx m = cx;
    m.a *= k;
    m.o.y += (1 - k) * 20;
    dr::roundRect(m, TX, 1210, TW, 180, 22, DARK);
    dr::roundRect(m, TX, 1210, 8, 180, 4, a);
    text(m, monoFont(), "PRACTICE", { TX + 34, 1248 }, 17, a, 0, 0.5f);
    boxText(m, r.exercise, { TX + 34, 1270, TW - 68, 100 }, 28, INK, Align::Start);
}

void captionBar(const Ctx& cx, const Reel& r, int t) {
    const double s = t / (double)FPS;
    std::string c;
    if (s < 4.5) c = r.hook;
    else if (s < 19.5) c = "Read each example aloud.";
    else if (s < 26.0) c = "Your turn.";
    else c = "Practice this with a new example.";
    const float k = 0.75f + 0.25f * ramp(t, 0, 12);
    Ctx m = cx;
    m.a *= k;
    dr::rectGrad4(m, 0, 1480, W, 330, withAlpha(BG, 0), withAlpha(BG, 0),
                  withAlpha(BG, 0.92f), withAlpha(BG, 0.92f));
    boxText(m, c, { 110, 1580, 860, 140 }, 30, CREAM);
}

std::vector<std::string> situationLines(const Reel& r) {
    std::vector<std::string> out;
    for (const std::string& d : r.demo) {
        if (d.find(" B: ") != std::string::npos) {
            const size_t p = d.find(" B: ");
            out.push_back(d.substr(0, p));
            out.push_back(d.substr(p + 1));
        } else out.push_back(d);
    }
    if (out.empty()) out.push_back("Listen and repeat.");
    return out;
}

void drawReel(const Ctx& cx, int t, const Reel& r) {
    const Color a = trackColor(r.track);
    backdrop(cx, r);
    header(cx, r);
    if (r.track == "situations") {
        const std::vector<std::string> lines = situationLines(r);
        const int m = std::min(3, (int)lines.size());
        for (int i = 0; i < m; i++)
            speechBubble(cx, lines[(size_t)i], i, ramp(t, 150 + i * 120, 174 + i * 120), a);
    } else {
        for (int i = 0; i < std::min(3, (int)r.demo.size()); i++) {
            const float k = ramp(t, 150 + i * 120, 174 + i * 120);
            if (r.track == "vocabulary") vocabCard(cx, r.demo[(size_t)i], i, k, a);
            else grammarCard(cx, r.demo[(size_t)i], i, k, a);
        }
    }
    practice(cx, r, t);
    captionBar(cx, r, t);
    hud(cx, r, t);
}

std::vector<Reel> loadReels() {
    std::vector<Reel> out;
    std::string path;
    try { path = media::resolve("data/german-cefr/reels.json"); } catch (...) {}
    if (path.empty()) {
        const char* fallback = "assets/data/german-cefr/reels.json";
        std::ifstream test(fallback);
        if (test) path = fallback;
    }
    if (path.empty()) return out;
    std::ifstream in(path, std::ios::binary);
    if (!in) return out;
    std::stringstream ss; ss << in.rdbuf();
    Json root = Json::parse(ss.str());
    for (const Json& j : root["reels"].asArray()) {
        Reel r;
        r.id = j.str("id", "");
        r.level = j.str("level", "A1");
        r.track = j.str("track", "grammar");
        r.title = j.str("title", "");
        r.hook = j.str("hook", "");
        r.exercise = j.str("exercise", "");
        r.takeaway = clean(j.str("takeaway", ""));
        r.source = j.str("source", "");
        for (const Json& d : j["demo"].asArray()) r.demo.push_back(d.asString());
        if (!r.id.empty()) out.push_back(std::move(r));
    }
    return out;
}

void addSfx(Nodes& roots, int frame, const char* file, float vol) {
    TNode* n = addRoot(roots, frame, 24);
    media::AudioOptions o;
    o.volume = vol;
    media::audio(n, std::string("assets/sfx/") + file, o);
}

void buildComp(Comp& c, const Reel& r) {
    c.roots.clear();
    TNode* bed = addRoot(c.roots, 0, TOTAL);
    media::AudioOptions bo;
    bo.volume = 3.0f;
    media::audio(bed, "assets/sfx/bgm.wav", bo);
    addRoot(c.roots, 0, TOTAL)->fn = [r](const Ctx& cx, int t) { drawReel(cx, t, r); };
    addSfx(c.roots, 150, "pop.wav", 0.08f);
    addSfx(c.roots, 270, "pop.wav", 0.07f);
    addSfx(c.roots, 390, "pop.wav", 0.07f);
    addSfx(c.roots, 570, "thud.wav", 0.09f);
    addSfx(c.roots, 780, "chime.wav", 0.10f);
}

} // namespace

void registerA1GermanReels(Registry& out) {
    const std::vector<Reel> reels = loadReels();
    for (const Reel& r : reels) {
        Comp c;
        c.name = lower(r.id);
        c.w = W; c.h = H; c.fps = FPS;
        c.duration = TOTAL;
        c.background = BG;
        c.music = false;
        c.build = [r](Comp& c, const Json&) { buildComp(c, r); };
        out.push_back(std::move(c));
    }
}

} // namespace rmtn










