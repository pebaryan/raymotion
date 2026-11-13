// raylib's image functions work on the CPU, so this needs no window or GL context. It lives in
// its own file because raylib.h and the UI headers do not belong in one translation unit.
#include "thumb.hpp"
#include <raylib.h>

namespace studio {

bool makeThumb(const std::string& src, const std::string& dst, int width) {
    SetTraceLogLevel(LOG_ERROR);
    Image im = LoadImage(src.c_str());
    if (!im.data || im.width <= 0 || im.height <= 0) return false;
    if (im.width > width) {
        const int height = (int)((long long)im.height * width / im.width);
        ImageResize(&im, width, height > 0 ? height : 1);
    }
    const bool ok = ExportImage(im, dst.c_str());
    UnloadImage(im);
    return ok;
}

} // namespace studio
