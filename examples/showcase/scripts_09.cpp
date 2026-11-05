// scripts_09.cpp -- "scripts-09": a 60 s landscape promo for raymotion 0.9's scripting.
// Brief: docs/scripts-09-brief.md. Text only, no voice.
//
// One AngelScript file is written on screen and run after every save. The editor's text is a
// pure function of time (a list of typing, paste and erase operations); at each save that text
// is written to out/scripts-09-assets/save-N/film.as, loaded with script::registerFile and its
// timeline grafted into the preview pane, frozen at frame 60 as `raymotion_script still film
// --at 60` would render it. So the code on screen and the code that renders are the same
// string. The two guard-rail errors are real too: the global variable fails to compile (the
// message is caught while building), the endless loop is run inside the frame and its abort
// is caught and printed. The save-to-frame times on the chips were measured with the CLI on
// those same files (brief section 5).
//
//   raymotion render scripts-09
#include "rmtn/raymotion.hpp"
#include "rmtn/script.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace rmtn {
namespace {

constexpr int W = 1920, H = 1080, FPS = 30;
constexpr double LENGTH = 60.0;
constexpr const char* ASSETS = "out/scripts-09-assets";
constexpr const char* SCORE = "out/scripts-09-assets/score.wav";
constexpr const char* UI_WAV = "out/scripts-09-assets/ui.wav";
constexpr const char* SRT_PATH = "out/scripts-09.srt";
constexpr const char* GLYPHS = "→·";

const Color BG = mkColor(0x0d1017), PANEL = mkColor(0x151a24), HEADER = mkColor(0x1b2130),
            EDGE = mkColor(0x262e3e), INK = mkColor(0xe9edf5), DIM = mkColor(0x7c8597),
            FAINT = mkColor(0x454e60), TEAL = mkColor(0x47dad8), AMBER = mkColor(0xf0ad52),
            CORAL = mkColor(0xff6b5a), SCRIPT_BG = mkColor(0x04040a), UNSEEN = { 0, 0, 0, 0 };

// chapter starts (s)
constexpr double C1 = 0, C2 = 8, C3 = 17, C4 = 29, C5 = 40, C6 = 50;
constexpr double SLIDE = 0.7;   // the full-screen chapters slide in and out over this

// ------------------------------------------------------------------ layout (brief section 6)
constexpr float ED_X = 72, ED_Y = 88, ED_W = 988, ED_H = 824, ED_HEAD = 48;
constexpr float PV_X = 1108, PV_Y = 88, PV_W = 740, PV_H = PV_W * 9 / 16;
constexpr float TM_X = 1108, TM_Y = 532, TM_W = 740, TM_H = ED_Y + ED_H - 532;
constexpr float CODE_SIZE = 25, CODE_LH = 1.26f;
constexpr float TERM_SIZE = 20, TERM_LH = 29;
constexpr int SCRIPT_W = 1280, FREEZE = 60;

Font bold() { return fonts::loadFor({ "Inter-Bold.ttf" }, GLYPHS, 96); }
Font regular() { return fonts::loadFor({ "Inter-Regular.ttf" }, GLYPHS, 96); }
Font mono() { return fonts::loadFor({ "JetBrainsMono-Regular.ttf" }, GLYPHS, 64); }

void text(Font f, const Ctx& cx, const std::string& s, Vector2 at, float size, Color col,
          float ax = 0, float ay = 0) {
    dr::text(f, cx, s.c_str(), at, size, 0, col, ax, ay);
}
float width(Font f, const std::string& s, float size) { return MeasureTextEx(f, s.c_str(), size, 0).x; }
float clamp01(double v) { return (float)std::clamp(v, 0.0, 1.0); }
float easeOut(double s, double a, double b) { const float u = clamp01((s - a) / (b - a)); return 1 - (1 - u) * (1 - u) * (1 - u); }
float fadeIn(double s, double a, double len = 0.4) { return easeOut(s, a, a + len); }
int frameOf(double s) { return (int)std::lround(s * FPS); }

// ------------------------------------------------------------------ the file being written
// Lines 1-4 exist when the film opens; everything else is typed, pasted or erased.
const std::string START =
    "void configure(Composition &inout c) {\n"
    "  c.name = \"film\"; c.w = 1280; c.h = 720;\n"
    "  c.duration = 300; scene(0, 300, \"draw\");\n"
    "}\n"
    "\n";

const std::string HELLO =
    "void draw(int t) {\n"
    "  float x = 120 + 4 * t;\n"
    "  draw::text(\"hello\", x, 260, 180, 0xffffff);\n"
    "}\n";
const std::string HELLO_BODY =
    "  float x = 120 + 4 * t;\n"
    "  draw::text(\"hello\", x, 260, 180, 0xffffff);\n";
const std::string STAGE =
    "  g3d::Scene3D@ world = g3d::scene();\n"
    "  world.camera(2.2f, 1.3f, 2.2f, 0, 1.1f, 0);\n"
    "  world.addDirectional(3, 5, 3, 0xffffff, 1, true);\n"
    "  g3d::Material3D@ ground = g3d::material();\n"
    "  ground.color(0x2a3140);\n"
    "  world.add(g3d::plane(8, 8), ground);\n"
    "  world.render(t, 0, 0, 1280, 720);\n";
const std::string STAGE_TOP = "  world.add(g3d::plane(8, 8), ground);\n";
const std::string BODY =
    "  string body = \"\"\"{\"clips\":[{\"name\":\"run\"}]}\"\"\";\n"
    "  world.addCharacter(body);\n";
const std::string CLIP_AT = "{\"name\":\"";
const std::string BODY_CHEER =
    "  string body = \"\"\"{\"clips\":[{\"name\":\"cheer\"}]}\"\"\";\n"
    "  world.addCharacter(body);\n";
const std::string CLOTH =
    "  PhysicsCloth@ flag = physics::cloth(18, 12, 0.1f,\n"
    "      -0.5f, 2, -0.9f,  0, 0, 1,  0, -1, 0);\n"
    "  flag.pinRow(0);\n";
const std::string WIND =
    "  PhysicsWind@ wind = physics::wind();\n"
    "  wind.strength(3);\n"
    "  wind.gust(0.6f);\n"
    "  flag.wind(wind);\n"
    "  world.addCloth(flag);\n";
const std::string GLOBAL = "int frames = 0;\n";
const std::string LOOP = "  while (true) {}\n";

struct Op {
    enum Kind { Type, Paste, Erase } kind;
    double t;
    std::string after;   // inserts go right after this text (empty: the end); erases search from it
    std::string text;
    double cps = 34;     // typing speed, characters a second
    double dur() const { return kind == Type ? text.size() / cps : kind == Erase ? 0.45 : 0.0; }
    double end() const { return t + dur(); }
};
constexpr double SELECT = 0.45;   // an erase shows its selection this long, then the text goes

const std::vector<Op>& ops() {
    static const std::vector<Op> list = {
        // chapter 1: a text file
        { Op::Type, 0.8, "", HELLO, 34 },
        // chapter 3: a body
        { Op::Erase, 17.9, "", HELLO_BODY },
        { Op::Paste, 18.6, "void draw(int t) {\n", STAGE },
        { Op::Type, 19.4, STAGE_TOP, BODY, 32 },
        { Op::Erase, 24.1, CLIP_AT, "run" },
        { Op::Type, 24.6, CLIP_AT, "cheer", 14 },
        // chapter 4: physics
        { Op::Erase, 29.4, "", BODY_CHEER },
        { Op::Paste, 30.1, STAGE_TOP, CLOTH },
        { Op::Type, 30.7, "flag.pinRow(0);\n", WIND, 38 },
        { Op::Erase, 35.4, "wind.strength(", "3" },
        { Op::Type, 35.9, "wind.strength(", "12", 10 },
        // chapter 5: guard rails
        { Op::Type, 40.5, "}\n", GLOBAL, 26 },
        { Op::Erase, 42.9, "", GLOBAL },
        { Op::Type, 43.7, "void draw(int t) {\n", LOOP, 26 },
        { Op::Erase, 46.1, "", LOOP },
    };
    return list;
}

struct Doc {
    std::string text;
    int caret = 0;
    int hiFrom = 0, hiTo = 0;    // the newest typed or pasted text
    float hi = 0;                //   its highlight, fading after the operation
    int selFrom = 0, selTo = 0;  // a selection about to be erased
    bool busy = false;           // typing right now (the caret does not blink)
};

size_t anchorEnd(const std::string& d, const std::string& after) {
    if (after.empty()) return d.size();
    const size_t p = d.find(after);
    if (p == std::string::npos) throw std::runtime_error("scripts-09: edit anchor not found: " + after);
    return p + after.size();
}

Doc docAt(double s) {
    Doc d;
    d.text = START;
    d.caret = (int)d.text.size();
    for (const Op& op : ops()) {
        if (s < op.t) break;
        d.hi = 0;
        d.selFrom = d.selTo = 0;
        if (op.kind == Op::Erase) {
            const size_t from = op.after.empty() ? 0 : anchorEnd(d.text, op.after) - op.after.size();
            const size_t p = d.text.find(op.text, from);
            if (p == std::string::npos) throw std::runtime_error("scripts-09: erase target not found: " + op.text);
            if (s < op.t + SELECT) {
                d.selFrom = (int)p;
                d.selTo = (int)(p + op.text.size());
                d.caret = d.selTo;
            } else {
                d.text.erase(p, op.text.size());
                d.caret = (int)p;
            }
            continue;
        }
        const size_t at = anchorEnd(d.text, op.after);
        const size_t n = op.kind == Op::Paste ? op.text.size()
                       : std::min(op.text.size(), (size_t)std::floor((s - op.t) * op.cps));
        std::string typed = op.text.substr(0, n);
        // typing whole lines: the new line opens first, as after pressing Enter
        if (n < op.text.size() && op.text.back() == '\n' && (typed.empty() || typed.back() != '\n')) typed += '\n';
        d.text.insert(at, typed);
        d.caret = (int)(at + n);
        d.hiFrom = (int)at;
        d.hiTo = (int)(at + n);
        d.busy = s < op.end();
        d.hi = 1 - clamp01((s - op.end() - 0.8) / 0.8);
    }
    return d;
}

// ------------------------------------------------------------------ saves and runs
struct Save {
    double t;        // the save (s)
    double latency;  // measured: save to frame (or error), brief section 5
    enum Result { Frame, CompileError, RunError } result;
};
// latency: `raymotion_script still film --at 60 --script save-N/film.as`, the median of 7
// runs after a warm-up, measured 2026-11-05 on the author's PC (brief section 5). Runs vary
// by ~10% between sessions, so the chips round to 0.1 s; the film's gap uses the median.
// Re-measure after any change to the scripts; never set by eye.
const Save SAVES[] = {
    { 4.3, 0.53, Save::Frame },          // 0 hello
    { 22.6, 0.58, Save::Frame },         // 1 run
    { 25.2, 0.57, Save::Frame },         // 2 cheer
    { 34.6, 0.56, Save::Frame },         // 3 cloth, wind 3
    { 36.6, 0.55, Save::Frame },         // 4 wind 12
    { 41.4, 0.02, Save::CompileError },  // 5 a global variable: rejected before any GL
    { 44.6, 0.72, Save::RunError },      // 6 an endless loop: stopped by the statement budget
    { 46.9, 0.56, Save::Frame },         // 7 the loop removed: the cloth again
};
constexpr int NSAVES = sizeof SAVES / sizeof SAVES[0];
constexpr double SPLIT = 38.0, SPLIT_END = C5;   // run 1 / run 2 of save 4
// two `still` runs of save 4 compared pixel by pixel (1280x720), brief section 5
constexpr const char* DIFF_TEXT = "pixels that differ: 0 of 921,600";
const char* const STILL_CMD = "raymotion_script still film --at 60 --script film.as";
const char* const STILL_OUT = "wrote out/film_still_0060.png";
const char* const LOOP_EXPECTED = "raymotion: film.as: script execution aborted (statement budget exceeded)";

std::string savePath(int k) { return std::string(ASSETS) + "/save-" + std::to_string(k) + "/film.as"; }
double resultAt(int k) { return SAVES[k].t + SAVES[k].latency; }

void writeScripts() {
    for (int k = 0; k < NSAVES; k++) {
        for (const Op& op : ops())
            if (SAVES[k].t >= op.t && SAVES[k].t < op.end() + 0.2)
                throw std::runtime_error("scripts-09: save " + std::to_string(k) + " lands inside an edit");
        std::filesystem::create_directories(std::filesystem::path(savePath(k)).parent_path());
        std::ofstream(savePath(k), std::ios::binary) << docAt(SAVES[k].t).text;
    }
}

// "D:\...\save-5\film.as:5:1: ..." -> "raymotion: film.as:5:1: ...", as the CLI prints it
std::string cliMessage(const std::string& what) {
    const size_t p = what.find("film.as");
    return "raymotion: " + (p == std::string::npos ? what : what.substr(p));
}

// ------------------------------------------------------------------ the terminal
struct TermLine { double at; std::string text; Color col; int save; bool command; };

std::vector<std::string> wrap(const std::string& s, size_t cols) {
    std::vector<std::string> out;
    std::string rest = s;
    while (rest.size() > cols) {
        size_t cut = rest.rfind(' ', cols);
        if (cut == std::string::npos || cut == 0) cut = cols;
        out.push_back(rest.substr(0, cut));
        rest = rest.substr(cut + (rest[cut] == ' ' ? 1 : 0));
    }
    out.push_back(rest);
    return out;
}

struct Film {
    std::vector<TermLine> term;   // in time order
    std::string compileError;     // caught from save 5
};

// ------------------------------------------------------------------ editor
code::Style codeStyle() {
    code::Style st;
    st.size = CODE_SIZE;
    st.lineHeight = CODE_LH;
    st.card = false;
    st.diffGutter = false;
    st.padding = { 26, 20 };
    code::Theme& th = st.theme;
    th.colors[(int)code::Kind::Text] = INK;
    th.colors[(int)code::Kind::Keyword] = mkColor(0xb39df3);
    th.colors[(int)code::Kind::Type] = mkColor(0x8fb8ff);
    th.colors[(int)code::Kind::Function] = mkColor(0xd7e3ff);
    th.colors[(int)code::Kind::String] = mkColor(0x9fd08c);
    th.colors[(int)code::Kind::Number] = mkColor(0xd9c7a6);
    th.colors[(int)code::Kind::Comment] = DIM;
    th.colors[(int)code::Kind::Operator] = mkColor(0xaab3c5);
    th.colors[(int)code::Kind::Punctuation] = mkColor(0x9aa3b5);
    th.colors[(int)code::Kind::Property] = mkColor(0xd7e3ff);
    th.colors[(int)code::Kind::Constant] = mkColor(0xd9c7a6);
    th.lineNumber = FAINT;
    return st;
}

// AngelScript's """heredoc""" is one string; the C-like lexer splits it, so recolour it.
void heredocs(code::Block& b) {
    for (auto& line : b.lines) {
        const size_t a = line.text.find("\"\"\"");
        if (a == std::string::npos) continue;
        const size_t z = line.text.find("\"\"\"", a + 3);
        if (z == std::string::npos) continue;
        for (auto& tok : line.tokens)
            if (tok.col >= (int)a && tok.col < (int)z + 3) tok.kind = code::Kind::String;
    }
}

// (line 1-based, column) of a byte offset; the file is ASCII
std::pair<int, int> lineCol(const std::string& s, int at) {
    int line = 1, col = 0;
    for (int i = 0; i < at && i < (int)s.size(); i++) {
        if (s[i] == '\n') { line++; col = 0; } else col++;
    }
    return { line, col };
}

void markRange(const Ctx& cx, const code::Block& b, const code::Style& st, Vector2 at,
               const std::string& s, int from, int to, Color col) {
    if (to <= from) return;
    const float cw = code::cellWidth(st), lh = st.size * st.lineHeight;
    auto [l0, c0] = lineCol(s, from);
    auto [l1, c1] = lineCol(s, to);
    for (int l = l0; l <= l1; l++) {
        const int a = l == l0 ? c0 : 0;
        int z = l == l1 ? c1 : a;
        if (l != l1) {   // to the end of the line, plus the newline
            size_t ls = 0;
            for (int k = 1; k < l; k++) ls = s.find('\n', ls) + 1;
            const size_t le = s.find('\n', ls);
            z = (int)((le == std::string::npos ? s.size() : le) - ls) + 1;
        }
        if (z <= a) continue;
        const Vector2 p = code::cell(b, st, at, l, a);
        dr::rect(cx, p.x - 2, p.y + 1, (z - a) * cw + 4, lh - 2, col);
    }
}

bool dirty(double s) {   // edited since the last save
    double lastEdit = -1, lastSave = -1;
    for (const Op& op : ops()) if (op.t <= s) lastEdit = op.t;
    for (const Save& v : SAVES) if (v.t <= s) lastSave = v.t;
    return lastEdit > lastSave;
}

void drawEditor(const Ctx& cx, double s, int t) {
    dr::roundRect(cx, ED_X - 1.5f, ED_Y - 1.5f, ED_W + 3, ED_H + 3, 18, EDGE);
    dr::roundRect(cx, ED_X, ED_Y, ED_W, ED_H, 18, PANEL);
    dr::roundRect(cx, ED_X, ED_Y, ED_W, ED_HEAD, 18, HEADER);
    dr::rect(cx, ED_X, ED_Y + ED_HEAD - 18, ED_W, 18, HEADER);
    dr::rect(cx, ED_X, ED_Y + ED_HEAD, ED_W, 1.5f, EDGE);
    // the tab: the file name, and a dot while there are unsaved edits
    dr::rect(cx, ED_X + 18, ED_Y + ED_HEAD - 3, 150, 3, TEAL);
    text(mono(), cx, "film.as", { ED_X + 30, ED_Y + ED_HEAD / 2 }, 22, INK, 0, 0.5f);
    if (dirty(s)) dr::circle(cx, { ED_X + 148, ED_Y + ED_HEAD / 2 }, 6, TEAL);

    const Doc d = docAt(s);
    code::Style st = codeStyle();
    code::Block b = code::Block::parse(d.text, "cpp");
    heredocs(b);
    const Vector2 at = { ED_X, ED_Y + ED_HEAD };
    if (d.selTo > d.selFrom) markRange(cx, b, st, at, d.text, d.selFrom, d.selTo, withAlpha(TEAL, 0.34f));
    if (d.hi > 0) markRange(cx, b, st, at, d.text, d.hiFrom, d.hiTo, withAlpha(TEAL, 0.16f * d.hi));
    code::draw(cx, b, at, st);
    // the parser drops trailing empty lines; an editor still numbers them up to the caret
    const int caretLine = lineCol(d.text, d.caret).first, lastLine = lineCol(d.text, (int)d.text.size()).first;
    for (int l = (int)b.lines.size() + 1; l <= std::max(caretLine, lastLine - 1); l++) {
        const std::string num = std::to_string(l);
        const Vector2 p = code::cell(b, st, at, l, 0);
        text(code::font(), cx, num, { p.x - (1 + (float)num.size()) * code::cellWidth(st), p.y + (st.size * st.lineHeight - st.size) / 2 },
             st.size, st.theme.lineNumber);
    }
    // the four configure lines stay dim: written once, never edited
    const Vector2 l1 = code::cell(b, st, at, 1, 0), l5 = code::cell(b, st, at, 5, 0);
    dr::rect(cx, ED_X + 2, l1.y, ED_W - 4, l5.y - l1.y, withAlpha(PANEL, 0.5f));

    // the caret: solid while typing, otherwise blinking
    if (d.busy || (t / 16) % 2 == 0) {
        auto [l, c] = lineCol(d.text, d.caret);
        const Vector2 p = code::cell(b, st, at, l, c);
        dr::rect(cx, p.x - 1, p.y + 3, 3, st.size * st.lineHeight - 6, TEAL);
    }
}

// ------------------------------------------------------------------ terminal
size_t termColumns() {
    code::Style st;
    st.size = TERM_SIZE;
    return (size_t)((TM_W - 40) / code::cellWidth(st));
}

double chapterStart(double s) {
    return s >= C5 ? C5 : s >= C4 ? C4 : s >= C3 ? C3 : 0;
}

// The terminal's visible rows at `s`: this chapter's commands and outputs, newest at the bottom.
struct Row { std::string text; Color col; int save; bool command; float y; };
std::vector<Row> termRows(const Film& f, double s) {
    std::vector<Row> rows;
    const double since = chapterStart(s) - 0.01;
    for (const TermLine& l : f.term) {
        if (l.at > s || l.at < since) continue;
        std::string shown = l.text;
        if (l.command) {   // typed quickly
            const size_t n = std::min(shown.size(), (size_t)((s - l.at) * 220));
            shown = "$ " + shown.substr(0, n);
        }
        for (const auto& w : wrap(shown, termColumns())) rows.push_back({ w, l.col, l.save, l.command, 0 });
    }
    const int fit = (int)((TM_H - 64) / TERM_LH);
    rows.erase(rows.begin(), rows.begin() + std::max(0, (int)rows.size() - fit));
    for (size_t i = 0; i < rows.size(); i++) rows[i].y = TM_Y + 52 + i * TERM_LH;
    return rows;
}

void drawTerminal(const Ctx& cx, const Film& f, double s) {
    dr::roundRect(cx, TM_X - 1.5f, TM_Y - 1.5f, TM_W + 3, TM_H + 3, 15, withAlpha(EDGE, 0.6f));
    dr::roundRect(cx, TM_X, TM_Y, TM_W, TM_H, 14, mkColor(0x090c12));
    text(regular(), cx, "terminal", { TM_X + 20, TM_Y + 22 }, 19, DIM, 0, 0.5f);
    for (const Row& r : termRows(f, s))
        if (r.col.a) text(mono(), cx, r.text, { TM_X + 20, r.y }, TERM_SIZE, r.col);
}

// ------------------------------------------------------------------ the preview pane
// the scripts' own background (the grafted timeline does not draw it)
void paneFrame(const Ctx& cx) {
    dr::roundRect(cx, PV_X - 1.5f, PV_Y - 1.5f, PV_W + 3, PV_H + 3, 6, EDGE);
    dr::rect(cx, PV_X, PV_Y, PV_W, PV_H, SCRIPT_BG);
}

// ------------------------------------------------------------------ chapter 2: the ZIP
struct ZipRow { const char* name; const char* size; const char* note; };
const ZipRow ZIP[] = {
    { "raymotion_script.exe", "8.3 MB", "runs .as files" },
    { "raymotion.exe", "11.9 MB", "" },
    { "scripts/hello.as", "2.2 KB", "a first script" },
    { "assets/", "275 files", "fonts, sounds, models" },
    { "README.txt  LICENSE.txt  NOTICE.txt", "", "" },
};
// README.txt in the ZIP: ffmpeg on PATH, for MP4 output (a still needs nothing more)
const char* const NEEDS[] = { "a C++ compiler", "CMake", "a build of raymotion", "a text editor", "ffmpeg, for MP4 files" };
constexpr int NNEEDS = sizeof NEEDS / sizeof NEEDS[0], STRUCK = 3;

void drawZip(const Ctx& cx, double s) {
    dr::rect(cx, 0, 0, W, H, BG);
    const double a = C2 - SLIDE;   // the content arrives with the slide
    // the archive
    const float x0 = 130, y0 = 300;
    text(mono(), cx, "raymotion-v0.9.1-windows-x64.zip", { x0, y0 }, 34, withAlpha(INK, fadeIn(s, a)));
    text(regular(), cx, "125 MB · 286 files · the v0.9.1 release", { x0, y0 + 54 }, 27, withAlpha(DIM, fadeIn(s, a + 0.2)));
    for (int i = 0; i < 5; i++) {
        const double at = a + 0.6 + 0.32 * i;
        const float u = fadeIn(s, at, 0.3f);
        if (u <= 0) continue;
        const float y = y0 + 150 + i * 84 + 10 * (1 - u);
        const bool key = i == 0;
        if (key) dr::roundRect(cx, x0 - 20, y - 16, 960, 68, 10, withAlpha(HEADER, u));
        text(mono(), cx, ZIP[i].name, { x0, y + 18 }, 31, withAlpha(key ? INK : mkColor(0xb7bfcf), u), 0, 0.5f);
        text(mono(), cx, ZIP[i].size, { x0 + 610, y + 18 }, 28, withAlpha(DIM, u), 1, 0.5f);
        if (*ZIP[i].note) text(regular(), cx, ZIP[i].note, { x0 + 650, y + 18 }, 27, withAlpha(DIM, u), 0, 0.5f);
    }
    // what a video needs
    const float nx = 1220, ny = 300;
    const double n0 = 11.2;
    text(bold(), cx, "To make a video you need", { nx, ny }, 38, withAlpha(INK, fadeIn(s, n0)));
    for (int i = 0; i < NNEEDS; i++) {
        const float u = fadeIn(s, n0 + 0.25 + 0.12 * i, 0.3f);
        const float y = ny + 168 + i * 84;
        const bool keep = i >= STRUCK;
        Color col = keep ? INK : mkColor(0xb7bfcf);
        const double strikeAt = 12.4 + 0.8 * i;
        const float k = keep ? 0 : easeOut(s, strikeAt, strikeAt + 0.25);
        if (k > 0) col = withAlpha(DIM, 1.f);
        text(regular(), cx, NEEDS[i], { nx + 50, y }, 34, withAlpha(col, u), 0, 0.5f);
        const float w = width(regular(), NEEDS[i], 34);
        if (k > 0) dr::rect(cx, nx + 44, y - 2, (w + 12) * k, 4, withAlpha(INK, u));
        if (keep) {
            const float c = fadeIn(s, 14.6 + 0.2 * (i - STRUCK), 0.3f);
            const Color tick = withAlpha(TEAL, c * u);
            dr::strokeSeg(cx, { nx + 8, y }, { nx + 18, y + 10 }, 5, tick);
            dr::strokeSeg(cx, { nx + 18, y + 10 }, { nx + 36, y - 12 }, 5, tick);
        } else {
            text(bold(), cx, "·", { nx + 14, y }, 34, withAlpha(DIM, u * (1 - k)), 0, 0.5f);
        }
    }
}

// ------------------------------------------------------------------ chapter 6: the end card
void drawEnd(const Ctx& cx, double s, int t) {
    dr::rect(cx, 0, 0, W, H, BG);
    const double a = C6 - 0.3;   // the logo draws on as the card lands
    icons::logo(cx, { W / 2.f, 330 }, 250, t, easeOut(s, a, a + 0.9), true);
    text(bold(), cx, "raymotion 0.9.1", { W / 2.f, 540 }, 84, withAlpha(INK, fadeIn(s, 52.0)), 0.5f, 0.5f);
    text(regular(), cx, "bodies, physics and scripts", { W / 2.f, 612 }, 34, withAlpha(DIM, fadeIn(s, 52.4)), 0.5f, 0.5f);
    const std::string cmd = "raymotion_script preview hello-script --script scripts/hello.as";
    const float u = fadeIn(s, 53.2, 0.3f);
    if (u > 0) {
        const size_t n = std::min(cmd.size(), (size_t)std::max(0.0, (s - 53.4) * 52));
        const float w = width(mono(), cmd, 30) + 80;
        dr::roundRect(cx, W / 2.f - w / 2, 690, w, 70, 12, withAlpha(HEADER, u));
        text(mono(), cx, cmd.substr(0, n), { W / 2.f - w / 2 + 40, 725 }, 30, withAlpha(INK, u), 0, 0.5f);
    }
    text(regular(), cx, "github.com/pebaryan/raymotion/releases", { W / 2.f, 830 }, 34, withAlpha(TEAL, fadeIn(s, 55.4)), 0.5f, 0.5f);
}

// ------------------------------------------------------------------ captions (brief section 4)
struct Caption { double from, to; const char* text; };
const Caption CAPTIONS[] = {
    { 0.3, 7.6, "A video is a text file now." },
    { 14.9, 16.6, "Unzip. Write. Render. No compiler." },
    { 26.4, 28.6, "Characters, lights, cameras: from the script." },
    { 38.6, 39.7, "Same frame, every run." },
    { 45.8, 49.5, "No globals. No runaway loops." },
};

void drawCaption(const Ctx& cx, double s) {
    for (const Caption& c : CAPTIONS) {
        if (s < c.from || s > c.to) continue;
        const float u = fadeIn(s, c.from, 0.35) * (1 - clamp01((s - (c.to - 0.3)) / 0.3));
        text(bold(), cx, c.text, { W / 2.f, 996 }, 46, withAlpha(INK, u), 0.5f, 0.5f);
    }
    if (s > 46.7 && s < 49.5) {
        const float u = fadeIn(s, 46.7) * (1 - clamp01((s - 49.2) / 0.3));
        text(regular(), cx, "Scripts are trusted code: run only scripts you would run as a program.",
             { W / 2.f, 1046 }, 22, withAlpha(DIM, u), 0.5f, 0.5f);
    }
}

// ------------------------------------------------------------------ chip and overlays
void drawChip(const Ctx& cx, double s) {
    for (int k = 0; k < NSAVES; k++) {
        if (SAVES[k].result != Save::Frame) continue;
        const double a = resultAt(k);
        if (s < a || s > a + 2.4 || (a < SPLIT && s >= SPLIT)) continue;
        const float u = fadeIn(s, a, 0.2) * (1 - clamp01((s - (a + 2.1)) / 0.3));
        char buf[64];
        std::snprintf(buf, sizeof buf, "save → frame  %.1f s", SAVES[k].latency);
        const float w = width(mono(), buf, 24) + 36;
        const float x = PV_X + 16, y = PV_Y + PV_H - 60 + 8 * (1 - u);
        dr::roundRect(cx, x, y, w, 44, 22, withAlpha(mkColor(0x2a2112), 0.92f * u));
        text(mono(), cx, buf, { x + 18, y + 22 }, 24, withAlpha(AMBER, u), 0, 0.5f);
    }
}

// the preview while a save failed: the last good frame, dimmed
void drawStale(const Ctx& cx, double s) {
    for (int k = 0; k < NSAVES; k++) {
        if (SAVES[k].result == Save::Frame) continue;
        const double a = resultAt(k), z = k + 1 < NSAVES ? SAVES[k + 1].t : C6;
        const double end = SAVES[k].result == Save::CompileError ? SAVES[k + 1].t : z;
        if (s < a || s >= end) continue;
        dr::rect(cx, PV_X, PV_Y, PV_W, PV_H, withAlpha(BG, 0.62f));
        text(regular(), cx, "no new frame", { PV_X + PV_W / 2, PV_Y + PV_H / 2 }, 28, DIM, 0.5f, 0.5f);
    }
}

void drawSplitLabels(const Ctx& cx, double s) {
    if (s < SPLIT || s >= SPLIT_END) return;
    const float u = fadeIn(s, SPLIT, 0.3);
    // the two runs are 16:9 halves in the middle of the pane: paper above, between, below
    const float hw = (PV_W - 12) / 2, hh = hw * 9 / 16, y0 = PV_Y + (PV_H - hh) / 2;
    const Color paper = withAlpha(BG, u);
    dr::rect(cx, PV_X, PV_Y, PV_W, y0 - PV_Y, paper);
    dr::rect(cx, PV_X, y0 + hh, PV_W, PV_Y + PV_H - (y0 + hh), paper);
    dr::rect(cx, PV_X + hw, y0, 12, hh, paper);
    for (int i = 0; i < 2; i++) {
        const float x = PV_X + i * (hw + 12);
        dr::polyStroke(cx, std::vector<Vector2>{ { x, y0 }, { x + hw, y0 }, { x + hw, y0 + hh }, { x, y0 + hh } }.data(), 4, true, 1.5f, withAlpha(EDGE, u));
        text(mono(), cx, i ? "run 2" : "run 1", { x, y0 - 24 }, 22, withAlpha(INK, u), 0, 0.5f);
    }
    const float r = fadeIn(s, 38.6, 0.3);
    if (r > 0) {
        const char* buf = DIFF_TEXT;
        const float w = width(mono(), buf, 24) + 36;
        const float x = PV_X + PV_W / 2 - w / 2, y = y0 + hh + 18;
        dr::roundRect(cx, x, y, w, 44, 22, withAlpha(mkColor(0x2a2112), 0.92f * r));
        text(mono(), cx, buf, { x + 18, y + 22 }, 24, withAlpha(AMBER, r), 0, 0.5f);
    }
}

// slides between the editor and the full-screen chapters: 1 = covered
float smooth(double s, double a, double b) { const float u = clamp01((s - a) / (b - a)); return u * u * (3 - 2 * u); }
float cover(double s) {
    if (s < C2 - SLIDE) return 0;
    if (s < C2) return smooth(s, C2 - SLIDE, C2);
    if (s < C3 - SLIDE) return 1;
    if (s < C3) return 1 - smooth(s, C3 - SLIDE, C3);
    if (s < C6 - SLIDE) return 0;
    if (s < C6) return smooth(s, C6 - SLIDE, C6);
    return 1;
}

void drawProgress(const Ctx& cx, double s) {
    dr::rect(cx, 0, 0, W * (float)(s / LENGTH), 3, withAlpha(AMBER, 0.55f));
}

// ------------------------------------------------------------------ building
std::shared_ptr<Film> makeFilm() {
    auto f = std::make_shared<Film>();
    for (int k = 0; k < NSAVES; k++) {
        f->term.push_back({ SAVES[k].t, STILL_CMD, INK, k, true });
        if (SAVES[k].result == Save::Frame) f->term.push_back({ resultAt(k), STILL_OUT, DIM, k, false });
        // the abort is printed by the run itself; these invisible rows hold its place
        else if (SAVES[k].result == Save::RunError) f->term.push_back({ resultAt(k), LOOP_EXPECTED, UNSEEN, k, false });
        else f->term.push_back({ resultAt(k), "", CORAL, k, false });   // filled from the compile
    }
    return f;
}

// Loads one saved file and grafts its timeline under `pane`, which shows frame 60.
std::unique_ptr<TNode> loadSave(int k, int from, int to, Vector2 at, float scale) {
    Registry reg;
    script::registerFile(reg, std::filesystem::absolute(savePath(k)).string());
    Comp& inner = reg.front();
    if (inner.build) inner.build(inner, inner.defaultProps);
    auto pane = std::make_unique<TNode>();
    pane->start = from;
    pane->dur = std::max(1, to - from);
    pane->freezeAt = FREEZE;
    pane->props.pos = at;
    pane->props.scale = scale;
    for (auto& r : inner.roots) pane->kids.push_back(std::move(r));
    return pane;
}

// `list` builds without the asset step, so in a fresh directory the saves are not written
// and the film builds without its script panes; a drawing run writes them first (c.assets)
bool scriptsWritten() {
    for (int k = 0; k < NSAVES; k++)
        if (!std::filesystem::exists(savePath(k))) return false;
    return true;
}

void buildFilm(Comp& c, bool thumb) {
    const bool saved = scriptsWritten();
    c.roots.clear();
    auto film = makeFilm();
    const float scale = PV_W / SCRIPT_W;

    // the compile error, caught from the real compiler
    for (int k = 0; k < NSAVES; k++) {
        if (!saved || SAVES[k].result != Save::CompileError) continue;
        try {
            Registry reg;
            script::registerFile(reg, std::filesystem::absolute(savePath(k)).string());
            film->compileError = "(compiled: expected an error)";
        } catch (const std::exception& e) {
            film->compileError = cliMessage(e.what());
        }
        for (TermLine& l : film->term) if (l.save == k && !l.command) l.text = film->compileError;
    }

    const int total = c.duration;
    // under everything: the editor, terminal and preview frame
    addRoot(c.roots, 0, total)->fn = [film](const Ctx& cx, int t) {
        const double s = t / (double)FPS;
        dr::rect(cx, 0, 0, W, H, BG);
        if (s >= C6) return;
        drawEditor(cx, s, t);
        drawTerminal(cx, *film, s);
        paneFrame(cx);
    };

    // the preview: each good save's frame, from its result to the next result
    std::vector<int> good;
    for (int k = 0; k < NSAVES; k++) if (saved && SAVES[k].result == Save::Frame) good.push_back(k);
    for (size_t i = 0; i < good.size(); i++) {
        const int k = good[i];
        const int from = frameOf(resultAt(k));
        const int to = i + 1 < good.size() ? frameOf(resultAt(good[i + 1])) : frameOf(C6);
        if (k == 4) {   // around the split: before, the two runs, after
            c.roots.push_back(loadSave(k, from, frameOf(SPLIT), { PV_X, PV_Y }, scale));
            const float hs = (PV_W - 12) / 2 / SCRIPT_W, hh = PV_H * hs * SCRIPT_W / PV_W;
            c.roots.push_back(loadSave(k, frameOf(SPLIT), frameOf(SPLIT_END), { PV_X, PV_Y + (PV_H - hh) / 2 }, hs));
            c.roots.push_back(loadSave(k, frameOf(SPLIT), frameOf(SPLIT_END), { PV_X + (PV_W + 12) / 2, PV_Y + (PV_H - hh) / 2 }, hs));
            c.roots.push_back(loadSave(k, frameOf(SPLIT_END), to, { PV_X, PV_Y }, scale));
        } else {
            c.roots.push_back(loadSave(k, from, to, { PV_X, PV_Y }, scale));
        }
    }

    // the endless loop: run inside the frame; its abort is printed in the terminal
    for (int k = 0; k < NSAVES; k++) {
        if (!saved || SAVES[k].result != Save::RunError) continue;
        auto holder = std::shared_ptr<TNode>(loadSave(k, 0, 1, { 0, 0 }, 1).release());
        const int from = frameOf(resultAt(k)), to = frameOf(C6);   // its rows stay to the chapter's end
        TNode* run = addRoot(c.roots, from, to - from);
        run->fn = [holder, film, k, from](const Ctx& cx, int lt) {
            std::string msg;
            try {
                drawNodeContent(*holder, FREEZE, cx);
                msg = "(ran: expected an abort)";
            } catch (const std::exception& e) {
                msg = cliMessage(e.what());
            }
            // write over the rows the terminal reserved for this save's output
            const auto wrapped = wrap(msg, termColumns());
            size_t i = 0;
            for (const Row& r : termRows(*film, (from + lt) / (double)FPS))
                if (r.save == k && !r.command && i < wrapped.size())
                    text(mono(), cx, wrapped[i++], { TM_X + 20, r.y }, TERM_SIZE, CORAL);
        };
    }

    // above: chips, split labels, captions, the full-screen chapters, wipes
    addRoot(c.roots, 0, total)->fn = [thumb](const Ctx& cx, int t) {
        const double s = t / (double)FPS;
        if (s < C6) {
            drawChip(cx, s);
            drawStale(cx, s);
            drawSplitLabels(cx, s);
        }
        const float k = cover(s);
        if (s >= C2 - SLIDE && s < C3) {
            // chapter 2 slides in from the right and out to the left
            const float x = s < C2 ? W * (1 - k) : -W * (1 - k);
            const Ctx shifted = nodeCtx(cx, NodeProps{ { x, 0 }, 1, 0, 1 });
            drawZip(shifted, s);
        }
        if (s >= C6 - SLIDE) {
            const Ctx shifted = nodeCtx(cx, NodeProps{ { 0, H * (1 - k) }, 1, 0, 1 });
            drawEnd(shifted, s, t);
        }
        drawCaption(cx, s);
        if (!thumb) drawProgress(cx, s);
    };
}

// ------------------------------------------------------------------ sound
// Key clicks, paste, save and frame blips: synthesised from the same operations and saves.
void writeUi(double total) {
    const int rate = 48000;
    std::vector<float> st((size_t)(total * rate) * 2 + 4, 0.f);
    auto add = [&](double at, double len, auto&& sample) {
        const size_t a = (size_t)(at * rate);
        for (size_t i = 0; i < (size_t)(len * rate); i++) {
            const size_t j = (a + i) * 2;
            if (j + 1 >= st.size()) break;
            const float v = sample(i / (double)rate);
            st[j] += v;
            st[j + 1] += v;
        }
    };
    for (const Op& op : ops()) {
        if (op.kind == Op::Type) {
            for (size_t i = 0; i < op.text.size(); i++) {
                if (op.text[i] == ' ') continue;
                const double at = op.t + i / op.cps;
                const float g = 0.16f + 0.08f * (float)rnd01(31, (int)(op.t * 100) + (int)i);
                const double f = 1800 + 900 * rnd01(37, (int)(op.t * 100) + (int)i);
                add(at, 0.03, [&](double u) {
                    const float noise = (float)rnd01(41, (int)(u * rate) + (int)i * 977) * 2 - 1;
                    return g * std::exp(-(float)u * 180) * (0.6f * noise + 0.4f * (float)std::sin(TAU * f * u));
                });
            }
        } else if (op.kind == Op::Paste) {
            add(op.t, 0.12, [&](double u) {
                const float noise = (float)rnd01(43, (int)(u * rate)) * 2 - 1;
                return 0.12f * std::exp(-(float)u * 30) * noise;
            });
        } else {
            add(op.t + SELECT, 0.05, [&](double u) {
                return 0.10f * std::exp(-(float)u * 80) * (float)std::sin(TAU * 520 * u);
            });
        }
    }
    for (int k = 0; k < NSAVES; k++) {
        add(SAVES[k].t, 0.16, [&](double u) {   // save: two soft notes
            const double f = u < 0.07 ? 660 : 990;
            const double v = u < 0.07 ? std::exp(-u * 40) : std::exp(-(u - 0.07) * 40);
            return (float)(0.12 * v * std::sin(TAU * f * u));
        });
        if (SAVES[k].result == Save::Frame)
            add(resultAt(k), 0.25, [&](double u) {   // the frame lands
                return (float)(0.10 * std::exp(-u * 18) * std::sin(TAU * 1320 * u));
            });
    }
    media::writeWav(UI_WAV, st, rate);
}

void writeScore(double total) {
    // D major, two chords a chapter; opens at chapter 2, thins under chapter 5, resolves on the logo
    Song song;
    const double chords[4][3] = { { 146.83, 220.00, 293.66 },   // D
                                  { 123.47, 185.00, 246.94 },   // Bm
                                  { 98.00, 196.00, 246.94 },    // G
                                  { 110.00, 164.81, 277.18 } }; // A
    for (double t = 0; t < C6; t += 2.0) {
        const int c = (int)(t / 2) % 4;
        const bool open = t >= C2 && t < C3;
        const bool thin = t >= C5;
        const double amp = thin ? 0.025 : open ? 0.05 : 0.04;
        for (int i = 0; i < 3; i++) song.push_back({ t, 2.2, chords[c][i], amp, false });
        if (!thin)
            for (int b = 0; b < 4; b++) song.push_back({ t + 0.5 * b, 0.45, chords[c][0] / 2, b == 0 ? 0.09 : 0.05, true });
    }
    for (int i = 0; i < 3; i++) song.push_back({ C6 + 0.5, total - C6 - 0.5, chords[0][i], 0.06, false });
    song.push_back({ C6 + 0.5, total - C6 - 0.5, 73.42, 0.07, false });
    const std::vector<float> mono = renderSong(song, total, 48000);
    std::vector<float> stereo(mono.size() * 2);
    for (size_t i = 0; i < mono.size(); i++) stereo[i * 2] = stereo[i * 2 + 1] = mono[i];
    media::writeWav(SCORE, stereo, 48000);
}

void writeSrt() {
    sub::Track tr;
    // the footnote joins the caption above it as a second line, so no two cues overlap
    const double FOOT = 46.7;
    const std::string NOTE = "Scripts are trusted code: run only scripts you would run as a program.";
    for (const Caption& c : CAPTIONS) {
        if (c.from < FOOT && c.to > FOOT) {
            tr.cues.push_back({ c.from, FOOT, c.text, false });
            tr.cues.push_back({ FOOT, c.to, std::string(c.text) + "\n" + NOTE, false });
        } else {
            tr.cues.push_back({ c.from, c.to, c.text, false });
        }
    }
    tr.cues.push_back({ 52.0, LENGTH, "raymotion 0.9.1", false });
    std::sort(tr.cues.begin(), tr.cues.end(), [](const sub::Cue& a, const sub::Cue& b) { return a.start < b.start; });
    sub::writeSrt(SRT_PATH, tr);
}

void ensureAssets() {
    std::filesystem::create_directories(ASSETS);
    writeScripts();
    writeUi(LENGTH);
    writeScore(LENGTH);
    writeSrt();
}

std::string existing(const std::string& file) {
    try { return media::resolve(file); } catch (...) { return ""; }
}

void sfx(Nodes& roots, double at, const char* file, float volume) {
    if (existing(file).empty()) return;
    TNode* n = addRoot(roots, std::max(0, frameOf(at)), 60);
    media::AudioOptions o;
    o.volume = volume;
    media::audio(n, file, o);
}

void addSound(Comp& c) {
    const int total = c.duration;
    if (!existing(SCORE).empty()) {
        TNode* bed = addRoot(c.roots, 0, total);
        media::AudioOptions o;
        o.volume = 0.66f;
        o.volumeAt = [total](int t) { return 1 - clamp01((t - (total - 45)) / 45.0); };
        media::audio(bed, SCORE, o);
    }
    if (!existing(UI_WAV).empty()) {
        TNode* ui = addRoot(c.roots, 0, total);
        media::AudioOptions o;
        o.volume = 0.95f;
        media::audio(ui, UI_WAV, o);
    }
    sfx(c.roots, C2 - SLIDE, "assets/sfx/whoosh.wav", 0.22f);
    sfx(c.roots, C3 - SLIDE, "assets/sfx/whoosh.wav", 0.18f);
    for (int i = 0; i < 3; i++) sfx(c.roots, 12.4 + 0.8 * i, "assets/sfx/pop.wav", 0.16f);
    sfx(c.roots, 14.6, "assets/sfx/chime.wav", 0.14f);
    for (int k = 0; k < NSAVES; k++)
        if (SAVES[k].result != Save::Frame) sfx(c.roots, resultAt(k), "assets/sfx/thud.wav", 0.30f);
    sfx(c.roots, 38.6, "assets/sfx/chime.wav", 0.16f);
    sfx(c.roots, C6 - SLIDE, "assets/sfx/whoosh.wav", 0.2f);
    sfx(c.roots, C6 + 0.5, "assets/sfx/boom.wav", 0.22f);
}

} // namespace

void registerScripts09(Registry& out) {
    {
        Comp c;
        c.name = "scripts-09";
        c.w = W; c.h = H; c.fps = FPS;
        c.duration = frameOf(LENGTH);
        c.background = BG;
        c.assets = [](const Comp&, const Json&) { ensureAssets(); };
        c.build = [](Comp& self, const Json&) {
            buildFilm(self, false);
            addSound(self);
        };
        out.push_back(std::move(c));
    }
    {
        // thumbnail: the cheering character, its code and the claim
        Comp c;
        c.name = "scripts-09-thumb";
        c.w = W; c.h = H; c.fps = FPS;
        c.duration = 1;
        c.background = BG;
        c.assets = [](const Comp&, const Json&) { std::filesystem::create_directories(ASSETS); writeScripts(); };
        c.build = [](Comp& self, const Json&) {
            Comp film;
            film.duration = frameOf(LENGTH);
            buildFilm(film, true);
            TNode* at = addRoot(self.roots, 0, 1);
            at->freezeAt = frameOf(26.6);
            for (auto& r : film.roots) at->kids.push_back(std::move(r));
        };
        out.push_back(std::move(c));
    }
}

} // namespace rmtn
