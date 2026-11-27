// River valley -- A river winds down a valley into a lake with a jetty, and a pool sits on a terrace: soft foamy shores, ripples carried by the current, hills mirrored in the lake.
// Category: 3D

// Water here is not the endless sea: `waterBodies` fills shapes. The lake is a polygon at level 0,
// the pool a small rectangle on the terrace, the river a path with a width and a flow speed.
//  - The shore: the water reads the depth of the ground under it, so it fades out to nothing where the
//    bank rises through the surface (no hard line) and a band of lace foam lies in the first 40 cm.
//  - The river's ripples are carried along the channel at the flow speed (a flow map: two layers half a
//    cycle apart, cross-faded), so they never slide or reset; it falls 1.8 m on its way to the lake.
//  - The lake reflects the hills, the trees and the jetty (one reflection per frame: the lake's level;
//    the river and the pool, at other levels, reflect the sky).
//  - The weather's wind roughens the lake over its own size as the fetch.
//
// The ground is a heightmap computed from a closed-form valley (no search along the river): scripts
// keep no globals, so it is made again each frame, on a 1 m grid.

// The lake: centre, radii; the river's level at its start (z = -58).
float LAKE_X() { return 6.0f; }
float LAKE_Z() { return 10.0f; }
float LAKE_RX() { return 17.0f; }
float LAKE_RZ() { return 12.0f; }
float RIVER_TOP() { return 1.8f; }
float PI2() { return 6.2831853f; }

float smooth01(float x) { x = x < 0 ? 0 : (x > 1 ? 1 : x); return x * x * (3 - 2 * x); }
float smin(float a, float b, float k) {
    float h = (0.5f + 0.5f * (b - a) / k);
    h = h < 0 ? 0 : (h > 1 ? 1 : h);
    return b + (a - b) * h - k * h * (1 - h);
}

// The river's centre line x at z, its slope, and its level.
float riverX(float z) { return -4.0f + 10.0f * sin(0.085f * z + 0.4f) + 3.0f * sin(0.21f * z); }
float riverDX(float z) { return 10.0f * 0.085f * cos(0.085f * z + 0.4f) + 3.0f * 0.21f * cos(0.21f * z); }
float riverLevel(float z) { float f = smooth01((z + 58.0f) / 56.0f); return RIVER_TOP() * (1.0f - f * 0.35f - f * f * 0.65f); }

float groundAt(float x, float z) {
    // Hills rise away from the valley, higher to the back.
    float hills = 1.2f + 9.0f * smooth01((abs(x + 4.0f) - 6.0f) / 55.0f) + 8.5f * smooth01((-z - 20.0f) / 45.0f)
                + 1.4f * sin(x * 0.11f + 1.0f) * sin(z * 0.09f + 0.5f) + 0.7f * sin(x * 0.27f) * sin(z * 0.23f + 2.0f);
    // The river's channel.
    float dx = x - riverX(z);
    float r = abs(dx) / sqrt(1.0f + riverDX(z) * riverDX(z));
    float lvl = riverLevel(z);
    float channel = lvl - 0.55f + 0.30f * r + 0.022f * r * r;
    if (z > -2.0f) channel = 9.0f;
    // The lake's bowl: deepest in the middle, a gentle bank round it.
    float ex = (x - LAKE_X()) / LAKE_RX(), ez = (z - LAKE_Z()) / LAKE_RZ();
    float e = sqrt(ex * ex + ez * ez);
    float bowl = e < 1.0f ? -3.2f * (1.0f - e * e) : 2.8f * (e - 1.0f) + 6.0f * (e - 1.0f) * (e - 1.0f);
    float h = smin(hills, smin(channel, bowl, 0.6f), 0.8f);
    // The terrace for the pool.
    float tx = smooth01((15.0f - abs(x + 26.0f)) / 6.0f), tz = smooth01((14.0f - abs(z - 19.0f)) / 6.0f);
    float t = tx * tz;
    h = h + (2.4f - h) * t;
    return h;
}

string groundGrid() {
    const int N = 121;
    string values = "";
    for (int j = 0; j < N; ++j)
        for (int i = 0; i < N; ++i) {
            float x = -60 + 120.0f * i / float(N - 1), z = -60 + 120.0f * j / float(N - 1);
            if (i + j > 0) values += ",";
            values += int(groundAt(x, z) * 100) / 100.0f;
        }
    return "{\"width\":" + N + ",\"height\":" + N + ",\"values\":[" + values + "]}";
}

string box(float x, float y, float z, float w, float h, float d, string color) {
    return "{\"geometry\":\"box\",\"width\":" + w + ",\"height\":" + h + ",\"depth\":" + d + ",\"position\":[" + x + "," + y + "," + z + "],"
           "\"material\":{\"color\":\"" + color + "\",\"roughness\":0.9}}";
}

string tree(float x, float z, float h, float s) {
    float y = groundAt(x, z);
    string crown = (s > 0.5f) ? "#2c4a2a" : "#3a5a2c";
    return "{\"geometry\":\"cylinder\",\"radius\":" + (0.16f * h / 6.0f) + ",\"height\":" + (h * 0.35f) + ",\"position\":[" + x + "," + (y + h * 0.17f) + "," + z + "],\"material\":{\"color\":\"#4a3524\",\"roughness\":1}},"
           "{\"geometry\":\"cone\",\"radius\":" + (h * 0.3f) + ",\"height\":" + (h * 0.85f) + ",\"position\":[" + x + "," + (y + h * 0.35f + h * 0.42f) + "," + z + "],\"material\":{\"color\":\"" + crown + "\",\"roughness\":1}}";
}

string points(const array<float> &in xs, const array<float> &in zs) {
    string s = "";
    for (uint i = 0; i < xs.length(); ++i) {
        if (i > 0) s += ",";
        s += "[" + xs[i] + "," + zs[i] + "]";
    }
    return s;
}

void configure(Composition &inout c) {
    c.name = "river-valley";
    c.w = 1920; c.h = 1080; c.fps = 30; c.duration = 300;
    c.background = 0x101820;
    c.statementBudget = 4000000;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float u = frame / 299.0f;
    float e = smooth01(u);
    // The camera drifts from the terrace pool, over the lake, to look up the river.
    float cx = -30.0f + 33.0f * e, cy = 6.5f - 2.8f * e, cz = 33.0f - 3.0f * e;
    float tx = -8.0f + 6.0f * e, ty = 1.4f - 0.3f * e, tz = 6.0f - 40.0f * e;
    string cam = "{\"position\":[" + cx + "," + cy + "," + cz + "],\"target\":[" + tx + "," + ty + "," + tz + "],\"fov\":50}";
    string weather = "{\"preset\":\"clear\",\"windSpeed\":4.5,\"windDirection\":70,\"gust\":0.3,\"cover\":0.3,\"haze\":0.25}";

    // The river: a path every 6 m, a level for each point, wider towards the lake.
    array<float> rx, rz, rl, rw;
    for (int i = 0; i < 11; ++i) {
        float z = -58.0f + 5.7f * i;
        rx.insertLast(riverX(z)); rz.insertLast(z);
        rl.insertLast(riverLevel(z));
        rw.insertLast(4.2f + 0.45f * i);
    }
    string levels = "", widths = "";
    for (uint i = 0; i < rl.length(); ++i) { if (i > 0) { levels += ","; widths += ","; } levels += rl[i]; widths += rw[i]; }
    string river = "{\"type\":\"river\",\"path\":[" + points(rx, rz) + "],\"width\":[" + widths + "],\"levels\":[" + levels + "],\"level\":0,\"flowSpeed\":1.4,\"narrowFaster\":true,"
        "\"deep\":\"#2a5a52\",\"absorb\":[0.45,0.12,0.09],\"reflections\":\"off\",\"ripple\":0.16,\"rippleLength\":0.9,\"foamWidth\":0.25,\"shoreFoam\":0.8,\"fps\":30}";
    // The lake: an outline just beyond the shoreline, so the bank hides its rim.
    string outline = "";
    for (int i = 0; i < 36; ++i) {
        float a = PI2() * i / 36.0f;
        float wob = 1.07f + 0.05f * sin(a * 3 + 1.0f);
        if (i > 0) outline += ",";
        outline += "[" + (LAKE_X() + LAKE_RX() * wob * cos(a)) + "," + (LAKE_Z() + LAKE_RZ() * wob * sin(a)) + "]";
    }
    string lake = "{\"type\":\"lake\",\"outline\":[" + outline + "],\"level\":0,\"preset\":\"calm\",\"fetch\":0,\"deep\":\"#23606a\",\"reflections\":\"scene\",\"refraction\":true,\"caustics\":0.5,\"fps\":30}";
    // The pool on the terrace: a rectangle inside its coping.
    string pool = "{\"type\":\"pool\",\"outline\":[[-30.4,15.6],[-21.6,15.6],[-21.6,22.4],[-30.4,22.4]],\"level\":3.2,\"preset\":\"calm\",\"deep\":\"#2c8aa0\",\"absorb\":[0.9,0.2,0.12],\"reflections\":\"off\",\"refraction\":true,\"caustics\":0.8,\"foam\":0,\"shoreFoam\":0,\"fps\":30}";

    string trees = "";
    for (int i = 0; i < 70; ++i) {
        float x = -56.0f + 112.0f * (0.5f + 0.5f * sin(i * 12.9898f));
        float z = -56.0f + 100.0f * (0.5f + 0.5f * sin(i * 78.233f + 3.0f));
        float y = groundAt(x, z);
        float ex = (x - LAKE_X()) / (LAKE_RX() + 5), ez = (z - LAKE_Z()) / (LAKE_RZ() + 5);
        float dr = abs(x - riverX(z));
        bool onTerrace = abs(x + 26.0f) < 15.0f && abs(z - 19.0f) < 13.0f;
        if (y < 2.2f || (ex * ex + ez * ez) < 1.0f || dr < 9.0f || onTerrace) continue;
        trees += tree(x, z, 5.5f + 3.0f * (0.5f + 0.5f * sin(i * 3.7f)), 0.5f + 0.5f * sin(i * 1.3f)) + ",";
    }

    string world = "{\"fps\":30,\"camera\":" + cam + ",\"ambient\":0.35,\"shadowMapSize\":2048,\"weather\":" + weather + ","
        "\"sky\":{\"enabled\":true,\"sunLight\":0,\"sunGlow\":1.0,\"clouds\":0.3,\"seed\":4,\"show\":true,\"sunDirection\":[0.35,0.33,-0.85]},"
        "\"lights\":[{\"type\":\"directional\",\"position\":[6,46,26],\"target\":[0,0,0],\"color\":\"#ffd9a8\",\"intensity\":2.3,\"castShadow\":true,\"shadowExtent\":62}],"
        "\"waterBodies\":[" + lake + "," + river + "," + pool + "],"
        "\"objects\":["
        "{\"geometry\":\"terrain\",\"area\":{\"min\":[-60,-60],\"max\":[60,60],\"ground\":" + groundGrid() + "},\"resX\":120,\"resZ\":120,\"material\":{\"color\":\"#556b3a\",\"roughness\":1}},"
        // the jetty
        + box(10.0f, 0.42f, 17.5f, 1.8f, 0.12f, 11.0f, "#6b5238") + ","
        + box(9.2f, 0.0f, 12.3f, 0.18f, 1.2f, 0.18f, "#4a3a28") + ","
        + box(10.8f, 0.0f, 12.3f, 0.18f, 1.2f, 0.18f, "#4a3a28") + ","
        + box(9.2f, 0.0f, 16.0f, 0.18f, 1.2f, 0.18f, "#4a3a28") + ","
        + box(10.8f, 0.0f, 16.0f, 0.18f, 1.2f, 0.18f, "#4a3a28") + ","
        // the pool: a floor and four walls of coping round the water
        + box(-26.0f, 2.55f, 19.0f, 9.6f, 0.3f, 7.6f, "#a9c2c6") + ","
        + box(-26.0f, 2.7f, 15.3f, 9.8f, 1.4f, 0.8f, "#d8d4c8") + ","
        + box(-26.0f, 2.7f, 22.7f, 9.8f, 1.4f, 0.8f, "#d8d4c8") + ","
        + box(-30.7f, 2.7f, 19.0f, 0.8f, 1.4f, 7.6f, "#d8d4c8") + ","
        + box(-21.3f, 2.7f, 19.0f, 0.8f, 1.4f, 7.6f, "#d8d4c8") + ","
        + trees +
        "{\"geometry\":\"box\",\"width\":0.1,\"height\":0.1,\"depth\":0.1,\"position\":[0,-50,0],\"material\":{\"color\":\"#000000\"}}"
        "]}";
    g3d::draw(world, frame, 0, 0, 1920, 1080);
    draw::text("WATER THAT FILLS A SHAPE", 60, 980, 30, 0xf4ead7);
    draw::text("soft shores and foam  -  a current you can read  -  hills in the lake", 60, 1020, 22, 0xd8d0c0);
}
