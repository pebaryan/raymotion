// code.cpp -- code:: lexing and highlighting, code cards with reveals, focus and marks, and
// morphs between two versions (see rmtn/code.hpp).
#include "rmtn/code.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/fonts.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <set>

namespace rmtn {
namespace code {
namespace {

// ------------------------------------------------------------------ languages

struct Spec {
    std::set<std::string> keywords, types, constants;
    std::vector<std::string> lineComments;
    std::string blockOpen, blockClose;
    bool hashPreproc = false;         // #include, #define (at the start of a line)
    bool decorators = false;          // @name
    bool rust = false;                // r#"raw"#, #[attr], name!, 'lifetimes
    bool rawCpp = false;              // R"(raw)"
    bool python = false;              // '''triple''' and """triple""", string prefixes
    bool backtick = false;            // `template` / raw strings, may span lines
    bool singleQuoteStrings = true;
    bool json = false;                // "key": -> Property
    bool shell = false;               // $VAR, commands, --flags, # only at a word start
    bool caseless = false;            // SQL
    bool sparql = false;              // ?vars, <iris>, prefix:names
    bool none = false;                // plain text
};

std::set<std::string> words(const char* s) {
    std::set<std::string> out;
    std::string w;
    for (const char* p = s;; p++) {
        if (*p == ' ' || *p == 0) { if (!w.empty()) out.insert(w); w.clear(); if (!*p) break; }
        else w += *p;
    }
    return out;
}

const Spec& spec(std::string lang) {
    for (char& ch : lang) ch = (char)std::tolower((unsigned char)ch);
    static const Spec cpp = [] {
        Spec s;
        s.keywords = words("alignas alignof asm auto break case catch class const constexpr consteval constinit const_cast continue "
                           "co_await co_return co_yield decltype default delete do dynamic_cast else enum explicit export extern for "
                           "friend goto if inline mutable namespace new noexcept operator private protected public register "
                           "reinterpret_cast requires return sizeof static static_assert static_cast struct switch template this "
                           "thread_local throw try typedef typeid typename union using virtual volatile while override final concept");
        s.types = words("void bool char char8_t char16_t char32_t wchar_t short int long float double signed unsigned size_t "
                        "int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t ptrdiff_t string vector map");
        s.constants = words("true false nullptr NULL");
        s.lineComments = { "//" };
        s.blockOpen = "/*"; s.blockClose = "*/";
        s.hashPreproc = true;
        s.rawCpp = true;
        return s;
    }();
    static const Spec csharp = [] {
        Spec s;
        s.keywords = words("abstract as base break case catch checked class const continue default delegate do else enum event "
                           "explicit extern finally fixed for foreach goto if implicit in interface internal is lock namespace new "
                           "operator out override params private protected public readonly ref return sealed sizeof stackalloc "
                           "static struct switch this throw try typeof unchecked unsafe using virtual volatile while async await "
                           "var get set init record yield where");
        s.types = words("bool byte char decimal double float int long object sbyte short string uint ulong ushort void dynamic");
        s.constants = words("true false null");
        s.lineComments = { "//" };
        s.blockOpen = "/*"; s.blockClose = "*/";
        s.hashPreproc = true;
        return s;
    }();
    static const Spec java = [] {
        Spec s;
        s.keywords = words("abstract assert break case catch class const continue default do else enum extends final finally for "
                           "goto if implements import instanceof interface native new package private protected public return "
                           "static strictfp super switch synchronized this throw throws transient try volatile while var record "
                           "yield sealed permits");
        s.types = words("boolean byte char double float int long short void String");
        s.constants = words("true false null");
        s.lineComments = { "//" };
        s.blockOpen = "/*"; s.blockClose = "*/";
        s.decorators = true;
        return s;
    }();
    static const Spec js = [] {
        Spec s;
        s.keywords = words("break case catch class const continue debugger default delete do else export extends finally for "
                           "function if import in instanceof let new return super switch this throw try typeof var void while "
                           "with yield async await of from as static get set");
        s.constants = words("true false null undefined NaN Infinity");
        s.lineComments = { "//" };
        s.blockOpen = "/*"; s.blockClose = "*/";
        s.backtick = true;
        s.decorators = true;
        return s;
    }();
    static const Spec ts = [] {
        Spec s = js;
        for (const std::string& w : words("interface type enum implements namespace declare readonly abstract private public "
                                          "protected keyof infer is satisfies")) s.keywords.insert(w);
        s.types = words("string number boolean any unknown never void object bigint symbol");
        return s;
    }();
    static const Spec python = [] {
        Spec s;
        s.keywords = words("and as assert async await break class continue def del elif else except finally for from global if "
                           "import in is lambda nonlocal not or pass raise return try while with yield match case");
        s.types = words("int float str bool list dict set tuple bytes object type");
        s.constants = words("True False None self cls");
        s.lineComments = { "#" };
        s.python = true;
        s.decorators = true;
        return s;
    }();
    static const Spec rust = [] {
        Spec s;
        s.keywords = words("as async await break const continue crate dyn else enum extern fn for if impl in let loop match mod "
                           "move mut pub ref return self static struct super trait type unsafe use where while");
        s.types = words("i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize f32 f64 bool char str String Vec Option Result "
                        "Box Self");
        s.constants = words("true false None Some Ok Err");
        s.lineComments = { "//" };
        s.blockOpen = "/*"; s.blockClose = "*/";
        s.rust = true;
        return s;
    }();
    static const Spec go = [] {
        Spec s;
        s.keywords = words("break case chan const continue default defer else fallthrough for func go goto if import interface "
                           "map package range return select struct switch type var");
        s.types = words("bool byte complex64 complex128 error float32 float64 int int8 int16 int32 int64 rune string uint uint8 "
                        "uint16 uint32 uint64 uintptr any");
        s.constants = words("true false nil iota");
        s.lineComments = { "//" };
        s.blockOpen = "/*"; s.blockClose = "*/";
        s.backtick = true;
        return s;
    }();
    static const Spec glsl = [] {
        Spec s;
        s.keywords = words("attribute const uniform varying layout centroid flat smooth break continue do for while switch case "
                           "default if else in out inout return discard struct precision highp mediump lowp");
        s.types = words("void bool int uint float double vec2 vec3 vec4 ivec2 ivec3 ivec4 bvec2 bvec3 bvec4 uvec2 uvec3 uvec4 "
                        "dvec2 dvec3 dvec4 mat2 mat3 mat4 sampler2D sampler3D samplerCube float2 float3 float4 float4x4 half");
        s.constants = words("true false");
        s.lineComments = { "//" };
        s.blockOpen = "/*"; s.blockClose = "*/";
        s.hashPreproc = true;
        return s;
    }();
    static const Spec json = [] {
        Spec s;
        s.constants = words("true false null");
        s.json = true;
        s.singleQuoteStrings = false;
        return s;
    }();
    static const Spec bash = [] {
        Spec s;
        s.keywords = words("if then else elif fi for while until do done case esac in function return local export select "
                           "break continue exit set unset readonly declare source");
        s.constants = words("true false");
        s.lineComments = { "#" };
        s.shell = true;
        return s;
    }();
    static const Spec sql = [] {
        Spec s;
        s.keywords = words("select from where and or not insert into values update set delete create table drop alter add join "
                           "left right inner outer full cross on as group by order having limit offset distinct union all case "
                           "when then else end is like in between exists primary key foreign references index view with "
                           "returning asc desc default unique check");
        s.types = words("int integer bigint smallint text varchar char boolean date timestamp numeric decimal real float double serial");
        s.constants = words("true false null");
        s.lineComments = { "--" };
        s.blockOpen = "/*"; s.blockClose = "*/";
        s.caseless = true;
        return s;
    }();
    static const Spec sparql = [] {
        Spec s;
        s.keywords = words("select construct ask describe where prefix base from named graph optional union minus filter bind "
                           "values as distinct reduced order by asc desc group having limit offset service not exists in a "
                           "insert delete data with using load clear drop create");
        s.types = words("count sum avg min max sample group_concat str lang langmatches datatype bound if coalesce regex "
                        "contains strstarts strends lcase ucase concat iri uri bnode isiri isuri isblank isliteral");
        s.constants = words("true false undef");
        s.lineComments = { "#" };
        s.caseless = true;
        s.sparql = true;
        return s;
    }();
    static const Spec text = [] { Spec s; s.none = true; return s; }();
    if (lang == "cpp" || lang == "c++" || lang == "c" || lang == "h" || lang == "hpp" || lang == "cc" || lang == "cxx") return cpp;
    if (lang == "csharp" || lang == "cs" || lang == "c#") return csharp;
    if (lang == "java" || lang == "kotlin") return java;
    if (lang == "javascript" || lang == "js" || lang == "jsx" || lang == "mjs") return js;
    if (lang == "typescript" || lang == "ts" || lang == "tsx") return ts;
    if (lang == "python" || lang == "py") return python;
    if (lang == "rust" || lang == "rs") return rust;
    if (lang == "go" || lang == "golang") return go;
    if (lang == "glsl" || lang == "hlsl" || lang == "shader" || lang == "frag" || lang == "vert") return glsl;
    if (lang == "json") return json;
    if (lang == "bash" || lang == "sh" || lang == "shell" || lang == "zsh" || lang == "console") return bash;
    if (lang == "sql") return sql;
    if (lang == "sparql" || lang == "rq") return sparql;
    if (lang == "text" || lang == "plain" || lang == "txt" || lang.empty()) return text;
    return cpp;
}

// ------------------------------------------------------------------ the lexer

bool identStart(char ch) { return std::isalpha((unsigned char)ch) || ch == '_' || (unsigned char)ch >= 0x80; }
bool identChar(char ch) { return std::isalnum((unsigned char)ch) || ch == '_' || (unsigned char)ch >= 0x80; }
bool opChar(char ch) { return std::strchr("+-*/%=<>!&|^~?:", ch) != nullptr && ch != 0; }
bool punctChar(char ch) { return std::strchr("()[]{};,.", ch) != nullptr && ch != 0; }
bool startsWith(const std::string& s, size_t i, const std::string& p) { return !p.empty() && s.compare(i, p.size(), p) == 0; }

struct Raw { std::string text; Kind kind; };

std::vector<Raw> lex(const std::string& s, const Spec& sp) {
    std::vector<Raw> out;
    if (sp.none) { out.push_back({ s, Kind::Text }); return out; }
    const size_t n = s.size();
    size_t i = 0;
    bool lineStart = true;            // only whitespace since the last newline
    bool command = true;              // shell: the next word is a command
    bool angleString = false;         // after #include: <...> is a string
    std::string prev;                 // the last significant token
    auto push = [&](size_t a, size_t b, Kind k) {
        out.push_back({ s.substr(a, b - a), k });
        i = b;
        if (k != Kind::Comment) prev = out.back().text;
        lineStart = false;
    };
    // a quoted run from i (the opening quote(s) `q`), honouring backslashes; single-line
    // unless `multi`
    auto quoted = [&](size_t from, const std::string& q, bool multi, bool escapes = true) {
        size_t e = from + q.size();
        while (e < n) {
            if (escapes && s[e] == '\\') { e += 2; continue; }
            if (startsWith(s, e, q)) return e + q.size();
            if (s[e] == '\n' && !multi) return e;
            e++;
        }
        return n;
    };
    auto nextNonSpace = [&](size_t from) { while (from < n && (s[from] == ' ' || s[from] == '\t')) from++; return from; };
    while (i < n) {
        const char ch = s[i];
        if (ch == '\n') {
            out.push_back({ "\n", Kind::Text });
            i++;
            lineStart = true;
            command = true;
            angleString = false;
            continue;
        }
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            size_t e = i;
            while (e < n && (s[e] == ' ' || s[e] == '\t' || s[e] == '\r')) e++;
            out.push_back({ s.substr(i, e - i), Kind::Text });
            i = e;
            continue;
        }
        // comments
        if (!sp.blockOpen.empty() && startsWith(s, i, sp.blockOpen)) {
            const size_t e = s.find(sp.blockClose, i + sp.blockOpen.size());
            push(i, e == std::string::npos ? n : e + sp.blockClose.size(), Kind::Comment);
            continue;
        }
        bool comment = false;
        for (const std::string& lc : sp.lineComments)
            if (startsWith(s, i, lc) && (!sp.shell || i == 0 || std::isspace((unsigned char)s[i - 1]))) comment = true;
        if (comment) {
            const size_t e = s.find('\n', i);
            push(i, e == std::string::npos ? n : e, Kind::Comment);
            continue;
        }
        // preprocessor directives and attributes
        if (sp.hashPreproc && ch == '#' && lineStart) {
            size_t e = nextNonSpace(i + 1);
            const size_t w = e;
            while (e < n && identChar(s[e])) e++;
            const std::string word = s.substr(w, e - w);
            angleString = word == "include" || word == "import";
            push(i, e, Kind::Preprocessor);
            continue;
        }
        if (angleString && ch == '<') {
            const size_t e = s.find('>', i);
            if (e != std::string::npos && s.find('\n', i) > e) { push(i, e + 1, Kind::String); continue; }
        }
        if (sp.rust && ch == '#' && (startsWith(s, i, "#[") || startsWith(s, i, "#!["))) {
            int depth = 0;
            size_t e = i;
            for (; e < n && s[e] != '\n'; e++) {
                if (s[e] == '[') depth++;
                if (s[e] == ']' && --depth == 0) { e++; break; }
            }
            push(i, e, Kind::Attribute);
            continue;
        }
        if (sp.decorators && ch == '@' && i + 1 < n && identStart(s[i + 1])) {
            size_t e = i + 1;
            while (e < n && (identChar(s[e]) || s[e] == '.')) e++;
            push(i, e, Kind::Attribute);
            continue;
        }
        // strings
        if (sp.rawCpp) {
            size_t p = i;
            for (const char* pre : { "u8R", "uR", "UR", "LR", "R" })
                if (startsWith(s, i, pre) && startsWith(s, i + std::strlen(pre), "\"")) { p = i + std::strlen(pre); break; }
            if (p > i && (i == 0 || !identChar(s[i - 1]))) {
                const size_t open = s.find('(', p + 1);
                if (open != std::string::npos && open - p <= 17) {
                    const std::string close = ")" + s.substr(p + 1, open - p - 1) + "\"";
                    const size_t e = s.find(close, open + 1);
                    push(i, e == std::string::npos ? n : e + close.size(), Kind::String);
                    continue;
                }
            }
        }
        if (sp.rust && (ch == 'r' || (ch == 'b' && i + 1 < n && s[i + 1] == 'r')) && (i == 0 || !identChar(s[i - 1]))) {
            size_t p = i + (ch == 'b' ? 2 : 1), hashes = 0;
            while (p < n && s[p] == '#') { p++; hashes++; }
            if (p < n && s[p] == '"' && (hashes > 0 || p == i + (ch == 'b' ? 2u : 1u))) {
                const std::string close = "\"" + std::string(hashes, '#');
                const size_t e = s.find(close, p + 1);
                push(i, e == std::string::npos ? n : e + close.size(), Kind::String);
                continue;
            }
        }
        if (sp.python) {
            size_t p = i;
            while (p < n && p - i < 2 && std::strchr("rRbBfFuU", s[p]) && s[p]) p++;
            if (p < n && (s[p] == '"' || s[p] == '\'') && (p == i || i == 0 || !identChar(s[i - 1]))) {
                const bool raw = std::count_if(s.begin() + (long)i, s.begin() + (long)p, [](char c) { return c == 'r' || c == 'R'; }) > 0;
                const std::string triple(3, s[p]);
                if (startsWith(s, p, triple)) { push(i, quoted(p, triple, true, !raw), Kind::String); continue; }
                push(i, quoted(p, std::string(1, s[p]), false, !raw), Kind::String);
                continue;
            }
        }
        if (sp.backtick && ch == '`') { push(i, quoted(i, "`", true), Kind::String); continue; }
        if (ch == '"' || (ch == '\'' && sp.singleQuoteStrings)) {
            if (sp.rust && ch == '\'') {
                // a lifetime ('a) unless it closes like a char literal ('a', '\n')
                size_t e = i + 1;
                if (e < n && s[e] == '\\') e += 2; else if (e < n) e++;
                while (e < n && ((unsigned char)s[e] & 0xC0) == 0x80) e++;
                if (!(e < n && s[e] == '\'')) {
                    size_t w = i + 1;
                    while (w < n && identChar(s[w])) w++;
                    push(i, w, Kind::Attribute);
                    continue;
                }
            }
            const size_t e = quoted(i, std::string(1, ch), sp.shell);
            const bool key = sp.json && ch == '"' && nextNonSpace(e) < n && s[nextNonSpace(e)] == ':';
            push(i, e, key ? Kind::Property : Kind::String);
            continue;
        }
        // SPARQL: ?variables, <iris>, prefix:names
        if (sp.sparql) {
            if ((ch == '?' || ch == '$') && i + 1 < n && identStart(s[i + 1])) {
                size_t e = i + 1;
                while (e < n && identChar(s[e])) e++;
                push(i, e, Kind::Property);
                continue;
            }
            if (ch == '<') {
                size_t e = i + 1;
                while (e < n && s[e] != '>' && !std::isspace((unsigned char)s[e]) && s[e] != '<') e++;
                if (e < n && s[e] == '>' && e > i + 1) { push(i, e + 1, Kind::String); continue; }
            }
            if (identStart(ch) || ch == ':') {
                size_t e = i;
                while (e < n && (identChar(s[e]) || s[e] == '-')) e++;
                if (e < n && s[e] == ':') {
                    push(i, e + 1, Kind::Type);
                    size_t l = e + 1;
                    while (l < n && (identChar(s[l]) || s[l] == '-')) l++;
                    if (l > e + 1) push(e + 1, l, Kind::Function);
                    i = l;
                    continue;
                }
            }
        }
        // shell variables and flags
        if (sp.shell && ch == '$') {
            size_t e = i + 1;
            if (e < n && s[e] == '{') { const size_t c = s.find('}', e); e = c == std::string::npos ? n : c + 1; }
            else if (e < n && s[e] == '(') { push(i, i + 2, Kind::Punctuation); continue; }
            else while (e < n && (identChar(s[e]) || (e == i + 1 && std::strchr("#?@*!$-0123456789", s[e]) && s[e]))) e++;
            push(i, std::max(e, i + 1), Kind::Property);
            continue;
        }
        if (sp.shell && ch == '-' && (i == 0 || std::isspace((unsigned char)s[i - 1])) && i + 1 < n && (std::isalpha((unsigned char)s[i + 1]) || s[i + 1] == '-')) {
            size_t e = i + 1;
            while (e < n && (identChar(s[e]) || s[e] == '-')) e++;
            push(i, e, Kind::Attribute);
            continue;
        }
        // numbers
        if (std::isdigit((unsigned char)ch) || (ch == '.' && i + 1 < n && std::isdigit((unsigned char)s[i + 1]))) {
            size_t e = i;
            const bool hex = ch == '0' && i + 1 < n && (s[i + 1] == 'x' || s[i + 1] == 'X');
            while (e < n) {
                const char d = s[e];
                if (identChar(d) || d == '\'') { e++; continue; }
                if (d == '.' && e + 1 < n && std::isdigit((unsigned char)s[e + 1])) { e++; continue; }
                if ((d == '+' || d == '-') && !hex && (s[e - 1] == 'e' || s[e - 1] == 'E')) { e++; continue; }
                break;
            }
            push(i, e, Kind::Number);
            continue;
        }
        // words
        if (identStart(ch) || (sp.shell && command && (ch == '.' || ch == '/'))) {
            size_t e = i;
            while (e < n && (identChar(s[e]) || (sp.shell && (s[e] == '-' || ((s[e] == '.' || s[e] == '/') && command))))) e++;
            if (e == i) e++;
            std::string w = s.substr(i, e - i);
            std::string key = w;
            if (sp.caseless) for (char& c : key) c = (char)std::tolower((unsigned char)c);
            const size_t after = nextNonSpace(e);
            Kind k = Kind::Text;
            const bool member = prev == "." || prev == "->" || prev == "?.";
            if (sp.rust && e < n && s[e] == '!' && !(e + 1 < n && s[e + 1] == '=')) { push(i, e + 1, Kind::Function); continue; }
            if (sp.keywords.count(key)) k = Kind::Keyword;
            else if (sp.constants.count(key)) k = Kind::Constant;
            else if (sp.types.count(key)) k = Kind::Type;
            else if (after < n && s[after] == '(') k = Kind::Function;
            else if (member) k = Kind::Property;
            else if (sp.shell && command) k = Kind::Function;
            else if (!sp.shell && !sp.caseless && std::isupper((unsigned char)w[0])) {
                bool lower = false, letters = false;
                for (char c : w) { if (std::islower((unsigned char)c)) lower = true; if (std::isalpha((unsigned char)c)) letters = true; }
                k = lower ? Kind::Type : (w.size() > 1 && letters ? Kind::Constant : Kind::Type);
            }
            if (sp.shell) command = k == Kind::Keyword && key != "in";
            push(i, e, k);
            continue;
        }
        if (opChar(ch)) {
            size_t e = i;
            while (e < n && opChar(s[e]) && !(s[e] == '/' && e + 1 < n && (s[e + 1] == '/' || s[e + 1] == '*'))) e++;
            if (e == i) e++;
            if (sp.shell) {
                const std::string op = s.substr(i, e - i);
                command = op == "|" || op == "||" || op == "&&" || op == "&";
            }
            push(i, e, Kind::Operator);
            continue;
        }
        if (punctChar(ch)) {
            if (sp.shell && ch == ';') command = true;
            push(i, i + 1, Kind::Punctuation);
            continue;
        }
        size_t e = i + 1;
        while (e < n && ((unsigned char)s[e] & 0xC0) == 0x80) e++;
        push(i, e, Kind::Text);
    }
    return out;
}

// ------------------------------------------------------------------ UTF-8 columns

int cols(const std::string& s) {
    int c = 0;
    for (unsigned char ch : s) if ((ch & 0xC0) != 0x80) c++;
    return c;
}
std::string prefix(const std::string& s, int n) {
    if (n <= 0) return "";
    size_t i = 0;
    int c = 0;
    while (i < s.size()) {
        if (((unsigned char)s[i] & 0xC0) != 0x80) { if (c == n) break; c++; }
        i++;
    }
    return s.substr(0, i);
}
bool blank(const std::string& s) { return s.find_first_not_of(" \t\r") == std::string::npos; }

std::string expandTabs(const std::string& s, int tab) {
    std::string out;
    int col = 0;
    for (size_t i = 0; i < s.size(); i++) {
        const char ch = s[i];
        if (ch == '\t') { const int k = tab - col % tab; out.append((size_t)k, ' '); col += k; }
        else if (ch == '\r') {}
        else { out += ch; col = ch == '\n' ? 0 : col + (((unsigned char)ch & 0xC0) != 0x80); }
    }
    return out;
}

float ramp(float u, float a, float b) { return std::clamp((u - a) / (b - a), 0.f, 1.f); }
float smooth(float u) { return u * u * (3 - 2 * u); }

} // namespace

// ------------------------------------------------------------------ Block

Block Block::parse(const std::string& source, const std::string& lang, int tabSize) {
    std::string s = expandTabs(source, std::max(1, tabSize));
    if (!s.empty() && s[0] == '\n') s.erase(0, 1);
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    Block b;
    b.lang = lang;
    b.lines.emplace_back();
    for (const Raw& r : lex(s, spec(lang))) {
        size_t from = 0;
        while (true) {
            const size_t nl = r.text.find('\n', from);
            const std::string piece = r.text.substr(from, nl == std::string::npos ? std::string::npos : nl - from);
            if (!piece.empty()) {
                Line& l = b.lines.back();
                l.tokens.push_back({ piece, r.kind, l.width });
                l.text += piece;
                l.width += cols(piece);
            }
            if (nl == std::string::npos) break;
            b.lines.emplace_back();
            from = nl + 1;
        }
    }
    return b;
}

int Block::columns() const {
    int w = 0;
    for (const Line& l : lines) w = std::max(w, l.width);
    return w;
}

int Block::chars() const {
    int n = 0;
    for (const Line& l : lines) n += l.width + 1;
    return std::max(0, n - 1);
}

std::pair<int, int> Block::find(int line, const std::string& needle, int nth) const {
    if (line < 1 || line > (int)lines.size() || needle.empty()) return { -1, -1 };
    const std::string& t = lines[(size_t)line - 1].text;
    size_t at = t.find(needle);
    for (int k = 0; k < nth && at != std::string::npos; k++) at = t.find(needle, at + 1);
    if (at == std::string::npos) return { -1, -1 };
    const int c = cols(t.substr(0, at));
    return { c, c + cols(needle) };
}

Mark Mark::text(const Block& b, int line, const std::string& needle, Color c, int nth) {
    Mark m;
    m.line = line;
    const auto r = b.find(line, needle, nth);
    m.from = r.first;
    m.to = r.second;
    m.color = c;
    return m;
}

// ------------------------------------------------------------------ themes

Theme Theme::dark() {
    Theme t;
    t.colors[(int)Kind::Text] = mkColor(0xabb2bf);
    t.colors[(int)Kind::Keyword] = mkColor(0xc678dd);
    t.colors[(int)Kind::Type] = mkColor(0xe5c07b);
    t.colors[(int)Kind::Function] = mkColor(0x61afef);
    t.colors[(int)Kind::String] = mkColor(0x98c379);
    t.colors[(int)Kind::Number] = mkColor(0xd19a66);
    t.colors[(int)Kind::Comment] = mkColor(0x7f848e);
    t.colors[(int)Kind::Operator] = mkColor(0x56b6c2);
    t.colors[(int)Kind::Punctuation] = mkColor(0x8b929e);
    t.colors[(int)Kind::Preprocessor] = mkColor(0xc678dd);
    t.colors[(int)Kind::Constant] = mkColor(0xd19a66);
    t.colors[(int)Kind::Property] = mkColor(0xe06c75);
    t.colors[(int)Kind::Attribute] = mkColor(0xe5c07b);
    t.background = mkColor(0x21252b);
    t.border = mkColor(0x3a404b);
    t.header = mkColor(0x1b1e23);
    t.title = mkColor(0x8b929e);
    t.lineNumber = mkColor(0x4b5263);
    t.focus = { 97, 175, 239, 34 };
    t.cursor = mkColor(0x528bff);
    t.added = mkColor(0x98c379);
    t.removed = mkColor(0xe06c75);
    return t;
}

Theme Theme::light() {
    Theme t;
    t.colors[(int)Kind::Text] = mkColor(0x24292f);
    t.colors[(int)Kind::Keyword] = mkColor(0xcf222e);
    t.colors[(int)Kind::Type] = mkColor(0x953800);
    t.colors[(int)Kind::Function] = mkColor(0x8250df);
    t.colors[(int)Kind::String] = mkColor(0x0a3069);
    t.colors[(int)Kind::Number] = mkColor(0x0550ae);
    t.colors[(int)Kind::Comment] = mkColor(0x6e7781);
    t.colors[(int)Kind::Operator] = mkColor(0xcf222e);
    t.colors[(int)Kind::Punctuation] = mkColor(0x57606a);
    t.colors[(int)Kind::Preprocessor] = mkColor(0xcf222e);
    t.colors[(int)Kind::Constant] = mkColor(0x0550ae);
    t.colors[(int)Kind::Property] = mkColor(0x0550ae);
    t.colors[(int)Kind::Attribute] = mkColor(0x8250df);
    t.background = mkColor(0xffffff);
    t.border = mkColor(0xd0d7de);
    t.header = mkColor(0xf6f8fa);
    t.title = mkColor(0x57606a);
    t.lineNumber = mkColor(0x8c959f);
    t.focus = { 84, 174, 255, 40 };
    t.cursor = mkColor(0x0969da);
    t.added = mkColor(0x1a7f37);
    t.removed = mkColor(0xcf222e);
    return t;
}

// ------------------------------------------------------------------ geometry

Font font(bool bold) { return fonts::load({ bold ? "JetBrainsMono-Bold.ttf" : "JetBrainsMono-Regular.ttf" }); }

float cellWidth(const Style& s) { return MeasureTextEx(font(s.bold), "M", s.size, 0).x; }

namespace {
struct Geo {
    float cw, lh, header, gutter, left, top, width, height;
    Vector2 at;
};
int digits(int n) { int d = 1; while (n >= 10) { n /= 10; d++; } return d; }
Geo geo(const Block& b, const Style& s, Vector2 at) {
    Geo g;
    g.at = at;
    g.cw = cellWidth(s);
    g.lh = s.size * s.lineHeight;
    g.header = s.title.empty() ? 0 : s.size * 1.7f;
    g.gutter = s.lineNumbers ? (digits((int)b.lines.size()) + 2) * g.cw : 0;
    g.left = at.x + s.padding.x + g.gutter;
    g.top = at.y + g.header + s.padding.y;
    g.width = 2 * s.padding.x + g.gutter + std::max(b.columns(), s.columns) * g.cw;
    g.height = g.header + 2 * s.padding.y + b.lines.size() * g.lh;
    return g;
}
Vector2 textAt(const Geo& g, const Style& s, int line, float col) {
    return { g.left + col * g.cw, g.top + (line - 1) * g.lh + (g.lh - s.size) / 2 };
}
Vector2 numberAt(const Geo& g, const Style& s, int line, const std::string& num) {
    return textAt(g, s, line, -2.f - (float)num.size() + 1);
}

void panel(const Ctx& c, const Style& s, Rectangle r, float alpha) {
    if (!s.card) return;
    const Theme& th = s.theme;
    dr::roundRect(c, r.x - 1.5f, r.y - 1.5f, r.width + 3, r.height + 3, s.radius + 1.5f, withAlpha(th.border, alpha));
    dr::roundRect(c, r.x, r.y, r.width, r.height, s.radius, withAlpha(th.background, alpha));
    if (s.title.empty()) return;
    const float h = s.size * 1.7f;
    dr::roundRect(c, r.x, r.y, r.width, h, s.radius, withAlpha(th.header, alpha));
    dr::rect(c, r.x, r.y + h - s.radius, r.width, s.radius, withAlpha(th.header, alpha));
    dr::rect(c, r.x, r.y + h, r.width, 1.5f, withAlpha(th.border, alpha));
    const Color dots[3] = { mkColor(0xff5f57), mkColor(0xfebc2e), mkColor(0x28c840) };
    const float dr_ = s.size * 0.22f;
    for (int k = 0; k < 3; k++) dr::circle(c, { r.x + s.padding.x * 0.6f + dr_ + k * dr_ * 3.3f, r.y + h / 2 }, dr_, withAlpha(dots[k], alpha));
    const float ts = s.size * 0.72f;
    const Vector2 m = MeasureTextEx(font(false), s.title.c_str(), ts, 0);
    dr::text(font(false), c, s.title.c_str(), { r.x + r.width / 2 - m.x / 2, r.y + h / 2 - ts / 2 }, ts, 0, withAlpha(th.title, alpha));
}

void say(const Ctx& c, const Style& s, const std::string& text, Vector2 p, Color col) {
    dr::text(font(s.bold), c, text.c_str(), p, s.size, 0, col);
}

// how lit a line is under a focus set (1 when the set is empty)
float within(const Focus& f, int line) {
    for (const Lines& l : f) if (line >= l.from && line <= l.to) return 1;
    return 0;
}
} // namespace

Rectangle bounds(const Block& b, const Style& s, Vector2 at) {
    const Geo g = geo(b, s, at);
    return { at.x, at.y, g.width, g.height };
}

Vector2 cell(const Block& b, const Style& s, Vector2 at, int line, int col) {
    const Geo g = geo(b, s, at);
    return { g.left + col * g.cw, g.top + (line - 1) * g.lh };
}

// ------------------------------------------------------------------ drawing a block

void draw(const Ctx& c, const Block& b, Vector2 at, const Style& s, const Look& look) {
    const Geo g = geo(b, s, at);
    const Theme& th = s.theme;
    panel(c, s, { at.x, at.y, g.width, g.height }, 1);
    const int n = (int)b.lines.size();
    const double reveal = look.reveal < 0 ? 1e18 : look.reveal;
    const float m = std::clamp(look.focusMix, 0.f, 1.f);
    const bool anyFocus = !look.focus.empty() || !look.focusFrom.empty();
    // where each line starts in the typed sequence
    std::vector<double> start((size_t)n + 1, 0);
    for (int i = 0; i < n; i++) start[(size_t)i + 1] = start[(size_t)i] + b.lines[(size_t)i].width + 1;
    // focus bands and marks sit behind the text
    for (int i = 1; i <= n && anyFocus; i++) {
        const float band = (look.focusFrom.empty() ? 0 : within(look.focusFrom, i)) * (1 - m) + (look.focus.empty() ? 0 : within(look.focus, i)) * m;
        if (band <= 0.001f) continue;
        dr::rect(c, at.x, g.top + (i - 1) * g.lh, g.width, g.lh, withAlpha(th.focus, band));
    }
    for (const Mark& mk : look.marks) {
        if (mk.line < 1 || mk.line > n || mk.to <= mk.from || mk.u <= 0) continue;
        const float u = std::clamp(mk.u, 0.f, 1.f);
        const Vector2 p = { g.left + mk.from * g.cw, g.top + (mk.line - 1) * g.lh };
        const float w = (mk.to - mk.from) * g.cw;
        if (mk.underline) dr::rect(c, p.x, p.y + g.lh * 0.86f, w * u, std::max(2.f, s.size * 0.09f), mk.color);
        else {
            const float pad = g.cw * 0.2f;
            dr::roundRect(c, p.x - pad, p.y + g.lh * 0.06f, (w + 2 * pad) * u, g.lh * 0.88f, s.size * 0.18f, withAlpha(mk.color, 0.22f));
            dr::rect(c, p.x - pad, p.y + g.lh * 0.88f, (w + 2 * pad) * u, std::max(2.f, s.size * 0.07f), withAlpha(mk.color, 0.9f));
        }
    }
    for (int i = 1; i <= n; i++) {
        const Line& l = b.lines[(size_t)i - 1];
        const double shown = reveal - start[(size_t)i - 1];
        if (shown < 0) break;
        float a = 1, dx = 0;
        if (look.lines >= 0) {
            a = std::clamp(look.lines - (float)(i - 1), 0.f, 1.f);
            if (a <= 0) break;
            dx = -g.cw * 1.5f * (1 - smooth(a));
            a = smooth(a);
        }
        if (anyFocus) {
            const float from = look.focusFrom.empty() ? 1 : (within(look.focusFrom, i) ? 1 : look.dim);
            const float to = look.focus.empty() ? 1 : (within(look.focus, i) ? 1 : look.dim);
            a *= from * (1 - m) + to * m;
        }
        if (s.lineNumbers && shown >= 0) {
            const std::string num = std::to_string(i);
            Vector2 p = numberAt(g, s, i, num);
            p.x += dx;
            say(c, s, num, p, withAlpha(th.lineNumber, a));
        }
        for (const Token& t : l.tokens) {
            if (blank(t.text)) continue;
            const int len = cols(t.text);
            const int k = (int)std::clamp(shown - t.col, 0.0, (double)len);
            if (k <= 0) break;
            Vector2 p = textAt(g, s, i, (float)t.col);
            p.x += dx;
            say(c, s, k < len ? prefix(t.text, k) : t.text, p, withAlpha(th.of(t.kind), a));
        }
    }
    if (look.cursor) {
        const double r = std::clamp(reveal, 0.0, (double)b.chars());
        int line = n;
        for (int i = 1; i <= n; i++)
            if (r <= start[(size_t)i - 1] + b.lines[(size_t)i - 1].width) { line = i; break; }
        const int col = (int)std::floor(r - start[(size_t)line - 1] + 1e-9);
        const bool typing = look.reveal >= 0 && look.reveal < b.chars();
        if (typing || (look.t / 18) % 2 == 0) {
            const Vector2 p = textAt(g, s, line, (float)std::min(col, b.lines[(size_t)line - 1].width));
            dr::rect(c, p.x, p.y - s.size * 0.08f, std::max(2.f, s.size * 0.1f), s.size * 1.16f, th.cursor);
        }
    }
}

// ------------------------------------------------------------------ morphs

namespace {
std::string trimmed(const std::string& s) {
    const size_t a = s.find_first_not_of(' '), e = s.find_last_not_of(' ');
    return a == std::string::npos ? "" : s.substr(a, e - a + 1);
}
std::vector<const Token*> solid(const Line& l) {
    std::vector<const Token*> out;
    for (const Token& t : l.tokens) if (!blank(t.text)) out.push_back(&t);
    return out;
}
// longest common subsequence: pairs of indices
template <class Eq>
std::vector<std::pair<int, int>> lcs(int n, int m, Eq eq) {
    std::vector<std::vector<int>> d((size_t)n + 1, std::vector<int>((size_t)m + 1, 0));
    for (int i = n - 1; i >= 0; i--)
        for (int j = m - 1; j >= 0; j--)
            d[(size_t)i][(size_t)j] = eq(i, j) ? d[(size_t)i + 1][(size_t)j + 1] + 1 : std::max(d[(size_t)i + 1][(size_t)j], d[(size_t)i][(size_t)j + 1]);
    std::vector<std::pair<int, int>> out;
    int i = 0, j = 0;
    while (i < n && j < m) {
        if (eq(i, j)) { out.push_back({ i, j }); i++; j++; }
        else if (d[(size_t)i + 1][(size_t)j] >= d[(size_t)i][(size_t)j + 1]) i++;
        else j++;
    }
    return out;
}
double similarity(const Line& x, const Line& y) {
    const auto a = solid(x), b = solid(y);
    if (a.empty() || b.empty()) return 0;
    const auto common = lcs((int)a.size(), (int)b.size(), [&](int i, int j) { return a[(size_t)i]->text == b[(size_t)j]->text; });
    return 2.0 * common.size() / (a.size() + b.size());
}
} // namespace

Morph::Morph(const Block& from, const Block& to) : a_(from), b_(to) {
    const int n = (int)a_.lines.size(), m = (int)b_.lines.size();
    lineTo_.assign((size_t)n, -1);
    lineFrom_.assign((size_t)m, -1);
    std::vector<bool> exact((size_t)n, false);
    // 1. identical lines (ignoring indentation) anchor the diff
    const auto anchors = lcs(n, m, [&](int i, int j) { return trimmed(a_.lines[(size_t)i].text) == trimmed(b_.lines[(size_t)j].text); });
    for (auto p : anchors) { lineTo_[(size_t)p.first] = p.second; lineFrom_[(size_t)p.second] = p.first; exact[(size_t)p.first] = true; }
    // 2. between anchors, pair edited lines that still share most of their tokens
    std::vector<std::pair<int, int>> bounds_ = anchors;
    bounds_.insert(bounds_.begin(), { -1, -1 });
    bounds_.push_back({ n, m });
    for (size_t k = 0; k + 1 < bounds_.size(); k++) {
        const int i0 = bounds_[k].first + 1, i1 = bounds_[k + 1].first, j0 = bounds_[k].second + 1, j1 = bounds_[k + 1].second;
        const int gn = i1 - i0, gm = j1 - j0;
        if (gn <= 0 || gm <= 0) continue;
        // an alignment that maximises total similarity, in order
        std::vector<std::vector<double>> d((size_t)gn + 1, std::vector<double>((size_t)gm + 1, 0));
        auto sim = [&](int i, int j) { const double v = similarity(a_.lines[(size_t)(i0 + i)], b_.lines[(size_t)(j0 + j)]); return v >= 0.4 ? v : 0.0; };
        for (int i = gn - 1; i >= 0; i--)
            for (int j = gm - 1; j >= 0; j--) {
                const double sv = sim(i, j);
                d[(size_t)i][(size_t)j] = std::max({ d[(size_t)i + 1][(size_t)j], d[(size_t)i][(size_t)j + 1], sv > 0 ? sv + d[(size_t)i + 1][(size_t)j + 1] : 0.0 });
            }
        int i = 0, j = 0;
        while (i < gn && j < gm) {
            const double sv = sim(i, j);
            if (sv > 0 && std::fabs(d[(size_t)i][(size_t)j] - (sv + d[(size_t)i + 1][(size_t)j + 1])) < 1e-12) {
                lineTo_[(size_t)(i0 + i)] = j0 + j;
                lineFrom_[(size_t)(j0 + j)] = i0 + i;
                i++; j++;
            } else if (d[(size_t)i + 1][(size_t)j] >= d[(size_t)i][(size_t)j + 1]) i++;
            else j++;
        }
    }
    // 3. pieces
    auto add = [&](const Token* x, int xl, const Token* y, int yl) {
        Piece p;
        p.text = x ? x->text : y->text;
        p.fromKind = x ? x->kind : y->kind;
        p.toKind = y ? y->kind : x->kind;
        p.fromLine = x ? xl : -1;
        p.fromCol = x ? x->col : 0;
        p.toLine = y ? yl : -1;
        p.toCol = y ? y->col : 0;
        p.number = false;
        pieces_.push_back(p);
    };
    auto number = [&](int fl, int tl, const std::string& text) {
        Piece p;
        p.text = text;
        p.fromKind = p.toKind = Kind::Text;
        p.fromLine = fl;
        p.toLine = tl;
        p.fromCol = p.toCol = 0;
        p.number = true;
        pieces_.push_back(p);
    };
    for (int i = 0; i < n; i++) {
        const int j = lineTo_[(size_t)i];
        const auto xs = solid(a_.lines[(size_t)i]);
        if (j < 0) {
            for (const Token* t : xs) add(t, i + 1, nullptr, -1);
            number(i + 1, -1, std::to_string(i + 1));
            continue;
        }
        const auto ys = solid(b_.lines[(size_t)j]);
        const auto common = exact[(size_t)i] && xs.size() == ys.size()
                                ? [&] { std::vector<std::pair<int, int>> v; for (int k = 0; k < (int)xs.size(); k++) v.push_back({ k, k }); return v; }()
                                : lcs((int)xs.size(), (int)ys.size(), [&](int p, int q) { return xs[(size_t)p]->text == ys[(size_t)q]->text; });
        std::vector<bool> usedX(xs.size(), false), usedY(ys.size(), false);
        for (auto pq : common) {
            add(xs[(size_t)pq.first], i + 1, ys[(size_t)pq.second], j + 1);
            usedX[(size_t)pq.first] = usedY[(size_t)pq.second] = true;
        }
        for (size_t k = 0; k < xs.size(); k++) if (!usedX[k]) add(xs[k], i + 1, nullptr, -1);
        for (size_t k = 0; k < ys.size(); k++) if (!usedY[k]) add(nullptr, -1, ys[k], j + 1);
        const std::string na = std::to_string(i + 1), nb = std::to_string(j + 1);
        if (na == nb) number(i + 1, j + 1, na);
        else { number(i + 1, -1, na); number(-1, j + 1, nb); }
    }
    for (int j = 0; j < m; j++) {
        if (lineFrom_[(size_t)j] >= 0) continue;
        for (const Token* t : solid(b_.lines[(size_t)j])) add(nullptr, -1, t, j + 1);
        number(-1, j + 1, std::to_string(j + 1));
    }
}

int Morph::added() const { return (int)std::count(lineFrom_.begin(), lineFrom_.end(), -1); }
int Morph::removed() const { return (int)std::count(lineTo_.begin(), lineTo_.end(), -1); }

void draw(const Ctx& c, const Morph& mo, Vector2 at, const Style& s, float u) {
    u = std::clamp(u, 0.f, 1.f);
    const Geo ga = geo(mo.a_, s, at), gb = geo(mo.b_, s, at);
    const Theme& th = s.theme;
    const float out = 1 - smooth(ramp(u, 0, 0.35f)), move = smooth(ramp(u, 0.15f, 0.85f)), in = smooth(ramp(u, 0.6f, 1));
    auto lerp = [](float x, float y, float k) { return x + (y - x) * k; };
    panel(c, s, { at.x, at.y, lerp(ga.width, gb.width, move), lerp(ga.height, gb.height, move) }, 1);
    // the diff: removed lines flash red as they go, added lines green as they come
    if (s.diffGutter && u > 0 && u < 1) {
        const float sign = s.size * 0.8f, gx = at.x + s.padding.x * 0.45f;
        for (int i = 0; i < (int)mo.a_.lines.size(); i++) {
            if (mo.lineTo_[(size_t)i] >= 0) continue;
            const float k = smooth(ramp(u, 0, 0.08f)) * out;
            dr::rect(c, at.x, ga.top + i * ga.lh, ga.width, ga.lh, withAlpha(th.removed, 0.16f * k));
            dr::text(font(s.bold), c, "-", { gx, ga.top + i * ga.lh + (ga.lh - sign) / 2 }, sign, 0, withAlpha(th.removed, k));
        }
        for (int j = 0; j < (int)mo.b_.lines.size(); j++) {
            if (mo.lineFrom_[(size_t)j] >= 0) continue;
            const float k = smooth(ramp(u, 0.55f, 0.75f)) * (1 - smooth(ramp(u, 0.88f, 1)));
            dr::rect(c, at.x, gb.top + j * gb.lh, gb.width, gb.lh, withAlpha(th.added, 0.16f * k));
            dr::text(font(s.bold), c, "+", { gx, gb.top + j * gb.lh + (gb.lh - sign) / 2 }, sign, 0, withAlpha(th.added, k));
        }
    }
    for (const Morph::Piece& p : mo.pieces_) {
        if (p.number && !s.lineNumbers) continue;
        Vector2 pa{}, pb{};
        if (p.fromLine > 0) pa = p.number ? numberAt(ga, s, p.fromLine, p.text) : textAt(ga, s, p.fromLine, (float)p.fromCol);
        if (p.toLine > 0) pb = p.number ? numberAt(gb, s, p.toLine, p.text) : textAt(gb, s, p.toLine, (float)p.toCol);
        Vector2 pos;
        float a;
        Color col;
        const Color ca = p.number ? th.lineNumber : th.of(p.fromKind), cb = p.number ? th.lineNumber : th.of(p.toKind);
        if (p.fromLine > 0 && p.toLine > 0) { pos = { lerp(pa.x, pb.x, move), lerp(pa.y, pb.y, move) }; a = 1; col = mixC(ca, cb, move); }
        else if (p.fromLine > 0) { pos = pa; a = p.number ? 1 - move : out; col = ca; }   // renumbering cross-fades
        else { pos = pb; a = p.number ? move : in; col = cb; }
        if (a <= 0.002f) continue;
        say(c, s, p.text, pos, withAlpha(col, a));
    }
}

} } // rmtn::code
