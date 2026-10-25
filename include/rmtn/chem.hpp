// rmtn/chem.hpp -- molecules and proteins on the timeline: SMILES, MOL / SDF and PDB in;
// 2D skeletal formulas and 3D ball-and-stick, space-filling and cartoon models out,
// built up and highlighted over time.
//
//     auto caffeine = chem::Depiction::make(chem::smiles("CN1C=NC2=C1C(=O)N(C(=O)N2C)C"));
//     caffeine->build(0);                                     // atoms and bonds appear in turn
//     caffeine->highlight(60, 20, chem::find(caffeine->molecule, "C=O")[0]);
//     addRoot(c.roots, 0, 180)->fn = chem::draw(caffeine, { 560, 140, 800, 800 });
//
//     auto scene = std::make_shared<g3d::Scene>();
//     g3d::Object3D* mol = chem::addMolecule(*scene, chem::loadMolecule("data/caffeine.sdf"));
//     chem::buildIn(mol, 0, 2, 12);                           // grows atom by atom
//     g3d::Object3D* crambin = chem::addCartoon(*scene, chem::loadProtein("data/1crn.pdb"));
//
// Coordinates are Angstroms for files with real geometry; SMILES depictions use bond
// length 1. 3D models are centred on the origin (scale and place them like any object).
#pragma once
#include <raylib.h>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "scene3d.hpp"
#include "timeline.hpp"

namespace rmtn {
namespace chem {

// ------------------------------------------------------------------ structures
struct Atom {
    std::string element;         // "C", "N", "Cl"; "*" for a SMILES wildcard
    int charge = 0, isotope = 0;
    int hydrogens = 0;           // attached hydrogens that are not atoms of their own
    bool aromatic = false;
    Vector3 pos{ 0, 0, 0 };
    // PDB records
    std::string name, residue;
    int residueSeq = 0;
    char chain = ' ';
    bool hetero = false;
};

struct Bond {
    enum class Stereo { None, Wedge, Hash };
    int a = 0, b = 0;
    int order = 1;               // 1, 2, 3 (aromatic bonds are kekulised where possible)
    bool aromatic = false;
    Stereo stereo = Stereo::None;    // from MOL files, pointing from a to b
};

struct Molecule {
    std::string name;
    std::vector<Atom> atoms;
    std::vector<Bond> bonds;
    bool coords3d = false;       // pos holds measured 3D geometry (else a 2D depiction, z = 0)

    // Smallest set of smallest rings, each as atom indices in ring order.
    std::vector<std::vector<int>> rings() const;
    std::string formula() const;             // Hill order: "C8H10N4O2"
    double mass() const;                     // average molecular weight, g/mol
    float radius() const;                    // furthest atom from the centroid
    std::vector<int> neighbors(int atom) const;
};

// SMILES (organic subset and bracket atoms, charges, isotopes, explicit H, bond orders,
// aromatic rings, branches, ring closures incl. %nn, dot-disconnected parts), with
// implicit hydrogens, kekulised aromatic rings and a fresh 2D depiction.
Molecule smiles(std::string_view text);
// MOL / SDF, V2000 and V3000 (`record` picks the SDF entry).
Molecule molfile(std::string_view text, int record = 0);
// .smi / .smiles, .mol, .sdf (and .sd), .pdb (every atom, bonds by distance); resolved
// like media::resolve.
Molecule loadMolecule(const std::string& file, int record = 0);

// New 2D coordinates from the bond graph: regular ring systems, zig-zag chains, bond
// length 1 (stress layout).
void depict(Molecule& m);
// Implicit hydrogens as atoms of their own (for 3D models).
Molecule withHydrogens(const Molecule& m);
// Rough 3D geometry from the bond graph: bond lengths from covalent radii, angles from
// hybridisation, flat aromatic rings (no conformer search, no stereo).
void embed3d(Molecule& m);
// Every match of a SMILES pattern (elements, aromaticity and bond orders compared;
// hydrogens ignored), as the matched atoms of `m` in pattern order. "C(=O)O" finds
// carboxyl groups, "c1ccccc1" benzene rings.
std::vector<std::vector<int>> find(const Molecule& m, std::string_view pattern);

// ------------------------------------------------------------------ element data
Color cpkColor(const std::string& element);      // Jmol colours
float covalentRadius(const std::string& element);    // Angstroms
float vdwRadius(const std::string& element);         // Angstroms

// ------------------------------------------------------------------ 2D skeletal formulas
struct Style2D {
    Font font{};                 // id 0: the bundled UI face (bold)
    float bondLength = 64;       // px at most; smaller when the box is tight
    float lineWidth = 3.4f;      // px at bondLength 64 (scales with it)
    float fontSize = 0.5f;       // atom labels, times the bond length
    Color ink{ 30, 34, 48, 255 };
    bool colorAtoms = true;      // heteroatoms (and their bond halves) in CPK colours
    bool carbonLabels = false;   // label every carbon
    bool aromaticCircles = false;    // aromatic rings as a circle inside single bonds
    Color highlight{ 255, 122, 40, 255 };
};

class Depiction {
public:
    explicit Depiction(Molecule m, Style2D style = {});
    static std::shared_ptr<Depiction> make(Molecule m, Style2D style = {});
    Molecule molecule;           // hydrogens folded into their atoms, 2D coordinates
    Style2D style;
    float rotation = 0;          // degrees, about the centre

    // Atoms appear breadth first from the most central atom, `stagger` frames apart;
    // bonds draw on between them.
    Depiction& build(int at, int stagger = 2, int duration = 12);
    // Atoms (indices into `molecule`) and the bonds among them light up and stay lit.
    Depiction& highlight(int at, int duration, std::vector<int> atoms, Color color = BLANK);
    Depiction& unhighlight(int at, int duration);
    // Draws frame t fitted inside `box` (local to ctx; empty = the whole composition).
    void draw(const Ctx& c, int t, Rectangle box = {});
    // Where atom `i` is drawn in `box` (frame-independent).
    Vector2 atomAt(int i, Rectangle box) const;

    struct Step { int type, at, dur; std::vector<int> atoms; Color color; };
private:
    std::vector<Step> steps_;
};
using DepictionPtr = std::shared_ptr<Depiction>;

inline std::function<void(const Ctx&, int)> draw(DepictionPtr d, Rectangle box = {}) {
    return [d, box](const Ctx& c, int t) { d->draw(c, t, box); };
}

// ------------------------------------------------------------------ 3D models
struct Style3D {
    enum class Mode { BallAndStick, SpaceFilling, Sticks };
    Mode mode = Mode::BallAndStick;
    float atomScale = 0.26f;     // ball radius as a fraction of the van der Waals radius
    float bondRadius = 0.14f;    // Angstroms
    bool hydrogens = true;       // show hydrogens (implicit ones are added and placed)
    float roughness = 0.35f;
    float metalness = 0.0f;
};

// Adds `m` as a group centred on the origin: one child per atom ("atom:<i>") and bond
// ("bond:<i>"), ordered breadth first so buildIn grows the molecule outwards. Molecules
// without 3D geometry are embedded first.
g3d::Object3D* addMolecule(g3d::Scene& scene, const Molecule& m, const Style3D& style = {});
g3d::Object3D* addMolecule(g3d::Object3D& parent, const Molecule& m, const Style3D& style = {});

// Grows a model built by addMolecule or addCartoon in child order: child i scales up from
// frame at + i * stagger over `duration` frames (the frame is the scene's local frame).
void buildIn(g3d::Object3D* model, int at, int stagger = 2, int duration = 14);

// ------------------------------------------------------------------ proteins
struct Residue {
    std::string name;            // "GLY"
    int seq = 0;
    char chain = ' ';
    char structure = 'C';        // 'H' helix, 'E' strand, 'C' coil
    int ca = -1, o = -1;         // atom indices
};

struct Protein {
    std::string id, title;
    std::vector<Atom> atoms;         // ATOM and HETATM records of the first model
    std::vector<Residue> residues;   // amino acids with a C-alpha, in file order
    // The atoms as a molecule, bonded by distance (hetero atoms optionally; water never).
    Molecule molecule(bool protein = true, bool hetero = true) const;
    float radius() const;            // furthest C-alpha from their centroid
};

// PDB format: ATOM / HETATM / HELIX / SHEET / TITLE / HEADER, first MODEL, first altLoc.
// Without HELIX / SHEET records helices are assigned from C-alpha geometry.
Protein pdb(std::string_view text);
Protein loadProtein(const std::string& file);

struct CartoonStyle {
    enum class Coloring { Rainbow, Chain, Structure };
    Coloring coloring = Coloring::Rainbow;   // rainbow: blue (N-terminus) to red (C-terminus)
    float coilRadius = 0.3f, helixWidth = 1.6f, strandWidth = 1.8f, thickness = 0.36f;
    int samples = 8;             // spline points per residue
    bool ligands = true;         // hetero groups other than water, as sticks
    float roughness = 0.45f;
};
// The backbone as a cartoon (coils as tubes, helices as ribbons, strands as arrows),
// centred on the C-alpha centroid; one child per residue ("res:<i>") so buildIn draws
// the chain from the N-terminus, then ligands.
g3d::Object3D* addCartoon(g3d::Scene& scene, const Protein& p, const CartoonStyle& style = {});

} // namespace chem
} // namespace rmtn
