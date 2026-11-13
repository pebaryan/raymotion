// Map prefabs -- geo-flyto and geo-route: a flight to a country and an illustrative route, each from one line of props.
// Category: Maps

void configure(Composition &inout c) {
    c.name = "map-prefabs";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 180;
    c.background = 0x0d1118;
    // The world, then Kenya: the map zooms out a little on the way and names where it lands.
    prefab::use(0, 90, "geo-flyto",
                "{\"from\":\"world\",\"to\":\"KEN\",\"box\":[0,0,1280,720],\"padding\":200,\"delay\":10,\"duration\":60,\"highlight\":\"#3d8f86\",\"textSize\":34}");
    // Nairobi to Singapore, framed on the route itself.
    prefab::use(90, 90, "geo-route",
                "{\"stops\":[[36.82,-1.29],[103.82,1.35]],\"labels\":[\"Nairobi\",\"Singapore\"],\"box\":[0,0,1280,720],\"padding\":170,\"delay\":10,\"duration\":55,\"arrow\":true,\"textSize\":24}");
    scene(0, c.duration, "draw");
}

// The prefabs draw the whole frame; nothing to add here.
void draw(int frame) {}
