// A procedural globe texture built once with the scriptable GlobeMap API.
void configure(Composition &inout c) {
    c.name = "globe-map-handles";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void assets() {
    geo::GeoLayer@ land = geo::load("assets/data/geo/land-110m.geojson");
    geo::GeoLayer@ countries = geo::load("assets/data/geo/countries-110m.geojson");
    globe::GlobeMap@ map = globe::map(1024, 0x102c44);
    map.gradient(0x234b73, 0x92bdd0, 0x172b3d);
    map.fill(land, 0x718d72);
    map.stroke(countries, 0xc6cfb6, 0.7);
    map.graticule(30, 0x7896a866, 0.5);
    map.disc(103.8, 1.3, 180, 0xffb444, 0.4);
    array<uint> pixels;
    map.rgba(pixels);
    assets::writeImage("out/generated/globe-map-handles.png", map.width(), map.height(), pixels);

    globe::GlobeField@ field = globe::field(512);
    field.add(103.8, 1.3, 520, 1.0);
    field.add(115.2, -8.7, 260, 0.7);
    field.add(139.7, -4.3, 180, 0.4);
    array<uint> heatPixels;
    field.rgba(field.max(), "fieldPalette", heatPixels);
    assets::writeImage("out/generated/globe-field-handles.png", field.width(), field.height(), heatPixels);

    globe::GlobeWeather@ weather = globe::weather();
    weather.cover(0.45f);
    weather.scale(4.0f);
    weather.evolve(0.04f);
    weather.seed(23);
    weather.windFunction("indonesianMonsoon");
    weather.addStorm(130, -10, 680, 0.6f, true);
    array<uint> cloudPixels;
    globe::cloudImage(512, 18.0, weather, cloudPixels);
    assets::writeImage("out/generated/globe-weather-callback.png", 512, 256, cloudPixels);
}

void draw(int frame) {
    double reveal = double(frame) / 89.0;
    g3d::Scene3D@ scene = g3d::scene();
    scene.lookAtGlobe(118, -3, 3.1);
    scene.addDirectional(-2, 3, 4, 0xffffff, 1.4, true);
    g3d::Material3D@ earth = g3d::material();
    earth.map("out/generated/globe-map-handles.png");
    earth.roughness(0.78);
    scene.add(globe::sphere(1, 80), earth);
    g3d::Material3D@ heat = g3d::material();
    heat.map("out/generated/globe-field-handles.png");
    heat.opacity(0.88);
    heat.doubleSided(true);
    heat.unlit(true);
    scene.add(globe::sphere(1.006, 80), heat);
    globe::route(scene, 106.8, -6.2, 115.2, -8.7, float(reveal), 0,
        "{\"height\":0.28,\"width\":0.009,\"color\":\"#ffbd5b\",\"segments\":50}");
    g3d::Object3D@ customRoutes = scene.addGroup();
    globe::pathFunction(customRoutes, "islandArc", float(reveal), 0,
        "{\"height\":0.24,\"width\":0.006,\"color\":\"#89d8bd\",\"segments\":48}");
    globe::route(scene, 106.8, -6.2, 139.7, -4.3, float(reveal), 0,
        "{\"height\":0.32,\"width\":0.007,\"color\":\"#ed795d\",\"segments\":60}");
    scene.render(frame, 0, 0, 1280, 720);
    draw::text("PROCEDURAL GLOBE MAP • INDONESIA ROUTES", 40, 675, 18, 0xb8c6cc);
}


float indonesianMonsoon(float latitude) {
    return latitude < -8.0f ? 7.5f : (latitude > 8.0f ? -5.0f : 2.0f);
}


uint fieldPalette(float value) {
    if (value < 0.2f) return 0x183b52;
    if (value < 0.55f) return 0x3e927d;
    if (value < 0.82f) return 0xf2b84b;
    return 0xff704f;
}


string islandArc(double u) {
    float angle = float(u) * 3.14159265f;
    float x = -0.55f + float(u) * 1.1f;
    float y = 0.22f + sin(angle) * 0.38f;
    float z = 0.55f - float(u) * 0.85f;
    return "[" + x + "," + y + "," + z + "]";
}
