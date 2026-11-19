// Flora prefabs -- flora-2d-hill, flora-tree-grow and flora-meadow: a hillside, a growing tree and a meadow, each from one line of props.
// Category: 3D

void configure(Composition &inout c) {
    c.name = "flora-prefabs";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 360;
    c.background = 0x101418;
    // A 2D hillside whose trees, shrubs and grass grow in over three seconds.
    prefab::use(0, 120, "flora-2d-hill", "{\"box\":[0,0,1280,720],\"trees\":4,\"delay\":5,\"duration\":90}");
    // An oak from a sapling, the lawn coming up round it.
    prefab::use(120, 120, "flora-tree-grow", "{\"box\":[0,0,1280,720],\"tree\":\"oak\",\"delay\":5,\"duration\":100}");
    // A meadow of oaks and birches in the wind, the camera slowly round it.
    prefab::use(240, 120, "flora-meadow", "{\"box\":[0,0,1280,720],\"trees\":12,\"orbit\":0.12}");
    scene(0, c.duration, "draw");
}

// The prefabs draw the whole frame; nothing to add here.
void draw(int frame) {}
