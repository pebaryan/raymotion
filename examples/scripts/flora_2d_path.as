// Flora 2D path -- A meadow grown to fit a curved hill with a pond cut out, in a gust with motion blur.
// Category: Drawing

// A flora2d meadow takes "path": SVG path data. Curves are flattened finely, so blades root
// inside the curved edge, and with "rule":"evenodd" an inner contour is a hole that stays
// bare. The wind reads time as frameTime(t), so motionBlur() blurs the swaying tips and
// leaves the roots sharp.
void configure(Composition &inout c) {
    c.name = "flora-2d-path";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 180;
    c.background = 0xdde6ec;
    motionBlur(8, 0.8f);
    scene(0, c.duration, "draw");
}

float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

void draw(int frame) {
    float t = frame / 30.0f;
    float rise = clamp01(t / 2.5f);
    draw::rect(0, 0, 1280, 720, 0xdde6ec);
    // the hill: a cubic crest with a pond (an oval contour inside it, so even-odd leaves it bare)
    string pond = "M 380 560 C 380 510 560 510 560 560 C 560 610 380 610 380 560 Z";
    string hill = "M 60 640 C 260 300 640 260 900 420 C 1040 500 1180 540 1240 640 Z " + pond;
    draw::rect(0, 640, 1280, 80, 0x4a6a34);
    vg::fill(vg::parse(hill), 0x5d7a3c, 1, vg::EVEN_ODD);      // the hill's ground under the blades
    vg::fill(vg::parse(pond), 0x6f9fbf);                        // water where the meadow stays bare
    flora2d::draw("{\"fps\":30,\"wind\":{\"strength\":1.3,\"gust\":1,\"gustSpeed\":420,\"gustWidth\":300,\"sway\":0.3},"
        "\"items\":["
        "{\"meadow\":{\"path\":\"" + hill + "\",\"rule\":\"evenodd\",\"preset\":\"tall\",\"height\":64,\"density\":0.45,\"u\":" + rise + ",\"seed\":3}}"
        "]}", frame);
    draw::text("A MEADOW THAT FOLLOWS A CURVE", 34, 34, 18, 0x23301f);
    draw::text("path with a pond cut out (even-odd); gusts and 8-sample motion blur on the tips", 34, 60, 12, 0x3a4a34);
}
