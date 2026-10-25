// render.cpp -- supersampled canvas, frame presentation, layers, graphics shutdown hooks.
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

namespace {

std::vector<std::function<void()>>& releasers() {
    static std::vector<std::function<void()>> list;
    return list;
}

// ---- render-target stack for the timeline tree and its layers
struct Target { RenderTexture rt; float scale; };
std::vector<Target> targets;            // innermost last; empty outside presentFrame's tree pass
std::vector<RenderTexture> pool;        // free offscreen layer targets
bool poolRegistered = false;
int currentFps = 30;

// "Over" for straight-alpha sources that keeps destination alpha correct: colour blends
// as usual, alpha accumulates as a + d(1-a). Opaque canvases stay opaque, and layer
// targets (cleared to transparent) end up holding premultiplied colour.
void standardBlend() {
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA,
                              RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
}

void bindTarget(const Target& t) {
    BeginTextureMode(t.rt);                           // flushes, resets projection + modelview
    rlDisableBackfaceCulling();                       // 2D: both windings are front faces
    rlScalef(t.scale, t.scale, 1.f);                  // draw in composition coordinates
    standardBlend();
}

RenderTexture acquire(int w, int h) {
    for (size_t i = 0; i < pool.size(); i++) {
        if (pool[i].texture.width == w && pool[i].texture.height == h) {
            RenderTexture rt = pool[i];
            pool.erase(pool.begin() + (long)i);
            return rt;
        }
    }
    if (!poolRegistered) {
        poolRegistered = true;
        onGraphicsShutdown([] {
            for (auto& rt : pool) UnloadRenderTexture(rt);
            pool.clear();
            poolRegistered = false;
        });
    }
    return LoadRenderTexture(w, h);
}
void release(RenderTexture rt) { if (rt.id) pool.push_back(rt); }

// ---- layer compositing
const char* MASK_FS = "#version 330\n"
"in vec2 fragTexCoord; in vec4 fragColor;\n"
"uniform sampler2D texture0; uniform sampler2D maskTex; uniform vec4 colDiffuse;\n"
"uniform int maskMode; uniform int invertMask;\n"
"out vec4 finalColor;\n"
"void main(){\n"
"    vec4 c = texture(texture0, fragTexCoord) * colDiffuse * fragColor;\n"
"    vec4 m = texture(maskTex, fragTexCoord);\n"
"    float k = maskMode == 1 ? dot(m.rgb, vec3(0.2126, 0.7152, 0.0722)) : m.a;\n"
"    if (invertMask == 1) k = 1.0 - k;\n"
"    finalColor = c * k;\n"                            // premultiplied: scale all channels
"}\n";
Shader maskShader{};
int maskTexLoc = -1, maskModeLoc = -1, invertLoc = -1;

void ensureMaskShader() {
    if (maskShader.id) return;
    maskShader = LoadShaderFromMemory(nullptr, MASK_FS);
    maskTexLoc = GetShaderLocation(maskShader, "maskTex");
    maskModeLoc = GetShaderLocation(maskShader, "maskMode");
    invertLoc = GetShaderLocation(maskShader, "invertMask");
    onGraphicsShutdown([] { UnloadShader(maskShader); maskShader = Shader{}; });
}

// Premultiplied source over the destination with the layer's blend mode.
void compositeBlend(Blend b) {
    int srcRGB = RL_ONE, dstRGB = RL_ONE_MINUS_SRC_ALPHA;
    if (b == Blend::Add) dstRGB = RL_ONE;
    else if (b == Blend::Multiply) srcRGB = RL_DST_COLOR;
    else if (b == Blend::Screen) dstRGB = RL_ONE_MINUS_SRC_COLOR;
    rlSetBlendFactorsSeparate(srcRGB, dstRGB, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
}

void composite(const Layer& layer, RenderTexture content, RenderTexture mask, float alpha, float scale) {
    const float w = (float)content.texture.width, h = (float)content.texture.height;
    const unsigned char a = (unsigned char)(alpha * 255.f + 0.5f);
    // The target's modelview holds the supersample scale (rlPushMatrix/rlLoadIdentity would
    // not clear it: rlgl applies them to a separate CPU transform), so the target-sized
    // texture is drawn in composition units.
    compositeBlend(layer.blend);
    if (mask.id) {
        ensureMaskShader();
        BeginShaderMode(maskShader);
        int mode = layer.maskMode == MaskMode::Luma ? 1 : 0, inv = layer.invertMask ? 1 : 0;
        SetShaderValueTexture(maskShader, maskTexLoc, mask.texture);
        SetShaderValue(maskShader, maskModeLoc, &mode, SHADER_UNIFORM_INT);
        SetShaderValue(maskShader, invertLoc, &inv, SHADER_UNIFORM_INT);
    }
    DrawTexturePro(content.texture, { 0, 0, w, -h }, { layer.offset.x, layer.offset.y, w / scale, h / scale },
                   { 0, 0 }, 0, Color{ a, a, a, a });
    if (mask.id) EndShaderMode();
    rlDrawRenderBatchActive();                        // draw with this blend before it changes
    standardBlend();
}

} // namespace

bool drawLayer(TNode& n, int t, const Ctx& c) {
    if (targets.empty()) return false;
    const Target parent = targets.back();
    const float alpha = c.a * n.layer->opacity;
    if (alpha <= 0.f) return true;                     // fully transparent: nothing to draw
    const int w = parent.rt.texture.width, h = parent.rt.texture.height;
    Ctx inner = c;
    inner.a = 1.f;                                     // opacity is applied once, to the group

    // Creating a render texture rebinds the framebuffer: flush what the parent has batched
    // and acquire everything before drawing (bindTarget(parent) below restores it).
    rlDrawRenderBatchActive();
    RenderTexture content = acquire(w, h), mask = n.layer->mask ? acquire(w, h) : RenderTexture{};
    targets.push_back({ content, parent.scale });
    bindTarget(targets.back());
    ClearBackground(BLANK);
    drawNodeContent(n, t, inner);
    if (mask.id) {
        targets.back() = { mask, parent.scale };
        bindTarget(targets.back());
        ClearBackground(BLANK);
        n.layer->mask(inner, t);
        restoreBlend();
    }
    targets.pop_back();
    if (!n.layer->filters.empty()) {
        rlDrawRenderBatchActive();                     // finish the content before acquiring (see above)
        for (const Filter& f : n.layer->filters) {
            RenderTexture out = acquire(w, h);
            applyFilter(f, content, out, parent.scale);
            release(content);
            content = out;
        }
    }
    bindTarget(parent);                                // flushes the layer passes
    composite(*n.layer, content, mask, alpha, parent.scale);
    release(content);
    release(mask);
    return true;
}

bool drawOffscreen(float alpha, const std::function<void(const RenderTexture&, float)>& body) {
    if (targets.empty()) return false;
    const Target parent = targets.back();
    if (alpha <= 0.f) return true;
    rlDrawRenderBatchActive();                         // see drawLayer
    RenderTexture content = acquire(parent.rt.texture.width, parent.rt.texture.height);
    targets.push_back({ content, parent.scale });
    bindTarget(targets.back());
    ClearBackground(BLANK);
    body(content, parent.scale);
    rlDrawRenderBatchActive();
    targets.pop_back();
    bindTarget(parent);
    composite(Layer{}, content, RenderTexture{}, alpha, parent.scale);
    release(content);
    return true;
}

void restoreBlend() {
    if (!targets.empty()) standardBlend();
}

int frameRate() { return currentFps; }
float canvasScale() { return targets.empty() ? 1.f : targets.back().scale; }

void presentFrame(Comp& c, int frame, Canvas& canvas) {
    currentFps = c.fps;
    if (c.render) {
        BeginTextureMode(canvas.output());
        rlDisableBackfaceCulling();   // hooks start in 2D state; 3D code enables what it needs
        c.render(c, frame, canvas.output());
        EndTextureMode();
        rlEnableBackfaceCulling();
        return;
    }
    targets.push_back({ canvas.drawTarget(), (float)canvas.scale() });
    bindTarget(targets.back());
    ClearBackground(c.background);
    drawTree(c.roots, frame);
    EndTextureMode();                                  // flush with the tree's GL state
    targets.clear();
    EndBlendMode();
    rlEnableBackfaceCulling();
    canvas.resolve();
}

void onGraphicsShutdown(std::function<void()> release) { releasers().push_back(std::move(release)); }

void shutdownGraphics() {
    auto& list = releasers();
    // Releasers may register more (none should); run a snapshot, newest first.
    auto snapshot = std::move(list);
    list.clear();
    for (auto it = snapshot.rbegin(); it != snapshot.rend(); ++it) (*it)();
    fonts::releaseAll();
}

} // namespace rmtn
