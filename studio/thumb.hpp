// thumb.hpp -- shrink a rendered still into a gallery thumbnail (CPU only, no window).
#pragma once
#include <string>

namespace studio {

// Reads the PNG at `src`, scales it to `width` px wide and writes `dst`. False on failure.
bool makeThumb(const std::string& src, const std::string& dst, int width);

} // namespace studio
