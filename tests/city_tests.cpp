// city_tests.cpp -- CityJSON: the bundled sample town (objects, levels of detail,
// semantics, parts, templates, the origin), CityJSON 1.0 and CityJSONSeq input, errors,
// triangulation (areas, holes, concave faces, winding), paths (along, trails) and camera
// projection, then rendering (GL): a city drawn with semantic colours, rising from the
// ground, and framed by the camera.
//
// With RMTN_CITYJSON_DIR set, every *.json / *.jsonl file there is read and triangulated
// too (real data that is not bundled).
#include "rmtn/raymotion.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
#define NEAR(a, b, eps) CHECK(std::fabs((double)(a) - (double)(b)) <= (eps))

template <class F> static bool throws(F f) {
    try { f(); } catch (const std::runtime_error&) { return true; }
    return false;
}

// Area of a face by its triangles, and the true area (outer ring less holes, by Newell).
static double triangleArea(const city::Face& f, const std::vector<int>& tris) {
    std::vector<Vector3> pts;
    for (const auto& r : f.rings) pts.insert(pts.end(), r.begin(), r.end());
    double a = 0;
    for (size_t i = 0; i + 2 < tris.size(); i += 3) {
        const Vector3 p = pts[(size_t)tris[i]], q = pts[(size_t)tris[i + 1]], r = pts[(size_t)tris[i + 2]];
        a += Vector3Length(Vector3CrossProduct(Vector3Subtract(q, p), Vector3Subtract(r, p))) / 2;
    }
    return a;
}
static double polygonArea(const city::Face& f) {
    double a = 0;
    for (size_t k = 0; k < f.rings.size(); k++) {
        double x = 0, y = 0, z = 0;
        const auto& r = f.rings[k];
        for (size_t i = 0; i < r.size(); i++) {
            const Vector3 p = r[i], q = r[(i + 1) % r.size()];
            x += ((double)p.y - q.y) * ((double)p.z + q.z);
            y += ((double)p.z - q.z) * ((double)p.x + q.x);
            z += ((double)p.x - q.x) * ((double)p.y + q.y);
        }
        a += (k == 0 ? 1 : -1) * std::sqrt(x * x + y * y + z * z) / 2;
    }
    return a;
}
// Every face of a model triangulates to its own area, wound like the face.
static int checkTriangles(const city::Model& m, const char* what) {
    int bad = 0, faces = 0;
    for (const city::Object& o : m.objects)
        for (const city::Geometry& g : o.geometry)
            for (const city::Face& f : g.faces) {
                Vector3 n;
                const auto tris = city::triangles(f, &n);
                const double want = polygonArea(f);
                if (want < 1e-4) continue;   // slivers and zero-area faces in real data
                faces++;
                const double got = triangleArea(f, tris);
                bool wound = true;
                std::vector<Vector3> pts;
                for (const auto& r : f.rings) pts.insert(pts.end(), r.begin(), r.end());
                for (size_t i = 0; i + 2 < tris.size(); i += 3) {
                    const Vector3 p = pts[(size_t)tris[i]], q = pts[(size_t)tris[i + 1]], r = pts[(size_t)tris[i + 2]];
                    const Vector3 c = Vector3CrossProduct(Vector3Subtract(q, p), Vector3Subtract(r, p));
                    if (Vector3Length(c) > 1e-6f && Vector3DotProduct(c, n) < 0) wound = false;
                }
                if (std::fabs(got - want) > 0.01 * want + 1e-3 || !wound) {
                    if (bad < 5) std::fprintf(stderr, "  %s: %s face of %d rings: area %.4f, triangles %.4f%s\n", what, o.id.c_str(),
                                              (int)f.rings.size(), want, got, wound ? "" : " (wound backwards)");
                    bad++;
                }
            }
    std::printf("%s: %d objects, %d faces, %d bad\n", what, (int)m.objects.size(), faces, bad);
    return bad;
}

static void town() {
    const city::Model m = city::load("data/city/town.city.json");
    CHECK(m.version == "2.0");
    CHECK(m.metadata["title"].asString().find("synthetic") != std::string::npos);
    CHECK(m.objects.size() == 220);
    CHECK(m.lods() == std::vector<std::string>({ "0", "1", "1.2", "2", "2.2" }));

    // The origin is the middle of the extent at its lowest point, so local coordinates are small.
    NEAR(m.origin[0], 500000, 400);
    NEAR(m.origin[1], 5000000, 400);
    NEAR(m.origin[2], -0.6, 1e-6);                                  // the canal's water
    const BoundingBox b = m.bounds();
    NEAR(b.min.x, -b.max.x, 0.01);
    NEAR(b.min.y, -b.max.y, 0.01);
    NEAR(b.min.z, 0, 1e-4);
    NEAR(b.max.z, 82.6, 1e-3);                                      // the tower
    const auto w = m.world({ 1, 2, 3 });
    NEAR(w[0], m.origin[0] + 1, 1e-6);
    const Vector3 back = m.local(w);
    NEAR(back.y, 2, 1e-4);

    // Parts and attributes.
    const city::Object& tower = m.get("tower");
    CHECK(tower.geometry.empty() && tower.at() == nullptr);
    CHECK(tower.children.size() == 2 && m.get("tower-shaft").parents[0] == "tower");
    NEAR(tower.attributes["measuredHeight"].asNumber(), 82, 1e-9);
    NEAR(m.get("tower-shaft").height(), 70, 1e-3);
    CHECK(m.find("nope") == nullptr && throws([&] { m.get("nope"); }));

    // Levels of detail and semantics.
    const city::Object& house = m.get("house-0");
    CHECK(house.geometry.size() == 2);
    CHECK(house.at()->lod == "2.2" && house.at("1.2")->lod == "1.2" && house.at("3")->lod == "2.2");
    int roofs = 0, walls = 0, grounds = 0;
    for (const city::Face& f : house.at()->faces) {
        roofs += f.kind == "RoofSurface";
        walls += f.kind == "WallSurface";
        grounds += f.kind == "GroundSurface";
    }
    CHECK(roofs == 2 && walls == 4 && grounds == 1);
    CHECK(house.at("1.2")->faces.size() == 6);
    CHECK(house.height("2.2") > house.height("1.2") - 1e-3);       // LoD 1.2 is cut at the ridge too

    // The courtyard: roof and ground have a hole each.
    const city::Object& yard = m.get("courtyard");
    int holed = 0;
    for (const city::Face& f : yard.at()->faces)
        if (f.rings.size() == 2) {
            holed++;
            NEAR(triangleArea(f, city::triangles(f)), 52.0 * 42 - 28.0 * 22, 1e-2);
        }
    CHECK(holed == 2);

    // Templates: each tree is the template scaled and moved to its reference point.
    const city::Object& tree = m.get("tree-0");
    CHECK(tree.type == "SolitaryVegetationObject" && tree.at()->faces.size() == 9);
    const double s = tree.attributes["height"].asNumber() / 9;
    NEAR(tree.height(), 9 * s, 0.01);

    // Street lamps: a second template, turned so each arm reaches over its street.
    const city::Model lamps = m.filter([](const city::Object& o) { return o.type == "CityFurniture"; });
    CHECK(lamps.objects.size() == 24);
    const city::Object& lamp = m.get("lamp-0");
    CHECK(lamp.attributes["street"].asString() == "street-ew-1" && lamp.at()->faces.size() == 18);
    int heads = 0;
    for (const city::Face& f : lamp.at()->faces) heads += f.kind == "+Lamp";
    CHECK(heads == 6);
    NEAR(lamp.height(), 7.2, 1e-3);
    const BoundingBox lb = lamp.bounds();
    CHECK(lb.max.y - lb.min.y > 2 && lb.max.x - lb.min.x < 1);     // lamp-0's arm points north (+y)
    CHECK(city::colorOf(city::Look{}, lamp, city::Face{}).r == city::Look{}.furniture.r);

    // The bus route is a line, its stops points; neither has faces.
    const city::Object& route = m.get("bus-route");
    CHECK(route.at()->type == "MultiLineString" && route.at()->lod == "0");
    CHECK(route.at()->faces.empty() && route.at()->lines.size() == 1 && route.at()->lines[0].size() == 6);
    CHECK(m.get("bus-stops").at()->points.size() == 3);
    NEAR(route.at()->lines[0][0].z, 0.3 - m.origin[2], 1e-3);     // 0.3 m in the file, above the canal here

    const city::Model buildings = m.filter([](const city::Object& o) { return o.type == "Building"; });
    CHECK(buildings.objects.size() == 154 && buildings.origin == m.origin);
    CHECK(checkTriangles(m, "town") == 0);

    // Colours: semantics first, then the object type, then the slope.
    city::Look look;
    const city::Face roof = house.at()->faces.back();
    CHECK(colorOf(look, house, roof).r == look.roof.r);
    city::Face plain{ { { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } } }, "", Json() };
    CHECK(colorOf(look, house, plain).g == look.roof.g);            // facing up: a roof
    CHECK(colorOf(look, m.get("canal"), plain).b == look.water.b);
    look.color = [](const city::Object& o, const city::Face&) { return o.id == "house-0" ? GOLD : BLANK; };
    CHECK(colorOf(look, house, roof).r == GOLD.r && colorOf(look, m.get("house-1"), roof).r == look.roof.r);
}

static void formats() {
    // CityJSON 1.0 style: no transform, real coordinates, a numeric lod.
    const city::Model one = city::parse(R"({"type":"CityJSON","version":"1.0","CityObjects":{
        "a":{"type":"Building","geometry":[{"type":"MultiSurface","lod":2,"boundaries":[[[0,1,2,3]]]}]}},
        "vertices":[[10.5,20,1],[12.5,20,1],[12.5,23,1],[10.5,23,1]]})");
    CHECK(one.objects[0].geometry[0].lod == "2");
    NEAR(one.origin[0], 11.5, 1e-9);
    NEAR(one.origin[2], 1, 1e-9);
    NEAR(triangleArea(one.objects[0].geometry[0].faces[0], city::triangles(one.objects[0].geometry[0].faces[0])), 6, 1e-4);

    // Solids, multi-solids, lines and points; semantics nested per shell.
    const city::Model shapes = city::parse(R"({"type":"CityJSON","version":"2.0","transform":{"scale":[1,1,1],"translate":[0,0,0]},
        "CityObjects":{
          "s":{"type":"Building","geometry":[{"type":"MultiSolid","lod":"1","boundaries":[[[[[0,1,2]],[[0,2,3]]]],[[[[0,3,1]]]]],
               "semantics":{"surfaces":[{"type":"RoofSurface"}],"values":[[[0,null]],[[null]]]}}]},
          "l":{"type":"Road","geometry":[{"type":"MultiLineString","lod":"0","boundaries":[[0,1],[2,3]]},
                                         {"type":"MultiPoint","lod":"0","boundaries":[0]}]}},
        "vertices":[[0,0,0],[10,0,0],[10,10,0],[0,10,5]]})");
    const city::Geometry& ms = shapes.get("s").geometry[0];
    CHECK(ms.faces.size() == 3 && ms.faces[0].kind == "RoofSurface" && ms.faces[1].kind.empty() && ms.faces[2].kind.empty());
    CHECK(shapes.get("l").geometry[0].lines.size() == 2 && shapes.get("l").geometry[1].points.size() == 1);

    // CityJSONSeq: a header line, then features with their own vertices.
    const city::Model seq = city::parse(
        "{\"type\":\"CityJSON\",\"version\":\"2.0\",\"transform\":{\"scale\":[0.01,0.01,0.01],\"translate\":[1000,2000,0]},\"CityObjects\":{},\"vertices\":[]}\n"
        "{\"type\":\"CityJSONFeature\",\"id\":\"a\",\"CityObjects\":{\"a\":{\"type\":\"Building\",\"geometry\":[{\"type\":\"MultiSurface\",\"lod\":\"1\",\"boundaries\":[[[0,1,2]]]}]}},\"vertices\":[[0,0,0],[100,0,0],[0,100,0]]}\n"
        "\n"
        "{\"type\":\"CityJSONFeature\",\"id\":\"b\",\"CityObjects\":{\"b\":{\"type\":\"Building\",\"geometry\":[{\"type\":\"MultiSurface\",\"lod\":\"1\",\"boundaries\":[[[0,1,2]]]}]}},\"vertices\":[[1000,0,0],[1100,0,0],[1000,100,300]]}\n");
    CHECK(seq.objects.size() == 2);
    NEAR(seq.origin[0], 1000 + 5.5, 1e-9);
    const BoundingBox sb = seq.get("b").bounds();
    NEAR(sb.min.x + seq.origin[0], 1010, 1e-4);
    NEAR(sb.max.z, 3, 1e-5);

    // A pretty-printed single document (several lines) is not mistaken for a sequence.
    const city::Model pretty = city::parse("{\n  \"type\": \"CityJSON\",\n  \"version\": \"2.0\",\n  \"CityObjects\": {},\n  \"vertices\": []\n}\n");
    CHECK(pretty.objects.empty() && pretty.version == "2.0");

    CHECK(throws([] { city::parse(R"({"type":"FeatureCollection","features":[]})"); }));
    CHECK(throws([] { city::parse(""); }));
    CHECK(throws([] { city::parse(R"({"type":"CityJSON","CityObjects":{"a":{"type":"Building","geometry":[{"type":"MultiSurface","boundaries":[[[0,1,5]]]}]}},"vertices":[[0,0,0],[1,0,0]]})"); }));
    CHECK(throws([] { city::parse(R"({"type":"CityJSON","CityObjects":{"a":{"type":"Building","geometry":[{"type":"Blob","boundaries":[]}]}},"vertices":[]})"); }));
    CHECK(throws([] { city::parse(R"({"type":"CityJSON","CityObjects":{"a":{"geometry":[]}},"vertices":[]})"); }));
    CHECK(throws([] { city::parse(R"({"type":"CityJSON","CityObjects":{"a":{"type":"X","geometry":[{"type":"GeometryInstance","template":0,"boundaries":[0]}]}},"vertices":[[0,0,0]]})"); }));
    CHECK(throws([] { city::load("data/city/nope.city.json"); }));
}

static void triangulation() {
    // A concave L, a comb, a square with two holes, and a vertical wall with a window.
    city::Face l{ { { { 0, 0, 0 }, { 4, 0, 0 }, { 4, 1, 0 }, { 1, 1, 0 }, { 1, 4, 0 }, { 0, 4, 0 } } }, "", Json() };
    NEAR(triangleArea(l, city::triangles(l)), 7, 1e-5);
    city::Face comb;
    comb.rings.push_back({ { 0, 0, 0 }, { 9, 0, 0 }, { 9, 5, 0 }, { 8, 5, 0 }, { 8, 1, 0 }, { 6, 1, 0 }, { 6, 5, 0 }, { 5, 5, 0 },
                           { 5, 1, 0 }, { 3, 1, 0 }, { 3, 5, 0 }, { 2, 5, 0 }, { 2, 1, 0 }, { 1, 1, 0 }, { 1, 5, 0 }, { 0, 5, 0 } });
    NEAR(triangleArea(comb, city::triangles(comb)), 9 + 4 * 4 * 1, 1e-5);
    city::Face holes;
    holes.rings.push_back({ { 0, 0, 0 }, { 10, 0, 0 }, { 10, 10, 0 }, { 0, 10, 0 } });
    holes.rings.push_back({ { 2, 2, 0 }, { 2, 4, 0 }, { 4, 4, 0 }, { 4, 2, 0 } });
    holes.rings.push_back({ { 6, 6, 0 }, { 8, 6, 0 }, { 8, 8, 0 }, { 6, 8, 0 } });   // wound the wrong way: still a hole
    NEAR(triangleArea(holes, city::triangles(holes)), 100 - 4 - 4, 1e-4);
    city::Face wall;
    wall.rings.push_back({ { 0, 0, 0 }, { 6, 0, 0 }, { 6, 0, 3 }, { 0, 0, 3 } });
    wall.rings.push_back({ { 2, 0, 1 }, { 2, 0, 2 }, { 4, 0, 2 }, { 4, 0, 1 } });
    Vector3 n;
    const auto t = city::triangles(wall, &n);
    NEAR(triangleArea(wall, t), 18 - 2, 1e-5);
    NEAR(n.y, -1, 1e-5);                                            // faces south (anticlockwise from there)
    city::Face flat{ { { { 0, 0, 0 }, { 1, 0, 0 }, { 2, 0, 0 } } }, "", Json() };
    CHECK(city::triangles(flat, &n).empty() && Vector3Length(n) == 0);

    CHECK(city::toScene(Vector3{ 1, 2, 3 }).y == 3 && city::toScene(Vector3{ 1, 2, 3 }).z == -2);
}

static void external() {
    const char* dir = std::getenv("RMTN_CITYJSON_DIR");
    if (!dir) return;
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        const std::string ext = e.path().extension().string();
        if (ext != ".json" && ext != ".jsonl") continue;
        try {
            const city::Model m = city::load(e.path().string());
            CHECK(checkTriangles(m, e.path().filename().string().c_str()) == 0);
        } catch (const std::exception& x) {
            std::fprintf(stderr, "FAIL %s: %s\n", e.path().string().c_str(), x.what());
            failures++;
        }
    }
}

// ---------------------------------------------------------------- rendering

static Image renderScene(g3d::ScenePtr scene, int frame) {
    Comp c;
    c.w = 480; c.h = 270; c.fps = 30; c.duration = 120; c.background = { 20, 24, 40, 255 };
    addRoot(c.roots, 0, 120)->fn = g3d::draw(scene);
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, frame, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}

static int countNear(const Image& img, Color want, int tol) {
    int n = 0;
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++) {
            const Color c = GetImageColor(img, x, y);
            n += std::abs(c.r - want.r) <= tol && std::abs(c.g - want.g) <= tol && std::abs(c.b - want.b) <= tol;
        }
    return n;
}

// Paths: length, the point and heading along them, and a trail drawing itself on.
static void paths() {
    const std::vector<Vector3> l = { { 0, 0, 0 }, { 0, 30, 0 }, { 40, 30, 0 } };   // north 30, then east 40
    NEAR(city::length(l), 70, 1e-4);
    float heading = -1;
    Vector3 p = city::along(l, 0, &heading);
    NEAR(p.y, 0, 1e-5);
    NEAR(heading, 0, 1e-3);
    p = city::along(l, 0.5f, &heading);                             // 35 m: 5 m past the corner
    NEAR(p.x, 5, 1e-4);
    NEAR(p.y, 30, 1e-4);
    NEAR(heading, 90, 1e-3);
    p = city::along(l, 2, &heading);
    NEAR(p.x, 40, 1e-4);
    NEAR(city::along({ { 1, 2, 3 } }, 0.5f).z, 3, 1e-6);
    NEAR(city::length({}), 0, 0);

    g3d::Scene scene;
    g3d::Object3D* trail = city::trail(scene, l, [](int t) { return t / 100.f; });
    CHECK(trail->name == "trail" && trail->children.size() == 3);   // two segments and a corner
    auto shown = [&](int t) {
        int n = 0;
        for (auto& ch : trail->children) {
            ch->up(t, *ch);
            n += ch->visible;
        }
        return n;
    };
    CHECK(shown(0) == 0 && shown(20) == 1 && shown(50) == 3 && shown(100) == 3);
    shown(20);                                                      // 14 m: part of the first segment
    const g3d::Object3D& first = *trail->children[0];
    NEAR(first.scale.z, 14, 1e-3);
    NEAR(first.position.z, -7, 1e-3);                               // halfway, 7 m north (scene -z)
    NEAR(first.position.y, 0.2 + 0.15, 1e-4);
    shown(50);
    NEAR(trail->children[2]->scale.z, 5, 1e-3);
    NEAR(trail->children[2]->position.x, 2.5, 1e-3);

    // Projection (no GL): a point in front of the camera lands where the view says.
    g3d::Camera cam;
    cam.position = { 0, 0, 10 };
    cam.target = { 0, 0, 0 };
    Vector2 at;
    CHECK(g3d::project(cam, { 0, 0, 0 }, { 0, 0, 1920, 1080 }, at));
    NEAR(at.x, 960, 0.5);
    NEAR(at.y, 540, 0.5);
    CHECK(g3d::project(cam, { 1, 1, 0 }, { 0, 0, 1920, 1080 }, at) && at.x > 960 && at.y < 540);
    CHECK(!g3d::project(cam, { 0, 0, 20 }, { 0, 0, 1920, 1080 }, at));   // behind
    cam.up = [](int t, g3d::Camera& self) { self.position.x = (float)t; };
    NEAR(g3d::cameraAt(cam, 3).position.x, 3, 0);
    NEAR(cam.position.x, 0, 0);
}

static void rendering() {
    const city::Model m = city::load("data/city/town.city.json");
    auto scene = std::make_shared<g3d::Scene>();
    scene->ambient = 1;
    city::Look look;
    look.roof = { 255, 0, 0, 255 };
    look.wall = { 0, 0, 255, 255 };
    g3d::Material unlit;
    unlit.unlit = true;                                             // exact colours to count
    g3d::Object3D* group = city::add(*scene, m, look, unlit);
    CHECK(group->children.size() == 153 + 2 + 19 + 1 + 9 + 8 + 1 + 24);  // everything with faces (not "tower", the route or stops)
    CHECK(scene->find("house-3") != nullptr && scene->find("tower") == nullptr && scene->find("tower-shaft") != nullptr);
    const Vector3 at = scene->find("tower-shaft")->position;
    NEAR(at.y, 12.6, 1e-3);                                         // at its own base (heights are above the canal)
    city::frame(scene->camera, m, 200, 40);

    Image a = renderScene(scene, 0);
    const int red = countNear(a, { 255, 0, 0, 255 }, 30), blue = countNear(a, { 0, 0, 255, 255 }, 30);
    CHECK(red > 2000 && blue > 1000);
    // Framed: the town fills the view without spilling off it (background on every edge).
    const Color corner = GetImageColor(a, 2, 2);
    CHECK(corner.r < 40 && corner.b > 30);
    UnloadImage(a);

    // Rising: before the first building starts nothing stands; part way, some do.
    city::rise(group, 10, 0.3f, 20);
    Image before = renderScene(scene, 5), during = renderScene(scene, 40), after = renderScene(scene, 119);
    const int b0 = countNear(before, { 0, 0, 255, 255 }, 30), b1 = countNear(during, { 0, 0, 255, 255 }, 30),
              b2 = countNear(after, { 0, 0, 255, 255 }, 30);
    CHECK(b0 == 0 && b1 > 0 && b2 > b1);
    UnloadImage(before);
    UnloadImage(during);
    UnloadImage(after);
}

static void smartgridPrefabs() {
    const char* ids[] = {
        "residential-detached", "residential-terrace", "residential-apartment",
        "commercial-office", "commercial-office-tower", "city-hall",
        "exhibition-event-hall", "commercial-shop", "industrial-factory-pv",
        "industrial-warehouse", "industrial-assembly-factory", "data-center",
        "power-station-turbine-hall", "power-station-control-building",
        "wind-turbine", "diesel-generator", "gas-turbine-generator", "hv-transformer",
        "ev-charging-pole", "ev-charging-hub-five-bay",
        "e-car", "distribution-substation", "lv-distribution-cabinet",
        "battery-storage", "battery-substation", "solar-carport", "grid-pole", "road-straight",
        "foliage-deciduous-tree", "foliage-conifer-tree", "foliage-street-tree",
        "foliage-orchard-tree", "foliage-shrub-cluster", "foliage-hedge-segment",
        "foliage-grass-patch", "foliage-flower-bed", "foliage-planter-tree",
        "street-traffic-light", "street-light", "solar-street-light",
        "traffic-sign-stop", "traffic-sign-warning", "traffic-sign-direction",
        "street-pedestrian-crossing", "bus-stop-shelter", "park-bench",
        "public-waste-bin", "street-bollard", "bicycle-rack",
    };
    for (const char* id : ids) {
        const city::Model m = city::load(std::string("data/city/smartgrid/") + id + ".city.json");
        CHECK(m.version == "2.0" && !m.objects.empty());
        CHECK(m.metadata["illustrative"].asBool());
        CHECK(checkTriangles(m,id) == 0);
        const BoundingBox b = m.bounds();
        CHECK(std::fabs((b.min.x+b.max.x)/2) < 4 && std::fabs((b.min.y+b.max.y)/2) < 4);
        for (const city::Object& o : m.objects) {
            CHECK(o.attributes["smartGridRole"].isString());
            CHECK(o.attributes["paletteRole"].isString());
            for (const std::string& parent : o.parents) {
                const city::Object& p = m.get(parent);
                CHECK(std::find(p.children.begin(),p.children.end(),o.id) != p.children.end());
            }
        }
    }
    const city::Model hub = city::load("data/city/smartgrid/ev-charging-hub-five-bay.city.json");
    for (int n=1; n<=5; ++n) CHECK(hub.find("charger-"+std::to_string(n)) != nullptr);
    const city::Model plant = city::load("data/city/smartgrid/industrial-factory-pv.city.json");
    CHECK(plant.get("factory-pv").parents == std::vector<std::string>{"factory"});
    const city::Model station = city::load("data/city/smartgrid/distribution-substation.city.json");
    CHECK(station.get("transformer").attributes["scenarioRatingKVA"].asNumber() == 630);
    const city::Model batteryYard = city::load("data/city/smartgrid/battery-substation.city.json");
    for (int n=1; n<=3; ++n) CHECK(batteryYard.find("battery-module-"+std::to_string(n)) != nullptr);
    CHECK(batteryYard.find("battery-switchgear") != nullptr);
    CHECK(batteryYard.find("battery-transformer") != nullptr);
    const city::Model civic = city::load("data/city/smartgrid/city-hall.city.json");
    CHECK(civic.get("clock-tower").parents == std::vector<std::string>{"city-hall"});
    const city::Model hall = city::load("data/city/smartgrid/exhibition-event-hall.city.json");
    CHECK(hall.find("event-canopy") != nullptr && hall.find("event-doors") != nullptr);
    const city::Model wind = city::load("data/city/smartgrid/wind-turbine.city.json");
    for (int n=1; n<=3; ++n) CHECK(wind.find("rotor-blade-"+std::to_string(n)) != nullptr);
    const city::Model hv = city::load("data/city/smartgrid/hv-transformer.city.json");
    for (int n=1; n<=3; ++n) CHECK(hv.find("hv-bushing-"+std::to_string(n)) != nullptr);
    CHECK(hv.find("conservator") != nullptr && hv.find("radiator-1-1") != nullptr);
    const city::Model data = city::load("data/city/smartgrid/data-center.city.json");
    CHECK(data.find("roof-cooler-1-1") != nullptr && data.find("cooling-louvres") != nullptr);
    const city::Model gas = city::load("data/city/smartgrid/gas-turbine-generator.city.json");
    CHECK(gas.find("air-filter-house") != nullptr && gas.find("exhaust-stack") != nullptr);
    const city::Model tree = city::load("data/city/smartgrid/foliage-deciduous-tree.city.json");
    CHECK(tree.find("tree-trunk") != nullptr && tree.find("main-crown") != nullptr);
    const city::Model conifer = city::load("data/city/smartgrid/foliage-conifer-tree.city.json");
    for (int n=1; n<=3; ++n) CHECK(conifer.find("branch-tier-"+std::to_string(n)) != nullptr);
    const city::Model grass = city::load("data/city/smartgrid/foliage-grass-patch.city.json");
    CHECK(grass.find("grass-ground") != nullptr && grass.find("grass-tufts") != nullptr);
    const city::Model signal = city::load("data/city/smartgrid/street-traffic-light.city.json");
    CHECK(signal.find("signal-red") != nullptr && signal.find("signal-amber") != nullptr &&
          signal.find("signal-green") != nullptr);
    const city::Model solarLamp = city::load("data/city/smartgrid/solar-street-light.city.json");
    CHECK(solarLamp.find("street-pv-panel") != nullptr && solarLamp.find("solar-battery") != nullptr);
    const city::Model crossing = city::load("data/city/smartgrid/street-pedestrian-crossing.city.json");
    CHECK(crossing.find("zebra-stripes") != nullptr);
    const city::Model shelter = city::load("data/city/smartgrid/bus-stop-shelter.city.json");
    CHECK(shelter.find("shelter-roof") != nullptr && shelter.find("shelter-back") != nullptr &&
          shelter.find("shelter-seat") != nullptr);
}

static void chemistryPrefabs() {
    const char* ids[] = {
        "lab-bench", "fume-hood", "chemical-storage-cabinet", "analytical-balance",
        "hotplate-stirrer", "rotary-evaporator", "hplc-stack", "bench-centrifuge",
        "drying-oven", "jacketed-reactor", "glassware-rack", "vacuum-pump",
    };
    for (const char* id : ids) {
        const city::Model m = city::load(std::string("data/city/chemistry-lab/") + id + ".city.json");
        CHECK(m.version == "2.0" && !m.objects.empty());
        CHECK(m.metadata["illustrative"].asBool());
        CHECK(checkTriangles(m,id) == 0);
        const BoundingBox b = m.bounds();
        CHECK(std::fabs((b.min.x+b.max.x)/2) < .4 && std::fabs((b.min.y+b.max.y)/2) < .4);
        CHECK(b.min.z >= 0 && b.max.z > b.min.z);
        for (const city::Object& o : m.objects) {
            CHECK(o.attributes["labRole"].isString());
            CHECK(o.attributes["paletteRole"].isString());
        }
    }
    const city::Model hood = city::load("data/city/chemistry-lab/fume-hood.city.json");
    CHECK(hood.find("sliding-sash") != nullptr && hood.find("exhaust-collar") != nullptr);
    const city::Model rotavap = city::load("data/city/chemistry-lab/rotary-evaporator.city.json");
    CHECK(rotavap.find("evaporation-flask") != nullptr && rotavap.find("condenser") != nullptr);
    const city::Model hplc = city::load("data/city/chemistry-lab/hplc-stack.city.json");
    for (int n=0; n<4; ++n) CHECK(hplc.find("screen-"+std::to_string(n)) != nullptr);
    const city::Model reactor = city::load("data/city/chemistry-lab/jacketed-reactor.city.json");
    CHECK(reactor.find("reactor-vessel") != nullptr && reactor.find("agitator-motor") != nullptr);
    city::Look look;
    look.material = [](const city::Object& object, g3d::Material& material) {
        if (object.attributes["paletteRole"].asString() == "glass") {
            material.roughness = .12f;
            material.metalness = 0;
            material.opacity = .4f;
        } else if (object.attributes["paletteRole"].asString() == "metal") {
            material.roughness = .28f;
            material.metalness = .9f;
        }
    };
    auto scene = std::make_shared<g3d::Scene>();
    city::add(*scene, hood, look);
    CHECK(scene->find("sliding-sash") != nullptr);
    NEAR(scene->find("sliding-sash")->material.opacity, .4f, 1e-5);
    NEAR(scene->find("hood-top")->material.metalness, .9f, 1e-5);
}

int main() {
    town();
    smartgridPrefabs();
    chemistryPrefabs();
    formats();
    triangulation();
    external();
    paths();
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 180, "city");
    if (!IsWindowReady()) {
        std::puts("no window: city rendering skipped");
        return failures ? 1 : 77;
    }
    rendering();
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d city check(s) failed\n", failures); return 1; }
    std::puts("city tests passed");
    return 0;
}
