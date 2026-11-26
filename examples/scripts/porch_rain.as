// Porch rain -- Rain builds from a drizzle to a downpour on a dusk porch: it stays dry under the roof, splashes on the path, and the drops glint in the lamp light.
// Category: 3D

// The rain is the scene's `precipitation`: GPU drops in a volume that follows the camera, each one a
// pure function of its place and the frame, so nothing is simulated. The kind, the intensity and
// the wind come from the scene's `weather`, which here builds from a drizzle (0.06) to a downpour
// over the clip, with the wind rising late and slanting the rain towards +x (the right of the frame).
//
// What to look for:
//  - the porch roof keeps the rain off what is under it (a depth map from above, along the fall);
//    as the wind slants the rain the dry strip narrows on the downwind side, as it would;
//  - splashes where the drops land on the stone path and the grass bed;
//  - streaks as long as a 1/60 s shutter, brighter where a drop lies between the camera and the lamp
//    on the post, or the low sun behind the house;
//  - the drops are fogged by the weather's haze.

string box(float x, float y, float z, float w, float h, float d, string color) {
    return "{\"geometry\":\"box\",\"width\":" + w + ",\"height\":" + h + ",\"depth\":" + d + ",\"position\":[" + x + "," + y + "," + z + "],"
           "\"material\":{\"color\":\"" + color + "\",\"roughness\":0.95}}";
}

string glow(float x, float y, float z, float r, string color) {
    return "{\"geometry\":\"sphere\",\"radius\":" + r + ",\"position\":[" + x + "," + y + "," + z + "],\"material\":{\"color\":\"" + color + "\",\"unlit\":true}}";
}

// The grass mask over x -9..9, z -2..12 in 0.25 m cells: none on the porch or on the path.
string maskGrid() {
    const int W = 73, H = 57;
    string values = "";
    for (int j = 0; j < H; ++j)
        for (int i = 0; i < W; ++i) {
            float x = -9 + 18 * i / float(W - 1), z = -2 + 14 * j / float(H - 1);
            bool porch = z < 1.7f && x > -2.8f && x < 2.8f;
            bool path = z >= 1.5f && x > -0.95f && x < 0.95f;
            if (values.length() > 0) values += ",";
            values += (porch || path) ? "0" : "1";
        }
    return "{\"width\":" + W + ",\"height\":" + H + ",\"values\":[" + values + "]}";
}

void configure(Composition &inout c) {
    c.name = "porch-rain";
    c.w = 1920; c.h = 1080; c.fps = 30; c.duration = 300;
    c.background = 0x0c0e14;
    scene(0, c.duration, "draw");
}

float smooth(float u) { u = u < 0 ? 0 : (u > 1 ? 1 : u); return u * u * (3 - 2 * u); }

void draw(int frame) {
    float u = frame / 299.0f;
    float intensity = 0.06f + 0.94f * smooth(u);
    float speed = 0.4f + 9.6f * smooth((u - 0.4f) / 0.6f);
    string weather = "{\"preset\":\"rain\",\"intensity\":" + intensity + ",\"windDirection\":0,\"windSpeed\":" + speed + ",\"gust\":0.2,\"cover\":0.9,\"haze\":0.3}";
    string cam = "{\"position\":[-1.2," + 1.55f + "," + (9.6f - 0.9f * u) + "],\"target\":[0.4,1.45,0],\"fov\":48}";
    string path = "";
    for (int i = 0; i < 11; ++i) {
        if (i > 0) path += ",";
        path += box(0.04f * ((i % 3) - 1), 0.03f, 2.1f + i * 1.0f, 1.7f, 0.06f, 0.88f, (i % 2 == 0) ? "#7c766e" : "#6f6a63");
    }
    string world = "{\"fps\":30,\"camera\":" + cam + ",\"ambient\":0.28,\"shadowMapSize\":2048,\"weather\":" + weather + ","
        "\"skyColor\":\"#6f7fa3\",\"groundColor\":\"#2a2e24\","
        "\"sky\":{\"enabled\":true,\"sunLight\":0,\"sunDirection\":[-0.57,0.09,-0.8],\"sunGlow\":1.1,\"clouds\":0.3,\"seed\":5,\"show\":true,"
        "\"zenith\":\"#2c3b5e\",\"horizon\":\"#c98f6a\"},"
        "\"precipitation\":{\"splashes\":true,\"occlusion\":true,\"radius\":30,\"farRadius\":160,\"seed\":7},"
        "\"lights\":["
        "{\"type\":\"directional\",\"position\":[-10,1.8,-14],\"target\":[0,0,0],\"color\":\"#ffb98a\",\"intensity\":2.4},"
        "{\"type\":\"point\",\"position\":[3.2,2.6,3.2],\"color\":\"#ffb766\",\"intensity\":3.2,\"range\":11},"
        "{\"type\":\"point\",\"position\":[0,2.45,0.2],\"color\":\"#ffd9a0\",\"intensity\":0.9,\"range\":5.5}],"
        "\"objects\":["
        "{\"geometry\":\"plane\",\"width\":300,\"depth\":300,\"material\":{\"color\":\"#2b3225\",\"roughness\":1}},"
        + box(0, 1.75f, -4.5f, 12, 3.5f, 5, "#948672") + ","
        + box(0, 3.65f, -4.5f, 13.4f, 0.3f, 6.4f, "#42352f") + ","
        + box(0, 0.1f, -0.3f, 5.2f, 0.2f, 3.4f, "#6b6055") + ","
        + box(0, 2.78f, -0.2f, 5.6f, 0.16f, 3.8f, "#4a3b33") + ","
        + box(-2.45f, 1.4f, 1.45f, 0.14f, 2.8f, 0.14f, "#d8d0c0") + ","
        + box(2.45f, 1.4f, 1.45f, 0.14f, 2.8f, 0.14f, "#d8d0c0") + ","
        + box(0, 1.3f, -1.96f, 1.1f, 2.2f, 0.08f, "#6a2e22") + ","
        "{\"geometry\":\"box\",\"width\":1.5,\"height\":1.2,\"depth\":0.06,\"position\":[-4,1.8,-1.97],\"material\":{\"color\":\"#ffbd6a\",\"unlit\":true}},"
        "{\"geometry\":\"box\",\"width\":1.5,\"height\":1.2,\"depth\":0.06,\"position\":[4,1.8,-1.97],\"material\":{\"color\":\"#ffbd6a\",\"unlit\":true}},"
        + box(3.2f, 1.3f, 3.2f, 0.09f, 2.6f, 0.09f, "#1e1e22") + ","
        + glow(3.2f, 2.68f, 3.2f, 0.15f, "#ffd9a0") + ","
        + glow(0, 2.58f, 0.2f, 0.09f, "#ffe3b8") + ","
        + path + ","
        "{\"grass\":{\"preset\":\"meadow\",\"min\":[-9,-2],\"max\":[9,12],\"density\":240,\"seed\":4,\"height\":0.35,\"fadeEnd\":45,\"shadowDistance\":16,\"mask\":" + maskGrid() + "}}"
        "]}";
    g3d::draw(world, frame, 0, 0, 1920, 1080);
    draw::text("RAIN ON A PORCH", 60, 990, 28, 0xf4ead7);
    draw::text("drizzle to downpour  -  dry under the roof  -  splashes on the path  -  glints in the lamp", 60, 1030, 20, 0xd8d0c0);
}
