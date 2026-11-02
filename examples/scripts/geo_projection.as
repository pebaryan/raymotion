void configure(Composition &inout c) {
    c.name = "geo-projection";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11151e;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    string geoJson = "{\"type\":\"FeatureCollection\",\"features\":[{\"type\":\"Feature\",\"id\":\"west\",\"properties\":{\"name\":\"West Arc\"},\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[[[105,-43],[132,-44],[139,-36],[130,-25],[118,-18],[112,-27],[105,-43]]]}},{\"type\":\"Feature\",\"id\":\"east\",\"properties\":{\"name\":\"East Arc\"},\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[[[139,-39],[154,-40],[153,-29],[146,-21],[139,-27],[139,-39]]]}}]}";
    geo::GeoLayer@ layer = geo::parse(geoJson);
    geo::GeoProjection@ globe = geo::projection("orthographic");
    globe.rotate(132, -30, -8);
    globe.fit(layer, 55, 95, 1170, 530, 0.08f);
    geo::GeoProjection@ flat = geo::projection("equalEarth");
    flat.fit(layer, 55, 95, 1170, 530, 0.08f);
    geo::GeoProjection@ view = geo::mix(globe, flat, float(frame) / 90.0f);

    draw::text("PROJECTION BLEND / GLOBE â†’ EQUAL EARTH", 54, 30, 21, 0xdfe4eb);
    Path@ grid = view.graticule(15, 10);
    vg::stroke(grid, 0x8190a544, 0.8f, 0.8f);
    geo::drawFeature(layer, 0, view, 0x347f89, 0xd3e3e6, 1.4f, 1);
    Path@ eastFill = view.path(layer, 1);
    Path@ eastEdges = view.lines(layer, 1);
    vg::fill(eastFill, 0xe7ab59, vg::EVEN_ODD);
    vg::stroke(eastEdges, 0xffe1a8, 1.4f);
    vg::stroke(view.outline(), 0x91a6b866, 1.2f);
    array<float> point;
    bool visible = view.project(132, -30, point);
    if (visible) draw::circle(point[0], point[1], 5, 0xff785f);
    array<float> backSidePoint;
    bool backSideVisible = globe.project(-40, 0, backSidePoint);
    globe.projectUnclipped(-40, 0, backSidePoint);
    array<double> customRing = {126, -35, 133, -35, 132, -30};
    array<int> ringEnds = {3};
    geo::GeoGeometry@ custom = geo::geometry();
    geo::GeoGeometry@ triangle = geo::polygon(customRing, ringEnds);
    custom.append(triangle);
    custom.append(geo::point(130, -32));
    array<double> customBounds;
    custom.bounds(customBounds);
    vg::stroke(view.lines(custom), 0x8ce0bf, 2.2f);
    draw::text("CUSTOM AREA KM²: " + triangle.area() + "  BOUNDS: " + customBounds[0] + ".." + customBounds[2], 56, 684, 14, 0x9eabbd);
    draw::text("PROJECTION VISIBLE: " + visible + "  BACK SIDE VISIBLE: " + backSideVisible, 56, 660, 16, 0x9eabbd);
}
