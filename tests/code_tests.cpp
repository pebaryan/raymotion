// code_tests.cpp -- code:: without a GPU: highlighting in each language (keywords, types,
// calls, members, strings that span lines, raw strings, comments, numbers, preprocessor,
// decorators, attributes, shell commands and variables, JSON keys), tabs and columns,
// finding text, reveal counts, and morph matching (moved, re-indented, edited, added
// and removed lines).
#include "rmtn/code.hpp"
#include <cstdio>
#include <string>

using namespace rmtn;
using code::Kind;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

// the kind of the first token on `line` with this text, or Count if none
static Kind kindOf(const code::Block& b, int line, const std::string& text) {
    for (const code::Token& t : b.lines[(size_t)line - 1].tokens) if (t.text == text) return t.kind;
    return Kind::Count;
}
static bool covers(const code::Block& b) {
    // tokens tile each line exactly, columns included
    for (const code::Line& l : b.lines) {
        std::string joined;
        int col = 0;
        for (const code::Token& t : l.tokens) {
            if (t.col != col) return false;
            joined += t.text;
            for (unsigned char ch : t.text) if ((ch & 0xC0) != 0x80) col++;
        }
        if (joined != l.text || col != l.width) return false;
    }
    return true;
}

static void cpp() {
    const code::Block b = code::Block::parse(R"(
#include <vector>
// sum the squares
static int total(const std::vector<int>& xs) {
    int s = 0;   /* running */
    for (int x : xs) s += x * x;
    auto msg = R"x(raw "text")x";
    return obj.size() + p->count + MAX_N + 0x1F + 2.5e-3f;
}
)", "cpp");
    CHECK(b.lines.size() == 8);
    CHECK(covers(b));
    CHECK(kindOf(b, 1, "#include") == Kind::Preprocessor && kindOf(b, 1, "<vector>") == Kind::String);
    CHECK(kindOf(b, 2, "// sum the squares") == Kind::Comment);
    CHECK(kindOf(b, 3, "static") == Kind::Keyword && kindOf(b, 3, "int") == Kind::Type && kindOf(b, 3, "total") == Kind::Function);
    CHECK(kindOf(b, 4, "/* running */") == Kind::Comment && kindOf(b, 4, "0") == Kind::Number);
    CHECK(kindOf(b, 5, "+=") == Kind::Operator && kindOf(b, 5, "(") == Kind::Punctuation);
    CHECK(kindOf(b, 6, R"(R"x(raw "text")x")") == Kind::String);
    CHECK(kindOf(b, 7, "size") == Kind::Function && kindOf(b, 7, "count") == Kind::Property);
    CHECK(kindOf(b, 7, "MAX_N") == Kind::Constant && kindOf(b, 7, "0x1F") == Kind::Number && kindOf(b, 7, "2.5e-3f") == Kind::Number);
    CHECK(kindOf(b, 3, "std") == Kind::Text);
}

static void languages() {
    const code::Block py = code::Block::parse("@cache\ndef fib(n: int) -> int:\n    \"\"\"Doc\n    string\"\"\"\n    return None if n < 2 else f'{n}'  # done\n", "python");
    CHECK(covers(py));
    CHECK(kindOf(py, 1, "@cache") == Kind::Attribute && kindOf(py, 2, "def") == Kind::Keyword && kindOf(py, 2, "fib") == Kind::Function);
    CHECK(kindOf(py, 3, "\"\"\"Doc") == Kind::String && kindOf(py, 4, "    string\"\"\"") == Kind::String);   // spans lines
    CHECK(kindOf(py, 5, "None") == Kind::Constant && kindOf(py, 5, "f'{n}'") == Kind::String && kindOf(py, 5, "# done") == Kind::Comment);

    const code::Block js = code::Block::parse("const s = `a\nb ${x}`;\nclass Point extends Base {}\nlet y = undefined;", "js");
    CHECK(covers(js) && kindOf(js, 1, "`a") == Kind::String && kindOf(js, 2, "b ${x}`") == Kind::String);
    CHECK(kindOf(js, 3, "Point") == Kind::Type && kindOf(js, 3, "extends") == Kind::Keyword && kindOf(js, 4, "undefined") == Kind::Constant);
    const code::Block ts = code::Block::parse("interface P { x: number }", "ts");
    CHECK(kindOf(ts, 1, "interface") == Kind::Keyword && kindOf(ts, 1, "number") == Kind::Type);

    const code::Block rs = code::Block::parse("#[derive(Debug)]\nfn get<'a>(s: &'a str) -> Option<&'a str> {\n    println!(\"{}\", r#\"raw\"#); Some(s)\n}", "rust");
    CHECK(covers(rs));
    CHECK(kindOf(rs, 1, "#[derive(Debug)]") == Kind::Attribute && kindOf(rs, 2, "fn") == Kind::Keyword && kindOf(rs, 2, "'a") == Kind::Attribute);
    CHECK(kindOf(rs, 2, "Option") == Kind::Type && kindOf(rs, 3, "println!") == Kind::Function && kindOf(rs, 3, "r#\"raw\"#") == Kind::String);
    CHECK(kindOf(rs, 3, "Some") == Kind::Constant);

    const code::Block go = code::Block::parse("func main() { var x int = nil; s := `raw` }", "go");
    CHECK(kindOf(go, 1, "func") == Kind::Keyword && kindOf(go, 1, "main") == Kind::Function && kindOf(go, 1, "nil") == Kind::Constant && kindOf(go, 1, "`raw`") == Kind::String);

    const code::Block sh = code::Block::parse("# build it\ncmake --build build -j 8 | tee \"$LOG\" && echo ${HOME}", "bash");
    CHECK(covers(sh));
    CHECK(kindOf(sh, 1, "# build it") == Kind::Comment && kindOf(sh, 2, "cmake") == Kind::Function && kindOf(sh, 2, "--build") == Kind::Attribute);
    CHECK(kindOf(sh, 2, "build") == Kind::Text && kindOf(sh, 2, "tee") == Kind::Function && kindOf(sh, 2, "echo") == Kind::Function);
    CHECK(kindOf(sh, 2, "${HOME}") == Kind::Property && kindOf(sh, 2, "\"$LOG\"") == Kind::String);

    const code::Block js2 = code::Block::parse("{ \"name\": \"raymotion\", \"stars\": 12, \"ok\": true }", "json");
    CHECK(kindOf(js2, 1, "\"name\"") == Kind::Property && kindOf(js2, 1, "\"raymotion\"") == Kind::String && kindOf(js2, 1, "true") == Kind::Constant);

    const code::Block sql = code::Block::parse("SELECT name FROM users WHERE id = 1 -- one", "sql");
    CHECK(kindOf(sql, 1, "SELECT") == Kind::Keyword && kindOf(sql, 1, "-- one") == Kind::Comment && kindOf(sql, 1, "1") == Kind::Number);
    const code::Block rq = code::Block::parse("PREFIX ex: <http://ex.org/>\nSELECT ?who WHERE { ?who a ex:Person ; ex:age ?n . FILTER(?n > 30) } # adults", "sparql");
    CHECK(kindOf(rq, 1, "PREFIX") == Kind::Keyword && kindOf(rq, 1, "ex:") == Kind::Type && kindOf(rq, 1, "<http://ex.org/>") == Kind::String);
    CHECK(kindOf(rq, 2, "?who") == Kind::Property && kindOf(rq, 2, "a") == Kind::Keyword && kindOf(rq, 2, "Person") == Kind::Function);
    CHECK(kindOf(rq, 2, "FILTER") == Kind::Keyword && kindOf(rq, 2, ">") == Kind::Operator && kindOf(rq, 2, "# adults") == Kind::Comment);

    const code::Block gl = code::Block::parse("#version 330\nuniform vec3 tint;\nvoid main() { gl_FragColor = vec4(tint, 1.0); }", "glsl");
    CHECK(kindOf(gl, 1, "#version") == Kind::Preprocessor && kindOf(gl, 2, "uniform") == Kind::Keyword && kindOf(gl, 2, "vec3") == Kind::Type);

    const code::Block plain = code::Block::parse("int x = 1; // not code", "text");
    CHECK(plain.lines[0].tokens.size() == 1 && plain.lines[0].tokens[0].kind == Kind::Text);
}

static void layout() {
    const code::Block b = code::Block::parse("\nif x:\n\tpass  \t\n\n", "python", 4);   // leading newline and trailing blanks go
    CHECK(b.lines.size() == 2 && b.lines[1].text == "    pass" && b.columns() == 8);
    CHECK(b.chars() == 5 + 1 + 8);
    const code::Block u = code::Block::parse("s = \"héllo wörld\"  # ü", "python");
    CHECK(u.lines[0].width == 22 && covers(u));                         // columns count characters, not bytes
    CHECK(u.find(1, "wörld") == std::make_pair(11, 16) && u.find(1, "#") == std::make_pair(19, 20));
    const code::Block d = code::Block::parse("a + a + a", "cpp");
    CHECK(d.find(1, "a", 2) == std::make_pair(8, 9) && d.find(1, "a", 3).first == -1 && d.find(9, "a").first == -1);
    const code::Mark m = code::Mark::text(d, 1, "+", RED, 1);
    CHECK(m.line == 1 && m.from == 6 && m.to == 7);
}

static int pieces(const code::Morph& m, int state) {   // 0 kept, 1 out, 2 in (code, not numbers)
    int n = 0;
    for (const auto& p : m.pieces()) {
        if (p.number) continue;
        const int s = p.fromLine > 0 && p.toLine > 0 ? 0 : p.fromLine > 0 ? 1 : 2;
        n += s == state;
    }
    return n;
}

static void morphs() {
    const code::Block a = code::Block::parse(R"(
int area(int w, int h) {
    return w * h;
}
)", "cpp");
    // identical: everything stays where it is
    const code::Morph same(a, a);
    CHECK(same.added() == 0 && same.removed() == 0 && pieces(same, 1) == 0 && pieces(same, 2) == 0);
    for (const auto& p : same.pieces()) CHECK(p.fromLine == p.toLine && p.fromCol == p.toCol);
    // a parameter added and a line inserted: the old tokens keep, the new ones fade in
    const code::Block b = code::Block::parse(R"(
// the area of a box
int area(int w, int h, int d) {
    const int base = w * h;
    return w * h;
}
)", "cpp");
    const code::Morph grow(a, b);
    CHECK(grow.removed() == 0 && grow.added() == 2);
    CHECK(pieces(grow, 1) == 0);
    bool movedDown = false;
    for (const auto& p : grow.pieces())
        if (!p.number && p.text == "return") movedDown = p.fromLine == 2 && p.toLine == 4 && p.fromCol == p.toCol;
    CHECK(movedDown);
    int newInts = 0;
    for (const auto& p : grow.pieces()) newInts += !p.number && p.fromLine < 0 && p.text == "d";
    CHECK(newInts == 1);
    // re-indented lines slide sideways
    const code::Block c = code::Block::parse("if (ok) {\n    run();\n}", "cpp"), c2 = code::Block::parse("run();", "cpp");
    const code::Morph unwrap(c, c2);
    CHECK(unwrap.removed() == 2 && unwrap.added() == 0);
    for (const auto& p : unwrap.pieces())
        if (!p.number && p.text == "run") CHECK(p.fromLine == 2 && p.toLine == 1 && p.fromCol == 4 && p.toCol == 0);
    // a rewrite with nothing in common: all out, all in
    const code::Morph swap(code::Block::parse("alpha();", "cpp"), code::Block::parse("struct Beta {};", "cpp"));
    CHECK(pieces(swap, 0) == 0 && swap.added() == 1 && swap.removed() == 1);
    // line numbers follow their lines; renumbered ones cross-fade
    int kept = 0, faded = 0;
    for (const auto& p : grow.pieces()) if (p.number) { kept += p.fromLine > 0 && p.toLine > 0; faded += p.fromLine > 0 && p.toLine < 0; }
    CHECK(kept == 0 && faded == 3);   // every line moved down past the new comment
}

int main() {
    cpp();
    languages();
    layout();
    morphs();
    if (failures) { std::fprintf(stderr, "%d code check(s) failed\n", failures); return 1; }
    std::printf("code tests passed\n");
    return 0;
}
