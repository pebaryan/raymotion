// rmtn/flags.hpp -- built-in country flag drawings and animated prefabs.
#pragma once
#include <string>
#include <vector>
#include "timeline.hpp"

namespace rmtn::flags {

// Supported lower-case ISO 3166-1 alpha-2 codes. Unknown codes throw.
const std::vector<std::string>& countries();
std::string title(const std::string& code);

// Draw a 3:2 flag centred on `centre`, `width` pixels wide. When badge is true,
// the flag sits on a rounded square. reveal grows the drawing from the centre.
void draw(const Ctx& c, const std::string& code, Vector2 centre, float width,
          bool badge = true, float reveal = 1);

// Registers flag-at, flag-de, ... and flag-row. Safe to call more than once.
void registerPrefabs();

} // namespace rmtn::flags
