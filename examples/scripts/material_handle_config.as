void configure(Composition &inout c) {
    c.name = "material-handle-config";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11151d;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    Material3D@ material = g3d::material();
    material.configure("{\"color\":\"#b8d7ffff\",\"shading\":\"toon\",\"toon\":{\"bands\":4,\"rim\":\"#ff875566\",\"rimWidth\":0.42},\"clip\":{\"normal\":[1,0,0],\"offset\":0.15,\"edgeWidth\":0.12,\"edgeColor\":\"#72dcff99\"},\"lineColor\":\"#18202aff\",\"lineWidth\":1.2}");
    Scene3D@ world = g3d::scene();
    Geometry3D@ geometry = g3d::sphere(1.0f, 32, 48);
    Object3D@ assembly = world.addGroup();
    assembly.name("orb-assembly");
    assembly.add(geometry, material);
    Object3D@ located = world.find("orb-assembly");
    Object3D@ orb = located.childAt(0);
    orb.position(0, 0.8f, 0);
    array<float> evaluatedCamera;
    world.cameraAt(frame, evaluatedCamera);
    world.render(frame, 0, 0, 1280, 720);
    draw::text("FULL MATERIAL JSON • RGBA RIM AND CLIP EDGE", 38, 676, 18, 0xe8e2d5);
}
