// Flora scatter -- Rolling hills, a track, a clearing: grass and groves of bushes placed by one area description.
// Category: 3D

// One area says where things grow: hills from noise, a track (a strip), a clearing (a
// circle), nothing on slopes steeper than 30 degrees, and a soft edge. The terrain mesh,
// the grass and the scattered bushes all read it, so the grass and the bushes keep off the
// same track. The bushes are spheres standing in for trees until the tree presets land;
// their groves come from the scatter's clumping. Same seed, same landscape, any frame.
string area() {
    return "{\"min\":[-90,-90],\"max\":[90,90],"
           "\"ground\":{\"hills\":{\"height\":9,\"scale\":48,\"seed\":3}},"
           "\"strips\":[{\"points\":[[-90,-35],[-40,-12],[0,-4],[35,10],[90,42]],\"width\":5}],"
           "\"clearings\":[[28,-30,14]],\"edge\":1.5,\"maxSlope\":30}";
}

void configure(Composition &inout c) {
    c.name = "flora-scatter";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 300;
    c.background = 0x101018;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float t = frame / 30.0f;
    // A slow flight over the hills, turning towards the clearing.
    float x = -40 + 5.0f * t, z = 55 - 6.0f * t;
    string cam = "{\"position\":[" + x + ",17," + z + "],\"target\":[" + (x + 18) + ",4," + (z - 30) + "],\"fov\":50}";
    string land = area();
    string world = "{\"camera\":" + cam + ",\"ambient\":0.32,\"shadowMapSize\":4096,"
                   "\"sky\":{\"enabled\":true,\"zenith\":\"#3a5a92\",\"horizon\":\"#e8c49a\",\"ground\":\"#3a3324\","
                   "\"sunDirection\":[0.5,0.32,-0.8],\"sunColor\":\"#ffe2b4\",\"sunGlow\":0.8,\"show\":true},"
                   "\"fogMode\":\"exponential2\",\"fogDensity\":0.008,\"fogFromSky\":true,"
                   "\"lights\":[{\"type\":\"directional\",\"position\":[25,16,-40],\"target\":[0,0,0],\"color\":\"#ffe2b4\","
                   "\"intensity\":2.2,\"castShadow\":true,\"shadowExtent\":60}],"
                   "\"objects\":["
                   "{\"geometry\":\"terrain\",\"area\":" + land + ",\"resX\":180,\"resZ\":180,"
                   "\"material\":{\"color\":\"#5b6230\",\"roughness\":1}},"
                   "{\"grass\":{\"preset\":\"meadow\",\"area\":" + land + ",\"density\":450,\"seed\":7,\"windDirection\":[1,-0.4]}},"
                   "{\"geometry\":\"sphere\",\"radius\":1,\"rings\":10,\"slices\":14,\"material\":{\"color\":\"#ffffff\",\"roughness\":0.9},"
                   "\"instances\":{\"scatter\":{\"area\":" + land + ",\"spacing\":3.5,\"clumping\":0.85,\"clumpScale\":30,"
                   "\"scaleMin\":0.7,\"scaleMax\":1.6,\"lift\":0.55,\"tints\":[\"#3b5528\",\"#4a6630\",\"#5b7236\"],"
                   "\"shadeVariation\":0.15,\"seed\":5}}}]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
    draw::text("ONE AREA, THREE LAYERS", 34, 650, 18, 0xf4ead7);
    draw::text("terrain, grass and groves from the same hills, track and clearing", 34, 676, 12, 0xd8d0c0);
}
