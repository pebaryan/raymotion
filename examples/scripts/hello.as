// hello.as -- a runtime composition: edit and rerun, no rebuild.
//
//   raymotion_script list               --script examples/scripts/hello.as
//   raymotion_script preview hello-script --script examples/scripts/hello.as
//   raymotion_script render hello-script  --script examples/scripts/hello.as --props '{"title":"Hi"}'
//
// Global variables are disabled (frames must not accumulate state), so the palette lives in
// functions and every value is computed from the frame.

uint paper() { return 0x101820; }
uint ink()   { return 0xf2f4ff; }
uint teal()  { return 0x47dad8; }
uint coral() { return 0xff6b5a; }

void configure(Composition &inout c) {
    c.name = "hello-script";
    c.w = 1280; c.h = 720; c.fps = 30;
    c.duration = 150;
    c.background = paper();
    scene(0, 90, "title");          // local frames 0..89
    scene(75, 75, "dots");          // overlaps the title by 15 frames
}

// 0 -> 1 over `length` frames starting at `from`, eased.
double reveal(int t, int from, int length) {
    return ease(E_OUT_CUBIC, clamp01(double(t - from) / length));
}

void title(int t) {
    double enter = reveal(t, 0, 24), leave = 1 - reveal(t, 75, 15);
    draw::rect(140, 300, float(1000 * enter), 6, teal(), float(leave));
    draw::text(props::text("title", "Hello from a script"), 140, float(280 - 20 * (1 - enter)), 72, ink(),
               float(enter * leave), 0, 1);
    draw::text("edit hello.as and run it again", 140, 340, 30, ink(), float(0.6 * reveal(t, 12, 20) * leave));
}

void dots(int t) {
    int count = int(props::number("dots", 24));
    for (int i = 0; i < count; i++) {
        // Seeded randomness: the same dot lands in the same place on every frame and render.
        double x = rrange(7, i, 120, 1160), y = rrange(11, i, 120, 600);
        double k = reveal(t, i * 2, 18);
        float r = float(10 + 18 * rnd01(13, i)) * float(k);
        draw::circle(float(x), float(y + 10 * sin(t * 0.08 + i)), r, i % 3 == 0 ? coral() : teal(), 0.85f);
    }
    draw::roundRect(440, 320, 400, 80, 40, ink(), float(reveal(t, 30, 20)));
    draw::text("video as code", 640, 360, 34, paper(), float(reveal(t, 36, 20)), 0.5f, 0.5f);
}
