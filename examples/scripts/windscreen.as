// Windscreen -- A night drive in the rain, seen through a windscreen: drops refract the street lights, run down, and a wiper sweeps them away.
// Category: 3D

// The road is a 3D scene: a wet road, street lights, tail lights ahead, oncoming headlights. The rain on
// the glass is a camera filter (`layerFilter`, type "lens") over the scene's whole image, so it works the
// same over any 2D composition. What to look for:
//  - the picture behind the glass is out of focus (blurBehind), so the lights are soft discs of bokeh, while
//    every drop shows the road sharp, flipped and shrunk: a tiny upside-down street inside each one;
//  - big drops start to slide, at a speed that depends on their size, and leave a trail of tiny drops;
//  - the wiper (a dark arm and blade, pivot below the frame) sweeps every 3.2 s and clears what it passes;
//    the glass then fills with new drops;
//  - the rate of the drops follows the scene's rain: it builds from a drizzle to a downpour over the clip.
// The drops are not simulated: each is a pure function of its place and the frame, so any frame renders alone.

string box(float x, float y, float z, float w, float h, float d, string color) {
    return "{\"geometry\":\"box\",\"width\":" + w + ",\"height\":" + h + ",\"depth\":" + d + ",\"position\":[" + x + "," + y + "," + z + "],"
           "\"material\":{\"color\":\"" + color + "\",\"roughness\":0.9}}";
}

string lamp(float x, float y, float z, float r, string color) {
    return "{\"geometry\":\"sphere\",\"radius\":" + r + ",\"position\":[" + x + "," + y + "," + z + "],\"material\":{\"color\":\"" + color + "\",\"unlit\":true}}";
}

string weatherJson() {
    // Drizzle at the start, a downpour at the end; the lens follows the same numbers.
    return "{\"preset\":\"rain\",\"precipitation\":\"rain\",\"intensity\":0.3,\"windDirection\":0,\"windSpeed\":3,\"cover\":0.0,\"haze\":0.0,"
           "\"history\":[{\"seconds\":0,\"kind\":\"rain\",\"intensity\":0.25},{\"seconds\":10,\"kind\":\"rain\",\"intensity\":1.0}]}";
}

void configure(Composition &inout c) {
    c.name = "windscreen";
    c.w = 1920; c.h = 1080; c.fps = 30; c.duration = 300;
    c.background = 0x05070c;
    scene(0, c.duration, "drive");
    layerFilter(0, "{\"type\":\"lens\","
        "\"rain\":{\"size\":1.0,\"slide\":1,\"blurBehind\":13,\"seed\":6,\"weather\":" + weatherJson() + ",\"gain\":1.15},"
        "\"wiper\":{\"period\":3.2,\"pivot\":[0.5,1.2],\"length\":1.4,\"from\":-62,\"to\":62,\"bladeWidth\":0.02}}");
}

void drive(int frame) {
    float z = -9.0f * frame / 30.0f;                         // 9 m/s
    string objects = "{\"geometry\":\"plane\",\"width\":400,\"depth\":600,\"position\":[0,0,-250],\"material\":{\"color\":\"#15171c\",\"roughness\":0.3,\"metallic\":0.0}}";
    // dashed centre line
    for (int i = 0; i < 40; ++i) objects += "," + box(0, 0.01f, -4.0f - i * 7.0f, 0.18f, 0.02f, 3.2f, "#8a8678");
    // street lights on both sides, and the kerb
    for (int i = 0; i < 14; ++i) {
        float lz = -6.0f - i * 21.0f;
        objects += "," + box(-7.2f, 4.2f, lz, 0.18f, 8.4f, 0.18f, "#1c1e23") + "," + lamp(-6.4f, 8.2f, lz, 0.5f, "#ffc982");
        objects += "," + box(7.2f, 4.2f, lz - 10.5f, 0.18f, 8.4f, 0.18f, "#1c1e23") + "," + lamp(6.4f, 8.2f, lz - 10.5f, 0.5f, "#ffd9a0");
    }
    // a car ahead (red tail lights) and an oncoming one (white headlights)
    objects += "," + box(-1.7f, 0.8f, z - 34.0f, 1.8f, 1.2f, 4.2f, "#20242c") + "," + lamp(-2.35f, 0.75f, z - 31.8f, 0.2f, "#ff2a1a") + "," + lamp(-1.05f, 0.75f, z - 31.8f, 0.2f, "#ff2a1a");
    objects += "," + box(2.0f, 0.8f, -78, 1.8f, 1.2f, 4.2f, "#20242c") + "," + lamp(1.4f, 0.65f, -75.8f, 0.24f, "#e8f0ff") + "," + lamp(2.6f, 0.65f, -75.8f, 0.24f, "#e8f0ff");
    for (int i = 0; i < 6; ++i) objects += "," + lamp(-12.0f + (i % 2) * 24.0f, 2.2f, -30.0f - i * 40.0f, 0.4f, (i % 3 == 0) ? "#7fd0ff" : "#ff9a52");
    string cam = "{\"position\":[-1.7," + 1.25f + "," + (z + 4.0f) + "],\"target\":[-1.2," + 1.2f + "," + (z - 40.0f) + "],\"fov\":58}";
    string world = "{\"fps\":30,\"camera\":" + cam + ",\"ambient\":0.07,\"skyColor\":\"#0a0f1a\",\"groundColor\":\"#101216\","
        "\"weather\":" + weatherJson() + ","
        "\"sky\":{\"enabled\":true,\"sunLight\":0,\"sunGlow\":0,\"clouds\":0,\"show\":true,\"zenith\":\"#0a1020\",\"horizon\":\"#5a4636\",\"ground\":\"#0a0c10\",\"haze\":\"#3a3040\",\"hazeAmount\":0.7},"
        "\"ground\":{\"puddleSize\":2.2,\"puddles\":0.7,\"seed\":2},"
        "\"lights\":[{\"type\":\"point\",\"position\":[-6.4,8.2," + (z - 6.0f) + "],\"color\":\"#ffc982\",\"intensity\":6,\"range\":40},"
        "{\"type\":\"point\",\"position\":[6.4,8.2," + (z - 16.5f) + "],\"color\":\"#ffd9a0\",\"intensity\":5,\"range\":40}],"
        "\"objects\":[" + objects + "]}";
    g3d::draw(world, frame, 0, 0, 1920, 1080);
}
