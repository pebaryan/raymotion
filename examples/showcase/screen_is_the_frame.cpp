// screen_is_the_frame.cpp -- "screen-is-the-frame": a 22 s demo of 2D in 3D (g3d::Surface). One
// app, drawn once with the ordinary 2D calls, is a phone screen on a desk; the camera flies in
// until the screen is the frame and cuts to drawing it straight into the frame (g3d::fillCamera,
// g3d::fillCtx), where the app's map breaks out of the phone; then the screen comes apart into
// four layer surfaces that stand apart in depth, with labels that are surfaces too.
//
//   raymotion render screen-is-the-frame
//
// The app ("Halte"), its stops and its times are fictional: see docs/screen-is-the-frame-brief.md
// and docs/screen-is-the-frame-qc.md.
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace rmtn {
namespace {

constexpr float W = 1920.f, H = 1080.f;
constexpr int FPS = 30, TOTAL = 660;                 // 22 s
const std::string ASSETS = "out/screen-is-the-frame-assets";

const Color INK = mkColor(0x14202b), BONE = mkColor(0xf1ede2), TEAL = mkColor(0x2fc4b2), AMBER = mkColor(0xf0a43c),
            DIM = mkColor(0xb4bcc4);

// ------------------------------------------------------------------ timing (brief sections 3 and 9), seconds
constexpr double PUSH_AT = 5.0, ARRIVE = 10.0, BREAK_OUT = 10.3, BUS_NOW = 12.6, FOLD = 13.4, BACK_3D = 14.5,
                 OPEN = 15.0, LABELS_AT = 15.9, LABEL_DT = 0.5, DRIFT = 17.2,
                 CLOSE = 19.2, CARD_AT = 19.5;
constexpr int ARRIVE_F = (int)(ARRIVE * FPS), BACK_F = (int)(BACK_3D * FPS);

float smooth(double u) { u = std::clamp(u, 0.0, 1.0); return (float)(u * u * (3 - 2 * u)); }
float window(double s, double from, double to) {
    const double f = 9.0 / FPS;
    return std::min(smooth((s - from) / f), smooth((to - s) / f));
}
Color ca(Color c, int a) { c.a = (unsigned char)std::clamp(a, 0, 255); return c; }   // an alpha of 0..255
Ctx faded(const Ctx& cx, float a) { Ctx m = cx; m.a *= std::clamp(a, 0.f, 1.f); return m; }
void text(const Ctx& cx, const std::string& s, Vector2 at, float size, Color col, float ax = 0, float ay = 0.5f,
          bool bold = false, float spacing = 0) {
    dr::text(fonts::ui(bold), cx, s.c_str(), at, size, spacing, col, ax, ay);
}
// `inner` (design px -> composition px) placed inside `outer` (composition px -> canvas px).
Ctx compose(const Ctx& outer, const Ctx& inner) {
    Ctx m = outer;
    m.o = { outer.o.x + outer.s * inner.o.x, outer.o.y + outer.s * inner.o.y };
    m.s = outer.s * inner.s;
    m.a = outer.a * inner.a;
    return m;
}

// ------------------------------------------------------------------ the app (fictional), in design px
// 390 x 844, the screen of a 6.1-inch phone in points. Drawn by one function in four layers, so the
// single screen and the exploded stack are the same drawing.
constexpr float AW = 390, AH = 844;
enum Layer { BACK = 1, MAP = 2, CARD = 4, TEXT = 8, ALL = 15 };
const Color APP_BG = mkColor(0xeef1f5), APP_BLUE = mkColor(0x2b6cf6), APP_INK = mkColor(0x14202b),
            APP_GREY = mkColor(0x5a6a7e), APP_LINE = mkColor(0xe1e6ee), MAP_BG = mkColor(0xe6ebe0),
            PARK = mkColor(0xcfe2bf), WATER = mkColor(0xb8d5ee), BUS = mkColor(0xe8912a);

struct R { float x, y, w, h; };
const R MAP_CARD{ 16, 186, 358, 334 };
const R PANEL{ 16, 544, 358, 272 };

// Liang-Barsky: clip segment a-b to r; false if nothing is left.
bool clip(Vector2& a, Vector2& b, R r) {
    const float dx = b.x - a.x, dy = b.y - a.y;
    float t0 = 0, t1 = 1;
    const float p[4] = { -dx, dx, -dy, dy }, q[4] = { a.x - r.x, r.x + r.w - a.x, a.y - r.y, r.y + r.h - a.y };
    for (int i = 0; i < 4; i++) {
        if (std::fabs(p[i]) < 1e-9f) { if (q[i] < 0) return false; continue; }
        const float u = q[i] / p[i];
        if (p[i] < 0) t0 = std::max(t0, u); else t1 = std::min(t1, u);
        if (t0 > t1) return false;
    }
    const Vector2 a0 = a;
    a = { a0.x + t0 * dx, a0.y + t0 * dy };
    b = { a0.x + t1 * dx, a0.y + t1 * dy };
    return true;
}
bool inside(Vector2 p, R r, float m = 0) { return p.x >= r.x + m && p.x <= r.x + r.w - m && p.y >= r.y + m && p.y <= r.y + r.h - m; }
void seg(const Ctx& cx, Vector2 a, Vector2 b, float w, Color col, R r) { if (clip(a, b, r)) dr::strokeSeg(cx, a, b, w, col); }
void block(const Ctx& cx, R b, R r, Color col) {
    const float x0 = std::max(b.x, r.x), y0 = std::max(b.y, r.y), x1 = std::min(b.x + b.w, r.x + r.w), y1 = std::min(b.y + b.h, r.y + r.h);
    if (x1 > x0 && y1 > y0) dr::rect(cx, x0, y0, x1 - x0, y1 - y0, col);
}

// The map lives in the app's own design px: the card on the phone is a window onto it, and
// when the card breaks out of the phone the window grows and shows more of the same map.
constexpr float STREET_X0 = 110, STREET_DX = 90, STREET_Y0 = 300, STREET_DY = 80;
const std::array<Vector2, 8> ROUTE{ { { -640, 540 }, { -340, 540 }, { -340, 380 }, { 110, 380 }, { 110, 300 }, { 560, 300 }, { 560, 460 }, { 1060, 460 } } };
struct Stop { Vector2 at; const char* name; };
const std::array<Stop, 6> STOPS{ { { { -520, 540 }, "Anggrek" }, { { -340, 460 }, "Kenanga" }, { { -110, 380 }, "Cempaka" },
                                   { { 300, 300 }, "Melati" }, { { 560, 400 }, "Kamboja" }, { { 830, 460 }, "Dahlia" } } };
constexpr int HERE = 3;                                      // the stop this screen is for

float routeLength(Vector2 p) {                               // distance along the route to p (p on it)
    float d = 0;
    for (size_t i = 0; i + 1 < ROUTE.size(); i++) {
        const Vector2 a = ROUTE[i], b = ROUTE[i + 1];
        const float l = Vector2Distance(a, b);
        const float u = Vector2DotProduct(Vector2Subtract(p, a), Vector2Subtract(b, a)) / (l * l);
        if (u >= -1e-4f && u <= 1 + 1e-4f && Vector2Distance(Vector2Lerp(a, b, u), p) < 0.5f) return d + u * l;
        d += l;
    }
    return d;
}
Vector2 routeAt(float d) {
    for (size_t i = 0; i + 1 < ROUTE.size(); i++) {
        const float l = Vector2Distance(ROUTE[i], ROUTE[i + 1]);
        if (d <= l) return Vector2Lerp(ROUTE[i], ROUTE[i + 1], d / l);
        d -= l;
    }
    return ROUTE.back();
}
// The bus: from (50, 380) to the stop, braking into it, arriving at BUS_NOW.
Vector2 busAt(double s) {
    static const float from = routeLength({ 50, 380 }), to = routeLength(STOPS[HERE].at);
    const double u = std::clamp(s / BUS_NOW, 0.0, 1.0);
    return routeAt(from + (to - from) * (float)(1 - std::pow(1 - u, 1.6)));
}

// The map card at rectangle `r` (design px; the app's own card, or bigger when broken out).
void drawMap(const Ctx& cx, R r, float rad, double s) {
    dr::roundRect(cx, r.x, r.y, r.w, r.h, rad, MAP_BG);
    const R in{ r.x + 4, r.y + 4, r.w - 8, r.h - 8 };      // keeps lines inside the rounded corners
    block(cx, { -700, 630, 1900, 60 }, in, WATER);           // a river, below the phone's window
    for (int i = -10; i <= 11; i++)
        for (int j = -4; j <= 7; j++) {
            if (rnd01(17, i * 31 + j) > 0.16) continue;
            block(cx, { STREET_X0 + i * STREET_DX + 7, STREET_Y0 + j * STREET_DY + 7, STREET_DX - 14, STREET_DY - 14 }, in, PARK);
        }
    for (int i = -10; i <= 11; i++) {
        const float x = STREET_X0 + i * STREET_DX;
        if (std::fabs(x - in.x) < 8 || std::fabs(x - in.x - in.w) < 8) continue;   // no sliver on the card's edge
        seg(cx, { x, -200 }, { x, 1100 }, i % 3 == 0 ? 12.f : 7.f, WHITE, in);
    }
    for (int j = -5; j <= 8; j++) {
        const float y = STREET_Y0 + j * STREET_DY;
        seg(cx, { -800, y }, { 1200, y }, j % 3 == 0 ? 12.f : 7.f, WHITE, in);
    }
    for (int pass = 0; pass < 2; pass++)                      // the route: a white casing, then blue
        for (size_t i = 0; i + 1 < ROUTE.size(); i++) {
            const float w = pass ? 7.f : 13.f;
            const Color col = pass ? APP_BLUE : WHITE;
            seg(cx, ROUTE[i], ROUTE[i + 1], w, col, in);
            if (i > 0 && inside(ROUTE[i], in, w / 2)) dr::circle(cx, ROUTE[i], w / 2, col);
        }
    for (int k = 0; k < (int)STOPS.size(); k++) {
        const Stop& st = STOPS[k];
        if (!inside(st.at, in, 10)) continue;
        const bool here = k == HERE;
        dr::circle(cx, st.at, here ? 11.f : 8.f, here ? APP_BLUE : WHITE);
        dr::ring(cx, st.at, here ? 11.f : 8.f, 3, here ? WHITE : APP_BLUE);
        const Vector2 lp{ st.at.x + 14, st.at.y - 18 };
        if (inside(lp, in, 4)) {
            dr::text(fonts::ui(true), cx, st.name, lp, here ? 17.f : 14.f, 0, ca(WHITE, 230), 0, 0.5f);   // a halo so names read over streets
            dr::text(fonts::ui(true), cx, st.name, lp, here ? 17.f : 14.f, 0, here ? APP_INK : APP_GREY, 0, 0.5f);
        }
    }
    const Vector2 bus = busAt(s);
    if (inside(bus, in, 12)) {
        if (s > BUS_NOW) {                                    // arrived: a ring goes out once
            const float u = (float)std::clamp((s - BUS_NOW) / 1.0, 0.0, 1.0);
            dr::ring(cx, bus, 13 + 30 * u, 3, ca(BUS, (int)(200 * (1 - u))));
        }
        dr::circle(cx, bus, 13, WHITE);
        dr::circle(cx, bus, 10, BUS);
        dr::text(fonts::ui(true), cx, "1", { bus.x, bus.y + 0.5f }, 13, 0, WHITE, 0.5f, 0.5f);
    }
}

std::string countdown(double s) {
    if (s >= BUS_NOW) return "Now";
    char b[16];
    std::snprintf(b, sizeof b, "0:%02d", (int)std::ceil(BUS_NOW - s - 1e-6));
    return b;
}

// The whole app, or some of its layers. `grow` (0..1) breaks the map card out to `frame`
// (design px), drawn over everything else.
void drawApp(const Ctx& cx, int t, int layers, float grow = 0, R frame = { 0, 0, AW, AH }) {
    const double s = t / (double)FPS;
    if (layers & BACK) {
        dr::rect(cx, 0, 0, AW, AH, APP_BG);
        dr::rectGrad4(cx, 0, 0, AW, 250, mkColor(0x2f74ff), mkColor(0x2f74ff), APP_BLUE, APP_BLUE);
    }
    if (layers & CARD) {
        for (int k = 3; k >= 1; k--) dr::roundRect(cx, PANEL.x - k, PANEL.y + 2 * k, PANEL.w + 2 * k, PANEL.h, 20 + k, { 20, 32, 43, 10 });
        dr::roundRect(cx, PANEL.x, PANEL.y, PANEL.w, PANEL.h, 20, WHITE);
        dr::roundRect(cx, 40, 716, 310, 8, 4, APP_LINE);
        const float done = (float)std::clamp(0.12 + 0.88 * s / BUS_NOW, 0.0, 1.0);
        dr::roundRect(cx, 40, 716, 310 * done, 8, 4, s >= BUS_NOW ? BUS : APP_BLUE);
        dr::strokeSeg(cx, { 40, 744 }, { 350, 744 }, 1.5f, APP_LINE);
    }
    if (layers & TEXT) {
        text(cx, "9:41", { 30, 24 }, 16, WHITE, 0, 0.5f, true);
        dr::roundRect(cx, 330, 17, 26, 13, 4, ca(WHITE, 120));                 // the battery
        dr::roundRect(cx, 332, 19, 18, 9, 2, WHITE);
        for (int k = 0; k < 4; k++) dr::rect(cx, 296 + k * 6, 27 - 3 * k, 4, 3 + 3 * k, WHITE);   // the signal
        text(cx, "HALTE", { 24, 72 }, 13, ca(WHITE, 200), 0, 0.5f, true, 3);
        text(cx, "Melati", { 22, 112 }, 40, WHITE, 0, 0.5f, true);
        text(cx, "Platform 2  \xC2\xB7  towards Kamboja", { 24, 150 }, 16, ca(WHITE, 225), 0, 0.5f);
        text(cx, "BUS 1  \xC2\xB7  TOWARDS KAMBOJA", { 40, 574 }, 13, APP_GREY, 0, 0.5f, true, 1.5f);
        text(cx, s >= BUS_NOW ? "at the stop" : "arrives in", { 40, 606 }, 18, APP_GREY, 0, 0.5f);
        text(cx, countdown(s), { 38, 662 }, 66, s >= BUS_NOW ? BUS : APP_INK, 0, 0.5f, true);
        text(cx, "Bus 6", { 40, 772 }, 16, APP_INK, 0, 0.5f, true);
        text(cx, "4 min", { 350, 772 }, 16, APP_GREY, 1, 0.5f);
        text(cx, "Bus 1", { 40, 798 }, 16, APP_INK, 0, 0.5f, true);
        text(cx, "9 min", { 350, 798 }, 16, APP_GREY, 1, 0.5f);
        dr::roundRect(cx, 130, 828, 130, 5, 2.5f, ca(APP_INK, 150));           // the home indicator
    }
    if (layers & MAP) {
        const float g = smooth(grow);
        const R r{ MAP_CARD.x + (frame.x - MAP_CARD.x) * g, MAP_CARD.y + (frame.y - MAP_CARD.y) * g,
                   MAP_CARD.w + (frame.w - MAP_CARD.w) * g, MAP_CARD.h + (frame.h - MAP_CARD.h) * g };
        drawMap(cx, r, 18 * (1 - g), s);
    }
}

// ------------------------------------------------------------------ the desk and the phone (metres)
constexpr float PHONE_W = 0.0755f, PHONE_H = 0.1587f, PHONE_D = 0.0078f;   // the body
constexpr float SCREEN_W = 0.0715f, SCREEN_H = 0.1547f;                     // 390 x 844 at 0.183 mm a px
constexpr float TILT = 20;                                                  // leaning back on a stand, degrees
const Vector3 PHONE_AT{ 0, 0.012f + 0.5f * PHONE_H * 0.94f, 0 };

Vector3 screenCentre() { return Vector3Add(PHONE_AT, Vector3Scale({ 0, std::sin(TILT * DEG2RAD), std::cos(TILT * DEG2RAD) }, 0.0041f)); }
Vector3 screenNormal() { return { 0, std::sin(TILT * DEG2RAD), std::cos(TILT * DEG2RAD) }; }

// Opening of the layer stack: 0 = one screen, 1 = apart.
float opened(double s) { return smooth((s - OPEN) / 1.6) * (1 - smooth((s - CLOSE) / 1.1)); }
float gap(double s) { return 0.0006f + 0.028f * opened(s); }

g3d::Camera around(Vector3 at, float r, float az, float el) {       // degrees; az 0 = in front, + to the right
    g3d::Camera c;
    c.fov = 40;
    c.nearPlane = 0.02f;
    c.farPlane = 20;
    c.target = at;
    c.position = Vector3Add(at, Vector3Scale({ std::sin(az * DEG2RAD) * std::cos(el * DEG2RAD), std::sin(el * DEG2RAD),
                                               std::cos(az * DEG2RAD) * std::cos(el * DEG2RAD) }, r));
    return c;
}
g3d::Camera arc(double s) {                                          // chapter 1: a slow arc from the right
    const float u = (float)std::clamp(s / ARRIVE, 0.0, 1.0);
    return around(screenCentre(), 0.44f - 0.05f * u, 30 - 16 * u, 32 - 8 * u);
}
// Chapter 4: out to the left, where the stack reads as depth, then a slow drift to the right
// that slides the front layers over the labels of the ones behind.
g3d::Camera orbit(double s) {
    const float u = smooth((s - DRIFT) / 2.6);
    return around(Vector3Add(screenCentre(), Vector3Scale(screenNormal(), 0.04f)), 0.42f, -48 + 70 * u, 16);
}
g3d::Camera camera(double s, const g3d::Camera& fill) {
    if (s < PUSH_AT) return arc(s);
    if (s < ARRIVE) return g3d::mix(arc(s), fill, smooth((s - PUSH_AT) / (ARRIVE - PUSH_AT)));
    if (s < BACK_3D) return fill;
    return g3d::mix(fill, orbit(s), smooth((s - BACK_3D) / 2.4));
}

struct Stage {
    std::shared_ptr<g3d::Scene> scene;
    std::shared_ptr<g3d::Surface> ui;                         // the one screen
    std::array<std::shared_ptr<g3d::Surface>, 4> layers;      // the same drawing, a layer each
};

g3d::Object3D* box(g3d::Object3D& parent, Vector3 size, Vector3 at, Color col, float rough = 0.8f, float metal = 0) {
    g3d::Material m;
    m.color = col;
    m.roughness = rough;
    m.metalness = metal;
    g3d::Object3D* o = parent.add(g3d::Geometry::box(size.x, size.y, size.z), m);
    o->position = at;
    return o;
}

// A label that is itself a surface: a dot on the layer's edge, a leader and a chip, always
// square to the camera at one screen px per design px, and hidden by whatever is in front.
std::shared_ptr<g3d::Surface> label(const char* name, int k) {
    auto l = std::make_shared<g3d::Surface>();
    l->pixels = { 230, 44 };
    l->size = { 0.23f, 0.044f };
    l->anchor = { 1, 0.5f };                                 // the dot end sits on the layer
    l->billboard = true;
    l->pixelSize = 1;
    const std::string nm = name;
    const double at = LABELS_AT + LABEL_DT * k;
    l->draw = [nm, at](const Ctx& cx, int t) {
        const double s = t / (double)FPS;
        const float a = smooth((s - at) / (10.0 / FPS)) * (1 - smooth((s - CARD_AT + 0.1) / (9.0 / FPS)));
        if (a < 0.001f) return;
        const Ctx m = faded(cx, a);
        const float slide = (1 - a) * 16;
        dr::roundRect(m, 0 + slide, 4, 170, 36, 8, ca(INK, 225));
        dr::text(code::font(true), m, nm.c_str(), { 16 + slide, 22 }, 22, 0, TEAL, 0, 0.5f);
        dr::strokeSeg(m, { 170 + slide, 22 }, { 224, 22 }, 2, TEAL);
        dr::circle(m, { 224, 22 }, 5, TEAL);
    };
    return l;
}

std::shared_ptr<Stage> makeStage() {
    auto st = std::make_shared<Stage>();
    auto scene = st->scene = std::make_shared<g3d::Scene>();
    scene->ambient = 0.5f;
    scene->skyColor = mkColor(0x8fa3c0);
    scene->groundColor = mkColor(0x3a2a20);
    scene->shadowMapSize = 2048;
    scene->fogColor = mkColor(0x161e28);                    // the far desk fades into the backdrop
    scene->fogNear = 0.6f;
    scene->fogFar = 1.7f;
    g3d::Light key = g3d::Light::directional({ -0.5f, 1.1f, 0.7f }, { 255, 226, 190, 255 }, 2.4f, true);
    key.shadowExtent = 0.45f;
    key.shadowBias = 0.002f;
    scene->lights.push_back(key);
    scene->lights.push_back(g3d::Light::directional({ 0.8f, 0.4f, -0.6f }, mkColor(0x7fa8d8), 0.6f, false));   // a cool rim

    // ---- the desk and what is on it
    g3d::Object3D& root = *scene->add();
    box(root, { 4.0f, 0.04f, 3.0f }, { 0, -0.02f, 0 }, mkColor(0x3e2c22), 0.7f);
    g3d::Material mug;
    mug.color = mkColor(0xd9d2c3);
    mug.roughness = 0.4f;
    root.add(g3d::Geometry::cylinder(0.04f, 0.09f), mug)->position = { -0.2f, 0.045f, -0.09f };
    g3d::Material coffee;
    coffee.color = mkColor(0x2a1810);
    coffee.roughness = 0.2f;
    root.add(g3d::Geometry::cylinder(0.035f, 0.002f), coffee)->position = { -0.2f, 0.0835f, -0.09f };
    g3d::Object3D* book = box(root, { 0.15f, 0.012f, 0.21f }, { 0.17f, 0.006f, 0.0f }, mkColor(0x2d4a5e), 0.85f);
    book->rotation = { 0, -14, 0 };
    box(root, { 0.14f, 0.002f, 0.2f }, { 0.172f, 0.0128f, 0.002f }, mkColor(0xbdb6a6), 0.95f)->rotation = { 0, -14, 0 };
    g3d::Material pen;
    pen.color = mkColor(0xc7472e);
    pen.roughness = 0.35f;
    g3d::Object3D* p = root.add(g3d::Geometry::cylinder(0.0045f, 0.14f), pen);
    p->position = { 0.08f, 0.0175f, 0.11f };
    p->rotation = { 90, 0, 60 };
    box(root, { 0.05f, 0.07f, 0.03f }, { 0, 0.035f, -0.045f }, mkColor(0x2b2f36), 0.6f)->rotation = { -TILT, 0, 0 };   // the stand

    // ---- the phone, its screen and the layers
    g3d::Object3D* phone = box(root, { PHONE_W, PHONE_H, PHONE_D }, PHONE_AT, mkColor(0x1b1e23), 0.35f, 0.4f);
    phone->rotation = { -TILT, 0, 0 };

    auto ui = st->ui = std::make_shared<g3d::Surface>();
    ui->pixels = { AW, AH };
    ui->size = { SCREEN_W, SCREEN_H };
    ui->background = APP_BG;
    ui->sharpness = 1.5f;                                     // small text, seen at a slant in chapter 1
    ui->draw = [](const Ctx& cx, int t) { drawApp(cx, t, ALL); };
    g3d::Object3D* screen = phone->add(ui);
    screen->position = { 0, 0, 0.0041f };
    screen->up = [](int t, g3d::Object3D& o) { o.visible = t < (int)(OPEN * FPS); };

    const char* names[4] = { "backdrop", "map", "card", "text" };
    const int bits[4] = { BACK, MAP, CARD, TEXT };
    const float labelY[4] = { 430, 250, 700, 112 };           // design px, where each label meets its layer: behind what is in front
    for (int i = 0; i < 4; i++) {
        auto s = st->layers[i] = std::make_shared<g3d::Surface>();
        s->pixels = { AW, AH };
        s->size = { SCREEN_W, SCREEN_H };
        s->background = i == 0 ? APP_BG : BLANK;
        s->sharpness = 1.5f;
        const int bit = bits[i];
        s->draw = [bit](const Ctx& cx, int t) { drawApp(cx, t, bit); };
        g3d::Object3D* o = phone->add(s);
        o->up = [i](int t, g3d::Object3D& self) {
            self.visible = t >= (int)(OPEN * FPS);
            self.position = { 0, 0, 0.0041f + i * gap(t / (double)FPS) };
        };
        g3d::Object3D* tag = o->add(label(names[i], i));
        tag->position = { -SCREEN_W / 2 - 0.002f, (0.5f - labelY[i] / AH) * SCREEN_H, 0.0005f };
    }

    // ---- the camera: fill is where the screen exactly fills the frame
    const g3d::Camera fill = g3d::fillCamera(arc(ARRIVE), *scene, *screen, ARRIVE_F, W / H);
    scene->camera = arc(0);
    scene->camera.up = [fill](int t, g3d::Camera& c) { const auto keep = c.up; c = camera(t / (double)FPS, fill); c.up = keep; };
    return st;
}

// ------------------------------------------------------------------ overlays and card
void chip(const Ctx& cx, const char* s, Vector2 at, float size, float a) {      // a kicker on a dark chip
    if (a < 0.001f) return;
    const Font f = fonts::ui(true);
    const float w = MeasureTextEx(f, s, size, 2).x;
    Ctx m = faded(cx, a);
    m.o.y += (1 - a) * 14 * m.s;
    dr::roundRect(m, at.x - 22, at.y - size * 0.5f - 16, w + 44, size + 32, 12, ca(INK, 205));
    text(m, s, at, size, BONE, 0, 0.5f, true, 2);
}

void hud(const Ctx& cx, int t, const Stage& st) {
    const double s = t / (double)FPS;
    const float a = window(s, 2.2, BACK_3D + 1.1);
    if (a < 0.001f) return;
    const Ctx m = faded(cx, a);
    const bool twoD = t >= ARRIVE_F && t < BACK_F;
    // the tag flips (squashes to nothing and back) 0.15 s either side of each cut
    float flip = 1;
    for (double cut : { ARRIVE, BACK_3D }) flip = std::min(flip, (float)std::clamp(std::fabs(s - cut) / 0.15, 0.0, 1.0));
    const float y = H - 80;
    dr::roundRect(m, 74, y - 30, 590, 60, 12, ca(INK, 205));
    const float hh = 40 * flip;
    dr::roundRect(m, 88, y - hh / 2, 64, hh, 8, twoD ? BONE : TEAL);
    if (flip > 0.3f) {
        Ctx k = m;
        k.a *= (flip - 0.3f) / 0.7f;
        dr::text(code::font(true), k, twoD ? "2D" : "3D", { 120, y + 1 }, 26, 0, INK, 0.5f, 0.5f);
    }
    char line[128];
    if (twoD)
        std::snprintf(line, sizeof line, "fillCtx  %d\xC3\x97%d  \xE2\x86\x92  frame %.2f\xC3\x97", (int)AW, (int)AH, H / AH);
    else
        std::snprintf(line, sizeof line, "g3d::Surface  %d\xC3\x97%d  \xE2\x86\x92  texture %.2f\xC3\x97", (int)AW, (int)AH, st.ui->resolution);
    dr::text(code::font(false), m, line, { 172, y + 1 }, 26, 0, twoD ? BONE : TEAL, 0, 0.5f);
}

void overlays(const Ctx& cx, int t, const Stage& st) {
    const double s = t / (double)FPS;
    chip(cx, "A 2D app, on a desk", { 118, 124 }, 44, window(s, 0.5, 4.4));
    chip(cx, "Fly in", { 118, 124 }, 44, window(s, 5.0, 8.8));
    chip(cx, "Now it is the frame", { 118, 124 }, 44, window(s, 10.2, 13.5));
    chip(cx, "One drawing, in layers", { 118, 124 }, 44, window(s, 14.8, 18.6));
    if (const float k = window(s, 1.0, 9.6); k > 0.001f)
        text(faded(cx, k), "FICTIONAL APP", { W - 96, 96 }, 20, ca(BONE, 200), 1, 0.5f, true, 5);
    hud(cx, t, st);
}

void endCard(const Ctx& cx, int t) {
    const float kt = smooth(t / 14.0), ka = smooth((t - 18) / 14.0);
    Ctx tm = faded(cx, kt);
    tm.o.y += (1 - kt) * 20 * tm.s;
    dr::rectGrad4(faded(cx, kt), 0, 700, W, 140, ca(INK, 0), ca(INK, 0), INK, INK);
    dr::rect(faded(cx, kt), 0, 840, W, 240, INK);
    text(tm, "THE SCREEN IS THE FRAME", { W / 2, 878 }, 84, BONE, 0.5f, 0.5f, true, 3);
    dr::text(code::font(false), faded(cx, ka), "g3d::Surface  \xC2\xB7  g3d::fillCamera  \xC2\xB7  g3d::fillCtx", { W / 2, 954 }, 28, 0, TEAL, 0.5f, 0.5f);
}

// ------------------------------------------------------------------ sound (brief section 7)
using procgen::Score;
constexpr int RATE = Score::RATE;

void genScore(const std::string& path) {
    const double end = TOTAL / (double)FPS;
    Score s(end + 1);
    auto sweep = [&](double a, double b, float amp, float lo, float hi) {      // filtered noise, lo -> hi
        const int i0 = (int)(a * RATE), len = (int)((b - a) * RATE);
        procgen::Svf f;
        for (int k = 0; k < len; k++) {
            const double u = (double)k / len;
            const float env = (float)std::sin(PI * u);
            s.put(i0 + k, f.bp(s.noise(), lo + (hi - lo) * (float)u, 0.5f, RATE) * env * amp, 0.5f, 0.35f, 0);
        }
    };
    for (int n : { 50, 57, 62, 65 }) s.pad(0, ARRIVE + 0.3, n, 0.05f, 900, 0.5f);           // the desk: a soft chord
    s.bass(0, ARRIVE, 38, 0.06f);
    for (double k = std::ceil(BUS_NOW) - 1; k >= 1; k--) s.tick(BUS_NOW - k, 0.06f);        // the countdown's seconds
    s.riser(PUSH_AT + 1.5, ARRIVE, 0.09f);                                                   // the push
    s.hat(ARRIVE, 0.35f, 0.5f);                                                              // the cut
    s.tick(ARRIVE, 0.2f);
    for (int n : { 62, 69, 74, 78 }) s.pad(ARRIVE, BACK_3D - ARRIVE + 0.3, n, 0.05f, 1700, 0.5f);   // the frame: brighter
    sweep(BREAK_OUT, BREAK_OUT + 1.1, 0.06f, 400, 2400);                                     // the card opens
    s.bell(BUS_NOW, 76, 0.07f, 0.45f);                                                       // the bus: a two-note chime
    s.bell(BUS_NOW + 0.16, 83, 0.06f, 0.55f);
    sweep(FOLD, FOLD + 1.1, 0.05f, 2400, 400);                                               // and folds
    s.hat(BACK_3D, 0.3f, 0.5f);
    s.tick(BACK_3D, 0.16f);
    s.bass(OPEN, CARD_AT - OPEN, 33, 0.08f);                                                 // the orbit: a low pulse
    for (double t = OPEN; t < CARD_AT - 0.2; t += 0.5) s.pluck(t, 45, 0.04f, 0.5f, 6);
    for (int n : { 57, 64, 69, 72 }) s.pad(OPEN, CARD_AT - OPEN + 0.3, n, 0.045f, 1200, 0.5f);
    for (int k = 0; k < 4; k++) s.pluck(LABELS_AT + LABEL_DT * k, 72 + 3 * k, 0.07f, 0.35f + 0.1f * k, 9);   // the labels
    s.impact(CARD_AT + 0.1, 0.2f);
    for (int j = 0; j < 4; j++) s.bell(CARD_AT + 0.15 + 0.12 * j, 69 + j * 3, 0.06f, 0.3f + 0.12f * j);
    for (int n : { 50, 57, 62, 69 }) s.pad(CARD_AT, end - CARD_AT, n, 0.06f, 1300, 0.5f);

    std::vector<float> mix = s.mix();
    for (float& x : mix) x = std::tanh(x * 3.0f);
    float peak = 0;
    for (float x : mix) peak = std::max(peak, std::fabs(x));
    const float g = peak > 0 ? 0.43f / peak : 1.f;
    for (float& x : mix) x *= g;
    media::writeWav(path, mix, RATE);
}

void ensureAssets() {
    std::filesystem::create_directories(ASSETS);
    const std::string score = ASSETS + "/score.wav";
    if (!std::filesystem::exists(score)) genScore(score);
}

void build(Comp& c, const Json&) {
    c.roots.clear();
    const auto st = makeStage();
    addRoot(c.roots, 0, TOTAL)->fn = [st](const Ctx& cx, int t) {
        dr::rectGrad4(cx, 0, 0, W, H, mkColor(0x1d2733), mkColor(0x1d2733), mkColor(0x0d1218), mkColor(0x0d1218));
        g3d::render(*st->scene, t, cx);
        if (t >= ARRIVE_F && t < BACK_F) {                    // the cut: the same app, drawn straight into the frame
            const Ctx d = compose(cx, g3d::fillCtx(*st->ui, W, H));
            const double s = t / (double)FPS;
            const float grow = (float)std::clamp((s - BREAK_OUT) / 1.0, 0.0, 1.0) * (1 - (float)std::clamp((s - FOLD) / 1.0, 0.0, 1.0));
            const R frame{ -d.o.x / d.s, -d.o.y / d.s, cx.s * W / d.s, cx.s * H / d.s };   // the whole frame, in design px
            drawApp(d, t - st->ui->start, ALL, grow, frame);
        }
        overlays(cx, t, *st);
    };
    const int e = (int)(CARD_AT * FPS);
    addRoot(c.roots, e, TOTAL - e)->fn = [](const Ctx& cx, int t) { endCard(cx, t); };
    TNode* logo = prefab::use(c.roots, e + 28, TOTAL - e - 28, "raymotion-logo",
                              Json::parse(R"({"size": 56, "layout": "horizontal", "tagline": "video as code", "delay": 2, "ink": "#f1ede2", "accent": "#2fc4b2"})"));
    logo->props.pos = { W / 2 - 150, 1020 };
    addRoot(c.roots, e + 36, TOTAL - e - 36)->fn = [](const Ctx& cx, int t) {
        dr::text(code::font(false), faded(cx, smooth(t / 12.0)), "github.com/pebaryan/raymotion", { W / 2 - 30, 1020 }, 26, 0, DIM, 0, 0.5f);
    };
    media::audio(addRoot(c.roots, 0, TOTAL), ASSETS + "/score.wav");
}

} // namespace

void registerScreenIsTheFrame(Registry& out) {
    Comp c;
    c.name = "screen-is-the-frame";
    c.w = (int)W; c.h = (int)H; c.fps = FPS;
    c.duration = TOTAL;
    c.background = INK;
    c.assets = [](const Comp&, const Json&) { ensureAssets(); };
    c.build = [](Comp& c, const Json& p) { build(c, p); };
    out.push_back(std::move(c));
}

} // namespace rmtn
