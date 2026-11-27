// Barn shafts -- A dark barn with one high window: the sun comes through it as shafts of dusty air that cross the floor, drifting as the sun moves.
// Category: 3D

// Light you can see needs something in the air to scatter it. The barn is full of dust: a thin exponential
// fog, and `volumetric` marches every view ray through it, asking the sun's shadow map at each step whether that
// bit of air is lit. Only the air behind the window opening is, so the shaft has the window's shape,
// tapers as the sun's angle dictates and ends where it meets the floor.
//  - `density` is the dust (per metre); `ambient` is how much of the dust's brightness it keeps out of the
//    sun (low: the barn is dark); `anisotropy` makes the air brighter looking toward the light.
//  - The sun is one directional light with a shadow map; it moves a little over the shot.

string box(float x, float y, float z, float w, float h, float d, string color, float rough) {
    return "{\"geometry\":\"box\",\"width\":" + w + ",\"height\":" + h + ",\"depth\":" + d + ",\"position\":[" + x + "," + y + "," + z + "],"
           "\"material\":{\"color\":\"" + color + "\",\"roughness\":" + rough + "}}";
}

void configure(Composition &inout c) {
    c.name = "barn-shafts";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 180;
    c.background = 0xcfd9e8;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float t = frame / 30.0f;
    float u = t / 6.0f;
    // The sun lowers and swings a little: where the window's shaft lands moves across the floor.
    float sx = -0.80f + 0.10f * u, sy = 0.62f - 0.07f * u, sz = -0.22f + 0.12f * u;
    string cam = "{\"position\":[" + (4.5f - 0.8f * u) + ",2.4,10.5],\"target\":[" + (-1.5f - 0.5f * u) + ",3.0,-4],\"fov\":62}";
    string wood = "#4a3524", plank = "#5b4630", floorc = "#3b3026";
    string objs =
        // floor, roof and the end walls
        box(0, -0.2f, 0, 17, 0.4f, 24, floorc, 1) + "," +
        box(0, 8.2f, 0, 17, 0.4f, 24, wood, 1) + "," +
        box(0, 4, -12.2f, 17, 8, 0.4f, plank, 1) + "," +
        box(0, 4, 12.2f, 17, 8, 0.4f, plank, 1) + "," +
        // the right wall, whole
        box(8.4f, 4, 0, 0.4f, 8, 24, plank, 1) + "," +
        // the left wall around a window: 4 m wide (z -3..1), 2.4 m high (y 4.6..7)
        box(-8.4f, 2.3f, 0, 0.4f, 4.6f, 24, plank, 1) + "," +
        box(-8.4f, 7.5f, 0, 0.4f, 1.0f, 24, plank, 1) + "," +
        box(-8.4f, 5.8f, -7.5f, 0.4f, 2.4f, 9, plank, 1) + "," +
        box(-8.4f, 5.8f, 6.5f, 0.4f, 2.4f, 11, plank, 1) + "," +
        // a mullion down the middle of the window
        box(-8.4f, 5.8f, -1, 0.3f, 2.4f, 0.18f, wood, 1) + "," +
        // hay and crates for the light to land on
        box(-2.5f, 0.6f, -1.5f, 2.4f, 1.2f, 1.2f, "#a58a45", 1) + "," +
        box(0.6f, 1.0f, -3.0f, 2.2f, 2.0f, 2.2f, "#9a7f3e", 1) + "," +
        box(-1.0f, 0.4f, 3.0f, 1.2f, 0.8f, 1.2f, "#6b4d2c", 0.9f) + "," +
        box(3.8f, 0.7f, -7.0f, 2.8f, 1.4f, 2.0f, "#a58a45", 1) + "," +
        box(5.4f, 1.6f, 1.0f, 0.8f, 3.2f, 0.8f, wood, 1);
    string world = "{\"camera\":" + cam + ",\"ambient\":0.07,\"background\":\"#cfd9e8\",\"skyColor\":\"#10121a\",\"groundColor\":\"#0c0a08\","
                   "\"shadowMapSize\":4096,"
                   "\"fogMode\":\"exponential\",\"fogDensity\":0.055,\"fogColor\":\"#fff0cc\","
                   "\"volumetric\":{\"quality\":\"high\",\"anisotropy\":0.6,\"ambient\":0.02},"
                   "\"lights\":[{\"type\":\"directional\",\"position\":[" + (sx * 40) + "," + (sy * 40) + "," + (sz * 40) + "],\"target\":[0,2,0],\"color\":\"#ffe4b0\","
                   "\"intensity\":3.0,\"castShadow\":true,\"shadowExtent\":22}],"
                   "\"objects\":[" + objs + "]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
}
