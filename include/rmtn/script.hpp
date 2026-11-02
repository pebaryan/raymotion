// Runtime-neutral scripting contract. Backends own their VM and expose timed draw functions.
#pragma once
#include "timeline.hpp"
#include "camera.hpp"

namespace rmtn::script {
struct Scene {
    int start = 0, duration = 0;
    std::string function;
    int parent = -1; // scene index; child timing is local to its parent
    int loopEvery = 0, freezeAt = -1;
    std::string updateFunction;
    bool isolated = false, invertMask = false;
    float layerOpacity = 1, layerOffsetX = 0, layerOffsetY = 0, layerShutter = 0.5f;
    Blend layerBlend = Blend::Normal;
    MaskMode layerMaskMode = MaskMode::Alpha;
    int layerBlurSamples = 1;
    std::string maskFunction;
    std::string layerUpdateFunction;
    std::vector<Filter> filters;
    Json drawProps;
    bool hasDrawProps = false;
};
struct MediaCue {
    enum class Kind { Video, Audio };
    Kind kind = Kind::Audio;
    int start = 0, duration = 0;
    int parentScene = -1; // scene index; cue timing is local to this parent
    std::string file;
    float x = 0, y = 0, w = 0, h = 0;
    std::string fit = "cover";
    float alignX = 0.5f, alignY = 0.5f;
    unsigned int tint = 0xffffff;
    double startFrom = 0, endAt = 0, playbackRate = 1;
    float volume = 1;
    std::vector<float> volumeKeys; // frame, gain multiplier pairs, relative to cue start
    std::string volumeFunction; // global float fn(int localFrame), called while mixing audio
    bool loop = false, muted = false;
};
struct CameraWorldCue {
    cam::Track track;
    std::string function;
    float parallax = 1;
};
struct PrefabCue {
    int start = 0, duration = 0;
    std::string name;
    Json props;
    int parentScene = -1; // scene index; cue timing is local to this parent
};
struct PrefabDefinition {
    std::string name, function;
    Json defaultProps;
};
struct VoiceCue {
    int start = 0;
    float volume = 1;
    double gap = 0.5, lead = 0.3, wordsPerSecond = 2.5;
    bool captions = true;
    std::string command, cacheDir = "out/tts";
    std::vector<std::string> lines;
    int parentScene = -1; // scene index; cue timing is local to this parent
};
struct TransitionCue {
    enum class Kind { Fade, Slide, Wipe };
    int fromScene = 0, toScene = 0, frames = 15;
    Kind kind = Kind::Fade;
    int direction = 1, easing = E_IN_OUT_CUBIC;
    std::string easingFunction;
};
struct Description {
    std::string name;
    Json defaultProps;
    std::string renderFunction;
    int w = 1920, h = 1080, fps = 30, duration = 0, supersample = 2;
    int statementBudget = 1000000;
    int motionBlurSamples = 1;
    float motionBlurShutter = 0.5f;
    unsigned int background = 0x04040a;
    int backgroundAlpha = 255;
    bool music = false;
    Song song;
    std::vector<Scene> scenes;
    std::vector<MediaCue> media;
    std::vector<CameraWorldCue> cameraWorlds;
    std::vector<PrefabCue> prefabs;
    std::vector<PrefabDefinition> scriptPrefabs;
    std::vector<VoiceCue> voiceovers;
    std::vector<TransitionCue> transitions;
};
class Program {
public:
    virtual ~Program() = default;
    // CPU only: called during registration and again with merged input props.
    virtual Description configure(const Json& props) = 0;
    // Called only inside the renderer. Frame is local to the scene.
    virtual void draw(const std::string& function, const Ctx& ctx, int frame,
                      const Json& props, int statementBudget) = 0;
    virtual void updateLayer(const std::string&, int, NodeProps&, Layer&, const Json&, int) {}
    virtual void updateScene(const std::string&, int, NodeProps&, const Json&, int) {}
    // Optional scalar callback for timeline media; frame is local to the media cue.
    virtual float evaluateFloat(const std::string&, int, const Json&, int) { throw std::runtime_error("script program does not support scalar callbacks"); }
    virtual double evaluateScalar(const std::string&, double, const Json&, int) { throw std::runtime_error("script program does not support double scalar callbacks"); }
    virtual cam::Shot evaluateShot(const std::string&, int, const Json&, int) { throw std::runtime_error("script program does not support camera callbacks"); }
    // Optional generated-asset hook, called only for frame rendering and before build.
    virtual bool hasAssetHook() const { return false; }
    virtual void generateAssets(const Json&) {}
};
using Loader = std::function<std::shared_ptr<Program>(const std::string& path)>;
// Register once at application startup. Extension includes the dot; duplicate registration throws.
void registerBackend(const std::string& extension, Loader loader);
// Loads one composition; callbacks retain the program for their entire lifetime.
// Built-in .as backend is optional at build time. Failures throw with source diagnostics.
void registerFile(Registry& registry, const std::string& path);
} // namespace rmtn::script
