// maps_2d.cpp -- "maps-2d": an engine showcase (not a film) of the 2D map helpers in geomap.hpp:
//   geo::base, choropleth, colorbar     a world map fills by a number per country
//   geo::Series                         a choropleth that steps through years on one scale
//   geo::route, along, length           a route on great circles, a moving marker, its length
//   geo::Projection::fly, highlight     a globe turns and zooms to one country, picked out
//   geo-route, geo-flyto                the same, from a few props, as prefabs
// Data: chapter 1 is Natural Earth's own gdp_md / pop (bundled); chapter 2's values are
// illustrative and say so on screen; the route's stops are real cities, its length is computed.
// There is no audio on purpose: this is a visual reference, like the script examples.
//   raymotion render maps-2d
#include "rmtn/raymotion.hpp"

#include <cstdio>

namespace rmtn {
namespace {

constexpr int FPS = 30, LEN = 210, XF = 20;
const Color INK = mkColor(0xf2f4ff), SUB = mkColor(0xb4bfcc), AMBER = mkColor(0xffd27a);

Font bold() { return fonts::ui(true); }
Font regular() { return fonts::ui(false); }

float ramp(int t, float a, float b) { return (float)interpolate(t, { a, b }, { 0, 1 }, clamped(ease::inOut(ease::cubic))); }
float lin(int t, float a, float b) { return (float)interpolate(t, { a, b }, { 0, 1 }, clamped()); }

struct Chapter { std::string title, note, code; };

// Caption card: what the chapter shows, and the call that makes it (as flora.cpp).
void caption(TNode* n, int len, const Chapter& ch) {
    TNode* card = n->add(12, len - 12);
    card->up = [len](int t, NodeProps& p) {
        const float in = (float)interpolate(t, { 0, 18 }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        const float out = (float)interpolate(t, { len - 40.0, len - 22.0 }, { 1, 0 }, clamped());
        p.opacity = in * out;
        p.pos = { 0, 24 * (1 - in) };
    };
    card->fn = [ch](const Ctx& cx, int) {
        const float w = std::max({ MeasureTextEx(regular(), ch.code.c_str(), 26, 0).x, MeasureTextEx(bold(), ch.title.c_str(), 60, 0).x,
                                   MeasureTextEx(regular(), ch.note.c_str(), 28, 0).x }) + 80;
        const float y = 1080 - 64 - 186;
        dr::roundRect(cx, 64, y, w, 186, 18, Color{ 12, 16, 22, 215 });
        dr::text(bold(), cx, ch.title.c_str(), { 104, y + 30 }, 60, 0, INK, 0, 0);
        dr::text(regular(), cx, ch.note.c_str(), { 104, y + 100 }, 28, 0, SUB, 0, 0);
        dr::text(regular(), cx, ch.code.c_str(), { 104, y + 142 }, 26, 0, AMBER, 0, 0);
    };
}

// A heading and a source line at the top left, fading in.
void heading(const Ctx& cx, int t, const char* title, const char* source) {
    const float k = ramp(t, 0, 20);
    dr::text(bold(), cx, title, { 64, 30 + 10 * (1 - k) }, 44, 0, Color{ INK.r, INK.g, INK.b, (unsigned char)(255 * k) }, 0, 0);
    dr::text(regular(), cx, source, { 64, 82 }, 22, 0, Color{ SUB.r, SUB.g, SUB.b, (unsigned char)(255 * k) }, 0, 0);
}

std::string thousands(long long v) {
    std::string d = std::to_string(v), out;
    for (size_t i = 0; i < d.size(); i++) {
        if (i && (d.size() - i) % 3 == 0) out += ',';
        out += d[i];
    }
    return out;
}

const geo::BaseStyle DARK = [] {
    geo::BaseStyle s;
    s.ocean = mkColor(0x121924);
    s.land = mkColor(0x222c3a);
    return s;
}();

// ---- 1: GDP per person from the bundled Natural Earth countries

// US$ per person: gdp_md (million US$) over pop, for every country that has both.
geo::Values gdpPerPerson() {
    geo::Values v;
    for (const geo::Feature& f : geo::countries().features) {
        const double pop = f.properties.num("pop", 0), gdp = f.properties.num("gdp_md", 0);
        const std::string code = f.properties["iso_a3"].isString() ? f.properties["iso_a3"].asString() : f.id;
        if (pop > 0 && gdp > 0 && code != "ATA") v.push_back({ code, gdp * 1e6 / pop });     // no Antarctica: nobody lives there
    }
    return v;
}

void choroplethChapter(TNode* n, int len) {
    auto world = std::make_shared<geo::Projection>(geo::Projection::equalEarth().fit({ 64, 120, 1792, 690 }, geo::Sphere));
    auto values = std::make_shared<geo::Values>(gdpPerPerson());
    auto scale = std::make_shared<geo::Scale>();
    scale->min = 0;
    scale->max = 60000;                          // the richest few share the top colour
    scale->colors = plot::viridis;
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        heading(cx, t, "GDP per person", "Natural Earth 1:110m (bundled): gdp_md / pop, US$; above 60,000 share the top colour");
        geo::base(cx, *world, DARK, ramp(t, 0, 50));
        geo::ChoroplethStyle cs;
        cs.scale = *scale;
        geo::choropleth(cx, *world, *values, cs, ramp(t, 45, 120));
        plot::ColorbarStyle bs;
        bs.title = "US$ per person";
        bs.textSize = 20;
        bs.colors = scale->colors;
        geo::colorbar(cx, { 1380, 880, 480, 44 }, *scale, bs, ramp(t, 70, 110));
        // ColorbarStyle has no tick labels of its own: say what the top colour means.
        const float k = ramp(t, 70, 110);
        const float w = MeasureTextEx(regular(), "60,000 and over", 20, 0).x;
        dr::text(regular(), cx, "60,000 and over", { 1860 - w, 850 }, 20, 0, Color{ AMBER.r, AMBER.g, AMBER.b, (unsigned char)(255 * k) }, 0, 0);
    };
}

// ---- 2: a series over years (illustrative)

const char* SERIES_CSV =
    "code,2000,2005,2010,2015,2020\n"
    "IDN,12,19,31,48,66\n"
    "MYS,21,38,56,71,88\n"
    "THA,9,17,29,47,72\n"
    "VNM,3,11,28,45,70\n"
    "PHL,6,10,22,39,58\n"
    "KHM,1,3,8,22,44\n"
    "LAO,1,2,6,17,36\n"
    "MMR,1,1,3,14,35\n";

void seriesChapter(TNode* n, int len) {
    const geo::Series series = geo::Series::from(plot::Table::parseCsv(SERIES_CSV));
    geo::Geometry region;
    for (const auto& row : series.rows) region.append(geo::countries().get(row.first).geometry);
    const Rectangle panel{ 64, 120, 1130, 690 };                    // the map lives here, clipped to it
    auto map = std::make_shared<geo::Projection>(geo::focus(geo::Projection::mercator(), panel, region, 30));
    auto scale = std::make_shared<geo::Scale>(series.scale());      // one range for every year
    auto data = std::make_shared<geo::Series>(series);
    n->add(0, len)->fn = [panel](const Ctx& cx, int) {              // a thin border round the panel
        dr::roundRect(cx, panel.x - 2, panel.y - 2, panel.width + 4, panel.height + 4, 20, Color{ 150, 168, 190, 140 });
    };
    TNode* mapNode = n->add(0, len);
    mapNode->isolate().mask = [panel](const Ctx& cx, int) {
        dr::roundRect(cx, panel.x, panel.y, panel.width, panel.height, 18, mkColor(0xffffff));
    };
    mapNode->fn = [=](const Ctx& cx, int t) {
        geo::BaseStyle bs;                                          // light ocean; land outside the eight is context
        bs.ocean = mkColor(0x1b2738);
        bs.land = mkColor(0x34435a);
        bs.graticule = Color{ 0, 0, 0, 0 };
        dr::rect(cx, panel.x, panel.y, panel.width, panel.height, bs.ocean);
        geo::base(cx, *map, bs, 1);
        geo::ChoroplethStyle cs;
        cs.scale = *scale;
        cs.stagger = 0;
        geo::choropleth(cx, *map, data->at(2000 + 20.0 * lin(t, 30, 180)), cs, ramp(t, 0, 25));
    };
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        dr::roundRect(cx, 1230, 120, 630, 690, 18, Color{ 18, 24, 34, 255 });
        const double year = 2000 + 20.0 * lin(t, 30, 180);
        plot::ColorbarStyle ks;
        ks.title = "illustrative index";
        ks.textSize = 20;
        geo::colorbar(cx, { 1270, 660, 550, 44 }, *scale, ks, ramp(t, 5, 30));
        char buf[16];
        std::snprintf(buf, sizeof buf, "%d", (int)std::lround(year));
        dr::text(bold(), cx, buf, { 1270, 250 }, 200, 0, INK, 0, 0);
        dr::text(regular(), cx, "illustrative values", { 1270, 480 }, 34, 0, AMBER, 0, 0);
        heading(cx, t, "Values over the years", "Illustrative values, not a dataset (eight countries, one colour scale)");
    };
}

// ---- 3: a route on great circles

void routeChapter(TNode* n, int len) {
    auto world = std::make_shared<geo::Projection>(geo::Projection::equalEarth().fit({ 64, 120, 1792, 690 }, geo::Sphere));
    struct Stop { const char* city; geo::LonLat at; };
    const Stop all[] = { { "Jakarta", { 106.85, -6.21 } }, { "Dubai", { 55.27, 25.20 } },
                         { "Vienna", { 16.37, 48.21 } }, { "New York", { -74.0, 40.71 } } };
    auto stops = std::make_shared<std::vector<geo::LonLat>>();
    auto marks = std::make_shared<std::vector<geo::Marker>>();
    for (const Stop& s : all) {
        stops->push_back(s.at);
        geo::Marker m;
        m.at = s.at;
        m.label = s.city;
        marks->push_back(m);
    }
    const long long km = (long long)std::llround(geo::length(*stops));   // computed, not typed in
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        heading(cx, t, "Jakarta to New York", "Stops are real cities; a route chosen for the demo, not a timetable");
        geo::base(cx, *world, DARK, 1);
        const float u = ramp(t, 20, 170);
        geo::RouteStyle rs;
        rs.ghost = Color{ 255, 255, 255, 40 };
        rs.head = 0;                              // the moving marker is drawn below from along()
        rs.width = 4;
        geo::route(cx, *world, *stops, rs, u);
        geo::MarkerStyle ms;
        ms.radius = 6;
        ms.textSize = 26;
        ms.bold = true;
        ms.stagger = 1;
        geo::markers(cx, *world, *marks, ms, ramp(t, 5, 170));
        double bearing = 0;
        const geo::LonLat here = geo::along(*stops, u, &bearing);
        Vector2 p;
        if (u > 0 && world->project(here, p)) {
            const unsigned char a = (unsigned char)(255 * (1 - lin(t, 164, 176)));      // fades out as it arrives
            dr::ring(cx, p, 14, 3, Color{ AMBER.r, AMBER.g, AMBER.b, a });
            dr::circle(cx, p, 7, Color{ 255, 255, 255, a });
        }
        char buf[64];
        std::snprintf(buf, sizeof buf, "%s km", thousands((long long)std::llround(geo::length(*stops) * u)).c_str());
        dr::text(bold(), cx, buf, { 1260, 870 }, 56, 0, INK, 0, 0);
        std::snprintf(buf, sizeof buf, "of %s km, heading %d deg", thousands(km).c_str(), (int)std::lround(bearing));
        dr::text(regular(), cx, buf, { 1260, 940 }, 26, 0, SUB, 0, 0);
    };
}

// ---- 4: a globe flies to a country

void flyChapter(TNode* n, int len) {
    const Rectangle box{ 480, 40, 960, 780 };
    auto globe = std::make_shared<geo::Projection>(geo::Projection::orthographic().rotate({ 20, 8 }).fit(box, geo::Sphere));
    const geo::Feature& kenya = geo::countries().get("KEN");
    auto region = std::make_shared<geo::Geometry>(kenya.geometry);
    auto close = std::make_shared<geo::Projection>(geo::focus(*globe, box, kenya.geometry, 150));
    const std::string name = kenya.name();
    const geo::LonLat at = kenya.labelPoint();
    n->add(0, len)->fn = [=](const Ctx& cx, int t) {
        const geo::Projection view = geo::Projection::fly(*globe, *close, ramp(t, 15, 120));
        geo::BaseStyle bs;                        // the default land and borders, as in chapters 3 and 5
        bs.graticuleStep = 10;
        geo::base(cx, view, bs, 1);
        geo::HighlightStyle hs;
        hs.fill = mkColor(0xe25d4e);              // no dim: the neighbours stay visible
        geo::highlight(cx, view, *region, hs, ramp(t, 100, 150));
        geo::LabelStyle ls;
        ls.size = 44;
        ls.bold = true;
        geo::labels(cx, view, { { at, name } }, ls, ramp(t, 125, 160));
        heading(cx, t, "A globe flies to a country", "Natural Earth 1:110m outline of Kenya, picked out after the flight");
    };
}

// ---- 5: the prefabs

void prefabChapter(TNode* n, int len) {
    // One after the other, each on the whole frame (a prefab draws its own base map).
    const int half = len / 2;
    TNode* route = n->add(0, half);
    prefab::use(route, 0, half, "geo-route",
                Json::parse(R"({"box":[0,0,1920,820],"stops":[[-0.13,51.5],[2.35,48.86],[13.4,52.52],[16.37,48.21]],
                                "labels":["London","Paris","Berlin","Vienna"],"padding":200,"delay":10,"duration":80,"textSize":30})"));
    TNode* fly = n->add(half, len - half);
    prefab::use(fly, 0, len - half, "geo-flyto",
                Json::parse(R"({"box":[0,0,1920,820],"from":"world","to":"BRA","padding":120,"delay":10,"duration":80,"textSize":40})"));
    for (TNode* part : { route, fly }) {
        const bool first = part == route;
        part->add(0, first ? half : len - half)->fn = [first](const Ctx& cx, int t) {
            const float k = ramp(t, 0, 16);
            dr::text(bold(), cx, first ? "geo-route" : "geo-flyto", { 64, 40 }, 52, 0, Color{ INK.r, INK.g, INK.b, (unsigned char)(255 * k) }, 0, 0);
        };
    }
}

const Chapter CHAPTERS[] = {
    { "A map filled by numbers", "GDP per person, Natural Earth; countries fade in, largest first",
      "geo::base(c, map, {}, u);  geo::choropleth(c, map, values, {.scale = s}, u);  geo::colorbar(c, box, s);" },
    { "The same map, over time", "a geo::Series on one colour scale; the values are illustrative",
      "geo::Series::from(table);  choropleth(c, map, series.at(year), {.scale = series.scale()}, u);" },
    { "A route, and where it has got to", "great circles, drawn on; the distance is computed by geo::length",
      "geo::route(c, map, stops, style, u);  geo::along(stops, u, &bearing);  geo::length(stops);" },
    { "A globe flies to a country", "one projection turns and zooms into the next, the country is picked out",
      "geo::Projection::fly(globe, close, u);  geo::highlight(c, view, region, style, u);  geo::labels(...)" },
    { "Two prefabs, a line of props each", "a route with stops, and a flight to a country",
      "prefab::use(n, 0, len, \"geo-route\", props);   prefab::use(n, 0, len, \"geo-flyto\", props);" },
};

}  // namespace

void registerMaps2D(Registry& out) {
    Comp c;
    c.name = "maps-2d";
    c.w = 1920; c.h = 1080; c.fps = FPS;
    c.background = mkColor(0x0d1118);
    TransitionSeries ts(c.roots, (float)c.w, (float)c.h);

    TNode* a = ts.add(LEN);
    choroplethChapter(a, LEN);
    caption(a, LEN, CHAPTERS[0]);

    ts.transition(Transition::fade(XF));
    TNode* b = ts.add(LEN);
    seriesChapter(b, LEN);
    caption(b, LEN, CHAPTERS[1]);

    ts.transition(Transition::fade(XF));
    TNode* r = ts.add(LEN);
    routeChapter(r, LEN);
    caption(r, LEN, CHAPTERS[2]);

    ts.transition(Transition::fade(XF));
    TNode* f = ts.add(LEN);
    flyChapter(f, LEN);
    caption(f, LEN, CHAPTERS[3]);

    ts.transition(Transition::wipe(XF, Direction::FromRight));
    TNode* p = ts.add(LEN);
    prefabChapter(p, LEN);
    caption(p, LEN, CHAPTERS[4]);

    c.duration = ts.end();
    out.push_back(std::move(c));
}

}  // namespace rmtn
