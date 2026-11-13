// Raymotion Studio -- an agentic editor for raymotion compositions, built on CUI.
//
// Panels (dockable): Code (the example gallery, then the editor with its problems), Brief
// (Markdown preview/edit), Assets, Preview (stills, a scrubber and silent draft playback) and
// Chat (the agent). The studio does not link the renderer: stills, draft frames and the live
// window come from the raymotion_script host, started as a child process, so a script that
// crashes or hangs never takes the editor with it.
#include <cui/cui.h>
#include <cui/cui_app.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "agent.hpp"
#include "proc.hpp"
#include "thumb.hpp"
#include "rmtn/json.hpp"

static cui_app* gApp = nullptr;

namespace fs = std::filesystem;
using namespace studio;
using rmtn::Json;

namespace {

constexpr size_t kCodeCap = 512 * 1024;
constexpr size_t kBriefCap = 256 * 1024;
constexpr size_t kMaxAssets = 4000;
constexpr int kThumbWidth = 480;
constexpr int kThumbJobs = 3;               // thumbnails rendered at once

enum Panel { kBrief, kCode, kAssets, kPreview, kChat, kPanels };

// ------------------------------------------------------------------ helpers
bool readText(const fs::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss; ss << f.rdbuf();
    out = ss.str();
    return true;
}
bool writeText(const fs::path& p, const std::string& s) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary);
    if (!f) return false;
    f << s;
    return (bool)f;
}
std::string lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}
std::string lowerExt(const fs::path& p) { return lower(toUtf8(p.extension())); }
void copyTo(char* dst, size_t cap, const std::string& s) { snprintf(dst, cap, "%s", s.c_str()); }
std::vector<const char*> cstrs(const std::vector<std::string>& v) {
    std::vector<const char*> out;
    for (const auto& s : v) out.push_back(s.c_str());
    return out;
}
std::string relTo(const fs::path& root, const fs::path& p) {
    std::error_code ec;
    fs::path r = fs::weakly_canonical(p, ec).lexically_relative(fs::weakly_canonical(root, ec));
    std::string s = toUtf8(r.empty() ? p : r);
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}
std::string trim(std::string s) {
    while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
    size_t i = 0;
    while (i < s.size() && isspace((unsigned char)s[i])) ++i;
    return s.substr(i);
}
// A file name from what someone typed: lower case, digits, '-' and '_'.
std::string slugify(const std::string& in) {
    std::string out;
    for (char c : trim(in)) {
        if (isalnum((unsigned char)c)) out += (char)tolower((unsigned char)c);
        else if (c == '-' || c == '_') out += c;
        else if ((c == ' ' || c == '.') && !out.empty() && out.back() != '-') out += '-';
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out;
}
std::string seconds(int frame, int fps) {
    char b[32];
    snprintf(b, sizeof b, "%.2f s", fps > 0 ? (double)frame / fps : 0.0);
    return b;
}

fs::path settingsPath() {
    const char* base = nullptr;
#ifdef _WIN32
    base = std::getenv("APPDATA");
    if (base) return fs::path(base) / "raymotion-studio" / "settings.json";
#endif
    base = std::getenv("XDG_CONFIG_HOME");
    if (base) return fs::path(base) / "raymotion-studio" / "settings.json";
    base = std::getenv("HOME");
    return fs::path(base ? base : ".") / ".config" / "raymotion-studio" / "settings.json";
}

// What "File > New script" writes. NAME becomes the composition's name.
const char* kBlankScript =
    "// NAME -- One sentence about what this shows.\n"
    "// Category: Mine\n"
    "//\n"
    "// Frames are pure functions of the frame number: no global variables, nothing accumulates.\n"
    "\n"
    "uint paper()  { return 0x101820; }\n"
    "uint ink()    { return 0xf2f4ff; }\n"
    "uint accent() { return 0x47dad8; }\n"
    "\n"
    "void configure(Composition &inout c) {\n"
    "    c.name = \"NAME\";\n"
    "    c.w = 1280; c.h = 720; c.fps = 30;\n"
    "    c.duration = 120;               // frames\n"
    "    c.background = paper();\n"
    "    scene(0, c.duration, \"title\");  // start, length, the function that draws it\n"
    "}\n"
    "\n"
    "// 0 -> 1 over `length` frames starting at `from`, eased.\n"
    "double reveal(int t, int from, int length) {\n"
    "    return ease(E_OUT_CUBIC, clamp01(double(t - from) / length));\n"
    "}\n"
    "\n"
    "void title(int t) {\n"
    "    double k = reveal(t, 0, 24);\n"
    "    draw::rect(140, 372, float(1000 * k), 6, accent());\n"
    "    draw::text(\"NAME\", 140, float(352 - 20 * (1 - k)), 72, ink(), float(k), 0, 1);\n"
    "    draw::text(\"Edit, save, and the preview follows.\", 140, 400, 30, ink(), float(0.6 * reveal(t, 12, 20)));\n"
    "}\n";

struct ChatItem {
    enum class Kind { User, Assistant, Tool, Error } kind = Kind::User;
    std::string text;                       // message text, or the tool's detail line
    std::string id, name, input, output;    // tool call
    cui_task_status status = CUI_TASK_RUNNING;
    float seconds = 0;
};

struct CompInfo { std::string name; int w = 0, h = 0, fps = 30, frames = 1; };

// One row of the host's `list` output: "name   1280x720 @30fps  5.0s  audio=0".
std::vector<CompInfo> parseList(const std::string& output) {
    std::vector<CompInfo> out;
    std::istringstream in(output);
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("fps") == std::string::npos) continue;
        std::istringstream ls(line);
        std::string name, size, fpsTok, secTok;
        ls >> name >> size >> fpsTok >> secTok;
        CompInfo c; c.name = name;
        sscanf(size.c_str(), "%dx%d", &c.w, &c.h);
        c.fps = atoi(fpsTok.c_str() + (!fpsTok.empty() && fpsTok[0] == '@' ? 1 : 0));
        if (c.fps <= 0) c.fps = 30;
        // The list prints seconds to one decimal, so this can be a frame or two off; the
        // host's "frame out of range" answer and a finished draft render both correct it.
        c.frames = std::max(1, (int)(atof(secTok.c_str()) * c.fps + 0.5));
        if (!name.empty()) out.push_back(c);
    }
    return out;
}

struct Diag { cui_severity severity = CUI_SEVERITY_ERROR; std::string message, file; int line = 0, column = 0; };

// The host reports "file:line:col: message" from the compiler and "file:line in function:
// message" from a running script. Anything else it says on failure becomes one plain row.
std::vector<Diag> parseDiagnostics(const std::string& output, const fs::path& root) {
    static const std::regex compile(R"(^(?:raymotion: )?(.+?):(\d+):(\d+): (.*)$)");
    static const std::regex runtime(R"(^(?:raymotion: )?(.+?):(\d+) in (.+?): (.*)$)");
    std::vector<Diag> out;
    std::istringstream in(output);
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        std::smatch m;
        Diag d;
        if (std::regex_match(line, m, compile)) {
            d.file = relTo(root, fromUtf8(m[1])); d.line = atoi(m[2].str().c_str()); d.column = atoi(m[3].str().c_str());
            d.message = m[4];
            if (d.message.rfind("Compiling ", 0) == 0) d.severity = CUI_SEVERITY_INFO;
        } else if (std::regex_match(line, m, runtime)) {
            d.file = relTo(root, fromUtf8(m[1])); d.line = atoi(m[2].str().c_str());
            d.message = m[4].str() + "  (in " + m[3].str() + ")";
        } else {
            if (line.rfind("raymotion: ", 0) == 0) line = line.substr(11);
            if (line[0] == '[' || line.rfind("wrote ", 0) == 0) continue;   // progress lines
            d.message = line;
        }
        out.push_back(d);
    }
    return out;
}

struct Example {
    std::string rel, stem, title, desc, category;
    int stillFrame = -1;                    // "// Still: N" in the header; otherwise 60% in
    enum class Thumb { None, Rendering, Ready, Failed } thumb = Thumb::None;
    fs::path thumbPath;
    cui_texture tex = 0;
    int tw = 0, th = 0;
};

// The first lines of an example say what it is:
//     // Title -- what it shows.
//     // Category: Name
bool readExampleHeader(const fs::path& p, Example& e) {
    std::ifstream f(p, std::ios::binary);
    std::string line;
    if (!f || !std::getline(f, line)) return false;
    line = trim(line);
    size_t sep = line.find(" -- ");
    if (line.rfind("// ", 0) != 0 || sep == std::string::npos) return false;
    e.title = trim(line.substr(3, sep - 3));
    e.desc = trim(line.substr(sep + 4));
    for (int i = 0; i < 6 && std::getline(f, line); ++i) {
        line = trim(line);
        if (line.rfind("// Category:", 0) == 0) e.category = trim(line.substr(12));
        else if (line.rfind("// Still:", 0) == 0) e.stillFrame = atoi(line.c_str() + 9);
    }
    if (e.category.empty()) e.category = "Other";
    return !e.title.empty();
}

// ------------------------------------------------------------------ the app
struct App {
    Settings set;
    fs::path root;
    Agent agent;
    cui_ctx* ctx = nullptr;

    // work finished off the UI thread is applied on it, in order
    struct Queue { std::mutex mu; std::vector<std::function<void()>> items; std::atomic<int> running{0}; };
    std::shared_ptr<Queue> queue = std::make_shared<Queue>();

    // panels and dialogs
    bool open[kPanels] = { true, true, true, true, true };
    const char* panelNames[kPanels] = { "Brief", "Code", "Assets", "Preview", "Chat" };
    std::string savedLayout;                 // from the last session
    bool showSettings = false, showPalette = false, showNewScript = false, showNewBrief = false, showConfirm = false;
    std::function<void()> afterConfirm;      // what the unsaved-changes dialog was guarding
    char newName[64] = "";
    int newFrom = 0;                         // 0 blank, 1 a copy of the open script
    std::string newError;

    // examples
    std::vector<Example> examples;
    std::vector<std::string> categories;     // in gallery order, "All" first
    bool showGallery = true;                 // the Code panel shows the gallery instead of the editor
    char exampleFilter[96] = "";
    int categoryIdx = 0;
    int thumbsRunning = 0;
    float galleryW = 720;

    // code
    std::vector<char> code = std::vector<char>(kCodeCap, 0);
    fs::path codePath;
    bool codeDirty = false;
    std::vector<std::string> scripts;
    int scriptIdx = -1;                      // -1: nothing open, the dropdown shows its placeholder
    std::string output = "Nothing has run yet.";
    std::vector<Diag> diags;
    std::vector<cui_text_mark> marks;        // the lines with errors, until the text changes
    int lowerTab = 0;                        // 0 problems, 1 output
    float codeSplit = 0.74f;
    bool checking = false, checkOk = false, checkedOnce = false;
    int caretLine = 1, caretCol = 1;
    bool exampleNote = true;                 // the "this is a shipped example" banner

    // brief
    std::vector<char> brief = std::vector<char>(kBriefCap, 0);
    fs::path briefPath;
    bool briefDirty = false;
    int briefTab = 0;                        // 0 preview, 1 edit
    std::vector<std::string> briefs;
    int briefIdx = -1;

    // assets
    std::vector<std::string> assets;         // workspace-relative
    std::vector<std::string> assetShown;
    char assetFilter[128] = "";
    int assetSel = -1;
    cui_texture assetTex = 0;
    int assetW = 0, assetH = 0;
    float assetBoxW = 300;
    float assetSplit = 0.5f;

    // preview
    std::vector<CompInfo> comps;
    std::vector<std::string> compNames;
    int compIdx = 0;
    float frame = 0;
    bool autoPreview = true;
    cui_texture still = 0;
    int stillW = 0, stillH = 0;
    cui_image_view view{};
    bool rendering = false;
    std::string previewNote;                 // under the picture: what is showing
    std::string previewError;
    // Full-size stills by composition: frame -> png. Dropped whenever the script changes.
    std::map<std::string, std::map<int, fs::path>> stills;
    uint64_t cacheVer = 1;
    fs::path shownPath;
    std::string failedKey;                   // the still that failed, so it is not retried in a loop
    double frameMovedAt = 0;
    int lastWant = -1;
    float openAt = -1;                       // where in the film a script opens (0..1), once its length is known
    // Draft frames (half size, no sound) for playback and instant scrubbing.
    bool playing = false, drafting = false;
    double playPos = 0;
    std::string draftComp;
    uint64_t draftVer = 0;
    int draftReady = 0;                      // frames of the draft that are complete on disk
    bool exporting = false;

    // chat
    std::vector<ChatItem> chat;
    std::vector<char> input = std::vector<char>(8192, 0);
    bool scrollChat = false;

    // settings buffers
    char urlBuf[256], keyBuf[256], modelBuf[128], hostBuf[512], rootBuf[512];
    int themeIdx = 1;
    float stepsF = 16;
    std::string settingsNote;
    bool settingsNoteError = false;
    std::vector<std::string> models;         // from GET /models
    int modelIdx = -1;
    std::string modelNote;
    bool loadingModels = false;

    struct Toast { std::string title, detail; cui_tone tone = CUI_TONE_NEUTRAL; };
    std::vector<Toast> toasts;               // shown next frame
    void toast(const std::string& title, cui_tone tone = CUI_TONE_NEUTRAL, const std::string& detail = {}) {
        toasts.push_back({ title, detail, tone });
    }

    ~App() {
        agent.cancel();
        // Wait for in-flight jobs so no thread outlives the objects it touches.
        for (int i = 0; i < 600 && queue->running.load() > 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // ---- settings and session ----------------------------------------------------
    Settings currentSettings() const {
        Settings s = set;
        s.baseUrl = urlBuf; s.apiKey = keyBuf; s.model = modelBuf; s.host = hostBuf; s.maxSteps = (int)stepsF;
        return s;
    }
    Settings hostSettings() const { Settings s = set; s.host = hostBuf; s.apiKey.clear(); return s; }
    bool agentReady() const {
        if (keyBuf[0] || std::getenv("OPENAI_API_KEY")) return true;
        return std::string(urlBuf).find("api.openai.com") == std::string::npos;   // local servers need no key
    }
    // Asks the server which models it has, using the values typed in Settings (saved or not).
    void loadModels() {
        if (loadingModels) return;
        loadingModels = true;
        modelNote = "Loading...";
        Settings s = currentSettings();
        async([this, s]() -> std::function<void()> {
            ModelList m = fetchModels(s);
            return [this, m] {
                loadingModels = false;
                if (!m.error.empty()) { modelNote = m.error; return; }
                models = m.ids;
                modelIdx = -1;
                for (size_t i = 0; i < models.size(); ++i) if (models[i] == modelBuf) modelIdx = (int)i;
                modelNote = std::to_string(models.size()) + " models";
            };
        });
    }
    Json session;                            // what was open last time
    void loadSettings() {
        std::string text;
        if (readText(settingsPath(), text)) {
            try {
                Json j = Json::parse(text);
                set.baseUrl = j.str("baseUrl", set.baseUrl);
                set.apiKey = j.str("apiKey", "");
                set.model = j.str("model", set.model);
                set.host = j.str("host", "");
                set.maxSteps = j.integer("maxSteps", set.maxSteps);
                set.darkTheme = j.flag("dark", true);
                std::string w = j.str("workspace", "");
                if (!w.empty() && fs::exists(fromUtf8(w))) root = fromUtf8(w);
                if (j.has("session") && j["session"].isObject()) session = j["session"];
            } catch (const std::exception&) {}
        }
        syncBuffers();
    }
    void syncBuffers() {
        copyTo(urlBuf, sizeof urlBuf, set.baseUrl);
        copyTo(keyBuf, sizeof keyBuf, set.apiKey);
        copyTo(modelBuf, sizeof modelBuf, set.model);
        copyTo(hostBuf, sizeof hostBuf, set.host);
        copyTo(rootBuf, sizeof rootBuf, toUtf8(root));
        themeIdx = set.darkTheme ? 1 : 0;
        stepsF = (float)set.maxSteps;
    }
    // Writes the applied settings (not what is being typed in the dialog) and the session.
    bool writeSettingsFile() {
        Json j;
        j.set("baseUrl", set.baseUrl); j.set("apiKey", set.apiKey); j.set("model", set.model);
        j.set("host", set.host); j.set("maxSteps", set.maxSteps); j.set("dark", set.darkTheme);
        j.set("workspace", toUtf8(root));
        Json s;
        s.set("script", codePath.empty() ? std::string() : relTo(root, codePath));
        s.set("brief", briefPath.empty() ? std::string() : relTo(root, briefPath));
        s.set("gallery", showGallery);
        s.set("frame", (int)frame);
        s.set("auto", autoPreview);
        char layout[2048] = "";
        if (ctx && cui_dock_save(ctx, "main", layout, sizeof layout) > 0 && strlen(layout) + 1 < sizeof layout) s.set("layout", std::string(layout));
        else if (!savedLayout.empty()) s.set("layout", savedLayout);
        j.set("session", s);
        return writeText(settingsPath(), j.dump());
    }
    void saveSettings() {
        // The folder field may have been typed into without pressing Enter.
        std::error_code ec;
        if (rootBuf[0] && fs::weakly_canonical(fromUtf8(rootBuf), ec) != root && !setWorkspace(rootBuf)) {
            toast("Settings not saved", CUI_TONE_DANGER, settingsNote);
            return;
        }
        set.baseUrl = urlBuf; set.apiKey = keyBuf; set.model = modelBuf; set.host = hostBuf;
        set.maxSteps = (int)stepsF; set.darkTheme = themeIdx == 1;
        settingsNoteError = !writeSettingsFile();
        settingsNote = settingsNoteError ? "Could not write " + toUtf8(settingsPath()) : "Saved";
    }
    void restoreSession() {
        savedLayout = session.str("layout", "");
        autoPreview = session.flag("auto", true);
        std::string s = session.str("script", ""), b = session.str("brief", "");
        std::error_code ec;
        if (!b.empty() && fs::is_regular_file(root / fromUtf8(b), ec)) openBrief(root / fromUtf8(b));
        if (!s.empty() && fs::is_regular_file(root / fromUtf8(s), ec)) {
            openScript(root / fromUtf8(s));
            frame = (float)std::max(0, session.integer("frame", 0));
            showGallery = session.flag("gallery", false);
        }
    }
    bool setWorkspace(const std::string& dir) {
        fs::path p = fromUtf8(dir);
        std::error_code ec;
        if (!fs::is_directory(p, ec)) { settingsNote = "Not a folder: " + dir; settingsNoteError = true; return false; }
        root = fs::weakly_canonical(p, ec);
        copyTo(rootBuf, sizeof rootBuf, toUtf8(root));
        codePath.clear(); briefPath.clear(); code[0] = 0; brief[0] = 0; codeDirty = briefDirty = false; scriptIdx = briefIdx = -1;
        comps.clear(); compNames.clear(); diags.clear(); marks.clear(); checkOk = checkedOnce = false; showGallery = true;
        clearFrames();
        rescan();
        settingsNote = "Workspace: " + toUtf8(root); settingsNoteError = false;
        return true;
    }

    // ---- background work -------------------------------------------------------
    // `work` runs on a thread; the closure it returns runs on the UI thread.
    void async(std::function<std::function<void()>()> work) {
        auto q = queue;
        q->running++;
        std::thread([q, work = std::move(work)] {
            std::function<void()> apply;
            try { apply = work(); } catch (...) {}
            { std::lock_guard<std::mutex> lk(q->mu); if (apply) q->items.push_back(std::move(apply)); }
            q->running--;
        }).detach();
    }
    void pumpQueue() {
        std::vector<std::function<void()>> items;
        { std::lock_guard<std::mutex> lk(queue->mu); items.swap(queue->items); }
        for (auto& f : items) f();
    }
    bool busy() const { return queue->running.load() > 0 || agent.busy(); }

    // ---- files -----------------------------------------------------------------
    fs::path relPath(const fs::path& p) const { return p.empty() ? p : fromUtf8(relTo(root, p)); }
    bool isShippedExample(const fs::path& p) const {
        return !p.empty() && relTo(root, p).rfind("examples/scripts/", 0) == 0;
    }
    void rescan() {
        scripts.clear(); briefs.clear(); assets.clear();
        std::error_code ec;
        auto walk = [&](const fs::path& dir, auto&& fn) {
            if (!fs::is_directory(dir, ec)) return;
            for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
                 !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
                std::string n = toUtf8(it->path().filename());
                if (it->is_directory(ec) && (n == ".git" || n.rfind("build", 0) == 0 || n == "out" || n == "third_party")) { it.disable_recursion_pending(); continue; }
                if (it->is_regular_file(ec)) fn(it->path());
            }
        };
        // A .cpp is offered only when the host can run it by itself: it registers its own compositions.
        auto runnableCpp = [](const fs::path& p) {
            std::string t;
            return readText(p, t) && t.find("registerComps") != std::string::npos;
        };
        std::vector<std::string> mine, shipped, native;
        for (const char* dir : { "studio-projects", "scripts" })
            walk(root / dir, [&](const fs::path& p) {
                std::string e = lowerExt(p);
                if (e == ".as" || (e == ".cpp" && runnableCpp(p))) mine.push_back(relTo(root, p));
            });
        walk(root / "examples", [&](const fs::path& p) {
            std::string e = lowerExt(p);
            if (e == ".as") shipped.push_back(relTo(root, p));
            else if (e == ".cpp" && runnableCpp(p)) native.push_back(relTo(root, p));
        });
        for (auto* v : { &mine, &shipped, &native }) { std::sort(v->begin(), v->end()); scripts.insert(scripts.end(), v->begin(), v->end()); }
        walk(root / "docs", [&](const fs::path& p) { if (lowerExt(p) == ".md" && toUtf8(p.filename()).find("-brief") != std::string::npos) briefs.push_back(relTo(root, p)); });
        walk(root / "assets", [&](const fs::path& p) { if (assets.size() < kMaxAssets) assets.push_back(relTo(root, p)); });
        std::sort(briefs.begin(), briefs.end());
        std::sort(assets.begin(), assets.end());
        applyAssetFilter();
        syncSelects();
        scanExamples();
    }
    // keep the dropdowns pointing at the open files
    void syncSelects() {
        scriptIdx = briefIdx = -1;
        std::string s = codePath.empty() ? "" : relTo(root, codePath), b = briefPath.empty() ? "" : relTo(root, briefPath);
        for (size_t i = 0; i < scripts.size(); ++i) if (scripts[i] == s) scriptIdx = (int)i;
        for (size_t i = 0; i < briefs.size(); ++i) if (briefs[i] == b) briefIdx = (int)i;
    }

    void applyAssetFilter() {
        assetShown.clear();
        std::string f = lower(assetFilter);
        for (const auto& a : assets)
            if (f.empty() || lower(a).find(f) != std::string::npos) assetShown.push_back(a);
        assetSel = -1;
    }

    // ---- examples ----------------------------------------------------------------
    void scanExamples() {
        std::map<std::string, Example> old;
        for (auto& e : examples) old[e.rel] = e;
        examples.clear();
        std::error_code ec;
        fs::path dir = root / "examples" / "scripts";
        if (fs::is_directory(dir, ec))
            for (const auto& ent : fs::directory_iterator(dir, ec)) {
                if (lowerExt(ent.path()) != ".as") continue;
                Example e;
                if (!readExampleHeader(ent.path(), e)) continue;
                e.rel = relTo(root, ent.path());
                e.stem = toUtf8(ent.path().stem());
                // The thumbnail belongs to this version of the file.
                auto when = fs::last_write_time(ent.path(), ec).time_since_epoch().count();
                auto size = fs::file_size(ent.path(), ec);
                e.thumbPath = root / "out" / "studio" / "thumbs" / (e.stem + "-" + std::to_string((unsigned long long)when % 100000000ull) + "-" + std::to_string(size) + ".png");
                auto it = old.find(e.rel);
                if (it != old.end() && it->second.thumbPath == e.thumbPath) { e.thumb = it->second.thumb; e.tex = it->second.tex; e.tw = it->second.tw; e.th = it->second.th; it->second.tex = 0; }
                else if (fs::exists(e.thumbPath, ec)) e.thumb = Example::Thumb::Ready;
                examples.push_back(e);
            }
        for (auto& [rel, e] : old) if (e.tex && ctx) cui_image_destroy(ctx, e.tex);
        // "Start here" leads; the rest follow in the order a newcomer meets them.
        static const char* order[] = { "Start here", "Motion", "Drawing", "Text", "Charts", "Maps", "3D", "Characters",
                                       "Physics and particles", "Audio and video" };
        auto rank = [&](const std::string& c) {
            for (int i = 0; i < (int)(sizeof order / sizeof *order); ++i) if (c == order[i]) return i;
            return 100;
        };
        std::stable_sort(examples.begin(), examples.end(), [&](const Example& a, const Example& b) {
            int ra = rank(a.category), rb = rank(b.category);
            if (ra != rb) return ra < rb;
            if (a.category != b.category) return a.category < b.category;
            return ra == 0 ? false : a.title < b.title;   // "Start here" keeps its own order below
        });
        // hello first, whatever its title sorts as
        auto hello = std::find_if(examples.begin(), examples.end(), [](const Example& e) { return e.stem == "hello"; });
        if (hello != examples.end()) std::rotate(examples.begin(), hello, hello + 1);
        categories = { "All" };
        for (const auto& e : examples)
            if (std::find(categories.begin(), categories.end(), e.category) == categories.end()) categories.push_back(e.category);
        categoryIdx = std::clamp(categoryIdx, 0, (int)categories.size() - 1);
    }
    // Renders missing thumbnails a few at a time: list, one still 60% in, then shrink it.
    void pumpThumbs() {
        for (size_t i = 0; i < examples.size() && thumbsRunning < kThumbJobs; ++i) {
            Example& e = examples[i];
            if (e.thumb != Example::Thumb::None) continue;
            e.thumb = Example::Thumb::Rendering;
            ++thumbsRunning;
            Settings s = hostSettings();
            fs::path r = root, script = root / fromUtf8(e.rel), thumb = e.thumbPath;
            std::string rel = e.rel;
            int wanted = e.stillFrame;
            async([this, s, r, script, thumb, rel, wanted]() -> std::function<void()> {
                bool ok = false;
                std::error_code ec;
                fs::create_directories(thumb.parent_path(), ec);
                HostResult l = hostList(s, r, toUtf8(script));
                std::vector<CompInfo> cs = l.ok ? parseList(l.output) : std::vector<CompInfo>{};
                if (!cs.empty()) {
                    int f = wanted >= 0 ? wanted : cs[0].frames * 6 / 10;
                    f = std::clamp(f, 0, std::max(0, cs[0].frames - 2));
                    fs::path full = thumb; full += ".full.png";
                    HostResult h = hostStill(s, r, toUtf8(script), cs[0].name, f, toUtf8(full));
                    ok = h.ok && makeThumb(toUtf8(full), toUtf8(thumb), kThumbWidth);
                    fs::remove(full, ec);
                }
                return [this, rel, ok] {
                    --thumbsRunning;
                    for (auto& e : examples) if (e.rel == rel) e.thumb = ok ? Example::Thumb::Ready : Example::Thumb::Failed;
                };
            });
        }
    }
    void openExample(const Example& e) {
        fs::path p = root / fromUtf8(e.rel);
        // Open on the frame the gallery showed, not on an empty first frame.
        int still = e.stillFrame;
        guardUnsaved([this, p, still] { openScript(p); exampleNote = true; if (still >= 0) frame = (float)still; else openAt = 0.6f; });
    }

    // ---- open / save -------------------------------------------------------------
    bool loadInto(std::vector<char>& buf, const fs::path& p, const char* what) {
        std::string t;
        if (!readText(p, t)) { toast(std::string("Cannot read the ") + what, CUI_TONE_DANGER, toUtf8(p)); return false; }
        if (t.size() + 1 > buf.size()) { toast(std::string("The ") + what + " is too large for the editor", CUI_TONE_DANGER); return false; }
        memcpy(buf.data(), t.data(), t.size());
        buf[t.size()] = 0;
        return true;
    }
    void openScript(const fs::path& p) {
        if (!loadInto(code, p, "script")) return;
        codePath = p; codeDirty = false; showGallery = false;
        clearFrames(); comps.clear(); compNames.clear(); compIdx = 0; frame = 0; playing = false;
        diags.clear(); marks.clear(); checkOk = checkedOnce = false; view = {};
        output = "Checking...";
        syncSelects();
        if (scriptIdx < 0) { rescan(); }
        check();
    }
    void openBrief(const fs::path& p) {
        if (loadInto(brief, p, "brief")) { briefPath = p; briefDirty = false; syncSelects(); }
    }
    bool saveCode() {
        if (codePath.empty()) return false;
        if (!writeText(codePath, code.data())) { toast("Cannot write " + toUtf8(codePath.filename()), CUI_TONE_DANGER); return false; }
        codeDirty = false; clearFrames();
        return true;
    }
    bool saveBrief() {
        if (briefPath.empty()) return false;
        if (!writeText(briefPath, brief.data())) { toast("Cannot write " + toUtf8(briefPath.filename()), CUI_TONE_DANGER); return false; }
        briefDirty = false;
        return true;
    }
    // Ctrl+S: write what changed; a saved script is checked again and the preview follows.
    void saveAll() {
        bool c = codeDirty && saveCode();
        if (briefDirty) saveBrief();
        if (c) check();
    }
    // Runs `then` now, or after the person decides what to do with unsaved edits.
    void guardUnsaved(std::function<void()> then) {
        if (!codeDirty) { then(); return; }
        afterConfirm = std::move(then);
        showConfirm = true;
    }
    void newBrief(const std::string& typed) {
        std::string slug = slugify(typed);
        if (slug.empty()) { newError = "Give it a name: letters, digits and dashes."; return; }
        fs::path dst = root / "docs" / (slug + "-brief.md"), tpl = root / "skills" / "raymotion" / "templates" / "BRIEF.md";
        std::error_code ec;
        if (!fs::exists(dst, ec)) {
            std::string t;
            if (!readText(tpl, t)) t = "# " + slug + "\n\nConceit (one sentence):\n";
            if (!writeText(dst, t)) { newError = "Could not write " + relTo(root, dst); return; }
        }
        rescan(); openBrief(dst); briefTab = 1; open[kBrief] = true;
        showNewBrief = false; newError.clear();
    }
    void newScript(const std::string& typed, bool copyOpen) {
        std::string slug = slugify(typed);
        if (slug.empty()) { newError = "Give it a name: letters, digits and dashes."; return; }
        fs::path dst = root / "studio-projects" / (slug + ".as");
        std::error_code ec;
        if (fs::exists(dst, ec)) { newError = relTo(root, dst) + " already exists."; return; }
        std::string text;
        if (copyOpen && !codePath.empty()) text = code.data();
        else {
            text = kBlankScript;
            for (size_t at; (at = text.find("NAME")) != std::string::npos;) text.replace(at, 4, slug);
        }
        if (!writeText(dst, text)) { newError = "Could not write " + relTo(root, dst); return; }
        codeDirty = false;
        rescan(); openScript(dst); open[kCode] = true;
        showNewScript = false; newError.clear();
        toast("Created " + relTo(root, dst), CUI_TONE_SUCCESS);
    }
    void askNewScript(bool copyOpen) {
        newFrom = copyOpen && !codePath.empty() ? 1 : 0;
        copyTo(newName, sizeof newName, newFrom ? toUtf8(codePath.stem()) + "-copy" : "");
        newError.clear(); showNewScript = true;
    }

    // ---- check / render ----------------------------------------------------------
    std::string curComp() const { return comps.empty() ? "" : comps[(size_t)std::clamp(compIdx, 0, (int)comps.size() - 1)].name; }
    CompInfo* comp() { return comps.empty() ? nullptr : &comps[(size_t)std::clamp(compIdx, 0, (int)comps.size() - 1)]; }
    int maxFrame() { CompInfo* c = comp(); return c ? std::max(0, c->frames - 1) : 0; }
    fs::path framesDir() const { return root / "out" / "studio" / "frames"; }
    fs::path draftDir(const std::string& name) const { return root / "out" / (name + "_draft_frames"); }
    fs::path draftFrame(const std::string& name, int f) const {
        char b[32]; snprintf(b, sizeof b, "f%05d.png", f);
        return draftDir(name) / b;
    }
    bool draftValid() const { return draftVer == cacheVer && !draftComp.empty() && draftComp == curComp(); }
    void clearFrames() {
        stills.clear(); ++cacheVer; shownPath.clear(); failedKey.clear(); previewError.clear(); draftReady = 0;
        std::error_code ec;
        if (!rendering) fs::remove_all(framesDir(), ec);     // only this session's stills live there
    }
    void setDiagnostics(const std::string& text, bool failed) {
        diags = failed ? parseDiagnostics(text, root) : std::vector<Diag>{};
        marks.clear();
        // Tint the lines the errors are on, until the next edit moves them.
        std::string openRel = codePath.empty() ? "" : relTo(root, codePath);
        std::vector<int> lines;
        for (const auto& d : diags) if (d.severity == CUI_SEVERITY_ERROR && d.line > 0 && d.file == openRel) lines.push_back(d.line);
        std::sort(lines.begin(), lines.end());
        lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
        int line = 1, start = 0;
        size_t li = 0;
        for (int i = 0; li < lines.size(); ++i) {
            char c = code[(size_t)i];
            if (c == '\n' || c == 0) {
                if (line == lines[li]) { marks.push_back({ start, i, 6 }); ++li; }   // 6: the red of the series colours
                if (c == 0) break;
                ++line; start = i + 1;
            }
        }
        if (failed) lowerTab = 0;
    }
    void check() {
        if (codePath.empty() || checking) return;
        if (codeDirty) saveCode();
        checking = true;
        Settings s = hostSettings();
        fs::path r = root, script = codePath;
        async([this, s, r, script]() -> std::function<void()> {
            HostResult h = hostList(s, r, toUtf8(script));
            return [this, h, script] {
                checking = false;
                if (script != codePath) return;
                checkedOnce = true; checkOk = h.ok;
                output = trim(h.output).empty() ? std::string(h.ok ? "No output." : "The host said nothing. Is raymotion_script built? See Settings.") : h.output;
                setDiagnostics(h.output, !h.ok);
                if (!h.ok) { playing = false; return; }
                // Keep the place: the same composition and frame as before the edit.
                std::string was = curComp();
                comps = parseList(h.output);
                compNames.clear();
                compIdx = 0;
                for (size_t i = 0; i < comps.size(); ++i) { compNames.push_back(comps[i].name); if (comps[i].name == was) compIdx = (int)i; }
                if (openAt >= 0) { frame = (float)(int)(openAt * (float)maxFrame()); openAt = -1; }
                frame = std::clamp(frame, 0.f, (float)maxFrame());
                if (comps.empty()) { checkOk = false; diags.push_back({ CUI_SEVERITY_ERROR, "The script registered no compositions.", "", 0, 0 }); }
            };
        });
    }
    // One full-size frame. `manual` is the button: it saves first and says so when it fails.
    void renderStill(bool manual) {
        if (codePath.empty() || rendering) return;
        if (manual && codeDirty) { saveCode(); check(); }
        std::string name = curComp();
        int f = std::clamp((int)frame, 0, maxFrame());
        if (stills[name].count(f)) return;
        rendering = true;
        Settings s = hostSettings();
        fs::path r = root, script = codePath, png = framesDir() / (std::to_string(cacheVer) + "-" + name + "-" + std::to_string(f) + ".png");
        uint64_t ver = cacheVer;
        std::error_code mk; fs::create_directories(png.parent_path(), mk);
        async([this, s, r, script, png, name, f, ver]() -> std::function<void()> {
            HostResult h = hostStill(s, r, toUtf8(script), name, f, toUtf8(png));
            return [this, h, png, name, f, ver] {
                rendering = false;
                if (ver != cacheVer) return;                       // the script changed meanwhile
                if (h.ok) { stills[name][f] = png; previewError.clear(); return; }
                // "frame out of range [0,N)": the list's rounded duration was a frame long.
                int n = 0;
                size_t at = h.output.find("frame out of range [0,");
                if (at != std::string::npos && (n = atoi(h.output.c_str() + at + 22)) > 0) {
                    for (auto& c : comps) if (c.name == name) c.frames = n;
                    frame = std::min(frame, (float)(n - 1));
                    return;
                }
                failedKey = std::to_string(ver) + "/" + name + "/" + std::to_string(f);
                previewError = trim(h.output);
                output = h.output;
                setDiagnostics(h.output, true);
            };
        });
    }
    // Half-size frames of the whole composition, for playback and scrubbing without a wait.
    void renderDraft() {
        if (codePath.empty() || drafting || !checkOk || comps.empty()) return;
        std::string name = curComp();
        drafting = true; draftComp = name; draftVer = cacheVer; draftReady = 0;
        Settings s = hostSettings();
        fs::path r = root, script = codePath, dir = draftDir(name);
        uint64_t ver = cacheVer;
        async([this, s, r, script, dir, name, ver]() -> std::function<void()> {
            std::error_code ec;
            fs::remove_all(dir, ec);                               // frames of an older version
            ProcResult p = runCapture({ findHost(s, r), "render", name, "--script", toUtf8(script), "--draft", "--png-frames" }, toUtf8(r));
            return [this, p, name, ver] {
                drafting = false;
                if (ver != cacheVer || name != draftComp) { draftReady = 0; return; }   // stale: Play renders it again
                int n = 0;
                size_t at = p.output.rfind("rendered ");
                if (p.code == 0 && at != std::string::npos) n = atoi(p.output.c_str() + at + 9);
                if (n <= 0) {
                    playing = false; draftVer = 0;
                    previewError = trim(p.output).empty() ? "The draft render failed." : trim(p.output);
                    output = p.output; setDiagnostics(p.output, true);
                    return;
                }
                draftReady = n;
                for (auto& c : comps) if (c.name == name) c.frames = n;   // the exact length
            };
        });
    }
    // While the draft renders, a frame is complete once the next one exists.
    void pollDraft() {
        if (!drafting) return;
        std::error_code ec;
        for (int i = 0; i < 64 && fs::exists(draftFrame(draftComp, draftReady + 1), ec); ++i) ++draftReady;
    }
    void togglePlay() {
        if (codePath.empty() || !checkOk) return;
        playing = !playing;
        if (playing) {
            playPos = frame >= (float)maxFrame() ? 0 : frame;
            if (!draftValid()) renderDraft();
        }
    }
    void step(int by) {
        playing = false;
        frame = (float)std::clamp((int)frame + by, 0, maxFrame());
    }
    void liveWindow() {
        if (codePath.empty()) return;
        if (codeDirty) { saveCode(); check(); }
        std::vector<std::string> a = { findHost(hostSettings(), root), "preview" };
        if (!curComp().empty()) a.push_back(curComp());
        a.insert(a.end(), { "--script", toUtf8(codePath) });
        if (!spawnDetached(a, toUtf8(root))) toast("Could not start the live window", CUI_TONE_DANGER, "Is raymotion_script built? See Settings.");
    }
    void exportVideo() {
        if (codePath.empty() || exporting || !checkOk) return;
        if (codeDirty) { saveCode(); check(); }
        exporting = true;
        std::string name = curComp();
        Settings s = hostSettings();
        fs::path r = root, script = codePath;
        async([this, s, r, script, name]() -> std::function<void()> {
            ProcResult p = runCapture({ findHost(s, r), "render", name, "--script", toUtf8(script) }, toUtf8(r));
            return [this, p, name] {
                exporting = false;
                output = p.output;
                size_t at = p.output.rfind("-> ");
                if (p.code == 0 && at != std::string::npos) toast("Rendered " + trim(p.output.substr(at + 3)), CUI_TONE_SUCCESS, "In the workspace folder.");
                else { toast("The video render failed", CUI_TONE_DANGER, "See Output in the Code panel."); setDiagnostics(p.output, true); lowerTab = 1; }
            };
        });
    }

    void loadStill(const fs::path& png) {
        if (!ctx) return;
        if (still) cui_image_destroy(ctx, still);
        int w = 0, h = 0;
        still = cui_image_load(ctx, toUtf8(png).c_str(), &w, &h);
        // A draft is half the size of a still: keep the zoom when only the scale changed.
        if (still && stillW > 0 && w != stillW && view.zoom > 0) { view.zoom *= (float)stillW / (float)w; view.cx *= (float)w / (float)stillW; view.cy *= (float)h / (float)std::max(1, stillH); }
        stillW = w; stillH = h;
    }
    // Chooses what the Preview shows for the current frame and loads it when it changes:
    // the exact still, else the draft frame, else the nearest earlier picture.
    void syncPreview() {
        std::string name = curComp();
        int want = std::clamp((int)frame, 0, maxFrame());
        fs::path pick;
        std::string note;
        auto& m = stills[name];
        auto exact = m.find(want);
        bool draft = draftValid() && draftReady > 0;
        if (exact != m.end() && !playing) { pick = exact->second; note = "frame " + std::to_string(want); }
        else if (draft && want < draftReady) { pick = draftFrame(name, want); note = "frame " + std::to_string(want) + "  (draft)"; }
        else {
            auto it = m.upper_bound(want);
            int sf = it == m.begin() ? -1 : std::prev(it)->first;
            int df = draft ? std::min(want, draftReady - 1) : -1;
            if (sf >= 0 && sf >= df) { pick = m[sf]; note = "frame " + std::to_string(sf) + "  (waiting for " + std::to_string(want) + ")"; }
            else if (df >= 0) { pick = draftFrame(name, df); note = "frame " + std::to_string(df) + "  (draft, waiting for " + std::to_string(want) + ")"; }
        }
        if (pick.empty()) {
            if (still) { cui_image_destroy(ctx, still); still = 0; }
            shownPath.clear(); previewNote.clear();
            return;
        }
        if (pick != shownPath || !still) { loadStill(pick); shownPath = pick; }
        previewNote = note;
    }
    // Once a frame: playback, and the still for the frame the scrubber rests on.
    void pumpPreview(cui_ctx* ui) {
        pollDraft();
        CompInfo* c = comp();
        if (playing && c) {
            if (!draftValid() && !drafting) renderDraft();        // saved while playing: render again and carry on
            if (draftValid() && draftReady > 0) {
                playPos += (double)cui_delta_time(ui) * c->fps;
                int end = drafting ? draftReady : std::min(draftReady, c->frames);
                if (playPos >= end) playPos = drafting ? (double)std::max(0, end - 1) : 0;   // wait for the render, else loop
                frame = (float)(int)playPos;
            }
            cui_request_wakeup(ui, 1.f / (float)c->fps);
        }
        int want = std::clamp((int)frame, 0, maxFrame());
        if (want != lastWant) { lastWant = want; frameMovedAt = cui_time(ui); }
        if (autoPreview && !playing && checkOk && c && !codePath.empty() && !rendering && !checking) {
            std::string key = std::to_string(cacheVer) + "/" + c->name + "/" + std::to_string(want);
            bool settled = cui_time(ui) - frameMovedAt > 0.12;
            if (!stills[c->name].count(want) && key != failedKey) {
                if (settled) renderStill(false);
                else cui_request_wakeup(ui, 0.13f);
            }
        }
        syncPreview();
    }
    void selectAsset(int i) {
        assetSel = i;
        if (assetTex) { cui_image_destroy(ctx, assetTex); assetTex = 0; }
        if (i < 0 || i >= (int)assetShown.size()) return;
        std::string e = lowerExt(fromUtf8(assetShown[(size_t)i]));
        if (e == ".png" || e == ".jpg" || e == ".jpeg")
            assetTex = cui_image_load(ctx, toUtf8(root / fromUtf8(assetShown[(size_t)i])).c_str(), &assetW, &assetH);
    }

    // ---- chat ------------------------------------------------------------------
    void send(const std::string& text) {
        ChatItem u; u.kind = ChatItem::Kind::User; u.text = text; chat.push_back(u);
        if (codeDirty && saveCode()) check();
        if (briefDirty) saveBrief();
        // The agent sees which file is open, like a person pointing at the screen.
        std::string msg = text;
        if (!codePath.empty()) msg += "\n\n[open script: " + relTo(root, codePath) + "]";
        if (!briefPath.empty()) msg += "\n[open brief: " + relTo(root, briefPath) + "]";
        if (!agent.start(currentSettings(), root, msg)) { ChatItem e; e.kind = ChatItem::Kind::Error; e.text = "The agent is still working."; chat.push_back(e); }
        scrollChat = true;
    }
    void pumpAgent() {
        for (const AgentEvent& e : agent.drain()) {
            using K = AgentEvent::Kind;
            switch (e.kind) {
            case K::Assistant: { ChatItem c; c.kind = ChatItem::Kind::Assistant; c.text = e.text; chat.push_back(c); break; }
            case K::ToolStart: {
                ChatItem c; c.kind = ChatItem::Kind::Tool; c.id = e.id; c.name = e.name; c.input = e.text;
                try { Json a = Json::parse(e.text); c.text = a.str("path", a.str("script", "")); } catch (...) {}
                chat.push_back(c); break;
            }
            case K::ToolEnd:
                for (auto it = chat.rbegin(); it != chat.rend(); ++it)
                    if (it->kind == ChatItem::Kind::Tool && it->id == e.id) {
                        it->output = e.text; it->seconds = e.seconds;
                        it->status = e.ok ? CUI_TASK_DONE : CUI_TASK_FAILED; break;
                    }
                break;
            case K::FileWritten: onAgentWrote(fromUtf8(e.name)); break;
            case K::StillReady: {
                // The agent rendered a frame: move the scrubber there and show it.
                std::error_code ec;
                int f = (int)e.seconds;
                fs::path keep = framesDir() / (std::to_string(cacheVer) + "-agent-" + std::to_string(f) + ".png");
                fs::create_directories(keep.parent_path(), ec);
                fs::copy_file(fromUtf8(e.name), keep, fs::copy_options::overwrite_existing, ec);
                if (!ec && !comps.empty()) { playing = false; frame = (float)std::clamp(f, 0, maxFrame()); stills[curComp()][(int)frame] = keep; }
                open[kPreview] = true;
                break;
            }
            case K::Error: { ChatItem c; c.kind = ChatItem::Kind::Error; c.text = e.text; chat.push_back(c); break; }
            case K::Done: break;
            }
            scrollChat = true;
        }
    }
    void onAgentWrote(const fs::path& p) {
        std::error_code ec;
        std::string e = lowerExt(p), name = toUtf8(p.filename());
        rescan();
        if (!codePath.empty() && fs::equivalent(p, codePath, ec)) {
            if (codeDirty) { toast("The agent changed " + name, CUI_TONE_WARNING, "You have unsaved edits; Reload takes the agent's version."); return; }
            float keep = frame;
            if (loadInto(code, p, "script")) { clearFrames(); check(); frame = keep; toast("The agent updated " + name); }
            return;
        }
        if (!briefPath.empty() && fs::equivalent(p, briefPath, ec)) {
            if (briefDirty) toast("The agent changed " + name, CUI_TONE_WARNING, "You have unsaved edits.");
            else if (loadInto(brief, p, "brief")) toast("The agent updated " + name);
            return;
        }
        clearFrames();   // it may be a file the open script includes or draws
        if ((e == ".as" || e == ".cpp") && !codeDirty) { openScript(p); toast("Opened " + name); }
        else if (e == ".md" && name.find("-brief") != std::string::npos && !briefDirty) { openBrief(p); toast("Opened " + name); }
    }
};

// ------------------------------------------------------------------ panels
cui_box toolbarRow() {
    cui_box row{}; row.dir = CUI_ROW; row.gap = 8; row.pad_x = 10; row.pad_y = 8; row.align = CUI_ALIGN_CENTER;
    return row;
}
void mutedText(cui_ctx* ui, const std::string& s, bool mono = false) {
    cui_text_opts t{}; t.color = CUI_COLOR_TEXT_MUTED; t.size = 12;
    if (mono) t.font = CUI_FONT_MONO;
    cui_text_ex(ui, s.c_str(), &t);
}

void briefPanel(cui_ctx* ui, App& a) {
    cui_box row = toolbarRow();
    cui_row_begin(ui, &row);
    if (!a.briefs.empty()) {
        auto items = cstrs(a.briefs);
        cui_select_opts so{}; so.width = CUI_FILL; so.searchable = true; so.placeholder = "Open a brief...";
        int idx = a.briefIdx;
        if (cui_select_ex(ui, "briefsel", items.data(), (int)items.size(), &idx, &so) && idx >= 0) {
            if (a.briefDirty) a.saveBrief();
            a.openBrief(a.root / fromUtf8(a.briefs[(size_t)idx]));
        }
    } else cui_spacer(ui);
    cui_button_opts bo{}; bo.icon = CUI_ICON_PLUS; bo.tooltip = "Copy the BRIEF.md template to docs/<name>-brief.md";
    if (cui_button_ex(ui, "New", &bo)) { a.newName[0] = 0; a.newError.clear(); a.showNewBrief = true; }
    if (!a.briefPath.empty()) {
        const char* tabs[2] = { "Read", "Edit" };
        cui_segmented_ex(ui, "brieftab", tabs, 2, &a.briefTab, nullptr);
        cui_button_opts sb{}; sb.variant = CUI_BUTTON_PRIMARY; sb.icon = CUI_ICON_SAVE; sb.disabled = !a.briefDirty; sb.shortcut = "Ctrl S";
        if (cui_button_ex(ui, "Save", &sb)) a.saveBrief();
    }
    cui_box_end(ui);
    cui_separator(ui);

    if (a.briefPath.empty()) {
        cui_box col{}; col.height = CUI_FILL; col.justify = CUI_JUSTIFY_CENTER; col.pad = 24;
        cui_column_begin(ui, &col);
        cui_empty_opts eo{}; eo.icon = CUI_ICON_NOTEBOOK_PEN;
        eo.description = "A brief comes before the composition: the conceit, a proof object for every chapter, the script and the reveal map.";
        eo.action = "New brief";
        if (cui_empty_state_ex(ui, "No brief open", &eo)) { a.newName[0] = 0; a.newError.clear(); a.showNewBrief = true; }
        cui_box_end(ui);
        return;
    }
    if (a.briefTab == 0) {
        cui_scroll_opts sc{}; sc.id = "briefscroll"; sc.pad = 16; sc.height = CUI_FILL;
        cui_scroll_begin(ui, &sc);
        cui_markdown_result r = cui_markdown_ex(ui, "briefmd", a.brief.data(), nullptr);
        if (r.task_offset >= 0) { cui_markdown_apply_task(a.brief.data(), &r); a.briefDirty = true; }
        if (r.link) cui_app_open_url(r.link);
        cui_scroll_end(ui);
    } else {
        cui_code_editor_opts eo{}; eo.lang = "text"; eo.height = -1; eo.size = 13;
        if (cui_code_editor_ex(ui, "briefedit", a.brief.data(), a.brief.size(), &eo).changed) a.briefDirty = true;
    }
}

// The Code panel before a script is open, and behind its Examples button.
void galleryView(cui_ctx* ui, App& a) {
    a.pumpThumbs();
    cui_box row = toolbarRow();
    cui_row_begin(ui, &row);
    if (!a.codePath.empty()) {
        cui_button_opts back{}; back.icon = CUI_ICON_ARROW_LEFT; back.tooltip = "Back to the open script";
        if (cui_button_ex(ui, toUtf8(a.codePath.filename()).c_str(), &back)) a.showGallery = false;
    }
    cui_input_opts io{}; io.placeholder = "Search examples"; io.width = CUI_FILL; io.clearable = true;
    cui_search_ex(ui, "exfilter", a.exampleFilter, sizeof a.exampleFilter, &io);
    if (a.categories.size() > 1) {
        auto cats = cstrs(a.categories);
        cui_select_opts so{}; so.width = CUI_PX(190);
        cui_select_ex(ui, "excat", cats.data(), (int)cats.size(), &a.categoryIdx, &so);
    }
    cui_button_opts nb{}; nb.icon = CUI_ICON_PLUS; nb.variant = CUI_BUTTON_PRIMARY; nb.tooltip = "A blank composition in studio-projects/"; nb.shortcut = "Ctrl N";
    if (cui_button_ex(ui, "New script", &nb)) a.askNewScript(false);
    cui_box_end(ui);
    cui_separator(ui);

    cui_scroll_opts sc{}; sc.id = "galleryscroll"; sc.pad = 16; sc.gap = 10; sc.height = CUI_FILL;
    cui_scroll_begin(ui, &sc);
    cui_box wrap{}; wrap.id = "gallerybox"; wrap.gap = 10;
    cui_id galleryId = cui_column_begin(ui, &wrap);
    if (a.examples.empty()) {
        cui_empty_opts eo{}; eo.icon = CUI_ICON_FOLDER_OPEN; eo.action = "Open workspace...";
        eo.description = "This folder has no examples/scripts. Open a raymotion checkout, or start a blank script.";
        if (cui_empty_state_ex(ui, "No examples here", &eo)) a.showSettings = true;
    }
    const float gap = 12, minCard = 200;
    int cols = std::max(1, (int)((a.galleryW + gap) / (minCard + gap)));
    float cardW = (a.galleryW - gap * (float)(cols - 1)) / (float)cols;
    float imgW = std::max(60.f, cardW - 2), imgH = imgW * 9.f / 16.f;
    std::string f = lower(a.exampleFilter);
    const std::string& cat = a.categories[(size_t)std::clamp(a.categoryIdx, 0, (int)a.categories.size() - 1)];
    const Example* clicked = nullptr;
    int shown = 0;
    for (size_t ci = 1; ci < a.categories.size(); ++ci) {
        if (a.categoryIdx > 0 && a.categories[ci] != cat) continue;
        std::vector<Example*> in;
        for (auto& e : a.examples)
            if (e.category == a.categories[ci] && (f.empty() || lower(e.title + " " + e.desc + " " + e.stem).find(f) != std::string::npos)) in.push_back(&e);
        if (in.empty()) continue;
        shown += (int)in.size();
        cui_gap(ui, ci > 1 ? 8.f : 0.f);
        cui_heading(ui, a.categories[ci].c_str(), 4);
        cui_box grid{}; grid.dir = CUI_GRID; grid.columns = cols; grid.gap = gap;
        cui_box_begin(ui, &grid);
        for (Example* e : in) {
            cui_push_id(ui, e->rel.c_str());
            cui_card_opts co{}; co.hoverable = true; co.pad = 1; co.gap = 1;
            co.id = "card";                  // a stable id: the default one moves when the layout above changes
            co.selected = !a.codePath.empty() && relTo(a.root, a.codePath) == e->rel;
            if (cui_card_begin(ui, &co)) clicked = e;
            if (e->thumb == Example::Thumb::Ready && !e->tex) e->tex = cui_image_load(ui, toUtf8(e->thumbPath).c_str(), &e->tw, &e->th);
            if (e->tex) {
                cui_image_opts im{}; im.width = CUI_PX(imgW); im.height = CUI_PX(imgH); im.radius = 7;
                cui_image_ex(ui, e->tex, &im);
            } else if (e->thumb == Example::Thumb::Failed) {
                cui_box ph{}; ph.width = CUI_PX(imgW); ph.height = CUI_PX(imgH); ph.align = CUI_ALIGN_CENTER; ph.justify = CUI_JUSTIFY_CENTER;
                ph.bg = CUI_COLOR_SURFACE_MUTED; ph.radius = 7;
                cui_column_begin(ui, &ph);
                mutedText(ui, "No picture: open it to see why");
                cui_box_end(ui);
            } else cui_skeleton(ui, CUI_PX(imgW), imgH);
            cui_box body{}; body.pad_x = 11; body.pad_top = 9; body.pad_bottom = 11; body.gap = 3;
            cui_column_begin(ui, &body);
            cui_label(ui, e->title.c_str());
            cui_text_opts d{}; d.color = CUI_COLOR_TEXT_SECONDARY; d.size = 12.5f; d.line_height = 1.35f;
            cui_text_ex(ui, e->desc.c_str(), &d);
            cui_box_end(ui);
            cui_card_end(ui);
            cui_pop_id(ui);
        }
        cui_box_end(ui);
    }
    if (!a.examples.empty() && shown == 0) {
        cui_empty_opts eo{}; eo.icon = CUI_ICON_SEARCH; eo.description = "Try a shorter word, or another category.";
        cui_empty_state_ex(ui, "Nothing matches", &eo);
    }
    cui_box_end(ui);
    float w = cui_get_rect(ui, galleryId).w;     // last frame's width decides the columns
    if (w > 40) a.galleryW = w;
    cui_scroll_end(ui);
    if (clicked) a.openExample(*clicked);
    if (a.thumbsRunning > 0) cui_request_wakeup(ui, 0.2f);
}

void codePanel(cui_ctx* ui, App& a) {
    if (a.showGallery || a.codePath.empty()) { galleryView(ui, a); return; }

    cui_box row = toolbarRow();
    cui_row_begin(ui, &row);
    cui_button_opts gb{}; gb.icon = CUI_ICON_GRID_3X3; gb.tooltip = "Browse the examples"; gb.shortcut = "Ctrl E";
    if (cui_icon_button_ex(ui, CUI_ICON_GRID_3X3, &gb)) a.showGallery = true;
    {
        auto items = cstrs(a.scripts);
        cui_select_opts so{}; so.width = CUI_FILL; so.searchable = true; so.placeholder = "Open a script...";
        int idx = a.scriptIdx;
        if (cui_select_ex(ui, "scriptsel", items.data(), (int)items.size(), &idx, &so) && idx >= 0 && idx != a.scriptIdx) {
            fs::path p = a.root / fromUtf8(a.scripts[(size_t)idx]);
            a.guardUnsaved([&a, p] { a.openScript(p); });
        }
    }
    cui_button_opts b{}; b.disabled = !a.codeDirty; b.loading = a.checking;
    b.icon = CUI_ICON_SAVE; b.tooltip = "Write the file, check it and refresh the preview"; b.shortcut = "Ctrl S"; b.variant = CUI_BUTTON_PRIMARY;
    if (cui_button_ex(ui, "Save", &b)) a.saveAll();
    cui_button_opts rb{}; rb.disabled = !a.codeDirty; rb.tooltip = "Discard the edits and read the file again";
    if (cui_icon_button_ex(ui, CUI_ICON_ROTATE_CCW, &rb)) { fs::path p = a.codePath; float f = a.frame; a.codeDirty = false; a.openScript(p); a.frame = f; }
    cui_box_end(ui);

    if (a.isShippedExample(a.codePath) && a.exampleNote) {
        cui_banner_opts bn{}; bn.tone = CUI_TONE_INFO; bn.icon = CUI_ICON_INFO; bn.action = "Save a copy"; bn.dismissible = true;
        if (cui_banner_ex(ui, "This is a shipped example. Saving changes the repository's copy.", &a.exampleNote, &bn)) a.askNewScript(true);
    }

    cui_split_opts sp{}; sp.vertical = true; sp.min_first = 120; sp.min_second = 72; sp.height = CUI_FILL;
    cui_split_begin(ui, "codesplit", &a.codeSplit, &sp);
    cui_code_editor_opts eo{}; eo.lang = "cpp"; eo.height = -1;
    eo.marks = a.marks.empty() ? nullptr : a.marks.data(); eo.mark_count = (int)a.marks.size();
    cui_code_editor_result er = cui_code_editor_ex(ui, "codeedit", a.code.data(), a.code.size(), &eo);
    if (er.changed) { a.codeDirty = true; a.marks.clear(); }
    if (er.refused) a.toast("The script is too large for the editor", CUI_TONE_DANGER);
    a.caretLine = er.line; a.caretCol = er.column;
    cui_split_next(ui);

    int errors = 0;
    for (const auto& d : a.diags) errors += d.severity == CUI_SEVERITY_ERROR;
    std::string problems = errors ? "Problems (" + std::to_string(errors) + ")" : "Problems";
    cui_box lrow{}; lrow.dir = CUI_ROW; lrow.pad_x = 10; lrow.pad_top = 4; lrow.align = CUI_ALIGN_CENTER;
    cui_row_begin(ui, &lrow);
    const char* tabs[2] = { problems.c_str(), "Output" };
    cui_tabs_ex(ui, "lowertab", tabs, 2, &a.lowerTab, nullptr);
    cui_box_end(ui);
    if (a.lowerTab == 0) {
        if (a.diags.empty()) {
            cui_box col{}; col.pad = 12;
            cui_column_begin(ui, &col);
            mutedText(ui, a.checking ? "Checking..." : !a.checkedOnce ? "Not checked yet." : a.codeDirty ? "No problems in the saved file. Save to check your edits." : "No problems.");
            cui_box_end(ui);
        } else {
            std::vector<cui_diagnostic> ds;
            for (const auto& d : a.diags) ds.push_back({ d.severity, d.message.c_str(), d.file.c_str(), d.line, d.column, nullptr, nullptr });
            cui_problems_opts po{}; po.no_toolbar = true;
            cui_box col{}; col.pad_x = 6; col.height = CUI_FILL; col.scroll_y = true; col.id = "problemsbox";
            cui_column_begin(ui, &col);
            cui_problems_result pr = cui_problems_ex(ui, "problems", ds.data(), (int)ds.size(), &po);
            cui_box_end(ui);
            int go = pr.activated >= 0 ? pr.activated : -1;
            if (go >= 0 && go < (int)a.diags.size() && a.diags[(size_t)go].line > 0) {
                const Diag& d = a.diags[(size_t)go];
                if (d.file == relTo(a.root, a.codePath)) cui_code_editor_goto(ui, "codeedit", d.line, std::max(1, d.column));
                else a.toast("That line is in " + d.file);
            }
        }
    } else {
        cui_scroll_opts sc{}; sc.id = "outputscroll"; sc.pad = 10; sc.height = CUI_FILL;
        cui_scroll_begin(ui, &sc);
        cui_text_opts to{}; to.font = CUI_FONT_MONO; to.size = 12;
        cui_text_ex(ui, a.output.c_str(), &to);
        cui_scroll_end(ui);
    }
    cui_split_end(ui);
}

void assetsPanel(cui_ctx* ui, App& a) {
    cui_box row = toolbarRow();
    cui_row_begin(ui, &row);
    cui_input_opts io{}; io.placeholder = "Filter assets"; io.clearable = true; io.width = CUI_FILL;
    if (cui_search_ex(ui, "assetfilter", a.assetFilter, sizeof a.assetFilter, &io) & CUI_CHANGED) a.applyAssetFilter();
    cui_button_opts rb{}; rb.tooltip = "Look for new files";
    if (cui_icon_button_ex(ui, CUI_ICON_REFRESH_CW, &rb)) a.rescan();
    cui_box_end(ui);
    cui_separator(ui);

    cui_split_opts sp{}; sp.vertical = true; sp.min_first = 80; sp.min_second = 80; sp.height = CUI_FILL;
    cui_split_begin(ui, "assetsplit", &a.assetSplit, &sp);
    cui_scroll_opts sc{}; sc.id = "assetlist"; sc.pad = 6; sc.height = CUI_FILL;
    cui_scroll_begin(ui, &sc);
    int clicked = -1;
    const int cap = 600;
    for (int i = 0; i < (int)a.assetShown.size() && i < cap; ++i) {
        cui_push_id_int(ui, i);
        cui_nav_opts no{}; no.selected = i == a.assetSel;
        std::string e = lowerExt(fromUtf8(a.assetShown[(size_t)i]));
        no.icon = e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".svg" ? CUI_ICON_IMAGE
                : e == ".wav" || e == ".mp3" || e == ".ogg" ? CUI_ICON_MUSIC
                : e == ".ttf" || e == ".otf" ? CUI_ICON_TYPE : CUI_ICON_FILE;
        if (cui_nav_item_ex(ui, a.assetShown[(size_t)i].c_str() + (a.assetShown[(size_t)i].rfind("assets/", 0) == 0 ? 7 : 0), &no)) clicked = i;
        cui_pop_id(ui);
    }
    if ((int)a.assetShown.size() > cap) mutedText(ui, "  " + std::to_string(a.assetShown.size() - cap) + " more: type in the filter to narrow the list.");
    if (a.assetShown.empty()) mutedText(ui, a.assets.empty() ? "  No files under assets/." : "  Nothing matches the filter.");
    cui_scroll_end(ui);
    if (clicked >= 0) a.selectAsset(clicked);
    cui_split_next(ui);

    cui_box col{}; col.pad = 12; col.gap = 8; col.height = CUI_FILL; col.id = "assetinfo"; col.scroll_y = true;
    cui_id infoId = cui_column_begin(ui, &col);
    if (a.assetSel >= 0 && a.assetSel < (int)a.assetShown.size()) {
        const std::string& rel = a.assetShown[(size_t)a.assetSel];
        std::error_code ec;
        auto sz = fs::file_size(a.root / fromUtf8(rel), ec);
        cui_text_opts mono{}; mono.font = CUI_FONT_MONO; mono.size = 12;
        cui_text_ex(ui, rel.c_str(), &mono);
        char info[96];
        if (a.assetTex && a.assetW > 0) snprintf(info, sizeof info, "%d x %d px   %.1f KB", a.assetW, a.assetH, ec ? 0.0 : (double)sz / 1024.0);
        else snprintf(info, sizeof info, "%.1f KB", ec ? 0.0 : (double)sz / 1024.0);
        mutedText(ui, info);
        if (a.assetTex && a.assetW > 0) {
            float w = std::min(a.assetBoxW - 24, (float)a.assetW), h = w * (float)a.assetH / (float)a.assetW;
            cui_image_opts im{}; im.width = CUI_PX(w); im.height = CUI_PX(h); im.radius = 6;
            cui_image_ex(ui, a.assetTex, &im);
        }
        cui_box brow{}; brow.dir = CUI_ROW; brow.gap = 8;
        cui_row_begin(ui, &brow);
        cui_button_opts cb{}; cb.icon = CUI_ICON_AT_SIGN; cb.tooltip = "Put this path in the message to the agent";
        if (cui_button_ex(ui, "Mention in chat", &cb)) {
            size_t n = strlen(a.input.data());
            std::string add = (n ? " " : "") + rel + " ";
            if (n + add.size() + 1 < a.input.size()) strcat(a.input.data(), add.c_str());
            a.open[kChat] = true;
        }
        // Scripts name assets relative to the assets folder.
        cui_copy_button(ui, "copyasset", rel.rfind("assets/", 0) == 0 ? rel.c_str() + 7 : rel.c_str());
        cui_box_end(ui);
    } else mutedText(ui, "Select a file to see it.");
    cui_box_end(ui);
    a.assetBoxW = std::max(120.f, cui_get_rect(ui, infoId).w);
    cui_split_end(ui);
}

void previewPanel(cui_ctx* ui, App& a) {
    CompInfo* c = a.comp();
    bool has = !a.codePath.empty() && a.checkOk && c;

    cui_box row = toolbarRow();
    cui_row_begin(ui, &row);
    if (a.compNames.size() > 1) {
        auto items = cstrs(a.compNames);
        cui_select_opts so{}; so.width = CUI_PX(200);
        if (cui_select_ex(ui, "compsel", items.data(), (int)items.size(), &a.compIdx, &so)) { a.frame = 0; a.playing = false; }
    } else if (c) cui_label(ui, c->name.c_str());
    if (c) {
        char info[96]; snprintf(info, sizeof info, "%dx%d  %d fps  %s", c->w, c->h, c->fps, seconds(c->frames, c->fps).c_str());
        cui_text_opts it{}; it.color = CUI_COLOR_TEXT_MUTED; it.size = 12; it.nowrap = true;
        cui_text_ex(ui, info, &it);
    }
    cui_spacer(ui);
    cui_check_opts sw{}; sw.disabled = a.codePath.empty();
    cui_switch_ex(ui, "Auto", &a.autoPreview, &sw);
    cui_tooltip(ui, "Render the frame under the scrubber whenever the script is saved or the frame changes");
    cui_button_opts rb{}; rb.loading = a.rendering; rb.disabled = a.codePath.empty();
    rb.tooltip = "Save and render this frame now"; rb.shortcut = "Ctrl R";
    if (cui_icon_button_ex(ui, CUI_ICON_REFRESH_CW, &rb)) { a.failedKey.clear(); a.renderStill(true); }
    cui_button_opts lb{}; lb.disabled = !has; lb.tooltip = "Play in the raymotion window, with sound (Esc closes it)"; lb.shortcut = "F5";
    if (cui_icon_button_ex(ui, CUI_ICON_EXTERNAL_LINK, &lb)) a.liveWindow();
    cui_box_end(ui);
    cui_separator(ui);

    // the picture
    cui_box stage{}; stage.height = CUI_FILL; stage.id = "stage"; stage.pad = 8; stage.gap = 8;
    cui_column_begin(ui, &stage);
    if (!a.previewError.empty()) {
        cui_alert_opts al{}; al.tone = CUI_TONE_DANGER; al.description = a.previewError.c_str();
        cui_alert_ex(ui, "This frame did not render", &al);
    }
    if (a.still && a.stillW > 0) {
        cui_image_viewer_opts vo{}; vo.height = CUI_FILL;
        cui_image_viewer_ex(ui, "viewer", a.still, a.stillW, a.stillH, &a.view, &vo);
    } else {
        cui_box mid{}; mid.height = CUI_FILL; mid.justify = CUI_JUSTIFY_CENTER;
        cui_column_begin(ui, &mid);
        cui_empty_opts eo{};
        if (a.codePath.empty()) {
            eo.icon = CUI_ICON_IMAGE; eo.description = "Pick an example in the Code panel, or start a new script.";
            cui_empty_state_ex(ui, "Nothing to preview yet", &eo);
        } else if (a.checking || a.rendering || (a.playing && a.drafting)) {
            cui_box sp{}; sp.align = CUI_ALIGN_CENTER; sp.gap = 10;
            cui_column_begin(ui, &sp);
            cui_spinner(ui, 22);
            mutedText(ui, a.checking ? "Checking the script..." : "Rendering...");
            cui_box_end(ui);
        } else if (!a.checkOk && a.checkedOnce) {
            eo.icon = CUI_ICON_TRIANGLE_ALERT; eo.description = "Fix the problems listed under the editor, then save.";
            cui_empty_state_ex(ui, "The script does not compile", &eo);
        } else if (a.previewError.empty()) {
            eo.icon = CUI_ICON_CAMERA; eo.action = "Render this frame"; eo.description = a.autoPreview ? "" : "Auto is off.";
            if (cui_empty_state_ex(ui, "No picture for this frame yet", &eo)) { a.failedKey.clear(); a.renderStill(true); }
        }
        cui_box_end(ui);
    }
    cui_box_end(ui);

    // transport
    cui_separator(ui);
    cui_box tr{}; tr.dir = CUI_ROW; tr.gap = 4; tr.pad_x = 10; tr.pad_top = 8; tr.align = CUI_ALIGN_CENTER;
    cui_row_begin(ui, &tr);
    cui_button_opts tb{}; tb.disabled = !has;
    tb.tooltip = "First frame"; tb.shortcut = "Home";
    if (cui_icon_button_ex(ui, CUI_ICON_SKIP_BACK, &tb)) { a.playing = false; a.frame = 0; }
    tb.tooltip = "Back one frame"; tb.shortcut = "Left";
    if (cui_icon_button_ex(ui, CUI_ICON_CHEVRON_LEFT, &tb)) a.step(-1);
    cui_button_opts pb{}; pb.disabled = !has; pb.variant = CUI_BUTTON_PRIMARY;
    pb.tooltip = a.playing ? "Pause" : "Play here: a silent half-size draft, rendered once per save"; pb.shortcut = "Space";
    if (cui_icon_button_ex(ui, a.playing ? CUI_ICON_PAUSE : CUI_ICON_PLAY, &pb)) a.togglePlay();
    tb.tooltip = "Forward one frame"; tb.shortcut = "Right";
    if (cui_icon_button_ex(ui, CUI_ICON_CHEVRON_RIGHT, &tb)) a.step(1);
    tb.tooltip = "Last frame"; tb.shortcut = "End";
    if (cui_icon_button_ex(ui, CUI_ICON_SKIP_FORWARD, &tb)) { a.playing = false; a.frame = (float)a.maxFrame(); }
    cui_gap(ui, 6);
    {
        cui_box sbox{}; sbox.width = CUI_FILL;
        cui_column_begin(ui, &sbox);
        cui_slider_opts sl{}; sl.step = 1; sl.disabled = !has; sl.width = CUI_FILL;
        float before = a.frame;
        if (cui_slider_ex(ui, "##frame", &a.frame, 0, (float)std::max(1, a.maxFrame()), &sl) && a.frame != before) a.playing = false;
        cui_box_end(ui);
    }
    cui_gap(ui, 6);
    if (c) {
        char t[64]; snprintf(t, sizeof t, "%d / %d   %s", (int)a.frame, a.maxFrame(), seconds((int)a.frame, c->fps).c_str());
        cui_text_opts to{}; to.font = CUI_FONT_MONO; to.size = 12; to.tabular = true; to.nowrap = true;
        cui_text_ex(ui, t, &to);
    }
    cui_box_end(ui);
    cui_box nrow{}; nrow.dir = CUI_ROW; nrow.pad_x = 12; nrow.pad_bottom = 8; nrow.pad_top = 2; nrow.gap = 10; nrow.align = CUI_ALIGN_CENTER;
    cui_row_begin(ui, &nrow);
    std::string note = a.previewNote;
    if (a.codeDirty && !note.empty()) note += "   unsaved edits are not in this picture";
    if (a.drafting && c) note += (note.empty() ? "" : "   ") + std::string("rendering draft ") + std::to_string(a.draftReady) + " / " + std::to_string(c->frames);
    mutedText(ui, note.empty() ? " " : note, true);
    cui_box_end(ui);

    // keys, when no text field has the keyboard
    if (has && !cui_wants_keyboard(ui)) {
        if (cui_key_pressed(ui, CUI_KEY_SPACE)) a.togglePlay();
        if (cui_key_pressed(ui, CUI_KEY_LEFT)) a.step(cui_key_mods(ui) & CUI_MOD_SHIFT ? -c->fps : -1);
        if (cui_key_pressed(ui, CUI_KEY_RIGHT)) a.step(cui_key_mods(ui) & CUI_MOD_SHIFT ? c->fps : 1);
        if (cui_key_pressed(ui, CUI_KEY_HOME)) { a.playing = false; a.frame = 0; }
        if (cui_key_pressed(ui, CUI_KEY_END)) { a.playing = false; a.frame = (float)a.maxFrame(); }
    }
}

void chatItemView(cui_ctx* ui, const ChatItem& c, int i) {
    cui_push_id_int(ui, i);
    switch (c.kind) {
    case ChatItem::Kind::User: {
        cui_chat_opts o{}; o.mine = true; o.markdown = false;
        cui_chat_message_ex(ui, c.text.c_str(), &o);
        break;
    }
    case ChatItem::Kind::Assistant: {
        cui_chat_opts o{}; o.author = "Agent"; o.markdown = true;
        cui_chat_message_ex(ui, c.text.c_str(), &o);
        break;
    }
    case ChatItem::Kind::Tool: {
        cui_tool_call_opts o{}; o.status = c.status; o.detail = c.text.c_str(); o.input = c.input.c_str();
        o.output = c.output.empty() ? nullptr : c.output.c_str(); o.seconds = c.seconds;
        cui_tool_call_ex(ui, "tool", c.name.c_str(), &o);
        break;
    }
    case ChatItem::Kind::Error: {
        cui_alert_opts al{}; al.tone = CUI_TONE_DANGER; al.description = c.text.c_str();
        cui_alert_ex(ui, "The agent stopped", &al);
        break;
    }
    }
    cui_pop_id(ui);
}

void chatPanel(cui_ctx* ui, App& a) {
    bool busy = a.agent.busy();
    cui_box col{}; col.height = CUI_FILL; col.id = "chatcol";
    cui_column_begin(ui, &col);

    cui_box row = toolbarRow();
    cui_row_begin(ui, &row);
    cui_button_opts mb{}; mb.variant = CUI_BUTTON_GHOST; mb.size = CUI_SIZE_SM; mb.icon = CUI_ICON_SPARKLES; mb.tooltip = "The model the agent uses. Click to change it.";
    if (cui_button_ex(ui, a.modelBuf[0] ? a.modelBuf : "No model", &mb)) a.showSettings = true;
    cui_spacer(ui);
    cui_button_opts nb{}; nb.disabled = busy || a.chat.empty(); nb.tooltip = "Start a new conversation";
    if (cui_icon_button_ex(ui, CUI_ICON_SQUARE_PEN, &nb)) { a.agent.reset(); a.chat.clear(); }
    cui_box_end(ui);
    cui_separator(ui);

    cui_scroll_opts sc{}; sc.id = "chatscroll"; sc.pad = 12; sc.gap = 10; sc.height = CUI_FILL;
    cui_scroll_begin(ui, &sc);
    if (!a.agentReady()) {
        cui_alert_opts al{}; al.tone = CUI_TONE_WARNING; al.icon = CUI_ICON_KEY_ROUND;
        al.description = "Add an API key, or point the base URL at a local server such as Ollama. Everything else in the Studio works without it.";
        cui_alert_ex(ui, "The agent is not set up", &al);
        cui_button_opts sb{}; sb.icon = CUI_ICON_SETTINGS;
        if (cui_button_ex(ui, "Open Settings", &sb)) a.showSettings = true;
    }
    if (a.chat.empty()) {
        cui_text_opts intro{}; intro.color = CUI_COLOR_TEXT_SECONDARY;
        cui_text_ex(ui, "Describe a film, or ask for a change to the open script. The agent reads AGENTS.md, writes the brief first, then the composition, checks it and renders stills you see in the Preview.", &intro);
        // title, detail, and what is actually sent
        struct S { const char* title; const char* detail; std::string prompt; };
        std::vector<S> s;
        std::string name = a.codePath.empty() ? "" : toUtf8(a.codePath.filename());
        bool errors = !a.diags.empty() && !a.checkOk;
        if (errors) s.push_back({ "Fix the problems", "Reads the errors and edits the open script", "Run check_script on the open script, fix every error it reports, and check again until it compiles." });
        if (!name.empty()) {
            s.push_back({ "Explain this script", "What each scene draws, and when", "Read the open script and explain, scene by scene, what it draws and when. Be brief." });
            s.push_back({ "Make a variation", "A copy in studio-projects/ with a new look", "Copy the open script to studio-projects/ under a new name, give it a different palette and timing, check it and render a still from the middle." });
        }
        s.push_back({ "Brief a 30 s explainer", "Fills docs/<name>-brief.md from the template", "Ask me for the topic in one question, then write a filled brief for a 30 second explainer to docs/<slug>-brief.md, following AGENTS.md." });
        std::vector<cui_prompt_suggestion> cards;
        for (const auto& x : s) cards.push_back({ nullptr, x.title, x.detail });
        cui_prompt_suggestions_opts po{}; po.columns = 1;
        int pick = cui_prompt_suggestions_ex(ui, "sugg", cards.data(), (int)cards.size(), &po);
        if (pick >= 0 && !busy) a.send(s[(size_t)pick].prompt);
    }
    for (int i = 0; i < (int)a.chat.size(); ++i) chatItemView(ui, a.chat[(size_t)i], i);
    if (busy) cui_typing_indicator(ui, "Agent");
    cui_scroll_end(ui);
    if (a.scrollChat) { cui_scroll_to(ui, "chatscroll", 1e7f); a.scrollChat = false; }

    cui_composer_opts co{}; co.placeholder = "Ask the agent...  (Enter sends, Shift+Enter adds a line)"; co.busy = busy;
    int fl = cui_chat_composer_ex(ui, "composer", a.input.data(), a.input.size(), &co);
    if ((fl & CUI_COMPOSER_SEND) && a.input[0]) { a.send(a.input.data()); a.input[0] = 0; }
    if (fl & CUI_COMPOSER_STOP) a.agent.cancel();
    cui_box_end(ui);
}

// ------------------------------------------------------------------ dialogs
void settingsForm(cui_ctx* ui, App& a) {
    cui_box col{}; col.pad = 4; col.gap = 10; col.max_height = 540; col.scroll_y = true; col.id = "settingscol";
    cui_column_begin(ui, &col);
    cui_heading(ui, "Agent (OpenAI-compatible)", 4);
    cui_input_opts io{};
    io.label = "Base URL"; io.hint = "Anything that serves /chat/completions: OpenAI, Ollama (http://localhost:11434/v1), llama.cpp, LM Studio";
    cui_input_ex(ui, "url", a.urlBuf, sizeof a.urlBuf, &io);
    io = {}; io.label = "API key"; io.password = true; io.hint = "Stored as plain text in the settings file. Empty uses OPENAI_API_KEY.";
    cui_input_ex(ui, "key", a.keyBuf, sizeof a.keyBuf, &io);
    io = {}; io.label = "Model"; io.hint = "Type a name, or load the server's list and pick one.";
    if (cui_input_ex(ui, "model", a.modelBuf, sizeof a.modelBuf, &io) & CUI_CHANGED) a.modelIdx = -1;
    {
        cui_box mrow{}; mrow.dir = CUI_ROW; mrow.gap = 8; mrow.align = CUI_ALIGN_CENTER;
        cui_row_begin(ui, &mrow);
        cui_button_opts lb{}; lb.icon = CUI_ICON_DOWNLOAD; lb.loading = a.loadingModels; lb.tooltip = "GET {base URL}/models with the key above";
        if (cui_button_ex(ui, "Load models", &lb)) a.loadModels();
        if (!a.models.empty()) {
            auto items = cstrs(a.models);
            cui_select_opts so{}; so.width = CUI_FILL; so.searchable = true; so.placeholder = "Pick a model...";
            if (cui_select_ex(ui, "modelsel", items.data(), (int)items.size(), &a.modelIdx, &so) && a.modelIdx >= 0)
                copyTo(a.modelBuf, sizeof a.modelBuf, a.models[(size_t)a.modelIdx]);
        }
        cui_box_end(ui);
        if (!a.modelNote.empty()) mutedText(ui, a.modelNote);
    }
    cui_slider_opts so{}; so.step = 1; so.show_value = true; so.format = "%.0f";
    cui_slider_ex(ui, "Max model round-trips per message", &a.stepsF, 2, 40, &so);

    cui_separator(ui);
    cui_heading(ui, "Workspace", 4);
    {
        cui_box wrow{}; wrow.dir = CUI_ROW; wrow.gap = 8; wrow.align = CUI_ALIGN_END;
        cui_row_begin(ui, &wrow);
        io = {}; io.label = "Workspace folder"; io.width = CUI_FILL;
        if (cui_input_ex(ui, "root", a.rootBuf, sizeof a.rootBuf, &io) & CUI_SUBMITTED) a.setWorkspace(a.rootBuf);
        cui_button_opts bo{}; bo.icon = CUI_ICON_FOLDER_OPEN;
        if (cui_button_ex(ui, "Browse...", &bo)) {
            // The native folder dialog blocks, so ask only on click.
            char out[512] = "";
            cui_file_dialog_opts fo{}; fo.kind = CUI_FILE_FOLDER; fo.title = "Workspace folder";
            if (gApp && cui_app_file_dialog(gApp, &fo, out, sizeof out)) a.setWorkspace(out);
        }
        cui_box_end(ui);
        mutedText(ui, "The raymotion checkout. The agent can read and write only inside this folder.");
    }
    io = {}; io.label = "raymotion_script executable"; io.hint = "Empty: next to this program, then build/.";
    cui_input_ex(ui, "host", a.hostBuf, sizeof a.hostBuf, &io);
    std::string host = findHost(a.hostSettings(), a.root);
    bool found = fs::exists(fromUtf8(host));
    cui_alert_opts al{}; al.tone = found ? CUI_TONE_SUCCESS : CUI_TONE_DANGER; al.description = host.c_str();
    cui_alert_ex(ui, found ? "Renderer found" : "Renderer not found: build the raymotion_script target", &al);

    cui_separator(ui);
    cui_heading(ui, "Appearance", 4);
    const char* themes[2] = { "Light", "Dark" };
    if (cui_segmented_ex(ui, "theme", themes, 2, &a.themeIdx, nullptr)) cui_set_theme(ui, a.themeIdx ? CUI_THEME_DARK : CUI_THEME_LIGHT);

    cui_box_end(ui);
}

const char* kDefaultLayout = "dock1 h0.46([*Code,Brief,Assets],h0.62([*Preview],[*Chat]))";

void dialogs(cui_ctx* ui, App& a) {
    if (a.showSettings) {
        cui_dialog_opts dopt{}; dopt.width = 580; dopt.description = "Agent endpoint, workspace and appearance.";
        if (cui_dialog_begin(ui, "Settings", &a.showSettings, &dopt)) {
            settingsForm(ui, a);
            cui_dialog_footer_begin(ui);
            if (!a.settingsNote.empty()) {
                cui_text_opts nt{}; nt.color = a.settingsNoteError ? CUI_COLOR_DANGER : CUI_COLOR_TEXT_MUTED;
                cui_text_ex(ui, a.settingsNote.c_str(), &nt);
            }
            cui_spacer(ui);
            cui_button_opts cl{};
            if (cui_button_ex(ui, "Close", &cl)) a.showSettings = false;
            cui_button_opts sv{}; sv.variant = CUI_BUTTON_PRIMARY; sv.icon = CUI_ICON_SAVE;
            if (cui_button_ex(ui, "Save", &sv)) a.saveSettings();
            cui_dialog_footer_end(ui);
            cui_dialog_end(ui);
        }
    }
    if (a.showNewScript) {
        cui_dialog_opts d{}; d.width = 460; d.description = "A composition in AngelScript, saved under studio-projects/.";
        if (cui_dialog_begin(ui, "New script", &a.showNewScript, &d)) {
            bool go = false;
            cui_input_opts io{}; io.label = "Name"; io.placeholder = "my-film"; io.autofocus = true; io.suffix = ".as";
            io.error = a.newError.empty() ? nullptr : a.newError.c_str();
            if (cui_input_ex(ui, "newscriptname", a.newName, sizeof a.newName, &io) & CUI_SUBMITTED) go = true;
            if (!a.codePath.empty()) {
                std::string copy = "A copy of " + toUtf8(a.codePath.filename());
                const char* from[2] = { "A blank composition", copy.c_str() };
                cui_radio_group(ui, "newfrom", from, 2, &a.newFrom);
            }
            cui_dialog_footer_begin(ui);
            cui_button_opts cl{};
            if (cui_button_ex(ui, "Cancel", &cl)) a.showNewScript = false;
            cui_button_opts ok{}; ok.variant = CUI_BUTTON_PRIMARY; ok.disabled = slugify(a.newName).empty();
            if (cui_button_ex(ui, "Create", &ok)) go = true;
            cui_dialog_footer_end(ui);
            cui_dialog_end(ui);
            if (go) a.newScript(a.newName, a.newFrom == 1);
        }
    }
    if (a.showNewBrief) {
        cui_dialog_opts d{}; d.width = 460; d.description = "Copies skills/raymotion/templates/BRIEF.md to docs/<name>-brief.md.";
        if (cui_dialog_begin(ui, "New brief", &a.showNewBrief, &d)) {
            bool go = false;
            cui_input_opts io{}; io.label = "Name"; io.placeholder = "my-film"; io.autofocus = true; io.suffix = "-brief.md";
            io.error = a.newError.empty() ? nullptr : a.newError.c_str();
            if (cui_input_ex(ui, "newbriefname", a.newName, sizeof a.newName, &io) & CUI_SUBMITTED) go = true;
            cui_dialog_footer_begin(ui);
            cui_button_opts cl{};
            if (cui_button_ex(ui, "Cancel", &cl)) a.showNewBrief = false;
            cui_button_opts ok{}; ok.variant = CUI_BUTTON_PRIMARY; ok.disabled = slugify(a.newName).empty();
            if (cui_button_ex(ui, "Create", &ok)) go = true;
            cui_dialog_footer_end(ui);
            cui_dialog_end(ui);
            if (go) a.newBrief(a.newName);
        }
    }
    if (a.showConfirm) {
        std::string what = toUtf8(a.codePath.filename()) + " has unsaved edits.";
        cui_dialog_opts d{}; d.width = 440; d.description = what.c_str();
        if (cui_dialog_begin(ui, "Save your changes?", &a.showConfirm, &d)) {
            int choice = 0;
            cui_dialog_footer_begin(ui);
            cui_button_opts cl{};
            if (cui_button_ex(ui, "Cancel", &cl)) choice = 3;
            cui_button_opts dc{}; dc.variant = CUI_BUTTON_DANGER;
            if (cui_button_ex(ui, "Discard", &dc)) choice = 2;
            cui_button_opts sv{}; sv.variant = CUI_BUTTON_PRIMARY; sv.icon = CUI_ICON_SAVE;
            if (cui_button_ex(ui, "Save", &sv)) choice = 1;
            cui_dialog_footer_end(ui);
            cui_dialog_end(ui);
            if (choice) {
                a.showConfirm = false;
                bool go = choice == 2 || (choice == 1 && a.saveCode());
                if (choice == 2) a.codeDirty = false;
                auto then = std::move(a.afterConfirm);
                a.afterConfirm = nullptr;
                if (go && then) then();
            }
        }
        if (!a.showConfirm) a.afterConfirm = nullptr;    // closed with Esc or the x
    }
}

void quit(App& a) { a.guardUnsaved([] { cui_app_quit(gApp); }); }
void openWorkspace(App& a) {
    a.guardUnsaved([&a] {
        char out[512] = "";
        cui_file_dialog_opts fo{}; fo.kind = CUI_FILE_FOLDER; fo.title = "Workspace folder";
        if (cui_app_file_dialog(gApp, &fo, out, sizeof out)) a.setWorkspace(out);
    });
}
void resetLayout(cui_ctx* ui, App& a) {
    for (bool& b : a.open) b = true;
    cui_dock_load(ui, "main", kDefaultLayout);
}

void menus(cui_ctx* ui, App& a) {
    bool script = !a.codePath.empty();
    cui_menu_bar_begin(ui, "menubar", nullptr);
    if (cui_menu_bar_menu_begin(ui, "File", nullptr)) {
        cui_menu_item_opts o{}; o.shortcut = "Ctrl N"; o.icon = CUI_ICON_PLUS;
        if (cui_menu_item_ex(ui, "New script...", &o)) a.askNewScript(false);
        o = {};
        if (cui_menu_item_ex(ui, "New brief...", &o)) { a.newName[0] = 0; a.newError.clear(); a.showNewBrief = true; }
        o = {}; o.shortcut = "Ctrl E"; o.icon = CUI_ICON_GRID_3X3;
        if (cui_menu_item_ex(ui, "Browse examples", &o)) { a.showGallery = true; a.open[kCode] = true; }
        cui_menu_separator(ui);
        o = {}; o.shortcut = "Ctrl S"; o.icon = CUI_ICON_SAVE; o.disabled = !a.codeDirty && !a.briefDirty;
        if (cui_menu_item_ex(ui, "Save", &o)) a.saveAll();
        o = {}; o.disabled = !script;
        if (cui_menu_item_ex(ui, "Save a copy...", &o)) a.askNewScript(true);
        cui_menu_separator(ui);
        o = {}; o.icon = CUI_ICON_FOLDER_OPEN;
        if (cui_menu_item_ex(ui, "Open workspace...", &o)) openWorkspace(a);
        o = {};
        if (cui_menu_item_ex(ui, "Rescan files", &o)) a.rescan();
        cui_menu_separator(ui);
        o = {}; o.shortcut = "Ctrl ,"; o.icon = CUI_ICON_SETTINGS;
        if (cui_menu_item_ex(ui, "Settings...", &o)) a.showSettings = true;
        cui_menu_separator(ui);
        o = {};
        if (cui_menu_item_ex(ui, "Quit", &o)) quit(a);
        cui_menu_bar_menu_end(ui);
    }
    if (cui_menu_bar_menu_begin(ui, "Run", nullptr)) {
        cui_menu_item_opts o{}; o.disabled = !script || a.checking;
        if (cui_menu_item_ex(ui, "Check script", &o)) a.check();
        o = {}; o.disabled = !script || a.rendering; o.shortcut = "Ctrl R";
        if (cui_menu_item_ex(ui, "Render this frame", &o)) { a.failedKey.clear(); a.renderStill(true); }
        o = {}; o.disabled = !script || !a.checkOk; o.shortcut = "Space";
        if (cui_menu_item_ex(ui, a.playing ? "Pause" : "Play draft in the Preview", &o)) a.togglePlay();
        o = {}; o.disabled = !script; o.shortcut = "F5";
        if (cui_menu_item_ex(ui, "Play in the live window (with sound)", &o)) a.liveWindow();
        cui_menu_separator(ui);
        o = {}; o.disabled = !script || !a.checkOk || a.exporting; o.icon = CUI_ICON_VIDEO;
        if (cui_menu_item_ex(ui, a.exporting ? "Rendering the video..." : "Render video to out/", &o)) a.exportVideo();
        cui_menu_bar_menu_end(ui);
    }
    if (cui_menu_bar_menu_begin(ui, "Agent", nullptr)) {
        cui_menu_item_opts o{}; o.disabled = !a.agent.busy();
        if (cui_menu_item_ex(ui, "Stop", &o)) a.agent.cancel();
        o = {}; o.disabled = a.agent.busy() || a.chat.empty();
        if (cui_menu_item_ex(ui, "New conversation", &o)) { a.agent.reset(); a.chat.clear(); }
        cui_menu_separator(ui);
        o = {};
        if (cui_menu_item_ex(ui, "Load models...", &o)) { a.showSettings = true; a.loadModels(); }
        cui_menu_bar_menu_end(ui);
    }
    if (cui_menu_bar_menu_begin(ui, "View", nullptr)) {
        cui_menu_item_opts o{}; o.shortcut = "Ctrl K"; o.icon = CUI_ICON_COMMAND;
        if (cui_menu_item_ex(ui, "Command palette...", &o)) a.showPalette = true;
        cui_menu_separator(ui);
        for (int i : { kCode, kPreview, kChat, kBrief, kAssets }) {
            o = {}; o.checked = a.open[i];
            if (cui_menu_item_ex(ui, a.panelNames[i], &o)) a.open[i] = !a.open[i];
        }
        cui_menu_separator(ui);
        o = {};
        if (cui_menu_item_ex(ui, "Reset layout", &o)) resetLayout(ui, a);
        cui_menu_separator(ui);
        cui_menu_label(ui, "Theme");
        o = {}; o.checked = a.themeIdx == 0;
        if (cui_menu_item_ex(ui, "Light", &o)) { a.themeIdx = 0; cui_set_theme(ui, CUI_THEME_LIGHT); a.set.darkTheme = false; }
        o = {}; o.checked = a.themeIdx == 1;
        if (cui_menu_item_ex(ui, "Dark", &o)) { a.themeIdx = 1; cui_set_theme(ui, CUI_THEME_DARK); a.set.darkTheme = true; }
        cui_menu_bar_menu_end(ui);
    }
    if (cui_menu_bar_menu_begin(ui, "Help", nullptr)) {
        cui_menu_item_opts o{}; o.icon = CUI_ICON_EXTERNAL_LINK;
        if (cui_menu_item_ex(ui, "Scripting reference", &o) && !cui_app_open_url(toUtf8(a.root / "docs" / "scripting.md").c_str())) a.toast("docs/scripting.md is not in this workspace", CUI_TONE_WARNING);
        if (cui_menu_item_ex(ui, "Studio guide", &o) && !cui_app_open_url(toUtf8(a.root / "docs" / "studio.md").c_str())) a.toast("docs/studio.md is not in this workspace", CUI_TONE_WARNING);
        cui_menu_separator(ui);
        o = {};
        if (cui_menu_item_ex(ui, "About Raymotion Studio", &o)) a.toast("Raymotion Studio", CUI_TONE_INFO, "Renderer: " + findHost(a.hostSettings(), a.root));
        cui_menu_bar_menu_end(ui);
    }
    cui_menu_bar_end(ui);
}

// Ctrl+K: every command, then every script.
void palette(cui_ctx* ui, App& a) {
    if (!a.showPalette) return;
    struct Cmd { std::string title; const char* icon; const char* group; const char* shortcut; std::function<void()> run; };
    std::vector<Cmd> cmds = {
        { "Save", CUI_ICON_SAVE, "Commands", "Ctrl S", [&] { a.saveAll(); } },
        { "New script...", CUI_ICON_PLUS, "Commands", "Ctrl N", [&] { a.askNewScript(false); } },
        { "Save a copy of this script...", CUI_ICON_COPY, "Commands", nullptr, [&] { a.askNewScript(true); } },
        { "New brief...", CUI_ICON_NOTEBOOK_PEN, "Commands", nullptr, [&] { a.newName[0] = 0; a.newError.clear(); a.showNewBrief = true; } },
        { "Browse examples", CUI_ICON_GRID_3X3, "Commands", "Ctrl E", [&] { a.showGallery = true; a.open[kCode] = true; } },
        { "Render this frame", CUI_ICON_CAMERA, "Commands", "Ctrl R", [&] { a.failedKey.clear(); a.renderStill(true); } },
        { "Play or pause the draft", CUI_ICON_PLAY, "Commands", "Space", [&] { a.togglePlay(); } },
        { "Play in the live window (with sound)", CUI_ICON_EXTERNAL_LINK, "Commands", "F5", [&] { a.liveWindow(); } },
        { "Render video to out/", CUI_ICON_VIDEO, "Commands", nullptr, [&] { a.exportVideo(); } },
        { "Check script", CUI_ICON_CIRCLE_CHECK, "Commands", nullptr, [&] { a.check(); } },
        { "Settings...", CUI_ICON_SETTINGS, "Commands", "Ctrl ,", [&] { a.showSettings = true; } },
        { "Reset layout", CUI_ICON_LAYOUT_DASHBOARD, "Commands", nullptr, [&] { resetLayout(ui, a); } },
        { "New conversation with the agent", CUI_ICON_SQUARE_PEN, "Commands", nullptr, [&] { if (!a.agent.busy()) { a.agent.reset(); a.chat.clear(); } } },
    };
    for (const auto& s : a.scripts) {
        fs::path p = a.root / fromUtf8(s);
        cmds.push_back({ s, CUI_ICON_FILE, "Scripts", nullptr, [&a, p] { a.guardUnsaved([&a, p] { a.openScript(p); a.open[kCode] = true; }); } });
    }
    for (const auto& b : a.briefs) {
        fs::path p = a.root / fromUtf8(b);
        cmds.push_back({ b, CUI_ICON_FILE_TEXT, "Briefs", nullptr, [&a, p] { if (a.briefDirty) a.saveBrief(); a.openBrief(p); a.open[kBrief] = true; } });
    }
    std::vector<cui_command> list;
    for (const auto& c : cmds) list.push_back({ c.title.c_str(), c.icon, c.group, c.shortcut });
    int pick = cui_command_palette(ui, &a.showPalette, list.data(), (int)list.size());
    if (pick >= 0) cmds[(size_t)pick].run();
}

void statusBar(cui_ctx* ui, App& a) {
    cui_status_bar_begin(ui, "status", nullptr);
    cui_status_item_opts o{}; o.icon = CUI_ICON_FOLDER; o.tooltip = "The workspace. Click to open another.";
    if (cui_status_bar_item_ex(ui, toUtf8(a.root.filename()).c_str(), &o)) openWorkspace(a);
    if (!a.codePath.empty()) {
        std::string file = relTo(a.root, a.codePath) + (a.codeDirty ? "  (unsaved)" : "");
        o = {}; o.icon = CUI_ICON_FILE; o.tone = a.codeDirty ? CUI_TONE_WARNING : CUI_TONE_NEUTRAL;
        cui_status_bar_text_ex(ui, file.c_str(), &o);
        if (!a.showGallery) {
            char pos[48]; snprintf(pos, sizeof pos, "Ln %d, Col %d", a.caretLine, a.caretCol);
            cui_status_bar_text_ex(ui, pos, nullptr);
        }
    }
    cui_status_bar_spacer(ui);
    CompInfo* c = a.comp();
    if (a.exporting) { cui_status_progress_opts p{}; p.label = "Rendering the video"; p.indeterminate = true; p.width = 90; cui_status_bar_progress_ex(ui, 0, &p); }
    if (a.drafting && c) { cui_status_progress_opts p{}; p.label = "Draft"; p.width = 90; cui_status_bar_progress_ex(ui, (float)a.draftReady / (float)std::max(1, c->frames), &p); }
    if (a.thumbsRunning > 0) {
        int left = 0;
        for (const auto& e : a.examples) left += e.thumb == Example::Thumb::None || e.thumb == Example::Thumb::Rendering;
        std::string t = "Rendering " + std::to_string(left) + " example pictures";
        o = {}; o.icon = CUI_ICON_IMAGE;
        cui_status_bar_text_ex(ui, t.c_str(), &o);
    }
    if (!a.codePath.empty()) {
        int errors = 0;
        for (const auto& d : a.diags) errors += d.severity == CUI_SEVERITY_ERROR;
        o = {};
        std::string t;
        if (a.checking) { t = "Checking"; o.icon = CUI_ICON_LOADER_CIRCLE; }
        else if (errors) { t = std::to_string(errors) + (errors == 1 ? " problem" : " problems"); o.icon = CUI_ICON_CIRCLE_X; o.tone = CUI_TONE_DANGER; o.tooltip = "Show the problems"; }
        else if (a.checkOk) { t = a.rendering ? "Rendering" : "Ready"; o.icon = a.rendering ? CUI_ICON_LOADER_CIRCLE : CUI_ICON_CIRCLE_CHECK; o.tone = CUI_TONE_SUCCESS; }
        if (!t.empty() && cui_status_bar_item_ex(ui, t.c_str(), &o) && errors) { a.showGallery = false; a.open[kCode] = true; a.lowerTab = 0; }
    }
    o = {}; o.icon = CUI_ICON_SPARKLES;
    bool ready = a.agentReady();
    o.tone = !ready ? CUI_TONE_WARNING : a.agent.busy() ? CUI_TONE_INFO : CUI_TONE_NEUTRAL;
    o.tooltip = "The agent. Click for its settings.";
    std::string agent = !ready ? "Agent not set up" : a.agent.busy() ? "Agent working" : a.modelBuf;
    if (cui_status_bar_item_ex(ui, agent.c_str(), &o)) a.showSettings = true;
    cui_status_bar_end(ui);
}

void frame(cui_ctx* ui, void* user) {
    App& a = *(App*)user;
    a.ctx = ui;
    a.pumpQueue();
    a.pumpAgent();
    a.pumpPreview(ui);
    if (a.busy()) cui_request_wakeup(ui, 0.1f);     // the workers cannot wake the loop themselves

    if (cui_shortcut(ui, CUI_MOD_PRIMARY, CUI_KEY_S)) a.saveAll();
    if (cui_shortcut(ui, CUI_MOD_PRIMARY, CUI_KEY_K) || cui_shortcut(ui, CUI_MOD_PRIMARY, CUI_KEY_P)) a.showPalette = true;
    if (cui_shortcut(ui, CUI_MOD_PRIMARY, CUI_KEY_N)) a.askNewScript(false);
    if (cui_shortcut(ui, CUI_MOD_PRIMARY, CUI_KEY_E)) { a.showGallery = !a.showGallery || a.codePath.empty(); a.open[kCode] = true; }
    if (cui_shortcut(ui, CUI_MOD_PRIMARY, CUI_KEY_R)) { a.failedKey.clear(); a.renderStill(true); }
    if (cui_shortcut(ui, CUI_MOD_PRIMARY, CUI_KEY_COMMA)) a.showSettings = true;
    if (cui_shortcut(ui, 0, CUI_KEY_F5)) a.liveWindow();
    for (auto& t : a.toasts) {
        cui_toast_opts o{}; o.tone = t.tone; o.description = t.detail.empty() ? nullptr : t.detail.c_str();
        if (t.tone == CUI_TONE_DANGER) o.duration = 8;
        cui_toast_ex(ui, t.title.c_str(), &o);
    }
    a.toasts.clear();

    menus(ui, a);
    dialogs(ui, a);
    palette(ui, a);

    cui_dockspace_opts ds{};
    ds.layout = a.savedLayout.empty() ? kDefaultLayout : a.savedLayout.c_str();
    ds.shortcuts = true;
    cui_dockspace_begin_ex(ui, "main", &ds);
    struct P { int idx; void (*fn)(cui_ctx*, App&); bool dirty; const char* icon; };
    P panels[kPanels] = {
        { kBrief, briefPanel, a.briefDirty, CUI_ICON_NOTEBOOK_PEN }, { kCode, codePanel, a.codeDirty, CUI_ICON_CODE },
        { kAssets, assetsPanel, false, CUI_ICON_FOLDER }, { kPreview, previewPanel, false, CUI_ICON_MONITOR },
        { kChat, chatPanel, false, CUI_ICON_SPARKLES },
    };
    for (auto& p : panels) {
        cui_dock_panel_opts po{}; po.dirty = p.dirty; po.icon = p.icon;
        if (cui_dock_panel_begin_ex(ui, a.panelNames[p.idx], &a.open[p.idx], &po)) {
            p.fn(ui, a);
            cui_dock_panel_end(ui);
        }
    }
    cui_dockspace_end(ui);
    statusBar(ui, a);
}

} // namespace

int main(int argc, char** argv) {
    App app;
    app.root = fs::current_path();
    app.loadSettings();
    std::string openArg;
    bool hidden = false, fresh = false;
    struct Click { int at; float x, y; };
    std::vector<Click> clicks;               // --click FRAME:X:Y, for screenshots of states behind a click
    for (int i = 1; i < argc; ++i) {
        bool more = i + 1 < argc;
        if (!strcmp(argv[i], "--workspace") && more) app.root = fs::weakly_canonical(fromUtf8(argv[++i]));
        else if (!strcmp(argv[i], "--url") && more) app.set.baseUrl = argv[++i];
        else if (!strcmp(argv[i], "--model") && more) app.set.model = argv[++i];
        else if (!strcmp(argv[i], "--key") && more) app.set.apiKey = argv[++i];
        else if (!strcmp(argv[i], "--open") && more) openArg = argv[++i];
        else if (!strcmp(argv[i], "--agent") && more) ++i;
        else if (!strcmp(argv[i], "--hidden")) hidden = true;
        else if (!strcmp(argv[i], "--fresh")) fresh = true;          // ignore the last session
        else if (!strcmp(argv[i], "--click") && more) { Click c{}; if (sscanf(argv[++i], "%d:%f:%f", &c.at, &c.x, &c.y) == 3) clicks.push_back(c); }
        else if (argv[i][0] != '-' && openArg.empty()) openArg = argv[i];
    }
    app.syncBuffers();
    app.rescan();

    // Headless: `raymotion_studio --agent "prompt"` runs one agent turn and prints the events.
    // Settings come from the settings file, with --url / --model / --key overriding them.
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--list-models") != 0) continue;
        ModelList m = fetchModels(app.set);
        for (const auto& id : m.ids) printf("%s\n", id.c_str());
        if (!m.error.empty()) { fprintf(stderr, "error: %s\n", m.error.c_str()); return 1; }
        return 0;
    }
    for (int i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "--agent") != 0) continue;
        app.agent.start(app.set, app.root, argv[i + 1]);
        bool done = false;
        while (!done) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            for (const AgentEvent& e : app.agent.drain()) {
                using K = AgentEvent::Kind;
                const char* k = e.kind == K::Assistant ? "assistant" : e.kind == K::ToolStart ? "tool>" : e.kind == K::ToolEnd ? (e.ok ? "tool<" : "tool<FAIL")
                              : e.kind == K::FileWritten ? "wrote" : e.kind == K::StillReady ? "still" : e.kind == K::Error ? "ERROR" : "done";
                printf("[%s] %s %s\n", k, e.name.c_str(), e.text.substr(0, 300).c_str());
                if (e.kind == K::Done) done = true;
            }
        }
        return 0;
    }

    cui_app_desc desc{};
    desc.title = "Raymotion Studio";
    desc.width = 1600; desc.height = 900;
    desc.hidden = hidden;
    gApp = cui_app_create(&desc);
    if (!gApp) { fprintf(stderr, "could not open a window\n"); return 1; }
    cui_ctx* first = cui_app_ui(gApp);
    app.ctx = first;
    cui_set_theme(first, app.set.darkTheme ? CUI_THEME_DARK : CUI_THEME_LIGHT);
    if (!fresh) app.restoreSession();
    if (!openArg.empty()) {
        std::error_code ec;
        fs::path p = fromUtf8(openArg);
        if (p.is_relative() && !fs::exists(p, ec)) p = app.root / p;
        if (fs::is_regular_file(p, ec)) {
            if (lowerExt(p) == ".md") app.openBrief(fs::weakly_canonical(p, ec));
            else app.openScript(fs::weakly_canonical(p, ec));
        } else fprintf(stderr, "cannot open %s\n", openArg.c_str());
    }
    int n = 0;
    while (true) {
        if (!cui_app_frame(gApp)) break;
        cui_ctx* ui = cui_app_ui(gApp);
        for (const auto& c : clicks) {       // after the frame began, so this frame sees them
            if (n == c.at) cui_input_mouse_move(ui, c.x, c.y);
            if (n == c.at + 2) cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
            if (n == c.at + 4) cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
        }
        frame(ui, &app);
        ++n;
    }
    if (!fresh && clicks.empty()) app.writeSettingsFile();   // remember what was open
    if (app.still) cui_image_destroy(app.ctx, app.still);
    cui_app_destroy(gApp);
    app.ctx = nullptr;
    gApp = nullptr;
    return 0;
}
