// Runtime-neutral scripting contract. Backends own their VM and expose timed draw functions.
#pragma once
#include "timeline.hpp"

namespace rmtn::script {
struct Scene {
    int start = 0, duration = 0;
    std::string function;
};
struct Description {
    std::string name;
    int w = 1920, h = 1080, fps = 30, duration = 0, supersample = 2;
    unsigned int background = 0x04040a;
    std::vector<Scene> scenes;
};
class Program {
public:
    virtual ~Program() = default;
    // CPU only: called during registration and again with merged input props.
    virtual Description configure(const Json& props) = 0;
    // Called only inside the renderer. Frame is local to the scene.
    virtual void draw(const std::string& function, const Ctx& ctx, int frame) = 0;
};
using Loader = std::function<std::shared_ptr<Program>(const std::string& path)>;
// Register once at application startup. Extension includes the dot; duplicate registration throws.
void registerBackend(const std::string& extension, Loader loader);
// Loads one composition; callbacks retain the program for their entire lifetime.
// Built-in .as backend is optional at build time. Failures throw with source diagnostics.
void registerFile(Registry& registry, const std::string& path);
} // namespace rmtn::script
