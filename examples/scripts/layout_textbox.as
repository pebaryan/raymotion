// Layout and wrapped text -- Measured, wrapped text placed with the layout helpers.
// Category: Text

void configure(Composition &inout c) {
    c.name = "layout-textbox";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11151e;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    array<float> viewport = {55, 65, 1170, 590};
    array<float> inset;
    layout::inset(viewport, 20, 24, 20, 24, inset);
    array<float> card;
    layout::place(inset, 520, 390, ALIGN_CENTER, ALIGN_CENTER, card);
    array<float> cardCenter;
    layout::center(card, cardCenter);
    draw::roundRect(card[0], card[1], card[2], card[3], 16, 0x202936);
    draw::text("LAYOUT + WRAPPED TEXT", card[0] + 24, card[1] + 22, 21, 0xe7ab59);
    string copy = "Raymotion scripts can measure and place real text blocks. Word wrapping respects the available width, keeps UTF-8 characters intact, and supports per-line alignment. Use the returned measurements to compose cards, labels, and captions from the same layout rules as native code.";
    array<float> measured;
    draw::textBox(copy, card[0] + 24, card[1] + 76, card[2] - 48, card[3] - 106,
        "{\"size\":22,\"lineHeight\":1.35,\"spacing\":0,\"color\":\"#dfe4eb\",\"align\":\"start\",\"valign\":\"start\",\"face\":\"ui\"}");
    layout::measureText(copy, 22, card[2] - 48, measured,
        "{\"lineHeight\":1.35,\"color\":\"#dfe4eb\",\"face\":\"ui\"}");
    array<string> lines;
    layout::wrapText(copy, 22, card[2] - 48, lines,
        "{\"lineHeight\":1.35,\"color\":\"#dfe4eb\",\"face\":\"ui\"}");
    draw::text("MEASURED BLOCK: " + measured[0] + " × " + measured[1] + " / " + lines.length() + " lines", 330, 585, 17, 0x9eabbd);
    draw::circle(cardCenter[0], cardCenter[1], 4, 0xd7707a);
}
