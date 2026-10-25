// subtitle_tests.cpp -- sub:: parsing (SRT, WebVTT, markup, errors), phrase timing and
// spans, SRT round trips, tts:: scripts through a stand-in engine (this program run with
// --speak), and burned-in captions (GL, skipped without a window).
#include "rmtn/raymotion.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace rmtn;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static bool near(double a, double b, double tol = 1e-6) { return std::fabs(a - b) <= tol; }

// The stand-in engine: 0.1 s of tone per character of the text file, as 16-bit mono WAV.
static int speak(const char* textfile, const char* out) {
    std::ifstream in(textfile, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const int rate = 16000, n = (int)(text.size() * 0.1 * rate);
    std::vector<float> stereo((size_t)n * 2);
    for (int i = 0; i < n; i++) stereo[2 * i] = stereo[2 * i + 1] = 0.3f * (float)std::sin(i * 0.1);
    return media::writeWav(out, stereo, rate) ? 0 : 1;
}

static void srt() {
    const std::string text =
        "\xEF\xBB\xBF" "1\r\n00:00:01,000 --> 00:00:03,500\r\nHello <b>there</b>,\r\nworld.\r\n\r\n"
        "2\r\n00:00:04,000 --> 00:00:06,000\r\n<i>Then the optimizer\r\nreorders the joins.</i>\r\n\r\n"
        "3\n01:00:00,250 --> 01:00:01,000\n{\\an8}Fish &amp; chips\n";
    const sub::Track t = sub::parse(text);
    CHECK(t.cues.size() == 3);
    CHECK(near(t.cues[0].start, 1) && near(t.cues[0].end, 3.5));
    CHECK(t.cues[0].text == "Hello there,\nworld.");
    CHECK(t.cues[1].italic && !t.cues[0].italic);
    CHECK(t.cues[1].text == "Then the optimizer\nreorders the joins.");
    CHECK(near(t.cues[2].start, 3600.25) && t.cues[2].text == "Fish & chips");
    CHECK(t.at(2) == &t.cues[0] && t.at(3.6) == nullptr && t.at(5) == &t.cues[1]);
    CHECK(near(t.seconds(), 3601) && t.frames(30) == 108030);
    // Round trip through toSrt.
    const sub::Track back = sub::parse(sub::toSrt(t));
    CHECK(back.cues.size() == 3 && back.cues[1].text == t.cues[1].text && back.cues[1].italic);
    CHECK(near(back.cues[2].end, t.cues[2].end, 1e-3));
    bool threw = false;
    try { sub::parse("1\n00:00:xx,000 --> 00:00:02,000\nbad\n"); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
}

static void vtt() {
    const std::string text =
        "WEBVTT\n\nNOTE a comment\nspanning lines\n\n"
        "intro\n00:01.500 --> 00:04.000 align:start position:10%\nFirst <v Roger>line</v>\n\n"
        "00:00:05.000 --> 00:00:07.250\nSecond\n";
    const sub::Track t = sub::parse(text);
    CHECK(t.cues.size() == 2);
    CHECK(near(t.cues[0].start, 1.5) && near(t.cues[0].end, 4) && t.cues[0].text == "First line");
    CHECK(near(t.cues[1].end, 7.25));
}

static void timing() {
    sub::Track t;
    t.cues = { { 0, 4, "Ask a knowledge graph a question.", false },
               { 5, 9, "Next comes the optimizer, which reorders joins.", false },
               { 10, 12, "Finally, results.", false } };
    CHECK(near(t.when("Ask"), 0));
    // "the optimizer" starts 11 characters into a 46-character cue (normalised).
    const double w = t.when("the optimizer");
    CHECK(w > 5.5 && w < 6.6);
    CHECK(near(t.when("REORDERS joins"), t.when("reorders")));
    CHECK(t.frameOf("finally", 30) == 300);
    bool threw = false;
    try { t.when("not said"); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { t.when("Ask", 1); } catch (const std::runtime_error&) { threw = true; }   // only after 1s
    CHECK(threw);
    const auto spans = t.spans({ "Ask", "Next comes", "Finally" }, 30, 15);
    CHECK(spans.size() == 3);
    CHECK(spans[0].from == 0 && spans[1].from == 150 && spans[2].from == 300);
    CHECK(spans[0].length == 150 && spans[2].length == 360 + 15 - 300);
    const sub::Track later = t.shifted(2);
    CHECK(near(later.when("Finally"), 12));
}

static void speech(const std::string& self, const fs::path& dir) {
    tts::Voice v;
    v.command = "\"" + self + "\" --speak {textfile} {out}";
    v.cacheDir = (dir / "tts").string();
    v.wordsPerSecond = 2;
    tts::Script s(v, { "One two three four.", "Five six." }, 0.5, 0.25);
    // Before generating: estimates from the words (4 and 2 words at 2 per second).
    CHECK(!s.lines()[0].spoken && near(s.lines()[0].seconds, 2) && near(s.lines()[1].start, 0.25 + 2 + 0.5));
    s.generate();
    CHECK(s.lines()[0].spoken && s.lines()[1].spoken);
    CHECK(near(s.lines()[0].seconds, 1.9, 0.01) && near(s.lines()[1].seconds, 0.9, 0.01));   // 0.1 s per character
    CHECK(near(s.lines()[1].start, 0.25 + 1.9 + 0.5, 0.01) && near(s.seconds(), 0.25 + 1.9 + 0.5 + 0.9, 0.02));
    const sub::Track track = s.track();
    CHECK(track.cues.size() == 2 && track.cues[1].text == "Five six." && near(track.when("five"), s.lines()[1].start, 1e-6));
    // Cached: a new script measures the takes without speaking; a changed line is new.
    const auto stamp = fs::last_write_time(s.lines()[0].file);
    tts::Script again(v, { "One two three four.", "Seven." }, 0.5, 0.25);
    again.measure();
    CHECK(again.lines()[0].spoken && !again.lines()[1].spoken);
    again.generate();
    CHECK(fs::last_write_time(again.lines()[0].file) == stamp && again.lines()[1].spoken);
    // A failing engine is reported.
    tts::Voice bad = v;
    bad.command = "\"" + self + "\" --fail {textfile} {out}";
    bool threw = false;
    try { tts::Script(bad, { "Nope." }).generate(); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    // Audio attaches at each line's start.
    Comp c;
    c.fps = 30;
    s.attach(addRoot(c.roots, 0, 200), 30);
    CHECK(c.roots[0]->kids.size() == 2 && c.roots[0]->kids[1]->start == (int)std::lround(s.lines()[1].start * 30));
    CHECK(sub::writeSrt((dir / "vo.srt").string(), track) && sub::load((dir / "vo.srt").string()).cues.size() == 2);
}

static void burnIn() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 180, "subtitles");
    if (!IsWindowReady()) { std::puts("no window: caption rendering skipped"); return; }
    sub::Track t;
    t.cues = { { 1, 3, "Hello captions", false } };
    Comp c;
    c.w = 640; c.h = 360; c.fps = 30; c.duration = 120; c.background = BLUE;
    sub::Style style = sub::defaultStyle();
    style.text.size = 40;
    style.bottom = 40;
    style.outline = BLANK;
    style.text.color = { 255, 255, 0, 255 };
    addRoot(c.roots, 0, 120)->fn = sub::captions(t, style);
    auto yellowRows = [&](int frame, int& top, int& bottom) {
        Canvas canvas;
        canvas.load(c.w, c.h, 1);
        presentFrame(c, frame, canvas);
        Image img = LoadImageFromTexture(canvas.output().texture);
        ImageFlipVertical(&img);
        int count = 0;
        top = c.h, bottom = -1;
        for (int y = 0; y < c.h; y++)
            for (int x = 0; x < c.w; x++) {
                const Color p = GetImageColor(img, x, y);
                if (p.r > 200 && p.g > 200 && p.b < 80) { count++; top = std::min(top, y); bottom = std::max(bottom, y); }
            }
        UnloadImage(img);
        canvas.unload();
        return count;
    };
    int top, bottom;
    CHECK(yellowRows(15, top, bottom) == 0);             // before the cue
    CHECK(yellowRows(60, top, bottom) > 100);
    CHECK(bottom < 360 - 40 + 2 && top > 360 - 40 - 60);  // above the bottom margin, one line high
    CHECK(yellowRows(95, top, bottom) == 0);             // after it
    shutdownGraphics();
    CloseWindow();
}

int main(int argc, char** argv) {
    if (argc == 4 && std::string(argv[1]) == "--speak") return speak(argv[2], argv[3]);
    if (argc == 4 && std::string(argv[1]) == "--fail") return 3;
    const fs::path dir = fs::temp_directory_path() / "raymotion-subtitle-test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    srt();
    vtt();
    timing();
    speech(fs::absolute(argv[0]).string(), dir);
    burnIn();
    fs::remove_all(dir, ec);
    if (failures) { std::fprintf(stderr, "%d subtitle check(s) failed\n", failures); return 1; }
    std::puts("subtitle tests passed");
    return 0;
}
