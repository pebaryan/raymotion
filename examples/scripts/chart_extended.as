// Gauge and parallel coordinates -- A radial gauge beside a multivariate profile.
// Category: Charts

void configure(Composition &inout c) {
    c.name = "chart-extended";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11151e;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    draw::text("SYSTEM HEALTH / MULTIVARIATE PROFILE", 50, 42, 22, 0xdfe4eb);
    charts::gauge(82, 210, 260, 145, 0.94f,
        "{\"range\":[0,100],\"title\":\"UPTIME\",\"unit\":\"%\",\"fill\":\"#6fc6b6\",\"sweepDeg\":250,\"thickness\":0.18,\"zones\":[{\"threshold\":50,\"color\":\"#d7707a\"},{\"threshold\":75,\"color\":\"#e7ab59\"}]} ");
    draw::text("Illustrative monitoring values", 50, 450, 16, 0x9eabbd);

    charts::ChartData@ profile = charts::table();
    charts::category(profile, "Latency");
    charts::category(profile, "Throughput");
    charts::category(profile, "Errors");
    charts::category(profile, "Saturation");
    array<float> edge = {18, 92, 8, 36};
    array<float> core = {35, 72, 14, 60};
    array<float> cache = {12, 58, 3, 26};
    charts::series(profile, "Edge", edge, 0xe7ab59);
    charts::series(profile, "Core", core, 0x6fc6b6);
    charts::series(profile, "Cache", cache, 0xd7707a);
    charts::parallel(profile, 440, 130, 790, 480, 0.95f,
        "{\"colorBy\":1,\"colors\":\"viridis\",\"lineWidth\":3,\"lineAlpha\":0.66,\"highlight\":1,\"axisColor\":\"#768398\",\"textColor\":\"#dfe4eb\",\"textSize\":17}");
}
