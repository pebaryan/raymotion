// Lake reflections -- A calm lake at dusk mirrors a jetty, a red boathouse, the far trees and the clouds; as the wind rises the mirror breaks up.
// Category: 3D

// The water reflects the scene (`reflections: "scene"`), so the boathouse and the cumulus show upside
// down in it. The reflection is bent by the same waves that roll the water: with the lake still it is a
// sharp copy, and as the wind rises the waves (their height limited by `fetch`, the 900 m of open
// water upwind) break it into streaks that smear and darken with distance.
//
// The weather is a function of the frame, so the clouds are given how far the wind has carried them
// ("windRun"): the integral of a speed that eases from 0.5 to 9 m/s over the shot.

string box(float x, float y, float z, float w, float h, float d, string color) {
    return "{\"geometry\":\"box\",\"width\":" + w + ",\"height\":" + h + ",\"depth\":" + d + ",\"position\":[" + x + "," + y + "," + z + "],"
           "\"material\":{\"color\":\"" + color + "\",\"roughness\":0.9}}";
}

string tree(float x, float z, float h) {
    return "{\"geometry\":\"cylinder\",\"radius\":0.35,\"height\":" + (h * 0.4f) + ",\"position\":[" + x + "," + (h * 0.2f) + "," + z + "],\"material\":{\"color\":\"#3a2a1c\",\"roughness\":1}},"
           "{\"geometry\":\"cone\",\"radius\":2.4,\"height\":" + h + ",\"position\":[" + x + "," + (h * 0.4f + h * 0.5f) + "," + z + "],\"material\":{\"color\":\"#1f3a2a\",\"roughness\":1}}";
}

void configure(Composition &inout c) {
    c.name = "lake-reflections";
    c.w = 1920; c.h = 1080; c.fps = 30; c.duration = 240;
    c.background = 0x101820;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float u = frame / 239.0f;
    float a = u * u * (3 - 2 * u);                                   // how far into the wind
    float speed = 0.5f + 8.5f * a;
    float run = 8.0f * (0.5f * a + 8.5f * (a * a * a - a * a * a * a / 2) / 1.0f);
    string weather = "{\"preset\":\"clear\",\"windSpeed\":" + speed + ",\"windDirection\":90,\"gust\":" + (0.1f + 0.5f * a) + ",\"cover\":0.35,\"cloudType\":\"cumulus\",\"windRun\":" + run + "}";
    string cam = "{\"position\":[-6,1.6,24],\"target\":[0,2.6,-30],\"fov\":46}";
    string trees = "";
    for (int i = 0; i < 26; ++i) {
        float x = -120 + i * 9.6f + 3.0f * sin(i * 2.7f);
        float h = 7 + 4 * (0.5f + 0.5f * sin(i * 1.9f));
        if (i > 0) trees += ",";
        trees += tree(x, -92 - 5 * (0.5f + 0.5f * cos(i * 3.1f)), h);
    }
    string world = "{\"fps\":30,\"camera\":" + cam + ",\"ambient\":0.3,\"shadowMapSize\":2048,\"weather\":" + weather + ","
        "\"sky\":{\"enabled\":true,\"sunLight\":0,\"sunGlow\":1.0,\"clouds\":0,\"seed\":3,\"show\":true,\"sunDirection\":[0.2,0.09,-1]},"
        "\"clouds\":{\"base\":900,\"top\":1900,\"quality\":\"film\"},"
        "\"water\":{\"preset\":\"calm\",\"level\":0,\"fetch\":900,\"openDepth\":6,\"foam\":0.2,\"reflections\":\"scene\",\"reflectionDistort\":1},"
        "\"lights\":[{\"type\":\"directional\",\"position\":[10,8,-30],\"target\":[0,0,-50],\"color\":\"#ffb074\",\"intensity\":2.2,\"castShadow\":false}],"
        "\"objects\":["
        + box(0, -1.5f, -150, 600, 3, 100, "#2b3524") + ","
        + box(0, -0.2f, -95, 600, 1.2f, 12, "#3a4a2c") + ","
        + box(6, 0.1f, 4, 2.0f, 0.18f, 26, "#5b4632") + ","
        + box(-18, 2.2f, -62, 11, 4.4f, 8, "#9a2a22") + ","
        + box(-18, 5.1f, -62, 12, 0.5f, 9, "#3b2a24") + ","
        + box(-24, 0.5f, -58, 8, 0.2f, 3, "#5b4632") + ","
        + trees + "]}";
    g3d::draw(world, frame, 0, 0, 1920, 1080);
    draw::text("ONE LAKE, ONE WIND", 60, 980, 30, 0xf4ead7);
    draw::text("wind " + int(speed + 0.5f) + " m/s  -  the reflection breaks as the waves grow", 60, 1020, 22, 0xd8d0c0);
}
