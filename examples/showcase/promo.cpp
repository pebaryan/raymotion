// promo.cpp -- "raymotion-promo": a 3.5-minute introduction to raymotion for YouTube,
// 1920x1080 at 60 fps. It explains the idea (a video is a function of its frame number),
// the pipeline from C++ to MP4 and the timeline tree, demonstrates sixteen features one
// by one next to the code that drives them, then points at the GitHub repository and
// rolls the credits.
//
// Like own-your-compute, nothing is read from disk that this file did not make: the score
// is synthesised, and the texture, photo, SVG logo, GIF, video clip and OBJ model used by
// the media and 3D chapters are generated into out/promo-assets-v1/ before the first frame is drawn (the
// GIF and the clip need ffmpeg).
//   raymotion render raymotion-promo
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <cstdio>
#include <filesystem>
#include <array>
#include <fstream>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace rmtn {
namespace {

using namespace procgen;

constexpr int FPS = 60;
constexpr int XF = 30;                        // every chapter transition overlaps 30 frames
const std::string ASSETS = "out/promo-assets-v1";   // bump when a generator changes
const std::string REPO = "github.com/pebaryan/raymotion";

const Color BG = mkColor(0x070912), PANEL = mkColor(0x0f1528), PANEL2 = mkColor(0x151d36),
            LINE = mkColor(0x253256), INK = mkColor(0xf2f4ff), DIM = mkColor(0x93a1c0),
            FAINT = mkColor(0x56648a), TEAL = mkColor(0x47dad8), AMBER = mkColor(0xffc365),
            ROSE = mkColor(0xff5fa8), IRIS = mkColor(0x9b8cff), MINT = mkColor(0xa6e86b),
            TYPE = mkColor(0x7fc4ff), COMMENT = mkColor(0x6b7a9c);

// The stage: the right-hand panel each feature chapter demonstrates in.
constexpr float SX = 860, SY = 150, SW = 950, SH = 780;

// ------------------------------------------------------------------ chapters
// Lengths are in frames from one chapter's start to the next (multiples of a 2 s bar at
// 120 bpm, so the score's sections land on the cuts).
struct Chapter { const char* name; int len; };
const Chapter CHAPTERS[] = {
    { "Intro", 600 }, { "The idea", 720 }, { "Under the hood", 960 }, { "The timeline", 720 },
    { "Keyframes & easing", 480 }, { "Springs", 480 }, { "Interpolation", 480 }, { "Vector drawing", 480 },
    { "Supersampling", 480 }, { "Text & layout", 480 }, { "Layers & blending", 480 }, { "Masks", 480 },
    { "Transitions", 480 }, { "Sequencing", 480 }, { "Props", 480 }, { "Prefabs", 480 },
    { "Media", 480 }, { "GPU 3D", 720 }, { "Audio", 480 }, { "Rendering", 480 },
    { "Open source", 720 }, { "Credits", 960 },
};
constexpr int NCH = (int)(sizeof(CHAPTERS) / sizeof(CHAPTERS[0]));
constexpr int FIRST_FEATURE = 4, NFEAT = 16;
constexpr int CH_OPEN = 20, CH_CREDITS = 21;

int chapterStart(int i) {
    int s = 0;
    for (int k = 0; k < i; k++) s += CHAPTERS[k].len;
    return s;
}
int totalFrames() { return chapterStart(NCH); }
int chapterAt(int T) {
    int ch = 0;
    for (int k = 0; k < NCH; k++) if (T >= chapterStart(k)) ch = k;
    return ch;
}

// ------------------------------------------------------------------ small helpers
template <class... A> std::string fmt(const char* f, A... a) {
    char b[256];
    snprintf(b, sizeof b, f, a...);
    return b;
}
Font bold() { return fonts::ui(true); }
Font regular() { return fonts::ui(false); }
Font mono() {
    static bool missing = false;
    if (!missing) {
        try { return fonts::load({ "consola.ttf", "CascadiaMono.ttf", "DejaVuSansMono.ttf", "Menlo.ttc" }); }
        catch (const std::exception&) { missing = true; }
    }
    return fonts::ui(false);
}
void say(const Ctx& cx, Font f, const std::string& s, Vector2 p, float size, Color col,
         double ax = 0, double ay = 0, float spacing = 0) {
    dr::text(f, cx, s.c_str(), p, size, spacing, col, ax, ay);
}
float textWidth(Font f, const std::string& s, float size, float spacing = 0) {
    return MeasureTextEx(f, s.c_str(), size, spacing).x;
}
// 0 -> 1 over `len` frames from t0, easing out.
float rise(double t, double t0, double len = 30) {
    return (float)interpolate(t, { t0, t0 + len }, { 0, 1 }, clamped(ease::out(ease::cubic)));
}
float fall(double t, double t0, double len = 30) { return 1 - rise(t, t0, len); }
float pop(double t, int delay = 0, double stiffness = 120, double damping = 13) {
    SpringOptions o;
    o.fps = FPS; o.delay = delay;
    o.config.stiffness = stiffness; o.config.damping = damping;
    return (float)spring(t, o);
}
void glow(const Ctx& cx, Vector2 p, float r, Color col, float a = 1) {
    DrawCircleGradient(mapP(cx, p), r * cx.s, withAlpha(col, cx.a * a), withAlpha(col, 0));
}
std::string upper(std::string s) {
    for (char& ch : s) ch = (char)std::toupper((unsigned char)ch);
    return s;
}
// A rounded label; returns its width.
float pill(const Ctx& cx, Vector2 p, const std::string& s, Font f, float size, Color fg, Color bg,
           double ax = 0, float padX = 16) {
    const float h = size * 1.75f, w = textWidth(f, s, size) + padX * 2, x = p.x - (float)ax * w;
    dr::roundRect(cx, x, p.y - h / 2, w, h, h / 2, bg);
    say(cx, f, s, { x + padX, p.y }, size, fg, 0, 0.5);
    return w;
}
// A child context: moved to `at` (in cx's space), rotated and scaled on top.
Ctx sub(const Ctx& cx, Vector2 at, float rot = 0, float scale = 1) {
    Ctx k = cx;
    k.o = mapP(cx, at);
    k.rot += rot;
    k.s *= scale;
    return k;
}

// The backdrop every chapter sits on: a dark gradient under a slowly drifting dot grid.
void backdrop(const Ctx& cx, int T) {
    dr::rectGrad4(cx, 0, 0, 1920, 1080, mkColor(0x0c1124), mkColor(0x0a0e1e), BG, BG);
    const float drift = std::fmod(T * 0.2f, 48.f);
    for (int j = 0; j < 23; j++)
        for (int i = 0; i < 42; i++) {
            const float x = i * 48.f - drift + 24, y = j * 48.f + 12;
            const float a = 0.10f + 0.06f * (float)std::sin(i * 0.7 + j * 0.45 + T * 0.01);
            dr::rect(cx, x - 1.2f, y - 1.2f, 2.4f, 2.4f, withAlpha(mkColor(0x6f86c0), a));
        }
}

// The raymotion mark: a ring with a play-head notch and a play triangle.
void logoMark(const Ctx& cx, Vector2 c, float r, float t = 0) {
    dr::ring(cx, c, r, r * 0.18f, TEAL);
    dr::arc(cx, c, r * 1.42f, r * 0.12f, -90 + t, -90 + t + 70, withAlpha(AMBER, 0.9f));
    const Vector2 tri[3] = { { c.x - r * 0.32f, c.y - r * 0.42f }, { c.x + r * 0.5f, c.y }, { c.x - r * 0.32f, c.y + r * 0.42f } };
    dr::fillPoly(cx, tri, 3, INK);
}

// ------------------------------------------------------------------ code, highlighted
struct Tok { std::string s; Color c; };
bool isKeyword(const std::string& w) {
    static const char* K[] = { "auto", "int", "float", "double", "const", "return", "for", "if", "else",
                               "true", "false", "void", "static", "struct", "bool", "char", "using" };
    for (const char* k : K) if (w == k) return true;
    return false;
}
std::vector<Tok> lex(const std::string& s) {
    std::vector<Tok> out;
    const size_t n = s.size();
    size_t i = 0;
    auto push = [&](size_t a, size_t b, Color c) { out.push_back({ s.substr(a, b - a), c }); };
    auto digit = [](char ch) { return std::isdigit((unsigned char)ch) != 0; };
    auto word = [](char ch) { return std::isalnum((unsigned char)ch) != 0 || ch == '_'; };
    if (n && s[0] == '$') { push(0, 1, MINT); i = 1; }
    while (i < n) {
        const char ch = s[i];
        if (ch == '/' && i + 1 < n && (s[i + 1] == '/' || s[i + 1] == '*') && (i == 0 || s[i - 1] != ':')) { push(i, n, COMMENT); break; }
        if (ch == 'R' && i + 2 < n && s[i + 1] == '"' && s[i + 2] == '(') {
            size_t e = s.find(")\"", i + 3);
            e = e == std::string::npos ? n : e + 2;
            push(i, e, AMBER); i = e; continue;
        }
        if (ch == '"') {
            size_t e = i + 1;
            while (e < n && s[e] != '"') e += s[e] == '\\' ? 2 : 1;
            e = std::min(n, e + 1);
            push(i, e, AMBER); i = e; continue;
        }
        if (ch == '-' && i + 2 < n && s[i + 1] == '-' && word(s[i + 2])) {
            size_t e = i + 2;
            while (e < n && (word(s[e]) || s[e] == '-')) e++;
            push(i, e, IRIS); i = e; continue;
        }
        if (digit(ch) || (ch == '.' && i + 1 < n && digit(s[i + 1]))) {
            size_t e = i;
            while (e < n && (word(s[e]) || s[e] == '.')) e++;
            push(i, e, ROSE); i = e; continue;
        }
        if (word(ch)) {
            size_t e = i;
            while (e < n && word(s[e])) e++;
            const std::string w = s.substr(i, e - i);
            size_t k = e;
            while (k < n && s[k] == ' ') k++;
            const Color c = isKeyword(w) ? IRIS : (k < n && s[k] == '(') ? TEAL
                          : std::isupper((unsigned char)w[0]) ? TYPE : INK;
            push(i, e, c); i = e; continue;
        }
        size_t e = i + 1;
        while (e < n && !word(s[e]) && s[e] != '"' && s[e] != '-' && !(s[e] == '/' && s[e - 1] != ':') && s[e] != '.') e++;
        push(i, e, DIM); i = e;
    }
    return out;
}
// Draws highlighted lines from (x, y), revealing only the first `chars` characters
// (newlines count one). Returns where the last visible character ended.
Vector2 drawCode(const Ctx& cx, const std::vector<std::string>& lines, float x, float y, float size,
                 float lineHeight, int chars, bool plainOutput = false) {
    Font f = mono();
    int left = chars;
    Vector2 end{ x, y };
    for (const std::string& line : lines) {
        if (left <= 0) break;
        float px = x;
        const bool plain = plainOutput && (line.empty() || line[0] != '$');
        const std::vector<Tok> toks = plain ? std::vector<Tok>{ { line, DIM } } : lex(line);
        for (const Tok& tk : toks) {
            if (left <= 0) break;
            std::string s = tk.s;
            if ((int)s.size() > left) s = s.substr(0, (size_t)left);
            say(cx, f, s, { px, y }, size, tk.c);
            px += textWidth(f, s, size);
            left -= (int)s.size();
        }
        end = { px, y };
        left -= 1;
        y += lineHeight;
    }
    return end;
}
void codeCard(const Ctx& cx, Rectangle r, const std::vector<std::string>& lines, int chars, const char* file,
              int t, float size = 22) {
    dr::roundRect(cx, r.x - 1.5f, r.y - 1.5f, r.width + 3, r.height + 3, 18, LINE);
    dr::roundRect(cx, r.x, r.y, r.width, r.height, 17, PANEL);
    const Color dots[3] = { mkColor(0xff6b6b), mkColor(0xffc365), mkColor(0x6be07a) };
    for (int k = 0; k < 3; k++) dr::circle(cx, { r.x + 26 + k * 20.f, r.y + 23 }, 6, withAlpha(dots[k], 0.85f));
    say(cx, mono(), file, { r.x + r.width - 22, r.y + 23 }, 17, FAINT, 1, 0.5);
    dr::rect(cx, r.x, r.y + 46, r.width, 1.5f, LINE);
    const float lh = size * 1.5f, y0 = r.y + 66;
    int total = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        total += (int)lines[i].size() + 1;
        if (chars > total - (int)lines[i].size() - 1)
            say(cx, mono(), std::to_string(i + 1), { r.x + 40, y0 + i * lh }, size * 0.8f, withAlpha(FAINT, 0.7f), 1, -0.1);
    }
    const Vector2 end = drawCode(cx, lines, r.x + 56, y0, size, lh, chars);
    if (chars < total + 30 && (t / 18) % 2 == 0) dr::rect(cx, end.x + 2, end.y + 2, size * 0.55f, size * 1.05f, withAlpha(TEAL, 0.9f));
}

// ------------------------------------------------------------------ generated assets
std::string asset(const std::string& file) { return ASSETS + "/" + file; }

// A tiled texture for the 3D box.
void genTiles(const std::string& path) {
    Pix p(512, 512, mkColor(0x1a2238));
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++) {
            const Color c = (i + j) % 2 ? mkColor(0x47dad8) : mkColor(0x9b8cff);
            for (int y = 0; y < 116; y++)
                for (int x = 0; x < 116; x++) {
                    const float n = fbm((i * 128 + x) * 0.05f, (j * 128 + y) * 0.05f, 31, 3);
                    p.blend(i * 128 + 6 + x, j * 128 + 6 + y, mixC(c, mkColor(0x0d1222), 0.25f + 0.35f * n), 1);
                }
            p.disc(i * 128 + 64.f, j * 128 + 64.f, 18, mkColor(0xf2f4ff), 0.9f);
        }
    p.save(path);
}

// A dusk landscape "photo": sky, sun, clouds, three ridges of mountains and a lake.
void genPhoto(const std::string& path) {
    const int W = 1600, H = 900, LAKE = 700;
    Pix p(W, H, BLACK);
    const float sunX = 1080, sunY = 520;
    for (int y = 0; y < LAKE; y++)
        for (int x = 0; x < W; x++) {
            Color c = mixC(mkColor(0x1b2550), mkColor(0xff9f6e), std::pow(y / (float)LAKE, 1.6f));
            const float d = std::sqrt((x - sunX) * (x - sunX) + (y - sunY) * (y - sunY));
            c = mixC(c, mkColor(0xffe2a8), std::exp(-d / 160.f) * 0.9f);
            if (d < 70) c = mixC(c, mkColor(0xfff4dc), std::clamp((70 - d) / 3.f, 0.f, 1.f));
            const float cl = smooth(0.52f, 0.78f, fbm(x * 0.0035f, y * 0.011f, 77, 4)) * (1 - y / (float)LAKE);
            c = mixC(c, mkColor(0xffc2c8), cl * 0.55f);
            p.at(x, y) = c;
        }
    const Color ridge[3] = { mkColor(0x6a4d78), mkColor(0x3d2f57), mkColor(0x1c1830) };
    for (int k = 0; k < 3; k++)
        for (int x = 0; x < W; x++) {
            const float top = 430 + k * 70 - 190 * fbm(x * (0.0022f + k * 0.0012f), k * 9.f, 50 + k, 5);
            for (int y = std::max(0, (int)top); y < LAKE; y++) p.at(x, y) = mixC(ridge[k], p.at(x, y), 0.15f);
        }
    for (int y = LAKE; y < H; y++)
        for (int x = 0; x < W; x++) {
            const int sx = std::clamp(x + (int)(4 * std::sin(y * 0.35f + x * 0.01f)), 0, W - 1);
            const int sy = std::clamp(2 * LAKE - y, 0, LAKE - 1);
            p.at(x, y) = mixC(p.at(sx, sy), mkColor(0x0b1022), 0.35f + 0.4f * (y - LAKE) / (float)(H - LAKE));
        }
    p.save(path);
}

// The logo as an SVG document, so the media chapter can zoom it without blur.
void genLogoSvg(const std::string& path) {
    std::ofstream o(path, std::ios::binary);
    o << R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="400" height="400" viewBox="0 0 400 400">
<defs><linearGradient id="g" x1="0" y1="0" x2="1" y2="1">
<stop offset="0" stop-color="#47dad8"/><stop offset="1" stop-color="#9b8cff"/></linearGradient></defs>
<rect x="0" y="0" width="400" height="400" rx="72" fill="#0f1528"/>
<circle cx="200" cy="200" r="118" fill="none" stroke="url(#g)" stroke-width="26"/>
<path d="M 200 52 A 148 148 0 0 1 340 160" fill="none" stroke="#ffc365" stroke-width="14" stroke-linecap="round"/>
<path d="M 168 146 L 262 200 L 168 254 Z" fill="#f2f4ff"/>
<circle cx="200" cy="200" r="168" fill="none" stroke="#253256" stroke-width="3" stroke-dasharray="6 14"/>
</svg>
)svg";
}

// Streams generated RGBA frames into ffmpeg. False when there is no ffmpeg or it fails.
bool encodeFrames(const std::string& out, const std::string& codecArgs, int w, int h, int rate, int frames,
                  const std::function<void(int, Pix&)>& draw) {
    const std::string ff = media::ffmpeg();
    if (ff.empty()) return false;
    std::string cmd = "\"" + ff + "\" -y -v error -f rawvideo -pix_fmt rgba -s " + std::to_string(w) + "x" +
                      std::to_string(h) + " -r " + std::to_string(rate) + " -i - " + codecArgs + " \"" + out + "\"";
#ifdef _WIN32
    cmd = "\"" + cmd + "\"";
    FILE* pipe = popen(cmd.c_str(), "wb");
#else
    FILE* pipe = popen(cmd.c_str(), "w");
#endif
    if (!pipe) return false;
    Pix px(w, h, BLACK);
    bool ok = true;
    for (int f = 0; f < frames && ok; f++) {
        draw(f, px);
        ok = fwrite(px.px.data(), sizeof(Color), px.px.size(), pipe) == px.px.size();
    }
    return pclose(pipe) == 0 && ok;
}

// A pixel-art loop: eight squares orbiting on a 25 x 25 grid, blown up 8x.
void genGif(const std::string& path) {
    encodeFrames(path, "-vf \"split[a][b];[a]palettegen[p];[b][p]paletteuse\" -loop 0", 200, 200, 20, 40,
                 [](int f, Pix& p) {
        for (int j = 0; j < 25; j++)
            for (int i = 0; i < 25; i++) {
                Color c = mkColor(0x111830);
                for (int k = 0; k < 8; k++) {
                    const float a = (float)(f / 40.0 * 2 * PI + k * PI / 4);
                    const float cx = 12 + 8 * std::cos(a), cy = 12 + 8 * std::sin(a);
                    if (std::fabs(i - cx) < 2.2f && std::fabs(j - cy) < 2.2f) c = ColorFromHSV(k * 45.f + f * 9.f, 0.65f, 1);
                }
                if ((i - 12) * (i - 12) + (j - 12) * (j - 12) < 7) c = mkColor(0xf2f4ff);
                for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x++) p.at(i * 8 + x, j * 8 + y) = c;
            }
    });
}

// A 4 s video clip: a checkered tunnel flying forward.
void genClip(const std::string& path) {
    encodeFrames(path, "-c:v mpeg4 -q:v 2 -pix_fmt yuv420p", 640, 360, 30, 120, [](int f, Pix& p) {
        const float t = f / 30.f;
        for (int y = 0; y < 360; y++)
            for (int x = 0; x < 640; x++) {
                const float dx = (x - 320) / 360.f, dy = (y - 180) / 360.f;
                const float r = std::sqrt(dx * dx + dy * dy) + 1e-4f, a = std::atan2(dy, dx);
                const float u = 0.35f / r + t * 1.5f, v = a / (float)PI * 4 + t * 0.25f;
                const bool check = ((int)std::floor(u * 4) + (int)std::floor(v * 4)) & 1;
                Color c = check ? ColorFromHSV(190 + 60 * std::sin(u * 0.7f), 0.6f, 0.95f) : mkColor(0x101830);
                p.at(x, y) = mixC(mkColor(0x05060c), c, std::clamp(r * 2.2f, 0.f, 1.f));
            }
    });
}

// A cut gem as an OBJ file (flat-shaded: one normal per face), for loadModel.
void genGem(const std::string& path) {
    std::vector<Vector3> v;
    const int N = 8;
    for (int i = 0; i < N; i++) {                                  // table
        const float a = (float)(i * 2 * PI / N);
        v.push_back({ 0.55f * std::cos(a), 0.42f, 0.55f * std::sin(a) });
    }
    for (int i = 0; i < N; i++) {                                  // girdle
        const float a = (float)((i + 0.5) * 2 * PI / N);
        v.push_back({ std::cos(a), 0, std::sin(a) });
    }
    v.push_back({ 0, 0.42f, 0 });                                  // table centre (16)
    v.push_back({ 0, -1.1f, 0 });                                  // culet (17)
    std::vector<std::array<int, 3>> faces;
    for (int i = 0; i < N; i++) {
        const int j = (i + 1) % N;
        faces.push_back({ 16, j, i });                             // table
        faces.push_back({ i, j, N + i });                          // crown
        faces.push_back({ j, N + j, N + i });
        faces.push_back({ N + i, N + j, 17 });                     // pavilion
    }
    std::ofstream o(path, std::ios::binary);
    o << "# generated by promo.cpp\n";
    for (const Vector3& p : v) o << "v " << p.x << " " << p.y << " " << p.z << "\n";
    for (const auto& f : faces) {
        const Vector3 a = v[(size_t)f[0]], b = v[(size_t)f[1]], c = v[(size_t)f[2]];
        const Vector3 u{ b.x - a.x, b.y - a.y, b.z - a.z }, w{ c.x - a.x, c.y - a.y, c.z - a.z };
        Vector3 n{ u.y * w.z - u.z * w.y, u.z * w.x - u.x * w.z, u.x * w.y - u.y * w.x };
        const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        o << "vn " << n.x / len << " " << n.y / len << " " << n.z / len << "\n";
    }
    for (size_t i = 0; i < faces.size(); i++)
        o << "f " << faces[i][0] + 1 << "//" << i + 1 << " " << faces[i][1] + 1 << "//" << i + 1 << " "
          << faces[i][2] + 1 << "//" << i + 1 << "\n";
}

// ------------------------------------------------------------------ score
// 120 bpm in E minor (Em-C-G-D), timed to the chapters: an ambient intro with a bell on
// each title letter, a groove that builds through the explanation, the full groove under
// the features (a breakdown opens the 3D chapter), a lift on the repository, and an E
// major pad under the credits.
int letterPop(int i) { return 80 + i * 16; }
void genScore(const std::string& path) {
    auto sec = [](int ch) { return chapterStart(ch) / (double)FPS; };
    Score s(totalFrames() / (double)FPS);
    struct Chord { int root; int pad[3]; int arp[4]; };
    const Chord em{ 40, { 64, 67, 71 }, { 76, 79, 83, 79 } }, cM{ 36, { 60, 64, 67 }, { 72, 76, 79, 76 } },
                gM{ 43, { 62, 67, 71 }, { 74, 79, 83, 79 } }, dM{ 38, { 62, 66, 69 }, { 74, 78, 81, 78 } },
                eM{ 40, { 64, 68, 71 }, { 76, 80, 83, 88 } };
    const Chord prog[4] = { em, cM, gM, dM };
    const double IDEA = sec(1), HOOD = sec(2), TREE = sec(3), FEAT = sec(FIRST_FEATURE), GPU = sec(17),
                 OPEN = sec(CH_OPEN), CRED = sec(CH_CREDITS);

    // Intro: an Em9 swell, a bell per letter, a riser into the first cut.
    s.pad(0, IDEA - 0.3, 52, 0.18f, 700, 0.4f); s.pad(0, IDEA - 0.3, 59, 0.14f, 700, 0.6f);
    s.pad(0.5, IDEA - 0.8, 66, 0.10f, 650); s.pad(3, IDEA - 3.3, 71, 0.06f, 900, 0.55f);
    const int pent[9] = { 76, 79, 81, 83, 86, 88, 91, 93, 95 };
    for (int i = 0; i < 9; i++) s.bell(letterPop(i) / (double)FPS, pent[i], 0.11f, 0.2f + 0.075f * i);
    s.riser(IDEA - 2.5, IDEA, 0.2f);
    s.impact(IDEA, 0.55f);

    for (double t = IDEA; t < OPEN - 1e-6; t += 2) {
        const int bar = (int)std::lround((t - IDEA) / 2);
        const Chord& ch = prog[bar % 4];
        const bool hood = t >= HOOD - 1e-6, tree = t >= TREE - 1e-6, feat = t >= FEAT - 1e-6;
        const bool breakdown = t >= GPU - 1e-6 && t < GPU + 4 - 1e-6;
        for (int p : ch.pad) s.pad(t, 1.9, p, feat ? 0.085f : 0.10f, feat ? 1700.f : 1100.f);
        const int lift = feat && (bar / 8) % 2 ? 12 : 0;
        for (int e = 0; e < 8; e++) s.pluck(t + e * 0.25, ch.arp[e % 4] + lift, hood ? 0.09f : 0.07f, e % 2 ? 0.3f : 0.7f);
        if (breakdown) continue;
        if (hood) for (int e = 0; e < 8; e++) s.bass(t + e * 0.25, 0.2, ch.root + (e % 4 == 3 ? 12 : 0), 0.26f);
        else { s.bass(t, 0.9, ch.root, 0.22f); s.bass(t + 1, 0.9, ch.root, 0.2f); }
        if (hood) for (int b = 0; b < 4; b++) s.kick(t + b * 0.5, 0.8f);
        else { s.kick(t, 0.5f); s.kick(t + 1, 0.45f); }
        if (hood) for (int h = 0; h < (feat && bar % 8 >= 4 ? 8 : 4); h++)
            s.hat(t + (feat && bar % 8 >= 4 ? h * 0.25 : h * 0.5 + 0.25), 0.12f, h % 2 ? 0.35f : 0.65f);
        if (tree) { s.clap(t + 0.5, 0.3f); s.clap(t + 1.5, 0.3f); }
    }
    for (int i = FIRST_FEATURE; i <= CH_OPEN; i++) s.riser(sec(i) - 0.6, sec(i), 0.08f);   // a swish into each cut
    s.impact(GPU, 0.4f);
    s.riser(GPU + 2, GPU + 4, 0.14f);

    // Open source: the lift, C-D-Em-C-D-E.
    const Chord lift[6] = { cM, dM, em, cM, dM, eM };
    s.impact(OPEN, 0.65f);
    for (int i = 0; i < 6; i++) {
        const double t = OPEN + i * 2;
        const Chord& ch = lift[i];
        for (int p : ch.pad) { s.pad(t, 1.9, p, 0.11f, 2300); s.pad(t, 1.9, p - 12, 0.07f, 1300, 0.6f); }
        for (int e = 0; e < 8; e++) s.pluck(t + e * 0.25, ch.arp[e % 4] + (i >= 4 ? 12 : 0), 0.09f, e % 2 ? 0.3f : 0.7f);
        for (int e = 0; e < 8; e++) s.bass(t + e * 0.25, 0.2, ch.root, 0.26f);
        for (int b = 0; b < 4; b++) s.kick(t + b * 0.5, 0.75f);
        s.clap(t + 0.5, 0.3f); s.clap(t + 1.5, 0.3f);
        for (int h = 0; h < 8; h++) s.hat(t + h * 0.25, 0.11f, h % 2 ? 0.35f : 0.65f);
    }
    // Credits: E major, held, with sparse bells.
    s.riser(CRED - 1.5, CRED, 0.12f);
    s.impact(CRED, 0.45f);
    for (int p : eM.pad) { s.pad(CRED, 13.5, p, 0.13f, 2000, 0.35f); s.pad(CRED, 13.5, p - 12, 0.09f, 1100, 0.65f); }
    s.bass(CRED, 6, 40, 0.24f);
    const int bells[6] = { 88, 83, 80, 76, 83, 88 };
    for (int i = 0; i < 6; i++) s.bell(CRED + 0.5 + i * 2.0, bells[i], 0.10f, 0.25f + 0.1f * i);
    media::writeWav(path, s.mix(), Score::RATE);
}

bool haveAsset(const std::string& file) { return std::filesystem::exists(asset(file)); }

void ensureAssets() {
    std::filesystem::create_directories(ASSETS);
    auto make = [](const std::string& file, void (*gen)(const std::string&)) {
        if (!haveAsset(file)) gen(asset(file));
    };
    make("tiles.png", genTiles);
    make("photo.png", genPhoto);
    make("logo.svg", genLogoSvg);
    make("gem.obj", genGem);
    make("loop.gif", genGif);
    make("clip.mp4", genClip);
    make("score.wav", genScore);
}

// ------------------------------------------------------------------ a tiny scene
// The same little animation is drawn at any size: as the "frame" being explained, as
// filmstrip thumbnails, as packets in the pipeline. Authored in 320 x 180 units.
void miniScene(const Ctx& cx, Rectangle r, int T, bool label = true) {
    Ctx k = sub(cx, { r.x, r.y }, 0, r.width / 320.f);
    const float hue = 225 + 25 * std::sin(T * 0.004f);
    dr::rectGrad4(k, 0, 0, 320, 180, ColorFromHSV(hue, 0.55f, 0.24f), ColorFromHSV(hue + 30, 0.5f, 0.2f),
                  ColorFromHSV(hue + 30, 0.6f, 0.1f), ColorFromHSV(hue, 0.6f, 0.12f));
    dr::rect(k, 0, 150, 320, 2, withAlpha(INK, 0.25f));
    const float bx = 30 + std::fmod(T * 1.1f, 260.f), by = 150 - 15 - std::fabs(std::sin(T * 0.06f)) * 85;
    dr::circle(k, { bx, 150 }, 12 * (1 - (150 - by) / 160), withAlpha(BLACK, 0.35f));
    dr::circle(k, { bx, by }, 15, TEAL);
    dr::star(k, { 262, 50 }, 22, 9, 5, T * 2.f, AMBER);
    if (label) say(k, mono(), fmt("f %04d", T), { 10, 8 }, 15, withAlpha(INK, 0.85f));
}

// ================================================================== intro
// A timeline draws across the screen; its play-head sweeps and pops each letter of the
// title up from behind the line; a light sweep crosses the title.
void titleLetters(const Ctx& cx, int t, Color col, bool springs) {
    static const char* TITLE = "RAYMOTION";
    Font f = bold();
    const float size = 190, gap = 10;
    float x = 960 - textWidth(f, TITLE, size, gap) / 2;
    for (int i = 0; TITLE[i]; i++) {
        const char s[2] = { TITLE[i], 0 };
        const float k = springs ? pop(t, letterPop(i), 170, 11) : 1;
        say(cx, f, s, { x, 372 + 170 * (1 - k) }, size, col);
        x += textWidth(f, s, size) + gap;
    }
}
float letterCenter(int i) {
    static const char* TITLE = "RAYMOTION";
    Font f = bold();
    const float size = 190, gap = 10;
    float x = 960 - textWidth(f, TITLE, size, gap) / 2;
    for (int k = 0; k < i; k++) { const char s[2] = { TITLE[k], 0 }; x += textWidth(f, s, size) + gap; }
    const char s[2] = { TITLE[i], 0 };
    return x + textWidth(f, s, size) / 2;
}
void sceneIntro(TNode* s, int len, int abs0) {
    const float LY = 590;
    s->add(0, len)->fn = [abs0](const Ctx& cx, int t) {
        backdrop(cx, abs0 + t);
        for (int i = 0; i < 90; i++) {                       // drifting dust
            const float x = (float)rnd01(501, i) * 1920, sp = 0.2f + (float)rnd01(502, i) * 0.5f;
            const float y = 1100 - std::fmod((float)rnd01(503, i) * 1100 + t * sp, 1150.f);
            glow(cx, { x + 20 * std::sin(t * 0.01f + i), y }, 10 + 14 * (float)rnd01(504, i), TEAL,
                 0.25f * rise(t, 0, 60));
        }
    };
    // The timeline: a line with frame ticks, then the play-head.
    s->add(0, len)->fn = [LY](const Ctx& cx, int t) {
        const float half = 720 * rise(t, 4, 50);
        dr::rect(cx, 960 - half, LY - 1.5f, half * 2, 3, withAlpha(INK, 0.55f));
        for (int i = 0; i <= 48; i++) {
            const float x = 240 + i * 30.f;
            if (std::fabs(x - 960) > half) continue;
            const float h = i % 4 == 0 ? 16.f : 8.f;
            dr::rect(cx, x - 1, LY + 6, 2, h, withAlpha(DIM, 0.6f * rise(t, 20, 40)));
        }
        if (t < 50) return;
        // Play-head position: through each letter's centre exactly when that letter pops.
        std::vector<double> ts{ 50 }, xs{ 240 };
        for (int i = 0; i < 9; i++) { ts.push_back(letterPop(i)); xs.push_back(letterCenter(i)); }
        ts.push_back(letterPop(8) + 50); xs.push_back(1680);
        double x = xs.back();
        for (size_t i = 1; i < ts.size(); i++)
            if (t <= ts[i]) { x = xs[i - 1] + (xs[i] - xs[i - 1]) * (t - ts[i - 1]) / (ts[i] - ts[i - 1]); break; }
        const float drop = pop(t, 50, 200, 14), a = fall(t, 470, 60);
        const float px = (float)x;
        glow(cx, { px, LY }, 90, TEAL, 0.45f * a);
        dr::rect(cx, px - 1.5f, LY - 230 * drop, 3, 230 * drop + 26, withAlpha(TEAL, a));
        const Vector2 head[3] = { { px - 14, LY - 230 * drop - 18 }, { px + 14, LY - 230 * drop - 18 }, { px, LY - 230 * drop } };
        dr::fillPoly(cx, head, 3, withAlpha(TEAL, a));
        say(cx, mono(), fmt("frame %03d", t), { px, LY + 44 }, 20, withAlpha(TEAL, a), 0.5, 0);
    };
    // Letters rise from behind the line (a mask hides what is still below it).
    TNode* title = s->add(0, len);
    title->isolate().mask = [LY](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1920, LY - 3, WHITE); };
    title->fn = [](const Ctx& cx, int t) {
        for (int i = 0; i < 9; i++) glow(cx, { letterCenter(i), 470 }, 160, TEAL, 0.10f * pop(t, letterPop(i), 170, 11));
        titleLetters(cx, t, INK, true);
    };
    // A light sweep: the title again, additive, masked to a slanted moving band.
    TNode* shine = s->add(330, 120);
    shine->isolate().blend = Blend::Add;
    shine->layer->mask = [](const Ctx& cx, int t) {
        const float x = -300 + t * 22.f;
        const Vector2 band[4] = { { x, 300 }, { x + 140, 300 }, { x + 40, 640 }, { x - 100, 640 } };
        dr::fillPoly(cx, band, 4, WHITE);
    };
    shine->fn = [](const Ctx& cx, int t) { titleLetters(cx, t + 330, withAlpha(TEAL, 0.9f), false); };
    s->add(0, len)->fn = [](const Ctx& cx, int t) {
        const float a = rise(t, 250, 40), b = rise(t, 300, 40);
        say(cx, regular(), "Motion graphics, written in C++", { 960, 668 + 20 * (1 - a) }, 50, withAlpha(INK, a), 0.5, 0);
        say(cx, mono(), "C++17  \xC2\xB7  raylib  \xC2\xB7  OpenGL  \xC2\xB7  ffmpeg  \xC2\xB7  no browser",
            { 960, 750 + 16 * (1 - b) }, 26, withAlpha(DIM, b), 0.5, 0, 1);
    };
}

// ================================================================== the idea
// A video is a function of its frame number: the preview plays, then jumps to arbitrary
// frames, and each still matches its place in the filmstrip.
void heading(const Ctx& cx, int t, const char* kicker, const char* title, float y = 150) {
    const float a = rise(t, 6, 26), b = rise(t, 12, 30);
    say(cx, mono(), kicker, { 960, y + 14 * (1 - a) }, 22, withAlpha(TEAL, a), 0.5, 0, 3);
    say(cx, bold(), title, { 960, y + 40 + 20 * (1 - b) }, 58, withAlpha(INK, b), 0.5, 0);
}
void sceneIdea(TNode* s, int len, int abs0) {
    struct Jump { int at, frame; };
    static const Jump JUMPS[] = { { 380, 4812 }, { 460, 15 }, { 540, 900 }, { 620, 2400 } };
    auto frameAt = [](int t) {
        int f = t;
        for (const Jump& j : JUMPS) if (t >= j.at) f = j.frame;
        return f;
    };
    s->add(0, len)->fn = [abs0](const Ctx& cx, int t) { backdrop(cx, abs0 + t); };
    s->add(0, len)->fn = [frameAt](const Ctx& cx, int t) {
        heading(cx, t, "THE IDEA", "A video is a function of its frame number");
        const int T = frameAt(t);
        const Rectangle P{ 560, 300, 800, 450 };
        const float k = pop(t, 20, 140, 15);
        const Ctx pc = sub(cx, { 960, 525 }, 0, k);
        dr::roundRect(pc, -404, -229, 808, 458, 14, LINE);
        miniScene(pc, { -400, -225, 800, 450 }, T);
        int jumpAge = 999;
        for (const Jump& j : JUMPS) if (t >= j.at) jumpAge = t - j.at;
        if (jumpAge < 20) dr::rect(pc, -400, -225, 800, 450, withAlpha(WHITE, 0.5f * (1 - jumpAge / 20.f)));
        (void)P;
        // The equation, with the live frame number.
        const float e = rise(t, 50, 30);
        Font f = mono();
        const std::string lhs = "frame(", num = std::to_string(T), rhs = ")  \xE2\x86\x92  pixels";
        const float w = textWidth(f, lhs + num + rhs, 40);
        float x = 960 - w / 2;
        say(cx, f, lhs, { x, 790 }, 40, withAlpha(INK, e)); x += textWidth(f, lhs, 40);
        say(cx, f, num, { x, 790 }, 40, withAlpha(AMBER, e)); x += textWidth(f, num, 40);
        say(cx, f, rhs, { x, 790 }, 40, withAlpha(INK, e));
        // Filmstrip: neighbours of the current frame, each drawn on its own.
        const float fs = rise(t, 80, 30);
        for (int i = -6; i <= 6; i++) {
            const int Ti = std::max(0, T + i * 6);
            const float x0 = 960 + i * 140.f - 64;
            const Ctx tc = sub(cx, { 0, 20 * (1 - fs) });
            Ctx faded = tc; faded.a *= fs * (i == 0 ? 1 : 0.55f);
            if (i == 0) dr::roundRect(faded, x0 - 4, 862, 136, 80, 8, TEAL);
            miniScene(faded, { x0, 866, 128, 72 }, Ti);
        }
    };
    // Left: the claim. Right: the stills, checked.
    s->add(0, len)->fn = [](const Ctx& cx, int t) {
        const float a = rise(t, 360, 30);
        TextStyle st{ bold(), 34 };
        st.color = withAlpha(INK, a);
        dr::textBox(cx, "No state carries between frames.", { 110, 330, 420, 100 }, st);
        TextStyle body{ regular(), 26 };
        body.color = withAlpha(DIM, a); body.lineHeight = 1.4f;
        dr::textBox(cx, "Render any frame on its own, in any order, on any machine, and you get the same "
                        "pixels as the full render.", { 110, 440, 420, 260 }, body);
        for (int i = 0; i < 4; i++) {
            const int at = JUMPS[i].at;
            const float b = rise(t, at, 20);
            if (b <= 0) continue;
            const float y = 360 + i * 70.f;
            pill(cx, { 1410, y + 8 * (1 - b) }, fmt("still --at %d", JUMPS[i].frame), mono(), 22,
                 withAlpha(INK, b), withAlpha(PANEL2, b));
            dr::circle(cx, { 1780, y }, 16, withAlpha(MINT, b * 0.9f));
            const Vector2 tick[3] = { { 1772, y }, { 1778, y + 6 }, { 1789, y - 7 } };
            dr::polyStroke(cx, tick, 3, false, 3.5f, withAlpha(BG, b));
        }
    };
}

// ================================================================== under the hood
void iconCode(const Ctx& cx) { say(cx, mono(), "{ }", { 0, 0 }, 54, TEAL, 0.5, 0.5); }
void iconTree(const Ctx& cx) {
    const Vector2 n[5] = { { 0, -26 }, { -32, 8 }, { 32, 8 }, { -48, 36 }, { -16, 36 } };
    dr::strokeSeg(cx, n[0], n[1], 3, DIM); dr::strokeSeg(cx, n[0], n[2], 3, DIM);
    dr::strokeSeg(cx, n[1], n[3], 3, DIM); dr::strokeSeg(cx, n[1], n[4], 3, DIM);
    for (int i = 0; i < 5; i++) dr::circle(cx, n[i], i ? 8.f : 11.f, i ? IRIS : TEAL);
}
void iconGpu(const Ctx& cx, int t) {
    Ctx k = sub(cx, { 0, 4 }, t * 0.6f);
    const Vector2 tri[3] = { { 0, -36 }, { 34, 24 }, { -34, 24 } };
    dr::fillPoly(k, tri, 3, withAlpha(ROSE, 0.35f));
    dr::polyStroke(k, tri, 3, true, 3, ROSE);
    dr::strokeSeg(k, tri[0], { 0, 24 }, 2, withAlpha(ROSE, 0.7f));
}
void iconCanvas(const Ctx& cx) {
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++)
            dr::rect(cx, -34 + i * 18.f, -34 + j * 18.f, 15, 15, withAlpha(AMBER, (i + j) % 3 ? 0.35f : 0.9f));
}
void iconReadback(const Ctx& cx, int t) {
    dr::arc(cx, { 0, 0 }, 30, 5, t * 3.f, t * 3.f + 140, MINT);
    dr::arc(cx, { 0, 0 }, 30, 5, t * 3.f + 180, t * 3.f + 320, MINT);
}
void iconFilm(const Ctx& cx) {
    dr::roundRect(cx, -42, -28, 84, 56, 8, withAlpha(TYPE, 0.3f));
    for (int i = 0; i < 6; i++) { dr::rect(cx, -36 + i * 13.f, -24, 7, 6, TYPE); dr::rect(cx, -36 + i * 13.f, 18, 7, 6, TYPE); }
    const Vector2 play[3] = { { -9, -11 }, { 13, 0 }, { -9, 11 } };
    dr::fillPoly(cx, play, 3, INK);
}
void sceneHood(TNode* s, int len, int abs0) {
    s->add(0, len)->fn = [abs0](const Ctx& cx, int t) { backdrop(cx, abs0 + t); };
    struct Card { const char* name; const char* sub; };
    static const Card CARDS[6] = {
        { "Your code", "C++17 compositions" }, { "Timeline tree", "nodes, tracks, springs" },
        { "raylib + OpenGL", "2D, 3D and shaders" }, { "Canvas", "2x / 4x supersampled" },
        { "Async readback", "pixel buffers + a writer thread" }, { "ffmpeg", "x264, NVENC, AAC audio" } };
    const float CW = 230, CH = 196, GAP = 50, X0 = (1920 - (6 * CW + 5 * GAP)) / 2, Y0 = 360;
    auto centerX = [=](int i) { return X0 + i * (CW + GAP) + CW / 2; };
    s->add(0, len)->fn = [=](const Ctx& cx, int t) {
        heading(cx, t, "UNDER THE HOOD", "From C++ to MP4, without a browser");
        // Packets: frames travelling the pipeline, one every 20 frames from t = 300.
        auto packetX = [&](int k, int tt, float& x) {
            const float u = (tt - 300 - k * 20) / 260.f;
            if (u < 0 || u > 1) return false;
            x = centerX(0) + (centerX(5) - centerX(0)) * u;
            return true;
        };
        for (int i = 0; i < 6; i++) {
            const float k = pop(t, 40 + i * 26, 150, 14);
            if (k <= 0.001f) continue;
            float hot = 0;
            for (int p = 0; p < 30; p++) {
                float x;
                if (packetX(p, t, x)) hot = std::max(hot, 1 - std::min(1.f, std::fabs(x - centerX(i)) / 90.f));
            }
            const Ctx c = sub(cx, { centerX(i), Y0 + CH / 2 }, 0, k);
            glow(c, { 0, 0 }, 170, TEAL, 0.25f * hot);
            dr::roundRect(c, -CW / 2 - 2, -CH / 2 - 2, CW + 4, CH + 4, 20, mixC(LINE, TEAL, hot * 0.8f));
            dr::roundRect(c, -CW / 2, -CH / 2, CW, CH, 18, PANEL);
            const Ctx ic = sub(c, { 0, -42 });
            switch (i) {
                case 0: iconCode(ic); break;
                case 1: iconTree(ic); break;
                case 2: iconGpu(ic, t); break;
                case 3: iconCanvas(ic); break;
                case 4: iconReadback(ic, t); break;
                default: iconFilm(ic); break;
            }
            say(c, bold(), CARDS[i].name, { 0, 28 }, 26, INK, 0.5, 0.5);
            TextStyle st{ regular(), 19 };
            st.color = DIM; st.align = Align::Center;
            dr::textBox(c, CARDS[i].sub, { -CW / 2 + 14, 50, CW - 28, 50 }, st);
            if (i < 5) {
                const float a = rise(t, 60 + i * 26, 20);
                const float ax = centerX(i) + CW / 2 + 8, bx = ax + (GAP - 16) * a;
                dr::strokeSeg(cx, { ax, Y0 + CH / 2 }, { bx, Y0 + CH / 2 }, 3, withAlpha(DIM, a));
                const Vector2 head[3] = { { bx, Y0 + CH / 2 - 7 }, { bx + 9, Y0 + CH / 2 }, { bx, Y0 + CH / 2 + 7 } };
                dr::fillPoly(cx, head, 3, withAlpha(DIM, a));
            }
        }
        // The packets themselves, riding over the cards.
        for (int p = 0; p < 30; p++) {
            float x;
            if (!packetX(p, t, x)) continue;
            const float y = Y0 - 46 + 8 * std::sin(x * 0.02f);
            dr::roundRect(cx, x - 38, y - 23, 76, 46, 6, withAlpha(INK, 0.8f));
            miniScene(cx, { x - 36, y - 21, 72, 42 }, 4000 + p * 6, false);
        }
        // Output.
        const float o = rise(t, 220, 30);
        const float ox = centerX(5);
        dr::strokeSeg(cx, { ox, Y0 + CH + 10 }, { ox, Y0 + CH + 10 + 40 * o }, 3, withAlpha(DIM, o));
        pill(cx, { ox, Y0 + CH + 80 }, "out/video.mp4", mono(), 24, withAlpha(INK, o), withAlpha(PANEL2, o), 0.5);
        // The stack.
        static const char* STACK[] = { "C++17", "raylib 6.2", "OpenGL 3.3", "ffmpeg", "CMake", "Windows \xC2\xB7 Linux \xC2\xB7 macOS" };
        float total = 0;
        for (const char* b : STACK) total += textWidth(mono(), b, 24) + 32 + 20;
        float x = 960 - (total - 20) / 2;
        for (int i = 0; i < 6; i++) {
            const float b = rise(t, 420 + i * 12, 24);
            x += pill(cx, { x, 760 + 14 * (1 - b) }, STACK[i], mono(), 24, withAlpha(INK, b),
                      withAlpha(mixC(PANEL2, TEAL, 0.18f), b)) + 20;
        }
        const float c = rise(t, 520, 30);
        TextStyle cap{ regular(), 30 };
        cap.color = withAlpha(DIM, c); cap.align = Align::Center;
        dr::textBox(cx, "Raw frames stream straight into the encoder: no temporary files, and drawing, "
                        "readback and encoding overlap.", { 360, 840, 1200, 120 }, cap);
    };
}

// ================================================================== the timeline tree
void sceneTree(TNode* s, int len, int abs0) {
    s->add(0, len)->fn = [abs0](const Ctx& cx, int t) { backdrop(cx, abs0 + t); };
    static const std::vector<std::string> CODE = {
        "Comp c;",
        "c.w = 1920; c.h = 1080; c.fps = 60;",
        "TNode* scene = addRoot(c.roots, 0, 300);",
        "",
        "TNode* title = scene->add(30, 240);",
        "title->up = [](int t, NodeProps& p) {",
        "    p.opacity = interpolate(t, {0, 20},",
        "                            {0, 1}, clamped());",
        "};",
        "title->fn = [](const Ctx& cx, int t) {",
        "    dr::text(font, cx, \"Hello\", {0, 0},",
        "             72, 0, WHITE, 0.5, 0.5);",
        "};",
        "",
        "// up: animate props   fn: draw",
        "// t is local: 0 at the node's start",
    };
    s->add(0, len)->fn = [](const Ctx& cx, int t) {
        heading(cx, t, "THE MODEL", "A composition is a tree of timed nodes");
        Ctx k = sub(cx, { 0, 30 * fall(t, 20, 30) });
        k.a *= rise(t, 20, 30);
        codeCard(k, { 110, 300, 690, 630 }, CODE, std::max(0, (t - 40) * 4), "hello.cpp", t);
    };
    struct El { const char* name; int from, dur; Color col; int depth; };
    static const El ELS[] = { { "scene", 0, 300, INK, 0 }, { "background", 0, 300, FAINT, 1 }, { "title", 30, 240, TEAL, 1 },
                              { "logo", 60, 230, AMBER, 1 }, { "particles", 100, 200, ROSE, 1 }, { "caption", 150, 140, IRIS, 1 } };
    s->add(0, len)->fn = [](const Ctx& cx, int t) {
        Ctx k = cx;
        k.a *= rise(t, 30, 30);
        const int T = std::min(299, std::max(0, (t - 60) % 340));
        dr::roundRect(k, SX - 2, 298, SW + 4, 634, 22, LINE);
        dr::roundRect(k, SX, 300, SW, 630, 20, PANEL);
        // Preview: what the tree draws at frame T. Each element is active only inside its
        // [from, from + dur) and sees its own local t.
        const Rectangle P{ 884, 324, 480, 270 };
        const Ctx pv = sub(k, { P.x, P.y }, 0, 1.5f);
        auto local = [T](const El& e) { return T - e.from; };
        auto on = [T](const El& e) { return T >= e.from && T < e.from + e.dur; };
        dr::rectGrad4(pv, 0, 0, 320, 180, mkColor(0x1a2550), mkColor(0x25305e), mkColor(0x101830), mkColor(0x0c1226));
        if (on(ELS[4])) for (int i = 0; i < 26; i++) {
            const int lt = local(ELS[4]);
            const float x = (float)rnd01(71, i) * 320, y = 180 - std::fmod((float)rnd01(72, i) * 180 + lt * 0.8f, 180.f);
            dr::circle(pv, { x, y }, 2.2f, withAlpha(ROSE, 0.8f));
        }
        if (on(ELS[3])) {
            const int lt = local(ELS[3]);
            const float sc = pop(lt, 0, 150, 12);
            logoMark(sub(pv, { 262, 56 }, 0, sc), { 0, 0 }, 22, lt * 3.f);
        }
        if (on(ELS[2])) {
            const int lt = local(ELS[2]);
            const float op = (float)interpolate(lt, { 0, 20 }, { 0, 1 }, clamped());
            Ctx tc = pv; tc.a *= op;
            say(tc, bold(), "Hello", { 130, 90 }, 50, INK, 0.5, 0.5);
        }
        if (on(ELS[5])) {
            const int lt = local(ELS[5]);
            Ctx tc = pv; tc.a *= rise(lt, 0, 15);
            dr::rect(tc, 0, 150, 320 * rise(lt, 0, 25), 30, withAlpha(IRIS, 0.8f));
            say(tc, mono(), "made with raymotion", { 12, 165 }, 13, INK, 0, 0.5);
        }
        say(k, mono(), fmt("frame %03d", T), { P.x + 8, P.y + P.height + 12 }, 18, DIM);
        // The tree, with active nodes lit.
        for (int i = 0; i < 6; i++) {
            const El& e = ELS[i];
            const float y = 340 + i * 42.f, x = 1400 + e.depth * 30.f;
            const bool a = on(e);
            if (e.depth) dr::strokeSeg(k, { 1408, y - 30 }, { 1408, y }, 2, LINE), dr::strokeSeg(k, { 1408, y }, { x - 4, y }, 2, LINE);
            dr::circle(k, { x + 6, y }, 7, a ? e.col : FAINT);
            say(k, mono(), e.name, { x + 22, y }, 21, a ? INK : FAINT, 0, 0.5);
        }
        // Lanes, as in an editor: one bar per node, and the play-head.
        const float LX = 1010, LW = 770, LY = 640;
        for (int i = 0; i < 6; i++) {
            const El& e = ELS[i];
            const float y = LY + i * 46.f;
            say(k, mono(), e.name, { 990, y + 16 }, 18, on(e) ? INK : FAINT, 1, 0.5);
            dr::roundRect(k, LX, y + 2, LW, 28, 6, withAlpha(LINE, 0.5f));
            const float x0 = LX + LW * e.from / 300.f, w = LW * e.dur / 300.f;
            dr::roundRect(k, x0, y + 2, w, 28, 6, withAlpha(e.col, on(e) ? 0.85f : 0.35f));
        }
        const float px = LX + LW * T / 300.f;
        dr::rect(k, px - 1.5f, LY - 14, 3, 6 * 46 + 18, AMBER);
        const Vector2 head[3] = { { px - 9, LY - 26 }, { px + 9, LY - 26 }, { px, LY - 12 } };
        dr::fillPoly(k, head, 3, AMBER);
    };
}

// ================================================================== feature chapters
using StageFn = std::function<void(TNode* stage, int len, int abs0)>;
struct Feature {
    const char* title;
    const char* desc;
    const char* file;
    std::vector<std::string> code;
    StageFn stage;
};

void featureScene(TNode* s, int len, int abs0, int number, std::shared_ptr<Feature> f) {
    s->add(0, len)->fn = [abs0](const Ctx& cx, int t) { backdrop(cx, abs0 + t); };
    const std::string kicker = fmt("FEATURE %02d / %02d", number, NFEAT);
    s->add(0, len)->fn = [f, kicker](const Ctx& cx, int t) {
        const float a0 = rise(t, 6, 24), a1 = rise(t, 12, 28), a2 = rise(t, 20, 30);
        say(cx, mono(), kicker, { 110, 160 + 16 * (1 - a0) }, 22, withAlpha(TEAL, a0), 0, 0, 3);
        TextStyle ts{ bold(), 60 };
        ts.color = withAlpha(INK, a1);
        dr::textBox(cx, f->title, { 110, 194 + 22 * (1 - a1), 720, 80 }, ts);
        TextStyle ds{ regular(), 29 };
        ds.color = withAlpha(DIM, a2); ds.lineHeight = 1.38f;
        dr::textBox(cx, f->desc, { 110, 292 + 18 * (1 - a2), 690, 170 }, ds);
    };
    TNode* code = s->add(0, len);
    code->up = [](int t, NodeProps& p) { const float k = rise(t, 26, 30); p.opacity = k; p.pos.y = 30 * (1 - k); };
    code->fn = [f](const Ctx& cx, int t) { codeCard(cx, { 110, 480, 690, 450 }, f->code, std::max(0, (t - 40) * 3), f->file, t); };
    TNode* frame = s->add(0, len);
    frame->up = [](int t, NodeProps& p) { p.opacity = std::min(1.f, pop(t, 8, 140, 16)); };
    frame->fn = [](const Ctx& cx, int) {
        dr::roundRect(cx, SX - 2, SY - 2, SW + 4, SH + 4, 22, LINE);
        dr::roundRect(cx, SX, SY, SW, SH, 20, PANEL);
    };
    const int delay = 18;
    TNode* stage = s->add(delay, len - delay);
    stage->props.pos = { SX, SY };
    stage->up = [](int t, NodeProps& p) { const float k = rise(t, 0, 26); p.opacity = k; p.pos = { SX, SY + 26 * (1 - k) }; };
    stage->isolate().mask = [](const Ctx& cx, int) { dr::roundRect(cx, 0, 0, SW, SH, 20, WHITE); };
    f->stage(stage, len - delay, abs0 + delay);
}

// ---------------------------------------------------------------- 01 keyframes & easing
void stageEasing(TNode* st, int len, int) {
    st->add(0, len)->fn = [](const Ctx& cx, int t) {
        const int E[6] = { E_LINEAR, E_OUT_CUBIC, E_IN_OUT_EXPO, E_BACK, E_ELASTIC, E_BOUNCE };
        const char* N[6] = { "E_LINEAR", "E_OUT_CUBIC", "E_IN_OUT_EXPO", "E_BACK", "E_ELASTIC", "E_BOUNCE" };
        const Color C[6] = { DIM, TEAL, IRIS, AMBER, ROSE, MINT };
        const int tt = t % 150;
        const float u = (float)clamp01((tt - 20) / 90.0);
        for (int i = 0; i < 6; i++) {
            const float y = 82 + i * 124.f;
            say(cx, mono(), N[i], { 210, y - 52 }, 17, DIM);
            const float gx = 40, gy = y - 22, gw = 150, gh = 56;
            dr::rect(cx, gx, gy + gh, gw, 1.5f, LINE);
            dr::rect(cx, gx, gy, 1.5f, gh, LINE);
            std::vector<Vector2> pts;
            for (int s = 0; s <= 40; s++) { const float q = s / 40.f; pts.push_back({ gx + gw * q, gy + gh - gh * (float)easeFn(E[i], q) }); }
            dr::polyStroke(cx, pts.data(), (int)pts.size(), false, 2.5f, C[i]);
            const float v = (float)easeFn(E[i], u);
            dr::circle(cx, { gx + gw * u, gy + gh - gh * v }, 5, INK);
            const float x0 = 270, x1 = 900;
            dr::rect(cx, x0, y + 5, x1 - x0, 2, LINE);
            dr::circle(cx, { x0, y + 6 }, 4, FAINT);
            dr::circle(cx, { x1, y + 6 }, 4, FAINT);
            for (int g = 4; g >= 1; g--) {
                const float ug = (float)clamp01((tt - 20 - g * 3) / 90.0);
                dr::circle(cx, { x0 + (x1 - x0) * (float)easeFn(E[i], ug), y + 6 }, 18, withAlpha(C[i], 0.07f * (5 - g)));
            }
            const float bx = x0 + (x1 - x0) * v;
            glow(cx, { bx, y + 6 }, 60, C[i], 0.35f);
            dr::circle(cx, { bx, y + 6 }, 18, C[i]);
        }
    };
}

// ---------------------------------------------------------------- 02 springs
void stageSprings(TNode* st, int len, int) {
    st->add(0, len)->fn = [](const Ctx& cx, int t) {
        const SpringConfig cfg[3] = { { 1, 180, 6 }, { 1, 100, 12 }, { 1, 120, 26 } };
        const char* names[3] = { "bouncy", "default", "stiff" };
        const Color C[3] = { ROSE, TEAL, AMBER };
        const int tt = t % 180;
        for (int i = 0; i < 3; i++) {
            const float x = 160 + i * 315.f;
            SpringOptions o;
            o.fps = FPS; o.config = cfg[i]; o.delay = 15;
            const float k = (float)spring(tt, o), sz = std::max(1.f, 190 * k);
            glow(cx, { x, 220 }, 160, C[i], 0.2f * k);
            dr::roundRect(cx, x - sz / 2, 220 - sz / 2, sz, sz, 28 * std::max(0.1f, k), C[i]);
            say(cx, bold(), names[i], { x, 384 }, 32, INK, 0.5, 0);
            say(cx, mono(), fmt("damping %d", (int)cfg[i].damping), { x, 428 }, 19, DIM, 0.5, 0);
            const float gx = x - 130, gy = 500, gw = 260, gh = 170;
            dr::roundRect(cx, gx - 12, gy - 16, gw + 24, gh + 32, 12, withAlpha(BG, 0.7f));
            dr::rect(cx, gx, gy + gh - gh / 1.6f, gw, 1.5f, withAlpha(INK, 0.25f));
            std::vector<Vector2> pts;
            SpringOptions q = o;
            q.delay = 0;
            for (int s = 0; s <= 90; s++) pts.push_back({ gx + gw * s / 90.f, gy + gh - gh * (float)spring(s * 1.6, q) / 1.6f });
            dr::polyStroke(cx, pts.data(), (int)pts.size(), false, 2.5f, C[i]);
            const float fpos = std::clamp((tt - 15) / 1.6f, 0.f, 90.f);
            dr::circle(cx, { gx + gw * fpos / 90.f, gy + gh - gh * k / 1.6f }, 6, INK);
            say(cx, mono(), fmt("settles in %d frames", measureSpring(FPS, cfg[i])), { x, gy + gh + 40 }, 18, DIM, 0.5, 0);
        }
    };
}

// ---------------------------------------------------------------- 03 interpolation
void stageInterp(TNode* st, int len, int) {
    st->add(0, len)->fn = [](const Ctx& cx, int t) {
        const float bx = 50, bw = 850, by = 80, bh = 56;
        say(cx, mono(), "interpolateColors(t, {0, 60, 120}, {TEAL, IRIS, AMBER})", { bx, 38 }, 18, DIM);
        for (int i = 0; i < 100; i++)
            dr::rect(cx, bx + bw * i / 100.f, by, bw / 100.f + 1, bh, interpolateColors(i * 1.2, { 0, 60, 120 }, { TEAL, IRIS, AMBER }));
        const double ct = 60 + 60 * std::sin(t * 0.03);
        const float mx = bx + bw * (float)(ct / 120);
        dr::rect(cx, mx - 2, by - 8, 4, bh + 16, INK);
        const Color cur = interpolateColors(ct, { 0, 60, 120 }, { TEAL, IRIS, AMBER });
        dr::circle(cx, { bx + 34, 196 }, 30, cur);
        say(cx, mono(), fmt("t = %5.1f   ->   #%02x%02x%02x", ct, cur.r, cur.g, cur.b), { bx + 84, 196 }, 22, INK, 0, 0.5);
        // interpolate(x, {0, 60}, {0, 1}) with three ways to leave the input range.
        const float gx = 90, gy = 290, gw = 800, gh = 380;
        auto X = [=](double x) { return gx + gw * (float)((x + 60) / 180); };
        auto Y = [=](double y) { return gy + gh - gh * (float)((y + 0.5) / 2.0); };
        dr::rect(cx, X(0), gy, X(60) - X(0), gh, withAlpha(TEAL, 0.07f));
        dr::rect(cx, gx, Y(0), gw, 1.5f, LINE);
        dr::rect(cx, gx, Y(1), gw, 1.5f, withAlpha(LINE, 0.6f));
        dr::rect(cx, X(0), gy, 1.5f, gh, LINE);
        say(cx, mono(), "input range", { (X(0) + X(60)) / 2, gy + 8 }, 17, withAlpha(TEAL, 0.8f), 0.5, 0);
        const Extrapolate modes[3] = { Extrapolate::Extend, Extrapolate::Clamp, Extrapolate::Wrap };
        const char* names[3] = { "Extend", "Clamp", "Wrap" };
        const Color C[3] = { TEAL, AMBER, ROSE };
        const double cur_x = -60 + 180 * ((t % 300) / 300.0);
        for (int m = 0; m < 3; m++) {
            const InterpOptions o{ ease::inOut(ease::cubic), modes[m], modes[m] };
            std::vector<Vector2> pts;
            for (int i = 0; i <= 180; i++) {
                const double x = -60 + i;
                pts.push_back({ X(x), Y(std::clamp(interpolate(x, { 0, 60 }, { 0, 1 }, o), -0.5, 1.5)) + m * 0.0f });
            }
            dr::polyStroke(cx, pts.data(), (int)pts.size(), false, 3, withAlpha(C[m], 0.9f));
            const double v = interpolate(cur_x, { 0, 60 }, { 0, 1 }, o);
            dr::circle(cx, { X(cur_x), Y(std::clamp(v, -0.5, 1.5)) }, 8, C[m]);
            pill(cx, { 90 + m * 280.f, 726 }, fmt("%s  %+.2f", names[m], v), mono(), 20, INK, withAlpha(C[m], 0.25f));
        }
        dr::rect(cx, X(cur_x) - 1, gy, 2, gh, withAlpha(INK, 0.35f));
    };
}

// ---------------------------------------------------------------- 04 vector drawing
void stageShapes(TNode* st, int len, int) {
    st->add(0, len)->fn = [](const Ctx& cx, int t) {
        auto cell = [](int i) { return Vector2{ 158 + (i % 3) * 317.f, 175 + (i / 3) * 390.f }; };
        static const char* LABELS[6] = { "dr::star", "dr::blob", "dr::ring + dr::arc", "dr::sector",
                                         "dr::rectGrad4 + roundRect", "dr::fillPoly, concave" };
        for (int i = 0; i < 6; i++) {
            const Vector2 c = cell(i);
            const float k = pop(t, i * 6, 150, 13);
            const Ctx sc = sub(cx, c, 0, k);
            switch (i) {
            case 0: {
                const int points = 5 + (t / 90) % 4;
                glow(sc, { 0, 0 }, 170, AMBER, 0.2f);
                dr::star(sc, { 0, 0 }, 120, 50 + 8 * std::sin(t * 0.05f), points, -90 + t * 0.6f, AMBER);
                break;
            }
            case 1:
                dr::blob(sc, { 0, 0 }, 108, 18, 5, t * 3.f, ROSE);
                dr::blob(sc, { 0, 0 }, 58, 10, 3, -t * 4.f, withAlpha(INK, 0.3f));
                break;
            case 2: {
                const float p = (float)std::fmod(t / 240.0, 1.0);
                dr::ring(sc, { 0, 0 }, 100, 16, LINE);
                dr::arc(sc, { 0, 0 }, 100, 16, -90, -90 + 360 * p, TEAL);
                say(sc, bold(), fmt("%d%%", (int)(p * 100)), { 0, 0 }, 48, INK, 0.5, 0.5);
                break;
            }
            case 3: {
                float v[4], sum = 0;
                for (int q = 0; q < 4; q++) { v[q] = 1 + 0.6f * std::sin(t * 0.02f + q * 1.7f); sum += v[q]; }
                const Color C[4] = { TEAL, IRIS, AMBER, ROSE };
                float a = -90;
                for (int q = 0; q < 4; q++) {
                    const float b = a + 360 * v[q] / sum;
                    dr::sector(sc, { 0, 0 }, 115, a, b, C[q]);
                    a = b;
                }
                dr::circle(sc, { 0, 0 }, 48, PANEL);
                break;
            }
            case 4: {
                const float r = 10 + 50 * (0.5f + 0.5f * std::sin(t * 0.04f));
                dr::roundRect(sc, -118, -118, 236, 236, r, withAlpha(IRIS, 0.35f));
                const Ctx rc = sub(sc, { 0, 0 }, t * 0.5f);
                const float h = t * 0.8f;
                dr::rectGrad4(rc, -78, -78, 156, 156, ColorFromHSV(h, 0.7f, 1), ColorFromHSV(h + 90, 0.7f, 1),
                              ColorFromHSV(h + 180, 0.7f, 1), ColorFromHSV(h + 270, 0.7f, 1));
                break;
            }
            default: {
                std::vector<Vector2> pts;
                for (int q = 0; q < 14; q++) {
                    const float a = (float)(q * 2 * PI / 14) + t * 0.006f;
                    const float r = (q % 2 ? 54.f : 122.f) * (1 + 0.16f * std::sin(t * 0.05f + q));
                    pts.push_back({ r * std::cos(a), r * std::sin(a) });
                }
                dr::fillPoly(sc, pts.data(), (int)pts.size(), withAlpha(MINT, 0.75f));
                dr::polyStroke(sc, pts.data(), (int)pts.size(), true, 3, INK);
                break;
            }
            }
            say(cx, mono(), LABELS[i], { c.x, c.y + 162 }, 19, DIM, 0.5, 0);
        }
    };
}

// ---------------------------------------------------------------- 05 supersampling
void stageSupersample(TNode* st, int len, int) {
    st->add(0, len)->fn = [](const Ctx& cx, int t) {
        // Top: the box filter. An edge sampled on a 4x grid, then halved twice.
        const float a = t * 0.012f, off = 0.25f * std::sin(t * 0.03f);
        auto inside = [&](float x, float y) { return (x - 1) * std::cos(a) + (y - 1) * std::sin(a) < off; };
        float A[8][8], B[4][4], Cg[2][2];
        for (int j = 0; j < 8; j++) for (int i = 0; i < 8; i++) A[j][i] = inside((i + 0.5f) / 4, (j + 0.5f) / 4) ? 1.f : 0.f;
        for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++)
            B[j][i] = (A[2 * j][2 * i] + A[2 * j][2 * i + 1] + A[2 * j + 1][2 * i] + A[2 * j + 1][2 * i + 1]) / 4;
        for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++)
            Cg[j][i] = (B[2 * j][2 * i] + B[2 * j][2 * i + 1] + B[2 * j + 1][2 * i] + B[2 * j + 1][2 * i + 1]) / 4;
        auto grid = [&](float x0, float y0, int n, auto get) {
            const float cs = 176.f / n;
            for (int j = 0; j < n; j++) for (int i = 0; i < n; i++)
                dr::rect(cx, x0 + i * cs + 1, y0 + j * cs + 1, cs - 2, cs - 2, mixC(mkColor(0x1a2240), TEAL, get(i, j)));
        };
        const float gy = 52;
        grid(70, gy, 8, [&](int i, int j) { return A[j][i]; });
        grid(387, gy, 4, [&](int i, int j) { return B[j][i]; });
        grid(704, gy, 2, [&](int i, int j) { return Cg[j][i]; });
        for (int k = 0; k < 2; k++) {
            const float ax = 262 + k * 317.f;
            dr::strokeSeg(cx, { ax, gy + 88 }, { ax + 100, gy + 88 }, 3, DIM);
            const Vector2 head[3] = { { ax + 100, gy + 80 }, { ax + 112, gy + 88 }, { ax + 100, gy + 96 } };
            dr::fillPoly(cx, head, 3, DIM);
            say(cx, mono(), "2x2 box", { ax + 56, gy + 62 }, 16, DIM, 0.5, 0.5);
        }
        say(cx, mono(), "drawn at 4x", { 158, gy + 196 }, 18, DIM, 0.5, 0);
        say(cx, mono(), "halved", { 475, gy + 196 }, 18, DIM, 0.5, 0);
        say(cx, mono(), "output pixels", { 792, gy + 196 }, 18, DIM, 0.5, 0);
        // Bottom: the same moving disc, one sample per pixel vs sixteen.
        const float dcx = 9 + 3.5f * std::cos(t * 0.011f), dcy = 9 + 3.5f * std::sin(t * 0.014f), R = 8.6f;
        auto cover = [&](int i, int j, int n) {
            int in = 0;
            for (int b = 0; b < n; b++) for (int q = 0; q < n; q++) {
                const float x = i + (q + 0.5f) / n, y = j + (b + 0.5f) / n;
                in += (x - dcx) * (x - dcx) + (y - dcy) * (y - dcy) < R * R;
            }
            return in / (float)(n * n);
        };
        for (int p = 0; p < 2; p++) {
            const float x0 = 60 + p * 460.f, y0 = 330, cs = 24;
            const int n = p ? 4 : 1;
            for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++)
                dr::rect(cx, x0 + i * cs, y0 + j * cs, cs - 1, cs - 1, mixC(mkColor(0x1a2240), TEAL, cover(i, j, n)));
            say(cx, mono(), p ? "--ss 4: 16 samples per pixel" : "--ss 1: 1 sample per pixel", { x0 + 192, y0 + 400 }, 19,
                p ? INK : DIM, 0.5, 0);
        }
    };
}

// ---------------------------------------------------------------- 06 text & layout
void stageLayout(TNode* st, int len, int) {
    st->add(0, len)->fn = [](const Ctx& cx, int t) {
        const float w1 = 1.6f + 0.9f * std::sin(t * 0.025f), w2 = 1.f + 0.6f * std::cos(t * 0.031f);
        const auto cols = layout::row({ 40, 40, 870, 400 }, { layout::px(220), layout::flex(w1), layout::flex(w2) }, 20);
        const std::string tags[3] = { "px(220)", fmt("flex(%.1f)", w1), fmt("flex(%.1f)", w2) };
        const Align al[3] = { Align::Start, Align::Center, Align::End };
        const char* alName[3] = { "Start", "Center", "End" };
        const Color C[3] = { TEAL, IRIS, AMBER };
        for (int i = 0; i < 3; i++) {
            const Rectangle r = cols[(size_t)i];
            dr::roundRect(cx, r.x, r.y, r.width, r.height, 14, withAlpha(C[i], 0.5f));
            dr::roundRect(cx, r.x + 2, r.y + 2, r.width - 4, r.height - 4, 12, mixC(PANEL, C[i], 0.1f));
            say(cx, mono(), tags[i], { r.x + 16, r.y + 16 }, 18, C[i]);
            TextStyle ts{ regular(), 25 };
            ts.color = INK; ts.align = al[i]; ts.lineHeight = 1.3f;
            const int lines = dr::textBox(cx, "Text wraps to its box word by word and re-flows the moment the layout changes.",
                                          layout::inset(r, 54, 18, 40, 18), ts);
            say(cx, mono(), fmt("%s, %d lines", alName[i], lines), { r.x + 16, r.y + r.height - 28 }, 15, DIM);
        }
        const auto cells = layout::grid({ 40, 480, 870, 270 }, 6, 2, 16);
        for (int i = 0; i < 12; i++) {
            const Rectangle r = cells[(size_t)i];
            const float k = pop(t % 240, 10 + i * 5, 160, 12);
            const Ctx sc = sub(cx, layout::center(r), 0, k);
            const Color c = ColorFromHSV(170 + i * 14.f, 0.55f, 0.9f);
            dr::roundRect(sc, -r.width / 2, -r.height / 2, r.width, r.height, 12, withAlpha(c, 0.22f));
            say(sc, bold(), std::to_string(i + 1), { 0, 0 }, 44, c, 0.5, 0.5);
        }
        say(cx, mono(), "layout::grid(area, 6, 2, 16)", { 40, 762 }, 16, DIM, 0, 0.5);
    };
}

// ---------------------------------------------------------------- 07 layers & blending
float pulseOpacity(int t) { return 0.55f + 0.2f * std::sin(t * 0.05f); }
void stageLayers(TNode* st, int len, int) {
    auto pair = [](const Ctx& cx, int) {
        dr::circle(cx, { -62, 0 }, 112, AMBER);
        dr::circle(cx, { 62, 0 }, 112, AMBER);
    };
    TNode* a = st->add(0, len);                       // opacity per primitive: the overlap doubles up
    a->props.pos = { 240, 200 };
    a->up = [](int t, NodeProps& p) { p.opacity = pulseOpacity(t); };
    a->fn = pair;
    TNode* b = st->add(0, len);                       // a layer: flattened first, then faded as one
    b->props.pos = { 710, 200 };
    b->isolate();
    b->layerUp = [](int t, NodeProps&, Layer& l) { l.opacity = pulseOpacity(t); };
    b->fn = pair;
    st->add(0, len)->fn = [](const Ctx& cx, int t) {
        say(cx, bold(), "per primitive", { 240, 358 }, 28, INK, 0.5, 0);
        say(cx, bold(), "isolate(): one layer", { 710, 358 }, 28, INK, 0.5, 0);
        say(cx, mono(), fmt("opacity %.2f", pulseOpacity(t)), { 475, 30 }, 18, DIM, 0.5, 0);
    };
    // Blend modes: three circles, each its own layer, over a matching backdrop.
    const Blend modes[3] = { Blend::Add, Blend::Multiply, Blend::Screen };
    const char* names[3] = { "Blend::Add", "Blend::Multiply", "Blend::Screen" };
    const Color backs[3] = { BLACK, INK, mkColor(0x202538) };
    const Color rgb[3] = { Color{ 255, 60, 60, 255 }, Color{ 60, 235, 90, 255 }, Color{ 70, 100, 255, 255 } };
    const Color cmy[3] = { Color{ 0, 215, 255, 255 }, Color{ 255, 40, 200, 255 }, Color{ 255, 230, 0, 255 } };
    for (int m = 0; m < 3; m++) {
        const float x0 = 30 + m * 305.f, y0 = 420, w = 280, h = 300;
        st->add(0, len)->fn = [=](const Ctx& cx, int) { dr::roundRect(cx, x0, y0, w, h, 16, backs[m]); };
        for (int k = 0; k < 3; k++) {
            TNode* c = st->add(0, len);
            c->isolate().blend = modes[m];
            const Color col = m == 1 ? cmy[k] : rgb[k];
            c->up = [=](int t, NodeProps& p) {
                const float ang = (float)(k * 2 * PI / 3 - PI / 2) + t * 0.015f;
                p.pos = { x0 + w / 2 + 52 * std::cos(ang), y0 + h / 2 + 52 * std::sin(ang) };
            };
            c->fn = [col](const Ctx& cx, int) { dr::circle(cx, { 0, 0 }, 78, col); };
        }
        st->add(0, len)->fn = [=](const Ctx& cx, int) { say(cx, mono(), names[m], { x0 + w / 2, y0 + h + 22 }, 19, DIM, 0.5, 0); };
    }
}

// ---------------------------------------------------------------- 08 masks
void stageMasks(TNode* st, int len, int) {
    TNode* word = st->add(0, len);
    word->isolate().mask = [](const Ctx& cx, int) { say(cx, bold(), "MASKS", { 475, 195 }, 250, WHITE, 0.5, 0.5, 8); };
    word->fn = [](const Ctx& cx, int t) {
        for (int i = -4; i < 16; i++) {
            const float x = i * 70.f + std::fmod(t * 2.5f, 140.f);
            const Vector2 band[4] = { { x, 40 }, { x + 70, 40 }, { x - 30, 360 }, { x - 100, 360 } };
            dr::fillPoly(cx, band, 4, ColorFromHSV(170 + (i & 7) * 22.f + t * 0.5f, 0.6f, 1));
        }
    };
    const float y0 = 420, w = 280, h = 300;
    auto tileX = [](int i) { return 30 + i * 305.f; };
    for (int i = 0; i < 3; i++)
        st->add(0, len)->fn = [=](const Ctx& cx, int) { dr::roundRect(cx, tileX(i), y0, w, h, 16, withAlpha(LINE, 0.35f)); };
    // Alpha: a growing, turning star reveals the photo.
    auto photo = media::image(asset("photo.png"));
    TNode* star = st->add(0, len);
    star->isolate().mask = [=](const Ctx& cx, int t) {
        const float k = (float)std::fmod(t / 200.0, 1.0);
        dr::star(cx, { tileX(0) + w / 2, y0 + h / 2 }, 40 + 260 * k, 18 + 120 * k, 5, -90 + t * 0.8f, WHITE);
    };
    star->fn = [=](const Ctx& cx, int) { media::drawImage(cx, *photo, { tileX(0), y0, w, h }, { media::Fit::Cover }); };
    // Luma: a soft white-to-black spot lights a dot field.
    TNode* spot = st->add(0, len);
    spot->isolate().maskMode = MaskMode::Luma;
    spot->layer->mask = [=](const Ctx& cx, int t) {
        dr::rect(cx, tileX(1), y0, w, h, BLACK);
        const Vector2 c{ tileX(1) + w / 2 + 80 * std::cos(t * 0.03f), y0 + h / 2 + 90 * std::sin(t * 0.041f) };
        DrawCircleGradient(mapP(cx, c), 120 * cx.s, WHITE, BLACK);
    };
    spot->fn = [=](const Ctx& cx, int) {
        dr::rect(cx, tileX(1), y0, w, h, mkColor(0x0c2a30));
        for (int j = 0; j < 12; j++) for (int i = 0; i < 11; i++)
            dr::circle(cx, { tileX(1) + 20 + i * 24.f, y0 + 18 + j * 24.f }, 7, TEAL);
    };
    // Inverted: a breathing circle cut out of a panel.
    TNode* hole = st->add(0, len);
    hole->isolate().invertMask = true;
    hole->layer->mask = [=](const Ctx& cx, int t) { dr::circle(cx, { tileX(2) + w / 2, y0 + h / 2 }, 70 + 40 * std::sin(t * 0.05f), WHITE); };
    hole->fn = [=](const Ctx& cx, int) {
        dr::roundRect(cx, tileX(2), y0, w, h, 16, AMBER);
        say(cx, bold(), "cut", { tileX(2) + 24, y0 + 22 }, 34, BG);
    };
    st->add(0, len)->fn = [=](const Ctx& cx, int) {
        const char* names[3] = { "alpha: a star reveal", "luma: soft spotlight", "invertMask: cut-out" };
        for (int i = 0; i < 3; i++) say(cx, mono(), names[i], { tileX(i) + w / 2, y0 + h + 22 }, 18, DIM, 0.5, 0);
    };
}

// ---------------------------------------------------------------- 09 transitions
void stageTransitions(TNode* st, int len, int) {
    TNode* mini = st->add(0, len);
    TransitionSeries ts(mini->kids, SW, SH);
    struct Card { const char* letter; Color col; const char* how; };
    static const Card CARDS[4] = { { "A", TEAL, "scene A" }, { "B", ROSE, "Transition::fade(30)" },
                                   { "C", AMBER, "slide(30, Direction::FromBottom)" },
                                   { "D", IRIS, "slide(30, Direction::FromRight)" } };
    const Transition tr[3] = { Transition::fade(30), Transition::slide(30, Direction::FromBottom),
                               Transition::slide(30, Direction::FromRight) };
    for (int i = 0; i < 4; i++) {
        if (i) ts.transition(tr[i - 1]);
        const Card c = CARDS[i];
        const bool last = i == 3;
        ts.add(150)->fn = [c, last](const Ctx& cx, int t) {
            dr::rectGrad4(cx, 0, 0, SW, SH, mixC(PANEL, c.col, 0.45f), mixC(PANEL, c.col, 0.25f), mixC(BG, c.col, 0.1f), mixC(BG, c.col, 0.3f));
            say(cx, bold(), c.letter, { SW / 2, SH / 2 - 40 }, 320, withAlpha(INK, 0.92f), 0.5, 0.5);
            pill(cx, { SW / 2, SH - 120 }, c.how, mono(), 24, INK, withAlpha(BG, 0.55f), 0.5);
            if (last) say(cx, regular(), "next: a real wipe  \xE2\x86\x92", { SW - 40, SH - 50 }, 26, withAlpha(INK, rise(t, 40, 30)), 1, 0.5);
        };
    }
}

// ---------------------------------------------------------------- 10 sequencing
void stageSequencing(TNode* st, int len, int) {
    TNode* root = st->add(0, len);
    root->loopEvery = 240;
    const float X0 = 220, X1 = 680;
    auto X = [=](float f) { return X0 + (X1 - X0) * f / 240; };
    const float lanes[3] = { 140, 390, 640 };
    root->fn = [=](const Ctx& cx, int t) {
        const char* names[3] = { "Series", "loop()", "freeze()" };
        const char* code[3] = { "s.add(90); s.add(90, -10); s.add(60, 10);", "loop(kids, 20, 50, 4)", "freeze(kids, 100, 80, 15)" };
        for (int i = 0; i < 3; i++) {
            const float y = lanes[i];
            say(cx, bold(), names[i], { 40, y - 4 }, 30, INK, 0, 0.5);
            dr::roundRect(cx, X0, y - 22, X1 - X0, 36, 8, withAlpha(LINE, 0.5f));
            say(cx, mono(), code[i], { X0, y + 40 }, 16, DIM);
            dr::circle(cx, { 820, y }, 86, mkColor(0x0b1020));
        }
        const Color sc[3] = { TEAL, ROSE, AMBER };
        const int sf[3] = { 0, 80, 180 }, sd[3] = { 90, 90, 60 };
        for (int i = 0; i < 3; i++)
            dr::roundRect(cx, X(sf[i]), lanes[0] - 22 + (i == 1 ? 6 : 0), X(sf[i] + sd[i]) - X(sf[i]), 30 - (i == 1 ? 6 : 0), 7, withAlpha(sc[i], 0.8f));
        for (int k = 0; k < 4; k++) dr::roundRect(cx, X(20 + k * 50.f) + 1, lanes[1] - 22, X(70) - X(20) - 2, 30, 7, withAlpha(IRIS, 0.75f));
        dr::roundRect(cx, X(0), lanes[2] - 22, X(100) - X(0), 30, 7, withAlpha(MINT, 0.75f));
        dr::roundRect(cx, X(100) + 1, lanes[2] - 22, X(180) - X(100) - 2, 30, 7, withAlpha(MINT, 0.3f));
        say(cx, mono(), "held", { (X(100) + X(180)) / 2, lanes[2] - 7 }, 15, INK, 0.5, 0.5);
        dr::roundRect(cx, X(180) + 1, lanes[2] - 22, X(240) - X(180) - 1, 30, 7, withAlpha(MINT, 0.75f));
        const float px = X((float)t);
        dr::rect(cx, px - 1.5f, 70, 3, 620, withAlpha(INK, 0.8f));
        say(cx, mono(), fmt("t = %d", t), { px, 52 }, 18, INK, 0.5, 0.5);
    };
    // The real nodes. Their content sees remapped local time, drawn in the circles.
    auto label = [](const Ctx& cx, Vector2 c, int t) { say(cx, mono(), fmt("local t = %d", t), { c.x, c.y + 104 }, 16, DIM, 0.5, 0.5); };
    Series s(root);
    const Color sc[3] = { TEAL, ROSE, AMBER };
    const int durs[3] = { 90, 90, 60 }, offs[3] = { 0, -10, 10 };
    for (int i = 0; i < 3; i++) {
        const Color col = sc[i];
        s.add(durs[i], offs[i])->fn = [=](const Ctx& cx, int t) {
            const Vector2 c{ 820, lanes[0] };
            const Ctx k = sub(cx, c, t * 2.f, pop(t, 0, 200, 14));
            if (i == 0) dr::circle(k, { 0, 0 }, 52, col);
            else if (i == 1) dr::star(k, { 0, 0 }, 62, 26, 5, -90, col);
            else dr::roundRect(k, -48, -48, 96, 96, 14, col);
            if (i != 1 || t >= 10) label(cx, c, t);
        };
    }
    loop(root->kids, 20, 50, 4)->fn = [=](const Ctx& cx, int t) {
        const Vector2 c{ 820, lanes[1] };
        dr::ring(cx, c, 16 + t * 1.3f, 8, withAlpha(IRIS, 1 - t / 55.f));
        dr::circle(cx, c, 16, IRIS);
        label(cx, c, t);
    };
    auto needle = [=](const Ctx& cx, int t, bool held) {
        const Vector2 c{ 820, lanes[2] };
        for (int k = 0; k < 12; k++) {
            const float a = (float)(k * PI / 6);
            dr::strokeSeg(cx, { c.x + 62 * std::cos(a), c.y + 62 * std::sin(a) }, { c.x + 72 * std::cos(a), c.y + 72 * std::sin(a) }, 3, FAINT);
        }
        const float a = (float)(t * 3 * PI / 180 - PI / 2);
        dr::strokeSeg(cx, c, { c.x + 60 * std::cos(a), c.y + 60 * std::sin(a) }, 5, held ? INK : MINT);
        dr::circle(cx, c, 7, MINT);
        label(cx, c, t);
    };
    root->add(0, 100)->fn = [=](const Ctx& cx, int t) { needle(cx, t, false); };
    freeze(root->kids, 100, 80, 15)->fn = [=](const Ctx& cx, int t) { needle(cx, t, true); };
    root->add(180, 60)->fn = [=](const Ctx& cx, int t) { needle(cx, t + 180, false); };
}

// ---------------------------------------------------------------- 11 props
void stageProps(TNode* st, int len, int) {
    static const std::vector<std::vector<std::string>> STATES = {
        { "{", "  \"title\": \"Hello\",", "  \"accent\": \"#47dad8\",", "  \"items\": [\"Fast\", \"Small\"]", "}" },
        { "{", "  \"title\": \"Launch day\",", "  \"accent\": \"#ff5fa8\",", "  \"items\": [\"Springs\", \"Layers\",", "            \"3D\"]", "}" },
        { "{", "  \"title\": \"v2.0\",", "  \"accent\": \"#ffc365\",", "  \"items\": [\"Props\", \"Prefabs\",", "            \"Media\", \"Audio\"]", "}" },
    };
    auto parsed = std::make_shared<std::vector<Json>>();
    for (const auto& lines : STATES) {
        std::string text;
        for (const auto& l : lines) text += l + "\n";
        parsed->push_back(Json::parse(text));
    }
    st->add(0, len)->fn = [parsed](const Ctx& cx, int t) {
        const int k = std::min(2, t / 160), lt = t - k * 160;
        const Json& p = (*parsed)[(size_t)k];
        drawCode(cx, { "$ raymotion render card --props @card.json" }, 30, 28, 20, 30, 999);
        dr::roundRect(cx, 30, 80, 420, 330, 14, mkColor(0x0a0f1e));
        say(cx, mono(), "card.json", { 50, 104 }, 17, FAINT, 0, 0.5);
        drawCode(cx, STATES[(size_t)k], 50, 142, 22, 34, 999);
        if (lt < 24) dr::roundRect(cx, 30, 80, 420, 330, 14, withAlpha(AMBER, 0.25f * (1 - lt / 24.f)));
        TextStyle note{ regular(), 23 };
        note.color = DIM; note.lineHeight = 1.4f;
        dr::textBox(cx, "--props is merged over defaultProps, key by key. build() runs once with the result, "
                        "before the first frame, so props can change content, size and length.", { 30, 440, 420, 300 }, note);
        // The card, built from the parsed props.
        const std::string title = p.str("title", "");
        const Color accent = p.color("accent", WHITE);
        const float cx0 = 490, cy0 = 80, cw = 430, ch = 640;
        dr::roundRect(cx, cx0, cy0, cw, ch, 18, PANEL2);
        dr::roundRect(cx, cx0, cy0, 12, ch, 6, accent);
        glow(cx, { cx0 + cw / 2, cy0 + 80 }, 260, accent, 0.18f);
        const float tk = pop(lt, 0, 170, 13);
        say(cx, bold(), title, { cx0 + 40, cy0 + 50 + 20 * (1 - tk) }, 58, withAlpha(INK, std::min(1.f, tk)));
        const auto& items = p["items"].asArray();
        for (size_t i = 0; i < items.size(); i++) {
            const float ik = pop(lt, 12 + (int)i * 8, 170, 13);
            const Ctx ic = sub(cx, { cx0 + 40, cy0 + 190 + i * 96.f }, 0, 1);
            Ctx fade = ic; fade.a *= std::min(1.f, ik);
            dr::roundRect(fade, 40 * (1 - ik), 0, cw - 80, 72, 36, withAlpha(accent, 0.22f));
            dr::circle(fade, { 40 * (1 - ik) + 36, 36 }, 10, accent);
            say(fade, bold(), items[i].asString(), { 40 * (1 - ik) + 64, 36 }, 30, INK, 0, 0.5);
        }
    };
}

// ---------------------------------------------------------------- 12 prefabs
void stagePrefabs(TNode* st, int len, int) {
    prefab::add({
        "promo-badge",
        Json::parse(R"({"label": "", "hue": 190, "delay": 0})"),
        [](TNode& n, const Json& p) {
            const std::string label = p.str("label", "");
            const float hue = (float)p.num("hue", 190);
            const int delay = p.integer("delay", 0);
            n.up = [delay](int t, NodeProps& pr) {
                const float k = pop(t, delay, 160, 12);
                pr.scale = k;
                pr.opacity = std::min(1.f, k * 1.5f);
            };
            n.fn = [label, hue](const Ctx& cx, int t) {
                const Color c = ColorFromHSV(hue, 0.6f, 0.95f);
                dr::circle(cx, { 0, 0 }, 72, withAlpha(c, 0.16f));
                dr::ring(cx, { 0, 0 }, 72, 4, c);
                dr::arc(cx, { 0, 0 }, 86, 4, t * 3.f, t * 3.f + 90, withAlpha(c, 0.8f));
                dr::star(cx, { 0, 0 }, 32, 14, 5, -90 + t * 1.5f, c);
                say(cx, bold(), label, { 0, 106 }, 24, INK, 0.5, 0.5);
            };
        },
    });
    static const char* NAMES[12] = { "Springs", "Easing", "Layers", "Masks", "Layout", "Text",
                                     "Media", "3D", "Audio", "Props", "Series", "Render" };
    for (int i = 0; i < 12; i++) {
        Json props;
        props.set("label", NAMES[i]);
        props.set("hue", 160.0 + i * 17);
        props.set("delay", 10 + i * 6);
        prefab::use(st, 0, len, "promo-badge", std::move(props))->props.pos = { 130 + (i % 4) * 230.f, 130 + (i / 4) * 250.f };
    }
}

// ---------------------------------------------------------------- 13 media
void stageMedia(TNode* st, int len, int) {
    const Rectangle tiles[4] = { { 25, 25, 440, 350 }, { 485, 25, 440, 350 }, { 25, 405, 440, 350 }, { 485, 405, 440, 350 } };
    const char* labels[4] = { "SVG: re-rasterised at every size", "PNG: Fit::Cover, panning", "GIF: plays from the node's frame 0",
                              "video: frame-accurate decode" };
    for (int i = 0; i < 4; i++) {
        const Rectangle r = tiles[i];
        TNode* tile = st->add(0, len);
        tile->props.pos = { r.x, r.y };
        tile->isolate().mask = [r](const Ctx& cx, int) { dr::roundRect(cx, 0, 0, r.width, r.height, 16, WHITE); };
        tile->fn = [r](const Ctx& cx, int) { dr::rect(cx, 0, 0, r.width, r.height, mkColor(0x0a0f1e)); };
        TNode* content = tile->add(0, len);
        if (i == 0) {
            auto logo = media::image(asset("logo.svg"));
            content->fn = [logo, r](const Ctx& cx, int t) {
                const float z = 0.75f + 1.6f * (0.5f - 0.5f * std::cos(t * 0.013f));
                const float side = 280 * z;
                media::drawImage(cx, *logo, { r.width / 2 - side / 2, r.height / 2 - side / 2, side, side });
            };
        } else if (i == 1) {
            auto photo = media::image(asset("photo.png"));
            content->fn = [photo, r](const Ctx& cx, int t) {
                media::ImageStyle s{ media::Fit::Cover };
                s.align = { 0.5f + 0.5f * std::sin(t * 0.012f), 0.5f };
                media::drawImage(cx, *photo, { 0, 0, r.width, r.height }, s);
            };
        } else if (i == 2) {
            if (haveAsset("loop.gif")) content->fn = media::imageFn(media::image(asset("loop.gif")), { 70, 25, 300, 300 });
            else content->fn = [](const Ctx& cx, int) { say(cx, mono(), "(needs ffmpeg)", { 220, 175 }, 22, DIM, 0.5, 0.5); };
        } else {
            if (haveAsset("clip.mp4")) {
                media::VideoOptions v;
                v.loop = true; v.muted = true;
                media::video(content, asset("clip.mp4"), { 0, 0, r.width, r.height }, v);
            } else content->fn = [](const Ctx& cx, int) { say(cx, mono(), "(needs ffmpeg)", { 220, 175 }, 22, DIM, 0.5, 0.5); };
        }
        tile->add(0, len)->fn = [r, l = labels[i]](const Ctx& cx, int) {
            pill(cx, { 16, r.height - 30 }, l, mono(), 17, INK, withAlpha(BG, 0.75f));
        };
    }
}

// ---------------------------------------------------------------- 14 GPU 3D
void stageGpu(TNode* st, int len, int) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->environmentGenerator = g3d::studioEnvironment(mkColor(0x3a4670), mkColor(0x1a2036), mkColor(0x0c0e18), 4);
    scene->environmentIntensity = 0.95f;
    scene->background = PANEL;
    scene->fogColor = PANEL;
    scene->fogNear = 15; scene->fogFar = 32;
    scene->add(g3d::Geometry::plane(80, 80), { mkColor(0x2a3048), 0.85f });
    g3d::Material stone;
    stone.color = mkColor(0x8a90a8); stone.roughness = 0.75f;
    struct Piece { g3d::GeometryPtr geo; g3d::Material mat; };
    g3d::Material tiles;
    tiles.map = asset("tiles.png"); tiles.roughness = 0.5f;
    std::vector<Piece> pieces = {
        { g3d::Geometry::box(1.1f, 1.1f, 1.1f), tiles },
        { g3d::Geometry::sphere(0.68f), { AMBER, 0.15f, 0.85f } },
        { g3d::Geometry::cylinder(0.5f, 1.2f), { IRIS, 0.45f } },
        { g3d::Geometry::cone(0.62f, 1.3f), { ROSE, 0.4f } },
        { g3d::Geometry::torus(0.5f, 0.18f), { MINT, 0.3f, 0.2f } },
    };
    for (int i = 0; i < 6; i++) {
        const float a = (float)(i * PI / 3 + PI / 6);
        const Vector3 at{ 4.3f * std::cos(a), 0, 4.3f * std::sin(a) };
        g3d::Object3D* ped = scene->add(g3d::Geometry::cylinder(0.8f, 0.6f, 32), stone);
        ped->position = { at.x, 0.3f, at.z };
        g3d::Object3D* o;
        if (i < 5) o = scene->add(pieces[(size_t)i].geo, pieces[(size_t)i].mat);
        else {
            o = scene->add(g3d::loadModel(asset("gem.obj")));
            o->modelMaterials = false;
            o->material = { mkColor(0x7fe0ff), 0.12f, 0.5f };
        }
        o->up = [i, at](int t, g3d::Object3D& self) {
            const float k = pop(t, 30 + i * 14, 150, 12);
            self.scale = { k, k, k };
            self.position = { at.x, 1.35f + 0.08f * std::sin(t * 0.05f + i), at.z };
            self.rotation = { i == 4 ? 70.f : 0.f, t * 1.2f + i * 40, 0 };
        };
    }
    g3d::Object3D* knot = scene->add(g3d::Geometry::knot(1.0f, 0.32f), { TEAL, 0.22f, 0.6f });
    knot->up = [](int t, g3d::Object3D& o) {
        const float k = pop(t, 6, 80, 9);
        o.scale = { k, k, k };
        o.position = { 0, 2.1f + 0.15f * std::sin(t * 0.04f), 0 };
        o.rotation = { 20, t * 1.4f, t * 0.5f };
    };
    scene->lights.push_back(g3d::Light::directional({ -6, 12, 5 }, mkColor(0xdfe6ff), 1.0f, true));
    scene->lights.back().shadowExtent = 9;
    g3d::Light sweep = g3d::Light::spot({ 6, 8, 6 }, { 0, 0, 0 }, AMBER, 2.2f, 26, true);
    sweep.range = 26;
    sweep.up = [](int t, g3d::Light& l) { const float a = t * 0.02f; l.target = { 3.5f * std::cos(a), 0, 3.5f * std::sin(a) }; };
    scene->lights.push_back(sweep);
    scene->lights.push_back(g3d::Light::point({ 0, 0.9f, 0 }, ROSE, 1.4f, 8));
    scene->camera.fov = 38;
    scene->camera.up = [](int t, g3d::Camera& cam) {
        const float dist = 13.5f - 2.5f * rise(t, 0, 300);
        const float a = 0.9f + t * 0.0045f;
        cam.position = { dist * std::cos(a), 5.2f, dist * std::sin(a) };
        cam.target = { 0, 1.4f, 0 };
        // Rack focus: the knot, then the nearest pedestal, then back.
        const Vector3 d{ cam.position.x, cam.position.y - 2.1f, cam.position.z };
        const float far = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z), nearF = far - 4.3f;
        const float rack = rise(t, 380, 50) * fall(t, 520, 50);
        cam.dof = { far + (nearF - far) * rack, 1.15f, 11 };
    };
    st->add(0, len)->fn = g3d::draw(scene, { 0, 0, SW, SH });
    // A second camera looking straight down, in its own viewport.
    auto top = std::make_shared<g3d::Camera>();
    top->position = { 0, 17, 0.01f };
    top->target = { 0, 0, 0 };
    top->fov = 48;
    const Rectangle inset{ SW - 24 - 300, SH - 24 - 200, 300, 200 };
    TNode* pip = st->add(240, len - 240);
    pip->up = [](int t, NodeProps& p) { p.opacity = rise(t, 0, 20); };
    pip->add(0, len)->fn = [inset](const Ctx& cx, int) { dr::roundRect(cx, inset.x - 4, inset.y - 4, inset.width + 8, inset.height + 8, 10, withAlpha(INK, 0.85f)); };
    pip->add(0, len)->fn = g3d::draw(scene, inset, top);
    st->add(0, len)->fn = [](const Ctx& cx, int t) {
        struct Chip { int at; const char* text; };
        static const Chip CHIPS[] = { { 60, "image-based lighting" }, { 130, "PCF shadows from 3 lights" },
                                      { 200, "rough, metal and textured materials" }, { 240, "a second camera, own viewport" },
                                      { 380, "depth of field: rack focus" }, { 460, "gem.obj: generated, then loaded" },
                                      { 540, "linear fog" } };
        for (int i = 0; i < 7; i++) {
            const float a = rise(t, CHIPS[i].at, 20);
            if (a <= 0) continue;
            Ctx k = sub(cx, { -20 * (1 - a), 0 });
            k.a *= a;
            pill(k, { 24, 40 + i * 46.f }, CHIPS[i].text, mono(), 18, INK, withAlpha(BG, 0.7f));
        }
    };
}

// ---------------------------------------------------------------- 15 audio
void stageAudio(TNode* st, int len, int abs0) {
    const std::string score = asset("score.wav");
    st->add(0, len)->fn = [score, abs0](const Ctx& cx, int t) {
        const std::vector<float>& pcm = media::decodeAudio(score, 48000);
        const long frames = (long)(pcm.size() / 2);
        const double now = (abs0 + t) / (double)FPS;
        auto sample = [&](long i, int ch) { return i >= 0 && i < frames ? pcm[(size_t)i * 2 + ch] : 0.f; };
        // Waveform, two seconds around now.
        const float wx = 40, ww = 870;
        for (int ch = 0; ch < 2; ch++) {
            const float cy = 100 + ch * 140.f;
            dr::rect(cx, wx, cy, ww, 1, withAlpha(LINE, 0.8f));
            say(cx, mono(), ch ? "R" : "L", { wx, cy - 54 }, 16, FAINT);
            for (int c = 0; c < 290; c++) {
                const long s0 = (long)((now - 1 + c / 290.0 * 2) * 48000), s1 = (long)((now - 1 + (c + 1) / 290.0 * 2) * 48000);
                float lo = 0, hi = 0;
                for (long i = s0; i < s1; i += 4) { const float v = sample(i, ch); lo = std::min(lo, v); hi = std::max(hi, v); }
                const float x = wx + c * ww / 290.f;
                dr::rect(cx, x, cy - hi * 58, ww / 290.f - 0.6f, (hi - lo) * 58 + 1, c < 145 ? TEAL : withAlpha(TEAL, 0.45f));
            }
        }
        dr::rect(cx, wx + ww / 2 - 1, 34, 2, 136 + 140, AMBER);
        say(cx, mono(), fmt("%.2f s", now), { wx + ww / 2 + 10, 34 }, 16, AMBER);
        // Spectrum: Goertzel at log-spaced bands over 2048 windowed samples.
        const long c0 = (long)(now * 48000) - 1024;
        const int BANDS = 40;
        const float sx = 40, sw = 870, sy = 560, sh = 220 - 30;
        for (int b = 0; b < BANDS; b++) {
            const double f = 50 * std::pow(12000.0 / 50, b / (double)(BANDS - 1));
            const double w = 2 * PI * f / 48000, coef = 2 * std::cos(w);
            double s1 = 0, s2 = 0;
            for (int i = 0; i < 2048; i++) {
                const double win = 0.5 - 0.5 * std::cos(2 * PI * i / 2047);
                const double x = 0.5 * (sample(c0 + i, 0) + sample(c0 + i, 1)) * win;
                const double s0 = x + coef * s1 - s2;
                s2 = s1; s1 = s0;
            }
            const double power = s1 * s1 + s2 * s2 - coef * s1 * s2;
            const float db = (float)(10 * std::log10(power + 1e-9));
            const float hgt = std::clamp((db + 10) / 60.f, 0.02f, 1.f) * sh;
            const float bw = sw / BANDS;
            dr::roundRect(cx, sx + b * bw + 2, sy - hgt, bw - 4, hgt, 3, ColorFromHSV(170 + b * 4.f, 0.6f, 0.95f));
        }
        say(cx, mono(), "50 Hz", { sx, sy + 12 }, 15, FAINT);
        say(cx, mono(), "12 kHz", { sx + sw, sy + 12 }, 15, FAINT, 1, 0);
        // Levels and the beat.
        for (int ch = 0; ch < 2; ch++) {
            double acc = 0;
            for (long i = (long)(now * 48000) - 2400; i < (long)(now * 48000); i++) acc += sample(i, ch) * sample(i, ch);
            const float rms = (float)std::sqrt(acc / 2400), db = 20 * std::log10(rms + 1e-6f);
            const float k = std::clamp((db + 48) / 48, 0.f, 1.f);
            const float y = 612 + ch * 34.f;
            say(cx, mono(), ch ? "R" : "L", { 40, y + 10 }, 16, FAINT, 0, 0.5);
            dr::roundRect(cx, 64, y, 600, 20, 10, withAlpha(LINE, 0.6f));
            dr::roundRect(cx, 64, y, 600 * k, 20, 10, k > 0.9f ? AMBER : MINT);
        }
        const float beat = (float)std::fmod(now, 0.5) / 0.5f;
        dr::circle(cx, { 820, 640 }, 26 + 14 * (1 - beat), withAlpha(ROSE, 0.9f * (1 - beat) + 0.1f));
        say(cx, mono(), "120 bpm", { 820, 690 }, 16, DIM, 0.5, 0);
        static const char* OPTS[] = { "startFrom", "endAt", "loop", "playbackRate", "volumeAt(t)" };
        float x = 40;
        for (const char* o : OPTS) x += pill(cx, { x, 740 }, o, mono(), 17, INK, withAlpha(IRIS, 0.25f)) + 12;
    };
}

// ---------------------------------------------------------------- 16 rendering
void stageRender(TNode* st, int len, int) {
    struct Line { int at; std::string text; };
    const int total = totalFrames();
    static std::vector<Line> script;
    script = {
        { 6, "$ raymotion list" },
        { 44, fmt("%-22s %dx%d @%dfps  %.1fs  audio=%d", "raymotion-promo", 1920, 1080, FPS, total / (double)FPS, 1) },
        { 70, "$ raymotion preview raymotion-promo" },
        { 118, "  a live window with sound; space pauses, esc quits" },
        { 146, "$ raymotion still raymotion-promo --at 4812" },
        { 196, "wrote out/raymotion-promo_still_4812.png" },
        { 222, "$ raymotion render raymotion-promo --encoder hevc_nvenc" },
    };
    st->add(0, len)->fn = [total](const Ctx& cx, int t) {
        dr::rect(cx, 0, 0, SW, 52, mkColor(0x161e38));
        const Color dots[3] = { mkColor(0xff6b6b), mkColor(0xffc365), mkColor(0x6be07a) };
        for (int k = 0; k < 3; k++) dr::circle(cx, { 28 + k * 22.f, 26 }, 7, dots[k]);
        say(cx, mono(), "terminal", { SW / 2, 26 }, 18, FAINT, 0.5, 0.5);
        std::vector<std::string> shown;
        int chars = 0;
        for (const Line& l : script) {
            if (t < l.at) break;
            const bool cmd = l.text[0] == '$';
            const int n = cmd ? std::min((int)l.text.size(), (t - l.at) * 2) : (int)l.text.size();
            shown.push_back(l.text);
            chars += n + 1;
            if (n < (int)l.text.size()) break;
        }
        // The render's progress, in the CLI's own format.
        const int p0 = 290, p1 = 420;
        if (t >= p0) {
            const int f = std::min(total, (int)((t - p0) / (double)(p1 - p0) * total) / 15 * 15);
            shown.push_back(fmt("[%4d/%4d]", f, total));
            chars += 99;
            if (t >= p1 + 10) { shown.push_back(fmt("rendered %d frames -> out/raymotion-promo.mp4", total)); chars += 99; }
        }
        drawCode(cx, shown, 30, 86, 22, 40, chars, true);
        if (t >= p0) {
            const float k = std::clamp((t - p0) / (float)(p1 - p0), 0.f, 1.f);
            const float y = 86 + 7 * 40.f + 6;
            dr::roundRect(cx, 200, y, 690, 22, 11, withAlpha(LINE, 0.7f));
            dr::roundRect(cx, 200, y, 690 * k, 22, 11, TEAL);
        }
        static const char* FLAGS[] = { "--draft", "--ss 4", "--crf 19", "--preset p7", "--png-frames", "--props @file.json" };
        float x = 30;
        for (int i = 0; i < 6; i++) {
            const float a = rise(t, 300 + i * 10, 20);
            x += pill(cx, { x, 740 }, FLAGS[i], mono(), 18, withAlpha(INK, a), withAlpha(IRIS, 0.25f * a)) + 12;
        }
    };
}

// ================================================================== open source
void sceneOpen(TNode* s, int len, int abs0) {
    s->add(0, len)->fn = [abs0](const Ctx& cx, int t) {
        backdrop(cx, abs0 + t);
        for (int i = 0; i < 70; i++) {                          // an orbit of sparks
            const float a = (float)(i * 2 * PI / 70) + t * 0.004f * (i % 2 ? 1 : -1);
            const float r = 800 + 60 * (float)rnd01(601, i);
            glow(cx, { 960 + r * std::cos(a), 520 + r * 0.55f * std::sin(a) }, 12 + 10 * (float)rnd01(602, i),
                 i % 3 ? TEAL : IRIS, 0.35f * rise(t, i, 40));
        }
    };
    s->add(0, len)->fn = [](const Ctx& cx, int t) {
        const float a = rise(t, 10, 30);
        say(cx, mono(), "OPEN SOURCE", { 960, 230 + 14 * (1 - a) }, 26, withAlpha(TEAL, a), 0.5, 0, 4);
        Font f = bold();
        const float size = 92, full = textWidth(f, REPO, size);
        const int n = std::clamp((t - 30) / 2, 0, (int)REPO.size());
        const float x0 = 960 - full / 2;
        say(cx, f, REPO.substr(0, (size_t)n), { x0, 290 }, size, INK);
        if (n < (int)REPO.size() && (t / 16) % 2 == 0) dr::rect(cx, x0 + textWidth(f, REPO.substr(0, (size_t)n), size) + 6, 300, 8, 96, TEAL);
        const float u = rise(t, 100, 40);
        dr::rect(cx, x0, 404, full * u, 5, TEAL);
        glow(cx, { x0 + full * u, 406 }, 50, TEAL, 0.6f * u * fall(t, 140, 40));
        // How to start.
        const float b = rise(t, 150, 30);
        Ctx k = sub(cx, { 0, 24 * (1 - b) });
        k.a *= b;
        dr::roundRect(k, 420, 480, 1080, 220, 18, LINE);
        dr::roundRect(k, 422, 482, 1076, 216, 17, PANEL);
        static const std::vector<std::string> CMDS = {
            "$ git clone https://github.com/pebaryan/raymotion.git",
            "$ cmake -S raymotion -B build && cmake --build build",
            "$ build/raymotion preview raymotion-promo" };
        drawCode(k, CMDS, 470, 516, 28, 56, std::max(0, (t - 170) * 3));
        const float c = rise(t, 380, 30);
        const float sk = pop(t, 380, 160, 10);
        dr::star(sub(cx, { 600, 820 }, t * 0.5f, sk), { 0, 0 }, 34, 15, 5, -90, AMBER);
        say(cx, regular(), "Star it, fork it, and make your own videos in C++", { 650, 820 }, 40, withAlpha(INK, c), 0, 0.5);
    };
}

// ================================================================== credits
void sceneCredits(TNode* s, int len, int abs0) {
    s->add(0, len)->fn = [abs0](const Ctx& cx, int t) { backdrop(cx, abs0 + t); };
    struct Entry { const char* role; const char* name; const char* note; };
    static const Entry ENTRIES[] = {
        { "", "A film made entirely in code", "" },
        { "Created by", "pebaryan", "" },
        { "Written, animated and scored by", "Claude", "Opus 5.5, Anthropic" },
        { "Engine", "raymotion", "C++17 on raylib and ffmpeg" },
        { "Built on", "raylib", "zlib licence" },
        { "Encoding and decoding", "ffmpeg", "" },
        { "Typeface", "Inter", "SIL Open Font License 1.1" },
        { "Image and SVG decoding", "stb_image, nanosvg", "" },
        { "Assets", "Every shape, texture, model and note", "generated by code" },
        { "Rendered with raymotion", "1920 x 1080, 60 fps", "" },
    };
    s->add(0, len)->fn = [](const Ctx& cx, int t) {
        constexpr int N = (int)(sizeof(ENTRIES) / sizeof(ENTRIES[0]));
        const float scroll = 3.2f;
        for (int i = 0; i < N; i++) {
            const Entry& e = ENTRIES[i];
            const float y = 1120 + i * 135.f - t * scroll;
            if (y < -100 || y > 1200) continue;
            const float a = std::clamp((y - 160) / 160.f, 0.f, 1.f) * std::clamp((980 - y) / 140.f, 0.f, 1.f);
            if (a <= 0) continue;
            if (e.role[0]) say(cx, mono(), upper(e.role), { 960, y - 36 }, 20, withAlpha(TEAL, a), 0.5, 0.5, 3);
            say(cx, bold(), e.name, { 960, y + 8 }, i == 0 ? 52 : 48, withAlpha(INK, a), 0.5, 0.5);
            if (e.note[0]) say(cx, regular(), e.note, { 960, y + 52 }, 24, withAlpha(DIM, a), 0.5, 0.5);
        }
        // End card.
        const float k = pop(t, 700, 120, 14), a = rise(t, 700, 30);
        if (a <= 0) return;
        Ctx c = cx;
        c.a *= a;
        logoMark(sub(c, { 960, 400 }, 0, k), { 0, 0 }, 64, t * 2.f);
        say(c, bold(), "raymotion", { 960, 540 }, 110, INK, 0.5, 0.5);
        say(c, mono(), REPO, { 960, 640 }, 34, TEAL, 0.5, 0.5);
        say(c, regular(), "Thanks for watching", { 960, 720 }, 30, DIM, 0.5, 0.5);
    };
}

// ================================================================== chrome
// On top of every chapter: the wordmark, the chapter name, and a scrubber with the
// chapter marks, the timecode and the frame number. Fades from black at the start and
// to black at the end.
void chrome(Nodes& roots) {
    const int total = totalFrames();
    addRoot(roots, 0, total)->fn = [total](const Ctx& cx, int T) {
        dr::rectGrad4(cx, 0, 0, 1920, 160, withAlpha(BLACK, 0.45f), withAlpha(BLACK, 0.45f), withAlpha(BLACK, 0), withAlpha(BLACK, 0));
        dr::rectGrad4(cx, 0, 900, 1920, 180, withAlpha(BLACK, 0), withAlpha(BLACK, 0), withAlpha(BLACK, 0.5f), withAlpha(BLACK, 0.5f));
        const float vis = rise(T, chapterStart(1) - 30, 30) * fall(T, chapterStart(CH_CREDITS), 30);
        if (vis > 0) {
            Ctx k = cx;
            k.a *= vis;
            logoMark(k, { 128, 62 }, 15, T * 2.f);
            say(k, bold(), "raymotion", { 156, 62 }, 28, INK, 0, 0.5);
            const int ch = chapterAt(T);
            const std::string label = ch >= FIRST_FEATURE && ch < FIRST_FEATURE + NFEAT
                ? fmt("%02d  \xC2\xB7  %s", ch - FIRST_FEATURE + 1, upper(CHAPTERS[ch].name).c_str()) : upper(CHAPTERS[ch].name);
            say(k, mono(), label, { 1810, 62 }, 20, DIM, 1, 0.5, 2);
            const float y = 1000, x0 = 110, x1 = 1810;
            dr::rect(k, x0, y - 1.5f, x1 - x0, 3, LINE);
            const float px = x0 + (x1 - x0) * T / (float)total;
            dr::rect(k, x0, y - 1.5f, px - x0, 3, TEAL);
            for (int i = 1; i < NCH; i++) {
                const float x = x0 + (x1 - x0) * chapterStart(i) / (float)total;
                dr::rect(k, x - 1, y - 6, 2, 12, i <= ch ? TEAL : FAINT);
            }
            glow(k, { px, y }, 26, TEAL, 0.6f);
            dr::circle(k, { px, y }, 6, INK);
            const int sec = T / FPS;
            say(k, mono(), fmt("%02d:%02d:%02d", sec / 60, sec % 60, T % FPS), { x0, y + 30 }, 18, DIM, 0, 0.5);
            say(k, mono(), fmt("frame %05d / %d  \xC2\xB7  %d fps", T, total, FPS), { x1, y + 30 }, 18, DIM, 1, 0.5);
        }
        const float black = std::max(fall(T, 0, 30), rise(T, total - 60, 60));
        if (black > 0) dr::rect(cx, 0, 0, 1920, 1080, withAlpha(BLACK, black));
    };
}

// ================================================================== the film
std::vector<std::shared_ptr<Feature>> features() {
    auto F = [](const char* title, const char* desc, const char* file, std::vector<std::string> code, StageFn stage) {
        return std::make_shared<Feature>(Feature{ title, desc, file, std::move(code), std::move(stage) });
    };
    return {
        F("Keyframes & easing", "Keyframe tracks animate any value over frames, with named easings from linear to elastic and bounce.",
          "keyframes.cpp", {
            "TrackFloat x;",
            "x.add(0, 0.f);",
            "x.add(90, 640.f, E_ELASTIC);",
            "",
            "node->up = [x](int t, NodeProps& p) {",
            "    p.pos.x = x.at(t);",
            "};",
            "",
            "// E_LINEAR  E_OUT_CUBIC  E_IN_OUT_EXPO",
            "// E_BACK    E_ELASTIC    E_BOUNCE" }, stageEasing),
        F("Springs", "Physically based springs, solved in closed form, so frame 3,000 costs the same as frame 3. "
                     "Tune mass, stiffness and damping.",
          "springs.cpp", {
            "SpringOptions s;",
            "s.fps = 60;",
            "s.delay = 15;",
            "s.config = {1, 180, 6};  // mass, stiffness,",
            "                         // damping",
            "node->up = [s](int t, NodeProps& p) {",
            "    p.scale = spring(t, s);",
            "};",
            "int n = measureSpring(60, s.config);" }, stageSprings),
        F("Interpolation", "interpolate() maps one range onto another, with easing and four ways to extrapolate. "
                           "interpolateColors() blends RGBA.",
          "interpolate.cpp", {
            "float y = interpolate(t, {0, 60}, {0, 1},",
            "    {ease::inOut(ease::cubic),",
            "     Extrapolate::Clamp, Extrapolate::Clamp});",
            "",
            "Color c = interpolateColors(t, {0, 60, 120},",
            "    {TEAL, IRIS, AMBER});",
            "",
            "Easing css = ease::bezier(0.25, 0.1, 0.25, 1);" }, stageInterp),
        F("Vector drawing", "Stars, blobs, rings, arcs, sectors, gradients and any polygon, ear-clipped, all through one "
                            "transform context.",
          "shapes.cpp", {
            "dr::star(cx, {0, 0}, 120, 50, 5, -90, AMBER);",
            "dr::blob(cx, {0, 0}, 108, 18, 5, t * 3.f, ROSE);",
            "dr::arc(cx, {0, 0}, 100, 16, -90, end, TEAL);",
            "dr::sector(cx, {0, 0}, 115, a0, a1, IRIS);",
            "dr::rectGrad4(cx, x, y, w, h, tl, tr, br, bl);",
            "dr::fillPoly(cx, pts.data(), n, MINT);" }, stageShapes),
        F("Supersampling", "Frames are drawn at 2x or 4x and box-filtered down by exact halving, so every edge, line and "
                           "glyph is smooth.",
          "quality.cpp", {
            "Comp c;",
            "c.supersample = 4;    // 1, 2 or 4",
            "",
            "// or on the command line:",
            "//   raymotion render promo --ss 4",
            "",
            "// you keep drawing in composition pixels:",
            "dr::circle(cx, {960, 540}, 200, TEAL);" }, stageSupersample),
        F("Text & layout", "Flex rows and columns, grids, and word-wrapped, aligned text boxes in the bundled Inter typeface.",
          "layout.cpp", {
            "auto cols = layout::row(frame,",
            "    {layout::px(220), layout::flex(w1),",
            "     layout::flex(w2)}, 20);",
            "",
            "TextStyle st{fonts::ui(), 25};",
            "st.align = Align::Center;",
            "int lines = dr::textBox(cx, text, cols[1], st);",
            "",
            "auto cells = layout::grid(area, 6, 2, 16);" }, stageLayout),
        F("Layers & blending", "isolate() renders a subtree offscreen and composites it as one: group opacity without "
                               "seams, plus Add, Multiply and Screen.",
          "layers.cpp", {
            "// per primitive: overlaps add up",
            "a->props.opacity = 0.5f;",
            "",
            "// as a layer: the group fades as one",
            "Layer& l = b->isolate();",
            "l.opacity = 0.5f;",
            "",
            "// blend the flattened group",
            "red->isolate().blend = Blend::Add;",
            "// Normal, Add, Multiply, Screen" }, stageLayers),
        F("Masks", "Alpha or luma masks, drawn like any other content and animated per frame: reveals, spotlights and cut-outs.",
          "masks.cpp", {
            "Layer& l = word->isolate();",
            "l.mask = [](const Ctx& cx, int t) {",
            "    dr::text(font, cx, \"MASKS\", {0, 0},",
            "             250, 8, WHITE, 0.5, 0.5);",
            "};",
            "",
            "spot->isolate().maskMode = MaskMode::Luma;",
            "hole->isolate().invertMask = true;" }, stageMasks),
        F("Transitions", "TransitionSeries joins scenes with fades, slides and wipes, like the ones carrying you between "
                         "these chapters.",
          "transitions.cpp", {
            "TransitionSeries ts(c.roots, c.w, c.h);",
            "ts.add(150)->fn = sceneA;",
            "ts.transition(Transition::fade(30));",
            "ts.add(150)->fn = sceneB;",
            "ts.transition(Transition::slide(30,",
            "    Direction::FromBottom));",
            "ts.add(150)->fn = sceneC;",
            "ts.transition(Transition::wipe(30));",
            "c.duration = ts.end();" }, stageTransitions),
        F("Sequencing", "Series places clips back to back, loop() repeats one, freeze() holds a frame: Remotion's timing "
                        "model, in C++.",
          "timing.cpp", {
            "Series s(root);",
            "s.add(90)->fn = intro;",
            "s.add(90, -10)->fn = body;   // overlap 10",
            "s.add(60, 10)->fn = outro;   // gap of 10",
            "",
            "loop(root->kids, 20, 50, 4)->fn = pulse;",
            "freeze(root->kids, 100, 80, 15)->fn = needle;" }, stageSequencing),
        F("Props", "Compositions take JSON props. --props merges over the defaults, and the build step can change "
                   "content, size and length.",
          "card.cpp", {
            "c.defaultProps = Json::parse(R\"({",
            "    \"title\": \"Hello\", \"accent\": \"#47dad8\",",
            "    \"items\": [\"Fast\", \"Small\"]})\");",
            "c.build = [](Comp& c, const Json& p) {",
            "    auto title = p.str(\"title\", \"\");",
            "    Color accent = p.color(\"accent\", WHITE);",
            "    for (const Json& it : p[\"items\"].asArray())",
            "        addPill(c, it.asString());",
            "};" }, stageProps),
        F("Prefabs", "Prefabs are named, parameterised subtrees. Register one once, then instantiate it anywhere with "
                     "its own props.",
          "prefabs.cpp", {
            "prefab::add({\"promo-badge\", Json::parse(",
            "    R\"({\"label\": \"\", \"hue\": 190})\"),",
            "    [](TNode& n, const Json& p) {",
            "        // n.up: spring in   n.fn: draw",
            "    }});",
            "",
            "for (int i = 0; i < 12; i++)",
            "    prefab::use(stage, 0, len, \"promo-badge\",",
            "                badgeProps(i));" }, stagePrefabs),
        F("Media", "Images, SVG, animated GIFs and frame-accurate video decode in the background, and their sound "
                   "joins the mix.",
          "media.cpp", {
            "auto logo = media::image(\"logo.svg\");",
            "node->fn = media::imageFn(logo, box);",
            "",
            "media::ImageStyle cover{media::Fit::Cover};",
            "media::drawImage(cx, *photo, box, cover);",
            "",
            "media::VideoOptions v;",
            "v.loop = true;",
            "media::video(clip, \"clip.mp4\", box, v);" }, stageMedia),
        F("GPU 3D", "Three.js-style scenes on the timeline: image-based light, up to eight lights with shadows, fog, "
                    "depth of field and model files.",
          "scene3d.cpp", {
            "auto scene = std::make_shared<g3d::Scene>();",
            "scene->environmentGenerator =",
            "    g3d::studioEnvironment(top, mid, low);",
            "scene->add(g3d::Geometry::knot(1, 0.32f),",
            "           {TEAL, 0.22f, 0.6f});",
            "scene->add(g3d::loadModel(\"gem.obj\"));",
            "scene->lights.push_back(g3d::Light::spot(",
            "    {6, 8, 6}, {0, 0, 0}, AMBER, 2, 26, true));",
            "scene->camera.dof = {12, 1.15f, 11};",
            "node->fn = g3d::draw(scene);" }, stageGpu),
        F("Audio", "Clips trim, loop, change speed and follow volume curves, mixed into the MP4's AAC track. This "
                   "score is synthesised in C++.",
          "audio.cpp", {
            "media::AudioOptions o;",
            "o.startFrom = 2.0;          // seconds",
            "o.loop = true;",
            "o.playbackRate = 1.0;",
            "o.volumeAt = [](int t) {",
            "    return std::min(1.f, t / 60.f);",
            "};",
            "media::audio(root, \"score.wav\", o);" }, stageAudio),
        F("Rendering", "Preview with sound, export stills or MP4s. Frames stream to ffmpeg on a writer thread, through "
                       "x264 or the GPU's encoder.",
          "main.cpp", {
            "static void registerComps(Registry& out) {",
            "    Comp c;",
            "    c.name = \"raymotion-promo\";",
            "    c.w = 1920; c.h = 1080; c.fps = 60;",
            "    // ... fill c.roots ...",
            "    out.push_back(std::move(c));",
            "}",
            "int main(int argc, const char** argv) {",
            "    return runCli(argc, argv, registerComps);",
            "}" }, stageRender),
    };
}

Transition transitionInto(int ch) {
    if (ch == 13) return Transition::wipe(XF, Direction::FromLeft);          // promised by the Transitions chapter
    if (ch == 1 || ch == CH_OPEN || ch == CH_CREDITS) return Transition::fade(XF);
    if (ch == FIRST_FEATURE || ch == 17) return Transition::slide(XF, Direction::FromBottom);
    return Transition::slide(XF, Direction::FromRight);
}

bool assetsReady = false;   // set by the assets hook; `list` builds without it

void buildPromo(Comp& c) {
    c.roots.clear();
    c.duration = totalFrames();
    if (!assetsReady) {   // metadata only: no generated files to read yet
        media::audio(addRoot(c.roots, 0, c.duration), asset("score.wav"));
        return;
    }
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    const auto feats = features();
    for (int ch = 0; ch < NCH; ch++) {
        if (ch) ts.transition(transitionInto(ch));
        const int len = CHAPTERS[ch].len + (ch + 1 < NCH ? XF : 0);
        TNode* s = ts.add(len);
        const int abs0 = chapterStart(ch);
        if (ch == 0) sceneIntro(s, len, abs0);
        else if (ch == 1) sceneIdea(s, len, abs0);
        else if (ch == 2) sceneHood(s, len, abs0);
        else if (ch == 3) sceneTree(s, len, abs0);
        else if (ch < FIRST_FEATURE + NFEAT) featureScene(s, len, abs0, ch - FIRST_FEATURE + 1, feats[(size_t)(ch - FIRST_FEATURE)]);
        else if (ch == CH_OPEN) sceneOpen(s, len, abs0);
        else sceneCredits(s, len, abs0);
    }
    c.duration = ts.end();
    chrome(c.roots);
    media::audio(addRoot(c.roots, 0, c.duration), asset("score.wav"));
}

} // namespace

void registerPromo(Registry& out) {
    Comp c;
    c.name = "raymotion-promo";
    c.w = 1920; c.h = 1080; c.fps = FPS;
    c.background = BG;
    c.duration = totalFrames();
    c.assets = [](const Comp&, const Json&) { ensureAssets(); assetsReady = true; };
    c.build = [](Comp& comp, const Json&) { buildPromo(comp); };
    out.push_back(std::move(c));
}

} // namespace rmtn
