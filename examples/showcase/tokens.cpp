// tokens.cpp -- "token-story": a ~40 s landscape short film explaining what a language
// model actually sees when it reads: sentence -> token pieces -> ids -> odds for the next
// piece. One continuous example ("The cat sat on the mat."), eight scenes, persistent
// illustrated objects, deterministic frame-driven animation, synthesised score.
//   raymotion preview token-story
//   raymotion render  token-story
#include "rmtn/raymotion.hpp"

namespace rmtn {

static unsigned hsh(unsigned x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }

static Comp makeTokenStory() {
    Comp c;
    c.name = "token-story"; c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = mkColor(0x0c1220);
    c.music = true;

    const float W = (float)c.w, H = (float)c.h;
    const Color BG = c.background, INK = mkColor(0xf2f4ff), ACCENT = mkColor(0x47dad8),
                PANEL = mkColor(0x1b2638), GOLDEN = mkColor(0xffc365), NOPE = mkColor(0xff5d5d),
                DIM = mkColor(0x8ea0b8);

    const Rectangle box = layout::inset(layout::fill(W, H), 110);

    // shared layout constants (art-local: origin at the illustration centre)
    const float CHIPX[5] = { -730.f, -365.f, 0.f, 365.f, 730.f };
    const float CHIPY = -70.f;
    const std::string WORDS[5] = { "The", "cat", "sat", "on", "the" };
    const int IDS[5] = { 464, 5882, 6377, 592, 262 };          // illustrative ids
    const std::string CANDS[3] = { "mat", "bed", "rug" };
    const float PROB_OLD[3] = { 0.58f, 0.21f, 0.13f };         // cat  -> mat most likely
    const float PROB_NEW[3] = { 0.22f, 0.49f, 0.09f };         // dog  -> bed most likely
    const float ROWY[3] = { -140.f, 0.f, 140.f };              // candidate rows
    const float BARX = 480.f, BARW = 290.f;                    // candidate bar tracks

    auto backdrop = [](Color col) { return [col](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1e5f, 1e5f, col); }; };

    // timing helpers (scene-local frames)
    auto popAt = [fps = c.fps](int f, int at) {
        SpringOptions o; o.fps = (double)fps; o.delay = at; o.config.damping = 11;
        return (float)spring((double)f, o);
    };
    auto slideAt = [](int f, int at, float dur = 14.f) {
        return (float)interpolate((double)f - at, { 0.0, (double)dur }, { 0.0, 1.0 },
                                  clamped(ease::out(ease::cubic)));
    };
    auto inAt = [](int f, int at, float dur = 14.f) {
        return (float)interpolate((double)f - at, { 0.0, (double)dur }, { 0.0, 1.0 },
                                  clamped(ease::in(ease::cubic)));
    };
    auto inOutAt = [](int f, int at, float dur = 14.f) {
        return (float)interpolate((double)f - at, { 0.0, (double)dur }, { 0.0, 1.0 },
                                  clamped(ease::inOut(ease::cubic)));
    };

    // chip: rounded token card with an accent edge; optional leading-space dot marker
    auto chipAt = [=](const Ctx& cx, const std::string& s, Vector2 pos, float k, Color col,
                      bool spaceDot, float size = 46.f, float wExtra = 64.f) {
        if (k <= 0.01f) return;
        Ctx m = cx; m.o = mapP(cx, pos); m.s *= std::min(1.f, k); m.a *= std::min(1.f, k);
        const float w = dr::measure(fonts::ui(false), s.c_str(), size, 0).x + wExtra;
        dr::roundRect(m, -w / 2, -44, w, 88, 18, PANEL);
        dr::roundRect(m, -w / 2, -44, 10, 88, 5, col);
        if (spaceDot) dr::circle(m, { -w / 2 + 24.f, 14.f }, 5.f, withAlpha(DIM, (200.f * std::min(1.f, k)) / 255.f));
        dr::text(fonts::ui(false), m, s.c_str(), { spaceDot ? 12.f : 0.f, 0.f }, size, 0, col, 0.5f, 0.5f);
    };

    // a machine panel + node layers (persisted identity across scenes 4, 6 and 7)
    auto drawMachine = [=](const Ctx& cx, int f, float alphaMul, float sc) {
        Ctx m = cx; m.o = mapP(cx, { -60, -40 }); m.s *= sc; m.a *= alphaMul;
        if (alphaMul <= 0.01f) return;
        dr::roundRect(m, -200, -150, 400, 300, 22, withAlpha(PANEL, 235.f / 255.f));
        dr::roundRect(m, -200, -150, 400, 8, 4, withAlpha(ACCENT, 200.f / 255.f));
        const float bh[3] = { 210.f, 250.f, 170.f };
        for (int i = 0; i < 3; i++) {
            const float bx = -70.f + i * 70.f;
            const float gl = 0.35f + 0.18f * std::sin(f * 0.09f + i);
            dr::roundRect(m, bx - 13, -bh[i] / 2, 26, bh[i], 12, withAlpha(DIM, gl * alphaMul * 255.f / 255.f));
            for (int j = 0; j < 4; j++) {
                const float dy = -bh[i] / 2 + (j + 0.5f) * (bh[i] / 4.f);
                Color pc = mixC(ACCENT, GOLDEN, 0.5f + 0.5f * std::sin(f * 0.12f + i * 1.7f + j * 0.9f));
                dr::circle(m, { bx, dy }, 6.f + 1.4f * std::sin(f * 0.2f + j),
                           withAlpha(pc, (210.f * alphaMul) / 255.f));
            }
        }
    };

    // candidate rows: label chips + odds bars (identical geometry in scenes 4 and 7)
    auto drawCandidates = [=](const Ctx& cx, const std::vector<float>& p, float chipK,
                              const std::vector<float>& barK, float alphaMul) {
        int goldIdx = 0;                                       // the golden bar is always the current max
        if (p[1] > p[goldIdx]) goldIdx = 1;
        if (p[2] > p[goldIdx]) goldIdx = 2;
        for (int i = 0; i < 3; i++) {
            Ctx cm = cx; cm.a *= alphaMul;
            chipAt(cm, CANDS[i], { BARX - 110, ROWY[i] }, chipK, INK, false, 38.f, 48.f);
            dr::roundRect(cm, BARX, ROWY[i] - 9, BARW, 18, 9, withAlpha(DIM, 70.f / 255.f));
            const float pNow = p[i] * std::min(1.f, barK[i]);
            if (pNow > 0.005f)
                dr::roundRect(cm, BARX, ROWY[i] - 9, BARW * pNow, 18, 9, i == goldIdx ? GOLDEN : ACCENT);
            if (barK[i] > 0.35f) {
                char buf[16]; snprintf(buf, sizeof buf, "%d%%", (int)std::lround(p[i] * 100.f));
                dr::text(fonts::ui(true), cm, buf, { BARX + BARW + 16, ROWY[i] }, 30, 0,
                         withAlpha(INK, (220.f * barK[i]) / 255.f), 0.f, 0.5f);
            }
        }
    };

    c.roots.clear();
    TransitionSeries ts(c.roots, W, H);

    // scene scaffold: kicker + one-line caption up top, illustration below
    int sceneNo = 0; const int totalScenes = 8;
    auto sceneCard = [&](const std::string& line, std::function<void(const Ctx&, int)> art, int dur) {
        TNode* s = ts.add(dur);
        sceneNo++;
        s->fn = backdrop(BG);
        TNode* t = s->add(0, s->dur);
        t->up = [](int f, NodeProps& pp) {
            const float k = (float)interpolate(f, { 0, 12 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
            pp.opacity = k;
            pp.pos.y = 36 * (1 - k);
        };
        t->fn = [line, art, idx = sceneNo, totalScenes, box, INK, ACCENT, DIM, W, H](const Ctx& cx, int f) {
            TextStyle kick; kick.font = fonts::ui(true); kick.size = 34; kick.color = ACCENT;
            kick.align = Align::Center;
            dr::textBox(cx, "WHAT A MODEL ACTUALLY SEES  \xc2\xb7  " + std::to_string(idx) + "/" + std::to_string(totalScenes),
                        { box.x, box.y + 4, box.width, 44 }, kick);
            TextStyle st; st.font = fonts::ui(true); st.size = 54; st.color = INK;
            st.lineHeight = 1.22f; st.align = Align::Center; st.valign = Align::Start;
            dr::textBox(cx, line, { box.x, box.y + 62, box.width, 170 }, st);
            Ctx m = cx; m.o.x += W / 2; m.o.y += H * 0.55f;   // art origin: illustration centre
            art(m, f);
        };
        return s;
    };

    const std::string SENT = "The cat sat on the";

    // ---- 1. hook: chat bubble types the sentence, gap blinks, "?" pops --------------
    sceneCard("Any chatbot finishes this sentence - how does it know?", [=](const Ctx& cx, int f) {
        const float bx = 0, by = -80;
        dr::roundRect(cx, bx - 490, by - 78, 980, 156, 26, PANEL);
        dr::roundRect(cx, bx - 490, by - 78, 980, 10, 5, withAlpha(DIM, 170.f / 255.f));
        const int n = (int)SENT.size();
        const int reveal = (int)std::min<double>((double)n, std::max(0.0, (double)n * easeFn(E_OUT_CUBIC, clamp01((f - 8) / 52.0))));
        const std::string sub = SENT.substr(0, reveal);
        const float tx = bx - 430;
        dr::text(fonts::ui(false), cx, sub.c_str(), { tx, by }, 58, 0, INK, 0.f, 0.5f);
        if (reveal >= n) {
            const float endX = tx + dr::measure(fonts::ui(false), SENT.c_str(), 58, 0).x;
            dr::roundRect(cx, endX + 8, by + 26, 150, 10, 5, GOLDEN);                    // the gap to fill
            if (f < 78 && f % 44 < 26)
                dr::rect(cx, endX + 166, by - 30, 6, 60, withAlpha(INK, 230.f / 255.f));  // blinking cursor
            const float q = popAt(f, 78);
            if (q > 0.01f) {
                Ctx m = cx; m.o = mapP(cx, { endX + 83, by - 108 }); m.s *= q; m.a *= std::min(1.f, q);
                dr::text(fonts::ui(true), m, "?", { 0, 0 }, 96, 0, GOLDEN, 0.5f, 0.5f);
            }
        }
    }, 120);
    ts.transition(Transition::slide(16, Direction::FromRight));

    // ---- 2. chop into tokens: cut lines drop, words separate into chips --------------
    sceneCard("First it chops the sentence into pieces - tokens.", [=](const Ctx& cx, int f) {
        Font F = fonts::ui(false);
        float wd[5], packed[5], rightEdge[5];
        const float g = 46.f;                                   // visible gap between the words
        float total = 0;
        for (int i = 0; i < 5; i++) { wd[i] = dr::measure(F, WORDS[i].c_str(), 52, 0).x; total += wd[i] + (i ? g : 0.f); }
        float cur = -total / 2;
        for (int i = 0; i < 5; i++) { if (i) cur += g; packed[i] = cur + wd[i] / 2; rightEdge[i] = cur + wd[i]; cur += wd[i]; }
        const float sep = slideAt(f, 36, 26);
        // chip frames fade in under the gliding words (dots mark glued spaces on all but the first)
        for (int i = 0; i < 5; i++) {
            if (sep <= 0.02f) continue;
            const float w = wd[i] + 72;
            Ctx m = cx; m.o = mapP(cx, { CHIPX[i], CHIPY }); m.a *= sep;
            dr::roundRect(m, -w / 2, -46, w, 92, 18, PANEL);
            dr::roundRect(m, -w / 2, -46, 10, 92, 5, ACCENT);
            if (i > 0) dr::circle(m, { -w / 2 + 22.f, 16.f }, 5.f, withAlpha(DIM, 200.f / 255.f));
        }
        // exactly one copy of each word, gliding from its packed slot to its chip
        for (int i = 0; i < 5; i++) {
            const Vector2 p{ packed[i] + (CHIPX[i] + (i ? 14 : 0) - packed[i]) * sep, CHIPY };
            dr::text(F, cx, WORDS[i].c_str(), p, 52, 0, INK, 0.5f, 0.5f);
        }
        // red cut lines drop in the gaps before the separation
        const float cutA = 1 - sep;
        if (cutA > 0.02f)
            for (int i = 0; i < 4; i++) {
                const float k = slideAt(f, 6 + i * 8, 18);
                if (k <= 0.01f) continue;
                const float mx = rightEdge[i] + g / 2;          // boundary between words
                dr::strokeSeg(cx, { mx, CHIPY - 96 }, { mx, CHIPY - 96 + 196 * k }, 6,
                              withAlpha(NOPE, cutA * 230.f / 255.f));
            }
        dr::text(fonts::ui(false), cx, "the dots mark spaces glued inside a piece", { 0, CHIPY + 130 }, 30, 0,
                 withAlpha(DIM, (180.f * slideAt(f, 74)) / 255.f), 0.5f, 0.5f);
    }, 130);
    ts.transition(Transition::fade(12));

    // ---- 3. flip ids -----------------------------------------------------------------
    sceneCard("Each piece flips into a number from its vocabulary.", [=](const Ctx& cx, int f) {
        Font F = fonts::ui(false);
        for (int i = 0; i < 5; i++) {
            const float w = dr::measure(F, WORDS[i].c_str(), 52, 0).x + 72;
            dr::roundRect(cx, CHIPX[i] - w / 2, CHIPY - 46, w, 92, 18, PANEL);
            dr::roundRect(cx, CHIPX[i] - w / 2, CHIPY - 46, 10, 92, 5, ACCENT);
            if (i > 0) dr::circle(cx, { CHIPX[i] - w / 2 + 22, CHIPY + 16 }, 5.f, withAlpha(DIM, 200.f / 255.f));
            const float outK = 1.f - slideAt(f, 12 + i * 9, 10);
            const float inK = slideAt(f, 21 + i * 9, 12);
            if (outK > 0.03f) {                                  // word squashes out
                Ctx m = cx; m.o = mapP(cx, { CHIPX[i] + (i ? 14 : 0), CHIPY }); m.s *= outK; m.a *= outK;
                dr::text(F, m, WORDS[i].c_str(), { 0, 0 }, 52, 0, INK, 0.5f, 0.5f);
            }
            if (inK > 0.03f) {                                   // id springs in
                char buf[12]; snprintf(buf, sizeof buf, "%d", IDS[i]);
                Ctx m = cx; m.o = mapP(cx, { CHIPX[i], CHIPY }); m.s *= inK; m.a *= inK;
                dr::text(fonts::ui(true), m, buf, { 0, 0 }, 44, 0, GOLDEN, 0.5f, 0.5f);
            }
            if (inK > 0.35f) {                                   // small token label under the chip
                dr::text(F, cx, WORDS[i].c_str(), { CHIPX[i], CHIPY + 78 }, 28, 0,
                         withAlpha(DIM, (190.f * std::min(1.f, inK)) / 255.f), 0.5f, 0.5f);
            }
        }
        dr::text(fonts::ui(false), cx, "ids are index names in its vocabulary - illustrative here", { 0, CHIPY + 150 }, 28, 0,
                 withAlpha(DIM, (170.f * slideAt(f, 78)) / 255.f), 0.5f, 0.5f);
    }, 140);
    ts.transition(Transition::slide(16, Direction::FromLeft));

    // ---- 4. the machine: numbers in, odds of the next piece out ----------------------
    sceneCard("Then it answers one question: what usually comes next?", [=](const Ctx& cx, int f) {
        // numbered chips stream into the intake, staggered
        for (int i = 0; i < 5; i++) {
            const float t = inAt(f, 12 + i * 11, 34);
            if (t <= 0.0f || t >= 1.f) continue;
            const Vector2 a{ CHIPX[i] * 0.62f, -265 }, b{ -190, -60 };
            const Vector2 p{ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
            Ctx m = cx; m.o = mapP(cx, p);
            const float sc = 0.62f + (0.15f - 0.62f) * t; m.s *= sc; m.a *= (1.f - inAt(f, 12 + i * 11 + 22, 12));
            char buf[12]; snprintf(buf, sizeof buf, "%d", IDS[i]);
            dr::roundRect(m, -46, -30, 92, 60, 12, PANEL);
            dr::text(fonts::ui(true), m, buf, { 0, 0 }, 28, 0, GOLDEN, 0.5f, 0.5f);
        }
        drawMachine(cx, f, 1.f, 1.f);
        dr::text(fonts::ui(true), cx, "what comes next?", { BARX + BARW / 2, -235 }, 34, 0,
                 withAlpha(DIM, (210.f * slideAt(f, 100)) / 255.f), 0.5f, 0.5f);
        const float ck = slideAt(f, 104, 18);
        std::vector<float> bars = { slideAt(f, 124, 30), slideAt(f, 132, 30), slideAt(f, 140, 30) };
        drawCandidates(cx, { PROB_OLD[0], PROB_OLD[1], PROB_OLD[2] }, ck, bars, 1.f);
        // verdict tag under the winning row
        const float v = popAt(f, 162);
        if (v > 0.01f) {
            Ctx m = cx; m.o = mapP(cx, { BARX + 20, ROWY[0] + 62 }); m.s *= v; m.a *= std::min(1.f, v);
            dr::text(fonts::ui(true), m, "most likely", { 0, 0 }, 28, 0, GOLDEN, 0.5f, 0.5f);
        }
        dr::text(fonts::ui(false), cx, "one pass = one prediction \xc2\xb7 odds illustrative", { BARX + BARW / 2, 320 }, 26, 0,
                 withAlpha(DIM, (160.f * slideAt(f, 150)) / 255.f), 0.5f, 0.5f);
    }, 190);
    ts.transition(Transition::fade(12));

    // ---- 5. append the winner ---------------------------------------------------------
    sceneCard("It appends the winner - and asks the same question again.", [=](const Ctx& cx, int f) {
        drawMachine(cx, f, 0.35f, 1.f);                          // dimmed but persistent world
        Font F = fonts::ui(false);
        // ghost of the candidate rows: plain labels + bars at low alpha
        for (int i = 0; i < 3; i++) {
            dr::text(F, cx, CANDS[i].c_str(), { BARX - 110, ROWY[i] }, 30, 0, withAlpha(INK, 90.f / 255.f), 0.5f, 0.5f);
            dr::roundRect(cx, BARX, ROWY[i] - 7, BARW, 14, 7, withAlpha(DIM, 45.f / 255.f));
            dr::roundRect(cx, BARX, ROWY[i] - 7, BARW * PROB_OLD[i], 14, 7,
                          i == 0 ? withAlpha(GOLDEN, 100.f / 255.f) : withAlpha(ACCENT, 80.f / 255.f));
        }
        // chat bubble returns; the sentence stays left-aligned so nothing jumps when the word lands
        const float by = -215, tx = -520, fsz = 44;
        dr::roundRect(cx, -580, by - 62, 1160, 124, 24, PANEL);
        dr::text(F, cx, SENT.c_str(), { tx, by }, fsz, 0, INK, 0.f, 0.5f);
        const float endX = tx + dr::measure(F, SENT.c_str(), fsz, 0).x;
        const float gapLeft = endX + 10, gapLen = 150 * (1.f - inOutAt(f, 52, 12));   // closes as the piece lands
        if (gapLen > 2) dr::roundRect(cx, gapLeft, by + 22, gapLen, 9, 4, GOLDEN);
        // the winning chip travels an arc from its bar row into the gap
        const float k = inOutAt(f, 10, 44);
        if (k > 0.0f && k < 1.f) {
            const Vector2 a{ BARX - 110, ROWY[0] }, b{ gapLeft + 75, by };
            const Vector2 ctrl{ BARX, by + 95 };
            const Vector2 p{ a.x * (1 - k) * (1 - k) + 2 * ctrl.x * (1 - k) * k + b.x * k * k,
                             a.y * (1 - k) * (1 - k) + 2 * ctrl.y * (1 - k) * k + b.y * k * k };
            Ctx m = cx; m.o = mapP(cx, p); m.s *= 1.0f - 0.15f * k;
            dr::roundRect(m, -50, -34, 100, 68, 14, PANEL);
            dr::text(fonts::ui(true), m, "mat", { 0, 0 }, 34, 0, GOLDEN, 0.5f, 0.5f);
        }
        // settled word + period + sparkles + badge
        const float land = inOutAt(f, 54, 10);
        if (land > 0.5f) {
            const float matX = endX + 18, sk = popAt(f, 52);
            Ctx m = cx; m.o = mapP(cx, { matX, by }); m.s *= std::min(1.f, sk); m.a *= std::min(1.f, sk);
            dr::text(F, m, "mat", { 0, 0 }, fsz, 0, GOLDEN, 0.f, 0.5f);
            const float dotAt = matX + dr::measure(F, "mat", fsz, 0).x + 12;
            const float dot = popAt(f, 88);
            if (dot > 0.01f) { Ctx n = cx; n.o = mapP(cx, { dotAt, by }); n.s *= dot; n.a *= std::min(1.f, dot);
                dr::circle(n, { 0, 15 }, 7.f, GOLDEN); }
            for (int s = 0; s < 5; s++) {
                const unsigned h = hsh(s * 977u + 7u);
                const float ang = (float)(h % 360u) * PI / 180.f;
                const float r = 62.f + (float)((h >> 8) % 40u);
                const Vector2 sp{ dotAt + r * std::cos(ang), by + r * std::sin(ang) };
                dr::star(cx, sp, 10, 4, 4, (float)f * 4 + s * 20,
                         withAlpha(GOLDEN, (200.f * slideAt(f, 56 + s * 5) * (1 - slideAt(f, 86, 18))) / 255.f));
            }
            chipAt(cx, "+1 piece", { BARX - 110, -292 }, popAt(f, 80), GOLDEN, false, 30, 44);
        }
    }, 140);
    ts.transition(Transition::fade(14));

    // ---- 6. loop: ask again, the chain grows ------------------------------------------
    sceneCard("Piece by piece, that becomes a whole paragraph.", [=](const Ctx& cx, int f) {
        // finished sentence bubble up left
        const float bx = -160, by = -270;
        dr::roundRect(cx, bx - 400, by - 52, 800, 104, 22, PANEL);
        dr::text(fonts::ui(false), cx, "The cat sat on the mat.", { bx, by }, 40, 0, INK, 0.5f, 0.5f);
        // machine stays where it was (persistent position, shifted origin -> local offset)
        drawMachine(cx, f, 1.f, 1.f);
        // loop arrow: bubble end -> around the right -> into the machine top
        const float a = inOutAt(f, 14, 44);
        const Vector2 A{ bx + 402, by }, Ctrl{ 720, -120 }, B{ 120, -184 };
        const int segs = (int)(a * 40);
        for (int i = 0; i < segs; i++) {
            const float u0 = i / 40.f, u1 = (i + 1) / 40.f;
            const auto bez = [&](float u) { return Vector2{ A.x * (1 - u) * (1 - u) + 2 * Ctrl.x * (1 - u) * u + B.x * u * u,
                                                            A.y * (1 - u) * (1 - u) + 2 * Ctrl.y * (1 - u) * u + B.y * u * u }; };
            dr::strokeSeg(cx, bez(u0), bez(u1), 7, withAlpha(ACCENT, 220.f / 255.f));
        }
        if (a > 0.95f) {
            const Vector2 dir{ -0.99f, -0.11f };   // tangent arriving down-left at B
            const Vector2 head{ B.x, B.y };
            const Vector2 p1{ head.x + 34 * dir.x - 20 * dir.y, head.y + 34 * dir.y + 20 * dir.x };
            const Vector2 p2{ head.x + 34 * dir.x + 20 * dir.y, head.y + 34 * dir.y - 20 * dir.x };
            const Vector2 pts[3] = { head, p1, p2 };
            dr::fillPoly(cx, pts, 3, ACCENT);
            dr::text(fonts::ui(true), cx, "ask again", { 560, -212 }, 30, 0, withAlpha(DIM, 210.f / 255.f), 0.5f, 0.5f);
        }
        // new pieces land on a growing chain
        const char* more[4] = { "It", "slept", "for", "hours" };
        for (int i = 0; i < 4; i++)
            chipAt(cx, more[i], { -360.f + i * 240.f, 215.f }, popAt(f, 74 + i * 18), ACCENT, i == 0, 42, 52);
        dr::text(fonts::ui(false), cx, "it stops when the likely next piece signals the end", { 0, 330 }, 28, 0,
                 withAlpha(DIM, (180.f * slideAt(f, 150)) / 255.f), 0.5f, 0.5f);
    }, 170);
    ts.transition(Transition::slide(16, Direction::FromRight));

    // ---- 7. swap cat -> dog: the same candidate list rebalances ----------------------
    sceneCard("Swap one piece, and every guess changes.", [=](const Ctx& cx, int f) {
        // input row above the machine: persistent chips, cat slides out, dog pops in
        for (int i = 0; i < 5; i++) {
            if (i == 1) continue;                       // cat handled below
            chipAt(cx, WORDS[i], { CHIPX[i] * 0.55f, -290.f }, 1.f, ACCENT, i > 0, 34, 44);
        }
        const float outK = slideAt(f, 8, 16), inK = popAt(f, 30);
        if (outK < 1.f) {                               // cat exits, rotating away
            Ctx m = cx; m.o = mapP(cx, { CHIPX[1] * 0.55f - 160 * outK, -290 + 60 * outK });
            m.rot += 26 * outK; m.a *= (1 - outK);
            dr::roundRect(m, -40, -32, 80, 64, 14, PANEL);
            dr::text(fonts::ui(false), m, "cat", { 0, 0 }, 34, 0, INK, 0.5f, 0.5f);
        }
        if (inK > 0.01f) {                              // dog arrives with a flash ring
            Ctx m = cx; m.o = mapP(cx, { CHIPX[1] * 0.55f, -290 }); m.s *= std::min(1.f, inK); m.a *= std::min(1.f, inK);
            dr::roundRect(m, -40, -32, 80, 64, 14, PANEL);
            dr::text(fonts::ui(true), m, "dog", { 0, 0 }, 34, 0, GOLDEN, 0.5f, 0.5f);
            const float ringA = 1 - slideAt(f, 32, 22);
            if (ringA > 0.02f) dr::ring(m, { 0, 0 }, 52 + 26 * (1 - ringA), 6, withAlpha(GOLDEN, (220.f * ringA) / 255.f));
        }
        dr::text(fonts::ui(true), cx, "new piece", { CHIPX[1] * 0.55f, -222 }, 26, 0,
                 withAlpha(GOLDEN, (220.f * slideAt(f, 40)) / 255.f), 0.5f, 0.5f);
        drawMachine(cx, f, 1.f, 1.f);
        // the very same candidate rows from scene 4: hold old odds, then morph live
        const float mK = inOutAt(f, 58, 36);
        std::vector<float> pNow(3), barK(3, 1.f);
        for (int i = 0; i < 3; i++) pNow[i] = PROB_OLD[i] + (PROB_NEW[i] - PROB_OLD[i]) * mK;
        drawCandidates(cx, pNow, 1.f, barK, 1.f);
        // verdict tag glides from the mat row to the bed row
        const Vector2 vt{ BARX + 20, ROWY[0] + 62 + (ROWY[1] - ROWY[0]) * mK };
        dr::text(fonts::ui(true), cx, "most likely", vt, 28, 0, GOLDEN, 0.5f, 0.5f);
        dr::text(fonts::ui(false), cx, "the odds follow whichever pieces went in", { BARX + BARW / 2, 320 }, 26, 0,
                 withAlpha(DIM, (160.f * slideAt(f, 130)) / 255.f), 0.5f, 0.5f);
    }, 190);
    ts.transition(Transition::fade(14));

    // ---- 8. payoff: mosaic of millions of these little predictions --------------------
    sceneCard("No reading, no thinking - just very good guesses about what comes next.", [=](const Ctx& cx, int f) {
        // background field: 7x3 mini panels, each with three hashed odds bars
        for (int r = 0; r < 3; r++) for (int col = 0; col < 7; col++) {
            const int idx = r * 7 + col;
            const float a = slideAt(f, 4 + idx * 4, 14) * 0.5f;
            if (a <= 0.02f) continue;
            const Vector2 pp{ -810.f + col * 270.f, -160.f + r * 190.f };
            Ctx m = cx; m.o = mapP(cx, pp); m.a *= a;
            dr::roundRect(m, -120, -75, 240, 150, 16, mkColor(0x141c2e));
            for (int b = 0; b < 3; b++) {
                const unsigned h = hsh(idx * 131u + b * 7919u + 3u);
                const float pw = 0.25f + (float)(h % 100u) / 133.f + 0.06f * std::sin(f * 0.06f + idx + b);
                dr::roundRect(m, -100, -34 + b * 34, 200 * std::min(1.f, pw), 14, 7,
                              (h & 1u) ? withAlpha(ACCENT, 210.f / 255.f) : withAlpha(GOLDEN, 210.f / 255.f));
            }
        }
        // centre card: the pipeline restated + credits
        const float card = popAt(f, 52);
        if (card > 0.01f) {
            Ctx m = cx; m.o = mapP(cx, { 0, -40 }); m.s *= std::min(1.12f, card); m.a *= std::min(1.f, card);
            dr::roundRect(m, -605, -200, 1210, 400, 30, mkColor(0x111827, 242));
            dr::roundRect(m, -605, -200, 1210, 10, 5, withAlpha(ACCENT, 210.f / 255.f));
            dr::text(fonts::ui(true), m, "The cat sat on the mat.", { 0, -128 }, 64, 0, INK, 0.5f, 0.5f);
            const float fl = slideAt(f, 76, 16);
            if (fl > 0.01f) {
                dr::text(fonts::ui(true), m, "sentence \xe2\x86\x92 pieces \xe2\x86\x92 numbers \xe2\x86\x92 what usually comes next",
                         { 0, -46 }, 34, 0, ACCENT, 0.5f, 0.5f);
                char c1[128]; snprintf(c1, sizeof c1, "made with raymotion \xc2\xb7 C++ / raylib 6.2 / ffmpeg");
                dr::text(fonts::ui(false), m, c1, { 0, 40 }, 30, 0, withAlpha(DIM, (210.f * fl) / 255.f), 0.5f, 0.5f);
                dr::text(fonts::ui(false), m, "every pixel and every note drawn from code", { 0, 110 }, 26, 0,
                         withAlpha(DIM, (150.f * slideAt(f, 96)) / 255.f), 0.5f, 0.5f);
            }
        }
    }, 190);

    // ---- outro ------------------------------------------------------------------------
    ts.transition(Transition::fade(18));
    TNode* o = ts.add(46);
    o->fn = [=](const Ctx& cx, int f) {
        dr::rect(cx, 0, 0, 1e5f, 1e5f, BG);
        const float k = (float)interpolate(f, { 8, 24 }, { 0, 1 }, clamped(ease::out(ease::cubic))) *
                        (1.f - (float)interpolate(f, { 38, 46 }, { 0, 1 }, clamped()));
        if (k <= 0.01f) return;
        dr::text(fonts::ui(true), cx, "raymotion", { W / 2, H * 0.48f }, 92, 0, withAlpha(INK, (255.f * k) / 255.f), 0.5f, 0.5f);
        dr::text(fonts::ui(false), cx, "motion graphics, generated entirely by code", { W / 2, H * 0.56f }, 30, 0,
                 withAlpha(ACCENT, (230.f * k) / 255.f), 0.5f, 0.5f);
    };

    c.duration = ts.end();

    // score: Am-F-C-G arpeggio, kick on chord changes, brighter arp during the finale
    {
        struct Chord { double bass, arp[4]; };
        static const Chord chords[] = {
            { 110.00, { 220.00, 261.63, 329.63, 440.00 } },   // A minor
            { 87.31, { 174.61, 220.00, 261.63, 349.23 } },    // F major
            { 130.81, { 196.00, 261.63, 329.63, 392.00 } },   // C major
            { 98.00, { 196.00, 246.94, 293.66, 392.00 } },    // G major
        };
        const double beat = 0.25;
        const double total = (double)c.duration / (double)c.fps;
        for (double t = 0; t < total; t += 2) {
            const Chord& ch = chords[(int)(t / 2) % 4];
            c.song.push_back({ t, 2.1, ch.bass, 0.09, false });
            const int order[8] = { 0, 1, 2, 3, 2, 1, 0, 1 };
            for (int i = 0; i < 8; i++) {
                const bool finale = t > total * 0.62;
                c.song.push_back({ t + i * beat, 0.9, ch.arp[order[i]] * (finale ? 2.0 : 1.0),
                                   finale ? 0.11 : 0.15, true });
            }
            c.song.push_back({ t, 0.16, 55.0, 0.28, true });                     // soft kick per chord
        }
        const double endT = total - 4.0;                                        // closing pad
        c.song.push_back({ endT, 4.5, 110.0, 0.13, false });
        c.song.push_back({ endT, 4.5, 164.81, 0.10, false });
        c.song.push_back({ endT, 4.5, 329.63, 0.07, false });
    }

    return c;
}

void registerTokenStory(Registry& out) { out.push_back(makeTokenStory()); }

} // namespace rmtn
