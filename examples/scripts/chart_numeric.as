// Numeric charts -- Histogram, bubbles, candlesticks and error bars on numeric axes.
// Category: Charts

void configure(Composition &inout c) {
    c.name = "chart-numeric";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11151e;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    array<float> samples = {12, 15, 18, 18, 19, 20, 21, 21, 22, 23, 25, 26, 27, 27, 30, 33, 36, 40};
    array<float> ohlc = {18,22,16,21, 21,24,19,20, 20,23,17,18, 18,25,18,24, 24,28,22,27, 27,29,24,25, 25,31,23,30};
    array<float> xy = {0,8, 1,13, 2,11, 3,20, 4,17, 5,25};
    array<float> errors = {2,3,2,4,3,3};
    draw::text("NUMERIC CHARTS", 42, 26, 22, 0xdfe4eb);
    draw::text("HISTOGRAM", 48, 74, 14, 0x9eabbd);
    charts::histogram(samples, 45, 100, 540, 255, 7, 0x6fc6b6, 0.95f, "{\"x\":[10,42,5],\"y\":[0,8,2]}");
    draw::text("BUBBLES", 660, 74, 14, 0x9eabbd);
    charts::bubbles("[{\"x\":1,\"y\":18,\"size\":16,\"label\":\"A\",\"color\":\"#6fc6b6\"},{\"x\":2,\"y\":32,\"size\":36,\"label\":\"B\",\"color\":\"#e7ab59\"},{\"x\":3.5,\"y\":24,\"size\":24,\"label\":\"C\",\"color\":\"#d7707a\"},{\"x\":5,\"y\":41,\"size\":54,\"label\":\"D\",\"color\":\"#6fc6b6\"}]", 650, 100, 585, 255, 0.95f,
        "{\"x\":[0,6,1],\"y\":[0,50,10],\"maxRadius\":42,\"opacity\":0.78,\"outline\":2,\"textSize\":16}");
    draw::text("CANDLESTICKS", 48, 392, 14, 0x9eabbd);
    charts::candles(ohlc, 45, 420, 540, 245, 0.95f,
        "{\"y\":[14,34,5],\"up\":\"#6fc6b6\",\"down\":\"#d7707a\",\"width\":0.62}");
    draw::text("MEASUREMENTS ± UNCERTAINTY", 660, 392, 14, 0x9eabbd);
    charts::errorBars(xy, errors, 650, 420, 585, 245, 0.95f, 0xe7ab59,
        "{\"x\":[-0.5,5.5,1],\"y\":[0,30,5],\"width\":3,\"cap\":11}");
}
