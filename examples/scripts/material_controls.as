// Materials -- Metal, glass and rough surfaces over water under a sky.
// Category: 3D

void configure(Composition &inout c) {
    c.name = "material-controls";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x10131b;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    array<float> instanceVertices, instanceNormals;
    g3d::geometryData("{\"geometry\":\"sphere\",\"radius\":0.12,\"rings\":8,\"slices\":12}", instanceVertices, instanceNormals);
    string bakedPositions = "[", bakedNormals = "[";
    for (int bakedFrame = 0; bakedFrame < 2; ++bakedFrame) {
        float size = bakedFrame == 0 ? 0.78f : 1.18f;
        for (uint vertex = 0; vertex < instanceVertices.length(); ++vertex) {
            if (bakedFrame > 0 || vertex > 0) { bakedPositions += ","; bakedNormals += ","; }
            bakedPositions += instanceVertices[vertex] * size;
            bakedNormals += instanceNormals[vertex];
        }
    }
    bakedPositions += "]"; bakedNormals += "]";
    string world = "{\"camera\":{\"position\":[7,5,10],\"target\":[0,1,0],\"fov\":42,\"focusDistance\":9,\"aperture\":0.04,\"focusTilt\":-3},\"studioEnvironment\":{\"top\":\"#263c62\",\"horizon\":\"#d08c68\",\"bottom\":\"#151923\",\"panels\":5},\"ambient\":0.45,\"cullFloor\":-1,\"lights\":[{\"type\":\"directional\",\"position\":[-3,7,4],\"intensity\":2.4,\"castShadow\":true,\"shadowExtent\":9,\"shadowBias\":0.015},{\"type\":\"spot\",\"position\":[3,5,-2],\"target\":[0,1,0],\"color\":\"#70d7ff\",\"intensity\":1.8,\"innerAngle\":14,\"outerAngle\":28}],\"sky\":{\"enabled\":true,\"zenith\":\"#273b6b\",\"horizon\":\"#e8a070\",\"ground\":\"#242634\",\"curve\":0.4,\"sunDirection\":[-0.4,0.3,-0.8],\"sunColor\":\"#ffe3a3\",\"sunSize\":2,\"sunGlow\":0.8,\"haze\":\"#f49c70\",\"hazeAmount\":0.55,\"stars\":0.03,\"clouds\":0.18,\"seed\":31,\"cloudDrift\":[0.0003,0.0001],\"show\":true},\"water\":{\"preset\":\"calm\",\"level\":-0.2,\"openDepth\":5,\"caustics\":0.3,\"foam\":0.25,\"sunGlint\":1,\"waves\":[{\"direction\":20,\"wavelength\":4.5,\"amplitude\":0.12,\"sharpness\":1.7,\"speed\":0.9},{\"direction\":115,\"wavelength\":2.8,\"amplitude\":0.045,\"sharpness\":2.4,\"speed\":1.3}]},\"lines\":{\"enabled\":true,\"color\":\"#131724\",\"width\":1.5,\"creaseAngle\":32,\"depthThreshold\":0.03,\"wobble\":0.25,\"wobbleScale\":48,\"boilEvery\":2},\"objects\":[{\"geometry\":\"sphere\",\"radius\":0.9,\"rings\":12,\"slices\":18,\"cullRadius\":1.4,\"cullCentre\":[0,0,0],\"position\":[-2,1.1,0],\"material\":{\"color\":\"#dc596e\",\"shading\":\"toon\",\"roughness\":0.52,\"toon\":{\"bands\":4,\"softness\":0.08,\"highlight\":0.5,\"highlightSize\":0.18,\"rim\":\"#ffbd76\",\"rimWidth\":0.45}}},{\"geometry\":\"torus\",\"radius\":0.8,\"tube\":0.28,\"radialSegments\":36,\"sides\":10,\"position\":[0,1,0],\"rotation\":[45,0,0],\"material\":{\"color\":\"#50a9bb\",\"shading\":\"gooch\",\"gooch\":{\"warm\":\"#ffc56e\",\"cool\":\"#3065a5\"},\"clip\":{\"normal\":[1,0,0],\"offset\":0.05,\"edgeWidth\":0.09,\"edgeColor\":\"#edffbd\"}}},{\"geometry\":\"box\",\"width\":1.5,\"height\":1.5,\"depth\":1.5,\"position\":[2,0.9,0],\"material\":{\"color\":\"#d8c7a1\",\"shading\":\"hatch\",\"hatch\":{\"ink\":\"#233045\",\"spacing\":6,\"angle\":38,\"weight\":0.85},\"lineWidth\":0.7,\"dynamicMap\":{\"width\":2,\"height\":2,\"every\":8,\"frames\":[[\"#ed674f\",\"#ffc15e\",\"#273d63\",\"#6fc6b6\"],[\"#6fc6b6\",\"#ed674f\",\"#ffc15e\",\"#273d63\"]]}}}]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
    string swarm = "{\"camera\":{\"position\":[0,2.2,5],\"target\":[0,0.3,0],\"fov\":42},\"ambient\":0.8,\"lights\":[{\"type\":\"directional\",\"position\":[-2,4,3],\"intensity\":1.4}],\"objects\":[{\"geometry\":\"sphere\",\"radius\":0.12,\"rings\":8,\"slices\":12,\"instances\":{\"frames\":2,\"positions\":" + bakedPositions + ",\"normals\":" + bakedNormals + ",\"items\":[";
    for (int i = 0; i < 18; ++i) {
        float theta = i * 0.34906585f + frame * 0.025f;
        if (i > 0) swarm += ",";
        swarm += "{\"position\":[" + cos(theta) * 1.5f + "," + sin(theta * 2) * 0.35f + "," + sin(theta) * 1.5f + "],\"yaw\":" + (theta * 57.29578f) + ",\"scale\":" + (0.75f + 0.2f * sin(theta * 3)) + ",\"frame\":" + (frame * 0.04f + i * 0.1f) + ",\"clipLength\":2}";
    }
    swarm += "]}}]}";
    g3d::draw(swarm, frame, 24, 48, 350, 260);
    draw::text("SCRIPTED GPU INSTANCES", 38, 288, 12, 0xe9e5dc);
    draw::text("BONELESS MESH DATA: " + instanceVertices.length() / 3 + " VERTICES / " + instanceNormals.length() / 3 + " NORMALS", 38, 306, 10, 0xb8c6cc);
    draw::text("MATERIALS / LIGHTING / WATER / SKY CONTROLS", 34, 675, 18, 0xe9e5dc);
}

void paintTexture(int frame) {
    draw::rect(0, 0, 256, 256, 0x121525);
    draw::circle(128, 128, 102, 0x31233e);
    draw::circle(128 + 30 * sin(frame * 0.08f), 122, 66, 0xe66358);
    draw::circle(128, 105, 43, 0xffc16d);
    draw::rect(40, 202, 176, 3, 0x86d5c2);
    draw::text("RAY / SCRIPT", 68, 220, 18, 0xf4ead7);
}
