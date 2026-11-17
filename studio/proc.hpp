// proc.hpp -- run a child process and capture its output (no shell, no quoting surprises).
#pragma once
#include <filesystem>
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

// This process's scratch folder in a workspace: out/studio/sessions/<pid>. Stills, drafts
// and the agent's frames go there, so several Studio windows on one workspace never
// overwrite or delete each other's files.
std::filesystem::path sessionDir(const std::filesystem::path& root);
// Deletes the session folders of Studio processes that are no longer running.
void removeStaleSessions(const std::filesystem::path& root);

} // namespace studio
