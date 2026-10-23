#pragma once
#include <cstdlib>
#include <string>
namespace rmtn {
// Path of an installed Windows font, honouring a non-default Windows directory.
inline std::string systemFontPath(const char* file){
    const char* windir=std::getenv("WINDIR");
    return std::string(windir && *windir ? windir : "C:/Windows") + "/Fonts/" + file;
}
}
