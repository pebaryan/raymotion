// agent.hpp -- the chat agent: an OpenAI-compatible tool-calling loop on a worker thread.
//
// The UI thread never blocks: start() launches the loop, drain() hands over what happened
// since the last frame. The agent works through five tools, all confined to the workspace:
// read_file, write_file, list_files, check_script, render_still.
#pragma once
#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "rmtn/json.hpp"

namespace studio {

struct Settings {
    std::string baseUrl = "https://api.openai.com/v1";   // anything that serves /chat/completions
    std::string apiKey;                                  // empty: OPENAI_API_KEY from the environment
    std::string model = "gpt-4.1";
    std::string host;                                    // raymotion_script executable ("" = auto-detect)
    int maxSteps = 16;                                   // model round-trips per user message
    int timeoutSeconds = 300;
    bool darkTheme = true;
};

struct AgentEvent {
    enum class Kind { Assistant, ToolStart, ToolEnd, FileWritten, StillReady, Error, Done };
    Kind kind = Kind::Done;
    std::string id;       // tool call id
    std::string name;     // tool name, or the file for FileWritten / StillReady
    std::string text;     // assistant text, tool input, tool output, or the error
    bool ok = true;
    float seconds = 0;
};

class Agent {
public:
    ~Agent();
    // False when a turn is already running.
    bool start(const Settings& s, const std::filesystem::path& workspace, const std::string& userText);
    void cancel();                       // takes effect before the next request or tool call
    bool busy() const { return busy_.load(); }
    void reset();                        // forget the conversation (only when idle)
    std::vector<AgentEvent> drain();

private:
    void run(Settings s, std::filesystem::path root);
    void post(AgentEvent e);

    std::thread worker_;
    std::atomic<bool> busy_{false}, cancel_{false};
    std::mutex mu_;
    std::vector<AgentEvent> events_;
    std::vector<rmtn::Json> messages_;   // owned by the worker while busy
};

// Where raymotion_script lives: the setting, then beside this executable, then the build tree.
std::string findHost(const Settings& s, const std::filesystem::path& workspace);

// The same host commands serve the UI panels and the agent: `<host> <verb> [name] --script <file> ...`.
struct HostResult { bool ok = false; std::string output; };
HostResult hostList(const Settings& s, const std::filesystem::path& root, const std::string& script);
HostResult hostStill(const Settings& s, const std::filesystem::path& root, const std::string& script,
                     const std::string& composition, int frame, const std::string& pngPath);

// GET {baseUrl}/models: the ids the server offers, sorted. error is empty on success.
struct ModelList { std::vector<std::string> ids; std::string error; };
ModelList fetchModels(const Settings& s);

std::string toUtf8(const std::filesystem::path& p);
std::filesystem::path fromUtf8(const std::string& s);

} // namespace studio
