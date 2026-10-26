// globe_tests.cpp -- globe::: places and tangents, g3d::lookRotation, CPU maps (land fill
// against the real share of land, the antimeridian and the poles, strokes, discs), fields,
// the heat palette, clouds (cover, seams, motion, storms), arcs and routes; then rendering
// (GL): a painted globe seen from two sides, a texture remade per frame, and an overlay shell.
#include "rmtn/raymotion.hpp"
#include <raymath.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
#define NEAR(a, b, eps) CHECK(std::fabs((double)(a) - (double)(b)) <= (eps))

static constexpr double kPi = 3.14159265358979323846;

static bool nearV(Vector3 a, Vector3 b, float eps) { return Vector3Distance(a, b) <= eps; }

static Vector3 turn(Vector3 rotation, Vector3 v) {   // as g3d applies Object3D::rotation
    return Vector3Transform(v, MatrixRotateXYZ(Vector3Scale(rotation, DEG2RAD)));
}

static void places() {
    CHECK(nearV(globe::point({ 0, 0 }), { 0, 0, 1 }, 1e-6f));
    CHECK(nearV(globe::point({ 90, 0 }), { 1, 0, 0 }, 1e-6f));
    CHECK(nearV(globe::point({ 0, 90 }, 2), { 0, 2, 0 }, 1e-6f));
    for (globe::LonLat p : { globe::LonLat{ -73.9, 40.7 }, globe::LonLat{ 151.2, -33.9 }, globe::LonLat{ 179.5, 10 } }) {
        const globe::LonLat q = globe::lonLat(globe::point(p, 3));
        NEAR(q.lon, p.lon, 1e-4);
        NEAR(q.lat, p.lat, 1e-4);
        const Vector3 up = globe::point(p), n = globe::north(p), e = globe::east(p);
        NEAR(Vector3DotProduct(up, n), 0, 1e-5);
        NEAR(Vector3DotProduct(up, e), 0, 1e-5);
        NEAR(Vector3Length(n), 1, 1e-5);
        CHECK(nearV(Vector3CrossProduct(e, n), up, 1e-5f));               // east x north = up
        // north really points north: a small step that way raises the latitude
        CHECK(globe::lonLat(Vector3Add(up, Vector3Scale(n, 0.01f))).lat > p.lat);
    }

    // lookRotation turns +z to forward and +y towards up, including straight along x.
    const Vector3 cases[][2] = { { { 0, 0, 1 }, { 0, 1, 0 } }, { { 1, 2, -3 }, { 0, 1, 0 } }, { { -1, 0, 0 }, { 0, 0, 1 } },
                                 { { 1, 0, 0 }, { 0, 1, 0 } }, { { 0.3f, -0.9f, 0.1f }, { 1, 1, 1 } }, { { 0, 1, 0 }, { 0, 1, 0 } } };
    for (const auto& c : cases) {
        const Vector3 r = g3d::lookRotation(c[0], c[1]);
        CHECK(nearV(turn(r, { 0, 0, 1 }), Vector3Normalize(c[0]), 1e-4f));
        const Vector3 y = turn(r, { 0, 1, 0 });
        NEAR(Vector3DotProduct(y, Vector3Normalize(c[0])), 0, 1e-4);
        if (Vector3Length(Vector3CrossProduct(c[0], c[1])) > 1e-3f) CHECK(Vector3DotProduct(y, c[1]) > 0);
    }

    // place: standing on the surface, facing the heading.
    g3d::Object3D o;
    globe::place(o, { 30, 45 }, 0.1f, 90);
    CHECK(nearV(o.position, globe::point({ 30, 45 }, 1.1f), 1e-5f));
    CHECK(nearV(turn(o.rotation, { 0, 1, 0 }), globe::point({ 30, 45 }), 1e-4f));
    CHECK(nearV(turn(o.rotation, { 0, 0, 1 }), globe::east({ 30, 45 }), 1e-4f));
}

// Share of the sphere's area a map covers with `want` (pixels weighted by cos(latitude)).
static double share(const globe::Map& m, Color want) {
    double hit = 0, all = 0;
    for (int y = 0; y < m.height(); y++) {
        const double w = std::cos((90 - (y + 0.5) * 180.0 / m.height()) * kPi / 180);
        for (int x = 0; x < m.width(); x++) {
            const Color c = const_cast<globe::Map&>(m).pixel(x, y);
            hit += w * (std::abs(c.r - want.r) < 60 && std::abs(c.g - want.g) < 60 && std::abs(c.b - want.b) < 60);
            all += w;
        }
    }
    return hit / all;
}

static void maps() {
    const Color SEA{ 0, 0, 255, 255 }, LAND{ 255, 0, 0, 255 }, LINE{ 0, 255, 0, 255 };
    globe::Map m(1440, SEA);
    CHECK(m.width() == 1440 && m.height() == 720);
    m.fill(geo::land().merged(), LAND);
    NEAR(share(m, LAND), 0.29, 0.03);                          // about 29% of the Earth is land
    CHECK(m.at({ 20, 0 }).r == 255);                           // the Congo
    CHECK(m.at({ -30, 0 }).b == 255);                          // the Atlantic
    CHECK(m.at({ 0, -85 }).r == 255);                          // Antarctica, closed over the pole
    CHECK(m.at({ 0, 88 }).b == 255);                           // the Arctic Ocean
    CHECK(m.at({ -40, 75 }).r == 255);                         // Greenland
    CHECK(m.at({ 170, 65 }).r == 255 && m.at({ -175, 66 }).r == 255);   // Chukotka, both sides of 180
    CHECK(m.at({ -150, 20 }).b == 255);                        // the Pacific
    // a hand-made polygon across the antimeridian
    globe::Map a(720, BLANK);
    geo::Geometry box;
    box.polygons.push_back({ { { 170, -10 }, { -170, -10 }, { -170, 10 }, { 170, 10 } } });
    a.fill(box, LAND);
    CHECK(a.at({ 175, 0 }).a == 255 && a.at({ -175, 0 }).a == 255 && a.at({ 0, 0 }).a == 0 && a.at({ 160, 0 }).a == 0);

    // Strokes: a graticule line, crisp at the equator, as wide on the ground near the pole.
    globe::Map g(1440, BLANK);
    g.graticule(30, LINE, 3);
    CHECK(g.at({ 10, 0 }).a > 200 && g.at({ 10, 2 }).a == 0 && g.at({ 30, 15 }).a > 200);
    CHECK(g.at({ 31, 70 }).a > 100);                           // 3 px at the equator is +-4.4 px (1.1 degrees) at 70

    // Discs measured in km.
    globe::Map d(1440, BLANK);
    d.disc({ 0, 60 }, 500, LINE, 0);
    CHECK(d.at({ 0, 60 }).a == 255 && d.at({ 7.2, 60 }).a == 255 && d.at({ 12.5, 60 }).a == 0);
    CHECK(d.at({ 0, 64 }).a == 255 && d.at({ 0, 65 }).a == 0);   // 500 km is 4.5 degrees of latitude

    // A regional map: its own box.
    globe::Map r(400, geo::Bounds{ 140, -45, 160, -25 });
    CHECK(r.height() == 400);
    r.fill(geo::land().merged(), LAND);
    CHECK(r.at({ 145, -30 }).r == 255 && r.at({ 158, -30 }).a == 0 && r.at({ 0, 0 }).a == 0);
    const Vector2 px = r.pixelOf({ 150, -35 });
    NEAR(px.x, 200, 1e-3);
    NEAR(px.y, 200, 1e-3);

    // over() and image()
    globe::Map base(64, SEA), top(64, BLANK);
    top.disc({ 0, 0 }, 3000, LAND, 0);
    base.over(top);
    CHECK(base.at({ 0, 0 }).r == 255 && base.at({ 180, 0 }).b == 255);
    Image img = base.image();
    CHECK(img.width == 64 && img.height == 32 && img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    UnloadImage(img);
}

static void fields() {
    globe::Field f(1440);
    f.add({ 10, 20 }, 1000, 1);
    NEAR(f.at({ 10, 20 }), 1, 0.01);
    NEAR(f.at({ 10, 20 + 1000 / 111.19 }), std::exp(-2.0), 0.02);   // 1/e^2 at the radius
    CHECK(f.at({ 10, 40 }) == 0);
    f.add({ 179.9, 0 }, 500, 2);
    CHECK(f.at({ -179.9, 0 }) > 1.8f);                               // across the antimeridian
    NEAR(f.max(), 2, 0.05);
    f.clear();
    CHECK(f.max() == 0);
    f.add({ 0, 89.9 }, 800, 1);
    CHECK(f.at({ 120, 88 }) > 0.5f);                                 // round the pole

    CHECK(globe::heat(0).a == 0 && globe::heat(1).a > 200 && globe::heat(1).r > 240);
    CHECK(globe::heat(0.3f).a < globe::heat(0.8f).a);
    globe::Field small(32);
    small.add({ 0, 0 }, 2000, 1);
    Image img = small.image(1);
    CHECK(img.width == 32 && ((Color*)img.data)[8 * 32 + 16].a > 200 && ((Color*)img.data)[0].a == 0);
    UnloadImage(img);
}

static void weather() {
    globe::Weather w;
    w.cover = 0.5f;
    int cloudy = 0, n = 0;
    for (double lat = -80; lat <= 80; lat += 4)
        for (double lon = -180; lon < 180; lon += 4) {
            cloudy += globe::cloud({ lon, lat }, 0, w) > 0.5f;
            n++;
        }
    CHECK(cloudy > n * 0.25 && cloudy < n * 0.75);
    w.cover = 0.15f;
    int fewer = 0;
    for (double lat = -80; lat <= 80; lat += 4)
        for (double lon = -180; lon < 180; lon += 4) fewer += globe::cloud({ lon, lat }, 0, w) > 0.5f;
    CHECK(fewer < cloudy / 2);
    w.cover = 0.5f;
    // seamless: across the antimeridian and round the pole
    float worst = 0;
    for (double lat = -60; lat <= 60; lat += 10)
        worst = std::max(worst, std::fabs(globe::cloud({ 179.999, lat }, 3, w) - globe::cloud({ -179.999, lat }, 3, w)));
    CHECK(worst < 0.02f);
    const float p0 = globe::cloud({ 0, 89.999 }, 3, w);
    for (double lon = -180; lon < 180; lon += 45) NEAR(globe::cloud({ lon, 89.999 }, 3, w), p0, 0.03);
    // moving
    int changed = 0;
    for (double lon = -180; lon < 180; lon += 5) changed += std::fabs(globe::cloud({ lon, 45 }, 0, w) - globe::cloud({ lon, 45 }, 4, w)) > 0.2f;
    CHECK(changed > 5);
    // a storm: a clear eye in a ring of cloud
    w.storms.push_back({ { 130, 15 }, 900, 1, false });
    CHECK(globe::cloud({ 130, 15 }, 2, w) < 0.1f);
    int ring = 0;
    for (int k = 0; k < 12; k++) {
        const double a = k * kPi / 6;
        ring += globe::cloud({ 130 + 2.2 * std::cos(a), 15 + 2.2 * std::sin(a) }, 2, w) > 0.5f;
    }
    CHECK(ring >= 7);
    Image img = globe::clouds(128, 1, w);
    CHECK(img.width == 128 && img.height == 64);
    UnloadImage(img);
}

static void routes() {
    const globe::LonLat lon{ -0.13, 51.5 }, tok{ 139.7, 35.7 };
    CHECK(nearV(globe::arc(lon, tok, 0), globe::point(lon), 1e-5f));
    CHECK(nearV(globe::arc(lon, tok, 1, -1, 2), globe::point(tok, 2), 1e-5f));
    const float mid = Vector3Length(globe::arc(lon, tok, 0.5f));
    CHECK(mid > 1.08f && mid < 1.2f);                          // lifted by distance
    NEAR(Vector3Length(globe::arc(lon, tok, 0.5f, 0.3f)), 1.3, 1e-4);
    // the midpoint of the great circle (over Siberia)
    const globe::LonLat m = globe::lonLat(globe::arc(lon, tok, 0.5f));
    CHECK(m.lat > 60 && m.lon > 40 && m.lon < 100);

    g3d::Scene scene;
    g3d::Object3D* r = globe::route(scene, lon, tok, [](int t) { return t / 100.f; }, {}, [](int t) { return t >= 80 ? (t - 80) / 20.f : 0.f; });
    CHECK(r->name == "route" && r->children.size() == 64);
    auto shown = [&](int t) {
        int k = 0;
        for (auto& ch : r->children) {
            ch->up(t, *ch);
            k += ch->visible;
        }
        return k;
    };
    CHECK(shown(0) == 0 && shown(50) == 32 && shown(100) == 0 && shown(90) == 26);   // 90: from 0.5 to 0.9
    shown(50);
    const g3d::Object3D& s = *r->children[10];                 // a whole segment, along the arc, lying flat
    const Vector3 a = globe::arc(lon, tok, 10 / 64.f), b = globe::arc(lon, tok, 11 / 64.f);
    CHECK(nearV(s.position, Vector3Lerp(a, b, 0.5f), 1e-4f));
    CHECK(nearV(turn(s.rotation, { 0, 0, 1 }), Vector3Normalize(Vector3Subtract(b, a)), 1e-3f));
    CHECK(Vector3DotProduct(turn(s.rotation, { 0, 1, 0 }), Vector3Normalize(s.position)) > 0.95f);   // tilted by the climb

    g3d::Object3D plane;
    globe::fly(plane, lon, tok, 0.3f);
    CHECK(nearV(plane.position, globe::arc(lon, tok, 0.3f), 1e-5f));
    const Vector3 ahead = Vector3Normalize(Vector3Subtract(globe::arc(lon, tok, 0.31f), globe::arc(lon, tok, 0.29f)));
    CHECK(Vector3DotProduct(turn(plane.rotation, { 0, 0, 1 }), ahead) > 0.999f);
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

static void rendering() {
    const Color SEA{ 0, 0, 255, 255 }, LAND{ 255, 0, 0, 255 };
    auto scene = std::make_shared<g3d::Scene>();
    globe::Map m(1024, SEA);
    m.fill(geo::land().merged(), LAND);
    g3d::Material earth;
    earth.unlit = true;
    earth.dynamicMap = m.texture();
    scene->add(globe::sphere(1, 96), earth);
    globe::look(scene->camera, { 20, 0 }, 3);                  // Africa in the middle
    Image a = renderScene(scene, 0);
    const Color ca = GetImageColor(a, 160, 160);
    CHECK(ca.r > 200 && ca.b < 60);
    UnloadImage(a);
    globe::look(scene->camera, { -150, 0 }, 3);                // the Pacific
    Image b = renderScene(scene, 0);
    const Color cb = GetImageColor(b, 160, 160);
    CHECK(cb.b > 200 && cb.r < 60);
    // north is up: looking at Europe, Africa (land) is below the middle, the Med (sea) at it
    UnloadImage(b);

    // A texture remade every frame, and a see-through shell over the globe.
    auto changing = std::make_shared<g3d::DynamicTexture>();
    int calls = 0;
    changing->make = [&calls](int t) {
        calls++;
        return GenImageColor(8, 4, t < 10 ? Color{ 0, 255, 0, 255 } : Color{ 255, 255, 0, 255 });
    };
    changing->every = 5;
    auto s2 = std::make_shared<g3d::Scene>();
    g3d::Material dyn;
    dyn.unlit = true;
    dyn.dynamicMap = changing;
    s2->add(globe::sphere(1, 32), dyn);
    globe::look(s2->camera, { 0, 0 }, 3);
    Image g0 = renderScene(s2, 0), g1 = renderScene(s2, 3), g2 = renderScene(s2, 12);
    CHECK(GetImageColor(g0, 160, 160).g > 200 && GetImageColor(g0, 160, 160).r < 60);
    CHECK(GetImageColor(g2, 160, 160).r > 200 && GetImageColor(g2, 160, 160).g > 200);
    CHECK(calls == 2);                                         // frames 0 and 3 share a turn
    UnloadImage(g0);
    UnloadImage(g1);
    UnloadImage(g2);

    auto s3 = std::make_shared<g3d::Scene>();
    s3->add(globe::sphere(1, 64), earth);
    globe::Map veil(256, BLANK);
    veil.disc({ 20, 0 }, 1500, WHITE, 0);
    g3d::Material shell;
    shell.unlit = true;
    shell.opacity = 0.999f;
    shell.dynamicMap = veil.texture();
    s3->add(globe::sphere(1.02f, 64), shell);
    globe::look(s3->camera, { 20, 0 }, 3);
    Image v = renderScene(s3, 0);
    const Color centre = GetImageColor(v, 160, 160);
    CHECK(centre.r > 240 && centre.g > 240 && centre.b > 240);  // the white disc covers Africa
    const Color off = GetImageColor(v, 160, 60);                // above it the globe shows through
    CHECK(!(off.r > 240 && off.g > 240 && off.b > 240) && (off.r > 150 || off.b > 150));
    UnloadImage(v);
}

int main() {
    places();
    maps();
    fields();
    weather();
    routes();
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 180, "globe");
    if (!IsWindowReady()) {
        std::puts("no window: globe rendering skipped");
        return failures ? 1 : 77;
    }
    rendering();
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d globe check(s) failed\n", failures); return 1; }
    std::puts("globe tests passed");
    return 0;
}
