void configure(Composition &inout c) {
    c.name = "feature-atlas";
    c.w = 1280;
    c.h = 720;
    c.fps = 30;
    c.duration = 180;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    draw::text("Raymotion feature atlas", 56, 48, 36, 0xffffff);
    txt::reveal("SCRIPTABLE MOTION", 58, 96, 19, 0x70d7ff, frame, 1.5, 12, 16);
    double springProgress = anim::spring(frame, 0, 1, 1, 130, 14, 30, 0, 45, false);
    draw::roundRect(58, 134, float(180 * clamp01(springProgress)), 7, 3, 0xffd166);
    array<float> itemPairs = { 0, 1, 0, 2, 150, 0 };
    array<float> cells;
    layout::row(58, 158, 400, 32, itemPairs, 8, cells);
    for (uint i = 0; i < cells.length(); i += 4)
        draw::roundRect(cells[i], cells[i + 1], cells[i + 2], cells[i + 3], 4, 0x273141);

    Path@ curve = vg::path();
    curve.moveTo(70, 210);
    curve.cubicTo(190, 90, 280, 330, 410, 190);
    vg::stroke(curve, 0x70d7ff, 8);
    draw::star(365, 190, 18, 8, 5, -90, 0xffd166);

    charts::ChartData@ data = charts::table();
    charts::category(data, "A");
    charts::category(data, "B");
    charts::category(data, "C");
    array<float> values = { 32, 58, 46 };
    charts::series(data, "Example", values, 0x7bd88f);
    charts::box(data, 60, 380, 360, 250);
    charts::legend(data, false);
    charts::bars(data, 60, 380, 360, 250, clamp01(float(frame) / 40));

    diagram::draw("graph LR\\n  source[Source] --> proof[Proof] --> result[Result]",
                  "mermaid", 470, 130, 730, 240, frame, "dark", 5, 14);
    chem::drawSmiles("CC(=O)O", 500, 400, 330, 220, frame, 2, 12);
    geo::drawWorld("countries", "orthographic", 850, 420, 180, 190,
                   106, -3, frame * 0.5f, "IDN", 0x253b45, 0xffb454, 0xb8c6cc, 15, 0x65717a);
    g3d::draw("{\"camera\":{\"position\":[3,2,5],\"target\":[0,0,0]},\"lights\":[{\"type\":\"directional\",\"position\":[3,5,2],\"castShadow\":true}],\"objects\":[{\"geometry\":\"torus\",\"radius\":1,\"tube\":0.3,\"material\":{\"color\":\"#70d7ff\",\"metalness\":0.6},\"rotation\":[45,0,0]}]}",
               frame, 1040, 420, 160, 190);
}
