void configure(Composition &inout c) {
    c.name = "space-card";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float u = frame / 89.0f;
    string json = "{\"view\":{\"mode\":\"tilted\",\"scale\":86,\"yaw\":-35,\"pitch\":-59,\"perspective\":0.25},\"axes\":{\"x\":{\"min\":-4,\"max\":4},\"y\":{\"min\":-4,\"max\":4},\"z\":{\"min\":-4,\"max\":4},\"gridXY\":true,\"grow\":[1,1," + u + "]},\"surfaces\":[{\"kind\":\"glueTorus\",\"roll\":" + u + ",\"bend\":" + u + ",\"width\":3,\"height\":6,\"color\":\"#267e91\",\"back\":\"#ffb454\",\"wire\":0.8,\"wireColor\":\"#b5ecf0\"}],\"primitives\":[{\"type\":\"arrow\",\"from\":[-3,-2,0],\"to\":[2,2,2],\"color\":\"#ffb454\",\"width\":4},{\"type\":\"label\",\"at\":[0,0,3],\"text\":\"SHEET -> TORUS\",\"color\":\"#f0eee8\",\"size\":24}]}";
    space::draw(json, frame, 0, 0, 1280, 720);
    draw::text("SPACE GEOMETRY • SURFACES, AXES, DEPTH SORTING", 40, 675, 18, 0xb8c6cc);
}
