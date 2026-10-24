// media.cpp -- asset lookup, image/SVG/GIF decoding, ffmpeg video decoding, audio mixing.
#include "rmtn/media.hpp"
#include "rmtn/render.hpp"
#include <rlgl.h>
#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

// stb_image for JPG/PNG/GIF/BMP/TGA/PSD/HDR/PIC/PNM, private to this file (raylib links
// its own reduced copy).
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS
#include "stb/stb_image.h"

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvg/nanosvg.h"
#include "nanosvg/nanosvgrast.h"

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace fs = std::filesystem;

namespace rmtn {
namespace media {
namespace {

// ------------------------------------------------------------------ processes
std::string quote(const std::string& s) { return "\"" + s + "\""; }

// cmd.exe strips the first and last quote of a command line that holds several quoted
// tokens, so on Windows the whole line gets one more enclosing pair.
std::string shellLine(const std::string& cmd) {
#ifdef _WIN32
    return "\"" + cmd + "\"";
#else
    return cmd;
#endif
}

#ifdef _WIN32
const char* DEVNULL = "NUL";
#else
const char* DEVNULL = "/dev/null";
#endif

FILE* openRead(const std::string& cmd) {
    if (getenv("RAYMOTION_DEBUG_CMD")) fprintf(stderr, "[CMD] %s\n", cmd.c_str());
#ifdef _WIN32
    return popen(shellLine(cmd).c_str(), "rb");
#else
    return popen(shellLine(cmd).c_str(), "r");
#endif
}

// Runs `cmd` and returns its standard output.
std::string readProcess(const std::string& cmd) {
    FILE* p = openRead(cmd);
    if (!p) return "";
    std::string out;
    char buf[1 << 16];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
    pclose(p);
    return out;
}

std::string readFile(const std::string& path) {
    std::ifstream in(fs::u8path(path), std::ios::binary);
    if (!in) throw std::runtime_error("media: cannot read " + path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string lowerExt(const std::string& path) {
    std::string e = fs::u8path(path).extension().u8string();
    for (auto& ch : e) ch = (char)std::tolower((unsigned char)ch);
    return e;
}

// ------------------------------------------------------------------ ffmpeg
std::string forcedFfmpeg;
std::string foundFfmpeg;
bool searchedFfmpeg = false;

bool ffmpegWorks(const std::string& exe) {
    std::string probe = quote(exe) + " -version >" + DEVNULL + " 2>&1";
    return std::system(shellLine(probe).c_str()) == 0;
}

std::string searchFfmpeg() {
    std::vector<std::string> candidates;
    if (const char* env = getenv("RAYMOTION_FFMPEG")) candidates.push_back(env);
    // winget installs Gyan.FFmpeg under a versioned directory in the user's profile.
    if (const char* local = getenv("LOCALAPPDATA")) {
        std::error_code ec;
        fs::path packages = fs::u8path(local) / "Microsoft" / "WinGet" / "Packages";
        for (fs::directory_iterator it(packages, ec), end; !ec && it != end; it.increment(ec)) {
            if (it->path().filename().string().rfind("Gyan.FFmpeg", 0) != 0) continue;
            std::error_code ec2;
            for (fs::directory_iterator sub(it->path(), ec2), e2; !ec2 && sub != e2; sub.increment(ec2))
                candidates.push_back((sub->path() / "bin" / "ffmpeg.exe").u8string());
        }
    }
    candidates.push_back("C:/Program Files/ffmpeg/bin/ffmpeg.exe");
    candidates.push_back("C:/ffmpeg/bin/ffmpeg.exe");
    for (const auto& p : candidates) {
        std::error_code ec;
        if (p.empty() || !fs::exists(fs::u8path(p), ec)) continue;
        if (ffmpegWorks(p)) return p;
    }
    if (ffmpegWorks("ffmpeg")) return "ffmpeg";   // last resort: PATH
    return "";
}

std::string requireFfmpeg(const std::string& why) {
    std::string ff = ffmpeg();
    if (ff.empty()) throw std::runtime_error("media: ffmpeg is needed to " + why + " (use --ffmpeg PATH or RAYMOTION_FFMPEG)");
    return ff;
}

// ffprobe sits beside ffmpeg.
std::string ffprobeFor(const std::string& ff) {
    if (ff == "ffmpeg") return "ffprobe";
    fs::path p = fs::u8path(ff);
    std::string name = p.filename().u8string();
    size_t at = name.find("ffmpeg");
    if (at == std::string::npos) return "ffprobe";
    name.replace(at, 6, "ffprobe");
    return (p.parent_path() / fs::u8path(name)).u8string();
}

// ------------------------------------------------------------------ files
std::vector<std::string> searchDirs;                     // addSearchPath, oldest first

std::string envDir(const char* var) {
    const char* v = std::getenv(var);
    return (v && *v) ? std::string(v) : std::string();
}

// ------------------------------------------------------------------ GPU ownership
// Media objects own textures; shutdownGraphics releases them all, and they are recreated
// on the next draw (e.g. with a new window).
struct GpuOwner {
    virtual ~GpuOwner() = default;
    virtual void releaseGpu() = 0;
};
std::set<GpuOwner*>& owners() {
    // Never destroyed: media held by static caches unregisters during exit.
    static auto* list = new std::set<GpuOwner*>;
    return *list;
}
bool hookRegistered = false;
void ensureHook() {
    if (hookRegistered) return;
    hookRegistered = true;
    onGraphicsShutdown([] {
        for (GpuOwner* o : owners()) o->releaseGpu();
        hookRegistered = false;
    });
}

Image imageFromRGBA(unsigned char* px, int w, int h) {
    Image img{};
    img.data = px; img.width = w; img.height = h; img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return img;
}

Texture2D uploadMipmapped(const Image& img) {
    Texture2D tex = LoadTextureFromImage(img);
    if (!tex.id) throw std::runtime_error("media: cannot create a texture");
    GenTextureMipmaps(&tex);
    SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);
    return tex;
}

Image rasterizeSvg(NSVGimage* svg, int width) {
    if (svg->width <= 0 || svg->height <= 0) throw std::runtime_error("media: SVG has no size");
    int w = width > 0 ? width : (int)std::ceil(svg->width);
    float scale = (float)w / svg->width;
    int h = std::max(1, (int)std::lround(svg->height * scale));
    unsigned char* px = (unsigned char*)std::malloc((size_t)w * h * 4);
    NSVGrasterizer* r = nsvgCreateRasterizer();
    nsvgRasterize(r, svg, 0, 0, scale, px, w, h, w * 4);
    nsvgDeleteRasterizer(r);
    return imageFromRGBA(px, w, h);
}

NSVGimage* parseSvg(const std::string& path) {
    std::string text = readFile(path);
    NSVGimage* svg = nsvgParse(&text[0], "px", 96.f);   // nsvgParse edits the text in place
    if (!svg) throw std::runtime_error("media: cannot parse SVG " + path);
    return svg;
}

// ffmpeg's first frame of `path` as PNG, decoded (WebP, AVIF, TIFF, ...).
Image imageViaFfmpeg(const std::string& path) {
    std::string ff = requireFfmpeg("decode " + path);
    std::string png = readProcess(quote(ff) + " -nostdin -v error -i " + quote(path) +
                                  " -frames:v 1 -f image2pipe -c:v png -pix_fmt rgba -");
    int w = 0, h = 0, n = 0;
    unsigned char* px = png.empty() ? nullptr
        : stbi_load_from_memory((const unsigned char*)png.data(), (int)png.size(), &w, &h, &n, 4);
    if (!px) throw std::runtime_error("media: cannot decode image " + path);
    return imageFromRGBA(px, w, h);
}

// Fits an iw x ih image into `dst`; returns the visible part of the placed image (local
// units) and the matching source region (image units).
bool place(float iw, float ih, Rectangle dst, const ImageStyle& st, Rectangle& box, Rectangle& src) {
    if (iw <= 0 || ih <= 0 || dst.width <= 0 || dst.height <= 0) return false;
    float bw = dst.width, bh = dst.height;
    if (st.fit == Fit::Contain || st.fit == Fit::Cover) {
        float k = st.fit == Fit::Contain ? std::min(dst.width / iw, dst.height / ih)
                                         : std::max(dst.width / iw, dst.height / ih);
        bw = iw * k; bh = ih * k;
    } else if (st.fit == Fit::None) {
        bw = iw; bh = ih;
    }
    Rectangle full{ dst.x + (dst.width - bw) * st.align.x, dst.y + (dst.height - bh) * st.align.y, bw, bh };
    float x0 = std::max(full.x, dst.x), y0 = std::max(full.y, dst.y);
    float x1 = std::min(full.x + bw, dst.x + dst.width), y1 = std::min(full.y + bh, dst.y + dst.height);
    if (x1 <= x0 || y1 <= y0) return false;
    box = { x0, y0, x1 - x0, y1 - y0 };
    float sx = iw / bw, sy = ih / bh;
    src = { (x0 - full.x) * sx, (y0 - full.y) * sy, (x1 - x0) * sx, (y1 - y0) * sy };
    return true;
}

} // namespace

// ------------------------------------------------------------------ files and tools
void addSearchPath(const std::string& dir) { searchDirs.push_back(dir); }

std::string resolve(const std::string& file) {
    std::error_code ec;
    if (fs::is_regular_file(fs::u8path(file), ec)) return file;
    std::vector<std::string> dirs(searchDirs.rbegin(), searchDirs.rend());
    dirs.push_back(envDir("RAYMOTION_ASSETS"));
    dirs.push_back(std::string(GetApplicationDirectory()) + "assets");
#ifdef RMTN_BUNDLED_ASSETS
    dirs.push_back(RMTN_BUNDLED_ASSETS);
#endif
    if (!fs::u8path(file).is_absolute()) {
        for (const auto& d : dirs) {
            if (d.empty()) continue;
            fs::path p = fs::u8path(d) / fs::u8path(file);
            if (fs::is_regular_file(p, ec)) return p.u8string();
        }
    }
    throw std::runtime_error("media: file not found: " + file);
}

std::string ffmpeg() {
    if (!forcedFfmpeg.empty()) return forcedFfmpeg;
    if (!searchedFfmpeg) { foundFfmpeg = searchFfmpeg(); searchedFfmpeg = true; }
    return foundFfmpeg;
}
void setFfmpeg(const std::string& path) { forcedFfmpeg = path; }

Image loadImage(const std::string& file, int svgWidth) {
    const std::string path = resolve(file);
    const std::string ext = lowerExt(path);
    if (ext == ".svg") {
        NSVGimage* svg = parseSvg(path);
        Image img;
        try { img = rasterizeSvg(svg, svgWidth); } catch (...) { nsvgDelete(svg); throw; }
        nsvgDelete(svg);
        return img;
    }
    if (ext == ".qoi" || ext == ".dds") {
        Image img = LoadImage(path.c_str());
        if (!img.data) throw std::runtime_error("media: cannot decode image " + path);
        return img;
    }
    std::string bytes = readFile(path);
    const auto* data = (const unsigned char*)bytes.data();
    int w = 0, h = 0, n = 0;
    if (stbi_is_hdr_from_memory(data, (int)bytes.size())) {
        float* px = stbi_loadf_from_memory(data, (int)bytes.size(), &w, &h, &n, 4);
        if (!px) throw std::runtime_error("media: cannot decode image " + path);
        Image img{};
        img.data = px; img.width = w; img.height = h; img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;
        return img;
    }
    if (unsigned char* px = stbi_load_from_memory(data, (int)bytes.size(), &w, &h, &n, 4))
        return imageFromRGBA(px, w, h);
    return imageViaFfmpeg(path);
}

// ------------------------------------------------------------------ images
struct ImageAsset::State : GpuOwner {
    std::string path;
    bool decoded = false, vector = false;
    int w = 0, h = 0;
    NSVGimage* svg = nullptr;
    std::vector<Image> pending;                  // decoded frames awaiting upload
    std::vector<Texture2D> frames;
    std::vector<double> ends;                    // GIF: cumulative frame end times, seconds
    struct Raster { int width; Texture2D tex; unsigned long long used; };
    std::vector<Raster> rasters;                 // SVG textures by width, least recent evicted
    unsigned long long clock = 0;

    ~State() override {
        owners().erase(this);
        for (auto& img : pending) UnloadImage(img);
        if (svg) nsvgDelete(svg);
        // Textures are released by shutdownGraphics, before the GL context goes away.
    }
    void releaseGpu() override {
        for (auto& t : frames) UnloadTexture(t);
        for (auto& r : rasters) UnloadTexture(r.tex);
        frames.clear();                           // bitmaps decode again on next use
        rasters.clear();
    }
    void decode() {
        if (decoded) return;
        if (path.empty()) path = resolve(fileName);
        const std::string ext = lowerExt(path);
        if (ext == ".svg") {
            if (!svg) svg = parseSvg(path);
            vector = true;
            w = std::max(1, (int)std::ceil(svg->width));
            h = std::max(1, (int)std::ceil(svg->height));
        } else if (ext == ".gif") {
            std::string bytes = readFile(path);
            int* delays = nullptr;
            int z = 0, n = 0;
            unsigned char* px = stbi_load_gif_from_memory((const unsigned char*)bytes.data(), (int)bytes.size(),
                                                          &delays, &w, &h, &z, &n, 4);
            if (!px) throw std::runtime_error("media: cannot decode GIF " + path);
            double t = 0;
            ends.clear();
            for (int i = 0; i < z; i++) {
                unsigned char* frame = (unsigned char*)std::malloc((size_t)w * h * 4);
                std::memcpy(frame, px + (size_t)w * h * 4 * i, (size_t)w * h * 4);
                pending.push_back(imageFromRGBA(frame, w, h));
                int ms = delays ? delays[i] : 0;
                t += (ms <= 10 ? 100 : ms) / 1000.0;   // as browsers: tiny delays play at 10 fps
                ends.push_back(t);
            }
            stbi_image_free(px);
            stbi_image_free(delays);
        } else {
            Image img = loadImage(path);
            w = img.width; h = img.height;
            pending.push_back(img);
        }
        decoded = true;
    }
    std::string fileName;
};

ImageAsset::ImageAsset(std::string file) : file_(std::move(file)), s_(std::make_unique<State>()) {
    s_->fileName = file_;
    owners().insert(s_.get());
}
ImageAsset::~ImageAsset() = default;

int ImageAsset::width() { s_->decode(); return s_->w; }
int ImageAsset::height() { s_->decode(); return s_->h; }
bool ImageAsset::isVector() { s_->decode(); return s_->vector; }
int ImageAsset::frameCount() { s_->decode(); return s_->ends.empty() ? 1 : (int)s_->ends.size(); }
double ImageAsset::duration() { s_->decode(); return s_->ends.empty() ? 0 : s_->ends.back(); }

int ImageAsset::frameAt(double seconds) {
    s_->decode();
    const auto& ends = s_->ends;
    if (ends.size() < 2) return 0;
    double t = std::fmod(seconds, ends.back());
    if (t < 0) t += ends.back();
    return (int)(std::upper_bound(ends.begin(), ends.end(), t) - ends.begin()) % (int)ends.size();
}

Texture2D ImageAsset::texture(int frame, float pixelWidth) {
    State& s = *s_;
    s.decode();
    ensureHook();
    if (s.vector) {
        // Rasterise in 1/8-octave steps (at most ~9% above the drawn size), up to 8192 px.
        float want = pixelWidth > 0 ? pixelWidth : (float)s.w;
        int width = (int)std::ceil(std::exp2(std::ceil(std::log2(std::max(want, 8.f)) * 8.f) / 8.f));
        width = std::min(width, 8192);
        for (auto& r : s.rasters)
            if (r.width == width) { r.used = ++s.clock; return r.tex; }
        Image img = rasterizeSvg(s.svg, width);
        rlDrawRenderBatchActive();
        Texture2D tex = uploadMipmapped(img);
        UnloadImage(img);
        if (s.rasters.size() >= 3) {
            auto old = std::min_element(s.rasters.begin(), s.rasters.end(),
                                        [](const State::Raster& a, const State::Raster& b) { return a.used < b.used; });
            UnloadTexture(old->tex);
            s.rasters.erase(old);
        }
        s.rasters.push_back({ width, tex, ++s.clock });
        return tex;
    }
    if (s.frames.empty()) {
        if (s.pending.empty()) { s.decoded = false; s.decode(); }
        rlDrawRenderBatchActive();
        for (auto& img : s.pending) { s.frames.push_back(uploadMipmapped(img)); UnloadImage(img); }
        s.pending.clear();
    }
    int n = (int)s.frames.size();
    return s.frames[((frame % n) + n) % n];
}

ImagePtr image(const std::string& file) {
    static std::map<std::string, ImagePtr> cache;
    auto& slot = cache[file];
    if (!slot) slot = std::make_shared<ImageAsset>(file);
    return slot;
}

void drawTexture(const Ctx& c, Texture2D tex, Rectangle src, Rectangle dst, Color tint) {
    if (!tex.id || tex.width <= 0 || tex.height <= 0) return;
    Color k = withAlpha(tint, c.a);
    if (k.a == 0) return;
    const float u0 = src.x / tex.width, v0 = src.y / tex.height;
    const float u1 = (src.x + src.width) / tex.width, v1 = (src.y + src.height) / tex.height;
    Vector2 tl = mapP(c, { dst.x, dst.y }), tr = mapP(c, { dst.x + dst.width, dst.y });
    Vector2 br = mapP(c, { dst.x + dst.width, dst.y + dst.height }), bl = mapP(c, { dst.x, dst.y + dst.height });
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(k.r, k.g, k.b, k.a);
    rlNormal3f(0, 0, 1);
    rlTexCoord2f(u0, v0); rlVertex2f(tl.x, tl.y);
    rlTexCoord2f(u0, v1); rlVertex2f(bl.x, bl.y);
    rlTexCoord2f(u1, v1); rlVertex2f(br.x, br.y);
    rlTexCoord2f(u1, v0); rlVertex2f(tr.x, tr.y);
    rlEnd();
    rlSetTexture(0);
}

void drawImage(const Ctx& c, ImageAsset& img, Rectangle dst, const ImageStyle& style, double seconds) {
    const float iw = (float)img.width(), ih = (float)img.height();
    Rectangle box, src;
    if (!place(iw, ih, dst, style, box, src)) return;
    // Pixels across the whole placed image on the current target (SVG raster size).
    float scale = box.width / src.width * iw;
    Texture2D tex = img.texture(img.frameAt(seconds), scale * c.s * canvasScale());
    const float k = (float)tex.width / iw;
    drawTexture(c, tex, { src.x * k, src.y * k, src.width * k, src.height * k }, box, style.tint);
}

std::function<void(const Ctx&, int)> imageFn(ImagePtr img, Rectangle dst, ImageStyle style) {
    return [img, dst, style](const Ctx& c, int t) { drawImage(c, *img, dst, style, (double)t / frameRate()); };
}

// ------------------------------------------------------------------ video
namespace {

double parseRate(const std::string& r) {
    size_t slash = r.find('/');
    double a = std::atof(r.c_str()), b = slash == std::string::npos ? 1 : std::atof(r.c_str() + slash + 1);
    return b > 0 ? a / b : 0;
}

struct ProbeResult { VideoInfo info; std::string rate; };   // rate: exact, e.g. "30000/1001"

const ProbeResult& probeFull(const std::string& file) {
    static std::map<std::string, ProbeResult> cache;
    const std::string path = resolve(file);
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;
    std::string ff = requireFfmpeg("read " + path);
    std::string out = readProcess(quote(ffprobeFor(ff)) + " -v error -show_entries"
        " stream=codec_type,width,height,r_frame_rate,avg_frame_rate:format=duration -of default=nw=1 " + quote(path));
    ProbeResult r;
    bool inVideo = false, seenVideo = false;       // fields of the first video stream only
    std::istringstream lines(out);
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "codec_type") {
            inVideo = v == "video" && !seenVideo;
            seenVideo = seenVideo || inVideo;
            if (v == "audio") r.info.hasAudio = true;
        } else if (k == "duration") {
            if (v != "N/A") r.info.duration = std::atof(v.c_str());
        } else if (inVideo) {
            if (k == "width") r.info.width = std::atoi(v.c_str());
            else if (k == "height") r.info.height = std::atoi(v.c_str());
            else if (k == "r_frame_rate" && parseRate(v) > 0) { r.rate = v; r.info.fps = parseRate(v); }
            else if (k == "avg_frame_rate" && r.info.fps <= 0 && parseRate(v) > 0) { r.rate = v; r.info.fps = parseRate(v); }
        }
    }
    if (out.empty()) throw std::runtime_error("media: cannot probe " + path + " (is ffprobe beside ffmpeg?)");
    return cache[path] = r;
}

} // namespace

VideoInfo probe(const std::string& file) { return probeFull(file).info; }

struct VideoDecoder::State : GpuOwner {
    std::string file, path, rate;
    VideoInfo info;
    bool probed = false;
    FILE* pipe = nullptr;
    int next = 0;                 // index of the frame the pipe yields next
    int shown = -1;               // index in `tex`
    int count = INT_MAX;          // frames known to exist (set at end of stream)
    std::vector<unsigned char> buf;
    Texture2D tex{};

    ~State() override { owners().erase(this); closePipe(); }
    void closePipe() { if (pipe) { pclose(pipe); pipe = nullptr; } }
    void releaseGpu() override {
        closePipe();
        if (tex.id) UnloadTexture(tex);
        tex = Texture2D{};
        shown = -1;
    }
    void ensureProbe() {
        if (probed) return;
        const ProbeResult& r = probeFull(file);
        path = resolve(file);
        info = r.info;
        rate = r.rate;
        if (info.width <= 0 || info.fps <= 0) throw std::runtime_error("media: no video stream in " + path);
        count = std::max(1, (int)std::ceil(info.duration * info.fps - 1e-3));
        buf.resize((size_t)info.width * info.height * 4);
        probed = true;
    }
    void start(int index) {
        closePipe();
        // Seek a quarter frame early so rounding cannot skip the wanted frame; the fps
        // filter then emits one frame per source frame from there.
        double t = std::max(0.0, (index - 0.25) / info.fps);
        char ss[32];
        snprintf(ss, sizeof ss, "%.6f", t);
        std::string ff = requireFfmpeg("decode " + path);
        pipe = openRead(quote(ff) + " -nostdin -v error -ss " + ss + " -i " + quote(path) +
                        " -an -sn -vf fps=" + rate + " -f rawvideo -pix_fmt rgba - 2>" + DEVNULL);
        if (!pipe) throw std::runtime_error("media: cannot start ffmpeg for " + path);
        next = index;
    }
    bool readFrame() { return fread(buf.data(), 1, buf.size(), pipe) == buf.size(); }
    Texture2D frame(int index) {
        ensureProbe();
        ensureHook();
        index = std::max(0, std::min(index, count - 1));
        if (index == shown && tex.id) return tex;
        // Forward within two seconds: keep reading; otherwise restart at the frame.
        if (!pipe || index < next || index > next + (int)(2 * info.fps) + 1) start(index);
        while (next <= index) {
            if (!readFrame()) {
                closePipe();
                if (next == 0) throw std::runtime_error("media: cannot decode video " + path);
                count = next;                    // the stream ended early: hold its last frame
                return frame(count - 1);
            }
            next++;
        }
        rlDrawRenderBatchActive();               // pending draws may still use the old frame
        if (!tex.id) {
            Image img = imageFromRGBA(buf.data(), info.width, info.height);
            tex = LoadTextureFromImage(img);
            if (!tex.id) throw std::runtime_error("media: cannot create a video texture");
            SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);
        } else {
            UpdateTexture(tex, buf.data());
        }
        GenTextureMipmaps(&tex);
        SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
        shown = index;
        return tex;
    }
};

VideoDecoder::VideoDecoder(std::string file) : file_(std::move(file)), s_(std::make_unique<State>()) {
    s_->file = file_;
    owners().insert(s_.get());
}
VideoDecoder::~VideoDecoder() = default;

const VideoInfo& VideoDecoder::info() { s_->ensureProbe(); return s_->info; }

Texture2D VideoDecoder::frameAt(double seconds) {
    s_->ensureProbe();
    return s_->frame((int)std::floor(seconds * s_->info.fps + 1e-4));
}

void video(TNode* node, const std::string& file, Rectangle dst, VideoOptions o) {
    auto dec = std::make_shared<VideoDecoder>(file);
    node->fn = [dec, dst, o](const Ctx& c, int t) {
        const VideoInfo& info = dec->info();
        double end = o.endAt > 0 ? std::min(o.endAt, info.duration) : info.duration;
        double sec = o.startFrom + (double)t / frameRate() * o.playbackRate;
        if (o.loop && end > o.startFrom) sec = o.startFrom + std::fmod(sec - o.startFrom, end - o.startFrom);
        else sec = std::min(sec, end - 0.5 / info.fps);          // hold the last frame
        Texture2D tex = dec->frameAt(sec);
        Rectangle box, src;
        if (place((float)info.width, (float)info.height, dst, o.style, box, src))
            drawTexture(c, tex, src, box, o.style.tint);
    };
    if (o.muted) return;
    AudioClip clip;
    clip.file = file;
    clip.startFrom = o.startFrom; clip.endAt = o.endAt;
    clip.playbackRate = o.playbackRate; clip.loop = o.loop;
    clip.volume = o.volume; clip.volumeAt = o.volumeAt;
    node->audio.push_back(std::move(clip));
}

// ------------------------------------------------------------------ audio
AudioClip& audio(TNode* node, const std::string& file, AudioOptions o) {
    AudioClip clip;
    clip.file = file;
    clip.startFrom = o.startFrom; clip.endAt = o.endAt;
    clip.playbackRate = o.playbackRate; clip.loop = o.loop;
    clip.volume = o.volume; clip.volumeAt = std::move(o.volumeAt);
    node->audio.push_back(std::move(clip));
    return node->audio.back();
}

const std::vector<float>& decodeAudio(const std::string& file, int rate) {
    static std::map<std::pair<std::string, int>, std::vector<float>> cache;
    const std::string path = resolve(file);
    auto key = std::make_pair(path, rate);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    std::vector<float> pcm;
    const std::string ext = lowerExt(path);
    bool native = false;
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".qoa") {
        Wave w = LoadWave(path.c_str());
        if (w.data && w.frameCount > 0) {
            WaveFormat(&w, rate, 32, 2);
            const float* f = (const float*)w.data;
            pcm.assign(f, f + (size_t)w.frameCount * 2);
            native = true;
        }
        UnloadWave(w);
    }
    if (!native) {
        std::string ff = requireFfmpeg("decode audio from " + path);
        std::string raw = readProcess(quote(ff) + " -nostdin -v error -i " + quote(path) +
                                      " -vn -ac 2 -ar " + std::to_string(rate) + " -f f32le - 2>" + DEVNULL);
        pcm.resize(raw.size() / sizeof(float) / 2 * 2);
        std::memcpy(pcm.data(), raw.data(), pcm.size() * sizeof(float));
        // Silence is fine for a video without sound; anything else that yields none is an error.
        if (pcm.empty() && probe(path).hasAudio) throw std::runtime_error("media: cannot decode audio from " + path);
    }
    return cache[key] = std::move(pcm);
}

namespace {

// One clip occurrence: the clip's node-local frame 0 is at absolute frame `origin`, and
// it plays for local frames [lo, hi).
struct Occurrence { const AudioClip* clip; long long origin; int lo, hi; };

void collect(const TNode& n, long long origin, int lo, int hi, std::vector<Occurrence>& out) {
    int a = std::max(lo, n.start), b = std::min(hi, n.start + n.dur);
    if (a >= b || n.freezeAt >= 0) return;               // Freeze silences its content
    int ta = a - n.start, tb = b - n.start;              // node-local frames
    long long base = origin + n.start;
    auto content = [&](long long o, int clo, int chi) {
        for (const auto& clip : n.audio) out.push_back({ &clip, o, clo, chi });
        for (const auto& k : n.kids) collect(*k, o, clo, chi, out);
    };
    if (n.loopEvery > 0) {
        const int L = n.loopEvery;
        for (int k = ta / L; k * L < tb; k++)
            content(base + (long long)k * L, std::max(ta - k * L, 0), std::min(tb - k * L, L));
    } else {
        content(base, ta, tb);
    }
}

bool anyClip(const TNode& n) {
    if (!n.audio.empty()) return true;
    for (const auto& k : n.kids) if (anyClip(*k)) return true;
    return false;
}

} // namespace

bool hasAudio(const Comp& c) {
    if (c.music && !c.song.empty()) return true;
    for (const auto& r : c.roots) if (anyClip(*r)) return true;
    return false;
}

std::vector<float> mixAudio(const Comp& c, int rate) {
    const double fps = c.fps;
    const size_t frames = (size_t)std::ceil(c.duration / fps * rate);
    std::vector<float> out(frames * 2, 0.f);
    std::vector<Occurrence> occ;
    for (const auto& r : c.roots) collect(*r, 0, 0, c.duration, occ);
    for (const Occurrence& o : occ) {
        const AudioClip& clip = *o.clip;
        const std::vector<float>& pcm = decodeAudio(clip.file, rate);
        const long long srcFrames = (long long)(pcm.size() / 2);
        const double s0 = clip.startFrom * rate;
        const double s1 = clip.endAt > 0 ? std::min(clip.endAt * rate, (double)srcFrames) : (double)srcFrames;
        if (s1 <= s0 || o.hi <= o.lo) continue;
        // Gain per local frame (one past the end too), linearly interpolated per sample.
        std::vector<float> gain((size_t)(o.hi - o.lo + 1));
        for (int f = o.lo; f <= o.hi; f++)
            gain[(size_t)(f - o.lo)] = clip.volume * (clip.volumeAt ? clip.volumeAt(f) : 1.f);
        const double originSec = o.origin / fps;
        long long a = (long long)std::ceil((o.origin + o.lo) / fps * rate);
        long long b = (long long)std::ceil((o.origin + o.hi) / fps * rate);
        a = std::max(a, 0LL);
        b = std::min(b, (long long)frames);
        for (long long s = a; s < b; s++) {
            double tau = (double)s / rate - originSec;           // seconds since local frame 0
            double pos = s0 + tau * rate * clip.playbackRate;
            if (pos >= s1) {
                if (!clip.loop) break;
                pos = s0 + std::fmod(pos - s0, s1 - s0);
            }
            long long i = (long long)pos;
            double frac = pos - (double)i;
            long long j = std::min(i + 1, srcFrames - 1);
            double fl = tau * fps - o.lo;
            size_t gi = (size_t)std::max(0.0, std::min(fl, (double)(gain.size() - 1)));
            double gu = std::max(0.0, std::min(fl - (double)gi, 1.0));
            double g = gain[gi] + (gain[std::min(gi + 1, gain.size() - 1)] - gain[gi]) * gu;
            for (int ch = 0; ch < 2; ch++) {
                double v = pcm[(size_t)i * 2 + ch] + (pcm[(size_t)j * 2 + ch] - pcm[(size_t)i * 2 + ch]) * frac;
                out[(size_t)s * 2 + ch] += (float)(v * g);
            }
        }
    }
    if (c.music && !c.song.empty()) {
        std::vector<float> song = renderSong(c.song, (double)frames / rate, rate);
        for (size_t s = 0; s < frames && s < song.size(); s++) { out[s * 2] += song[s]; out[s * 2 + 1] += song[s]; }
    }
    for (auto& v : out) v = std::max(-1.f, std::min(1.f, v));
    return out;
}

bool writeWav(const std::string& path, const std::vector<float>& stereo, int rate) {
    std::ofstream o(fs::u8path(path), std::ios::binary);
    if (!o) return false;
    const uint32_t dataBytes = (uint32_t)(stereo.size() * 2);
    auto w32 = [&o](uint32_t v) { char b[4] = { (char)v, (char)(v >> 8), (char)(v >> 16), (char)(v >> 24) }; o.write(b, 4); };
    auto w16 = [&o](uint16_t v) { char b[2] = { (char)v, (char)(v >> 8) }; o.write(b, 2); };
    o.write("RIFF", 4); w32(36 + dataBytes); o.write("WAVEfmt ", 8); w32(16);
    w16(1); w16(2); w32((uint32_t)rate); w32((uint32_t)rate * 4); w16(4); w16(16);
    o.write("data", 4); w32(dataBytes);
    std::vector<int16_t> s(stereo.size());
    for (size_t i = 0; i < stereo.size(); i++) s[i] = (int16_t)std::lround(std::max(-1.f, std::min(1.f, stereo[i])) * 32767.f);
    o.write((const char*)s.data(), (std::streamsize)dataBytes);
    return (bool)o;
}

} // namespace media
} // namespace rmtn
