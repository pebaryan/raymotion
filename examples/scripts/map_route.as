// Route and fly-to -- A globe flies from Indonesia to Austria while a great-circle route draws itself on.
// Category: Maps

void configure(Composition &inout c) {
    c.name = "map-route";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 180;
    c.background = 0x0d1118;
    scene(0, c.duration, "draw");
}

float ramp(int frame, int from, int to) { return clamp01(float(frame - from) / float(to - from)); }
float smooth(float t) { return t * t * (3 - 2 * t); }

void draw(int frame) {
    geo::GeoLayer@ world = geo::layer("countries");
    geo::GeoGeometry@ indonesia = world.geometry(world.find("IDN"));
    geo::GeoGeometry@ austria = world.geometry(world.find("AUT"));

    // Two views of one globe: each country framed in the same box.
    geo::GeoProjection@ globe = geo::projection("orthographic");
    geo::GeoProjection@ from = geo::focus(globe, indonesia, 340, 110, 600, 500, 120);
    geo::GeoProjection@ to = geo::focus(globe, austria, 340, 110, 600, 500, 190);
    float travel = smooth(ramp(frame, 30, 150));
    geo::GeoProjection@ view = geo::fly(from, to, travel);

    geo::base(view, 1, "{\"ocean\":\"#121b28\",\"land\":\"#263243\",\"graticuleStep\":10}");
    geo::highlight(view, indonesia, 1 - ramp(frame, 40, 70), "{\"fill\":\"#3d8f86\",\"stroke\":\"#a8e6d9\",\"width\":1.5}");
    geo::highlight(view, austria, ramp(frame, 125, 165), "{\"fill\":\"#e25d4e\",\"stroke\":\"#ffd8cf\",\"width\":2}");

    // Jakarta, Dubai, Vienna. The line reaches each stop as the camera does.
    array<double> stops = {106.85, -6.21, 55.27, 25.20, 16.37, 48.21};
    geo::route(view, stops, ramp(frame, 30, 150), "{\"ghost\":\"#ffffff22\",\"arrow\":true,\"head\":8,\"width\":3}");
    geo::markers(view, "[{\"lon\":106.85,\"lat\":-6.21,\"label\":\"Jakarta\"},{\"lon\":55.27,\"lat\":25.20,\"label\":\"Dubai\"},{\"lon\":16.37,\"lat\":48.21,\"label\":\"Vienna\"}]",
                 ramp(frame, 5, 150), "{\"radius\":5,\"color\":\"#ffc45c\",\"textSize\":20,\"bold\":true,\"stagger\":1}");

    array<double> here;
    geo::along(stops, ramp(frame, 30, 150), here);
    draw::text("JAKARTA TO VIENNA", 40, 30, 26, 0xeef2f6);
    draw::text("An illustrative route through Dubai, " + int(geo::length(stops)) + " km along great circles.", 40, 66, 15, 0x8d9aab);
    draw::text("HEADING " + int(here[2]) + " DEG", 40, 664, 16, 0xffc45c);
}
