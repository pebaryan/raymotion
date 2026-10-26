// austria_time_travel.cpp -- "austria-time-travel": five thousand years of Austria for its
// National Day, as a trip in a time machine. A year dial and a location readout ride on
// top; between chapters the dial spins through the centuries in a warp. Each chapter shows
// off a different part of the engine:
//
//   today        a 3D globe, a calendar page flipping backwards (cal::page), a decoded title
//   the ice      Alpine terrain, a dawn g3d::Sky, fog, snow, a copper axe in depth of field
//   salt         Hallstatt as a tilt-shift miniature (DOF tilt) under morning mist
//   Rome         geo3d: the globe unrolls into a map, the Danube frontier draws on, a legion
//                marches along it, Vindobona morphs into Vienna (txt::Morph)
//   the name     the deed of 996 typed onto parchment (Filter::paperGrain), the word morphs
//   empire       a tabletop map: the Habsburg lands extruded, the Ottoman siege as a flow arc
//   collapse     the extrusions sink under rain; the republic of 1918 stays
//   1938-45      more than 66,000 lights in the shape of Austria, one for each Jewish victim
//   1945-55      ten years of calendar pages, four occupying powers' flags leave, a masked flag
//   neutrality   a cloth flag waving in 3D (Geometry::update), cal::month ringing 26 October
//   now          the EU on a globe (geo3d::choropleth), Austria's population chart
//   home         fireworks at dusk (fx::burst3d)
//
// Voice-over: assets/vo/austria-time-travel/*.wav, from lines.json by tools/vo_batch.py
// (the author's Qwen3-TTS voice clone). The captions are the "caption" of each line, split
// into sentences. Every chapter lasts as long as its lines; visual cues follow the words.
//
// Music: generated, from fragments of Austrian composers' music (all public domain):
// Johann Strauss II, "An der schoenen blauen Donau" (1866); W. A. Mozart, "Eine kleine
// Nachtmusik" K. 525 (1787) and the Rondo alla Turca from the Sonata K. 331 (1783).
//
// Sources: docs/austria-time-travel-script.md.
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace rmtn {
namespace {

constexpr int W = 1920, H = 1080, FPS = 30;
constexpr int LEAD = 16, GAP = 12, TAIL = 26, XF = 26;   // frames around and between lines; warp length
const Rectangle VIEW{ 0, 0, (float)W, (float)H };
const std::string VO_DIR = "assets/vo/austria-time-travel/";

const Color INK = mkColor(0xf6f1e7), DIM = mkColor(0xb9b2a6), BG = mkColor(0x06080f), AMBER = mkColor(0xe2b04a),
            FLAG_RED = mkColor(0xed2939), BRASS = mkColor(0xd9a441), INKBROWN = mkColor(0x3b2614);

float smooth01(float x) {
    x = std::clamp(x, 0.f, 1.f);
    return x * x * (3 - 2 * x);
}
float ramp(double t, double a, double b) { return smooth01((float)((t - a) / (b - a))); }
float window(double t, double a, double b, double fade = 12) { return ramp(t, a, a + fade) * (1 - ramp(t, b - fade, b)); }

unsigned hash(unsigned x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x;
}
float rnd(unsigned a, unsigned b = 0) { return (hash(a * 9781u + b * 6271u + 17u) & 0xffffff) / 16777216.f; }

float vnoise(float x, float y, unsigned seed) {
    const int ix = (int)std::floor(x), iy = (int)std::floor(y);
    const float fx = x - ix, fy = y - iy, u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    auto h = [seed](int a, int b) { return rnd((unsigned)a * 73856093u ^ (unsigned)b * 19349663u, seed); };
    return (h(ix, iy) * (1 - u) + h(ix + 1, iy) * u) * (1 - v) + (h(ix, iy + 1) * (1 - u) + h(ix + 1, iy + 1) * u) * v;
}
float fbm(float x, float y, unsigned seed) {
    float v = 0, a = 0.5f;
    for (int i = 0; i < 5; i++) { v += a * vnoise(x, y, seed + i); x = x * 2.03f + 7.1f; y = y * 2.03f + 3.3f; a *= 0.5f; }
    return v;
}
// Sharp crests where the noise crosses its middle: mountain ridges.
float ridged(float x, float y, unsigned seed) {
    float v = 0, a = 0.55f;
    for (int i = 0; i < 5; i++) {
        const float n = 1 - std::fabs(2 * vnoise(x, y, seed + i) - 1);
        v += a * n * n;
        x = x * 2.07f + 1.7f; y = y * 2.07f + 9.2f; a *= 0.48f;
    }
    return v;
}

Font serif() { return fonts::load({ "georgia.ttf", "Georgia.ttf", "DejaVuSerif.ttf", "LiberationSerif-Regular.ttf", "Inter-Regular.ttf" }); }
Font serifItalic() { return fonts::load({ "georgiai.ttf", "Georgia Italic.ttf", "DejaVuSerif-Italic.ttf", "LiberationSerif-Italic.ttf", "Inter-Regular.ttf" }); }
Font serifBold() { return fonts::load({ "georgiab.ttf", "Georgia Bold.ttf", "DejaVuSerif-Bold.ttf", "LiberationSerif-Bold.ttf", "Inter-Bold.ttf" }); }
Font bold() { return fonts::ui(true); }
Font regular() { return fonts::ui(false); }

TextStyle style(Font f, float size, Color c, float spacing = 0) {
    TextStyle s;
    s.font = f;
    s.size = size;
    s.color = c;
    s.spacing = spacing;
    return s;
}

// A small label with a leader line up from a point (2D, over a 3D scene).
void pin(const Ctx& cx, Vector2 at, const char* title, const char* sub, float k, float rise = 70, Color accent = AMBER, float lean = 0) {
    if (k <= 0) return;
    Ctx c = cx;
    c.a *= k;
    const float r = smooth01(k * 1.4f);
    const Vector2 top{ at.x + lean * r, at.y - rise * r };
    dr::circle(c, at, 6, accent);
    dr::ring(c, at, 12 + 6 * (1 - k), 2, withAlpha(accent, 0.7f));
    dr::strokeSeg(c, at, top, 2, withAlpha(INK, 0.8f));
    dr::text(bold(), c, title, { top.x, top.y - 52 }, 30, 2, INK, 0.5);
    if (sub && *sub) dr::text(regular(), c, sub, { top.x, top.y - 18 }, 20, 0, DIM, 0.5);
}

// ---------------------------------------------------------------- voice-over

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

struct Vo {
    std::string file, text, caption;
    double seconds = 0;
    int frames() const { return (int)std::ceil(seconds * FPS); }
    // Frames from the start of the line to where `phrase` is spoken, by its place in the text.
    int cue(const char* phrase) const {
        const size_t at = text.find(phrase);
        const double u = at == std::string::npos ? 0 : (double)at / std::max<size_t>(1, text.size());
        return (int)std::round(u * seconds * FPS);
    }
};

std::vector<Vo> loadVo() {
    std::vector<Vo> out;
    std::ifstream in(VO_DIR + "lines.json", std::ios::binary);
    if (!in) return out;                       // run from elsewhere: no script, no comp
    std::stringstream ss;
    ss << in.rdbuf();
    const Json spec = Json::parse(ss.str());
    for (const Json& l : spec["lines"].asArray()) {
        Vo v;
        v.file = VO_DIR + l.str("file", "");
        v.text = l.str("text", "");
        v.caption = l.str("caption", v.text);
        v.seconds = wavSeconds(v.file);
        if (v.seconds <= 0) v.seconds = v.text.size() / 14.0;     // not generated yet: ~14 chars/s
        out.push_back(v);
    }
    return out;
}

// ---------------------------------------------------------------- chapters

struct Chapter {
    std::vector<Vo> lines;
    std::vector<int> at;                       // chapter-local frame each line starts
    int len = 0;
    int start = 0;                             // absolute, set when placed
    std::string place, coords;
    std::vector<std::pair<int, double>> years; // chapter-local frame -> year on the dial (BC < 0)

    explicit Chapter(std::vector<Vo> l) : lines(std::move(l)) {
        int f = LEAD;
        for (const Vo& v : lines) {
            at.push_back(f);
            f += v.frames() + GAP;
        }
        len = f - GAP + TAIL;
    }
    int cue(size_t line, const char* phrase, int shift = 0) const { return at[line] + lines[line].cue(phrase) + shift; }
    int end(size_t line) const { return at[line] + lines[line].frames(); }
};

// ---------------------------------------------------------------- the dial

// A mechanical counter for |year|, digits rolling as the value passes through them.
void odometer(const Ctx& cx, double year, Vector2 right, float size) {
    const double a = std::fabs(year);
    const float cell = size * 0.64f;
    const int digits = std::max(1, (int)std::floor(std::log10(std::max(1.0, a))) + 1);
    const char* era = year < -0.5 ? "BC" : year < 999.5 ? "AD" : "";
    float x = right.x;
    if (*era) {
        dr::text(bold(), cx, era, { x, right.y + size * 0.42f }, size * 0.36f, 3, AMBER, 1);
        x -= dr::measure(bold(), era, size * 0.36f, 3).x + size * 0.18f;
    }
    for (int k = 0; k < digits; k++) {
        const double p = std::pow(10.0, k), base = a / p;
        const int d = (int)std::fmod(std::floor(base), 10.0);
        float frac;
        if (k == 0) frac = (float)(a - std::floor(a));
        else frac = (float)std::clamp(std::fmod(a, p) - (p - 1), 0.0, 1.0);
        const float cxp = x - cell * (k + 0.5f);
        char s0[2] = { (char)('0' + d), 0 }, s1[2] = { (char)('0' + (d + 1) % 10), 0 };
        Ctx c0 = cx, c1 = cx;
        c0.a *= 1 - frac;
        c1.a *= frac;
        dr::text(bold(), c0, s0, { cxp, right.y - frac * size * 0.6f }, size, 0, INK, 0.5);
        if (frac > 0.01f) dr::text(bold(), c1, s1, { cxp, right.y + (1 - frac) * size * 0.6f }, size, 0, INK, 0.5);
    }
}

struct YearKey { double frame, year; bool warp; };

double yearAt(const std::vector<YearKey>& keys, double f) {
    if (keys.empty()) return 2026;
    if (f <= keys.front().frame) return keys.front().year;
    for (size_t i = 1; i < keys.size(); i++) {
        if (f > keys[i].frame) continue;
        const YearKey& a = keys[i - 1];
        const YearKey& b = keys[i];
        const double u = std::clamp((f - a.frame) / std::max(1e-6, b.frame - a.frame), 0.0, 1.0);
        const double e = b.warp ? easeFn(E_IN_OUT_EXPO, u) : easeFn(E_IN_OUT_CUBIC, u);
        return a.year + (b.year - a.year) * e;
    }
    return keys.back().year;
}

// How strongly the time warp shows at absolute frame f (0..1): around every chapter cut.
float warpAt(const std::vector<int>& cuts, double f) {
    float w = 0;
    for (int c : cuts) {
        const double u = (f - (c - 10)) / (XF + 20.0);
        if (u > 0 && u < 1) w = std::max(w, (float)std::pow(std::sin(PI * u), 2));
    }
    return w;
}

// Light streaming past from the middle of the frame, a flash at the peak.
void drawWarp(const Ctx& cx, float w, double f) {
    if (w <= 0.01f) return;
    BeginBlendMode(BLEND_ADDITIVE);
    const Vector2 o{ W / 2.f, H / 2.f };
    for (int i = 0; i < 340; i++) {
        const float ang = rnd((unsigned)i, 1) * 2 * PI, speed = 0.5f + rnd((unsigned)i, 2);
        const float r = std::fmod((float)(rnd((unsigned)i, 3) * 1300 + f * 38 * speed), 1300.f);
        const float len = (40 + 260 * w) * (0.3f + r / 1300) * speed;
        const Vector2 d{ std::cos(ang), std::sin(ang) };
        const Vector2 a{ o.x + d.x * (60 + r), o.y + d.y * (60 + r) };
        const Vector2 b{ a.x + d.x * len, a.y + d.y * len };
        const Color col = i % 3 == 0 ? mkColor(0xffd9a0) : i % 3 == 1 ? mkColor(0xa8c8ff) : WHITE;
        dr::strokeSeg(cx, a, b, 1.5f + 2.5f * rnd((unsigned)i, 4), withAlpha(col, 0.55f * w));
    }
    for (int k = 0; k < 6; k++) dr::circle(cx, o, 80.f + k * 140 * w, withAlpha(mkColor(0xfff2dd), 0.05f * w * w));
    EndBlendMode();
    restoreBlend();
}

// The heads-up display: where we are on the left, when on the right.
void drawHud(const Ctx& cx, const Chapter& ch, int local, double year, float warp) {
    Ctx c = cx;
    dr::rectGrad4(c, 0, 0, W, 210, withAlpha(BG, 0.62f), withAlpha(BG, 0.62f), withAlpha(BG, 0), withAlpha(BG, 0));
    const float k = ramp(local, XF, XF + 18) * (1 - 0.8f * warp);
    Ctx p = c;
    p.a *= k;
    dr::text(regular(), p, "LOCATION", { 96, 58 }, 17, 5, AMBER);
    dr::text(bold(), p, ch.place.c_str(), { 96, 82 + 8 * (1 - k) }, 36, 1, INK);
    dr::text(code::font(false), p, ch.coords.c_str(), { 96, 128 + 8 * (1 - k) }, 19, 0, DIM);
    dr::text(regular(), c, "YEAR", { W - 96.f, 58 }, 17, 5, AMBER, 1);
    odometer(c, year, { W - 96.f, 80 }, 74);
    // The machine's status light: amber at rest, white hot in the warp.
    dr::circle(c, { W - 96.f - 120, 66 }, 5, mixC(mkColor(0xffa628), WHITE, warp));
}

// Captions: each line split into sentences, timed by their share of its characters.
sub::Track captionTrack(const std::vector<Chapter>& chapters) {
    sub::Track tr;
    for (const Chapter& ch : chapters)
        for (size_t i = 0; i < ch.lines.size(); i++) {
            const Vo& v = ch.lines[i];
            std::vector<std::string> parts;
            std::string cur;
            for (size_t k = 0; k < v.caption.size(); k++) {
                cur += v.caption[k];
                const char c = v.caption[k];
                const bool endSentence = (c == '.' || c == '?' || c == '!' || c == ':') && (k + 1 == v.caption.size() || v.caption[k + 1] == ' ');
                // "26 October 1955." keeps its full stop; "4.5" and "9.2" have no space after.
                if (endSentence && cur.size() > 18) {
                    parts.push_back(cur);
                    cur.clear();
                    if (k + 1 < v.caption.size()) k++;
                }
            }
            if (!cur.empty()) {
                if (cur.size() < 14 && !parts.empty()) parts.back() += " " + cur;
                else parts.push_back(cur);
            }
            size_t total = 0;
            for (const auto& p : parts) total += p.size();
            const double from = (ch.start + ch.at[i]) / (double)FPS;
            double t = from;
            for (const auto& p : parts) {
                const double d = v.seconds * p.size() / std::max<size_t>(1, total);
                tr.cues.push_back({ t, t + d + 0.15, p, false });
                t += d;
            }
        }
    return tr;
}

// ---------------------------------------------------------------- shared 3D pieces

// A map texture painted on first use (not when the composition is registered).
g3d::DynamicTexturePtr lazyMap(std::function<globe::Map()> paint) {
    auto tex = std::make_shared<g3d::DynamicTexture>();
    tex->every = 1 << 30;
    tex->make = [paint](int) { return paint().image(); };
    return tex;
}

// A grid mesh with per-vertex positions, normals and colours from `at(i, j)`.
struct GridVertex { Vector3 p, n; Color c; Vector2 uv; };
g3d::GeometryPtr grid(int nx, int ny, std::function<GridVertex(int, int)> at) {
    return std::make_shared<g3d::Geometry>([nx, ny, at] {
        Mesh m{};
        const int verts = (nx + 1) * (ny + 1);
        m.vertexCount = verts;
        m.triangleCount = nx * ny * 2;
        m.vertices = (float*)MemAlloc(verts * 3 * sizeof(float));
        m.normals = (float*)MemAlloc(verts * 3 * sizeof(float));
        m.texcoords = (float*)MemAlloc(verts * 2 * sizeof(float));
        m.colors = (unsigned char*)MemAlloc(verts * 4);
        m.indices = (unsigned short*)MemAlloc(m.triangleCount * 3 * sizeof(unsigned short));
        for (int j = 0; j <= ny; j++)
            for (int i = 0; i <= nx; i++) {
                const int k = j * (nx + 1) + i;
                const GridVertex v = at(i, j);
                m.vertices[k * 3] = v.p.x; m.vertices[k * 3 + 1] = v.p.y; m.vertices[k * 3 + 2] = v.p.z;
                m.normals[k * 3] = v.n.x; m.normals[k * 3 + 1] = v.n.y; m.normals[k * 3 + 2] = v.n.z;
                m.texcoords[k * 2] = v.uv.x; m.texcoords[k * 2 + 1] = v.uv.y;
                m.colors[k * 4] = v.c.r; m.colors[k * 4 + 1] = v.c.g; m.colors[k * 4 + 2] = v.c.b; m.colors[k * 4 + 3] = v.c.a;
            }
        int t = 0;
        for (int j = 0; j < ny; j++)
            for (int i = 0; i < nx; i++) {
                const unsigned short a = (unsigned short)(j * (nx + 1) + i), b = (unsigned short)(a + 1),
                                     c = (unsigned short)(a + nx + 1), d = (unsigned short)(c + 1);
                m.indices[t++] = a; m.indices[t++] = c; m.indices[t++] = b;
                m.indices[t++] = b; m.indices[t++] = c; m.indices[t++] = d;
            }
        UploadMesh(&m, true);
        return m;
    });
}

// A heightfield `size` across, n x n cells, coloured by `color(x, z)`.
g3d::GeometryPtr terrain(float size, int n, std::function<float(float, float)> height, std::function<Color(float, float)> color) {
    return grid(n, n, [=](int i, int j) {
        const float x = (i / (float)n - 0.5f) * size, z = (j / (float)n - 0.5f) * size, e = size / n * 0.5f;
        const Vector3 nn = Vector3Normalize({ height(x - e, z) - height(x + e, z), 2 * e, height(x, z - e) - height(x, z + e) });
        return GridVertex{ { x, height(x, z), z }, nn, color(x, z), { i / (float)n, j / (float)n } };
    });
}

// ================================================================ 1. today

void todayChapter(TNode* n, Chapter& ch) {
    ch.place = "Vienna, Austria";
    ch.coords = "48.21 N  16.37 E";
    const int set = ch.cue(0, "Set the dial"), hold = ch.cue(0, "Hold on");
    ch.years = { { 0, 2026 }, { ch.len - 8, 2026 } };

    auto scene = std::make_shared<g3d::Scene>();
    scene->ambient = 0.25f;
    scene->lights.push_back(g3d::Light::directional({ -4, 2, 5 }, mkColor(0xfff1dc), 1.3f));
    scene->camera.fov = 30;
    g3d::Material earth;
    earth.dynamicMap = lazyMap([] {
        globe::Map map(4096, mkColor(0x0b1a33));
        map.fill(geo::land().merged(), mkColor(0x24344f)).stroke(geo::countries().merged(), mkColor(0x3c5378), 1.4f);
        map.fill(geo::countries().get("AUT").geometry, FLAG_RED);
        map.graticule(15, withAlpha(mkColor(0x3c5378), 0.35f), 1);
        return map;
    });
    earth.roughness = 0.75f;
    scene->add(globe::sphere(1, 160), earth);
    scene->camera.up = [set, hold](int t, g3d::Camera& cam) {
        const float out = ramp(t, set - 10, hold + 30);
        globe::look(cam, { 2.4 - 26 * out + 0.02 * t, 44.6 - 7 * out }, 1.9f + 1.75f * out);
    };
    // Stars behind.
    n->add(0, ch.len)->fn = [](const Ctx& c, int t) {
        for (int i = 0; i < 420; i++) {
            const float tw = 0.55f + 0.45f * std::sin(t * 0.07f + i);
            dr::circle(c, { rnd(i, 1) * W, rnd(i, 2) * H }, 0.6f + 1.4f * rnd(i, 3), withAlpha(WHITE, 0.25f + 0.5f * rnd(i, 4) * tw));
        }
    };
    TNode* g = n->add(0, ch.len);
    g->fn = g3d::draw(scene);

    // The title, decoded.
    auto title = txt::later([] { return txt::Text::make("TIME TRAVEL", style(bold(), 128, INK, 12)); });
    auto sub = txt::later([] { return txt::Text::make("through the history of Austria", style(serifItalic(), 54, AMBER)); });
    n->add(0, ch.len)->fn = [title, sub, hold](const Ctx& cx, int t) {
        Ctx c = cx;
        c.a *= 1 - ramp(t, hold + 6, hold + 26);
        txt::Reveal r;
        r.stagger = 3;
        r.duration = 22;
        r.offset = { 0, 0 };
        txt::scramble(c, title(), { 120, 400 }, t - 8, r, "0123456789<>/\\|=+*#", 2);
        txt::Reveal w;
        w.by = txt::Unit::Word;
        w.stagger = 6;
        w.offset = { 0, 24 };
        txt::draw(c, sub(), { 126, 548 }, t - 40, w);
    };
    // Today's page, then the pages run backwards as the dial is set.
    cal::Clock clock;
    const double today = (double)cal::days({ 2026, 10, 26 });
    clock.key(0, today).key(set, today).key(hold + 20, today - 120);
    n->add(0, ch.len)->fn = [clock, hold](const Ctx& cx, int t) {
        Ctx c = cx;
        c.a *= ramp(t, 20, 34) * (1 - ramp(t, hold + 10, hold + 30));
        cal::Style cs;
        cs.accent = FLAG_RED;
        cal::page(c, { 1440, 330, 330, 390 }, clock.day(t), cs);
    };
}

// ================================================================ 2. the ice

namespace ice {
constexpr float SIZE = 620;
float valleyX(float z) { return 22 * std::sin(z * 0.008f) + 9 * std::sin(z * 0.021f + 1); }
float floorH(float z) { return 18 + 0.08f * (250 - z); }
float height(float x, float z) {
    // A U-shaped glacier valley: a flat floor, walls that steepen, ridges and peaks above.
    const float d = x - valleyX(z), valley = smooth01((std::fabs(d) - 18) / 90);
    const float peaks = 150 * ridged(x * 0.005f + 3, z * 0.005f + 7, 31) + 40 * ridged(x * 0.018f, z * 0.018f + 4, 17) * valley;
    return floorH(z) + valley * (20 + peaks) + 1.2f * fbm(x * 0.09f, z * 0.09f, 4) - 6;
}
Color color(float x, float z) {
    const float e = 1.2f;
    const Vector3 nn = Vector3Normalize({ height(x - e, z) - height(x + e, z), 2 * e, height(x, z - e) - height(x, z + e) });
    const float slope = 1 - nn.y, d = std::fabs(x - valleyX(z)), n = fbm(x * 0.05f, z * 0.05f, 13);
    Color c = mixC(mkColor(0xfbfcff), mkColor(0xe6ecf4), n);                          // snow
    c = mixC(c, mkColor(0x2b2724), smooth01((slope - 0.3f + 0.3f * (n - 0.5f)) / 0.08f));    // rock where it is steep
    c = mixC(mkColor(0xa9c9df), c, smooth01((d - 14) / 10));                           // glacier ice down the middle
    return c;
}
} // namespace ice

void iceChapter(TNode* n, Chapter& ch) {
    ch.place = "\xc3\x96tztal Alps";
    ch.coords = "46.78 N  10.84 E  3,210 m";
    const int found = ch.cue(0, "In nineteen"), axe = ch.cue(0, "his copper axe");
    ch.years = { { 0, -3300 }, { found - 20, -3300 }, { found + 30, 1991 } };

    auto scene = std::make_shared<g3d::Scene>();
    scene->camera.fov = 40;
    scene->camera.farPlane = 900;
    scene->shadowMapSize = 2048;
    scene->fogMode = g3d::FogMode::Exponential;
    scene->fogDensity = 0.0022f;
    scene->fogFromSky = true;
    scene->sky = g3d::Sky::at(8.6f);
    scene->sky.sunLight = 0;
    scene->sky.clouds = 0.45f;
    scene->sky.resolution = 2048;
    scene->up = [](int t, g3d::Scene& self) { self.sky.cloudDrift = { 0.0006f, 0.0002f }; (void)t; };
    g3d::Light sun = g3d::Light::directional({ 0, 0, 0 }, mkColor(0xffe2c0), 2.0f, true);
    {
        const Vector3 d = Vector3Normalize(g3d::Sky::at(8.6f).sunDirection);
        sun.position = Vector3Scale(d, 400);
        sun.target = { 0, 0, 0 };
        sun.shadowExtent = 320;
        sun.shadowBias = 0.4f;
    }
    g3d::Material ground;
    ground.roughness = 0.8f;
    scene->add(terrain(ice::SIZE, 250, ice::height, ice::color), ground);

    // The axe on a slab of glacier ice.
    const float az = -40, ax = ice::valleyX(az) + 3;
    float top = -1e9f;
    for (int i = -3; i <= 3; i++)
        for (int j = -3; j <= 3; j++) top = std::max(top, ice::height(ax + i, az + j));
    const Vector3 axePos{ ax, top + 0.06f, az };
    // A patch of crusted ice and old snow for it to lie on.
    g3d::Material crust;
    crust.roughness = 0.5f;
    scene->add(terrain(16, 80, [top](float x, float z) {
        const float edge = smooth01((std::max(std::fabs(x), std::fabs(z)) - 5) / 3);
        return top - 0.02f + 0.06f * fbm(x * 1.5f, z * 1.5f, 77) - 3 * edge;
    }, [](float x, float z) {
        const float grain = fbm(x * 0.9f + 3, z * 0.9f, 78);
        return mixC(mkColor(0xcfdde8), mkColor(0xf6f8fb), smooth01((grain - 0.4f) / 0.12f));
    }), crust)->position = { ax, 0, az };
    g3d::Object3D* tool = scene->add();
    tool->position = axePos;
    tool->rotation = { 0, 28, 0 };
    g3d::Material wood, copper;
    wood.color = mkColor(0x9a6c3e);
    wood.roughness = 0.85f;
    copper.color = mkColor(0xc27a45);
    copper.metalness = 0.92f;
    copper.roughness = 0.32f;
    g3d::Object3D* haft = tool->add(g3d::Geometry::cylinder(0.014f, 0.6f, 16), wood);
    haft->rotation = { 0, 0, 90 };
    haft->position = { 0, 0.014f, 0 };
    // The knee-shaped end of the haft holds the blade, bound with tar and leather.
    g3d::Object3D* knee = tool->add(g3d::Geometry::box(0.05f, 0.03f, 0.12f), wood);
    knee->position = { 0.31f, 0.016f, 0.04f };
    g3d::Object3D* blade = tool->add(g3d::Geometry::box(0.035f, 0.012f, 0.095f), copper);
    blade->position = { 0.31f, 0.02f, 0.13f };
    g3d::Material leather;
    leather.color = mkColor(0x2a1d14);
    leather.roughness = 0.9f;
    tool->add(g3d::Geometry::box(0.056f, 0.036f, 0.03f), leather)->position = { 0.31f, 0.016f, 0.075f };

    // The sun's shadows close in on the axe for the close-up.
    sun.up = [axePos, axe](int t, g3d::Light& self) {
        const float k = smooth01((t - axe + 50) / 40.f);
        self.target = Vector3Lerp({ 0, 0, 0 }, axePos, k);
        self.position = Vector3Add(self.target, Vector3Scale(Vector3Normalize(g3d::Sky::at(8.6f).sunDirection), 400 - 380 * k));
        self.shadowExtent = 320 - 318.5f * k;
        self.shadowBias = 0.4f - 0.395f * k;
    };
    scene->lights.push_back(sun);

    // Snow, born round the camera as it flies.
    auto camPath = std::make_shared<std::function<Vector3(float)>>([](float u) {
        const float z = 240 - 270 * u;
        return Vector3{ ice::valleyX(z), ice::floorH(z) + 40 - 20 * u, z };
    });
    fx::Emitter3D snow = fx::snow3d(70, 70, 30);
    snow.rate = 30;
    snow.life = { 260, 360 };
    snow.speed = { 0.05f, 0.09f };
    snow.direction = { 0.35f, -1, 0.1f };
    snow.size = { 0.05f, 0.11f };
    snow.maxParticles = 18000;
    snow.prewarm = true;
    const int flyEnd = axe - 30;
    snow.origin = [camPath, flyEnd](float f) {
        const Vector3 p = (*camPath)(std::clamp(f / (float)flyEnd, 0.f, 1.f));
        return Vector3{ p.x, p.y + 20, p.z - 40 };
    };
    scene->add(snow);
    fx::Emitter3D flurry = fx::snow3d(5, 5, 2.5f);
    flurry.size = { 0.012f, 0.025f };
    scene->add(flurry)->position = { axePos.x, axePos.y + 2.2f, axePos.z };

    scene->camera.up = [camPath, flyEnd, axePos](int t, g3d::Camera& cam) {
        const float u = std::clamp(t / (float)flyEnd, 0.f, 1.f), e = 0.5f * u + 0.5f * smooth01(u);
        // Up the glacier towards the peaks at its head.
        const Vector3 p = (*camPath)(e);
        const float za = p.z - 160;
        Vector3 pos = p, target = { ice::valleyX(za), ice::floorH(za) + 34, za };
        // Then down to the ice, close on the axe.
        const float k = smooth01((t - flyEnd + 10) / 70.f);
        const Vector3 closePos{ axePos.x + 0.55f, axePos.y + 0.42f, axePos.z + 0.75f };
        pos = Vector3Lerp(pos, closePos, k);
        target = Vector3Lerp(target, axePos, k);
        cam.position = pos;
        cam.target = target;
        cam.nearPlane = 0.02f + 0.5f * (1 - k);
        cam.dof = k > 0.5f ? g3d::Camera::DepthOfField{ Vector3Distance(pos, axePos), 0.8f, 14, 0 } : g3d::Camera::DepthOfField{};
    };
    n->add(0, ch.len)->fn = g3d::draw(scene);
    n->add(0, ch.len)->fn = [scene, axePos, axe, found](const Ctx& cx, int t) {
        const g3d::Camera cam = g3d::cameraAt(scene->camera, t);
        Vector2 p;
        const Vector3 bladeAt = Vector3Add(axePos, Vector3RotateByAxisAngle({ 0.31f, 0.02f, 0.13f }, { 0, 1, 0 }, 28 * DEG2RAD));
        if (t > axe - 6 && g3d::project(cam, bladeAt, VIEW, p))
            pin(cx, p, "COPPER AXE", "c. 3300 BC  |  yew haft, copper blade", ramp(t, axe - 6, axe + 14), 180);
        Ctx c = cx;
        c.a *= window(t, found, found + 110, 14);
        dr::roundRect(c, 96, 820, 520, 92, 10, withAlpha(BG, 0.6f));
        dr::text(regular(), c, "FOUND 19 SEPTEMBER 1991", { 120, 838 }, 18, 4, AMBER);
        dr::text(bold(), c, "Tisenjoch glacier, 3,210 m", { 120, 866 }, 30, 0, INK);
    };
}

// ================================================================ 3. salt

namespace lake {
constexpr float SIZE = 240, WATER = 0.4f;
float height(float x, float z) {
    const float behind = smooth01((-z - 6) / 46);
    const float wall = behind * (62 * (0.7f + 0.45f * fbm(x * 0.025f + 4, z * 0.025f, 3)) + 30 * ridged(x * 0.012f, z * 0.012f + 2, 8) * behind);
    const float shore = 1.3f + 0.12f * std::max(0.f, -z - 3);
    const float bed = -4 * smooth01((z + 2) / 12);
    const float far = 34 * smooth01((z - 66) / 40) * (0.6f + 0.6f * fbm(x * 0.03f, z * 0.03f, 6));
    return shore + wall + bed + far + 0.35f * fbm(x * 0.25f, z * 0.25f, 5);
}
Color color(float x, float z) {
    const float e = 0.5f, h = height(x, z);
    const Vector3 nn = Vector3Normalize({ height(x - e, z) - height(x + e, z), 2 * e, height(x, z - e) - height(x, z + e) });
    const float slope = 1 - nn.y, n = fbm(x * 0.2f, z * 0.2f, 7);
    Color c = mixC(mkColor(0x6e9748), mkColor(0x355f33), smooth01((h - 3) / 6 + (n - 0.5f)));
    c = mixC(mkColor(0xb9ab86), c, smooth01((h - WATER - 0.2f) / 0.6f));
    c = mixC(c, mkColor(0x8b8379), smooth01((slope - 0.45f) / 0.15f));
    c = mixC(c, mkColor(0xf0f2f4), smooth01((h - 70 + 8 * n) / 6));
    return c;
}
} // namespace lake

void saltChapter(TNode* n, Chapter& ch) {
    ch.place = "Hallstatt, Salzkammergut";
    ch.coords = "47.56 N  13.65 E";
    const int dig = ch.cue(0, "Miners"), age = ch.cue(0, "gave its name");
    ch.years = { { 0, -1400 }, { age - 10, -1400 }, { age + 40, -800 }, { ch.len - 8, -800 } };

    auto scene = std::make_shared<g3d::Scene>();
    scene->camera.fov = 24;
    scene->camera.farPlane = 600;
    scene->shadowMapSize = 2048;
    scene->sky = g3d::Sky::at(9.4f);
    scene->sky.sunLight = 0;
    scene->sky.clouds = 0.4f;
    scene->sky.resolution = 2048;
    scene->fogMode = g3d::FogMode::Exponential;
    scene->fogDensity = 0.06f;
    scene->fogHeight = 0;
    scene->fogFalloff = 0.45f;
    scene->fogFromSky = true;
    scene->up = [](int t, g3d::Scene& self) { self.fogDensity = 0.07f - 0.04f * smooth01(t / 300.f); };
    g3d::Light sun = g3d::Light::directional({ 0, 0, 0 }, mkColor(0xfff0da), 1.5f, true);
    sun.position = Vector3Scale(Vector3Normalize(g3d::Sky::at(9.4f).sunDirection), 160);
    sun.target = { 0, 0, -10 };
    sun.shadowExtent = 60;
    sun.shadowBias = 0.1f;
    scene->lights.push_back(sun);
    g3d::Material ground;
    ground.roughness = 0.95f;
    scene->add(terrain(lake::SIZE, 240, lake::height, lake::color), ground);
    g3d::Material water;
    water.color = mkColor(0x2a5667);
    water.roughness = 0.05f;
    water.castShadow = false;
    scene->add(g3d::Geometry::plane(lake::SIZE, lake::SIZE), water)->position = { 0, lake::WATER, 0 };

    // The village: houses stepped up the shore under the wall, a slender church spire.
    auto box = g3d::Geometry::box(1, 1, 1);
    auto roof = g3d::Geometry::cone(0.8f, 0.75f, 4);
    g3d::Material wall, roofs, darkRoof, spire;
    wall.color = mkColor(0xf1e9da);
    wall.roughness = 0.9f;
    roofs.color = mkColor(0x7a3b2e);
    roofs.roughness = 0.8f;
    darkRoof.color = mkColor(0x4a3a33);
    darkRoof.roughness = 0.8f;
    spire.color = mkColor(0x3d5550);
    spire.roughness = 0.6f;
    for (unsigned i = 0, placed = 0; i < 4000 && placed < 70; i++) {
        const float x = -26 + 52 * rnd(i, 1), z = -4 - 14 * rnd(i, 2), h = lake::height(x, z);
        const float e = 0.5f;
        const float ny = Vector3Normalize({ lake::height(x - e, z) - lake::height(x + e, z), 2 * e, lake::height(x, z - e) - lake::height(x, z + e) }).y;
        if (h < lake::WATER + 0.4f || h > 9 || ny < 0.55f || std::fabs(x - 4) < 2.2f) continue;
        g3d::Object3D* house = scene->add();
        house->position = { x, h - 0.2f, z };
        house->rotation = { 0, (rnd(i, 3) - 0.5f) * 12, 0 };
        const float s = 0.8f + 0.5f * rnd(i, 4), tall = 1.1f + 0.9f * rnd(i, 5);
        g3d::Object3D* body = house->add(box, wall);
        body->scale = { 1.2f * s, tall, 1.0f * s };
        body->position = { 0, tall / 2, 0 };
        g3d::Object3D* r = house->add(roof, rnd(i, 6) < 0.6f ? roofs : darkRoof);
        r->position = { 0, tall + 0.36f, 0 };
        r->rotation = { 0, 45, 0 };
        r->scale = { 1.1f * s, 1, 0.9f * s };
        placed++;
    }
    const float chx = 4, chz = -8, chh = lake::height(chx, chz);
    g3d::Object3D* church = scene->add();
    church->position = { chx, chh - 0.2f, chz };
    g3d::Object3D* nave = church->add(box, wall);
    nave->scale = { 1.8f, 2.4f, 3.6f };
    nave->position = { 0, 1.2f, -1.2f };
    g3d::Object3D* tower = church->add(box, wall);
    tower->scale = { 1.4f, 5.4f, 1.4f };
    tower->position = { 0, 2.7f, 0.9f };
    g3d::Object3D* point = church->add(g3d::Geometry::cone(0.75f, 4.4f, 8), spire);
    point->position = { 0, 7.6f, 0.9f };
    g3d::Object3D* naveRoof = church->add(roof, darkRoof);
    naveRoof->position = { 0, 2.75f, -1.2f };
    naveRoof->rotation = { 0, 45, 0 };
    naveRoof->scale = { 1.6f, 1, 3.2f };
    auto pine = g3d::Geometry::cone(0.6f, 2.6f, 7);
    g3d::Material needles;
    needles.color = mkColor(0x2c4f31);
    needles.roughness = 0.9f;
    for (unsigned i = 0, placed = 0; i < 9000 && placed < 650; i++) {
        const float x = (rnd(i, 11) - 0.5f) * 140, z = -10 - 60 * rnd(i, 12), h = lake::height(x, z);
        if (h < 6 || h > 55 || fbm(x * 0.08f, z * 0.08f, 41) < 0.45f) continue;
        g3d::Object3D* p = scene->add(pine, needles);
        const float s = 0.8f + 0.6f * rnd(i, 13);
        p->position = { x, h + 1.2f * s - 0.2f, z };
        p->scale = { s, s, s };
        placed++;
    }
    const Vector3 village{ 0, 3.5f, -9 };
    scene->camera.up = [village, len = ch.len](int t, g3d::Camera& cam) {
        const float a = (-16 + 22 * t / (float)len) * DEG2RAD;
        cam.target = village;
        cam.target = { village.x, village.y + 5, village.z };
        cam.position = { village.x + 78 * std::sin(a), 17, village.z + 78 * std::cos(a) };
        cam.dof = { Vector3Distance(cam.position, village), 3.0f, 16, -30 };
    };
    n->add(0, ch.len)->fn = g3d::draw(scene);
    n->add(0, ch.len)->fn = [scene, dig, age](const Ctx& cx, int t) {
        const g3d::Camera cam = g3d::cameraAt(scene->camera, t);
        Vector2 p;
        const Vector3 mine{ 14, lake::height(14, -30) + 0.5f, -30 };
        if (g3d::project(cam, mine, VIEW, p))
            pin(cx, p, "SALT MINE", "worked since the 15th century BC", ramp(t, dig, dig + 18), 90);
        Ctx c = cx;
        c.a *= ramp(t, age + 6, age + 26);
        dr::roundRect(c, 96, 820, 600, 92, 10, withAlpha(BG, 0.55f));
        dr::text(regular(), c, "AN AGE OF EUROPE", { 120, 838 }, 18, 4, AMBER);
        dr::text(bold(), c, "The Hallstatt period, c. 800-450 BC", { 120, 866 }, 30, 0, INK);
    };
}

// ================================================================ 4. Rome

// The Danube from its source to the Black Sea (approximate; not in the 1:110m rivers).
const std::vector<geo::LonLat>& danube() {
    static const std::vector<geo::LonLat> pts = {
        { 8.50, 47.95 }, { 9.99, 48.40 }, { 11.42, 48.76 }, { 12.10, 49.02 }, { 13.46, 48.57 }, { 14.29, 48.31 },
        { 15.33, 48.23 }, { 15.60, 48.40 }, { 16.40, 48.22 }, { 17.11, 48.14 }, { 17.65, 47.76 }, { 18.12, 47.75 },
        { 18.74, 47.80 }, { 19.10, 47.76 }, { 19.05, 47.50 }, { 18.88, 46.80 }, { 18.68, 45.99 }, { 19.00, 45.35 },
        { 19.85, 45.25 }, { 20.46, 44.82 }, { 21.40, 44.70 }, { 22.50, 44.65 }, { 22.88, 43.99 }, { 24.40, 43.70 },
        { 25.95, 43.85 }, { 27.27, 44.12 }, { 27.97, 45.27 }, { 28.05, 45.43 }, { 28.80, 45.17 }, { 29.65, 45.15 } };
    return pts;
}
geo::LonLat danubeAt(float u) {
    const auto& p = danube();
    std::vector<double> acc{ 0 };
    for (size_t i = 1; i < p.size(); i++) acc.push_back(acc.back() + geo::angle(p[i - 1], p[i]));
    const double d = std::clamp((double)u, 0.0, 1.0) * acc.back();
    for (size_t i = 1; i < p.size(); i++)
        if (d <= acc[i]) {
            const double k = (d - acc[i - 1]) / std::max(1e-9, acc[i] - acc[i - 1]);
            return { p[i - 1].lon + (p[i].lon - p[i - 1].lon) * k, p[i - 1].lat + (p[i].lat - p[i - 1].lat) * k };
        }
    return p.back();
}

// Aim at `p` on the surface from `dist` above it, leaning back towards the south by `lean`.
void lookAt(g3d::Camera& cam, const geo3d::Surface& s, geo::LonLat p, float dist, float lean) {
    cam.target = s.point(p);
    const Vector3 up = s.up(p), north = s.north(p);
    cam.position = Vector3Add(cam.target, Vector3Scale(Vector3Normalize(Vector3Subtract(Vector3Scale(up, 1 - lean), Vector3Scale(north, lean))), dist));
    cam.upVector = north;
}

void romeChapter(TNode* n, Chapter& ch) {
    ch.place = "Province of Noricum";
    ch.coords = "47.2 N  14.2 E";
    const int north = ch.cue(0, "Then Rome"), frontier = ch.cue(0, "and the Danube"), camp = ch.cue(0, "A legion"),
              today = ch.cue(0, "Today, we call");
    ch.years = { { 0, -15 }, { camp - 10, -15 }, { camp + 30, 100 }, { ch.len - 8, 100 } };

    auto surface = std::make_shared<geo3d::Surface>(geo::Projection::equalEarth(), geo::LonLat{ 14, 46 });
    auto scene = std::make_shared<g3d::Scene>();
    scene->ambient = 0.4f;
    scene->lights.push_back(g3d::Light::directional({ -3, 4, 6 }, mkColor(0xfff0d8), 1.1f));
    scene->camera.fov = 32;
    g3d::Object3D* world = scene->add();
    g3d::Material paper;
    paper.dynamicMap = lazyMap([] {
        globe::Map map(4096, mkColor(0x14313f));
        map.fill(geo::land().merged(), mkColor(0xc3a874));
        map.fill(geo::lakes().merged(), mkColor(0x14313f));
        map.graticule(10, withAlpha(mkColor(0x2c5566), 0.5f), 1);
        return map;
    });
    paper.roughness = 0.9f;
    geo3d::base(*world, surface, paper, 256);
    globe::RouteStyle river;
    river.width = 0.0042f;
    river.color = mkColor(0xff5a3c);
    globe::path(*world, [surface](float u) { return surface->point(danubeAt(u), 0.002f); },
                [frontier](int t) { return ramp(t, frontier, frontier + 70); }, river);
    scene->up = [surface, frontier](int t, g3d::Scene&) { surface->morph = ramp(t, frontier - 30, frontier + 50); };
    const geo::LonLat rome{ 12.48, 41.89 }, vienna{ 16.37, 48.21 };
    scene->camera.up = [surface, north, camp](int t, g3d::Camera& cam) {
        const float k = ramp(t, north - 10, camp + 20);
        const geo::LonLat p{ 12.48 + (16.0 - 12.48) * k, 41.89 + (47.8 - 41.89) * k };
        lookAt(cam, *surface, p, 2.6f - 2.12f * k, 0.15f + 0.3f * k);
    };
    n->add(0, ch.len)->fn = g3d::draw(scene);

    auto vind = txt::later([] { return txt::Text::make("VINDOBONA", style(bold(), 46, INK, 6)); });
    auto wien = txt::later([] { return txt::Text::make("VIENNA", style(bold(), 46, AMBER, 6)); });
    auto morph = txt::later([vind, wien] { return txt::Morph(vind(), wien()); });
    n->add(0, ch.len)->fn = [scene, surface, rome, vienna, north, frontier, camp, today, morph](const Ctx& cx, int t) {
        const g3d::Camera cam = g3d::cameraAt(scene->camera, t);
        Vector2 p;
        auto at = [&](geo::LonLat ll, float h) { return geo3d::screen(cam, *surface, ll, h, VIEW, p); };
        if (at(rome, 0)) pin(cx, p, "ROMA", "", window(t, 10, frontier, 14), 60, FLAG_RED);
        // The legion on the march along the frontier.
        const float march = ramp(t, frontier + 40, frontier + 70);
        if (march > 0) {
            BeginBlendMode(BLEND_ADDITIVE);
            for (int i = 0; i < 70; i++) {
                const float u = 0.17f + std::fmod(i / 70.f * 0.42f + t * 0.0009f, 0.42f);
                if (!at(danubeAt(u), 0.004f)) continue;
                Ctx c = cx;
                c.a *= march;
                dr::circle(c, p, 7, withAlpha(mkColor(0xff6040), 0.25f));
                dr::circle(c, p, 2.6f, mkColor(0xffd2a0));
            }
            EndBlendMode();
            restoreBlend();
        }
        struct Fort { geo::LonLat at; const char *name, *sub; float rise, lean; };
        const Fort forts[] = { { { 16.86, 48.11 }, "CARNUNTUM", "capital of Pannonia", 120, 170 },
                               { { 14.47, 48.21 }, "LAURIACUM", "legion fortress", 110, -150 },
                               { { 14.37, 46.70 }, "VIRUNUM", "capital of Noricum", -60, -40 } };
        for (int i = 0; i < 3; i++)
            if (at(forts[i].at, 0)) pin(cx, p, forts[i].name, forts[i].sub, ramp(t, camp + 8 * i, camp + 8 * i + 16), forts[i].rise, AMBER, forts[i].lean);
        if (at({ 14.6, 47.3 }, 0)) {
            Ctx c = cx;
            c.a *= ramp(t, frontier, frontier + 20) * 0.85f;
            dr::text(serifItalic(), c, "N o r i c u m", { p.x, p.y }, 40, 0, mkColor(0x5a3d1c), 0.5, 0.5);
        }
        if (at(vienna, 0)) {
            const float k = ramp(t, camp, camp + 16);
            if (k > 0) {
                Ctx c = cx;
                c.a *= k;
                dr::circle(c, p, 8, FLAG_RED);
                dr::ring(c, p, 16, 2.5f, withAlpha(FLAG_RED, 0.7f));
                morph().draw(c, { p.x, p.y + 62 }, ramp(t, today, today + 30), { 0.5f, 0.5f });
            }
        }
    };
}

// ================================================================ 5. the name

void nameChapter(TNode* n, Chapter& ch) {
    ch.place = "Neuhofen an der Ybbs";
    ch.coords = "48.06 N  14.85 E";
    const int deed = ch.cue(0, "an emperor's deed"), name = ch.cue(0, "Ostarrichi"), trace = ch.cue(0, "It is the oldest");
    ch.years = { { 0, 996 }, { ch.len - 8, 996 } };

    TNode* sheet = n->add(0, ch.len);
    sheet->isolate().filters = { Filter::paperGrain(0.5f, 1.3f) };
    sheet->fn = [](const Ctx& c, int) {
        dr::rect(c, 0, 0, W, H, mkColor(0xe8d9b5));
        for (int k = 0; k < 14; k++) {
            const float m = k * 18.f;
            dr::roundRect(c, -60 + m, -60 + m, W + 120 - 2 * m, H + 120 - 2 * m, 120, withAlpha(mkColor(0x8a6a3a), 0.05f));
        }
        for (int i = 0; i < 90; i++)        // foxing
            dr::circle(c, { rnd(i, 21) * W, rnd(i, 22) * H }, 6 + 30 * rnd(i, 23), withAlpha(mkColor(0xa07a40), 0.04f + 0.05f * rnd(i, 24)));
    };
    auto l1 = txt::later([] { return txt::Text::make("... in regione vulgari vocabulo Ostarr\xc3\xae" "chi", style(serifItalic(), 62, INKBROWN)); });
    auto l2 = txt::later([] { return txt::Text::make("in marcha et in comitatu Heinrici comitis ...", style(serifItalic(), 62, INKBROWN)); });
    auto from = txt::later([] { return txt::Text::make("Ostarr\xc3\xae" "chi", style(serifBold(), 150, FLAG_RED)); });
    auto to = txt::later([] { return txt::Text::make("\xc3\x96sterreich", style(serifBold(), 150, FLAG_RED)); });
    auto morph = txt::later([from, to] { return txt::Morph(from(), to()); });
    n->add(0, ch.len)->fn = [l1, l2, morph, deed, name, trace](const Ctx& cx, int t) {
        const float rate = 1.1f;
        const double typed = std::max(0.0, (double)(t - deed + 6) * rate);
        const float lift = ramp(t, trace - 8, trace + 24);
        Ctx c = cx;
        c.o.y -= 130 * lift;
        const Vector2 a{ 960, 330 }, b{ 960, 420 };
        if (typed < l1().letters) txt::typewriter(c, l1(), a, typed, false, t, { 0.5f, 0 });
        else {
            const auto w = l1().word(5);
            txt::draw(c, l1(), a, txt::highlight(w.first, w.second, FLAG_RED, ramp(t, name, name + 12), -4, 1.06f), { 0.5f, 0 });
        }
        if (typed > l1().letters) txt::typewriter(c, l2(), b, typed - l1().letters, false, t, { 0.5f, 0 });
        Ctx tr = c;
        tr.a *= ramp(t, name + 20, name + 40);
        dr::text(regular(), tr, "\"... in a region the people call Ostarr\xc3\xae" "chi, in the march and county of Count Henry ...\"",
                 { 960, 532 }, 31, 0, mkColor(0x553b22), 0.5);
        // The word, then the word today.
        Ctx big = cx;
        big.a *= lift;
        morph().draw(big, { 960, 700 }, ramp(t, trace + 30, trace + 70), { 0.5f, 0.5f });
        Ctx note = cx;
        note.a *= ramp(t, deed + 20, deed + 40);
        dr::text(regular(), note, "DEED OF EMPEROR OTTO III  |  BRUCHSAL, 1 NOVEMBER 996  |  BAVARIAN STATE ARCHIVES, MUNICH",
                 { 960, 960 }, 19, 3, mkColor(0x6b5235), 0.5);
        // The seal, pressed.
        const float s = ramp(t, deed + 30, deed + 40);
        if (s > 0) {
            Ctx sc = cx;
            sc.a *= s;
            const Vector2 o{ 1660, 820 };
            const float r = 74 * (1.3f - 0.3f * s);
            dr::blob(sc, o, r, 5, 9, 20, mkColor(0x8e1f1a));
            dr::ring(sc, o, r * 0.72f, 3, mkColor(0x6a1410));
            dr::star(sc, o, r * 0.42f, r * 0.18f, 6, 0, mkColor(0xb13a2c));
        }
    };
}

// ================================================================ 6 + 7. empire and collapse

// The surface's map, laid on a table: map plane (z = radius, facing +z) face up at y = 0.
Vector3 onTable(const geo3d::Surface& s, geo::LonLat p, float h) {
    const Vector3 q = s.point(p, h);
    return { q.x, q.z - s.radius(), -q.y };
}

// A sharper base for one region than geo3d::base's whole-world image: a grid over the box,
// textured with a globe::Map of just that box.
g3d::Object3D* regionBase(g3d::Object3D& parent, geo3d::SurfacePtr s, geo::Bounds b, const g3d::Material& m, int nx, int ny) {
    auto geom = grid(nx, ny, [s, b, nx, ny](int i, int j) {
        const geo::LonLat p{ b.west + (b.east - b.west) * i / nx, b.north - (b.north - b.south) * j / ny };
        return GridVertex{ s->point(p), s->up(p), WHITE, { i / (float)nx, j / (float)ny } };
    });
    return parent.add(geom, m);
}

struct EmpireTimes { int habsburg, siege, census, war, gone, republic, len; };

const char* const CROWN_LANDS[] = { "AUT", "CZE", "HUN", "SVK", "SVN", "HRV", "BIH" };

void empireChapter(TNode* n, Chapter& ch) {
    ch.place = "Vienna, seat of the Habsburgs";
    ch.coords = "48.21 N  16.37 E";
    EmpireTimes T;
    T.habsburg = ch.cue(0, "The Habsburgs");
    T.siege = ch.cue(0, "Vienna survives");
    T.census = ch.cue(0, "By nineteen ten");
    T.war = ch.cue(1, "Four years");
    T.gone = ch.cue(1, "the empire is gone");
    T.republic = ch.cue(1, "a small republic");
    T.len = ch.len;
    ch.years = { { 0, 1278 }, { T.siege - 20, 1282 }, { T.siege + 10, 1683 }, { T.census - 10, 1683 }, { T.census + 30, 1910 },
                 { T.war - 10, 1910 }, { T.war + 10, 1914 }, { T.gone - 30, 1914 }, { T.gone + 20, 1918 }, { ch.len - 8, 1918 } };

    const geo::Bounds box{ 2, 37, 34, 56 };
    auto s = std::make_shared<geo3d::Surface>(geo::Projection::equalEarth(), geo::LonLat{ 17, 47 }, 1.f, 44.f);
    s->morph = 1;
    auto scene = std::make_shared<g3d::Scene>();
    scene->shadowMapSize = 2048;
    scene->camera.fov = 32;
    const g3d::Sky fair = [] { g3d::Sky k = g3d::Sky::at(15.6f); k.clouds = 0.3f; k.sunLight = 0; return k; }();
    const g3d::Sky storm = [] {
        g3d::Sky k = g3d::Sky::at(15.6f);
        k.clouds = 0.95f;
        k.cloudColor = mkColor(0x9aa0aa);
        k.cloudShade = mkColor(0x3e434d);
        k.zenith = mkColor(0x2c323d);
        k.horizon = mkColor(0x6f747c);
        k.sunGlow = 0.05f;
        k.hazeAmount = 0.05f;
        k.sunLight = 0;
        return k;
    }();
    scene->fogMode = g3d::FogMode::Exponential;
    scene->fogFromSky = true;
    scene->up = [fair, storm, T](int t, g3d::Scene& self) {
        const float w = ramp(t, T.war - 10, T.war + 60) * (1 - 0.5f * ramp(t, T.republic, T.republic + 50));
        self.sky = g3d::Sky::mix(fair, storm, w);
        self.sky.cloudDrift = { 0.0006f + 0.002f * w, 0.0002f };
        self.fogDensity = 0.02f + 0.06f * w;
    };
    g3d::Light sun = g3d::Light::directional({ -9, 7, -6 }, mkColor(0xffe2bf), 1.6f, true);
    sun.shadowExtent = 6;
    sun.up = [T](int t, g3d::Light& self) { self.intensity = 1.6f - 1.1f * ramp(t, T.war - 10, T.war + 60) * (1 - 0.5f * ramp(t, T.republic, T.republic + 50)); };
    scene->lights.push_back(sun);
    g3d::Object3D* desk = scene->add();
    desk->rotation = { -90, 0, 0 };
    desk->position = { 0, -1, 0 };
    g3d::Material sheet;
    sheet.dynamicMap = lazyMap([box] {
        globe::Map map(4096, box, mkColor(0x8aa3b5));
        map.fill(geo::land().merged(), mkColor(0xe2d5b8)).stroke(geo::countries().merged(), mkColor(0xa89878), 2.2f);
        map.fill(geo::lakes().merged(), mkColor(0x8aa3b5));
        map.graticule(5, withAlpha(mkColor(0x6f8798), 0.35f), 1.2f);
        return map;
    });
    sheet.roughness = 0.85f;
    regionBase(*desk, s, box, sheet, 120, 80);
    // A table under the map.
    g3d::Material oak;
    oak.color = mkColor(0x5b3a22);
    oak.roughness = 0.55f;
    scene->add(g3d::Geometry::box(30, 0.4f, 22), oak)->position = { 0, -0.215f, 0 };

    // The crown lands, by today's borders: rising one after another from Austria outward,
    // and sinking again in 1918, all but Austria.
    geo3d::Values lands;
    for (const char* k : CROWN_LANDS) lands.push_back({ k, 1 });
    geo3d::ExtrudeStyle ex;
    ex.height = 0.16f;
    ex.scale.min = 0;
    ex.scale.max = 1.2;
    ex.scale.colors = [](float v) { return v > 0.9f ? FLAG_RED : mixC(mkColor(0xb8862e), AMBER, v / 0.85f); };
    ex.detail = 1.5f;
    ex.value = [T](int t, const std::string& key) {
        int idx = 0;
        while (idx < 7 && key != CROWN_LANDS[idx]) idx++;
        const float in = ramp(t, T.habsburg + 10 + 14 * idx, T.habsburg + 40 + 14 * idx);
        if (idx == 0) return (double)(in * (1 + 0.2f * ramp(t, T.republic, T.republic + 30)));
        const float out = ramp(t, T.war + 30 + 18 * idx, T.war + 70 + 18 * idx);
        return (double)(in * (1 - out));
    };
    geo3d::extrude(*desk, s, lands, ex);
    // The siege: an arc from Constantinople that stops short of Vienna, then falls back.
    std::vector<geo3d::Flow> siege{ { { 28.97, 41.01 }, { 16.37, 48.21 }, 1, mkColor(0xb3182b) } };
    geo3d::FlowStyle fs;
    fs.height = 0.5f;
    fs.width = 0.028f;
    fs.head = [T](int t) { return 0.96f * ramp(t, T.siege - 6, T.siege + 40); };
    fs.tail = [T](int t) { return 0.96f * ramp(t, T.siege + 70, T.siege + 110); };
    geo3d::flows(*desk, s, siege, fs);
    // The war's rain.
    fx::Emitter3D rain = fx::rain3d(4.5f, 3.5f, 3);
    rain.from = T.war;
    rain.until = T.republic + 20;
    rain.prewarm = false;
    scene->add(rain)->position = { 0.2f, 3, -0.4f };

    const Vector3 centre = onTable(*s, { 17.5, 47.2 }, 0);
    scene->camera.up = [centre, T](int t, g3d::Camera& cam) {
        const float a = (-24 + 34 * t / (float)T.len) * DEG2RAD, close = 0.15f * ramp(t, T.republic - 20, T.republic + 60);
        const Vector3 aim = Vector3Lerp(centre, Vector3{ centre.x - 0.1f, 0, centre.z + 0.05f }, close);
        cam.target = aim;
        const float d = 4.0f * (1 - close);
        cam.position = { aim.x + d * std::sin(a), 2.5f * (1 - close), aim.z + d * std::cos(a) };
        cam.dof = { Vector3Distance(cam.position, cam.target), 2.0f, 14, -26 };
    };
    n->add(0, ch.len)->fn = g3d::draw(scene);
    n->add(0, ch.len)->fn = [scene, s, T](const Ctx& cx, int t) {
        const g3d::Camera cam = g3d::cameraAt(scene->camera, t);
        Vector2 p;
        if (g3d::project(cam, onTable(*s, { 16.37, 48.21 }, 0.2f), VIEW, p)) {
            pin(cx, p, "VIENNA 1683", "the second Ottoman siege, lifted 12 September", window(t, T.siege + 10, T.siege + 150), 90, mkColor(0xff4a5c));
            pin(cx, p, "REPUBLIC", "German-Austria, proclaimed 12 November 1918", ramp(t, T.republic, T.republic + 16), 110, FLAG_RED);
        }
        Ctx c = cx;
        c.a *= window(t, T.habsburg + 10, T.siege - 10, 14);
        dr::roundRect(c, 96, 820, 560, 92, 10, withAlpha(BG, 0.55f));
        dr::text(regular(), c, "HOUSE OF HABSBURG", { 120, 838 }, 18, 4, AMBER);
        dr::text(bold(), c, "Dukes of Austria from 1282", { 120, 866 }, 30, 0, INK);
        // The census count.
        Ctx k = cx;
        k.a *= window(t, T.census, T.war + 10, 14);
        const double people = 51.39 * ramp(t, T.census, T.census + 50);
        char num[32];
        std::snprintf(num, sizeof num, "%.1f million", people);
        dr::roundRect(k, 96, 780, 640, 132, 10, withAlpha(BG, 0.55f));
        dr::text(regular(), k, "AUSTRIA-HUNGARY, CENSUS OF 1910", { 120, 798 }, 18, 4, AMBER);
        dr::text(bold(), k, num, { 120, 826 }, 54, 0, INK);
        dr::text(regular(), k, "Lands shown by today's borders of the states wholly within it.", { 120, 886 }, 17, 0, DIM);
        // 12 November 1918 on a page.
        Ctx pc = cx;
        pc.a *= ramp(t, T.republic - 4, T.republic + 14);
        cal::Style cs;
        cs.accent = FLAG_RED;
        cal::page(pc, { 1530, 640, 270, 320 }, (double)cal::days({ 1918, 11, 12 }), cs);
    };
}

// ================================================================ 8. 1938-1945

void darkChapter(TNode* n, Chapter& ch) {
    ch.place = "Austria";
    ch.coords = "47.6 N  14.1 E";
    const int annex = ch.cue(0, "In March"), murdered = ch.cue(0, "More than"), each = ch.cue(0, "Each light");
    ch.years = { { 0, 1934 }, { annex - 10, 1934 }, { annex + 20, 1938 }, { each, 1938 }, { ch.len - 8, 1945 } };

    // More than 66,000 points inside Austria, in a fixed order, the first ones scattered.
    auto proj = std::make_shared<geo::Projection>(geo::Projection::conicConformal().parallels(46, 49).rotate({ 13.3, 47.6 }));
    const geo::Geometry& aut = geo::countries().get("AUT").geometry;
    proj->fit({ 200, 250, 1520, 600 }, aut);
    auto pts = txt::later([proj] {
        const geo::Geometry& shape = geo::countries().get("AUT").geometry;
        const geo::Bounds bb = geo::bounds(shape);
        std::vector<Vector2> out;
        for (unsigned i = 0; out.size() < 66000 && i < 2000000; i++) {
            const geo::LonLat p{ bb.west + (bb.east - bb.west) * rnd(i, 31), bb.south + (bb.north - bb.south) * rnd(i, 32) };
            if (geo::contains(shape, p)) out.push_back((*proj)(p));
        }
        return out;
    });
    auto outline = std::make_shared<vg::Path>(proj->lines(aut));
    n->add(0, ch.len)->fn = [pts, outline, annex, murdered, each](const Ctx& cx, int t) {
        dr::rect(cx, 0, 0, W, H, mkColor(0x030306));
        // The country's outline goes out with the annexation.
        Ctx o = cx;
        o.a *= 0.7f * (1 - ramp(t, annex + 10, annex + 50)) + 0.12f * ramp(t, each, each + 40);
        vg::stroke(o, *outline, vg::Stroke(mkColor(0xb9b2a6), 2.5f));
        const float shown = ramp(t, murdered - 10, each + 10);
        const std::vector<Vector2>& all = pts();
        const int count = (int)(all.size() * (0.5f * shown + 0.5f * shown * shown));
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < count; i++) {
            const Vector2 p = all[i];
            const float flick = 0.7f + 0.3f * std::sin(t * 0.09f + i * 1.7f);
            const float age = std::clamp((float)(count - i) / 900.f, 0.f, 1.f);
            dr::rect(cx, p.x - 1, p.y - 1, 2.2f, 2.2f, withAlpha(mkColor(0xffd59a), 0.55f * flick * age));
        }
        EndBlendMode();
        restoreBlend();
        Ctx k = cx;
        k.a *= ramp(t, each + 20, each + 50);
        dr::text(regular(), k, "More than 66,000 Austrian Jews were murdered in the Shoah; the D\xc3\x96W has identified 63,863 of them by name.",
                 { 960, 900 }, 22, 0, DIM, 0.5);
    };
}

// ================================================================ 9. 1945-1955

void drawSovietBadge(const Ctx& c, Vector2 at, float w) {
    dr::roundRect(c, at.x - w / 2 - w * 0.08f, at.y - w / 3 - w * 0.08f, w * 1.16f, w * 2 / 3 + w * 0.16f, w * 0.12f, mkColor(0x262c38));
    dr::rect(c, at.x - w / 2, at.y - w / 3, w, w * 2 / 3, mkColor(0xcc0000));
    const Color y = mkColor(0xffd700);
    const Vector2 o{ at.x - w * 0.26f, at.y - w * 0.08f };
    const float u = w * 0.016f;
    dr::star(c, { o.x, o.y - 10 * u }, 3.2f * u, 1.4f * u, 5, -90, y);
    dr::arc(c, { o.x + 0.5f * u, o.y + 2 * u }, 6.5f * u, 2.2f * u, 200, 400, y);              // the sickle
    dr::strokeSeg(c, { o.x - 5 * u, o.y + 8 * u }, { o.x + 4 * u, o.y - 1 * u }, 2 * u, y);  // the hammer's shaft
    dr::strokeSeg(c, { o.x + 1.5f * u, o.y - 3.5f * u }, { o.x + 6.5f * u, o.y + 1.5f * u }, 3 * u, y);   // and head
}

void freeChapter(TNode* n, Chapter& ch) {
    ch.place = "Vienna, Belvedere";
    ch.coords = "48.19 N  16.38 E";
    const int declared = ch.cue(0, "the Second Republic"), occupy = ch.cue(0, "Four powers"), treaty = ch.cue(0, "until the State"),
              leave = ch.cue(0, "The last soldiers");
    ch.years = { { 0, 1945 }, { occupy, 1945 }, { treaty, 1955 }, { ch.len - 8, 1955 } };

    auto proj = std::make_shared<geo::Projection>(geo::Projection::conicConformal().parallels(46, 49).rotate({ 13.3, 47.6 }));
    const geo::Geometry& aut = geo::countries().get("AUT").geometry;
    proj->fit({ 360, 330, 1200, 470 }, aut);
    auto shape = std::make_shared<vg::Path>(proj->path(aut));
    const Rectangle r = shape->bounds();
    n->add(0, ch.len)->fn = [](const Ctx& c, int) { dr::rect(c, 0, 0, W, H, mkColor(0x0d1118)); };
    // Austria: grey under occupation, the flag once it is free (a mask of the country).
    n->add(0, ch.len)->fn = [shape](const Ctx& c, int) { vg::fill(c, *shape, mkColor(0x3a404b), vg::FillRule::EvenOdd); };
    TNode* flag = n->add(0, ch.len);
    flag->isolate().mask = [shape](const Ctx& c, int) { vg::fill(c, *shape, WHITE, vg::FillRule::EvenOdd); };
    flag->up = [leave](int t, NodeProps& p) { p.opacity = ramp(t, leave + 10, leave + 40); };
    flag->fn = [r, leave](const Ctx& c, int t) {
        const float k = ramp(t, leave + 10, leave + 60);
        const float h = r.height / 3;
        dr::rect(c, r.x, r.y, r.width * k, h, FLAG_RED);
        dr::rect(c, r.x, r.y + h, r.width * k, h, WHITE);
        dr::rect(c, r.x, r.y + 2 * h, r.width * k, h + 1, FLAG_RED);
    };
    n->add(0, ch.len)->fn = [shape](const Ctx& c, int) { vg::stroke(c, *shape, vg::Stroke(withAlpha(INK, 0.6f), 2)); };
    // The four powers round it, leaving in October 1955.
    n->add(0, ch.len)->fn = [occupy, leave](const Ctx& cx, int t) {
        struct Power { const char* code; const char* name; Vector2 at; Vector2 away; };
        const Power pw[4] = { { "us", "United States", { 420, 300 }, { -1, -0.4f } }, { "gb", "United Kingdom", { 1500, 300 }, { 1, -0.4f } },
                              { "fr", "France", { 420, 880 }, { -1, 0.4f } }, { "su", "Soviet Union", { 1500, 880 }, { 1, 0.4f } } };
        for (int i = 0; i < 4; i++) {
            const float in = ramp(t, occupy + 6 * i, occupy + 6 * i + 18), out = ramp(t, leave + 8 * i, leave + 8 * i + 30);
            if (in <= 0 || out >= 1) continue;
            Ctx c = cx;
            c.a *= in * (1 - out);
            const Vector2 at{ pw[i].at.x + pw[i].away.x * 500 * out * out, pw[i].at.y + pw[i].away.y * 500 * out * out };
            if (i == 3) drawSovietBadge(c, at, 150);
            else flags::draw(c, pw[i].code, at, 150, true, in);
            dr::text(regular(), c, pw[i].name, { at.x, at.y + 76 }, 22, 1, DIM, 0.5);
        }
    };
    // Ten years of pages.
    cal::Clock clock;
    clock.key(0, cal::Date{ 1945, 4, 27 }).key(occupy, cal::Date{ 1945, 4, 27 }).key(treaty, cal::Date{ 1955, 5, 15 })
         .key(leave, cal::Date{ 1955, 5, 15 }).key(leave + 40, cal::Date{ 1955, 10, 25 });
    n->add(0, ch.len)->fn = [clock, declared, treaty](const Ctx& cx, int t) {
        Ctx c = cx;
        c.a *= ramp(t, declared - 10, declared + 6);
        cal::Style cs;
        cs.accent = FLAG_RED;
        cal::page(c, { 825, 520, 270, 320 }, clock.day(t), cs);
        Ctx k = cx;
        k.a *= window(t, treaty, treaty + 110, 14);
        dr::text(regular(), k, "STATE TREATY  |  BELVEDERE PALACE, 15 MAY 1955", { 960, 880 }, 20, 3, AMBER, 0.5);
    };
}

// ================================================================ 10 + 12. the flag

// A flag of red-white-red cloth on a pole, waving: the mesh is reshaped every frame.
g3d::Object3D* clothFlag(g3d::Scene& scene, Vector3 pole) {
    static constexpr int NX = 64, NY = 40;
    static constexpr float FW = 3.0f, FH = 2.0f;
    auto shape = grid(NX, NY, [](int i, int j) {
        const float y = FH * (1 - j / (float)NY);
        const Color c = j * 3 < NY || j * 3 >= 2 * NY ? FLAG_RED : WHITE;
        return GridVertex{ { FW * i / NX, y, 0 }, { 0, 0, 1 }, c, { i / (float)NX, j / (float)NY } };
    });
    g3d::Material cloth;
    cloth.roughness = 0.75f;
    cloth.doubleSided = true;
    g3d::Object3D* group = scene.add();
    group->position = pole;
    g3d::Material metal;
    metal.color = mkColor(0xd8d8dc);
    metal.metalness = 0.8f;
    metal.roughness = 0.3f;
    g3d::Object3D* stick = group->add(g3d::Geometry::cylinder(0.05f, 7.5f, 16), metal);
    stick->position = { 0, -3.75f + 0.2f, 0 };
    g3d::Material brass;
    brass.color = BRASS;
    brass.metalness = 0.9f;
    brass.roughness = 0.25f;
    group->add(g3d::Geometry::sphere(0.11f, 16, 24), brass)->position = { 0, 0.32f, 0 };
    g3d::Object3D* f = group->add(shape, cloth);
    f->position = { 0.05f, -FH, 0 };
    f->up = [shape](int t, g3d::Object3D&) {
        shape->update([t](Mesh& m) {
            for (int j = 0; j <= NY; j++)
                for (int i = 0; i <= NX; i++) {
                    const int k = j * (NX + 1) + i;
                    const float x = FW * i / NX, y = FH * (1 - j / (float)NY), pin = std::pow(x / FW, 1.15f);
                    const float ph = x * 2.3f - t * 0.17f + y * 0.6f;
                    const float z = 0.32f * pin * std::sin(ph) + 0.08f * pin * std::sin(ph * 2.1f + 1);
                    const float dzdx = 0.32f * pin * 2.3f * std::cos(ph) + 0.08f * pin * 2.3f * 2.1f * std::cos(ph * 2.1f + 1);
                    const float dzdy = 0.32f * pin * 0.6f * std::cos(ph);
                    m.vertices[k * 3] = x - 0.06f * pin * (1 - std::cos(ph));
                    m.vertices[k * 3 + 1] = y - 0.12f * pin * pin;
                    m.vertices[k * 3 + 2] = z;
                    const Vector3 nn = Vector3Normalize({ -dzdx, -dzdy, 1 });
                    m.normals[k * 3] = nn.x; m.normals[k * 3 + 1] = nn.y; m.normals[k * 3 + 2] = nn.z;
                }
        });
    };
    return group;
}

std::shared_ptr<g3d::Scene> flagScene(std::function<g3d::Sky(int)> sky, std::function<float(int)> sunPower) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->camera.fov = 34;
    scene->camera.farPlane = 800;
    scene->fogMode = g3d::FogMode::Exponential;
    scene->fogDensity = 0.006f;
    scene->fogFromSky = true;
    scene->up = [sky](int t, g3d::Scene& self) { self.sky = sky(t); self.sky.sunLight = 0; };
    g3d::Light sun = g3d::Light::directional({ 0, 0, 0 }, mkColor(0xffe0b8), 1.6f, true);
    sun.shadowExtent = 12;
    sun.up = [sky, sunPower](int t, g3d::Light& self) {
        const Vector3 d = Vector3Normalize(sky(t).sunDirection);
        self.position = Vector3Scale(d, 60);
        self.target = { 0, 0, 0 };
        self.intensity = sunPower(t);
    };
    scene->lights.push_back(sun);
    g3d::Material meadow;
    meadow.color = mkColor(0x56783e);
    meadow.roughness = 0.95f;
    scene->add(terrain(600, 120, [](float x, float z) {
        const float r = std::hypot(x, z);
        return 22 * smooth01((r - 60) / 160) * (0.5f + fbm(x * 0.01f, z * 0.01f, 3)) + 0.5f * fbm(x * 0.05f, z * 0.05f, 4);
    }, [](float x, float z) { return mixC(mkColor(0x5f8742), mkColor(0x3f6033), fbm(x * 0.03f, z * 0.03f, 9)); }), meadow);
    clothFlag(*scene, { 0, 7.5f, 0 });
    return scene;
}

void neutralChapter(TNode* n, Chapter& ch) {
    ch.place = "Parliament, Vienna";
    ch.coords = "48.21 N  16.36 E";
    const int on = ch.cue(0, "On the twenty"), neutral = ch.cue(0, "permanently neutral"), later = ch.cue(0, "Ten years later");
    ch.years = { { 0, 1955 }, { later - 6, 1955 }, { later + 30, 1965 }, { ch.len - 8, 1965 } };
    auto scene = flagScene([](int) { g3d::Sky k = g3d::Sky::at(16.6f); k.clouds = 0.45f; return k; }, [](int) { return 1.6f; });
    scene->camera.up = [len = ch.len](int t, g3d::Camera& cam) {
        const float a = (20 + 16 * t / (float)len) * DEG2RAD;
        cam.target = { 1.4f, 6.4f, 0 };
        cam.position = { 1.4f + 8.5f * std::sin(a), 3.2f, 8.5f * std::cos(a) };
    };
    n->add(0, ch.len)->fn = g3d::draw(scene);
    cal::Clock clock;
    clock.key(0, cal::Date{ 1955, 10, 19 }).key(on + 20, cal::Date{ 1955, 10, 26 });
    n->add(0, ch.len)->fn = [clock, on, neutral, later](const Ctx& cx, int t) {
        Ctx c = cx;
        c.a *= ramp(t, 20, 40);
        cal::Style cs;
        cs.accent = FLAG_RED;
        cal::month(c, { 96, 330, 420, 360 }, clock.day(t), cs);
        Ctx k = cx;
        k.a *= ramp(t, neutral, neutral + 20);
        dr::text(serifBold(), k, "Permanently neutral.", { 96, 740 + 10 * (1 - k.a) }, 64, 0, INK);
        dr::text(regular(), k, "BUNDESVERFASSUNGSGESETZ \xC3\x9C" "BER DIE NEUTRALIT\xC3\x84T \xC3\x96STERREICHS, 26.10.1955",
                 { 100, 822 }, 18, 3, AMBER);
        Ctx l = cx;
        l.a *= ramp(t, later + 10, later + 30);
        dr::text(regular(), l, "NATIONAL DAY SINCE 1965", { 100, 860 }, 18, 3, AMBER);
        (void)on;
    };
}

// ================================================================ 11. now

void nowChapter(TNode* n, Chapter& ch) {
    ch.place = "Austria, European Union";
    ch.coords = "47.6 N  14.1 E";
    const int eu = ch.cue(0, "Austria joins"), country = ch.cue(0, "A country"), today = ch.cue(0, "nine point two");
    ch.years = { { 0, 1995 }, { country, 1995 }, { today + 10, 2026 }, { ch.len - 8, 2026 } };

    auto surface = std::make_shared<geo3d::Surface>(geo::Projection::equalEarth(), geo::LonLat{ 14, 46 });
    auto scene = std::make_shared<g3d::Scene>();
    scene->ambient = 0.35f;
    scene->lights.push_back(g3d::Light::directional({ -3, 3, 6 }, mkColor(0xfff2e0), 1.15f));
    scene->camera.fov = 30;
    g3d::Object3D* world = scene->add();
    g3d::Material paper;
    paper.dynamicMap = lazyMap([] {
        globe::Map map(4096, mkColor(0x0c1b33));
        map.fill(geo::land().merged(), mkColor(0x2b3a52));
        geo3d::Values members;
        for (const char* k : { "BEL", "BGR", "HRV", "CYP", "CZE", "DNK", "EST", "FIN", "FRA", "DEU", "GRC", "HUN", "IRL", "ITA", "LVA",
                               "LTU", "LUX", "NLD", "POL", "PRT", "ROU", "SVK", "SVN", "ESP", "SWE" })
            members.push_back({ k, 1 });
        geo3d::Scale eu1;
        eu1.min = 0;
        eu1.max = 1;
        eu1.colors = [](float) { return mkColor(0x2f5ec4); };
        geo3d::choropleth(map, members, eu1);
        map.stroke(geo::countries().merged(), mkColor(0x5a6f92), 1.3f);
        return map;
    });
    paper.roughness = 0.8f;
    geo3d::base(*world, surface, paper, 256);
    geo3d::ExtrudeStyle ex;
    ex.height = 0.05f;
    ex.scale.min = 0;
    ex.scale.max = 1;
    ex.scale.colors = [](float) { return FLAG_RED; };
    ex.grow = [eu](int t) { return ramp(t, eu, eu + 30); };
    ex.unlit = false;
    geo3d::extrude(*world, surface, { { "AUT", 1 } }, ex);
    scene->camera.up = [surface, len = ch.len](int t, g3d::Camera& cam) {
        const float k = t / (float)len;
        lookAt(cam, *surface, { 18 - 8 * k, 46 + 2 * k }, 1.5f - 0.45f * k, 0.25f + 0.1f * k);
        cam.position = Vector3Add(cam.position, { 0.25f, 0, 0 });
    };
    n->add(0, ch.len)->fn = g3d::draw(scene);
    // Twelve stars round Austria as it joins.
    n->add(0, ch.len)->fn = [scene, surface, eu](const Ctx& cx, int t) {
        const g3d::Camera cam = g3d::cameraAt(scene->camera, t);
        Vector2 p;
        if (!geo3d::screen(cam, *surface, { 14.1, 47.6 }, 0.05f, VIEW, p)) return;
        const float k = window(t, eu, eu + 140, 18);
        for (int i = 0; i < 12; i++) {
            const float a = i * 30 * DEG2RAD - PI / 2 + 0.3f * (1 - k);
            Ctx c = cx;
            c.a *= ramp(t, eu + 2 * i, eu + 2 * i + 12) * k;
            dr::star(c, { p.x + std::cos(a) * 170 * (0.8f + 0.2f * k), p.y + std::sin(a) * 170 * (0.8f + 0.2f * k) }, 15, 6, 5, -90, mkColor(0xffcc00));
        }
    };
    // Population of today's Austria (Statistik Austria).
    n->add(0, ch.len)->fn = [country, today](const Ctx& cx, int t) {
        struct P { int year; double m; };
        const P data[] = { { 1869, 4.50 }, { 1910, 6.65 }, { 1934, 6.76 }, { 1951, 6.93 }, { 2001, 8.03 }, { 2021, 8.97 }, { 2026, 9.22 } };
        const Rectangle box{ 1160, 360, 640, 400 };
        auto px = [&](double y, double m) { return Vector2{ box.x + (float)((y - 1860) / 170) * box.width, box.y + box.height - (float)((m - 3.5) / 6.5) * box.height }; };
        Ctx c = cx;
        c.a *= ramp(t, country - 20, country);
        dr::roundRect(c, box.x - 40, box.y - 110, box.width + 80, box.height + 190, 14, withAlpha(BG, 0.7f));
        dr::text(regular(), c, "PEOPLE IN TODAY'S AUSTRIA, MILLIONS", { box.x, box.y - 80 }, 18, 3, AMBER);
        for (int m = 4; m <= 9; m++) {
            const Vector2 a = px(1860, m), b = px(2030, m);
            dr::strokeSeg(c, a, b, 1, withAlpha(INK, 0.12f));
            char s[8];
            std::snprintf(s, sizeof s, "%d", m);
            dr::text(regular(), c, s, { a.x - 12, a.y - 11 }, 18, 0, DIM, 1);
        }
        for (int y : { 1900, 1950, 2000 }) dr::text(regular(), c, std::to_string(y).c_str(), { px(y, 3.5).x, box.y + box.height + 10 }, 18, 0, DIM, 0.5);
        const float u = ramp(t, country, today + 20);
        const double reach = 1869 + (2026 - 1869) * u;
        for (int i = 1; i < 7; i++) {
            if (data[i - 1].year > reach) break;
            const double y1 = std::min<double>(data[i].year, reach), k = (y1 - data[i - 1].year) / (data[i].year - data[i - 1].year);
            const Vector2 a = px(data[i - 1].year, data[i - 1].m), b = px(y1, data[i - 1].m + (data[i].m - data[i - 1].m) * k);
            dr::strokeSeg(c, a, b, 5, FLAG_RED);
        }
        for (const P& d : data)
            if (d.year <= reach) dr::circle(c, px(d.year, d.m), 7, INK);
        if (u > 0.02f) {
            const Vector2 a = px(1869, 4.5);
            dr::text(bold(), c, "4.5", { a.x + 10, a.y + 8 }, 30, 0, INK);
        }
        if (u > 0.98f) {
            const Vector2 b = px(2026, 9.22);
            dr::text(bold(), c, "9.2", { b.x - 12, b.y - 46 }, 34, 0, INK, 1);
        }
        dr::text(regular(), c, "Censuses 1869-2021; 1 January 2026 (preliminary).", { box.x, box.y + box.height + 44 }, 16, 0, DIM);
    };
}

// ================================================================ 12. home

void homeChapter(TNode* n, Chapter& ch) {
    ch.place = "Vienna, Austria";
    ch.coords = "48.21 N  16.37 E";
    const int happy = ch.cue(0, "Happy");
    ch.years = { { 0, 2026 }, { ch.len, 2026 } };
    auto scene = flagScene([](int t) {
        g3d::Sky k = g3d::Sky::at(19.0f + 0.5f * smooth01(t / 300.f));
        k.clouds = 0.25f;
        k.stars = std::max(k.stars, 0.5f);
        return k;
    }, [](int t) { return 0.9f - 0.6f * smooth01(t / 300.f); });
    for (int i = 0; i < 14; i++) {
        const Color cols[3] = { FLAG_RED, WHITE, AMBER };
        fx::Emitter3D b = fx::burst3d(140, { cols[i % 3], cols[(i + 1) % 3] });
        b.size = { 0.012f, 0.022f };
        b.burstAt = happy - 30 + 16 * i;
        b.seed = 100 + i;
        g3d::Object3D* o = scene->add(b);
        o->position = { -40 + 80 * rnd(i, 51), 34 + 16 * rnd(i, 52), -90 - 40 * rnd(i, 53) };
        o->scale = { 14, 14, 14 };
    }
    scene->camera.up = [len = ch.len](int t, g3d::Camera& cam) {
        const float k = t / (float)len;
        cam.target = { 1.4f, 9.0f + 3 * k, -6 };
        cam.position = { 6.5f - 3 * k, 2.6f, 10.5f + 2 * k };
    };
    n->add(0, ch.len)->fn = g3d::draw(scene);
    auto title = txt::later([] { return txt::Text::make("Alles Gute, \xc3\x96sterreich!", style(serifBold(), 104, INK)); });
    n->add(0, ch.len)->fn = [title, happy](const Ctx& cx, int t) {
        txt::Reveal r;
        r.by = txt::Unit::Letter;
        r.stagger = 1.6f;
        r.duration = 18;
        r.offset = { 0, 40 };
        r.scale = 0.6f;
        Ctx back = cx;
        back.a *= ramp(t, happy - 10, happy + 20);
        dr::rectGrad4(back, 0, 300, W, 420, withAlpha(BG, 0), withAlpha(BG, 0), withAlpha(BG, 0.55f), withAlpha(BG, 0.55f));
        txt::draw(cx, title(), { 960, 430 }, t - happy + 6, r, { 0.5f, 0.5f });
        Ctx c = cx;
        c.a *= ramp(t, happy + 30, happy + 50);
        dr::text(regular(), c, "HAPPY NATIONAL DAY  |  26 OCTOBER 2026", { 960, 520 }, 26, 5, AMBER, 0.5);
        flags::draw(c, "at", { 960, 640 }, 120, true, ramp(t, happy + 30, happy + 50));
    };
    prefab::use(n, std::max(0, ch.len - 120), 120, "raymotion-logo",
                Json::parse(R"({"size": 70, "tagline": "made with raymotion", "layout": "horizontal"})"))->props.pos = { 960, 960 };
}

// ================================================================ the score

using procgen::Score;

// Bowed strings: two detuned saws, vibrato after the attack, a soft low-pass.
void strings(Score& s, double t0, double dur, double midi, float amp, float pan, float bright = 1) {
    const int a = (int)(t0 * Score::RATE), len = (int)((dur + 0.5) * Score::RATE);
    procgen::Svf f1, f2;
    float ph[2] = { 0.13f, 0.61f };
    for (int k = 0; k < len; k++) {
        const double t = (double)k / Score::RATE;
        const double vib = 0.12 * std::sin(TAU * 5.4 * t) * std::min(1.0, std::max(0.0, (t - 0.18) / 0.3));
        float x = 0;
        for (int o = 0; o < 2; o++) {
            const float dt = (float)(procgen::midiHz(midi + vib + (o ? 0.07 : -0.06)) / Score::RATE);
            ph[o] += dt; if (ph[o] >= 1) ph[o] -= 1;
            x += (2 * ph[o] - 1) - procgen::polyBlep(ph[o], dt);
        }
        const float env = (float)(std::min(1.0, t / 0.07) * (t > dur ? std::exp(-(t - dur) * 7) : 1.0));
        const float cut = (float)(900 + 2200 * bright * std::min(1.0, t / 0.25));
        x = f2.lp(f1.lp(x * 0.5f, cut, 0.8f, Score::RATE), cut * 1.3f, 0.9f, Score::RATE);
        if (a + k < s.n) s.put(a + k, x * env * amp * s.duck[a + k], pan, 0.5f, 0.06f);
    }
}

// A soft flute-like line: sine and a little of its octave, breath, vibrato.
void flute(Score& s, double t0, double dur, double midi, float amp, float pan) {
    const int a = (int)(t0 * Score::RATE), len = (int)((dur + 0.3) * Score::RATE);
    double ph = 0;
    procgen::Svf f;
    for (int k = 0; k < len; k++) {
        const double t = (double)k / Score::RATE;
        const double hz = procgen::midiHz(midi + 0.1 * std::sin(TAU * 5 * t) * std::min(1.0, t / 0.4));
        ph += hz / Score::RATE;
        const float breath = f.bp(s.noise(), (float)hz * 2, 0.3f, Score::RATE) * 0.15f;
        const float x = (float)(std::sin(TAU * ph) + 0.18 * std::sin(2 * TAU * ph)) + breath;
        const float env = (float)(std::min(1.0, t / 0.06) * (t > dur ? std::exp(-(t - dur) * 9) : 1.0));
        if (a + k < s.n) s.put(a + k, x * env * amp * s.duck[a + k], pan, 0.45f, 0.1f);
    }
}

// Wind over ice: noise through a slowly wandering band-pass.
void wind(Score& s, double t0, double t1, float amp) {
    procgen::Svf f;
    for (int k = (int)(t0 * Score::RATE); k < (int)(t1 * Score::RATE) && k < s.n; k++) {
        const double t = (double)k / Score::RATE;
        const float fade = (float)(std::min(1.0, (t - t0) / 1.5) * std::min(1.0, (t1 - t) / 1.5));
        const float cut = (float)(650 + 300 * std::sin(t * 0.7) + 150 * std::sin(t * 1.9 + 1));   // 200 to 1100 Hz: never below zero
        const float x = f.bp(s.noise(), cut, 1.2f, Score::RATE);
        s.put(k, x * amp * fade * s.duck[k], 0.5f + 0.3f * (float)std::sin(t * 0.4), 0.4f, 0);
    }
}

// A note in a melody: when (beats from the phrase start), how long (beats), pitch (MIDI).
struct Note { double beat, len; int midi; };

// The Blue Danube waltz's main theme (Johann Strauss II, 1866), in D: the "da da da daa,
// ping ping, ping ping" phrases. 3/4: bars of three beats.
std::vector<Note> danubeTheme(bool minor) {
    const int D4 = 62, Fs4 = minor ? 65 : 66, A4 = 69, Cs4 = 61, E4 = 64, B4 = minor ? 70 : 71, D5 = 74;
    std::vector<Note> out;
    auto phrase = [&](double bar, int a, int b, int c, int top, int echo) {
        out.push_back({ bar * 3 + 0, 1, a });
        out.push_back({ bar * 3 + 1, 1, b });
        out.push_back({ bar * 3 + 2, 1, c });
        out.push_back({ bar * 3 + 3, 2.6, c == top ? c : top });
        out.push_back({ bar * 3 + 7, 0.8, top + 12 });
        out.push_back({ bar * 3 + 8, 0.8, top + 12 });
        out.push_back({ bar * 3 + 10, 0.8, echo });
        out.push_back({ bar * 3 + 11, 0.8, echo });
    };
    phrase(0, D4, Fs4, A4, A4, Fs4 + 12);
    phrase(4, D4, Fs4, A4, A4, 79);                 // ... G G
    phrase(8, Cs4, E4, B4, B4, 79);
    phrase(12, Cs4, E4, B4, B4, Fs4 + 12);
    phrase(16, D4, Fs4, A4, D5, A4 + 12);
    return out;
}
// The chord under each bar of the theme: D (bars 0-7), A7 (8-15), D (16-19).
int danubeRoot(int bar) { return bar >= 8 && bar < 16 ? 45 : 50; }

// Eine kleine Nachtmusik K. 525, the opening (Mozart, 1787), in G: 4/4.
std::vector<Note> nachtmusik() {
    return { { 0, 1, 67 }, { 1.5, 0.5, 62 }, { 2, 1, 67 }, { 3.5, 0.5, 62 },
             { 4, 0.5, 67 }, { 4.5, 0.5, 62 }, { 5, 0.5, 67 }, { 5.5, 0.5, 71 }, { 6, 1, 74 },
             { 8, 1, 72 }, { 9.5, 0.5, 69 }, { 10, 1, 72 }, { 11.5, 0.5, 69 },
             { 12, 0.5, 72 }, { 12.5, 0.5, 69 }, { 13, 0.5, 66 }, { 13.5, 0.5, 69 }, { 14, 1, 62 } };
}

// Rondo alla Turca, K. 331 (Mozart, 1783), the opening, in A minor: 2/4, sixteenths.
std::vector<Note> turca() {
    const double q = 0.25;
    std::vector<Note> out;
    const int run[4][5] = { { 71, 69, 68, 69, 72 }, { 74, 72, 71, 72, 76 }, { 77, 76, 75, 76, 83 }, { 83, 81, 80, 81, 84 } };
    for (int b = 0; b < 4; b++) {
        const int* r = run[b];
        const double at = b * 2;
        if (b == 2) {
            const int full[8] = { 77, 76, 75, 76, 83, 81, 80, 81 };
            for (int i = 0; i < 8; i++) out.push_back({ at + i * q, q, full[i] });
            continue;
        }
        for (int i = 0; i < 4; i++) out.push_back({ at + i * q, q, r[i] });
        out.push_back({ at + 1, 1, r[4] });
    }
    return out;
}

struct Plan {
    double total = 0;
    std::vector<double> cuts;                        // seconds of each chapter's start
    std::vector<std::pair<double, double>> speech;   // spoken spans, seconds
    std::map<std::string, double> at;                // musical cues, seconds
};

void genScore(const std::string& path, const Plan& p) {
    Score s(p.total + 1);
    // The music steps back while the voice speaks.
    for (int i = 0; i < s.n; i++) s.duck[i] = 1;
    for (const auto& sp : p.speech)
        for (int i = (int)((sp.first - 0.25) * Score::RATE); i < (int)((sp.second + 0.4) * Score::RATE) && i < s.n; i++)
            if (i >= 0) s.duck[i] = 0.55f;
    for (int pass = 0; pass < 3; pass++)     // soften the edges
        for (int i = 1; i < s.n; i++) s.duck[i] = std::max(s.duck[i], s.duck[i - 1] - 0.00004f), s.duck[s.n - i] = std::max(s.duck[s.n - i], s.duck[s.n - i + 1] - 0.00004f);
    auto A = [&](const char* k) { return p.at.at(k); };

    // Warps: a rush up into each cut and an impact as we land.
    for (size_t i = 1; i < p.cuts.size(); i++) {
        s.riser(p.cuts[i] - 1.2, p.cuts[i] + 0.35, 0.05f);
        s.impact(p.cuts[i] + XF / 2.0 / FPS, 0.07f);
        for (int k = 0; k < 10; k++) s.bell(p.cuts[i] - 0.6 + k * 0.06, 96 - k * 2, 0.012f, k % 2 ? 0.2f : 0.8f);
    }

    // 1. Today: the Danube theme on a music box, then a rewind as the dial is set.
    {
        const double beat = 0.42;
        const auto theme = danubeTheme(false);
        const double end = A("set");
        for (const Note& n : theme) {
            const double t = 0.4 + n.beat * beat;
            if (t > end) break;
            s.bell(t, n.midi + 12, 0.035f, 0.35f + 0.3f * (n.midi % 2));
        }
        for (int bar = 0; bar * 3 * beat + 0.4 < end; bar++) {
            const double t = 0.4 + bar * 3 * beat;
            s.pad(t, 3 * beat, danubeRoot(bar % 20) + 12, 0.03f, 900, 0.5f);
            s.pad(t, 3 * beat, danubeRoot(bar % 20) + 19, 0.025f, 900, 0.5f);
        }
        // The rewind: the theme backwards, faster and higher.
        double t = end;
        int k = 0;
        for (auto it = theme.rbegin(); it != theme.rend() && t < A("hold") + 0.6; ++it, ++k) {
            s.pluck(t, it->midi + 12 + k * 0.4, 0.03f, k % 2 ? 0.25f : 0.75f, 9);
            t += std::max(0.035, 0.22 * std::pow(0.9, k));
        }
        s.riser(end, A("hold") + 0.6, 0.04f);
    }
    // 2. The ice: wind, open fifths, glints of ice.
    wind(s, p.cuts[1], p.cuts[2] + 1, 0.12f);
    for (double t = p.cuts[1]; t < p.cuts[2] + 1; t += 4.8) {
        s.pad(t, 4.7, 38, 0.05f, 400, 0.5f);
        s.pad(t, 4.7, 45, 0.04f, 500, 0.5f);
    }
    for (int k = 0; k < 16; k++) s.bell(p.cuts[1] + 1 + k * (p.cuts[2] - p.cuts[1] - 1) / 16, 86 + (int)(rnd(k, 61) * 9), 0.012f, rnd(k, 62));
    // 3. Salt: a modal ostinato, plucked, over a drone.
    for (double t = p.cuts[2], b = 0; t < p.cuts[3] + 0.5; t += 0.36, b++) {
        const int notes[8] = { 62, 69, 67, 69, 64, 69, 67, 72 };
        s.pluck(t, notes[(int)b % 8], 0.03f, (int)b % 2 ? 0.3f : 0.7f, 4);
        if ((int)b % 8 == 0) { s.pad(t, 2.8, 50, 0.04f, 600, 0.5f); s.bass(t, 2.6, 38, 0.08f); }
    }
    // 4. Rome: a slow march, low brass.
    {
        const double beat = 60.0 / 92;
        int b = 0;
        for (double t = p.cuts[3]; t < p.cuts[4] + 0.3; t += beat, b++) {
            s.kick(t, b % 2 ? 0.05f : 0.09f);
            if (b % 4 == 3) s.clap(t + beat / 2, 0.02f);
            if (b % 4 == 0) {
                const int roots[4] = { 38, 34, 36, 33 };
                const int r = roots[(b / 4) % 4];
                s.pad(t, beat * 3.9, r + 12, 0.05f, 700, 0.4f);
                s.pad(t, beat * 3.9, r + 19, 0.04f, 700, 0.6f);
                s.bass(t, beat * 3.5, r, 0.1f);
            }
        }
    }
    // 5. The name: harp arpeggios and a distant monastery bell.
    for (double t = p.cuts[4], b = 0; t < p.cuts[5] + 0.5; t += 0.24, b++) {
        const int arp[2][6] = { { 50, 57, 62, 66, 69, 74 }, { 48, 55, 60, 64, 67, 72 } };
        const int* a = arp[((int)b / 12) % 2];
        const int i = (int)b % 12, j = i < 6 ? i : 11 - i;
        s.pluck(t, a[j] + 12, 0.025f, 0.3f + 0.4f * j / 5, 3);
    }
    for (double t = p.cuts[4] + 1; t < p.cuts[5]; t += 3.7) s.bell(t, 50, 0.04f, 0.6f);
    // 6. Empire: Mozart's Nachtmusik on strings; the Rondo alla Turca for the siege; then the
    // Danube waltz in full for 1910.
    {
        const double beat = 60.0 / 128;
        for (int rep = 0; rep < 2; rep++)
            for (const Note& n : nachtmusik()) {
                const double t = A("habsburg") - 0.3 + rep * 16 * beat + n.beat * beat;
                if (t > A("siege") - 0.6) break;
                strings(s, t, n.len * beat * 0.92, n.midi, 0.05f, 0.45f);
                strings(s, t, n.len * beat * 0.92, n.midi - 12, 0.04f, 0.55f);
            }
        const int chord[4][3] = { { 55, 59, 62 }, { 55, 59, 62 }, { 54, 57, 62 }, { 54, 57, 62 } };
        for (int bar = 0; bar < 8; bar++) {
            const double t = A("habsburg") - 0.3 + bar * 4 * beat;
            if (t > A("siege") - 0.6) break;
            for (int k : chord[bar % 4]) s.pad(t, 4 * beat, k, 0.025f, 800, 0.5f);
            s.bass(t, 3.5 * beat, bar % 4 < 2 ? 43 : 38, 0.08f);
        }
        const double tb = 60.0 / 116 / 2;            // a quarter of a 2/4 bar, as written: eighth = tb
        for (int rep = 0; rep < 2; rep++)
            for (const Note& n : turca()) {
                const double t = A("siege") + rep * 8 * tb * 2 + n.beat * tb * 2;
                s.pluck(t, n.midi, 0.06f, 0.4f, 6);
                s.pluck(t, n.midi - 12, 0.03f, 0.6f, 6);
            }
        for (int k = 0; k < 32; k++) {               // the janissary band: drum and cymbal
            const double t = A("siege") + k * tb * 2;
            s.kick(t, k % 2 ? 0.05f : 0.1f);
            if (k % 2) s.hat(t, 0.03f, 0.5f);
            s.pluck(t + tb, k % 4 < 2 ? 57 : 52, 0.025f, 0.5f, 7);
        }
        const double beatW = 0.4;
        const double t0 = A("census") - 0.2;
        for (const Note& n : danubeTheme(false)) {
            const double t = t0 + n.beat * beatW;
            if (t > A("war") - 0.2) break;
            strings(s, t, n.len * beatW * 0.95, n.midi, 0.06f, 0.45f);
            strings(s, t, n.len * beatW * 0.95, n.midi + 12, 0.03f, 0.55f, 1.3f);
        }
        for (int bar = 0; bar < 40; bar++) {         // oom-pah-pah
            const double t = t0 + bar * 3 * beatW;
            if (t > A("war") - 0.2) break;
            const int r = danubeRoot(bar % 20);
            s.bass(t, beatW * 0.9, r - 12, 0.1f);
            for (int k = 1; k < 3; k++) {
                s.pluck(t + k * beatW, r + 16 - (r == 45 ? 1 : 0), 0.025f, 0.35f, 8);
                s.pluck(t + k * beatW, r + 12 + 7, 0.025f, 0.65f, 8);
            }
        }
    }
    // 7. Collapse: the waltz again, in the minor, slowing and sagging until it stops.
    {
        double t = A("war");
        int k = 0;
        for (const Note& n : danubeTheme(true)) {
            const double slow = 0.42 * (1 + 0.035 * k), sag = -0.02 * k;
            if (t > A("gone") + 0.3) break;
            strings(s, t, n.len * slow * 0.95, n.midi + sag, 0.055f, 0.5f, 0.6f);
            if (k % 3 == 0) s.pad(t, 3 * slow, 38 + sag, 0.04f, 500, 0.5f);
            t = A("war") + (n.beat + 1) * 0.42 * (1 + 0.035 * k);
            k++;
        }
        s.impact(A("gone") + 0.4, 0.14f);
        s.pad(A("gone") + 0.4, 3.5, 26, 0.08f, 300, 0.5f);
        // The republic: one clear, quiet chord.
        for (int m : { 62, 66, 69 }) s.pad(A("republic"), 4.5, m, 0.035f, 1200, 0.5f);
        s.bell(A("republic") + 0.2, 74, 0.03f, 0.5f);
    }
    // 8. Silence, but for a low drone and one bell.
    s.pad(p.cuts[7] + 0.5, p.cuts[8] - p.cuts[7] - 1, 26, 0.05f, 220, 0.5f);
    s.bell(A("each"), 62, 0.035f, 0.5f);
    // 9. Free: the theme rising softly out of the dark, then lifting for the treaty.
    {
        const double beatW = 0.5, t0 = p.cuts[8] + 1.2;
        for (const Note& n : danubeTheme(false)) {
            const double t = t0 + n.beat * beatW;
            if (t > p.cuts[9]) break;
            const float grow = (float)std::clamp((t - t0) / (A("treaty") - t0), 0.2, 1.0);
            flute(s, t, n.len * beatW * 0.92, n.midi + 12, 0.04f * grow, 0.5f);
        }
        for (double t = t0; t < p.cuts[9] + 0.5; t += 3 * beatW * 2) {
            const int bar = (int)((t - t0) / (3 * beatW));
            s.pad(t, 6 * beatW, danubeRoot(bar % 20) + 12, 0.035f, 900, 0.5f);
        }
        s.riser(A("leave") - 1.5, A("leave"), 0.03f);
    }
    // 10. Neutrality: the Nachtmusik as a fanfare.
    {
        const double beat = 60.0 / 120;
        for (const Note& n : nachtmusik()) {
            const double t = A("neutral") - 0.5 + n.beat * beat;
            strings(s, t, n.len * beat * 0.92, n.midi, 0.06f, 0.4f, 1.2f);
            strings(s, t, n.len * beat * 0.92, n.midi + 12, 0.035f, 0.6f, 1.2f);
            s.pluck(t, n.midi + 12, 0.02f, 0.5f, 6);
        }
        for (int bar = 0; bar < 4; bar++) {
            const int r = bar < 2 ? 43 : 38;
            s.bass(A("neutral") - 0.5 + bar * 4 * beat, 3.6 * beat, r, 0.1f);
            s.pad(A("neutral") - 0.5 + bar * 4 * beat, 4 * beat, r + 24, 0.03f, 900, 0.5f);
        }
        for (double t = p.cuts[9]; t < A("neutral") - 0.5; t += 4) s.pad(t, 3.9, 55, 0.03f, 700, 0.5f);
    }
    // 11 and 12. Now and home: the full waltz, to the end.
    {
        const double beatW = 0.4, t0 = p.cuts[10] + 0.4;
        const auto theme = danubeTheme(false);
        const double loop = 20 * 3 * beatW;
        for (int rep = 0; rep < 4; rep++)
            for (const Note& n : theme) {
                const double t = t0 + rep * loop + n.beat * beatW;
                if (t > p.total - 3.2) break;
                strings(s, t, n.len * beatW * 0.95, n.midi, 0.06f, 0.45f, 1.2f);
                if (t > p.cuts[11]) strings(s, t, n.len * beatW * 0.95, n.midi + 12, 0.035f, 0.55f, 1.4f);
                if (rep > 0) s.bell(t, n.midi + 24, 0.012f, 0.5f);
            }
        for (int bar = 0; ; bar++) {
            const double t = t0 + bar * 3 * beatW;
            if (t > p.total - 3.2) break;
            const int r = danubeRoot(bar % 20);
            s.bass(t, beatW * 0.9, r - 12, 0.11f);
            for (int k = 1; k < 3; k++) {
                s.pluck(t + k * beatW, r + 16 - (r == 45 ? 1 : 0), 0.025f, 0.35f, 8);
                s.pluck(t + k * beatW, r + 19, 0.025f, 0.65f, 8);
            }
            if (t > p.cuts[11]) s.hat(t + beatW, 0.02f, 0.4f);
        }
        // Fireworks: thumps and crackle.
        for (int i = 0; i < 14; i++) {
            const double t = A("happy") - 1 + 16 * i / 30.0;
            s.kick(t, 0.06f);
            for (int k = 0; k < 14; k++) s.hat(t + 0.25 + k * 0.035 + rnd(i, k) * 0.03, 0.02f, rnd(i, k + 20));
        }
        const double end = p.total - 3.2;
        for (int m : { 50, 54, 57, 62, 66 }) strings(s, end, 2.6, m + 12, 0.05f, 0.5f);
        s.bass(end, 2.8, 38, 0.15f);
        s.bell(end, 86, 0.04f, 0.5f);
    }
    media::writeWav(path, s.mix(), Score::RATE);
}

} // namespace

void registerAustriaTimeTravel(Registry& out) {
    std::vector<Vo> vo = loadVo();
    if (vo.size() < 12) return;               // the script lives in assets/: only from the repo root
    Comp c;
    c.name = "austria-time-travel";
    c.w = W; c.h = H; c.fps = FPS;
    c.background = BG;

    using Make = void (*)(TNode*, Chapter&);
    const std::vector<std::pair<std::vector<int>, Make>> plan = {
        { { 0 }, todayChapter }, { { 1 }, iceChapter }, { { 2 }, saltChapter }, { { 3 }, romeChapter },
        { { 4 }, nameChapter }, { { 5, 6 }, empireChapter }, { { 7 }, darkChapter }, { { 8 }, freeChapter },
        { { 9 }, neutralChapter }, { { 10 }, nowChapter }, { { 11 }, homeChapter } };
    std::vector<Chapter> chapters;
    TransitionSeries ts(c.roots, (float)W, (float)H);
    for (size_t i = 0; i < plan.size(); i++) {
        std::vector<Vo> lines;
        for (int k : plan[i].first) lines.push_back(vo[k]);
        Chapter ch(lines);
        if (i == plan.size() - 1) ch.len += 90;   // the ending breathes
        if (i) ts.transition(Transition::fade(XF));
        TNode* node = ts.add(ch.len);
        ch.start = node->start;
        plan[i].second(node, ch);
        chapters.push_back(std::move(ch));
    }
    c.duration = ts.end();

    // The dial: each chapter's keys, the warps between them.
    std::vector<YearKey> keys;
    std::vector<int> cuts;
    for (size_t i = 0; i < chapters.size(); i++) {
        const Chapter& ch = chapters[i];
        if (i) cuts.push_back(ch.start);
        auto ys = ch.years;
        std::sort(ys.begin(), ys.end());
        if (i && !ys.empty()) ys.insert(ys.begin(), { XF + 4, ys.front().second });
        for (size_t k = 0; k < ys.size(); k++) {
            const int latest = ch.len - XF - 8;
            const double f = ch.start + std::min(latest, std::max(ys[k].first, i ? XF + 4 : 0));
            if (!keys.empty() && f <= keys.back().frame) continue;
            keys.push_back({ f, ys[k].second, i && k == 0 });
        }
        if (!keys.empty()) {   // hold until the warp starts
            const double hold = ch.start + ch.len - XF - 8;
            if (hold > keys.back().frame) keys.push_back({ hold, keys.back().year, false });
        }
    }
    // Overlays, over every chapter: the warp, the HUD, the captions.
    auto chs = std::make_shared<std::vector<Chapter>>(chapters);
    addRoot(c.roots, 0, c.duration)->fn = [cuts](const Ctx& cx, int t) { drawWarp(cx, warpAt(cuts, t), t); };
    addRoot(c.roots, 0, c.duration)->fn = [chs, keys, cuts](const Ctx& cx, int t) {
        size_t i = 0;
        while (i + 1 < chs->size() && t >= (*chs)[i + 1].start + XF / 2) i++;
        drawHud(cx, (*chs)[i], t - (*chs)[i].start, yearAt(keys, t), warpAt(cuts, t));
    };
    sub::Style cap = sub::defaultStyle();
    cap.text.size = 34;
    cap.bottom = 58;
    cap.maxWidth = 1500;
    addRoot(c.roots, 0, c.duration)->fn = sub::captions(captionTrack(chapters), cap);

    // The voice, the music.
    Plan score;
    score.total = c.duration / (double)FPS;
    for (const Chapter& ch : chapters) {
        score.cuts.push_back(ch.start / (double)FPS);
        for (size_t i = 0; i < ch.lines.size(); i++) {
            media::AudioOptions o;
            o.volume = 1;
            media::audio(addRoot(c.roots, ch.start + ch.at[i], ch.lines[i].frames() + 10), ch.lines[i].file, o);
            score.speech.push_back({ (ch.start + ch.at[i]) / (double)FPS, (ch.start + ch.end(i)) / (double)FPS });
        }
    }
    auto mark = [&](size_t chapter, size_t line, const char* phrase) {
        score.at[phrase] = (chapters[chapter].start + chapters[chapter].cue(line, phrase)) / (double)FPS;
    };
    mark(0, 0, "Set the dial"); score.at["set"] = score.at["Set the dial"];
    mark(0, 0, "Hold on"); score.at["hold"] = score.at["Hold on"];
    mark(5, 0, "The Habsburgs"); score.at["habsburg"] = score.at["The Habsburgs"];
    mark(5, 0, "Vienna survives"); score.at["siege"] = score.at["Vienna survives"];
    mark(5, 0, "By nineteen ten"); score.at["census"] = score.at["By nineteen ten"];
    mark(5, 1, "Four years"); score.at["war"] = score.at["Four years"];
    mark(5, 1, "the empire is gone"); score.at["gone"] = score.at["the empire is gone"];
    mark(5, 1, "a small republic"); score.at["republic"] = score.at["a small republic"];
    mark(6, 0, "Each light"); score.at["each"] = score.at["Each light"];
    mark(7, 0, "until the State"); score.at["treaty"] = score.at["until the State"];
    mark(7, 0, "The last soldiers"); score.at["leave"] = score.at["The last soldiers"];
    mark(8, 0, "On the twenty"); score.at["neutral"] = score.at["On the twenty"];
    mark(10, 0, "Happy"); score.at["happy"] = score.at["Happy"];
    const std::string file = "out/austria-time-travel-assets-v1/score-" + std::to_string(c.duration) + ".wav";
    c.assets = [score, file](const Comp&, const Json&) {
        if (std::filesystem::exists(file)) return;
        std::filesystem::create_directories(std::filesystem::path(file).parent_path());
        genScore(file, score);
    };
    media::AudioOptions bed;
    bed.volume = 0.8f;
    media::audio(addRoot(c.roots, 0, c.duration), file, bed);
    out.push_back(std::move(c));
}

} // namespace rmtn
