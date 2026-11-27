// Lightning 2D -- Three strikes over a night skyline, each its own jagged bolt with branches, a glow and a screen flash.
// Category: Physics and particles

// `fx::draw` takes a particle emitter, or an object with a "lightning" key: one strike, drawn from the same generator as
// the 3D bolts. It is a pure function of the frame: a leader that grows the channel in about 35 ms, a return stroke held
// for at least `minVisible` (so a 30 fps film always catches it), two to four restrikes that flicker over a few tenths
// of a second, then nothing. Keys: from, to ([x, y] px), seed (its shape and timing), start (the frame it begins),
// width (px, the hot core), glow, color, intensity, flash (0..1: how much the whole picture lights up).

void configure(Composition &inout c) {
    c.name = "lightning-2d";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 150;
    c.background = 0x070a14;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    // A skyline: boxes of fixed heights.
    for (int i = 0; i < 18; ++i) {
        float h = 90 + float((i * 53) % 7) * 34;
        draw::rect(i * 72, 720 - h, 70, h, 0x0c1019);
    }
    draw::rect(0, 690, 1280, 30, 0x090c14);
    // Three strikes at frames 20, 62 and 104; the second is a cloud-to-cloud one that stays high.
    fx::draw("{\"lightning\":{\"from\":[430,-20],\"to\":[470,610],\"seed\":4,\"start\":20,\"width\":3,\"flash\":0.35}}", frame);
    fx::draw("{\"lightning\":{\"from\":[880,-20],\"to\":[620,250],\"seed\":9,\"start\":62,\"width\":2.5,\"flash\":0.2,\"color\":\"#c8b8ff\"}}", frame);
    fx::draw("{\"lightning\":{\"from\":[1090,-20],\"to\":[1060,640],\"seed\":15,\"start\":104,\"width\":3.5,\"glow\":1.3,\"flash\":0.4}}", frame);
    draw::text("LIGHTNING", 40, 40, 22, 0xe8e2d8);
    draw::text("one generator: jagged channel, branches, hot core and glow, restrikes, a screen flash", 40, 72, 13, 0x9ba4ae);
}
