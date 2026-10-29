// a1_sein_six_seats.cpp -- "a1-sein-six-seats": a 38.5 s vertical German lesson.
// Six measured scene WAVs in assets/vo/a1-sein; the table is the proof object.
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace rmtn {
namespace {

constexpr int W = 1080, H = 1920, FPS = 30, TOTAL = 1155;
constexpr int SCENE_START[6] = { 0, 167, 306, 513, 806, 1038 };
constexpr int SCENE_END[6] = { 167, 306, 513, 806, 1038, 1155 };
constexpr const char* VO_FILES[6] = {
    "s01.wav", "s02.wav", "s03.wav", "s04.wav", "s05.wav", "s06.wav"
};

const Color BG = mkColor(0x101a17), TABLE = mkColor(0x1c4036), EDGE = mkColor(0x2f5b4d),
            CREAM = mkColor(0xf3ebdd), INK = mkColor(0x101a17), MUTED = mkColor(0x8fa6a0),
            AMBER = mkColor(0xffb45c), MINT = mkColor(0x7fe0c3), PAPER = mkColor(0xf7f4ec),
            RULE = mkColor(0x2b4b40);

const float TX = 96, TY = 300, TW = 888, TH = 1000;
const float SET_X[2] = { 160, 580 }, SET_W = 340;
const float CARD_Y[3] = { 430, 690, 950 }, PLATE_Y[3] = { 535, 795, 1055 };
const float CARD_H = 84, PLATE_H = 92;

struct Caption {
    double a, b;
    const char* text;
};
const Caption CAPTIONS[] = {
    { 0.00, 2.15, "Why ich bin — but du bist?" },
    { 2.15, 5.30, "Same verb. Six seats." },
    { 5.58, 7.10, "Sein is irregular." },
    { 7.10, 10.21, "Learn it seat by seat." },
    { 10.21, 11.90, "ich bin = I am" },
    { 11.90, 13.45, "du bist = you are" },
    { 13.45, 17.12, "er/sie/es ist = he, she, it is" },
    { 17.12, 18.85, "wir sind = we are" },
    { 18.85, 20.60, "ihr seid = you all are" },
    { 20.60, 24.40, "sie/Sie sind = they / you (formal)" },
    { 24.40, 26.87, "Sind appears twice." },
    { 26.87, 33.35, "Your turn: Wir ___ hier." },
    { 33.35, 34.59, "sind" },
    { 34.59, 38.47, "Six seats. One verb. Learn the table." },
};

float ramp(int t, float a, float b) {
    const float u = std::clamp((t - a) / std::max(0.001f, b - a), 0.f, 1.f);
    return u * u * (3 - 2 * u);
}
float mixf(float a, float b, float u) { return a + (b - a) * u; }

Font sans() { return fonts::load({ "ArchivoBlack-Regular.ttf", "Inter-Bold.ttf" }, 96); }
Font body() { return fonts::load({ "SpaceGrotesk-Variable.ttf", "Inter-Regular.ttf" }, 96); }
Font mono() { return fonts::load({ "JetBrainsMono-Bold.ttf" }, 64); }

void text(Font f, const Ctx& cx, const char* s, Vector2 at, float size, Color col,
          float ax = 0, float ay = 0.5f) {
    dr::text(f, cx, s, at, size, 0, col, ax, ay);
}

void card(const Ctx& cx, float x, float y, float w, float h, const char* pronoun,
          float k, float alpha, bool formal) {
    if (k <= 0.001f || alpha <= 0.001f) return;
    Ctx m = cx;
    m.o = mapP(cx, { x + w / 2, y + h / 2 + (1 - k) * 16 });
    m.s *= 0.94f + 0.06f * k;
    m.a *= std::min(1.f, k) * alpha;
    dr::roundRect(m, -w / 2, -h / 2, w, h, 16, CREAM);
    const float size = std::strlen(pronoun) > 8 ? 34.f : 48.f;
    text(body(), m, pronoun, { 0, formal ? -8.f : 0.f }, size, INK, 0.5f, 0.5f);
    if (formal) {
        text(mono(), m, "formal", { 0, 24 }, 15, MUTED, 0.5f, 0.5f);
    }
}

void plate(const Ctx& cx, float x, float y, float w, float h, const char* form,
           float k, float alpha) {
    if (k <= 0.001f || alpha <= 0.001f) return;
    const float sx = k < 0.5f ? 1.f - 2.f * k : 2.f * k - 1.f;
    const float ww = std::max(8.f, w * sx);
    Ctx m = cx;
    m.o = mapP(cx, { x + w / 2, y + h / 2 });
    m.a *= alpha;
    dr::roundRect(m, -ww / 2, -h / 2, ww, h, 14, TABLE);
    dr::roundRect(m, -ww / 2, -h / 2, ww, 3, 1.5f, MINT);
    if (k > 0.55f) {
        const float a = std::clamp((k - 0.55f) / 0.45f, 0.f, 1.f);
        Ctx q = m;
        q.a *= a;
        text(body(), q, form, { 0, 0 }, 58, MINT, 0.5f, 0.5f);
    }
}

void emptySetting(const Ctx& cx, float x, float y, float w, float h, float alpha) {
    Ctx m = cx;
    m.a *= 0.28f * alpha;
    dr::roundRect(m, x, y, w, h, 16, withAlpha(CREAM, 0.16f));
    dr::circle(m, { x + w / 2, y + h / 2 }, 4, withAlpha(CREAM, 0.36f));
}

void drawTable(const Ctx& cx, float alpha, float plaqueK) {
    if (alpha <= 0.001f) return;
    Ctx m = cx;
    m.a *= alpha;
    dr::roundRect(m, TX - 5, TY - 5, TW + 10, TH + 10, 36, EDGE);
    dr::roundRect(m, TX, TY, TW, TH, 30, TABLE);
    for (int c = 0; c < 2; c++) {
        for (int r = 0; r < 3; r++) {
            emptySetting(m, SET_X[c], CARD_Y[r], SET_W, CARD_H, 1);
            emptySetting(m, SET_X[c], PLATE_Y[r], SET_W, PLATE_H, 1);
        }
    }
    if (plaqueK > 0.001f) {
        Ctx q = m;
        q.o = mapP(m, { W / 2, 368 + (1 - plaqueK) * -22 });
        q.a *= plaqueK;
        dr::roundRect(q, -150, -38, 300, 76, 16, AMBER);
        text(sans(), q, "SEIN", { 0, -7 }, 42, INK, 0.5f, 0.5f);
        text(mono(), q, "to be", { 0, 22 }, 15, INK, 0.5f, 0.5f);
    }
}

void setting(const Ctx& cx, int col, int row, const char* pronoun, const char* form,
             float cardK, float plateK, float alpha, bool formal = false) {
    card(cx, SET_X[col], CARD_Y[row], SET_W, CARD_H, pronoun, cardK, alpha, formal);
    plate(cx, SET_X[col], PLATE_Y[row], SET_W, PLATE_H, form, plateK, alpha);
}

void hookCard(const Ctx& cx, float x, float y, const char* pronoun, const char* form,
              float cardK, float plateK, float alpha) {
    card(cx, x, y, 420, 100, pronoun, cardK, alpha, false);
    plate(cx, x, y + 112, 420, 108, form, plateK, alpha);
}

void hookLink(const Ctx& cx, float k, float alpha) {
    if (k <= 0.001f) return;
    const float x0 = 510, x1 = 570;
    const float x = mixf(x0, x1, k);
    Ctx m = cx;
    m.a *= alpha;
    dr::strokeSeg(m, { x0, 866 }, { x, 866 }, 4, AMBER);
    dr::circle(m, { x, 866 }, 8, AMBER);
}

void sharedLink(const Ctx& cx, float k, float alpha) {
    if (k <= 0.001f) return;
    const float x = 948, y0 = 581, y1 = 1101;
    const float y = mixf(y0, y1, k);
    Ctx m = cx;
    m.a *= alpha;
    dr::strokeSeg(m, { x, y0 }, { x, y }, 6, MINT);
    if (k >= 0.98f) {
        dr::strokeSeg(m, { 920, y0 }, { x, y0 }, 3, MINT);
        dr::strokeSeg(m, { 920, y1 }, { x, y1 }, 3, MINT);
        dr::circle(m, { x, y0 }, 7, MINT);
        dr::circle(m, { x, y1 }, 7, MINT);
    }
}

void drawSentence(const Ctx& cx, int t) {
    const float k = ramp(t, 0, 12);
    const float strip = ramp(t, 24, 34);
    const float answer = ramp(t, 198, 210);
    Ctx m = cx;
    m.a *= k;
    if (k <= 0.001f) return;
    text(mono(), m, "YOUR TURN", { W / 2, 252 }, 20, AMBER, 0.5f, 0.5f);
    Ctx s = m;
    s.o = mapP(m, { W / 2, 735 + (1 - strip) * 24 });
    s.a *= strip;
    dr::roundRect(s, -420, -88, 840, 176, 22, CREAM);
    text(sans(), s, "Wir", { -330, -2 }, 52, INK, 0, 0.5f);
    text(sans(), s, "hier.", { 244, -2 }, 52, INK, 0, 0.5f);
    if (answer <= 0.001f) {
        dr::roundRect(s, -74, -48, 178, 96, 14, withAlpha(TABLE, 1));
        dr::roundRect(s, -74, -48, 178, 3, 1.5f, MINT);
        const float arc = ramp(t, 72, 192);
        if (arc > 0.001f && t < 198) {
            dr::arc(s, { 15, 0 }, 64, 5, -90, -90 + 360 * arc, withAlpha(MINT, 0.95f));
        }
    } else {
        Ctx q = s;
        q.a *= answer;
        dr::roundRect(q, -74, -48, 178, 96, 14, TABLE);
        dr::roundRect(q, -74, -48, 178, 3, 1.5f, MINT);
        text(body(), q, "sind", { 15, 0 }, 55, MINT, 0.5f, 0.5f);
    }
}

void drawEndCard(const Ctx& cx, int t) {
    const float k = ramp(t, 0, 12);
    if (k <= 0.001f) return;
    Ctx m = cx;
    m.o = mapP(cx, { W / 2, 1445 + (1 - k) * 22 });
    m.a *= k;
    dr::roundRect(m, -380, -56, 760, 112, 18, CREAM);
    text(sans(), m, "SIX SEATS. ONE VERB.", { 0, -12 }, 34, INK, 0.5f, 0.5f);
    text(mono(), m, "A1 · GRAMMAR · A1-GRA-01", { 0, 27 }, 20, INK, 0.5f, 0.5f);
}

void scene1(const Ctx& cx, int t) {
    if (t >= 116) drawTable(cx, ramp(t, 116, 126), 0);
    const float fade = 1.f - ramp(t, 120, 134);
    if (fade > 0.001f) {
        hookCard(cx, 90, 700, "ich", "bin", 0.9f + 0.1f * ramp(t, 0, 10), ramp(t, 15, 27), fade);
        hookCard(cx, 570, 700, "du", "bist", ramp(t, 42, 52), ramp(t, 59, 71), fade);
        hookLink(cx, ramp(t, 83, 95), fade);
    }
}

void scene2(const Ctx& cx, int t) {
    drawTable(cx, 1, ramp(t, 0, 12));
}

void scene3(const Ctx& cx, int t) {
    drawTable(cx, 1, 1);
    setting(cx, 0, 0, "ich", "bin", ramp(t, 0, 10), ramp(t, 9, 21), 1);
    setting(cx, 0, 1, "du", "bist", ramp(t, 25, 35), ramp(t, 34, 46), 1);
    setting(cx, 0, 2, "er/sie/es", "ist", ramp(t, 71, 81), ramp(t, 79, 91), 1);
}

void scene4(const Ctx& cx, int t) {
    drawTable(cx, 1, 1);
    setting(cx, 0, 0, "ich", "bin", 1, 1, 1);
    setting(cx, 0, 1, "du", "bist", 1, 1, 1);
    setting(cx, 0, 2, "er/sie/es", "ist", 1, 1, 1);
    setting(cx, 1, 0, "wir", "sind", ramp(t, 0, 10), ramp(t, 9, 21), 1);
    setting(cx, 1, 1, "ihr", "seid", ramp(t, 22, 32), ramp(t, 31, 43), 1);
    setting(cx, 1, 2, "sie / Sie", "sind", ramp(t, 48, 58), ramp(t, 57, 69), 1, true);
    sharedLink(cx, ramp(t, 218, 230), 1);
}

void scene5(const Ctx& cx, int t) {
    const float tableA = mixf(0.35f, 1.f, ramp(t, 198, 210));
    drawTable(cx, tableA, tableA);
    setting(cx, 0, 0, "ich", "bin", 1, 1, tableA);
    setting(cx, 0, 1, "du", "bist", 1, 1, tableA);
    setting(cx, 0, 2, "er/sie/es", "ist", 1, 1, tableA);
    setting(cx, 1, 0, "wir", "sind", 1, 1, tableA);
    setting(cx, 1, 1, "ihr", "seid", 1, 1, tableA);
    setting(cx, 1, 2, "sie / Sie", "sind", 1, 1, tableA, true);
    sharedLink(cx, 1, tableA);
    drawSentence(cx, t);
}

void scene6(const Ctx& cx, int t) {
    drawTable(cx, 1, 1);
    setting(cx, 0, 0, "ich", "bin", 1, 1, 1);
    setting(cx, 0, 1, "du", "bist", 1, 1, 1);
    setting(cx, 0, 2, "er/sie/es", "ist", 1, 1, 1);
    setting(cx, 1, 0, "wir", "sind", 1, 1, 1);
    setting(cx, 1, 1, "ihr", "seid", 1, 1, 1);
    setting(cx, 1, 2, "sie / Sie", "sind", 1, 1, 1, true);
    sharedLink(cx, 1, 1);
    drawEndCard(cx, t);
}

int sceneAt(int t) {
    int i = 0;
    while (i + 1 < 6 && t >= SCENE_START[i + 1]) i++;
    return i;
}

void backdrop(const Ctx& cx) {
    dr::rectGrad4(cx, 0, 0, W, H, mkColor(0x14241f), mkColor(0x0e1b17),
                  mkColor(0x08110e), mkColor(0x08110e));
    for (int j = 0; j < 24; j++) {
        for (int i = 0; i < 14; i++) {
            const float x = 34 + i * 78.f + (j % 2) * 19.f;
            const float y = 160 + j * 76.f;
            dr::circle(cx, { x, y }, 1.2f,
                       withAlpha(MUTED, 0.07f + 0.035f * (j % 3)));
        }
    }
}

void hud(const Ctx& cx, int t) {
    const int i = sceneAt(t);
    const Font m = mono();
    char page[16];
    std::snprintf(page, sizeof page, "%02d / 06", i + 1);
    dr::rect(cx, 72, 54, W - 144, 2, withAlpha(RULE, 0.9f));
    dr::rect(cx, 72, 54, (W - 144) * (t + 1.f) / TOTAL, 2, AMBER);
    text(m, cx, "A1 · GRAMMAR", { 72, 92 }, 21, AMBER, 0, 0.5f);
    text(m, cx, page, { W - 72, 92 }, 19, MUTED, 1, 0.5f);
    text(m, cx, "Standard German · A1 tag from supplied roadmap · language review pending",
         { 72, 1850 }, 15, withAlpha(MUTED, 0.85f), 0, 0.5f);
}

void caption(const Ctx& cx, int t) {
    const double s = t / (double)FPS;
    const Caption* c = nullptr;
    for (const Caption& x : CAPTIONS) {
        if (s >= x.a && s < x.b) { c = &x; break; }
    }
    if (!c) return;
    const float k = std::min({ 1.f, (float)((s - c->a) * FPS / 6.0),
                               (float)((c->b - s) * FPS / 6.0) });
    if (k <= 0.001f) return;
    Ctx m = cx;
    m.a *= k;
    dr::rectGrad4(m, 0, 1320, W, 430, withAlpha(BG, 0), withAlpha(BG, 0),
                  withAlpha(BG, 0.90f), withAlpha(BG, 0.90f));
    TextStyle st;
    st.font = body();
    st.size = 42;
    st.lineHeight = 1.22f;
    st.color = CREAM;
    st.align = Align::Center;
    st.valign = Align::Center;
    dr::textBox(m, c->text, { 100, 1530, 880, 180 }, st);
}

void sfx(Nodes& roots, int frame, const char* file, float volume) {
    TNode* n = addRoot(roots, frame, 30);
    media::AudioOptions o;
    o.volume = volume * 0.65f;
    media::audio(n, std::string("assets/sfx/") + file, o);
}

void ensureAssets() {
    std::filesystem::create_directories("out/a1-sein-assets");
    Song song;
    struct Chord { double bass, notes[4]; };
    const Chord chords[] = {
        { 110.00, { 220.00, 261.63, 329.63, 440.00 } },
        { 87.31, { 174.61, 220.00, 261.63, 349.23 } },
        { 130.81, { 196.00, 261.63, 329.63, 392.00 } },
        { 98.00, { 196.00, 246.94, 293.66, 392.00 } },
    };
    for (double t = 0; t < TOTAL / (double)FPS; t += 2.0) {
        const Chord& ch = chords[(int)(t / 2) % 4];
        song.push_back({ t, 2.1, ch.bass, 0.075, false });
        const int order[8] = { 0, 1, 2, 3, 2, 1, 0, 1 };
        for (int i = 0; i < 8; i++)
            song.push_back({ t + i * 0.25, 0.85, ch.notes[order[i]], 0.045, true });
    }
    const std::vector<float> mono = renderSong(song, TOTAL / (double)FPS, 48000);
    std::vector<float> stereo(mono.size() * 2);
    for (size_t i = 0; i < mono.size(); i++) {
        stereo[i * 2] = mono[i];
        stereo[i * 2 + 1] = mono[i];
    }
    media::writeWav("out/a1-sein-assets/bed.wav", stereo, 48000);

    sub::Track tr;
    for (const Caption& c : CAPTIONS)
        tr.cues.push_back({ c.a, c.b, c.text, false });
    sub::writeSrt("out/a1-sein-six-seats.srt", tr);
}

void buildFilm(Comp& c) {
    c.roots.clear();
    addRoot(c.roots, 0, TOTAL)->fn = [](const Ctx& cx, int) { backdrop(cx); };

    for (int i = 0; i < 6; i++) {
        TNode* n = addRoot(c.roots, SCENE_START[i], SCENE_END[i] - SCENE_START[i]);
        media::AudioOptions vo;
        vo.volume = 0.82f;
        media::audio(n, std::string("assets/vo/a1-sein/") + VO_FILES[i], vo);
        n->fn = [i](const Ctx& cx, int t) {
            switch (i) {
                case 0: scene1(cx, t); break;
                case 1: scene2(cx, t); break;
                case 2: scene3(cx, t); break;
                case 3: scene4(cx, t); break;
                case 4: scene5(cx, t); break;
                default: scene6(cx, t); break;
            }
        };
    }

    addRoot(c.roots, 0, TOTAL)->fn = [](const Ctx& cx, int t) {
        hud(cx, t);
        caption(cx, t);
    };

    if (std::filesystem::exists("out/a1-sein-assets/bed.wav")) {
        TNode* bed = addRoot(c.roots, 0, TOTAL);
        media::AudioOptions o;
        o.volume = 0.45f;
        o.volumeAt = [](int t) {
            float g = 1.f;
            for (int i = 0; i < 6; i++) {
                if (t >= SCENE_START[i] - 6 && t < SCENE_END[i] + 6) g = std::min(g, 0.42f);
            }
            return g;
        };
        media::audio(bed, "out/a1-sein-assets/bed.wav", o);
    }

    sfx(c.roots, 0, "pop.wav", 0.20f);
    sfx(c.roots, 15, "pop.wav", 0.16f);
    sfx(c.roots, 42, "pop.wav", 0.20f);
    sfx(c.roots, 59, "pop.wav", 0.16f);
    sfx(c.roots, 116, "thud.wav", 0.22f);
    sfx(c.roots, 167, "pop.wav", 0.18f);
    sfx(c.roots, 306, "pop.wav", 0.18f);
    sfx(c.roots, 331, "pop.wav", 0.16f);
    sfx(c.roots, 377, "pop.wav", 0.16f);
    sfx(c.roots, 513, "pop.wav", 0.18f);
    sfx(c.roots, 538, "pop.wav", 0.16f);
    sfx(c.roots, 562, "pop.wav", 0.16f);
    sfx(c.roots, 1004, "chime.wav", 0.24f);
    sfx(c.roots, 1038, "chime.wav", 0.26f);
}

} // namespace

void registerA1SeinSixSeats(Registry& out) {
    Comp c;
    c.name = "a1-sein-six-seats";
    c.w = W; c.h = H; c.fps = FPS;
    c.duration = TOTAL;
    c.background = BG;
    c.assets = [](const Comp&, const Json&) { ensureAssets(); };
    c.build = [](Comp& c, const Json&) { buildFilm(c); };
    out.push_back(std::move(c));
}

} // namespace rmtn









