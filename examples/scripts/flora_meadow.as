// Flora meadow -- A million blades of grass at golden hour, a path mown through them, gusts crossing the field.
// Category: 3D

// The grass is a field made on the GPU: no blade is stored, so the area can be a landscape.
// The mask grid is 0 where the path runs (and thin at its edges), 1 elsewhere; the tall grid
// lets the grass by the path grow a little higher. (Scripts keep no state between frames,
// so the grids are rebuilt each frame; the engine keeps the field while they stay the same.)

float pathX(float z) { return 2.5f * sin(z * 0.07f) + 1.2f * sin(z * 0.19f + 1.0f); }

// "mask" or "tall" as a grid object.
string grid(bool mask) {
    const int GRID = 121;             // samples a side over the 120 m square: one a metre
    const float HALF = 60;
    string values = "";
    for (int j = 0; j < GRID; ++j)
        for (int i = 0; i < GRID; ++i) {
            float x = -HALF + 2 * HALF * i / (GRID - 1), z = -HALF + 2 * HALF * j / (GRID - 1);
            float d = abs(x - pathX(z));
            float m = d < 0.7f ? 0.0f : (d > 1.6f ? 1.0f : (d - 0.7f) / 0.9f);
            float h = 1.0f + 0.5f * (d > 1.6f && d < 4.0f ? 1.0f : 0.0f);
            if (values.length() > 0) values += ",";
            values += "" + (int((mask ? m : h) * 100) / 100.0f);
        }
    return "{\"width\":" + GRID + ",\"height\":" + GRID + ",\"values\":[" + values + "]}";
}

void configure(Composition &inout c) {
    c.name = "flora-meadow";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 240;
    c.background = 0x101018;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float t = frame / 30.0f;
    const float HALF = 60;
    // Walking up the path: the camera's position and aim are functions of t.
    float z = 8 - 0.9f * t;
    float ahead = z - 6;
    string cam = "{\"position\":[" + pathX(z) + ",1.45," + z + "],\"target\":[" + pathX(ahead) + ",1.0," + ahead + "],\"fov\":50}";
    string grass = "{\"preset\":\"meadow\",\"min\":[" + (-HALF) + "," + (-HALF) + "],\"max\":[" + HALF + "," + HALF + "],"
                   "\"density\":700,\"seed\":11,\"windDirection\":[0.4,-1],\"gust\":0.45,"
                   "\"mask\":" + grid(true) + ",\"tall\":" + grid(false) + "}";
    string world = "{\"camera\":" + cam + ",\"ambient\":0.3,\"shadowMapSize\":4096,"
                   "\"sky\":{\"enabled\":true,\"zenith\":\"#33406e\",\"horizon\":\"#f2b170\",\"ground\":\"#3a3324\","
                   "\"sunDirection\":[0.25,0.16,-1],\"sunColor\":\"#ffd49a\",\"sunGlow\":1.0,\"show\":true},"
                   "\"fogMode\":\"exponential2\",\"fogDensity\":0.012,\"fogFromSky\":true,"
                   "\"lights\":[{\"type\":\"directional\",\"position\":[2.5,1.6,-10],\"target\":[0,0,0],\"color\":\"#ffd49a\","
                   "\"intensity\":2.4,\"castShadow\":true,\"shadowExtent\":16}],"
                   "\"objects\":["
                   "{\"geometry\":\"plane\",\"width\":400,\"depth\":400,\"material\":{\"color\":\"#5d6a2e\",\"roughness\":1}},"
                   "{\"grass\":" + grass + "}]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
    draw::text("A MEADOW, MADE ON THE GPU", 34, 650, 18, 0xf4ead7);
    draw::text("grass field: tiles culled on the CPU, blades from a hash in the vertex shader", 34, 676, 12, 0xd8d0c0);
}
