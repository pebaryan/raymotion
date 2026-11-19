// City plants -- A CityJSON town whose trees and park become grown trees and grass instead of flat solids.
// Category: Maps

// "plants" on a city draws its SolitaryVegetationObjects as trees (as tall as their height
// attribute; conifers by species) and its PlantCover as grass, or shrubs when its
// averageHeight says so; the trees' own solids are left out, the park stays as ground. The wind
// sways them; every frame is a function of t.
void configure(Composition &inout c) {
    c.name = "city-plants";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 180;
    c.background = 0x9fb4c8;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float k = frame / 179.0f;
    k = k * k * (3 - 2 * k);                                // from above the town down into the park
    float turn = 2.6f - 0.35f * k;
    float reach = 95 - 63 * k, high = 42 - 38 * k;
    float cx = 144 + reach * sin(turn), cz = 80 + reach * cos(turn);
    g3d::draw("{\"camera\":{\"position\":[" + cx + "," + high + "," + cz + "],\"target\":[144," + (2 + 2 * k) + ",80],\"fov\":46},\"objects\":[{\"geometry\":\"plane\",\"width\":1200,\"depth\":1200,\"position\":[0,-0.7,0],\"material\":{\"color\":\"#8d9070\",\"roughness\":1}}],\"ambient\":0.45,\"shadowMapSize\":4096,"
              "\"sky\":{\"enabled\":true,\"zenith\":\"#5d82b8\",\"horizon\":\"#e6dcc8\",\"show\":true},"
              "\"lights\":[{\"type\":\"directional\",\"position\":[200,90,120],\"target\":[144,0,80],\"intensity\":2.2,\"castShadow\":true,\"shadowExtent\":90}],"
              "\"city\":{\"file\":\"assets/data/city/town.city.json\","
              "\"roof\":\"#c9765e\",\"wall\":\"#ece3d2\",\"road\":\"#50555f\","
              "\"plants\":{\"grass\":\"lawn\",\"wind\":1}}}", frame, 0, 0, 1280, 720);
    draw::text("TREES AND A PARK FROM CITYJSON VEGETATION", 34, 676, 16, 0x1e2630);
}
