// Leaf cards -- Cut-out leaves lit from behind by a low sun, over a tinted instanced meadow.
// Category: 3D

// A leaf drawn into a 32 x 32 texture: green inside a pointed oval, clear outside, with a
// paler midrib. The material's alphaCutoff turns the clear texels into holes that also stay
// out of the shadows.
string leafPixels() {
    string pixels = "[";
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            float u = (x + 0.5f) / 16.0f - 1.0f, v = (y + 0.5f) / 16.0f - 1.0f;
            float halfWidth = 0.55f * (1.0f - v * v) * (1.0f - 0.35f * v);   // narrower towards the tip
            bool inside = abs(u) < halfWidth;
            bool rib = abs(u) < 0.05f && inside;
            if (pixels.length() > 1) pixels += ",";
            if (!inside) pixels += "[88,164,62,0]";      // clear, but leaf-coloured: filtering then blends no dark fringe in
            else if (rib) pixels += "[196,222,120,255]";
            else pixels += "[" + (88 + int(30 * v)) + "," + (164 + int(20 * u)) + ",62,255]";
        }
    return pixels + "]";
}

void configure(Composition &inout c) {
    c.name = "leaf-cards";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 120;
    c.background = 0x16131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float t = frame / 30.0f;
    string leaf = "{\"color\":\"#ffffff\",\"roughness\":0.6,\"doubleSided\":true,\"alphaCutoff\":0.5,"
                  "\"translucency\":0.9,\"translucencyColor\":\"#ffe08a\","
                  "\"dynamicMap\":{\"width\":32,\"height\":32,\"frames\":[" + leafPixels() + "]}}";
    string objects = "{\"geometry\":\"plane\",\"width\":40,\"depth\":40,\"material\":{\"color\":\"#4b5a34\",\"roughness\":1}}";
    // Leaves on a branch, each turning a little in the breeze (a function of t, not accumulated).
    for (int i = 0; i < 9; ++i) {
        float x = -1.6f + 0.4f * i, y = 1.5f + 0.25f * sin(i * 1.7f);
        float sway = 12 * sin(t * 1.3f + i * 0.8f);
        objects += ",{\"geometry\":\"plane\",\"width\":0.55,\"depth\":0.9,\"position\":[" + x + "," + y + ",0],"
                   "\"rotation\":[" + (80 + sway) + "," + (i * 23 % 40 - 20) + "," + (i % 2 == 0 ? 25 : -25) + "],"
                   "\"material\":" + leaf + "}";
    }
    // A meadow of tinted instances: one baked blade, many colours.
    objects += ",{\"geometry\":\"cone\",\"radius\":0.03,\"height\":0.5,\"slices\":4,"
               "\"material\":{\"color\":\"#ffffff\",\"roughness\":0.9},\"instances\":{\"items\":[";
    for (int i = 0; i < 400; ++i) {
        float a = i * 2.39996f, r = 0.35f * sqrt(float(i));
        float g = 0.5f + 0.5f * sin(i * 12.9898f);
        int red = 70 + int(60 * g), green = 120 + int(70 * (1 - g)), blue = 40;
        if (i > 0) objects += ",";
        objects += "{\"position\":[" + (r * cos(a)) + ",0.25," + (r * sin(a) - 2) + "],\"yaw\":" + (a * 57.3f) +
                   ",\"scale\":" + (0.7f + 0.6f * g) + ",\"tint\":[" + red + "," + green + "," + blue + "]}";
    }
    objects += "]}}";
    string world = "{\"camera\":{\"position\":[0,1.6,5],\"target\":[0,1.2,0],\"fov\":45},\"ambient\":0.25,"
                   "\"sky\":{\"enabled\":true,\"zenith\":\"#2a2f5e\",\"horizon\":\"#f0a060\",\"ground\":\"#2a2420\","
                   "\"sunDirection\":[0.06,0.34,-1],\"sunColor\":\"#ffd9a0\",\"sunGlow\":0.9,\"show\":true},"
                   "\"lights\":[{\"type\":\"directional\",\"position\":[0.6,3.6,-10],\"color\":\"#ffd9a0\",\"intensity\":2.2,"
                   "\"castShadow\":true,\"shadowExtent\":12}],"
                   "\"objects\":[" + objects + "]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
    draw::text("CUT-OUT LEAVES, LIT FROM BEHIND", 34, 650, 18, 0xf4ead7);
    draw::text("alphaCutoff + translucency + instance tint", 34, 676, 12, 0xc8c0b0);
}
