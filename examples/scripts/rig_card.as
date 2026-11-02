uint sampleOutfit(int region, float u, float v) {
    if (region == 0 && abs(u - 0.5f) < 0.035f) return 0xf2c14e;
    if (region == 0 && (v < 0.16f || v > 0.84f)) return 0x26324a;
    return region == 0 ? 0x8d3458 : 0x252d50;
}

void configure(Composition &inout c) {
    c.name = "rig-card";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11131b;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    double seconds = frame / 30.0;
    draw::rect(0, 0, 635, 720, 0x202632);
    string character = "{\"preset\":\"toon\",\"hair\":\"hair.bun\",\"hat\":\"hat.cap\",\"skin\":\"#e7b08d\",\"hairColor\":\"#282538\",\"top\":\"#8d3458\",\"outfitSampleFunction\":\"sampleOutfit\",\"bottom\":\"#252d50\",\"accent\":\"#f0ad52\",\"finish\":\"toon\",\"softness\":0.65,\"eyeSize\":1.15,\"eyeSpacing\":1.1,\"clips\":[{\"name\":\"idle\",\"from\":0},{\"name\":\"keys\",\"from\":1.2,\"fade\":0.2,\"loop\":true,\"keys\":[{\"time\":0,\"joints\":{\"shoulder-R\":[-22,0,42],\"elbow-R\":[-50,0,0]}},{\"time\":0.45,\"joints\":{\"shoulder-R\":[-22,0,65],\"elbow-R\":[-50,0,0]}},{\"time\":0.9,\"joints\":{\"shoulder-R\":[-22,0,42],\"elbow-R\":[-50,0,0]}}]}],\"expressions\":[{\"time\":0,\"name\":\"neutral\"},{\"time\":1.3,\"name\":\"surprised\",\"fade\":0.2},{\"time\":2.2,\"name\":\"happy\",\"fade\":0.3}],\"gaze\":[{\"time\":0,\"x\":0.2,\"y\":0},{\"time\":1.4,\"x\":-0.3,\"y\":-0.2}],\"speech\":[{\"from\":1.1,\"to\":2.7,\"text\":\"selamat malam\"}],\"wear\":[{\"at\":2,\"pop\":0.5,\"look\":{\"preset\":\"toon\",\"hair\":\"hair.bun\",\"skin\":\"#e7b08d\",\"top\":\"#326d63\",\"bottom\":\"#252d50\",\"accent\":\"#f0ad52\",\"finish\":\"toon\"}}],\"view\":{\"originX\":310,\"originY\":640,\"scale\":280,\"yaw\":-5,\"outline\":2.5}}";
    rig::draw(character, seconds, 0, 0, 635, 720);
    draw::text("2D PUPPET • TIMELINE, EXPRESSION, GAZE + SPEECH", 34, 680, 16, 0xb8c6cc);
    string sceneJson = "{\"fps\":30,\"camera\":{\"position\":[0,1.2,4],\"target\":[0,0.9,0]},\"ambient\":0.8,\"lights\":[{\"type\":\"directional\",\"position\":[-2,4,3],\"intensity\":1.5}],\"objects\":[{\"prefab\":{\"type\":\"character\",\"kind\":\"robot\",\"top\":\"#328c9b\",\"bottom\":\"#343a4c\",\"accent\":\"#ffbd5b\",\"finish\":\"plastic\",\"clips\":[{\"name\":\"idle\",\"from\":0},{\"name\":\"cheer\",\"from\":1.6}]},\"position\":[0,0,0],\"rotation\":[0,25,0]}]}";
    g3d::draw(sceneJson, frame, 635, 0, 645, 720);
    draw::text("3D RIG • SCRIPTABLE BUILDS + MOTION", 680, 680, 16, 0xb8c6cc);
}
