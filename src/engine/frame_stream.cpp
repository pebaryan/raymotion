// frame_stream.cpp -- asynchronous GPU readback (pixel-pack buffers) and a writer thread.
#include "rmtn/frame_stream.hpp"
#include <rlgl.h>
#include <cstring>

#if defined(_WIN32) && !defined(_WIN64)
#define RMTN_GLAPI __stdcall
#else
#define RMTN_GLAPI
#endif

namespace rmtn {
namespace {

// raylib keeps its GL loader private; the few entry points needed here are resolved once.
struct Gl {
    void (RMTN_GLAPI* genBuffers)(int, unsigned int*) = nullptr;
    void (RMTN_GLAPI* deleteBuffers)(int, const unsigned int*) = nullptr;
    void (RMTN_GLAPI* bindBuffer)(unsigned int, unsigned int) = nullptr;
    void (RMTN_GLAPI* bufferData)(unsigned int, ptrdiff_t, const void*, unsigned int) = nullptr;
    void* (RMTN_GLAPI* mapBufferRange)(unsigned int, ptrdiff_t, ptrdiff_t, unsigned int) = nullptr;
    unsigned char (RMTN_GLAPI* unmapBuffer)(unsigned int) = nullptr;
    void (RMTN_GLAPI* readPixels)(int, int, int, int, unsigned int, unsigned int, void*) = nullptr;
    void (RMTN_GLAPI* pixelStorei)(unsigned int, int) = nullptr;
    bool ok() const { return genBuffers && deleteBuffers && bindBuffer && bufferData && mapBufferRange && unmapBuffer && readPixels && pixelStorei; }
};
const Gl& gl() {
    static Gl g = [] {
        Gl r;
        r.genBuffers = (decltype(r.genBuffers))rlGetProcAddress("glGenBuffers");
        r.deleteBuffers = (decltype(r.deleteBuffers))rlGetProcAddress("glDeleteBuffers");
        r.bindBuffer = (decltype(r.bindBuffer))rlGetProcAddress("glBindBuffer");
        r.bufferData = (decltype(r.bufferData))rlGetProcAddress("glBufferData");
        r.mapBufferRange = (decltype(r.mapBufferRange))rlGetProcAddress("glMapBufferRange");
        r.unmapBuffer = (decltype(r.unmapBuffer))rlGetProcAddress("glUnmapBuffer");
        r.readPixels = (decltype(r.readPixels))rlGetProcAddress("glReadPixels");
        r.pixelStorei = (decltype(r.pixelStorei))rlGetProcAddress("glPixelStorei");
        return r;
    }();
    return g;
}

const unsigned int GL_PIXEL_PACK_BUFFER = 0x88EB, GL_STREAM_READ = 0x88E1, GL_MAP_READ_BIT = 0x0001,
                   GL_RGBA = 0x1908, GL_RED = 0x1903, GL_UNSIGNED_BYTE = 0x1401, GL_PACK_ALIGNMENT = 0x0D05;

// RGB -> planar yuv420p (BT.709, limited range: Y 16..235, Cb/Cr 16..240), drawn into a one-channel target w wide and h*3/2 tall whose
// rows, read bottom row first, are the stream: h rows of Y, then the U plane, then the V
// plane, each chroma row half a target row. The source is a render texture, so its
// texel row 0 is the picture's bottom row.
const char* const YUV_FS = R"(#version 330
uniform sampler2D texture0;
uniform ivec2 size;
out vec4 finalColor;
vec3 px(int x, int row) { return texelFetch(texture0, ivec2(x, size.y - 1 - row), 0).rgb; }
void main() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    float v;
    if (p.y < size.y) {
        v = 16.0 + dot(px(p.x, p.y), vec3(46.559, 156.629, 15.812));
    } else {
        int cw = size.x / 2, plane = cw * (size.y / 2);
        int i = (p.y - size.y) * size.x + p.x;
        bool isV = i >= plane;
        if (isV) i -= plane;
        int x = 2 * (i % cw), row = 2 * (i / cw);
        vec3 c = (px(x, row) + px(x + 1, row) + px(x, row + 1) + px(x + 1, row + 1)) * 0.25;
        v = 128.0 + (isV ? dot(c, vec3(112.0, -101.730, -10.270)) : dot(c, vec3(-25.664, -86.336, 112.0)));
    }
    finalColor = vec4(v / 255.0, 0.0, 0.0, 1.0);
}
)";

} // namespace

FrameStream::~FrameStream() { close(); }

void FrameStream::releaseYuv() {
    if (yuvFbo_) rlUnloadFramebuffer(yuvFbo_);
    if (yuvTex_) rlUnloadTexture(yuvTex_);
    if (toYuv_.id) UnloadShader(toYuv_);
    yuvFbo_ = yuvTex_ = 0;
    toYuv_ = Shader{};
}

bool FrameStream::open(int w, int h, Sink sink, int depth, Pixels pixels) {
    close();
    if (!gl().ok() || w <= 0 || h <= 0 || depth < 1) return false;
    if (pixels == Pixels::Yuv420) {
        if (w % 2 || h % 2) return false;
        toYuv_ = LoadShaderFromMemory(nullptr, YUV_FS);
        const int sizeLoc = toYuv_.id ? GetShaderLocation(toYuv_, "size") : -1;
        yuvTex_ = sizeLoc >= 0 ? rlLoadTexture(nullptr, w, h * 3 / 2, RL_PIXELFORMAT_UNCOMPRESSED_GRAYSCALE, 1) : 0;
        yuvFbo_ = yuvTex_ ? rlLoadFramebuffer() : 0;
        if (yuvFbo_) {
            rlFramebufferAttach(yuvFbo_, yuvTex_, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
            if (!rlFramebufferComplete(yuvFbo_)) { rlUnloadFramebuffer(yuvFbo_); yuvFbo_ = 0; }
        }
        if (!yuvFbo_) { releaseYuv(); return false; }
        const int size[2] = { w, h };
        SetShaderValue(toYuv_, sizeLoc, size, SHADER_UNIFORM_IVEC2);
    }
    pixels_ = pixels;
    w_ = w; h_ = h; bytes_ = pixels == Pixels::Yuv420 ? (size_t)w * h * 3 / 2 : (size_t)w * h * 4;
    sink_ = std::move(sink);
    pbo_.assign((size_t)depth, 0);
    gl().genBuffers(depth, pbo_.data());
    for (unsigned int b : pbo_) {
        gl().bindBuffer(GL_PIXEL_PACK_BUFFER, b);
        gl().bufferData(GL_PIXEL_PACK_BUFFER, (ptrdiff_t)bytes_, nullptr, GL_STREAM_READ);
    }
    gl().bindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    // Two spare CPU buffers let the writer hold one frame while the next is copied.
    free_.assign((size_t)depth + 2, std::vector<unsigned char>(bytes_));
    queue_.clear();
    next_ = inFlight_ = 0;
    done_ = failed_ = false;
    open_ = true;
    thread_ = std::thread([this] { writer(); });
    return true;
}

bool FrameStream::push(const RenderTexture& rt) {
    if (!open_) return false;
    if (inFlight_ == pbo_.size()) collect();           // the slot about to be reused
    if (pixels_ == Pixels::Yuv420) {
        RenderTexture target{};
        target.id = yuvFbo_;
        target.texture.id = yuvTex_;
        target.texture.width = w_; target.texture.height = h_ * 3 / 2;
        target.texture.mipmaps = 1; target.texture.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
        BeginTextureMode(target);
        rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);     // a copy, not a blend
        BeginBlendMode(BLEND_CUSTOM);
        BeginShaderMode(toYuv_);
        // The quad only has to cover the target and bind the source; the shader reads texels by position.
        DrawTexturePro(rt.texture, { 0, 0, (float)rt.texture.width, (float)rt.texture.height },
                       { 0, 0, (float)w_, (float)(h_ * 3 / 2) }, { 0, 0 }, 0, WHITE);
        EndShaderMode();
        EndBlendMode();
        EndTextureMode();
    }
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(pixels_ == Pixels::Yuv420 ? yuvFbo_ : rt.id);
    gl().bindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[next_]);
    if (pixels_ == Pixels::Yuv420) {
        gl().pixelStorei(GL_PACK_ALIGNMENT, 1);              // rows of w bytes: w is even, not always a multiple of 4
        gl().readPixels(0, 0, w_, h_ * 3 / 2, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        gl().pixelStorei(GL_PACK_ALIGNMENT, 4);
    } else gl().readPixels(0, 0, w_, h_, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);   // queued, returns at once
    gl().bindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    rlDisableFramebuffer();
    next_ = (next_ + 1) % pbo_.size();
    inFlight_++;
    std::lock_guard<std::mutex> lock(m_);
    return !failed_;
}

void FrameStream::collect() {
    std::vector<unsigned char> buf;
    {
        std::unique_lock<std::mutex> lock(m_);
        cv_.wait(lock, [this] { return !free_.empty() || failed_; });
        if (!free_.empty()) { buf = std::move(free_.back()); free_.pop_back(); }
    }
    const size_t oldest = (next_ + pbo_.size() - inFlight_) % pbo_.size();
    inFlight_--;
    if (buf.empty()) return;                           // the sink failed; drop the frame
    gl().bindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[oldest]);
    if (void* src = gl().mapBufferRange(GL_PIXEL_PACK_BUFFER, 0, (ptrdiff_t)bytes_, GL_MAP_READ_BIT)) {
        std::memcpy(buf.data(), src, bytes_);
        gl().unmapBuffer(GL_PIXEL_PACK_BUFFER);
    } else {
        std::lock_guard<std::mutex> lock(m_);
        failed_ = true;
    }
    gl().bindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    std::lock_guard<std::mutex> lock(m_);
    if (failed_) free_.push_back(std::move(buf));
    else queue_.push_back(std::move(buf));
    cv_.notify_all();
}

void FrameStream::writer() {
    for (;;) {
        std::vector<unsigned char> buf;
        {
            std::unique_lock<std::mutex> lock(m_);
            cv_.wait(lock, [this] { return !queue_.empty() || done_; });
            if (queue_.empty()) return;                // done and drained
            buf = std::move(queue_.front());
            queue_.pop_front();
        }
        const bool ok = sink_(buf.data(), bytes_);
        std::lock_guard<std::mutex> lock(m_);
        if (!ok) { failed_ = true; queue_.clear(); }
        free_.push_back(std::move(buf));
        cv_.notify_all();
        if (failed_) return;
    }
}

bool FrameStream::close() {
    if (!open_) return true;
    while (inFlight_ > 0) collect();
    {
        std::lock_guard<std::mutex> lock(m_);
        done_ = true;
    }
    cv_.notify_all();
    thread_.join();
    gl().deleteBuffers((int)pbo_.size(), pbo_.data());
    pbo_.clear();
    free_.clear();
    queue_.clear();
    releaseYuv();
    open_ = false;
    return !failed_;
}

} // namespace rmtn
