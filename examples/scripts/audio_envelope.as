void configure(Composition &inout c) {
    c.name = "audio-envelope";
    c.w = 1280;
    c.h = 720;
    c.fps = 30;
    c.duration = 120;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");

    array<float> gain = { 0, 0, 12, 1, 72, 0.45, 108, 0.45, 119, 0 };
    media::audioEnvelope(0, c.duration, "audio/austria/chime.wav", gain,
                         0, 0, 1, true, 0.7);
}

void draw(int frame) {
    draw::text("A local-frame gain envelope", 96, 120, 42, 0xf0f3f7);
    draw::text("The cue fades in, holds, then fades out.", 96, 184, 24, 0x9aa8b7);
    draw::rect(96, 300, 1088, 2, 0x394554);
    draw::circle(96 + 1088.0f * float(frame) / 119, 301, 8, 0xffb454);
}
