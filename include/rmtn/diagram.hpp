// rmtn/diagram.hpp -- diagrams and graphs on the timeline: flowcharts and state machines
// (Mermaid, Graphviz DOT), BPMN 2.0 processes, knowledge graphs (Turtle / N-Triples,
// JSON-LD, JSON) and trees (Newick), laid out in-engine and animated element by element.
//
//     auto d = dg::Diagram::make(dg::load("order.mmd"));     // or dg::mermaid(R"(graph LR ...)")
//     d->build(0);                                            // reveal in flow order
//     d->trace(90, 60, { "cart", "pay", "ship" });           // a pulse runs the path, lighting it
//     d->focus(160, 30, { "pay" });                           // the camera moves in
//     d->morph(220, 40, dg::load("order-v2.mmd"));            // glide to a new version
//     addRoot(c.roots, 0, 300)->fn = dg::draw(d, { 80, 80, 1760, 920 });
//
// A Graph is plain data (nodes, edges, groups) in diagram units, which are composition px
// at zoom 1; parsers fill it and layouts position it. A Diagram owns one or more graph
// states, a theme and a script of timed actions, and draws any frame on its own. Node
// sizes come from the theme's font metrics, so layout runs on the first draw (or the
// first geometry query), once a window exists.
//
// Element ids name nodes, groups and edges. An edge is named by its id, which defaults to
// "from->to" ("from->to#2" for a second one); "a->b" also finds an edge stored as b->a.
// A name that matches no id is tried as a node or group label.
#pragma once
#include <raylib.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "json.hpp"
#include "timeline.hpp"

namespace rmtn {
namespace dg {

// ------------------------------------------------------------------ graph model
enum class Shape {
    Rect, Rounded, Stadium, Circle, DoubleCircle, Ellipse, Diamond, Hexagon,
    Parallelogram, ParallelogramAlt, Trapezoid, TrapezoidAlt, Cylinder, Subroutine,
    Flag, Note, Document, Point, Text,
    StartState, EndState,                                    // state diagrams: filled dot, bullseye
    Task, Event, Gateway, DataObject, DataStore, Annotation,  // BPMN (see Node::marker)
};
enum class LabelPlace { Inside, Below, Above, Right, Left };
enum class Arrow { None, Normal, Open, Circle, OpenCircle, Cross, Diamond, OpenDiamond };
enum class Stroke { Solid, Dashed, Dotted };
enum class Direction { TB, BT, LR, RL };
enum class LayoutKind {
    Auto,      // the graph's own preference, else Layered when directed, Force when not
    Layered,   // ranks along the direction (flowcharts, processes, dependency graphs)
    Force,     // stress-minimised network (knowledge graphs, undirected graphs)
    Tree,      // tidy tree along the direction, branch lengths when the source has them
    Radial,    // tree around its root
    Circle,    // nodes on a circle in order
    None,      // positions as given (BPMN diagram interchange, JSON with x/y)
};

// Per-element overrides of the theme; unset fields use it.
struct Style {
    std::optional<Color> fill, stroke, text;
    std::optional<float> strokeWidth;
    std::optional<Stroke> dash;
};

struct Node {
    std::string id, label;
    Shape shape = Shape::Rounded;
    std::string group;               // owning group id ("" = top level)
    std::string kind;                // classifier: RDF class, BPMN element, Mermaid class
    // BPMN icon. Events: message, timer, error, signal, terminate, conditional, escalation,
    // link, compensation, cancel (and "throw" / "nonInterrupting" flags in `kind`).
    // Gateways: exclusive, parallel, inclusive, event, complex. Tasks: user, service,
    // script, send, receive, manual, businessRule, subProcess, call.
    std::string marker;
    Style style;
    LabelPlace labelPlace = LabelPlace::Inside;
    float labelAngle = 0;            // degrees (radial trees)
    Vector2 pos{ 0, 0 };             // centre
    Vector2 size{ 0, 0 };            // 0: sized from the label and shape
    bool fixed = false;              // pos and size come from the source; layouts keep them
    Json data;                       // source attributes (RDF literals, DOT attributes, ...)
    std::vector<std::string> labelLines;   // set by sizeNodes: the wrapped label...
    Vector2 labelSize{ 0, 0 };             // ...and its size
};

struct Edge {
    std::string id, from, to, label;
    Arrow head = Arrow::Normal, tail = Arrow::None;
    Stroke stroke = Stroke::Solid;
    float width = 1;                 // multiplier on the theme's edge width
    Style style;
    std::string kind;                // e.g. BPMN sequenceFlow / messageFlow / association
    int minLength = 1;               // layered layout: ranks spanned at least
    std::vector<Vector2> points;     // route from the source's boundary to the target's
    bool curved = true;              // smooth curve through `points` (false: straight segments)
    std::optional<Vector2> labelPos; // centre of the label (default: halfway along)
    bool fixed = false;              // route comes from the source
    std::vector<std::string> labelLines;   // set by sizeNodes
    Vector2 labelSize{ 0, 0 };
};

struct Group {
    enum class Kind { Cluster, Pool, Lane };
    std::string id, label, parent;
    Kind kind = Kind::Cluster;
    bool horizontal = true;          // pools and lanes: label band on the left (else on top)
    Style style;
    Rectangle rect{};                // from layout (bounds of its members) or the source
    bool fixed = false;
    Vector2 labelSize{ 0, 0 };       // set by sizeNodes
};

struct Graph {
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<Group> groups;
    Direction direction = Direction::TB;
    bool directed = true;
    LayoutKind layout = LayoutKind::Auto;     // the source's preference
    std::string title;

    Node* node(std::string_view id);
    const Node* node(std::string_view id) const;
    Edge* edge(std::string_view id);
    const Edge* edge(std::string_view id) const;
    Group* group(std::string_view id);
    const Group* group(std::string_view id) const;
    // The node `id`, created (labelled with its id) when absent.
    Node& addNode(const std::string& id, const std::string& label = "", Shape shape = Shape::Rounded);
    // Adds an edge, creating missing endpoints; its id is "from->to" (#2, #3 for repeats).
    Edge& addEdge(const std::string& from, const std::string& to, const std::string& label = "");
    Group& addGroup(const std::string& id, const std::string& label = "", const std::string& parent = "");
    // Whether node `nodeId` is inside `group`, directly or through nested groups.
    bool inGroup(std::string_view nodeId, std::string_view group) const;
    Rectangle bounds() const;        // nodes, routes, labels and groups
};

// ------------------------------------------------------------------ parsers
// All throw std::runtime_error with a line number on malformed input.

// Mermaid `flowchart` / `graph` (node shapes, edge styles and labels, chains and `&`,
// subgraphs, classDef / class / style / linkStyle) and `stateDiagram` (states, [*],
// transitions with labels, composite states, notes are skipped).
Graph mermaid(std::string_view text);
// Graphviz DOT: graph / digraph / strict, node and edge statements and attribute lists,
// default `node` / `edge` / `graph` attributes, subgraphs and clusters, rankdir, shape,
// label, color, fillcolor, fontcolor, style (dashed, dotted, bold, filled, rounded),
// arrowhead / arrowtail / dir, pos (with neato -n style fixed positions).
Graph dot(std::string_view text);
// BPMN 2.0 XML: processes, collaborations with pools, lanes and message flows, every flow
// node (events with their definitions, tasks by type, sub-processes, call activities,
// gateways), data objects and stores, text annotations, sequence flows and associations.
// Uses the file's diagram interchange (BPMNDI) layout when present, else lays out LR.
Graph bpmn(std::string_view xml);

struct RdfOptions {
    bool literalNodes = true;        // literal values as their own nodes (else kept in Node::data)
    bool typeEdges = false;          // rdf:type as edges to class nodes (else Node::kind only)
    int maxLiteralLength = 48;       // longer literals are cut with an ellipsis
    std::string language = "en";     // preferred label language
};
// Turtle (prefixes, base, `a`, `;` and `,` lists, blank nodes and [ ] property lists,
// collections, typed and language-tagged literals) and N-Triples. Labels come from
// rdfs:label, skos:prefLabel, foaf:name, schema:name, dc:title, else the IRI's local name.
Graph turtle(std::string_view text, const RdfOptions& options = {});
// JSON-LD (@context prefixes and @vocab, @id, @type, @graph, nested and referenced
// objects, @value / @language literals, arrays).
Graph jsonld(const Json& doc, const RdfOptions& options = {});
// Plain JSON graphs: {"nodes": [...], "edges"|"links": [...]} with id / label / group /
// shape / color / x / y and from|source / to|target / label, or Cytoscape's
// {"elements": [{"data": {...}}]}.
Graph json(const Json& doc);
// Newick trees "(A:0.1,B:0.2,(C:0.3,D:0.4)E:0.5)F;": tips are labels, inner nodes dots,
// branch lengths kept for the tree layout (Edge `data` is unused; lengths live in
// Node::data["length"]).
Graph newick(std::string_view text);

// `format`: mermaid, dot, bpmn, turtle, ntriples, jsonld, json, newick.
Graph parse(std::string_view text, std::string_view format);
// Reads `file` (resolved like media::resolve) by its extension: .mmd .mermaid, .dot .gv,
// .bpmn, .ttl .nt, .jsonld, .json (JSON-LD when it has @context / @id / @graph), .nwk
// .newick .tre; other extensions are sniffed from the content.
Graph load(const std::string& file);

// ------------------------------------------------------------------ theme and layout
struct Theme {
    Font font{}, bold{};             // id 0: the bundled UI face (fonts::ui)
    float fontSize = 24, lineHeight = 1.2f, maxLabelWidth = 260;
    float padX = 22, padY = 14;      // label to node border
    float strokeWidth = 2.5f, edgeWidth = 2.5f, corner = 10, arrowSize = 14;
    Color fill{ 255, 255, 255, 255 }, stroke{ 52, 58, 78, 255 }, text{ 30, 34, 48, 255 };
    Color edge{ 92, 98, 120, 255 }, edgeText{ 70, 76, 96, 255 }, edgeLabelFill{ 247, 248, 252, 235 };
    Color groupFill{ 236, 240, 248, 255 }, groupStroke{ 176, 186, 210, 255 }, groupText{ 82, 92, 120, 255 };
    Color literalFill{ 250, 246, 230, 255 };
    Color highlight{ 255, 122, 40, 255 };
    // Nodes with a `kind` (RDF classes, BPMN element types when coloured) take these fills
    // in order of first appearance; the stroke is the fill darkened.
    std::vector<Color> palette;
    bool colorKinds = true;          // false: every node uses `fill`

    static Theme light();
    static Theme dark();
    static Theme blueprint();
    static Theme bpmn();             // bpmn.io's look: white shapes, black strokes, no kind colours
};

struct LayoutOptions {
    LayoutKind kind = LayoutKind::Auto;
    std::optional<Direction> direction;  // default: the graph's
    float nodeSpacing = 46;          // between neighbours in a rank
    float rankSpacing = 72;          // between ranks (layered, tree)
    float edgeLength = 170;          // force: ideal distance between linked nodes
    bool branchLengths = true;       // tree: use Newick branch lengths when present
    int iterations = 300;            // force
};

// Sizes nodes whose size is 0 from their labels (needs the window for font metrics).
void sizeNodes(Graph& g, const Theme& theme);
// Positions nodes, routes edges and fits groups. Node sizes must be set; fixed nodes,
// routes and groups are kept. `hint` (optional) seeds force layouts so a new version of
// a graph keeps the old positions where ids match.
void layout(Graph& g, const LayoutOptions& options = {}, const Graph* hint = nullptr);

// ------------------------------------------------------------------ animation
// A diagram on the timeline. Frames are the drawing node's local frames; durations are
// frames; `color` BLANK means the theme's highlight. Elements that are revealed (build,
// reveal) are hidden before their reveal; everything else is visible from frame 0.
class Diagram {
public:
    explicit Diagram(Graph g, Theme theme = Theme::light(), LayoutOptions layout = {});
    ~Diagram();
    static std::shared_ptr<Diagram> make(Graph g, Theme theme = Theme::light(), LayoutOptions layout = {});

    Theme theme;
    LayoutOptions layoutOptions;
    float maxZoom = 1.25f;           // fitting the whole diagram never scales it up past this
    float margin = 40;               // diagram units kept around the fitted content

    // Reveals every node, edge and group in flow order (breadth first from the sources):
    // a node every `stagger` frames, each edge drawn on from its source as it appears.
    Diagram& build(int at, int stagger = 6, int duration = 18);
    Diagram& reveal(int at, int duration, std::vector<std::string> ids);   // nodes pop, edges draw on
    Diagram& hide(int at, int duration, std::vector<std::string> ids);
    Diagram& highlight(int at, int duration, std::vector<std::string> ids, Color color = BLANK);
    Diagram& unhighlight(int at, int duration, std::vector<std::string> ids = {});    // {}: all
    // Fades everything but `keep` (and the edges between kept nodes) to the background;
    // `keep` {} brings everything back.
    Diagram& dim(int at, int duration, std::vector<std::string> keep);
    // A glowing pulse travels the edges between consecutive nodes of `path`, lighting each
    // node and edge as it passes; they stay lit until unhighlight.
    Diagram& trace(int at, int duration, std::vector<std::string> path, Color color = BLANK);
    // `count` packets run along each of `edges` (ids, or node ids for all their out-edges),
    // spaced evenly, without lighting anything: data flowing.
    Diagram& flow(int at, int duration, std::vector<std::string> edges, int count = 3, Color color = BLANK);
    // Moves the camera to frame `ids` (nodes, groups or edges), zooming in up to `zoom`;
    // {} returns to the whole diagram. Drawing is not clipped to the box: zoomed content
    // runs past it (put the node in a Layer with a mask to crop).
    Diagram& focus(int at, int duration, std::vector<std::string> ids, float zoom = 2.f);
    // Glides to another version of the graph: nodes and edges with the same id move and
    // restyle, new ones fade in, missing ones fade out. Laid out like the first.
    Diagram& morph(int at, int duration, Graph next);

    // Geometry at frame t, in the drawing box's coordinates (lays out on first use).
    Rectangle nodeRect(const std::string& id, int t, Rectangle box);
    const Graph& graph(int state = 0);       // a state laid out
    int states() const;

    // Draws frame t inside `box` (local to ctx; empty = the whole composition).
    void draw(const Ctx& c, int t, Rectangle box = {});

    struct Action;                   // implementation details
    struct Impl;
private:
    void prepare();              // sizes, lays out and resolves the script once
    std::unique_ptr<Impl> impl_;
};
using DiagramPtr = std::shared_ptr<Diagram>;

// A TNode::fn drawing `d` in `box` (empty = the whole composition).
inline std::function<void(const Ctx&, int)> draw(DiagramPtr d, Rectangle box = {}) {
    return [d, box](const Ctx& c, int t) { d->draw(c, t, box); };
}

// ------------------------------------------------------------------ helpers shared with chem
namespace detail {
// Stress majorization (SMACOF) in `dim` (2 or 3) dimensions: positions `x` (n*dim, used as
// the start when `init`, else from classical MDS) towards target distances `d` (n*n,
// < 0 = no constraint) with weights d^-2 (times `w` when given).
void stress(std::vector<double>& x, int n, int dim, const std::vector<double>& d, int iterations,
            bool init, const std::vector<double>* w = nullptr);
// Path through `pts` sampled as a smooth curve (Catmull-Rom) or polyline.
std::vector<Vector2> samplePath(const std::vector<Vector2>& pts, bool curved);
}

} // namespace dg
} // namespace rmtn
