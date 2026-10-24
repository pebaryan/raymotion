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
    bool ok() const { return genBuffers && deleteBuffers && bindBuffer && bufferData && mapBufferRange && unmapBuffer && readPixels; }
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
        return r;
    }();
    return g;
}

const unsigned int GL_PIXEL_PACK_BUFFER = 0x88EB, GL_STREAM_READ = 0x88E1, GL_MAP_READ_BIT = 0x0001,
                   GL_RGBA = 0x1908, GL_UNSIGNED_BYTE = 0x1401;

} // namespace

FrameStream::~FrameStream() { close(); }

bool FrameStream::open(int w, int h, Sink sink, int depth) {
    close();
    if (!gl().ok() || w <= 0 || h <= 0 || depth < 1) return false;
    w_ = w; h_ = h; bytes_ = (size_t)w * h * 4;
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
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(rt.id);
    gl().bindBuffer(GL_PIXEL_PACK_BUFFER, pbo_[next_]);
    gl().readPixels(0, 0, w_, h_, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);   // queued, returns at once
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
    open_ = false;
    return !failed_;
}

} // namespace rmtn
