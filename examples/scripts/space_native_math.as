// Space maths -- Surfaces, rotations and projections computed in the script.
// Category: 3D

uint meshFaceColor(float x, float y, float z) {
    if (z > 0.45f) return 0xf2a65a;
    if (x < 0) return 0x38b7c3;
    return 0x4354a3;
}

uint meshCellColor(int row, int col) {
    if ((row + col) % 2 == 0) return 0x38b7c3;
    return 0x4354a3;
}

string nativeGridPoint(float u, float v) {
    float x = (u - 0.5f) * 4;
    float y = (0.5f - v) * 4;
    float z = sin(u * 6.2831853f) * cos(v * 6.2831853f) * 0.45f;
    return "[" + x + "," + y + "," + z + "]";
}

string bendPoint(float x, float y, float z) {
    float angle = y * 0.18f;
    return "[" + (x * cos(angle) - z * sin(angle)) + "," + y + "," + (x * sin(angle) + z * cos(angle)) + "]";
}

double graphHeight(double x, double y) {
    return sin(x) * cos(y) * 0.45;
}

void configure(Composition &inout c) {
    c.name = "space-native-math";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 60;
    c.background = 0x10141d;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    array<float> rotated, projected, polytope, geodesic, tiling, arc, planePoint, meshPoints, meshOrigin, meshScreen, cameraPoint;
    float angle = float(frame) * 2.0f;
    space::rotate4(1, 0, 0, 1, 0, 3, angle, rotated);
    space::project4(rotated[0], rotated[1], rotated[2], rotated[3], 3, projected);
    space::tesseract4(1, polytope);
    space::hyperbolic::geodesic(-0.65, 0.0, 0.65, 0.0, 48, geodesic);
    space::hyperbolic::tiling(7, 3, 45, 0.04, tiling);
    space::sphere::arc(0, 0, 2, 0, 2, 0, 2, 48, arc);
    space::projectOnPlane(1, 2, 3, 0, 0, 0, 0, 0, 1, planePoint);
    space::SpaceMesh@ mesh = space::gridFunction("nativeGridPoint", 28, 20);
    mesh.points(meshPoints);
    space::SpaceMesh@ mapped = space::mappedFunction(mesh, "bendPoint");
    space::SpaceMesh@ graph = space::graphFunction("graphHeight", -3, 3, -3, 3, 24, 24);
    mapped.point(0, 0, meshOrigin);
    space::SpaceView@ view = space::tiltedView(965, 385, 48, -35, -58, 0.2f);
    view.project(projected[0], projected[1], projected[2], meshScreen);
    view.toView(projected[0], projected[1], projected[2], cameraPoint);

    string json = "{\"view\":{\"mode\":\"tilted\",\"scale\":95,\"yaw\":-30,\"pitch\":-58,\"perspective\":0.2},\"primitives\":[{\"type\":\"arrow\",\"from\":[0,0,0],\"to\":[" + projected[0] + "," + projected[1] + "," + projected[2] + "],\"color\":\"#f2a65a\"},{\"type\":\"label\",\"at\":[0,0,0],\"text\":\"4D ROTATION -> 3D PROJECTION\",\"size\":22}]}";
    space::draw(json, frame, 0, 0, 1280, 720);
    space::drawMesh(mapped, view, "{\"colorFunction\":\"meshFaceColor\",\"back\":\"#ffb454\",\"wire\":0.8,\"wireColor\":\"#b5ecf0\"}");
    space::SpaceView@ cellView = space::tiltedView(320, 385, 48, -35, -58, 0.2f);
    space::drawMesh(graph, cellView, "{\"cellColorFunction\":\"meshCellColor\",\"back\":\"#ffb454\",\"wire\":0.8}");
    draw::text("SPACE NATIVE MATH", 640, 50, 25, 0xe6e9ef, 1, 0.5f);
    draw::text("tesseract vertices / edges: " + polytope.length() + " floats", 640, 620, 16, 0xa9bac8, 1, 0.5f);
    draw::text("hyperbolic points: " + geodesic.length() / 2 + "   sphere arc points: " + arc.length() / 3, 640, 650, 16, 0xa9bac8, 1, 0.5f);
    draw::text("tile data: " + tiling.length() + " floats   plane z: " + planePoint[2] + "   mesh: " + mapped.pointCount() + "   view scale@p: " + view.scaleAt(projected[0], projected[1], projected[2]), 640, 680, 16, 0xa9bac8, 1, 0.5f);
}
