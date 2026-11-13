#include "proc.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cerrno>
#endif

namespace studio {

std::string exeDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH]; DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return n ? std::filesystem::path(buf).parent_path().u8string().c_str() : std::string();
#else
    std::error_code ec;
    return std::filesystem::read_symlink("/proc/self/exe", ec).parent_path().string();
#endif
}

namespace {
#ifdef _WIN32
std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}
// Quoting rules of the Windows CRT command-line parser.
std::wstring commandLine(const std::vector<std::string>& args) {
    std::wstring out;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) out += L' ';
        std::wstring a = widen(args[i]);
        out += L'"';
        const wchar_t kSlash = 92;   // backslash, spelled as a number to keep escaping out of it
        size_t slashes = 0;
        for (wchar_t ch : a) {
            if (ch == kSlash) { ++slashes; continue; }
            out.append(slashes * (ch == L'"' ? 2 : 1), kSlash);
            if (ch == L'"') out += kSlash;
            out += ch;
            slashes = 0;
        }
        out.append(slashes * 2, kSlash);
        out += L'"';
    }
    return out;
}
#endif
} // namespace

ProcResult runCapture(const std::vector<std::string>& args, const std::string& cwd) {
    ProcResult r;
    if (args.empty()) return r;
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{ sizeof sa, nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return r;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr; si.hStdError = wr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    std::wstring cmd = commandLine(args), wcwd = widen(cwd);
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                             wcwd.empty() ? nullptr : wcwd.c_str(), &si, &pi);
    CloseHandle(wr);
    if (!ok) { CloseHandle(rd); r.output = "cannot start '" + args[0] + "'"; return r; }
    char buf[4096]; DWORD got;
    while (ReadFile(rd, buf, sizeof buf, &got, nullptr) && got) r.output.append(buf, got);
    CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code);
    r.code = (int)code;
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
#else
    int fds[2];
    if (pipe(fds) != 0) return r;
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return r; }
    if (pid == 0) {
        if (!cwd.empty() && chdir(cwd.c_str()) != 0) _exit(127);
        dup2(fds[1], 1); dup2(fds[1], 2); close(fds[0]); close(fds[1]);
        std::vector<char*> argv;
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    close(fds[1]);
    char buf[4096]; ssize_t n;
    while ((n = read(fds[0], buf, sizeof buf)) > 0 || (n < 0 && errno == EINTR)) if (n > 0) r.output.append(buf, (size_t)n);
    close(fds[0]);
    int st = 0; waitpid(pid, &st, 0);
    r.code = WIFEXITED(st) ? WEXITSTATUS(st) : 1;
    if (r.code == 127 && r.output.empty()) r.output = "cannot start '" + args[0] + "'";
#endif
    return r;
}

bool spawnDetached(const std::vector<std::string>& args, const std::string& cwd) {
    if (args.empty()) return false;
#ifdef _WIN32
    STARTUPINFOW si{}; si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    std::wstring cmd = commandLine(args), wcwd = widen(cwd);
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        wcwd.empty() ? nullptr : wcwd.c_str(), &si, &pi)) return false;
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return true;
#else
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        if (fork() != 0) _exit(0);              // double fork: no zombie
        if (!cwd.empty() && chdir(cwd.c_str()) != 0) _exit(127);
        std::vector<char*> argv;
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    int st = 0; waitpid(pid, &st, 0);
    return true;
#endif
}

} // namespace studio
