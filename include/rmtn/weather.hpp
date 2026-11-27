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
// Lightning is drawn by Scene::lightning (strikes from the rate here or from its own list), the rainbow by the
// scene when Weather::rainbow allows it. The ground (wetness, settled snow) is drawn by the lit shader; see Weather::ground and Scene::ground.
#pragma once
#include <raylib.h>
#include <vector>

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
        CloudType type = CloudType::Cumulus;    // read by Scene::clouds (volumetric clouds) unless Clouds::ownWeather
    } clouds;

    struct Precip {
        Precipitation kind = Precipitation::None;   // used by the rain and snow systems (Scene::precipitation draws it)
        float intensity = 0;            // 0..1: drizzle to downpour
    } precipitation;

    // Wet ground and settled snow (W4). Both are accumulated from the weather's history, as a pure function of
    // the time (see ground()). A value >= 0 here PINS that quantity instead (ground wet to that degree, snow that
    // deep, whatever the history says); the default -1 lets it accumulate.
    float wetness = -1;                 // 0..1 pin: how wet the ground is (-1: soaks in and dries by itself)
    float snowCover = -1;               // 0..1 pin: settled snow as a share of kSnowFull (-1: builds and melts by itself)
    float haze = 0.05f;                 // 0..1: 0 clear air, 1 thick fog; sets the exponential fog
    float lightningRate = 0;            // strikes a minute: drawn by Scene::lightning (rmtn/scene3d.hpp) unless it has its own rate

    // The rainbow (W5). Drawn by the scene when it is on: Auto shows it when rain falls (here, or out of sight:
    // distantRain) with the sun out and under 42 degrees high, behind the camera; On shows it whatever the rain and the
    // sun's height (the sun still has to be somewhere: the bow is centred on the point opposite it); Off never.
    struct Rainbow {
        enum class Mode { Auto, On, Off };
        Mode mode = Mode::Auto;
        float distantRain = 0;          // 0..1: rain falling out of sight, which makes a bow when none falls here
        float rainDistance = 2500;      // metres to the rain: geometry nearer than a third of it hides the bow, it is full by this distance
        float strength = 1;             // 0..2 times the bow's brightness
    } rainbow;
    // How much of the bow shows for the rain that falls (0..1): the weather's rain intensity (rain and sleet; a bow
    // needs drops) or the distant rain, whichever is more, through a soft curve.
    float rainbowRain() const;
    // The sun's share of the bow's strength: it follows how much sun gets through cloud (sunScale) and fades out as the sun
    // sets, and in Auto above 42 degrees (the bow is below the horizon then). `elevation` is the sun's height in degrees.
    float rainbowSun(float elevation) const;

    // Air temperature in degrees Celsius: snow sticks and builds below about 0, melts above it. kAutoTemperature
    // (the default) takes it from the kind of precipitation: -3 for snow and sleet, 8 for rain, 10 otherwise.
    static constexpr float kAutoTemperature = 1000;
    float temperature = kAutoTemperature;
    float airTemperature() const;

    // How fast the ground's clock runs against the film's. Real snow lays 1-5 cm an hour and a puddle takes
    // minutes to fill, so a 20 s clip shows nothing at 1. With 600 a second of film is ten minutes of weather:
    // a 20 s clip is 3 hours (about 15 cm of snow at intensity 1). Applies to wetness and snow depth only.
    float accumulationScale = 1;

    // What the weather has been, to build the ground up from: key i holds from `seconds` (film time) until the
    // next key, and between two keys of the same kind the intensity changes linearly. Keys in ascending time.
    // Before the first key nothing fell; after the last its value holds, so end with { t, None, 0 } where the
    // rain stops. Empty (the default): the current kind and intensity are taken to have held since t = 0.
    struct Key { float seconds = 0; Precipitation kind = Precipitation::None; float intensity = 0; };
    std::vector<Key> history;

    struct GroundState { float wetness = 0, snowDepth = 0; };
    // The ground at `seconds` of film time: wetness 0..1 and the depth of settled snow in metres.
    //   wetness: dw/ds = rain / kSoakTime * (1 - w) - w / dryTime, s in weather seconds (film seconds x
    //            accumulationScale), rain = the intensity of rain (0.6 of sleet's); dryTime from dryTimeConstant().
    //   snow:    depth grows at kSnowRate * intensity (0.4 of sleet's) while the air is cold (it sticks fully
    //            at or under 0, not at all from 3 degrees up), and melts at kMeltRate per degree above 0, faster in sun.
    // It is integrated by fixed steps of 0.1 s of film (coarser past 400 s) over the history, so it is a function of
    // `seconds` and this struct alone, never of an earlier frame. A pinned wetness or snowCover replaces its value.
    GroundState ground(double seconds) const;
    float wetnessAt(double seconds) const { return ground(seconds).wetness; }
    float snowDepthAt(double seconds) const { return ground(seconds).snowDepth; }
    float dryTimeConstant() const;      // weather seconds: wetness falls to 1/e in this long once the rain has stopped
    float dryingTime() const;           // weather seconds for wetness to fall from 1 to under 0.1 (2.3 time constants)
    static constexpr float kSoakTime = 150;         // s: time constant of soaking at intensity 1
    static constexpr float kDryTime = 1500;         // s: time constant of drying in calm air under full sun
    static constexpr float kSnowRate = 0.05f / 3600;    // m/s: 5 cm an hour at intensity 1
    static constexpr float kMeltRate = 0.005f / 3600;   // m/s per degree above 0 (0.5 cm an hour per degree), times 0.5 + sunScale()
    static constexpr float kSnowFull = 0.10f;       // m: snowCover 1 is this deep (and the shader's snow is whole by 6 cm)

    // Presets. All have `enabled` true and a wind towards 17 degrees; change what you need. The ground
    // (wetness, snow cover) is left to accumulate (-1); the precipitation decides.
    //             wind m/s  gust  cover  type          precipitation   haze  strikes/min
    //   clear         3      0.2   0.15   cumulus       none            0.05    0
    //   overcast      5      0.25  0.90   stratus       none            0.15    0
    //   rain          7      0.4   0.95   stratus       rain 0.5        0.25    0
    //   storm        20      0.8   1.00   cumulonimbus  rain 1.0        0.30    6
    //   snow          4      0.2   0.90   stratus       snow 0.5        0.35    0
    //   fog           1      0.05  0.60   deck          none            0.90    0
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
