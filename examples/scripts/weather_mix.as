// Weather mix -- A calm meadow and pond turn into a storm: one wind bends the grass and trees, rolls the water and slants the rain.
// Category: 3D

// The scene's `weather` is one state (wind, cloud cover, haze) that the grass, the trees, the
// pond's waves, the rain and the sky all read. Here it is mixed from the "clear" preset into
// "storm" over the shot, so the wind rises from a light breeze to a gale and the sky greys over.
// Nothing else in the scene is animated. The wind blows towards 20 degrees (about +x, a little
// +z): look at which way the grass leans, the crests run and the rain falls.
//
// The weather is a function of the frame (nothing carries over), so the clouds are given how far
// the wind has carried them ("windRun", metres): the integral of the wind speed, which for a
// speed that eases from 3 to 20 m/s over T seconds is T * (3u + 17 (u^3 - u^4 / 2)).


// The grass mask: 0 over the pond and its bank, 1 elsewhere (a grid one metre apart over 60 m).
string maskGrid() {
    const int GRID = 61;
    const float HALF = 30;
    string values = "";
    for (int j = 0; j < GRID; ++j)
        for (int i = 0; i < GRID; ++i) {
            float x = -HALF + 2 * HALF * i / (GRID - 1), z = -HALF + 2 * HALF * j / (GRID - 1);
            bool pond = x > -5.5f && x < 11.5f && z > -9.5f && z < 0.5f;
            if (values.length() > 0) values += ",";
            values += pond ? "0" : "1";
        }
    return "{\"width\":" + GRID + ",\"height\":" + GRID + ",\"values\":[" + values + "]}";
}

string bank(float x, float z, float w, float d) {
    return "{\"geometry\":\"box\",\"width\":" + w + ",\"height\":0.8,\"depth\":" + d + ",\"position\":[" + x + ",-0.4," + z + "],"
           "\"material\":{\"color\":\"#4f5b2c\",\"roughness\":1}}";
}

void configure(Composition &inout c) {
    c.name = "weather-mix";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 240;
    c.background = 0x101018;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float u = frame / 239.0f;
    float a = u * u * (3 - 2 * u);                                   // how far into the storm
    float run = 8.0f * (3 * a + 17 * (a * a * a - a * a * a * a / 2));
    float speed = 3 + 17 * a;
    string weather = "{\"preset\":\"clear\",\"to\":\"storm\",\"amount\":" + a + ",\"windDirection\":20,\"windRun\":" + run + "}";
    string cam = "{\"position\":[" + (-4.5f + 2.5f * u) + ",2.6,10],\"target\":[3,0.4,-3.5],\"fov\":50}";
    string world = "{\"fps\":30,\"camera\":" + cam + ",\"ambient\":0.35,\"shadowMapSize\":2048,\"weather\":" + weather + ","
                   "\"sky\":{\"enabled\":true,\"sunLight\":0,\"sunGlow\":0.7,\"clouds\":0.2,\"cloudScale\":1.4,\"seed\":6,\"show\":true},"
                   "\"water\":{\"preset\":\"calm\",\"fetch\":17,\"waves\":[{\"direction\":0,\"wavelength\":6,\"amplitude\":0.06,\"sharpness\":1.5},{\"direction\":32,\"wavelength\":4.38,\"amplitude\":0.044,\"sharpness\":1.3},{\"direction\":-27,\"wavelength\":3.48,\"amplitude\":0.035},{\"direction\":-40,\"wavelength\":2.46,\"amplitude\":0.025},{\"direction\":14,\"wavelength\":1.98,\"amplitude\":0.02},{\"direction\":40,\"wavelength\":1.62,\"amplitude\":0.016},{\"direction\":-10,\"wavelength\":1.31,\"amplitude\":0.013}],\"level\":-0.9,\"openDepth\":4,\"foam\":0.5},"
                   "\"lights\":[{\"type\":\"directional\",\"position\":[8,13,-10],\"target\":[0,0,0],\"color\":\"#fff0d8\","
                   "\"intensity\":2.4,\"castShadow\":true,\"shadowExtent\":24}],"
                   "\"objects\":["
                   + bank(0, -209, 800, 400) + "," + bank(0, 200, 800, 400) + "," + bank(-202.5f, -4.5f, 395, 9) + "," + bank(205.5f, -4.5f, 389, 9) + ","
                   "{\"grass\":{\"preset\":\"meadow\",\"min\":[-30,-30],\"max\":[30,30],\"density\":260,\"seed\":4,\"height\":0.4,"
                   "\"fadeEnd\":60,\"shadowDistance\":24,\"mask\":" + maskGrid() + "}},"
                   "{\"forest\":{\"trees\":[\"oak\",\"birch\"],\"at\":[[-7,0,-9,20,1.0,0],[13,0,-9,80,0.9,1],[-13,0,-2,150,0.8,0],[16,0,2,10,0.7,1]],"
                   "\"reducedFrom\":40,\"impostorFrom\":90}},"
                   "{\"particles\":\"rain\",\"width\":34,\"depth\":26,\"height\":8,\"seed\":3,\"position\":[3,8,-2],"
                   "\"particleConfig\":{\"from\":105,\"prewarm\":false}}]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
    draw::text("ONE WEATHER, ONE WIND", 34, 650, 18, 0xf4ead7);
    draw::text("wind " + int(speed + 0.5f) + " m/s towards 20 deg  -  cover " + int(100 * (0.15f + 0.85f * a)) + "%  -  grass, trees, waves, rain and clouds all follow it", 34, 676, 12, 0xd8d0c0);
}
