// sparql.cpp -- "sparql": an explainer of how a SPARQL query is processed, narrated.
//   intro     the title and the five stages
//   graph     a small knowledge graph builds, with its triples typed beside it
//   query     the query typed in; variables and triple patterns picked out
//   parse     the query morphs into its algebra while the operator tree builds
//   optimize  cardinality estimates reorder the two patterns
//   match     the selective pattern found in the graph through an index; ?work bound
//   join      each ?work looked up against `wrote`; ?person bound
//   results   the solution table, and the five stages again
// Voice-over: assets/vo/sparql/*.wav, generated from assets/vo/sparql/lines.json with
// tools/vo_batch.py (Qwen3-TTS clone). Each scene lasts as long as its line, and visual cues
// are placed by where their words fall in the line. A quiet pad bed is synthesised into out/.
//   raymotion render sparql
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace rmtn {
namespace {

constexpr int XF = 16, LEAD = 14, TAIL = 26;
const Color BG = mkColor(0x0e131d), PANEL = mkColor(0x182030), LINE = mkColor(0x2a3448), INK = mkColor(0xf2efe8),
            MUTED = mkColor(0x8b93a7), SUN = mkColor(0xffd166), SKY = mkColor(0x6ec6ff), ROSE = mkColor(0xff7a90),
            MINT = mkColor(0x8bd17c), LILAC = mkColor(0xc49bff);

Font bold() { return fonts::ui(true); }
Font regular() { return fonts::ui(false); }

float ramp(double t, double a, double b, Easing e = ease::inOut(ease::cubic)) {
    return (float)interpolate(t, { a, b }, { 0, 1 }, clamped(e));
}

// ------------------------------------------------------------------ voice-over timing

// Seconds of audio in a PCM WAV (0 if missing).
double wavSeconds(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return 0;
    unsigned char h[12];
    double secs = 0;
    unsigned byteRate = 0;
    if (std::fread(h, 1, 12, f) == 12 && !std::memcmp(h, "RIFF", 4) && !std::memcmp(h + 8, "WAVE", 4)) {
        unsigned char ch[8];
        while (std::fread(ch, 1, 8, f) == 8) {
            const unsigned size = ch[4] | ch[5] << 8 | ch[6] << 16 | (unsigned)ch[7] << 24;
            if (!std::memcmp(ch, "fmt ", 4)) {
                unsigned char fmt[16];
                if (size < 16 || std::fread(fmt, 1, 16, f) != 16) break;
                byteRate = fmt[8] | fmt[9] << 8 | fmt[10] << 16 | (unsigned)fmt[11] << 24;
                std::fseek(f, (long)(size - 16 + (size & 1)), SEEK_CUR);
            } else if (!std::memcmp(ch, "data", 4)) {
                if (byteRate) secs = (double)size / byteRate;
                break;
            } else std::fseek(f, (long)(size + (size & 1)), SEEK_CUR);
        }
    }
    std::fclose(f);
    return secs;
}

// One narrated scene: its line, how long it is, and where words fall in it.
struct Vo {
    std::string file, text;
    double seconds = 0;
    int len() const { return LEAD + (int)std::ceil(seconds * 30) + TAIL; }
    // Frame (scene-local) at which `phrase` is spoken, estimated by its place in the text.
    int cue(const char* phrase, int shift = 0) const {
        const size_t at = text.find(phrase);
        const double u = at == std::string::npos ? 0 : (double)at / std::max<size_t>(1, text.size());
        return LEAD + (int)std::round(u * seconds * 30) + shift;
    }
};

std::vector<Vo> loadVo() {
    const std::string dir = "assets/vo/sparql/";
    std::vector<Vo> out;
    std::ifstream in(dir + "lines.json", std::ios::binary);
    if (!in) return out;                       // run from elsewhere: no script, no comp
    std::stringstream ss;
    ss << in.rdbuf();
    const Json spec = Json::parse(ss.str());
    for (const Json& l : spec["lines"].asArray()) {
        Vo v;
        v.file = dir + l.str("file", "");
        v.text = l.str("text", "");
        v.seconds = wavSeconds(v.file);
        if (v.seconds <= 0) v.seconds = v.text.size() / 15.0;     // not generated yet: ~15 chars/s
        out.push_back(v);
    }
    return out;
}

// ------------------------------------------------------------------ shared pieces

TextStyle style(float size, Color c = INK, Font f = Font{}) {
    TextStyle s;
    s.font = f.texture.id ? f : bold();
    s.size = size;
    s.color = c;
    return s;
}

void backdrop(TNode* n, int len) {
    n->add(0, len)->fn = [](const Ctx& cx, int) {
        dr::rect(cx, 0, 0, 1920, 1080, BG);
        for (int x = 0; x <= 1920; x += 80) dr::rect(cx, (float)x, 0, 1, 1080, withAlpha(LINE, 0.25f));
        for (int y = 0; y <= 1080; y += 80) dr::rect(cx, 0, (float)y, 1920, 1, withAlpha(LINE, 0.25f));
    };
}

// "2 · THE QUERY" top left, revealed letter by letter, and a headline under it.
void heading(TNode* n, int len, int step, const char* kicker, const char* headline) {
    const std::string k = std::to_string(step) + "  \xC2\xB7  " + kicker, h = headline;
    const auto texts = txt::later([k, h] {
        TextStyle ks = style(26, SUN);
        ks.spacing = 4;
        return std::make_pair(txt::Text::make(k, ks), txt::Text::make(h, style(50)));
    });
    n->add(0, len)->fn = [texts, len](const Ctx& cx, int t) {
        const auto& [kt, ht] = texts();
        txt::Reveal r;
        r.stagger = 0.8f;
        r.duration = 10;
        r.offset = { 0, 0 };
        r.fade = 1;
        Ctx c = cx;
        c.a *= 1 - ramp(t, len - 14.0, len - 2.0, ease::linear);
        txt::draw(c, kt, { 80, 56 }, t - 4, r);
        txt::Reveal w;
        w.by = txt::Unit::Word;
        w.stagger = 3;
        w.duration = 16;
        w.offset = { 0, 24 };
        txt::draw(c, ht, { 80, 96 }, t - 10, w);
    };
}

void voice(TNode* n, const Vo& v) {
    media::AudioOptions o;
    o.volume = 0.95f;
    media::audio(n->add(LEAD, v.len() - LEAD), v.file, o);
}

// A rounded label chip, centred on c; k = 0..1 pops it in.
void chip(const Ctx& cx, const char* s, Vector2 c, float size, Color fill, Color ink, float k, Font f = Font{}) {
    if (k <= 0.001f) return;
    const Font font = f.texture.id ? f : bold();
    const Vector2 m = MeasureTextEx(font, s, size, 0);
    const float w = m.x + size * 1.2f, h = size * 1.9f;
    Ctx q = cx;
    q.o = mapP(cx, c);
    q.s = cx.s * (0.85f + 0.15f * k);
    q.a = cx.a * k;
    dr::roundRect(q, -w / 2, -h / 2, w, h, h / 2.4f, fill);
    dr::text(font, q, s, { 0, 0 }, size, 0, ink, 0.5, 0.5);
}

const char* STAGES[] = { "Parse", "Optimize", "Match", "Join", "Project" };
const Color STAGE_COLORS[] = { SKY, LILAC, MINT, SUN, ROSE };

// The five stages in a row centred at y; `lit` (0..5, fractional) lights them in turn,
// `show` (0..5) pops them in.
void stages(const Ctx& cx, float y, float show, float lit, int active = -1) {
    const float gap = 290, x0 = 960 - gap * 2;
    for (int i = 0; i < 5; i++) {
        const float k = std::clamp(show - i, 0.f, 1.f);
        const float on = std::clamp(lit - i, 0.f, 1.f);
        const bool act = i == active;
        const Color fill = mixC(PANEL, STAGE_COLORS[i], act ? 1.f : 0.85f * on);
        const Color ink = mixC(MUTED, BG, act ? 1.f : on);
        if (i < 4 && k > 0) dr::rect(cx, x0 + i * gap + 90, y - 1.5f, (gap - 180) * std::clamp(show - i - 0.6f, 0.f, 1.f), 3, withAlpha(LINE, k));
        chip(cx, STAGES[i], { x0 + i * gap, y }, 30, fill, ink, ease::out(ease::back())(k));
    }
}

// ------------------------------------------------------------------ the knowledge graph

dg::Graph knowledgeGraph() {
    dg::Graph g;
    g.direction = dg::Direction::LR;
    auto node = [&](const char* id, const char* label, const char* kind, float x, float y) {
        dg::Node& n = g.addNode(id, label);
        n.kind = kind;
        n.pos = { x, y };
        if (std::strcmp(kind, "Topic") == 0) n.shape = dg::Shape::Stadium;
    };
    node("babbage", "Charles Babbage", "Person", 0, -90);
    node("ada", "Ada Lovelace", "Person", 0, 80);
    node("turing", "Alan Turing", "Person", 0, 305);
    node("hopper", "Grace Hopper", "Person", 0, 540);
    node("engine", "Analytical Engine", "Work", 440, -90);
    node("noteG", "Note G", "Work", 440, 80);
    node("p1950", "Computing Machinery and Intelligence", "Work", 440, 230);
    node("p1936", "On Computable Numbers", "Work", 440, 390);
    node("a0", "A-0 System", "Work", 440, 540);
    node("computing", "Computing", "Topic", 880, 155);
    node("maths", "Mathematics", "Topic", 880, 390);
    node("compilers", "Compilers", "Topic", 880, 540);
    g.addEdge("babbage", "engine", "designed");
    g.addEdge("ada", "noteG", "wrote");
    g.addEdge("noteG", "engine", "describes");
    g.addEdge("turing", "p1950", "wrote");
    g.addEdge("turing", "p1936", "wrote");
    g.addEdge("hopper", "a0", "wrote");
    g.addEdge("noteG", "computing", "about");
    g.addEdge("p1950", "computing", "about");
    g.addEdge("p1936", "maths", "about");
    g.addEdge("a0", "compilers", "about");
    return g;
}

std::shared_ptr<dg::Diagram> kgDiagram() {
    dg::Theme th = dg::Theme::dark();
    th.fontSize = 22;
    th.maxLabelWidth = 230;
    th.highlight = SUN;
    th.palette = { mkColor(0x5b3a4a), mkColor(0x24445e), mkColor(0x2f5138) };   // Person, Work, Topic
    dg::LayoutOptions lo;
    lo.kind = dg::LayoutKind::None;
    auto d = dg::Diagram::make(knowledgeGraph(), th, lo);
    d->maxZoom = 1.0f;
    return d;
}

// ------------------------------------------------------------------ the query

const char* QUERY = R"(PREFIX ex: <http://example.org/>

SELECT ?person ?work
WHERE {
  ?person ex:wrote ?work .
  ?work   ex:about ex:Computing .
})";

const char* ALGEBRA = R"((project (?person ?work)
  (bgp
    (triple ?person ex:wrote ?work)
    (triple ?work ex:about ex:Computing))))";

code::Style codeStyle(const char* title, float size = 30) {
    code::Style st;
    st.title = title;
    st.size = size;
    return st;
}

// A results table: headers and rows; `rows` (fractional) slides rows in, `cols` reveals
// columns left to right (fractional).
void table(const Ctx& cx, Vector2 at, const std::vector<std::string>& heads, const std::vector<std::vector<std::string>>& data,
           const std::vector<float>& widths, float rows, float cols, float alpha = 1) {
    const float rh = 64, fs = 26;
    float total = 0;
    for (float w : widths) total += w;
    Ctx c = cx;
    c.a *= alpha;
    dr::roundRect(c, at.x, at.y, total, rh * (1 + std::max(0.f, rows)) + 12, 14, PANEL);
    float x = at.x;
    for (size_t j = 0; j < heads.size(); j++) {
        const float k = std::clamp(cols - (float)j, 0.f, 1.f);
        dr::text(code::font(), c, heads[j].c_str(), { x + 24, at.y + rh / 2 + 4 }, fs, 0, withAlpha(SKY, k), 0, 0.5);
        for (size_t i = 0; i < data.size(); i++) {
            const float r = std::clamp(rows - (float)i, 0.f, 1.f) * k;
            if (r <= 0) continue;
            const float y = at.y + rh * (i + 1.5f) + 10 * (1 - r) + 4;
            dr::text(bold(), c, data[i][j].c_str(), { x + 24, y }, fs, 0, withAlpha(INK, r), 0, 0.5);
        }
        x += widths[j];
    }
    dr::rect(c, at.x + 16, at.y + rh, total - 32, 2, LINE);
}

// ------------------------------------------------------------------ scenes

void introScene(TNode* n, const Vo& v) {
    const int len = v.len();
    backdrop(n, len);
    const auto texts = txt::later([] {
        TextStyle s = style(92);
        s.align = Align::Center;
        return std::make_pair(txt::Text::make("How a SPARQL query\nbecomes an answer", s), txt::Text::make("one query, followed through the engine", style(36, MUTED, regular())));
    });
    const int stagesAt = v.cue("Let's open");
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        const auto& [title, sub] = texts();
        txt::Reveal r;
        r.stagger = 1.2f;
        r.duration = 18;
        r.offset = { 0, 50 };
        r.scale = 0.8f;
        const float lift = 90 * ramp(t, stagesAt - 10, stagesAt + 20);
        txt::draw(cx, title, { 960, 400 - lift }, t - 6, r, { 0.5f, 0.5f });
        txt::Reveal w;
        w.by = txt::Unit::Word;
        w.stagger = 3;
        w.offset = { 0, 16 };
        txt::draw(cx, sub, { 960, 580 - lift }, t - 50, w, { 0.5f, 0.5f });
        stages(cx, 760, (t - stagesAt) / 7.f, 0);
    };
    voice(n, v);
}

void graphScene(TNode* n, const Vo& v) {
    const int len = v.len();
    backdrop(n, len);
    heading(n, len, 1, "THE DATA", "Facts as triples");
    auto d = kgDiagram();
    const int adaAt = v.cue("Ada Lovelace"), noteAt = v.cue("Note G is about"), graphAt = v.cue("together");
    d->build(v.cue("First"), 4, 16);
    d->dim(adaAt - 4, 14, { "ada", "noteG" });
    d->highlight(adaAt, 12, { "ada", "noteG", "ada->noteG" });
    d->dim(noteAt - 4, 14, { "ada", "noteG", "computing" });
    d->highlight(noteAt, 12, { "computing", "noteG->computing" });
    d->dim(graphAt, 20, {});
    d->unhighlight(graphAt + 10, 20);
    TNode* g = n->add(0, len);
    g->fn = [d](const Ctx& cx, int t) { d->draw(cx, t, { 60, 190, 1160, 760 }); };
    // the same facts written as triples
    const code::Block turtle = code::Block::parse(R"(ex:ada    ex:wrote  ex:noteG .
ex:noteG  ex:about  ex:Computing .
ex:turing ex:wrote  ex:p1950 .
ex:p1950  ex:about  ex:Computing .
# ... 6 more)", "sparql");
    const int tripleAt = v.cue("as triples");
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        code::Style st = codeStyle("facts.ttl", 24);
        st.lineNumbers = false;
        const Vector2 at{ 1260, 330 };
        const float in = ramp(t, tripleAt - 20, tripleAt);
        Ctx c = cx;
        c.a *= in;
        c.o.x += 40 * (1 - in);
        code::Look look;
        look.lines = (t - tripleAt) / 8.f;
        if (t > adaAt - 4 && t < graphAt) {
            look.focus = { { 1, 1 } };
            if (t > noteAt - 4) { look.focusFrom = { { 1, 1 } }; look.focus = { { 2, 2 } }; look.focusMix = ramp(t, noteAt - 4, noteAt + 8); }
        }
        code::draw(c, turtle, at, st, look);
        // subject / predicate / object over the columns
        const char* names[] = { "subject", "predicate", "object" };
        const int cols[] = { 0, 10, 20 };
        const Color colors[] = { ROSE, SUN, SKY };
        for (int i = 0; i < 3; i++) {
            const Vector2 p = code::cell(turtle, st, at, 1, cols[i]);
            const float k = ramp(t, tripleAt + 10 + i * 8, tripleAt + 24 + i * 8);
            const float top = code::bounds(turtle, st, at).y;      // above the card, over each column
            dr::text(bold(), c, names[i], { p.x, top - 30 + 10 * (1 - k) }, 24, 0, withAlpha(colors[i], k), 0, 0.5);
        }
    };
    voice(n, v);
}

void queryScene(TNode* n, const Vo& v) {
    const int len = v.len();
    backdrop(n, len);
    heading(n, len, 2, "THE QUERY", "Who wrote something about computing?");
    const code::Block q = code::Block::parse(QUERY, "sparql");
    const int shapeAt = v.cue("In SPARQL"), varAt = v.cue("Variables"), patAt = v.cue("Each line"), holeAt = v.cue("a triple with holes");
    const auto note = txt::later([] { return txt::Text::make("a triple with holes in it", style(30, SUN)); });
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        const code::Style st = codeStyle("query.rq", 34);
        const Vector2 at{ 960 - code::bounds(q, st).width / 2, 250 };
        code::Look look;
        look.reveal = (t - shapeAt + 20) * 4.0;
        look.cursor = true;
        look.t = t;
        if (t >= varAt) {
            const float k = ramp(t, varAt, varAt + 12);
            for (const char* var : { "?person", "?work" }) {
                code::Mark m = code::Mark::text(q, 3, var, SKY);
                m.u = k * (1 - ramp(t, patAt, patAt + 10));
                look.marks.push_back(m);
            }
        }
        if (t >= patAt) {
            look.focus = { { 5, 6 } };
            look.focusMix = ramp(t, patAt, patAt + 14);
            look.focusFrom = {};
            for (int line = 5; line <= 6; line++)
                for (const char* var : { "?person", "?work" }) {
                    if (q.find(line, var).first < 0) continue;
                    code::Mark m = code::Mark::text(q, line, var, SUN);
                    m.u = ramp(t, holeAt - 6, holeAt + 10);
                    m.underline = true;
                    look.marks.push_back(m);
                }
        }
        code::draw(cx, q, at, st, look);
        txt::Reveal r;
        r.stagger = 1;
        r.offset = { 0, 20 };
        const Rectangle b = code::bounds(q, st, at);
        txt::draw(cx, note(), { 960, b.y + b.height + 70 }, t - holeAt, r, { 0.5f, 0.5f });
    };
    voice(n, v);
}

void parseScene(TNode* n, const Vo& v) {
    const int len = v.len();
    backdrop(n, len);
    heading(n, len, 3, "PARSE", "From text to algebra");
    const code::Block q = code::Block::parse(QUERY, "sparql"), alg = code::Block::parse(ALGEBRA, "sparql");
    const code::Morph m(q, alg);
    const int algAt = v.cue("into an algebra"), bgpAt = v.cue("The two triple"), projAt = v.cue("A projection");
    dg::Graph tree;
    tree.direction = dg::Direction::TB;
    tree.addNode("proj", "Project  ?person ?work").kind = "op";
    tree.addNode("bgp", "BGP  (join)").kind = "op";
    tree.addNode("tp1", "?person wrote ?work").kind = "tp";
    tree.addNode("tp2", "?work about Computing").kind = "tp";
    tree.addEdge("proj", "bgp");
    tree.addEdge("bgp", "tp1");
    tree.addEdge("bgp", "tp2");
    dg::Theme th = dg::Theme::dark();
    th.fontSize = 24;
    th.palette = { mkColor(0x3d3360), mkColor(0x24445e) };
    auto d = dg::Diagram::make(tree, th);
    d->reveal(bgpAt, 18, { "tp1", "tp2" });
    d->reveal(bgpAt + 14, 18, { "bgp", "bgp->tp1", "bgp->tp2" });
    d->reveal(projAt, 18, { "proj", "proj->bgp" });
    d->highlight(projAt + 10, 14, { "proj" }, ROSE);
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        code::Style st = codeStyle("query.rq", 26);
        st.columns = 44;
        const Vector2 at{ 80, 250 };
        const float u = ramp(t, algAt - 10, algAt + 40, ease::linear);
        if (u <= 0) code::draw(cx, q, at, st);
        else if (u >= 1) {
            code::Look look;
            if (t >= projAt) { look.focus = { { 1, 1 } }; look.focusMix = ramp(t, projAt, projAt + 12); }
            else if (t >= bgpAt) { look.focus = { { 2, 4 } }; look.focusMix = ramp(t, bgpAt, bgpAt + 12); }
            code::draw(cx, alg, at, st, look);
        } else code::draw(cx, m, at, st, u);
        stages(cx, 1000, 5, 0, 0);
        d->draw(cx, t, { 1000, 230, 840, 600 });
    };
    voice(n, v);
}

void optimizeScene(TNode* n, const Vo& v) {
    const int len = v.len();
    backdrop(n, len);
    heading(n, len, 4, "OPTIMIZE", "Same answer, less work");
    const int statsAt = v.cue("Statistics"), fourAt = v.cue("four triples"), twoAt = v.cue("only two"), swapAt = v.cue("So the more");
    const auto note = txt::later([] { return txt::Text::make("order changes the work, not the answer", style(30, MUTED, regular())); });
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        const float in = ramp(t, 10, 30);
        const float swap = ramp(t, swapAt, swapAt + 30, ease::inOut(ease::expo));
        struct P { const char* text; int count; const int at; Color color; };
        const P ps[2] = { { "?person  ex:wrote  ?work", 4, fourAt, MINT }, { "?work  ex:about  ex:Computing", 2, twoAt, SUN } };
        for (int i = 0; i < 2; i++) {
            const float slot = i == 0 ? swap : 1 - swap;            // 0: top, 1: bottom
            const float y = 330 + 230 * slot;
            const float arc = std::sin(swap * PI) * (i == 0 ? 70.f : -70.f);
            Ctx c = cx;
            c.a *= in;
            c.o.x += arc;
            dr::roundRect(c, 260, y, 1400, 170, 22, PANEL);
            dr::text(bold(), c, (slot < 0.5f ? "1st" : "2nd"), { 310, y + 85 }, 30, 0, MUTED, 0, 0.5);
            dr::text(code::font(), c, ps[i].text, { 420, y + 60 }, 34, 0, INK, 0, 0.5);
            // the estimate: one dot per matching triple
            const float k = ramp(t, ps[i].at - 4, ps[i].at + 14);
            for (int j = 0; j < ps[i].count; j++) {
                const float kj = std::clamp(k * ps[i].count - j, 0.f, 1.f);
                dr::circle(c, { 440.f + j * 46, y + 122 }, 14 * ease::out(ease::back())(kj), ps[i].color);
            }
            char est[48];
            std::snprintf(est, sizeof est, "~%d matches", ps[i].count);
            dr::text(bold(), c, est, { 1600, y + 122 }, 28, 0, withAlpha(ps[i].color, k), 1, 0.5);
            dr::text(regular(), c, "estimate", { 1600, y + 60 }, 24, 0, withAlpha(MUTED, ramp(t, statsAt, statsAt + 12)), 1, 0.5);
        }
        txt::Reveal r;
        r.by = txt::Unit::Word;
        r.stagger = 3;
        r.offset = { 0, 16 };
        txt::draw(cx, note(), { 960, 870 }, t - v.cue("does not change"), r, { 0.5f, 0.5f });
        stages(cx, 1000, 5, 0, 1);
    };
    voice(n, v);
}

void matchScene(TNode* n, const Vo& v) {
    const int len = v.len();
    backdrop(n, len);
    heading(n, len, 5, "MATCH", "Find the first pattern");
    auto d = kgDiagram();
    const int idxAt = v.cue("Using an index"), findAt = v.cue("finds every"), bindAt = v.cue("Each match"),
              aAt = v.cue("Note G, and"), bAt = v.cue("Computing Machinery");
    d->dim(idxAt, 20, { "computing", "noteG", "p1950" });
    d->trace(findAt, 30, { "computing", "noteG" }, MINT);
    d->trace(findAt + 16, 30, { "computing", "p1950" }, MINT);
    d->highlight(aAt, 12, { "noteG" }, SKY);
    d->highlight(bAt, 12, { "p1950" }, SKY);
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        d->draw(cx, t, { 40, 190, 1180, 760 });
        // the pattern and its index
        const float in = ramp(t, 8, 26);
        Ctx c = cx;
        c.a *= in;
        dr::roundRect(c, 1250, 200, 620, 150, 18, PANEL);
        dr::text(regular(), c, "pattern", { 1280, 236 }, 22, 0, MUTED, 0, 0.5);
        dr::text(code::font(), c, "?work ex:about ex:Computing", { 1280, 290 }, 28, 0, INK, 0, 0.5);
        const float ik = ramp(t, idxAt, idxAt + 16);
        dr::roundRect(c, 1250, 370, 620, 110, 18, withAlpha(mkColor(0x1f3a2c), ik));
        dr::text(bold(), c, "POS index", { 1280, 405 }, 24, 0, withAlpha(MINT, ik), 0, 0.5);
        dr::text(code::font(), c, "(about, Computing) -> 2 subjects", { 1280, 448 }, 24, 0, withAlpha(INK, ik), 0, 0.5);
        const float rows = ramp(t, aAt, aAt + 12) + ramp(t, bAt, bAt + 12);
        table(cx, { 1250, 520 }, { "?work" }, { { "Note G" }, { "Computing Machinery and Int..." } }, { 620 },
              rows, ramp(t, bindAt, bindAt + 12), ramp(t, bindAt - 6, bindAt + 10));
        stages(cx, 1000, 5, 0, 2);
    };
    voice(n, v);
}

void joinScene(TNode* n, const Vo& v) {
    const int len = v.len();
    backdrop(n, len);
    heading(n, len, 6, "JOIN", "Each binding becomes a lookup");
    auto d = kgDiagram();
    const int lookAt = v.cue("It is a lookup"), adaQ = v.cue("Who wrote Note G"), adaA = v.cue("Ada Lovelace. Who"),
              turQ = v.cue("Who wrote Computing"), turA = v.cue("Alan Turing"), loopAt = v.cue("nested loop");
    d->dim(0, 1, { "computing", "noteG", "p1950" });
    d->highlight(0, 1, { "noteG", "p1950" }, SKY);
    d->dim(adaQ - 6, 16, { "computing", "noteG", "p1950", "ada" });
    d->trace(adaQ, 24, { "noteG", "ada" }, SUN);
    d->dim(turQ - 6, 16, { "computing", "noteG", "p1950", "ada", "turing" });
    d->trace(turQ, 24, { "p1950", "turing" }, SUN);
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        d->draw(cx, t, { 40, 190, 1180, 760 });
        const float in = ramp(t, lookAt - 10, lookAt + 8);
        Ctx c = cx;
        c.a *= in;
        dr::roundRect(c, 1250, 200, 620, 150, 18, PANEL);
        dr::text(regular(), c, "for each ?work, look up", { 1280, 236 }, 22, 0, MUTED, 0, 0.5);
        const bool second = t >= turQ - 4;
        dr::text(code::font(), c, second ? "?person ex:wrote ex:p1950" : "?person ex:wrote ex:noteG", { 1280, 290 }, 26, 0, INK, 0, 0.5);
        // the ?person column fills in as each lookup answers
        const float cols = 1 + ramp(t, adaA - 6, adaA + 8);
        table(cx, { 1250, 400 }, { "?work", "?person" },
              { { "Note G", "Ada Lovelace" }, { "Comp. Machinery...", t >= turA - 4 ? "Alan Turing" : "" } },
              { 340, 280 }, 2, cols);
        chip(cx, "nested loop join", { 1560, 680 }, 26, mkColor(0x4a3d1c), SUN, ramp(t, loopAt - 4, loopAt + 12));
        stages(cx, 1000, 5, 0, 3);
    };
    voice(n, v);
}

void resultsScene(TNode* n, const Vo& v) {
    const int len = v.len();
    backdrop(n, len);
    heading(n, len, 7, "PROJECT", "The answer");
    const int tableAt = v.cue("the solutions"), listAt = v.cue("Parse,"), endAt = v.cue("That is how");
    const std::vector<int> stageCues = { v.cue("Parse,"), v.cue("optimize,"), v.cue("match,"), v.cue("join,"), v.cue("project.") };
    const auto closing = txt::later([] {
        return txt::Text::make("SELECT ?person ?work  \xE2\x86\x92  answers", style(40, MUTED, code::font()));
    });
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        const float rows = ramp(t, tableAt, tableAt + 12) + ramp(t, tableAt + 10, tableAt + 22);
        const float lift = 60 * ramp(t, listAt - 20, listAt + 10);
        table(cx, { 560, 260 - lift }, { "?person", "?work" }, { { "Ada Lovelace", "Note G" }, { "Alan Turing", "Computing Machinery and Intelligence" } },
              { 300, 500 }, rows, 2, ramp(t, 0, 20));
        float lit = 0;
        for (int i = 0; i < 5; i++) lit += ramp(t, stageCues[(size_t)i] - 4, stageCues[(size_t)i] + 8);
        stages(cx, 640, 5, lit);
        txt::Reveal r;
        r.stagger = 1;
        r.offset = { 0, 0 };
        txt::scramble(cx, closing(), { 960, 820 }, t - endAt, r, "?{}<>:.SELECTWHERE", 2, { 0.5f, 0.5f });
    };
    voice(n, v);
}

// A soft pad bed in D major, quiet enough to sit under the voice.
void genBed(const std::string& path, double end) {
    using procgen::Score;
    Score s(end);
    const int chords[4][3] = { { 50, 57, 62 }, { 47, 54, 62 }, { 43, 55, 59 }, { 45, 52, 61 } };
    const double bar = 4 * 60.0 / 80;
    int i = 0;
    for (double t = 0; t < end - 3; t += bar, i++) {
        const int* ch = chords[i % 4];
        for (int k = 0; k < 3; k++) s.pad(t, bar * 1.05, ch[k] + 12, 0.05f, 1100, 0.3f + 0.2f * k);
        s.bass(t, bar * 0.9, ch[0] - 12, 0.08f);
        for (int e = 0; e < 4; e++) s.pluck(t + e * bar / 4, ch[(e * 2) % 3] + 24, 0.012f, e % 2 ? 0.3f : 0.7f, 5);
    }
    for (int k = 0; k < 3; k++) s.pad(end - 3, 3, chords[0][k] + 12, 0.06f, 1400, 0.5f);
    media::writeWav(path, s.mix(), Score::RATE);
}

} // namespace

void registerSparql(Registry& out) {
    Comp c;
    c.name = "sparql";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    const std::vector<Vo> vo = loadVo();
    if (vo.size() < 8) return;                 // the script lives in assets/: only from the repo root
    void (*make[])(TNode*, const Vo&) = { introScene, graphScene, queryScene, parseScene, optimizeScene, matchScene, joinScene, resultsScene };
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    for (size_t i = 0; i < vo.size() && i < 8; i++) {
        if (i) ts.transition(Transition::fade(XF));
        make[i](ts.add(vo[i].len()), vo[i]);
    }
    c.duration = ts.end();
    const double seconds = c.duration / 30.0;
    // named by length: the bed follows the voice-over when it is regenerated
    const std::string bedFile = "out/sparql-assets-v1/bed-" + std::to_string(c.duration) + ".wav";
    c.assets = [seconds, bedFile](const Comp&, const Json&) {
        if (std::filesystem::exists(bedFile)) return;
        std::filesystem::create_directories(std::filesystem::path(bedFile).parent_path());
        genBed(bedFile, seconds);
    };
    media::AudioOptions bed;
    bed.volume = 0.35f;
    media::audio(addRoot(c.roots, 0, c.duration), bedFile, bed);
    out.push_back(std::move(c));
}

} // namespace rmtn
