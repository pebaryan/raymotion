// rmtn/media.hpp -- images, SVG, animated GIFs, video and audio on the timeline
// (Remotion's <Img>, <Gif>, <OffthreadVideo>, <Audio> and staticFile).
//
//     auto logo = media::image("logo.svg");                 // resolved like staticFile()
//     node->fn = [logo](const Ctx& c, int t) { media::drawImage(c, *logo, { 0, 0, 400, 200 }); };
//
//     media::VideoOptions v;
//     v.startFrom = 2.0;                                     // seconds into the file
//     media::video(addRoot(c.roots, 30, 150), "clip.mp4", { 0, 0, 1920, 1080 }, v);
//     media::audio(addRoot(c.roots, 0, c.duration), "music.ogg").volume = 0.6f;
//
// Media is decoded where it is first drawn (or mixed), so compositions can be built before
// a window exists; a missing or unreadable file throws std::runtime_error from there.
// Video frames and non-native formats are decoded by ffmpeg (found as for rendering);
// PNG/JPG/GIF/BMP/TGA/PSD/HDR/QOI/DDS images, SVG and WAV/OGG/MP3/QOA audio need no ffmpeg.
#pragma once
#include <raylib.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "timeline.hpp"

namespace rmtn {
namespace media {

// ------------------------------------------------------------------ files
// Remotion's staticFile: `file` as given when it exists, else the first match in the
// search paths (latest first), $RAYMOTION_ASSETS, <exe dir>/assets, then the bundled
// assets. Throws std::runtime_error when there is none.
std::string resolve(const std::string& file);
void addSearchPath(const std::string& dir);

// The ffmpeg used for decoding and encoding ("" when there is none): `setFfmpeg`, else
// $RAYMOTION_FFMPEG, winget's Gyan.FFmpeg, common install dirs, then PATH.
std::string ffmpeg();
void setFfmpeg(const std::string& path);

// Decodes any supported image (straight alpha; HDR as 32-bit float RGBA, linear). SVGs
// are rasterised `svgWidth` pixels wide (0: their own size). Caller unloads.
Image loadImage(const std::string& file, int svgWidth = 0);

// ------------------------------------------------------------------ images
enum class Fit {
    Fill,       // stretch to the box
    Contain,    // fit inside the box, keeping the aspect ratio
    Cover,      // fill the box, keeping the aspect ratio, cropping the overflow
    None,       // natural size (1 image pixel = 1 composition pixel)
};
struct ImageStyle {
    Fit fit = Fit::Contain;
    Vector2 align{ 0.5f, 0.5f };    // where the image sits (Contain/None) or is cropped (Cover)
    Color tint = WHITE;             // multiplies the pixels; its alpha times the node opacity
};

// A still image, animated GIF or SVG. Textures are created on first draw, mipmapped and
// trilinear filtered; an SVG is re-rasterised for the size it is drawn at, so it stays
// sharp at any scale and supersampling level.
class ImageAsset {
public:
    explicit ImageAsset(std::string file);
    ~ImageAsset();
    ImageAsset(const ImageAsset&) = delete;
    ImageAsset& operator=(const ImageAsset&) = delete;
    const std::string& file() const { return file_; }
    int width();                    // natural size in pixels (an SVG's document size)
    int height();
    int frameCount();               // > 1 for animated GIFs
    double duration();              // seconds per GIF loop (0 for stills)
    bool isVector();
    // The texture for frame `frame` (GIFs; wraps) or, for SVG, one at least `pixelWidth` wide.
    Texture2D texture(int frame = 0, float pixelWidth = 0);
    int frameAt(double seconds);    // GIF frame shown `seconds` into the loop
    struct State;
private:
    std::string file_;
    std::unique_ptr<State> s_;
};
using ImagePtr = std::shared_ptr<ImageAsset>;
ImagePtr image(const std::string& file);          // shared per file

// Draws `img` in the box `dst` (node-local units) through the node ctx. `seconds` picks
// the frame of an animated GIF (it loops).
void drawImage(const Ctx& c, ImageAsset& img, Rectangle dst, const ImageStyle& style = {}, double seconds = 0);
// Draws the region `src` (texture pixels, top-down) of `tex` into the quad `dst`.
void drawTexture(const Ctx& c, Texture2D tex, Rectangle src, Rectangle dst, Color tint = WHITE);
// A node fn drawing `img`; a GIF plays from the node's local frame 0.
std::function<void(const Ctx&, int)> imageFn(ImagePtr img, Rectangle dst, ImageStyle style = {});

// ------------------------------------------------------------------ video
struct VideoInfo {
    int width = 0, height = 0;
    double fps = 0, duration = 0;   // frames per second, seconds
    bool hasAudio = false;
};
VideoInfo probe(const std::string& file);         // via ffprobe (beside ffmpeg); cached

// Frame-accurate decoding: a dedicated ffmpeg process streams frames, so playing forward
// costs one read per frame and seeking restarts it. Each decoder serves one playback.
class VideoDecoder {
public:
    explicit VideoDecoder(std::string file);
    ~VideoDecoder();
    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;
    const VideoInfo& info();
    // Texture of the frame shown at `seconds` into the file (clamped to its frames).
    Texture2D frameAt(double seconds);
    struct State;
private:
    std::string file_;
    std::unique_ptr<State> s_;
};

struct VideoOptions {
    double startFrom = 0;           // seconds into the file that the node's frame 0 shows
    double endAt = 0;               // seconds; the file stops (or loops) here (0: its end)
    double playbackRate = 1;
    bool loop = false;
    float volume = 1;               // soundtrack gain
    std::function<float(int t)> volumeAt;           // times `volume`, by node-local frame
    bool muted = false;
    ImageStyle style{ Fit::Cover };
};
// Makes `node` play `file` in the box `dst`, from the node's local frame 0, and adds its
// soundtrack to the node's audio (unless muted or silent). Sets node->fn.
void video(TNode* node, const std::string& file, Rectangle dst, VideoOptions options = {});

// ------------------------------------------------------------------ audio
// Plays `file` (or a video's soundtrack) from the node's local frame 0 while the node is
// active. Returns the clip for later changes.
struct AudioOptions {
    double startFrom = 0, endAt = 0;                // seconds into the file (endAt 0: its end)
    double playbackRate = 1;                        // resampled: pitch follows speed
    bool loop = false;
    float volume = 1;
    std::function<float(int t)> volumeAt;           // times `volume`, by node-local frame
};
AudioClip& audio(TNode* node, const std::string& file, AudioOptions options = {});

// Decoded audio: interleaved stereo float at `rate`. Cached per (file, rate).
const std::vector<float>& decodeAudio(const std::string& file, int rate);

// The composition's soundtrack: every audio clip in the tree at its absolute time (Loop
// repeats it, Freeze silences it, clips end with their node) plus the procedural song
// when `c.music` is set. Interleaved stereo float at `rate`, duration/fps seconds long.
std::vector<float> mixAudio(const Comp& c, int rate = 48000);
bool hasAudio(const Comp& c);
bool writeWav(const std::string& path, const std::vector<float>& stereo, int rate);   // 16-bit PCM

} // namespace media
} // namespace rmtn
