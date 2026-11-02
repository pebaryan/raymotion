void configure(Composition &inout c) {
    c.name = "subtitle-style-card";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 900;
    c.background = 0x10131c;
    int phraseFrame = subtitles::frameOf("assets/vo/austria/narration.srt", "Ostarrichi appeared", c.fps, 0);
    scene(phraseFrame, c.duration - phraseFrame, "draw");
}

void draw(int frame) {
    draw::text("SUBTITLE TRACK / CUSTOM STYLE", 48, 80, 28, 0xe9e5dc);
    double spokenAt = subtitles::when("assets/vo/austria/narration.srt", "Ostarrichi appeared", 0);
    subtitles::drawFileStyled("assets/vo/austria/narration.srt", spokenAt + frame / 30.0,
        "{\"size\":38,\"maxWidth\":1000,\"bottom\":70,\"align\":\"center\",\"textAlign\":\"center\",\"font\":\"uiBold\",\"outline\":\"#090b10e8\",\"outlineWidth\":4,\"box\":\"#10131caa\",\"boxPadding\":22,\"boxRadius\":14,\"fade\":5,\"upcoming\":\"#70d7ff\"}");
}
