// Flora styles -- One grove, six looks: realistic, low-poly toon with ink, Gooch, hatch, paper cut-outs and splat canopies.
// Category: 3D

// The same scatter, wind and grass code under each look. A forest's "build" picks the
// tree style (realistic, lowpoly, paper, splat) and the shading of its materials; the "toon"
// grass preset draws wide blades in three flat colour bands. Every frame is a function of t.
void configure(Composition &inout c) {
    c.name = "flora-styles";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 240;
    c.background = 0x101018;
    scene(0, c.duration, "draw");
}

string panel(float turn, string build, string grass, string grassMaterial, string extra) {
    float cx = 30 * sin(turn), cz = 30 * cos(turn);
    return "{\"camera\":{\"position\":[" + cx + ",9," + cz + "],\"target\":[0,4,0],\"fov\":46},"
           "\"ambient\":0.4,\"shadowMapSize\":2048," + extra +
           "\"sky\":{\"enabled\":true,\"zenith\":\"#6f93c8\",\"horizon\":\"#f0dfc0\",\"ground\":\"#3a3324\",\"show\":true},"
           "\"lights\":[{\"type\":\"directional\",\"position\":[20,26,-14],\"target\":[0,0,0],\"color\":\"#fff2d8\","
           "\"intensity\":2.2,\"castShadow\":true,\"shadowExtent\":30}],"
           "\"objects\":["
           "{\"geometry\":\"plane\",\"width\":90,\"depth\":90,\"material\":{\"color\":\"#5d6a38\",\"roughness\":1" + grassMaterial + "}},"
           "{\"grass\":{\"preset\":\"" + grass + "\",\"min\":[-24,-24],\"max\":[24,24],\"seed\":3},\"material\":{\"roughness\":0.9" + grassMaterial + "}},"
           "{\"forest\":{\"trees\":[\"oak\",\"spruce\",\"birch\"],\"build\":" + build + ","
           "\"scatter\":{\"area\":{\"min\":[-13,-13],\"max\":[13,13]},\"spacing\":7.5,\"scaleMin\":0.55,\"scaleMax\":0.8,\"seed\":5},"
           "\"windDirection\":[1,-0.3]}}]}";
}

void draw(int frame) {
    float t = frame / 30.0f;
    float turn = 0.5f + 0.05f * t;
    int w = 426, h = 360;
    g3d::draw(panel(turn, "{}", "meadow", "", ""), frame, 0, 0, w, h);
    g3d::draw(panel(turn, "{\"style\":\"lowpoly\",\"shading\":\"toon\"}", "toon", ",\"shading\":\"toon\",\"lineWidth\":0",
                    "\"lines\":{\"color\":\"#1d2018\",\"width\":1.5},"), frame, w, 0, w, h);
    g3d::draw(panel(turn, "{\"style\":\"lowpoly\",\"shading\":\"gooch\"}", "dry", ",\"shading\":\"gooch\"", ""), frame, 2 * w, 0, 1280 - 2 * w, h);
    g3d::draw(panel(turn, "{\"style\":\"lowpoly\",\"shading\":\"hatch\"}", "toon", ",\"shading\":\"hatch\"", ""), frame, 0, h, w, h);
    g3d::draw(panel(turn, "{\"style\":\"paper\"}", "toon", ",\"shading\":\"toon\"", ""), frame, w, h, w, h);
    g3d::draw(panel(turn, "{\"style\":\"splat\"}", "meadow", "", ""), frame, 2 * w, h, 1280 - 2 * w, h);
    string[] names = { "REALISTIC", "LOW-POLY, TOON + INK", "LOW-POLY, GOOCH", "LOW-POLY, HATCH", "PAPER CUT-OUTS", "SPLAT CANOPIES" };
    for (int i = 0; i < 6; i++)
        draw::text(names[i], 14 + w * (i % 3), 14 + h * (i / 3), 13, 0x1d2018);
}
