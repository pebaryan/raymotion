// rmtn/layout.hpp -- rectangle layout (fill, inset, align, flex rows/columns, grids) and
// wrapped, aligned text boxes. Rectangles are in whatever space you draw in: composition
// pixels, or a node's local space when used inside its fn.
#pragma once
#include <raylib.h>
#include <algorithm>
#include <string>
#include <vector>
#include "timeline.hpp"

namespace rmtn {

enum class Align { Start, Center, End };

namespace layout {

// Remotion's <AbsoluteFill>: the whole w x h area.
inline Rectangle fill(float w, float h) { return { 0, 0, w, h }; }
inline Rectangle inset(Rectangle r, float top, float right, float bottom, float left) {
    return { r.x + left, r.y + top, std::max(0.f, r.width - left - right), std::max(0.f, r.height - top - bottom) };
}
inline Rectangle inset(Rectangle r, float all) { return inset(r, all, all, all, all); }
inline Vector2 center(Rectangle r) { return { r.x + r.width / 2, r.y + r.height / 2 }; }
// A `size` box placed inside `outer`.
inline Rectangle place(Rectangle outer, Vector2 size, Align h = Align::Center, Align v = Align::Center) {
    auto at = [](float start, float span, float len, Align a) {
        return a == Align::Start ? start : a == Align::End ? start + span - len : start + (span - len) / 2;
    };
    return { at(outer.x, outer.width, size.x, h), at(outer.y, outer.height, size.y, v), size.x, size.y };
}

// A child of a row/column: a fixed main-axis size, or a share of the leftover space.
struct Item { float size = 0, flex = 0; };
inline Item px(float size) { return { size, 0 }; }
inline Item flex(float weight = 1) { return { 0, weight }; }

// Splits r along x (row) or y (column). Fixed items get their size; flex items share
// what is left after fixed sizes and gaps, by weight. Cross axis stretches to r.
std::vector<Rectangle> row(Rectangle r, const std::vector<Item>& items, float gap = 0);
std::vector<Rectangle> column(Rectangle r, const std::vector<Item>& items, float gap = 0);
// cols x rows equal cells, row-major.
std::vector<Rectangle> grid(Rectangle r, int cols, int rows, float gap = 0);

} // namespace layout

// ------------------------------------------------------------------ text boxes
struct TextStyle {
    Font font{};                 // e.g. fonts::ui()
    float size = 32;
    float spacing = 0;           // extra px between glyphs
    float lineHeight = 1.25f;    // multiple of size
    Color color = WHITE;
    Align align = Align::Start;  // horizontal, per line
    Align valign = Align::Start; // the block within the box
};

// Greedy word wrap to `width`; '\n' forces a break, words wider than the box are split
// between characters (UTF-8 safe).
std::vector<std::string> wrapText(const std::string& text, const TextStyle& style, float width);
// Size of the wrapped block: widest line x (lines * lineHeight * size).
Vector2 measureTextBox(const std::string& text, const TextStyle& style, float width);

namespace dr {
// Wraps `text` to box.width and draws it aligned inside `box` (local to ctx). Returns the
// number of lines that fit; lines below the box are not drawn.
int textBox(const Ctx& c, const std::string& text, Rectangle box, const TextStyle& style);
}

} // namespace rmtn
