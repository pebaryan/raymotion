// rmtn/weather.hpp -- one weather state for a 3D scene (g3d::Weather, Scene::weather).
//
//     scene->weather = g3d::Weather::storm();          // wind, cloud cover, haze, rain...
//     scene->up = [](int t, g3d::Scene& s) {           // a clear morning turns into a storm
//         s.weather = g3d::Weather::mix(g3d::Weather::clear(), g3d::Weather::storm(), t / 300.f);
//     };
//
// The weather is data. While `enabled`, the scene's own systems read it when they are drawn:
// grass fields, forests and growing trees, the water's waves, rain and snow emitters and the
// sky's cloud drift take their wind from it; the sky's cloud cover, its greyness and the sun
// light's strength follow the cloud cover; the haze sets the fog. The user's own structs are
// never changed (the values are worked out for each frame from `Scene::weather` and from t),
// so a weather animated in Scene::up renders every frame on its own. Each affected struct has
// an opt-out (`ownWind` on Field, flora::ForestSpec, Water and fx::Emitter3D; `ownClouds` and
// `ownLight` on Sky; `ownFog` on Scene) that keeps its own settings. With `enabled` false (the
// default) nothing is read and nothing changes. docs/weather.md has the rules and the table.
//
// Some fields (cloud type, precipitation, wetness, snow cover, lightning) are data for the
// systems that come later in this release: setting them changes no pixel yet.
#pragma once
#include <raylib.h>

namespace rmtn {
namespace g3d {

enum class CloudType { Cumulus, Stratus, Cumulonimbus, Deck };
enum class Precipitation { None, Rain, Snow, Sleet };

struct Weather {
    bool enabled = false;               // false: the scene ignores all of this
    float fps = 30;                     // the film's frames per second: converts m/s to per frame, as Field::fps

    // One wind for everything that moves in it.
    struct Wind {
        float direction = 17;           // degrees about +y the wind blows TOWARDS (0 = +x, 90 = +z, as Water::Wave)
        float speed = 3;                // metres a second, steady (3 a light breeze, 8 fresh, 20 a gale)
        float gust = 0.3f;              // 0..1: how hard the gust fronts push on top of the steady wind
        float gustSpeed = 5;            // metres a second the gust fronts travel downwind
        float gustScale = 25;           // metres between gust fronts
    } wind;

    // The metres the wind has carried the cloud layer since frame 0. < 0 (the default): speed times
    // time, right for a steady wind. A wind that changes over a shot sets this from the integral of
    // its speed (a function of t), or the clouds would swim as the speed changes.
    float windRun = -1;

    struct Clouds {
        float cover = 0.25f;            // 0..1: the share of the sky under cloud
        CloudType type = CloudType::Cumulus;    // used by volumetric clouds (not drawn yet)
    } clouds;

    struct Precip {
        Precipitation kind = Precipitation::None;   // used by the rain and snow systems (not drawn yet)
        float intensity = 0;            // 0..1: drizzle to downpour
    } precipitation;

    float wetness = 0;                  // 0..1: how wet the ground is (not drawn yet)
    float snowCover = 0;                // 0..1: how much settled snow (not drawn yet)
    float haze = 0.05f;                 // 0..1: 0 clear air, 1 thick fog; sets the exponential fog
    float lightningRate = 0;            // strikes a minute (not drawn yet)

    // Presets. All have `enabled` true and a wind towards 17 degrees; change what you need.
    //             wind m/s  gust  cover  type          precipitation   wet  snow  haze  strikes/min
    //   clear         3      0.2   0.15   cumulus       none             0    0    0.05    0
    //   overcast      5      0.25  0.90   stratus       none             0    0    0.15    0
    //   rain          7      0.4   0.95   stratus       rain 0.5         0.8  0    0.25    0
    //   storm        20      0.8   1.00   cumulonimbus  rain 1.0         1    0    0.30    6
    //   snow          4      0.2   0.90   stratus       snow 0.5         0    0.6  0.35    0
    //   fog           1      0.05  0.60   deck          none             0.3  0    0.90    0
    static Weather clear();
    static Weather overcast();
    static Weather rain();
    static Weather storm();
    static Weather snow();
    static Weather fog();

    // Between two states. Every number is interpolated (the wind direction by angle, the short
    // way round); the kinds and types switch at t = 0.5, which draws nothing today. `enabled`
    // is true when either is.
    static Weather mix(const Weather& a, const Weather& b, float t);

    // ---- how the weather reaches each system (the rules docs/weather.md tabulates)
    Vector2 windVector() const;         // unit (x, z) the wind blows towards
    float fogDensity() const;           // exponential fog per metre: 0.065 * haze^2 (haze 0.3: visibility 0.7 km)
    float cloudGrey() const;            // 0..1 how far the sky greys: cover^1.5 * 0.85
    float sunScale() const;             // the sun light's intensity under cloud: 1 - 0.75 * cover
    float ambientScale() const;         // the ambient light under cloud: 1 + 0.35 * cover
    float skyLightScale() const;        // how strongly the sky lights the scene (Sky::lighting) under cloud: 1 - 0.35 * cover
    float grassSway() const;            // Field::sway = 0.015 * speed, at most 0.6
    float grassGust() const;            // Field::gust = 0.7 * gust
    float treeWind() const;             // ForestSpec::wind = speed / 8, at least 0.05 (the loop stays alive)
    float treeGust() const;             // ForestSpec::gust = 1.2 * gust
    // Waves, fetch-limited (JONSWAP): U = wind speed, F = metres of open water upwind (Water::fetch), g = 9.81.
    //   significant height  Hs = min(0.0016 U sqrt(F / g), 0.21 U^2 / g)         (the fully developed sea)
    //   peak period         Tp = min(1 / (3.5 (g / U) (g F / U^2)^-0.33), 7.16 U / g)
    //   longest wavelength  L  = g Tp^2 / (2 pi)   (deep water)
    // Water scales its authored waves so their Hs and longest wavelength are these (see inWeather).
    float waveHeight(float fetch) const;        // Hs in metres
    float wavePeakLength(float fetch) const;    // L in metres
    static constexpr float kMaxSteepness = 0.12f;   // 2 a / L never above this, so crests never fold
    float cloudTravel(double seconds) const;    // metres the wind has carried the clouds by then
    // Sky-image units a cloud moves a metre of wind travel (the sky's `cloudDrift` is per frame:
    // the default 0.0004 a frame is 10 m/s at 30 fps). The clouds move downwind.
    static constexpr float kCloudPerMetre = 0.0012f;
    // How much of the wind's speed a rain or snow particle takes up sideways (Emitter3D::windFollow).
    static constexpr float kRainFollow = 0.5f, kSnowFollow = 0.3f;
};

} // namespace g3d
} // namespace rmtn
