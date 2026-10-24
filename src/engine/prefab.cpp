// prefab.cpp -- the prefab registry and instance creation.
#include "rmtn/prefab.hpp"
#include <map>
#include <stdexcept>

namespace rmtn {
namespace prefab {

namespace {
// Never destroyed: registered at startup and used for the process's lifetime.
std::map<std::string, Prefab>& registry() {
    static auto* r = new std::map<std::string, Prefab>;
    return *r;
}
} // namespace

void add(Prefab p) {
    if (p.name.empty()) throw std::runtime_error("prefab: a prefab needs a name");
    if (!p.build) throw std::runtime_error("prefab: " + p.name + " has no build");
    registry()[p.name] = std::move(p);
}

const Prefab& get(const std::string& name) {
    auto it = registry().find(name);
    if (it == registry().end()) throw std::runtime_error("prefab: no prefab named " + name);
    return it->second;
}

std::vector<std::string> names() {
    std::vector<std::string> out;
    for (const auto& [name, p] : registry()) out.push_back(name);
    return out;
}

void apply(TNode* node, const std::string& name, Json props) {
    const Prefab& pf = get(name);
    pf.build(*node, pf.defaultProps.merged(props));
}

TNode* use(TNode* parent, int from, int dur, const std::string& name, Json props) {
    TNode* n = parent->add(from, dur);
    apply(n, name, std::move(props));
    return n;
}

TNode* use(Nodes& list, int from, int dur, const std::string& name, Json props) {
    TNode* n = addRoot(list, from, dur);
    apply(n, name, std::move(props));
    return n;
}

} // namespace prefab
} // namespace rmtn
