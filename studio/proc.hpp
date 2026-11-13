// proc.hpp -- run a child process and capture its output (no shell, no quoting surprises).
#pragma once
#include <string>
#include <vector>

namespace studio {

struct ProcResult {
    int code = -1;          // exit code; -1 when the process could not be started
    std::string output;     // stdout + stderr, merged
};

// Runs args[0] with the rest as arguments, in `cwd` (empty = inherit). Blocks.
ProcResult runCapture(const std::vector<std::string>& args, const std::string& cwd = {});

// Starts a process and returns immediately (the live preview window). False on failure.
bool spawnDetached(const std::vector<std::string>& args, const std::string& cwd = {});

// Directory holding the running executable ("" when unknown).
std::string exeDir();

} // namespace studio
