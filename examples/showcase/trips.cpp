// trips.cpp -- "trips" (1920x1080) and "trips_v" (1080x1670): President Prabowo Subianto's
// trips abroad, staged as a long-form feature and built twice from one timeline.
//
//   trips    landscape, for the showcase
//   trips_v  portrait, matched to the 480x742 promo footage (0.6467 vs 0.6469) so the
//            globe cut and the real footage can sit in one vertical edit
//
// One itinerary, one clock, one camera rig; only the fit and the HUD layout differ. Every
// layout number is derived from the frame, not copied from the landscape grid: the globe is
// fitted to the NARROW edge (height in 16:9, width in 9:14), the caption takes a top band,
// the calendar and the running totals take a bottom band.
//
// The scene itself: legs last 14-40 frames by their arc (not 4-9), a landing holds 18, the
// quiet weeks take 10-28 by their real days -- 180 s against the old cut's 20 s. The camera
// RIDES the plane (it looks at wherever the plane is that frame, on the same eased curve the
// plane flies, then settles while the clock catches up) instead of averaging its position
// over +-90 frames and lagging a continent behind.
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <raymath.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <memory>
#include <set>
#include <string>

namespace rmtn {
namespace {

const Color BG = mkColor(0x05070f), INK = mkColor(0xeef0ff), DIM = mkColor(0x8a91b4), AQUA = mkColor(0x47dad8),
            GOLDEN = mkColor(0xffc857);
constexpr float FOV = 30;

float smooth01(float x) {
    x = std::clamp(x, 0.f, 1.f);
    return x * x * (3 - 2 * x);
}

unsigned hash(unsigned x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x;
}
float rnd(unsigned a, unsigned b = 0) { return (hash(a * 9781u + b * 6271u + 17u) & 0xffffff) / 16777216.f; }

float vnoise(float x, float y, unsigned seed) {
    const int ix = (int)std::floor(x), iy = (int)std::floor(y);
    const float fx = x - ix, fy = y - iy, u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    auto h = [seed](int a, int b) { return rnd((unsigned)a * 73856093u ^ (unsigned)b * 19349663u, seed); };
    return (h(ix, iy) * (1 - u) + h(ix + 1, iy) * u) * (1 - v) + (h(ix, iy + 1) * (1 - u) + h(ix + 1, iy + 1) * u) * v;
}

// ---------------------------------------------------------------- layout

struct Look {
    int w, h;
    float dNear, dFar, dLead;                       // camera distance: hop, haul, lead-in
    float capX, capY, titleW;                       // headline, auto-fitted to titleW
    float countriesY, countriesSz;                  // "N countries", under the headline
    float calX, calY, calW, calH;                   // calendar page, bottom left
    float kmX, kmY, kmSz;                           // distance flown, beside the calendar
    float statX, tripY, tripSz, routeY, routeSz, dateY, dateSz;   // bottom-right block
    float pillSz, labelSz, labelDx, labelDy, jakartaDy, ovX, ovY;
    float bottomX, bottomY, bottomW, bottomH;       // scrim alphas: TL TR BR BL
    float sTL, sTR, sBR, sBL;
    bool portrait;
};

const Look LAND{ 1920, 1080, 4.30f, 5.25f, 4.75f,
                 120.f, 96.f, 1680.f,
                 214.f, 40.f,
                 120.f, 800.f, 190.f, 230.f,
                 348.f, 902.f, 46.f,
                 1800.f, 800.f, 38.f, 852.f, 50.f, 922.f, 38.f,
                 27.f, 29.f, 13.f, -13.f, 38.f, 170.f, 38.f,
                 1100.f, 660.f, 820.f, 420.f,
                 0.f, 0.f, 0.f, 0.86f,
                 false };

// Portrait, matched to the promo footage: 480x742 -> 0.6469, 1080/1670 -> 0.6467.
// Horizontal half-FOV here is 9.83 deg, so the globe is fitted by width: dNear 6.25 gives
// 93.6% of the frame width, dFar 7.15 gives 82%. That leaves a 330 px band above and below
// the globe: headline + country count on top, calendar, distance and the trip block below.
const Look VERT{ 1080, 1670, 6.25f, 7.15f, 6.55f,
                 56.f, 92.f, 968.f,
                 206.f, 44.f,
                 70.f, 1404.f, 150.f, 182.f,
                 252.f, 1508.f, 48.f,
                 1010.f, 1400.f, 38.f, 1446.f, 54.f, 1552.f, 38.f,
                 21.f, 23.f, 11.f, -11.f, 30.f, 120.f, 30.f,
                 0.f, 1330.f, 1080.f, 340.f,
                 0.f, 0.f, 0.86f, 0.86f,
                 true };

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

std::shared_ptr<globe::Map> paintEarth(int W) {
    globe::Map lm(W, geo::Bounds{ -180, -90, 180, 90 }, BLANK);
    lm.fill(geo::land().merged(), WHITE);
    globe::Map m(W, geo::Bounds{ -180, -90, 180, 90 }, BLANK);
    for (int y = 0; y < m.height(); y++)
        for (int x = 0; x < W; x++) {
            const unsigned char a = lm.pixel(x, y).a;
            if (!a) continue;
            const globe::LonLat p = m.place(x + 0.5f, y + 0.5f);
            Color c = biome(p.lat, p.lon);
            c.a = a;
            m.pixel(x, y) = c;
        }
    geo::Geometry ice;
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

void backdrop(TNode* scene, ViewFn view, const Look& L) {
    scene->add(0, scene->dur)->fn = [view, L](const Ctx& cx, int t) {
        dr::rect(cx, 0, 0, (float)L.w, (float)L.h, BG);
        for (unsigned i = 0; i < 520; i++) {
            const float x = rnd(i, 1) * L.w, y = rnd(i, 2) * L.h, s = 0.6f + 1.6f * rnd(i, 3) * rnd(i, 4);
            const float tw = 0.55f + 0.45f * std::sin(t * 0.05f + i);
            dr::circle(cx, { x, y }, s, withAlpha(mixC(INK, AQUA, rnd(i, 5) * 0.5f), (0.25f + 0.6f * rnd(i, 6)) * tw));
        }
        const float D = view(t).distance;
        const float r = std::tan(std::asin(1 / D)) / std::tan(FOV / 2 * DEG2RAD) * (L.h / 2.f);
        for (int k = 14; k >= 1; k--) {
            const float f = k / 14.f;
            dr::circle(cx, { L.w / 2.f, L.h / 2.f }, r * (1 + 0.075f * f), withAlpha(mkColor(0x4f9dff), 0.05f * (1 - f) + 0.012f));
        }
    };
}

bool facing(const g3d::Camera& cam, Vector3 p) {
    return Vector3DotProduct(Vector3Normalize(p), Vector3Subtract(cam.position, p)) > 0;
}

void pill(const Ctx& c, Vector2 at, const char* text, Color fg, Color bg, float size) {
    const float w = MeasureTextEx(code::font(false), text, size, 0).x + size * 1.25f;
    dr::roundRect(c, at.x - w / 2, at.y - size * 0.95f, w, size * 1.9f, size * 0.95f, bg);
    dr::text(code::font(false), c, text, at, size, 0, fg, 0.5f, 0.5f);
}

std::string thousands(double v) {
    std::string s = std::to_string((long long)std::llround(v)), out;
    for (size_t i = 0; i < s.size(); i++) {
        if (i && (s.size() - i) % 3 == 0) out += ',';
        out += s[i];
    }
    return out;
}

// One headline, auto-fitted to the frame width. No kicker, no source note.
void caption(TNode* scene, const Look& L, const char* title) {
    scene->add(0, scene->dur)->fn = [L, title](const Ctx& cx, int t) {
        const float k = (float)interpolate(t, { 0, 14 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        Ctx c = cx;
        c.a *= k;
        const float probe = 100.f;
        const float tw = MeasureTextEx(fonts::ui(true), title, probe, 0).x;
        const float size = tw > 1.f ? L.titleW / tw * probe : 64.f;
        dr::rectGrad4(c, 0, 0, (float)L.w, L.h * 0.30f, withAlpha(BG, 0.72f), withAlpha(BG, 0.72f), withAlpha(BG, 0), withAlpha(BG, 0));
        dr::text(fonts::ui(true), c, title, { L.capX, L.capY + 14 * (1 - k) }, size, 0, INK);
    };
}

// A size that keeps `text` inside `maxW` at `size` or below.
float fitText(const Font& f, const char* text, float size, float maxW) {
    const float w = MeasureTextEx(f, text, size, 0).x;
    return w > maxW && w > 0 ? size * maxW / w : size;
}

// ---------------------------------------------------------------- Prabowo's trips

struct Stop {
    const char* city;
    const char* country;
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

struct Leg {
    globe::LonLat a, b;
    const char* from;
    const char* to;
    const char* country;
    int trip;
    double day;
    double km;
    double angle;
    int start, end;
};

// Timing, in frames at 30 fps. A leg runs 14-40 frames by its arc (a hop to Kuala Lumpur is
// a beat, a Jakarta-New York haul is a shot); a landing holds 18 frames so the label and the
// calendar land; the quiet weeks between trips take 10-28 frames by their real days.
constexpr int LEAD = 44, HOLD = 18, TAIL = 96;

struct Travels {
    std::vector<Leg> legs;
    cal::Clock clock;
    std::vector<int> tripStart, tripEnd;
    int dur = 0;
};

const Travels& travels() {
    static const Travels all = [] {
        Travels r;
        const double inaug = (double)cal::days(INAUGURATION);
        r.clock.key(0, inaug).key(LEAD, inaug + 0.3);
        int f = LEAD;
        double prev = inaug;
        for (int ti = 0; ti < (int)trips().size(); ti++) {
            const Trip& trip = trips()[ti];
            const double depart = (double)cal::days(cal::parse(trip.front().arrive));
            f += std::clamp(10 + (int)((depart - prev) / 8), 10, 28);
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
                l.angle = geo::angle(l.a, l.b);
                l.start = f;
                l.end = f + std::clamp(14 + (int)std::lround(l.angle / DEG2RAD / 6.0), 14, 40);
                r.legs.push_back(l);
                r.clock.key(l.end, l.day + 0.5);
                f = l.end + HOLD;
                from = l.b;
                fromName = l.to;
            }
            r.tripEnd.push_back(f);
            prev = (double)cal::days(cal::parse(trip.back().leave));
        }
        r.dur = f + TAIL;
        return r;
    }();
    return all;
}

// The camera path at a given fit: over the plane, easing along the curve the plane flies, so
// it never lags; distance follows the leg's arc and eases across the holds, so a haul
// followed by a hop never snaps.
std::vector<View> makeView(const Travels& r, const Look& L) {
    const auto legBase = [&](const Leg& l) {
        return L.dNear + (L.dFar - L.dNear) * (float)std::clamp(l.angle / PI, 0.0, 1.0);
    };
    std::vector<View> view((size_t)r.dur, View{ JAKARTA, L.dLead });
    for (int t = 0; t < r.dur; t++) {
        const Leg* cur = nullptr;
        for (const Leg& l : r.legs)
            if (t >= l.start && t < l.end) { cur = &l; break; }
        if (cur) {
            const float u = (float)interpolate(t, { (double)cur->start, (double)cur->end }, { 0, 1 }, clamped(ease::inOut(ease::quad)));
            view[(size_t)t] = { geo::interpolate(cur->a, cur->b, u), legBase(*cur) + (L.dFar - L.dNear) * 0.22f * std::sin(PI * u) };
            continue;
        }
        const Leg* prev = nullptr;
        const Leg* next = nullptr;
        for (const Leg& l : r.legs)
            if (t >= l.end) prev = &l;
        for (const Leg& l : r.legs)
            if (t < l.start) { next = &l; break; }
        const globe::LonLat at = prev ? prev->b : JAKARTA;
        const float from = prev ? legBase(*prev) - (L.dFar - L.dNear) * 0.06f : L.dLead;
        float to = from, k = 0;
        if (next) {
            to = legBase(*next);
            const int a0 = prev ? prev->end : 0, a1 = next->start;
            k = smooth01((float)(t - a0) / (float)std::max(1, a1 - a0));
        }
        view[(size_t)t] = { at, from + (to - from) * k };
    }
    return view;
}

// The whole globe scene as one node, so other compositions can reuse it.
TNode* buildTripsNode(TransitionSeries& ts, const Look& L) {
    const Travels& tr = travels();
    const int dur = tr.dur;
    const std::vector<View> views = makeView(tr, L);
    ViewFn v = [views](int t) { return views[(size_t)std::clamp(t, 0, (int)views.size() - 1)]; };

    TNode* s = ts.add(dur);
    backdrop(s, v, L);
    {
        auto g = globeScene(v);
        g3d::Material dot;
        dot.unlit = true;
        dot.color = GOLDEN;
        g3d::Material homeDot = dot;
        homeDot.color = AQUA;
        const float gs = L.portrait ? 0.85f : 1.f;
        g->add(g3d::Geometry::sphere(0.014f * gs, 12, 16), homeDot)->position = globe::point(JAKARTA, 1.002f);
        std::set<std::string> seen;
        for (const Leg& l : tr.legs) {
            if (!l.country || !seen.insert(l.to).second) continue;
            g3d::Object3D* d = g->add(g3d::Geometry::sphere(0.0095f * gs, 10, 14), dot);
            d->position = globe::point(l.b, 1.002f);
            const int from = l.end - 1;
            d->up = [from](int t, g3d::Object3D& self) { self.visible = t >= from; };
        }
        const Color home = mixC(GOLDEN, AQUA, 0.65f);
        for (const Leg& l : tr.legs) {
            const int start = l.start, end = l.end, done = tr.tripEnd[l.trip];
            globe::RouteStyle st;
            st.width = L.portrait ? 0.0048f : 0.0052f;
            st.color = l.country ? GOLDEN : home;
            st.segments = std::clamp(12 + (int)(l.angle / 3), 12, 64);
            g3d::Object3D* r = globe::route(*g, l.a, l.b, [start, end](int t) {
                return (float)interpolate(t, { (double)start, (double)end }, { 0, 1 }, clamped(ease::inOut(ease::quad)));
            }, st);
            const Color bright = st.color, dim = mixC(st.color, BG, 0.62f);
            r->up = [done, bright, dim](int t, g3d::Object3D& self) {
                const Color c = mixC(bright, dim, smooth01((t - done) / 40.f));
                for (auto& seg : self.children) seg->material.color = c;
            };
        }
        g3d::Object3D* p = g->add();
        // A real twin-engine passenger jet (aircraft3d), scaled from metres to globe radii:
        // 0.08 of a radius nose-to-tail, so it reads as a silhouette without hiding the route.
        g3d::Object3D* jet = aircraft3d::addPassengerJet(*p);
        const float js = L.portrait ? 0.0031f : 0.0034f;
        jet->scale = { js, js, js };
        jet->position = { 0.f, 4.6f * js, 0.f };   // lift the fuselage onto the arc (radius units)
        for (auto& c : jet->children) {
            if (c->name.find("wheel") != std::string::npos || c->name.find("strut") != std::string::npos)
                c->visible = false;           // airborne: gear up
            c->material.castShadow = false;
        }
        p->up = [](int t, g3d::Object3D& self) {
            const Travels& tt = travels();
            self.visible = false;
            for (const Leg& l : tt.legs) {
                if (t < l.start || t >= l.end) continue;
                const float u = (float)interpolate(t, { (double)l.start, (double)l.end }, { 0, 1 }, clamped(ease::inOut(ease::quad)));
                globe::fly(self, l.a, l.b, u);
                self.visible = true;
            }
        };
        s->add(0, dur)->fn = g3d::draw(g);

        const Rectangle frame{ 0.f, 0.f, (float)L.w, (float)L.h };
        // Names as the plane lands, with a ring spreading out (near side only).
        s->add(0, dur)->fn = [g, L, frame](const Ctx& cx, int t) {
            const Travels& tt = travels();
            const g3d::Camera cam = g3d::cameraAt(g->camera, t);
            Vector2 at;
            const Vector3 jp = globe::point(JAKARTA, 1.0f);
            if (facing(cam, jp) && g3d::project(cam, jp, frame, at)) {
                Ctx c = cx;
                c.a *= (float)interpolate(t, { 4, 18 }, { 0, 1 }, clamped());
                pill(c, { at.x, at.y + L.jakartaDy }, "Jakarta", BG, AQUA, L.pillSz);
            }
            std::vector<Vector2> taken;
            for (auto it = tt.legs.rbegin(); it != tt.legs.rend(); ++it) {
                const Leg& l = *it;
                if (!l.country || t < l.end || t >= l.end + 72) continue;
                const Vector3 q = globe::point(l.b, 1.0f);
                if (!facing(cam, q) || !g3d::project(cam, q, frame, at)) continue;
                if (std::any_of(taken.begin(), taken.end(), [at, L](Vector2 o) {
                        return std::fabs(o.x - at.x) < L.ovX && std::fabs(o.y - at.y) < L.ovY; }))
                    continue;
                taken.push_back(at);
                const float age = (float)(t - l.end), ring = smooth01(age / 24);
                Ctx c = cx;
                if (ring < 1) dr::ring(c, at, 7 + 30 * ring, L.pillSz * 0.11f, withAlpha(GOLDEN, 1 - ring));
                c.a *= smooth01(age / 6) * (1 - smooth01((age - 46) / 22));
                // Keep the name inside the frame: flip the anchor near the right edge.
                const bool flip = at.x > (float)L.w * 0.62f;
                dr::text(fonts::ui(false), c, l.to,
                         { at.x + (flip ? -L.labelDx : L.labelDx), at.y + L.labelDy }, L.labelSz, 0, INK, flip ? 1.f : 0.f, 1);
            }
        };

        // Headline's country count (top left), the calendar with the distance flown beside
        // it (bottom left), and the trip block (bottom right).
        s->add(0, dur)->fn = [L](const Ctx& cx, int t) {
            const Travels& tt = travels();
            Ctx c = cx;
            c.a *= (float)interpolate(t, { 6, 22 }, { 0, 1 }, clamped());
            dr::rectGrad4(c, L.bottomX, L.bottomY, L.bottomW, L.bottomH,
                          withAlpha(BG, L.sTL), withAlpha(BG, L.sTR), withAlpha(BG, L.sBR), withAlpha(BG, L.sBL));
            int trip = -1;
            const Leg* now = nullptr;
            double km = 0;
            std::set<std::string> countries;
            for (const Leg& l : tt.legs) {
                if (t < l.start) break;
                trip = l.trip;
                now = &l;
                km += l.km * std::clamp((t - l.start) / (double)(l.end - l.start), 0.0, 1.0);
                if (l.country && t >= l.end) countries.insert(l.country);
            }
            char line[160];
            std::snprintf(line, sizeof line, countries.size() == 1 ? "1 country" : "%d countries", (int)countries.size());
            dr::text(fonts::ui(true), c, line, { L.capX, L.countriesY }, L.countriesSz, 0, GOLDEN);

            cal::Style st;
            st.accent = mkColor(0xc8302c);
            cal::page(c, { L.calX, L.calY, L.calW, L.calH }, tt.clock.day(t), st);

            std::snprintf(line, sizeof line, "%s km", thousands(km).c_str());
            dr::text(fonts::ui(true), c, line, { L.kmX, L.kmY }, L.kmSz, 0, INK);

            const float ax = 1.f;
            if (trip < 0) {
                dr::text(fonts::ui(true), c, "IN OFFICE", { L.statX, L.tripY }, L.tripSz, 0, INK, ax, 0);
                return;
            }
            std::snprintf(line, sizeof line, "TRIP %d OF %d", trip + 1, (int)trips().size());
            dr::text(fonts::ui(true), c, line, { L.statX, L.tripY }, L.tripSz, 0, INK, ax, 0);
            const bool flying = t < now->end;
            if (flying) std::snprintf(line, sizeof line, "%s to %s", now->from, now->to);
            else std::snprintf(line, sizeof line, now->country ? "in %s" : "home in %s", now->to);
            const float avail = L.statX - (L.portrait ? 236.f : 470.f);
            dr::text(fonts::ui(true), c, line, { L.statX, L.routeY }, fitText(fonts::ui(true), line, L.routeSz, avail), 0, GOLDEN, ax, 0);
            dr::text(code::font(false), c, cal::format(tt.clock.date(t), "%d/%m/%Y").c_str(), { L.statX, L.dateY }, L.dateSz, 0, INK, ax, 0);
        };

        caption(s, L, "Prabowo Subianto's trips abroad");
    }
    return s;
}

void buildTrips(Registry& out, const char* name, const Look& L) {
    Comp comp;
    comp.name = name;
    comp.w = L.w; comp.h = L.h; comp.fps = 30;
    comp.background = BG;
    TransitionSeries ts(comp.roots, (float)comp.w, (float)comp.h);
    buildTripsNode(ts, L);
    comp.duration = ts.end();
    out.push_back(std::move(comp));
}

} // namespace

void registerTrips(Registry& out) {
    buildTrips(out, "trips", LAND);
    buildTrips(out, "trips_v", VERT);
}

// Used by "trip_promo": the landscape look for widescreen, the portrait look otherwise.
TNode* addTripsScene(TransitionSeries& ts, int w, int h) {
    return buildTripsNode(ts, w >= h ? LAND : VERT);
}

// Flight and landing sound for the same scene, so it stays in step with the legs: a looped
// wind that rises while the plane is actually moving (and drops in the holds), a touchdown
// thud on every landing, a lift as each trip departs. The composed beds live in trip_promo.
void addTripsSfx(TNode* node) {
    const Travels& tr = travels();
    const std::string SFX = "D:/code/raymotion/assets/sfx/";

    TNode* w = node->add(0, node->dur);
    media::AudioOptions wo;
    wo.volume = 1.0f;
    wo.loop = true;
    media::audio(w, SFX + "wind.wav", wo).volumeAt = [](int t) {
        const Travels& tt = travels();
        float fly = 0.f;
        for (const Leg& l : tt.legs) {
            if (t < l.start - 6 || t > l.end + 6) continue;
            const float a = std::clamp((t - (l.start - 6)) / 6.f, 0.f, 1.f);
            const float b = std::clamp(((l.end + 6) - t) / 6.f, 0.f, 1.f);
            fly = std::max(fly, std::min(a, b));
        }
        return 0.05f + 0.55f * fly;
    };

    for (const Leg& l : tr.legs) {
        TNode* n = node->add(l.end - 2, 18);
        media::AudioOptions o;
        o.volume = 0.62f;
        o.playbackRate = 0.92 + 0.08 * (l.trip % 3);
        media::audio(n, SFX + "thud.wav", o);
    }
    for (int start : tr.tripStart) {
        TNode* n = node->add(start - 10, 30);
        media::AudioOptions o;
        o.volume = 0.26f;
        media::audio(n, SFX + "lift.wav", o);
    }
}

} // namespace rmtn
