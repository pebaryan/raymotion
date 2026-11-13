// Voxels and splats -- A voxel model beside a seeded splat cloud.
// Category: 3D

void configure(Composition &inout c) {
    c.name = "voxel-splat-scene";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x0d1118;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    string world = "{\"camera\":{\"position\":[7,6,11],\"target\":[0,1.5,0],\"fov\":40},\"ambient\":0.65,\"lights\":[{\"type\":\"directional\",\"position\":[-4,8,5],\"intensity\":2.1,\"castShadow\":true}],\"objects\":[{\"geometry\":\"plane\",\"width\":14,\"depth\":10,\"position\":[0,-0.02,0],\"material\":{\"color\":\"#1b242d\",\"roughness\":0.95}},{\"voxel\":{\"size\":[24,18,24],\"cellSize\":0.18,\"centre\":true,\"palette\":[\"#523a4d\",\"#c16a58\",\"#e5b764\",\"#69a9a0\"],\"shapes\":[{\"type\":\"box\",\"from\":[5,0,5],\"to\":[18,9,18],\"color\":1},{\"type\":\"box\",\"from\":[3,9,3],\"to\":[20,10,20],\"color\":2},{\"type\":\"line\",\"from\":[3,10,3],\"to\":[11,17,11],\"color\":2},{\"type\":\"line\",\"from\":[20,10,3],\"to\":[11,17,11],\"color\":2},{\"type\":\"line\",\"from\":[3,10,20],\"to\":[11,17,11],\"color\":2},{\"type\":\"line\",\"from\":[20,10,20],\"to\":[11,17,11],\"color\":2},{\"type\":\"box\",\"from\":[10,0,5],\"to\":[13,7,5],\"color\":4},{\"type\":\"box\",\"from\":[8,0,4],\"to\":[15,1,6],\"color\":3}],\"ao\":true,\"aoStrength\":0.7},\"position\":[-2.2,0,0]},{\"splat\":{\"type\":\"sphere\",\"centre\":[2.2,1.5,0],\"radius\":1.25,\"count\":9000,\"seed\":41,\"color\":\"#ffb454\",\"scale\":1,\"opacity\":0.95}}]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
    draw::text("PROCEDURAL VOXEL GRID + DETERMINISTIC SPLAT CLOUD", 36, 675, 19, 0xe8ded1);
}
