// Blue ridges -- A valley and six ridges that recede to 35 km: each one paler and bluer than the one before, until the last is almost the sky.
// Category: 3D

// All the ridges are one terrain with the same dark green colour; nothing is painted to look far away.
// `aerialPerspective` does it from the depth of every pixel: the thing's own light loses its blue first (so the far
// ridges lose their green and warm tones), and the air in between adds the sky's horizon colour, until the farthest
// ridge is nearly the sky itself. Its density falls off with altitude (`scaleHeight`), so a ridge top that stands up
// out of the haze is clearer than its foot.
// The ground is a heightmap computed from the ridge lines (no search), made again each frame: 120 x 120 samples
// over 48 km x 48 km.

float ridgeZ(int i) { if (i == 0) return -2500.0f; if (i == 1) return -6000.0f; if (i == 2) return -11000.0f; if (i == 3) return -17000.0f; if (i == 4) return -24000.0f; return -32000.0f; }
float ridgeH(int i) { if (i == 0) return 260.0f; if (i == 1) return 900.0f; if (i == 2) return 1500.0f; if (i == 3) return 2300.0f; if (i == 4) return 3300.0f; return 4500.0f; }
float ridgeW(int i) { if (i == 0) return 1400.0f; if (i == 1) return 2100.0f; if (i == 2) return 2900.0f; if (i == 3) return 3500.0f; if (i == 4) return 4200.0f; return 5200.0f; }

// The ground's height at (x, z): the ridges, each a long crest whose height wanders along x.
float groundAt(float x, float z) {
    float h = 0.0f;
    for (int i = 0; i < 6; ++i) {
        float d = (z - ridgeZ(i)) / ridgeW(i);
        float crest = 0.62f + 0.38f * sin(x * 0.00061f + i * 2.1f) * sin(x * 0.00153f + i * 0.7f + 1.0f)
                    + 0.18f * sin(x * 0.0043f + i * 1.3f);
        h += ridgeH(i) * crest * pow(0.36788f, d * d);
    }
    // The lookout we stand on, at the near end.
    float dz = (z - 2000.0f) / 1300.0f;
    h += 330.0f * (0.85f + 0.15f * sin(x * 0.004f)) * pow(0.36788f, dz * dz);
    // The valley floor rises a little to the sides.
    float side = abs(x) / 24000.0f;
    return h + 60.0f * side * side;
}

string groundGrid() {
    const int NX = 120, NZ = 120;
    string values = "";
    for (int j = 0; j < NZ; ++j)
        for (int i = 0; i < NX; ++i) {
            float x = -24000.0f + 48000.0f * i / float(NX - 1), z = -44000.0f + 48000.0f * j / float(NZ - 1);
            if (i + j > 0) values += ",";
            values += int(groundAt(x, z));
        }
    return "{\"width\":" + NX + ",\"height\":" + NZ + ",\"values\":[" + values + "]}";
}

string trees(float t) {
    string s = "";
    for (int i = 0; i < 46; ++i) {
        float x = -520.0f + 1040.0f * (0.5f + 0.5f * sin(i * 12.9898f));
        float z = 600.0f - 1900.0f * (0.5f + 0.5f * sin(i * 78.233f + 3.0f));
        float h = 40.0f + 45.0f * (0.5f + 0.5f * sin(i * 3.7f));
        float y = groundAt(x, z);
        if (s.length() > 0) s += ",";
        s += "{\"geometry\":\"cone\",\"radius\":" + (h * 0.22f) + ",\"height\":" + h + ",\"position\":[" + x + "," + (y + h * 0.5f) + "," + z + "],\"material\":{\"color\":\"#243a24\",\"roughness\":1}}";
    }
    return s;
}

void configure(Composition &inout c) {
    c.name = "blue-ridges";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 240;
    c.background = 0x101018;
    c.statementBudget = 4000000;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float u = frame / 239.0f;
    float cz = 1500.0f - 500.0f * u;                                   // a slow walk down the valley
    float cx = -120.0f + 90.0f * u;
    float cy = groundAt(cx, cz) + 30.0f;
    string cam = "{\"position\":[" + cx + "," + cy + "," + cz + "],\"target\":[" + (120.0f - 60.0f * u) + "," + (cy + 330.0f) + "," + (cz - 9000.0f) + "],\"fov\":48,\"near\":8,\"far\":90000}";
    string land = "{\"min\":[-24000,-44000],\"max\":[24000,4000],\"ground\":" + groundGrid() + "}";
    string world = "{\"camera\":" + cam + ",\"ambient\":0.45,"
                   "\"sky\":{\"enabled\":true,\"zenith\":\"#3f6fb5\",\"horizon\":\"#bcd2ea\",\"ground\":\"#5a5a52\",\"sunLight\":0,\"sunColor\":\"#fff2d8\","
                   "\"sunSize\":2.0,\"sunGlow\":0.7,\"hazeAmount\":0.25,\"haze\":\"#f4d9b4\",\"clouds\":0.15,\"cloudScale\":1.5,\"seed\":3,\"show\":true},"
                   "\"aerialPerspective\":{\"enabled\":true,\"scaleHeight\":2200,\"strength\":2.4},"
                   "\"lights\":[{\"type\":\"directional\",\"position\":[-6000,5200,-4000],\"target\":[0,0,-6000],\"color\":\"#fff0d8\",\"intensity\":2.2,\"castShadow\":false}],"
                   "\"objects\":["
                   "{\"geometry\":\"terrain\",\"area\":" + land + ",\"resX\":239,\"resZ\":239,\"material\":{\"color\":\"#3a5a28\",\"roughness\":1}},"
                   + trees(u) + "]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
}
