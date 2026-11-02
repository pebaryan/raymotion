void configure(Composition &inout c) {
    c.name = "transition-authoring";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 100;
    c.background = 0x11131b;
    scene(0, 60, "opening");
    scene(40, 60, "reveal");
    sceneTransitionFunction(0, 1, 20, "easeWipe", TRANSITION_WIPE, FROM_LEFT);
}

void opening(int frame) {
    draw::rect(0, 0, 1280, 720, 0x183340);
    draw::text("OUTGOING SCENE", 640, 350, 38, 0xe8e2d8, 1, 0.5f);
}

void reveal(int frame) {
    draw::rect(0, 0, 1280, 720, 0x713c4a);
    draw::text("EASED WIPE REVEAL", 640, 350, 38, 0xffc15e, 1, 0.5f);
}

double easeWipe(double progress) {
    return progress * progress * (3.0 - 2.0 * progress);
}
