// First snow -- Snow starts to fall on a cabin in a meadow and over twenty seconds it settles: on the roof, the ground, the fir tops and the fence rails, while the walls stay bare.
// Category: 3D

// The snow is the scene's `precipitation`, and what it leaves behind is the ground's state: the
// weather says how long it has been snowing, `accumulationScale` runs the ground's clock 500 times
// faster than the film's (a 20 s clip is 2 h 45 min of snowfall: about 7 cm at this intensity), and the
// lit shader turns that depth into cover. Nothing is stored from frame to frame: the depth at a frame
// is worked out from the weather and the time alone.
//
// What to look for:
//  - snow first lies in patches on the surfaces that face up, and joins into a sheet by about 5 cm;
//  - the walls, the underside of the eaves and the steep tiers of the firs keep their colour;
//  - the tips of the meadow grass whiten before the blades do;
//  - the porch roof is a shelter: the ground under it stays bare (the snow's depth map from above).

string box(float x, float y, float z, float w, float h, float d, string color) {
    return "{\"geometry\":\"box\",\"width\":" + w + ",\"height\":" + h + ",\"depth\":" + d + ",\"position\":[" + x + "," + y + "," + z + "],"
           "\"material\":{\"color\":\"" + color + "\",\"roughness\":0.95}}";
}

string slab(float x, float y, float z, float w, float h, float d, float tilt, string color) {
    return "{\"geometry\":\"box\",\"width\":" + w + ",\"height\":" + h + ",\"depth\":" + d + ",\"position\":[" + x + "," + y + "," + z + "],\"rotation\":[" + tilt + ",0,0],"
           "\"material\":{\"color\":\"" + color + "\",\"roughness\":0.9}}";
}

// A fir: a trunk and four tiers of cones, each about 36 degrees from the horizontal.
string fir(float x, float z, float s, string leaf) {
    string o = box(x, 0.5f * s, z, 0.28f * s, 1.0f * s, 0.28f * s, "#4a3626");
    for (int i = 0; i < 4; ++i) {
        float r = (1.7f - 0.36f * i) * s, h = 0.73f * r;
        float y = (1.0f + 0.78f * i) * s + 0.5f * h;
        o += ",{\"geometry\":\"cone\",\"radius\":" + r + ",\"height\":" + h + ",\"slices\":24,\"position\":[" + x + "," + y + "," + z + "],"
             "\"material\":{\"color\":\"" + leaf + "\",\"roughness\":1}}";
    }
    return o;
}

// The grass mask over x -16..16, z -8..16 in 0.5 m cells: none under the cabin and its path.
string maskGrid() {
    const int W = 65, H = 49;
    string values = "";
    for (int j = 0; j < H; ++j)
        for (int i = 0; i < W; ++i) {
            float x = -16 + 32 * i / float(W - 1), z = -8 + 24 * j / float(H - 1);
            bool cabin = x > -3.6f && x < 3.6f && z > -3.0f && z < 3.0f;
            bool path = x > -0.8f && x < 0.8f && z >= 2.0f;
            if (values.length() > 0) values += ",";
            values += (cabin || path) ? "0" : "1";
        }
    return "{\"width\":" + W + ",\"height\":" + H + ",\"values\":[" + values + "]}";
}

void configure(Composition &inout c) {
    c.name = "first-snow";
    c.w = 1920; c.h = 1080; c.fps = 30; c.duration = 600;
    c.background = 0x0c0e14;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float u = frame / 599.0f;
    string weather = "{\"preset\":\"snow\",\"intensity\":0.5,\"accumulationScale\":500,\"windDirection\":25,\"windSpeed\":1.5,\"gust\":0.1,\"cover\":0.92,\"haze\":0.3}";
    string cam = "{\"position\":[" + (-8.0f + 1.2f * u) + ",3.1,11.5],\"target\":[0.4,1.9,0],\"fov\":46}";
    string fence = "";
    for (int i = 0; i < 9; ++i) {
        float x = -9.0f + 2.0f * i;
        fence += box(x, 0.55f, 6.5f, 0.12f, 1.1f, 0.12f, "#5b4a3a") + ",";
        if (i < 8) fence += box(x + 1.0f, 0.95f, 6.58f, 2.0f, 0.1f, 0.14f, "#6a5846") + "," + box(x + 1.0f, 0.62f, 6.72f, 2.0f, 0.1f, 0.14f, "#6a5846") + ",";    // 14 cm rails, the lower one set forward so snow can reach it
    }
    string world = "{\"fps\":30,\"camera\":" + cam + ",\"ambient\":0.4,\"shadowMapSize\":2048,\"weather\":" + weather + ","
        "\"skyColor\":\"#8a98b0\",\"groundColor\":\"#3a3d40\","
        "\"sky\":{\"enabled\":true,\"sunLight\":0,\"sunDirection\":[-0.5,0.3,-0.8],\"sunGlow\":0.6,\"clouds\":0.6,\"seed\":9,\"show\":true,"
        "\"zenith\":\"#5b6a88\",\"horizon\":\"#c8c4c0\"},"
        "\"precipitation\":{\"splashes\":false,\"occlusion\":true,\"radius\":28,\"farRadius\":120,\"seed\":3},"
        "\"lights\":["
        "{\"type\":\"directional\",\"position\":[-8,9,-6],\"target\":[0,0,0],\"color\":\"#fff0dc\",\"intensity\":1.6,\"castShadow\":true},"
        "{\"type\":\"point\",\"position\":[-1.6,1.9,3.6],\"color\":\"#ffb766\",\"intensity\":1.4,\"range\":7}],"
        "\"objects\":["
        "{\"geometry\":\"plane\",\"width\":300,\"depth\":300,\"material\":{\"color\":\"#4d5236\",\"roughness\":1}},"
        + box(0, 1.3f, 0, 6, 2.6f, 4.4f, "#8a5a3c") + ","
        + slab(0, 3.4f, 1.28f, 6.8f, 0.14f, 3.05f, 32, "#4a3b33") + ","
        + slab(0, 3.4f, -1.28f, 6.8f, 0.14f, 3.05f, -32, "#4a3b33") + ","
        + box(1.8f, 4.0f, -0.6f, 0.7f, 1.6f, 0.7f, "#6e665e") + ","
        + box(0, 1.0f, 2.21f, 0.95f, 2.0f, 0.06f, "#3a2418") + ","
        "{\"geometry\":\"box\",\"width\":1.1,\"height\":0.9,\"depth\":0.06,\"position\":[-1.9,1.6,2.22],\"material\":{\"color\":\"#ffcf86\",\"unlit\":true,\"weathering\":false}},"
        "{\"geometry\":\"box\",\"width\":1.1,\"height\":0.9,\"depth\":0.06,\"position\":[1.9,1.6,2.22],\"material\":{\"color\":\"#ffcf86\",\"unlit\":true,\"weathering\":false}},"
        + box(0, 2.5f, 3.2f, 3.4f, 0.12f, 2.0f, "#4a3b33") + ","
        + box(-1.55f, 1.25f, 4.1f, 0.12f, 2.5f, 0.12f, "#d8d0c0") + ","
        + box(1.55f, 1.25f, 4.1f, 0.12f, 2.5f, 0.12f, "#d8d0c0") + ","
        + fir(-7.5f, -3.0f, 1.25f, "#244433") + ","
        + fir(-9.5f, 2.0f, 1.0f, "#2a4a38") + ","
        + fir(8.0f, -2.0f, 1.3f, "#22402f") + ","
        + fir(10.5f, 3.0f, 0.95f, "#2a4a38") + ","
        + fence +
        "{\"grass\":{\"preset\":\"meadow\",\"min\":[-16,-8],\"max\":[16,16],\"density\":260,\"seed\":4,\"height\":0.3,\"fadeEnd\":50,\"shadowDistance\":14,\"mask\":" + maskGrid() + "}}"
        "]}";
    g3d::draw(world, frame, 0, 0, 1920, 1080);
    draw::text("FIRST SNOW", 60, 990, 28, 0xf4ead7);
    draw::text("settling on the roof, the ground, the firs and the fence  -  the walls stay bare", 60, 1030, 20, 0xd8d0c0);
}
