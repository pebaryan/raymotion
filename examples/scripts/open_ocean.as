// Open ocean -- A low camera over a breezy sea with whitecaps rises in one move to ten kilometres, where the horizon curves and the sun's glitter path widens.
// Category: 3D

// The sea is a wave spectrum (`water.spectrum`): a JONSWAP sea of the weather's wind and the water's fetch,
// built as three cascades of grids with patches about 300 m, 40 m and 5 m, so there is no one tile to see.
// The wind blows towards +x at 9 m/s; the camera looks across it, towards the low sun. Whitecaps come from
// the crests that bunch up the most (the Jacobian of the choppy displacement), and fade behind them.
//
// The camera climbs along one curve from 2.4 m to 10 km, turning from the horizon to look down a little as
// it goes. The sea follows the planet's curvature (R = 6371 km), so at height h the horizon sits sqrt(2h/R)
// below horizontal (3.2 degrees at 10 km), and the far sea fades into the sky's horizon colour.
// Clouds, reflections and the weather are on; nothing else is in the scene.

void configure(Composition &inout c) {
    c.name = "open-ocean";
    c.w = 1920; c.h = 1080; c.fps = 30; c.duration = 330;
    c.background = 0x101820;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float u = frame / 329.0f;
    float k = u * u * (3 - 2 * u);                                    // eases in and out
    float height = 2.4f * pow(10000.0f / 2.4f, k);                    // 2.4 m to 10 km, evenly in log height
    float pitch = -0.02f - 0.2f * k;                                  // from level to looking down (rise per unit run)
    string cam = "{\"position\":[0," + height + ",0],\"target\":[0," + (height + pitch * 400.0f) + ",-400],\"fov\":62}";
    string weather = "{\"preset\":\"clear\",\"windSpeed\":9,\"windDirection\":0,\"gust\":0.2,\"cover\":0.14,\"cloudType\":\"cumulus\",\"haze\":0.02}";
    string world = "{\"fps\":30,\"camera\":" + cam + ",\"ambient\":0.35,\"weather\":" + weather + ","
        "\"sky\":{\"enabled\":true,\"sunLight\":0,\"sunGlow\":1.0,\"clouds\":0,\"seed\":5,\"show\":true,\"sunDirection\":[0.25,0.3,-1]},"
        "\"clouds\":{\"base\":1800,\"top\":3200,\"quality\":\"preview\"},"
        "\"water\":{\"preset\":\"calm\",\"level\":0,\"fetch\":60000,\"openDepth\":40,\"foam\":0.9,\"reflections\":\"quarter\","
        "\"spectrum\":{\"enabled\":true,\"cascades\":3,\"choppiness\":1.0,\"swell\":0.25,\"swellDirection\":-35,\"seed\":3}},"
        "\"lights\":[{\"type\":\"directional\",\"position\":[3,3.6,-12],\"target\":[0,0,0],\"color\":\"#fff0d8\",\"intensity\":2.6,\"castShadow\":false}],"
        "\"objects\":[]}";
    g3d::draw(world, frame, 0, 0, 1920, 1080);
    draw::text("OPEN OCEAN", 40, 1010, 20, 0xf4ead7);
    draw::text("height " + int(height + 0.5f) + " m  -  wind 9 m/s  -  three cascades of a JONSWAP spectrum, curvature R = 6371 km", 40, 1040, 14, 0xd8d0c0);
}
