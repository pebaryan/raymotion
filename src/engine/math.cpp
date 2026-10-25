// math.cpp -- the math:: typesetter: a TeX-subset parser, layout after TeX's rules (The
// TeXbook, appendix G) with an OpenType MATH font's constants and variants, and drawing,
// writing and transforms over vector paths (see rmtn/math.hpp).
#include "rmtn/math.hpp"
#include "math_font.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <unordered_map>

namespace rmtn {
namespace math {
namespace {

// ------------------------------------------------------------------ symbols

enum class T { Ord, Op, Bin, Rel, Open, Close, Punct, Inner, None };

struct Sym { int cp; T type; };
const std::unordered_map<std::string, Sym>& symbols() {
    static const std::unordered_map<std::string, Sym> m = {
        // ordinary
        { "infty", { 0x221E, T::Ord } }, { "nabla", { 0x2207, T::Ord } }, { "partial", { 0x1D715, T::Ord } },
        { "forall", { 0x2200, T::Ord } }, { "exists", { 0x2203, T::Ord } }, { "nexists", { 0x2204, T::Ord } },
        { "emptyset", { 0x2205, T::Ord } }, { "varnothing", { 0x2205, T::Ord } }, { "hbar", { 0x210F, T::Ord } },
        { "ell", { 0x2113, T::Ord } }, { "Re", { 0x211C, T::Ord } }, { "Im", { 0x2111, T::Ord } },
        { "aleph", { 0x2135, T::Ord } }, { "prime", { 0x2032, T::Ord } }, { "angle", { 0x2220, T::Ord } },
        { "triangle", { 0x25B3, T::Ord } }, { "neg", { 0x00AC, T::Ord } }, { "lnot", { 0x00AC, T::Ord } },
        { "top", { 0x22A4, T::Ord } }, { "bot", { 0x22A5, T::Ord } }, { "degree", { 0x00B0, T::Ord } },
        { "imath", { 0x1D6A4, T::Ord } }, { "jmath", { 0x1D6A5, T::Ord } }, { "wp", { 0x2118, T::Ord } },
        { "backslash", { 0x005C, T::Ord } }, { "vdots", { 0x22EE, T::Ord } }, { "dagger", { 0x2020, T::Ord } },
        { "ddagger", { 0x2021, T::Ord } }, { "sharp", { 0x266F, T::Ord } }, { "flat", { 0x266D, T::Ord } },
        { "natural", { 0x266E, T::Ord } }, { "clubsuit", { 0x2663, T::Ord } }, { "heartsuit", { 0x2661, T::Ord } },
        { "spadesuit", { 0x2660, T::Ord } }, { "diamondsuit", { 0x2662, T::Ord } }, { "square", { 0x25A1, T::Ord } },
        { "Box", { 0x25A1, T::Ord } }, { "blacksquare", { 0x25A0, T::Ord } }, { "lbrace", { '{', T::Open } },
        { "rbrace", { '}', T::Close } }, { "vert", { '|', T::Ord } }, { "Vert", { 0x2016, T::Ord } },
        { "lvert", { '|', T::Open } }, { "rvert", { '|', T::Close } }, { "lVert", { 0x2016, T::Open } },
        { "rVert", { 0x2016, T::Close } }, { "langle", { 0x27E8, T::Open } }, { "rangle", { 0x27E9, T::Close } },
        { "lfloor", { 0x230A, T::Open } }, { "rfloor", { 0x230B, T::Close } }, { "lceil", { 0x2308, T::Open } },
        { "rceil", { 0x2309, T::Close } }, { "lbrack", { '[', T::Open } }, { "rbrack", { ']', T::Close } },
        // dots
        { "ldots", { 0x2026, T::Inner } }, { "dots", { 0x2026, T::Inner } }, { "cdots", { 0x22EF, T::Inner } },
        { "ddots", { 0x22F1, T::Inner } }, { "dotsc", { 0x2026, T::Inner } }, { "dotsb", { 0x22EF, T::Inner } },
        // binary operators
        { "pm", { 0x00B1, T::Bin } }, { "mp", { 0x2213, T::Bin } }, { "times", { 0x00D7, T::Bin } },
        { "div", { 0x00F7, T::Bin } }, { "cdot", { 0x22C5, T::Bin } }, { "ast", { 0x2217, T::Bin } },
        { "star", { 0x22C6, T::Bin } }, { "circ", { 0x2218, T::Bin } }, { "bullet", { 0x2219, T::Bin } },
        { "cup", { 0x222A, T::Bin } }, { "cap", { 0x2229, T::Bin } }, { "wedge", { 0x2227, T::Bin } },
        { "land", { 0x2227, T::Bin } }, { "vee", { 0x2228, T::Bin } }, { "lor", { 0x2228, T::Bin } },
        { "oplus", { 0x2295, T::Bin } }, { "ominus", { 0x2296, T::Bin } }, { "otimes", { 0x2297, T::Bin } },
        { "oslash", { 0x2298, T::Bin } }, { "odot", { 0x2299, T::Bin } }, { "setminus", { 0x2216, T::Bin } },
        { "sqcup", { 0x2294, T::Bin } }, { "sqcap", { 0x2293, T::Bin } }, { "uplus", { 0x228E, T::Bin } },
        { "amalg", { 0x2A3F, T::Bin } }, { "wr", { 0x2240, T::Bin } }, { "diamond", { 0x22C4, T::Bin } },
        // relations
        { "leq", { 0x2264, T::Rel } }, { "le", { 0x2264, T::Rel } }, { "geq", { 0x2265, T::Rel } },
        { "ge", { 0x2265, T::Rel } }, { "neq", { 0x2260, T::Rel } }, { "ne", { 0x2260, T::Rel } },
        { "approx", { 0x2248, T::Rel } }, { "equiv", { 0x2261, T::Rel } }, { "sim", { 0x223C, T::Rel } },
        { "simeq", { 0x2243, T::Rel } }, { "cong", { 0x2245, T::Rel } }, { "propto", { 0x221D, T::Rel } },
        { "in", { 0x2208, T::Rel } }, { "notin", { 0x2209, T::Rel } }, { "ni", { 0x220B, T::Rel } },
        { "subset", { 0x2282, T::Rel } }, { "supset", { 0x2283, T::Rel } }, { "subseteq", { 0x2286, T::Rel } },
        { "supseteq", { 0x2287, T::Rel } }, { "subsetneq", { 0x228A, T::Rel } }, { "supsetneq", { 0x228B, T::Rel } },
        { "to", { 0x2192, T::Rel } }, { "rightarrow", { 0x2192, T::Rel } }, { "leftarrow", { 0x2190, T::Rel } },
        { "gets", { 0x2190, T::Rel } }, { "Rightarrow", { 0x21D2, T::Rel } }, { "Leftarrow", { 0x21D0, T::Rel } },
        { "Leftrightarrow", { 0x21D4, T::Rel } }, { "leftrightarrow", { 0x2194, T::Rel } },
        { "iff", { 0x27FA, T::Rel } }, { "implies", { 0x27F9, T::Rel } }, { "impliedby", { 0x27F8, T::Rel } },
        { "mapsto", { 0x21A6, T::Rel } }, { "longrightarrow", { 0x27F6, T::Rel } }, { "longleftarrow", { 0x27F5, T::Rel } },
        { "longleftrightarrow", { 0x27F7, T::Rel } }, { "Longrightarrow", { 0x27F9, T::Rel } },
        { "Longleftarrow", { 0x27F8, T::Rel } }, { "Longleftrightarrow", { 0x27FA, T::Rel } },
        { "longmapsto", { 0x27FC, T::Rel } }, { "uparrow", { 0x2191, T::Rel } }, { "downarrow", { 0x2193, T::Rel } },
        { "updownarrow", { 0x2195, T::Rel } }, { "Uparrow", { 0x21D1, T::Rel } }, { "Downarrow", { 0x21D3, T::Rel } },
        { "nearrow", { 0x2197, T::Rel } }, { "searrow", { 0x2198, T::Rel } }, { "hookrightarrow", { 0x21AA, T::Rel } },
        { "rightleftharpoons", { 0x21CC, T::Rel } }, { "ll", { 0x226A, T::Rel } }, { "gg", { 0x226B, T::Rel } },
        { "perp", { 0x27C2, T::Rel } }, { "parallel", { 0x2225, T::Rel } }, { "mid", { 0x2223, T::Rel } },
        { "nmid", { 0x2224, T::Rel } }, { "coloneqq", { 0x2254, T::Rel } }, { "approxeq", { 0x224A, T::Rel } },
        { "asymp", { 0x224D, T::Rel } }, { "doteq", { 0x2250, T::Rel } }, { "models", { 0x22A7, T::Rel } },
        { "vdash", { 0x22A2, T::Rel } }, { "dashv", { 0x22A3, T::Rel } }, { "prec", { 0x227A, T::Rel } },
        { "succ", { 0x227B, T::Rel } }, { "preceq", { 0x2AAF, T::Rel } }, { "succeq", { 0x2AB0, T::Rel } },
        { "lesssim", { 0x2272, T::Rel } }, { "gtrsim", { 0x2273, T::Rel } }, { "sqsubseteq", { 0x2291, T::Rel } },
        { "nleq", { 0x2270, T::Rel } }, { "ngeq", { 0x2271, T::Rel } }, { "leqslant", { 0x2A7D, T::Rel } },
        { "geqslant", { 0x2A7E, T::Rel } }, { "colon", { ':', T::Punct } },
    };
    return m;
}

struct BigOp { int cp; bool limits; };
const std::unordered_map<std::string, BigOp>& bigOps() {
    static const std::unordered_map<std::string, BigOp> m = {
        { "sum", { 0x2211, true } }, { "prod", { 0x220F, true } }, { "coprod", { 0x2210, true } },
        { "int", { 0x222B, false } }, { "iint", { 0x222C, false } }, { "iiint", { 0x222D, false } },
        { "oint", { 0x222E, false } }, { "oiint", { 0x222F, false } }, { "bigcup", { 0x22C3, true } },
        { "bigcap", { 0x22C2, true } }, { "bigvee", { 0x22C1, true } }, { "bigwedge", { 0x22C0, true } },
        { "bigoplus", { 0x2A01, true } }, { "bigotimes", { 0x2A02, true } }, { "bigodot", { 0x2A00, true } },
        { "biguplus", { 0x2A04, true } }, { "bigsqcup", { 0x2A06, true } },
    };
    return m;
}

const std::unordered_map<std::string, std::pair<const char*, bool>>& functions() {
    static const std::unordered_map<std::string, std::pair<const char*, bool>> m = {
        { "lim", { "lim", true } }, { "liminf", { "lim inf", true } }, { "limsup", { "lim sup", true } },
        { "max", { "max", true } }, { "min", { "min", true } }, { "sup", { "sup", true } }, { "inf", { "inf", true } },
        { "det", { "det", true } }, { "gcd", { "gcd", true } }, { "Pr", { "Pr", true } },
        { "argmax", { "arg max", true } }, { "argmin", { "arg min", true } },
        { "sin", { "sin", false } }, { "cos", { "cos", false } }, { "tan", { "tan", false } }, { "cot", { "cot", false } },
        { "sec", { "sec", false } }, { "csc", { "csc", false } }, { "arcsin", { "arcsin", false } },
        { "arccos", { "arccos", false } }, { "arctan", { "arctan", false } }, { "sinh", { "sinh", false } },
        { "cosh", { "cosh", false } }, { "tanh", { "tanh", false } }, { "coth", { "coth", false } },
        { "log", { "log", false } }, { "ln", { "ln", false } }, { "lg", { "lg", false } }, { "exp", { "exp", false } },
        { "deg", { "deg", false } }, { "dim", { "dim", false } }, { "ker", { "ker", false } }, { "arg", { "arg", false } },
        { "hom", { "hom", false } }, { "tr", { "tr", false } }, { "rank", { "rank", false } }, { "sgn", { "sgn", false } },
    };
    return m;
}

struct Acc { int cp; bool wide; };
const std::unordered_map<std::string, Acc>& accents() {
    static const std::unordered_map<std::string, Acc> m = {
        { "hat", { 0x0302, false } }, { "widehat", { 0x0302, true } }, { "tilde", { 0x0303, false } },
        { "widetilde", { 0x0303, true } }, { "bar", { 0x0304, false } }, { "vec", { 0x20D7, false } },
        { "dot", { 0x0307, false } }, { "ddot", { 0x0308, false } }, { "dddot", { 0x20DB, false } },
        { "check", { 0x030C, false } }, { "widecheck", { 0x030C, true } }, { "breve", { 0x0306, false } },
        { "acute", { 0x0301, false } }, { "grave", { 0x0300, false } }, { "mathring", { 0x030A, false } },
    };
    return m;
}

// Lowercase Greek names in Unicode order (U+03B1..U+03C9).
const char* const GREEK[] = { "alpha", "beta", "gamma", "delta", "varepsilon", "zeta", "eta", "theta", "iota", "kappa",
                              "lambda", "mu", "nu", "xi", "omicron", "pi", "rho", "varsigma", "sigma", "tau", "upsilon",
                              "varphi", "chi", "psi", "omega" };
const std::unordered_map<std::string, int>& greekExtra() {
    static const std::unordered_map<std::string, int> m = {
        { "epsilon", 0x1D716 }, { "vartheta", 0x1D717 }, { "varkappa", 0x1D718 }, { "phi", 0x1D719 },
        { "varrho", 0x1D71A }, { "varpi", 0x1D71B },
        { "Gamma", 0x0393 }, { "Delta", 0x0394 }, { "Theta", 0x0398 }, { "Lambda", 0x039B }, { "Xi", 0x039E },
        { "Pi", 0x03A0 }, { "Sigma", 0x03A3 }, { "Upsilon", 0x03A5 }, { "Phi", 0x03A6 }, { "Psi", 0x03A8 },
        { "Omega", 0x03A9 },
    };
    return m;
}

enum class Alpha { It, Rm, Bf, BfIt, Bb, Cal, Frak, Sf, Tt };

// A character in a math alphabet (Unicode Mathematical Alphanumeric Symbols).
int alphabet(int c, Alpha a) {
    const bool lower = c >= 'a' && c <= 'z', upper = c >= 'A' && c <= 'Z', digit = c >= '0' && c <= '9';
    const bool greek = c >= 0x3B1 && c <= 0x3C9, Greek = c >= 0x391 && c <= 0x3A9;
    auto exc = [c](std::initializer_list<std::pair<int, int>> l, int fallback) {
        for (const auto& p : l) if (p.first == c) return p.second;
        return fallback;
    };
    switch (a) {
        case Alpha::Rm: return c;
        case Alpha::It:
            if (c == 'h') return 0x210E;
            if (lower) return 0x1D44E + c - 'a';
            if (upper) return 0x1D434 + c - 'A';
            if (greek) return 0x1D6FC + c - 0x3B1;
            return c;
        case Alpha::Bf:
            if (lower) return 0x1D41A + c - 'a';
            if (upper) return 0x1D400 + c - 'A';
            if (digit) return 0x1D7CE + c - '0';
            if (greek) return 0x1D6C2 + c - 0x3B1;
            if (Greek) return 0x1D6A8 + c - 0x391;
            return c;
        case Alpha::BfIt:
            if (lower) return 0x1D482 + c - 'a';
            if (upper) return 0x1D468 + c - 'A';
            if (digit) return 0x1D7CE + c - '0';
            if (greek) return 0x1D736 + c - 0x3B1;
            if (Greek) return 0x1D71C + c - 0x391;
            return c;
        case Alpha::Bb:
            if (upper) return exc({ { 'C', 0x2102 }, { 'H', 0x210D }, { 'N', 0x2115 }, { 'P', 0x2119 }, { 'Q', 0x211A },
                                    { 'R', 0x211D }, { 'Z', 0x2124 } }, 0x1D538 + c - 'A');
            if (lower) return 0x1D552 + c - 'a';
            if (digit) return 0x1D7D8 + c - '0';
            return c;
        case Alpha::Cal:
            if (upper) return exc({ { 'B', 0x212C }, { 'E', 0x2130 }, { 'F', 0x2131 }, { 'H', 0x210B }, { 'I', 0x2110 },
                                    { 'L', 0x2112 }, { 'M', 0x2133 }, { 'R', 0x211B } }, 0x1D49C + c - 'A');
            if (lower) return exc({ { 'e', 0x212F }, { 'g', 0x210A }, { 'o', 0x2134 } }, 0x1D4B6 + c - 'a');
            return c;
        case Alpha::Frak:
            if (upper) return exc({ { 'C', 0x212D }, { 'H', 0x210C }, { 'I', 0x2111 }, { 'R', 0x211C }, { 'Z', 0x2128 } },
                                  0x1D504 + c - 'A');
            if (lower) return 0x1D51E + c - 'a';
            return c;
        case Alpha::Sf:
            if (upper) return 0x1D5A0 + c - 'A';
            if (lower) return 0x1D5BA + c - 'a';
            if (digit) return 0x1D7E2 + c - '0';
            return c;
        case Alpha::Tt:
            if (upper) return 0x1D670 + c - 'A';
            if (lower) return 0x1D68A + c - 'a';
            if (digit) return 0x1D7F6 + c - '0';
            return c;
    }
    return c;
}

bool parseColor(const std::string& s, Color& out) {
    static const std::unordered_map<std::string, unsigned> named = {
        { "red", 0xfc6255 }, { "green", 0x83c167 }, { "blue", 0x58c4dd }, { "yellow", 0xffd166 },
        { "orange", 0xff8c42 }, { "purple", 0x9a72ac }, { "pink", 0xd147bd }, { "teal", 0x5cd0b3 },
        { "gold", 0xf0ac5f }, { "maroon", 0xc55f73 }, { "gray", 0x888888 }, { "grey", 0x888888 },
        { "white", 0xffffff }, { "black", 0x000000 }, { "cyan", 0x00ffff }, { "magenta", 0xff00ff },
        { "brown", 0x8b5a2b },
    };
    if (auto it = named.find(s); it != named.end()) { out = mkColor(it->second); return true; }
    std::string h = s;
    if (!h.empty() && h[0] == '#') h = h.substr(1);
    if (h.size() != 6 || h.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) return false;
    out = mkColor((unsigned)std::strtoul(h.c_str(), nullptr, 16));
    return true;
}

// ------------------------------------------------------------------ syntax tree

enum class K { Char, Group, Frac, Sqrt, BigOp, Func, Delim, BigDelim, Accent, Space, Text, Array, Over, Under, Style, Phantom };
enum class Env { Matrix, Cases, Aligned, Gathered };

struct Node;
using List = std::vector<std::shared_ptr<Node>>;
struct Node {
    K k = K::Char;
    T type = T::Ord;
    int cp = 0, cp2 = 0;              // Char/BigOp/Accent/BigDelim: glyph; Delim: left, right
    List a, b;                        // children (Frac: num, den; Sqrt: body, index)
    List sup, sub;
    bool hasSup = false, hasSub = false;
    int limits = -1;                  // -1 default, 0 \nolimits, 1 \limits
    bool limitsDefault = false;
    std::string text;
    float em = 0;                     // Space width; BigDelim size
    int fracStyle = -1;               // -1 current, 0 display, 1 text
    bool rule = true;                 // Frac: false for \binom
    bool wide = false;                // Accent
    std::vector<std::vector<List>> rows;
    Env env = Env::Matrix;
    int phantom = 0;                  // 0 full, 1 h (no height), 2 v (no width)
    bool hasColor = false;
    Color color{};
    std::string id;
    int forceStyle = -1;              // Style: \displaystyle etc.
};
std::shared_ptr<Node> make(K k, T t = T::Ord) {
    auto n = std::make_shared<Node>();
    n->k = k;
    n->type = t;
    return n;
}

// ------------------------------------------------------------------ parser

class Parser {
public:
    explicit Parser(const std::string& s) : s_(s) {}

    List parse() {
        auto rows = parseRows(true);
        if (pos_ < s_.size()) fail("unexpected '" + s_.substr(pos_, 1) + "'");
        if (rows.size() == 1 && rows[0].size() == 1) return rows[0][0];
        bool cols = false;
        for (const auto& r : rows) cols = cols || r.size() > 1;
        auto n = make(K::Array);
        n->env = cols ? Env::Aligned : Env::Gathered;
        n->rows = std::move(rows);
        if (cols) prepareAligned(*n);
        return { n };
    }

private:
    std::string s_;
    size_t pos_ = 0;
    Alpha alpha_ = Alpha::It;

    [[noreturn]] void fail(const std::string& what) const {
        throw std::runtime_error("math::tex: " + what + " at offset " + std::to_string(pos_) + " in \"" + s_ + "\"");
    }
    bool end() const { return pos_ >= s_.size(); }
    void skipSpace() { while (!end() && std::isspace((unsigned char)s_[pos_])) pos_++; }
    int peekCp(size_t* len = nullptr) const {
        const unsigned char c = (unsigned char)s_[pos_];
        int cp = c, n = 1;
        if (c >= 0xF0) { cp = c & 0x07; n = 4; }
        else if (c >= 0xE0) { cp = c & 0x0F; n = 3; }
        else if (c >= 0xC0) { cp = c & 0x1F; n = 2; }
        for (int i = 1; i < n && pos_ + i < s_.size(); i++) cp = (cp << 6) | ((unsigned char)s_[pos_ + i] & 0x3F);
        if (len) *len = (size_t)n;
        return cp;
    }
    // The command name after a backslash at pos_ (letters, or one other character).
    std::string peekCommand() const {
        if (end() || s_[pos_] != '\\' || pos_ + 1 >= s_.size()) return "";
        size_t e = pos_ + 1;
        if (std::isalpha((unsigned char)s_[e])) while (e < s_.size() && std::isalpha((unsigned char)s_[e])) e++;
        else e++;
        return s_.substr(pos_ + 1, e - pos_ - 1);
    }
    std::string readCommand() {
        const std::string c = peekCommand();
        pos_ += 1 + c.size();
        return c;
    }
    bool atStop(bool rows) {
        skipSpace();
        if (end() || s_[pos_] == '}') return true;
        if (s_[pos_] == '&') { if (rows) return true; fail("'&' outside an environment"); }
        const std::string c = peekCommand();
        if (c == "\\") { if (rows) return true; fail("'\\\\' outside an environment"); }
        return c == "right" || c == "end";
    }

    std::vector<std::vector<List>> parseRows(bool topLevel) {
        std::vector<std::vector<List>> rows(1);
        while (true) {
            rows.back().push_back(parseList(true));
            skipSpace();
            if (!end() && s_[pos_] == '&') { pos_++; continue; }
            if (peekCommand() == "\\") { readCommand(); rows.emplace_back(); continue; }
            if (!end() && s_[pos_] == '}') fail("unbalanced '}'");
            if (peekCommand() == "right") fail("\\right without \\left");
            if (peekCommand() == "end" && topLevel) fail("\\end without \\begin");
            break;
        }
        if (rows.size() > 1 && rows.back().size() == 1 && rows.back()[0].empty()) rows.pop_back();   // trailing '\\'
        return rows;
    }

    // In aligned, a cell after '&' starts with an empty atom so "&= b" gets relation spacing.
    static void prepareAligned(Node& n) {
        for (auto& row : n.rows)
            for (size_t j = 1; j < row.size(); j += 2) row[j].insert(row[j].begin(), make(K::Group));
    }

    std::string rawArg() {
        skipSpace();
        if (end() || s_[pos_] != '{') fail("expected '{'");
        int depth = 0;
        const size_t start = pos_ + 1;
        for (; pos_ < s_.size(); pos_++) {
            if (s_[pos_] == '\\') { pos_++; continue; }
            if (s_[pos_] == '{') depth++;
            else if (s_[pos_] == '}' && --depth == 0) { pos_++; return s_.substr(start, pos_ - 1 - start); }
        }
        fail("unbalanced '{'");
    }
    List arg() {
        skipSpace();
        if (end()) fail("missing argument");
        if (s_[pos_] == '{') {
            pos_++;
            List l = parseList(false);
            if (end() || s_[pos_] != '}') fail("missing '}'");
            pos_++;
            return l;
        }
        auto n = atom();
        if (!n) fail("missing argument");
        return { n };
    }
    List argIn(Alpha a) {
        const Alpha old = alpha_;
        alpha_ = a;
        List l = arg();
        alpha_ = old;
        return l;
    }
    static std::shared_ptr<Node> group(List l, T t = T::Ord) {
        auto n = make(K::Group, t);
        n->a = std::move(l);
        return n;
    }

    List parseList(bool rows) {
        List out;
        while (!atStop(rows)) {
            const std::string c = peekCommand();
            if (c == "color" || c == "displaystyle" || c == "textstyle" || c == "scriptstyle" || c == "scriptscriptstyle") {
                readCommand();
                auto n = make(K::Style);
                if (c == "color") {
                    const std::string name = rawArg();
                    if (!parseColor(name, n->color)) fail("unknown colour '" + name + "'");
                    n->hasColor = true;
                } else n->forceStyle = c == "displaystyle" ? 0 : c == "textstyle" ? 1 : c == "scriptstyle" ? 2 : 3;
                n->a = parseList(rows);                 // the rest of the group
                out.push_back(n);
                break;
            }
            if (c == "limits" || c == "nolimits") {
                readCommand();
                if (out.empty() || (out.back()->k != K::BigOp && out.back()->k != K::Func)) fail("\\" + c + " must follow an operator");
                out.back()->limits = c == "limits" ? 1 : 0;
                continue;
            }
            auto n = atom();
            if (!n) continue;
            scripts(n, out);
            out.push_back(n);
        }
        return out;
    }

    // Scripts attach to n. Primes follow at full size (the font's prime glyphs are already
    // raised), and scripts after them attach to the primes: f'(x), f''^2.
    void scripts(std::shared_ptr<Node>& n, List& out) {
        while (true) {
            skipSpace();
            if (end()) return;
            const char ch = s_[pos_];
            if (ch == '^' || ch == '_') {
                pos_++;
                const bool up = ch == '^';
                if (up ? n->hasSup : n->hasSub) fail(up ? "double superscript" : "double subscript");
                (up ? n->sup : n->sub) = arg();
                (up ? n->hasSup : n->hasSub) = true;
            } else if (ch == '\'') {
                int count = 0;
                while (!end() && s_[pos_] == '\'') { pos_++; count++; }
                if (n->hasSup) fail("prime after a superscript");
                auto p = make(K::Char);
                p->cp = count == 1 ? 0x2032 : count == 2 ? 0x2033 : count == 3 ? 0x2034 : 0x2057;
                out.push_back(n);
                n = p;
            } else break;
        }
    }
    int leftDepth_ = 0;

    std::shared_ptr<Node> character(int cp) {
        auto n = make(K::Char);
        n->cp = cp;
        if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') ||
            (cp >= 0x391 && cp <= 0x3C9))
            n->cp = alphabet(cp, alpha_ == Alpha::It && cp >= '0' && cp <= '9' ? Alpha::Rm : alpha_);
        switch (cp) {
            case '+': n->type = T::Bin; break;
            case '-': n->cp = 0x2212; n->type = T::Bin; break;
            case '*': n->cp = 0x2217; n->type = T::Bin; break;
            case '=': case '<': case '>': case ':': n->type = T::Rel; break;
            case ',': case ';': n->type = T::Punct; break;
            case '(': case '[': n->type = T::Open; break;
            case ')': case ']': case '!': case '?': n->type = T::Close; break;
            default:
                for (const auto& [name, sym] : symbols())
                    if (sym.cp == cp && sym.type != T::Open && sym.type != T::Close) { n->type = sym.type; break; }
                for (const auto& [name, op] : bigOps())
                    if (op.cp == cp) { n->k = K::BigOp; n->type = T::Op; n->limitsDefault = op.limits; break; }
        }
        return n;
    }

    int delimiter() {
        skipSpace();
        if (end()) fail("missing delimiter");
        if (s_[pos_] != '\\') {
            size_t len;
            const int cp = peekCp(&len);
            pos_ += len;
            if (cp == '.') return 0;
            if (cp == '<') return 0x27E8;
            if (cp == '>') return 0x27E9;
            if (cp == '(' || cp == ')' || cp == '[' || cp == ']' || cp == '|' || cp == '/') return cp;
            fail("bad delimiter");
        }
        const std::string c = readCommand();
        if (c == "{" || c == "lbrace") return '{';
        if (c == "}" || c == "rbrace") return '}';
        if (c == "|") return 0x2016;
        if (auto it = symbols().find(c); it != symbols().end()) {
            const int cp = it->second.cp;
            if (cp == '|' || cp == 0x2016 || cp == '[' || cp == ']' || cp == 0x27E8 || cp == 0x27E9 || cp == 0x230A ||
                cp == 0x230B || cp == 0x2308 || cp == 0x2309 || cp == '{' || cp == '}' || cp == 0x2191 || cp == 0x2193 ||
                cp == 0x2195 || cp == 0x21D1 || cp == 0x21D3 || cp == '\\')
                return cp;
        }
        fail("bad delimiter \\" + c);
    }

    std::shared_ptr<Node> environment() {
        const std::string name = rawArg();
        auto n = make(K::Array, T::Inner);
        if (name == "matrix" || name == "pmatrix" || name == "bmatrix" || name == "Bmatrix" || name == "vmatrix" || name == "Vmatrix") {
            n->env = Env::Matrix;
            n->cp = name == "pmatrix" ? '(' : name == "bmatrix" ? '[' : name == "Bmatrix" ? '{' : name == "vmatrix" ? '|' : name == "Vmatrix" ? 0x2016 : 0;
            n->cp2 = name == "pmatrix" ? ')' : name == "bmatrix" ? ']' : name == "Bmatrix" ? '}' : name == "vmatrix" ? '|' : name == "Vmatrix" ? 0x2016 : 0;
        } else if (name == "cases") { n->env = Env::Cases; n->cp = '{'; }
        else if (name == "aligned" || name == "align" || name == "align*" || name == "split") n->env = Env::Aligned;
        else if (name == "gathered" || name == "gather" || name == "gather*") n->env = Env::Gathered;
        else fail("unknown environment '" + name + "'");
        n->rows = parseRows(false);
        if (peekCommand() != "end") fail("missing \\end{" + name + "}");
        readCommand();
        if (rawArg() != name) fail("\\begin{" + name + "} ended by a different \\end");
        if (n->env == Env::Aligned) prepareAligned(*n);
        if (n->env == Env::Matrix && !n->cp) n->type = T::Ord;
        return n;
    }

    std::shared_ptr<Node> atom() {
        skipSpace();
        if (end()) return nullptr;
        const char ch = s_[pos_];
        if (ch == '{') return group(arg());
        if (ch == '^' || ch == '_' || ch == '\'') return make(K::Group);   // script on nothing
        if (ch == '~') { pos_++; auto n = make(K::Space, T::None); n->em = 0.333f; return n; }
        if (ch != '\\') {
            size_t len;
            const int cp = peekCp(&len);
            pos_ += len;
            return character(cp);
        }
        const std::string c = readCommand();
        if (c.empty()) fail("lone backslash");
        if (auto it = symbols().find(c); it != symbols().end()) {
            auto n = make(K::Char, it->second.type);
            n->cp = it->second.cp;
            return n;
        }
        for (int i = 0; i < 25; i++)
            if (c == GREEK[i]) { auto n = make(K::Char); n->cp = alphabet(0x3B1 + i, alpha_ == Alpha::Rm ? Alpha::Rm : alpha_ == Alpha::Bf ? Alpha::Bf : alpha_ == Alpha::BfIt ? Alpha::BfIt : Alpha::It); return n; }
        if (auto it = greekExtra().find(c); it != greekExtra().end()) { auto n = make(K::Char); n->cp = it->second; return n; }
        if (auto it = bigOps().find(c); it != bigOps().end()) {
            auto n = make(K::BigOp, T::Op);
            n->cp = it->second.cp;
            n->limitsDefault = it->second.limits;
            return n;
        }
        if (auto it = functions().find(c); it != functions().end()) {
            auto n = make(K::Func, T::Op);
            n->text = it->second.first;
            n->limitsDefault = it->second.second;
            return n;
        }
        if (auto it = accents().find(c); it != accents().end()) {
            auto n = make(K::Accent);
            n->cp = it->second.cp;
            n->wide = it->second.wide;
            n->a = arg();
            return n;
        }
        static const std::unordered_map<std::string, float> spaces = {
            { ",", 3 / 18.f }, { ":", 4 / 18.f }, { ">", 4 / 18.f }, { ";", 5 / 18.f }, { "!", -3 / 18.f },
            { " ", 0.333f }, { "quad", 1.f }, { "qquad", 2.f }, { "thinspace", 3 / 18.f }, { "enspace", 0.5f },
        };
        if (auto it = spaces.find(c); it != spaces.end()) { auto n = make(K::Space, T::None); n->em = it->second; return n; }
        static const std::string literal = "{}%&#$_";
        if (c.size() == 1 && literal.find(c[0]) != std::string::npos) {
            auto n = character((unsigned char)c[0]);
            n->type = c == "{" ? T::Open : c == "}" ? T::Close : T::Ord;
            return n;
        }
        if (c == "|") { auto n = make(K::Char); n->cp = 0x2016; return n; }
        if (c == "frac" || c == "dfrac" || c == "tfrac" || c == "cfrac") {
            auto n = make(K::Frac, T::Inner);
            n->fracStyle = c == "dfrac" || c == "cfrac" ? 0 : c == "tfrac" ? 1 : -1;
            n->a = arg();
            n->b = arg();
            return n;
        }
        if (c == "binom" || c == "dbinom" || c == "tbinom") {
            auto n = make(K::Frac, T::Inner);
            n->rule = false;
            n->fracStyle = c == "dbinom" ? 0 : c == "tbinom" ? 1 : -1;
            n->a = arg();
            n->b = arg();
            return n;
        }
        if (c == "sqrt") {
            auto n = make(K::Sqrt);
            skipSpace();
            if (!end() && s_[pos_] == '[') {
                pos_++;
                const size_t close = s_.find(']', pos_);
                if (close == std::string::npos) fail("missing ']'");
                Parser inner(s_.substr(pos_, close - pos_));
                inner.alpha_ = alpha_;
                n->b = inner.parseList(false);
                pos_ = close + 1;
            }
            n->a = arg();
            return n;
        }
        if (c == "left") {
            auto n = make(K::Delim, T::Inner);
            n->cp = delimiter();
            leftDepth_++;
            n->a = parseList(false);
            leftDepth_--;
            if (peekCommand() != "right") fail("\\left without \\right");
            readCommand();
            n->cp2 = delimiter();
            return n;
        }
        static const std::unordered_map<std::string, std::pair<float, T>> bigs = {
            { "big", { 1.2f, T::Ord } }, { "Big", { 1.8f, T::Ord } }, { "bigg", { 2.4f, T::Ord } }, { "Bigg", { 3.0f, T::Ord } },
            { "bigl", { 1.2f, T::Open } }, { "Bigl", { 1.8f, T::Open } }, { "biggl", { 2.4f, T::Open } }, { "Biggl", { 3.0f, T::Open } },
            { "bigr", { 1.2f, T::Close } }, { "Bigr", { 1.8f, T::Close } }, { "biggr", { 2.4f, T::Close } }, { "Biggr", { 3.0f, T::Close } },
            { "bigm", { 1.2f, T::Rel } }, { "Bigm", { 1.8f, T::Rel } }, { "biggm", { 2.4f, T::Rel } }, { "Biggm", { 3.0f, T::Rel } },
        };
        if (auto it = bigs.find(c); it != bigs.end()) {
            auto n = make(K::BigDelim, it->second.second);
            n->cp = delimiter();
            n->em = it->second.first;
            return n;
        }
        if (c == "middle") {
            if (!leftDepth_) fail("\\middle outside \\left ... \\right");
            auto n = make(K::BigDelim, T::Ord);
            n->cp = delimiter();
            n->em = -1;                                 // sized with the enclosing \left ... \right
            return n;
        }
        if (c == "overline" || c == "underline") { auto n = make(c == "overline" ? K::Over : K::Under); n->a = arg(); return n; }
        if (c == "text" || c == "textrm" || c == "mbox" || c == "textnormal") { auto n = make(K::Text); n->text = rawArg(); return n; }
        if (c == "operatorname" || c == "operatornamewithlimits") {
            bool star = false;
            if (!end() && s_[pos_] == '*') { pos_++; star = true; }
            auto n = make(K::Func, T::Op);
            n->text = rawArg();
            n->limitsDefault = star || c == "operatornamewithlimits";
            return n;
        }
        static const std::unordered_map<std::string, Alpha> fonts = {
            { "mathrm", Alpha::Rm }, { "mathit", Alpha::It }, { "mathbf", Alpha::Bf }, { "boldsymbol", Alpha::BfIt },
            { "bm", Alpha::BfIt }, { "mathbb", Alpha::Bb }, { "mathcal", Alpha::Cal }, { "mathscr", Alpha::Cal },
            { "mathfrak", Alpha::Frak }, { "mathsf", Alpha::Sf }, { "mathtt", Alpha::Tt },
        };
        if (auto it = fonts.find(c); it != fonts.end()) return group(argIn(it->second));
        if (c == "textcolor") {
            auto n = make(K::Style);
            const std::string name = rawArg();
            if (!parseColor(name, n->color)) fail("unknown colour '" + name + "'");
            n->hasColor = true;
            n->a = arg();
            return n;
        }
        if (c == "id") {
            auto n = make(K::Style);
            n->id = rawArg();
            if (n->id.empty()) fail("empty \\id");
            n->a = arg();
            return n;
        }
        if (c == "phantom" || c == "hphantom" || c == "vphantom") {
            auto n = make(K::Phantom);
            n->phantom = c == "phantom" ? 0 : c == "hphantom" ? 1 : 2;
            n->a = arg();
            return n;
        }
        if (c == "mathop" || c == "mathbin" || c == "mathrel" || c == "mathopen" || c == "mathclose" || c == "mathpunct" || c == "mathord" || c == "mathinner") {
            const T t = c == "mathop" ? T::Op : c == "mathbin" ? T::Bin : c == "mathrel" ? T::Rel : c == "mathopen" ? T::Open
                      : c == "mathclose" ? T::Close : c == "mathpunct" ? T::Punct : c == "mathinner" ? T::Inner : T::Ord;
            return group(arg(), t);
        }
        if (c == "begin") return environment();
        if (c == "bmod") { auto n = make(K::Func, T::Bin); n->text = "mod"; return n; }
        if (c == "pmod") {
            auto n = make(K::Group);
            auto sp = make(K::Space, T::None);
            sp->em = 1;
            auto mod = make(K::Text);
            mod->text = "(mod ";
            auto close = make(K::Text);
            close->text = ")";
            n->a = { sp, mod };
            List l = arg();
            n->a.insert(n->a.end(), l.begin(), l.end());
            n->a.push_back(close);
            return n;
        }
        fail("unknown command \\" + c);
    }
};

// ------------------------------------------------------------------ boxes

struct Item {
    int glyph = -1;                   // -1: rule
    float x = 0, y = 0, s = 1;        // glyph: baseline origin, em scale; rule: top-left
    float w = 0, h = 0;               // rule size
    bool hasColor = false;
    Color color{};
    std::vector<std::string> parts;
};
struct Box {
    float w = 0, h = 0, d = 0, ic = 0;
    int glyph = -1;                   // a lone glyph (for accents and scripts)
    std::vector<Item> items;
};
void put(Box& dst, const Box& src, float dx, float dy) {
    for (Item it : src.items) {
        it.x += dx;
        it.y += dy;
        dst.items.push_back(std::move(it));
    }
}
void shiftY(Box& b, float dy) {
    for (Item& it : b.items) it.y += dy;
    b.h -= dy;
    b.d += dy;
}

struct St { int s = 0; bool cramped = false; };   // 0 display, 1 text, 2 script, 3 scriptscript
St supStyle(St st) { return { st.s <= 1 ? 2 : 3, st.cramped }; }
St subStyle(St st) { return { st.s <= 1 ? 2 : 3, true }; }
St numStyle(St st) { return { std::min(st.s + 1, 3), st.cramped }; }
St denStyle(St st) { return { std::min(st.s + 1, 3), true }; }
St cramp(St st) { return { st.s, true }; }

// TeX's inter-atom spacing in mu; negative: only in display and text styles.
const int SPACING[8][8] = {
    //  Ord Op Bin Rel Open Close Punct Inner
    { 0, 3, -4, -5, 0, 0, 0, -3 },      // Ord
    { 3, 3, 0, -5, 0, 0, 0, -3 },       // Op
    { -4, -4, 0, 0, -4, 0, 0, -4 },     // Bin
    { -5, -5, 0, 0, -5, 0, 0, -5 },     // Rel
    { 0, 0, 0, 0, 0, 0, 0, 0 },         // Open
    { 0, 3, -4, -5, 0, 0, 0, -3 },      // Close
    { -3, -3, 0, -3, -3, -3, -3, -3 },  // Punct
    { -3, 3, -4, -5, -3, 0, -3, -3 },   // Inner
};

class Layout {
public:
    Layout(const Font& f) : f_(f) {}

    Box list(const List& l, St st) {
        struct A { Box b; T t; };
        std::vector<A> atoms;
        for (const auto& n : l) {
            if (n->k == K::Space) {
                Box b;
                b.w = n->em * scale(st);
                atoms.push_back({ std::move(b), T::None });
                continue;
            }
            atoms.push_back({ node(*n, st), n->type });
        }
        // A binary operator with nothing to operate on is ordinary (TeX rules 5 and 6).
        T prev = T::None;
        for (size_t i = 0; i < atoms.size(); i++) {
            if (atoms[i].t == T::None) continue;
            if (atoms[i].t == T::Bin) {
                T next = T::None;
                for (size_t j = i + 1; j < atoms.size() && next == T::None; j++) next = atoms[j].t;
                if (prev == T::None || prev == T::Bin || prev == T::Op || prev == T::Rel || prev == T::Open || prev == T::Punct ||
                    next == T::Rel || next == T::Close || next == T::Punct || next == T::None)
                    atoms[i].t = T::Ord;
            }
            prev = atoms[i].t;
        }
        Box out;
        float x = 0;
        prev = T::None;
        for (A& a : atoms) {
            if (a.t != T::None && prev != T::None) {
                const int mu = SPACING[(int)prev][(int)a.t];
                if (mu > 0 || (mu < 0 && st.s <= 1)) x += std::abs(mu) / 18.f * scale(st);
            }
            put(out, a.b, x, 0);
            x += a.b.w;
            out.h = std::max(out.h, a.b.h);
            out.d = std::max(out.d, a.b.d);
            if (a.t != T::None) prev = a.t;
        }
        out.w = x;
        if (atoms.size() == 1 && atoms[0].t != T::None) { out.glyph = atoms[0].b.glyph; out.ic = atoms[0].b.ic; }
        return out;
    }

private:
    const Font& f_;
    float middle_ = 0;                // the size of \middle delimiters in the current \left ... \right
    int middles_ = 0;

    float scale(St st) const { return st.s <= 1 ? 1.f : st.s == 2 ? f_.scriptScale : f_.scriptScriptScale; }
    float k(Mc c, St st) const { return f_.k[c] * scale(st); }

    int glyphOf(int cp) const {
        int g = f_.glyph(cp);
        if (!g) {
            char buf[64];
            std::snprintf(buf, sizeof buf, "math::tex: the font has no glyph for U+%04X", cp);
            throw std::runtime_error(buf);
        }
        return g;
    }
    Box glyphBox(int g, float s) const {
        const Metrics m = f_.metrics(g);
        Box b;
        b.w = m.advance * s;
        b.h = std::max(0.f, m.y1 * s);
        b.d = std::max(0.f, -m.y0 * s);
        b.ic = f_.italic(g) * s;
        b.glyph = g;
        Item it;
        it.glyph = g;
        it.s = s;
        b.items.push_back(it);
        return b;
    }
    static Item rule(float x, float y, float w, float h) {
        Item it;
        it.x = x; it.y = y; it.w = w; it.h = h;
        return it;
    }
    void centerOnAxis(Box& b, St st) const {
        const float axis = k(AxisHeight, st);
        shiftY(b, (b.h - b.d) / 2 - axis);
    }

    // A delimiter at least `total` tall (height + depth), centred on the axis.
    Box delimiter(int cp, float total, St st) const {
        const float s = scale(st);
        if (cp == 0) { Box b; b.w = 0.12f * s; return b; }
        const int g = glyphOf(cp);
        Box b = glyphBox(g, s);
        if (const Font::Construction* c = f_.vertical(g)) {
            bool done = false;
            for (const auto& v : c->variants)
                if (v.advance * s >= total) { b = glyphBox(v.glyph, s); done = true; break; }
            if (!done && !c->parts.empty()) { b = assembly(c->parts, total, s); done = true; }
            if (!done && !c->variants.empty()) b = glyphBox(c->variants.back().glyph, s);
        }
        b.glyph = -1;
        b.ic = 0;
        centerOnAxis(b, st);
        return b;
    }

    // Stacks a vertical glyph assembly (parts bottom to top) to reach `total`.
    Box assembly(const std::vector<Font::Part>& parts, float total, float s) const {
        const float target = total / s, minO = f_.minConnectorOverlap;
        std::vector<Font::Part> seq;
        for (int reps = 0; reps < 64; reps++) {
            seq.clear();
            for (const auto& p : parts) {
                if (p.extender) for (int r = 0; r < reps; r++) seq.push_back(p);
                else seq.push_back(p);
            }
            float full = 0;
            for (const auto& p : seq) full += p.full;
            if (full - minO * (seq.size() - 1) >= target || seq.empty()) break;
        }
        float full = 0, maxO = 1e9f;
        for (size_t i = 0; i < seq.size(); i++) {
            full += seq[i].full;
            if (i) maxO = std::min(maxO, std::min(seq[i - 1].end, seq[i].start));
        }
        const size_t joins = seq.empty() ? 0 : seq.size() - 1;
        const float o = joins ? std::clamp((full - target) / joins, minO, std::max(minO, maxO)) : 0;
        Box b;
        float cursor = 0;                               // em from the bottom, upwards
        for (const auto& p : seq) {
            const Metrics m = f_.metrics(p.glyph);
            Item it;
            it.glyph = p.glyph;
            it.s = s;
            it.y = -(cursor - m.y0) * s;
            b.items.push_back(it);
            b.w = std::max(b.w, m.advance * s);
            cursor += p.full - o;
        }
        b.h = (cursor + o) * s;
        return b;
    }

    bool limits(const Node& n, St st) const { return n.limits == 1 || (n.limits == -1 && n.limitsDefault && st.s == 0); }

    Box node(const Node& n, St st) {
        Box b = nucleus(n, st);
        if (!n.hasSup && !n.hasSub) {
            if (n.k == K::Char) b.w += b.ic;           // italic correction when no script follows
            return b;
        }
        if ((n.k == K::BigOp || n.k == K::Func) && limits(n, st)) return limitsBox(b, n, st);
        return scriptsBox(b, n, st);
    }

    Box text(const std::string& s, St st) const {
        Box b;
        const float sc = scale(st);
        float x = 0;
        for (size_t i = 0; i < s.size();) {
            const unsigned char c = (unsigned char)s[i];
            int cp = c, len = 1;
            if (c >= 0xF0) { cp = c & 0x07; len = 4; }
            else if (c >= 0xE0) { cp = c & 0x0F; len = 3; }
            else if (c >= 0xC0) { cp = c & 0x1F; len = 2; }
            for (int j = 1; j < len && i + j < s.size(); j++) cp = (cp << 6) | ((unsigned char)s[i + j] & 0x3F);
            i += (size_t)len;
            const int g = glyphOf(cp);
            Box gb = glyphBox(g, sc);
            if (cp != ' ') put(b, gb, x, 0);
            x += gb.w;
            b.h = std::max(b.h, gb.h);
            b.d = std::max(b.d, gb.d);
        }
        b.w = x;
        return b;
    }

    Box nucleus(const Node& n, St st) {
        const float s = scale(st);
        switch (n.k) {
            case K::Char: return glyphBox(glyphOf(n.cp), s);
            case K::Group: return list(n.a, st);
            case K::Space: { Box b; b.w = n.em * s; return b; }
            case K::Text: return text(n.text, st);
            case K::Func: { Box b = text(n.text, st); b.glyph = -1; return b; }
            case K::BigOp: {
                int g = glyphOf(n.cp);
                if (st.s == 0)
                    if (const Font::Construction* c = f_.vertical(g)) {
                        for (const auto& v : c->variants) {
                            g = v.glyph;
                            if (v.advance >= f_.displayOperatorMinHeight) break;
                        }
                    }
                Box b = glyphBox(g, s);
                centerOnAxis(b, st);
                b.glyph = -1;                           // scripts drop as on a box (TeX rule 18a)
                return b;
            }
            case K::Style: {
                Box b = list(n.a, n.forceStyle >= 0 ? St{ n.forceStyle, st.cramped } : st);
                for (Item& it : b.items) {
                    if (n.hasColor && !it.hasColor) { it.hasColor = true; it.color = n.color; }
                    if (!n.id.empty()) it.parts.insert(it.parts.begin(), n.id);
                }
                return b;
            }
            case K::Phantom: {
                Box b = list(n.a, st);
                b.items.clear();
                b.glyph = -1;
                if (n.phantom == 1) b.h = b.d = 0;
                if (n.phantom == 2) b.w = 0;
                return b;
            }
            case K::Frac: return fraction(n, st);
            case K::Sqrt: return radical(n, st);
            case K::Delim: {
                const float outerMiddle = middle_;
                const int middles = middles_;
                middle_ = 0;
                Box inner = list(n.a, st);
                const float axis = k(AxisHeight, st);
                const float delta = std::max(inner.h - axis, inner.d + axis);
                const float total = std::max(2 * delta * 0.901f, 2 * delta - 0.5f * s);
                if (middles_ != middles) {              // \middle: lay out again with them at full size
                    middle_ = total;
                    inner = list(n.a, st);
                }
                middle_ = outerMiddle;
                Box l = delimiter(n.cp, total, st), r = delimiter(n.cp2, total, st), b;
                put(b, l, 0, 0);
                put(b, inner, l.w, 0);
                put(b, r, l.w + inner.w, 0);
                b.w = l.w + inner.w + r.w;
                b.h = std::max({ l.h, inner.h, r.h });
                b.d = std::max({ l.d, inner.d, r.d });
                return b;
            }
            case K::BigDelim:
                if (n.em < 0) { middles_++; return delimiter(n.cp, middle_, st); }
                return delimiter(n.cp, n.em * s, st);
            case K::Accent: return accent(n, st);
            case K::Over: case K::Under: {
                Box body = list(n.a, n.k == K::Over ? cramp(st) : st), b;
                put(b, body, 0, 0);
                b.w = body.w;
                if (n.k == K::Over) {
                    const float t = k(OverbarRuleThickness, st), gap = k(OverbarVerticalGap, st);
                    b.items.push_back(rule(0, -(body.h + gap + t), body.w, t));
                    b.h = body.h + gap + t + k(OverbarExtraAscender, st);
                    b.d = body.d;
                } else {
                    const float t = k(UnderbarRuleThickness, st), gap = k(UnderbarVerticalGap, st);
                    b.items.push_back(rule(0, body.d + gap, body.w, t));
                    b.h = body.h;
                    b.d = body.d + gap + t + k(UnderbarExtraDescender, st);
                }
                return b;
            }
            case K::Array: return array(n, st);
        }
        return {};
    }

    Box scriptsBox(const Box& base, const Node& n, St st) {
        const St ps = supStyle(st), qs = subStyle(st);
        const Box p = n.hasSup ? list(n.sup, ps) : Box{}, q = n.hasSub ? list(n.sub, qs) : Box{};
        const bool ch = base.glyph >= 0;
        float u = 0, v = 0;
        if (n.hasSup) {
            u = std::max({ ch ? 0.f : base.h - k(SuperscriptBaselineDropMax, st),
                           k(st.cramped ? SuperscriptShiftUpCramped : SuperscriptShiftUp, st),
                           p.d + k(SuperscriptBottomMin, st) });
        }
        if (n.hasSub) {
            v = std::max(ch ? 0.f : base.d + k(SubscriptBaselineDropMin, st), k(SubscriptShiftDown, st));
            if (!n.hasSup) v = std::max(v, q.h - k(SubscriptTopMax, st));
        }
        if (n.hasSup && n.hasSub) {
            const float gap = (u - p.d) - (q.h - v), min = k(SubSuperscriptGapMin, st);
            if (gap < min) {
                v += min - gap;
                const float bottom = u - p.d, maxBottom = k(SuperscriptBottomMaxWithSubscript, st);
                if (bottom < maxBottom) {
                    const float lift = std::min(maxBottom - bottom, v - (q.h - k(SubscriptTopMax, st)));
                    if (lift > 0) { u += lift; v -= lift; }
                }
            }
        }
        const float xSup = base.w + base.ic, xSub = base.w;   // italic correction outside the advance (OpenType)
        Box b;
        put(b, base, 0, 0);
        if (n.hasSup) put(b, p, xSup, -u);
        if (n.hasSub) put(b, q, xSub, v);
        const float space = k(SpaceAfterScript, st);
        b.w = std::max(n.hasSup ? xSup + p.w : 0.f, n.hasSub ? xSub + q.w : 0.f) + space;
        b.w = std::max(b.w, base.w);
        b.h = std::max(base.h, n.hasSup ? u + p.h : 0.f);
        b.d = std::max(base.d, n.hasSub ? v + q.d : 0.f);
        return b;
    }

    Box limitsBox(const Box& base, const Node& n, St st) {
        const Box p = n.hasSup ? list(n.sup, supStyle(st)) : Box{}, q = n.hasSub ? list(n.sub, subStyle(st)) : Box{};
        const float W = std::max({ base.w, p.w, q.w });
        Box b;
        put(b, base, (W - base.w) / 2, 0);
        b.h = base.h;
        b.d = base.d;
        if (n.hasSup) {
            const float rise = std::max(k(UpperLimitBaselineRiseMin, st), k(UpperLimitGapMin, st) + p.d);
            put(b, p, (W - p.w) / 2 + base.ic / 2, -base.h - rise);
            b.h = base.h + rise + p.h;
        }
        if (n.hasSub) {
            const float drop = std::max(k(LowerLimitBaselineDropMin, st), k(LowerLimitGapMin, st) + q.h);
            put(b, q, (W - q.w) / 2 - base.ic / 2, base.d + drop);
            b.d = base.d + drop + q.d;
        }
        b.w = W;
        return b;
    }

    Box fraction(const Node& n, St outer) {
        const St st = n.fracStyle == 0 ? St{ 0, outer.cramped } : n.fracStyle == 1 ? St{ std::max(1, outer.s == 0 ? 1 : outer.s), outer.cramped } : outer;
        const Box num = list(n.a, numStyle(st)), den = list(n.b, denStyle(st));
        const bool disp = st.s == 0;
        const float axis = k(AxisHeight, st);
        float u, v, t = 0;
        if (n.rule) {
            t = k(FractionRuleThickness, st);
            u = k(disp ? FractionNumeratorDisplayStyleShiftUp : FractionNumeratorShiftUp, st);
            v = k(disp ? FractionDenominatorDisplayStyleShiftDown : FractionDenominatorShiftDown, st);
            const float ng = k(disp ? FractionNumDisplayStyleGapMin : FractionNumeratorGapMin, st);
            const float dg = k(disp ? FractionDenomDisplayStyleGapMin : FractionDenominatorGapMin, st);
            if ((u - num.d) - (axis + t / 2) < ng) u = ng + axis + t / 2 + num.d;
            if ((axis - t / 2) - (den.h - v) < dg) v = dg + den.h - axis + t / 2;
        } else {
            u = k(disp ? StackTopDisplayStyleShiftUp : StackTopShiftUp, st);
            v = k(disp ? StackBottomDisplayStyleShiftDown : StackBottomShiftDown, st);
            const float min = k(disp ? StackDisplayStyleGapMin : StackGapMin, st), gap = (u - num.d) - (den.h - v);
            if (gap < min) { u += (min - gap) / 2; v += (min - gap) / 2; }
        }
        const float pad = n.rule ? 0.12f * scale(st) : 0, W = std::max(num.w, den.w);
        Box b;
        put(b, num, pad + (W - num.w) / 2, -u);
        put(b, den, pad + (W - den.w) / 2, v);
        if (n.rule) b.items.push_back(rule(pad, -(axis + t / 2), W, t));
        b.w = W + 2 * pad;
        b.h = std::max(u + num.h, axis + t / 2);
        b.d = std::max(v + den.d, t / 2 - axis);
        if (n.rule) return b;
        // \binom: in parentheses sized to the stack
        const float delta = std::max(b.h - axis, b.d + axis);
        const float total = std::max(2 * delta * 0.901f, 2 * delta - 0.5f * scale(st));
        Box l = delimiter('(', total, st), r = delimiter(')', total, st), out;
        put(out, l, 0, 0);
        put(out, b, l.w, 0);
        put(out, r, l.w + b.w, 0);
        out.w = l.w + b.w + r.w;
        out.h = std::max({ l.h, b.h, r.h });
        out.d = std::max({ l.d, b.d, r.d });
        return out;
    }

    Box radical(const Node& n, St st) {
        const Box body = list(n.a, cramp(st));
        const float t = k(RadicalRuleThickness, st);
        float gap = k(st.s == 0 ? RadicalDisplayStyleVerticalGap : RadicalVerticalGap, st);
        const float target = body.h + body.d + gap + t;
        const float s = scale(st);
        const int g0 = glyphOf(0x221A);
        Box rad = glyphBox(g0, s);
        if (const Font::Construction* c = f_.vertical(g0)) {
            bool done = false;
            for (const auto& v : c->variants)
                if (v.advance * s >= target) { rad = glyphBox(v.glyph, s); done = true; break; }
            if (!done && !c->parts.empty()) rad = assembly(c->parts, target, s);
            else if (!done && !c->variants.empty()) rad = glyphBox(c->variants.back().glyph, s);
        }
        const float total = rad.h + rad.d;
        if (total > target) gap += (total - target) / 2;
        const float top = body.h + gap + t;            // the rule's top above the baseline
        shiftY(rad, rad.h - top);                      // radical's top at the rule's top
        Box b;
        float x = 0;
        if (!n.b.empty()) {                            // index
            const Box idx = list(n.b, St{ 3, false });
            const float before = k(RadicalKernBeforeDegree, st), after = k(RadicalKernAfterDegree, st);
            const float raise = f_.radicalDegreeBottomRaise * (rad.h + rad.d);
            put(b, idx, before, rad.d - raise);
            x = std::max(0.f, before + idx.w + after);
            b.h = std::max(b.h, raise - rad.d + idx.h);
        }
        put(b, rad, x, 0);
        x += rad.w;
        put(b, body, x, 0);
        b.items.push_back(rule(x, -top, body.w, t));
        b.w = x + body.w;
        b.h = std::max(b.h, top + k(RadicalExtraAscender, st));
        b.d = std::max(body.d, rad.d);
        return b;
    }

    Box accent(const Node& n, St st) {
        const float s = scale(st);
        Box base = list(n.a, cramp(st));
        if (n.a.size() == 1 && n.a[0]->k == K::Char && !n.a[0]->hasSup && !n.a[0]->hasSub) base.w -= base.ic;   // attach over the letter itself
        int g = glyphOf(n.cp);
        if (n.wide || base.w > f_.metrics(g).advance * s * 1.5f)
            if (const Font::Construction* c = f_.horizontal(g))
                for (const auto& v : c->variants) {
                    if (v.advance * s > base.w * 1.1f) break;
                    g = v.glyph;
                }
        const Box acc = glyphBox(g, s);
        float baseX = base.w / 2, accX = acc.w / 2;
        float tx;
        if (base.glyph >= 0 && f_.topAccent(base.glyph, tx)) baseX = tx * s;
        if (f_.topAccent(g, tx)) accX = tx * s;
        const float lift = std::max(0.f, base.h - k(AccentBaseHeight, st));
        Box b;
        put(b, base, 0, 0);
        put(b, acc, baseX - accX, -lift);
        b.w = base.w + (n.a.size() == 1 && n.a[0]->k == K::Char ? base.ic : 0);
        b.h = std::max(base.h, acc.h + lift);
        b.d = base.d;
        return b;
    }

    Box array(const Node& n, St st) {
        const St cellSt = (n.env == Env::Aligned || n.env == Env::Gathered) ? St{ 0, st.cramped } : St{ std::max(1, st.s), st.cramped };
        const float s = scale(st);
        size_t cols = 0;
        for (const auto& r : n.rows) cols = std::max(cols, r.size());
        std::vector<std::vector<Box>> cells;
        std::vector<float> colW(cols, 0), rowH, rowD;
        const float strutH = 0.85f * s, strutD = 0.35f * s;
        const float jot = (n.env == Env::Aligned || n.env == Env::Gathered) ? 0.3f * s : 0.f;
        for (const auto& r : n.rows) {
            cells.emplace_back();
            float h = strutH, d = strutD;
            for (size_t j = 0; j < r.size(); j++) {
                cells.back().push_back(list(r[j], cellSt));
                const Box& c = cells.back().back();
                colW[j] = std::max(colW[j], c.w);
                h = std::max(h, c.h);
                d = std::max(d, c.d);
            }
            rowH.push_back(h);
            rowD.push_back(d + jot);
        }
        if (!rowD.empty()) rowD.back() -= jot;
        auto gapAfter = [&](size_t j) {
            if (n.env == Env::Aligned) return j % 2 == 1 ? 2.0f * s : 0.f;
            return 1.0f * s;
        };
        std::vector<float> colX(cols, 0);
        float W = 0;
        for (size_t j = 0; j < cols; j++) {
            colX[j] = W;
            W += colW[j] + (j + 1 < cols ? gapAfter(j) : 0);
        }
        float H = 0;
        for (size_t i = 0; i < rowH.size(); i++) H += rowH[i] + rowD[i];
        const float axis = k(AxisHeight, st), top = -axis - H / 2;
        Box body;
        float y = top;
        for (size_t i = 0; i < cells.size(); i++) {
            y += rowH[i];
            for (size_t j = 0; j < cells[i].size(); j++) {
                const Box& c = cells[i][j];
                float x = colX[j];
                if (n.env == Env::Matrix || n.env == Env::Gathered) x += (colW[j] - c.w) / 2;
                else if (n.env == Env::Aligned && j % 2 == 0) x += colW[j] - c.w;
                put(body, c, x, y);
            }
            y += rowD[i];
        }
        body.w = W;
        body.h = axis + H / 2;
        body.d = H / 2 - axis;
        if (!n.cp && !n.cp2) return body;
        const float delta = std::max(body.h - axis, body.d + axis);   // sized like \left ... \right
        const float total = std::max(2 * delta * 0.901f, 2 * delta - 0.5f * s);
        Box l = n.cp ? delimiter(n.cp, total, st) : Box{}, r = n.cp2 ? delimiter(n.cp2, total, st) : Box{}, b;
        const float padL = n.cp ? 0.1f * s : 0, padR = n.cp2 ? 0.1f * s : 0;
        put(b, l, 0, 0);
        put(b, body, l.w + padL, 0);
        put(b, r, l.w + padL + W + padR, 0);
        b.w = l.w + padL + W + padR + r.w;
        b.h = std::max({ l.h, body.h, r.h });
        b.d = std::max({ l.d, body.d, r.d });
        return b;
    }
};

Rectangle inkBox(const Font& f, const Piece& p) {
    if (p.glyph < 0) return { p.pos.x, p.pos.y, p.rule.x, p.rule.y };
    const Metrics m = f.metrics(p.glyph);
    return { p.pos.x + m.x0 * p.size, p.pos.y - m.y1 * p.size, (m.x1 - m.x0) * p.size, (m.y1 - m.y0) * p.size };
}
Rectangle unite(Rectangle a, Rectangle b) {
    if (a.width <= 0 && a.height <= 0) return b;
    const float x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y);
    return { x0, y0, std::max(a.x + a.width, b.x + b.width) - x0, std::max(a.y + a.height, b.y + b.height) - y0 };
}

vg::Path piecePath(const Font& f, const Piece& p, Vector2 at, float size) {
    if (p.glyph < 0) return vg::Path::rect({ at.x, at.y, p.rule.x * size / std::max(p.size, 1e-6f), p.rule.y * size / std::max(p.size, 1e-6f) });
    return f.outline(p.glyph).scaled(size, size).translated(at);
}

Color resolve(const Piece& p, const Look& look, float extra) {
    Color c = p.color;
    for (auto it = p.parts.rbegin(); it != p.parts.rend(); ++it)
        if (auto f = look.colors.find(*it); f != look.colors.end()) { c = f->second; break; }
    float a = look.alpha * extra;
    for (const std::string& part : p.parts)
        if (auto f = look.opacity.find(part); f != look.opacity.end()) a *= f->second;
    return withAlpha(c, std::clamp(a, 0.f, 1.f));
}

// Fills the collected outlines, one GPU pass per colour.
struct Batch {
    std::vector<std::pair<Color, vg::Path>> groups;
    void add(Color c, const vg::Path& p) {
        if (c.a == 0) return;
        for (auto& g : groups)
            if (g.first.r == c.r && g.first.g == c.g && g.first.b == c.b && g.first.a == c.a) { g.second.append(p); return; }
        groups.push_back({ c, p });
    }
    void draw(const Ctx& cx) const {
        for (const auto& g : groups) vg::fill(cx, g.second, g.first);
    }
};

// Reading order: left edge, then top.
std::vector<int> readingOrder(const Font& f, const std::vector<Piece>& ps) {
    std::vector<int> idx(ps.size());
    for (size_t i = 0; i < ps.size(); i++) idx[i] = (int)i;
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
        const Rectangle ra = inkBox(f, ps[a]), rb = inkBox(f, ps[b]);
        if (std::fabs(ra.x - rb.x) > 1e-3f) return ra.x < rb.x;
        return ra.y < rb.y;
    });
    return idx;
}

// Pairs equal glyphs (rules with rules): the k-th occurrence in one reading order with the
// k-th in the other, so every shared symbol glides, even when terms swap sides.
void match(const std::vector<Piece>& A, const std::vector<int>& ia, const std::vector<Piece>& B, const std::vector<int>& ib,
           std::vector<std::pair<int, int>>& out) {
    std::unordered_map<int, std::vector<int>> pool;
    for (int j : ib) pool[B[j].glyph].push_back(j);
    std::unordered_map<int, size_t> next;
    for (int i : ia) {
        auto it = pool.find(A[i].glyph);
        if (it == pool.end()) continue;
        size_t& k = next[A[i].glyph];
        if (k < it->second.size()) out.push_back({ i, it->second[k++] });
    }
}

} // namespace

// ------------------------------------------------------------------ Formula

Formula Formula::tex(const std::string& tex, const Options& o) {
    Formula f;
    f.source = tex;
    f.font_ = Font::load(o.font);
    const List l = Parser(tex).parse();
    Layout lay(*f.font_);
    const Box b = lay.list(l, St{ o.display ? 0 : 1, false });
    for (const Item& it : b.items) {
        Piece p;
        p.glyph = it.glyph;
        p.pos = { it.x * o.size, it.y * o.size };
        p.size = it.glyph >= 0 ? it.s * o.size : o.size;
        if (it.glyph < 0) p.rule = { it.w * o.size, it.h * o.size };
        p.color = it.hasColor ? it.color : o.color;
        p.parts = it.parts;
        if (!p.parts.empty()) p.part = p.parts.back();
        f.pieces.push_back(std::move(p));
    }
    f.width = b.w * o.size;
    f.ascent = b.h * o.size;
    f.descent = b.d * o.size;
    return f;
}

Rectangle Formula::bounds() const { return { 0, -ascent, width, ascent + descent }; }
Rectangle Formula::bounds(const std::string& part) const {
    Rectangle r{ 0, 0, 0, 0 };
    for (const Piece& p : pieces)
        if (std::find(p.parts.begin(), p.parts.end(), part) != p.parts.end()) r = unite(r, inkBox(*font_, p));
    return r;
}
bool Formula::has(const std::string& part) const {
    for (const Piece& p : pieces)
        if (std::find(p.parts.begin(), p.parts.end(), part) != p.parts.end()) return true;
    return false;
}
vg::Path Formula::path(const Piece& p) const {
    if (p.glyph < 0) return vg::Path::rect({ p.pos.x, p.pos.y, p.rule.x, p.rule.y });
    return font_->outline(p.glyph).scaled(p.size, p.size).translated(p.pos);
}
vg::Path Formula::path() const {
    vg::Path out;
    for (const Piece& p : pieces) out.append(path(p));
    return out;
}

Vector2 origin(const Formula& f, Vector2 at, Vector2 anchor) {
    return { at.x - anchor.x * f.width, at.y + f.ascent - anchor.y * (f.ascent + f.descent) };
}
Rectangle place(const Formula& f, Vector2 at, Vector2 anchor, const std::string& part) {
    const Vector2 o = origin(f, at, anchor);
    const Rectangle r = part.empty() ? f.bounds() : f.bounds(part);
    return { r.x + o.x, r.y + o.y, r.width, r.height };
}

void draw(const Ctx& c, const Formula& f, Vector2 at, Vector2 anchor, const Look& look) {
    const Vector2 o = origin(f, at, anchor);
    Batch batch;
    for (const Piece& p : f.pieces) batch.add(resolve(p, look, 1), f.path(p).translated(o));
    batch.draw(c);
}

void write(const Ctx& c, const Formula& f, Vector2 at, Vector2 anchor, float u, const Look& look) {
    if (u <= 0) return;
    if (u >= 1) return draw(c, f, at, anchor, look);
    const Vector2 o = origin(f, at, anchor);
    // Like vg::write on a drawing: pieces start one after another and overlap.
    struct Dummy {};
    std::vector<int> order(f.pieces.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        const auto& pa = f.pieces[a]; const auto& pb = f.pieces[b];
        return pa.pos.x < pb.pos.x - 1e-3f;
    });
    const size_t n = order.size();
    const float lag = 0.3f, dur = 1 / (1 + (n > 0 ? n - 1 : 0) * lag);
    Batch done;
    for (size_t k = 0; k < n; k++) {
        const Piece& p = f.pieces[order[k]];
        const float ui = (u - k * lag * dur) / dur;
        if (ui <= 0) continue;
        const Color col = resolve(p, look, 1);
        const vg::Path path = f.path(p).translated(o);
        if (ui >= 1) done.add(col, path);
        else vg::write(c, path, vg::Style::filled(col), ui);
    }
    done.draw(c);
}

// ------------------------------------------------------------------ Transform

Transform::Transform(const Formula& from, const Formula& to) : a_(from), b_(to) {
    std::vector<bool> usedA(a_.pieces.size(), false), usedB(b_.pieces.size(), false);
    const std::vector<int> oa = readingOrder(*a_.font_, a_.pieces), ob = readingOrder(*b_.font_, b_.pieces);
    // 1. within parts of the same name
    std::vector<std::string> names;
    for (const Piece& p : a_.pieces)
        if (!p.part.empty() && std::find(names.begin(), names.end(), p.part) == names.end()) names.push_back(p.part);
    for (const std::string& name : names) {
        std::vector<int> ia, ib;
        for (int i : oa) if (a_.pieces[i].part == name) ia.push_back(i);
        for (int j : ob) if (b_.pieces[j].part == name) ib.push_back(j);
        if (ib.empty()) continue;
        std::vector<std::pair<int, int>> m;
        match(a_.pieces, ia, b_.pieces, ib, m);
        for (const auto& pr : m) { pairs_.push_back(pr); usedA[pr.first] = usedB[pr.second] = true; }
    }
    // 2. everything left, in reading order
    std::vector<int> ia, ib;
    for (int i : oa) if (!usedA[i]) ia.push_back(i);
    for (int j : ob) if (!usedB[j]) ib.push_back(j);
    std::vector<std::pair<int, int>> m;
    match(a_.pieces, ia, b_.pieces, ib, m);
    for (const auto& pr : m) { pairs_.push_back(pr); usedA[pr.first] = usedB[pr.second] = true; }
    for (size_t i = 0; i < usedA.size(); i++) if (!usedA[i]) onlyA_.push_back((int)i);
    for (size_t j = 0; j < usedB.size(); j++) if (!usedB[j]) onlyB_.push_back((int)j);
}

void Transform::draw(const Ctx& c, Vector2 at, Vector2 anchor, float u, const Look& look) const {
    u = std::clamp(u, 0.f, 1.f);
    if (u <= 0) return math::draw(c, a_, at, anchor, look);
    if (u >= 1) return math::draw(c, b_, at, anchor, look);
    const Vector2 oa = origin(a_, at, anchor), ob = origin(b_, at, anchor);
    Batch batch;
    for (const auto& [i, j] : pairs_) {
        const Piece& p = a_.pieces[i];
        const Piece& q = b_.pieces[j];
        const Vector2 pos{ oa.x + p.pos.x + (ob.x + q.pos.x - oa.x - p.pos.x) * u, oa.y + p.pos.y + (ob.y + q.pos.y - oa.y - p.pos.y) * u };
        const Color col = mixC(resolve(p, look, 1), resolve(q, look, 1), u);
        vg::Path path;
        if (p.glyph < 0) {
            const Vector2 sz{ p.rule.x + (q.rule.x - p.rule.x) * u, p.rule.y + (q.rule.y - p.rule.y) * u };
            path = vg::Path::rect({ pos.x, pos.y, sz.x, sz.y });
        } else {
            const float size = p.size + (q.size - p.size) * u;
            path = a_.font_->outline(p.glyph).scaled(size, size).translated(pos);
        }
        batch.add(col, path);
    }
    const float out = std::clamp(1 - u / 0.6f, 0.f, 1.f), in = std::clamp((u - 0.4f) / 0.6f, 0.f, 1.f);
    for (int i : onlyA_) batch.add(resolve(a_.pieces[i], look, out), a_.path(a_.pieces[i]).translated(oa));
    for (int j : onlyB_) batch.add(resolve(b_.pieces[j], look, in), b_.path(b_.pieces[j]).translated(ob));
    batch.draw(c);
}

} } // rmtn::math
