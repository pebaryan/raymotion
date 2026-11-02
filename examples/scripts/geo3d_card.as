void configure(Composition &inout c) {
    c.name = "geo3d-card";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float morph = frame < 45 ? frame / 45.0f : 1.0f;
    string json = "{\"camera\":{\"position\":[0,1.4,7.2],\"target\":[0,0,0],\"fov\":42},\"ambient\":0.7,\"lights\":[{\"type\":\"directional\",\"position\":[-2,4,4],\"intensity\":1.5}],\"geo3d\":{\"projection\":\"equalEarth\",\"centerLon\":118,\"centerLat\":-3,\"radius\":1,\"morph\":" + morph + ",\"mapWidth\":512,\"ocean\":\"#102c44\",\"land\":\"#54786a\",\"border\":\"#a7b8aa\",\"graticule\":30,\"values\":{\"IDN\":280,\"AUS\":26,\"IND\":420,\"JPN\":188,\"USA\":92},\"choropleth\":{\"low\":\"#265b78\",\"high\":\"#ffc05b\"},\"marks\":{\"kind\":\"column\",\"size\":0.06,\"height\":0.55,\"colorFunction\":\"geoPalette\",\"stagger\":0.2,\"growFunction\":\"geoReveal\",\"valueFunction\":\"adjustMarkValue\"},\"extrusions\":{\"height\":0.22,\"low\":\"#315b78\",\"high\":\"#f5a84a\",\"growFunction\":\"geoReveal\",\"valueFunction\":\"adjustExtrusionValue\"},\"flows\":[{\"from\":[106.8,-6.2],\"to\":[139.7,-4.3],\"value\":3,\"color\":\"#ffbd5b\"},{\"from\":[106.8,-6.2],\"to\":[77.2,28.6],\"value\":2,\"color\":\"#ee795d\"}],\"flowHeight\":0.4,\"flowWidth\":0.012,\"flowHeadFunction\":\"geoReveal\",\"flowTailFunction\":\"geoTail\",\"progress\":1}}";
    g3d::draw(json, frame, 0, 0, 1280, 720);
    draw::text("GLOBE -> MAP • CHOROPLETH, MARKS, EXTRUSIONS + FLOWS", 40, 675, 18, 0xb8c6cc);
}


float geoReveal(int frame) {
    return clamp01(double(frame) / 72.0);
}

float geoTail(int frame) {
    return clamp01(double(frame - 15) / 72.0);
}


uint geoPalette(float t) {
    if (t < 0.34f) return 0x50b8d2;
    if (t < 0.68f) return 0x77c58b;
    return 0xffbd5b;
}

double adjustMarkValue(int frame, double baseValue, double longitude, double latitude, string key, uint rgba) {
    return baseValue * (1.0 + 0.08 * sin(double(frame) / 18.0 + longitude / 24.0));
}

double adjustExtrusionValue(int frame, double baseValue, string key) {
    return baseValue * (0.85 + 0.15 * cos(double(frame) / 21.0));
}
