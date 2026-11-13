#include "agent.hpp"
#include "http.hpp"
#include "proc.hpp"
#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using rmtn::Json;

namespace studio {

std::string toUtf8(const fs::path& p) { auto u = p.u8string(); return std::string(u.begin(), u.end()); }
fs::path fromUtf8(const std::string& s) { return fs::u8path(s); }

namespace {

constexpr size_t kMaxToolOutput = 14000;     // bytes returned to the model per tool call
constexpr size_t kMaxFile = 1u << 20;        // write_file limit

std::string clip(std::string s, size_t n = kMaxToolOutput) {
    if (s.size() > n) { s.resize(n); s += "\n...[truncated]"; }
    return s;
}

bool readFile(const fs::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss; ss << f.rdbuf();
    out = ss.str();
    return true;
}

// Resolve a workspace-relative path; refuses anything that leaves the workspace.
// Writes are additionally kept out of .git, third_party and build trees.
bool resolve(const fs::path& root, const std::string& rel, bool forWrite, fs::path& out, std::string& err) {
    std::error_code ec;
    fs::path base = fs::weakly_canonical(root, ec);
    fs::path p = fs::weakly_canonical(base / fromUtf8(rel), ec);
    if (ec) { err = "bad path: " + rel; return false; }
    fs::path r = p.lexically_relative(base);
    if (r.empty() || *r.begin() == "..") { err = "path is outside the workspace: " + rel; return false; }
    if (forWrite) {
        std::string first = toUtf8(*r.begin());
        if (first == ".git" || first == "third_party" || first.rfind("build", 0) == 0) {
            err = "writing to '" + first + "' is not allowed";
            return false;
        }
    }
    out = p;
    return true;
}

std::string apiUrl(std::string base, const char* path) {
    while (!base.empty() && base.back() == '/') base.pop_back();
    return base + path;
}

HttpHeaders apiHeaders(const Settings& s) {
    std::string key = s.apiKey;
    if (key.empty()) { const char* k = std::getenv("OPENAI_API_KEY"); if (k) key = k; }
    HttpHeaders h = { { "Content-Type", "application/json" } };
    if (!key.empty()) h.push_back({ "Authorization", "Bearer " + key });
    return h;
}

const char* kTools = R"JSON([
 {"type":"function","function":{"name":"read_file","description":"Read a UTF-8 text file in the workspace.",
  "parameters":{"type":"object","properties":{"path":{"type":"string","description":"workspace-relative path"}},"required":["path"]}}},
 {"type":"function","function":{"name":"write_file","description":"Create or overwrite a text file in the workspace (parent folders are created). Use for briefs (docs/<slug>-brief.md) and compositions (.as or .cpp).",
  "parameters":{"type":"object","properties":{"path":{"type":"string"},"content":{"type":"string"}},"required":["path","content"]}}},
 {"type":"function","function":{"name":"list_files","description":"List files under a workspace folder (recursive, at most 300 entries).",
  "parameters":{"type":"object","properties":{"path":{"type":"string","description":"folder, default '.'"},"extension":{"type":"string","description":"optional filter such as .as or .png"}}}}},
 {"type":"function","function":{"name":"check_script","description":"Compile a composition script with raymotion_script and list its compositions. Returns compiler errors with line numbers.",
  "parameters":{"type":"object","properties":{"script":{"type":"string","description":"path to a .as or .cpp file"}},"required":["script"]}}},
 {"type":"function","function":{"name":"render_still","description":"Render one frame of a composition to a PNG that the user sees in the Preview panel.",
  "parameters":{"type":"object","properties":{"script":{"type":"string"},"composition":{"type":"string","description":"composition name; omit when the script has one"},"frame":{"type":"integer","description":"frame index, default 0"}},"required":["script"]}}}
])JSON";

const char* kSystem =
    "You are the authoring agent inside Raymotion Studio, a motion-graphics editor. The user "
    "writes briefs (Markdown) and compositions (AngelScript .as or C++ .cpp) that the "
    "raymotion_script host renders. You work in the user's workspace through tools.\n"
    "Rules:\n"
    "- Before writing a composition, read AGENTS.md and the files it names "
    "(skills/raymotion/references/authoring-standard.md, worked-examples.md) with read_file, and "
    "write a filled brief to docs/<slug>-brief.md first. Do not skip the brief.\n"
    "- After writing a script, run check_script and fix every error, then render_still at chosen "
    "frames and say what you expect to see. Do not claim a film is done because it compiles.\n"
    "- Keep replies short. Say what you changed and what is still unverified.\n";

} // namespace

ModelList fetchModels(const Settings& s) {
    ModelList out;
    if (s.baseUrl.empty()) { out.error = "Set the base URL first."; return out; }
    HttpResponse r = httpRequest("GET", apiUrl(s.baseUrl, "/models"), apiHeaders(s), {}, 30);
    if (!r.error.empty()) { out.error = r.error; return out; }
    Json j;
    try { j = Json::parse(r.body); }
    catch (const std::exception&) { out.error = "HTTP " + std::to_string(r.status) + ": the server did not return JSON: " + clip(r.body, 200); return out; }
    if (j.has("error")) {
        const Json& er = j["error"];
        out.error = "HTTP " + std::to_string(r.status) + ": " + (er.isObject() ? er.str("message", "error") : clip(j.dump(), 200));
        return out;
    }
    // OpenAI, Ollama (/v1), llama.cpp and LM Studio: {"data":[{"id":"..."}]}. Some servers use {"models":[{"name":"..."}]}.
    for (const Json& m : j["data"].isArray() ? j["data"].asArray() : Json::Array{}) {
        std::string id = m.str("id", "");
        if (!id.empty()) out.ids.push_back(id);
    }
    if (out.ids.empty() && j["models"].isArray())
        for (const Json& m : j["models"].asArray()) {
            std::string id = m.str("id", m.str("name", ""));
            if (!id.empty()) out.ids.push_back(id);
        }
    std::sort(out.ids.begin(), out.ids.end());
    out.ids.erase(std::unique(out.ids.begin(), out.ids.end()), out.ids.end());
    if (out.ids.empty()) out.error = "The server listed no models (HTTP " + std::to_string(r.status) + ").";
    return out;
}

std::string findHost(const Settings& s, const fs::path& workspace) {
    std::error_code ec;
    if (!s.host.empty()) return s.host;
    std::vector<fs::path> candidates;
#ifdef _WIN32
    const char* exe = "raymotion_script.exe";
#else
    const char* exe = "raymotion_script";
#endif
    if (!exeDir().empty()) candidates.push_back(fromUtf8(exeDir()) / exe);
#ifdef RMTN_STUDIO_HOST
    candidates.push_back(RMTN_STUDIO_HOST);
#endif
    for (const char* d : { "build/Release", "build", "build/Debug" }) candidates.push_back(workspace / d / exe);
    for (const auto& c : candidates) if (fs::exists(c, ec)) return toUtf8(c);
    return exe;   // hope for PATH
}

HostResult hostList(const Settings& s, const fs::path& root, const std::string& script) {
    ProcResult r = runCapture({ findHost(s, root), "list", "--script", script }, toUtf8(root));
    return { r.code == 0, r.output };
}

HostResult hostStill(const Settings& s, const fs::path& root, const std::string& script,
                     const std::string& composition, int frame, const std::string& pngPath) {
    // The CLI wants a positional name; a script with one composition does not need the caller to know it.
    std::string name = composition;
    if (name.empty()) {
        HostResult l = hostList(s, root, script);
        std::istringstream in(l.output);
        std::string line;
        while (l.ok && std::getline(in, line)) {
            if (line.find("fps") == std::string::npos) continue;
            std::istringstream ls(line);
            ls >> name;
            break;
        }
        if (name.empty()) return { false, l.ok ? "no compositions found in " + script : l.output };
    }
    std::vector<std::string> a = { findHost(s, root), "still", name };
    a.insert(a.end(), { "--script", script, "--at", std::to_string(frame), "--png", pngPath });
    ProcResult r = runCapture(a, toUtf8(root));
    return { r.code == 0, r.output };
}

Agent::~Agent() {
    cancel();
    if (worker_.joinable()) worker_.join();
}

void Agent::post(AgentEvent e) {
    std::lock_guard<std::mutex> lk(mu_);
    events_.push_back(std::move(e));
}

std::vector<AgentEvent> Agent::drain() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<AgentEvent> out;
    out.swap(events_);
    return out;
}

void Agent::cancel() { cancel_ = true; }

void Agent::reset() {
    if (busy_) return;
    messages_.clear();
}

bool Agent::start(const Settings& s, const fs::path& workspace, const std::string& userText) {
    if (busy_.exchange(true)) return false;
    if (worker_.joinable()) worker_.join();
    cancel_ = false;
    Json user; user.set("role", "user"); user.set("content", userText);
    messages_.push_back(user);
    worker_ = std::thread([this, s, workspace] { run(s, workspace); });
    return true;
}

void Agent::run(Settings s, fs::path root) {
    using clock = std::chrono::steady_clock;
    auto fail = [&](const std::string& msg) {
        AgentEvent e; e.kind = AgentEvent::Kind::Error; e.text = msg; e.ok = false; post(e);
    };
    try {
        if (s.apiKey.empty()) { const char* k = std::getenv("OPENAI_API_KEY"); if (k) s.apiKey = k; }
        if (s.baseUrl.empty()) { fail("Set the API base URL in Settings."); AgentEvent d; post(d); busy_ = false; return; }

        Json tools = Json::parse(kTools);
        std::string system = kSystem;
        std::string agentsMd;
        if (readFile(root / "AGENTS.md", agentsMd)) system += "\nThe repository's AGENTS.md follows.\n\n" + clip(agentsMd, 9000);

        std::error_code ec;

        for (int step = 0; step < s.maxSteps && !cancel_; ++step) {
            // ---- request --------------------------------------------------------
            Json body; body.set("model", s.model);
            Json::Array msgs;
            Json sys; sys.set("role", "system"); sys.set("content", system);
            msgs.push_back(sys);
            for (const auto& m : messages_) msgs.push_back(m);
            body.set("messages", Json(msgs));
            body.set("tools", tools);
            body.set("tool_choice", "auto");

            HttpResponse hr = httpRequest("POST", apiUrl(s.baseUrl, "/chat/completions"), apiHeaders(s), body.dump(), s.timeoutSeconds);
            if (!hr.error.empty()) { fail("Request failed: " + hr.error); break; }
            const std::string& resp = hr.body;
            std::string code = std::to_string(hr.status);

            Json j;
            try { j = Json::parse(resp); }
            catch (const std::exception& e) { fail("HTTP " + code + ": not JSON (" + std::string(e.what()) + "): " + clip(resp, 600)); break; }
            if (j.has("error")) {
                const Json& er = j["error"];
                fail("HTTP " + code + ": " + (er.isObject() ? er.str("message", "error") : clip(j.dump(), 600)));
                break;
            }
            const Json& msg = j["choices"][0]["message"];
            if (!msg.isObject()) { fail("Unexpected response: " + clip(resp, 600)); break; }

            Json stored; stored.set("role", "assistant");
            stored.set("content", msg["content"].isString() ? msg["content"].asString() : std::string());
            bool hasCalls = msg["tool_calls"].isArray() && msg["tool_calls"].size() > 0;
            if (hasCalls) stored.set("tool_calls", msg["tool_calls"]);
            messages_.push_back(stored);

            if (msg["content"].isString() && !msg["content"].asString().empty()) {
                AgentEvent e; e.kind = AgentEvent::Kind::Assistant; e.text = msg["content"].asString(); post(e);
            }
            if (!hasCalls) break;   // final answer

            // ---- tools ----------------------------------------------------------
            for (const Json& call : msg["tool_calls"].asArray()) {
                std::string id = call.str("id", ""), name = call["function"].str("name", "");
                std::string rawArgs = call["function"].str("arguments", "{}");
                AgentEvent st; st.kind = AgentEvent::Kind::ToolStart; st.id = id; st.name = name; st.text = rawArgs; post(st);

                auto t0 = clock::now();
                bool ok = true; std::string out;
                if (cancel_) { ok = false; out = "cancelled by the user"; }
                else {
                    try {
                        Json args = Json::parse(rawArgs.empty() ? "{}" : rawArgs);
                        fs::path p; std::string err;
                        if (name == "read_file") {
                            if (!resolve(root, args.str("path", ""), false, p, err)) { ok = false; out = err; }
                            else if (!readFile(p, out)) { ok = false; out = "cannot read " + args.str("path", ""); }
                            else out = clip(out);
                        } else if (name == "write_file") {
                            std::string content = args.str("content", "");
                            if (!resolve(root, args.str("path", ""), true, p, err)) { ok = false; out = err; }
                            else if (content.size() > kMaxFile) { ok = false; out = "file too large"; }
                            else {
                                fs::create_directories(p.parent_path(), ec);
                                std::ofstream f(p, std::ios::binary);
                                if (!f) { ok = false; out = "cannot write " + args.str("path", ""); }
                                else {
                                    f << content; f.close();
                                    out = "wrote " + std::to_string(content.size()) + " bytes to " + args.str("path", "");
                                    AgentEvent fw; fw.kind = AgentEvent::Kind::FileWritten; fw.name = toUtf8(p); post(fw);
                                }
                            }
                        } else if (name == "list_files") {
                            std::string ext = args.str("extension", "");
                            if (!resolve(root, args.str("path", "."), false, p, err)) { ok = false; out = err; }
                            else {
                                fs::path base = fs::weakly_canonical(root, ec);
                                int n = 0;
                                for (auto it = fs::recursive_directory_iterator(p, fs::directory_options::skip_permission_denied, ec);
                                     !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
                                    fs::path rel = it->path().lexically_relative(p);
                                    std::string first = rel.empty() ? "" : toUtf8(*rel.begin());
                                    if (first == ".git" || first.rfind("build", 0) == 0) { it.disable_recursion_pending(); continue; }
                                    if (!it->is_regular_file(ec)) continue;
                                    if (!ext.empty() && toUtf8(it->path().extension()) != ext) continue;
                                    out += toUtf8(it->path().lexically_relative(base)) + "\n";
                                    if (++n >= 300) { out += "...[300 entries shown]\n"; break; }
                                }
                                if (out.empty()) out = "(no files)";
                            }
                        } else if (name == "check_script") {
                            if (!resolve(root, args.str("script", ""), false, p, err)) { ok = false; out = err; }
                            else { HostResult h = hostList(s, root, toUtf8(p)); ok = h.ok; out = clip(h.output.empty() ? "(no output)" : h.output); }
                        } else if (name == "render_still") {
                            if (!resolve(root, args.str("script", ""), false, p, err)) { ok = false; out = err; }
                            else {
                                fs::path png = root / "out" / "studio" / "preview.png";
                                int frame = args.integer("frame", 0);
                                HostResult h = hostStill(s, root, toUtf8(p), args.str("composition", ""), frame, toUtf8(png));
                                ok = h.ok; out = clip(h.output);
                                if (ok) {
                                    AgentEvent sr; sr.kind = AgentEvent::Kind::StillReady; sr.name = toUtf8(png);
                                    sr.text = args.str("composition", ""); sr.seconds = (float)frame; post(sr);
                                }
                            }
                        } else { ok = false; out = "unknown tool: " + name; }
                    } catch (const std::exception& e) { ok = false; out = std::string("tool error: ") + e.what(); }
                }
                float secs = std::chrono::duration<float>(clock::now() - t0).count();
                AgentEvent en; en.kind = AgentEvent::Kind::ToolEnd; en.id = id; en.name = name; en.text = out; en.ok = ok; en.seconds = secs; post(en);

                Json tm; tm.set("role", "tool"); tm.set("tool_call_id", id); tm.set("content", ok ? out : "ERROR: " + out);
                messages_.push_back(tm);
            }
            if (step + 1 == s.maxSteps) fail("Stopped after " + std::to_string(s.maxSteps) + " steps. Send another message to continue.");
        }
        if (cancel_) fail("Stopped.");
    } catch (const std::exception& e) {
        fail(std::string("agent error: ") + e.what());
    }
    AgentEvent d; d.kind = AgentEvent::Kind::Done; post(d);
    busy_ = false;
}

} // namespace studio
