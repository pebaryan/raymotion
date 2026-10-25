// diagram_tests.cpp -- diagram and structure parsing, layout and chemistry, without a
// window: Mermaid, DOT, BPMN, Turtle, JSON-LD, JSON, Newick; layered / force / tree
// layouts; SMILES, MOL / SDF, PDB, rings, formulas, substructure search, depiction.
#include "rmtn/diagram.hpp"
#include "rmtn/chem.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

using namespace rmtn;

static bool ok = true;
static void expect(bool cond, const char* what) {
    if (!cond) { std::fprintf(stderr, "FAIL: %s\n", what); ok = false; }
}
template <class F> static void throws(F f, const char* what) {
    try { f(); } catch (const std::exception&) { return; }
    std::fprintf(stderr, "FAIL: no error for %s\n", what);
    ok = false;
}

// Sizes every node like a short label would, so layouts run without fonts.
static void sized(dg::Graph& g, float w = 120, float h = 50) {
    for (dg::Node& n : g.nodes) if (n.size.x <= 0) n.size = { w, h };
    for (dg::Edge& e : g.edges) if (!e.label.empty()) e.labelSize = { 60, 24 };
    for (dg::Group& gr : g.groups) gr.labelSize = { 80, 22 };
}
static bool overlap(const dg::Node& a, const dg::Node& b) {
    return std::fabs(a.pos.x - b.pos.x) < (a.size.x + b.size.x) / 2 - 0.5f && std::fabs(a.pos.y - b.pos.y) < (a.size.y + b.size.y) / 2 - 0.5f;
}
static bool anyOverlap(const dg::Graph& g) {
    for (size_t i = 0; i < g.nodes.size(); i++)
        for (size_t j = i + 1; j < g.nodes.size(); j++) if (overlap(g.nodes[i], g.nodes[j])) return true;
    return false;
}
static bool inside(Rectangle r, const dg::Node& n) {
    return n.pos.x - n.size.x / 2 >= r.x - 0.01f && n.pos.x + n.size.x / 2 <= r.x + r.width + 0.01f &&
           n.pos.y - n.size.y / 2 >= r.y - 0.01f && n.pos.y + n.size.y / 2 <= r.y + r.height + 0.01f;
}

static void mermaidTests() {
    dg::Graph g = dg::mermaid(R"(
        %% an order flow
        flowchart LR
          A([Start]) --> B{In stock?}
          B -- yes --> C[Ship it]
          B -->|no| D[(Backorder)]
          C & D --> E((Done))
          E -.-> F>Survey] ==> G[[Archive]]
          A --- H{{Hex}} --o I[/In/] --x J[\Out\]
          A ---> K[/Trap\]
          subgraph warehouse [Warehouse]
            C
            D
          end
          classDef hot fill:#f96,stroke:#333,color:#fff
          class C hot
          style D fill:#ccf
          linkStyle 0 stroke:#f00
    )");
    expect(g.direction == dg::Direction::LR, "mermaid direction");
    expect(g.nodes.size() == 11, "mermaid node count");
    expect(g.node("A") && g.node("A")->shape == dg::Shape::Stadium && g.node("A")->label == "Start", "mermaid stadium");
    expect(g.node("B")->shape == dg::Shape::Diamond, "mermaid diamond");
    expect(g.node("D")->shape == dg::Shape::Cylinder, "mermaid cylinder");
    expect(g.node("E")->shape == dg::Shape::Circle, "mermaid circle");
    expect(g.node("F")->shape == dg::Shape::Flag, "mermaid flag");
    expect(g.node("G")->shape == dg::Shape::Subroutine, "mermaid subroutine");
    expect(g.node("H")->shape == dg::Shape::Hexagon, "mermaid hexagon");
    expect(g.node("I")->shape == dg::Shape::Parallelogram, "mermaid parallelogram");
    expect(g.node("J")->shape == dg::Shape::ParallelogramAlt, "mermaid alt parallelogram");
    expect(g.node("K")->shape == dg::Shape::Trapezoid, "mermaid trapezoid");
    const dg::Edge* yes = g.edge("B->C");
    expect(yes && yes->label == "yes", "mermaid inline edge label");
    const dg::Edge* no = g.edge("B->D");
    expect(no && no->label == "no", "mermaid pipe edge label");
    expect(g.edge("C->E") && g.edge("D->E"), "mermaid & fan-in");
    const dg::Edge* dotted = g.edge("E->F");
    expect(dotted && dotted->stroke == dg::Stroke::Dotted, "mermaid dotted link");
    const dg::Edge* thick = g.edge("F->G");
    expect(thick && thick->width == 2, "mermaid thick link");
    const dg::Edge* open = g.edge("A->H");
    expect(open && open->head == dg::Arrow::None, "mermaid open link");
    expect(g.edge("H->I") && g.edge("H->I")->head == dg::Arrow::Circle, "mermaid circle head");
    expect(g.edge("I->J") && g.edge("I->J")->head == dg::Arrow::Cross, "mermaid cross head");
    expect(g.edge("A->K") && g.edge("A->K")->minLength == 2, "mermaid long link");
    expect(g.group("warehouse") && g.group("warehouse")->label == "Warehouse", "mermaid subgraph");
    expect(g.node("C")->group == "warehouse" && g.node("D")->group == "warehouse", "mermaid subgraph members");
    expect(g.node("C")->style.fill && g.node("C")->style.fill->r == 0xff && g.node("C")->style.text, "mermaid classDef");
    expect(g.node("D")->style.fill && g.node("D")->style.fill->b == 0xff, "mermaid style");
    expect(g.edges[0].style.stroke && g.edges[0].style.stroke->r == 255, "mermaid linkStyle");

    dg::Graph chain = dg::mermaid("graph TD; a-->b-->c; a-->c");
    expect(chain.nodes.size() == 3 && chain.edges.size() == 3, "mermaid chain with semicolons");
    expect(chain.direction == dg::Direction::TB, "mermaid TD");

    dg::Graph st = dg::mermaid(R"(
        stateDiagram-v2
          [*] --> Idle
          Idle --> Running : start
          Running --> Idle : stop
          state Running {
            [*] --> Warm
            Warm --> Hot
          }
          Running --> [*]
    )");
    expect(st.node("[*]start") && st.node("[*]start")->shape == dg::Shape::StartState, "state start");
    expect(st.node("[*]end") && st.node("[*]end")->shape == dg::Shape::EndState, "state end");
    const dg::Edge* start = st.edge("Idle->Running");
    expect(start && start->to == "Running/[*]start" && start->label == "start", "state transition into a composite enters its start");
    const dg::Edge* stop = st.edge("Running->Idle");
    expect(stop && stop->from == "Hot" && !st.node("Running"), "state transition out of a composite leaves its last state");
    expect(st.group("Running") && st.node("Running/[*]start") && st.node("Running/[*]start")->group == "Running", "state composite");
    expect(st.node("Warm") && st.node("Warm")->group == "Running", "state composite member");

    throws([] { dg::mermaid("sequenceDiagram\n A->>B: hi"); }, "unsupported mermaid diagram");
    throws([] { dg::mermaid("graph TD\n A --> "); }, "dangling mermaid link");
}

static void dotTests() {
    dg::Graph g = dg::dot(R"(
        // a pipeline
        digraph pipeline {
          rankdir=LR; label="Build";
          node [shape=box style="rounded,filled" fillcolor="#e0f0ff"];
          edge [color=gray];
          fetch -> build -> test [label="ok"];
          test -> deploy [style=dashed, arrowhead=vee];
          subgraph cluster_ci { label="CI"; build; test; }
          "a b" -> fetch [dir=back];
          db [shape=cylinder label="Data\nstore"];
          deploy -> db;
          { rank=same; x; y }
        }
    )");
    expect(g.directed && g.direction == dg::Direction::LR && g.title == "Build", "dot graph attributes");
    expect(g.node("fetch") && g.node("fetch")->shape == dg::Shape::Rounded, "dot default node shape");
    expect(g.node("fetch")->style.fill && g.node("fetch")->style.fill->b == 0xff, "dot fillcolor");
    expect(g.edge("build->test") && g.edge("build->test")->label == "ok", "dot chain label");
    expect(g.edge("fetch->build") && g.edge("fetch->build")->label == "ok", "dot chain attributes apply to each edge");
    const dg::Edge* dep = g.edge("test->deploy");
    expect(dep && dep->stroke == dg::Stroke::Dashed && dep->head == dg::Arrow::Open, "dot edge style");
    expect(g.edge("a b->fetch") && g.edge("a b->fetch")->head == dg::Arrow::None && g.edge("a b->fetch")->tail == dg::Arrow::Normal, "dot dir=back");
    expect(g.group("cluster_ci") && g.group("cluster_ci")->label == "CI", "dot cluster");
    expect(g.node("build")->group == "cluster_ci", "dot cluster member");
    expect(g.node("db")->shape == dg::Shape::Cylinder && g.node("db")->label == "Data\nstore", "dot label escapes");
    expect(g.node("x") && g.node("y"), "dot anonymous subgraph");
    dg::Graph u = dg::dot("graph { a -- b -- c; c -- a }");
    expect(!u.directed && u.edges.size() == 3 && u.edges[0].head == dg::Arrow::None, "dot undirected");
    dg::Graph pos = dg::dot("digraph { a [pos=\"0,0!\"]; b [pos=\"100,50!\"]; a -> b }");
    expect(pos.layout == dg::LayoutKind::None && std::fabs(pos.node("b")->pos.y + 50) < 0.01f, "dot fixed positions");
    throws([] { dg::dot("digraph { a -> }"); }, "dot dangling edge");
}

static const char* BPMN = R"(<?xml version="1.0" encoding="UTF-8"?>
<bpmn:definitions xmlns:bpmn="http://www.omg.org/spec/BPMN/20100524/MODEL" xmlns:bpmndi="http://www.omg.org/spec/BPMN/20100524/DI"
    xmlns:dc="http://www.omg.org/spec/DD/20100524/DC" xmlns:di="http://www.omg.org/spec/DD/20100524/DI" id="defs">
  <bpmn:collaboration id="collab">
    <bpmn:participant id="shop" name="Shop" processRef="p1"/>
    <bpmn:participant id="client" name="Customer"/>
    <bpmn:messageFlow id="m1" sourceRef="client" targetRef="start"/>
  </bpmn:collaboration>
  <bpmn:process id="p1">
    <bpmn:laneSet><bpmn:lane id="sales" name="Sales"><bpmn:flowNodeRef>start</bpmn:flowNodeRef><bpmn:flowNodeRef>check</bpmn:flowNodeRef></bpmn:lane>
      <bpmn:lane id="ops" name="Ops"><bpmn:flowNodeRef>gw</bpmn:flowNodeRef><bpmn:flowNodeRef>ship</bpmn:flowNodeRef><bpmn:flowNodeRef>end</bpmn:flowNodeRef></bpmn:lane></bpmn:laneSet>
    <bpmn:startEvent id="start" name="Order in"><bpmn:messageEventDefinition/></bpmn:startEvent>
    <bpmn:userTask id="check" name="Check &amp; confirm"/>
    <bpmn:exclusiveGateway id="gw" name="OK?" default="f3"/>
    <bpmn:serviceTask id="ship" name="Ship"/>
    <bpmn:endEvent id="end"><bpmn:terminateEventDefinition/></bpmn:endEvent>
    <bpmn:sequenceFlow id="f1" sourceRef="start" targetRef="check"/>
    <bpmn:sequenceFlow id="f2" sourceRef="check" targetRef="gw"/>
    <bpmn:sequenceFlow id="f3" name="yes" sourceRef="gw" targetRef="ship"/>
    <bpmn:sequenceFlow id="f4" sourceRef="ship" targetRef="end"/>
  </bpmn:process>
  <bpmndi:BPMNDiagram id="d"><bpmndi:BPMNPlane id="pl" bpmnElement="collab">
    <bpmndi:BPMNShape id="s_shop" bpmnElement="shop" isHorizontal="true"><dc:Bounds x="100" y="100" width="700" height="300"/></bpmndi:BPMNShape>
    <bpmndi:BPMNShape id="s_sales" bpmnElement="sales" isHorizontal="true"><dc:Bounds x="130" y="100" width="670" height="150"/></bpmndi:BPMNShape>
    <bpmndi:BPMNShape id="s_ops" bpmnElement="ops" isHorizontal="true"><dc:Bounds x="130" y="250" width="670" height="150"/></bpmndi:BPMNShape>
    <bpmndi:BPMNShape id="s_client" bpmnElement="client" isHorizontal="true"><dc:Bounds x="100" y="440" width="700" height="80"/></bpmndi:BPMNShape>
    <bpmndi:BPMNShape id="s1" bpmnElement="start"><dc:Bounds x="180" y="157" width="36" height="36"/><bpmndi:BPMNLabel><dc:Bounds x="170" y="200" width="56" height="14"/></bpmndi:BPMNLabel></bpmndi:BPMNShape>
    <bpmndi:BPMNShape id="s2" bpmnElement="check"><dc:Bounds x="270" y="135" width="100" height="80"/></bpmndi:BPMNShape>
    <bpmndi:BPMNShape id="s3" bpmnElement="gw"><dc:Bounds x="420" y="300" width="50" height="50"/></bpmndi:BPMNShape>
    <bpmndi:BPMNShape id="s4" bpmnElement="ship"><dc:Bounds x="520" y="285" width="100" height="80"/></bpmndi:BPMNShape>
    <bpmndi:BPMNShape id="s5" bpmnElement="end"><dc:Bounds x="680" y="307" width="36" height="36"/></bpmndi:BPMNShape>
    <bpmndi:BPMNEdge id="e1" bpmnElement="f1"><di:waypoint x="216" y="175"/><di:waypoint x="270" y="175"/></bpmndi:BPMNEdge>
    <bpmndi:BPMNEdge id="e2" bpmnElement="f2"><di:waypoint x="370" y="175"/><di:waypoint x="445" y="175"/><di:waypoint x="445" y="300"/></bpmndi:BPMNEdge>
    <bpmndi:BPMNEdge id="e3" bpmnElement="f3"><di:waypoint x="470" y="325"/><di:waypoint x="520" y="325"/><bpmndi:BPMNLabel><dc:Bounds x="480" y="305" width="20" height="14"/></bpmndi:BPMNLabel></bpmndi:BPMNEdge>
    <bpmndi:BPMNEdge id="e4" bpmnElement="f4"><di:waypoint x="620" y="325"/><di:waypoint x="680" y="325"/></bpmndi:BPMNEdge>
    <bpmndi:BPMNEdge id="e5" bpmnElement="m1"><di:waypoint x="198" y="440"/><di:waypoint x="198" y="193"/></bpmndi:BPMNEdge>
  </bpmndi:BPMNPlane></bpmndi:BPMNDiagram>
</bpmn:definitions>)";

static void bpmnTests() {
    dg::Graph g = dg::bpmn(BPMN);
    expect(g.layout == dg::LayoutKind::None, "bpmn uses its diagram interchange");
    expect(g.nodes.size() == 5, "bpmn node count");
    const dg::Node* s = g.node("start");
    expect(s && s->shape == dg::Shape::Event && s->marker == "message" && s->label == "Order in", "bpmn message start event");
    expect(s && s->fixed && std::fabs(s->pos.x - 198) < 0.01f && std::fabs(s->size.x - 36) < 0.01f, "bpmn shape bounds");
    expect(g.node("check")->shape == dg::Shape::Task && g.node("check")->marker == "user" && g.node("check")->label == "Check & confirm", "bpmn user task");
    expect(g.node("gw")->shape == dg::Shape::Gateway && g.node("gw")->marker == "exclusive", "bpmn gateway");
    expect(g.node("ship")->marker == "service", "bpmn service task");
    expect(g.node("end")->marker == "terminate" && g.node("end")->kind.find("throw") != std::string::npos, "bpmn terminate end");
    expect(g.node("start")->group == "sales" && g.node("ship")->group == "ops", "bpmn lanes");
    expect(g.group("shop") && g.group("shop")->kind == dg::Group::Kind::Pool && g.group("sales")->parent == "shop", "bpmn pool and lane nesting");
    expect(g.group("client") && g.group("client")->rect.height == 80, "bpmn black-box pool");
    const dg::Edge* f2 = g.edge("f2");
    expect(f2 && f2->points.size() == 3 && !f2->curved && f2->fixed, "bpmn waypoints");
    expect(g.edge("f3") && g.edge("f3")->labelPos && g.edge("f3")->tail == dg::Arrow::Cross, "bpmn default flow and label");
    const dg::Edge* m1 = g.edge("m1");
    expect(m1 && m1->stroke == dg::Stroke::Dashed && m1->tail == dg::Arrow::OpenCircle, "bpmn message flow");
    // Without diagram interchange it lays out left to right.
    std::string bare = BPMN;
    bare = bare.substr(0, bare.find("<bpmndi:BPMNDiagram")) + "</bpmn:definitions>";
    dg::Graph lay = dg::bpmn(bare);
    expect(lay.layout == dg::LayoutKind::Layered && lay.direction == dg::Direction::LR && lay.edges.size() == 4, "bpmn without coordinates");
    throws([] { dg::bpmn("<bpmn:definitions><bpmn:process></bpmn:definitions>"); }, "bpmn mismatched tags");
}

static void rdfTests() {
    dg::Graph g = dg::turtle(R"(
        @prefix ex: <http://example.org/> .
        @prefix foaf: <http://xmlns.com/foaf/0.1/> .
        @prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .
        ex:alice a foaf:Person ; foaf:name "Alice"@en ; foaf:knows ex:bob , ex:carol ;
                 foaf:age 42 .
        ex:bob a foaf:Person ; rdfs:label "Bob" .
        ex:carol foaf:knows [ a foaf:Person ; foaf:name "Dan" ] .
        <http://example.org/carol> ex:likes ( ex:tea ex:cake ) .
    )");
    const dg::Node* alice = g.node("http://example.org/alice");
    expect(alice && alice->label == "Alice" && alice->kind == "Person", "turtle label and type");
    expect(g.node("http://example.org/bob") && g.node("http://example.org/bob")->label == "Bob", "turtle rdfs:label");
    int knows = 0, literals = 0, list = 0;
    for (const dg::Edge& e : g.edges) {
        if (e.label == "knows") knows++;
        if (e.label == "likes") list++;
    }
    for (const dg::Node& n : g.nodes) if (n.kind == "@literal") literals++;
    expect(knows == 3, "turtle object lists and blank nodes");
    expect(literals == 1, "turtle literal nodes (labels fold into nodes)");
    expect(list == 1 && g.node("http://example.org/tea"), "turtle collections");
    bool typedBlank = false;
    for (const dg::Node& n : g.nodes) if (n.id.rfind("_:", 0) == 0 && n.label == "Dan") typedBlank = true;
    expect(typedBlank, "turtle blank node property list");
    dg::Graph nt = dg::turtle("<http://a/x> <http://a/p> <http://a/y> .\n<http://a/x> <http://a/q> \"v\" .\n");
    expect(nt.edges.size() == 2, "n-triples");
    dg::RdfOptions noLit;
    noLit.literalNodes = false;
    dg::Graph nl = dg::turtle("<http://a/x> <http://a/q> \"v\" .", noLit);
    expect(nl.nodes.size() == 1 && nl.nodes[0].data.str("q", "") == "v", "turtle literals as data");
    throws([] { dg::turtle("ex:a ex:b ex:c ."); }, "turtle undeclared prefix");

    dg::Graph ld = dg::jsonld(Json::parse(R"({
        "@context": { "schema": "http://schema.org/", "name": "schema:name", "knows": { "@id": "schema:knows", "@type": "@id" } },
        "@id": "http://ex.org/ann", "@type": "schema:Person", "name": "Ann",
        "knows": ["http://ex.org/ben"],
        "schema:worksFor": { "@id": "http://ex.org/acme", "name": "Acme" }
    })"));
    expect(ld.node("http://ex.org/ann") && ld.node("http://ex.org/ann")->label == "Ann" && ld.node("http://ex.org/ann")->kind == "Person", "json-ld node");
    expect(ld.node("http://ex.org/acme") && ld.node("http://ex.org/acme")->label == "Acme", "json-ld nested object");
    expect(ld.node("http://ex.org/ben") != nullptr, "json-ld @id-typed term");
}

static void jsonAndTreeTests() {
    dg::Graph g = dg::json(Json::parse(R"({"nodes": [{"id": "a", "label": "A", "group": "g1"}, {"id": 2, "shape": "circle"}],
                                           "links": [{"source": "a", "target": 2, "label": "to"}], "directed": false})"));
    expect(g.nodes.size() == 2 && g.node("2") && g.node("2")->shape == dg::Shape::Circle, "json nodes");
    expect(g.edges.size() == 1 && g.edges[0].head == dg::Arrow::None && g.edges[0].label == "to", "json links");
    expect(g.group("g1") && g.node("a")->group == "g1", "json groups");
    dg::Graph cy = dg::json(Json::parse(R"({"elements": [{"data": {"id": "p"}}, {"data": {"id": "c", "parent": "p"}}, {"data": {"id": "d"}},
                                            {"data": {"id": "e1", "source": "c", "target": "d"}}]})"));
    expect(cy.nodes.size() == 2 && cy.group("p") && cy.node("c")->group == "p" && cy.edge("e1"), "cytoscape compound nodes");

    dg::Graph t = dg::newick("((Human:0.1,Chimp:0.12)Hominini:0.3,'Gorilla gorilla':0.45,Orang_utan:0.6)Root;");
    expect(t.layout == dg::LayoutKind::Tree && t.nodes.size() == 6 && t.edges.size() == 5, "newick structure");
    expect(t.node("Human") && t.node("Gorilla gorilla") && t.node("Orang utan"), "newick labels and quoting");
    expect(std::fabs(t.node("Chimp")->data.num("length", 0) - 0.12) < 1e-9, "newick branch lengths");
    throws([] { dg::newick("((a,b);"); }, "newick unbalanced");
}

static void layoutTests() {
    // Layered: edges point down the ranks, ranks do not overlap, groups hold their members.
    dg::Graph g = dg::mermaid(R"(graph TD
        a --> b --> c --> d
        a --> c
        b --> e --> d
        d --> a
        subgraph s [Stage]
          b
          e
        end)");
    sized(g);
    dg::layout(g);
    expect(!anyOverlap(g), "layered: no overlapping nodes");
    expect(g.node("a")->pos.y < g.node("b")->pos.y && g.node("b")->pos.y < g.node("c")->pos.y && g.node("c")->pos.y < g.node("d")->pos.y, "layered: ranks follow edges");
    expect(g.group("s")->rect.width > 0 && inside(g.group("s")->rect, *g.node("b")) && inside(g.group("s")->rect, *g.node("e")), "layered: group encloses members");
    expect(!inside(g.group("s")->rect, *g.node("a")) && !inside(g.group("s")->rect, *g.node("d")), "layered: group excludes others");
    for (const dg::Edge& e : g.edges) expect(e.points.size() >= 2, "layered: every edge routed");
    // Back edge d -> a still ends at a's boundary.
    const dg::Edge* back = g.edge("d->a");
    const dg::Node* a = g.node("a");
    expect(back && std::fabs(back->points.back().y - a->pos.y) <= a->size.y / 2 + 0.6f && std::fabs(back->points.back().x - a->pos.x) <= a->size.x / 2 + 0.6f,
           "layered: back edge clipped to its target");
    // Labels get their own slot.
    dg::Graph lab = dg::mermaid("graph LR; x -->|a long label| y");
    sized(lab);
    dg::layout(lab);
    expect(lab.node("x")->pos.x < lab.node("y")->pos.x && lab.edges[0].labelPos, "layered LR with label");
    expect(lab.node("y")->pos.x - lab.node("x")->pos.x > 120 + 60 - 1, "layered: label fits between ranks");
    // Determinism.
    dg::Graph g2 = dg::mermaid("graph TD; a-->b; a-->c; b-->d; c-->d; c-->e; e-->f; b-->f");
    dg::Graph g3 = g2;
    sized(g2); sized(g3);
    dg::layout(g2); dg::layout(g3);
    bool same = true;
    for (size_t i = 0; i < g2.nodes.size(); i++) same = same && g2.nodes[i].pos.x == g3.nodes[i].pos.x && g2.nodes[i].pos.y == g3.nodes[i].pos.y;
    expect(same, "layered: deterministic");

    // Force: a knowledge graph spreads out without overlaps; disconnected parts packed.
    dg::Graph kg = dg::turtle(R"(@prefix : <http://k/> .
        :a :p :b , :c , :d . :b :p :c . :d :p :e , :f . :x :p :y .)");
    sized(kg, 110, 44);
    dg::layout(kg);
    expect(!anyOverlap(kg), "force: no overlapping nodes");
    const float ab = std::hypot(kg.node("http://k/a")->pos.x - kg.node("http://k/b")->pos.x, kg.node("http://k/a")->pos.y - kg.node("http://k/b")->pos.y);
    const float ae = std::hypot(kg.node("http://k/a")->pos.x - kg.node("http://k/e")->pos.x, kg.node("http://k/a")->pos.y - kg.node("http://k/e")->pos.y);
    expect(ab < ae, "force: neighbours closer than two hops");
    dg::Graph kg2 = kg;
    kg2.nodes.push_back(kg2.nodes[0]);
    kg2.nodes.back().id = "http://k/new";
    kg2.addEdge("http://k/a", "http://k/new");
    for (dg::Edge& e : kg2.edges) e.points.clear();
    dg::layout(kg2, {}, &kg);
    const dg::Node* stayed = kg2.node("http://k/c");
    const dg::Node* before = kg.node("http://k/c");
    expect(std::hypot(stayed->pos.x - before->pos.x, stayed->pos.y - before->pos.y) < 120, "force: hint keeps positions");

    // Trees: a phylogram grows left to right with branch lengths.
    dg::Graph t = dg::newick("((A:1,B:2):1,C:1);");
    for (dg::Node& n : t.nodes) n.labelSize = { 20, 20 };
    dg::layout(t);
    const dg::Node* A = t.node("A");
    const dg::Node* B = t.node("B");
    const dg::Node* C = t.node("C");
    expect(A->pos.x < B->pos.x && std::fabs(A->pos.x - C->pos.x * 2) < 1, "tree: branch lengths along x");
    expect(A->pos.y != B->pos.y && B->pos.y != C->pos.y, "tree: tips spread across");
    dg::LayoutOptions radial;
    radial.kind = dg::LayoutKind::Radial;
    dg::layout(t, radial);
    expect(A->labelAngle != 0, "radial tree: tip labels rotate");
}

static void chemTests() {
    using namespace rmtn::chem;
    const Molecule caffeine = smiles("CN1C=NC2=C1C(=O)N(C(=O)N2C)C caffeine");
    expect(caffeine.formula() == "C8H10N4O2", "smiles: caffeine formula");
    expect(std::fabs(caffeine.mass() - 194.19) < 0.05, "smiles: caffeine mass");
    expect(caffeine.name == "caffeine", "smiles: title after the string");
    expect(caffeine.rings().size() == 2, "smiles: caffeine has two rings");
    expect(find(caffeine, "C=O").size() == 2, "find: two carbonyls in caffeine");

    const Molecule benzene = smiles("c1ccccc1");
    int doubles = 0;
    for (const Bond& b : benzene.bonds) doubles += b.order == 2;
    expect(doubles == 3 && benzene.formula() == "C6H6", "smiles: benzene kekulised");
    expect(smiles("Cc1ccccc1").formula() == "C7H8", "smiles: toluene");
    expect(find(smiles("Cc1ccccc1"), "c1ccccc1").size() == 1, "find: one benzene ring in toluene");
    expect(smiles("c1ccc2ccccc2c1").formula() == "C10H8", "smiles: naphthalene");
    expect(smiles("c1cc[nH]c1").formula() == "C4H5N", "smiles: pyrrole");
    expect(smiles("c1ccncc1").formula() == "C5H5N", "smiles: pyridine");
    expect(smiles("CC(=O)Oc1ccccc1C(=O)O").formula() == "C9H8O4", "smiles: aspirin");
    expect(smiles("[NH4+].[Cl-]").formula() == "ClH4N", "smiles: charges and dots");
    expect(smiles("C[N+](C)(C)C").formula() == "C4H12N", "smiles: quaternary nitrogen");
    expect(smiles("[13CH4]").atoms[0].isotope == 13, "smiles: isotopes");
    expect(smiles("C1CC2CCC1C2").rings().size() == 2, "smiles: bridged rings");
    expect(smiles("C%10CCCCC%10").rings().size() == 1, "smiles: %nn ring closures");
    expect(smiles("OC(=O)C(N)Cc1c[nH]c2ccccc12").formula() == "C11H12N2O2", "smiles: tryptophan");
    expect(find(smiles("OC(=O)CC(=O)O"), "C(=O)O").size() == 2, "find: two carboxyls");
    throws([] { smiles("C1CC"); }, "smiles unclosed ring");
    throws([] { smiles("C(C"); }, "smiles unclosed branch");

    // Depiction: bonds about one unit, atoms apart.
    for (const char* s : { "CN1C=NC2=C1C(=O)N(C(=O)N2C)C", "CC(C)Cc1ccc(cc1)C(C)C(=O)O", "C1CCC2(CC1)CCCC2", "c1ccc2cc3ccccc3cc2c1" }) {
        const Molecule m = smiles(s);
        double sum = 0, worst = 0;
        for (const Bond& b : m.bonds) {
            const double d = std::hypot(m.atoms[b.a].pos.x - m.atoms[b.b].pos.x, m.atoms[b.a].pos.y - m.atoms[b.b].pos.y);
            sum += d;
            worst = std::max(worst, std::fabs(d - 1));
        }
        double closest = 1e9;
        for (size_t i = 0; i < m.atoms.size(); i++)
            for (size_t j = i + 1; j < m.atoms.size(); j++)
                closest = std::min(closest, (double)std::hypot(m.atoms[i].pos.x - m.atoms[j].pos.x, m.atoms[i].pos.y - m.atoms[j].pos.y));
        char what[160];
        std::snprintf(what, sizeof what, "depict %s: mean bond %.2f, worst %.2f, closest %.2f", s, sum / m.bonds.size(), worst, closest);
        expect(std::fabs(sum / m.bonds.size() - 1) < 0.08 && worst < 0.3 && closest > 0.6, what);
    }

    const Molecule sdf = loadMolecule("data/caffeine.sdf");
    expect(sdf.atoms.size() == 24 && sdf.coords3d && sdf.formula() == "C8H10N4O2", "sdf: caffeine 3D");
    Molecule emb = withHydrogens(smiles("CCO"));
    embed3d(emb);
    double cc = 0;
    for (const Bond& b : emb.bonds)
        if (emb.atoms[b.a].element == "C" && emb.atoms[b.b].element == "C")
            cc = std::sqrt(std::pow(emb.atoms[b.a].pos.x - emb.atoms[b.b].pos.x, 2) + std::pow(emb.atoms[b.a].pos.y - emb.atoms[b.b].pos.y, 2) + std::pow(emb.atoms[b.a].pos.z - emb.atoms[b.b].pos.z, 2));
    expect(emb.atoms.size() == 9 && std::fabs(cc - 1.52) < 0.15, "embed3d: ethanol C-C length");

    const Protein crn = loadProtein("data/1crn.pdb");
    int helix = 0, strand = 0;
    for (const Residue& r : crn.residues) { helix += r.structure == 'H'; strand += r.structure == 'E'; }
    expect(crn.id == "1CRN" && crn.residues.size() == 46, "pdb: crambin residues");
    expect(helix == 21 && strand == 8, "pdb: helix and sheet records");
    const Molecule all = crn.molecule();
    expect(all.atoms.size() == 327 && all.bonds.size() > 320 && all.bonds.size() < 350, "pdb: bonds by distance");
    expect(crn.radius() > 10 && crn.radius() < 25, "pdb: radius");
}

int main() {
    try {
        mermaidTests();
        dotTests();
        bpmnTests();
        rdfTests();
        jsonAndTreeTests();
        layoutTests();
        chemTests();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: unexpected error: %s\n", e.what());
        return 1;
    }
    if (!ok) return 1;
    std::printf("diagram tests passed\n");
    return 0;
}
