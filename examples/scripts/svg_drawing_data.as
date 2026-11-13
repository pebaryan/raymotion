// SVG data -- Shapes read from an SVG file, restyled and drawn on.
// Category: Drawing

void configure(Composition &inout c) {
    c.name = "svg-drawing-data";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11131b;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    SvgDrawing@ source = vg::loadSvg("assets/images/script-lower-third.svg");
    Path@ first = source.shapePath(0);
    array<float> dash;
    source.dash(0, dash);
    array<float> bounds;
    first.bounds(bounds);
    float progress = float(frame) / 90.0f;
    vg::draw(source, 120, 140, 420, 420);
    vg::write(source, 740, 140, 420, 420, progress, 0.5f);
    draw::text("SVG SHAPES: " + source.count() + " • VIEWBOX: " + source.width() + " × " + source.height(), 640, 615, 18, 0xe8e2d8, 1, 0.5f);
    draw::text("FIRST PATH: " + first.length() + " PX • FILL RGBA: " + source.fillColor(0), 640, 640, 14, 0x9ba4ae, 1, 0.5f);
    draw::text("STROKE DASH ENTRIES: " + dash.length(), 640, 670, 14, 0x9ba4ae, 1, 0.5f);
}
