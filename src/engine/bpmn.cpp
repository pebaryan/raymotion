// bpmn.cpp -- BPMN 2.0 XML to dg::Graph, with a small XML reader. Uses the file's
// diagram interchange (BPMNDI) when it has one, so a process looks as it was modelled.
#include "diagram_internal.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <functional>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace rmtn {
namespace dg {
namespace detail {

// ------------------------------------------------------------------ XML
std::string Xml::local() const {
    const size_t c = name.find(':');
    return c == std::string::npos ? name : name.substr(c + 1);
}
const std::string* Xml::attr(std::string_view localName) const {
    for (const auto& [k, v] : attrs) {
        const size_t c = k.find(':');
        const std::string_view l = c == std::string::npos ? std::string_view(k) : std::string_view(k).substr(c + 1);
        if (l == localName) return &v;
    }
    return nullptr;
}
std::string Xml::get(std::string_view localName, std::string_view fallback) const {
    const std::string* v = attr(localName);
    return v ? *v : std::string(fallback);
}

namespace {
std::string decodeEntities(std::string_view s) {
    if (s.find('&') == std::string_view::npos) return std::string(s);
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] != '&') { out += s[i]; continue; }
        const size_t semi = s.find(';', i);
        if (semi == std::string_view::npos) { out += s[i]; continue; }
        const std::string_view ent = s.substr(i + 1, semi - i - 1);
        if (ent == "amp") out += '&';
        else if (ent == "lt") out += '<';
        else if (ent == "gt") out += '>';
        else if (ent == "quot") out += '"';
        else if (ent == "apos") out += '\'';
        else if (!ent.empty() && ent[0] == '#') {
            const long cp = ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X') ? std::strtol(std::string(ent.substr(2)).c_str(), nullptr, 16)
                                                                                : std::strtol(std::string(ent.substr(1)).c_str(), nullptr, 10);
            int len = 0;
            const char* u = CodepointToUTF8((int)cp, &len);
            out.append(u, (size_t)len);
        } else { out += '&'; continue; }
        i = semi;
    }
    return out;
}

class XmlReader {
public:
    explicit XmlReader(std::string_view s) : s_(s) {}
    Xml run() {
        Xml doc;
        doc.name = "#document";
        content(doc, true);
        for (Xml& k : doc.kids) return std::move(k);
        throw std::runtime_error("xml: no root element");
    }

private:
    [[noreturn]] void err(const char* what) { fail("xml: line %d: %s", line_, what); }
    bool starts(const char* t) const { return s_.compare(i_, std::strlen(t), t) == 0; }
    void advance(size_t n) {
        for (size_t k = 0; k < n && i_ < s_.size(); k++, i_++) if (s_[i_] == '\n') line_++;
    }
    void skipTo(const char* t) {
        const size_t f = s_.find(t, i_);
        if (f == std::string_view::npos) err("unterminated markup");
        advance(f - i_ + std::strlen(t));
    }
    void content(Xml& parent, bool top) {
        while (i_ < s_.size()) {
            if (starts("<!--")) { skipTo("-->"); continue; }
            if (starts("<?")) { skipTo("?>"); continue; }
            if (starts("<![CDATA[")) {
                advance(9);
                const size_t f = s_.find("]]>", i_);
                if (f == std::string_view::npos) err("unterminated CDATA");
                parent.text += std::string(s_.substr(i_, f - i_));
                advance(f - i_ + 3);
                continue;
            }
            if (starts("<!")) { skipTo(">"); continue; }
            if (starts("</")) {
                if (top) err("unexpected closing tag");
                const size_t close = s_.find('>', i_);
                if (close == std::string_view::npos) err("unterminated closing tag");
                const std::string name = trim(s_.substr(i_ + 2, close - i_ - 2));
                if (name != parent.name) err(("</" + name + "> closes <" + parent.name + ">").c_str());
                advance(close - i_ + 1);
                return;
            }
            if (s_[i_] == '<') { parent.kids.push_back(element()); continue; }
            const size_t lt = s_.find('<', i_);
            const size_t end = lt == std::string_view::npos ? s_.size() : lt;
            parent.text += decodeEntities(s_.substr(i_, end - i_));
            advance(end - i_);
        }
        if (!top) err(("missing </" + parent.name + ">").c_str());
    }
    Xml element() {
        Xml e;
        e.line = line_;
        advance(1);
        size_t j = i_;
        while (j < s_.size() && !std::isspace((unsigned char)s_[j]) && s_[j] != '>' && s_[j] != '/') j++;
        e.name = std::string(s_.substr(i_, j - i_));
        advance(j - i_);
        for (;;) {
            while (i_ < s_.size() && std::isspace((unsigned char)s_[i_])) advance(1);
            if (i_ >= s_.size()) err("unterminated tag");
            if (starts("/>")) { advance(2); return e; }
            if (s_[i_] == '>') { advance(1); break; }
            size_t k = i_;
            while (k < s_.size() && s_[k] != '=' && !std::isspace((unsigned char)s_[k]) && s_[k] != '>') k++;
            const std::string key(s_.substr(i_, k - i_));
            advance(k - i_);
            while (i_ < s_.size() && std::isspace((unsigned char)s_[i_])) advance(1);
            if (i_ >= s_.size() || s_[i_] != '=') { e.attrs.push_back({ key, "" }); continue; }
            advance(1);
            while (i_ < s_.size() && std::isspace((unsigned char)s_[i_])) advance(1);
            const char q = i_ < s_.size() ? s_[i_] : '\0';
            if (q != '"' && q != '\'') err("attribute value must be quoted");
            const size_t close = s_.find(q, i_ + 1);
            if (close == std::string_view::npos) err("unterminated attribute value");
            e.attrs.push_back({ key, decodeEntities(s_.substr(i_ + 1, close - i_ - 1)) });
            advance(close - i_ + 1);
        }
        content(e, false);
        return e;
    }
    std::string_view s_;
    size_t i_ = 0;
    int line_ = 1;
};
} // namespace

Xml parseXml(std::string_view text) { return XmlReader(text).run(); }

} // namespace detail

using namespace detail;

// ------------------------------------------------------------------ BPMN
namespace {
const std::map<std::string, std::string> EVENT_DEFS = {
    { "messageEventDefinition", "message" }, { "timerEventDefinition", "timer" }, { "errorEventDefinition", "error" },
    { "signalEventDefinition", "signal" }, { "terminateEventDefinition", "terminate" },
    { "conditionalEventDefinition", "conditional" }, { "escalationEventDefinition", "escalation" },
    { "linkEventDefinition", "link" }, { "compensateEventDefinition", "compensation" }, { "cancelEventDefinition", "cancel" },
};
const std::map<std::string, std::string> TASKS = {
    { "task", "" }, { "userTask", "user" }, { "serviceTask", "service" }, { "scriptTask", "script" },
    { "sendTask", "send" }, { "receiveTask", "receive" }, { "manualTask", "manual" },
    { "businessRuleTask", "businessRule" }, { "callActivity", "call" }, { "subProcess", "subProcess" },
    { "transaction", "subProcess" }, { "adHocSubProcess", "subProcess" },
};
const std::map<std::string, std::string> GATEWAYS = {
    { "exclusiveGateway", "exclusive" }, { "parallelGateway", "parallel" }, { "inclusiveGateway", "inclusive" },
    { "eventBasedGateway", "event" }, { "complexGateway", "complex" },
};

struct Builder {
    Graph g;
    std::unordered_map<std::string, const Xml*> byId;
    std::unordered_map<std::string, std::string> nodeGroup;     // flow node -> lane or pool
    std::unordered_set<std::string> gateways;
    std::unordered_map<std::string, std::string> defaults;       // flow id -> default marker owner

    void index(const Xml& x) {
        if (const std::string* id = x.attr("id")) byId[*id] = &x;
        for (const Xml& k : x.kids) index(k);
    }
    static std::string name(const Xml& x) { return cleanLabel(x.get("name")); }

    void lanes(const Xml& laneSet, const std::string& parent) {
        for (const Xml& lane : laneSet.kids) {
            if (lane.local() != "lane") continue;
            const std::string id = lane.get("id");
            Group& gr = g.addGroup(id, name(lane), parent);
            gr.kind = Group::Kind::Lane;
            for (const Xml& k : lane.kids) {
                if (k.local() == "flowNodeRef") nodeGroup[trim(k.text)] = id;
                if (k.local() == "childLaneSet") lanes(k, id);
            }
        }
    }

    void flowElements(const Xml& container, const std::string& group) {
        for (const Xml& x : container.kids) {
            const std::string t = x.local(), id = x.get("id");
            if (t == "laneSet") { lanes(x, group); continue; }
            if (id.empty()) continue;
            auto nodeIn = [&](Shape shape) -> Node& {
                Node& n = g.addNode(id, name(x), shape);
                n.label = name(x);
                n.kind = t;
                n.group = nodeGroup.count(id) ? nodeGroup[id] : group;
                return n;
            };
            if (t.size() > 5 && t.compare(t.size() - 5, 5, "Event") == 0) {
                Node& n = nodeIn(Shape::Event);
                n.labelPlace = LabelPlace::Below;
                for (const Xml& k : x.kids) {
                    auto d = EVENT_DEFS.find(k.local());
                    if (d != EVENT_DEFS.end()) { n.marker = d->second; break; }
                }
                if (t == "intermediateThrowEvent" || t == "endEvent") n.kind += " throw";
                if (t == "boundaryEvent" && x.get("cancelActivity", "true") == "false") n.kind += " nonInterrupting";
                if (t == "startEvent" && x.get("isInterrupting", "true") == "false") n.kind += " nonInterrupting";
                continue;
            }
            if (auto gw = GATEWAYS.find(t); gw != GATEWAYS.end()) {
                Node& n = nodeIn(Shape::Gateway);
                n.marker = gw->second;
                n.labelPlace = LabelPlace::Below;
                gateways.insert(id);
                if (x.attr("default")) defaults[x.get("default")] = id;
                continue;
            }
            if (auto tk = TASKS.find(t); tk != TASKS.end()) {
                Node& n = nodeIn(Shape::Task);
                n.marker = tk->second;
                if (x.attr("default")) defaults[x.get("default")] = id;
                for (const Xml& k : x.kids) {           // data associations hang off the activity
                    const std::string kl = k.local();
                    if (kl != "dataInputAssociation" && kl != "dataOutputAssociation") continue;
                    std::string src, dst;
                    for (const Xml& r : k.kids) {
                        if (r.local() == "sourceRef") src = trim(r.text);
                        if (r.local() == "targetRef") dst = trim(r.text);
                    }
                    if (kl == "dataOutputAssociation") src = id;
                    else dst = id;
                    pending.push_back({ k.get("id", src + "->" + dst), src, dst, "", "dataAssociation" });
                }
                if (t == "subProcess" || t == "transaction" || t == "adHocSubProcess") subProcesses.push_back({ &x, n.group });
                continue;
            }
            if (t == "dataObjectReference" || t == "dataObject") {
                if (t == "dataObject" && byIdRefFromRef(id)) continue;   // shown through its reference
                Node& n = nodeIn(Shape::DataObject);
                n.labelPlace = LabelPlace::Below;
                continue;
            }
            if (t == "dataStoreReference") { Node& n = nodeIn(Shape::DataStore); n.labelPlace = LabelPlace::Below; continue; }
            if (t == "textAnnotation") {
                Node& n = nodeIn(Shape::Annotation);
                for (const Xml& k : x.kids) if (k.local() == "text") n.label = cleanLabel(k.text);
                continue;
            }
            if (t == "sequenceFlow") {
                bool cond = false;
                for (const Xml& k : x.kids) if (k.local() == "conditionExpression") cond = true;
                pending.push_back({ id, x.get("sourceRef"), x.get("targetRef"), name(x), cond ? "sequenceFlow conditional" : "sequenceFlow" });
                continue;
            }
            if (t == "association") {
                pending.push_back({ id, x.get("sourceRef"), x.get("targetRef"), "", "association" + std::string(x.get("associationDirection") == "One" ? " one" : "") });
                continue;
            }
        }
    }
    bool byIdRefFromRef(const std::string& dataObjectId) const {
        for (auto& [id, x] : byId) if (x->local() == "dataObjectReference" && x->get("dataObjectRef") == dataObjectId) return true;
        return false;
    }

    struct PendingEdge { std::string id, from, to, label, kind; };
    std::vector<PendingEdge> pending;
    std::vector<std::pair<const Xml*, std::string>> subProcesses;

    void edges() {
        for (const PendingEdge& p : pending) {
            Edge e;
            e.id = p.id; e.from = p.from; e.to = p.to; e.label = p.label;
            e.kind = p.kind.substr(0, p.kind.find(' '));
            e.curved = false;
            if (e.kind == "sequenceFlow") {
                e.head = Arrow::Normal;
                if (p.kind.find("conditional") != std::string::npos && !gateways.count(p.from)) e.tail = Arrow::OpenDiamond;
                if (defaults.count(p.id)) e.tail = Arrow::Cross;      // default flow: a slash at the source
            } else if (e.kind == "messageFlow") {
                e.stroke = Stroke::Dashed; e.head = Arrow::Open; e.tail = Arrow::OpenCircle;
            } else {                                                   // association, data association
                e.stroke = Stroke::Dotted;
                e.head = p.kind.find("one") != std::string::npos || e.kind == "dataAssociation" ? Arrow::Open : Arrow::None;
            }
            g.edges.push_back(std::move(e));
        }
    }
};
} // namespace

Graph bpmn(std::string_view text) {
    const Xml root = parseXml(text);
    if (root.local() != "definitions") fail("bpmn: line %d: root element is <%s>, not <definitions>", root.line, root.name);
    Builder b;
    b.index(root);
    // Pools: participants of collaborations, each naming the process it shows.
    std::unordered_map<std::string, std::string> processPool;
    for (const Xml& x : root.kids) {
        if (x.local() != "collaboration") continue;
        for (const Xml& k : x.kids) {
            if (k.local() == "participant") {
                Group& gr = b.g.addGroup(k.get("id"), cleanLabel(k.get("name")));
                gr.kind = Group::Kind::Pool;
                if (k.attr("processRef")) processPool[k.get("processRef")] = k.get("id");
            } else if (k.local() == "messageFlow") {
                b.pending.push_back({ k.get("id"), k.get("sourceRef"), k.get("targetRef"), cleanLabel(k.get("name")), "messageFlow" });
            } else if (k.local() == "textAnnotation" || k.local() == "association") {
                Xml wrapper;
                wrapper.kids.push_back(k);
                b.flowElements(wrapper, "");
            }
        }
    }
    for (const Xml& x : root.kids) {
        if (x.local() != "process") continue;
        const std::string pool = processPool.count(x.get("id")) ? processPool[x.get("id")] : "";
        // Lanes first so flow nodes know their lane.
        for (const Xml& k : x.kids) if (k.local() == "laneSet") b.lanes(k, pool);
        Xml noLanes = x;
        noLanes.kids.erase(std::remove_if(noLanes.kids.begin(), noLanes.kids.end(), [](const Xml& k) { return k.local() == "laneSet"; }), noLanes.kids.end());
        b.flowElements(noLanes, pool);
    }

    // Diagram interchange.
    struct Bounds { float x, y, w, h; };
    std::unordered_map<std::string, Bounds> shapes, labels;
    std::unordered_map<std::string, std::vector<Vector2>> waypoints;
    std::unordered_map<std::string, bool> expanded, horizontal;
    std::function<void(const Xml&)> di = [&](const Xml& x) {
        const std::string t = x.local();
        auto bounds = [](const Xml& el, Bounds& out) {
            for (const Xml& k : el.kids)
                if (k.local() == "Bounds") {
                    out = { (float)std::atof(k.get("x", "0").c_str()), (float)std::atof(k.get("y", "0").c_str()),
                            (float)std::atof(k.get("width", "0").c_str()), (float)std::atof(k.get("height", "0").c_str()) };
                    return true;
                }
            return false;
        };
        if (t == "BPMNShape") {
            const std::string el = x.get("bpmnElement");
            Bounds bd{};
            if (bounds(x, bd)) shapes[el] = bd;
            if (x.attr("isExpanded")) expanded[el] = x.get("isExpanded") == "true";
            if (x.attr("isHorizontal")) horizontal[el] = x.get("isHorizontal") == "true";
            for (const Xml& k : x.kids) if (k.local() == "BPMNLabel" && bounds(k, bd)) labels[el] = bd;
            return;
        }
        if (t == "BPMNEdge") {
            const std::string el = x.get("bpmnElement");
            auto& pts = waypoints[el];
            for (const Xml& k : x.kids) {
                if (k.local() == "waypoint") pts.push_back({ (float)std::atof(k.get("x", "0").c_str()), (float)std::atof(k.get("y", "0").c_str()) });
                Bounds bd{};
                if (k.local() == "BPMNLabel" && bounds(k, bd)) labels[el] = bd;
            }
            return;
        }
        for (const Xml& k : x.kids) di(k);
    };
    for (const Xml& x : root.kids) if (x.local() == "BPMNDiagram") { di(x); break; }    // the first diagram

    // Expanded sub-processes become groups holding their content.
    for (auto& [sx, group] : b.subProcesses) {
        const std::string id = sx->get("id");
        if (!expanded.count(id) || !expanded[id] || !shapes.count(id)) continue;
        Group& gr = b.g.addGroup(id, Builder::name(*sx), group);
        gr.kind = Group::Kind::Cluster;
        gr.style.dash = std::nullopt;
        b.g.nodes.erase(std::remove_if(b.g.nodes.begin(), b.g.nodes.end(), [&](const Node& n) { return n.id == id; }), b.g.nodes.end());
        b.flowElements(*sx, id);
    }
    b.edges();

    Graph& g = b.g;
    if (!shapes.empty()) {
        g.layout = LayoutKind::None;
        g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(), [&](const Node& n) { return !shapes.count(n.id); }), g.nodes.end());
        for (Node& n : g.nodes) {
            const Bounds& bd = shapes[n.id];
            n.pos = { bd.x + bd.w / 2, bd.y + bd.h / 2 };
            n.size = { bd.w, bd.h };
            n.fixed = true;
            if (n.labelPlace == LabelPlace::Below && labels.count(n.id)) {
                const Bounds& lb = labels[n.id];
                if (lb.y + lb.h / 2 < n.pos.y - bd.h / 2) n.labelPlace = LabelPlace::Above;
                else if (lb.x > n.pos.x + bd.w / 2 && lb.y < n.pos.y + bd.h / 2) n.labelPlace = LabelPlace::Right;
            }
        }
        for (Group& gr : g.groups) {
            if (!shapes.count(gr.id)) continue;
            const Bounds& bd = shapes[gr.id];
            gr.rect = { bd.x, bd.y, bd.w, bd.h };
            gr.fixed = true;
            if (horizontal.count(gr.id)) gr.horizontal = horizontal[gr.id];
        }
        g.edges.erase(std::remove_if(g.edges.begin(), g.edges.end(), [&](const Edge& e) { return waypoints[e.id].size() < 2; }), g.edges.end());
        for (Edge& e : g.edges) {
            e.points = waypoints[e.id];
            e.fixed = true;
            if (labels.count(e.id) && !e.label.empty()) {
                const Bounds& lb = labels[e.id];
                e.labelPos = Vector2{ lb.x + lb.w / 2, lb.y + lb.h / 2 };
            }
        }
    } else {
        g.layout = LayoutKind::Layered;
        g.direction = Direction::LR;
        // Without coordinates an edge must join two nodes.
        g.edges.erase(std::remove_if(g.edges.begin(), g.edges.end(), [&](const Edge& e) { return !g.node(e.from) || !g.node(e.to); }), g.edges.end());
        for (Edge& e : g.edges) e.curved = false;
    }
    for (Group& gr : g.groups)
        if (gr.kind != Group::Kind::Cluster) gr.style.fill = gr.style.fill.value_or(Color{ 0, 0, 0, 0 });
    return g;
}

} // namespace dg
} // namespace rmtn
