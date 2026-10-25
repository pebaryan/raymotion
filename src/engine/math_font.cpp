// math_font.cpp -- OpenType math fonts: stb_truetype outlines and metrics plus the MATH
// table (see math_font.hpp). stb_truetype is compiled privately here (raylib keeps its own
// copy static too).
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb/stb_truetype.h"
#include "math_font.hpp"
#include "rmtn/media.hpp"
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <stdexcept>

namespace rmtn {
namespace math {

struct Font::Impl {
    std::vector<unsigned char> data;
    stbtt_fontinfo info{};
    float upem = 1000;
};

Font::~Font() = default;

namespace {

int u16(const unsigned char* p) { return (p[0] << 8) | p[1]; }
int s16(const unsigned char* p) { return (short)u16(p); }

// Coverage table -> glyph to coverage index.
std::unordered_map<int, int> coverage(const unsigned char* t) {
    std::unordered_map<int, int> m;
    const int format = u16(t);
    if (format == 1) {
        const int n = u16(t + 2);
        for (int i = 0; i < n; i++) m[u16(t + 4 + 2 * i)] = i;
    } else if (format == 2) {
        const int n = u16(t + 2);
        for (int i = 0; i < n; i++) {
            const unsigned char* r = t + 4 + 6 * i;
            const int start = u16(r), end = u16(r + 2), idx = u16(r + 4);
            for (int g = start; g <= end; g++) m[g] = idx + g - start;
        }
    }
    return m;
}

// MathItalicsCorrectionInfo / MathTopAccentAttachment: coverage + MathValueRecords.
std::unordered_map<int, float> valuesByGlyph(const unsigned char* t, float upem) {
    std::unordered_map<int, float> out;
    const std::unordered_map<int, int> cov = coverage(t + u16(t));
    const int n = u16(t + 2);
    for (const auto& [g, i] : cov)
        if (i < n) out[g] = s16(t + 4 + 4 * i) / upem;
    return out;
}

void constructions(const unsigned char* mv, const unsigned char* cov, int count, const unsigned char* offsets,
                   float upem, std::unordered_map<int, Font::Construction>& out) {
    if (!count || cov == mv) return;
    const std::unordered_map<int, int> c = coverage(cov);
    for (const auto& [g, i] : c) {
        if (i >= count) continue;
        const unsigned char* gc = mv + u16(offsets + 2 * i);
        Font::Construction con;
        const int assembly = u16(gc), variants = u16(gc + 2);
        for (int v = 0; v < variants; v++) con.variants.push_back({ u16(gc + 4 + 4 * v), u16(gc + 6 + 4 * v) / upem });
        if (assembly) {
            const unsigned char* ga = gc + assembly;
            const int parts = u16(ga + 4);
            for (int p = 0; p < parts; p++) {
                const unsigned char* r = ga + 6 + 10 * p;
                con.parts.push_back({ u16(r), u16(r + 2) / upem, u16(r + 4) / upem, u16(r + 6) / upem, (u16(r + 8) & 1) != 0 });
            }
        }
        out[g] = std::move(con);
    }
}

} // namespace

std::shared_ptr<const Font> Font::load(const std::string& file) {
    static std::mutex mu;
    static std::map<std::string, std::shared_ptr<const Font>> cache;
    const std::string path = media::resolve(file.empty() ? std::string("fonts/latinmodern-math.otf") : file);
    std::lock_guard<std::mutex> lock(mu);
    if (auto it = cache.find(path); it != cache.end()) return it->second;

    std::shared_ptr<Font> f(new Font());
    f->impl_ = std::make_unique<Impl>();
    Impl& im = *f->impl_;
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("math: cannot read font " + path);
    im.data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    const int offset = stbtt_GetFontOffsetForIndex(im.data.data(), 0);
    if (offset < 0 || !stbtt_InitFont(&im.info, im.data.data(), offset)) throw std::runtime_error("math: not a font: " + path);
    const unsigned char* base = im.data.data();
    const stbtt_uint32 head = stbtt__find_table(im.data.data(), (stbtt_uint32)offset, "head");
    im.upem = head ? (float)u16(base + head + 18) : 1000.f;
    const stbtt_uint32 mathTable = stbtt__find_table(im.data.data(), (stbtt_uint32)offset, "MATH");
    if (!mathTable) throw std::runtime_error("math: " + path + " has no MATH table (use an OpenType math font)");
    const float upem = im.upem;
    const unsigned char* M = base + mathTable;

    const unsigned char* C = M + u16(M + 4);   // MathConstants
    f->scriptScale = s16(C) / 100.f;
    f->scriptScriptScale = s16(C + 2) / 100.f;
    f->delimitedSubFormulaMinHeight = u16(C + 4) / upem;
    f->displayOperatorMinHeight = u16(C + 6) / upem;
    for (int i = 0; i < McCount; i++) f->k[i] = s16(C + 8 + 4 * i) / upem;
    f->radicalDegreeBottomRaise = s16(C + 8 + 4 * McCount) / 100.f;

    const unsigned char* G = M + u16(M + 6);   // MathGlyphInfo
    if (u16(G)) f->italic_ = valuesByGlyph(G + u16(G), upem);
    if (u16(G + 2)) f->accent_ = valuesByGlyph(G + u16(G + 2), upem);

    const unsigned char* V = M + u16(M + 8);   // MathVariants
    f->minConnectorOverlap = u16(V) / upem;
    const int vCount = u16(V + 6), hCount = u16(V + 8);
    constructions(V, V + u16(V + 2), vCount, V + 10, upem, f->vert_);
    constructions(V, V + u16(V + 4), hCount, V + 10 + 2 * vCount, upem, f->horiz_);

    cache[path] = f;
    return f;
}

int Font::glyph(int codepoint) const { return stbtt_FindGlyphIndex(&impl_->info, codepoint); }

Metrics Font::metrics(int g) const {
    Metrics m;
    int adv = 0, lsb = 0, x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetGlyphHMetrics(&impl_->info, g, &adv, &lsb);
    const float s = 1 / impl_->upem;
    m.advance = adv * s;
    if (stbtt_GetGlyphBox(&impl_->info, g, &x0, &y0, &x1, &y1)) {
        m.x0 = x0 * s; m.y0 = y0 * s; m.x1 = x1 * s; m.y1 = y1 * s;
    }
    return m;
}

float Font::italic(int g) const {
    const auto it = italic_.find(g);
    return it == italic_.end() ? 0.f : it->second;
}
bool Font::topAccent(int g, float& x) const {
    const auto it = accent_.find(g);
    if (it == accent_.end()) return false;
    x = it->second;
    return true;
}
const Font::Construction* Font::vertical(int g) const {
    const auto it = vert_.find(g);
    return it == vert_.end() ? nullptr : &it->second;
}
const Font::Construction* Font::horizontal(int g) const {
    const auto it = horiz_.find(g);
    return it == horiz_.end() ? nullptr : &it->second;
}

const vg::Path& Font::outline(int g) const {
    static std::mutex mu;
    std::lock_guard<std::mutex> lock(mu);
    if (auto it = outlines_.find(g); it != outlines_.end()) return it->second;
    vg::Path p;
    stbtt_vertex* v = nullptr;
    const int n = stbtt_GetGlyphShape(&impl_->info, g, &v);
    const float s = 1 / impl_->upem;
    auto P = [s](float x, float y) { return Vector2{ x * s, -y * s }; };
    bool open = false;
    for (int i = 0; i < n; i++) {
        switch (v[i].type) {
            case STBTT_vmove:
                if (open) p.close();
                p.moveTo(P(v[i].x, v[i].y));
                open = true;
                break;
            case STBTT_vline: p.lineTo(P(v[i].x, v[i].y)); break;
            case STBTT_vcurve: p.quadTo(P(v[i].cx, v[i].cy), P(v[i].x, v[i].y)); break;
            case STBTT_vcubic: p.cubicTo(P(v[i].cx, v[i].cy), P(v[i].cx1, v[i].cy1), P(v[i].x, v[i].y)); break;
        }
    }
    if (open) p.close();
    stbtt_FreeShape(&impl_->info, v);
    return outlines_[g] = std::move(p);
}

} } // rmtn::math
