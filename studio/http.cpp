#include "http.hpp"
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>

namespace studio {
namespace {

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::string lastError(const char* what) {
    DWORD e = GetLastError();
    char buf[64];
    snprintf(buf, sizeof buf, " (WinHTTP error %lu)", (unsigned long)e);
    std::string msg = what;
    switch (e) {
    case ERROR_WINHTTP_TIMEOUT: msg += ": timed out"; break;
    case ERROR_WINHTTP_CANNOT_CONNECT: msg += ": cannot connect"; break;
    case ERROR_WINHTTP_NAME_NOT_RESOLVED: msg += ": host name not found"; break;
    case ERROR_WINHTTP_SECURE_FAILURE: msg += ": TLS certificate rejected"; break;
    case ERROR_WINHTTP_CONNECTION_ERROR: msg += ": connection lost"; break;
    default: break;
    }
    return msg + buf;
}

struct Handle {
    HINTERNET h = nullptr;
    ~Handle() { if (h) WinHttpCloseHandle(h); }
    explicit operator bool() const { return h != nullptr; }
};

} // namespace

HttpResponse httpRequest(const std::string& method, const std::string& url, const HttpHeaders& headers,
                         const std::string& body, int timeoutSeconds) {
    HttpResponse out;
    std::wstring wurl = widen(url);
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof uc;
    uc.dwSchemeLength = uc.dwHostNameLength = uc.dwUrlPathLength = uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc) || !uc.lpszHostName) {
        out.error = "invalid URL: " + url;
        return out;
    }
    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path(uc.lpszUrlPath ? uc.lpszUrlPath : L"/", uc.lpszUrlPath ? uc.dwUrlPathLength + uc.dwExtraInfoLength : 1);
    bool secure = uc.nScheme == INTERNET_SCHEME_HTTPS;

    // Automatic proxy support needs Windows 8.1; older systems fall back to the default.
    Handle session;
    session.h = WinHttpOpen(L"raymotion-studio/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) session.h = WinHttpOpen(L"raymotion-studio/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { out.error = lastError("WinHttpOpen"); return out; }
    int ms = timeoutSeconds > 0 ? timeoutSeconds * 1000 : 300000;
    WinHttpSetTimeouts(session.h, 10000, 10000, ms, ms);

    Handle conn;
    conn.h = WinHttpConnect(session.h, host.c_str(), uc.nPort, 0);
    if (!conn) { out.error = lastError("connect"); return out; }
    Handle req;
    req.h = WinHttpOpenRequest(conn.h, widen(method).c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
                               WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
    if (!req) { out.error = lastError("open request"); return out; }

    std::wstring hdr;
    for (const auto& h : headers) hdr += widen(h.first + ": " + h.second) + L"\r\n";
    if (!WinHttpSendRequest(req.h, hdr.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : hdr.c_str(), (DWORD)-1L,
                            body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0)) {
        out.error = lastError("send");
        return out;
    }
    if (!WinHttpReceiveResponse(req.h, nullptr)) { out.error = lastError("receive"); return out; }

    DWORD status = 0, size = sizeof status;
    WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    out.status = (int)status;
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(req.h, &avail)) { out.error = lastError("read"); return out; }
        if (!avail) break;
        size_t at = out.body.size();
        out.body.resize(at + avail);
        DWORD got = 0;
        if (!WinHttpReadData(req.h, &out.body[at], avail, &got)) { out.body.resize(at); out.error = lastError("read"); return out; }
        out.body.resize(at + got);
    }
    return out;
}

} // namespace studio

#else   // ---------------------------------------------------------------- POSIX: curl
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include "proc.hpp"

namespace studio {

HttpResponse httpRequest(const std::string& method, const std::string& url, const HttpHeaders& headers,
                         const std::string& body, int timeoutSeconds) {
    namespace fs = std::filesystem;
    HttpResponse out;
    std::error_code ec;
    fs::path dir = fs::temp_directory_path(ec) / ("raymotion-studio-" + std::to_string((long)getpid()));
    fs::create_directories(dir, ec);
    fs::path hdrFile = dir / "headers.txt", bodyFile = dir / "body.json", respFile = dir / "response.txt";
    { std::ofstream h(hdrFile); for (const auto& kv : headers) h << kv.first << ": " << kv.second << "\n"; }
    std::vector<std::string> args = { "curl", "-sS", "--max-time", std::to_string(timeoutSeconds > 0 ? timeoutSeconds : 300),
                                      "-X", method, url, "-H", "@" + hdrFile.string(), "-o", respFile.string(), "-w", "%{http_code}" };
    if (!body.empty()) { std::ofstream(bodyFile, std::ios::binary) << body; args.push_back("--data-binary"); args.push_back("@" + bodyFile.string()); }
    ProcResult r = runCapture(args);
    std::ifstream f(respFile, std::ios::binary);
    std::ostringstream ss; ss << f.rdbuf();
    out.body = ss.str();
    fs::remove_all(dir, ec);
    if (r.code != 0) out.error = "curl failed: " + r.output;
    else out.status = atoi(r.output.c_str());
    return out;
}

} // namespace studio
#endif
