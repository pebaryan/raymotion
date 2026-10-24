// cli.cpp -- raymotion command line: list | preview | render | still
#include "rmtn/cli.hpp"
#include <raylib.h>
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
#include "rmtn/media.hpp"

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
Image grabFrame(const Canvas& canvas,int W,int H){
    Image image=LoadImageFromTexture(canvas.output().texture);
    if (!image.data) return image;
    ImageFlipVertical(&image);
    if (image.width!=W || image.height!=H) ImageResize(&image,W,H);
    return image;
}

bool saveFrame(const Canvas& canvas,int W,int H,const std::string& path){
    Image image=grabFrame(canvas,W,H);
    if (!image.data) return false;
    bool ok=ExportImage(image,path.c_str());
    UnloadImage(image);
    if (!ok) fprintf(stderr,"cannot write frame: %s\n",path.c_str());
    return ok;
}

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
void prepare(Comp& c, const Json& overrides) {
    if (c.build) c.build(c, c.defaultProps.merged(overrides.isNull() ? Json(Json::Object{}) : overrides));
    else if (overrides.size()) fprintf(stderr, "note: '%s' takes no props; --props ignored\n", c.name.c_str());
    if (c.w <= 0 || c.h <= 0 || c.fps <= 0 || c.duration <= 0)
        throw std::runtime_error("composition '" + c.name + "' needs positive size, fps and duration");
}

void usage() {
    puts("usage:");
    puts("  raymotion list                                  list compositions");
    puts("  raymotion preview <name> [--mute]               play with sound in a live window (esc quits)");
    puts("  raymotion render  <name> [--crf N] [--draft] [--mute] [--png-frames]");
    puts("                        render every frame via raylib, pipe to ffmpeg -> mp4");
    puts("                        --png-frames writes a PNG sequence instead (no ffmpeg)");
    puts("  raymotion still   <name> --at FRAME [--png PATH] write one PNG frame");
    puts("  raymotion props   <name>                        print the default props (JSON)");
    puts("options: --ffmpeg PATH overrides auto-detected ffmpeg binary");
    puts("         --ss N        supersampling 1, 2 or 4 (default: the composition's)");
    puts("         --props JSON  input props merged over the defaults; @file.json reads a file");
}

int runMain(int argc, const char** argv, const RegisterFn& registerAll) {
    const char* mode = argc > 1 ? argv[1] : "";
    if (!strcmp(mode, "list")) {
        Registry comps; registerAll(comps);
        for (auto& c : comps) {
            prepare(c, Json());
            printf("%-22s %dx%d @%dfps  %.1fs  audio=%d\n", c.name.c_str(), c.w, c.h, c.fps,
                   (double)c.duration / c.fps, (int)media::hasAudio(c));
        }
        return 0;
    }
    if (!strcmp(mode, "help") || !mode[0]) { usage(); return 1; }

    if (argc < 3) { usage(); return 1; }
    const std::string want = argv[2];

    const char* crfArg = nullptr, *ffArg = nullptr, *atArg = nullptr, *pngArg = nullptr, *ssArg = nullptr, *propsArg = nullptr;
    bool draft = false, noMusic = false, pngFrames = false;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--crf") && i+1 < argc) crfArg = argv[++i];
        else if (!strcmp(argv[i], "--ffmpeg") && i+1 < argc) ffArg = argv[++i];
        else if (!strcmp(argv[i], "--at") && i+1 < argc) atArg = argv[++i];
        else if (!strcmp(argv[i], "--png") && i+1 < argc) pngArg = argv[++i];
        else if (!strcmp(argv[i], "--ss") && i+1 < argc) ssArg = argv[++i];
        else if (!strcmp(argv[i], "--props") && i+1 < argc) propsArg = argv[++i];
        else if (!strcmp(argv[i], "--draft")) draft = true;
        else if (!strcmp(argv[i], "--mute") || !strcmp(argv[i], "--no-music")) noMusic = true;
        else if (!strcmp(argv[i], "--png-frames")) pngFrames = true;
    }

    Registry comps; registerAll(comps);
    Comp* comp = nullptr;
    for (auto& c : comps) if (c.name == want) comp = &c;
    if (!comp) { printf("unknown composition '%s'\n", want.c_str()); usage(); return 1; }
    if (!strcmp(mode, "props")) { puts(comp->defaultProps.dump().c_str()); return 0; }
    prepare(*comp, propsArg ? loadProps(propsArg) : Json());
    if (ffArg) media::setFfmpeg(ffArg);

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
        bool ok = saveFrame(canvas,W,H,path);
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

    // ---- render: frame-by-frame capture piped into ffmpeg ----------------------
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
        std::string wav = "", mp4 = "out/" + comp->name + (draft ? "_draft.mp4" : ".mp4");
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
            cmd += " -c:v h264 -pix_fmt yuv420p -crf " + std::to_string(crf) + " -movflags +faststart";
            if (!wav.empty()) cmd += " -c:a aac -b:a 192k -shortest";
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
        double t0 = GetTime();
        bool ok = true;
        for (int f = 0; f < comp->duration && ok; f++) {
            BeginDrawing();
            drawFrame(*comp, f,canvas);
            EndDrawing();                 // flush before reading the canvas
            if (pngFrames) {
                char fname[160]; snprintf(fname, sizeof fname, "%s/f%05d.png", dir.c_str(), f);
                ok = saveFrame(canvas,W,H,fname);
            } else {
                Image image = grabFrame(canvas,W,H);
                size_t bytes = (size_t)W*H*4;
                ok = image.data && fwrite(image.data, 1, bytes, pipe) == bytes;
                UnloadImage(image);
                if (!ok) fprintf(stderr, "ffmpeg stopped accepting frames at %d\n", f);
            }
            if (f % 15 == 0 || f+1 == comp->duration)
                printf("[%4d/%4d]\n", f, comp->duration);
        }
        double dt = GetTime() - t0;
        closeCanvas(canvas);
        if (pngFrames) {
            if (!ok) return 2;
            printf("rendered %d frames in %.1fs -> %s/\n", comp->duration, dt, dir.c_str());
            return 0;
        }
        int rc = pclose(pipe);
        printf("rendered %d frames in %.1fs -> %s (rc=%d)\n", comp->duration, dt, mp4.c_str(), rc);
        return ok && rc == 0 ? 0 : 3;
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
