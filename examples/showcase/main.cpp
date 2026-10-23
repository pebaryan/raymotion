// The showcase project: demo, FX and Here-and-There compositions behind the engine CLI.
#include "rmtn/cli.hpp"

namespace rmtn { void register_scenes(Registry& out); }

int main(int argc, const char** argv) {
    return rmtn::runCli(argc, argv, rmtn::register_scenes);
}
