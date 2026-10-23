// fonts.cpp -- font search paths, cached loading and release.
#include "rmtn/fonts.hpp"
#include <cstdlib>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;

namespace rmtn { namespace fonts {
namespace {

std::vector<std::string> extraDirs;                       // addSearchPath, latest first
std::map<std::string, std::string> foundCache;            // file name -> path ("" = absent)
std::map<std::pair<std::string, int>, Font> loaded;       // (path, size) -> font

std::string envDir(const char* var, const char* suffix) {
    const char* v = std::getenv(var);
    return (v && *v) ? std::string(v) + suffix : std::string();
}

// Directories searched non-recursively (project assets) and recursively (system fonts).
std::vector<std::string> assetDirs() {
    std::vector<std::string> dirs(extraDirs.begin(), extraDirs.end());
    dirs.push_back(envDir("RAYMOTION_ASSETS", "/fonts"));
    dirs.push_back(std::string(GetApplicationDirectory()) + "assets/fonts");
#ifdef RMTN_BUNDLED_ASSETS
    dirs.push_back(std::string(RMTN_BUNDLED_ASSETS) + "/fonts");
#endif
    return dirs;
}
std::vector<std::string> systemDirs() {
#if defined(_WIN32)
    return { envDir("WINDIR", "/Fonts").empty() ? "C:/Windows/Fonts" : envDir("WINDIR", "/Fonts"),
             envDir("LOCALAPPDATA", "/Microsoft/Windows/Fonts") };
#elif defined(__APPLE__)
    return { envDir("HOME", "/Library/Fonts"), "/Library/Fonts", "/System/Library/Fonts",
             "/System/Library/Fonts/Supplemental" };
#else
    return { envDir("HOME", "/.local/share/fonts"), envDir("HOME", "/.fonts"),
             "/usr/local/share/fonts", "/usr/share/fonts" };
#endif
}

std::string lookIn(const std::string& dir, const std::string& file, bool recursive) {
    if (dir.empty()) return "";
    std::error_code ec;
    fs::path direct = fs::u8path(dir) / fs::u8path(file);
    if (fs::is_regular_file(direct, ec)) return direct.u8string();
    if (!recursive || !fs::is_directory(fs::u8path(dir), ec)) return "";
    for (fs::recursive_directory_iterator it(fs::u8path(dir), fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
        if (it->path().filename().u8string() == file && it->is_regular_file(ec)) return it->path().u8string();
    }
    return "";
}

// ASCII, Latin-1 and common typographic punctuation (glyphs a face lacks draw as '?').
std::vector<int> glyphSet() {
    std::vector<int> cps;
    for (int c = 32; c < 127; c++) cps.push_back(c);
    for (int c = 160; c < 256; c++) if (c != 0xAD) cps.push_back(c);   // soft hyphen never renders
    for (int c : { 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026,
                   0x2190, 0x2192, 0x2260, 0x2264, 0x2265, 0x2212 }) cps.push_back(c);
    return cps;
}

} // namespace

void addSearchPath(const std::string& dir) {
    extraDirs.insert(extraDirs.begin(), dir);
    foundCache.clear();
}

std::string find(const std::string& file) {
    auto hit = foundCache.find(file);
    if (hit != foundCache.end()) return hit->second;
    std::string path;
    for (const auto& d : assetDirs()) if (path.empty()) path = lookIn(d, file, false);
    for (const auto& d : systemDirs()) if (path.empty()) path = lookIn(d, file, true);
    foundCache[file] = path;
    return path;
}

Font load(const std::vector<std::string>& candidates, int baseSize) {
    std::string tried;
    for (const auto& name : candidates) {
        std::string path = find(name);
        tried += (tried.empty() ? "" : ", ") + name;
        if (path.empty()) continue;
        auto key = std::make_pair(path, baseSize);
        auto hit = loaded.find(key);
        if (hit != loaded.end()) return hit->second;
        std::vector<int> cps = glyphSet();
        Font font = LoadFontEx(path.c_str(), baseSize, cps.data(), (int)cps.size());
        if (!IsFontValid(font) || font.texture.id == GetFontDefault().texture.id) continue;
        GenTextureMipmaps(&font.texture);
        SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
        loaded[key] = font;
        return font;
    }
    throw std::runtime_error("no usable font among: " + tried);
}

Font ui(bool bold, int baseSize) {
    return load({ bold ? "Inter-Bold.ttf" : "Inter-Regular.ttf" }, baseSize);
}

void releaseAll() {
    for (auto& entry : loaded) UnloadFont(entry.second);
    loaded.clear();
}

} } // rmtn::fonts
