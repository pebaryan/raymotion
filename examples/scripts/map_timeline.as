// Choropleth over time -- A region's values change year by year on one colour scale, from a small CSV table.
// Category: Maps

void configure(Composition &inout c) {
    c.name = "map-timeline";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 180;
    c.background = 0x0d1118;
    scene(0, c.duration, "draw");
}

float ramp(int frame, int from, int to) { return clamp01(float(frame - from) / float(to - from)); }

// A row per country, a column per year. Illustrative numbers, not a dataset.
string table() {
    return "code,2000,2005,2010,2015,2020\n"
           "IDN,12,19,31,48,66\n"
           "MYS,21,38,56,71,88\n"
           "THA,9,17,29,47,72\n"
           "VNM,3,11,28,45,70\n"
           "PHL,6,10,22,39,58\n"
           "KHM,1,3,8,22,44\n"
           "LAO,1,2,6,17,36\n"
           "MMR,1,1,3,14,35\n";
}

void draw(int frame) {
    geo::GeoLayer@ world = geo::layer("countries");
    array<string> codes = {"IDN", "MYS", "THA", "VNM", "PHL", "KHM", "LAO", "MMR"};
    geo::GeoGeometry@ region = geo::geometry();
    for (uint i = 0; i < codes.length(); i++) region.append(world.geometry(world.find(codes[i])));

    geo::GeoProjection@ map = geo::focus(geo::projection("mercator"), region, 60, 110, 820, 560, 20);
    geo::base(map, 1, "{\"ocean\":\"#00000000\",\"land\":\"#1c2533\",\"graticule\":\"#00000000\",\"coast\":\"#3b4a5e\",\"border\":\"#2c394a\"}");

    // The year runs 2000 -> 2020; values between two columns are blended.
    double year = 2000 + 20.0 * ramp(frame, 30, 160);
    string style = "{\"min\":0,\"max\":100,\"colors\":[\"#17324d\",\"#1f7a8c\",\"#7fc8a9\",\"#f6d67a\"],\"stagger\":0}";
    geo::choropleth(map, geo::valuesAt(table(), year), ramp(frame, 0, 25), style);
    geo::colorbar(940, 300, 20, 220, "{}", ramp(frame, 5, 30),
                  "{\"min\":0,\"max\":100,\"colors\":[\"#17324d\",\"#1f7a8c\",\"#7fc8a9\",\"#f6d67a\"],\"suffix\":\"%\",\"textSize\":18}");

    draw::text("" + int(year), 930, 150, 96, 0xeef2f6);
    draw::text("ONE SCALE, FIVE COLUMNS", 60, 30, 26, 0xeef2f6);
    draw::text("Illustrative values. geo::valuesAt(csv, year) blends between the years in the table.", 60, 66, 15, 0x8d9aab);
}
