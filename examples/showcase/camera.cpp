// camera.cpp -- "camera": cam::, a 2D camera over a canvas bigger than the frame.
//   A 6000 x 3400 atlas of four stations joined by a route. The camera opens on the whole
//   map, glides in on the first station, flies (zoom out, across, back in) to the second,
//   turns as it arrives at the third, where a burst shakes it, then closes in on an
//   inscription too small to read from anywhere else and pulls back out to the whole map.
//   A starfield behind moves with parallax 0.35; the captions and the mini-map stay fixed.
// The score (96 bpm on Am-F-C-G) is synthesised into out/ on first use.
//   raymotion render camera
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace rmtn {
namespace {

const std::string SCORE = "out/camera-assets-v1/score.wav";
const Color BG = mkColor(0x101420), INK = mkColor(0xf4f1ea), MUTED = mkColor(0x8a90a2), CARD = mkColor(0x1d2333);
const Color SKY = mkColor(0x58c4dd), SUN = mkColor(0xffd166), CORAL = mkColor(0xfc6255), MINT = mkColor(0x83c167),
            ORCHID = mkColor(0xc38fd8);
const Rectangle MAP{ 0, 0, 6000, 3400 };
const Vector2 FRAME{ 1920, 1080 };

// the stations: where they sit on the canvas and the box the camera frames
const Vector2 S1{ 900, 2500 }, S2{ 2500, 900 }, S3{ 3900, 2350 }, S4{ 5150, 950 };
Rectangle around(Vector2 c, float w, float h) { return { c.x - w / 2, c.y - h / 2, w, h }; }
// a station framed with room below for the caption
Rectangle view(Vector2 c) { return around({ c.x, c.y + 90 }, 1300, 940); }
const Vector2 NOTE{ S4.x + 250, S4.y + 205 };   // the inscription

// frames
constexpr int T_WIDE = 45, T_S1 = 115, T_S1H = 165, T_S2 = 270, T_S2H = 320, T_S3 = 395, T_BOOM = 420, T_S3H = 480,
              T_S4 = 570, T_NOTE = 640, T_NOTEH = 700, T_OUT = 790, T_END = 840;

Font bold() { return fonts::ui(true); }
Font mono() {
    static bool missing = false;
    if (!missing) {
        try { return fonts::load({ "consola.ttf", "CascadiaMono.ttf", "DejaVuSansMono.ttf", "Menlo.ttc" }); }
        catch (const std::exception&) { missing = true; }
    }
    return fonts::ui(false);
}

float ramp(double t, double a, double b, Easing e = ease::inOut(ease::cubic)) {
    return (float)interpolate(t, { a, b }, { 0, 1 }, clamped(e));
}

float hash(unsigned k) {
    k = k * 2654435761u + 0x9e3779b9u;
    k ^= k >> 15; k *= 0x2c1b3c6du; k ^= k >> 12;
    return (k & 0xffffff) / 16777216.f;
}

cam::Track track() {
    cam::Track tr;
    tr.start(cam::Shot::fit(MAP, FRAME, 0.04f))
        .hold(T_WIDE)
        .to(T_S1, cam::Shot::fit(view(S1), FRAME))
        .hold(T_S1H)
        .fly(T_S2, cam::Shot::fit(view(S2), FRAME))
        .hold(T_S2H)
        .to(T_S3, cam::Shot::fit(view(S3), FRAME, 0.06f, -12))
        .hold(T_S3H)
        .fly(T_S4, cam::Shot::fit(view(S4), FRAME))
        .to(T_NOTE, cam::Shot{ NOTE, 11 })
        .hold(T_NOTEH)
        .to(T_OUT, cam::Shot::fit(MAP, FRAME, 0.04f), ease::inOut(ease::expo))
        .hold(T_END);
    return tr;
}

// the burst at the third station shakes the camera, dying away over a second and a half
cam::Shot shot(const cam::Track& tr, int t) {
    const float k = t < T_BOOM ? 0 : std::exp(-(t - T_BOOM) / 14.f);
    return k > 0.01f ? cam::shake(tr.at(t), t, 26 * k, 3.5f, 7) : tr.at(t);
}

// ------------------------------------------------------------------ the canvas

void card(const Ctx& c, Vector2 at, const char* title, Color accent) {
    vg::fill(c, vg::Path::rect(around(at, 1000, 660), 36), CARD);
    vg::stroke(c, vg::Path::rect(around(at, 1000, 660), 36), vg::Stroke(withAlpha(accent, 0.6f), 4));
    plot::label(c, title, { at.x - 450, at.y - 270 }, { 0, 0.5f }, 46, INK);
}

void canvas(const Ctx& c, int t) {
    // a survey grid with coordinates every 1000 px
    vg::Path grid;
    for (float x = 0; x <= MAP.width; x += 200) grid.moveTo({ x, 0 }).lineTo({ x, MAP.height });
    for (float y = 0; y <= MAP.height; y += 200) grid.moveTo({ 0, y }).lineTo({ MAP.width, y });
    vg::stroke(c, grid, vg::Stroke({ 255, 255, 255, 14 }, 2));
    vg::stroke(c, vg::Path::rect(MAP), vg::Stroke(withAlpha(MUTED, 0.5f), 6));
    for (int x = 1000; x < MAP.width; x += 1000)
        for (int y = 1000; y < MAP.height; y += 1000)
            plot::label(c, "(" + std::to_string(x) + ",\\," + std::to_string(y) + ")", { x + 14.f, y - 14.f }, { 0, 1 }, 28, withAlpha(MUTED, 0.6f));

    // the route, drawn in as the camera travels it
    vg::Path route = vg::Path::smooth({ S1, { 1500, 1500 }, S2, { 3300, 1300 }, S3, { 4500, 1700 }, S4 });
    vg::Stroke rs(withAlpha(SUN, 0.8f), 10);
    rs.dash = { 30, 24 };
    rs.dashOffset = -t * 2.f;
    vg::stroke(c, route.trim(0, ramp(t, 0, T_S4 - 30.0, ease::linear)), rs);

    // 1: circles
    card(c, S1, R"(\text{1. Circles})", SKY);
    for (int k = 0; k < 6; k++) {
        const float r = 40.f + 38 * k, grow = ramp(t, 60 + 8 * k, 100 + 8 * k);
        if (grow > 0) vg::stroke(c, vg::Path::arc({ S1.x - 170, S1.y + 40 }, r, -90, -90 + 360 * grow), vg::Stroke(mixC(SKY, MINT, k / 5.f), 6));
    }
    plot::label(c, R"(x^2 + y^2 = r^2)", { S1.x + 110, S1.y + 40 }, { 0, 0.5f }, 60, SKY);

    // 2: polygons
    card(c, S2, R"(\text{2. Polygons})", MINT);
    for (int k = 3; k <= 8; k++) {
        const float spin = 12.f * t / 30;
        const Vector2 at{ S2.x - 330 + 132 * (k - 3), S2.y + 10 };
        vg::fill(c, vg::Path::regular(at, 54, k, -90 + spin * (k % 2 ? 1 : -1)), withAlpha(mixC(MINT, SKY, (k - 3) / 5.f), 0.9f));
    }
    plot::label(c, R"(\text{interior angle } = \pi - \tfrac{2\pi}{n})", { S2.x, S2.y + 190 }, { 0.5f, 0.5f }, 44, MINT);

    // 3: a sunflower, and the burst
    card(c, S3, R"(\text{3. Phyllotaxis})", CORAL);
    const float golden = 137.50776f * 0.017453293f;
    const int seeds = (int)(420 * ramp(t, T_S2H, T_S3 + 10.0, ease::out(ease::cubic)));
    for (int i = 0; i < seeds; i++) {
        const float r = 11.f * std::sqrt((float)i), a = i * golden;
        vg::fill(c, vg::Path::circle({ S3.x - 150 + r * std::cos(a), S3.y + 40 + r * std::sin(a) }, 7), mixC(SUN, CORAL, i / 420.f));
    }
    plot::label(c, R"(\theta_n = n \cdot 137.5^\circ,\ r_n \propto \sqrt n)", { S3.x + 105, S3.y + 40 }, { 0, 0.5f }, 36, CORAL);
    if (t >= T_BOOM && t < T_BOOM + 45) {
        const float u = (t - T_BOOM) / 45.f;
        vg::stroke(c, vg::Path::circle({ S3.x - 150, S3.y + 40 }, 300 + 1100 * u), vg::Stroke(withAlpha(SUN, 1 - u), 12 * (1 - u) + 2));
    }

    // 4: the fine print
    card(c, S4, R"(\text{4. The fine print})", ORCHID);
    plot::label(c, R"(\text{Look closer.})", { S4.x, S4.y - 20 }, { 0.5f, 0.5f }, 72, ORCHID);
    vg::stroke(c, vg::Path::circle(NOTE, 34), vg::Stroke(withAlpha(ORCHID, 0.5f), 3));
    plot::label(c, R"(\text{you can only read this})", { NOTE.x, NOTE.y - 4 }, { 0.5f, 0.5f }, 5.5f, INK);
    plot::label(c, R"(\text{at eleven times zoom})", { NOTE.x, NOTE.y + 4 }, { 0.5f, 0.5f }, 5.5f, INK);
}

// the far layer: a starfield on a coarse lattice, so the parallax reads
void stars(const Ctx& c, int) {
    for (unsigned i = 0; i < 1400; i++) {
        const Vector2 p{ -3000 + 12000 * hash(3 * i), -2500 + 8000 * hash(3 * i + 1) };
        const float b = hash(3 * i + 2);
        vg::fill(c, vg::Path::circle(p, 2 + 5 * b * b), { 200, 210, 255, (unsigned char)(40 + 120 * b) });
    }
}

// ------------------------------------------------------------------ the HUD

void hud(const Ctx& c, int t, const cam::Track& tr) {
    const cam::Shot s = shot(tr, t);
    // mini-map, top right: the canvas at 1/20 and the part in view
    const float k = 0.05f, mx = 1920 - 60 - MAP.width * k, my = 60;
    dr::roundRect(c, mx - 12, my - 12, MAP.width * k + 24, MAP.height * k + 24, 12, mkColor(0x222737, 220));
    for (Vector2 p : { S1, S2, S3, S4 }) dr::circle(c, { mx + p.x * k, my + p.y * k }, 5, MUTED);
    std::vector<Vector2> view;
    for (Vector2 f : { Vector2{ 0, 0 }, Vector2{ 1920, 0 }, Vector2{ 1920, 1080 }, Vector2{ 0, 1080 } }) {
        const Vector2 q = cam::toCanvas(s, f, FRAME);
        view.push_back({ mx + std::clamp(q.x, -400.f, MAP.width + 400) * k, my + std::clamp(q.y, -400.f, MAP.height + 400) * k });
    }
    vg::stroke(c, vg::Path::polyline(view, true), vg::Stroke(SUN, 3));
    char buf[64];
    std::snprintf(buf, sizeof buf, "zoom %5.2fx  turn %+4.0f deg", s.zoom, s.rot);
    dr::text(mono(), c, buf, { mx - 12, my + MAP.height * k + 24 }, 24, 0, MUTED, 0, 0);

    // captions, bottom left
    struct Cap { int from, to; const char* title; const char* code; };
    static const Cap caps[] = {
        { 0, T_S1, "A canvas bigger than the frame", "cam::world(roots, 0, len, track)   Shot::fit(rect, frame)" },
        { T_S1, T_S1H + 10, "Glide in", "track.to(frame, Shot::fit(box))   // geometric zoom, steady on screen" },
        { T_S1H + 10, T_S2H, "Fly", "track.fly(frame, shot)   // van Wijk-Nuij: out, across, back in" },
        { T_S2H, T_BOOM, "Turn", "Shot::fit(box, frame, margin, -12)   // the view turns, the canvas counter-turns" },
        { T_BOOM, T_S3H + 10, "Shake", "cam::shake(shot, t, amount, freq, seed)" },
        { T_S3H + 10, T_NOTEH, "Zoom in on anything", "Shot{ point, 11 }   // vector paths and TeX stay sharp" },
        { T_NOTEH, T_END + 1, "Parallax and HUD", "cam::world(..., 0.35)   cam::visible(shot)   cam::toCanvas(shot, p)" },
    };
    for (const Cap& cp : caps) {
        const float a = ramp(t, cp.from, cp.from + 15.0) * (1 - ramp(t, cp.to - 12.0, cp.to, ease::linear));
        if (a <= 0) continue;
        Ctx cc = c;
        cc.a *= a;
        const float w = std::max(MeasureTextEx(mono(), cp.code, 26, 0).x, MeasureTextEx(bold(), cp.title, 44, 0).x) + 72;
        dr::roundRect(cc, 60, 1080 - 56 - 132, w, 132, 16, mkColor(0x222737, 235));
        dr::text(bold(), cc, cp.title, { 96, 1080 - 56 - 112 }, 44, 0, INK, 0, 0);
        dr::text(mono(), cc, cp.code, { 96, 1080 - 56 - 50 }, 26, 0, SUN, 0, 0);
    }
}

// The score, 96 bpm on Am-F-C-G: pad and arpeggio, a bell on each arrival, a riser into the
// burst and an impact on it.
void genScore(const std::string& path, double end) {
    using procgen::Score;
    Score s(end);
    struct Chord { int root; int pad[3]; int arp[4]; };
    const Chord am{ 45, { 60, 64, 69 }, { 72, 76, 81, 76 } }, f{ 41, { 60, 65, 69 }, { 72, 77, 81, 77 } },
                c{ 48, { 60, 64, 67 }, { 72, 76, 79, 76 } }, g{ 43, { 59, 62, 67 }, { 71, 74, 79, 74 } };
    const Chord prog[4] = { am, f, c, g };
    const double bar = 4 * 60.0 / 96, beat = bar / 4, stop = end - 2.4, boom = T_BOOM / 30.0;
    int i = 0;
    for (double t = 0; t < stop; t += bar, i++) {
        const Chord& ch = prog[i % 4];
        const bool drums = t >= T_S1 / 30.0, full = t >= T_S2 / 30.0;
        for (int p : ch.pad) s.pad(t, bar * 0.95, p, 0.08f, full ? 1700 : 1000);
        if (t >= bar)
            for (int e = 0; e < 8; e++) s.pluck(t + e * beat / 2, ch.arp[e % 4], e % 4 == 0 ? 0.05f : 0.03f, e % 2 ? 0.3f : 0.7f);
        for (int e = 0; e < 4; e++) s.bass(t + e * beat, beat * 0.85, ch.root, drums ? 0.22f : 0.14f);
        if (drums) { s.kick(t, 0.45f); s.kick(t + 2 * beat, 0.4f); }
        if (full) { s.clap(t + beat, 0.14f); s.clap(t + 3 * beat, 0.14f); }
        if (t >= bar * 2) for (int h = 0; h < 8; h++) s.hat(t + h * beat / 2, h % 2 ? 0.07f : 0.04f, h % 2 ? 0.35f : 0.65f);
    }
    const int arrivals[] = { T_S1, T_S2, T_S3, T_S4, T_NOTE, T_OUT };
    for (size_t k = 0; k < 6; k++) s.bell(arrivals[k] / 30.0, 76 + (int)(k % 3) * 3, 0.09f, 0.5f);
    s.riser(boom - 1.4, boom, 0.07f);
    s.impact(boom, 0.18f);
    for (int p : am.pad) { s.pad(stop, 2.4, p, 0.12f, 2000, 0.35f); s.pad(stop, 2.4, p - 12, 0.08f, 1200, 0.65f); }
    s.bass(stop, 2.4, am.root - 12, 0.22f);
    for (int k = 0; k < 4; k++) s.bell(stop + k * 0.4, am.arp[k] + 12, 0.08f, 0.2f + 0.2f * k);
    media::writeWav(path, s.mix(), Score::RATE);
}

} // namespace

void registerCamera(Registry& out) {
    Comp c;
    c.name = "camera";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    c.duration = T_END;
    const cam::Track tr = track();
    auto follow = [tr](int t) { return shot(tr, t); };
    cam::world(c.roots, 0, T_END, follow, FRAME, 0.35f)->fn = stars;
    cam::world(c.roots, 0, T_END, follow, FRAME)->fn = canvas;
    addRoot(c.roots, 0, T_END)->fn = [tr](const Ctx& cx, int t) { hud(cx, t, tr); };
    const double seconds = T_END / 30.0;
    c.assets = [seconds](const Comp&, const Json&) {
        if (std::filesystem::exists(SCORE)) return;
        std::filesystem::create_directories(std::filesystem::path(SCORE).parent_path());
        genScore(SCORE, seconds);
    };
    media::audio(addRoot(c.roots, 0, c.duration), SCORE);
    out.push_back(std::move(c));
}

} // namespace rmtn
