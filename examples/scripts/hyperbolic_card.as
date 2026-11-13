// Hyperbolic tiling -- A tiling of the Poincare disk with a geodesic.
// Category: 3D

void configure(Composition &inout c) {
    c.name = "hyperbolic-card";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float a = frame / 89.0f * 70.0f;
    string json = "{\"view\":{\"mode\":\"flat\",\"scale\":280},\"hyperbolic\":{\"p\":7,\"q\":3,\"maxTiles\":95,\"minSize\":0.008,\"even\":\"#19566b\",\"odd\":\"#ad6847\",\"edge\":\"#e8d9bd\",\"edgeWidth\":0.6,\"geodesics\":[{\"from\":[-0.8,0],\"to\":[0.65,0.45],\"samples\":80,\"color\":\"#ffc35c\",\"width\":2.5},{\"from\":[0,0],\"to\":[0.5,0.2],\"samples\":80,\"color\":\"#f1eee7\",\"width\":1.5}]}}";
    space::draw(json, frame, 0, 0, 1280, 720);
    draw::text("HYPERBOLIC TILING • POINCARE DISK", 40, 675, 18, 0xb8c6cc);
}
