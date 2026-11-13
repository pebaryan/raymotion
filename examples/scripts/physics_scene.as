// Rope and cloth -- A cloth hung from a rope, simulated the same on every render.
// Category: Physics and particles

void configure(Composition &inout c) {
    c.name = "physics-scene";
    c.w = 1280;
    c.h = 720;
    c.fps = 30;
    c.duration = 90;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    g3d::draw("{\"camera\":{\"position\":[3,2.4,5],\"target\":[0,0.6,0]},\"lights\":[{\"type\":\"directional\",\"position\":[3,5,3],\"castShadow\":true}],\"objects\":[{\"geometry\":\"plane\",\"width\":5,\"depth\":5,\"position\":[0,0,0],\"material\":{\"color\":\"#17232d\",\"roughness\":0.9}},{\"physics\":{\"kind\":\"rope\",\"from\":[-1,1.6,0],\"to\":[1,1.6,0],\"segments\":36,\"radius\":0.035,\"pinStart\":true,\"pinEnd\":true,\"bendEnabled\":true,\"bend\":0.18,\"params\":{\"gravity\":[0,-9.81,0],\"damping\":0.008,\"substeps\":5,\"iterations\":12},\"colliders\":[{\"type\":\"plane\",\"point\":[0,0,0],\"normal\":[0,1,0]}]},\"material\":{\"color\":\"#ffb454\",\"roughness\":0.4}},{\"physics\":{\"kind\":\"cloth\",\"cols\":28,\"rows\":20,\"spacing\":0.065,\"origin\":[-0.9,1.3,-0.2],\"axisU\":[1,0,0],\"axisV\":[0,0,-1],\"pinCorners\":true,\"stretch\":0.92,\"bend\":0.15,\"wind\":{\"direction\":[1,0,0],\"strength\":2.4,\"gust\":0.8,\"gustHz\":0.6,\"seed\":19}},\"material\":{\"color\":\"#70d7ff\",\"doubleSided\":true,\"roughness\":0.75}}]}",
               frame, 80, 60, 1120, 600);
    draw::text("ROPE + CLOTH • FRAME DETERMINISTIC", 44, 665, 18, 0xb8c6cc);
}
