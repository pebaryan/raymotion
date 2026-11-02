void configure(Composition &inout c) {
    c.name = "layer-card";
    c.w = 960; c.h = 540; c.fps = 30; c.duration = 60;
    c.background = 0x11131b;
    scene(0, 60, "drawScene");
    layer(0, 0.92f, 0, 0, "screen", "alpha", false, 3, 0.5f);
    layerFilter(0, "{\"type\":\"posterize\",\"levels\":5}");
    layerFilter(0, "{\"type\":\"paper\",\"size\":2,\"strength\":0.16}");
}

void drawScene(int frame) {
    float x = 480 + sin(frame * 0.08f) * 180;
    draw::circle(x, 240, 110, 0xb84a6d);
    draw::circle(480 - sin(frame * 0.08f) * 180, 240, 76, 0xe6ae57);
    draw::text("ISOLATED LAYER / FILTER STACK", 480, 470, 24, 0xf4ead7, 1, 0.5f);
}
