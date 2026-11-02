void configure(Composition &inout c) {
    c.name = "textfx-authoring";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11151e;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float t = float(frame);
    draw::text("AUTHORED TEXT MOTION", 54, 32, 22, 0xdfe4eb);
    draw::text("WORD REVEAL / EDGE ORDER", 56, 100, 14, 0x9eabbd);
    txt::revealStyled("The quiet room remembers every name.", 56, 140, 31, 0xe7ab59, t,
        "{\"size\":31,\"face\":\"bold\",\"spacing\":0,\"width\":650}",
        "{\"by\":\"word\",\"order\":\"edges\",\"stagger\":8,\"duration\":24,\"ease\":\"out-cubic\",\"fade\":1,\"offsetY\":35,\"scale\":0.92}");

    draw::text("TRAVELLING GLYPH WAVE", 56, 245, 14, 0x9eabbd);
    txt::wave("TIDAL SIGNAL", 58, 286, 42, 0x6fc6b6, t, 10, 2.4f, 0.16f,
        "{\"size\":42,\"face\":\"bold\",\"spacing\":2}");
    draw::text("HIGHLIGHTED RANGE", 56, 380, 14, 0x9eabbd);
    txt::highlight("A map of the vanished coast", 58, 425, 34, 0xdfe4eb, 10, 18, 0xe7ab59, 0.9f, -8, 1.08f,
        "{\"size\":34,\"face\":\"ui\",\"width\":650}");

    draw::text("TEXT MORPH", 770, 245, 14, 0x9eabbd);
    txt::morph("NO SIGNAL", "STILL HERE", 770, 318, 39, 0xd7707a, float(frame) / 90.0f,
        "{\"size\":39,\"face\":\"bold\",\"spacing\":1}", 0, 0, "inOut-cubic");
    draw::text("LINE REVEAL / SCRAMBLE", 770, 390, 14, 0x9eabbd);
    txt::scrambleStyled("Something crossed the threshold.", 770, 435, 26, 0xdfe4eb, t,
        "{\"size\":26,\"face\":\"ui\",\"width\":420}",
        "{\"by\":\"line\",\"order\":\"backward\",\"stagger\":7,\"duration\":20,\"offsetY\":24,\"ease\":\"out-back\"}");
}
