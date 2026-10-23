// main.cpp -- raymotion CLI: list | preview | render | still
#include <raylib.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include "rmtn/timeline.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/synth.hpp"
#include "rmtn/preview.hpp"

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

using namespace rmtn;

// FX-tier renderer implemented in fx_present.cpp
namespace rmtn {
void presentFrame(Comp& c, int frame, RenderTexture output);
void releaseFrameResources();
}

static void drawFrame(Comp& c,int frame,RenderTexture canvas){
    presentFrame(c,frame,canvas);
    ClearBackground(BLACK);
    DrawTexturePro(canvas.texture,{0,0,(float)c.w,-(float)c.h},
                   fitCanvas(c.w,c.h,GetScreenWidth(),GetScreenHeight()), {0,0},0,WHITE);
}

static void closeCanvas(RenderTexture canvas){
    UnloadRenderTexture(canvas);
    releaseFrameResources();
    CloseWindow();
}

// Canvas pixels top-down, resized to the output size. Caller unloads.
static Image grabFrame(RenderTexture canvas,int W,int H){
    Image image=LoadImageFromTexture(canvas.texture);
    if (!image.data) return image;
    ImageFlipVertical(&image);
    if (image.width!=W || image.height!=H) ImageResize(&image,W,H);
    return image;
}

static bool saveFrame(RenderTexture canvas,int W,int H,const std::string& path){
    Image image=grabFrame(canvas,W,H);
    if (!image.data) return false;
    bool ok=ExportImage(image,path.c_str());
    UnloadImage(image);
    if (!ok) fprintf(stderr,"cannot write frame: %s\n",path.c_str());
    return ok;
}

static std::string quote(const std::string& s) { return "\"" + s + "\""; }

// cmd.exe strips the first and last quote of a command line that holds several quoted
// tokens, so on Windows the whole line gets one more enclosing pair.
static std::string shellLine(const std::string& cmd) {
#ifdef _WIN32
    return "\"" + cmd + "\"";
#else
    return cmd;
#endif
}

static bool ffmpegWorks(const std::string& exe) {
#ifdef _WIN32
    const char* devnull = "NUL";
#else
    const char* devnull = "/dev/null";
#endif
    std::string probe = quote(exe) + " -version >" + devnull + " 2>&1";
    return std::system(shellLine(probe).c_str()) == 0;
}

static std::string ffmpegPath(const char* forced) {
    if (forced && *forced) return forced;
    namespace fs = std::filesystem;
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

static void usage() {
    puts("usage:");
    puts("  raymotion list                                  list compositions");
    puts("  raymotion preview <name>                        play in a live window (esc quits)");
    puts("  raymotion render  <name> [--crf N] [--draft] [--no-music] [--png-frames]");
    puts("                        render every frame via raylib, pipe to ffmpeg -> mp4");
    puts("                        --png-frames writes a PNG sequence instead (no ffmpeg)");
    puts("  raymotion still   <name> --at FRAME [--png PATH] write one PNG frame");
    puts("options: --ffmpeg PATH overrides auto-detected ffmpeg binary");
}

static int runMain(int argc, const char** argv) {
    const char* mode = argc > 1 ? argv[1] : "";
    if (!strcmp(mode, "list")) {
        std::vector<Comp> comps; register_scenes(comps);
        for (auto& c : comps)
            printf("%-22s %dx%d @%dfps  %.1fs  music=%d\n", c.name.c_str(), c.w, c.h, c.fps,
                   (double)c.duration / c.fps, (int)c.music);
        return 0;
    }
    if (!strcmp(mode, "help") || !mode[0]) { usage(); return 1; }

    if (argc < 3) { usage(); return 1; }
    const std::string want = argv[2];

    const char* crfArg = nullptr, *ffArg = nullptr, *atArg = nullptr, *pngArg = nullptr;
    bool draft = false, noMusic = false, pngFrames = false;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--crf") && i+1 < argc) crfArg = argv[++i];
        else if (!strcmp(argv[i], "--ffmpeg") && i+1 < argc) ffArg = argv[++i];
        else if (!strcmp(argv[i], "--at") && i+1 < argc) atArg = argv[++i];
        else if (!strcmp(argv[i], "--png") && i+1 < argc) pngArg = argv[++i];
        else if (!strcmp(argv[i], "--draft")) draft = true;
        else if (!strcmp(argv[i], "--no-music")) noMusic = true;
        else if (!strcmp(argv[i], "--png-frames")) pngFrames = true;
    }

    std::vector<Comp> comps; register_scenes(comps);
    Comp* comp = nullptr;
    for (auto& c : comps) if (c.name == want) comp = &c;
    if (!comp) { printf("unknown composition '%s'\n", want.c_str()); usage(); return 1; }

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
        RenderTexture canvas=LoadRenderTexture(comp->w,comp->h);
        if (!IsRenderTextureValid(canvas)){ CloseWindow(); return 2; }
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
        RenderTexture canvas=LoadRenderTexture(comp->w,comp->h);
        if (!IsRenderTextureValid(canvas)){ CloseWindow(); return 2; }
        SetTargetFPS(60);
        double playhead = 0, speed = 1.0;
        bool paused = false;
        while (!WindowShouldClose()) {   // esc (raylib's exit key) or the window's close button
            BeginDrawing();
            drawFrame(*comp, (int)playhead % comp->duration,canvas);
            char hud[96]; snprintf(hud, sizeof hud, "[%04d/%04d] x%.2f [%s]",
                (int)playhead % comp->duration, comp->duration, speed, paused ? "PAUSED" : "PLAY");
            DrawText(hud, 12, 12, 18, Color{ (unsigned char)240, (unsigned char)240, (unsigned char)255, (unsigned char)180 });
            EndDrawing();
            if (!paused) playhead += comp->fps * GetFrameTime() * speed;
            if (IsKeyDown(KEY_RIGHT)) speed = 2.0; else if (IsKeyDown(KEY_LEFT)) speed = 0.25; else speed = 1.0;
            int kp = GetKeyPressed();
            if (kp == KEY_SPACE) paused = !paused;   // rare spurious repeats; acceptable for a preview
        }
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
            ff = ffmpegPath(ffArg);
            if (ff.empty()) { puts("ffmpeg not found (use --ffmpeg PATH, or --png-frames to skip encoding)"); return 2; }
        }
        std::string wav = "", mp4 = "out/" + comp->name + (draft ? "_draft.mp4" : ".mp4");
        if (comp->music && !noMusic && !draft && !pngFrames) {
            wav = dir + "/track.wav";
            double total = (double)comp->duration / comp->fps;
            auto samples = renderSong(comp->song, total + 0.3, 22050);
            if (!writeWavMono(wav, 22050, samples)) { puts("wav write failed"); wav.clear(); }
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
            if (!wav.empty()) cmd += " -c:a aac -b:a 160k -shortest";
            cmd += " " + quote(mp4);
            if (getenv("RAYMOTION_DEBUG_CMD")) printf("[CMD] %s\n", cmd.c_str());
            pipe = popen(shellLine(cmd).c_str(), "wb");
            if (!pipe) { fprintf(stderr, "cannot start ffmpeg: %s\n", ff.c_str()); return 3; }
        }

        InitWindow(W, H, ("raymotion render - " + comp->name).c_str());
        RenderTexture canvas=LoadRenderTexture(comp->w,comp->h);
        if (!IsRenderTextureValid(canvas)){ if (pipe) pclose(pipe); CloseWindow(); return 2; }
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


int main(int argc,const char** argv){
    try { return runMain(argc,argv); }
    catch (const std::exception& error){
        fprintf(stderr,"raymotion: %s\n",error.what());
        return 2;
    }
}
