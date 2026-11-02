void configure(Composition &inout c) {
    c.name = "video-style-card";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 30;
    c.background = 0x10131c;
    string source = media::resolve("C:\\Users\\aryan\\AppData\\Local\\Temp\\raymotion-video-style-source.mp4");
    int vw = 0, vh = 0;
    double videoFps = 0, videoDuration = 0;
    bool hasAudio = false;
    media::probe(source, vw, vh, videoFps, videoDuration, hasAudio);
    float videoHeight = c.w * float(vh) / float(vw);
    scene(0, c.duration, "draw");
    media::videoStyled(0, c.duration, source, 120, 70, 1040, videoHeight, 0, 0, 1, true, 0, true, "cover", 0.7f, 0.4f, 0x93dbff);
}
void draw(int frame) {
    draw::roundRect(30, 620, 420, 50, 12, 0x16202a, 0.9f);
    draw::text("VIDEO FIT • ALIGNMENT • TINT", 52, 634, 20, 0xe9e5dc);
}
