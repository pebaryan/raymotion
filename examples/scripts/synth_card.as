// Synth -- A score made of notes, partials and envelopes, with no audio file.
// Category: Audio and video

void configure(Composition &inout c) {
    c.name = "synth-card";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11131b;
    scene(0, c.duration, "draw");
    synth::enable();
    synth::note(0.0, 0.7, 110.0, 0.65, true);
    synth::note(0.0, 1.2, 164.81, 0.24, false);
    synth::note(0.0, 1.2, 220.0, 0.20, false);
    synth::note(0.8, 0.55, 130.81, 0.5, true);
    synth::note(1.2, 1.2, 155.56, 0.26, false);
    synth::note(1.2, 1.2, 207.65, 0.18, false);
    synth::note(2.1, 0.75, 82.41, 0.58, true);
    synth::note(2.1, 0.9, 123.47, 0.22, false);
}

void draw(int frame) {
    float beat = (frame % 24) / 24.0f;
    draw::circle(640, 330, 110 + beat * 45, 0x7d3048, 0.12f);
    draw::ring(640, 330, 175 + beat * 25, 3, 0xf1ad59, 0.8f);
    draw::text("PROCEDURAL SCORE", 640, 300, 36, 0xf0eee8, 1, 0.5f);
    draw::text("synth::note • sine partials • deterministic envelope", 640, 364, 18, 0xb8c6cc, 1, 0.5f);
    for (int i = 0; i < 8; ++i) {
        float x = 380 + i * 75;
        float h = 20 + ((i * 13 + frame * (i + 2)) % 100);
        draw::rect(x, 490 - h, 14, h, (i % 2 == 0) ? 0x70d7ff : 0xffb454, 0.85f);
    }
}
