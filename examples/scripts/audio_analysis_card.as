void configure(Composition &inout c) {
    c.name = "audio-analysis-card";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 90;
    c.background = 0x10131c;
    media::audio(0, c.duration, "assets/audio/austria/chime.wav", 0, 0, 1, true, 0.32f);
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    AudioAnalysis@ analysis = aud::analyze("assets/audio/austria/chime.wav", 30, 0, 48, 40, 12000, 36, 1, 6);
    float level = analysis.level(frame);
    float bass = analysis.energy(frame, 35, 180);
    float presence = analysis.energy(frame, 1200, 6000);
    float pulse = analysis.pulse(frame, 20);
    array<float> spectrum;
    analysis.spectrum(frame, spectrum);
    array<float> localWaveform;
    analysis.waveform(frame, 64, 12, localWaveform);
    float peakBand = 0;
    for (uint i = 0; i < spectrum.length(); ++i) if (spectrum[i] > peakBand) peakBand = spectrum[i];
    float wavePeak = 0;
    for (uint i = 0; i < localWaveform.length(); ++i) {
        float magnitude = localWaveform[i] < 0 ? -localWaveform[i] : localWaveform[i];
        if (magnitude > wavePeak) wavePeak = magnitude;
    }
    aud::waveform(analysis, 60, 65, 1160, 105, frame, 256, 42, 0x70d7ff, 2);
    aud::bars(analysis, 60, 220, 1160, 300, frame, 0xffb454, 0.22f, false, 4);
    aud::radial(analysis, 640, 525, 72, 80, frame, 0xf16f5b, 4, frame * 1.3f);
    draw::text("AUDIO REACTIVE • LEVEL " + level, 60, 630, 18, 0xe9e5dc);
    draw::text("BASS " + bass + "   PRESENCE " + presence + "   PEAK BAND " + peakBand + "   LOCAL WAVE " + wavePeak, 60, 665, 16, 0xb8c6cc);
    draw::text("BEATS " + analysis.beatCount(frame) + " / " + analysis.beatTotal() + "   FIRST BEAT " + (analysis.beatTotal() > 0 ? analysis.beatFrame(0) : 0) + "   BPM " + analysis.bpm(), 60, 690, 14, 0x8395a8);
    draw::circle(1080, 620, 8 + pulse * 22, 0xffb454, 0.8f);
}
