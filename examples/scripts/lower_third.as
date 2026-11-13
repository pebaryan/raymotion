// Lower third -- A complete name strap built from transforms, a mask and timed reveals.
// Category: Start here
//
// A small 2D composition using the quick-motion-graphics bindings.
void configure(Composition &inout c) {
    c.name = "lower-third";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 210;
    c.supersample = 1; c.background = 0x111820;
    c.statementBudget = 100000;
}

void draw(int t) {
    double reveal = ease(E_OUT_CUBIC, clamp01(double(t) / 26.0));
    draw::gradient(0, 0, 1280, 720, 0x101820, 0x1b2934, 0x26323b, 0x131a20);
    draw::push(float(-860.0 * (1.0 - reveal)), 0, 0, 1, float(reveal));
    draw::roundRect(72, 482, 910, 158, 18, 0x18232c, 0.96);
    draw::gradient(72, 482, 12, 158, 0xe4b06c, 0xc8794c, 0x8d4539, 0xd48a50);

    // The SVG is a tiny hand-made badge supplied with raymotion's source assets.
    draw::image("images/script-lower-third.svg", 102, 504, 112, 112, 1);

    array<float> notch = { 238, 500, 254, 516, 238, 532 };
    draw::fillPoly(notch, 0xe4b06c, 1);
    draw::text("FIELD NOTES", 280, 515, 17, 0xe4b06c, 1, 0, 0, draw::FONT_MONO_BOLD);
    draw::text(props::text("title", "A story in motion"), 280, 543, 34, 0xf5f0e6, 1, 0, 0, draw::FONT_UI_BOLD);
    draw::text(props::text("subtitle", "A concise line that earns its place."), 282, 593,
               19, 0xb9c5cc, 1, 0, 0, draw::FONT_UI);
    draw::pop();

    // A slim progress mark is derived from t, so any frame can be rendered directly.
    draw::roundRect(72, 657, 910, 3, 2, 0x33414a);
    draw::roundRect(72, 657, float(910.0 * clamp01(double(t) / 210.0)), 3, 2, 0xe4b06c);
}
