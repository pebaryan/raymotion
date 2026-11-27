// Weather prefabs -- weather, water-body, rain and lightning: a clear sky turning to a storm, a lake, a downpour and a night of strikes, each from one line of props.
// Category: 3D

void configure(Composition &inout c) {
    c.name = "weather-prefabs";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 480;
    c.background = 0x101418;
    // A clear afternoon that clouds over into a squall between frames 20 and 100; `to`, `start` and `end` are the blend.
    prefab::use(0, 120, "weather", "{\"box\":[0,0,1280,720],\"preset\":\"cumulus day\",\"to\":\"squall\",\"start\":20,\"end\":100}");
    // A lake in a breeze, the camera slowly round it; `look` is calm, breezy or choppy.
    prefab::use(120, 120, "water-body", "{\"box\":[0,0,1280,720],\"look\":\"breezy\",\"orbit\":0.1}");
    // Rain that builds after half a second, the ground soaking and drops on the lens.
    prefab::use(240, 120, "rain", "{\"box\":[0,0,1280,720],\"intensity\":0.8,\"delay\":0.5,\"ramp\":2,\"lens\":0.6}");
    // Seven strikes in four seconds under a storm sky. Add "clip" to place a thunder sample at each strike's arrival time.
    prefab::use(360, 120, "lightning", "{\"box\":[0,0,1280,720],\"count\":7,\"span\":4,\"seed\":2}");
    scene(0, c.duration, "draw");
}

// The prefabs draw the whole frame; nothing to add here.
void draw(int frame) {}
