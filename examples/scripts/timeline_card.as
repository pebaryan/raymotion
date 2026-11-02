void configure(Composition &inout c) {
    c.name = "timeline-card";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 105;
    c.background = 0x11131b;
    motionBlur(4, 0.5f);
    loop(0, 15, 6, "orbit");
    freeze(90, 15, 30, "orbit");
}

void orbit(int frame) {
    float phase = frame / 15.0f * 360.0f;
    draw::circle(640, 360, 190, 0x26313c);
    draw::circle(640 + cos(phase * 0.01745329252f) * 190, 360 + sin(phase * 0.01745329252f) * 190, 28, 0xffb454);
    draw::text("LOOP + FREEZE + COMPOSITION MOTION BLUR", 640, 630, 24, 0xe9e5dc, 1, 0.5f);
    draw::text("local frame: " + frame, 640, 665, 16, 0xb8c6cc, 1, 0.5f);
}
