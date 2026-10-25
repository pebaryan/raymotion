// code.cpp -- "code": code::, code blocks for programming explainers.
//   typing     a C++ function typed in, with a caret
//   languages  six languages and the light theme, line by line
//   focus      a binary search walked through a few lines at a time, with notes and marks
//   changes    callbacks refactored to async/await, then wrapped in try/catch: unchanged
//              code glides, edits keep their common tokens, the rest fades out and in
// The score (110 bpm on Em-C-G-D) is synthesised into out/ on first use.
//   raymotion render code
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <filesystem>

namespace rmtn {
namespace {

constexpr int XF = 20;
const std::string SCORE = "out/code-assets-v1/score.wav";
const Color BG = mkColor(0x14161c), INK = mkColor(0xf4f1ea), MUTED = mkColor(0x8a90a2), SUN = mkColor(0xffd166);

Font bold() { return fonts::ui(true); }

float ramp(double t, double a, double b, Easing e = ease::inOut(ease::cubic)) {
    return (float)interpolate(t, { a, b }, { 0, 1 }, clamped(e));
}

void backdrop(TNode* n, int len) {
    n->add(0, len)->fn = [](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1920, 1080, BG); };
}

void caption(TNode* n, int len, const char* title, const char* api) {
    TNode* card = n->add(12, len - 12);
    card->up = [len](int t, NodeProps& p) {
        const float in = ramp(t, 0, 18, ease::out(ease::cubic)), out = 1 - ramp(t, len - 40.0, len - 22.0, ease::linear);
        p.opacity = in * out;
        p.pos = { 0, 20 * (1 - in) };
    };
    const std::string tt = title, aa = api;
    card->fn = [tt, aa](const Ctx& cx, int) {
        const float w = std::max(MeasureTextEx(code::font(), aa.c_str(), 24, 0).x, MeasureTextEx(bold(), tt.c_str(), 44, 0).x) + 72;
        dr::roundRect(cx, 60, 1080 - 56 - 132, w, 132, 16, mkColor(0x222737, 235));
        dr::text(bold(), cx, tt.c_str(), { 96, 1080 - 56 - 112 }, 44, 0, INK, 0, 0);
        dr::text(code::font(), cx, aa.c_str(), { 96, 1080 - 56 - 50 }, 24, 0, SUN, 0, 0);
    };
}

// top-left that centres a card on `c`
Vector2 centred(const code::Block& b, const code::Style& s, Vector2 c) {
    const Rectangle r = code::bounds(b, s);
    return { std::round(c.x - r.width / 2), std::round(c.y - r.height / 2) };
}

// ------------------------------------------------------------------ scenes

void typingScene(TNode* n, int len) {
    backdrop(n, len);
    const code::Block src = code::Block::parse(R"(
#include "rmtn/raymotion.hpp"
using namespace rmtn;

// A title that springs up and fades in.
void title(Comp& c, const std::string& text) {
    TNode* n = addRoot(c.roots, 0, 90);
    n->up = [](int t, NodeProps& p) {
        p.opacity = interpolate(t, { 0, 20 }, { 0, 1 });
        p.pos.y = 40 * (1 - spring(t, 30));
    };
    n->fn = [text](const Ctx& cx, int) { say(cx, text); };
}
)", "cpp");
    n->add(0, len)->fn = [src](const Ctx& cx, int t) {
        code::Style st;
        st.title = "title.cpp";
        st.size = 27;
        code::Look look;
        look.reveal = std::max(0.0, (t - 20) * 2.6);
        look.cursor = true;
        look.t = t;
        code::draw(cx, src, centred(src, st, { 960, 470 }), st, look);
    };
    caption(n, len, "Typed in", "look.reveal = chars;  look.cursor = true;  code::draw(cx, block, at, style, look)");
}

void languagesScene(TNode* n, int len) {
    backdrop(n, len);
    struct Card { const char* lang; const char* file; const char* src; bool light; };
    static const Card cards[] = {
        { "python", "fib.py", "@cache\ndef fib(n: int) -> int:\n    \"\"\"The n-th Fibonacci number.\"\"\"\n    return n if n < 2 else fib(n-1) + fib(n-2)", false },
        { "rust", "main.rs", "#[derive(Debug)]\nstruct Point { x: f32, y: f32 }\n\nfn main() {\n    let p = Point { x: 1.0, y: 2.0 };\n    println!(\"{:?}\", p);\n}", false },
        { "go", "server.go", "func main() {\n    http.HandleFunc(\"/\", hello)\n    log.Fatal(http.ListenAndServe(\":80\", nil))\n}", false },
        { "bash", "build.sh", "# configure and build\ncmake -B build -G Ninja\ncmake --build build -j \"$JOBS\"\n./build/raymotion render code", false },
        { "json", "props.json", "{\n  \"title\": \"Hello\",\n  \"duration\": 4.5,\n  \"loop\": true,\n  \"tags\": [\"intro\", null]\n}", true },
        { "sql", "top.sql", "SELECT name, COUNT(*) AS plays\nFROM videos\nWHERE year = 2026  -- this year\nGROUP BY name\nORDER BY plays DESC\nLIMIT 10;", false },
    };
    std::vector<code::Block> blocks;
    for (const Card& c : cards) blocks.push_back(code::Block::parse(c.src, c.lang));
    n->add(0, len)->fn = [blocks](const Ctx& cx, int t) {
        for (size_t i = 0; i < blocks.size(); i++) {
            const Card& c = cards[i];
            code::Style st;
            st.size = 21;
            st.title = c.file;
            st.lineNumbers = false;
            st.padding = { 26, 20 };
            if (c.light) st.theme = code::Theme::light();
            const int col = (int)(i % 3), row = (int)(i / 3);
            const Rectangle box = code::bounds(blocks[i], st);
            const Vector2 at{ std::round(330 + col * 630.f - box.width / 2), row == 0 ? 90.f : 480.f };   // tops aligned per row
            const float start = 10 + 14.f * i, in = ramp(t, start, start + 20, ease::out(ease::cubic));
            Ctx c2 = cx;
            c2.a *= in;
            c2.o.y += 24 * (1 - in);
            code::Look look;
            look.lines = std::max(0.f, (t - start - 10) / 7.f);
            code::draw(c2, blocks[i], at, st, look);
        }
    };
    caption(n, len, "Many languages, two themes", "Block::parse(src, \"rust\")   look.lines = n   Theme::light()");
}

void focusScene(TNode* n, int len) {
    backdrop(n, len);
    const code::Block src = code::Block::parse(R"(
def binary_search(items, target):
    lo, hi = 0, len(items) - 1
    while lo <= hi:
        mid = (lo + hi) // 2
        if items[mid] == target:
            return mid
        if items[mid] < target:
            lo = mid + 1
        else:
            hi = mid - 1
    return -1
)", "python");
    struct Step { code::Lines lines; const char* note; const char* mark; int markLine; };
    static const Step steps[] = {
        { { 1, 1 }, "a sorted list and the value to find", "items", 1 },
        { { 2, 2 }, "search the whole list to begin with", "len(items) - 1", 2 },
        { { 3, 4 }, "look at the middle of what is left", "(lo + hi) // 2", 4 },
        { { 5, 6 }, "found it", "return mid", 6 },
        { { 7, 10 }, "otherwise throw away the half it cannot be in", "lo = mid + 1", 8 },
        { { 11, 11 }, "nothing left: it is not there", "-1", 11 },
    };
    const int per = 44, first = 30;
    n->add(0, len)->fn = [src, per, first](const Ctx& cx, int t) {
        code::Style st;
        st.size = 30;
        st.title = "search.py";
        const Vector2 at{ 170, 150 };
        code::Look look;
        const int k = std::clamp((t - first) / per, 0, 5), local = (t - first) - k * per;
        const float on = ramp(t, first - 20, first);
        if (t >= first - 20) {
            look.focus = { steps[k].lines };
            look.focusFrom = k > 0 ? code::Focus{ steps[k - 1].lines } : code::Focus{};
            look.focusMix = k > 0 ? ramp(local, 0, 14) : on;
            look.dim = 0.25f;
            look.marks.push_back(code::Mark::text(src, steps[k].markLine, steps[k].mark, SUN));
            look.marks.back().u = ramp(local, 10, 24, ease::out(ease::cubic));
            if (t > first + 6 * per) look.focusMix = 1 - ramp(t, first + 6 * per, first + 6 * per + 20), look.marks.clear();
        }
        code::draw(cx, src, at, st, look);
        // the note, pinned beside the focused lines
        const Rectangle box = code::bounds(src, st, at);
        const code::Lines& ln = steps[k].lines;
        const Vector2 top = code::cell(src, st, at, ln.from, 0);
        const float lh = st.size * st.lineHeight, midY = top.y + (ln.to - ln.from + 1) * lh / 2;
        const float a = ramp(local, 6, 20) * (t > first + 6 * per ? 1 - ramp(t, first + 6 * per, first + 6 * per + 20) : 1);
        if (t >= first && a > 0) {
            const float x0 = box.x + box.width + 20, x1 = x0 + 60;
            dr::strokeSeg(cx, { x0, midY }, { x1 - 6, midY }, 3, withAlpha(SUN, a));
            dr::circle(cx, { x0, midY }, 6, withAlpha(SUN, a));
            dr::text(bold(), cx, steps[k].note, { x1 + 4, midY }, 34, 0, withAlpha(INK, a), 0, 0.5);
        }
    };
    caption(n, len, "Line focus and marks", "look.focus = {{3, 4}};  look.focusFrom, focusMix   Mark::text(block, line, \"mid\", c)");
}

void changesScene(TNode* n, int len) {
    backdrop(n, len);
    const code::Block v1 = code::Block::parse(R"(
function load(url) {
  return fetch(url)
    .then(res => res.json())
    .then(data => render(data));
}
)", "ts");
    const code::Block v2 = code::Block::parse(R"(
async function load(url) {
  const res = await fetch(url);
  const data = await res.json();
  render(data);
}
)", "ts");
    const code::Block v3 = code::Block::parse(R"(
async function load(url: string): Promise<void> {
  try {
    const res = await fetch(url);
    const data = await res.json();
    render(data);
  } catch (err) {
    console.error("load failed", err);
  }
}
)", "ts");
    const code::Morph m12(v1, v2), m23(v2, v3);
    const int a = 50, b = 210, dur = 70;
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        code::Style st;
        st.size = 32;
        st.title = "load.ts";
        st.columns = 50;
        const Vector2 at{ 960 - code::bounds(v3, st).width / 2, 150 };
        if (t < a) code::draw(cx, v1, at, st);
        else if (t < b) code::draw(cx, m12, at, st, ramp(t, a, a + dur, ease::linear));
        else code::draw(cx, m23, at, st, ramp(t, b, b + dur, ease::linear));
        const char* step = t < a + dur / 2 ? "promises" : t < b + dur / 2 ? "async / await" : "with types and error handling";
        const float k = t < a + dur / 2 ? 1 - ramp(t, a, a + 15) : t < b + dur / 2 ? ramp(t, a + dur / 2, a + dur / 2 + 15) * (1 - ramp(t, b, b + 15))
                                                                                  : ramp(t, b + dur / 2, b + dur / 2 + 15);
        dr::text(bold(), cx, step, { 960, 90 }, 38, 0, withAlpha(MUTED, k), 0.5, 0.5);
    };
    caption(n, len, "Changes between versions", "const code::Morph change(before, after);   code::draw(cx, change, at, style, u)");
}

// The score, 110 bpm on Em-C-G-D: a plucky arpeggio like keys being typed, a bell at each
// chapter.
void genScore(const std::string& path, double end, const std::vector<double>& chapters) {
    using procgen::Score;
    Score s(end);
    struct Chord { int root; int pad[3]; int arp[4]; };
    const Chord em{ 40, { 59, 64, 67 }, { 71, 76, 79, 76 } }, c{ 48, { 60, 64, 67 }, { 72, 76, 79, 76 } },
                g{ 43, { 59, 62, 67 }, { 71, 74, 79, 74 } }, d{ 50, { 57, 62, 66 }, { 69, 74, 78, 74 } };
    const Chord prog[4] = { em, c, g, d };
    const double bar = 4 * 60.0 / 110, beat = bar / 4, stop = end - 2.4;
    int i = 0;
    for (double t = 0; t < stop; t += bar, i++) {
        const Chord& ch = prog[i % 4];
        const bool drums = t >= chapters[0], full = t >= chapters[2];
        for (int p : ch.pad) s.pad(t, bar * 0.95, p, 0.07f, full ? 1700 : 1000);
        for (int e = 0; e < 8; e++) s.pluck(t + e * beat / 2, ch.arp[(e * 3) % 4] + (e % 2 ? 0 : 12), e % 4 == 0 ? 0.045f : 0.03f, e % 2 ? 0.3f : 0.7f, 7);
        for (int e = 0; e < 4; e++) s.bass(t + e * beat, beat * 0.85, ch.root, drums ? 0.22f : 0.14f);
        if (drums) { s.kick(t, 0.45f); s.kick(t + 2 * beat, 0.4f); }
        if (full) { s.clap(t + beat, 0.14f); s.clap(t + 3 * beat, 0.14f); }
        if (t >= bar * 2) for (int h = 0; h < 8; h++) s.hat(t + h * beat / 2, h % 2 ? 0.07f : 0.04f, h % 2 ? 0.35f : 0.65f);
    }
    for (size_t k = 0; k < chapters.size(); k++) s.bell(chapters[k], 76 + (int)(k % 3) * 3, 0.09f, 0.5f);
    for (int p : em.pad) { s.pad(stop, 2.4, p, 0.12f, 2000, 0.35f); s.pad(stop, 2.4, p - 12, 0.08f, 1200, 0.65f); }
    s.bass(stop, 2.4, em.root - 12, 0.22f);
    for (int k = 0; k < 4; k++) s.bell(stop + k * 0.4, em.arp[k] + 12, 0.08f, 0.2f + 0.2f * k);
    media::writeWav(path, s.mix(), Score::RATE);
}

} // namespace

void registerCode(Registry& out) {
    Comp c;
    c.name = "code";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    struct S { int len; void (*make)(TNode*, int); };
    const S scenes[] = { { 230, typingScene }, { 220, languagesScene }, { 330, focusScene }, { 340, changesScene } };
    std::vector<double> chapters;
    bool first = true;
    for (const S& s : scenes) {
        if (!first) { ts.transition(Transition::fade(XF)); chapters.push_back((ts.end() - XF) / 30.0); }
        first = false;
        s.make(ts.add(s.len), s.len);
    }
    c.duration = ts.end();
    const double seconds = c.duration / 30.0;
    c.assets = [seconds, chapters](const Comp&, const Json&) {
        if (std::filesystem::exists(SCORE)) return;
        std::filesystem::create_directories(std::filesystem::path(SCORE).parent_path());
        genScore(SCORE, seconds, chapters);
    };
    media::audio(addRoot(c.roots, 0, c.duration), SCORE);
    out.push_back(std::move(c));
}

} // namespace rmtn
