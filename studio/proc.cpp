#include "proc.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
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
unsigned long currentPid() {
#ifdef _WIN32
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}
bool processRunning(unsigned long pid) {
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h) return GetLastError() == ERROR_ACCESS_DENIED;   // exists, but not ours to open
    DWORD code = 0;
    const bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
#else
    return kill((pid_t)pid, 0) == 0 || errno == EPERM;
#endif
}
} // namespace

std::filesystem::path sessionDir(const std::filesystem::path& root) {
    return root / "out" / "studio" / "sessions" / std::to_string(currentPid());
}

void removeStaleSessions(const std::filesystem::path& root) {
    std::error_code ec;
    const std::filesystem::path all = root / "out" / "studio" / "sessions";
    for (const auto& entry : std::filesystem::directory_iterator(all, ec)) {
        const std::string name = entry.path().filename().string();
        if (name.empty() || name.find_first_not_of("0123456789") != std::string::npos || name.size() > 10) continue;
        const unsigned long pid = std::stoul(name);
        if (pid == currentPid() || processRunning(pid)) continue;
        std::error_code rm;
        std::filesystem::remove_all(entry.path(), rm);
    }
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

Spawned& Spawned::operator=(Spawned&& o) noexcept {
    if (this != &o) {
#ifdef _WIN32
        if (handle_) CloseHandle((HANDLE)handle_);
#endif
        handle_ = o.handle_; pid_ = o.pid_;
        o.handle_ = nullptr; o.pid_ = 0;
    }
    return *this;
}

Spawned::~Spawned() {
#ifdef _WIN32
    if (handle_) CloseHandle((HANDLE)handle_);   // the process keeps running
#else
    if (pid_) { int st; waitpid((pid_t)pid_, &st, WNOHANG); }
#endif
}

bool Spawned::running() {
    if (!pid_) return false;
#ifdef _WIN32
    return WaitForSingleObject((HANDLE)handle_, 0) == WAIT_TIMEOUT;
#else
    int st;
    return waitpid((pid_t)pid_, &st, WNOHANG) == 0;   // also reaps it once it has exited
#endif
}

void Spawned::stop() {
    if (!running()) return;
#ifdef _WIN32
    TerminateProcess((HANDLE)handle_, 0);
    WaitForSingleObject((HANDLE)handle_, 2000);
#else
    kill((pid_t)pid_, SIGTERM);
    int st;
    for (int i = 0; i < 40 && waitpid((pid_t)pid_, &st, WNOHANG) == 0; ++i) usleep(50000);
#endif
}

Spawned spawn(const std::vector<std::string>& args, const std::string& cwd) {
    Spawned s;
    if (args.empty()) return s;
#ifdef _WIN32
    STARTUPINFOW si{}; si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    std::wstring cmd = commandLine(args), wcwd = widen(cwd);
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        wcwd.empty() ? nullptr : wcwd.c_str(), &si, &pi)) return s;
    CloseHandle(pi.hThread);
    s.handle_ = pi.hProcess;
    s.pid_ = (long)pi.dwProcessId;
#else
    pid_t pid = fork();
    if (pid < 0) return s;
    if (pid == 0) {
        if (!cwd.empty() && chdir(cwd.c_str()) != 0) _exit(127);
        std::vector<char*> argv;
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    s.pid_ = (long)pid;
#endif
    return s;
}

} // namespace studio
