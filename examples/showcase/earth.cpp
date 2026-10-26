// earth.cpp -- "earth": globe:: and cal::, the Earth in 3D. A globe painted from Natural
// Earth turns under drifting clouds; President Prabowo Subianto's trips abroad since he took
// office fly out of Jakarta leg by leg, a desk calendar tearing off the days; a simulated
// outbreak spreads from Dubai as a heat map; the 2026 fires in Sumatra and Kalimantan burn
// close up while their haze drifts to Singapore and Sarawak, the month ticking by; and a
// simulated typhoon winds up over the Philippine Sea. The trips and the haze reports are
// real (dated, sourced below); hotspot positions, the smoke model, the outbreak and the
// typhoon are made up.
//
// Trips: "List of international presidential trips made by Prabowo Subianto", Wikipedia
// (as of September 2026). Haze: BMKG and BNPB hotspot counts, the Ministry of Forestry's
// 1 September count, NEA Singapore's haze updates, and Malaysian press reports of the API
// in Sarawak and school closures (August-September 2026).
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <raymath.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <set>
#include <string>

namespace rmtn {
namespace {

const Color BG = mkColor(0x05070f), INK = mkColor(0xeef0ff), DIM = mkColor(0x8a91b4), AQUA = mkColor(0x47dad8),
            GOLDEN = mkColor(0xffc857), CORAL = mkColor(0xff6b5a);
constexpr float FOV = 30;

void caption(TNode* scene, const char* kicker, const char* title) {
    scene->add(0, scene->dur)->fn = [kicker, title](const Ctx& cx, int t) {
        const float k = (float)interpolate(t, { 0, 14 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        Ctx c = cx;
        c.a *= k;
        dr::rectGrad4(c, 0, 0, 1400, 420, withAlpha(BG, 0.8f), withAlpha(BG, 0), withAlpha(BG, 0), withAlpha(BG, 0));   // a scrim
        dr::text(fonts::ui(false), c, kicker, { 120, 96 + 12 * (1 - k) }, 28, 4, AQUA);
        dr::text(fonts::ui(true), c, title, { 120, 136 + 12 * (1 - k) }, 64, 0, INK);
    };
}

// A scrim behind the counters in the bottom right.
void cornerScrim(const Ctx& c) {
    dr::rectGrad4(c, 1100, 700, 820, 380, withAlpha(BG, 0), withAlpha(BG, 0), withAlpha(BG, 0.8f), withAlpha(BG, 0));
}

// A small note under the caption: where the data comes from, or that it is made up.
void note(TNode* scene, const char* text) {
    scene->add(10, scene->dur - 10)->fn = [text](const Ctx& cx, int t) {
        Ctx c = cx;
        c.a *= (float)interpolate(t, { 0, 14 }, { 0, 1 }, clamped());
        dr::text(code::font(false), c, text, { 122, 226 }, 22, 0, DIM, 0, 0);
    };
}
void simulated(TNode* scene) { note(scene, "simulated data"); }

float smooth01(float x) {
    x = std::clamp(x, 0.f, 1.f);
    return x * x * (3 - 2 * x);
}

unsigned hash(unsigned x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x;
}
float rnd(unsigned a, unsigned b = 0) { return (hash(a * 9781u + b * 6271u + 17u) & 0xffffff) / 16777216.f; }

// Value noise on a plane, 0..1.
float vnoise(float x, float y, unsigned seed) {
    const int ix = (int)std::floor(x), iy = (int)std::floor(y);
    const float fx = x - ix, fy = y - iy, u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    auto h = [seed](int a, int b) { return rnd((unsigned)a * 73856093u ^ (unsigned)b * 19349663u, seed); };
    return (h(ix, iy) * (1 - u) + h(ix + 1, iy) * u) * (1 - v) + (h(ix, iy + 1) * (1 - u) + h(ix + 1, iy + 1) * u) * v;
}

// ---------------------------------------------------------------- the painted Earth

Color biome(double lat, double lon) {
    const float a = (float)std::fabs(lat), n = 1.6f * (vnoise((float)lon / 14, (float)lat / 10, 3) - 0.5f) + 0.6f * (vnoise((float)lon / 4, (float)lat / 4, 4) - 0.5f);
    const Color tropic = mkColor(0x3f6f3a), desert = mkColor(0xc4a774), temperate = mkColor(0x648a4c),
                boreal = mkColor(0x4c6845), tundra = mkColor(0x8f9483);
    const float dry = smooth01((a - 15 + 12 * n) / 5) * (1 - smooth01((a - 31 + 10 * n) / 5));
    Color c = mixC(tropic, temperate, smooth01((a - 25) / 15));
    c = mixC(c, boreal, smooth01((a - 50 + 6 * n) / 8));
    c = mixC(c, tundra, smooth01((a - 64 + 5 * n) / 6));
    return mixC(c, desert, dry * 0.85f);
}

// The Earth's colours, for the whole world or (sharper, for close-ups) one box.
std::shared_ptr<globe::Map> paintEarth(int W, geo::Bounds box = { -180, -90, 180, 90 }) {
    globe::Map land(W, box, BLANK);
    land.fill(geo::land().merged(), WHITE);
    globe::Map m(W, box, BLANK);
    for (int y = 0; y < m.height(); y++)
        for (int x = 0; x < W; x++) {
            const unsigned char a = land.pixel(x, y).a;
            if (!a) continue;
            const globe::LonLat p = m.place(x + 0.5f, y + 0.5f);
            Color c = biome(p.lat, p.lon);
            c.a = a;
            m.pixel(x, y) = c;
        }
    geo::Geometry ice;                                      // Antarctica, Greenland
    for (const auto& f : geo::land().features)
        for (const auto& poly : f.geometry.polygons) {
            double top = -90;
            for (const auto& p : poly[0]) top = std::max(top, p.lat);
            if (top < -55) ice.polygons.push_back(poly);
        }
    ice.append(geo::countries().get("GRL").geometry);
    m.fill(ice, mkColor(0xe8eef2));
    m.fill(geo::lakes().merged(), mkColor(0x24557e));
    m.stroke(geo::countries().merged(), mkColor(0xf4efe0, 110), 1.6f);
    m.graticule(30, mkColor(0xffffff, 22), 1.4f);
    m.gradient(mkColor(0x14304c), mkColor(0x1d5c86), mkColor(0x14304c));
    return std::make_shared<globe::Map>(std::move(m));
}

// The Earth's texture, painted on first use and shared by every scene.
g3d::DynamicTexturePtr earthTexture() {
    static g3d::DynamicTexturePtr tex = [] {
        auto t = std::make_shared<g3d::DynamicTexture>();
        t->make = [](int) {
            static std::shared_ptr<globe::Map> painted;
            static std::once_flag once;
            std::call_once(once, [] { painted = paintEarth(4096); });
            return painted->image();
        };
        t->every = INT_MAX;
        return t;
    }();
    return tex;
}

// ---------------------------------------------------------------- a globe scene

struct View {
    globe::LonLat at;
    float distance;
};
using ViewFn = std::function<View(int t)>;

// The globe lit from the upper left of the view (so the side we look at is in daylight,
// the terminator just showing), the camera above `view(t).at`.
g3d::ScenePtr globeScene(const ViewFn& view) {
    auto scene = std::make_shared<g3d::Scene>();
    scene->ambient = 0.16f;
    scene->skyColor = mkColor(0x9fb8ff);
    scene->groundColor = mkColor(0x101420);
    g3d::Light sun = g3d::Light::directional({ 0, 0, 10 }, mkColor(0xfff4e6), 1.45f);
    sun.up = [view](int t, g3d::Light& self) {
        const View v = view(t);
        self.position = globe::point({ v.at.lon - 48, std::clamp(v.at.lat + 28, -80.0, 80.0) }, 20);
        self.target = { 0, 0, 0 };
    };
    scene->lights.push_back(sun);
    scene->camera.fov = FOV;
    scene->camera.farPlane = 50;
    scene->camera.nearPlane = 0.01f;
    scene->camera.up = [view](int t, g3d::Camera& cam) {
        const View v = view(t);
        globe::look(cam, v.at, v.distance);
    };
    g3d::Material earth;
    earth.roughness = 0.8f;
    earth.dynamicMap = earthTexture();
    earth.castShadow = false;
    scene->add(globe::sphere(1, 192), earth)->name = "earth";
    return scene;
}

// Clouds on a shell just above the ground, remade every `every` frames.
g3d::Object3D* addClouds(g3d::Scene& scene, std::function<globe::Weather(int t)> weather, int width, int every,
                         float opacity = 0.999f) {
    g3d::Material m;
    m.roughness = 1;
    m.opacity = opacity;
    m.castShadow = false;
    m.dynamicMap = std::make_shared<g3d::DynamicTexture>();
    m.dynamicMap->make = [weather, width](int t) { return globe::clouds(width, t / 30.0, weather(t)); };
    m.dynamicMap->every = every;
    g3d::Object3D* o = scene.add(globe::sphere(1.012f, 128), m);
    o->name = "clouds";
    return o;
}

// Stars, and the atmosphere's glow round the limb, drawn before the globe.
void backdrop(TNode* scene, ViewFn view) {
    scene->add(0, scene->dur)->fn = [view](const Ctx& cx, int t) {
        dr::rect(cx, 0, 0, 1920, 1080, BG);
        for (unsigned i = 0; i < 420; i++) {
            const float x = rnd(i, 1) * 1920, y = rnd(i, 2) * 1080, s = 0.6f + 1.6f * rnd(i, 3) * rnd(i, 4);
            const float tw = 0.55f + 0.45f * std::sin(t * 0.05f + i);
            dr::circle(cx, { x, y }, s, withAlpha(mixC(INK, AQUA, rnd(i, 5) * 0.5f), (0.25f + 0.6f * rnd(i, 6)) * tw));
        }
        const float D = view(t).distance;
        const float r = std::tan(std::asin(1 / D)) / std::tan(FOV / 2 * DEG2RAD) * 540;
        for (int k = 14; k >= 1; k--) {
            const float f = k / 14.f;
            dr::circle(cx, { 960, 540 }, r * (1 + 0.075f * f), withAlpha(mkColor(0x4f9dff), 0.05f * (1 - f) + 0.012f));
        }
    };
}

// A 3D point is on the side of the globe facing the camera.
bool facing(const g3d::Camera& cam, Vector3 p) {
    return Vector3DotProduct(Vector3Normalize(p), Vector3Subtract(cam.position, p)) > 0;
}

void pill(const Ctx& c, Vector2 at, const char* text, Color fg, Color bg, float size = 24) {
    const float w = MeasureTextEx(code::font(false), text, size, 0).x + 28;
    dr::roundRect(c, at.x - w / 2, at.y - size * 0.95f, w, size * 1.9f, size * 0.95f, bg);
    dr::text(code::font(false), c, text, at, size, 0, fg, 0.5f, 0.5f);
}

globe::LonLat cityAt(const char* name) { return geo::cities().get(name).labelPoint(); }

View lerpView(View a, View b, float k) {
    return { geo::interpolate(a.at, b.at, k), a.distance + (b.distance - a.distance) * k };
}

// "1234567" -> "1,234,567"
std::string thousands(double v) {
    std::string s = std::to_string((long long)std::llround(v)), out;
    for (size_t i = 0; i < s.size(); i++) {
        if (i && (s.size() - i) % 3 == 0) out += ',';
        out += s[i];
    }
    return out;
}

// ---------------------------------------------------------------- Prabowo's trips

// Every stop of every trip abroad from his inauguration (20 October 2024) to September
// 2026, with the days he arrived and left, from Wikipedia's list of his presidential trips.
struct Stop {
    const char* city;
    const char* country;        // ISO code
    globe::LonLat at;
    const char* arrive;
    const char* leave;
};
using Trip = std::vector<Stop>;

const globe::LonLat JAKARTA{ 106.85, -6.21 };
const cal::Date INAUGURATION{ 2024, 10, 20 };

const std::vector<Trip>& trips() {
    static const std::vector<Trip> t = {
        { { "Beijing", "CN", { 116.40, 39.90 }, "2024-11-08", "2024-11-10" },
          { "Washington", "US", { -77.04, 38.91 }, "2024-11-10", "2024-11-12" },
          { "Lima", "PE", { -77.04, -12.05 }, "2024-11-13", "2024-11-16" },
          { "Rio de Janeiro", "BR", { -43.17, -22.91 }, "2024-11-16", "2024-11-19" },
          { "London", "GB", { -0.13, 51.51 }, "2024-11-20", "2024-11-22" },
          { "Abu Dhabi", "AE", { 54.37, 24.45 }, "2024-11-23", "2024-11-23" } },
        { { "Cairo", "EG", { 31.24, 30.04 }, "2024-12-17", "2024-12-19" } },
        { { "Kuala Lumpur", "MY", { 101.69, 3.14 }, "2025-01-09", "2025-01-09" } },
        { { "New Delhi", "IN", { 77.21, 28.61 }, "2025-01-23", "2025-01-26" },
          { "Kuala Lumpur", "MY", { 101.69, 3.14 }, "2025-01-27", "2025-01-27" } },
        { { "Putrajaya", "MY", { 101.69, 2.93 }, "2025-04-06", "2025-04-06" } },
        { { "Abu Dhabi", "AE", { 54.37, 24.45 }, "2025-04-09", "2025-04-09" },
          { "Ankara", "TR", { 32.86, 39.93 }, "2025-04-09", "2025-04-11" },
          { "Cairo", "EG", { 31.24, 30.04 }, "2025-04-11", "2025-04-12" },
          { "Doha", "QA", { 51.53, 25.29 }, "2025-04-12", "2025-04-13" },
          { "Amman", "JO", { 35.93, 31.95 }, "2025-04-13", "2025-04-14" } },
        { { "Bandar Seri Begawan", "BN", { 114.94, 4.90 }, "2025-05-14", "2025-05-14" } },
        { { "Bangkok", "TH", { 100.50, 13.76 }, "2025-05-17", "2025-05-19" } },
        { { "Kuala Lumpur", "MY", { 101.69, 3.14 }, "2025-05-25", "2025-05-27" } },
        { { "Singapore", "SG", { 103.82, 1.35 }, "2025-06-15", "2025-06-16" },
          { "Prague", "CZ", { 14.42, 50.09 }, "2025-06-17", "2025-06-18" },
          { "St Petersburg", "RU", { 30.31, 59.94 }, "2025-06-18", "2025-06-20" } },
        { { "Jeddah", "SA", { 39.17, 21.54 }, "2025-07-01", "2025-07-03" },
          { "Rio de Janeiro", "BR", { -43.17, -22.91 }, "2025-07-05", "2025-07-07" },
          { "Brasilia", "BR", { -47.88, -15.79 }, "2025-07-08", "2025-07-09" },
          { "Brussels", "BE", { 4.35, 50.85 }, "2025-07-12", "2025-07-13" },
          { "Paris", "FR", { 2.35, 48.86 }, "2025-07-13", "2025-07-15" },
          { "Minsk", "BY", { 27.56, 53.90 }, "2025-07-15", "2025-07-15" } },
        { { "Singapore", "SG", { 103.82, 1.35 }, "2025-08-09", "2025-08-09" } },
        { { "Beijing", "CN", { 116.40, 39.90 }, "2025-09-03", "2025-09-03" } },
        { { "Doha", "QA", { 51.53, 25.29 }, "2025-09-12", "2025-09-12" },
          { "Abu Dhabi", "AE", { 54.37, 24.45 }, "2025-09-12", "2025-09-12" } },
        { { "Osaka", "JP", { 135.50, 34.69 }, "2025-09-20", "2025-09-20" },
          { "New York", "US", { -74.01, 40.71 }, "2025-09-20", "2025-09-24" },
          { "Ottawa", "CA", { -75.70, 45.42 }, "2025-09-24", "2025-09-24" },
          { "Amsterdam", "NL", { 4.90, 52.37 }, "2025-09-25", "2025-09-26" } },
        { { "Sharm El Sheikh", "EG", { 34.33, 27.92 }, "2025-10-13", "2025-10-13" } },
        { { "Kuala Lumpur", "MY", { 101.69, 3.14 }, "2025-10-25", "2025-10-28" } },
        { { "Gyeongju", "KR", { 129.21, 35.86 }, "2025-10-30", "2025-11-01" } },
        { { "Sydney", "AU", { 151.21, -33.87 }, "2025-11-11", "2025-11-12" } },
        { { "Islamabad", "PK", { 73.05, 33.68 }, "2025-12-08", "2025-12-09" },
          { "Moscow", "RU", { 37.62, 55.76 }, "2025-12-10", "2025-12-10" } },
        { { "London", "GB", { -0.13, 51.51 }, "2026-01-18", "2026-01-21" },
          { "Davos", "CH", { 9.84, 46.80 }, "2026-01-21", "2026-01-23" },
          { "Paris", "FR", { 2.35, 48.86 }, "2026-01-23", "2026-01-24" } },
        { { "Washington", "US", { -77.04, 38.91 }, "2026-02-17", "2026-02-21" },
          { "London", "GB", { -0.13, 51.51 }, "2026-02-23", "2026-02-24" },
          { "Amman", "JO", { 35.93, 31.95 }, "2026-02-24", "2026-02-25" },
          { "Abu Dhabi", "AE", { 54.37, 24.45 }, "2026-02-25", "2026-02-26" } },
        { { "Tokyo", "JP", { 139.69, 35.69 }, "2026-03-29", "2026-03-31" },
          { "Seoul", "KR", { 126.98, 37.57 }, "2026-03-31", "2026-04-02" } },
        { { "Moscow", "RU", { 37.62, 55.76 }, "2026-04-13", "2026-04-13" },
          { "Paris", "FR", { 2.35, 48.86 }, "2026-04-13", "2026-04-14" } },
        { { "Lapu-Lapu City", "PH", { 123.95, 10.31 }, "2026-05-07", "2026-05-09" } },
        { { "Paris", "FR", { 2.35, 48.86 }, "2026-05-26", "2026-05-29" } },
        { { "Vladivostok", "RU", { 131.89, 43.12 }, "2026-09-02", "2026-09-04" } },
        { { "New Delhi", "IN", { 77.21, 28.61 }, "2026-09-11", "2026-09-13" } },
    };
    return t;
}

// One flight: out from Jakarta, on between stops, or home.
struct Leg {
    globe::LonLat a, b;
    const char* from;
    const char* to;
    const char* country;        // where it lands (nullptr: home)
    int trip;
    double day;                 // the day it lands
    double km;
    int start, end;             // frames in the scene
};

constexpr int TRIPS_FROM = 34, TRIPS_TAIL = 60;

// The legs laid out in frames: each flight takes 4 to 9 frames by its length; the days
// between trips go by in 2 to 10 frames, so the calendar races through the quiet weeks.
// The clock is at a half day as each plane lands, before the page starts to turn.
struct Travels {
    std::vector<Leg> legs;
    cal::Clock clock;
    std::vector<int> tripStart, tripEnd;
    int dur = 0;
    std::vector<View> view;     // per frame: a camera that follows the plane, smoothed
};

const Travels& travels() {
    static const Travels all = [] {
        Travels r;
        const double inaug = (double)cal::days(INAUGURATION);
        r.clock.key(0, inaug).key(TRIPS_FROM, inaug + 0.3);
        int f = TRIPS_FROM;
        double prev = inaug;
        for (int ti = 0; ti < (int)trips().size(); ti++) {
            const Trip& trip = trips()[ti];
            const double depart = (double)cal::days(cal::parse(trip.front().arrive));
            f += std::clamp(2 + (int)((depart - prev) / 10), 2, 10);
            r.clock.key(f, depart);
            r.tripStart.push_back(f);
            globe::LonLat from = JAKARTA;
            const char* fromName = "Jakarta";
            for (size_t k = 0; k <= trip.size(); k++) {
                const bool home = k == trip.size();
                Leg l;
                l.a = from;
                l.b = home ? JAKARTA : trip[k].at;
                l.from = fromName;
                l.to = home ? "Jakarta" : trip[k].city;
                l.country = home ? nullptr : trip[k].country;
                l.trip = ti;
                l.day = (double)cal::days(cal::parse(home ? trip.back().leave : trip[k].arrive));
                l.km = geo::distance(l.a, l.b);
                l.start = f;
                l.end = f + std::clamp(4 + (int)std::lround(geo::angle(l.a, l.b) / 30), 4, 9);
                r.legs.push_back(l);
                r.clock.key(l.end, l.day + 0.5);
                f = l.end;
                from = l.b;
                fromName = l.to;
            }
            r.tripEnd.push_back(f);
            prev = (double)cal::days(cal::parse(trip.back().leave));
        }
        r.dur = f + TRIPS_TAIL;
        // Where the plane is each frame (in Jakarta between trips), then a Gaussian blur of
        // that over time; the more the blurred direction is spread round the globe, the
        // further out the camera goes.
        std::vector<Vector3> at(r.dur, globe::point(JAKARTA));
        for (const Leg& l : r.legs)
            for (int t = l.start; t <= l.end && t < r.dur; t++)
                at[t] = globe::point(geo::interpolate(l.a, l.b, (t - l.start) / (double)(l.end - l.start)));
        const float sigma = 30;
        for (int t = 0; t < r.dur; t++) {
            Vector3 m{ 0, 0, 0 };
            for (int d = -90; d <= 90; d++) {
                const int u = std::clamp(t + d, 0, r.dur - 1);
                m = Vector3Add(m, Vector3Scale(at[u], std::exp(-0.5f * d * d / (sigma * sigma))));
            }
            const float norm = sigma * std::sqrt(2 * PI), spread = 1 - Vector3Length(m) / norm;
            globe::LonLat p = globe::lonLat(m);
            p.lat = std::clamp(p.lat * 0.65 + 8, -12.0, 40.0);
            r.view.push_back({ p, std::min(6.2f, 4.1f + 4.0f * spread) });
        }
        return r;
    }();
    return all;
}

const globe::LonLat DUBAI = { 55.30, 25.27 };              // where the made-up outbreak starts

// ---------------------------------------------------------------- the outbreak

constexpr int OUT_FROM = 20;
constexpr double DAYS_PER_FRAME = 0.42;

struct Outbreak {
    std::vector<globe::LonLat> at;
    std::vector<double> pop, arrival;           // arrival: day the outbreak reaches each city
};

// Spread by land at about 700 km a day from every city it has reached, and by air between
// the 45 biggest cities (a few days, more for long hauls).
const Outbreak& outbreak() {
    static Outbreak o = [] {
        Outbreak r;
        for (const auto& f : geo::cities().features) {
            r.at.push_back(f.labelPoint());
            r.pop.push_back(std::max(1e5, f.properties["pop"].asNumber()));
        }
        const size_t n = r.at.size();
        r.arrival.assign(n, 1e9);
        size_t origin = 0;
        double best = 1e9;
        for (size_t i = 0; i < n; i++) {
            const double d = geo::distance(r.at[i], DUBAI);
            if (d < best) { best = d; origin = i; }
        }
        std::vector<size_t> byPop(n);
        for (size_t i = 0; i < n; i++) byPop[i] = i;
        std::sort(byPop.begin(), byPop.end(), [&](size_t a, size_t b) { return r.pop[a] > r.pop[b]; });
        std::vector<bool> airport(n, false);
        for (size_t k = 0; k < std::min<size_t>(45, n); k++) airport[byPop[k]] = true;
        airport[origin] = true;
        std::vector<bool> done(n, false);
        r.arrival[origin] = 0;
        for (size_t step = 0; step < n; step++) {
            size_t u = n;
            for (size_t i = 0; i < n; i++)
                if (!done[i] && (u == n || r.arrival[i] < r.arrival[u])) u = i;
            if (u == n || r.arrival[u] >= 1e9) break;
            done[u] = true;
            for (size_t v = 0; v < n; v++) {
                if (done[v]) continue;
                const double km = geo::distance(r.at[u], r.at[v]);
                double cost = km / 700 + 4 * rnd((unsigned)u, (unsigned)v);
                if (airport[u] && airport[v]) cost = std::min(cost, 6 + km / 4000 + 8 * rnd((unsigned)v, 7));
                r.arrival[v] = std::min(r.arrival[v], r.arrival[u] + cost);
            }
        }
        return r;
    }();
    return o;
}

float growth(double day, double arrival) { return smooth01((float)((day - arrival) / 30)); }

// ---------------------------------------------------------------- fires and haze, 2026

// Fire seasons in Indonesia's peatlands: South Sumatra burned first (late August), then
// West and Central Kalimantan (September). Southerly and south-easterly winds carried the
// smoke to Singapore, Peninsular Malaysia and Sarawak. Activity per province follows the
// reported hotspot counts; where each hotspot sits is made up (scattered over the peat
// districts), and so is the smoke, a plume blown downwind from each district.
const geo::Bounds HAZE_BOX{ 95, -8, 121, 8 };
const cal::Date HAZE_FROM{ 2026, 8, 15 };

struct Key {
    float x, y;
};

struct Province {
    const char* name;
    std::vector<globe::LonLat> districts;
    int dots;                                       // hotspots at full activity
    std::vector<Key> activity;                      // (days after 15 Aug, 0..1)
};

const std::vector<Province>& provinces() {
    static const std::vector<Province> p = {
        { "South Sumatra", { { 105.6, -3.3 }, { 104.6, -2.4 }, { 103.9, -2.6 } }, 240,
          { { 0, .12f }, { 6, .4f }, { 12, .7f }, { 16, 1 }, { 18, .45f }, { 21, .35f }, { 26, .3f }, { 31, .35f }, { 36, .2f }, { 41, .12f } } },
        { "Riau", { { 102.5, 0.2 }, { 101.6, 1.5 } }, 70, { { 0, .25f }, { 6, .45f }, { 12, .3f }, { 16, .12f }, { 17, .03f }, { 41, .05f } } },
        { "Jambi", { { 103.8, -1.4 }, { 103.3, -1.2 } }, 60, { { 0, .2f }, { 6, .4f }, { 16, .1f }, { 17, .02f }, { 41, .04f } } },
        { "West Kalimantan", { { 110.2, -1.9 }, { 109.5, -0.5 }, { 110.6, 0.1 } }, 230,
          { { 0, .04f }, { 8, .15f }, { 12, .35f }, { 16, .7f }, { 17, 1 }, { 21, .95f }, { 26, .8f }, { 31, .75f }, { 36, .55f }, { 41, .45f } } },
        { "Central Kalimantan", { { 114.0, -2.8 }, { 114.4, -2.3 }, { 112.9, -2.5 } }, 200,
          { { 0, .04f }, { 10, .18f }, { 14, .5f }, { 17, .9f }, { 21, .95f }, { 26, .85f }, { 31, .7f }, { 36, .5f }, { 41, .4f } } },
        { "South Kalimantan", { { 115.0, -3.0 } }, 50, { { 0, 0 }, { 14, .1f }, { 17, .15f }, { 26, .2f }, { 41, .1f } } },
    };
    return p;
}

float keyed(const std::vector<Key>& keys, float x) {
    if (x <= keys.front().x) return keys.front().y;
    for (size_t i = 1; i < keys.size(); i++)
        if (x <= keys[i].x) {
            const Key& a = keys[i - 1];
            const Key& b = keys[i];
            return a.y + (b.y - a.y) * (x - a.x) / (b.x - a.x);
        }
    return keys.back().y;
}

// Where the smoke goes (degrees clockwise from north) through the season: north-west in
// August, round to the north for Sarawak and Singapore in early September, west-north-west
// mid-month (Kalimantan's smoke to Singapore), then north again.
float windTo(float since) {
    static const std::vector<Key> k = {
        { 0, -45 }, { 14, -40 }, { 17, -15 }, { 24, -20 }, { 27, -62 }, { 33, -64 }, { 36, -22 }, { 41, -18 } };
    return keyed(k, since) + 8 * std::sin(since * 1.3f) + 4 * std::sin(since * 3.1f);
}

globe::LonLat move(globe::LonLat p, float headingDeg, double km) {
    const double h = headingDeg * DEG2RAD;
    return { p.lon + km * std::sin(h) / (111.32 * std::cos(p.lat * DEG2RAD)), p.lat + km * std::cos(h) / 111.32 };
}

struct Hotspot {
    globe::LonLat at;
    int province;
    float threshold;            // burning while the province's activity is above this
};

// Hotspots scattered about the districts, on land only.
const std::vector<Hotspot>& hotspots() {
    static const std::vector<Hotspot> h = [] {
        globe::Map land(520, HAZE_BOX);
        land.fill(geo::land().merged(), WHITE);
        std::vector<Hotspot> r;
        unsigned seed = 1;
        for (int p = 0; p < (int)provinces().size(); p++) {
            const Province& pr = provinces()[p];
            for (int i = 0; i < pr.dots; i++) {
                const globe::LonLat c = pr.districts[i % pr.districts.size()];
                for (int tries = 0; tries < 40; tries++) {
                    seed++;
                    const float u = std::max(1e-6f, rnd(seed, 1)), v = rnd(seed, 2);
                    const float g = std::sqrt(-2 * std::log(u)) * 0.38f;
                    const globe::LonLat q{ c.lon + g * std::cos(2 * PI * v), c.lat + g * std::sin(2 * PI * v) };
                    if (land.at(q).a < 200) continue;
                    r.push_back({ q, p, rnd(seed, 3) });
                    break;
                }
            }
        }
        return r;
    }();
    return h;
}

Image hotspotImage(double day) {
    const float since = (float)(day - cal::days(HAZE_FROM));
    globe::Map m(1040, HAZE_BOX);
    const Color glow = mkColor(0xff5a1f, 70), core = mkColor(0xffd36b);
    for (size_t i = 0; i < hotspots().size(); i++) {
        const Hotspot& h = hotspots()[i];
        const float a = keyed(provinces()[h.province].activity, since);
        if (h.threshold >= a) continue;
        const float flick = 0.7f + 0.3f * rnd((unsigned)i, (unsigned)std::floor(day * 3));
        m.disc(h.at, 9 + 5 * flick, glow, 0.9f);
        m.disc(h.at, 3.2f, withAlpha(core, flick), 0.4f);
    }
    return m.image();
}

// Smoke: from each district a plume of puffs downwind, each puff carrying what burned when
// it left (the wind takes a day or so to cross the region), wider the further it goes;
// broken up by noise that drifts with the day.
Image smokeImage(double day) {
    const float since = (float)(day - cal::days(HAZE_FROM));
    globe::Field f(416, HAZE_BOX);
    constexpr int PUFFS = 18;
    constexpr double STEP = 50, KM_PER_DAY = 520;
    for (const Province& pr : provinces()) {
        const float weight = pr.dots / 230.f / (float)pr.districts.size() * 1.6f;
        for (const globe::LonLat& d : pr.districts) {
            globe::LonLat p = d;
            for (int k = 0; k < PUFFS; k++) {
                const float left = since - (float)(k * STEP / KM_PER_DAY);
                const float a = keyed(pr.activity, left);
                const double radius = 45 + k * STEP * 0.33;   // thick over the fires, thinning as it spreads
                if (a > 0.02f) f.add(p, radius, weight * a * 1.5f * std::sqrt(45 / (float)radius) * (1 - k / (PUFFS + 3.f)));
                p = move(p, windTo(left), STEP);
            }
        }
    }
    Image img = GenImageColor(f.width(), f.height(), BLANK);
    Color* px = (Color*)img.data;
    const Color thin = mkColor(0xcfc4b0), thick = mkColor(0x8a7a66);
    const float drift = (float)day * 0.35f;
    for (int y = 0; y < f.height(); y++)
        for (int x = 0; x < f.width(); x++) {
            const float v = f.values()[(size_t)y * f.width() + x];
            if (v < 0.01f) continue;
            const float n = 0.6f * vnoise(x / 22.f - drift, y / 22.f, 21) + 0.4f * vnoise(x / 7.f, y / 7.f + drift, 22);
            const float d = v * (0.45f + 1.1f * n);
            Color c = mixC(thin, thick, smooth01(d / 2.2f));
            c.a = (unsigned char)(215 * smooth01(d / 1.3f));
            px[(size_t)y * f.width() + x] = c;
        }
    return img;
}

struct Report {
    cal::Date date;
    globe::LonLat at;
    const char* place;
    const char* text;
    const char* source;
};

const std::vector<Report>& reports() {
    static const std::vector<Report> r = {
        { { 2026, 8, 21 }, { 104.6, -2.8 }, "South Sumatra", "733 hotspots in Sumatra, 377 of them here", "BMKG" },
        { { 2026, 8, 31 }, { 104.9, -3.0 }, "South Sumatra", "1,013 hotspots", "BNPB" },
        { { 2026, 9, 1 }, { 111.8, -1.6 }, "Kalimantan", "570 hotspots in the West, 488 in Central", "Forestry Ministry" },
        { { 2026, 9, 4 }, { 110.57, 1.17 }, "Serian, Sarawak", "API 514, hazardous; Kuching 313", "Malaysian press" },
        { { 2026, 9, 5 }, { 103.82, 1.35 }, "Singapore", "PSI 78-104 as southerly winds bring smoke", "NEA" },
        { { 2026, 9, 7 }, { 110.34, 1.55 }, "Sarawak", "647 schools closed", "Malaysian press" },
        { { 2026, 9, 15 }, { 103.82, 1.35 }, "Singapore", "PSI 154 islandwide, the first since 2019", "NEA" },
        { { 2026, 9, 24 }, { 110.57, 1.17 }, "Serian, Sarawak", "API 221; schools close again", "Malaysian press" },
    };
    return r;
}

// An air-quality reading shown by a city from one day to the next.
struct Reading {
    globe::LonLat at;
    cal::Date from, until;
    const char* text;
    Color color;
};

const Color HAZARDOUS = mkColor(0x9b1d33), VERY_UNHEALTHY = mkColor(0x7b3fa0);

const std::vector<Reading>& readings() {
    static const std::vector<Reading> r = {
        { { 110.57, 1.17 }, { 2026, 9, 4 }, { 2026, 9, 23 }, "API 514", HAZARDOUS },
        { { 110.57, 1.17 }, { 2026, 9, 24 }, { 2026, 12, 31 }, "API 221", VERY_UNHEALTHY },
        { { 110.34, 1.55 }, { 2026, 9, 4 }, { 2026, 9, 14 }, "API 313", HAZARDOUS },
        { { 103.82, 1.35 }, { 2026, 9, 5 }, { 2026, 9, 14 }, "PSI 104", CORAL },
        { { 103.82, 1.35 }, { 2026, 9, 15 }, { 2026, 12, 31 }, "PSI 154", CORAL },
    };
    return r;
}

struct Town {
    const char* name;
    globe::LonLat at;
    bool right;                 // label on the right of the dot
};

const Town TOWNS[] = {
    { "Singapore", { 103.82, 1.35 }, true }, { "Kuala Lumpur", { 101.69, 3.14 }, false },
    { "Pekanbaru", { 101.45, 0.51 }, false }, { "Jambi", { 103.61, -1.61 }, false },
    { "Palembang", { 104.76, -2.99 }, false }, { "Pontianak", { 109.34, -0.03 }, false },
    { "Kuching", { 110.34, 1.55 }, false }, { "Serian", { 110.57, 1.17 }, true },
    { "Palangka Raya", { 113.92, -2.21 }, true }, { "Bandar Seri Begawan", { 114.94, 4.90 }, true },
    { "Jakarta", { 106.85, -6.21 }, true },
};

// The season's clock: two weeks of August quickly, the first half of September slowly
// (where the reports bunch up), then on to the 24th.
constexpr int HAZE_START = 44, HAZE_DUR = 450;
cal::Clock hazeClock() {
    cal::Clock c;
    const double d0 = (double)cal::days(HAZE_FROM);
    c.key(HAZE_START, d0).key(130, d0 + 16.3).key(330, d0 + 32.3).key(410, d0 + 40.3);
    return c;
}

int frameOf(const cal::Clock& c, double day) {
    for (int f = 0; f < 10000; f++)
        if (c.day(f) >= day) return f;
    return 10000;
}

// ---------------------------------------------------------------- the typhoon

const globe::LonLat STORM_FROM{ 139.5, 11.0 }, STORM_TO{ 127.5, 15.6 }, TRACK_END{ 119.0, 20.5 };
constexpr int TRACK_DRAW = 40;

globe::Weather typhoon(int t, int dur) {
    globe::Weather w;
    w.cover = 0.42f;
    w.seed = 5;
    const float k = smooth01((float)t / dur);
    w.storms.push_back({ geo::interpolate(STORM_FROM, STORM_TO, k), 820, 0.55f + 0.45f * k, false });
    return w;
}

// ---------------------------------------------------------------- the score

const std::string SCORE = "out/earth-assets-v2/score.wav";

struct Cues {
    int s2 = 0, s3 = 0, s4 = 0, s5 = 0, end = 0, s5dur = 0;
};

// 84 bpm on Am-F-C-G: a wide pad and slow bass throughout, a soft pulse once the flights
// begin, darker (filtered, no pulse) under the outbreak and the haze. Effects: a tick per
// take-off and a pluck per landing, climbing through each trip, a low bell for each return
// home; a tick for every ten days of the outbreak with a low swell as it spreads; under the
// haze a tick per day, crackle as the fires grow and a sting for each report; wind,
// thunder and a deep swell for the typhoon.
void genScore(const std::string& path, const Cues& q) {
    using procgen::Score;
    const double fps = 30, end = q.end / fps;
    Score s(end + 0.5);
    struct Chord { int root; int pad[3]; int arp[4]; };
    const Chord am{ 45, { 57, 60, 64 }, { 69, 72, 76, 72 } }, f{ 41, { 57, 60, 65 }, { 69, 72, 77, 72 } },
                c{ 48, { 55, 60, 64 }, { 67, 72, 76, 72 } }, g{ 43, { 55, 59, 62 }, { 67, 71, 74, 71 } };
    const Chord prog[4] = { am, f, c, g };
    const double bar = 4 * 60.0 / 84, beat = bar / 4, stop = end - 2.6;
    auto sec = [](int frame) { return frame / 30.0; };
    int i = 0;
    for (double t = 0; t < stop; t += bar, i++) {
        const Chord& ch = prog[i % 4];
        const bool flights = t >= sec(q.s2) - 0.1 && t < sec(q.s3) - 0.1;
        const bool dark = t >= sec(q.s3) - 0.1 && t < sec(q.s5) - 0.1;
        const bool storm = t >= sec(q.s5) - 0.1;
        for (int p : ch.pad) s.pad(t, bar * 0.97, p, 0.075f, dark ? 700 : storm ? 1400 : 1100, 0.5f);
        s.pad(t, bar * 0.97, ch.pad[0] - 12, 0.05f, 600, 0.5f);
        s.bass(t, bar * 0.48, ch.root, 0.16f);
        s.bass(t + 2 * beat, bar * 0.48, ch.root, 0.12f);
        if (flights || storm) {
            s.kick(t, 0.28f);
            s.kick(t + 2 * beat, 0.22f);
            for (int h = 0; h < 8; h++) s.hat(t + h * beat / 2, h % 2 ? 0.035f : 0.02f, h % 2 ? 0.35f : 0.65f);
        }
        if (!dark)
            for (int e = 0; e < 4; e++) s.pluck(t + e * beat, ch.arp[e] + (storm ? 0 : 12), 0.02f, e % 2 ? 0.3f : 0.7f, 6);
        if (dark) s.kick(t, 0.18f);
    }
    for (int at : { q.s2, q.s3, q.s4, q.s5 }) s.riser(sec(at) - 0.7, sec(at), 0.035f);
    s.bell(0.4, 81, 0.06f, 0.5f);
    // Trips.
    const int climb[] = { 0, 2, 3, 5, 7, 8, 10 };
    int within = 0;
    for (const Leg& l : travels().legs) {
        within = l.a.lon == JAKARTA.lon && l.a.lat == JAKARTA.lat ? 0 : within + 1;
        s.tick(sec(q.s2 + l.start), 0.02f);
        if (l.country) s.pluck(sec(q.s2 + l.end), 69 + climb[std::min(within, 6)], 0.035f, 0.25f + 0.5f * (l.trip % 2), 5);
        else s.bell(sec(q.s2 + l.end), 57, 0.03f, 0.5f);
    }
    // Outbreak: ticks every ten days, a swell as it takes off.
    for (int day = 10; day < 140; day += 10) {
        const int frame = OUT_FROM + (int)(day / DAYS_PER_FRAME);
        if (frame >= q.s4 - q.s3) break;
        s.tick(sec(q.s3 + frame), 0.04f);
        s.pluck(sec(q.s3 + frame), 57 - (day / 10) % 3, 0.04f, 0.5f, 3);
    }
    s.riser(sec(q.s3 + 120), sec(q.s3 + 260), 0.03f);
    // Haze: a tick as each day comes up, crackle with the fires, a sting for each report.
    {
        const cal::Clock clock = hazeClock();
        for (int fr = HAZE_START + 1; fr < q.s5 - q.s4; fr++) {
            if (std::floor(clock.day(fr)) > std::floor(clock.day(fr - 1))) s.tick(sec(q.s4 + fr), 0.018f);
            float burning = 0;
            for (const Province& pr : provinces()) burning += keyed(pr.activity, (float)(clock.day(fr) - cal::days(HAZE_FROM))) * pr.dots;
            const int pops = std::min(5, (int)(burning / 140));
            for (int k = 0; k < pops; k++)
                if (rnd((unsigned)fr, (unsigned)k) < 0.5f)
                    s.hat(sec(q.s4 + fr) + rnd((unsigned)fr, (unsigned)k + 9) / 30, 0.01f + 0.015f * rnd((unsigned)k, (unsigned)fr), rnd((unsigned)fr * 3, (unsigned)k));
        }
        s.pad(sec(q.s4 + HAZE_START), sec(q.s5 - q.s4 - HAZE_START), 33, 0.07f, 220, 0.5f);
        int k = 0;
        for (const Report& r : reports()) {
            const double at = sec(q.s4 + frameOf(clock, (double)cal::days(r.date)));
            s.impact(at, 0.035f);
            s.bell(at, (k % 2 ? 64 : 60) + 12, 0.045f, 0.3f + 0.4f * ((k + 1) % 2));
            k++;
        }
    }
    // Typhoon: wind in gusts, thunder, a deep swell as it strengthens.
    for (int k = 0; k < 6; k++) {
        const double t0 = sec(q.s5) + k * q.s5dur / 6.0 / fps;
        s.riser(t0, t0 + q.s5dur / 6.0 / fps * 0.9, 0.02f + 0.008f * k);
    }
    for (int k = 0; k < 4; k++) s.impact(sec(q.s5) + (40 + k * 55 + 20 * rnd((unsigned)k, 3)) / fps, 0.05f + 0.015f * k);
    s.bell(sec(q.s5 + TRACK_DRAW), 76, 0.05f, 0.4f);
    // The end: a held A minor with bells.
    for (int p : am.pad) { s.pad(stop, 2.6, p, 0.1f, 1600, 0.35f); s.pad(stop, 2.6, p - 12, 0.07f, 1000, 0.65f); }
    s.bass(stop, 2.6, am.root - 12, 0.2f);
    for (int k = 0; k < 4; k++) s.bell(stop + k * 0.4, am.arp[k] + 12, 0.06f, 0.2f + 0.2f * k);
    media::writeWav(path, s.mix(), Score::RATE);
}

} // namespace

void registerEarth(Registry& out) {
    Comp comp;
    comp.name = "earth";
    comp.w = 1920; comp.h = 1080; comp.fps = 30;
    comp.background = BG;
    TransitionSeries ts(comp.roots, (float)comp.w, (float)comp.h);
    auto scene = [&](int dur, ViewFn view) {
        TNode* s = ts.add(dur);
        backdrop(s, view);
        return s;
    };

    // 1. The Earth, painted on the CPU from the bundled Natural Earth layers, turning under
    // clouds that drift with the winds (trade winds west, westerlies east).
    ViewFn v1 = [](int t) {
        const float k = smooth01(t / 240.f);
        return View{ { 10 + 50 * k, 18 + 6 * k }, 5.6f - 0.6f * k };
    };
    TNode* s1 = scene(240, v1);
    {
        auto g = globeScene(v1);
        addClouds(*g, [](int) { globe::Weather w; w.cover = 0.38f; return w; }, 1024, 2);
        s1->add(0, 240)->fn = g3d::draw(g);
        caption(s1, "GLOBE::SPHERE, GLOBE::MAP, GLOBE::CLOUDS", "the Earth, from Natural Earth");
    }

    // 2. President Prabowo Subianto's trips abroad since his inauguration: every leg from
    // Jakarta and back along great circles, older trips fading, the camera trailing the
    // plane, and a desk calendar tearing off the days as they go.
    ts.transition(Transition::fade(16));
    const int s2dur = travels().dur;
    ViewFn v2 = [](int t) { return travels().view[std::clamp(t, 0, travels().dur - 1)]; };
    TNode* s2 = scene(s2dur, v2);
    {
        auto g = globeScene(v2);
        g3d::Material dot;
        dot.unlit = true;
        dot.color = GOLDEN;
        g3d::Material homeDot = dot;
        homeDot.color = AQUA;
        g->add(g3d::Geometry::sphere(0.012f, 12, 16), homeDot)->position = globe::point(JAKARTA, 1.002f);
        // A dot at each city, from the first landing there.
        std::set<std::string> seen;
        for (const Leg& l : travels().legs) {
            if (!l.country || !seen.insert(l.to).second) continue;
            g3d::Object3D* d = g->add(g3d::Geometry::sphere(0.0075f, 10, 14), dot);
            d->position = globe::point(l.b, 1.002f);
            const int from = l.end - 1;
            d->up = [from](int t, g3d::Object3D& self) { self.visible = t >= from; };
        }
        const Color home = mixC(GOLDEN, AQUA, 0.65f);
        for (const Leg& l : travels().legs) {
            const int start = l.start, end = l.end, done = travels().tripEnd[l.trip];
            globe::RouteStyle st;
            st.width = 0.0038f;
            st.color = l.country ? GOLDEN : home;
            st.segments = std::clamp(12 + (int)(geo::angle(l.a, l.b) / 3), 12, 64);
            g3d::Object3D* r = globe::route(*g, l.a, l.b, [start, end](int t) {
                return (float)interpolate(t, { (double)start, (double)end }, { 0, 1 }, clamped(ease::inOut(ease::quad)));
            }, st);
            // Once the trip is over its legs dim, so the latest one stands out.
            const Color bright = st.color, dim = mixC(st.color, BG, 0.62f);
            r->up = [done, bright, dim](int t, g3d::Object3D& self) {
                const Color c = mixC(bright, dim, smooth01((t - done) / 24.f));
                for (auto& seg : self.children) seg->material.color = c;
            };
        }
        g3d::Material plane;
        plane.color = WHITE;
        plane.emissive = mkColor(0x808080);
        g3d::Object3D* p = g->add();
        g3d::Object3D* body = p->add(g3d::Geometry::cone(0.011f, 0.04f, 12), plane);
        body->rotation = g3d::lookRotation({ 0, -1, 0 }, { 0, 0, 1 });   // apex (+y) forward (+z)
        p->up = [](int t, g3d::Object3D& self) {
            const Travels& tr = travels();
            self.visible = false;
            for (const Leg& l : tr.legs) {
                if (t < l.start || t >= l.end) continue;
                const float u = (float)interpolate(t, { (double)l.start, (double)l.end }, { 0, 1 }, clamped(ease::inOut(ease::quad)));
                globe::fly(self, l.a, l.b, u);
                self.visible = true;
            }
        };
        s2->add(0, s2dur)->fn = g3d::draw(g);
        // Names as the plane lands, with a ring spreading out (near side only); Jakarta always.
        s2->add(0, s2dur)->fn = [g](const Ctx& cx, int t) {
            const Travels& tr = travels();
            const g3d::Camera cam = g3d::cameraAt(g->camera, t);
            Vector2 at;
            const Vector3 jp = globe::point(JAKARTA, 1.0f);
            if (facing(cam, jp) && g3d::project(cam, jp, { 0, 0, 1920, 1080 }, at)) {
                Ctx c = cx;
                c.a *= (float)interpolate(t, { 4, 18 }, { 0, 1 }, clamped());
                pill(c, { at.x, at.y + 34 }, "Jakarta", BG, AQUA, 24);
            }
            // Newest first; an older name too close to a newer one (Putrajaya, Kuala Lumpur) gives way.
            std::vector<Vector2> taken;
            for (auto it = tr.legs.rbegin(); it != tr.legs.rend(); ++it) {
                const Leg& l = *it;
                if (!l.country || t < l.end || t >= l.end + 48) continue;
                const Vector3 q = globe::point(l.b, 1.0f);
                if (!facing(cam, q) || !g3d::project(cam, q, { 0, 0, 1920, 1080 }, at)) continue;
                if (std::any_of(taken.begin(), taken.end(), [at](Vector2 o) { return std::fabs(o.x - at.x) < 150 && std::fabs(o.y - at.y) < 34; }))
                    continue;
                taken.push_back(at);
                const float age = (float)(t - l.end), ring = smooth01(age / 20);
                Ctx c = cx;
                if (ring < 1) dr::ring(c, at, 6 + 26 * ring, 2.5f, withAlpha(GOLDEN, 1 - ring));
                c.a *= smooth01(age / 6) * (1 - smooth01((age - 30) / 18));
                dr::text(fonts::ui(false), c, l.to, { at.x + 12, at.y - 12 }, 26, 0, INK, 0, 1);
            }
        };
        // The calendar, and where he is.
        s2->add(0, s2dur)->fn = [](const Ctx& cx, int t) {
            const Travels& tr = travels();
            Ctx c = cx;
            c.a *= (float)interpolate(t, { 6, 22 }, { 0, 1 }, clamped());
            cornerScrim(c);
            cal::Style st;
            st.accent = mkColor(0xc8302c);
            cal::page(c, { 1650, 752, 190, 230 }, tr.clock.day(t), st);
            int trip = -1;
            const Leg* now = nullptr;
            double km = 0;
            std::set<std::string> countries;
            for (const Leg& l : tr.legs) {
                if (t < l.start) break;
                trip = l.trip;
                now = &l;
                km += l.km * std::clamp((t - l.start) / (double)(l.end - l.start), 0.0, 1.0);
                if (l.country && t >= l.end) countries.insert(l.country);
            }
            char line[96];
            const float x = 1606;
            if (trip < 0) {
                dr::text(fonts::ui(true), c, "IN OFFICE", { x, 790 }, 48, 0, INK, 1, 0);
                dr::text(code::font(false), c, "from 20 October 2024", { x, 856 }, 26, 0, GOLDEN, 1, 0);
                return;
            }
            std::snprintf(line, sizeof line, "TRIP %d OF %d", trip + 1, (int)trips().size());
            dr::text(fonts::ui(true), c, line, { x, 778 }, 48, 0, INK, 1, 0);
            const bool flying = t < now->end;
            if (flying) std::snprintf(line, sizeof line, "%s to %s", now->from, now->to);
            else std::snprintf(line, sizeof line, now->country ? "in %s" : "home in %s", now->to);
            dr::text(code::font(false), c, line, { x, 844 }, 28, 0, GOLDEN, 1, 0);
            dr::text(code::font(false), c, cal::format(tr.clock.date(t), "%A %e %B %Y").c_str(), { x, 888 }, 24, 0, INK, 1, 0);
            std::snprintf(line, sizeof line, "%d countries, %s km flown", (int)countries.size(), thousands(km).c_str());
            dr::text(code::font(false), c, line, { x, 936 }, 22, 0, DIM, 1, 0);
        };
        caption(s2, "GLOBE::ROUTE, GLOBE::FLY, CAL::PAGE", "Prabowo Subianto's trips abroad");
        note(s2, "since 20 Oct 2024; itineraries from Wikipedia");
    }

    // 3. A heat map on a shell over the globe: an outbreak reaching city after city, along
    // the roads and the flights (simulated), remade every frame.
    ts.transition(Transition::fade(16));
    const int s3dur = 330;
    ViewFn v3 = [](int t) {
        const float k = smooth01(t / 330.f);
        return View{ { 62 - 75 * k, 22 + 4 * std::sin(k * 3.14159f) }, 4.0f + 0.9f * smooth01(t / 120.f) };
    };
    TNode* s3 = scene(s3dur, v3);
    {
        auto g = globeScene(v3);
        g3d::Material heatMat;
        heatMat.unlit = true;
        heatMat.opacity = 0.999f;
        heatMat.castShadow = false;
        heatMat.dynamicMap = std::make_shared<g3d::DynamicTexture>();
        heatMat.dynamicMap->make = [](int t) {
            const Outbreak& o = outbreak();
            const double day = (t - OUT_FROM) * DAYS_PER_FRAME;
            globe::Field f(1024);
            for (size_t i = 0; i < o.at.size(); i++) {
                const float k = growth(day, o.arrival[i]);
                if (k <= 0) continue;
                const float size = (float)std::clamp((std::log10(o.pop[i]) - 5) / 2.6, 0.15, 1.0);
                f.add(o.at[i], 180 + 620 * k * size, k * (0.35f + 0.65f * size));
            }
            return f.image(2.1f);
        };
        g->add(globe::sphere(1.004f, 160), heatMat)->name = "heat";
        s3->add(0, s3dur)->fn = g3d::draw(g);
        // The day and a rough count, and the palette's key.
        s3->add(OUT_FROM, s3dur - OUT_FROM)->fn = [](const Ctx& cx, int lt) {
            const Outbreak& o = outbreak();
            const double day = lt * DAYS_PER_FRAME;
            double cases = 0;
            int cities = 0;
            for (size_t i = 0; i < o.at.size(); i++) {
                const float k = growth(day, o.arrival[i]);
                cases += o.pop[i] * 0.004 * k * k;
                cities += k > 0;
            }
            Ctx c = cx;
            c.a *= (float)interpolate(lt, { 0, 14 }, { 0, 1 }, clamped());
            cornerScrim(c);
            char line[64];
            std::snprintf(line, sizeof line, "DAY %d", (int)day);
            dr::text(fonts::ui(true), c, line, { 1800, 860 }, 64, 0, INK, 1, 0);
            std::snprintf(line, sizeof line, cases >= 1e6 ? "%.1fM cases, %d cities" : "%.0fk cases, %d cities",
                          cases >= 1e6 ? cases / 1e6 : cases / 1e3, cities);
            dr::text(code::font(false), c, line, { 1800, 940 }, 28, 0, GOLDEN, 1, 0);
            for (int i = 0; i < 100; i++) dr::rect(c, 1500 + 3 * i, 1000, 3.5f, 14, withAlpha(globe::heat(i / 99.f), 1));
            dr::text(code::font(false), c, "few", { 1500, 1022 }, 20, 0, DIM, 0, 0);
            dr::text(code::font(false), c, "many", { 1800, 1022 }, 20, 0, DIM, 1, 0);
        };
        caption(s3, "GLOBE::FIELD, GLOBE::HEAT", "an outbreak, spreading");
        simulated(s3);
    }

    // 4. Close up on Sumatra and Borneo, 15 August to 24 September 2026: hotspots flaring
    // across the peat districts, haze drifting downwind to Singapore and Sarawak, the reports
    // as they came in, and a month calendar keeping the date.
    ts.transition(Transition::fade(16));
    const int s4dur = HAZE_DUR;
    ViewFn v4 = [](int t) {
        const float k = smooth01((t - 4) / 70.f);
        const View far{ { 112, -4 }, 4.8f }, near{ { 107.4, -0.4 }, 1.78f };
        View v = lerpView(far, near, k);
        v.at.lon += 0.8 * smooth01((t - 70) / 380.f);
        return v;
    };
    TNode* s4 = scene(s4dur, v4);
    {
        auto g = globeScene(v4);
        const cal::Clock clock = hazeClock();
        // A sharper copy of the map under the close-up, the hotspots over it, the smoke above.
        const geo::Bounds near{ 86, -16, 130, 16 };
        g3d::Material ground;
        ground.roughness = 0.8f;
        ground.castShadow = false;
        ground.dynamicMap = std::make_shared<g3d::DynamicTexture>();
        ground.dynamicMap->make = [near](int) { return paintEarth(2600, near)->image(); };
        ground.dynamicMap->every = INT_MAX;
        g->add(globe::patch(near, 1.0004f, 96), ground)->name = "close-up";
        g3d::Material fireMat;
        fireMat.unlit = true;
        fireMat.opacity = 0.999f;
        fireMat.castShadow = false;
        fireMat.dynamicMap = std::make_shared<g3d::DynamicTexture>();
        fireMat.dynamicMap->make = [clock](int t) { return hotspotImage(clock.day(t)); };
        fireMat.dynamicMap->every = 2;
        g->add(globe::patch(HAZE_BOX, 1.0012f, 96), fireMat)->name = "hotspots";
        g3d::Material smokeMat = fireMat;
        smokeMat.unlit = false;
        smokeMat.roughness = 1;
        smokeMat.dynamicMap = std::make_shared<g3d::DynamicTexture>();
        smokeMat.dynamicMap->make = [clock](int t) { return smokeImage(clock.day(t)); };
        smokeMat.dynamicMap->every = 2;
        g->add(globe::patch(HAZE_BOX, 1.006f, 96), smokeMat)->name = "smoke";
        s4->add(0, s4dur)->fn = g3d::draw(g);
        // Towns, the latest readings beside them, a pulse where each report comes from.
        std::vector<int> reportAt;
        for (const Report& r : reports()) reportAt.push_back(frameOf(clock, (double)cal::days(r.date)));
        s4->add(0, s4dur)->fn = [g, clock, reportAt](const Ctx& cx, int t) {
            const g3d::Camera cam = g3d::cameraAt(g->camera, t);
            Ctx c = cx;
            c.a *= (float)interpolate(t, { 40, 60 }, { 0, 1 }, clamped());
            const long today = (long)std::floor(clock.day(t));
            Vector2 at;
            for (const Town& town : TOWNS) {
                if (!g3d::project(cam, globe::point(town.at, 1.006f), { 0, 0, 1920, 1080 }, at)) continue;
                dr::circle(c, at, 5, withAlpha(BG, 0.7f));
                dr::circle(c, at, 3.5f, INK);
                const float dx = town.right ? 10.f : -10.f;
                dr::text(code::font(false), c, town.name, { at.x + dx + 1.5f, at.y + 1.5f }, 21, 0, withAlpha(BG, 0.8f), town.right ? 0 : 1, 0.5);
                dr::text(code::font(false), c, town.name, { at.x + dx, at.y }, 21, 0, INK, town.right ? 0 : 1, 0.5);
            }
            for (const Reading& r : readings()) {
                if (today < cal::days(r.from) || today > cal::days(r.until)) continue;
                if (!g3d::project(cam, globe::point(r.at, 1.006f), { 0, 0, 1920, 1080 }, at)) continue;
                const bool right = r.at.lon > 110.5 || r.at.lon < 104;
                pill(c, { at.x + (right ? 64.f : -64.f), at.y + 26 }, r.text, INK, r.color, 20);
            }
            for (size_t i = 0; i < reports().size(); i++) {
                const float age = (float)(t - reportAt[i]);
                if (age < 0 || age > 40) continue;
                if (!g3d::project(cam, globe::point(reports()[i].at, 1.006f), { 0, 0, 1920, 1080 }, at)) continue;
                const float k = smooth01(age / 40);
                dr::ring(cx, at, 10 + 70 * k, 3, withAlpha(CORAL, 1 - k));
            }
        };
        // The reports, newest at the bottom, and the calendar.
        s4->add(0, s4dur)->fn = [clock, reportAt](const Ctx& cx, int t) {
            Ctx c = cx;
            c.a *= (float)interpolate(t, { 30, 50 }, { 0, 1 }, clamped());
            dr::rectGrad4(c, 0, 520, 980, 560, withAlpha(BG, 0), withAlpha(BG, 0), withAlpha(BG, 0), withAlpha(BG, 0.85f));
            cornerScrim(c);
            int newest = -1;
            for (size_t i = 0; i < reportAt.size(); i++)
                if (t >= reportAt[i]) newest = (int)i;
            for (int i = newest, row = 0; i >= 0 && row < 4; i--, row++) {
                const Report& r = reports()[i];
                const float k = smooth01((t - reportAt[i]) / 12.f);
                const float slide = row == 0 ? (1 - k) * 30 : 0;
                // Each row moves up as the next report pushes it.
                const float push = i < newest ? smooth01((t - reportAt[i + 1]) / 12.f) : 1;
                const float y = 930 - 96 * (row - 1 + push) + slide;
                Ctx rc = c;
                rc.a *= (row == 0 ? k : 1) * (row == 0 ? 1 : 0.62f) * (row == 3 ? 1 - push : 1);
                dr::roundRect(rc, 70, y, 640, 82, 12, withAlpha(BG, 0.86f));
                dr::rect(rc, 70, y + 12, 4, 58, CORAL);
                char date[16];
                std::snprintf(date, sizeof date, "%d %s", r.date.day, cal::monthName(r.date.month, true));
                dr::text(code::font(false), rc, date, { 92, y + 14 }, 22, 0, GOLDEN, 0, 0);
                dr::text(fonts::ui(true), rc, r.place, { 186, y + 12 }, 26, 0, INK, 0, 0);
                dr::text(code::font(false), rc, r.source, { 692, y + 16 }, 18, 0, DIM, 1, 0);
                dr::text(code::font(false), rc, r.text, { 92, y + 50 }, 22, 0, INK, 0, 0);
            }
            dr::text(code::font(false), c, "hotspot positions and smoke are illustrative; dates and readings as reported",
                     { 72, 1046 }, 18, 0, DIM, 0, 0);
            const double day = clock.day(t);
            cal::Style st;
            st.accent = mkColor(0xc8302c);
            cal::month(c, { 1500, 712, 360, 300 }, day, st);
            dr::text(fonts::ui(true), c, cal::format(cal::date(day), "%A %e %B").c_str(), { 1860, 676 }, 34, 0, INK, 1, 0.5);
        };
        caption(s4, "GLOBE::PATCH, GLOBE::FIELD, CAL::MONTH", "fires and haze, Indonesia 2026");
        note(s4, "hotspot counts: BMKG, BNPB, Forestry Ministry; air quality: NEA, Malaysia");
    }

    // 5. Weather: clouds wound into a typhoon that strengthens as it tracks west, and its
    // forecast track drawn on just above the clouds (simulated).
    ts.transition(Transition::fade(16));
    const int s5dur = 330;
    ViewFn v5 = [](int t) {
        const float k = smooth01(t / 230.f), away = smooth01((t - 250) / 80.f);
        const View a{ { 136, 9 }, 2.6f }, b{ { 128, 15 }, 1.9f }, wide{ { 125, 18 }, 5.0f };
        return lerpView(lerpView(a, b, k), wide, away);
    };
    TNode* s5 = scene(s5dur, v5);
    {
        auto g = globeScene(v5);
        addClouds(*g, [s5dur](int t) { return typhoon(t, s5dur); }, 2048, 2);
        globe::RouteStyle st;
        st.width = 0.0025f;
        st.color = mkColor(0xffd9a0);
        st.segments = 60;
        g3d::Object3D* track = globe::path(*g->add(), [](float u) { return globe::point(geo::interpolate(STORM_FROM, TRACK_END, u), 1.02f); },
                                           [](int t) { return (float)interpolate(t, { 10, (double)TRACK_DRAW + 30 }, { 0, 1 }, clamped(ease::inOut(ease::cubic))); }, st);
        (void)track;
        s5->add(0, s5dur)->fn = g3d::draw(g);
        s5->add(0, s5dur)->fn = [g, s5dur](const Ctx& cx, int t) {
            const g3d::Camera cam = g3d::cameraAt(g->camera, t);
            const globe::Weather w = typhoon(t, s5dur);
            const globe::Storm& s = w.storms[0];
            Vector2 at;
            const Vector3 eye = globe::point(s.centre, 1.02f);
            if (!facing(cam, eye) || !g3d::project(cam, eye, { 0, 0, 1920, 1080 }, at)) return;
            Ctx c = cx;
            c.a *= (float)interpolate(t, { 20, 36 }, { 0, 1 }, clamped()) * (1 - smooth01((t - 250) / 40.f));
            const int cat = 1 + (int)std::floor((s.strength - 0.55f) / 0.45f * 3.99f);
            char line[48];
            std::snprintf(line, sizeof line, "TYPHOON  category %d", cat);
            dr::ring(c, at, 18, 2.5f, withAlpha(CORAL, 0.9f));
            vg::stroke(c, vg::Path::line({ at.x + 14, at.y - 14 }, { at.x + 90, at.y - 90 }), vg::Stroke(CORAL, 2.5f));
            pill(c, { at.x + 90 + 130, at.y - 90 }, line, INK, withAlpha(BG, 0.85f), 24);
        };
        caption(s5, "GLOBE::CLOUDS, GLOBE::PATH", "a typhoon, winding up");
        simulated(s5);
    }

    comp.duration = ts.end();
    Cues cues;
    cues.s2 = s2->start;
    cues.s3 = s3->start;
    cues.s4 = s4->start;
    cues.s5 = s5->start;
    cues.end = comp.duration;
    cues.s5dur = s5dur;
    comp.assets = [cues](const Comp&, const Json&) {
        if (std::filesystem::exists(SCORE)) return;
        std::filesystem::create_directories(std::filesystem::path(SCORE).parent_path());
        genScore(SCORE, cues);
    };
    media::audio(addRoot(comp.roots, 0, comp.duration), SCORE);
    out.push_back(std::move(comp));
}

} // namespace rmtn
