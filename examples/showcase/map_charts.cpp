// map_charts.cpp -- "map-charts": geo3d:: data on a globe and on a map. Population as
// columns on a turning globe; the globe unrolls into an Equal Earth map and the world's
// largest migration corridors arc across it; then the map lies on a table as a miniature,
// GDP per person raised country by country, under a dusk sky with fog and a tilt-shift lens.
//
// Data: population and GDP from the bundled Natural Earth countries (pop_est, gdp_md;
// 2019 estimates). Corridors: IOM World Migration Report 2024, Figure 1 (people born in the
// first country living in the second in 2020, UN DESA); the two largest sizes are as the
// report states them, the rest are read from its chart and are approximate.
#include "rmtn/raymotion.hpp"
#include "procgen.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace rmtn {
namespace {

const Color INK = mkColor(0xeef0ff), DIM = mkColor(0xa8b0d0), AQUA = mkColor(0x47dad8), BG = mkColor(0x04060d);
const Color OCEAN = mkColor(0x0d1a33), LAND = mkColor(0x2a3b5c), BORDER = mkColor(0x4a5f88);

constexpr int GLOBE = 300, UNROLL = 330, TABLE = 330;   // frames per part
const std::string SCORE = "out/map-charts-assets-v1/score.wav";

unsigned hash(unsigned x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x;
}
float rnd(unsigned a, unsigned b = 0) { return (hash(a * 9781u + b * 6271u + 17u) & 0xffffff) / 16777216.f; }

float smooth01(float x) {
    x = std::clamp(x, 0.f, 1.f);
    return x * x * (3 - 2 * x);
}

void caption(TNode* scene, std::string kicker, std::string title) {
    scene->add(0, scene->dur)->fn = [kicker, title](const Ctx& cx, int t) {
        const float k = (float)interpolate(t, { 0, 14 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        Ctx c = cx;
        c.a *= k;
        dr::rectGrad4(c, 0, 0, 1300, 380, withAlpha(BG, 0.6f), withAlpha(BG, 0), withAlpha(BG, 0), withAlpha(BG, 0));
        dr::text(fonts::ui(false), c, kicker.c_str(), { 120, 96 + 12 * (1 - k) }, 28, 4, AQUA);
        dr::text(fonts::ui(true), c, title.c_str(), { 120, 136 + 12 * (1 - k) }, 64, 0, INK);
    };
}

void footnote(TNode* scene, std::string text) {
    scene->add(0, scene->dur)->fn = [text](const Ctx& cx, int t) {
        Ctx c = cx;
        c.a *= (float)interpolate(t, { 20, 40 }, { 0, 1 }, clamped());
        dr::text(fonts::ui(false), c, text.c_str(), { 120, 1020 }, 20, 0, DIM);
    };
}

// ---------------------------------------------------------------- data

struct Country {
    std::string iso, name;
    double pop, gdpPerPerson;
};

// Countries with a million people or more, largest first.
const std::vector<Country>& countries() {
    static const std::vector<Country> all = [] {
        std::vector<Country> out;
        for (const geo::Feature& f : geo::countries().features) {
            const double pop = f.properties["pop"].asNumber(), gdp = f.properties["gdp_md"].asNumber();
            const std::string iso = f.properties["iso_a3"].asString();
            if (pop < 1e6 || gdp <= 0 || iso.size() != 3 || iso == "-99") continue;
            out.push_back({ iso, f.properties["name"].asString(), pop, gdp * 1e6 / pop });
        }
        std::sort(out.begin(), out.end(), [](const Country& a, const Country& b) { return a.pop > b.pop; });
        return out;
    }();
    return all;
}

struct Corridor {
    const char *from, *to;
    double millions;
};
const Corridor CORRIDORS[] = {
    { "MEX", "USA", 11 },  { "SYR", "TUR", 3.9 }, { "UKR", "RUS", 3.7 }, { "IND", "ARE", 3.5 }, { "RUS", "UKR", 3.4 },
    { "IND", "USA", 2.8 }, { "AFG", "IRN", 2.8 }, { "KAZ", "RUS", 2.6 }, { "IND", "SAU", 2.5 }, { "BGD", "IND", 2.5 },
    { "RUS", "KAZ", 2.5 }, { "CHN", "USA", 2.2 }, { "POL", "DEU", 2.1 }, { "PHL", "USA", 2.1 }, { "MMR", "THA", 1.9 },
    { "TUR", "DEU", 1.8 }, { "VEN", "COL", 1.8 }, { "IDN", "SAU", 1.7 }, { "DZA", "FRA", 1.7 }, { "AFG", "PAK", 1.6 },
    { "UKR", "POL", 1.5 },
};

geo::LonLat placeOf(const char* iso) { return geo::countries().get(iso).labelPoint(); }

Color warm(float t) { return mixC(mkColor(0xffd166), mkColor(0xff5d73), smooth01(t)); }

// Paper to deep teal: poor countries stay close to the map, rich ones stand out.
Color wealthColor(float t) {
    const Color stops[] = { mkColor(0xefe2c4), mkColor(0x9fd3a8), mkColor(0x3c9d93), mkColor(0x1f5f7a), mkColor(0x17304f) };
    t = std::clamp(t, 0.f, 1.f) * 4;
    const int i = std::min(3, (int)t);
    return mixC(stops[i], stops[i + 1], t - i);
}

constexpr float COLUMN = 0.6f;   // the most populous country's column, in globe radii

std::shared_ptr<g3d::DynamicTexture> baseMap(Color ocean, Color land, Color border) {
    globe::Map map(4096, ocean);
    map.fill(geo::land().merged(), land).stroke(geo::countries().merged(), border, 1.6f);
    map.graticule(30, withAlpha(border, 0.35f), 1);
    return map.texture();
}

// ---------------------------------------------------------------- score

void genScore(const std::string& path, int frames) {
    using procgen::Score;
    const double end = frames / 30.0;
    Score s(end + 0.5);
    const int prog[4][4] = { { 45, 64, 69, 72 }, { 41, 65, 69, 72 }, { 43, 62, 67, 71 }, { 40, 64, 67, 71 } };
    const double bar = 4 * 60.0 / 84, stop = end - 2.4;
    int i = 0;
    for (double t = 0; t < stop; t += bar, i++) {
        const int* ch = prog[i % 4];
        for (int k = 1; k < 4; k++) s.pad(t, bar * 0.98, ch[k], 0.06f, 900, 0.5f);
        s.bass(t, bar * 0.9, ch[0], 0.13f);
        for (int e = 0; e < 8; e++)
            if (rnd((unsigned)i, (unsigned)e) < 0.55f) s.pluck(t + e * bar / 8, ch[1 + e % 3] + 12, 0.022f, e % 2 ? 0.3f : 0.7f, 6);
        s.hat(t + bar / 2, 0.012f, 0.6f);
    }
    s.impact(GLOBE / 30.0, 0.12f);
    s.riser((GLOBE + UNROLL) / 30.0 - 1.6, (GLOBE + UNROLL) / 30.0, 0.03f);
    s.impact((GLOBE + UNROLL) / 30.0, 0.1f);
    for (int k = 1; k < 4; k++) s.pad(stop, 2.4, prog[0][k], 0.08f, 900, 0.5f);
    s.bass(stop, 2.4, prog[0][0] - 12, 0.16f);
    media::writeWav(path, s.mix(), Score::RATE);
}

} // namespace

void registerMapCharts(Registry& out) {
    Comp comp;
    comp.name = "map-charts";
    comp.w = 1920; comp.h = 1080; comp.fps = 30;
    comp.background = BG;
    comp.duration = GLOBE + UNROLL + TABLE;
    const Rectangle VIEW{ 0, 0, 1920, 1080 };

    // ---- 1 and 2: one surface, a globe that unrolls into a map
    auto surface = std::make_shared<geo3d::Surface>(geo::Projection::equalEarth(), geo::LonLat{ 10, 0 });
    auto scene = std::make_shared<g3d::Scene>();
    scene->ambient = 0.35f;
    scene->lights.push_back(g3d::Light::directional({ 4, 3, 6 }, WHITE, 1.1f));
    scene->camera.fov = 30;
    g3d::Object3D* world = scene->add();
    g3d::Material paper;
    paper.dynamicMap = baseMap(OCEAN, LAND, BORDER);
    paper.roughness = 0.9f;
    geo3d::base(*world, surface, paper, 256);

    // Population columns, the 40 largest countries.
    std::vector<geo3d::Datum> people;
    for (size_t i = 0; i < 40 && i < countries().size(); i++) {
        const Country& c = countries()[i];
        people.push_back({ placeOf(c.iso.c_str()), c.pop / 1e6, c.iso, BLANK });
    }
    geo3d::MarkStyle cols;
    cols.size = 0.028f;
    cols.height = COLUMN;
    cols.scale.colors = warm;
    cols.stagger = 2;
    cols.grow = [](int t) {
        const float rise = smooth01((t - 20) / 40.f), fall = smooth01((t - GLOBE - 70) / 25.f);
        return rise * (1 - fall);
    };
    geo3d::marks(*world, surface, people, cols);

    // Corridors, once the map is flat.
    std::vector<geo3d::Flow> moves;
    for (const Corridor& c : CORRIDORS) moves.push_back({ placeOf(c.from), placeOf(c.to), c.millions, BLANK });
    geo3d::FlowStyle fl;
    fl.height = 0.6f;
    fl.width = 0.03f;
    fl.scale.colors = [](float t) { return mixC(mkColor(0x3ad6c5), mkColor(0xfff2b0), std::sqrt(t)); };
    fl.stagger = 4;
    fl.head = [](int t) { return smooth01((t - GLOBE - 100) / 45.f); };
    geo3d::flows(*world, surface, moves, fl);

    // Turn round from Asia to Africa, then sit square in front of the map as it unrolls,
    // backing off to take in the whole width.
    scene->camera.up = [](int t, g3d::Camera& cam) {
        const float k = smooth01(t / (float)GLOBE), a = (95 - 95 * k) * DEG2RAD, el = (22 - 22 * k) * DEG2RAD;
        const float m = smooth01((t - GLOBE) / 90.f), d = 4.6f + (8.4f - 4.6f) * m;
        // Once flat, look up the map from the south a little, so the arcs show their rise.
        const float tilt = 26 * DEG2RAD * smooth01((t - GLOBE - 80) / 50.f);
        cam.target = { 0, -0.3f * tilt, m };
        const Vector3 dir{ std::sin(a) * std::cos(el), std::sin(el) - std::sin(tilt), std::cos(a) * std::cos(el) * std::cos(tilt) };
        cam.position = Vector3Add(cam.target, Vector3Scale(Vector3Normalize(dir), d));
    };
    scene->up = [surface](int t, g3d::Scene&) { surface->morph = smooth01((t - GLOBE) / 90.f); };

    TransitionSeries ts(comp.roots, (float)comp.w, (float)comp.h);
    TNode* first = ts.add(GLOBE + UNROLL);
    first->add(0, first->dur)->fn = g3d::draw(scene);
    // Labels on the six largest, while they stand.
    first->add(0, GLOBE + 70)->fn = [scene, surface, people, VIEW](const Ctx& cx, int t) {
        const g3d::Camera cam = g3d::cameraAt(scene->camera, t);
        const float k = smooth01((t - 60) / 20.f) * (1 - smooth01((t - GLOBE - 40) / 20.f));
        if (k <= 0) return;
        const double top = people[0].value;
        for (size_t i = 0; i < 6; i++) {
            const geo3d::Datum& d = people[i];
            Vector2 p;
            if (!geo3d::screen(cam, *surface, d.at, COLUMN * (float)(d.value / top) + 0.04f, VIEW, p)) continue;
            Ctx c = cx;
            c.a *= k;
            const std::string name = geo::countries().get(d.key).properties["name"].asString();
            char num[32];
            std::snprintf(num, sizeof num, "%.0f M", d.value);
            dr::text(fonts::ui(true), c, name.c_str(), { p.x, p.y - 30 }, 24, 0, INK, 1, 0.5f);
            dr::text(fonts::ui(false), c, num, { p.x, p.y - 4 }, 20, 0, DIM, 1, 0.5f);
        }
    };
    first->add(0, GLOBE + 70)->fn = [](const Ctx& cx, int t) {
        Ctx c = cx;
        c.a *= 1 - smooth01((t - GLOBE - 40) / 20.f);
        plot::ColorbarStyle cs;
        cs.colors = warm;
        cs.title = "people, millions";
        plot::colorbar(c, { 1700, 620, 26, 300 }, plot::Range(0, 1400, 200), cs, (t - 40) / 40.f);
    };
    // The three largest corridors, labelled on the map.
    first->add(GLOBE + 120, UNROLL - 120)->fn = [scene, surface, VIEW](const Ctx& cx, int t) {
        const g3d::Camera cam = g3d::cameraAt(scene->camera, t + GLOBE + 120);
        for (int i = 0; i < 3; i++) {
            const Corridor& co = CORRIDORS[i];
            const float k = smooth01((t - 20 - 12 * i) / 15.f);
            if (k <= 0) continue;
            const geo::LonLat a = placeOf(co.from), b = placeOf(co.to);
            const float rise = 0.6f * (float)std::sqrt(geo::angle(a, b) / 180.0);
            Vector2 p;
            if (!geo3d::screen(cam, *surface, geo::interpolate(a, b, 0.5), rise + 0.05f, VIEW, p)) continue;
            Ctx c = cx;
            c.a *= k;
            char line[96];
            std::snprintf(line, sizeof line, "%s to %s, %s%.1f M", geo::countries().get(co.from).properties["name"].asString().c_str(),
                          geo::countries().get(co.to).properties["name"].asString().c_str(), i < 2 ? "" : "about ", co.millions);
            dr::text(fonts::ui(true), c, line, { p.x, p.y - 10 }, 22, 0, INK, 1, 0.5f);
        }
    };
    caption(first->add(0, GLOBE), "GEO3D::MARKS ON A GLOBE", "where the people are");
    footnote(first->add(0, GLOBE), "Population, 2019 estimates (Natural Earth). 40 largest countries.");
    caption(first->add(GLOBE, UNROLL), "SURFACE::MORPH, GEO3D::FLOWS", "the globe unrolls: who moved where");
    footnote(first->add(GLOBE, UNROLL),
             "Largest migration corridors: people born in one country living in another, 2020. IOM World Migration Report 2024 (UN DESA).");

    // ---- 3: the map on a table, a miniature
    ts.transition(Transition::fade(24));
    TNode* third = ts.add(TABLE);
    auto flat = std::make_shared<geo3d::Surface>(geo::Projection::equalEarth(), geo::LonLat{ 10, 0 }, 1.f, 8.f);
    flat->morph = 1;
    auto table = std::make_shared<g3d::Scene>();
    table->sky = g3d::Sky::at(17.4f);
    table->sky.clouds = 0.35f;
    table->sky.sunLight = 0;
    table->fogMode = g3d::FogMode::Exponential;
    table->fogDensity = 0.025f;
    table->fogFromSky = true;
    g3d::Light sun = g3d::Light::directional({ -12, 6, -10 }, mkColor(0xffd2ac), 1.6f, true);
    sun.shadowExtent = 7;
    table->lights.push_back(sun);
    table->camera.fov = 32;
    g3d::Object3D* desk = table->add();
    desk->rotation = { -90, 0, 0 };          // the map plane (facing +z) laid face up
    desk->position = { 0, -1, 0 };           // its surface (z = 1) at y = 0
    g3d::Material sheet;
    sheet.dynamicMap = baseMap(mkColor(0x7b97ad), mkColor(0xd9cdb2), mkColor(0x8a7d66));
    sheet.roughness = 0.85f;
    geo3d::base(*desk, flat, sheet, 256);
    geo3d::Values wealth;
    for (const Country& c : countries()) wealth.push_back({ c.iso, c.gdpPerPerson / 1000 });
    geo3d::ExtrudeStyle ex;
    ex.height = 0.32f;
    ex.scale.min = 0;
    ex.scale.max = 70;
    ex.scale.colors = wealthColor;
    ex.stagger = 0.6f;
    ex.grow = [](int t) { return smooth01((t - 30) / 30.f); };
    ex.detail = 4;
    geo3d::extrude(*desk, flat, wealth, ex);
    table->camera.up = [](int t, g3d::Camera& cam) {
        const float a = (-30 + 40 * t / (float)TABLE) * DEG2RAD;
        cam.target = { 0.6f, 0, -0.6f };
        cam.position = { cam.target.x + 5.6f * std::sin(a), 4.2f, cam.target.z + 5.6f * std::cos(a) };
        cam.dof = { Vector3Distance(cam.position, cam.target), 2.2f, 14, -30 };
    };
    third->add(0, TABLE)->fn = g3d::draw(table);
    third->add(0, TABLE)->fn = [](const Ctx& c, int t) {
        plot::ColorbarStyle cs;
        cs.colors = wealthColor;
        cs.title = "GDP per person, $ thousand";
        plot::colorbar(c, { 1440, 960, 360, 22 }, plot::Range(0, 70, 10), cs, (t - 40) / 40.f);
    };
    caption(third, "GEO3D::EXTRUDE, SKY, DOF.TILT", "a miniature of the world's wealth");
    footnote(third, "GDP per person, 2019 estimates (Natural Earth). Heights capped at $70k.");

    comp.assets = [](const Comp& c, const Json&) {
        if (std::filesystem::exists(SCORE)) return;
        std::filesystem::create_directories(std::filesystem::path(SCORE).parent_path());
        genScore(SCORE, c.duration);
    };
    comp.duration = ts.end();
    media::audio(addRoot(comp.roots, 0, comp.duration), SCORE);
    out.push_back(std::move(comp));
}

} // namespace rmtn
