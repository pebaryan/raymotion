// layout.cpp -- flex rows/columns, grids, text wrapping and text boxes.
#include "rmtn/layout.hpp"
#include "rmtn/draw.hpp"
#include <algorithm>

namespace rmtn {
namespace layout {
namespace {
std::vector<Rectangle> split(Rectangle r, const std::vector<Item>& items, float gap, bool horizontal) {
    const float mainLen = horizontal ? r.width : r.height;
    float fixed = gap * (items.empty() ? 0 : (float)items.size() - 1), weights = 0;
    for (const Item& it : items) { fixed += it.flex > 0 ? 0 : it.size; weights += std::max(0.f, it.flex); }
    const float spare = std::max(0.f, mainLen - fixed);
    std::vector<Rectangle> out;
    float pos = horizontal ? r.x : r.y;
    for (const Item& it : items) {
        float len = it.flex > 0 ? (weights > 0 ? spare * it.flex / weights : 0) : it.size;
        out.push_back(horizontal ? Rectangle{ pos, r.y, len, r.height } : Rectangle{ r.x, pos, r.width, len });
        pos += len + gap;
    }
    return out;
}
}
std::vector<Rectangle> row(Rectangle r, const std::vector<Item>& items, float gap) { return split(r, items, gap, true); }
std::vector<Rectangle> column(Rectangle r, const std::vector<Item>& items, float gap) { return split(r, items, gap, false); }
std::vector<Rectangle> grid(Rectangle r, int cols, int rows, float gap) {
    std::vector<Rectangle> out;
    if (cols <= 0 || rows <= 0) return out;
    for (const Rectangle& line : column(r, std::vector<Item>((size_t)rows, flex()), gap))
        for (const Rectangle& cell : row(line, std::vector<Item>((size_t)cols, flex()), gap)) out.push_back(cell);
    return out;
}
} // namespace layout

namespace {
float lineWidth(const std::string& s, const TextStyle& st) {
    return s.empty() ? 0.f : MeasureTextEx(st.font, s.c_str(), st.size, st.spacing).x;
}
// Byte length of the UTF-8 sequence starting at s[i].
size_t charLen(const std::string& s, size_t i) {
    size_t n = 1;
    while (i + n < s.size() && ((unsigned char)s[i + n] & 0xC0) == 0x80) n++;
    return n;
}
}

std::vector<std::string> wrapText(const std::string& text, const TextStyle& st, float width) {
    std::vector<std::string> lines;
    std::string line, word;
    auto fits = [&](const std::string& s) { return lineWidth(s, st) <= width; };
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string joined = line.empty() ? word : line + " " + word;
        if (fits(joined)) { line = joined; word.clear(); return; }
        if (!line.empty()) { lines.push_back(line); line.clear(); }
        while (!fits(word)) {                          // a single word wider than the box
            size_t cut = 0;
            while (cut < word.size()) {
                size_t next = cut + charLen(word, cut);
                if (!fits(word.substr(0, next)) && cut > 0) break;
                cut = next;
            }
            if (cut >= word.size()) break;
            lines.push_back(word.substr(0, cut));
            word = word.substr(cut);
        }
        line = word;
        word.clear();
    };
    for (char ch : text) {
        if (ch == ' ' || ch == '\n') {
            flushWord();
            if (ch == '\n') { lines.push_back(line); line.clear(); }
        } else word += ch;
    }
    flushWord();
    if (!line.empty() || lines.empty()) lines.push_back(line);
    return lines;
}

Vector2 measureTextBox(const std::string& text, const TextStyle& st, float width) {
    auto lines = wrapText(text, st, width);
    float w = 0;
    for (const auto& l : lines) w = std::max(w, lineWidth(l, st));
    return { w, (float)lines.size() * st.size * st.lineHeight };
}

int dr::textBox(const Ctx& c, const std::string& text, Rectangle box, const TextStyle& st) {
    auto lines = wrapText(text, st, box.width);
    const float lh = st.size * st.lineHeight;
    // Lines that fit vertically; at least one so a too-small box still shows something.
    size_t shown = std::max<size_t>(1, std::min(lines.size(), (size_t)(box.height / lh + 1e-3f)));
    const float blockH = (float)shown * lh;
    float y = st.valign == Align::Start ? box.y : st.valign == Align::End ? box.y + box.height - blockH
                                                                          : box.y + (box.height - blockH) / 2;
    for (size_t i = 0; i < shown; i++) {
        float w = lineWidth(lines[i], st);
        float x = st.align == Align::Start ? box.x : st.align == Align::End ? box.x + box.width - w
                                                                            : box.x + (box.width - w) / 2;
        // Centre the glyph row within its line box.
        dr::text(st.font, c, lines[i].c_str(), { x, y + (lh - st.size) / 2 }, st.size, st.spacing, st.color);
        y += lh;
    }
    return (int)shown;
}

} // namespace rmtn
