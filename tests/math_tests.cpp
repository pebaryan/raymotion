// math_tests.cpp -- math:: typesetting without a GPU: parsing and its errors, TeX spacing,
// scripts, fractions, radicals, big operators, delimiters, environments, parts and
// transforms' matching.
#include "rmtn/math.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static math::Formula tex(const char* s, bool display = true) {
    math::Options o;
    o.size = 100;
    o.display = display;
    return math::Formula::tex(s, o);
}
static bool throws(const char* s) {
    try { tex(s); } catch (const std::runtime_error&) { return true; }
    return false;
}
static int rules(const math::Formula& f) {
    int n = 0;
    for (const auto& p : f.pieces) n += p.glyph < 0;
    return n;
}

static void parsing() {
    CHECK(tex("x").pieces.size() == 1);
    CHECK(tex("a + b = c").pieces.size() == 5);
    CHECK(tex(R"(\alpha\beta\Gamma)").pieces.size() == 3);
    CHECK(tex(R"(\frac{1}{2})").pieces.size() == 3 && rules(tex(R"(\frac{1}{2})")) == 1);
    CHECK(rules(tex(R"(\sqrt{x})")) == 1);
    CHECK(rules(tex(R"(\binom{n}{k})")) == 0);
    CHECK(tex(R"(\text{if } x)").pieces.size() == 3);   // spaces in \text take room, not pieces
    CHECK(tex(R"(\phantom{xyz}a)").pieces.size() == 1);
    CHECK(tex(R"(\phantom{xyz}a)").width > tex("a").width * 2);
    CHECK(tex(R"(\sin x)").pieces.size() == 4);
    // errors carry their position
    CHECK(throws(R"(\nosuchcommand)"));
    CHECK(throws("{x"));
    CHECK(throws("x}"));
    CHECK(throws("x^1^2"));
    CHECK(throws(R"(\left( x)"));
    CHECK(throws(R"(\begin{matrix} a \end{pmatrix})"));
    CHECK(throws(R"(\color{nocolour} x)"));
    CHECK(throws(R"(\middle|)"));
    try { tex(R"(a + \oops)"); } catch (const std::runtime_error& e) { CHECK(std::string(e.what()).find("\\oops") != std::string::npos); }
}

static void spacing() {
    // relations get thick spaces, binary operators medium ones, and a leading minus none
    const float a = tex("a").width, eq = tex("=").width, plus = tex("+").width;
    const float rel = tex("a=a").width - 2 * a - eq, bin = tex("a+a").width - 2 * a - plus;
    CHECK(std::fabs(rel - 2 * 100 * 5 / 18.f) < 1);   // a space each side
    CHECK(std::fabs(bin - 2 * 100 * 4 / 18.f) < 1);
    CHECK(std::fabs(tex("-a").width - tex("-").width - a) < 0.5f);
    // in scripts the spaces vanish
    const auto s = tex("x^{a+a}"), t = tex("x^{aa+}");
    CHECK(std::fabs(s.width - t.width) < 0.5f);
    CHECK(tex(R"(a\quad b)").width - tex("ab").width > 99);
}

static void scripts() {
    const auto x = tex("x"), sup = tex("x^2"), sub = tex("x_2"), both = tex("x_2^2");
    CHECK(sup.ascent > x.ascent && sub.descent > x.descent);
    CHECK(both.ascent >= sup.ascent - 0.5f && both.descent >= sub.descent - 0.5f);
    // a script is smaller than the base
    CHECK(sup.pieces[1].size < sup.pieces[0].size * 0.8f);
    // cramped (under a radical): superscripts sit lower
    const auto r = tex(R"(\sqrt{x^2})"), plain = tex("x^2");
    float yr = 0, yp = 0;
    for (const auto& p : r.pieces) if (p.glyph >= 0 && p.size < 90) yr = p.pos.y;
    yp = plain.pieces[1].pos.y;
    CHECK(yr > yp);
    // primes are full size and take a following superscript
    const auto pr = tex("f'^2");
    CHECK(pr.pieces.size() == 3 && std::fabs(pr.pieces[1].size - 100) < 0.01f && pr.pieces[2].size < 90);
}

static void structures() {
    // display fractions are bigger than inline ones
    const auto d = tex(R"(\frac{a}{b})"), t = tex(R"(\frac{a}{b})", false);
    CHECK(d.ascent > t.ascent && d.descent > t.descent);
    CHECK(tex(R"(\tfrac{a}{b})").ascent < d.ascent);
    // the bar sits on the math axis
    for (const auto& p : d.pieces)
        if (p.glyph < 0) CHECK(std::fabs(-(p.pos.y + p.rule.y / 2) - 25) < 2);
    // display sums are bigger and take limits above and below
    const auto sd = tex(R"(\sum_{i=0}^n)"), st = tex(R"(\sum_{i=0}^n)", false);
    CHECK(sd.ascent > st.ascent);
    float sumY = 0, upY = 0, downY = 0;
    for (const auto& p : sd.pieces) {
        if (p.size > 90) sumY = p.pos.y;
        else if (p.pos.y < 0) upY = p.pos.y;
        else downY = p.pos.y;
    }
    CHECK(upY < sumY - 50 && downY > sumY + 50);
    CHECK(sd.width < 200);
    CHECK(st.width > tex(R"(\sum)", false).width * 1.3f);   // inline: scripts to the side
    // \left( \right) grow with their content
    const auto small = tex(R"(\left( x \right))"), big = tex(R"(\left( \frac{\frac{a}{b}}{\frac{c}{d}} \right))");
    CHECK(big.ascent + big.descent > (small.ascent + small.descent) * 2);
    // \middle grows with them
    CHECK(tex(R"(\left\{ \frac{a}{b} \middle| c \right\})").ascent > tex(R"(\left\{ a \middle| c \right\})").ascent);
    // radicals cover their content; an index widens
    CHECK(tex(R"(\sqrt{\frac{a}{b}})").ascent > tex(R"(\frac{a}{b})").ascent);
    CHECK(tex(R"(\sqrt[123]{x})").width > tex(R"(\sqrt{x})").width);   // a short index tucks into the sign
    // matrices: rows stack, columns spread
    const auto m1 = tex(R"(\begin{pmatrix} a & b \end{pmatrix})"), m2 = tex(R"(\begin{pmatrix} a & b \\ a & b \end{pmatrix})");
    CHECK(m2.ascent + m2.descent > (m1.ascent + m1.descent) * 1.5f);
    CHECK(m2.width < m1.width * 1.3f);              // only the taller parentheses widen it
    // aligned: the relations line up
    const auto al = tex(R"(a &= bbbb \\ cccc &= d)");
    std::vector<float> eqX;
    const int eqG = tex("=").pieces[0].glyph;
    for (const auto& p : al.pieces) if (p.glyph == eqG) eqX.push_back(p.pos.x);
    CHECK(eqX.size() == 2 && std::fabs(eqX[0] - eqX[1]) < 0.01f);
    // gathered lines stack
    CHECK(tex(R"(a \\ b)").ascent + tex(R"(a \\ b)").descent > 150);
}

static void parts() {
    const auto f = tex(R"(\id{lhs}{x^2} = \id{rhs}{\textcolor{red}{y} + \id{c}{1}})");
    CHECK(f.has("lhs") && f.has("rhs") && f.has("c") && !f.has("nope"));
    const Rectangle l = f.bounds("lhs"), r = f.bounds("rhs"), c = f.bounds("c");
    CHECK(l.x + l.width < r.x);
    CHECK(c.x >= r.x && c.x + c.width <= r.x + r.width + 0.01f);
    int inC = 0;
    for (const auto& p : f.pieces)
        if (p.part == "c") { inC++; CHECK(p.parts.size() == 2 && p.parts[0] == "rhs"); }
    CHECK(inC == 1);
    bool red = false;
    for (const auto& p : f.pieces) red = red || (p.color.r > 200 && p.color.g < 150);
    CHECK(red);
    // place(): the anchor puts the metric box's point on `at`
    const Rectangle all = math::place(f, { 500, 300 }, { 0.5f, 0.5f }, "");
    CHECK(std::fabs(all.x + all.width / 2 - 500) < 0.01f && std::fabs(all.y + all.height / 2 - 300) < 0.01f);
    const Vector2 o = math::origin(f, { 0, 0 }, { 0, 1 });
    CHECK(std::fabs(o.x) < 0.01f && std::fabs(o.y + f.descent) < 0.01f);
    // outlines
    CHECK(!f.path().empty());
    const Rectangle pb = f.path().bounds();
    CHECK(pb.x >= -5 && pb.x + pb.width <= f.width + 5);
}

static void transforms() {
    const auto a = tex("x^2 - 5x + 6 = 0"), b = tex(R"(x = \frac{5 \pm \sqrt{25 - 24}}{2})");
    const math::Transform t(a, b);
    CHECK(t.matched() >= 5);
    // parts match by name first, even when their glyphs move far
    const auto p = tex(R"(\id{u}{a} + \id{v}{a})"), q = tex(R"(\id{v}{a} + \id{u}{a})");
    CHECK(math::Transform(p, q).matched() == 3);
    CHECK(math::Transform(a, a).matched() == (int)a.pieces.size());
}

int main() {
    parsing();
    spacing();
    scripts();
    structures();
    parts();
    transforms();
    if (failures) { std::fprintf(stderr, "%d math check(s) failed\n", failures); return 1; }
    std::printf("math tests passed\n");
    return 0;
}
