// weather_prefabs.cpp -- see rmtn/weather_prefabs.hpp. The presets, the rain, water and lightning
// helpers, and the four prefabs ("weather", "water-body", "rain", "lightning"). All of it sets the
// engine's own structs; frames stay pure functions of t (every `up` assigns absolute values).
#include "rmtn/weather_prefabs.hpp"
#include "rmtn/flora.hpp"
#include "rmtn/media.hpp"
#include "rmtn/prefab.hpp"
#include "rmtn/draw.hpp"

#include <raymath.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace rmtn {
namespace wx {
namespace {

float lerpf(float a, float b, float t) { return a + (b - a) * t; }
float smoothstepf(float e0, float e1, float x) {
    const float u = std::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
    return u * u * (3 - 2 * u);
}
Color mixc(Color a, Color b, float t) {
    auto m = [t](unsigned char x, unsigned char y) { return (unsigned char)std::lround(x + (y - x) * t); };
    return { m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), 255 };
}
std::string normalised(const std::string& s) {
    std::string out;
    for (char c : s) out += (c == '-' || c == '_') ? ' ' : (char)std::tolower((unsigned char)c);
    return out;
}

// ---------------------------------------------------------------- the presets
Look make(const char* name) {
    using g3d::Weather;
    using Q = g3d::Volumetric::Quality;
    const std::string n = name;
    Look k;
    if (n == "clear") {
        k.weather = Weather::clear();
        k.weather.clouds.cover = 0.08f;
        k.weather.wetness = 0;  k.weather.snowCover = 0;
        k.hour = 11;  k.cloudBase = 1800;  k.cloudTop = 3000;  k.cloudDensity = 0.8f;
        k.aerial = 1;  k.viewFromSun = 70;
    } else if (n == "cumulus day") {
        k.weather = Weather::clear();
        k.weather.clouds.cover = 0.45f;
        k.weather.wind.speed = 5;
        k.weather.haze = 0.08f;
        k.weather.wetness = 0;  k.weather.snowCover = 0;
        k.hour = 13.5f;  k.cloudBase = 1400;  k.cloudTop = 3200;
        k.aerial = 1;  k.viewFromSun = 80;
    } else if (n == "overcast") {
        k.weather = Weather::overcast();
        k.weather.haze = 0.2f;
        k.weather.wetness = 0.1f;  k.weather.snowCover = 0;
        k.hour = 12;  k.cloudBase = 700;  k.cloudTop = 1500;  k.cloudDensity = 1.1f;
        k.aerial = 0.7f;  k.sunIntensity = 2;  k.viewFromSun = 90;
    } else if (n == "squall") {
        k.weather = Weather::rain();
        k.weather.precipitation.intensity = 0.9f;
        k.weather.wind.speed = 13;  k.weather.wind.gust = 0.75f;
        k.weather.clouds.cover = 0.98f;
        k.weather.haze = 0.4f;
        k.weather.wetness = 0.75f;  k.weather.snowCover = 0;
        k.hour = 15;  k.cloudBase = 450;  k.cloudTop = 1800;  k.cloudDensity = 1.6f;
        k.aerial = 0.5f;  k.sunIntensity = 2;  k.viewFromSun = 90;
    } else if (n == "thunderstorm") {
        k.weather = Weather::storm();
        k.weather.wind.speed = 16;
        k.weather.clouds.cover = 0.62f;                 // open sky between the cells, so a tower and its anvil stand against it
        k.weather.wetness = 0.9f;  k.weather.snowCover = 0;
        k.hour = 15;  k.cloudBase = 600;  k.cloudTop = 11000;  k.cloudDensity = 3;  k.gloom = 0.6f;
        k.aerial = 0.5f;  k.sunIntensity = 1.4f;  k.ambient = 0.45f;  k.viewFromSun = 140;  k.shafts = Q::Low;      // the sun behind the camera: the towers are lit from the front, the base is dark
    } else if (n == "snowfall") {
        k.weather = Weather::snow();
        k.weather.precipitation.intensity = 0.6f;
        k.weather.clouds.cover = 0.92f;
        k.weather.wetness = 0;  k.weather.snowCover = 1;
        k.hour = 10.5f;  k.cloudBase = 600;  k.cloudTop = 1400;  k.cloudDensity = 0.8f;
        k.aerial = 0.5f;  k.sunIntensity = 2.6f;  k.ambient = 0.95f;  k.viewFromSun = 90;
    } else if (n == "fog dawn") {
        k.weather = Weather::fog();
        k.weather.haze = 0.8f;
        k.weather.clouds.cover = 0.5f;
        k.weather.wetness = 0.2f;  k.weather.snowCover = 0;
        k.hour = 6.3f;  k.cloudBase = 900;  k.cloudTop = 1300;  k.cloudDensity = 0.6f;
        k.shafts = Q::Medium;  k.sunIntensity = 2.6f;  k.viewFromSun = 25;
    } else if (n == "golden evening") {
        k.weather = Weather::clear();
        k.weather.clouds.cover = 0.35f;
        k.weather.haze = 0.12f;
        k.weather.wetness = 0.3f;  k.weather.snowCover = 0;
        k.weather.rainbow.distantRain = 0.55f;          // the rain that has passed stands in the east
        k.hour = 17.6f;  k.cloudBase = 1500;  k.cloudTop = 3000;
        k.shafts = Q::Medium;  k.aerial = 1.3f;  k.sunIntensity = 2.8f;  k.viewFromSun = 160;
    } else {
        throw std::invalid_argument("");
    }
    k.weather.enabled = true;
    return k;
}

std::string choices() {
    std::string s;
    for (const auto& n : presetNames()) s += (s.empty() ? "" : ", ") + n;
    return s;
}

}  // namespace

const std::vector<std::string>& presetNames() {
    static const std::vector<std::string> names = { "clear", "cumulus day", "overcast", "squall", "thunderstorm",
                                                     "snowfall", "fog dawn", "golden evening" };
    return names;
}

Look look(const std::string& name) {
    try { return make(normalised(name).c_str()); }
    catch (const std::invalid_argument&) {
        throw std::invalid_argument("no weather preset \"" + name + "\" (choose " + choices() + ")");
    }
}

Look mix(const Look& a, const Look& b, float amount) {
    using g3d::Precipitation;
    const float t = std::clamp(amount, 0.f, 1.f);
    Look k;
    k.weather = g3d::Weather::mix(a.weather, b.weather, t);
    if (k.weather.precipitation.kind == Precipitation::None) {
        // one side falls and the other does not: the one that falls is used all through, from intensity 0
        k.weather.precipitation.kind = a.weather.precipitation.kind != Precipitation::None ? a.weather.precipitation.kind
                                                                                          : b.weather.precipitation.kind;
    } else if (a.weather.precipitation.kind == Precipitation::None && b.weather.precipitation.kind != Precipitation::None) {
        k.weather.precipitation.kind = b.weather.precipitation.kind;
    } else if (b.weather.precipitation.kind == Precipitation::None && a.weather.precipitation.kind != Precipitation::None) {
        k.weather.precipitation.kind = a.weather.precipitation.kind;
    }
    k.hour = lerpf(a.hour, b.hour, t);
    k.gloom = lerpf(a.gloom, b.gloom, t);
    k.cloudBase = lerpf(a.cloudBase, b.cloudBase, t);
    k.cloudTop = lerpf(a.cloudTop, b.cloudTop, t);
    k.cloudDensity = lerpf(a.cloudDensity, b.cloudDensity, t);
    k.shafts = (g3d::Volumetric::Quality)std::max((int)a.shafts, (int)b.shafts);
    k.aerial = lerpf(a.aerial, b.aerial, t);
    k.sunIntensity = lerpf(a.sunIntensity, b.sunIntensity, t);
    k.ambient = a.ambient >= 0 && b.ambient >= 0 ? lerpf(a.ambient, b.ambient, t) : (t < 0.5f ? a.ambient : b.ambient);
    k.viewFromSun = lerpf(a.viewFromSun, b.viewFromSun, t);
    return k;
}

void apply(g3d::Scene& s, const Look& k, float fps) {
    s.weather = k.weather;
    s.weather.enabled = true;
    s.weather.fps = fps;

    g3d::Clouds& c = s.clouds;
    c.enabled = true;
    c.ownWeather = false;
    c.base = k.cloudBase;
    c.top = std::max(k.cloudTop, k.cloudBase + 100);
    c.density = k.cloudDensity;
    c.fps = fps;
    if (k.ambient >= 0) s.ambient = k.ambient;

    const bool falls = k.weather.precipitation.kind != g3d::Precipitation::None && k.weather.precipitation.intensity > 0.001f;
    s.precipitation.enabled = falls;
    s.precipitation.ownWeather = false;
    s.precipitation.fps = fps;
    s.ground.ownWeather = false;

    if (k.weather.lightningRate > 0.001f) s.lightning.enabled = true;       // never switched off: a schedule may be in use
    s.lightning.fps = fps;

    s.volumetric.quality = k.shafts;
    s.aerial.enabled = k.aerial > 0.001f;
    s.aerial.strength = k.aerial;

    g3d::Sky sky = g3d::Sky::at(k.hour);
    sky.enabled = true;
    sky.show = true;
    sky.seed = s.sky.seed;
    sky.resolution = s.sky.resolution;
    if (k.gloom > 0.001f) {                                  // a heavy sky: the colours go slate and dim, which the clouds' ambient light reads
        auto slate = [&](Color c) { return Color{ (unsigned char)(lerpf(c.r, 70, 0.8f * k.gloom) * (1 - 0.3f * k.gloom)), (unsigned char)(lerpf(c.g, 80, 0.8f * k.gloom) * (1 - 0.3f * k.gloom)), (unsigned char)(lerpf(c.b, 86, 0.8f * k.gloom) * (1 - 0.3f * k.gloom)), c.a }; };
        sky.zenith = slate(sky.zenith);  sky.horizon = slate(sky.horizon);  sky.ground = slate(sky.ground);  sky.haze = slate(sky.haze);
    }
    if (!s.lights.empty() && s.lights[0].type == g3d::Light::Type::Directional) {
        // the key light is the sun: it shines from where the sky's sun is, and never from under the ground
        Vector3 d = Vector3Normalize(sky.sunDirection);
        d.y = std::max(d.y, 0.05f);
        d = Vector3Normalize(d);
        g3d::Light& sun = s.lights[0];
        sun.position = Vector3Add(sun.target, Vector3Scale(d, 40));
        sun.color = sky.sunColor;
        sun.intensity = k.sunIntensity;
        sky.sunLight = 0;
    }
    s.sky = sky;
}

void applyPreset(g3d::Scene& scene, const std::string& name, float fps) { apply(scene, look(name), fps); }

void applyBlend(g3d::Scene& scene, const std::string& a, const std::string& b, float amount, float fps) {
    apply(scene, mix(look(a), look(b), amount), fps);
}

// ---------------------------------------------------------------- water bodies
std::vector<Vector2> shoreOutline(Vector2 centre, Vector2 radius, uint32_t seed) {
    const int n = 24;
    const float R = std::max(0.5f * (radius.x + radius.y), 0.1f);
    std::vector<Vector2> ring;
    for (int i = 0; i < n; i++) {
        const float a = 2 * PI * (float)i / n;
        const float wob = 1 + 0.25f * (2 * flora::noise(std::cos(a) * R + 100, std::sin(a) * R + 100, R * 0.9f, seed) - 1);
        ring.push_back({ centre.x + std::cos(a) * radius.x * wob, centre.y + std::sin(a) * radius.y * wob });
    }
    return ring;
}

g3d::WaterBody waterBody(const WaterSpec& spec) {
    g3d::WaterBody b;
    const bool river = spec.path.size() >= 2;
    if (river) b = g3d::WaterBody::river(spec.path, spec.width, spec.level, spec.flow);
    else b = g3d::WaterBody::lake(spec.shore.empty() ? shoreOutline(spec.centre, spec.radius, spec.seed) : spec.shore, spec.level);
    g3d::Water& w = b.look;
    switch (spec.look) {
    case Surface::Calm: break;
    case Surface::Breezy:
        for (auto& wave : w.waves) wave.amplitude *= 2.4f;
        w.foam = 0.25f;
        b.ripple = 0.26f;
        break;
    case Surface::Choppy:
        if (!river) { const float keepFetch = w.fetch; w = g3d::Water::choppy(); w.fetch = keepFetch; }
        w.foam = 0.5f;
        b.ripple = 0.4f;
        break;
    }
    if (!spec.reflections) {
        w.reflections = g3d::Water::Reflections::Off;
        w.refraction = false;
    }
    w.enabled = true;
    b.level = spec.level;
    return b;
}

// ---------------------------------------------------------------- rain
float rainIntensity(const RainSpec& spec, double seconds) {
    const float u = (float)((seconds - spec.delay) / std::max(spec.ramp, 1e-3f));
    return spec.intensity * smoothstepf(0, 1, u);
}

void addRain(g3d::Scene& s, const RainSpec& spec) {
    g3d::Weather w = g3d::Weather::rain();
    w.fps = spec.fps;
    w.wind.speed = spec.windSpeed;
    w.wind.direction = spec.windDirection;
    w.clouds.cover = spec.cover;
    w.precipitation.kind = g3d::Precipitation::Rain;
    w.precipitation.intensity = spec.intensity;
    w.wetness = -1;
    w.accumulationScale = spec.wetRate;
    w.history = { { 0, g3d::Precipitation::Rain, 0 },
                  { spec.delay, g3d::Precipitation::Rain, 0 },
                  { spec.delay + std::max(spec.ramp, 1e-3f), g3d::Precipitation::Rain, spec.intensity } };
    s.weather = w;
    s.precipitation.enabled = true;
    s.precipitation.ownWeather = false;
    s.precipitation.seed = spec.seed;
    s.precipitation.fps = spec.fps;
    s.ground.ownWeather = false;
    s.ground.seed = spec.seed;
    auto before = s.up;
    s.up = [before, spec](int t, g3d::Scene& self) {
        if (before) before(t, self);
        self.weather.precipitation.intensity = rainIntensity(spec, frameTime(t) / std::max(spec.fps, 1.f));
    };
}

fx::Lens rainLens(const RainSpec& spec) {
    fx::Lens l;
    if (spec.lens <= 0) return l;
    l.rain.rateAt = [spec](double seconds) {
        return std::clamp(spec.lens * rainIntensity(spec, seconds) / std::max(spec.intensity, 0.05f), 0.f, 1.f);
    };
    l.rain.blurBehind = 6 * spec.lens;
    l.rain.seed = (int)spec.seed;
    return l;
}

// ---------------------------------------------------------------- lightning
std::vector<g3d::Lightning::Strike> strikeSchedule(const StrikeSpec& spec) {
    std::vector<g3d::Lightning::Strike> out;
    const int n = std::clamp(spec.count, 0, 512);
    const unsigned long long seed = spec.seed;
    const float slot = spec.span / (float)std::max(n, 1);
    for (int i = 0; i < n; i++) {
        const unsigned long long k = (unsigned long long)i * 8;
        auto u = [&](int j) { return (float)rnd01(seed, k + (unsigned long long)j); };
        g3d::Lightning::Strike s;
        s.time = spec.start + slot * ((float)i + 0.1f + 0.8f * u(1));
        const float r = spec.radius * std::sqrt(u(2));
        const float a = 2 * PI * u(3);
        const float x = spec.area.x + r * std::cos(a), z = spec.area.y + r * std::sin(a);
        const bool ground = u(4) < spec.groundShare;
        const float top = spec.cloudBase + 300 + 700 * u(5);
        const float bearing = 2 * PI * u(6), reach = 60 + 340 * u(7);
        s.start = { x, top, z };
        if (ground) s.end = { x + 0.3f * reach * std::cos(bearing), spec.groundY, z + 0.3f * reach * std::sin(bearing) };
        else s.end = { x + reach * std::cos(bearing), top - 250 - 150 * u(7), z + reach * std::sin(bearing) };    // stays in the cloud
        s.seed = (uint32_t)(spec.seed * 1000003u + (uint32_t)i * 7919u + 11u) | 1u;
        s.intensity = 0.8f + 0.4f * u(0);
        out.push_back(s);
    }
    return out;
}

void addLightning(g3d::Scene& s, const StrikeSpec& spec) {
    s.lightning.enabled = true;
    s.lightning.strikes = strikeSchedule(spec);
    s.lightning.rate = 0;
    s.lightning.followClouds = false;
    s.lightning.groundY = spec.groundY;
    s.lightning.cloudBase = spec.cloudBase;
    s.lightning.area = spec.area;
    s.lightning.radius = spec.radius;
}

std::vector<g3d::Thunder> thunderCues(const StrikeSpec& spec, Vector3 listener) {
    std::vector<g3d::Thunder> out;
    for (const auto& s : strikeSchedule(spec)) {
        const Vector3 mid = Vector3Scale(Vector3Add(s.start, s.end), 0.5f);
        const Vector3 points[3] = { s.start, mid, s.end };
        g3d::Thunder c;
        c.distance = 1e30f;
        for (const Vector3& p : points) {
            const float d = Vector3Distance(p, listener);
            if (d < c.distance) { c.distance = d; c.position = p; }
        }
        c.strikeTime = s.time;
        c.arrives = s.time + bolt::thunderDelay(c.distance);
        c.toGround = s.end.y <= spec.groundY + 50;
        c.seed = s.seed;
        out.push_back(c);
    }
    std::sort(out.begin(), out.end(), [](const g3d::Thunder& a, const g3d::Thunder& b) { return a.arrives < b.arrives; });
    return out;
}

// ---------------------------------------------------------------- the landscape the prefabs draw
namespace {

Rectangle boxOf(const Json& p, const char* name) {
    const Json& b = p["box"];
    if (!b.isArray() || b.size() != 4) throw std::runtime_error(std::string(name) + ": box must be [x, y, width, height]");
    const Rectangle r{ (float)b[0].asNumber(), (float)b[1].asNumber(), (float)b[2].asNumber(), (float)b[3].asNumber() };
    if (!(r.width > 0 && r.height > 0)) throw std::runtime_error(std::string(name) + ": box needs a positive size");
    return r;
}

Vector3 vec3(const Json& a, Vector3 fallback, const char* name, const char* key) {
    if (a.isNull()) return fallback;
    if (!a.isArray() || a.size() != 3) throw std::runtime_error(std::string(name) + ": " + key + " must be [x, y, z]");
    return { (float)a[(size_t)0].asNumber(), (float)a[(size_t)1].asNumber(), (float)a[(size_t)2].asNumber() };
}

std::vector<Vector2> points(const Json& a, const char* name, const char* key) {
    std::vector<Vector2> out;
    if (a.isNull()) return out;
    if (!a.isArray()) throw std::runtime_error(std::string(name) + ": " + key + " must be a list of [x, z] points");
    for (const Json& p : a.asArray()) {
        if (!p.isArray() || p.size() < 2) throw std::runtime_error(std::string(name) + ": " + key + " points are [x, z]");
        out.push_back({ (float)p[(size_t)0].asNumber(), (float)p[(size_t)1].asNumber() });
    }
    return out;
}

// The signed distance to a polygon, metres, negative inside.
float ringDistance(const std::vector<Vector2>& ring, float x, float z) {
    float best = 1e30f;
    bool inside = false;
    const size_t n = ring.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Vector2 a = ring[j], b = ring[i];
        if (((a.y > z) != (b.y > z)) && (x < (b.x - a.x) * (z - a.y) / (b.y - a.y) + a.x)) inside = !inside;
        const Vector2 ab{ b.x - a.x, b.y - a.y };
        const float len2 = ab.x * ab.x + ab.y * ab.y;
        const float u = len2 > 1e-9f ? std::clamp(((x - a.x) * ab.x + (z - a.y) * ab.y) / len2, 0.f, 1.f) : 0.f;
        best = std::min(best, std::hypot(x - (a.x + u * ab.x), z - (a.y + u * ab.y)));
    }
    return inside ? -best : best;
}
// The distance to a path's centre line less half its width: negative on the river.
float pathDistance(const std::vector<Vector2>& path, float halfWidth, float x, float z) {
    float best = 1e30f;
    for (size_t i = 0; i + 1 < path.size(); i++) {
        const Vector2 a = path[i], b = path[i + 1];
        const Vector2 ab{ b.x - a.x, b.y - a.y };
        const float len2 = ab.x * ab.x + ab.y * ab.y;
        const float u = len2 > 1e-9f ? std::clamp(((x - a.x) * ab.x + (z - a.y) * ab.y) / len2, 0.f, 1.f) : 0.f;
        best = std::min(best, std::hypot(x - (a.x + u * ab.x), z - (a.y + u * ab.y)));
    }
    return best - halfWidth;
}

struct Land {
    float size = 60;
    uint32_t seed = 1;
    float hills = 1.2f;
    std::vector<Vector2> shore;         // a lake or pool
    std::vector<Vector2> path;          // a river
    float riverWidth = 5;
    float depth = 1.6f, bank = 3;       // the basin's depth and the width over which its bank slopes
    int trees = 10;
    std::string grass = "meadow";
    Color ground{ 93, 107, 60, 255 };
    Vector3 keepClear{ 0, 0, 0 };       // no tree within 6 m of this (x, z)
    float fps = 30;
};

std::shared_ptr<g3d::Scene> buildLand(const Land& L) {
    auto s = std::make_shared<g3d::Scene>();
    s->ambient = 0.45f;
    s->shadowMapSize = 2048;
    s->camera.farPlane = 60000;
    s->sky.enabled = true;
    s->sky.resolution = 512;
    g3d::Light sun = g3d::Light::directional({ 30, 40, 30 }, { 255, 244, 224, 255 }, 2.4f, true);
    sun.shadowExtent = L.size * 0.8f;
    s->lights.push_back(sun);

    auto shore = std::make_shared<std::vector<Vector2>>(L.shore);
    auto path = std::make_shared<std::vector<Vector2>>(L.path);
    const bool basin = shore->size() >= 3 || path->size() >= 2;
    const float size = L.size, hills = L.hills, depth = L.depth, bank = std::max(L.bank, 0.1f), halfW = 0.5f * L.riverWidth;
    const uint32_t seed = L.seed;
    auto distance = [shore, path, halfW](float x, float z) {
        if (path->size() >= 2) return pathDistance(*path, halfW, x, z);
        return ringDistance(*shore, x, z);
    };
    auto height = [=](float x, float z) {
        const float edge = 1 - smoothstepf(size * 0.7f, size, std::max(std::fabs(x), std::fabs(z)));
        const float land = (0.3f + hills * flora::noise(x, z, 30, seed)) * edge;
        if (!basin) return land;
        const float s = smoothstepf(-bank, bank, distance(x, z));
        return lerpf(-depth, land, s);
    };

    g3d::Material ground;
    ground.color = L.ground;
    ground.roughness = 1;
    flora::Area land = flora::rect({ -size, -size }, { size, size });
    land.ground = height;
    s->add(flora::terrain(land, 180, 180), ground);
    // the ground out past the detailed square, to the horizon
    const float far = 20000;
    const float mid = (far + size) / 2;
    s->add(g3d::Geometry::plane(2 * far, far - size), ground)->position = { 0, 0, mid };
    s->add(g3d::Geometry::plane(2 * far, far - size), ground)->position = { 0, 0, -mid };
    s->add(g3d::Geometry::plane(far - size, 2 * size), ground)->position = { mid, 0, 0 };
    s->add(g3d::Geometry::plane(far - size, 2 * size), ground)->position = { -mid, 0, 0 };

    if (!L.grass.empty() && L.grass != "none") {
        g3d::Field g;
        try { g = flora::grass(L.grass); }
        catch (const std::invalid_argument&) { throw std::runtime_error("no grass \"" + L.grass + "\""); }
        g.fps = L.fps;
        flora::Area meadow = flora::rect({ -size * 0.7f, -size * 0.7f }, { size * 0.7f, size * 0.7f });
        meadow.ground = height;
        meadow.edge = 4;
        if (shore->size() >= 3) {
            // keep the grass off the bank and the bed
            std::vector<Vector2> grown = *shore;
            Vector2 c{ 0, 0 };
            for (const auto& q : grown) { c.x += q.x / grown.size(); c.y += q.y / grown.size(); }
            for (auto& q : grown) { const Vector2 d{ q.x - c.x, q.y - c.y }; const float len = std::max(std::hypot(d.x, d.y), 0.01f); q.x += d.x / len * bank * 0.5f; q.y += d.y / len * bank * 0.5f; }
            meadow.exclude.push_back(grown);
        }
        if (path->size() >= 2) meadow.strips.push_back({ *path, L.riverWidth + bank });
        flora::addGrass(*s, meadow, g, L.seed);
    }

    g3d::Material bark;
    bark.color = { 62, 44, 30, 255 };
    bark.roughness = 1;
    for (int i = 0, placed = 0; i < 4 * L.trees + 8 && placed < L.trees; i++) {
        const float a = 2 * PI * (float)rnd01(L.seed + 77, (unsigned long long)i * 3 + 1);
        const float r = 12 + (size * 0.85f - 12) * std::sqrt((float)rnd01(L.seed + 77, (unsigned long long)i * 3 + 2));
        const float x = r * std::cos(a), z = r * std::sin(a);
        if (std::hypot(x - L.keepClear.x, z - L.keepClear.z) < 6) continue;
        if (basin && distance(x, z) < bank + 2) continue;
        const float h = 5 + 5 * (float)rnd01(L.seed + 77, (unsigned long long)i * 3 + 3);
        const float y = height(x, z);
        g3d::Material leaf;
        const float v = (float)rnd01(L.seed + 78, (unsigned long long)i);
        leaf.color = mixc({ 34, 66, 40, 255 }, { 58, 92, 46, 255 }, v);
        leaf.roughness = 1;
        s->add(g3d::Geometry::cylinder(0.25f, h * 0.4f, 12), bark)->position = { x, y + h * 0.2f, z };
        s->add(g3d::Geometry::cone(h * 0.3f, h * 0.8f, 16), leaf)->position = { x, y + h * 0.4f + h * 0.4f, z };
        placed++;
    }
    return s;
}

// A camera that looks along `target` from `from` and drifts round the vertical by `orbit` degrees a second.
void orbitCamera(g3d::Scene& s, Vector3 from, Vector3 target, float orbit, float fov, float fps) {
    s.camera.fov = fov;
    s.camera.up = [from, target, orbit, fps](int t, g3d::Camera& cam) {
        const float yaw = orbit * DEG2RAD * (float)(frameTime(t) / std::max(fps, 1.f));
        const Vector3 look = Vector3RotateByAxisAngle(Vector3Subtract(target, from), { 0, 1, 0 }, yaw);
        cam.position = from;
        cam.target = Vector3Add(from, look);
    };
}

Vector3 aimAt(Vector3 from, float headingDeg, float pitchDeg) {
    const float h = headingDeg * DEG2RAD;
    return { from.x + 100 * std::cos(h), from.y + 100 * std::tan(pitchDeg * DEG2RAD), from.z + 100 * std::sin(h) };
}

float headingOf(Vector3 d) { return std::atan2(d.z, d.x) * RAD2DEG; }

float smoothf(float u) { return smoothstepf(0, 1, u); }

Land landFrom(const Json& p, const char* name) {
    Land L;
    L.size = std::clamp((float)p.num("size", L.size), 20.f, 250.f);
    L.seed = (uint32_t)p.integer("seed", 1);
    L.trees = std::clamp(p.integer("trees", L.trees), 0, 200);
    L.grass = p.str("grass", L.grass);
    L.ground = p.color("ground", L.ground);
    L.fps = std::max(1.f, (float)p.num("fps", 30));
    (void)name;
    return L;
}

g3d::Clouds::Quality qualityOf(const Json& p, const char* name) {
    const std::string q = p.str("quality", "film");
    if (q == "preview") return g3d::Clouds::Quality::Preview;
    if (q == "film") return g3d::Clouds::Quality::Film;
    if (q == "high") return g3d::Clouds::Quality::High;
    throw std::runtime_error(std::string(name) + ": quality must be preview, film or high");
}

// "auto" leaves a look's own shafts; the others replace them. Returns whether it replaced.
bool shaftsOf(const Json& p, const char* name, g3d::Volumetric::Quality& q) {
    const std::string v = p.str("shafts", "auto");
    if (v == "auto") return false;
    if (v == "off") q = g3d::Volumetric::Quality::Off;
    else if (v == "low") q = g3d::Volumetric::Quality::Low;
    else if (v == "medium") q = g3d::Volumetric::Quality::Medium;
    else if (v == "high") q = g3d::Volumetric::Quality::High;
    else throw std::runtime_error(std::string(name) + ": shafts must be auto, off, low, medium or high");
    return true;
}

Look lookOrThrow(const std::string& name, const char* prefab) {
    try { return look(name); }
    catch (const std::invalid_argument& e) { throw std::runtime_error(std::string(prefab) + ": " + e.what()); }
}

// The camera props every prefab shares: `from` [x, y, z], and either `at` [x, y, z] or `heading` (degrees about +y, 0 = +x,
// 90 = +z) and `pitch` (degrees up), plus `orbit` (degrees a second the view turns about the vertical).
struct View { Vector3 from, target; float heading, orbit; };
View viewFrom(const Json& p, const char* name, Vector3 from, float heading, float pitch, Vector3* at = nullptr) {
    View v;
    v.from = vec3(p["from"], from, name, "from");
    v.orbit = (float)p.num("orbit", 0.0);
    if (p.has("at")) v.target = vec3(p["at"], { 0, 0, 0 }, name, "at");
    else if (at && !p.has("heading")) v.target = *at;
    else v.target = aimAt(v.from, (float)p.num("heading", heading), (float)p.num("pitch", pitch));
    v.heading = headingOf(Vector3Subtract(v.target, v.from));
    return v;
}

// ---------------------------------------------------------------- the prefabs
// A storm: a tower with an anvil standing on the horizon ahead of the camera, and a smaller one further off to the side.
void addStormCells(g3d::Scene& s, Vector3 from, float heading, float cloudTop) {
    const float h = heading * DEG2RAD, c = std::cos(h), sn = std::sin(h);
    g3d::Clouds::Storm cell, far;
    cell.x = from.x + c * 26000 + sn * 3000;  cell.z = from.z + sn * 26000 - c * 3000;  cell.radius = 7000;  cell.top = cloudTop - 500;
    far.x = from.x + c * 38000 - sn * 14000;  far.z = from.z + sn * 38000 + c * 14000;  far.radius = 6000;  far.top = cell.top;
    s.clouds.storms = { cell, far };
    s.clouds.anvil = 1;
}

void buildWeather(TNode& n, const Json& p) {
    const char* name = "weather";
    const Rectangle box = boxOf(p, name);
    const Look a = lookOrThrow(p.str("preset", "cumulus day"), name);
    const std::string toName = p.str("to", "");
    const bool blends = !toName.empty();
    const Look b = blends ? lookOrThrow(toName, name) : a;
    const float start = (float)p.num("start", 0), end = (float)p.num("end", 0);
    Land L = landFrom(p, name);
    const float fps = L.fps;
    // "auto" grass: a meadow, but bare ground under settled snow (the grass would hide it)
    if (L.grass == "auto") L.grass = a.weather.snowCover >= 0.3f ? "none" : "meadow";
    // the camera: facing the sun's azimuth plus the look's `viewFromSun`, unless given
    const Vector3 sun = g3d::Sky::at(a.hour).sunDirection;
    const View v = viewFrom(p, name, { 0, 2, 0 }, headingOf(sun) + a.viewFromSun, 6);
    L.keepClear = v.from;
    if (p.flag("lake", true)) {
        // a lake ahead of the camera, in the direction it faces
        const float h = v.heading * DEG2RAD;
        const Vector2 centre{ v.from.x + std::cos(h) * L.size * 0.55f, v.from.z + std::sin(h) * L.size * 0.55f };
        L.shore = shoreOutline(centre, { L.size * 0.38f, L.size * 0.2f }, L.seed);
    }
    auto s = buildLand(L);
    if (!L.shore.empty()) {
        WaterSpec spec;
        spec.shore = L.shore;
        spec.look = std::max(a.weather.wind.speed, b.weather.wind.speed) >= 14 ? Surface::Choppy : Surface::Breezy;      // a gale whips the lake
        g3d::WaterBody lake = waterBody(spec);
        lake.look.fps = fps;
        s->waterBodies.push_back(lake);
    }
    s->clouds.seed = (unsigned)L.seed;
    if (std::max(a.cloudTop, b.cloudTop) > 8000) addStormCells(*s, v.from, v.heading, std::max(a.cloudTop, b.cloudTop));
    s->clouds.quality = qualityOf(p, name);
    s->lightning.seed = L.seed;
    s->precipitation.seed = L.seed;
    s->ground.seed = L.seed;
    s->lightning.groundY = 0;
    s->lightning.area = { v.from.x + std::cos(v.heading * DEG2RAD) * 3000, v.from.z + std::sin(v.heading * DEG2RAD) * 3000 };
    s->lightning.radius = 2200;
    orbitCamera(*s, v.from, v.target, v.orbit, (float)p.num("fov", 55), fps);
    g3d::Volumetric::Quality q = a.shafts;
    const bool forced = shaftsOf(p, name, q);
    auto setLook = [=](g3d::Scene& sc, float amount) {
        Look k = blends ? mix(a, b, amount) : a;
        if (forced) k.shafts = q;
        apply(sc, k, fps);
    };
    setLook(*s, 0);
    s->up = [=](int t, g3d::Scene& sc) {
        const float amount = !blends ? 0.f : end <= start ? (frameTime(t) >= start ? 1.f : 0.f) : smoothf((float)((frameTime(t) - start) / (end - start)));
        setLook(sc, amount);
    };
    n.fn = g3d::draw(s, box);
}

void buildWaterBody(TNode& n, const Json& p) {
    const char* name = "water-body";
    const Rectangle box = boxOf(p, name);
    Land L = landFrom(p, name);
    WaterSpec w;
    const std::string kind = p.str("type", "lake");
    const std::string how = p.str("look", "calm");
    if (how == "calm") w.look = Surface::Calm;
    else if (how == "breezy") w.look = Surface::Breezy;
    else if (how == "choppy") w.look = Surface::Choppy;
    else throw std::runtime_error(std::string(name) + ": look must be calm, breezy or choppy");
    if (kind != "lake" && kind != "pool" && kind != "river") throw std::runtime_error(std::string(name) + ": type must be lake, pool or river");
    w.seed = L.seed;
    w.level = 0;
    w.reflections = p.flag("reflections", true);
    w.shore = points(p["shore"], name, "shore");
    if (!w.shore.empty() && w.shore.size() < 3) throw std::runtime_error(std::string(name) + ": shore needs at least three [x, z] points");
    if (kind == "river") {
        w.path = points(p["path"], name, "path");
        if (w.path.empty()) w.path = { { -L.size * 0.8f, -14 }, { -L.size * 0.3f, -22 }, { L.size * 0.1f, -16 }, { L.size * 0.8f, -28 } };
        if (w.path.size() < 2) throw std::runtime_error(std::string(name) + ": path needs at least two [x, z] points");
        w.width = std::clamp((float)p.num("width", 6), 0.5f, 100.f);
        w.flow = std::clamp((float)p.num("flow", 1.2), 0.f, 30.f);
        L.path = w.path;
        L.riverWidth = w.width;
        L.depth = 1.2f;
    } else {
        if (w.shore.empty()) {
            const bool pool = kind == "pool";
            w.centre = { 0, -L.size * (pool ? 0.2f : 0.38f) };
            w.radius = pool ? Vector2{ 6, 3.5f } : Vector2{ L.size * 0.42f, L.size * 0.22f };
            w.shore = shoreOutline(w.centre, w.radius, L.seed);
        }
        L.shore = w.shore;
        if (kind == "pool") { L.bank = 0.4f; L.depth = 1.6f; }
    }
    L.trees = p.integer("trees", 8);
    auto s = buildLand(L);
    g3d::WaterBody body = waterBody(w);
    body.look.fps = L.fps;
    if (kind == "pool") body.edgeSoft = 0.15f;
    s->waterBodies.push_back(std::move(body));
    // the sky: the hour of day, with clouds for the water to mirror
    Look k = look("cumulus day");
    k.hour = (float)p.num("hour", 15.5);
    k.weather.clouds.cover = (float)p.num("cover", 0.4);
    k.weather.wind.speed = 0;                                    // the water keeps its own waves
    k.aerial = 1;
    k.sunIntensity = 2.6f;
    apply(*s, k, L.fps);
    s->weather.enabled = false;                                    // sky and clouds from the look; the waves are the body's own
    s->ownFog = false;
    s->clouds.ownWeather = true;
    s->clouds.cover = k.weather.clouds.cover;
    s->clouds.type = g3d::CloudType::Cumulus;
    s->clouds.seed = (unsigned)L.seed;
    s->clouds.quality = qualityOf(p, name);
    s->fogMode = g3d::FogMode::Exponential;
    s->fogDensity = 0.0006f;
    s->fogFromSky = true;
    s->sky.clouds = 0;
    Vector3 centre{ 0, 0, 0 };
    if (!L.path.empty()) { for (const auto& q : L.path) { centre.x += q.x / L.path.size(); centre.z += q.y / L.path.size(); } }
    else for (const auto& q : L.shore) { centre.x += q.x / L.shore.size(); centre.z += q.y / L.shore.size(); }
    Vector3 at = centre;
    const View v = viewFrom(p, name, { 0, 2.6f, 0 }, 0, 0, &at);
    L.keepClear = v.from;
    orbitCamera(*s, v.from, v.target, v.orbit, (float)p.num("fov", 55), L.fps);
    n.fn = g3d::draw(s, box);
}

RainSpec rainSpecOf(const Json& p, float fps) {
    RainSpec r;
    r.intensity = std::clamp((float)p.num("intensity", r.intensity), 0.f, 1.f);
    r.windSpeed = std::clamp((float)p.num("wind", r.windSpeed), 0.f, 60.f);
    r.windDirection = (float)p.num("windDirection", r.windDirection);
    r.delay = std::max(0.f, (float)p.num("delay", 0));
    r.ramp = std::max(0.01f, (float)p.num("ramp", r.ramp));
    r.wetRate = std::clamp((float)p.num("wetRate", r.wetRate), 0.f, 100000.f);
    r.lens = std::clamp((float)p.num("lens", 0), 0.f, 1.f);
    r.cover = std::clamp((float)p.num("cover", r.cover), 0.f, 1.f);
    r.seed = (uint32_t)p.integer("seed", 1);
    r.fps = fps;
    return r;
}

void buildRain(TNode& n, const Json& p) {
    const char* name = "rain";
    const Rectangle box = boxOf(p, name);
    Land L = landFrom(p, name);
    L.size = std::min(L.size, 50.f);
    L.trees = p.integer("trees", 6);
    const RainSpec spec = rainSpecOf(p, L.fps);
    const View v = viewFrom(p, name, { 0, 1.8f, 7 }, -90, -3);
    L.keepClear = v.from;
    auto s = buildLand(L);
    // a shelter with a roof, to keep the rain off a bench, a lamp-post and a few barrels beside it
    g3d::Material wood;
    wood.color = { 96, 70, 48, 255 };
    wood.roughness = 0.9f;
    g3d::Material roof;
    roof.color = { 70, 76, 84, 255 };
    roof.roughness = 0.6f;
    g3d::Material metal;
    metal.color = { 40, 44, 50, 255 };
    metal.roughness = 0.5f;
    metal.metalness = 0.6f;
    const float sx = -1, sz = -3, w = 4, d = 2.6f, h = 2.5f;
    for (int i = 0; i < 4; i++)
        s->add(g3d::Geometry::box(0.15f, h, 0.15f), wood)->position = { sx + (i % 2 ? w / 2 : -w / 2) * 0.92f, h / 2, sz + (i / 2 ? d / 2 : -d / 2) * 0.92f };
    s->add(g3d::Geometry::box(w + 0.6f, 0.15f, d + 0.6f), roof)->position = { sx, h + 0.07f, sz };
    s->add(g3d::Geometry::box(2.4f, 0.1f, 0.5f), wood)->position = { sx, 0.45f, sz - 0.7f };
    s->add(g3d::Geometry::cylinder(0.06f, 4.2f, 12), metal)->position = { 3.2f, 2.1f, -2.5f };
    s->add(g3d::Geometry::sphere(0.2f, 12, 16), metal)->position = { 3.2f, 4.3f, -2.5f };
    for (int i = 0; i < 3; i++) s->add(g3d::Geometry::cylinder(0.3f, 0.9f, 20), wood)->position = { 4.4f + 0.7f * (float)i, 0.45f, -1.6f - 0.5f * (float)(i % 2) };
    Look k = look("squall");
    k.weather = g3d::Weather::rain();
    k.hour = (float)p.num("hour", 15);
    k.cloudBase = 500;  k.cloudTop = 1700;  k.cloudDensity = 1.5f;
    k.aerial = 0.5f;  k.sunIntensity = 1.8f;
    apply(*s, k, L.fps);
    s->clouds.seed = (unsigned)L.seed;
    s->clouds.quality = qualityOf(p, name);
    addRain(*s, spec);
    s->weather.accumulationScale = spec.wetRate;
    orbitCamera(*s, v.from, v.target, v.orbit, (float)p.num("fov", 55), L.fps);
    n.fn = g3d::draw(s, box);
    if (spec.lens > 0) n.isolate().filters.push_back(fx::lens(rainLens(spec)));
}

StrikeSpec strikeSpecOf(const Json& p, Vector3 from, float heading) {
    StrikeSpec k;
    k.count = std::clamp(p.integer("count", k.count), 0, 512);
    k.start = (float)p.num("start", 0);
    k.span = std::max(0.1f, (float)p.num("span", k.span));
    k.radius = std::max(1.f, (float)p.num("radius", k.radius));
    k.cloudBase = (float)p.num("cloudBase", k.cloudBase);
    k.groundShare = std::clamp((float)p.num("groundShare", k.groundShare), 0.f, 1.f);
    k.seed = (uint32_t)p.integer("seed", 1);
    const float h = heading * DEG2RAD;
    k.area = { from.x + std::cos(h) * 3000, from.z + std::sin(h) * 3000 };
    if (p.has("area")) {
        const Json& a = p["area"];
        if (!a.isArray() || a.size() != 2) throw std::runtime_error("lightning: area must be [x, z]");
        k.area = { (float)a[(size_t)0].asNumber(), (float)a[(size_t)1].asNumber() };
    }
    return k;
}

void buildLightning(TNode& n, const Json& p) {
    const char* name = "lightning";
    const Rectangle box = boxOf(p, name);
    Land L = landFrom(p, name);
    L.size = std::min(L.size, 50.f);
    L.trees = p.integer("trees", 6);
    const View v = viewFrom(p, name, { 0, 2, 0 }, -90, 14);
    L.keepClear = v.from;
    const float fps = L.fps;
    const StrikeSpec spec = strikeSpecOf(p, v.from, v.heading);
    auto s = buildLand(L);
    Look k = look("thunderstorm");
    k.weather.lightningRate = 0;                            // the schedule is the strikes
    k.weather.precipitation.intensity = (float)p.num("rain", 0.8);
    k.cloudBase = spec.cloudBase;
    apply(*s, k, fps);
    addStormCells(*s, v.from, v.heading, k.cloudTop);
    s->clouds.seed = (unsigned)L.seed;
    s->clouds.quality = qualityOf(p, name);
    s->precipitation.seed = L.seed;
    addLightning(*s, spec);
    s->lightning.light = (float)p.num("light", 1);
    orbitCamera(*s, v.from, v.target, v.orbit, (float)p.num("fov", 60), fps);
    n.fn = g3d::draw(s, box);
    // the thunder: a clip placed where each strike's sound arrives
    const std::string clip = p.str("clip", "");
    if (!clip.empty()) {
        const float volume = std::max(0.f, (float)p.num("volume", 1));
        for (const g3d::Thunder& c : thunderCues(spec, v.from)) {
            const int at = (int)std::lround(c.arrives * fps);
            if (at < 0 || at >= n.dur) continue;
            TNode* kid = n.add(at, n.dur - at);
            media::AudioOptions o;
            o.volume = volume * std::clamp(1.f / (1.f + c.distance / 1500.f), 0.15f, 1.f);
            media::audio(kid, clip, o);
        }
    }
}

}  // namespace

void registerPrefabs() {
    prefab::add({ "weather",
        Json::parse(R"({"box":[0,0,1920,1080],"preset":"cumulus day","to":"","start":0,"end":0,"size":60,"lake":true,
                        "trees":10,"grass":"auto","pitch":6,"orbit":0,"fov":55,"quality":"film",
                        "shafts":"auto","seed":1,"fps":30,"ground":"#5d6b3c"})"),
        buildWeather });
    prefab::add({ "water-body",
        Json::parse(R"({"box":[0,0,1920,1080],"type":"lake","look":"calm","width":6,"flow":1.2,
                        "reflections":true,"hour":15.5,"cover":0.4,"size":60,"trees":8,"grass":"lawn","orbit":0,"fov":55,
                        "quality":"film","seed":1,"fps":30,"ground":"#5d6b3c"})"),
        buildWaterBody });
    prefab::add({ "rain",
        Json::parse(R"({"box":[0,0,1920,1080],"intensity":0.6,"wind":7,"windDirection":17,"delay":0,"ramp":1,"wetRate":60,
                        "lens":0,"cover":0.95,"size":40,"trees":6,"orbit":0,"fov":55,"quality":"film","seed":1,"fps":30,
                        "ground":"#4a5636"})"),
        buildRain });
    prefab::add({ "lightning",
        Json::parse(R"({"box":[0,0,1920,1080],"count":6,"start":0,"span":10,"radius":1500,"cloudBase":1200,"groundShare":0.5,
                        "rain":0.8,"light":1,"clip":"","volume":1,"size":40,"trees":6,"orbit":0,"fov":60,"quality":"film",
                        "seed":1,"fps":30,"ground":"#3f4a2e"})"),
        buildLightning });
}

}  // namespace wx
}  // namespace rmtn
