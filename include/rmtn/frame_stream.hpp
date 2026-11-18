// rmtn/frame_stream.hpp -- stream rendered frames off the GPU without stalling on them.
#pragma once
#include <raylib.h>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace rmtn {

// Reads frames back through a ring of pixel-pack buffers, so the copy of frame N
// overlaps drawing frames N+1.. instead of waiting for the GPU, and hands each frame
// to `sink` on a writer thread, so a slow consumer (an encoder pipe) overlaps drawing too.
// Frames reach the sink in push order. As Rgba they are w*h*4 bytes, rows bottom-up (GL
// order). As Yuv420 they are converted on the GPU first and arrive as planar yuv420p,
// w*h*3/2 bytes, rows top-down: BT.709 limited range, each chroma sample the mean of its
// 2x2 pixels, alpha ignored. That is the conversion the render command asks ffmpeg for
// when it is sent RGBA, done here so the encoder process is spared it and less than half
// the bytes cross the readback and the pipe. Yuv420 needs an even width and height.
class FrameStream {
public:
    using Sink = std::function<bool(const unsigned char* pixels, size_t bytes)>;   // false: stop
    enum class Pixels { Rgba, Yuv420 };
    ~FrameStream();
    // depth = frames in flight on the GPU. Needs a current GL 3.3 context.
    bool open(int w, int h, Sink sink, int depth = 3, Pixels pixels = Pixels::Rgba);
    // Queues a readback of `rt` (w x h). False once the sink has failed.
    bool push(const RenderTexture& rt);
    // Drains every queued frame and stops the writer. True if all frames were written.
    bool close();
private:
    void collect();                   // oldest PBO -> CPU buffer -> writer queue
    void writer();
    int w_ = 0, h_ = 0;
    Pixels pixels_ = Pixels::Rgba;
    Shader toYuv_{};                  // Yuv420: the conversion pass and its one-channel target
    unsigned int yuvFbo_ = 0, yuvTex_ = 0;
    void releaseYuv();
    size_t bytes_ = 0;
    Sink sink_;
    std::vector<unsigned int> pbo_;
    size_t next_ = 0, inFlight_ = 0;  // ring cursor and readbacks not yet collected
    std::mutex m_;
    std::condition_variable cv_;
    std::vector<std::vector<unsigned char>> free_;
    std::deque<std::vector<unsigned char>> queue_;
    bool done_ = false, failed_ = false, open_ = false;
    std::thread thread_;
};

} // namespace rmtn
