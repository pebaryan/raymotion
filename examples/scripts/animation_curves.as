void configure(Composition &inout c) {
    c.name = "animation-curves";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11151e;
    motionBlur(8, 0.5f);
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    double sample = frameTime(frame);
    draw::text("ANIMATION CURVES + COLOR INTERPOLATION", 44, 38, 22, 0xdfe4eb);
    string[] names = {"in-quad", "out-cubic", "inOut-sine", "back", "elastic"};
    for (int i = 0; i < int(names.length()); ++i) {
        float t = float(sample) / 90.0f;
        float eased = float(anim::curve(names[i], t, 1.5));
        draw::text(names[i], 54, 120 + i * 88, 17, 0xaeb9c9);
        draw::roundRect(270, 112 + i * 88, 700, 20, 10, 0x293341);
        draw::roundRect(270, 112 + i * 88, 700 * clamp01(eased), 20, 10, 0x6fc6b6);
    }
    float bez = float(anim::bezier(float(sample) / 90.0f, 0.22, 0.75, 0.35, 1));
    array<float> range = {0, 1};
    array<uint> colors = {0xd7707a, 0xe7ab59};
    array<float> rgba;
    anim::interpolateColors(bez, range, colors, rgba);
    uint mixed = (uint(rgba[0]) << 16) | (uint(rgba[1]) << 8) | uint(rgba[2]);
    draw::text("BUILT-IN BEZIER COLOR", 54, 560, 17, 0xaeb9c9);
    draw::roundRect(270, 550, 700, 24, 10, mixed);
    anim::interpolateColorsFunction(bez, range, colors, "colorEase", rgba);
    array<float> rgbaKeys = {215, 112, 122, 255, 231, 171, 89, 255};
    anim::interpolateColorsFunction(bez, range, rgbaKeys, "colorEase", rgba);
    uint customMixed = (uint(rgba[0]) << 16) | (uint(rgba[1]) << 8) | uint(rgba[2]);
    draw::text("SCRIPT EASING COLOR (RGB + RGBA)", 54, 600, 17, 0xaeb9c9);
    draw::roundRect(360, 590, 610, 24, 10, customMixed);
    double springValue = anim::spring(sample, 0, 1, 1, 110, 12, 30, 0, 60, false);
    int settleFrames = anim::measureSpring(30, 1, 110, 12, 0.005);
    draw::text("SPRING / SUBFRAME SAMPLED   SETTLES BY FRAME " + settleFrames, 54, 650, 16, 0xdfe4eb);
    draw::roundRect(650, 644, 360, 20, 10, 0x293341);
    draw::roundRect(650, 644, 360 * clamp01(float(springValue)), 20, 10, 0xd7707a);
}

double colorEase(double t) {
    return t * t * (3.0 - 2.0 * t);
}
