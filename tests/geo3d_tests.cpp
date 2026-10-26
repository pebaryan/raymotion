// geo3d_tests.cpp -- geo3d::: the surface (globe, map and the morph between, centred and
// turned), data by country code, scales, marks standing on the surface, flows; then
// rendering (GL): a base map with an extruded country and a column, on the globe and
// unrolled onto the map, and labels hidden round the back of the globe.
#include "rmtn/raymotion.hpp"
#include "rmtn/geo3d.hpp"
#include <raymath.h>
#include <cmath>
#include <cstdio>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
#define NEAR(a, b, eps) CHECK(std::fabs((double)(a) - (double)(b)) <= (eps))

static bool nearV(Vector3 a, Vector3 b, float eps) { return Vector3Distance(a, b) <= eps; }

static Vector3 turn(Vector3 rotation, Vector3 v) {   // as g3d applies Object3D::rotation
    return Vector3Transform(v, MatrixRotateXYZ(Vector3Scale(rotation, DEG2RAD)));
}

static void surface() {
    for (geo::LonLat c : { geo::LonLat{ 0, 0 }, geo::LonLat{ 110, -5 }, geo::LonLat{ -70, 40 } }) {
        geo3d::Surface s(geo::Projection::equirectangular(), c, 2);
        // The centre faces +z on the globe and touches the map there.
        CHECK(nearV(s.onGlobe(c), { 0, 0, 2 }, 1e-4f));
        CHECK(nearV(s.onMap(c), { 0, 0, 2 }, 1e-3f));
        CHECK(nearV(s.up(c), { 0, 0, 1 }, 1e-4f));
        // East is +x and north +y, on both.
        const geo::LonLat e{ c.lon + 10, c.lat }, n{ c.lon, c.lat + 10 };
        CHECK(s.onGlobe(e).x > 0.2f && s.onMap(e).x > 0.2f);
        CHECK(s.onGlobe(n).y > 0.2f && s.onMap(n).y > 0.2f);
        NEAR(Vector3Length(s.onGlobe({ 30, 60 }, 0.5f)), 2.5, 1e-4);
        CHECK(nearV(s.north(c), { 0, 1, 0 }, 1e-3f));
        s.morph = 1;
        if (c.lat == 0) CHECK(nearV(s.north({ c.lon + 30, 20 }), { 0, 1, 0 }, 1e-3f));   // plain equirectangular: north is straight up
        CHECK(nearV(s.up({ c.lon + 60, 0 }), { 0, 0, 1 }, 1e-5f));
        CHECK(nearV(s.point({ c.lon + 60, 0 }, 0.3f), s.onMap({ c.lon + 60, 0 }, 0.3f), 1e-6f));
        // Halfway: halfway between.
        s.morph = 0.5f;
        const geo::LonLat p{ c.lon + 40, c.lat - 20 };
        CHECK(nearV(s.point(p), Vector3Lerp(s.onGlobe(p), s.onMap(p), 0.5f), 1e-5f));
        NEAR(Vector3Length(s.up(p)), 1, 1e-5);
    }
    // The map is `width` across; by default the equator keeps its length.
    geo3d::Surface s;
    NEAR(s.onMap({ 179.99, 0 }).x - s.onMap({ -179.99, 0 }).x, 2 * PI, 1e-2);
    geo3d::Surface w(geo::Projection::equalEarth(), {}, 1, 4);
    NEAR(w.onMap({ 179.99, 0 }).x - w.onMap({ -179.99, 0 }).x, 4, 1e-2);
    CHECK(w.onMap({ 0, 60 }).y > 0 && w.onMap({ 0, -60 }).y < 0);
    const int v = w.version();
    w.setMap(geo::Projection::robinson());
    CHECK(w.version() != v);
    NEAR(w.onMap({ 179.99, 0 }).x - w.onMap({ -179.99, 0 }).x, 4, 1e-2);
}

static void data() {
    const auto d = geo3d::byCountry({ { "IDN", 281 }, { "nowhere", 5 }, { "BR", 216 } });
    CHECK(d.size() == 2);
    CHECK(d[0].key == "IDN" && d[0].value == 281 && d[1].key == "BR");
    const geo::LonLat id = geo::countries().get("IDN").labelPoint();
    CHECK(d[0].at.lon == id.lon && d[0].at.lat == id.lat);

    const geo3d::Scale s = geo3d::Scale{}.over({ 3, 9, 6 });
    CHECK(s.min == 0 && s.max == 9);
    NEAR(s.t(4.5), 0.5, 1e-6);
    const geo3d::Scale neg = geo3d::Scale{}.over({ -4, 2 });
    CHECK(neg.min == -4 && neg.max == 2);
    geo3d::Scale fixed;
    fixed.min = 10;
    fixed.max = 20;
    fixed.colors = [](float t) { return Color{ (unsigned char)(t * 255), 0, 0, 255 }; };
    CHECK(fixed.over({ 1, 100 }).max == 20);
    CHECK(fixed(20).r == 255 && fixed(0).r == 0 && fixed(15).r > 120 && fixed(15).r < 135);
}

static void marks() {
    auto s = std::make_shared<geo3d::Surface>(geo::Projection::equirectangular(), geo::LonLat{ 100, 0 });
    g3d::Object3D root;
    geo3d::MarkStyle st;
    st.height = 1;
    st.size = 0.1f;
    st.stagger = 10;
    st.grow = [](int t) { return t / 20.f; };
    std::vector<geo3d::Datum> d = { { { 100, 0 }, 2, "a" }, { { 120, 30 }, 4, "b" } };
    g3d::Object3D* group = geo3d::marks(root, s, d, st);
    CHECK(group->children.size() == 2);
    g3d::Object3D& a = *group->children[0];
    g3d::Object3D& b = *group->children[1];
    // Largest first: b starts at 0, a 10 frames later.
    a.up(5, a);
    b.up(5, b);
    CHECK(!a.visible && b.visible);
    a.up(40, a);
    b.up(40, b);
    NEAR(b.scale.y, 1, 1e-4);
    NEAR(a.scale.y, 0.5, 1e-4);
    CHECK(nearV(b.position, s->point({ 120, 30 }), 1e-5f));
    CHECK(nearV(turn(b.rotation, { 0, 1, 0 }), s->up({ 120, 30 }), 1e-3f));   // standing up
    CHECK(b.name == "b" && b.children.size() == 1);
    // Flat: the column follows the map.
    s->morph = 1;
    b.up(40, b);
    CHECK(nearV(b.position, s->onMap({ 120, 30 }), 1e-5f));
    CHECK(nearV(turn(b.rotation, { 0, 1, 0 }), { 0, 0, 1 }, 1e-3f));

    // Flows: one path each, lifted off the surface in the middle.
    g3d::Object3D* f = geo3d::flows(root, s, { { { 100, 0 }, { 140, 10 }, 3 }, { { 100, 0 }, { 60, 20 }, 1 } });
    CHECK(f->children.size() == 2);
}

// ---- rendering

static Image renderScene(g3d::ScenePtr scene, int frame) {
    Comp c;
    c.w = 320; c.h = 320; c.fps = 30; c.duration = 120; c.background = { 0, 0, 0, 255 };
    addRoot(c.roots, 0, 120)->fn = g3d::draw(scene);
    Canvas canvas;
    canvas.load(c.w, c.h, 1);
    presentFrame(c, frame, canvas);
    Image img = LoadImageFromTexture(canvas.output().texture);
    ImageFlipVertical(&img);
    canvas.unload();
    return img;
}

static Vector2 px(const g3d::Scene& scene, Vector3 p) {
    Vector2 out{ -1, -1 };
    g3d::project(scene.camera, p, { 0, 0, 320, 320 }, out);
    return out;
}

static void rendering() {
    const Color SEA{ 0, 0, 255, 255 }, LAND{ 255, 0, 0, 255 };
    auto s = std::make_shared<geo3d::Surface>(geo::Projection::equirectangular(), geo::LonLat{ -55, -10 });
    auto scene = std::make_shared<g3d::Scene>();
    g3d::Object3D* world = scene->add();
    globe::Map m(1024, SEA);
    m.fill(geo::land().merged(), LAND);
    g3d::Material paper;
    paper.unlit = true;
    paper.dynamicMap = m.texture();
    geo3d::base(*world, s, paper, 96);
    geo3d::ExtrudeStyle ex;
    ex.height = 0.25f;
    ex.unlit = true;
    ex.scale.colors = [](float) { return Color{ 0, 255, 0, 255 }; };
    geo3d::extrude(*world, s, { { "BRA", 1 } }, ex);
    scene->camera.position = { 0, 0, 4 };
    scene->camera.target = { 0, 0, 0 };

    // The globe: Brazil faces us, risen and green; the Atlantic east of it blue.
    const geo::LonLat brazil = geo::countries().get("BRA").labelPoint();
    Image a = renderScene(scene, 0);
    Vector2 q = px(*scene, s->point(brazil, 0.25f));
    Color c = GetImageColor(a, (int)q.x, (int)q.y);
    CHECK(c.g > 200 && c.r < 60 && c.b < 60);
    q = px(*scene, s->point({ -25, -20 }));
    c = GetImageColor(a, (int)q.x, (int)q.y);
    CHECK(c.b > 200 && c.g < 60);
    UnloadImage(a);

    // Unrolled: the same from in front of the map.
    s->morph = 1;
    scene->camera.position = { 0, 0, 1 + 6 };
    scene->camera.target = { 0, 0, 1 };
    Image b = renderScene(scene, 1);
    q = px(*scene, s->onMap(brazil, 0.25f));
    c = GetImageColor(b, (int)q.x, (int)q.y);
    CHECK(c.g > 200 && c.r < 60);
    q = px(*scene, s->onMap({ -140, -20 }));                    // the Pacific
    c = GetImageColor(b, (int)q.x, (int)q.y);
    CHECK(q.x > 2 && c.b > 200 && c.g < 60);
    q = px(*scene, s->onMap({ 20, 5 }));                        // Africa
    c = GetImageColor(b, (int)q.x, (int)q.y);
    CHECK(c.r > 200 && c.g < 60);
    UnloadImage(b);

    // Countries the map's edge cuts (Russia and Alaska across the antimeridian, centred at
    // 10 E) stay at their own side: nothing stretched across the middle of the map, top or wall.
    auto seam = std::make_shared<geo3d::Surface>(geo::Projection::equirectangular(), geo::LonLat{ 10, 0 });
    seam->morph = 1;
    auto s2 = std::make_shared<g3d::Scene>();
    g3d::Object3D* w2 = s2->add();
    geo3d::base(*w2, seam, paper, 96);
    geo3d::extrude(*w2, seam, { { "USA", 1 }, { "RUS", 1 } }, ex);
    s2->camera.position = { 0, -3, 1 + 5.5f };                // from the south, so walls show
    s2->camera.target = { 0, 0, 1 };
    Image e = renderScene(s2, 0);
    for (geo::LonLat sea : { geo::LonLat{ -30, 45 }, geo::LonLat{ -30, 62 }, geo::LonLat{ -10, 70 }, geo::LonLat{ 65, 10 }, geo::LonLat{ -30, 53 }, geo::LonLat{ -40, 51.5 } }) {
        q = px(*s2, seam->onMap(sea));
        c = GetImageColor(e, (int)q.x, (int)q.y);
        CHECK(c.b > 200 && c.g < 60);
    }
    for (const char* iso : { "USA", "RUS" }) {
        q = px(*s2, seam->onMap(geo::countries().get(iso).labelPoint(), 0.25f));
        c = GetImageColor(e, (int)q.x, (int)q.y);
        CHECK(c.g > 200 && c.r < 60);
    }
    UnloadImage(e);

    // Labels: the front of the globe shows, the back does not.
    s->morph = 0;
    scene->camera.position = { 0, 0, 4 };
    scene->camera.target = { 0, 0, 0 };
    Vector2 out;
    CHECK(geo3d::screen(scene->camera, *s, { -55, -10 }, 0, { 0, 0, 320, 320 }, out));
    NEAR(out.x, 160, 1);
    CHECK(!geo3d::screen(scene->camera, *s, { 125, 10 }, 0, { 0, 0, 320, 320 }, out));
}

int main() {
    surface();
    data();
    marks();
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 180, "geo3d");
    if (!IsWindowReady()) {
        std::puts("no window: geo3d rendering skipped");
        return failures ? 1 : 77;
    }
    rendering();
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d geo3d check(s) failed\n", failures); return 1; }
    std::puts("geo3d tests passed");
    return 0;
}
