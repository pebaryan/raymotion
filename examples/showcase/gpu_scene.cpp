// gpu_scene.cpp -- "gpu-3d": a lit, shadowed GPU scene on the timeline, with an orbiting
// camera, a studio environment the metal reflects, a sweeping spot light, a shadow-casting
// point light, depth of field, fog, a second camera in an inset viewport, and 2D titles.
#include "rmtn/raymotion.hpp"

namespace rmtn {

void registerGpuScene(Registry& out) {
    Comp c;
    c.name = "gpu-3d"; c.w = 1920; c.h = 1080; c.fps = 30; c.duration = 240;
    const Color sky = mkColor(0x0b1020), ink = mkColor(0xf2f4ff), teal = mkColor(0x47dad8), amber = mkColor(0xffc365);
    c.background = sky;
    const int fps = c.fps;

    auto scene = std::make_shared<g3d::Scene>();
    // Image-based lighting: a dim studio with soft light boxes for the metal to reflect.
    scene->environmentGenerator = g3d::studioEnvironment(mkColor(0x3a4670), mkColor(0x1a2036), mkColor(0x0c0e18), 4);
    scene->environmentIntensity = 0.9f;
    scene->fogColor = sky;
    scene->fogNear = 14; scene->fogFar = 34;

    g3d::Material floor;
    floor.color = mkColor(0x30364a);
    floor.roughness = 0.9f;
    scene->add(g3d::Geometry::plane(80, 80), floor);

    // The centrepiece: a polished knot that springs up, then turns.
    g3d::Material metal;
    metal.color = teal; metal.roughness = 0.22f; metal.metalness = 0.55f;
    g3d::Object3D* knot = scene->add(g3d::Geometry::knot(1.1f, 0.34f), metal);
    knot->up = [fps](int t, g3d::Object3D& o) {
        float k = (float)spring(t, { (double)fps, { 1, 80, 9 } });
        o.scale = { k, k, k };
        o.position = { 0, 2.0f + 0.15f * std::sin(t * 0.05f), 0 };
        o.rotation = { 20, t * 1.5f, t * 0.6f };
    };
    g3d::Material halo;
    halo.color = amber; halo.roughness = 0.35f; halo.emissive = Color{ 60, 40, 10, 255 };
    g3d::Object3D* ring = scene->add(g3d::Geometry::torus(2.4f, 0.06f, 96, 12), halo);
    ring->up = [](int t, g3d::Object3D& o) {
        o.position = { 0, 2.0f, 0 };
        o.rotation = { 70 + 10 * std::sin(t * 0.03f), 0, t * 0.8f };
    };

    // Eight spheres orbit at different heights, popping in one after another.
    const int count = 8;
    for (int i = 0; i < count; i++) {
        g3d::Material m;
        m.color = ColorFromHSV(190.f + i * 22.f, 0.55f, 0.95f);
        m.roughness = 0.4f;
        g3d::Object3D* ball = scene->add(g3d::Geometry::sphere(0.32f), m);
        ball->up = [i, count, fps](int t, g3d::Object3D& o) {
            SpringOptions s;
            s.fps = fps; s.delay = 20 + i * 4; s.config.damping = 11;
            float k = (float)spring(t, s);
            float a = (float)(i * 2 * PI / count) + t * 0.02f;
            o.scale = { k, k, k };
            o.position = { 4.2f * std::cos(a), 0.6f + 0.35f * std::sin(t * 0.08f + i), 4.2f * std::sin(a) };
        };
    }
    // Low pillars at the edge of the light, below the camera's line of sight.
    g3d::Material stone;
    stone.color = mkColor(0x8a90a8);
    stone.roughness = 0.8f;
    for (int i = 0; i < 6; i++) {
        float a = (float)(i * 2 * PI / 6 + PI / 6);
        g3d::Object3D* p = scene->add(g3d::Geometry::box(0.7f, 1.5f + (i % 3), 0.7f), stone);
        p->position = { 7.f * std::cos(a), (1.5f + (i % 3)) / 2, 7.f * std::sin(a) };
        p->rotation = { 0, -a * RAD2DEG, 0 };
    }

    scene->lights.push_back(g3d::Light::directional({ -6, 12, 5 }, mkColor(0xdfe6ff), 1.1f, true));
    scene->lights.back().shadowExtent = 11;
    g3d::Light sweep = g3d::Light::spot({ 6, 7, 6 }, { 0, 0, 0 }, amber, 2.2f, 24);
    sweep.range = 22;
    sweep.up = [](int t, g3d::Light& l) {
        float a = t * 0.025f;
        l.target = { 3.5f * std::cos(a), 0, 3.5f * std::sin(a) };
    };
    scene->lights.push_back(sweep);
    // A pink point light under the knot throws the spheres' shadows outward.
    g3d::Light glow = g3d::Light::point({ 0, 0.8f, 0 }, mkColor(0xff4fa3), 1.6f, 9);
    glow.castShadow = true;
    scene->lights.push_back(glow);

    // Main camera: dollies in with a spring and orbits.
    scene->camera.fov = 40;
    scene->camera.up = [fps](int t, g3d::Camera& cam) {
        float dist = (float)interpolate(spring(t, { (double)fps, { 1, 30, 12 } }), { 0, 1 }, { 22, 12 });
        float a = (float)interpolate(t, { 0, 240 }, { 0.3, 1.9 });
        cam.position = { dist * std::cos(a), 6.0f + 1.0f * std::sin(t * 0.02f), dist * std::sin(a) };
        cam.target = { 0, 1.6f, 0 };
        // Focus on the knot, so the pillars behind it soften.
        Vector3 d{ cam.position.x, cam.position.y - 2.0f, cam.position.z };
        cam.dof.focusDistance = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        cam.dof.aperture = 1.2f;
        cam.dof.maxBlur = 10;
    };

    // Backdrop gradient behind the scene (the scene's background is transparent).
    addRoot(c.roots, 0, c.duration)->fn = [=](const Ctx& cx, int) {
        dr::rectGrad4(cx, 0, 0, 1920, 1080, mkColor(0x161c36), mkColor(0x161c36), sky, sky);
    };
    addRoot(c.roots, 0, c.duration)->fn = g3d::draw(scene);

    // Inset: a top-down camera in a framed viewport, fading in after two seconds.
    auto top = std::make_shared<g3d::Camera>();
    top->position = { 0, 18, 0.01f };
    top->target = { 0, 0, 0 };
    top->fov = 50;
    const Rectangle inset{ 1920 - 64 - 480, 1080 - 64 - 300, 480, 300 };
    TNode* pip = addRoot(c.roots, 60, c.duration - 60);
    pip->up = [](int t, NodeProps& p) { p.opacity = (float)interpolate(t, { 0, 20 }, { 0, 1 }, clamped()); };
    TNode* frame = pip->add(0, c.duration);
    frame->fn = [=](const Ctx& cx, int) {
        dr::roundRect(cx, inset.x - 6, inset.y - 6, inset.width + 12, inset.height + 12, 14, Fade(ink, 0.85f));
        dr::rect(cx, inset.x, inset.y, inset.width, inset.height, sky);
    };
    pip->add(0, c.duration)->fn = g3d::draw(scene, inset, top);

    // Title, revealed left to right.
    TNode* title = addRoot(c.roots, 12, c.duration - 12);
    title->isolate().mask = [fps](const Ctx& cx, int t) {
        float k = (float)interpolate(t, { 0, 0.7 * fps }, { 0, 1 }, clamped(ease::out(ease::cubic)));
        dr::rect(cx, 0, 0, 1920 * k, 1080, WHITE);
    };
    title->fn = [=](const Ctx& cx, int) {
        auto rows = layout::column({ 96, 72, 1000, 200 }, { layout::px(110), layout::px(56) }, 4);
        TextStyle big;
        big.font = fonts::ui(true); big.size = 96; big.color = ink;
        dr::textBox(cx, "GPU 3D", rows[0], big);
        TextStyle small = big;
        small.font = fonts::ui(false); small.size = 40; small.color = teal;
        dr::textBox(cx, "reflections, shadows, depth of field and two cameras", rows[1], small);
    };

    out.push_back(std::move(c));
}

} // namespace rmtn
