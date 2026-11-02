void configure(Composition &inout c) {
    c.name = "physics-callbacks";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    PhysicsParams@ params = physics::params();
    params.gravity(0, -9.81f, 0);
    params.damping(0.01f);
    params.substeps(5);
    params.iterations(12);

    PhysicsRope@ rope = physics::rope(-1, 1.6f, 0, 1, 1.6f, 0, 36, params);
    rope.pin(0, -1, 1.6f, 0);
    rope.pinEndFunction("swayPin");
    rope.bend(0.18f);
    rope.collide(physics::plane(0, 0, 0, 0, 1, 0));

    PhysicsCloth@ cloth = physics::cloth(24, 18, 0.07f,
        -0.8f, 1.3f, -0.15f, 1, 0, 0, 0, 0, -1, params);
    cloth.pinRow(0);
    cloth.pinFunction(23, "swayPin");
    cloth.wind(physics::wind());
    cloth.tear(2.4f);

    g3d::Scene3D@ world = g3d::scene();
    world.camera(3, 2.4f, 5, 0, 0.7f, 0);
    world.addDirectional(3, 5, 3, 0xffffff, 1.8f, true);
    world.add(g3d::plane(5, 5), g3d::material());
    world.addRope(rope, 0.035f);
    world.addCloth(cloth);
    world.render(frame, 0, 0, 1280, 720);
    draw::text("PHYSICS CALLBACKS • ABSOLUTE SECOND PIN MOTION", 40, 675, 18, 0xb8c6cc);
}

string swayPin(double seconds) {
    return "[1," + (1.55f + 0.22f * sin(float(seconds) * 1.4f)) + ",0]";
}
