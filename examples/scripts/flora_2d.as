// Flora 2D -- A hillside in three layers: far trees in haze, a meadow, and a near oak that grows while wind crosses the grass.
// Category: Drawing

// flora2d::draw takes a list of items drawn in order (far first). An item's "depth"
// (0 near .. 1 far) shifts it with the camera and fades its colours into the haze; "u"
// grows it (grass rises behind a front, trees run their wood out from the root and leaf
// last). The wind's gusts travel along x. Every frame is a function of t alone.
void configure(Composition &inout c) {
    c.name = "flora-2d";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 300;
    c.background = 0xdde6ec;
    scene(0, c.duration, "draw");
}

float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

void draw(int frame) {
    float t = frame / 30.0f;
    float grow = clamp01((t - 1.0f) / 6.0f);                // the oak: 1..7 s
    float field = clamp01(t / 3.0f);                        // the near grass: 0..3 s
    string cam = "[" + (40 * t) + ",0]";                    // a slow pan to the right
    draw::rect(0, 0, 1280, 720, 0xdde6ec);
    flora2d::draw("{\"fps\":30,\"camera\":" + cam + ",\"haze\":\"#dde6ec\","
        "\"wind\":{\"strength\":1,\"gust\":0.8,\"gustSpeed\":300},"
        "\"items\":["
        // far: a ridge of silhouetted trees
        "{\"ground\":{\"depth\":0.75,\"hills\":{\"x0\":-200,\"x1\":2400,\"y\":430,\"amplitude\":40,\"wavelength\":900,\"seed\":3},\"bottom\":720,\"color\":\"#3f5a3a\"}},"
        "{\"tree\":{\"depth\":0.75,\"preset\":\"pine\",\"x\":220,\"height\":150,\"silhouette\":true,\"seed\":1}},"
        "{\"tree\":{\"depth\":0.75,\"preset\":\"pine\",\"x\":300,\"height\":120,\"silhouette\":true,\"seed\":2}},"
        "{\"tree\":{\"depth\":0.75,\"preset\":\"oak\",\"x\":760,\"height\":130,\"silhouette\":true,\"leafSize\":13,\"seed\":3}},"
        "{\"tree\":{\"depth\":0.75,\"preset\":\"pine\",\"x\":1180,\"height\":160,\"silhouette\":true,\"seed\":4}},"
        "{\"tree\":{\"depth\":0.75,\"preset\":\"pine\",\"x\":1260,\"height\":130,\"silhouette\":true,\"seed\":5}},"
        "{\"tree\":{\"depth\":0.75,\"preset\":\"oak\",\"x\":1700,\"height\":140,\"silhouette\":true,\"leafSize\":14,\"seed\":6}},"
        // middle: a meadow on a nearer hill, a birch and shrubs
        "{\"ground\":{\"depth\":0.4,\"hills\":{\"x0\":-200,\"x1\":2400,\"y\":540,\"amplitude\":30,\"wavelength\":700,\"seed\":5},\"bottom\":720,\"color\":\"#5d7a3e\"}},"
        "{\"grass\":{\"depth\":0.4,\"preset\":\"meadow\",\"height\":26,\"density\":0.6,\"seed\":2}},"
        "{\"tree\":{\"depth\":0.4,\"preset\":\"birch\",\"x\":980,\"height\":230,\"seed\":8}},"
        "{\"shrub\":{\"depth\":0.4,\"x\":520,\"size\":60,\"height\":40,\"seed\":3}},"
        "{\"shrub\":{\"depth\":0.4,\"x\":1500,\"size\":70,\"height\":46,\"seed\":4}},"
        // near: the growing oak, tall grass and tufts, coming up from the left
        "{\"ground\":{\"hills\":{\"x0\":-200,\"x1\":2800,\"y\":650,\"amplitude\":16,\"wavelength\":600,\"seed\":9},\"bottom\":720,\"color\":\"#4a6a34\"}},"
        "{\"tree\":{\"preset\":\"oak\",\"x\":640,\"height\":420,\"u\":" + grow + ",\"seed\":11}},"
        "{\"grass\":{\"preset\":\"tall\",\"height\":70,\"u\":" + field + ",\"seed\":4}},"
        "{\"tuft\":{\"at\":[300,660],\"preset\":\"tall\",\"height\":90,\"count\":18,\"u\":" + field + ",\"seed\":5}},"
        "{\"tuft\":{\"at\":[1120,652],\"preset\":\"dry\",\"height\":70,\"count\":14,\"u\":" + field + ",\"seed\":6}}"
        "]}", frame);
    draw::text("A HILLSIDE IN THREE LAYERS", 34, 34, 18, 0x23301f);
    draw::text("far trees in haze, a meadow, a near oak growing while gusts cross the grass", 34, 60, 12, 0x3a4a34);
}
