// weather.cpp -- g3d::Weather: presets, mix, and how the weather's numbers become each system's
// own settings. CPU only; render() in scene3d.cpp calls the inWeather() functions every frame.
#include "rmtn/scene3d.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace rmtn {
namespace g3d {

namespace {
float lerp(float a, float b, float t) { return a + (b - a) * t; }

Color mixC(Color a, Color b, float t) {
    auto m = [t](unsigned char x, unsigned char y) { return (unsigned char)std::lround(x + (y - x) * t); };
    return { m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), a.a };
}

// The grey a colour fades to under cloud: its own brightness, a little darker.
Color greyOf(Color c, float darken) {
    const float l = (0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b) * (1 - darken);
    const unsigned char v = (unsigned char)std::clamp((int)std::lround(l), 0, 255);
    return { v, v, v, c.a };
}

Weather preset(float speed, float gust, float gustSpeed, float cover, CloudType type, Precipitation kind, float intensity,
               float wet, float snow, float haze, float strikes) {
    Weather w;
    w.enabled = true;
    w.wind.speed = speed;
    w.wind.gust = gust;
    w.wind.gustSpeed = gustSpeed;
    w.clouds = { cover, type };
    w.precipitation = { kind, intensity };
    w.wetness = wet;
    w.snowCover = snow;
    w.haze = haze;
    w.lightningRate = strikes;
    return w;
}
} // namespace

Weather Weather::clear() { return preset(3, 0.2f, 4, 0.15f, CloudType::Cumulus, Precipitation::None, 0, 0, 0, 0.05f, 0); }
Weather Weather::overcast() { return preset(5, 0.25f, 5, 0.9f, CloudType::Stratus, Precipitation::None, 0, 0, 0, 0.15f, 0); }
Weather Weather::rain() { return preset(7, 0.4f, 7, 0.95f, CloudType::Stratus, Precipitation::Rain, 0.5f, 0.8f, 0, 0.25f, 0); }
Weather Weather::storm() { return preset(20, 0.8f, 14, 1, CloudType::Cumulonimbus, Precipitation::Rain, 1, 1, 0, 0.3f, 6); }
Weather Weather::snow() { return preset(4, 0.2f, 4, 0.9f, CloudType::Stratus, Precipitation::Snow, 0.5f, 0, 0.6f, 0.35f, 0); }
Weather Weather::fog() { return preset(1, 0.05f, 2, 0.6f, CloudType::Deck, Precipitation::None, 0, 0.3f, 0, 0.9f, 0); }

Weather Weather::mix(const Weather& a, const Weather& b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    Weather w = t < 0.5f ? a : b;                  // the kinds and types switch at 0.5
    w.enabled = a.enabled || b.enabled;
    w.fps = lerp(a.fps, b.fps, t);
    // The wind direction the short way round: 350 to 10 degrees goes through 0, not 180.
    const float turn = std::fmod(std::fmod(b.wind.direction - a.wind.direction, 360.f) + 540.f, 360.f) - 180.f;
    w.wind.direction = a.wind.direction + turn * t;
    w.wind.speed = lerp(a.wind.speed, b.wind.speed, t);
    w.wind.gust = lerp(a.wind.gust, b.wind.gust, t);
    w.wind.gustSpeed = lerp(a.wind.gustSpeed, b.wind.gustSpeed, t);
    w.wind.gustScale = lerp(a.wind.gustScale, b.wind.gustScale, t);
    w.windRun = a.windRun >= 0 && b.windRun >= 0 ? lerp(a.windRun, b.windRun, t) : -1.f;
    w.clouds.cover = lerp(a.clouds.cover, b.clouds.cover, t);
    w.precipitation.intensity = lerp(a.precipitation.intensity, b.precipitation.intensity, t);
    w.wetness = lerp(a.wetness, b.wetness, t);
    w.snowCover = lerp(a.snowCover, b.snowCover, t);
    w.haze = lerp(a.haze, b.haze, t);
    w.lightningRate = lerp(a.lightningRate, b.lightningRate, t);
    return w;
}

// ---- the rules

Vector2 Weather::windVector() const {
    const float d = wind.direction * DEG2RAD;
    return { std::cos(d), std::sin(d) };
}
float Weather::fogDensity() const { const float h = std::clamp(haze, 0.f, 1.f); return 0.065f * h * h; }
float Weather::cloudGrey() const { return std::pow(std::clamp(clouds.cover, 0.f, 1.f), 1.5f) * 0.85f; }
float Weather::sunScale() const { return 1 - 0.75f * std::clamp(clouds.cover, 0.f, 1.f); }
float Weather::ambientScale() const { return 1 + 0.35f * std::clamp(clouds.cover, 0.f, 1.f); }
float Weather::skyLightScale() const { return 1 - 0.35f * std::clamp(clouds.cover, 0.f, 1.f); }
float Weather::grassSway() const { return std::clamp(0.015f * wind.speed, 0.f, 0.6f); }
float Weather::grassGust() const { return 0.7f * std::clamp(wind.gust, 0.f, 1.f); }
float Weather::treeWind() const { return std::max(wind.speed / 8.f, 0.05f); }
float Weather::treeGust() const { return 1.2f * std::clamp(wind.gust, 0.f, 1.f); }
float Weather::waveHeight(float fetch) const {
    const float g = 9.81f, U = std::max(wind.speed, 0.1f), F = std::max(fetch, 1.f);
    return std::min(0.0016f * U * std::sqrt(F / g), 0.21f * U * U / g);
}
float Weather::wavePeakLength(float fetch) const {
    const float g = 9.81f, U = std::max(wind.speed, 0.1f), F = std::max(fetch, 1.f);
    const float fp = 3.5f * (g / U) * std::pow(g * F / (U * U), -0.33f);
    const float tp = std::min(1.f / fp, 7.16f * U / g);
    return g * tp * tp / (2 * PI);
}
float Weather::cloudTravel(double seconds) const { return windRun >= 0 ? windRun : (float)(wind.speed * seconds); }

// ---- each system

Field inWeather(const Field& f, const Weather& w, const Matrix* world) {
    Field out = f;
    if (!w.enabled || f.ownWind) return out;
    Vector2 d = w.windVector();
    if (world) {                                    // the shader wants the wind in the field's own space
        const Matrix inv = MatrixInvert(*world);
        const Vector3 a = Vector3Transform({ d.x, 0, d.y }, inv), o = Vector3Transform({ 0, 0, 0 }, inv);
        const Vector2 l{ a.x - o.x, a.z - o.z };
        if (Vector2Length(l) > 1e-6f) d = Vector2Normalize(l);
    }
    out.windDirection = d;
    out.sway = w.grassSway();
    out.gust = w.grassGust();
    out.gustSpeed = w.wind.gustSpeed;
    out.gustScale = w.wind.gustScale;
    return out;
}

Water inWeather(const Water& water, const Weather& w) {
    Water out = water;
    if (!w.enabled || water.ownWind || water.waves.empty()) return out;
    // The waves' mean heading, weighted by amplitude; the wind turns it all by one angle.
    float sx = 0, sy = 0;
    for (const Water::Wave& v : water.waves) {
        const float a = v.direction * DEG2RAD, m = std::fabs(v.amplitude);
        sx += m * std::cos(a);
        sy += m * std::sin(a);
    }
    const float mean = sx * sx + sy * sy > 1e-12f ? std::atan2(sy, sx) * RAD2DEG : water.waves[0].direction;
    const float turn = w.wind.direction - mean;
    // Fetch-limited sea: scale the authored waves so Hs and the longest wavelength are the wind's.
    float sum = 0, longest = 0;
    for (const Water::Wave& v : water.waves) { sum += v.amplitude * v.amplitude / 2; longest = std::max(longest, v.wavelength); }
    const float hs0 = 4 * std::sqrt(sum);
    const float amp = hs0 > 1e-9f ? w.waveHeight(water.fetch) / hs0 : 1.f;
    const float len = longest > 1e-6f ? w.wavePeakLength(water.fetch) / longest : 1.f;
    for (Water::Wave& v : out.waves) {
        v.direction += turn;
        v.wavelength *= len;
        v.amplitude *= amp;
        const float cap = 0.5f * Weather::kMaxSteepness * v.wavelength;    // 2 a / L <= kMaxSteepness
        if (std::fabs(v.amplitude) > cap) v.amplitude = std::copysign(cap, v.amplitude);
    }
    return out;
}

Sky inWeather(const Sky& sky, const Weather& w, double frame) {
    Sky k = sky;
    if (!w.enabled || !sky.enabled || sky.ownClouds) return k;
    const float cover = std::clamp(w.clouds.cover, 0.f, 1.f), g = w.cloudGrey(), keep = 1 - cover;
    k.clouds = cover;
    // Under cover the sky goes to grey, a little darker; the sun's disc, glow and the stars go.
    k.zenith = mixC(sky.zenith, greyOf(sky.zenith, 0.1f), g);
    k.horizon = mixC(sky.horizon, greyOf(sky.horizon, 0.12f), g);
    k.haze = mixC(sky.haze, greyOf(sky.haze, 0.1f), g);
    k.cloudColor = mixC(sky.cloudColor, greyOf(sky.cloudColor, 0.12f), g);
    k.cloudShade = mixC(sky.cloudShade, greyOf(sky.cloudShade, 0.3f), g);
    k.sunSize = sky.sunSize * keep * keep;
    k.sunGlow = sky.sunGlow * (1 - 0.85f * cover);
    k.hazeAmount = sky.hazeAmount * (1 - 0.5f * cover);
    k.stars = sky.stars * keep;
    // The clouds drift downwind. The sky shader moves a cloud against the sign of its offset.
    const Vector2 d = w.windVector();
    const float run = w.cloudTravel(frame / std::max(w.fps, 1.f)) * Weather::kCloudPerMetre;
    k.cloudDrift = { 0, 0 };
    k.cloudShift = { -d.x * run, -d.y * run };
    return k;
}

fx::Emitter3D inWeather(const fx::Emitter3D& e, const Weather& w, const Matrix& world) {
    fx::Emitter3D out = e;
    if (!w.enabled || e.ownWind || e.windFollow <= 0) return out;
    const Vector2 d = w.windVector();
    const float v = e.windFollow * w.wind.speed / std::max(w.fps, 1.f);       // units a frame, in the world
    const Matrix inv = MatrixInvert(world);
    const Vector3 a = Vector3Transform({ d.x * v, 0, d.y * v }, inv), o = Vector3Transform({ 0, 0, 0 }, inv);
    out.drift = { e.drift.x + a.x - o.x, e.drift.y + a.y - o.y, e.drift.z + a.z - o.z };
    if (std::fabs(e.direction.y) > 1e-4f) out.direction = { 0, e.direction.y, 0 };     // the wind sets the slant
    return out;
}

Water waterOf(const Scene& scene) { return inWeather(scene.water, scene.weather); }

Clouds inWeather(const Clouds& c, const Weather& w) {
    Clouds k = c;
    if (!w.enabled || c.ownWeather) return k;
    k.cover = std::clamp(w.clouds.cover, 0.f, 1.f);
    k.type = w.clouds.type;
    if (k.type == CloudType::Cumulonimbus) k.top = std::max(k.top, 12000.f);   // the tower reaches the tropopause
    return k;
}

Vector2 Clouds::travel(const Weather& w, double seconds) const {
    if (w.enabled && !ownWeather) {
        const Vector2 d = w.windVector();
        const float run = w.cloudTravel(seconds);
        return { d.x * run, d.y * run };
    }
    return { (float)(drift.x * seconds), (float)(drift.y * seconds) };
}

// ---- precipitation (Downfall): the CPU twin of the shader in precip_glsl.hpp. Keep them in step.
namespace {
uint32_t dropHash(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
float dropRand(uint32_t x) { return (float)(dropHash(x) >> 8) / 16777216.f; }
float fract1(float x) { return x - std::floor(x); }
} // namespace

Vector2 Downfall::slant(bool snow) const {
    const bool flake = snow && kind == Precipitation::Snow;         // sleet falls at the rain's slant
    const Vector2 sp = flake ? snowSpeed : rainSpeed;
    const float mean = std::max(0.5f * (sp.x + sp.y), 0.1f);
    const float k = std::max(windSpeed, 0.f) * (flake ? Weather::kSnowFollow : Weather::kRainFollow) / mean;
    const float d = windDirection * DEG2RAD;
    return { std::cos(d) * k, std::sin(d) * k };
}

uint32_t Downfall::dropsPerTile() const {
    const float density = kind == Precipitation::Snow ? snowDensity : kind == Precipitation::Sleet ? 0.5f * (rainDensity + snowDensity) : rainDensity;
    return (uint32_t)std::max(0.f, std::round(density * kTile * kTile * kCell));
}
uint32_t Downfall::farDropsPerTile() const {
    return (uint32_t)std::max(0.f, std::round(farDensity * kFarTile * kFarTile * kFarCell));
}

Downfall::Drop Downfall::drop(int ix, int iy, int iz, uint32_t k, double seconds, bool faraway) const {
    const float S = faraway ? kFarTile : kTile, Hc = faraway ? kFarCell : kCell;
    const uint32_t n = faraway ? farDropsPerTile() : dropsPerTile();
    const uint32_t sd = faraway ? seed ^ 0x9E3779B9u : seed;
    const uint32_t ck = dropHash((uint32_t)ix * 73856093u ^ (uint32_t)iz * 19349663u ^ (uint32_t)iy * 83492791u ^ sd * 2654435761u);
    const uint32_t h = dropHash(k * 2654435769u ^ ck);
    const float r1 = dropRand(h), r2 = dropRand(h + 1u), r3 = dropRand(h + 2u), r4 = dropRand(h + 3u);
    const float r5 = dropRand(h + 4u), r6 = dropRand(h + 5u), r7 = dropRand(h + 6u), r8 = dropRand(h + 7u);
    Drop d;
    d.rank = ((float)k + 0.5f) / std::max((float)n, 1.f);
    d.snow = kind == Precipitation::Snow || (kind == Precipitation::Sleet && r5 < 0.4f);
    const Vector2 sp = d.snow ? snowSpeed : rainSpeed, sz = d.snow ? snowSize : rainSize;
    const float v = d.snow ? sp.x + (sp.y - sp.x) * r6 : sp.x + (sp.y - sp.x) * r3;
    d.diameter = sz.x + (sz.y - sz.x) * r3 * r3;
    const Vector2 a = slant(d.snow);
    const float s = (float)seconds;
    const float f = fract1(s * v / Hc + r4);
    d.phase = f;
    const float hgt = Hc * (1 - f);
    const float qx = ((float)ix + r1) * S, qz = ((float)iz + r2) * S;
    Vector3 pos{ qx - a.x * hgt, (float)iy * Hc + hgt, qz - a.y * hgt };
    Vector3 vel{ a.x * v, -v, a.y * v };
    if (d.snow) {
        const float w1 = 1.f + r1, w2 = 1.3f * (1.f + r2), p1 = r7 * 6.2831853f, p2 = r8 * 6.2831853f;
        pos.x += flutter * std::sin(s * w1 + p1);
        pos.z += flutter * std::sin(s * w2 + p2);
        vel.x += flutter * w1 * std::cos(s * w1 + p1);
        vel.z += flutter * w2 * std::cos(s * w2 + p2);
    }
    d.position = pos;
    d.velocity = vel;
    return d;
}

Downfall inWeather(const Downfall& d, const Weather& w) {
    Downfall k = d;
    if (!w.enabled) return k;
    if (!d.ownWeather) {
        k.kind = w.precipitation.kind;
        k.intensity = std::clamp(w.precipitation.intensity, 0.f, 1.f);
    }
    if (!d.ownWind) {
        k.windDirection = w.wind.direction;
        k.windSpeed = w.wind.speed;
        k.fps = w.fps;
    }
    return k;
}

} // namespace g3d
} // namespace rmtn
