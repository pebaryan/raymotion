// Flora forest -- A mixed wood on rolling hills: full trees near the camera, impostors far off, all in the wind.
// Category: 3D

// One area (hills, a track, a clearing) carries the terrain, the grass and the forest. Four
// kinds of tree are grown from presets; the forest scatters them in groves, draws each at the
// detail its distance needs (full, reduced, then a picture of itself) and cross-fades
// between them so nothing pops. The camera rises from the track to above the canopy.
string area() {
    return "{\"min\":[-160,-160],\"max\":[160,160],"
           "\"ground\":{\"hills\":{\"height\":10,\"scale\":70,\"seed\":3}},"
           "\"strips\":[{\"points\":[[-160,-40],[-60,-12],[0,-2],[50,14],[160,50]],\"width\":7}],"
           "\"clearings\":[[30,-34,18]],\"edge\":2,\"maxSlope\":32}";
}

void configure(Composition &inout c) {
    c.name = "flora-forest";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 360;
    c.background = 0x101018;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float t = frame / 30.0f;
    float u = t / 12.0f;                                     // 0..1 over the shot
    float ease = u * u * (3 - 2 * u);
    // Along the track, then up and back over the trees.
    float x = -70 + 60 * ease, z = -16 + 10 * ease, y = 15 + 30 * ease * ease;
    string cam = "{\"position\":[" + x + "," + y + "," + z + "],\"target\":[" + (x + 40) + "," + (2 + 10 * ease) + "," + (z - 6 - 30 * ease) + "],\"fov\":55}";
    string land = area();
    string world = "{\"camera\":" + cam + ",\"ambient\":0.34,\"shadowMapSize\":4096,"
                   "\"sky\":{\"enabled\":true,\"zenith\":\"#4a6aa0\",\"horizon\":\"#e9cfa6\",\"ground\":\"#3a3324\","
                   "\"sunDirection\":[0.45,0.42,-0.78],\"sunColor\":\"#ffe6bc\",\"sunGlow\":0.7,\"show\":true},"
                   "\"fogMode\":\"exponential2\",\"fogDensity\":0.0045,\"fogFromSky\":true,"
                   "\"lights\":[{\"type\":\"directional\",\"position\":[30,28,-52],\"target\":[0,0,0],\"color\":\"#ffe6bc\","
                   "\"intensity\":2.3,\"castShadow\":true,\"shadowExtent\":70}],"
                   "\"objects\":["
                   "{\"geometry\":\"terrain\",\"area\":" + land + ",\"resX\":200,\"resZ\":200,"
                   "\"material\":{\"color\":\"#55602e\",\"roughness\":1}},"
                   "{\"grass\":{\"preset\":\"meadow\",\"area\":" + land + ",\"density\":300,\"seed\":7,\"windDirection\":[1,-0.3],"
                   "\"fadeEnd\":70,\"shadowDistance\":30}},"
                   "{\"forest\":{\"trees\":[\"oak\",\"birch\",{\"preset\":\"spruce\",\"seed\":4},\"pine\"],"
                   "\"scatter\":{\"area\":" + land + ",\"spacing\":9,\"clumping\":0.75,\"clumpScale\":45,"
                   "\"scaleMin\":0.75,\"scaleMax\":1.2,\"seed\":5},"
                   "\"reducedFrom\":35,\"impostorFrom\":90,\"blend\":8,\"windDirection\":[1,-0.3],\"gust\":0.5}}]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
    draw::text("A MIXED WOOD", 34, 650, 18, 0xf4ead7);
    draw::text("oak, birch, spruce and pine: full trees near, pictures of themselves far off", 34, 676, 12, 0xd8d0c0);
}
