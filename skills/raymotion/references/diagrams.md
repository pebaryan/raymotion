# Diagrams, molecules and proteins

## Diagrams (`dg::`, `rmtn/diagram.hpp`)

Load a diagram from its own format, lay it out in-engine (no Graphviz), and script its
animation. Layout runs on the first draw (it needs font metrics), so build `Diagram`s
while registering.

```cpp
auto d = dg::Diagram::make(dg::load("flow.mmd"));          // or dg::mermaid(R"(flowchart LR ...)")
d->build(0, 6, 18);                         // reveal everything in flow order: a node every 6 frames
d->highlight(80, 12, { "Pay" });            // ids or labels; colour defaults to the theme's
d->trace(100, 60, { "Cart", "Pay", "Ship" });   // a pulse runs the path and lights it
d->flow(170, 90, { "Pay" }, 3);             // 3 packets loop along Pay's out-edges (or edge ids)
d->dim(170, 20, { "Pay", "Ship" });         // everything else fades back; {} restores
d->focus(170, 30, { "Pay" }, 2.f);          // camera zooms in (up to 2x); {} zooms back out
d->unhighlight(260, 12);                    // {} = everything
d->morph(270, 40, dg::load("flow-v2.mmd")); // same ids move and restyle; new fade in; gone fade out
d->reveal(330, 12, { "Refund" });           // also hide(at, dur, ids)
addRoot(c.roots, 0, 400)->fn = dg::draw(d, { 80, 60, 1760, 760 });   // box in composition px
```

All times are frames in the drawing node's local time. Each action's `ids` are node,
edge or group ids, or labels. Edges are named `"from->to"`, or by their id from the
file (BPMN flows, DOT/JSON ids). A name that matches nothing throws when the diagram
first draws, so a still catches typos. Elements named in a `build` or `reveal` are
hidden until then; everything else shows from frame 0.

**Formats.** `dg::load(file)` picks the parser by extension, or you can call one directly:

| | extension | function |
|---|---|---|
| Mermaid flowchart / stateDiagram | `.mmd` `.mermaid` | `dg::mermaid(text)` |
| Graphviz DOT | `.dot` `.gv` | `dg::dot(text)` |
| BPMN 2.0 XML (uses its DI coordinates) | `.bpmn` | `dg::bpmn(xml)` |
| RDF Turtle / N-Triples | `.ttl` `.nt` | `dg::turtle(text, RdfOptions)` |
| JSON-LD | `.jsonld` | `dg::jsonld(Json)` |
| JSON graph or Cytoscape elements | `.json` | `dg::json(Json)` |
| Newick tree | `.nwk` `.newick` `.tre` | `dg::newick(text)` |

`dg::RdfOptions`: `literalNodes` (literal values as nodes, else in `Node::data`),
`typeEdges` (draw rdf:type as edges), `maxLiteralLength`, `language`.

**Building a graph in code.** `dg::Graph g; g.addNode("a", "Start", dg::Shape::Stadium); g.addEdge("a", "b", "next");`
then use `g.addGroup("grp", "Group label")` and set `node->group = "grp"`. `g.direction = dg::Direction::LR`
(note the `dg::` prefix: plain `Direction` is the transitions enum).

**Themes and layout.** `dg::Diagram::make(graph, dg::Theme::dark())`: `light()`,
`dark()`, `blueprint()`, `bpmn()`. Fields include `font`, `bold`, `fontSize`,
`maxLabelWidth`, `fill`, `stroke`, `text`, `edge`, `highlight`, `palette` (fills for node
kinds) and `colorKinds`. The layout is chosen automatically (layered for directed
graphs and flowcharts, force for RDF and undirected graphs, tree for Newick). To
override, pass `dg::LayoutOptions` with `kind` (`Layered`, `Force`, `Tree`, `Radial`,
`Circle`, `None`), `direction`, `nodeSpacing`, `rankSpacing` or `edgeLength` as the third
argument of `make`. `d->maxZoom` caps how far a small diagram is scaled up to fill its box.

**Tips.**
- The diagram is fitted to the box, so a wide flowchart in a tall box comes out small.
  Prefer LR for wide boxes and TB for tall ones.
- Long labels wrap at `theme.maxLabelWidth`.
- `focus` zooms beyond the box (drawing is not clipped). Put the node in a layer with a
  mask to crop it, or keep the zoom modest.
- `d->nodeRect(id, t, box)` gives a node's rectangle at frame t, in the same
  coordinates as the box. Use it to place your own callouts next to a node.

## Molecules (`chem::`, `rmtn/chem.hpp`)

```cpp
chem::Molecule m = chem::smiles("CN1C=NC2=C1C(=O)N(C(=O)N2C)C caffeine");  // text after a space is the name
chem::Molecule f = chem::loadMolecule("caffeine.sdf");      // .smi .mol .sdf .pdb
m.formula();  m.mass();  m.rings();                         // "C8H10N4O2", 194.19, ring atom lists
auto hits = chem::find(m, "C=O");                           // substructure matches (atom index lists)

chem::Style2D st; st.bondLength = 90;                       // px at most; also ink, colorAtoms, carbonLabels, aromaticCircles, highlight
auto mol = chem::Depiction::make(m, st);
mol->rotation = -15;                                        // degrees
mol->build(0, 3, 12);                                       // atoms appear outwards, bonds draw on
mol->highlight(60, 16, hits[0], ORANGE);                    // also unhighlight(at, dur)
addRoot(c.roots, 0, 180)->fn = chem::draw(mol, { 80, 60, 800, 700 });
Vector2 p = mol->atomAt(3, { 80, 60, 800, 700 });          // where atom 3 is drawn, for labels
```

3D (inside a `g3d::Scene`, see `3d.md`):

```cpp
chem::Style3D s3; s3.mode = chem::Style3D::Mode::BallAndStick;   // SpaceFilling, Sticks; hydrogens, atomScale, bondRadius
g3d::Object3D* model = chem::addMolecule(*scene, chem::loadMolecule("caffeine.sdf"), s3);
chem::buildIn(model, 20, 2, 12);            // child i grows in at frame 20 + 2 i over 12 frames
model->up = [](int t, g3d::Object3D& o) { o.rotation = { 15, t * 0.9f, 0 }; };
scene->camera.position = { 0, 0, 15 };      // SMILES without 3D coordinates get a rough embedding
```

Coordinates are Angstroms, centred on the origin. A small drug-sized molecule fits at a
camera distance of about 15 (fov 45).

## Proteins

```cpp
chem::Protein p = chem::loadProtein("1crn.pdb");            // ATOM/HETATM, HELIX/SHEET, first model
chem::CartoonStyle cs;                                      // coloring Rainbow | Chain | Structure; widths; ligands
g3d::Object3D* cart = chem::addCartoon(*scene, p, cs);      // helices as ribbons, strands as arrows
chem::buildIn(cart, 10, 2, 10);                             // grows from the N-terminus, residue by residue
scene->camera.position = { 0, 0, 2.5f * p.radius() + 8 };
```

`p.residues` lists residues with their secondary structure (`'H'`, `'E'` or `'C'`), and
`p.molecule()` gives every atom as a `Molecule` (for ball-and-stick). Download
structures from https://www.rcsb.org (`https://files.rcsb.org/download/<ID>.pdb`) and
small molecules from PubChem (SDF, 3D conformer).
