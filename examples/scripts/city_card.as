// City model -- A CityJSON town that rises from the ground under a fitted camera.
// Category: Maps

void configure(Composition &inout c) {
    c.name = "city-card";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    g3d::draw("{\"camera\":{\"position\":[0,1,1],\"target\":[0,0,0]},\"background\":\"#10131c\",\"ambient\":0.5,\"lights\":[{\"type\":\"directional\",\"position\":[2,4,3],\"intensity\":2}],\"city\":{\"file\":\"assets/data/city/town.city.json\",\"rise\":true,\"delay\":4,\"stagger\":0.2,\"length\":24,\"frameCamera\":true,\"azimuth\":220,\"elevation\":32,\"aspect\":1.866,\"roof\":\"#dc795f\",\"wall\":\"#e8deca\",\"road\":\"#424854\",\"vegetation\":\"#648b68\",\"colorFunction\":\"chooseFaceColor\",\"materialFunction\":\"styleCityObject\"}}", frame, 50, 35, 1180, 650);
    draw::text("SYNTHETIC CITYJSON • SCRIPTABLE RISE + CAMERA FIT", 40, 675, 18, 0xb8c6cc);
}

string chooseFaceColor(string inputJson) {
    if (inputJson.findFirst("RoofSurface") >= 0) return "#dc795f";
    if (inputJson.findFirst("WallSurface") >= 0) return "#e8deca";
    if (inputJson.findFirst("TrafficArea") >= 0) return "#424854";
    return "null";
}

string styleCityObject(string inputJson) {
    if (inputJson.findFirst("Building") >= 0)
        return "{\"roughness\":0.82,\"metalness\":0.02}";
    return "null";
}
