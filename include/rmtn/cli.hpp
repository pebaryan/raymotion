// rmtn/cli.hpp -- the raymotion command line, reusable by any project:
//
//     void registerComps(rmtn::Registry& r) { /* r.push_back(...) */ }
//     int main(int argc, const char** argv) { return rmtn::runCli(argc, argv, registerComps); }
#pragma once
#include <functional>
#include "timeline.hpp"

namespace rmtn {

using RegisterFn = std::function<void(Registry&)>;

// list | preview | render | still, over the compositions `registerAll` adds.
// Returns the process exit code; exceptions are reported and mapped to 2.
int runCli(int argc, const char** argv, const RegisterFn& registerAll);

} // namespace rmtn
