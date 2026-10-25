// rmtn/textfx.hpp -- txt:: text animation: lay text out once into glyphs, then reveal it
// letter by letter, word by word or line by line with a staggered delay, type it, decode
// it from scrambled characters, style each glyph (waves, highlights), or morph one text
// into another.
//
//   const auto title = txt::later([] { return txt::Text::make("Every frame is a function", style(), 1200); });
//   node->fn = [title](const Ctx& cx, int t) {
//       txt::Reveal r;                                   // by letter, 2 frames apart
//       r.by = txt::Unit::Word;
//       r.offset = { 0, 40 };                            // each word rises 40 px as it fades in
//       txt::draw(cx, title(), { 960, 540 }, t, r, { 0.5f, 0.5f });
//   };
//
// Times are in frames since the animation started; everything is a pure function of t.
#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "anim.hpp"
#include "layout.hpp"
#include "timeline.hpp"

namespace rmtn {
namespace txt {

struct Glyph {
    std::string s;                    // one character, UTF-8
    Vector2 pos;                      // top-left, in the text's own box (0, 0 at its top-left)
    float width = 0;                  // advance, px
    int index = 0;                    // among the visible (non-space) glyphs
    int word = 0, line = 0;           // word and line it belongs to (spaces: the word before)
    int inWord = 0;                   // position within its word
    bool space = false;               // whitespace: laid out, never drawn
};

// Text laid out with a TextStyle (font, size, spacing, lineHeight, colour, align). With a
// width it wraps like dr::textBox; otherwise it breaks only at '\n'.
struct Text {
    std::vector<Glyph> glyphs;
    Vector2 size{ 0, 0 };             // the block's box
    int letters = 0, words = 0, lines = 0;
    TextStyle style;
    std::string source;
    static Text make(const std::string& text, const TextStyle& style, float width = 0);
    Rectangle bounds(Vector2 at = { 0, 0 }, Vector2 anchor = { 0, 0 }) const;
    // The glyph range [first, last] of the n-th word (0-based), for highlights.
    std::pair<int, int> word(int n) const;
};

// Compositions are registered before the window opens, and laying text out needs its font,
// so make Texts (and Morphs) on first use: later(make) returns a callable that runs `make`
// the first time and hands back the same result after. Copies share it.
template <class F> auto later(F make) {
    using T = decltype(make());
    auto slot = std::make_shared<std::optional<T>>();
    return [slot, make]() -> const T& { if (!*slot) slot->emplace(make()); return **slot; };
}

// How one glyph is drawn: changes relative to its resting place.
struct GlyphLook {
    Vector2 offset{ 0, 0 };           // px
    float scale = 1;                  // about the glyph's centre
    float rotate = 0;                 // degrees, about its centre
    float alpha = 1;
    Color color{ 0, 0, 0, 0 };        // starts as the style's colour
    std::string replace;              // non-empty: draw this character instead (scramble)
};
using Styler = std::function<void(const Glyph& g, GlyphLook& look)>;

// Draws the text with its top-left at at - anchor * size; `styler` adjusts each glyph.
void draw(const Ctx& c, const Text& text, Vector2 at, const Styler& styler = {}, Vector2 anchor = { 0, 0 });

// ------------------------------------------------------------------ staggered reveals

enum class Unit { Letter, Word, Line };
enum class Order { Forward, Backward, Center, Edges, Random };

struct Reveal {
    Unit by = Unit::Letter;
    Order order = Order::Forward;
    float stagger = 2;                // frames between one unit starting and the next
    float duration = 14;              // frames each unit takes
    Easing ease = ease::out(ease::cubic);
    // where each unit starts from (it eases to its resting place)
    float fade = 1;                   // 1: from transparent; 0: no fade
    Vector2 offset{ 0, 30 };          // px
    float scale = 1;                  // 1: no scaling; 0.5: grows from half size
    float rotate = 0;                 // degrees
    bool out = false;                 // run backwards: units leave in the same order
    unsigned seed = 1;                // for Order::Random
};
// Frames until the last unit has finished.
float length(const Text& text, const Reveal& r);
// 0..1 progress of glyph g at time t.
float progress(const Text& text, const Glyph& g, double t, const Reveal& r);
void draw(const Ctx& c, const Text& text, Vector2 at, double t, const Reveal& r, Vector2 anchor = { 0, 0 });

// ------------------------------------------------------------------ typing and decoding

// The first `chars` visible characters (fractional: the next one fades in), with an
// optional caret after them that blinks once typing has stopped (t = frame, for the blink).
void typewriter(const Ctx& c, const Text& text, Vector2 at, double chars, bool caret = true, int t = 0,
                Vector2 anchor = { 0, 0 });
// Each unit shows random characters from `charset` until it settles on its own (the
// reveal's timing decides when; fade and offset apply too). New random picks every `every` frames.
void scramble(const Ctx& c, const Text& text, Vector2 at, double t, const Reveal& r,
              const std::string& charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#%&*+=?", int every = 2,
              Vector2 anchor = { 0, 0 });

// ------------------------------------------------------------------ stylers

// A travelling wave: glyphs bob by `amp` px, `wavelength` glyphs apart, `speed` glyphs/frame.
Styler wave(double t, float amp = 10, float wavelength = 8, float speed = 0.25f);
// Recolour (and optionally lift and scale) glyphs [first, last] by u = 0..1.
Styler highlight(int first, int last, Color c, float u = 1, float lift = 0, float scale = 1);
// Runs stylers in order.
Styler chain(std::vector<Styler> stylers);

// ------------------------------------------------------------------ morphing

// One text turning into another: characters they share (in order) glide to their new
// places, the rest fade out and in, shrinking and growing a little.
class Morph {
public:
    Morph(const Text& from, const Text& to);
    const Text& from() const { return a_; }
    const Text& to() const { return b_; }
    int kept() const { return (int)pairs_.size(); }
    // u = 0: `from` at at - anchor * from.size; u = 1: `to` at at - anchor * to.size.
    void draw(const Ctx& c, Vector2 at, float u, Vector2 anchor = { 0, 0 }, Easing e = ease::inOut(ease::cubic)) const;
private:
    Text a_, b_;
    std::vector<std::pair<int, int>> pairs_;   // glyph index in a_ -> in b_
    std::vector<int> toOf_, fromOf_;           // -1: unmatched
};

} } // rmtn::txt
