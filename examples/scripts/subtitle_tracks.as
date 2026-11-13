// Subtitle tracks -- An SRT built in the script, queried by cue and drawn as captions.
// Category: Text

string sourceSrt() {
    return "1\n00:00:00,300 --> 00:00:02,200\n<i>The storm has a memory.</i>\n\n2\n00:00:02,400 --> 00:00:05,100\nIt remembers every light\nthat went out.\n";
}

void configure(Composition &inout c) {
    c.name = "subtitle-tracks";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 180;
    c.background = 0x11151e;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    subtitles::SubtitleTrack@ track = subtitles::parse(sourceSrt());
    double seconds = double(frame) / 30.0;
    draw::text("SUBTITLE TRACK API", 54, 42, 22, 0xdfe4eb);
    draw::text("CUES: " + track.count() + "   DURATION: " + track.seconds() + " s", 56, 110, 18, 0x9eabbd);
    array<string> phrases = {"The storm", "every light"};
    array<int> spans;
    subtitles::spans(track, phrases, 30, 30, spans);
    draw::text("PHRASE SPANS: " + spans[0] + "/" + spans[1] + "  •  " + spans[2] + "/" + spans[3], 56, 150, 18, 0x9eabbd);
    draw::text("FIRST CUE: " + track.cueText(0), 56, 202, 18, 0xe7ab59);
    draw::text("SRT ROUND TRIP: " + subtitles::toSrt(track).length() + " chars", 56, 242, 16, 0x9eabbd);
    subtitles::drawCueStyled(track, 0, seconds,
        "{\"size\":46,\"align\":\"center\",\"maxWidth\":1100,\"bottom\":170,\"outlineWidth\":4,\"box\":\"#11151edc\",\"boxPadding\":24,\"boxRadius\":18}");
}
