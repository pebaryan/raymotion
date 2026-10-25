// diagram_parse.cpp -- text formats to dg::Graph: Mermaid flowcharts and state diagrams,
// Graphviz DOT, Newick trees, JSON graphs, Turtle / N-Triples and JSON-LD.
#include "diagram_internal.hpp"
#include "rmtn/media.hpp"
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace rmtn {
namespace dg {
namespace detail {

std::string trim(std::string_view s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return std::string(s.substr(a, b - a));
}
std::string lower(std::string_view s) {
    std::string r(s);
    for (char& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}
std::string cleanLabel(std::string_view in) {
    std::string s(in), out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '<') {                              // <br>, <br/>, <br /> break; other tags vanish
            const size_t close = s.find('>', i);
            if (close != std::string::npos) {
                std::string tag = lower(s.substr(i + 1, close - i - 1));
                tag.erase(std::remove(tag.begin(), tag.end(), ' '), tag.end());
                if (tag == "br" || tag == "br/") out += '\n';
                i = close;
                continue;
            }
        }
        if (s[i] == '&') {
            const size_t semi = s.find(';', i);
            if (semi != std::string::npos && semi - i <= 8) {
                const std::string ent = s.substr(i + 1, semi - i - 1);
                const char* rep = ent == "amp" ? "&" : ent == "lt" ? "<" : ent == "gt" ? ">" : ent == "quot" ? "\"" :
                                  ent == "apos" ? "'" : ent == "nbsp" ? " " : nullptr;
                if (rep) { out += rep; i = semi; continue; }
                if (!ent.empty() && ent[0] == '#') {
                    const long cp = ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X') ? std::strtol(ent.c_str() + 2, nullptr, 16)
                                                                                        : std::strtol(ent.c_str() + 1, nullptr, 10);
                    if (cp > 0 && cp < 0x110000) {
                        int len = 0;
                        const char* u = CodepointToUTF8((int)cp, &len);
                        out.append(u, (size_t)len);
                        i = semi;
                        continue;
                    }
                }
            }
        }
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n') { out += '\n'; i++; continue; }
        out += s[i];
    }
    // Markdown emphasis markers in Mermaid "markdown strings".
    std::string r;
    for (size_t i = 0; i < out.size(); i++) {
        if ((out[i] == '*' || out[i] == '`') && (i + 1 < out.size() && out[i + 1] == out[i])) { i++; continue; }
        if (out[i] == '`') continue;
        r += out[i];
    }
    return trim(r);
}

std::string readFile(const std::string& file) {
    const std::string path = media::resolve(file);
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("dg: cannot read " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void fail(const char* format, int line, std::string_view what) {
    char buf[512];
    std::snprintf(buf, sizeof buf, format, line, std::string(what).c_str());
    throw std::runtime_error(buf);
}

} // namespace detail

using namespace detail;

namespace {
// CSS / X11 colour names (the common ones), "#rgb", "#rrggbb", "#rrggbbaa", "rgb(r,g,b)".
std::optional<Color> colorOf(std::string_view text) {
    std::string s = lower(trim(text));
    if (s.empty()) return std::nullopt;
    if (s[0] == '#') {
        try { return parseColor(s); } catch (...) { return std::nullopt; }
    }
    if (s.rfind("rgb", 0) == 0) {
        int r = 0, g = 0, b = 0;
        float a = 1;
        const size_t p = s.find('(');
        if (p != std::string::npos) {
            const int got = std::sscanf(s.c_str() + p + 1, "%d , %d , %d , %f", &r, &g, &b, &a);
            if (got >= 3) return Color{ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)(std::clamp(a, 0.f, 1.f) * 255) };
        }
        return std::nullopt;
    }
    static const std::unordered_map<std::string, unsigned> names = {
        { "black", 0x000000 }, { "white", 0xffffff }, { "red", 0xff0000 }, { "green", 0x008000 }, { "blue", 0x0000ff },
        { "yellow", 0xffff00 }, { "orange", 0xffa500 }, { "purple", 0x800080 }, { "pink", 0xffc0cb }, { "gray", 0x808080 },
        { "grey", 0x808080 }, { "lightgray", 0xd3d3d3 }, { "lightgrey", 0xd3d3d3 }, { "darkgray", 0xa9a9a9 },
        { "lightblue", 0xadd8e6 }, { "lightgreen", 0x90ee90 }, { "lightyellow", 0xffffe0 }, { "lightpink", 0xffb6c1 },
        { "darkblue", 0x00008b }, { "darkgreen", 0x006400 }, { "darkred", 0x8b0000 }, { "navy", 0x000080 },
        { "teal", 0x008080 }, { "cyan", 0x00ffff }, { "magenta", 0xff00ff }, { "brown", 0xa52a2a }, { "gold", 0xffd700 },
        { "salmon", 0xfa8072 }, { "tomato", 0xff6347 }, { "coral", 0xff7f50 }, { "khaki", 0xf0e68c }, { "lavender", 0xe6e6fa },
        { "violet", 0xee82ee }, { "orchid", 0xda70d6 }, { "plum", 0xdda0dd }, { "skyblue", 0x87ceeb }, { "steelblue", 0x4682b4 },
        { "royalblue", 0x4169e1 }, { "dodgerblue", 0x1e90ff }, { "crimson", 0xdc143c }, { "forestgreen", 0x228b22 },
        { "seagreen", 0x2e8b57 }, { "olive", 0x808000 }, { "lime", 0x00ff00 }, { "maroon", 0x800000 }, { "silver", 0xc0c0c0 },
        { "beige", 0xf5f5dc }, { "ivory", 0xfffff0 }, { "wheat", 0xf5deb3 }, { "tan", 0xd2b48c }, { "chocolate", 0xd2691e },
        { "indigo", 0x4b0082 }, { "turquoise", 0x40e0d0 }, { "aquamarine", 0x7fffd4 }, { "palegreen", 0x98fb98 },
        { "lightsalmon", 0xffa07a }, { "lightcyan", 0xe0ffff }, { "aliceblue", 0xf0f8ff }, { "honeydew", 0xf0fff0 },
        { "mintcream", 0xf5fffa }, { "whitesmoke", 0xf5f5f5 }, { "gainsboro", 0xdcdcdc }, { "slategray", 0x708090 },
        { "dimgray", 0x696969 }, { "firebrick", 0xb22222 }, { "goldenrod", 0xdaa520 }, { "peachpuff", 0xffdab9 },
        { "mistyrose", 0xffe4e1 }, { "lemonchiffon", 0xfffacd }, { "cornsilk", 0xfff8dc }, { "linen", 0xfaf0e6 },
    };
    if (s == "transparent" || s == "none") return Color{ 0, 0, 0, 0 };
    auto it = names.find(s);
    if (it != names.end()) return mkColor(it->second);
    return std::nullopt;
}

// "fill:#f9f,stroke:#333,stroke-width:4px,color:#fff,stroke-dasharray: 5 5"
void applyCss(Style& st, std::string_view css) {
    std::string s(css);
    size_t i = 0;
    while (i < s.size()) {
        size_t j = s.find(',', i);
        // A comma inside rgb(...) does not end the declaration.
        const size_t paren = s.find('(', i);
        if (paren != std::string::npos && paren < j) { const size_t close = s.find(')', paren); if (close != std::string::npos) j = s.find(',', close); }
        if (j == std::string::npos) j = s.size();
        const std::string decl = s.substr(i, j - i);
        const size_t colon = decl.find(':');
        if (colon != std::string::npos) {
            const std::string k = lower(trim(decl.substr(0, colon))), v = trim(decl.substr(colon + 1));
            if (k == "fill" || k == "background" || k == "background-color") { if (auto c = colorOf(v)) st.fill = c; }
            else if (k == "stroke" || k == "border-color") { if (auto c = colorOf(v)) st.stroke = c; }
            else if (k == "color") { if (auto c = colorOf(v)) st.text = c; }
            else if (k == "stroke-width") st.strokeWidth = (float)std::atof(v.c_str());
            else if (k == "stroke-dasharray") st.dash = Stroke::Dashed;
        }
        i = j + 1;
    }
}

Direction directionOf(std::string_view d) {
    const std::string s = lower(trim(d));
    if (s == "lr") return Direction::LR;
    if (s == "rl") return Direction::RL;
    if (s == "bt") return Direction::BT;
    return Direction::TB;
}

// Statements split on newlines and on ';' outside quotes and brackets; %% comments gone.
struct Stmt { std::string text; int line; };
std::vector<Stmt> statements(std::string_view text, bool splitSemicolons) {
    std::vector<Stmt> out;
    std::string cur;
    int line = 1, startLine = 1, depth = 0;
    bool quote = false;
    auto flush = [&] {
        std::string t = trim(cur);
        if (!t.empty()) out.push_back({ t, startLine });
        cur.clear();
        startLine = line;
    };
    for (size_t i = 0; i < text.size(); i++) {
        const char ch = text[i];
        if (!quote && ch == '%' && i + 1 < text.size() && text[i + 1] == '%') {
            while (i < text.size() && text[i] != '\n') i++;
            flush(); line++; startLine = line;
            continue;
        }
        if (ch == '"') quote = !quote;
        if (!quote) {
            if (ch == '[' || ch == '(' || ch == '{') depth++;
            if ((ch == ']' || ch == ')' || ch == '}') && depth > 0) depth--;
        }
        if (ch == '\n') { flush(); line++; startLine = line; quote = false; depth = 0; continue; }
        if (ch == ';' && splitSemicolons && !quote && depth == 0) { flush(); continue; }
        if (ch != '\r') cur += ch;
    }
    flush();
    return out;
}

// ------------------------------------------------------------------ Mermaid flowchart
struct MermaidLink {
    Arrow head = Arrow::None, tail = Arrow::None;
    Stroke stroke = Stroke::Solid;
    float width = 1;
    int minLength = 1;
    bool invisible = false;
    std::string label;
};

bool isIdChar(char c) {
    return std::isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.' || c == '$' || (unsigned char)c >= 0x80;
}

// A link starting at s[i] (spaces skipped); returns false when there is none.
bool parseLink(const std::string& s, size_t& i, MermaidLink& L) {
    size_t p = i;
    while (p < s.size() && s[p] == ' ') p++;
    if (p >= s.size()) return false;
    if (s.compare(p, 3, "~~~") == 0) {
        while (p < s.size() && s[p] == '~') p++;
        L = {}; L.invisible = true;
        i = p;
        return true;
    }
    MermaidLink r;
    size_t q = p;
    if ((s[q] == '<' || s[q] == 'o' || s[q] == 'x') && q + 1 < s.size() && (s[q + 1] == '-' || s[q + 1] == '=' || s[q + 1] == '.')) {
        r.tail = s[q] == '<' ? Arrow::Normal : s[q] == 'o' ? Arrow::Circle : Arrow::Cross;
        q++;
    }
    const size_t bodyStart = q;
    while (q < s.size() && (s[q] == '-' || s[q] == '=' || s[q] == '.')) q++;
    std::string body = s.substr(bodyStart, q - bodyStart);
    if (body.size() < 2) return false;
    auto classify = [&](const std::string& b, MermaidLink& out) {
        const int dashes = (int)std::count(b.begin(), b.end(), '-'), eqs = (int)std::count(b.begin(), b.end(), '='),
                  dots = (int)std::count(b.begin(), b.end(), '.');
        if (eqs > 0) { out.width = 2; }
        if (dots > 0) { out.stroke = Stroke::Dotted; }
        const int n = dots > 0 ? dots : (eqs > 0 ? eqs : dashes);
        return n;
    };
    // "-- text -->" form: an opening of two characters followed by a space.
    if (body.size() == 2 && q < s.size() && s[q] == ' ' && r.tail == Arrow::None) {
        static const char* closers[] = { "-->", "---", "--o", "--x", ".->", ".-", "==>", "===", "==o", "==x" };
        size_t best = std::string::npos, bestLen = 0;
        for (const char* c : closers) {
            const size_t f = s.find(c, q);
            if (f != std::string::npos && f < best) { best = f; bestLen = std::strlen(c); }
        }
        if (best == std::string::npos) return false;
        r.label = cleanLabel(trim(s.substr(q, best - q)));
        // Back up over the rest of the closing run ("--->" / "-..->").
        size_t e = best;
        while (e > q && (s[e - 1] == '-' || s[e - 1] == '.' || s[e - 1] == '=')) e--;
        size_t k = best + bestLen - 1;
        std::string closing = s.substr(e, k - e + 1);
        q = k + 1;
        const char last = closing.back();
        if (last == '>' || last == 'o' || last == 'x') { r.head = last == '>' ? Arrow::Normal : last == 'o' ? Arrow::Circle : Arrow::Cross; closing.pop_back(); }
        classify(body + closing, r);
        r.minLength = 1;
    } else {
        if (q < s.size() && (s[q] == '>' || ((s[q] == 'o' || s[q] == 'x') && (q + 1 >= s.size() || s[q + 1] == ' ' || s[q + 1] == '|')))) {
            r.head = s[q] == '>' ? Arrow::Normal : s[q] == 'o' ? Arrow::Circle : Arrow::Cross;
            q++;
        }
        const int n = classify(body, r);
        if (r.stroke == Stroke::Dotted) r.minLength = std::max(1, n);
        else r.minLength = std::max(1, r.head != Arrow::None ? n - 1 : n - 2);
        if (r.head == Arrow::None && n < 3 && r.stroke == Stroke::Solid) return false;   // "--" alone is not a link
    }
    while (q < s.size() && s[q] == ' ') q++;
    if (q < s.size() && s[q] == '|') {
        const size_t close = s.find('|', q + 1);
        if (close != std::string::npos) { r.label = cleanLabel(s.substr(q + 1, close - q - 1)); q = close + 1; }
    }
    L = r;
    i = q;
    return true;
}

struct MermaidNodeRef { std::string id, label, cls; bool hasShape = false; Shape shape = Shape::Rect; };

// One node reference "id", "id[label]", "id((label)):::cls" at s[i].
bool parseNodeRef(const std::string& s, size_t& i, MermaidNodeRef& out, int line) {
    size_t p = i;
    while (p < s.size() && s[p] == ' ') p++;
    const size_t start = p;
    while (p < s.size() && isIdChar(s[p])) {
        // An id stops before a link ("A-->B", "A---B", "A-.->B").
        if ((s[p] == '-' || s[p] == '.' || s[p] == '=') && p + 1 < s.size() && (s[p + 1] == '-' || s[p + 1] == '.' || s[p + 1] == '=' || s[p + 1] == '>')) break;
        p++;
    }
    if (p == start) return false;
    out = {};
    out.id = s.substr(start, p - start);
    struct Delim { const char* open; const char* close; Shape shape; };
    static const Delim delims[] = {
        { "(((", ")))", Shape::DoubleCircle }, { "((", "))", Shape::Circle }, { "([", "])", Shape::Stadium },
        { "[[", "]]", Shape::Subroutine }, { "[(", ")]", Shape::Cylinder }, { "{{", "}}", Shape::Hexagon },
        { "[/", "/]", Shape::Parallelogram }, { "[\\", "\\]", Shape::ParallelogramAlt },
        { "[/", "\\]", Shape::Trapezoid }, { "[\\", "/]", Shape::TrapezoidAlt },
        { "(", ")", Shape::Rounded }, { "[", "]", Shape::Rect }, { "{", "}", Shape::Diamond }, { ">", "]", Shape::Flag },
    };
    // The longest opening that matches; among those, the delimiter that closes first
    // ([/in\] and [/out/] share an opening).
    const Delim* pick = nullptr;
    size_t pickOpen = 0, pickClose = std::string::npos;
    for (const Delim& d : delims) {
        const size_t ol = std::strlen(d.open);
        if (s.compare(p, ol, d.open) != 0 || ol < pickOpen) continue;
        size_t e = p + ol;                               // skip a quoted label
        if (e < s.size() && s[e] == '"') { const size_t qe = s.find('"', e + 1); e = qe == std::string::npos ? s.size() : qe + 1; }
        const size_t close = s.find(d.close, e);
        if (close == std::string::npos) continue;
        if (ol > pickOpen || close < pickClose) { pick = &d; pickOpen = ol; pickClose = close; }
    }
    if (pick) {
        std::string label = s.substr(p + pickOpen, pickClose - p - pickOpen);
        if (label.size() >= 2 && label.front() == '"' && label.back() == '"') label = label.substr(1, label.size() - 2);
        out.label = cleanLabel(label);
        out.hasShape = true;
        out.shape = pick->shape;
        p = pickClose + std::strlen(pick->close);
    }
    if (s.compare(p, 3, ":::") == 0) {
        size_t e = p + 3;
        while (e < s.size() && isIdChar(s[e])) e++;
        out.cls = s.substr(p + 3, e - p - 3);
        p = e;
    }
    (void)line;
    i = p;
    return true;
}

Graph mermaidFlowchart(const std::vector<Stmt>& st, Direction dir) {
    Graph g;
    g.direction = dir;
    g.layout = LayoutKind::Layered;
    std::vector<std::string> groupStack;
    std::map<std::string, Style> classes;
    std::vector<std::pair<std::string, std::string>> nodeClasses;   // (node, class)
    std::vector<std::pair<std::string, std::string>> linkStyles;    // (index or "default", css)
    std::vector<std::string> invisible;
    std::unordered_set<std::string> declared;
    auto touch = [&](const MermaidNodeRef& r) {
        Node* n = g.node(r.id);
        if (!n) {
            n = &g.addNode(r.id, r.id, Shape::Rect);
            if (!groupStack.empty()) n->group = groupStack.back();
        } else if (n->group.empty() && !groupStack.empty()) {
            n->group = groupStack.back();
        }
        if (r.hasShape) {
            n->shape = r.shape;
            n->label = r.label;
            declared.insert(r.id);
        }
        if (!r.cls.empty()) nodeClasses.push_back({ r.id, r.cls });
        return r.id;
    };
    for (const Stmt& sm : st) {
        const std::string& s = sm.text;
        const std::string head = lower(s.substr(0, s.find(' ')));
        if (head == "subgraph") {
            std::string rest = trim(s.substr(8));
            std::string id = rest, label = rest;
            const size_t br = rest.find('[');
            if (br != std::string::npos && rest.back() == ']') {
                id = trim(rest.substr(0, br));
                label = rest.substr(br + 1, rest.size() - br - 2);
            } else if (rest.find(' ') != std::string::npos || rest.find('"') != std::string::npos) {
                id = "subgraph" + std::to_string(g.groups.size() + 1);
            }
            if (label.size() >= 2 && label.front() == '"' && label.back() == '"') label = label.substr(1, label.size() - 2);
            if (id.empty()) id = "subgraph" + std::to_string(g.groups.size() + 1);
            g.addGroup(id, cleanLabel(label), groupStack.empty() ? "" : groupStack.back());
            groupStack.push_back(id);
            continue;
        }
        if (head == "end") { if (!groupStack.empty()) groupStack.pop_back(); continue; }
        if (head == "direction" || head == "click" || head == "accTitle:" || head == "accdescr:") continue;
        if (head == "classdef") {
            std::istringstream in(s.substr(8));
            std::string names, css;
            in >> names;
            std::getline(in, css);
            std::stringstream ns(names);
            std::string nm;
            while (std::getline(ns, nm, ',')) applyCss(classes[trim(nm)], css);
            continue;
        }
        if (head == "class") {
            std::istringstream in(s.substr(5));
            std::string ids, cls;
            in >> ids >> cls;
            std::stringstream is(ids);
            std::string id;
            while (std::getline(is, id, ',')) nodeClasses.push_back({ trim(id), cls });
            continue;
        }
        if (head == "style") {
            std::istringstream in(s.substr(5));
            std::string id, css;
            in >> id;
            std::getline(in, css);
            if (!g.node(id)) g.addNode(id, id, Shape::Rect);
            applyCss(g.node(id)->style, css);
            continue;
        }
        if (head == "linkstyle") {
            std::istringstream in(s.substr(9));
            std::string which, css;
            in >> which;
            std::getline(in, css);
            linkStyles.push_back({ which, css });
            continue;
        }
        // A chain: group (link group)*, each group "a & b & c".
        size_t i = 0;
        std::vector<std::string> prev;
        MermaidLink pending;
        bool havePending = false;
        while (i < s.size()) {
            std::vector<std::string> grp;
            MermaidNodeRef r;
            if (!parseNodeRef(s, i, r, sm.line)) fail("mermaid: line %d: cannot read \"%s\"", sm.line, s);
            grp.push_back(touch(r));
            for (;;) {
                size_t p = i;
                while (p < s.size() && s[p] == ' ') p++;
                if (p < s.size() && s[p] == '&') {
                    i = p + 1;
                    if (!parseNodeRef(s, i, r, sm.line)) fail("mermaid: line %d: expected a node after & in \"%s\"", sm.line, s);
                    grp.push_back(touch(r));
                } else break;
            }
            if (havePending)
                for (const std::string& a : prev)
                    for (const std::string& b : grp) {
                        Edge& e = g.addEdge(a, b, pending.label);
                        e.head = pending.head; e.tail = pending.tail;
                        e.stroke = pending.stroke; e.width = pending.width;
                        e.minLength = pending.minLength;
                        if (pending.invisible) { e.kind = "invisible"; invisible.push_back(e.id); }
                    }
            prev = grp;
            while (i < s.size() && s[i] == ' ') i++;
            if (i >= s.size()) break;
            MermaidLink L;
            if (!parseLink(s, i, L)) fail("mermaid: line %d: expected a link in \"%s\"", sm.line, s);
            pending = L;
            havePending = true;
            while (i < s.size() && s[i] == ' ') i++;
            if (i >= s.size()) fail("mermaid: line %d: link without a target in \"%s\"", sm.line, s);
        }
    }
    for (auto& [id, cls] : nodeClasses) {
        Node* n = g.node(id);
        if (!n) continue;
        n->kind = cls;
        auto it = classes.find(cls);
        if (it == classes.end()) continue;
        if (it->second.fill) n->style.fill = it->second.fill;
        if (it->second.stroke) n->style.stroke = it->second.stroke;
        if (it->second.text) n->style.text = it->second.text;
        if (it->second.strokeWidth) n->style.strokeWidth = it->second.strokeWidth;
        if (it->second.dash) n->style.dash = it->second.dash;
    }
    if (classes.count("default"))
        for (Node& n : g.nodes) {
            const Style& d = classes["default"];
            if (d.fill && !n.style.fill) n.style.fill = d.fill;
            if (d.stroke && !n.style.stroke) n.style.stroke = d.stroke;
            if (d.text && !n.style.text) n.style.text = d.text;
        }
    for (auto& [which, css] : linkStyles) {
        if (lower(which) == "default") { for (Edge& e : g.edges) applyCss(e.style, css); continue; }
        std::stringstream ws(which);
        std::string k;
        while (std::getline(ws, k, ',')) {
            const int idx = std::atoi(k.c_str());
            if (idx >= 0 && idx < (int)g.edges.size()) applyCss(g.edges[idx].style, css);
        }
    }
    // Edges to a subgraph connect to its first member (as a stand-in for the box).
    for (Edge& e : g.edges)
        for (std::string* end : { &e.from, &e.to }) {
            if (!g.group(*end)) continue;
            Node* n = g.node(*end);
            if (n && !declared.count(*end)) {
                for (const Node& m : g.nodes)
                    if (m.id != *end && g.inGroup(m.id, *end)) { *end = m.id; break; }
            }
        }
    g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(), [&](const Node& n) {
        if (!g.group(n.id) || declared.count(n.id)) return false;
        for (const Edge& e : g.edges) if (e.from == n.id || e.to == n.id) return false;
        return true;
    }), g.nodes.end());
    return g;
}

// ------------------------------------------------------------------ Mermaid state diagram
Graph mermaidState(const std::vector<Stmt>& st) {
    Graph g;
    g.direction = Direction::TB;
    g.layout = LayoutKind::Layered;
    std::vector<std::string> scope;
    auto scoped = [&](const std::string& id, bool end) {
        if (id != "[*]") return id;
        const std::string base = (scope.empty() ? std::string() : scope.back() + "/") + (end ? "[*]end" : "[*]start");
        if (!g.node(base)) {
            Node& n = g.addNode(base, "", end ? Shape::EndState : Shape::StartState);
            n.label.clear();
            if (!scope.empty()) n.group = scope.back();
        }
        return base;
    };
    auto state = [&](const std::string& id) {
        if (!g.node(id)) {
            Node& n = g.addNode(id, id, Shape::Rounded);
            if (!scope.empty()) n.group = scope.back();
        }
        return g.node(id);
    };
    bool inNote = false;
    for (const Stmt& sm : st) {
        std::string s = sm.text;
        const std::string ls = lower(s);
        if (inNote) { if (ls.rfind("end note", 0) == 0) inNote = false; continue; }
        if (ls.rfind("note ", 0) == 0) { if (s.find(':') == std::string::npos) inNote = true; continue; }
        if (ls.rfind("direction", 0) == 0) { if (scope.empty()) g.direction = directionOf(s.substr(9)); continue; }
        if (s == "}") { if (!scope.empty()) scope.pop_back(); continue; }
        if (s == "--" || ls.rfind("classdef", 0) == 0 || ls.rfind("class ", 0) == 0) continue;
        if (ls.rfind("state ", 0) == 0) {
            std::string rest = trim(s.substr(6));
            const bool opens = !rest.empty() && rest.back() == '{';
            if (opens) rest = trim(rest.substr(0, rest.size() - 1));
            std::string id = rest, label;
            if (!rest.empty() && rest[0] == '"') {           // state "Long name" as id
                const size_t q = rest.find('"', 1);
                label = rest.substr(1, q - 1);
                const size_t as = lower(rest).find(" as ", q);
                id = as != std::string::npos ? trim(rest.substr(as + 4)) : label;
            } else {
                const size_t as = lower(rest).find(" as ");
                if (as != std::string::npos) { label = trim(rest.substr(0, as)); id = trim(rest.substr(as + 4)); }
            }
            std::string stereo;
            const size_t lt = id.find("<<");
            if (lt != std::string::npos) { stereo = lower(id.substr(lt)); id = trim(id.substr(0, lt)); }
            if (opens) {
                g.addGroup(id, label.empty() ? id : label, scope.empty() ? "" : scope.back());
                scope.push_back(id);
                continue;
            }
            Node* n = state(id);
            if (!label.empty()) n->label = cleanLabel(label);
            if (stereo.find("choice") != std::string::npos) { n->shape = Shape::Diamond; n->label.clear(); n->size = { 34, 34 }; }
            if (stereo.find("fork") != std::string::npos || stereo.find("join") != std::string::npos) {
                n->shape = Shape::Rect; n->label.clear(); n->size = { 90, 10 }; n->style.fill = Color{ 40, 44, 60, 255 };
            }
            continue;
        }
        const size_t arrow = s.find("-->");
        if (arrow != std::string::npos) {
            std::string a = trim(s.substr(0, arrow)), rest = s.substr(arrow + 3), label;
            const size_t colon = rest.find(':');
            if (colon != std::string::npos) { label = cleanLabel(rest.substr(colon + 1)); rest = rest.substr(0, colon); }
            std::string b = trim(rest);
            if (a.empty() || b.empty()) fail("mermaid: line %d: incomplete transition \"%s\"", sm.line, s);
            a = scoped(a, false);
            b = scoped(b, true);
            if (!g.node(a)) state(a);
            if (!g.node(b)) state(b);
            g.addEdge(a, b, label);
            continue;
        }
        const size_t colon = s.find(':');
        if (colon != std::string::npos) {                  // "id : description"
            Node* n = state(trim(s.substr(0, colon)));
            const std::string d = cleanLabel(s.substr(colon + 1));
            n->label = n->label == n->id ? d : n->label + "\n" + d;
            continue;
        }
        state(trim(s));
    }
    // A composite state referenced in a transition is drawn as its box: transitions enter at
    // its start (else its first state) and leave from its end (else its last state).
    for (Edge& e : g.edges)
        for (std::string* end : { &e.from, &e.to }) {
            if (!g.group(*end)) continue;
            const bool into = end == &e.to;
            std::string inner = *end + (into ? "/[*]start" : "/[*]end");
            if (!g.node(inner)) {
                inner.clear();
                for (const Node& m : g.nodes)
                    if (m.id != *end && g.inGroup(m.id, *end) && m.shape != Shape::StartState) {
                        inner = m.id;
                        if (into) break;
                    }
            }
            if (!inner.empty()) *end = inner;
        }
    g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(), [&](const Node& n) {
        if (!g.group(n.id)) return false;
        for (const Edge& e : g.edges) if (e.from == n.id || e.to == n.id) return false;
        return true;
    }), g.nodes.end());
    return g;
}

// ------------------------------------------------------------------ DOT
struct DotTok { enum Kind { Id, Punct, End } kind; std::string text; int line; bool quoted = false; };

std::vector<DotTok> dotTokens(std::string_view s) {
    std::vector<DotTok> out;
    int line = 1;
    size_t i = 0;
    bool lineStart = true;
    while (i < s.size()) {
        const char c = s[i];
        if (c == '\n') { line++; i++; lineStart = true; continue; }
        if (std::isspace((unsigned char)c)) { i++; continue; }
        if (lineStart && c == '#') { while (i < s.size() && s[i] != '\n') i++; continue; }
        lineStart = false;
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') { while (i < s.size() && s[i] != '\n') i++; continue; }
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
            i += 2;
            while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) { if (s[i] == '\n') line++; i++; }
            i += 2;
            continue;
        }
        if (c == '"') {
            std::string v;
            i++;
            while (i < s.size() && s[i] != '"') {
                if (s[i] == '\\' && i + 1 < s.size()) {
                    const char n = s[i + 1];
                    if (n == '"') v += '"';
                    else if (n == 'n' || n == 'l' || n == 'r') v += '\n';
                    else if (n == '\n') { line++; }
                    else { v += '\\'; v += n; }
                    i += 2;
                    continue;
                }
                if (s[i] == '\n') line++;
                v += s[i++];
            }
            i++;
            // "a" + "b" concatenation
            out.push_back({ DotTok::Id, v, line, true });
            continue;
        }
        if (c == '<') {                                   // HTML string: nested angle brackets
            int depth = 0;
            std::string v;
            do {
                if (s[i] == '<') depth++;
                else if (s[i] == '>') depth--;
                if (s[i] == '\n') line++;
                v += s[i++];
            } while (i < s.size() && depth > 0);
            out.push_back({ DotTok::Id, cleanLabel(v.substr(1, v.size() - 2)), line, true });
            continue;
        }
        if (c == '-' && i + 1 < s.size() && (s[i + 1] == '>' || s[i + 1] == '-')) {
            out.push_back({ DotTok::Punct, std::string(s.substr(i, 2)), line });
            i += 2;
            continue;
        }
        if (std::strchr("{}[]=;,:+", c)) { out.push_back({ DotTok::Punct, std::string(1, c), line }); i++; continue; }
        if (std::isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-' || (unsigned char)c >= 0x80) {
            size_t j = i;
            while (j < s.size() && (std::isalnum((unsigned char)s[j]) || s[j] == '_' || s[j] == '.' || (unsigned char)s[j] >= 0x80 ||
                                    (s[j] == '-' && j == i))) j++;
            out.push_back({ DotTok::Id, std::string(s.substr(i, j - i)), line });
            i = j;
            continue;
        }
        fail("dot: line %d: unexpected character '%s'", line, std::string(1, c));
    }
    out.push_back({ DotTok::End, "", line });
    return out;
}

using Attrs = std::map<std::string, std::string>;

class DotParser {
public:
    explicit DotParser(std::string_view text) : t_(dotTokens(text)) {}

    Graph run() {
        if (is("strict")) i_++;
        if (is("digraph")) { g_.directed = true; i_++; }
        else if (is("graph")) { g_.directed = false; i_++; }
        else fail("dot: line %d: expected graph or digraph, got \"%s\"", t_[i_].line, t_[i_].text);
        if (t_[i_].kind == DotTok::Id) i_++;
        g_.layout = LayoutKind::Layered;
        Scope root;
        expect("{");
        stmts(root, "");
        expect("}");
        finish();
        return std::move(g_);
    }

private:
    struct Scope { Attrs node, edge; std::vector<std::string> members; };

    bool is(const char* s) const { return t_[i_].kind != DotTok::End && lower(t_[i_].text) == s && !t_[i_].quoted; }
    bool punct(const char* s) const { return t_[i_].kind == DotTok::Punct && t_[i_].text == s; }
    void expect(const char* s) {
        if (!punct(s)) fail("dot: line %d: expected %s", t_[i_].line, s);
        i_++;
    }
    std::string id() {
        if (t_[i_].kind != DotTok::Id) fail("dot: line %d: expected an id, got \"%s\"", t_[i_].line, t_[i_].text);
        std::string v = t_[i_++].text;
        while (punct("+") && t_[i_ + 1].kind == DotTok::Id) { i_++; v += t_[i_++].text; }
        return v;
    }
    void attrList(Attrs& a) {
        while (punct("[")) {
            i_++;
            while (!punct("]")) {
                const std::string k = lower(id());
                std::string v = "true";
                if (punct("=")) { i_++; v = id(); }
                a[k] = v;
                if (punct(",") || punct(";")) i_++;
            }
            i_++;
        }
    }
    // node_id [port]; returns the id
    std::string nodeId() {
        std::string n = id();
        if (punct(":")) { i_++; id(); if (punct(":")) { i_++; id(); } }
        return n;
    }
    void touch(const std::string& n, Scope& sc, const std::string& group) {
        if (!g_.node(n)) {
            Node& nd = g_.addNode(n, n, Shape::Ellipse);
            nd.group = group;
            nodeAttrs_[n] = sc.node;
        } else if (!group.empty() && g_.node(n)->group.empty()) {
            g_.node(n)->group = group;
        }
        sc.members.push_back(n);
    }
    // A subgraph or a single node; returns the node ids it stands for.
    std::vector<std::string> operand(Scope& sc, const std::string& group) {
        if (is("subgraph") || punct("{")) {
            Scope inner = sc;
            inner.members.clear();
            subgraph(inner, group);
            for (const auto& m : inner.members) sc.members.push_back(m);
            return inner.members;
        }
        const std::string n = nodeId();
        touch(n, sc, group);
        return { n };
    }
    void subgraph(Scope& sc, const std::string& parentGroup) {
        std::string name;
        if (is("subgraph")) { i_++; if (t_[i_].kind == DotTok::Id) name = id(); }
        std::string group = parentGroup;
        const bool cluster = lower(name).rfind("cluster", 0) == 0;
        if (cluster) {
            g_.addGroup(name, "", parentGroup);
            group = name;
        }
        expect("{");
        stmts(sc, group, cluster ? name : "");
        expect("}");
    }
    void stmts(Scope& sc, const std::string& group, const std::string& clusterId = "") {
        while (!punct("}") && t_[i_].kind != DotTok::End) {
            if (punct(";")) { i_++; continue; }
            if (is("graph") || is("node") || is("edge")) {
                const std::string kind = lower(t_[i_++].text);
                Attrs a;
                attrList(a);
                if (kind == "node") for (auto& [k, v] : a) sc.node[k] = v;
                else if (kind == "edge") for (auto& [k, v] : a) sc.edge[k] = v;
                else graphAttrs(a, clusterId);
                continue;
            }
            if (t_[i_].kind == DotTok::Id && t_[i_ + 1].kind == DotTok::Punct && t_[i_ + 1].text == "=") {
                const std::string k = lower(id());
                i_++;
                graphAttrs({ { k, id() } }, clusterId);
                continue;
            }
            std::vector<std::string> left = operand(sc, group);
            if (punct("->") || punct("--")) {
                std::vector<std::vector<std::string>> chain{ left };
                while (punct("->") || punct("--")) { i_++; chain.push_back(operand(sc, group)); }
                Attrs a = sc.edge;
                attrList(a);
                for (size_t k = 0; k + 1 < chain.size(); k++)
                    for (const auto& u : chain[k])
                        for (const auto& v : chain[k + 1]) edges_.push_back({ u, v, a });
            } else if (left.size() == 1 && punct("[")) {
                Attrs a;
                attrList(a);
                for (auto& [k, v] : a) nodeAttrs_[left[0]][k] = v;
            }
        }
    }
    void graphAttrs(const Attrs& a, const std::string& clusterId) {
        for (auto& [k, v] : a) {
            if (!clusterId.empty()) {
                Group* gr = g_.group(clusterId);
                if (k == "label") gr->label = cleanLabel(v);
                else if (k == "color" || k == "pencolor") { if (auto c = colorOf(v)) gr->style.stroke = c; }
                else if (k == "fillcolor" || k == "bgcolor") { if (auto c = colorOf(v)) gr->style.fill = c; }
                else if (k == "fontcolor") { if (auto c = colorOf(v)) gr->style.text = c; }
                continue;
            }
            if (k == "rankdir") g_.direction = directionOf(v);
            else if (k == "label") g_.title = cleanLabel(v);
            else if (k == "layout") {
                const std::string l = lower(v);
                if (l == "neato" || l == "fdp" || l == "sfdp") g_.layout = LayoutKind::Force;
                else if (l == "circo") g_.layout = LayoutKind::Circle;
                else if (l == "twopi") g_.layout = LayoutKind::Radial;
            }
        }
    }
    static Shape shapeOf(const std::string& s, bool rounded) {
        const std::string v = lower(s);
        if (v == "box" || v == "rect" || v == "rectangle" || v == "square" || v == "box3d" || v == "component" ||
            v == "tab" || v == "folder" || v == "msquare" || v == "record" || v == "underline")
            return rounded ? Shape::Rounded : Shape::Rect;
        if (v == "mrecord") return Shape::Rounded;
        if (v == "circle") return Shape::Circle;
        if (v == "doublecircle" || v == "mcircle") return Shape::DoubleCircle;
        if (v == "diamond" || v == "mdiamond") return Shape::Diamond;
        if (v == "hexagon" || v == "octagon" || v == "doubleoctagon" || v == "tripleoctagon") return Shape::Hexagon;
        if (v == "parallelogram") return Shape::Parallelogram;
        if (v == "trapezium" || v == "house") return Shape::Trapezoid;
        if (v == "invtrapezium" || v == "invhouse") return Shape::TrapezoidAlt;
        if (v == "cylinder") return Shape::Cylinder;
        if (v == "note") return Shape::Note;
        if (v == "point") return Shape::Point;
        if (v == "plaintext" || v == "plain" || v == "none") return Shape::Text;
        if (v == "cds" || v == "rarrow" || v == "larrow") return Shape::Flag;
        return Shape::Ellipse;
    }
    static Arrow arrowOf(const std::string& s) {
        const std::string v = lower(s);
        if (v == "none") return Arrow::None;
        if (v == "vee" || v == "open" || v == "lvee" || v == "rvee") return Arrow::Open;
        if (v == "dot") return Arrow::Circle;
        if (v == "odot") return Arrow::OpenCircle;
        if (v == "diamond" || v == "box") return Arrow::Diamond;
        if (v == "odiamond" || v == "obox") return Arrow::OpenDiamond;
        if (v == "tee" || v == "crow" || v == "inv") return Arrow::Cross;
        return Arrow::Normal;
    }
    static void styleOf(const Attrs& a, Style& st, bool& rounded, bool& invisible, float& width) {
        auto it = a.find("style");
        if (it != a.end()) {
            const std::string s = lower(it->second);
            if (s.find("dashed") != std::string::npos) st.dash = Stroke::Dashed;
            if (s.find("dotted") != std::string::npos) st.dash = Stroke::Dotted;
            if (s.find("bold") != std::string::npos) width = 2;
            if (s.find("rounded") != std::string::npos) rounded = true;
            if (s.find("invis") != std::string::npos) invisible = true;
        }
        if ((it = a.find("color")) != a.end()) {
            const std::string first = it->second.substr(0, it->second.find(':'));
            if (auto c = colorOf(first)) st.stroke = c;
        }
        if ((it = a.find("fillcolor")) != a.end()) { if (auto c = colorOf(it->second)) st.fill = c; }
        else if (a.count("style") && lower(a.at("style")).find("filled") != std::string::npos && st.stroke) st.fill = st.stroke;
        if ((it = a.find("fontcolor")) != a.end()) { if (auto c = colorOf(it->second)) st.text = c; }
        if ((it = a.find("penwidth")) != a.end()) width = std::max(0.5f, (float)std::atof(it->second.c_str()));
    }
    void finish() {
        bool allPos = !g_.nodes.empty();
        for (Node& n : g_.nodes) {
            const Attrs& a = nodeAttrs_[n.id];
            bool rounded = false, invisible = false;
            float width = 1;
            Style st;
            styleOf(a, st, rounded, invisible, width);
            n.style = st;
            if (width != 1) n.style.strokeWidth = 2.5f * width;
            auto it = a.find("shape");
            n.shape = shapeOf(it != a.end() ? it->second : "ellipse", rounded);
            if ((it = a.find("label")) != a.end()) {
                std::string l = it->second;
                auto sh = a.find("shape");
                if (sh != a.end() && lower(sh->second).find("record") != std::string::npos) {
                    std::string r;
                    for (char c : l) { if (c == '{' || c == '}') continue; r += c == '|' ? '\n' : c; }
                    l = r;
                }
                n.label = l == "\\N" ? n.id : cleanLabel(l);
            }
            if (n.shape == Shape::Point) n.label.clear();
            if (invisible) n.style.fill = n.style.stroke = n.style.text = Color{ 0, 0, 0, 0 };
            if ((it = a.find("pos")) != a.end()) {
                float x = 0, y = 0;
                if (std::sscanf(it->second.c_str(), "%f,%f", &x, &y) == 2) n.pos = { x, -y };
                else allPos = false;
            } else allPos = false;
            n.data = Json(Json::Object{});
            for (auto& [k, v] : a) n.data.set(k, v);
        }
        if (allPos) g_.layout = LayoutKind::None;
        for (auto& [u, v, a] : edges_) {
            auto lab = a.find("label");
            if (lab == a.end()) lab = a.find("xlabel");
            Edge& e = g_.addEdge(u, v, lab != a.end() ? cleanLabel(lab->second) : "");
            bool rounded = false, invisible = false;
            float width = 1;
            styleOf(a, e.style, rounded, invisible, width);
            e.width = width;
            if (e.style.dash) e.stroke = *e.style.dash;
            std::string dirv = g_.directed ? "forward" : "none";
            if (a.count("dir")) dirv = lower(a.at("dir"));
            e.head = dirv == "forward" || dirv == "both" ? (a.count("arrowhead") ? arrowOf(a.at("arrowhead")) : Arrow::Normal) : Arrow::None;
            e.tail = dirv == "back" || dirv == "both" ? (a.count("arrowtail") ? arrowOf(a.at("arrowtail")) : Arrow::Normal) : Arrow::None;
            if (a.count("minlen")) e.minLength = std::max(1, std::atoi(a.at("minlen").c_str()));
            if (invisible) e.kind = "invisible";
        }
    }

    std::vector<DotTok> t_;
    size_t i_ = 0;
    Graph g_;
    std::map<std::string, Attrs> nodeAttrs_;
    struct PendingEdge { std::string u, v; Attrs a; };
    std::vector<PendingEdge> edges_;
};

// ------------------------------------------------------------------ Newick
class NewickParser {
public:
    explicit NewickParser(std::string_view s) : s_(s) {}
    Graph run() {
        g_.layout = LayoutKind::Tree;
        g_.direction = Direction::LR;
        skip();
        subtree("");
        skip();
        if (i_ < s_.size() && s_[i_] == ';') i_++;
        skip();
        if (i_ < s_.size()) fail("newick: line %d: trailing text \"%s\"", line_, std::string(s_.substr(i_, 20)));
        // Unique labels become the ids, so animations can name tips.
        std::map<std::string, int> uses;
        for (const Node& n : g_.nodes) if (!n.label.empty()) uses[n.label]++;
        std::map<std::string, std::string> rename;
        for (Node& n : g_.nodes)
            if (!n.label.empty() && uses[n.label] == 1 && !g_.node(n.label)) { rename[n.id] = n.label; }
        for (Node& n : g_.nodes) if (rename.count(n.id)) n.id = rename[n.id];
        for (Edge& e : g_.edges) {
            if (rename.count(e.from)) e.from = rename[e.from];
            if (rename.count(e.to)) e.to = rename[e.to];
            e.id = e.from + "->" + e.to;
        }
        return std::move(g_);
    }

private:
    void skip() {
        for (;;) {
            while (i_ < s_.size() && std::isspace((unsigned char)s_[i_])) { if (s_[i_] == '\n') line_++; i_++; }
            if (i_ < s_.size() && s_[i_] == '[') { while (i_ < s_.size() && s_[i_] != ']') i_++; i_++; continue; }
            break;
        }
    }
    std::string label() {
        skip();
        std::string v;
        if (i_ < s_.size() && s_[i_] == '\'') {
            i_++;
            while (i_ < s_.size()) {
                if (s_[i_] == '\'' && i_ + 1 < s_.size() && s_[i_ + 1] == '\'') { v += '\''; i_ += 2; continue; }
                if (s_[i_] == '\'') { i_++; break; }
                v += s_[i_++];
            }
            return v;
        }
        while (i_ < s_.size() && !std::strchr("():,;[", s_[i_]) && !std::isspace((unsigned char)s_[i_])) {
            v += s_[i_] == '_' ? ' ' : s_[i_];
            i_++;
        }
        return v;
    }
    std::string subtree(const std::string& parent) {
        skip();
        const std::string id = "n" + std::to_string(g_.nodes.size());
        Node& n = g_.addNode(id, "", Shape::Point);
        n.label.clear();
        std::vector<std::string> kids;
        if (i_ < s_.size() && s_[i_] == '(') {
            i_++;
            for (;;) {
                kids.push_back(subtree(id));
                skip();
                if (i_ < s_.size() && s_[i_] == ',') { i_++; continue; }
                if (i_ < s_.size() && s_[i_] == ')') { i_++; break; }
                fail("newick: line %d: expected , or ) %s", line_, "");
            }
        }
        const std::string lab = label();
        Node* me = g_.node(id);
        me->label = lab;
        me->labelPlace = kids.empty() ? LabelPlace::Right : LabelPlace::Above;
        me->size = kids.empty() ? Vector2{ 8, 8 } : Vector2{ 7, 7 };
        me->data = Json(Json::Object{});
        skip();
        if (i_ < s_.size() && s_[i_] == ':') {
            i_++;
            skip();
            size_t j = i_;
            while (j < s_.size() && (std::isdigit((unsigned char)s_[j]) || std::strchr("+-.eE", s_[j]))) j++;
            me->data.set("length", std::atof(std::string(s_.substr(i_, j - i_)).c_str()));
            i_ = j;
        }
        if (!parent.empty()) {
            Edge& e = g_.addEdge(parent, id);
            e.head = Arrow::None;
        }
        return id;
    }
    std::string_view s_;
    size_t i_ = 0;
    int line_ = 1;
    Graph g_;
};

// ------------------------------------------------------------------ RDF
struct Term {
    enum Kind { Iri, Blank, Literal } kind = Iri;
    std::string value, lang, datatype;
    bool operator==(const Term& o) const { return kind == o.kind && value == o.value && lang == o.lang && datatype == o.datatype; }
};
struct Triple { Term s, p, o; };

const char* RDF = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const char* XSD = "http://www.w3.org/2001/XMLSchema#";

class TurtleParser {
public:
    explicit TurtleParser(std::string_view s) : s_(s) {
        prefixes_["rdf"] = RDF;
        prefixes_["rdfs"] = "http://www.w3.org/2000/01/rdf-schema#";
        prefixes_["xsd"] = XSD;
        prefixes_["owl"] = "http://www.w3.org/2002/07/owl#";
    }
    std::vector<Triple> run() {
        for (;;) {
            ws();
            if (i_ >= s_.size()) break;
            if (directive()) continue;
            Term subj;
            ws();
            if (peek() == '[') {
                subj = blankList();
                ws();
                if (peek() == '.') { i_++; continue; }
            } else subj = subject();
            predicateObjects(subj);
            ws();
            if (peek() != '.') err("expected '.'");
            i_++;
        }
        return std::move(out_);
    }
    std::map<std::string, std::string> prefixes_;

private:
    [[noreturn]] void err(const char* what) {
        fail("turtle: line %d: %s", line_, std::string(what) + " near \"" + std::string(s_.substr(i_, 24)) + "\"");
    }
    char peek(size_t k = 0) const { return i_ + k < s_.size() ? s_[i_ + k] : '\0'; }
    void ws() {
        for (;;) {
            while (i_ < s_.size() && std::isspace((unsigned char)s_[i_])) { if (s_[i_] == '\n') line_++; i_++; }
            if (peek() == '#') { while (i_ < s_.size() && s_[i_] != '\n') i_++; continue; }
            break;
        }
    }
    bool word(const char* w, bool ci) {
        const size_t n = std::strlen(w);
        if (i_ + n > s_.size()) return false;
        std::string got(s_.substr(i_, n));
        if (ci ? lower(got) != lower(w) : got != w) return false;
        const char after = peek(n);
        if (std::isalnum((unsigned char)after) || after == ':' || after == '_') return false;
        i_ += n;
        return true;
    }
    bool directive() {
        const size_t save = i_;
        bool sparql = false;
        if (word("@prefix", false) || (sparql = word("PREFIX", true))) {
            ws();
            size_t j = i_;
            while (j < s_.size() && s_[j] != ':') j++;
            const std::string pfx = trim(s_.substr(i_, j - i_));
            i_ = j + 1;
            ws();
            prefixes_[pfx] = resolve(iriRef());
            ws();
            if (!sparql) { if (peek() != '.') err("expected '.' after @prefix"); i_++; }
            return true;
        }
        if (word("@base", false) || (sparql = word("BASE", true))) {
            ws();
            base_ = iriRef();
            ws();
            if (!sparql) { if (peek() != '.') err("expected '.' after @base"); i_++; }
            return true;
        }
        i_ = save;
        return false;
    }
    std::string resolve(const std::string& iri) const {
        if (iri.find(':') != std::string::npos || base_.empty()) return iri;
        if (!iri.empty() && iri[0] == '#') return base_ + iri;
        const size_t slash = base_.rfind('/');
        return (slash == std::string::npos ? base_ : base_.substr(0, slash + 1)) + iri;
    }
    std::string iriRef() {
        if (peek() != '<') err("expected <iri>");
        const size_t close = s_.find('>', i_);
        if (close == std::string::npos) err("unterminated <iri>");
        std::string v(s_.substr(i_ + 1, close - i_ - 1));
        i_ = close + 1;
        return v;
    }
    Term iri() {
        ws();
        if (peek() == '<') return { Term::Iri, resolve(iriRef()) };
        // prefixed name
        size_t j = i_;
        while (j < s_.size() && (std::isalnum((unsigned char)s_[j]) || s_[j] == '_' || s_[j] == '-' || (unsigned char)s_[j] >= 0x80)) j++;
        if (j >= s_.size() || s_[j] != ':') err("expected an IRI or prefixed name");
        const std::string pfx(s_.substr(i_, j - i_));
        size_t k = j + 1;
        while (k < s_.size() && (std::isalnum((unsigned char)s_[k]) || std::strchr("_-.%:", s_[k]) || (unsigned char)s_[k] >= 0x80)) {
            if (s_[k] == '\\' && k + 1 < s_.size()) { k += 2; continue; }
            k++;
        }
        while (k > j + 1 && s_[k - 1] == '.') k--;      // a trailing '.' ends the statement
        auto it = prefixes_.find(pfx);
        if (it == prefixes_.end()) err(("undeclared prefix '" + pfx + "'").c_str());
        Term t{ Term::Iri, it->second + std::string(s_.substr(j + 1, k - j - 1)) };
        i_ = k;
        return t;
    }
    Term blankLabel() {
        size_t j = i_ + 2;
        while (j < s_.size() && (std::isalnum((unsigned char)s_[j]) || s_[j] == '_' || s_[j] == '-')) j++;
        Term t{ Term::Blank, "_:" + std::string(s_.substr(i_ + 2, j - i_ - 2)) };
        i_ = j;
        return t;
    }
    Term freshBlank() { return { Term::Blank, "_:b" + std::to_string(++blank_) }; }
    Term subject() {
        ws();
        if (peek() == '_' && peek(1) == ':') return blankLabel();
        if (peek() == '(') return collection();
        return iri();
    }
    Term blankList() {
        i_++;                                            // [
        Term b = freshBlank();
        ws();
        if (peek() != ']') predicateObjects(b);
        ws();
        if (peek() != ']') err("expected ']'");
        i_++;
        return b;
    }
    Term collection() {
        i_++;                                            // (
        std::vector<Term> items;
        for (;;) {
            ws();
            if (peek() == ')') { i_++; break; }
            if (i_ >= s_.size()) err("unterminated collection");
            items.push_back(object());
        }
        if (items.empty()) return { Term::Iri, std::string(RDF) + "nil" };
        // rdf:first / rdf:rest chains read badly as a picture: link the items from one list node.
        Term head = freshBlank();
        for (const Term& it : items) out_.push_back({ head, { Term::Iri, std::string(RDF) + "li" }, it });
        return head;
    }
    std::string stringBody() {
        const char q = peek();
        const bool longQ = peek(1) == q && peek(2) == q;
        i_ += longQ ? 3 : 1;
        std::string v;
        while (i_ < s_.size()) {
            if (longQ ? (peek() == q && peek(1) == q && peek(2) == q) : peek() == q) { i_ += longQ ? 3 : 1; return v; }
            char c = s_[i_];
            if (c == '\n') { if (!longQ) err("newline in string"); line_++; }
            if (c == '\\' && i_ + 1 < s_.size()) {
                const char n = s_[i_ + 1];
                i_ += 2;
                switch (n) {
                case 'n': v += '\n'; break;
                case 't': v += '\t'; break;
                case 'r': break;
                case 'u': case 'U': {
                    const int len = n == 'u' ? 4 : 8;
                    const int cp = (int)std::strtol(std::string(s_.substr(i_, len)).c_str(), nullptr, 16);
                    int bytes = 0;
                    const char* u = CodepointToUTF8(cp, &bytes);
                    v.append(u, (size_t)bytes);
                    i_ += len;
                    break;
                }
                default: v += n;
                }
                continue;
            }
            v += c;
            i_++;
        }
        err("unterminated string");
    }
    Term object() {
        ws();
        const char c = peek();
        if (c == '<') return iri();
        if (c == '_' && peek(1) == ':') return blankLabel();
        if (c == '[') return blankList();
        if (c == '(') return collection();
        if (c == '"' || c == '\'') {
            Term t{ Term::Literal, stringBody() };
            if (peek() == '@') {
                size_t j = i_ + 1;
                while (j < s_.size() && (std::isalnum((unsigned char)s_[j]) || s_[j] == '-')) j++;
                t.lang = std::string(s_.substr(i_ + 1, j - i_ - 1));
                i_ = j;
            } else if (peek() == '^' && peek(1) == '^') {
                i_ += 2;
                t.datatype = iri().value;
            }
            return t;
        }
        if (std::isdigit((unsigned char)c) || ((c == '-' || c == '+' || c == '.') && std::isdigit((unsigned char)peek(1)))) {
            size_t j = i_ + 1;
            while (j < s_.size() && (std::isdigit((unsigned char)s_[j]) || std::strchr("eE+-", s_[j]) ||
                                     (s_[j] == '.' && j + 1 < s_.size() && std::isdigit((unsigned char)s_[j + 1])))) j++;
            Term t{ Term::Literal, std::string(s_.substr(i_, j - i_)) };
            t.datatype = std::string(XSD) + (t.value.find_first_of("eE") != std::string::npos ? "double" :
                                             t.value.find('.') != std::string::npos ? "decimal" : "integer");
            i_ = j;
            return t;
        }
        if (word("true", false) || word("false", false)) {
            Term t{ Term::Literal, s_[i_ - 1] == 'e' && s_[i_ - 2] == 'u' ? "true" : "false" };
            t.datatype = std::string(XSD) + "boolean";
            return t;
        }
        return iri();
    }
    void predicateObjects(const Term& subj) {
        for (;;) {
            ws();
            Term pred;
            if (peek() == 'a' && (std::isspace((unsigned char)peek(1)) || peek(1) == '<' || peek(1) == '[')) {
                i_++;
                pred = { Term::Iri, std::string(RDF) + "type" };
            } else pred = iri();
            for (;;) {
                Term obj = object();
                out_.push_back({ subj, pred, obj });
                ws();
                if (peek() == ',') { i_++; continue; }
                break;
            }
            ws();
            if (peek() == ';') {
                while (peek() == ';') { i_++; ws(); }
                if (peek() == '.' || peek() == ']' || i_ >= s_.size()) return;
                continue;
            }
            return;
        }
    }
    std::string_view s_;
    size_t i_ = 0;
    int line_ = 1;
    int blank_ = 0;
    std::string base_;
    std::vector<Triple> out_;
};

// Triples to a graph: resources become nodes, links become edges, literals become small
// value nodes (or node data), labels and types fold into their subjects.
Graph rdfGraph(const std::vector<Triple>& triples, const std::map<std::string, std::string>& declared, const RdfOptions& o) {
    std::map<std::string, std::string> prefixes = {
        { "rdf", RDF }, { "rdfs", "http://www.w3.org/2000/01/rdf-schema#" }, { "xsd", XSD },
        { "owl", "http://www.w3.org/2002/07/owl#" }, { "foaf", "http://xmlns.com/foaf/0.1/" },
        { "schema", "http://schema.org/" }, { "skos", "http://www.w3.org/2004/02/skos/core#" },
        { "dc", "http://purl.org/dc/elements/1.1/" }, { "dcterms", "http://purl.org/dc/terms/" },
        { "wd", "http://www.wikidata.org/entity/" }, { "wdt", "http://www.wikidata.org/prop/direct/" },
    };
    for (auto& [k, v] : declared) prefixes[k] = v;
    auto localName = [](const std::string& iri) {
        size_t cut = iri.find_last_of("#/");
        std::string l = cut == std::string::npos ? iri : iri.substr(cut + 1);
        if (l.empty() && cut != std::string::npos && cut > 0) {
            const size_t prev = iri.find_last_of("#/", cut - 1);
            l = iri.substr(prev + 1, cut - prev - 1);
        }
        return l.empty() ? iri : l;
    };
    auto compact = [&](const std::string& iri) {
        std::string best, pfx;
        for (auto& [k, v] : prefixes) if (!v.empty() && iri.rfind(v, 0) == 0 && v.size() > best.size()) { best = v; pfx = k; }
        if (best.empty()) return iri;
        return (pfx.empty() ? std::string(":") : pfx + ":") + iri.substr(best.size());
    };
    static const std::vector<std::string> labelPreds = {
        "http://www.w3.org/2000/01/rdf-schema#label", "http://www.w3.org/2004/02/skos/core#prefLabel",
        "http://xmlns.com/foaf/0.1/name", "http://schema.org/name", "https://schema.org/name",
        "http://purl.org/dc/elements/1.1/title", "http://purl.org/dc/terms/title",
    };
    const std::string type = std::string(RDF) + "type";
    auto key = [](const Term& t) { return t.kind == Term::Blank ? t.value : t.value; };

    Graph g;
    g.layout = LayoutKind::Force;
    g.directed = true;
    std::unordered_map<std::string, int> labelRank;      // best label found per node (lower = better)
    std::unordered_map<std::string, std::string> label;
    auto ensure = [&](const Term& t) -> Node& {
        const std::string id = key(t);
        if (Node* n = g.node(id)) return *n;
        Node& n = g.addNode(id, t.kind == Term::Blank ? "" : localName(t.value), t.kind == Term::Blank ? Shape::Point : Shape::Stadium);
        if (t.kind == Term::Blank) { n.label.clear(); n.size = { 16, 16 }; }
        n.data = Json(Json::Object{});
        n.data.set("iri", t.kind == Term::Blank ? t.value : compact(t.value));
        return n;
    };
    // First pass: labels and types.
    for (const Triple& tr : triples) {
        if (tr.o.kind != Term::Literal || tr.s.kind == Term::Literal) continue;
        const auto lp = std::find(labelPreds.begin(), labelPreds.end(), tr.p.value);
        if (lp == labelPreds.end()) continue;
        const int rank = (int)(lp - labelPreds.begin()) * 3 + (tr.o.lang == o.language ? 0 : tr.o.lang.empty() ? 1 : 2);
        const std::string k = key(tr.s);
        if (!labelRank.count(k) || rank < labelRank[k]) { labelRank[k] = rank; label[k] = tr.o.value; }
    }
    int literals = 0;
    for (const Triple& tr : triples) {
        Node& s = ensure(tr.s);              // (a reference only until the next node is added)
        const std::string sid = s.id;
        if (label.count(key(tr.s))) s.label = label[key(tr.s)];
        const bool isLabel = std::find(labelPreds.begin(), labelPreds.end(), tr.p.value) != labelPreds.end();
        if (tr.p.value == type && tr.o.kind != Term::Literal) {
            const std::string cls = localName(tr.o.value);
            if (s.kind.empty()) s.kind = cls;
            if (o.typeEdges) {
                Node& c = ensure(tr.o);
                c.kind = "@class";
                g.addEdge(sid, c.id, "type").kind = "rdf:type";
            }
            continue;
        }
        if (tr.o.kind == Term::Literal) {
            if (isLabel) continue;
            s.data.set(localName(tr.p.value), tr.o.value);
            if (!o.literalNodes) continue;
            std::string v = tr.o.value;
            if ((int)v.size() > o.maxLiteralLength) {
                size_t cut = (size_t)o.maxLiteralLength;
                while (cut > 0 && ((unsigned char)v[cut] & 0xC0) == 0x80) cut--;
                v = v.substr(0, cut) + "\xE2\x80\xA6";
            }
            Node& lit = g.addNode("lit:" + std::to_string(++literals), v, Shape::Rect);
            lit.kind = "@literal";
            lit.data = Json(Json::Object{});
            if (!tr.o.datatype.empty()) lit.data.set("datatype", compact(tr.o.datatype));
            if (!tr.o.lang.empty()) lit.data.set("lang", tr.o.lang);
            Edge& e = g.addEdge(sid, lit.id, localName(tr.p.value));
            e.kind = compact(tr.p.value);
            continue;
        }
        Node& ob = ensure(tr.o);
        if (label.count(key(tr.o))) ob.label = label[key(tr.o)];
        const std::string p = tr.p.value == std::string(RDF) + "li" ? "" : localName(tr.p.value);
        Edge& e = g.addEdge(sid, ob.id, p);
        e.kind = compact(tr.p.value);
    }
    // Blank nodes with a label or a type are drawn like resources (the type in brackets).
    for (Node& n : g.nodes) {
        if (n.shape != Shape::Point || (n.label.empty() && n.kind.empty())) continue;
        n.shape = Shape::Stadium;
        if (n.label.empty()) n.label = "(" + n.kind + ")";
        n.size = { 0, 0 };
    }
    return g;
}

// ------------------------------------------------------------------ JSON-LD
class JsonLd {
public:
    JsonLd(const RdfOptions& o) : o_(o) {}
    Graph run(const Json& doc) {
        Ctx root;
        if (doc.isArray()) for (const Json& it : doc.asArray()) node(it, root);
        else node(doc, root);
        return rdfGraph(out_, prefixes_, o_);
    }
    std::map<std::string, std::string> prefixes_;

private:
    struct Ctx { std::map<std::string, std::string> terms; std::map<std::string, bool> idTyped; std::string vocab, base; };
    void context(const Json& c, Ctx& ctx) {
        if (c.isArray()) { for (const Json& it : c.asArray()) context(it, ctx); return; }
        if (!c.isObject()) return;
        for (auto& [k, v] : c.asObject()) {
            if (k == "@vocab" && v.isString()) { ctx.vocab = v.asString(); continue; }
            if (k == "@base" && v.isString()) { ctx.base = v.asString(); continue; }
            if (v.isString()) {
                ctx.terms[k] = v.asString();
                const std::string& s = v.asString();
                if (!s.empty() && (s.back() == '/' || s.back() == '#')) prefixes_[k] = s;
            } else if (v.isObject()) {
                if (v["@id"].isString()) ctx.terms[k] = v["@id"].asString();
                if (v["@type"].isString() && v["@type"].asString() == "@id") ctx.idTyped[k] = true;
            }
        }
    }
    std::string expand(const std::string& t, const Ctx& ctx, bool vocab) const {
        if (t.rfind("_:", 0) == 0) return t;
        auto it = ctx.terms.find(t);
        if (it != ctx.terms.end()) return expand(it->second, ctx, false);
        const size_t colon = t.find(':');
        if (colon != std::string::npos) {
            const std::string pfx = t.substr(0, colon);
            auto p = ctx.terms.find(pfx);
            if (p != ctx.terms.end()) return p->second + t.substr(colon + 1);
            return t;
        }
        if (vocab && !ctx.vocab.empty()) return ctx.vocab + t;
        if (!vocab && !ctx.base.empty()) return ctx.base + t;
        return t;
    }
    Term node(const Json& obj, Ctx ctx) {
        if (!obj.isObject()) return {};
        if (obj.has("@context")) context(obj["@context"], ctx);
        if (obj.has("@graph") && !obj.has("@id")) {
            const Json& gr = obj["@graph"];
            if (gr.isArray()) for (const Json& it : gr.asArray()) node(it, ctx);
            else node(gr, ctx);
            return {};
        }
        Term subj = obj["@id"].isString() ? Term{ obj["@id"].asString().rfind("_:", 0) == 0 ? Term::Blank : Term::Iri, expand(obj["@id"].asString(), ctx, false) }
                                          : Term{ Term::Blank, "_:j" + std::to_string(++blank_) };
        for (auto& [k, v] : obj.asObject()) {
            if (k == "@context" || k == "@id") continue;
            if (k == "@type") {
                auto add = [&](const Json& t) { if (t.isString()) out_.push_back({ subj, { Term::Iri, std::string(RDF) + "type" }, { Term::Iri, expand(t.asString(), ctx, true) } }); };
                if (v.isArray()) for (const Json& t : v.asArray()) add(t); else add(v);
                continue;
            }
            if (!k.empty() && k[0] == '@') continue;
            const Term pred{ Term::Iri, expand(k, ctx, true) };
            const bool idTyped = ctx.idTyped.count(k) > 0;
            auto value = [&](const Json& val) {
                if (val.isObject()) {
                    if (val.has("@value")) {
                        Term lit{ Term::Literal, val["@value"].isString() ? val["@value"].asString() : val["@value"].dump() };
                        if (val["@language"].isString()) lit.lang = val["@language"].asString();
                        out_.push_back({ subj, pred, lit });
                        return;
                    }
                    if (val.has("@list")) {
                        for (const Json& it : val["@list"].asArray()) out_.push_back({ subj, pred, it.isObject() ? node(it, ctx) : Term{ Term::Literal, it.isString() ? it.asString() : it.dump() } });
                        return;
                    }
                    const Term o = node(val, ctx);
                    if (!o.value.empty()) out_.push_back({ subj, pred, o });
                    return;
                }
                if (val.isString()) {
                    if (idTyped) out_.push_back({ subj, pred, { Term::Iri, expand(val.asString(), ctx, false) } });
                    else out_.push_back({ subj, pred, { Term::Literal, val.asString() } });
                    return;
                }
                if (!val.isNull()) out_.push_back({ subj, pred, { Term::Literal, val.dump() } });
            };
            if (v.isArray()) for (const Json& it : v.asArray()) value(it); else value(v);
        }
        return subj;
    }
    RdfOptions o_;
    std::vector<Triple> out_;
    int blank_ = 0;
};

Shape shapeName(const std::string& s) {
    static const std::map<std::string, Shape> names = {
        { "rect", Shape::Rect }, { "box", Shape::Rect }, { "rounded", Shape::Rounded }, { "stadium", Shape::Stadium },
        { "pill", Shape::Stadium }, { "circle", Shape::Circle }, { "doublecircle", Shape::DoubleCircle },
        { "ellipse", Shape::Ellipse }, { "diamond", Shape::Diamond }, { "hexagon", Shape::Hexagon },
        { "parallelogram", Shape::Parallelogram }, { "trapezoid", Shape::Trapezoid }, { "cylinder", Shape::Cylinder },
        { "database", Shape::Cylinder }, { "subroutine", Shape::Subroutine }, { "flag", Shape::Flag }, { "note", Shape::Note },
        { "document", Shape::Document }, { "point", Shape::Point }, { "dot", Shape::Point }, { "text", Shape::Text },
    };
    auto it = names.find(lower(s));
    return it == names.end() ? Shape::Rounded : it->second;
}
} // namespace

// ------------------------------------------------------------------ public entry points
Graph mermaid(std::string_view text) {
    std::vector<Stmt> st = statements(text, true);
    // Front matter (---\n...\n---) and %%{init}%% directives are skipped.
    size_t first = 0;
    if (!st.empty() && st[0].text == "---") {
        first = 1;
        while (first < st.size() && st[first].text != "---") first++;
        first++;
    }
    if (first >= st.size()) throw std::runtime_error("mermaid: empty diagram");
    const std::string head = st[first].text;
    const std::string kw = lower(head.substr(0, head.find(' ')));
    std::vector<Stmt> body(st.begin() + (long)first + 1, st.end());
    if (kw == "graph" || kw.rfind("flowchart", 0) == 0) {
        const size_t sp = head.find(' ');
        return mermaidFlowchart(body, directionOf(sp == std::string::npos ? "TB" : head.substr(sp + 1)));
    }
    if (kw.rfind("statediagram", 0) == 0) return mermaidState(body);
    fail("mermaid: line %d: unsupported diagram type \"%s\" (flowchart, graph and stateDiagram are)", st[first].line, head);
}

Graph dot(std::string_view text) { return DotParser(text).run(); }

Graph newick(std::string_view text) { return NewickParser(text).run(); }

Graph turtle(std::string_view text, const RdfOptions& options) {
    TurtleParser p(text);
    std::vector<Triple> t = p.run();
    return rdfGraph(t, p.prefixes_, options);
}

Graph jsonld(const Json& doc, const RdfOptions& options) { return JsonLd(options).run(doc); }

Graph json(const Json& doc) {
    Graph g;
    g.layout = LayoutKind::Force;
    const Json* nodes = &doc["nodes"];
    const Json* edges = doc.has("edges") ? &doc["edges"] : &doc["links"];
    Json cyNodes, cyEdges;
    if (doc["elements"].isArray() || doc["elements"].isObject()) {        // Cytoscape
        Json::Array ns, es;
        auto take = [&](const Json& el) { (el["data"].has("source") ? es : ns).push_back(el["data"]); };
        const Json& els = doc["elements"];
        if (els.isArray()) for (const Json& el : els.asArray()) take(el);
        else {
            if (els["nodes"].isArray()) for (const Json& el : els["nodes"].asArray()) take(el);
            if (els["edges"].isArray()) for (const Json& el : els["edges"].asArray()) take(el);
        }
        cyNodes = Json(std::move(ns));
        cyEdges = Json(std::move(es));
        nodes = &cyNodes;
        edges = &cyEdges;
    }
    if (doc.has("directed")) g.directed = doc.flag("directed", true);
    if (doc.has("direction")) g.direction = directionOf(doc.str("direction", "TB"));
    if (doc.has("layout")) {
        const std::string l = lower(doc.str("layout", "auto"));
        g.layout = l == "layered" ? LayoutKind::Layered : l == "tree" ? LayoutKind::Tree : l == "radial" ? LayoutKind::Radial :
                   l == "circle" ? LayoutKind::Circle : l == "none" ? LayoutKind::None : LayoutKind::Force;
    }
    bool allPos = nodes->isArray() && nodes->size() > 0;
    if (nodes->isArray())
        for (const Json& n : nodes->asArray()) {
            const std::string id = n["id"].isNumber() ? Json(n["id"].asNumber()).dump() : n.str("id", "");
            if (id.empty()) throw std::runtime_error("json graph: a node has no id");
            Node& nd = g.addNode(id, n.has("label") ? n.str("label", id) : n.str("name", id), shapeName(n.str("shape", "rounded")));
            nd.label = cleanLabel(nd.label);
            nd.group = n.str("parent", n.str("group", ""));
            nd.kind = n.str("kind", n.str("type", ""));
            if (n.has("color")) nd.style.fill = n.color("color", WHITE);
            if (n.has("x") && n.has("y")) nd.pos = { (float)n.num("x", 0), (float)n.num("y", 0) };
            else allPos = false;
            if (n.has("width")) nd.size.x = (float)n.num("width", 0);
            if (n.has("height")) nd.size.y = (float)n.num("height", 0);
            nd.data = n;
        }
    // A node's "group" that is itself a node (Cytoscape compound) or a name becomes a group.
    for (Node& n : g.nodes) {
        if (n.group.empty()) continue;
        if (!g.group(n.group)) {
            const Node* parentNode = g.node(n.group);
            g.addGroup(n.group, parentNode ? parentNode->label : n.group);
        }
    }
    g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(), [&](const Node& n) { return g.group(n.id) != nullptr; }), g.nodes.end());
    if (allPos) g.layout = LayoutKind::None;
    if (edges->isArray())
        for (const Json& e : edges->asArray()) {
            auto end = [&](const char* a, const char* b) {
                const Json& v = e.has(a) ? e[a] : e[b];
                return v.isNumber() ? Json(v.asNumber()).dump() : v.isString() ? v.asString() : std::string();
            };
            const std::string from = end("from", "source"), to = end("to", "target");
            if (from.empty() || to.empty()) throw std::runtime_error("json graph: an edge needs from/source and to/target");
            Edge& ed = g.addEdge(from, to, cleanLabel(e.str("label", "")));
            if (!g.directed) ed.head = Arrow::None;
            if (e.has("id")) ed.id = e.str("id", ed.id);
            if (e.has("color")) ed.style.stroke = e.color("color", GRAY);
            if (e.flag("dashed", false)) ed.stroke = Stroke::Dashed;
            ed.kind = e.str("kind", e.str("type", ""));
        }
    return g;
}

Graph parse(std::string_view text, std::string_view format) {
    const std::string f = lower(format);
    if (f == "mermaid" || f == "mmd") return mermaid(text);
    if (f == "dot" || f == "graphviz" || f == "gv") return dot(text);
    if (f == "bpmn") return bpmn(text);
    if (f == "turtle" || f == "ttl" || f == "ntriples" || f == "nt") return turtle(text);
    if (f == "jsonld" || f == "json-ld") return jsonld(Json::parse(text));
    if (f == "json") return json(Json::parse(text));
    if (f == "newick" || f == "nwk") return newick(text);
    throw std::runtime_error("dg: unknown format \"" + f + "\"");
}

Graph load(const std::string& file) {
    const std::string text = readFile(file);
    const size_t dot = file.find_last_of('.');
    const std::string ext = dot == std::string::npos ? "" : lower(file.substr(dot + 1));
    if (ext == "mmd" || ext == "mermaid") return mermaid(text);
    if (ext == "dot" || ext == "gv") return dg::dot(text);
    if (ext == "bpmn") return bpmn(text);
    if (ext == "ttl" || ext == "nt" || ext == "turtle") return turtle(text);
    if (ext == "jsonld") return jsonld(Json::parse(text));
    if (ext == "nwk" || ext == "newick" || ext == "tre" || ext == "tree") return newick(text);
    if (ext == "json") {
        const Json doc = Json::parse(text);
        if (doc.has("@context") || doc.has("@graph") || doc.has("@id") || doc.isArray()) return jsonld(doc);
        return json(doc);
    }
    // Sniff.
    const std::string head = lower(trim(std::string_view(text).substr(0, 400)));
    if (head.rfind("<", 0) == 0 && head.find("bpmn") != std::string::npos) return bpmn(text);
    if (head.rfind("{", 0) == 0 || head.rfind("[", 0) == 0) {
        const Json doc = Json::parse(text);
        if (doc.has("@context") || doc.has("@graph") || doc.isArray()) return jsonld(doc);
        return json(doc);
    }
    if (head.rfind("(", 0) == 0) return newick(text);
    if (head.rfind("digraph", 0) == 0 || head.rfind("strict", 0) == 0 ||
        (head.rfind("graph", 0) == 0 && head.find('{') != std::string::npos && head.find("-->") == std::string::npos))
        return dg::dot(text);
    if (head.rfind("graph", 0) == 0 || head.rfind("flowchart", 0) == 0 || head.rfind("statediagram", 0) == 0 || head.rfind("---", 0) == 0)
        return mermaid(text);
    if (head.find("@prefix") != std::string::npos || head.rfind("<http", 0) == 0 || head.find("prefix ") != std::string::npos) return turtle(text);
    throw std::runtime_error("dg: cannot tell the format of " + file);
}

} // namespace dg
} // namespace rmtn
