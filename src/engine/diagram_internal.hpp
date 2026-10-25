// diagram_internal.hpp -- pieces shared by the diagram parsers, layouts and renderer.
#pragma once
#include "rmtn/diagram.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace rmtn {
namespace dg {
namespace detail {

// A node's outline around its centre (closed polygon, diagram units). Curved shapes are
// sampled finely enough to draw.
std::vector<Vector2> outline(const Node& n, float corner);
// Where the segment from n's centre towards `toward` leaves the node's outline.
Vector2 boundaryPoint(const Node& n, Vector2 toward, float corner);
// Straight route between two nodes' boundaries, bent sideways by `bend` (diagram units,
// perpendicular at the middle) so parallel edges separate.
std::vector<Vector2> straightRoute(const Node& a, const Node& b, float bend, float corner);
// A loop on the side of `n` for an edge from a node to itself.
std::vector<Vector2> selfLoop(const Node& n, int index);
// The box a node's label occupies (inside, or beside it per labelPlace).
Rectangle labelRect(const Node& n);
// Fits group rectangles around their members (innermost first), leaving `pad` and room
// for the label on top. Fixed groups are kept.
void fitGroups(Graph& g, float pad);
// Length of the polyline.
float pathLength(const std::vector<Vector2>& pts);
// Point and unit tangent at arc length `s` along a polyline.
Vector2 pointAt(const std::vector<Vector2>& pts, float s, Vector2* tangent = nullptr);

// Text helpers for the parsers.
std::string trim(std::string_view s);
std::string lower(std::string_view s);
// "<br>", "<br/>" and "\n" escapes to newlines; HTML entities and tags dropped.
std::string cleanLabel(std::string_view s);
// Reads a whole file (resolved like media::resolve); throws when it cannot.
std::string readFile(const std::string& file);
[[noreturn]] void fail(const char* format, int line, std::string_view what);

// A minimal XML DOM (elements, attributes, text; namespaces kept as written).
struct Xml {
    std::string name;                // qualified name as written (e.g. "bpmn:task")
    std::vector<std::pair<std::string, std::string>> attrs;
    std::vector<Xml> kids;
    std::string text;                // concatenated character data
    int line = 0;
    std::string local() const;       // name without its prefix
    const std::string* attr(std::string_view localName) const;   // by local name
    std::string get(std::string_view localName, std::string_view fallback = "") const;
};
Xml parseXml(std::string_view text);

} // namespace detail
} // namespace dg
} // namespace rmtn
