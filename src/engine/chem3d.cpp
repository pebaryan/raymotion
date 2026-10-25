// chem3d.cpp -- 3D molecular models on g3d scenes (ball-and-stick, sticks, space
// filling), PDB parsing and protein cartoons.
#include "rmtn/chem.hpp"
#include "rmtn/anim.hpp"
#include "rmtn/media.hpp"
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace rmtn {
namespace chem {

namespace {
// ------------------------------------------------------------------ mesh building
struct MeshBuf {
    std::vector<float> v, n, tc;
    std::vector<unsigned short> idx;
    int vertex(Vector3 p, Vector3 nrm) {
        v.insert(v.end(), { p.x, p.y, p.z });
        n.insert(n.end(), { nrm.x, nrm.y, nrm.z });
        tc.insert(tc.end(), { 0.f, 0.f });
        return (int)v.size() / 3 - 1;
    }
    void tri(int a, int b, int c) { idx.insert(idx.end(), { (unsigned short)a, (unsigned short)b, (unsigned short)c }); }
    Mesh upload() const {
        Mesh m{};
        m.vertexCount = (int)v.size() / 3;
        m.triangleCount = (int)idx.size() / 3;
        auto copy = [](const auto& src) {
            using T = typename std::decay_t<decltype(src)>::value_type;
            T* p = (T*)MemAlloc((unsigned)(src.size() * sizeof(T)));
            std::copy(src.begin(), src.end(), p);
            return p;
        };
        m.vertices = copy(v);
        m.normals = copy(n);
        m.texcoords = copy(tc);
        m.indices = copy(idx);
        UploadMesh(&m, false);
        return m;
    }
};

Vector3 perpendicular(Vector3 d) {
    const Vector3 ref = std::fabs(d.y) < 0.9f ? Vector3{ 0, 1, 0 } : Vector3{ 1, 0, 0 };
    return Vector3Normalize(Vector3CrossProduct(d, ref));
}

// An open cylinder from p0 to p1.
void tube(MeshBuf& mb, Vector3 p0, Vector3 p1, float r, int slices) {
    const Vector3 d = Vector3Normalize(Vector3Subtract(p1, p0));
    const Vector3 u = perpendicular(d), w = Vector3CrossProduct(d, u);
    const int base = (int)mb.v.size() / 3;
    for (int i = 0; i <= slices; i++) {
        const float a = 2 * PI * i / slices;
        const Vector3 nrm = Vector3Add(Vector3Scale(u, std::cos(a)), Vector3Scale(w, std::sin(a)));
        mb.vertex(Vector3Add(p0, Vector3Scale(nrm, r)), nrm);
        mb.vertex(Vector3Add(p1, Vector3Scale(nrm, r)), nrm);
    }
    for (int i = 0; i < slices; i++) {
        const int a = base + i * 2, b = a + 1, c = a + 2, e = a + 3;
        mb.tri(a, c, b);
        mb.tri(b, c, e);
    }
}

g3d::GeometryPtr meshGeometry(MeshBuf mb) {
    auto shared = std::make_shared<MeshBuf>(std::move(mb));
    return std::make_shared<g3d::Geometry>([shared] { return shared->upload(); });
}

// Directions for `h` new hydrogens on an atom whose existing bonds point along `bonds`.
std::vector<Vector3> hydrogenDirections(const std::vector<Vector3>& bonds, int h, int i) {
    Vector3 sum{ 0, 0, 0 };
    for (Vector3 b : bonds) sum = Vector3Add(sum, Vector3Normalize(b));
    Vector3 base = Vector3Length(sum) > 1e-3f ? Vector3Normalize(Vector3Negate(sum)) : Vector3{ 0, 1, 0 };
    if (bonds.empty()) base = { 0, 1, 0 };
    std::vector<Vector3> out;
    if (h == 1 && !bonds.empty()) { out.push_back(base); return out; }
    Vector3 p = bonds.empty() ? Vector3{ 1, 0, 0 } : perpendicular(base);
    if (bonds.size() == 1) {                         // keep the plane of the existing bond's partner off
        const Vector3 q = Vector3CrossProduct(base, Vector3Normalize(bonds[0]));
        if (Vector3Length(q) > 1e-3f) p = Vector3Normalize(q);
    }
    const float tilt = (bonds.size() >= 2 || h >= 3) ? 70.5f * DEG2RAD : 60.f * DEG2RAD;
    for (int k = 0; k < h; k++) {
        const float a = 2 * PI * k / h + i * 0.7f;
        const Vector3 around = Vector3RotateByAxisAngle(p, base, a);
        out.push_back(Vector3Normalize(Vector3Add(Vector3Scale(base, std::cos(tilt)), Vector3Scale(around, std::sin(tilt)))));
    }
    return out;
}

// The molecule as it will be shown: hydrogens explicit (and placed) or gone, 3D geometry.
Molecule prepared(const Molecule& in, bool hydrogens) {
    Molecule m = in;
    if (!hydrogens) {
        std::vector<int> remap(m.atoms.size(), -1);
        Molecule out;
        out.name = m.name;
        out.coords3d = m.coords3d;
        for (size_t i = 0; i < m.atoms.size(); i++)
            if (m.atoms[i].element != "H") { remap[i] = (int)out.atoms.size(); out.atoms.push_back(m.atoms[i]); out.atoms.back().hydrogens = 0; }
        for (const Bond& b : m.bonds) if (remap[b.a] >= 0 && remap[b.b] >= 0) { Bond nb = b; nb.a = remap[b.a]; nb.b = remap[b.b]; out.bonds.push_back(nb); }
        if (!out.coords3d) embed3d(out);
        return out;
    }
    if (!m.coords3d) {
        m = withHydrogens(m);
        embed3d(m);
        return m;
    }
    // Measured geometry with implicit hydrogens: place them off the existing bonds.
    const int n = (int)m.atoms.size();
    for (int i = 0; i < n; i++) {
        const int h = m.atoms[i].hydrogens;
        if (h <= 0) continue;
        std::vector<Vector3> bonds;
        for (int o : m.neighbors(i)) bonds.push_back(Vector3Subtract(m.atoms[o].pos, m.atoms[i].pos));
        const float len = covalentRadius(m.atoms[i].element) + covalentRadius("H");
        for (Vector3 d : hydrogenDirections(bonds, h, i)) {
            Atom H;
            H.element = "H";
            H.pos = Vector3Add(m.atoms[i].pos, Vector3Scale(d, len));
            m.atoms.push_back(H);
            Bond b;
            b.a = i; b.b = (int)m.atoms.size() - 1;
            m.bonds.push_back(b);
        }
        m.atoms[i].hydrogens = 0;
    }
    return m;
}

g3d::Material materialFor(Color c, const Style3D& st) {
    g3d::Material mat;
    mat.color = c;
    mat.roughness = st.roughness;
    mat.metalness = st.metalness;
    return mat;
}

g3d::Object3D* buildMolecule(g3d::Object3D& parent, const Molecule& in, const Style3D& st, bool center) {
    Molecule m = prepared(in, st.hydrogens);
    const int n = (int)m.atoms.size();
    Vector3 c{ 0, 0, 0 };
    if (center && n > 0) {
        for (const Atom& a : m.atoms) c = Vector3Add(c, a.pos);
        c = Vector3Scale(c, 1.f / n);
    }
    g3d::Object3D* group = parent.add();
    group->name = m.name.empty() ? "molecule" : m.name;
    std::map<std::pair<std::string, int>, g3d::GeometryPtr> spheres;
    auto radiusOf = [&](const Atom& a) {
        switch (st.mode) {
        case Style3D::Mode::SpaceFilling: return vdwRadius(a.element);
        case Style3D::Mode::Sticks: return st.bondRadius;
        default: return vdwRadius(a.element) * st.atomScale;
        }
    };
    // Breadth-first order so a build grows outwards along the bonds.
    std::vector<int> order;
    {
        std::vector<char> seen(n, 0);
        for (int s = 0; s < n; s++) {
            if (seen[s]) continue;
            std::queue<int> q;
            q.push(s); seen[s] = 1;
            while (!q.empty()) {
                const int v = q.front(); q.pop();
                order.push_back(v);
                for (int w : m.neighbors(v)) if (!seen[w]) { seen[w] = 1; q.push(w); }
            }
        }
    }
    std::vector<char> placed(n, 0), bondDone(m.bonds.size(), 0);
    for (int i : order) {
        const Atom& a = m.atoms[i];
        const float r = radiusOf(a);
        const auto key = std::make_pair(a.element, (int)std::lround(r * 1000));
        if (!spheres.count(key)) spheres[key] = g3d::Geometry::sphere(r, 20, 28);
        g3d::Object3D* o = group->add(spheres[key], materialFor(cpkColor(a.element), st));
        o->name = "atom:" + std::to_string(i);
        o->position = Vector3Subtract(a.pos, c);
        placed[i] = 1;
        if (st.mode == Style3D::Mode::SpaceFilling) continue;
        for (size_t k = 0; k < m.bonds.size(); k++) {
            const Bond& b = m.bonds[k];
            if (bondDone[k]) continue;
            const int other = b.a == i ? b.b : b.b == i ? b.a : -1;
            if (other < 0 || !placed[other]) continue;
            bondDone[k] = 1;
            // Grows from the atom placed first towards this one.
            const Vector3 p0 = Vector3Subtract(m.atoms[other].pos, c), p1 = Vector3Subtract(a.pos, c);
            const Vector3 d = Vector3Subtract(p1, p0), dir = Vector3Normalize(d);
            Vector3 side = perpendicular(dir);
            for (int nb : m.neighbors(other)) {
                if (nb == i) continue;
                const Vector3 q = Vector3Subtract(Vector3Subtract(m.atoms[nb].pos, c), p0);
                const Vector3 pn = Vector3CrossProduct(dir, q);
                if (Vector3Length(pn) > 1e-3f) { side = Vector3Normalize(Vector3CrossProduct(pn, dir)); break; }
            }
            const int lines = st.mode == Style3D::Mode::Sticks ? 1 : std::clamp(b.order, 1, 3);
            const float r = st.bondRadius * (lines == 1 ? 1.f : lines == 2 ? 0.62f : 0.5f);
            const float spread = lines == 1 ? 0 : st.bondRadius * (lines == 2 ? 1.05f : 1.5f);
            g3d::Object3D* bond = group->add();
            bond->name = "bond:" + std::to_string(k);
            bond->position = p0;
            for (int half = 0; half < 2; half++) {
                MeshBuf mb;
                for (int s = 0; s < lines; s++) {
                    const float off = lines == 1 ? 0 : lines == 2 ? (s ? spread : -spread) : (s - 1) * spread;
                    const Vector3 o3 = Vector3Scale(side, off);
                    const Vector3 a0 = Vector3Add(o3, Vector3Scale(d, half ? 0.5f : 0.f)), a1 = Vector3Add(o3, Vector3Scale(d, half ? 1.f : 0.5f));
                    tube(mb, a0, a1, r, 14);
                }
                const Color col = cpkColor(half ? a.element : m.atoms[other].element);
                bond->add(meshGeometry(std::move(mb)), materialFor(col, st));
            }
        }
    }
    return group;
}

std::string trimmed(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}
std::string col(const std::string& l, size_t at, size_t len) { return at < l.size() ? l.substr(at, len) : std::string(); }

bool nucleotide(const std::string& r) {
    return r == "DA" || r == "DC" || r == "DG" || r == "DT" || r == "DU" || r == "A" || r == "C" || r == "G" || r == "U";
}
} // namespace

g3d::Object3D* addMolecule(g3d::Object3D& parent, const Molecule& m, const Style3D& style) { return buildMolecule(parent, m, style, true); }
g3d::Object3D* addMolecule(g3d::Scene& scene, const Molecule& m, const Style3D& style) {
    g3d::Object3D* holder = scene.add();
    holder->name = m.name.empty() ? "molecule" : m.name;
    // The model is the holder's only child; return it so callers can animate it.
    return buildMolecule(*holder, m, style, true);
}

void buildIn(g3d::Object3D* model, int at, int stagger, int duration) {
    if (!model) return;
    duration = std::max(1, duration);
    for (size_t i = 0; i < model->children.size(); i++) {
        const int start = at + (int)i * stagger;
        model->children[i]->up = [start, duration](int t, g3d::Object3D& o) {
            const float u = std::clamp((t - start) / (float)duration, 0.f, 1.f);
            const float s = (float)std::max(0.0, ease::out(ease::back(1.4))(u));
            o.scale = { s, s, s };
            o.visible = u > 0;
        };
    }
}

// ------------------------------------------------------------------ proteins
Protein pdb(std::string_view text) {
    Protein p;
    std::istringstream in{ std::string(text) };
    std::string l;
    struct Range { char chain; int from, to; char ss; };
    std::vector<Range> ranges;
    bool modelDone = false;
    struct Key { char chain; int seq; char icode; bool operator==(const Key& o) const { return chain == o.chain && seq == o.seq && icode == o.icode; } };
    std::vector<std::pair<Key, std::vector<int>>> groups;   // residue atoms in file order
    while (std::getline(in, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        const std::string rec = col(l, 0, 6);
        if (rec == "HEADER") p.id = trimmed(col(l, 62, 4));
        else if (rec == "TITLE ") { const std::string t = trimmed(col(l, 10, 70)); p.title += (p.title.empty() ? "" : " ") + t; }
        else if (rec == "ENDMDL") modelDone = true;
        else if (rec == "HELIX ") ranges.push_back({ col(l, 19, 1)[0], std::atoi(col(l, 21, 4).c_str()), std::atoi(col(l, 33, 4).c_str()), 'H' });
        else if (rec == "SHEET ") ranges.push_back({ col(l, 21, 1)[0], std::atoi(col(l, 22, 4).c_str()), std::atoi(col(l, 33, 4).c_str()), 'E' });
        else if ((rec == "ATOM  " || rec == "HETATM") && !modelDone) {
            const char alt = l.size() > 16 ? l[16] : ' ';
            if (alt != ' ' && alt != 'A' && alt != '1') continue;
            Atom a;
            a.name = trimmed(col(l, 12, 4));
            a.residue = trimmed(col(l, 17, 3));
            a.chain = l.size() > 21 ? l[21] : ' ';
            a.residueSeq = std::atoi(col(l, 22, 4).c_str());
            a.pos = { (float)std::atof(col(l, 30, 8).c_str()), (float)std::atof(col(l, 38, 8).c_str()), (float)std::atof(col(l, 46, 8).c_str()) };
            a.hetero = rec == "HETATM";
            std::string el = trimmed(col(l, 76, 2));
            if (el.empty()) for (char ch : a.name) if (std::isalpha((unsigned char)ch)) { el = std::string(1, ch); break; }
            if (!el.empty()) { el[0] = (char)std::toupper((unsigned char)el[0]); for (size_t k = 1; k < el.size(); k++) el[k] = (char)std::tolower((unsigned char)el[k]); }
            a.element = el;
            const std::string ch = col(l, 78, 2);
            if (ch.size() == 2 && std::isdigit((unsigned char)ch[0])) a.charge = (ch[0] - '0') * (ch[1] == '-' ? -1 : 1);
            const Key key{ a.chain, a.residueSeq, l.size() > 26 ? l[26] : ' ' };
            if (groups.empty() || !(groups.back().first == key)) groups.push_back({ key, {} });
            groups.back().second.push_back((int)p.atoms.size());
            p.atoms.push_back(std::move(a));
        }
    }
    if (p.atoms.empty()) throw std::runtime_error("pdb: no ATOM or HETATM records");
    for (auto& [key, ids] : groups) {
        Residue r;
        r.chain = key.chain;
        r.seq = key.seq;
        r.name = p.atoms[ids[0]].residue;
        bool hasN = false, hasC = false;
        int phos = -1;
        for (int i : ids) {
            const Atom& a = p.atoms[i];
            if (a.name == "CA" && a.element == "C") r.ca = i;
            else if (a.name == "O") r.o = i;
            else if (a.name == "N") hasN = true;
            else if (a.name == "C") hasC = true;
            else if (a.name == "P") phos = i;
            else if (a.name == "C4'" && r.o < 0 && nucleotide(r.name)) r.o = i;
        }
        if (r.ca >= 0 && !(hasN || hasC)) r.ca = -1;
        if (r.ca < 0 && phos >= 0 && nucleotide(r.name)) r.ca = phos;
        if (r.ca >= 0 && (!p.atoms[r.ca].hetero || hasN)) p.residues.push_back(r);
    }
    for (Residue& r : p.residues)
        for (const Range& g : ranges)
            if (g.chain == r.chain && r.seq >= g.from && r.seq <= g.to) r.structure = g.ss;
    if (ranges.empty()) {
        // Helices from C-alpha spacing: i..i+3 about 5.0-5.6 A and i..i+4 about 6.2 A.
        auto dist = [&](size_t a, size_t b) { return Vector3Distance(p.atoms[p.residues[a].ca].pos, p.atoms[p.residues[b].ca].pos); };
        const size_t n = p.residues.size();
        std::vector<char> h(n, 0);
        for (size_t i = 0; i + 4 < n; i++) {
            if (p.residues[i].chain != p.residues[i + 4].chain) continue;
            const float d3 = dist(i, i + 3), d4 = dist(i, i + 4);
            if (d3 > 4.6f && d3 < 5.8f && d4 > 5.6f && d4 < 6.8f) for (size_t k = i; k <= i + 4; k++) h[k] = 1;
        }
        for (size_t i = 0; i < n; i++) if (h[i]) p.residues[i].structure = 'H';
    }
    return p;
}

Protein loadProtein(const std::string& file) {
    const std::string path = media::resolve(file);
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("chem: cannot read " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return pdb(ss.str());
}

Molecule Protein::molecule(bool protein, bool hetero) const {
    Molecule m;
    m.name = id;
    m.coords3d = true;
    for (const Atom& a : atoms) {
        if (a.hetero ? !hetero : !protein) continue;
        if (a.residue == "HOH" || a.residue == "WAT" || a.residue == "DOD") continue;
        m.atoms.push_back(a);
    }
    // Bonds by distance, through a grid of 2.4 A cells.
    const float cell = 2.4f;
    std::unordered_map<long long, std::vector<int>> grid;
    auto keyOf = [&](int x, int y, int z) { return ((long long)(x + 4096) << 26) | ((long long)(y + 4096) << 13) | (long long)(z + 4096); };
    auto cellOf = [&](Vector3 p) { return std::array<int, 3>{ (int)std::floor(p.x / cell), (int)std::floor(p.y / cell), (int)std::floor(p.z / cell) }; };
    for (int i = 0; i < (int)m.atoms.size(); i++) { const auto c = cellOf(m.atoms[i].pos); grid[keyOf(c[0], c[1], c[2])].push_back(i); }
    for (int i = 0; i < (int)m.atoms.size(); i++) {
        const auto c = cellOf(m.atoms[i].pos);
        const float ri = covalentRadius(m.atoms[i].element);
        for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) for (int dz = -1; dz <= 1; dz++) {
            auto it = grid.find(keyOf(c[0] + dx, c[1] + dy, c[2] + dz));
            if (it == grid.end()) continue;
            for (int j : it->second) {
                if (j <= i) continue;
                if (m.atoms[i].element == "H" && m.atoms[j].element == "H") continue;
                const float d = Vector3Distance(m.atoms[i].pos, m.atoms[j].pos);
                if (d > 0.4f && d < ri + covalentRadius(m.atoms[j].element) + 0.45f) { Bond b; b.a = i; b.b = j; m.bonds.push_back(b); }
            }
        }
    }
    return m;
}

float Protein::radius() const {
    if (residues.empty()) return 0;
    Vector3 c{ 0, 0, 0 };
    for (const Residue& r : residues) c = Vector3Add(c, atoms[r.ca].pos);
    c = Vector3Scale(c, 1.f / residues.size());
    float rad = 0;
    for (const Residue& r : residues) rad = std::max(rad, Vector3Distance(c, atoms[r.ca].pos));
    return rad;
}

g3d::Object3D* addCartoon(g3d::Scene& scene, const Protein& p, const CartoonStyle& st) {
    g3d::Object3D* holder = scene.add();
    holder->name = p.id.empty() ? "protein" : p.id;
    g3d::Object3D* group = holder->add();
    group->name = holder->name;
    const size_t n = p.residues.size();
    if (n == 0) return group;
    Vector3 center{ 0, 0, 0 };
    for (const Residue& r : p.residues) center = Vector3Add(center, p.atoms[r.ca].pos);
    center = Vector3Scale(center, 1.f / n);

    // Colours.
    std::map<char, int> chainIndex;
    for (const Residue& r : p.residues) if (!chainIndex.count(r.chain)) { const int k = (int)chainIndex.size(); chainIndex[r.chain] = k; }
    static const unsigned chainColors[] = { 0x4f8ff7, 0xf7934f, 0x5fd38d, 0xe25d8a, 0xb18cf2, 0xf2d24f, 0x4fd2e2, 0xc9925b };
    auto colorOf = [&](size_t i) -> Color {
        const Residue& r = p.residues[i];
        switch (st.coloring) {
        case CartoonStyle::Coloring::Chain: return mkColor(chainColors[chainIndex[r.chain] % 8]);
        case CartoonStyle::Coloring::Structure: return mkColor(r.structure == 'H' ? 0xf0507a : r.structure == 'E' ? 0xf5c242 : 0xd5dae6);
        default: return ColorFromHSV(240.f * (1 - (float)i / std::max<size_t>(1, n - 1)), 0.72f, 0.95f);
        }
    };

    // Fragments: runs of residues joined along the chain.
    std::vector<std::pair<size_t, size_t>> frags;
    for (size_t i = 0; i < n;) {
        size_t j = i + 1;
        while (j < n && p.residues[j].chain == p.residues[j - 1].chain &&
               Vector3Distance(p.atoms[p.residues[j].ca].pos, p.atoms[p.residues[j - 1].ca].pos) < (nucleotide(p.residues[j].name) ? 8.f : 4.4f)) j++;
        frags.push_back({ i, j });
        i = j;
    }
    const int K = 12;                                       // cross-section points
    for (auto [f0, f1] : frags) {
        const int m = (int)(f1 - f0);
        std::vector<Vector3> P(m), O(m);
        for (int i = 0; i < m; i++) P[i] = Vector3Subtract(p.atoms[p.residues[f0 + i].ca].pos, center);
        for (int i = 0; i < m; i++) {
            const Residue& r = p.residues[f0 + i];
            Vector3 g = r.o >= 0 ? Vector3Subtract(Vector3Subtract(p.atoms[r.o].pos, center), P[i]) : Vector3{ 0, 0, 0 };
            if (Vector3Length(g) < 1e-3f) {
                const Vector3 a = P[std::max(0, i - 1)], b = P[std::min(m - 1, i + 1)];
                g = Vector3CrossProduct(Vector3Subtract(b, a), { 0, 1, 0 });
            }
            g = Vector3Normalize(g);
            if (i > 0 && Vector3DotProduct(g, O[i - 1]) < 0) g = Vector3Negate(g);     // no sudden flips
            O[i] = g;
        }
        auto at = [&](int i) { return P[std::clamp(i, 0, m - 1)]; };
        auto spline = [&](float u) {
            u = std::clamp(u, 0.f, (float)(m - 1));
            int i = std::min((int)std::floor(u), std::max(0, m - 2));
            const float t = u - i;
            if (m == 1) return P[0];
            const Vector3 p0 = i > 0 ? at(i - 1) : Vector3Subtract(Vector3Scale(at(0), 2), at(1));
            const Vector3 p1 = at(i), p2 = at(i + 1);
            const Vector3 p3 = i + 2 < m ? at(i + 2) : Vector3Subtract(Vector3Scale(at(m - 1), 2), at(m - 2));
            const float t2 = t * t, t3 = t2 * t;
            auto comp = [&](float a, float b, float c, float d) {
                return 0.5f * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3);
            };
            return Vector3{ comp(p0.x, p1.x, p2.x, p3.x), comp(p0.y, p1.y, p2.y, p3.y), comp(p0.z, p1.z, p2.z, p3.z) };
        };
        auto ssAt = [&](int i) { return p.residues[f0 + std::clamp(i, 0, m - 1)].structure; };
        auto sizeOf = [&](int i) -> Vector2 {
            const char s = ssAt(i);
            if (s == 'H') return { st.helixWidth, st.thickness };
            if (s == 'E') return { st.strandWidth, st.thickness };
            return { st.coilRadius * 2, st.coilRadius * 2 };
        };
        auto section = [&](float u, Vector3& c, Vector3& T, Vector3& N, Vector3& B, Vector2& size) {
            c = spline(u);
            T = Vector3Normalize(Vector3Subtract(spline(u + 0.05f), spline(u - 0.05f)));
            if (Vector3Length(T) < 1e-4f) T = { 1, 0, 0 };
            const int i = std::clamp((int)std::floor(u), 0, m - 1);
            const float f = u - i;
            Vector3 g = Vector3Normalize(Vector3Lerp(O[i], O[std::min(m - 1, i + 1)], f));
            N = Vector3Subtract(g, Vector3Scale(T, Vector3DotProduct(g, T)));
            N = Vector3Length(N) < 1e-4f ? perpendicular(T) : Vector3Normalize(N);
            B = Vector3CrossProduct(T, N);
            const float s = f * f * (3 - 2 * f);
            const Vector2 a = sizeOf(i), b = sizeOf(i + 1);
            size = { a.x + (b.x - a.x) * s, a.y + (b.y - a.y) * s };
            // Strands end in an arrowhead across their last residue.
            const int j = std::clamp((int)std::lround(u), 0, m - 1);
            if (ssAt(j) == 'E' && (j == m - 1 || ssAt(j + 1) != 'E')) {
                const float k = std::clamp(u - (j - 0.5f), 0.f, 1.f);
                size = { st.strandWidth * 1.65f * (1 - k) + st.coilRadius * 2 * k, st.thickness };
            } else if (ssAt(j) == 'E') size = { st.strandWidth, st.thickness };
        };
        for (int i = 0; i < m; i++) {
            const float u0 = std::max(0.f, i - 0.5f), u1 = std::min((float)(m - 1), i + 0.5f);
            const int S = std::max(2, (int)std::ceil(st.samples * (u1 - u0)) + 1);
            MeshBuf mb;
            Vector3 origin{ 0, 0, 0 };
            std::vector<int> rings;
            for (int s = 0; s < S; s++) {
                const float u = u0 + (u1 - u0) * s / (S - 1);
                Vector3 c, T, N, B;
                Vector2 size;
                section(u, c, T, N, B, size);
                if (s == 0) origin = c;
                rings.push_back((int)mb.v.size() / 3);
                const float a2 = size.x / 2, b2 = size.y / 2;
                for (int k = 0; k <= K; k++) {
                    const float th = 2 * PI * k / K, cs = std::cos(th), sn = std::sin(th);
                    const Vector3 pos = Vector3Add(Vector3Subtract(c, origin), Vector3Add(Vector3Scale(B, a2 * cs), Vector3Scale(N, b2 * sn)));
                    const Vector3 nrm = Vector3Normalize(Vector3Add(Vector3Scale(B, cs / std::max(0.01f, a2)), Vector3Scale(N, sn / std::max(0.01f, b2))));
                    mb.vertex(pos, nrm);
                }
                // Caps where a fragment begins or ends.
                const bool cap = (i == 0 && s == 0) || (i == m - 1 && s == S - 1);
                if (cap) {
                    const Vector3 face = s == 0 ? Vector3Negate(T) : T;
                    const int mid = mb.vertex(Vector3Subtract(c, origin), face);
                    const int first = mb.vertex(Vector3Add(Vector3Subtract(c, origin), Vector3Scale(B, a2)), face);
                    for (int k = 1; k <= K; k++) {
                        const float th = 2 * PI * k / K;
                        const int v = mb.vertex(Vector3Add(Vector3Subtract(c, origin), Vector3Add(Vector3Scale(B, a2 * std::cos(th)), Vector3Scale(N, b2 * std::sin(th)))), face);
                        if (s == 0) mb.tri(mid, v, v - 1); else mb.tri(mid, v - 1, v);
                    }
                    (void)first;
                }
            }
            for (int s = 0; s + 1 < S; s++)
                for (int k = 0; k < K; k++) {
                    const int a = rings[s] + k, b = rings[s] + k + 1, c2 = rings[s + 1] + k, d = rings[s + 1] + k + 1;
                    mb.tri(a, c2, b);
                    mb.tri(b, c2, d);
                }
            g3d::Material mat;
            mat.color = colorOf(f0 + i);
            mat.roughness = st.roughness;
            g3d::Object3D* seg = group->add(meshGeometry(std::move(mb)), mat);
            seg->name = "res:" + std::to_string(f0 + i);
            seg->position = origin;
        }
    }
    if (st.ligands) {
        Molecule lig = p.molecule(false, true);
        if (!lig.atoms.empty()) {
            for (Atom& a : lig.atoms) a.pos = Vector3Subtract(a.pos, center);
            Style3D ls;
            ls.mode = Style3D::Mode::Sticks;
            ls.hydrogens = false;
            ls.bondRadius = 0.2f;
            g3d::Object3D* l = buildMolecule(*group, lig, ls, false);
            l->name = "ligands";
        }
    }
    return group;
}

} // namespace chem
} // namespace rmtn
