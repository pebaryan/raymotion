#include "rmtn/cli.hpp"
int main(int argc, const char** argv) {
    return rmtn::runCli(argc, argv, [](rmtn::Registry&) {});
}
