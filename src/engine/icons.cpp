// icons.cpp -- social media icons (Simple Icons paths, CC0) and the raymotion logo, with
// their prefabs.
#include "rmtn/icons.hpp"
#include "rmtn/anim.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/fonts.hpp"
#include "rmtn/prefab.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace rmtn {
namespace icons {

namespace {

struct Brand {
    const char* name;
    const char* title;
    unsigned color;
    bool dark;              // too dark to show on a dark frame: Glyph uses Look::color
    const char* d;          // Simple Icons path data, 24 x 24
    int under;              // contour filled white beneath the Glyph look (-1: none)
    int badgeOnly;          // the contour alone on a badge, scaled to `badgeSize` (-1: the whole mark)
    float badgeSize;        // the badge mark's larger side, as a share of the badge
};

const Brand BRANDS[] = {
    { "x", "X", 0x000000, true,
      "M18.901 1.153h3.68l-8.04 9.19L24 22.846h-7.406l-5.8-7.584-6.638 7.584H.474l8.6-9.83L0 1.154h7.594l5.243 6.932Z"
      "M17.61 20.644h2.039L6.486 3.24H4.298Z",
      -1, -1, 0.5f },
    { "youtube", "YouTube", 0xFF0000, false,
      "M23.498 6.186a3.016 3.016 0 0 0-2.122-2.136C19.505 3.545 12 3.545 12 3.545s-7.505 0-9.377.505A3.017 3.017 0 0 0 "
      ".502 6.186C0 8.07 0 12 0 12s0 3.93.502 5.814a3.016 3.016 0 0 0 2.122 2.136c1.871.505 9.376.505 9.376.505s7.505 0 "
      "9.377-.505a3.015 3.015 0 0 0 2.122-2.136C24 15.93 24 12 24 12s0-3.93-.502-5.814zM9.545 15.568V8.432L15.818 12"
      "l-6.273 3.568z",
      1, 1, 0.36f },
    { "github", "GitHub", 0x181717, true,
      "M12 .297c-6.63 0-12 5.373-12 12 0 5.303 3.438 9.8 8.205 11.385.6.113.82-.258.82-.577 0-.285-.01-1.04-.015-2.04"
      "-3.338.724-4.042-1.61-4.042-1.61C4.422 18.07 3.633 17.7 3.633 17.7c-1.087-.744.084-.729.084-.729 1.205.084 1.838 "
      "1.236 1.838 1.236 1.07 1.835 2.809 1.305 3.495.998.108-.776.417-1.305.76-1.605-2.665-.3-5.466-1.332-5.466-5.93 "
      "0-1.31.465-2.38 1.235-3.22-.135-.303-.54-1.523.105-3.176 0 0 1.005-.322 3.3 1.23.96-.267 1.98-.399 3-.405 1.02.006 "
      "2.04.138 3 .405 2.28-1.552 3.285-1.23 3.285-1.23.645 1.653.24 2.873.12 3.176.765.84 1.23 1.91 1.23 3.22 0 4.61"
      "-2.805 5.625-5.475 5.92.42.36.81 1.096.81 2.22 0 1.606-.015 2.896-.015 3.286 0 .315.21.69.825.57C20.565 22.092 "
      "24 17.592 24 12.297c0-6.627-5.373-12-12-12",
      -1, -1, 0.6f },
    { "instagram", "Instagram", 0xE4405F, false,
      "M7.0301.084c-1.2768.0602-2.1487.264-2.911.5634-.7888.3075-1.4575.72-2.1228 1.3877-.6652.6677-1.075 1.3368-1.3802 "
      "2.127-.2954.7638-.4956 1.6365-.552 2.914-.0564 1.2775-.0689 1.6882-.0626 4.947.0062 3.2586.0206 3.6671.0825 "
      "4.9473.061 1.2765.264 2.1482.5635 2.9107.308.7889.72 1.4573 1.388 2.1228.6679.6655 1.3365 1.0743 2.1285 1.38.7632"
      ".295 1.6361.4961 2.9134.552 1.2773.056 1.6884.069 4.9462.0627 3.2578-.0062 3.668-.0207 4.9478-.0814 1.28-.0607 "
      "2.147-.2652 2.9098-.5633.7889-.3086 1.4578-.72 2.1228-1.3881.665-.6682 1.0745-1.3378 1.3795-2.1284.2957-.7632"
      ".4966-1.636.552-2.9124.056-1.2809.0692-1.6898.063-4.948-.0063-3.2583-.021-3.6668-.0817-4.9465-.0607-1.2797-.264"
      "-2.1487-.5633-2.9117-.3084-.7889-.72-1.4568-1.3876-2.1228C21.2982 1.33 20.628.9208 19.8378.6165 19.074.321 "
      "18.2017.1197 16.9244.0645 15.6471.0093 15.236-.005 11.977.0014 8.718.0076 8.31.0215 7.0301.0839m.1402 21.6932"
      "c-1.17-.0509-1.8053-.2453-2.2287-.408-.5606-.216-.96-.4771-1.3819-.895-.422-.4178-.6811-.8186-.9-1.378-.1644"
      "-.4234-.3624-1.058-.4171-2.228-.0595-1.2645-.072-1.6442-.079-4.848-.007-3.2037.0053-3.583.0607-4.848.05-1.169"
      ".2456-1.805.408-2.2282.216-.5613.4762-.96.895-1.3816.4188-.4217.8184-.6814 1.3783-.9003.423-.1651 1.0575-.3614 "
      "2.227-.4171 1.2655-.06 1.6447-.072 4.848-.079 3.2033-.007 3.5835.005 4.8495.0608 1.169.0508 1.8053.2445 2.228"
      ".408.5608.216.96.4754 1.3816.895.4217.4194.6816.8176.9005 1.3787.1653.4217.3617 1.056.4169 2.2263.0602 1.2655"
      ".0739 1.645.0796 4.848.0058 3.203-.0055 3.5834-.061 4.848-.051 1.17-.245 1.8055-.408 2.2294-.216.5604-.4763.96"
      "-.8954 1.3814-.419.4215-.8181.6811-1.3783.9-.4224.1649-1.0577.3617-2.2262.4174-1.2656.0595-1.6448.072-4.8493"
      ".079-3.2045.007-3.5825-.006-4.848-.0608M16.953 5.5864A1.44 1.44 0 1 0 18.39 4.144a1.44 1.44 0 0 0-1.437 1.4424"
      "M5.8385 12.012c.0067 3.4032 2.7706 6.1557 6.173 6.1493 3.4026-.0065 6.157-2.7701 6.1506-6.1733-.0065-3.4032"
      "-2.771-6.1565-6.174-6.1498-3.403.0067-6.156 2.771-6.1496 6.1738M8 12.0077a4 4 0 1 1 4.008 3.9921A3.9996 3.9996 "
      "0 0 1 8 12.0077",
      -1, -1, 0.58f },
    { "tiktok", "TikTok", 0x000000, true,
      "M12.525.02c1.31-.02 2.61-.01 3.91-.02.08 1.53.63 3.09 1.75 4.17 1.12 1.11 2.7 1.62 4.24 1.79v4.03c-1.44-.05"
      "-2.89-.35-4.2-.97-.57-.26-1.1-.59-1.62-.93-.01 2.92.01 5.84-.02 8.75-.08 1.4-.54 2.79-1.35 3.94-1.31 1.92-3.58 "
      "3.17-5.91 3.21-1.43.08-2.86-.31-4.08-1.03-2.02-1.19-3.44-3.37-3.65-5.71-.02-.5-.03-1-.01-1.49.18-1.9 1.12-3.72 "
      "2.58-4.96 1.66-1.44 3.98-2.13 6.15-1.72.02 1.48-.04 2.96-.04 4.44-.99-.32-2.15-.23-3.02.37-.63.41-1.11 1.04-1.36 "
      "1.75-.21.51-.15 1.07-.14 1.61.24 1.64 1.82 3.02 3.5 2.87 1.12-.01 2.19-.66 2.77-1.61.19-.33.4-.67.41-1.06.1-1.79"
      ".06-3.57.07-5.36.01-4.03-.01-8.05.02-12.07z",
      -1, -1, 0.54f },
    { "whatsapp", "WhatsApp", 0x25D366, false,
      "M17.472 14.382c-.297-.149-1.758-.867-2.03-.967-.273-.099-.471-.148-.67.15-.197.297-.767.966-.94 1.164-.173.199"
      "-.347.223-.644.075-.297-.15-1.255-.463-2.39-1.475-.883-.788-1.48-1.761-1.653-2.059-.173-.297-.018-.458.13-.606"
      ".134-.133.298-.347.446-.52.149-.174.198-.298.298-.497.099-.198.05-.371-.025-.52-.075-.149-.669-1.612-.916-2.207"
      "-.242-.579-.487-.5-.669-.51-.173-.008-.371-.01-.57-.01-.198 0-.52.074-.792.372-.272.297-1.04 1.016-1.04 2.479 0 "
      "1.462 1.065 2.875 1.213 3.074.149.198 2.096 3.2 5.077 4.487.709.306 1.262.489 1.694.625.712.227 1.36.195 1.871"
      ".118.571-.085 1.758-.719 2.006-1.413.248-.694.248-1.289.173-1.413-.074-.124-.272-.198-.57-.347m-5.421 7.403h"
      "-.004a9.87 9.87 0 01-5.031-1.378l-.361-.214-3.741.982.998-3.648-.235-.374a9.86 9.86 0 01-1.51-5.26c.001-5.45 "
      "4.436-9.884 9.888-9.884 2.64 0 5.122 1.03 6.988 2.898a9.825 9.825 0 012.893 6.994c-.003 5.45-4.437 9.884-9.885 "
      "9.884m8.413-18.297A11.815 11.815 0 0012.05 0C5.495 0 .16 5.335.157 11.892c0 2.096.547 4.142 1.588 5.945L.057 24"
      "l6.305-1.654a11.882 11.882 0 005.683 1.448h.005c6.554 0 11.89-5.335 11.893-11.893a11.821 11.821 0 00-3.48-8.413Z",
      -1, -1, 0.6f },
    { "telegram", "Telegram", 0x26A5E4, false,
      "M11.944 0A12 12 0 0 0 0 12a12 12 0 0 0 12 12 12 12 0 0 0 12-12A12 12 0 0 0 12 0a12 12 0 0 0-.056 0z"
      "m4.962 7.224c.1-.002.321.023.465.14a.506.506 0 0 1 .171.325c.016.093.036.306.02.472-.18 1.898-.962 6.502-1.36 "
      "8.627-.168.9-.499 1.201-.82 1.23-.696.065-1.225-.46-1.9-.902-1.056-.693-1.653-1.124-2.678-1.8-1.185-.78-.417"
      "-1.21.258-1.91.177-.184 3.247-2.977 3.307-3.23.007-.032.014-.15-.056-.212s-.174-.041-.249-.024c-.106.024-1.793 "
      "1.14-5.061 3.345-.48.33-.913.49-1.302.48-.428-.008-1.252-.241-1.865-.44-.752-.245-1.349-.374-1.297-.789.027-.216"
      ".325-.437.893-.663 3.498-1.524 5.83-2.529 6.998-3.014 3.332-1.386 4.025-1.627 4.476-1.635z",
      1, 1, 0.56f },
    { "twitch", "Twitch", 0x9146FF, false,
      "M4.285 0 2.143 5.714v15.429h5.714V24l2.857-2.857H15l7.857-7.857V0H4.285z"
      "m16.429 12.286-4.286 4.286h-4.286l-3.571 3.571v-3.571H4.286V2.143h16.428v10.143z"
      "M17.143 5.714H15v6.429h2.143V5.714zM11.429 5.714H9.286v6.429h2.143V5.714z",
      -1, -1, 0.6f },
};

struct Prepared {
    const Brand* brand;
    vg::Path glyph;         // centred on the origin
    vg::Path under;         // the contour shown white beneath the Glyph look
    vg::Path badgeMark;     // what a badge shows, centred and scaled to its 24 box
};

vg::Path contour(const vg::Path& p, int i) {
    vg::Path out;
    if (i >= 0 && i < (int)p.contours.size()) out.contours.push_back(p.contours[i]);
    return out;
}

const Prepared& prepared(const std::string& name) {
    static const std::map<std::string, Prepared> all = [] {
        std::map<std::string, Prepared> m;
        for (const Brand& b : BRANDS) {
            Prepared p;
            p.brand = &b;
            p.glyph = vg::Path::parse(b.d).translated({ -12, -12 });
            p.under = contour(p.glyph, b.under);
            if (b.badgeOnly < 0) {
                p.badgeMark = p.glyph;
            } else {
                // Recentre the lone contour on its bounds and scale its larger side to 24.
                vg::Path one = contour(p.glyph, b.badgeOnly);
                const Rectangle r = one.bounds();
                const float k = 24 / std::max(r.width, r.height);
                p.badgeMark = one.translated({ -(r.x + r.width / 2), -(r.y + r.height / 2) }).scaled(k, k);
            }
            m[b.name] = std::move(p);
        }
        return m;
    }();
    auto it = all.find(name);
    if (it == all.end()) throw std::runtime_error("icons: no brand named '" + name + "'");
    return it->second;
}

Vector2 polar(float deg, float r) { return { r * std::cos(deg * (float)PI / 180), r * std::sin(deg * (float)PI / 180) }; }

float clamp01(float v) { return std::clamp(v, 0.f, 1.f); }
float smooth(float v) { v = clamp01(v); return v * v * (3 - 2 * v); }

// A context whose units are `scale` px, centred at `centre`.
Ctx at(const Ctx& c, Vector2 centre, float scale) {
    Ctx k = c;
    k.o = mapP(c, centre);
    k.s *= scale;
    return k;
}

// The mark itself: filled, or written on while u < 1.
void mark(const Ctx& c, const vg::Path& p, Color col, float u) {
    if (u >= 1) { vg::fill(c, p, col, vg::FillRule::EvenOdd); return; }
    if (u <= 0) return;
    vg::Style s = vg::Style::filled(col);
    s.rule = vg::FillRule::EvenOdd;
    s.stroke = vg::Stroke(col, 0.45f);
    vg::write(c, p, s, u);
}

// TikTok's mark: cyan and red copies offset behind the main one.
void tiktok(const Ctx& c, const vg::Path& p, Color col, float u) {
    const float o = 0.55f;
    mark(c, p.translated({ -o, -o }), mkColor(0x25F4EE), u);
    mark(c, p.translated({ o, o }), mkColor(0xFE2C55), u);
    mark(c, p, col, u);
}

// Half the shape's width at height y (the shape is `size` across, centred).
float extent(Shape shape, float size, float corner, float y) {
    const float half = size / 2, ay = std::fabs(y);
    if (ay > half) return 0;
    if (shape == Shape::Square) return half;
    if (shape == Shape::Circle) return std::sqrt(half * half - ay * ay);
    const float r = std::min(half, corner * size);
    if (ay <= half - r) return half;
    const float dy = ay - (half - r);
    return half - r + std::sqrt(std::max(0.f, r * r - dy * dy));
}

// Instagram's gradient, yellow at the bottom left to violet at the top right.
Color instagramAt(float u) {
    static const Color stops[] = { mkColor(0xFEDA75), mkColor(0xFA7E1E), mkColor(0xD62976), mkColor(0x962FBF),
                                   mkColor(0x4F5BD5) };
    u = clamp01(u) * 4;
    const int i = std::min(3, (int)u);
    return mixC(stops[i], stops[i + 1], u - i);
}

// The badge shape filled with a diagonal gradient: rows that follow the shape's edges,
// each cut into cells whose corners carry the gradient's colours.
void gradientBadge(const Ctx& c, float size, Shape shape, float corner) {
    const int rows = 48, cols = 24;
    const float half = size / 2;
    auto colorAt = [&](Vector2 p) { return dr::TA(c, instagramAt(((p.x - p.y) / size + 1) / 2)); };
    for (int j = 0; j < rows; j++) {
        const float y0 = -half + size * j / rows, y1 = -half + size * (j + 1) / rows;
        const float e0 = extent(shape, size, corner, y0), e1 = extent(shape, size, corner, y1);
        for (int i = 0; i < cols; i++) {
            const float u0 = (float)i / cols, u1 = (float)(i + 1) / cols;
            const Vector2 p[4] = { { -e0 + 2 * e0 * u0, y0 }, { -e0 + 2 * e0 * u1, y0 }, { -e1 + 2 * e1 * u1, y1 },
                                   { -e1 + 2 * e1 * u0, y1 } };
            Vector2 s[4];
            Color k[4];
            for (int q = 0; q < 4; q++) { s[q] = mapP(c, p[q]); k[q] = colorAt(p[q]); }
            DrawTriangleGradient(s[0], s[2], s[1], k[0], k[2], k[1]);
            DrawTriangleGradient(s[0], s[3], s[2], k[0], k[3], k[2]);
        }
    }
}

vg::Path shapePath(Shape shape, float size, float corner) {
    const float h = size / 2;
    if (shape == Shape::Circle) return vg::Path::circle({ 0, 0 }, h);
    return vg::Path::rect({ -h, -h, size, size }, shape == Shape::Rounded ? corner * size : 0);
}

void solidBadge(const Ctx& c, float size, Shape shape, float corner, Color col) {
    const float h = size / 2;
    if (shape == Shape::Circle) dr::circle(c, { 0, 0 }, h, col);
    else if (shape == Shape::Square) dr::rect(c, -h, -h, size, size, col);
    else dr::roundRect(c, -h, -h, size, size, corner * size, col);
}

} // namespace

const std::vector<std::string>& brands() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> v;
        for (const Brand& b : BRANDS) v.push_back(b.name);
        return v;
    }();
    return names;
}

const vg::Path& glyph(const std::string& brand) { return prepared(brand).glyph; }
Color brandColor(const std::string& brand) { return mkColor(prepared(brand).brand->color); }
std::string title(const std::string& brand) { return prepared(brand).brand->title; }

Style parseStyle(const std::string& s) {
    if (s == "badge") return Style::Badge;
    if (s == "glyph") return Style::Glyph;
    if (s == "mono") return Style::Mono;
    if (s == "outline") return Style::Outline;
    throw std::runtime_error("icons: unknown style '" + s + "' (badge, glyph, mono or outline)");
}

Shape parseShape(const std::string& s) {
    if (s == "rounded") return Shape::Rounded;
    if (s == "circle") return Shape::Circle;
    if (s == "square") return Shape::Square;
    throw std::runtime_error("icons: unknown shape '" + s + "' (rounded, circle or square)");
}

void draw(const Ctx& c, const std::string& name, Vector2 centre, float size, const Look& look, float reveal) {
    const Prepared& p = prepared(name);
    const Brand& b = *p.brand;
    if (size <= 0 || reveal <= 0) return;
    // Drawing on: the badge grows over the first half, the mark writes itself from 0.2.
    const float grow = reveal >= 1 ? 1 : smooth(reveal * 2), u = reveal >= 1 ? 1 : clamp01((reveal - 0.2f) / 0.8f);
    const bool tik = std::string(b.name) == "tiktok";
    const Ctx box = at(c, centre, 1);
    if (look.style == Style::Glyph || look.style == Style::Mono) {
        const Ctx g = at(c, centre, size / 24);
        if (look.style == Style::Mono) { mark(g, p.glyph, look.color, u); return; }
        const Color col = b.dark ? look.color : mkColor(b.color);
        if (!p.under.empty()) mark(g, p.under, WHITE, u);
        if (tik) tiktok(g, p.glyph, col, u);
        else mark(g, p.glyph, col, u);
        return;
    }
    const float s = size * grow, inner = look.shape == Shape::Circle ? 0.92f : 1.f;
    if (look.style == Style::Outline) {
        vg::stroke(box, shapePath(look.shape, s * 0.96f, look.corner), vg::Stroke(look.color, size * 0.04f));
        mark(at(c, centre, size * 0.52f * inner / 24), p.glyph, look.color, u);
        return;
    }
    if (std::string(b.name) == "instagram") gradientBadge(box, s, look.shape, look.corner);
    else solidBadge(box, s, look.shape, look.corner, mkColor(b.color));
    const Ctx g = at(c, centre, size * b.badgeSize * inner / 24);
    if (tik) tiktok(g, p.badgeMark, look.color, u);
    else mark(g, p.badgeMark, look.color, u);
}

void logo(const Ctx& c, Vector2 centre, float size, double t, float reveal, bool badge, float spin) {
    if (size <= 0 || reveal <= 0) return;
    const Ctx k = at(c, centre, size / 400);       // the SVG's 400 x 400 units
    const Color teal = mkColor(0x47dad8), iris = mkColor(0x9b8cff), amber = mkColor(0xffc365), ink = mkColor(0xf2f4ff);
    if (badge) {
        const float g = reveal >= 1 ? 1 : smooth(reveal * 2.5f);
        dr::roundRect(k, -200 * g, -200 * g, 400 * g, 400 * g, 72 * g, mkColor(0x0f1528));
    }
    // The ring sweeps on clockwise from the top, its gradient running top left to bottom right.
    const float sweep = 360 * (reveal >= 1 ? 1 : smooth(reveal / 0.6f));
    const int segments = 72;
    for (int i = 0; i < segments; i++) {
        const float a0 = -90 + 360.f * i / segments, a1 = -90 + 360.f * (i + 1) / segments;
        if (a0 >= -90 + sweep) break;
        const float mid = (a0 + a1) / 2 * (float)PI / 180;
        const float u = ((std::cos(mid) + std::sin(mid)) / std::sqrt(2.f) + 1) / 2;
        dr::arc(k, { 0, 0 }, 118, 26, a0, std::min(a1, -90 + sweep), mixC(teal, iris, u));
    }
    // The play triangle springs in.
    const float pop = reveal >= 1 ? 1 : reveal < 0.35f ? 0 : (float)spring((reveal - 0.35f) * 60, SpringOptions{});
    if (pop > 0) {
        const Vector2 tri[3] = { { -32 * pop, -54 * pop }, { 62 * pop, 0 }, { -32 * pop, 54 * pop } };
        dr::fillPoly(k, tri, 3, ink);
    }
    // The play-head arc fades in and orbits.
    const float arcA = reveal >= 1 ? 1 : clamp01((reveal - 0.5f) / 0.3f);
    if (arcA > 0) {
        const float a = -90 + (float)std::fmod(t * spin, 360.0);
        dr::arc(k, { 0, 0 }, 148, 14, a, a + 70 * arcA, withAlpha(amber, arcA));
        dr::circle(k, polar(a + 70 * arcA, 148), 7, withAlpha(amber, arcA));
        dr::circle(k, polar(a, 148), 7, withAlpha(amber, arcA));
    }
}

// ------------------------------------------------------------------ prefabs

namespace {

// How an instance enters, at local time t (frames after its delay).
struct Phase {
    float scale = 1, alpha = 1, reveal = 1;
};

void checkEntrance(const std::string& kind) {
    if (kind != "pop" && kind != "draw" && kind != "fade" && kind != "none")
        throw std::runtime_error("icons: unknown entrance '" + kind + "' (pop, draw, fade or none)");
}

Phase entrance(const std::string& kind, double t, double length, int fps) {
    Phase ph;
    if (kind == "none") return ph;
    if (t < 0) return { 0, 0, 0 };
    if (kind == "pop") {
        SpringOptions o;
        o.fps = fps;
        o.config.damping = 12;
        ph.scale = (float)spring(t, o);
        ph.alpha = clamp01((float)t / 6);
    } else if (kind == "fade") {
        ph.alpha = smooth((float)(t / 12));
    } else {
        ph.reveal = clamp01((float)(t / length));
    }
    return ph;
}

// Something that follows the entrance: 0 until `after` frames in, then fades in.
float follow(const std::string& kind, double t, double after) {
    return kind == "none" ? 1.f : smooth((float)((t - after) / 14));
}

Look lookFrom(const Json& p) {
    Look look;
    look.style = parseStyle(p.str("style", "badge"));
    look.shape = parseShape(p.str("shape", "rounded"));
    look.color = p.color("color", WHITE);
    look.corner = (float)p.num("corner", 0.22);
    return look;
}

} // namespace

void registerPrefabs() {
    // One prefab per brand: the icon, optionally followed by a handle to its right; the
    // pair is centred on the node's origin.
    for (const std::string& brand : brands()) {
        prefab::add({
            "social-" + brand,
            Json::parse(R"({"size": 160, "style": "badge", "shape": "rounded", "color": "#ffffff", "handle": "",
                            "ink": "#f2f4ff", "in": "pop", "delay": 0, "fps": 30})"),
            [brand](TNode& n, const Json& p) {
                const float size = (float)p.num("size", 160);
                const Look look = lookFrom(p);
                const std::string handle = p.str("handle", ""), in = p.str("in", "pop");
                checkEntrance(in);
                const Color ink = p.color("ink", mkColor(0xf2f4ff));
                const int delay = p.integer("delay", 0), fps = p.integer("fps", 30);
                n.fn = [=](const Ctx& c, int frame) {
                    const double t = frameTime(frame) - delay;
                    const Phase ph = entrance(in, t, 30, fps);
                    if (ph.alpha <= 0) return;
                    Ctx k = c;
                    k.a *= ph.alpha;
                    Font font = fonts::ui(true);
                    const float fs = size * 0.36f, gap = size * 0.22f;
                    const float textW = handle.empty() ? 0 : dr::measure(font, handle.c_str(), fs, 0).x;
                    const float x0 = -(size + (handle.empty() ? 0 : gap + textW)) / 2;
                    draw(k, brand, { x0 + size / 2, 0 }, size * ph.scale, look, ph.reveal);
                    if (handle.empty()) return;
                    const float ha = follow(in, t, in == "draw" ? 18 : 8);
                    if (ha > 0)
                        dr::text(font, k, handle.c_str(), { x0 + size + gap - 24 * (1 - ha), 0 }, fs, 0, withAlpha(ink, ha),
                                 0, 0.5);
                };
            },
        });
    }

    // A row of icons popping in one after another, with an optional handle beneath.
    prefab::add({
        "social-row",
        Json::parse(R"({"brands": ["x", "youtube", "github", "instagram", "tiktok", "whatsapp", "telegram", "twitch"],
                        "size": 120, "gap": 40, "style": "badge", "shape": "rounded", "color": "#ffffff",
                        "stagger": 4, "handle": "", "ink": "#f2f4ff", "in": "pop", "delay": 0, "fps": 30})"),
        [](TNode& n, const Json& p) {
            std::vector<std::string> list;
            for (const Json& b : p["brands"].asArray()) {
                list.push_back(b.asString());
                prepared(list.back());                     // unknown brands fail at build time
            }
            const float size = (float)p.num("size", 120), gap = (float)p.num("gap", 40);
            const Look look = lookFrom(p);
            const std::string handle = p.str("handle", ""), in = p.str("in", "pop");
            checkEntrance(in);
            const Color ink = p.color("ink", mkColor(0xf2f4ff));
            const double stagger = p.num("stagger", 4);
            const int delay = p.integer("delay", 0), fps = p.integer("fps", 30);
            n.fn = [=](const Ctx& c, int frame) {
                const double t = frameTime(frame) - delay;
                const int count = (int)list.size();
                const float x0 = -(count * size + (count - 1) * gap) / 2 + size / 2;
                for (int i = 0; i < count; i++) {
                    const Phase ph = entrance(in, t - i * stagger, 30, fps);
                    if (ph.alpha <= 0) continue;
                    Ctx k = c;
                    k.a *= ph.alpha;
                    draw(k, list[i], { x0 + i * (size + gap), 0 }, size * ph.scale, look, ph.reveal);
                }
                if (handle.empty()) return;
                const float ha = follow(in, t, (count - 1) * stagger + (in == "draw" ? 24 : 10));
                if (ha > 0)
                    dr::text(fonts::ui(true), c, handle.c_str(), { 0, size / 2 + size * 0.5f + 16 * (1 - ha) }, size * 0.32f,
                             0, withAlpha(ink, ha), 0.5, 0.5);
            };
        },
    });

    // The raymotion logo: the mark with the wordmark beside it (or beneath, "stacked") and
    // an optional tagline, centred on the node's origin. "draw" sweeps the ring on, springs
    // the triangle in and spells the wordmark out.
    prefab::add({
        "raymotion-logo",
        Json::parse(R"({"size": 220, "wordmark": true, "tagline": "", "layout": "horizontal", "badge": false,
                        "spin": 2, "ink": "#f2f4ff", "accent": "#47dad8", "in": "draw", "length": 40, "delay": 0,
                        "fps": 30})"),
        [](TNode& n, const Json& p) {
            const float size = (float)p.num("size", 220), spin = (float)p.num("spin", 2);
            const bool wordmark = p.flag("wordmark", true), badge = p.flag("badge", false);
            const std::string tagline = p.str("tagline", ""), layout = p.str("layout", "horizontal"),
                              in = p.str("in", "draw");
            checkEntrance(in);
            if (layout != "horizontal" && layout != "stacked")
                throw std::runtime_error("raymotion-logo: layout is horizontal or stacked, not '" + layout + "'");
            const Color ink = p.color("ink", mkColor(0xf2f4ff)), accent = p.color("accent", mkColor(0x47dad8));
            const double length = std::max(1.0, p.num("length", 40));
            const int delay = p.integer("delay", 0), fps = p.integer("fps", 30);
            n.fn = [=](const Ctx& c, int frame) {
                const double t = frameTime(frame) - delay;
                const Phase ph = entrance(in, t, length * 0.7, fps);
                if (ph.alpha <= 0) return;
                Ctx k = c;
                k.a *= ph.alpha;
                const float mark = size * (badge ? 1.f : 0.8f);    // the mark's visible width
                Font bold = fonts::ui(true), light = fonts::ui(false);
                const float fs = size * 0.42f, ts = fs * 0.36f, gap = size * 0.14f;
                const char* word = "raymotion";
                const float wordW = wordmark ? dr::measure(bold, word, fs, 0).x : 0;
                const float tagW = tagline.empty() ? 0 : dr::measure(light, tagline.c_str(), ts, 2).x;
                const bool stacked = layout == "stacked";
                Vector2 centre{ 0, 0 }, wordAt{ 0, 0 }, tagAt{ 0, 0 };
                if (!wordmark) {
                    tagAt = { 0, mark / 2 + gap + ts / 2 };
                } else if (stacked) {
                    const float h = mark + gap + fs + (tagline.empty() ? 0 : ts * 1.6f);
                    centre = { 0, -h / 2 + mark / 2 };
                    wordAt = { 0, -h / 2 + mark + gap + fs / 2 };
                    tagAt = { 0, wordAt.y + fs / 2 + ts * 0.9f };
                } else {
                    const float w = mark + gap + std::max(wordW, tagW);
                    centre = { -w / 2 + mark / 2, 0 };
                    wordAt = { -w / 2 + mark + gap, tagline.empty() ? 0 : -ts * 0.7f };
                    tagAt = { wordAt.x, fs * 0.42f };
                }
                logo(k, centre, size * ph.scale, t, ph.reveal, badge, spin);
                if (wordmark) {
                    // Letters rise into place one after another.
                    const float u = in == "draw" ? smooth((float)((t - length * 0.3) / (length * 0.6))) : 1.f;
                    const int letters = 9;
                    for (int i = 0; i < letters; i++) {
                        const float a = clamp01((u * (letters + 3) - i) / 3);
                        if (a <= 0) break;
                        const std::string before(word, i), one(word + i, 1);
                        const float dx = before.empty() ? 0 : dr::measure(bold, before.c_str(), fs, 0).x;
                        const float x = stacked ? wordAt.x - wordW / 2 + dx : wordAt.x + dx;
                        dr::text(bold, k, one.c_str(), { x, wordAt.y + fs * 0.25f * (1 - a) }, fs, 0, withAlpha(ink, a), 0,
                                 0.5);
                    }
                }
                if (!tagline.empty()) {
                    const float ta = follow(in, t, in == "draw" ? length * 0.8 : 6);
                    if (ta > 0)
                        dr::text(light, k, tagline.c_str(), tagAt, ts, 2, withAlpha(accent, ta),
                                 stacked || !wordmark ? 0.5 : 0, 0.5);
                }
            };
        },
    });
}

} // namespace icons
} // namespace rmtn
