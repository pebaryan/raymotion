// Backend contract tests: a second language can join without changing CLI or renderer.
#include "rmtn/script.hpp"
#include <iostream>
#include <stdexcept>

using namespace rmtn;
namespace {
void require(bool ok) { if (!ok) throw std::runtime_error("script backend contract failed"); }
struct TestProgram : script::Program {
    int frame = -1;
    script::Description configure(const Json& props) override {
        script::Description d;
        d.name = "backend-test"; d.w = props.integer("width", 64); d.h = 32; d.duration = 20;
        d.scenes.push_back({5, 10, "body"});
        return d;
    }
    void draw(const std::string& function, const Ctx&, int t) override {
        require(function == "body"); frame = t;
    }
};
}
int main() {
    try {
        auto program = std::make_shared<TestProgram>();
        std::weak_ptr<TestProgram> weak = program;
        script::registerBackend(".testvm", [weak](const std::string&) { return weak.lock(); });
        Registry r;
        script::registerFile(r, "unused.TESTVM");
        require(r.size() == 1 && r[0].w == 64);
        r[0].build(r[0], Json::parse("{\"width\":128}"));
        require(r[0].w == 128 && r[0].roots.size() == 1);
        r[0].roots[0]->fn(Ctx{}, 8);
        require(program->frame == 8);
        try { script::registerFile(r, "unused.testvm"); require(false); }
        catch (const std::runtime_error& e) { require(std::string(e.what()).find("duplicate") != std::string::npos); }
        try { r[0].build(r[0], Json::parse("{\"width\":-1}")); require(false); }
        catch (const std::runtime_error& e) { require(std::string(e.what()).find("invalid") != std::string::npos); }
        program.reset(); require(!weak.expired());
        r.clear(); require(weak.expired());
        std::cout << "script backend contract passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
