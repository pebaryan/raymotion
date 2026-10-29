// Malaria metabolomics explainer: FRAME-style flat cards, 13 scenes, Qwen3-TTS voice.
// Source brief: project/001-malaria-metabolomics-explainer.md
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace rmtn { namespace {
constexpr int W = 1920, H = 1080, FPS = 30, N = 13;
constexpr float LEAD = 0.35f;
const std::string VO_DIR = "vo/malaria-sentences-norm/";
const std::string SFX_DIR = "audio/austria/";
const std::string POP = "audio/flags/pop.wav";

const Color CREAM{ 242, 233, 214, 255 }, INK{ 21, 21, 21, 255 }, HGREEN{ 63, 174, 106, 255 };
const Color HPINK{ 239, 91, 123, 255 }, HYELLOW{ 243, 198, 78, 255 }, HORANGE{ 231, 111, 81, 255 };
const Color MUTED{ 122, 116, 104, 255 };

struct Line {
    std::string file, text;
    double seconds = 0;
    int at = 0, frames = 0;
};
struct Scene {
    std::string section, pill;
    std::vector<Line> lines;
    int len = 0, start = 0;
};

double wavSeconds(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return 0;
    unsigned char h[12]; double secs = 0; unsigned byteRate = 0;
    if (std::fread(h, 1, 12, f) == 12 && !std::memcmp(h, "RIFF", 4) && !std::memcmp(h + 8, "WAVE", 4)) {
        unsigned char c[8];
        while (std::fread(c, 1, 8, f) == 8) {
            const unsigned size = c[4] | c[5] << 8 | c[6] << 16 | (unsigned)c[7] << 24;
            if (!std::memcmp(c, "fmt ", 4)) {
                unsigned char fmt[16];
                if (size < 16 || std::fread(fmt, 1, 16, f) != 16) break;
                byteRate = fmt[8] | fmt[9] << 8 | fmt[10] << 16 | (unsigned)fmt[11] << 24;
                std::fseek(f, (long)(size - 16 + (size & 1)), SEEK_CUR);
            } else if (!std::memcmp(c, "data", 4)) {
                if (byteRate) secs = (double)size / byteRate;
                break;
            } else std::fseek(f, (long)(size + (size & 1)), SEEK_CUR);
        }
    }
    std::fclose(f); return secs;
}

std::vector<Scene> makeScenes() {
    const char* sections[N] = { "MALARIA / METABOLOMICS", "THE STAKES", "THE IDEA", "THE TRAP",
        "THE DATA", "CLEANING", "NARROWING", "THE HONEST TEST", "THE RESULT", "THE CANDIDATES",
        "THE DEMO", "WHAT IT IS / IS NOT", "NEXT" };
    const char* pills[N] = { "PROOF OF CONCEPT", "WHO 2024", "LC-MS SERUM", "OVERFITTING",
        "PR000960", "PCA / COMBAT", "FEATURE SELECTION", "LODO-CV", "AUC-ROC",
        "220 -> 11", "OPEN SOURCE", "HONEST LIMITS", "3 STEPS" };
    const float tails[N] = { 1.5f, 2.0f, 2.0f, 2.0f, 2.0f, 2.5f, 2.5f, 2.5f, 3.0f, 2.5f, 2.0f, 2.5f, 5.0f };
    struct Spec { int scene, line; std::string text, file; };
    std::vector<Spec> specs;
    std::string manifest;
    try { manifest = media::resolve(VO_DIR + "lines.json"); } catch (...) {}
    if (!manifest.empty()) {
        std::ifstream in(manifest, std::ios::binary);
        if (in) {
            std::stringstream ss; ss << in.rdbuf();
            Json spec = Json::parse(ss.str());
            for (const Json& l : spec["lines"].asArray()) {
                Spec x;
                x.scene = (int)l.num("scene", 0);
                x.line = (int)l.num("line", 0);
                x.text = l.str("text", "");
                x.file = l.str("file", "");
                if (x.scene >= 1 && x.scene <= N && !x.file.empty()) specs.push_back(std::move(x));
            }
        }
    }
    const int leadFrames = (int)std::lround(LEAD * FPS);
    const int gapFrames = 8;                 // 0.267 s between sentences
    std::vector<Scene> out;
    int cursor = 0;
    for (int si = 0; si < N; ++si) {
        Scene s;
        s.section = sections[si]; s.pill = pills[si];
        int at = leadFrames;
        for (const Spec& x : specs) {
            if (x.scene != si + 1) continue;
            Line l;
            l.file = x.file; l.text = x.text;
            std::string vp;
            try { vp = media::resolve(VO_DIR + l.file); } catch (...) {}
            l.seconds = vp.empty() ? 0 : wavSeconds(vp);
            if (l.seconds <= 0) l.seconds = std::max(1.0, l.text.size() / 14.0);
            l.frames = (int)std::ceil(l.seconds * FPS);
            l.at = at;
            at += l.frames + gapFrames;
            s.lines.push_back(std::move(l));
        }
        s.len = at - gapFrames + (int)std::lround(tails[si] * FPS);
        s.start = cursor;
        cursor += s.len;
        out.push_back(std::move(s));
    }
    return out;
}

// ------------------------------------------------------------------ paper tools
float smooth01(float x) { x = std::clamp(x, 0.f, 1.f); return x * x * (3 - 2 * x); }
float ramp(float t, float a, float b) { return smooth01((t - a) / std::max(.001f, b - a)); }
Font sans() { return fonts::ui(true); }
Font mono() { return fonts::load({ "JetBrainsMono-Bold.ttf" }, 64); }

float mw(Font f, const char* s, float size) { return MeasureTextEx(f, s, size, 0).x; }
void text(Font f, const char* s, float x, float y, float size, Color c, float ax = 0) {
    DrawTextEx(f, s, { x, y }, size, 0, c);
    (void)ax;
}
void textC(Font f, const char* s, float x, float y, float size, Color c) {
    DrawTextEx(f, s, { x - mw(f, s, size) * .5f, y }, size, 0, c);
}
void textR(Font f, const char* s, float x, float y, float size, Color c) {
    DrawTextEx(f, s, { x - mw(f, s, size), y }, size, 0, c);
}
std::vector<std::string> wrap(Font f, const std::string& s, float size, float width) {
    std::vector<std::string> lines; std::string line, word;
    auto flush = [&] {
        if (word.empty()) return;
        const std::string next = line.empty() ? word : line + " " + word;
        if (!line.empty() && mw(f, next.c_str(), size) > width) { lines.push_back(line); line = word; }
        else line = next;
        word.clear();
    };
    for (char ch : s) {
        if (ch == ' ') flush(); else word += ch;
    }
    flush(); if (!line.empty()) lines.push_back(line);
    if (lines.empty()) lines.emplace_back();
    return lines;
}
void textBlock(Font f, const std::string& s, float x, float y, float size, float width, Color c,
               float lineH = 1.25f, int maxLines = 99) {
    auto lines = wrap(f, s, size, width);
    for (size_t i = 0; i < lines.size() && (int)i < maxLines; ++i)
        text(f, lines[i].c_str(), x, y + i * size * lineH, size, c);
}
void shadowBox(float x, float y, float w, float h, Color fill, bool shadow = false) {
    if (shadow) DrawRectangle((int)(x + 12), (int)(y + 12), (int)w, (int)h, HORANGE);
    DrawRectangle((int)x, (int)y, (int)w, (int)h, fill);
    DrawRectangleLinesEx({ x, y, w, h }, 6, INK);
}
void box(float x, float y, float w, float h, Color fill) { shadowBox(x, y, w, h, fill, false); }
void arrow(float x0, float y0, float x1, float y1, Color c, float th = 4) {
    DrawLineEx({ x0, y0 }, { x1, y1 }, th, c);
    const float a = std::atan2(y1 - y0, x1 - x0), s = 14;
    DrawTriangle({ x1, y1 }, { x1 - s * std::cos(a - .5f), y1 - s * std::sin(a - .5f) },
                 { x1 - s * std::cos(a + .5f), y1 - s * std::sin(a + .5f) }, c);
}
void check(float x, float y, Color c = HGREEN) {
    DrawLineEx({ x, y + 14 }, { x + 10, y + 25 }, 6, c);
    DrawLineEx({ x + 10, y + 25 }, { x + 34, y }, 6, c);
}
void cross(float x, float y, Color c = HPINK) {
    DrawLineEx({ x, y }, { x + 25, y + 25 }, 6, c);
    DrawLineEx({ x + 25, y }, { x, y + 25 }, 6, c);
}
void stamp(const char* s, float x, float y, float size, Color fill, Color ink, float rot = -4) {
    const float w = mw(mono(), s, size) + 44, h = size * 1.45f;
    DrawRectanglePro({ x + 8, y + 8, w, h }, { 0, 0 }, rot, HORANGE);
    DrawRectanglePro({ x, y, w, h }, { 0, 0 }, rot, fill);
    const Vector2 origin{ 0, 0 };
    DrawTextPro(mono(), s, { x + 22, y + size * .18f }, origin, rot, size, 0, ink);
}
void pill(const char* s, Color fill = INK, Color ink = CREAM) {
    const Font f = mono(); const float w = mw(f, s, 19) + 34;
    DrawRectangleRounded({ 1856 - w, 24, w, 38 }, .5f, 8, fill);
    text(f, s, 1856 - w + 17, 34, 19, ink);
}
void topbar(const Scene& s) {
    DrawRectangle(64, 34, 1792, 2, INK);
    text(mono(), s.section.c_str(), 64, 42, 19, INK);
    pill(s.pill.c_str());
}
void footer(int ch, float local, Color c = INK) {
    DrawRectangle(64, 1012, 1792, 2, c);
    text(mono(), "PRASETYO & RUSDI  /  SUPPLIED PROJECT BRIEF", 64, 1028, 17, withAlpha(c, .8f));
    char buf[32]; std::snprintf(buf, sizeof buf, "%02d / %02d", ch + 1, N);
    textR(mono(), buf, 1856, 1028, 17, withAlpha(c, .8f));
}
void caption(const Scene& s, float local, bool greenGround) {
    if (s.lines.empty()) return;
    int li = -1;
    for (int i = 0; i < (int)s.lines.size(); ++i)
        if (local * FPS >= s.lines[i].at) li = i;
    if (li < 0) return;
    const Line& l = s.lines[li];
    const float a = l.at / (float)FPS;
    const float alpha = std::min(1.f, std::max(0.f, (local - a) / .15f));
    const Font f = mono();
    const Color col = greenGround ? CREAM : INK;
    auto lines = wrap(f, l.text, 23, 1700);
    const float y0 = 900;
    DrawRectangle(64, 886, 1792, 2, withAlpha(col, .45f * alpha));
    for (size_t i = 0; i < lines.size() && i < 3; ++i)
        text(f, lines[i].c_str(), 64, y0 + i * 29, 23, withAlpha(col, .9f * alpha));
}

// ------------------------------------------------------------------ scenes
void scene01(const Scene&, float t) {
    text(sans(), "DOES IT", 64, 238, 184, INK);
    text(sans(), "TRAVEL?", 64, 418, 184, HPINK);
    text(mono(), "CROSS-DATASET MACHINE LEARNING FOR MALARIA METABOLOMICS", 68, 674, 25, MUTED);
    textBlock(mono(), "A 3-minute proof-of-concept, told without ML jargon.", 68, 718, 26, 1100, INK);
    for (int i = 0; i < 12; ++i) {
        const float x = 1360 + (i % 4) * 120 + (i / 4) * 18;
        const float y = 160 + (i / 4) * 170 + (i % 4) * 12;
        const float r = 18 + (i % 3) * 10;
        DrawCircleLines((int)x, (int)y, r, withAlpha(INK, .55f));
        DrawCircle((int)x, (int)y, 5, i % 3 == 0 ? HPINK : i % 3 == 1 ? HGREEN : HYELLOW);
    }
    const float a = ramp(t, 1.5f, 2.3f);
    if (a > 0) text(mono(), "ONE QUESTION. TWO DATASETS. THIRTEEN SCENES.", 68, 810, 22, withAlpha(MUTED, a));
}
void scene02(const Scene&, float t) {
    text(sans(), "A STALLED FIGHT.", 64, 118, 64, INK);
    struct Stat { const char* value; const char* label; const char* sub; Color fill; };
    const Stat st[3] = {
        { "282M", "CASES", "2024 estimate", HGREEN },
        { "610K", "DEATHS", "2024 estimate", HPINK },
        { "US$5.4B", "FUNDING GAP", "whole response", HORANGE },
    };
    for (int i = 0; i < 3; ++i) {
        const float x = 64.f + i * 604.f, y = 224;
        const float u = ramp(t, .2f + i * .35f, .9f + i * .35f);
        shadowBox(x, y + (1 - u) * 18, 560, 316, st[i].fill, i == 1);
        text(mono(), st[i].label, x + 24, y + 26, 20, INK);
        text(sans(), st[i].value, x + 24, y + 110, 92, INK);
        text(mono(), st[i].sub, x + 24, y + 244, 20, withAlpha(INK, .8f));
    }
    textBlock(mono(), "pfhrp2 gene deletions can compromise rapid tests; reported in 46 endemic countries. Funding gap = US$9.3B target - US$3.9B funded; not research funding.",
              64, 620, 22, 1700, INK, 1.25f, 2);
    text(mono(), "SOURCE: WHO WORLD MALARIA REPORT 2024, VIA SUPPLIED BRIEF", 64, 720, 17, MUTED);
}
void scene03(const Scene&, float t) {
    text(sans(), "THE CHEMICAL FINGERPRINT.", 64, 118, 58, INK);
    shadowBox(64, 220, 480, 560, CREAM, true);
    text(mono(), "SERUM", 96, 248, 22, INK);
    DrawCircle(300, 430, 104, HGREEN);
    DrawTriangle({ 226, 492 }, { 374, 492 }, { 300, 610 }, HGREEN);
    DrawCircle(300, 430, 44, CREAM);
    text(mono(), "ONE DROP", 96, 700, 20, MUTED);
    box(584, 220, 1272, 560, CREAM);
    text(mono(), "SMALL MOLECULES IN BLOOD", 616, 248, 21, INK);
    const char* names[9] = { "ALA", "GLU", "VIT A", "S1P", "LIPID", "TAU", "PHE", "RET", "GLN" };
    for (int i = 0; i < 9; ++i) {
        const float x = 636 + i * 130;
        const float healthy = 80 + ((i * 47) % 120);
        const float sick = 70 + ((i * 83 + 40) % 190);
        const float u = ramp(t, 5.0f + i * .12f, 6.4f + i * .12f);
        const float h = healthy + (sick - healthy) * u;
        DrawRectangle((int)x, (int)(650 - h), 74, (int)h, i % 3 == 1 ? HPINK : i % 3 == 2 ? HGREEN : INK);
        DrawRectangleLinesEx({ x, 650 - h, 74, h }, 4, INK);
        text(mono(), names[i], x, 674, 18, INK);
    }
    text(mono(), t < 6.0f ? "HEALTHY MIX" : "INFECTED MIX", 1530, 760, 22, t < 6.0f ? HGREEN : HPINK);
    text(mono(), "AMINO ACIDS / VITAMINS / LIPIDS", 616, 720, 20, MUTED);
}
void scene04(const Scene&, float t) {
    DrawRectangle(64, 100, 1792, 780, HORANGE);
    DrawRectangleLinesEx({ 64, 100, 1792, 780 }, 6, INK);
    text(sans(), "MEMORISING ISN'T LEARNING.", 96, 132, 72, CREAM);
    text(mono(), "OVERFITTING", 100, 222, 22, INK);
    box(100, 286, 900, 390, CREAM);
    text(mono(), "220 METABOLITES", 124, 306, 19, INK);
    text(mono(), "~300 SAMPLES", 124, 334, 19, INK);
    for (int c = 0; c < 220; ++c) {
        const float x = 124 + c * 3.9f;
        DrawLineEx({ x, 372 }, { x, 628 }, 1, withAlpha(INK, .32f));
    }
    for (int r = 0; r < 40; ++r) {
        const float y = 372 + r * 6.4f;
        DrawLineEx({ 124, y }, { 990, y }, 1, withAlpha(INK, .22f));
    }
    shadowBox(1150, 270, 610, 470, CREAM, true);
    text(mono(), "ANSWER KEY", 1180, 296, 22, INK);
    for (int i = 0; i < 9; ++i) DrawLineEx({ 1180, 344.f + i * 28 }, { 1660, 344.f + i * 28 }, 4, withAlpha(INK, .35f));
    const float u = ramp(t, 4.0f, 5.0f);
    if (u > 0) {
        stamp("MEMORISED != LEARNED", 1100, 590 + (1 - u) * 16, 28, HPINK, CREAM, -4);
    }
    textBlock(mono(), "With many columns and few samples, a model can fit the noise and still fail on new data.",
              100, 710, 22, 930, CREAM, 1.25f, 2);
}
void scene05(const Scene&, float t) {
    text(sans(), "TWO DATASETS. ONE QUESTION.", 64, 118, 54, INK);
    shadowBox(64, 190, 1250, 500, CREAM, true);
    text(mono(), "PUBLIC SERUM COHORTS / BURKINA FASO", 88, 216, 20, MUTED);
    text(mono(), "ST001516", 92, 270, 26, INK); text(mono(), "ST001517", 820, 270, 26, INK);
    const char* rows[4][3] = {
        { "OBSERVATIONS", "199", "106" }, { "METABOLITES", "263", "253" },
        { "INFECTED / NOT", "100 / 99", "53 / 53" }, { "CHILDREN", "Gouin, 2-12", "Fulani, 5-12" } };
    for (int r = 0; r < 4; ++r) {
        const float y = 340 + r * 78;
        DrawLineEx({ 88, y }, { 1280, y }, 2, withAlpha(INK, .3f));
        text(mono(), rows[r][0], 92, y + 20, 20, MUTED);
        text(mono(), rows[r][1], 820, y + 20, 23, INK);
        text(mono(), rows[r][2], 1080, y + 20, 23, INK);
    }
    box(1348, 190, 508, 500, CREAM);
    text(mono(), "POOLED", 1376, 218, 20, MUTED);
    text(sans(), "305", 1376, 270, 94, HGREEN);
    text(mono(), "SAMPLES", 1376, 366, 22, INK);
    text(sans(), "220", 1376, 432, 94, HPINK);
    text(mono(), "SHARED METABOLITES", 1376, 528, 22, INK);
    const float u = ramp(t, 2.5f, 3.5f);
    if (u > 0) stamp("NOT 305 DIFFERENT CHILDREN", 1348, 606 + (1 - u) * 14, 24, HYELLOW, INK, -4);
    text(mono(), "REPEAT VISITS / DATASET = POPULATION + SITE", 64, 744, 22, INK);
}
void scene06(const Scene&, float t) {
    text(sans(), "CLEANING, THEN TWO ISLANDS.", 64, 118, 54, INK);
    const char* steps[4] = { "LOG SCALE", "NORMALISE / PQN", "FILL GAPS / KNN ~8%", "STANDARDISE / Z-SCORE" };
    for (int i = 0; i < 4; ++i) {
        const float y = 180 + i * 144;
        box(64, y, 760, 122, CREAM);
        text(mono(), ("0" + std::to_string(i + 1)).c_str(), 88, y + 20, 20, MUTED);
        text(mono(), steps[i], 150, y + 18, 26, INK);
        const float u = ramp(t, .4f + i * .5f, .9f + i * .5f);
        DrawRectangle((int)(144 + 0), (int)(y + 82), (int)(600 * u), 8, i == 3 ? HGREEN : HYELLOW);
    }
    shadowBox(880, 180, 976, 500, CREAM, true);
    text(mono(), "PCA / SCHEMATIC", 908, 204, 20, MUTED);
    const float u = ramp(t, 7.0f, 10.0f);
    const float sep = 220 - 180 * u;
    const float cL = 1370 - sep, cR = 1370 + sep;
    for (int k = 0; k < 2; ++k) {
        const float cx = k ? cR : cL;
        for (int i = 0; i < 38; ++i) {
            const float a = i * 2.399f, r = 40 + (i % 9) * 10;
            const float x = cx + std::cos(a) * r;
            const float y = 410 + std::sin(a) * r * .7f + ((i * 37) % 30);
            DrawCircle((int)x, (int)y, 7, k ? HGREEN : HPINK);
        }
        text(mono(), k ? "ST001517" : "ST001516", cx - 50, 530, 19, INK);
    }
    text(mono(), u < .8f ? "PC1 GAP 19.33" : "AFTER COMBAT 0.04", 930, 590, 22, INK);
    text(mono(), "COMBAT: DIAGNOSTIC ONLY - NOT USED IN THE PREDICTIVE PIPELINE", 908, 646, 18, MUTED);
    box(880, 704, 976, 154, CREAM);
    text(mono(), "HISTOGRAM / BEFORE -> AFTER", 908, 726, 18, MUTED);
    for (int i = 0; i < 12; ++i) {
        const float h1 = 18 + (i < 6 ? 80 - i * 8 : 24 + i * 2);
        const float h2 = 18 + 44 * std::exp(-std::pow((i - 5.5f) / 3.2f, 2));
        DrawRectangle(916 + i * 30, 838 - h1, 18, h1, withAlpha(INK, .28f));
        DrawRectangle(1270 + i * 30, 838 - h2, 18, h2, HYELLOW);
    }
}
void scene07(const Scene&, float t) {
    text(sans(), "BOTH METHODS MUST PICK.", 64, 118, 58, INK);
    shadowBox(64, 200, 1040, 590, CREAM, true);
    const float cx1 = 430, cx2 = 700, cy = 480, r = 220;
    DrawCircle((int)cx1, (int)cy, (int)r, withAlpha(HYELLOW, .25f));
    DrawCircle((int)cx2, (int)cy, (int)r, withAlpha(HGREEN, .25f));
    DrawCircleLines((int)cx1, (int)cy, r, INK); DrawCircleLines((int)cx2, (int)cy, r, INK);
    text(mono(), "LASSO", 300, 250, 24, INK); text(mono(), "RANDOM FOREST", 650, 250, 24, INK);
    text(sans(), "55", 320, 468, 72, INK); text(sans(), "20", 545, 468, 72, INK); text(sans(), "33", 770, 468, 72, INK);
    text(mono(), "5-FOLD", 320, 560, 18, MUTED); text(mono(), "SHARED", 525, 560, 18, MUTED);
    const float u = ramp(t, 3.5f, 4.5f);
    if (u > 0) stamp("YES / YES = KEEP", 760, 660 + (1 - u) * 14, 25, HGREEN, INK, -4);
    text(mono(), "ILLUSTRATIVE: ONE TRAINING DATASET / ACTUAL FOLDS 20 + 34 / FINAL 11", 88, 820, 17, MUTED);
    box(1136, 200, 720, 590, CREAM);
    text(mono(), "FEATURE SELECTION", 1164, 228, 20, MUTED);
    textBlock(sans(), "ONE VOTE IS NOT ENOUGH.", 1164, 282, 52, 650, INK, 1.08f, 2);
    textBlock(mono(), "Two independent reviewers. A metabolite survives only if both say yes.", 1164, 410, 24, 630, INK, 1.25f, 3);
    check(1164, 540); text(mono(), "LINEAR", 1210, 540, 22, INK);
    check(1164, 596); text(mono(), "TREE-BASED", 1210, 596, 22, INK);
    text(mono(), "FINAL POOLED SET: 11", 1164, 700, 22, HGREEN);
}
void scene08(const Scene&, float t) {
    text(sans(), "TRAIN ON ONE. TEST ON THE OTHER.", 64, 118, 54, INK);
    text(mono(), "LEAVE-ONE-DATASET-OUT", 64, 180, 20, MUTED);
    for (int lane = 0; lane < 2; ++lane) {
        const float y = 250 + lane * 270;
        const char* a = lane == 0 ? "ST001516" : "ST001517";
        const char* b = lane == 0 ? "ST001517" : "ST001516";
        shadowBox(64, y, 650, 190, CREAM, lane == 0);
        text(mono(), "TRAIN", 92, y + 22, 20, HGREEN); text(mono(), a, 92, y + 64, 38, INK);
        for (int i = 0; i < 5; ++i) {
            DrawRectangleLinesEx({ 92.f + i * 40, y + 128, 32, 32 }, 3, INK);
        }
        text(mono(), "TUNE / 5-FOLD HERE", 330, y + 134, 18, MUTED);
        arrow(730, y + 95, 876, y + 95, INK);
        box(900, y, 620, 190, CREAM);
        text(mono(), "TEST / LOCKED", 928, y + 22, 20, HPINK); text(mono(), b, 928, y + 64, 38, INK);
        DrawRectangleLinesEx({ 1090, y + 122, 100, 46 }, 4, INK);
        DrawCircleLines(1110, y + 120, 18, INK); DrawLineEx({ 1110, y + 120 }, { 1110, y + 138 }, 5, INK);
        text(mono(), "NO PEEKING", 1210, y + 130, 18, HPINK);
    }
    const float u = ramp(t, 5.0f, 6.0f);
    if (u > 0) stamp("CLEANING + TUNING FIT ONLY ON TRAIN", 64, 790 + (1 - u) * 12, 22, HYELLOW, INK, -3);
    text(mono(), "2 FOLDS / DESCRIPTIVE / INNER-CV IS OPTIMISTIC", 1020, 800, 18, MUTED);
}
void scene09(const Scene&, float t) {
    text(sans(), "PRACTICE TEST vs REAL EXAM.", 64, 118, 56, INK);
    text(mono(), "AUC-ROC / 0.5 = COIN FLIP / 1.0 = PERFECT", 68, 190, 20, MUTED);
    shadowBox(64, 224, 1792, 650, CREAM, true);
    const float x0 = 250, x1 = 1680, y0 = 620;
    auto X = [&](double auc) { return x0 + (float)((auc - .5) / .5) * (x1 - x0); };
    DrawLineEx({ x0, y0 }, { x1, y0 }, 4, INK);
    const double ticks[6] = { .5, .6, .7, .8, .9, 1.0 };
    for (double v : ticks) {
        DrawLineEx({ X(v), y0 }, { X(v), y0 + 16 }, 3, INK);
        char b[16]; std::snprintf(b, sizeof b, "%.1f", v);
        textC(mono(), b, X(v), y0 + 26, 18, MUTED);
    }
    DrawLineEx({ X(.5), 320 }, { X(.5), 680 }, 3, withAlpha(INK, .4f));
    text(mono(), "COIN FLIP", X(.5) + 10, 660, 17, MUTED);
    text(mono(), "PRACTICE TEST / INNER-CV", 96, 278, 20, HGREEN);
    text(mono(), "REAL EXAM / HELD-OUT", 96, 474, 20, HPINK);
    const double inner[4] = { .931, .956, .981, .996 };
    const double held[10] = { .569, .647, .499, .633, .646, .638, .587, .574, .699, .644 };
    for (int i = 0; i < 4; ++i) {
        const float u = ramp(t, 1.5f + i * .35f, 2.0f + i * .35f);
        if (u > 0) DrawCircle((int)X(inner[i]), (int)(330 + (i % 3) * 18), 13, HGREEN);
    }
    for (int i = 0; i < 10; ++i) {
        const float u = ramp(t, 5.0f + i * .18f, 5.5f + i * .18f);
        if (u > 0) DrawCircle((int)X(held[i]), (int)(488 + (i % 4) * 22), 12, HPINK);
    }
    const float gu = ramp(t, 9.0f, 10.0f);
    if (gu > 0) {
        DrawRectanglePro({ 1040, 250 + (1 - gu) * 18, 330, 54 }, { 0, 0 }, -4, HYELLOW);
        DrawTextPro(mono(), "GAP 0.293-0.357", { 1062, 264 + (1 - gu) * 18 }, { 0, 0 }, -4, 23, 0, INK);
    }
    const float mu = ramp(t, 15.0f, 16.0f);
    if (mu > 0) {
        DrawLineEx({ X(.64), 380 }, { X(.64), 600 }, 4, withAlpha(INK, .5f));
        text(mono(), "~0.64", X(.64) - 40, 350, 28, INK);
        text(mono(), "BEST AVERAGES: LR 0.642 / TUNED XGB 0.643", 1060, 710, 18, MUTED);
    }
    text(mono(), "2 FOLDS / NO RANKING CLAIM", 96, 768, 19, MUTED);
    text(mono(), "THE GAP IS THE FINDING", 1310, 768, 22, HPINK);
}
void scene10(const Scene&, float t) {
    text(sans(), "ELEVEN FOR REVIEW.", 64, 118, 58, INK);
    shadowBox(64, 220, 470, 560, CREAM, true);
    text(mono(), "FEATURE REDUCTION", 92, 246, 18, MUTED);
    text(sans(), "220", 92, 300, 100, INK);
    text(mono(), "CANDIDATES", 92, 408, 18, MUTED);
    arrow(210, 460, 210, 610, INK);
    text(sans(), "11", 92, 620, 100, HGREEN);
    text(mono(), "20:1 / NOT A PERFORMANCE GAIN", 92, 732, 18, MUTED);
    const char* names[3] = { "PHENYLALANINE", "SPHINGOSINE-1-PHOSPHATE", "RETINOL (VITAMIN A)" };
    const char* coef[3] = { "+1.214", "-1.121", "-0.944" };
    const char* dir[3] = { "UP WITH INFECTION", "DOWN", "DOWN" };
    for (int i = 0; i < 3; ++i) {
        const float y = 220 + i * 162;
        box(600, y, 1256, 138, CREAM);
        text(mono(), names[i], 624, y + 18, 22, INK);
        text(sans(), coef[i], 624, y + 56, 54, INK);
        text(mono(), dir[i], 940, y + 70, 20, MUTED);
        if (i == 0) arrow(1420, y + 112, 1420, y + 28, INK, 5);
        else arrow(1420, y + 28, 1420, y + 112, INK, 5);
    }
    const char* rest[8] = { "3-UREIDOPROPIONATE", "TAURINE", "OTHER 1", "OTHER 2", "OTHER 3", "OTHER 4", "OTHER 5", "OTHER 6" };
    for (int i = 0; i < 8; ++i) {
        const float x = 600 + (i % 4) * 312, y = 720 + (i / 4) * 62;
        box(x, y, 290, 50, withAlpha(INK, .06f));
        text(mono(), rest[i], x + 10, y + 14, 15, withAlpha(INK, .65f));
    }
    text(mono(), "LOPANSRI 2006 / DHANGADAMAJHI 2020 / RAZA 2009", 64, 790, 16, MUTED);
    text(mono(), "LITERATURE-COMPATIBLE, NOT NEW", 64, 820, 20, HPINK);
    text(mono(), "TOP-5 SHAP LISTS DIFFERED BETWEEN FOLDS", 64, 850, 17, MUTED);
}
void scene11(const Scene&, float t) {
    text(sans(), "11 VALUES IN. WHY OUT.", 64, 118, 58, INK);
    shadowBox(64, 200, 1792, 610, CREAM, true);
    text(mono(), "RESEARCH DEMO", 96, 226, 20, MUTED);
    for (int i = 0; i < 11; ++i) {
        const float x = 96 + (i % 4) * 130, y = 280 + (i / 4) * 90;
        box(x, y, 112, 66, withAlpha(INK, .05f));
        text(mono(), "VAL", x + 10, y + 10, 14, MUTED);
        text(mono(), ("M" + std::to_string(i + 1)).c_str(), x + 10, y + 30, 20, INK);
    }
    arrow(660, 500, 800, 500, INK);
    box(820, 300, 300, 360, withAlpha(HGREEN, .18f));
    textC(mono(), "MODEL", 970, 460, 28, INK);
    textC(mono(), "11 -> PREDICTION", 970, 500, 18, MUTED);
    arrow(1140, 500, 1280, 500, INK);
    box(1300, 300, 500, 360, CREAM);
    text(mono(), "PREDICTION + WHY", 1324, 326, 20, INK);
    for (int i = 0; i < 5; ++i) {
        const float w = 80 + (i * 53) % 240;
        DrawRectangle(1324, 380 + i * 48, (int)w, 24, i % 2 ? HGREEN : HPINK);
        text(mono(), ("M" + std::to_string(i + 1)).c_str(), 1330 + w + 12, 382 + i * 48, 16, INK);
    }
    const float u = ramp(t, 2.0f, 3.0f);
    if (u > 0) stamp("RESEARCH DEMO / NOT A DIAGNOSTIC", 96, 740 + (1 - u) * 12, 22, HYELLOW, INK, -3);
}
void scene12(const Scene&, float t) {
    text(sans(), "A METHOD, NOT A DIAGNOSTIC.", 64, 118, 56, INK);
    shadowBox(64, 200, 860, 600, CREAM, true);
    box(996, 200, 860, 600, CREAM);
    DrawRectangle(64, 200, 860, 60, HGREEN); DrawRectangle(996, 200, 860, 60, HPINK);
    text(mono(), "WHAT IT IS", 92, 218, 22, INK);
    text(mono(), "WHAT IT IS NOT", 1024, 218, 22, INK);
    const char* yes[4] = { "A transparent, reusable cross-dataset workflow",
        "An honest report of a modest result", "11 candidates for expert review", "Open code" };
    const char* no[4] = { "A validated biomarker panel", "A diagnostic", "New knowledge about Indonesian malaria", "A commercial clinical product" };
    for (int i = 0; i < 4; ++i) {
        const float y = 316 + i * 112;
        check(96, y, HGREEN);
        textBlock(mono(), yes[i], 140, y - 2, 22, 730, INK, 1.2f, 2);
        cross(1028, y, HPINK);
        textBlock(mono(), no[i], 1072, y - 2, 22, 730, INK, 1.2f, 2);
    }
    text(mono(), "THE COHORTS ARE WEST AFRICAN CHILDREN", 64, 838, 18, MUTED);
}
void scene13(const Scene&, float t) {
    if (t < 5.0f) {
        const char* steps[3] = { "INDEPENDENT COHORTS", "TARGETED CONFIRMATION", "PROSPECTIVE LOCAL STUDIES" };
        for (int i = 0; i < 3; ++i) {
            const float x = 64 + i * 604;
            shadowBox(x, 270, 560, 220, i == 0 ? CREAM : i == 1 ? HPINK : HYELLOW, i == 0);
            text(mono(), ("0" + std::to_string(i + 1)).c_str(), x + 24, 294, 20, INK);
            textBlock(sans(), steps[i], x + 24, 350, 40, 510, INK, 1.02f, 2);
        }
        text(mono(), "THE NEXT THREE MOVES", 64, 220, 22, MUTED);
    } else {
        DrawRectangle(0, 0, W, H, HGREEN);
        text(sans(), "MODEST RESULT.", 96, 250, 104, CREAM);
        text(sans(), "HONEST METHOD.", 96, 376, 104, CREAM);
        const float u = ramp(t, 6.0f, 7.0f);
        if (u > 0) stamp("PROOF OF CONCEPT", 1150, 300 + (1 - u) * 20, 34, HPINK, CREAM, -6);
        text(mono(), "PRASETYO & RUSDI  /  PT GALENIC SYSTEMS INDONESIA  /  UNIVERSITAS ISLAM BANDUNG", 96, 760, 19, CREAM);
        text(mono(), "DATA: METABOLOMICS WORKBENCH PR000960  /  ABDRABOU ET AL., NAT METAB 2021", 96, 796, 17, withAlpha(CREAM, .8f));
        text(mono(), "GALENIC-SYSTEMS/MALARIA-METABOLOMICS-DEMO", 96, 832, 19, HYELLOW);
        text(mono(), "A CLIENT PREVIEW MADE WITH RAYMOTION", 96, 900, 17, withAlpha(CREAM, .65f));
    }
}

void drawFilm(const Ctx&, int frame, const std::shared_ptr<std::vector<Scene>>& scenes) {
    const float t = frame / (float)FPS;
    int ch = 0;
    while (ch + 1 < N && t >= (*scenes)[ch + 1].start / (float)FPS) ++ch;
    const Scene& s = (*scenes)[ch];
    const float local = t - s.start / (float)FPS;
    const bool green = ch == N - 1 && local >= 5.0f;
    ClearBackground(green ? HGREEN : CREAM);
    if (!green) {
        topbar(s);
        footer(ch, local);
    }
    switch (ch) {
    case 0: scene01(s, local); break;
    case 1: scene02(s, local); break;
    case 2: scene03(s, local); break;
    case 3: scene04(s, local); break;
    case 4: scene05(s, local); break;
    case 5: scene06(s, local); break;
    case 6: scene07(s, local); break;
    case 7: scene08(s, local); break;
    case 8: scene09(s, local); break;
    case 9: scene10(s, local); break;
    case 10: scene11(s, local); break;
    case 11: scene12(s, local); break;
    default: scene13(s, local); break;
    }
    if (!green) caption(s, local, green);
}

// ------------------------------------------------------------------ thumbnail
void drawThumb() {
    ClearBackground(CREAM);
    text(sans(), "DOES IT", 72, 170, 132, INK);
    text(sans(), "TRAVEL?", 72, 300, 132, HPINK);
    text(mono(), "CROSS-DATASET MALARIA METABOLOMICS", 78, 510, 24, MUTED);
    textBlock(mono(), "A model can score 0.99 on its practice test. On unseen data: 0.50-0.70. That gap is the finding.",
              78, 560, 28, 980, INK, 1.2f, 3);
    shadowBox(1120, 190, 700, 620, CREAM, true);
    const char* labels[2] = { "PRACTICE TEST", "REAL EXAM" };
    const Color cols[2] = { HGREEN, HPINK };
    for (int i = 0; i < 2; ++i) {
        text(mono(), labels[i], 1150, 230 + i * 270, 22, cols[i]);
        const float base = 380 + i * 270;
        for (int k = 0; k < 6; ++k) {
            const float x = 1170 + k * 88;
            DrawCircle((int)x, (int)(base + (k % 3) * 14), 9, cols[i]);
        }
    }
    text(mono(), "PROOF OF CONCEPT / INTERNAL PREVIEW", 72, 920, 20, MUTED);
    text(mono(), "PRASETYO & RUSDI / SUPPLIED BRIEF", 72, 966, 18, MUTED);
}

} // namespace

void registerMalariaMetabolomics(Registry& out) {
    auto scenes = std::make_shared<std::vector<Scene>>(makeScenes());
    const int total = scenes->empty() ? 1 : (*scenes)[N - 1].start + (*scenes)[N - 1].len;
    Comp c;
    c.name = "malaria-metabolomics";
    c.w = W; c.h = H; c.fps = FPS; c.duration = total; c.background = CREAM;
    c.assets = [scenes](const Comp&, const Json&) {
        sub::Track tr;
        for (const Scene& s : *scenes) {
            for (const Line& l : s.lines) {
                const double from = (s.start + l.at) / (double)FPS;
                const double to = (s.start + l.at + l.frames) / (double)FPS;
                tr.cues.push_back({ from, to, l.text, false });
            }
        }
        sub::writeSrt("out/malaria-metabolomics.srt", tr);
    };
    TNode* root = addRoot(c.roots, 0, total);
    root->fn = [scenes](const Ctx& cx, int t) { drawFilm(cx, t, scenes); };

    auto sfx = [&](const std::string& file, int frame, float volume) {
        std::string p;
        try { p = media::resolve(file); } catch (...) { return; }
        media::AudioOptions o; o.volume = volume * .35f;
        media::audio(addRoot(c.roots, frame, 45), p, o);
    };
    for (int i = 0; i < N; ++i) {
        const Scene& s = (*scenes)[i];
        for (const Line& l : s.lines) {
            std::string voice;
            try { voice = media::resolve(VO_DIR + l.file); } catch (...) {}
            if (!voice.empty()) {
                media::AudioOptions vo; vo.volume = .75f;
                media::audio(addRoot(c.roots, s.start + l.at, l.frames + 2), voice, vo);
            }
        }
        if (i == 0) { sfx(SFX_DIR + "stamp.wav", s.start + 8, .3f); sfx(SFX_DIR + "lowhit.wav", s.start + 78, .45f); }
        else if (i == 1) { sfx(SFX_DIR + "stamp.wav", s.start + 10, .25f); sfx(SFX_DIR + "stamp.wav", s.start + 90, .25f); sfx(SFX_DIR + "stamp.wav", s.start + 180, .25f); }
        else if (i == 2) { sfx(POP, s.start + 20, .4f); sfx(POP, s.start + 60, .4f); sfx(SFX_DIR + "whoosh.wav", s.start + 120, .35f); }
        else if (i == 3) { sfx(SFX_DIR + "whoosh.wav", s.start + 40, .3f); sfx(SFX_DIR + "lowhit.wav", s.start + 160, .35f); }
        else if (i == 4) { sfx(SFX_DIR + "whoosh.wav", s.start + 20, .25f); sfx(SFX_DIR + "whoosh.wav", s.start + 100, .25f); }
        else if (i == 5) { for (int k = 0; k < 4; ++k) sfx(POP, s.start + 30 + k * 45, .28f); sfx(SFX_DIR + "whoosh.wav", s.start + 220, .3f); }
        else if (i == 6) { sfx(POP, s.start + 30, .3f); sfx(POP, s.start + 160, .3f); sfx(SFX_DIR + "lowhit.wav", s.start + 230, .35f); }
        else if (i == 7) { sfx(SFX_DIR + "lowhit.wav", s.start + 40, .4f); sfx(SFX_DIR + "whoosh.wav", s.start + 180, .3f); }
        else if (i == 8) { sfx(SFX_DIR + "chime.wav", s.start + 40, .25f); sfx(POP, s.start + 180, .25f); }
        else if (i == 9) { sfx(SFX_DIR + "whoosh.wav", s.start + 40, .3f); sfx(POP, s.start + 180, .28f); sfx(POP, s.start + 250, .28f); }
        else if (i == 10) { sfx(POP, s.start + 30, .3f); sfx(POP, s.start + 100, .3f); }
        else if (i == 11) { sfx(SFX_DIR + "chime.wav", s.start + 30, .25f); sfx(SFX_DIR + "lowhit.wav", s.start + 200, .3f); }
        else { sfx(SFX_DIR + "stamp.wav", s.start + 10, .35f); sfx(SFX_DIR + "chime.wav", s.start + 170, .3f); }
    }
    out.push_back(std::move(c));

    Comp t;
    t.name = "malaria-metabolomics-thumb";
    t.w = W; t.h = H; t.fps = FPS; t.duration = 60; t.background = CREAM;
    TNode* tn = addRoot(t.roots, 0, 60);
    tn->fn = [](const Ctx&, int) { drawThumb(); };
    out.push_back(std::move(t));
}
} // namespace rmtn
