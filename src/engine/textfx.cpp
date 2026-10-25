// textfx.cpp -- txt:: glyph layout, staggered reveals, typing, decoding, stylers and text
// morphs (see rmtn/textfx.hpp).
#include "rmtn/textfx.hpp"
#include "rmtn/draw.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace rmtn {
namespace txt {
namespace {

Font usable(Font f) { return f.texture.id == 0 ? GetFontDefault() : f; }

// UTF-8 characters of s
std::vector<std::string> chars(const std::string& s) {
    std::vector<std::string> out;
    for (size_t i = 0; i < s.size();) {
        size_t n = 1;
        const unsigned char c = (unsigned char)s[i];
        if (c >= 0xF0) n = 4; else if (c >= 0xE0) n = 3; else if (c >= 0xC0) n = 2;
        n = std::min(n, s.size() - i);
        out.push_back(s.substr(i, n));
        i += n;
    }
    return out;
}

// the advance of one character, as raylib's DrawTextEx steps it
float advance(Font f, const std::string& ch, float size) {
    int bytes = 0;
    const int cp = GetCodepoint(ch.c_str(), &bytes);
    const int i = GetGlyphIndex(f, cp);
    const float adv = f.glyphs[i].advanceX != 0 ? (float)f.glyphs[i].advanceX : f.recs[i].width;
    return adv * size / (float)f.baseSize;
}

bool isSpace(const std::string& s) { return s == " " || s == "\t"; }

float clamp01(double v) { return (float)std::clamp(v, 0.0, 1.0); }
float smooth(float u) { return u * u * (3 - 2 * u); }
float ramp(float u, float a, float b) { return clamp01((u - a) / (b - a)); }

unsigned hash(unsigned a, unsigned b, unsigned c) {
    unsigned h = a * 2654435761u ^ (b + 0x9e3779b9u + (a << 6) + (a >> 2)) ^ (c * 40503u);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}

Vector2 origin(const Text& t, Vector2 at, Vector2 anchor) { return { at.x - anchor.x * t.size.x, at.y - anchor.y * t.size.y }; }

// one glyph, transformed about its centre
void glyph(const Ctx& c, const TextStyle& st, float size, const std::string& s, Vector2 centre, const GlyphLook& l) {
    if (l.alpha <= 0.002f || s.empty()) return;
    Ctx k = c;
    k.o = mapP(c, { centre.x + l.offset.x, centre.y + l.offset.y });
    k.s = c.s * l.scale;
    k.rot = c.rot + l.rotate;
    const Font f = usable(st.font);
    const float w = MeasureTextEx(f, s.c_str(), size, 0).x;
    dr::text(f, k, s.c_str(), { -w / 2, -size / 2 }, size, 0, withAlpha(l.color, l.alpha));
}
Vector2 centreOf(const Text& t, const Glyph& g, Vector2 o) {
    return { o.x + g.pos.x + g.width / 2, o.y + g.pos.y + t.style.size / 2 };
}

int unitOf(const Glyph& g, Unit u) { return u == Unit::Letter ? g.index : u == Unit::Word ? g.word : g.line; }
int units(const Text& t, Unit u) { return u == Unit::Letter ? t.letters : u == Unit::Word ? t.words : t.lines; }

// when each unit starts, in units of `stagger`
std::vector<float> ranks(int n, Order o, unsigned seed) {
    std::vector<float> r((size_t)std::max(n, 0));
    const float mid = (n - 1) / 2.f;
    for (int k = 0; k < n; k++) {
        switch (o) {
        case Order::Forward: r[(size_t)k] = (float)k; break;
        case Order::Backward: r[(size_t)k] = (float)(n - 1 - k); break;
        case Order::Center: r[(size_t)k] = std::fabs(k - mid); break;
        case Order::Edges: r[(size_t)k] = mid - std::fabs(k - mid); break;
        case Order::Random: break;
        }
    }
    if (o == Order::Random) {
        std::vector<int> idx((size_t)n);
        std::iota(idx.begin(), idx.end(), 0);
        std::sort(idx.begin(), idx.end(), [seed](int a, int b) { return hash((unsigned)a, seed, 7) < hash((unsigned)b, seed, 7); });
        for (int k = 0; k < n; k++) r[(size_t)idx[(size_t)k]] = (float)k;
    }
    return r;
}

// the look a reveal gives a glyph at progress p
GlyphLook revealLook(const Reveal& r, float p, Color base) {
    const float e = (float)(r.ease ? r.ease(p) : p);
    GlyphLook l;
    l.color = base;
    l.alpha = 1 - r.fade * (1 - std::clamp(e, 0.f, 1.f));
    l.offset = { r.offset.x * (1 - e), r.offset.y * (1 - e) };
    l.scale = r.scale + (1 - r.scale) * e;
    l.rotate = r.rotate * (1 - e);
    return l;
}

} // namespace

// ------------------------------------------------------------------ layout

Text Text::make(const std::string& text, const TextStyle& style, float width) {
    Text t;
    t.style = style;
    t.source = text;
    const Font f = usable(style.font);
    std::vector<std::string> lines;
    if (width > 0) lines = wrapText(text, style, width);
    else {
        size_t from = 0;
        while (true) {
            const size_t nl = text.find('\n', from);
            lines.push_back(text.substr(from, nl == std::string::npos ? std::string::npos : nl - from));
            if (nl == std::string::npos) break;
            from = nl + 1;
        }
    }
    const float lh = style.size * style.lineHeight;
    std::vector<float> lineW;
    std::vector<std::vector<Glyph>> rows;
    int word = -1;
    for (size_t li = 0; li < lines.size(); li++) {
        std::vector<Glyph> row;
        float x = 0;
        bool inWord = false;
        int inWordIndex = 0;
        for (const std::string& ch : chars(lines[li])) {
            Glyph g;
            g.s = ch;
            g.space = isSpace(ch);
            g.width = advance(f, ch == "\t" ? " " : ch, style.size);
            g.pos = { x, (float)li * lh + (lh - style.size) / 2 };
            g.line = (int)li;
            if (!g.space) {
                if (!inWord) { word++; inWord = true; inWordIndex = 0; }
                g.index = t.letters++;
                g.inWord = inWordIndex++;
            } else inWord = false;
            g.word = std::max(word, 0);
            x += g.width + style.spacing;
            row.push_back(g);
        }
        lineW.push_back(row.empty() ? 0 : x - style.spacing);
        rows.push_back(row);
    }
    const float boxW = width > 0 ? width : (lineW.empty() ? 0 : *std::max_element(lineW.begin(), lineW.end()));
    for (size_t li = 0; li < rows.size(); li++) {
        const float shift = style.align == Align::Start ? 0 : style.align == Align::End ? boxW - lineW[li] : (boxW - lineW[li]) / 2;
        for (Glyph& g : rows[li]) { g.pos.x += shift; t.glyphs.push_back(g); }
    }
    t.words = word + 1;
    t.lines = (int)lines.size();
    t.size = { boxW, (float)lines.size() * lh };
    return t;
}

Rectangle Text::bounds(Vector2 at, Vector2 anchor) const {
    const Vector2 o = origin(*this, at, anchor);
    return { o.x, o.y, size.x, size.y };
}

std::pair<int, int> Text::word(int n) const {
    int first = -1, last = -1;
    for (const Glyph& g : glyphs)
        if (!g.space && g.word == n) { if (first < 0) first = g.index; last = g.index; }
    return { first, last };
}

void draw(const Ctx& c, const Text& text, Vector2 at, const Styler& styler, Vector2 anchor) {
    const Vector2 o = origin(text, at, anchor);
    for (const Glyph& g : text.glyphs) {
        if (g.space) continue;
        GlyphLook l;
        l.color = text.style.color;
        if (styler) styler(g, l);
        glyph(c, text.style, text.style.size, l.replace.empty() ? g.s : l.replace, centreOf(text, g, o), l);
    }
}

// ------------------------------------------------------------------ reveals

float length(const Text& text, const Reveal& r) {
    const std::vector<float> rk = ranks(units(text, r.by), r.order, r.seed);
    const float last = rk.empty() ? 0 : *std::max_element(rk.begin(), rk.end());
    return last * r.stagger + r.duration;
}

float progress(const Text& text, const Glyph& g, double t, const Reveal& r) {
    const std::vector<float> rk = ranks(units(text, r.by), r.order, r.seed);
    const int u = unitOf(g, r.by);
    if (u < 0 || u >= (int)rk.size()) return 1;
    const float p = clamp01((t - rk[(size_t)u] * r.stagger) / std::max(r.duration, 1e-3f));
    return r.out ? 1 - p : p;
}

void draw(const Ctx& c, const Text& text, Vector2 at, double t, const Reveal& r, Vector2 anchor) {
    const std::vector<float> rk = ranks(units(text, r.by), r.order, r.seed);
    const Vector2 o = origin(text, at, anchor);
    for (const Glyph& g : text.glyphs) {
        if (g.space) continue;
        const int u = unitOf(g, r.by);
        float p = u < (int)rk.size() ? clamp01((t - rk[(size_t)u] * r.stagger) / std::max(r.duration, 1e-3f)) : 1;
        if (r.out) p = 1 - p;
        glyph(c, text.style, text.style.size, g.s, centreOf(text, g, o), revealLook(r, p, text.style.color));
    }
}

// ------------------------------------------------------------------ typing and decoding

void typewriter(const Ctx& c, const Text& text, Vector2 at, double n, bool caret, int t, Vector2 anchor) {
    const Vector2 o = origin(text, at, anchor);
    const double shown = std::clamp(n, 0.0, (double)text.letters);
    Vector2 caretAt{ o.x, o.y + (text.style.size * text.style.lineHeight - text.style.size) / 2 };
    for (const Glyph& g : text.glyphs) {
        if (g.space) continue;
        const float a = clamp01(shown - g.index);
        if (a <= 0) break;
        GlyphLook l;
        l.color = text.style.color;
        l.alpha = a;
        glyph(c, text.style, text.style.size, g.s, centreOf(text, g, o), l);
        caretAt = { o.x + g.pos.x + g.width + text.style.spacing, o.y + g.pos.y };
    }
    if (!caret) return;
    const bool typing = shown < text.letters;
    if (!typing && (t / 18) % 2 != 0) return;
    const float s = text.style.size;
    dr::rect(c, caretAt.x + s * 0.04f, caretAt.y - s * 0.02f, std::max(2.f, s * 0.07f), s * 1.08f, text.style.color);
}

void scramble(const Ctx& c, const Text& text, Vector2 at, double t, const Reveal& r, const std::string& charset, int every,
              Vector2 anchor) {
    const std::vector<std::string> pool = chars(charset);
    const std::vector<float> rk = ranks(units(text, r.by), r.order, r.seed);
    const Vector2 o = origin(text, at, anchor);
    const unsigned tick = (unsigned)std::floor(t / std::max(every, 1));
    for (const Glyph& g : text.glyphs) {
        if (g.space) continue;
        const int u = unitOf(g, r.by);
        float p = u < (int)rk.size() ? clamp01((t - rk[(size_t)u] * r.stagger) / std::max(r.duration, 1e-3f)) : 1;
        if (r.out) p = 1 - p;
        GlyphLook l = revealLook(r, p, text.style.color);
        if (p < 1 && !pool.empty()) {
            l.replace = pool[hash((unsigned)g.index, tick, r.seed) % pool.size()];
            // a stand-in wider than the slot shrinks to fit, so neighbours never overlap
            const float w = MeasureTextEx(usable(text.style.font), l.replace.c_str(), text.style.size, 0).x;
            if (w > g.width) l.scale *= g.width / w;
        }
        glyph(c, text.style, text.style.size, l.replace.empty() ? g.s : l.replace, centreOf(text, g, o), l);
    }
}

// ------------------------------------------------------------------ stylers

Styler wave(double t, float amp, float wavelength, float speed) {
    return [=](const Glyph& g, GlyphLook& l) {
        l.offset.y -= amp * (float)std::sin(6.283185307179586 * (g.index - speed * t) / std::max(wavelength, 1e-3f));
    };
}

Styler highlight(int first, int last, Color col, float u, float lift, float scale) {
    return [=](const Glyph& g, GlyphLook& l) {
        if (g.index < first || g.index > last) return;
        const float k = std::clamp(u, 0.f, 1.f);
        l.color = mixC(l.color, col, k);
        l.offset.y -= lift * k;
        l.scale *= 1 + (scale - 1) * k;
    };
}

Styler chain(std::vector<Styler> stylers) {
    return [stylers = std::move(stylers)](const Glyph& g, GlyphLook& l) {
        for (const Styler& s : stylers) if (s) s(g, l);
    };
}

// ------------------------------------------------------------------ morphing

Morph::Morph(const Text& from, const Text& to) : a_(from), b_(to) {
    std::vector<int> xa, xb;   // glyph vector indices of the visible characters
    for (size_t i = 0; i < a_.glyphs.size(); i++) if (!a_.glyphs[i].space) xa.push_back((int)i);
    for (size_t i = 0; i < b_.glyphs.size(); i++) if (!b_.glyphs[i].space) xb.push_back((int)i);
    const size_t n = xa.size(), m = xb.size();
    std::vector<std::vector<int>> d(n + 1, std::vector<int>(m + 1, 0));
    for (size_t i = n; i-- > 0;)
        for (size_t j = m; j-- > 0;)
            d[i][j] = a_.glyphs[(size_t)xa[i]].s == b_.glyphs[(size_t)xb[j]].s ? d[i + 1][j + 1] + 1 : std::max(d[i + 1][j], d[i][j + 1]);
    toOf_.assign(a_.glyphs.size(), -1);
    fromOf_.assign(b_.glyphs.size(), -1);
    size_t i = 0, j = 0;
    while (i < n && j < m) {
        if (a_.glyphs[(size_t)xa[i]].s == b_.glyphs[(size_t)xb[j]].s) {
            pairs_.push_back({ xa[i], xb[j] });
            toOf_[(size_t)xa[i]] = xb[j];
            fromOf_[(size_t)xb[j]] = xa[i];
            i++; j++;
        } else if (d[i + 1][j] >= d[i][j + 1]) i++;
        else j++;
    }
}

void Morph::draw(const Ctx& c, Vector2 at, float u, Vector2 anchor, Easing e) const {
    u = std::clamp(u, 0.f, 1.f);
    const float m = (float)(e ? e(u) : u);
    const Vector2 oa = origin(a_, at, anchor), ob = origin(b_, at, anchor);
    const TextStyle& late = u < 0.5f ? a_.style : b_.style;
    for (const auto& pq : pairs_) {
        const Glyph& ga = a_.glyphs[(size_t)pq.first];
        const Glyph& gb = b_.glyphs[(size_t)pq.second];
        const Vector2 ca = centreOf(a_, ga, oa), cb = centreOf(b_, gb, ob);
        GlyphLook l;
        l.color = mixC(a_.style.color, b_.style.color, m);
        const float size = a_.style.size + (b_.style.size - a_.style.size) * m;
        glyph(c, late, size, ga.s, { ca.x + (cb.x - ca.x) * m, ca.y + (cb.y - ca.y) * m }, l);
    }
    const float out = smooth(ramp(u, 0, 0.55f)), in = smooth(ramp(u, 0.45f, 1));
    for (size_t k = 0; k < a_.glyphs.size(); k++) {
        const Glyph& g = a_.glyphs[k];
        if (g.space || toOf_[k] >= 0) continue;
        GlyphLook l;
        l.color = a_.style.color;
        l.alpha = 1 - out;
        l.scale = 1 - 0.35f * out;
        l.offset = { 0, -a_.style.size * 0.25f * out };
        glyph(c, a_.style, a_.style.size, g.s, centreOf(a_, g, oa), l);
    }
    for (size_t k = 0; k < b_.glyphs.size(); k++) {
        const Glyph& g = b_.glyphs[k];
        if (g.space || fromOf_[k] >= 0) continue;
        GlyphLook l;
        l.color = b_.style.color;
        l.alpha = in;
        l.scale = 0.65f + 0.35f * in;
        l.offset = { 0, b_.style.size * 0.25f * (1 - in) };
        glyph(c, b_.style, b_.style.size, g.s, centreOf(b_, g, ob), l);
    }
}

} } // rmtn::txt
