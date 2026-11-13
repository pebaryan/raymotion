// Images -- One picture drawn contained, then cover-fitted, aligned and tinted.
// Category: Drawing

void configure(Composition &inout c) {
    c.name = "image-style-card";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 30;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    media::ImageAsset@ source = media::image("assets/media/fractal.jpg");
    draw::imageStyled("assets/media/fractal.jpg", 80, 80, 500, 480, "contain", 0.5f, 0.5f, 0xffffff, 1);
    draw::imageStyled("assets/media/fractal.jpg", 700, 80, 500, 480, "cover", 0.65f, 0.4f, 0x89ddff, 0.92f);
    draw::text("CONTAIN • CENTERED", 80, 610, 20, 0xe9e5dc);
    draw::text("COVER • ALIGNED + TINTED", 700, 610, 20, 0xe9e5dc);
    draw::text("SOURCE: " + source.width() + " × " + source.height() + (source.isVector() ? " VECTOR" : " RASTER"), 80, 650, 15, 0x9ba4ae);
}
