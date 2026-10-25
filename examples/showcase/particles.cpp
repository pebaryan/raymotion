// particles.cpp -- "particles": the fx:: emitters, one scene each.
//   fireworks   rockets (a moving origin) that end in bursts
//   campfire    fire, smoke and rising sparks
//   rain, snow  prewarmed weather in two depths, with splashes
//   comet       a glowing trail behind a source that follows a path
//   custom look e.draw draws each particle (binary digits here)
//   finale      a confetti cannon and falling confetti
//   raymotion render particles
#include "rmtn/raymotion.hpp"

namespace rmtn {
namespace {

constexpr int XF = 20;
const Color WHITE_ = mkColor(0xffffff), SUB = mkColor(0xb8bdd0);
const Color AMBER = mkColor(0xffc365), ROSE = mkColor(0xff6b8a), TEAL = mkColor(0x47dad8), IRIS = mkColor(0x8c78ff),
            LEAF = mkColor(0x9be15d), FLAME = mkColor(0xff8a3d);

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

void sky(TNode* n, int len, Color top, Color bottom) {
    n->add(0, len)->fn = [top, bottom](const Ctx& cx, int) { dr::rectGrad4(cx, 0, 0, 1920, 1080, top, top, bottom, bottom); };
}

// An emitter drawn by its own node, placed at `at`.
TNode* emit(TNode* parent, int from, int len, Vector2 at, fx::Emitter e) {
    TNode* n = parent->add(from, len);
    n->props.pos = at;
    n->fn = fx::fn(std::move(e));
    return n;
}

// Caption card: what is on screen and the code that makes it.
void caption(TNode* n, int len, const char* title, const char* code) {
    TNode* card = n->add(12, len - 12);
    card->up = [len](int t, NodeProps& p) {
        const float in = (float)interpolate(t, { 0, 18 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        const float out = (float)interpolate(t, { len - 40.0, len - 22.0 }, { 1, 0 }, clamped());
        p.opacity = in * out;
        p.pos = { 0, 20 * (1 - in) };
    };
    const std::string t = title, c = code;
    card->fn = [t, c](const Ctx& cx, int) {
        const float w = std::max(MeasureTextEx(mono(), c.c_str(), 26, 0).x, MeasureTextEx(bold(), t.c_str(), 44, 0).x) + 72;
        dr::roundRect(cx, 60, 1080 - 56 - 132, w, 132, 16, Color{ 8, 10, 20, 200 });
        dr::text(bold(), cx, t.c_str(), { 96, 1080 - 56 - 112 }, 44, 0, WHITE_, 0, 0);
        dr::text(mono(), cx, c.c_str(), { 96, 1080 - 56 - 50 }, 26, 0, AMBER, 0, 0);
    };
}

// A rocket climbs from `ground` to `apex` over `rise` frames, leaving a trail, then bursts.
void firework(TNode* n, int at, int len, Vector2 ground, Vector2 apex, int rise, std::vector<Color> colors,
              unsigned long long seed) {
    fx::Emitter trail;
    trail.seed = seed;
    trail.rate = 2.5f;
    trail.until = rise;
    trail.origin = [ground, apex, rise](float f) { return mixV(ground, apex, ease::out(ease::quad)(std::min(1.f, f / rise))); };
    trail.speed = { 0.2f, 1.2f };
    trail.gravity = { 0, 0.05f };
    trail.life = { 14, 26 };
    trail.look = fx::Look::Glow;
    trail.size = { 8, 14 };
    trail.sizeEnd = 0.2f;
    trail.colors = { mkColor(0xffe6b0) };
    trail.additive = true;
    emit(n, at, len - at, { 0, 0 }, trail);

    fx::Emitter b = fx::burst(150, colors);
    b.seed = seed + 1;
    b.burstAt = rise;
    b.origin = [apex](float) { return apex; };
    emit(n, at, len - at, { 0, 0 }, b);

    // A flash at the moment of the burst.
    fx::Emitter flash;
    flash.seed = seed + 2;
    flash.burst = 1;
    flash.burstAt = rise;
    flash.origin = [apex](float) { return apex; };
    flash.speed = 0;
    flash.life = 14;
    flash.look = fx::Look::Glow;
    flash.size = 260;
    flash.sizeEnd = 1.6f;
    flash.colors = { withAlpha(colors[0], 0.5f) };
    flash.fadeIn = 0;
    flash.fadeOut = 1;
    flash.additive = true;
    emit(n, at, len - at, { 0, 0 }, flash);
}

void fireworksScene(TNode* n, int len) {
    sky(n, len, mkColor(0x060818), mkColor(0x1d1440));
    emit(n, 0, len, { 960, 540 }, [] { fx::Emitter e = fx::embers(1920, 1080); e.rate = 0.25f; e.size = { 3, 7 }; return e; }());
    firework(n, 4, len, { 560, 1100 }, { 520, 330 }, 34, { AMBER, FLAME }, 11);
    firework(n, 22, len, { 1400, 1100 }, { 1440, 280 }, 36, { ROSE, IRIS }, 21);
    firework(n, 46, len, { 960, 1100 }, { 980, 200 }, 40, { TEAL, WHITE_ }, 31);
    firework(n, 74, len, { 300, 1100 }, { 260, 460 }, 30, { LEAF, AMBER }, 41);
    firework(n, 92, len, { 1650, 1100 }, { 1640, 520 }, 30, { IRIS, TEAL }, 51);
    TNode* t = n->add(40, len - 40);
    t->up = [](int f, NodeProps& p) {
        p.opacity = (float)interpolate(f, { 0, 24 }, { 0, 1 }, clamped());
        p.pos = { 0, 24 * (1 - p.opacity) };
    };
    t->fn = [](const Ctx& cx, int) {
        dr::text(bold(), cx, "Particles", { 960, 640 }, 150, 0, WHITE_, 0.5, 0.5);
        dr::text(regular(), cx, "every particle is a pure function of the frame", { 960, 750 }, 36, 0, SUB, 0.5, 0.5);
    };
    caption(n, len, "Fireworks", "fx::burst(150, { AMBER, ORANGE });  trail.origin = rocketPath;");
}

void campfireScene(TNode* n, int len) {
    sky(n, len, mkColor(0x0b0710), mkColor(0x2a1410));
    n->add(0, len)->fn = [](const Ctx& cx, int) {
        dr::rect(cx, 0, 860, 1920, 220, mkColor(0x140b08));
        // Firelight on the ground.
        DrawCircleGradient(mapP(cx, { 960, 860 }), 520 * cx.s, dr::TA(cx, Color{ 255, 120, 40, 70 }), Color{ 255, 120, 40, 0 });
        const Color wood = mkColor(0x4a2c1c);
        auto log = [&](float a) {
            const float ca = std::cos(a * (float)PI / 180), sa = std::sin(a * (float)PI / 180);
            auto P = [&](float x, float y) { return Vector2{ 960 + x * ca - y * sa, 850 + x * sa + y * ca }; };
            dr::quad(cx, P(-170, -18), P(170, -18), P(170, 18), P(-170, 18), wood);
        };
        log(14);
        log(-14);
    };
    fx::Emitter smoke = fx::smoke(110);
    smoke.prewarm = true;
    smoke.gravity = { 0.012f, -0.015f };   // a light breeze
    emit(n, 0, len, { 960, 700 }, smoke);
    fx::Emitter fire = fx::fire(170);
    fire.rate = 4;
    fire.prewarm = true;
    emit(n, 0, len, { 960, 840 }, fire);
    fx::Emitter rising;
    rising.seed = 5;
    rising.rate = 0.7f;
    rising.prewarm = true;
    rising.area = fx::Area::Line;
    rising.extent = { 140, 0 };
    rising.life = { 50, 90 };
    rising.speed = { 3, 6 };
    rising.spread = 50;
    rising.gravity = { 0, -0.02f };
    rising.drag = 0.02f;
    rising.turbulence = 20;
    rising.look = fx::Look::Glow;
    rising.size = { 5, 10 };
    rising.sizeEnd = 0.3f;
    rising.colors = { AMBER, FLAME };
    rising.additive = true;
    emit(n, 0, len, { 960, 800 }, rising);
    caption(n, len, "Campfire", "fx::fire(170)  fx::smoke(110)  + a stream of rising glows");
}

void rainScene(TNode* n, int len) {
    sky(n, len, mkColor(0x0a1020), mkColor(0x1c2a40));
    n->add(0, len)->fn = [](const Ctx& cx, int) {
        for (int i = 0; i < 24; i++) {
            const float x = i * 82.f - 10, h = (float)rrange(3, i, 160, 520), w = (float)rrange(3, i + 100, 70, 110);
            dr::rect(cx, x, 1000 - h, w, h, mkColor(0x0c121e));
            for (int k = 0; k < 6; k++)
                if (rnd01(4, i * 16 + k) < 0.35)
                    dr::rect(cx, x + 12 + (k % 2) * 30, 1000 - h + 30 + (k / 2) * 60, 14, 22, Color{ 255, 210, 120, 120 });
        }
        dr::rect(cx, 0, 1000, 1920, 80, mkColor(0x0a0e18));
        dr::rect(cx, 1306, 560, 8, 440, mkColor(0x1a2030));
        DrawCircleGradient(mapP(cx, { 1310, 560 }), 260 * cx.s, dr::TA(cx, Color{ 255, 220, 150, 80 }), Color{ 255, 220, 150, 0 });
        dr::circle(cx, { 1310, 560 }, 14, Color{ 255, 236, 200, 255 });
    };
    fx::Emitter far = fx::rain(1920, 1100);
    far.seed = 3;
    far.size = { 0.8f, 1.4f };
    far.speed = { 16, 20 };
    far.life = { 1100 / 18.f, 1100 / 15.f };
    far.colors = { Color{ 150, 170, 210, 90 } };
    emit(n, 0, len, { 960, -40 }, far);
    emit(n, 0, len, { 960, -40 }, fx::rain(1920, 1100));
    fx::Emitter splash;
    splash.seed = 9;
    splash.rate = 4;
    splash.area = fx::Area::Line;
    splash.extent = { 1920, 0 };
    splash.life = { 8, 13 };
    splash.speed = { 2, 4 };
    splash.spread = 120;
    splash.gravity = { 0, 0.4f };
    splash.size = { 2, 3.5f };
    splash.colors = { Color{ 190, 205, 235, 170 } };
    splash.fadeIn = 0;
    emit(n, 0, len, { 960, 1004 }, splash);
    caption(n, len, "Rain", "fx::rain()  // prewarmed: the sky is full on the first frame");
}

void snowScene(TNode* n, int len) {
    sky(n, len, mkColor(0x1b2340), mkColor(0x6b6f9a));
    fx::Emitter far = fx::snow(2000, 1100);
    far.seed = 4;
    far.size = { 2, 4 };
    far.speed = { 0.5f, 1 };
    far.life = { 1100 / 0.9f, 1100 / 0.6f };
    far.rate = 2;
    far.colors = { Color{ 230, 235, 255, 150 } };
    emit(n, 0, len, { 960, -20 }, far);
    n->add(0, len)->fn = [](const Ctx& cx, int) {
        std::vector<Vector2> hill;
        hill.push_back({ 0, 1080 });
        for (int i = 0; i <= 48; i++) {
            const float x = i * 40.f;
            hill.push_back({ x, 860 + 60 * std::sin(x * 0.004f) + 30 * std::sin(x * 0.011f + 1) });
        }
        hill.push_back({ 1920, 1080 });
        dr::fillPoly(cx, hill.data(), (int)hill.size(), mkColor(0xe8ecf8));
    };
    fx::Emitter near = fx::snow(2000, 1100);
    near.size = { 6, 12 };
    near.speed = { 1.4f, 2.2f };
    near.life = { 1100 / 1.6f, 1100 / 1.1f };
    emit(n, 0, len, { 960, -20 }, near);
    caption(n, len, "Snow", "fx::snow()  // two emitters, far and near");
}

Vector2 cometPath(float f) {
    return { 960 + 720 * std::cos(f * 0.024f - 2.6f), 470 + 260 * std::sin(f * 0.048f) };
}

void cometScene(TNode* n, int len) {
    n->add(0, len)->fn = [](const Ctx& cx, int t) {
        dr::rect(cx, 0, 0, 1920, 1080, mkColor(0x04050c));
        for (int i = 0; i < 260; i++) {
            const float tw = 0.6f + 0.4f * std::sin(t * 0.1f + i);
            dr::circle(cx, { (float)rrange(8, i, 0, 1920), (float)rrange(8, i + 1000, 0, 1080) },
                       (float)rrange(8, i + 2000, 0.6, 2.0), withAlpha(WHITE_, tw * (float)rrange(8, i + 3000, 0.3, 0.9)));
        }
    };
    fx::Emitter tail;
    tail.seed = 12;
    tail.rate = 6;
    tail.origin = cometPath;
    tail.life = { 34, 60 };
    tail.speed = { 0.2f, 1.4f };
    tail.turbulence = 8;
    tail.look = fx::Look::Glow;
    tail.size = { 30, 56 };
    tail.sizeEnd = 0;
    tail.overLife = { mkColor(0xffffff), TEAL, withAlpha(IRIS, 0.0f) };
    tail.fadeIn = 0;
    tail.fadeOut = 0.6f;
    tail.additive = true;
    emit(n, 0, len, { 0, 0 }, tail);
    n->add(0, len)->fn = [](const Ctx& cx, int t) {
        const Vector2 h = mapP(cx, cometPath((float)t));
        DrawCircleGradient(h, 60 * cx.s, dr::TA(cx, Color{ 200, 250, 255, 200 }), Color{ 200, 250, 255, 0 });
        dr::circle(cx, cometPath((float)t), 9, WHITE_);
    };
    caption(n, len, "Moving source", "tail.origin = [](float f) { return path(f); };");
}

void customScene(TNode* n, int len) {
    sky(n, len, mkColor(0x03100a), mkColor(0x08200f));
    fx::Emitter bits;
    bits.seed = 7;
    bits.rate = 2;
    bits.prewarm = true;
    bits.area = fx::Area::Line;
    bits.extent = { 1900, 0 };
    bits.life = { 250, 330 };
    bits.speed = { 3.5f, 5.5f };
    bits.spread = 0;
    bits.size = { 18, 44 };
    bits.colors = { LEAF, mkColor(0x3ddc84), mkColor(0xc8ffd0) };
    bits.fadeIn = 0.1f;
    bits.fadeOut = 0.4f;
    bits.draw = [](const Ctx& cx, const fx::Particle& p) {
        const char* glyph = (p.random < 0.5f) == ((int)(p.age / 18) % 2 == 0) ? "0" : "1";   // flips as it rises
        dr::text(mono(), cx, glyph, p.pos, p.size, 0, withAlpha(p.color, p.alpha), 0.5, 0.5);
    };
    emit(n, 0, len, { 960, 1120 }, bits);
    caption(n, len, "Custom look", "bits.draw = [](const Ctx& cx, const fx::Particle& p) { ... };");
}

void finaleScene(TNode* n, int len) {
    sky(n, len, mkColor(0x14102a), mkColor(0x2a1840));
    fx::Emitter rainOf = fx::confetti(2100);
    rainOf.from = 30;
    emit(n, 0, len, { 960, -40 }, rainOf);
    for (int side = 0; side < 2; side++) {
        fx::Emitter cannon = fx::confetti();
        cannon.seed = 70 + side;
        cannon.rate = 0;
        cannon.burst = 140;
        cannon.burstAt = 6;
        cannon.burstSpread = 8;
        cannon.area = fx::Area::Point;
        cannon.speed = { 20, 40 };
        cannon.angle = side ? -118.f : -62.f;
        cannon.spread = 30;
        cannon.gravity = { 0, 0.3f };
        cannon.drag = 0.04f;
        cannon.life = { 150, 220 };
        emit(n, 0, len, { side ? 1880.f : 40.f, 1100 }, cannon);
    }
    TNode* t = n->add(18, len - 18);
    t->up = [](int f, NodeProps& p) {
        p.opacity = (float)interpolate(f, { 0, 20 }, { 0, 1 }, clamped());
        p.scale = (float)spring(f, { 30.0 });
        p.pos = { 960, 500 };
    };
    t->fn = [](const Ctx& cx, int) {
        dr::text(mono(), cx, "#include \"rmtn/particles.hpp\"", { 0, -10 }, 52, 0, AMBER, 0.5, 0.5);
        dr::text(regular(), cx, "burst  sparks  fountain  confetti  rain  snow  fire  smoke  embers", { 0, 70 }, 34, 0, SUB, 0.5, 0.5);
    };
}

} // namespace

void registerParticles(Registry& out) {
    Comp c;
    c.name = "particles";
    c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = mkColor(0x060818);
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);
    struct S { int len; void (*make)(TNode*, int); };
    const S scenes[] = { { 150, fireworksScene }, { 150, campfireScene }, { 130, rainScene }, { 130, snowScene },
                         { 140, cometScene }, { 130, customScene }, { 170, finaleScene } };
    bool first = true;
    for (const S& s : scenes) {
        if (!first) ts.transition(Transition::fade(XF));
        first = false;
        s.make(ts.add(s.len), s.len);
    }
    c.duration = ts.end();
    out.push_back(std::move(c));
}

} // namespace rmtn
