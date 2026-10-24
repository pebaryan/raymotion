// media_tests.cpp -- images (fits, alignment, transforms, JPG/HDR/GIF/SVG decoding, lookup),
// frame-accurate video decoding and the audio mixer (timing, trims, rates, loops, gain,
// Loop/Freeze/nesting, soundtracks of videos). ffmpeg-made fixtures are skipped without it.
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

constexpr int W = 240, H = 160;

static Image render(Comp& c, int frame, int ss = 1) {
    Canvas canvas;
    canvas.load(c.w, c.h, ss);
    presentFrame(c, frame, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}
static bool close(Color a, Color b, int tol = 3) {
    return std::abs(a.r - b.r) <= tol && std::abs(a.g - b.g) <= tol && std::abs(a.b - b.b) <= tol && std::abs(a.a - b.a) <= tol;
}
static Comp comp(int fps = 30, int duration = 90) {
    Comp c;
    c.w = W; c.h = H; c.fps = fps; c.duration = duration; c.background = BLACK;
    return c;
}
// Colour at (x, y) of frame `frame` of a composition drawing `fn`.
static Color pixelOf(std::function<void(const Ctx&, int)> fn, int x, int y, int frame = 0, NodeProps props = {}, int fps = 30) {
    Comp c = comp(fps);
    TNode* n = addRoot(c.roots, 0, c.duration);
    n->fn = std::move(fn);
    n->props = props;
    Image img = render(c, frame);
    Color k = GetImageColor(img, x, y);
    UnloadImage(img);
    return k;
}

static std::string ff;     // ffmpeg, or "" to skip its fixtures

static bool run(const std::string& args) {
    std::string cmd = "\"" + ff + "\" -y -v error -nostdin " + args;
#ifdef _WIN32
    cmd = "\"" + cmd + "\"";            // cmd.exe strips one enclosing pair of quotes
#endif
    return std::system(cmd.c_str()) == 0;
}
static std::string q(const fs::path& p) { return "\"" + p.u8string() + "\""; }

// 64x32: red left half, blue right half.
static fs::path halves(const fs::path& dir) {
    Image img = GenImageColor(64, 32, BLUE);
    ImageDrawRectangle(&img, 0, 0, 32, 32, RED);
    fs::path p = dir / "halves.png";
    ExportImage(img, p.u8string().c_str());
    UnloadImage(img);
    return p;
}

static void imageFits(const fs::path& dir) {
    auto img = media::image(halves(dir).u8string());
    CHECK(img->width() == 64 && img->height() == 32 && img->frameCount() == 1 && !img->isVector());
    auto at = [&](media::ImageStyle st, int x, int y, Rectangle dst = { 0, 0, W, H }) {
        return pixelOf(media::imageFn(img, dst, st), x, y);
    };
    media::ImageStyle fill{ media::Fit::Fill };
    CHECK(close(at(fill, 30, 80), RED) && close(at(fill, 210, 80), BLUE) && close(at(fill, 30, 2), RED));
    // Contain: 240x120, centred vertically, then pinned to the top.
    media::ImageStyle contain;
    CHECK(close(at(contain, 120, 10), BLACK) && close(at(contain, 30, 30), RED) && close(at(contain, 210, 130), BLUE));
    contain.align = { 0.5f, 0 };
    CHECK(close(at(contain, 120, 150), BLACK) && close(at(contain, 30, 5), RED));
    // Cover: 320x160 cropped to the box; centred, then left-aligned (shows image x 0..48).
    media::ImageStyle cover{ media::Fit::Cover };
    CHECK(close(at(cover, 2, 80), RED) && close(at(cover, 237, 80), BLUE));
    cover.align = { 0, 0.5f };
    CHECK(close(at(cover, 150, 80), RED) && close(at(cover, 200, 80), BLUE));
    // None: natural size, centred, nothing outside it.
    media::ImageStyle none{ media::Fit::None };
    CHECK(close(at(none, 100, 80), RED) && close(at(none, 140, 80), BLUE) && close(at(none, 50, 80), BLACK) &&
          close(at(none, 120, 60), BLACK));
    // Through the node transform: rotated a quarter turn about the centre, red ends up on top.
    NodeProps turn;
    turn.pos = { 120, 80 }; turn.rot = 90;
    auto fnTurned = media::imageFn(img, { -60, -30, 120, 60 }, fill);
    CHECK(close(pixelOf(fnTurned, 120, 50, 0, turn), RED) && close(pixelOf(fnTurned, 120, 110, 0, turn), BLUE) &&
          close(pixelOf(fnTurned, 60, 80, 0, turn), BLACK));
    // Opacity and tint.
    NodeProps half;
    half.opacity = 0.5f;
    Color faded = pixelOf(media::imageFn(img, { 0, 0, W, H }, fill), 30, 80, 0, half);
    CHECK(close(faded, Color{ 115, 21, 28, 255 }));                      // raylib's RED is (230, 41, 55)
    media::ImageStyle tinted = fill;
    tinted.tint = Color{ 255, 255, 255, 64 };
    Color t = at(tinted, 30, 80);
    CHECK(std::abs(t.r - 230 * 64 / 255) <= 3);
}

static void imageFormats(const fs::path& dir) {
    // HDR (Radiance RGBE, flat scanline): float RGBA, linear values above 1.
    {
        fs::path p = dir / "two.hdr";
        std::ofstream o(p, std::ios::binary);
        o << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 2\n";
        const unsigned char px[8] = { 128, 0, 0, 130, 0, 128, 0, 129 };   // (2,0,0), (0,1,0)
        o.write((const char*)px, 8);
        o.close();
        Image img = media::loadImage(p.u8string());
        CHECK(img.format == PIXELFORMAT_UNCOMPRESSED_R32G32B32A32 && img.width == 2 && img.height == 1);
        if (img.format == PIXELFORMAT_UNCOMPRESSED_R32G32B32A32) {
            const float* f = (const float*)img.data;
            CHECK(std::fabs(f[0] - 2) < 1e-4f && f[1] == 0 && std::fabs(f[5] - 1) < 1e-4f && f[3] == 1);
        }
        UnloadImage(img);
    }
    // SVG: document size, rasterised for the drawn size, crisp when scaled up.
    {
        fs::path p = dir / "flag.svg";
        std::ofstream(p) << "<svg xmlns='http://www.w3.org/2000/svg' width='100' height='50' viewBox='0 0 100 50'>"
                            "<rect width='50' height='50' fill='#ff0000'/><rect x='50' width='50' height='50' fill='#0000ff'/></svg>";
        auto svg = media::image(p.u8string());
        CHECK(svg->isVector() && svg->width() == 100 && svg->height() == 50);
        int tw = svg->texture(0, 1000).width;
        CHECK(tw >= 1000 && tw <= 1100);
        // Drawn 25 x 12.5 units and scaled up 8x: a raster of the node-local size would
        // smear the edge over ~8 pixels; the re-rasterised one keeps it within one.
        NodeProps big;
        big.scale = 8;
        auto fn = media::imageFn(svg, { 0, 0, 25, 12.5f }, { media::Fit::Fill });
        const Color pureRed{ 255, 0, 0, 255 }, pureBlue{ 0, 0, 255, 255 };
        CHECK(close(pixelOf(fn, 98, 50, 0, big), pureRed, 8) && close(pixelOf(fn, 102, 50, 0, big), pureBlue, 8));
    }
    // Lookup and errors.
    {
        fs::create_directories(dir / "lib");
        fs::copy_file(dir / "halves.png", dir / "lib" / "found.png", fs::copy_options::overwrite_existing);
        media::addSearchPath((dir / "lib").u8string());
        CHECK(fs::path(media::resolve("found.png")) == dir / "lib" / "found.png");
        bool threw = false;
        try { media::resolve("no-such-file.png"); } catch (const std::runtime_error&) { threw = true; }
        CHECK(threw);
        threw = false;
        try { media::image("no-such-file.png")->width(); } catch (const std::runtime_error&) { threw = true; }
        CHECK(threw);
    }
    if (ff.empty()) { std::puts("skip: JPG/GIF fixtures (no ffmpeg)"); return; }
    // JPG decodes without ffmpeg (stb_image), unlike raylib's default build.
    {
        fs::path jpg = dir / "halves.jpg";
        CHECK(run("-i " + q(dir / "halves.png") + " -q:v 2 " + q(jpg)));
        Image img = media::loadImage(jpg.u8string());
        CHECK(img.width == 64 && close(GetImageColor(img, 8, 16), RED, 16) && close(GetImageColor(img, 56, 16), BLUE, 16));
        UnloadImage(img);
    }
    // Animated GIF: red, green, blue at 2 fps, looping.
    {
        const Color cols[3] = { RED, GREEN, BLUE };
        for (int i = 0; i < 3; i++) {
            Image f = GenImageColor(16, 16, cols[i]);
            ExportImage(f, (dir / ("g" + std::to_string(i) + ".png")).u8string().c_str());
            UnloadImage(f);
        }
        fs::path gif = dir / "rgb.gif";
        CHECK(run("-framerate 2 -i " + q(dir / "g%d.png") + " " + q(gif)));
        auto anim = media::image(gif.u8string());
        CHECK(anim->frameCount() == 3 && std::fabs(anim->duration() - 1.5) < 1e-6);
        CHECK(anim->frameAt(0) == 0 && anim->frameAt(0.6) == 1 && anim->frameAt(1.2) == 2 && anim->frameAt(1.6) == 0);
        auto fn = media::imageFn(anim, { 0, 0, W, H }, { media::Fit::Fill });
        // (the GIF palette shifts the colours slightly)
        CHECK(close(pixelOf(fn, 120, 80, 0, {}, 10), RED, 20) && close(pixelOf(fn, 120, 80, 6, {}, 10), GREEN, 20) &&
              close(pixelOf(fn, 120, 80, 12, {}, 10), BLUE, 20) && close(pixelOf(fn, 120, 80, 16, {}, 10), RED, 20));
    }
}

// Frame i of the test video: a colour unique to i.
static Color frameColor(int i) { return Color{ (unsigned char)(i * 8), (unsigned char)(255 - i * 8), (unsigned char)(i % 2 ? 200 : 40), 255 }; }

static Color texel(Texture2D tex) {
    Image img = LoadImageFromTexture(tex);
    Color k = GetImageColor(img, img.width / 2, img.height / 2);
    UnloadImage(img);
    return k;
}

static void video(const fs::path& dir) {
    if (ff.empty()) { std::puts("skip: video (no ffmpeg)"); return; }
    for (int i = 0; i < 30; i++) {
        Image f = GenImageColor(64, 36, frameColor(i));
        char name[32];
        std::snprintf(name, sizeof name, "v%03d.png", i);
        ExportImage(f, (dir / name).u8string().c_str());
        UnloadImage(f);
    }
    // Lossless RGB at 10 fps with a 3 s sine soundtrack.
    const fs::path clip = dir / "clip.mkv";
    CHECK(run("-framerate 10 -i " + q(dir / "v%03d.png") + " -f lavfi -i sine=frequency=440:duration=3:sample_rate=48000"
              " -c:v ffv1 -pix_fmt bgr0 -c:a pcm_s16le -shortest " + q(clip)));
    media::VideoInfo info = media::probe(clip.u8string());
    CHECK(info.width == 64 && info.height == 36 && std::fabs(info.fps - 10) < 1e-9 && info.hasAudio);
    CHECK(std::fabs(info.duration - 3) < 0.05);

    media::VideoDecoder dec(clip.u8string());
    bool exact = true;
    for (int i = 0; i < 30; i++) exact = exact && close(texel(dec.frameAt(i / 10.0)), frameColor(i), 1);
    CHECK(exact);                                                       // sequential
    for (int i : { 25, 3, 17, 29, 0, 28, 1 }) CHECK(close(texel(dec.frameAt(i / 10.0 + 0.05)), frameColor(i), 1));  // seeks
    CHECK(close(texel(dec.frameAt(9.0)), frameColor(29), 1));          // past the end: the last frame
    CHECK(close(texel(dec.frameAt(-1.0)), frameColor(0), 1));

    {   // Determinism: a scrambled visit order shows exactly the sequential frames.
        media::VideoDecoder dec2(clip.u8string());
        bool scrambled = true;
        for (int i : { 29, 4, 17, 0, 25, 8, 12, 1, 21, 3, 29, 0 })
            scrambled = scrambled && close(texel(dec2.frameAt(i / 10.0)), frameColor(i), 1);
        CHECK(scrambled);
        // Two decoders on the same file, interleaved, do not disturb each other.
        media::VideoDecoder a(clip.u8string()), b(clip.u8string());
        bool inter = true;
        for (int i = 0; i < 30; i++)
            inter = inter && close(texel(a.frameAt(i / 10.0)), frameColor(i), 1) &&
                    close(texel(b.frameAt((29 - i) / 10.0)), frameColor(29 - i), 1);
        CHECK(inter);
    }

    // On the timeline: startFrom and playbackRate, looping, a held last frame.
    auto at = [&](media::VideoOptions o, int frame) {
        Comp c = comp(30, 300);
        media::video(addRoot(c.roots, 0, c.duration), clip.u8string(), { 0, 0, W, H }, o);
        Image img = render(c, frame);
        Color k = GetImageColor(img, W / 2, H / 2);
        UnloadImage(img);
        return k;
    };
    media::VideoOptions fast;
    fast.startFrom = 1.0; fast.playbackRate = 2;
    CHECK(close(at(fast, 15), frameColor(20), 1));                     // 1 s + 0.5 s * 2
    media::VideoOptions looped;
    looped.loop = true;
    CHECK(close(at(looped, 105), frameColor(5), 1));                   // 3.5 s -> 0.5 s
    media::VideoOptions trimmed;
    trimmed.startFrom = 0.5; trimmed.endAt = 1.5; trimmed.loop = true;
    CHECK(close(at(trimmed, 36), frameColor(7), 1));                   // 0.5 + (1.2 mod 1.0)
    CHECK(close(at({}, 200), frameColor(29), 1));

    // Its soundtrack joins the mix unless muted.
    Comp c = comp(30, 120);
    media::video(addRoot(c.roots, 30, 30), clip.u8string(), { 0, 0, W, H });
    CHECK(media::hasAudio(c));
    std::vector<float> mix = media::mixAudio(c);
    auto energy = [&](double a, double b) {
        double e = 0;
        for (size_t s = (size_t)(a * 48000); s < (size_t)(b * 48000); s++) e += mix[s * 2] * mix[s * 2];
        return e / ((b - a) * 48000);
    };
    CHECK(energy(1.1, 1.9) > 0.003 && energy(0, 0.95) == 0 && energy(2.05, 3.9) == 0);
    Comp quiet = comp(30, 120);
    media::VideoOptions muted;
    muted.muted = true;
    media::video(addRoot(quiet.roots, 0, 60), clip.u8string(), { 0, 0, W, H }, muted);
    CHECK(!media::hasAudio(quiet));
}

static void audioMix(const fs::path& dir) {
    const int R = 48000;
    // dc.wav: 2 s of constant (0.5, -0.25); ramp.wav: 1 s whose value is its time in seconds.
    std::vector<float> dc((size_t)R * 2 * 2), ramp((size_t)R * 2);
    for (size_t s = 0; s < dc.size() / 2; s++) { dc[s * 2] = 0.5f; dc[s * 2 + 1] = -0.25f; }
    for (size_t s = 0; s < ramp.size() / 2; s++) ramp[s * 2] = ramp[s * 2 + 1] = (float)s / R;
    const std::string dcWav = (dir / "dc.wav").u8string(), rampWav = (dir / "ramp.wav").u8string();
    CHECK(media::writeWav(dcWav, dc, R) && media::writeWav(rampWav, ramp, R));
    Wave w = LoadWave(dcWav.c_str());
    CHECK(w.channels == 2 && w.sampleRate == (unsigned)R && w.frameCount == (unsigned)R * 2);
    UnloadWave(w);

    auto near = [](float a, float b) { return std::fabs(a - b) < 2e-3f; };
    auto L = [&](const std::vector<float>& m, double sec) { return m[(size_t)std::llround(sec * R) * 2]; };

    {   // Starts with its node, ends with it; both channels; a gain curve by local frame.
        Comp c = comp(30, 90);
        media::audio(addRoot(c.roots, 30, 30), dcWav);
        std::vector<float> m = media::mixAudio(c);
        CHECK(m.size() == (size_t)R * 3 * 2);
        CHECK(m[(size_t)(R - 1) * 2] == 0 && near(m[(size_t)R * 2], 0.5f) && near(m[(size_t)R * 2 + 1], -0.25f));
        CHECK(near(L(m, 1.5), 0.5f) && L(m, 2.01) == 0 && L(m, 0.5) == 0);
        Comp g = comp(30, 90);
        media::audio(addRoot(g.roots, 0, 60), dcWav).volumeAt = [](int t) { return t / 30.f; };
        std::vector<float> gm = media::mixAudio(g);
        CHECK(near(L(gm, 0.5), 0.25f) && near(L(gm, 0.25), 0.125f));          // interpolated between frames
    }
    {   // Trim and rate: the value heard at local time tau is 0.25 + 0.5 tau.
        Comp c = comp(30, 90);
        media::AudioOptions o;
        o.startFrom = 0.25; o.playbackRate = 0.5;
        media::audio(addRoot(c.roots, 0, 90), rampWav, o);
        std::vector<float> m = media::mixAudio(c);
        CHECK(near(L(m, 0.5), 0.5f) && near(L(m, 1.0), 0.75f) && L(m, 1.6) == 0);   // past the file's end: silence
    }
    {   // A looping clip: [0, 0.25) of ramp over and over.
        Comp c = comp(30, 60);
        media::AudioOptions o;
        o.endAt = 0.25; o.loop = true;
        media::audio(addRoot(c.roots, 0, 60), rampWav, o);
        std::vector<float> m = media::mixAudio(c);
        CHECK(near(L(m, 0.3), 0.05f) && near(L(m, 1.1), 0.1f));
    }
    {   // <Loop> restarts it, <Freeze> silences it, nesting offsets and clips it.
        Comp c = comp(30, 90);
        media::audio(loop(c.roots, 0, 15, 4), rampWav);
        std::vector<float> m = media::mixAudio(c);
        CHECK(near(L(m, 0.6), 0.1f) && near(L(m, 1.9), 0.4f) && L(m, 2.1) == 0);
        Comp f = comp(30, 90);
        media::audio(freeze(f.roots, 0, 30, 5), rampWav);
        CHECK(!media::mixAudio(f).empty() && L(media::mixAudio(f), 0.5) == 0);
        Comp n = comp(30, 90);
        media::audio(addRoot(n.roots, 30, 15)->add(10, 30), dcWav);
        std::vector<float> nm = media::mixAudio(n);
        CHECK(L(nm, 1.3) == 0 && near(L(nm, 1.4), 0.5f) && L(nm, 1.6) == 0);
    }
    {   // The procedural song still plays, now mixed with clips.
        Comp c = comp(30, 60);
        c.music = true;
        c.song = { { 0, 1, 440, 0.5, false } };
        CHECK(media::hasAudio(c));
        std::vector<float> m = media::mixAudio(c);
        double e = 0;
        for (size_t s = 0; s < (size_t)R; s++) e += m[s * 2] * m[s * 2];
        CHECK(e > 100);
        CHECK(!media::hasAudio(comp()));
    }
}

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(W, H, "raymotion media");
    if (!IsWindowReady()) return 77;
    const fs::path dir = fs::temp_directory_path() / "raymotion-media-test";
    fs::create_directories(dir);
    ff = media::ffmpeg();
    try {
        imageFits(dir);
        imageFormats(dir);
        video(dir);
        audioMix(dir);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: exception: %s\n", e.what());
        failures++;
    }
    shutdownGraphics();
    CloseWindow();
    std::error_code ec;
    fs::remove_all(dir, ec);
    if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    std::puts("media tests passed");
    return 0;
}
