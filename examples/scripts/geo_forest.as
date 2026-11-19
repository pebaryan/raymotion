// Forest cover -- Trees spread across Finland, Sweden and Norway, west to east, as the globe unrolls into a map.
// Category: Maps

// "forest" on a geo3d scene scatters low-poly trees over regions (country keys or
// [west, south, east, north] boxes), `spacing` degrees apart, `height` world units tall,
// standing along the surface's up whether it is a globe, a map or between. `start` and
// `duration` grow the cover in; `stagger` makes the eastern trees start later.
void configure(Composition &inout c) {
    c.name = "geo-forest";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 240;
    c.background = 0x0e1622;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float morph = frame < 120 ? 0.0f : (frame < 200 ? (frame - 120) / 80.0f : 1.0f);
    morph = morph * morph * (3 - 2 * morph);
    string json = "{\"camera\":{\"position\":[0,0.2,2.25],\"target\":[0,0.05,0.9],\"fov\":40},\"ambient\":0.55,"
        "\"lights\":[{\"type\":\"directional\",\"position\":[-2,3,4],\"intensity\":1.6}],"
        "\"geo3d\":{\"projection\":\"equirectangular\",\"centerLon\":18,\"centerLat\":63,\"radius\":1,\"morph\":" + morph + ","
        "\"mapWidth\":2048,\"ocean\":\"#17324a\",\"land\":\"#b9ad8c\",\"border\":\"#6f6650\","
        "\"forest\":{\"regions\":[\"FIN\",\"SWE\",\"NOR\"],\"spacing\":0.45,\"height\":0.018,\"stagger\":0.5,\"start\":10,\"duration\":100,\"seed\":3}}}";
    g3d::draw(json, frame, 0, 0, 1280, 720);
    draw::text("FOREST COVER, NORDIC COUNTRIES (ILLUSTRATIVE DENSITY)", 40, 675, 18, 0xc8d2d0);
}
