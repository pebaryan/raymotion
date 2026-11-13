// Splat colours -- A splat sphere whose colours come from a script function of position.
// Category: 3D

uint tintSplat(float x, float y, float z, int index) {
    if (y > 0.2f) return 0xffbf69ff;
    if (y < -0.2f) return 0x4c91cfff;
    return (index % 2 == 0) ? 0xeaf0d8ff : 0xa8c7b4ff;
}

void configure(Composition &inout c) {
    c.name = "splat-color-function";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11151d;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    SplatCloud@ cloud = splat::sphereFunction(0, 1.5f, 0, 1.2f, 2400, "tintSplat", 18);
    string world = "{\"camera\":{\"position\":[5,4,8],\"target\":[0,1,0]},\"objects\":[{\"splat\":" + cloud.descriptor() + ",\"position\":[0,0,0]}]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
    draw::text("POSITION-SAMPLED COLOR • SEEDED PLACEMENT", 36, 674, 20, 0xe9e2d5);
}
