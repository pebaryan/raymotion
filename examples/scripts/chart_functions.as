uint contourColor(float t) {
    if (t < 0.5f) return 0x2457a6;
    return 0xf08a45;
}

double authoredGraph(double x) {
    if (x < 0) return sin(x);
    return 0.65 * cos(2 * x);
}

string authoredParametric(double t) {
    float radius = 1 + 0.3f * cos(4 * float(t));
    return "[" + radius * cos(float(t)) + "," + radius * sin(float(t)) + "]";
}

double authoredContour(double x, double y) {
    return sin(x) * cos(y) + 0.15 * sin(x * y);
}

void configure(Composition &inout c) {
    c.name = "chart-functions";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11151e;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    draw::text("FUNCTION PLOTS / EXPRESSION FORMULAS", 42, 24, 22, 0xdfe4eb);
    string axes = "{\"function\":\"authoredGraph\",\"x\":[-6.28,6.28,3.14],\"y\":[-1.5,1.5,0.5],\"axes\":{\"xGrid\":true,\"yGrid\":true,\"gridColor\":\"#8593a922\",\"numberSize\":15,\"numberColor\":\"#aeb9c9\",\"color\":\"#768398\"}}";
    draw::text("script function graph", 48, 68, 15, 0x9eabbd);
    charts::graph("sin(x)", 45, 92, 555, 250, axes, 0.97f, 0x6fc6b6, 3.5f);
    draw::text("PARAMETRIC / ROSE", 660, 68, 15, 0x9eabbd);
    charts::parametric("", "", 650, 92, 585, 250, 0.97f,
        "{\"function\":\"authoredParametric\",\"x\":[-2.5,2.5,1],\"y\":[-2.5,2.5,1],\"domain\":[0,6.283185307],\"samples\":500,\"axes\":{\"gridColor\":\"#8593a922\",\"numberSize\":15}}", 0xe7ab59, 3.5f);
    draw::text("POLAR / FIVE LOBES", 48, 385, 15, 0x9eabbd);
    charts::polar("1+0.6*cos(5*theta)", 45, 410, 555, 250, 0.97f,
        "{\"x\":[-2,2,1],\"y\":[-2,2,1],\"domain\":[0,6.283185307],\"samples\":700,\"axes\":{\"gridColor\":\"#8593a922\",\"numberSize\":15}}", 0xd7707a, 3.5f);
    draw::text("CONTOUR / sin(x) cos(y)", 660, 385, 15, 0x9eabbd);
    charts::contour("", 650, 410, 585, 250,
        "{\"function2D\":\"authoredContour\",\"x\":[-3.14,3.14,1.57],\"y\":[-3.14,3.14,1.57],\"levels\":[-0.8,-0.4,0,0.4,0.8],\"cell\":8,\"colors\":\"diverging\",\"fill\":true,\"fillAlpha\":0.62,\"labels\":false,\"lineWidth\":1.4}", 0.97f);
}
