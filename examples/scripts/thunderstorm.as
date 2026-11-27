// Thunderstorm -- Cumulonimbus towers with anvils roll over a stormy sea: flashes in the cloud, bolts to the water, then the rain moves off and a rainbow stands in it.
// Category: 3D

// A storm from start to finish, built from three scene objects:
//   `weather`    one state: the wind and the sea it raises, the rain, and (late) the distant rain that makes a bow;
//   `clouds`     cumulonimbus towers, each its own shape, with an anvil spread downwind (the wind blows towards +x);
//   `lightning`  explicit strikes: {time, start, end, seed}. A strike that ends at the water is a bolt (a jagged channel
//                with branches: a leader, a return stroke and a few restrikes); one that stays in the cloud is a flash
//                that lights the cloud from inside. Each also lights the sea, the rain and the cloud for its few frames.
//
// The camera looks at the storm over the sea. Seconds 1 to 6 the flashes and three bolts; from 4 s the storm passes (the
// weather is mixed from "storm" to "clear"), the cover thins and the sun, behind the camera and 20 degrees up, comes
// out; from about 9 s the rain that is left stands in the distance and the rainbow appears opposite the sun: a soft
// spectral arc with the secondary outside it, and a darker band between the two.
//
// Everything is a function of the frame (the strikes are a fixed list, their shapes come from their seeds), so any frame
// renders alone. For the sound, `g3d::thunderCues` (C++) lists each strike's time, distance and the time the thunder
// arrives (distance / 343 m/s); the clip itself has no soundtrack.

float smooth(float a, float b, float x) {
    float u = (x - a) / (b - a);
    if (u < 0) u = 0;
    if (u > 1) u = 1;
    return u * u * (3 - 2 * u);
}

string strike(float time, float x, float z, float topY, float endY, int seed) {
    return "{\"time\":" + time + ",\"start\":[" + x + "," + topY + "," + z + "],\"end\":[" + (x + 40) + "," + endY + "," + (z - 30) + "],\"seed\":" + seed + "}";
}

void configure(Composition &inout c) {
    c.name = "thunderstorm";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 360;
    c.background = 0x101018;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float t = frame / 30.0f;
    float pass = smooth(4.0f, 10.0f, t);                              // 0: the storm, 1: clear
    float rain = 1.0f - smooth(5.0f, 9.0f, t);
    float bow = smooth(8.0f, 10.5f, t);
    float cover = 0.16f - 0.11f * smooth(4.5f, 10.0f, t);
    string cam = "{\"position\":[0,9,0],\"target\":[" + (400 - 300 * smooth(0.0f, 12.0f, t)) + ",2300,-9000],\"fov\":54,\"near\":0.5,\"far\":400000}";
    string weather = "{\"preset\":\"storm\",\"to\":\"clear\",\"amount\":" + pass + ",\"windDirection\":0,\"windRun\":" + (t * 12.0f) + ",\"cloudType\":\"cumulonimbus\","
                     "\"rainbow\":{\"distantRain\":" + (bow * 0.9f) + ",\"rainDistance\":3000}}";
    string strikes = strike(1.20f, -900, -5200, 3400, 3000, 11) + "," + strike(2.45f, 700, -6400, 3800, 3300, 12) + ","
                   + strike(2.90f, -1300, -3600, 1300, 0, 3) + "," + strike(3.55f, 1500, -7000, 4200, 3600, 13) + ","
                   + strike(4.20f, 650, -4200, 1300, 0, 21) + "," + strike(4.80f, -400, -6000, 3600, 3100, 14) + ","
                   + strike(5.70f, -2100, -4800, 1300, 0, 8);
    string world = "{\"fps\":30,\"camera\":" + cam + ",\"ambient\":0.34,\"weather\":" + weather + ","
        "\"sky\":{\"enabled\":true,\"sunLight\":0,\"sunDirection\":[-0.3,0.35,0.9],\"sunGlow\":0.5,\"clouds\":0,\"seed\":5,\"show\":true},"
        "\"clouds\":{\"base\":1200,\"top\":11000,\"cloudType\":\"cumulonimbus\",\"quality\":\"film\",\"seed\":3,\"ownWeather\":true,\"cover\":" + cover + ","
        "\"drift\":[12,0],\"shadows\":false,\"anvil\":1,\"towerVariety\":1,\"storms\":[{\"x\":-1000,\"z\":-11000,\"radius\":6500,\"top\":10500},{\"x\":18000,\"z\":-30000,\"radius\":5500,\"top\":11000}]},"
        "\"precipitation\":{\"kind\":\"rain\",\"intensity\":" + (0.9f * rain) + "},"
        "\"lightning\":{\"strikes\":[" + strikes + "],\"followClouds\":false,\"width\":1.6,\"glow\":1.5},"
        "\"water\":{\"preset\":\"calm\",\"level\":0,\"fetch\":100000,\"openDepth\":40,\"foam\":0.9,\"reflections\":\"quarter\","
        "\"spectrum\":{\"enabled\":true,\"cascades\":3,\"seed\":3}},"
        "\"lights\":[{\"type\":\"directional\",\"position\":[-30,35,90],\"target\":[0,0,0],\"color\":\"#fff0d8\",\"intensity\":2.4,\"castShadow\":false}],"
        "\"objects\":[]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
    draw::text("THUNDERSTORM", 40, 660, 20, 0xf4ead7);
    draw::text("cumulonimbus with anvils  -  strikes from a list  -  rain moves off, the sun behind the camera, a rainbow opposite it", 40, 688, 13, 0xd8d0c0);
}
