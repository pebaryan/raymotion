// Flora growth -- A bare hill greens from one spot outwards, then a grove grows from saplings, nearest first.
// Category: 3D

// Growth is a reveal u = 0..1, as in plot:: and geo::. The grass comes up where its
// arrival (here, distance from a point) is passed by `grow`; blades rise rather than pop.
// The trees extend their trunks, sprout branches outwards and unfold their leaves last;
// with growFrom, the ones nearer the spot start first. Every frame is a function of t alone.
void configure(Composition &inout c) {
    c.name = "flora-growth";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 360;
    c.background = 0x101018;
    scene(0, c.duration, "draw");
}

float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

void draw(int frame) {
    float t = frame / 30.0f;
    float grass = clamp01(t / 5.0f);                       // 0..5 s: the meadow spreads
    float trees = clamp01((t - 3.0f) / 8.0f);              // 3..11 s: the grove grows
    float turn = 0.35f + 0.04f * t;                         // a slow orbit
    float cx = 34 * sin(turn), cz = 34 * cos(turn);
    string land = "{\"min\":[-40,-40],\"max\":[40,40],\"ground\":{\"hills\":{\"height\":3,\"scale\":40,\"seed\":2}}}";
    string world = "{\"camera\":{\"position\":[" + cx + ",11," + cz + "],\"target\":[0,4,0],\"fov\":48},"
                   "\"ambient\":0.36,\"shadowMapSize\":4096,"
                   "\"sky\":{\"enabled\":true,\"zenith\":\"#4f72aa\",\"horizon\":\"#ecd6b0\",\"ground\":\"#3a3324\","
                   "\"sunDirection\":[0.5,0.45,-0.7],\"sunColor\":\"#fff0d0\",\"sunGlow\":0.6,\"show\":true},"
                   "\"fogMode\":\"exponential2\",\"fogDensity\":0.006,\"fogFromSky\":true,"
                   "\"lights\":[{\"type\":\"directional\",\"position\":[25,24,-35],\"target\":[0,0,0],\"color\":\"#fff0d0\","
                   "\"intensity\":2.2,\"castShadow\":true,\"shadowExtent\":40}],"
                   "\"objects\":["
                   "{\"geometry\":\"terrain\",\"area\":" + land + ",\"resX\":120,\"resZ\":120,"
                   "\"material\":{\"color\":\"#6a5a40\",\"roughness\":1}},"
                   "{\"grass\":{\"preset\":\"meadow\",\"area\":" + land + ",\"density\":500,\"seed\":3,"
                   "\"arrival\":{\"from\":[0,0],\"reach\":40},\"grow\":" + grass + "}},"
                   "{\"forest\":{\"trees\":[\"oak\",\"birch\",\"shrub\"],"
                   "\"scatter\":{\"area\":{\"min\":[-16,-16],\"max\":[16,16],\"ground\":{\"hills\":{\"height\":3,\"scale\":40,\"seed\":2}}},"
                   "\"spacing\":7,\"scaleMin\":0.6,\"scaleMax\":0.9,\"seed\":4},"
                   "\"grow\":" + trees + ",\"growFrom\":[0,0],\"growReach\":18,\"windDirection\":[1,-0.4]}}]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
    draw::text("FROM ONE SPOT", 34, 650, 18, 0xf4ead7);
    draw::text("grass rises outwards, then trees grow from saplings, nearest first", 34, 676, 12, 0xd8d0c0);
}
