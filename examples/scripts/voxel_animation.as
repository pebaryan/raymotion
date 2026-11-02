void configure(Composition &inout c) {
    c.name = "voxel-animation";
    c.w = 960; c.h = 540; c.fps = 30; c.duration = 96;
    c.background = 0x10131b;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    g3d::draw("{\"camera\":{\"position\":[4,3,6],\"target\":[0,0.8,0],\"fov\":42},\"ambient\":0.7,\"lights\":[{\"type\":\"directional\",\"position\":[-3,6,4],\"castShadow\":true}],\"objects\":[{\"geometry\":\"plane\",\"width\":8,\"depth\":8,\"position\":[0,-0.04,0],\"material\":{\"color\":\"#26313b\"}},{\"voxelFrames\":{\"hold\":8,\"frames\":[{\"size\":[8,8,8],\"cellSize\":0.22,\"centre\":true,\"palette\":[\"#e99b55\"],\"shapes\":[{\"type\":\"sphere\",\"centre\":[3.5,3.5,3.5],\"radius\":2.7,\"color\":1}]},{\"size\":[8,8,8],\"cellSize\":0.22,\"centre\":true,\"palette\":[\"#70c3b1\"],\"shapes\":[{\"type\":\"box\",\"from\":[2,2,2],\"to\":[5,5,5],\"color\":1}]},{\"size\":[8,8,8],\"cellSize\":0.22,\"centre\":true,\"palette\":[\"#d66b71\"],\"shapes\":[{\"type\":\"sphere\",\"centre\":[3.5,3.5,3.5],\"radius\":2.7,\"color\":1},{\"type\":\"box\",\"from\":[3,0,3],\"to\":[4,7,4],\"color\":1}]}]}}]}", frame, 0, 0, 960, 540);
    draw::text("HELD, LOOPING VOXEL FRAMES", 32, 500, 18, 0xdce5e4);
}
