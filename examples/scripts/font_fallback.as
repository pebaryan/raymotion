void configure(Composition &inout c) {
    c.name = "font-fallback";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11131b;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    array<string> candidates = { "MissingDisplayFont.ttf", "SpaceGrotesk-Variable.ttf" };
    draw::textFile("TĀMĀKI MAKAURAU • JAKARTA", 640, 170, 44, 0xe7c27a, candidates, 1, 0.5f);
    string style = "{\"face\":\"file\",\"fontCandidates\":[\"MissingDisplayFont.ttf\",\"SpaceGrotesk-Variable.ttf\"],\"baseSize\":112,\"spacing\":1.2,\"align\":\"center\"}";
    txt::revealStyled("Māori names stay intact", 640, 365, 37, 0x90d1c4, frame, style,
        "{\"by\":\"word\",\"stagger\":8,\"duration\":18,\"offsetY\":26,\"ease\":\"out-cubic\"}", 0.5f, 0.5f);
    draw::text("ORDERED FALLBACK • UTF-8 GLYPH ATLAS", 640, 630, 18, 0x9ba4ae, 1, 0.5f);
}
