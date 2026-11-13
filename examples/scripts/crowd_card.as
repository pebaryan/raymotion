// Crowd -- Agents with goals that flee a threat, the same on every render.
// Category: Characters

void configure(Composition &inout c) {
    c.name = "crowd-card";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    float time = frame / 30.0f;
    string json = "{\"fps\":30,\"camera\":{\"position\":[0,5,10],\"target\":[0,1,0],\"fov\":46},\"ambient\":0.72,\"lights\":[{\"type\":\"directional\",\"position\":[-3,7,5],\"intensity\":1.7,\"castShadow\":true}],\"objects\":[{\"geometry\":\"plane\",\"width\":20,\"depth\":14,\"material\":{\"color\":\"#252a34\",\"roughness\":1}}],\"crowd\":{\"from\":0,\"to\":3,\"seed\":83,\"dt\":0.033333333,\"lookFunction\":\"villagerLook\",\"lookSeed\":16,\"faces\":0,\"boundsMin\":[-8,0,-5],\"boundsMax\":[8,6,5],\"groups\":[{\"name\":\"villagers\",\"count\":18,\"spawn\":[-3,0,2],\"spawnSize\":[3,0,2],\"cruise\":1.1,\"maxSpeed\":2.8,\"rest\":0.18,\"flees\":[\"penunggu\"],\"goalFan\":[2,0,0],\"goalFunction\":\"villagerGoal\"}],\"predators\":[{\"name\":\"penunggu\",\"pathFunction\":\"penungguPath\"}],\"obstacles\":[{\"at\":[0,0,0],\"radius\":0.7,\"height\":2}]}}";
    crowd::Simulation@ simulation = crowd::bake(json);
    array<float> goal, threat;
    bool hasGoal = simulation.goalAt(0, time, 0, goal);
    simulation.predatorAt(0, time, threat);
    string worldInfo = simulation.worldInfoJson();
    string groupInfo = simulation.groupInfoJson(simulation.groupIndex(0));
    string obstacleInfo = simulation.obstacleInfoJson(0);
    g3d::draw(json, frame, 0, 0, 1280, 720);
    if (hasGoal)
        draw::text("QUERY • " + simulation.groupName(0) + " / " + simulation.size() + " agents • goal " + goal[0] + ", " + goal[2] + " • " + simulation.predatorName(0) + " " + threat[0] + ", " + threat[2], 40, 642, 13, 0xe2bd82);
    draw::text("SAMPLED CROWD SIM • DETERMINISTIC GOALS + FLEE RESPONSE", 40, 675, 18, 0xb8c6cc);
}

string villagerLook(int index) {
    if (index % 3 == 0)
        return "{\"preset\":\"toon\",\"skin\":\"#b9a084\",\"top\":\"#6d4038\",\"bottom\":\"#323a48\",\"hairColor\":\"#30232a\",\"finish\":\"matte\"}";
    if (index % 3 == 1)
        return "{\"preset\":\"toon\",\"skin\":\"#d6ad82\",\"top\":\"#5d6a45\",\"bottom\":\"#493a35\",\"hairColor\":\"#57402d\",\"finish\":\"matte\"}";
    return "{\"preset\":\"toon\",\"skin\":\"#c6a88f\",\"top\":\"#8b704d\",\"bottom\":\"#29313b\",\"hairColor\":\"#3c3029\",\"finish\":\"matte\"}";
}

string villagerGoal(double seconds, int index) {
    float lane = float(index % 5);
    float x = 2.0f + lane * 0.4f + sin(float(seconds) * 0.8f) * 0.3f;
    float z = -3.0f + seconds * 1.3f;
    return "[" + x + ",0," + z + "]";
}

string penungguPath(double seconds) {
    float x = 4.0f - seconds * 2.0f;
    float z = 3.0f - seconds * 1.4f;
    return "[" + x + ",0," + z + "]";
}