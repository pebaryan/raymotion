// German CEFR banner videos: 18 animated 1200x400 headers, voice-free with BGM/SFX.
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace rmtn {
namespace {

constexpr int W = 1200, H = 400, FPS = 30, TOTAL = 180;
const Color BG = mkColor(0x101820), BG2 = mkColor(0x080d12), INK = mkColor(0xf3ebdd),
            MUTED = mkColor(0x8fa6a0), MINT = mkColor(0x7fe0c3), AMBER = mkColor(0xffb45c),
            CORAL = mkColor(0xff8a7a), DARK = mkColor(0x121f24);

struct Banner {
    std::string level, track, slug;
    std::vector<std::string> titles;
};

float ramp(int t, float a, float b) {
    const float u = std::clamp((t - a) / std::max(0.001f, b - a), 0.f, 1.f);
    return u * u * (3 - 2 * u);
}
Color accent(const std::string& track) {
    if (track == "Wortschatz") return AMBER;
    if (track == "Situationen") return CORAL;
    return MINT;
}
Font titleFont() { return fonts::load({ "ArchivoBlack-Regular.ttf", "Inter-Bold.ttf" }, 96); }
Font bodyFont() { return fonts::load({ "SpaceGrotesk-Variable.ttf", "Inter-Regular.ttf" }, 96); }
Font monoFont() { return fonts::load({ "JetBrainsMono-Bold.ttf" }, 64); }

void text(const Ctx& cx, Font f, const std::string& s, Vector2 at, float size, Color c,
          float ax = 0, float ay = 0.5f) {
    dr::text(f, cx, s.c_str(), at, size, 0, c, ax, ay);
}
void boxText(const Ctx& cx, const std::string& s, Rectangle box, float size, Color c) {
    TextStyle st; st.font = bodyFont(); st.size = size; st.lineHeight = 1.06f;
    st.color = c; st.align = Align::Start; st.valign = Align::Center;
    dr::textBox(cx, s, box, st);
}

void backdrop(const Ctx& cx) {
    dr::rectGrad4(cx, 0, 0, W, H, mkColor(0x14272a), BG2, BG2, BG2);
    for (int j = 0; j < 11; j++)
        for (int i = 0; i < 36; i++)
            dr::circle(cx, { 20.f + i * 34.f + (j % 2) * 10.f, 24.f + j * 34.f }, 0.9f,
                       withAlpha(MINT, 0.06f + 0.02f * (j % 3)));
}

void row(const Ctx& cx, const std::string& s, int index, int col, float k, Color a) {
    if (k <= 0.001f) return;
    const float x = 424 + col * 360;
    const float y = 56 + (index % 4) * 74;
    Ctx m = cx;
    m.a *= std::min(1.f, k);
    m.o.y += (1 - k) * 12;
    dr::roundRect(m, x, y, 340, 60, 12, withAlpha(DARK, 0.94f));
    dr::roundRect(m, x, y, 5, 60, 2.5f, a);
    char n[8]; std::snprintf(n, sizeof n, "%02d", index + 1);
    text(m, monoFont(), n, { x + 14, y + 30 }, 12, a, 0, 0.5f);
    boxText(m, s, { x + 42, y + 5, 284, 50 }, 15, INK);
}

void drawBanner(const Ctx& cx, int t, const Banner& b) {
    const Color a = accent(b.track);
    backdrop(cx);
    const float kin = ramp(t, 0, 13);
    const float trackK = ramp(t, 8, 22);
    const float lineK = ramp(t, 10, 34);

    Ctx m = cx;
    m.a *= kin;
    m.o.y += (1 - kin) * 18;
    text(m, monoFont(), "GERMAN CEFR  /  SHORTS", { 48, 34 }, 11, a, 0, 0.5f);
    text(m, titleFont(), b.level, { 48, 94 }, 126, INK, 0, 0.5f);
    text(m, monoFont(), b.track, { 48, 228 }, 26, a, 0, 0.5f);
    text(m, monoFont(), "8 REELS  ·  30 SECONDS", { 48, 266 }, 12, MUTED, 0, 0.5f);
    text(m, monoFont(), "A1 — C2  ·  144 SHORTS", { 48, 336 }, 12, withAlpha(MUTED, 0.8f), 0, 0.5f);
    dr::rect(m, 48, 292, 300 * lineK, 2, a);

    Ctx q = cx;
    q.a *= trackK;
    text(q, monoFont(), "8 TITLES", { 1128, 34 }, 12, MUTED, 1, 0.5f);

    for (int i = 0; i < 8 && i < (int)b.titles.size(); i++) {
        const int col = i / 4;
        const float k = ramp(t, 24 + i * 8, 38 + i * 8);
        row(cx, b.titles[(size_t)i], i, col, k, a);
    }
}

std::vector<Banner> loadBanners() {
    std::vector<Banner> out;
    std::string path;
    try { path = media::resolve("data/german-cefr/banners.json"); } catch (...) {}
    if (path.empty()) {
        const char* fallback = "assets/data/german-cefr/banners.json";
        std::ifstream test(fallback);
        if (test) path = fallback;
    }
    if (path.empty()) return out;
    std::ifstream in(path, std::ios::binary);
    if (!in) return out;
    std::stringstream ss; ss << in.rdbuf();
    Json root = Json::parse(ss.str());
    for (const Json& j : root["groups"].asArray()) {
        Banner b;
        b.level = j.str("level", "A1");
        b.track = j.str("track", "Grammatik");
        b.slug = j.str("slug", "");
        for (const Json& x : j["titles"].asArray()) b.titles.push_back(x.asString());
        if (!b.slug.empty()) out.push_back(std::move(b));
    }
    return out;
}

void sfx(Nodes& roots, int frame, const char* file, float vol) {
    TNode* n = addRoot(roots, frame, 20);
    media::AudioOptions o; o.volume = vol;
    media::audio(n, std::string("assets/sfx/") + file, o);
}

void build(Comp& c, const Banner& b) {
    c.roots.clear();
    addRoot(c.roots, 0, TOTAL)->fn = [b](const Ctx& cx, int t) { drawBanner(cx, t, b); };
    TNode* bed = addRoot(c.roots, 0, TOTAL);
    media::AudioOptions bo; bo.volume = 3.0f;
    media::audio(bed, "assets/sfx/bgm.wav", bo);
    for (int i = 0; i < 8; i++) sfx(c.roots, 24 + i * 8, "pop.wav", 0.07f);
    sfx(c.roots, 150, "chime.wav", 0.10f);
}

} // namespace

void registerGermanCefrBanners(Registry& out) {
    const std::vector<Banner> groups = loadBanners();
    for (const Banner& b : groups) {
        Comp c;
        c.name = "banner-" + b.slug;
        c.w = W; c.h = H; c.fps = FPS;
        c.duration = TOTAL;
        c.background = BG;
        c.build = [b](Comp& c, const Json&) { build(c, b); };
        out.push_back(std::move(c));
    }
}

} // namespace rmtn

