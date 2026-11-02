void configure(Composition &inout c) {
    c.name = "particle-authoring";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x11131b;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    string fire = "{\"seed\":818,\"burst\":24,\"burstAt\":10,\"burstSpread\":8,\"rate\":0.55,\"from\":0,\"until\":90,\"life\":[22,46],\"area\":\"line\",\"extent\":[120,4],\"originFunction\":\"emberSource\",\"speed\":[2,6],\"angle\":-90,\"spread\":34,\"gravity\":[0,-0.025],\"drag\":0.02,\"turbulence\":0.45,\"look\":\"glow\",\"size\":[5,15],\"sizeEnd\":0.12,\"colors\":[\"#ff6b35\",\"#ffc15e\"],\"overLife\":[\"#fff0bd\",\"#ff7a36\",\"#542b62\"],\"fadeIn\":0.08,\"fadeOut\":0.45,\"additive\":true}";
    fx::draw(fire, frame);
    uint alive = fx::particleCount(fire, frame);
    array<float> sample;
    bool hasSample = fx::particle(fire, frame, 0, sample);
    draw::text("SCRIPTED PARTICLE EMITTER", 640, 60, 23, 0xe8e2d8, 1, 0.5f);
    draw::text("birth • motion • life colour • additive glow", 640, 95, 15, 0x9ba4ae, 1, 0.5f);
    draw::text("LIVE: " + alive + (hasSample ? "  FIRST SIZE: " + sample[8] : ""), 640, 135, 14, 0xc0c9d2, 1, 0.5f);
}

string emberSource(double birthFrame) {
    return "[" + (640 + 34 * sin(birthFrame * 0.045f)) + ",590]";
}
