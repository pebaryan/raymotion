// Icons and flags -- Brand marks and country flags from the built-in catalogues.
// Category: Drawing

void configure(Composition &inout c) {
    c.name = "icon-flag-catalog";
    c.w = 960; c.h = 540; c.fps = 30; c.duration = 90;
    c.background = 0x11151e;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    array<string> brands, languages, countries;
    icons::brands(brands);
    icons::languages(languages);
    flags::countries(countries);

    draw::text("VECTOR MARKS / FLAGS", 54, 48, 30, 0xf0f3f7);
    draw::text("Native catalogs, metadata, and reusable glyph paths", 54, 86, 16, 0x9aa8b7);
    draw::roundRect(54, 130, 400, 300, 18, 0x1b2430);
    draw::text("BRAND MARKS", 82, 160, 13, 0x70d7ff);
    icons::draw("youtube", 152, 245, 92, icons::BADGE, icons::ROUNDED, 0xffffff, clamp01(float(frame) / 20));
    icons::draw("github", 290, 245, 92, icons::MONO, icons::CIRCLE, 0xe9e5dc, clamp01(float(frame) / 28));
    draw::text(icons::title("youtube") + "  /  brand RGB " + icons::brandColor("youtube"), 82, 320, 16, 0xe9e5dc);
    draw::text("" + brands.length() + " brands   /   " + languages.length() + " languages", 82, 360, 14, 0x9aa8b7);

    draw::roundRect(484, 130, 422, 300, 18, 0x1b2430);
    draw::text("COUNTRY FLAGS", 512, 160, 13, 0xffc16d);
    flags::draw("id", 615, 245, 142, true, clamp01(float(frame) / 22));
    flags::draw("jp", 790, 245, 142, true, clamp01(float(frame - 8) / 22));
    draw::text(flags::title("id") + "  /  " + flags::title("jp"), 512, 320, 16, 0xe9e5dc);
    draw::text("" + countries.length() + " supported ISO country codes", 512, 360, 14, 0x9aa8b7);
    Path@ glyph = icons::glyph("github").scaled(2, 2).translated(858, 468);
    vg::fill(glyph, 0x70d7ff);
    draw::text("Native glyph Path   /   editable with vg::Path operations", 54, 480, 14, 0x65717a);
}
