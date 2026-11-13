// Custom rig part -- A body part made from primitives by a script factory.
// Category: Characters
//
// Custom rig::PartDef.make factory expressed as bounded native primitive records.
void scriptLantern(const rig::RigBuild@ build, int side, float softness, array<float> &out fields, array<string> &out strings) {
    fields.resize(35);
    strings.resize(5);
    fields[0] = 0; // Prim::Super
    fields[1] = build.jointIndex("head");
    fields[2] = 0; fields[3] = 0.22f; fields[4] = 0;
    fields[8] = 0.12f; fields[9] = 0.16f; fields[10] = 0.12f;
    fields[11] = 1; fields[12] = 1; fields[13] = 1; fields[14] = -1; fields[15] = 1;
    fields[16] = 8; // Paint::Fixed
    fields[17] = 255; fields[18] = 162; fields[19] = 48; fields[20] = 255;
    fields[21] = 0; fields[22] = 0; fields[23] = 1;
    fields[24] = build.jointIndex("shoulder-L");
    fields[25] = build.jointIndex("elbow-L");
    fields[26] = build.jointIndex("wrist-L");
    fields[27] = 0.05f; fields[28] = 0.04f; fields[29] = softness;
    fields[32] = -1; fields[34] = -2;
    strings[0] = "warm script lantern";
}

void configure(Composition &inout c) {
    c.name = "rig-custom-part"; c.w = 640; c.h = 360; c.fps = 30; c.duration = 90;
    c.background = 0x17131c;
    rig::registerPart("script-lantern", "hat", "scriptLantern");
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    string look = "{\"preset\":\"toon\",\"hat\":\"script-lantern\",\"hair\":\"hair.bun\",\"view\":{\"originX\":320,\"originY\":320,\"scale\":220}}";
    rig::draw(look, frame / 30.0, 0, 0, 640, 360);
}
