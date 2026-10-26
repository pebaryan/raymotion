// rmtn/icons.hpp -- social media icons and the raymotion logo, as drawings and prefabs.
//
//     icons::draw(cx, "youtube", { 960, 540 }, 160);                  // a red badge, white play
//     icons::Look mono{ icons::Style::Mono };  mono.color = INK;
//     icons::draw(cx, "github", { 1200, 540 }, 96, mono);
//     icons::logo(cx, { 300, 120 }, 120, t);                          // the raymotion mark
//
// or, as prefabs (registered by the CLI before the project's own, which may replace them):
//
//     prefab::use(root, 0, 90, "social-instagram", Json::parse(R"({"handle": "@raymotion"})"));
//     prefab::use(root, 0, 120, "social-row", Json::parse(R"({"brands": ["x", "youtube", "github"]})"));
//     prefab::use(root, 0, 150, "raymotion-logo", Json::parse(R"({"tagline": "video as code"})"));
//
// Brands: x, youtube, github, instagram, tiktok, whatsapp, telegram. The glyphs are the
// Simple Icons paths (CC0); the logos themselves are trademarks of their owners, so follow
// each brand's guidelines when you use them.
#pragma once
#include <string>
#include <vector>
#include "paths.hpp"
#include "timeline.hpp"

namespace rmtn {
namespace icons {

const std::vector<std::string>& brands();
// The brand's mark in a 24 x 24 box centred on the origin (-12..12); fill it EvenOdd.
// Throws std::runtime_error for an unknown brand.
const vg::Path& glyph(const std::string& brand);
Color brandColor(const std::string& brand);
std::string title(const std::string& brand);   // "X", "YouTube", "GitHub", ...

enum class Style {
    Badge,      // the brand's colour (Instagram's gradient) behind a white mark
    Glyph,      // the mark alone in the brand's colour (dark brands use Look::color)
    Mono,       // the mark alone in Look::color
    Outline,    // the badge's outline and the mark, both in Look::color
};
enum class Shape { Rounded, Circle, Square };

struct Look {
    Style style = Style::Badge;
    Shape shape = Shape::Rounded;
    Color color = WHITE;        // Mono, Outline, dark brands' Glyph, and the mark on a badge
    float corner = 0.22f;       // Rounded: corner radius as a share of the size
};
Style parseStyle(const std::string& s);   // "badge", "glyph", "mono", "outline"; throws otherwise
Shape parseShape(const std::string& s);   // "rounded", "circle", "square"

// Draws `brand` centred at `centre`, `size` px across (node-local). `reveal` < 1 draws it
// on: the badge grows in and the mark writes itself (vg::write).
void draw(const Ctx& c, const std::string& brand, Vector2 centre, float size, const Look& look = {},
          float reveal = 1);

// The raymotion mark: a gradient ring, a play triangle and an amber play-head arc that
// orbits at `spin` degrees per frame. `size` is the badge's width (the ring is ~0.6 of it);
// `badge` draws the dark rounded square behind it. `reveal` < 1 draws it on.
void logo(const Ctx& c, Vector2 centre, float size, double t, float reveal = 1, bool badge = false,
          float spin = 2);

// Registers social-x, social-youtube, ..., social-row and raymotion-logo. Safe to call twice.
void registerPrefabs();

} // namespace icons
} // namespace rmtn
