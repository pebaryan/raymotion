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

// A canvas cleared to a transparent background (Comp::background with alpha 0) holds
// premultiplied colour. Video codecs and PNG want straight alpha: this converts `pixels`
// RGBA8 pixels in place (fully transparent pixels become 0, 0, 0, 0).
void unpremultiply(unsigned char* rgba, size_t pixels);

// Renders `body` into a transparent offscreen target the size of the current canvas
// target (colour and depth cleared, 2D state bound with the supersample scale), then
// composites it over the current target with `alpha` (premultiplied "over"). `body` may
// bind other framebuffers but must rebind `target` before drawing into it again. Returns
// false without calling `body` when no canvas target is active (outside presentFrame).
bool drawOffscreen(float alpha, const std::function<void(const RenderTexture& target, float scale)>& body);

// Fills by winding number (vector paths): every triangle of `add` counts +1 and every one
// of `subtract` -1 at the pixels it covers (3 points per triangle, composition coordinates).
// Pixels whose count is odd (`evenOdd`) or non-zero get `color`, drawn over the current
// target with its alpha. A fan of each closed contour, split by orientation, fills the
// contours exactly by SVG's rules. Returns false without drawing when no canvas is active.
bool fillWinding(const std::vector<Vector2>& add, const std::vector<Vector2>& subtract, Color color, bool evenOdd);

// Runs one layer filter over `src` (premultiplied, raylib RT orientation) into `dst` of the
// same size; `scale` is target px per composition px. drawLayer uses it for Layer::filters.
void applyFilter(const Filter& f, const RenderTexture& src, const RenderTexture& dst, float scale);

// While presentFrame runs: the frame rate of the composition being drawn and the scale
// from composition units to pixels of the current target (the supersample factor).
// Outside it: 30 and 1.
int frameRate();
float canvasScale();
// While presentFrame runs: the size of the current target in composition units (the
// composition's size); outside it, {0, 0}.
Vector2 canvasSize();

// GL-owning code registers a release callback; shutdownGraphics() runs them all (then
// unloads cached fonts) and must be called before CloseWindow.
void onGraphicsShutdown(std::function<void()> release);
void shutdownGraphics();

} // namespace rmtn
