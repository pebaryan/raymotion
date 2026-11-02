string pathWarp(double x, double y) {
    return "[" + (x + 12.0 * sin(y / 35.0)) + "," + (y + 8.0 * sin(x / 28.0)) + "]";
}

void configure(Composition &inout c) {
    c.name = "path-authoring";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11131b;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    array<float> controls = { 0, 70, 45, 6, 92, 0, 140, 18, 190, 0, 235, 54, 275, 8 };
    Path@ wave = vg::smooth(controls, false);
    Path@ star = vg::regular(0, 0, 105, 7, -90);
    Path@ shifted = star.translated(640, 365).mapped("pathWarp");
    PathMorph@ morph = vg::morph(shifted, wave.translated(500, 350), 96);
    float progress = float(frame) / 90.0f;
    Path@ shape = morph.at(progress).scaled(2.1f, 2.1f, 640, 365);
    array<float> bounds;
    shape.bounds(bounds);
    array<float> flattened;
    array<int> contourRanges;
    shape.flatten(0.5f, flattened, contourRanges);
    array<float> dash = { 10, 5, 2, 5 };
    vg::strokeDashed(shape, 0xf1b75b, 3, dash, -frame * 0.6f, 0.9f, vg::JOIN_ROUND, vg::CAP_ROUND, 4);
    Path@ trimmed = shape.trim(0.05f, 0.38f);
    vg::stroke(trimmed, 0x7fd4c6, 5, 0.9f);
    draw::text("PATH MORPH • SMOOTH • DASH • TRIM", 640, 70, 22, 0xe8e2d8, 1, 0.5f);
    draw::text("BOUNDS: " + bounds[0] + ", " + bounds[1] + "  SIZE " + bounds[2] + " × " + bounds[3] + "  FLAT PTS: " + flattened.length() / 2, 640, 635, 15, 0x9ba4ae, 1, 0.5f);
}
