// Prefabs -- People, a room and props placed from the prefab library.
// Category: Characters

void configure(Composition &inout c) {
    c.name = "prefab-card";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    g3d::draw("{\"camera\":{\"position\":[5,4,8],\"target\":[0,1,0],\"fov\":42},\"ambient\":0.7,\"lights\":[{\"type\":\"directional\",\"position\":[-3,7,5],\"intensity\":2,\"castShadow\":true}],\"objects\":[{\"geometry\":\"plane\",\"width\":9,\"depth\":7,\"material\":{\"color\":\"#28313a\",\"roughness\":0.95}},{\"prefab\":{\"type\":\"human\",\"kind\":\"human-resident\"},\"position\":[-1.3,0,0],\"scale\":[1.2,1.2,1.2]},{\"prefab\":{\"type\":\"human\",\"kind\":\"human-construction-worker\"},\"position\":[1.3,0,0]},{\"prefab\":{\"type\":\"interior\",\"kind\":\"office-desk\"},\"position\":[0,0,-1.2]},{\"prefab\":{\"type\":\"computer\",\"kind\":\"pc-case\"},\"position\":[0,0,-1.2]}]}", frame, 40, 25, 1200, 650);
    draw::text("SCRIPTABLE HUMAN + INTERIOR + COMPUTER PREFABS", 40, 675, 18, 0xb8c6cc);
}
