void configure(Composition &inout c) {
    c.name = "generated-assets";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 30;
    c.background = 0x11131b;
    scene(0, c.duration, "draw");
}

void assets() {
    assets::writeText("out/script-assets/moon.svg", "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"360\" height=\"360\" viewBox=\"0 0 360 360\"><rect width=\"360\" height=\"360\" rx=\"180\" fill=\"#202b46\"/><circle cx=\"160\" cy=\"160\" r=\"94\" fill=\"#f4c66a\"/><circle cx=\"202\" cy=\"128\" r=\"94\" fill=\"#202b46\"/><path d=\"M80 282 Q180 220 280 282\" fill=\"none\" stroke=\"#80c8c0\" stroke-width=\"8\" stroke-linecap=\"round\"/></svg>");
    array<uint> swatches = { 0x13213dff, 0x344b75ff, 0xe6ab57ff, 0x9bd1c3ff };
    assets::writeImage("out/script-assets/swatches.png", 2, 2, swatches);
}

void draw(int frame) {
    draw::image("out/script-assets/moon.svg", 180, 150, 360, 360);
    draw::image("out/script-assets/swatches.png", 740, 215, 300, 300);
    draw::text("ASSETS GENERATED BEFORE BUILD", 640, 620, 23, 0xe8e2d8, 1, 0.5f);
    draw::text("SVG source • RGBA pixel grid", 640, 654, 15, 0x9ba4ae, 1, 0.5f);
}
