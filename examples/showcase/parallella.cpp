// parallella.cpp -- "parallella": one ~80 s vertical film in two chapters.
// Brief: docs/parallella-brief.md. Source: parallella-agent STORY.md / VERIFICATION.md
// (2026-11-02). Chapter 1, DOES IT RUN?: a $99 board from 2012 wrote a sentence; its 16 cores
// hold 512 KB, the model is 60.8 MB (drawn to scale), trained ternary it is 16x smaller, and the
// board's output matches the reference word for word. Chapter 2, HOW FAST?: one tok/s ruler with
// a dashed PROJECTED 60; the matrix pass measures 59, the whole model 15.8, the batch 40 (80).
//
// Merges the two parts parallella-2012 and parallella-sixty into one film. The board model is
// the first, unreleased draft's. Timing follows the voice: each line's WAV header and
// words.json onsets; missing WAVs fall back to the target seconds, so the film renders silently.
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace rmtn {
namespace {

constexpr int W = 1080, H = 1920, FPS = 30;
constexpr const char* VO_DIR = "assets/vo/parallella/";
constexpr const char* ASSETS = "out/parallella-assets";
constexpr const char* SCORE = "out/parallella-assets/score.wav";
constexpr const char* SRT_PATH = "out/parallella.srt";
constexpr const char* GLYPHS = "·–×≈−";

const Color BG = mkColor(0x0d1117), PANEL = mkColor(0x161c26), GRID = mkColor(0x222b38),
            INK = mkColor(0xe8edf2), DIM = mkColor(0x6f7c8e), AMBER = mkColor(0xffb547),
            TEAL = mkColor(0x4fd1c5), TEAL2 = mkColor(0x2f9e95), SLATE = mkColor(0x8b98a9), CORAL = mkColor(0xff6b5b);

// ------------------------------------------------------------------ the data (brief §5)
constexpr double SRAM_MB = 16 * 32 * 1024 / 1e6;   // V: 32 KiB per core; 16 cores = 0.524 MB
constexpr double FP32_MB = 60.77;                  // V (QAT table): stories15M, fp32, bytes/token
constexpr double TERN_MB = 3.80;                   // V (QAT table): ternary, 2-bit packed
constexpr const char* PROMPT = "Once upon a time";
constexpr const char* SAMPLE = ", there was a little girl named Lily. She loved to play with her toys, but one day her mom";
constexpr int SAMPLE_TOKENS = 24;                  // S §5 / V: identical for all 24 tokens
constexpr double TYPE_S = SAMPLE_TOKENS / 15.80;   // typed at the decoder's measured rate (S §5)
// V (QAT table, greedy samples): the ternary model's own text
constexpr const char* TERN_SAMPLE = "\"...a little girl named Sarah. She was three years old and loved to play outside.\"";

// chapter 2: the speed (S = STORY.md, V = VERIFICATION.md)
constexpr double PROJECTED = 60.0;     // S §1: stories15M at 1.58 bit/weight, streamed (a projection)
constexpr double ARM = 13.84;          // S §2/§10: ARM A9, stories15M Q8, 2 threads, measured
constexpr double FIRST = 49.4;         // S §2: 49.3/49.5 (nano-6), 49.2/48.8 (nano-3), 8 KB chunks
constexpr double BAND0 = 57.0, BAND1 = 59.4;   // S §2: 1 KB x 120 chunks, 56.9-59.4; "57 to 59"
constexpr double DECODER = 15.80;      // S §5/§10: the whole model, single stream
constexpr double BATCH8 = 40.17;       // S §6: 8 streams, one board, 199 ms per step
constexpr double BOARDS2 = 80.19;      // S §8: two boards, 16 streams
constexpr int STREAMS = 8;

// ------------------------------------------------------------------ the ruler (chapter 2)
constexpr float Y0 = 1330, YTOP = 330, VMAX = 90;          // 0 at Y0, 90 tok/s at YTOP
constexpr float BAR_W = 150, ARM_X = 300, EPI_X = 520;     // column centres
constexpr float RULE_X0 = 190, RULE_X1 = 930;
float yOf(double v) { return Y0 - (float)(v / VMAX) * (Y0 - YTOP); }

// ------------------------------------------------------------------ the voice-over lines
struct Line { const char* file; const char* text; double fallback; };
const Line LINES[] = {
    { "01-hook.wav", "A ninety-nine dollar board from twenty twelve wrote this.", 3.3 },
    { "02-machine.wav", "It's a Parallella. One chip with sixteen cores, each holding thirty-two kilobytes. Half a megabyte in all.", 7.2 },
    { "03-problem.wav", "A small story model has fifteen million weights: sixty megabytes. It cannot fit, so the weights stream in for every word.", 8.2 },
    { "04-shrink.wav", "So the model was trained with three values per weight: minus one, zero and one. Sixteen times smaller, and it still writes.", 9.3 },
    { "05-proof.wav", "Every multiply runs on that chip. Checked word for word against a reference: identical.", 5.4 },
    { "06-bridge.wav", "So it runs. How fast? On paper, sixty tokens a second.", 4.4 },
    { "07-arm.wav", "The board's own ARM processor, for comparison, manages fourteen.", 4.3 },
    { "08-measure.wav", "Measured on the chip: forty-nine. The cause was the transfer size. One-kilobyte chunks: fifty-nine.", 8.0 },
    { "09-twist.wav", "But that was one matrix pass, not the model. The whole model runs at just under sixteen.", 6.4 },
    { "10-batch.wav", "Batching shares the weights. Eight stories at once: forty. Two boards: eighty. Each story still waits a fifth of a second per token.", 8.5 },
    { "11-close.wav", "Sixty was true, for a matrix pass. Measure the thing you ship.", 4.5 },
};
constexpr int NLINES = sizeof LINES / sizeof LINES[0];
constexpr int NSCENES = 11;
constexpr int CH2 = 5;                 // the first scene of chapter 2 (HOW FAST?)

std::vector<std::string> words(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    for (std::string w; in >> w;) out.push_back(w);
    return out;
}

double wavSeconds(const std::string& file) {
    std::string path;
    try { path = media::resolve(file); } catch (...) { return -1; }
    std::ifstream f(path, std::ios::binary);
    char riff[12];
    if (!f.read(riff, 12) || std::memcmp(riff, "RIFF", 4) || std::memcmp(riff + 8, "WAVE", 4)) return -1;
    unsigned byteRate = 0;
    char hdr[8];
    while (f.read(hdr, 8)) {
        unsigned size;
        std::memcpy(&size, hdr + 4, 4);
        if (!std::memcmp(hdr, "fmt ", 4)) {
            std::vector<char> fmt(size);
            f.read(fmt.data(), size);
            if (size >= 12) std::memcpy(&byteRate, fmt.data() + 8, 4);
        } else if (!std::memcmp(hdr, "data", 4)) {
            return byteRate ? size / (double)byteRate : -1;
        } else {
            f.seekg(size + (size & 1), std::ios::cur);
        }
    }
    return -1;
}

// ------------------------------------------------------------------ the plan
struct Plan {
    double start[NLINES] = {}, dur[NLINES] = {};
    bool voiced[NLINES] = {};
    std::vector<double> on[NLINES], off[NLINES];
    std::vector<std::string> word[NLINES];
    double scene[NSCENES + 1] = {};
    int frames = 0;

    double W0(int l, int w) const { return on[l][std::clamp(w, 0, (int)on[l].size() - 1)]; }
    double W1(int l, int w) const { return off[l][std::clamp(w, 0, (int)off[l].size() - 1)]; }
    double end(int l) const { return start[l] + dur[l]; }
    int sceneAt(double s) const {
        int i = 0;
        while (i + 1 < NSCENES && s >= scene[i + 1]) i++;
        return i;
    }
};

Json loadWordTimings() {
    try {
        std::ifstream f(media::resolve(std::string(VO_DIR) + "words.json"), std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        return Json::parse(ss.str());
    } catch (...) {
        return Json();
    }
}

std::shared_ptr<Plan> makePlan() {
    auto p = std::make_shared<Plan>();
    const Json timings = loadWordTimings();
    std::vector<double> local[NLINES][2];
    for (int l = 0; l < NLINES; l++) {
        p->word[l] = words(LINES[l].text);
        const int n = (int)p->word[l].size();
        const double measured = wavSeconds(std::string(VO_DIR) + LINES[l].file);
        p->voiced[l] = measured > 0;
        p->dur[l] = p->voiced[l] ? measured : LINES[l].fallback;
        const Json* t = timings.isObject() && timings.has(LINES[l].file) ? &timings[LINES[l].file] : nullptr;
        if (t && t->isArray() && (int)t->size() == n) {
            for (int w = 0; w < n; w++) {
                local[l][0].push_back((*t)[w][1].asNumber());
                local[l][1].push_back((*t)[w][2].asNumber());
            }
        } else {
            const double head = p->voiced[l] ? 0.15 : 0.0, tail = p->voiced[l] ? 0.15 : 0.0;
            std::vector<double> wsum(n);
            double weight = 0;
            for (int w = 0; w < n; w++) {
                const std::string& s = p->word[l][w];
                const bool stop = s.back() == '.' || s.back() == ':' || s.back() == ',';
                wsum[w] = s.size() + 2.0 + (stop ? 4.0 : 0.0);
                weight += wsum[w];
            }
            double at = head;
            const double span = std::max(0.1, p->dur[l] - head - tail);
            for (int w = 0; w < n; w++) {
                const double len = span * wsum[w] / weight;
                local[l][0].push_back(at);
                local[l][1].push_back(at + len * 0.8);
                at += len;
            }
        }
    }
    const double lead[NSCENES] = { 0.3, 0.3, 0.3, 0.3, 0.3, 0.5, 0.3, 0.3, 0.3, 0.3, 0.3 };
    const double hold[NSCENES] = { 0.6, 0.7, 1.0, 1.3, 1.4, 0.5, 0.6, 1.1, 1.0, 0.9, 3.0 };
    double s = 0;
    for (int l = 0; l < NLINES; l++) {
        p->scene[l] = s;
        p->start[l] = s + lead[l];
        for (size_t w = 0; w < local[l][0].size(); w++) {
            p->on[l].push_back(p->start[l] + local[l][0][w]);
            p->off[l].push_back(p->start[l] + local[l][1][w]);
        }
        s = p->end(l) + hold[l];
    }
    p->scene[NSCENES] = s;
    p->frames = (int)std::ceil(s * FPS);
    return p;
}

// ------------------------------------------------------------------ helpers
float easeOut(double s, double a, double b) {
    const double u = std::clamp((s - a) / std::max(1e-3, b - a), 0.0, 1.0);
    return (float)(1 - std::pow(1 - u, 3));
}
float enter(double s, double at) { return easeOut(s, at - 12.0 / FPS, at); }
double smooth(double x) { x = std::clamp(x, 0.0, 1.0); return x * x * (3.0 - 2.0 * x); }

Font heavy() { return fonts::loadFor({ "ArchivoBlack-Regular.ttf", "Inter-Bold.ttf" }, GLYPHS, 128); }
Font bold() { return fonts::loadFor({ "Inter-Bold.ttf" }, GLYPHS, 96); }
Font regular() { return fonts::loadFor({ "Inter-Regular.ttf" }, GLYPHS, 96); }
Font mono() { return fonts::loadFor({ "JetBrainsMono-Bold.ttf", "Inter-Bold.ttf" }, GLYPHS, 96); }

void text(Font f, const Ctx& cx, const std::string& s, Vector2 at, float size, Color col,
          float ax = 0, float ay = 0.5f, float spacing = 0) {
    dr::text(f, cx, s.c_str(), at, size, spacing, col, ax, ay);
}
float width(Font f, const std::string& s, float size, float spacing = 0) {
    return dr::measure(f, s.c_str(), size, spacing).x;
}
void outline(const Ctx& cx, float x, float y, float w, float h, float lw, Color col) {
    const Vector2 r[4] = { { x, y }, { x + w, y }, { x + w, y + h }, { x, y + h } };
    dr::polyStroke(cx, r, 4, true, lw, col);
}
Ctx faded(const Ctx& cx, float k) { Ctx m = cx; m.a *= k; return m; }
double moveTo(double s, double t0, double t1, double a, double b) { return a + (b - a) * easeOut(s, t0, t1); }
std::string fmt1(double v) {
    char b[16];
    std::snprintf(b, sizeof b, "%.1f", v);
    return b;
}

// Word wrap of `all` into lines no wider than maxW.
std::vector<std::string> wrap(Font f, const std::string& all, float size, float maxW) {
    std::vector<std::string> lines(1);
    std::istringstream in(all);
    for (std::string wd; in >> wd;) {
        const std::string tryL = lines.back().empty() ? wd : lines.back() + " " + wd;
        if (!lines.back().empty() && width(f, tryL, size) > maxW) lines.push_back(wd);
        else lines.back() = tryL;
    }
    return lines;
}

// A verdict stamp: an outlined plate, tilted, landing with a short overshoot.
void stamp(const Ctx& cx, const char* s, Vector2 at, double sNow, double land, Color col) {
    const double u = (sNow - (land - 8.0 / FPS)) / (8.0 / FPS);
    if (u <= 0) return;
    const float k = (float)std::min(1.0, u);
    const float scale = u < 1 ? 1.35f - 0.35f * k : 1.f;
    const Font f = heavy();
    const float w = width(f, s, 40, 2) + 44, h = 72;
    Ctx m = cx;
    m.o = mapP(cx, at);
    m.rot += -5.f;
    m.s *= scale;
    m.a *= k;
    dr::roundRect(m, -w / 2, -h / 2, w, h, 10, col);
    dr::roundRect(m, -w / 2 + 5, -h / 2 + 5, w - 10, h - 10, 7, BG);
    text(f, m, s, { 0, 2 }, 40, col, 0.5f, 0.5f, 2);
}

// ------------------------------------------------------------------ the board's sentence
// The prompt in DIM, then the board's own continuation typed at its measured 15.8 tok/s
// from `t0`. Hook and close: the same panel, so the reel loops on it.
constexpr float PANEL_Y = 290, PANEL_H = 300;
void sentencePanel(const Ctx& cx, double s, double t0, float k) {
    if (k <= 0.001f) return;
    Ctx m = faded(cx, k);
    const float x = 84, y = PANEL_Y, w = 912, h = PANEL_H;
    dr::roundRect(m, x, y, w, h, 14, withAlpha(PANEL, 0.94f));
    dr::rect(m, x, y, 6, h, TEAL);
    text(bold(), m, "the board's own output", { x + 32, y + 40 }, 30, DIM, 0, 0.5f);
    const double u = std::clamp((s - t0) / TYPE_S, 0.0, 1.0);
    const std::string all = std::string(PROMPT) + SAMPLE;
    const size_t promptN = std::strlen(PROMPT);
    const size_t shown = promptN + (size_t)std::lround(u * std::strlen(SAMPLE));
    const Font f = mono();
    const float size = 36, lh = 50;
    const auto lines = wrap(f, all, size, w - 64);
    size_t at = 0;   // index into `all` of the line start
    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& L = lines[i];
        const float yy = y + 100 + i * lh;
        float xx = x + 32;
        for (size_t c = 0; c < L.size(); c++) {
            const size_t gi = at + c;
            if (gi >= shown) break;
            const std::string ch(1, L[c]);
            text(f, m, ch, { xx, yy }, size, gi < promptN ? DIM : INK, 0, 0.5f);
            xx += width(f, ch, size);
        }
        // the caret, while typing
        if (u < 1 && shown >= at && shown <= at + L.size() && std::fmod(s * 2.5, 1.0) < 0.6)
            dr::rect(m, xx + 2, yy - 20, 16, 40, TEAL);
        at += L.size() + 1;
    }
}

// ------------------------------------------------------------------ the 3D board
// The board model of the first draft: an 86 x 54 mm PCB in decimetres, its two chips,
// ports and passives, with the Epiphany's 16 cores rising out of it as a 4x4 grid.
struct Cam { float r, a, y, tx, ty, tz; };
const Cam HERO0 = { 3.70f, -0.62f, 2.15f, 0.00f, -0.16f, 0.00f };
const Cam HERO1 = { 3.55f, -0.46f, 2.10f, 0.00f, -0.16f, 0.00f };
const Cam GRIDC = { 0.95f, 0.40f, 1.05f, -0.165f, 0.02f, -0.030f };
const Cam GRIDD = { 0.90f, 0.52f, 1.00f, -0.165f, 0.02f, -0.030f };
Cam mixCam(const Cam& A, const Cam& B, double f) {
    auto L = [f](float a, float b) { return (float)(a + (b - a) * f); };
    return { L(A.r, B.r), L(A.a, B.a), L(A.y, B.y), L(A.tx, B.tx), L(A.ty, B.ty), L(A.tz, B.tz) };
}

// When the 16 cores rise: over "sixteen cores" of line 2.
double coreAt(const Plan& p, int i) {
    const double a = p.W0(1, 6) - 0.28, b = p.W1(1, 7);
    return a + (b - a) * i / 16.0;
}

g3d::ScenePtr buildBoard(std::shared_ptr<Plan> plan) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->shadowMapSize = 1024;
    scene->ambient = 0.45f;
    scene->skyColor = mkColor(0xa8b8d8);
    scene->groundColor = mkColor(0x141a26);
    scene->fogColor = BG; scene->fogNear = 14; scene->fogFar = 30;
    scene->camera.fov = 30;
    scene->camera.up = [plan](int t, g3d::Camera& cam) {
        const Plan& p = *plan;
        const double s = t / (double)FPS;
        Cam c;
        switch (p.sceneAt(s)) {
            case 0: c = mixCam(HERO0, HERO1, (s - p.scene[0]) / (p.scene[1] - p.scene[0])); break;
            case 1: {
                // dolly from the hero view to the Epiphany chip, landing before "sixteen"
                const double land = std::max(p.scene[1] + 0.8, p.W0(1, 6) - 0.5);
                const double f = smooth((s - p.scene[1]) / (land - p.scene[1]));
                const Cam near = mixCam(GRIDC, GRIDD, (s - land) / std::max(0.1, p.scene[2] - land));
                c = mixCam(HERO1, s < land ? GRIDC : near, s < land ? f : 1.0);
                break;
            }
            default: c = HERO1; break;                                   // the board is not drawn after scene 2
        }
        cam.position = { c.tx + c.r * std::sin(c.a), c.y, c.tz + c.r * std::cos(c.a) };
        cam.target = { c.tx, c.ty, c.tz };
    };

    g3d::Material floorMat;
    floorMat.color = mkColor(0x121824); floorMat.roughness = 0.95f;
    scene->add(g3d::Geometry::plane(60, 60), floorMat)->position = { 0, -0.055f, 0 };

    g3d::Object3D* board = scene->add();
    board->up = [](int t, g3d::Object3D& o) {
        o.rotation = { 0.0f, (float)(-6.0 + 12.0 * std::sin(t * 0.0022)), 0.0f };
        o.position = { 0.0f, (float)(0.004 * std::sin(t * 0.021)), 0.0f };
    };
    auto part = [board](Vector3 size, Vector3 pos, Color col, float rough, float metal, int bands) {
        g3d::Material m;
        m.color = col; m.roughness = rough; m.metalness = metal;
        m.shading = g3d::Shading::Toon;
        m.toon.bands = bands;
        m.toon.rim = Color{ 130, 200, 255, 110 };
        g3d::Object3D* o = board->add(g3d::Geometry::box(size.x, size.y, size.z), m);
        o->position = pos;
        return o;
    };
    const Color pcb = mkColor(0x1d4a78), pcbDark = mkColor(0x143156), gold = mkColor(0xd0a53a),
                chipBlack = mkColor(0x11151c), metal = mkColor(0xa9b3c0);

    part({ 0.86f, 0.028f, 0.54f }, { 0, 0.000f, 0 }, pcb, 0.75f, 0.05f, 3);
    part({ 0.862f, 0.010f, 0.542f }, { 0, -0.012f, 0 }, pcbDark, 0.80f, 0.05f, 3);

    auto mountHole = [board, gold](float x, float z) {
        g3d::Material m;
        m.color = gold; m.roughness = 0.30f; m.metalness = 0.95f;
        m.shading = g3d::Shading::Toon; m.toon.bands = 3;
        g3d::Object3D* ring = board->add(g3d::Geometry::torus(0.016f, 0.0045f, 28, 10), m);
        ring->position = { x, 0.016f, z };
        ring->rotation = { (float)(PI / 2), 0, 0 };
        g3d::Material dm;
        dm.color = mkColor(0x070a10); dm.roughness = 0.9f; dm.metalness = 0.0f;
        dm.shading = g3d::Shading::Toon; dm.toon.bands = 3;
        g3d::Object3D* hole = board->add(g3d::Geometry::cylinder(0.0130f, 0.030f, 20), dm);
        hole->position = { x, 0.003f, z };
    };
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2)
            mountHole(sx * 0.393f, sz * 0.232f);

    // chips, left to right as on the board: E16G301 (Epiphany), then the Zynq
    part({ 0.150f, 0.012f, 0.150f }, { -0.165f, 0.030f, -0.030f }, chipBlack, 0.55f, 0.15f, 4);
    part({ 0.132f, 0.012f, 0.132f }, { -0.165f, 0.042f, -0.030f }, mkColor(0xcfd6dd), 0.45f, 0.55f, 4);
    part({ 0.160f, 0.018f, 0.160f }, { 0.055f, 0.033f, 0.015f }, chipBlack, 0.55f, 0.15f, 4);
    auto pinRing = [&part, gold](Vector3 c, float w, float d) {
        for (int i = 0; i < 9; i++) {
            const float fx = c.x - w * 0.5f + (i + 0.5f) * (w / 9.0f);
            part({ 0.006f, 0.004f, 0.009f }, { fx, 0.024f, c.z - d * 0.5f - 0.005f }, gold, 0.30f, 0.95f, 3);
            part({ 0.006f, 0.004f, 0.009f }, { fx, 0.024f, c.z + d * 0.5f + 0.005f }, gold, 0.30f, 0.95f, 3);
        }
        for (int i = 0; i < 9; i++) {
            const float fz = c.z - d * 0.5f + (i + 0.5f) * (d / 9.0f);
            part({ 0.009f, 0.004f, 0.006f }, { c.x - w * 0.5f - 0.005f, 0.024f, fz }, gold, 0.30f, 0.95f, 3);
            part({ 0.009f, 0.004f, 0.006f }, { c.x + w * 0.5f + 0.005f, 0.024f, fz }, gold, 0.30f, 0.95f, 3);
        }
    };
    pinRing({ -0.165f, 0, -0.030f }, 0.150f, 0.150f);
    pinRing({ 0.055f, 0, 0.015f }, 0.160f, 0.160f);

    // DDR3 pair, flash, small parts
    part({ 0.090f, 0.014f, 0.125f }, { -0.020f, 0.031f, 0.020f }, chipBlack, 0.70f, 0.10f, 3);
    part({ 0.105f, 0.014f, 0.090f }, { 0.045f, 0.031f, -0.145f }, chipBlack, 0.70f, 0.10f, 3);
    part({ 0.055f, 0.012f, 0.040f }, { -0.330f, 0.030f, 0.120f }, mkColor(0x1a1f28), 0.80f, 0.0f, 3);
    part({ 0.020f, 0.012f, 0.020f }, { 0.250f, 0.032f, -0.170f }, mkColor(0x2a2f38), 0.70f, 0.1f, 3);
    part({ 0.095f, 0.004f, 0.050f }, { -0.300f, 0.030f, 0.190f }, mkColor(0xe4e6e8), 0.60f, 0.0f, 3);
    {
        g3d::Material sm;
        sm.color = mkColor(0xeceff2); sm.roughness = 0.7f; sm.metalness = 0.0f;
        sm.shading = g3d::Shading::Toon; sm.toon.bands = 3;
        board->add(g3d::Geometry::cylinder(0.026f, 0.004f, 24), sm)->position = { 0.230f, 0.030f, 0.030f };
    }

    // power jack and LAN port on the short +x edge; microSD and micro-USB at the near-left corner
    const Color portMetal = mkColor(0x707b88), portDark = mkColor(0x090c11), jackBlack = mkColor(0x14171d);
    part({ 0.130f, 0.100f, 0.150f }, { 0.375f, 0.062f, 0.105f }, portMetal, 0.40f, 0.85f, 4);
    part({ 0.008f, 0.058f, 0.112f }, { 0.442f, 0.058f, 0.105f }, portDark, 0.85f, 0.0f, 3);
    part({ 0.050f, 0.006f, 0.012f }, { 0.395f, 0.114f, 0.075f }, portDark, 0.85f, 0.0f, 3);
    part({ 0.050f, 0.006f, 0.012f }, { 0.395f, 0.114f, 0.135f }, portDark, 0.85f, 0.0f, 3);
    part({ 0.100f, 0.075f, 0.085f }, { 0.370f, 0.052f, -0.115f }, jackBlack, 0.60f, 0.30f, 3);
    part({ 0.008f, 0.038f, 0.038f }, { 0.422f, 0.052f, -0.115f }, portDark, 0.85f, 0.0f, 3);
    part({ 0.055f, 0.012f, 0.095f }, { -0.395f, 0.030f, -0.090f }, mkColor(0x1a1f28), 0.80f, 0.0f, 3);
    part({ 0.050f, 0.045f, 0.045f }, { -0.400f, 0.035f, -0.195f }, metal, 0.40f, 0.80f, 4);

    // small passives over the free board area (seeded: every render identical)
    {
        unsigned seed = 12345u;
        auto rnd = [&seed]() { seed = seed * 1664525u + 1013904223u; return (float)((seed >> 8) & 0xFFFF) / 65535.0f; };
        auto blocked = [](float x, float z) {
            if (std::fabs(x - 0.055f) < 0.100f && std::fabs(z - 0.015f) < 0.100f) return true;
            if (std::fabs(x + 0.165f) < 0.095f && std::fabs(z + 0.030f) < 0.095f) return true;
            if (x > 0.240f) return true;
            if (std::fabs(x + 0.020f) < 0.055f && std::fabs(z - 0.020f) < 0.075f) return true;
            if (std::fabs(x - 0.045f) < 0.060f && std::fabs(z + 0.145f) < 0.055f) return true;
            if (std::fabs(x) > 0.370f && std::fabs(z) > 0.208f) return true;
            if (std::fabs(x + 0.300f) < 0.060f && std::fabs(z - 0.190f) < 0.035f) return true;
            if (x < -0.355f && z < -0.040f) return true;
            return false;
        };
        int placed = 0;
        for (int guard = 0; guard < 500 && placed < 30; guard++) {
            const float x = -0.385f + rnd() * 0.770f;
            const float z = -0.225f + rnd() * 0.450f;
            if (blocked(x, z)) continue;
            const float w = 0.012f + rnd() * 0.020f;
            const float d = 0.010f + rnd() * 0.016f;
            const float h = 0.008f + rnd() * 0.004f;
            const int kind = (int)(rnd() * 3.0f);
            const Color c = kind == 0 ? mkColor(0x2a2f38) : kind == 1 ? mkColor(0x8a6f4a) : mkColor(0xb9c2cc);
            part({ w, h, d }, { x, 0.026f, z }, c, 0.75f, 0.15f, 3);
            placed++;
        }
    }

    // the 16 cores rise out of the Epiphany on "sixteen cores", and stay
    const float GX = -0.170f, GZ = -0.035f, GS = 0.030f, GY = 0.160f;
    for (int i = 0; i < 16; i++) {
        const int row = i / 4, col = i % 4;
        g3d::Material m;
        m.color = mkColor(0x123c4a);
        m.emissive = Color{ 40, 190, 180, 255 };
        m.roughness = 0.35f; m.metalness = 0.2f;
        m.shading = g3d::Shading::Toon; m.toon.bands = 3;
        g3d::Object3D* o = scene->add(g3d::Geometry::box(0.024f, 0.010f, 0.024f), m);
        o->position = { GX + (col - 1.5f) * GS, GY, GZ + (row - 1.5f) * GS };
        o->up = [plan, i](int t, g3d::Object3D& ob) {
            const double s = t / (double)FPS, t0 = coreAt(*plan, i);
            const double k = smooth((s - t0) / 0.4);
            const double breath = 1.0 + 0.05 * std::sin(s * 1.8 + i);
            ob.scale = { (float)(k * breath), (float)k, (float)(k * breath) };
            ob.visible = s >= t0;
        };
    }

    scene->lights.push_back(g3d::Light::directional({ -4.0f, 7.0f, 4.0f }, mkColor(0xfff2e0), 1.15f, true));
    scene->lights.back().shadowExtent = 3.0f;
    scene->lights.push_back(g3d::Light::directional({ 4.0f, 3.0f, -4.0f }, mkColor(0x63e2e0), 0.70f, false));
    scene->lights.push_back(g3d::Light::point({ 0.0f, 1.1f, 1.6f }, mkColor(0xbfd4ff), 0.55f, 8.0f));
    scene->lines.enabled = true;
    scene->lines.width = 2.0f;
    scene->lines.creaseAngle = 34;
    scene->lines.wobble = 0.7f;
    scene->lines.boilEvery = 3;
    scene->lines.color = mkColor(0x0a0d15);
    return scene;
}

// A dark ramp under the captions / headlines, so type clears the board.
void scrim(const Ctx& cx, float y0) {
    const int N = 30;
    for (int i = 0; i < N; i++) {
        const double f = (double)i / (N - 1);
        const float h = (H - y0) / N;
        dr::rect(cx, -2, y0 + i * h, W + 4, h + 2, withAlpha(BG, (float)std::pow(f, 0.5) * 0.97f));
    }
}

// ------------------------------------------------------------------ scenes
constexpr double TYPE0 = -0.10;        // the hook's typing began just before frame 0

void drawHook(const Ctx& cx, double s, const Plan& p) {
    scrim(cx, 1180);
    sentencePanel(cx, s, TYPE0, 1);
    const float k1 = enter(s, p.W0(0, 1)), k2 = enter(s, p.W0(0, 7));
    text(heavy(), faded(cx, k1), "A $99 BOARD", { W / 2.f, 1420 + (1 - k1) * 14 }, 96, INK, 0.5f, 0.5f);
    text(heavy(), faded(cx, k1), "FROM 2012", { W / 2.f, 1515 + (1 - k1) * 14 }, 64, SLATE, 0.5f, 0.5f);
    text(heavy(), faded(cx, k2), "WROTE THIS.", { W / 2.f, 1620 + (1 - k2) * 14 }, 96, AMBER, 0.5f, 0.5f);
}

// The spec card for the machine: name, then the cores and their memory, built up by the line.
void machineCard(const Ctx& cx, double s, const Plan& p) {
    const float k = enter(s, p.W0(1, 2));
    if (k <= 0.001f) return;
    Ctx m = faded(cx, k);
    const float x = 84, y = 196 + (1 - k) * -16, w = 912, h = 210;
    dr::roundRect(m, x, y, w, h, 14, withAlpha(PANEL, 0.94f));
    dr::rect(m, x, y, 6, h, TEAL);
    text(heavy(), m, "PARALLELLA", { x + 32, y + 44 }, 44, INK, 0, 0.5f, 2);
    text(bold(), m, "$99 \xC2\xB7 Kickstarter, 2012", { x + w - 32, y + 44 }, 30, SLATE, 1, 0.5f);
    const float kc = enter(s, p.W0(1, 6));
    const float km = enter(s, p.W0(1, 10));
    const float kt = enter(s, p.W0(1, 12));
    text(bold(), faded(m, enter(s, p.W0(1, 4))), "Epiphany chip", { x + 32, y + 112 }, 34, TEAL, 0, 0.5f);
    text(mono(), faded(m, kc), "16 cores", { x + 300, y + 112 }, 38, INK, 0, 0.5f);
    text(mono(), faded(m, km), "\xC3\x97 32 KB", { x + 500, y + 112 }, 38, INK, 0, 0.5f);
    text(mono(), faded(m, kt), "= 512 KB of memory, in all", { x + 32, y + 168 }, 38, TEAL, 0, 0.5f);
}

void drawMachine(const Ctx& cx, double s, const Plan& p) {
    scrim(cx, 1260);
    machineCard(cx, s, p);
}

// ---- the problem and the shrink: a 2D frame, areas to scale
// Area = bytes. The cores' 512 KB is a 4x4 block of tiles; the model is a square around it.
constexpr float MEM_SIDE = 56;                                   // 0.524 MB
float sideOf(double mb) { return MEM_SIDE * (float)std::sqrt(mb / SRAM_MB); }
constexpr float BLOCK_CX = 540, BLOCK_BOTTOM = 950;               // the model squares sit on this
constexpr float CHIP_Y = 1090;                                    // the cores' panel

void coresPanel(const Ctx& cx, float k, float alarm) {
    if (k <= 0.001f) return;
    Ctx m = faded(cx, k);
    const float x = 84, y = CHIP_Y, w = 912, h = 150;
    dr::roundRect(m, x, y, w, h, 14, PANEL);
    // 16 tiles whose summed area is MEM_SIDE^2
    const float tile = MEM_SIDE / 4;
    const float tx = BLOCK_CX - MEM_SIDE / 2, ty = y + (h - MEM_SIDE) / 2;
    for (int i = 0; i < 16; i++)
        dr::rect(m, tx + (i % 4) * tile + 1, ty + (i / 4) * tile + 1, tile - 2, tile - 2, TEAL);
    if (alarm > 0.001f) {
        Ctx a = faded(m, alarm);
        outline(a, tx - 8, ty - 8, MEM_SIDE + 16, MEM_SIDE + 16, 4, CORAL);
    }
    text(bold(), m, "the chip's memory", { x + 32, y + 52 }, 30, DIM, 0, 0.5f);
    text(mono(), m, "512 KB", { x + 32, y + 100 }, 40, TEAL, 0, 0.5f);
    text(bold(), m, "drawn to scale:", { x + w - 32, y + 52 }, 30, DIM, 1, 0.5f);
    text(bold(), m, "area = bytes", { x + w - 32, y + 100 }, 30, DIM, 1, 0.5f);
}

// The weights stream from the model down into the cores: dashes flowing on a pipe.
void stream(const Ctx& cx, double s, float k, float topY) {
    if (k <= 0.001f) return;
    Ctx m = faded(cx, k);
    const float y0 = topY + 10, y1 = CHIP_Y + (150 - MEM_SIDE) / 2 - 14;
    if (y1 <= y0 + 10) return;
    const float ph = (float)std::fmod(s * 1.6, 1.0) * 30;
    for (float y = y0 - 30 + ph; y < y1; y += 30) {
        const float a = std::max(y, y0), b = std::min(y + 16, y1);
        if (b > a) dr::rect(m, BLOCK_CX - 3, a, 6, b - a, AMBER);
    }
    const Vector2 head[3] = { { BLOCK_CX - 14, y1 - 2 }, { BLOCK_CX + 14, y1 - 2 }, { BLOCK_CX, y1 + 12 } };
    dr::fillPoly(m, head, 3, AMBER);
    text(bold(), m, "streamed in, every token", { BLOCK_CX + 24, (y0 + y1) / 2 }, 30, AMBER, 0, 0.5f);
}

// The fp32 model: a square of `side`, weight texture inside.
void modelBlock(const Ctx& cx, float side, Color fill, Color edge) {
    if (side <= 1) return;
    const float x = BLOCK_CX - side / 2, y = BLOCK_BOTTOM - side;
    dr::rect(cx, x, y, side, side, fill);
    const float step = 22;
    for (float gx = x + step; gx < x + side - 2; gx += step) dr::rect(cx, gx, y, 1, side, withAlpha(edge, 0.18f));
    for (float gy = y + step; gy < y + side - 2; gy += step) dr::rect(cx, x, gy, side, 1, withAlpha(edge, 0.18f));
    outline(cx, x, y, side, side, 3, edge);
}

void drawProblem(const Ctx& cx, double s, const Plan& p) {
    coresPanel(cx, 1, enter(s, p.W0(2, 11)) * (float)(0.6 + 0.4 * std::cos((s - p.W0(2, 11)) * 6)));
    // the model grows from the cores' size to its own on "fifteen million weights"
    const float big = sideOf(FP32_MB);
    const float g = easeOut(s, p.W0(2, 5) - 0.28, p.W0(2, 8));
    const float side = MEM_SIDE + (big - MEM_SIDE) * g;
    if (s > p.W0(2, 5) - 0.28) modelBlock(cx, side, withAlpha(SLATE, 0.30f), SLATE);
    const float top = BLOCK_BOTTOM - big;
    Ctx l1 = faded(cx, enter(s, p.W0(2, 6)));
    text(bold(), l1, "a small story model", { BLOCK_CX - big / 2 + 28, top + 40 }, 32, INK, 0, 0.5f);
    text(mono(), l1, "15.2 M weights", { BLOCK_CX - big / 2 + 28, top + 88 }, 40, INK, 0, 0.5f);
    Ctx l2 = faded(cx, enter(s, p.W0(2, 8)));
    text(mono(), l2, "60.8 MB", { BLOCK_CX, top + big / 2 }, 96, INK, 0.5f, 0.5f);
    text(bold(), l2, "at full precision (fp32)", { BLOCK_CX, top + big / 2 + 76 }, 32, SLATE, 0.5f, 0.5f);
    // it cannot fit
    Ctx l3 = faded(cx, enter(s, p.W0(2, 11)));
    text(heavy(), l3, "116\xC3\x97 too big", { BLOCK_CX - big / 2 - 4, top - 44 }, 48, CORAL, 0, 0.5f);
    stream(cx, s, enter(s, p.W0(2, 16)), BLOCK_BOTTOM);
}

// Eight illustrative weights; on "minus one, zero and one" each snaps to its ternary value.
const float WEIGHTS[8] = { 0.137f, -0.402f, 0.018f, 0.291f, -0.077f, -0.344f, 0.009f, 0.226f };
int ternary(float w) { return std::fabs(w) < 0.1f ? 0 : (w > 0 ? 1 : -1); }

void drawShrink(const Ctx& cx, double s, const Plan& p) {
    coresPanel(cx, 1, 0);
    const float big = sideOf(FP32_MB), small = sideOf(TERN_MB);
    const double shrinkAt = p.W0(3, 15) - 0.28;
    const float g = easeOut(s, shrinkAt, shrinkAt + 0.7);
    const float side = big + (small - big) * g;
    const float top = BLOCK_BOTTOM - big;
    // the ghost of the fp32 block stays once it has shrunk
    if (g > 0.001f) outline(faded(cx, 0.6f * g), BLOCK_CX - big / 2, top, big, big, 3, SLATE);
    text(mono(), faded(cx, 1 - 0.4f * g), "60.8 MB fp32", { BLOCK_CX - big / 2 + 24, top + 36 }, 32, g > 0.5f ? SLATE : INK, 0, 0.5f);
    modelBlock(cx, side, mixC(withAlpha(SLATE, 0.30f), withAlpha(AMBER, 0.85f), g), mixC(SLATE, AMBER, g));
    stream(cx, s, 1, BLOCK_BOTTOM);
    // the weight strip: in the block, before the shrink
    const float stripK = enter(s, p.W0(3, 2)) * (1 - easeOut(s, shrinkAt - 0.3, shrinkAt));
    if (stripK > 0.001f) {
        Ctx m = faded(cx, stripK);
        const float cw = 96, ch = 96, gap = 8, x0 = BLOCK_CX - (8 * cw + 7 * gap) / 2, y0 = top + 170;
        const double snapAt = p.W0(3, 10) - 0.28;
        text(bold(), m, "weights (illustrative)", { BLOCK_CX, y0 - 34 }, 30, DIM, 0.5f, 0.5f);
        for (int i = 0; i < 8; i++) {
            const float snap = easeOut(s, snapAt + i * 0.06, snapAt + i * 0.06 + 0.25);
            const float x = x0 + i * (cw + gap);
            const int tv = ternary(WEIGHTS[i]);
            const Color tc = tv > 0 ? TEAL : tv < 0 ? CORAL : DIM;
            dr::roundRect(m, x, y0, cw, ch, 10, mixC(PANEL, withAlpha(tc, 0.28f), snap));
            char b[16];
            std::snprintf(b, sizeof b, "%.2f", WEIGHTS[i]);
            std::string raw = b;
            if (raw.rfind("-0", 0) == 0) raw = "\xE2\x88\x92" + raw.substr(2); else if (raw.rfind("0", 0) == 0) raw = raw.substr(1);
            const std::string tern = tv > 0 ? "+1" : tv < 0 ? "\xE2\x88\x92" "1" : "0";
            text(mono(), faded(m, 1 - snap), raw, { x + cw / 2, y0 + ch / 2 }, 28, INK, 0.5f, 0.5f);
            text(mono(), faded(m, snap), tern, { x + cw / 2, y0 + ch / 2 }, 44, tc, 0.5f, 0.5f);
        }
        Ctx q = faded(m, enter(s, p.W0(3, 10)));
        text(bold(), q, "trained this way from the start", { BLOCK_CX, y0 + ch + 48 }, 32, INK, 0.5f, 0.5f);
    }
    // after the shrink: its size and what it writes
    if (g > 0.001f) {
        const float smallTop = BLOCK_BOTTOM - small;
        Ctx a = faded(cx, enter(s, p.W0(3, 16)));
        text(mono(), a, "3.8 MB ternary", { BLOCK_CX, smallTop - 84 }, 44, AMBER, 0.5f, 0.5f);
        text(bold(), a, "16\xC3\x97 smaller", { BLOCK_CX, smallTop - 36 }, 34, AMBER, 0.5f, 0.5f);
        Ctx b = faded(cx, enter(s, p.W0(3, 19)));
        const Font f = regular();
        const auto lines = wrap(f, TERN_SAMPLE, 34, big - 80);
        text(bold(), b, "the ternary model writes:", { BLOCK_CX - big / 2 + 40, top + 112 }, 30, DIM, 0, 0.5f);
        for (size_t i = 0; i < lines.size(); i++)
            text(f, b, lines[i], { BLOCK_CX - big / 2 + 40, top + 164 + i * 48.f }, 34, INK, 0, 0.5f);
    }
}

// ---- the proof: the board's output and the reference, word for word
void textPanel(const Ctx& cx, float y, const char* label, Color labelCol, int nWords, float match) {
    const float x = 84, w = 912, h = 290;
    dr::roundRect(cx, x, y, w, h, 14, PANEL);
    dr::rect(cx, x, y, 6, h, labelCol);
    text(bold(), cx, label, { x + 32, y + 40 }, 30, labelCol, 0, 0.5f);
    const std::string all = std::string(PROMPT) + SAMPLE;
    const auto ws = words(all);
    const int promptW = (int)words(PROMPT).size();
    const Font f = mono();
    const float size = 34, lh = 48;
    const auto lines = wrap(f, all, size, w - 64);
    int wi = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        float xx = x + 32;
        for (const std::string& wd : words(lines[i])) {
            if (wi >= promptW + nWords) return;
            const Color c = wi < promptW ? DIM : mixC(INK, TEAL, match);
            text(f, cx, wd, { xx, y + 100 + i * lh }, size, c, 0, 0.5f);
            xx += width(f, wd + " ", size);
            wi++;
        }
    }
}

void drawProof(const Ctx& cx, double s, const Plan& p) {
    // the chip: every multiply on its cores
    {
        const float k = enter(s, p.W0(4, 0));                    // with the cut: no empty frame
        Ctx m = faded(cx, k);
        const float x = 84, y = 196, w = 912, h = 120;
        dr::roundRect(m, x, y, w, h, 14, PANEL);
        for (int i = 0; i < 16; i++) {
            const float on = (float)std::clamp((s - (p.W0(4, 1) - 0.28) - i * 0.03) / 0.15, 0.0, 1.0);
            dr::rect(m, x + 32 + (i % 4) * 22, y + 16 + (i / 4) * 22, 18, 18, mixC(GRID, TEAL, on));
        }
        text(bold(), m, "every multiply", { x + 150, y + 42 }, 34, INK, 0, 0.5f);
        text(bold(), m, "on the Epiphany's cores", { x + 150, y + 84 }, 34, TEAL, 0, 0.5f);
    }
    const int nW = (int)words(SAMPLE).size();
    const double a = p.W0(4, 6) - 0.28, b = p.W1(4, 12);
    const int shown = (int)std::lround(std::clamp((s - a) / (b - a), 0.0, 1.0) * nW);
    const float match = enter(s, p.W0(4, 13));
    const float kb = enter(s, p.W0(4, 2));                        // the empty panels, then the words
    if (kb > 0.001f) {
        textPanel(faded(cx, kb), 380, "board (all matmuls on the Epiphany)", TEAL, shown, match);
        textPanel(faded(cx, kb), 700, "reference (numpy, full recompute)", SLATE, shown, match);
    }
    stamp(cx, "24 OF 24 TOKENS IDENTICAL", { W / 2.f, 1080 }, s, p.W0(4, 13) + 0.1, TEAL);
}

// ================================================================== chapter 2: HOW FAST?
// One ruler. Each scene draws it as the previous scene left it, then its own change.
void ruler(const Ctx& cx, float k) {
    if (k <= 0.001f) return;
    Ctx m = cx;
    m.a *= k;
    for (int v = 0; v <= 90; v += 10) {
        const float y = yOf(v);
        dr::rect(m, RULE_X0, y - 1, RULE_X1 - RULE_X0, v == 0 ? 3.f : 2.f, v == 0 ? DIM : GRID);
        text(mono(), m, std::to_string(v), { RULE_X0 - 18, y }, 32, DIM, 1, 0.5f);
    }
    text(bold(), m, "tokens / second", { RULE_X0 - 18, YTOP - 30 }, 32, DIM, 0, 0.5f);
}

// The projection: dashed, never solid; it rises to `v`.
void projection(const Ctx& cx, double v, float tagK, float pulse) {
    if (v <= 0.05) return;
    const float y = yOf(v);
    const float lw = 5.f + 3.f * pulse;
    for (float x = RULE_X0; x < RULE_X1; x += 34) dr::strokeSeg(cx, { x, y }, { std::min(x + 20, RULE_X1), y }, lw, AMBER);
    if (tagK > 0.001f) {
        Ctx m = cx;
        m.a *= tagK;
        text(mono(), m, "PROJECTED 60", { RULE_X1, y - 30 + (1 - tagK) * 10 }, 34, AMBER, 1, 0.5f);
    }
}

// A solid bar from 0 to v, with its value above it (when valueK > 0).
void bar(const Ctx& cx, float xc, double v, Color col, float valueK, const std::string& value, Color valueCol) {
    if (v <= 0.01) return;
    const float top = yOf(v);
    dr::rect(cx, xc - BAR_W / 2, top, BAR_W, Y0 - top, col);
    if (valueK > 0.001f) {
        Ctx m = cx;
        m.a *= valueK;
        if (std::abs(top - yOf(PROJECTED)) < 56)
            text(mono(), m, value, { xc + BAR_W / 2 + 14, top + 26 }, 40, valueCol, 0, 0.5f);
        else
            text(mono(), m, value, { xc, top - 30 + (1 - valueK) * 8 }, 40, valueCol, 0.5f, 0.5f);
    }
}

void columnName(const Ctx& cx, float xc, const char* name, float k, Color col) {
    if (k <= 0.001f) return;
    Ctx m = cx;
    m.a *= k;
    text(bold(), m, name, { xc, Y0 + 36 }, 30, col, 0.5f, 0.5f);
}

// The DMA chunk gauge: one setting, flipped.
void chunkGauge(const Ctx& cx, float k, float flip) {
    if (k <= 0.001f) return;
    Ctx m = cx;
    m.a *= k;
    const float x = 690, y = 880 + (1 - k) * 12, w = 240, h = 132;
    dr::roundRect(m, x, y, w, h, 14, PANEL);
    text(bold(), m, "DMA chunk", { x + w / 2, y + 32 }, 32, DIM, 0.5f, 0.5f);
    // the old value slides out upward, the new one in from below
    Ctx a = m, b = m;
    a.a *= 1 - flip;
    b.a *= flip;
    text(mono(), a, "8 KB", { x + w / 2, y + 86 - 20 * flip }, 48, AMBER, 0.5f, 0.5f);
    text(mono(), b, "1 KB", { x + w / 2, y + 86 + 20 * (1 - flip) }, 48, TEAL, 0.5f, 0.5f);
}

// The batch: a column of stream segments, one per story, each 1/8 of the board's aggregate.
void streamStack(const Ctx& cx, double v, double perBoard, Color c1, Color c2) {
    if (v <= 0.01) return;
    const double seg = perBoard / STREAMS;
    for (int i = 0; i < 2 * STREAMS; i++) {
        const double a = i * seg, b = std::min(v, (i + 1) * seg);
        if (b <= a) break;
        const float y1 = yOf(a), y0 = yOf(b);
        dr::rect(cx, EPI_X - BAR_W / 2, y0 + 1.5f, BAR_W, std::max(0.f, y1 - y0 - 3.f), i < STREAMS ? c1 : c2);
    }
}

// The bridge: "So it runs. How fast? On paper, sixty tokens a second." The ruler is up from
// the cut; the dashed projection climbs to 60 on "sixty".
double riseFrom(const Plan& p) { return p.W0(5, 5) - 0.28; }
double riseTo(const Plan& p) { return p.W0(5, 7) + 0.2; }
void drawBridge(const Ctx& cx, double s, const Plan& p) {
    ruler(cx, 1);
    projection(cx, moveTo(s, riseFrom(p), riseTo(p), 0.0, PROJECTED), enter(s, p.W0(5, 8)), 0);
    // "So it runs." closes chapter 1 on the new ground; "How fast?" replaces it
    const float k0 = enter(s, p.W0(5, 0)) * (1 - easeOut(s, p.W0(5, 3) - 0.4, p.W0(5, 3) - 0.28));
    text(heavy(), faded(cx, k0), "IT RUNS.", { W / 2.f, 420 }, 112, TEAL, 0.5f, 0.5f);
    const float k1 = enter(s, p.W0(5, 3)), k2 = enter(s, p.W0(5, 7));
    text(heavy(), faded(cx, k1), "HOW FAST?", { W / 2.f, 420 + (1 - k1) * 14 }, 112, INK, 0.5f, 0.5f);
    text(heavy(), faded(cx, k2), "60 TOK/S ON PAPER.", { W / 2.f, 540 + (1 - k2) * 14 }, 80, AMBER, 0.5f, 0.5f);
    columnName(cx, EPI_X, "EPIPHANY", enter(s, p.W0(5, 0)), TEAL);
}

void armColumn(const Ctx& cx, double v, float k) {
    bar(cx, ARM_X, v, SLATE, k, fmt1(ARM), SLATE);
    columnName(cx, ARM_X, "ARM A9", 1, SLATE);
}

// "The board's own ARM processor, for comparison, manages fourteen."
void drawArm(const Ctx& cx, double s, const Plan& p) {
    ruler(cx, 1);
    projection(cx, PROJECTED, 1, 0);
    columnName(cx, EPI_X, "EPIPHANY", 1, TEAL);
    const double v = moveTo(s, p.W0(6, 3) - 0.28, p.W0(6, 8), 0.0, ARM);
    if (v > 0.01) armColumn(cx, v, enter(s, p.W0(6, 8)));
}

void drawMeasure(const Ctx& cx, double s, const Plan& p) {
    ruler(cx, 1);
    armColumn(cx, ARM, 1);
    columnName(cx, EPI_X, "EPIPHANY", 1, TEAL);
    const double flipAt = p.W0(7, 11);
    double v = moveTo(s, p.W0(7, 0) - 0.28, p.W0(7, 4), 0.0, FIRST);
    if (s > flipAt - 0.28) v = moveTo(s, flipAt - 0.28, p.W0(7, 13), FIRST, BAND1);
    const bool held = s >= p.W0(7, 13);
    // the band the retuned runs land in
    if (held) {
        Ctx m = cx;
        m.a *= enter(s, p.W0(7, 13) + 0.2);
        dr::rect(m, RULE_X0, yOf(BAND1), RULE_X1 - RULE_X0, yOf(BAND0) - yOf(BAND1), withAlpha(TEAL, 0.16f));
    }
    const std::string label = held ? "57\xE2\x80\x93" "59" : fmt1(v);
    bar(cx, EPI_X, v, TEAL, enter(s, p.W0(7, 0) + 0.1), label, TEAL);
    projection(cx, PROJECTED, 1, 0);
    chunkGauge(cx, enter(s, p.W0(7, 9)), easeOut(s, flipAt - 0.28, flipAt));
    stamp(cx, "PROJECTION HELD", { 600, 480 }, s, p.W1(7, 13) + 0.45, TEAL);
}

// "But that was one matrix pass, not the model. The whole model runs at just under sixteen."
// The pass stays as a ghost; the model's bar drops to 15.8, beside the ARM's 13.8.
void drawTwist(const Ctx& cx, double s, const Plan& p) {
    ruler(cx, 1);
    armColumn(cx, ARM, 1);
    columnName(cx, EPI_X, "EPIPHANY", 1, TEAL);
    const double dropAt = p.W0(8, 9);
    const double v = moveTo(s, dropAt - 0.28, p.W0(8, 16), BAND1, DECODER);
    if (s > dropAt - 0.28) outline(faded(cx, 0.55f), EPI_X - BAR_W / 2, yOf(BAND1), BAR_W, Y0 - yOf(BAND1), 3, TEAL);
    bar(cx, EPI_X, v, TEAL, s > dropAt - 0.28 ? enter(s, p.W0(8, 16)) : 1.f,
        s > dropAt - 0.28 ? fmt1(DECODER) : std::string("57\xE2\x80\x93" "59"), TEAL);
    projection(cx, PROJECTED, 1, 0);
    stamp(cx, "ONE MATRIX PASS", { EPI_X + 40, 900 }, s, p.W0(8, 4), CORAL);
    // the bar is the model that wrote chapter 1's sentence
    Ctx m = faded(cx, enter(s, p.W0(8, 11)));
    text(bold(), m, "the whole model:", { EPI_X + BAR_W / 2 + 14, yOf(DECODER) - 60 }, 32, TEAL, 0, 0.5f);
    text(bold(), m, "the one that wrote", { EPI_X + BAR_W / 2 + 14, yOf(DECODER) - 20 }, 30, DIM, 0, 0.5f);
    text(bold(), m, "the sentence", { EPI_X + BAR_W / 2 + 14, yOf(DECODER) + 16 }, 30, DIM, 0, 0.5f);
}

void drawBatch(const Ctx& cx, double s, const Plan& p) {
    ruler(cx, 1);
    armColumn(cx, ARM, 1);
    // ghost of the matrix pass, the one-story mark, then the stories stacking up
    {
        Ctx g = cx;
        g.a *= 0.55f;
        outline(g, EPI_X - BAR_W / 2, yOf(BAND1), BAR_W, Y0 - yOf(BAND1), 3, TEAL);
    }
    double v = DECODER;
    const double t8 = p.W0(9, 4) - 0.28, t16 = p.W0(9, 9) - 0.28;
    if (s > t8) v = moveTo(s, t8, p.W0(9, 7), DECODER, BATCH8);   // lands on "once": held through "forty"
    if (s > t16) v = moveTo(s, t16, p.W0(9, 11), BATCH8, BOARDS2);
    if (s <= t8) bar(cx, EPI_X, v, TEAL, 1, fmt1(DECODER), TEAL);
    else streamStack(cx, v, BATCH8, TEAL, TEAL2);
    // the single-story level stays marked beside the column
    dr::strokeSeg(cx, { EPI_X - BAR_W / 2 - 22, yOf(DECODER) }, { EPI_X - BAR_W / 2 - 4, yOf(DECODER) }, 4, INK);
    if (s > t8) {
        const bool two = s > t16;
        const float k = two ? enter(s, p.W0(9, 11)) : enter(s, p.W0(9, 8));
        Ctx m = cx;
        m.a *= k;
        text(mono(), m, two ? fmt1(BOARDS2) : fmt1(BATCH8), { EPI_X, yOf(v) - 30 }, 40, TEAL, 0.5f, 0.5f);
        text(bold(), cx, "8 stories", { EPI_X + BAR_W / 2 + 14, (yOf(0) + yOf(std::min(v, BATCH8))) / 2 }, 32, TEAL, 0, 0.5f);
        if (two) {
            Ctx q = cx;
            q.a *= enter(s, p.W0(9, 10));
            text(bold(), q, "board two: 8 more", { EPI_X + BAR_W / 2 + 14, yOf(72) }, 32, TEAL2, 0, 0.5f);
        }
    }
    columnName(cx, EPI_X, "EPIPHANY", 1, TEAL);
    projection(cx, PROJECTED, 1, 0);
    // the honest limit: each story is a slice of the column
    const float ck = enter(s, p.W0(9, 15));
    if (ck > 0.001f) {
        Ctx m = cx;
        m.a *= ck;
        const float x = 640, y = 1150 + (1 - ck) * 10;
        dr::roundRect(m, x, y, 300, 120, 14, PANEL);
        text(bold(), m, "each story", { x + 24, y + 32 }, 32, DIM, 0, 0.5f);
        text(mono(), m, "5 tok/s", { x + 24, y + 80 }, 40, INK, 0, 0.5f);
        text(bold(), m, "199 ms", { x + 276, y + 80 }, 30, DIM, 1, 0.5f);
    }
}

void drawRulerClose(const Ctx& cx, double s, const Plan& p, int frames) {
    const double end = frames / (double)FPS;
    // the loop: in the last 10 frames everything sinks back to the empty ruler of frame 0
    const float sink = 1 - easeOut(s, end - 10.0 / FPS, end);
    ruler(cx, 1);
    const float pulse = (float)std::max(0.0, 1 - std::abs(s - p.W0(10, 5)) / 0.6);
    armColumn(cx, ARM * sink, sink);
    streamStack(cx, BOARDS2 * sink, BATCH8, TEAL, TEAL2);
    if (sink > 0.99f) {
        // the matrix pass: the band the retuned runs landed in, lit as the line names it
        Ctx g = cx;
        g.a *= enter(s, p.W0(10, 5));
        dr::rect(g, RULE_X0, yOf(BAND1), RULE_X1 - RULE_X0, yOf(BAND0) - yOf(BAND1), withAlpha(TEAL, 0.16f + 0.2f * pulse));
        text(bold(), g, "matrix pass 57\xE2\x80\x93" "59", { RULE_X1, yOf(BAND0) + 26 }, 32, TEAL, 1, 0.5f);
        text(mono(), cx, fmt1(BOARDS2), { EPI_X, yOf(BOARDS2) - 30 }, 40, TEAL, 0.5f, 0.5f);
        text(bold(), cx, "16 stories, 2 boards", { EPI_X + BAR_W / 2 + 14, yOf(72) }, 32, TEAL2, 0, 0.5f);
        dr::strokeSeg(cx, { EPI_X - BAR_W / 2 - 22, yOf(DECODER) }, { EPI_X - BAR_W / 2 - 4, yOf(DECODER) }, 4, INK);
        text(mono(), cx, "1 story 15.8", { EPI_X + BAR_W / 2 + 14, yOf(DECODER) }, 32, INK, 0, 0.5f);
    }
    columnName(cx, EPI_X, "EPIPHANY", 1, TEAL);
    projection(cx, PROJECTED * sink, sink, 0);   // the tag leaves with the line
    // the line: spoken, set as the headline (no caption at the same time)
    const float k1 = enter(s, p.W0(10, 0)), k2 = enter(s, p.W0(10, 3)), k3 = enter(s, p.W0(10, 7));
    Ctx a = cx, b = cx, c = cx;
    a.a *= k1 * sink;
    b.a *= k2 * sink;
    c.a *= k3 * sink;
    text(heavy(), a, "SIXTY WAS TRUE.", { W / 2.f, 1455 }, 66, INK, 0.5f, 0.5f);
    text(heavy(), b, "FOR A MATRIX PASS.", { W / 2.f, 1535 }, 66, AMBER, 0.5f, 0.5f);
    text(heavy(), c, "MEASURE WHAT YOU SHIP.", { W / 2.f, 1630 }, 56, TEAL, 0.5f, 0.5f);
    Ctx src = cx;
    src.a *= enter(s, p.W1(10, 11) + 0.6) * sink;
    text(regular(), src, "Source: parallella-agent STORY.md, VERIFICATION.md \xC2\xB7 Parallella", { W / 2.f, 214 }, 26, DIM, 0.5f, 0.5f);
    text(regular(), src, "boards nano-6 / nano-3, 31 Oct\xE2\x80\x93" "2 Nov 2026", { W / 2.f, 248 }, 26, DIM, 0.5f, 0.5f);
}

// ------------------------------------------------------------------ captions
struct CaptionSeg { int line, from, to; };
const CaptionSeg SEGS[] = {
    { 1, 0, 2 }, { 1, 3, 7 }, { 1, 8, 11 }, { 1, 12, 16 },
    { 2, 0, 7 }, { 2, 8, 9 }, { 2, 10, 12 }, { 2, 13, 20 },
    { 3, 0, 9 }, { 3, 10, 14 }, { 3, 15, 17 }, { 3, 18, 21 },
    { 4, 0, 5 }, { 4, 6, 13 },
    { 6, 0, 4 }, { 6, 5, 8 },
    { 7, 0, 4 }, { 7, 5, 10 }, { 7, 11, 13 },
    { 8, 0, 8 }, { 8, 9, 16 },
    { 9, 0, 3 }, { 9, 4, 8 }, { 9, 9, 11 }, { 9, 12, 22 },
};
constexpr int NSEGS = sizeof SEGS / sizeof SEGS[0];

struct Placed { std::string w; float x, y; int index; };
std::vector<Placed> wrapWords(Font f, const std::vector<std::string>& ws, float size, float maxW,
                              float lineH, int* lines) {
    std::vector<Placed> out;
    const float space = width(f, " ", size);
    float x = 0, y = 0;
    int n = 1;
    for (int i = 0; i < (int)ws.size(); i++) {
        const float w = width(f, ws[i], size);
        if (x > 0 && x + w > maxW) { x = 0; y += lineH; n++; }
        out.push_back({ ws[i], x, y, i });
        x += w + space;
    }
    *lines = n;
    return out;
}

void captions(const Ctx& cx, double s, const Plan& p) {
    for (int i = 0; i < NSEGS; i++) {
        const CaptionSeg& g = SEGS[i];
        const double a = p.W0(g.line, g.from) - 0.15;
        double b = p.W1(g.line, g.to) + 0.6;
        if (i + 1 < NSEGS) b = std::min(b, p.W0(SEGS[i + 1].line, SEGS[i + 1].from) - 0.15);
        b = std::min(b, p.scene[g.line + 1]);
        if (s < a || s >= b) continue;
        const float k = std::min({ 1.f, (float)((s - a) * FPS / 5), (float)((b - s) * FPS / 5) });
        std::vector<std::string> ws;
        for (int w = g.from; w <= g.to; w++) ws.push_back(p.word[g.line][w]);
        const Font f = bold();
        const float size = 50, lh = size * 1.24f;
        int lines = 0;
        auto placed = wrapWords(f, ws, size, 880, lh, &lines);
        for (float tryW = 420; tryW < 880; tryW += 20) {
            int n = 0;
            auto t = wrapWords(f, ws, size, tryW, lh, &n);
            if (n == lines) { placed = t; break; }
        }
        std::vector<float> lineW(lines, 0);
        for (const Placed& w : placed) {
            const int li = (int)std::lround(w.y / lh);
            lineW[li] = std::max(lineW[li], w.x + width(f, w.w, size));
        }
        const float top = 1560 - lines * lh / 2;
        Ctx m = faded(cx, k);
        for (const Placed& w : placed) {
            const int li = (int)std::lround(w.y / lh);
            const bool spoken = s >= p.W0(g.line, g.from + w.index) - 0.05;
            const float x = W / 2.f - lineW[li] / 2 + w.x;
            text(f, m, w.w, { x, top + w.y + lh / 2 }, size, spoken ? INK : withAlpha(INK, 0.32f));
        }
        return;
    }
}

void hud(const Ctx& cx, double s, const Plan& p, int frames) {
    const float x0 = 84, x1 = W - 84;
    dr::rect(cx, x0, 120, x1 - x0, 2, GRID);
    dr::rect(cx, x0, 120, (x1 - x0) * (float)std::min(1.0, s * FPS / frames), 2, s >= p.scene[CH2] ? AMBER : TEAL);
    const bool two = p.sceneAt(s) >= CH2;
    text(mono(), cx, two ? "02 \xC2\xB7 HOW FAST?" : "01 \xC2\xB7 DOES IT RUN?", { x0, 154 }, 26, two ? AMBER : TEAL, 0, 0.5f, 1);
    char page[16];
    std::snprintf(page, sizeof page, "%02d / %02d", p.sceneAt(s) + 1, NSCENES);
    text(mono(), cx, page, { x1, 154 }, 26, DIM, 1, 0.5f);
}

// ------------------------------------------------------------------ audio
std::string existing(const std::string& file) {
    try { return media::resolve(file); } catch (...) { return ""; }
}

void sfx(Nodes& roots, double at, const char* file, float volume) {
    if (existing(file).empty()) return;
    TNode* n = addRoot(roots, std::max(0, (int)std::lround(at * FPS)), 45);
    media::AudioOptions o;
    o.volume = volume;
    media::audio(n, file, o);
}

void ensureAssets() {
    std::filesystem::create_directories(ASSETS);
    const auto p = makePlan();
    const double total = p->frames / (double)FPS;
    // D-minor pulse through the problem, opening to F major for the proof, resolving on the close.
    Song song;
    const double minor[2] = { 146.83, 116.54 }, major[2] = { 174.61, 130.81 };
    for (double t = 0; t < p->scene[10]; t += 1.0) {
        const bool open = (t >= p->scene[4] && t < p->scene[5]) || t >= p->scene[9];
        const int bar = (int)(t / 4) % 2;
        const double bass = (open ? major[bar] : minor[bar]) / 2;
        song.push_back({ t, 0.9, bass, 0.10, true });
        if ((int)t % 2 == 1) song.push_back({ t + 0.5, 0.6, bass * 4, 0.025, true });
    }
    const double c0 = p->scene[10];
    song.push_back({ c0, total - c0, 87.31, 0.06, false });
    song.push_back({ c0 + 0.3, total - c0 - 0.3, 174.61, 0.04, false });
    song.push_back({ c0 + 0.6, total - c0 - 0.6, 220.00, 0.035, false });
    song.push_back({ c0 + 0.9, total - c0 - 0.9, 261.63, 0.03, false });
    const std::vector<float> mono = renderSong(song, total, 48000);
    std::vector<float> stereo(mono.size() * 2);
    for (size_t i = 0; i < mono.size(); i++) stereo[i * 2] = stereo[i * 2 + 1] = mono[i];
    media::writeWav(SCORE, stereo, 48000);

    sub::Track tr;
    auto add = [&](int l, int from, int to) {
        std::string t;
        for (int w = from; w <= to; w++) t += (t.empty() ? "" : " ") + p->word[l][w];
        tr.cues.push_back({ p->W0(l, from), p->W1(l, to) + 0.4, t, false });
    };
    add(0, 0, (int)p->word[0].size() - 1);
    for (int i = 0; i < NSEGS - 11; i++) add(SEGS[i].line, SEGS[i].from, SEGS[i].to);   // chapter 1
    add(5, 0, 2);
    add(5, 3, 4);
    add(5, 5, (int)p->word[5].size() - 1);
    for (int i = (int)(sizeof SEGS / sizeof SEGS[0]) - 11; i < NSEGS; i++) add(SEGS[i].line, SEGS[i].from, SEGS[i].to);
    add(10, 0, 6);
    add(10, 7, (int)p->word[10].size() - 1);
    for (size_t i = 0; i + 1 < tr.cues.size(); i++)
        tr.cues[i].end = std::min(tr.cues[i].end, tr.cues[i + 1].start - 0.05);
    sub::writeSrt(SRT_PATH, tr);
}

void buildFilm(Comp& c) {
    const auto plan = makePlan();
    c.duration = plan->frames;
    c.roots.clear();
    const int frames = c.duration;
    auto board = g3d::draw(buildBoard(plan));

    addRoot(c.roots, 0, c.duration)->fn = [plan, frames, board](const Ctx& cx, int t) {
        const double s = t / (double)FPS;
        const Plan& p = *plan;
        dr::rect(cx, 0, 0, W, H, BG);
        const int sc = p.sceneAt(s);
        if (sc <= 1) board(cx, t);
        switch (sc) {
            case 0: drawHook(cx, s, p); break;
            case 1: drawMachine(cx, s, p); break;
            case 2: drawProblem(cx, s, p); break;
            case 3: drawShrink(cx, s, p); break;
            case 4: drawProof(cx, s, p); break;
            case 5: drawBridge(cx, s, p); break;
            case 6: drawArm(cx, s, p); break;
            case 7: drawMeasure(cx, s, p); break;
            case 8: drawTwist(cx, s, p); break;
            case 9: drawBatch(cx, s, p); break;
            default: drawRulerClose(cx, s, p, frames); break;
        }
        hud(cx, s, p, frames);
        captions(cx, s, p);
    };

    for (int l = 0; l < NLINES; l++) {
        if (!plan->voiced[l]) continue;
        TNode* n = addRoot(c.roots, (int)std::lround(plan->start[l] * FPS), (int)std::ceil(plan->dur[l] * FPS) + 2);
        media::AudioOptions vo;
        vo.volume = 0.85f;
        media::audio(n, std::string(VO_DIR) + LINES[l].file, vo);
    }

    if (!existing(SCORE).empty()) {
        TNode* bed = addRoot(c.roots, 0, c.duration);
        media::AudioOptions o;
        o.volume = 0.8f;
        o.volumeAt = [plan, frames](int t) {
            const double s = t / (double)FPS;
            float g = 1.f;
            for (int l = 0; l < NLINES; l++) {
                const double a = plan->start[l] - 0.2, b = plan->end(l) + 0.2;
                const float in = (float)std::clamp((s - (a - 0.2)) / 0.2, 0.0, 1.0);
                const float out = (float)std::clamp(((b + 0.2) - s) / 0.2, 0.0, 1.0);
                g = std::min(g, 1 - 0.6f * std::min(in, out));
            }
            return g * (1 - easeOut(t, frames - 20, frames));
        };
        media::audio(bed, SCORE, o);
    }

    const Plan& p = *plan;
    const char* POP = "assets/sfx/pop.wav";
    const char* CHIME = "assets/sfx/chime.wav";
    const char* THUD = "assets/sfx/thud.wav";
    const char* STAMP = "assets/audio/malaria/stamp.wav";
    const char* IMPACT = "assets/audio/malaria/impact.wav";
    sfx(c.roots, p.W0(1, 2) - 0.4, POP, 0.14f);                       // the machine card
    for (int i = 0; i < 16; i += 2) sfx(c.roots, coreAt(p, i), POP, 0.07f + 0.005f * i);   // cores rise
    sfx(c.roots, p.W0(2, 8) - 0.05, THUD, 0.30f);                     // 60.8 MB lands
    sfx(c.roots, p.W0(2, 11) - 0.2, IMPACT, 0.30f);                   // 116x too big
    sfx(c.roots, p.W0(3, 10) - 0.28, POP, 0.18f);                     // the weights snap
    sfx(c.roots, p.W0(3, 15) - 0.28, THUD, 0.26f);                    // the shrink
    sfx(c.roots, p.W0(4, 13) + 0.05, STAMP, 0.38f);                   // identical
    // chapter 2: a tick per gridline the dashed line passes (10 ... 60)
    for (int g = 1; g <= 6; g++) {
        const double u = 1 - std::cbrt(1 - g / 6.0 + 1e-9);         // invert the ease-out
        sfx(c.roots, riseFrom(p) + (riseTo(p) - riseFrom(p)) * u, POP, 0.10f + 0.02f * g);
    }
    sfx(c.roots, p.W0(7, 4) - 0.05, THUD, 0.25f);                     // 49.4 lands short
    sfx(c.roots, p.W0(7, 11) - 0.3, POP, 0.22f);                      // the gauge flips
    sfx(c.roots, p.W1(7, 13) + 0.2, STAMP, 0.38f);                    // PROJECTION HELD
    sfx(c.roots, p.W0(8, 4) - 0.27, STAMP, 0.38f);                    // ONE MATRIX PASS
    sfx(c.roots, p.W0(8, 9) - 0.28, IMPACT, 0.40f);                   // the drop
    sfx(c.roots, p.W0(9, 8) - 0.05, POP, 0.24f);                      // 40
    sfx(c.roots, p.W0(9, 11) - 0.05, CHIME, 0.30f);                   // 80
    sfx(c.roots, p.W1(10, 11) + 0.6, CHIME, 0.22f);                   // the source lands
}

} // namespace

void registerParallella(Registry& out) {
    Comp c;
    c.name = "parallella";
    c.w = W; c.h = H; c.fps = FPS;
    c.duration = makePlan()->frames;
    c.background = BG;
    c.assets = [](const Comp&, const Json&) { ensureAssets(); };
    c.build = [](Comp& c, const Json&) { buildFilm(c); };
    out.push_back(std::move(c));
}

} // namespace rmtn
