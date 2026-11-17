// proc.hpp -- run a child process and capture its output (no shell, no quoting surprises).
#pragma once
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace studio {

struct ProcResult {
    int code = -1;          // exit code; -1 when the process could not be started
    std::string output;     // stdout + stderr, merged
};

// Runs args[0] with the rest as arguments, in `cwd` (empty = inherit). Blocks.
ProcResult runCapture(const std::vector<std::string>& args, const std::string& cwd = {});

// A process started by spawn() (the live preview window). It keeps running when this object
// goes away; stop() ends it. Move-only.
class Spawned {
public:
    Spawned() = default;
    Spawned(Spawned&& o) noexcept { *this = std::move(o); }
    Spawned& operator=(Spawned&& o) noexcept;
    Spawned(const Spawned&) = delete;
    Spawned& operator=(const Spawned&) = delete;
    ~Spawned();
    explicit operator bool() const { return pid_ != 0; }
    bool running();
    void stop();
private:
    friend Spawned spawn(const std::vector<std::string>&, const std::string&);
    void* handle_ = nullptr;   // Windows: held so the process id cannot be reused under us
    long pid_ = 0;
};

// Starts a process and returns immediately; an empty Spawned on failure.
Spawned spawn(const std::vector<std::string>& args, const std::string& cwd = {});

// Directory holding the running executable ("" when unknown).
std::string exeDir();

// This process's scratch folder in a workspace: out/studio/sessions/<pid>. Stills, drafts
// and the agent's frames go there, so several Studio windows on one workspace never
// overwrite or delete each other's files.
std::filesystem::path sessionDir(const std::filesystem::path& root);
// Deletes the session folders of Studio processes that are no longer running.
void removeStaleSessions(const std::filesystem::path& root);

} // namespace studio
