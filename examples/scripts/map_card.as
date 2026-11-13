// Map -- A country highlighted on an orthographic globe, with distance and bearing.
// Category: Maps

void configure(Composition &inout c) {
    c.name = "map-card";
    c.w = 1280;
    c.h = 720;
    c.fps = 30;
    c.duration = 90;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    double distanceKm = geo::distance(106.8, -6.2, 16.4, 48.2);
    double bearingDeg = geo::bearing(106.8, -6.2, 16.4, 48.2);
    draw::text("One globe, many projections", 112, 82, 38, 0xf0f3f7);
    geo::drawWorld("file:assets/data/geo/countries-110m.geojson", "orthographic", 180, 160, 480, 480,
                   106, -3, frame, "IDN", 0x253b45, 0xffb454, 0xb8c6cc, 15, 0x65717a);
    draw::text("Great-circle distance and bearing are scriptable.", 730, 260, 24, 0x9aa8b7);
    draw::text("DISTANCE • km", 730, 340, 18, 0xb8c6cc);
    draw::roundRect(730, 374, float(360 * clamp01(distanceKm / 16000)), 10, 5, 0xffb454);
    draw::text("INITIAL BEARING • degrees", 730, 420, 18, 0xb8c6cc);
    draw::roundRect(730, 454, float(360 * clamp01(bearingDeg / 360)), 10, 5, 0x70d7ff);
    draw::text("The highlighted geometry follows the orthographic view.",
               730, 610, 20, 0x9aa8b7);
}
