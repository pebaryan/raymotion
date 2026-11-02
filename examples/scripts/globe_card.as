void configure(Composition &inout c) {
    c.name = "globe-card";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float reveal = frame / 89.0f;
    string json = "{\"camera\":{\"position\":[0,0,3.2],\"target\":[0,0,0]},\"fps\":30,\"ambient\":0.75,\"lights\":[{\"type\":\"directional\",\"position\":[-2,3,4],\"intensity\":1.4}],\"globe\":{\"radius\":1,\"mapWidth\":512,\"segments\":80,\"lookCamera\":true,\"centerLon\":118,\"centerLat\":-3,\"cameraDistance\":3.1,\"ocean\":\"#102c44\",\"land\":\"#718d72\",\"border\":\"#c6cfb6\",\"graticule\":30,\"clouds\":true,\"cloudCover\":0.38,\"cloudScale\":5,\"cloudEvolve\":0.025,\"cloudEvery\":3,\"storms\":[{\"center\":[130,-10],\"radiusKm\":700,\"strength\":0.65,\"south\":true}],\"routes\":[{\"from\":[106.8,-6.2],\"to\":[115.2,-8.7],\"height\":0.28,\"width\":0.009,\"headFunction\":\"islandRouteHead\",\"tailFunction\":\"islandRouteTail\",\"color\":\"#ffbd5b\",\"segments\":50},{\"from\":[106.8,-6.2],\"to\":[139.7,-4.3],\"height\":0.32,\"width\":0.007,\"head\":" + reveal + ",\"color\":\"#ed795d\",\"segments\":60}]}}";
    g3d::draw(json, frame, 0, 0, 1280, 720);
    draw::text("INDONESIAN ARCHIPELAGO • GLOBE ROUTES + CYCLONE CLOUDS", 40, 675, 18, 0xb8c6cc);
}


float islandRouteHead(int frame) {
    return clamp01(double(frame) / 89.0);
}

float islandRouteTail(int frame) {
    return clamp01(double(frame - 12) / 89.0);
}
