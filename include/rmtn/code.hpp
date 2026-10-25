// rmtn/code.hpp -- code:: code blocks for programming explainers: syntax highlighting,
// typewriter and line-by-line reveals, line focus, marks, and animated changes from one
// version of the code to the next.
//
//   const code::Block v1 = code::Block::parse(R"(int area(int w, int h) {
//       return w * h;
//   })", "cpp");
//   const code::Block v2 = code::Block::parse(..., "cpp");
//   const code::Morph change(v1, v2);                  // diff once, while registering
//   node->fn = [=](const Ctx& cx, int t) {
//       code::Style st;                                  // theme, size, line numbers, title bar
//       code::Look look;
//       look.reveal = t * 1.5;                           // characters typed so far
//       look.cursor = true;
//       if (t < 150) code::draw(cx, v1, { 200, 200 }, st, look);
//       else code::draw(cx, change, { 200, 200 }, st, (t - 150) / 60.f);
//   };
//
// Lines are numbered from 1 and columns from 0, as an editor shows them. Text is set in
// the bundled JetBrains Mono on a fixed grid, so columns line up across lines.
#pragma once
#include <string>
#include <vector>
#include "timeline.hpp"

namespace rmtn {
namespace code {

enum class Kind {
    Text,          // identifiers and anything unclassified
    Keyword,
    Type,          // built-in types and Capitalised names
    Function,      // a name followed by '(' (or a Rust macro!)
    String,
    Number,
    Comment,
    Operator,
    Punctuation,
    Preprocessor,  // #include, #define ...
    Constant,      // true, false, null, None, ALL_CAPS
    Property,      // a member after '.' or '->', a JSON key, a shell variable
    Attribute,     // @decorators, #[attributes], --flags
    Count
};

struct Token {
    std::string text;                 // UTF-8; tabs already expanded
    Kind kind = Kind::Text;
    int col = 0;                      // starting column
};
struct Line {
    std::vector<Token> tokens;
    std::string text;                 // the whole line
    int width = 0;                    // in columns
};

// A highlighted block of code. Languages: cpp (c, c++, h, hpp), c, csharp (cs), java,
// javascript (js, jsx), typescript (ts, tsx), python (py), rust (rs), go, glsl (hlsl),
// json, bash (sh, shell), sql, and text (no highlighting). Unknown names lex as C-like.
struct Block {
    std::vector<Line> lines;
    std::string lang;
    // A leading newline and trailing whitespace are dropped; tabs become `tabSize` spaces.
    static Block parse(const std::string& source, const std::string& lang = "cpp", int tabSize = 4);
    int columns() const;              // the longest line
    int chars() const;                // characters, a newline counting one (for reveal)
    // Column range of the n-th (0-based) occurrence of `needle` on `line`; { -1, -1 } if absent.
    std::pair<int, int> find(int line, const std::string& needle, int nth = 0) const;
};

// ------------------------------------------------------------------ looks

struct Theme {
    Color colors[(int)Kind::Count];
    Color background, border, header, title, lineNumber, focus, cursor;
    Color added, removed;             // diff marks in the gutter during a Morph
    static Theme dark();              // One Dark-like
    static Theme light();             // GitHub light-like
    Color of(Kind k) const { return colors[(int)k]; }
};

struct Style {
    Theme theme = Theme::dark();
    float size = 28;                  // font size, px
    float lineHeight = 1.5f;          // in font sizes
    bool bold = false;
    bool lineNumbers = true;
    bool card = true;                 // the rounded panel behind the code
    std::string title;                // a header bar with window dots and this title, if set
    Vector2 padding{ 36, 28 };
    float radius = 18;
    int columns = 0;                  // minimum width in columns (0: fit the longest line)
    bool diffGutter = true;           // + and - beside added and removed lines while morphing
};

// Lines to emphasise, 1-based and inclusive.
struct Lines { int from, to; };
using Focus = std::vector<Lines>;

// A box or underline over columns [from, to) of a line, drawn in `color`; u grows it in.
struct Mark {
    int line = 1, from = 0, to = 0;
    Color color{ 255, 209, 102, 255 };
    float u = 1;
    bool underline = false;
    // Marks the n-th occurrence of `needle` on `line`.
    static Mark text(const Block& b, int line, const std::string& needle, Color c, int nth = 0);
};

struct Look {
    double reveal = -1;               // typewriter: characters shown (Block::chars() for all); < 0: all
    bool cursor = false;              // a caret after the last character shown
    int t = 0;                        // frame, for the caret's blink (it stays solid while typing)
    float lines = -1;                 // line by line: lines shown, fractional fades the next in; < 0: all
    // Focus: lines outside it dim to `dim`; a band sits behind it. Empty: no focus. Blends
    // from `focusFrom` to `focus` by `focusMix` (0..1), so focus can glide between lines.
    Focus focus, focusFrom;
    float focusMix = 1;
    float dim = 0.28f;
    std::vector<Mark> marks;
};

// ------------------------------------------------------------------ drawing

// The card's box with its top-left at `at` (node px).
Rectangle bounds(const Block& b, const Style& s, Vector2 at = { 0, 0 });
// Top-left of the cell at (line, col), for pinning arrows and notes to the code.
Vector2 cell(const Block& b, const Style& s, Vector2 at, int line, int col);
float cellWidth(const Style& s);
void draw(const Ctx& c, const Block& b, Vector2 at, const Style& s, const Look& look = {});

// The change from one block to another, worked out once: unchanged lines and tokens glide
// to their new places (re-indented lines too), edited lines keep their common tokens, and
// the rest fade out (first) and in (last).
class Morph {
public:
    Morph(const Block& from, const Block& to);
    const Block& from() const { return a_; }
    const Block& to() const { return b_; }
    struct Piece {
        std::string text;
        Kind fromKind, toKind;
        int fromLine, fromCol;        // -1: not in `from` (fades in)
        int toLine, toCol;            // -1: not in `to` (fades out)
        bool number;                  // a line number
    };
    const std::vector<Piece>& pieces() const { return pieces_; }
    int added() const;                // lines only in `to`
    int removed() const;              // lines only in `from`
private:
    Block a_, b_;
    std::vector<Piece> pieces_;
    std::vector<int> lineFrom_;       // per line of `to`: its line in `from`, or -1
    std::vector<int> lineTo_;         // per line of `from`: its line in `to`, or -1
    friend void draw(const Ctx&, const Morph&, Vector2, const Style&, float);
};
// u = 0 draws `from`, 1 draws `to`.
void draw(const Ctx& c, const Morph& m, Vector2 at, const Style& s, float u);

Font font(bool bold = false);         // the bundled JetBrains Mono

} } // rmtn::code
