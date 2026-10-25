// particles3d.cpp -- "particles-3d": fx::Emitter3D in a g3d scene. A campfire (flames,
// smoke, embers) lights a night yard with a flickering shadow-casting light; a fountain
// plays into a basin; a crate bursts into lit chunks that land and rest; fireworks open
// overhead and confetti falls to finish. The camera orbits, so the particles pass behind
// the geometry and the fog takes the far ones.
#include "rmtn/raymotion.hpp"

namespace rmtn {

void registerParticles3D(Registry& out) {
    Comp c;
    c.name = "particles-3d"; c.w = 1920; c.h = 1080; c.fps = 30; c.duration = 420;
    const Color night = mkColor(0x090c18), ink = mkColor(0xf2f4ff), amber = mkColor(0xffc365);
    c.background = night;
    const int boom = 75;                          // the crate bursts

    auto scene = std::make_shared<g3d::Scene>();
    scene->skyColor = mkColor(0x2a3358);
    scene->groundColor = mkColor(0x1a1410);
    scene->ambient = 0.22f;
    scene->fogColor = night;
    scene->fogNear = 12; scene->fogFar = 30;

    g3d::Material ground;
    ground.color = mkColor(0x2c3040);
    ground.roughness = 0.95f;
    scene->add(g3d::Geometry::plane(80, 80), ground);
    g3d::Material stone;
    stone.color = mkColor(0x8a8478);
    stone.roughness = 0.85f;

    // ---- the campfire, left: logs, flames, smoke and embers
    g3d::Object3D* camp = scene->add();
    camp->position = { -3.2f, 0, 0.6f };
    g3d::Material wood;
    wood.color = mkColor(0x4a2e1c);
    wood.roughness = 0.9f;
    for (int i = 0; i < 3; i++) {
        g3d::Object3D* log = camp->add(g3d::Geometry::cylinder(0.08f, 0.9f, 16), wood);
        log->position = { 0, 0.1f, 0 };
        log->rotation = { 0, i * 60.f, 72 };
    }
    for (int i = 0; i < 9; i++) {
        const float a = i * 2 * PI / 9;
        camp->add(g3d::Geometry::sphere(0.12f, 12, 16), stone)->position = { 0.55f * std::cos(a), 0.05f, 0.55f * std::sin(a) };
    }
    camp->add(fx::smoke3d(0.18f))->position = { 0, 0.9f, 0 };
    camp->add(fx::fire3d(0.26f))->position = { 0, 0.12f, 0 };
    fx::Emitter3D embers = fx::embers3d(0.3f);
    embers.rate = 0.9f;
    camp->add(embers)->position = { 0, 0.3f, 0 };
    g3d::Light fireLight = g3d::Light::point({ -3.2f, 0.8f, 0.6f }, mkColor(0xff9a4a), 2.4f, 9);
    fireLight.castShadow = true;
    fireLight.up = [](int t, g3d::Light& l) {   // flicker from a few incommensurate sines
        l.intensity = 2.2f + 0.35f * std::sin(t * 0.9f) + 0.2f * std::sin(t * 2.3f + 1) + 0.15f * std::sin(t * 5.1f);
    };
    scene->lights.push_back(fireLight);

    // ---- the fountain, right: a basin and a jet that falls back into it
    g3d::Object3D* fountain = scene->add();
    fountain->position = { 3.4f, 0, -0.8f };
    fountain->add(g3d::Geometry::torus(0.9f, 0.12f, 64, 16), stone)->rotation = { 90, 0, 0 };
    fountain->children.back()->position = { 0, 0.12f, 0 };
    fountain->add(g3d::Geometry::cylinder(0.12f, 0.6f, 24), stone)->position = { 0, 0.3f, 0 };
    g3d::Material water;
    water.color = mkColor(0x3a6a8a);
    water.roughness = 0.15f;
    water.opacity = 0.8f;
    fountain->add(g3d::Geometry::cylinder(0.85f, 0.02f, 48), water)->position = { 0, 0.14f, 0 };
    fx::Emitter3D jet = fx::fountain3d(9, mkColor(0x9ad8ff));
    jet.speed = { 0.085f, 0.095f };
    jet.spread = 18;
    jet.gravity = { 0, -0.004f, 0 };
    jet.life = { 46, 50 };                       // gone as they reach the pool
    jet.fadeOut = 0.15f;
    fountain->add(jet)->position = { 0, 0.6f, 0 };

    // ---- the crate, centre: sits, bursts at `boom` into chunks that land and rest
    g3d::Material crate;
    crate.color = mkColor(0xc08a4a);
    crate.roughness = 0.7f;
    g3d::Object3D* box = scene->add(g3d::Geometry::box(0.8f, 0.8f, 0.8f), crate);
    box->up = [boom](int t, g3d::Object3D& o) {
        const float squash = (float)interpolate(t, { boom - 12.0, (double)boom }, { 1, 0.8 }, clamped(ease::in(ease::cubic)));
        o.visible = t < boom;
        o.scale = { 1 / std::sqrt(squash), squash, 1 / std::sqrt(squash) };
        o.position = { 0, 0.4f * squash, 0.4f };
    };
    fx::Emitter3D chunks = fx::debris3d(46);
    chunks.burstAt = boom;
    chunks.area = fx::Volume::Box;
    chunks.extent = { 0.6f, 0.6f, 0.6f };
    chunks.life = { 400, 400 };
    chunks.fadeOut = 0.02f;
    chunks.size = { 0.1f, 0.22f };
    chunks.colors = { WHITE, mkColor(0xd8c8b0), mkColor(0xa89070) };
    scene->add(chunks, g3d::Geometry::box(1, 0.6f, 0.8f), crate)->position = { 0, 0.35f, 0.4f };
    fx::Emitter3D flash = fx::sparks3d(0, mkColor(0xffd08a));
    flash.burst = 120;
    flash.burstAt = boom;
    flash.spread = 200;
    flash.speed = { 0.06f, 0.16f };
    scene->add(flash)->position = { 0, 0.4f, 0.4f };

    // ---- fireworks overhead, then confetti
    const struct { int at; Vector3 p; Color a, b; } shells[] = {
        { 160, { -2.5f, 6.2f, -4 }, mkColor(0xff6b5a), mkColor(0xffc365) },
        { 200, { 2.8f, 6.8f, -5 }, mkColor(0x47dad8), mkColor(0xb8f3ff) },
        { 235, { 0.2f, 7.4f, -6 }, mkColor(0xb48cff), mkColor(0xffffff) },
        { 262, { -4.5f, 6.6f, -7 }, mkColor(0xffd166), mkColor(0xff8c42) },
    };
    for (const auto& s : shells) {
        fx::Emitter3D shell = fx::burst3d(260, { s.a, s.b });
        shell.burstAt = s.at;
        shell.speed = { 0.1f, 0.16f };
        shell.size = { 0.05f, 0.08f };
        shell.seed = (unsigned long long)s.at;
        scene->add(shell)->position = s.p;
        // the rising trail before it opens
        fx::Emitter3D trail = fx::sparks3d(2.5f, s.b);
        trail.from = s.at - 24;
        trail.until = s.at;
        trail.direction = { 0, -1, 0 };
        trail.spread = 30;
        trail.speed = { 0.01f, 0.03f };
        trail.gravity = { 0, -0.002f, 0 };
        const Vector3 top = s.p;
        const int at = s.at;
        trail.origin = [top, at](float f) {
            const float k = std::clamp((f - (at - 24)) / 24.f, 0.f, 1.f);
            return Vector3{ 0, -top.y * (1 - k * (2 - k)), 0 };   // eases out to the top
        };
        trail.look = fx::Look::Glow;
        trail.size = { 0.08f, 0.12f };
        scene->add(trail)->position = top;
    }
    fx::Emitter3D paper = fx::confetti3d(9, 6);
    paper.from = 265;
    paper.rate *= 1.6f;
    scene->add(paper)->position = { 0, 6.5f, 0 };

    // ---- light and camera
    scene->lights.push_back(g3d::Light::directional({ 6, 12, 8 }, mkColor(0x8090c8), 0.35f, true));
    scene->lights.back().shadowExtent = 9;
    scene->camera.fov = 42;
    scene->camera.up = [](int t, g3d::Camera& cam) {
        const float a = (float)interpolate(t, { 0, 420 }, { 1.95, 1.05 }, clamped(ease::inOut(ease::sine)));
        const float dist = (float)interpolate(t, { 0, 140, 300, 420 }, { 9.5, 8.5, 13.5, 12 }, clamped(ease::inOut(ease::cubic)));
        // looks up for the fireworks, then back down as the confetti falls
        const float lift = (float)interpolate(t, { 120, 220, 290, 400 }, { 0, 1, 1, 0.3 }, clamped(ease::inOut(ease::cubic)));
        cam.position = { dist * std::cos(a), 2.6f + 1.2f * lift, dist * std::sin(a) };
        cam.target = { 0, 0.8f + 2.4f * lift, 0 };
    };

    addRoot(c.roots, 0, c.duration)->fn = [=](const Ctx& cx, int) {
        dr::rectGrad4(cx, 0, 0, 1920, 1080, mkColor(0x141a34), mkColor(0x141a34), night, night);
    };
    addRoot(c.roots, 0, c.duration)->fn = g3d::draw(scene);

    TNode* title = addRoot(c.roots, 12, 130);
    title->up = [](int t, NodeProps& p) {
        p.opacity = (float)(interpolate(t, { 0, 18 }, { 0, 1 }, clamped()) * interpolate(t, { 110, 130 }, { 1, 0 }, clamped()));
    };
    title->fn = [=](const Ctx& cx, int) {
        auto rows = layout::column({ 96, 72, 1200, 200 }, { layout::px(110), layout::px(56) }, 4);
        TextStyle big;
        big.font = fonts::ui(true); big.size = 96; big.color = ink;
        dr::textBox(cx, "Particles in 3D", rows[0], big);
        TextStyle small = big;
        small.font = fonts::ui(false); small.size = 40; small.color = amber;
        dr::textBox(cx, "depth-tested, fogged, and mesh particles that land", rows[1], small);
    };
    out.push_back(std::move(c));
}

} // namespace rmtn
