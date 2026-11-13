// 3D particles -- An emitter in a 3D scene, configured from the script.
// Category: Physics and particles

void configure(Composition &inout c) {
    c.name = "particle3d-authoring";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x0c1018;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    string config = "{\"seed\":431,\"burst\":80,\"rate\":3.5,\"life\":[28,58],\"area\":\"disc\",\"extent\":[0.38,0.2,0.38],\"originFunction\":\"flameSource\",\"speed\":[0.015,0.065],\"direction\":[0,1,0],\"spread\":24,\"gravity\":[0,0.0002,0],\"turbulence\":0.018,\"look\":\"glow\",\"size\":[0.07,0.18],\"sizeEnd\":0.08,\"colors\":[\"#ff6638\",\"#ffc45b\"],\"overLife\":[\"#fff0bd\",\"#ff783f\",\"#50284c\"],\"additive\":true}";
    string world = "{\"camera\":{\"position\":[3,2.8,6],\"target\":[0,1,0],\"fov\":42},\"ambient\":0.35,\"lights\":[{\"type\":\"directional\",\"position\":[-3,6,4],\"intensity\":1.8,\"castShadow\":true}],\"objects\":[{\"geometry\":\"plane\",\"width\":5,\"depth\":5,\"material\":{\"color\":\"#19232d\",\"roughness\":0.9}},{\"particles\":\"fire\",\"radius\":0.42,\"seed\":431,\"particleConfig\":" + config + ",\"position\":[0,0,0]}]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
    draw::text("3D EMITTER • SCRIPT CONFIGURED", 640, 44, 21, 0xe8e2d8, 1, 0.5f);
    array<float> data;
    uint alive = fx::count3D(config, frame);
    bool hasParticle = fx::particle3D(config, frame, 0, data);
    array<float> tail;
    bool hasTail = hasParticle && fx::positionAt3D(config, frame, 0, 4, tail);
    draw::text("LIVE: " + alive + (hasTail ? "  TRAIL SAMPLE: " + tail[0] + ", " + tail[1] + ", " + tail[2] : ""), 640, 680, 14, 0xb8c6cc, 1, 0.5f);
}

string flameSource(double birthFrame) {
    return "[" + (0.3f * sin(birthFrame * 0.04f)) + ",0,0]";
}
