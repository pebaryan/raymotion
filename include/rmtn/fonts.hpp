// rmtn/fonts.hpp -- font lookup, loading and lifetime.
#pragma once
#include <raylib.h>
#include <string>
#include <vector>

namespace rmtn { namespace fonts {

// Font files are looked up by file name, in order: directories added with addSearchPath
// (latest first), $RAYMOTION_ASSETS/fonts, <executable dir>/assets/fonts, the engine's
// bundled assets/fonts, then the platform font directories (searched recursively).
void addSearchPath(const std::string& dir);
std::string find(const std::string& file);          // full path, or "" if not found

// Loads the first candidate file that exists, rasterised at `baseSize` px with mipmapped
// trilinear filtering (crisp when drawn at or below baseSize). Results are cached per
// (file, size) until releaseAll(). Throws std::runtime_error if no candidate loads.
// Needs an open window (GL context).
Font load(const std::vector<std::string>& candidates, int baseSize = 96);

// Same, but the atlas also gets every code point in `sampleUtf8`, so a face outside the
// Latin-1 set (Japanese, Chinese, ...) can be drawn. Only the glyphs the sample uses are
// rasterised, which keeps the atlas small; pass every string the video will show.
// Results are cached per (file, size, code point set).
Font loadFor(const std::vector<std::string>& candidates, const std::string& sampleUtf8, int baseSize = 96);

// The engine's default face: bundled Inter (SIL OFL 1.1), regular or bold.
Font ui(bool bold = false, int baseSize = 96);

// Unloads every cached font. The engine calls this from shutdownGraphics().
void releaseAll();

} } // rmtn::fonts
