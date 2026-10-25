// cli.cpp -- raymotion command line: list | preview | render | still
#include "rmtn/cli.hpp"
#include <raylib.h>
#include <rlgl.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include "rmtn/timeline.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/synth.hpp"
#include "rmtn/preview.hpp"
#include "rmtn/render.hpp"
#include "rmtn/frame_stream.hpp"
#include "rmtn/media.hpp"
#include "rmtn/prefab.hpp"

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace rmtn {
namespace {

const int AUDIO_RATE = 48000;

// Preview sound: the mixed soundtrack streamed from a cursor. While it plays the playhead
// follows the audio clock, so picture and sound stay together.
class PreviewAudio {
public:
    bool start(std::vector<float> mix) {
        mix_ = std::move(mix);
        InitAudioDevice();
        if (!IsAudioDeviceReady()) return false;
        stream_ = LoadAudioStream(AUDIO_RATE, 32, 2);
        SetAudioStreamCallback(stream_, feed);
        PlayAudioStream(stream_);
        on_ = true;
        return true;
    }
    bool active() const { return on_; }
    bool playing() const { return playing_; }
    void play(bool p) { playing_ = p; }
    void seek(double seconds) { cursor_ = (long long)std::llround(seconds * AUDIO_RATE); }
    double seconds() const { return (double)cursor_ / AUDIO_RATE; }
    void stop() {
        if (!on_) return;
        playing_ = false;
        UnloadAudioStream(stream_);
        CloseAudioDevice();
        on_ = false;
    }
private:
    static void feed(void* out, unsigned int frames) {     // audio thread
        float* o = (float*)out;
        const long long n = (long long)(mix_.size() / 2);
        long long from = cursor_.load(), c = from;
        for (unsigned int i = 0; i < frames; i++) {
            if (!playing_ || n == 0) { o[2 * i] = o[2 * i + 1] = 0; continue; }
            if (c >= n || c < 0) c = 0;                       // the preview loops
            o[2 * i] = mix_[(size_t)c * 2];
            o[2 * i + 1] = mix_[(size_t)c * 2 + 1];
            c++;
        }
        cursor_.compare_exchange_strong(from, c);            // a seek meanwhile wins
    }
    static inline std::vector<float> mix_;
    static inline std::atomic<long long> cursor_{ 0 };
    static inline std::atomic<bool> playing_{ false };
    AudioStream stream_{};
    bool on_ = false;
};

void drawFrame(Comp& c,int frame,Canvas& canvas){
    presentFrame(c,frame,canvas);
    ClearBackground(BLACK);
    DrawTexturePro(canvas.output().texture,{0,0,(float)c.w,-(float)c.h},
                   fitCanvas(c.w,c.h,GetScreenWidth(),GetScreenHeight()), {0,0},0,WHITE);
}

// Supersampling factor: --ss overrides the composition; drafts and custom renderers use 1.
int canvasScale(const Comp& c,const char* ssArg,bool draft){
    if (c.render || draft) return 1;
    return ssArg ? atoi(ssArg) : c.supersample;
}

void closeCanvas(Canvas& canvas){
    canvas.unload();
    shutdownGraphics();
    CloseWindow();
}

// Canvas pixels top-down, resized to the output size. Caller unloads.
// With `alpha`, the canvas holds premultiplied colour over a transparent background: it is
// resized premultiplied (so edges do not pick up dark fringes), then made straight. Without
// it the frame is opaque, as in an mp4: a transparent background shows as black.
Image grabFrame(const Canvas& canvas,int W,int H,bool alpha=false){
    Image image=LoadImageFromTexture(canvas.output().texture);
    if (!image.data) return image;
    ImageFlipVertical(&image);
    if (image.width!=W || image.height!=H) ImageResize(&image,W,H);
    unsigned char* px=(unsigned char*)image.data;
    const size_t count=(size_t)image.width*image.height;
    if (alpha) unpremultiply(px,count);
    else for (size_t i=0;i<count;i++) px[i*4+3]=255;
    return image;
}

bool saveFrame(const Canvas& canvas,int W,int H,const std::string& path,bool alpha=false){
    Image image=grabFrame(canvas,W,H,alpha);
    if (!image.data) return false;
    bool ok=ExportImage(image,path.c_str());
    UnloadImage(image);
    if (!ok) fprintf(stderr,"cannot write frame: %s\n",path.c_str());
    return ok;
}

std::string quote(const std::string& s) { return "\"" + s + "\""; }

// Output containers. mp4 is H.264 (or --encoder); prores is ProRes 4444 in .mov, the
// editing format Premiere, Resolve and Final Cut take with alpha; webm is VP9, with alpha
// for browsers and OBS.
enum class Format { Mp4, ProRes, WebM };
const char* extension(Format f) { return f == Format::ProRes ? ".mov" : f == Format::WebM ? ".webm" : ".mp4"; }

std::string formatArgs(Format f, bool alpha, int crf) {
    if (f == Format::ProRes)
        return std::string(" -c:v prores_ks -profile:v 4444 -vendor apl0 -pix_fmt ") +
               (alpha ? "yuva444p10le -alpha_bits 16" : "yuv444p10le") + " -c:a pcm_s16le";
    // VP9 alpha needs auto-alt-ref off; the crf scale is 0..63, so ours is scaled up a little.
    return " -c:v libvpx-vp9 -crf " + std::to_string(std::min(63, crf + 12)) + " -b:v 0 -row-mt 1 -deadline good -cpu-used 4" +
           (alpha ? " -pix_fmt yuva420p -auto-alt-ref 0" : " -pix_fmt yuv420p") + " -c:a libopus -b:a 160k";
}

bool endsWith(const std::string& s, const char* tail) {
    const size_t n = strlen(tail);
    return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

// ffmpeg video codec arguments. Each hardware family spells constant quality and speed its
// own way, so --crf and --preset are translated: the scales are close, not identical.
std::string videoCodecArgs(const std::string& encoder, const char* preset, int crf) {
    const std::string q = std::to_string(crf);
    std::string a = " -c:v " + encoder;
    if (endsWith(encoder, "_nvenc"))
        a += " -rc vbr -cq " + q + " -b:v 0 -pix_fmt yuv420p";
    else if (endsWith(encoder, "_qsv"))
        a += " -global_quality " + q + " -pix_fmt nv12";
    else if (endsWith(encoder, "_amf"))
        a += " -rc cqp -qp_i " + q + " -qp_p " + q + " -qp_b " + q + " -pix_fmt yuv420p";
    else
        a += " -crf " + q + " -pix_fmt yuv420p";
    if (preset) a += (endsWith(encoder, "_amf") ? " -quality " : " -preset ") + std::string(preset);
    return a;
}

// cmd.exe strips the first and last quote of a command line that holds several quoted
// tokens, so on Windows the whole line gets one more enclosing pair.
std::string shellLine(const std::string& cmd) {
#ifdef _WIN32
    return "\"" + cmd + "\"";
#else
    return cmd;
#endif
}

// --props value: inline JSON, or @path to a JSON file.
Json loadProps(const char* arg) {
    if (arg[0] != '@') return Json::parse(arg);
    std::ifstream in(std::filesystem::u8path(arg + 1), std::ios::binary);
    if (!in) throw std::runtime_error(std::string("cannot read props file: ") + (arg + 1));
    std::stringstream text;
    text << in.rdbuf();
    return Json::parse(text.str());
}

// Builds a composition from its default props merged with `overrides`, then checks it.
// `drawing` runs the composition's asset hook first; listing metadata skips it.
void prepare(Comp& c, const Json& overrides, bool drawing) {
    const Json props = c.defaultProps.merged(overrides.isNull() ? Json(Json::Object{}) : overrides);
    if (drawing && c.assets) c.assets(c, props);
    if (c.build) c.build(c, props);
    else if (overrides.size()) fprintf(stderr, "note: '%s' takes no props; --props ignored\n", c.name.c_str());
    if (c.w <= 0 || c.h <= 0 || c.fps <= 0 || c.duration <= 0)
        throw std::runtime_error("composition '" + c.name + "' needs positive size, fps and duration");
}

void usage() {
    puts("usage:");
    puts("  raymotion list                                  list compositions");
    puts("  raymotion preview <name> [--mute]               play with sound in a live window (esc quits)");
    puts("  raymotion render  <name> [--crf N] [--encoder NAME] [--preset NAME]");
    puts("                        [--draft] [--mute] [--png-frames]");
    puts("                        render every frame via raylib, pipe to ffmpeg -> mp4");
    puts("                        --png-frames writes a PNG sequence instead (no ffmpeg)");
    puts("                        --encoder NAME any ffmpeg encoder (default h264 = libx264;");
    puts("                          h264_nvenc, hevc_nvenc, h264_qsv, h264_amf are hardware)");
    puts("                        --preset NAME encoder speed: ultrafast..veryslow for x264,");
    puts("                          p1..p7 for nvenc, speed|balanced|quality for amf");
    puts("                        --crf N  quality, lower is better (default 19)");
    puts("                        --format F mp4 (default), prores (ProRes 4444 .mov) or");
    puts("                          webm (VP9)");
    puts("                        --alpha  transparent background (prores, webm or");
    puts("                          --png-frames; prores is the default with --alpha)");
    puts("  raymotion still   <name> --at FRAME [--png PATH] write one PNG frame");
    puts("                        --alpha  transparent background (an RGBA PNG)");
    puts("  raymotion props   <name>                        print the default props (JSON)");
    puts("  raymotion prefabs                               list prefabs and their default props");
    puts("options: --ffmpeg PATH overrides auto-detected ffmpeg binary");
    puts("         --ss N        supersampling 1, 2 or 4 (default: the composition's)");
    puts("         --props JSON  input props merged over the defaults; @file.json reads a file");
}

int runMain(int argc, const char** argv, const RegisterFn& registerAll) {
    const char* mode = argc > 1 ? argv[1] : "";
    if (!strcmp(mode, "list")) {
        Registry comps; registerAll(comps);
        for (auto& c : comps) {
            prepare(c, Json(), false);
            printf("%-22s %dx%d @%dfps  %.1fs  audio=%d\n", c.name.c_str(), c.w, c.h, c.fps,
                   (double)c.duration / c.fps, (int)media::hasAudio(c));
        }
        return 0;
    }
    if (!strcmp(mode, "prefabs")) {
        Registry comps; registerAll(comps);   // registration also fills the prefab registry
        for (const auto& n : prefab::names())
            printf("%-22s %s\n", n.c_str(), prefab::get(n).defaultProps.dump().c_str());
        return 0;
    }
    if (!strcmp(mode, "help") || !mode[0]) { usage(); return 1; }

    if (argc < 3) { usage(); return 1; }
    const std::string want = argv[2];

    const char* crfArg = nullptr, *ffArg = nullptr, *atArg = nullptr, *pngArg = nullptr, *ssArg = nullptr, *propsArg = nullptr,
               *encoderArg = nullptr, *presetArg = nullptr, *formatArg = nullptr;
    bool draft = false, noMusic = false, pngFrames = false, alpha = false;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--crf") && i+1 < argc) crfArg = argv[++i];
        else if (!strcmp(argv[i], "--ffmpeg") && i+1 < argc) ffArg = argv[++i];
        else if (!strcmp(argv[i], "--encoder") && i+1 < argc) encoderArg = argv[++i];
        else if (!strcmp(argv[i], "--preset") && i+1 < argc) presetArg = argv[++i];
        else if (!strcmp(argv[i], "--at") && i+1 < argc) atArg = argv[++i];
        else if (!strcmp(argv[i], "--png") && i+1 < argc) pngArg = argv[++i];
        else if (!strcmp(argv[i], "--ss") && i+1 < argc) ssArg = argv[++i];
        else if (!strcmp(argv[i], "--props") && i+1 < argc) propsArg = argv[++i];
        else if (!strcmp(argv[i], "--draft")) draft = true;
        else if (!strcmp(argv[i], "--mute") || !strcmp(argv[i], "--no-music")) noMusic = true;
        else if (!strcmp(argv[i], "--png-frames")) pngFrames = true;
        else if (!strcmp(argv[i], "--format") && i+1 < argc) formatArg = argv[++i];
        else if (!strcmp(argv[i], "--alpha")) alpha = true;
    }
    Format format = alpha ? Format::ProRes : Format::Mp4;
    if (formatArg) {
        if (!strcmp(formatArg, "mp4")) format = Format::Mp4;
        else if (!strcmp(formatArg, "prores") || !strcmp(formatArg, "mov")) format = Format::ProRes;
        else if (!strcmp(formatArg, "webm")) format = Format::WebM;
        else { printf("unknown --format '%s' (mp4, prores or webm)\n", formatArg); return 1; }
    }
    if (alpha && format == Format::Mp4 && !pngFrames) {
        puts("mp4 (H.264) has no alpha channel: use --format prores or --format webm with --alpha");
        return 1;
    }
    if (encoderArg && format != Format::Mp4) { puts("--encoder applies to mp4 only"); return 1; }

    Registry comps; registerAll(comps);
    Comp* comp = nullptr;
    for (auto& c : comps) if (c.name == want) comp = &c;
    if (!comp) { printf("unknown composition '%s'\n", want.c_str()); usage(); return 1; }
    if (!strcmp(mode, "props")) { puts(comp->defaultProps.dump().c_str()); return 0; }
    if (ffArg) media::setFfmpeg(ffArg);   // before the asset hook, which may encode with it
    prepare(*comp, propsArg ? loadProps(propsArg) : Json(), true);
    if (alpha) {
        comp->background = BLANK;
        if (comp->render) fprintf(stderr, "note: '%s' draws with a custom renderer; its background may not be transparent\n", comp->name.c_str());
    }

    int W = comp->w, H = comp->h;
    if (draft) { W = (W / 2) & ~1; H = (H / 2) & ~1; }   // yuv420p needs even dimensions

    // ---- still: one deterministic frame --------------------------------------
    if (!strcmp(mode, "still")) {
        int at = atArg ? atoi(atArg) : 0;
        if (at < 0 || at >= comp->duration) { printf("frame out of range [0,%d)\n", comp->duration); return 1; }
        std::string path;
        if (pngArg) path = pngArg;
        else { char b[96]; snprintf(b, sizeof b, "out/%s_still_%04d.png", comp->name.c_str(), at); path = b; }
        std::error_code ec;
        auto parent=std::filesystem::path(path).parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent,ec);
        if (ec){ fprintf(stderr,"cannot create PNG directory: %s\n",ec.message().c_str()); return 2; }
        InitWindow(W, H, "raymotion-still");
        Canvas canvas;
        if (!canvas.load(comp->w,comp->h,canvasScale(*comp,ssArg,draft))){ CloseWindow(); return 2; }
        // Frames carry no state between them, so only the requested one is drawn.
        BeginDrawing();
        drawFrame(*comp, at, canvas);
        EndDrawing();              // capture the offscreen canvas, independent of desktop size
        bool ok = saveFrame(canvas,W,H,path,alpha);
        closeCanvas(canvas);
        if (!ok) return 2;
        printf("wrote %s\n", path.c_str());
        return 0;
    }

    // ---- preview: interactive playback ----------------------------------------
    if (!strcmp(mode, "preview")) {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
        InitWindow(W, H, ("raymotion preview - " + comp->name).c_str());
        // The desktop may clamp height independently; re-fit both dimensions.
        Rectangle initialFit=fitCanvas(comp->w,comp->h,GetScreenWidth(),GetScreenHeight());
        SetWindowSize((int)initialFit.width,(int)initialFit.height);
        SetWindowMinSize(240,240);
        Canvas canvas;
        if (!canvas.load(comp->w,comp->h,canvasScale(*comp,ssArg,draft))){ CloseWindow(); return 2; }
        SetTargetFPS(60);
        PreviewAudio audio;
        if (!noMusic && media::hasAudio(*comp) && !audio.start(media::mixAudio(*comp, AUDIO_RATE)))
            fprintf(stderr, "no audio device: previewing without sound\n");
        double playhead = 0, speed = 1.0;
        bool paused = false;
        while (!WindowShouldClose()) {   // esc (raylib's exit key) or the window's close button
            BeginDrawing();
            drawFrame(*comp, (int)playhead % comp->duration,canvas);
            char hud[96]; snprintf(hud, sizeof hud, "[%04d/%04d] x%.2f [%s]",
                (int)playhead % comp->duration, comp->duration, speed, paused ? "PAUSED" : "PLAY");
            DrawText(hud, 12, 12, 18, Color{ (unsigned char)240, (unsigned char)240, (unsigned char)255, (unsigned char)180 });
            EndDrawing();
            if (IsKeyDown(KEY_RIGHT)) speed = 2.0; else if (IsKeyDown(KEY_LEFT)) speed = 0.25; else speed = 1.0;
            int kp = GetKeyPressed();
            if (kp == KEY_SPACE) paused = !paused;   // rare spurious repeats; acceptable for a preview
            // Sound plays at normal speed only; then the audio clock drives the playhead.
            bool audible = audio.active() && !paused && speed == 1.0;
            if (audible) {
                if (!audio.playing()) audio.seek(std::fmod(playhead, (double)comp->duration) / comp->fps);
                audio.play(true);
                playhead = audio.seconds() * comp->fps;
            } else {
                audio.play(false);
                if (!paused) playhead += comp->fps * GetFrameTime() * speed;
            }
        }
        audio.stop();
        closeCanvas(canvas);
        return 0;
    }

    // ---- render: frames streamed into ffmpeg while the next ones draw ---------
    if (!strcmp(mode, "render")) {
        std::string dir = "out/" + comp->name + (draft ? "_draft_frames" : "_frames");
        std::filesystem::create_directories(dir);
        // Remove only generated numbered frames, never unrelated directory contents.
        for (const auto& entry : std::filesystem::directory_iterator(dir)){
            const std::string name=entry.path().filename().string();
            if (name.size()>=10 && name.front()=='f' && name.substr(name.size()-4)==".png" &&
                std::all_of(name.begin()+1,name.end()-4,[](char ch){ return ch>='0' && ch<='9'; }) &&
                entry.is_regular_file()) std::filesystem::remove(entry.path());
        }
        std::string ff;
        if (!pngFrames) {
            ff = media::ffmpeg();
            if (ff.empty()) { puts("ffmpeg not found (use --ffmpeg PATH, or --png-frames to skip encoding)"); return 2; }
        }
        std::string wav = "", mp4 = "out/" + comp->name + (draft ? "_draft" : "") + extension(format);
        if (!noMusic && !pngFrames && media::hasAudio(*comp)) {
            wav = dir + "/track.wav";
            if (!media::writeWav(wav, media::mixAudio(*comp, AUDIO_RATE), AUDIO_RATE)) { puts("wav write failed"); wav.clear(); }
        }

        FILE* pipe = nullptr;
        if (!pngFrames) {
            int crf = crfArg ? atoi(crfArg) : 19;
            std::string cmd = quote(ff) + " -y -hide_banner -loglevel warning"
                " -f rawvideo -pix_fmt rgba -s " + std::to_string(W) + "x" + std::to_string(H) +
                " -framerate " + std::to_string(comp->fps) + " -i -";
            if (!wav.empty()) cmd += " -i " + quote(wav);
            cmd += " -frames:v " + std::to_string(comp->duration);
            cmd += " -vf vflip";            // frames arrive bottom-up, straight from GL
            if (format == Format::Mp4) {
                cmd += videoCodecArgs(encoderArg ? encoderArg : "h264", presetArg, crf) + " -movflags +faststart";
                if (!wav.empty()) cmd += " -c:a aac -b:a 192k";
            } else cmd += formatArgs(format, alpha, crf);
            if (!wav.empty()) cmd += " -shortest";
            cmd += " " + quote(mp4);
            if (getenv("RAYMOTION_DEBUG_CMD")) printf("[CMD] %s\n", cmd.c_str());
#ifdef _WIN32
            pipe = popen(shellLine(cmd).c_str(), "wb");   // binary: no CRLF translation of pixels
#else
            pipe = popen(shellLine(cmd).c_str(), "w");    // POSIX pipes are always binary
#endif
            if (!pipe) { fprintf(stderr, "cannot start ffmpeg: %s\n", ff.c_str()); return 3; }
        }

        InitWindow(W, H, ("raymotion render - " + comp->name).c_str());
        Canvas canvas;
        if (!canvas.load(comp->w,comp->h,canvasScale(*comp,ssArg,draft))){ if (pipe) pclose(pipe); CloseWindow(); return 2; }
        // Drafts are downscaled on the GPU: an exact 2:1 bilinear draw is a 2x2 box filter.
        RenderTexture scaled{};
        const bool resize = W != comp->w || H != comp->h;
        FrameStream stream;
        if (pipe) {
            if (resize) {
                scaled = LoadRenderTexture(W, H);
                SetTextureFilter(canvas.output().texture, TEXTURE_FILTER_BILINEAR);
            }
            // Runs on the writer thread, so making alpha straight costs no drawing time.
            std::vector<unsigned char> straight;
            if (!stream.open(W, H, [pipe, alpha, straight](const unsigned char* rgba, size_t bytes) mutable {
                    if (!alpha) return fwrite(rgba, 1, bytes, pipe) == bytes;
                    straight.assign(rgba, rgba + bytes);
                    unpremultiply(straight.data(), bytes / 4);
                    return fwrite(straight.data(), 1, bytes, pipe) == bytes;
                })) { fprintf(stderr, "cannot start frame readback\n"); pclose(pipe); closeCanvas(canvas); return 3; }
        }
        double t0 = GetTime();
        bool ok = true;
        for (int f = 0; f < comp->duration && ok; f++) {
            BeginDrawing();
            drawFrame(*comp, f,canvas);
            EndDrawing();                 // flush before reading the canvas
            if (pngFrames) {
                char fname[160]; snprintf(fname, sizeof fname, "%s/f%05d.png", dir.c_str(), f);
                ok = saveFrame(canvas,W,H,fname,alpha);
            } else {
                if (resize) {
                    BeginTextureMode(scaled);
                    ClearBackground(BLANK);
                    // a copy, not a blend: transparent pixels must stay transparent
                    rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
                    BeginBlendMode(BLEND_CUSTOM);
                    DrawTexturePro(canvas.output().texture, { 0, 0, (float)comp->w, -(float)comp->h },
                                   { 0, 0, (float)W, (float)H }, { 0, 0 }, 0, WHITE);
                    EndBlendMode();
                    EndTextureMode();
                }
                ok = stream.push(resize ? scaled : canvas.output());
                if (!ok) fprintf(stderr, "ffmpeg stopped accepting frames at %d\n", f);
            }
            if (f % 15 == 0 || f+1 == comp->duration)
                printf("[%4d/%4d]\n", f, comp->duration);
        }
        if (pipe && !stream.close() && ok) { fprintf(stderr, "ffmpeg stopped accepting frames\n"); ok = false; }
        double dt = GetTime() - t0;
        if (scaled.id) UnloadRenderTexture(scaled);
        closeCanvas(canvas);
        if (pngFrames) {
            if (!ok) return 2;
            printf("rendered %d frames in %.1fs -> %s/\n", comp->duration, dt, dir.c_str());
            return 0;
        }
        int rc = pclose(pipe);
        if (!ok || rc != 0) { fprintf(stderr, "render failed: ffmpeg exited with %d (see its messages above)\n", rc); return 3; }
        printf("rendered %d frames in %.1fs -> %s\n", comp->duration, dt, mp4.c_str());
        return 0;
    }

    usage();
    return 1;
}

} // namespace

int runCli(int argc, const char** argv, const RegisterFn& registerAll){
    // raylib's INFO lines drown the CLI's own output; RAYMOTION_VERBOSE=1 restores them.
    if (!getenv("RAYMOTION_VERBOSE")) SetTraceLogLevel(LOG_WARNING);
    try { return runMain(argc,argv,registerAll); }
    catch (const std::exception& error){
        fprintf(stderr,"raymotion: %s\n",error.what());
        return 2;
    }
}

} // namespace rmtn
