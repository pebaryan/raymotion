// scenes.cpp -- compositions registry. Every visual and audio element is computed from numbers.
#include <raylib.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <memory>
#include "rmtn/timeline.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/render3d.hpp"

namespace rmtn {

void register_ht_explainer(Registry& out);
void register_ht_plain(Registry& out);
void register_ht_feed(Registry& out);
void registerFxSeries(Registry& out);
void registerGpuScene(Registry& out);
void registerPrefabs(Registry& out);
void registerAspFeed(Registry& out);
void registerTokenStory(Registry& out);
void registerOwnCompute(Registry& out);
void registerPromo(Registry& out);
void registerNpr(Registry& out);
void registerDiagrams(Registry& out);
void registerParticles(Registry& out);
void registerParticles3D(Registry& out);
void registerAudioReactive(Registry& out);
void registerCaptions(Registry& out);
void registerIcons(Registry& out);
void registerFlags(Registry& out);
void registerAustriaJourney(Registry& out);
void registerAustriaTimeTravel(Registry& out);
void registerMaps(Registry& out);
void registerCities(Registry& out);
void registerEarth(Registry& out);
void registerAtmosphere(Registry& out);
void registerMapCharts(Registry& out);
void registerPaths(Registry& out);
void registerEquations(Registry& out);
void registerPlots(Registry& out);
void registerCharts(Registry& out);
void registerSpaces(Registry& out);
void registerCamera(Registry& out);
void registerCode(Registry& out);
void registerText(Registry& out);
void registerMotionBlur(Registry& out);
void registerSparql(Registry& out);
void registerLowerThird(Registry& out);

void register_scenes(Registry& out) {
using namespace dr;

// ============================================================================ probe
{
    Comp c; c.name = "probe"; c.w = 1280; c.h = 720; c.fps = 30; c.duration = 60;
    const int W = c.w, H = c.h, FPS = c.fps;

    TNode* n = addRoot(c.roots, 0, 60);
    n->fn = [=](const Ctx& cx, int t) {
        dr::rect(cx, 0, 0, (float)W, (float)H, mkColor(0x12101d));
        double u = (double)(t % 45) / 45.0;                 // parabola hop
        float x = (float)(W*0.12) + (float)t * (float)(W*0.76/FPS*2);
        if (x > W - 120) x = (float)W - 120;
        float y = (float)(H*0.78) - (float)std::fabs(std::sin(u*PI)) * (float)(H*0.45);
        for (int g = 3; g >= 1; g--) {                     // motion ghosts
            double ug = (double)((t - g*2) % 45) / 45.0;
            float yg = (float)(H*0.78) - (float)std::fabs(std::sin(ug*PI)) * (float)(H*0.45);
            Color col = mixC(mkColor(0x36e7ff,90), mkColor(0xff3b9e,90), (float)g/3.0f);
            dr::circle(cx, { x - (float)g*46, yg }, 34.f, col);
        }
        float pulse = 34.f * (1.f + 0.16f*std::sin(t*0.4f));
        dr::circle(cx, { x, y }, pulse, mkColor(0x36e7ff,235));
        dr::ring(cx, { x, y }, pulse + 14.f, 3.f, mkColor(0xffffff, 120));
    };

    n = addRoot(c.roots, 0, 60);
    n->fn = [=](const Ctx& cx, int) {
        Font f = GetFontDefault();
        dr::text(f, cx, "PROBE", { W/2.f, H/2.f }, 90.f, 14.f, mkColor(0xf2f4ff,230), 0.5, 0.5);
    };
    out.push_back(std::move(c));
}

// ============================================================================ demo-neon
{
    Comp c; c.name = "demo-neon"; c.w = 1920; c.h = 1080; c.fps = 30; c.duration = 225; c.music = true;
    const int W = c.w, H = c.h;
    const Color BG   = mkColor(0x0a0a14), GLOW = mkColor(0x241238);
    const Color NEONC= mkColor(0x36e7ff), NEONM= mkColor(0xff3b9e), NEONY= mkColor(0xffd35c);
    const Color INKW = mkColor(0xf2f4ff);
    const std::string TITLE = "RAYMOTION";

    // --- background: base fill + drifting radial glow -------------------------
    TNode* bg = addRoot(c.roots, 0, 225);
    bg->fn = [=](const Ctx& cx, int t) {
        dr::rect(cx, 0, 0, (float)W, (float)H, BG);
        Vector2 ctr = mapP(cx, { W/2.0f + 160.0f*std::sin((float)t*0.011f), H*0.42f });
        Color inner = mixC(GLOW, NEONM, 0.10f + 0.10f*std::sin(t*0.02f));
        Color outer = withAlpha(BG, (0.0f)/255.f);
        DrawCircleGradient(ctr, 1500.0f, withAlpha(inner, (255*cx.a*.55f)/255.f), outer);
    };

    // --- ambient blobs --------------------------------------------------------
    TNode* blobs = addRoot(c.roots, 0, 225);
    blobs->up = [](int t, NodeProps& p) { TrackFloat o{}; o.add(0,0,E_OUT_QUAD); o.add(40,1,E_LINEAR); o.add(200,1,E_LINEAR); o.add(225,0,E_IN_QUAD); p.opacity=o.at(t); };
    blobs->fn = [=](const Ctx& cx, int t) {
        const unsigned long long seed = 424242ULL;
        for (int k = 0; k < 7; k++) {
            float bx = W*(0.12f + 0.76f*(float)rnd01(seed,k)) + 30.0f*std::sin((float)(t+k*40)*0.04f);
            float by = H*(0.22f + 0.58f*(float)rnd01(seed,k+99)) + 24.0f*std::cos((float)(t+k*33)*0.05f);
            float base = (float)rrange(seed,k+3, 60,160);
            float amp = base*0.22f;
            int freq = (k % 3) + 2;
            float ph = (float)((t*(2+k) + k*57) % 360);
            Color col = mixC(NEONC, NEONM, (float)rnd01(seed,k+7));
            dr::blob(cx, { bx, by }, base, amp, freq, ph, withAlpha(col, (34)/255.f));
        }
    };

    // --- parallax grid (shot B intro) ----------------------------------------
    TNode* grid = addRoot(c.roots, 145, 80);
    grid->up = [](int t, NodeProps& p) { TrackFloat o{}; o.add(0,0,E_LINEAR); o.add(24,1,E_LINEAR); o.add(66,1,E_LINEAR); o.add(79,0,E_IN_QUAD); p.opacity=o.at(t); };
    grid->fn = [=](const Ctx& cx, int t) {
        const float cell = 64.f;
        Color gcol = mkColor(0x3552c9, 66);
        float yy = std::fmod((float)t*1.25f, cell);
        float xx = std::fmod((float)t*0.35f, cell);
        for (float y = -cell; y < H + cell; y += cell) dr::strokeSeg(cx, { 0, y+yy }, { (float)W, y+yy }, 1.4f, gcol);
        for (float x = -cell; x < W + cell; x += cell) dr::strokeSeg(cx, { x+xx, 0 }, { x+xx, (float)H }, 1.4f, gcol);
    };

    // --- intro flash (over bg/blobs, under title) -----------------------------
    TNode* flash = addRoot(c.roots, 58, 8);
    flash->up = [](int t, NodeProps& p) { TrackFloat o{}; o.add(0,0.88f,E_OUT_EXPO); o.add(7,0.0f,E_LINEAR); p.opacity=o.at(t); };
    flash->fn = [=](const Ctx& cx, int) { dr::rect(cx, 0, 0, (float)W, (float)H, INKW); };

    // --- particle burst behind the title --------------------------------------
    TNode* part = addRoot(c.roots, 66, 66);
    part->props.pos = { W/2.0f, H*0.52f };
    part->fn = [=](const Ctx& cx, int t) {
        const unsigned long long seed = 777ULL;
        for (int i = 0; i < 46; i++) {
            float birth = (float)rrange(seed,i, 0,14);
            float L = (float)rrange(seed,i+500, 30,60);
            double u = ((double)t - birth) / L;
            if (u < 0 || u > 1) continue;
            double a = PI + rnd01(seed,i)*PI;                      // upward half
            double dist = 90.0 + 520.0*powOut(u,2.6);
            Vector2 p{ (float)(std::cos(a)*dist), (float)(std::sin(a)*dist) };
            Vector2 q{ (float)(std::cos(a)*(dist-34.0)), (float)(std::sin(a)*(dist-34.0)) };
            float alphaMul = (u < 0.1 ? u/0.1 : 1.0-u);
            Color col = mixC(NEONC, NEONY, (float)rnd01(seed,i+21));
            dr::strokeSeg(cx, q, p, 1.6f, withAlpha(col, (128*alphaMul)/255.f));
            float rr = (float)rrange(seed,i+2, 2.5,7) * (float)(1.0-0.35*u);
            dr::circle(cx, p, rr, withAlpha(col, (210*alphaMul)/255.f));
        }
    };

    // --- typewriter title ------------------------------------------------------
    TNode* title = addRoot(c.roots, 60, 115);
    title->props.pos = { W/2.0f, H*0.46f };
    title->up = [W,H](int t, NodeProps& p) {
        TrackFloat op{}; op.add(0,0,E_LINEAR); op.add(14,1,E_LINEAR); op.add(100,1,E_LINEAR); op.add(114,0,E_IN_QUAD);
        p.opacity = op.at(t);
        TrackVec pv{}; pv.add(0,{0,90},E_LINEAR); pv.add(26,{0,0},E_BACK);
        p.pos.x = W/2.0f; p.pos.y = H*0.46f + (float)pv.at(t).y;
    };
    title->fn = [=](const Ctx& cx, int t) {
        Font f = GetFontDefault();
        int reveal = (int)std::min<double>((double)TITLE.size(),
                       std::max(0.0, (double)TITLE.size() * easeFn(E_OUT_CUBIC, clamp01(t/42.0))));
        std::string sub = TITLE.substr(0, reveal);
        Vector2 fullM = measure(f, TITLE.c_str(), 150.f*cx.s, 10.f*cx.s);
        float startX = -fullM.x*0.5f;
        dr::text(f, cx, sub.c_str(), { startX, 0 }, 150.f, 10.f, INKW, 0, 0);
        if (t < 50 && t % 24 < 14) {          // blinking cursor while typing
            float wRev = (reveal>0)? measure(f, sub.c_str(), 150.f*cx.s, 10.f*cx.s).x : 0;
            dr::rect(cx, startX + wRev + 12, 18, 10, 130, NEONM);
        }
        double e = easeFn(E_OUT_QUAD, clamp01((t-18)/32.0));     // subtitle slide-up
        if (e > 0.001) {
            const char* ST = "motion graphics, generated entirely by code";
            Color scol = NEONC; scol.a = (unsigned char)(scol.a * e * 0.95);
            dr::text(f, cx, ST, { 0, (float)(120 + (1-e)*40) }, 34.f, 6.f, scol, 0.5, 0);
        }
    };

    // --- stat chips row ----------------------------------------------------------
    TNode* row = addRoot(c.roots, 150, 78);
    row->props.pos = { W/2.0f, H*0.78f };
    row->up = [](int t, NodeProps& p) { TrackFloat o{}; o.add(0,1); o.add(64,1); o.add(77,0,E_IN_QUAD); p.opacity=o.at(t); };
    struct ChipSpec { const char* label; float xoff; int off; bool counted; };
    const ChipSpec specs[3] = {
        { "CANVAS 1920x1080", -470, 0, false },
        { "FRAMES RENDERED",     0, 10, true },
        { "ASSETS: CODE ONLY", 470, 20, false },
    };
    for (int k = 0; k < 3; k++) {
        const ChipSpec& sp = specs[k];
        TNode* chip = row->add(sp.off, 70);
        chip->up = [sp](int t, NodeProps& p) {
            TrackVec in{}; in.add(0,{sp.xoff - 380, 0},E_LINEAR); in.add(24,{sp.xoff,0},E_BACK);
            p.pos = in.at(t);
        };
        chip->fn = [=](const Ctx& cx, int t) {
            Font f = GetFontDefault();
            Color panel = mkColor(0x10193a, 210), edge = mixC(NEONC, NEONY, (float)k/2.0f);
            dr::roundRect(cx, -215, -66, 430, 132, 18, panel);
            dr::ring(cx, { 0,0 }, 86, 2, withAlpha(edge, (70)/255.f));
            dr::text(f, cx, sp.label, { -190, -18 }, 26.f, 3.f, withAlpha(INKW, (225)/255.f), 0, 0);
            char buf[24]; buf[0] = 0;
            if (sp.counted) snprintf(buf, sizeof buf, "%04d", (int)(t + sp.off)); // counts local frame time
            if (buf[0]) dr::text(f, cx, buf, { -190, 26 }, 40.f, 4.f, edge, 0, 0);
        };
    }

    // --- rotating logo mark + wordmark -------------------------------------------
    TNode* mark = addRoot(c.roots, 186, 39);
    mark->props.pos = { W*0.79f, H*0.28f };
    mark->up = [](int t, NodeProps& p) {
        TrackFloat op{}; op.add(0,0); op.add(12,1,E_OUT_CUBIC); op.add(39,1); p.opacity=op.at(t);
        TrackVec sc{}; sc.add(0,{0.55,0.55},E_LINEAR); sc.add(16,{1,1},E_BACK);
        p.scale = sc.at(t).x;
    };
    mark->fn = [=](const Ctx& cx, int t) {
        dr::ring(cx, {0,0}, 92, 6, NEONC);
        dr::ring(cx, {0,0}, 64, 3, NEONM);
        float rotOff = (float)easeFn(E_IN_OUT_CUBIC, clamp01(t/39.0)) * 72.0f;
        dr::star(cx, {0,0}, 52, 22, 5, rotOff, NEONY);
        float a0 = (float)t*4.0f, a1 = a0 + 46.0f;
        dr::sector(cx, {0,0}, 108, a0, a1, withAlpha(INKW, (60)/255.f));
    };
    TNode* wmark = addRoot(c.roots, 190, 35);
    wmark->props.pos = { W/2.0f, H - 70.0f };
    wmark->up = [](int t, NodeProps& p) { TrackFloat o{}; o.add(0,0); o.add(20,1,E_OUT_CUBIC); o.add(28,1); o.add(34,0,E_IN_QUAD); p.opacity=o.at(t); };
    wmark->fn = [=](const Ctx& cx, int) {
        Font f = GetFontDefault();
        dr::text(f, cx, "RAYMOTION  //  raylib 6.2 + ffmpeg", { 0, 0 }, 28.f, 5.f, withAlpha(INKW, (210)/255.f), 0.5, 0);
    };

    // --- outro fade ------------------------------------------------------------
    TNode* outro = addRoot(c.roots, 213, 12);
    outro->up = [](int t, NodeProps& p) { TrackFloat o{}; o.add(0,0); o.add(11,1,E_IN_QUAD); p.opacity=o.at(t); };
    outro->fn = [=](const Ctx& cx, int) { dr::rect(cx, -10, -10, (float)W+20, (float)H+20, mkColor(0x05050a)); };

    // --- soundtrack: sine-sum pads and plucks -----------------------------------
    for (auto e : { SongEvent{0.3,2.4,131.0,0.30,false}, SongEvent{2.4,2.6,196.0,0.28,false}, SongEvent{4.6,2.8,174.6,0.30,false} })
        c.song.push_back(e);
    for (int i = 0; i < 21; i++) {
        SongEvent p{}; p.t0 = 0.6 + i*0.31; p.dur = 0.55; p.amp = 0.34; p.pluck = true;
        double base[] = { 523.25, 659.25, 784.0 };
        p.freq = base[i % 3] * (i % 6 < 3 ? 1.0 : 1.16);
        c.song.push_back(p);
    }

    out.push_back(std::move(c));
}

// ============================================================================ geo-orbit (procedural 3D)
{
    Comp c; c.name = "geo-orbit"; c.w = 1920; c.h = 1032; c.fps = 30; c.duration = 270; c.music = true;
    const int W = c.w, H = c.h, FPS = c.fps;

    using namespace d3;
    Mesh knot  = torusKnot(1.0f, 240, 10, 0.30f);
    Mesh ball  = icosphere(1.0f);
    Mesh sat0  = sphereMesh(1.0f, 10, 14);
    Mesh cryst = octahedron(1.0f);
    Mesh sats[5];
    for (int k = 0; k < 5; k++) {
        sats[k] = sat0;
        Color tint = ColorFromHSV((k*71 + 200) % 360, 0.85f, 0.95f);
        for (auto& f : sats[k]) f.col = tint;
    }
    Mesh gems[7];
    for (int i = 0; i < 7; i++) {
        gems[i] = cryst;
        Color tint = ColorFromHSV((i*47 + 20) % 360, 0.8f, 0.9f);
        for (auto& f : gems[i]) f.col = tint;
    }

    Mesh sun = sphereMesh(1.0f, 10, 20);
    for (auto& f : sun) f.col = ColorFromHSV(26, 0.9f, 0.95f);

    TNode* n3 = addRoot(c.roots, 0, 270);
    n3->fn = [=](const Ctx&, int t) {
        double tt = (double)t / FPS;
        float spin = (float)(tt * 0.9);
        Cam cam{ { 0.45f*(float)std::sin(tt*0.19), 2.5f + 0.3f*(float)std::sin(tt*0.29), -12.5f + 0.5f*(float)std::cos(tt*0.15) },
                 { 0.f, 1.7f, 4.f }, 46.f };

        d3::frameBegin();

        for (int i = 0; i < 260; i++) {                      // twinkling starfield
            unsigned h = i*2654435761u + (unsigned)t*327579u; h ^= h>>13; h *= 1303377u; h ^= h>>15;
            float sx = (float)(h % (unsigned)W);
            float sy = (float)((h/7919u) % (unsigned)(H*14/25));
            float tw = 0.5f + 0.5f*std::sin(tt*(1.0+(i%5)) + (float)i);
            int a = (int)(125*tw); if (a > 4) {
                Color sc = withAlpha(ColorFromHSV((i*37)%360, 0.45f, 1.f), (a)/255.f);
                dr::circle(Ctx{}, { sx, sy }, 1.6f, sc);
            }
        }

        grid3d(cam, W, H, 36.f, 34, withAlpha(mkColor(0x1fc4ff), (255)/255.f), 13.f);

        DrawOpts sunO{}; sunO.amb = 0.35f; sunO.diff = 0.9f; sunO.alpha = 150; sunO.wire = true;
        drawMesh(sun, trs({ 4.5f, 4.2f, 27.f }, { 0.f, spin*0.04f, 0.f }, 3.6f), cam, W, H, sunO);
                DrawOpts knotO{}; knotO.rainbow = true; knotO.alpha = 255; knotO.wire = false;
        drawMesh(knot, trs({ -1.0f, 1.9f, 6.5f }, { spin*0.55f, spin, 0.4f }, 0.95f + 0.05f*std::sin(spin*2.f)), cam, W, H, knotO);
        DrawOpts ballO{}; ballO.amb = 0.15f; ballO.diff = 1.f; ballO.spec = 0.65f; ballO.wire = true;
        drawMesh(ball, trs({ 1.4f, 2.0f, 1.6f }, { 0.f, spin*0.85f, 0.f }, 1.3f), cam, W, H, ballO);
        for (int k = 0; k < 5; k++) {
            double ph = tt*0.85 + k*(6.283/5);
            V3 op{ -1.0f + 2.4f*(float)std::cos(ph), 1.9f + 0.9f*(float)std::sin(ph*1.3), 6.5f + 1.7f*(float)std::sin(ph) };
            if (k < 2) {
                for (int gh = 3; gh >= 1; gh--) {
                    double pg = ph - gh*0.13;
                    V3 gp{ -1.0f + 2.4f*(float)std::cos(pg), 1.9f + 0.9f*(float)std::sin(pg*1.3), 6.5f + 1.7f*(float)std::sin(pg) };
                    DrawOpts go{}; go.alpha = 55; go.amb = 0.05f; go.diff = 0.3f; go.wire = false;
                    drawMesh(sats[k], trs(gp, { (float)pg, (float)pg*1.4f, 0.f }, 0.24f), cam, W, H, go);
                }
            }
            DrawOpts so{}; so.alpha = 255; so.wire = false;
            drawMesh(sats[k], trs(op, { (float)ph, (float)ph*1.4f, 0.f }, 0.27f), cam, W, H, so);
        }
        for (int i = 0; i < 7; i++) {
            float px = -8.f + 2.7f*i, pz = 10.f + (i%3)*2.6f;
            double bob = 0.66 + 0.16*std::sin(tt*1.3 + i*1.9);
            DrawOpts co{}; co.amb = 0.18f; co.diff = 1.1f; co.alpha = 255;
            drawMesh(gems[i], trs({ px, (float)bob, pz }, { 0.f, (float)(tt*0.4 + i), (float)(i*0.35) }, 0.6f), cam, W, H, co);
        }
        d3::frameEnd3D(W, H);
    };

    // --- HUD overlays ------------------------------------------------------------
    {
        TNode* intro = addRoot(c.roots, 0, 20);
        intro->up = [](int t, NodeProps& p) { TrackFloat o{}; o.add(0,1); o.add(19,0,E_IN_QUAD); p.opacity=o.at(t); };
        intro->fn = [=](const Ctx& cx, int) { dr::rect(cx, -10, -10, (float)W+20, (float)H+20, mkColor(0x04040a)); };

        TNode* title = addRoot(c.roots, 22, 30);
        title->props.pos = { W/2.0f, H*0.30f };
        title->up = [H](int t, NodeProps& p) {
            double e = easeFn(E_OUT_CUBIC, clamp01((t-6)/30.0));
            TrackFloat o{}; o.add(0,0); o.add(16,1,E_OUT_QUAD); o.add(28,1); o.add(29,0);
            p.pos.y = (float)(H*0.30 + (1.0-e)*90.0); p.opacity = o.at(t);
        };
        title->fn = [=](const Ctx& cx, int) {
            Font f = GetFontDefault();
            dr::text(f, cx, "GEO-ORBIT", { 0, 0 }, 120.f, 12.f, withAlpha(mkColor(0xf2f4ff), (255)/255.f), 0.5, 0);
        };

        TNode* tcode = addRoot(c.roots, 0, 270);
        tcode->props.pos = { 44.f, (float)H - 44.f };
        tcode->fn = [=](const Ctx& cx, int t) {
            Font f = GetFontDefault();
            char buf[32]; snprintf(buf, sizeof buf, "FRAME %03d / 270", t);
            dr::text(f, cx, buf, { 0, 0 }, 24.f, 3.f, withAlpha(mkColor(0x1fc4ff), (175)/255.f), 0, 0);
        };

        TNode* outro = addRoot(c.roots, 255, 15);
        outro->up = [](int t, NodeProps& p) { TrackFloat o{}; o.add(0,0); o.add(14,1,E_IN_QUAD); p.opacity=o.at(t); };
        outro->fn = [=](const Ctx& cx, int) { dr::rect(cx, -10, -10, (float)W+20, (float)H+20, mkColor(0x04040a)); };

        TNode* bar = addRoot(c.roots, 0, 270);
        bar->fn = [=](const Ctx& cx, int t) {
            float pr = (float)t/(float)270.f;
            dr::rect(cx, 60.f, (float)H-16.f, (float)(W-120)*pr, 5.f, withAlpha(mkColor(0x1fc4ff), (90)/255.f));
        };
    }

    // --- soundtrack: pads + pentatonic plucks + kicks ----------------------------
    for (auto e : { SongEvent{0.4,3.2,98.0,0.34,false}, SongEvent{3.6,3.4,147.0,0.30,false}, SongEvent{6.8,2.0,196.0,0.28,false} })
        c.song.push_back(e);
    for (int i = 0; i < 34; i++) {
        SongEvent p{}; p.t0 = 1.2 + i*0.24; p.dur = 0.4; p.amp = 0.30; p.pluck = true;
        static const double pent[] = { 392.0, 448.4, 523.25, 587.3, 698.4 };
        p.freq = pent[i % 5] * (i % 9 < 4 ? 1.0 : 1.31);
        c.song.push_back(p);
    }
    for (int i = 0; i < 9; i++) {
        SongEvent k{}; k.t0 = 1.0 + i*1.0; k.dur = 0.16; k.amp = 0.5; k.pluck = true; k.freq = 74.0;
        c.song.push_back(k);
    }

    out.push_back(std::move(c));
}

registerFxSeries(out);
registerGpuScene(out);
register_ht_explainer(out);
register_ht_plain(out);
register_ht_feed(out);
registerPrefabs(out);
registerAspFeed(out);
registerTokenStory(out);
registerOwnCompute(out);
registerPromo(out);
registerNpr(out);
registerDiagrams(out);
registerParticles(out);
registerParticles3D(out);
registerAudioReactive(out);
registerCaptions(out);
registerIcons(out);
registerFlags(out);
registerAustriaJourney(out);
registerAustriaTimeTravel(out);
registerMaps(out);
registerCities(out);
registerEarth(out);
registerAtmosphere(out);
registerMapCharts(out);
registerPaths(out);
registerEquations(out);
registerPlots(out);
registerCharts(out);
registerSpaces(out);
registerCamera(out);
registerCode(out);
registerText(out);
registerMotionBlur(out);
registerSparql(out);
registerLowerThird(out);
} // register_scenes
} // namespace rmtn
