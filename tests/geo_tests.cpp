// geo_tests.cpp -- maps: the bundled Natural Earth layers and lookups, GeoJSON and
// TopoJSON parsing, measures on the sphere, and projections (formulas, fit, clipping at
// the antimeridian and the horizon, mixes), and the map helpers that need no window
// (flights, focus, scales and series, routes). The projected paths are measured.
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace rmtn;

static const double kPi = 3.14159265358979323846;   // raylib's kPi is a float

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
#define NEAR(a, b, eps) CHECK(std::fabs((double)(a) - (double)(b)) <= (eps))

template <class F> static bool throws(F f) {
    try { f(); } catch (const std::runtime_error&) { return true; }
    return false;
}

// Signed area of a path's flattened contours (screen px; y down, so the map's
// anticlockwise rings come out negative).
static double signedArea(const vg::Path& p) {
    double a = 0;
    for (const vg::Polyline& l : p.flatten(0.1f))
        for (size_t i = 0; i < l.pts.size(); i++) {
            const Vector2 u = l.pts[i], v = l.pts[(i + 1) % l.pts.size()];
            a += (double)u.x * v.y - (double)v.x * u.y;
        }
    return a / 2;
}

static void data() {
    const geo::Layer& world = geo::countries();
    CHECK(world.size() == 177);
    CHECK(&world == &geo::countries());                         // loaded once
    const geo::Feature& kenya = world.get("KEN");
    CHECK(kenya.name() == "Kenya");
    CHECK(world.find("ke") == &kenya && world.find("kenya") == &kenya);
    CHECK(kenya.properties["continent"].asString() == "Africa");
    CHECK(world.find("Atlantis") == nullptr);
    CHECK(throws([&] { world.get("Atlantis"); }));
    CHECK(world.find("United States of America") != nullptr && world.find("USA") == world.find("us"));

    CHECK(geo::land().size() > 100 && geo::lakes().size() > 10 && geo::rivers().size() > 5);
    const geo::Layer& cities = geo::cities();
    CHECK(cities.size() > 200);
    CHECK(cities.features[0].properties["pop"].asNumber() >= cities.features[1].properties["pop"].asNumber());
    CHECK(cities.find("Nairobi") != nullptr && cities.get("Nairobi").geometry.points.size() == 1);

    const geo::Layer africa = world.filter([](const geo::Feature& f) { return f.properties["continent"].asString() == "Africa"; });
    CHECK(africa.size() > 40 && africa.size() < 60);
    CHECK(africa.merged().polygons.size() >= africa.size());

    // Every ring was normalised, so every country (and all land) has a positive area.
    double total = 0;
    for (const geo::Feature& f : world.features) {
        const double a = geo::area(f.geometry);
        CHECK(a > 0);
        total += a;
    }
    NEAR(geo::area(kenya.geometry), 582000, 60000);
    NEAR(geo::area(world.get("RUS").geometry), 17.1e6, 1.5e6);   // crosses the antimeridian
    NEAR(geo::area(world.get("ATA").geometry), 12.3e6, 2.0e6);   // around the pole (NE's Antarctica is ~12.3e6)
    NEAR(geo::area(geo::land().merged()), 1.49e8, 0.08e8);
    CHECK(total > 1.3e8 && total < 1.6e8);

    const geo::LonLat nairobi = cities.get("Nairobi").geometry.points[0];
    CHECK(geo::contains(kenya.geometry, nairobi));
    CHECK(!geo::contains(kenya.geometry, { 0, 0 }));
    const geo::LonLat c = geo::centroid(kenya.geometry);
    CHECK(geo::contains(kenya.geometry, c));
    NEAR(c.lon, 37.8, 1.5);
    NEAR(kenya.labelPoint().lon, 37.9, 1.0);
    // Away from the equator too (the latitude once came out near zero for any small area).
    const geo::LonLat at = geo::centroid(world.get("AUT").geometry), au = geo::centroid(world.get("AUS").geometry);
    NEAR(at.lon, 14.1, 1.0);
    NEAR(at.lat, 47.6, 0.6);
    CHECK(geo::contains(world.get("AUT").geometry, at));
    NEAR(au.lat, -25.5, 2.0);
    const geo::LonLat line = geo::centroid(geo::Geometry::line({ { 10, 60 }, { 20, 60 } }));
    NEAR(line.lon, 15, 1e-6);
    CHECK(line.lat > 60 && line.lat < 60.5);
    geo::Geometry pts;
    pts.points = { { 10, 50 }, { 10, 70 } };
    NEAR(geo::centroid(pts).lat, 60, 1e-6);
    const geo::Bounds b = geo::bounds(kenya.geometry);
    CHECK(b.west > 33 && b.east < 42.5 && b.south > -5 && b.north < 5.7);
}

static void parsing() {
    const geo::Layer one = geo::parse(R"({"type":"Feature","id":7,"properties":{"name":"Box"},
        "geometry":{"type":"Polygon","coordinates":[[[0,0],[0,10],[10,10],[10,0],[0,0]],[[2,2],[4,2],[4,4],[2,4],[2,2]]]}})");
    CHECK(one.size() == 1 && one.features[0].id == "7" && one.features[0].name() == "Box");
    const geo::Geometry& box = one.features[0].geometry;
    CHECK(box.polygons.size() == 1 && box.polygons[0].size() == 2 && box.polygons[0][0].size() == 4);   // closing point dropped
    CHECK(geo::area(box) > 0);                                   // clockwise outer ring was turned round
    CHECK(geo::contains(box, { 5, 5 }) && !geo::contains(box, { 3, 3 }) && !geo::contains(box, { 11, 5 }));

    const geo::Layer bare = geo::parse(R"({"type":"GeometryCollection","geometries":[
        {"type":"Point","coordinates":[1,2]},{"type":"LineString","coordinates":[[0,0],[5,5]]}]})");
    CHECK(bare.size() == 1 && bare.features[0].geometry.points.size() == 1 && bare.features[0].geometry.lines.size() == 1);

    // TopoJSON: two unit squares sharing their middle edge (arc 1), quantized and delta-coded.
    const std::string topo = R"({"type":"Topology","transform":{"scale":[1,1],"translate":[10,20]},
        "objects":{"squares":{"type":"GeometryCollection","geometries":[
            {"type":"Polygon","id":"west","arcs":[[0,1]]},
            {"type":"Polygon","id":"east","properties":{"name":"East"},"arcs":[[2,-2]]}]},
          "dot":{"type":"Point","coordinates":[1,1]}},
        "arcs":[[[1,0],[-1,0],[0,1],[1,0]],[[1,1],[0,-1]],[[1,0],[1,0],[0,1],[-1,0]]]})";
    const geo::Layer all = geo::parse(topo);
    CHECK(all.size() == 3);
    const geo::Layer squares = geo::parse(topo, "squares");
    CHECK(squares.size() == 2 && squares.get("east").name() == "East");
    const geo::Ring& west = squares.get("west").geometry.polygons[0][0];
    CHECK(west.size() == 4);
    bool corner = false;
    for (const geo::LonLat& p : west) corner = corner || (p.lon == 10 && p.lat == 21);
    CHECK(corner);
    CHECK(geo::contains(squares.get("east").geometry, { 11.5, 20.5 }) && !geo::contains(squares.get("west").geometry, { 11.5, 20.5 }));
    CHECK(all.features[2].geometry.points[0].lon == 11 && all.features[2].geometry.points[0].lat == 21);
    CHECK(throws([&] { geo::parse(topo, "nope"); }));

    CHECK(throws([] { geo::parse("{\"type\":\"Polygon\",\"coordinates\":[[[0]]]}"); }));
    CHECK(throws([] { geo::parse("{\"type\":\"Blob\",\"coordinates\":[]}"); }));
    CHECK(throws([] { geo::parse("not json"); }));
    CHECK(throws([] { geo::load("data/geo/no-such-file.geojson"); }));
}

static void sphere() {
    const geo::LonLat london{ -0.1276, 51.5072 }, paris{ 2.3522, 48.8566 }, sydney{ 151.21, -33.87 };
    NEAR(geo::distance(london, paris), 344, 3);
    NEAR(geo::distance(london, sydney), 16990, 60);
    NEAR(geo::angle({ 0, 0 }, { 90, 0 }), 90, 1e-9);
    const geo::LonLat mid = geo::interpolate({ 0, 0 }, { 90, 0 }, 0.5);
    NEAR(mid.lon, 45, 1e-9);
    NEAR(mid.lat, 0, 1e-9);
    NEAR(geo::bearing({ 0, 0 }, { 0, 10 }), 0, 1e-9);
    NEAR(geo::bearing({ 0, 0 }, { 10, 0 }), 90, 1e-9);
    NEAR(geo::bearing({ 0, 0 }, { -10, 0 }), 270, 1e-9);
    NEAR(geo::interpolate(london, sydney, 1).lon, sydney.lon, 1e-6);
}

static void formulas() {
    using geo::Projection;
    auto unit = [](Projection p) { return p.scale(1).translate({ 0, 0 }); };
    const float PI_F = (float)kPi;
    Vector2 v = unit(Projection::equirectangular())({ 90, 45 });
    NEAR(v.x, PI_F / 2, 1e-5);
    NEAR(v.y, -PI_F / 4, 1e-5);                                  // north is up (smaller y)
    NEAR(unit(Projection::mercator())({ 0, 45 }).y, -0.881374, 1e-5);
    NEAR(unit(Projection::robinson())({ 180, 0 }).x, 0.8487 * kPi, 1e-4);
    NEAR(unit(Projection::robinson())({ 0, 90 }).y, -1.3523, 1e-4);
    NEAR(unit(Projection::equalEarth())({ 0, 0 }).x, 0, 1e-6);
    NEAR(unit(Projection::equalEarth())({ 180, 0 }).x, 2.7066, 1e-3);   // d3: 2.7066...
    NEAR(unit(Projection::mollweide())({ 0, 90 }).y, -std::sqrt(2.0), 1e-4);
    NEAR(unit(Projection::mollweide())({ 180, 0 }).x, 2 * std::sqrt(2.0), 1e-4);
    NEAR(unit(Projection::orthographic())({ 90, 0 }).x, 1, 1e-6);
    NEAR(unit(Projection::stereographic())({ 90, 0 }).x, 1, 1e-6);
    NEAR(unit(Projection::azimuthalEquidistant())({ 90, 0 }).x, kPi / 2, 1e-6);
    NEAR(unit(Projection::azimuthalEqualArea())({ 90, 0 }).x, std::sqrt(2.0), 1e-6);
    NEAR(unit(Projection::gnomonic())({ 45, 0 }).x, 1, 1e-6);

    // Rotation brings the centre to the middle; a globe hides the far side.
    Projection globe = Projection::orthographic().rotate({ 37, -1 }).scale(100).translate({ 500, 500 });
    v = globe({ 37, -1 });
    NEAR(v.x, 500, 1e-3);
    NEAR(v.y, 500, 1e-3);
    Vector2 out;
    CHECK(globe.project({ 40, 5 }, out) && !globe.project({ 37 + 180, 1 }, out));
    CHECK(globe({ 47, -1 }).x > 500);                            // east is to the right
    CHECK(Projection::equirectangular().project({ 179, 0 }, out));
    // A roll turns the map about the centre: north becomes east under a 90 degree roll.
    Projection rolled = Projection::azimuthalEqualArea().rotate({ 0, 0 }, 90).scale(100).translate({ 0, 0 });
    v = rolled({ 0, 10 });
    CHECK(std::fabs(v.x) > 10 && std::fabs(v.y) < 1e-3);

    // Conics: the standard parallels keep their true scale, and meridians converge northward.
    Projection albers = Projection::conicEqualArea().parallels(29.5, 45.5).rotate({ -96, 0 }).scale(1).translate({ 0, 0 });
    const Vector2 a = albers({ -100, 30 }), b = albers({ -100, 50 });
    CHECK(b.y < a.y);
    const float w30 = albers({ -90, 30 }).x - albers({ -100, 30 }).x, w50 = albers({ -90, 50 }).x - albers({ -100, 50 }).x;
    CHECK(w50 < w30);
    Projection lambert = Projection::conicConformal().parallels(33, 45).rotate({ -96, 0 }).scale(1).translate({ 0, 0 });
    CHECK(lambert({ -100, 50 }).y < lambert({ -100, 30 }).y);
    Projection eqd = Projection::conicEquidistant().parallels(20, 60).scale(1).translate({ 0, 0 });
    NEAR(eqd({ 0, 30 }).y - eqd({ 0, 40 }).y, 10 * kPi / 180, 1e-5);   // equidistant along meridians
}

static void fitting() {
    using geo::Projection;
    const Rectangle box{ 100, 50, 1600, 900 };
    Projection map = Projection::equalEarth().fit(box, geo::Sphere);
    Rectangle r = map.outline().bounds();
    CHECK(std::fabs(r.width - box.width) < 1.5f || std::fabs(r.height - box.height) < 1.5f);
    CHECK(r.x >= box.x - 1 && r.y >= box.y - 1 && r.x + r.width <= box.x + box.width + 1 && r.y + r.height <= box.y + box.height + 1);
    NEAR(r.x + r.width / 2, box.x + box.width / 2, 1);
    NEAR(r.y + r.height / 2, box.y + box.height / 2, 1);

    // Fitting a country frames it, with padding.
    Projection kenya = Projection::mercator().fit(box, geo::countries().get("KEN").geometry, 40);
    r = kenya.path(geo::countries().get("KEN").geometry).bounds();
    CHECK(std::fabs(r.height - (box.height - 80)) < 2 || std::fabs(r.width - (box.width - 80)) < 2);
    CHECK(r.x >= box.x + 39 && r.y >= box.y + 39);

    const geo::Layer one = geo::countries().filter([](const geo::Feature& f) { return f.id == "FRA"; });
    CHECK(one.size() == 1);
    const Rectangle f = Projection::equirectangular().fit(box, one).path(one.merged()).bounds();
    CHECK(f.width > 0 && f.height > 0);
    const Rectangle e = Projection::equirectangular().fit(box, geo::Geometry{}).outline().bounds();   // empty: the sphere
    NEAR(e.width, box.width, 1.5);

    const Rectangle g = Projection::orthographic().rotate({ 20, 10 }).fit({ 0, 0, 800, 800 }, geo::Sphere).outline().bounds();
    NEAR(g.width, 800, 1.5);
    NEAR(g.height, 800, 1.5);
}

// Equal-area projections keep area fractions, so the land's share of the map is the land's
// share of the sphere (about 29%), however the map is rotated -- as long as the pieces
// cut at the antimeridian or the horizon are closed the right way round.
static void clipping() {
    using geo::Projection;
    const geo::Geometry land = geo::land().merged();
    const double share = geo::area(land) / (4 * kPi * geo::EARTH_RADIUS_KM * geo::EARTH_RADIUS_KM);
    CHECK(share > 0.27 && share < 0.31);
    const geo::LonLat turns[] = { { 0, 0 }, { 100, 0 }, { -160, 30 }, { 37, -60 }, { 180, 0 }, { 10, 89 } };
    for (const geo::LonLat t : turns) {
        for (Projection p : { Projection::equalEarth(), Projection::mollweide() }) {
            p.rotate(t).fit({ 0, 0, 1000, 500 }, geo::Sphere);
            const double whole = std::fabs(signedArea(p.outline()));
            const double a = signedArea(p.path(land));
            CHECK(a < 0);                                        // consistently anticlockwise
            if (std::fabs(-a / whole - share) > 0.01)
                std::fprintf(stderr, "  land share %.4f vs %.4f (rotate %g %g, kind %d)\n", -a / whole, share, t.lon, t.lat, (int)p.kind());
            NEAR(-a / whole, share, 0.01);
            const Rectangle b = p.path(land).bounds(), o = p.outline().bounds();
            CHECK(b.x >= o.x - 1 && b.x + b.width <= o.x + o.width + 1);   // nothing wraps across the map
        }
    }

    // A hemisphere (azimuthal equal-area clipped at 90 degrees): the land seen from each
    // side, measured on the map, matches a count of sample points on the sphere.
    auto seen = [&](geo::LonLat centre) {
        int in = 0, total = 0;
        const int N = 6000;
        for (int i = 0; i < N; i++) {                          // a Fibonacci sphere
            const double z = 1 - (i + 0.5) * 2 / N, rr = std::sqrt(1 - z * z), lon = std::fmod(i * 137.50776405, 360) - 180;
            const geo::LonLat q{ lon, std::asin(z) * 180 / kPi };
            if (geo::angle(q, centre) > 90) continue;
            total++;
            (void)rr;
            if (geo::contains(land, q)) in++;
        }
        return (double)in / total;
    };
    for (const geo::LonLat t : { geo::LonLat{ 0, 0 }, geo::LonLat{ 160, 0 }, geo::LonLat{ 0, -90 }, geo::LonLat{ -100, 45 } }) {
        Projection p = Projection::azimuthalEqualArea().clipAngle(90).rotate(t).fit({ 0, 0, 800, 800 }, geo::Sphere);
        const double whole = std::fabs(signedArea(p.outline()));
        const double a = -signedArea(p.path(land)) / whole, expect = seen(t);
        if (std::fabs(a - expect) > 0.025) std::fprintf(stderr, "  hemisphere %g %g: %.4f vs %.4f\n", t.lon, t.lat, a, expect);
        NEAR(a, expect, 0.025);
    }

    // The orthographic globe: the disc is covered by one ocean-free polygon when the whole
    // visible side is inside it (a cap bigger than a hemisphere).
    geo::Geometry cap;
    geo::Ring ring;
    for (int i = 0; i < 72; i++) ring.push_back({ i * 5.0 - 180, -30 });   // eastward: the north is inside
    cap.polygons.push_back({ ring });
    Projection globe = Projection::orthographic().rotate({ 0, 90 }).fit({ 0, 0, 400, 400 }, geo::Sphere);
    NEAR(std::fabs(signedArea(globe.path(cap))), std::fabs(signedArea(globe.outline())), 50);
    geo::Geometry north;
    geo::Ring top;
    for (int i = 0; i < 72; i++) top.push_back({ i * 5.0 - 180, 10 });
    north.polygons.push_back({ top });
    globe.rotate({ 0, -90 });
    CHECK(globe.path(north).empty());                            // looking at the south pole: none of it shows
    // Lines are cut, not closed.
    const vg::Path grid = Projection::orthographic().rotate({ 30, 20 }).fit({ 0, 0, 400, 400 }, geo::Sphere).graticule(30);
    CHECK(!grid.empty());
    for (const vg::Contour& c : grid.contours) CHECK(!c.closed || c.segs.size() > 10);
}

static void mixing() {
    using geo::Projection;
    const Rectangle box{ 0, 0, 1000, 500 };
    const Projection flat = Projection::equirectangular().fit(box, geo::Sphere);
    const Projection globe = Projection::orthographic().rotate({ 40, 10 }).fit({ 250, 0, 500, 500 }, geo::Sphere);
    const Vector2 p0 = Projection::mix(flat, globe, 0)({ 10, 10 }), p1 = Projection::mix(flat, globe, 1)({ 10, 10 });
    const Vector2 f = flat({ 10, 10 }), g = globe({ 10, 10 });
    NEAR(p0.x, f.x, 1e-3);
    NEAR(p0.y, f.y, 1e-3);
    NEAR(p1.x, g.x, 1e-3);
    NEAR(p1.y, g.y, 1e-3);
    const Projection half = Projection::mix(flat, globe, 0.5);
    CHECK(!half.path(geo::land().merged()).empty());
    CHECK(!half.outline().empty());
    const Rectangle r = Projection(half).fit(box, geo::Sphere).outline().bounds();
    CHECK(r.width <= box.width + 1.5f && r.height <= box.height + 1.5f);
    CHECK(std::fabs(r.width - box.width) < 1.5f || std::fabs(r.height - box.height) < 1.5f);
}

static double pathLength(const vg::Path& p) { return p.empty() ? 0 : p.length(); }

static void flying() {
    using geo::Projection;
    const Rectangle box{ 0, 0, 1000, 600 };
    const Projection a = Projection::orthographic().rotate({ 106.8, -6.2 }).scale(400).translate({ 500, 300 });
    const Projection b = Projection::orthographic().rotate({ 16.4, 48.2 }).scale(1600).translate({ 500, 300 });
    // The ends are the two views.
    for (const geo::LonLat q : { geo::LonLat{ 100, 0 }, geo::LonLat{ 20, 45 } }) {
        const Vector2 a0 = Projection::fly(a, b, 0)(q), b1 = Projection::fly(a, b, 1)(q);
        NEAR(a0.x, a(q).x, 1e-2);
        NEAR(a0.y, a(q).y, 1e-2);
        NEAR(b1.x, b(q).x, 1e-2);
        NEAR(b1.y, b(q).y, 1e-2);
    }
    // Halfway between two views of the same scale, told only to pan: the centre is
    // halfway along the great circle.
    const Projection b400 = Projection::orthographic().rotate({ 16.4, 48.2 }).scale(400).translate({ 500, 300 });
    const Projection pan = Projection::fly(a, b400, 0.5, 1);
    const geo::LonLat want = geo::interpolate({ 106.8, -6.2 }, { 16.4, 48.2 }, 0.5);
    NEAR(pan.rotation().lon, want.lon, 1e-6);
    NEAR(pan.rotation().lat, want.lat, 1e-6);
    NEAR(pan.scaleValue(), 400, 1e-6);
    NEAR(pan.translation().x, 500, 1e-3);
    // A long trip zooms out on its own; the zoom can also be given.
    const Projection mid = Projection::fly(a, b, 0.5);
    CHECK(mid.scaleValue() < 800 - 1 && mid.scaleValue() >= 100 - 1e-6);
    NEAR(Projection::fly(a, b, 0.5, 1).scaleValue(), 800, 1e-6);
    NEAR(Projection::fly(a, b, 0.5, 4).scaleValue(), 200, 1e-6);
    // Zooming in, most of the way is covered early (while still far out), and the centre
    // only ever moves forward along the great circle.
    const double whole = geo::angle({ 106.8, -6.2 }, { 16.4, 48.2 });
    CHECK(geo::angle({ 106.8, -6.2 }, Projection::fly(a, b, 0.5, 1).rotation()) > whole * 0.55);
    double gone = 0;
    for (int i = 1; i <= 50; i++) {
        const geo::LonLat c = Projection::fly(a, b, i / 50.0).rotation();
        const double d = geo::angle({ 106.8, -6.2 }, c);
        CHECK(d >= gone - 1e-9);
        NEAR(d + geo::angle(c, { 16.4, 48.2 }), whole, 1e-6);
        gone = d;
    }
    // Views that are not centred on their own middle (a region fitted in a box): the
    // target ends in place and, halfway, the globe is still on screen.
    const geo::Geometry& at = geo::countries().get("AUT").geometry;
    const geo::Geometry& id = geo::countries().get("IDN").geometry;
    const Projection fa = geo::focus(Projection::orthographic(), box, id, 100), fb = geo::focus(Projection::orthographic(), box, at, 200);
    for (int i = 0; i <= 20; i++) {
        const Projection v = Projection::fly(fa, fb, i / 20.0);
        const Rectangle disc = v.outline().bounds();
        CHECK(disc.x < 700 && disc.x + disc.width > 300 && disc.y < 450 && disc.y + disc.height > 150);
    }
    const Vector2 e1 = Projection::fly(fa, fb, 1)(geo::centroid(at)), e2 = fb(geo::centroid(at));
    NEAR(e1.x, e2.x, 1e-2);
    NEAR(e1.y, e2.y, 1e-2);
    // A short hop does not zoom out.
    const Projection near = Projection::orthographic().rotate({ 107.5, -6.5 }).scale(400).translate({ 500, 300 });
    NEAR(Projection::fly(a, near, 0.5).scaleValue(), 400, 1e-6);
    // The scale never jumps between frames.
    double prev = a.scaleValue();
    for (int i = 1; i <= 100; i++) {
        const double k = Projection::fly(a, b, i / 100.0).scaleValue();
        CHECK(std::fabs(std::log(k / prev)) < 0.12);
        prev = k;
    }
    // Different kinds blend as mix() does, and still draw.
    const Projection flat = Projection::equalEarth().fit(box, geo::Sphere);
    const Projection across = Projection::fly(a, flat, 0.5);
    CHECK(!across.path(geo::land().merged()).empty());
    NEAR(Projection::fly(a, flat, 1)({ 10, 10 }).x, flat({ 10, 10 }).x, 1e-2);

    // focus: a globe turns to face the region, a world map keeps north up and only zooms.
    const geo::Geometry& austria = geo::countries().get("AUT").geometry;
    const geo::LonLat mid2 = geo::centroid(austria);
    const Projection g = geo::focus(a, box, austria, 40);
    NEAR(g.rotation().lon, mid2.lon, 1e-9);
    NEAR(g.rotation().lat, mid2.lat, 1e-9);
    const Rectangle gb = g.path(austria).bounds();
    CHECK(gb.x >= 39 && gb.y >= 39 && gb.x + gb.width <= 961 && gb.y + gb.height <= 561);
    CHECK(std::fabs(gb.width - 920) < 2 || std::fabs(gb.height - 520) < 2);
    const Projection f = geo::focus(flat, box, austria, 40);
    NEAR(f.rotation().lon, 0, 1e-12);
    NEAR(f.rotation().lat, 0, 1e-12);
    CHECK(f.scaleValue() > flat.scaleValue() * 10);
    const Projection conic = geo::focus(Projection::conicEqualArea().rotate({ 0, 0 }), box, austria);
    NEAR(conic.rotation().lon, mid2.lon, 1e-9);
    NEAR(conic.rotation().lat, 0, 1e-12);
}

static void mapData() {
    // Scale: the range from the data, from zero when nothing is negative.
    const geo::Values v = { { "IDN", 3 }, { "IND", 9 }, { "DEU", 6 } };
    const geo::Scale s = geo::Scale{}.over(v);
    NEAR(s.min, 0, 1e-12);
    NEAR(s.max, 9, 1e-12);
    NEAR(s.t(4.5), 0.5, 1e-6);
    geo::Scale fixed;
    fixed.min = 2;
    fixed.max = 4;
    fixed.colors = [](float t) { return Color{ (unsigned char)(t * 255), 0, 0, 255 }; };
    CHECK(fixed.over(v).max == 4 && fixed(3).r == 127 && fixed(99).r == 255);

    // Series: from a table, blended between columns, held outside them.
    const plot::Table t = plot::Table::parseCsv("code,2000,2010,2020\nIDN,214,244,274\nIND,1060,1240,1400\n");
    const geo::Series se = geo::Series::from(t);
    CHECK(se.times.size() == 3 && se.times[1] == 2010 && se.rows.size() == 2 && se.rows[0].first == "IDN");
    NEAR(se.at(2005)[0].second, 229, 1e-9);
    NEAR(se.at(2015)[1].second, 1320, 1e-9);
    NEAR(se.at(1900)[0].second, 214, 1e-9);
    NEAR(se.at(2100)[1].second, 1400, 1e-9);
    NEAR(se.at(2010)[0].second, 244, 1e-9);
    NEAR(se.scale().max, 1400, 1e-9);
    const plot::Table named = plot::Table::parseCsv("code,before,after\nIDN,1,3\n");
    const geo::Series ns = geo::Series::from(named);
    CHECK(ns.times[0] == 0 && ns.times[1] == 1);
    NEAR(ns.at(0.5)[0].second, 2, 1e-9);
    CHECK(geo::Series{}.at(1).empty());

    // Markers and labels from layers.
    const auto big = geo::places(geo::cities(), 5);
    CHECK(big.size() == 5 && !big[0].label.empty() && big[0].value >= big[4].value && big[0].value > 1e6);
    const geo::Feature& first = geo::cities().features[0];
    NEAR(big[0].at.lon, first.geometry.points[0].lon, 1e-12);
    CHECK(geo::places(geo::cities()).size() == geo::cities().size());
    const auto nm = geo::names(geo::countries(), 3);
    CHECK(nm.size() == 3 && nm[0].text == geo::countries().features[0].name());
    NEAR(nm[0].at.lat, geo::countries().features[0].labelPoint().lat, 1e-12);
}

static void routes() {
    using geo::Projection;
    const geo::LonLat jakarta{ 106.8, -6.2 }, dubai{ 55.3, 25.3 }, vienna{ 16.4, 48.2 };
    const std::vector<geo::LonLat> trip = { jakarta, dubai, vienna };
    const double d1 = geo::distance(jakarta, dubai), d2 = geo::distance(dubai, vienna);
    NEAR(geo::length(trip), d1 + d2, 1e-6);
    CHECK(geo::length({ jakarta }) == 0 && geo::length({}) == 0);

    // along: by distance, through the stops, with the heading.
    double heading = -1;
    geo::LonLat p = geo::along(trip, 0, &heading);
    NEAR(p.lon, jakarta.lon, 1e-9);
    NEAR(heading, geo::bearing(jakarta, dubai), 1e-6);
    p = geo::along(trip, d1 / (d1 + d2));
    NEAR(p.lon, dubai.lon, 1e-6);
    NEAR(p.lat, dubai.lat, 1e-6);
    p = geo::along(trip, 1, &heading);
    NEAR(p.lon, vienna.lon, 1e-9);
    CHECK(heading > 270 && heading < 320);                       // arriving west-north-west
    p = geo::along(trip, 0.5 * d1 / (d1 + d2));
    NEAR(geo::distance(jakarta, p), d1 / 2, 1e-6);
    p = geo::along(trip, 7);
    NEAR(p.lat, vienna.lat, 1e-9);
    CHECK(geo::along({ dubai }, 0.3).lon == dubai.lon);

    // route: grows with the fraction, ends where along() says, and a part is a part.
    const Projection flat = Projection::equirectangular().fit({ 0, 0, 1000, 500 }, geo::Sphere);
    const vg::Path whole = geo::route(flat, trip), half = geo::route(flat, trip, 0, 0.5), back = geo::route(flat, trip, 0.5, 1);
    CHECK(whole.contours.size() == 1 && !whole.contours[0].closed);
    CHECK(pathLength(half) > 0 && pathLength(half) < pathLength(whole));
    NEAR(pathLength(half) + pathLength(back), pathLength(whole), 0.5);
    const Vector2 end = half.pointAt(1), want = flat(geo::along(trip, 0.5));
    NEAR(end.x, want.x, 0.05);
    NEAR(end.y, want.y, 0.05);
    CHECK(geo::route(flat, trip, 0.6, 0.6).empty() && geo::route(flat, { jakarta }).empty() && geo::route(flat, trip, 0.8, 0.2).empty());
    // A great circle, not a straight line on the map: Tokyo to Los Angeles bows north.
    const geo::LonLat tokyo{ 139.7, 35.7 }, la{ -118.2, 34.1 };
    const Projection pacific = Projection::equirectangular().rotate({ 180, 0 }).fit({ 0, 0, 1000, 500 }, geo::Sphere);
    const vg::Path arc = geo::route(pacific, { tokyo, la });
    CHECK(arc.contours.size() == 1);
    CHECK(arc.bounds().y < pacific(tokyo).y - 20);
    // Across the antimeridian on an unrotated map it is cut in two, not drawn across the world.
    const vg::Path cut = geo::route(flat, { tokyo, la });
    CHECK(cut.contours.size() == 2);
    CHECK(pathLength(cut) < 400);
    // On a globe, the part round the back is clipped away.
    const Projection globe = Projection::orthographic().rotate(jakarta).fit({ 0, 0, 600, 600 }, geo::Sphere);
    const vg::Path seen = geo::route(globe, { jakarta, la });
    CHECK(!seen.empty());
    Vector2 hidden;
    CHECK(!globe.project(la, hidden));
    const Rectangle disc = globe.outline().bounds(), sb = seen.bounds();
    CHECK(sb.x >= disc.x - 1 && sb.y >= disc.y - 1 && sb.x + sb.width <= disc.x + disc.width + 1 && sb.y + sb.height <= disc.y + disc.height + 1);
}

static void prefabs() {
    using geo::Projection;
    CHECK(Projection::named("equalEarth").kind() == geo::Kind::EqualEarth);
    CHECK(Projection::named("conicConformal").kind() == geo::Kind::ConicConformal);
    CHECK(throws([] { Projection::named("flat"); }));

    geo::registerPrefabs();
    geo::registerPrefabs();                                      // twice is fine
    const auto names = prefab::names();
    auto has = [&](const std::string& n) { return std::find(names.begin(), names.end(), n) != names.end(); };
    CHECK(has("geo-route") && has("geo-flyto"));
    Nodes roots;
    TNode* route = prefab::use(roots, 0, 90, "geo-route", Json::parse(R"({"stops": [[106.85, -6.21], [55.27, 25.2], [16.37, 48.21]],
        "labels": ["Jakarta", "Dubai", "Vienna"], "projection": "orthographic", "box": [100, 100, 800, 600]})"));
    CHECK(route && route->fn);
    TNode* fly = prefab::use(roots, 0, 90, "geo-flyto", Json::parse(R"({"from": "IDN", "to": "Austria", "projection": "orthographic"})"));
    CHECK(fly && fly->fn);
    CHECK(prefab::use(roots, 0, 30, "geo-route")->fn && prefab::use(roots, 0, 30, "geo-flyto")->fn);   // the defaults
    CHECK(throws([&] { prefab::use(roots, 0, 30, "geo-route", Json::parse(R"({"stops": [[10, 10]]})")); }));
    CHECK(throws([&] { prefab::use(roots, 0, 30, "geo-route", Json::parse(R"({"projection": "flat"})")); }));
    CHECK(throws([&] { prefab::use(roots, 0, 30, "geo-route", Json::parse(R"({"fit": "tight"})")); }));
    CHECK(throws([&] { prefab::use(roots, 0, 30, "geo-route", Json::parse(R"({"box": [0, 0, 0, 100]})")); }));
    CHECK(throws([&] { prefab::use(roots, 0, 30, "geo-flyto", Json::parse(R"({"to": "Atlantis"})")); }));
}

int main() {
    data();
    parsing();
    sphere();
    formulas();
    fitting();
    clipping();
    mixing();
    flying();
    mapData();
    routes();
    prefabs();
    if (failures) std::fprintf(stderr, "%d geo check(s) failed\n", failures);
    else std::printf("geo: all checks passed\n");
    return failures ? 1 : 0;
}
