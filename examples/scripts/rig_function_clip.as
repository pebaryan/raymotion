// Arbitrary rig::Clip expressed as a pure function of clip-local seconds.
void proceduralPose(const rig::RigBuild@ build, double seconds, array<float> &out values) {
    values.resize(3 + build.jointCount() * 3);
    int head = 3 + build.jointIndex("head") * 3;
    int shoulder = 3 + build.jointIndex("shoulder-R") * 3;
    int elbow = 3 + build.jointIndex("elbow-R") * 3;
    values[head] = float(sin(seconds * 3.0) * 12);
    values[shoulder] = -18; values[shoulder + 2] = float(18 + sin(seconds * 4.0) * 24);
    values[elbow] = -42;
    values[0] = 0; values[1] = float(abs(sin(seconds * 3.0)) * 0.035); values[2] = 0;
}

void configure(Composition &inout c) {
    c.name = "rig-function-clip"; c.w = 640; c.h = 360; c.fps = 30; c.duration = 90;
    c.background = 0x141923;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    string character = "{\"preset\":\"toon\",\"clips\":[{\"name\":\"function\",\"callback\":\"proceduralPose\",\"from\":0}],\"view\":{\"originX\":320,\"originY\":330,\"scale\":210}}";
    rig::draw(character, frame / 30.0, 0, 0, 640, 360);
}

double proceduralSoftness(double seconds) {
    return 0.28 + 0.22 * (0.5 + 0.5 * sin(seconds * 2.0));
}
