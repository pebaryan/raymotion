// Sunset sea -- A spectrum sea at dusk meets a far headland and the low sun: aerial perspective hazes the land into the sky's horizon colour, and the sea's own haze takes over at the waterline without a seam.
// Category: 3D

// The seam test case of Scene::aerial: the land is in the depth buffer and gets the aerial haze; the sea is not, so it keeps W9's
// haze, in the same horizon colour. A submerged shelf of the terrain (below the waterline) must not show through the sea.

float hAt(float x, float z) {
    float dx = x / 5500.0f, dz = (z + 12500.0f) / 2500.0f;
    float h = 900.0f * pow(0.36788f, dx * dx + dz * dz) * (0.7f + 0.3f * sin(x * 0.0021f) * sin(x * 0.0047f + 1.0f));
    float dx2 = (x - 7500.0f) / 2500.0f, dz2 = (z + 22000.0f) / 3200.0f;
    h += 500.0f * pow(0.36788f, dx2 * dx2 + dz2 * dz2);
    float ex = abs(x) / 10000.0f, ez = abs(z + 13000.0f) / 9000.0f;
    float ed = ex > ez ? ex : ez;
    return h - 260.0f * ed * ed * ed * ed - 60.0f;
}
string grid() {
    const int N = 80;
    string v = "";
    for (int j = 0; j < N; ++j)
        for (int i = 0; i < N; ++i) {
            float x = -10000.0f + 20000.0f * i / float(N - 1), z = -34000.0f + 30000.0f * j / float(N - 1);
            if (i + j > 0) v += ",";
            v += int(hAt(x, z));
        }
    return "{\"width\":" + N + ",\"height\":" + N + ",\"values\":[" + v + "]}";
}

void configure(Composition &inout c) {
    c.name = "sunset-sea";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 120;
    c.background = 0x101820;
    c.statementBudget = 4000000;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float drift = 0.6f * sin(frame * 0.02f);
    string cam = "{\"position\":[0,7,0],\"target\":[" + (200.0f + 700.0f * drift) + ",18,-1000],\"fov\":60,\"near\":2,\"far\":90000}";
    string world = "{\"fps\":30,\"camera\":" + cam + ",\"ambient\":0.35,"
        "\"sky\":{\"enabled\":true,\"zenith\":\"#35508a\",\"horizon\":\"#f4a878\",\"ground\":\"#40302a\",\"sunLight\":0,\"sunColor\":\"#ffcf9a\",\"sunSize\":2.4,\"sunGlow\":1.0,\"haze\":\"#ff9a60\",\"hazeAmount\":0.6,\"clouds\":0,\"show\":true,\"sunDirection\":[0.45,0.05,-1]},"
        "\"aerialPerspective\":{\"enabled\":true,\"scaleHeight\":2200,\"strength\":2.0},"
        "\"water\":{\"preset\":\"calm\",\"level\":0,\"fetch\":40000,\"openDepth\":40,\"foam\":0.6,"
        "\"spectrum\":{\"enabled\":true,\"cascades\":3,\"windSpeed\":7,\"seed\":3}},"
        "\"lights\":[{\"type\":\"directional\",\"position\":[4.5,0.5,-10],\"target\":[0,0,0],\"color\":\"#ffcf9a\",\"intensity\":2.4,\"castShadow\":false}],"
        "\"objects\":[{\"geometry\":\"terrain\",\"area\":{\"min\":[-10000,-34000],\"max\":[10000,-4000],\"ground\":" + grid() + "},\"resX\":159,\"resZ\":159,\"material\":{\"color\":\"#3d4a30\",\"roughness\":1}}]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
}
