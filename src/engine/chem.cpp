// chem.cpp -- molecules: element data, SMILES and MOL / SDF parsing, ring perception,
// kekulisation, 2D depiction and rough 3D embedding, substructure search, and the
// animated 2D skeletal formula.
#include "rmtn/chem.hpp"
#include "rmtn/anim.hpp"
#include "rmtn/diagram.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/fonts.hpp"
#include "rmtn/media.hpp"
#include "rmtn/render.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace rmtn {
namespace chem {

// ------------------------------------------------------------------ element data
namespace {
struct Element { const char* sym; double mass; float cov, vdw; unsigned color; std::vector<int> valences; };
const std::vector<Element>& elements() {
    static const std::vector<Element> table = {
        { "H", 1.008, 0.31f, 1.20f, 0xFFFFFF, { 1 } },    { "He", 4.003, 0.28f, 1.40f, 0xD9FFFF, { 0 } },
        { "Li", 6.94, 1.28f, 1.82f, 0xCC80FF, { 1 } },    { "B", 10.81, 0.84f, 1.92f, 0xFFB5B5, { 3 } },
        { "C", 12.011, 0.76f, 1.70f, 0x909090, { 4 } },   { "N", 14.007, 0.71f, 1.55f, 0x3050F8, { 3, 5 } },
        { "O", 15.999, 0.66f, 1.52f, 0xFF0D0D, { 2 } },   { "F", 18.998, 0.57f, 1.47f, 0x90E050, { 1 } },
        { "Na", 22.990, 1.66f, 2.27f, 0xAB5CF2, { 1 } },  { "Mg", 24.305, 1.41f, 1.73f, 0x8AFF00, { 2 } },
        { "Al", 26.982, 1.21f, 1.84f, 0xBFA6A6, { 3 } },  { "Si", 28.085, 1.11f, 2.10f, 0xF0C8A0, { 4 } },
        { "P", 30.974, 1.07f, 1.80f, 0xFF8000, { 3, 5 } }, { "S", 32.06, 1.05f, 1.80f, 0xFFFF30, { 2, 4, 6 } },
        { "Cl", 35.45, 1.02f, 1.75f, 0x1FF01F, { 1 } },   { "K", 39.098, 2.03f, 2.75f, 0x8F40D4, { 1 } },
        { "Ca", 40.078, 1.76f, 2.31f, 0x3DFF00, { 2 } },  { "Mn", 54.938, 1.39f, 2.05f, 0x9C7AC7, {} },
        { "Fe", 55.845, 1.32f, 2.04f, 0xE06633, {} },     { "Co", 58.933, 1.26f, 2.00f, 0xF090A0, {} },
        { "Ni", 58.693, 1.24f, 1.97f, 0x50D050, {} },     { "Cu", 63.546, 1.32f, 1.96f, 0xC88033, {} },
        { "Zn", 65.38, 1.22f, 2.01f, 0x7D80B0, {} },      { "As", 74.922, 1.19f, 1.85f, 0xBD80E3, { 3, 5 } },
        { "Se", 78.971, 1.20f, 1.90f, 0xFFA100, { 2, 4, 6 } }, { "Br", 79.904, 1.20f, 1.85f, 0xA62929, { 1 } },
        { "Ag", 107.87, 1.45f, 2.11f, 0xC0C0C0, {} },     { "Sn", 118.71, 1.39f, 2.17f, 0x668080, {} },
        { "I", 126.904, 1.39f, 1.98f, 0x940094, { 1 } },  { "Pt", 195.08, 1.36f, 2.13f, 0xD0D0E0, {} },
        { "Au", 196.97, 1.36f, 2.14f, 0xFFD123, {} },     { "Hg", 200.59, 1.32f, 2.23f, 0xB8B8D0, {} },
    };
    return table;
}
const Element* elementOf(const std::string& s) {
    for (const Element& e : elements()) if (s == e.sym) return &e;
    return nullptr;
}
std::string capitalized(std::string s) {
    if (!s.empty()) s[0] = (char)std::toupper((unsigned char)s[0]);
    for (size_t i = 1; i < s.size(); i++) s[i] = (char)std::tolower((unsigned char)s[i]);
    return s;
}
}

Color cpkColor(const std::string& e) { const Element* el = elementOf(e); return mkColor(el ? el->color : 0xFF1493); }
float covalentRadius(const std::string& e) { const Element* el = elementOf(e); return el ? el->cov : 1.5f; }
float vdwRadius(const std::string& e) { const Element* el = elementOf(e); return el ? el->vdw : 2.0f; }

// ------------------------------------------------------------------ molecule queries
std::vector<int> Molecule::neighbors(int atom) const {
    std::vector<int> out;
    for (const Bond& b : bonds) {
        if (b.a == atom) out.push_back(b.b);
        else if (b.b == atom) out.push_back(b.a);
    }
    return out;
}

std::vector<std::vector<int>> Molecule::rings() const {
    const int n = (int)atoms.size(), m = (int)bonds.size();
    std::vector<std::vector<std::pair<int, int>>> adj(n);       // (neighbour, bond)
    for (int i = 0; i < m; i++) { adj[bonds[i].a].push_back({ bonds[i].b, i }); adj[bonds[i].b].push_back({ bonds[i].a, i }); }
    // Cycle rank = bonds - atoms + components.
    int comps = 0;
    {
        std::vector<int> seen(n, 0);
        for (int i = 0; i < n; i++) {
            if (seen[i]) continue;
            comps++;
            std::vector<int> st{ i };
            seen[i] = 1;
            while (!st.empty()) { const int v = st.back(); st.pop_back(); for (auto [w, _] : adj[v]) if (!seen[w]) { seen[w] = 1; st.push_back(w); } }
        }
    }
    const int rank = m - n + comps;
    if (rank <= 0) return {};
    // For every bond, the shortest cycle through it (BFS around it), then a minimal
    // independent set (GF(2) elimination over bond-incidence vectors), smallest first.
    struct Cycle { std::vector<int> atoms; std::vector<char> bondSet; };
    std::vector<Cycle> cands;
    std::set<std::vector<int>> seenCycles;
    for (int e = 0; e < m; e++) {
        const int s = bonds[e].a, t = bonds[e].b;
        std::vector<int> prev(n, -1), prevBond(n, -1);
        std::queue<int> q;
        q.push(s); prev[s] = s;
        while (!q.empty() && prev[t] < 0) {
            const int v = q.front(); q.pop();
            for (auto [w, bi] : adj[v]) {
                if (bi == e || prev[w] >= 0) continue;
                prev[w] = v; prevBond[w] = bi; q.push(w);
            }
        }
        if (prev[t] < 0) continue;
        Cycle c;
        c.bondSet.assign(m, 0);
        c.bondSet[e] = 1;
        for (int v = t; v != s; v = prev[v]) { c.atoms.push_back(v); c.bondSet[prevBond[v]] = 1; }
        c.atoms.push_back(s);
        std::vector<int> key = c.atoms;
        std::sort(key.begin(), key.end());
        if (seenCycles.insert(key).second) cands.push_back(std::move(c));
    }
    std::stable_sort(cands.begin(), cands.end(), [](const Cycle& a, const Cycle& b) { return a.atoms.size() < b.atoms.size(); });
    std::vector<std::vector<char>> basis;            // reduced rows
    std::vector<int> pivots;
    std::vector<std::vector<int>> out;
    for (const Cycle& c : cands) {
        std::vector<char> v = c.bondSet;
        for (size_t r = 0; r < basis.size(); r++)
            if (v[pivots[r]]) for (int k = 0; k < m; k++) v[k] ^= basis[r][k];
        int piv = -1;
        for (int k = 0; k < m; k++) if (v[k]) { piv = k; break; }
        if (piv < 0) continue;
        basis.push_back(v);
        pivots.push_back(piv);
        out.push_back(c.atoms);
        if ((int)out.size() == rank) break;
    }
    return out;
}

double Molecule::mass() const {
    double s = 0;
    for (const Atom& a : atoms) {
        const Element* e = elementOf(a.element);
        s += (e ? e->mass : 0) + a.hydrogens * 1.008;
    }
    return s;
}

std::string Molecule::formula() const {
    std::map<std::string, int> count;
    for (const Atom& a : atoms) {
        if (a.element != "*") count[a.element]++;
        if (a.hydrogens) count["H"] += a.hydrogens;
    }
    std::string out;
    auto emit = [&](const std::string& e) {
        auto it = count.find(e);
        if (it == count.end() || it->second == 0) return;
        out += e;
        if (it->second > 1) out += std::to_string(it->second);
        count.erase(it);
    };
    if (count.count("C")) { emit("C"); emit("H"); }
    for (auto& [e, n] : std::map<std::string, int>(count)) emit(e);
    return out;
}

float Molecule::radius() const {
    if (atoms.empty()) return 0;
    Vector3 c{ 0, 0, 0 };
    for (const Atom& a : atoms) { c.x += a.pos.x; c.y += a.pos.y; c.z += a.pos.z; }
    c = { c.x / atoms.size(), c.y / atoms.size(), c.z / atoms.size() };
    float r = 0;
    for (const Atom& a : atoms) r = std::max(r, std::sqrt((a.pos.x - c.x) * (a.pos.x - c.x) + (a.pos.y - c.y) * (a.pos.y - c.y) + (a.pos.z - c.z) * (a.pos.z - c.z)));
    return r;
}

// ------------------------------------------------------------------ SMILES
namespace {
[[noreturn]] void smilesError(std::string_view s, size_t at, const char* what) {
    throw std::runtime_error("smiles: " + std::string(what) + " at " + std::to_string(at) + " in \"" + std::string(s) + "\"");
}

// Sum of bond orders at atom i (aromatic bonds count 1).
int bondSum(const Molecule& m, int i) {
    int s = 0;
    for (const Bond& b : m.bonds) if (b.a == i || b.b == i) s += b.order;
    return s;
}

int defaultValence(const Atom& a) {
    const Element* e = elementOf(a.element);
    int v = e && !e->valences.empty() ? e->valences[0] : 0;
    const std::string& s = a.element;
    if ((s == "N" || s == "P" || s == "As") && a.charge == 1) v = 4;
    else if ((s == "O" || s == "S" || s == "Se") && a.charge == 1) v = 3;
    else if ((s == "O" || s == "S") && a.charge == -1) v = 1;
    else if ((s == "N") && a.charge == -1) v = 2;
    else if (s == "C" && a.charge != 0) v = 3;
    else if (s == "B" && a.charge == -1) v = 4;
    return v;
}

// Turns aromatic bonds into alternating single and double bonds (a perfect matching of
// the atoms that still need a double bond). Returns false when none exists.
bool kekulize(Molecule& m, const std::vector<char>& implicitH) {
    const int n = (int)m.atoms.size();
    std::vector<char> need(n, 0);
    bool any = false;
    for (int i = 0; i < n; i++) {
        const Atom& a = m.atoms[i];
        if (!a.aromatic) continue;
        any = true;
        const int sum = bondSum(m, i) + (implicitH[i] ? 0 : a.hydrogens);
        need[i] = defaultValence(a) - sum >= 1;
    }
    if (!any) return true;
    std::vector<std::vector<int>> cand(n);           // aromatic bonds to other needy atoms
    for (int k = 0; k < (int)m.bonds.size(); k++) {
        const Bond& b = m.bonds[k];
        if (b.aromatic && need[b.a] && need[b.b]) { cand[b.a].push_back(k); cand[b.b].push_back(k); }
    }
    std::vector<int> mate(n, -1);
    long budget = 200000;
    std::function<bool()> solve = [&]() -> bool {
        if (--budget < 0) return false;
        int pick = -1;
        size_t best = 1000;
        for (int i = 0; i < n; i++) {
            if (!need[i] || mate[i] >= 0) continue;
            size_t opts = 0;
            for (int k : cand[i]) { const int o = m.bonds[k].a == i ? m.bonds[k].b : m.bonds[k].a; if (mate[o] < 0) opts++; }
            if (opts < best) { best = opts; pick = i; }
        }
        if (pick < 0) return true;
        if (best == 0) return false;
        for (int k : cand[pick]) {
            const int o = m.bonds[k].a == pick ? m.bonds[k].b : m.bonds[k].a;
            if (mate[o] >= 0) continue;
            mate[pick] = k; mate[o] = k;
            if (solve()) return true;
            mate[pick] = -1; mate[o] = -1;
        }
        return false;
    };
    if (!solve()) return false;
    for (int k = 0; k < (int)m.bonds.size(); k++) {
        Bond& b = m.bonds[k];
        if (!b.aromatic) continue;
        b.order = (mate[b.a] == k) ? 2 : 1;
    }
    return true;
}

void implicitHydrogens(Molecule& m, const std::vector<char>& implicitH, bool kekulized) {
    for (int i = 0; i < (int)m.atoms.size(); i++) {
        if (!implicitH[i]) continue;
        Atom& a = m.atoms[i];
        const Element* e = elementOf(a.element);
        if (!e || e->valences.empty()) continue;
        int sum = bondSum(m, i);
        if (a.aromatic && !kekulized) sum += 1;
        if (a.charge != 0) { a.hydrogens = std::max(0, defaultValence(a) - sum); continue; }
        int h = 0;
        for (int v : e->valences) if (v >= sum) { h = v - sum; break; }
        a.hydrogens = h;
    }
}

// The bond graph of a SMILES string (aromatic flags kept, no kekulisation yet).
Molecule smilesGraph(std::string_view s, std::vector<char>& implicitH) {
    Molecule m;
    int prev = -1;
    std::vector<int> branches;
    int pendingOrder = 0;
    bool pendingAromatic = false, pendingExplicit = false;
    struct Open { int atom; int order; bool explicitBond; bool aromatic; };
    std::map<int, Open> rings;
    auto bondTo = [&](int a, int b, int order, bool explicitBond, bool aromaticBond) {
        Bond bd;
        bd.a = a; bd.b = b;
        if (explicitBond) { bd.order = std::max(1, order); bd.aromatic = aromaticBond; }
        else if (m.atoms[a].aromatic && m.atoms[b].aromatic) { bd.order = 1; bd.aromatic = true; }
        else bd.order = 1;
        m.bonds.push_back(bd);
    };
    auto addAtom = [&](Atom a, bool organic) {
        m.atoms.push_back(std::move(a));
        implicitH.push_back(organic ? 1 : 0);
        const int idx = (int)m.atoms.size() - 1;
        if (prev >= 0) bondTo(prev, idx, pendingOrder, pendingExplicit, pendingAromatic);
        pendingOrder = 0; pendingExplicit = false; pendingAromatic = false;
        prev = idx;
    };
    for (size_t i = 0; i < s.size(); i++) {
        const char c = s[i];
        if (std::isspace((unsigned char)c)) break;               // "SMILES name"
        if (c == '(') { if (prev < 0) smilesError(s, i, "branch before any atom"); branches.push_back(prev); continue; }
        if (c == ')') { if (branches.empty()) smilesError(s, i, "unbalanced )"); prev = branches.back(); branches.pop_back(); continue; }
        if (c == '-' || c == '/' || c == '\\') { pendingOrder = 1; pendingExplicit = true; continue; }
        if (c == '=') { pendingOrder = 2; pendingExplicit = true; continue; }
        if (c == '#') { pendingOrder = 3; pendingExplicit = true; continue; }
        if (c == '$') { pendingOrder = 4; pendingExplicit = true; continue; }
        if (c == ':') { pendingOrder = 1; pendingExplicit = true; pendingAromatic = true; continue; }
        if (c == '.') { prev = -1; continue; }
        if (std::isdigit((unsigned char)c) || c == '%') {
            int num = c - '0';
            if (c == '%') {
                if (i + 2 >= s.size() || !std::isdigit((unsigned char)s[i + 1]) || !std::isdigit((unsigned char)s[i + 2])) smilesError(s, i, "bad %nn ring number");
                num = (s[i + 1] - '0') * 10 + (s[i + 2] - '0');
                i += 2;
            }
            if (prev < 0) smilesError(s, i, "ring closure before any atom");
            auto it = rings.find(num);
            if (it == rings.end()) {
                rings[num] = { prev, pendingOrder, pendingExplicit, pendingAromatic };
            } else {
                const Open o = it->second;
                rings.erase(it);
                const bool ex = pendingExplicit || o.explicitBond;
                bondTo(o.atom, prev, pendingExplicit ? pendingOrder : o.order, ex, pendingExplicit ? pendingAromatic : o.aromatic);
            }
            pendingOrder = 0; pendingExplicit = false; pendingAromatic = false;
            continue;
        }
        if (c == '[') {
            const size_t close = s.find(']', i);
            if (close == std::string_view::npos) smilesError(s, i, "unterminated [");
            const std::string_view in = s.substr(i + 1, close - i - 1);
            size_t k = 0;
            Atom a;
            while (k < in.size() && std::isdigit((unsigned char)in[k])) a.isotope = a.isotope * 10 + (in[k++] - '0');
            if (k >= in.size()) smilesError(s, i, "empty bracket atom");
            std::string sym(1, in[k++]);
            if (sym == "*") { a.element = "*"; }
            else {
                if (k < in.size() && std::islower((unsigned char)in[k])) {
                    // "Cl", "Fe" (a known two-letter element); aromatic "se", "as".
                    const std::string two = sym + in[k];
                    const bool upper = std::isupper((unsigned char)sym[0]) != 0;
                    if ((upper && elementOf(two)) || (!upper && (two == "se" || two == "as"))) { sym = two; k++; }
                }
                a.aromatic = std::islower((unsigned char)sym[0]) != 0;
                a.element = capitalized(sym);
            }
            while (k < in.size() && in[k] == '@') k++;
            if (k < in.size() && (in.compare(k, 2, "TH") == 0 || in.compare(k, 2, "AL") == 0 || in.compare(k, 2, "SP") == 0 ||
                                  in.compare(k, 2, "TB") == 0 || in.compare(k, 2, "OH") == 0)) {
                k += 2;
                while (k < in.size() && std::isdigit((unsigned char)in[k])) k++;
            }
            if (k < in.size() && in[k] == 'H') {
                k++;
                int h = 1;
                if (k < in.size() && std::isdigit((unsigned char)in[k])) { h = 0; while (k < in.size() && std::isdigit((unsigned char)in[k])) h = h * 10 + (in[k++] - '0'); }
                a.hydrogens = h;
            }
            while (k < in.size() && (in[k] == '+' || in[k] == '-')) {
                const int sign = in[k] == '+' ? 1 : -1;
                k++;
                if (k < in.size() && std::isdigit((unsigned char)in[k])) {
                    int v = 0;
                    while (k < in.size() && std::isdigit((unsigned char)in[k])) v = v * 10 + (in[k++] - '0');
                    a.charge += sign * v;
                } else a.charge += sign;
            }
            addAtom(a, false);
            i = close;
            continue;
        }
        // Organic subset.
        Atom a;
        if (c == 'C' && i + 1 < s.size() && s[i + 1] == 'l') { a.element = "Cl"; i++; }
        else if (c == 'B' && i + 1 < s.size() && s[i + 1] == 'r') { a.element = "Br"; i++; }
        else if (std::strchr("BCNOPSFI", c)) a.element = std::string(1, c);
        else if (std::strchr("bcnops", c)) { a.element = std::string(1, (char)std::toupper((unsigned char)c)); a.aromatic = true; }
        else if (c == '*') a.element = "*";
        else smilesError(s, i, "unexpected character");
        addAtom(a, a.element != "*");
    }
    if (!rings.empty()) smilesError(s, s.size(), "unclosed ring");
    if (!branches.empty()) smilesError(s, s.size(), "unclosed branch");
    return m;
}
} // namespace

Molecule smiles(std::string_view text) {
    std::vector<char> implicitH;
    const std::string trimmed = [&] { size_t a = 0; while (a < text.size() && std::isspace((unsigned char)text[a])) a++; return std::string(text.substr(a)); }();
    Molecule m = smilesGraph(trimmed, implicitH);
    const size_t sp = trimmed.find_first_of(" \t");
    if (sp != std::string::npos) {
        size_t a = sp;
        while (a < trimmed.size() && std::isspace((unsigned char)trimmed[a])) a++;
        m.name = trimmed.substr(a);
        while (!m.name.empty() && std::isspace((unsigned char)m.name.back())) m.name.pop_back();
    }
    const bool ok = kekulize(m, implicitH);
    implicitHydrogens(m, implicitH, ok);
    depict(m);
    return m;
}

// ------------------------------------------------------------------ MOL / SDF
namespace {
std::vector<std::string> lines(std::string_view text) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : text) {
        if (c == '\n') { out.push_back(cur); cur.clear(); }
        else if (c != '\r') cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}
std::string field(const std::string& l, size_t at, size_t len) { return at < l.size() ? l.substr(at, len) : std::string(); }
int toInt(const std::string& s) { return std::atoi(s.c_str()); }
float toFloat(const std::string& s) { return (float)std::atof(s.c_str()); }
}

Molecule molfile(std::string_view text, int record) {
    std::vector<std::string> L = lines(text);
    size_t start = 0;
    for (int r = 0; r < record; r++) {               // SDF records end with $$$$
        while (start < L.size() && L[start].rfind("$$$$", 0) != 0) start++;
        start++;
        if (start >= L.size()) throw std::runtime_error("molfile: no record " + std::to_string(record));
    }
    if (start + 3 >= L.size()) throw std::runtime_error("molfile: too short");
    Molecule m;
    m.name = L[start];
    while (!m.name.empty() && std::isspace((unsigned char)m.name.back())) m.name.pop_back();
    const std::string& counts = L[start + 3];
    bool any3d = false;
    if (counts.find("V3000") != std::string::npos) {
        // V3000: "M  V30 BEGIN ATOM" / "M  V30 i El x y z ..." / BEGIN BOND "M  V30 i order a b ..."
        enum { None, Atoms, Bonds } sec = None;
        std::map<int, int> idx;
        for (size_t i = start + 4; i < L.size() && L[i].rfind("M  END", 0) != 0; i++) {
            std::string l = L[i];
            if (l.rfind("M  V30 ", 0) != 0) continue;
            l = l.substr(7);
            while (!l.empty() && l.back() == '-' && i + 1 < L.size()) { l.pop_back(); ++i; l += L[i].substr(std::min<size_t>(7, L[i].size())); }
            if (l.rfind("BEGIN ATOM", 0) == 0) { sec = Atoms; continue; }
            if (l.rfind("BEGIN BOND", 0) == 0) { sec = Bonds; continue; }
            if (l.rfind("END", 0) == 0) { sec = None; continue; }
            std::istringstream in(l);
            if (sec == Atoms) {
                int id; std::string el; float x, y, z;
                in >> id >> el >> x >> y >> z;
                Atom a;
                a.element = el;
                a.pos = { x, -y, z };
                if (z != 0) any3d = true;
                std::string kv;
                int aamap;
                in >> aamap;
                while (in >> kv) {
                    if (kv.rfind("CHG=", 0) == 0) a.charge = std::atoi(kv.c_str() + 4);
                    if (kv.rfind("MASS=", 0) == 0) a.isotope = std::atoi(kv.c_str() + 5);
                }
                idx[id] = (int)m.atoms.size();
                m.atoms.push_back(a);
            } else if (sec == Bonds) {
                int id, order, a, b;
                in >> id >> order >> a >> b;
                Bond bd;
                bd.a = idx[a]; bd.b = idx[b];
                bd.order = order == 4 ? 1 : std::clamp(order, 1, 3);
                bd.aromatic = order == 4;
                std::string kv;
                while (in >> kv) if (kv.rfind("CFG=", 0) == 0) { const int cfg = std::atoi(kv.c_str() + 4); bd.stereo = cfg == 1 ? Bond::Stereo::Wedge : cfg == 3 ? Bond::Stereo::Hash : Bond::Stereo::None; }
                m.bonds.push_back(bd);
            }
        }
    } else {
        const int na = toInt(field(counts, 0, 3)), nb = toInt(field(counts, 3, 3));
        if (start + 4 + na + nb > L.size()) throw std::runtime_error("molfile: atom / bond block truncated");
        for (int i = 0; i < na; i++) {
            const std::string& l = L[start + 4 + i];
            Atom a;
            a.pos = { toFloat(field(l, 0, 10)), -toFloat(field(l, 10, 10)), toFloat(field(l, 20, 10)) };
            if (a.pos.z != 0) any3d = true;
            std::string el = field(l, 31, 3);
            el.erase(std::remove(el.begin(), el.end(), ' '), el.end());
            a.element = el;
            const int ch = toInt(field(l, 36, 3));
            if (ch >= 1 && ch <= 7 && ch != 4) a.charge = 4 - ch;
            m.atoms.push_back(a);
        }
        for (int i = 0; i < nb; i++) {
            const std::string& l = L[start + 4 + na + i];
            Bond b;
            b.a = toInt(field(l, 0, 3)) - 1;
            b.b = toInt(field(l, 3, 3)) - 1;
            const int t = toInt(field(l, 6, 3)), st = toInt(field(l, 9, 3));
            b.order = t == 4 ? 1 : std::clamp(t, 1, 3);
            b.aromatic = t == 4;
            b.stereo = st == 1 ? Bond::Stereo::Wedge : st == 6 ? Bond::Stereo::Hash : Bond::Stereo::None;
            if (b.a < 0 || b.b < 0 || b.a >= na || b.b >= na) throw std::runtime_error("molfile: bond to a missing atom");
            m.bonds.push_back(b);
        }
        bool chgSeen = false;
        for (size_t i = start + 4 + na + nb; i < L.size() && L[i].rfind("M  END", 0) != 0; i++) {
            const std::string& l = L[i];
            if (l.rfind("M  CHG", 0) == 0 || l.rfind("M  ISO", 0) == 0) {
                const bool chg = l[3] == 'C';
                if (chg && !chgSeen) { for (Atom& a : m.atoms) a.charge = 0; chgSeen = true; }
                std::istringstream in(l.substr(6));
                int n;
                in >> n;
                for (int k = 0; k < n; k++) {
                    int at, v;
                    in >> at >> v;
                    if (at >= 1 && at <= na) { if (chg) m.atoms[at - 1].charge = v; else m.atoms[at - 1].isotope = v; }
                }
            }
        }
    }
    m.coords3d = any3d;
    // Hydrogens the file leaves implicit, for formulas and labels.
    std::vector<char> implicitH(m.atoms.size(), 1);
    const bool ok = kekulize(m, std::vector<char>(m.atoms.size(), 0));
    implicitHydrogens(m, implicitH, ok);
    return m;
}

Molecule loadMolecule(const std::string& file, int record) {
    const std::string path = media::resolve(file);
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("chem: cannot read " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    std::string ext = path.substr(path.find_last_of('.') + 1);
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    if (ext == "smi" || ext == "smiles") {
        std::istringstream ls(text);
        std::string line;
        for (int r = 0; std::getline(ls, line); ) {
            if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
            if (r++ == record) return smiles(line);
        }
        throw std::runtime_error("chem: no SMILES record " + std::to_string(record) + " in " + path);
    }
    if (ext == "pdb" || ext == "ent") return pdb(text).molecule(true, true);
    return molfile(text, record);
}

// ------------------------------------------------------------------ 2D depiction
namespace {
// Zig-zag chain distance for k bonds (120 degree angles, bond 1).
double zigzag(int k) {
    if (k <= 0) return 0;
    const double dx = 0.8660254 * k, dy = (k % 2) ? 0.5 : 0.0;
    return std::sqrt(dx * dx + dy * dy);
}
std::vector<std::vector<int>> hopDistances(const Molecule& m) {
    const int n = (int)m.atoms.size();
    std::vector<std::vector<int>> adj(n);
    for (const Bond& b : m.bonds) { adj[b.a].push_back(b.b); adj[b.b].push_back(b.a); }
    std::vector<std::vector<int>> d(n, std::vector<int>(n, -1));
    for (int s = 0; s < n; s++) {
        std::queue<int> q;
        d[s][s] = 0; q.push(s);
        while (!q.empty()) {
            const int v = q.front(); q.pop();
            for (int w : adj[v]) if (d[s][w] < 0) { d[s][w] = d[s][v] + 1; q.push(w); }
        }
    }
    return d;
}

// Regular-polygon templates for each ring system (fused rings share an edge, spiro rings
// an atom); returns per-atom template positions and system ids (-1 = not placed).
void ringTemplates(const Molecule& m, std::vector<Vector2>& pos, std::vector<int>& system) {
    const int n = (int)m.atoms.size();
    pos.assign(n, { 0, 0 });
    system.assign(n, -1);
    std::vector<std::vector<int>> rings = m.rings();
    std::vector<char> done(rings.size(), 0);
    int sys = 0;
    for (size_t r0 = 0; r0 < rings.size(); r0++) {
        if (done[r0]) continue;
        // Start each system with its largest ring that has the most fused neighbours.
        auto place = [&](const std::vector<int>& ring, Vector2 center, float startAngle, float dirSign) {
            const int k = (int)ring.size();
            const float R = 0.5f / std::sin((float)PI / k);
            for (int i = 0; i < k; i++) {
                const float a = startAngle + dirSign * 2 * (float)PI * i / k;
                pos[ring[i]] = { center.x + R * std::cos(a), center.y + R * std::sin(a) };
                system[ring[i]] = sys;
            }
        };
        place(rings[r0], { 0, 0 }, (float)PI / 2 + (float)PI / rings[r0].size(), 1);
        done[r0] = 1;
        std::vector<int> placedRings{ (int)r0 };
        for (bool progress = true; progress;) {
            progress = false;
            for (size_t r = 0; r < rings.size(); r++) {
                if (done[r]) continue;
                const std::vector<int>& ring = rings[r];
                const int k = (int)ring.size();
                std::vector<int> shared;
                for (int i = 0; i < k; i++) if (system[ring[i]] == sys) shared.push_back(i);
                if (shared.empty()) continue;
                const float R = 0.5f / std::sin((float)PI / k), apo = 0.5f / std::tan((float)PI / k);
                if (shared.size() == 2 && ((shared[1] - shared[0]) == 1 || (shared[0] == 0 && shared[1] == k - 1))) {
                    // Fused on an edge: the new ring's centre is across the edge from the placed side.
                    int ia = shared[0], ib = shared[1];
                    if (ia == 0 && ib == k - 1) std::swap(ia, ib);
                    const Vector2 A = pos[ring[ia]], B = pos[ring[ib]];
                    const Vector2 mid{ (A.x + B.x) / 2, (A.y + B.y) / 2 };
                    Vector2 nrm{ -(B.y - A.y), B.x - A.x };
                    const float l = std::max(1e-6f, std::hypot(nrm.x, nrm.y));
                    nrm = { nrm.x / l, nrm.y / l };
                    // Pick the side away from the atoms already placed.
                    Vector2 cen{ 0, 0 };
                    int cnt = 0;
                    for (int i = 0; i < n; i++) if (system[i] == sys) { cen.x += pos[i].x; cen.y += pos[i].y; cnt++; }
                    cen = { cen.x / cnt, cen.y / cnt };
                    if ((cen.x - mid.x) * nrm.x + (cen.y - mid.y) * nrm.y > 0) nrm = { -nrm.x, -nrm.y };
                    const Vector2 C{ mid.x + nrm.x * apo, mid.y + nrm.y * apo };
                    const float aA = std::atan2(A.y - C.y, A.x - C.x), aB = std::atan2(B.y - C.y, B.x - C.x);
                    float step = aB - aA;
                    while (step > PI) step -= 2 * (float)PI;
                    while (step < -PI) step += 2 * (float)PI;
                    const float sgn = step > 0 ? 1.f : -1.f;
                    for (int i = 0; i < k; i++) {
                        const int at = ring[(ia + i) % k];
                        if (system[at] == sys) continue;
                        const float a = aA + sgn * 2 * (float)PI * i / k;
                        pos[at] = { C.x + R * std::cos(a), C.y + R * std::sin(a) };
                        system[at] = sys;
                    }
                } else if (shared.size() == 1) {
                    // Spiro: outwards from the system through the shared atom.
                    const int s = ring[shared[0]];
                    Vector2 cen{ 0, 0 };
                    int cnt = 0;
                    for (int i = 0; i < n; i++) if (system[i] == sys && i != s) { cen.x += pos[i].x; cen.y += pos[i].y; cnt++; }
                    cen = { cen.x / std::max(1, cnt), cen.y / std::max(1, cnt) };
                    Vector2 d{ pos[s].x - cen.x, pos[s].y - cen.y };
                    const float l = std::max(1e-6f, std::hypot(d.x, d.y));
                    d = { d.x / l, d.y / l };
                    const Vector2 C{ pos[s].x + d.x * R, pos[s].y + d.y * R };
                    const float a0 = std::atan2(pos[s].y - C.y, pos[s].x - C.x);
                    for (int i = 1; i < k; i++) {
                        const int at = ring[(shared[0] + i) % k];
                        const float a = a0 + 2 * (float)PI * i / k;
                        pos[at] = { C.x + R * std::cos(a), C.y + R * std::sin(a) };
                        system[at] = sys;
                    }
                } else continue;                         // bridged: left to the stress layout
                done[r] = 1;
                progress = true;
            }
        }
        sys++;
    }
}
}

void depict(Molecule& m) {
    const int n = (int)m.atoms.size();
    if (n == 0) return;
    m.coords3d = false;
    if (n == 1) { m.atoms[0].pos = { 0, 0, 0 }; return; }
    const auto hop = hopDistances(m);
    std::vector<Vector2> tpl;
    std::vector<int> sys;
    ringTemplates(m, tpl, sys);
    std::vector<double> d((size_t)n * n, 0.0), w((size_t)n * n, 1.0);
    // Disconnected parts: laid out separately, then placed side by side.
    std::vector<int> comp(n, -1);
    int comps = 0;
    for (int i = 0; i < n; i++) {
        if (comp[i] >= 0) continue;
        for (int j = 0; j < n; j++) if (hop[i][j] >= 0) comp[j] = comps;
        comps++;
    }
    float offset = 0;
    for (int ci = 0; ci < comps; ci++) {
        std::vector<int> ids;
        for (int i = 0; i < n; i++) if (comp[i] == ci) ids.push_back(i);
        const int k = (int)ids.size();
        std::vector<double> dd((size_t)k * k, 0.0), ww((size_t)k * k, 1.0);
        for (int a = 0; a < k; a++)
            for (int b = 0; b < k; b++) {
                if (a == b) continue;
                const int i = ids[a], j = ids[b];
                if (sys[i] >= 0 && sys[i] == sys[j]) {
                    dd[(size_t)a * k + b] = std::max(0.3f, std::hypot(tpl[i].x - tpl[j].x, tpl[i].y - tpl[j].y));
                    ww[(size_t)a * k + b] = 12;
                } else {
                    dd[(size_t)a * k + b] = zigzag(hop[i][j]);
                    ww[(size_t)a * k + b] = hop[i][j] <= 2 ? 3 : 1;
                }
            }
        std::vector<double> x;
        dg::detail::stress(x, k, 2, dd, 400, false, &ww);
        // Principal axis horizontal, centred.
        double mx = 0, my = 0;
        for (int a = 0; a < k; a++) { mx += x[a * 2]; my += x[a * 2 + 1]; }
        mx /= k; my /= k;
        double sxx = 0, syy = 0, sxy = 0;
        for (int a = 0; a < k; a++) {
            const double ax = x[a * 2] - mx, ay = x[a * 2 + 1] - my;
            sxx += ax * ax; syy += ay * ay; sxy += ax * ay;
        }
        const double th = 0.5 * std::atan2(2 * sxy, sxx - syy), cs = std::cos(-th), sn = std::sin(-th);
        float minX = 1e9f, maxX = -1e9f;
        for (int a = 0; a < k; a++) {
            const double ax = x[a * 2] - mx, ay = x[a * 2 + 1] - my;
            const float px = (float)(ax * cs - ay * sn), py = (float)(ax * sn + ay * cs);
            m.atoms[ids[a]].pos = { px, py, 0 };
            minX = std::min(minX, px); maxX = std::max(maxX, px);
        }
        for (int a = 0; a < k; a++) m.atoms[ids[a]].pos.x += offset - minX;
        offset += (maxX - minX) + 1.6f;
    }
    // Centre everything.
    float cx = 0, cy = 0;
    for (const Atom& a : m.atoms) { cx += a.pos.x; cy += a.pos.y; }
    cx /= n; cy /= n;
    for (Atom& a : m.atoms) { a.pos.x -= cx; a.pos.y -= cy; }
}

Molecule withHydrogens(const Molecule& in) {
    Molecule m = in;
    for (int i = 0, n = (int)in.atoms.size(); i < n; i++) {
        for (int h = 0; h < in.atoms[i].hydrogens; h++) {
            Atom H;
            H.element = "H";
            const float a = (float)(h * 2.1 + i);
            H.pos = { in.atoms[i].pos.x + 0.6f * std::cos(a), in.atoms[i].pos.y + 0.6f * std::sin(a), in.atoms[i].pos.z };
            m.atoms.push_back(H);
            Bond b;
            b.a = i; b.b = (int)m.atoms.size() - 1;
            m.bonds.push_back(b);
        }
        m.atoms[i].hydrogens = 0;
    }
    return m;
}

void embed3d(Molecule& m) {
    const int n = (int)m.atoms.size();
    if (n == 0) return;
    if (n == 1) { m.atoms[0].pos = { 0, 0, 0 }; m.coords3d = true; return; }
    const auto hop = hopDistances(m);
    std::vector<std::vector<std::pair<int, const Bond*>>> adj(n);
    for (const Bond& b : m.bonds) { adj[b.a].push_back({ b.b, &b }); adj[b.b].push_back({ b.a, &b }); }
    auto bondLen = [&](const Bond& b) {
        const float base = covalentRadius(m.atoms[b.a].element) + covalentRadius(m.atoms[b.b].element);
        return base * (b.aromatic ? 0.93f : b.order == 2 ? 0.87f : b.order == 3 ? 0.78f : 1.f);
    };
    auto angleAt = [&](int c) {
        int doubles = 0;
        bool triple = false, aromatic = false;
        for (auto [o, b] : adj[c]) { if (b->order == 2) doubles++; if (b->order == 3) triple = true; if (b->aromatic) aromatic = true; }
        const int steric = (int)adj[c].size() + m.atoms[c].hydrogens;
        if (triple || doubles >= 2) return 180.f;
        if (doubles == 1 || aromatic || steric == 3) return 120.f;
        return 109.47f;
    };
    std::vector<double> d((size_t)n * n, -1.0), w((size_t)n * n, 0.0);
    auto set = [&](int i, int j, double v, double wt) {
        if (d[(size_t)i * n + j] >= 0 && w[(size_t)i * n + j] >= wt) return;
        d[(size_t)i * n + j] = d[(size_t)j * n + i] = v;
        w[(size_t)i * n + j] = w[(size_t)j * n + i] = wt;
    };
    for (const Bond& b : m.bonds) set(b.a, b.b, bondLen(b), 8);
    for (int c = 0; c < n; c++) {
        const float ang = angleAt(c) * DEG2RAD;
        for (size_t x = 0; x < adj[c].size(); x++)
            for (size_t y = x + 1; y < adj[c].size(); y++) {
                const double a = bondLen(*adj[c][x].second), b = bondLen(*adj[c][y].second);
                set(adj[c][x].first, adj[c][y].first, std::sqrt(a * a + b * b - 2 * a * b * std::cos(ang)), 4);
            }
    }
    // Flat rings: regular polygons.
    for (const auto& ring : m.rings()) {
        const int k = (int)ring.size();
        double avg = 0;
        for (int i = 0; i < k; i++)
            for (auto [o, b] : adj[ring[i]]) if (o == ring[(i + 1) % k]) avg += bondLen(*b);
        avg /= k;
        const double R = avg / (2 * std::sin(PI / k));
        for (int i = 0; i < k; i++)
            for (int j = i + 1; j < k; j++) set(ring[i], ring[j], 2 * R * std::sin(PI * (j - i) / k), k <= 6 ? 5 : 2);
    }
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            if (hop[i][j] < 0) { set(i, j, 12, 0.01); continue; }
            if (hop[i][j] >= 3 && d[(size_t)i * n + j] < 0) set(i, j, 1.25 * hop[i][j] * 0.86 + 0.3, 0.2);
        }
    for (size_t k = 0; k < d.size(); k++) if (d[k] < 0) { d[k] = 0; w[k] = 0; }
    // Start from a 2D depiction lifted off the plane, so rings and chains begin untangled.
    Molecule flat = m;
    depict(flat);
    std::vector<double> x((size_t)n * 3);
    for (int i = 0; i < n; i++) {
        x[i * 3] = flat.atoms[i].pos.x * 1.4;
        x[i * 3 + 1] = flat.atoms[i].pos.y * 1.4;
        x[i * 3 + 2] = 0.35 * std::sin(i * 2.17 + 0.4);
    }
    // stress() skips d <= 0 pairs; weights scale the d^-2 terms.
    for (size_t k = 0; k < d.size(); k++) if (w[k] <= 0) d[k] = -1;
    dg::detail::stress(x, n, 3, d, 500, true, &w);
    double cx = 0, cy = 0, cz = 0;
    for (int i = 0; i < n; i++) { cx += x[i * 3]; cy += x[i * 3 + 1]; cz += x[i * 3 + 2]; }
    cx /= n; cy /= n; cz /= n;
    for (int i = 0; i < n; i++) m.atoms[i].pos = { (float)(x[i * 3] - cx), (float)(x[i * 3 + 1] - cy), (float)(x[i * 3 + 2] - cz) };
    m.coords3d = true;
}

// ------------------------------------------------------------------ substructure search
std::vector<std::vector<int>> find(const Molecule& m, std::string_view pattern) {
    std::vector<char> implicitH;
    const Molecule p = smilesGraph(pattern, implicitH);
    const int pn = (int)p.atoms.size(), mn = (int)m.atoms.size();
    std::vector<std::vector<std::pair<int, int>>> padj(pn), madj(mn);    // (neighbour, bond)
    for (int k = 0; k < (int)p.bonds.size(); k++) { padj[p.bonds[k].a].push_back({ p.bonds[k].b, k }); padj[p.bonds[k].b].push_back({ p.bonds[k].a, k }); }
    for (int k = 0; k < (int)m.bonds.size(); k++) { madj[m.bonds[k].a].push_back({ m.bonds[k].b, k }); madj[m.bonds[k].b].push_back({ m.bonds[k].a, k }); }
    auto atomOk = [&](int pi, int mi) {
        const Atom& a = p.atoms[pi];
        const Atom& b = m.atoms[mi];
        if (a.element == "*") return true;
        return a.element == b.element && a.aromatic == b.aromatic && (a.charge == 0 || a.charge == b.charge);
    };
    auto bondOk = [&](int pb, int mb) {
        const Bond& a = p.bonds[pb];
        const Bond& b = m.bonds[mb];
        if (a.aromatic || b.aromatic) return a.aromatic == b.aromatic;
        return a.order == b.order;
    };
    // Pattern atoms in breadth-first order so each (after the first) has a matched neighbour.
    std::vector<int> order, parent(pn, -1);
    {
        std::vector<char> seen(pn, 0);
        for (int s = 0; s < pn; s++) {
            if (seen[s]) continue;
            std::queue<int> q;
            q.push(s); seen[s] = 1;
            while (!q.empty()) {
                const int v = q.front(); q.pop();
                order.push_back(v);
                for (auto [w, _] : padj[v]) if (!seen[w]) { seen[w] = 1; parent[w] = v; q.push(w); }
            }
        }
    }
    std::vector<std::vector<int>> out;
    std::set<std::vector<int>> unique;
    std::vector<int> map(pn, -1);
    std::vector<char> used(mn, 0);
    std::function<void(size_t)> extend = [&](size_t depth) {
        if (out.size() > 512) return;
        if (depth == order.size()) {
            std::vector<int> key = map;
            std::sort(key.begin(), key.end());
            if (unique.insert(key).second) out.push_back(map);
            return;
        }
        const int pv = order[depth];
        std::vector<int> cands;
        if (parent[pv] >= 0) for (auto [w, _] : madj[map[parent[pv]]]) cands.push_back(w);
        else for (int i = 0; i < mn; i++) cands.push_back(i);
        for (int mv : cands) {
            if (used[mv] || !atomOk(pv, mv)) continue;
            bool ok = true;
            for (auto [pw, pb] : padj[pv]) {          // every bond to an already matched atom must exist
                if (map[pw] < 0) continue;
                int mb = -1;
                for (auto [mw, b] : madj[mv]) if (mw == map[pw]) { mb = b; break; }
                if (mb < 0 || !bondOk(pb, mb)) { ok = false; break; }
            }
            if (!ok) continue;
            map[pv] = mv; used[mv] = 1;
            extend(depth + 1);
            map[pv] = -1; used[mv] = 0;
        }
    };
    if (pn > 0) extend(0);
    return out;
}

// ------------------------------------------------------------------ 2D skeletal formulas
namespace {
// Label colour: the CPK colour, darkened when it would be too pale on paper.
Color labelColor(const std::string& el, Color ink) {
    if (el == "C" || el == "H" || el == "*") return ink;
    Color c = cpkColor(el);
    const float lum = (0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b) / 255;
    if (lum > 0.5f) {
        const float k = 0.5f / lum;
        c = { (unsigned char)(c.r * k), (unsigned char)(c.g * k), (unsigned char)(c.b * k), 255 };
    }
    return c;
}
bool showLabel(const Molecule& m, int i, bool carbons) {
    const Atom& a = m.atoms[i];
    if (a.element != "C") return true;
    if (carbons || a.charge || a.isotope) return true;
    for (const Bond& b : m.bonds) if (b.a == i || b.b == i) return false;
    return true;
}
}

Depiction::Depiction(Molecule m, Style2D st) : style(std::move(st)) {
    // Fold hydrogen atoms into their neighbours' counts.
    std::vector<int> keep(m.atoms.size(), 1);
    for (const Bond& b : m.bonds) {
        for (int k = 0; k < 2; k++) {
            const int h = k ? b.b : b.a, o = k ? b.a : b.b;
            const Atom& a = m.atoms[h];
            if (a.element == "H" && !a.charge && !a.isotope && m.atoms[o].element != "H" && keep[h]) { keep[h] = 0; m.atoms[o].hydrogens++; }
        }
    }
    std::vector<int> remap(m.atoms.size(), -1);
    Molecule out;
    out.name = m.name;
    for (size_t i = 0; i < m.atoms.size(); i++) if (keep[i]) { remap[i] = (int)out.atoms.size(); out.atoms.push_back(m.atoms[i]); }
    for (const Bond& b : m.bonds) if (remap[b.a] >= 0 && remap[b.b] >= 0) { Bond nb = b; nb.a = remap[b.a]; nb.b = remap[b.b]; out.bonds.push_back(nb); }
    bool flat = !m.coords3d;
    if (flat) {                                      // MOL 2D: scale to bond length 1
        std::vector<float> lens;
        for (const Bond& b : out.bonds) lens.push_back(std::hypot(out.atoms[b.a].pos.x - out.atoms[b.b].pos.x, out.atoms[b.a].pos.y - out.atoms[b.b].pos.y));
        std::sort(lens.begin(), lens.end());
        const float med = lens.empty() ? 1 : lens[lens.size() / 2];
        if (med < 1e-3f) flat = false;
        else for (Atom& a : out.atoms) { a.pos.x /= med; a.pos.y /= med; }
    }
    if (!flat) depict(out);
    molecule = std::move(out);
}
std::shared_ptr<Depiction> Depiction::make(Molecule m, Style2D st) { return std::make_shared<Depiction>(std::move(m), std::move(st)); }

Depiction& Depiction::build(int at, int stagger, int duration) { steps_.push_back({ 0, at, std::max(1, duration), { stagger }, BLANK }); return *this; }
Depiction& Depiction::highlight(int at, int duration, std::vector<int> atoms, Color color) { steps_.push_back({ 1, at, std::max(1, duration), std::move(atoms), color }); return *this; }
Depiction& Depiction::unhighlight(int at, int duration) { steps_.push_back({ 2, at, std::max(1, duration), {}, BLANK }); return *this; }

namespace {
struct Fit { Vector2 center; float scale; float rot; };
Fit fitOf(const Molecule& m, Rectangle box, float maxBond, float rotDeg) {
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    const float cs = std::cos(rotDeg * DEG2RAD), sn = std::sin(rotDeg * DEG2RAD);
    for (const Atom& a : m.atoms) {
        const float x = a.pos.x * cs - a.pos.y * sn, y = a.pos.x * sn + a.pos.y * cs;
        x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y);
    }
    if (m.atoms.empty()) { x0 = y0 = x1 = y1 = 0; }
    const float pad = 1.0f;                          // labels and hydrogens past the outer atoms
    const float s = std::min({ maxBond, box.width / std::max(0.5f, x1 - x0 + 2 * pad), box.height / std::max(0.5f, y1 - y0 + 2 * pad) });
    return { { (x0 + x1) / 2, (y0 + y1) / 2 }, s, rotDeg };
}
Vector2 place(const Fit& f, Rectangle box, Vector3 p) {
    const float cs = std::cos(f.rot * DEG2RAD), sn = std::sin(f.rot * DEG2RAD);
    const float x = p.x * cs - p.y * sn, y = p.x * sn + p.y * cs;
    return { box.x + box.width / 2 + (x - f.center.x) * f.scale, box.y + box.height / 2 + (y - f.center.y) * f.scale };
}
Rectangle boxOr(Rectangle box, const Ctx& c) {
    if (box.width > 0 && box.height > 0) return box;
    const Vector2 cs = canvasSize();
    return { -c.o.x / c.s, -c.o.y / c.s, cs.x / c.s, cs.y / c.s };
}
void line(const Ctx& c, Vector2 a, Vector2 b, float w, Color col) {
    dr::strokeSeg(c, a, b, w, col);
}
}

Vector2 Depiction::atomAt(int i, Rectangle box) const {
    const Fit f = fitOf(molecule, box, style.bondLength, rotation);
    return place(f, box, molecule.atoms.at((size_t)i).pos);
}

void Depiction::draw(const Ctx& c, int t, Rectangle boxIn) {
    const Molecule& m = molecule;
    const int n = (int)m.atoms.size();
    if (n == 0) return;
    const Rectangle box = boxOr(boxIn, c);
    if (box.width <= 0) return;
    const Fit f = fitOf(m, box, style.bondLength, rotation);
    const float L = f.scale;                          // px per bond
    const float lw = style.lineWidth * L / 64.f, fs = style.fontSize * L;
    const Font font = style.font.texture.id ? style.font : fonts::ui(true);
    std::vector<Vector2> P(n);
    for (int i = 0; i < n; i++) P[i] = place(f, box, m.atoms[i].pos);

    // Reveal times (build steps) and highlights.
    std::vector<float> atomVis(n, 1), bondVis(m.bonds.size(), 1), hl(n, 0);
    std::vector<Color> hlColor(n, style.highlight);
    const Easing eo = ease::out(ease::cubic);
    for (const Step& s : steps_) {
        if (s.type != 0) continue;
        // Breadth first from the atom nearest the centre.
        int start = 0;
        float best = 1e9f;
        for (int i = 0; i < n; i++) { const float d = std::hypot(m.atoms[i].pos.x, m.atoms[i].pos.y); if (d < best) { best = d; start = i; } }
        std::vector<int> when(n, -1), order;
        std::vector<char> seen(n, 0);
        for (int root = start, guard = 0; guard <= n; guard++) {
            if (!seen[root]) {
                std::queue<int> q;
                q.push(root); seen[root] = 1;
                while (!q.empty()) {
                    const int v = q.front(); q.pop();
                    order.push_back(v);
                    for (int w : m.neighbors(v)) if (!seen[w]) { seen[w] = 1; q.push(w); }
                }
            }
            root = -1;
            for (int i = 0; i < n; i++) if (!seen[i]) { root = i; break; }
            if (root < 0) break;
        }
        const int stagger = s.atoms.empty() ? 2 : s.atoms[0];
        for (size_t k = 0; k < order.size(); k++) when[order[k]] = s.at + (int)k * stagger;
        for (int i = 0; i < n; i++) atomVis[i] = std::clamp((t - when[i]) / (float)s.dur, 0.f, 1.f);
        for (size_t k = 0; k < m.bonds.size(); k++) {
            const int a = when[m.bonds[k].a], b = when[m.bonds[k].b];
            const int st = std::min(a, b) + s.dur / 4;
            bondVis[k] = std::clamp((t - st) / (float)std::max(s.dur, std::abs(b - a)), 0.f, 1.f);
        }
    }
    for (const Step& s : steps_) {
        if (t < s.at) continue;
        const float u = (float)eo(std::clamp((t - s.at) / (float)s.dur, 0.f, 1.f));
        if (s.type == 1) for (int i : s.atoms) if (i >= 0 && i < n) { hl[i] = std::max(hl[i], u); if (s.color.a) hlColor[i] = s.color; }
        if (s.type == 2) for (float& h : hl) h *= 1 - u;
    }

    std::vector<char> labeled(n);
    for (int i = 0; i < n; i++) labeled[i] = showLabel(m, i, style.carbonLabels);
    const auto rings = m.rings();
    auto ringOf = [&](int a, int b) -> int {                // smallest ring holding bond a-b
        int best = -1;
        for (int r = 0; r < (int)rings.size(); r++) {
            const auto& R = rings[r];
            for (size_t i = 0; i < R.size(); i++) {
                const int x = R[i], y = R[(i + 1) % R.size()];
                if ((x == a && y == b) || (x == b && y == a)) { if (best < 0 || R.size() < rings[best].size()) best = r; }
            }
        }
        return best;
    };
    auto ringCenter = [&](int r) {
        Vector2 cc{ 0, 0 };
        for (int i : rings[r]) { cc.x += P[i].x; cc.y += P[i].y; }
        return Vector2{ cc.x / rings[r].size(), cc.y / rings[r].size() };
    };
    const float labelR = fs * 0.62f;
    auto trimEnd = [&](Vector2 from, Vector2 to, int atom) {     // stop short of a label
        if (!labeled[atom]) return to;
        const float d = std::hypot(to.x - from.x, to.y - from.y);
        if (d < 1e-3f) return to;
        const float k = std::max(0.f, (d - labelR) / d);
        return Vector2{ from.x + (to.x - from.x) * k, from.y + (to.y - from.y) * k };
    };
    auto inkOf = [&](int atom) { return style.colorAtoms && labeled[atom] ? labelColor(m.atoms[atom].element, style.ink) : style.ink; };
    // A segment drawn in two halves coloured by their atoms, revealed from `from`'s end.
    auto halves = [&](Vector2 a, Vector2 b, int ia, int ib, float w, float vis, bool reverse) {
        if (vis <= 0) return;
        Vector2 s = a, e = b;
        if (reverse) std::swap(s, e);
        const Vector2 end{ s.x + (e.x - s.x) * vis, s.y + (e.y - s.y) * vis };
        const Vector2 mid{ (a.x + b.x) / 2, (a.y + b.y) / 2 };
        const Color ca = inkOf(ia), cb = inkOf(ib);
        if (ca.r == cb.r && ca.g == cb.g && ca.b == cb.b) { line(c, s, end, w, ca); return; }
        // From s towards end, switching colour at the middle.
        const float total = std::hypot(e.x - s.x, e.y - s.y), shown = total * vis, half = total / 2;
        const Color c0 = reverse ? cb : ca, c1 = reverse ? ca : cb;
        if (shown <= half) { line(c, s, end, w, c0); return; }
        line(c, s, mid, w, c0);
        line(c, mid, end, w, c1);
    };

    // Highlight glow under everything.
    for (size_t k = 0; k < m.bonds.size(); k++) {
        const Bond& b = m.bonds[k];
        const float h = std::min(hl[b.a], hl[b.b]) * bondVis[k];
        if (h <= 0.003f) continue;
        line(c, P[b.a], P[b.b], lw * 4.2f, withAlpha(hlColor[b.a], 0.38f * h));
    }
    for (int i = 0; i < n; i++)
        if (hl[i] > 0.003f && atomVis[i] > 0) dr::circle(c, P[i], L * 0.3f, withAlpha(hlColor[i], 0.32f * hl[i] * atomVis[i]));

    // Bonds.
    for (size_t k = 0; k < m.bonds.size(); k++) {
        const Bond& b = m.bonds[k];
        const float vis = (float)ease::inOut(ease::cubic)(bondVis[k]);
        if (vis <= 0) continue;
        const bool rev = false;
        const Vector2 A = trimEnd(P[b.b], P[b.a], b.a), B = trimEnd(P[b.a], P[b.b], b.b);
        Vector2 dir{ P[b.b].x - P[b.a].x, P[b.b].y - P[b.a].y };
        const float len = std::max(1e-3f, std::hypot(dir.x, dir.y));
        dir = { dir.x / len, dir.y / len };
        const Vector2 nrm{ -dir.y, dir.x };
        if (b.stereo != Bond::Stereo::None) {
            const Vector2 e{ A.x + (B.x - A.x) * vis, A.y + (B.y - A.y) * vis };
            const float wide = L * 0.11f * vis;
            if (b.stereo == Bond::Stereo::Wedge) {
                const Vector2 tri[3] = { A, { e.x + nrm.x * wide, e.y + nrm.y * wide }, { e.x - nrm.x * wide, e.y - nrm.y * wide } };
                dr::fillPoly(c, tri, 3, inkOf(b.a));
            } else {
                for (int h = 0; h <= 6; h++) {
                    const float u = h / 6.f * vis, ww = L * 0.11f * u;
                    const Vector2 q{ A.x + (B.x - A.x) * u, A.y + (B.y - A.y) * u };
                    line(c, { q.x + nrm.x * ww, q.y + nrm.y * ww }, { q.x - nrm.x * ww, q.y - nrm.y * ww }, lw * 0.8f, inkOf(b.a));
                }
            }
            continue;
        }
        const bool aromaticRing = style.aromaticCircles && b.aromatic;
        const int order = aromaticRing ? 1 : b.order;
        if (order == 1) { halves(A, B, b.a, b.b, lw, vis, rev); continue; }
        const float gap = L * 0.17f;
        if (order == 3) {
            halves(A, B, b.a, b.b, lw, vis, rev);
            for (float sgn : { -1.f, 1.f })
                halves({ A.x + nrm.x * gap * sgn, A.y + nrm.y * gap * sgn }, { B.x + nrm.x * gap * sgn, B.y + nrm.y * gap * sgn }, b.a, b.b, lw, vis, rev);
            continue;
        }
        // Double: inside the ring, or to the side with more substituents, or centred.
        const int r = ringOf(b.a, b.b);
        float side = 0;
        if (r >= 0) {
            const Vector2 rc = ringCenter(r);
            side = ((rc.x - P[b.a].x) * nrm.x + (rc.y - P[b.a].y) * nrm.y) > 0 ? 1.f : -1.f;
        } else {
            const auto na = m.neighbors(b.a), nb = m.neighbors(b.b);
            if (na.size() > 1 && nb.size() > 1) {
                float s = 0;
                for (int o : na) if (o != b.b) s += (P[o].x - P[b.a].x) * nrm.x + (P[o].y - P[b.a].y) * nrm.y;
                for (int o : nb) if (o != b.a) s += (P[o].x - P[b.b].x) * nrm.x + (P[o].y - P[b.b].y) * nrm.y;
                side = s >= 0 ? 1.f : -1.f;
            }
        }
        if (side == 0) {
            for (float sgn : { -0.5f, 0.5f })
                halves({ A.x + nrm.x * gap * sgn, A.y + nrm.y * gap * sgn }, { B.x + nrm.x * gap * sgn, B.y + nrm.y * gap * sgn }, b.a, b.b, lw, vis, rev);
            continue;
        }
        halves(A, B, b.a, b.b, lw, vis, rev);
        const float inset = L * 0.14f;
        Vector2 a2{ P[b.a].x + nrm.x * gap * side, P[b.a].y + nrm.y * gap * side }, b2{ P[b.b].x + nrm.x * gap * side, P[b.b].y + nrm.y * gap * side };
        a2 = labeled[b.a] ? trimEnd(b2, a2, b.a) : Vector2{ a2.x + dir.x * inset, a2.y + dir.y * inset };
        b2 = labeled[b.b] ? trimEnd(a2, b2, b.b) : Vector2{ b2.x - dir.x * inset, b2.y - dir.y * inset };
        halves(a2, b2, b.a, b.b, lw, vis, rev);
    }
    if (style.aromaticCircles)
        for (const auto& R : rings) {
            bool arom = true;
            for (int i : R) if (!m.atoms[i].aromatic) arom = false;
            if (!arom) continue;
            float vis = 1;
            for (int i : R) vis = std::min(vis, atomVis[i]);
            if (vis <= 0) continue;
            Vector2 cc{ 0, 0 };
            for (int i : R) { cc.x += P[i].x; cc.y += P[i].y; }
            cc = { cc.x / R.size(), cc.y / R.size() };
            const float rad = 0.5f / std::sin((float)PI / R.size()) * L * 0.62f;
            dr::arc(c, cc, rad, lw, -90, -90 + 360 * (float)eo(vis), style.ink);
        }
    // Round joints where unlabelled atoms meet several bonds.
    for (int i = 0; i < n; i++) {
        if (labeled[i] || atomVis[i] <= 0) continue;
        int shown = 0;
        for (size_t k = 0; k < m.bonds.size(); k++) if ((m.bonds[k].a == i || m.bonds[k].b == i) && bondVis[k] > 0.02f) shown++;
        if (shown >= 2) dr::circle(c, P[i], lw * 0.5f, style.ink);
    }

    // Atom labels: element, hydrogens on the open side, charge.
    for (int i = 0; i < n; i++) {
        if (!labeled[i]) continue;
        const float vis = atomVis[i];
        if (vis <= 0) continue;
        const Atom& a = m.atoms[i];
        const float pop = (float)std::max(0.0, ease::out(ease::back(1.8))(vis));
        Ctx lc = c;
        lc.o = mapP(c, { P[i].x * (1 - pop), P[i].y * (1 - pop) });
        lc.s = c.s * pop;
        const Color col = withAlpha(inkOf(i), std::min(1.f, vis * 2));
        const float f1 = fs, f2 = fs * 0.68f;
        std::string sym = a.element == "*" ? "*" : a.element;
        if (a.isotope) {
            dr::text(font, lc, std::to_string(a.isotope).c_str(), { P[i].x - fs * 0.35f, P[i].y - f1 * 0.5f }, f2, 0, col, 1.0, 0.3);
        }
        dr::text(font, lc, sym.c_str(), P[i], f1, 0, col, 0.5, 0.5);
        const Vector2 symSize = MeasureTextEx(font, sym.c_str(), f1, 0);
        float rightEdge = P[i].x + symSize.x / 2;
        if (a.hydrogens > 0) {
            // Hydrogens go on the side with fewer bonds.
            float dx = 0;
            for (int o : m.neighbors(i)) dx += P[o].x - P[i].x;
            const bool left = dx > L * 0.3f;
            const std::string cnt = a.hydrogens > 1 ? std::to_string(a.hydrogens) : "";
            const float hw = MeasureTextEx(font, "H", f1, 0).x, cw = cnt.empty() ? 0 : MeasureTextEx(font, cnt.c_str(), f2, 0).x;
            if (left) {
                const float x = P[i].x - symSize.x / 2 - cw;
                dr::text(font, lc, "H", { x - hw, P[i].y }, f1, 0, col, 0, 0.5);
                if (!cnt.empty()) dr::text(font, lc, cnt.c_str(), { x, P[i].y + f1 * 0.22f }, f2, 0, col, 0, 0.5);
            } else {
                dr::text(font, lc, "H", { rightEdge, P[i].y }, f1, 0, col, 0, 0.5);
                rightEdge += hw;
                if (!cnt.empty()) { dr::text(font, lc, cnt.c_str(), { rightEdge, P[i].y + f1 * 0.22f }, f2, 0, col, 0, 0.5); rightEdge += cw; }
            }
        }
        if (a.charge) {
            const std::string q = (std::abs(a.charge) > 1 ? std::to_string(std::abs(a.charge)) : "") + (a.charge > 0 ? "+" : "\xE2\x88\x92");
            dr::text(font, lc, q.c_str(), { rightEdge + 1, P[i].y - f1 * 0.42f }, f2, 0, col, 0, 0.5);
        }
    }
}

} // namespace chem
} // namespace rmtn
