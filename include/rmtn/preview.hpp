#pragma once
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
namespace rmtn {
inline Rectangle fitCanvas(int canvasW,int canvasH,int windowW,int windowH){
    if(canvasW<=0 || canvasH<=0 || windowW<=0 || windowH<=0) return {};
    float scale=std::min((float)windowW/canvasW,(float)windowH/canvasH);
    float w=canvasW*scale,h=canvasH*scale;
    return {((float)windowW-w)*.5f,((float)windowH-h)*.5f,w,h};
}

// The preview window's seek bar: a strip along the bottom edge (above `bottomInset`,
// the height of anything already drawn there) holding a time readout and a track.
struct Scrubber {
    Rectangle strip{};   // the whole bar; a press anywhere in it starts a drag
    Rectangle track{};   // the line the playhead moves along
};
inline Scrubber scrubber(int windowW,int windowH,int bottomInset=0){
    const float stripH=44, margin=16;
    Scrubber s;
    s.strip={0,(float)(windowH-bottomInset)-stripH,(float)windowW,stripH};
    s.track={margin,s.strip.y+30,std::max(0.f,(float)windowW-2*margin),6};
    return s;
}
// The frame under a pointer at x: the track's ends are the first and last frames, and a
// pointer past either end stays on that frame.
inline int scrubFrame(const Rectangle& track,float x,int duration){
    if(duration<=1 || track.width<=0) return 0;
    const float u=std::clamp((x-track.x)/track.width,0.f,1.f);
    return std::min(duration-1,(int)std::lround(u*(float)(duration-1)));
}
inline float scrubX(const Rectangle& track,int frame,int duration){
    if(duration<=1) return track.x;
    return track.x+track.width*(float)std::clamp(frame,0,duration-1)/(float)(duration-1);
}
// m:ss.ff, where ff is the frame within the second.
inline std::string timecode(int frame,int fps){
    if(fps<=0) return "0:00.00";
    frame=std::max(0,frame);
    const int seconds=frame/fps;
    char out[32];
    std::snprintf(out,sizeof out,"%d:%02d.%02d",seconds/60,seconds%60,frame%fps);
    return out;
}
}
