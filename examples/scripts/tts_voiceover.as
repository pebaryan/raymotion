// Voice-over -- Lines spoken by a TTS command, cached, with captions. Needs a voice command set up.
// Category: Audio and video

void configure(Composition &inout c) {
    c.name = "tts-voiceover";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 360;
    c.background = 0x0d1118;
    array<string> lines = {
        "Di dalam rumah tua, jam berhenti berdetak.",
        "Lalu seseorang berbisik dari lantai atas."
    };
    tts::speak(lines, 24, 0.9f, "", "out/tts/halloween", 0.45, 0.2, 2.8, true);
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float pulse = 0.5f + 0.5f * sin(frame * 0.09f);
    draw::rect(0, 0, 1280, 720, 0x0d1118);
    draw::circle(640, 285, 105 + pulse * 5, 0x281824, 0.9f);
    draw::circle(640, 285, 72, 0x792f46, 0.7f + pulse * 0.2f);
    draw::text("VOICEOVER / CACHED TAKES / AUTO CAPTIONS", 640, 120, 26, 0xe7d8ca, 1, 0.5f);
    draw::text("AngelScript declares lines; the asset hook renders audio before build.", 640, 550, 17, 0x99a4b4, 1, 0.5f);
}
