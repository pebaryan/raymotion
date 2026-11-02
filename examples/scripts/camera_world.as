void configure(Composition &inout c) {
    c.name = "camera-world";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x10141d;
    scene(0, c.duration, "draw");
    array<float> keys = {
        0, 640, 360, 1.0f, 0, 0, 0, 1,
        42, 860, 355, 1.8f, 0, 7, 0, 1,
        89, 640, 360, 1.0f, 0, 7, 1, 1.414f
    };
    cam::world(keys, c.w, c.h, 1.0f);
    media::audio(0, c.duration, "audio/austria/chime.wav", 0, 0, 1, true, 0.25f);
}

void draw(int frame) {
    draw::rect(150, 130, 980, 460, 0x1a2532);
    for (int i = 0; i < 9; ++i) {
        float x = 190.0f + float(i) * 110.0f;
        draw::line(x, 180, x, 540, 0x334657, 2);
    }
    for (int i = 0; i < 4; ++i) {
        float y = 190.0f + float(i) * 100.0f;
        draw::line(190, y, 1090, y, 0x334657, 2);
    }
    draw::circle(850, 355, 24, 0xf2a65a);
    draw::text("WORLD NODE: MEDIA + ART MOVE TOGETHER", 640, 90, 24, 0xe8edf2, 1, 0.5f);
}
