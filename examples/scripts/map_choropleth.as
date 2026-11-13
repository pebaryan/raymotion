// Choropleth -- A world map draws itself on, then countries fill by population density, with a key and labels.
// Category: Maps

void configure(Composition &inout c) {
    c.name = "map-choropleth";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 150;
    c.background = 0x0d1118;
    scene(0, c.duration, "draw");
}

float ramp(int frame, int from, int to) { return clamp01(float(frame - from) / float(to - from)); }

void draw(int frame) {
    geo::GeoLayer@ world = geo::layer("countries");
    geo::GeoProjection@ map = geo::projection("equalEarth");
    map.fitSphere(40, 96, 1200, 560, 0);

    // People per square kilometre, from the bundled Natural Earth population and the
    // country's own area on the sphere.
    string density = "{";
    for (int i = 0; i < world.count(); i++) {
        double pop = parseFloat(world.property(i, "pop"));
        double area = world.area(i);
        if (pop <= 0 || area <= 0) continue;
        if (density.length() > 1) density += ",";
        density += "\"" + world.property(i, "iso_a3") + "\":" + (pop / area);
    }
    density += "}";

    string scale = "{\"min\":0,\"max\":300,\"colors\":[\"#16324a\",\"#1f6f8b\",\"#56b6a4\",\"#f2d16b\",\"#f08a4b\"]}";
    geo::base(map, ramp(frame, 0, 50), "{\"land\":\"#222c3a\",\"ocean\":\"#121924\"}");
    geo::choropleth(map, density, ramp(frame, 40, 100), scale);
    geo::colorbar(60, 520, 18, 130, density, ramp(frame, 60, 100),
                  "{\"min\":0,\"max\":300,\"colors\":[\"#16324a\",\"#1f6f8b\",\"#56b6a4\",\"#f2d16b\",\"#f08a4b\"],\"title\":\"people per sq km\",\"textSize\":16}");

    // The eight most populous countries, named. Labels that would collide are left out.
    string big = "[";
    array<string> codes = {"CHN", "IND", "USA", "IDN", "PAK", "BRA", "NGA", "BGD"};
    for (uint i = 0; i < codes.length(); i++) {
        int at = world.find(codes[i]);
        if (at < 0) continue;
        if (big.length() > 1) big += ",";
        big += "{\"lon\":" + world.labelLongitude(at) + ",\"lat\":" + world.labelLatitude(at) + ",\"text\":\"" + world.name(at) + "\"}";
    }
    big += "]";
    geo::labels(map, big, ramp(frame, 95, 130), "{\"size\":18,\"bold\":true}");

    draw::text("POPULATION DENSITY", 40, 30, 26, 0xeef2f6);
    draw::text("Natural Earth 1:110m (bundled). Values above 300 share the top colour.", 40, 66, 15, 0x8d9aab);
}
