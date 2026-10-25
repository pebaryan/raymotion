// rmtn/math.hpp -- typeset mathematics (math::): a TeX subset laid out in-engine with an
// OpenType math font (Latin Modern Math is bundled), drawn as vector paths, written on,
// coloured by part, and transformed from one equation into another.
//
//   auto f = math::Formula::tex(R"(\id{lhs}{e^{i\pi}} + 1 = 0)");   // build while registering
//   node->fn = [f](const Ctx& cx, int t) {
//       math::write(cx, f, { 960, 540 }, { 0.5f, 0.5f }, t / 45.f);   // handwriting reveal
//   };
//
// Supported: letters (math italic), digits, operators and relations with TeX spacing;
// ^ _ and primes; \frac \dfrac \tfrac \binom \sqrt[n]{}; \sum \prod \int \oint \lim ... with
// limits; \left( \right) and other stretchy delimiters; \hat \bar \vec \dot \tilde \widehat
// accents; \overline \underline; \text \mathrm \mathbf \mathbb \mathcal \operatorname;
// Greek and ~200 symbols; \, \: \; \! \quad \qquad; matrix, pmatrix, bmatrix, vmatrix,
// Bmatrix, cases, aligned and gathered environments (a top-level \\ makes lines); \color
// \textcolor; \phantom; and \id{name}{...}, which names a part for colouring, placement
// and transforms. Unknown commands throw with their position.
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "paths.hpp"
#include "timeline.hpp"

namespace rmtn {
namespace math {

struct Options {
    Options() = default;
    Options(float px, Color c = { 255, 255, 255, 255 }, bool displayStyle = true)
        : size(px), color(c), display(displayStyle) {}
    float size = 64;                // px per em (the font size)
    Color color{ 255, 255, 255, 255 };   // for everything not coloured by \color
    bool display = true;              // display style (big operators, limits); false: inline
    std::string font;                 // an OpenType font with a MATH table; "" = Latin Modern Math
};

// One glyph or rule of a typeset formula, in formula px: the origin is the left end of
// the baseline, y grows downwards.
struct Piece {
    int glyph = -1;                   // font glyph index; -1 for a rule (fraction bar, radical, overline)
    Vector2 pos{ 0, 0 };              // glyph: baseline origin; rule: top-left corner
    float size = 0;                   // glyph: em size in px
    Vector2 rule{ 0, 0 };             // rule: width and height in px
    Color color{ 255, 255, 255, 255 };
    std::string part;                 // innermost \id, "" if none
    std::vector<std::string> parts;   // every enclosing \id, outermost first
};

class Font;
class Transform;

class Formula {
public:
    std::vector<Piece> pieces;        // in reading order
    float width = 0, ascent = 0, descent = 0;   // the metric box: x 0..width, y -ascent..descent
    std::string source;

    static Formula tex(const std::string& tex, const Options& options = {});   // throws on bad input

    // The metric box, or the ink bounds of a part (every piece inside \id{part}), in formula px.
    Rectangle bounds() const;
    Rectangle bounds(const std::string& part) const;
    bool has(const std::string& part) const;
    // A piece's outline (or all of them) in formula px.
    vg::Path path(const Piece& p) const;
    vg::Path path() const;

private:
    std::shared_ptr<const Font> font_;
    friend class math::Transform;
};

// Where the formula's origin goes so that `anchor` (fractions of its metric box; 0.5, 0.5
// is the centre) lands on `at`.
Vector2 origin(const Formula& f, Vector2 at, Vector2 anchor = { 0.5f, 0.5f });
// A part's ink box in node coordinates when the formula is drawn at `at`/`anchor`
// (for highlight boxes, braces and labels).
Rectangle place(const Formula& f, Vector2 at, Vector2 anchor, const std::string& part);

struct Look {
    std::map<std::string, Color> colors;   // part -> colour (overrides \color inside it)
    std::map<std::string, float> opacity;  // part -> opacity
    float alpha = 1;                       // the whole formula
};
void draw(const Ctx& c, const Formula& f, Vector2 at, Vector2 anchor = { 0.5f, 0.5f }, const Look& look = {});
// Handwriting reveal, u = 0..1: glyphs left to right, each outline drawing on then filling.
void write(const Ctx& c, const Formula& f, Vector2 at, Vector2 anchor, float u, const Look& look = {});

// One formula into another, u = 0..1. Pieces are matched within parts of the same name
// first, then by glyph (the k-th x on the left with the k-th x on the right, in reading
// order); matched pieces glide and rescale to their new places, the rest fade out (u 0 to
// 0.6) or in (0.4 to 1). Ease u yourself.
class Transform {
public:
    Transform() = default;
    Transform(const Formula& from, const Formula& to);
    void draw(const Ctx& c, Vector2 at, Vector2 anchor, float u, const Look& look = {}) const;
    int matched() const { return (int)pairs_.size(); }
private:
    Formula a_, b_;
    std::vector<std::pair<int, int>> pairs_;
    std::vector<int> onlyA_, onlyB_;
};

} } // rmtn::math
