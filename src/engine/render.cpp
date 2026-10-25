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

// ---- motion blur: samples are summed into a float target, so still pixels come out unchanged
double sampleOffset = 0;                // frames: subframe() while a blur is being sampled
std::vector<RenderTexture> floatPool;
bool floatPoolRegistered = false;

RenderTexture acquireFloat(int w, int h) {
    for (size_t i = 0; i < floatPool.size(); i++) {
        if (floatPool[i].texture.width == w && floatPool[i].texture.height == h) {
            RenderTexture rt = floatPool[i];
            floatPool.erase(floatPool.begin() + (long)i);
            return rt;
        }
    }
    if (!floatPoolRegistered) {
        floatPoolRegistered = true;
        onGraphicsShutdown([] {
            for (auto& rt : floatPool) UnloadRenderTexture(rt);
            floatPool.clear();
            floatPoolRegistered = false;
        });
    }
    RenderTexture rt{};
    rt.id = rlLoadFramebuffer();
    rlEnableFramebuffer(rt.id);
    rt.texture.id = rlLoadTexture(nullptr, w, h, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1);
    rt.texture.width = w;
    rt.texture.height = h;
    rt.texture.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;
    rt.texture.mipmaps = 1;
    rlFramebufferAttach(rt.id, rt.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferComplete(rt.id);
    rlDisableFramebuffer();
    return rt;
}
void releaseFloat(RenderTexture rt) { if (rt.id) floatPool.push_back(rt); }

// The moments a blur samples (frames from the frame itself) and their weights: out of 255
// in all, so the weights sum to exactly one.
double blurOffset(const MotionBlur& b, int k) { return b.shutter * ((k + 0.5) / b.samples - 0.5); }
unsigned char blurWeight(const MotionBlur& b, int k) {
    return (unsigned char)(std::lround(255.0 * (k + 1) / b.samples) - std::lround(255.0 * k / b.samples));
}

// dst += src * weight / 255 (premultiplied, all channels)
void accumulate(const RenderTexture& src, const RenderTexture& dst, unsigned char weight) {
    BeginTextureMode(dst);
    rlSetBlendFactors(RL_ONE, RL_ONE, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM);
    const float w = (float)src.texture.width, h = (float)src.texture.height;
    DrawTexturePro(src.texture, { 0, 0, w, -h }, { 0, 0, w, h }, { 0, 0 }, 0, Color{ weight, weight, weight, weight });
    EndBlendMode();
    EndTextureMode();
}
// dst = src, unblended
void copyTarget(const RenderTexture& src, const RenderTexture& dst) {
    BeginTextureMode(dst);
    rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM);
    const float w = (float)src.texture.width, h = (float)src.texture.height;
    DrawTexturePro(src.texture, { 0, 0, w, -h }, { 0, 0, w, h }, { 0, 0 }, 0, WHITE);
    EndBlendMode();
    EndTextureMode();
}
void clearTarget(const RenderTexture& rt, Color c) {
    BeginTextureMode(rt);
    ClearBackground(c);
    EndTextureMode();
}

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

namespace {
// ---- winding fills (vector paths)
// The coverage pass adds +1 (or -1) per triangle to the red channel of a target cleared to
// 128, so each pixel ends up holding 128 + its winding number; the shader then keeps the
// pixels the fill rule says are inside.
const char* WINDING_FS = "#version 330\n"
"in vec2 fragTexCoord; in vec4 fragColor;\n"
"uniform sampler2D texture0; uniform int evenOdd;\n"
"out vec4 finalColor;\n"
"void main(){\n"
"    int w = int(floor(texture(texture0, fragTexCoord).r * 255.0 + 0.5)) - 128;\n"
"    bool inside = evenOdd == 1 ? (abs(w) % 2) == 1 : w != 0;\n"
"    finalColor = inside ? fragColor : vec4(0.0);\n"
"}\n";
Shader windingShader{};
int evenOddLoc = -1;

void drawTriangles(const std::vector<Vector2>& tris, Color col) {
    for (size_t i = 0; i + 2 < tris.size(); i += 3) DrawTriangle(tris[i], tris[i + 1], tris[i + 2], col);
}

} // namespace

bool fillWinding(const std::vector<Vector2>& add, const std::vector<Vector2>& subtract, Color color, bool evenOdd) {
    if (targets.empty()) return false;
    if (color.a == 0 || (add.empty() && subtract.empty())) return true;
    if (!windingShader.id) {
        windingShader = LoadShaderFromMemory(nullptr, WINDING_FS);
        evenOddLoc = GetShaderLocation(windingShader, "evenOdd");
        onGraphicsShutdown([] { UnloadShader(windingShader); windingShader = Shader{}; });
    }
    const Target parent = targets.back();
    rlDrawRenderBatchActive();
    const int w = parent.rt.texture.width, h = parent.rt.texture.height;
    RenderTexture counts = acquire(w, h);
    bindTarget({ counts, parent.scale });
    ClearBackground(Color{ 128, 0, 0, 0 });
    rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ONE, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    drawTriangles(add, Color{ 1, 0, 0, 0 });
    rlDrawRenderBatchActive();
    if (!subtract.empty()) {
        rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ONE, RL_ONE, RL_FUNC_REVERSE_SUBTRACT, RL_FUNC_REVERSE_SUBTRACT);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        drawTriangles(subtract, Color{ 1, 0, 0, 0 });
        rlDrawRenderBatchActive();
    }
    bindTarget(parent);                                // standard blend: colour over the content
    BeginShaderMode(windingShader);
    int eo = evenOdd ? 1 : 0;
    SetShaderValue(windingShader, evenOddLoc, &eo, SHADER_UNIFORM_INT);
    DrawTexturePro(counts.texture, { 0, 0, (float)w, (float)-h }, { 0, 0, w / parent.scale, h / parent.scale },
                   { 0, 0 }, 0, color);
    EndShaderMode();
    rlDrawRenderBatchActive();
    release(counts);
    return true;
}

void restoreBlend() {
    if (!targets.empty()) standardBlend();
}

bool drawLayerBlurred(TNode& n, int lt, int t, const Ctx& parentCtx) {
    if (targets.empty()) return false;
    const Target parent = targets.back();
    const MotionBlur blur = n.layer->motionBlur;
    const int w = parent.rt.texture.width, h = parent.rt.texture.height;
    rlDrawRenderBatchActive();                         // see drawLayer
    RenderTexture sum = acquireFloat(w, h), sample = acquire(w, h), content = acquire(w, h);
    RenderTexture mask = n.layer->mask ? acquire(w, h) : RenderTexture{};
    clearTarget(sum, BLANK);
    const double base = sampleOffset;
    for (int k = 0; k < blur.samples; k++) {
        // the node moves too: its props are evaluated at each moment
        sampleOffset = base + blurOffset(blur, k);
        if (n.up) n.up(lt, n.props);
        if (n.layerUp) n.layerUp(t, n.props, *n.layer);
        Ctx inner = nodeCtx(parentCtx, n.props);
        inner.a = 1.f;
        targets.push_back({ sample, parent.scale });
        bindTarget(targets.back());
        ClearBackground(BLANK);
        drawNodeContent(n, lt, inner);
        rlDrawRenderBatchActive();
        targets.pop_back();
        accumulate(sample, sum, blurWeight(blur, k));
    }
    sampleOffset = base;
    if (n.up) n.up(lt, n.props);                       // opacity, mask and offset from the frame itself
    if (n.layerUp) n.layerUp(t, n.props, *n.layer);
    const Ctx c = nodeCtx(parentCtx, n.props);
    Ctx inner = c;
    inner.a = 1.f;
    copyTarget(sum, content);
    releaseFloat(sum);
    release(sample);
    if (mask.id) {
        targets.push_back({ mask, parent.scale });
        bindTarget(targets.back());
        ClearBackground(BLANK);
        n.layer->mask(inner, lt);
        restoreBlend();
        rlDrawRenderBatchActive();
        targets.pop_back();
    }
    if (!n.layer->filters.empty()) {
        for (const Filter& f : n.layer->filters) {
            RenderTexture out = acquire(w, h);
            applyFilter(f, content, out, parent.scale);
            release(content);
            content = out;
        }
    }
    bindTarget(parent);
    const float alpha = c.a * n.layer->opacity;
    if (alpha > 0.f) composite(*n.layer, content, mask, alpha, parent.scale);
    release(content);
    release(mask);
    return true;
}

double subframe() { return sampleOffset; }

int frameRate() { return currentFps; }
float canvasScale() { return targets.empty() ? 1.f : targets.back().scale; }
Vector2 canvasSize() {
    if (targets.empty()) return { 0, 0 };
    const Target& t = targets.back();
    return { t.rt.texture.width / t.scale, t.rt.texture.height / t.scale };
}

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
    if (c.motionBlur.samples > 1) {
        // the whole frame, drawn at each moment and averaged
        const RenderTexture out = canvas.drawTarget();
        const int w = out.texture.width, h = out.texture.height;
        RenderTexture sum = acquireFloat(w, h), sample = acquire(w, h);
        clearTarget(sum, BLANK);
        const double base = sampleOffset;
        for (int k = 0; k < c.motionBlur.samples; k++) {
            sampleOffset = base + blurOffset(c.motionBlur, k);
            targets.push_back({ sample, (float)canvas.scale() });
            bindTarget(targets.back());
            ClearBackground(c.background);
            drawTree(c.roots, frame);
            EndTextureMode();
            targets.clear();
            EndBlendMode();
            accumulate(sample, sum, blurWeight(c.motionBlur, k));
        }
        sampleOffset = base;
        copyTarget(sum, out);
        releaseFloat(sum);
        release(sample);
        rlEnableBackfaceCulling();
        canvas.resolve();
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
