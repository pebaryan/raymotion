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
// Frames reach the sink in push order as w*h*4 RGBA bytes, rows bottom-up (GL order).
class FrameStream {
public:
    using Sink = std::function<bool(const unsigned char* rgba, size_t bytes)>;   // false: stop
    ~FrameStream();
    // depth = frames in flight on the GPU. Needs a current GL 3.3 context.
    bool open(int w, int h, Sink sink, int depth = 3);
    // Queues a readback of `rt` (w x h). False once the sink has failed.
    bool push(const RenderTexture& rt);
    // Drains every queued frame and stops the writer. True if all frames were written.
    bool close();
private:
    void collect();                   // oldest PBO -> CPU buffer -> writer queue
    void writer();
    int w_ = 0, h_ = 0;
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
