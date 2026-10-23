// rmtn/render.hpp -- composition-sized render targets with supersampled antialiasing.
#pragma once
#include <raylib.h>
#include <functional>
#include <vector>
#include "timeline.hpp"

namespace rmtn {

// Offscreen target for one composition. With supersample N > 1 the scene is drawn at
// N x the composition size and box-filtered down by repeated exact halving, which
// antialiases every edge, line and glyph (raylib render textures have no MSAA).
class Canvas {
public:
    // supersample is rounded down to 1, 2 or 4. Returns false if a target failed.
    bool load(int w, int h, int supersample);
    void unload();
    int width() const { return w_; }
    int height() const { return h_; }
    int scale() const { return ss_; }
    RenderTexture drawTarget() const { return levels_.front(); }   // (w*N) x (h*N)
    RenderTexture output() const { return levels_.back(); }        // w x h, raylib RT orientation
    void resolve();                                                // drawTarget -> output
private:
    int w_ = 0, h_ = 0, ss_ = 1;
    std::vector<RenderTexture> levels_;
};

// Draws frame `frame` of `c` into canvas.output(): the custom render hook if the
// composition has one, otherwise the timeline tree (in composition coordinates,
// supersampled when the canvas scale is > 1).
void presentFrame(Comp& c, int frame, Canvas& canvas);

// GL-owning code registers a release callback; shutdownGraphics() runs them all (then
// unloads cached fonts) and must be called before CloseWindow.
void onGraphicsShutdown(std::function<void()> release);
void shutdownGraphics();

} // namespace rmtn
