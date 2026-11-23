// Cube skybox -- A sky given as six cube faces: it is the backdrop, lights a chrome and a matte ball, and the camera turns through it.
// Category: 3D

// "environment" takes an image path (one equirectangular picture) or an object:
//   {"faces": [+x, -x, +y, -y, +z, -z files]}   six faces, as skybox packs ship them
//   {"cross": "sky-cross.png"}                  one 4x3 horizontal cross (+y above +z, -y below)
// A pack made for OpenGL or three.js works as it is (the +x and -x faces swap sides for the
// right-handed world, so the sky is not mirrored). Faces must be square and the same size. To use your own sky, point the six paths at your files. These
// six are 512 px faces drawn once for this example.
string faces() {
    string dir = "assets/textures/skybox-example/";
    return "[\"" + dir + "px.png\",\"" + dir + "nx.png\",\"" + dir + "py.png\",\"" + dir + "ny.png\","
           "\"" + dir + "pz.png\",\"" + dir + "nz.png\"]";
}

void configure(Composition &inout c) {
    c.name = "cube-skybox";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 240;
    c.background = 0x000000;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float t = frame / 240.0f;
    float yaw = -0.42f - 0.7f * t;// a slow turn across the sun and the two balls (radians)
    float tx = 8 * cos(yaw), tz = 8 * sin(yaw);
    string world = "{\"camera\":{\"position\":[0,1.2,0],\"target\":[" + tx + ",1.9," + tz + "],\"fov\":44},"
                   "\"environment\":{\"faces\":" + faces() + "},\"showEnvironment\":true,\"environmentIntensity\":1,"
                   "\"objects\":["
                   "{\"geometry\":\"sphere\",\"radius\":0.7,\"position\":[4.1,1.2,-2.8],\"material\":{\"color\":\"#ffffff\",\"metalness\":1,\"roughness\":0.04}},"
                   "{\"geometry\":\"sphere\",\"radius\":0.7,\"position\":[2.8,1.2,-5.3],\"material\":{\"color\":\"#d8d0c4\",\"metalness\":0,\"roughness\":0.9}}]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
    draw::text("A SKY IN SIX FACES", 34, 650, 18, 0xf4ead7);
    draw::text("the same six pictures are the backdrop, the reflection and the light on the matte ball", 34, 676, 12, 0xe8dcc8);
}
