void configure(Composition &inout c) {
    c.name = "camera-card";
    c.w = 1280;
    c.h = 720;
    c.fps = 30;
    c.duration = 180;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    array<float> wide;
    cam::fit(0, 0, 1280, 720, 1280, 720, 0.08, 0, wide);
    array<float> keys = {
        0, wide[0], wide[1], wide[2], wide[3], E_LINEAR, 0, 1.414,
        90, 700, 350, 1.12, -3, E_OUT_CUBIC, 0, 1.414,
        150, 720, 350, 1.35, -8, E_IN_OUT_CUBIC, 1, 1.414
    };
    array<float> shot;
    cam::track(frame, keys, 1280, 720, shot);
    array<float> shaken;
    cam::shake(shot, frame, 0.8, 0.7, 2026, shaken);

    cam::apply(shaken, 1280, 720);
    draw::circle(900, 430, 130, 0x263948);
    draw::ring(900, 430, 130, 3, 0x70d7ff, 0.8);
    draw::text("A frame driven camera", 350, 130, 38, 0xf0f3f7);
    draw::text("fit  →  track  →  seeded shake", 350, 190, 22, 0xffb454);
    cam::restore();

    draw::text("HUD stays fixed while the canvas moves", 42, 664, 19, 0x9aa8b7);
}
