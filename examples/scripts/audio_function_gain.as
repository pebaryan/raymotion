// Audio gain from a function -- The same fade, with the gain returned by a script function.
// Category: Audio and video

float cueGain(int localFrame) {
    if (localFrame < 12) return float(localFrame) / 12.0f;
    if (localFrame < 72) return 1.0f;
    if (localFrame < 108) return 0.45f;
    return 0.45f * (120.0f - float(localFrame)) / 12.0f;
}

void configure(Composition &inout c) {
    c.name = "audio-function-gain";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 120;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
    media::audio(0, c.duration, "audio/austria/chime.wav", 0, 0, 1, true, 0.7f, "cueGain");
}

void draw(int frame) {
    draw::text("A scripted local-frame gain", 96, 120, 42, 0xf0f3f7);
    draw::text("The callback fades in, holds, then fades out.", 96, 184, 24, 0x9aa8b7);
    draw::rect(96, 300, 1088, 2, 0x394554);
    draw::circle(96 + 1088.0f * float(frame) / 119, 301, 8, 0xffb454);
}
