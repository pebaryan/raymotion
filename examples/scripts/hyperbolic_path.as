string projectDiskPoint(double x, double y) {
    return "[" + (640.0 + x * 250.0) + "," + (360.0 - y * 250.0) + "]";
}

void configure(Composition &inout c) {
    c.name = "hyperbolic-path-callback";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11131b;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    array<float> points;
    space::hyperbolic::geodesic(-0.72, -0.38, 0.68, 0.46, 96, points);
    Path@ curve = space::hyperbolic::path(points, "projectDiskPoint", false, 250, 30, 780, 660);
    vg::stroke(curve, 0x79d9c4, 4, 0.95f);
    draw::text("HYPERBOLIC GEODESIC TO CLIPPED PATH", 640, 72, 22, 0xf0e5d2, 1, 0.5f);
}
