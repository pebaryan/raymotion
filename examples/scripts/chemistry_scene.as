void configure(Composition &inout c) {
    c.name = "chemistry-scene";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    ChemMolecule@ benzene = chem::smiles("C1=CC=CC=C1");
    ChemMolecule@ explicitHydrogens = chem::withHydrogens(benzene);
    float dataReveal = clamp01(float(frame - 8) / 12.0f);
    float modelCue = clamp01(float(frame - 18) / 6.0f);
    float modelReveal = clamp01(float(frame - 24) / 18.0f);
    float turn = frame * 1.2f;
    string world = "{\"camera\":{\"position\":[0,2.2,7],\"target\":[0,0,0],\"fov\":42},\"ambient\":0.55,\"lights\":[{\"type\":\"directional\",\"position\":[-3,5,4],\"intensity\":2.2,\"castShadow\":true}],\"objects\":[{\"geometry\":\"plane\",\"width\":7,\"depth\":5,\"position\":[0,-1.2,0],\"material\":{\"color\":\"#17232d\",\"roughness\":0.9}},{\"chemMolecule\":{\"smiles\":\"C1=CC=CC=C1\",\"mode\":\"ballAndStick\",\"hydrogens\":true},\"scale\":[" + (0.55f * modelReveal) + "," + (0.55f * modelReveal) + "," + (0.55f * modelReveal) + "],\"rotation\":[0," + turn + ",0]}]}";
    g3d::draw(world, frame, 0, 0, 1280, 720);
    draw::text("FORMULA " + benzene.formula() + "   /   MASS " + benzene.mass(), 40, 38, 18, 0xe9e5dc, dataReveal);
    draw::text("HEAVY ATOMS " + benzene.atomCount() + "   /   EXPLICIT ATOMS " + explicitHydrogens.atomCount() + "   /   RINGS " + benzene.ringCount(), 40, 68, 15, 0x70d7ff, dataReveal);
    draw::text("SAME MOLECULE / 3D", 40, 101, 14, 0xffbf69, modelCue);
    draw::text("BENZENE / SCRIPTED 3D MOLECULAR MODEL", 40, 672, 20, 0xe9e5dc);
}
