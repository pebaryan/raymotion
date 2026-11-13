// Calculus plots -- A vector field, a scatter, a Riemann sum and a tangent line.
// Category: Charts

void configure(Composition &inout c) {
    c.name = "plot-calculus";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11151e;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    string axes = "{\"x\":[-3,3,1],\"y\":[-3,3,1],\"axes\":{\"xGrid\":true,\"yGrid\":true,\"gridColor\":\"#8593a922\",\"numberSize\":14,\"numberColor\":\"#aeb9c9\"}}";
    draw::text("VECTOR FIELD", 48, 32, 15, 0x9eabbd);
    charts::field("-y", "x", 45, 60, 550, 275,
        "{\"x\":[-3,3,1],\"y\":[-3,3,1],\"spacing\":42,\"maxLength\":30,\"slow\":\"#58c4dd\",\"fast\":\"#fc6255\",\"axes\":{\"gridColor\":\"#8593a922\",\"numberSize\":13}}", 0.95f);
    draw::text("SCATTER", 662, 32, 15, 0x9eabbd);
    array<float> points = {-2,-1.2f, -1.4f,1.1f, -0.5f,-0.2f, 0.2f,1.8f, 0.9f,-0.7f, 1.7f,0.6f, 2.4f,2.1f};
    charts::scatter(points, 650, 60, 585, 275, 0.95f, 0x6fc6b6, 6, axes);

    draw::text("MIDPOINT RIEMANN SUM / x²", 48, 375, 15, 0x9eabbd);
    charts::riemann("x^2", 45, 405, 550, 260, -2, 2, 12, 0.5f, 0.95f, 0x58c4dd55,
        "{\"x\":[-3,3,1],\"y\":[0,5,1],\"axes\":{\"xGrid\":true,\"yGrid\":true,\"gridColor\":\"#8593a922\",\"numberSize\":13}}");
    draw::text("AREA + TANGENT / sin(x)", 662, 375, 15, 0x9eabbd);
    charts::area("sin(x)", 650, 405, 585, 260, -3, 3, "", 0.95f, 0x58c4dd44, 0x6fc6b6, 3, axes);
    charts::tangent("sin(x)", 650, 405, 585, 260, 0.6, 260, 0xe7ab59, 3, 0.95f, axes);
}
