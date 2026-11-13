// Video -- A video fitted, aligned and tinted. Give it one with --props {"video":"clip.mp4"}.
// Category: Audio and video
//
//   raymotion_script preview video-style-card --script examples/scripts/video_style_card.as
//       --props '{"video":"path/to/clip.mp4"}'
//
// The repository ships no video, so without the prop the card only says how to pass one.

void configure(Composition &inout c) {
    c.name = "video-style-card";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x10131c;
    scene(0, c.duration, "draw");
    string path = props::text("video", "");
    if (path == "") return;

    string source = media::resolve(path);
    int vw = 0, vh = 0;
    double videoFps = 0, videoDuration = 0;
    bool hasAudio = false;
    media::probe(source, vw, vh, videoFps, videoDuration, hasAudio);
    // Cover-fit into a 1040 x 500 window, aligned 70% across and 40% down, tinted cool.
    media::videoStyled(0, c.duration, source, 120, 70, 1040, 500, 0, 0, 1, true, 0, true,
                       "cover", 0.7f, 0.4f, 0x93dbff);
}

void draw(int frame) {
    if (props::text("video", "") == "") {
        draw::roundRect(120, 70, 1040, 500, 18, 0x16202a);
        draw::text("No video yet", 640, 290, 44, 0xe9e5dc, 1, 0.5f, 0.5f);
        draw::text("--props '{\"video\":\"path/to/clip.mp4\"}'", 640, 350, 22, 0x93dbff, 1, 0.5f, 0.5f);
    }
    draw::roundRect(30, 620, 420, 50, 12, 0x16202a, 0.9f);
    draw::text("VIDEO FIT • ALIGNMENT • TINT", 52, 634, 20, 0xe9e5dc);
}
