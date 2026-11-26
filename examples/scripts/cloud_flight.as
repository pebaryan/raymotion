// Cloud flight -- The camera flies under cumulus over green hills, climbs through the base and comes out above a sea of cloud with gaps.
// Category: 3D

// `clouds` in the scene descriptor is a raymarched layer between `base` and `top` metres above the
// ground, round a planet, so a camera can fly through it. This clip climbs from 60 m to 3 700 m
// in ten seconds: under the flat bases (shadows on the hills), into the cloud (everything goes
// white and soft), and out of the top over the puffs, with the ground showing through the gaps.
// Frame 0 is under the cloud, frame 150 inside it and the last frame above it.
//
// Everything is a function of the frame: the camera path, the drift and the hills (a fixed
// sequence of numbers), so any frame renders alone.

float hash(int i) {
    uint n = uint(i * 7919 + 13);                                       // Wang's integer hash
    n = (n ^ 61) ^ (n >> 16);
    n = n * 9;
    n = n ^ (n >> 4);
    n = n * 0x27d4eb2d;
    n = n ^ (n >> 15);
    return float(n & 0xffff) / 65535.0f;
}

// Twenty-four flat green hills along the flight line, a pure function of their index.
string hills() {
    string list = "";
    for (int i = 0; i < 24; ++i) {
        float x = (hash(i * 4) - 0.5f) * 5200;
        if (x > -1500 && x < 1500) x += x < 0 ? -2400 : 2400;               // keep the line of flight clear
        float z = 800 - hash(i * 4 + 1) * 8200;
        float r = 700 + hash(i * 4 + 2) * 900;
        float h = 90 + hash(i * 4 + 3) * 220;
        if (list.length() > 0) list += ",";
        list += "{\"geometry\":\"sphere\",\"radius\":1,\"rings\":14,\"slices\":28,\"position\":[" + x + ",-20," + z + "],\"scale\":[" + r + "," + h + "," + r + "],"
                "\"material\":{\"color\":\"#5f8a3c\",\"roughness\":1}}";
    }
    return list;
}

void configure(Composition &inout c) {
    c.name = "cloud-flight";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 300;
    c.background = 0x101018;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float u = frame / 299.0f;
    float climb = u * u * (3 - 2 * u);                                  // eases in and out
    float y = 60 + 3640 * climb;
    float z = -5200 * u;
    float pitch = 0.30f - 0.42f * u;                                    // looks up under the cloud, down at the sea above it
    string cam = "{\"position\":[0," + y + "," + z + "],\"target\":[0," + (y + 1000 * pitch) + "," + (z - 1000) + "],\"fov\":62,\"near\":1,\"far\":300000}";
    string clouds = "{\"base\":1400,\"top\":3000,\"cover\":0.62,\"cloudType\":\"cumulus\",\"seed\":7,\"quality\":\"film\",\"drift\":[14,5],\"fps\":30,"
                    "\"shadowExtent\":20000}";
    string world = "{\"fps\":30,\"camera\":" + cam + ",\"ambient\":0.4,\"clouds\":" + clouds + ","
                   "\"sky\":{\"enabled\":true,\"sunLight\":0,\"sunDirection\":[-0.4,0.62,-0.55],\"sunGlow\":0.8,\"seed\":3},"
                   "\"lights\":[{\"type\":\"directional\",\"position\":[-40,62,-55],\"target\":[0,0,0],\"color\":\"#fff2dc\",\"intensity\":2.3}],"
                   "\"objects\":[{\"geometry\":\"plane\",\"width\":600000,\"depth\":600000,\"material\":{\"color\":\"#6f9a48\",\"roughness\":1}},"
                   + hills() + "]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
    draw::roundRect(24, 636, 560, 66, 10, 0x1f2937);
    draw::text("INTO THE CLOUD", 38, 650, 18, 0xf4ead7);
    draw::text(int(y) + " m  -  cloud layer 1 400 to 3 000 m", 38, 676, 12, 0xe0e4ec);
}
