// subtitles.cpp -- SRT/WebVTT parsing, phrase timing, burned-in captions, and a shell-
// command text-to-speech hook (see rmtn/subtitles.hpp).
#include "rmtn/subtitles.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/fonts.hpp"
#include "rmtn/media.hpp"
#include "rmtn/render.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace rmtn {
namespace sub {
namespace {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

// "01:02:03,456", "01:02:03.456", "02:03.456" (VTT) -> seconds.
double parseTime(const std::string& raw, int block) {
    const std::string s = trim(raw);
    int parts[3] = { 0, 0, 0 }, n = 0;
    size_t i = 0;
    while (i < s.size() && n < 3) {
        if (!std::isdigit((unsigned char)s[i])) break;
        int v = 0;
        while (i < s.size() && std::isdigit((unsigned char)s[i])) v = v * 10 + (s[i++] - '0');
        parts[n++] = v;
        if (i < s.size() && s[i] == ':') i++;
        else break;
    }
    double ms = 0;
    if (i < s.size() && (s[i] == ',' || s[i] == '.')) {
        i++;
        double scale = 0.1;
        while (i < s.size() && std::isdigit((unsigned char)s[i])) { ms += (s[i++] - '0') * scale; scale /= 10; }
    }
    if (n < 2 || i != s.size()) throw std::runtime_error("subtitles: bad time '" + s + "' in cue " + std::to_string(block));
    const double h = n == 3 ? parts[0] : 0, m = n == 3 ? parts[1] : parts[0], sec = n == 3 ? parts[2] : parts[1];
    return h * 3600 + m * 60 + sec + ms;
}

// Removes <tags> and {\override} blocks, decodes the common entities.
std::string stripMarkup(const std::string& s, bool& italic) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '<') {
            const size_t close = s.find('>', i);
            if (close != std::string::npos) {
                std::string tag = s.substr(i + 1, close - i - 1);
                std::transform(tag.begin(), tag.end(), tag.begin(), [](char ch) { return (char)std::tolower((unsigned char)ch); });
                if (tag == "i") italic = true;
                i = close;
                continue;
            }
        }
        if (s[i] == '{' && i + 1 < s.size() && s[i + 1] == '\\') {
            const size_t close = s.find('}', i);
            if (close != std::string::npos) { i = close; continue; }
        }
        if (s[i] == '&') {
            static const std::pair<const char*, const char*> ents[] = { { "&amp;", "&" }, { "&lt;", "<" }, { "&gt;", ">" }, { "&nbsp;", " " }, { "&quot;", "\"" } };
            bool hit = false;
            for (const auto& [e, r] : ents)
                if (s.compare(i, strlen(e), e) == 0) { out += r; i += strlen(e) - 1; hit = true; break; }
            if (hit) continue;
        }
        out += s[i];
    }
    return out;
}

// Lowercase letters and digits with single spaces (UTF-8 bytes kept as they are).
std::string normal(const std::string& s) {
    std::string out;
    bool space = false;
    for (unsigned char ch : s) {
        if (ch >= 0x80 || std::isalnum(ch)) {
            if (space && !out.empty()) out += ' ';
            space = false;
            out += (char)(ch < 0x80 ? std::tolower(ch) : ch);
        } else if (ch != '\'') space = true;           // "don't" stays one word
    }
    return out;
}

} // namespace

double Track::seconds() const {
    double end = 0;
    for (const Cue& c : cues) end = std::max(end, c.end);
    return end;
}
int Track::frames(int fps) const { return (int)std::ceil(seconds() * fps - 1e-9); }

const Cue* Track::at(double t) const {
    for (const Cue& c : cues)
        if (t >= c.start && t < c.end) return &c;
    return nullptr;
}

double Track::when(const std::string& phrase, double after) const {
    const std::string want = normal(phrase);
    if (want.empty()) throw std::runtime_error("subtitles: empty phrase");
    for (const Cue& c : cues) {
        if (c.end <= after) continue;
        const std::string text = normal(c.text);
        for (size_t pos = text.find(want); pos != std::string::npos; pos = text.find(want, pos + 1)) {
            if (pos > 0 && text[pos - 1] != ' ') continue;   // whole words only at the start
            const double t = c.start + (c.end - c.start) * (double)pos / std::max<size_t>(1, text.size());
            if (t >= after - 1e-9) return t;
        }
    }
    throw std::runtime_error("subtitles: phrase not found after " + std::to_string(after) + "s: \"" + phrase + "\"");
}

int Track::frameOf(const std::string& phrase, int fps, double after) const {
    return (int)std::lround(when(phrase, after) * fps);
}

std::vector<Track::Span> Track::spans(const std::vector<std::string>& phrases, int fps, int tail) const {
    std::vector<int> at;
    double after = 0;
    for (size_t i = 0; i < phrases.size(); i++) {
        const double t = when(phrases[i], after);
        at.push_back(i == 0 ? 0 : (int)std::lround(t * fps));
        after = t;
    }
    at.push_back(std::max(at.empty() ? 0 : at.back() + 1, frames(fps) + tail));
    std::vector<Span> out;
    for (size_t i = 0; i + 1 < at.size(); i++) out.push_back({ at[i], std::max(1, at[i + 1] - at[i]) });
    return out;
}

Track Track::shifted(double s) const {
    Track t = *this;
    for (Cue& c : t.cues) { c.start += s; c.end += s; }
    return t;
}

Track parse(const std::string& raw) {
    std::string text = raw;
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);
    std::string s;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\r') { s += '\n'; if (i + 1 < text.size() && text[i + 1] == '\n') i++; }
        else s += text[i];
    }
    Track track;
    std::istringstream in(s);
    std::string line;
    std::vector<std::string> block;
    int count = 0;
    auto flush = [&] {
        if (block.empty()) return;
        count++;
        size_t k = 0;
        while (k < block.size() && block[k].find("-->") == std::string::npos) k++;
        if (k == block.size()) { block.clear(); return; }      // WEBVTT header, NOTE, STYLE, stray numbers
        const std::string& timing = block[k];
        const size_t arrow = timing.find("-->");
        std::string right = trim(timing.substr(arrow + 3));
        right = right.substr(0, right.find_first_of(" \t"));      // VTT cue settings follow the time
        Cue c;
        c.start = parseTime(timing.substr(0, arrow), count);
        c.end = parseTime(right, count);
        std::string body;
        for (size_t j = k + 1; j < block.size(); j++) body += (body.empty() ? "" : "\n") + block[j];
        c.text = trim(stripMarkup(body, c.italic));
        track.cues.push_back(c);
        block.clear();
    };
    while (std::getline(in, line)) {
        if (trim(line).empty()) flush();
        else block.push_back(line);
    }
    flush();
    std::stable_sort(track.cues.begin(), track.cues.end(), [](const Cue& a, const Cue& b) { return a.start < b.start; });
    return track;
}

Track load(const std::string& file) {
    const std::string path = media::resolve(file);
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    if (!in) throw std::runtime_error("subtitles: cannot read " + path);
    std::stringstream text;
    text << in.rdbuf();
    return parse(text.str());
}

std::string toSrt(const Track& track) {
    auto stamp = [](double t) {
        long long ms = std::llround(std::max(0.0, t) * 1000);
        char b[32];
        snprintf(b, sizeof b, "%02lld:%02lld:%02lld,%03lld", ms / 3600000, ms / 60000 % 60, ms / 1000 % 60, ms % 1000);
        return std::string(b);
    };
    std::string out;
    int n = 1;
    for (const Cue& c : track.cues) {
        out += std::to_string(n++) + "\n" + stamp(c.start) + " --> " + stamp(c.end) + "\n";
        out += (c.italic ? "<i>" + c.text + "</i>" : c.text) + "\n\n";
    }
    return out;
}

bool writeSrt(const std::string& path, const Track& track) {
    std::error_code ec;
    const auto parent = std::filesystem::u8path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    std::ofstream out(std::filesystem::u8path(path), std::ios::binary);
    out << toSrt(track);
    return (bool)out;
}

Style defaultStyle() {
    Style s;
    s.text.size = 52;
    s.text.lineHeight = 1.2f;
    s.text.color = WHITE;
    return s;
}

void draw(const Ctx& c0, const Cue& cue, double seconds, const Style& style) {
    if (seconds < cue.start || seconds >= cue.end || cue.text.empty()) return;
    const double fps = frameRate();
    const float fade = style.fade > 0 ? (float)std::clamp(std::min(seconds - cue.start, cue.end - seconds) * fps / style.fade, 0.0, 1.0) : 1.f;
    if (fade <= 0) return;
    Ctx c = c0;
    c.a *= fade;
    TextStyle ts = style.text;
    if (!ts.font.texture.id) ts.font = fonts::ui(true);
    Vector2 frame = canvasSize();
    if (frame.x <= 0) frame = { 1920, 1080 };
    const std::vector<std::string> lines = wrapText(cue.text, ts, style.maxWidth);
    const float lineH = ts.size * ts.lineHeight;
    std::vector<float> widths;
    float widest = 0;
    for (const std::string& l : lines) {
        widths.push_back(dr::measure(ts.font, l.c_str(), ts.size, ts.spacing).x);
        widest = std::max(widest, widths.back());
    }
    const float top = frame.y - style.bottom - lineH * lines.size();
    auto left = [&](float w) {
        return style.align == Align::Start ? style.margin : style.align == Align::End ? frame.x - style.margin - w : (frame.x - w) / 2;
    };
    if (style.box.a > 0) {
        const float p = style.boxPadding;
        dr::roundRect(c, left(widest) - p, top - p * 0.6f, widest + 2 * p, lineH * lines.size() + p * 1.2f, style.boxRadius, style.box);
    }
    // Karaoke: the share of the cue's characters spoken by now.
    const bool karaoke = style.upcoming.a > 0;
    size_t total = 0;
    for (const std::string& l : lines) total += l.size() + 1;
    const double spoken = karaoke ? (seconds - cue.start) / (cue.end - cue.start) * total : 1e18;
    size_t before = 0;
    auto glyphs = [&](const std::string& str, Vector2 at, Color col) {
        if (style.outline.a > 0 && style.outlineWidth > 0) {
            const Color o = withAlpha(style.outline, col.a / 255.f);
            for (int k = 0; k < 12; k++) {
                const float a = k * (float)PI / 6;
                dr::text(ts.font, c, str.c_str(), { at.x + std::cos(a) * style.outlineWidth, at.y + std::sin(a) * style.outlineWidth },
                         ts.size, ts.spacing, o);
            }
        }
        dr::text(ts.font, c, str.c_str(), at, ts.size, ts.spacing, col);
    };
    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& l = lines[i];
        const float x = left(widths[i]), y = top + lineH * i + (lineH - ts.size) / 2;
        if (!karaoke) { glyphs(l, { x, y }, ts.color); before += l.size() + 1; continue; }
        size_t w = 0;
        while (w < l.size()) {
            size_t e = l.find(' ', w);
            if (e == std::string::npos) e = l.size();
            const std::string word = l.substr(w, e - w);
            const float dx = dr::measure(ts.font, l.substr(0, w).c_str(), ts.size, ts.spacing).x;
            const bool said = before + w + word.size() / 2.0 <= spoken;
            glyphs(word, { x + dx, y }, said ? ts.color : style.upcoming);
            w = e + 1;
        }
        before += l.size() + 1;
    }
}

std::function<void(const Ctx&, int)> captions(Track track, Style style) {
    return [track = std::move(track), style](const Ctx& c, int t) {
        const double s = frameTime(t) / frameRate();
        if (const Cue* cue = track.at(s)) draw(c, *cue, s, style);
    };
}

} // namespace sub

// ---------------------------------------------------------------- text to speech
namespace tts {
namespace {

std::string hashHex(const std::string& s) {
    unsigned long long h = 1469598103934665603ULL;
    for (unsigned char ch : s) { h ^= ch; h *= 1099511628211ULL; }
    char b[20];
    snprintf(b, sizeof b, "%016llx", h);
    return b;
}

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    for (size_t p = s.find(from); p != std::string::npos; p = s.find(from, p + to.size())) s.replace(p, from.size(), to);
    return s;
}

double estimate(const std::string& text, double wps) {
    int words = 0;
    bool in = false;
    for (unsigned char ch : text) {
        const bool w = !std::isspace(ch);
        words += w && !in;
        in = w;
    }
    return std::max(0.6, words / std::max(0.5, wps));
}

} // namespace

Voice Voice::system() {
    Voice v;
#if defined(_WIN32)
    v.command = "powershell -NoProfile -NonInteractive -Command \"Add-Type -AssemblyName System.Speech; "
                "$s = New-Object System.Speech.Synthesis.SpeechSynthesizer; $s.Rate = 0; "
                "$s.SetOutputToWaveFile('{out}'); $s.Speak([IO.File]::ReadAllText('{textfile}')); $s.Dispose()\"";
#elif defined(__APPLE__)
    v.command = "say --file-format=WAVE --data-format=LEI16@22050 -o '{out}' -f '{textfile}'";
#else
    v.command = "espeak-ng -w '{out}' -f '{textfile}'";
#endif
    return v;
}

Script::Script(Voice voice, std::vector<std::string> lines, double gap, double lead)
    : voice_(std::move(voice)), gap_(gap), lead_(lead) {
    for (std::string& text : lines) {
        Line l;
        l.text = std::move(text);
        l.file = voice_.cacheDir + "/" + hashHex(voice_.command + "\n" + l.text) + ".wav";
        l.seconds = estimate(l.text, voice_.wordsPerSecond);
        lines_.push_back(std::move(l));
    }
    layout();
}

void Script::layout() {
    double t = lead_;
    for (Line& l : lines_) {
        l.start = t;
        t += l.seconds + gap_;
    }
}

void Script::measure() {
    for (Line& l : lines_) {
        std::error_code ec;
        l.spoken = std::filesystem::is_regular_file(std::filesystem::u8path(l.file), ec);
        if (!l.spoken) continue;
        const std::vector<float>& pcm = media::decodeAudio(l.file, 48000);
        l.seconds = pcm.size() / 2.0 / 48000;
    }
    layout();
}

void Script::generate() {
    namespace fs = std::filesystem;
    fs::create_directories(fs::u8path(voice_.cacheDir));
    for (const Line& l : lines_) {
        std::error_code ec;
        if (fs::is_regular_file(fs::u8path(l.file), ec)) continue;
        if (voice_.command.empty()) throw std::runtime_error("tts: no voice command");
        const fs::path wav = fs::absolute(fs::u8path(l.file));
        const fs::path txt = fs::path(wav).replace_extension(".txt"), part = fs::path(wav).replace_extension(".part.wav");
        {
            std::ofstream out(txt, std::ios::binary);
            out << l.text;
        }
        fs::remove(part, ec);
        std::string cmd = replaceAll(replaceAll(voice_.command, "{textfile}", txt.u8string()), "{out}", part.u8string());
#ifdef _WIN32
        cmd = "\"" + cmd + "\"";            // cmd.exe drops the outer quotes of a command with several
#endif
        const int rc = std::system(cmd.c_str());
        const bool ok = rc == 0 && fs::is_regular_file(part, ec) && fs::file_size(part, ec) > 44;
        fs::remove(txt, ec);
        if (!ok) {
            fs::remove(part, ec);
            throw std::runtime_error("tts: the voice command failed (" + std::to_string(rc) + ") on \"" + l.text + "\"");
        }
        fs::rename(part, wav, ec);
        if (ec) throw std::runtime_error("tts: cannot store " + wav.u8string());
    }
    measure();
}

double Script::seconds() const {
    return lines_.empty() ? lead_ : lines_.back().start + lines_.back().seconds;
}
int Script::frames(int fps) const { return (int)std::ceil(seconds() * fps); }

sub::Track Script::track() const {
    sub::Track t;
    for (const Line& l : lines_) t.cues.push_back({ l.start, l.start + l.seconds, l.text, false });
    return t;
}

void Script::attach(TNode* node, int fps, float volume) const {
    for (const Line& l : lines_) {
        if (!l.spoken) continue;
        media::AudioOptions o;
        o.volume = volume;
        media::audio(node->add((int)std::lround(l.start * fps), (int)std::ceil(l.seconds * fps) + 1), l.file, o);
    }
}

} // namespace tts
} // namespace rmtn
