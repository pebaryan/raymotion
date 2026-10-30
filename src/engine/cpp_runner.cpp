#include "rmtn/json.hpp"
#include "CppInfo.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <cctype>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <cerrno>
#endif

namespace rmtn {
namespace {
int launch(const std::vector<std::string>& args) {
#ifdef _WIN32
    // _wspawnv concatenates arguments; quote each for the Windows CRT parser.
    std::vector<std::wstring> storage;
    for (const auto& arg : args) {
        auto wide = std::filesystem::u8path(arg).wstring();
        std::wstring quoted = L"\"";
        size_t slashes = 0;
        for (wchar_t ch : wide) {
            if (ch == L'\\') { ++slashes; continue; }
            quoted.append(slashes * (ch == L'"' ? 2 : 1), L'\\');
            if (ch == L'"') quoted += L'\\';
            quoted += ch; slashes = 0;
        }
        quoted.append(slashes * 2, L'\\'); quoted += L'"';
        storage.push_back(std::move(quoted));
    }
    std::vector<const wchar_t*> argv;
    for (const auto& arg : storage) argv.push_back(arg.c_str());
    argv.push_back(nullptr);
    auto executable = std::filesystem::u8path(args.front()).wstring();
    auto status = _wspawnv(_P_WAIT, executable.c_str(), argv.data());
    if (status == -1) throw std::runtime_error("cannot start CMake for C++ composition");
    return (int)status;
#else
    std::vector<char*> argv;
    for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    auto pid = fork();
    if (pid < 0) throw std::runtime_error("cannot start CMake for C++ composition");
    if (pid == 0) { execv(argv[0], argv.data()); _exit(127); }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0)
        if (errno != EINTR) throw std::runtime_error("cannot wait for C++ composition");
    return WIFEXITED(status) ? WEXITSTATUS(status) : 2;
#endif
}
}
// A native source runs in a separate executable linked to the prebuilt engine.
// This avoids crossing C++/STL or graphics-state boundaries between DLLs.
std::optional<int> runNativeCpp(int argc, const char** argv) {
    std::string source, entry = "registerComps";
    Json::Array forwarded;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--cpp-entry") {
            if (++i >= argc) throw std::runtime_error("--cpp-entry requires a function name");
            entry = argv[i]; continue;
        }
        if (arg == "--script" && i + 1 < argc) {
            auto extension = std::filesystem::u8path(argv[i + 1]).extension().string();
            for (char& ch : extension) ch = (char)std::tolower((unsigned char)ch);
            if (extension == ".cpp" || extension == ".cc" || extension == ".cxx") {
                if (!source.empty()) throw std::runtime_error("use one native C++ composition source per invocation");
                source = std::filesystem::absolute(std::filesystem::u8path(argv[++i])).generic_u8string();
                continue;
            }
        }
        forwarded.emplace_back(arg);
    }
    if (source.empty()) return std::nullopt;
    if (!std::filesystem::exists(RMTN_CPP_SDK))
        throw std::runtime_error("native C++ requires the original build-tree SDK: " RMTN_CPP_SDK);
    Json request(Json::Object{{"source", source}, {"entry", entry}, {"args", std::move(forwarded)},
        {"sdk", RMTN_CPP_SDK}, {"compiler", cpp_info::compiler}, {"generator", cpp_info::generator},
        {"make_program", cpp_info::makeProgram}, {"config", RMTN_CPP_CONFIG},
        {"platform", cpp_info::platform}, {"toolset", cpp_info::toolset}});
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
#ifdef _WIN32
    const auto pid = _getpid();
#else
    const auto pid = getpid();
#endif
    auto requests = std::filesystem::path(RMTN_CPP_SDK).parent_path() / "requests";
    std::filesystem::create_directories(requests);
    auto path = requests / ("raymotion-cpp-" + std::to_string(pid) + "-" + std::to_string(stamp) + ".json");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ec; std::filesystem::remove(path, ec); }
    } cleanup{path};
    std::ofstream output(path, std::ios::binary);
    output << request.dump(); output.close();
    if (!output) throw std::runtime_error("cannot write C++ runner request");
    const int status = launch({RMTN_CMAKE, "-DRMTN_REQUEST=" + path.generic_u8string(), "-P", RMTN_CPP_DRIVER});
    return status == 0 ? 0 : 2;
}
} // namespace rmtn
