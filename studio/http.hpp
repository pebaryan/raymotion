// http.hpp -- a minimal blocking HTTP(S) client for the agent and the model list.
//
// Windows: WinHTTP (system proxy settings and certificate store, no child process, nothing
// written to disk, so an API key never leaves memory). Elsewhere: the `curl` executable.
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace studio {

struct HttpResponse {
    int status = 0;          // HTTP status; 0 when the request itself failed
    std::string body;
    std::string error;       // transport error ("" when a response arrived, whatever its status)
};

using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

// Blocks up to `timeoutSeconds` for each phase (connect, send, receive).
HttpResponse httpRequest(const std::string& method, const std::string& url, const HttpHeaders& headers,
                         const std::string& body, int timeoutSeconds);

} // namespace studio
