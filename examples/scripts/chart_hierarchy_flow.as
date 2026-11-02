void configure(Composition &inout c) {
    c.name = "chart-hierarchy-flow";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11151e;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    string budget = "Operations/Field, 22\nOperations/Studio, 16\nOperations/Cloud, 11\nResearch/Methods, 14\nResearch/Prototypes, 9";
    string flows = "from, to, value\nSolar, Grid, 12\nWind, Grid, 9\nGrid, Homes, 13\nGrid, Industry, 8\nHomes, Storage, 3\nStorage, Industry, 2";
    draw::text("HIERARCHY + FLOW STRUCTURES", 45, 30, 22, 0xdfe4eb);
    draw::text("SUNBURST", 45, 70, 14, 0x9eabbd);
    charts::sunburst(budget, 245, 250, 165, 0.96f,
        "{\"inner\":0.25,\"gapDeg\":0.8,\"labels\":true,\"textSize\":14,\"textColor\":\"#18202b\"}");
    draw::text("TREEMAP", 455, 70, 14, 0x9eabbd);
    charts::treemap(budget, 455, 95, 775, 255, 0.96f,
        "{\"gap\":5,\"labels\":true,\"values\":true,\"textSize\":18,\"textColor\":\"#18202b\"}");
    draw::text("SANKEY", 455, 385, 14, 0x9eabbd);
    charts::sankey(flows, 455, 410, 775, 250, 0.96f,
        "{\"nodeWidth\":18,\"nodePad\":22,\"linkAlpha\":0.4,\"textSize\":15}");
    draw::text("CHORD", 45, 445, 14, 0x9eabbd);
    charts::chord(flows, 245, 570, 125, 0.96f,
        "{\"arcWidth\":18,\"padDeg\":5,\"ribbonAlpha\":0.38,\"textSize\":12}");
}
