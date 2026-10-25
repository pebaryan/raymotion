// prefabs.cpp -- a library of reusable prefab assets and a "prefab-reel" composition
// that shows them off. Each prefab draws centred on its node's origin; the composition
// places and times every instance (`prefab::use`), and each one is parameterized by
// props merged over its defaults (see `raymotion prefabs`).
#include "rmtn/raymotion.hpp"

namespace rmtn {

namespace {

const Color INK = mkColor(0xf2f4ff), PANEL = mkColor(0x1b2638), BG = mkColor(0x0c1220),
            ACCENT = mkColor(0x47dad8), GOLDEN = mkColor(0xffc365);

Color colorProp(const Json& p, const char* key, Color fallback) { return p.color(key, fallback); }

} // namespace

// ------------------------------------------------------------------ prefabs

static void registerPrefabs() {
    // A full-screen title: accent bar wipes in, the title springs up, subtitle fades.
    prefab::add({
        "title-card",
        Json::parse(R"({"title": "raymotion", "subtitle": "", "accent": "#47dad8", "ink": "#f2f4ff", "fps": 30})"),
        [](TNode& n, const Json& p) {
            const std::string title = p.str("title", "raymotion"), sub = p.str("subtitle", "");
            const Color accent = colorProp(p, "accent", ACCENT), ink = colorProp(p, "ink", INK);
            const int fps = p.integer("fps", 30);
            n.up = [fps](int t, NodeProps& pr) {
                SpringOptions o;
                o.fps = fps; o.config.damping = 14;
                pr.scale = (float)spring(t, o);
            };
            n.fn = [title, sub, accent, ink, fps](const Ctx& c, int t) {
                const float bar = (float)interpolate(t, { 0, 12 }, { 0, 260 }, clamped(ease::out(ease::cubic)));
                dr::roundRect(c, -bar / 2, -150, bar, 14, 7, accent);
                const float o1 = (float)interpolate(t, { 8, 26 }, { 0, 1 }, clamped());
                dr::text(fonts::ui(true), c, title.c_str(), { 0, -10 }, 120, 1, ink, 0.5, 0.5);
                if (!sub.empty()) {
                    const float o2 = (float)interpolate(t, { 20, 38 }, { 0, 1 }, clamped());
                    dr::text(fonts::ui(false), c, sub.c_str(), { 0, 90 }, 44, 1,
                             withAlpha(accent, o2), 0.5, 0.5);
                }
            };
        },
    });

    // A name banner that slides in from the left: name over a role line, accent edge.
    prefab::add({
        "lower-third",
        Json::parse(R"({"name": "Name", "role": "", "w": 560, "h": 130, "accent": "#47dad8", "panel": "#1b2638", "ink": "#f2f4ff", "fps": 30})"),
        [](TNode& n, const Json& p) {
            const std::string name = p.str("name", "Name"), role = p.str("role", "");
            const float w = (float)p.num("w", 560), h = (float)p.num("h", 130);
            const Color accent = colorProp(p, "accent", ACCENT), panel = colorProp(p, "panel", PANEL),
                        ink = colorProp(p, "ink", INK);
            n.fn = [name, role, w, h, accent, panel, ink](const Ctx& c, int t) {
                // Slide the card in locally: the instance node's pos belongs to the caller.
                const float k = (float)interpolate(t, { 0, 14 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
                const float fade = (float)interpolate(t, { 0, 8 }, { 0, 1 }, clamped());
                const float off = -60 * (1 - k);
                const double th = c.rot * (PI / 180.0);
                Ctx m = c;                               // shift the origin along local x
                m.o.x += (float)(off * c.s * std::cos(th));
                m.o.y += (float)(off * c.s * std::sin(th));
                dr::roundRect(m, -w / 2, -h / 2, w, h, 18, panel);
                dr::rect(m, -w / 2, -h / 2, 10, h, accent);
                dr::text(fonts::ui(true), m, name.c_str(), { -w / 2 + 36, role.empty() ? 0.f : -18.f },
                         52, 1, withAlpha(ink, fade), 0.0, 0.5);
                if (!role.empty())
                    dr::text(fonts::ui(false), m, role.c_str(), { -w / 2 + 36, 32 }, 30, 1,
                             withAlpha(accent, fade), 0.0, 0.5);
            };
        },
    });

    // A marquee band: the text scrolls left through a masked panel, looping seamlessly.
    prefab::add({
        "ticker",
        Json::parse(R"({"text": "breaking news  ", "w": 1100, "h": 96, "speed": 3, "accent": "#47dad8", "panel": "#1b2638", "ink": "#f2f4ff"})"),
        [](TNode& n, const Json& p) {
            const std::string text = p.str("text", "breaking news  ");
            const float w = (float)p.num("w", 1100), h = (float)p.num("h", 96);
            const float speed = (float)p.num("speed", 3);
            const Color accent = colorProp(p, "accent", ACCENT), panel = colorProp(p, "panel", PANEL),
                        ink = colorProp(p, "ink", INK);
            Layer& clip = n.isolate();
            clip.mask = [w, h](const Ctx& c, int) { dr::rect(c, -w / 2, -h / 2, w, h, WHITE); };
            n.fn = [text, w, h, speed, accent, panel, ink](const Ctx& c, int t) {
                dr::rect(c, -w / 2, -h / 2, w, h, panel);
                dr::rect(c, -w / 2, -h / 2, w, 6, accent);
                Font f = fonts::ui(true);
                const float unit = dr::measure(f, text.c_str(), 52, 1).x + 90;
                float x = -w / 2 - std::fmod(t * speed, unit);
                for (; x < w / 2; x += unit)
                    dr::text(f, c, text.c_str(), { x, 0 }, 52, 1, ink, 0.0, 0.5);
            };
        },
    });

    // A spinning ray burst with a star at its heart.
    prefab::add({
        "starburst",
        Json::parse(R"({"rays": 14, "spin": 0.4, "accent": "#47dad8", "gold": "#ffc365"})"),
        [](TNode& n, const Json& p) {
            const int rays = std::max(3, p.integer("rays", 14));
            const float spin = (float)p.num("spin", 0.4);
            const Color accent = colorProp(p, "accent", ACCENT), gold = colorProp(p, "gold", GOLDEN);
            n.fn = [rays, spin, accent, gold](const Ctx& c, int t) {
                const float rot = t * spin;
                for (int i = 0; i < rays; i++) {
                    const float a = rot + 360.f * i / rays;
                    const float r = (float)std::cos((a - rot) * 3) * 12;
                    dr::strokeSeg(c, { std::cos(a * 3.14159f / 180) * (60 + r), std::sin(a * 3.14159f / 180) * (60 + r) },
                                  { std::cos(a * 3.14159f / 180) * (150 - r), std::sin(a * 3.14159f / 180) * (150 - r) },
                                  7, i % 2 ? gold : accent);
                }
                dr::star(c, { 0, 0 }, 46, 20, 5, rot * 2, accent);
            };
        },
    });

    // A countdown: a ring depleting over the node's length around the seconds left.
    prefab::add({
        "countdown",
        Json::parse(R"({"label": "", "accent": "#47dad8", "ink": "#f2f4ff", "fps": 30})"),
        [](TNode& n, const Json& p) {
            const std::string label = p.str("label", "");
            const Color accent = colorProp(p, "accent", ACCENT), ink = colorProp(p, "ink", INK);
            const int fps = p.integer("fps", 30);
            const int total = std::max(1, n.dur);
            n.fn = [label, accent, ink, fps, total](const Ctx& c, int t) {
                const int left = std::max(0, (int)std::ceil((total - t) / (double)fps));
                dr::ring(c, { 0, 0 }, 120, 10, withAlpha(ink, 0.16f));
                dr::arc(c, { 0, 0 }, 120, 10, -90, -90 + 360.f * (total - t) / total, accent);
                dr::text(fonts::ui(true), c, std::to_string(left).c_str(), { 0, label.empty() ? 0.f : -20.f },
                         110, 1, ink, 0.5, 0.5);
                if (!label.empty())
                    dr::text(fonts::ui(false), c, label.c_str(), { 0, 60 }, 32, 1, accent, 0.5, 0.5);
            };
        },
    });

    // Deterministic confetti: seeded randomness, so every render is identical.
    prefab::add({
        "confetti",
        Json::parse(R"({"count": 70, "seed": 7, "h": 1080, "accent": "#47dad8", "gold": "#ffc365"})"),
        [](TNode& n, const Json& p) {
            const int count = std::max(1, p.integer("count", 70));
            const unsigned long long seed = (unsigned long long)p.integer("seed", 7);
            const float h = (float)p.num("h", 1080);
            const Color accent = colorProp(p, "accent", ACCENT), gold = colorProp(p, "gold", GOLDEN);
            n.fn = [count, seed, h, accent, gold](const Ctx& c, int t) {
                for (int i = 0; i < count; i++) {
                    const float x = (float)rrange(seed, i * 8 + 1, -960, 960) + 40 * (float)std::sin(t * 0.05 + i);
                    const float v = (float)rrange(seed, i * 8 + 2, 3, 10);
                    const float y = (float)std::fmod(t * v + (float)rrange(seed, i * 8 + 3, 0, h), h) - h / 2;
                    const float s = (float)rrange(seed, i * 8 + 4, 6, 14), a = t * (float)rrange(seed, i * 8 + 5, 1, 6);
                    const Color col = (i % 3 == 0) ? gold : accent;
                    const float ca = std::cos(a * 3.14159f / 180), sa = std::sin(a * 3.14159f / 180);
                    const Vector2 p0{ x + ca * s - sa * s / 2, y + sa * s + ca * s / 2 },
                        p1{ x - ca * s - sa * s / 2, y - sa * s + ca * s / 2 },
                        p2{ x - ca * s + sa * s / 2, y - sa * s - ca * s / 2 },
                        p3{ x + ca * s + sa * s / 2, y + sa * s - ca * s / 2 };
                    dr::quad(c, p0, p1, p2, p3, col);
                }
            };
        },
    });
}

// ------------------------------------------------------------------ the reel

// "prefab-reel": one scene per prefab, joined by fades. Built once at registration; it
// takes no props.
static Comp makeReel() {
    Comp c;
    c.name = "prefab-reel"; c.w = 1920; c.h = 1080; c.fps = 30;
    c.background = BG;
    const float W = (float)c.w, H = (float)c.h;

    TransitionSeries ts(c.roots, W, H);
    auto backdrop = [](Color col) { return [col](const Ctx& cx, int) { dr::rect(cx, 0, 0, 1e5f, 1e5f, col); }; };
    auto scene = [&](int dur, Color bg) {
        TNode* s = ts.add(dur);
        s->fn = backdrop(bg);
        return s;
    };

    TNode* s1 = scene(105, BG);
    TNode* conf = s1->add(0, 105);                         // confetti behind the card
    conf->props.pos = { W / 2, H / 2 };
    prefab::apply(conf, "confetti");
    TNode* card = s1->add(0, 105);
    card->props.pos = { W / 2, H / 2 };
    prefab::apply(card, "title-card", Json::parse(R"({"title": "Prefabs", "subtitle": "reusable building blocks"})"));

    ts.transition(Transition::fade(20));
    TNode* s2 = scene(110, PANEL);
    prefab::apply(s2, "lower-third", Json::parse(R"({"name": "Ada Lovelace", "role": "analytical engine dept"})"));
    s2->props.pos = { W / 2, H / 2 - 180 };
    TNode* tick = s2->add(20, 90);
    tick->props.pos = { 0, 480 };                          // below the banner, near the bottom
    prefab::apply(tick, "ticker", Json::parse(R"({"text": "composing video in pure C++   "})"));

    ts.transition(Transition::fade(20));
    TNode* s3 = scene(120, BG);
    s3->props.pos = { W / 2, H / 2 };
    prefab::apply(s3, "starburst");

    ts.transition(Transition::fade(20));
    TNode* s4 = scene(150, PANEL);
    s4->props.pos = { W / 2, H / 2 };
    prefab::apply(s4, "countdown", Json::parse(R"({"label": "until launch"})"));

    c.duration = ts.end();
    return c;
}

void registerPrefabs(Registry& out) {
    registerPrefabs();
    out.push_back(makeReel());
}

} // namespace rmtn
