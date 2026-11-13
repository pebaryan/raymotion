// World camera from a function -- The same camera, with the shot returned by a script function each frame.
// Category: Motion

string movingShot(int frame) {
    array<float> from = { 640, 360, 1.0f, 0 };
    array<float> to = { 860, 355, 1.8f, 0 };
    array<float> shot;
    cam::mix(from, to, clamp01(float(frame) / 42.0f), shot);
    return "[" + shot[0] + "," + shot[1] + "," + shot[2] + "," + shot[3] + "]";
}

void configure(Composition &inout c) {
    c.name = "camera-world-function";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x10141d;
    scene(0, c.duration, "draw");
    cam::worldFunction("movingShot", c.w, c.h, 1.0f);
}

void draw(int frame) {
    draw::rect(150, 130, 980, 460, 0x1a2532);
    draw::circle(850, 355, 24, 0xf2a65a);
    draw::text("FUNCTION CAMERA: EVERY ROOT FOLLOWS THE SHOT", 640, 90, 23, 0xe8edf2, 1, 0.5f);
}
