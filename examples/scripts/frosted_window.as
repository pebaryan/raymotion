// Frosted window -- A winter evening seen through a window: frost creeps in from the edges and corners, with a patch wiped clear by a hand.
// Category: Drawing

// The evening is a plain 2D drawing (sky, hills, a house with lit windows, a lamp post). The frost is one
// camera filter on its layer: `layerFilter` with type "lens" and a "frost" object.
//  - `amount` creeps from 0 to its value between `startFrame` and `endFrame`, spreading from the edges and
//    corners inwards (`fromEdges`), as a branching ice texture that scatters, whitens and blurs what is behind;
//  - `clears` wipes a circular patch (x, y, radius in px) with a ragged edge, and the view through it stays sharp;
//  - the frost is a pure function of the frame and its seed, so any frame renders alone.
// Try "kind": "condensation" for a fog with streaks where drops ran, and add a "rain" object to the filter.

void configure(Composition &inout c) {
    c.name = "frosted-window";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 240;
    c.background = 0x0a1020;
    scene(0, c.duration, "evening");
    layerFilter(0, "{\"type\":\"lens\",\"frost\":{\"amount\":0.95,\"fromEdges\":true,\"kind\":\"frost\",\"startFrame\":10,\"endFrame\":200,\"seed\":3,"
                   "\"clears\":[[760,430,120]]}}");
}

void evening(int frame) {
    // sky: bands from deep blue to a warm horizon
    for (int i = 0; i < 24; ++i) {
        float k = i / 23.0f;
        int r = int(16 + 120 * k * k), g = int(26 + 70 * k * k), b = int(60 + 40 * k);
        draw::rect(0, i * 18, 1280, 19, (r << 16) | (g << 8) | b);
    }
    // stars
    for (int i = 0; i < 40; ++i) draw::circle((i * 331) % 1280, (i * 97) % 260, 1.4f, 0xdfe8ff);
    // hills
    draw::circle(300, 900, 560, 0x1b2a3f);
    draw::circle(1000, 960, 600, 0x16233a);
    // snow field
    draw::rect(0, 520, 1280, 200, 0xc9d6e6);
    // house
    draw::rect(420, 330, 360, 220, 0x5a3a35);
    draw::rect(400, 300, 400, 40, 0x3a2724);
    draw::rect(470, 380, 70, 80, 0xffc46b);
    draw::rect(660, 380, 70, 80, 0xffd98a);
    draw::rect(572, 440, 56, 110, 0x2b1d1b);
    // lamp post with a glow
    draw::rect(960, 330, 8, 220, 0x20242c);
    draw::circle(964, 326, 70, 0x4a3b2a);
    draw::circle(964, 326, 26, 0xffd9a0);
    draw::circle(964, 326, 11, 0xfff2d0);
    // a pine
    draw::rect(188, 470, 14, 80, 0x2a1d17);
    for (int i = 0; i < 4; ++i) draw::circle(195, 430 - i * 38, 70 - i * 14, 0x1d3a2e);
}
