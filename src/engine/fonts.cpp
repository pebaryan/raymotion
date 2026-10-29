// fonts.cpp -- font search paths, cached loading and release.
#include "rmtn/fonts.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace rmtn { namespace fonts {
namespace {

std::vector<std::string> extraDirs;                       // addSearchPath, latest first
std::map<std::string, std::string> foundCache;            // file name -> path ("" = absent)
std::map<std::pair<std::string, int>, Font> loaded;       // (path, size) -> font
std::map<std::string, Font> loadedFor;                    // (path, size, code points) -> font

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

// UTF-8 -> code points; malformed bytes are skipped (they would only add a '?').
std::vector<int> decodeUtf8(const std::string& s) {
    std::vector<int> out;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = (unsigned char)s[i];
        int cp = 0, extra = 0;
        if (c < 0x80) { cp = c; extra = 0; }
        else if ((c >> 5) == 0x06) { cp = c & 0x1F; extra = 1; }
        else if ((c >> 4) == 0x0E) { cp = c & 0x0F; extra = 2; }
        else if ((c >> 3) == 0x1E) { cp = c & 0x07; extra = 3; }
        else { i++; continue; }
        if (i + (size_t)extra >= s.size()) break;
        bool ok = true;
        for (int k = 1; k <= extra; k++) {
            const unsigned char cc = (unsigned char)s[i + k];
            if ((cc >> 6) != 0x02) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        i += (size_t)extra + 1;
        if (ok) out.push_back(cp);
    }
    return out;
}

std::vector<int> glyphsFor(const std::string& sampleUtf8) {
    std::vector<int> cps = glyphSet();
    for (int cp : decodeUtf8(sampleUtf8)) cps.push_back(cp);
    std::sort(cps.begin(), cps.end());
    cps.erase(std::unique(cps.begin(), cps.end()), cps.end());
    return cps;
}

// FNV-1a over the sorted code points: the cache key for loadFor.
std::uint64_t glyphHash(const std::vector<int>& cps) {
    std::uint64_t h = 1469598103934665603ull;
    for (int cp : cps) { h ^= (std::uint64_t)(std::uint32_t)cp; h *= 1099511628211ull; }
    return h;
}

// ---- TrueType collections ----------------------------------------------------------
// raylib's LoadFontEx hands stb_truetype offset 0, so a .ttc (font collection) fails.
// Extract the first member as a standalone sfnt in memory: copy its table directory and
// rebase the table offsets into a fresh buffer, then load that with LoadFontFromMemory.

std::uint32_t be32(const unsigned char* p) {
    return ((std::uint32_t)p[0] << 24) | ((std::uint32_t)p[1] << 16) | ((std::uint32_t)p[2] << 8) | p[3];
}
std::uint16_t be16(const unsigned char* p) { return (std::uint16_t)(((std::uint16_t)p[0] << 8) | p[1]); }
void put32(std::vector<unsigned char>& v, size_t at, std::uint32_t x) {
    v[at] = (unsigned char)(x >> 24); v[at + 1] = (unsigned char)(x >> 16);
    v[at + 2] = (unsigned char)(x >> 8); v[at + 3] = (unsigned char)x;
}
void put16(std::vector<unsigned char>& v, size_t at, std::uint16_t x) {
    v[at] = (unsigned char)(x >> 8); v[at + 1] = (unsigned char)x;
}

bool isCollection(const std::string& path) {
    std::string ext = path;
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return ext.size() > 4 && (ext.compare(ext.size() - 4, 4, ".ttc") == 0 ||
                              ext.compare(ext.size() - 4, 4, ".otc") == 0);
}

bool firstCollectionMember(const std::string& path, std::vector<unsigned char>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<unsigned char> in((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (in.size() < 20 || be32(in.data()) != 0x74746366u) return false;      // 'ttcf'
    const std::uint32_t count = be32(in.data() + 8);
    if (count == 0) return false;
    const std::uint32_t off = be32(in.data() + 12);
    if ((std::uint64_t)off + 12 > in.size()) return false;
    const std::uint16_t tables = be16(in.data() + off + 4);
    if ((std::uint64_t)off + 12 + 16ull * tables > in.size()) return false;
    out.assign(12 + 16 * (size_t)tables, 0);
    put32(out, 0, be32(in.data() + off));
    put16(out, 4, tables);
    std::uint16_t pow2 = 1; std::uint16_t log2 = 0;
    while ((std::uint32_t)pow2 * 2 <= tables) { pow2 = (std::uint16_t)(pow2 * 2); log2++; }
    put16(out, 6, (std::uint16_t)(pow2 * 16));
    put16(out, 8, log2);
    put16(out, 10, (std::uint16_t)(tables * 16 - pow2 * 16));
    size_t cursor = 12 + 16 * (size_t)tables;
    for (std::uint16_t i = 0; i < tables; i++) {
        const unsigned char* rec = in.data() + off + 12 + 16 * (size_t)i;
        const std::uint32_t tag = be32(rec), checksum = be32(rec + 4);
        const std::uint32_t toff = be32(rec + 8), len = be32(rec + 12);
        if ((std::uint64_t)toff + len > in.size()) return false;
        while (cursor % 4) { out.push_back(0); cursor++; }
        put32(out, 12 + 16 * (size_t)i, tag);
        put32(out, 12 + 16 * (size_t)i + 4, checksum);
        put32(out, 12 + 16 * (size_t)i + 8, (std::uint32_t)cursor);
        put32(out, 12 + 16 * (size_t)i + 12, len);
        out.insert(out.end(), in.begin() + toff, in.begin() + toff + len);
        cursor += len;
    }
    return true;
}

Font loadFile(const std::string& path, int baseSize, const std::vector<int>& cps) {
    std::vector<int> mutableCps = cps;
    if (isCollection(path)) {
        std::vector<unsigned char> sfnt;
        if (!firstCollectionMember(path, sfnt)) return Font{};
        return LoadFontFromMemory(".ttf", sfnt.data(), (int)sfnt.size(), baseSize,
                                  mutableCps.data(), (int)mutableCps.size());
    }
    return LoadFontEx(path.c_str(), baseSize, mutableCps.data(), (int)mutableCps.size());
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
        Font font = loadFile(path, baseSize, cps);
        if (!IsFontValid(font) || font.texture.id == GetFontDefault().texture.id) continue;
        GenTextureMipmaps(&font.texture);
        SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
        loaded[key] = font;
        return font;
    }
    throw std::runtime_error("no usable font among: " + tried);
}

Font loadFor(const std::vector<std::string>& candidates, const std::string& sampleUtf8, int baseSize) {
    const std::vector<int> cps = glyphsFor(sampleUtf8);
    const std::uint64_t hash = glyphHash(cps);
    std::string tried;
    for (const auto& name : candidates) {
        std::string path = find(name);
        tried += (tried.empty() ? "" : ", ") + name;
        if (path.empty()) continue;
        std::string key = path + "|" + std::to_string(baseSize) + "|" + std::to_string(hash);
        auto hit = loadedFor.find(key);
        if (hit != loadedFor.end()) return hit->second;
        Font font = loadFile(path, baseSize, cps);
        if (!IsFontValid(font) || font.texture.id == GetFontDefault().texture.id) continue;
        GenTextureMipmaps(&font.texture);
        SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
        loadedFor[key] = font;
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
    for (auto& entry : loadedFor) UnloadFont(entry.second);
    loadedFor.clear();
}

} } // rmtn::fonts
