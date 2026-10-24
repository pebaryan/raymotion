// rmtn/prefab.hpp -- named, parameterized subtree builders ("components" in Remotion
// terms). A project registers prefabs once, at startup; any composition instantiates
// them as ordinary timeline nodes, so Series/loop/freeze/isolate/layout/media compose
// with them unchanged.
//
//     prefab::add({ "feature-card", Json::parse(R"({"accent": [30,144,255,255]})"),
//                   [](TNode& n, const Json& p) {
//                       n.up = [t0 = p.integer("delay", 0)](int t, NodeProps& pr) {
//                           pr.opacity = (float)interpolate(t - t0, { 0, 15 }, { 0, 1 }, clamped());
//                       };
//                   } });
//     prefab::use(root, 30, 60, "feature-card", Json{{"title", "Fast"}});
//
// The caller owns timing: an instance is a node whose start/dur the composition sets;
// the prefab fills its content. Props are the prefab's defaultProps merged under the
// instance's (as Comp::build does), and a missing name throws std::runtime_error.
#pragma once
#include "timeline.hpp"
#include <functional>
#include <string>
#include <vector>

namespace rmtn {
namespace prefab {

struct Prefab {
    std::string name;
    Json defaultProps;                                        // merged under instance props
    std::function<void(TNode& node, const Json& props)> build;    // fills node's subtree
};

void add(Prefab p);                        // register project-wide; duplicates replace
const Prefab& get(const std::string& name);
std::vector<std::string> names();          // sorted

// Makes `node` an instance of `name`: merges props over the prefab's defaultProps and
// runs its build. The node keeps the start/dur/pos the caller gave it.
void apply(TNode* node, const std::string& name, Json props = {});

// Convenience: parent->add(from, dur) + apply; returns the node.
TNode* use(TNode* parent, int from, int dur, const std::string& name, Json props = {});
TNode* use(Nodes& list, int from, int dur, const std::string& name, Json props = {});

} // namespace prefab
} // namespace rmtn
