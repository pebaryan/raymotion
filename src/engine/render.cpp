// render.cpp -- supersampled canvas, frame presentation, graphics shutdown hooks.
#include "rmtn/render.hpp"
#include "rmtn/fonts.hpp"
#include <rlgl.h>

namespace rmtn {

bool Canvas::load(int w, int h, int supersample) {
    unload();
    w_ = w; h_ = h;
    ss_ = supersample >= 4 ? 4 : supersample >= 2 ? 2 : 1;
    for (int s = ss_; s >= 1; s /= 2) {
        RenderTexture rt = LoadRenderTexture(w * s, h * s);
        if (!IsRenderTextureValid(rt)) { unload(); return false; }
        // Exact 2:1 bilinear sampling lands between four texels: a 2x2 box filter.
        if (s > 1) SetTextureFilter(rt.texture, TEXTURE_FILTER_BILINEAR);
        levels_.push_back(rt);
    }
    return true;
}

void Canvas::unload() {
    for (auto& rt : levels_) UnloadRenderTexture(rt);
    levels_.clear();
}

void Canvas::resolve() {
    // Straight copy (ONE, ZERO) so colour and alpha pass through unblended.
    rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
    for (size_t i = 1; i < levels_.size(); i++) {
        const RenderTexture& src = levels_[i - 1];
        const RenderTexture& dst = levels_[i];
        BeginTextureMode(dst);
        BeginBlendMode(BLEND_CUSTOM);
        DrawTexturePro(src.texture, { 0, 0, (float)src.texture.width, -(float)src.texture.height },
                       { 0, 0, (float)dst.texture.width, (float)dst.texture.height }, { 0, 0 }, 0, WHITE);
        EndBlendMode();
        EndTextureMode();
    }
}

void presentFrame(Comp& c, int frame, Canvas& canvas) {
    if (c.render) {
        BeginTextureMode(canvas.output());
        rlDisableBackfaceCulling();   // hooks start in 2D state; 3D code enables what it needs
        c.render(c, frame, canvas.output());
        EndTextureMode();
        rlEnableBackfaceCulling();
        return;
    }
    BeginTextureMode(canvas.drawTarget());
    ClearBackground(c.background);
    // 2D has no back faces: without this, triangles wound the "wrong" way silently vanish.
    // GL state applies when the batch flushes, i.e. at EndTextureMode.
    rlDisableBackfaceCulling();
    rlPushMatrix();
    rlScalef((float)canvas.scale(), (float)canvas.scale(), 1.f);   // keep composition coordinates
    drawTree(c.roots, frame);
    rlPopMatrix();
    EndTextureMode();
    rlEnableBackfaceCulling();
    canvas.resolve();
}

namespace {
std::vector<std::function<void()>>& releasers() {
    static std::vector<std::function<void()>> list;
    return list;
}
}

void onGraphicsShutdown(std::function<void()> release) { releasers().push_back(std::move(release)); }

void shutdownGraphics() {
    auto& list = releasers();
    for (auto it = list.rbegin(); it != list.rend(); ++it) (*it)();
    list.clear();
    fonts::releaseAll();
}

} // namespace rmtn
