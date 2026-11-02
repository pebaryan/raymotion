void configure(Composition &inout c) {
    c.name = "chart-authoring";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11151e;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    charts::ChartData@ samples = charts::table();
    for (int i = 0; i < 12; ++i) charts::category(samples, "Sample " + (i + 1));
    array<float> control = {10, 12, 13, 14, 16, 18, 19, 22, 23, 27, 29, 34};
    array<float> batchA = {16, 18, 17, 20, 21, 23, 22, 25, 27, 28, 30, 32};
    array<float> batchB = {8, 9, 11, 10, 12, 13, 14, 15, 17, 18, 20, 22};
    charts::series(samples, "Control", control);
    charts::series(samples, "Batch A", batchA);
    charts::series(samples, "Batch B", batchB);
    charts::style(samples, "{\"y\":[0,36,10],\"palette\":[\"#e7ab59\",\"#6fc6b6\",\"#d7707a\"],\"values\":false,\"legend\":false,\"textSize\":18,\"textColor\":\"#e3e7ee\",\"axes\":{\"color\":\"#768398\",\"numberColor\":\"#aeb9c9\",\"gridColor\":\"#8593a922\",\"yGrid\":true,\"xNumbers\":false,\"arrows\":false,\"framed\":true,\"ticks\":false}}");
    charts::violin(samples, 55, 105, 570, 510, 0.92f);
    charts::style(samples, "{\"box\":[675,105,550,510],\"y\":[0,36,10],\"palette\":[\"#e7ab59\",\"#6fc6b6\",\"#d7707a\"],\"values\":true,\"decimals\":0,\"legend\":false,\"textSize\":19,\"textColor\":\"#e3e7ee\",\"axes\":{\"color\":\"#768398\",\"numberColor\":\"#aeb9c9\",\"gridColor\":\"#8593a922\",\"yGrid\":true,\"xNumbers\":false,\"arrows\":false,\"framed\":true,\"ticks\":false}}");
    charts::boxplot(samples, 675, 105, 550, 510, 0.92f);
    draw::text("DISTRIBUTIONS / VIOLIN + TUKEY BOX PLOTS", 55, 665, 19, 0xdfe4eb);
}
