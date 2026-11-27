// weather_prefab_tests.cpp -- W12: the weather presets, the rain, water and lightning helpers, the four prefabs
// ("weather", "water-body", "rain", "lightning") and the script keys that reach them (tests/test_script.py has the
// script side). The first half needs no GL; the second renders the prefabs at 320 x 180 and, with no window, is
// skipped (the run still passes on what the first half checked).
#include "rmtn/raymotion.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>

using namespace rmtn;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static bool throwsWith(const std::function<void()>& f, const char* needle) {
    try { f(); } catch (const std::exception& e) { return std::string(e.what()).find(needle) != std::string::npos; }
    return false;
}

// ---------------------------------------------------------------- no GL
static void presets() {
    const auto& names = wx::presetNames();
    CHECK(names.size() == 8);
    for (const auto& n : names) {
        const wx::Look k = wx::look(n);
        CHECK(k.weather.enabled);
        CHECK(k.hour >= 0 && k.hour <= 24);
        CHECK(k.weather.wetness >= 0 && k.weather.snowCover >= 0);          // pinned, so presets mix without surprises
    }
    CHECK(throwsWith([] { wx::look("hurricane"); }, "choose clear, cumulus day"));
    CHECK(wx::look("Fog-Dawn").hour == wx::look("fog dawn").hour);          // case, '-' and '_' do not matter
    CHECK(wx::look("golden_evening").hour == wx::look("golden evening").hour);

    // Each is coherent: the weather says what the sky and the ground should show.
    const wx::Look clear = wx::look("clear"), day = wx::look("cumulus day"), over = wx::look("overcast"), squall = wx::look("squall"),
                   storm = wx::look("thunderstorm"), snow = wx::look("snowfall"), fog = wx::look("fog dawn"), gold = wx::look("golden evening");
    CHECK(clear.weather.clouds.cover < 0.2f && clear.weather.precipitation.kind == g3d::Precipitation::None);
    CHECK(day.weather.clouds.cover > 0.3f && day.weather.clouds.cover < 0.6f && day.weather.clouds.type == g3d::CloudType::Cumulus);
    CHECK(over.weather.clouds.cover > 0.85f && over.weather.clouds.type == g3d::CloudType::Stratus && over.weather.lightningRate == 0);
    CHECK(squall.weather.precipitation.kind == g3d::Precipitation::Rain && squall.weather.precipitation.intensity > 0.7f);
    CHECK(squall.weather.clouds.cover > 0.9f && squall.weather.wind.speed > 10 && squall.weather.wetness > 0.5f);
    CHECK(storm.weather.lightningRate > 0 && storm.weather.clouds.type == g3d::CloudType::Cumulonimbus && storm.cloudTop > 8000 && storm.gloom > 0.3f);
    CHECK(squall.weather.lightningRate == 0);
    CHECK(snow.weather.precipitation.kind == g3d::Precipitation::Snow && snow.weather.snowCover > 0.5f && snow.weather.wetness == 0);
    CHECK(fog.weather.haze > 0.7f && fog.shafts != g3d::Volumetric::Quality::Off && fog.hour < 8);
    CHECK(gold.hour > 16.5f && gold.hour < 18.5f && gold.weather.rainbow.distantRain > 0.3f);
    CHECK(clear.weather.snowCover == 0 && storm.weather.snowCover == 0);
    std::printf("[wxprefab] presets: %zu named looks, squall rain %.2f cover %.2f, storm top %.0f m\n", names.size(),
                squall.weather.precipitation.intensity, squall.weather.clouds.cover, storm.cloudTop);
}

static void blending() {
    const wx::Look a = wx::look("clear"), b = wx::look("squall");
    const wx::Look m0 = wx::mix(a, b, 0), m1 = wx::mix(a, b, 1), mh = wx::mix(a, b, 0.5f);
    CHECK(m0.weather.precipitation.intensity == 0 && m1.weather.precipitation.intensity == b.weather.precipitation.intensity);
    // rain is rain for the whole blend, from intensity 0: it fades in, it does not switch on halfway
    float worst = 0, last = 0;
    for (int i = 0; i <= 100; i++) {
        const wx::Look k = wx::mix(a, b, i / 100.f);
        if (i > 0) { CHECK(k.weather.precipitation.kind == g3d::Precipitation::Rain); }
        worst = std::max(worst, std::fabs(k.weather.precipitation.intensity - last));
        last = k.weather.precipitation.intensity;
        CHECK(k.weather.wind.speed >= a.weather.wind.speed - 1e-3f && k.weather.wind.speed <= b.weather.wind.speed + 1e-3f);
    }
    CHECK(worst < 0.02f);
    CHECK(std::fabs(mh.hour - 0.5f * (a.hour + b.hour)) < 1e-3f);
    // the shafts and the lightning are on when either end has them
    CHECK(wx::mix(wx::look("clear"), wx::look("fog dawn"), 0.1f).shafts == wx::look("fog dawn").shafts);
    g3d::Scene s;
    wx::applyBlend(s, "clear", "thunderstorm", 0.05f);
    CHECK(s.lightning.enabled && s.weather.lightningRate > 0);
    CHECK(throwsWith([&] { wx::applyBlend(s, "clear", "nope", 0.5f); }, "no weather preset"));
    std::printf("[wxprefab] blend: largest rain step %.4f over 100 steps\n", worst);
}

static void applying() {
    g3d::Scene s;
    s.lights.push_back(g3d::Light::directional({ 1, 1, 1 }));
    wx::applyPreset(s, "squall");
    CHECK(s.weather.enabled && s.clouds.enabled && s.precipitation.enabled && s.sky.enabled && s.aerial.enabled);
    CHECK(s.sky.sunLight == 0 && s.lights[0].position.y > 0);
    wx::applyPreset(s, "clear");
    CHECK(!s.precipitation.enabled && !s.volumetric.enabled());
    wx::applyPreset(s, "fog dawn");
    CHECK(s.volumetric.enabled());
    // the key light follows the sun: evening is lower than noon, and never under the ground
    wx::applyPreset(s, "cumulus day");
    const float noon = Vector3Normalize(Vector3Subtract(s.lights[0].position, s.lights[0].target)).y;
    wx::applyPreset(s, "golden evening");
    const float eve = Vector3Normalize(Vector3Subtract(s.lights[0].position, s.lights[0].target)).y;
    CHECK(eve < noon && eve > 0);
    // pure: the same preset twice makes the same state
    g3d::Scene t, u;
    wx::applyPreset(t, "thunderstorm");
    wx::applyPreset(u, "thunderstorm");
    CHECK(t.weather.wind.speed == u.weather.wind.speed && t.clouds.top == u.clouds.top && t.sky.zenith.r == u.sky.zenith.r);
    std::printf("[wxprefab] light height noon %.2f evening %.2f\n", noon, eve);
}

static void rain() {
    wx::RainSpec spec;
    spec.delay = 2;  spec.ramp = 3;  spec.intensity = 0.8f;
    CHECK(wx::rainIntensity(spec, 0) == 0 && wx::rainIntensity(spec, 2) == 0);
    CHECK(wx::rainIntensity(spec, 5) == spec.intensity && wx::rainIntensity(spec, 50) == spec.intensity);
    float prev = 0;
    for (int i = 0; i <= 60; i++) { const float r = wx::rainIntensity(spec, 2 + 3 * i / 60.0); CHECK(r >= prev); prev = r; }
    g3d::Scene s;
    wx::addRain(s, spec);
    CHECK(s.weather.enabled && s.precipitation.enabled && s.weather.precipitation.kind == g3d::Precipitation::Rain);
    // the ground soaks from the history: dry before the rain, wetter the longer it falls, never past 1
    const float w1 = s.weather.wetnessAt(1), w6 = s.weather.wetnessAt(6), w12 = s.weather.wetnessAt(12), w20 = s.weather.wetnessAt(20);
    CHECK(w1 < 0.001f && w6 > 0.02f && w12 > w6 && w20 >= w12 && w20 <= 1);
    // Scene::up sets the intensity from t alone
    s.up(30, s);                                                    // 1 s: before the delay
    CHECK(s.weather.precipitation.intensity == 0);
    s.up(300, s);
    CHECK(s.weather.precipitation.intensity == spec.intensity);
    s.up(30, s);
    CHECK(s.weather.precipitation.intensity == 0);                  // and back: a frame does not depend on the one before
    spec.lens = 0;
    CHECK(!wx::rainLens(spec).rain.rateAt && wx::rainLens(spec).rain.rate == 0);
    spec.lens = 0.8f;
    const fx::Lens l = wx::rainLens(spec);
    CHECK(l.rain.rateAt && l.rain.rateAt(0) == 0 && l.rain.rateAt(10) > 0.7f && l.rain.rateAt(10) <= 1);
    std::printf("[wxprefab] rain: wetness 1 s %.3f, 6 s %.3f, 12 s %.3f, 20 s %.3f\n", w1, w6, w12, w20);
}

static void water() {
    const auto ring = wx::shoreOutline({ 5, -10 }, { 20, 10 }, 3);
    CHECK(ring.size() == 24);
    CHECK(ring == wx::shoreOutline({ 5, -10 }, { 20, 10 }, 3));
    CHECK(ring != wx::shoreOutline({ 5, -10 }, { 20, 10 }, 4));
    for (const auto& p : ring) {
        const float r = std::hypot((p.x - 5) / 20, (p.y + 10) / 10);
        CHECK(r > 0.74f && r < 1.26f);                              // within 25 per cent of the ellipse
    }
    wx::WaterSpec spec;
    spec.seed = 3;
    g3d::WaterBody lake = wx::waterBody(spec);
    CHECK(!lake.isRiver() && lake.outline.size() == 24 && lake.look.enabled);
    CHECK(lake.contains(0, 0) && !lake.contains(100, 100));
    auto height = [&](wx::Surface s) {
        wx::WaterSpec w;
        w.look = s;
        const g3d::WaterBody b = wx::waterBody(w);
        float lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < 400; i++) { const float h = b.look.height(i * 0.37f, i * 0.23f, 1.5); lo = std::min(lo, h); hi = std::max(hi, h); }
        return hi - lo;
    };
    const float calm = height(wx::Surface::Calm), breezy = height(wx::Surface::Breezy), choppy = height(wx::Surface::Choppy);
    CHECK(calm > 0 && calm < breezy && breezy < choppy);
    CHECK(choppy < 1.0f);                                           // decimetres, not a sea
    std::printf("[wxprefab] water: wave range calm %.3f breezy %.3f choppy %.3f m\n", calm, breezy, choppy);
    wx::WaterSpec off;
    off.reflections = false;
    const g3d::WaterBody dry = wx::waterBody(off);
    CHECK(dry.look.reflections == g3d::Water::Reflections::Off && !dry.look.refraction);
    CHECK(wx::waterBody(spec).look.reflections == g3d::Water::Reflections::Scene);
    wx::WaterSpec river;
    river.path = { { 0, 0 }, { 10, 4 }, { 22, 3 } };
    river.width = 6;  river.flow = 2;
    const g3d::WaterBody r = wx::waterBody(river);
    CHECK(r.isRiver() && r.flowSpeed == 2 && r.width.size() == 1 && r.width[0] == 6);
}

static void lightning() {
    wx::StrikeSpec spec;
    spec.count = 12;  spec.start = 4;  spec.span = 12;  spec.seed = 9;  spec.area = { 100, -2000 };  spec.radius = 800;
    const auto a = wx::strikeSchedule(spec);
    CHECK(a.size() == 12);
    const auto b = wx::strikeSchedule(spec);
    bool same = a.size() == b.size();
    for (size_t i = 0; same && i < a.size(); i++) same = a[i].time == b[i].time && a[i].seed == b[i].seed && a[i].end.x == b[i].end.x;
    CHECK(same);                                                    // a pure function of the spec
    wx::StrikeSpec other = spec;
    other.seed = 10;
    CHECK(wx::strikeSchedule(other)[3].time != a[3].time);
    std::set<uint32_t> seeds;
    int ground = 0;
    for (size_t i = 0; i < a.size(); i++) {
        const float slot = spec.span / spec.count;
        CHECK(a[i].time >= spec.start + slot * (float)i && a[i].time < spec.start + slot * (float)(i + 1));     // one in each part
        CHECK(std::hypot(a[i].start.x - spec.area.x, a[i].start.z - spec.area.y) <= spec.radius + 1e-3f);
        CHECK(a[i].start.y > spec.cloudBase);
        seeds.insert(a[i].seed);
        if (a[i].end.y <= spec.groundY + 1e-3f) ground++;
    }
    CHECK(seeds.size() == a.size());                                // every bolt has its own shape
    wx::StrikeSpec none = spec, all = spec;
    none.groundShare = 0;  all.groundShare = 1;
    for (const auto& s : wx::strikeSchedule(none)) CHECK(s.end.y > none.groundY + 50);         // all stay in the cloud
    for (const auto& s : wx::strikeSchedule(all)) CHECK(s.end.y == all.groundY);
    // thunder: sorted by arrival, a sound at 343 m/s, the nearest of the channel's ends and middle
    const Vector3 ear{ 0, 2, 0 };
    const auto cues = wx::thunderCues(spec, ear);
    CHECK(cues.size() == a.size());
    for (size_t i = 0; i < cues.size(); i++) {
        CHECK(std::fabs(cues[i].arrives - cues[i].strikeTime - cues[i].distance / 343.0) < 1e-3);
        CHECK(Vector3Distance(cues[i].position, ear) == cues[i].distance || std::fabs(Vector3Distance(cues[i].position, ear) - cues[i].distance) < 1e-2f);
        if (i) CHECK(cues[i].arrives >= cues[i - 1].arrives);
        CHECK(cues[i].distance > 1000 && cues[i].distance < 4000);
    }
    // the same numbers g3d::thunderCues gives on a scene that holds the schedule
    g3d::Scene s;
    s.camera.position = ear;
    wx::addLightning(s, spec);
    const auto engine = g3d::thunderCues(s, 30 * 30, 30);
    CHECK(engine.size() == cues.size());
    for (size_t i = 0; i < engine.size() && i < cues.size(); i++) CHECK(std::fabs(engine[i].arrives - cues[i].arrives) < 1e-3);
    std::printf("[wxprefab] lightning: %zu strikes (%d to the ground), first sound at %.2f s\n", a.size(), ground, cues.front().arrives);
}

static void prefabs() {
    wx::registerPrefabs();
    wx::registerPrefabs();                                          // twice is fine
    const auto names = prefab::names();
    for (const char* n : { "weather", "water-body", "rain", "lightning" }) CHECK(std::find(names.begin(), names.end(), n) != names.end());
    Nodes roots;
    for (const char* n : { "weather", "water-body", "rain", "lightning" }) {
        TNode* node = prefab::use(roots, 0, 60, n, Json::parse(R"({"box":[0,0,320,180],"trees":2,"quality":"preview"})"));
        CHECK(node->fn != nullptr && node->start == 0 && node->dur == 60);
    }
    auto bad = [](const char* name, const char* props, const char* needle) {
        Nodes r;
        CHECK(throwsWith([&] { prefab::use(r, 0, 10, name, Json::parse(props)); }, needle));
    };
    bad("weather", R"({"preset":"hurricane"})", "no weather preset");
    bad("weather", R"({"to":"nope"})", "no weather preset");
    bad("weather", R"({"box":[0,0,0,10]})", "positive size");
    bad("weather", R"({"quality":"ultra"})", "quality must be");
    bad("weather", R"({"shafts":"lots"})", "shafts must be");
    bad("weather", R"({"grass":"astroturf"})", "no grass");
    bad("water-body", R"({"look":"stormy"})", "look must be");
    bad("water-body", R"({"type":"ocean"})", "type must be");
    bad("water-body", R"({"shore":[[0,0],[1,1]]})", "at least three");
    bad("water-body", R"({"shore":[[0,0],[1],[2,2]]})", "[x, z]");
    bad("water-body", R"({"type":"river","path":[[0,0]]})", "at least two");
    bad("rain", R"({"from":[1,2]})", "[x, y, z]");
    bad("lightning", R"({"area":[1]})", "[x, z]");
    // the thunder clip is placed where the sound arrives: one node a strike, none past the end
    Nodes withClip;
    TNode* n = prefab::use(withClip, 0, 600, "lightning", Json::parse(R"({"box":[0,0,64,36],"trees":0,"count":5,"span":10,"seed":2,"clip":"thunder.wav"})"));
    CHECK(n->kids.size() >= 3 && n->kids.size() <= 5);
    for (const auto& k : n->kids) { CHECK(k->audio.size() == 1 && k->audio[0].file == "thunder.wav" && k->start > 0 && k->start + k->dur == 600); }
    Nodes noClip;
    CHECK(prefab::use(noClip, 0, 600, "lightning", Json::parse(R"({"box":[0,0,64,36],"trees":0})"))->kids.empty());
    std::printf("[wxprefab] prefabs: 4 registered, %zu thunder clips placed\n", n->kids.size());
}

// ---------------------------------------------------------------- GL
static Image render(Comp& c, int frame) {
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, frame, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}
static Image prefabFrame(const char* name, const std::string& props, int frame, int frames = 300) {
    Comp c;
    c.w = 320; c.h = 180; c.duration = frames; c.background = BLACK;
    prefab::use(c.roots, 0, frames, name, Json::parse(props));
    return render(c, frame);
}
static const std::string kSmall = R"("box":[0,0,320,180],"quality":"preview","trees":4)";
static std::string props(const std::string& extra) { return "{" + kSmall + (extra.empty() ? "" : "," + extra) + "}"; }
static float luma(Color c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }
static Color meanOf(const Image& img, int x0, int y0, int x1, int y1) {
    double r = 0, g = 0, b = 0;
    int n = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) { const Color c = GetImageColor(img, x, y); r += c.r; g += c.g; b += c.b; n++; }
    return { (unsigned char)(r / n), (unsigned char)(g / n), (unsigned char)(b / n), 255 };
}
static long long diff(const Image& a, const Image& b) {
    long long d = 0;
    for (int y = 0; y < a.height; y++) for (int x = 0; x < a.width; x++) {
        const Color p = GetImageColor(a, x, y), q = GetImageColor(b, x, y);
        d += std::abs(p.r - q.r) + std::abs(p.g - q.g) + std::abs(p.b - q.b);
    }
    return d;
}
static void dump(const char* name, const Image& img) {
    if (const char* dir = std::getenv("RMTN_WXPREFAB_DUMP")) { fs::create_directories(dir); ExportImage(img, (fs::path(dir) / name).string().c_str()); }
}

static void renderedPresets() {
    std::vector<Image> shots;
    for (const auto& n : wx::presetNames()) {
        Image img = prefabFrame("weather", props("\"preset\":\"" + n + "\",\"size\":40"), 20);
        std::string file = n;
        std::replace(file.begin(), file.end(), ' ', '_');
        dump((file + ".png").c_str(), img);
        shots.push_back(img);
    }
    for (size_t i = 0; i < shots.size(); i++)
        for (size_t j = i + 1; j < shots.size(); j++) CHECK(diff(shots[i], shots[j]) > 320LL * 180 * 4);       // eight different pictures
    // what the sky shows: the top fifth of the picture. Clear is blue; a squall, an overcast and a snowfall are grey.
    auto sky = [&](size_t i) { return meanOf(shots[i], 0, 0, 320, 36); };
    const Color clear = sky(0), over = sky(2), squall = sky(3), storm = sky(4), snow = sky(5);
    const float clearBlue = (float)clear.b - clear.r;
    const float overB = std::fabs((float)over.b - over.r), squallB = std::fabs((float)squall.b - squall.r),
                stormB = std::fabs((float)storm.b - storm.r), snowB = std::fabs((float)snow.b - snow.r);
    CHECK(clearBlue > 40);
    CHECK(overB < 14 && squallB < 14 && snowB < 14 && stormB < 30);                   // grey (a storm slate-blue): no blue sky under rain
    CHECK(luma(storm) < luma(squall) && luma(storm) < 0.5f * luma(clear));         // the storm sky is the darkest
    CHECK(luma(squall) < luma(clear) * 0.8f);                                         // and darker than a clear one
    std::printf("[wxprefab] sky blue-minus-red: clear %.0f, overcast %.0f, squall %.0f, thunderstorm %.0f, snowfall %.0f; luma clear %.0f squall %.0f\n",
                clearBlue, overB, squallB, stormB, snowB, luma(clear), luma(squall));
    // the ground is wet under rain: the same rain with a ground that never soaks (wetRate 0) looks different
    const Image wet = prefabFrame("rain", props("\"intensity\":0.6,\"wetRate\":600,\"grass\":\"none\",\"size\":30"), 20);
    const Image dry = prefabFrame("rain", props("\"intensity\":0.6,\"wetRate\":0,\"grass\":\"none\",\"size\":30"), 20);
    const float lw = luma(meanOf(wet, 0, 130, 320, 180)), ld = luma(meanOf(dry, 0, 130, 320, 180));
    std::printf("[wxprefab] rain ground luma wet %.1f, dry %.1f\n", lw, ld);
    CHECK(std::fabs(lw - ld) > 4);
    for (auto& i : shots) UnloadImage(i);
    UnloadImage(wet);
    UnloadImage(dry);
}

// A water body in the scene must not take the snow off the ground (the mirrored clouds left the rain's depth map unbound,
// which read as "everything is sheltered").
static void snowWithWater() {
    const std::string bare = "\"preset\":\"snowfall\",\"grass\":\"none\",\"trees\":0,\"size\":40";
    const Image lake = prefabFrame("weather", props(bare + ",\"lake\":true"), 20);
    const Image none = prefabFrame("weather", props(bare + ",\"lake\":false"), 20);
    dump("snow-lake.png", lake);
    dump("snow-nolake.png", none);
    const float a = luma(meanOf(lake, 0, 140, 90, 180)), b = luma(meanOf(none, 0, 140, 90, 180));    // the foreground, left of the lake
    const Image sunless = prefabFrame("weather", props("\"preset\":\"overcast\",\"grass\":\"none\",\"trees\":0,\"size\":40,\"lake\":true"), 20);
    const float bareGround = luma(meanOf(sunless, 0, 140, 90, 180));
    std::printf("[wxprefab] snow ground luma with a lake %.1f, without %.1f, bare ground %.1f\n", a, b, bareGround);
    CHECK(std::fabs(a - b) < 0.1f * b);
    CHECK(a > bareGround * 1.3f);
    UnloadImage(lake); UnloadImage(none); UnloadImage(sunless);
}

static void renderedBlend() {
    const std::string blend = "\"preset\":\"clear\",\"to\":\"squall\",\"start\":60,\"end\":180,\"lake\":false,\"size\":40";
    const Image a = prefabFrame("weather", props(blend), 10), m = prefabFrame("weather", props(blend), 120), z = prefabFrame("weather", props(blend), 200);
    const Image a2 = prefabFrame("weather", props(blend), 10);
    CHECK(diff(a, a2) == 0);                                                     // the same t, the same pixels
    CHECK(diff(a, m) > 320LL * 180 * 8 && diff(m, z) > 320LL * 180 * 8);
    const Image plain = prefabFrame("weather", props("\"preset\":\"clear\",\"lake\":false,\"size\":40"), 10);
    CHECK(diff(a, plain) == 0);                                                  // before `start` it is the first preset
    const Image end = prefabFrame("weather", props("\"preset\":\"squall\",\"lake\":false,\"size\":40"), 10);
    CHECK(diff(z, end) < 320LL * 180 * 3 * 40);                                  // after `end` it is the second (the clouds have drifted on)
    // going in frame order and out of it gives the same frame
    const Image back = prefabFrame("weather", props(blend), 120);
    CHECK(diff(m, back) == 0);
    for (const Image* i : { &a, &m, &z, &a2, &plain, &end, &back }) UnloadImage(*i);
}

static void renderedRainAndLightning() {
    // rain: nothing falls before `delay`, then it does; the glass changes the picture
    const std::string r = "\"intensity\":0.9,\"delay\":2,\"ramp\":1,\"size\":30";
    UnloadImage(prefabFrame("rain", props(r), 10));
    const long long before = g3d::precipitationDropsSubmitted();
    UnloadImage(prefabFrame("rain", props(r), 150));
    const long long after = g3d::precipitationDropsSubmitted();
    CHECK(before == 0 && after > 1000);
    const Image plain = prefabFrame("rain", props(r), 150), glass = prefabFrame("rain", props(r + ",\"lens\":0.8"), 150);
    CHECK(diff(plain, glass) > 320LL * 180 * 4);
    const Image none = prefabFrame("rain", props("\"intensity\":0,\"size\":30"), 150);
    CHECK(diff(plain, none) > 320LL * 180 * 3);
    UnloadImage(plain); UnloadImage(glass); UnloadImage(none);
    std::printf("[wxprefab] rain drops submitted: before the delay %lld, at 5 s %lld\n", before, after);

    // lightning: the picture is brighter during a strike and back to what it was after it
    wx::StrikeSpec spec;
    spec.count = 3;  spec.span = 9;  spec.seed = 5;
    const auto strikes = wx::strikeSchedule(spec);
    const std::string l = "\"count\":3,\"span\":9,\"seed\":5,\"size\":30";
    const int f = (int)std::lround(strikes[1].time * 30);
    Image calm = prefabFrame("lightning", props(l), f - 20), hit = prefabFrame("lightning", props(l), f + 2), after2 = prefabFrame("lightning", props(l), f + 120);
    const float lc = luma(meanOf(calm, 0, 0, 320, 180)), lh = luma(meanOf(hit, 0, 0, 320, 180)), la = luma(meanOf(after2, 0, 0, 320, 180));
    std::printf("[wxprefab] lightning at frame %d: mean luma before %.1f, during %.1f, after %.1f\n", f, lc, lh, la);
    CHECK(lh > lc + 2);                                                          // the flash
    CHECK(std::fabs(la - lc) < lh - lc);                                         // gone again
    dump("lightning-hit.png", hit);
    UnloadImage(calm); UnloadImage(hit); UnloadImage(after2);
}

static void renderedWater() {
    const Image calm = prefabFrame("water-body", props("\"look\":\"calm\""), 20), choppy = prefabFrame("water-body", props("\"look\":\"choppy\""), 20);
    CHECK(diff(calm, choppy) > 320LL * 180 * 2);
    // the shore parameter: a pool near the camera in a different place is a different picture
    const Image near = prefabFrame("water-body", props("\"shore\":[[-8,-6],[8,-6],[8,-16],[-8,-16]]"), 20);
    const Image far = prefabFrame("water-body", props("\"shore\":[[-8,-20],[8,-20],[8,-40],[-8,-40]]"), 20);
    CHECK(diff(near, far) > 320LL * 180 * 4);
    // water is blue-grey where the pool is and not where it is not: the band the near pool fills
    const Color inside = meanOf(near, 130, 100, 190, 120);
    CHECK(inside.b >= inside.r);
    dump("water-calm.png", calm);
    dump("water-choppy.png", choppy);
    const Image river = prefabFrame("water-body", props("\"type\":\"river\""), 20);
    CHECK(diff(river, calm) > 320LL * 180 * 3);
    for (const Image* i : { &calm, &choppy, &near, &far, &river }) UnloadImage(*i);
}

static Image look(g3d::ScenePtr scene, int w, int h, int frame) {
    Comp c;
    c.w = w; c.h = h; c.duration = 600; c.background = BLACK;
    addRoot(c.roots, 0, 600)->fn = g3d::draw(scene, {});
    return render(c, frame);
}

// A thick low deck seen from below, the sun high on the far side of it: the base is dark and nothing shines white. (The march
// lit the underside by a sky that ignored most of the deck above it and the lobes by an unblocked sun, which drew a neon rim
// round every lobe of a storm.)
static g3d::ScenePtr deckScene(bool clouds, float pitchDeg) {
    auto s = std::make_shared<g3d::Scene>();
    const float az = 1.0f, el = 63 * DEG2RAD;
    s->sky.enabled = true;  s->sky.show = true;  s->sky.sunLight = 0;
    s->sky.sunDirection = { std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az) };
    s->ambient = 0.3f;
    const float look = az + PI, p = pitchDeg * DEG2RAD;
    s->camera.position = { 0, 2, 0 };
    s->camera.target = { std::cos(p) * std::cos(look) * 1000, 2 + std::sin(p) * 1000, std::cos(p) * std::sin(look) * 1000 };
    s->camera.fov = 60;  s->camera.farPlane = 400000;
    s->clouds.enabled = clouds;  s->clouds.type = g3d::CloudType::Cumulonimbus;
    s->clouds.base = 1000;  s->clouds.top = 11000;  s->clouds.cover = 1;  s->clouds.density = 3;
    s->clouds.quality = g3d::Clouds::Quality::Film;  s->clouds.shadows = false;
    return s;
}
static void thickDeck() {
    Image sky = look(deckScene(false, 0), 320, 180, 0), deck = look(deckScene(true, 35), 320, 180, 0);
    float horizon = 0, base = 0, peak = 0;
    for (int y = 70; y < 86; y++) for (int x = 0; x < 320; x++) horizon += luma(GetImageColor(sky, x, y));
    horizon /= 16 * 320;
    for (int y = 60; y < 120; y++) for (int x = 110; x < 210; x++) base += luma(GetImageColor(deck, x, y));
    base /= 60 * 100;
    for (int y = 0; y < 180; y++) for (int x = 0; x < 320; x++) peak = std::max(peak, luma(GetImageColor(deck, x, y)));
    dump("deck-below.png", deck);
    dump("deck-sky.png", sky);
    std::printf("[wxprefab] thick deck: horizon sky luma %.1f, base %.1f (%.2f of it), brightest pixel %.1f (%.2f of it)\n", horizon, base, base / horizon, peak, peak / horizon);
    CHECK(base <= 0.5f * horizon);
    CHECK(peak <= 1.2f * horizon);                                   // the view is 100 degrees from the sun: no silver lining
    UnloadImage(sky); UnloadImage(deck);
}
int main() try {
    SetTraceLogLevel(LOG_WARNING);
    presets();
    blending();
    applying();
    rain();
    water();
    lightning();
    prefabs();
    if (failures) { std::printf("[wxprefab] %d failures before GL\n", failures); return 1; }
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 180, "raymotion weather prefabs");
    if (!IsWindowReady()) {
        std::printf("[wxprefab] no window: the rendered checks are skipped\n");
        return 0;
    }
    renderedPresets();
    snowWithWater();
    renderedBlend();
    renderedRainAndLightning();
    renderedWater();
    thickDeck();
    shutdownGraphics();
    CloseWindow();
    std::printf(failures ? "[wxprefab] %d failures\n" : "[wxprefab] all passed\n", failures);
    return failures ? 1 : 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "[wxprefab] exception: %s\n", e.what());
    return 1;
}
