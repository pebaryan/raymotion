// Forest light -- Morning mist in a wood: the low sun behind the trees comes through the canopy as shafts in the mist, and the camera walks the track.
// Category: 3D

// The mist is an exponential fog that thins with height (`heightFalloff`), and `volumetric` marches every
// view ray through it, asking the sun's shadow map at each step whether that bit of mist is lit. The
// trees cast the shadow (their leaf cards are cut out in the shadow map), so the mist is bright only where the sun
// reaches it between the crowns, and the shafts get their shapes from the canopy's gaps.
//  - The sun is a low directional light, ahead and to the left. Mist looks brightest toward the sun
//    (`anisotropy`) and the shafts fade with distance as the mist thickens; `ambient` is how much of
//    the mist's brightness it keeps in the shade of the trees.
//  - The light's target walks with the camera, so the shadow map (50 m round it) always covers what is in view.
//  - `aerialPerspective` tints the far trees by the same sky colour the mist takes.

string land() {
    return "{\"min\":[-200,-200],\"max\":[200,200],"
           "\"strips\":[{\"points\":[[-200,-6],[-60,-3],[0,-4],[80,-1],[200,-5]],\"width\":3.2}],"
           "\"clearings\":[[34,18,9]],\"edge\":2,\"maxSlope\":30}";
}

void configure(Composition &inout c) {
    c.name = "forest-light";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 360;
    c.background = 0x101018;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float t = frame / 30.0f;
    float x = -34 + 1.3f * t;                                  // a walking pace along the track
    float z = -3.6f + 0.35f * sin(0.35f * t);
    float aim = 0.12f * sin(0.25f * t);                         // the head turns a little
    string cam = "{\"position\":[" + x + ",1.7," + z + "],\"target\":[" + (x + 20) + "," + (2.4f + 1.6f * aim) + "," + (z - 3.2f + 8 * aim) + "],\"fov\":62}";
    // The sun: 14 degrees up, ahead and to the left of the way we walk.
    float sx = 0.95f, sy = 0.2f, sz = -0.24f;
    float tx = x + 22, tz = z - 2;                               // the shadow map follows the walk
    string area = land();
    string world = "{\"camera\":" + cam + ",\"ambient\":0.5,\"shadowMapSize\":4096,"
                   "\"sky\":{\"enabled\":true,\"zenith\":\"#6f8fb8\",\"horizon\":\"#f0d9b0\",\"ground\":\"#59503c\","
                   "\"sunLight\":0,\"sunColor\":\"#fff0cf\",\"sunSize\":2.2,\"sunGlow\":0.9,\"haze\":\"#ffcf94\",\"hazeAmount\":0.55,\"show\":true},"
                   "\"fogMode\":\"exponential\",\"fogDensity\":0.001,\"fogFromSky\":true,"
                   "\"volumetric\":{\"quality\":\"medium\",\"density\":0.075,\"heightFalloff\":0.1,\"heightBase\":0,\"anisotropy\":0.75,\"ambient\":0.05},"
                   "\"aerialPerspective\":{\"enabled\":true,\"scaleHeight\":1500,\"strength\":0.6},"
                   "\"lights\":[{\"type\":\"directional\",\"position\":[" + (tx + sx * 90) + "," + (sy * 90) + "," + (tz + sz * 90) + "],\"target\":[" + tx + ",0," + tz + "],"
                   "\"color\":\"#ffe6bc\",\"intensity\":2.8,\"castShadow\":true,\"shadowExtent\":55}],"
                   "\"objects\":["
                   "{\"geometry\":\"terrain\",\"area\":" + area + ",\"resX\":160,\"resZ\":160,"
                   "\"material\":{\"color\":\"#4d5a2c\",\"roughness\":1}},"
                   "{\"grass\":{\"preset\":\"meadow\",\"area\":" + area + ",\"density\":220,\"seed\":7,\"windDirection\":[1,-0.3],"
                   "\"height\":0.35,\"fadeEnd\":40,\"shadowDistance\":14}},"
                   "{\"forest\":{\"trees\":[\"birch\",\"oak\",{\"preset\":\"spruce\",\"seed\":4},\"birch\"],"
                   "\"scatter\":{\"area\":" + area + ",\"spacing\":5.5,\"clumping\":0.6,\"clumpScale\":30,"
                   "\"scaleMin\":0.8,\"scaleMax\":1.25,\"seed\":11},"
                   "\"reducedFrom\":30,\"impostorFrom\":70,\"blend\":2,\"windDirection\":[1,-0.3],\"gust\":0.3}}]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
}
