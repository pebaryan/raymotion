// Geo data -- GeoJSON features measured: bounds, area, centroid and a great-circle route.
// Category: Maps

void configure(Composition &inout c) {
    c.name = "geo-data";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11151e;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    string geoJson = "{\"type\":\"FeatureCollection\",\"features\":[{\"type\":\"Feature\",\"id\":\"isle\",\"properties\":{\"name\":\"Moon Isle\",\"region\":\"South Sea\"},\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[[[146,-18],[154,-18],[154,-11],[146,-11],[146,-18]]]}}]}";
    geo::GeoLayer@ layer = geo::parse(geoJson);
    int isle = layer.find("isle");
    geo::GeoLayer@ southSea = layer.filter("region", "South Sea");
    array<float> metrics;
    geo::metrics(layer, isle, metrics);
    array<float> midway;
    geo::interpolate(170, 20, -170, 20, float(frame) / 90.0f, midway);
    double routeKm = geo::distance(170, 20, -170, 20);
    double routeDegrees = geo::angle(170, 20, -170, 20);
    double routeBearing = geo::bearing(170, 20, -170, 20);
    draw::text("GEOJSON FEATURE ANALYSIS", 54, 44, 23, 0xdfe4eb);
    draw::text("FEATURE: " + layer.name(isle) + " / " + layer.property(isle, "region"), 56, 120, 19, 0xe7ab59);
    draw::text("FEATURES IN LAYER: " + layer.count(), 56, 166, 18, 0x9eabbd);
    draw::text("FILTERED REGION COUNT: " + southSea.count() + " / LABEL: " + layer.labelLongitude(isle) + ", " + layer.labelLatitude(isle), 56, 192, 16, 0x9eabbd);
    draw::text("CENTROID: " + metrics[0] + ", " + metrics[1], 56, 225, 18, 0x9eabbd);
    draw::text("BOUNDS: " + metrics[2] + ", " + metrics[3] + " → " + metrics[4] + ", " + metrics[5], 56, 270, 18, 0x9eabbd);
    draw::text("POLYGON AREA: " + metrics[6] + " km²", 56, 315, 18, 0x9eabbd);
    draw::text("CONTAINS CENTER: " + layer.contains(isle, 150, -14), 56, 360, 18, 0x6fc6b6);
    draw::text("GREAT-CIRCLE ROUTE MIDPOINT: " + midway[0] + ", " + midway[1], 56, 430, 18, 0x9eabbd);
    draw::text("ROUTE: " + routeKm + " km / " + routeDegrees + "° / bearing " + routeBearing + "°", 56, 475, 18, 0x9eabbd);
    draw::circle(850, 320, 5, 0xe7ab59);
    draw::text("All coordinates are illustrative", 56, 630, 15, 0x768398);
}
