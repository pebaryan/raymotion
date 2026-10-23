#pragma once
#include <raylib.h>
#include <algorithm>
namespace rmtn {
inline Rectangle fitCanvas(int canvasW,int canvasH,int windowW,int windowH){
    if(canvasW<=0 || canvasH<=0 || windowW<=0 || windowH<=0) return {};
    float scale=std::min((float)windowW/canvasW,(float)windowH/canvasH);
    float w=canvasW*scale,h=canvasH*scale;
    return {((float)windowW-w)*.5f,((float)windowH-h)*.5f,w,h};
}
}
