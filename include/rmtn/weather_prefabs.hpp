// rmtn/weather_prefabs.hpp -- weather and water as drop-ins: four prefabs ("weather", "water-body",
// "rain", "lightning") and the C++ helpers under them. Nothing here draws by itself: every helper
// sets the engine's own structs (g3d::Weather, Clouds, Downfall, Ground, Lightning, WaterBody,
// fx::Lens), so what they make is documented in docs/weather.md and docs/water.md, and every frame is
// a function of t.
//
//     wx::applyPreset(*scene, "squall");                                    // sky, clouds, rain, wet ground
//     wx::applyBlend(*scene, "cumulus day", "thunderstorm", amount);        // amount 0..1 from t
//     wx::WaterSpec lake;  lake.look = wx::Surface::Breezy;
//     scene->waterBodies.push_back(wx::waterBody(lake));
//     wx::RainSpec rain;  rain.intensity = 0.7f;
//     wx::addRain(*scene, rain);
//     wx::StrikeSpec strikes;  strikes.count = 8;  strikes.seed = 4;
//     wx::addLightning(*scene, strikes);
//
//     prefab::use(root, 0, 300, "weather", Json::parse(R"({"preset":"cumulus day","to":"thunderstorm","start":60,"end":240})"));
//
// `registerPrefabs()` adds the four prefabs (runCli calls it); from a script they are used with
// prefab::use(start, duration, name, propsJson). The props are in docs/weather.md ("Prefabs") and
// docs/water.md.
#pragma once
#include "json.hpp"
#include "lens.hpp"
#include "scene3d.hpp"
#include <string>
#include <vector>

namespace rmtn {
namespace wx {

// ---------------------------------------------------------------- weather presets
// One named look of the sky and the weather under it: a g3d::Weather (wind, cover, precipitation, haze,
// lightning rate, rainbow, ground pins), the hour of the day for the sky and sun, the cloud layer, and the
// light in the air. The eight presets: "clear", "cumulus day", "overcast", "squall", "thunderstorm",
// "snowfall", "fog dawn" and "golden evening". Names are matched ignoring case, with '-' and '_' as spaces.
struct Look {
    g3d::Weather weather;
    float hour = 12;                    // 0..24: the sky's colours and the sun's place (g3d::Sky::at)
    float cloudBase = 1500, cloudTop = 3500, cloudDensity = 1;      // metres, and g3d::Clouds::density
    g3d::Volumetric::Quality shafts = g3d::Volumetric::Quality::Off;    // sun shafts in the haze
    float aerial = 0;                   // g3d::AerialPerspective::strength (0: off)
    float sunIntensity = 2.4f;          // of the key light before the clouds dim it
    float ambient = -1;                 // Scene::ambient (the sky's light on shadows); < 0 leaves the scene's own
    float gloom = 0;                    // 0..1: the sky's colours go dark slate (a storm's ambient light)
    float viewFromSun = 40;             // degrees: which way the prefab's camera looks, from the sun's azimuth (0 into the sun, 180 away from it)
};
const std::vector<std::string>& presetNames();
// The look for `name`; throws std::invalid_argument naming the choices when there is none.
Look look(const std::string& name);
// Between two looks: every number interpolated (the weather by Weather::mix, so wind direction goes the short way
// round); the cloud type and the precipitation kind switch at 0.5, but a kind that one side has and the other lacks is
// used for the whole blend (rain fades in from 0 rather than popping in), and the shafts and the lightning are on when
// either side has them.
Look mix(const Look& a, const Look& b, float amount);
// Sets a scene to a look: weather (enabled), clouds, precipitation, ground, lightning, volumetric, aerial, and the sky
// (Sky::at(hour); sunLight 0 and the first light, when it is directional, put where the sun is). The scene keeps its
// objects, lights, camera and water. Pure: the same look makes the same scene.
void apply(g3d::Scene& scene, const Look& look, float fps = 30);
void applyPreset(g3d::Scene& scene, const std::string& name, float fps = 30);
// A blend of two presets as `amount` (0 = a, 1 = b); call it from Scene::up with an amount computed from t.
void applyBlend(g3d::Scene& scene, const std::string& a, const std::string& b, float amount, float fps = 30);

// ---------------------------------------------------------------- water bodies
// How the water moves: Calm a mirror, Breezy a rippled lake, Choppy a bay with waves of a few decimetres.
enum class Surface { Calm, Breezy, Choppy };
struct WaterSpec {
    std::vector<Vector2> shore;         // the outline in x, z (empty: an irregular ellipse of `radius` round `centre`)
    Vector2 centre{ 0, 0 };
    Vector2 radius{ 24, 14 };           // metres, x and z, of the default ellipse
    uint32_t seed = 1;                  // the default outline's irregularity
    float level = 0;                    // surface height, metres
    Surface look = Surface::Calm;
    bool reflections = true;            // g3d::Water::Reflections::Scene, and the bed seen refracted
    std::vector<Vector2> path;          // two or more points: a river along it, not a lake
    float width = 5;                    // river width, metres
    float flow = 1;                     // river speed, m/s
};
// The outline the default ellipse gets: 24 points, radius varied by smooth noise (seed) by up to 25 per cent.
std::vector<Vector2> shoreOutline(Vector2 centre, Vector2 radius, uint32_t seed);
// A lake or river with the look filled in (a g3d::WaterBody::lake or ::river, then the waves and the colours of `look`).
g3d::WaterBody waterBody(const WaterSpec& spec);

// ---------------------------------------------------------------- rain
struct RainSpec {
    float intensity = 0.6f;             // 0..1: drizzle to downpour
    float windSpeed = 7;                // m/s; the wind blows towards `windDirection` (degrees about +y, 0 = +x)
    float windDirection = 17;
    float delay = 0;                    // seconds before it starts
    float ramp = 1;                     // seconds it takes to build to `intensity`
    float wetRate = 60;                 // film seconds are this many weather seconds: how fast the ground soaks (1 is real time)
    float lens = 0;                     // 0..1: raindrops on the camera's glass (0: none)
    float cover = 0.95f;                // the cloud cover the rain falls from
    float fps = 30;                     // the film's frames per second
    uint32_t seed = 1;
};
// The rain's intensity `seconds` into the film: 0 until `delay`, then a smooth rise to `intensity` over `ramp`.
float rainIntensity(const RainSpec& spec, double seconds);
// Rain as one drop-in: sets Scene::weather to rain (its wind, cover and a history that starts the rain at `delay`),
// Scene::precipitation (taken from the weather), Scene::ground (soaking up from that history at `wetRate`, puddles that
// ring), and chains onto Scene::up a step that sets the weather's rain intensity from t (rainIntensity). The sky is the
// scene's own; with weather on it greys with the cover.
void addRain(g3d::Scene& scene, const RainSpec& spec);
// The lens filter for the glass: rain with a rate that follows the same history as the ground. Rate 0 when spec.lens is 0.
fx::Lens rainLens(const RainSpec& spec);

// ---------------------------------------------------------------- lightning
struct StrikeSpec {
    int count = 6;                      // strikes
    float start = 0, span = 10;         // seconds: they fall in [start, start + span), one in each of `count` equal parts
    Vector2 area{ 0, -3000 };           // centre (x, z) of the disc they fall in, metres
    float radius = 1500;
    float cloudBase = 1200;             // y of the cloud base they start above
    float groundY = 0;
    float groundShare = 0.5f;           // the share that reaches the ground (the rest stay in the cloud)
    uint32_t seed = 1;
};
// The strikes, explicit: time, start, end and seed of each. A pure function of the spec: the same spec, the same list.
std::vector<g3d::Lightning::Strike> strikeSchedule(const StrikeSpec& spec);
// Sets Scene::lightning to those strikes (no rate, not following the clouds), enabled.
void addLightning(g3d::Scene& scene, const StrikeSpec& spec);
// The thunder: each strike's time, distance from `listener` (the nearest of its ends and its middle) and the time its sound
// arrives (distance / 343 m/s), sorted by arrival. Works on the schedule alone: no scene to build.
std::vector<g3d::Thunder> thunderCues(const StrikeSpec& spec, Vector3 listener);

// ---------------------------------------------------------------- prefabs
// "weather", "water-body", "rain", "lightning". Safe to call twice.
void registerPrefabs();

} // namespace wx
} // namespace rmtn
