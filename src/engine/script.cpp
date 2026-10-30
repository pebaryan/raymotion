#include "rmtn/script.hpp"
#include <filesystem>
#include <map>
#include <stdexcept>
#include <cctype>
#include <limits>

namespace rmtn::script {
#ifdef RMTN_ANGELSCRIPT
std::shared_ptr<Program> loadAngelScript(const std::string& path);
#endif
namespace {
std::string normalized(std::string extension) {
    for (char& ch : extension) ch = (char)std::tolower((unsigned char)ch);
    return extension;
}
std::map<std::string, Loader>& backends() {
    static std::map<std::string, Loader> value = {
#ifdef RMTN_ANGELSCRIPT
        {".as", loadAngelScript}
#endif
    };
    return value;
}
void validate(const Description& d) {
    if (d.name.empty() || d.name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
        throw std::runtime_error("script composition name must contain only letters, digits, '_' or '-'");
    if (d.w < 2 || d.h < 2 || d.w > 16384 || d.h > 16384 || d.fps < 1 || d.fps > 1000 || d.duration < 1)
        throw std::runtime_error("invalid script composition dimensions, fps or duration");
    if (d.supersample != 1 && d.supersample != 2 && d.supersample != 4)
        throw std::runtime_error("script supersample must be 1, 2 or 4");
    for (const auto& s : d.scenes)
        if (s.function.empty() || s.start < 0 || s.duration < 1 || (long long)s.start + s.duration > d.duration)
            throw std::runtime_error("script scene must lie inside the composition timeline");
}
void apply(Comp& c, const Description& d, const std::shared_ptr<Program>& program) {
    validate(d);
    c.name = d.name; c.w = d.w; c.h = d.h; c.fps = d.fps;
    c.duration = d.duration; c.supersample = d.supersample; c.background = mkColor(d.background);
    c.roots.clear();
    auto scenes = d.scenes;
    if (scenes.empty()) scenes.push_back({0, d.duration, "draw"});
    for (const auto& scene : scenes) {
        auto function = scene.function;
        addRoot(c.roots, scene.start, scene.duration)->fn = [program, function](const Ctx& ctx, int t) {
            program->draw(function, ctx, t);
        };
    }
}
}
void registerBackend(const std::string& extension, Loader loader) {
    auto key = normalized(extension);
    if (key.size() < 2 || key.front() != '.' || !loader)
        throw std::runtime_error("script backend needs an extension and a loader");
    if (!backends().emplace(key, std::move(loader)).second)
        throw std::runtime_error("script backend already registered: " + key);
}
void registerFile(Registry& registry, const std::string& path) {
    auto ext = normalized(std::filesystem::path(path).extension().string());
    auto it = backends().find(ext);
    if (it == backends().end()) throw std::runtime_error("no script backend for '" + ext + "' (enable RAYMOTION_SCRIPTING for .as)");
    auto program = it->second(std::filesystem::absolute(path).string());
    if (!program) throw std::runtime_error("script backend returned no program: " + path);
    Comp c;
    apply(c, program->configure(Json()), program);
    for (const auto& existing : registry)
        if (existing.name == c.name) throw std::runtime_error("duplicate composition name: " + c.name);
    const auto name = c.name;
    c.build = [program, name](Comp& target, const Json& props) {
        auto description = program->configure(props);
        if (description.name != name) throw std::runtime_error("script props must not change composition name");
        apply(target, description, program);
    };
    registry.push_back(std::move(c));
}
} // namespace rmtn::script
