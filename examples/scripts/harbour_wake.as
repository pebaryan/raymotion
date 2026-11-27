// Harbour wake -- A launch crosses a calm harbour on a curved course and leaves a Kelvin wake that spreads, reaches a moored dinghy and sets it rocking; a buoy rides the swell and a stone dropped near the camera sends rings across the water.
// Category: 3D

// Everything on the water here is worked out from the time alone: nothing is simulated.
//  - `wakes`: the launch's course (a curve through four points), speed and size are all the wake needs. The waves
//    it left at each earlier moment spread out with deep-water dispersion and add up into the Kelvin V (the arms
//    lie 19.5 degrees off the track at any speed) with feathered crests; white foam lines the first crests and a
//    broken trail of propwash follows the stern; `spray` throws droplets off the bow.
//  - `ripples`: a stone falls in at 3 s. Its rings are a train of capillary-gravity waves (the long ones run ahead
//    at 0.3 m/s, the fine ones trail behind), round, and they fade.
//  - `ride`: the launch, the dinghy and the buoy sit on the surface they are floating on. Each takes the mean
//    height under its footprint and the pitch and roll of the plane through it, so the long launch does not
//    jitter on waves shorter than itself. The launch follows wake 0 (its place and heading come from the same
//    course as the wake); the dinghy stands still and bobs when the waves reach it.
// The sea reflects the quay, the jetty and the boats (Water::reflections) and a light wind roughens it a little.

float PI2() { return 6.2831853f; }
float smooth01(float x) { x = x < 0 ? 0 : (x > 1 ? 1 : x); return x * x * (3 - 2 * x); }

string box(float x, float y, float z, float w, float h, float d, string color) {
    return "{\"geometry\":\"box\",\"width\":" + w + ",\"height\":" + h + ",\"depth\":" + d + ",\"position\":[" + x + "," + y + "," + z + "],"
           "\"material\":{\"color\":\"" + color + "\",\"roughness\":0.85}}";
}

string cylinder(float x, float y, float z, float r, float h, string color) {
    return "{\"geometry\":\"cylinder\",\"radius\":" + r + ",\"height\":" + h + ",\"position\":[" + x + "," + y + "," + z + "],"
           "\"material\":{\"color\":\"" + color + "\",\"roughness\":0.9}}";
}

// A bow: a box turned 45 degrees (a diamond from above) inside a group stretched along z, so its flat sides run to a point
// `len` metres ahead of z. The widest part is `beam` across, at z.
string bow(float y, float z, float beam, float len, float h, string color) {
    float side = beam * 0.7071f;
    return "{\"position\":[0," + y + "," + z + "],\"scale\":[1,1," + (len / (beam * 0.5f)) + "],\"children\":[{\"geometry\":\"box\",\"width\":" + side + ",\"height\":" + h + ",\"depth\":" + side
           + ",\"rotation\":[0,45,0],\"material\":{\"color\":\"" + color + "\",\"roughness\":0.6}}]}";
}

// The launch: a hull with a pointed bow, a cabin and a rail; 7 m long, 2.2 m across, the origin at the waterline.
string launch() {
    return "{\"name\":\"launch\",\"ride\":{\"wake\":0,\"length\":7,\"beam\":2.2,\"draft\":0.0},\"children\":["
        + box(0, 0.15f, -1.2f, 2.2f, 1.0f, 4.6f, "#e8e2d4") + ","
        + bow(0.15f, 1.1f, 2.2f, 2.1f, 1.0f, "#e8e2d4") + ","
        + box(0, 0.58f, -1.2f, 2.26f, 0.12f, 4.66f, "#b3331f") + ","
        + box(0, 1.15f, -1.7f, 1.6f, 1.0f, 2.2f, "#f4f1ea") + ","
        + box(0, 1.28f, -0.58f, 1.5f, 0.5f, 0.06f, "#5e7f94") + ","
        + box(0, 1.8f, -1.7f, 1.8f, 0.1f, 2.4f, "#6f7d86") + "]}";
}

// The dinghy: moored beside the jetty, bow towards the quay (-z). Its origin is on the water at (x, z).
string dinghy(float x, float z) {
    return "{\"name\":\"dinghy\",\"position\":[" + x + ",0," + z + "],\"ride\":{\"length\":3.2,\"beam\":1.5,\"draft\":0.0,\"heading\":-90},\"children\":["
        + box(0, 0.12f, -0.3f, 1.5f, 0.5f, 2.5f, "#2f5f7a") + ","
        + bow(0.12f, 0.95f, 1.5f, 1.3f, 0.5f, "#2f5f7a") + ","
        + box(0, 0.4f, -0.3f, 1.56f, 0.08f, 2.56f, "#e8e2d4") + ","
        + box(0, 0.5f, -0.5f, 1.2f, 0.1f, 0.35f, "#9a7a52") + ","
        + box(0, 0.5f, 0.35f, 1.2f, 0.1f, 0.3f, "#9a7a52") + "]}";
}

// A mooring buoy: red can with a white band and a mast; floats where it is.
string buoy(float x, float z) {
    return "{\"name\":\"buoy\",\"position\":[" + x + ",0," + z + "],\"ride\":{\"length\":0.9,\"beam\":0.9,\"draft\":0.1,\"heading\":90},\"children\":["
        + cylinder(0, 0.18f, 0, 0.42f, 0.7f, "#c43a24") + ","
        + cylinder(0, 0.36f, 0, 0.435f, 0.14f, "#f1ede4") + ","
        + cylinder(0, 0.95f, 0, 0.04f, 0.9f, "#3a3a3e") + ","
        + box(0, 1.5f, 0, 0.22f, 0.22f, 0.22f, "#e6b422") + "]}";
}

void configure(Composition &inout c) {
    c.name = "harbour-wake";
    c.w = 1920; c.h = 1080; c.fps = 30; c.duration = 600;
    c.background = 0x101820;
    c.statementBudget = 4000000;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float u = frame / 599.0f;
    // The camera eases in a little from the quay's edge, looking across the harbour.
    float e = smooth01(u);
    string cam = "{\"position\":[" + (4.0f - 4.0f * e) + "," + (10.5f - 1.0f * e) + "," + (16.0f - 2.0f * e) + "],\"target\":[" + (-1.0f + 1.0f * e) + ",0.0," + (-20.0f) + "],\"fov\":50}";
    string weather = "{\"preset\":\"clear\",\"windSpeed\":1.0,\"windDirection\":200,\"gust\":0.1,\"cover\":0.25,\"haze\":0.18}";

    // The launch's course across the harbour, right to left, in a gentle S; 3 m/s, 7 m long. Its path starts off the right of the
    // frame and its time is negative, so the wake and the propwash already exist at frame 0 and come into view from outside it.
    string wakes = "[{\"path\":[[110,3],[70,0.5],[46,-1.5],[26,-5],[4,-9],[-16,-10.5],[-30,-7.5],[-46,-3]],\"speed\":3,\"start\":-25.5,\"length\":7,\"beam\":2.2,\"strength\":1,\"life\":34,\"trail\":10,\"spray\":true}]";
    // A stone near the camera at 3 s, another at 11 s.
    string ripples = "[{\"x\":0.5,\"z\":8.5,\"time\":3.0,\"strength\":0.1,\"size\":0.05,\"splash\":true},{\"x\":-3.0,\"z\":6.5,\"time\":11.0,\"strength\":0.09,\"size\":0.04,\"splash\":true}]";

    string quay = box(0, 0.1f, -45, 140, 2.4f, 8, "#7d776b") + "," + box(0, 1.35f, -41.2f, 140, 0.18f, 0.5f, "#aaa392") + ",";
    string houses = box(-26, 5.1f, -52, 14, 7.6f, 10, "#c9b79a") + "," + box(-8, 4.3f, -52, 9, 6.0f, 10, "#b8654a") + "," + box(6, 5.8f, -53, 12, 9.0f, 10, "#d9d2c0")
        + "," + box(24, 4.1f, -52, 14, 5.6f, 10, "#8c9aa4") + "," + box(42, 4.7f, -52, 12, 6.8f, 10, "#c68b5b") + ",";
    // The jetty: a deck on piles running out from the quay; the dinghy lies against its side.
    string jetty = box(-3, 0.62f, -26.5f, 2.2f, 0.18f, 27, "#7a5f3f") + ",";
    for (int i = 0; i < 8; ++i) jetty += cylinder(-3 + (i % 2 == 0 ? -0.9f : 0.9f), -0.4f, -38.5f + 3.6f * i, 0.13f, 2.1f, "#4a3a28") + ",";
    string objects = quay + houses + jetty + dinghy(-0.95f, -16.0f) + "," + buoy(16, -21) + "," + buoy(26, -27) + "," + launch();

    string world = "{\"fps\":30,\"camera\":" + cam + ",\"ambient\":0.4,\"shadowMapSize\":2048,\"weather\":" + weather + ","
        "\"sky\":{\"enabled\":true,\"sunLight\":0,\"sunGlow\":1.0,\"clouds\":0.25,\"seed\":6,\"show\":true,\"sunDirection\":[0.55,0.38,-0.8]},"
        "\"lights\":[{\"type\":\"directional\",\"position\":[27,19,-40],\"target\":[0,0,0],\"color\":\"#ffe3c0\",\"intensity\":2.4,\"castShadow\":true,\"shadowExtent\":70}],"
        "\"water\":{\"preset\":\"calm\",\"level\":0,\"deep\":\"#0f4a5a\",\"fps\":30,\"reflections\":\"scene\",\"foam\":0,\"ownWind\":true,\"waves\":["
        "{\"direction\":20,\"wavelength\":9,\"amplitude\":0.018},{\"direction\":-25,\"wavelength\":4.2,\"amplitude\":0.01},"
        "{\"direction\":50,\"wavelength\":1.9,\"amplitude\":0.005},{\"direction\":5,\"wavelength\":0.9,\"amplitude\":0.0025}]},"
        "\"wakes\":" + wakes + ",\"ripples\":" + ripples + ","
        "\"objects\":[" + objects + "]}";
    g3d::draw(world, frame, 0, 0, 1920, 1080);
    draw::text("HARBOUR WAKE", 60, 990, 30, 0xf4ead7);
    draw::text("a Kelvin V worked out from the course  -  foam and spray  -  rings from a stone  -  boats that ride the waves", 60, 1030, 20, 0xd8d0c0);
}
