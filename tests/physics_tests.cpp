#include "rmtn/raymotion.hpp"
#include <raylib.h>
#include <raymath.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <chrono>
using namespace rmtn;

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static bool same(const std::vector<Vector3>& a, const std::vector<Vector3>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) if (std::memcmp(&a[i], &b[i], sizeof(Vector3)) != 0) return false;
    return true;
}
static float distance(Vector3 a, Vector3 b) { return Vector3Length(Vector3Subtract(a, b)); }
static bool finite(Vector3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }

static void ropeTests(physics::Params p) {
    physics::Rope hanging({ 0, 0, 0 }, { 0, 2, 0 }, 20, p);
    hanging.pinEnd();
    const auto& hang = hanging.frame(120);
    const float rest = 2.0f / 20;
    CHECK(hang.back().y > hang.front().y);   // the pinned end is the top
    CHECK(std::fabs(hang.front().x - hang.back().x) < 0.04f);
    CHECK(std::fabs(hang.front().z - hang.back().z) < 0.04f);
    float ropeLength = 0;
    for (size_t i = 1; i < hang.size(); ++i) {
        const float segment = distance(hang[i], hang[i - 1]);
        ropeLength += segment;
        CHECK(std::fabs(segment - rest) < rest * 0.02f);
    }
    CHECK(std::fabs(ropeLength - 2.0f) < 0.02f);

    physics::Rope first({ 0, 0, 0 }, { 0, 1, 0 }, 12, p), second({ 0, 0, 0 }, { 0, 1, 0 }, 12, p);
    first.pinEnd(); second.pinEnd();
    const auto future = first.frame(90);
    for (int i = 0; i <= 90; ++i) second.frame(i);
    CHECK(same(future, second.frame(90)));
    const auto past = first.frame(10);
    CHECK(same(past, first.frame(10)));

    physics::Rope moving({ 0, 0, 0 }, { 0, 1, 0 }, 8, p);
    moving.pinEnd([](double t) { return Vector3{ (float)t, 0, 0 }; });
    CHECK(std::fabs(moving.frame(30).back().x - 1) < 1e-5f);
    physics::Rope movingPinA({ 0, 0, 0 }, { 0, 1, 0 }, 8, p);
    physics::Rope movingPinB({ 0, 0, 0 }, { 0, 1, 0 }, 8, p);
    auto pinPath = [](double t) { return Vector3{ (float)std::sin(t), (float)std::cos(t), 0 }; };
    movingPinA.pinEnd(pinPath); movingPinB.pinEnd(pinPath);
    const auto movingFuture = movingPinA.frame(90);
    for (int i = 0; i <= 90; ++i) movingPinB.frame(i);
    CHECK(same(movingFuture, movingPinB.frame(90)));

    physics::Rope floor({ 0, 1, 0 }, { 0, 2, 0 }, 8, p);
    floor.collide(physics::Collider::plane({ 0, 0, 0 }, { 0, 1, 0 }));
    for (Vector3 q : floor.frame(120)) CHECK(q.y >= p.thickness - 1e-3f);

    auto drivenSphere = [](double t) { return Vector3{ (float)(0.15 * std::sin(t)), 0, 0 }; };
    physics::Rope movingColliderA({ -0.5f, 0.7f, 0 }, { 0.5f, 0.7f, 0 }, 10, p);
    physics::Rope movingColliderB({ -0.5f, 0.7f, 0 }, { 0.5f, 0.7f, 0 }, 10, p);
    physics::Collider sphereA = physics::Collider::sphere({ 0, 0, 0 }, 0.2f);
    physics::Collider sphereB = sphereA;
    sphereA.drive = drivenSphere; sphereB.drive = drivenSphere;
    movingColliderA.collide(sphereA); movingColliderB.collide(sphereB);
    const auto colliderFuture = movingColliderA.frame(90);
    for (int i = 0; i <= 90; ++i) movingColliderB.frame(i);
    CHECK(same(colliderFuture, movingColliderB.frame(90)));

    physics::Rope bent({ 0, 0, 0 }, { 1, 0, 0 }, 10, p);
    bent.bend(0.8f);
    CHECK(bent.frame(20).size() == 11);
}

static void clothTests(physics::Params p) {
    physics::Cloth cloth(12, 10, 0.15f, { -0.825f, 2, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, p);
    cloth.pinCorners();
    const auto cp = cloth.frame(90);
    CHECK(cp[5 * 12 + 6].y < cp[0].y);
    for (Vector3 q : cp) CHECK(finite(q));
    float maxStretch = 0;
    for (int y = 0; y < cloth.rows(); ++y) for (int x = 0; x < cloth.columns(); ++x) {
        const int i = y * cloth.columns() + x;
        if (x + 1 < cloth.columns()) maxStretch = std::max(maxStretch, distance(cp[i], cp[i + 1]) / 0.15f - 1);
        if (y + 1 < cloth.rows()) maxStretch = std::max(maxStretch, distance(cp[i], cp[i + cloth.columns()]) / 0.15f - 1);
    }
    CHECK(maxStretch < 0.05f);

    physics::Cloth drape(14, 14, 0.12f, { -0.78f, 1.3f, -0.78f }, { 1, 0, 0 }, { 0, 0, 1 }, p);
    drape.pinCorners().collide(physics::Collider::sphere({ 0, 0, 0 }, 0.5f));
    const auto dp = drape.frame(120);
    float lowest = 10;
    for (Vector3 q : dp) { CHECK(distance(q, { 0, 0, 0 }) >= 0.5f + p.thickness - 0.015f); lowest = std::min(lowest, q.y); }
    CHECK(lowest < 0.5f);

    physics::Cloth windA(8, 8, 0.1f, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, p);
    physics::Cloth windB(8, 8, 0.1f, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, p);
    physics::Cloth windC(8, 8, 0.1f, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, p);
    windA.pinRow(0).wind({ { 1, 0, 0 }, 4, 0.4f, 1, 5 });
    windB.pinRow(0).wind({ { 1, 0, 0 }, 4, 0.4f, 1, 5 });
    windC.pinRow(0).wind({ { 1, 0, 0 }, 4, 0.4f, 1, 9 });
    const auto wa = windA.frame(30), wb = windB.frame(30), wc = windC.frame(30);
    CHECK(same(wa, wb));
    CHECK(!same(wa, wc));
    CHECK(wa.back().x > windA.frame(0).back().x);

    physics::Cloth windOrderA(8, 8, 0.1f, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, p);
    physics::Cloth windOrderB(8, 8, 0.1f, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, p);
    windOrderA.pinRow(0).wind({ { 1, 0, 0 }, 4, 0.4f, 1, 31 });
    windOrderB.pinRow(0).wind({ { 1, 0, 0 }, 4, 0.4f, 1, 31 });
    const auto windyFuture = windOrderA.frame(90);
    for (int i = 0; i <= 90; ++i) windOrderB.frame(i);
    CHECK(same(windyFuture, windOrderB.frame(90)));

    physics::Cloth tearing(8, 8, 0.1f, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, p);
    const int intact = tearing.liveConstraintCount();
    CHECK(intact > 0);
    physics::Cloth pulled(8, 8, 0.1f, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, p);
    pulled.pin(0, [](double) { return Vector3{ -10, 0, 0 }; }).tear(1.05f);
    CHECK(pulled.liveConstraintCount(5) < intact);

    physics::Cloth self(12, 12, 0.1f, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, p);
    self.selfCollision(true);
    CHECK(self.frame(10).size() == 144);
}

// On a tilted plane a rope slides down; friction holds it back.
static void frictionTest(physics::Params p) {
    auto slide = [&](float friction) {
        physics::Params q = p;
        q.friction = friction;
        physics::Rope rope({ 0, 0.1f, 0 }, { 0.2f, 0.1f, 0 }, 4, q);
        rope.collide(physics::Collider::plane({ 0, 0, 0 }, { 0.5f, 1, 0 }));
        const auto& end = rope.frame(60);
        return distance(end[2], { 0.1f, 0.1f, 0 });
    };
    const float free = slide(0), held = slide(0.9f);
    CHECK(free > 0.2f);
    CHECK(held < free * 0.5f);
}

static void meshTests(physics::Params p) {
    physics::Rope rope({ 0, 0, 0 }, { 0, 2, 0 }, 20, p);
    Mesh tube = physics::ropeMesh(rope, 10, 0.03f, 8);
    CHECK(tube.vertexCount == 21 * 8 + 2);
    CHECK(tube.triangleCount == (20 * 8 * 2 + 8 * 2));
    for (int i = 0; i < tube.vertexCount * 3; ++i) CHECK(std::isfinite(tube.normals[i]));
    for (int i = 0; i < tube.triangleCount * 3; ++i) CHECK(tube.indices[i] < tube.vertexCount);
    UnloadMesh(tube);

    physics::Cloth cloth(5, 4, 0.2f, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, p);
    Mesh sheet = physics::clothMesh(cloth, 0);
    CHECK(sheet.vertexCount == 40);
    CHECK(sheet.triangleCount == 2 * 4 * 3 * 2);
    for (int i = 0; i < sheet.vertexCount; ++i) {
        const float x = sheet.normals[i * 3], y = sheet.normals[i * 3 + 1], z = sheet.normals[i * 3 + 2];
        CHECK(std::fabs(std::sqrt(x * x + y * y + z * z) - 1) < 1e-4f);
    }
    UnloadMesh(sheet);
    Mesh oneSided = physics::clothMesh(cloth, 0, false);
    CHECK(oneSided.vertexCount == 20);
    CHECK(oneSided.triangleCount == 2 * 4 * 3);
    UnloadMesh(oneSided);
}

static void timingTest(physics::Params p) {
    physics::Cloth cloth(64, 64, 0.02f, { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, p);
    const auto begin = std::chrono::steady_clock::now();
    cloth.frame(1);
    const auto end = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(end - begin).count();
    std::printf("64x64 cloth: %.3f ms per simulated frame\n", ms);
}

static int glTest() {
    constexpr int width = 240, height = 160;
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(width, height, "raymotion physics");
    if (!IsWindowReady()) return 77;
    physics::Params p;
    physics::Rope rope({ 0, 1, 0 }, { 0, 3, 0 }, 12, p); rope.pinEnd();
    physics::Cloth cloth(8, 8, 0.12f, { -0.42f, 1.7f, -0.42f }, { 1, 0, 0 }, { 0, 0, 1 }, p); cloth.pinCorners();
    auto scene = std::make_shared<g3d::Scene>();
    scene->camera.position = { 3, 3, 6 }; scene->camera.target = { 0, 1.4f, 0 };
    scene->lights.push_back(g3d::Light::directional({ 4, 8, 3 }, WHITE, 2, true));
    g3d::Material material; material.color = { 90, 180, 230, 255 };
    scene->add(physics::ropeObject(rope, 0.025f, material));
    scene->add(physics::clothObject(cloth, material));
    Comp comp; comp.w = width; comp.h = height; comp.duration = 70; comp.background = BLACK;
    addRoot(comp.roots, 0, comp.duration)->fn = g3d::draw(scene, {});
    auto render = [&](int frame) {
        Canvas canvas; canvas.load(comp.w, comp.h, 1); presentFrame(comp, frame, canvas);
        Image image = LoadImageFromTexture(canvas.output().texture); ImageFlipVertical(&image); canvas.unload(); return image;
    };
    Image f5 = render(5), f60 = render(60), repeated = render(60);
    bool changed = false, visible = false, repeatable = true;
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        Color a = GetImageColor(f5, x, y), b = GetImageColor(f60, x, y), c = GetImageColor(repeated, x, y);
        changed |= a.r != b.r || a.g != b.g || a.b != b.b;
        visible |= b.r != 0 || b.g != 0 || b.b != 0;
        repeatable &= b.r == c.r && b.g == c.g && b.b == c.b;
    }
    CHECK(changed); CHECK(visible); CHECK(repeatable);
    UnloadImage(f5); UnloadImage(f60); UnloadImage(repeated);
    shutdownGraphics(); CloseWindow();
    return 0;
}

int main() {
    physics::Params params; params.substeps = 4; params.iterations = 8;
    ropeTests(params);
    clothTests(params);
    frictionTest(params);
    meshTests(params);
    timingTest(params);
    const int glResult = glTest();
    if (glResult == 77) return 77;
    if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    std::puts("physics tests passed");
    return 0;
}
