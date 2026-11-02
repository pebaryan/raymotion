void configure(Composition &inout c) {
    c.name = "code-card";
    c.w = 1280;
    c.h = 720;
    c.fps = 30;
    c.duration = 120;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    draw::text("Code is part of the picture", 112, 88, 38, 0xf0f3f7);
    code::draw("double distance(double lon, double lat) {\n    return greatCircle(lon, lat);\n}",
               "cpp", 112, 190, 28, "dark", frame, frame * 1.6, -1, true,
               "geography.cpp", true);
    draw::text("Syntax color, line reveal and the caret are frame driven.",
               112, 535, 22, 0x8e9baa);
}
